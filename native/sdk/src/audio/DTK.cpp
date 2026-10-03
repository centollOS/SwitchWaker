// cos_sdk: the DTK library (disc track streaming), silent (docs/NATIVE_PORT_PHASE2_3.md,
// step 2.6f). Aurora has none. The game itself does not call DTK (JAudio streams through
// DVDPrepareStreamAsync and the AI directly); this is here so the whole SDK API of Aurora's
// <dolphin/dtk.h> links.
//
// This is the SDK's own DTK state machine, unchanged, over two things that make it silent:
// - the AI of AIStubs.cpp plays nothing and never raises the stream interrupt, so the stream
//   callback __DTKCallbackForAIInterrupt is never called: the position stays 0, no track ever
//   ends, and the playlist only moves through DTKNextTrack/DTKPrevTrack;
// - Aurora's DVD stream commands (DVDPrepareStreamAsync, DVDCancelStreamAsync,
//   DVDStopStreamAtEndAsync, ...) complete at once and call their callback before returning, so
//   every DTK_STATE_BUSY ends inside the call that entered it and DTKShutdown's wait for the
//   flush to finish ends at once.
// DTKInit logs once that disc streaming is silent.
//
// Provenance: copied from Dusklight libs/dolphin/src/dtk/dtk.c (CC0, ref/dusklight at 40457c6),
// itself a decompilation of the Dolphin SDK's dtk.c. Changed: compiled as C++ (C linkage kept for
// the API; the static helpers and state in an unnamed namespace), NULL -> nullptr, unused
// parameter names commented out, the DTKFlushTracks "no command block" call passes nullptr, and
// the log line in DTKInit.
#include "../os/os_internal.h"

#include <dolphin/ai.h>
#include <dolphin/dtk.h>
#include <dolphin/dvd.h>
#include <dolphin/os.h>

namespace {

DTKTrack* __DTKCurrentTrack;
DTKTrack* __DTKPlayListHead;
DTKTrack* __DTKPlayListTail;
volatile u32 __DTKState;
volatile u32 __DTKTempState;
volatile u32 __DTKRepeatMode;
volatile u32 __DTKPosition;
volatile u32 __DTKInterruptFrequency;
volatile u8 __DTKVolumeL;
volatile u8 __DTKVolumeR;
volatile u32 __DTKShutdownFlag;
volatile u32 __DTKTrackEnded;
DTKFlushCallback __DTKFlushCallback;
int __busy_for_ais_address;

DVDCommandBlock __block_for_run_callback;
DVDCommandBlock __block_for_prep_callback;
DVDCommandBlock __block_for_stream_status;
DVDCommandBlock __block_for_ais_isr;
DVDCommandBlock __block_for_flushtracks;
DVDCommandBlock __block_for_repeatmode;
DVDCommandBlock __block_for_set_state;
DVDCommandBlock __block_for_next_track;
DVDCommandBlock __block_for_prev_track;

void __DTKStartAi(void) {
    AISetStreamVolLeft(__DTKVolumeL);
    AISetStreamVolRight(__DTKVolumeR);
    AIResetStreamSampleCount();
    AISetStreamTrigger(__DTKInterruptFrequency);
    AISetStreamPlayState(1);
}

void __DTKStopAi(void) {
    AISetStreamVolLeft(0);
    AISetStreamVolRight(0);
    AISetStreamPlayState(0);
}

void __DTKCheckUserCallback(DTKTrack* track, u32 event) {
    ASSERTLINE(84, track);
    if (track && track->callback && (track->eventMask & event)) {
        track->callback(track->eventMask & event);
    }
}

void __DTKForward(void) {
    BOOL old = OSDisableInterrupts();
    if (__DTKCurrentTrack && __DTKCurrentTrack->next) {
        __DTKCurrentTrack = __DTKCurrentTrack->next;
    }
    OSRestoreInterrupts(old);
}

void __DTKBackward(void) {
    BOOL old = OSDisableInterrupts();
    if (__DTKCurrentTrack && __DTKCurrentTrack->prev) {
        __DTKCurrentTrack = __DTKCurrentTrack->prev;
    }
    OSRestoreInterrupts(old);
}

void __DTKCallbackForStreamStatus(s32 result, DVDCommandBlock* /*block*/) {
    if ((result & 0xFF) == 0) {
        __DTKTrackEnded = 1;
        __DTKPosition = 0;
    }
}

void __DTKCallbackForRun(s32 /*result*/, DVDFileInfo* /*fileInfo*/) {
    __DTKStartAi();
    DVDStopStreamAtEndAsync(&__block_for_run_callback, 0);
    __DTKState = DTK_STATE_RUN;
    __DTKCheckUserCallback(__DTKCurrentTrack, 1);
}

void __DTKCallbackForPreparePaused(s32 /*result*/, DVDFileInfo* /*fileInfo*/) {
    __DTKStopAi();
    DVDStopStreamAtEndAsync(&__block_for_prep_callback, 0);
    __DTKState = DTK_STATE_PAUSE;
    __DTKCheckUserCallback(__DTKCurrentTrack, 32);
}

void __DTKPrepareCurrentTrack(void) {
    DVDPrepareStreamAsync(&__DTKCurrentTrack->dvdFileInfo, 0, 0, __DTKCallbackForRun);
}

void __DTKPrepareCurrentTrackPaused(void) {
    DVDPrepareStreamAsync(&__DTKCurrentTrack->dvdFileInfo, 0, 0, __DTKCallbackForPreparePaused);
}

void __DTKCallbackForPlaylist(s32 result, DVDCommandBlock* /*block*/) {
    __DTKPosition = result;
    __busy_for_ais_address = 0;

    if (__DTKTrackEnded) {
        __DTKTrackEnded = 0;
        __DTKCheckUserCallback(__DTKCurrentTrack, 16);
        __DTKState = DTK_STATE_BUSY;

        switch (__DTKRepeatMode) {
        case DTK_MODE_NOREPEAT:
            if (__DTKCurrentTrack) {
                if (__DTKCurrentTrack->next) {
                    __DTKCurrentTrack = __DTKCurrentTrack->next;
                    __DTKStopAi();
                    __DTKPrepareCurrentTrack();
                } else {
                    __DTKCurrentTrack = __DTKPlayListHead;
                    __DTKStopAi();
                    __DTKState = DTK_STATE_STOP;
                }
            }
            break;
        case DTK_MODE_ALLREPEAT:
            if (__DTKCurrentTrack) {
                if (__DTKCurrentTrack->next) {
                    __DTKCurrentTrack = __DTKCurrentTrack->next;
                    __DTKStopAi();
                    __DTKPrepareCurrentTrack();
                } else {
                    __DTKCurrentTrack = __DTKPlayListHead;
                    __DTKStopAi();
                    __DTKPrepareCurrentTrack();
                }
            }
            break;
        case DTK_MODE_REPEAT1:
            if (__DTKCurrentTrack) {
                __DTKStopAi();
                __DTKPrepareCurrentTrack();
            }
            break;
        }
    } else {
        DVDGetStreamErrorStatusAsync(&__block_for_stream_status, __DTKCallbackForStreamStatus);
    }
}

void __DTKCallbackForAIInterrupt(u32 count) {
    AISetStreamTrigger(count + __DTKInterruptFrequency);
    if (__DTKCurrentTrack && !__busy_for_ais_address) {
        __busy_for_ais_address = 1;
        DVDGetStreamPlayAddrAsync(&__block_for_ais_isr, __DTKCallbackForPlaylist);
    }
}

void __DTKCallbackForFlush(s32 /*result*/, DVDCommandBlock* /*block*/) {
    DTKTrack* track;

    AISetStreamPlayState(0);
    track = __DTKPlayListHead;
    while (track) {
        DVDClose(&track->dvdFileInfo);
        track = track->next;
    }

    __DTKPlayListHead = nullptr;
    __DTKPlayListTail = nullptr;
    __DTKCurrentTrack = nullptr;
    __DTKState = DTK_STATE_STOP;

    if (__DTKFlushCallback) {
        __DTKFlushCallback();
        __DTKFlushCallback = nullptr;
    }

    __DTKState = DTK_STATE_STOP;
    __DTKShutdownFlag = 0;
}

void __DTKCallbackForStop(s32 /*result*/, DVDCommandBlock* /*block*/) {
    __DTKCheckUserCallback(__DTKCurrentTrack, 2);
    __DTKState = DTK_STATE_STOP;
}

void __DTKCallbackForNextTrack(s32 /*result*/, DVDCommandBlock* /*block*/) {
    AISetStreamPlayState(0);
    __DTKForward();
    __DTKState = DTK_STATE_STOP;
    DTKSetState(__DTKTempState);
}

void __DTKCallbackForPrevTrack(s32 /*result*/, DVDCommandBlock* /*block*/) {
    AISetStreamPlayState(0);
    __DTKBackward();
    __DTKState = DTK_STATE_STOP;
    DTKSetState(__DTKTempState);
}

} // namespace

extern "C" {

void DTKInit(void) {
    COS_SDK_LOG_ONCE("DTKInit: disc track streaming is not emulated yet (phase 5): tracks queue "
                     "and change state, but nothing plays and no track ever ends");
    __DTKCurrentTrack = nullptr;
    __DTKPlayListHead = nullptr;
    __DTKPlayListTail = nullptr;
    __DTKState = DTK_STATE_STOP;
    __DTKRepeatMode = DTK_MODE_NOREPEAT;
    __DTKPosition = 0;
    __DTKInterruptFrequency = 48000;
    __DTKVolumeL = 255;
    __DTKVolumeR = 255;

    AISetStreamVolLeft(0);
    AISetStreamVolRight(0);
    AIRegisterStreamCallback(__DTKCallbackForAIInterrupt);
    AIResetStreamSampleCount();
    AISetStreamPlayState(0);
}

void DTKShutdown(void) {
    AISetStreamVolLeft(0);
    AISetStreamVolRight(0);
    AIRegisterStreamCallback(nullptr);
    AIResetStreamSampleCount();
    AISetStreamPlayState(0);

    __DTKShutdownFlag = 1;
    DTKFlushTracks(nullptr);
    __DTKState = DTK_STATE_STOP;

    while (__DTKShutdownFlag) {}
}

u32 DTKQueueTrack(char* fileName, DTKTrack* track, u32 eventMask, DTKCallback callback) {
    u32 startTrack;
    BOOL old;

    startTrack = 0;
    if (!DVDOpen(fileName, &track->dvdFileInfo)) {
        return 1;
    }

    old = OSDisableInterrupts();
    track->fileName = fileName;
    track->eventMask = eventMask;
    track->callback = callback;

    if (__DTKPlayListHead == nullptr) {
        __DTKPlayListHead = track;
        __DTKPlayListTail = track;
        track->prev = nullptr;
        track->next = nullptr;
        if (__DTKState == DTK_STATE_RUN) {
            startTrack = 1;
        }
    } else {
        __DTKPlayListTail->next = track;
        track->prev = __DTKPlayListTail;
        __DTKPlayListTail = track;
        track->next = nullptr;
    }

    if (__DTKCurrentTrack == nullptr) {
        __DTKCurrentTrack = track;
    }

    OSRestoreInterrupts(old);
    __DTKCheckUserCallback(track, 8);

    if (startTrack != 0) {
        __DTKState = DTK_STATE_BUSY;
        __DTKPrepareCurrentTrack();
    }

    return 0;
}

u32 DTKRemoveTrack(DTKTrack* track) {
    BOOL old;

    if (track == __DTKCurrentTrack) {
        return 2;
    }

    old = OSDisableInterrupts();
    DVDClose(&track->dvdFileInfo);

    if (track == __DTKPlayListHead && track == __DTKPlayListTail) {
        __DTKPlayListHead = nullptr;
        __DTKPlayListTail = nullptr;
        OSRestoreInterrupts(old);
        return 0;
    }

    if (track == __DTKPlayListHead) {
        __DTKPlayListHead = track->next;
        __DTKPlayListHead->prev = nullptr;
        if (__DTKRepeatMode == DTK_MODE_ALLREPEAT) {
            __DTKPlayListTail->next = __DTKPlayListHead;
        }
        OSRestoreInterrupts(old);
        return 0;
    }

    if (track == __DTKPlayListTail) {
        __DTKPlayListTail = track->prev;
        __DTKPlayListTail->next = nullptr;
        if (__DTKRepeatMode == DTK_MODE_ALLREPEAT) {
            __DTKPlayListTail->next = __DTKPlayListHead;
        }
        OSRestoreInterrupts(old);
        return 0;
    }

    track->prev->next = track->next;
    track->next->prev = track->prev;
    OSRestoreInterrupts(old);
    return 0;
}

int DTKFlushTracks(DTKFlushCallback callback) {
    u32 temp;

    if (__DTKState == DTK_STATE_BUSY) {
        return 0;
    }

    temp = __DTKState;
    __DTKState = DTK_STATE_BUSY;
    __DTKFlushCallback = callback;
    if (temp == DTK_STATE_RUN) {
        DVDCancelStreamAsync(&__block_for_flushtracks, __DTKCallbackForFlush);
    } else {
        __DTKCallbackForFlush(0, nullptr);
    }
    return 1;
}

void DTKSetSampleRate(u32 /*samplerate*/) {
    // obsolete
}

void DTKSetInterruptFrequency(u32 samples) {
    __DTKInterruptFrequency = samples;
    AIResetStreamSampleCount();
    AISetStreamTrigger(__DTKInterruptFrequency);
}

void DTKSetRepeatMode(u32 repeat) {
    __DTKRepeatMode = repeat;
}

int DTKSetState(u32 state) {
    if (__DTKState == state) {
        return 1;
    }

    if (__DTKState == DTK_STATE_BUSY) {
        return 0;
    }

    switch (state) {
    case DTK_STATE_STOP:
        if (__DTKCurrentTrack) {
            __DTKState = DTK_STATE_BUSY;
            AISetStreamVolLeft(0);
            AISetStreamVolRight(0);
            AISetStreamPlayState(0);
            DVDCancelStreamAsync(&__block_for_set_state, __DTKCallbackForStop);
        }
        break;
    case DTK_STATE_RUN:
        if (__DTKState == DTK_STATE_PAUSE) {
            __DTKStartAi();
            __DTKState = DTK_STATE_RUN;
            if (__DTKCurrentTrack) {
                __DTKCheckUserCallback(__DTKCurrentTrack, 1);
            }
        } else if (__DTKCurrentTrack) {
            __DTKState = DTK_STATE_BUSY;
            __DTKPrepareCurrentTrack();
        } else {
            __DTKState = DTK_STATE_RUN;
        }
        __DTKTrackEnded = 0;
        break;
    case DTK_STATE_PREPARE:
        if (__DTKState == DTK_STATE_STOP) {
            if (__DTKCurrentTrack) {
                __DTKState = DTK_STATE_BUSY;
                __DTKPrepareCurrentTrackPaused();
            }
            __DTKTrackEnded = 0;
        }
        break;
    case DTK_STATE_PAUSE:
        AISetStreamPlayState(0);
        if (__DTKState == DTK_STATE_RUN) {
            __DTKState = DTK_STATE_PAUSE;
        }
        __DTKCheckUserCallback(__DTKCurrentTrack, 4);
        break;
    }

    return 1;
}

int DTKNextTrack(void) {
    if (__DTKState == DTK_STATE_BUSY) {
        return 0;
    }

    if (__DTKCurrentTrack) {
        __DTKTempState = __DTKState;
        __DTKState = DTK_STATE_BUSY;
        if (__DTKTempState == DTK_STATE_RUN) {
            AISetStreamVolLeft(0);
            AISetStreamVolRight(0);
            DVDCancelStreamAsync(&__block_for_next_track, __DTKCallbackForNextTrack);
        } else {
            __DTKForward();
            __DTKState = __DTKTempState;
        }

        return 1;
    }

    return 0;
}

int DTKPrevTrack(void) {
    if (__DTKState == DTK_STATE_BUSY) {
        return 0;
    }

    if (__DTKCurrentTrack) {
        __DTKTempState = __DTKState;
        __DTKState = DTK_STATE_BUSY;
        if (__DTKTempState == DTK_STATE_RUN) {
            AISetStreamVolLeft(0);
            AISetStreamVolRight(0);
            DVDCancelStreamAsync(&__block_for_prev_track, __DTKCallbackForPrevTrack);
        } else {
            __DTKBackward();
            __DTKState = __DTKTempState;
        }

        return 1;
    }

    return 0;
}

u32 DTKGetSampleRate(void) {
    return 1;  // obsolete
}

u32 DTKGetRepeatMode(void) {
    return __DTKRepeatMode;
}

u32 DTKGetState(void) {
    return __DTKState;
}

u32 DTKGetPosition(void) {
    return __DTKPosition;
}

u32 DTKGetInterruptFrequency(void) {
    return __DTKInterruptFrequency;
}

DTKTrack* DTKGetCurrentTrack(void) {
    return __DTKCurrentTrack;
}

void DTKSetVolume(u8 left, u8 right) {
    __DTKVolumeL = left;
    __DTKVolumeR = right;
    if (__DTKState == DTK_STATE_RUN) {
        AISetStreamVolLeft(left);
        AISetStreamVolRight(right);
    }
}

u16 DTKGetVolume(void) {
    return (__DTKVolumeL << 8) | __DTKVolumeR;
}

} // extern "C"
