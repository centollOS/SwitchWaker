// cos_sdk: the EXI bus (docs/NATIVE_PORT_PHASE2_3.md, step 2.6e). Aurora declares the EXI API in
// <dolphin/exi.h> but has no EXI library.
//
// The host has an EXI bus with nothing on it. The memory cards are Aurora's aurora_card
// (kabufuda), SRAM and the RTC are cos_sdk's OSSram/OSRtc (step 2.6b), and none of them goes
// through EXI. So:
// - The lock is real software state, as on the console: EXILock takes a free channel, a second
//   EXILock on a locked channel queues its unlock callback (one per device, at most three) and
//   returns FALSE, and EXIUnlock calls the first queued callback with interrupts disabled.
//   EXIGetState reports EXI_STATE_LOCKED with the locking device, and EXISetExiCallback keeps the
//   channel's callback (never called: no device raises an EXI interrupt).
// - Nothing answers a selection: EXISelect and EXISelectSD fail (logged once; on the console a
//   select of an empty memory-card slot fails the same way), so EXIImm, EXIImmEx, EXIDma and
//   EXISync, which need a selected device, return FALSE as the SDK does for an unselected
//   channel, and EXIDeselect returns FALSE (nothing selected). EXIGetID and EXIGetType return 0
//   ("no device"; *id and *type are left alone).
// - EXIProbe is FALSE for the slots (channels 0 and 1) and TRUE for channel 2, which has no
//   external slot (the SDK's __EXIProbe); EXIProbeEx is -1 ("nothing attached") for the slots;
//   EXIAttach fails because nothing is attached, and EXIDetach of an unattached channel is TRUE.
// - EXIGetTypeString is the SDK's table. The decomp's version falls off the end for an unknown
//   type (undefined); this one returns "Unknown".
// Bad channel numbers abort, as the SDK's ASSERTs would in a debug build.
//
// Provenance: Dusklight src/dusk/stubs.cpp (CC0, ref/dusklight, the "EXI" section) logs and
// returns FALSE from EXILock, EXIUnlock, EXISelect, EXIDeselect, EXIImm, EXIDma and EXISync.
// Changed: the lock and its unlock queue follow the decomp's src/dolphin/exi/EXIBios.c
// (EXILock/EXIUnlock), so lock/unlock pairs and EXIGetState behave as on a console with empty
// slots; only device access fails; and the rest of the API Aurora declares is defined.
#include "../os/os_internal.h"

#include <dolphin/exi.h>

#include <cstring>

using namespace cos_sdk::os;

namespace {

constexpr s32 kMaxChan = 3; // EXI channels 0, 1 and 2
constexpr int kMaxDev = 3;  // devices per channel (the unlock queue's size)

// EXIGetState bits (the decomp's EXI_STATE_*, dolphin/exi/EXIBios.h).
constexpr u32 kStateAttached = 0x08;
constexpr u32 kStateLocked = 0x10;

EXIControl sEcb[kMaxChan]; // guarded by Lock()

EXIControl& Channel(const char* func, s32 chan) {
    if (chan < 0 || chan >= kMaxChan) {
        Fatal("%s: invalid EXI channel %d", func, (int)chan);
    }
    return sEcb[chan];
}

} // namespace

extern "C" {

void EXIInit(void) {
    Guard guard;
    std::memset(sEcb, 0, sizeof(sEcb));
}

EXICallback EXISetExiCallback(s32 channel, EXICallback callback) {
    Guard guard;
    EXIControl& exi = Channel("EXISetExiCallback", channel);
    EXICallback old = exi.exiCallback;
    exi.exiCallback = callback;
    return old;
}

BOOL EXILock(s32 channel, u32 device, EXICallback callback) {
    Guard guard;
    EXIControl& exi = Channel("EXILock", channel);
    if (device >= kMaxDev) {
        Fatal("EXILock: invalid EXI device %u on channel %d", device, (int)channel);
    }
    if ((exi.state & kStateLocked) != 0) {
        if (callback != nullptr) {
            for (int i = 0; i < exi.items; i++) {
                if (exi.queue[i].dev == device) {
                    return FALSE;
                }
            }
            if (exi.items < kMaxDev) {
                exi.queue[exi.items].callback = callback;
                exi.queue[exi.items].dev = device;
                exi.items++;
            }
        }
        return FALSE;
    }
    exi.state |= kStateLocked;
    exi.dev = device;
    return TRUE;
}

BOOL EXIUnlock(s32 channel) {
    Guard guard;
    EXIControl& exi = Channel("EXIUnlock", channel);
    if ((exi.state & kStateLocked) == 0) {
        return FALSE;
    }
    exi.state &= ~kStateLocked;
    if (exi.items > 0) {
        EXICallback unlocked = exi.queue[0].callback;
        exi.items--;
        if (exi.items > 0) {
            std::memmove(&exi.queue[0], &exi.queue[1], sizeof(exi.queue[0]) * exi.items);
        }
        unlocked(channel, nullptr);
    }
    return TRUE;
}

u32 EXIGetState(s32 channel) {
    Guard guard;
    return Channel("EXIGetState", channel).state;
}

BOOL EXISelect(s32 channel, u32 device, u32 frequency) {
    {
        Guard guard;
        Channel("EXISelect", channel);
    }
    COS_SDK_LOG_ONCE("EXISelect(channel %d, device %u, frequency %u): no EXI device is emulated; "
                     "every selection fails (logged once)",
                     (int)channel, device, frequency);
    return FALSE;
}

int EXISelectSD(s32 chan, u32 dev, u32 freq) {
    return EXISelect(chan, dev, freq);
}

BOOL EXIDeselect(s32 channel) {
    Guard guard;
    Channel("EXIDeselect", channel);
    return FALSE; // nothing is ever selected
}

BOOL EXIImm(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback) {
    (void)buffer;
    (void)length;
    (void)type;
    (void)callback;
    Guard guard;
    Channel("EXIImm", channel);
    return FALSE;
}

BOOL EXIImmEx(s32 channel, void* buffer, s32 length, u32 type) {
    (void)buffer;
    (void)length;
    (void)type;
    Guard guard;
    Channel("EXIImmEx", channel);
    return FALSE;
}

BOOL EXIDma(s32 channel, void* buffer, s32 length, u32 type, EXICallback callback) {
    (void)buffer;
    (void)length;
    (void)type;
    (void)callback;
    Guard guard;
    Channel("EXIDma", channel);
    return FALSE;
}

BOOL EXISync(s32 channel) {
    Guard guard;
    Channel("EXISync", channel);
    return FALSE;
}

BOOL EXIProbe(s32 channel) {
    Guard guard;
    Channel("EXIProbe", channel);
    return channel == 2 ? TRUE : FALSE;
}

s32 EXIProbeEx(s32 channel) {
    return EXIProbe(channel) ? 1 : -1;
}

void EXIProbeReset(void) {}

BOOL EXIAttach(s32 channel, EXICallback callback) {
    (void)callback;
    Guard guard;
    Channel("EXIAttach", channel);
    return FALSE; // nothing to attach to
}

BOOL EXIDetach(s32 channel) {
    Guard guard;
    EXIControl& exi = Channel("EXIDetach", channel);
    exi.state &= ~kStateAttached;
    return TRUE;
}

s32 EXIGetID(s32 channel, u32 device, u32* id) {
    (void)device;
    (void)id;
    Guard guard;
    Channel("EXIGetID", channel);
    return 0;
}

s32 EXIGetType(s32 chan, u32 dev, u32* type) {
    (void)type;
    u32 id;
    return EXIGetID(chan, dev, &id);
}

char* EXIGetTypeString(u32 type) {
    switch (type) {
    case EXI_MEMORY_CARD_59:
        return const_cast<char*>("Memory Card 59");
    case EXI_MEMORY_CARD_123:
        return const_cast<char*>("Memory Card 123");
    case EXI_MEMORY_CARD_251:
        return const_cast<char*>("Memory Card 251");
    case EXI_MEMORY_CARD_507:
        return const_cast<char*>("Memory Card 507");
    case EXI_USB_ADAPTER:
        return const_cast<char*>("USB Adapter");
    case 0x80000020:
    case 0x80000080:
    case 0x80000040:
    case 0x80000008:
    case 0x80000010:
    case 0x80000004:
        return const_cast<char*>("Net Card");
    case EXI_ETHER_VIEWER:
        return const_cast<char*>("Artist Ether");
    case 0x4020100:
    case 0x4020300:
    case EXI_ETHER:
    case EXI_STREAM_HANGER:
        return const_cast<char*>("Stream Hanger");
    case EXI_IS_VIEWER:
        return const_cast<char*>("IS Viewer");
    default:
        return const_cast<char*>("Unknown");
    }
}

} // extern "C"
