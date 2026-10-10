/*
 * pc_dynres.h - dynamic resolution of the 3D scene (native/src/pc/features/pc_dynres.cpp;
 * docs/SWITCH_PERF_STUDY.md, section 8). Off by default.
 *
 * COS_DYNRES=1            a safety net for GPU-bound scenes: the painter draws the 3D part of each
 *                         frame into the top-left part of the EFB (Aurora's content scale,
 *                         native/patches/aurora/0009) at a lower internal resolution, copies it,
 *                         and draws it stretched over the whole EFB before the 2D, which keeps the
 *                         full resolution (HUD, menus, text). Levels in COS_FB_SCALE units: the
 *                         base scale, then 5/6, 3/4 and 2/3 of it (1.5 -> 1.25, 1.125, 1.0; 2.25
 *                         docked -> 1.875, 1.6875, 1.5; pc_dynres_policy.h), or COS_DYNRES_LEVELS
 *                         (a comma list below the base). The GPU samples are per present (the GPU
 *                         time of the frames read back since the last game frame over their
 *                         number). The level drops one step when their p95 over the last 60 game
 *                         frames stays above COS_DYNRES_HIGH ms (default 30; COS_DYNRES_HIGH60,
 *                         default 15, while the 60 fps mode is on: 16.7 ms a present) for two
 *                         evaluations (every 30 frames), and rises one step when the p95 scaled by
 *                         the pixel ratio of the level above stays under COS_DYNRES_LOW ms
 *                         (default 27; COS_DYNRES_LOW60, default 13) for four evaluations. Needs
 *                         the Switch's GPU timer; elsewhere it stays at the base unless forced.
 * COS_DYNRES=auto         the default (options menu "Automática con 60 fps"): as 1 while the 60 fps
 *                         mode is on (docs/FPS60_PLAN.md step E), the base scale while it is off.
 *                         0 (the menu's "Desactivada") keeps it off even with 60 fps.
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
/* 60 fps budget guard (pc_frame.cpp): 1 when the GPU's p95 per present (over at least 30 game
   frames at this level) is over budgetMs and dynres cannot go lower (off, fixed, idle, at its
   floor). The Switch only (no GPU timer elsewhere: 0). */
int pc_dynres_gpu_limited(double budgetMs);
/* The GPU's p95 per present over the samples at this level (0 without samples). */
double pc_dynres_gpu_p95(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_DYNRES_H */
