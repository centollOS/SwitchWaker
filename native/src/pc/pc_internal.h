// Shared state of the run harness (native/src/pc/pc_*.cpp, docs/NATIVE_PORT_PHASE4_6.md step 6.0).
// Public API: native/include/pc/pc_harness.h.
#pragma once

#include "pc/pc_harness.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class JUTResFont;

namespace pc {

struct Config {
    const char* disc = nullptr;      // COS_DISC
    const char* smoke = nullptr;     // COS_SMOKE
    const char* milestone = nullptr; // COS_MILESTONE
    const char* trace = nullptr;     // COS_TRACE
    const char* runDir = nullptr;    // COS_RUN_DIR
    const char* input = nullptr;     // COS_INPUT
    const char* bootStage = nullptr; // COS_BOOT_STAGE (parsed by loadBootStage)
    double timeoutS = 0;             // COS_TIMEOUT_S, 0 = off
    double stallS = 0;               // COS_STALL_S, 0 = off
    unsigned int frames = 0;         // COS_FRAMES, 0 = off
    bool uncapped = false;           // COS_UNCAPPED
    unsigned int perfEvery = 0;      // COS_PERF_EVERY: game-thread frame times every N frames, 0 = off
    const char* perfPath = nullptr;  // COS_PERF: CSV of per-frame game-thread times (step 6.7)
    bool audio = true;               // COS_AUDIO (off/0 -> false)
    // COS_SYNC_PIPELINES (render audit A3): Aurora compiles a draw's pipeline before drawing it
    // (AuroraConfig::blockingPipelines, Aurora patch 0005) instead of skipping the draw until its
    // pipeline is ready. Default: on when COS_SHOT / COS_SHOT_EVERY capture frames, off otherwise.
    bool syncPipelines = false;
    // COS_HITCH_MS=<n>: one "[cos] hitch" line for every game frame whose busy time (the frame
    // minus the pace wait) exceeds n ms, with its split and what else happened in it (pipelines
    // built, texture bytes uploaded, resources loaded, scene created). 0 = off (the default; the
    // Switch build sets 50).
    unsigned int hitchMs = 0;
    // COS_HEAP_CHECK=<n> (bug B4): every n game frames, check() every JKR heap of the tree from the
    // root (block signatures, list links, sizes); the first failure names the heap and ends the
    // run as a check failure. 0 = off.
    unsigned int heapCheckEvery = 0;
    unsigned int heapReportEvery = 0; // COS_HEAP_REPORT (bug B8)
    int allocFailuresMax = -1;        // COS_ALLOC_FAILURES_MAX (bug B13); -1: no limit
    // COS_FPS_OVERLAY: a frame-rate panel drawn with Aurora's ImGui (pc_overlay.cpp). Off by default;
    // the Switch build sets 1.
    bool fpsOverlay = false;
    // COS_FPS_OVERLAY_DETAIL=compact: the panel shows the frame rate and the game thread only
    // (full, the default: every line). The options menu changes both at run time.
    bool fpsOverlayCompact = false;
    // COS_PERF_LOG=0: no "[cos] perf" / "[cos] perf-switch" lines (the windows are still measured,
    // COS_PERF's CSV and the hitch lines are unchanged). Default 1.
    bool perfLog = true;
};

extern Config gConfig;

// Blocks the calling thread for good (pause(); Horizon has none, so a sleep loop there).
[[noreturn]] void waitForever();

// Milliseconds since pc_harness_init (monotonic).
uint64_t elapsedMs();
uint64_t monotonicNs();

// The running executable's path (the Mac's _NSGetExecutablePath, Linux's /proc/self/exe; not
// resolved through symlinks). False, with out empty, where the host has no such query (the
// Switch) or it fails; callers then fall back to argv[0] or the current directory.
bool executablePath(char* out, size_t size);

// pc_milestone.cpp
bool isKnownMilestone(const char* name);
void printMilestones(int fd);

// pc_crash.cpp
void installCrashHandler();
// Writes "scene=... frame=... retrace=... ms=... last_res=..." (one line, no prefix) to fd.
void writeState(int fd);
// The scene process the game created last (fpcNm_* procName, -1 before the first) and its name;
// the number of resources the game has loaded (pc_trace_resource) and the last one's path.
int traceScene();
const char* traceSceneName(int procName);
unsigned int traceResourceCount();
void traceLastResource(char* out, size_t size);
// Formats into a fixed buffer and writes to fd; usable from the crash handler.
void writef(int fd, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
// Opens <COS_RUN_DIR>/<name> for writing (truncated), or -1 without a run directory.
int openRunFile(const char* name);
// Every thread but the caller: suspended, then a frame-pointer backtrace of each (stall report).
void dumpAllThreads(int fd);

// pc_disc.cpp: 0 if COS_DISC is a readable GZLE01 revision 0 image, else prints why and returns
// PC_EXIT_DISC.
int checkDisc();

// pc_smoke.cpp: runs COS_SMOKE if it is a test that runs before the SDK (it never returns then);
// exits PC_EXIT_USAGE for an unknown name; returns for no COS_SMOKE.
void runEarlySmoke();
// pc_smoke.cpp: runs COS_SMOKE if it is a test that runs right after the disc check (disc-ls); it
// never returns then.
void runDiscSmoke();
// pc_smoke.cpp: runs COS_SMOKE if it is a test that runs right after the Aurora bring-up (heap);
// it never returns then.
void runAuroraSmoke();
// pc_smoke.cpp: runs COS_SMOKE if it is a test that runs once mDoMch_Create made the heaps (font,
// arc-sweep, msg-sweep, jpa-sweep, stage-sweep, blo-sweep, save, dzb-sweep, audio-parse,
// j3d-sweep, anm-sweep, amp-sweep, stb-sweep; from pc_heaps_created); it never returns then.
void runHeapsSmoke();
// pc_heap.cpp: COS_HEAP_CHECK, from pc_frame_end.
void heapCheckFrame(unsigned int frame);
// pc_heap.cpp: one line with the free total and the largest free block of the game's heaps (root,
// system, main, game, archive, command, the PLAY scene's 2D heap); COS_HEAP_REPORT=N logs it
// every N frames, the options menu when it opens. `why` names the caller.
void heapReport(const char* why);
// pc_heap.cpp: JKR allocations that failed since the start (each one is logged, bug B8).
unsigned int heapAllocFailures();
// pc_heap.cpp: COS_SMOKE=heap.
[[noreturn]] void smokeHeap();
// pc_font.cpp: COS_SMOKE=font.
[[noreturn]] void smokeFont();
// pc_font.cpp: checks a JUTResFont made from the BFN bytes (length bytes at `bytes`, the font's
// file `path` on the disc) against an independent reading of those bytes: block counts, INF1, and
// getFontCode/getWidthEntry/loadImage of every code its MAP1 blocks cover (one past each end too).
// With reportFd >= 0 it writes the font's FONT/INF1/WID1/MAP1/GLY1 lines under `path`
// (disc_manifest.py --check-font/--check-msg syntax). Logs "[cos] <test>: ..."; returns the number
// of errors found.
int checkResFont(const char* test, const char* path, JUTResFont& font, const uint8_t* bytes,
                 uint32_t length, int reportFd);
// pc_msg.cpp: COS_SMOKE=msg-sweep.
[[noreturn]] void smokeMsgSweep();
// pc_jpa.cpp: COS_SMOKE=jpa-sweep.
[[noreturn]] void smokeJpaSweep();
// pc_stage.cpp: COS_SMOKE=stage-sweep.
[[noreturn]] void smokeStageSweep();
// pc_blo.cpp: COS_SMOKE=blo-sweep.
[[noreturn]] void smokeBloSweep();
// pc_save.cpp: COS_SMOKE=save. prepareSaveSmoke (from pc_aurora_init, before CARDInit) moves
// the card of slot A into <COS_RUN_DIR>/card; smokeSave runs from pc_heaps_created.
void prepareSaveSmoke();
[[noreturn]] void smokeSave();
// pc_save.cpp: points the card of slot A at the empty folder <COS_RUN_DIR>/card/ (before
// CARDInit), so a run starts from a clean card and never touches the user's (`who` names the
// caller in the log; exit PC_EXIT_USAGE without COS_RUN_DIR). runCardGciPath: the game's save file
// in that folder, nullptr when prepareRunCard was not called.
void prepareRunCard(const char* who);
const char* runCardGciPath();
// pc_dzb.cpp: COS_SMOKE=dzb-sweep.
[[noreturn]] void smokeDzbSweep();
// pc_audio.cpp: COS_SMOKE=audio-parse.
[[noreturn]] void smokeAudioParse();
// pc_j3d.cpp: COS_SMOKE=j3d-sweep.
[[noreturn]] void smokeJ3dSweep();
// pc_anm.cpp: COS_SMOKE=anm-sweep.
[[noreturn]] void smokeAnmSweep();
// pc_amp.cpp: COS_SMOKE=amp-sweep.
[[noreturn]] void smokeAmpSweep();
// pc_stb.cpp: COS_SMOKE=stb-sweep.
[[noreturn]] void smokeStbSweep();
// pc_blur.cpp: COS_SMOKE=blur-pos (bug B3, the sword blur positions).
[[noreturn]] void smokeBlurPos();
// pc_arc.cpp: COS_SMOKE=arc-sweep.
[[noreturn]] void smokeArcSweep();
bool isKnownSmoke(const char* name);
void printSmokes(int fd);

// pc_input.cpp (step 6.3): reads the COS_INPUT script (exit PC_EXIT_USAGE if it is malformed, or
// if COS_SMOKE=pad-echo has none).
void loadInput();
// pc_input.cpp: the COS_INPUT script's state for game frame `frame` (GameCube PAD_* bits and the
// raw main stick); false without a script. The options menu reads it (the game's pad reads are
// blocked while the menu is open).
bool inputScriptAt(unsigned int frame, uint16_t* buttons, int8_t* stickX, int8_t* stickY);

// pc_watchdog.cpp
void startWatchdog();
// Counts as progress for COS_STALL_S while the game's frame counter cannot move (the shader
// loading screen before the game starts). Any thread.
void watchdogPulse();

// pc_overlay.cpp: COS_FPS_OVERLAY's panel, drawn into this frame's ImGui frame (call between
// aurora_begin_frame and aurora_end_frame). busyNs: this frame's game-thread busy time so far.
void overlayFrame(uint64_t busyNs);

// pc_frame.cpp: milestone M6 logo-res, once pc_logo_res_synced reported every resource and the
// logo scene made its scene request (`how` says which: dComIfG_changeOpeningScene, or the
// COS_BOOT_STAGE request of step 6.4). Logged once.
void logoResDone(const char* how);

// pc_title_stage.cpp: milestone M8 title-stage. pc_stage_created arms it with sea_T's start room
// (M7); titleStageFrame (pc_frame_end, every game frame) waits until that room is loaded, its BG
// collision registered and its actors created, then reports title-stage 300 frames later.
void titleStageArm(int roomNo);
void titleStageFrame(unsigned int frames);
// The room checks behind M8, shared with M12: nullptr once room roomNo's ROOM_SCENE executes,
// Room<n> holds room.dzr, its dStage_roomDt_c is set, its BG collision is registered and its
// actors are created (*created, when not null, gets their count); otherwise the first unmet
// condition.
const char* stageRoomReady(int roomNo, int* created);

// pc_outset.cpp: milestone M12 outset-debug. pc_stage_created arms it with the COS_BOOT_STAGE
// stage's start room; outsetFrame (pc_frame_end, every game frame) waits until the PLAY scene
// executes with that stage, the room is up and the player actor finished creating, then reports
// outset-debug 300 frames later. It then measures M13 outset-control: a 120-frame hold of pad 0's
// main stick that moves the player more than 300 units, and 3,600 frames since he was in the room.
void outsetArm(const char* stageName, int roomNo);
void outsetFrame(unsigned int frames);
// pc_outset.cpp: the M12 probe found the player in the COS_BOOT_STAGE start room (PLAY scene executing,
// room up, player actor created).
bool outsetLinkReady();

// pc_actor_sweep.cpp (step 6.9): COS_SMOKE=actor-sweep; actorSweepFrame runs from pc_frame_end
// every game frame and, once outsetLinkReady, spawns every actor profile next to the player in turn,
// runs it 30 frames and deletes it, then exits 0.
void actorSweepFrame(unsigned int frames);
// pc_fx_sweep.cpp: COS_SMOKE=fx-sweep; fxSweepFrame runs from pc_frame_end every game frame and,
// once outsetLinkReady, creates every loaded particle emitter (common and scene, COS_FX_SWEEP) in
// front of the player a few at a time, in the Normal and Toon groups, so their pipelines are
// recorded (native/tools/gen_pipeline_cache.sh), then exits 0.
void fxSweepFrame(unsigned int frames);
// pc_res_sweep.cpp: COS_SMOKE=res-sweep; resSweepFrame runs from pc_frame_end every game frame and,
// once outsetLinkReady, takes every archive of the disc in turn and draws its J3D models (lit as an
// actor, then as a room) and its BLO screens for a frame each from the PLAY scene's draw
// (pc_play_draw), so their pipelines are recorded; exits 0 when done.
void resSweepFrame(unsigned int frames);
// pc_bgm_hop.cpp (bug B1): COS_SMOKE=bgm-hop; bgmHopFrame runs from pc_frame_end every game
// frame and, once the player is in the COS_BOOT_STAGE island room, measures the island BGM, goes to a
// house and back and checks the BGM came back.
void bgmHopFrame(unsigned int frames);
// pc_telescope_demo.cpp (bug B6): COS_SMOKE=telescope-demo; telescopeDemoFrame runs from pc_frame_end every game
// frame (and telescopeDemoFrameEnd right after aurora_end_frame): through the telescope demo's
// cinemascope part, the picture must reach the left and right edges.
void telescopeDemoFrame(unsigned int frames);
void telescopeDemoFrameEnd(unsigned int frame);

// pc_shore.cpp (bug B7): the fixed debug camera of COS_CAMERA / COS_SMOKE=shore-foam
// (pc_camera_override applies it from then on).
void setFixedCamera(const float eye[3], const float center[3]);
// pc_shore.cpp (bug B7): COS_SMOKE=shore-foam; shoreFoamFrame runs from pc_frame_end every game
// frame and, once the player is in Outset, moves a fixed camera along the cliffs under the rope bridge
// and measures how much of the shore foam changes from frame to frame.
void shoreFoamFrame(unsigned int frames);
// pc_npc_variants.cpp (bug B9): COS_SMOKE=npc-variants; runs from pc_frame_end every game frame
// and checks that two Windfall women sharing one model keep their own clothes across a reload.
void npcVariantsFrame(unsigned int frames);

// pc_title.cpp: milestone M9 title (see pc_title_drawn); titleFrame runs from pc_frame_end every
// game frame. titleReached: the milestone was logged.
void titleFrame(unsigned int frames);
bool titleReached();

// pc_title_audio.cpp (step 5.5): COS_SMOKE=title-audio; titleAudioFrame runs from pc_frame_end
// every game frame and, once the title is reached, measures the audio output level and the
// sequence ticks over the next 300 game frames, then exits.
void titleAudioFrame(unsigned int frames);

// pc_file_select.cpp: milestone M10 file-select (see pc_name_scene_drawn); fileSelectFrame runs
// from pc_frame_end every game frame.
void fileSelectFrame(unsigned int frames);

// pc_new_game.cpp: milestones M11 new-game and M14 outset-real, the real new-game flow from a
// clean card. newGameNameScene gets the name scene's procedures (from pc_name_scene_drawn);
// newGameFrame runs from pc_frame_end every game frame. newGameNeedsCleanCard: COS_MILESTONE is
// one of the two (pc_aurora_init then calls prepareRunCard).
void newGameNameScene(int mainProc, int memCardCheckProc, int drawProc);
void newGameFrame(unsigned int frames);
bool newGameNeedsCleanCard();

// pc_boot.cpp (step 6.4): parses COS_BOOT_STAGE into gBootStage (exit PC_EXIT_USAGE if it is
// malformed).
void loadBootStage();
// Debug story presets (pc_preset.cpp): COS_BOOT_PRESET (checked; nullptr when unset) and the
// sailing preset's state, put on the file in memory (inPlay: the PLAY scene runs, so the button
// items shown are updated too). Its spawn is kSailingSpawn: on the boat, Windfall's north-west corner.
struct PresetSpawn {
    const char* stage;
    int room;
    int point;
    const char* spec; // as COS_BOOT_STAGE
};
constexpr PresetSpawn kSailingSpawn = {"sea", 11, 102, "sea:11:102"};
const char* bootPreset();
void applySailingPreset(bool inPlay);
// A smoke test's own controller state for port 0 (pc_input.cpp; COS_SMOKE=sailing); on wins over a
// COS_INPUT script.
void setDrivenPad(bool on, uint16_t buttons, int8_t stickX, int8_t stickY);
void sailingFrame(unsigned int frames);
// COS_SMOKE=rope (pc_rope.cpp, bug B19): the hanging ropes of Tetra's ship.
void ropeFrame(unsigned int frames);
// COS_SMOKE=evcam (pc_evcam.cpp, bug B20): the event camera's pointer arguments.
void evcamFrame(unsigned int frames);
// COS_SMOKE=wind-screen (pc_wind_screen.cpp): the wind song's wind-direction screen.
void windScreenFrame(unsigned int frames);

// pc_shot.cpp: parses COS_SHOT / COS_SHOT_EVERY (exit PC_EXIT_USAGE if malformed); without
// them the screenshots stay off. Returns whether any frame will be captured.
bool loadShots();
// pc_shot.cpp: after aurora_end_frame of game frame `frame` (pc_frame_count numbering): saves the
// presented image as shot-<frame>.png if COS_SHOT or COS_SHOT_EVERY names that frame.
void shotFrameEnd(unsigned int frame);
// pc_shot.cpp (bug B6): reads back the frame Aurora just presented (call it right after
// aurora_end_frame, as shotFrameEnd is) and hands check its 8-bit RGB rows on the render worker;
// returns once check ran (or the readback failed, logged, and check did not run).
void shotProbe(unsigned int frame,
               std::function<void(const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height)> check);
// pc_shot.cpp: reads back the presented image of game frame `frame` (call right after its
// aurora_end_frame, e.g. from pc_frame_end's per-frame hooks) and hands it to sink as 8-bit RGB
// rows, on the render worker (where large allocations come from the host heap, not the game's
// JKR heaps), then waits for the worker. False (logged) when the readback failed; the sink is
// not called then. saveFramePng (render worker too) writes such an image as shot-<frame>.png
// into COS_SHOT_DIR / COS_RUN_DIR / the current directory.
using FrameSink = void (*)(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width,
                           uint32_t height, void* user);
bool captureFrame(unsigned int frame, FrameSink sink, void* user);
void saveFramePng(unsigned int frame, const std::vector<uint8_t>& rgb, uint32_t width, uint32_t height);

// pc_frame.cpp (step 6.7): creates the COS_PERF file and writes its header row (exit
// PC_EXIT_USAGE if it cannot be created); nothing without COS_PERF. perfFlush writes out the rows
// still buffered (pc_exit; any thread, never blocks).
void perfOpen();
void perfFlush();
// pc_frame.cpp: COS_PERF_EVERY changed at run time (options menu); 0 stops the perf lines.
void perfSetEvery(unsigned int every);

// pc_frame.cpp: "[cos] pacing: frames= wall= requested= ..." since the frame loop started (nothing
// before it).
void writePacing(int fd);

// pc_main.cpp: COS_FB_SCALE at run time (options menu): Aurora resizes the EFB with the next event
// pump. 0 = the window's size.
void setFrameBufferScale(float scale);

// pc_menu.cpp: the options menu (ZL+ZR+Minus / L+R+Z / F1). menuInit registers the built-in
// settings (after aurora_initialize); menuFrame runs in pc_frame_end before the FPS overlay, inside
// the frame's ImGui frame. menuOpen: the menu is open (the game is paused and gets no pad input).
void menuInit();
// pc_menu.cpp: the options-menu smoke script asked for its own memory card ("#card run"); pc_main
// then calls prepareRunCard before the game's CARDInit.
bool menuSmokeWantsRunCard();
void menuFrame();
bool menuOpen();
// pc_menu.cpp: after aurora_end_frame of game frame `frame` (the screenshot action, and
// COS_SMOKE=options-menu's checks).
void menuFrameEnd(unsigned int frame);

// pc_precompile.cpp: Aurora's boot pipeline warm-up (COS_PRECOMPILE, COS_PRECOMPILE_LOG).
// precompileInit runs right after aurora_initialize and precompileLoadingScreen after it (before
// the game starts: returns once the loading screen's pipelines are built, or at once without one);
// precompileOverlay from pc_frame_end before aurora_end_frame and precompileFrame after it, every
// game frame.
void precompileInit(const char* cacheDir);
void precompileLoadingScreen();
void precompileOverlay();
void precompileFrame(unsigned int frames);

} // namespace pc
