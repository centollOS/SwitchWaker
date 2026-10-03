// Shared state of the run harness (native/src/pc/pc_*.cpp, docs/NATIVE_PORT_PHASE4_6.md step 6.0).
// Public API: native/include/pc/pc_harness.h.
#pragma once

#include "pc/pc_harness.h"

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
    double timeoutS = 0;             // COS_TIMEOUT_S, 0 = off
    double stallS = 0;               // COS_STALL_S, 0 = off
    unsigned int frames = 0;         // COS_FRAMES, 0 = off
    bool uncapped = false;           // COS_UNCAPPED
    bool audio = true;               // COS_AUDIO (off/0 -> false)
};

extern Config gConfig;

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
// arc-sweep, msg-sweep, jpa-sweep, stage-sweep, blo-sweep; from pc_heaps_created); it never returns then.
void runHeapsSmoke();
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
// pc_arc.cpp: COS_SMOKE=arc-sweep.
[[noreturn]] void smokeArcSweep();
bool isKnownSmoke(const char* name);
void printSmokes(int fd);

// pc_input.cpp (step 6.3): reads the COS_INPUT script (exit PC_EXIT_USAGE if it is malformed, or
// if COS_SMOKE=pad-echo has none).
void loadInput();

// pc_watchdog.cpp
void startWatchdog();

// pc_frame.cpp: "[cos] pacing: frames= wall= requested= ..." since the frame loop started (nothing
// before it).
void writePacing(int fd);

} // namespace pc
