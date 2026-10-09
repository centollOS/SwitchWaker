/*
 * game_hooks.h - the larger host changes of game/, moved out of the decompiled files (step G3 of
 * docs/GAME_CODE_ORGANIZATION.md). game/ keeps a one-line call under TARGET_PC, the original code
 * in the #else; the bodies are in native/src/pc/game_hooks/ (one file per area, named in each
 * section below). Header-level replacements (declarations, inline hot-path math) are separate
 * headers in native/include/pc/game_hooks/, included by the game header they replace.
 *
 * C++ only, compiled like the game units (cos_pc links cos_game_headers); the game types are
 * forward-declared so a game file includes this header wherever its own includes end.
 */
#ifndef PC_GAME_HOOKS_H
#define PC_GAME_HOOKS_H

#include "helpers/endian.h"

/*
 * A callable handed to a hook, which calls it back: e.g. a lambda that draws with the game
 * function's locals. Holds a reference: the callable must outlive the hook call.
 */
class PcFnRef {
public:
    template <typename F>
    PcFnRef(F& f) : mCall(&callImpl<F>), mObj(&f) {}
    void operator()() const { mCall(mObj); }

private:
    template <typename F>
    static void callImpl(void* obj) { (*static_cast<F*>(obj))(); }
    void (*mCall)(void*);
    void* mObj;
};

/* ---- The mist at a lower resolution (pc_gpu_opts.h), native/src/pc/game_hooks/pc_gpu_hooks.cpp --- */

/* drawCloudShadow (d_kankyo_rain.cpp): draws the mist sprites, at a reduced resolution with
   COS_MIST_LOWRES (default 1/4), or as the GameCube does. drawSprites draws them with the current GX
   state. */
void pc_kyr_draw_mist(PcFnRef drawSprites);

/* ---- Picto box capture (m_Do_graphic.cpp), native/src/pc/game_hooks/pc_capture.cpp ---------- */

/* mDoGph_Painter, capture step 3 (bug B36): whether the GXCopyTex picture is in dest, in RAM, for
   encode_s3tc. The first call asks for it and returns false; the copy is read back from the GPU
   after that frame's aurora_end_frame, and the next frame's call returns true. format: GX_TF_I8 or
   GX_TF_RGB565 (other formats: true at once, nothing read back). */
bool pc_gph_capture_ready(void* dest, u32 width, u32 height, int format);

/* dSnap_packet::Judge (d_snap.cpp, bug B37): the picto box's subject check peeks the EFB alpha of
   the shutter area right after drawing each registered object's volume with its own alpha. The
   request copies that area of the EFB (logical coordinates) at this point of the frame; after the
   frame's aurora_end_frame the copy is read back and done runs, where pc_efb_peek_argb reads it
   as GXPeekARGB does (0xAARRGGBB; outside the area or without a readback: white). */
void pc_efb_peek_request(u16 left, u16 top, u16 width, u16 height, void (*done)());
u32 pc_efb_peek_argb(u16 x, u16 y);
/* PcJudgePixels's result, for the log and COS_SMOKE=picto-box: count registered objects in table
   (dSnap_RegistObjElm), the photo result. */
class dSnap_RegistObjElm;
void pc_snap_judged(int count, const dSnap_RegistObjElm* table, int result);

/* ---- Logo scene (d_s_logo.cpp): run harness, native/src/pc/game_hooks/pc_logo_hooks.cpp ------ */

class dScnLogo_c;

/* dvdWaitDraw, once every load command has synced: milestone M6 logo-res, then the debug stage
   boot (COS_BOOT_STAGE, step 6.4), which requests the PLAY scene itself and returns true (the
   opening scene is not requested). */
bool pc_logo_dvd_synced(dScnLogo_c* i_this);

/* phase_2, the logo scene created: milestone M5 logo-scene; the debug stage boot's request is
   not made yet. */
void pc_logo_scene_created_hook();

/* ---- Frame pacing (JFWDisplay.cpp), native/src/pc/game_hooks/pc_jfw_display.cpp -------------- */

/* waitForTick: waits out p1 ticks (or p2 retraces when p1 is 0) with pc_frame_pace and makes the
   retraces the console's VI interrupt would have counted meanwhile (step 6.2). */
void pc_wait_for_tick(unsigned int p1, unsigned short p2);

/* ---- 60 presents a second (pc_frame.cpp) -------------------------------------------------------- */

/* COS_FPS60_TEST=1 ([dev]; docs/FPS60_PLAN.md, step A): each game frame of two retraces is presented
   twice. Paint A (fpcM_Management's cAPIGph_Painter, the lists of the previous draw pass) is
   presented at once (pc_frame_split); logic and the draw pass run; paint B repaints the new lists
   with cAPIGph_Painter alone (no draw pass) between pc_paint_extra_begin and pc_paint_extra_end,
   and pc_frame_end presents it. Each paint waits one retrace, so the game keeps its speed. The
   options menu open, a frame of another length, or the mode off: the frame runs as before. */

/* fpcM_Management, right after paint A: 1 when this frame is split (paint A's wait was halved);
   paint A is then presented (Aurora frame ended, the next one begun) and the caller paints B after
   callBack2. 0 otherwise (nothing done). */
extern "C" int pc_frame_split(void);
/* Around paint B (fpcM_Management): pc_paint_is_extra() is 1 in between. With COS_PAINT_PURITY
   they snapshot and compare the game's writable globals (pc_paint_purity.cpp). */
extern "C" void pc_paint_extra_begin(void);
extern "C" void pc_paint_extra_end(void);
/* 1 while paint B runs: the guards that keep it from advancing the game's paint-time state (fade,
   wipe, picto box capture, JUTFader, sea scroll, weather counters). Always 0 with the mode off. */
extern "C" int pc_paint_is_extra(void);
/* COS_FPS60_TEST: 1 when on (read once). */
extern "C" int pc_fps60_test(void);
/* pc_wait_for_tick: the retraces this wait lasts for a game frame of `retraces` (1 for each paint of
   a split frame, else `retraces`; 0, no wait, for COS_PAINT_PURITY_REPEAT's second paint B). */
extern "C" unsigned int pc_frame_wait_retraces(unsigned int retraces);

/* ---- 60 fps step B: the camera of paint B (native/src/pc/game_hooks/pc_fps60_camera.cpp) -------- */

struct view_class;

/* camera_draw (d_camera.cpp, the draw pass), once the view and projection are built: keeps this
   camera and the one of the draw pass before (COS_FPS60_TEST only). */
void pc_fps60_camera_drawn(view_class* view);
/* dCamera_c::Reset: the camera is placed without its smoothing (a cut when it starts). */
void pc_fps60_camera_reset(void);
/* mDoGph_Painter, after the world projection is loaded / after the camera block: in paint B, records
   the view delta to the camera halfway between the two draw passes' cameras in the GX stream
   (Aurora patch 0019), unless it is a cut; nothing in paint A or with the mode off. */
void pc_fps60_view_begin(view_class* view);
void pc_fps60_view_end(void);
/* pc_frame_split, every frame after the logic: 0 while a transition is in progress or just ended
   (scene request pending, overlap, wipe, screen fade, JUTFader fading, monotone changing, stage
   change; a 3-frame cool-down after any of them): the frame is then presented once (no paint B),
   since paint B would repaint lists the scene being built or deleted left half made (flicker). */
int pc_fps60_paint_b_allowed(void);
/* f_pc_node_req.cpp: a node (scene) request is queued. */
int pc_fpcNdRq_pending();
/* The perf line: paints B interpolated and not (cuts) so far. */
void pc_fps60_camera_stats(unsigned long* interpolated, unsigned long* skipped);

/* ---- Collision data (c_bg_s.cpp), native/src/pc/game_hooks/pc_c_bg_s.cpp --------------------- */

class cBgD_t;

/* cBgS::ConvDzb, the first time a DZB is seen: its table offsets become pointers (OFFSET_PTR), its
   vertex table host-order (step 4.10). */
void cBgS_PcConvDzbTables(cBgD_t* pbgd);

/* ---- Widescreen HUD (d_meter.cpp, pc_aspect.h), native/src/pc/game_hooks/pc_meter_hooks.cpp --- */

struct fopMsgM_pane_class;

/* A 16:9 Gecko code value interpolated for COS_ASPECT and rounded. */
short dMeter_pcLerpS16(float v43, float v169);
/* dMeter_Create: the HUD tuning values the 16:9 code writes into g_meter_mapHIO and g_meterHIO. */
void dMeter_pcAspectHIO();
/* dMeter_Draw's fopMsgM_setAlpha for five panes: the pane shifted with the HUD first. */
void dMeter_pcSetAlphaShifted(fopMsgM_pane_class* i_pane);

/* ---- Audio init data (JAIInitData.cpp), native/src/pc/game_hooks/pc_audio_hooks.cpp ---------- */

namespace JAInter { namespace BankWave { struct initOnCode_s; } }

/* JaiInit.aaf's bank or wave-system list at `list` as a host table with a zero terminator; *words is
   the number of list words before the list's own terminator. */
JAInter::BankWave::initOnCode_s* JAIPcMakeInitOnCodeTable(BE(u32)* list, int* words);

#endif /* PC_GAME_HOOKS_H */
