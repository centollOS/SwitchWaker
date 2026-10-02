// Floating buoy and simulated cloth flag.

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_buoyflag.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo_wether.h"
#include "d/actor/d_a_sea.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"
#include "res/Object/Cloth.h"
#include "JSystem/JUtility/JUTTexture.h"


namespace daObjBuoyflag {
namespace {
static const char L_arcname[] = "Cloth";
struct Attr_c {
    f32 spring, gravity, windScale, minWind, drag, wave;
    f32 fallGravity, airXZ, airY, seaXZ, seaY;
    s16 tiltSpeed;
    f32 airTiltDrag, seaTiltDrag, tiltSpring;
};
static const Attr_c L_attr = {0.5f, -0.03f, 20.0f, 0.5f, 0.03f, 0.6f, -8.0f, 0.006f, 0.002f, 0.04f, 0.015f, 1000, 0.03f, 0.08f, 0.005f};
inline const Attr_c& attr() { return L_attr; }
namespace Khasi {
#include "assets/l_k_taru02TEX__d_a_obj_buoyflag.h"
static Vec l_pos[11] = {
    {1.335263f, -0.151631f, -4.755283f},
    {-4.254907f, -0.151631f, -2.938927f},
    {-4.254908f, -0.151631f, 2.938926f},
    {1.335263f, -0.151631f, 4.755283f},
    {4.790178f, -0.151631f, 0.0f},
    {0.986666f, 97.26365f, -3.202602f},
    {-2.778212f, 97.26365f, -1.979313f},
    {-2.778212f, 97.26365f, 1.979313f},
    {0.986663f, 97.26365f, 3.202602f},
    {3.313482f, 97.26365f, 0.0f},
    {-0.053924f, 99.66593f, 0.0f},
};
static f32 l_texCoord[7][2] = {
    {0.0302949995f, 0.0f},
    {0.449999988f, 0.0f},
    {0.449999988f, 1.0f},
    {0.0302949995f, 1.0f},
    {0.253329009f, 0.0f},
    {0.454356015f, 1.0f},
    {0.0f, 1.0f},
};
}
namespace Khasi {
#include "assets/l_Khasi_00DL__d_a_obj_buoyflag.h"
}
namespace Khata {
#include "assets/l_k_hata01TEX__d_a_obj_buoyflag.h"
static Vec l_pos[35] = {
    {0.0f, -22.499968f, 0.0f},
    {12.5f, -22.499968f, 0.0f},
    {25.0f, -22.499968f, 0.0f},
    {37.5f, -22.499968f, 0.0f},
    {50.0f, -22.499968f, 0.0f},
    {62.5f, -22.499968f, 0.0f},
    {75.0f, -22.499968f, 0.0f},
    {0.0f, -9.999967f, 0.0f},
    {12.5f, -9.999967f, 0.0f},
    {25.0f, -9.999967f, 0.0f},
    {37.5f, -9.999967f, 0.0f},
    {50.0f, -9.999967f, 0.0f},
    {62.5f, -9.999967f, 0.0f},
    {75.0f, -9.999967f, 0.0f},
    {0.0f, 2.500033f, 0.0f},
    {12.5f, 2.500033f, 0.0f},
    {25.0f, 2.500033f, 0.0f},
    {37.5f, 2.500033f, 0.0f},
    {50.0f, 2.500033f, 0.0f},
    {62.5f, 2.500033f, 0.0f},
    {75.0f, 2.500033f, 0.0f},
    {0.0f, 15.000032f, 0.0f},
    {12.5f, 15.000032f, 0.0f},
    {25.0f, 15.000032f, 0.0f},
    {37.5f, 15.000032f, 0.0f},
    {50.0f, 15.000032f, 0.0f},
    {62.5f, 15.000032f, 0.0f},
    {75.0f, 15.000032f, 0.0f},
    {0.0f, 27.500032f, 0.0f},
    {12.5f, 27.500032f, 0.0f},
    {25.0f, 27.500032f, 0.0f},
    {37.5f, 27.500032f, 0.0f},
    {50.0f, 27.500032f, 0.0f},
    {62.5f, 27.500032f, 0.0f},
    {75.0f, 27.500032f, 0.0f},
};
static f32 l_texCoord[35][2] = {
    {0.833333015f, 0.75f},
    {1.0f, 0.75f},
    {1.0f, 1.0f},
    {0.833333015f, 1.0f},
    {0.5f, 0.75f},
    {0.666666985f, 0.75f},
    {0.666666985f, 1.0f},
    {0.5f, 1.0f},
    {0.333332986f, 0.0f},
    {0.333332986f, 0.25f},
    {0.166666999f, 0.25f},
    {0.166666999f, 0.0f},
    {0.0f, 0.25f},
    {0.0f, 0.0f},
    {0.333332986f, 0.75f},
    {0.333332986f, 1.0f},
    {0.166666999f, 1.0f},
    {0.0f, 1.0f},
    {0.0f, 0.75f},
    {0.166666999f, 0.75f},
    {0.833333015f, 0.5f},
    {1.0f, 0.5f},
    {0.666666985f, 0.5f},
    {0.5f, 0.5f},
    {0.333332986f, 0.5f},
    {0.0f, 0.5f},
    {0.166666999f, 0.5f},
    {0.833333015f, 0.25f},
    {1.0f, 0.25f},
    {0.666666985f, 0.25f},
    {0.5f, 0.25f},
    {0.833333015f, 0.0f},
    {1.0f, 0.0f},
    {0.666666985f, 0.0f},
    {0.5f, 0.0f},
};
}
}
namespace {
namespace Khata {
#include "assets/l_Khata_00DL__d_a_obj_buoyflag.h"
#include "assets/l_matDL__d_a_obj_buoyflag.h"
} // namespace Khata
} // namespace
cXyz Packet_c::M_hasi_nrm[11];
bool Packet_c::M_hasi_nrm_flag;

}

inline void daObjBuoyflag::Packet_c::calc_pos_spring(int y, int x) {
    DrawVtx_c* prev = &mDraw[mBuffer ^ 1];
    cXyz* pos = &prev->pos[y][x];
    if (x > 0) calc_pos_spring_near(pos, &prev->pos[y][x - 1], 12.5f, attr().spring);
    if (x < 6) calc_pos_spring_near(pos, &prev->pos[y][x + 1], 12.5f, attr().spring);
    if (y > 0) calc_pos_spring_near(pos, &prev->pos[y - 1][x], 12.5f, 0.8f * attr().spring);
    if (y < 4) calc_pos_spring_near(pos, &prev->pos[y + 1][x], 12.5f, attr().spring);
}
inline void daObjBuoyflag::Packet_c::calc_pos_gravity(int y, int x) {
    f32 a = 0.25f * (4 - y);
    f32 b = (1.0f / 6.0f) * x;
    f32 g = 0.5f * ((a + b) * attr().gravity);
    mForce += mGravity * g;
}
inline void daObjBuoyflag::Packet_c::calc_pos_wave(int y, int x) {
    DrawVtx_c* prev = &mDraw[mBuffer ^ 1];
    const cXyz* normal = &prev->normal[y][x];
    f32 a = 0.25f * y - 0.5f;
    f32 b = (1.0f / 6.0f) * x;
    f32 distance = std::sqrtf(a * a + b * b);
    s16 angle1 = (int)(32768.0f * distance + mPhase[9]);
    s16 angle2 = (int)(32768.0f * distance + mPhase[10]);
    s16 angle3 = 32768.0f * distance + mPhase[11];
#if VERSION == VERSION_DEMO
    f32 wave = 1.0f + (1.0f / 3.0f) * (cM_ssin(angle1) + cM_ssin(angle2) + cM_ssin(angle3));
#else
    // fakematch: the sine table lookups are written out so the sum is computed before the inprod call
    f32 wave = 1.0f + (1.0f / 3.0f) * (
        jmaSinTable[(u16)angle1 >> jmaSinShift] +
        jmaSinTable[(u16)angle2 >> jmaSinShift] +
        jmaSinTable[(u16)angle3 >> jmaSinShift]);
#endif
    f32 dot = normal->inprod(mWind);
    mForce += *normal * (dot * (wave * attr().wave * (1.0f / attr().windScale)));
}
inline void daObjBuoyflag::Packet_c::calc_pos_spd(int y, int x) {
    cXyz* speed = &mMove.speed[y][x];
    *speed += mForce;
    f32 drag = -((0.6f + 0.4f * ((1.0f / 6.0f) * x)) * attr().drag);
    cXyz wind, relative, friction;
    wind = mWind;
    cXyz random(0.9f + cM_rndF(0.2f), 0.9f + cM_rndF(0.2f), 0.9f + cM_rndF(0.2f));
    wind *= random;
    relative = *speed - wind;
    friction = relative * drag;
    *speed += friction;
}

/* 000000EC-000001BC       .text hasi_nrm_init__Q213daObjBuoyflag8Packet_cFv */
void daObjBuoyflag::Packet_c::hasi_nrm_init() {
    if (!M_hasi_nrm_flag) {
        M_hasi_nrm_flag = true;
        const Vec* pos = Khasi::l_pos;
        for (int i = 0; i < 10; i++, pos++) {
            M_hasi_nrm[i].set(pos->x, 0.0f, pos->z);
            M_hasi_nrm[i].normalize();
        }
        M_hasi_nrm[10].set(0.0f, 1.0f, 0.0f);
    }
}

/* 000001F8-00000744       .text draw_hata__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::draw_hata(Act_c* actor) {
    dKy_tevstr_c* tev = &actor->tevStr;
    DrawVtx_c* draw = &mDraw[mBuffer];
    u8* texture = &Khata::l_k_hata01TEX[actor->prm_get_texture() * 0x800];
    j3dSys.reinitGX();
#if VERSION > VERSION_JPN
    GXSetNumIndStages(0);
#endif
    dKy_GxFog_tevstr_set(tev);
    dKy_setLight_mine(tev);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxDesc(GX_VA_NRM, GX_INDEX8);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetArray(GX_VA_POS, draw->pos, sizeof(cXyz));
    GXSetArray(GX_VA_NRM, draw->normal, sizeof(cXyz));
    GXSetArray(GX_VA_TEX0, Khata::l_texCoord, sizeof(f32) * 2);
    GXTexObj texObj;
    GXInitTexObj(&texObj, texture, 64, 64, GX_TF_CMPR, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&texObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    ResTIMG* texInfo = (ResTIMG*)dComIfG_getObjectRes(L_arcname, dRes_INDEX_CLOTH_BTI_CLOTHTOON_e);
    u8* image = (u8*)texInfo;
    image += texInfo->imageOffset;
    GXInitTexObj(&texObj, image, texInfo->width, texInfo->height,
        GXTexFmt(texInfo->format), GXTexWrapMode(texInfo->wrapS), GXTexWrapMode(texInfo->wrapT), GXBool(texInfo->mipmapCount > 1));
    GXInitTexObjLOD(&texObj, GXTexFilter(texInfo->minFilter), GXTexFilter(texInfo->magFilter),
        texInfo->minLOD * 0.125f, texInfo->maxLOD * 0.125f, texInfo->LODBias * 0.01f,
        texInfo->biasClamp, texInfo->doEdgeLOD, GXAnisotropy(texInfo->maxAnisotropy));
    GXLoadTexObj(&texObj, GX_TEXMAP1);
    GXSetNumChans(1);
    u8 stages, lights;
    if (tev->mColorK1.a) {
        stages = 3;
        lights = GX_LIGHT0 | GX_LIGHT1;
    } else {
        stages = 2;
        lights = GX_LIGHT0;
    }
    GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, lights, GX_DF_CLAMP, GX_AF_NONE);
    GXSetNumTexGens(2);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_SRTG, GX_TG_COLOR0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetNumTevStages(stages);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_CPREV, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    if (stages == 3) {
        GXSetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP0, GX_TEV_SWAP2);
        GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_C2, GX_CC_TEXC, GX_CC_CPREV);
        GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    }
    GXSetTevColorS10(GX_TEVREG0, tev->mColorC0);
    GXSetTevColor(GX_TEVREG1, tev->mColorK0);
    GXSetTevColor(GX_TEVREG2, tev->mColorK1);
    GXCallDisplayList(Khata::l_matDL, 0x20);
    GXLoadPosMtxImm(mFlagViewMtx, GX_PNMTX0);
    GXLoadNrmMtxImm(mFlagViewMtx, GX_PNMTX0);
    GXSetCullMode(GX_CULL_BACK);
    GXCallDisplayList(Khata::l_Khata_00DL, 0xE0);
    GXSetCullMode(GX_CULL_FRONT);
    GXSetArray(GX_VA_NRM, draw->backNormal, sizeof(cXyz));
    GXCallDisplayList(Khata::l_Khata_00DL, 0xE0);
}

/* 00000744-00000C4C       .text draw_hasi__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::draw_hasi(Act_c* actor) {
    dKy_tevstr_c* tev = &actor->tevStr;
    j3dSys.reinitGX();
#if VERSION > VERSION_JPN
    GXSetNumIndStages(0);
#endif
    dKy_GxFog_tevstr_set(tev);
    dKy_setLight_mine(tev);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
    GXSetVtxDesc(GX_VA_NRM, GX_INDEX8);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetArray(GX_VA_POS, Khasi::l_pos, sizeof(cXyz));
    GXSetArray(GX_VA_NRM, M_hasi_nrm, sizeof(cXyz));
    GXSetArray(GX_VA_TEX0, Khasi::l_texCoord, sizeof(f32) * 2);
    GXTexObj texObj;
    GXInitTexObj(&texObj, Khasi::l_k_taru02TEX, 32, 64, GX_TF_CMPR, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&texObj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    ResTIMG* texInfo = (ResTIMG*)dComIfG_getObjectRes(L_arcname, dRes_INDEX_CLOTH_BTI_CLOTHTOON_e);
    u8* image = (u8*)texInfo;
    image += texInfo->imageOffset;
    GXInitTexObj(&texObj, image, texInfo->width, texInfo->height,
        GXTexFmt(texInfo->format), GXTexWrapMode(texInfo->wrapS), GXTexWrapMode(texInfo->wrapT), GXBool(texInfo->mipmapCount > 1));
    GXInitTexObjLOD(&texObj, GXTexFilter(texInfo->minFilter), GXTexFilter(texInfo->magFilter),
        texInfo->minLOD * 0.125f, texInfo->maxLOD * 0.125f, texInfo->LODBias * 0.01f,
        texInfo->biasClamp, texInfo->doEdgeLOD, GXAnisotropy(texInfo->maxAnisotropy));
    GXLoadTexObj(&texObj, GX_TEXMAP1);
    GXSetNumChans(1);
    u8 stages, lights;
    if (tev->mColorK1.a) {
        stages = 3;
        lights = GX_LIGHT0 | GX_LIGHT1;
    } else {
        stages = 2;
        lights = GX_LIGHT0;
    }
    GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, lights, GX_DF_CLAMP, GX_AF_NONE);
    GXSetNumTexGens(2);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_SRTG, GX_TG_COLOR0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetNumTevStages(stages);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_CPREV, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    if (stages == 3) {
        GXSetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP0, GX_TEV_SWAP2);
        GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
        GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_C2, GX_CC_TEXC, GX_CC_CPREV);
        GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    }
    GXSetTevColorS10(GX_TEVREG0, tev->mColorC0);
    GXSetTevColor(GX_TEVREG1, tev->mColorK0);
    GXSetTevColor(GX_TEVREG2, tev->mColorK1);
    GXCallDisplayList(Khata::l_matDL, 0x20);
    GXLoadPosMtxImm(mPoleViewMtx, GX_PNMTX0);
    GXLoadNrmMtxImm(mPoleViewMtx, GX_PNMTX0);
    GXSetCullMode(GX_CULL_BACK);
    GXCallDisplayList(Khasi::l_Khasi_00DL, 0xA0);
}

/* 00000C4C-00000CB8       .text draw__Q213daObjBuoyflag8Packet_cFv */
void daObjBuoyflag::Packet_c::draw() {
    Act_c* actor = (Act_c*)getUserArea();
    if (actor->attr_type().flag) draw_hata(actor);
    draw_hasi(actor);
#if VERSION > VERSION_JPN
    J3DShape::resetVcdVatCache();
#endif
}

/* 00000CB8-00000F7C       .text init__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::init(Act_c* actor) {
    static const cXyz back(0.0f, 0.0f, -1.0f);
    for (int b = 0; b < 2; b++) {
        DrawVtx_c* draw = &mDraw[b];
        int y, index;
        for (y = 0, index = 0; y < 5; y++) {
            for (int x = 0; x < 7; x++, index++) {
                draw->pos[y][x] = Khata::l_pos[index];
                draw->normal[y][x] = cXyz::BaseZ;
                draw->backNormal[y][x] = back;
            }
        }
    }
    mBuffer = 0;
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 7; x++) {
            mMove.speed[y][x] = cXyz::Zero;
        }
    }
    mPhase[0] = cM_rndFX(32768.0f);
    mPhase[1] = cM_rndFX(32768.0f);
    mPhase[2] = cM_rndFX(32768.0f);
    mPhase[3] = cM_rndFX(32768.0f);
    mPhase[4] = cM_rndFX(32768.0f);
    mPhase[5] = cM_rndFX(32768.0f);
    mPhase[6] = cM_rndFX(32768.0f);
    mPhase[7] = cM_rndFX(32768.0f);
    mPhase[8] = cM_rndFX(32768.0f);
    mPhase[9] = cM_rndFX(32768.0f);
    mPhase[10] = cM_rndFX(32768.0f);
    mPhase[11] = cM_rndFX(32768.0f);
    calc(actor);
}

/* 00000F7C-000014AC       .text calc_wind_base__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::calc_wind_base(Act_c* actor) {
    // USA: the two square-root temporary stack slots are reversed.
    DrawVtx_c* prev = &mDraw[mBuffer ^ 1];
    mPhase[0] += 1400;
    mPhase[1] += 5500;
    mPhase[2] += 1680;
    mPhase[3] += 7650;
    mPhase[4] += 1900;
    mPhase[5] += 6500;
    if (cM_rnd() < 0.05f) {
        mPhase[0] += 2000;
        mPhase[1] += 2000;
        mPhase[2] += 2000;
        mPhase[3] += 2000;
        mPhase[4] += 2000;
        mPhase[5] += 2000;
    }
    mPhase[6] += 430;
    mPhase[7] += 1500;
    mPhase[8] += 8000;
    mPhase[9] += 1000;
    mPhase[10] += 3210;
    mPhase[11] += 8765;
    int x = 4096.0f * cM_ssin(mPhase[0]) + 1024.0f * cM_ssin(mPhase[1]);
    int y = 4608.0f * cM_ssin(mPhase[2]) + 1536.0f * cM_ssin(mPhase[3]);
    int z = 5632.0f * cM_ssin(mPhase[4]) + 2048.0f * cM_ssin(mPhase[5]);
    f32 wave = cM_rndF(0.2f) + (0.2f + 0.35f * (1.0f + (0.15f * cM_ssin(mPhase[8]) + (0.5f * cM_ssin(mPhase[6]) + 0.35f * cM_ssin(mPhase[7])))));
    cXyz wind = dKyw_get_AllWind_vecpow(&prev->pos[0][0]);
    wind *= 0.5f * wave * attr().windScale;
    cXyz motion(0.2f * (actor->m10C0[0][3] - actor->m1090[0][3]), 0.2f * (actor->m10C0[1][3] - actor->m1090[1][3]), 0.2f * (actor->m10C0[2][3] - actor->m1090[2][3]));
    f32 mag2 = motion.abs2();
    if (mag2 > 625.0f) {
        motion /= mag2;
        motion *= 25.0f;
    }
    wind += motion;
    mDoMtx_stack_c::copy(actor->m1090);
    mDoMtx_stack_c::inverse();
    mDoMtx_stack_c::multVecSR(&cXyz::BaseY, &mGravity);
    mDoMtx_stack_c::ZXYrotM(x, y, z);
    mDoMtx_stack_c::multVecSR(&wind, &mWind);
    mag2 = mWind.abs2();
    if (mag2 < attr().minWind * attr().minWind) {
        if (mag2 < 0.001f) {
            mDoMtx_stack_c::multVecSR(&cXyz::BaseZ, &mWind);
            mWind *= attr().minWind;
        } else {
            f32 scale = attr().minWind / std::sqrtf(mag2);
            mWind *= scale;
        }
    }
}

/* 000014AC-000015FC       .text calc_pos_spring_near__Q213daObjBuoyflag8Packet_cFPC4cXyzPC4cXyzff */
void daObjBuoyflag::Packet_c::calc_pos_spring_near(const cXyz* pos, const cXyz* near, float distance, float spring) {
    cXyz delta = *pos - *near;
    f32 length = delta.abs();
    if (length > 0.01f) mForce += delta * (length - distance) * -spring / length;
}

/* 000015FC-00001BC0       .text calc_pos__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::calc_pos(Act_c* actor) {
    DrawVtx_c* draw = &mDraw[mBuffer];
    DrawVtx_c* prev = &mDraw[mBuffer ^ 1];
    calc_wind_base(actor);
    for (int y = 0; y < 5; y++) {
        for (s32 x = 0; x < 7; x++) {
            if ((y == 0 && x == 0) || (y == 4 && x == 0)) continue;
            mForce = cXyz::Zero;
            calc_pos_spring(y, x);
            calc_pos_gravity(y, x);
            calc_pos_wave(y, x);
            calc_pos_spd(y, x);
            draw->pos[y][x] = prev->pos[y][x] + mMove.speed[y][x];
        }
    }
}

/* 00001BC0-000020E4       .text calc_nrm__Q213daObjBuoyflag8Packet_cFv */
void daObjBuoyflag::Packet_c::calc_nrm() {
    DrawVtx_c* draw = &mDraw[mBuffer];
    DrawVtx_c* prev = &mDraw[mBuffer ^ 1];
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 7; x++) {
            const cXyz& pos = prev->pos[y][x];
            const cXyz& down = prev->pos[y + 1][x];
            const cXyz& up = prev->pos[y - 1][x];
            const cXyz& right = prev->pos[y][x + 1];
            const cXyz& left = prev->pos[y][x - 1];
            cXyz vertical, horizontal;
            if (y == 0) vertical = down - pos;
            else if (y == 4) vertical = pos - up;
            else {
                cXyz a = pos - up;
                cXyz b = down - pos;
                cXyz c = up * 0.57475f;
                c += a * 0.358875f;
                c += b * 0.111375f;
                c += down * 0.425249964f;
                cXyz d = up * 0.425249964f;
                d += a * 0.383625f;
                d += b * 0.136125f;
                d += down * 0.57475f;
                vertical = d - c;
            }
            if (x == 0) horizontal = right - pos;
            else if (x == 6) horizontal = pos - left;
            else {
                cXyz a = pos - left;
                cXyz b = right - pos;
                cXyz c = left * 0.57475f;
                c += a * 0.358875f;
                c += b * 0.111375f;
                c += right * 0.425249964f;
                cXyz d = left * 0.425249964f;
                d += a * 0.383625f;
                d += b * 0.136125f;
                d += right * 0.57475f;
                horizontal = d - c;
            }
            cXyz normal = horizontal.outprod(vertical);
            if (normal.normalizeRS()) {
                draw->normal[y][x] = normal;
                draw->backNormal[y][x] = normal;
                draw->backNormal[y][x] *= -1.0f;
            }
        }
    }
}

/* 000020E4-0000220C       .text calc__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::calc(Act_c* actor) {
    mDoMtx_stack_c::copy(actor->m1090);
    mDoMtx_stack_c::scaleM(actor->scale);
    cMtx_copy(mDoMtx_stack_c::get(), mPoleMtx);
    if (actor->attr_type().flag) {
        static const cXyz offset(0.0f, 60.0f, 0.0f);
        mDoMtx_stack_c::copy(actor->m1090);
        mDoMtx_stack_c::scaleM(actor->scale);
        mDoMtx_stack_c::transM(offset);
        cMtx_copy(mDoMtx_stack_c::get(), mFlagMtx);
        mBuffer ^= 1;
        calc_pos(actor);
        calc_nrm();
    }
}

/* 0000220C-00002294       .text update__Q213daObjBuoyflag8Packet_cFPQ213daObjBuoyflag5Act_c */
void daObjBuoyflag::Packet_c::update(Act_c* actor) {
    PSMTXConcat(j3dSys.getViewMtx(), mPoleMtx, mPoleViewMtx);
    if (actor->attr_type().flag) PSMTXConcat(j3dSys.getViewMtx(), mFlagMtx, mFlagViewMtx);
    j3dSys.getDrawBuffer(0)->entryImm(this, 0);
}

const dCcD_SrcCyl daObjBuoyflag::Act_c::M_cyl_src = {
    { {0, {{0, 0, 0}, {0xFF1DFEFF, 9}, 0x79}}, {0, 0, 1, 0, 0}, {0, 0, 0, 0, 1}, {0} },
    {{{0.0f, 12.0f, 0.0f}, 5.0f, 88.0f}}
};
const daObjBuoyflag::AttrType_c daObjBuoyflag::Act_c::M_attr_type[4] = {{1.0f, true, true}, {4.0f, false, true}, {5.76f, false, true}, {2.0f, false, false}};

/* 00002294-000022BC       .text prm_get_texture__Q213daObjBuoyflag5Act_cCFv */
int daObjBuoyflag::Act_c::prm_get_texture() const {
    return daObj::PrmAbstract(this, PRM_TEXTURE_W, PRM_TEXTURE_S);
}

/* 000022BC-000022D4       .text attr_type__Q213daObjBuoyflag5Act_cCFv */
inline const daObjBuoyflag::AttrType_c& daObjBuoyflag::Act_c::attr_type() const {
    return M_attr_type[mType];
}

/* 000022D4-0000234C       .text mtx_init__Q213daObjBuoyflag5Act_cFv */
void daObjBuoyflag::Act_c::mtx_init() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    cMtx_copy(mDoMtx_stack_c::get(), m1090);
    cMtx_copy(mDoMtx_stack_c::get(), m10C0);
}

/* 0000234C-000024B0       .text mode_afl__Q213daObjBuoyflag5Act_cFv */
bool daObjBuoyflag::Act_c::mode_afl() {
    current.pos.x = m1090[0][3];
    current.pos.y = m1090[1][3];
    current.pos.z = m1090[2][3];
    mPacket.calc(this);
    fopAcM_setCullSizeSphere(this, 0.0f, 50.0f * scale.y, 0.0f, 90.0f * scale.x);
    if (attr_type().collision) {
        f32 radius = scale.x * M_cyl_src.mCylAttr.mCyl.mRadius;
        f32 height = scale.y * M_cyl_src.mCylAttr.mCyl.mHeight;
        cXyz center;
        center.set(M_cyl_src.mCylAttr.mCyl.mCenter);
        center *= scale;
        center += current.pos;
        mCyl.cM3dGCyl::Set(center, radius, height);
        dComIfG_Ccsp()->Set(&mCyl);
    }
    return true;
}

/* 000024B0-0000278C       .text mode_jumpToSea__Q213daObjBuoyflag5Act_cFv */
bool daObjBuoyflag::Act_c::mode_jumpToSea() {
    if (m1128) {
        gravity = attr().fallGravity;
        mTilt = 0;
        mTiltSpeed = attr().tiltSpeed;
        s16 y = cM_rndFX(32768.0f);
        s16 x = cM_rndFX(4000.0f);
        mDoMtx_stack_c::ZXYrotS(x, y, 0);
        mDoMtx_stack_c::multVecSR(&cXyz::BaseZ, &mTiltAxis);
    }
    f32 sea = daSea_calcWave(current.pos.x, current.pos.z);
    if (current.pos.y < sea - 180.0f * scale.y) return false;
    if (current.pos.y > sea) {
        mTiltSpeed -= mTilt * attr().tiltSpring;
        mTiltSpeed *= 1.0f - attr().airTiltDrag;
        mTilt += (s16)(int)mTiltSpeed;
        daObj::posMoveF_stream(this, NULL, &cXyz::Zero, attr().airXZ, attr().airY);
    } else {
        mTiltSpeed -= mTilt * attr().tiltSpring;
        mTiltSpeed *= 1.0f - attr().seaTiltDrag;
        mTilt += (s16)(int)mTiltSpeed;
        daObj::posMoveF_stream(this, NULL, &cXyz::Zero, attr().seaXZ, attr().seaY);
    }
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::transM(0.0f, 60.0f, 0.0f);
    mDoMtx_quatStack_c::rotAxisRadS(&mTiltAxis, cM_s2rad(mTilt));
    mDoMtx_stack_c::quatM(mDoMtx_quatStack_c::get());
    mDoMtx_stack_c::concat(m10F0);
    mDoMtx_stack_c::transM(0.0f, -60.0f, 0.0f);
    cMtx_copy(mDoMtx_stack_c::get(), m1090);
    mPacket.calc(this);
    return true;
}


cPhs_State daObjBuoyflag::Act_c::_create() {
    fopAcM_SetupActor(this, Act_c);
    mType = prm_get_type();
    cPhs_State phase = dComIfG_resLoad(&mPhase, L_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        scale.setall(attr_type().scale);
        fopAcM_SetMtx(this, m1090);
        fopAcM_setCullSizeSphere(this, 0.0f, 50.0f, 0.0f, 2000.0f);
        mtx_init();
        if (prm_get_noCull()) fopAcM_OffStatus(this, fopAcStts_CULL_e | fopAcStts_NOCULLEXEC_e);
        if (attr_type().collision) {
            mStts.Init(255, 255, this);
            mCyl.Set(M_cyl_src);
            mCyl.SetStts(&mStts);
        }
        mPacket.setUserArea((u32)this);
        mPacket.init(this);
        m1124 = 0;
        m1128 = true;
    }
    return phase;
}
bool daObjBuoyflag::Act_c::_delete() {
    dComIfG_resDeleteDemo(&mPhase, L_arcname);
    return true;
}
bool daObjBuoyflag::Act_c::_execute() {
    typedef bool (Act_c::*ModeProc)();
    static ModeProc mode_proc[] = {&Act_c::mode_afl, &Act_c::mode_jumpToSea};
    if (!(this->*mode_proc[m1124])()) {
        fopAcM_delete(this);
    } else {
        m1128 = false;
        cMtx_copy(m1090, m10C0);
    }
    return true;
}
bool daObjBuoyflag::Act_c::_draw() {
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    mPacket.update(this);
    return true;
}

namespace daObjBuoyflag {
namespace {
/* 0000278C-000027AC       .text Mthd_Create__Q213daObjBuoyflag30@unnamed@d_a_obj_buoyflag_cpp@FPv */
cPhs_State Mthd_Create(void* actor) {
    return static_cast<Act_c*>(actor)->_create();
}

/* 000029FC-00002A2C       .text Mthd_Delete__Q213daObjBuoyflag30@unnamed@d_a_obj_buoyflag_cpp@FPv */
BOOL Mthd_Delete(void* actor) {
    return static_cast<Act_c*>(actor)->_delete();
}

/* 00002A2C-00002B04       .text Mthd_Execute__Q213daObjBuoyflag30@unnamed@d_a_obj_buoyflag_cpp@FPv */
BOOL Mthd_Execute(void* actor) {
    return static_cast<Act_c*>(actor)->_execute();
}

/* 00002B04-00002B54       .text Mthd_Draw__Q213daObjBuoyflag30@unnamed@d_a_obj_buoyflag_cpp@FPv */
BOOL Mthd_Draw(void* actor) {
    return static_cast<Act_c*>(actor)->_draw();
}

/* 00002B54-00002B5C       .text Mthd_IsDelete__Q213daObjBuoyflag30@unnamed@d_a_obj_buoyflag_cpp@FPv */
BOOL Mthd_IsDelete(void*) {
    return TRUE;
}

static actor_method_class Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace
}; // namespace daObjBuoyflag

actor_process_profile_definition g_profile_Obj_Buoyflag = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0009,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Buoyflag_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjBuoyflag::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Buoyflag_e,
    /* Actor SubMtd */ &daObjBuoyflag::Mthd_Table,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLSPHERE_CUSTOM_e,
};
