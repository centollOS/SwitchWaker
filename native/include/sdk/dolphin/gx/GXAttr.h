// Forwarder (phase 2, step 2.4): the game's dolphin/gx/GXAttr.h over Aurora's <dolphin/gx.h>, which
// declares GXVtxDescList, GXVtxAttrFmtList, the vertex descriptor/format/array functions,
// GXSetTexCoordGen(2) and GXSetNumTexGens (with const pointers and, for GXSetArray under
// TARGET_PC, a size argument behind GXSETARRAY: Aurora's signatures win, step 2.7). Added here: the
// decomp's SDK-internal __GXSetVCD, __GXCalculateVLim and __GXSetVAT (nothing on the host defines
// them). The decomp's struct tags (_GXVtxDescList, _GXVtxAttrFmtList) are not Aurora's; no game
// unit uses them.
#ifndef COS_SDK_DOLPHIN_GX_GXATTR_H
#define COS_SDK_DOLPHIN_GX_GXATTR_H

#include <dolphin/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

void __GXSetVCD(void);
void __GXCalculateVLim(void);
void __GXSetVAT(void);

#ifdef __cplusplus
}
#endif

#endif
