#include "dolphin/gx/GXFrameBuf.h"
#include "dolphin/gx/GX.h"

GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    8,
    8,
    10,
    12,
    10,
    8,
    8,
};

GXRenderModeObj GXNtsc480Int = {
    VI_TVMODE_NTSC_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    0,
    0,
    21,
    22,
    21,
    0,
    0,
};

GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_MPAL_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    8,
    8,
    10,
    12,
    10,
    8,
    8,
};

GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT,
    640,
    528,
    528,
    40,
    23,
    640,
    528,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    8,
    8,
    10,
    12,
    10,
    8,
    8,
};

GXRenderModeObj GXEurgb60Hz480IntDf = {
    VI_TVMODE_EURGB60_INT,
    640,
    480,
    480,
    40,
    0,
    640,
    480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    8,
    8,
    10,
    12,
    10,
    8,
    8,
};

void GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height) {
    gx->cpDispSrc = 0;
    GX_BITFIELD_SET(gx->cpDispSrc, 22, 10, left);
    GX_BITFIELD_SET(gx->cpDispSrc, 12, 10, top);
    GX_BITFIELD_SET(gx->cpDispSrc, 0, 8, 73);

    gx->cpDispSize = 0;
    GX_BITFIELD_SET(gx->cpDispSize, 22, 10, width - 1);
    GX_BITFIELD_SET(gx->cpDispSize, 12, 10, height - 1);
    GX_BITFIELD_SET(gx->cpDispSize, 0, 8, 74);
}

void GXSetTexCopySrc(u16 left, u16 top, u16 width, u16 height) {
    gx->cpTexSrc = 0;
    GX_BITFIELD_SET(gx->cpTexSrc, 22, 10, left);
    GX_BITFIELD_SET(gx->cpTexSrc, 12, 10, top);
    GX_BITFIELD_SET(gx->cpTexSrc, 0, 8, 0x49);
    gx->cpTexSize = 0;
    GX_BITFIELD_SET(gx->cpTexSize, 22, 10, width - 1);
    GX_BITFIELD_SET(gx->cpTexSize, 12, 10, height - 1);
    GX_BITFIELD_SET(gx->cpTexSize, 0, 8, 0x4A);
}

void GXSetDispCopyDst(u16 wd, u16 ht) {
    u16 stride;

    stride = (int)wd * 2;
    gx->cpDispStride = 0;
    SET_REG_FIELD(gx->cpDispStride, 10, 0, (stride >> 5));
    SET_REG_FIELD(gx->cpDispStride, 8, 24, 0x4D);
}

void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) {
    u32 rowTiles;
    u32 colTiles;
    u32 cmpTiles;
    u32 peTexFmt;
    u32 peTexFmtH;

    gx->cpTexZ = 0;
    peTexFmt = fmt & 0xF;

    if (fmt == GX_TF_Z16) {
        peTexFmt = 0xB;
    }

    switch (fmt) {
    case GX_TF_I4:
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_IA8:
    case GX_CTF_YUVA8:
        SET_REG_FIELD(gx->cpTex, 2, 15, 3);
        break;
    default:
        SET_REG_FIELD(gx->cpTex, 2, 15, 2);
        break;
    }

    gx->cpTexZ = (fmt & 0x10) == 0x10;
    peTexFmtH = (peTexFmt >> 3) & 1;
    !peTexFmt;
    SET_REG_FIELD(gx->cpTex, 1, 3, peTexFmtH);
    peTexFmt = peTexFmt & 7;
    __GetImageTileCount(fmt, wd, ht, &rowTiles, &colTiles, &cmpTiles);

    gx->cpTexStride = 0;
    SET_REG_FIELD(gx->cpTexStride, 10, 0, rowTiles * cmpTiles);
    SET_REG_FIELD(gx->cpTexStride, 8, 24, 0x4D);
    SET_REG_FIELD(gx->cpTex, 1, 9, mipmap);
    SET_REG_FIELD(gx->cpTex, 3, 4, peTexFmt);
}

void GXSetDispCopyFrame2Field(GXCopyMode arg0) {
    GX_BITFIELD_SET(gx->cpDisp, 18, 2, arg0);
    GX_BITFIELD_SET(gx->cpTex, 18, 2, 0);
}

// clang-format off
#define INSERT_FIELD(reg, value, nbits, shift)                                 \
    (reg) = ((u32) (reg) & ~(((1 << (nbits)) - 1) << (shift))) |               \
            ((u32) (value) << (shift));
// clang-format on

void GXSetCopyClamp(GXFBClamp clamp) {
    u8 clmpT;
    u8 clmpB;

    clmpT = (clamp & 1) == 1;
    clmpB = (clamp & 2) == 2;

    SET_REG_FIELD(gx->cpDisp, 1, 0, clmpT);
    SET_REG_FIELD(gx->cpDisp, 1, 1, clmpB);
    SET_REG_FIELD(gx->cpTex, 1, 0, clmpT);
    SET_REG_FIELD(gx->cpTex, 1, 1, clmpB);
}

static u32 __GXGetNumXfbLines(u32 height, u32 scale) {
    u32 numLines;
    u32 actualHeight;
    u32 newScale;

    numLines = (height - 1) * 0x100;
    actualHeight = (numLines / scale) + 1;

    newScale = scale;

    if (newScale > 0x80 && newScale < 0x100) {
        while (newScale % 2 == 0) {
            newScale /= 2;
        }

        if (height % newScale == 0) {
            actualHeight++;
        }
    }

    if (actualHeight > 0x400) {
        actualHeight = 0x400;
    }

    return actualHeight;
}

u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {
    u32 scale = (u32)(256.0f / yScale) & 0x1FF;

    return __GXGetNumXfbLines(efbHeight, scale);
}

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 scale;
    u32 height1;
    u32 height2;
    f32 scale2;
    f32 scale1;

    height1 = xfbHeight;
    scale1 = (f32)xfbHeight / (f32)efbHeight;
    scale = (u32)(256.0f / scale1) & 0x1FF;
    height2 = __GXGetNumXfbLines(efbHeight, scale);

    while (height2 > xfbHeight) {
        height1--;
        scale1 = (f32)height1 / (f32)efbHeight;
        scale = (u32)(256.0f / scale1) & 0x1FF;
        height2 = __GXGetNumXfbLines(efbHeight, scale);
    }

    scale2 = scale1;
    while (height2 < xfbHeight) {
        scale2 = scale1;
        height1++;
        scale1 = (f32)height1 / (f32)efbHeight;
        scale = (u32)(256.0f / scale1) & 0x1FF;
        height2 = __GXGetNumXfbLines(efbHeight, scale);
    }

    return scale2;
}

u32 GXSetDispCopyYScale(f32 vertScale) {
    u32 scale;
    GXBool check;
    u32 height;
    u32 reg;

    scale = (u32)(256.0f / vertScale) & 0x1FF;
    check = (scale != 0x100);

    reg = 0;
    GX_SET_REG(reg, scale, 23, 31);
    GX_SET_REG(reg, 0x4E, 0, 7);
    GX_BP_LOAD_REG(reg);
    gx->bpSentNot = GX_FALSE;

    GX_SET_REG(gx->cpDisp, check, 21, 21);

    height = (gx->cpDispSize >> 10 & 0x3FF) + 1;

    return __GXGetNumXfbLines(height, scale);
}

void GXSetCopyClear(GXColor color, u32 clear_z) {
    u32 r6 = 0;
    GX_BITFIELD_SET(r6, 24, 8, color.r);
    GX_BITFIELD_SET(r6, 16, 8, color.a);
    GX_BITFIELD_SET(r6, 0, 8, 0x4f);
    GXFIFO.u8 = 0x61;
    GXFIFO.u32 = r6;

    r6 = 0;
    GX_BITFIELD_SET(r6, 24, 8, color.b);
    GX_BITFIELD_SET(r6, 16, 8, color.g);
    GX_BITFIELD_SET(r6, 0, 8, 0x50);
    GXFIFO.u8 = 0x61;
    GXFIFO.u32 = r6;

    r6 = 0;
    GX_BITFIELD_SET(r6, 8, 24, clear_z);
    GX_BITFIELD_SET(r6, 0, 8, 0x51);
    GXFIFO.u8 = 0x61;
    GXFIFO.u32 = r6;

    gx->bpSentNot = 0;
}

void GXSetCopyFilter(GXBool useAA, u8 samplePattern[12][2], GXBool doVertFilt, u8 vFilt[7]) {
    u32 vals[4];
    u32 unk1;
    u32 unk2;

    if (useAA) {
        vals[0] = 0;
        GX_SET_REG(vals[0], samplePattern[0][0], 28, 31);
        GX_SET_REG(vals[0], samplePattern[0][1], 24, 27);
        GX_SET_REG(vals[0], samplePattern[1][0], 20, 23);
        GX_SET_REG(vals[0], samplePattern[1][1], 16, 19);
        GX_SET_REG(vals[0], samplePattern[2][0], 12, 15);
        GX_SET_REG(vals[0], samplePattern[2][1], 8, 11);
        GX_SET_REG(vals[0], 1, 0, 7);

        vals[1] = 0;
        GX_SET_REG(vals[1], samplePattern[3][0], 28, 31);
        GX_SET_REG(vals[1], samplePattern[3][1], 24, 27);
        GX_SET_REG(vals[1], samplePattern[4][0], 20, 23);
        GX_SET_REG(vals[1], samplePattern[4][1], 16, 19);
        GX_SET_REG(vals[1], samplePattern[5][0], 12, 15);
        GX_SET_REG(vals[1], samplePattern[5][1], 8, 11);
        GX_SET_REG(vals[1], 2, 0, 7);

        vals[2] = 0;
        GX_SET_REG(vals[2], samplePattern[6][0], 28, 31);
        GX_SET_REG(vals[2], samplePattern[6][1], 24, 27);
        GX_SET_REG(vals[2], samplePattern[7][0], 20, 23);
        GX_SET_REG(vals[2], samplePattern[7][1], 16, 19);
        GX_SET_REG(vals[2], samplePattern[8][0], 12, 15);
        GX_SET_REG(vals[2], samplePattern[8][1], 8, 11);
        GX_SET_REG(vals[2], 3, 0, 7);

        vals[3] = 0;
        GX_SET_REG(vals[3], samplePattern[9][0], 28, 31);
        GX_SET_REG(vals[3], samplePattern[9][1], 24, 27);
        GX_SET_REG(vals[3], samplePattern[10][0], 20, 23);
        GX_SET_REG(vals[3], samplePattern[10][1], 16, 19);
        GX_SET_REG(vals[3], samplePattern[11][0], 12, 15);
        GX_SET_REG(vals[3], samplePattern[11][1], 8, 11);
        GX_SET_REG(vals[3], 4, 0, 7);
    } else {
        vals[0] = 0x01666666;
        vals[1] = 0x02666666;
        vals[2] = 0x03666666;
        vals[3] = 0x04666666;
    }

    GX_BP_LOAD_REG(vals[0]);
    GX_BP_LOAD_REG(vals[1]);
    GX_BP_LOAD_REG(vals[2]);
    GX_BP_LOAD_REG(vals[3]);

    if (doVertFilt) {
        unk1 = 0;
        SET_REG_FIELD(unk1, 8, 24, 0x53);
        unk2 = 0;
        SET_REG_FIELD(unk2, 8, 24, 0x54);
        SET_REG_FIELD(unk1, 6, 0, vFilt[0]);
        SET_REG_FIELD(unk1, 6, 6, vFilt[1]);
        SET_REG_FIELD(unk1, 6, 12, vFilt[2]);
        SET_REG_FIELD(unk1, 6, 18, vFilt[3]);
        SET_REG_FIELD(unk2, 6, 0, vFilt[4]);
        SET_REG_FIELD(unk2, 6, 6, vFilt[5]);
        SET_REG_FIELD(unk2, 6, 12, vFilt[6]);
    } else {
        unk1 = 0x53595000;
        unk2 = 0x54000015;
    }

    GX_BP_LOAD_REG(unk1);
    GX_BP_LOAD_REG(unk2);

    gx->bpSentNot = GX_FALSE;
}

void GXSetDispCopyGamma(GXGamma gamma) {
    GX_BITFIELD_SET(gx->cpDisp, 23, 2, gamma);
}

void GXCopyDisp(void* dest, GXBool clear) {
    u32 reg;
    u32 tempPeCtrl;
    u32 phyAddr;
    u8 changePeCtrl;

    if (clear) {
        reg = gx->zmode;
        SET_REG_FIELD(reg, 1, 0, 1);
        SET_REG_FIELD(reg, 3, 1, 7);
        GX_WRITE_RAS_REG(reg);

        reg = gx->cmode0;
        SET_REG_FIELD(reg, 1, 0, 0);
        SET_REG_FIELD(reg, 1, 1, 0);
        GX_WRITE_RAS_REG(reg);
    }

    changePeCtrl = FALSE;
    if ((clear || (u32)GET_REG_FIELD(gx->peCtrl, 3, 0) == 3)
        && (u32)GET_REG_FIELD(gx->peCtrl, 1, 6) == 1) {
        changePeCtrl = TRUE;
        tempPeCtrl = gx->peCtrl;
        SET_REG_FIELD(tempPeCtrl, 1, 6, 0);
        GX_WRITE_RAS_REG(tempPeCtrl);
    }

    GX_WRITE_RAS_REG(gx->cpDispSrc);
    GX_WRITE_RAS_REG(gx->cpDispSize);
    GX_WRITE_RAS_REG(gx->cpDispStride);

    phyAddr = (u32)dest & 0x3FFFFFFF;
    reg = 0;
    SET_REG_FIELD(reg, 21, 0, phyAddr >> 5);
    SET_REG_FIELD(reg, 8, 24, 0x4B);
    GX_WRITE_RAS_REG(reg);

    SET_REG_FIELD(gx->cpDisp, 1, 11, clear);
    SET_REG_FIELD(gx->cpDisp, 1, 14, 1);
    SET_REG_FIELD(gx->cpDisp, 8, 24, 0x52);
    GX_WRITE_RAS_REG(gx->cpDisp);

    if (clear) {
        GX_WRITE_RAS_REG(gx->zmode);
        GX_WRITE_RAS_REG(gx->cmode0);
    }
    if (changePeCtrl) {
        GX_WRITE_RAS_REG(gx->peCtrl);
    }
    gx->bpSentNot = 0;
}

void GXCopyTex(void* dest, GXBool clear) {
    u32 reg;
    u32 tempPeCtrl;
    u32 phyAddr;
    u8 changePeCtrl;

    if (clear) {
        reg = gx->zmode;
        SET_REG_FIELD(reg, 1, 0, 1);
        SET_REG_FIELD(reg, 3, 1, 7);
        GX_WRITE_RAS_REG(reg);

        reg = gx->cmode0;
        SET_REG_FIELD(reg, 1, 0, 0);
        SET_REG_FIELD(reg, 1, 1, 0);
        GX_WRITE_RAS_REG(reg);
    }

    changePeCtrl = 0;
    tempPeCtrl = gx->peCtrl;
    if (((u8)gx->cpTexZ != 0) && ((u32)(tempPeCtrl & 7) != 3)) {
        changePeCtrl = 1;
        tempPeCtrl = (tempPeCtrl & 0xFFFFFFF8) | 3;
    }
    if (((clear != 0) || ((u32)(tempPeCtrl & 7) == 3))
        && ((u32)((tempPeCtrl >> 6U) & 1) == 1)) {
        changePeCtrl = 1;
        tempPeCtrl &= 0xFFFFFFBF;
    }
    if (changePeCtrl) {
        GX_WRITE_RAS_REG(tempPeCtrl);
    }

    GX_WRITE_RAS_REG(gx->cpTexSrc);
    GX_WRITE_RAS_REG(gx->cpTexSize);
    GX_WRITE_RAS_REG(gx->cpTexStride);

    phyAddr = (u32)dest & 0x3FFFFFFF;
    reg = 0;
    SET_REG_FIELD(reg, 21, 0, phyAddr >> 5);
    SET_REG_FIELD(reg, 8, 24, 0x4B);
    GX_WRITE_RAS_REG(reg);

    SET_REG_FIELD(gx->cpTex, 1, 11, clear);
    SET_REG_FIELD(gx->cpTex, 1, 14, 0);
    SET_REG_FIELD(gx->cpTex, 8, 24, 0x52);
    GX_WRITE_RAS_REG(gx->cpTex);

    if (clear != 0) {
        GX_WRITE_RAS_REG(gx->zmode);
        GX_WRITE_RAS_REG(gx->cmode0);
    }
    if (changePeCtrl) {
        GX_WRITE_RAS_REG(gx->peCtrl);
    }
    gx->bpSentNot = 0;
}

void GXClearBoundingBox(void) {
    GXFIFO.u8 = GX_LOAD_BP_REG;
    GXFIFO.u32 = 0x550003FF;
    GXFIFO.u8 = GX_LOAD_BP_REG;
    GXFIFO.u32 = 0x560003FF;
    set_x2(GX_FALSE);
}
