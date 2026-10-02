// Forwarder (phase 2, step 2.4): the game's dolphin/gx/GXFrameBuf.h over Aurora's <dolphin/gx.h>
// (GXFrameBuffer.h declares the render modes and the copy functions). Added here, as the decomp
// declares them: GXCopyMode (the decomp's GXEnum.h has it, Aurora's does not), GXEurgb60Hz480IntDf,
// GXSetDispCopyFrame2Field and GXClearBoundingBox. Nothing on the host defines the last three yet;
// no game unit uses them.
#ifndef COS_SDK_DOLPHIN_GX_GXFRAMEBUF_H
#define COS_SDK_DOLPHIN_GX_GXFRAMEBUF_H

#include <dolphin/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _GXCopyMode {
    /* 0x0 */ GX_COPY_PROGRESSIVE,
    /* 0x1 */ GX_COPY_INTLC_EVEN,
    /* 0x2 */ GX_COPY_INTLC_ODD,
} GXCopyMode;

extern GXRenderModeObj GXEurgb60Hz480IntDf;

void GXSetDispCopyFrame2Field(GXCopyMode mode);
void GXClearBoundingBox(void);

#ifdef __cplusplus
}
#endif

#endif
