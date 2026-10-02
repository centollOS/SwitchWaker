// Forwarder (phase 2, step 2.4): the game's dolphin/gx/GXMisc.h over Aurora's <dolphin/gx.h>
// (GXManage.h, GXPixel.h and GXCpu2Efb.h declare GXSetMisc, GXFlush, GXDrawDone, the GXPoke* and
// GXPeek* functions and the draw sync/done callbacks). Added here, as the decomp declares them:
// GXSetDrawSync (m_Do_graphic.cpp calls it; Aurora has no definition, Dusklight defines it in its
// stubs, step 2.6) and the SDK-internal __GXAbort and __GXPEInit.
#ifndef COS_SDK_DOLPHIN_GX_GXMISC_H
#define COS_SDK_DOLPHIN_GX_GXMISC_H

#include <dolphin/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

void GXSetDrawSync(u16 token);
void __GXAbort(void);
void __GXPEInit(void);

#ifdef __cplusplus
}
#endif

#endif
