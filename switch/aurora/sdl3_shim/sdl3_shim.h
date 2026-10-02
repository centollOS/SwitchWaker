// Internal interfaces between the parts of the Switch SDL 3 shim.
#pragma once

#include <SDL3/SDL.h>

#define SDL3_SHIM_WIDTH 1280
#define SDL3_SHIM_HEIGHT 720

void sdl3_shim_set_error(const char* format, ...);

// Gamepad: configures libnx input once; pump samples it and queues
// SDL_EVENT_GAMEPAD_ADDED/REMOVED when the controller appears or goes.
void sdl3_shim_gamepad_init(void);
void sdl3_shim_gamepad_pump(void);

// Queues an event for SDL_PollEvent after running the event watches.
void sdl3_shim_queue_event(const SDL_Event* event);
