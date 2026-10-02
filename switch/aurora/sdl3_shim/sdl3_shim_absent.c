// SDL 3 devices the Switch build does not have: haptic devices, standalone
// sensors, keyboard and mouse. Each reports absence the way SDL does when the
// device is missing.
#include <SDL3/SDL.h>
#include <stdlib.h>

#include "sdl3_shim.h"

SDL_HapticID* SDL_GetHaptics(int* count) {
    if (count != NULL)
        *count = 0;
    return calloc(1, sizeof(SDL_HapticID));
}
const char* SDL_GetHapticNameForID(SDL_HapticID instance_id) { (void)instance_id; return NULL; }
SDL_Haptic* SDL_OpenHaptic(SDL_HapticID instance_id) { (void)instance_id; return NULL; }
void SDL_CloseHaptic(SDL_Haptic* haptic) { (void)haptic; }
bool SDL_HapticRumbleSupported(SDL_Haptic* haptic) { (void)haptic; return false; }
bool SDL_InitHapticRumble(SDL_Haptic* haptic) { (void)haptic; return false; }
bool SDL_PlayHapticRumble(SDL_Haptic* haptic, float strength, Uint32 length) {
    (void)haptic;
    (void)strength;
    (void)length;
    return false;
}
bool SDL_StopHapticRumble(SDL_Haptic* haptic) { (void)haptic; return false; }

SDL_SensorID* SDL_GetSensors(int* count) {
    if (count != NULL)
        *count = 0;
    return calloc(1, sizeof(SDL_SensorID));
}
SDL_SensorType SDL_GetSensorTypeForID(SDL_SensorID instance_id) {
    (void)instance_id;
    return SDL_SENSOR_INVALID;
}
SDL_Sensor* SDL_OpenSensor(SDL_SensorID instance_id) { (void)instance_id; return NULL; }
void SDL_CloseSensor(SDL_Sensor* sensor) { (void)sensor; }
bool SDL_GetSensorData(SDL_Sensor* sensor, float* data, int num_values) {
    (void)sensor;
    (void)data;
    (void)num_values;
    return false;
}
void SDL_UpdateSensors(void) {}

const bool* SDL_GetKeyboardState(int* numkeys) {
    static const bool kNoKeys[SDL_SCANCODE_COUNT];
    if (numkeys != NULL)
        *numkeys = SDL_SCANCODE_COUNT;
    return kNoKeys;
}
SDL_Window* SDL_GetKeyboardFocus(void) { return NULL; }

static SDL_MouseButtonFlags no_mouse(float* x, float* y) {
    if (x != NULL)
        *x = 0.0f;
    if (y != NULL)
        *y = 0.0f;
    return 0;
}
SDL_MouseButtonFlags SDL_GetMouseState(float* x, float* y) { return no_mouse(x, y); }
SDL_MouseButtonFlags SDL_GetGlobalMouseState(float* x, float* y) { return no_mouse(x, y); }
SDL_MouseButtonFlags SDL_GetRelativeMouseState(float* x, float* y) { return no_mouse(x, y); }
SDL_Window* SDL_GetMouseFocus(void) { return NULL; }
