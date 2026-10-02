// Forwarder (phase 2, step 2.4): the game's dolphin/gx/GXInit.h over Aurora's <dolphin/gx.h>, which
// declares GXInit, GXTexRegionCallback and the texture/TLUT region API. Added here, as the decomp
// declares them: GXTlutRegionCallback and the SDK-internal __GXDefaultTexRegionCallback,
// __GXDefaultTlutRegionCallback, __GXShutdown, __GXInitRevisionBits and __GXInitGX (nothing on the
// host defines them).
//
// Left out on purpose: GXData and its `gx` pointer (the SDK's own GX state; aurora_gx keeps its
// own), GXSetWasteFlags and set_x2 (which write it), and the CP/PE/PI/MEM register addresses,
// pointers and GX_GET/SET_*_REG accessors. No game unit uses them (d_map.cpp includes this header
// for the texture object functions).
#ifndef COS_SDK_DOLPHIN_GX_GXINIT_H
#define COS_SDK_DOLPHIN_GX_GXINIT_H

#include <dolphin/gx.h>
#include <dolphin/mtx/mtx.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef GXTlutRegion* (*GXTlutRegionCallback)(u32 idx);

GXTexRegion* __GXDefaultTexRegionCallback(const GXTexObj* obj, GXTexMapID id);
GXTlutRegion* __GXDefaultTlutRegionCallback(u32 tlut);
BOOL __GXShutdown(BOOL final);
void __GXInitRevisionBits(void);
void __GXInitGX(void);

#ifdef __cplusplus
}
#endif

#endif
