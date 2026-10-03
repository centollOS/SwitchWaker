// Shared state of the run harness (native/src/pc/pc_*.cpp, docs/NATIVE_PORT_PHASE4_6.md step 6.0).
// Public API: native/include/pc/pc_harness.h.
#pragma once

#include "pc/pc_harness.h"

#include <cstddef>
#include <cstdint>

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
    // COS_FPS_OVERLAY: a frame-rate panel drawn with Aurora's ImGui (pc_overlay.cpp). Off by default;
    // the Switch build sets 1.
    bool fpsOverlay = false;
};

extern Config gConfig;

// Blocks the calling thread for good (pause(); Horizon has none, so a sleep loop there).
[[noreturn]] void waitForever();

// Milliseconds since pc_harness_init (monotonic).
uint64_t elapsedMs();
uint64_t monotonicNs();

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
// main stick that moves Link more than 300 units, and 3,600 frames since he was in the room.
void outsetArm(const char* stageName, int roomNo);
void outsetFrame(unsigned int frames);
// pc_outset.cpp: the M12 probe found Link in the COS_BOOT_STAGE start room (PLAY scene executing,
// room up, player actor created).
bool outsetLinkReady();

// pc_actor_sweep.cpp (step 6.9): COS_SMOKE=actor-sweep; actorSweepFrame runs from pc_frame_end
// every game frame and, once outsetLinkReady, spawns every actor profile next to Link in turn,
// runs it 30 frames and deletes it, then exits 0.
void actorSweepFrame(unsigned int frames);
// pc_bgm_hop.cpp (bug B1): COS_SMOKE=bgm-hop; bgmHopFrame runs from pc_frame_end every game
// frame and, once Link is in the COS_BOOT_STAGE island room, measures the island BGM, goes to a
// house and back and checks the BGM came back.
void bgmHopFrame(unsigned int frames);

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

// pc_shot.cpp: parses COS_SHOT / COS_SHOT_EVERY (exit PC_EXIT_USAGE if malformed); without
// them the screenshots stay off. Returns whether any frame will be captured.
bool loadShots();
// pc_shot.cpp: after aurora_end_frame of game frame `frame` (pc_frame_count numbering): saves the
// presented image as shot-<frame>.png if COS_SHOT or COS_SHOT_EVERY names that frame.
void shotFrameEnd(unsigned int frame);

// pc_frame.cpp (step 6.7): creates the COS_PERF file and writes its header row (exit
// PC_EXIT_USAGE if it cannot be created); nothing without COS_PERF. perfFlush writes out the rows
// still buffered (pc_exit; any thread, never blocks).
void perfOpen();
void perfFlush();

// pc_frame.cpp: "[cos] pacing: frames= wall= requested= ..." since the frame loop started (nothing
// before it).
void writePacing(int fd);

// pc_precompile.cpp: Aurora's boot pipeline warm-up (COS_PRECOMPILE, COS_PRECOMPILE_LOG).
// precompileInit runs right after aurora_initialize and precompileLoadingScreen after it (before
// the game starts: returns once the loading screen's pipelines are built, or at once without one);
// precompileOverlay from pc_frame_end before aurora_end_frame and precompileFrame after it, every
// game frame.
void precompileInit();
void precompileLoadingScreen();
void precompileOverlay();
void precompileFrame(unsigned int frames);

} // namespace pc
