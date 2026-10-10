/*
 * pc_gpu_opts.h - the mist at a lower resolution, the depth-of-field switch and the GPU-group
 * diagnostics of the native port (docs/SWITCH_PERF_STUDY.md; native/src/pc/features/pc_gpu_opts.cpp).
 * The lower-resolution sky and the offscreen shadow casters were removed once the deko3d renderer
 * made them pointless (docs/DEKO3D_MIGRATION_PLAN.md, phase 4); the mist stayed: at full resolution
 * it cost 12.9 ms of GPU a frame in the Tower of the Gods (90 sprites).
 *
 * COS_MIST_LOWRES=<n>    (default 4; 0 = off; [dev] only, no menu row) the mist (drawCloudShadow,
 *                         d_kankyo_rain.cpp: up to 100 blended sprites with no depth test, many screens
 *                         of fragments in the forests and the Tower) is drawn into an offscreen target 1/n
 *                         of the EFB in each direction and composited with the same blend equation
 *                         (n = 2, 3 or 4; 0 or 1 = off). Skipped while spot lights or motion blur read
 *                         the EFB alpha the mist used to write.
 *
 * COS_DOF=0               drawDepth (m_Do_graphic.cpp, every play frame: the distance blur, i.e.
 *                         depth of field) is skipped while neither the monotone (grey) effect nor
 *                         the motion blur (which reads drawDepth's colour copy) is on: far scenery
 *                         is no longer softened. Gone with it: the Z16 copy of the depth buffer and
 *                         the half-size colour copy (two EFB pass breaks and their conversion
 *                         passes) and the full-screen composite. For measurement.
 *
 * Diagnostics (docs/SWITCH_PERF_STUDY.md, section 8):
 * COS_GPU_GROUPS=1        the painter inserts a GX debug marker before each draw-list bucket and
 *                         effect (sky, BG, opaque, translucent, particles, DOF, 2D, ...); the
 *                         Switch's GPU timer (dawn-switch-gl-gpu-groups.patch) times the draws
 *                         between markers and the perf-switch "gpu groups" line lists them.
 * COS_GPU_GROUPS=2        as 1, plus a marker before each J3D packet whose label differs from the
 *                         previous one ("<bucket>/<material name>" for model materials, the
 *                         packet's class otherwise): finer, costs a GPU timer query per group.
 * COS_DRAW_CENSUS=<frame>[,<frame>...]  for each listed game frame, Aurora's draw census
 *                         (native/patches/aurora/0008) writes census-<frame>-draws.csv (every GX
 *                         draw: bucket/material marker, samples written, shader and texture summary)
 *                         and census-<frame>-passes.csv (render passes, EFB copies) into
 *                         COS_RUN_DIR (or "."); those frames get COS_GPU_GROUPS=2 markers.
 *
 * The first call of each reads its variable; game code calls them on the game thread only.
 */
#ifndef PC_GPU_OPTS_H
#define PC_GPU_OPTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* The pixel size of a logicalW x logicalH region of the 640x480 EFB at the current internal
   resolution, as Aurora maps viewports and EFB copies (gx.cpp map_logical_viewport, GXFrameBuffer.cpp
   scale_copy_dst): lround(logical * EFB pixels / logical EFB size), at least 1. */
void pc_efb_pixel_size(unsigned int logicalW, unsigned int logicalH, unsigned int* outW,
                       unsigned int* outH);

/* COS_MIST_LOWRES: the divisor (0 = off). */
int pc_mist_lowres(void);
/* The low-resolution mist target's size for the current EFB and a host buffer to name its copy
   texture (GXCopyTex destination), or NULL when the target would be under 16x16. */
void* pc_mist_lowres_target(unsigned int* w, unsigned int* h);

/* Zero when COS_DOF=0 (the depth-of-field composite is skipped where that is safe). */
int pc_dof_enabled(void);

/* The current GPU group level: 0 off, 1 buckets, 2 buckets and packets (COS_GPU_GROUPS, or 2 in a
   COS_DRAW_CENSUS frame). Cheap: a load. */
extern int pc_gpu_groups_level;
/* Level >= 1: a GX debug marker named `name` (a static string) starts a new GPU group. */
void pc_gpu_group(const char* name);
/* Level >= 2: a marker for the J3D packet about to draw, unless its label equals the last one. */
void pc_gpu_group_packet(const void* packet);
/* Called by the frame loop before each game frame's aurora_begin_frame (frame = pc_frame_count()+1). */
void pc_gpu_groups_frame_begin(unsigned int frame);

/* Run-time changes from the options menu (pc_menu.cpp), game thread, between frames: the same
   values as the variables (COS_DOF 0/1, COS_GPU_GROUPS 0-2). Each takes effect with the next
   frame the game draws. */
void pc_dof_set(int enabled);
void pc_gpu_groups_set(int level);

#ifdef __cplusplus
}
#endif

#endif /* PC_GPU_OPTS_H */
