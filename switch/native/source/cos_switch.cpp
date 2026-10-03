// The native port's Switch platform layer: logs, run options, system report, crash report, exit.
//
// Logs. stdout and stderr (the harness's "[cos] ..." lines, OSReport, Aurora's log) go, through a
// devoptab tee as in the translated port (switch/host/source/switch_main.c), to
//   - the live USB log (switch/source/common/usb_log.c; scripts/switch/usb_log.py on the computer);
//   - COS_SWITCH_ROOT/centollos.log on the SD card (the previous run's is kept as centollos.prev.log), written
//     by a thread of its own so a game thread never waits on the SD card. A crash or an exit
//     writes what is still queued before the process ends.
//
// Memory: the process's used and total memory at start, every 15 seconds (from the log writer
// thread) and at exit: "[switch] memory: used N MiB of M MiB".
//
// Run options: COS_SWITCH_ROOT/env.txt, one NAME=value per line (# comments), applied before the
// Switch defaults (setenv without overwrite): COS_DISC (the shared GZLE01.iso), COS_RUN_DIR (the
// native directory, for backtrace.txt), COS_PERF_EVERY=60, COS_STALL_S=90 and COS_ASPECT=16:9 (the
// console's 1280x720 screen; COS_ASPECT=4:3 in env.txt gives the GameCube picture, pillarboxed).
//
// Crash report: libnx's user exception handler prints the exception, the registers, the thread,
// the NRO's load address and a frame-pointer backtrace as offsets into centollos.elf (for addr2line),
// then the harness's state line (scene, frame, last resource), writes the logs out and returns the
// exception to the kernel unhandled, so Atmosphère still writes its crash report.
// abort() (Aurora's fatal log and asserts, newlib's assert, std::terminate) is wrapped
// (-Wl,--wrap=abort) to print the same backtrace and state and to write the logs out before the
// process ends; on its own it would end the process with the last log lines still queued.
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/iosupport.h>
#include <sys/stat.h>
#include <switch.h>
#include <unistd.h>

#include "cos_switch_internal.h"
#include "usb_log.h"

namespace {

constexpr const char* kLogPath = COS_SWITCH_ROOT "/centollos.log";
constexpr const char* kPrevLogPath = COS_SWITCH_ROOT "/centollos.prev.log";
constexpr size_t kRingSize = 1u << 20;

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

void startLogs() {
    mkdir("/switch", 0777);
    mkdir("/switch/centollos", 0777);
    mkdir(COS_SWITCH_ROOT, 0777);
    remove(kPrevLogPath);
    rename(kLogPath, kPrevLogPath);
    gLogFile = fopen(kLogPath, "w");
    mutexInit(&gRingLock);
    mutexInit(&gFileLock);
    condvarInit(&gRingChanged);
    gUsb = usb_log_start();
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
void loadEnvFile() {
    FILE* file = fopen(COS_SWITCH_ROOT "/env.txt", "r");
    if (file == nullptr) {
        sayf("[switch] no %s/env.txt; Switch defaults only\n", COS_SWITCH_ROOT);
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), file) != nullptr) {
        line[strcspn(line, "\r\n")] = '\0';
        char* equals = strchr(line, '=');
        if (line[0] == '#' || line[0] == '\0' || equals == nullptr) {
            continue;
        }
        *equals = '\0';
        setenv(line, equals + 1, 1);
        sayf("[switch] env.txt: %s=%s\n", line, equals + 1);
    }
    fclose(file);
}

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

} // namespace

extern "C" int cos_switch_describe_mode(char* out, size_t size) {
    const char* mode = appletGetOperationMode() == AppletOperationMode_Console ? "docked" : "handheld";
    u32 gpu = 0, emc = 0;
    const bool gpuOk = readClock(PcvModuleId_GPU, PcvModule_GPU, &gpu);
    const bool emcOk = readClock(PcvModuleId_EMC, PcvModule_EMC, &emc);
    if (!gpuOk && !emcOk) {
        return snprintf(out, size, "%s, clocks unavailable (%s rc 0x%x)", mode,
                        hosversionAtLeast(8, 0, 0) ? "clkrst" : "pcv", (unsigned)gClockServiceRc);
    }
    return snprintf(out, size, "%s, gpu %.1f MHz, emc %.1f MHz", mode, gpu / 1e6, emc / 1e6);
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
    sayf("[switch] centollos native: %s; %s; memory %llu MiB, %llu MiB used at start; core mask 0x%llx; "
         "image at 0x%llx\n",
         appletTypeName(applet), mode, (unsigned long long)(total >> 20), (unsigned long long)(used >> 20),
         (unsigned long long)cores, (unsigned long long)cos_switch_image_base());
    sayf("[switch] logs: %s %s, USB live log %s\n", kLogPath, gLogFile != nullptr ? "open" : "NOT open",
         gUsb ? "started" : "unavailable");
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
    // The NRO is a few hundred MiB at most; anything else is not in centollos.elf.
    if (addr >= base && addr - base < (1ull << 30)) {
        sayf("%s0x%llx (centollos.elf+0x%llx)", label, (unsigned long long)addr, (unsigned long long)(addr - base));
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
    sayf("[cos] image base=0x%llx: aarch64-none-elf-addr2line -f -C -i -e centollos.elf <offset>\n",
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
    sayf("[cos] exit 13 (crash); Atmosphère writes its report to atmosphere/crash_reports/\n");
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
    sayf("[cos] image base=0x%llx: aarch64-none-elf-addr2line -f -C -i -e centollos.elf <offset>\n",
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
    startLogs();
    sayf("[switch] the game, native port (phase 7); argv[0]=%s\n",
         argc > 0 && argv != nullptr && argv[0] != nullptr ? argv[0] : "-");
    reportSystem();
    loadEnvFile();
    setDefault("COS_DISC", COS_SWITCH_DEFAULT_DISC);
    setDefault("COS_RUN_DIR", COS_SWITCH_ROOT);
    setDefault("COS_PERF_EVERY", "60");
    setDefault("COS_HITCH_MS", "50");
    setDefault("COS_FPS_OVERLAY", "1");
    setDefault("COS_STALL_S", "90");
    setDefault("COS_ASPECT", "16:9");
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

void cos_switch_exit(int code) {
    static std::atomic<bool> sExiting{false};
    bool expected = false;
    if (!sExiting.compare_exchange_strong(expected, true)) {
        for (;;) {
            svcSleepThread(1000000000ULL);
        }
    }
    reportMemory();
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
