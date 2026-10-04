/*
 * pc_gpu_opts.h - opt-in GPU-side reductions of the native port, for A/B runs on the Switch
 * (docs/SWITCH_PERF_STUDY.md; native/src/pc/pc_gpu_opts.cpp).
 * All default off: with none set the game renders exactly as before.
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

#ifdef __cplusplus
}
#endif

#endif /* PC_GPU_OPTS_H */
