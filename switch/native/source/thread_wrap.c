// Every pthread of the native port on the Switch (the NRO links with -Wl,--wrap=pthread_create and
// --wrap=pthread_detach):
//
// - detached threads are reaped: libnx does not implement pthread_detach (newlib.c's
//   __syscall_thread_detach), so a detached pthread that ends keeps its stack (at least 4 MiB, below)
//   and its kernel thread object for good, and once the process's thread limit is reached
//   pthread_create fails with ENOMEM. The SDK's OSThread makes every game thread detached, and the
//   game makes one per picto box photo (m_Do_graphic.cpp's capture thread): the crash after a photo
//   in a long session. pthread_detach hands the thread to a reaper thread, which joins it once its
//   handle signals (the thread ended). Only pthread_detach counts: newlib's pthread_create ignores
//   the attribute's detach state, and its PTHREAD_CREATE_DETACHED is 0, the value of every fresh
//   attribute, so the attribute cannot tell a detached thread from a joinable one;
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
// - Aurora's pipeline compile thread (COS_SWITCH_THREAD_COMPILE, named by Aurora's Switch patch
//   0010) never shares the render worker's core: during the loading screen and the warm-up the two
//   would otherwise take turns on one core, as Horizon does not time-slice equal priorities;
// - the thread table of the perf-switch lines (cos_switch_thread_table): each busy thread's role,
//   preferred core, affinity mask, CPU time and entry point (an offset for addr2line with switchwaker.elf).
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "cos_switch_internal.h"

#define MINIMUM_STACK_SIZE (4u * 1024u * 1024u)
#define THREAD_PRIORITY 0x2C

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);
int __real_pthread_detach(pthread_t thread);
int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);

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

// The compile thread on the application's cores except the render worker's, preferring core 1 or 2.
static void place_compile_thread(Handle handle) {
    const int render = atomic_load(&g_render_core);
    if (render < 0)
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
    if (role == COS_SWITCH_THREAD_RENDER) {
        s32 ideal = -1;
        u64 mask = 0;
        svcGetThreadCoreMask(&ideal, &mask, CUR_THREAD_HANDLE);
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

// ---- reaper of detached threads -------------------------------------------------------------------
// newlib's pthread_t is libnx's struct __pthread_t*, whose first member is the libnx Thread.
#define MAX_REAP 64 // svcWaitSynchronization's limit

static Mutex g_reap_lock;
static pthread_t g_reap[MAX_REAP];
static int g_reap_count;
static atomic_uint g_reaped;
static atomic_int g_reaper_started;

static Handle thread_handle(pthread_t t) { return ((const Thread*)t)->handle; }

// The process's threads in use and its limit (kernel resource limit), -1 if unknown.
static void thread_limit(s64* used, s64* limit) {
    *used = *limit = -1;
    u64 rl = 0;
    if (R_FAILED(svcGetInfo(&rl, InfoType_ResourceLimit, INVALID_HANDLE, 0)) || rl == 0)
        return;
    svcGetResourceLimitCurrentValue(used, (Handle)rl, LimitableResource_Threads);
    svcGetResourceLimitLimitValue(limit, (Handle)rl, LimitableResource_Threads);
    svcCloseHandle((Handle)rl);
}

static void* reaper(void* unused) {
    (void)unused;
    for (;;) {
        Handle handles[MAX_REAP];
        pthread_t threads[MAX_REAP];
        mutexLock(&g_reap_lock);
        const int n = g_reap_count;
        for (int i = 0; i < n; i++) {
            threads[i] = g_reap[i];
            handles[i] = thread_handle(g_reap[i]);
        }
        mutexUnlock(&g_reap_lock);
        if (n == 0) {
            svcSleepThread(100000000ull);
            continue;
        }
        s32 index = -1;
        // 100 ms: new threads to watch are picked up at the next round
        const Result rc = svcWaitSynchronization(&index, handles, n, 100000000ull);
        if (R_VALUE(rc) == KERNELRESULT(InvalidHandle)) {
            // a thread that is no longer one (it should not happen): dropped, so the others are still reaped
            for (int i = 0; i < n; i++) {
                s32 one = -1;
                if (R_VALUE(svcWaitSynchronization(&one, &handles[i], 1, 0)) != KERNELRESULT(InvalidHandle))
                    continue;
                fprintf(stderr, "[switch] reaper: pthread %p has no valid handle (0x%x): dropped\n", (void*)threads[i],
                        (unsigned)handles[i]);
                mutexLock(&g_reap_lock);
                for (int j = 0; j < g_reap_count; j++) {
                    if (g_reap[j] == threads[i]) {
                        g_reap[j] = g_reap[--g_reap_count];
                        break;
                    }
                }
                mutexUnlock(&g_reap_lock);
            }
            continue;
        }
        if (R_FAILED(rc) || index < 0 || index >= n)
            continue;
        const pthread_t done = threads[index];
        mutexLock(&g_reap_lock);
        for (int i = 0; i < g_reap_count; i++) {
            if (g_reap[i] == done) {
                g_reap[i] = g_reap[--g_reap_count];
                break;
            }
        }
        mutexUnlock(&g_reap_lock);
        // the registry keeps the thread's CPU time; its handle is closed below and may be reused
        const int count = atomic_load(&g_thread_count);
        for (int i = 0; i < count; i++) {
            if (g_threads[i].handle == handles[index]) {
                u64 ticks = 0;
                if (R_SUCCEEDED(svcGetInfo(&ticks, InfoType_ThreadTickCount, handles[index], UINT64_MAX)))
                    atomic_store(&g_threads[i].ticks, ticks);
                g_threads[i].handle = INVALID_HANDLE;
            }
        }
        pthread_join(done, NULL); // the thread has ended: frees its stack and closes its handle
        const unsigned reaped = atomic_fetch_add(&g_reaped, 1) + 1;
        if (reaped <= 8 || reaped % 16 == 0) {
            s64 used, limit;
            thread_limit(&used, &limit);
            fprintf(stderr, "[switch] ended detached thread joined (%u so far); threads in use %lld of %lld\n",
                    reaped, (long long)used, (long long)limit);
        }
    }
    return NULL;
}

// false: the list is full (the thread is then left detached, as before)
static bool reap_add(pthread_t t) {
    if (atomic_exchange(&g_reaper_started, 1) == 0) {
        pthread_t r;
        if (__wrap_pthread_create(&r, NULL, reaper, NULL) != 0)
            fprintf(stderr, "[switch] the reaper of detached threads could not start\n");
    }
    mutexLock(&g_reap_lock);
    for (int i = 0; i < g_reap_count; i++) {
        if (g_reap[i] == t) { // detached twice
            mutexUnlock(&g_reap_lock);
            return true;
        }
    }
    const bool ok = g_reap_count < MAX_REAP;
    if (ok)
        g_reap[g_reap_count++] = t;
    mutexUnlock(&g_reap_lock);
    if (!ok) {
        fprintf(stderr, "[switch] %d detached threads already wait to be joined: one more is left as it is\n",
                MAX_REAP);
        static int dumped;
        if (!dumped++) {
            mutexLock(&g_reap_lock);
            for (int i = 0; i < g_reap_count; i++) {
                const Handle h = thread_handle(g_reap[i]);
                uintptr_t fn = 0;
                for (int j = 0; j < atomic_load(&g_thread_count); j++)
                    if (g_threads[j].handle == h)
                        fn = atomic_load(&g_threads[j].start);
                fprintf(stderr, "[switch]   waiting %d: pthread %p handle 0x%x fn 0x%llx\n", i, (void*)g_reap[i],
                        (unsigned)h, (unsigned long long)(fn ? fn - cos_switch_image_base() : 0));
            }
            mutexUnlock(&g_reap_lock);
        }
    }
    return ok;
}

int __wrap_pthread_detach(pthread_t thread) {
    if (reap_add(thread))
        return 0;
    return __real_pthread_detach(thread);
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
    if (result != 0) {
        free(t);
        s64 used, limit;
        thread_limit(&used, &limit);
        fprintf(stderr, "[switch] pthread_create failed (%d) after %u threads, %u joined; threads in use %lld of %lld\n",
                result, atomic_load(&g_created), atomic_load(&g_reaped), (long long)used, (long long)limit);
    } else {
        atomic_fetch_add(&g_created, 1);
    }
    if (attr == NULL)
        pthread_attr_destroy(&sized);
    return result;
}
