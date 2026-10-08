// The debug server (switch/native/source/debug_server.cpp) on the development machine, for testing the protocol
// and scripts/switch/switchwaker_debug.py without a console:
//   c++ -std=c++20 -O1 -pthread -Iswitch/native/source scripts/switch/debug_server_host_test.cpp \
//       switch/native/source/debug_server.cpp -o build/debug_server_host_test
//   build/debug_server_host_test <port> <root folder>
// A line is logged every 100 ms ("[test] tick n"), the injected controller state is logged when it changes, and
// "shot" sends a copy of <root>/shot.png when there is one; "quit" ends the program.
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "debug_server.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <port> <root folder>\n", argv[0]);
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    static std::string root = argv[2];
    static bool quit = false;
    debugsrv::Config c;
    c.port = atoi(argv[1]);
    c.root = root;
    c.log = [](const char* line) { fprintf(stderr, "%s\n", line); };
    debugsrv::add_command("info", "info", [](const debugsrv::Args&) { return debugsrv::ok("host test\n"); });
    debugsrv::add_command("shot", "shot", [](const debugsrv::Args&) {
        const std::string copy = root + "/shot_sent.png";
        FILE* in = fopen((root + "/shot.png").c_str(), "rb");
        if (!in) return debugsrv::err("no shot.png in the root folder");
        FILE* out = fopen(copy.c_str(), "wb");
        char b[4096];
        size_t n;
        while ((n = fread(b, 1, sizeof b, in))) fwrite(b, 1, n, out);
        fclose(in);
        fclose(out);
        debugsrv::Reply r;
        r.file = copy;
        r.removeFile = true;
        return r;
    });
    debugsrv::add_command("quit", "quit", [](const debugsrv::Args&) {
        debugsrv::Reply r = debugsrv::ok("quitting");
        r.after = [] { quit = true; };
        return r;
    });
    if (!debugsrv::start(c)) return 1;
    fprintf(stderr, "listening on port %d\n", c.port);
    debugsrv::Injected last;
    for (int tick = 0; !quit; tick++) {
        if (tick % 25 == 0) {
            char line[64];
            int n = snprintf(line, sizeof line, "[test] tick %d\n", tick / 25);
            debugsrv::log_tap(line, size_t(n));
        }
        const debugsrv::Injected now = debugsrv::injected();
        if (now.buttons != last.buttons || now.stick[0] != last.stick[0] || now.x[0] != last.x[0] ||
            now.y[0] != last.y[0] || now.stick[1] != last.stick[1]) {
            char line[160];
            int n = snprintf(line, sizeof line, "[test] input buttons 0x%llx left %d %.2f %.2f right %d %.2f %.2f\n",
                             (unsigned long long)now.buttons, now.stick[0], now.x[0], now.y[0], now.stick[1], now.x[1],
                             now.y[1]);
            debugsrv::log_tap(line, size_t(n));
            fputs(line, stderr);
            last = now;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }
    debugsrv::stop();
    return 0;
}
