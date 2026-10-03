// cos_sdk_smoke: the "host_alloc" test. cos_sdk's own bookkeeping (the alarm queue, the thread
// side table, HostThread and launch records, adopted host threads) must never call the global
// operator new/delete: on PC those are the game's (JKRHeap.cpp), which allocate from the current
// JKRHeap and free under its OSMutex, and the alarm thread may not block on a mutex. This program
// replaces the global forms with counting ones; while the test runs alarms (one-shot, periodic,
// cancelled, cancelled by tag), creates, runs and joins an OS thread and lets a plain host thread
// adopt an OSThread, they must be called zero times.
//
// Not built into cos_sdk_smoke_tsan: ThreadSanitizer intercepts operator new/delete itself.
#include "smoke.h"

#include <dolphin/os.h>

#include <pthread.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <thread>

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define COS_SMOKE_TSAN 1
#endif
#endif

#ifndef COS_SMOKE_TSAN

namespace {

std::atomic<bool> sCounting{false};
std::atomic<long> sGlobalCalls{0};

void Count() {
    if (sCounting.load(std::memory_order_relaxed)) {
        sGlobalCalls.fetch_add(1, std::memory_order_relaxed);
    }
}

void* CountedAlloc(std::size_t size, std::size_t align) {
    Count();
    if (size == 0) {
        size = 1;
    }
    void* p = align <= alignof(std::max_align_t)
                  ? std::malloc(size)
                  : std::aligned_alloc(align, (size + align - 1) & ~(align - 1));
    if (p == nullptr) {
        std::abort();
    }
    return p;
}

void CountedFree(void* p) noexcept {
    if (p != nullptr) {
        Count();
        std::free(p);
    }
}

} // namespace

void* operator new(std::size_t size) {
    return CountedAlloc(size, alignof(std::max_align_t));
}
void* operator new[](std::size_t size) {
    return CountedAlloc(size, alignof(std::max_align_t));
}
void* operator new(std::size_t size, std::align_val_t align) {
    return CountedAlloc(size, static_cast<std::size_t>(align));
}
void* operator new[](std::size_t size, std::align_val_t align) {
    return CountedAlloc(size, static_cast<std::size_t>(align));
}
void operator delete(void* p) noexcept {
    CountedFree(p);
}
void operator delete[](void* p) noexcept {
    CountedFree(p);
}
void operator delete(void* p, std::size_t) noexcept {
    CountedFree(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    CountedFree(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    CountedFree(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    CountedFree(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    CountedFree(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    CountedFree(p);
}

namespace {

constexpr u32 kStackSize = 16 * 1024;
alignas(32) u8 sWorkerStack[kStackSize];
OSThread sWorker;

OSMessageQueue sQueue;
OSMessage sSlots[16];

void SendTag(OSAlarm* alarm, OSContext*) {
    OSSendMessage(&sQueue, reinterpret_cast<void*>(static_cast<std::uintptr_t>(alarm->tag)),
                  OS_MESSAGE_NOBLOCK);
}

void Never(OSAlarm*, OSContext*) {
    std::abort();
}

void* Worker(void* param) {
    return param;
}

void* Foreign(void*) {
    // The first SDK call of a plain host thread adopts it (an OSThread of its own); its record is
    // dropped when the thread ends.
    return OSGetCurrentThread() != nullptr ? reinterpret_cast<void*>(1) : nullptr;
}

} // namespace

COS_SMOKE_TEST(host_alloc) {
    OSInitMessageQueue(&sQueue, sSlots, 16);
    OSMessage msg = nullptr;
    OSAlarm once, periodic, cancelled, tagged;

    sGlobalCalls.store(0);
    sCounting.store(true);

    OSCreateAlarm(&once);
    OSCreateAlarm(&periodic);
    OSCreateAlarm(&cancelled);
    OSCreateAlarm(&tagged);
    OSSetAlarmTag(&once, 1);
    OSSetAlarmTag(&periodic, 2);
    OSSetAlarmTag(&tagged, 0x77);
    OSSetAlarm(&cancelled, OSMillisecondsToTicks(20), Never);
    OSSetAlarm(&tagged, OSMillisecondsToTicks(20), Never);
    OSSetAlarm(&once, OSMillisecondsToTicks(2), SendTag);
    OSSetPeriodicAlarm(&periodic, OSGetTime(), OSMillisecondsToTicks(2), SendTag);
    OSCancelAlarm(&cancelled);
    OSCancelAlarms(0x77);
    int onceSeen = 0, periodicSeen = 0;
    while (onceSeen < 1 || periodicSeen < 4) {
        COS_SMOKE_CHECK(OSReceiveMessage(&sQueue, &msg, OS_MESSAGE_BLOCK));
        (reinterpret_cast<std::uintptr_t>(msg) == 1 ? onceSeen : periodicSeen)++;
    }
    OSCancelAlarm(&periodic);

    COS_SMOKE_CHECK(OSCreateThread(&sWorker, Worker, reinterpret_cast<void*>(0x42),
                                   sWorkerStack + kStackSize, kStackSize, 16, 0));
    OSResumeThread(&sWorker);
    void* result = nullptr;
    COS_SMOKE_CHECK(OSJoinThread(&sWorker, &result));
    COS_SMOKE_CHECK(result == reinterpret_cast<void*>(0x42));

    pthread_t foreign;
    COS_SMOKE_CHECK(pthread_create(&foreign, nullptr, Foreign, nullptr) == 0);
    void* adopted = nullptr;
    COS_SMOKE_CHECK(pthread_join(foreign, &adopted) == 0);
    COS_SMOKE_CHECK(adopted != nullptr);

    // The worker's host thread drops its HostThread reference in its thread-local destructors,
    // after OSJoinThread has returned: give it time to end.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    sCounting.store(false);
    const long calls = sGlobalCalls.load();
    if (calls != 0) {
        std::fprintf(stderr, "host_alloc: cos_sdk called the global operator new/delete %ld times\n",
                     calls);
    }
    COS_SMOKE_CHECK(calls == 0);
    return true;
}

#endif // COS_SMOKE_TSAN
