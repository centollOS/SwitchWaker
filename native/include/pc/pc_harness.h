/*
 * pc_harness.h - the run harness of the native executable centollos (docs/NATIVE_PORT_PHASE4_6.md,
 * step 6.0). Implemented in native/src/pc/pc_*.cpp, linked into centollos (and into the link census
 * bundle) as the static library cos_pc. Only TARGET_PC code calls it.
 *
 * Environment (read once by pc_harness_init):
 *   COS_DISC       path of the GZLE01 disc image (required unless the smoke test needs none)
 *   COS_SMOKE      run one smoke test instead of the game (static-init, crash-test, ...)
 *   COS_MILESTONE  exit 0 as soon as this milestone is logged (static-init, aurora-up, ...)
 *   COS_TIMEOUT_S  in-process watchdog: exit 10 after this many seconds (0 or unset: off)
 *   COS_STALL_S    exit 11 when the game frame counter is frozen this long (0 or unset: off)
 *   COS_TRACE      comma list of trace channels: res, scene (or all)
 *   COS_UNCAPPED   1: no frame pacing (used from step 6.2)
 *   COS_AUDIO      off: audio stays silent (used from step 6.1)
 *   COS_FRAMES     exit 0 after this many game frames
 *   COS_RUN_DIR    directory for backtrace.txt / stall.txt (set by native/tools/run.sh)
 *
 * Exit codes: see PC_EXIT_* below.
 */
#ifndef PC_HARNESS_H
#define PC_HARNESS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_EXIT_REACHED = 0,       /* milestone reached, smoke test passed, COS_FRAMES done */
    PC_EXIT_CHECK_FAILED = 1,  /* a smoke test ran to its end and found errors */
    PC_EXIT_USAGE = 2,         /* unknown COS_SMOKE / COS_MILESTONE, malformed number */
    PC_EXIT_TIMEOUT = 10,      /* COS_TIMEOUT_S elapsed */
    PC_EXIT_STALL = 11,        /* frame counter frozen for COS_STALL_S */
    PC_EXIT_PANIC = 12,        /* OSPanic (JUT_ASSERT ends there too) */
    PC_EXIT_SIGNAL = 13,       /* fatal signal caught by the crash handler */
    PC_EXIT_DISC = 14,         /* COS_DISC missing, unreadable or not GZLE01 revision 0 */
};

/* Called first thing in main: reads the environment, installs the crash handler and the
   watchdog, runs a COS_SMOKE test that needs no SDK (and exits), then checks the disc
   (exit 14 on failure) and runs a COS_SMOKE test that needs only the disc (disc-ls; it exits).
   Returns only when the game should boot. */
void pc_harness_init(int argc, char* argv[]);

/* Aurora bring-up (step 6.1, pc_main.cpp), called by main right after pc_harness_init: Aurora's
   window and device (MEM1 256 MiB, ARAM 16 MiB), aurora_dvd_open(COS_DISC) with the disc ID check
   (exit 14 unless GZLE01 version 0), OSInit, the cos_sdk thread hooks that give each new OS thread
   the current JKRHeap of the thread that resumed it, and COS_AUDIO=off. Exits on failure. */
void pc_aurora_init(int argc, char* argv[]);

/* The OSThread record the process main thread runs as (cos_sdk's default thread), which runs
   main01 on PC: m_Do_main.cpp binds mainThread to it. Usable during static initialisation. */
struct OSThread* pc_main_thread(void);

/* Logs "[cos] MILESTONE <name> frame= retrace= ms=" and exits 0 if <name> is COS_MILESTONE. */
void pc_milestone(const char* name);

/* One game frame done (called by the frame loop, step 6.2). Feeds the stall watchdog and
   COS_FRAMES. */
void pc_frame_tick(void);
unsigned int pc_frame_count(void);

/* Trace channels (COS_TRACE) and the state the crash handler prints. */
int pc_trace_enabled(const char* channel);
void pc_trace_scene(int procName);
void pc_trace_resource(const char* path, int entryNum);

/* Options for later steps. */
const char* pc_env_disc(void);
int pc_env_uncapped(void);
int pc_env_audio(void);

/* OSPanic on PC: prints the state and the host backtrace, exits 12. */
__attribute__((noreturn)) void pc_panic(const char* file, int line);

/* Flushes what it can (without blocking on a stdio lock another thread holds) and _Exit(code).
   Only the first caller exits; a concurrent second caller blocks forever. */
__attribute__((noreturn)) void pc_exit(int code);

#ifdef __cplusplus
}
#endif

#endif /* PC_HARNESS_H */
