// The frame loop and its pacing (docs/NATIVE_PORT_PHASE4_6.md, step 6.2).
//
// - pc_frame_begin / pc_frame_end wrap each iteration of main01's loop (m_Do_main.cpp, TARGET_PC):
//   Aurora's event pump (aurora_update; a quit request ends the process), aurora_begin_frame
//   before the game reads the pad, runs and draws (fapGm_Execute encodes GX and, inside
//   JFWDisplay::beginRender, makes the VI retraces), aurora_end_frame after it. Then the frame
//   counts (pc_frame_tick: stall watchdog, COS_FRAMES) and milestones M4 frame-loop and M5
//   logo-scene are checked.
// - COS_PERF_EVERY=<n> (phase 7, for the Switch at its stock 1020 MHz): every n frames one line
//   with the game thread's busy time per frame (the frame minus the pace wait: average and
//   maximum, and the aurora_begin_frame/aurora_end_frame parts), the average wait, the frame rate
//   and the VI retrace rate (60 a second is full speed), then the phase split and the CPU time
//   below as averages. Off by default (the Switch build turns it on).
// - COS_PERF=<file> (step 6.7): one CSV row per game frame (columns at kPerfCsvHeader): the wall
//   time from pc_frame_begin to the end of aurora_end_frame, the busy part (wall minus the pace
//   wait), the game thread's CPU time (CLOCK_THREAD_CPUTIME_ID, which leaves out time blocked;
//   the CPU the pace wait spins away is left out too) and the split: pumpEvents with
//   aurora_begin_frame, mDoCPd_Read, mDoAud_Execute, the fapGm_Execute logic (fapGm_Execute minus
//   the painter), the mDoGph_Painter GX encode (pc_perf_begin/pc_perf_end brackets, minus the pace
//   wait inside them), aurora_end_frame, and the rest of the busy time. Times in ms. Rows are
//   buffered and written out when the buffer fills and at exit (pc_exit). The same numbers feed
//   the COS_PERF_EVERY lines, so the Mac's CSV and the Switch's log compare directly.
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
#include "pc/pc_gpu_opts.h"
#include "pc/pc_dynres.h"
#include "pc/pc_hd_textures.h"

#include "JSystem/JAudio/osdsp_task.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "m_Do/m_Do_audio.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <dolphin/gx/GXTexture.h>
#include <dolphin/vi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <numeric>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif
#if defined(__SWITCH__)
#include "cos_switch.h"

// switch/dawn/patches/dawn-switch-gl-gpu-groups.patch: GPU time per group of draws (the frame's
// last GX debug marker, COS_GPU_GROUPS, or the render pass's label), running totals.
struct DawnSwitchGpuGroup {
    char name[64];
    uint64_t ns;
    uint64_t segments;
    uint64_t draws;
};
extern "C" size_t dawn_switch_gl_gpu_groups(DawnSwitchGpuGroup* out, size_t max);
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
uint64_t sEventsDoneNs = 0; // pumpEvents returned (aurora_begin_frame starts)
uint64_t sBeginDoneNs = 0; // aurora_begin_frame returned
uint64_t sPaceStartNs = 0;
uint64_t sPaceEndNs = 0;
bool sFrameLoopLogged = false;
bool sFrameLoopWarned = false;
// M5 (step 4.5): the LOGO scene was created (pc_logo_scene_created, game thread) and the texture
// bytes Aurora reported uploaded before and after that.
bool sLogoCreated = false;
bool sLogoLogged = false;
unsigned int sLogoFrame = 0;
uint64_t sUploadBeforeLogo = 0;
uint64_t sUploadSinceLogo = 0;
// M6 (step 4.8): object archives with entries left unconverted, and the synced report.
int sLogoResGaps = 0;
bool sLogoResSynced = false;
bool sLogoResLogged = false;
// Step 5.A: mDoAud_Create has finished (COS_AUDIO=on).
bool sAudioLogged = false;

// Step 6.7: COS_PERF, COS_PERF_EVERY or COS_HITCH_MS is set (perfOpen decides it once).
bool sPerfOn = false;
// Pace waits of the current frame: their sum (ns) and the game thread CPU they used (the
// limiter's final spin).
uint64_t sFrameWaitNs = 0;
uint64_t sFrameWaitCpuNs = 0;
uint64_t sFrameStartCpuNs = 0;
// pc_perf_begin/pc_perf_end: per phase, the time of this frame (pace wait left out) and the open
// bracket's start.
struct PerfPhase {
    uint64_t startNs = 0;
    uint64_t startWaitNs = 0; // sFrameWaitNs at pc_perf_begin
    uint64_t ns = 0;
};
PerfPhase sPhase[PC_PERF_PHASES];

// The game thread's CPU time in ns, or UINT64_MAX where the clock is missing.
uint64_t threadCpuNs() {
#if defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    }
#endif
    return UINT64_MAX;
}

// One game frame's numbers (ns), as the CSV row and the COS_PERF_EVERY sums use them.
struct PerfFrame {
    uint64_t wallNs, busyNs, cpuNs, waitNs, beginNs, cpdNs, audNs, logicNs, painterNs, endFrameNs,
        otherNs;
    uint64_t eventsNs; // the pumpEvents part of beginNs (not in the CSV)
    bool cpuValid;
};

// What else happened in a frame, for the hitch lines and the Switch's perf lines: Aurora's
// pipeline and texture counters, and the harness's resource and scene traces.
struct FrameEvents {
    uint32_t createdPipelines = 0; // AuroraStats::createdPipelines (a running total)
    unsigned int resources = 0;    // traceResourceCount (a running total)
    int scene = -1;                // traceScene
};
FrameEvents sPrevEvents;          // at the previous frame's end
bool sPrevEventsValid = false;

#if defined(__SWITCH__)
// The Switch's graphics and disc counters (running totals) at the previous frame's end and at the
// start of the COS_PERF_EVERY window.
CosSwitchGfxStats sSwPrev{};
CosSwitchGfxStats sSwWindow{};
FrameEvents sSwWindowEvents;
uint64_t sSwWindowTexBytes = 0;
uint64_t sSwWindowEventsNs = 0;
uint64_t sSwWindowStartNs = 0;
unsigned int sSwFrames = 0;
bool sSwStarted = false;
// GPU time of each frame read back in the window (ns; dawn-switch-gl-gpu-timer.patch), for the
// window's max and p95.
std::vector<uint64_t> sSwGpuFrameNs;
#endif
// The last frame perfFrameEnd measured, for perfPlatformFrame (called after the perf line).
PerfFrame sLastPerfFrame{};
FrameEvents sLastEvents;

double msOf(uint64_t ns) {
    return ns / 1e6;
}

#if defined(__SWITCH__)
// The deferred GL work of Dawn's flushes other than CommandBuffer::Execute and the context release
// (buffer map/unmap, object creation, buffer and texture writes), as a running total.
uint64_t glOtherWorkNs(const CosSwitchGfxStats& s) {
    const uint64_t known = s.glExecuteNs + s.glReleaseNs;
    return s.glFlushNs > known ? s.glFlushNs - known : 0;
}

uint64_t subOrZero(uint64_t a, uint64_t b) {
    return a > b ? a - b : 0;
}

// The render passes' replay (dawn-switch-gl-replay-timers.patch), as a running total.
uint64_t glReplayNs(const CosSwitchGfxStats& s) {
    return s.glPipelineNs + s.glBindGroupNs + s.glImmediatesNs + s.glVertexStateNs + s.glDrawCallNs;
}

// What no timer of a whole render pass covers (dawn-switch-gl-pass-timers.patch): the command
// loop's own work and anything Mesa did inside calls not timed apart.
uint64_t glPassResidualNs(const CosSwitchGfxStats& s) {
    return subOrZero(s.glPassTotalNs, s.glPassLazyClearNs + s.glPassFramebufferNs +
                                          s.glPassDefaultStateNs + s.glPassClearNs + s.glPassEndNs +
                                          s.glPassDynamicStateNs + glReplayNs(s));
}

uint64_t glFirstPassResidualNs(const CosSwitchGfxStats& s) {
    return subOrZero(s.glFirstPassNs, s.glFirstPassLazyClearNs + s.glFirstPassFramebufferNs +
                                          s.glFirstPassDefaultStateNs + s.glFirstPassClearNs +
                                          s.glFirstPassEndNs + s.glFirstPassReplayNs);
}

// Execute minus its passes and copies: the other commands and the loop.
uint64_t glExecuteResidualNs(const CosSwitchGfxStats& s) {
    return subOrZero(s.glExecuteNs, s.glPassTotalNs + s.glBufCopyNs + s.glTexCopyNs);
}
#endif

// COS_HITCH_MS: one line for a frame whose busy time is above the threshold. `ev`/`prev`: the
// frame's end and the previous frame's end; `texBytes`: Aurora's last texture upload size.
void hitchLine(unsigned int frame, const PerfFrame& f, const FrameEvents& ev, const FrameEvents& prev,
               const AuroraStats* stats) {
    char res[160];
    traceLastResource(res, sizeof(res));
    const unsigned int resLoads = ev.resources - prev.resources;
    const bool sceneChanged = ev.scene != prev.scene;
    const uint64_t beginFrameNs = f.beginNs > f.eventsNs ? f.beginNs - f.eventsNs : 0;
    char platform[2048] = "";
#if defined(__SWITCH__)
    CosSwitchGfxStats now{};
    cos_switch_gfx_stats(&now);
    const CosSwitchGfxStats& p = sSwPrev;
    snprintf(platform, sizeof(platform),
             "; switch: slot wait %.1f, staging wait %.1f, queue-full wait %.1f, worker busy %.1f "
             "(encode %.1f, submit %.1f, present %.1f, events %.1f), gl fence wait %.1f, glFinish "
             "%.1f, pipeline compile %.1f ms (%llu), dvd %llu reads %.1f KiB %.1f ms; dawn gl: %llu "
             "draws, %llu tex binds, %llu texparams, execute %.1f, other work %.1f, release %.1f ms "
             "(first pass %.1f: fbo %.1f, clears %.1f, replay %.1f; other passes %.1f; buffer "
             "copies %.1f, first %.1f; texture copies %.1f; tex uploads %llu in %.1f ms, %llu "
             "compressed calls (%llu packed), %llu textures created in %.1f ms); gpu %.1f ms over "
             "%llu frames read back; "
             "cpu: game %.1f, render worker %.1f, audio %.1f, dvd %.1f, other %.1f ms",
             msOf(now.frameSlotWaitNs - p.frameSlotWaitNs), msOf(now.stagingWaitNs - p.stagingWaitNs),
             msOf(now.queueFullWaitNs - p.queueFullWaitNs), msOf(now.workerBusyNs - p.workerBusyNs),
             msOf(now.workerEncodeNs - p.workerEncodeNs), msOf(now.workerSubmitNs - p.workerSubmitNs),
             msOf(now.workerPresentNs - p.workerPresentNs), msOf(now.workerEventsNs - p.workerEventsNs),
             msOf(now.glWaitNs - p.glWaitNs), msOf(now.glFinishNs - p.glFinishNs),
             msOf(now.pipelineCompileNs - p.pipelineCompileNs),
             (unsigned long long)(now.pipelineCompiles - p.pipelineCompiles),
             (unsigned long long)(now.dvdReads - p.dvdReads), (now.dvdBytes - p.dvdBytes) / 1024.0,
             msOf(now.dvdNs - p.dvdNs), (unsigned long long)(now.glDraws - p.glDraws),
             (unsigned long long)(now.glTexBinds - p.glTexBinds),
             (unsigned long long)(now.glTexParams - p.glTexParams), msOf(now.glExecuteNs - p.glExecuteNs),
             msOf(glOtherWorkNs(now) - glOtherWorkNs(p)), msOf(now.glReleaseNs - p.glReleaseNs),
             msOf(now.glFirstPassNs - p.glFirstPassNs),
             msOf(now.glFirstPassFramebufferNs - p.glFirstPassFramebufferNs),
             msOf(now.glFirstPassClearNs - p.glFirstPassClearNs),
             msOf(now.glFirstPassReplayNs - p.glFirstPassReplayNs),
             msOf(subOrZero(now.glPassTotalNs - p.glPassTotalNs, now.glFirstPassNs - p.glFirstPassNs)),
             msOf(now.glBufCopyNs - p.glBufCopyNs), msOf(now.glFirstBufCopyNs - p.glFirstBufCopyNs),
             msOf(now.glTexCopyNs - p.glTexCopyNs), (unsigned long long)(now.glTexUploads - p.glTexUploads),
             msOf(now.glTexUploadNs - p.glTexUploadNs),
             (unsigned long long)(now.glCompressedUploadCalls - p.glCompressedUploadCalls),
             (unsigned long long)(now.glCompressedRepacks - p.glCompressedRepacks),
             (unsigned long long)(now.glTexCreates - p.glTexCreates), msOf(now.glTexCreateNs - p.glTexCreateNs),
             msOf(now.gpuTotalNs - p.gpuTotalNs),
             (unsigned long long)(now.gpuFrames - p.gpuFrames), msOf(now.cpuGameNs - p.cpuGameNs),
             msOf(now.cpuRenderNs - p.cpuRenderNs), msOf(now.cpuAudioNs - p.cpuAudioNs),
             msOf(now.cpuDvdNs - p.cpuDvdNs), msOf(now.cpuOtherNs - p.cpuOtherNs));
#endif
    writef(STDERR_FILENO,
           "[cos] hitch frame %u: busy %.1f ms (wall %.1f): events %.1f, begin_frame %.1f, cpd %.1f, "
           "aud %.1f, logic %.1f, painter %.1f, end_frame %.1f, other %.1f; pipelines +%u (%u "
           "queued), tex upload %.1f KiB, res loads +%u%s%s, scene %s%s%s\n",
           frame, msOf(f.busyNs), msOf(f.wallNs), msOf(f.eventsNs), msOf(beginFrameNs), msOf(f.cpdNs),
           msOf(f.audNs), msOf(f.logicNs), msOf(f.painterNs), msOf(f.endFrameNs), msOf(f.otherNs),
           (unsigned int)(ev.createdPipelines - prev.createdPipelines),
           stats != nullptr ? (unsigned int)stats->queuedPipelines : 0u,
           stats != nullptr ? stats->lastTextureUploadSize / 1024.0 : 0.0, resLoads,
           resLoads != 0 ? " last " : "", resLoads != 0 ? res : "", traceSceneName(ev.scene),
           sceneChanged ? " (new)" : "", platform);
}

// COS_PERF: the CSV. Rows go to sCsvBuf (game thread); sCsvLen is published after each row, so
// perfFlush from another thread (pc_exit) writes whole rows only. sCsvFlushing makes one writer.
constexpr const char* kPerfCsvHeader =
    "frame,t_ms,wall_ms,busy_ms,cpu_ms,wait_ms,begin_ms,cpd_read_ms,aud_execute_ms,logic_ms,"
    "painter_ms,aurora_end_frame_ms,other_ms,retrace\n";
int sCsvFd = -1;
char sCsvBuf[256 * 1024];
std::atomic<size_t> sCsvLen{0};
std::atomic_flag sCsvFlushing = ATOMIC_FLAG_INIT;

void csvWriteAll(const char* p, size_t n) {
    while (n > 0) {
        const ssize_t w = write(sCsvFd, p, n);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        p += w;
        n -= (size_t)w;
    }
}

void csvRow(unsigned int frame, uint64_t tNs, const PerfFrame& f, uint32_t retrace) {
    if (sCsvFd < 0) {
        return;
    }
    char row[256];
    char cpu[24] = "";
    if (f.cpuValid) {
        snprintf(cpu, sizeof(cpu), "%.3f", f.cpuNs / 1e6);
    }
    const int len = snprintf(row, sizeof(row),
                             "%u,%.3f,%.3f,%.3f,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u\n",
                             frame, tNs / 1e6, f.wallNs / 1e6, f.busyNs / 1e6, cpu, f.waitNs / 1e6,
                             f.beginNs / 1e6, f.cpdNs / 1e6, f.audNs / 1e6, f.logicNs / 1e6,
                             f.painterNs / 1e6, f.endFrameNs / 1e6, f.otherNs / 1e6,
                             (unsigned int)retrace);
    if (len <= 0 || len >= (int)sizeof(row)) {
        return;
    }
    size_t used = sCsvLen.load(std::memory_order_relaxed);
    if (used + (size_t)len > sizeof(sCsvBuf)) {
        perfFlush();
        used = sCsvLen.load(std::memory_order_relaxed);
        if (used + (size_t)len > sizeof(sCsvBuf)) {
            return; // pc_exit is writing the buffer out; the process ends
        }
    }
    memcpy(sCsvBuf + used, row, (size_t)len);
    sCsvLen.store(used + (size_t)len, std::memory_order_release);
}

// COS_PERF_EVERY: sums over the frames since the last perf line.
struct PerfWindow {
    bool started = false;
    unsigned int frames = 0;
    uint64_t startNs = 0;      // pc_frame_begin of the window's first frame
    uint32_t startRetrace = 0;
    uint64_t busyNs = 0;       // the frame minus the pace wait
    uint64_t maxBusyNs = 0;
    uint64_t waitNs = 0;
    uint64_t beginNs = 0;      // pumpEvents + aurora_begin_frame
    uint64_t endFrameNs = 0;   // aurora_end_frame
    uint64_t cpdNs = 0;
    uint64_t audNs = 0;
    uint64_t logicNs = 0;
    uint64_t painterNs = 0;
    uint64_t cpuNs = 0;
    bool cpuValid = true;
} sPerf;

// The Switch's "[cos] perf-switch" line, every COS_PERF_EVERY frames right after the perf line:
// what aurora_begin_frame waited for, the render worker's time per frame, the GL queue's fences,
// pipeline creation, texture uploads and disc reads. Nothing on other hosts.
void perfPlatformFrame(const PerfFrame& f, const FrameEvents& ev, const AuroraStats* stats,
                       unsigned int last, uint64_t now) {
#if defined(__SWITCH__)
    CosSwitchGfxStats cur{};
    cos_switch_gfx_stats(&cur);
    if (!sSwStarted) {
        // Like the perf line, the first window starts with the loop (its first frame included).
        sSwStarted = true;
        sSwWindow = CosSwitchGfxStats{};
        sSwWindowEvents = FrameEvents{};
        sSwWindowStartNs = sLoopStartNs;
    }
    if (cur.gpuFrames > sSwPrev.gpuFrames && sSwPrev.workerFrames != 0) {
        // Frames read back since the last game frame (usually one): each gets their average.
        const uint64_t frames = cur.gpuFrames - sSwPrev.gpuFrames;
        const uint64_t each = (cur.gpuTotalNs - sSwPrev.gpuTotalNs) / frames;
        for (uint64_t i = 0; i < frames && sSwGpuFrameNs.size() < 4096; ++i) {
            sSwGpuFrameNs.push_back(each);
        }
    }
    sSwPrev = cur;
    sSwWindowTexBytes += stats != nullptr ? stats->lastTextureUploadSize : 0;
    sSwWindowEventsNs += f.eventsNs;
    sSwFrames++;
    if (gConfig.perfEvery == 0 || sSwFrames < gConfig.perfEvery) {
        return;
    }
    // COS_PERF_LOG=0 (options menu, "Líneas perf en el log"): the window is measured and closed as
    // usual, only its lines are left out of the log.
    if (gConfig.perfLog) {
    const double n = sSwFrames;
    const CosSwitchGfxStats& w = sSwWindow;
    const double workerFrames = cur.workerFrames > w.workerFrames ? cur.workerFrames - w.workerFrames : 0;
    const double wf = workerFrames > 0 ? workerFrames : 1;
    const double wallS = (now - sSwWindowStartNs) / 1e9;
    writef(STDERR_FILENO,
           "[cos] perf-switch frames %u-%u: begin: events %.2f, slot wait %.2f, staging wait %.2f; "
           "queue-full wait %.2f; render worker %.2f ms/frame busy (encode %.2f, end_frame %.2f: "
           "unmap %.2f, acquire %.2f, submit %.2f, present %.2f; events %.2f), %.1f presents/s; "
           "gl %llu fences (%llu in flight), %llu waits %.2f ms, %llu glFinish %.2f ms; pipelines "
           "%u created, %llu compiled in %.1f ms (longest so far %.1f ms), %u queued; tex upload "
           "%.1f KiB; dvd %llu reads %.1f KiB %.1f ms; res loads %u; scene %s\n"
           "[cos] perf-switch dawn gl per frame: %.1f passes, %.1f draws, %.1f pipelines, %.1f bind "
           "groups, %.1f tex binds, %.1f texparams (%.1f skipped), %.1f uniform uploads, %.1f buffer "
           "copies %.1f KiB, %.1f tex uploads (%.2f ms, %.1f compressed calls, %.1f textures created "
           "%.2f ms); flush %.2f ms (%.1f items): execute %.2f, other work "
           "%.2f, release %.2f\n",
           last - (unsigned int)n + 1, last, msOf(sSwWindowEventsNs) / n,
           msOf(cur.frameSlotWaitNs - w.frameSlotWaitNs) / n, msOf(cur.stagingWaitNs - w.stagingWaitNs) / n,
           msOf(cur.queueFullWaitNs - w.queueFullWaitNs) / n, msOf(cur.workerBusyNs - w.workerBusyNs) / wf,
           msOf(cur.workerEncodeNs - w.workerEncodeNs) / wf,
           msOf(cur.workerEndFrameNs - w.workerEndFrameNs) / wf,
           msOf(cur.workerUnmapNs - w.workerUnmapNs) / wf,
           msOf(cur.workerAcquireNs - w.workerAcquireNs) / wf,
           msOf(cur.workerSubmitNs - w.workerSubmitNs) / wf,
           msOf(cur.workerPresentNs - w.workerPresentNs) / wf,
           msOf(cur.workerEventsNs - w.workerEventsNs) / wf, wallS > 0 ? workerFrames / wallS : 0.0,
           (unsigned long long)(cur.glFences - w.glFences), (unsigned long long)cur.glFencesPending,
           (unsigned long long)(cur.glWaits - w.glWaits), msOf(cur.glWaitNs - w.glWaitNs),
           (unsigned long long)(cur.glFinishes - w.glFinishes), msOf(cur.glFinishNs - w.glFinishNs),
           (unsigned int)(ev.createdPipelines - sSwWindowEvents.createdPipelines),
           (unsigned long long)(cur.pipelineCompiles - w.pipelineCompiles),
           msOf(cur.pipelineCompileNs - w.pipelineCompileNs), msOf(cur.pipelineCompileMaxNs),
           stats != nullptr ? (unsigned int)stats->queuedPipelines : 0u, sSwWindowTexBytes / 1024.0,
           (unsigned long long)(cur.dvdReads - w.dvdReads), (cur.dvdBytes - w.dvdBytes) / 1024.0,
           msOf(cur.dvdNs - w.dvdNs), ev.resources - sSwWindowEvents.resources,
           traceSceneName(ev.scene), (cur.glPasses - w.glPasses) / wf, (cur.glDraws - w.glDraws) / wf,
           (cur.glPipelines - w.glPipelines) / wf, (cur.glBindGroups - w.glBindGroups) / wf,
           (cur.glTexBinds - w.glTexBinds) / wf, (cur.glTexParams - w.glTexParams) / wf,
           (cur.glTexParamsSkipped - w.glTexParamsSkipped) / wf, (cur.glUniforms - w.glUniforms) / wf,
           (cur.glBufCopies - w.glBufCopies) / wf, (cur.glBufCopyBytes - w.glBufCopyBytes) / 1024.0 / wf,
           (cur.glTexUploads - w.glTexUploads) / wf, msOf(cur.glTexUploadNs - w.glTexUploadNs) / wf,
           (cur.glCompressedUploadCalls - w.glCompressedUploadCalls) / wf,
           (cur.glTexCreates - w.glTexCreates) / wf, msOf(cur.glTexCreateNs - w.glTexCreateNs) / wf,
           msOf(cur.glFlushNs - w.glFlushNs) / wf,
           (cur.glFlushItems - w.glFlushItems) / wf, msOf(cur.glExecuteNs - w.glExecuteNs) / wf,
           msOf(glOtherWorkNs(cur) - glOtherWorkNs(w)) / wf, msOf(cur.glReleaseNs - w.glReleaseNs) / wf);
    {
        // The render passes' replay split (dawn-switch-gl-replay-timers.patch): per frame, and the
        // glDraw* time per draw for the draws after a pipeline change, after a texture bind, others.
        const double draws = (cur.glDraws - w.glDraws) / wf;
        const double afterPipe = (cur.glDrawsAfterPipeline - w.glDrawsAfterPipeline) / wf;
        const double afterTex = (cur.glDrawsAfterTextures - w.glDrawsAfterTextures) / wf;
        const double others = draws > afterPipe + afterTex ? draws - afterPipe - afterTex : 0;
        const double drawMs = msOf(cur.glDrawCallNs - w.glDrawCallNs) / wf;
        const double afterPipeMs = msOf(cur.glDrawAfterPipelineNs - w.glDrawAfterPipelineNs) / wf;
        const double afterTexMs = msOf(cur.glDrawAfterTexturesNs - w.glDrawAfterTexturesNs) / wf;
        const double othersMs = drawMs > afterPipeMs + afterTexMs ? drawMs - afterPipeMs - afterTexMs : 0;
        const auto usEach = [](double ms, double count) { return count > 0 ? ms * 1000.0 / count : 0.0; };
        writef(STDERR_FILENO,
               "[cos] perf-switch dawn gl replay per frame: pipelines %.2f ms, bind groups %.2f, "
               "immediates %.2f, vertex state %.2f, draw calls %.2f (%.1f after a pipeline change "
               "%.2f ms = %.1f us each, %.1f after a texture bind %.2f ms = %.1f us each, %.1f "
               "others %.2f ms = %.1f us each); %.1f UBO binds, %.1f VAO binds, %.1f index binds\n",
               msOf(cur.glPipelineNs - w.glPipelineNs) / wf, msOf(cur.glBindGroupNs - w.glBindGroupNs) / wf,
               msOf(cur.glImmediatesNs - w.glImmediatesNs) / wf,
               msOf(cur.glVertexStateNs - w.glVertexStateNs) / wf, drawMs, afterPipe, afterPipeMs,
               usEach(afterPipeMs, afterPipe), afterTex, afterTexMs, usEach(afterTexMs, afterTex), others,
               othersMs, usEach(othersMs, others), (cur.glUniformBufferBinds - w.glUniformBufferBinds) / wf,
               (cur.glVertexArrayBinds - w.glVertexArrayBinds) / wf,
               (cur.glIndexBufferBinds - w.glIndexBufferBinds) / wf);
    }
    {
        // The rest of Execute (dawn-switch-gl-pass-timers.patch): every pass, the first pass of
        // each Execute alone, the copies, and what is left.
        const auto per = [&](uint64_t c, uint64_t p) { return msOf(c - p) / wf; };
        const double firstPasses = (cur.glFirstPasses - w.glFirstPasses) / wf;
        writef(STDERR_FILENO,
               "[cos] perf-switch dawn gl execute split per frame: passes %.2f ms (lazy clears %.2f, "
               "fbo setup %.2f, default state %.2f, clears %.2f, pass end %.2f, viewport/scissor/"
               "blend %.2f, replay %.2f, residual %.2f); first pass x%.2f %.2f ms (lazy clears %.2f, "
               "fbo setup %.2f, default state %.2f, clears %.2f, pass end %.2f, replay %.2f, "
               "residual %.2f); buffer copies %.2f ms (%.1f before the first pass %.2f ms, first "
               "copy %.2f ms); %.1f texture copies %.2f ms; execute residual %.2f ms\n",
               per(cur.glPassTotalNs, w.glPassTotalNs), per(cur.glPassLazyClearNs, w.glPassLazyClearNs),
               per(cur.glPassFramebufferNs, w.glPassFramebufferNs),
               per(cur.glPassDefaultStateNs, w.glPassDefaultStateNs),
               per(cur.glPassClearNs, w.glPassClearNs), per(cur.glPassEndNs, w.glPassEndNs),
               per(cur.glPassDynamicStateNs, w.glPassDynamicStateNs), per(glReplayNs(cur), glReplayNs(w)),
               per(glPassResidualNs(cur), glPassResidualNs(w)), firstPasses,
               per(cur.glFirstPassNs, w.glFirstPassNs),
               per(cur.glFirstPassLazyClearNs, w.glFirstPassLazyClearNs),
               per(cur.glFirstPassFramebufferNs, w.glFirstPassFramebufferNs),
               per(cur.glFirstPassDefaultStateNs, w.glFirstPassDefaultStateNs),
               per(cur.glFirstPassClearNs, w.glFirstPassClearNs),
               per(cur.glFirstPassEndNs, w.glFirstPassEndNs),
               per(cur.glFirstPassReplayNs, w.glFirstPassReplayNs),
               per(glFirstPassResidualNs(cur), glFirstPassResidualNs(w)),
               per(cur.glBufCopyNs, w.glBufCopyNs),
               (cur.glBufCopiesBeforeFirstPass - w.glBufCopiesBeforeFirstPass) / wf,
               per(cur.glBufCopyBeforeFirstPassNs, w.glBufCopyBeforeFirstPassNs),
               per(cur.glFirstBufCopyNs, w.glFirstBufCopyNs), (cur.glTexCopies - w.glTexCopies) / wf,
               per(cur.glTexCopyNs, w.glTexCopyNs), per(glExecuteResidualNs(cur), glExecuteResidualNs(w)));
    }
    {
        // GPU time per frame (dawn-switch-gl-gpu-timer.patch), over the frames read back in the
        // window (a few frames behind the CPU).
        const uint64_t gpuFrames = cur.gpuFrames - w.gpuFrames;
        if (cur.gpuTimerState != 1) {
            writef(STDERR_FILENO, "[cos] perf-switch gpu: %s\n",
                   cur.gpuTimerState == 2   ? "no timer (the driver has no GL_EXT_disjoint_timer_query)"
                   : cur.gpuTimerState == 3 ? "timer off (COS_SWITCH_GPU_TIMER=0)"
                                            : "timer not started");
        } else if (gpuFrames == 0) {
            writef(STDERR_FILENO, "[cos] perf-switch gpu: no frame read back (%llu dropped, %llu disjoint)\n",
                   (unsigned long long)(cur.gpuDropped - w.gpuDropped),
                   (unsigned long long)(cur.gpuDisjoint - w.gpuDisjoint));
        } else {
            const double g = (double)gpuFrames;
            const auto gms = [&](uint64_t c, uint64_t p) { return msOf(c - p) / g; };
            uint64_t maxNs = 0, p95Ns = 0;
            if (!sSwGpuFrameNs.empty()) {
                std::sort(sSwGpuFrameNs.begin(), sSwGpuFrameNs.end());
                maxNs = sSwGpuFrameNs.back();
                p95Ns = sSwGpuFrameNs[(sSwGpuFrameNs.size() - 1) * 95 / 100];
            }
            writef(STDERR_FILENO,
                   "[cos] perf-switch gpu per frame (%llu read back): %.2f ms (p95 %.2f, max %.2f): "
                   "efb passes %.2f, tex copy conv %.2f, present %.2f, imgui %.2f, copies %.2f, other "
                   "%.2f; first pass %.2f; %llu dropped, %llu disjoint; timer x%s\n",
                   (unsigned long long)gpuFrames, gms(cur.gpuTotalNs, w.gpuTotalNs), msOf(p95Ns),
                   msOf(maxNs), gms(cur.gpuEfbNs, w.gpuEfbNs), gms(cur.gpuTexConvNs, w.gpuTexConvNs),
                   gms(cur.gpuPresentNs, w.gpuPresentNs), gms(cur.gpuImguiNs, w.gpuImguiNs),
                   gms(cur.gpuCopyNs, w.gpuCopyNs), gms(cur.gpuOtherNs, w.gpuOtherNs),
                   gms(cur.gpuFirstPassNs, w.gpuFirstPassNs),
                   (unsigned long long)(cur.gpuDropped - w.gpuDropped),
                   (unsigned long long)(cur.gpuDisjoint - w.gpuDisjoint),
                   // dawn-switch-gl-gpu-timer-scale.patch: Tegra PTIMER ticks to real time.
                   getenv("COS_SWITCH_GPU_TIMER_SCALE") != nullptr ? getenv("COS_SWITCH_GPU_TIMER_SCALE")
                                                                   : "1.6276 (PTIMER)");
        }
        sSwGpuFrameNs.clear();
        // Per group (dawn-switch-gl-gpu-groups.patch): the window's GPU time per frame of each
        // group, largest first; groups are EFB passes, or draw-list buckets with COS_GPU_GROUPS.
        static std::vector<DawnSwitchGpuGroup> sGroupsPrev;
        static std::vector<DawnSwitchGpuGroup> sGroupsNow(512);
        const size_t groupCount = dawn_switch_gl_gpu_groups(sGroupsNow.data(), sGroupsNow.size());
        if (gpuFrames > 0 && groupCount > 0) {
            struct Row {
                const char* name;
                double ms, draws;
            };
            std::vector<Row> rows;
            double totalMs = 0;
            for (size_t i = 0; i < groupCount; ++i) {
                const DawnSwitchGpuGroup& c = sGroupsNow[i];
                const DawnSwitchGpuGroup* q = i < sGroupsPrev.size() ? &sGroupsPrev[i] : nullptr;
                const uint64_t ns = c.ns - (q != nullptr ? q->ns : 0);
                if (ns == 0) {
                    continue;
                }
                const double ms = msOf(ns) / (double)gpuFrames;
                rows.push_back(Row{c.name, ms, (double)(c.draws - (q != nullptr ? q->draws : 0)) / (double)gpuFrames});
                totalMs += ms;
            }
            std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.ms > b.ms; });
            std::string line;
            char item[160];
            for (size_t i = 0; i < rows.size() && i < 24; ++i) {
                snprintf(item, sizeof(item), "%s%s %.2f (%.0f)", i ? ", " : "", rows[i].name, rows[i].ms,
                         rows[i].draws);
                line += item;
            }
            writef(STDERR_FILENO, "[cos] perf-switch gpu groups per frame (ms, draws; %.2f ms in %zu groups): %s\n",
                   totalMs, rows.size(), line.c_str());
        }
        sGroupsPrev.assign(sGroupsNow.begin(), sGroupsNow.begin() + groupCount);
    }
    {
        // The CPU/GPU overlap check (docs/SWITCH_PERF_STUDY.md, section 6): the first render
        // pass's LoadOp clears waited for the previous frame's GPU work while Dawn freed its
        // swapchain texture under it; with the GL deletes deferred they cost tens of us.
        const double firstPasses = (double)(cur.glFirstPasses - w.glFirstPasses);
        // The present split (dawn-switch-gl-present-split.patch): the blit to the window, whose
        // first draw dequeues an NWindow buffer (waits for the compositor to free one), and
        // eglSwapBuffers (flush and queue). A blit near the GPU time means no CPU/GPU overlap.
        const double presents = (double)(cur.glPresents - w.glPresents);
        writef(STDERR_FILENO,
               "[cos] perf-switch present per present (%.0f): blit+dequeue %.2f ms, swap %.2f ms\n", presents,
               presents > 0 ? msOf(cur.glPresentBlitNs - w.glPresentBlitNs) / presents : 0.0,
               presents > 0 ? msOf(cur.glPresentSwapNs - w.glPresentSwapNs) / presents : 0.0);
        writef(STDERR_FILENO,
               "[cos] perf-switch gl stall: first-pass clears %.2f ms per frame; deferred deletes "
               "%s: +%llu deferred, +%llu deleted, +%llu forced, %llu pending\n",
               firstPasses > 0 ? msOf(cur.glFirstPassClearNs - w.glFirstPassClearNs) / firstPasses : 0.0,
               cur.glDeferDeleteState == 1   ? "on"
               : cur.glDeferDeleteState == 2 ? "off (COS_SWITCH_GL_DEFER_DELETE=0)"
                                             : "unused",
               (unsigned long long)(cur.glDeferDeletes - w.glDeferDeletes),
               (unsigned long long)(cur.glDeferDeletesDone - w.glDeferDeletesDone),
               (unsigned long long)(cur.glDeferDeletesForced - w.glDeferDeletesForced),
               (unsigned long long)cur.glDeferDeletePending);
    }
    // The operation mode and clocks now (docking mid-run changes them).
    char mode[128];
    cos_switch_describe_mode(mode, sizeof(mode));
    // CPU time per game frame of the threads (thread_wrap.c), against the render worker's busy
    // wall time: a worker CPU time well below its busy time means it waits (GPU, or preempted).
    writef(STDERR_FILENO,
           "[cos] perf-switch cpu per frame: game thread %.2f ms, render worker %.2f ms (busy %.2f ms "
           "wall), audio %.2f, dvd %.2f, compile %.2f, other threads %.2f; %s\n",
           msOf(cur.cpuGameNs - w.cpuGameNs) / n, msOf(cur.cpuRenderNs - w.cpuRenderNs) / n,
           msOf(cur.workerBusyNs - w.workerBusyNs) / n, msOf(cur.cpuAudioNs - w.cpuAudioNs) / n,
           msOf(cur.cpuDvdNs - w.cpuDvdNs) / n, msOf(cur.cpuCompileNs - w.cpuCompileNs) / n,
           msOf(cur.cpuOtherNs - w.cpuOtherNs) / n, mode);
    {
        // Where the render worker blocks inside Mesa (switch/native/source/nv_wrap.c): push-buffer
        // chunk switches (they wait for the GPU to retire the ring's next chunk), libdrm's blocking
        // fence waits, GPU submissions; per frame, the worker and every other thread apart. With
        // the worker CPU and busy times above: busy - CPU - fence waits = time the worker was
        // runnable but not running (another thread on its core).
        const auto nvLine = [&](int i) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "pushbuf space %.1f calls %.2f ms, fence waits %.1f %.2f ms (%.1f polls), kicks %.1f "
                     "%.2f ms, %.1f KiB pushed",
                     (cur.nvSpaceCalls[i] - w.nvSpaceCalls[i]) / wf, msOf(cur.nvSpaceNs[i] - w.nvSpaceNs[i]) / wf,
                     (cur.nvFenceWaits[i] - w.nvFenceWaits[i]) / wf,
                     msOf(cur.nvFenceWaitNs[i] - w.nvFenceWaitNs[i]) / wf,
                     (cur.nvFencePolls[i] - w.nvFencePolls[i]) / wf, (cur.nvKicks[i] - w.nvKicks[i]) / wf,
                     msOf(cur.nvKickNs[i] - w.nvKickNs[i]) / wf,
                     (cur.nvPushWords[i] - w.nvPushWords[i]) * 4.0 / 1024.0 / wf);
            return std::string(buf);
        };
        writef(STDERR_FILENO, "[cos] perf-switch nv per frame: render worker: %s; other threads: %s\n",
               nvLine(0).c_str(), nvLine(1).c_str());
        char table[1024];
        cos_switch_thread_table(table, sizeof(table), n);
        writef(STDERR_FILENO, "[cos] perf-switch threads (ms CPU per frame): %s\n", table);
    }
    } else {
        sSwGpuFrameNs.clear();
    }
    sSwWindow = cur;
    sSwWindowEvents = ev;
    sSwWindowTexBytes = 0;
    sSwWindowEventsNs = 0;
    sSwWindowStartNs = now;
    sSwFrames = 0;
#else
    (void)f;
    (void)ev;
    (void)stats;
    (void)last;
    (void)now;
#endif
}

void perfFrameEnd(uint64_t endFrameStartNs, uint64_t now, const AuroraStats* stats) {
    if (!sPerfOn) {
        return;
    }
    const uint64_t cpuNow = threadCpuNs();
    PerfFrame f{};
    f.waitNs = sFrameWaitNs;
    f.wallNs = now - sFrameStartNs;
    f.busyNs = f.wallNs > f.waitNs ? f.wallNs - f.waitNs : 0;
    f.cpuValid = cpuNow != UINT64_MAX && sFrameStartCpuNs != UINT64_MAX;
    if (f.cpuValid) {
        const uint64_t cpu = cpuNow - sFrameStartCpuNs;
        f.cpuNs = cpu > sFrameWaitCpuNs ? cpu - sFrameWaitCpuNs : 0;
    }
    f.beginNs = sBeginDoneNs - sFrameStartNs;
    f.eventsNs = sEventsDoneNs > sFrameStartNs ? sEventsDoneNs - sFrameStartNs : 0;
    f.cpdNs = sPhase[PC_PERF_CPD_READ].ns;
    f.audNs = sPhase[PC_PERF_AUD_EXECUTE].ns;
    f.painterNs = sPhase[PC_PERF_PAINTER].ns;
    const uint64_t gameNs = sPhase[PC_PERF_GAME].ns;
    f.logicNs = gameNs > f.painterNs ? gameNs - f.painterNs : 0;
    f.endFrameNs = now - endFrameStartNs;
    const uint64_t known = f.beginNs + f.cpdNs + f.audNs + gameNs + f.endFrameNs;
    f.otherNs = f.busyNs > known ? f.busyNs - known : 0;
    const uint32_t retrace = VIGetRetraceCount();
    const unsigned int last = pc_frame_count() + 1;
    csvRow(last, sFrameStartNs - sLoopStartNs, f, retrace);

    FrameEvents ev;
    ev.createdPipelines = stats != nullptr ? stats->createdPipelines : 0;
    ev.resources = traceResourceCount();
    ev.scene = traceScene();
    if (!sPrevEventsValid) {
        sPrevEvents = ev;
        sPrevEventsValid = true;
    }
    if (gConfig.hitchMs != 0 && f.busyNs > (uint64_t)gConfig.hitchMs * 1000000ull) {
        hitchLine(last, f, ev, sPrevEvents, stats);
    }
    sLastPerfFrame = f;
    sLastEvents = ev;
    sPrevEvents = ev;

    if (gConfig.perfEvery == 0) {
        return;
    }
    if (!sPerf.started) {
        sPerf.started = true;
        sPerf.startNs = sLoopStartNs;
        sPerf.startRetrace = sLoopStartRetrace;
    }
    sPerf.frames++;
    sPerf.busyNs += f.busyNs;
    sPerf.maxBusyNs = f.busyNs > sPerf.maxBusyNs ? f.busyNs : sPerf.maxBusyNs;
    sPerf.waitNs += f.waitNs;
    sPerf.beginNs += f.beginNs;
    sPerf.endFrameNs += f.endFrameNs;
    sPerf.cpdNs += f.cpdNs;
    sPerf.audNs += f.audNs;
    sPerf.logicNs += f.logicNs;
    sPerf.painterNs += f.painterNs;
    sPerf.cpuNs += f.cpuNs;
    sPerf.cpuValid = sPerf.cpuValid && f.cpuValid;
    if (sPerf.frames < gConfig.perfEvery) {
        return;
    }
    const double n = sPerf.frames;
    const double wallS = (now - sPerf.startNs) / 1e9;
    char cpu[48] = "n/a";
    if (sPerf.cpuValid) {
        snprintf(cpu, sizeof(cpu), "%.2f ms avg", sPerf.cpuNs / n / 1e6);
    }
    if (gConfig.perfLog) {
    writef(STDERR_FILENO, "[cos] perf frames %u-%u: game thread %.2f ms avg, %.2f ms max (begin %.2f, "
                          "aurora_end_frame %.2f); pace wait %.2f ms avg; %.1f fps, %.1f retraces/s "
                          "(60 = full speed); cpd_read %.2f, aud_execute %.2f, logic %.2f, painter "
                          "%.2f; cpu %s\n",
           last - sPerf.frames + 1, last, sPerf.busyNs / n / 1e6, sPerf.maxBusyNs / 1e6,
           sPerf.beginNs / n / 1e6, sPerf.endFrameNs / n / 1e6, sPerf.waitNs / n / 1e6,
           wallS > 0 ? n / wallS : 0.0, wallS > 0 ? (retrace - sPerf.startRetrace) / wallS : 0.0,
           sPerf.cpdNs / n / 1e6, sPerf.audNs / n / 1e6, sPerf.logicNs / n / 1e6,
           sPerf.painterNs / n / 1e6, cpu);
    }
    sPerf = PerfWindow{};
    sPerf.started = true;
    sPerf.startNs = now;
    sPerf.startRetrace = retrace;
}

// Drains Aurora's events. A quit request (window closed) ends the process: exit 0 for a plain
// run, 1 when a milestone or COS_FRAMES was still expected.
void pumpEvents() {
    const AuroraEvent* event;
    {
        JKRPcHostAllocScope hostAlloc;
        event = aurora_update();
    }
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

void perfOpen() {
    sPerfOn = gConfig.perfEvery != 0 || gConfig.perfPath != nullptr || gConfig.hitchMs != 0;
    if (gConfig.perfPath == nullptr) {
        return;
    }
    sCsvFd = open(gConfig.perfPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (sCsvFd < 0) {
        writef(STDERR_FILENO, "[cos] COS_PERF=\"%s\": cannot create it (%s)\n", gConfig.perfPath,
               strerror(errno));
        pc_exit(PC_EXIT_USAGE);
    }
    csvWriteAll(kPerfCsvHeader, strlen(kPerfCsvHeader));
    writef(STDERR_FILENO, "[cos] perf: one CSV row per game frame to %s (COS_PERF)\n",
           gConfig.perfPath);
}

void perfSetEvery(unsigned int every) {
    if (every == gConfig.perfEvery) {
        return;
    }
    gConfig.perfEvery = every;
    if (every != 0) {
        sPerfOn = true;
        // A new window from now (the first one otherwise counts from the start of the loop).
        sPerf = PerfWindow{};
        sPerf.started = sLoopStartNs != 0;
        sPerf.startNs = monotonicNs();
        sPerf.startRetrace = VIGetRetraceCount();
    }
    writef(STDERR_FILENO, "[cos] perf: game-thread frame times every %u frames (options menu)\n", every);
}

void perfFlush() {
    if (sCsvFd < 0 || sCsvFlushing.test_and_set(std::memory_order_acquire)) {
        return;
    }
    const size_t len = sCsvLen.load(std::memory_order_acquire);
    csvWriteAll(sCsvBuf, len);
    sCsvLen.store(0, std::memory_order_release);
    sCsvFlushing.clear(std::memory_order_release);
}

void logoResDone(const char* how) {
    if (sLogoResSynced && !sLogoResLogged) {
        sLogoResLogged = true;
        if (gConfig.audio && !mDoAud_zelAudio_c::isInitFlag()) {
            // The logo scene waits for it, so this would be a harness or game-flow error.
            writef(STDERR_FILENO, "[cos] logo-res: COS_AUDIO=on but mDoAud_Create has not finished\n");
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        writef(STDERR_FILENO, "[cos] logo-res: %s\n", how);
        pc_milestone("logo-res");
    }
}

} // namespace pc

using namespace pc;

extern "C" {

void pc_perf_begin(int phase) {
    if (!sPerfOn || phase < 0 || phase >= PC_PERF_PHASES) {
        return;
    }
    sPhase[phase].startWaitNs = sFrameWaitNs;
    sPhase[phase].startNs = monotonicNs();
}

void pc_perf_end(int phase) {
    if (!sPerfOn || phase < 0 || phase >= PC_PERF_PHASES || sPhase[phase].startNs == 0) {
        return;
    }
    const uint64_t ns = monotonicNs() - sPhase[phase].startNs;
    const uint64_t waitNs = sFrameWaitNs - sPhase[phase].startWaitNs;
    sPhase[phase].ns += ns > waitNs ? ns - waitNs : 0;
    sPhase[phase].startNs = 0;
}

void pc_frame_pace(unsigned long long periodNs) {
    sRequestedNs += periodNs;
    const uint64_t cpuStart = sPerfOn ? threadCpuNs() : UINT64_MAX;
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
    sFrameWaitNs += sPaceEndNs - sPaceStartNs;
    if (sPerfOn) {
        const uint64_t cpuEnd = threadCpuNs();
        if (cpuStart != UINT64_MAX && cpuEnd != UINT64_MAX) {
            sFrameWaitCpuNs += cpuEnd - cpuStart;
        }
    }
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
    sFrameWaitNs = 0;
    if (sPerfOn) {
        sFrameStartCpuNs = threadCpuNs();
        sFrameWaitCpuNs = 0;
        for (PerfPhase& phase : sPhase) {
            phase.ns = 0;
        }
    }
    pumpEvents();
    sEventsDoneNs = monotonicNs();
    // COS_GPU_GROUPS / COS_DRAW_CENSUS (pc_gpu_opts.h): before Aurora starts recording the frame.
    pc_gpu_groups_frame_begin(pc_frame_count() + 1);
    pc_dynres_frame_begin(pc_frame_count() + 1); // COS_DYNRES (pc_dynres.h)
    // Refused while the window cannot present (minimised, no surface yet): the console would not
    // run a frame without a display either. The stall watchdog reports a refusal that lasts.
    for (;;) {
        bool begun;
        {
            JKRPcHostAllocScope hostAlloc;
            begun = aurora_begin_frame();
        }
        if (begun) {
            break;
        }
        usleep(10 * 1000);
        pumpEvents();
    }
    sBeginDoneNs = monotonicNs();
}

void pc_frame_end(void) {
    const uint64_t endFrameStartNs = monotonicNs();
    {
        // Our own tools allocate host memory, not the game's current heap (JKRHeap.cpp): with the
        // menu open its rows, labels and settings strings were about 1,400 operator new calls
        // (~95 KiB) per 100 frames in the main heap (bug B8).
        JKRPcHostAllocScope hostAlloc;
        // The options menu (pc_menu.cpp): input, pause and its window, in this frame's ImGui frame.
        menuFrame();
        if (gConfig.fpsOverlay) {
            const uint64_t frameNs = endFrameStartNs - sFrameStartNs;
            overlayFrame(frameNs > sFrameWaitNs ? frameNs - sFrameWaitNs : 0);
        }
        precompileOverlay();
    }
    const AuroraStats* stats;
    {
        // Aurora's frame work allocates host memory, not the game's current heap (JKRHeap.cpp).
        JKRPcHostAllocScope hostAlloc;
        aurora_end_frame();
        // COS_SHOT: the frame is queued to Aurora's render worker; the readback goes in behind it.
        shotFrameEnd(pc_frame_count() + 1);
        telescopeDemoFrameEnd(pc_frame_count() + 1);
        menuFrameEnd(pc_frame_count() + 1);
        // COS_HD_TEXTURES (pc_hd_textures.h): the runtime toggle and the stats line.
        pc_hd_textures_frame_end(pc_frame_count() + 1);
        stats = aurora_get_stats();
    }
    if (gConfig.heapCheckEvery != 0 && (pc_frame_count() + 1) % gConfig.heapCheckEvery == 0) {
        heapCheckFrame(pc_frame_count() + 1);
    }
    if (gConfig.heapReportEvery != 0 && (pc_frame_count() + 1) % gConfig.heapReportEvery == 0) {
        heapReport("every COS_HEAP_REPORT frames");
    }
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
    // them every frame and keep the largest (stats above).
    if (stats != nullptr && stats->drawCallCount > sMaxDrawCalls) {
        sMaxDrawCalls = stats->drawCallCount;
    }
    if (stats != nullptr) {
        (sLogoCreated ? sUploadSinceLogo : sUploadBeforeLogo) += stats->lastTextureUploadSize;
    }

    const uint64_t perfNow = monotonicNs();
    perfFrameEnd(endFrameStartNs, perfNow, stats);
    if (sPerfOn) {
        perfPlatformFrame(sLastPerfFrame, sLastEvents, stats, pc_frame_count() + 1, perfNow);
    }
    pc_frame_tick();

    const unsigned int frames = pc_frame_count();
    titleStageFrame(frames);
    titleFrame(frames);
    titleAudioFrame(frames);
    outsetFrame(frames);
    actorSweepFrame(frames);
    fxSweepFrame(frames);
    resSweepFrame(frames);
    bgmHopFrame(frames);
    telescopeDemoFrame(frames);
    shoreFoamFrame(frames);
    npcVariantsFrame(frames);
    sailingFrame(frames);
    fileSelectFrame(frames);
    newGameFrame(frames);
    precompileFrame(frames);
    // Step 5.A (decision H10): with COS_AUDIO=on, mDoAud_Execute retries mDoAud_Create every frame
    // until JAudio is up (JAIZelBasic::init, the audio thread's DSP boot and handshake), then sets
    // the init flag the logo scene waits for.
    if (gConfig.audio && !sAudioLogged && mDoAud_zelAudio_c::isInitFlag()) {
        sAudioLogged = true;
        writef(STDERR_FILENO, "[cos] audio: mDoAud_Create done at frame %u; DSP handshake %s\n",
               frames, Dsp_Running_Check() ? "done" : "not done");
    }
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

    // M5 logo-scene: the scene exists and Aurora uploaded texture data after it was created (the
    // logo scene draws the boot logo from its first frame, logoInDraw).
    if (sLogoCreated && !sLogoLogged && sUploadSinceLogo > 0) {
        sLogoLogged = true;
        writef(STDERR_FILENO, "[cos] logo-scene: created at frame %u; %llu texture bytes uploaded "
                              "since (%llu before), up to %u draw calls in a frame\n",
               sLogoFrame, (unsigned long long)sUploadSinceLogo,
               (unsigned long long)sUploadBeforeLogo, (unsigned int)sMaxDrawCalls);
        pc_milestone("logo-scene");
    }
}

void pc_logo_res_object(const char* name, int files, int loaded) {
    writef(STDERR_FILENO, "[cos] logo-res: %s.arc %d files, %d converted\n", name, files, loaded);
    if (files <= 0 || loaded != files) {
        sLogoResGaps++;
    }
}

void pc_logo_res_synced(int archives, int files, int missing) {
    writef(STDERR_FILENO, "[cos] logo-res: all commands synced at frame %u: %d archives mounted, "
                          "%d files in main RAM, %d empty\n",
           pc_frame_count(), archives, files, missing);
    if (missing != 0 || sLogoResGaps != 0) {
        writef(STDERR_FILENO, "[cos] logo-res: %d object archive(s) incomplete, %d command(s) "
                              "left nothing\n", sLogoResGaps, missing);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    sLogoResSynced = true;
}

void pc_opening_scene_called(void) {
    logoResDone("dComIfG_changeOpeningScene called");
}

void pc_stage_created(const char* stageName, int roomNo, int stageFiles, int hasDzs) {
    const char* name = stageName != nullptr ? stageName : "(null)";
    writef(STDERR_FILENO, "[cos] stage: %s room %d created at frame %u; Stage archive %d files, "
                          "stage.dzs %s\n",
           name, roomNo, pc_frame_count(), stageFiles, hasDzs ? "found" : "missing");
    if (strcmp(name, "sea_T") == 0) {
        if (stageFiles <= 0 || !hasDzs) {
            writef(STDERR_FILENO, "[cos] opening: sea_T stage archive not mounted\n");
            pc_exit(PC_EXIT_CHECK_FAILED);
        }
        pc_milestone("opening");
        titleStageArm(roomNo);
    }
    outsetArm(name, roomNo);
}

void pc_logo_scene_created(int logoFiles, const ResTIMG* timg, unsigned int size) {
    if (logoFiles <= 0 || timg == nullptr) {
        writef(STDERR_FILENO, "[cos] logo-scene: Logo archive not mounted (%d entries, timg %p)\n",
               logoFiles, (const void*)timg);
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    const u32 width = timg->width;
    const u32 height = timg->height;
    const u32 imageOffset = timg->imageOffset != 0 ? (u32)timg->imageOffset : 0x20;
    const u32 imageSize = GXGetTexBufferSize(width, height, timg->format, timg->mipmapEnabled != 0,
                                             timg->mipmapCount);
    writef(STDERR_FILENO, "[cos] logo-scene: Logo archive %d entries; the logo BTI %ux%u "
                          "format %u, %u colours, image at 0x%x (%u bytes) in %u bytes\n",
           logoFiles, (unsigned int)width, (unsigned int)height, (unsigned int)timg->format,
           (unsigned int)(u16)timg->numColors, (unsigned int)imageOffset, (unsigned int)imageSize,
           size);
    if (width != 376 || height != 104 || imageOffset < sizeof(ResTIMG) ||
        (uint64_t)imageOffset + imageSize > size) {
        writef(STDERR_FILENO, "[cos] logo-scene: the boot logo's header is wrong\n");
        pc_exit(PC_EXIT_CHECK_FAILED);
    }
    sLogoCreated = true;
    sLogoFrame = pc_frame_count();
}

} // extern "C"
