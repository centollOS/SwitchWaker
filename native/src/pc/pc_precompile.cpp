// Pipeline precompile at boot (docs/SWITCH_BUILD.md, "Pipeline precompile"): a loading screen until
// the boot path's pipelines are built, a corner indicator while the rest are, and progress lines.
//
// At start Aurora queues every pipeline its pipeline cache knows (<cache>/pipeline_cache.db, merged
// at each start with the bundled initial_pipeline_cache.db next to the executable, if there is
// one: native/tools/gen_pipeline_cache.sh) on its compile thread and builds them while the game
// runs. The bundled file marks the pipelines recorded on the boot path (logos to Outset) as
// priority 0 (its pipeline_priority table); the Switch's Aurora queues those first (Switch patch
// 0008). On the Mac that all takes a moment; on the Switch every build holds the one GL context for
// 0.1-0.2 s (Mesa 20.1 compiles from source each run), so:
//   COS_PRECOMPILE=boot  (Switch default) a loading screen ("Preparing shaders... N/M" and a bar)
//                        before the game starts, until the priority pipelines are built (about
//                        20 s); then the game starts and the rest build behind the logos and menus,
//                        with "Shaders N/M" in the bottom-right corner, until the game first enters
//                        its PLAY scene: there the warm-up ends and what is left is built when first
//                        drawn (aurora_switch_stop_precompile, Switch patch 0007). A bundled file
//                        without the priority table gives no loading screen. Behind the logos the
//                        warm-up is throttled (Switch patch 0009, aurora_switch_set_warmup_throttle):
//                        each build holds the GL context the render worker needs for 0.1-0.3 s, so
//                        back to back they left the logos and menus at 4-7 frames/s; with
//                        COS_PRECOMPILE_DUTY=d (default 0.5; 0 or 1: not throttled) the compile
//                        thread builds about a fraction d of the time, each build starting right
//                        after a present; a pipeline a draw waits for is never held back. Only a
//                        build slower than COS_PRECOMPILE_SLOW_MS (default 25, a shader cache miss;
//                        Switch patch 0011) holds the next one back: with Mesa's shader cache warm
//                        (~12 ms a build) the warm-up runs back to back;
//   COS_PRECOMPILE=full  the loading screen until every known pipeline is built (minutes; no
//                        stutter from a pipeline the cache knows afterwards);
//   COS_PRECOMPILE=all   no loading screen; build every known pipeline whatever the game does,
//                        with the corner indicator until done (the behaviour before boot's screen);
//   COS_PRECOMPILE=off   no warm-up: each pipeline is built when first drawn.
// The loading screen keeps presenting frames and pumping Aurora's events (the Switch's HOME button,
// a closed window) and keeps the stall watchdog fed; the game's frame counter does not move.
//
// COS_PRECOMPILE_SCREEN says when boot and full show that loading screen:
//   auto    (default) only when the builds are slow. The warm-up starts without it while the
//           harness watches the first builds (up to kProbeBuilds of them, at most kProbeMs): if they
//           take no longer than COS_PRECOMPILE_SLOW_MS each on average (Mesa's shader cache has
//           them: ~12 ms against 100-300 ms for a miss), the game starts at once and the warm-up
//           goes on behind the logos, the priority pipelines still first and a pipeline a draw
//           needs still built when asked for; if not (a cold cache: the first start, or new
//           shaders), the loading screen comes up as before. If the builds turn slow behind the
//           logos (a partly warm cache) and the priority pipelines left would take more than
//           kLateMs at that pace, the loading screen comes up there until they are built (Switch);
//   always  the loading screen whenever the pipelines it waits for are not built yet (as before);
//   never   no loading screen; the warm-up runs behind the logos as with auto's fast case.
// The choice is logged in one "[cos] precompile screen" line.
//
// On the Mac nothing changes unless COS_PRECOMPILE is set (Aurora there is unpatched: its warm-up
// always runs to the end in order of first use, so `off` only hides the indicator). With it set the
// loading screen and indicator are drawn as on the Switch, to try them; `boot` there waits until
// as many pipelines were built since start as the bundled file next to build/native-mac/centollos marks
// priority 0 (an approximation: the Mac does not sort them first).
//
// COS_PRECOMPILE_LOG (default on on the Switch, off elsewhere): "[cos] precompile N/M pipelines"
// once a second while the warm-up runs (with the loading screen's priority count while it is up),
// and a last line when it is done or stopped. On the Mac the counts come from AuroraStats: M is what
// was queued when Aurora came up, N the pipelines built since (the warm-up's and any a draw asked
// for first).
#include "pc_internal.h"

#include "JSystem/JKernel/JKRHeap.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <imgui.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#if defined(__SWITCH__)
#include <aurora/switch_precompile.h>

// Dawn's shared GL program cache (switch/dawn/patches/dawn-switch-gl-program-share.patch).
extern "C" void dawn_switch_gl_program_stats(uint64_t* linked, uint64_t* shared);
// Dawn's GL counters (switch/dawn/patches, switch_stats::Counter): 65-67 are the render pipeline
// builds, their Tint translation ns and their ns holding the GL context
// (dawn-switch-gl-pipeline-compile.patch).
extern "C" void dawn_switch_gl_cmd_stats(uint64_t* out, size_t count);
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#include <sqlite3.h>
#endif

namespace pc {

namespace {

enum class Policy { Boot, Full, All, Off };
enum class Screen { Auto, Always, Never }; // COS_PRECOMPILE_SCREEN

constexpr int kLogoScene = 5; // fpcNm_LOGO_SCENE_e (pc_crash.cpp's scene names)
constexpr int kPlayScene = 7; // fpcNm_PLAY_SCENE_e

// COS_PRECOMPILE_SCREEN=auto: the first builds watched before deciding (at most this many, for at
// most this long: a cold build takes 100-300 ms, so a cold cache shows the screen after ~0.3 s)...
constexpr uint32_t kProbeBuilds = 8;
constexpr uint64_t kProbeMs = 300;
// ...and behind the logos, the slow builds needed (at least) and the priority work left at their
// pace (at least) that bring the loading screen up there.
constexpr uint32_t kLateSlowBuilds = 3;
constexpr double kLateMs = 1000.0;

bool sLog = false;
bool sUi = false;     // draw the loading screen and the corner indicator
bool sActive = false; // a warm-up is running and being reported
Policy sPolicy = Policy::All;
Screen sScreen = Screen::Auto;
double sSlowMs = 25.0;      // COS_PRECOMPILE_SLOW_MS
float sDuty = 0.0f;         // the throttle duty set behind the logos (Switch; 0 = off)
bool sBehindLogos = false;  // the loading screen was skipped: the target set builds behind the logos
bool sLateChecked = false;  // auto: the logos are over or the target set is built; no late screen
uint32_t sLateDone = 0;     // auto behind the logos: the warm-up's done count and compile time
double sLateCompileS = 0;   //   at the previous frame, and the slow builds seen since the game
uint32_t sSlowBuilds = 0;   //   started and their compile time
double sSlowS = 0;
uint64_t sStartNs = 0;
uint64_t sNextLogNs = 0;
// Mac (AuroraStats): queued and created when Aurora came up, and the bundled file's priority rows.
uint32_t sTotal0 = 0;
uint32_t sCreated0 = 0;
uint32_t sPriorityRows = 0;

struct Progress {
    uint32_t total = 0;
    uint32_t done = 0;
    uint32_t pending = 0;
    uint32_t dropped = 0;
    uint32_t priorityTotal = 0; // the priority pipelines queued (0: none known)
    uint32_t priorityDone = 0;
    double compileS = -1; // compile thread time on the warm-up, -1 = unknown
    float throttleDuty = 0; // Switch: the warm-up throttle (0 off), builds held back, time waited
    uint32_t throttled = 0;
    uint32_t unthrottled = 0; // warm-up builds fast enough (a shader cache hit) to be followed at once
    double throttleWaitS = 0;
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
    p.priorityTotal = s.priorityTotal;
    p.priorityDone = s.priorityDone;
    p.compileS = s.compileNs / 1e9;
    p.throttleDuty = s.throttleDuty;
    p.throttled = s.throttled;
    p.unthrottled = s.unthrottled;
    p.throttleWaitS = s.throttleWaitNs / 1e9;
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
    // Approximation (see the top): the first sPriorityRows built.
    p.priorityTotal = sPriorityRows < sTotal0 ? sPriorityRows : sTotal0;
    p.priorityDone = p.done < p.priorityTotal ? p.done : p.priorityTotal;
#endif
    return p;
}

#if defined(__APPLE__) && !defined(__SWITCH__)
// The bundled file's priority-0 rows (<directory of the executable>/initial_pipeline_cache.db).
uint32_t readPriorityRows() {
    char exe[PATH_MAX] = {};
    uint32_t size = sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) != 0) {
        return 0;
    }
    char real[PATH_MAX];
    const char* path = realpath(exe, real) != nullptr ? real : exe;
    const char* slash = strrchr(path, '/');
    if (slash == nullptr) {
        return 0;
    }
    char dbPath[PATH_MAX];
    snprintf(dbPath, sizeof(dbPath), "%.*s/initial_pipeline_cache.db", (int)(slash - path), path);
    sqlite3* db = nullptr;
    uint32_t rows = 0;
    if (sqlite3_open_v2(dbPath, &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM pipeline_priority WHERE priority = 0", -1,
                               &stmt, nullptr) == SQLITE_OK &&
            sqlite3_step(stmt) == SQLITE_ROW) {
            rows = (uint32_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return rows;
}
#endif

// "; GL programs 37 linked, 4 shared; build split ..." (Switch) or nothing.
void programNote(char* out, size_t size, const Progress& p) {
    out[0] = '\0';
#if defined(__SWITCH__)
    uint64_t linked = 0;
    uint64_t shared = 0;
    dawn_switch_gl_program_stats(&linked, &shared);
    uint64_t gl[68] = {};
    dawn_switch_gl_cmd_stats(gl, 68);
    const double builds = gl[65] > 0 ? (double)gl[65] : 1.0;
    char throttle[96] = "";
    if (p.throttleDuty > 0) {
        snprintf(throttle, sizeof(throttle), "; throttle duty %.2f, %u held back, %.1f s waited, %u fast builds not held",
                 p.throttleDuty, p.throttled, p.throttleWaitS, p.unthrottled);
    }
    // Tint (no GL context) against the GL part (compile, link; the render worker waits for it).
    snprintf(out, size,
             "; GL programs %llu linked, %llu shared; %llu pipeline builds: tint %.0f ms, GL context "
             "%.0f ms each%s",
             (unsigned long long)linked, (unsigned long long)shared, (unsigned long long)gl[65],
             gl[66] / 1e6 / builds, gl[67] / 1e6 / builds, throttle);
#else
    (void)p;
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
    char programs[256];
    programNote(programs, sizeof(programs), p);
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

constexpr ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

// The loading screen's panel, centred: "Preparing shaders... N/M" and a bar.
void drawLoadingPanel(uint32_t done, uint32_t total) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##cos_precompile_loading", nullptr, kPanelFlags)) {
        ImGui::SetWindowFontScale(2.0f);
        ImGui::Text("Preparing shaders... %u/%u", done, total);
        const float width = display.x * 0.4f;
        ImGui::ProgressBar(total != 0 ? (float)done / (float)total : 0.0f, ImVec2(width, 0.0f), "");
    }
    ImGui::End();
}

// Fed to the loading screen: its done and total counts, and whether it is finished.
struct LoadingTarget {
    uint32_t done = 0;
    uint32_t total = 0;
    bool finished = false;
};

LoadingTarget loadingTarget(const Progress& p) {
    LoadingTarget t;
    if (sPolicy == Policy::Full) {
        t.done = p.done;
        t.total = p.total;
        t.finished = p.pending == 0;
    } else {
        t.done = p.priorityDone;
        t.total = p.priorityTotal;
        t.finished = p.priorityDone >= p.priorityTotal;
    }
    return t;
}

// Aurora's events during the loading screen: a quit request (window closed; on the Switch the
// system ending the app) ends the process as the frame loop's pumpEvents does.
void loadingEvents() {
    for (const AuroraEvent* event = aurora_update(); event != nullptr && event->type != AURORA_NONE; event++) {
        if (event->type == AURORA_EXIT) {
            const bool expected = gConfig.milestone != nullptr || gConfig.frames != 0;
            writef(STDERR_FILENO, "[cos] quit requested (window closed) on the shader loading screen%s\n",
                   expected ? " before the expected end of the run" : "");
            pc_exit(expected ? PC_EXIT_CHECK_FAILED : PC_EXIT_REACHED);
        }
    }
}

} // namespace

// After aurora_initialize (pc_aurora_init): Aurora has queued its warm-up.
void precompileInit() {
    const char* policy = getenv("COS_PRECOMPILE");
    const bool policySet = policy != nullptr && policy[0] != '\0';
#if defined(__SWITCH__)
    sPolicy = Policy::Boot;
    sUi = true;
    const bool logDefault = true;
#else
    sPolicy = Policy::All;
    sUi = policySet; // the Mac draws nothing unless asked
    const bool logDefault = false;
#endif
    if (policySet) {
        if (strcmp(policy, "boot") == 0) {
            sPolicy = Policy::Boot;
        } else if (strcmp(policy, "full") == 0) {
            sPolicy = Policy::Full;
        } else if (strcmp(policy, "all") == 0) {
            sPolicy = Policy::All;
        } else if (strcmp(policy, "off") == 0) {
            sPolicy = Policy::Off;
            sUi = false;
        } else {
            writef(STDERR_FILENO, "[cos] COS_PRECOMPILE=%s: expected boot, full, all or off\n", policy);
            pc_exit(PC_EXIT_USAGE);
        }
    }
    const char* screen = getenv("COS_PRECOMPILE_SCREEN");
    if (screen != nullptr && screen[0] != '\0') {
        if (strcmp(screen, "auto") == 0) {
            sScreen = Screen::Auto;
        } else if (strcmp(screen, "always") == 0) {
            sScreen = Screen::Always;
        } else if (strcmp(screen, "never") == 0) {
            sScreen = Screen::Never;
        } else {
            writef(STDERR_FILENO, "[cos] COS_PRECOMPILE_SCREEN=%s: expected auto, always or never\n", screen);
            pc_exit(PC_EXIT_USAGE);
        }
    }
    // COS_PRECOMPILE_SLOW_MS (default 25, about a frame): a build longer than this is a shader cache
    // miss. It decides auto's loading screen, and behind the logos only a build longer than this holds
    // the next one back (Switch patch 0011); cache hits (~12 ms) warm up back to back. 0: every build
    // counts as slow.
    const char* slowEnv = getenv("COS_PRECOMPILE_SLOW_MS");
    if (slowEnv != nullptr && slowEnv[0] != '\0') {
        sSlowMs = strtod(slowEnv, nullptr);
    }
    if (!(sSlowMs >= 0.0)) {
        sSlowMs = 0.0;
    }
    const char* log = getenv("COS_PRECOMPILE_LOG");
    sLog = log == nullptr || log[0] == '\0' ? logDefault : strcmp(log, "0") != 0;
    sStartNs = monotonicNs();
    sNextLogNs = sStartNs + 1000000000ull;

#if !defined(__SWITCH__)
    const AuroraStats* stats = aurora_get_stats();
    sTotal0 = stats != nullptr ? stats->queuedPipelines : 0;
    sCreated0 = stats != nullptr ? stats->createdPipelines : 0;
#if defined(__APPLE__)
    if (sPolicy == Policy::Boot) {
        sPriorityRows = readPriorityRows();
    }
#endif
#endif
    const Progress p = readProgress();
    sActive = p.total > 0;
    if (sLog) {
        static const char* const kPolicy[] = {
            "boot: loading screen for the priority set, then until the first PLAY scene",
            "full: loading screen for every pipeline", "all", "off"};
        static const char* const kScreen[] = {"auto", "always", "never"};
        const bool screenApplies = sPolicy == Policy::Boot || sPolicy == Policy::Full;
        writef(STDERR_FILENO,
               "[cos] precompile: %u pipelines queued from the pipeline cache, %u of them priority (%s%s%s)\n",
               p.total, p.priorityTotal, kPolicy[(int)sPolicy],
               screenApplies ? "; COS_PRECOMPILE_SCREEN=" : "", screenApplies ? kScreen[(int)sScreen] : "");
    }
#if defined(__SWITCH__)
    if (sActive && sPolicy == Policy::Off) {
        stopWarmup("COS_PRECOMPILE=off", 0, sStartNs);
    }
#endif
}

namespace {
void loadingScreen();

// The warm-up throttle behind the logos (Switch patch 0009; COS_PRECOMPILE_DUTY), 0 = off.
void setThrottle(float duty) {
#if defined(__SWITCH__)
    aurora_switch_set_warmup_throttle(duty);
#else
    (void)duty;
#endif
}

// COS_PRECOMPILE=boot on the Switch, once the game starts: the rest of the warm-up is throttled
// (Switch patch 0009) so that the logos and menus keep their frame rate.
void startThrottle() {
#if defined(__SWITCH__)
    if (!sActive || sPolicy != Policy::Boot) {
        return;
    }
    float duty = 0.5f;
    const char* env = getenv("COS_PRECOMPILE_DUTY");
    if (env != nullptr && env[0] != '\0') {
        duty = strtof(env, nullptr);
    }
    const bool on = duty > 0.0f && duty < 1.0f;
    // Only a build longer than COS_PRECOMPILE_SLOW_MS (a shader cache miss) holds the next one back.
    aurora_switch_set_warmup_slow_ns((uint64_t)(sSlowMs * 1e6));
    sDuty = on ? duty : 0.0f;
    setThrottle(sDuty);
    if (sLog) {
        if (on) {
            writef(STDERR_FILENO,
                   "[cos] precompile throttle: after a build longer than %.0f ms (COS_PRECOMPILE_SLOW_MS) the "
                   "next waits so that the warm-up builds about %.0f%% of the time, starting after a present "
                   "(COS_PRECOMPILE_DUTY=%.2f); faster builds follow at once\n",
                   sSlowMs, duty * 100.0, duty);
        } else {
            writef(STDERR_FILENO, "[cos] precompile throttle: off (COS_PRECOMPILE_DUTY=%s)\n",
                   env != nullptr ? env : "");
        }
    }
#endif
}
} // namespace

// After precompileInit, before the game starts: with COS_PRECOMPILE=boot or full, present the
// loading screen until its pipelines are built (COS_PRECOMPILE_SCREEN=auto: only if the builds are
// slow), then (boot, Switch) throttle the rest.
void precompileLoadingScreen() {
    loadingScreen();
    startThrottle();
}

namespace {
// COS_PRECOMPILE_SCREEN_FPS (default 10, 1-60): the loading screen's frame rate cap. At 60 the
// screen took 1458 presents in 37 s on the Switch while 176 pipelines built (17.4 s and 147
// presents in the run before): every present runs the render worker and takes the GL context.
uint64_t loadingScreenPeriodNs() {
    double fps = 10.0;
    const char* env = getenv("COS_PRECOMPILE_SCREEN_FPS");
    if (env != nullptr && env[0] != '\0') {
        fps = strtod(env, nullptr);
    }
    if (!(fps >= 1.0)) {
        fps = 1.0;
    }
    if (fps > 60.0) {
        fps = 60.0;
    }
    return (uint64_t)(1e9 / fps);
}

const char* targetName() {
    return sPolicy == Policy::Full ? "queued" : "priority";
}

// COS_PRECOMPILE_SCREEN=auto, before the game starts: watches the warm-up's first builds (up to
// kProbeBuilds, at most kProbeMs; nothing is presented meanwhile, events are pumped) and returns
// whether they were fast, i.e. no loading screen is needed. Logs the decision.
bool probeFast() {
    // The first event pump can take a while (the Mac's window shows: ~190 ms), so it comes before the
    // clock starts; the game's first frame would pay it anyway.
    watchdogPulse();
    loadingEvents();
    const uint64_t startNs = monotonicNs();
    const Progress p0 = readProgress();
    Progress p = p0;
    LoadingTarget t = loadingTarget(p);
    for (;;) {
        if (t.finished || p.done - p0.done >= kProbeBuilds || monotonicNs() - startNs >= kProbeMs * 1000000ull) {
            break;
        }
        watchdogPulse();
        loadingEvents();
        usleep(2000);
        p = readProgress();
        t = loadingTarget(p);
    }
    const double wallS = (monotonicNs() - startNs) / 1e9;
    const uint32_t builds = p.done - p0.done;
    // The compile thread's time per build where Aurora reports it (Switch), else the wall time per
    // build (the warm-up runs back to back meanwhile).
    const bool compileKnown = p.compileS >= 0 && p0.compileS >= 0;
    const double eachMs = builds == 0 ? wallS * 1000.0
                                      : (compileKnown ? (p.compileS - p0.compileS) : wallS) * 1000.0 / builds;
    const bool fast = t.finished || (builds > 0 && eachMs <= sSlowMs);
    if (sLog) {
        char measured[128];
        if (builds == 0) {
            snprintf(measured, sizeof(measured), "no build finished in %.2f s", wallS);
        } else {
            snprintf(measured, sizeof(measured), "%u builds in %.2f s, %.1f ms each%s", builds, wallS, eachMs,
                     compileKnown ? "" : " (wall time)");
        }
        if (t.finished) {
            writef(STDERR_FILENO,
                   "[cos] precompile screen auto: %s: all %u %s pipelines already built, no loading screen; the "
                   "game starts\n",
                   measured, t.total, targetName());
        } else if (fast) {
            writef(STDERR_FILENO,
                   "[cos] precompile screen auto: %s (COS_PRECOMPILE_SLOW_MS=%.0f): shaders cached, no loading "
                   "screen; the game starts and the %u %s pipelines left build first behind the logos\n",
                   measured, sSlowMs, t.total - t.done, targetName());
        } else {
            writef(STDERR_FILENO,
                   "[cos] precompile screen auto: %s (COS_PRECOMPILE_SLOW_MS=%.0f): shader cache cold, loading "
                   "screen for the %u %s pipelines left\n",
                   measured, sSlowMs, t.total - t.done, targetName());
        }
    }
    return fast;
}

// Presents the loading screen (at most COS_PRECOMPILE_SCREEN_FPS frames a second) and pumps events
// until its pipelines are built.
void runLoadingScreen() {
    Progress p = readProgress();
    LoadingTarget t = loadingTarget(p);
    const uint64_t startNs = monotonicNs();
    uint64_t nextLogNs = startNs + 1000000000ull;
    unsigned int presented = 0;
    const uint64_t periodNs = loadingScreenPeriodNs();
    if (sLog) {
        writef(STDERR_FILENO, "[cos] precompile loading screen: waiting for %u %s pipelines, %.0f frames/s "
                              "at most (COS_PRECOMPILE_SCREEN_FPS)\n",
               t.total, targetName(), 1e9 / (double)periodNs);
    }
    for (;;) {
        const uint64_t frameStartNs = monotonicNs();
        watchdogPulse();
        loadingEvents();
        p = readProgress();
        t = loadingTarget(p);
        if (t.finished) {
            break;
        }
        if (aurora_begin_frame()) {
            drawLoadingPanel(t.done, t.total);
            aurora_end_frame();
            presented++;
        }
        const uint64_t now = monotonicNs();
        if (sLog && now >= nextLogNs) {
            nextLogNs = now + 1000000000ull;
            writef(STDERR_FILENO, "[cos] precompile loading screen %u/%u, %.1f s, %u frames presented\n",
                   t.done, t.total, (now - startNs) / 1e9, presented);
        }
        // At most COS_PRECOMPILE_SCREEN_FPS frames a second (default 10): each present needs the
        // render worker and, on the Switch, the one GL context the compile thread builds with.
        const uint64_t frameNs = monotonicNs() - frameStartNs;
        if (frameNs < periodNs) {
            usleep((useconds_t)((periodNs - frameNs) / 1000));
        }
    }
    const uint64_t endNs = monotonicNs();
    if (sLog) {
        writef(STDERR_FILENO,
               "[cos] precompile loading screen done: %u/%u %s pipelines in %.1f s, %u frames presented; "
               "%u/%u of the warm-up built, the game %s\n",
               t.done, t.total, targetName(), (endNs - startNs) / 1e9, presented, p.done, p.total,
               pc_frame_count() == 0 ? "starts" : "goes on");
    }
}

// Before the game starts, with COS_PRECOMPILE=boot or full: the loading screen, unless
// COS_PRECOMPILE_SCREEN says otherwise (auto: only when the first builds are slow).
void loadingScreen() {
    if (!sActive || !sUi || (sPolicy != Policy::Boot && sPolicy != Policy::Full)) {
        return;
    }
    const Progress p = readProgress();
    const LoadingTarget t = loadingTarget(p);
    if (sPolicy == Policy::Boot && t.total == 0) {
        if (sLog) {
            writef(STDERR_FILENO, "[cos] precompile loading screen: skipped (the bundled pipeline cache "
                                  "marks no priority pipelines; native/tools/gen_pipeline_cache.sh)\n");
        }
        return;
    }
    if (t.finished) {
        return;
    }
    if (sScreen == Screen::Never) {
        if (sLog) {
            writef(STDERR_FILENO, "[cos] precompile screen never (COS_PRECOMPILE_SCREEN): no loading screen; "
                                  "the %u %s pipelines left build first behind the logos\n",
                   t.total - t.done, targetName());
        }
        sBehindLogos = true;
        sLateChecked = true;
        return;
    }
    if (sScreen == Screen::Auto) {
        if (probeFast()) {
            sBehindLogos = true;
            const Progress now = readProgress();
            sLateDone = now.done;
            sLateCompileS = now.compileS;
            return;
        }
    } else if (sLog) {
        writef(STDERR_FILENO, "[cos] precompile screen always (COS_PRECOMPILE_SCREEN): loading screen for the "
                              "%u %s pipelines left\n",
               t.total - t.done, targetName());
    }
    runLoadingScreen();
}

// COS_PRECOMPILE_SCREEN=auto with the game started behind no loading screen, every frame while the
// logos run: if the builds turned slow (a partly warm shader cache) and the target set left would
// take more than kLateMs at their pace, the loading screen comes up until it is built. Needs the
// compile thread's time (Switch); once past the logo scene, or with the set built, it stops looking.
void lateScreenCheck(const Progress& p) {
    if (!sBehindLogos || sLateChecked) {
        return;
    }
    const LoadingTarget t = loadingTarget(p);
    const int scene = traceScene();
    if (t.finished || p.compileS < 0 || (scene >= 0 && scene != kLogoScene)) {
        sLateChecked = true;
        return;
    }
    const uint32_t builds = p.done - sLateDone;
    const double compileS = p.compileS - sLateCompileS;
    sLateDone = p.done;
    sLateCompileS = p.compileS;
    if (builds == 0 || compileS * 1000.0 / builds <= sSlowMs) {
        return;
    }
    sSlowBuilds += builds;
    sSlowS += compileS;
    const double eachMs = sSlowS * 1000.0 / sSlowBuilds;
    const double leftMs = (t.total - t.done) * eachMs;
    if (sSlowBuilds < kLateSlowBuilds || leftMs < kLateMs) {
        return;
    }
    sLateChecked = true;
    if (sLog) {
        writef(STDERR_FILENO,
               "[cos] precompile screen auto: builds turned slow behind the logos (%u slow builds, %.0f ms "
               "each; %u %s pipelines left, about %.1f s at that pace): loading screen until they are built\n",
               sSlowBuilds, eachMs, t.total - t.done, targetName(), leftMs / 1000.0);
    }
    // Back to back meanwhile (the throttle would hold slow builds back for the frames' sake).
    setThrottle(0.0f);
    {
        // Aurora's frame work allocates host memory, not the game's current heap (JKRHeap.cpp).
        JKRPcHostAllocScope hostAlloc;
        runLoadingScreen();
    }
    setThrottle(sDuty);
}
} // namespace

// pc_frame_end before aurora_end_frame, every game frame: "Shaders N/M" in the bottom-right corner
// while the warm-up runs.
void precompileOverlay() {
    if (!sActive || !sUi || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    const Progress p = readProgress();
    if (p.pending == 0) {
        return;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x - 8.0f, display.y - 8.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("##cos_precompile_indicator", nullptr, kPanelFlags)) {
        ImGui::Text("Shaders %u/%u", p.done, p.total);
    }
    ImGui::End();
}

// pc_frame_end, every game frame.
void precompileFrame(unsigned int frames) {
    if (!sActive) {
        return;
    }
    const uint64_t now = monotonicNs();
    if (sPolicy == Policy::Boot && traceScene() == kPlayScene) {
#if defined(__SWITCH__)
        stopWarmup("PLAY scene", frames, now);
#else
        // Unpatched Aurora goes on building; only the report and the indicator end.
        sActive = false;
        if (sLog) {
            logLine("no longer reported (PLAY scene) at ", readProgress(), frames, now);
        }
#endif
        return;
    }
    const Progress p = readProgress();
    if (p.pending == 0) {
        sActive = false;
        if (sLog) {
            logLine("done: ", p, frames, now);
        }
        return;
    }
    if (sScreen == Screen::Auto && sUi) {
        lateScreenCheck(p);
    }
    if (sLog && now >= sNextLogNs) {
        sNextLogNs = now + 1000000000ull;
        logLine("", p, frames, now);
    }
}

} // namespace pc
