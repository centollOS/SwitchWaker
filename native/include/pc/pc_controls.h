/*
 * pc_controls.h - control options (native/src/pc/features/pc_controls.cpp).
 *
 * COS_CAMERA_INVERT_X=1   the C stick's horizontal axis turns the camera the other way
 * COS_CAMERA_INVERT_Y=1   the C stick's vertical axis tilts the camera the other way
 * COS_CAMERA_SHAKE=1      the camera shakes (quakes, hits, explosions, cutscenes); default off
 *
 * Applied only where the camera reads the C stick (d_camera.cpp, dCamera_cStickX/Y): the wind
 * baton's left hand, the item menu's songs, the grappling hook and the figure viewer keep the C
 * stick's real directions. Options menu: Gráficos > "Invertir cámara horizontal" and "Invertir
 * cámara vertical", live. The camera shake: every shake reaches the camera through dVibration_c::Run
 * (d_vibration.cpp), which starts none while it is off (the controller rumble is kept); Gráficos >
 * "Temblor de cámara", live. The game thread calls all of them.
 */
#ifndef PC_CONTROLS_H
#define PC_CONTROLS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Nonzero when that C stick axis is inverted (read from the environment the first time). */
int pc_camera_invert_x(void);
int pc_camera_invert_y(void);
/* The options menu: set an axis (0 or 1). */
void pc_camera_invert_x_set(int on);
void pc_camera_invert_y_set(int on);
/* Nonzero when the camera may shake (COS_CAMERA_SHAKE=1; off by default), and its setter. */
int pc_camera_shake(void);
void pc_camera_shake_set(int on);

#ifdef __cplusplus
}
#endif

#endif
