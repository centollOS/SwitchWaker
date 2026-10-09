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

/* fpcM_Management (f_pc_manager.cpp), after the game frame's draw pass: with COS_FPS60_TEST=1 and
   a game frame of two retraces, ends this Aurora frame, begins another and returns 1: the caller
   paints and runs the draw pass again (the same scene presented twice; each paint then waits one
   retrace instead of two). 0 otherwise. A measurement of the cost of 60 presents a second. */
extern "C" int pc_frame_extra(void);
/* COS_FPS60_TEST: 1 when on (read once). */
extern "C" int pc_fps60_test(void);
/* pc_wait_for_tick: whether the frame's last wait was halved (a two-retrace frame under the test). */
extern "C" void pc_frame_halved_wait(int halved);

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
