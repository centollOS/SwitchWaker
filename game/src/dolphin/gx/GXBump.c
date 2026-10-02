/**
 * GXBump.c
 * Description:
 */

#include "dolphin/gx/GXBump.h"
#include "dolphin/gx/GX.h"

void GXSetTevIndirect(GXTevStageID tevStage, GXIndTexStageID texStage, GXIndTexFormat texFmt,
                      GXIndTexBiasSel biasSel, GXIndTexMtxID mtxID, GXIndTexWrap wrapS,
                      GXIndTexWrap wrapT, u8 addPrev, u8 utcLod, GXIndTexAlphaSel alphaSel) {
    u32 field = 0;

    GX_BITFIELD_SET(field, 30, 2, texStage);
    GX_BITFIELD_SET(field, 28, 2, texFmt);
    GX_BITFIELD_SET(field, 25, 3, biasSel);
    GX_BITFIELD_SET(field, 23, 2, alphaSel);
    GX_BITFIELD_SET(field, 19, 4, mtxID);
    GX_BITFIELD_SET(field, 16, 3, wrapS);
    GX_BITFIELD_SET(field, 13, 3, wrapT);
    GX_BITFIELD_SET(field, 12, 1, utcLod);
    GX_BITFIELD_SET(field, 11, 1, addPrev);
    GX_BITFIELD_SET(field, 0, 8, tevStage + 0x10);

    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = field;

    gx->bpSentNot = 0;
}

void GXSetIndTexMtx(GXIndTexMtxID mtxID, f32 offset[6], s8 scale_exp) {
    s32 mtx[6];
    u32 field;
    u32 id;

    switch (mtxID) {
    case GX_ITM_0:
    case GX_ITM_1:
    case GX_ITM_2:
        id = mtxID - 1;
        break;
    case GX_ITM_S0:
    case GX_ITM_S1:
    case GX_ITM_S2:
        id = mtxID - 5;
        break;
    case GX_ITM_T0:
    case GX_ITM_T1:
    case GX_ITM_T2:
        id = mtxID - 9;
        break;
    default:
        id = 0;
        break;
    }

    mtx[0] = (int)(1024.0f * offset[0]) & 0x7FF;
    mtx[1] = (int)(1024.0f * offset[3]) & 0x7FF;
    scale_exp += 0x11;
    field = 0;
    SET_REG_FIELD(field, 11, 0, mtx[0]);
    SET_REG_FIELD(field, 11, 11, mtx[1]);
    SET_REG_FIELD(field, 2, 22, scale_exp & 3);
    SET_REG_FIELD(field, 8, 24, id * 3 + 6);
    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = field;

    mtx[2] = (int)(1024.0f * offset[1]) & 0x7FF;
    mtx[3] = (int)(1024.0f * offset[4]) & 0x7FF;
    field = 0;
    SET_REG_FIELD(field, 11, 0, mtx[2]);
    SET_REG_FIELD(field, 11, 11, mtx[3]);
    SET_REG_FIELD(field, 2, 22, (scale_exp >> 2) & 3);
    SET_REG_FIELD(field, 8, 24, id * 3 + 7);
    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = field;

    mtx[4] = (int)(1024.0f * offset[2]) & 0x7FF;
    mtx[5] = (int)(1024.0f * offset[5]) & 0x7FF;
    field = 0;
    SET_REG_FIELD(field, 11, 0, mtx[4]);
    SET_REG_FIELD(field, 11, 11, mtx[5]);
    SET_REG_FIELD(field, 2, 22, (scale_exp >> 4) & 3);
    SET_REG_FIELD(field, 8, 24, id * 3 + 8);
    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = field;

    gx->bpSentNot = 0;
}

void GXSetIndTexCoordScale(GXIndTexStageID texStage, GXIndTexScale scaleS, GXIndTexScale scaleT) {
    GXData* data;

    switch (texStage) {
    case GX_INDTEXSTAGE0:
        data = gx;
        GX_BITFIELD_SET(data->IndTexScale0, 28, 4, scaleS);
        GX_BITFIELD_SET(data->IndTexScale0, 24, 4, scaleT);
        GX_BITFIELD_SET(data->IndTexScale0, 0, 8, 0x25);
        GXFIFO.u8 = 0x61;
        GXFIFO.s32 = data->IndTexScale0;
        break;
    case GX_INDTEXSTAGE1:
        data = gx;
        GX_BITFIELD_SET(data->IndTexScale0, 20, 4, scaleS);
        GX_BITFIELD_SET(data->IndTexScale0, 16, 4, scaleT);
        GX_BITFIELD_SET(data->IndTexScale0, 0, 8, 0x25);
        GXFIFO.u8 = 0x61;
        GXFIFO.s32 = data->IndTexScale0;
        break;
    case GX_INDTEXSTAGE2:
        data = gx;
        GX_BITFIELD_SET(data->IndTexScale1, 28, 4, scaleS);
        GX_BITFIELD_SET(data->IndTexScale1, 24, 4, scaleT);
        GX_BITFIELD_SET(data->IndTexScale1, 0, 8, 0x26);
        GXFIFO.u8 = 0x61;
        GXFIFO.s32 = data->IndTexScale1;
        break;
    case GX_INDTEXSTAGE3:
        data = gx;
        GX_BITFIELD_SET(data->IndTexScale1, 20, 4, scaleS);
        GX_BITFIELD_SET(data->IndTexScale1, 16, 4, scaleT);
        GX_BITFIELD_SET(data->IndTexScale1, 0, 8, 0x26);
        GXFIFO.u8 = 0x61;
        GXFIFO.s32 = data->IndTexScale1;
        break;
    }

    gx->bpSentNot = 0;
}

void GXSetIndTexOrder(GXIndTexStageID stage, GXTexCoordID coord, GXTexMapID map) {
    switch (stage) {
    case GX_INDTEXSTAGE0:
        GX_BITFIELD_SET(gx->iref, 29, 3, map);
        GX_BITFIELD_SET(gx->iref, 26, 3, coord);
        break;
    case GX_INDTEXSTAGE1:
        GX_BITFIELD_SET(gx->iref, 23, 3, map);
        GX_BITFIELD_SET(gx->iref, 20, 3, coord);
        break;
    case GX_INDTEXSTAGE2:
        GX_BITFIELD_SET(gx->iref, 17, 3, map);
        GX_BITFIELD_SET(gx->iref, 14, 3, coord);
        break;
    case GX_INDTEXSTAGE3:
        GX_BITFIELD_SET(gx->iref, 11, 3, map);
        GX_BITFIELD_SET(gx->iref, 8, 3, coord);
        break;
    }

    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = gx->iref;
    GXSetWasteFlags();
}

void GXSetNumIndStages(u8 num) {
    GXData* data = gx;
    GX_BITFIELD_SET(data->genMode, 13, 3, num);
    data->dirtyState |= GX_DIRTY_BP_MASK | GX_DIRTY_GEN_MODE;
}

#pragma dont_inline on
void GXSetTevDirect(GXTevStageID tev_stage) {
    GXSetTevIndirect(tev_stage, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_OFF,
                     GX_ITW_OFF, 0U, 0, 0);
}

void GXSetTevIndWarp(GXTevStageID tev_stage, GXIndTexStageID ind_stage, u8 signed_offset,
                     u8 replace_mode, GXIndTexMtxID matrix_sel) {
    GXIndTexWrap wrap = (replace_mode != 0) ? GX_ITW_0 : GX_ITW_OFF;

    GXSetTevIndirect(tev_stage, ind_stage, GX_ITF_8,
                     (signed_offset != 0) ? GX_ITB_STU : GX_ITB_NONE, matrix_sel, wrap, wrap, 0U, 0,
                     0);
}
#pragma dont_inline reset

void __GXUpdateBPMask(void) {
    u32 nIndStages;
    u32 i;
    u32 tmap;
    u32 new_imask;
    u32 nStages;
    u32 new_dmask;

    new_imask = 0;
    new_dmask = 0;
    nIndStages = GET_REG_FIELD(gx->genMode, 3, 16);
    for (i = 0; i < nIndStages; i++) {
        switch (i) {
        case 0:
            tmap = GET_REG_FIELD(gx->iref, 3, 0);
            break;
        case 1:
            tmap = GET_REG_FIELD(gx->iref, 3, 6);
            break;
        case 2:
            tmap = GET_REG_FIELD(gx->iref, 3, 12);
            break;
        case 3:
            tmap = GET_REG_FIELD(gx->iref, 3, 18);
            break;
        }
        new_imask |= 1 << tmap;
    }

    if ((u8)gx->bpMask != new_imask) {
        SET_REG_FIELD(gx->bpMask, 8, 0, new_imask);
        GXFIFO.u8 = 0x61;
        GXFIFO.s32 = gx->bpMask;
        gx->bpSentNot = 0;
    }
}

void __GXSetIndirectMask(u32 mask) {
    GXData* data = gx;

    GX_BITFIELD_SET(data->bpMask, 24, 8, mask);
    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = data->bpMask;
    data->bpSentNot = 0;
}

void __GXFlushTextureState(void) {
    GXFIFO.u8 = 0x61;
    GXFIFO.s32 = gx->bpMask;
    gx->bpSentNot = 0;
}
