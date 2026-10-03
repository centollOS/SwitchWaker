// cos_sdk: OSMutex and OSCond (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a; the model is in
// os_internal.h).
//
// Provenance: adapted from Dusklight src/dusk/OSMutex.cpp (CC0, ref/dusklight). Kept from it: the
// GameCube-visible fields (mutex->thread, mutex->count) are maintained as on the console, the
// mutex is recursive, and OSWaitCond releases every recursion level and restores the count.
// Changed: instead of a side table of std::recursive_mutex and std::condition_variable_any per
// object (whose game-visible fields were written outside any lock), the GameCube SDK's own
// algorithms run under the OS lock with OSSleepThread/OSWakeupThread on mutex->queue and
// cond->queue, the held mutexes are linked on thread->queueMutex, and __OSUnlockAllMutex releases
// them when a thread exits or is cancelled. No side table, so no stale entries when game memory
// holding a mutex is freed and reused. Priority inheritance is not modelled (host threads ignore
// priorities, phase 6).
#include "os_internal.h"

using namespace cos_sdk::os;

namespace {

void MutexQueueAdd(OSMutexQueue* queue, OSMutex* mutex) {
    OSMutex* prev = queue->tail;
    if (prev == nullptr) {
        queue->head = mutex;
    } else {
        prev->link.next = mutex;
    }
    mutex->link.prev = prev;
    mutex->link.next = nullptr;
    queue->tail = mutex;
}

void MutexQueueRemove(OSMutexQueue* queue, OSMutex* mutex) {
    OSMutex* next = mutex->link.next;
    OSMutex* prev = mutex->link.prev;
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
    mutex->link.next = mutex->link.prev = nullptr;
}

void LockLocked(OSMutex* mutex) {
    OSThread* self = tCurrent;
    for (;;) {
        OSThread* owner = mutex->thread;
        if (owner == nullptr) {
            mutex->thread = self;
            mutex->count++;
            MutexQueueAdd(&self->queueMutex, mutex);
            return;
        }
        if (owner == self) {
            mutex->count++;
            return;
        }
        self->mutex = mutex;
        SleepLocked(&mutex->queue, false);
        self->mutex = nullptr;
    }
}

// Gives the mutex up entirely and wakes its waiters. The caller owns it.
void ReleaseLocked(OSThread* self, OSMutex* mutex) {
    MutexQueueRemove(&self->queueMutex, mutex);
    mutex->thread = nullptr;
    mutex->count = 0;
    WakeupLocked(&mutex->queue);
}

} // namespace

namespace cos_sdk::os {

void UnlockAllMutexLocked(OSThread* thread) {
    while (OSMutex* mutex = thread->queueMutex.head) {
        ReleaseLocked(thread, mutex);
    }
}

} // namespace cos_sdk::os

extern "C" {

void OSInitMutex(OSMutex* mutex) {
    OSInitThreadQueue(&mutex->queue);
    mutex->thread = nullptr;
    mutex->count = 0;
    mutex->link.next = mutex->link.prev = nullptr;
}

void OSLockMutex(OSMutex* mutex) {
    Guard guard;
    LockLocked(mutex);
}

void OSUnlockMutex(OSMutex* mutex) {
    Guard guard;
    OSThread* self = tCurrent;
    // As on the GameCube, unlocking a mutex the caller does not own does nothing.
    if (mutex->thread == self && --mutex->count == 0) {
        ReleaseLocked(self, mutex);
    }
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    Guard guard;
    OSThread* self = tCurrent;
    if (mutex->thread == nullptr) {
        mutex->thread = self;
        mutex->count++;
        MutexQueueAdd(&self->queueMutex, mutex);
        return TRUE;
    }
    if (mutex->thread == self) {
        mutex->count++;
        return TRUE;
    }
    return FALSE;
}

void __OSUnlockAllMutex(OSThread* thread) {
    Guard guard;
    UnlockAllMutexLocked(thread);
}

int __OSCheckDeadLock(OSThread* thread) {
    // GameCube: follows owner -> waited-on mutex -> owner and reports a cycle back to `thread`.
    Guard guard;
    for (OSMutex* mutex = thread->mutex; mutex != nullptr;) {
        OSThread* owner = mutex->thread;
        if (owner == nullptr) {
            return FALSE;
        }
        if (owner == thread) {
            return TRUE;
        }
        mutex = owner->mutex;
    }
    return FALSE;
}

void OSInitCond(OSCond* cond) {
    OSInitThreadQueue(&cond->queue);
}

void OSWaitCond(OSCond* cond, OSMutex* mutex) {
    Guard guard;
    OSThread* self = tCurrent;
    if (mutex->thread != self) {
        // GameCube: does nothing when the caller does not own the mutex. That is always a bug.
        Log("OSWaitCond(%p, %p): the calling thread does not own the mutex; not waiting",
            static_cast<void*>(cond), static_cast<void*>(mutex));
        return;
    }
    const s32 count = mutex->count;
    ReleaseLocked(self, mutex);
    // The lock is held from the release to the sleep, so no OSSignalCond can be missed.
    SleepLocked(&cond->queue, false);
    LockLocked(mutex);
    mutex->count = count;
}

void OSSignalCond(OSCond* cond) {
    Guard guard;
    WakeupLocked(&cond->queue);
}

} // extern "C"
