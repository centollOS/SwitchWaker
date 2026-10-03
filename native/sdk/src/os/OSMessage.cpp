// cos_sdk: OSMessageQueue (docs/NATIVE_PORT_PHASE2_3.md, step 2.6a; the model is in
// os_internal.h).
//
// Provenance: adapted from the message-queue part of Dusklight src/dusk/stubs.cpp (CC0,
// ref/dusklight). Kept from it: the ring buffer over mq->msgArray with firstIndex/usedCount as on
// the console, OSJamMessage inserting at the front, and blocking sends and receives returning FALSE
// once the process shuts down (dusk::IsShuttingDown, here COSSdkRequestShutdown). Changed: instead
// of a side table with a std::mutex and two condition variables per queue, the GameCube SDK's
// algorithm runs under the OS lock with OSSleepThread/OSWakeupThread on mq->queueSend and
// mq->queueReceive, so an alarm handler or a thread with interrupts disabled can send to a queue
// too, and no side-table entry outlives the game's queue. Pointers in messages are full 64-bit.
#include "os_internal.h"

using namespace cos_sdk::os;

extern "C" {

void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
    OSInitThreadQueue(&mq->queueSend);
    OSInitThreadQueue(&mq->queueReceive);
    mq->msgArray = msgArray;
    mq->msgCount = msgCount;
    mq->firstIndex = 0;
    mq->usedCount = 0;
}

BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    Guard guard;
    while (mq->msgCount <= mq->usedCount) {
        if (!(flags & OS_MESSAGE_BLOCK) || !SleepLocked(&mq->queueSend, true)) {
            return FALSE;
        }
    }
    const s32 lastIndex = (mq->firstIndex + mq->usedCount) % mq->msgCount;
    mq->msgArray[lastIndex] = msg;
    mq->usedCount++;
    WakeupLocked(&mq->queueReceive);
    return TRUE;
}

BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
    Guard guard;
    while (mq->usedCount == 0) {
        if (!(flags & OS_MESSAGE_BLOCK) || !SleepLocked(&mq->queueReceive, true)) {
            return FALSE;
        }
    }
    if (msg != nullptr) {
        *msg = mq->msgArray[mq->firstIndex];
    }
    mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
    mq->usedCount--;
    WakeupLocked(&mq->queueSend);
    return TRUE;
}

BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    Guard guard;
    while (mq->msgCount <= mq->usedCount) {
        if (!(flags & OS_MESSAGE_BLOCK) || !SleepLocked(&mq->queueSend, true)) {
            return FALSE;
        }
    }
    mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
    mq->msgArray[mq->firstIndex] = msg;
    mq->usedCount++;
    WakeupLocked(&mq->queueReceive);
    return TRUE;
}

} // extern "C"
