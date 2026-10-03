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
 *   COS_TRACE      comma list of trace channels: res, scene, frame (or all)
 *   COS_UNCAPPED   1: no frame pacing and no vsync (pc_frame_pace, Aurora)
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

/* Milestone M2 (step 4.2, pc_heap.cpp), called by main01 right after mDoMch_Create returned:
   check() on the root, system, main, game, archive and command heaps; logs "heaps" if all hold,
   else exits 1. Then runs COS_SMOKE=font (step 4.3), which exits. */
void pc_heaps_created(void);

/* Milestone M3 (step 4.3, pc_milestone.cpp), called by LOAD_COPYDATE on the DVD thread: main01
   queued it after mDoGph_Create and mDoCPd_Create returned. Logs the date read from /COPYDATE and
   "gfx-create" when the read succeeded (status nonzero, the string no longer the placeholder),
   else exits 1. */
void pc_copydate_loaded(int status, const char* copydate);

/* Milestone M5 (step 4.5, pc_frame.cpp), called by d_s_logo.cpp's phase_2 when the LOGO scene is
   created: logoFiles is the entry count (countFile) of the mounted Logo archive (0 when it is not
   mounted), logoTimg the boot logo's BTI header and logoSize its size in the archive.
   Checks the header (376x104, the image inside the resource), then pc_frame_end logs "logo-scene"
   after the first frame in which Aurora uploaded texture data; exits 1 when the archive is not
   mounted or the header is wrong. */
struct ResTIMG;
void pc_logo_scene_created(int logoFiles, const struct ResTIMG* logoTimg,
                           unsigned int logoSize);

/* Logs "[cos] MILESTONE <name> frame= retrace= ms=" and exits 0 if <name> is COS_MILESTONE. */
void pc_milestone(const char* name);

/* One game frame done (called by pc_frame_end). Feeds the stall watchdog and COS_FRAMES. */
void pc_frame_tick(void);
unsigned int pc_frame_count(void);

/* The frame loop (step 6.2, pc_frame.cpp). main01 calls pc_frame_begin at the top of each
   iteration (Aurora's event pump, then aurora_begin_frame, retried while the window cannot
   present; a quit request exits) and pc_frame_end at the bottom (aurora_end_frame, pc_frame_tick,
   milestone M4 frame-loop: 120 frames, the retrace count went up, Aurora counted draw calls). */
void pc_frame_begin(void);
void pc_frame_end(void);

/* The wait of JFWDisplay's waitForTick (step 6.2): returns once periodNs have passed since the
   previous call returned (Dusklight's limiter); returns at once with COS_UNCAPPED. */
void pc_frame_pace(unsigned long long periodNs);

/* One NTSC VI retrace (59.94 Hz): 1001/60000 s. */
#define PC_RETRACE_PERIOD_NS 16683333ull

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
