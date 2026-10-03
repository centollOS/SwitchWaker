// cos_sdk: OSThread on host threads, plus the shared OS lock, logging and the hooks of
// cos_sdk/hooks.h (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a; the model is in os_internal.h).
//
// Provenance: adapted from Dusklight src/dusk/OSThread.cpp (CC0, ref/dusklight). Kept from it: the
// side table of host data per OSThread, the default (main) thread with a dummy stack whose
// stackBase/stackEnd JKRThread reads, GameCube-style creation (suspended until OSResumeThread, the
// host thread is launched by the first resume), self-suspension that blocks, and thread-specific
// storage. Changed:
// - all OS state is guarded by one lock, the "interrupts disabled" lock of OSInterrupt.cpp, instead
//   of separate maps and per-queue condition variables that game-visible fields were written
//   outside of (TSan-clean);
// - OSSleepThread/OSWakeupThread, OSJoinThread, OSExitThread, OSDetachThread and OSCancelThread
//   follow the GameCube SDK's algorithms (thread queues in priority order, the active-thread list,
//   joinable vs detached, mutexes released on exit) instead of being no-ops or CRASH();
// - threads are detached pthreads, so OSExitThread can end the calling thread (pthread_exit);
//   OSJoinThread waits on OS state and stores the full 64-bit exit value;
// - OSSuspendThread/OSCancelThread on another running thread act at its next SDK call (logged);
// - threads not made by OSCreateThread (host threads) get their own OSThread instead of sharing
//   the default thread;
// - dusk::IsShuttingDown, JKRHeap and Tracy couplings are replaced by cos_sdk/hooks.h;
// - OSSetCurrentThreadName (Dusklight-only) is not part of this file.
#include "os_internal.h"

#include "cos_sdk/hooks.h"

#include <pthread.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <unordered_map>

namespace cos_sdk::os {

// ---------------------------------------------------------------------------------------------
// Shared state

std::mutex& Lock() {
    static std::mutex* const sLock = new std::mutex(); // never destroyed
    return *sLock;
}

thread_local bool tInterruptsDisabled = false;
thread_local OSThread* tCurrent = nullptr;
thread_local HostThread* tHost = nullptr;

namespace {

// Keeps the HostThread of an OSCreateThread thread alive until the host thread ends. Thread-local
// destructors run at pthread_exit too.
thread_local std::shared_ptr<HostThread> tHostRef;

using HostMap = std::unordered_map<OSThread*, std::shared_ptr<HostThread>>;
HostMap& Hosts() {
    static HostMap* const sHosts = new HostMap(); // never destroyed
    return *sHosts;
}

// The GameCube's default thread: the thread that first calls the SDK (the main thread).
OSThread sDefaultThread;
u8 sDefaultStack[64 * 1024];
bool sDefaultClaimed = false;

// __OSActiveThreadQueue (linked through OSThread::linkActive).
OSThreadQueue sActiveQueue = {nullptr, nullptr};

s32 sSchedulerSuspendCount = 0;
OSSwitchThreadCallback sSwitchThreadCallback = nullptr;
COSSdkThreadStartHook sThreadStartHook = nullptr;
bool sShuttingDown = false;
bool sLoggedSwitchCallback = false;
bool sLoggedIdleFunction = false;

// Host stack for OSCreateThread threads. The game's stack buffer is only bookkeeping (stackBase,
// stackEnd and the magic word JKRThread checks); 64-bit code needs more room than the GameCube
// sizes, so the host stack is at least 1 MiB and 4x the requested size.
size_t HostStackSize(u32 requested) {
    size_t size = static_cast<size_t>(requested) * 4;
    if (size < 1024 * 1024) {
        size = 1024 * 1024;
    }
    return (size + 0x3FFF) & ~static_cast<size_t>(0x3FFF);
}

void ActiveAdd(OSThread* thread) {
    OSThread* prev = sActiveQueue.tail;
    if (prev == nullptr) {
        sActiveQueue.head = thread;
    } else {
        prev->linkActive.next = thread;
    }
    thread->linkActive.prev = prev;
    thread->linkActive.next = nullptr;
    sActiveQueue.tail = thread;
}

bool ActiveContains(OSThread* thread) {
    for (OSThread* t = sActiveQueue.head; t != nullptr; t = t->linkActive.next) {
        if (t == thread) {
            return true;
        }
    }
    return false;
}

void ActiveRemove(OSThread* thread) {
    if (!ActiveContains(thread)) {
        return;
    }
    OSThread* next = thread->linkActive.next;
    OSThread* prev = thread->linkActive.prev;
    if (next == nullptr) {
        sActiveQueue.tail = prev;
    } else {
        next->linkActive.prev = prev;
    }
    if (prev == nullptr) {
        sActiveQueue.head = next;
    } else {
        prev->linkActive.next = next;
    }
    thread->linkActive.next = thread->linkActive.prev = nullptr;
}

// Fields every new thread record starts with (OSCreateThread and the default thread).
void InitRecord(OSThread* thread, OSPriority priority, u16 attr) {
    std::memset(thread, 0, sizeof(OSThread));
    thread->attr = attr;
    thread->priority = priority;
    thread->base = priority;
    thread->val = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
    OSInitThreadQueue(&thread->queueJoin);
}

void WriteStackMagic(u8* where) {
    const u32 magic = OS_THREAD_STACK_MAGIC;
    std::memcpy(where, &magic, sizeof(magic));
}

// A host thread that calls the SDK without having been made by OSCreateThread (any thread after
// the first). Its record lives as long as the host thread.
struct ForeignThread {
    OSThread record;
    std::shared_ptr<HostThread> host = std::make_shared<HostThread>();

    ~ForeignThread() {
        if (tInterruptsDisabled) {
            // The host thread ends with interrupts disabled: give the lock back for the others.
            Hosts().erase(&record);
            tInterruptsDisabled = false;
            Lock().unlock();
            Log("host thread %p ended with interrupts disabled", static_cast<void*>(&record));
            return;
        }
        std::lock_guard<std::mutex> lock(Lock());
        Hosts().erase(&record);
    }
};
thread_local std::unique_ptr<ForeignThread> tForeign;

void ClaimDefaultThreadLocked() {
    InitRecord(&sDefaultThread, 16, OS_THREAD_ATTR_DETACH);
    sDefaultThread.state = OS_THREAD_STATE_RUNNING;
    sDefaultThread.stackBase = sDefaultStack + sizeof(sDefaultStack);
    sDefaultThread.stackEnd = sDefaultStack;
    WriteStackMagic(sDefaultStack);
    ActiveAdd(&sDefaultThread);

    auto host = std::make_shared<HostThread>();
    tHost = host.get();
    tHostRef = host;
    Hosts()[&sDefaultThread] = std::move(host);
    tCurrent = &sDefaultThread;
    sDefaultClaimed = true;
}

struct Launch {
    OSThread* thread;
    std::shared_ptr<HostThread> host;
};

void* ThreadEntry(void* arg) {
    std::unique_ptr<Launch> launch(static_cast<Launch*>(arg));
    OSThread* thread = launch->thread;
    tHostRef = launch->host;
    tHost = tHostRef.get();
    tCurrent = thread;
    launch.reset();

    void* (*func)(void*);
    void* param;
    COSSdkThreadStartHook startHook;
    {
        Guard guard; // checkpoint: a cancel or suspend that came before the thread got here
        func = tHost->func;
        param = tHost->param;
        startHook = sThreadStartHook;
    }
    if (startHook != nullptr) {
        startHook(thread);
    }
    // Returning from the entry function is OSExitThread with its result, as on the GameCube
    // (the entry's link register points at OSExitThread).
    OSExitThread(func(param));
    return nullptr; // not reached
}

void LaunchLocked(OSThread* thread, const std::shared_ptr<HostThread>& host) {
    host->started = true;
    thread->state = OS_THREAD_STATE_RUNNING;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    const u8* base = thread->stackBase;
    const u8* end = thread->stackEnd;
    const u32 requested = (base != nullptr && end != nullptr && base > end)
                              ? static_cast<u32>(base - end)
                              : 0;
    pthread_attr_setstacksize(&attr, HostStackSize(requested));
    auto* launch = new Launch{thread, host};
    pthread_t handle;
    const int err = pthread_create(&handle, &attr, &ThreadEntry, launch);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        delete launch;
        Fatal("OSResumeThread(%p): pthread_create failed (%d: %s)", static_cast<void*>(thread),
              err, std::strerror(err));
    }
}

// The GameCube's OSExitThread/OSCancelThread bookkeeping for a thread that ends.
void EndThreadLocked(OSThread* thread) {
    OSClearContext(&thread->context);
    if (thread->attr & OS_THREAD_ATTR_DETACH) {
        ActiveRemove(thread);
        thread->state = 0;
    } else {
        thread->state = OS_THREAD_STATE_MORIBUND;
    }
    UnlockAllMutexLocked(thread);
    WakeupLocked(&thread->queueJoin);
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Guard, adoption, checkpoint, waiting

Guard::Guard() : mOwns(!tInterruptsDisabled) {
    if (mOwns) {
        Lock().lock();
        tInterruptsDisabled = true;
    }
    if (tCurrent == nullptr) {
        AdoptCurrentLocked();
    }
    CheckpointLocked();
}

Guard::~Guard() {
    // tInterruptsDisabled is false if the thread released the lock to exit (ExitCurrentLocked).
    if (mOwns && tInterruptsDisabled) {
        tInterruptsDisabled = false;
        Lock().unlock();
    }
}

void AdoptCurrentLocked() {
    if (tCurrent != nullptr) {
        return;
    }
    if (!sDefaultClaimed) {
        ClaimDefaultThreadLocked();
        return;
    }
    tForeign = std::make_unique<ForeignThread>();
    OSThread* record = &tForeign->record;
    InitRecord(record, 16, OS_THREAD_ATTR_DETACH);
    record->state = OS_THREAD_STATE_RUNNING;
    Hosts()[record] = tForeign->host;
    tHost = tForeign->host.get();
    tCurrent = record;
}

void CheckpointLocked() {
    HostThread* host = tHost;
    if (host == nullptr || host->alarmThread) {
        return;
    }
    if (host->cancelled) {
        ExitCurrentLocked();
    }
    if (tCurrent->suspend > 0) {
        // Suspended by another thread while it ran game code: stop here until resumed.
        tCurrent->state = OS_THREAD_STATE_READY;
        WaitLocked([] { return true; });
        tCurrent->state = OS_THREAD_STATE_RUNNING;
    }
}

HostThread* FindHostLocked(OSThread* thread) {
    auto it = Hosts().find(thread);
    return it == Hosts().end() ? nullptr : it->second.get();
}

void NotifyLocked(OSThread* thread) {
    if (HostThread* host = FindHostLocked(thread)) {
        host->cv.notify_all();
    }
}

void NotifyAllLocked() {
    for (auto& entry : Hosts()) {
        entry.second->cv.notify_all();
    }
    NotifyAlarmThreadLocked();
}

bool ShuttingDownLocked() {
    return sShuttingDown;
}

void ExitCurrentLocked() {
    HostThread* host = tHost;
    if (host == nullptr || !host->created) {
        Fatal("thread %p cannot exit: it was not made by OSCreateThread (the main thread or a "
              "host thread)",
              static_cast<void*>(tCurrent));
    }
    tInterruptsDisabled = false;
    Lock().unlock();
    pthread_exit(nullptr);
}

void QueueRemove(OSThreadQueue* queue, OSThread* thread) {
    OSThread* next = thread->link.next;
    OSThread* prev = thread->link.prev;
    if (next == nullptr) {
        queue->tail = prev;
    } else {
        next->link.prev = prev;
    }
    if (prev == nullptr) {
        queue->head = next;
    } else {
        prev->link.next = next;
    }
    thread->link.next = thread->link.prev = nullptr;
}

void QueueAddPrio(OSThreadQueue* queue, OSThread* thread) {
    OSThread* next = queue->head;
    while (next != nullptr && next->priority <= thread->priority) {
        next = next->link.next;
    }
    if (next == nullptr) {
        OSThread* prev = queue->tail;
        if (prev == nullptr) {
            queue->head = thread;
        } else {
            prev->link.next = thread;
        }
        thread->link.prev = prev;
        thread->link.next = nullptr;
        queue->tail = thread;
    } else {
        thread->link.next = next;
        OSThread* prev = next->link.prev;
        next->link.prev = thread;
        thread->link.prev = prev;
        if (prev == nullptr) {
            queue->head = thread;
        } else {
            prev->link.next = thread;
        }
    }
}

bool SleepLocked(OSThreadQueue* queue, bool wakeOnShutdown) {
    OSThread* self = tCurrent;
    if (wakeOnShutdown && sShuttingDown) {
        return false;
    }
    self->state = OS_THREAD_STATE_WAITING;
    self->queue = queue;
    QueueAddPrio(queue, self);
    WaitLocked([&] {
        return self->state != OS_THREAD_STATE_WAITING || (wakeOnShutdown && sShuttingDown);
    });
    if (self->state == OS_THREAD_STATE_WAITING) {
        // Woken by the shutdown, not by OSWakeupThread.
        QueueRemove(queue, self);
        self->queue = nullptr;
        self->state = OS_THREAD_STATE_RUNNING;
        return false;
    }
    self->state = OS_THREAD_STATE_RUNNING;
    return true;
}

void WakeupLocked(OSThreadQueue* queue) {
    while (OSThread* thread = queue->head) {
        QueueRemove(queue, thread);
        thread->queue = nullptr;
        thread->state = OS_THREAD_STATE_READY;
        NotifyLocked(thread);
    }
}

void Log(const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::fputs("[cos_sdk] ", stderr);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
    va_end(args);
}

void Fatal(const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    std::fputs("[cos_sdk] FATAL: ", stderr);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
    va_end(args);
    std::fflush(stderr);
    std::abort();
}

} // namespace cos_sdk::os

using namespace cos_sdk::os;

extern "C" {

// ---------------------------------------------------------------------------------------------
// Thread system

void __OSThreadInit(void) {
    Guard guard; // claims the default thread for the caller if nobody has
}

void OSInitThreadQueue(OSThreadQueue* queue) {
    queue->head = queue->tail = nullptr;
}

OSThread* OSGetCurrentThread(void) {
    if (tCurrent == nullptr) {
        Guard guard;
    }
    return tCurrent;
}

BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize,
                    OSPriority priority, u16 attr) {
    if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
        return FALSE;
    }
    Guard guard;
    auto it = Hosts().find(thread);
    if (it != Hosts().end()) {
        const HostThread& old = *it->second;
        const bool live = old.alarmThread || !old.created ||
                          (old.started && !OSIsThreadTerminated(thread));
        if (live) {
            Fatal("OSCreateThread(%p): the record belongs to a thread that is still running",
                  static_cast<void*>(thread));
        }
    }
    if (ActiveContains(thread)) {
        ActiveRemove(thread); // a joinable thread that ended and was never joined
    }

    InitRecord(thread, priority, attr & OS_THREAD_ATTR_DETACH);
    thread->state = OS_THREAD_STATE_READY;
    thread->suspend = 1;
    // GameCube stacks grow down: `stack` is the top.
    thread->stackBase = static_cast<u8*>(stack);
    thread->stackEnd = static_cast<u8*>(stack) - stackSize;
    if (stack != nullptr) {
        WriteStackMagic(thread->stackEnd);
    }

    auto host = std::make_shared<HostThread>();
    host->func = func;
    host->param = param;
    host->created = true;
    Hosts()[thread] = std::move(host);
    ActiveAdd(thread);
    return TRUE;
}

s32 OSResumeThread(OSThread* thread) {
    Guard guard;
    const s32 previous = thread->suspend--;
    if (thread->suspend < 0) {
        thread->suspend = 0;
    } else if (thread->suspend == 0) {
        auto it = Hosts().find(thread);
        if (it != Hosts().end()) {
            HostThread& host = *it->second;
            if (host.created && !host.started && thread->state == OS_THREAD_STATE_READY) {
                LaunchLocked(thread, it->second);
            } else {
                host.cv.notify_all(); // self-suspended, or waiting at a checkpoint
            }
        }
    }
    return previous;
}

s32 OSSuspendThread(OSThread* thread) {
    Guard guard;
    const s32 previous = thread->suspend++;
    if (thread == tCurrent) {
        // GameCube: the thread stops at once and runs again after OSResumeThread.
        thread->state = OS_THREAD_STATE_READY;
        WaitLocked([] { return true; });
        thread->state = OS_THREAD_STATE_RUNNING;
    } else if (previous == 0) {
        HostThread* host = FindHostLocked(thread);
        if (host != nullptr && thread->state == OS_THREAD_STATE_RUNNING &&
            (host->started || !host->created)) {
            Log("OSSuspendThread(%p): the thread is running on another host thread; it stops at "
                "its next OS call",
                static_cast<void*>(thread));
        }
    }
    return previous;
}

BOOL OSIsThreadSuspended(OSThread* thread) {
    Guard guard;
    return thread->suspend > 0 ? TRUE : FALSE;
}

BOOL OSIsThreadTerminated(OSThread* thread) {
    // Callers inside cos_sdk already hold the lock; a Guard nests.
    Guard guard;
    return (thread->state == OS_THREAD_STATE_MORIBUND || thread->state == 0) ? TRUE : FALSE;
}

void OSSleepThread(OSThreadQueue* queue) {
    Guard guard;
    SleepLocked(queue, false);
}

void OSWakeupThread(OSThreadQueue* queue) {
    Guard guard;
    WakeupLocked(queue);
}

void OSYieldThread(void) {
    {
        Guard guard; // checkpoint
    }
    std::this_thread::yield();
}

void OSExitThread(void* val) {
    Guard guard;
    OSThread* self = tCurrent;
    if (!tHost->created) {
        Fatal("OSExitThread on thread %p, which OSCreateThread did not make (the main thread or a "
              "host thread)",
              static_cast<void*>(self));
    }
    self->val = val;
    EndThreadLocked(self);
    ExitCurrentLocked();
}

void OSCancelThread(OSThread* thread) {
    Guard guard;
    HostThread* host = FindHostLocked(thread);
    bool wasWaiting = false;
    switch (thread->state) {
    case OS_THREAD_STATE_READY:
    case OS_THREAD_STATE_RUNNING:
        break;
    case OS_THREAD_STATE_WAITING:
        wasWaiting = true;
        if (thread->queue != nullptr) {
            QueueRemove(thread->queue, thread);
            thread->queue = nullptr;
        }
        break;
    default:
        return; // already ended
    }
    if (host != nullptr && (!host->created || host->alarmThread)) {
        Fatal("OSCancelThread(%p): the thread was not made by OSCreateThread (the main thread or a "
              "host thread) and cannot be stopped",
              static_cast<void*>(thread));
    }
    EndThreadLocked(thread);
    if (thread == tCurrent) {
        ExitCurrentLocked();
    }
    if (host != nullptr && host->started) {
        host->cancelled = true;
        host->cv.notify_all();
        if (!wasWaiting && thread->suspend <= 0) {
            Log("OSCancelThread(%p): the thread is running on another host thread; it ends at its "
                "next OS call",
                static_cast<void*>(thread));
        }
    }
}

void OSDetachThread(OSThread* thread) {
    Guard guard;
    thread->attr |= OS_THREAD_ATTR_DETACH;
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        ActiveRemove(thread);
        thread->state = 0;
    }
    WakeupLocked(&thread->queueJoin);
}

BOOL OSJoinThread(OSThread* thread, void** val) {
    Guard guard;
    if (!(thread->attr & OS_THREAD_ATTR_DETACH) && thread->state != OS_THREAD_STATE_MORIBUND &&
        thread->queueJoin.head == nullptr) {
        SleepLocked(&thread->queueJoin, false);
        if (!ActiveContains(thread)) {
            return FALSE;
        }
    }
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        if (val != nullptr) {
            *val = thread->val;
        }
        ActiveRemove(thread);
        thread->state = 0;
        return TRUE;
    }
    return FALSE;
}

s32 OSCheckActiveThreads(void) {
    Guard guard;
    s32 count = 0;
    for (OSThread* t = sActiveQueue.head; t != nullptr; t = t->linkActive.next) {
        count++;
    }
    return count;
}

// ---------------------------------------------------------------------------------------------
// Priority and scheduler. Priorities are recorded but do not order host threads (phase 6).

BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
        return FALSE;
    }
    Guard guard;
    thread->base = priority;
    thread->priority = priority;
    return TRUE;
}

s32 OSGetThreadPriority(OSThread* thread) {
    Guard guard;
    return thread->base;
}

s32 OSDisableScheduler(void) {
    Guard guard;
    return sSchedulerSuspendCount++;
}

s32 OSEnableScheduler(void) {
    Guard guard;
    return sSchedulerSuspendCount--;
}

OSSwitchThreadCallback OSSetSwitchThreadCallback(OSSwitchThreadCallback callback) {
    Guard guard;
    OSSwitchThreadCallback previous = sSwitchThreadCallback;
    sSwitchThreadCallback = callback;
    if (callback != nullptr && !sLoggedSwitchCallback) {
        sLoggedSwitchCallback = true;
        Log("OSSetSwitchThreadCallback: host threads never switch, so the callback is not called; "
            "per-thread state goes through COSSdkSetThreadStartHook (cos_sdk/hooks.h)");
    }
    return previous;
}

OSThread* OSSetIdleFunction(OSIdleFunction idleFunction, void* param, void* stack, u32 stackSize) {
    (void)param;
    (void)stack;
    (void)stackSize;
    Guard guard;
    if (idleFunction != nullptr && !sLoggedIdleFunction) {
        sLoggedIdleFunction = true;
        Log("OSSetIdleFunction: the host has no idle thread; the idle function is not called");
    }
    return nullptr;
}

OSThread* OSGetIdleFunction(void) {
    return nullptr;
}

void OSClearStack(u8 val) {
    (void)val; // the host stack belongs to the host OS
}

// ---------------------------------------------------------------------------------------------
// Thread-specific storage (the calling thread's own fields: no lock needed)

void OSSetThreadSpecific(s32 index, void* ptr) {
    OSThread* thread = OSGetCurrentThread();
    if (index >= 0 && index < OS_THREAD_SPECIFIC_MAX) {
        thread->specific[index] = ptr;
    }
}

void* OSGetThreadSpecific(s32 index) {
    OSThread* thread = OSGetCurrentThread();
    if (index >= 0 && index < OS_THREAD_SPECIFIC_MAX) {
        return thread->specific[index];
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// cos_sdk/hooks.h

void COSSdkRequestShutdown(void) {
    Guard guard;
    sShuttingDown = true;
    NotifyAllLocked();
}

BOOL COSSdkIsShuttingDown(void) {
    Guard guard;
    return sShuttingDown ? TRUE : FALSE;
}

COSSdkThreadStartHook COSSdkSetThreadStartHook(COSSdkThreadStartHook hook) {
    Guard guard;
    COSSdkThreadStartHook previous = sThreadStartHook;
    sThreadStartHook = hook;
    return previous;
}

OSSwitchThreadCallback COSSdkGetSwitchThreadCallback(void) {
    Guard guard;
    return sSwitchThreadCallback;
}

} // extern "C"
