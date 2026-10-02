/**
 * GXLight.c
 * Description:
 */

#include "dolphin/gx/GXLight.h"
#include "dolphin/gx/GX.h"
#include "math.h"

void GXInitLightAttn(GXLightObj* obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    obj->a0 = a0;
    obj->a1 = a1;
    obj->a2 = a2;
    obj->k0 = k0;
    obj->k1 = k1;
    obj->k2 = k2;
}

void GXInitLightSpot(GXLightObj* obj, f32 cutoff, GXSpotFn spot_fn) {
    f32 rad;
    f32 a, b, c, d;

    if (cutoff <= 0.0f || cutoff > 90.0f) {
        spot_fn = GX_SP_OFF;
    }

    rad = cosf((3.1415927f * cutoff) / 180.0f);

    switch (spot_fn) {
    case GX_SP_FLAT:
        a = -1000.0f * rad;
        b = 1000.0f;
        c = 0.0f;
        break;
    case GX_SP_COS:
        b = 1.0f / (1.0f - rad);
        a = -rad * b;
        c = 0.0f;
        break;
    case GX_SP_COS2:
        c = 1.0f / (1.0f - rad);
        a = 0.0f;
        b = -rad * c;
        break;
    case GX_SP_SHARP:
        d = 1.0f / ((1.0f - rad) * (1.0f - rad));
        a = (rad * (rad - 2.0f)) * d;
        b = 2.0f * d;
        c = -d;
        break;
    case GX_SP_RING1:
        d = 1.0f / ((1.0f - rad) * (1.0f - rad));
        c = -4.0f * d;
        a = c * rad;
        b = 4.0f * (1.0f + rad) * d;
        break;
    case GX_SP_RING2:
        d = 1.0f / ((1.0f - rad) * (1.0f - rad));
        a = 1.0f - 2.0f * rad * rad * d;
        b = 4.0f * rad * d;
        c = -2.0f * d;
        break;
    case GX_SP_OFF:
    default:
        a = 1.0f;
        b = 0.0f;
        c = 0.0f;
        break;
    }

    obj->a0 = a;
    obj->a1 = b;
    obj->a2 = c;
}

void GXInitLightDistAttn(GXLightObj* obj, f32 dist, f32 brightness, GXDistAttnFn dist_fn) {
    f32 a, b, c;

    if (dist < 0.0f) {
        dist_fn = GX_DA_OFF;
    }

    if (brightness <= 0.0f || brightness >= 1.0f) {
        dist_fn = GX_DA_OFF;
    }

    switch (dist_fn) {
    case GX_DA_GENTLE:
        a = 1.0f;
        b = (1.0f - brightness) / (brightness * dist);
        c = 0.0f;
        break;
    case GX_DA_MEDIUM:
        a = 1.0f;
        b = (0.5f * (1.0f - brightness)) / (brightness * dist);
        c = (0.5f * (1.0f - brightness)) / (dist * (brightness * dist));
        break;
    case GX_DA_STEEP:
        a = 1.0f;
        b = 0.0f;
        c = (1.0f - brightness) / (dist * (brightness * dist));
        break;
    case GX_DA_OFF:
    default:
        a = 1.0f;
        b = 0.0f;
        c = 0.0f;
        break;
    }

    obj->k0 = a;
    obj->k1 = b;
    obj->k2 = c;
}

void GXInitLightPos(GXLightObj* obj, f32 x, f32 y, f32 z) {
    obj->posX = x;
    obj->posY = y;
    obj->posZ = z;
}

void GXInitLightDir(GXLightObj* obj, f32 x, f32 y, f32 z) {
    obj->dirX = -x;
    obj->dirY = -y;
    obj->dirZ = -z;
}

void GXInitLightColor(GXLightObj* obj, GXColor color) {
    *(u32*)&obj->color = *(u32*)&color;
}

static inline void PushLight(const register GXLightObj* lt_obj, register void* dest) {
    register u32 zero, color;
    register f32 a0_a1, a2_k0, k1_k2;
    register f32 px_py, pz_dx, dy_dz;
#ifdef __MWERKS__  // clang-format off
    asm {
        lwz     color, 12(lt_obj)
        xor     zero, zero, zero
        psq_l   a0_a1, 16(lt_obj), 0, 0
        psq_l   a2_k0, 24(lt_obj), 0, 0
        psq_l   k1_k2, 32(lt_obj), 0, 0
        psq_l   px_py, 40(lt_obj), 0, 0
        psq_l   pz_dx, 48(lt_obj), 0, 0
        psq_l   dy_dz, 56(lt_obj), 0, 0

        stw     zero,  0(dest)
        stw     zero,  0(dest)
        stw     zero,  0(dest)
        stw     color, 0(dest)
        psq_st  a0_a1, 0(dest), 0, 0
        psq_st  a2_k0, 0(dest), 0, 0
        psq_st  k1_k2, 0(dest), 0, 0
        psq_st  px_py, 0(dest), 0, 0
        psq_st  pz_dx, 0(dest), 0, 0
        psq_st  dy_dz, 0(dest), 0, 0
    }
#endif  // clang-format on
}

void GXLoadLightObjImm(GXLightObj* obj, GXLightID light) {
    u32 addr;
    u32 idx;
    GXLightObj* pObj = (GXLightObj*)obj;

    idx = 31 - __cntlzw(light);
    idx &= 7;

    addr = 0x600 + idx * 0x10;

    GX_XF_LOAD_REG_HDR(addr | (0x10 - 1) << 16);

    PushLight(pObj, (void*)GXFIFO_ADDR);
    gx->bpSentNot = 1;
}

void GXSetChanAmbColor(GXChannelID chan, GXColor color) {
    u32 reg;
    u32 colorID;

    switch (chan) {
    case GX_COLOR0:
        colorID = 0;
        reg = gx->ambColor[0] & 0xFF;
        reg |= GXCOLOR_AS_U32(color) & 0xFFFFFF00;
        break;
    case GX_COLOR1:
        colorID = 1;
        reg = gx->ambColor[1] & 0xFF;
        reg |= GXCOLOR_AS_U32(color) & 0xFFFFFF00;
        break;
    case GX_ALPHA0:
        colorID = 0;
        reg = gx->ambColor[0] & 0xFFFFFF00;
        reg |= color.a;
        break;
    case GX_ALPHA1:
        colorID = 1;
        reg = gx->ambColor[1] & 0xFFFFFF00;
        reg |= color.a;
        break;
    case GX_COLOR0A0:
        reg = GXCOLOR_AS_U32(color);
        colorID = 0;
        break;
    case GX_COLOR1A1:
        reg = GXCOLOR_AS_U32(color);
        colorID = 1;
        break;
    default:
        return;
    }

    GX_WRITE_XF_REG(colorID + 10, reg);
    gx->bpSentNot = GX_TRUE;
    gx->ambColor[colorID] = reg;
}

void GXSetChanMatColor(GXChannelID chan, GXColor color) {
    u32 reg;
    u32 colorID;

    switch (chan) {
    case GX_COLOR0:
        colorID = 0;
        reg = gx->matColor[0] & 0xFF;
        reg |= GXCOLOR_AS_U32(color) & 0xFFFFFF00;
        break;
    case GX_COLOR1:
        colorID = 1;
        reg = gx->matColor[1] & 0xFF;
        reg |= GXCOLOR_AS_U32(color) & 0xFFFFFF00;
        break;
    case GX_ALPHA0:
        colorID = 0;
        reg = gx->matColor[0] & 0xFFFFFF00;
        reg |= color.a;
        break;
    case GX_ALPHA1:
        colorID = 1;
        reg = gx->matColor[1] & 0xFFFFFF00;
        reg |= color.a;
        break;
    case GX_COLOR0A0:
        reg = GXCOLOR_AS_U32(color);
        colorID = 0;
        break;
    case GX_COLOR1A1:
        reg = GXCOLOR_AS_U32(color);
        colorID = 1;
        break;
    default:
        return;
    }

    GX_WRITE_XF_REG(colorID + 12, reg);
    gx->bpSentNot = GX_TRUE;
    gx->matColor[colorID] = reg;
}

void GXSetNumChans(u8 nChans) {
    SET_REG_FIELD(gx->genMode, 3, 4, nChans);
    GX_WRITE_XF_REG(9, nChans);
    gx->dirtyState |= GX_DIRTY_GEN_MODE;
}

void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src,
                   u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn) {
    u32 reg = 0;
    u32 idx = (u32)chan & 0x3;

    SET_REG_FIELD(reg, 1, 1, enable);
    SET_REG_FIELD(reg, 1, 0, mat_src);
    SET_REG_FIELD(reg, 1, 6, amb_src);
    SET_REG_FIELD(reg, 2, 7, (attn_fn == GX_AF_SPEC) ? GX_DF_NONE : diff_fn);
    SET_REG_FIELD(reg, 1, 9, attn_fn != GX_AF_NONE);
    SET_REG_FIELD(reg, 1, 10, attn_fn != GX_AF_SPEC);
    SET_REG_FIELD(reg, 4, 2, light_mask & 0xF);
    SET_REG_FIELD(reg, 4, 11, (light_mask >> 4) & 0xF);

    GX_WRITE_XF_REG(idx + 14, reg);

    if (chan == GX_COLOR0A0) {
        GX_WRITE_XF_REG(16, reg);
    } else if (chan == GX_COLOR1A1) {
        GX_WRITE_XF_REG(17, reg);
    }

    gx->bpSentNot = GX_TRUE;
}
