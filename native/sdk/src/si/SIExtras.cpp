// cos_sdk: the SI functions Aurora's aurora_si lacks (docs/NATIVE_PORT_PHASE2_3.md, step 2.6e).
//
// aurora_si has only SIProbe, which asks Aurora's input layer what is on a port (a GameCube
// controller, a WaveBird, or SI_ERROR_NO_RESPONSE). Aurora's PAD library reads the controllers
// itself, so nothing on the host goes through the serial interface's registers. Here:
// - SIGetType is SIProbe, and SIGetTypeAsync calls its callback at once with interrupts disabled
//   and returns the type, as the SDK does when the type is already known.
// - SIGetStatus reports SI_ERROR_NO_RESPONSE for a port SIProbe finds empty and 0 otherwise.
// - SITransfer accepts one transfer per channel and completes it after `delay` ticks (at least
//   one) on the alarm timer thread, in "interrupt" context, with SI_ERROR_NO_RESPONSE: no device
//   answers raw SI commands on the host. That is what an empty port gives on the console, and the
//   callback runs later, never inside SITransfer, as on the console. SIBusy and SIIsChanBusy
//   report the pending transfers. The first transfer is logged.
// - Polling: SISetXY, SIEnablePolling and SIDisablePolling keep the SDK's poll word and return it
//   as the SDK computes it; SISetCommand keeps the per-channel command; SITransferCommands has no
//   hardware to latch into. SIGetResponse returns FALSE (no polled data is ever latched; PAD reads
//   go through Aurora) and logs that once.
// - SIRegisterPollingHandler/SIUnregisterPollingHandler keep the SDK's four-entry table with its
//   return values. No polling interrupt exists on the host, so the handlers are never called;
//   the first registration logs that.
// - SIInit forgets the poll word, the commands and the handlers. Transfers in flight still
//   complete.
// Bad channel numbers abort, as the SDK's ASSERTs would in a debug build.
//
// The declarations the game adds for these (SICallback, SITypeCallback and the functions) are in
// native/include/sdk/dolphin/si/SIBios.h; cos_sdk compiles against Aurora's headers only
// (sdk.cmake), so they are repeated here with the same signatures.
//
// Provenance: not in Dusklight (it uses only Aurora's SIProbe). The SDK behaviour follows the
// decomp's src/dolphin/si/SIBios.c.
#include "../os/os_internal.h"

#include <dolphin/os.h>
#include <dolphin/si.h>

#include <cstring>

using namespace cos_sdk::os;

extern "C" {
typedef void (*SICallback)(s32 chan, u32 sr, OSContext* context);
typedef void (*SITypeCallback)(s32 chan, u32 type);

BOOL SIBusy(void);
BOOL SIIsChanBusy(s32 chan);
BOOL SIRegisterPollingHandler(__OSInterruptHandler handler);
BOOL SIUnregisterPollingHandler(__OSInterruptHandler handler);
void SIInit(void);
u32 SIGetStatus(s32 chan);
void SISetCommand(s32 chan, u32 command);
void SITransferCommands(void);
u32 SISetXY(u32 x, u32 y);
u32 SIEnablePolling(u32 poll);
u32 SIDisablePolling(u32 poll);
BOOL SIGetResponse(s32 chan, void* data);
BOOL SITransfer(s32 chan, void* output, u32 outputBytes, void* input, u32 inputBytes,
                SICallback callback, OSTime delay);
u32 SIGetType(s32 chan);
u32 SIGetTypeAsync(s32 chan, SITypeCallback callback);
}

namespace {

struct Transfer {
    OSAlarm alarm;
    SICallback callback = nullptr;
    bool pending = false;
};

// All guarded by Lock().
Transfer sTransfer[SI_MAX_CHAN];
u32 sPoll = 0;
u32 sCommand[SI_MAX_CHAN] = {};
__OSInterruptHandler sPollingHandler[4] = {};
OSContext sInterruptContext; // the context the completion callbacks get

void CheckChan(const char* func, s32 chan) {
    if (chan < 0 || chan >= SI_MAX_CHAN) {
        Fatal("%s: invalid SI channel %d", func, (int)chan);
    }
}

// Alarm handler (runs with Lock() held): the transfer on this channel ends with no response.
void TransferDone(OSAlarm* alarm, OSContext* context) {
    (void)context;
    for (s32 chan = 0; chan < SI_MAX_CHAN; chan++) {
        Transfer& t = sTransfer[chan];
        if (&t.alarm != alarm || !t.pending) {
            continue;
        }
        SICallback callback = t.callback;
        t.pending = false;
        t.callback = nullptr;
        if (callback != nullptr) {
            callback(chan, SI_ERROR_NO_RESPONSE, &sInterruptContext);
        }
        return;
    }
}

} // namespace

extern "C" {

void SIInit(void) {
    Guard guard;
    sPoll = 0;
    std::memset(sCommand, 0, sizeof(sCommand));
    std::memset(sPollingHandler, 0, sizeof(sPollingHandler));
}

BOOL SIBusy(void) {
    Guard guard;
    for (const Transfer& t : sTransfer) {
        if (t.pending) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL SIIsChanBusy(s32 chan) {
    Guard guard;
    CheckChan("SIIsChanBusy", chan);
    return sTransfer[chan].pending ? TRUE : FALSE;
}

BOOL SITransfer(s32 chan, void* output, u32 outputBytes, void* input, u32 inputBytes,
                SICallback callback, OSTime delay) {
    (void)output;
    (void)outputBytes;
    (void)input;
    (void)inputBytes;
    Guard guard;
    CheckChan("SITransfer", chan);
    Transfer& t = sTransfer[chan];
    if (t.pending) {
        return FALSE; // one transfer per channel, as in the SDK
    }
    COS_SDK_LOG_ONCE("SITransfer(channel %d): raw SI transfers are not emulated; they complete with "
                     "SI_ERROR_NO_RESPONSE (logged once)",
                     (int)chan);
    t.pending = true;
    t.callback = callback;
    OSCreateAlarm(&t.alarm);
    OSSetAlarm(&t.alarm, delay > 0 ? delay : 1, TransferDone);
    return TRUE;
}

u32 SIGetType(s32 chan) {
    CheckChan("SIGetType", chan);
    return SIProbe(chan);
}

u32 SIGetTypeAsync(s32 chan, SITypeCallback callback) {
    const u32 type = SIGetType(chan);
    Guard guard;
    if (callback != nullptr) {
        callback(chan, type);
    }
    return type;
}

u32 SIGetStatus(s32 chan) {
    return SIGetType(chan) == SI_ERROR_NO_RESPONSE ? SI_ERROR_NO_RESPONSE : 0;
}

void SISetCommand(s32 chan, u32 command) {
    Guard guard;
    CheckChan("SISetCommand", chan);
    sCommand[chan] = command;
}

void SITransferCommands(void) {}

u32 SISetXY(u32 x, u32 y) {
    if (x < 8 || x > 1023 || y > 255) {
        Fatal("SISetXY(%u, %u): out of range (8 <= x <= 1023, y <= 255)", x, y);
    }
    Guard guard;
    sPoll &= 0xFC0000FF;
    sPoll |= (x << 16) | (y << 8);
    return sPoll;
}

u32 SIEnablePolling(u32 poll) {
    Guard guard;
    if (poll == 0) {
        return sPoll;
    }
    poll >>= 24;
    const u32 en = poll & 0xF0;
    poll &= (en >> 4) | 0x03FFFFF0;
    poll &= 0xFC0000FF;
    sPoll &= ~(en >> 4);
    sPoll |= poll;
    return sPoll;
}

u32 SIDisablePolling(u32 poll) {
    Guard guard;
    if (poll == 0) {
        return sPoll;
    }
    poll >>= 24;
    poll &= 0xF0;
    sPoll &= ~poll;
    return sPoll;
}

BOOL SIGetResponse(s32 chan, void* data) {
    (void)data;
    CheckChan("SIGetResponse", chan);
    COS_SDK_LOG_ONCE("SIGetResponse(channel %d): no SI polling on the host (PAD reads controllers "
                     "through Aurora); returning FALSE (logged once)",
                     (int)chan);
    return FALSE;
}

BOOL SIRegisterPollingHandler(__OSInterruptHandler handler) {
    Guard guard;
    for (auto& h : sPollingHandler) {
        if (h == handler) {
            return TRUE;
        }
    }
    for (auto& h : sPollingHandler) {
        if (h == nullptr) {
            COS_SDK_LOG_ONCE("SIRegisterPollingHandler: there is no SI polling interrupt on the "
                             "host; registered handlers are never called (logged once)");
            h = handler;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL SIUnregisterPollingHandler(__OSInterruptHandler handler) {
    Guard guard;
    for (auto& h : sPollingHandler) {
        if (h == handler) {
            h = nullptr;
            return TRUE;
        }
    }
    return FALSE;
}

} // extern "C"
