// cos_sdk: the GBA library (docs/NATIVE_PORT_PHASE2_3.md, step 2.6e). Aurora has none.
//
// The host has no Game Boy Advance link cable, so every port behaves as one with nothing plugged
// in (decision of the plan: "all GBA* return not ready"):
// - GBAGetStatus, GBAReset, GBARead, GBAWrite and GBAJoyBoot return GBA_NOT_READY, which is what
//   the console's synchronous calls return for an empty port (the SI probe gets no response);
//   their output buffers and status bytes are left untouched, as on the console.
// - The ...Async variants return GBA_NOT_READY too. The SDK reports a request it cannot start
//   through the return value and then never calls the callback; that is what happens here, so a
//   caller never waits for a callback that will not come.
// - GBAGetProcessStatus returns GBA_READY and leaves *percentp alone: on the console that means
//   "no request or JoyBoot in progress on this port", which is always true here (none can start).
// - GBAInit only logs, once, that the GBA link is not emulated (JUTGba::create calls it; the
//   Tingle Tuner and the other GBA features then see no GBA).
// Bad channel numbers abort, as the SDK's ASSERTs would in a debug build: on the console they
// index past __GBA[4].
//
// Not defined: the library's private state and helpers of dolphin/gba/GBAPriv.h (__GBA,
// __GBAReset, __GBAHandler, __GBASync, __GBATransfer, ...). Only the GBA library itself uses them.
//
// Aurora's <dolphin/gba.h> includes a bare <types.h>, which only native/include/sdk provides (for
// the game), and cos_sdk compiles against Aurora's headers only (sdk.cmake). So the few names
// used here are repeated below with gba.h's values and signatures; the smoke test ("gba") calls
// these functions through the header the game uses and checks the values.
//
// Provenance: not in Dusklight (Dusklight's game has no GBA features). Return values follow the
// decomp's src/dolphin/gba/GBA.c and GBAGetProcessStatus.c.
#include "../os/os_internal.h"

#include <dolphin/types.h>

using namespace cos_sdk::os;

// From <dolphin/gba.h>.
#define GBA_MAX_CHAN 4
#define GBA_READY 0
#define GBA_NOT_READY 1

extern "C" {
typedef void (*GBACallback)(s32 chan, s32 ret);
}

namespace {

void CheckChan(const char* func, s32 chan) {
    if (chan < 0 || chan >= GBA_MAX_CHAN) {
        Fatal("%s: invalid GBA channel %d", func, (int)chan);
    }
}

s32 NotReady(const char* func, s32 chan) {
    CheckChan(func, chan);
    return GBA_NOT_READY;
}

} // namespace

extern "C" {

void GBAInit(void) {
    COS_SDK_LOG_ONCE("GBAInit: the GBA link is not emulated; every GBA port reports "
                     "GBA_NOT_READY (nothing connected)");
}

s32 GBAGetStatus(s32 chan, u8* status) {
    (void)status;
    return NotReady("GBAGetStatus", chan);
}

s32 GBAGetStatusAsync(s32 chan, u8* status, GBACallback callback) {
    (void)status;
    (void)callback;
    return NotReady("GBAGetStatusAsync", chan);
}

s32 GBAReset(s32 chan, u8* status) {
    (void)status;
    return NotReady("GBAReset", chan);
}

s32 GBAResetAsync(s32 chan, u8* status, GBACallback callback) {
    (void)status;
    (void)callback;
    return NotReady("GBAResetAsync", chan);
}

s32 GBAGetProcessStatus(s32 chan, u8* percentp) {
    (void)percentp;
    CheckChan("GBAGetProcessStatus", chan);
    return GBA_READY;
}

s32 GBARead(s32 chan, u8* dst, u8* status) {
    (void)dst;
    (void)status;
    return NotReady("GBARead", chan);
}

s32 GBAReadAsync(s32 chan, u8* dst, u8* status, GBACallback callback) {
    (void)dst;
    (void)status;
    (void)callback;
    return NotReady("GBAReadAsync", chan);
}

s32 GBAWrite(s32 chan, u8* src, u8* status) {
    (void)src;
    (void)status;
    return NotReady("GBAWrite", chan);
}

s32 GBAWriteAsync(s32 chan, u8* src, u8* status, GBACallback callback) {
    (void)src;
    (void)status;
    (void)callback;
    return NotReady("GBAWriteAsync", chan);
}

s32 GBAJoyBoot(s32 chan, s32 palette_color, s32 palette_speed, u8* programp, s32 length,
               u8* status) {
    (void)palette_color;
    (void)palette_speed;
    (void)programp;
    (void)length;
    (void)status;
    return NotReady("GBAJoyBoot", chan);
}

s32 GBAJoyBootAsync(s32 chan, s32 palette_color, s32 palette_speed, u8* programp, s32 length,
                    u8* status, GBACallback callback) {
    (void)palette_color;
    (void)palette_speed;
    (void)programp;
    (void)length;
    (void)status;
    (void)callback;
    return NotReady("GBAJoyBootAsync", chan);
}

} // extern "C"
