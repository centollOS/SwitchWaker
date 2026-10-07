// The settings file and registry of the options menu (native/include/pc/pc_settings.h).
//
// The file is a list of NAME=value lines (NAME@handheld=value / NAME@docked=value for per-mode
// settings), rewritten whole after each change (to <path>.tmp, then renamed over the file, so a
// crash or a HOME-kill mid-write leaves the old file). Values the environment fixed at start are
// locked: they are never written and the menu cannot change them.
#include "pc/pc_settings.h"

#include "pc_internal.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#if defined(__SWITCH__)
#include "cos_switch.h"
#endif

extern char** environ;

namespace {

// stored[0]: the plain NAME=value line; stored[1 + mode]: NAME@<mode>=value.
constexpr int kSlots = 1 + PC_MODES;
const char* const kModeSuffix[PC_MODES] = {"handheld", "docked"};
const char* const kModeName[PC_MODES] = {"Portátil", "Sobremesa"};

struct Subscriber {
    PcSettingApplyFn fn;
    void* user;
};

struct Entry {
    PcSettingDesc desc{};
    bool registered = false;
    std::string stored[kSlots];
    bool has[kSlots] = {};
    std::string bootValue; // a PC_SETTING_RESTART setting's value when it was registered
    std::vector<Subscriber> subscribers;
};

struct State {
    bool loaded = false;
    std::string path;
    std::map<std::string, std::string> envAtStart; // COS_* (and MESA_*) variables before the file
    std::set<std::string> fileSet;                 // variables pc_settings_load_early set
    std::map<std::string, Entry> entries;
    std::vector<std::string> order;                // registered keys, menu order
    bool orderDirty = false;
    PcOperationMode lastMode = PC_MODE_HANDHELD;
    bool haveLastMode = false;
    unsigned int pollCount = 0;
};

State& st() {
    static State* s = new State();
    return *s;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) {
        b++;
    }
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) {
        e--;
    }
    return s.substr(b, e - b);
}

void computePath() {
    State& s = st();
    if (!s.path.empty()) {
        return;
    }
    if (const char* p = getenv("COS_SETTINGS"); p != nullptr && p[0] != '\0') {
        s.path = p;
        return;
    }
#if defined(__SWITCH__)
    s.path = COS_SWITCH_ROOT "/user/settings.ini";
#else
    char exe[PATH_MAX] = {};
    pc::executablePath(exe, sizeof(exe));
    char real[PATH_MAX];
    std::string dir = ".";
    if (exe[0] != '\0') {
        const char* path = realpath(exe, real) != nullptr ? real : exe;
        const char* slash = strrchr(path, '/');
        if (slash != nullptr) {
            dir.assign(path, (size_t)(slash - path));
        }
    }
    s.path = dir + "/user/settings.ini";
#endif
}

// Parses the file into the entries' stored values (clearing the old ones first).
void readFile() {
    State& s = st();
    for (auto& [key, e] : s.entries) {
        for (int i = 0; i < kSlots; i++) {
            e.stored[i].clear();
            e.has[i] = false;
        }
    }
    FILE* f = fopen(s.path.c_str(), "r");
    if (f == nullptr) {
        return;
    }
    char line[512];
    unsigned int count = 0;
    while (fgets(line, sizeof(line), f) != nullptr) {
        std::string l = trim(line);
        if (l.empty() || l[0] == '#') {
            continue;
        }
        const size_t eq = l.find('=');
        if (eq == std::string::npos || eq == 0) {
            continue;
        }
        std::string name = trim(l.substr(0, eq));
        const std::string value = trim(l.substr(eq + 1));
        int slot = 0;
        const size_t at = name.find('@');
        if (at != std::string::npos) {
            const std::string suffix = name.substr(at + 1);
            name = name.substr(0, at);
            slot = -1;
            for (int m = 0; m < PC_MODES; m++) {
                if (suffix == kModeSuffix[m]) {
                    slot = 1 + m;
                }
            }
            if (slot < 0) {
                continue;
            }
        }
        Entry& e = s.entries[name];
        e.stored[slot] = value;
        e.has[slot] = true;
        count++;
    }
    fclose(f);
    pc::writef(STDERR_FILENO, "[cos] settings: %u value(s) from %s\n", count, s.path.c_str());
}

void makeParents(const std::string& path) {
    for (size_t i = 1; i < path.size(); i++) {
        if (path[i] == '/') {
            const std::string dir = path.substr(0, i);
            mkdir(dir.c_str(), 0755);
        }
    }
}

bool writeFile() {
    State& s = st();
    makeParents(s.path);
    const std::string tmp = s.path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (f == nullptr) {
        pc::writef(STDERR_FILENO, "[cos] settings: cannot write %s: %s\n", tmp.c_str(), strerror(errno));
        return false;
    }
    fputs("# Ajustes de SwitchWaker, escritos por el menú de opciones\n"
          "# (ZL+ZR+Menos en la Switch; F1 o L+R+Z en el Mac). Una línea NOMBRE=valor, o\n"
          "# NOMBRE@handheld= / NOMBRE@docked= para los valores de cada modo. env.txt y las\n"
          "# variables de entorno mandan sobre este archivo.\n",
          f);
    for (const auto& [key, e] : s.entries) {
        for (int i = 0; i < kSlots; i++) {
            if (!e.has[i]) {
                continue;
            }
            if (i == 0) {
                fprintf(f, "%s=%s\n", key.c_str(), e.stored[i].c_str());
            } else {
                fprintf(f, "%s@%s=%s\n", key.c_str(), kModeSuffix[i - 1], e.stored[i].c_str());
            }
        }
    }
    const bool ok = fflush(f) == 0;
    fclose(f);
#ifdef __SWITCH__
    // Horizon's rename does not replace an existing file (EEXIST): drop the old one first. A kill
    // between the two calls leaves only the .tmp, and the next save writes it again.
    if (ok) {
        remove(s.path.c_str());
    }
#endif
    if (!ok || rename(tmp.c_str(), s.path.c_str()) != 0) {
        pc::writef(STDERR_FILENO, "[cos] settings: cannot replace %s: %s\n", s.path.c_str(), strerror(errno));
        return false;
    }
    return true;
}

Entry* find(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    auto it = st().entries.find(key);
    return it != st().entries.end() ? &it->second : nullptr;
}

bool isLocked(const char* key) {
    return st().envAtStart.count(key) != 0;
}

bool perMode(const Entry& e) {
    return e.registered && (e.desc.flags & PC_SETTING_PER_MODE) != 0;
}

std::string valueFor(const char* key, PcOperationMode mode) {
    State& s = st();
    auto env = s.envAtStart.find(key);
    if (env != s.envAtStart.end()) {
        return env->second;
    }
    Entry* e = find(key);
    if (e != nullptr) {
        if (perMode(*e) && e->has[1 + mode]) {
            return e->stored[1 + mode];
        }
        if (e->has[0]) {
            return e->stored[0];
        }
        if (perMode(*e)) {
            // A per-mode value saved for the other mode only: this mode still has its default.
        } else {
            for (int m = 0; m < PC_MODES; m++) {
                if (e->has[1 + m]) {
                    return e->stored[1 + m];
                }
            }
        }
    }
    // The platform default: what the environment holds unless the file put it there.
    if (s.fileSet.count(key) == 0) {
        if (const char* v = getenv(key); v != nullptr) {
            return v;
        }
    }
    if (e != nullptr && e->registered && e->desc.defaultValue != nullptr) {
        return e->desc.defaultValue;
    }
    return "";
}

void notify(Entry& e, const char* key, const std::string& value) {
    pc::writef(STDERR_FILENO, "[cos] settings: %s = %s%s\n", key, value.c_str(),
               e.registered && (e.desc.flags & PC_SETTING_RESTART) ? " (next start)" : "");
    if (e.registered && e.desc.apply != nullptr) {
        e.desc.apply(key, value.c_str(), e.desc.user);
    }
    for (const Subscriber& sub : e.subscribers) {
        sub.fn(key, value.c_str(), sub.user);
    }
}

void sortOrder() {
    State& s = st();
    if (!s.orderDirty) {
        return;
    }
    s.orderDirty = false;
    std::stable_sort(s.order.begin(), s.order.end(), [&](const std::string& a, const std::string& b) {
        const PcSettingDesc& da = s.entries[a].desc;
        const PcSettingDesc& db = s.entries[b].desc;
        if (da.tab != db.tab) {
            return da.tab < db.tab;
        }
        return da.order < db.order;
    });
}

PcOperationMode queryMode() {
#if defined(__SWITCH__)
    return cos_switch_docked() ? PC_MODE_DOCKED : PC_MODE_HANDHELD;
#else
    const char* v = getenv("COS_OPERATION_MODE");
    return v != nullptr && strcmp(v, "docked") == 0 ? PC_MODE_DOCKED : PC_MODE_HANDHELD;
#endif
}

// Thread-local copies for pc_settings_get's return value (a pointer that stays valid until the
// next call on the same thread).
const char* keep(const std::string& v) {
    static thread_local std::string sBuf[4];
    static thread_local unsigned int sNext = 0;
    std::string& b = sBuf[sNext++ % 4];
    b = v;
    return b.c_str();
}

} // namespace

extern "C" {

void pc_settings_load_early(void) {
    State& s = st();
    if (s.loaded) {
        return;
    }
    s.loaded = true;
    computePath();
    for (char** e = environ; e != nullptr && *e != nullptr; e++) {
        const char* eq = strchr(*e, '=');
        if (eq == nullptr) {
            continue;
        }
        std::string name(*e, (size_t)(eq - *e));
        if (name.rfind("COS_", 0) == 0 && name != "COS_SETTINGS" && name != "COS_OPERATION_MODE") {
            s.envAtStart[name] = eq + 1;
        }
    }
    readFile();
    s.lastMode = queryMode();
    s.haveLastMode = true;
    // Every value the file gives that the environment does not: into the environment, for code
    // that reads its variable once at start. Per-mode lines: the current mode's wins over a plain
    // line.
    unsigned int set = 0, locked = 0;
    for (auto& [key, e] : s.entries) {
        const char* value = nullptr;
        if (e.has[1 + s.lastMode]) {
            value = e.stored[1 + s.lastMode].c_str();
        } else if (e.has[0]) {
            value = e.stored[0].c_str();
        }
        if (value == nullptr) {
            continue;
        }
        if (s.envAtStart.count(key) != 0) {
            locked++;
            pc::writef(STDERR_FILENO, "[cos] settings: %s from the environment (%s) wins over the file (%s)\n",
                       key.c_str(), s.envAtStart[key].c_str(), value);
            continue;
        }
        setenv(key.c_str(), value, 1);
        s.fileSet.insert(key);
        set++;
    }
    pc::writef(STDERR_FILENO, "[cos] settings: mode %s; %u from the file, %u overridden by the environment\n",
               kModeSuffix[s.lastMode], set, locked);
}

void pc_settings_reload(void) {
    State& s = st();
    std::map<std::string, std::string> before;
    for (const std::string& key : s.order) {
        before[key] = valueFor(key.c_str(), s.lastMode);
    }
    readFile();
    for (const std::string& key : s.order) {
        const std::string now = valueFor(key.c_str(), s.lastMode);
        if (now != before[key]) {
            notify(s.entries[key], key.c_str(), now);
        }
    }
}

const char* pc_settings_path(void) {
    computePath();
    return st().path.c_str();
}

int pc_settings_register(const PcSettingDesc* desc) {
    if (desc == nullptr || desc->key == nullptr) {
        return -1;
    }
    State& s = st();
    Entry& e = s.entries[desc->key];
    const bool fresh = !e.registered;
    e.desc = *desc;
    e.registered = true;
    if (fresh) {
        s.order.push_back(desc->key);
        e.bootValue = valueFor(desc->key, s.lastMode);
    }
    s.orderDirty = true;
    return 0;
}

const char* pc_settings_get(const char* key) {
    return keep(valueFor(key, st().lastMode));
}

const char* pc_settings_get_mode(const char* key, PcOperationMode mode) {
    return keep(valueFor(key, mode));
}

int pc_settings_set_mode(const char* key, PcOperationMode mode, const char* value) {
    State& s = st();
    Entry* e = find(key);
    if (e == nullptr || !e->registered || isLocked(key) || value == nullptr || mode < 0 || mode >= PC_MODES) {
        return -1;
    }
    const std::string before = valueFor(key, s.lastMode);
    const int slot = perMode(*e) ? 1 + mode : 0;
    e->stored[slot] = value;
    e->has[slot] = true;
    if (!(e->desc.flags & PC_SETTING_NO_SAVE)) {
        writeFile();
    }
    const std::string now = valueFor(key, s.lastMode);
    if (now != before) {
        notify(*e, key, now);
    } else if (perMode(*e) && mode != s.lastMode) {
        pc::writef(STDERR_FILENO, "[cos] settings: %s@%s = %s (applies in that mode)\n", key, kModeSuffix[mode],
                   value);
    }
    return 0;
}

int pc_settings_set(const char* key, const char* value) {
    return pc_settings_set_mode(key, st().lastMode, value);
}

int pc_settings_locked(const char* key) {
    return key != nullptr && isLocked(key) ? 1 : 0;
}

int pc_settings_restart_pending(const char* key) {
    Entry* e = find(key);
    if (e == nullptr || !e->registered || !(e->desc.flags & PC_SETTING_RESTART)) {
        return 0;
    }
    return valueFor(key, st().lastMode) != e->bootValue ? 1 : 0;
}

int pc_settings_subscribe(const char* key, PcSettingApplyFn fn, void* user) {
    if (key == nullptr || fn == nullptr) {
        return -1;
    }
    st().entries[key].subscribers.push_back({fn, user});
    return 0;
}

int pc_settings_has_subscriber(const char* key) {
    Entry* e = find(key);
    return e != nullptr && !e->subscribers.empty() ? 1 : 0;
}

PcOperationMode pc_settings_mode(void) {
    State& s = st();
    if (!s.haveLastMode) {
        s.lastMode = queryMode();
        s.haveLastMode = true;
    }
    return s.lastMode;
}

const char* pc_settings_mode_name(PcOperationMode mode) {
    return mode >= 0 && mode < PC_MODES ? kModeName[mode] : "?";
}

void pc_settings_poll_mode(void) {
    State& s = st();
    // appletGetOperationMode is a service call: twice a second at 30 fps is enough.
    if (s.pollCount++ % 15 != 0) {
        return;
    }
    const PcOperationMode mode = queryMode();
    if (s.haveLastMode && mode == s.lastMode) {
        return;
    }
    const PcOperationMode old = s.lastMode;
    s.lastMode = mode;
    s.haveLastMode = true;
    pc::writef(STDERR_FILENO, "[cos] settings: operation mode %s -> %s\n", kModeSuffix[old], kModeSuffix[mode]);
    for (const std::string& key : s.order) {
        Entry& e = s.entries[key];
        if (!perMode(e) || (e.desc.flags & PC_SETTING_RESTART)) {
            continue;
        }
        const std::string a = valueFor(key.c_str(), old);
        const std::string b = valueFor(key.c_str(), mode);
        if (a != b) {
            notify(e, key.c_str(), b);
        }
    }
}

int pc_settings_count(void) {
    return (int)st().order.size();
}

const PcSettingDesc* pc_settings_at(int index) {
    State& s = st();
    sortOrder();
    if (index < 0 || index >= (int)s.order.size()) {
        return nullptr;
    }
    return &s.entries[s.order[index]].desc;
}

} // extern "C"
