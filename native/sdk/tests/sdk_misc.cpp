// cos_sdk_smoke: the "misc" test of step 2.6b (OS report/panic/fatal, reset, SRAM, RTC,
// stopwatch, font, PPC registers, caches, error handlers, console information).
//
// Functions that end the process (OSFatal, OSPanic, PPCHalt, OSResetSystem without a reset hook,
// ...) run in a forked child whose stderr is captured: the check is the way the child ended plus
// the message it printed.
#include "smoke.h"

#include <dolphin/base/PPCArch.h>
#include <dolphin/os.h>

#include "cos_sdk/hooks.h"
#include "cos_sdk/sram.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

// Aurora's <dolphin/os.h> makes __OSBusClock a low-memory read; cos_sdk defines the variable the
// decomp's headers declare (OSMisc.cpp).
#undef __OSBusClock
extern "C" u32 __OSBusClock;

namespace {

struct ChildResult {
    bool exited = false;   // ended through exit/_Exit
    int exitCode = -1;     // when exited
    int signal = 0;        // when killed by a signal
    std::string stderrText;
};

// Runs `fn` in a forked child with its stderr captured; the child ends with exit code 0 if `fn`
// returns. A child that hangs is killed after 10 seconds (SIGALRM).
ChildResult RunChild(void (*fn)()) {
    ChildResult result;
    int fds[2];
    if (pipe(fds) != 0) {
        return result;
    }
    std::fflush(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        alarm(10);
        fn();
        std::fflush(nullptr);
        _exit(0);
    }
    close(fds[1]);
    char buf[512];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) {
        result.stderrText.append(buf, static_cast<std::size_t>(n));
    }
    close(fds[0]);
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) != pid) {
        return result;
    }
    if (WIFEXITED(status)) {
        result.exited = true;
        result.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.signal = WTERMSIG(status);
    }
    return result;
}

bool Aborted(const ChildResult& r, const char* text) {
    if (r.signal != SIGABRT || r.stderrText.find(text) == std::string::npos) {
        std::fprintf(stderr, "child: signal %d, exit %d, stderr:\n%s\n", r.signal, r.exitCode,
                     r.stderrText.c_str());
        return false;
    }
    return true;
}

bool ExitedZero(const ChildResult& r, const char* text) {
    if (!r.exited || r.exitCode != 0 || r.stderrText.find(text) == std::string::npos) {
        std::fprintf(stderr, "child: signal %d, exit %d, stderr:\n%s\n", r.signal, r.exitCode,
                     r.stderrText.c_str());
        return false;
    }
    return true;
}

// ---- reset ------------------------------------------------------------------------------------

constexpr u32 kStackSize = 16 * 1024;
alignas(32) u8 sResetStack[kStackSize];
OSThread sResetThread;

char sResetLog[64];
int sResetLogLen = 0;
bool sReadyOnce = false;

void LogReset(char c, BOOL final) {
    if (sResetLogLen + 2 < static_cast<int>(sizeof(sResetLog))) {
        sResetLog[sResetLogLen++] = c;
        sResetLog[sResetLogLen++] = final ? 'F' : '-';
    }
}

BOOL ResetA(BOOL final) { // priority 10, registered first; not ready on its first call
    LogReset('a', final);
    if (!final && !sReadyOnce) {
        sReadyOnce = true;
        return FALSE;
    }
    return TRUE;
}
BOOL ResetB(BOOL final) { // priority 5
    LogReset('b', final);
    return TRUE;
}
BOOL ResetC(BOOL final) { // priority 10, after a
    LogReset('c', final);
    return TRUE;
}
BOOL ResetD(BOOL final) { // unregistered before the reset
    LogReset('d', final);
    return TRUE;
}

OSResetFunctionInfo sInfoA = {ResetA, 10, nullptr, nullptr};
OSResetFunctionInfo sInfoB = {ResetB, 5, nullptr, nullptr};
OSResetFunctionInfo sInfoC = {ResetC, 10, nullptr, nullptr};
OSResetFunctionInfo sInfoD = {ResetD, 1, nullptr, nullptr};

int sHookReset = -1;
u32 sHookCode = 0;
BOOL sHookForceMenu = FALSE;
BOOL sHookInterruptsEnabled = FALSE;

void ResetHook(int reset, u32 resetCode, BOOL forceMenu) {
    sHookReset = reset;
    sHookCode = resetCode;
    sHookForceMenu = forceMenu;
    // Interrupts are back at the caller's level (enabled).
    sHookInterruptsEnabled = (PPCMfmsr() & 0x8000) != 0;
    // As game glue that restarts the game on another thread: end this one.
    OSExitThread(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x5E7)));
}

u8 sSaveRegion[64];

void* ResetThreadMain(void*) {
    OSResetSystem(OS_RESET_HOTRESET, 0x1234, TRUE);
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(0xBAD)); // never reached
}

bool TestReset() {
    OSRegisterResetFunction(&sInfoA);
    OSRegisterResetFunction(&sInfoD);
    OSRegisterResetFunction(&sInfoB);
    OSRegisterResetFunction(&sInfoC);
    OSUnregisterResetFunction(&sInfoD);
    OSSetSaveRegion(sSaveRegion, sSaveRegion + sizeof(sSaveRegion));
    void* start = nullptr;
    void* end = nullptr;
    OSGetSaveRegion(&start, &end);
    COS_SMOKE_CHECK(start == sSaveRegion && end == sSaveRegion + sizeof(sSaveRegion));
    OSGetSavedRegion(&start, &end);
    COS_SMOKE_CHECK(start == nullptr && end == nullptr);
    COS_SMOKE_CHECK(OSGetResetCode() == 0);

    COS_SMOKE_CHECK(COSSdkSetResetHook(ResetHook) == nullptr);
    COS_SMOKE_CHECK(OSCreateThread(&sResetThread, ResetThreadMain, nullptr,
                                   sResetStack + sizeof(sResetStack), kStackSize, 16, 0));
    OSResumeThread(&sResetThread);
    void* ret = nullptr;
    COS_SMOKE_CHECK(OSJoinThread(&sResetThread, &ret));
    COS_SMOKE_CHECK(reinterpret_cast<std::uintptr_t>(ret) == 0x5E7);
    COS_SMOKE_CHECK(COSSdkSetResetHook(nullptr) == ResetHook);

    // Two non-final passes (a was not ready the first time), then the final one, in priority
    // order (b, then a and c in registration order); d was unregistered.
    sResetLog[sResetLogLen] = '\0';
    COS_SMOKE_CHECK(std::strcmp(sResetLog, "b-a-c-b-a-c-bFaFcF") == 0);
    COS_SMOKE_CHECK(sHookReset == OS_RESET_HOTRESET && sHookCode == 0x1234 && sHookForceMenu);
    COS_SMOKE_CHECK(sHookInterruptsEnabled);
    COS_SMOKE_CHECK(OSGetResetCode() == 0x1234);
    OSGetSavedRegion(&start, &end);
    COS_SMOKE_CHECK(start == sSaveRegion && end == sSaveRegion + sizeof(sSaveRegion));
    OSSram* sram = __OSLockSram();
    COS_SMOKE_CHECK(sram != nullptr);
    const bool forceMenuFlag = (sram->flags & 0x40) != 0;
    __OSUnlockSram(FALSE);
    COS_SMOKE_CHECK(forceMenuFlag);

    OSUnregisterResetFunction(&sInfoA);
    OSUnregisterResetFunction(&sInfoB);
    OSUnregisterResetFunction(&sInfoC);
    COSSdkSetResetCode(0);
    COS_SMOKE_CHECK(OSGetResetCode() == 0);
    COS_SMOKE_CHECK(!OSGetResetSwitchState() && !OSGetResetButtonState());

    // Without a hook OSResetSystem ends the process (exit code 0), as it never returns.
    COS_SMOKE_CHECK(ExitedZero(RunChild([] {
                                   OSResetSystem(OS_RESET_SHUTDOWN, 0, FALSE);
                                   std::fputs("OSResetSystem returned\n", stderr);
                                   std::abort();
                               }),
                               "no reset hook is set"));
    return true;
}

// ---- SRAM and RTC -----------------------------------------------------------------------------

bool TestSram() {
    COS_SMOKE_CHECK(__OSCheckSram());
    COS_SMOKE_CHECK(OSGetSoundMode() == OS_SOUND_MODE_STEREO);
    COS_SMOKE_CHECK(OSGetLanguage() == OS_LANGUAGE_ENGLISH);
    COS_SMOKE_CHECK(OSGetProgressiveMode() == OS_PROGRESSIVE_MODE_OFF);
    COS_SMOKE_CHECK(OSGetEuRgb60Mode() == OS_EURGB60_OFF);
    COS_SMOKE_CHECK(OSGetVideoMode() == OS_VIDEO_MODE_NTSC);

    OSSetSoundMode(OS_SOUND_MODE_MONO);
    COS_SMOKE_CHECK(OSGetSoundMode() == OS_SOUND_MODE_MONO);
    OSSetSoundMode(OS_SOUND_MODE_STEREO);
    COS_SMOKE_CHECK(OSGetSoundMode() == OS_SOUND_MODE_STEREO);
    OSSetProgressiveMode(1);
    COS_SMOKE_CHECK(OSGetProgressiveMode() == 1);
    COS_SMOKE_CHECK(OSGetSoundMode() == OS_SOUND_MODE_STEREO); // other bits are kept
    OSSetProgressiveMode(0);
    COS_SMOKE_CHECK(OSGetProgressiveMode() == 0);
    OSSetEuRgb60Mode(1);
    COS_SMOKE_CHECK(OSGetEuRgb60Mode() == 1);
    OSSetEuRgb60Mode(0);
    OSSetLanguage(OS_LANGUAGE_FRENCH);
    COS_SMOKE_CHECK(OSGetLanguage() == OS_LANGUAGE_FRENCH);
    OSSetLanguage(OS_LANGUAGE_ENGLISH);
    OSSetWirelessID(2, 0xBEEF);
    COS_SMOKE_CHECK(OSGetWirelessID(2) == 0xBEEF && OSGetWirelessID(1) == 0);
    OSSetGbsMode(3);
    COS_SMOKE_CHECK(OSGetGbsMode() == 3);
    COS_SMOKE_CHECK(__OSCheckSram()); // commits recompute the checksums
    COS_SMOKE_CHECK(__OSSyncSram());

    // The lock is exclusive: a second lock (even of the other half) fails until the unlock, and
    // interrupts stay disabled meanwhile.
    OSSram* sram = __OSLockSram();
    COS_SMOKE_CHECK(sram != nullptr);
    COS_SMOKE_CHECK((PPCMfmsr() & 0x8000) == 0);
    COS_SMOKE_CHECK(__OSLockSramEx() == nullptr);
    COS_SMOKE_CHECK((PPCMfmsr() & 0x8000) == 0);
    __OSUnlockSram(FALSE);
    COS_SMOKE_CHECK((PPCMfmsr() & 0x8000) != 0);

    // RTC: seconds since 2000 from the wall clock; __OSSetRTC moves it by an offset.
    u32 rtc = 0;
    COS_SMOKE_CHECK(__OSGetRTC(&rtc));
    const auto now = static_cast<u32>(OSGetSystemTime() / OS_TIMER_CLOCK);
    COS_SMOKE_CHECK(rtc + 2 >= now && rtc <= now + 2);
    COS_SMOKE_CHECK(rtc > 24u * 365 * 24 * 3600); // after 2024
    COS_SMOKE_CHECK(__OSSetRTC(now + 1000));
    COS_SMOKE_CHECK(__OSGetRTC(&rtc));
    COS_SMOKE_CHECK(rtc + 2 >= now + 1000 && rtc <= now + 1002);
    COS_SMOKE_CHECK(__OSSetRTC(static_cast<u32>(OSGetSystemTime() / OS_TIMER_CLOCK)));
    return true;
}

// ---- PPC registers and caches -----------------------------------------------------------------

bool TestPpc() {
    // MSR[EE] is the interrupt state.
    COS_SMOKE_CHECK((PPCMfmsr() & 0x8000) != 0);
    BOOL level = OSDisableInterrupts();
    COS_SMOKE_CHECK(level && (PPCMfmsr() & 0x8000) == 0);
    OSRestoreInterrupts(level);
    COS_SMOKE_CHECK((PPCMfmsr() & 0x8000) != 0);
    PPCMtmsr(PPCMfmsr() & ~0x8000u);
    COS_SMOKE_CHECK(OSEnableInterrupts() == FALSE); // PPCMtmsr had disabled them
    // JUTException::run: clear FE0/FE1, keep the rest.
    const u32 msr = PPCMfmsr();
    PPCMtmsr(PPCMfmsr() & ~0x0900u);
    COS_SMOKE_CHECK(PPCMfmsr() == (msr & ~0x0900u));
    COS_SMOKE_CHECK(PPCAndCMsr(0x8000) == msr && (PPCMfmsr() & 0x8000) == 0);
    COS_SMOKE_CHECK(PPCOrMsr(0x8000) == (msr & ~0x8000u) && (PPCMfmsr() & 0x8000) != 0);

    COS_SMOKE_CHECK((PPCMfhid2() & 0x10000000) != 0); // locked cache usable (d_a_movie_player)
    COS_SMOKE_CHECK(PPCMfpvr() == 0x00083214);
    const u32 hid0 = PPCMfhid0();
    PPCDisableSpeculation();
    COS_SMOKE_CHECK((PPCMfhid0() & 0x200) != 0);
    PPCEnableSpeculation();
    COS_SMOKE_CHECK(PPCMfhid0() == (hid0 & ~0x200u));
    PPCMtwpar(0x0C008000);
    COS_SMOKE_CHECK(PPCMfwpar() == 0x0C008000);

    // The decrementer counts down at the timer clock.
    PPCMtdec(0x7FFFFFFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const u32 dec = PPCMfdec();
    COS_SMOKE_CHECK(dec < 0x7FFFFFFF - OSMillisecondsToTicks(4));
    COS_SMOKE_CHECK(dec > 0x7FFFFFFF - OSMillisecondsToTicks(5000));

    PPCMtfpscr(0);
    PPCSetFpNonIEEEMode();
    COS_SMOKE_CHECK((PPCMffpscr() & FPSCR_NI) != 0);
    PPCSetFpIEEEMode();
    COS_SMOKE_CHECK(PPCMffpscr() == 0);
    PPCSync();
    PPCEieio();

    // dcbz clears the 32-byte block; LCStoreData (Aurora) copies.
    alignas(32) u8 block[64];
    std::memset(block, 0xFF, sizeof(block));
    DCBlockZero(block + 40);
    COS_SMOKE_CHECK(block[31] == 0xFF && block[32] == 0 && block[63] == 0);
    auto* lc = static_cast<u8*>(LCGetBase());
    for (int i = 0; i < 0x2000; i++) {
        lc[i] = static_cast<u8>(i * 7);
    }
    static u8 out[0x2000];
    COS_SMOKE_CHECK(LCStoreData(out, lc, sizeof(out)) == 2);
    COS_SMOKE_CHECK(std::memcmp(out, lc, sizeof(out)) == 0);
    LCAlloc(lc, 0x2000);
    LCQueueWait(0);
    DCFlushRange(out, sizeof(out));
    ICSync();
    return true;
}

// ---- console, errors, stopwatch, font ---------------------------------------------------------

void ErrorHandler(OSError, OSContext*, ...) {}

bool TestMisc() {
    COS_SMOKE_CHECK(OSGetConsoleType() == OS_CONSOLE_RETAIL1);
    COS_SMOKE_CHECK(OSGetConsoleSimulatedMemSize() == 0x01800000);
    COS_SMOKE_CHECK(__OSBusClock == 162000000);
    COS_SMOKE_CHECK(__OSFpscrEnableBits == 0xF8);

    COS_SMOKE_CHECK(OSSetErrorHandler(OS_ERROR_DSI, ErrorHandler) == nullptr);
    COS_SMOKE_CHECK(COSSdkGetErrorHandler(OS_ERROR_DSI) == ErrorHandler);
    COS_SMOKE_CHECK(OSSetErrorHandler(OS_ERROR_DSI, nullptr) == ErrorHandler);
    COS_SMOKE_CHECK(COSSdkGetErrorHandler(OS_ERROR_DSI) == nullptr);
    OSProtectRange(OS_PROTECT_CHAN0, nullptr, 0, OS_PROTECT_CONTROL_RDWR);

    static char name[] = "smoke";
    OSStopwatch sw;
    OSInitStopwatch(&sw, name);
    COS_SMOKE_CHECK(OSCheckStopwatch(&sw) == 0);
    OSStartStopwatch(&sw);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    COS_SMOKE_CHECK(OSCheckStopwatch(&sw) >= OSMillisecondsToTicks(9));
    OSStopStopwatch(&sw);
    COS_SMOKE_CHECK(sw.hits == 1 && sw.total >= OSMillisecondsToTicks(9));
    COS_SMOKE_CHECK(sw.min == sw.total && sw.max == sw.total);
    OSStopStopwatch(&sw); // not running: no change
    COS_SMOKE_CHECK(sw.hits == 1);
    OSResetStopwatch(&sw);
    COS_SMOKE_CHECK(sw.hits == 0 && sw.total == 0 && sw.name == name);

    COS_SMOKE_CHECK(OSGetFontEncode() == OS_FONT_ENCODE_ANSI);
    COS_SMOKE_CHECK(OSSetFontEncode(OS_FONT_ENCODE_SJIS) == OS_FONT_ENCODE_ANSI);
    COS_SMOKE_CHECK(OSGetFontEncode() == OS_FONT_ENCODE_SJIS);
    OSSetFontEncode(OS_FONT_ENCODE_ANSI);
    COS_SMOKE_CHECK(OSInitFont(nullptr) == FALSE);
    return true;
}

// ---- report, panic, fatal and other ends ------------------------------------------------------

bool TestEnds() {
    ChildResult r = RunChild([] { OSReport("report %d %s\n", 7, "ok"); });
    COS_SMOKE_CHECK(ExitedZero(r, "report 7 ok\n"));
    r = RunChild([] {
        OSStopwatch sw;
        static char name[] = "dump";
        OSInitStopwatch(&sw, name);
        OSDumpStopwatch(&sw);
    });
    COS_SMOKE_CHECK(ExitedZero(r, "Stopwatch [dump]"));

    COS_SMOKE_CHECK(Aborted(RunChild([] { OSFatal(GXColor{255, 255, 255, 255}, GXColor{0, 0, 0, 255}, "boom"); }),
                            "OSFatal: boom"));
    COS_SMOKE_CHECK(Aborted(RunChild([] { OSPanic("file.c", 12, "bad %d", 3); }),
                            "PANIC: bad 3 in \"file.c\" on line 12."));
    COS_SMOKE_CHECK(Aborted(RunChild([] { PPCHalt(); }), "PPCHalt"));
    COS_SMOKE_CHECK(Aborted(RunChild([] { PPCMtdmaL(0x2); }), "locked-cache DMA"));
    COS_SMOKE_CHECK(Aborted(RunChild([] {
                                s32 width = 0;
                                OSGetFontWidth("A", &width);
                            }),
                            "no IPL font"));
    COS_SMOKE_CHECK(Aborted(RunChild([] { OSSetErrorHandler(17, nullptr); }), "out of range"));
    return true;
}

} // namespace

COS_SMOKE_TEST(misc) {
    return TestMisc() && TestSram() && TestPpc() && TestReset() && TestEnds();
}
