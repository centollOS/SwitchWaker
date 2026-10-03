// The frame loop and its pacing (docs/NATIVE_PORT_PHASE4_6.md, step 6.2).
//
// - pc_frame_begin / pc_frame_end wrap each iteration of main01's loop (m_Do_main.cpp, TARGET_PC):
//   Aurora's event pump (aurora_update; a quit request ends the process), aurora_begin_frame
//   before the game reads the pad, runs and draws (fapGm_Execute encodes GX and, inside
//   JFWDisplay::beginRender, makes the VI retraces), aurora_end_frame after it. Then the frame
//   counts (pc_frame_tick: stall watchdog, COS_FRAMES) and milestone M4 frame-loop is checked.
// - pc_frame_pace is the wait of JFWDisplay's waitForTick (JFWDisplay.cpp, TARGET_PC): it sleeps
//   until the period the game asked for has passed since the previous wait, with Dusklight's
//   limiter (mach_wait_until for all but the last 2 ms, then a spin). COS_UNCAPPED skips it.
//
// From Dusklight (CC0, ref/dusklight/src/dusk/time.h: class Limiter, its macOS NanoSleep;
// ref/dusklight/src/m_Do/m_Do_main.cpp main01: the aurora_update / aurora_begin_frame /
// aurora_end_frame order around the game's frame). Changed: the clock is the harness's monotonic
// clock instead of SDL_GetTicksNS; only the macOS sleep is kept (other hosts use nanosleep plus
// the same spin); no frame-usage statistics, interpolation, turbo mode or settings; a refused
// aurora_begin_frame waits and retries instead of skipping the game frame.
#include "pc_internal.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <dolphin/vi.h>

#include <array>
#include <atomic>
#include <ctime>
#include <numeric>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

namespace pc {

namespace {

// M4 (docs/NATIVE_PORT_PHASE4_6.md): 120 game frames, the VI retrace count went up, and Aurora
// counted draw calls.
constexpr unsigned int kFrameLoopFrames = 120;

class Limiter {
public:
    using duration_t = uint64_t;

    void Reset() { m_oldTime = monotonicNs(); }

    duration_t Sleep(duration_t targetFrameTime) {
        if (targetFrameTime == 0) {
            return 0;
        }
        const uint64_t start = monotonicNs();
        duration_t adjustedSleepTime = SleepTime(targetFrameTime);
        if (adjustedSleepTime > 0) {
            NanoSleep(adjustedSleepTime);
            const duration_t elapsed = monotonicNs() - start;
            const duration_t overslept = elapsed > adjustedSleepTime ? elapsed - adjustedSleepTime : 0;
            if (overslept < targetFrameTime) {
                m_overheadTimes[m_overheadTimeIdx] = overslept;
                m_overheadTimeIdx = (m_overheadTimeIdx + 1) % m_overheadTimes.size();
            }
        }
        Reset();
        return adjustedSleepTime;
    }

private:
    uint64_t m_oldTime = 0;
    std::array<duration_t, 4> m_overheadTimes{};
    size_t m_overheadTimeIdx = 0;
    duration_t m_overhead = 0;

    duration_t SleepTime(duration_t targetFrameTime) {
        const duration_t elapsed = monotonicNs() - m_oldTime;
        const duration_t sleepTime = elapsed < targetFrameTime ? targetFrameTime - elapsed : 0;
        m_overhead = std::accumulate(m_overheadTimes.begin(), m_overheadTimes.end(), duration_t{0}) /
                     m_overheadTimes.size();
        return sleepTime > m_overhead ? sleepTime - m_overhead : 0;
    }

#if defined(__APPLE__)
    // Hybrid: sleep in the kernel until 2 ms before the target, then spin on the clock.
    void NanoSleep(const duration_t duration) {
        const uint64_t startMach = mach_absolute_time();
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        const uint64_t totalTicks = (duration * timebase.denom) / timebase.numer;
        const uint64_t targetMach = startMach + totalTicks;
        const uint64_t bufferTicks = (2'000'000ull * timebase.denom) / timebase.numer;
        if (totalTicks > bufferTicks) {
            mach_wait_until(targetMach - bufferTicks);
        }
        while (mach_absolute_time() < targetMach) {
#if defined(__aarch64__) || defined(__arm__)
            asm volatile("yield" ::: "memory");
#endif
        }
    }
#else
    void NanoSleep(const duration_t duration) {
        const uint64_t target = monotonicNs() + duration;
        if (duration > 2'000'000ull) {
            const uint64_t ns = duration - 2'000'000ull;
            struct timespec ts = {(time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull)};
            nanosleep(&ts, nullptr);
        }
        while (monotonicNs() < target) {
        }
    }
#endif
};

Limiter sLimiter;          // game thread only (waitForTick)
bool sLimiterStarted = false;
uint64_t sRequestedNs = 0; // sum of the periods the game asked pc_frame_pace for
// The first wait returned, and the period it waited for: the steady-state figures leave out the
// first frame, which carries the boot's loading (scene creation, the first archives).
uint64_t sFirstPaceEndNs = 0;
uint64_t sFirstPeriodNs = 0;
uint64_t sLoopStartNs = 0; // first pc_frame_begin
uint32_t sLoopStartRetrace = 0;
uint32_t sMaxDrawCalls = 0;     // largest drawCallCount Aurora reported after a frame
// COS_TRACE=frame: per-frame times (ns): pc_frame_begin (events and aurora_begin_frame), from there
// to the pace wait, the wait itself, from the wait to pc_frame_end, and aurora_end_frame.
bool sTraceFrame = false;
uint64_t sFrameStartNs = 0;
uint64_t sBeginDoneNs = 0; // aurora_begin_frame returned
uint64_t sPaceStartNs = 0;
uint64_t sPaceEndNs = 0;
bool sFrameLoopLogged = false;
bool sFrameLoopWarned = false;

// Drains Aurora's events. A quit request (window closed) ends the process: exit 0 for a plain
// run, 1 when a milestone or COS_FRAMES was still expected.
void pumpEvents() {
    const AuroraEvent* event = aurora_update();
    for (; event != nullptr && event->type != AURORA_NONE; event++) {
        if (event->type == AURORA_EXIT) {
            const bool expected = gConfig.milestone != nullptr || gConfig.frames != 0;
            writef(STDERR_FILENO, "[cos] quit requested (window closed) at frame %u%s\n",
                   pc_frame_count(), expected ? " before the expected end of the run" : "");
            pc_exit(expected ? PC_EXIT_CHECK_FAILED : PC_EXIT_REACHED);
        }
    }
}

} // namespace

void writePacing(int fd) {
    if (sLoopStartNs == 0) {
        return;
    }
    const uint64_t now = monotonicNs();
    const uint64_t wallNs = now - sLoopStartNs;
    // From the end of the first wait to now: the waits of the other frames, against what they
    // asked for (the time after the last wait, its frame's end, is a fraction of a millisecond).
    const uint64_t steadyWallNs = sFirstPaceEndNs != 0 ? now - sFirstPaceEndNs : 0;
    const uint64_t steadyRequestedNs = sRequestedNs - sFirstPeriodNs;
    writef(fd, "[cos] pacing: frames=%u wall=%.1f ms requested=%.1f ms; after frame 1: wall=%.1f ms "
               "requested=%.1f ms ratio=%.4f; uncapped=%d retraces=%u\n",
           pc_frame_count(), wallNs / 1e6, sRequestedNs / 1e6, steadyWallNs / 1e6,
           steadyRequestedNs / 1e6,
           steadyRequestedNs != 0 ? (double)steadyWallNs / (double)steadyRequestedNs : 0.0,
           gConfig.uncapped ? 1 : 0, (unsigned int)(VIGetRetraceCount() - sLoopStartRetrace));
}

} // namespace pc

using namespace pc;

extern "C" {

void pc_frame_pace(unsigned long long periodNs) {
    sRequestedNs += periodNs;
    sPaceStartNs = monotonicNs();
    if (!gConfig.uncapped) {
        if (!sLimiterStarted) {
            // The first wait measures from the start of the loop, not from process start.
            sLimiterStarted = true;
            sLimiter.Reset();
        }
        sLimiter.Sleep(periodNs);
    }
    sPaceEndNs = monotonicNs();
    if (sFirstPaceEndNs == 0) {
        sFirstPaceEndNs = sPaceEndNs;
        sFirstPeriodNs = periodNs;
    }
}

void pc_frame_begin(void) {
    if (sLoopStartNs == 0) {
        sLoopStartNs = monotonicNs();
        sLoopStartRetrace = VIGetRetraceCount();
        writef(STDERR_FILENO, "[cos] frame loop: start, %s\n",
               gConfig.uncapped ? "uncapped (COS_UNCAPPED)" : "paced by JFWDisplay");
        sTraceFrame = pc_trace_enabled("frame") != 0;
    }
    sFrameStartNs = monotonicNs();
    sPaceStartNs = sPaceEndNs = 0;
    pumpEvents();
    // Refused while the window cannot present (minimised, no surface yet): the console would not
    // run a frame without a display either. The stall watchdog reports a refusal that lasts.
    while (!aurora_begin_frame()) {
        usleep(10 * 1000);
        pumpEvents();
    }
    sBeginDoneNs = monotonicNs();
}

void pc_frame_end(void) {
    const uint64_t endFrameStartNs = monotonicNs();
    aurora_end_frame();
    if (sTraceFrame) {
        const uint64_t now = monotonicNs();
        const uint64_t paceStart = sPaceStartNs != 0 ? sPaceStartNs : endFrameStartNs;
        const uint64_t paceEnd = sPaceEndNs != 0 ? sPaceEndNs : endFrameStartNs;
        writef(STDERR_FILENO, "[cos] trace frame %u: begin %.2f ms, before wait %.2f ms, wait %.2f ms, "
                              "after wait %.2f ms, aurora_end_frame %.2f ms, retrace %u\n",
               pc_frame_count() + 1, (sBeginDoneNs - sFrameStartNs) / 1e6,
               (paceStart - sBeginDoneNs) / 1e6, (paceEnd - paceStart) / 1e6,
               (endFrameStartNs - paceEnd) / 1e6, (now - endFrameStartNs) / 1e6,
               (unsigned int)VIGetRetraceCount());
    }

    // Aurora publishes a frame's counters when its render worker has taken the frame, so look at
    // them every frame and keep the largest.
    const AuroraStats* stats = aurora_get_stats();
    if (stats != nullptr && stats->drawCallCount > sMaxDrawCalls) {
        sMaxDrawCalls = stats->drawCallCount;
    }

    pc_frame_tick();

    const unsigned int frames = pc_frame_count();
    if (!sFrameLoopLogged && frames >= kFrameLoopFrames) {
        const uint32_t retraces = VIGetRetraceCount() - sLoopStartRetrace;
        if (retraces > 0 && sMaxDrawCalls > 0) {
            sFrameLoopLogged = true;
            writef(STDERR_FILENO, "[cos] frame-loop: %u frames, %u retraces, up to %u draw calls "
                                  "in a frame\n",
                   frames, (unsigned int)retraces, (unsigned int)sMaxDrawCalls);
            writePacing(STDERR_FILENO);
            pc_milestone("frame-loop");
        } else if (!sFrameLoopWarned) {
            sFrameLoopWarned = true;
            writef(STDERR_FILENO, "[cos] frame-loop: not yet at frame %u: %u retraces, up to %u "
                                  "draw calls in a frame\n",
                   frames, (unsigned int)retraces, (unsigned int)sMaxDrawCalls);
        }
    }
}

} // extern "C"
