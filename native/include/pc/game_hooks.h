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
/* 1 while 60 fps is on: the options menu's COS_FPS60 (per operation mode, live), unless
   COS_FPS60_TEST=1/0 (environment or [dev]) forces it for the run. */
extern "C" int pc_fps60_test(void);
/* The menu's apply callback for COS_FPS60: on/off from the next game frame. */
extern "C" void pc_fps60_set(int on);
/* pc_wait_for_tick: the retraces this wait lasts for a game frame of `retraces` (1 for each paint of
   a split frame, else `retraces`; 0, no wait, for COS_PAINT_PURITY_REPEAT's second paint B). */
extern "C" unsigned int pc_frame_wait_retraces(unsigned int retraces);
/* fpcM_Management, after the draw pass (and callBack2), before paint B: step E's budget guard measures
   the draw pass here (docs/FPS60_PLAN.md "Step E"); COS_FPS60_TEST_DELAY_MS adds its test delay. */
extern "C" void pc_frame_draw_end(void);
/* Step E: the next frame is presented once (no paint B), and the 3 after it, as in a transition:
   a window resize, an operation mode change, a new internal resolution (the swapchain or the EFB
   resized between two paints). `why` is logged (a string literal). */
extern "C" void pc_fps60_hold(const char* why);

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
/* mDoGph_Painter, before the shadow images (dComIfGd_imageDrawShadow): in paint B, decides the cut and
   points the models at their blended world-space joints already (step D: the real shadows are cast
   from the models at t); pc_fps60_view_begin does the rest. COS_FPS60_SHADOWS=0: nothing here. */
void pc_fps60_paint_prepare(view_class* view);
/* pc_frame_split, every frame after the logic: 0 while a transition is in progress or just ended
   (scene request pending, overlap, wipe, screen fade, JUTFader fading, monotone changing, stage
   change; a 3-frame cool-down after any of them): the frame is then presented once (no paint B),
   since paint B would repaint lists the scene being built or deleted left half made (flicker). */
int pc_fps60_paint_b_allowed(void);
/* f_pc_node_req.cpp: a node (scene) request is queued. */
int pc_fpcNdRq_pending();
/* The perf line: paints B interpolated and not (cuts) so far. */
void pc_fps60_camera_stats(unsigned long* interpolated, unsigned long* skipped);
/* d_kankyo_wether.cpp, around the sun, lens flare, star and sky cloud packets (built around the
   draw pass's camera eye): in paint B they get the camera's turn only (the eye of frame N+1 with the
   blended orientation), then the full delta again. Nothing outside a blended paint B. */
void pc_fps60_sky_begin(void);
void pc_fps60_sky_end(void);

/* ---- 60 fps step C: objects of paint B (native/src/pc/game_hooks/pc_fps60_models.cpp) ---------- */

class J3DModel;
/* J3DModel::initialize: a model (re)made at this address; its earlier captures are dropped. */
void pc_fps60_model_init(J3DModel* model);
/* The end of J3DModel::viewCalc: in the draw pass (not the painter), keeps the model's view-space draw
   and normal matrices and the view they were made with (COS_FPS60_TEST only). */
void pc_fps60_model_viewcalc(J3DModel* model);
/* fpcM_Management, around the draw pass (fpcDw_Handler and callBack2): viewCalc calls in between
   are listed; at the end the listed models' matrices are copied (the frame's "cur", the last one
   becoming "prev"). */
void pc_fps60_models_draw_begin(void);
void pc_fps60_models_draw_end(void);
/* cAPIGph_Painter, around mDoGph_Painter (both paints): 1 inside. */
void pc_fps60_painting(int on);
/* pc_fps60_view_begin (paint B, no cut): points every model captured in both draw passes at matrices
   blended at t into a scratch arena (V_cur * lerp(W_prev, W_cur)); pc_fps60_view_end puts the
   models' own matrices back. viewPrev/viewCur: the cameras' view matrices of the two draw passes. */
void pc_fps60_models_paint_begin(float t, const float viewPrev[3][4], const float viewCur[3][4]);
void pc_fps60_models_paint_end(void);
/* pc_fps60_view_begin, after the shadow images: the rest of the blend (view-space draw matrices, the
   CPU-skinned packets' base, mViewBaseMtx), which the shadow images' viewCalc would undo. */
void pc_fps60_models_paint_apply(void);
/* dDlst_shadowReal_c::imageDraw in paint B: for a CPU-skinned caster blended in this paint, the world
   turn/move R_t * R_cur^-1 its skinned vertices (made at N+1) need (returns 1); else 0. */
int pc_fps60_models_skin_delta(J3DModel* model, float out[3][4]);
/* A custom packet's view-space matrix loaded at paint time (made in the draw pass with the camera's
   view, e.g. daSail_packet_c's): out = m, except in a blended paint B, where out is the blend at t
   of the matrix the same packet (key) loaded in this frame's paint A (the draw pass before's) and m,
   as for J3D models. Host memory only. */
void pc_fps60_packet_mtx(const void* key, const float m[3][4], float out[3][4]);
/* The perf line's step C part (counts and ms since the last call), "" with the mode off. */
void pc_fps60_models_stats(char* out, unsigned long size, double frames);
/* The blend's t in a blended paint B (models on, camera not cut), else 0: paint-time counters that
   paint A advances (the sea's texture scroll) are drawn that fraction of a step ahead in paint B. */
float pc_fps60_paint_t(void);
/* 1 inside a blended paint B (pc_fps60_paint_t's t applies, whatever its value). */
int pc_fps60_paint_blending(void);
/* An array of floats a packet draws from, made in the logic or the draw pass (the sea's wave
   heights): paint A keeps a copy (by key); in a blended paint B the result is the blend at t of that
   copy and cur (host memory, valid until the next call with the key), else cur itself. */
const float* pc_fps60_paint_floats(const void* key, const float* cur, unsigned int n);
/* pc_fps60_paint_t for the sea's texture scroll; 0 with COS_FPS60_SEA=0 (which also turns off
   pc_fps60_paint_floats, whose only user is the sea). */
float pc_fps60_sea_t(void);
/* out = the blend at t of two 3x4 matrices as for the models (3x3 columns turned keeping their
   length, translation lerped); 0 (out undefined) past 300 units or a 45-degree turn. */
int pc_fps60_blend_mtx(const float a[3][4], const float b[3][4], float out[3][4], float t);

/* ---- 60 fps step D: particles of paint B (native/src/pc/game_hooks/pc_fps60_particles.cpp) ------ */

class JPAEmitterManager;
class JPABaseEmitter;
class JPABaseParticle;
/* JPAEmitterManager's constructor: the particle and emitter pools (host tables indexed by slot). */
void pc_fps60_particles_register(JPAEmitterManager* mgr, JPABaseParticle* ptcls, unsigned int nptcl,
                                 JPABaseEmitter* emtrs, unsigned int nemtr);
/* pc_fps60_models_draw_end: every live particle and emitter kept (this frame's "cur"). */
void pc_fps60_particles_capture(void);
/* pc_fps60_view_begin, paint B not cut: particles drawn at t for the rest of this paint. */
void pc_fps60_particles_paint_begin(float t);
/* JPAEmitterManager::draw, around one emitter's draw: in a blended paint B its particles' and its own
   fields are set to the blend at t (begin returns 1), and put back by end. */
int pc_fps60_particles_emitter_begin(JPABaseEmitter* e);
void pc_fps60_particles_emitter_end(JPABaseEmitter* e);
/* dPa_waveEcallBack::draw: v (an offset in the emitter's frame of calc) turned by the emitter's
   blended turn, inside its emitter_begin/_end; unchanged otherwise. */
void pc_fps60_particles_emitter_turn(JPABaseEmitter* e, float v[3]);
/* Strips that map their texture along the particle list (dPa_trackEcallBack, the ship's wake): in a
   blended paint B, (1 - t) * the particles born this frame in the emitter's list, moved back with
   it (their texture coordinate along the strip starts that many particles earlier); else 0. */
float pc_fps60_particles_strip_shift(JPABaseEmitter* e);
/* The perf line's particle part. */
void pc_fps60_particles_stats(char* out, unsigned long size, double frames);

/* ---- 60 fps step D: lines and motion blur (native/src/pc/game_hooks/pc_fps60_misc.cpp) ----------- */

/* mDoExt_3DlineMat0/1_c::draw, per line: the positions to point GX_VA_POS at. Paint A: cur (the
   packet's drawn count kept); a blended paint B: the blend at t of prev (the other array, paint A's)
   and cur in scratch memory that outlives the GX worker's read, if paint A drew the packet with the
   same count this game frame and nothing jumped; else cur. count = positions (3 floats each). */
const float* pc_fps60_line_positions(const void* key, int lineNo, const float* cur, const float* prev,
                                     unsigned int count);
/* 1 when the present being painted follows the previous one by one retrace (paint B; paint A after a
   frame that ended with paint B). */
extern "C" int pc_fps60_presents_close(void);
/* motionBlure: the blur rate (0-255) and texture matrix for this present: the same decay per game
   frame when two presents share it (sqrt of the rate, the matrix's step halved), else as given. */
unsigned char pc_fps60_blur_rate(unsigned char rate, const float m[3][4], float out[3][4]);
/* The perf line's lines part. */
void pc_fps60_misc_stats(char* out, unsigned long size, double frames);

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
