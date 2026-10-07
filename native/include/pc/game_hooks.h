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

/* ---- GPU options (pc_gpu_opts.h), native/src/pc/game_hooks/pc_gpu_hooks.cpp ---------------- */

class camera_process_class;
struct view_port_class;

/* drawCloudShadow (d_kankyo_rain.cpp): draws the forest mist sprites, at a reduced resolution
   with COS_MIST_LOWRES and both ways with COS_MIST_AB, or as the GameCube does. drawSprites draws
   them with the current GX state; setupState sets the state drawCloudShadow set before them. */
void pc_kyr_draw_mist(PcFnRef drawSprites, PcFnRef setupState);

/* mDoGph_Painter (m_Do_graphic.cpp), around the sky lists: COS_SKY_LOWRES draws them into a
   smaller target (begin returns whether it did), end stretches the result back and, on a
   COS_SKY_AB frame, redraws the sky the original way for the comparison copies. */
bool pc_gph_sky_lowres_begin(camera_process_class* camera, view_port_class* viewport);
void pc_gph_sky_lowres_end(camera_process_class* camera);

/* mDoGph_Painter, after the 2D: COS_MIST_AB shows the A/B frame's copies over everything. */
void pc_gph_mist_ab_show();

/* dDlst_shadowControl_c::imageDraw (d_drawlist.cpp), COS_SHADOW_OFFSCREEN: opens the offscreen
   target the casters are drawn into, in place of the EFB's corner (closed by GXRestoreFrameBuffer). */
void pc_shadow_image_offscreen_open();

/* ---- Picto box capture (m_Do_graphic.cpp), native/src/pc/game_hooks/pc_capture.cpp ---------- */

/* mDoGph_Painter, capture step 3 (bug B36): whether the GXCopyTex picture is in dest, in RAM, for
   encode_s3tc. The first call asks for it and returns false; the copy is read back from the GPU
   after that frame's aurora_end_frame, and the next frame's call returns true. format: GX_TF_I8 or
   GX_TF_RGB565 (other formats: true at once, nothing read back). */
bool pc_gph_capture_ready(void* dest, u32 width, u32 height, int format);

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
