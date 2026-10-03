// SDL 3 functions that Aurora 3227d76 (native/'s pin) and the SDK (native/sdk) call beyond those
// the translated port's Aurora needed (switch/aurora/sdl3_shim, which this file extends for the
// native port's NRO). Same rules as the shim: what the Switch has, on libnx and newlib, or an
// explicit failure.
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>

#include "sdl3_shim.h"

// Errors: SDL_SetError always returns false (SDL_InvalidParamError is a macro over it).
bool SDL_SetError(SDL_PRINTF_FORMAT_STRING const char* fmt, ...) {
    char message[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);
    sdl3_shim_set_error("%s", message);
    return false;
}

// Files (Aurora's atomic file writer and its GCI memory card folder).
bool SDL_RenamePath(const char* oldpath, const char* newpath) {
    if (oldpath == NULL || newpath == NULL)
        return SDL_SetError("SDL_RenamePath: null path");
    // newlib's rename does not replace an existing file on the SD card; SDL's does.
    remove(newpath);
    if (rename(oldpath, newpath) != 0)
        return SDL_SetError("could not rename %s to %s", oldpath, newpath);
    return true;
}

bool SDL_ReadU8(SDL_IOStream* src, Uint8* value) {
    Uint8 byte;
    if (SDL_ReadIO(src, &byte, 1) != 1)
        return false;
    if (value != NULL)
        *value = byte;
    return true;
}

bool SDL_ReadU16LE(SDL_IOStream* src, Uint16* value) {
    Uint8 bytes[2];
    if (SDL_ReadIO(src, bytes, sizeof bytes) != sizeof bytes)
        return false;
    if (value != NULL)
        *value = (Uint16)(bytes[0] | bytes[1] << 8);
    return true;
}

bool SDL_WriteU16LE(SDL_IOStream* dst, Uint16 value) {
    const Uint8 bytes[2] = {(Uint8)value, (Uint8)(value >> 8)};
    return SDL_WriteIO(dst, bytes, sizeof bytes) == sizeof bytes;
}

// Random bits for temporary file names: the system's random generator.
Uint32 SDL_rand_bits(void) {
    Uint32 value = 0;
    randomGet(&value, sizeof value);
    return value;
}

// Thread priorities are set when the thread is created (switch/native/source/thread_wrap.c).
bool SDL_SetCurrentThreadPriority(SDL_ThreadPriority priority) {
    (void)priority;
    return true;
}

// One window: no event names another one.
SDL_Window* SDL_GetWindowFromEvent(const SDL_Event* event) {
    (void)event;
    return NULL;
}

// The SDK's AI output (native/sdk/src/audio/AI.cpp) names the driver in its log line; the shim's
// one playback stream is libnx's audout (switch/aurora/sdl3_shim/sdl3_shim_audio.c).
const char* SDL_GetCurrentAudioDriver(void) { return "audout"; }
