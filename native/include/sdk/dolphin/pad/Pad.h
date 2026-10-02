// Forwarder (phase 2, step 2.3): the game's dolphin/pad/Pad.h over Aurora's <dolphin/pad.h>, which
// declares PADStatus, the button and error constants and the PAD API. Added here: the sampling
// callback API the decomp declares and Aurora lacks (nothing defines PADSetSamplingCallback yet;
// the game does not call it). The decomp's file-static SI callbacks are left out.
#ifndef COS_SDK_DOLPHIN_PAD_PAD_H
#define COS_SDK_DOLPHIN_PAD_PAD_H

#include <dolphin/pad.h>
#include <dolphin/pad/Padclamp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*PADSamplingCallback)(void);
PADSamplingCallback PADSetSamplingCallback(PADSamplingCallback callback);

#ifdef __cplusplus
}
#endif

#endif
