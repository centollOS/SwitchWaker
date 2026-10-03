// Pipeline precompile at boot (docs/SWITCH_BUILD.md, "Pipeline precompile"): progress lines for
// Aurora's warm-up and, on the Switch, when it ends.
//
// At start Aurora queues every pipeline its pipeline cache knows (<cache>/pipeline_cache.db, merged
// at each start with the bundled initial_pipeline_cache.db next to the executable, if there is
// one: native/tools/gen_pipeline_cache.sh) on its compile thread, in order of first use, and builds
// them while the game runs. On the Mac that takes a moment; on the Switch every build holds the one
// GL context for 0.1-0.4 s (Mesa 20.1 compiles from source each run), so the warm-up belongs to the
// logos and menus:
//   COS_PRECOMPILE=boot  (Switch default) build until the game first enters its PLAY scene, then
//                        leave the rest to be built when first drawn (aurora_switch_stop_precompile,
//                        Aurora Switch patch 0007);
//   COS_PRECOMPILE=all   build every known pipeline whatever the game does (Aurora's behaviour);
//   COS_PRECOMPILE=off   no warm-up: each pipeline is built when first drawn.
// COS_PRECOMPILE_LOG (default on on the Switch, off elsewhere): "[cos] precompile N/M pipelines"
// once a second while the warm-up runs, and a last line when it is done or stopped. On the Mac only
// the log exists (Aurora there is unpatched: its warm-up always runs to the end) and counts from
// AuroraStats: M is what was queued when Aurora came up, N the pipelines built since (the warm-up's
// and any a draw asked for first).
#include "pc_internal.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#if defined(__SWITCH__)
#include <aurora/switch_precompile.h>

// Dawn's shared GL program cache (switch/dawn/patches/dawn-switch-gl-program-share.patch).
extern "C" void dawn_switch_gl_program_stats(uint64_t* linked, uint64_t* shared);
#endif

namespace pc {

namespace {

enum class Policy { Boot, All, Off };

constexpr int kPlayScene = 7; // fpcNm_PLAY_SCENE_e (pc_crash.cpp's scene names)

bool sLog = false;
bool sActive = false; // a warm-up is running and being reported
Policy sPolicy = Policy::All;
uint64_t sStartNs = 0;
uint64_t sNextLogNs = 0;
// Mac (AuroraStats): queued and created when Aurora came up.
uint32_t sTotal0 = 0;
uint32_t sCreated0 = 0;

struct Progress {
    uint32_t total = 0;
    uint32_t done = 0;
    uint32_t pending = 0;
    uint32_t dropped = 0;
    double compileS = -1; // compile thread time on the warm-up, -1 = unknown
};

Progress readProgress() {
    Progress p;
#if defined(__SWITCH__)
    AuroraSwitchPrecompile s{};
    aurora_switch_get_precompile(&s);
    p.total = s.total;
    p.done = s.done;
    p.pending = s.pending;
    p.dropped = s.dropped;
    p.compileS = s.compileNs / 1e9;
#else
    const AuroraStats* stats = aurora_get_stats();
    const uint32_t created = stats != nullptr ? stats->createdPipelines : sCreated0;
    const uint32_t queued = stats != nullptr ? stats->queuedPipelines : 0;
    p.total = sTotal0;
    p.done = created - sCreated0 < sTotal0 ? created - sCreated0 : sTotal0;
    p.pending = queued < sTotal0 ? queued : sTotal0;
    if (p.done + p.pending > sTotal0) {
        p.done = sTotal0 - p.pending;
    }
#endif
    return p;
}

// ", 37 GL programs shared" (Switch) or nothing.
void programNote(char* out, size_t size) {
    out[0] = '\0';
#if defined(__SWITCH__)
    uint64_t linked = 0;
    uint64_t shared = 0;
    dawn_switch_gl_program_stats(&linked, &shared);
    snprintf(out, size, "; GL programs %llu linked, %llu shared", (unsigned long long)linked,
             (unsigned long long)shared);
#else
    (void)size;
#endif
}

void logLine(const char* what, const Progress& p, unsigned int frames, uint64_t now) {
    const double s = (now - sStartNs) / 1e9;
    char compile[64] = "";
    if (p.compileS >= 0 && p.done > 0) {
        snprintf(compile, sizeof(compile), ", compile %.1f s (%.0f ms each)", p.compileS,
                 p.compileS * 1000 / p.done);
    }
    char programs[96];
    programNote(programs, sizeof(programs));
    writef(STDERR_FILENO, "[cos] precompile %s%u/%u pipelines, %.1f s%s%s; frame %u, scene %s\n", what,
           p.done, p.total, s, compile, programs, frames, traceSceneName(traceScene()));
}

#if defined(__SWITCH__)
void stopWarmup(const char* why, unsigned int frames, uint64_t now) {
    const uint32_t dropped = aurora_switch_stop_precompile();
    const Progress p = readProgress();
    if (sLog) {
        char what[96];
        snprintf(what, sizeof(what), "stopped (%s; %u left to build when first drawn) at ", why,
                 dropped);
        logLine(what, p, frames, now);
    }
    sActive = false;
}
#endif

} // namespace

// After aurora_initialize (pc_aurora_init): Aurora has queued its warm-up.
void precompileInit() {
#if defined(__SWITCH__)
    const char* policy = getenv("COS_PRECOMPILE");
    sPolicy = Policy::Boot;
    if (policy != nullptr && strcmp(policy, "all") == 0) {
        sPolicy = Policy::All;
    } else if (policy != nullptr && strcmp(policy, "off") == 0) {
        sPolicy = Policy::Off;
    }
    const bool logDefault = true;
#else
    sPolicy = Policy::All;
    const bool logDefault = false;
#endif
    const char* log = getenv("COS_PRECOMPILE_LOG");
    sLog = log == nullptr || log[0] == '\0' ? logDefault : strcmp(log, "0") != 0;
    sStartNs = monotonicNs();
    sNextLogNs = sStartNs + 1000000000ull;

#if !defined(__SWITCH__)
    const AuroraStats* stats = aurora_get_stats();
    sTotal0 = stats != nullptr ? stats->queuedPipelines : 0;
    sCreated0 = stats != nullptr ? stats->createdPipelines : 0;
#endif
    const Progress p = readProgress();
    sActive = p.total > 0;
    if (sLog) {
        static const char* const kPolicy[] = {"boot: until the first PLAY scene", "all", "off"};
        writef(STDERR_FILENO, "[cos] precompile: %u pipelines queued from the pipeline cache (%s)\n",
               p.total, kPolicy[(int)sPolicy]);
    }
#if defined(__SWITCH__)
    if (sActive && sPolicy == Policy::Off) {
        stopWarmup("COS_PRECOMPILE=off", 0, sStartNs);
    }
#endif
}

// pc_frame_end, every game frame.
void precompileFrame(unsigned int frames) {
    if (!sActive) {
        return;
    }
    const uint64_t now = monotonicNs();
#if defined(__SWITCH__)
    if (sPolicy == Policy::Boot && traceScene() == kPlayScene) {
        stopWarmup("PLAY scene", frames, now);
        return;
    }
#endif
    const Progress p = readProgress();
    if (p.pending == 0) {
        sActive = false;
        if (sLog) {
            logLine("done: ", p, frames, now);
        }
        return;
    }
    if (sLog && now >= sNextLogNs) {
        sNextLogNs = now + 1000000000ull;
        logLine("", p, frames, now);
    }
}

} // namespace pc
