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
//   per role in the perf-switch and hitch lines (cos_switch_thread_role, cos_switch_thread_cpu_ns).
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>

#include "cos_switch_internal.h"

#define MINIMUM_STACK_SIZE (4u * 1024u * 1024u)
#define THREAD_PRIORITY 0x2C

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);

static atomic_int g_next_core = 1;
static atomic_int g_forced_core = -1; // cos_switch_next_thread_core: the next thread only
static atomic_uint g_created;

// ---- thread registry ------------------------------------------------------------------------------
#define MAX_THREADS 96

struct ThreadEntry {
    Handle handle;
    atomic_int role;              // COS_SWITCH_THREAD_*
    _Atomic unsigned long long ticks; // last ThreadTickCount read (kept once the thread is gone)
};

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
        atomic_store(&g_thread_count, n + 1);
    }
    mutexUnlock(&g_thread_lock);
    return entry;
}

void cos_switch_thread_role(int role) {
    struct ThreadEntry* entry = find_or_add_current();
    if (entry != NULL)
        atomic_store(&entry->role, role);
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
    const u32 mask = application_core_mask();
    int core = t.core;
    if ((mask & (1u << core)) == 0)
        core = __builtin_ctz(mask);
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, mask);
    svcSetThreadPriority(CUR_THREAD_HANDLE, THREAD_PRIORITY);
    find_or_add_current();
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
