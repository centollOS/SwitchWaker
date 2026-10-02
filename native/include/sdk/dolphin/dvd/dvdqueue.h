// Forwarder (phase 2, step 2.4): the game's dolphin/dvd/dvdqueue.h. Aurora declares none of the DVD
// waiting-queue internals (aurora_dvd has its own queue), so these are the decomp's declarations;
// nothing on the host defines them.
#ifndef COS_SDK_DOLPHIN_DVD_DVDQUEUE_H
#define COS_SDK_DOLPHIN_DVD_DVDQUEUE_H

#include <dolphin/dvd/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif

void __DVDClearWaitingQueue(void);
int __DVDPushWaitingQueue(s32 prio, DVDCommandBlock* block);
DVDCommandBlock* __DVDPopWaitingQueue(void);
int __DVDCheckWaitingQueue(void);
int __DVDDequeueWaitingQueue(DVDCommandBlock* block);
int __DVDIsBlockInWaitingQueue(DVDCommandBlock* block);

#ifdef __cplusplus
}
#endif

#endif
