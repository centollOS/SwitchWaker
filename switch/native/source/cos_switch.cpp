// The native port's Switch platform layer: logs, run options, system report, crash report, exit.
//
// Logs. stdout and stderr (the harness's "[cos] ..." lines, OSReport, Aurora's log) go, through a
// devoptab tee as in the translated port (switch/host/source/switch_main.c), to
//   - the live USB log (switch/source/common/usb_log.c; scripts/switch/usb_log.py on the computer),
//     only with COS_USB_LOG=1 (off by default: it holds the USB port for the whole run);
//   - a session log on the SD card, COS_SWITCH_ROOT/logs/switchwaker_<date>_<time>.log (the console clock at
//     start; the GL NRO's are switchwaker_gl_<date>_<time>.log, COS_SWITCH_NRO_NAME), as SwitchWakerHD's:
//     the newest file in logs/ is always the current session, and logs/ keeps the
//     kMaxSessionLogs most recent sessions (the oldest are deleted, so the logs cannot fill the SD card).
//     The switchwaker.log / switchwaker.prev.log of earlier builds move into logs/. Written by a thread of
//     its own so a game thread never waits on the SD card. A crash or an exit writes what is still queued
//     before the process ends;
//   - the debug server's "log" streams (debug_server.h; Depuración > "Servidor de depuración" in the
//     options menu, COS_DEBUG_SERVER, cos_debug.cpp).
//
// Memory: the process's used and total memory at start, every 15 seconds (from the log writer
// thread) and at exit: "[switch] memory: used N MiB of M MiB". The shader cache's lines
// (cos_shader_cache.cpp) come with it while they change, and at exit.
//
// Run options: the options menu's settings file, COS_SWITCH_ROOT/user/settings.ini
// (native/include/pc/pc_settings.h): pc_settings_load_early copies its values, and the variables of
// its [dev] section (developer options without a menu row), into the environment, then the Switch
// defaults fill what is still unset (setenv without overwrite): COS_DISC (the shared GZLE01.iso),
// COS_RUN_DIR (the native directory, for backtrace.txt), COS_STALL_S=90, COS_ASPECT=16:9 (the
// console's 1280x720 screen) and COS_FB_SCALE=1.5. The defaults are for players: no perf or hitch
// lines, no frame-rate panel, no debug server, no USB log (the menu's Depuración tab turns them on).
// The native/env.txt of earlier builds is moved into the settings file once
// (pc_settings_migrate_env_file) and kept as env.txt.old.
// COS_SWITCH_GPU_PROFILE (460 by default, 384, default) picks the console's official handheld performance
// configuration through apm (CPU 1020 MHz always); the previous one is restored at exit. The options
// menu changes it at run time (cos_switch_set_gpu_profile).
//
// Crash report: libnx's user exception handler prints the exception, the registers, the thread,
// the NRO's load address and a frame-pointer backtrace as offsets into switchwaker.elf (for addr2line),
// then the harness's state line (scene, frame, last resource), writes the logs out and returns the
// exception to the kernel unhandled, so Atmosphère still writes its crash report; a reaper thread
// ends the process 5 s later if the kernel has not (startReaper).
// abort() (Aurora's fatal log and asserts, newlib's assert, std::terminate) is wrapped
// (-Wl,--wrap=abort) to print the same backtrace and state and to write the logs out before the
// process ends; on its own it would end the process with the last log lines still queued.
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <ctime>
#include <dirent.h>
#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/iosupport.h>
#include <sys/stat.h>
#include <switch.h>
#include <unistd.h>

#include "cos_switch_internal.h"
#include "debug_server.h"
#include "usb_log.h"

extern "C" void pc_settings_load_early(void); // native/src/pc/features/pc_settings.cpp
extern "C" int pc_settings_migrate_env_file(const char* envPath, const char* oldPath);

namespace {

constexpr const char* kLogDir = COS_SWITCH_ROOT "/logs";
constexpr size_t kMaxSessionLogs = 10; // the current session's included
constexpr size_t kRingSize = 1u << 20;
char gLogPath[128];                     // this session's log

// ---- SD card log ---------------------------------------------------------------------------------
uint8_t gRing[kRingSize];
size_t gHead; // next byte to write
size_t gTail; // next byte to store on the SD card
Mutex gRingLock;
CondVar gRingChanged;
Mutex gFileLock; // the FILE and the order of what is stored
FILE* gLogFile;
Thread gWriter;
std::atomic<bool> gWriterRunning{false};
std::atomic<bool> gCrashing{false};
constexpr u64 kReaperDelayNs = 5000000000ULL; // crash to the end of the process (reaperMain)
std::atomic<unsigned> gDropped{0};
bool gUsb;

size_t queuedLocked() { return (gHead + kRingSize - gTail) % kRingSize; }

// mutexLock, or after a crash a bounded try: the crashed thread may hold the lock.
bool lockOrGiveUp(Mutex* m) {
    if (!gCrashing.load(std::memory_order_relaxed)) {
        mutexLock(m);
        return true;
    }
    for (int i = 0; i < 200; i++) {
        if (mutexTryLock(m)) {
            return true;
        }
        svcSleepThread(1000000ULL);
    }
    return false;
}

// Moves what is queued to the SD card. With `all`, until the ring is empty.
void storeQueued() {
    const bool fileLocked = lockOrGiveUp(&gFileLock);
    static uint8_t chunk[64 * 1024];
    for (;;) {
        const bool ringLocked = lockOrGiveUp(&gRingLock);
        size_t size = queuedLocked();
        if (size > sizeof(chunk)) {
            size = sizeof(chunk);
        }
        const size_t first = kRingSize - gTail < size ? kRingSize - gTail : size;
        memcpy(chunk, gRing + gTail, first);
        memcpy(chunk + first, gRing, size - first);
        gTail = (gTail + size) % kRingSize;
        condvarWakeAll(&gRingChanged);
        if (ringLocked) {
            mutexUnlock(&gRingLock);
        }
        if (size == 0) {
            break;
        }
        if (gLogFile != nullptr) {
            fwrite(chunk, 1, size, gLogFile);
        }
    }
    if (gLogFile != nullptr) {
        fflush(gLogFile);
    }
    if (fileLocked) {
        mutexUnlock(&gFileLock);
    }
}

void sayf(const char* format, ...) __attribute__((format(printf, 1, 2)));

void reportMemory() {
    u64 total = 0, used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    sayf("[switch] memory: used %llu MiB of %llu MiB\n", (unsigned long long)(used >> 20),
         (unsigned long long)(total >> 20));
}

void writerMain(void*) {
    u64 lastMemoryReport = armGetSystemTick();
    while (gWriterRunning.load(std::memory_order_acquire)) {
        if (armTicksToNs(armGetSystemTick() - lastMemoryReport) >= 15000000000ULL) {
            lastMemoryReport = armGetSystemTick();
            reportMemory();
            char shaders[2048];
            if (cos_switch_shader_cache_report(shaders, sizeof(shaders), 0) > 0) {
                sayf("%s", shaders);
            }
        }
        mutexLock(&gRingLock);
        if (queuedLocked() == 0) {
            condvarWaitTimeout(&gRingChanged, &gRingLock, 100000000ULL);
        }
        mutexUnlock(&gRingLock);
        storeQueued();
    }
}

void queueBytes(const char* data, size_t size) {
    usb_log_write(data, size);
    if (!gCrashing.load(std::memory_order_relaxed)) {
        debugsrv::log_tap(data, size); // (it takes a lock: not after a crash)
    }
    if (gLogFile == nullptr) {
        return;
    }
    const bool locked = lockOrGiveUp(&gRingLock);
    while (size > 0) {
        size_t space = kRingSize - 1 - queuedLocked();
        if (space == 0) {
            // The SD card is behind: wait for the writer a little, then drop.
            if (!locked || !gWriterRunning.load() ||
                R_FAILED(condvarWaitTimeout(&gRingChanged, &gRingLock, 50000000ULL)) ||
                (space = kRingSize - 1 - queuedLocked()) == 0) {
                gDropped.fetch_add((unsigned)size);
                break;
            }
        }
        const size_t n = size < space ? size : space;
        for (size_t i = 0; i < n; i++) {
            gRing[gHead] = (uint8_t)data[i];
            gHead = (gHead + 1) % kRingSize;
        }
        data += n;
        size -= n;
    }
    condvarWakeAll(&gRingChanged);
    if (locked) {
        mutexUnlock(&gRingLock);
    }
}

ssize_t teeWrite(struct _reent*, void*, const char* data, size_t size) {
    queueBytes(data, size);
    return (ssize_t)size;
}

const devoptab_t gTee = {
    .name = "tee",
    .write_r = teeWrite,
};

void sayf(const char* format, ...) {
    char line[1024];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (n < 0) {
        return;
    }
    if (n >= (int)sizeof(line)) {
        n = sizeof(line) - 1;
    }
    queueBytes(line, (size_t)n);
}

// logs/<NRO name>_<date>_<time>.log for a file's time
void sessionLogName(char* out, size_t size, time_t when) {
    struct tm t{};
    localtime_r(&when, &t);
    snprintf(out, size, "%s/" COS_SWITCH_NRO_NAME "_%04d-%02d-%02d_%02d-%02d-%02d.log", kLogDir, t.tm_year + 1900, t.tm_mon + 1,
             t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
}

// This session's file in logs/, the logs of builds before session logs moved there (named by their
// modification time), and room for this session: the oldest session logs go (modification time, then name).
void startSessionLog() {
    mkdir(kLogDir, 0777);
    for (const char* old : {COS_SWITCH_ROOT "/switchwaker.prev.log", COS_SWITCH_ROOT "/switchwaker.log"}) {
        struct stat st;
        if (stat(old, &st) != 0) {
            continue;
        }
        char name[128];
        sessionLogName(name, sizeof(name), st.st_mtime);
        if (stat(name, &st) == 0 || rename(old, name) != 0) {
            remove(old);
        }
    }
    sessionLogName(gLogPath, sizeof(gLogPath), time(nullptr));
    struct Log {
        std::string name;
        time_t mtime;
    };
    std::vector<Log> logs;
    if (DIR* dir = opendir(kLogDir)) {
        while (dirent* e = readdir(dir)) {
            if (strncmp(e->d_name, "switchwaker_", 12) != 0) {
                continue;
            }
            const std::string path = std::string(kLogDir) + "/" + e->d_name;
            if (path == gLogPath) {
                continue; // (a restart within the same second: reopened below)
            }
            struct stat st;
            logs.push_back({path, stat(path.c_str(), &st) == 0 ? st.st_mtime : 0});
        }
        closedir(dir);
    }
    std::sort(logs.begin(), logs.end(), [](const Log& a, const Log& b) {
        return a.mtime != b.mtime ? a.mtime < b.mtime : a.name < b.name;
    });
    for (size_t i = 0; i + kMaxSessionLogs <= logs.size(); i++) {
        remove(logs[i].name.c_str());
    }
}

void startLogs() {
    mkdir("/switch", 0777);
    mkdir("/switch/switchwaker", 0777);
    mkdir(COS_SWITCH_ROOT, 0777);
    startSessionLog();
    gLogFile = fopen(gLogPath, "w");
    mutexInit(&gRingLock);
    mutexInit(&gFileLock);
    condvarInit(&gRingChanged);
    if (gLogFile != nullptr) {
        gWriterRunning.store(true, std::memory_order_release);
        // Core 2, away from the game thread (core 0); the process's default core if that fails.
        bool started = R_SUCCEEDED(threadCreate(&gWriter, writerMain, nullptr, nullptr, 0x10000, 0x2C, 2)) ||
                       R_SUCCEEDED(threadCreate(&gWriter, writerMain, nullptr, nullptr, 0x10000, 0x2C, -2));
        if (!started || R_FAILED(threadStart(&gWriter))) {
            gWriterRunning.store(false);
        }
    }
    devoptab_list[STD_OUT] = &gTee;
    devoptab_list[STD_ERR] = &gTee;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

// ---- run options ---------------------------------------------------------------------------------
void setDefault(const char* name, const char* value) {
    if (getenv(name) == nullptr) {
        setenv(name, value, 0);
        sayf("[switch] default: %s=%s\n", name, value);
    }
}

// ---- system report -------------------------------------------------------------------------------
// ---- operation mode and clocks ---------------------------------------------------------------------
// clkrst (firmware 8.0.0+) or pcv (older): the GPU and memory controller clocks change with the
// operation mode (handheld GPU 307.2/384 MHz, docked 768 MHz at stock), which sets the GPU budget.
int gClockService = 0; // 0 not opened yet, 1 clkrst, 2 pcv, -1 unavailable
Result gClockServiceRc = 0;

bool readClock(PcvModuleId id, PcvModule legacy, u32* hz) {
    if (gClockService == 0) {
        if (hosversionAtLeast(8, 0, 0)) {
            gClockServiceRc = clkrstInitialize();
            gClockService = R_SUCCEEDED(gClockServiceRc) ? 1 : -1;
        } else {
            gClockServiceRc = pcvInitialize();
            gClockService = R_SUCCEEDED(gClockServiceRc) ? 2 : -1;
        }
    }
    if (gClockService == 1) {
        ClkrstSession session;
        if (R_FAILED(clkrstOpenSession(&session, id, 3))) {
            return false;
        }
        const Result rc = clkrstGetClockRate(&session, hz);
        clkrstCloseSession(&session);
        return R_SUCCEEDED(rc);
    }
    if (gClockService == 2) {
        return R_SUCCEEDED(pcvGetClockRate(legacy, hz));
    }
    return false;
}

const char* appletTypeName(AppletType type) {
    switch (type) {
    case AppletType_Application: return "application (title mode)";
    case AppletType_SystemApplication: return "system application";
    case AppletType_LibraryApplet: return "library applet (album/applet mode)";
    case AppletType_SystemApplet: return "system applet";
    case AppletType_OverlayApplet: return "overlay applet";
    default: return "unknown";
    }
}

// ---- handheld GPU profile (apm) -------------------------------------------------------------------
// The console's official performance configurations, set through apm the way a retail game asks for
// them: the CPU stays at the stock 1020 MHz, only the handheld GPU clock goes up. From switchbrew,
// PTM services, "PerformanceConfiguration" (https://switchbrew.org/wiki/PTM_services):
//   id          CPU     GPU     EMC
//   0x00020003  1020.0  307.2   1331.2   handheld default
//   0x00020004  1020.0  384.0   1331.2
//   0x92220008  1020.0  460.8   1331.2
//   0x00010001  1020.0  768.0   1600.0   docked default (left alone: nothing faster at CPU 1020)
//   0x00010000  1020.0  384.0   1600.0 / 0x92220007 1020.0 460.8 1600.0   (EMC 1600; not used here:
//               switchbrew does not say they are accepted in handheld, try with COS_SWITCH_GPU_PROFILE=0x...)
// ApmPerformanceMode_Normal is handheld, _Boost docked. apm keeps one configuration per mode and
// switches between them itself when the console is docked or undocked (libnx's own
// __nx_applet_PerformanceConfiguration sets both once at start), so nothing is re-applied on a
// mode change. libnx's appletInitialize already opened apm for an application (apmInitialize is
// reference counted); in applet mode apm is not available to homebrew and the profile is skipped.
// COS_SWITCH_GPU_PROFILE=460 (default) | 384 | default | 0x<configuration id>; 460 falls back to
// 384, then to the system's own configuration, logging each Result.
bool gApmChanged = false;
bool gApmHaveSaved = false;
u32 gApmSaved = 0;

// Applies one COS_SWITCH_GPU_PROFILE value; at start (the settings file already applied) and
// from the options menu. True if a configuration was accepted or nothing had to be done.
bool setGpuProfile(const char* profile) {
    if (profile == nullptr || profile[0] == '\0') {
        profile = "460";
    }
    const AppletType applet = appletGetAppletType();
    if (applet != AppletType_Application && applet != AppletType_SystemApplication) {
        sayf("[switch] gpu profile %s skipped: apm needs title mode (application)\n", profile);
        return false;
    }
    if (strcmp(profile, "default") == 0) {
        if (!gApmChanged) {
            sayf("[switch] gpu profile: default (handheld configuration left to the system)\n");
            return true;
        }
        const Result rc = apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, gApmSaved);
        sayf("[switch] gpu profile: default, restored handheld configuration 0x%08x: rc 0x%x\n",
             (unsigned)gApmSaved, (unsigned)rc);
        gApmChanged = false;
        return R_SUCCEEDED(rc);
    }
    u32 chain[3] = {};
    int count = 0;
    if (strcmp(profile, "460") == 0) {
        chain[count++] = 0x92220008;
        chain[count++] = 0x00020004;
    } else if (strcmp(profile, "384") == 0) {
        chain[count++] = 0x00020004;
    } else {
        chain[count++] = (u32)strtoul(profile, nullptr, 0);
    }
    Result rc = apmInitialize();
    if (R_FAILED(rc)) {
        sayf("[switch] gpu profile %s: apmInitialize failed rc 0x%x; system default kept\n", profile,
             (unsigned)rc);
        return false;
    }
    if (!gApmHaveSaved) {
        rc = apmGetPerformanceConfiguration(ApmPerformanceMode_Normal, &gApmSaved);
        sayf("[switch] gpu profile %s: handheld configuration before 0x%08x (rc 0x%x)\n", profile,
             (unsigned)gApmSaved, (unsigned)rc);
        if (R_FAILED(rc)) {
            gApmSaved = 0x00020003; // the handheld default, to restore at exit
        }
        gApmHaveSaved = true;
    }
    for (int i = 0; i < count; i++) {
        rc = apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, chain[i]);
        sayf("[switch] gpu profile: set handheld configuration 0x%08x: rc 0x%x%s\n", (unsigned)chain[i],
             (unsigned)rc, R_SUCCEEDED(rc) ? "" : " (failed)");
        if (R_SUCCEEDED(rc)) {
            gApmChanged = true;
            return true;
        }
    }
    sayf("[switch] gpu profile: no configuration accepted; system default kept\n");
    return false;
}

void applyGpuProfile() {
    setGpuProfile(getenv("COS_SWITCH_GPU_PROFILE"));
}

void restoreGpuProfile() {
    if (!gApmChanged) {
        return;
    }
    gApmChanged = false;
    const Result rc = apmSetPerformanceConfiguration(ApmPerformanceMode_Normal, gApmSaved);
    sayf("[switch] gpu profile: restored handheld configuration 0x%08x: rc 0x%x\n", (unsigned)gApmSaved,
         (unsigned)rc);
}

} // namespace

extern "C" int cos_switch_docked(void) {
    return appletGetOperationMode() == AppletOperationMode_Console ? 1 : 0;
}

extern "C" int cos_switch_set_gpu_profile(const char* profile) {
    const bool ok = setGpuProfile(profile);
    char mode[160];
    cos_switch_describe_mode(mode, sizeof(mode));
    sayf("[switch] clocks after the gpu profile: %s\n", mode);
    return ok ? 1 : 0;
}

extern "C" int cos_switch_describe_mode(char* out, size_t size) {
    const char* mode = appletGetOperationMode() == AppletOperationMode_Console ? "docked" : "handheld";
    // The configuration apm is applying now (application only; 0 when it cannot be read).
    u32 config = 0;
    const AppletType applet = appletGetAppletType();
    if (applet == AppletType_Application || applet == AppletType_SystemApplication) {
        appletGetCurrentPerformanceConfiguration(&config);
    }
    u32 cpu = 0, gpu = 0, emc = 0;
    const bool cpuOk = readClock(PcvModuleId_CpuBus, PcvModule_CpuBus, &cpu);
    const bool gpuOk = readClock(PcvModuleId_GPU, PcvModule_GPU, &gpu);
    const bool emcOk = readClock(PcvModuleId_EMC, PcvModule_EMC, &emc);
    if (!cpuOk && !gpuOk && !emcOk) {
        return snprintf(out, size, "%s, config 0x%08x, clocks unavailable (%s rc 0x%x)", mode,
                        (unsigned)config, hosversionAtLeast(8, 0, 0) ? "clkrst" : "pcv",
                        (unsigned)gClockServiceRc);
    }
    return snprintf(out, size, "%s, config 0x%08x, cpu %.1f MHz, gpu %.1f MHz, emc %.1f MHz", mode,
                    (unsigned)config, cpu / 1e6, gpu / 1e6, emc / 1e6);
}

extern "C" void cos_switch_system_language(char* out, size_t size) {
    if (size == 0) {
        return;
    }
    out[0] = '\0';
    if (R_FAILED(setInitialize())) {
        return;
    }
    u64 code = 0;
    if (R_SUCCEEDED(setGetSystemLanguage(&code))) {
        // The language code is an ASCII string of up to 8 bytes ("es-419"), zero padded.
        char text[9] = {};
        memcpy(text, &code, 8);
        snprintf(out, size, "%s", text);
    }
    setExit();
}

namespace {

void reportSystem() {
    u64 total = 0, used = 0, cores = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&cores, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
    const AppletType applet = appletGetAppletType();
    char mode[128];
    cos_switch_describe_mode(mode, sizeof(mode));
    sayf("[switch] SwitchWaker: %s; %s; memory %llu MiB, %llu MiB used at start; core mask 0x%llx; "
         "image at 0x%llx\n",
         appletTypeName(applet), mode, (unsigned long long)(total >> 20), (unsigned long long)(used >> 20),
         (unsigned long long)cores, (unsigned long long)cos_switch_image_base());
    sayf("[switch] log: %s %s (the newest %zu sessions are kept in %s)\n", gLogPath,
         gLogFile != nullptr ? "open" : "NOT open", kMaxSessionLogs, kLogDir);
    if (applet != AppletType_Application && applet != AppletType_SystemApplication) {
        sayf("[switch] WARNING: not started as an application: applets get far less memory than "
             "the game needs. Hold R while starting an installed game to open the Homebrew Menu in "
             "title mode.\n");
    }
    // MEM1 (256 MiB, decision H5), ARAM (16 MiB), Aurora and Dawn: about 1 GiB with headroom.
    if (total < (1ull << 30)) {
        sayf("[switch] WARNING: %llu MiB is less than the 1 GiB the game is expected to need\n",
             (unsigned long long)(total >> 20));
    }
}

// ---- crash report --------------------------------------------------------------------------------
void (*gStateWriter)(int) = nullptr;

const char* exceptionName(u32 desc) {
    switch (desc) {
    case ThreadExceptionDesc_InstructionAbort: return "instruction abort";
    case ThreadExceptionDesc_MisalignedPC: return "misaligned PC";
    case ThreadExceptionDesc_MisalignedSP: return "misaligned SP";
    case ThreadExceptionDesc_SError: return "SError";
    case ThreadExceptionDesc_BadSVC: return "bad SVC";
    case ThreadExceptionDesc_Trap: return "trap";
    case ThreadExceptionDesc_Other: return "data abort or other";
    default: return "unknown";
    }
}

void writeAddress(const char* label, uintptr_t addr) {
    const uintptr_t base = cos_switch_image_base();
    // The NRO is a few hundred MiB at most; anything else is not in switchwaker.elf.
    if (addr >= base && addr - base < (1ull << 30)) {
        sayf("%s0x%llx (" COS_SWITCH_NRO_NAME ".elf+0x%llx)", label, (unsigned long long)addr, (unsigned long long)(addr - base));
    } else {
        sayf("%s0x%llx", label, (unsigned long long)addr);
    }
}

void writeBacktrace(uintptr_t fp, int depth) {
    for (; fp != 0 && depth < 64; depth++) {
        uintptr_t next = 0, ret = 0;
        if (!cos_switch_read_word(fp, &next) || !cos_switch_read_word(fp + 8, &ret) || ret == 0) {
            break;
        }
        char label[32];
        snprintf(label, sizeof(label), "[cos]   #%d ", depth);
        writeAddress(label, ret - 4);
        sayf("\n");
        if (next <= fp) {
            break;
        }
        fp = next;
    }
}

void crashReport(ThreadExceptionDump* ctx) {
    sayf("\n[cos] CRASH %s (0x%x) esr=0x%x far=0x%llx\n", exceptionName(ctx->error_desc), ctx->error_desc,
         ctx->esr, (unsigned long long)ctx->far.x);
    if (ctx->far.x != 0 && ctx->far.x < 0x100000000ull) {
        sayf("[cos] hint: fault address below 4 GiB: a pointer truncated to 32 bits, a 32-bit offset used "
             "as a pointer, or NULL plus an offset\n");
    }
    sayf("[cos] thread %llu\n", (unsigned long long)cos_switch_thread_id());
    writeAddress("[cos] pc=", ctx->pc.x);
    writeAddress(" lr=", ctx->lr.x);
    sayf(" fp=0x%llx sp=0x%llx\n", (unsigned long long)ctx->fp.x, (unsigned long long)ctx->sp.x);
    for (int r = 0; r < 29; r++) {
        sayf("%sx%d=0x%llx%s", (r % 4) == 0 ? "[cos]   " : " ", r, (unsigned long long)ctx->cpu_gprs[r].x,
             (r % 4) == 3 || r == 28 ? "\n" : "");
    }
    sayf("[cos] image base=0x%llx: aarch64-none-elf-addr2line -f -C -i -e " COS_SWITCH_NRO_NAME ".elf <offset>\n",
         (unsigned long long)cos_switch_image_base());
    sayf("[cos] backtrace (pc, lr, then the frame records' return addresses as call sites):\n");
    writeAddress("[cos]   #0 ", ctx->pc.x);
    sayf("\n");
    writeAddress("[cos]   #1 ", ctx->lr.x);
    sayf("\n");
    writeBacktrace(ctx->fp.x, 2);
    if (gStateWriter != nullptr) {
        sayf("[cos] state: ");
        gStateWriter(STDERR_FILENO);
    }
    reportMemory();
    sayf("[cos] exit 13 (crash); from hbmenu Atmosphère writes its report to atmosphere/crash_reports/, "
         "from the HOME menu forwarder this log is the report (the process is ended in %llu s)\n",
         (unsigned long long)(kReaperDelayNs / 1000000000ULL));
}

// The crash reaper: from hbmenu the kernel ends the process once Atmosphère has written its
// report, but run as an application (the HOME menu forwarder) there is no report and the process
// stays alive with the game thread stopped and the audio threads still playing, until the stall
// watchdog ends it 90 s later. This thread, started with the logs, ends it a few seconds after
// the crash report instead (the report and the logs are written by then).
Thread gReaper;

void reaperMain(void*) {
    while (!gCrashing.load(std::memory_order_acquire)) {
        svcSleepThread(250000000ULL);
    }
    svcSleepThread(kReaperDelayNs);
    svcExitProcess();
}

void startReaper() {
    if (R_SUCCEEDED(threadCreate(&gReaper, reaperMain, nullptr, nullptr, 0x2000, 0x2C, -2))) {
        threadStart(&gReaper);
    }
}

} // namespace

extern "C" {

// libnx's user exception handler and the stack it runs on (one for every thread: a second
// exception while the first is reported waits for the process to end).
alignas(16) u8 __nx_exception_stack[0x20000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    bool expected = false;
    if (!gCrashing.compare_exchange_strong(expected, true)) {
        for (;;) {
            svcSleepThread(1000000000ULL);
        }
    }
    crashReport(ctx);
    restoreGpuProfile();
    cos_switch_flush_logs();
    usb_log_stop(1000);
    // Not handled: the kernel goes on as without this handler (Atmosphère's crash report, then the
    // process ends). 0xF801 is what libnx returns for an exception it leaves to a debugger.
    svcReturnFromException(0xF801);
}

__attribute__((noreturn)) void __real_abort(void);

__attribute__((noreturn)) void __wrap_abort(void) {
    static std::atomic<bool> sAborting{false};
    bool expected = false;
    if (!sAborting.compare_exchange_strong(expected, true)) {
        __real_abort();
    }
    sayf("\n[cos] ABORT (abort() called) on thread %llu\n", (unsigned long long)cos_switch_thread_id());
    sayf("[cos] image base=0x%llx: aarch64-none-elf-addr2line -f -C -i -e " COS_SWITCH_NRO_NAME ".elf <offset>\n",
         (unsigned long long)cos_switch_image_base());
    sayf("[cos] backtrace (call sites):\n");
    writeBacktrace((uintptr_t)__builtin_frame_address(0), 0);
    if (gStateWriter != nullptr) {
        sayf("[cos] state: ");
        gStateWriter(STDERR_FILENO);
    }
    cos_switch_exit(134);
}

void cos_switch_start(int argc, char** argv) {
    debugsrv::keep_log(); // the debug server's log text from the first line (cos_switch_debug_start)
    startLogs();
    startReaper();
    sayf("[switch] SwitchWaker, native port (phase 7); argv[0]=%s\n",
         argc > 0 && argv != nullptr && argv[0] != nullptr ? argv[0] : "-");
    reportSystem();
    // The env.txt of earlier builds: its lines move into the settings file (menu settings as such,
    // the rest into [dev]) before it is read, so they apply in this session already.
    pc_settings_migrate_env_file(COS_SWITCH_ROOT "/env.txt", COS_SWITCH_ROOT "/env.txt.old");
    // The options menu's settings file (native/include/pc/pc_settings.h), [dev] included: before
    // the Switch defaults below, which it overrides.
    pc_settings_load_early();
    // COS_DEBUG_SERVER (Depuración > "Servidor de depuración", at the next start): the debug server
    // for development (cos_debug.cpp), off by default.
    cos_switch_debug_start();
    // The USB live log (scripts/switch/usb_log.py) holds the console's USB port as 057e:3000 for
    // the whole run, so it is off by default (players: the port stays free, e.g. for SysDVR's USB
    // mode); Depuración > "Registro en directo por USB" (COS_USB_LOG=1, at the next start) turns it
    // on for development. The log file on the SD card is written either way. Started after the
    // settings file, so the lines before it are only in the file.
    setDefault("COS_USB_LOG", "0");
    {
        const char* usb = getenv("COS_USB_LOG");
        if (usb != nullptr && strcmp(usb, "0") == 0) {
            sayf("[switch] USB live log off (COS_USB_LOG=1 turns it on): the USB port is free\n");
        } else {
            gUsb = usb_log_start();
            sayf("[switch] USB live log %s\n", gUsb ? "started" : "unavailable");
        }
    }
    setDefault("COS_DISC", COS_SWITCH_DEFAULT_DISC);
    setDefault("COS_RUN_DIR", COS_SWITCH_ROOT);
    setDefault("COS_STALL_S", "90");
    setDefault("COS_ASPECT", "16:9");
    // The internal resolution: 1280x720, the screen's (COS_FB_SCALE=1.125 960x540, 1.0 854x480).
    setDefault("COS_FB_SCALE", "1.5");
    applyGpuProfile();
    char mode[160];
    cos_switch_describe_mode(mode, sizeof(mode));
    sayf("[switch] clocks after the gpu profile: %s\n", mode);
#if defined(COS_SWITCH_DEKO3D)
    // The deko3d NRO starts no EGL: no Mesa shader cache (its shaders are the DKSH cache, switch/deko).
    sayf("[switch] renderer: deko3d (switch/deko); Mesa's shader cache unused\n");
#else
    // Before Aurora starts EGL, which creates Mesa's shader cache (cos_shader_cache.cpp).
    char note[640];
    if (cos_switch_shader_cache_setup(note, sizeof(note)) > 0) {
        sayf("%s", note);
    }
#endif
}

void cos_switch_flush_logs(void) {
    storeQueued();
    if (gDropped.load() != 0) {
        char line[96];
        const int n = snprintf(line, sizeof(line), "[switch] %u log bytes dropped (SD card behind)\n",
                               gDropped.exchange(0));
        if (gLogFile != nullptr && n > 0) {
            fwrite(line, 1, (size_t)n, gLogFile);
            fflush(gLogFile);
        }
    }
}

void cos_switch_show_error(const char* text) {
    if (appletGetAppletType() != AppletType_Application) {
        sayf("[switch] error message not shown (needs title mode): %s\n", text);
        return;
    }
    ErrorApplicationConfig c;
    // the short dialog and the details page show the same text
    if (R_SUCCEEDED(errorApplicationCreate(&c, text, text))) {
        const Result rc = errorApplicationShow(&c);
        sayf("[switch] error message shown (rc 0x%x)\n", rc);
    }
}

void cos_switch_exit(int code) {
    static std::atomic<bool> sExiting{false};
    bool expected = false;
    if (!sExiting.compare_exchange_strong(expected, true)) {
        for (;;) {
            svcSleepThread(1000000000ULL);
        }
    }
    restoreGpuProfile();
    reportMemory();
    char shaders[2048];
    if (cos_switch_shader_cache_report(shaders, sizeof(shaders), 1) > 0) {
        sayf("%s", shaders);
    }
    sayf("[switch] exit %d after %u threads; ending the process\n", code, cos_switch_threads_created());
    cos_switch_flush_logs();
    usb_log_stop(1000);
    // The game's threads (JAudio, DVD, Aurora's workers, Dawn's) cannot be stopped from here, and
    // hbloader would start the Homebrew Menu in this same process next to them: end the process.
    svcExitProcess();
}

// switch.ld defines __start__ as the absolute symbol 0 (the NRO is linked at 0), so its address is
// 0 at run time too, whatever the load address: the crash reports of the first console run said
// "image base=0x0". The base is the start of the mapping that holds this function: hbloader maps
// the NRO's text segment, which begins at offset 0 (crt0), as one read-execute block.
void cos_switch_restart(void) {
    restoreGpuProfile();
    reportMemory();
    sayf("[switch] restart (debug server reload): the forwarder loads the NRO again\n");
    cos_switch_flush_logs();
    usb_log_stop(1000);
    static const char kArg[] = "switchwaker debug reload";
    const Result rc = appletRestartProgram(kArg, sizeof(kArg));
    // (on success the system ends this process)
    applyGpuProfile();
    sayf("[switch] restart failed: appletRestartProgram rc 0x%x\n", rc);
}

uintptr_t cos_switch_image_base(void) {
    static uintptr_t sBase;
    if (sBase == 0) {
        MemoryInfo info{};
        u32 page = 0;
        const uintptr_t here = reinterpret_cast<uintptr_t>(&cos_switch_image_base);
        if (R_SUCCEEDED(svcQueryMemory(&info, &page, here)) && info.type != MemType_Unmapped) {
            sBase = info.addr;
        }
    }
    return sBase;
}

uint64_t cos_switch_thread_id(void) {
    u64 id = 0;
    svcGetThreadId(&id, CUR_THREAD_HANDLE);
    return id;
}

int cos_switch_read_word(uintptr_t addr, uintptr_t* out) {
    if (addr == 0 || (addr & 7) != 0 || out == nullptr) {
        return 0;
    }
    MemoryInfo info{};
    u32 page = 0;
    if (R_FAILED(svcQueryMemory(&info, &page, addr)) || info.type == MemType_Unmapped ||
        (info.perm & Perm_R) == 0 || addr + 8 > info.addr + info.size) {
        return 0;
    }
    *out = *reinterpret_cast<const uintptr_t*>(addr);
    return 1;
}

void cos_switch_set_crash_state_writer(void (*writer)(int fd)) { gStateWriter = writer; }

} // extern "C"
