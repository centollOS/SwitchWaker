/*
 * pc_dynres.h - dynamic resolution of the 3D scene (native/src/pc/features/pc_dynres.cpp;
 * docs/SWITCH_PERF_STUDY.md, section 8). Off by default.
 *
 * COS_DYNRES=1            a safety net for GPU-bound scenes: the painter draws the 3D part of each
 *                         frame into the top-left part of the EFB (Aurora's content scale,
 *                         native/patches/aurora/0009) at a lower internal resolution, copies it,
 *                         and draws it stretched over the whole EFB before the 2D, which keeps the
 *                         full resolution (HUD, menus, text). Levels in COS_FB_SCALE units: the
 *                         base scale (1.5 = 1280x720), then 1.25 and 1.125 (COS_DYNRES_LEVELS, a
 *                         comma list below the base). The level drops one step when the GPU
 *                         timer's p95 over the last 60 frames stays above COS_DYNRES_HIGH ms
 *                         (default 30) for two evaluations (every 30 frames), and rises one step
 *                         when the p95 scaled by the pixel ratio of the level above stays under
 *                         COS_DYNRES_LOW ms (default 27) for four evaluations. Needs the Switch's
 *                         GPU timer (dawn-switch-gl-gpu-timer.patch); elsewhere it stays at the
 *                         base unless forced.
 * COS_DYNRES=fixed:<scale> the 3D always at that scale (e.g. fixed:1.25; for checks and A/B).
 * COS_DYNRES_CYCLE=<n>    with COS_DYNRES=1: step through the levels every n frames (Mac checks of
 *                         the transitions without a GPU timer).
 *
 * The game thread calls all of them.
 */
#ifndef PC_DYNRES_H
#define PC_DYNRES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Before each game frame (pc_frame_begin): reads the GPU timer and moves between levels. */
void pc_dynres_frame_begin(unsigned int frame);
/* The painter, before the first 3D drawing: sets the reduced scale if the level asks for one. */
void pc_dynres_3d_begin(void);
/* The painter, after the 3D drawing and before the 2D: stretches the 3D over the EFB and restores
   the full scale. Nothing if pc_dynres_3d_begin set no reduced scale. */
void pc_dynres_3d_end(void);
/* The options menu, between frames: starts over with these values of COS_DYNRES ("0" or NULL:
   off, "1", "fixed:<scale>") and COS_FB_SCALE (the base; 0 = unset). */
void pc_dynres_configure(const char* mode, float base);
/* The content scale in effect for the 3D drawing now (1 = the whole EFB). */
float pc_dynres_content_scale(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_DYNRES_H */
