/**
 * d_a_mt.cpp
 * Enemy - Magtail
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_mt.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_particle_name.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_path.h"
#include "f_op/f_op_kankyo_mng.h"
#include "c/c_damagereaction.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAMath.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Mt.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_hostIO.h"
#include "JSystem/J3DGraphAnimator/J3DNode.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"

static u8 mt_count;
static daMt_HIO_c l_HIO;
static s32 mt_all_count;
static s32 mt_fight_count;
static s32 j_index;

static u16 mt_tex_anm_idx[] = {0x36, 0x37};
static u16 mt_tex_max_frame[] = {6, 1};
static int brk_data[] = {
    dRes_INDEX_MT_BRK_MG_HEAD1_e,
    dRes_INDEX_MT_BRK_MG_BODY1_e,
    dRes_INDEX_MT_BRK_MG_BODY1_e,
    dRes_INDEX_MT_BRK_MG_BODY1_e,
    dRes_INDEX_MT_BRK_MG_BODY1_e,
    dRes_INDEX_MT_BRK_MG_BODY1_e,
    dRes_INDEX_MT_BRK_MG_BODY1_e,
    dRes_INDEX_MT_BRK_MG_TAIL1_e,
};
static int btk_data[] = {
    dRes_INDEX_MT_BTK_MG_HEAD1_e,
    dRes_INDEX_MT_BTK_MG_BODY1_e,
    dRes_INDEX_MT_BTK_MG_BODY1_e,
    dRes_INDEX_MT_BTK_MG_BODY1_e,
    dRes_INDEX_MT_BTK_MG_BODY1_e,
    dRes_INDEX_MT_BTK_MG_BODY1_e,
    dRes_INDEX_MT_BTK_MG_BODY1_e,
    dRes_INDEX_MT_BTK_MG_TAIL1_e,
};
static int move_ad[] = {0, -6, -12, -18, -24, -30, -36, -42};
static int move_ad2[] = {0, -3, -6, -9, -12, -15, -18, -21};
static u8 br_no[] = {0, 1, 1, 2, 2, 2, 1, 0, 0, 0, 0};
static s16 br_ya[] = {-0x32C8, -0x2328, -0x0FA0, 0, 0, 0, 0, 0, 0, 0, 0};

/* 000000EC-000001E8       .text __ct__10daMt_HIO_cFv */
daMt_HIO_c::daMt_HIO_c() {
    m04 = 0;
    m05 = 0;
    m06 = 0;
    m07 = 0;
    m08 = 0x157C;
    m0C = -7500.0f;
    m10 = 15;
    m14 = 0.5f;
    m18 = 21.0f;
    m1C = 0.8f;
    m20 = 0.8f;
    m24 = 450.0f;
    m28 = 350.0f;
    m2C = 30.0f;
    m30 = 17;
    m32 = 40;
    m34 = 400.0f;
    m38 = 45;
    m3C = 43;
    m3E = 47;
    m3A = 70;
    m40 = 55.0f;
    m44 = 30.0f;
    m48 = 25.0f;
    m4C = 0.65f;
    m50 = -5;
    m52 = 0x2AF8;
    m54 = 0x258;
    m58 = 1.0f;
    m5C = 1.0f;
}

/* 00000230-000002D8       .text anm_init__FP8mt_classifUcfi */
void anm_init(mt_class* i_this, int bckIdx, float morf, unsigned char loopMode, float speed, int) {
    i_this->mpMorf[0]->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("Mt", bckIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
}

/* 000002D8-00000350       .text mt_a_d_sub__FPvPv */
void* mt_a_d_sub(void* ac1, void*) {
    if (fopAcM_IsActor(ac1) && fopAcM_GetName(ac1) == fpcNm_MT_e) {
        mt_class* i_this = (mt_class*)ac1;
        mt_all_count++;
        if (i_this->m454 == 1) {
            mt_fight_count++;
        }
    }
    return NULL;
}

/* 00000350-00000390       .text mt_check__FP8mt_class */
void mt_check(mt_class* i_this) {
    mt_fight_count = 0;
    mt_all_count = 0;
    fpcEx_Search(mt_a_d_sub, i_this);
}

/* 00000390-00000478       .text mt_bg_check__FP8mt_class */
void mt_bg_check(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    f32 tmp = 40.0f + REG0_F(3);

    actor->current.pos.y -= tmp;
    actor->old.pos.y -= tmp;
    fopAcM_OnStatus(actor, fopAcStts_FREEZE_e);
    i_this->mAcch.CrrPos(*dComIfG_Bgsp());
    actor->current.pos.y += tmp;
    actor->old.pos.y += tmp;

    if (actor->home.pos.y - actor->current.pos.y > 1000.0f && actor->current.pos.y - i_this->mAcch.GetGroundH() > 5000.0f) {
        i_this->m1CBC = 1;
        fopAcM_delete(actor);
    }
}

/* 00000478-0000053C       .text tex_anm_set__FP8mt_classUs */
void tex_anm_set(mt_class* i_this, unsigned short idx) {
    J3DAnmTexPattern* btp;

    i_this->m345 = 1;
    btp = (J3DAnmTexPattern*)dComIfG_getObjectRes("Mt", mt_tex_anm_idx[idx]);
    i_this->m346 = mt_tex_max_frame[idx];
    i_this->mpBtp = btp;
    i_this->m344 = 0;
    i_this->mpBtp->setFrame(0.0f);

    u16 materialNum = btp->getUpdateMaterialNum();
    for (u16 i = 0; i < materialNum; i++) {
        i_this->mpTexNoAnm[i].setAnmTexPattern(btp);
    }
}

/* 0000053C-000005EC       .text mt_eye_tex_anm__FP8mt_class */
void mt_eye_tex_anm(mt_class* i_this) {
    if (i_this->m464 != 0) {
        i_this->m464--;
    } else {
        i_this->m464 = (s16)(cM_rndF(100.0f) + 50.0f);
        if (i_this->m345 == 0) {
            tex_anm_set(i_this, 0);
        }
    }

    if (i_this->m345 != 0) {
        if (i_this->m344 < i_this->m346) {
            i_this->m344++;
        } else {
            i_this->m345 = 0;
        }
    }
}

/* 000005EC-00000720       .text nodeCallBack_head__FP7J3DNodei */
static BOOL nodeCallBack_head(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        mt_class* i_this = (mt_class*)model->getUserArea();

        if (i_this != NULL) {
            cXyz unused;
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
            if (jntNo == 2) {
                cMtx_YrotM(*calc_mtx, -i_this->mJntRot[0].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[0].z);
            } else if (jntNo == 3) {
                cMtx_YrotM(*calc_mtx, (s16)i_this->mJntRot[15].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[15].z);
            } else {
                MtxScale(i_this->m18F0, i_this->m18F0, i_this->m18F0, 1);
            }
            model->setAnmMtx(jntNo, *calc_mtx);
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 0000075C-0000098C       .text nodeCallBack_body__FP7J3DNodei */
static BOOL nodeCallBack_body(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        mt_class* i_this = (mt_class*)model->getUserArea();

        if (i_this != NULL) {
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
            if (jntNo == 2) {
                cMtx_YrotM(*calc_mtx, -i_this->mJntRot[j_index + 1].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[j_index + 1].z);
            } else if (jntNo == 3) {
                cMtx_YrotM(*calc_mtx, -i_this->mJntRot[j_index + 2].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[j_index + 2].z);
            } else if (jntNo == 4) {
                cMtx_YrotM(*calc_mtx, i_this->mJntRot[j_index + 16].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[j_index + 16].z);
            } else if (jntNo == 5) {
                cMtx_YrotM(*calc_mtx, i_this->mJntRot[j_index + 17].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[j_index + 17].z);
                j_index += 2;
            }
            model->setAnmMtx(jntNo, *calc_mtx);
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 0000098C-00000B28       .text nodeCallBack_tail__FP7J3DNodei */
static BOOL nodeCallBack_tail(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        mt_class* i_this = (mt_class*)model->getUserArea();

        if (i_this != NULL) {
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
            if (jntNo == 2) {
                cMtx_YrotM(*calc_mtx, -i_this->mJntRot[13].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[13].z);
            } else if (jntNo == 3) {
                cMtx_YrotM(*calc_mtx, -i_this->mJntRot[14].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[14].z);
            } else if (jntNo == 4) {
                cMtx_YrotM(*calc_mtx, i_this->mJntRot[28].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[28].z);
            } else if (jntNo == 5) {
                cMtx_YrotM(*calc_mtx, i_this->mJntRot[29].x);
                cMtx_ZrotM(*calc_mtx, -i_this->mJntRot[29].z);
            }
            model->setAnmMtx(jntNo, *calc_mtx);
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 00000B28-0000171C       .text body_control2__FP8mt_class */
void body_control2(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    int i;
    s16 angX;
    s16 angY;
    f32 dy;
    f32 y;
    f32 inertia = i_this->m18F4;

    i_this->m4A0[0] = actor->current.pos;
    i_this->m560[0] = actor->shape_angle;

    cXyz* p4A0 = i_this->m4A0;
    cXyz* p500 = i_this->m500;
    csXyz* p560 = i_this->m560;
    cXyz* p590 = i_this->m590;

    dBgS_LinChk linChk;
    cXyz end[2];
    cXyz offset;
    cXyz step;
    cXyz start;
    cXyz wave;
    wave.x = 0.0f;
    wave.y = 0.0f;
    wave.z = 0.0f;

    for (i = 0; i < 8; i++, p4A0++, p560++, p590++, p500++) {
        if (i > 0) {
            u8 hit = 0;
            start = *p4A0;
            start.y += 50.0f;

            cMtx_YrotS(*calc_mtx, p560->y);
            offset.x = 3.0f;
            offset.y = -200.0f;
            offset.z = 0.0f;
            MtxPosition(&offset, &end[0]);
            end[0] += *p4A0;
            linChk.Set(&start, &end[0], actor);
            if (dComIfG_Bgsp()->LineCross(&linChk)) {
                end[0] = linChk.GetCross();
                hit = 1;
            }

            offset.x *= -1.0f;
            MtxPosition(&offset, &end[1]);
            end[1] += *p4A0;
            linChk.Set(&start, &end[1], actor);
            if (dComIfG_Bgsp()->LineCross(&linChk)) {
                end[1] = linChk.GetCross();
                hit += 1;
            }

            s16 wall_z = 0;
            if (hit == 2) {
                y = p4A0->y - 10.0f;
                f32 ground_y = end[0].y + l_HIO.m18;
                if (y < ground_y) {
                    y = ground_y;
                    offset = end[0] - end[1];
                    f32 dist = std::sqrtf(offset.x * offset.x + offset.z * offset.z);
                    wall_z = (s16)cM_atan2s(offset.y, dist);
                }
            }
            cLib_addCalcAngleS2(&p560->z, wall_z, 2, 0x400);

            dy = p590->y + (y - p4A0[-1].y);

            if (i_this->m48E == 0) {
                offset.x = 3.0f * cM_ssin(i_this->m46A * (REG0_S(5) + 1500) + i * (REG0_S(6) + 7500));
                offset.y = 0.0f;
                offset.z = -5.0f + REG0_F(3);
                cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
                MtxPosition(&offset, &wave);
            }

            f32 dx = wave.x + (p590->x + (p4A0->x - p4A0[-1].x));
            f32 dz = wave.z + (p590->z + (p4A0->z - p4A0[-1].z));

            angY = (s16)cM_atan2s(dx, dz);
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            angX = -cM_atan2s(dy, dist);

            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 35.0f + REG0_F(7);
            cMtx_YrotS(*calc_mtx, angY);
            cMtx_XrotM(*calc_mtx, angX);
            MtxPosition(&offset, &step);

            p560->y = angY + 0x8000;
            p560->x = -angX;

            p500->x = p4A0->x;
            p500->y = p4A0->y;
            p500->z = p4A0->z;
            p4A0->x = p4A0[-1].x + step.x;
            p4A0->y = p4A0[-1].y + step.y;
            p4A0->z = p4A0[-1].z + step.z;

            p590->x = inertia * (p4A0->x - p500->x);
            p590->y = inertia * (p4A0->y - p500->y);
            p590->z = inertia * (p4A0->z - p500->z);
        }

        J3DModel* model = i_this->mpMorf[i]->getModel();
        model->setBaseScale(actor->scale);

        mDoMtx_stack_c::transS(p4A0->x, p4A0->y, p4A0->z);
        mDoMtx_stack_c::YrotM(p560->y);
        mDoMtx_stack_c::XrotM(p560->x);
        mDoMtx_stack_c::ZrotM(p560->z);
        if (i == 0) {
            mDoMtx_stack_c::YrotM(i_this->m468);
        }
        if (i == 0) {
            mDoMtx_stack_c::scaleM(l_HIO.m1C, l_HIO.m1C, l_HIO.m1C);
        } else {
            mDoMtx_stack_c::scaleM(i_this->m600[i], i_this->m600[i] * i_this->m620[i], 1.0f);
            if (i == 7) {
                mDoMtx_stack_c::scaleM(i_this->m18F0, i_this->m18F0, i_this->m18F0);
            }
        }
        mDoMtx_stack_c::transM(0.0f, 0.0f, i_this->m470);
        model->setBaseTRMtx(mDoMtx_stack_c::get());

        if (i == 0) {
#if VERSION == VERSION_DEMO
            offset.set(0.0f, 0.0f, 30.0f + REG0_F(9));
#else
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 30.0f + REG0_F(9);
#endif
            mDoMtx_stack_c::multVec(&offset, &actor->eyePos);
            i_this->mEyeSph.SetC(actor->eyePos);

#if VERSION == VERSION_DEMO
            offset.set(0.0f, 0.0f, 100.0f + REG6_F(9));
#else
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 100.0f + REG6_F(9);
#endif
            mDoMtx_stack_c::multVec(&offset, &step);
            i_this->mSph[0].SetC(step);

            i_this->mSph[0].OffAtVsEnemyBit();
            i_this->mSph[0].OnAtSPrmBit(cCcD_AtSPrm_VsOther_e);

            if (i_this->mC04 == 1) {
                i_this->mSph[0].OffAtSetBit();
                i_this->mSph[0].OffCoSetBit();
                i_this->mSph[0].OffTgSetBit();
                i_this->mEyeSph.SetR(40.0f);
            } else {
                if (i_this->mC04 == 2) {
                    i_this->mSph[0].OnAtSetBit();
                } else {
                    i_this->mSph[0].OffAtSetBit();
                }
                i_this->mSph[0].OnCoSetBit();
                i_this->mSph[0].OnTgSetBit();
                i_this->mSph[0].SetR(l_HIO.m40);
                i_this->mEyeSph.SetR(l_HIO.m44);
            }
            dComIfG_Ccsp()->Set(&i_this->mEyeSph);
        } else {
            i_this->mSph[i].SetC(*p4A0);
            if (i_this->m460 != 0) {
                i_this->mSph[i].SetR(-200.0f);
            } else {
                i_this->mSph[i].SetR(l_HIO.m48);
            }
        }
        dComIfG_Ccsp()->Set(&i_this->mSph[i]);

        if (i_this->mC01 != 0 && i >= 1) {
            int idx;
            if (i_this->mC00 != 0) {
                idx = move_ad2[i] & 0x3F;
            } else {
                idx = move_ad[i] & 0x3F;
            }
            for (int j = 0; j < 6; j++) {
                int slot = (idx + j) & 0x3F;
                f32 dx_i = (f32)j * ((p4A0[-1].x - p4A0->x) / 5.0f);
                f32 dy_i = (f32)j * ((p4A0[-1].y - p4A0->y) / 5.0f);
                f32 dz_i = (f32)j * ((p4A0[-1].z - p4A0->z) / 5.0f);
                i_this->m6F4[slot].x = p4A0->x + dx_i;
                i_this->m6F4[slot].y = p4A0->y + dy_i;
                i_this->m6F4[slot].z = p4A0->z + dz_i;
                i_this->m9F4[slot] = *p560;
            }
        }
    }

    if (i_this->mC01 != 0) {
        i_this->mC01 = 0;
        i_this->m454 = 0;
        i_this->m455 = 0;
        i_this->mBF4 = 0;
        i_this->m458 = 100;
        i_this->m48E = 0;
        anm_init(i_this, 10, 20.0f, 2, 1.0f, 0);
    }

    cLib_addCalc0(&i_this->m18F4, 1.0f, 0.01f);
    cLib_addCalc2(&i_this->m470, 20.0f, 1.0f, 1.0f);

    s16 tmp = (s16)((2000.0f + REG0_F(5)) * cM_ssin(i_this->m46A * (REG0_S(8) + 600)));
    cLib_addCalcAngleS2(&actor->current.angle.z, tmp, 2, 0x400);
}

/* 00001B54-00001E44       .text wall_check_sub__FP8mt_classP4cXyzP4cXyz */
BOOL wall_check_sub(mt_class* i_this, cXyz* i_start, cXyz* i_end) {
    dBgS_LinChk linChk;

    linChk.Set(i_start, i_end, i_this);
    if (dComIfG_Bgsp()->LineCross(&linChk)) {
        return TRUE;
    }
    return FALSE;
}

/* 00001E44-00001F10       .text body_wall_check__FP8mt_class */
void body_wall_check(mt_class* i_this) {
    for (int i = 1; i < 8; i++) {
        cXyz sp14 = i_this->m500[i];
        sp14.y += 50.0f;
        cXyz sp08 = i_this->m4A0[i];
        sp08.y += 50.0f;
        if (wall_check_sub(i_this, &sp14, &sp08)) {
            i_this->m4A0[i].x = i_this->m500[i].x;
            i_this->m4A0[i].z = i_this->m500[i].z;
        }
    }
}

/* 00001F10-000022D8       .text body_control1__FP8mt_class */
void body_control1(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;

    i_this->m6F4[i_this->mBF4] = actor->current.pos;
    i_this->m9F4[i_this->mBF4] = actor->shape_angle;
    i_this->mB74[i_this->mBF4] = i_this->m468;

    for (int i = 0; i < 8; i++) {
        int idx;
        if (i_this->mC00 != 0) {
            idx = (i_this->mBF4 + move_ad2[i]) & 0x3F;
        } else {
            idx = (i_this->mBF4 + move_ad[i]) & 0x3F;
        }

        J3DModel* model = i_this->mpMorf[i]->getModel();
        model->setBaseScale(actor->scale);

        mDoMtx_stack_c::transS(i_this->m6F4[idx].x, i_this->m6F4[idx].y, i_this->m6F4[idx].z);
        mDoMtx_stack_c::YrotM(i_this->m9F4[idx].y);
        mDoMtx_stack_c::XrotM(i_this->m9F4[idx].x);
        mDoMtx_stack_c::ZrotM(i_this->m9F4[idx].z);
        mDoMtx_stack_c::YrotM(i_this->mB74[idx]);

        if (i == 0) {
            mDoMtx_stack_c::scaleM(l_HIO.m1C, l_HIO.m1C, l_HIO.m1C);
        } else {
            mDoMtx_stack_c::scaleM(i_this->m600[i], i_this->m600[i] * i_this->m620[i], 1.0f);
        }
        mDoMtx_stack_c::transM(0.0f, 0.0f, i_this->m470);
        model->setBaseTRMtx(mDoMtx_stack_c::get());

        if (i == 0) {
            cXyz offset;
#if VERSION == VERSION_DEMO
            offset.set(0.0f, 0.0f, 30.0f + REG0_F(9));
#else
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 30.0f + REG0_F(9);
#endif
            mDoMtx_stack_c::multVec(&offset, &actor->eyePos);
            i_this->mEyeSph.SetC(actor->eyePos);
            i_this->mEyeSph.SetR(l_HIO.m44);
            dComIfG_Ccsp()->Set(&i_this->mEyeSph);

#if VERSION == VERSION_DEMO
            offset.set(0.0f, 0.0f, 100.0f + REG6_F(9));
#else
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 100.0f + REG6_F(9);
#endif
            cXyz sphPos;
            mDoMtx_stack_c::multVec(&offset, &sphPos);
            i_this->mSph[0].SetC(sphPos);
            i_this->mSph[0].SetR(50.0f);
        } else {
            i_this->mSph[i].SetC(i_this->m6F4[idx]);
            if (i_this->m460 != 0) {
                i_this->mSph[i].SetR(-200.0f);
            } else {
                i_this->mSph[i].SetR(l_HIO.m48);
            }
        }
        dComIfG_Ccsp()->Set(&i_this->mSph[i]);

        if (i_this->mC01 != 0) {
            i_this->m4A0[i] = i_this->m6F4[idx];
            i_this->m560[i] = i_this->m9F4[idx];
        }
    }

    if (i_this->m48E == 0) {
        i_this->mBF4++;
    }
    i_this->mBF4 &= 0x3F;

    if (i_this->mC01 != 0) {
        i_this->mC01 = 0;
        i_this->m454 = 1;
        i_this->m455 = 0;
        i_this->m456 = l_HIO.m10;
        i_this->m48E = 0;
        anm_init(i_this, 10, 20.0f, 2, 1.0f, 0);
    }

    cLib_addCalc2(&i_this->m470, -10.0f, 1.0f, 1.0f);
}

/* 000022D8-000028BC       .text body_control3__FP8mt_class */
void body_control3(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    int i;
    s16 angX;
    s16 angY;
    s16 extra;
    cXyz offset;
    cXyz step;
    cXyz wave_pos;
    cXyz world_off;

    i_this->m4A0[0] = actor->current.pos;
    i_this->m560[0] = actor->shape_angle;

    cXyz* p500 = i_this->m500;
    cXyz* p4A0 = i_this->m4A0;
    csXyz* p560 = i_this->m560;
    s16 ang_off = 0;

    cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
    cMtx_XrotM(*calc_mtx, actor->shape_angle.x);
    cMtx_XrotM(*calc_mtx, actor->shape_angle.z);

    offset.x = 0.0f;
    offset.y = i_this->m474 * 32.0f;
    offset.z = i_this->m474 * -17.0f;
    MtxPosition(&offset, &world_off);

    extra = 0;
    if (i_this->m18FC != 0) {
        extra = (s16)((i_this->m18FC & 2) * 500);
    }

    for (i = 0; i < 8; i++, p4A0++, p560++, p500++) {
        if (i > 0) {
            ang_off -= (s16)(i_this->m48A + extra);

            offset.x = i_this->m330 * cM_ssin(i_this->m466 * (REG0_S(5) + 5000) + i * (REG0_S(6) + 7000));
            offset.y = 0.0f;
            offset.z = -i_this->m478;
            cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
            cMtx_XrotM(*calc_mtx, actor->shape_angle.x + ang_off);
            cMtx_ZrotM(*calc_mtx, actor->shape_angle.z);
            MtxPosition(&offset, &wave_pos);

            f32 dx = wave_pos.x + (p4A0->x - p4A0[-1].x);
            f32 dy = wave_pos.y + (p4A0->y - p4A0[-1].y);
            f32 dz = wave_pos.z + (p4A0->z - p4A0[-1].z);

            angY = (s16)cM_atan2s(dx, dz);
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            angX = -cM_atan2s(dy, dist);

            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 35.0f + REG0_F(7);
            cMtx_YrotS(*calc_mtx, angY);
            cMtx_XrotM(*calc_mtx, angX);
            MtxPosition(&offset, &step);

            s16 diff = angY - actor->shape_angle.y;
            if (diff < 0) {
                diff = -diff;
            }
            if ((u16)diff < 0x4000) {
                p560->y = angY;
                p560->x = -(0x8000 - angX);
            } else {
                p560->y = angY + 0x8000;
                p560->x = -angX;
            }

            p500->x = p4A0->x;
            p500->y = p4A0->y;
            p500->z = p4A0->z;
            p4A0->x = p4A0[-1].x + step.x;
            p4A0->y = p4A0[-1].y + step.y;
            p4A0->z = p4A0[-1].z + step.z;
        }

        J3DModel* model = i_this->mpMorf[i]->getModel();
        model->setBaseScale(actor->scale);

        mDoMtx_stack_c::transS(p4A0->x + world_off.x, p4A0->y + world_off.y, p4A0->z + world_off.z);
        mDoMtx_stack_c::YrotM(p560->y);
        mDoMtx_stack_c::XrotM(p560->x);
        mDoMtx_stack_c::ZrotM(p560->z);
        if (i == 0) {
            mDoMtx_stack_c::scaleM(l_HIO.m20, l_HIO.m20, l_HIO.m20);
        } else {
            mDoMtx_stack_c::scaleM(i_this->m600[i], i_this->m600[i] * i_this->m620[i], 1.0f);
            if (i == 7) {
                mDoMtx_stack_c::scaleM(i_this->m18F0, i_this->m18F0, i_this->m18F0);
            }
        }
        mDoMtx_stack_c::transM(0.0f, 0.0f, i_this->m470);
        model->setBaseTRMtx(mDoMtx_stack_c::get());

        if (i == 0) {
            actor->eyePos = actor->current.pos;
            i_this->mSph[0].SetC(actor->current.pos);
            i_this->mSph[0].OnAtSetBit();
            i_this->mEyeSph.SetR(-30.0f);
            if (std::fabsf(actor->speedF) > 2.0f) {
                i_this->mSph[0].OnAtSPrmBit(cCcD_AtSPrm_VsEnemy_e | cCcD_AtSPrm_VsOther_e);
            } else {
                i_this->mSph[0].OffAtSPrmBit(cCcD_AtSPrm_VsEnemy_e | cCcD_AtSPrm_VsOther_e);
            }
            if (i_this->m466 < 0x5A) {
                i_this->mSph[0].OnAtVsPlayerBit();
                i_this->mSph[0].SetR(60.0f);
            } else {
                i_this->mSph[0].OffAtVsPlayerBit();
                i_this->mSph[0].SetR(40.0f);
            }
            dComIfG_Ccsp()->Set(&i_this->mSph[0]);
        }
    }

    cLib_addCalc0(&i_this->m18F4, 1.0f, 0.01f);
    cLib_addCalc2(&i_this->m470, 20.0f, 1.0f, 1.0f);
    cLib_addCalcAngleS2(&actor->current.angle.z, 0, 2, 0x400);
}

/* 000028BC-00002AB0       .text body_control4__FP8mt_class */
void body_control4(mt_class* i_this) {
    cXyz* p4A0 = &i_this->m4A0[1];
    csXyz* p560 = &i_this->m560[1];
    cXyz* p590 = &i_this->m590[1];

    J3DModel* model = i_this->mpMorf[0]->getModel();
    mDoMtx_stack_c::scaleS(0.0f, 0.0f, 0.0f);
    model->setBaseTRMtx(mDoMtx_stack_c::get());

    for (int i = 1; i < 8; i++, p4A0++, p560++, p590++) {
        if (i_this->m5F0[i] != 0) {
            i_this->m5F0[i]--;
        } else {
            dComIfGp_particle_setSimple(dPa_name::ID_IT_SN_O_MAGT_HAHEN_B, p4A0);
            p4A0->x += p590->x;
            p4A0->y += p590->y;
            p4A0->z += p590->z;
            p590->y -= 2.5f;
            p560->x += 0x1800;
            p560->y += 0x1000;
            if (p590->y < 0.0f) {
                cLib_addCalc0(&i_this->m600[i], 1.0f, 0.025f);
            }
        }

        J3DModel* model_i = i_this->mpMorf[i]->getModel();
        mDoMtx_stack_c::transS(p4A0->x, p4A0->y, p4A0->z);
        mDoMtx_stack_c::YrotM(p560->y);
        mDoMtx_stack_c::XrotM(p560->x);
        mDoMtx_stack_c::ZrotM(p560->z);
        mDoMtx_stack_c::scaleM(i_this->m600[i], i_this->m600[i], i_this->m600[i]);
        if (i == 7) {
            mDoMtx_stack_c::scaleM(0.0f, 0.0f, 0.0f);
        }
        model_i->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

/* 00002AB0-00003008       .text body_control5__FP8mt_class */
void body_control5(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;

    i_this->m4A0[0] = actor->current.pos;
    i_this->m560[0] = actor->shape_angle;

    int i;
    f32 dy;
    cXyz* p4A0 = i_this->m4A0;
    csXyz* p560 = i_this->m560;
    f32 ground_y = l_HIO.m18 + i_this->mAcch.GetGroundH();

    for (i = 0; i < 8; i++, p4A0++, p560++) {
        cXyz vec;
        cXyz step;
        cXyz wave_pos;

        if (i > 0) {
            vec.x = i_this->m474 * ((50.0f + REG0_F(4)) * cM_ssin(i_this->m46A * (REG0_S(5) + 3500) + i * (REG0_S(6) + 7000)));
            vec.y = i_this->m474 * ((80.0f + REG0_F(5)) * cM_ssin(i_this->m46A * (REG0_S(7) + 4500) + i * (REG0_S(8) + 6000)));
            vec.z = -30.0f + REG0_F(3);
            cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
            MtxPosition(&vec, &wave_pos);

            f32 y = (p4A0->y - 10.0f) + wave_pos.y;
            if (y < ground_y) {
                y = ground_y;
            }

            f32 dx = wave_pos.x + (p4A0->x - p4A0[-1].x);
            dy = y - p4A0[-1].y;
            f32 dz = wave_pos.z + (p4A0->z - p4A0[-1].z);

            int angY = (s16)cM_atan2s(dx, dz);
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            int angX = (s16)-cM_atan2s(dy, dist);

            vec.x = 0.0f;
            vec.y = 0.0f;
            vec.z = 35.0f + REG0_F(7);
            cMtx_YrotS(*calc_mtx, angY);
            cMtx_XrotM(*calc_mtx, angX);
            MtxPosition(&vec, &step);

            p560->y = angY + 0x8000;
            p560->x = -angX;
            p4A0->x = p4A0[-1].x + step.x;
            p4A0->y = p4A0[-1].y + step.y;
            p4A0->z = p4A0[-1].z + step.z;
        }

        J3DModel* model = i_this->mpMorf[i]->getModel();
        model->setBaseScale(actor->scale);

        mDoMtx_stack_c::transS(p4A0->x, p4A0->y, p4A0->z);
        mDoMtx_stack_c::YrotM(p560->y);
        mDoMtx_stack_c::XrotM(p560->x);
        mDoMtx_stack_c::ZrotM(p560->z);
        if (i == 0) {
            mDoMtx_stack_c::YrotM(i_this->m468);
        }
        if (i == 0) {
            mDoMtx_stack_c::scaleM(l_HIO.m1C, l_HIO.m1C, l_HIO.m1C);
        } else {
            mDoMtx_stack_c::scaleM(i_this->m600[i], i_this->m600[i] * i_this->m620[i], 1.0f);
            if (i == 7) {
                mDoMtx_stack_c::scaleM(i_this->m18F0, i_this->m18F0, i_this->m18F0);
            }
        }
        mDoMtx_stack_c::transM(0.0f, 0.0f, i_this->m470);
        model->setBaseTRMtx(mDoMtx_stack_c::get());

        if (i == 0) {
#if VERSION == VERSION_DEMO
            vec.set(0.0f, 0.0f, 30.0f + REG0_F(9));
#else
            vec.x = 0.0f;
            vec.y = 0.0f;
            vec.z = 30.0f + REG0_F(9);
#endif
            mDoMtx_stack_c::multVec(&vec, &actor->eyePos);
            i_this->mEyeSph.SetC(actor->eyePos);
            i_this->mEyeSph.SetR(30.0f);
            dComIfG_Ccsp()->Set(&i_this->mEyeSph);
        } else {
            i_this->mSph[i].OffCoSetBit();
            i_this->mSph[i].SetC(*p4A0);
        }

        i_this->mSph[i].OffAtSetBit();
        i_this->mSph[i].OffCoSetBit();
        dComIfG_Ccsp()->Set(&i_this->mSph[i]);
    }

    cLib_addCalc2(&i_this->m470, 20.0f, 1.0f, 1.0f);
    i_this->m468 = (s16)(i_this->m474 * ((3000.0f + REG0_F(7)) * cM_ssin(i_this->m46A * (REG0_S(0) + 3000))));
}

/* 00003008-00003210       .text br_draw__FP8mt_class */
void br_draw(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (i_this->m18D4 == 0) {
        return;
    }

    MtxTrans(actor->eyePos.x, actor->eyePos.y, actor->eyePos.z, 0);
    cMtx_YrotM(*calc_mtx, actor->shape_angle.y);
    cMtx_XrotM(*calc_mtx, actor->shape_angle.x);
    cMtx_ZrotM(*calc_mtx, actor->shape_angle.z);

    f32 scale = l_HIO.m1C * (2.0f + REG0_F(4));
    MtxPush();
    cMtx_YrotM(*calc_mtx, br_ya[i_this->m18D4 - 1]);
    cMtx_XrotM(*calc_mtx, -0x4000);
    MtxScale(scale, scale, scale, 1);

    J3DModel* model = i_this->br_modelL[br_no[i_this->m18D4 - 1]];
    g_env_light.setLightTevColorType(model, &actor->tevStr);
    model->setBaseTRMtx(*calc_mtx);
    mDoExt_modelUpdateDL(model);
    MtxPull();

    cMtx_ZrotM(*calc_mtx, -0x8000);
    cMtx_YrotM(*calc_mtx, br_ya[i_this->m18D4 - 1]);
    cMtx_XrotM(*calc_mtx, -0x4000);
    MtxScale(scale, scale, scale, 1);

    model = i_this->br_modelR[br_no[i_this->m18D4 - 1]];
    g_env_light.setLightTevColorType(model, &actor->tevStr);
    model->setBaseTRMtx(*calc_mtx);
    mDoExt_modelUpdateDL(model);
}

/* 00003210-00003360       .text daMt_shadowDraw__FP8mt_class */
static void daMt_shadowDraw(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (!fopAcM_checkCarryNow(actor)) {
        cXyz pos(actor->current.pos.x, actor->current.pos.y + 150.0f + REG0_F(18), actor->current.pos.z);
        i_this->mShadowId = dComIfGd_setShadow(
            i_this->mShadowId,
            1,
            i_this->mpMorf[0]->getModel(),
            &pos,
            800.0f + REG0_F(19),
            40.0f + REG0_F(17),
            actor->current.pos.y,
            i_this->mAcch.GetGroundH(),
            i_this->mAcch.m_gnd,
            &actor->tevStr,
            0,
            1.0f,
            &dDlst_shadowControl_c::mSimpleTexObj
        );
        for (int i = 1; i < 8; i++) {
            dComIfGd_addRealShadow(i_this->mShadowId, i_this->mpMorf[i]->getModel());
        }
    } else {
        u32 shadowId = daPy_getPlayerLinkActorClass()->getShadowID();
        if (shadowId != 0) {
            for (int i = 0; i < 8; i++) {
                dComIfGd_addRealShadow(shadowId, i_this->mpMorf[i]->getModel());
            }
        }
    }
}

/* 00003360-0000361C       .text daMt_Draw__FP8mt_class */
static BOOL daMt_Draw(mt_class* i_this) {
    cXyz sp14(0.0f, 0.0f, 0.0f);

    if (i_this->m2BB != 0) {
        return TRUE;
    }

    j_index = 0;
    for (int i = 0; i < 8; i++) {
        J3DModel* model = i_this->mpMorf[i]->getModel();
        if (i_this->mEnemyIce.mLightShrinkTimer == 0) {
            cXyz sp08;
            MTXMultVec(model->getBaseTRMtx(), &sp14, &sp08);
            g_env_light.settingTevStruct(0, &sp08, &i_this->tevStr);
        } else {
            i_this->scale.x = i_this->scale.y = i_this->scale.z = i_this->mEnemyIce.mScaleXZ;
            model->setBaseScale(i_this->scale);
        }

        g_env_light.setLightTevColorType(model, &i_this->tevStr);
        i_this->btk[i]->entry(model->getModelData());
        i_this->brk[i]->entry(model->getModelData());

        if (i_this->m2E4 == 0) {
            int brk_frame = i_this->m2E8 + i * l_HIO.m50;
            while (brk_frame < 0) {
                brk_frame += 41;
            }
            i_this->brk[i]->setFrame(brk_frame);

            int btk_frame = i_this->m2EC + i * l_HIO.m50;
            while (btk_frame < 0) {
                btk_frame += 31;
            }
            i_this->btk[i]->setFrame(btk_frame);
        } else {
#if VERSION == VERSION_DEMO
            int brk_frame = i_this->m2E8;
            i_this->brk[i]->setFrame(brk_frame);
            int btk_frame = i_this->m2EC;
            i_this->btk[i]->setFrame(btk_frame);
#else
            i_this->brk[i]->setFrame(i_this->m2E8);
            i_this->btk[i]->setFrame(i_this->m2EC);
#endif
        }

        if (i == 0) {
            model->getModelData()->setTexNoAnimator(i_this->mpBtp, i_this->mpTexNoAnm);
            i_this->mpBtp->setFrame(i_this->m344);
        }

        i_this->mpMorf[i]->updateDL();
    }

    br_draw(i_this);
    daMt_shadowDraw(i_this);
    fopAc_ac_c* actor = i_this;
    dSnap_RegistFig(DSNAP_TYPE_UNKAF, actor, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 0000361C-000037B0       .text bakuha__FP8mt_class */
void bakuha(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;

    fopAcM_createDisappear(actor, &actor->eyePos, 10, daDisItem_IBALL_e);
    if (i_this->m2B6 == 0) {
        if (i_this->m2B9 != 0) {
            dComIfGs_onSwitch(i_this->m2B9, fopAcM_GetRoomNo(actor));
        }
    }

    i_this->m454 = 3;
    i_this->m45A = REG0_S(0) + 57;

    for (int i = 1; i < 8; i++) {
        i_this->m590[i].x = cM_rndFX(30.0f + REG0_F(4));
        i_this->m590[i].y = REG0_F(5) + (20.0f + cM_rndF(10.0f));
        i_this->m590[i].z = cM_rndFX(30.0f + REG0_F(4));
        i_this->m5F0[i] = (s16)cM_rndF(3.0f);
        if (i_this->m18F8 == 3) {
            i_this->m5F0[i] += 5;
        }
    }

    dComIfGp_particle_set(dPa_name::ID_IT_SN_MAGT_HAHEN_A, &actor->current.pos);
}

/* 000037B0-000042C4       .text mt_move__FP8mt_class */
void mt_move(mt_class* i_this) {
    static f32 check_x[] = {0.0f, 0.0f, 50.0f, -50.0f, -1.0f, 1.0f};
    static f32 check_y[] = {-50.0f, -50.0f, -150.0f, -150.0f, 100.0f, 100.0f};
    static f32 check_z[] = {150.0f, -10.0f, 0.0f, 0.0f, 200.0f, 200.0f};
    static u8 check_bitD[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};

    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#endif
    dBgS_LinChk linChk;
    u16 bits = 0;
    cXyz dest[6];
    cXyz cross[6];
    cXyz offset;
    cXyz trans;
    cXyz start;
    cXyz diff;
    int probe_n;
    s16 target_y;

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    cMtx_ZrotM(*calc_mtx, actor->current.angle.z);
    offset.x = 0.0f;
    offset.y = 50.0f;
    offset.z = 0.0f;
    MtxPosition(&offset, &start);
    start = start + actor->current.pos;

    if (abs(actor->current.angle.x) < 0x1000) {
        probe_n = 6;
    } else {
        probe_n = 4;
    }

    for (int i = 0; i < probe_n; i++) {
        offset.x = check_x[i];
        offset.y = check_y[i];
        offset.z = check_z[i];
        MtxPosition(&offset, &dest[i]);
        dest[i] += actor->current.pos;
        linChk.Set(&start, &dest[i], actor);
        if (dComIfG_Bgsp()->LineCross(&linChk)) {
            cross[i] = linChk.GetCross();
            bits |= check_bitD[i];
        }
    }

    if ((bits & 3) == 3) {
        diff = cross[0] - cross[1];
        offset.x = 0.0f;
        offset.y = l_HIO.m18;
        offset.z = 0.0f;
        MtxPosition(&offset, &trans);
        if (i_this->m48E == 0) {
            cLib_addCalc2(&actor->current.pos.x, trans.x + (cross[1].x + 0.5f * diff.x), 1.0f, 1.0f);
            cLib_addCalc2(&actor->current.pos.y, trans.y + (cross[1].y + 0.5f * diff.y), 1.0f, 1.0f);
            cLib_addCalc2(&actor->current.pos.z, trans.z + (cross[1].z + 0.5f * diff.z), 1.0f, 1.0f);
        }
        actor->current.angle.x = -cM_atan2s(diff.y, std::sqrtf(diff.x * diff.x + diff.z * diff.z));
        if (std::fabsf(diff.x) > 0.1f || std::fabsf(diff.z) > 0.1f) {
            i_this->m48C = cM_atan2s(diff.x, diff.z);
        }
        s16 yaw_diff = i_this->m48C - actor->current.angle.y;
        if (yaw_diff < 0) {
            yaw_diff = -yaw_diff;
        }
        if ((u16)yaw_diff > 0x4000) {
            actor->current.angle.x = 0x8000 - actor->current.angle.x;
        }
        i_this->m492 = 0x17;
    } else if ((bits & 1) == 0) {
        if (i_this->m492 == 0) {
            actor->current.angle.x += (s16)(REG0_S(2) + 0x800);
        } else {
            i_this->m492--;
        }
    }

    if ((bits & 0xC) == 0xC) {
        diff = cross[2] - cross[3];
        cMtx_XrotS(*calc_mtx, -actor->current.angle.x);
        cMtx_YrotM(*calc_mtx, -actor->current.angle.y);
        MtxPosition(&diff, &trans);
        actor->current.angle.z = cM_atan2s(trans.y, std::sqrtf(trans.x * trans.x + trans.z * trans.z));
    }

    if (abs(actor->current.angle.x) < 0x1000) {
        if ((bits & 0x30) == 0x30) {
            diff = cross[4] - cross[5];
            actor->current.angle.y = cM_atan2s(diff.x, diff.z) + 0x4000;
        } else {
            if (i_this->m2B4 >= 10) {
                actor->current.angle.y += i_this->m488;
            } else {
                if ((s8)i_this->m2BC != 0) {
                    diff = i_this->m47C - actor->current.pos;
                    target_y = (s16)cM_atan2s(diff.x, diff.z);
                    i_this->m488 = 0x800;
                    f32 xz = std::sqrtf(diff.x * diff.x + diff.z * diff.z);
                    if (xz < 100.0f) {
                        i_this->m2BD += i_this->m2BE;
                        if (i_this->m2BD >= (s8)i_this->mpPath->m_num) {
                            if (dPath_ChkClose(i_this->mpPath)) {
                                i_this->m2BD = 0;
                            } else {
                                i_this->m2BE = -1;
                                i_this->m2BD = i_this->mpPath->m_num - 2;
                            }
                        } else if (i_this->m2BD < 0) {
                            i_this->m2BE = 1;
                            i_this->m2BD = 1;
                        }
                        dPnt* pnt = &i_this->mpPath->m_points[i_this->m2BD];
                        i_this->m47C.x = pnt->m_position.x;
                        i_this->m47C.y = pnt->m_position.y;
                        i_this->m47C.z = pnt->m_position.z;
                    }
                } else {
                    target_y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
                    cLib_addCalcAngleS2(&i_this->m488, REG0_S(4) + 0x400, 1, 0x10);
                }

                if (i_this->m2B5 == 1) {
                    actor->current.angle.y = target_y;
                } else {
                    cLib_addCalcAngleS2(&actor->current.angle.y, target_y, 0x10, i_this->m488);
                }
            }

            if (i_this->m2B4 < 10) {
                if (i_this->m18F9 == 0) {
                    if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < l_HIO.m24) {
                        u8 steep = 0;
                        for (int i = 0; i < 0x40; i += 8) {
                            if (abs(i_this->m9F4[i].x) > 0x1000) {
                                steep = 1;
                                break;
                            }
                        }
                        if (steep == 0) {
                            i_this->mC01 = 1;
                        }
                    }
                }
            }
        }
    } else if (i_this->m2B4 < 10) {
        i_this->m488 = 0;
    }

    if (i_this->m48E == 0) {
        f32 amp = 3500.0f + 1000.0f * cM_ssin(i_this->m46A * 100);
        cLib_addCalcAngleS2(&i_this->m468, amp * cM_ssin(i_this->m46A * (REG0_S(0) + 2000)), 4, 0x400);
        f32 spd;
        if (i_this->mC00 != 0) {
            spd = 10.0f;
        } else {
            spd = 5.0f;
        }
        cLib_addCalc2(&actor->speedF, spd, 1.0f, spd);
    } else {
        cLib_addCalc0(&actor->speedF, 1.0f, 5.0f);
    }

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    cMtx_YrotM(*calc_mtx, i_this->m468);
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = actor->speedF;
    MtxPosition(&offset, &actor->speed);
    fopAcM_posMove(actor, NULL);

    if (bits == 0) {
        i_this->m46C++;
        if (i_this->m46C >= 10) {
            i_this->m454 = 1;
            i_this->m455 = 0x11;
        }
    } else {
        i_this->m46C = 0;
    }
}

/* 000042C4-00005088       .text mt_fight__FP8mt_class */
void mt_fight(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    daPy_py_c* py = (daPy_py_c*)player;
#else
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#endif
    cXyz offset;
    cXyz pos;
    s16 angs[5];
    s16 wave_ang = 0;

    switch (i_this->m455) {
    case 0:
        mt_check(i_this);
        if (mt_fight_count <= 1) {
            angs[0] = 0;
            angs[1] = 0;
            angs[2] = 0;
            angs[3] = 0;
            angs[4] = 0;
        } else if (mt_fight_count == 2) {
            angs[0] = 0x2000;
            angs[1] = 0x2000;
            angs[2] = 0;
            angs[3] = 0;
            angs[4] = 0;
        } else if (mt_fight_count == 3) {
            angs[0] = 0;
            angs[1] = -0x3000;
            angs[2] = 0x3000;
            angs[3] = 0;
            angs[4] = 0;
        } else if (mt_fight_count == 4) {
            angs[0] = 0x2000;
            angs[1] = -0x2000;
            angs[2] = 0x5000;
            angs[3] = -0x5000;
            angs[4] = 0;
        } else {
            angs[0] = 0;
            angs[1] = 0x3000;
            angs[2] = -0x3000;
            angs[3] = 0x6000;
            angs[4] = -0x6000;
        }

        dAttention_c& attn = dComIfGp_getAttention();
        if (attn.Lockon() && actor == attn.LockonTarget(0)) {
            mt_count = 0;
        } else {
            mt_count++;
        }
        if (mt_count >= 5) {
            mt_count -= 5;
        }

        pos = player->current.pos - actor->current.pos;
        pos.y += 90.0f + REG6_F(5);
        i_this->m496 = cM_atan2s(pos.x, pos.z);
        i_this->m494 = -cM_atan2s(pos.y, std::sqrtf(pos.x * pos.x + pos.z * pos.z));

        offset.x = (30.0f + REG0_F(10)) * cM_ssin(i_this->m46A * 500);
        offset.y = 60.0f + (20.0f + REG0_F(11)) * cM_ssin(i_this->m46A * 1400);
        offset.z = l_HIO.m28 + (30.0f + REG0_F(12)) * cM_ssin(i_this->m46A * 600);
        wave_ang = (s16)((-2000.0f + REG0_F(16)) * cM_ssin(i_this->m46A * 1400));

        cMtx_YrotS(*calc_mtx, player->shape_angle.y + angs[mt_count]);
        MtxPosition(&offset, &pos);
        cLib_addCalc2(&actor->current.pos.x, player->current.pos.x + pos.x, 0.1f, 4.0f + REG0_F(10));
        cLib_addCalc2(&actor->current.pos.z, player->current.pos.z + pos.z, 0.1f, 4.0f + REG0_F(10));

        if ((fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) > 100.0f + l_HIO.m24 || i_this->m18F9 != 0) && i_this->m466 == 0) {
            cLib_addCalc2(&actor->current.pos.y, 40.0f + i_this->mAcch.GetGroundH(), 0.5f, 3.0f);
            i_this->m494 = wave_ang = 0;
            if (std::fabsf(actor->current.pos.y - (40.0f + i_this->mAcch.GetGroundH())) < 2.0f) {
                i_this->mC01 = 1;
            }
            break;
        }

        cLib_addCalc2(&actor->current.pos.y, pos.y + (l_HIO.m18 + i_this->mAcch.GetGroundH()), 0.1f, 5.0f);
        if (i_this->m456 == 0) {
            i_this->m456 = l_HIO.m10;
            if (cM_rndF(1.0f) < l_HIO.m14 && i_this->m466 == 0) {
                i_this->m455 = 1;
                i_this->mC02 = 0;
                cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
                offset.x = 0.0f;
                offset.y = 30.0f;
                offset.z = -30.0f;
                MtxPosition(&offset, &i_this->m47C);
                i_this->m47C += actor->current.pos;
                anm_init(i_this, 8, 5.0f, 2, 1.0f, 0);
                fopAcM_monsSeStart(actor, JA_SE_CV_MG_ATTACK, 0);
            }
        }
        break;
    case 1: {
        f32 max_step;
        f32 step;
        if (DEMO_SELECT(py, player)->getCutType() == daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT) {
            dAttention_c& attn = dComIfGp_getAttention();
            if (attn.Lockon() && actor == attn.LockonTarget(0)) {
                i_this->m454 = 6;
                i_this->m456 = 0;
                anm_init(i_this, 7, 2.0f, 0, 1.0f, 0);
                break;
            }
        }

        i_this->mC02++;
        if (i_this->mC02 >= l_HIO.m30 && i_this->mC02 <= l_HIO.m32) {
            i_this->setBtAttackData(0.0f, 20.0f, 10000.0f, 3);
            i_this->setBtMaxDis(l_HIO.m34);
            i_this->setBtNowFrame(10.0f);
        }
        if (i_this->mC02 == l_HIO.m3C) {
            anm_init(i_this, 9, 2.0f, 0, 1.0f, 0);
        }
        if (i_this->mC02 == l_HIO.m38) {
            cMtx_YrotS(*calc_mtx, i_this->m496);
            cMtx_XrotM(*calc_mtx, i_this->m494);
            offset.x = 0.0f;
            offset.y = -100.0f + REG6_F(6);
            offset.z = 200.0f + REG6_F(7);
            MtxPosition(&offset, &i_this->m47C);
            i_this->m47C += actor->current.pos;
        }
        if (i_this->mC02 >= l_HIO.m38) {
            max_step = l_HIO.m2C;
            step = 0.5f;
            if (i_this->mC02 == l_HIO.m3E) {
                anm_init(i_this, 7, 2.0f, 0, 1.0f, 0);
            }
        } else {
            max_step = 4.0f + REG0_F(11);
            step = 0.1f;
        }
        if (i_this->mC02 == (s16)(l_HIO.m3E + REG0_S(3))) {
            fopAcM_monsSeStart(actor, JA_SE_CM_MAGTAIL_ATTACK, 0);
        }
        if (i_this->mC02 < (s16)(l_HIO.m3E + (REG6_S(7) + 3))) {
            i_this->mC04 = 1;
        }
        if (i_this->mC02 == (s16)(l_HIO.m3E + 2)) {
            i_this->m18D4 = 1;
        }
        if (i_this->mC02 >= (s16)(l_HIO.m3E + 2) && i_this->mC02 <= (s16)(l_HIO.m3E + 15)) {
            i_this->mC04 = 2;
        }
        cLib_addCalc2(&actor->current.pos.x, i_this->m47C.x, step, max_step);
        cLib_addCalc2(&actor->current.pos.y, 20.0f + i_this->m47C.y, step, max_step);
        cLib_addCalc2(&actor->current.pos.z, i_this->m47C.z, step, max_step);
        actor->speed.x = i_this->m47C.x - actor->current.pos.x;
        actor->speed.y = -1.0f;
        actor->speed.z = i_this->m47C.z - actor->current.pos.z;
        if (DEMO_SELECT(py, player)->checkPlayerGuard() && i_this->mSph[0].ChkAtHit()) {
            i_this->m455 = 15;
            cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
            offset.x = 0.0f;
            offset.y = 80.0f;
            offset.z = -120.0f;
            MtxPosition(&offset, &i_this->m47C);
            i_this->m47C += actor->current.pos;
            i_this->m462 = 10;
            i_this->m456 = 10;
            anm_init(i_this, 10, 2.0f, 2, 1.0f, 0);
            i_this->m18D4 = 0;
        }
        if (i_this->mC02 == l_HIO.m3A) {
            i_this->m455 = 0;
            i_this->m456 = l_HIO.m10;
            anm_init(i_this, 10, 5.0f, 2, 1.0f, 0);
        }
        break;
    }
    case 10:
        actor->current.pos.y += actor->speed.y;
        actor->speed.y += actor->gravity;
        if (actor->speed.y < -100.0f) {
            actor->speed.y = -100.0f;
        }
        if (i_this->mAcch.ChkGroundHit()) {
            i_this->m455 = 0;
        }
        break;
    case 15:
        cLib_addCalc2(&actor->current.pos.x, i_this->m47C.x, 0.5f, 20.0f);
        cLib_addCalc2(&actor->current.pos.y, 20.0f + i_this->m47C.y, 0.5f, 20.0f);
        cLib_addCalc2(&actor->current.pos.z, i_this->m47C.z, 0.5f, 20.0f);
        actor->speed.x = i_this->m47C.x - actor->current.pos.x;
        actor->speed.y = -1.0f;
        actor->speed.z = i_this->m47C.z - actor->current.pos.z;
        if (i_this->m456 == 0) {
            i_this->m455 = 0;
            i_this->m456 = l_HIO.m10;
            anm_init(i_this, 10, 5.0f, 2, 1.0f, 0);
        }
        break;
    case 17:
        if (i_this->mAcch.ChkGroundHit()) {
            i_this->m455 = 23;
            i_this->m456 = (s16)(cM_rndF(50.0f) + 50.0f);
            i_this->m48E = i_this->m456;
        }
        goto fight_roll;
    case 20: {
        s16 ang_x = actor->current.angle.x - 0x500;
        actor->current.angle.x = ang_x;
        s16 target_y = cM_atan2s(actor->speed.x, actor->speed.z) + 0x8000;
        cLib_addCalcAngleS2(&actor->current.angle.y, target_y, 2, 0x400);
        cLib_addCalc2(&i_this->m18F4, 0.9f, 1.0f, 0.1f);
        if (actor->speed.y <= 1.0f) {
            if (i_this->m18F8 != 0) {
                if (i_this->mAcch.ChkGroundHit()) {
                    i_this->m18F8 = 2;
                }
            } else {
                i_this->mSph[0].OffTgShield();
                tex_anm_set(i_this, 1);
                i_this->m454 = 2;
                i_this->m466 = l_HIO.m54;
                i_this->m478 = 0.0f;
                i_this->m48A = 0;
                i_this->m330 = 0.0f;
                actor->speedF = -std::sqrtf(actor->speed.x * actor->speed.x + actor->speed.y * actor->speed.y + actor->speed.z * actor->speed.z);
                i_this->m455 = 0;
                fopAcM_SetMin(actor, -100.0f, -100.0f, -100.0f);
                fopAcM_SetMax(actor, 100.0f, 100.0f, 100.0f);
            }
        }
        goto fight_roll;
    }
    case 23:
        actor->speed.y = 0.0f;
        cLib_addCalcAngleS2(&actor->current.angle.x, 0, 1, 0x400);
        actor->speed.x *= 0.2f;
        actor->speed.z *= 0.2f;
        if (i_this->m456 == 0) {
            i_this->m455 = 10;
        }
    fight_roll:
        i_this->m460 = 5;
        actor->current.pos.x += actor->speed.x;
        actor->current.pos.y += actor->speed.y;
        actor->current.pos.z += actor->speed.z;
        actor->speed.y += actor->gravity;
        if (actor->speed.y < -100.0f) {
            actor->speed.y = -100.0f;
        }
        break;
    }

    if (i_this->m454 < 2) {
        if (i_this->m455 < 10) {
            cLib_addCalcAngleS2(&actor->current.angle.y, i_this->m496, 2, 0x400);
            pos.y = actor->current.pos.y - i_this->mAcch.GetGroundH();
            if (pos.y > 250.0f) {
                i_this->m455 = 10;
            }
            cLib_addCalcAngleS2(&actor->current.angle.x, i_this->m494 + wave_ang, 4, 0x800);
        }
        cLib_addCalcAngleS2(&i_this->m468, 0, 1, 0x100);
    }
}

/* 00005088-00005A04       .text mt_move_maru__FP8mt_class */
void mt_move_maru(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    u32 se_vol = 0;
    cXyz pos;
    cXyz offset;
    f32 old_speed_y;
    f32 step;
    f32 max_step;

    i_this->m464 = 3;
    switch (i_this->m455) {
    case 0:
        if (i_this->m456 == 0) {
            i_this->mSph[0].OnCoSetBit();
        }
#if VERSION == VERSION_DEMO
        s16 roll = 200.0f * actor->speedF;
        actor->shape_angle.x += roll;
#else
        actor->shape_angle.x += (s16)(200.0f * actor->speedF);
#endif
        cMtx_YrotS(*calc_mtx, actor->current.angle.y);
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = actor->speedF;
        MtxPosition(&offset, &pos);
        actor->speed.x = pos.x;
        actor->speed.z = pos.z;
        actor->current.pos.x += actor->speed.x;
        actor->current.pos.y += actor->speed.y;
        actor->current.pos.z += actor->speed.z;
        actor->speed.y += actor->gravity;
        if (actor->speed.y < -100.0f) {
            actor->speed.y = -100.0f;
        }

        {
            old_speed_y = actor->speed.y;
            mt_bg_check(i_this);
            if (i_this->mAcch.ChkGroundHit()) {
                if (old_speed_y < -50.0f + REG0_F(12)) {
                    i_this->m18F8 = 2;
                }
                step = 0.0f;
                max_step = 1.0f;
                dBgS_GndChk gndChk;
                cXyz ahead_ofs;
                cXyz gnd_pos;
                gnd_pos = actor->current.pos;
                gnd_pos.y += 50.0f;
                gndChk.SetPos(&gnd_pos);
                gnd_pos.y = dComIfG_Bgsp()->GroundCross(&gndChk);
                cMtx_YrotS(*calc_mtx, actor->current.angle.y);
                offset.x = 0.0f;
                offset.y = 50.0f;
                offset.z = 5.0f;
                MtxPosition(&offset, &ahead_ofs);
                cXyz ahead = actor->current.pos + ahead_ofs;
                gndChk.SetPos(&ahead);
                ahead.y = dComIfG_Bgsp()->GroundCross(&gndChk);
                if (ahead.y != -G_CM3D_F_INF) {
                    if (ahead.y < gnd_pos.y - 1.0f) {
                        step = 5.0f;
                        max_step = 0.3f;
                    } else if (ahead.y > gnd_pos.y + 1.0f) {
                        step = -5.0f;
                        max_step = 0.3f;
                    }
                }
                cLib_addCalc2(&actor->speedF, step, 1.0f, l_HIO.m5C * max_step);
                if (old_speed_y < -15.0f + REG0_F(14)) {
                    actor->speed.y = old_speed_y * (-0.4f + REG0_F(15));
                    se_vol = (u32)(actor->speed.y * (6.0f + REG0_F(5)));
                    if (se_vol > 100) {
                        se_vol = 100;
                    }
                } else {
                    actor->speed.y = -5.0f;
                }
            }
        }

        if (i_this->mAcch.ChkWallHit() && std::fabsf(actor->speedF) > 3.0f) {
            actor->speedF *= -0.5f;
            se_vol = 50;
        }
        if (se_vol != 0) {
            fopAcM_seStart(actor, JA_SE_CM_MAGBALL_BOUND, se_vol);
        }

        cLib_onBit<u32>(actor->attention_info.flags, fopAc_Attn_ACTION_CARRY_e);
        actor->attention_info.distances[fopAc_Attn_TYPE_CARRY_e] = 9;
        if (fopAcM_CheckStatus(actor, fopAcStts_CARRY_e)) {
            cLib_offBit<u32>(actor->attention_info.flags, fopAc_Attn_ACTION_CARRY_e);
            i_this->m455 = 1;
        }
        break;
    case 1:
        i_this->mSph[0].OffCoSetBit();
        actor->current.angle.x = 0;
        actor->current.angle.y = actor->shape_angle.y;
        actor->current.angle.z = actor->shape_angle.z;
        if (!fopAcM_CheckStatus(actor, fopAcStts_CARRY_e)) {
            if (actor->speedF > 0.0f) {
                i_this->m455 = 0;
                actor->speedF = 20.0f * l_HIO.m58;
                actor->speed.y = 20.0f * l_HIO.m58;
                i_this->m456 = 20;
            } else {
                i_this->m455 = 0;
                actor->speedF = 0.0f;
                actor->speed.y = -15.0f + REG0_F(11);
                i_this->m456 = 20;
            }
        } else if (dComIfGp_event_runCheck()) {
            i_this->m466++;
        }
        break;
    }

    cLib_addCalcAngleS2(&i_this->m468, 0, 1, 0x100);
    if (i_this->m466 > 0x46) {
        cLib_addCalc2(&i_this->m474, 1.0f, 1.0f, 0.05f);
        s16 target = l_HIO.m52;
        if (i_this->m466 < 0x6E) {
            if (i_this->m466 > 0x64 || i_this->m466 < 0x50) {
                target -= 0x5DC;
            }
            s16 ang = 0x7800;
            cLib_addCalcAngleS2(&actor->shape_angle.x, ang, 4, 0x300);
        }
        cLib_addCalcAngleS2(&i_this->m48A, target, 4, 0x1000);
        cLib_addCalc2(&i_this->m478, 1000.0f, 1.0f, 5.0f);
        cLib_addCalc2(&i_this->m338, 0.3f, 1.0f, 0.01f);
    } else {
        if (i_this->m466 == 0x46) {
            i_this->m330 = 2500.0f + REG0_F(3);
            i_this->m338 = 0.5f;
            if (i_this->m18FA == 0) {
                actor->speed.y = 30.0f + REG0_F(11);
            }
        }
        cLib_addCalc0(&i_this->m330, 1.0f, 125.0f);
        cLib_addCalcAngleS2(&i_this->m468, 5.0f * (i_this->m330 * cM_ssin(i_this->m466 * (REG0_S(5) + 5000))), 2, 0x1000);
        cLib_addCalcAngleS2(&actor->shape_angle.x, 0, 4, 0x1000);
        cLib_addCalc2(&i_this->m474, -0.4f, 1.0f, 0.2f);
        cLib_addCalcAngleS2(&i_this->m48A, (4.0f + REG0_F(14)) * (i_this->m330 * cM_ssin(i_this->m466 * (REG0_S(5) + 5000))), 1, 0x1000);
        actor->current.angle.x = actor->shape_angle.x;
        actor->shape_angle.y = actor->current.angle.y + i_this->m468;
        cLib_addCalc0(&actor->speedF, 1.0f, 0.5f);
    }

    if (i_this->m466 == 0x64) {
        i_this->m2E4 = 2;
    }

    if (i_this->m348 != 0) {
        if (i_this->m348 == 1) {
            i_this->mp450 = dComIfGp_particle_setToon(
                dPa_name::ID_AK_SN_MAGTAILSTEAM, &actor->current.pos, &actor->current.angle, NULL, 0xB4, NULL,
                (int)actor->current.roomNo);
            if (i_this->mp450 != NULL) {
                i_this->mp450->becomeImmortalEmitter();
            }
            i_this->m348 = 2;
            i_this->m34A = 80;
        }
        if (i_this->mp450 != NULL) {
            MtxTrans(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z, 0);
            cMtx_YrotM(*calc_mtx, actor->shape_angle.y);
            i_this->mp450->setGlobalRTMatrix(*calc_mtx);
            if (i_this->m34A == 0) {
                i_this->mp450->quitImmortalEmitter();
                i_this->mp450->becomeInvalidEmitter();
                i_this->mp450 = NULL;
                i_this->m348 = 0;
            } else {
                u8 alpha = 0xB4;
                if (i_this->m34A < 30) {
                    alpha = (u8)(i_this->m34A * 6);
                }
                i_this->mp450->setGlobalAlpha(alpha);
            }
        }
    }
}

/* 00005B9C-00005C54       .text water_damage_se_set__FP8mt_class */
void water_damage_se_set(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAcM_seStart(actor, JA_SE_CM_MAGTAIL_WATER, 0);
    fopAcM_monsSeStart(actor, JA_SE_CV_MG_WATER, 0);
    i_this->m348 = 1;
}

/* 00005C54-0000614C       .text damage_check__FP8mt_class */
#if VERSION == VERSION_DEMO
void damage_check(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    cXyz offset;
    CcAtInfo atInfo;
    atInfo.pParticlePos = NULL;
    u8 hit_kind = 0;
    int start = 0;

    i_this->mStts.Move();
    if (i_this->mC04 == 1) {
        start = 2;
    }

    for (int i = start; i < 8; i++) {
        if (i_this->mSph[i].ChkTgHit() && i_this->m460 == 0) {
            atInfo.mpObj = i_this->mSph[i].GetTgHitObj();
            if (atInfo.mpObj->ChkAtType(AT_TYPE_LIGHT_ARROW)) {
                i_this->m1CBC = 1;
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                i_this->mEnemyIce.mYOffset = -20.0f + REG0_F(0);
                return;
            }

            at_power_check(&atInfo);
            if (atInfo.mResultingAttackType == 4 || atInfo.mpObj->ChkAtType(AT_TYPE_ICE_ARROW)) {
                hit_kind = 2;
                actor->health = 0;
                i_this->m460 = 5;
                water_damage_se_set(i_this);
                break;
            }

            if (i_this->m454 == 2 && (atInfo.mResultingAttackType == 6 || atInfo.mResultingAttackType == 2)) {
                if (atInfo.mResultingAttackType == 2) {
                    i_this->m18F8 = 3;
                } else {
                    i_this->m18F8 = 2;
                }
                i_this->m460 = 5;
                return;
            }

            if (atInfo.mResultingAttackType == 2) {
                hit_kind = 1;
                i_this->m460 = 5;
                break;
            }

            if (atInfo.mResultingAttackType != 6 && i_this->m454 == 1) {
                i_this->m455 = 15;
                cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
                offset.x = 0.0f;
                offset.y = 60.0f;
                offset.z = -120.0f;
                MtxPosition(&offset, &i_this->m47C);
                i_this->m47C += actor->current.pos;
                i_this->m462 = REG6_S(7) + 6;
                i_this->m456 = REG6_S(8) + 6;
                i_this->m460 = 5;
                def_se_set(actor, atInfo.mpObj, 0x40);
                return;
            }

            if (i_this->m454 == 2) {
                i_this->m460 = 5;
                actor->health = 20;
                cc_at_check(actor, &atInfo);
                actor->health = 2;
                i_this->m18FB -= atInfo.mDamage;
                f32 scale_v = 1.0f;
                if ((s8)i_this->m18FB <= 0) {
                    i_this->m18F8 = 2;
                    dScnPly_ply_c::setPauseTimer(REG0_S(7) + 6);
                    dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &actor->current.pos);
                    fopAcM_seStart(actor, JA_SE_LK_LAST_HIT, 0);
                    scale_v = 2.0f;
                } else {
                    dScnPly_ply_c::setPauseTimer(REG0_S(5) + 1);
                }
                cXyz scale;
                scale.x = scale.y = scale.z = scale_v;
                csXyz angle;
                angle.x = angle.z = 0;
                angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
                dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &actor->current.pos, &angle, &scale);
                dKy_SordFlush_set(actor->current.pos, 1);
                i_this->m18FC = 12;
                dComIfGp_particle_set(dPa_name::ID_IT_SN_MAGT_HAHEN_C, &actor->current.pos);
            }
            break;
        }
    }

    if (i_this->m460 == 0) {
        if (i_this->mEyeSph.ChkTgHit()) {
            i_this->m460 = 5;
            atInfo.mpObj = i_this->mEyeSph.GetTgHitObj();
            atInfo.pParticlePos = i_this->mEyeSph.GetTgHitPosP();
            if (atInfo.mpObj->ChkAtType(AT_TYPE_LIGHT_ARROW)) {
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                return;
            }

            at_power_check(&atInfo);
            if (atInfo.mResultingAttackType != 6 && atInfo.mResultingAttackType != 3 &&
                atInfo.mResultingAttackType != 4 && atInfo.mResultingAttackType != 2 && i_this->mC04 != 1)
            {
                return;
            }

            atInfo.mpActor = cc_at_check(actor, &atInfo);
            if (l_HIO.m06 != 0) {
                actor->health = 10;
                i_this->m18FB = 8;
            }
            if (atInfo.mResultingAttackType == 6) {
                i_this->m18F8 = 1;
                actor->health = 0;
            } else if (atInfo.mResultingAttackType == 4 || atInfo.mpObj->ChkAtType(AT_TYPE_ICE_ARROW)) {
                hit_kind = 2;
                actor->health = 0;
                water_damage_se_set(i_this);
            }

            if ((s8)actor->health <= 0) {
                hit_kind = 1;
            } else {
                i_this->m455 = 15;
                cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
                offset.x = 0.0f;
                offset.y = 60.0f;
                offset.z = -120.0f;
                MtxPosition(&offset, &i_this->m47C);
                i_this->m47C += actor->current.pos;
                i_this->m462 = 25;
                i_this->m456 = 10;
                actor->current.angle.x = -0x4000;
            }

            fopAcM_monsSeStart(actor, JA_SE_CV_MG_DAMAGE, 0);
            anm_init(i_this, 10, 2.0f, 2, 1.0f, 0);
            i_this->m18D4 = 0;
        }
    }

    if (hit_kind != 0) {
        cXyz speed;
        cMtx_YrotS(*calc_mtx, atInfo.m0C.y);
        speed.x = 0.0f;
        speed.y = 40.0f * l_HIO.m4C;
        speed.z = -20.0f * l_HIO.m4C;
        MtxPosition(&speed, &actor->speed);
        if (hit_kind == 2) {
            actor->speed.y = 0.0f;
        }
        i_this->m454 = 1;
        i_this->m455 = 0x14;
        i_this->m2E4 = 1;
    }
}
#else
void damage_check(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    cXyz offset;
    CcAtInfo atInfo;
    atInfo.pParticlePos = NULL;
    u8 hit_kind = 0;
    int start = 0;

    i_this->mStts.Move();
    if (i_this->mC04 == 1) {
        start = 2;
    }

    for (int i = start; i < 8; i++) {
        if (i_this->mSph[i].ChkTgHit() && i_this->m460 == 0) {
            atInfo.mpObj = i_this->mSph[i].GetTgHitObj();
            if (atInfo.mpObj->ChkAtType(AT_TYPE_LIGHT_ARROW)) {
                i_this->m1CBC = 1;
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                i_this->mEnemyIce.mYOffset = -20.0f + REG0_F(0);
                actor->health = 0;
                return;
            }

            at_power_check(&atInfo);
            if (atInfo.mResultingAttackType == 4 || atInfo.mpObj->ChkAtType(AT_TYPE_ICE_ARROW)) {
                hit_kind = 2;
                i_this->m18FB = 0;
                i_this->m460 = 5;
                water_damage_se_set(i_this);
                break;
            }

            if (i_this->m454 == 2 && (atInfo.mResultingAttackType == 6 || atInfo.mResultingAttackType == 2)) {
                if (atInfo.mResultingAttackType == 2) {
                    i_this->m18F8 = 3;
                } else {
                    i_this->m18F8 = 2;
                }
                i_this->m460 = 5;
                return;
            }

            if (atInfo.mResultingAttackType == 2) {
                hit_kind = 1;
                i_this->m460 = 5;
                break;
            }

            if (atInfo.mResultingAttackType != 6 && i_this->m454 == 1) {
                i_this->m455 = 15;
                cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
                offset.x = 0.0f;
                offset.y = 60.0f;
                offset.z = -120.0f;
                MtxPosition(&offset, &i_this->m47C);
                i_this->m47C += actor->current.pos;
                i_this->m462 = REG6_S(7) + 6;
                i_this->m456 = REG6_S(8) + 6;
                i_this->m460 = 5;
                def_se_set(actor, atInfo.mpObj, 0x40);
                return;
            }

            if (i_this->m454 == 2) {
                i_this->m460 = 5;
                cc_at_check(actor, &atInfo);
                if (actor->health <= 0) {
                    i_this->m18F8 = 2;
                }
                i_this->m18FC = 12;
                dComIfGp_particle_set(dPa_name::ID_IT_SN_MAGT_HAHEN_C, &actor->current.pos);
            }
            break;
        }
    }

    if (i_this->m460 == 0) {
        if (i_this->mEyeSph.ChkTgHit()) {
            i_this->m460 = 5;
            atInfo.mpObj = i_this->mEyeSph.GetTgHitObj();
            atInfo.pParticlePos = i_this->mEyeSph.GetTgHitPosP();
            if (atInfo.mpObj->ChkAtType(AT_TYPE_LIGHT_ARROW)) {
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                return;
            }

            at_power_check(&atInfo);
            i_this->m18FB -= atInfo.mDamage;
            if (atInfo.mResultingAttackType != 6 && atInfo.mResultingAttackType != 3 &&
                atInfo.mResultingAttackType != 4 && atInfo.mResultingAttackType != 2 && i_this->mC04 != 1)
            {
                return;
            }

            atInfo.mpActor = cc_at_check(actor, &atInfo);
            if (atInfo.mResultingAttackType == 6) {
                i_this->m18F8 = 1;
                i_this->m18FB = 0;
            } else if (atInfo.mResultingAttackType == 4 || atInfo.mpObj->ChkAtType(AT_TYPE_ICE_ARROW)) {
                hit_kind = 2;
                i_this->m18FB = 0;
                water_damage_se_set(i_this);
            }

            if ((s8)i_this->m18FB <= 0) {
                hit_kind = 1;
            } else {
                i_this->m455 = 15;
                cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
                offset.x = 0.0f;
                offset.y = 60.0f;
                offset.z = -120.0f;
                MtxPosition(&offset, &i_this->m47C);
                i_this->m47C += actor->current.pos;
                i_this->m462 = 25;
                i_this->m456 = 10;
                actor->current.angle.x = -0x4000;
            }

            fopAcM_monsSeStart(actor, JA_SE_CV_MG_DAMAGE, 0);
            anm_init(i_this, 10, 2.0f, 2, 1.0f, 0);
            i_this->m18D4 = 0;
        }
    }

    if (hit_kind != 0) {
        cXyz speed;
        cMtx_YrotS(*calc_mtx, atInfo.m0C.y);
        speed.x = 0.0f;
        speed.y = 40.0f * l_HIO.m4C;
        speed.z = -20.0f * l_HIO.m4C;
        MtxPosition(&speed, &actor->speed);
        if (hit_kind == 2) {
            actor->speed.y = 0.0f;
        }
        i_this->m454 = 1;
        i_this->m455 = 0x14;
        i_this->m2E4 = 1;
    }
}
#endif

/* 00006188-000074D4       .text daMt_Execute__FP8mt_class */
static BOOL daMt_Execute(mt_class* i_this) {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);

    if (enemy_ice(&i_this->mEnemyIce)) {
        return TRUE;
    }

    if (i_this->m2BB != 0) {
        if (dComIfGs_isSwitch(i_this->m2BB - 1, fopAcM_GetRoomNo(i_this))) {
            i_this->m2BB = 0;
        } else {
            return TRUE;
        }
    }

    fopAcM_OnStatus(i_this, fopAcStts_SHOWMAP_e);

    dBgS_ObjGndChk_Yogan gndChk;
    {
        f32 pos_x = i_this->current.pos.x;
        f32 pos_y = i_this->current.pos.y;
        f32 pos_z = i_this->current.pos.z;
        pos_y += 200.0f;
        cXyz* lava_pos = gndChk.GetPointP();
        lava_pos->x = pos_x;
        lava_pos->y = pos_y;
        lava_pos->z = pos_z;
        f32 lava_y = dComIfG_Bgsp()->GroundCross(&gndChk);
#if VERSION == VERSION_DEMO
        if (lava_y != -G_CM3D_F_INF) {
            if (i_this->current.pos.y - 30.0f + REG0_F(13) < lava_y) {
                if (i_this->m18FA == 0) {
                    i_this->speedF *= 0.1f;
                    i_this->speed.y = 0.0f;
                    fopKyM_createMpillar(&cXyz(pos_x, lava_y, pos_z), 0.5f);
                }
                i_this->m18FA = 1;
                i_this->gravity = -0.5f;
                if (i_this->speed.y < -5.0f) {
                    i_this->speed.y = -5.0f;
                }
            } else {
                i_this->m18FA = 0;
                i_this->gravity = -3.0f;
            }
        }
#else
        if (lava_y != -G_CM3D_F_INF && i_this->current.pos.y - 30.0f + REG0_F(13) < lava_y) {
            if (i_this->m18FA == 0) {
                i_this->speedF *= 0.1f;
                i_this->speed.y = 0.0f;
                fopKyM_createMpillar(&cXyz(pos_x, lava_y, pos_z), 0.5f);
            }
            i_this->m18FA = 1;
            i_this->gravity = -0.5f;
            if (i_this->speed.y < -5.0f) {
                i_this->speed.y = -5.0f;
            }
        } else {
            i_this->m18FA = 0;
            i_this->gravity = -3.0f;
        }
#endif
    }

    i_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
    mt_eye_tex_anm(i_this);

    if (l_HIO.m04 == 0) {
        if (i_this->m18D4 != 0) {
            i_this->m18D4++;
            if (i_this->m18D4 == 11) {
                i_this->m18D4 = 0;
            }
        }

        i_this->setBtAttackData(100.0f, 100.0f, 10000.0f, 0);
        i_this->setBtNowFrame(0.0f);

        for (int i = 0; i < 5; i++) {
            if ((&i_this->m456)[i] != 0) {
                (&i_this->m456)[i]--;
            }
        }
        if (i_this->m18FC != 0) {
            i_this->m18FC--;
        }
        if (i_this->m460 != 0) {
            i_this->m460--;
        }
        if (i_this->m462 != 0) {
            i_this->m462--;
        }
        if (i_this->m34A != 0) {
            i_this->m34A--;
        }
        if (i_this->m48E != 0) {
            i_this->m48E--;
            i_this->m490++;
        } else if (i_this->m490 != 0) {
            if (i_this->m490 > 15) {
                i_this->m490 = 15;
            }
            i_this->m490--;
        }

        i_this->m46A++;
        if (i_this->m466 != 0) {
            i_this->m466--;
        }

        if (i_this->m454 < 3) {
            damage_check(i_this);
        }

        i_this->mC04 = 0;

        dBgS_LinChk linChk;
        cXyz actor_pos;
        cXyz player_pos = player->current.pos;
        player_pos.y += 20.0f;
        actor_pos = i_this->current.pos;
        actor_pos.y += 30.0f;
        linChk.Set(&actor_pos, &player_pos, i_this);
        if (dComIfG_Bgsp()->LineCross(&linChk)) {
            i_this->m18F9 = 1;
        } else {
            i_this->m18F9 = 0;
        }

        switch (i_this->m454) {
        case 0: {
            mt_move(i_this);
            cLib_addCalcAngleS2(&i_this->shape_angle.x, i_this->current.angle.x, 2, 0x800);
            cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 2, 0x800);
            cLib_addCalcAngleS2(&i_this->shape_angle.z, i_this->current.angle.z, 4, 0x400);
            body_control1(i_this);
            cXyz* cc_move = i_this->mStts.GetCCMoveP();
            if (cc_move != NULL) {
                i_this->current.pos.x += cc_move->x;
                i_this->current.pos.z += cc_move->z;
                for (int i = 0; i < 0x40; i++) {
                    i_this->m6F4[i].x += cc_move->x;
                    i_this->m6F4[i].z += cc_move->z;
                }
            }
            if (i_this->m458 == 0) {
                i_this->m458 = (s16)(150.0f + cM_rndF(100.0f));
                if (l_HIO.m05 == 0 && abs(i_this->current.angle.x) < 0x1000) {
                    i_this->m48E = (s16)(REG0_F(8) + (50.0f + cM_rndF(50.0f)));
                }
            }
            break;
        }
        case 1: {
            mt_fight(i_this);
            f32 amp = i_this->m462 * (500.0f + REG0_F(14));
            i_this->m49A.y = (s16)(amp * cM_ssin(i_this->m46A * 0x2100));
            i_this->m49A.x = (s16)(amp * cM_scos(i_this->m46A * 0x2300));
            i_this->shape_angle = i_this->current.angle + i_this->m49A;
            body_control2(i_this);
            body_wall_check(i_this);
            mt_bg_check(i_this);
            cXyz* cc_move = i_this->mStts.GetCCMoveP();
            if (cc_move != NULL) {
                i_this->current.pos.x += cc_move->x;
                i_this->current.pos.z += cc_move->z;
            }
            break;
        }
        case 2: {
            mt_move_maru(i_this);
            body_control3(i_this);
            if (i_this->m466 <= 100) {
                body_wall_check(i_this);
            } else if (i_this->m18FA != 0 && i_this->m466 > 50) {
                i_this->m466 = 71;
            }
            cXyz* cc_move = i_this->mStts.GetCCMoveP();
            if (cc_move != NULL) {
                i_this->current.pos.x += cc_move->x;
                i_this->current.pos.z += cc_move->z;
            }
            if (i_this->m466 == 50) {
                i_this->m2E4 = 0;
                i_this->m454 = 1;
                i_this->m455 = 0;
#if VERSION == VERSION_DEMO
                i_this->health = 2;
                i_this->m18FB = 8;
#else
                i_this->m18FB = 2;
                i_this->health = 8;
#endif
                i_this->mSph[0].OnTgShield();
                i_this->m456 = l_HIO.m10;
                i_this->m48E = 0;
                anm_init(i_this, 10, 20.0f, 2, 1.0f, 0);
                i_this->cull.box.min.x = -200.0f;
                i_this->cull.box.min.y = -200.0f;
                i_this->cull.box.min.z = -200.0f;
                i_this->cull.box.max.x = 200.0f;
                i_this->cull.box.max.y = 200.0f;
                i_this->cull.box.max.z = 200.0f;
                i_this->mSph[0].OffAtSPrmBit(cCcD_AtSPrm_VsEnemy_e | cCcD_AtSPrm_VsOther_e);
                i_this->mSph[0].OnAtVsPlayerBit();
            }
            break;
        }
        case 3:
            i_this->m460 = 5;
            i_this->attention_info.flags = 0;
            body_control4(i_this);
            if (i_this->m45A == 1) {
                i_this->m1CBC = 1;
                fopAcM_delete(i_this);
                fopAcM_onActor(i_this);
            }
            break;
        case 6: {
            if (player->getCutType() == daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT) {
                cMtx_YrotS(*calc_mtx, i_this->shape_angle.y);
                cXyz offset;
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = -50.0f + REG0_F(14);
                cXyz pos;
                MtxPosition(&offset, &pos);
                cLib_addCalc2(&i_this->current.pos.x, player->current.pos.x + pos.x, 0.5f, 50.0f);
                cLib_addCalc2(&i_this->current.pos.z, player->current.pos.z + pos.z, 0.5f, 50.0f);
            }
            i_this->m464 = 3;
            i_this->m2E4 = 1;
            i_this->m460 = 5;
            body_control5(i_this);
            i_this->current.pos.y += i_this->speed.y;
            i_this->speed.y += i_this->gravity;
            mt_bg_check(i_this);
            if (i_this->m456 == 0) {
                if (player->getCutType() != daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT) {
                    i_this->m2E4 = 0;
                    i_this->m454 = 1;
                    i_this->m455 = 0;
                    i_this->mSph[0].OnTgShield();
                    i_this->m456 = l_HIO.m10;
                    i_this->m48E = 0;
                    anm_init(i_this, 10, 20.0f, 2, 1.0f, 0);
                    i_this->mSph[0].OffAtSPrmBit(cCcD_AtSPrm_VsEnemy_e | cCcD_AtSPrm_VsOther_e);
                    i_this->mSph[0].OnAtVsPlayerBit();
                } else {
                    i_this->m474 = 0.2f + REG0_F(13);
                    if (i_this->mEyeSph.ChkTgHit()) {
#if VERSION > VERSION_DEMO
                        i_this->health = 0;
#endif
                        i_this->m456 = REG0_S(3) + 40;
                        i_this->m474 = 1.5f + REG0_F(13);
                        mDoAud_seStart(JA_SE_LK_LAST_HIT, &i_this->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(i_this)));
#if VERSION > VERSION_DEMO
                        fopAcM_monsSeStart(i_this, JA_SE_CV_MG_DAMAGE, 0);
#endif
                        CcAtInfo atInfo;
                        atInfo.mpObj = i_this->mEyeSph.GetTgHitObj();
                        at_power_check(&atInfo);
                        mDoAud_onEnemyDamage();
                        mDoAud_bgmHitSound(atInfo.mHitSoundId);
                        dScnPly_ply_c::setPauseTimer(REG0_S(7) + 6);
                        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &i_this->current.pos);
                        cXyz scale;
                        scale.x = scale.y = scale.z = 2.0f;
                        csXyz angle;
                        angle.z = 0;
                        angle.x = 0;
                        angle.y = fopAcM_searchActorAngleY(i_this, (fopAc_ac_c*)dComIfGp_getPlayer(0));
                        dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &i_this->current.pos, &angle, &scale);
                        dKy_SordFlush_set(i_this->current.pos, 1);
                        anm_init(i_this, 8, 5.0f, 0, 1.0f, 0);
                    }
                }
            } else {
                i_this->m46E = 0;
                cLib_addCalc0(&i_this->m474, 0.05f, 0.02f + REG0_F(12));
                if (i_this->m456 == 1) {
                    bakuha(i_this);
                }
            }
            break;
        }
        }

        if (i_this->m466 > 60) {
            cLib_addCalc0(&i_this->m18F0, 1.0f, 0.05f);
        } else {
            cLib_addCalc2(&i_this->m18F0, 1.0f, 1.0f, 0.1f);
        }
    }

    {
        f32 height = 40.0f;
        dBgS_ObjLinChk linChk1;
        cXyz old_pos = i_this->old.pos;
        cXyz old_up = i_this->old.pos;
        old_up.y += height;
        linChk1.Set(&old_pos, &old_up, i_this);
        if (dComIfG_Bgsp()->LineCross(&linChk1)) {
            height = (linChk1.GetCross().y - 1.0f) - i_this->old.pos.y;
            if (height < 0.0f) {
                height = 0.0f;
            }
        }

        cXyz start = i_this->old.pos;
        start.y += height;
        cXyz end = i_this->current.pos;
        end.y += height;
        dBgS_ObjLinChk linChk2;
        linChk2.Set(&start, &end, i_this);
        bool fwd = dComIfG_Bgsp()->LineCross(&linChk2);
        dBgS_ObjLinChk linChk3;
        linChk3.Set(&end, &start, i_this);
        bool back = dComIfG_Bgsp()->LineCross(&linChk3);
        if (fwd && !back) {
            i_this->current.pos.x = linChk2.GetCross().x;
            i_this->current.pos.y = linChk2.GetCross().y;
            i_this->current.pos.z = linChk2.GetCross().z;
            cM3dGPla* pla = dComIfG_Bgsp()->GetTriPla(linChk2);
            i_this->current.pos.x += pla->mNormal.x;
            i_this->current.pos.y += pla->mNormal.y;
            i_this->current.pos.z += pla->mNormal.z;
        }
    }

    i_this->attention_info.position.x = i_this->eyePos.x;
    i_this->attention_info.position.y = i_this->eyePos.y;
    i_this->attention_info.position.z = i_this->eyePos.z;

    int add = 1;
    int lo;
    int hi;
    int hi2;
    switch (i_this->m2E4) {
    case 0:
        lo = 0;
        hi = 0x28;
        hi2 = 0x1E;
        break;
    case 1:
        lo = 0x28;
        hi = 0x64;
        hi2 = 0x5A;
        add = 2;
        break;
    case 2:
        lo = 0x64;
        hi = 0x82;
        hi2 = 0x78;
        break;
    }
    i_this->m2E8 += add;
    if (i_this->m2E8 > hi) {
        if (i_this->m2E4 == 0) {
            i_this->m2E8 = lo;
        } else {
            i_this->m2E8 = hi;
        }
    }
    i_this->m2EC += add;
    if (i_this->m2EC > hi2) {
        if (i_this->m2E4 == 0) {
            i_this->m2EC = 0;
        } else {
            i_this->m2EC = hi2;
        }
    }

    if (i_this->m454 < 2 || i_this->m466 <= 0x78) {
        if (i_this->m466 == 0x78) {
            i_this->m490 = 15;
        }
        i_this->mpMorf[0]->play(&i_this->current.pos, 0, 0);
        i_this->m46E += l_HIO.m08;
        for (int i = 0; i < 0x1E; i++) {
            int idx = i;
            if (i >= 15) {
                idx = i - 15;
            }
            s16 wave;
            if (i_this->m48E != 0) {
                if (idx >= i_this->m490) {
                    wave = i_this->m46E;
                } else {
                    wave = 0;
                }
            } else {
                idx = 14 - idx;
                if (idx >= i_this->m490) {
                    wave = i_this->m46E;
                } else {
                    wave = 0;
                }
            }
            i_this->mJntRot[i].x = (s16)((5000.0f + REG6_F(11)) * -cM_ssin(wave + i * (REG6_S(2) + 0x32C8)));
#if VERSION == VERSION_DEMO
            s16 tz = l_HIO.m0C + (5000.0f + REG6_F(12)) * cM_scos(wave + i * (REG6_S(3) + 0x32C8));
            cLib_addCalcAngleS2(&i_this->mJntRot[i].z, tz, 1, i_this->m334);
#else
            cLib_addCalcAngleS2(
                &i_this->mJntRot[i].z,
                (s16)(l_HIO.m0C + (5000.0f + REG6_F(12)) * cM_scos(wave + i * (REG6_S(3) + 0x32C8))),
                1,
                i_this->m334
            );
#endif
        }
        cLib_addCalcAngleS2(&i_this->m334, 0x2000, 1, 0x100);
        for (int i = 0; i < 8; i++) {
            i_this->m620[i] = 1.0f + i_this->m338 * cM_ssin(i_this->m46A * (REG0_S(5) + 0x9C4) + i * (REG0_S(6) + 0x1D4C));
        }
        cLib_addCalc2(&i_this->m338, 0.1f, 1.0f, 0.002f);
    } else {
        i_this->m334 = 0;
        for (int i = 0; i < 0x1E; i++) {
            i_this->mJntRot[i].x = 0;
            cLib_addCalcAngleS2(&i_this->mJntRot[i].z, 0x2710, 0xA, 0x12C);
        }
    }

    if ((i_this->m2B4 & 1) == 0) {
        for (int i = 0; i < 8; i++) {
            J3DModel* model = i_this->mpMorf[i]->getModel();
            cXyz offset;
            offset.x = REG0_F(5);
            offset.y = REG0_F(6);
            offset.z = REG0_F(7);
            MTXMultVec(model->getBaseTRMtx(), &offset, &i_this->m350[i]);
            if ((i_this->m454 >= 2 && i_this->m466 > 40) || i_this->m454 == 6 || l_HIO.m07 != 0) {
                i_this->m350[i].y += 10000.0f;
            }
            if (i >= 0 && i <= 6) {
                dComIfGp_particle_setSimple(dPa_name::ID_IT_SN_O_MAGT_FCHIP, &i_this->m350[i]);
            }
            if (i >= 1 && i <= 6) {
                dComIfGp_particle_setSimple(dPa_name::ID_IT_SN_O_MAGT_NETSU, &i_this->m350[i]);
            }
            if (i_this->m34C == 0 && (i == 1 || i == 3 || i == 5)) {
                dComIfGp_particle_setProjection(
                    dPa_name::ID_IT_SP_MAGT_KAGERO, &i_this->m350[i], NULL, NULL, 0xFF, &i_this->mPa[i]);
            }
        }
        i_this->m34C = 1;
    }

    if (i_this->m454 < 2 && i_this->m18FA == 0 && i_this->m48E == 0) {
        if (i_this->m45C == 0) {
            i_this->m45C = (s16)(45.0f + cM_rndF(45.0f));
            fopAcM_monsSeStart(i_this, JA_SE_CV_MG_NORMAL, 0);
        }
        if (i_this->m45E == 0) {
            i_this->m45E = (s16)(6.0f + cM_rndF(3.0f));
            fopAcM_monsSeStart(i_this, JA_SE_CM_MAGTAIL_MOVE, 0);
        }
    }

    if (i_this->m18F8 >= 2) {
        bakuha(i_this);
        i_this->m18F8 = -1;
    }

    return TRUE;
}

/* 00007CC4-00007CCC       .text daMt_IsDelete__FP8mt_class */
static BOOL daMt_IsDelete(mt_class*) {
    return TRUE;
}

/* 00007CCC-00007E18       .text daMt_Delete__FP8mt_class */
static BOOL daMt_Delete(mt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    dComIfG_resDelete(&i_this->mPhs, "Mt");

#if VERSION == VERSION_DEMO
    l_HIO.removeHIO();
#else
    if (i_this->mp450 != NULL) {
        i_this->mp450->quitImmortalEmitter();
        i_this->mp450->becomeInvalidEmitter();
        i_this->mp450 = NULL;
    }
#endif

    for (int i = 0; i < 8; i++) {
        i_this->mPa[i].remove();
    }

    if (i_this->m1CBC != 0 && i_this->m2B6 != 0 && i_this->m2BA != 0 && !dComIfGs_isSwitch(i_this->m2BA, fopAcM_GetRoomNo(actor))) {
        fopAcM_prm_class* append = fopAcM_CreateAppend();
        append->base.position = actor->home.pos;
        append->base.angle = actor->home.angle;
        append->base.parameters = fopAcM_GetParam(actor);
        append->room_no = actor->current.roomNo;
        fpcSCtRq_Request(fpcLy_CurrentLayer(), fpcNm_MT_e, NULL, NULL, append);
    }

    return TRUE;
}

/* 00007E18-00008400       .text CallbackCreateHeap__FP10fopAc_ac_c */
static BOOL CallbackCreateHeap(fopAc_ac_c* i_this) {
    mt_class* actor = (mt_class*)i_this;

    static int bmd_data[] = {
        dRes_INDEX_MT_BDL_MG_HEAD_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_TAIL_e,
    };
    static f32 scale_data[] = {1.0f, 1.0f, 1.0f, 0.975f, 0.925f, 0.825f, 0.75f, 0.525f};

    for (int i = 0; i < 8; i++) {
        actor->mpMorf[i] = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("Mt", bmd_data[i]),
            NULL,
            NULL,
            NULL,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            1,
            NULL,
            0x80000,
            0x37440402
        );
        if (actor->mpMorf[i] == NULL || actor->mpMorf[i]->getModel() == NULL) {
            return FALSE;
        }

        J3DModel* model = actor->mpMorf[i]->getModel();
        J3DModelData* modelData = model->getModelData();

        actor->btk[i] = new mDoExt_btkAnm();
        JUT_ASSERT(DEMO_SELECT(0x11A2, 0x11C0), actor->btk[i]);
#if VERSION == VERSION_DEMO
        actor->btk[i]->init(
            model->getModelData(),
            (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Mt", btk_data[i]),
            TRUE,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            false,
            FALSE
        );
#else
        if (!actor->btk[i]->init(
                model->getModelData(),
                (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Mt", btk_data[i]),
                TRUE,
                J3DFrameCtrl::EMode_LOOP,
                1.0f,
                0,
                -1,
                false,
                FALSE
            ))
        {
            return FALSE;
        }
#endif

        actor->brk[i] = new mDoExt_brkAnm();
        JUT_ASSERT(DEMO_SELECT(0x11AC, 0x11CD), actor->brk[i]);
#if VERSION == VERSION_DEMO
        actor->brk[i]->init(
            model->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("Mt", brk_data[i]),
            TRUE,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            false,
            FALSE
        );
#else
        if (!actor->brk[i]->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("Mt", brk_data[i]),
                TRUE,
                J3DFrameCtrl::EMode_LOOP,
                1.0f,
                0,
                -1,
                false,
                FALSE
            ))
        {
            return FALSE;
        }
#endif

        if (i == 0) {
            anm_init(actor, dRes_INDEX_MT_BCK_WAIT1_e, 20.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, 0);

            J3DAnmTexPattern* btp = NULL;
            for (int j = 0; j < 2; j++) {
                btp = (J3DAnmTexPattern*)dComIfG_getObjectRes("Mt", mt_tex_anm_idx[j]);
                btp->searchUpdateMaterialID(model->getModelData());
            }

            u16 materialNum = btp->getUpdateMaterialNum();
            actor->mpTexNoAnm = new J3DTexNoAnm[materialNum];
            for (u16 j = 0; j < materialNum; j++) {
                actor->mpTexNoAnm[j].setAnmIndex(j);
            }
            tex_anm_set(actor, 0);
        }

        model->setUserArea((u32)actor);

        for (u16 jntNo = 0; jntNo < modelData->getJointNum(); jntNo++) {
            if (i == 0) {
                if (jntNo >= 2 && jntNo <= 5) {
                    modelData->getJointNodePointer(jntNo)->setCallBack(nodeCallBack_head);
                }
            } else if (i == 7) {
                if (jntNo >= 2 && jntNo <= 5) {
                    modelData->getJointNodePointer(jntNo)->setCallBack(nodeCallBack_tail);
                }
            } else {
                if (jntNo >= 2 && jntNo <= 5) {
                    modelData->getJointNodePointer(jntNo)->setCallBack(nodeCallBack_body);
                }
            }
        }

        actor->m600[i] = scale_data[i];
    }

    static int br_bmd[] = {
        dRes_INDEX_MT_BDL_KBA_e,
        dRes_INDEX_MT_BDL_KBB_e,
        dRes_INDEX_MT_BDL_KBC_e,
    };

    for (int i = 0; i < 3; i++) {
        J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("Mt", br_bmd[i]);
        JUT_ASSERT(DEMO_SELECT(0x1256, 0x127A), modelData != 0);
        actor->br_modelL[i] = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
        JUT_ASSERT(DEMO_SELECT(0x1259, 0x127D), actor->br_modelL[i] != 0);
        actor->br_modelR[i] = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
        JUT_ASSERT(DEMO_SELECT(0x125B, 0x127F), actor->br_modelR[i] != 0);
        actor->br_modelL[i]->setBaseScale(i_this->scale);
        actor->br_modelR[i]->setBaseScale(i_this->scale);
    }

    return TRUE;
}

// Unused; stripped by the linker. Name/signature from d_a_mtD.map; body reconstructed from its leftover
// bmd_data/scale_data/br_bmd statics. The map gives it CallbackCreateHeap's size (minus the actor cast), so it is
// the same heap setup taking the mt_class directly; its assert line numbers are estimated.
static BOOL useHeapInit(mt_class* i_this) {
    static int bmd_data[] = {
        dRes_INDEX_MT_BDL_MG_HEAD_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_BODY_e,
        dRes_INDEX_MT_BDL_MG_TAIL_e,
    };
    static f32 scale_data[] = {1.0f, 1.0f, 1.0f, 0.975f, 0.925f, 0.825f, 0.75f, 0.525f};

    for (int i = 0; i < 8; i++) {
        i_this->mpMorf[i] = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("Mt", bmd_data[i]),
            NULL,
            NULL,
            NULL,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            1,
            NULL,
            0x80000,
            0x37440402
        );
        if (i_this->mpMorf[i] == NULL || i_this->mpMorf[i]->getModel() == NULL) {
            return FALSE;
        }

        J3DModel* model = i_this->mpMorf[i]->getModel();
        J3DModelData* modelData = model->getModelData();

        i_this->btk[i] = new mDoExt_btkAnm();
        JUT_ASSERT(DEMO_SELECT(0x1289, 0x12AD), i_this->btk[i]);
#if VERSION == VERSION_DEMO
        i_this->btk[i]->init(
            model->getModelData(),
            (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Mt", btk_data[i]),
            TRUE,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            false,
            FALSE
        );
#else
        if (!i_this->btk[i]->init(
                model->getModelData(),
                (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Mt", btk_data[i]),
                TRUE,
                J3DFrameCtrl::EMode_LOOP,
                1.0f,
                0,
                -1,
                false,
                FALSE
            ))
        {
            return FALSE;
        }
#endif

        i_this->brk[i] = new mDoExt_brkAnm();
        JUT_ASSERT(DEMO_SELECT(0x1293, 0x12BA), i_this->brk[i]);
#if VERSION == VERSION_DEMO
        i_this->brk[i]->init(
            model->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("Mt", brk_data[i]),
            TRUE,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            false,
            FALSE
        );
#else
        if (!i_this->brk[i]->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("Mt", brk_data[i]),
                TRUE,
                J3DFrameCtrl::EMode_LOOP,
                1.0f,
                0,
                -1,
                false,
                FALSE
            ))
        {
            return FALSE;
        }
#endif

        if (i == 0) {
            anm_init(i_this, dRes_INDEX_MT_BCK_WAIT1_e, 20.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, 0);

            J3DAnmTexPattern* btp = NULL;
            for (int j = 0; j < 2; j++) {
                btp = (J3DAnmTexPattern*)dComIfG_getObjectRes("Mt", mt_tex_anm_idx[j]);
                btp->searchUpdateMaterialID(model->getModelData());
            }

            u16 materialNum = btp->getUpdateMaterialNum();
            i_this->mpTexNoAnm = new J3DTexNoAnm[materialNum];
            for (u16 j = 0; j < materialNum; j++) {
                i_this->mpTexNoAnm[j].setAnmIndex(j);
            }
            tex_anm_set(i_this, 0);
        }

        model->setUserArea((u32)i_this);

        for (u16 jntNo = 0; jntNo < modelData->getJointNum(); jntNo++) {
            if (i == 0) {
                if (jntNo >= 2 && jntNo <= 5) {
                    modelData->getJointNodePointer(jntNo)->setCallBack(nodeCallBack_head);
                }
            } else if (i == 7) {
                if (jntNo >= 2 && jntNo <= 5) {
                    modelData->getJointNodePointer(jntNo)->setCallBack(nodeCallBack_tail);
                }
            } else {
                if (jntNo >= 2 && jntNo <= 5) {
                    modelData->getJointNodePointer(jntNo)->setCallBack(nodeCallBack_body);
                }
            }
        }

        i_this->m600[i] = scale_data[i];
    }

    static int br_bmd[] = {
        dRes_INDEX_MT_BDL_KBA_e,
        dRes_INDEX_MT_BDL_KBB_e,
        dRes_INDEX_MT_BDL_KBC_e,
    };

    for (int i = 0; i < 3; i++) {
        J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("Mt", br_bmd[i]);
        JUT_ASSERT(DEMO_SELECT(0x133D, 0x1367), modelData != 0);
        i_this->br_modelL[i] = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
        JUT_ASSERT(DEMO_SELECT(0x1340, 0x136A), i_this->br_modelL[i] != 0);
        i_this->br_modelR[i] = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
        JUT_ASSERT(DEMO_SELECT(0x1342, 0x136C), i_this->br_modelR[i] != 0);
        i_this->br_modelL[i]->setBaseScale(i_this->scale);
        i_this->br_modelR[i]->setBaseScale(i_this->scale);
    }

    return TRUE;
}

/* 000084AC-000088A8       .text daMt_Create__FP10fopAc_ac_c */
static cPhs_State daMt_Create(fopAc_ac_c* i_this) {
    fopAcM_SetupActor(i_this, mt_class);
    mt_class* a_this = (mt_class*)i_this;

    static dCcD_SrcSph sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_UNK800,
            /* SrcObjAt  Atp     */ 1,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ AT_TYPE_ALL,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
            /* SrcGObjAt Se      */ dCcG_SE_UNK2,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ dCcG_AtSPrm_NoConHit_e,
            /* SrcGObjTg Se      */ dCcG_SE_METAL,
            /* SrcGObjTg HitMark */ dCcg_TgHitMark_Purple_e,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 30.0f,
        }},
    };
    static dCcD_SrcSph eye_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
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
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 30.0f,
        }},
    };

    cPhs_State phase_state = dComIfG_resLoad(&a_this->mPhs, "Mt");
    if (phase_state == cPhs_COMPLEATE_e) {
        i_this->gbaName = 5;

        a_this->m2B4 = fopAcM_GetParam(i_this);
        if (a_this->m2B4 == 0xFF) {
            a_this->m2B4 = 0;
        }
        a_this->m2B5 = (fopAcM_GetParam(i_this) >> 8) & 0x7F;
        a_this->m2B6 = (fopAcM_GetParam(i_this) >> 15) & 1;
        a_this->m2B7 = fopAcM_GetParam(i_this) >> 16;
        a_this->m2B8 = fopAcM_GetParam(i_this) >> 24;

        if (a_this->m2B6 == 0) {
            a_this->m2B9 = i_this->current.angle.z;
            if (a_this->m2B9 != 0) {
                if (dComIfGs_isSwitch(a_this->m2B9, fopAcM_GetRoomNo(i_this))) {
                    return cPhs_ERROR_e;
                }
            }
        } else {
            a_this->m2BA = i_this->current.angle.z;
        }
        i_this->current.angle.z = 0;

        i_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("magtail", 0);

        if (!fopAcM_entrySolidHeap(i_this, CallbackCreateHeap, 0x1BFC8)) {
            return cPhs_ERROR_e;
        }

        a_this->initBt(162.5f, 200.0f);

        if (a_this->m2B4 >= 10) {
            switch (a_this->m2B5) {
            case 1:
                a_this->m488 = 1000;
                break;
            case 2:
                a_this->m488 = 500;
                break;
            case 3:
                a_this->m488 = 250;
                break;
            case 11:
                a_this->m488 = -1000;
                break;
            case 12:
                a_this->m488 = -500;
                break;
            case 13:
                a_this->m488 = -250;
                break;
            }
        } else if (a_this->m2B7 != 0xFF) {
            a_this->mpPath = dPath_GetRoomPath(a_this->m2B7, fopAcM_GetRoomNo(i_this));
            if (a_this->mpPath == NULL) {
                return cPhs_ERROR_e;
            }
            a_this->m2BC = a_this->m2B7 + 1;
            a_this->m2BE = 1;
            dPnt* pnt = a_this->mpPath->m_points;
            a_this->m47C.x = pnt->m_position.x;
            a_this->m47C.y = pnt->m_position.y;
            a_this->m47C.z = pnt->m_position.z;
        }

        if (a_this->m2B8 != 0xFF) {
            a_this->m2BB = a_this->m2B8 + 1;
        }

        fopAcM_SetMtx(i_this, a_this->mpMorf[1]->getModel()->getBaseTRMtx());
        i_this->cull.box.min.x = -200.0f;
        i_this->cull.box.min.y = -200.0f;
        i_this->cull.box.min.z = -200.0f;
        i_this->cull.box.max.x = 200.0f;
        i_this->cull.box.max.y = 200.0f;
        i_this->cull.box.max.z = 200.0f;
        i_this->gravity = -3.0f;
        a_this->m46A = cM_rndF(32768.0f);

        for (int i = 0; i < 0x40; i++) {
            a_this->m6F4[i] = i_this->current.pos;
            a_this->m9F4[i] = i_this->current.angle;
        }

#if VERSION == VERSION_DEMO
        l_HIO.entryHIO("マグテイル");
#endif

#if VERSION == VERSION_DEMO
        a_this->mAcch.Set(fopAcM_GetPosition_p(i_this), fopAcM_GetOldPosition_p(i_this), i_this, 1, &a_this->mAcchCir, fopAcM_GetSpeed_p(i_this), NULL, NULL);
#else
        a_this->mAcch.Set(&i_this->current.pos, &i_this->old.pos, i_this, 1, &a_this->mAcchCir, &i_this->speed, NULL, NULL);
#endif
        a_this->mAcchCir.SetWall(50.0f, 19.0f + REG0_F(0));
        a_this->mStts.Init(250, 2, i_this);

        for (int i = 0; i < 8; i++) {
            a_this->mSph[i].Set(sph_src);
            a_this->mSph[i].SetStts(&a_this->mStts);
        }
        a_this->mSph[0].SetAtAtp(2);

        a_this->mEyeSph.Set(eye_sph_src);
        a_this->mEyeSph.SetStts(&a_this->mStts);

        a_this->m18F0 = 1.0f;
#if VERSION == VERSION_DEMO
        i_this->health = 2;
        i_this->max_health = 2;
        a_this->m18FB = 8;
#else
        a_this->m18FB = 2;
        i_this->max_health = 8;
        i_this->health = 8;
#endif
        a_this->mEnemyIce.mpActor = i_this;
        a_this->mEnemyIce.mDeathSwitch = a_this->m2B9;

        daMt_Execute(a_this);
    }
    return phase_state;
}

static actor_method_class l_daMt_Method = {
    (process_method_func)daMt_Create,
    (process_method_func)daMt_Delete,
    (process_method_func)daMt_Execute,
    (process_method_func)daMt_IsDelete,
    (process_method_func)daMt_Draw,
};

actor_process_profile_definition g_profile_MT = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_MT_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(mt_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_MT_e,
    /* Actor SubMtd */ &l_daMt_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_FREEZE_e | fopAcStts_UNK40000_e | fopAcStts_UNK8000000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
