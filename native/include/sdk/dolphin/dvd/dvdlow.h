// Forwarder (phase 2, step 2.4): the game's dolphin/dvd/dvdlow.h over Aurora's <dolphin/dvd.h>, which
// declares DVDLowCallback and the DVDLow* API. Added here: the decomp's SDK-internal
// work-around and interrupt functions (nothing on the host defines them). Left out on purpose:
// __DIRegs, the DVD interface registers the decomp defines in the header at a fixed address.
#ifndef COS_SDK_DOLPHIN_DVD_DVDLOW_H
#define COS_SDK_DOLPHIN_DVD_DVDLOW_H

#include <dolphin/dvd/dvd.h>
#include <dolphin/os/OSUtil.h>
#include <dolphin/os/OSContext.h>

#ifdef __cplusplus
extern "C" {
#endif

void __DVDInitWA(void);
void __DVDInterruptHandler(__OSInterrupt interrupt, OSContext* context);
void __DVDLowSetWAType(u32 type, u32 location);

#ifdef __cplusplus
}
#endif

#endif
