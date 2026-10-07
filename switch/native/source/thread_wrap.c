// Every pthread of the native port on the Switch (the NRO links with -Wl,--wrap=pthread_create):
//
// - a stack of at least 4 MiB, page-aligned: libnx gives a pthread 128 KiB by default, which
//   Tint (Dawn's shader compiler) overflows, and refuses a size that is not a multiple of 4 KiB
//   (switch/host/source/thread_stack.c is the translated port's version of this);
// - the application cores: libnx creates pthreads on the process's default core with priority
//   0x3B, below the main thread's 0x2C, so every thread (the game, JAudio's, the DVD thread,
//   Aurora's render worker, Dawn's) would share core 0 and the helpers would only run when the
//   game thread waits. Here each new thread prefers core 1 or 2 in turn (the game thread core 0,
//   see cos_switch_main.cpp), may run on any core the process has among 0-2, and gets 0x2C;
// - a registry of the threads' kernel handles, for their CPU time (svcGetInfo ThreadTickCount)
//   per role in the perf-switch and hitch lines (cos_switch_thread_role, cos_switch_thread_cpu_ns);
// - COS_SWITCH_CORES=pinned (env.txt; off by default): when the render worker, JAudio's audio
//   thread and the game's DVD thread name their role, the render worker is pinned to core 2 alone
//   and the audio and DVD threads to core 1, so the worker never waits behind them (Horizon does not
//   time-slice threads of equal priority). The other threads keep the default above;
// - COS_SWITCH_CORES=isolate: the render worker alone on core 2 and every other thread (the game's
//   included, which keeps core 0 as its preferred core) on cores 0-1, applied to the threads that
//   exist when the render worker names itself and to every thread created later;
// - Aurora's pipeline compile thread (COS_SWITCH_THREAD_COMPILE, named by Aurora's Switch patch
//   0010) never shares the render worker's core (COS_SWITCH_COMPILE_CORE=auto, the default; =off
//   leaves it where the default put it): during the loading screen and the warm-up the two would
//   otherwise take turns on one core, as Horizon does not time-slice equal priorities;
// - the thread table of the perf-switch lines (cos_switch_thread_table): each busy thread's role,
//   preferred core, affinity mask, CPU time and entry point (an offset for addr2line with switchwaker.elf).
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "cos_switch_internal.h"

#define MINIMUM_STACK_SIZE (4u * 1024u * 1024u)
#define THREAD_PRIORITY 0x2C

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);

static atomic_int g_next_core = 1;
static atomic_int g_forced_core = -1; // cos_switch_next_thread_core: the next thread only
static atomic_uint g_created;

static u32 application_core_mask(void);

// ---- thread registry ------------------------------------------------------------------------------
#define MAX_THREADS 96

struct ThreadEntry {
    Handle handle;
    atomic_int role;              // COS_SWITCH_THREAD_*
    _Atomic unsigned long long ticks; // last ThreadTickCount read (kept once the thread is gone)
    _Atomic uintptr_t start;      // the entry point given to pthread_create (0: not ours)
    unsigned long long tableTicks; // ticks at the last cos_switch_thread_table (its caller only)
};

// The calling thread's role, for nv_wrap.c's per-thread buckets.
static _Thread_local int t_role = COS_SWITCH_THREAD_OTHER;
int cos_switch_thread_is_render(void) { return t_role == COS_SWITCH_THREAD_RENDER; }

// COS_SWITCH_CORES: 0 default (spread), 1 pinned, 2 isolate.
static int cores_policy(void) {
    static int policy = -1;
    if (policy < 0) {
        const char* v = getenv("COS_SWITCH_CORES");
        policy = v == NULL ? 0 : strcmp(v, "pinned") == 0 ? 1 : strcmp(v, "isolate") == 0 ? 2 : 0;
    }
    return policy;
}
// The render worker's preferred core once it named itself (-1 before).
static atomic_int g_render_core = -1;
static u32 application_core_mask(void);

static struct ThreadEntry g_threads[MAX_THREADS];
static atomic_int g_thread_count;
static Mutex g_thread_lock;

static struct ThreadEntry* find_or_add_current(void) {
    const Handle self = threadGetCurHandle();
    const int count = atomic_load(&g_thread_count);
    for (int i = 0; i < count; i++) {
        if (g_threads[i].handle == self)
            return &g_threads[i];
    }
    mutexLock(&g_thread_lock);
    struct ThreadEntry* entry = NULL;
    const int n = atomic_load(&g_thread_count);
    if (n < MAX_THREADS) {
        entry = &g_threads[n];
        entry->handle = self;
        atomic_store(&entry->role, COS_SWITCH_THREAD_OTHER);
        atomic_store(&entry->ticks, 0);
        atomic_store(&entry->start, 0);
        entry->tableTicks = 0;
        atomic_store(&g_thread_count, n + 1);
    }
    mutexUnlock(&g_thread_lock);
    return entry;
}

// COS_SWITCH_CORES=pinned: the core of a thread of this role, or -1 to leave it as it is.
static int pinned_core(int role) {
    if (cores_policy() != 1)
        return -1;
    switch (role) {
    case COS_SWITCH_THREAD_RENDER: return 2;
    case COS_SWITCH_THREAD_AUDIO:
    case COS_SWITCH_THREAD_DVD: return 1;
    default: return -1;
    }
}

static const char* role_name(int role) {
    switch (role) {
    case COS_SWITCH_THREAD_GAME: return "game";
    case COS_SWITCH_THREAD_RENDER: return "render";
    case COS_SWITCH_THREAD_AUDIO: return "audio";
    case COS_SWITCH_THREAD_DVD: return "dvd";
    case COS_SWITCH_THREAD_COMPILE: return "compile";
    default: return "other";
    }
}

// Moves a thread to `mask` (within the application's cores), keeping its preferred core if the
// mask has it, else the lowest core of `prefer_mask & mask`, else of the mask.
static Result set_mask(Handle handle, u32 mask, u32 prefer_mask) {
    mask &= application_core_mask();
    if (mask == 0)
        return 0;
    s32 ideal = -1;
    u64 old_mask = 0;
    svcGetThreadCoreMask(&ideal, &old_mask, handle);
    if (ideal < 0 || (mask & (1u << ideal)) == 0)
        ideal = (prefer_mask & mask) != 0 ? __builtin_ctz(prefer_mask & mask) : __builtin_ctz(mask);
    return svcSetThreadCoreMask(handle, ideal, mask);
}

// COS_SWITCH_COMPILE_CORE=auto (default): the compile thread on the application's cores except the
// render worker's, preferring core 1 or 2.
static void place_compile_thread(Handle handle) {
    static int on = -1;
    if (on < 0) {
        const char* v = getenv("COS_SWITCH_COMPILE_CORE");
        on = v == NULL || strcmp(v, "off") != 0;
    }
    const int render = atomic_load(&g_render_core);
    if (!on || render < 0 || cores_policy() == 2)
        return;
    const u32 mask = application_core_mask() & ~(1u << render);
    const Result rc = set_mask(handle, mask, 0x6);
    fprintf(stderr, "[switch] compile thread kept off the render worker's core %d (mask 0x%x)%s\n", render,
            (unsigned)mask, R_SUCCEEDED(rc) ? "" : " (refused)");
}

void cos_switch_thread_role(int role) {
    struct ThreadEntry* entry = find_or_add_current();
    if (entry != NULL)
        atomic_store(&entry->role, role);
    t_role = role;
    const int core = pinned_core(role);
    if (core >= 0 && (application_core_mask() & (1u << core)) != 0) {
        const Result rc = svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
        fprintf(stderr, "[switch] COS_SWITCH_CORES=pinned: %s thread on core %d only%s\n",
                role == COS_SWITCH_THREAD_RENDER  ? "render worker"
                : role == COS_SWITCH_THREAD_AUDIO ? "audio"
                                                  : "dvd",
                core, R_SUCCEEDED(rc) ? "" : " (refused)");
    }
    if (role == COS_SWITCH_THREAD_RENDER) {
        s32 ideal = -1;
        u64 mask = 0;
        svcGetThreadCoreMask(&ideal, &mask, CUR_THREAD_HANDLE);
        if (cores_policy() == 2 && (application_core_mask() & 0x4) != 0) {
            // isolate: the worker alone on core 2, every other thread (now and later) on 0-1.
            svcSetThreadCoreMask(CUR_THREAD_HANDLE, 2, 0x4);
            ideal = 2;
            const int count = atomic_load(&g_thread_count);
            int moved = 0;
            for (int i = 0; i < count; i++) {
                struct ThreadEntry* e = &g_threads[i];
                if (e == entry)
                    continue;
                if (R_SUCCEEDED(set_mask(e->handle, 0x3, 0x2)))
                    moved++;
            }
            fprintf(stderr, "[switch] COS_SWITCH_CORES=isolate: render worker alone on core 2, %d other threads on "
                            "cores 0-1\n", moved);
        }
        atomic_store(&g_render_core, ideal);
        // A compile thread that named itself first.
        const int count = atomic_load(&g_thread_count);
        for (int i = 0; i < count; i++) {
            if (atomic_load(&g_threads[i].role) == COS_SWITCH_THREAD_COMPILE)
                place_compile_thread(g_threads[i].handle);
        }
    } else if (role == COS_SWITCH_THREAD_COMPILE) {
        place_compile_thread(CUR_THREAD_HANDLE);
    }
}

// Aurora's Switch patch 0010 calls this on its pipeline compile thread when it starts.
void aurora_switch_pipeline_thread_started(void) { cos_switch_thread_role(COS_SWITCH_THREAD_COMPILE); }

int cos_switch_thread_table(char* out, size_t size, double frames) {
    size_t used = 0;
    out[0] = '\0';
    if (frames <= 0)
        frames = 1;
    const uintptr_t base = cos_switch_image_base();
    const int count = atomic_load(&g_thread_count);
    for (int i = 0; i < count && used + 64 < size; i++) {
        struct ThreadEntry* e = &g_threads[i];
        u64 ticks = 0;
        if (R_SUCCEEDED(svcGetInfo(&ticks, InfoType_ThreadTickCount, e->handle, UINT64_MAX)))
            atomic_store(&e->ticks, ticks);
        else
            ticks = atomic_load(&e->ticks);
        const double ms = armTicksToNs(ticks - e->tableTicks) / 1e6 / frames;
        e->tableTicks = ticks;
        if (ms < 0.3)
            continue;
        s32 ideal = -1;
        u64 mask = 0;
        svcGetThreadCoreMask(&ideal, &mask, e->handle);
        const uintptr_t start = atomic_load(&e->start);
        int n = snprintf(out + used, size - used, "%s%s#%d c%d/0x%llx %.1f", used ? ", " : "",
                         role_name(atomic_load(&e->role)), i, (int)ideal, (unsigned long long)mask, ms);
        if (n > 0 && (size_t)n < size - used)
            used += (size_t)n;
        if (start != 0 && atomic_load(&e->role) == COS_SWITCH_THREAD_OTHER) {
            n = snprintf(out + used, size - used, " (fn 0x%llx)", (unsigned long long)(start - base));
            if (n > 0 && (size_t)n < size - used)
                used += (size_t)n;
        }
    }
    return (int)used;
}

void cos_switch_thread_cpu_ns(uint64_t out[COS_SWITCH_THREAD_ROLES]) {
    for (int r = 0; r < COS_SWITCH_THREAD_ROLES; r++)
        out[r] = 0;
    const int count = atomic_load(&g_thread_count);
    for (int i = 0; i < count; i++) {
        struct ThreadEntry* entry = &g_threads[i];
        u64 ticks = 0;
        // Subtype -1: the thread's time on all cores. Fails once the thread has ended.
        if (R_SUCCEEDED(svcGetInfo(&ticks, InfoType_ThreadTickCount, entry->handle, UINT64_MAX)))
            atomic_store(&entry->ticks, ticks);
        else
            ticks = atomic_load(&entry->ticks);
        const int role = atomic_load(&entry->role);
        out[role >= 0 && role < COS_SWITCH_THREAD_ROLES ? role : 0] += armTicksToNs(ticks);
    }
}

struct Trampoline {
    void* (*start)(void*);
    void* arg;
    int core;
};

static u32 application_core_mask(void) {
    static u32 mask;
    if (mask == 0) {
        u64 process_mask = 0;
        if (R_FAILED(svcGetInfo(&process_mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0)))
            process_mask = 0x7;
        mask = (u32)process_mask & 0x7;
        if (mask == 0)
            mask = 0x1;
    }
    return mask;
}

static void* trampoline(void* raw) {
    struct Trampoline t = *(struct Trampoline*)raw;
    free(raw);
    u32 mask = application_core_mask();
    int core = t.core;
    if (cores_policy() == 2 && atomic_load(&g_render_core) >= 0 && (mask & 0x3) != 0) {
        // isolate, after the render worker took core 2: cores 0-1 only.
        mask &= 0x3;
        if (core == 2)
            core = 1;
    }
    if ((mask & (1u << core)) == 0)
        core = __builtin_ctz(mask);
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, mask);
    svcSetThreadPriority(CUR_THREAD_HANDLE, THREAD_PRIORITY);
    struct ThreadEntry* entry = find_or_add_current();
    if (entry != NULL)
        atomic_store(&entry->start, (uintptr_t)t.start);
    return t.start(t.arg);
}

void cos_switch_next_thread_core(int core) { atomic_store(&g_forced_core, core); }

unsigned cos_switch_threads_created(void) { return atomic_load(&g_created); }

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg) {
    pthread_attr_t sized;
    size_t requested = 0;
    if (attr != NULL) {
        sized = *attr;
        if (pthread_attr_getstacksize(attr, &requested) != 0)
            requested = 0;
    } else {
        pthread_attr_init(&sized);
    }
    size_t size = requested < MINIMUM_STACK_SIZE ? MINIMUM_STACK_SIZE : requested;
    size = (size + 0xFFF) & ~(size_t)0xFFF;
    pthread_attr_setstacksize(&sized, size);

    struct Trampoline* t = malloc(sizeof *t);
    if (t == NULL) {
        if (attr == NULL)
            pthread_attr_destroy(&sized);
        return __real_pthread_create(thread, attr, start, arg);
    }
    t->start = start;
    t->arg = arg;
    const int forced = atomic_exchange(&g_forced_core, -1);
    if (forced >= 0) {
        t->core = forced;
    } else {
        // 1, 2, 1, 2...
        t->core = 1 + (atomic_fetch_add(&g_next_core, 1) - 1) % 2;
    }
    const int result = __real_pthread_create(thread, &sized, trampoline, t);
    if (result != 0)
        free(t);
    else
        atomic_fetch_add(&g_created, 1);
    if (attr == NULL)
        pthread_attr_destroy(&sized);
    return result;
}
