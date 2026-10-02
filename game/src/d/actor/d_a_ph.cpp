/**
 * d_a_ph.cpp
 * Enemy - Peahat/Seahat
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_ph.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_sea.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_jnt_hit.h"
#include "d/d_material.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_kankyo_mng.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_lib.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DNode.h"
#include "c/c_damagereaction.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/Ph.h"

// The demo's Ph archive has the *WA and *_CHAKU animations in swapped order.
#if VERSION == VERSION_DEMO
#define PH_BCK_PUCHIWA dRes_INDEX_PH_BCK_PUCHI_CHAKU_e
#define PH_BCK_BUCHIWA dRes_INDEX_PH_BCK_BUCHI_CHAKU_e
#define PH_BCK_PUCHI_CHAKU dRes_INDEX_PH_BCK_PUCHIWA_e
#define PH_BCK_BUCHI_CHAKU dRes_INDEX_PH_BCK_BUCHIWA_e
#else
#define PH_BCK_PUCHIWA dRes_INDEX_PH_BCK_PUCHIWA_e
#define PH_BCK_BUCHIWA dRes_INDEX_PH_BCK_BUCHIWA_e
#define PH_BCK_PUCHI_CHAKU dRes_INDEX_PH_BCK_PUCHI_CHAKU_e
#define PH_BCK_BUCHI_CHAKU dRes_INDEX_PH_BCK_BUCHI_CHAKU_e
#endif

/* 00000078-00000158       .text nodeCallBack_UP__FP7J3DNodei */
static BOOL nodeCallBack_UP(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        ph_class* i_this = (ph_class*)model->getUserArea();
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        if (i_this) {
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);

            u8 changed = 0;
            if (jntNo == PHP_JNT_HANE_CENT_e) {
                cXyz offset;
                offset.x = 0.0f;
                offset.y = -80.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->m02D8);
                changed = 1;
            }

            if (changed) {
                model->setAnmMtx(jntNo, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return TRUE;
}

/* 00000158-0000025C       .text nodeCallBack_DW__FP7J3DNodei */
static BOOL nodeCallBack_DW(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        ph_class* i_this = (ph_class*)model->getUserArea();
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        if (i_this) {
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);

            u8 changed = 0;
            cXyz offset;
            if (jntNo == 1) {
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->m02C0);
                changed = 1;
            }

            if (jntNo == 0) {
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->m02CC);
                changed = 1;
            }

            if (changed) {
                model->setAnmMtx(jntNo, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return TRUE;
}

/* 0000025C-000003FC       .text daPH_Draw__FP8ph_class */
static BOOL daPH_Draw(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    J3DModel* bodyModel = i_this->mpBodyMorf->getModel();
    J3DModel* propModel = i_this->mpPropellerMorf->getModel();

    g_env_light.setLightTevColorType(bodyModel, &actor->tevStr);
    g_env_light.setLightTevColorType(propModel, &actor->tevStr);

    if (i_this->mType == 1) {
        dSnap_RegistFig(DSNAP_TYPE_UNKBB, actor, actor->attention_info.position, actor->shape_angle.y, 1.0f, 1.0f, 1.0f);
    } else {
        dSnap_RegistFig(DSNAP_TYPE_UNKB1, actor, actor->attention_info.position, actor->shape_angle.y, 1.0f, 1.0f, 1.0f);
    }

    if (i_this->mEnemyIce.mFreezeTimer > 20) {
        dMat_control_c::iceEntryDL(i_this->mpBodyMorf, -1, &i_this->mBodyInvisibleModel);
        if (i_this->m02FC.x) {
            if (!i_this->m037C) {
                dMat_control_c::iceEntryDL(i_this->mpPropellerMorf, -1, &i_this->mPropellerInvisibleModel);
            }
        }
        return TRUE;
    }

    i_this->mpBodyMorf->entryDL();
    i_this->mpPropellerMorf->entryDL();

    cXyz pos = actor->current.pos;
    pos += i_this->m02E4;
    if (!(actor->actor_status & fopAcStts_CARRY_e)) {
        f32 radius = 36.0f;
        if (i_this->mType == 1) {
            radius = 70.0f;
        }
        dComIfGd_setSimpleShadow2(
            &pos, i_this->mAcch.GetGroundH(), radius, i_this->mAcch.m_gnd,
            0, 1.0f, dDlst_shadowControl_c::getSimpleTex()
        );
    }

    return TRUE;
}

/* 000003FC-000007C0       .text anm_init__FP8ph_classifUcfii */
void anm_init(ph_class* i_this, int bckFileIdx, float morf, unsigned char loopMode, float speed, int soundFileIdx, int modelType) {
#if VERSION <= VERSION_JPN
    i_this->m0374 = bckFileIdx;
#endif
    if (i_this->mType == 0) {
        if (modelType == 0) {
#if VERSION > VERSION_JPN
            i_this->m0374 = bckFileIdx;
#endif
            if (soundFileIdx >= 0) {
                i_this->mpPropellerMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("PH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    dComIfG_getObjectRes("PH", soundFileIdx)
                );
            } else {
                i_this->mpPropellerMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("PH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    NULL
                );
            }
        } else {
            if (soundFileIdx >= 0) {
                i_this->mpBodyMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("PH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    dComIfG_getObjectRes("PH", soundFileIdx)
                );
            } else {
                i_this->mpBodyMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("PH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    NULL
                );
            }
        }
    } else {
        if (modelType == 0) {
            if (soundFileIdx >= 0) {
                i_this->mpPropellerMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("SH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    dComIfG_getObjectRes("SH", soundFileIdx)
                );
            } else {
                i_this->mpPropellerMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("SH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    NULL
                );
            }
        } else {
            if (soundFileIdx >= 0) {
                i_this->mpBodyMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("SH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    dComIfG_getObjectRes("SH", soundFileIdx)
                );
            } else {
                i_this->mpBodyMorf->setAnm(
                    (J3DAnmTransform*)dComIfG_getObjectRes("SH", bckFileIdx),
                    loopMode, morf, speed, 0.0f, -1.0f,
                    NULL
                );
            }
        }
    }
}

/* 000007C0-0000097C       .text puropera_sound__FP8ph_class */
void puropera_sound(ph_class* i_this) {
    u8 play = 0;

    if (i_this->mType == 0) {
        if (!(i_this->m0354 & 1)) {
            if (abs(i_this->m033A) < 0x1000) {
                play = 1;
                i_this->m0354 |= 1;
            }
        }
        if (!(i_this->m0354 & 2)) {
            if ((s16)cLib_distanceAngleS(i_this->m033A, 0x6000) < 0x1000) {
                play = 1;
                i_this->m0354 |= 2;
            }
        }
        if (!(i_this->m0354 & 4)) {
            if ((s16)cLib_distanceAngleS(i_this->m033A, -0x4000) < 0x1000) {
                play = 1;
                i_this->m0354 |= 4;
            }
        }
        if (play) {
            fopAcM_seStart(i_this, JA_SE_CM_PH_FLY, 0);
            if ((i_this->m0354 & 7) == 7) {
                i_this->m0354 = 0;
            }
        }
    } else {
        u32 vol = (u32)((f32)i_this->m0348 / 81.0f);
        if (vol > 100) {
            vol = 100;
        }
        fopAcM_seStart(i_this, JA_SE_CM_SH_PROPELLER, vol);
    }
}

/* 0000097C-00000AE4       .text zaisitu_sound__FP8ph_classP8cCcD_Obj */
BOOL zaisitu_sound(ph_class* i_this, cCcD_Obj* hitObj) {
    fopAc_ac_c* actor = i_this;

    if (hitObj != NULL) {
        u32 atType = hitObj->GetAtType();
        if ((atType & AT_TYPE_SWORD) || (atType & AT_TYPE_UNK800) || (atType & AT_TYPE_MACHETE) ||
            (atType & AT_TYPE_DARKNUT_SWORD) || (atType & AT_TYPE_MOBLIN_SPEAR))
        {
            fopAcM_seStart(actor, JA_SE_LK_SW_HIT_S, 0x42);
        } else if ((atType & AT_TYPE_BOOMERANG) || (atType & AT_TYPE_BOKO_STICK) || (atType & AT_TYPE_STALFOS_MACE)) {
            fopAcM_seStart(actor, JA_SE_LK_W_WEP_HIT, 0x42);
        } else if (atType & AT_TYPE_WIND) {
            return TRUE;
        } else {
            fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x42);
        }
    }

    return FALSE;
}

/* 00000AE4-00000B4C       .text puropera_kaiten__FP8ph_class */
void puropera_kaiten(ph_class* i_this) {
    i_this->m033A += i_this->m0348;
    cLib_addCalcAngleS2(&i_this->m0348, i_this->m034A, 1, i_this->m034C);
    cLib_addCalcAngleS2(&i_this->m034C, 0x100, 1, 0x10);
    puropera_sound(i_this);
}

/* 00000B4C-00000D2C       .text fuwafuwa_set__FP8ph_class */
void fuwafuwa_set(ph_class* i_this) {
    if (i_this->mType == 0) {
        i_this->m0356[0] += 0x2BC;
        i_this->m0356[1] += 0xC8;
        i_this->m0356[2] += 0xC8;
        i_this->m02F0.y = 30.0f * cM_ssin(i_this->m0356[0]);
        i_this->m02F0.x = 200.0f * cM_scos(i_this->m0356[1]);
        i_this->m02F0.z = 200.0f * cM_scos(i_this->m0356[2]);
    } else {
        i_this->m0356[0] += (int)(350.0f + REG12_F(0));
        i_this->m0356[1] += (int)(100.0f + REG12_F(1));
        i_this->m0356[2] += (int)(100.0f + REG12_F(1));
        i_this->m02F0.y = (30.0f + REG12_F(2)) * cM_ssin(i_this->m0356[0]);
        i_this->m02F0.x = (1000.0f + REG12_F(3)) * cM_scos(i_this->m0356[1]);
        i_this->m02F0.z = (1000.0f + REG12_F(3)) * cM_scos(i_this->m0356[2]);
    }
}

/* 00000D2C-000011F4       .text fly_angle_set__FP8ph_classUc */
void fly_angle_set(ph_class* i_this, unsigned char param) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    s16 targetY = actor->current.angle.y;
    s16 targetZ = actor->current.angle.z;
    s16 maxStep = 0x200;

    switch (param) {
    case 1: {
        f32 dx = actor->current.pos.x - player->current.pos.x;
        f32 dy = actor->current.pos.y - (player->current.pos.y + i_this->m0378);
        f32 dz = actor->current.pos.z - player->current.pos.z;
        f32 distXZ = std::sqrtf(dx * dx + dz * dz);
        s16 targetX = cM_atan2s(dy, distXZ);
        cLib_addCalcAngleS2(&actor->current.angle.x, targetX, 1, 0x200);
    }
    case 3: {
        targetY = fopAcM_searchPlayerAngleY(actor);
        cLib_addCalcAngleS2(&actor->current.angle.y, targetY, 1, 0x500);
        break;
    }
    case 2: {
        f32 dx = actor->current.pos.x - player->current.pos.x;
        f32 dy = actor->current.pos.y - (player->current.pos.y + i_this->m0378);
        f32 dz = actor->current.pos.z - player->current.pos.z;
        f32 distXZ = std::sqrtf(dx * dx + dz * dz);
        s16 targetX = cM_atan2s(dy, distXZ);
        cLib_addCalcAngleS2(&actor->current.angle.x, targetX, 1, 0x200);
        dy = actor->current.pos.y - player->current.pos.y;
        targetX = cM_atan2s(dy, std::sqrtf(dx * dx + dz * dz));
        cLib_addCalcAngleS2(&actor->shape_angle.x, targetX, 1, 0x200);
        maxStep = 0x500;
        break;
    }
    case 4: {
        f32 dx = actor->current.pos.x - player->current.pos.x;
        f32 dy = actor->current.pos.y - (player->current.pos.y + i_this->m0378);
        f32 dz = actor->current.pos.z - player->current.pos.z;
        f32 distXZ = std::sqrtf(dx * dx + dz * dz);
        s16 targetX = (s16)cM_atan2s(dy, distXZ);
        cLib_addCalcAngleS2(&actor->current.angle.x, targetX, 1, 0x200);
        cLib_addCalcAngleS2(&actor->shape_angle.x, actor->current.angle.x, 1, 0x200);
        i_this->m0356[4] += 0xBB8;
        targetZ = 7000.0f * cM_ssin(i_this->m0356[4]);
        break;
    }
    }

    if (param <= 1 || param == 3) {
        if (i_this->mType == 0) {
            i_this->m0356[3] += 0x320;
            cLib_addCalcAngleS2(&actor->shape_angle.x, 4000.0f * cM_ssin(i_this->m0356[3]), 1, 0x200);
        } else {
            i_this->m0356[3] += 0x384;
            cLib_addCalcAngleS2(&actor->shape_angle.x, 4000.0f * cM_ssin(i_this->m0356[3]), 1, 0x200);
        }
    }

    if (param != 4) {
        cLib_addCalcAngleS2(&actor->shape_angle.y, targetY, 1, maxStep);
    }
    cLib_addCalcAngleS2(&actor->shape_angle.z, targetZ, 1, 0x200);
}

/* 000011F4-00001A10       .text body_atari_check__FP8ph_class */
BOOL body_atari_check(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    CcAtInfo atInfo;
    atInfo.pParticlePos = NULL;
    u8 skipAtCheck;

    i_this->mStts.Move();
    if (i_this->m0364[4] != 0) {
        return FALSE;
    }
    if (i_this->mBodySph.ChkTgHit()) {
    i_this->m0340 = 0;
    cCcD_Obj* hitObj = i_this->mBodySph.GetTgHitObj();
    if (hitObj == NULL) {
        return FALSE;
    }

    i_this->m0364[4] = 8;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    skipAtCheck = 0;

    switch (hitObj->GetAtType()) {
    case AT_TYPE_GRAPPLING_HOOK:
#if VERSION > VERSION_DEMO
        skipAtCheck = 1;
#endif
        if (i_this->mType == 0) {
            if (i_this->m02FC.x && !i_this->m037C) {
                actor->stealItemLeft = i_this->m0344;
#if VERSION > VERSION_JPN
                if (i_this->m0374 != dRes_INDEX_PH_BCK_PFLY_e) {
                    anm_init(i_this, dRes_INDEX_PH_BCK_PFLY_e, 5.0f, 2, 1.0f, -1, 0);
                }
#endif
                if (actor->stealItemLeft > 0) {
                    s8 oldHealth = actor->health;
                    actor->health = 10;
                    atInfo.mpObj = i_this->mBodySph.GetTgHitObj();
                    cc_at_check(actor, &atInfo);
                    i_this->m0343++;
                    actor->health = oldHealth;
                    if (i_this->m0344 > 0) {
                        i_this->m0344--;
                    }
                }
                dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &actor->attention_info.position);
#if VERSION == VERSION_DEMO
                skipAtCheck = 1;
#endif
                i_this->mAtCyl.OffAtSetBit();
                i_this->mAtCyl.ClrAtSet();
                i_this->m033F = 1;
                i_this->m0346 = 10;
            } else {
                actor->stealItemLeft = 0;
#if VERSION > VERSION_DEMO
                skipAtCheck = 0;
#endif
                i_this->m033F = 4;
                i_this->m0346 = 0x28;
            }
        }
        break;
    case AT_TYPE_SWORD: {
        u8 cutType = player->getCutType();
        if (cutType == daPy_py_c::CUT_TYPE_CUT_EA) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_EB) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_TURN) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_ROLL) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_SWORD) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_STICK) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_MACHETE) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_BT_JUMPCUT) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_BT_ROLLCUT) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_CLUB) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_DN_SWORD) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_SPEAR) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_EXA) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_EXB) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_EXMJ) {
            goto sword_smash;
        }
        if (cutType == daPy_py_c::CUT_TYPE_CUT_KESA) {
            goto sword_smash;
        }
        goto sword_done;
    sword_smash:
        i_this->m0340 = 3;
    sword_done:
        i_this->m033F = 4;
        i_this->m0346 = 0x28;
        break;
    }
    case AT_TYPE_WIND:
        i_this->m0340 = 2;
        skipAtCheck = 1;
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        i_this->m033F = 5;
        i_this->m0346 = 0x32;
        break;
    case AT_TYPE_FIRE:
    case AT_TYPE_FIRE_ARROW:
        i_this->mBodyEnemyFire.mFireDuration = 100;
        if (i_this->m02FC.x) {
            i_this->mPropellerEnemyFire.mFireDuration = 100;
        }
        i_this->m0340 = 1;
        i_this->m033F = 4;
        i_this->m0346 = 0x2B;
        break;
    case AT_TYPE_LIGHT_ARROW:
        actor->current.pos += i_this->m02E4;
        i_this->m02E4.setall(0.0f);
        i_this->m02F0.setall(0.0f);
        i_this->mEnemyIce.mLightShrinkTimer = 1;
        i_this->mEnemyIce.mParticleScale = 1.0f;
        if (i_this->mType == 1) {
            i_this->mEnemyIce.mParticleScale = 4.0f;
        }
        i_this->mEnemyIce.mYOffset = 0.0f;
        if (i_this->mType == 1) {
            i_this->mEnemyIce.mYOffset = 100.0f;
        }
        actor->attention_info.flags = 0;
        if (i_this->mType == 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_DIE, 0);
        } else {
            fopAcM_monsSeStart(actor, JA_SE_CV_SH_DIE, 0);
        }
        i_this->m0340 = 1;
        i_this->m033F = 4;
        i_this->m0346 = 0x2B;
        break;
    case AT_TYPE_ICE_ARROW:
        skipAtCheck = 1;
        actor->current.pos += i_this->m02E4;
        i_this->m02E4.setall(0.0f);
        i_this->m02F0.setall(0.0f);
        enemy_fire_remove(&i_this->mBodyEnemyFire);
        enemy_fire_remove(&i_this->mPropellerEnemyFire);
        i_this->mEnemyIce.mParticleScale = 1.0f + i_this->m039C;
        i_this->m0342 = 1;
        i_this->mEnemyIce.mFreezeDuration = 200;
        i_this->m0340 = 1;
        // fallthrough
    case AT_TYPE_NORMAL_ARROW:
        i_this->m0340 = 1;
        i_this->m033F = 4;
        i_this->m0346 = 0x2B;
        break;
    case AT_TYPE_BOMB:
        i_this->m0340 = 8;
        i_this->m033F = 4;
        i_this->m0346 = 0x2B;
        break;
    case AT_TYPE_SKULL_HAMMER:
        i_this->m033F = 4;
        i_this->m0346 = 0x2F;
        i_this->m0340 = 6;
        if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
            i_this->m0340 = 7;
            i_this->m0346 = 0x2B;
        }
        break;
    case AT_TYPE_HOOKSHOT:
        if (i_this->mType == 0) {
            i_this->m0340 = 9;
            skipAtCheck = 1;
            if (i_this->m02FC.x && !i_this->m037C) {
                dScnPly_ply_c::nextPauseTimer = 2;
                i_this->m033F = 2;
                i_this->m0346 = 0x14;
            }
        } else {
            i_this->m0340 = 1;
            i_this->m033F = 4;
            i_this->m0346 = 0x2B;
        }
        break;
    case AT_TYPE_BOOMERANG:
        if (i_this->m033F != 2) {
            if (i_this->m02FC.x && !i_this->m037C) {
                i_this->m0340 = 4;
                mDoAud_onEnemyDamage();
                dScnPly_ply_c::nextPauseTimer = 2;
                i_this->m033F = 2;
                i_this->m0346 = 0x14;
                return TRUE;
            }
        }
        i_this->m0340 = 5;
        // fallthrough
    default:
        i_this->m033F = 4;
        i_this->m0346 = 0x28;
        break;
    }

    if (skipAtCheck == 0) {
    daPy_py_c* player2 = (daPy_py_c*)dComIfGp_getPlayer(0);
    cXyz hitPos = *i_this->mBodySph.GetTgHitPosP();
    atInfo.mpObj = i_this->mBodySph.GetTgHitObj();
    cc_at_check(actor, &atInfo);

    if (i_this->m0340 == 3 || i_this->m0340 == 6 || i_this->m0340 == 7 || actor->health <= 0) {
        cXyz scaleB;
        cXyz scaleA;
        scaleA.setall(1.0f);
        scaleB.setall(2.0f);
        if (i_this->mType == 1) {
            f32 big = 3.25f + REG8_F(9);
            scaleB.setall(big);
            scaleA.setall(1.25f);
        }
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hitPos, &player2->shape_angle, &scaleA);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hitPos, &player2->shape_angle, &scaleB);
    } else {
        dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &hitPos, &player2->shape_angle, NULL);
    }
    return TRUE;
    }
    }
    return FALSE;
}

/* 00001A10-00001C18       .text hajiki_check__FP8ph_class */
BOOL hajiki_check(ph_class* i_this) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    u8 bounced = 0;

    if (i_this->mTgCyl.ChkTgHit()) {
        cCcD_Obj* hitObj = i_this->mTgCyl.GetTgHitObj();
        if (zaisitu_sound(i_this, hitObj)) {
            i_this->mAtCyl.OffAtSetBit();
            i_this->mAtCyl.ClrAtSet();
            i_this->m033F = 5;
            i_this->m0346 = 0x32;
            return TRUE;
        }
        if (hitObj != NULL) {
            if (hitObj->GetAtType() & AT_TYPE_SWORD) {
                bounced = 1;
            }
        }
    }

    if (i_this->mTgCyl.ChkAtShieldHit()) {
        cXyz pos = *i_this->mTgCyl.GetAtHitPosP();
        dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, &pos);
        fopAcM_seStart(i_this, JA_SE_OBJ_COL_SWS_NMTLP, 0);
        bounced = 1;
    }

    if (i_this->mAtCyl.ChkAtShieldHit()) {
        bounced = 1;
    }

    if (bounced) {
        f32 dx = (i_this->current.pos.x + i_this->m02E4.x) - player->current.pos.x;
        f32 dz = (i_this->current.pos.z + i_this->m02E4.z) - player->current.pos.z;
        i_this->current.angle.y = cM_atan2s(dx, dz);
        if (i_this->mType == 0) {
            i_this->speedF = 28.0f;
        } else {
            i_this->speedF = 48.0f;
        }
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        i_this->m033F = 1;
        i_this->m0346 = 10;
        return TRUE;
    }

    return FALSE;
}

/* 00001C18-00001C90       .text ph_wall_hit_check__FP8ph_class */
BOOL ph_wall_hit_check(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (i_this->mAcch.ChkWallHit()) {
        actor->current.angle.y += 0x4000;
        if (i_this->mType == 0) {
            actor->speedF = 28.0f;
        } else {
            actor->speedF = 48.0f;
        }
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        i_this->m033F = 1;
        i_this->m0346 = 10;
        return TRUE;
    }

    return FALSE;
}

/* 00001C90-00001E10       .text ph_hani_check__FP8ph_classffUc */
BOOL ph_hani_check(ph_class* i_this, float xzRange, float yRange, unsigned char useHome) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 dx;
    f32 dy;
    f32 dz;

    if (useHome == 0) {
        dx = (i_this->current.pos.x + i_this->m02E4.x) - player->current.pos.x;
        dy = (i_this->current.pos.y + i_this->m02E4.y) - player->current.pos.y;
        dz = (i_this->current.pos.z + i_this->m02E4.z) - player->current.pos.z;
    } else {
        dx = i_this->m032C.x - player->current.pos.x;
        dy = i_this->m032C.y - player->current.pos.y;
        dz = i_this->m032C.z - player->current.pos.z;
    }

    f32 distXZ = std::sqrtf(dx * dx + dz * dz);
    if (distXZ < xzRange) {
        f32 distY = std::sqrtf(dy * dy);
        if (distY < yRange) {
            return TRUE;
        }
    }

    return FALSE;
}

/* 00001E10-00001F0C       .text shibuki_set__FP8ph_class4cXyzf */
void shibuki_set(ph_class* i_this, cXyz pos, float scale) {
    scale += i_this->m039C;
    fopKyM_createWpillar(&pos, scale, 0.5f, 0);
    if (i_this->m033F == 2) {
        fopAcM_seStart(i_this, JA_SE_CM_SH_LANDING_SEA, 0);
    } else {
        if (i_this->m02FC.x) {
            if (!i_this->m037C) {
                fopAcM_seStart(i_this, JA_SE_CM_SH_RIPPLE, 0);
            }
        }
    }
}

/* 00001F0C-00002318       .text sea_water_check__FP8ph_classUc */
BOOL sea_water_check(ph_class* i_this, unsigned char param) {
    fopAc_ac_c* actor = i_this;
    u8 inWater = 0;
    u8 inSea = 0;
    f32 bob;

    i_this->m05BC = actor->current.pos;
#if VERSION > VERSION_DEMO
    i_this->m05BC.y = i_this->m032C.y;
#endif
    i_this->m05BC += i_this->m02E4;

    if (param == 0 || param == 2) {
        if (!actor->gravity) {
            actor->gravity = -5.0f;
        }
    }

    if (daSea_ChkArea(actor->current.pos.x, actor->current.pos.z)) {
        f32 waveY = daSea_calcWave(actor->current.pos.x, actor->current.pos.z);
        if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) > 50000.0f) {
            waveY = 200.0f + dComIfGp_getPlayer(0)->current.pos.y;
        }
        i_this->m05BC.y = waveY;

        f32 yOff = 100.0f;
        if (i_this->m0346 == 3) {
            yOff = 300.0f;
        }
        if (actor->current.pos.y + i_this->m02E4.y < waveY + yOff) {
            if (param == 0 || param == 2) {
                actor->gravity = 0.0f;
                i_this->m02F0.y = 0.0f;
                if (param == 0) {
                    actor->speed.setall(0.0f);
                }
                i_this->m0370 += 0x3E8;
                bob = 2.0f + 2.0f * cM_ssin(i_this->m0370);
                cLib_addCalc2(&actor->current.pos.y, waveY - bob, 1.0f, i_this->m0398);
                cLib_addCalc2(&i_this->m0398, 100.0f, 1.0f, 30.0f);
            }
            inWater = 1;
            inSea = 1;
        }
    } else if (i_this->mAcch.MaskWaterIn()) {
        inWater = 2;
        f32 waterY = i_this->mAcch.m_wtr.GetHeight();
        i_this->m05BC.y = i_this->mAcch.m_wtr.GetHeight();
        if (actor->current.pos.y + i_this->m02E4.y < 100.0f + waterY) {
            if (param == 0 || param == 2) {
                actor->gravity = 0.0f;
                if (param == 0) {
                    actor->speed.setall(0.0f);
                }
                i_this->m0370 += 0x3E8;
                if (i_this->mType == 0) {
                    bob = 2.0f + 2.0f * cM_ssin(i_this->m0370);
                } else {
                    bob = 20.0f + 40.0f * cM_ssin(i_this->m0370);
                }
                cLib_addCalc2(&actor->current.pos.y, waterY - bob, 1.0f, i_this->m0398);
                cLib_addCalc2(&i_this->m0398, 30.0f, 1.0f, 3.0f);
            }
            inWater = 1;
        }
    }

    if (inWater != 0) {
        if (i_this->m0346 != 3 && i_this->m0346 != 0x3D) {
            actor->speedF = 0.0f;
        }
        if (inWater == 1 && i_this->m0341 == 0) {
            cXyz scale;
            scale.setall(1.0f);
            if (i_this->mType == 1) {
                scale.setall(4.0f);
            }
            i_this->mParticleCallBack.end();
            dComIfGp_particle_setShipTail(dPa_name::ID_AK_JN_HAMON00, &i_this->m05BC, NULL, &scale, 0xFF, &i_this->mParticleCallBack);
            i_this->m0370 = 0;
            i_this->m0341 = 1;
            i_this->mParticleCallBack.setRate(0.0f);
            shibuki_set(i_this, i_this->m05BC, 0.5f);
        }
        return inSea ? 2 : 1;
    }

    if (i_this->m0341 != 0) {
        i_this->mParticleCallBack.end();
        i_this->m0341 = 0;
        i_this->m0398 = 0.0f;
    }
    return 0;
}

/* 00002318-0000263C       .text ph_fly_move__FP8ph_class */
void ph_fly_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;

    switch (i_this->m0346) {
    case 0:
        anm_init(i_this, dRes_INDEX_PH_BCK_PFLY_e, 5.0f, 2, 1.0f, -1, 0);
        anm_init(i_this, dRes_INDEX_PH_BCK_BFLY_e, 5.0f, 2, 1.0f, -1, 1);
        i_this->mBodySph.SetTgType(
            AT_TYPE_BOMB | AT_TYPE_BOOMERANG | AT_TYPE_NORMAL_ARROW | AT_TYPE_HOOKSHOT | AT_TYPE_SKULL_HAMMER |
            AT_TYPE_FIRE_ARROW | AT_TYPE_ICE_ARROW | AT_TYPE_LIGHT_ARROW | AT_TYPE_GRAPPLING_HOOK
        );
        i_this->m034A = 0x1000;
        i_this->m034C = 0;
        i_this->m0346++;
        // fallthrough
    case 1:
        cLib_addCalcAngleS2(&actor->current.angle.x, 0, 1, 0x200);
        cLib_addCalc0(&actor->speedF, 0.2f, 0.5f);
        actor->speed.y = 0.0f;
        fly_angle_set(i_this, 0);
        fuwafuwa_set(i_this);
        if (i_this->m0364[0] == 0) {
            if (ph_hani_check(i_this, i_this->m0390, i_this->m0394, 0)) {
                i_this->m0378 = 150.0f;
                i_this->m0346++;
            }
        }
        break;
    case 2: {
#if VERSION == VERSION_DEMO
        f32 hani_dist = 250.0f;
#endif
        cLib_addCalc2(&actor->speedF, 8.0f, 0.5f, 1.0f);
        fly_angle_set(i_this, 1);
        fuwafuwa_set(i_this);
        if (ph_hani_check(i_this, DEMO_SELECT(hani_dist, 250.0f), 25400.0f, 0)) {
            i_this->m034A = 0x2000;
            i_this->m034C = 0;
            i_this->m0364[0] = 0x14;
            i_this->m0346++;
        }
        break;
    }
    case 3:
        i_this->m0352 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        i_this->m0378 = 80.0f;
        i_this->m02F0.setall(0.0f);
        i_this->m0346++;
        // fallthrough
    case 4: {
        f32 speed = 16.0f;
        cLib_addCalc2(&actor->speedF, speed, 1.0f, 2.0f);
        fly_angle_set(i_this, 2);
        cLib_addCalcAngleS2(&actor->current.angle.y, i_this->m0352, 1, 0x500);
        cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x500);
        if (i_this->m0364[0] == 0) {
            i_this->m034A = 0x1000;
            i_this->m034C = 0;
            i_this->m0378 = 150.0f;
            s16 time = 60;
            i_this->m0364[0] = time + cM_rndF(time);
            i_this->mAtCyl.OnAtSetBit();
            i_this->mAtCyl.OnAtHitBit();
            for (int i = 0; i < 7; i++) {
                i_this->m0356[i] = 0;
            }
            i_this->m0346 = 1;
        }
        break;
    }
    }

    puropera_kaiten(i_this);
    if (sea_water_check(i_this, 1)) {
        f32 y = actor->current.pos.y + i_this->m02E4.y;
#if VERSION == VERSION_DEMO
        if (y < 200.0f + i_this->m05BC.y + REG8_F(8)) {
#else
        if (y < 200.0f + i_this->m05BC.y) {
#endif
            actor->current.angle.x = 0;
        }
    }
    if (!body_atari_check(i_this)) {
        if (!ph_wall_hit_check(i_this)) {
            hajiki_check(i_this);
        }
    }
}

/* 0000263C-00002E18       .text ph_fly_sea_move__FP8ph_class */
void ph_fly_sea_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    daShip_c* ship = dComIfGp_getShipActor();

    if (REG8_S(3) != 0) {
        return;
    }

    switch (i_this->m0346) {
    case 0: {
        for (int i = 0; i < 7; i++) {
            i_this->m0356[i] = 0;
        }
        actor->current.angle.z = 0;
        if (i_this->m0374 != dRes_INDEX_PH_BCK_PFLY_e) {
            anm_init(i_this, dRes_INDEX_PH_BCK_PFLY_e, 5.0f, 2, 1.0f, -1, 0);
            anm_init(i_this, dRes_INDEX_PH_BCK_BFLY_e, 5.0f, 2, 1.0f, -1, 1);
        }
        i_this->mAtCyl.OnAtSetBit();
        i_this->mAtCyl.OnAtHitBit();
        i_this->mBodySph.SetTgType(
            AT_TYPE_BOMB | AT_TYPE_BOOMERANG | AT_TYPE_NORMAL_ARROW | AT_TYPE_HOOKSHOT | AT_TYPE_SKULL_HAMMER |
            AT_TYPE_FIRE_ARROW | AT_TYPE_ICE_ARROW | AT_TYPE_LIGHT_ARROW | AT_TYPE_GRAPPLING_HOOK
        );
        i_this->m0378 = 150.0f;
        i_this->m034A = 0x1000;
        i_this->m034C = 0;
        i_this->m0346++;
        // fallthrough
    }
    case 1:
        if (dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
            if (i_this->m0372 > 0) {
                i_this->m0372--;
            }
            if (i_this->m0372 == 0) {
                if (ph_hani_check(i_this, i_this->m0390, i_this->m0394, 1)) {
                    i_this->m0378 = 150.0f;
                    i_this->m034A = 0x2000;
                    i_this->m034C = 0;
                    i_this->m0346++;
                }
            }
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 5.0f + REG12_F(6));
        cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 1, 0x200);
        cLib_addCalcAngleS2(&actor->shape_angle.x, 0, 1, 0x200);
        fuwafuwa_set(i_this);
        i_this->m0352 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        break;
    case 2:
        i_this->m0364[0] = 0x39;
        i_this->m0352 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        actor->shape_angle.z = 0;
        i_this->m0378 = 80.0f;
        i_this->m02F0.setall(0.0f);
        i_this->m0346++;
        // fallthrough
    case 3:
        if (!ph_hani_check(i_this, i_this->m038C, 25400.0f, 1)) {
            i_this->m0346 = 4;
            break;
        }
        if (i_this->m0364[2] == 0) {
            f32 dx = i_this->m032C.x - actor->current.pos.x;
            f32 dz = i_this->m032C.z - actor->current.pos.z;
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            if (dist > i_this->m038C) {
                i_this->m0364[2] = 0x1E;
                i_this->m0346 = 4;
                break;
            }
        }
        if (!dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
            i_this->m0372 = 0x3C;
            i_this->m0346 = 0;
            f32 dist = fopAcM_searchActorDistance(actor, (fopAc_ac_c*)ship);
            if (dist < 200.0f + REG12_F(7)) {
                i_this->m0346 = 7;
            }
            break;
        }
        if (i_this->m0364[0] == 0) {
            i_this->m0372 = 0x3C;
            i_this->m0346 = 0;
            break;
        }
        f32 speed = 98.0f;
        cLib_addCalc2(&actor->speedF, speed, 1.0f, 10.0f);
        if (i_this->m0364[1] == 0) {
            i_this->m0364[1] = 0xF;
            i_this->m0341 = 0;
        }
        break;
    case 4:
        i_this->m0364[0] = 0x1E;
        actor->current.angle.z = 0;
        i_this->m0378 = 150.0f;
        i_this->m034A = 0x1000;
        i_this->m034C = 0;
        i_this->m0346++;
        // fallthrough
    case 5:
        cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 1, 0x200);
        cLib_addCalcAngleS2(&actor->shape_angle.x, 0, 1, 0x200);
        cLib_addCalc0(&actor->speedF, 1.0f, 5.0f + REG12_F(6));
        i_this->m0352 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        if (i_this->m0364[0] == 0) {
            i_this->m0346++;
        }
        break;
    case 6: {
        cLib_addCalc2(&actor->speedF, 150.0f, 0.5f, 1.0f);
        f32 dx = i_this->m032C.x - actor->current.pos.x;
        f32 dz = i_this->m032C.z - actor->current.pos.z;
        i_this->m0352 = cM_atan2s(dx, dz);
        f32 dist = std::sqrtf(dx * dx + dz * dz);
        if (dist < 50.0f) {
            i_this->m0372 = 0x3C;
            i_this->m0346 = 0;
        }
        break;
    }
    case 7: {
        for (int i = 0; i < 7; i++) {
            i_this->m0356[i] = 0;
        }
        actor->current.angle.z = 0;
        if (i_this->m0374 != dRes_INDEX_PH_BCK_PFLY_e) {
            anm_init(i_this, dRes_INDEX_PH_BCK_PFLY_e, 5.0f, 2, 1.0f, -1, 0);
            anm_init(i_this, dRes_INDEX_PH_BCK_BFLY_e, 5.0f, 2, 1.0f, -1, 1);
        }
        actor->speedF = 98.0f;
        {
            f32 dx = i_this->m032C.x - ((fopAc_ac_c*)ship)->current.pos.x;
            f32 dz = i_this->m032C.z - ((fopAc_ac_c*)ship)->current.pos.z;
            i_this->m0352 = cM_atan2s(dx, dz);
        }
        i_this->m0346++;
        // fallthrough
    }
    case 8: {
        cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 1, 0x200);
        cLib_addCalcAngleS2(&actor->shape_angle.x, 0, 1, 0x200);
        cLib_addCalc0(&actor->speedF, 1.0f, 5.0f + REG12_F(6));
        if (actor->speedF < 1.0f) {
            if (!ph_hani_check(i_this, i_this->m038C, 25400.0f, 1)) {
                i_this->m0346 = 4;
            } else {
                i_this->m0346 = 0;
                actor->speedF = 0.0f;
            }
        }
        break;
    }
    }

    if (i_this->m0346 >= 4 && i_this->m0346 <= 6) {
        if (dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
            if (ph_hani_check(i_this, i_this->m0390, i_this->m0394, 1)) {
                i_this->m0378 = 150.0f;
                i_this->m034A = 0x2000;
                i_this->m034C = 0;
                i_this->m0346 = 2;
            }
        }
    }

    puropera_kaiten(i_this);
    cLib_addCalcAngleS2(&actor->current.angle.y, i_this->m0352, 1, 0x500);
    cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x500);
    sea_water_check(i_this, 1);
    f32 targetY = 500.0f + i_this->m05BC.y + REG12_F(8);
    if (i_this->m0346 == 3) {
        cLib_addCalcAngleS2(&actor->shape_angle.x, 4096.0f + REG12_F(11), 1, 0x800);
        targetY = player->current.pos.y;
    }
    cLib_addCalc2(&actor->current.pos.y, targetY, 1.0f, 50.0f + REG12_F(10));
    if (!body_atari_check(i_this)) {
        if (!ph_wall_hit_check(i_this)) {
            hajiki_check(i_this);
        }
    }
}

/* 00002E18-00002F50       .text ph_hane_move__FP8ph_class */
void ph_hane_move(ph_class* i_this) {
    switch (i_this->m0346) {
    case 10:
        i_this->m0378 = 100.0f;
        i_this->m034A = 0x1000;
        i_this->m034C = 0;
        i_this->m036E = 0;
        i_this->m0364[0] = 0xF;
        i_this->m0356[4] = 0;
        i_this->m0346++;
        // fallthrough
    case 11:
        i_this->shape_angle.y += 0x500;
        cLib_addCalc0(&i_this->speedF, 0.5f, 1.0f);
        fly_angle_set(i_this, 4);
        if (i_this->m0364[0] == 0) {
            if (i_this->speedF < 0.1f) {
                i_this->m033F = 0;
                if (i_this->mType == 0) {
                    i_this->m0364[0] = 0x3C;
                    i_this->mAtCyl.OnAtSetBit();
                    i_this->mAtCyl.OnAtHitBit();
                    for (int i = 0; i < 7; i++) {
                        i_this->m0356[i] = 0;
                    }
                    i_this->m0346 = 1;
                } else {
                    i_this->m0346 = 0;
                }
            }
        }
        break;
    }

    body_atari_check(i_this);
    puropera_kaiten(i_this);
}

/* 00002F50-00003768       .text ph_bunri_move__FP8ph_class */
void ph_bunri_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    // Unused, but its dead load leaves &g_dComIfG_gameInfo hoisted into a register as in the target.
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif

    switch (i_this->m0346) {
    case 0x14:
        i_this->m037C = 1.0f;
        i_this->mBodySph.OnCoSetBit();
        i_this->mTgCyl.OffTgSetBit();
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        i_this->mBodySph.SetTgType(0xFF3DFEFF);
        i_this->m02F0 = i_this->m02E4;
        i_this->m02F0.y = 0.0f;
        if (i_this->mType == 0) {
            i_this->m0350 = 0x384;
            fopAcM_seStart(actor, JA_SE_CM_PH_CUT_PROPELLER, 0);
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_CUT_PROPELLER, 0);
        } else {
            i_this->m0350 = 0x96;
            fopAcM_seStart(actor, JA_SE_CM_SH_CUT_PROP, 0);
            fopAcM_monsSeStart(actor, JA_SE_CV_SH_CUT_PROP, 0);
        }
        actor->speedF = 0.0f;
        anm_init(i_this, dRes_INDEX_PH_BCK_PAWAY_e, 1.0f, 0, 1.0f, -1, 0);
        anm_init(i_this, dRes_INDEX_PH_BCK_BDAMAGE_e, 1.0f, 0, 2.0f, -1, 1);
        i_this->m0346++;
        // fallthrough
    case 0x15:
        if (!i_this->mpBodyMorf->isStop()) {
            break;
        }
        i_this->m0308 = actor->current.pos;
        if (i_this->mType != 1) {
            actor->speed.y = 10.0f;
            actor->gravity = -3.0f;
        } else {
            actor->speed.y = 10.0f;
            actor->gravity = -10.0f;
        }
        i_this->m0346++;
        // fallthrough
    case 0x16:
        if (!daSea_ChkArea(actor->current.pos.x, actor->current.pos.z)) {
            if (i_this->mAcch.ChkGroundHit()) {
                anm_init(i_this, dRes_INDEX_PH_BCK_BCHAKUCHI_e, 1.0f, 0, 1.0f, -1, 1);
                fopAcM_seStart(actor, JA_SE_CM_PH_FALL, dComIfG_Bgsp()->GetMtrlSndId(i_this->mAcch.m_gnd));
                i_this->m0346++;
            }
        }
        break;
    case 0x17:
        if (!i_this->mpBodyMorf->isStop()) {
            break;
        }
        i_this->m0346++;
        // fallthrough
    case 0x18:
        if (i_this->m0350 == 0) {
            i_this->m033F = 3;
            i_this->m0346 = 0x1E;
            break;
        } else {
            f32 dx = i_this->m0308.x - actor->current.pos.x;
            f32 dz = i_this->m0308.z - actor->current.pos.z;
            i_this->mBodySph.SetTgType(0xFF3DFEFF);
            actor->current.angle.y = actor->current.angle.y + cM_rndFX(16384.0f);
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            if (dist >= 200.0f) {
                s16 yaw = cM_atan2s(dx, dz);
                s16 angDist = cLib_distanceAngleS(actor->current.angle.y, yaw);
                if (angDist < 0x2000) {
                    actor->current.angle.y = cM_atan2s(dx, dz);
                } else {
                    s16 dMinus;
                    s16 plus = actor->current.angle.y + 0x2000;
                    s16 minus = actor->current.angle.y - 0x2000;
                    dMinus = cLib_distanceAngleS(minus, (s16)cM_atan2s(dx, dz));
                    s16 dPlus = cLib_distanceAngleS(plus, (s16)cM_atan2s(dx, dz));
                    if (dPlus < dMinus) {
                        actor->current.angle.y = plus;
                    } else {
                        actor->current.angle.y = minus;
                    }
                }
            }
            anm_init(i_this, dRes_INDEX_PH_BCK_BJUMP_e, 2.0f, 0, 1.0f, -1, 1);
            i_this->m0346 = 0x19;
        }
        // fallthrough
    case 0x19:
        if (i_this->mpBodyMorf->getFrame() < 7.0f) {
            cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x1000);
        }
        if (i_this->mpBodyMorf->checkFrame(7.0f)) {
            actor->speedF = 5.0f;
            actor->gravity = -3.0f;
            actor->speed.y = 20.0f;
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_JUMP, 0);
            i_this->m0346++;
        }
        break;
    case 0x1A:
        if (i_this->mAcch.ChkGroundHit()) {
            fopAcM_seStart(actor, JA_SE_CM_PH_JUMP, dComIfG_Bgsp()->GetMtrlSndId(i_this->mAcch.m_gnd));
            actor->speedF = 0.0f;
            i_this->m0346 = 0x18;
        }
        break;
    }

    if (i_this->m0346 >= 0x18) {
        if (i_this->m0350 > 0) {
            i_this->m0350--;
        }
    }

    cLib_addCalcAngleS2(&actor->current.angle.x, 0, 1, 0x500);
    cLib_addCalcAngleS2(&actor->shape_angle.x, actor->current.angle.x, 1, 0x500);
    cLib_addCalcAngleS2(&actor->current.angle.z, 0, 1, 0x500);
    cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 1, 0x500);

    if (body_atari_check(i_this)) {
        s16 oldMode = i_this->m0346;
        if (i_this->m0340 == 2) {
            fopAcM_seStart(actor, JA_SE_CM_PH_HIT_WIND, 0);
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_CUT_PROPELLER, 0);
            s16 yaw = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            actor->current.angle.y = yaw + 0x8000;
            actor->speedF = 30.0f;
            i_this->m0346 = oldMode;
            i_this->m033F = 2;
        }
    }

    BOOL water = sea_water_check(i_this, 1);
    if (water) {
        i_this->m033F = 6;
        if (water == 2) {
            i_this->m0346 = 0x3C;
        } else {
            i_this->m0346 = 0x46;
        }
    }
}

/* 00003768-00003F34       .text ph_fujyou_move__FP8ph_class */
void ph_fujyou_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;

    switch (i_this->m0346) {
    case 0x1E:
        fopAcM_seStart(actor, JA_SE_CM_PH_RECOVER, 0);
        i_this->m0388 = 200.0f + actor->current.pos.y;
        i_this->mBodySph.OffCoSetBit();
        i_this->mTgCyl.OnTgSetBit();
        i_this->mAtCyl.OnAtSetBit();
        i_this->mAtCyl.OnAtHitBit();
        anm_init(i_this, dRes_INDEX_PH_BCK_PHUKKI_e, 5.0f, 0, 1.0f, -1, 0);
        anm_init(i_this, dRes_INDEX_PH_BCK_BHUKKI_e, 5.0f, 0, 1.0f, -1, 1);
        i_this->m0346++;
        // fallthrough
    case 0x1F:
        cLib_addCalc2(&i_this->m02FC.x, 1.0f + i_this->m03A0, 1.0f, 0.3f + (0.3f + i_this->m03A0));
        i_this->m02FC.y = i_this->m02FC.z = i_this->m02FC.x;
        if (i_this->m02FC.x > 0.8f + i_this->m03A0) {
            f32 scl = 1.0f + i_this->m03A0;
            i_this->m02FC.x = scl;
            i_this->m02FC.y = scl;
            i_this->m02FC.z = scl;
        }
        if (i_this->mpBodyMorf->isStop()) {
            i_this->m034A = 0x1000;
            i_this->m034C = 0;
            i_this->m0384 = 0.0f;
            for (int i = 0; i < 7; i++) {
                i_this->m0356[i] = 0;
            }
            actor->gravity = -3.0f;
            i_this->m0346++;
        }
        break;
    case 0x20:
        i_this->m033A += (s16)(1000.0f * i_this->m0384);
        puropera_sound(i_this);
        cLib_addCalc2(&i_this->m0384, 4.0f, 0.03f, 0.05f);
        i_this->m0356[4] += (s16)(200.0f * i_this->m0384);
        cLib_addCalcAngleS2(&actor->current.angle.z, 4000.0f * cM_ssin(i_this->m0356[4]), 1, 0x1000);
        actor->shape_angle.z = actor->current.angle.z;
        actor->speed.y = i_this->m0384;
        i_this->m0356[0] += 0x7D0;
        i_this->m0356[1] += 0x4B0;
        i_this->m0356[2] += 0x4B0;
        i_this->m02F0.y = 10.0f * cM_ssin(i_this->m0356[0]);
        i_this->m02F0.x = 20.0f * cM_scos(i_this->m0356[1]);
        i_this->m02F0.z = 20.0f * cM_scos(i_this->m0356[2]);
        if (actor->current.pos.y > i_this->m0388) {
            i_this->mAtCyl.OnAtSetBit();
            i_this->mAtCyl.OnAtHitBit();
            for (int i = 0; i < 7; i++) {
                i_this->m0356[i] = 0;
            }
            actor->current.angle.z = 0;
            i_this->m033F = 0;
            i_this->mBodySph.SetTgType(
                AT_TYPE_BOMB | AT_TYPE_BOOMERANG | AT_TYPE_NORMAL_ARROW | AT_TYPE_HOOKSHOT | AT_TYPE_SKULL_HAMMER |
                AT_TYPE_FIRE_ARROW | AT_TYPE_ICE_ARROW | AT_TYPE_LIGHT_ARROW | AT_TYPE_GRAPPLING_HOOK
            );
            actor->gravity = -3.0f;
            i_this->m0346 = 1;
        }
        break;
    case 0x21:
        fopAcM_seStart(actor, JA_SE_CM_SH_RECOVER_PROP, 0);
        i_this->mBodySph.OffCoSetBit();
        anm_init(i_this, dRes_INDEX_PH_BCK_PHUKKI_e, 5.0f, 0, 1.0f, -1, 0);
        anm_init(i_this, dRes_INDEX_PH_BCK_BHUKKI_e, 5.0f, 0, 1.0f, -1, 1);
        i_this->m0346++;
        // fallthrough
    case 0x22:
        cLib_addCalc2(&i_this->m02FC.x, 1.0f + i_this->m03A0, 1.0f, 0.3f + (0.3f + i_this->m03A0));
        i_this->m02FC.y = i_this->m02FC.z = i_this->m02FC.x;
        if (i_this->m02FC.x > 0.8f + i_this->m03A0) {
            f32 scl = 1.0f + i_this->m03A0;
            i_this->m02FC.x = scl;
            i_this->m02FC.y = scl;
            i_this->m02FC.z = scl;
        }
        if (i_this->mpBodyMorf->isStop()) {
            i_this->m034A = 0x1000;
            i_this->m034C = 0;
            i_this->m0384 = 0.0f;
            for (int i = 0; i < 7; i++) {
                i_this->m0356[i] = 0;
            }
            actor->gravity = -3.0f;
            i_this->m0346++;
        }
        break;
    case 0x23:
        i_this->m033A += (s16)(1000.0f * i_this->m0384);
        puropera_sound(i_this);
        cLib_addCalc2(&i_this->m0384, 6.0f, 0.3f, 0.5f);
        i_this->m0356[4] += (s16)(200.0f * i_this->m0384);
        cLib_addCalcAngleS2(&actor->current.angle.z, 4000.0f * cM_ssin(i_this->m0356[4]), 1, 0x1000);
        actor->shape_angle.z = actor->current.angle.z;
        actor->speed.y = i_this->m0384;
        i_this->m0356[0] += 0x7D0;
        i_this->m0356[1] += 0x4B0;
        i_this->m0356[2] += 0x4B0;
        i_this->m02F0.y = 10.0f * cM_ssin(i_this->m0356[0]);
        i_this->m02F0.x = 20.0f * cM_scos(i_this->m0356[1]);
        i_this->m02F0.z = 20.0f * cM_scos(i_this->m0356[2]);
        if (std::fabsf(actor->current.pos.y - (500.0f + i_this->m05BC.y)) < 10.0f) {
            i_this->m033F = 0;
            i_this->m0346 = 4;
        }
        break;
    case 0x24:
        i_this->m033A += (s16)(1000.0f * i_this->m0384);
        puropera_sound(i_this);
        actor->speed.y = 70.0f;
        if (actor->current.pos.y > 500.0f + i_this->m05BC.y) {
            i_this->m033F = 0;
            i_this->m0346 = 0;
        }
        break;
    }

    if (i_this->mType != 1) {
        if (sea_water_check(i_this, 1)) {
            if (actor->current.pos.y + i_this->m02E4.y < 100.0f + i_this->m05BC.y) {
                actor->gravity = 0.0f;
            }
        }
    }
    if (!body_atari_check(i_this)) {
        if (hajiki_check(i_this)) {
            anm_init(i_this, dRes_INDEX_PH_BCK_PFLY_e, 5.0f, 2, 1.0f, -1, 0);
            f32 scl = 1.0f + i_this->m03A0;
            i_this->m02FC.x = scl;
            i_this->m02FC.y = scl;
            i_this->m02FC.z = scl;
        }
    }
}

/* 00003F34-0000405C       .text dead_item__FP8ph_class */
void dead_item(ph_class* i_this) {
    cXyz pos = i_this->current.pos;
    pos += i_this->m02E4;
    pos.y += 40.0f;

    u8 scale = (u8)(5.0f + 2.0f * i_this->m039C);
    if (i_this->mType == 0) {
        fopAcM_createDisappear(i_this, &pos, scale, daDisItem_IBALL_e, i_this->stealItemBitNo);
    } else {
        fopAcM_createDisappear(i_this, &pos, scale, daDisItem_IBALL_e, 0xFF);
        int n = dComIfGs_getEventReg(dSv_event_flag_c::UNK_7EFF) + 1;
        n = cLib_maxLimit<int>(n, 0xFF) & 0xFF;
        dComIfGs_setEventReg(dSv_event_flag_c::UNK_7EFF, n);
    }

    fopAcM_onActor(i_this);
    fopAcM_delete(i_this);
}

/* 0000405C-00004988       .text ph_damage_dead_move__FP8ph_class */
void ph_damage_dead_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    switch (i_this->m0346) {
    case 0x28:
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, player) + 0x8000;
        if (actor->health <= 0 || i_this->m0340 == 3) {
            actor->speedF = 25.0f;
            i_this->m034E = 0x2000;
        } else {
            actor->speedF = 15.0f;
            i_this->m034E = 0x1000;
        }
        if (i_this->m0341 != 0) {
            i_this->m034E = -0x8000;
        }
        if (actor->health > 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_DAMAGE, 0);
        } else if (i_this->mType == 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_DIE, 0);
        } else {
            fopAcM_monsSeStart(actor, JA_SE_CV_SH_DIE, 0);
        }
        i_this->m0364[0] = 0;
        if (i_this->m02FC.x) {
            anm_init(i_this, PH_BCK_PUCHI_CHAKU, 1.0f, 0, 1.0f, -1, 0);
        }
        anm_init(i_this, dRes_INDEX_PH_BCK_BDAMAGE_e, 1.0f, 0, 1.0f, -1, 1);
        i_this->m0346++;
        // fallthrough
    case 0x29:
        cLib_addCalc0(&actor->speedF, 0.5f, 1.0f);
        if (actor->health <= 0) {
            if (actor->speedF < 5.0f) {
                cLib_addCalc0(&i_this->m02FC.x, 0.3f, 1.0f + i_this->m03A0);
                {
                    f32 tmp = i_this->m02FC.x;
                    i_this->m02FC.z = tmp;
                    i_this->m02FC.y = tmp;
                }
                cLib_addCalc0(&actor->scale.x, 0.3f, 1.0f + i_this->m039C);
                {
                    f32 tmp = actor->scale.x;
                    actor->scale.z = tmp;
                    actor->scale.y = tmp;
                }
            }
        }
        actor->shape_angle.y += i_this->m034E;
        cLib_addCalcAngleS2(&i_this->m034E, 0, 1, 0x200);
        if (i_this->mpBodyMorf->isStop()) {
            if (actor->health > 0) {
                actor->speedF = 0.0f;
                i_this->m033F = 2;
                i_this->m0346 = 0x18;
            } else {
                dead_item(i_this);
            }
        }
        break;
    case 0x2A:
        if (i_this->m0364[0] == 0) {
            i_this->m033F = 2;
            i_this->m0346 = 0x18;
        }
        break;
    case 0x2B: {
        f32 dx = (actor->current.pos.x + i_this->m02E4.x) - player->current.pos.x;
        f32 dz = (actor->current.pos.z + i_this->m02E4.z) - player->current.pos.z;
        actor->current.angle.y = cM_atan2s(dx, dz);
        actor->speedF = 25.0f;
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        actor->health = 0;
        i_this->m0356[4] = 0;
        if (i_this->mType == 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_DIE, 0);
        } else {
            fopAcM_monsSeStart(actor, JA_SE_CV_SH_DIE, 0);
        }
        anm_init(i_this, dRes_INDEX_PH_BCK_BDAMAGE_e, 1.0f, 0, 1.0f, -1, 1);
        f32 scl = 1.0f + i_this->m039C;
        i_this->m0320.x = scl;
        i_this->m0320.y = scl;
        i_this->m0320.z = scl;
        if (i_this->m0342 != 0) {
            actor->speed.setall(0.0f);
            actor->speedF = 0.0f;
            i_this->m0364[0] = 0;
            i_this->m0346 = 0x2E;
            break;
        }
        if (!i_this->mAcch.ChkGroundHit()) {
            actor->speed.y = 0.0f;
            actor->gravity = 0.0f;
            i_this->m0346 = 0x2C;
            i_this->m0364[0] = 0x3C;
            if (i_this->m0340 == 7) {
                i_this->m0364[0] = 0;
            }
            anm_init(i_this, PH_BCK_PUCHI_CHAKU, 1.0f, 0, 1.0f, -1, 0);
            if (i_this->m0340 == 6) {
                i_this->m0364[0] = 0;
                actor->speedF = 15.0f;
                actor->speed.y = 20.0f;
                actor->gravity = -5.0f;
                i_this->m0346 = 0x2D;
            }
        } else {
            i_this->m0346 = 0x2E;
        }
        break;
    }
    case 0x2C:
        cLib_addCalc0(&actor->speedF, 0.5f, 0.5f);
        actor->shape_angle.y += 0x500;
        fly_angle_set(i_this, 4);
        if (i_this->m0364[0] != 0) {
            if (!i_this->mpPropellerMorf->isStop()) {
                break;
            }
        }
        dead_item(i_this);
        break;
    case 0x2D:
        if (i_this->m0364[0] == 1) {
            actor->gravity = -5.0f;
            actor->speed.y = 23.0f;
            if (i_this->m0356[4] > 1) {
                actor->speed.y = 15.0f;
            }
            actor->speedF = 5.0f;
            i_this->m0320.x = 0.8f + i_this->m039C;
            i_this->m0320.y = 1.2f + i_this->m039C;
        }
        if (i_this->m0364[0] == 0) {
            if (i_this->mAcch.ChkGroundHit()) {
                actor->speedF = 0.0f;
                i_this->m0320.x = 1.2f + i_this->m039C;
                i_this->m0320.y = 0.8f + i_this->m039C;
                i_this->m0364[0] = 3;
                i_this->m0356[4]++;
                if (i_this->m0356[4] > 2) {
                    dead_item(i_this);
                }
            }
        }
        cLib_addCalc2(&actor->scale.x, i_this->m0320.x, 0.5f, 0.5f);
        cLib_addCalc2(&actor->scale.y, i_this->m0320.y, 0.5f, 0.5f);
        break;
    case 0x2E:
        cLib_addCalc0(&actor->speedF, 0.5f, 0.5f);
        actor->shape_angle.y += 0x500;
        if (i_this->m0364[0] == 0) {
            dead_item(i_this);
        }
        break;
    case 0x2F: {
        actor->shape_angle.x = 0;
        actor->shape_angle.z = 0;
        actor->current.angle.x = 0;
        actor->current.angle.z = 0;
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        i_this->mBodySph.OffCoSetBit();
        i_this->mBodySph.OffTgSetBit();
        actor->speedF = 0.0f;
        actor->gravity = -3.0f;
        actor->attention_info.flags = 0;
        actor->health = 0;
        i_this->m0356[4] = 0;
        if (i_this->m02FC.x) {
            anm_init(i_this, PH_BCK_PUCHI_CHAKU, 1.0f, 0, 1.0f, -1, 0);
        }
        anm_init(i_this, dRes_INDEX_PH_BCK_BDAMAGE_e, 1.0f, 0, 1.0f, -1, 1);
        i_this->m0380 = 10.0f;
        if (i_this->mType == 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_PH_DIE, 0);
        } else {
            fopAcM_monsSeStart(actor, JA_SE_CV_SH_DIE, 0);
        }
        i_this->m0346++;
        // fallthrough
    }
    case 0x30:
        cLib_addCalc2(&actor->scale.y, 0.1f, 1.0f, 0.5f);
        cLib_addCalc2(&actor->scale.x, 1.7f, 1.0f, 0.7f);
        if (i_this->m02FC.x) {
            i_this->m02FC.x = actor->scale.x;
            i_this->m02FC.y = actor->scale.y;
            i_this->m02FC.z = actor->scale.z;
        }
        if (actor->scale.y < 0.1f) {
            actor->scale.y = 0.1f;
            actor->scale.x = 1.7f;
            i_this->m0364[0] = 0x14;
            i_this->m0346++;
        }
        break;
    case 0x31:
        if (i_this->m0364[0] == 0) {
            dead_item(i_this);
        }
        break;
    }

    actor->current.angle.x = 0;
    actor->current.angle.z = 0;
    cLib_addCalcAngleS2(&actor->shape_angle.x, actor->current.angle.x, 1, 0x500);
    cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 1, 0x500);
    if (actor->health > 0) {
        u8 prevWater = i_this->m0341;
        body_atari_check(i_this);
        BOOL water = sea_water_check(i_this, 0);
        if (water != 0) {
            if (prevWater == 0) {
                if (i_this->m0341 != 0) {
                    i_this->m033F = 6;
                    if (water == 2) {
                        i_this->m0346 = 0x3C;
                    } else {
                        i_this->m0346 = 0x46;
                    }
                }
            }
        }
    }
}

/* 00004988-00005134       .text ph_wind_move__FP8ph_class */
void ph_wind_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    BOOL water = sea_water_check(i_this, 1);

    switch (i_this->m0346) {
    case 0x32:
        fopAcM_seStart(actor, JA_SE_CM_PH_HIT_WIND, 0);
        fopAcM_monsSeStart(actor, JA_SE_CV_PH_CUT_PROPELLER, 0);
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        i_this->mTgCyl.OffTgSetBit();
        i_this->mBodySph.OnCoSetBit();
        i_this->mBodySph.SetTgType(0xFF3DFEFF);
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
        {
            f32 dy = (actor->current.pos.y + i_this->m02E4.y) - player->current.pos.y;
            f32 distY = std::sqrtf(dy * dy);
            actor->speed.y = 10.0f - 0.1f * distY;
        }
        if (actor->speed.y < 0.0f) {
            actor->speed.y = 3.0f;
        }
        actor->speedF = 30.0f;
        actor->gravity = 0.0f;
        i_this->m0350 = 0;
        for (int i = 0; i < 7; i++) {
            i_this->m0356[i] = 0;
        }
        actor->current.angle.x = 0;
        actor->current.angle.z = 0;
        i_this->m02F0.setall(0.0f);
        i_this->m0356[6] = -0x1000;
        i_this->m0356[5] = -0x2710;
        if (cM_rnd() < 0.5f) {
            i_this->m0356[5] = 0x2710;
            i_this->m0356[6] = 0x1000;
        }
        anm_init(i_this, PH_BCK_PUCHI_CHAKU, 1.0f, 0, 1.0f, -1, 0);
        anm_init(i_this, PH_BCK_BUCHI_CHAKU, 1.0f, 0, 1.0f, -1, 1);
        i_this->m0346++;
        // fallthrough
    case 0x33:
        i_this->m0356[4] += i_this->m0356[5];
        cLib_addCalcAngleS2(&actor->shape_angle.z, 20000.0f * cM_ssin(i_this->m0356[4]), 1, 0x500);
        cLib_addCalcAngleS2(&actor->shape_angle.x, 20000.0f * cM_ssin(i_this->m0356[4]), 1, 0x500);
        actor->shape_angle.y += i_this->m0356[6];
        cLib_addCalc0(&actor->speedF, 0.3f, 1.0f);
        cLib_addCalc0(&actor->speed.y, 0.3f, 1.0f);
        if (actor->speedF > 0.2f) {
            break;
        }
        i_this->m0356[5] = 0;
        actor->speedF = 0.0f;
        i_this->m0346++;
        // fallthrough
    case 0x34:
        if (water) {
            i_this->m033F = 6;
            if (water == 2) {
                i_this->m0346 = 0x3C;
            } else {
                i_this->m0346 = 0x46;
            }
            return;
        }
        actor->gravity = -0.3f;
        actor->shape_angle.y += i_this->m0356[6];
        cLib_addCalcAngleS2(&i_this->m0356[6], 0, 1, 0x100);
        fly_angle_set(i_this, 0);
        if (i_this->mAcch.ChkGroundHit()) {
            fopAcM_seStart(actor, JA_SE_CM_PH_FALL, dComIfG_Bgsp()->GetMtrlSndId(i_this->mAcch.m_gnd));
            actor->gravity = -3.0f;
            anm_init(i_this, PH_BCK_PUCHIWA, 1.0f, 0, 1.0f, -1, 0);
            anm_init(i_this, PH_BCK_BUCHIWA, 1.0f, 0, 1.0f, -1, 1);
            i_this->m0346++;
        }
        break;
    case 0x35:
        if (water) {
            i_this->m033F = 6;
            if (water == 2) {
                i_this->m0346 = 0x3C;
            } else {
                i_this->m0346 = 0x46;
            }
            return;
        }
        {
            cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 1, 0x500);
            cLib_addCalcAngleS2(&actor->shape_angle.x, actor->current.angle.x, 1, 0x500);
        }
        actor->shape_angle.y += i_this->m0356[5];
        if (i_this->mpBodyMorf->isStop()) {
            if (!i_this->m02FC.x) {
                actor->speedF = 0.0f;
                i_this->m033F = 2;
                i_this->m0346 = 0x18;
                return;
            }
            i_this->m0364[1] = (s16)(50.0f + cM_rndF(50.0f));
            actor->shape_angle.x = 0;
            actor->shape_angle.z = 0;
            i_this->m0346++;
        }
        break;
    case 0x36:
        if (water) {
            i_this->m033F = 6;
            if (water == 2) {
                i_this->m0346 = 0x3C;
            } else {
                i_this->m0346 = 0x46;
            }
            return;
        }
        cLib_addCalc0(&actor->speedF, 0.3f, 1.0f);
        if (actor->speedF > 0.1f) {
            actor->shape_angle.y += i_this->m0356[5];
        }
        if (i_this->m0364[1] == 0) {
            actor->speedF = 0.0f;
            if (!i_this->m02FC.x) {
                i_this->m033F = 2;
                i_this->m0346 = 0x18;
            } else {
                i_this->m033F = 3;
                i_this->m0346 = 0x1E;
            }
        }
        break;
    }

    if (body_atari_check(i_this)) {
        actor->shape_angle.x = 0;
        actor->shape_angle.z = 0;
        if (i_this->m0340 == 2) {
            if (i_this->mAcch.ChkGroundHit()) {
                i_this->m0356[5] = -0x3E8;
                if (cM_rnd() < 0.5f) {
                    i_this->m0356[5] = 0x3E8;
                }
                actor->speedF = 20.0f;
                if (i_this->m0374 != PH_BCK_PUCHI_CHAKU) {
                    anm_init(i_this, PH_BCK_PUCHI_CHAKU, 1.0f, 0, 1.0f, -1, 0);
                }
                fopAcM_seStart(actor, JA_SE_CM_PH_HIT_WIND, 0);
                fopAcM_monsSeStart(actor, JA_SE_CV_PH_CUT_PROPELLER, 0);
                i_this->m0346 = 0x36;
            } else {
                i_this->m033F = 5;
                i_this->m0346 = 0x32;
            }
        }
    }
}

/* 00005134-00005628       .text ph_water_move__FP8ph_class */
void ph_water_move(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    switch (i_this->m0346) {
    case 0x3C:
        i_this->mBodySph.OnCoSetBit();
        i_this->mBodySph.SetTgType(0xFF3DFEFF);
        actor->shape_angle.x = 0;
        actor->shape_angle.z = 0;
        actor->current.angle.x = 0;
        actor->current.angle.z = 0;
        for (int i = 0; i < 7; i++) {
            i_this->m0356[i] = 0;
        }
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        if (i_this->mType != 1) {
            if (!i_this->m02FC.x) {
                i_this->m0364[1] = (s16)(200.0f + cM_rndF(200.0f));
            } else {
                i_this->m0364[1] = (s16)(100.0f + cM_rndF(100.0f));
            }
            anm_init(i_this, PH_BCK_PUCHIWA, 1.0f, 0, 1.0f, -1, 0);
            anm_init(i_this, PH_BCK_BUCHIWA, 1.0f, 0, 1.0f, -1, 1);
        } else {
            i_this->m0364[1] = 0x96;
            i_this->mAtCyl.OnAtSetBit();
            i_this->mAtCyl.OnAtHitBit();
            anm_init(i_this, dRes_INDEX_PH_BCK_BJAWS_e, 1.0f, 2, 1.0f, -1, 1);
        }
        i_this->m0364[2] = 0x14;
        {
            f32 dx = (actor->current.pos.x + i_this->m02E4.x) - player->current.pos.x;
            f32 dz = (actor->current.pos.z + i_this->m02E4.z) - player->current.pos.z;
            actor->current.angle.y = cM_atan2s(dx, dz) + 0x8000;
        }
        i_this->m0346++;
        // fallthrough
    case 0x3D:
        if (i_this->m0364[0] == 0) {
            i_this->m0364[0] = 0xF;
            i_this->m0341 = 0;
        }
        {
            cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x700);
        }
        f32 speed = 30.0f;
        cLib_addCalc2(&actor->speedF, speed, 1.0f, 10.0f);
        {
            u32 vol = (u32)(3.4f * actor->speedF);
            if (vol > 100) {
                vol = 100;
            }
            fopAcM_seStart(actor, JA_SE_CM_SH_CRUISING, vol);
        }
        if (!ph_hani_check(i_this, i_this->m038C, 25400.0f, 1)) {
            i_this->m033F = 3;
            i_this->m0346 = 0x21;
            break;
        }
        if (i_this->m0364[2] == 0) {
            f32 dx = i_this->m032C.x - actor->current.pos.x;
            f32 dz = i_this->m032C.z - actor->current.pos.z;
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            if (dist > i_this->m038C) {
                i_this->m033F = 3;
                i_this->m0346 = 0x21;
                break;
            }
        }
        if (i_this->m0364[1] == 0) {
            i_this->m033F = 3;
            i_this->m0346 = 0x21;
        }
        break;
    case 0x46:
        i_this->mBodySph.OnCoSetBit();
        i_this->mBodySph.SetTgType(0xFF3DFEFF);
        actor->shape_angle.x = 0;
        actor->shape_angle.z = 0;
        actor->current.angle.x = 0;
        actor->current.angle.z = 0;
        for (int i = 0; i < 7; i++) {
            i_this->m0356[i] = 0;
        }
        i_this->mAtCyl.OffAtSetBit();
        i_this->mAtCyl.ClrAtSet();
        if (!i_this->m02FC.x) {
            i_this->m0364[1] = (s16)(200.0f + cM_rndF(200.0f));
        } else {
            i_this->m0364[1] = (s16)(100.0f + cM_rndF(100.0f));
        }
        anm_init(i_this, PH_BCK_PUCHIWA, 1.0f, 0, 1.0f, -1, 0);
        anm_init(i_this, PH_BCK_BUCHIWA, 1.0f, 0, 1.0f, -1, 1);
        i_this->m0346++;
        // fallthrough
    case 0x47:
        if (i_this->m0364[1] == 0) {
            i_this->m033F = 3;
            if (i_this->mType == 1) {
                i_this->m0346 = 0x21;
            } else {
                i_this->m0346 = 0x1E;
            }
        }
        break;
    }

    body_atari_check(i_this);
    sea_water_check(i_this, 2);
}

/* 00005628-0000573C       .text BG_check__FP8ph_class */
void BG_check(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (i_this->mType == 0) {
        f32 wallR = 40.0f;
        if (i_this->m02FC.x) {
            if (!i_this->m037C) {
                wallR = 100.0f;
            }
        }
        i_this->mAcchCir.SetWall(40.0f, wallR);
    } else {
        i_this->mAcchCir.SetWall(50.0f, 500.0f);
    }

    actor->current.pos += i_this->m02E4;
    actor->old.pos += i_this->m02E4;
    actor->current.pos.y -= i_this->m0380;
    actor->old.pos.y -= i_this->m0380;
    i_this->mAcch.CrrPos(*dComIfG_Bgsp());
    actor->current.pos.y += i_this->m0380;
    actor->old.pos.y += i_this->m0380;
    actor->current.pos -= i_this->m02E4;
    actor->old.pos -= i_this->m02E4;
}

/* 0000573C-0000583C       .text UP_draw_SUB__FP8ph_class */
void UP_draw_SUB(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    J3DModel* model = i_this->mpPropellerMorf->getModel();

    mDoMtx_stack_c::transS(i_this->m0314.x, i_this->m0314.y, i_this->m0314.z);
    if (i_this->m0346 == 4 || i_this->m0346 == 0x33 || i_this->m0346 == 3) {
        mDoMtx_stack_c::YrotM(actor->shape_angle.y);
        mDoMtx_stack_c::XrotM(actor->shape_angle.x);
        mDoMtx_stack_c::ZrotM(actor->shape_angle.z);
        mDoMtx_stack_c::YrotM(i_this->m033A);
    } else {
        mDoMtx_stack_c::YrotM(actor->shape_angle.y);
        mDoMtx_stack_c::YrotM(i_this->m033A);
    }

    mDoMtx_stack_c::scaleM(i_this->m02FC.x, i_this->m02FC.y, i_this->m02FC.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    i_this->mpPropellerMorf->calc();
    enemy_fire(&i_this->mPropellerEnemyFire);
}

/* 0000583C-0000591C       .text DW_draw_SUB__FP8ph_class */
void DW_draw_SUB(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    J3DModel* model = i_this->mpBodyMorf->getModel();

    mDoMtx_stack_c::transS(
        actor->current.pos.x + i_this->m02E4.x,
        actor->current.pos.y + i_this->m02E4.y,
        actor->current.pos.z + i_this->m02E4.z
    );
    mDoMtx_stack_c::YrotM(actor->shape_angle.y);
    mDoMtx_stack_c::XrotM(actor->shape_angle.x);
    mDoMtx_stack_c::ZrotM(actor->shape_angle.z);
    mDoMtx_stack_c::scaleM(actor->scale.x, actor->scale.y, actor->scale.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &actor->current.pos, &actor->tevStr);
    i_this->mpBodyMorf->calc();
    enemy_fire(&i_this->mBodyEnemyFire);
}

/* 0000591C-000061A4       .text daPH_Execute__FP8ph_class */
static BOOL daPH_Execute(ph_class* i_this) {
    fopAc_ac_c* actor = i_this;
    cXyz offset;
    cXyz pos;

    if (i_this->mType == 0) {
        fopAcM_setGbaName(actor, 0x2D, 0x08, 0x25);
    } else {
        fopAcM_setGbaName(actor, 0x2D, 0x1F, 0x27);
    }

    if (enemy_ice(&i_this->mEnemyIce)) {
        i_this->mpBodyMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::now);
        i_this->mpBodyMorf->calc();
        if (i_this->m02FC.x) {
            if (!i_this->m037C) {
                if (i_this->mType == 1) {
                    i_this->m02FC.setall(i_this->m03A0);
                } else {
                    i_this->m02FC = actor->scale;
                }
                i_this->m02FC.x *= i_this->mEnemyIce.mScaleXZ;
                i_this->m02FC.y *= i_this->mEnemyIce.mScaleY;
                i_this->m02FC.z *= i_this->mEnemyIce.mScaleXZ;
                i_this->m0314 = i_this->m02C0;
                UP_draw_SUB(i_this);
            }
        }
        return TRUE;
    }

    actor->eyePos.x = i_this->m02C0.x;
    actor->eyePos.y = i_this->m02C0.y;
    actor->eyePos.z = i_this->m02C0.z;
    actor->eyePos.y -= 30.0f;
    actor->eyePos.y -= 50.0f * i_this->m039C;

    for (int i = 0; i < 5; i++) {
        if (i_this->m0364[i] != 0) {
            i_this->m0364[i]--;
        }
    }

    if (REG8_F(0)) {
        i_this->m039C = 8.0f + REG8_F(0);
        actor->scale.setall(1.0f + i_this->m039C);
    }
    if (REG8_F(1)) {
        i_this->m03A0 = 5.0f + REG8_F(1);
    }

    switch (i_this->m033F) {
    case 0:
        if (i_this->mType == 0) {
            ph_fly_move(i_this);
        } else {
            ph_fly_sea_move(i_this);
        }
        break;
    case 1:
        ph_hane_move(i_this);
        if (i_this->mType == 1) {
            sea_water_check(i_this, 1);
        }
        break;
    case 2:
        ph_bunri_move(i_this);
        break;
    case 3:
        ph_fujyou_move(i_this);
        if (i_this->mType == 1) {
            if (sea_water_check(i_this, 1)) {
                if (i_this->m0341 != 0) {
                    f32 mul = 10.0f * i_this->m039C;
                    if (actor->current.pos.y < i_this->m05BC.y - mul) {
                        actor->current.pos.y = i_this->m05BC.y - mul;
                    }
                }
            }
        } else {
            sea_water_check(i_this, 1);
        }
        break;
    case 4:
        ph_damage_dead_move(i_this);
        break;
    case 5:
        ph_wind_move(i_this);
        break;
    case 6:
        ph_water_move(i_this);
        break;
    }

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);

    {
        if (i_this->m037C) {
            if (i_this->m02FC.x) {
                f32 lim = 1000.0f;
                puropera_kaiten(i_this);
                cLib_addCalc2(&i_this->m037C, 10.0f, 0.5f, 1.0f);
                if (i_this->mType == 1) {
                    cLib_addCalc2(&i_this->m037C, 30.0f, 1.0f, 5.0f);
                    i_this->m034A = 0x2000;
                    lim = 2000.0f;
                }
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = -i_this->m037C;
                MtxPosition(&offset, &pos);
                i_this->m0314 += pos;
                i_this->m0314.y += i_this->m037C;
                if (i_this->m0314.y > actor->current.pos.y + lim) {
                    i_this->m02FC.setall(0.0f);
                    i_this->m037C = 0.0f;
                }
            }
        } else {
            i_this->m0314 = i_this->m02C0;
        }
    }

    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = actor->speedF;
    MtxPosition(&offset, &pos);
    actor->speed.x = pos.x;
    actor->speed.z = pos.z;
    if (i_this->m033F != 2 && i_this->m033F != 3 && i_this->m033F != 5 && i_this->m033F != 4) {
        actor->speed.y = pos.y;
    } else {
        actor->speed.y += actor->gravity;
        if (actor->speed.y < -20.0f) {
            actor->speed.y = -20.0f;
        }
    }

    fopAcM_posMove(actor, i_this->mStts.GetCCMoveP());

    if (i_this->m033F == 1) {
        if (i_this->m0341 != 0) {
            if (actor->current.pos.y < 40.0f + i_this->m05BC.y) {
                actor->current.pos.y = 40.0f + i_this->m05BC.y;
            }
        }
    }

    if (i_this->mType == 0) {
        cLib_addCalc2(&i_this->m02E4.x, i_this->m02F0.x, 0.3f, 3.0f);
        cLib_addCalc2(&i_this->m02E4.y, i_this->m02F0.y, 0.3f, 3.0f);
        cLib_addCalc2(&i_this->m02E4.z, i_this->m02F0.z, 0.3f, 3.0f);
    } else {
        cLib_addCalc2(&i_this->m02E4.x, i_this->m02F0.x, 1.0f, 30.0f);
        cLib_addCalc2(&i_this->m02E4.y, i_this->m02F0.y, 1.0f, 30.0f);
        cLib_addCalc2(&i_this->m02E4.z, i_this->m02F0.z, 1.0f, 30.0f);
        cLib_addCalc2(&actor->current.pos.x, actor->current.pos.x + i_this->m02E4.x, 1.0f, 30.0f);
        cLib_addCalc2(&actor->current.pos.y, actor->current.pos.y + i_this->m02E4.y, 1.0f, 30.0f);
        cLib_addCalc2(&actor->current.pos.z, actor->current.pos.z + i_this->m02E4.z, 1.0f, 30.0f);
        cLib_addCalc0(&i_this->m02E4.x, 1.0f, 30.0f);
        cLib_addCalc0(&i_this->m02E4.y, 1.0f, 30.0f);
        cLib_addCalc0(&i_this->m02E4.z, 1.0f, 30.0f);
    }

    i_this->mpBodyMorf->play(NULL, 0, 0);
    i_this->mpPropellerMorf->play(NULL, 0, 0);
    DW_draw_SUB(i_this);
    UP_draw_SUB(i_this);

    if (i_this->m033F != 4) {
        if (i_this->mType == 0) {
            i_this->mAtCyl.SetC(i_this->m02D8);
            i_this->mAtCyl.SetH(60.0f);
            i_this->mAtCyl.SetR(35.0f);
            dComIfG_Ccsp()->Set(&i_this->mAtCyl);
            i_this->mTgCyl.SetC(i_this->m02D8);
            i_this->mTgCyl.SetH(60.0f);
            i_this->mTgCyl.SetR(100.0f);
            dComIfG_Ccsp()->Set(&i_this->mTgCyl);
            i_this->mBodySph.SetC(i_this->m02CC);
            i_this->mBodySph.SetR(40.0f);
            dComIfG_Ccsp()->Set(&i_this->mBodySph);
        } else {
            offset = i_this->m02CC;
            offset.y -= 150.0f + REG8_F(10);
            i_this->mTgCyl.SetC(offset);
            i_this->mTgCyl.SetH(500.0f + REG8_F(12));
            i_this->mTgCyl.SetR(500.0f + REG8_F(13));
            dComIfG_Ccsp()->Set(&i_this->mTgCyl);
            offset = i_this->m02CC;
            offset.y -= 150.0f + REG8_F(10);
            i_this->mAtCyl.SetC(offset);
            i_this->mAtCyl.SetH(350.0f + REG8_F(14));
            i_this->mAtCyl.SetR(250.0f + REG8_F(15));
            dComIfG_Ccsp()->Set(&i_this->mAtCyl);
            i_this->mBodySph.SetC(i_this->m02CC);
            i_this->mBodySph.SetR(70.0f + REG8_F(16) + i_this->m039C * (40.0f + REG8_F(17)));
            dComIfG_Ccsp()->Set(&i_this->mBodySph);
        }
    }

    actor->attention_info.position = i_this->m02C0;
    if (i_this->mType == 1) {
        actor->attention_info.position.y -= 100.0f;
    }
    BG_check(i_this);
    return TRUE;
}

/* 000061A4-000061AC       .text daPH_IsDelete__FP8ph_class */
static BOOL daPH_IsDelete(ph_class*) {
    return TRUE;
}

/* 000061AC-00006224       .text daPH_Delete__FP8ph_class */
static BOOL daPH_Delete(ph_class* i_this) {
    if (i_this->mType == 0) {
        dComIfG_resDeleteDemo(&i_this->mPhs, "PH");
    } else {
        dComIfG_resDeleteDemo(&i_this->mPhs, "SH");
    }
    i_this->mParticleCallBack.remove();
    enemy_fire_remove(&i_this->mBodyEnemyFire);
    enemy_fire_remove(&i_this->mPropellerEnemyFire);
    return TRUE;
}

/* 00006224-000062B4       .text useArrowHeapInit__FP10fopAc_ac_c */
static BOOL useArrowHeapInit(fopAc_ac_c* a_this) {
    static Vec sph_offset[] = {{0.0f, 0.0f, 0.0f}};
    static Vec up_sph_offset[] = {{0.0f, -30.0f, 0.0f}};
    static __jnt_hit_data_c search_data_norm[] = {
        {
            /* mShapeType  */ JntHitType_SPH_e,
            /* mJointIndex */ 0,
            /* mRadius     */ 26.0f,
            /* mpOffsets   */ sph_offset,
        },
        {
            /* mShapeType  */ JntHitType_SPH_e,
            /* mJointIndex */ 1,
            /* mRadius     */ 20.0f,
            /* mpOffsets   */ up_sph_offset,
        },
    };
    static __jnt_hit_data_c search_data_sh[] = {
        {
            /* mShapeType  */ JntHitType_SPH_e,
            /* mJointIndex */ 0,
            /* mRadius     */ 250.0f,
            /* mpOffsets   */ sph_offset,
        },
        {
            /* mShapeType  */ JntHitType_SPH_e,
            /* mJointIndex */ 1,
            /* mRadius     */ 150.0f,
            /* mpOffsets   */ up_sph_offset,
        },
    };

    ph_class* i_this = (ph_class*)a_this;
    if (i_this->mType == 0) {
        i_this->mpJntHit = JntHit_create(i_this->mpBodyMorf->getModel(), search_data_norm, ARRAY_SIZE(search_data_norm));
    } else {
        i_this->mpJntHit = JntHit_create(i_this->mpBodyMorf->getModel(), search_data_sh, ARRAY_SIZE(search_data_sh));
    }

    if (i_this->mpJntHit != NULL) {
        fopAcM_SetJntHit(a_this, i_this->mpJntHit);
    } else {
        return FALSE;
    }
    return TRUE;
}

/* 000062B4-00006700       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* a_this) {
    ph_class* i_this = (ph_class*)a_this;

    if (i_this->mType == 0) {
        i_this->mpPropellerMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("PH", dRes_INDEX_PH_BDL_PHP_e),
            NULL, NULL,
            (J3DAnmTransform*)dComIfG_getObjectRes("PH", dRes_INDEX_PH_BCK_PFLY_e),
            J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x37441422
        );
    } else {
        i_this->mpPropellerMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("SH", dRes_INDEX_PH_BDL_PHP_e),
            NULL, NULL,
            (J3DAnmTransform*)dComIfG_getObjectRes("SH", dRes_INDEX_PH_BCK_PFLY_e),
            J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0, 0x11020203
        );
    }

    if (i_this->mpPropellerMorf == NULL || i_this->mpPropellerMorf->getModel() == NULL) {
        return FALSE;
    }

    i_this->mpPropellerMorf->getModel()->setUserArea((u32)i_this);
    for (u16 i = 0; i < i_this->mpPropellerMorf->getModel()->getModelData()->getJointNum(); i++) {
        i_this->mpPropellerMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_UP);
    }

    if (i_this->mType == 0) {
        i_this->mpBodyMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("PH", dRes_INDEX_PH_BDL_PHB_e),
            NULL, NULL,
            (J3DAnmTransform*)dComIfG_getObjectRes("PH", dRes_INDEX_PH_BCK_BFLY_e),
            J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x37441422
        );
    } else {
        i_this->mpBodyMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("SH", dRes_INDEX_PH_BDL_PHB_e),
            NULL, NULL,
            (J3DAnmTransform*)dComIfG_getObjectRes("SH", dRes_INDEX_PH_BCK_BFLY_e),
            J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0, 0x11020203
        );
    }

    if (i_this->mpBodyMorf == NULL || i_this->mpBodyMorf->getModel() == NULL) {
        return FALSE;
    }

    i_this->mpBodyMorf->getModel()->setUserArea((u32)i_this);
    for (u16 i = 0; i < i_this->mpBodyMorf->getModel()->getModelData()->getJointNum(); i++) {
        i_this->mpBodyMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_DW);
    }

    if (!i_this->mPropellerInvisibleModel.create(i_this->mpPropellerMorf->getModel())) {
        return FALSE;
    }
    if (!i_this->mBodyInvisibleModel.create(i_this->mpBodyMorf->getModel())) {
        return FALSE;
    }

    u8 result = useArrowHeapInit(a_this);
    return result ? TRUE : FALSE;
}

/* 00006700-00006D1C       .text daPH_Create__FP10fopAc_ac_c */
static cPhs_State daPH_Create(fopAc_ac_c* a_this) {
    static dCcD_SrcCyl at_hit_puropera_co_cyl = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
            /* SrcObjAt  Atp     */ 1,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ 0,
            /* SrcObjTg  SPrm    */ 0,
#if VERSION == VERSION_DEMO
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsPlayer_e,
#else
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
#endif
            /* SrcGObjAt Se      */ dCcG_SE_METAL,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK8,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ 0,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGCylS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
            /* Height */ 0.0f,
        }},
    };
    static dCcD_SrcCyl tg_hit_puropera_co_cyl = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_NoTgHitInfSet_e,
            /* SrcObjTg  Type    */ AT_TYPE_SWORD | AT_TYPE_WIND,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsPlayer_e,
            /* SrcGObjAt Se      */ dCcG_SE_METAL,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK1,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ dCcG_SE_METAL,
            /* SrcGObjTg HitMark */ dCcg_TgHitMark_Purple_e,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGCylS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
            /* Height */ 0.0f,
        }},
    };
    static dCcD_SrcSph body_co_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_UNK2000,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
#if VERSION == VERSION_DEMO
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_GrpAll_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsEnemy_e,
#else
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
#endif
            /* SrcGObjAt Se      */ dCcG_SE_UNK6,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e | dCcG_TgSPrm_NoHitMark_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
        }},
    };
    static u8 fire_j[] = {
        PHB_JNT_HARA_e,
        PHB_JNT_CONNECTA_e,
        PHB_JNT_HIGEA_e,
        PHB_JNT_HIGEB_e,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };
    static f32 fire_sc[] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };
    static u8 fire_j2[] = {
        PHP_JNT_HANE_CENT_e,
        PHP_JNT_HANEA01_e,
        PHP_JNT_HANEA03_e,
        PHP_JNT_HANEA04_e,
        PHP_JNT_HANEB01_e,
        PHP_JNT_HANEB03_e,
        PHP_JNT_HANEB04_e,
        PHP_JNT_HANEC01_e,
        PHP_JNT_HANEC03_e,
        PHP_JNT_HANEC04_e,
    };
    static f32 fire_sc2[] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    ph_class* i_this = (ph_class*)a_this;
    u32 heapSize = 0x2740;

    i_this->mType = fopAcM_GetParam(a_this);
    if (i_this->mType == 0xFF) {
        i_this->mType = 0;
    }
    if (REG8_S(9) != 0) {
        i_this->mType = 1;
    }

    cPhs_State phase;
    if (i_this->mType == 0) {
        phase = dComIfG_resLoad(&i_this->mPhs, "PH");
    } else {
        phase = dComIfG_resLoad(&i_this->mPhs, "SH");
        heapSize = DEMO_SELECT(0, 0x3E40);
    }

    if (phase == cPhs_COMPLEATE_e) {
        fopAcM_ct(i_this, ph_class);

        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, heapSize)) {
            return cPhs_ERROR_e;
        }

    u32 param = fopAcM_GetParam(a_this);
    u8 prmXZ = (param >> 8) & 0xFF;
    u8 prmY = (param >> 16) & 0xFF;

    if (i_this->mType == 0) {
        i_this->m0390 = 1000.0f;
        i_this->m0394 = 500.0f;
    } else {
        i_this->m0390 = 12000.0f;
        i_this->m0394 = 6000.0f;
        i_this->m039C = 8.0f;
        i_this->m03A0 = 5.0f;
        f32 scl = 1.0f + i_this->m039C;
        a_this->scale.setall(scl);
    }

    if (prmXZ != 0xFF) {
        i_this->m0390 = 100.0f * (f32)prmXZ;
    }
    if (prmY != 0xFF) {
        i_this->m0394 = 100.0f * (f32)prmY;
    }

    i_this->m038C = 800.0f + i_this->m0390;
    a_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;

    if (i_this->mType == 0) {
        a_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("p_hat", 0);
        a_this->attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 3;
        a_this->stealItemLeft = 1;
        i_this->m0344 = a_this->stealItemLeft;
    } else {
        a_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("sea_hat", 0);
        a_this->attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 0x2A;
        a_this->gbaName = 0x1F;
    }

    fopAcM_SetMtx(a_this, i_this->mpBodyMorf->getModel()->getBaseTRMtx());
    i_this->mAcch.Set(
        fopAcM_GetPosition_p(a_this), fopAcM_GetOldPosition_p(a_this), a_this, 1,
        &i_this->mAcchCir, fopAcM_GetSpeed_p(a_this), NULL, NULL
    );
    i_this->mStts.Init(0x50, 4, a_this);
    a_this->max_health = 1;
    a_this->health = 1;
    i_this->m02FC = a_this->scale;

    i_this->mBodySph.Set(body_co_sph_src);
    i_this->mBodySph.SetStts(&i_this->mStts);
    i_this->mBodySph.OffCoSetBit();
    i_this->mAtCyl.Set(at_hit_puropera_co_cyl);
    i_this->mAtCyl.SetStts(&i_this->mStts);
    i_this->mTgCyl.Set(tg_hit_puropera_co_cyl);
    i_this->mTgCyl.SetStts(&i_this->mStts);

    i_this->m0356[0] = (fopAcM_GetID(a_this) & 0xFF) << 13;
    i_this->m0356[1] = (fopAcM_GetID(a_this) & 0xFF) << 13;
    i_this->m0356[2] = (fopAcM_GetID(a_this) & 0xFF) << 13;
    i_this->m0356[3] = (fopAcM_GetID(a_this) & 0xFF) << 13;

    i_this->mEnemyIce.mpActor = a_this;
    i_this->mEnemyIce.mWallRadius = 40.0f;
    i_this->mEnemyIce.mCylHeight = 40.0f;
    i_this->mEnemyIce.m1B0 = 1;

    i_this->mBodyEnemyFire.mpMcaMorf = i_this->mpBodyMorf;
    i_this->mBodyEnemyFire.mpActor = a_this;
    for (int i = 0; i < 10; i++) {
        i_this->mBodyEnemyFire.mFlameJntIdxs[i] = fire_j[i];
        i_this->mBodyEnemyFire.mParticleScale[i] = fire_sc[i];
        i_this->mBodyEnemyFire.mParticleScale[i] += i_this->m039C;
    }

    i_this->mPropellerEnemyFire.mpMcaMorf = i_this->mpPropellerMorf;
    i_this->mPropellerEnemyFire.mpActor = a_this;
    for (int i = 0; i < 10; i++) {
        i_this->mPropellerEnemyFire.mFlameJntIdxs[i] = fire_j2[i];
        i_this->mPropellerEnemyFire.mParticleScale[i] = fire_sc2[i];
        i_this->mPropellerEnemyFire.mParticleScale[i] += i_this->m039C;
    }

    i_this->m0380 = 30.0f;
    a_this->gravity = -3.0f;
    BG_check(i_this);
    a_this->gravity = 0.0f;
    i_this->m032C = a_this->current.pos;

    anm_init(i_this, dRes_INDEX_PH_BCK_PFLY_e, 5.0f, 2, 1.0f, -1, 0);
    anm_init(i_this, dRes_INDEX_PH_BCK_BFLY_e, 5.0f, 2, 1.0f, -1, 1);

    if (i_this->mType == 1) {
        i_this->mAtCyl.SetAtSpl(dCcG_At_Spl_UNK9);
#if VERSION == VERSION_DEMO
        fopAcM_setCullSizeBox(a_this, -500.0f, -500.0f, -500.0f, 500.0f, 500.0f, 500.0f);
#else
        fopAcM_setCullSizeBox(a_this, -200.0f, -200.0f, -200.0f, 200.0f, 200.0f, 200.0f);
        a_this->cullSizeFar = 10000.0f + REG8_F(8) / mDoLib_clipper::mSystemFar;
#endif
        i_this->m033F = 0;
        i_this->m0346 = 0;
        a_this->actor_status &= ~fopAcStts_UNK80000_e;
        i_this->m0341 = 1;
        i_this->m0380 = 100.0f;
        i_this->m02FC.setall(i_this->m03A0);
    } else {
        fopAcM_setCullSizeBox(a_this, -80.0f, -80.0f, -80.0f, 80.0f, 80.0f, 80.0f);
        i_this->mBodySph.SetTgType(
            AT_TYPE_BOMB | AT_TYPE_BOOMERANG | AT_TYPE_NORMAL_ARROW | AT_TYPE_HOOKSHOT | AT_TYPE_SKULL_HAMMER |
            AT_TYPE_FIRE_ARROW | AT_TYPE_ICE_ARROW | AT_TYPE_LIGHT_ARROW | AT_TYPE_GRAPPLING_HOOK
        );
        if (i_this->mAcch.ChkGroundHit()) {
            i_this->m02FC.setall(0.0f);
            i_this->m033F = 3;
            i_this->m0346 = 0x1E;
        }
    }

    i_this->mpBodyMorf->play(NULL, 0, 0);
    i_this->mpPropellerMorf->play(NULL, 0, 0);
    DW_draw_SUB(i_this);
    i_this->m0314 = i_this->m02C0;
    UP_draw_SUB(i_this);
    }

    return phase;
}

static actor_method_class l_daPH_Method = {
    (process_method_func)daPH_Create,
    (process_method_func)daPH_Delete,
    (process_method_func)daPH_Execute,
    (process_method_func)daPH_IsDelete,
    (process_method_func)daPH_Draw,
};

actor_process_profile_definition g_profile_PH = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_PH_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(ph_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_PH_e,
    /* Actor SubMtd */ &l_daPH_Method,
    /* Status       */ fopAcStts_SHOWMAP_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e | fopAcStts_UNK80000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
