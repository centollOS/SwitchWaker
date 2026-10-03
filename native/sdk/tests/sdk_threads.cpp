// cos_sdk_smoke: the "threads" and "alarms" tests of step 2.6a (OS threads, interrupts, mutexes,
// conditions, message queues and alarms on host threads). Also built with -fsanitize=thread as
// cos_sdk_smoke_tsan (native/cmake/sdk.cmake), which must report no race.
#include "smoke.h"

#include <dolphin/os.h>

#include "cos_sdk/hooks.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace {

constexpr u32 kStackSize = 16 * 1024;

struct Stack {
    alignas(32) u8 bytes[kStackSize];
    void* top() { return bytes + sizeof(bytes); }
};

void* AsMessage(std::uintptr_t value) {
    return reinterpret_cast<void*>(value);
}

std::uintptr_t FromMessage(OSMessage msg) {
    return reinterpret_cast<std::uintptr_t>(msg);
}

// Waits (on the host clock) until `done()` holds; false after about two seconds.
template <class Pred>
bool WaitUntil(Pred done) {
    for (int i = 0; i < 2000; i++) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return done();
}

// ---- message ping-pong ------------------------------------------------------------------------

constexpr int kMessages = 1000;

OSMessageQueue sPingQueue, sPongQueue;
OSMessage sPingSlots[4], sPongSlots[4];

void* PingThread(void*) {
    for (int i = 1; i <= kMessages; i++) {
        OSSendMessage(&sPingQueue, AsMessage(i), OS_MESSAGE_BLOCK);
        OSMessage reply = nullptr;
        OSReceiveMessage(&sPongQueue, &reply, OS_MESSAGE_BLOCK);
        if (FromMessage(reply) != static_cast<std::uintptr_t>(i) * 2) {
            return AsMessage(0xBAD);
        }
    }
    return AsMessage(0x600D);
}

void* PongThread(void*) {
    // Exits through OSExitThread rather than by returning.
    for (int i = 1; i <= kMessages; i++) {
        OSMessage msg = nullptr;
        OSReceiveMessage(&sPingQueue, &msg, OS_MESSAGE_BLOCK);
        OSSendMessage(&sPongQueue, AsMessage(FromMessage(msg) * 2), OS_MESSAGE_BLOCK);
    }
    // A full 64-bit value must survive OSJoinThread.
    OSExitThread(AsMessage(static_cast<std::uintptr_t>(0x1234567890ull)));
    return nullptr;
}

// ---- mutex and condition counter --------------------------------------------------------------

constexpr int kCounterThreads = 4;
constexpr int kIncrements = 5000;

OSMutex sCounterMutex;
OSCond sCounterCond;
int sCounter = 0;        // guarded by sCounterMutex
int sFinishedThreads = 0; // guarded by sCounterMutex

void* CounterThread(void*) {
    for (int i = 0; i < kIncrements; i++) {
        OSLockMutex(&sCounterMutex);
        OSLockMutex(&sCounterMutex); // recursive
        sCounter++;
        OSUnlockMutex(&sCounterMutex);
        OSUnlockMutex(&sCounterMutex);
        if ((i & 63) == 0) {
            OSYieldThread();
        }
    }
    OSLockMutex(&sCounterMutex);
    sFinishedThreads++;
    OSSignalCond(&sCounterCond);
    OSUnlockMutex(&sCounterMutex);
    return nullptr;
}

// ---- sleep/wakeup with interrupts disabled, self-suspension, thread-specific ------------------

OSThreadQueue sFlagQueue;
bool sFlag = false; // guarded by interrupts disabled
void* sSpecificSeen = nullptr;

void* SleeperThread(void*) {
    OSSetThreadSpecific(0, AsMessage(0x51));
    BOOL enabled = OSDisableInterrupts();
    while (!sFlag) {
        OSSleepThread(&sFlagQueue);
    }
    OSRestoreInterrupts(enabled);
    sSpecificSeen = OSGetThreadSpecific(0);
    // Suspend self; the main thread resumes us.
    OSSuspendThread(OSGetCurrentThread());
    return AsMessage(0x77);
}

// ---- cancelling a thread blocked in OSReceiveMessage --------------------------------------------

OSMessageQueue sCancelQueue;
OSMessage sCancelSlots[1];

void* BlockedReceiver(void*) {
    OSMessage msg;
    OSReceiveMessage(&sCancelQueue, &msg, OS_MESSAGE_BLOCK);
    return AsMessage(0xBAD); // not reached: cancelled while waiting
}

OSThread sPing, sPong, sCounters[kCounterThreads], sSleeper, sReceiver;
Stack sPingStack, sPongStack, sCounterStacks[kCounterThreads], sSleeperStack, sReceiverStack;

// ---- alarms -----------------------------------------------------------------------------------

OSMessageQueue sAlarmQueue;
OSMessage sAlarmSlots[16];

void SendTagHandler(OSAlarm* alarm, OSContext*) {
    OSSendMessage(&sAlarmQueue, AsMessage(alarm->tag), OS_MESSAGE_NOBLOCK);
}

std::atomic<int> sPeriodicFires{0};
void PeriodicHandler(OSAlarm*, OSContext*) {
    sPeriodicFires++;
    OSSendMessage(&sAlarmQueue, AsMessage(0xFE), OS_MESSAGE_NOBLOCK);
}

std::atomic<int> sNeverFires{0};
void NeverHandler(OSAlarm*, OSContext*) {
    sNeverFires++;
}

// JFWDisplay::threadSleep: disable interrupts, set an alarm that resumes this thread, suspend.
OSThread* sWaitingThread = nullptr; // written and read with interrupts disabled
void ResumeHandler(OSAlarm*, OSContext*) {
    OSResumeThread(sWaitingThread);
}

int sRearms = 0; // only touched by the handler (timer thread)
void RearmHandler(OSAlarm* alarm, OSContext*) {
    if (++sRearms < 3) {
        OSSetAlarm(alarm, OSMillisecondsToTicks(1), RearmHandler);
    } else {
        OSSendMessage(&sAlarmQueue, AsMessage(0x3A), OS_MESSAGE_NOBLOCK);
    }
}

} // namespace

COS_SMOKE_TEST(threads) {
    OSThread* self = OSGetCurrentThread();
    COS_SMOKE_CHECK(self != nullptr);
    COS_SMOKE_CHECK(OSGetCurrentThread() == self);
    COS_SMOKE_CHECK(self->stackBase != nullptr && self->stackEnd != nullptr);
    const s32 activeBefore = OSCheckActiveThreads();
    COS_SMOKE_CHECK(activeBefore >= 1);

    // Interrupt state is a per-thread boolean, nested as on the GameCube.
    BOOL outer = OSDisableInterrupts();
    COS_SMOKE_CHECK(outer == TRUE);
    BOOL inner = OSDisableInterrupts();
    COS_SMOKE_CHECK(inner == FALSE);
    COS_SMOKE_CHECK(OSRestoreInterrupts(inner) == FALSE);
    COS_SMOKE_CHECK(OSRestoreInterrupts(outer) == FALSE);
    COS_SMOKE_CHECK(OSEnableInterrupts() == TRUE);

    // Two threads exchange 1000 messages each way through queues of 4 (blocking both ways).
    OSInitMessageQueue(&sPingQueue, sPingSlots, 4);
    OSInitMessageQueue(&sPongQueue, sPongSlots, 4);
    COS_SMOKE_CHECK(OSCreateThread(&sPing, PingThread, nullptr, sPingStack.top(), kStackSize, 16, 0));
    COS_SMOKE_CHECK(OSCreateThread(&sPong, PongThread, nullptr, sPongStack.top(), kStackSize, 16, 0));
    COS_SMOKE_CHECK(OSIsThreadSuspended(&sPing));
    COS_SMOKE_CHECK(!OSIsThreadTerminated(&sPing));
    COS_SMOKE_CHECK(OSCheckActiveThreads() == activeBefore + 2);
    // The stack bookkeeping JKRThread reads: base is the top, the magic word sits at the end.
    COS_SMOKE_CHECK(sPing.stackBase == sPingStack.top());
    COS_SMOKE_CHECK(sPing.stackEnd == sPingStack.bytes);
    COS_SMOKE_CHECK(*reinterpret_cast<u32*>(sPing.stackEnd) == OS_THREAD_STACK_MAGIC);
    COS_SMOKE_CHECK(OSResumeThread(&sPong) == 1);
    COS_SMOKE_CHECK(OSResumeThread(&sPing) == 1);
    void* pingResult = nullptr;
    void* pongResult = nullptr;
    COS_SMOKE_CHECK(OSJoinThread(&sPing, &pingResult));
    COS_SMOKE_CHECK(OSJoinThread(&sPong, &pongResult));
    COS_SMOKE_CHECK(FromMessage(pingResult) == 0x600D);
    COS_SMOKE_CHECK(FromMessage(pongResult) == 0x1234567890ull);
    COS_SMOKE_CHECK(OSIsThreadTerminated(&sPing) && OSIsThreadTerminated(&sPong));
    COS_SMOKE_CHECK(OSCheckActiveThreads() == activeBefore);

    // Non-blocking sends and receives, and OSJamMessage putting a message in front.
    OSMessageQueue mq;
    OSMessage slots[2];
    OSInitMessageQueue(&mq, slots, 2);
    OSMessage got = nullptr;
    COS_SMOKE_CHECK(!OSReceiveMessage(&mq, &got, OS_MESSAGE_NOBLOCK));
    COS_SMOKE_CHECK(OSSendMessage(&mq, AsMessage(1), OS_MESSAGE_NOBLOCK));
    COS_SMOKE_CHECK(OSJamMessage(&mq, AsMessage(2), OS_MESSAGE_NOBLOCK));
    COS_SMOKE_CHECK(!OSSendMessage(&mq, AsMessage(3), OS_MESSAGE_NOBLOCK));
    COS_SMOKE_CHECK(OSReceiveMessage(&mq, &got, OS_MESSAGE_NOBLOCK) && FromMessage(got) == 2);
    COS_SMOKE_CHECK(OSReceiveMessage(&mq, &got, OS_MESSAGE_BLOCK) && FromMessage(got) == 1);

    // Mutex and condition: four threads count to 20000 under a recursive mutex while the main
    // thread waits on the condition until all of them are done.
    OSInitMutex(&sCounterMutex);
    OSInitCond(&sCounterCond);
    COS_SMOKE_CHECK(OSTryLockMutex(&sCounterMutex));
    COS_SMOKE_CHECK(sCounterMutex.thread == self && sCounterMutex.count == 1);
    for (int i = 0; i < kCounterThreads; i++) {
        COS_SMOKE_CHECK(OSCreateThread(&sCounters[i], CounterThread, nullptr,
                                       sCounterStacks[i].top(), kStackSize, 12, 0));
        OSResumeThread(&sCounters[i]);
    }
    while (sFinishedThreads < kCounterThreads) {
        OSWaitCond(&sCounterCond, &sCounterMutex);
        COS_SMOKE_CHECK(sCounterMutex.thread == self && sCounterMutex.count == 1);
    }
    COS_SMOKE_CHECK(sCounter == kCounterThreads * kIncrements);
    OSUnlockMutex(&sCounterMutex);
    COS_SMOKE_CHECK(sCounterMutex.thread == nullptr && sCounterMutex.count == 0);
    for (int i = 0; i < kCounterThreads; i++) {
        COS_SMOKE_CHECK(OSJoinThread(&sCounters[i], nullptr));
    }

    // OSSleepThread/OSWakeupThread with interrupts disabled, thread-specific storage and
    // self-suspension.
    OSInitThreadQueue(&sFlagQueue);
    COS_SMOKE_CHECK(OSCreateThread(&sSleeper, SleeperThread, nullptr, sSleeperStack.top(),
                                   kStackSize, 16, 0));
    OSResumeThread(&sSleeper);
    COS_SMOKE_CHECK(WaitUntil([] {
        BOOL enabled = OSDisableInterrupts();
        const bool waiting = sSleeper.state == OS_THREAD_STATE_WAITING;
        OSRestoreInterrupts(enabled);
        return waiting;
    }));
    {
        BOOL enabled = OSDisableInterrupts();
        sFlag = true;
        OSWakeupThread(&sFlagQueue);
        OSRestoreInterrupts(enabled);
    }
    COS_SMOKE_CHECK(WaitUntil([] { return OSIsThreadSuspended(&sSleeper) == TRUE; }));
    COS_SMOKE_CHECK(OSResumeThread(&sSleeper) == 1);
    void* sleeperResult = nullptr;
    COS_SMOKE_CHECK(OSJoinThread(&sSleeper, &sleeperResult));
    COS_SMOKE_CHECK(FromMessage(sleeperResult) == 0x77);
    COS_SMOKE_CHECK(FromMessage(sSpecificSeen) == 0x51);
    COS_SMOKE_CHECK(OSGetThreadSpecific(0) == nullptr); // the main thread's own slot

    // OSCancelThread on a thread blocked in OSReceiveMessage: it ends without taking the message
    // sent afterwards, and a detached cancelled thread leaves the active list.
    OSInitMessageQueue(&sCancelQueue, sCancelSlots, 1);
    COS_SMOKE_CHECK(OSCreateThread(&sReceiver, BlockedReceiver, nullptr, sReceiverStack.top(),
                                   kStackSize, 16, 0));
    OSResumeThread(&sReceiver);
    COS_SMOKE_CHECK(WaitUntil([] {
        BOOL enabled = OSDisableInterrupts();
        const bool waiting = sReceiver.state == OS_THREAD_STATE_WAITING;
        OSRestoreInterrupts(enabled);
        return waiting;
    }));
    OSDetachThread(&sReceiver);
    OSCancelThread(&sReceiver);
    COS_SMOKE_CHECK(OSIsThreadTerminated(&sReceiver));
    COS_SMOKE_CHECK(OSSendMessage(&sCancelQueue, AsMessage(9), OS_MESSAGE_NOBLOCK));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    COS_SMOKE_CHECK(OSReceiveMessage(&sCancelQueue, &got, OS_MESSAGE_NOBLOCK) &&
                    FromMessage(got) == 9);
    COS_SMOKE_CHECK(OSCheckActiveThreads() == activeBefore);

    // A record can be reused once its thread has ended.
    COS_SMOKE_CHECK(OSCreateThread(&sPing, PingThread, nullptr, sPingStack.top(), kStackSize, 16, 0));
    OSDetachThread(&sPing);
    OSCancelThread(&sPing); // never started: nothing runs
    COS_SMOKE_CHECK(OSIsThreadTerminated(&sPing));
    COS_SMOKE_CHECK(OSCheckActiveThreads() == activeBefore);
    return true;
}

COS_SMOKE_TEST(alarms) {
    OSInitMessageQueue(&sAlarmQueue, sAlarmSlots, 16);
    OSMessage msg = nullptr;

    // One-shot alarm: fires once, after its delay, and clears its handler.
    OSAlarm once;
    OSCreateAlarm(&once);
    OSSetAlarmTag(&once, 0x11);
    const OSTime start = OSGetTime();
    OSSetAlarm(&once, OSMillisecondsToTicks(5), SendTagHandler);
    COS_SMOKE_CHECK(OSReceiveMessage(&sAlarmQueue, &msg, OS_MESSAGE_BLOCK));
    COS_SMOKE_CHECK(FromMessage(msg) == 0x11);
    COS_SMOKE_CHECK(OSGetTime() - start >= OSMillisecondsToTicks(5));
    {
        BOOL enabled = OSDisableInterrupts();
        const bool cleared = once.handler == nullptr;
        OSRestoreInterrupts(enabled);
        COS_SMOKE_CHECK(cleared);
    }

    // Alarms fire in time order, not set order.
    OSAlarm late, early;
    OSCreateAlarm(&late);
    OSCreateAlarm(&early);
    OSSetAlarmTag(&late, 0x22);
    OSSetAlarmTag(&early, 0x21);
    OSSetAlarm(&late, OSMillisecondsToTicks(30), SendTagHandler);
    OSSetAlarm(&early, OSMillisecondsToTicks(10), SendTagHandler);
    COS_SMOKE_CHECK(OSReceiveMessage(&sAlarmQueue, &msg, OS_MESSAGE_BLOCK) &&
                    FromMessage(msg) == 0x21);
    COS_SMOKE_CHECK(OSReceiveMessage(&sAlarmQueue, &msg, OS_MESSAGE_BLOCK) &&
                    FromMessage(msg) == 0x22);

    // A cancelled alarm never fires, also when cancelled by tag.
    OSAlarm cancelled, tagged;
    OSCreateAlarm(&cancelled);
    OSCreateAlarm(&tagged);
    OSSetAlarmTag(&tagged, 0x99);
    OSSetAlarm(&cancelled, OSMillisecondsToTicks(10), NeverHandler);
    OSSetAlarm(&tagged, OSMillisecondsToTicks(10), NeverHandler);
    OSCancelAlarm(&cancelled);
    OSCancelAlarms(0x99);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    COS_SMOKE_CHECK(sNeverFires.load() == 0);

    // Periodic alarm: five fires, then cancelled; no fire after OSCancelAlarm returns.
    OSAlarm periodic;
    OSCreateAlarm(&periodic);
    OSSetPeriodicAlarm(&periodic, OSGetTime(), OSMillisecondsToTicks(2), PeriodicHandler);
    for (int i = 0; i < 5; i++) {
        COS_SMOKE_CHECK(OSReceiveMessage(&sAlarmQueue, &msg, OS_MESSAGE_BLOCK) &&
                        FromMessage(msg) == 0xFE);
    }
    OSCancelAlarm(&periodic);
    const int firesAtCancel = sPeriodicFires.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    COS_SMOKE_CHECK(sPeriodicFires.load() == firesAtCancel);
    while (OSReceiveMessage(&sAlarmQueue, &msg, OS_MESSAGE_NOBLOCK)) {
        COS_SMOKE_CHECK(FromMessage(msg) == 0xFE); // fires queued before the cancel
    }

    // A handler that sets its own alarm again.
    OSAlarm rearm;
    OSCreateAlarm(&rearm);
    OSSetAlarm(&rearm, OSMillisecondsToTicks(1), RearmHandler);
    COS_SMOKE_CHECK(OSReceiveMessage(&sAlarmQueue, &msg, OS_MESSAGE_BLOCK) &&
                    FromMessage(msg) == 0x3A);

    // JFWDisplay's sleep: interrupts disabled, an alarm resumes the thread, the thread suspends
    // itself. The alarm handler must get the lock while the thread is suspended.
    for (int i = 0; i < 3; i++) {
        OSAlarm wake;
        OSCreateAlarm(&wake);
        BOOL enabled = OSDisableInterrupts();
        sWaitingThread = OSGetCurrentThread();
        OSSetAlarm(&wake, OSMillisecondsToTicks(2), ResumeHandler);
        COS_SMOKE_CHECK(OSSuspendThread(sWaitingThread) == 0);
        COS_SMOKE_CHECK(!OSIsThreadSuspended(sWaitingThread));
        OSRestoreInterrupts(enabled);
    }
    return true;
}
