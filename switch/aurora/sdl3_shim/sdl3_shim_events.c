// SDL 3 events for the Switch shim: a bounded queue, event watches, user
// event registration, and a pump that turns libnx applet and controller state
// into the SDL events Aurora handles (quit, gamepad added/removed).
#include <SDL3/SDL.h>
#include <string.h>
#include <switch.h>

#include "sdl3_shim.h"

#define QUEUE_SIZE 256
#define MAX_WATCHES 8

static SDL_Event g_queue[QUEUE_SIZE];
static int g_head;
static int g_count;
static Mutex g_lock;
static struct {
    SDL_EventFilter filter;
    void* userdata;
} g_watches[MAX_WATCHES];
static Uint32 g_next_user_event = SDL_EVENT_USER;
static bool g_quit_sent;

void sdl3_shim_queue_event(const SDL_Event* event) {
    for (int i = 0; i < MAX_WATCHES; ++i) {
        if (g_watches[i].filter != NULL)
            g_watches[i].filter(g_watches[i].userdata, (SDL_Event*)event);
    }
    mutexLock(&g_lock);
    if (g_count < QUEUE_SIZE) {
        g_queue[(g_head + g_count) % QUEUE_SIZE] = *event;
        ++g_count;
    }
    mutexUnlock(&g_lock);
}

static void pump(void) {
    // appletMainLoop services HOME/sleep and returns false once the system
    // asks the application to exit.
    if (!g_quit_sent && !appletMainLoop()) {
        SDL_Event quit;
        memset(&quit, 0, sizeof quit);
        quit.type = SDL_EVENT_QUIT;
        quit.common.timestamp = SDL_GetTicks() * 1000000ULL;
        g_quit_sent = true;
        sdl3_shim_queue_event(&quit);
    }
    sdl3_shim_gamepad_pump();
}

bool SDL_PollEvent(SDL_Event* event) {
    pump();
    mutexLock(&g_lock);
    const bool available = g_count > 0;
    if (available) {
        if (event != NULL)
            *event = g_queue[g_head];
        g_head = (g_head + 1) % QUEUE_SIZE;
        --g_count;
    }
    mutexUnlock(&g_lock);
    return available;
}

bool SDL_WaitEvent(SDL_Event* event) {
    while (!SDL_PollEvent(event))
        svcSleepThread(1000000);
    return true;
}

bool SDL_PushEvent(SDL_Event* event) {
    if (event == NULL)
        return false;
    sdl3_shim_queue_event(event);
    return true;
}

Uint32 SDL_RegisterEvents(int numevents) {
    mutexLock(&g_lock);
    Uint32 first = 0;
    if (numevents > 0 && g_next_user_event + (Uint32)numevents <= SDL_EVENT_LAST) {
        first = g_next_user_event;
        g_next_user_event += (Uint32)numevents;
    }
    mutexUnlock(&g_lock);
    return first;
}

bool SDL_AddEventWatch(SDL_EventFilter filter, void* userdata) {
    for (int i = 0; i < MAX_WATCHES; ++i) {
        if (g_watches[i].filter == NULL) {
            g_watches[i].filter = filter;
            g_watches[i].userdata = userdata;
            return true;
        }
    }
    sdl3_shim_set_error("too many event watches");
    return false;
}

void SDL_RemoveEventWatch(SDL_EventFilter filter, void* userdata) {
    for (int i = 0; i < MAX_WATCHES; ++i) {
        if (g_watches[i].filter == filter && g_watches[i].userdata == userdata)
            g_watches[i].filter = NULL;
    }
}
