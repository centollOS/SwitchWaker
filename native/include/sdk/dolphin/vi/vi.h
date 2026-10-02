// Forwarder (phase 2, step 2.3): the game's dolphin/vi/vi.h over Aurora's <dolphin/vi.h>.
// Aurora declares the VI API and, in <dolphin/gx/GXStruct.h>, the format macros, VITVMode and
// VIXFBMode. Added here: the decomp's VI_3D and VI_GCA format numbers and VIPositionCallback.
// Left out on purpose: __VIRegs (the VI registers, defined in the decomp's header at a fixed
// address) and the SDK-internal VITimingInfo/VIPositionInfo tables. The decomp's extra VITVMode
// values (NTSC 3D, GCA) cannot be added to Aurora's enum; the game does not use them.
#ifndef COS_SDK_DOLPHIN_VI_VI_H
#define COS_SDK_DOLPHIN_VI_VI_H

#include <dolphin/vi.h>
#include <dolphin/os/OSUtil.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VI_3D  (3)
#define VI_GCA (6)

typedef void (*VIPositionCallback)(s16 x, s16 y);

#ifdef __cplusplus
}
#endif

#endif
