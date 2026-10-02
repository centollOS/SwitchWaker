// The SDL 3 functions Aurora calls, implemented on libnx for the Switch
// (there is no public SDL 3 port for Horizon). Only that subset exists: a
// call outside it is a link error, not a silent no-op. This file: init,
// errors, hints, memory, time, properties, GUIDs, pixel formats, surfaces.
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "sdl3_shim.h"

static char g_error[256] = "";

void sdl3_shim_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_error, sizeof g_error, format, args);
    va_end(args);
}

const char* SDL_GetError(void) { return g_error; }

bool SDL_Init(SDL_InitFlags flags) { return SDL_InitSubSystem(flags); }

bool SDL_InitSubSystem(SDL_InitFlags flags) {
    if ((flags & (SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) != 0)
        sdl3_shim_gamepad_init();
    return true;
}

void SDL_Quit(void) {}

bool SDL_SetHint(const char* name, const char* value) {
    (void)name;
    (void)value;
    return true;
}

void SDL_free(void* mem) { free(mem); }

int SDL_strcmp(const char* str1, const char* str2) { return strcmp(str1, str2); }

Uint64 SDL_GetTicks(void) { return armTicksToNs(armGetSystemTick()) / 1000000ULL; }

void SDL_Delay(Uint32 ms) { svcSleepThread((s64)ms * 1000000LL); }

// Properties: a few small sets, enough for the window and the gamepad. Values
// keep their own type; reading a missing name returns the caller's default.
#define PROPERTY_SETS 16
#define PROPERTIES_PER_SET 32

typedef enum { VALUE_POINTER, VALUE_STRING, VALUE_NUMBER, VALUE_BOOLEAN } ValueType;

typedef struct {
    char name[64];
    ValueType type;
    void* pointer;
    char* string;
    Sint64 number;
    bool boolean;
} Property;

typedef struct {
    bool used;
    Property entries[PROPERTIES_PER_SET];
    int count;
} PropertySet;

static PropertySet g_sets[PROPERTY_SETS];
static Mutex g_properties_lock;

SDL_PropertiesID SDL_CreateProperties(void) {
    mutexLock(&g_properties_lock);
    for (int i = 0; i < PROPERTY_SETS; ++i) {
        if (!g_sets[i].used) {
            memset(&g_sets[i], 0, sizeof g_sets[i]);
            g_sets[i].used = true;
            mutexUnlock(&g_properties_lock);
            return (SDL_PropertiesID)(i + 1);
        }
    }
    mutexUnlock(&g_properties_lock);
    sdl3_shim_set_error("out of property sets");
    return 0;
}

// Returns the named entry, creating it when create is set. Caller holds the lock.
static Property* find_property(SDL_PropertiesID props, const char* name, bool create) {
    if (props == 0 || props > PROPERTY_SETS || name == NULL)
        return NULL;
    PropertySet* set = &g_sets[props - 1];
    if (!set->used)
        return NULL;
    for (int i = 0; i < set->count; ++i) {
        if (strcmp(set->entries[i].name, name) == 0)
            return &set->entries[i];
    }
    if (!create || set->count == PROPERTIES_PER_SET)
        return NULL;
    Property* entry = &set->entries[set->count++];
    memset(entry, 0, sizeof *entry);
    snprintf(entry->name, sizeof entry->name, "%s", name);
    return entry;
}

static bool set_property(SDL_PropertiesID props, const char* name, ValueType type,
                         void* pointer, const char* string, Sint64 number, bool boolean) {
    mutexLock(&g_properties_lock);
    Property* entry = find_property(props, name, true);
    if (entry != NULL) {
        free(entry->string);
        entry->type = type;
        entry->pointer = pointer;
        entry->string = string != NULL ? strdup(string) : NULL;
        entry->number = number;
        entry->boolean = boolean;
    }
    mutexUnlock(&g_properties_lock);
    return entry != NULL;
}

bool SDL_SetPointerProperty(SDL_PropertiesID props, const char* name, void* value) {
    return set_property(props, name, VALUE_POINTER, value, NULL, 0, false);
}

bool SDL_SetStringProperty(SDL_PropertiesID props, const char* name, const char* value) {
    return set_property(props, name, VALUE_STRING, NULL, value, 0, false);
}

bool SDL_SetNumberProperty(SDL_PropertiesID props, const char* name, Sint64 value) {
    return set_property(props, name, VALUE_NUMBER, NULL, NULL, value, false);
}

bool SDL_SetBooleanProperty(SDL_PropertiesID props, const char* name, bool value) {
    return set_property(props, name, VALUE_BOOLEAN, NULL, NULL, 0, value);
}

void* SDL_GetPointerProperty(SDL_PropertiesID props, const char* name, void* default_value) {
    mutexLock(&g_properties_lock);
    const Property* entry = find_property(props, name, false);
    void* value = entry != NULL && entry->type == VALUE_POINTER ? entry->pointer : default_value;
    mutexUnlock(&g_properties_lock);
    return value;
}

Sint64 SDL_GetNumberProperty(SDL_PropertiesID props, const char* name, Sint64 default_value) {
    mutexLock(&g_properties_lock);
    const Property* entry = find_property(props, name, false);
    Sint64 value = default_value;
    if (entry != NULL && entry->type == VALUE_NUMBER)
        value = entry->number;
    else if (entry != NULL && entry->type == VALUE_BOOLEAN)
        value = entry->boolean ? 1 : 0;
    mutexUnlock(&g_properties_lock);
    return value;
}

bool SDL_GetBooleanProperty(SDL_PropertiesID props, const char* name, bool default_value) {
    mutexLock(&g_properties_lock);
    const Property* entry = find_property(props, name, false);
    bool value = default_value;
    if (entry != NULL && entry->type == VALUE_BOOLEAN)
        value = entry->boolean;
    else if (entry != NULL && entry->type == VALUE_NUMBER)
        value = entry->number != 0;
    mutexUnlock(&g_properties_lock);
    return value;
}

// GUIDs: the 32-hex-digit text form SDL uses.
void SDL_GUIDToString(SDL_GUID guid, char* pszGUID, int cbGUID) {
    if (pszGUID == NULL || cbGUID <= 0)
        return;
    static const char digits[] = "0123456789abcdef";
    int out = 0;
    for (int i = 0; i < 16 && out + 2 < cbGUID; ++i) {
        pszGUID[out++] = digits[guid.data[i] >> 4];
        pszGUID[out++] = digits[guid.data[i] & 15];
    }
    pszGUID[out] = '\0';
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

SDL_GUID SDL_StringToGUID(const char* pchGUID) {
    SDL_GUID guid;
    memset(&guid, 0, sizeof guid);
    for (int i = 0; pchGUID != NULL && i < 16; ++i) {
        const int high = hex_value(pchGUID[2 * i]);
        const int low = high < 0 ? -1 : hex_value(pchGUID[2 * i + 1]);
        if (high < 0 || low < 0)
            break;
        guid.data[i] = (Uint8)(high << 4 | low);
    }
    return guid;
}

// Window icons are the only surfaces Aurora makes; the Switch has no use for them.
SDL_PixelFormat SDL_GetPixelFormatForMasks(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask,
                                           Uint32 Amask) {
    if (bpp == 32 && Rmask == 0x000000ff && Gmask == 0x0000ff00 && Bmask == 0x00ff0000 &&
        Amask == 0xff000000)
        return SDL_PIXELFORMAT_ABGR8888;
    if (bpp == 32 && Rmask == 0xff000000 && Gmask == 0x00ff0000 && Bmask == 0x0000ff00 &&
        Amask == 0x000000ff)
        return SDL_PIXELFORMAT_RGBA8888;
    return SDL_PIXELFORMAT_UNKNOWN;
}

SDL_Surface* SDL_CreateSurfaceFrom(int width, int height, SDL_PixelFormat format, void* pixels,
                                   int pitch) {
    SDL_Surface* surface = calloc(1, sizeof *surface);
    if (surface == NULL)
        return NULL;
    surface->w = width;
    surface->h = height;
    surface->format = format;
    surface->pixels = pixels;
    surface->pitch = pitch;
    return surface;
}

void SDL_DestroySurface(SDL_Surface* surface) { free(surface); }
