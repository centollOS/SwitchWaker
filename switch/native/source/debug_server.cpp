// Debug server (debug_server.h): one listening thread, one thread per connection (at most kMaxConns), the
// log's recent text in memory for "log" streams, the injected controller state for the input poll.
#include "debug_server.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

#ifdef __SWITCH__
#include <pthread.h>
#include <switch.h>
#endif

namespace debugsrv {
namespace {

Config g_cfg;
std::atomic<bool> g_running{false};
std::atomic<bool> g_keep{false};  // keep_log: the log's text is kept before start
int g_listen = -1;

struct Command {
    std::string usage;
    Handler handler;
};
std::mutex g_cmdMu;
std::map<std::string, Command> g_cmds;

// the log's most recent text: g_logBuf holds bytes [g_logBase, g_logBase + size) of everything logged since start
std::mutex g_logMu;
std::condition_variable g_logCv;
std::string g_logBuf;
uint64_t g_logBase = 0;
constexpr size_t kLogKeep = 2u << 20;

// open connection sockets (stop closes them)
std::mutex g_sockMu;
std::vector<int> g_sockets;
std::atomic<int> g_conns{0};
constexpr int kMaxConns = 4;

void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char* fmt, ...) {
    if (!g_cfg.log) return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    g_cfg.log(line);
}

uint64_t now_ns() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
}

void spawn(void (*fn)(void*), void* arg) {
#ifdef __SWITCH__
    // Horizon commits a small default stack for std::thread
    struct Start {
        void (*fn)(void*);
        void* arg;
    };
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128 << 10);
    pthread_t t;
    auto* s = new Start{fn, arg};
    if (pthread_create(&t, &attr, [](void* p) -> void* {
            Start s = *static_cast<Start*>(p);
            delete static_cast<Start*>(p);
            s.fn(s.arg);
            return nullptr;
        }, s) == 0)
        pthread_detach(t);
    else
        delete s;
    pthread_attr_destroy(&attr);
#else
    std::thread(fn, arg).detach();
#endif
}

// ---------------------------------------------------------------- injected controller state
std::mutex g_inMu;
uint64_t g_until[16] = {};  // per button: 0 up, else held until this time (UINT64_MAX: until released)
struct Stick {
    float x = 0, y = 0;
    uint64_t until = 0;
};
Stick g_sticks[2];

const struct {
    const char* name;
    int bit;
} kButtons[] = {
    {"A", 0},      {"B", 1},     {"X", 2},      {"Y", 3},     {"LS", 4},     {"RS", 5},     {"L", 6},     {"R", 7},
    {"ZL", 8},     {"ZR", 9},    {"PLUS", 10},  {"+", 10},    {"MINUS", 11}, {"-", 11},     {"LEFT", 12}, {"UP", 13},
    {"RIGHT", 14}, {"DOWN", 15}, {"LSTICK", 4}, {"RSTICK", 5},
};
#ifdef __SWITCH__
static_assert(HidNpadButton_A == 1 << 0 && HidNpadButton_StickL == 1 << 4 && HidNpadButton_ZL == 1 << 8 &&
              HidNpadButton_Minus == 1 << 11 && HidNpadButton_Left == 1 << 12 && HidNpadButton_Down == 1 << 15);
#endif

// "A", "A+B", "zl", "+" -> bits; false on an unknown name
bool parse_buttons(std::string s, uint32_t& bits) {
    for (auto& ch : s) ch = char(toupper((unsigned char)ch));
    auto one = [&](const std::string& name) {
        for (auto& b : kButtons)
            if (name == b.name) return bits |= 1u << b.bit, true;
        return false;
    };
    if (s == "+" || s == "-") return one(s);
    for (size_t start = 0;;) {
        const size_t end = s.find('+', start);
        if (!one(s.substr(start, end == std::string::npos ? std::string::npos : end - start))) return false;
        if (end == std::string::npos) return true;
        start = end + 1;
    }
}

// the buttons of args (the last one may be a duration in ms)
bool parse_press(const Args& a, uint32_t& bits, int& ms) {
    bits = 0;
    for (size_t i = 0; i < a.size(); i++) {
        char* end;
        long v = strtol(a[i].c_str(), &end, 10);
        if (i + 1 == a.size() && i > 0 && *end == 0 && !a[i].empty()) {
            ms = int(std::clamp(v, 1l, 60000l));
            continue;
        }
        if (!parse_buttons(a[i], bits)) return false;
    }
    return bits != 0;
}

void wait_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// ---------------------------------------------------------------- connections
struct Conn {
    int fd = -1;
    std::string in;  // bytes received and not consumed yet
};

bool send_all(int fd, const void* data, size_t n) {
    const char* p = static_cast<const char*>(data);
    while (n) {
#ifdef MSG_NOSIGNAL
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
#else
        ssize_t w = send(fd, p, n, 0);
#endif
        if (w <= 0) return false;
        p += w;
        n -= size_t(w);
    }
    return true;
}
bool send_header(int fd, bool ok, uint64_t n) {
    char h[48];
    int k = snprintf(h, sizeof h, "%s %llu\n", ok ? "ok" : "err", (unsigned long long)n);
    return send_all(fd, h, size_t(k));
}

bool fill(Conn& c) {
    char buf[16384];
    ssize_t r = recv(c.fd, buf, sizeof buf, 0);
    if (r <= 0) return false;
    c.in.append(buf, size_t(r));
    return true;
}
bool read_line(Conn& c, std::string& line) {
    for (;;) {
        size_t nl = c.in.find('\n');
        if (nl != std::string::npos) {
            line = c.in.substr(0, nl);
            c.in.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }
        if (c.in.size() > 4096 || !fill(c)) return false;
    }
}

Args split(const std::string& line) {
    Args out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && line[i] == ' ') i++;
        if (i >= line.size()) break;
        std::string a;
        if (line[i] == '"') {
            size_t e = line.find('"', i + 1);
            if (e == std::string::npos) e = line.size();
            a = line.substr(i + 1, e - i - 1);
            i = e + 1;
        } else {
            size_t e = line.find(' ', i);
            if (e == std::string::npos) e = line.size();
            a = line.substr(i, e - i);
            i = e;
        }
        out.push_back(std::move(a));
    }
    return out;
}

std::string resolve(const std::string& p) {
    if (p.empty()) return g_cfg.root.empty() ? "." : g_cfg.root;
    if (p[0] == '/' || p.find(':') != std::string::npos) return p;
    return g_cfg.root.empty() ? p : g_cfg.root + "/" + p;
}

uint32_t crc32_update(uint32_t crc, const uint8_t* p, size_t n) {
    static uint32_t table[256];
    static const bool init = [] {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        return true;
    }();
    (void)init;
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

bool send_reply(Conn& c, const Reply& r) {
    if (r.file.empty()) return send_header(c.fd, r.ok, r.text.size()) && send_all(c.fd, r.text.data(), r.text.size());
    FILE* f = fopen(r.file.c_str(), "rb");
    if (!f) {
        const std::string e = "cannot open " + r.file;
        return send_header(c.fd, false, e.size()) && send_all(c.fd, e.data(), e.size());
    }
    struct stat st;
    const uint64_t size = fstat(fileno(f), &st) == 0 ? uint64_t(st.st_size) : 0;
    bool good = send_header(c.fd, true, size);
    static thread_local std::vector<char> buf(256 << 10);
    uint64_t left = size;
    while (good && left) {
        size_t n = fread(buf.data(), 1, std::min<uint64_t>(left, buf.size()), f);
        if (!n) break;
        good = send_all(c.fd, buf.data(), n);
        left -= n;
    }
    fclose(f);
    if (r.removeFile) remove(r.file.c_str());
    return good && !left;
}

// "put <path> <size>": size bytes follow the line; written to <path>.part, then renamed over path
Reply do_put(Conn& c, const Args& a) {
    if (a.size() != 2) return err("usage: put <path> <size>");
    const uint64_t size = strtoull(a[1].c_str(), nullptr, 10);
    const std::string path = resolve(a[0]), part = path + ".part";
    FILE* f = fopen(part.c_str(), "wb");
    // the bytes are read either way, so the connection stays in step
    uint64_t left = size;
    uint32_t crc = 0;
    bool wrote = f != nullptr;
    if (f) setvbuf(f, nullptr, _IOFBF, 256 << 10);
    while (left) {
        if (c.in.empty() && !fill(c)) {
            if (f) fclose(f), remove(part.c_str());
            return err("connection closed during the upload");
        }
        const size_t n = size_t(std::min<uint64_t>(left, c.in.size()));
        crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(c.in.data()), n);
        if (f && fwrite(c.in.data(), 1, n, f) != n) wrote = false;
        c.in.erase(0, n);
        left -= n;
    }
    if (!f) return err("cannot write " + part);
    if (fclose(f) != 0) wrote = false;
    if (!wrote) {
        remove(part.c_str());
        return err("write failed: " + part + " (SD card full?)");
    }
    remove(path.c_str());  // (FAT: rename does not replace)
    if (rename(part.c_str(), path.c_str()) != 0) return err("cannot rename " + part + " to " + path);
    char t[64];
    snprintf(t, sizeof t, "crc32 %08x size %llu", crc, (unsigned long long)size);
    say("[debug] put %s: %llu bytes", path.c_str(), (unsigned long long)size);
    return ok(t);
}

Reply do_ls(const Args& a) {
    const std::string path = resolve(a.empty() ? "" : a[0]);
    DIR* d = opendir(path.c_str());
    if (!d) return err("cannot open the folder " + path);
    std::vector<std::string> lines;
    while (dirent* e = readdir(d)) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        struct stat st;
        const std::string full = path + "/" + e->d_name;
        char l[400];
        if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) snprintf(l, sizeof l, "d %12s %s", "-", e->d_name);
        else
            snprintf(l, sizeof l, "f %12llu %s", stat(full.c_str(), &st) == 0 ? (unsigned long long)st.st_size : 0ull,
                     e->d_name);
        lines.push_back(l);
    }
    closedir(d);
    std::sort(lines.begin(), lines.end(), [](const std::string& x, const std::string& y) {
        return x.compare(15, std::string::npos, y, 15, std::string::npos) < 0;
    });
    std::string out;
    for (auto& l : lines) out += l + "\n";
    return ok(out);
}

// "log [all]": the connection becomes a stream of the log's text (all: from the oldest text kept)
void do_log(Conn& c, bool all) {
    uint64_t pos;
    {
        std::lock_guard<std::mutex> lk(g_logMu);
        pos = all ? g_logBase : g_logBase + g_logBuf.size();
    }
    if (!send_header(c.fd, true, 0)) return;
    std::string chunk;
    while (g_running.load()) {
        {
            std::unique_lock<std::mutex> lk(g_logMu);
            g_logCv.wait_for(lk, std::chrono::milliseconds(500),
                             [&] { return g_logBase + g_logBuf.size() > pos || !g_running.load(); });
            chunk.clear();
            if (pos < g_logBase) {
                char note[96];
                snprintf(note, sizeof note, "[debug] %llu bytes of log skipped (the stream fell behind)\n",
                         (unsigned long long)(g_logBase - pos));
                chunk = note;
                pos = g_logBase;
            }
            chunk.append(g_logBuf, size_t(pos - g_logBase), std::string::npos);
            pos = g_logBase + g_logBuf.size();
        }
        if (!chunk.empty() && !send_all(c.fd, chunk.data(), chunk.size())) return;
        // the client closed the stream?
        pollfd p{c.fd, POLLIN, 0};
        if (poll(&p, 1, 0) > 0) {
            char b[256];
            if (recv(c.fd, b, sizeof b, 0) <= 0) return;
        }
    }
}

Reply builtin(Conn& c, const std::string& name, const Args& a, bool& handled) {
    handled = true;
    if (name == "ping") return ok("pong");
    if (name == "help") {
        std::string out =
            "ping\nhelp\nlog [all]              the log as it is written (all: the last 2 MiB first)\n"
            "logtext                the log text kept (the last 2 MiB)\n"
            "get <path>\nput <path> <size>      then size bytes\nls [path]\nrm <path>\nmkdir <path>\n"
            "press <buttons> [ms]   A B X Y L R ZL ZR PLUS MINUS UP DOWN LEFT RIGHT LS RS, A+B together; default 120 ms\n"
            "hold <buttons>\nrelease [buttons]\nstick <L|R> <x> <y> [ms]   -1..1, y up; 0 0 lets go\n";
        std::lock_guard<std::mutex> lk(g_cmdMu);
        for (auto& [n, cmd] : g_cmds) out += cmd.usage + "\n";
        return ok(out);
    }
    if (name == "logtext") {  // the log text kept (the running session's file cannot be opened while it is written)
        std::lock_guard<std::mutex> lk(g_logMu);
        if (!g_logBase) return ok(g_logBuf);
        char note[96];
        snprintf(note, sizeof note, "[debug] (the first %llu bytes of the log are not kept)\n", (unsigned long long)g_logBase);
        return ok(note + g_logBuf);
    }
    if (name == "get") {
        if (a.size() != 1) return err("usage: get <path>");
        Reply r;
        r.file = resolve(a[0]);
        return r;
    }
    if (name == "put") return do_put(c, a);
    if (name == "ls") return do_ls(a);
    if (name == "rm") {
        if (a.size() != 1) return err("usage: rm <path>");
        const std::string p = resolve(a[0]);
        return remove(p.c_str()) == 0 || rmdir(p.c_str()) == 0 ? ok() : err("cannot remove " + p);
    }
    if (name == "mkdir") {
        if (a.size() != 1) return err("usage: mkdir <path>");
        const std::string p = resolve(a[0]);
        struct stat st;
        return mkdir(p.c_str(), 0777) == 0 || (stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? ok()
                                                                                                 : err("cannot create " + p);
    }
    if (name == "press" || name == "hold") {
        uint32_t bits;
        int ms = 120;
        if (!parse_press(a, bits, ms)) return err("unknown button (A B X Y L R ZL ZR PLUS MINUS UP DOWN LEFT RIGHT LS RS)");
        {
            const uint64_t until = name == "hold" ? UINT64_MAX : now_ns() + uint64_t(ms) * 1000000;
            std::lock_guard<std::mutex> lk(g_inMu);
            for (int i = 0; i < 16; i++)
                if (bits & (1u << i)) g_until[i] = until;
        }
        // a press replies once released and one game frame has seen it up, so presses in a row are separate
        if (name == "press") wait_ms(ms + 50);
        return ok();
    }
    if (name == "release") {
        uint32_t bits = 0;
        int unused = 0;
        if (!a.empty() && !parse_press(a, bits, unused)) return err("unknown button");
        std::lock_guard<std::mutex> lk(g_inMu);
        for (int i = 0; i < 16; i++)
            if (a.empty() || (bits & (1u << i))) g_until[i] = 0;
        if (a.empty()) g_sticks[0].until = g_sticks[1].until = 0;
        return ok();
    }
    if (name == "stick") {
        if (a.size() < 3) return err("usage: stick <L|R> <x> <y> [ms]");
        const int which = toupper((unsigned char)a[0][0]) == 'R' ? 1 : 0;
        const float x = std::clamp(strtof(a[1].c_str(), nullptr), -1.0f, 1.0f);
        const float y = std::clamp(strtof(a[2].c_str(), nullptr), -1.0f, 1.0f);
        const int ms = a.size() > 3 ? atoi(a[3].c_str()) : 0;
        {
            std::lock_guard<std::mutex> lk(g_inMu);
            Stick& s = g_sticks[which];
            s.x = x, s.y = y;
            s.until = x == 0 && y == 0 && !ms ? 0 : ms > 0 ? now_ns() + uint64_t(ms) * 1000000 : UINT64_MAX;
        }
        if (ms > 0) wait_ms(ms + 50);
        return ok();
    }
    handled = false;
    return {};
}

struct ConnStart {
    int fd;
};

void connection(void* arg) {
    Conn c;
    c.fd = static_cast<ConnStart*>(arg)->fd;
    delete static_cast<ConnStart*>(arg);
    if (g_cfg.threadStart) g_cfg.threadStart();
    std::string line;
    while (g_running.load() && read_line(c, line)) {
        Args a = split(line);
        if (a.empty()) continue;
        const std::string name = a[0];
        a.erase(a.begin());
        if (name == "log") {
            do_log(c, !a.empty() && a[0] == "all");
            break;
        }
        bool handled;
        Reply r = builtin(c, name, a, handled);
        if (!handled) {
            Handler h;
            {
                std::lock_guard<std::mutex> lk(g_cmdMu);
                auto it = g_cmds.find(name);
                if (it != g_cmds.end()) h = it->second.handler;
            }
            r = h ? h(a) : err("unknown command " + name + " (help lists them)");
        }
        const bool sent = send_reply(c, r);
        if (r.after) r.after();
        if (!sent) break;
    }
    {
        std::lock_guard<std::mutex> lk(g_sockMu);
        auto it = std::find(g_sockets.begin(), g_sockets.end(), c.fd);
        if (it != g_sockets.end()) {
            g_sockets.erase(it);
            close(c.fd);
        }
    }
    g_conns--;
}

void listener(void*) {
    if (g_cfg.threadStart) g_cfg.threadStart();
    while (g_running.load()) {
        sockaddr_in from{};
        socklen_t len = sizeof from;
        const int fd = accept(g_listen, reinterpret_cast<sockaddr*>(&from), &len);
        if (fd < 0) {
            if (!g_running.load()) break;
            wait_ms(100);
            continue;
        }
        if (g_conns.load() >= kMaxConns) {
            const char busy[] = "err 39\ntoo many connections (4 at most) open\n";
            send_all(fd, busy, sizeof busy - 1);
            close(fd);
            continue;
        }
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        {
            std::lock_guard<std::mutex> lk(g_sockMu);
            g_sockets.push_back(fd);
        }
        g_conns++;
        say("[debug] connection from %s", inet_ntoa(from.sin_addr));
        spawn(connection, new ConnStart{fd});
    }
}

}  // namespace

bool start(const Config& config) {
    if (g_running.load()) return true;
    g_cfg = config;
#ifdef __SWITCH__
    const Result rc = socketInitializeDefault();
    if (R_FAILED(rc)) {
        say("[debug] server off: socket service unavailable (rc 0x%x)", rc);
        return false;
    }
#endif
    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(uint16_t(config.port));
    if (g_listen < 0 || bind(g_listen, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(g_listen, 4) != 0) {
        say("[debug] server off: cannot listen on port %d", config.port);
        if (g_listen >= 0) close(g_listen);
        g_listen = -1;
#ifdef __SWITCH__
        socketExit();
#endif
        return false;
    }
    g_running = true;
    spawn(listener, nullptr);
    return true;
}

bool running() { return g_running.load(std::memory_order_relaxed); }

void add_command(const char* name, const char* usage, Handler handler) {
    std::lock_guard<std::mutex> lk(g_cmdMu);
    g_cmds[name] = {usage, std::move(handler)};
}

void log_tap(const char* data, size_t size) {
    if ((!g_running.load(std::memory_order_relaxed) && !g_keep.load(std::memory_order_relaxed)) || !size) return;
    {
        std::lock_guard<std::mutex> lk(g_logMu);
        g_logBuf.append(data, size);
        if (g_logBuf.size() > kLogKeep) {
            // keep the newest half, from a line start
            size_t cut = g_logBuf.size() - kLogKeep / 2;
            const size_t nl = g_logBuf.find('\n', cut);
            if (nl != std::string::npos) cut = nl + 1;
            g_logBuf.erase(0, cut);
            g_logBase += cut;
        }
    }
    g_logCv.notify_all();
}

void keep_log() { g_keep = true; }

void drop_log() {
    g_keep = false;
    if (g_running.load()) return;
    std::lock_guard<std::mutex> lk(g_logMu);
    std::string().swap(g_logBuf);
    g_logBase = 0;
}

void stop() {
    if (!g_running.exchange(false)) return;
    g_logCv.notify_all();
    {
        std::lock_guard<std::mutex> lk(g_sockMu);
        for (int fd : g_sockets) {
            shutdown(fd, SHUT_RDWR);
            close(fd);
        }
        g_sockets.clear();
    }
    if (g_listen >= 0) {
        shutdown(g_listen, SHUT_RDWR);
        close(g_listen);
        g_listen = -1;
    }
#ifdef __SWITCH__
    socketExit();
#endif
}

Injected injected() {
    Injected out;
    const uint64_t now = now_ns();
    std::lock_guard<std::mutex> lk(g_inMu);
    for (int i = 0; i < 16; i++) {
        if (g_until[i] && now >= g_until[i]) g_until[i] = 0;
        if (g_until[i]) out.buttons |= 1ull << i;
    }
    for (int s = 0; s < 2; s++) {
        Stick& st = g_sticks[s];
        if (st.until && now >= st.until) st.until = 0;
        if (st.until) out.stick[s] = true, out.x[s] = st.x, out.y[s] = st.y;
    }
    return out;
}

}  // namespace debugsrv
