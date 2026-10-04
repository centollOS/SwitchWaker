// Watchdog of the run harness (docs/NATIVE_PORT_PHASE4_6.md, step 6.0): a detached thread that
// checks every 100 ms
// - COS_TIMEOUT_S: the run has lasted this long -> exit 10 (frames may still be advancing; the
//   report says whether they moved during the last second);
// - COS_STALL_S: the game frame counter (pc_frame_tick) has not moved for this long (nor
//   watchdogPulse been called: the shader loading screen), counted from start-up before the first
//   frame -> a backtrace of every other thread (to stderr and
//   <COS_RUN_DIR>/stall.txt), then exit 11. `sample` would need developer mode (decision H9), so
//   the threads are suspended and walked in-process instead.
#include "pc_internal.h"

#include <atomic>
#include <pthread.h>
#include <unistd.h>

namespace pc {

namespace {

uint64_t sWatchStartNs = 0;
std::atomic<unsigned int> sPulses{0}; // watchdogPulse

void* watchdogMain(void*) {
#if defined(__APPLE__)
    pthread_setname_np("decomp-watchdog");
#elif defined(__linux__)
    pthread_setname_np(pthread_self(), "cos-watchdog"); // at most 15 characters on Linux
#endif
    const uint64_t timeoutNs = (uint64_t)(gConfig.timeoutS * 1e9);
    const uint64_t stallNs = (uint64_t)(gConfig.stallS * 1e9);
    unsigned int lastFrames = pc_frame_count();
    unsigned int lastPulses = sPulses.load(std::memory_order_relaxed);
    uint64_t lastChange = sWatchStartNs;
    unsigned int framesOneSecondAgo = lastFrames;
    uint64_t secondMark = sWatchStartNs;

    for (;;) {
        usleep(100 * 1000);
        const uint64_t now = monotonicNs();
        const unsigned int frames = pc_frame_count();
        const unsigned int pulses = sPulses.load(std::memory_order_relaxed);
        if (frames != lastFrames || pulses != lastPulses) {
            lastFrames = frames;
            lastPulses = pulses;
            lastChange = now;
        }
        unsigned int framesLastSecond = frames - framesOneSecondAgo;
        if (now - secondMark >= 1000000000ull) {
            framesOneSecondAgo = frames;
            secondMark = now;
        }

        if (timeoutNs != 0 && now - sWatchStartNs >= timeoutNs) {
            writef(STDERR_FILENO, "[cos] TIMEOUT after %gs (frames %s)\n[cos] state: ",
                   gConfig.timeoutS, framesLastSecond != 0 ? "advancing" : "not advancing");
            writeState(STDERR_FILENO);
            pc_exit(PC_EXIT_TIMEOUT);
        }
        if (stallNs != 0 && now - lastChange >= stallNs) {
            writef(STDERR_FILENO, "[cos] STALL: frame counter frozen at %u for %gs\n[cos] state: ",
                   frames, gConfig.stallS);
            writeState(STDERR_FILENO);
            int fd = openRunFile("stall.txt");
            if (fd >= 0) {
                writef(fd, "[cos] STALL: frame counter frozen at %u for %gs\n[cos] state: ", frames,
                       gConfig.stallS);
                writeState(fd);
                dumpAllThreads(fd);
                close(fd);
                writef(STDERR_FILENO, "[cos] thread backtraces in %s/stall.txt\n", gConfig.runDir);
            } else {
                dumpAllThreads(STDERR_FILENO);
            }
            pc_exit(PC_EXIT_STALL);
        }
    }
    return nullptr;
}

} // namespace

void watchdogPulse() {
    sPulses.fetch_add(1, std::memory_order_relaxed);
}

void startWatchdog() {
    if (gConfig.timeoutS <= 0 && gConfig.stallS <= 0) {
        return;
    }
    sWatchStartNs = monotonicNs();
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attr, watchdogMain, nullptr) != 0) {
        writef(STDERR_FILENO, "[cos] watchdog: pthread_create failed\n");
    }
    pthread_attr_destroy(&attr);
}

} // namespace pc
