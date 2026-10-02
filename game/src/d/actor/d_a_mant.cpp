/**
 * d_a_mant.cpp
 * Object - Phantom Ganon's cape/Darknut cape
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include <math.h>
#include "d/actor/d_a_mant.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_particle_name.h"
#include "m_Do/m_Do_audio.h"
#include "SSystem/SComponent/c_lib.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_mtx.h"
#include "d/d_s_play.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"

#include "dolphin/gf/GF.h"
#include "JSystem/J3DGraphBase/J3DShape.h"

#include "assets/l_pg_mantle1TEX__d_a_mant.h"
#include "assets/l_pg_mantle1_palettePAL__d_a_mant.h"
#include "assets/l_tn_boro_b1TEX__d_a_mant.h"
#include "assets/l_tn_boro_b1_palettePAL__d_a_mant.h"
#include "assets/l_tn_boro_k1TEX__d_a_mant.h"
#include "assets/l_tn_boro_k1_palettePAL__d_a_mant.h"
#include "assets/l_tn_boro1TEX__d_a_mant.h"
#include "assets/l_tn_boro1_palettePAL__d_a_mant.h"
#include "assets/l_tn_mantle1TEX__d_a_mant.h"
#include "assets/l_tn_mantle1_palettePAL__d_a_mant.h"

static cXyz* v_pos;
static int v_count;
static int mesh_cc_ct;
static cXyz non_pos(-20000.0f, -200000.0f, -100000.0f);

static f32 l_pos[][3] = {
    {-100.0f, -100.0f, 0.0f},
    {-75.0f, -100.0f, 0.0f},
    {-50.0f, -100.0f, 0.0f},
    {-25.0f, -100.0f, 0.0f},
    {0.0f, -100.0f, 0.0f},
    {25.0f, -100.0f, 0.0f},
    {50.0f, -100.0f, 0.0f},
    {75.0f, -100.0f, 0.0f},
    {100.0f, -100.0f, 0.0f},
    {-100.0f, -75.0f, 0.0f},
    {-75.0f, -75.0f, 0.0f},
    {-50.0f, -75.0f, 0.0f},
    {-25.0f, -75.0f, 0.0f},
    {0.0f, -75.0f, 0.0f},
    {25.0f, -75.0f, 0.0f},
    {50.0f, -75.0f, 0.0f},
    {75.0f, -75.0f, 0.0f},
    {100.0f, -75.0f, 0.0f},
    {-100.0f, -50.0f, 0.0f},
    {-75.0f, -50.0f, 0.0f},
    {-50.0f, -50.0f, 0.0f},
    {-25.0f, -50.0f, 0.0f},
    {0.0f, -50.0f, 0.0f},
    {25.0f, -50.0f, 0.0f},
    {50.0f, -50.0f, 0.0f},
    {75.0f, -50.0f, 0.0f},
    {100.0f, -50.0f, 0.0f},
    {-100.0f, -25.0f, 0.0f},
    {-75.0f, -25.0f, 0.0f},
    {-50.0f, -25.0f, 0.0f},
    {-25.0f, -25.0f, 0.0f},
    {0.0f, -25.0f, 0.0f},
    {25.0f, -25.0f, 0.0f},
    {50.0f, -25.0f, 0.0f},
    {75.0f, -25.0f, 0.0f},
    {100.0f, -25.0f, 0.0f},
    {-100.0f, 0.0f, 0.0f},
    {-75.0f, 0.0f, 0.0f},
    {-50.0f, 0.0f, 0.0f},
    {-25.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f},
    {25.0f, 0.0f, 0.0f},
    {50.0f, 0.0f, 0.0f},
    {75.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 0.0f},
    {-100.0f, 25.0f, 0.0f},
    {-75.0f, 25.0f, 0.0f},
    {-50.0f, 25.0f, 0.0f},
    {-25.0f, 25.0f, 0.0f},
    {0.0f, 25.0f, 0.0f},
    {25.0f, 25.0f, 0.0f},
    {50.0f, 25.0f, 0.0f},
    {75.0f, 25.0f, 0.0f},
    {100.0f, 25.0f, 0.0f},
    {-100.0f, 50.0f, 0.0f},
    {-75.0f, 50.0f, 0.0f},
    {-50.0f, 50.0f, 0.0f},
    {-25.0f, 50.0f, 0.0f},
    {0.0f, 50.0f, 0.0f},
    {25.0f, 50.0f, 0.0f},
    {50.0f, 50.0f, 0.0f},
    {75.0f, 50.0f, 0.0f},
    {100.0f, 50.0f, 0.0f},
    {-100.0f, 75.0f, 0.0f},
    {-75.0f, 75.0f, 0.0f},
    {-50.0f, 75.0f, 0.0f},
    {-25.0f, 75.0f, 0.0f},
    {0.0f, 75.0f, 0.0f},
    {25.0f, 75.0f, 0.0f},
    {50.0f, 75.0f, 0.0f},
    {75.0f, 75.0f, 0.0f},
    {100.0f, 75.0f, 0.0f},
    {-100.0f, 100.0f, 0.0f},
    {-75.0f, 100.0f, 0.0f},
    {-50.0f, 100.0f, 0.0f},
    {-25.0f, 100.0f, 0.0f},
    {0.0f, 100.0f, 0.0f},
    {25.0f, 100.0f, 0.0f},
    {50.0f, 100.0f, 0.0f},
    {75.0f, 100.0f, 0.0f},
    {100.0f, 100.0f, 0.0f},
};

static f32 l_texCoord[][2] = {
    {0.125f, 0.0f},
    {0.125f, 0.125f},
    {0.0f, 0.125f},
    {0.0f, 0.0f},
    {0.125f, 0.25f},
    {0.0f, 0.25f},
    {0.125f, 0.375f},
    {0.0f, 0.375f},
    {0.125f, 0.5f},
    {0.0f, 0.5f},
    {0.125f, 0.625f},
    {0.0f, 0.625f},
    {0.125f, 0.75f},
    {0.0f, 0.75f},
    {0.125f, 0.875f},
    {0.0f, 0.875f},
    {0.125f, 1.0f},
    {-0.0f, 1.0f},
    {0.25f, 0.0f},
    {0.25f, 0.125f},
    {0.25f, 0.25f},
    {0.25f, 0.375f},
    {0.25f, 0.5f},
    {0.25f, 0.625f},
    {0.25f, 0.75f},
    {0.25f, 0.875f},
    {0.25f, 1.0f},
    {0.375f, 0.0f},
    {0.375f, 0.125f},
    {0.375f, 0.25f},
    {0.375f, 0.375f},
    {0.375f, 0.5f},
    {0.375f, 0.625f},
    {0.375f, 0.75f},
    {0.375f, 0.875f},
    {0.375f, 1.0f},
    {0.5f, 0.0f},
    {0.5f, 0.125f},
    {0.5f, 0.25f},
    {0.5f, 0.375f},
    {0.5f, 0.5f},
    {0.5f, 0.625f},
    {0.5f, 0.75f},
    {0.5f, 0.875f},
    {0.5f, 1.0f},
    {0.625f, 0.0f},
    {0.625f, 0.125f},
    {0.625f, 0.25f},
    {0.625f, 0.375f},
    {0.625f, 0.5f},
    {0.625f, 0.625f},
    {0.625f, 0.75f},
    {0.625f, 0.875f},
    {0.625f, 1.0f},
    {0.75f, 0.0f},
    {0.75f, 0.125f},
    {0.75f, 0.25f},
    {0.75f, 0.375f},
    {0.75f, 0.5f},
    {0.75f, 0.625f},
    {0.75f, 0.75f},
    {0.75f, 0.875f},
    {0.75f, 1.0f},
    {0.875f, 0.0f},
    {0.875f, 0.125f},
    {0.875f, 0.25f},
    {0.875f, 0.375f},
    {0.875f, 0.5f},
    {0.875f, 0.625f},
    {0.875f, 0.75f},
    {0.875f, 0.875f},
    {0.875f, 1.0f},
    {1.0f, 0.0f},
    {1.0f, 0.125f},
    {1.0f, 0.25f},
    {1.0f, 0.375f},
    {1.0f, 0.5f},
    {1.0f, 0.625f},
    {1.0f, 0.75f},
    {1.0f, 0.875f},
    {1.0f, 1.0f},
};

#include "assets/l_DL__d_a_mant.h"
#include "assets/l_matDL__d_a_mant.h"
#include "assets/l_matDL2__d_a_mant.h"
#include "assets/l_matDL_LIGHTOFF__d_a_mant.h"

static u8* tex_d[] = {l_tn_mantle1TEX, l_tn_boro1TEX, l_tn_boro_b1TEX, NULL, l_tn_boro_k1TEX, NULL, l_pg_mantle1TEX};
static u8* pal_d[] = {l_tn_mantle1_palettePAL, l_tn_boro1_palettePAL, l_tn_boro_b1_palettePAL, NULL, l_tn_boro_k1_palettePAL, NULL, l_pg_mantle1_palettePAL};

/* 000000EC-000003EC       .text draw__15daMant_packet_cFv */
void daMant_packet_c::draw() {
    static GXVtxDescList l_vtxDescList[] = {
        {GX_VA_POS, GX_INDEX8}, {GX_VA_NRM, GX_INDEX8}, {GX_VA_TEX0, GX_INDEX8}, {GX_VA_NULL, GX_NONE},
    };
    static GXVtxAttrFmtList l_vtxAttrFmtList[] = {
        {GX_VA_POS, GX_POS_XYZ, GX_F32, 0}, {GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0},
        {GX_VA_TEX0, GX_TEX_ST, GX_F32, 0}, {GX_VA_NULL, GX_POS_XYZ, GX_F32, 0},
    };
    u8* texture = tex_d[mTexNo];
    u8* palette = pal_d[mTexNo];
    j3dSys.reinitGX();
#if VERSION > VERSION_JPN
    GXSetNumIndStages(0);
#endif
    dKy_GxFog_tevstr_set(mpTevStr);
    dKy_setLight_mine(mpTevStr);
    GFSetVtxDescv(l_vtxDescList);
    GFSetVtxAttrFmtv(GX_VTXFMT0, l_vtxAttrFmtList);
    GFSetArray(GX_VA_POS, mPosition[mBuffer], sizeof(cXyz));
    GFSetArray(GX_VA_NRM, mNormal[mBuffer], sizeof(cXyz));
    GFSetArray(GX_VA_TEX0, l_texCoord, sizeof(l_texCoord[0]));
    GXTlutObj tlut;
    GXTexObj tex;
    GXInitTlutObj(&tlut, palette, GX_TL_RGB5A3, 256);
    GXInitTexObjCI(&tex, texture, 128, 128, GX_TF_C4, GX_CLAMP, GX_CLAMP, GX_FALSE, GX_TLUT0);
    GXInitTexObjLOD(&tex, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTlut(&tlut, GX_TLUT0);
    GXLoadTexObj(&tex, GX_TEXMAP0);
    GXColor color = {255, 255, 255, 255};
    if (mArg0 == mant_class::Type_PHANTOM_GANON_e) {
        GXColorS10 colorS10 = {255, 255, 255, 255};
        GFSetTevColorS10(GX_TEVREG0, colorS10);
        GFSetTevColor(GX_TEVREG1, color);
        GXCallDisplayList(l_matDL_LIGHTOFF, 0x80);
        GFLoadPosMtxImm(mMtx, GX_PNMTX0);
        Mtx nrm;
        mDoMtx_inverseTranspose(mMtx, nrm);
        GFLoadNrmMtxImm(nrm, GX_PNMTX0);
        GXCallDisplayList(l_DL, 0x1E0);
    } else {
        GXColorS10 colorS10 = {55, 55, 55, 255};
        GFSetTevColorS10(GX_TEVREG0, colorS10);
        GFSetTevColor(GX_TEVREG1, color);
        GXCallDisplayList(l_matDL, 0xA0);
        GFLoadPosMtxImm(mMtx, GX_PNMTX0);
        Mtx nrm;
        mDoMtx_inverseTranspose(mMtx, nrm);
        GFLoadNrmMtxImm(nrm, GX_PNMTX0);
        GXCallDisplayList(l_DL, 0x1E0);
        colorS10.r = 30;
        colorS10.g = 30;
        colorS10.b = 30;
        color.r = 0;
        color.g = 0;
        color.b = 0;
        GFSetTevColorS10(GX_TEVREG0, colorS10);
        GFSetTevColor(GX_TEVREG1, color);
        GXCallDisplayList(l_matDL2, 0x40);
        GFLoadPosMtxImm(mMtx2, GX_PNMTX0);
        mDoMtx_inverseTranspose(mMtx2, nrm);
        GFLoadNrmMtxImm(nrm, GX_PNMTX0);
        GXCallDisplayList(l_DL, 0x1E0);
    }
    mBuffer ^= 1;
#if VERSION > VERSION_JPN
    J3DShape::resetVcdVatCache();
#endif
}

/* 000003EC-000004E8       .text daMant_Draw__FP10mant_class */
static BOOL daMant_Draw(mant_class* i_this) {
    if (i_this->scale.y <= 0.01f) {
        return TRUE;
    }
    g_env_light.settingTevStruct(5, &i_this->current.pos, &i_this->tevStr);
    MtxTrans(0.0f, 0.0f, 0.0f, 0);
    mDoMtx_concat(j3dSys.getViewMtx(), *calc_mtx, i_this->mPacket.getMtx());
    if (i_this->mType != mant_class::Type_PHANTOM_GANON_e) {
        MtxTrans(0.0f, DEMO_SELECT(-3.0f + REG6_F(4), -3.0f), 0.0f, 0);
        mDoMtx_concat(j3dSys.getViewMtx(), *calc_mtx, i_this->mPacket.getMtx2());
    }
    i_this->mPacket.setTevStr(&i_this->tevStr);
    j3dSys.getDrawBuffer(0)->entryImm(&i_this->mPacket, 0);
    return TRUE;
}

/* 000004E8-00000FC0       .text joint_control__FP10mant_classP8mant_j_si */
void joint_control(mant_class* i_this, mant_j_s* joint, int column) {
    static f32 d_p[] = {0.6f, 0.4f, 0.3f, 0.3f, 0.2f, 0.2f, 0.2f, 0.1f};
    static f32 d_p2[] = {5.0f, 2.0f, 1.0f, 0.5f, 0.0f, -0.3f, -0.3f, 0.0f};
    int i;
    f32 x, y, z;
    s16 angleX, angleY;
    cXyz* pos = joint->pos;
    cXyz* velocity = joint->velocity;
    cXyz local, offset, back, force, influence, waveLocal, wave, wind, windForce;
    dBgS_GndChk gnd;
#if VERSION == VERSION_DEMO
    x = joint->pos[0].x;
    y = joint->pos[0].y;
    z = joint->pos[0].z;
    y += 50.0f;
    gnd.m_pos.set(x, y, z);
#else
    y = pos->y;
    z = pos->z;
    y += 50.0f;
    gnd.m_pos.set(pos->x, y, z);
#endif
    f32 ground = 1.5f + dComIfG_Bgsp()->GroundCross(&gnd);
    if (ground - pos->y > 50.0f) {
        ground = pos->y;
    }
    influence.set(0.0f, 0.0f, 0.0f);
    waveLocal.set(0.0f, 0.0f, 0.0f);
    wave.set(0.0f, 0.0f, 0.0f);
    cMtx_YrotS(*calc_mtx, joint->angle.y);
#if VERSION == VERSION_DEMO
    local.x = 0.0f;
    local.y = 0.0f;
    local.z = -5.0f + REG0_F(1);
#else
    local.set(0.0f, 0.0f, -5.0f);
#endif
    MtxPosition(&local, &back);
    windForce.set(0.0f, 0.0f, 0.0f);
    s16 windAngle = i_this->mWindAngle + (s16)((column - 4) * DEMO_SELECT(3000 + REG0_S(3), 3000));
    mDoMtx_YrotS(*calc_mtx, windAngle);
    local.x = 0.0f;
    local.y = 0.0f;
    local.z = i_this->mWindStrength;
    s16 diff = windAngle - joint->angle.y;
    if (diff < 0) {
        diff *= -1;
    }
    f32 facing;
    if ((u16)diff < 0x4000) {
        local.z *= DEMO_SELECT(0.05f + REG14_F(11), 0.05f);
        facing = 0.0f;
    } else {
        facing = 1.0f;
    }
    MtxPosition(&local, &wind);
    local.x = 0.0f;
    local.y = 0.0f;
    local.z = i_this->m1BF8 + cM_ssin(column * 23000);
    local.z *= i_this->scale.y;
    for (i = 0; i < 9; ++i, ++pos, ++velocity) {
        if (i > 0) {
            force = back * d_p[i - 1];
            if (std::fabsf(i_this->mWindStrength) > 0.1f) {
                windForce = wind;
                windForce.y = DEMO_SELECT(i_this->mWindStrength * (0.5f + REG14_F(4)), 0.5f * i_this->mWindStrength) * cM_ssin(i_this->mFrame * 4096 + i * DEMO_SELECT(10000 + REG14_S(0), 10000) + column * 10000);
            }
            if (i_this->mWindInfluence > 0.01f) {
                influence = back * d_p2[i - 1] * i_this->mWindInfluence * facing;
            }
            if (i_this->mType == mant_class::Type_PHANTOM_GANON_e) {
                waveLocal.z = DEMO_SELECT((2.0f + REG14_F(4)), 2.0f) * cM_ssin(i_this->mFrame * DEMO_SELECT(2048 + REG14_S(1), 2048) + i * DEMO_SELECT(10000 + REG14_S(0), 10000) + column * 10000);
                cMtx_YrotS(*calc_mtx, joint->angle.y);
                MtxPosition(&waveLocal, &wave);
            }
            x = wave.x + (influence.x + (windForce.x + (force.x + (velocity->x + (pos->x - pos[-1].x)))));
            z = wave.z + (influence.z + (windForce.z + (force.z + (velocity->z + (pos->z - pos[-1].z)))));
            f32 nextY = windForce.y + (i_this->mGravity + (pos->y + velocity->y));
            if (nextY < ground) {
                nextY = ground;
            }
            y = nextY - pos[-1].y;
            angleX = -cM_atan2s(y, z);
            angleY = (s16) cM_atan2s(x, std::sqrtf(y * y + z * z));
            mDoMtx_XrotS(*calc_mtx, angleX);
            mDoMtx_YrotM(*calc_mtx, angleY);
            MtxPosition(&local, &offset);
            *velocity = *pos;
            pos->x = pos[-1].x + offset.x;
            pos->y = pos[-1].y + offset.y;
            pos->z = pos[-1].z + offset.z;
            velocity->x = i_this->m1BFC * (pos->x - velocity->x);
            velocity->y = i_this->m1BFC * (pos->y - velocity->y);
            velocity->z = i_this->m1BFC * (pos->z - velocity->z);
        }
        *v_pos = *pos;
        ++v_pos;
        ++v_count;
        if (i_this->mType == mant_class::Type_DARKNUT_e && !(i & 1) && !(column & 1) && i != 0 && i != 8 && column != 0 && column != 8) {
            if (i_this->m2834 != 0) {
                i_this->mMeshSph[mesh_cc_ct].SetR(DEMO_SELECT(30.0f + REG14_F(5), 30.0f));
                i_this->mMeshSph[mesh_cc_ct].SetC(*pos);
            } else {
                i_this->mMeshSph[mesh_cc_ct].SetR(-200.0f);
                i_this->mMeshSph[mesh_cc_ct].SetC(non_pos);
            }
            dComIfG_Ccsp()->Set(&i_this->mMeshSph[mesh_cc_ct]);
            #if VERSION == VERSION_DEMO
            if (i_this->m2836 == 0 && i_this->mMeshSph[mesh_cc_ct].ChkTgHit()) {
#else
            if (i_this->m2836 == 0 && i_this->mDeleteTimer == 0 && i_this->mMeshSph[mesh_cc_ct].ChkTgHit()) {
#endif
                i_this->m2836 = 10;
                CcAtInfo info;
                info.mpObj = i_this->mMeshSph[mesh_cc_ct].GetTgHitObj();
                at_power_check(&info);
                fopAc_ac_c* const player = dComIfGp_getPlayer(0);
                s8 torn = 0;
                if (info.mResultingAttackType == 1 || info.mResultingAttackType == 2) {
                    if (info.mResultingAttackType == 2) {
                        if (i_this->mPacket.getTexNo() == 0) {
                            i_this->mPacket.setTexNo(2);
                            i_this->m2834 = 0;
                            torn = 1;
                        }
                    } else if (info.mResultingAttackType == 1) {
                        if (i_this->mPacket.getTexNo() == 0) {
                            i_this->mPacket.setTexNo(4);
                            i_this->m2834 = 0;
                            torn = 1;
                        }
                    }
                }
                if (torn) {
                    csXyz angle = i_this->current.angle;
                    angle.y += 0x8000;
                    dComIfGp_particle_set(dPa_name::ID_IT_SN_TN_MANTOKIRE00, pos, &angle, NULL, 255, NULL, fopAcM_GetRoomNo(i_this), &i_this->tevStr.mColorK0, &i_this->tevStr.mColorK0);
                    mDoAud_seStart(JA_SE_CM_TN_BREAK_MANTLE, &i_this->current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(i_this)));
                }
                i_this->m1C0E = DEMO_SELECT(2 + REG14_S(3), 2);
                i_this->mWindAngle = player->shape_angle.y;
                i_this->mWindStrength = DEMO_SELECT(10.0f + REG14_F(3), 10.0f);
                if (info.mResultingAttackType == 5) {
                    i_this->mDeleteTimer = DEMO_SELECT(24 + REG0_S(0), 26);
                    i_this->m1C0E = DEMO_SELECT(6 + REG14_S(6), 6);
                    return;
                }
            }
            ++mesh_cc_ct;
            if (i == 4 && column == 4) {
                i_this->mWindSph.SetR(DEMO_SELECT(200.0f + REG14_F(6), 200.0f));
                i_this->mWindSph.SetC(*pos);
                dComIfG_Ccsp()->Set(&i_this->mWindSph);
            }
        }
    }
}

/* 0000137C-000015F0       .text mant_v_calc__FP10mant_class */
void mant_v_calc(mant_class* i_this) {
    cXyz local, offset, delta;
    csXyz angle(0, 0, 0);
    delta = i_this->mAnchor[0] - i_this->mAnchor[1];
    angle.y = cM_atan2s(delta.x, delta.z) + DEMO_SELECT(REG13_S(3) + 0x4000, 0x4000);
    mant_j_s* joint = i_this->mJoint;
    local.x = 0.0f;
    for (int i = 0; i < 9; ++i, ++joint) {
        i_this->mJoint[i].pos[0].x = i_this->mAnchor[1].x + delta.x / 8.0f * i;
        i_this->mJoint[i].pos[0].y = i_this->mAnchor[1].y + delta.y / 8.0f * i;
        i_this->mJoint[i].pos[0].z = i_this->mAnchor[1].z + delta.z / 8.0f * i;
        cMtx_YrotS(*calc_mtx, angle.y);
        f32 sin = cM_ssin(cM_rad2s(0.3926991f * i));
        local.y = DEMO_SELECT(sin * (-10.0f + REG0_F(11)), -10.0f * sin);
        local.z = DEMO_SELECT(sin * (-20.0f + REG0_F(12)), -20.0f * sin);
        MtxPosition(&local, &offset);
        i_this->mJoint[i].pos[0] += offset;
        i_this->mJoint[i].angle = angle;
        i_this->mJoint[i].angle.y += (s16)((i - 4) * DEMO_SELECT(3000 + REG0_S(3), 3000));
        joint_control(i_this, joint, i);
    }
}

/* 000015F0-000017EC       .text mant_n_calc__FP10mant_class */
void mant_n_calc(mant_class* i_this) {
    cXyz local, normal, delta;
    local.x = 0.0f;
    local.y = 0.0f;
    local.z = 1.0f;
    mDoMtx_YrotS(*calc_mtx, 20384);
    MtxPosition(&local, &normal);
    cXyz* pos = i_this->mPacket.getPos();
    cXyz* nrm = i_this->mPacket.getNrm();
    for (int i = 0; i < 81; ++i, ++pos, ++nrm) {
        if (i == 8 || i == 17 || i == 26 || i == 35 || i == 44 || i == 53 || i == 62 || i == 71) {
            *nrm = nrm[-1];
        } else if (i >= 72) {
            *nrm = nrm[-9];
        } else {
            delta = pos[10] - pos[0];
            mDoMtx_YrotS(*calc_mtx, cM_atan2s(delta.x, delta.z));
            mDoMtx_XrotM(*calc_mtx, -cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z)));
            MtxPosition(&normal, nrm);
        }
    }
}

/* 000017EC-000019C0       .text mant_move__FP10mant_class */
void mant_move(mant_class* i_this) {
    v_count = 0;
    v_pos = i_this->mPacket.getPos();
    mesh_cc_ct = 0;
    mant_v_calc(i_this);
    mant_n_calc(i_this);
#if VERSION > VERSION_JPN
    DCStoreRangeNoSync(i_this->mPacket.getPos(), 81 * sizeof(cXyz));
#else
    DCFlushRange(i_this->mPacket.getPos(), 81 * sizeof(cXyz));
#endif
    if (i_this->mDeleteTimer != 0) {
        --i_this->mDeleteTimer;
        cXyz* pos = i_this->mPacket.getPos();
        for (int i = 0; i < 81; ++i, ++pos) {
            if ((i & 3) == 0) {
                dComIfGp_particle_setSimple(dPa_name::ID_IT_SN_O_TN_MANTOFIRE00, pos);
            }
        }
        #if VERSION == VERSION_DEMO
        if (i_this->mDeleteTimer == 0) {
#else
        if (i_this->mDeleteTimer <= 2) {
#endif
            fopAcM_delete(i_this);
            return;
        }
    }
    cLib_addCalc2(&i_this->m1BF8, DEMO_SELECT(30.0f + REG0_F(4), 30.0f), 0.1f, 1.0f);
    cLib_addCalc2(&i_this->m1BFC, DEMO_SELECT(0.7f + REG0_F(5), 0.7f), 0.1f, 0.05f);
    if (i_this->mWindSph.ChkTgHit()) {
        i_this->m1C0E = DEMO_SELECT(10 + REG14_S(4), 10);
        i_this->mWindAngle = dComIfGp_getPlayer(0)->shape_angle.y;
    }
    if (i_this->m1C0E != 0) {
        --i_this->m1C0E;
        cLib_addCalc2(&i_this->mWindStrength, DEMO_SELECT(50.0f + REG14_F(1), 50.0f), 0.2f, DEMO_SELECT(1.5f + REG14_F(2), 1.5f));
    } else {
        cLib_addCalc0(&i_this->mWindStrength, 0.1f, DEMO_SELECT(1.0f + REG14_F(3), 1.0f));
    }
    if (i_this->m1C0E > DEMO_SELECT((s16)(4 + REG14_S(8)), 4)) {
        cLib_addCalc2(&i_this->mWindInfluence, 1.0f, 1.0f, DEMO_SELECT(1.0f + REG14_F(8), 1.0f));
    } else {
        cLib_addCalc0(&i_this->mWindInfluence, 0.1f, DEMO_SELECT(0.1f + REG14_F(9), 0.1f));
    }
}

/* 000019C0-00001A3C       .text daMant_Execute__FP10mant_class */
static BOOL daMant_Execute(mant_class* i_this) {
    ++i_this->mFrame;
    if (i_this->m2836 != 0) {
        --i_this->m2836;
    }
    mant_move(i_this);
    MtxTrans(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z, 0);
    PSMTXCopy(*calc_mtx, i_this->mMtx);
#if VERSION == VERSION_DEMO
    if (i_this->mType == mant_class::Type_PHANTOM_GANON_e) {
        i_this->m1BF8 = 40.0f + REG0_F(4);
        i_this->m1BFC = 0.8f + REG0_F(5);
        i_this->mGravity = -5.0f + REG0_F(6);
    }
#endif
    return TRUE;
}

/* 00001A3C-00001A44       .text daMant_IsDelete__FP10mant_class */
static BOOL daMant_IsDelete(mant_class*) {
    return TRUE;
}

/* 00001A44-00001A4C       .text daMant_Delete__FP10mant_class */
static BOOL daMant_Delete(mant_class*) {
    return TRUE;
}

/* 00001A4C-00001D18       .text daMant_Create__FP10fopAc_ac_c */
static cPhs_State daMant_Create(fopAc_ac_c* actor) {
    mant_class* i_this = (mant_class*)actor;
    static dCcD_SrcSph wind_cc_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_WIND,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ 0,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 200.0f,
        }},
    };
    static dCcD_SrcSph mesh_cc_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ 0,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 30.0f,
        }},
    };
    fopAcM_SetupActor(actor, mant_class);
    i_this->mType = fopAcM_GetParam(i_this);
    fopAcM_SetMin(i_this, -2000.0f, -2000.0f, -2000.0f);
    fopAcM_SetMax(i_this, 2000.0f, 2000.0f, 2000.0f);
    fopAcM_SetMtx(i_this, i_this->mMtx);
    i_this->mPacket.setarg0(i_this->mType);
    if (i_this->mType == mant_class::Type_PHANTOM_GANON_e) {
        i_this->m1BF8 = DEMO_SELECT(40.0f + REG0_F(4), 40.0f);
        i_this->m1BFC = DEMO_SELECT(0.8f + REG0_F(5), 0.8f);
        i_this->mGravity = DEMO_SELECT(-5.0f + REG0_F(6), -5.0f);
        i_this->mPacket.setTexNo(6);
    } else {
        i_this->m1BF8 = DEMO_SELECT(30.0f + REG0_F(4), 30.0f);
        i_this->m1BFC = DEMO_SELECT(0.7f + REG0_F(5), 0.7f);
        i_this->mGravity = DEMO_SELECT(-10.0f + REG0_F(6), -10.0f);
        i_this->mStts.Init(200, 255, i_this);
        i_this->mWindSph.Set(wind_cc_sph_src);
        i_this->mWindSph.SetStts(&i_this->mStts);
        for (int i = 0; i < 9; ++i) {
            i_this->mMeshSph[i].Set(mesh_cc_sph_src);
            i_this->mMeshSph[i].SetStts(&i_this->mStts);
        }
        i_this->m2834 = 10;
    }
    i_this->scale.set(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 10; ++i) {
        daMant_Execute(i_this);
    }
    return cPhs_COMPLEATE_e;
}

static actor_method_class l_daMant_Method = {
    (process_method_func)daMant_Create,
    (process_method_func)daMant_Delete,
    (process_method_func)daMant_Execute,
    (process_method_func)daMant_IsDelete,
    (process_method_func)daMant_Draw,
};

actor_process_profile_definition g_profile_MANT = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_MANT_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(mant_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_MANT_e,
    /* Actor SubMtd */ &l_daMant_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
