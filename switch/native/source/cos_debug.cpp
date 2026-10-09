// SwitchWaker's debug server commands (debug_server.h; docs/DEBUG_SERVER.md; client
// scripts/switch/switchwaker_debug.py). COS_DEBUG_SERVER (the options menu's Depuración > "Servidor de
// depuración": 1, port 6543; a port number in user/settings.ini) starts it right after the settings file is
// read, so a change applies at the next start; it is off by default, and without it the log text kept since
// start is let go.
//
//   info               build, frame, stage, memory, applet type, address
//   warps, warp ...    the options menu's travel list (pc_menu.cpp), applied by the game thread once a file is played
//   shot [game]        PNG of the next frame (pc_shot.cpp): with the FPS panel and the menu, or the game alone
//   reload             restart: the forwarder loads the NRO again (cos_switch_restart)
//   quit               end the process (cos_switch_exit)
//   crash              a data abort on the server's connection thread: tests the crash report and the
//                      end of the process with the game and its audio running (cos_switch.cpp's reaper)
// Paths of get / put / ls are relative to /switch/switchwaker (the NRO; native/ holds user/settings.ini and logs/).
#include <arpa/inet.h>
#include <switch.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "cos_switch_internal.h"
#include "debug_server.h"

#ifndef COS_SWITCH_VERSION_STR
#define COS_SWITCH_VERSION_STR "dev"
#endif

extern "C" int pc_debug_warp_list(char* out, size_t size);                                         // pc_menu.cpp
extern "C" int pc_debug_warp(int index, const char* stage, int room, int point, char* done, size_t size);
extern "C" int pc_debug_status(char* out, size_t size);
extern "C" unsigned int pc_debug_shot_request(const char* path, int overlay);                     // pc_shot.cpp
extern "C" unsigned int pc_debug_shot_done(int* ok);

namespace {

int gPort = 0;

std::string ipText() {
    in_addr a;
    a.s_addr = (uint32_t)gethostid(); // libnx: the console's address on the network (nifm), network order
    return inet_ntoa(a);
}

debugsrv::Reply info(const debugsrv::Args&) {
    char status[128];
    pc_debug_status(status, sizeof(status));
    u64 total = 0, used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    const AppletType type = appletGetAppletType();
    char b[640];
    snprintf(b, sizeof(b), "version %s\n%smemory used %llu MiB of %llu MiB\nrunning as %s\naddress %s:%d\n",
             COS_SWITCH_VERSION_STR, status, (unsigned long long)(used >> 20), (unsigned long long)(total >> 20),
             type == AppletType_Application ? "application (the HOME menu forwarder: reload works)"
                                            : "applet or hbmenu (reload does not work: quit, then start it again)",
             ipText().c_str(), gPort);
    return debugsrv::ok(b);
}

debugsrv::Reply warps(const debugsrv::Args&) {
    static char list[32 << 10];
    const int n = pc_debug_warp_list(list, sizeof(list));
    return debugsrv::ok(std::string(list, (size_t)n));
}

debugsrv::Reply warp(const debugsrv::Args& a) {
    if (a.empty()) {
        return debugsrv::err("usage: warp <number from warps> | warp <stage> [room] [point]");
    }
    char* end;
    const long index = strtol(a[0].c_str(), &end, 10);
    const bool byIndex = *end == 0 && a.size() == 1;
    if (!byIndex && a[0].size() > 7) {
        return debugsrv::err("a stage name has 7 characters at most");
    }
    char done[64];
    if (!pc_debug_warp(byIndex ? (int)index : -1, a[0].c_str(), a.size() > 1 ? atoi(a[1].c_str()) : 0,
                       a.size() > 2 ? atoi(a[2].c_str()) : 0, done, sizeof(done))) {
        return debugsrv::err("no warp number " + a[0] + " (warps lists them)");
    }
    fprintf(stderr, "[cos] debug server: warp to %s requested\n", done);
    return debugsrv::ok(std::string("warp to ") + done +
                        " requested (it happens once a file is being played and no scene change is in progress)");
}

// "shot [game]": the next frame end writes the PNG (pc_shot.cpp); sent, then deleted
debugsrv::Reply shot(const debugsrv::Args& a) {
    const bool game = !a.empty() && a[0] == "game";
    const char* path = COS_SWITCH_ROOT "/debug_shot.png";
    const unsigned int ticket = pc_debug_shot_request(path, game ? 0 : 1);
    for (int i = 0; i < 400; i++) { // 20 s
        int ok = 0;
        if (pc_debug_shot_done(&ok) >= ticket) {
            if (!ok) {
                return debugsrv::err("the frame could not be read back or written (the log says why)");
            }
            debugsrv::Reply r;
            r.file = path;
            r.removeFile = true;
            return r;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return debugsrv::err("no frame within 20 s (is the game presenting frames?)");
}

debugsrv::Reply reload(const debugsrv::Args&) {
    if (appletGetAppletType() != AppletType_Application) {
        return debugsrv::err("reload needs the HOME menu forwarder (from hbmenu: quit, then start it again)");
    }
    debugsrv::Reply r = debugsrv::ok("restarting: the forwarder loads the NRO from the SD card again");
    r.after = [] {
        debugsrv::stop();
        cos_switch_restart();
    };
    return r;
}

debugsrv::Reply quit(const debugsrv::Args&) {
    debugsrv::Reply r = debugsrv::ok("quitting");
    r.after = [] {
        fprintf(stderr, "[cos] debug server: quit\n");
        debugsrv::stop();
        cos_switch_exit(0);
    };
    return r;
}

debugsrv::Reply crash(const debugsrv::Args&) {
    debugsrv::Reply r = debugsrv::ok("crashing: the log has the report; the process ends within seconds");
    r.after = [] {
        fprintf(stderr, "[cos] debug server: crash requested, writing through a null pointer\n");
        *(volatile u32*)(uintptr_t)0x10 = 0x7777;
    };
    return r;
}

} // namespace

extern "C" void cos_switch_debug_start(void) {
    const char* e = getenv("COS_DEBUG_SERVER");
    if (e == nullptr || *e == '\0' || strcmp(e, "0") == 0) {
        debugsrv::drop_log();
        return;
    }
    gPort = atoi(e) > 1 ? atoi(e) : 6543;
    debugsrv::add_command("info", "info                   build, frame, stage, memory, address", info);
    debugsrv::add_command("warps", "warps                  the travel destinations, numbered", warps);
    debugsrv::add_command("warp", "warp <n> | warp <stage> [room] [point]", warp);
    debugsrv::add_command("shot", "shot [game]            PNG of the next frame (game: without the panel and menu)", shot);
    debugsrv::add_command("reload", "reload                 restart: the forwarder loads the NRO again (put it first)", reload);
    debugsrv::add_command("quit", "quit                   end the program", quit);
    debugsrv::add_command("crash", "crash                  a deliberate crash (tests the crash report)", crash);
    debugsrv::Config c;
    c.port = gPort;
    c.root = "/switch/switchwaker";
    c.log = [](const char* line) { fprintf(stderr, "%s\n", line); };
    // above the game's threads, off its core (core 0): the server answers while the game is busy
    c.threadStart = [] {
        svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2C);
        svcSetThreadCoreMask(CUR_THREAD_HANDLE, -1, 0x6);
    };
    if (debugsrv::start(c)) {
        fprintf(stderr,
                "[switch] debug server listening on %s:%d (COS_DEBUG_SERVER; client scripts/switch/switchwaker_debug.py; "
                "local network only, no password)\n",
                ipText().c_str(), gPort);
    } else {
        debugsrv::drop_log();
    }
}

extern "C" const char* cos_switch_version(void) { return COS_SWITCH_VERSION_STR; }

extern "C" int cos_switch_debug_input(uint64_t* buttons, int32_t sticks[4]) {
    if (!debugsrv::running()) {
        return 0;
    }
    const debugsrv::Injected in = debugsrv::injected();
    *buttons |= in.buttons;
    for (int s = 0; s < 2; s++) {
        if (in.stick[s]) {
            sticks[2 * s] = (int32_t)(in.x[s] * JOYSTICK_MAX);
            sticks[2 * s + 1] = (int32_t)(in.y[s] * JOYSTICK_MAX);
        }
    }
    return 1;
}
