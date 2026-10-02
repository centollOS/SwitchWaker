// SDL 3 video for the Switch shim: one fixed 1280x720 full-screen window.
// Aurora renders offscreen through Dawn on the Switch and presents through the
// libnx framebuffer, so the window only answers size and state queries. There
// is no SDL renderer: creating one fails, which Aurora's ImGui path tolerates.
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "sdl3_shim.h"

struct SDL_Window {
    SDL_PropertiesID properties;
};

static struct SDL_Window g_window;
static bool g_window_open;

SDL_Window* SDL_CreateWindowWithProperties(SDL_PropertiesID props) {
    (void)props;
    if (g_window_open) {
        sdl3_shim_set_error("the Switch has one window");
        return NULL;
    }
    g_window.properties = SDL_CreateProperties();
    g_window_open = true;
    return &g_window;
}

void SDL_DestroyWindow(SDL_Window* window) {
    if (window == &g_window)
        g_window_open = false;
}

SDL_PropertiesID SDL_GetWindowProperties(SDL_Window* window) {
    return window != NULL ? window->properties : 0;
}

bool SDL_GetWindowSize(SDL_Window* window, int* w, int* h) {
    (void)window;
    if (w != NULL)
        *w = SDL3_SHIM_WIDTH;
    if (h != NULL)
        *h = SDL3_SHIM_HEIGHT;
    return true;
}

bool SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h) {
    return SDL_GetWindowSize(window, w, h);
}

float SDL_GetWindowDisplayScale(SDL_Window* window) {
    (void)window;
    return 1.0f;
}

SDL_WindowFlags SDL_GetWindowFlags(SDL_Window* window) {
    (void)window;
    return SDL_WINDOW_FULLSCREEN | SDL_WINDOW_INPUT_FOCUS;
}

// The window's state cannot change: these succeed without effect.
bool SDL_ShowWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_RaiseWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_RestoreWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_SyncWindow(SDL_Window* window) { (void)window; return true; }
bool SDL_SetWindowFullscreen(SDL_Window* window, bool fullscreen) {
    (void)window;
    (void)fullscreen;
    return true;
}
bool SDL_SetWindowIcon(SDL_Window* window, SDL_Surface* icon) {
    (void)window;
    (void)icon;
    return true;
}
bool SDL_SetWindowMinimumSize(SDL_Window* window, int min_w, int min_h) {
    (void)window;
    (void)min_w;
    (void)min_h;
    return true;
}
bool SDL_SetWindowPosition(SDL_Window* window, int x, int y) {
    (void)window;
    (void)x;
    (void)y;
    return true;
}
bool SDL_SetWindowSize(SDL_Window* window, int w, int h) {
    (void)window;
    (void)w;
    (void)h;
    return true;
}
bool SDL_SetWindowTitle(SDL_Window* window, const char* title) {
    (void)window;
    (void)title;
    return true;
}
bool SDL_DisableScreenSaver(void) { return true; }
bool SDL_EnableScreenSaver(void) { return true; }

const char* SDL_GetCurrentVideoDriver(void) { return "switch"; }
SDL_DisplayID SDL_GetPrimaryDisplay(void) { return 1; }
SDL_DisplayOrientation SDL_GetCurrentDisplayOrientation(SDL_DisplayID displayID) {
    (void)displayID;
    return SDL_ORIENTATION_LANDSCAPE;
}
SDL_DisplayOrientation SDL_GetNaturalDisplayOrientation(SDL_DisplayID displayID) {
    (void)displayID;
    return SDL_ORIENTATION_LANDSCAPE;
}

// No SDL renderer on the Switch.
SDL_Renderer* SDL_CreateRendererWithProperties(SDL_PropertiesID props) {
    (void)props;
    sdl3_shim_set_error("no SDL renderer on the Switch");
    return NULL;
}
void SDL_DestroyRenderer(SDL_Renderer* renderer) { (void)renderer; }
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, SDL_PixelFormat format,
                               SDL_TextureAccess access, int w, int h) {
    (void)renderer;
    (void)format;
    (void)access;
    (void)w;
    (void)h;
    return NULL;
}
void SDL_DestroyTexture(SDL_Texture* texture) { (void)texture; }
bool SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch) {
    (void)texture;
    (void)rect;
    (void)pixels;
    (void)pitch;
    return false;
}
bool SDL_SetTextureScaleMode(SDL_Texture* texture, SDL_ScaleMode scaleMode) {
    (void)texture;
    (void)scaleMode;
    return false;
}
bool SDL_GetRenderOutputSize(SDL_Renderer* renderer, int* w, int* h) {
    (void)renderer;
    return SDL_GetWindowSize(NULL, w, h);
}
bool SDL_GetRenderScale(SDL_Renderer* renderer, float* scaleX, float* scaleY) {
    (void)renderer;
    if (scaleX != NULL)
        *scaleX = 1.0f;
    if (scaleY != NULL)
        *scaleY = 1.0f;
    return true;
}
bool SDL_SetRenderScale(SDL_Renderer* renderer, float scaleX, float scaleY) {
    (void)renderer;
    (void)scaleX;
    (void)scaleY;
    return false;
}
bool SDL_SetRenderLogicalPresentation(SDL_Renderer* renderer, int w, int h,
                                      SDL_RendererLogicalPresentation mode) {
    (void)renderer;
    (void)w;
    (void)h;
    (void)mode;
    return false;
}
bool SDL_RenderClear(SDL_Renderer* renderer) { (void)renderer; return false; }
bool SDL_RenderPresent(SDL_Renderer* renderer) { (void)renderer; return false; }
bool SDL_ConvertEventToRenderCoordinates(SDL_Renderer* renderer, SDL_Event* event) {
    (void)renderer;
    (void)event;
    return true;
}
