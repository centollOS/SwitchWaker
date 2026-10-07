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
//                        before the game starts when COS_PRECOMPILE_SCREEN (below) calls for one:
//                        with auto, until the whole warm-up is built; with priority or always, until
//                        the priority pipelines are built (about 20 s); then the game starts and
//                        the rest build behind the logos and menus, with "Shaders N/M" in the
//                        bottom-right corner, until the game first enters
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
//   auto     (default) only when there is slow work: a cold shader cache's builds (100-300 ms each
//            on the console) or the rest of a warm-up a previous start did not finish. The warm-up
//            starts without it while the harness watches the first builds (up to kProbeBuilds of
//            them, at most kProbeMs) and reads <cache>/precompile_state.txt, the outcome of the last
//            warm-up (complete, or stopped at N of M). If the builds average more than
//            COS_PRECOMPILE_SLOW_MS each (cold), every pipeline left is slow work; if they are fast
//            and the last warm-up completed, none is; if it stopped at N of M (or there is no
//            record), the pipelines past N are assumed cold. If that slow work, at the measured (or
//            recorded; 100 ms when neither) pace, would take more than COS_PRECOMPILE_SCREEN_MIN_S
//            (default 3 s), the loading screen comes up and stays until the WHOLE warm-up is built,
//            back to back (no throttle), with "N/M, ~X s" left from the measured pace: the first
//            start (or the first after a shader change) does the one-time work up front and the
//            title and menus never stutter on it. Otherwise the game starts at once and the warm-up
//            goes on behind the logos as with priority's fast case; if the builds turn slow there
//            (Switch, logo scene) the loading screen comes up there until the whole warm-up is
//            built, once the sample is big enough (at least 8 slow builds among the last 16, or
//            1.5 s of slow work measured) and the slow work left - the pipelines left times the
//            recent builds' slow share times their cost - passes the same limit (pc_precompile_gate.h).
//            A build's time leaves out its wait for the GL context (another thread, the render
//            worker, had it; Dawn's dawn-switch-gl-pipeline-wait.patch): on a warm boot the builds
//            during the game's first frame waited ~100 ms each for it and used to bring up the
//            screen. Every build longer than COS_PRECOMPILE_SLOW_MS is logged ("[cos] precompile
//            slow build": its pipeline key and where its time went);
//   priority the earlier auto: the loading screen for the priority set only, when the first builds
//            are slow (cold); the rest builds behind the logos (throttled). If the builds turn slow
//            behind the logos and the priority pipelines left would take more than kLateMs at that
//            pace, the loading screen comes up there until they are built (Switch);
//   always   the loading screen whenever the pipelines it waits for (the priority set for boot) are
//            not built yet (the behaviour before auto);
//   never    no loading screen; the warm-up runs behind the logos.
// The choice is logged in one "[cos] precompile screen" line, with the counts and the estimate.
// The loading screen's text follows the console's language (Spanish or English; COS_LANG=es|en
// overrides it, and LANG elsewhere).
//
// On the Mac nothing changes unless COS_PRECOMPILE is set (Aurora there is unpatched: its warm-up
// always runs to the end in order of first use, so `off` only hides the indicator). With it set the
// loading screen and indicator are drawn as on the Switch, to try them; `boot` there waits until
// as many pipelines were built since start as the bundled file next to build/native-mac/switchwaker marks
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
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

#if defined(__SWITCH__)
#include "cos_switch.h"

#include <aurora/switch_precompile.h>

// Dawn's shared GL program cache (switch/dawn/patches/dawn-switch-gl-program-share.patch).
extern "C" void dawn_switch_gl_program_stats(uint64_t* linked, uint64_t* shared);
// Dawn's GL counters (switch/dawn/patches, switch_stats::Counter): 65-67 are the render pipeline
// builds, their Tint translation ns and their ns holding the GL context
// (dawn-switch-gl-pipeline-compile.patch).
extern "C" void dawn_switch_gl_cmd_stats(uint64_t* out, size_t count);
#else
#include <limits.h>
#include <sqlite3.h>
#endif

#include "pc_precompile_gate.h"

namespace pc {

namespace {

enum class Policy { Boot, Full, All, Off };
enum class Screen { Auto, Priority, Always, Never }; // COS_PRECOMPILE_SCREEN

constexpr int kLogoScene = 5; // fpcNm_LOGO_SCENE_e (pc_crash.cpp's scene names)
constexpr int kPlayScene = 7; // fpcNm_PLAY_SCENE_e

// COS_PRECOMPILE_SCREEN=auto: the first builds watched before deciding (at most this many, for at
// most this long: a cold build takes 100-300 ms, so a cold cache shows the screen after ~0.3 s)...
constexpr uint32_t kProbeBuilds = 8;
constexpr uint64_t kProbeMs = 300;
// ...and behind the logos, the slow builds needed (at least) and the priority work left at their
// pace (at least) that bring the loading screen up there.
constexpr uint32_t kLateSlowBuilds = 3;
constexpr double kLateMs = 1000.0; // priority
// auto: a cold build's cost when none was measured or recorded (the console: 104 ms on average over
// 1120 pipelines), and the recent builds behind the logos whose slow share predicts the rest.
constexpr double kColdBuildMs = 100.0;
// auto behind the logos: the sample needed and the estimate (pc_precompile_gate.h).
constexpr uint32_t kLateWindowBuilds = 16;
constexpr uint32_t kLateMinSlowBuilds = 8;
constexpr double kLateMinSlowS = 1.5;
// "[cos] precompile slow build" lines at most (a cold cache has a thousand).
constexpr uint32_t kSlowBuildLogMax = 48;

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
// auto behind the logos: the recent builds' slow share and cost.
LateGate sLateGate;
double sLateWaitS = 0;        // the warm-up's GL context wait at the previous frame
// The slow builds Aurora reported (Switch patch 0012): the last one read, those logged, and of
// those read since the previous frame, the warm-up's slow ones behind the logos and their own time.
uint64_t sSlowSeq = 0;
uint32_t sSlowLogged = 0;
uint32_t sSlowLost = 0;     // slow builds Aurora's ring dropped before they were read
uint32_t sSlowOwnTotal = 0; // of the slow builds read, those slow for their own work
uint32_t sFrameSlowBuilds = 0;
double sFrameSlowS = 0;
double sScreenMinS = 3.0;   // COS_PRECOMPILE_SCREEN_MIN_S
bool sTargetAll = false;    // auto: the loading screen (when it comes up) waits for the whole warm-up
bool sSpanish = false;      // the loading screen's language
// <cache>/precompile_state.txt (auto): the last warm-up's outcome; "" = not kept.
char sStatePath[PATH_MAX] = "";
bool sStateComplete = false; // "complete" written in this run
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
    double waitS = 0;     // of compileS, waiting for the GL context (another thread had it)
    // The builds' own time (compileS without the wait for the GL context), -1 = unknown.
    double ownS() const { return compileS < 0 ? -1 : compileS - waitS; }
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
    p.waitS = s.contextWaitNs / 1e9;
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

#if !defined(__SWITCH__)
// The bundled file's priority-0 rows (<directory of the executable>/initial_pipeline_cache.db).
uint32_t readPriorityRows() {
    char exe[PATH_MAX] = {};
    if (!executablePath(exe, sizeof(exe))) {
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

// The last warm-up's outcome (<cache>/precompile_state.txt, one line):
//   "decomp-precompile-state 1 complete DONE TOTAL SLOW_MS": every queued pipeline was built;
//   "decomp-precompile-state 1 partial DONE TOTAL SLOW_MS": the loading screen stopped (the app was
//   closed) at DONE of TOTAL; SLOW_MS is the average cost of its slow builds (0: none measured).
// Written by the loading screen of the whole warm-up (at its start, every 2 s and at its end) and
// when a warm-up finishes behind the logos. Missing or damaged reads as no record.
struct WarmupState {
    enum Kind { None, Partial, Complete } kind = None;
    uint32_t done = 0;
    uint32_t total = 0;
    double slowMs = 0;
};

WarmupState readState() {
    WarmupState st;
    if (sStatePath[0] == '\0') {
        return st;
    }
    const int fd = open(sStatePath, O_RDONLY);
    if (fd < 0) {
        return st;
    }
    char text[160] = {};
    const ssize_t n = read(fd, text, sizeof(text) - 1);
    close(fd);
    if (n <= 0) {
        return st;
    }
    char kind[16] = {};
    unsigned int version = 0;
    unsigned int done = 0;
    unsigned int total = 0;
    double slowMs = 0;
    if (sscanf(text, "decomp-precompile-state %u %15s %u %u %lf", &version, kind, &done, &total, &slowMs) != 5 ||
        version != 1 || done > total || !(slowMs >= 0.0)) {
        return st;
    }
    if (strcmp(kind, "complete") == 0) {
        st.kind = WarmupState::Complete;
    } else if (strcmp(kind, "partial") == 0) {
        st.kind = WarmupState::Partial;
    } else {
        return st;
    }
    st.done = done;
    st.total = total;
    st.slowMs = slowMs;
    return st;
}

void writeState(bool complete, uint32_t done, uint32_t total, double slowMs) {
    if (sStatePath[0] == '\0') {
        return;
    }
    char text[160];
    const int len = snprintf(text, sizeof(text), "decomp-precompile-state 1 %s %u %u %.1f\n",
                             complete ? "complete" : "partial", done, total, slowMs);
    // One short write: a torn file does not parse and reads as no record (the safe side: a screen).
    const int fd = open(sStatePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }
    if (write(fd, text, (size_t)len) != len && sLog) {
        writef(STDERR_FILENO, "[cos] precompile: could not write %s\n", sStatePath);
    }
    close(fd);
}

// The console's language (Switch), else LANG; COS_LANG overrides both. Spanish or not.
bool readUiSpanish() {
    const char* lang = getenv("COS_LANG");
    char system[16] = "";
    if (lang == nullptr || lang[0] == '\0') {
#if defined(__SWITCH__)
        cos_switch_system_language(system, sizeof(system));
        lang = system;
#else
        lang = getenv("LC_ALL");
        if (lang == nullptr || lang[0] == '\0') {
            lang = getenv("LANG");
        }
#endif
    }
    return lang != nullptr && strncmp(lang, "es", 2) == 0;
}

// "~45 s" or "~2 min 05 s".
void formatEta(char* out, size_t size, double seconds) {
    const int s = seconds < 1.0 ? 1 : (int)(seconds + 0.5);
    if (s < 60) {
        snprintf(out, size, "~%d s", s);
    } else {
        snprintf(out, size, "~%d min %02d s", s / 60, s % 60);
    }
}

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
    char compile[192] = "";
    if (p.compileS >= 0 && p.done > 0) {
        // The wait for the GL context (the render worker had it) and the builds slower than
        // COS_PRECOMPILE_SLOW_MS: for their own work (cache misses), or only for that wait.
        snprintf(compile, sizeof(compile),
                 ", compile %.1f s (%.0f ms each; %.1f s of it waiting for the GL context); %llu slow builds, "
                 "%u of them for their own work%s",
                 p.compileS, p.compileS * 1000 / p.done, p.waitS, (unsigned long long)sSlowSeq, sSlowOwnTotal,
                 sSlowLost > 0 ? " (of those read)" : "");
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

// The loading screen's panel, centred: "Preparing shaders... N/M" (the priority set) or "Preparing
// shaders (first start only): N/M, ~X s" (the whole warm-up; etaS < 0: no estimate yet), in the
// console's language, and a bar.
void drawLoadingPanel(uint32_t done, uint32_t total, bool all, double etaS) {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, display.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.0f);
    if (ImGui::Begin("##cos_precompile_loading", nullptr, kPanelFlags)) {
        ImGui::SetWindowFontScale(2.0f);
        char line[128];
        if (all) {
            char eta[32] = "";
            if (etaS >= 0) {
                eta[0] = ',';
                eta[1] = ' ';
                formatEta(eta + 2, sizeof(eta) - 2, etaS);
            }
            snprintf(line, sizeof(line),
                     sSpanish ? "Preparando shaders (solo la primera vez): %u/%u%s"
                              : "Preparing shaders (first start only): %u/%u%s",
                     done, total, eta);
        } else {
            snprintf(line, sizeof(line), sSpanish ? "Preparando shaders... %u/%u" : "Preparing shaders... %u/%u",
                     done, total);
        }
        ImGui::TextUnformatted(line);
        // As wide as the text, and at least 40% of the screen.
        float width = ImGui::CalcTextSize(line).x;
        if (width < display.x * 0.4f) {
            width = display.x * 0.4f;
        }
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

// The whole warm-up (full, or auto's screen) or the priority set (boot with priority or always).
bool targetAll() {
    return sPolicy == Policy::Full || sTargetAll;
}

LoadingTarget loadingTarget(const Progress& p) {
    LoadingTarget t;
    if (targetAll()) {
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

// After aurora_initialize (pc_aurora_init): Aurora has queued its warm-up. cacheDir: Aurora's cache
// directory, where auto keeps precompile_state.txt.
void precompileInit(const char* cacheDir) {
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
        } else if (strcmp(screen, "priority") == 0) {
            sScreen = Screen::Priority;
        } else if (strcmp(screen, "always") == 0) {
            sScreen = Screen::Always;
        } else if (strcmp(screen, "never") == 0) {
            sScreen = Screen::Never;
        } else {
            writef(STDERR_FILENO, "[cos] COS_PRECOMPILE_SCREEN=%s: expected auto, priority, always or never\n", screen);
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
#if defined(__SWITCH__)
    // Also the threshold of Aurora's slow build list, from the probe on.
    aurora_switch_set_warmup_slow_ns((uint64_t)(sSlowMs * 1e6));
#endif
    // COS_PRECOMPILE_SCREEN_MIN_S (default 3): auto shows the loading screen only when the slow work
    // left would take longer than this; less is built behind the logos.
    const char* minEnv = getenv("COS_PRECOMPILE_SCREEN_MIN_S");
    if (minEnv != nullptr && minEnv[0] != '\0') {
        sScreenMinS = strtod(minEnv, nullptr);
    }
    if (!(sScreenMinS >= 0.0)) {
        sScreenMinS = 0.0;
    }
    {
        LateGateConfig gate;
        gate.windowBuilds = kLateWindowBuilds;
        gate.minSlowBuilds = kLateMinSlowBuilds;
        gate.minSlowS = kLateMinSlowS;
        gate.screenMinS = sScreenMinS;
        sLateGate.setConfig(gate);
    }
    const char* log = getenv("COS_PRECOMPILE_LOG");
    sLog = log == nullptr || log[0] == '\0' ? logDefault : strcmp(log, "0") != 0;
    sStartNs = monotonicNs();
    sNextLogNs = sStartNs + 1000000000ull;

#if !defined(__SWITCH__)
    const AuroraStats* stats = aurora_get_stats();
    sTotal0 = stats != nullptr ? stats->queuedPipelines : 0;
    sCreated0 = stats != nullptr ? stats->createdPipelines : 0;
    if (sPolicy == Policy::Boot) {
        sPriorityRows = readPriorityRows();
    }
#endif
    const Progress p = readProgress();
    sActive = p.total > 0;
    if (sUi && sScreen == Screen::Auto && (sPolicy == Policy::Boot || sPolicy == Policy::Full) &&
        cacheDir != nullptr && cacheDir[0] != '\0') {
        snprintf(sStatePath, sizeof(sStatePath), "%s/precompile_state.txt", cacheDir);
    }
    sSpanish = sUi && pc_ui_spanish();
    if (sLog) {
        static const char* const kPolicy[] = {
            "boot: the warm-up runs until the first PLAY scene",
            "full: loading screen for every pipeline", "all", "off"};
        static const char* const kScreen[] = {
            "auto (a loading screen for the whole warm-up when its slow work would take more than "
            "COS_PRECOMPILE_SCREEN_MIN_S)",
            "priority (a loading screen for the priority set when the shader cache is cold)",
            "always", "never"};
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

bool pc_ui_spanish() {
    static const bool spanish = readUiSpanish();
    return spanish;
}

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
    return targetAll() ? "queued" : "priority";
}

// The warm-up's first builds, watched before the game starts.
struct Probe {
    uint32_t builds = 0;
    double wallS = 0;
    double eachMs = 0;       // the compile thread's time per build (Switch), else the wall time
    bool compileKnown = false;
    bool finished = false;   // the loading screen's target is already built
    bool fast = false;       // no slower than COS_PRECOMPILE_SLOW_MS each
    char measured[128] = "";
};

// COS_PRECOMPILE_SCREEN=auto or priority, before the game starts: watches the warm-up's first builds
// (up to kProbeBuilds, at most kProbeMs; nothing is presented meanwhile, events are pumped).
Probe probe() {
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
    Probe m;
    m.wallS = (monotonicNs() - startNs) / 1e9;
    m.builds = p.done - p0.done;
    // The compile thread's time per build where Aurora reports it (Switch), else the wall time per
    // build (the warm-up runs back to back meanwhile).
    m.compileKnown = p.compileS >= 0 && p0.compileS >= 0;
    m.eachMs = m.builds == 0 ? m.wallS * 1000.0
                             : (m.compileKnown ? (p.ownS() - p0.ownS()) : m.wallS) * 1000.0 / m.builds;
    m.finished = t.finished;
    m.fast = t.finished || (m.builds > 0 && m.eachMs <= sSlowMs);
    if (m.builds == 0) {
        snprintf(m.measured, sizeof(m.measured), "no build finished in %.2f s", m.wallS);
    } else {
        snprintf(m.measured, sizeof(m.measured), "%u builds in %.2f s, %.1f ms each%s", m.builds, m.wallS,
                 m.eachMs, m.compileKnown ? "" : " (wall time)");
    }
    return m;
}

// COS_PRECOMPILE_SCREEN=priority: whether the first builds were fast, i.e. no loading screen is
// needed for the priority set. Logs the decision.
bool probeFast() {
    const Probe m = probe();
    const LoadingTarget t = loadingTarget(readProgress());
    if (sLog) {
        if (m.finished) {
            writef(STDERR_FILENO,
                   "[cos] precompile screen priority: %s: all %u %s pipelines already built, no loading screen; "
                   "the game starts\n",
                   m.measured, t.total, targetName());
        } else if (m.fast) {
            writef(STDERR_FILENO,
                   "[cos] precompile screen priority: %s (COS_PRECOMPILE_SLOW_MS=%.0f): shaders cached, no "
                   "loading screen; the game starts and the %u %s pipelines left build first behind the logos\n",
                   m.measured, sSlowMs, t.total - t.done, targetName());
        } else {
            writef(STDERR_FILENO,
                   "[cos] precompile screen priority: %s (COS_PRECOMPILE_SLOW_MS=%.0f): shader cache cold, "
                   "loading screen for the %u %s pipelines left\n",
                   m.measured, sSlowMs, t.total - t.done, targetName());
        }
    }
    return m.fast;
}

// COS_PRECOMPILE_SCREEN=auto, before the game starts: the probe and the last warm-up's record give
// the slow work left; the loading screen (for the whole warm-up) comes up if it would take longer
// than COS_PRECOMPILE_SCREEN_MIN_S. Returns whether it should, and in *estMs the expected time per
// build left (the loading screen's first estimate). Logs the decision in one line.
bool autoWantsScreen(double* estMs) {
    const Probe m = probe();
    const Progress p = readProgress();
    const LoadingTarget t = loadingTarget(p);
    *estMs = kColdBuildMs;
    if (m.finished) {
        if (sLog) {
            writef(STDERR_FILENO,
                   "[cos] precompile screen auto: %s: all %u queued pipelines already built, no loading screen; "
                   "the game starts\n",
                   m.measured, t.total);
        }
        return false;
    }
    const uint32_t left = t.total - t.done;
    const WarmupState st = readState();
    uint32_t slowLeft = left; // the pipelines left that are expected to be slow, and their cost
    double slowMs = m.fast ? kColdBuildMs : m.eachMs;
    char why[192];
    if (!m.fast) {
        snprintf(why, sizeof(why), "shader cache cold");
    } else if (st.kind == WarmupState::Complete) {
        slowLeft = 0;
        snprintf(why, sizeof(why), "shaders cached and the last warm-up was complete (%u pipelines)", st.total);
    } else if (st.kind == WarmupState::Partial) {
        // The first st.done of the queue were built then (Aurora queues them in the same order), the
        // rest were not.
        slowLeft = t.total > st.done ? t.total - st.done : 0;
        if (slowLeft > left) {
            slowLeft = left;
        }
        if (st.slowMs > 0) {
            slowMs = st.slowMs;
        }
        snprintf(why, sizeof(why),
                 "shaders cached so far, but the last warm-up stopped at %u/%u, so the %u pipelines past it are "
                 "taken as uncached at %.0f ms each%s",
                 st.done, st.total, slowLeft, slowMs, st.slowMs > 0 ? " (recorded)" : "");
    } else {
        snprintf(why, sizeof(why),
                 "shaders cached so far, but no record of a complete warm-up (%s), so all %u left are taken as "
                 "uncached at %.0f ms each",
                 sStatePath[0] != '\0' ? "precompile_state.txt" : "no cache directory", left, slowMs);
    }
    const double fastMs = m.fast && m.builds > 0 ? m.eachMs : slowMs;
    const double slowS = slowLeft * slowMs / 1000.0;
    const double totalS = slowS + (left - slowLeft) * fastMs / 1000.0;
    const bool screen = slowS > sScreenMinS;
    *estMs = left > 0 ? totalS * 1000.0 / left : kColdBuildMs;
    if (sLog) {
        if (screen) {
            writef(STDERR_FILENO,
                   "[cos] precompile screen auto: %s (COS_PRECOMPILE_SLOW_MS=%.0f): %s; slow work about %.1f s "
                   "(> COS_PRECOMPILE_SCREEN_MIN_S=%.1f): loading screen until the whole warm-up is built, %u of "
                   "%u pipelines left, about %.0f s\n",
                   m.measured, sSlowMs, why, slowS, sScreenMinS, left, t.total, totalS);
        } else {
            writef(STDERR_FILENO,
                   "[cos] precompile screen auto: %s (COS_PRECOMPILE_SLOW_MS=%.0f): %s; slow work about %.1f s "
                   "(<= COS_PRECOMPILE_SCREEN_MIN_S=%.1f): no loading screen; the game starts and the %u of %u "
                   "pipelines left (%u priority) build behind the logos\n",
                   m.measured, sSlowMs, why, slowS, sScreenMinS, left, t.total,
                   p.priorityTotal - p.priorityDone);
        }
    }
    return screen;
}

// Presents the loading screen (at most COS_PRECOMPILE_SCREEN_FPS frames a second) and pumps events
// until its pipelines are built. For the whole warm-up it shows the time left at the pace of the
// last few seconds (once at least 3 builds over 0.5 s give one; the log uses estMs per build until
// then) and keeps precompile_state.txt current.
void runLoadingScreen(double estMs) {
    Progress p = readProgress();
    LoadingTarget t = loadingTarget(p);
    const bool all = targetAll();
    const uint64_t startNs = monotonicNs();
    uint64_t nextLogNs = startNs + 1000000000ull;
    uint64_t nextStateNs = startNs + 2000000000ull;
    uint64_t nextEtaNs = startNs;
    unsigned int presented = 0;
    const uint64_t periodNs = loadingScreenPeriodNs();
    // The pace: (time, done) once per loop, the last kPaceSamples; measured over up to kPaceNs.
    constexpr int kPaceSamples = 128;
    constexpr uint64_t kPaceNs = 5000000000ull;
    uint64_t paceNs[kPaceSamples];
    uint32_t paceDone[kPaceSamples];
    int paceCount = 0;
    int pacePos = 0;
    double msEach = estMs; // the log's estimate until the pace is measured
    bool paced = false;
    double etaS = -1;
    // The slow builds seen (per loop: compile time or wall time per build over COS_PRECOMPILE_SLOW_MS),
    // for the record's cost of a slow build.
    uint32_t slowBuilds = 0;
    double slowS = 0;
    uint32_t prevDone = p.done;
    double prevCompileS = p.ownS();
    uint64_t prevNs = startNs;
    const uint32_t startDone = p.done;
    auto recordedSlowMs = [&]() { return slowBuilds >= 4 ? slowS * 1000.0 / slowBuilds : 0.0; };
    if (sLog) {
        writef(STDERR_FILENO, "[cos] precompile loading screen: waiting for %u %s pipelines, %.0f frames/s "
                              "at most (COS_PRECOMPILE_SCREEN_FPS)%s\n",
               t.total - t.done, targetName(), 1e9 / (double)periodNs,
               all ? ", builds back to back" : "");
    }
    if (all) {
        writeState(false, p.done, p.total, 0.0);
    }
    for (;;) {
        const uint64_t frameStartNs = monotonicNs();
        watchdogPulse();
        loadingEvents();
        p = readProgress();
        t = loadingTarget(p);
        const uint64_t now0 = monotonicNs();
        if (p.done > prevDone) {
            const uint32_t builds = p.done - prevDone;
            const bool compileKnown = p.compileS >= 0 && prevCompileS >= 0;
            const double spentS = compileKnown ? p.ownS() - prevCompileS : (now0 - prevNs) / 1e9;
            if (spentS * 1000.0 / builds > sSlowMs) {
                slowBuilds += builds;
                slowS += spentS;
            }
            prevDone = p.done;
            prevCompileS = p.ownS();
            prevNs = now0;
        }
        if (t.finished) {
            break;
        }
        if (all) {
            paceNs[pacePos] = now0;
            paceDone[pacePos] = p.done;
            pacePos = (pacePos + 1) % kPaceSamples;
            if (paceCount < kPaceSamples) {
                paceCount++;
            }
            // The oldest sample within kPaceNs.
            int oldest = (pacePos - paceCount + kPaceSamples) % kPaceSamples;
            for (int i = 0; i < paceCount; i++) {
                const int k = (pacePos - paceCount + i + kPaceSamples) % kPaceSamples;
                if (now0 - paceNs[k] <= kPaceNs) {
                    oldest = k;
                    break;
                }
            }
            const uint32_t built = p.done - paceDone[oldest];
            if (built >= 3 && now0 - paceNs[oldest] >= 500000000ull) {
                msEach = (now0 - paceNs[oldest]) / 1e6 / built;
                paced = true;
            }
            // The figure on screen: once the pace is measured, changed once a second.
            if (paced && now0 >= nextEtaNs) {
                nextEtaNs = now0 + 1000000000ull;
                etaS = (t.total - t.done) * msEach / 1000.0;
            }
            if (now0 >= nextStateNs) {
                nextStateNs = now0 + 2000000000ull;
                writeState(false, p.done, p.total, recordedSlowMs());
            }
        }
        if (aurora_begin_frame()) {
            drawLoadingPanel(t.done, t.total, all, etaS);
            aurora_end_frame();
            presented++;
        }
        const uint64_t now = monotonicNs();
        if (sLog && now >= nextLogNs) {
            nextLogNs = now + 1000000000ull;
            char eta[64] = "";
            if (all) {
                snprintf(eta, sizeof(eta), ", about %.0f s left at %.0f ms each", (t.total - t.done) * msEach / 1000.0,
                         msEach);
            }
            writef(STDERR_FILENO, "[cos] precompile loading screen %u/%u, %.1f s, %u frames presented%s\n", t.done,
                   t.total, (now - startNs) / 1e9, presented, eta);
        }
        // At most COS_PRECOMPILE_SCREEN_FPS frames a second (default 10): each present needs the
        // render worker and, on the Switch, the one GL context the compile thread builds with.
        const uint64_t frameNs = monotonicNs() - frameStartNs;
        if (frameNs < periodNs) {
            usleep((useconds_t)((periodNs - frameNs) / 1000));
        }
    }
    const uint64_t endNs = monotonicNs();
    if (all) {
        writeState(true, p.done, p.total, recordedSlowMs());
        sStateComplete = sStatePath[0] != '\0';
    }
    if (sLog) {
        char slow[96] = "";
        if (all) {
            snprintf(slow, sizeof(slow), " (%u slow builds, %.0f ms each)", slowBuilds,
                     slowBuilds > 0 ? slowS * 1000.0 / slowBuilds : 0.0);
        }
        writef(STDERR_FILENO,
               "[cos] precompile loading screen done: %u/%u %s pipelines in %.1f s, %u built%s, %u frames "
               "presented; %u/%u of the warm-up built, the game %s\n",
               t.done, t.total, targetName(), (endNs - startNs) / 1e9, p.done - startDone, slow, presented,
               p.done, p.total, pc_frame_count() == 0 ? "starts" : "goes on");
    }
}

// Before the game starts, with COS_PRECOMPILE=boot or full: the loading screen, unless
// COS_PRECOMPILE_SCREEN says otherwise (auto: when the slow work left is long enough; priority: when
// the first builds are slow).
void loadingScreen() {
    if (!sActive || !sUi || (sPolicy != Policy::Boot && sPolicy != Policy::Full)) {
        return;
    }
    sTargetAll = sScreen == Screen::Auto;
    const Progress p = readProgress();
    const LoadingTarget t = loadingTarget(p);
    if (sPolicy == Policy::Boot && !targetAll() && t.total == 0) {
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
    double estMs = kColdBuildMs;
    const bool show = sScreen == Screen::Auto       ? autoWantsScreen(&estMs)
                      : sScreen == Screen::Priority ? !probeFast()
                                                    : true;
    if (!show) {
        sBehindLogos = true;
        const Progress now = readProgress();
        sLateDone = now.done;
        sLateCompileS = now.compileS;
        sLateWaitS = now.waitS;
        return;
    }
    if (sScreen == Screen::Always && sLog) {
        writef(STDERR_FILENO, "[cos] precompile screen always (COS_PRECOMPILE_SCREEN): loading screen for the "
                              "%u %s pipelines left\n",
               t.total - t.done, targetName());
    }
    runLoadingScreen(estMs);
}

// Every game frame while the warm-up runs (Switch patch 0012): logs the builds Aurora found slower
// than COS_PRECOMPILE_SLOW_MS since the last call - which pipeline, and whether the time went to
// its own shader work or to waiting for the GL context - and counts, for lateScreenCheck, the
// warm-up's builds behind the logos that were slow for their own work.
void drainSlowBuilds() {
    sFrameSlowBuilds = 0;
    sFrameSlowS = 0;
#if defined(__SWITCH__)
    AuroraSwitchSlowBuild builds[32];
    const uint64_t prevSeq = sSlowSeq;
    const uint32_t n = aurora_switch_get_slow_builds(builds, 32, &sSlowSeq);
    if (n > 0 && builds[0].seq > prevSeq + 1) {
        sSlowLost += (uint32_t)(builds[0].seq - prevSeq - 1); // more than Aurora's ring keeps
    }
    for (uint32_t i = 0; i < n; i++) {
        const AuroraSwitchSlowBuild& b = builds[i];
        const double ownMs = (b.ns - (b.waitNs < b.ns ? b.waitNs : b.ns)) / 1e6;
        const bool slowOwn = ownMs > sSlowMs;
        if (slowOwn) {
            sSlowOwnTotal++;
        }
        // This frame's warm-up builds behind the logos (lateScreenCheck's sLateDone: the done count
        // at the previous frame) that were slow for their own work.
        if (b.warmup && slowOwn && sBehindLogos && b.warmupDone > sLateDone) {
            sFrameSlowBuilds++;
            sFrameSlowS += ownMs / 1000.0;
        }
        if (!sLog) {
            continue;
        }
        if (sSlowLogged >= kSlowBuildLogMax) {
            if (sSlowLogged++ == kSlowBuildLogMax) {
                writef(STDERR_FILENO, "[cos] precompile slow build: more than %u; the rest are not listed\n",
                       kSlowBuildLogMax);
            }
            continue;
        }
        sSlowLogged++;
        writef(STDERR_FILENO,
               "[cos] precompile slow build %llu: pipeline %016llx (%s, %u/%u built), %.1f ms: tint %.1f, waited "
               "for the GL context %.1f, held it %.1f ms (%s); frame %u\n",
               (unsigned long long)b.seq, (unsigned long long)b.hash, b.warmup ? "warm-up" : "for a draw",
               b.warmupDone, readProgress().total, b.ns / 1e6, b.translateNs / 1e6, b.waitNs / 1e6,
               b.heldNs / 1e6,
               slowOwn ? "its own work: a shader cache miss or a slow cache read"
                       : "slow only for the wait: the render worker had the context, not a cache miss",
               pc_frame_count());
    }
#endif
}

// With the game started behind no loading screen (auto or priority), every frame while the logos
// run: whether the builds turned slow (a partly warm shader cache) enough to bring the loading
// screen up there. auto: LateGate (pc_precompile_gate.h) over the builds slow for their own work
// (drainSlowBuilds): a sample big enough (kLateMinSlowBuilds of the last kLateWindowBuilds, or
// kLateMinSlowS measured) and the slow work left at the recent slow share and cost over
// COS_PRECOMPILE_SCREEN_MIN_S; priority: the priority set left at the slow builds' pace (own time,
// the GL context wait left out) would take more than kLateMs. Needs the compile thread's time
// (Switch); once past the logo scene, or with the target built, it stops looking.
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
    const double compileS = (p.compileS - sLateCompileS) - (p.waitS - sLateWaitS);
    sLateDone = p.done;
    sLateCompileS = p.compileS;
    sLateWaitS = p.waitS;
    if (builds == 0) {
        return;
    }
    double leftMs = 0;
    char detail[256];
    if (sScreen == Screen::Auto) {
        const LateGateVerdict v = sLateGate.add(builds, sFrameSlowBuilds, sFrameSlowS, t.total - t.done);
        if (!v.screen) {
            return;
        }
        leftMs = v.leftMs;
        snprintf(detail, sizeof(detail),
                 "%u of the last %u builds slow, %.0f ms each; %.1f s of slow work measured; %u queued pipelines "
                 "left, %.0f%% of them slow at that share: about %.1f s (> COS_PRECOMPILE_SCREEN_MIN_S=%.1f)",
                 v.windowSlow, v.windowBuilds, v.slowEachMs, v.measuredSlowS, t.total - t.done,
                 v.slowShare * 100.0, leftMs / 1000.0, sScreenMinS);
    } else {
        if (!(compileS * 1000.0 / builds > sSlowMs)) {
            return;
        }
        sSlowBuilds += builds;
        sSlowS += compileS;
        const double eachMs = sSlowS * 1000.0 / sSlowBuilds;
        leftMs = (t.total - t.done) * eachMs;
        if (sSlowBuilds < kLateSlowBuilds || leftMs < kLateMs) {
            return;
        }
        snprintf(detail, sizeof(detail), "%u slow builds, %.0f ms each; %u %s pipelines left, about %.1f s at that pace",
                 sSlowBuilds, eachMs, t.total - t.done, targetName(), leftMs / 1000.0);
    }
    sLateChecked = true;
    if (sLog) {
        writef(STDERR_FILENO,
               "[cos] precompile screen %s: builds turned slow behind the logos (%s): loading screen until %s\n",
               sScreen == Screen::Auto ? "auto" : "priority", detail,
               targetAll() ? "the whole warm-up is built" : "they are built");
    }
    // Back to back meanwhile (the throttle would hold slow builds back for the frames' sake).
    setThrottle(0.0f);
    {
        // Aurora's frame work allocates host memory, not the game's current heap (JKRHeap.cpp).
        JKRPcHostAllocScope hostAlloc;
        runLoadingScreen(leftMs / (t.total - t.done));
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
    drainSlowBuilds();
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
        // auto: the next start knows every queued pipeline was built (behind the logos this time).
        if (!sStateComplete && sStatePath[0] != '\0') {
            writeState(true, p.done, p.total, 0.0);
            sStateComplete = true;
        }
        return;
    }
    if ((sScreen == Screen::Auto || sScreen == Screen::Priority) && sUi) {
        lateScreenCheck(p);
    }
    if (sLog && now >= sNextLogNs) {
        sNextLogNs = now + 1000000000ull;
        logLine("", p, frames, now);
    }
}

} // namespace pc
