/*
 * pc_harness.h - the run harness of the native executable switchwaker (docs/NATIVE_PORT_PHASE4_6.md,
 * step 6.0). Implemented in native/src/pc (README.md there), linked into switchwaker (and into the
 * link census bundle) as the static library cos_pc. Only TARGET_PC code calls it.
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
 *   COS_INPUT      input script for controller port 0 (step 6.3, pc_input.cpp)
 *   COS_BOOT_STAGE <stage>:<room>[:<point>[:<layer>]]: debug stage boot (step 6.4, pc_boot.cpp)
 *   COS_BOOT_EVENTS hex event bits the debug boot sets on its new file, e.g. 2A80 (pc_boot.cpp)
 *   COS_BOOT_ITEMS hex item numbers the debug boot gives its new file, the first on X, e.g. 20
 *   COS_BOOT_PRESET sailing: a story preset on the debug boot's new file (pc_preset.cpp)
 *   COS_PERF_EVERY every this many frames, one line of game-thread frame times (pc_frame.cpp)
 *   COS_PERF       file that gets one CSV row of game-thread times per game frame (step 6.7)
 *   COS_ASPECT     4:3 (default), 16:9 or 16:10: the widescreen option (pc_aspect.h)
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

/* Milestone M6 (step 4.8, pc_frame.cpp). d_s_logo.cpp's dvdWaitDraw, once every l_*Command has
   synced, reports each object archive the logo scene keeps resident (pc_logo_res_object: its
   file count and how many files dRes_info_c::loadResource converted, which must be all) and
   then the count of mounted archives, of files read to main RAM and of commands that left
   nothing (pc_logo_res_synced; exits 1 on any gap). dComIfG_changeOpeningScene calls
   pc_opening_scene_called on entry, which logs "logo-res" after a complete report. */
void pc_logo_res_object(const char* name, int files, int loaded);
void pc_logo_res_synced(int archives, int files, int missing);
void pc_opening_scene_called(void);

/* Milestone M7 (boot loop, pc_frame.cpp), called at the end of every dStage_Create: the start
   stage's name and room, the file count of the mounted "Stage" archive (0 when it is not mounted)
   and whether stage.dzs was found. Logs the stage; logs "opening" for the title opening's sea_T
   once the archive is mounted and stage.dzs read, else exits 1 for sea_T. For sea_T it also arms
   milestone M8 title-stage (pc_title_stage.cpp) for that room. */
void pc_stage_created(const char* stageName, int roomNo, int stageFiles, int hasDzs);

/* Milestone M9 title (boot loop, pc_title.cpp), called by daTitle_proc_c::proc_draw right after
   the title_logo BLO screen was drawn. pc_frame_end then checks, reading game state only, that
   the d_a_title actor finished creating, drew its screen in this frame with the logo pane fully
   faded in, and that its JPA emitters were set (the title smoke live with particles, the sparkle
   emitter set once); 60 such frames in a row log "title". */
void pc_title_drawn(void);

/* Milestone M10 file-select (boot loop, pc_file_select.cpp), called at the end of
   dScnName_c::draw with the name scene's main, memory card check and draw procedure indices.
   pc_frame_end counts the frames in which the name scene was drawn with a screen up (any draw
   procedure but NoneDraw); 60 such frames in a row log "file-select". */
void pc_name_scene_drawn(int mainProc, int memCardCheckProc, int drawProc);

/* Debug stage boot (step 6.4, decision H4, pc_boot.cpp). COS_BOOT_STAGE=<stage>:<room>[:<point>
   [:<layer>]] (point 0 and layer -1 when left out; the new game starts at sea:44:206) makes the
   logo scene, once its resources synced, start a new file and go straight to the PLAY scene at
   that stage instead of dComIfG_changeOpeningScene: no title, file select, name entry or intro.
   pc_boot_stage returns the parsed request, or NULL without COS_BOOT_STAGE. */
struct PcBootStage {
    char stage[8]; /* stage name, at most 7 characters (dStage_startStage_c::mName) */
    int room;      /* 0..63 */
    int point;     /* s16 */
    int layer;     /* -1 (the game picks it) .. 15 */
};
const struct PcBootStage* pc_boot_stage(void);
/* d_s_logo.cpp, once its PLAY scene request was accepted: the next stage the game now holds.
   Logs it and checks it against COS_BOOT_STAGE (exit 1 on a difference), then logs M6 logo-res
   (the request stands in for dComIfG_changeOpeningScene). */
void pc_boot_stage_requested(const char* stage, int room, int point, int layer);
// dScnPly_Draw, after the actors' draws: the harness's own draws into the game's draw lists
// (COS_SMOKE=res-sweep, native/src/pc/harness/sweeps/pc_res_sweep.cpp); nothing otherwise.
void pc_play_draw(void);
/* d_s_play.cpp phase_1, when a PLAY scene takes the next stage as its start stage: with
   COS_BOOT_STAGE, the first one must be the requested stage (logged; exit 1 on a difference). */
void pc_play_stage_started(const char* stage, int room, int point, int layer);
/* COS_BOOT_EVENTS=<hex>[,<hex>...] (with COS_BOOT_STAGE; bug B6): story event bits (dSv_event_flag_c
   values, e.g. 2A80) the debug boot sets on its new file, so a stage boot can start later in the
   story. Returns how many of them were stored in out (at most max); exits 2 if malformed. */
int pc_boot_event_bits(unsigned short* out, int max);
/* COS_BOOT_ITEMS=<hex>[,<hex>...] (with COS_BOOT_STAGE; bug B6): items (dItemNo_*, e.g. 20 for the
   telescope) the debug boot gives its new file with execItemGet; the first one is put on X.
   Returns how many were stored in out (at most max); exits 2 if malformed. */
int pc_boot_items(unsigned short* out, int max);
/* COS_BOOT_PRESET=sailing (pc_preset.cpp): the debug boot puts that story preset on its new file
   after COS_BOOT_EVENTS and COS_BOOT_ITEMS (event bits, items, the X/Y items); without
   COS_BOOT_STAGE the boot starts at the preset's spawn (sailing: sea:11:102, on the boat).
   Exits 2 for an unknown preset. Nothing without COS_BOOT_PRESET. */
void pc_boot_preset_apply(void);

/* Fixed debug camera (bug B7, pc_shore.cpp): with COS_CAMERA=<eye x>,<eye y>,<eye z>,<center x>,
   <center y>,<center z> (or while COS_SMOKE=shore-foam holds its view), camera_draw (d_camera.cpp)
   gets that eye and centre in place of the game camera's every frame; returns nonzero then.
   Without it nothing is changed and 0 is returned. */
int pc_camera_override(float* eye, float* center);

/* Logs "[cos] MILESTONE <name> frame= retrace= ms=" and exits 0 if <name> is COS_MILESTONE. */
void pc_milestone(const char* name);

/* One game frame done (called by pc_frame_end). Feeds the stall watchdog and COS_FRAMES. */
void pc_frame_tick(void);
unsigned int pc_frame_count(void);

/* Input injection (step 6.3, pc_input.cpp). mDoCPd_Read calls pc_pad_feed before
   JUTGamePad::read: with COS_INPUT, port 0 gets the script's state for this game frame through
   Aurora's virtual pad (PADSetVirtualStatus). pc_pad_read_done, once g_mDoCPd_cpadInfo is
   converted, runs the check of COS_SMOKE=pad-echo (which ends the process after the script). */
void pc_pad_feed(void);
void pc_pad_read_done(void);

/* The frame loop (step 6.2, pc_frame.cpp). main01 calls pc_frame_begin at the top of each
   iteration (Aurora's event pump, then aurora_begin_frame, retried while the window cannot
   present; a quit request exits) and pc_frame_end at the bottom (aurora_end_frame, pc_frame_tick,
   milestone M4 frame-loop: 120 frames, the retrace count went up, Aurora counted draw calls). */
void pc_frame_begin(void);
void pc_frame_end(void);

/* The wait of JFWDisplay's waitForTick (step 6.2): returns once periodNs have passed since the
   previous call returned (Dusklight's limiter); returns at once with COS_UNCAPPED. */
void pc_frame_pace(unsigned long long periodNs);

/* Performance instrumentation (step 6.7, pc_frame.cpp). main01 brackets mDoCPd_Read,
   mDoAud_Execute and fapGm_Execute, and cAPIGph_Painter brackets the painter (mDoGph_Painter,
   which fapGm_Execute runs first), with pc_perf_begin/pc_perf_end; time spent in pc_frame_pace
   inside a bracket is left out of it. The COS_PERF rows and COS_PERF_EVERY lines report them;
   without either both calls return at once. */
enum {
    PC_PERF_CPD_READ = 0,    /* mDoCPd_Read */
    PC_PERF_AUD_EXECUTE = 1, /* mDoAud_Execute */
    PC_PERF_GAME = 2,        /* fapGm_Execute, the painter included */
    PC_PERF_PAINTER = 3,     /* mDoGph_Painter: the GX encode of the frame's draw lists */
    PC_PERF_PAINTER2 = 4,    /* COS_FPS60_TEST: paint B, the lists painted again (game_hooks.h) */
    PC_PERF_SPLIT = 5,       /* COS_FPS60_TEST: paint A's present (pc_frame_split) */
    PC_PERF_PHASES = 6,
};
void pc_perf_begin(int phase);
void pc_perf_end(int phase);

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
