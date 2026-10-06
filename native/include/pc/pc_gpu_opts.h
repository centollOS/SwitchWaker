/*
 * pc_gpu_opts.h - opt-in GPU-side reductions of the native port, for A/B runs on the Switch
 * (docs/SWITCH_PERF_STUDY.md; native/src/pc/features/pc_gpu_opts.cpp).
 * All default off except COS_MIST_LOWRES (near-identical output, see below): with none set the
 * game renders as before.
 *
 * COS_SHADOW_OFFSCREEN=1  the real-time shadow casters (dDlst_shadowControl_c::imageDraw) are drawn
 *                         into an offscreen target of the EFB region's pixel size (GXCreateFrameBuffer)
 *                         instead of the corner of the EFB; the I4 copies keep their size, so the
 *                         shadows look the same, but the main EFB pass is no longer broken (and
 *                         reloaded) once per shadow: at 1280x720 the shadow passes bind a 512x384 target rather
 *                         than the 1280x720 EFB, and their depth clears become full-target clears.
 * COS_SHADOW_OFFSCREEN=gc as 1, at the GameCube's own size whatever the internal resolution: a
 *                         256x256 target and 128x128 textures (at 1280x720, a third of the caster
 *                         pixels and a quarter of the copy texels); shadow edges are softer/blockier,
 *                         as on the console. For measurement.
 * COS_DOF=0               drawDepth (m_Do_graphic.cpp, every play frame: the distance blur, i.e.
 *                         depth of field) is skipped while neither the monotone (grey) effect nor
 *                         the motion blur (which reads drawDepth's colour copy) is on: far scenery
 *                         is no longer softened. Gone with it: the Z16 copy of the depth buffer and
 *                         the half-size colour copy (two EFB pass breaks and their conversion
 *                         passes) and the full-screen composite. For measurement.
 *
 * COS_MIST_LOWRES=<n>    (default 4; 0 = off) the forest mist (drawCloudShadow, d_kankyo_rain.cpp: up to 100 blended
 *                         sprites with no depth test, 10-12 screens of fragments in A_mori) is drawn
 *                         into an offscreen target 1/n of the EFB in each direction and composited
 *                         with the same blend equation (n = 2, 3 or 4; 0 or 1 = off). Skipped while
 *                         spot lights or motion blur read the EFB alpha the mist used to write.
 *
 * COS_SKY_LOWRES=<n>     (default off) the sky lists (dome, sky layers, clouds, sun; 3.4-4 screens
 *                         of mostly blended full-screen fragments outdoors at 1280x720) are drawn
 *                         into an offscreen target 1/n of the EFB in each direction over a copy of
 *                         the cleared EFB and stretched back (n = 2..4). The sky is drawn first,
 *                         writes neither depth nor alpha, so only its resolution changes (softer
 *                         cloud edges). Skipped when the 2D copy list or the viewport would make
 *                         it differ.
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

enum {
    PC_SHADOW_OFFSCREEN_OFF = 0,
    PC_SHADOW_OFFSCREEN_SAME = 1, /* COS_SHADOW_OFFSCREEN=1 */
    PC_SHADOW_OFFSCREEN_GC = 2,   /* COS_SHADOW_OFFSCREEN=gc */
};
/* PC_SHADOW_OFFSCREEN_*. */
int pc_shadow_offscreen(void);
/* The pixel size of a logicalW x logicalH region of the 640x480 EFB at the current internal
   resolution, as Aurora maps viewports and EFB copies (gx.cpp map_logical_viewport, GXFrameBuffer.cpp
   scale_copy_dst): lround(logical * EFB pixels / logical EFB size), at least 1. */
void pc_efb_pixel_size(unsigned int logicalW, unsigned int logicalH, unsigned int* outW,
                       unsigned int* outH);
/* Logs the offscreen shadow target's size the first time and whenever it changes. */
void pc_shadow_offscreen_opened(unsigned int w, unsigned int h, unsigned int copyW,
                                unsigned int copyH);

/* Zero when COS_DOF=0 (the depth-of-field composite is skipped where that is safe). */
int pc_dof_enabled(void);

/* COS_MIST_LOWRES: the divisor (0 = off). */
int pc_mist_lowres(void);
/* The low-resolution mist target's size for the current EFB and a host buffer to name its copy
   texture (GXCopyTex destination), or NULL when the target would be under 16x16. */
void* pc_mist_lowres_target(unsigned int* w, unsigned int* h);

/* COS_MIST_AB=<frame> (Mac checks; runs are not frame-reproducible): in that frame the mist is
   drawn both ways from the same scene, the low-resolution result copied to buffer 2 and the
   original to buffer 1; frames +1 and +2 show buffer 1 and 2 over everything (shoot them with
   COS_SHOT). pc_mist_ab_frame: this game frame is the A/B frame. pc_mist_ab_buffer(i): the copy
   texture names (0 = the scene before the mist). pc_mist_ab_show: the buffer to show this frame,
   or NULL. */
int pc_mist_ab_frame(void);
void* pc_mist_ab_buffer(int index);
void* pc_mist_ab_show(void);

/* COS_SKY_LOWRES: called by the painter around the sky lists. begin returns 1 when the sky goes to
   the reduced target (the viewport is the whole logical EFB and has_sky/copy2d_empty allow it);
   end copies it back over the EFB (colour only) and leaves GX for the J3D drawing that follows
   (j3dSys.reinitGX and the projection are the caller's). */
int pc_sky_lowres_begin(float vpNear, float vpFar, int hasSky, int copy2dEmpty);
void pc_sky_lowres_end(void);
/* COS_SKY_AB=<frame>: like COS_MIST_AB for the sky (shown through pc_mist_ab_show); copy the
   whole EFB into a copy texture; redraw the cleared EFB pc_sky_lowres_begin copied. */
int pc_sky_ab_frame(void);
void pc_ab_copy_efb(void* buf);
void pc_sky_ab_restore(void);

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
   values as the variables (COS_SHADOW_OFFSCREEN as PC_SHADOW_OFFSCREEN_*, COS_DOF 0/1,
   COS_MIST_LOWRES and COS_SKY_LOWRES 0 or 2-4, COS_GPU_GROUPS 0-2). Each takes effect with the
   next frame the game draws. */
void pc_shadow_offscreen_set(int mode);
void pc_dof_set(int enabled);
void pc_mist_lowres_set(int div);
void pc_sky_lowres_set(int div);
void pc_gpu_groups_set(int level);

#ifdef __cplusplus
}
#endif

#endif /* PC_GPU_OPTS_H */
