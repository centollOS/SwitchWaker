/*
 * pc_aspect.h - the widescreen option of the native port (COS_ASPECT, native/src/pc/pc_aspect.cpp).
 *
 * COS_ASPECT=4:3 | 16:9 | 16:10 (default 4:3; the Switch harness sets 16:9). At 4:3 the game runs
 * as on the GameCube. A wider aspect does in C what the community 16:9 Gecko code does to the
 * GameCube executable (mods/widescreen/GZLE01.gecko, Dolphin's GZLE01.ini "$16:9 Widescreen"; each
 * of its lines is decoded in docs/MODS.md): the camera's aspect (projection and view culling), the
 * 2D screen bounds, the HUD and minimap positions, the menus' full-screen masks, and Aurora
 * presents the 640x480 picture stretched to that aspect (Aurora patch 0006), as a widescreen TV
 * shows the console's anamorphic output.
 *
 * Each number the 16:9 code changes is interpolated between the game's 4:3 value and the code's
 * 16:9 value with t = (A - 4/3) / (16/9 - 4/3): 0 at 4:3, 1 at 16:9, 0.6 at 16:10, the same
 * interpolation as scripts/mods/widescreen_aspect.py.
 *
 * The first call reads COS_ASPECT; pc_harness_init makes that call (and exits 2 on a malformed
 * value), so game code reads a fixed value. Static initialisers must not call these: the Switch
 * harness sets its default in main, after them.
 */
#ifndef PC_ASPECT_H
#define PC_ASPECT_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_ASPECT_4_3 = 0,
    PC_ASPECT_16_9 = 1,
    PC_ASPECT_16_10 = 2,
};

/* PC_ASPECT_*: the COS_ASPECT the process runs with. */
int pc_aspect(void);
/* "4:3", "16:9" or "16:10". */
const char* pc_aspect_name(void);
/* Width / height of the presented picture: 4/3, 16/9 or 16/10. */
float pc_aspect_ratio(void);
/* t of the interpolation: 0 at 4:3, 1 at 16:9, 0.6 at 16:10. */
float pc_aspect_t(void);
/* Reads and checks COS_ASPECT (pc_harness_init): exits 2 (PC_EXIT_USAGE) on an unknown value. */
void pc_aspect_init(void);

/* Nonzero when the picture is wider than 4:3: the widescreen changes apply. */
static inline int pc_aspect_wide(void) {
    return pc_aspect() != PC_ASPECT_4_3;
}

/* The game's 4:3 value v43 moved toward the 16:9 code's value v169 (exactly v169 at 16:9). */
static inline float pc_aspect_lerp(float v43, float v169) {
    return v43 + (v169 - v43) * pc_aspect_t();
}

/* The values the 16:9 code uses in several places. */
/* 2D screen left and right (m_Do_graphic's ortho -9..650; .sdata2 0x803F7D68 / 0x803F7D6C). */
static inline float pc_aspect_2d_left(void) {
    return pc_aspect_lerp(-9.0f, -123.0f);
}
static inline float pc_aspect_2d_right(void) {
    return pc_aspect_lerp(650.0f, 767.0f);
}
/* How far the HUD moves toward each edge (the pool constant 114.0 at 0x803F92CC). */
static inline float pc_aspect_hud_shift(void) {
    return pc_aspect_lerp(0.0f, 114.0f);
}

#ifdef __cplusplus
}
#endif

#endif /* PC_ASPECT_H */
