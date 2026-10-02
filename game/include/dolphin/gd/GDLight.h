#ifndef GDLIGHT_H
#define GDLIGHT_H

#include "dolphin/gx/GX.h"
#include "dolphin/types.h"

#ifdef __cplusplus
extern "C" {
#endif

inline static u16 __GDLightID2Index(GXLightID id) {
    u16 lightIdx = (31 - __cntlzw(id));
    if (lightIdx > 7)
        lightIdx = 0;
    return lightIdx;
}

inline static u16 __GDLightID2Offset(GXLightID id) {
    return __GDLightID2Index(id) * 0x10;
}

#ifdef __cplusplus
}
#endif

#endif /* GDLIGHT_H */
