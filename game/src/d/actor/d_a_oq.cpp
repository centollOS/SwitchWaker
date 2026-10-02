/**
 * d_a_oq.cpp
 * Enemy - Octorok
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_oq.h"
#include "d/actor/d_a_bomb.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_sea.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_jnt_hit.h"
#include "d/d_kankyo.h"
#include "d/d_material.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_kankyo_mng.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/Oq.h"

static s16 hazure_shoot_timer;

static csXyz roll_dt[] = {
    csXyz(0, 0x1000, 0x1000),
    csXyz(0, 0x1000, -0x1000),
    csXyz(0, -0x1000, 0x1000),
    csXyz(0, -0x1000, -0x1000),
};

/* 000000EC-00000208       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        oq_class* i_this = (oq_class*)model->getUserArea();
        if (i_this != NULL) {
            cXyz offset;
            if (jntNo == OQ_JNT_KUTI_e || jntNo == OQ_JNT_KUTI2_e) {
                MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
            }
            if (jntNo == OQ_JNT_KUTI2_e) {
                offset.x = 40.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mMouthPos2);
            }
            if (jntNo == OQ_JNT_KUTI2_e) {
                offset.x = 20.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mMouthPos);
            }
            if (jntNo == OQ_JNT_KUTI_e || jntNo == OQ_JNT_KUTI2_e) {
                model->setAnmMtx(jntNo, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }
    return TRUE;
}

/* 00000208-00000310       .text draw_SUB__FP8oq_class */
void draw_SUB(oq_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    mDoMtx_stack_c::transS(i_this->current.pos.x, i_this->current.pos.y + i_this->mDrawOffsetY, i_this->current.pos.z);
    mDoMtx_stack_c::YrotM(i_this->shape_angle.y);
    mDoMtx_stack_c::XrotM(i_this->shape_angle.x);
    mDoMtx_stack_c::ZrotM(i_this->shape_angle.z);
    mDoMtx_stack_c::scaleM(i_this->scale.x, i_this->scale.y, i_this->scale.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());

    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        i_this->mpMorf->calc();
        enemy_fire(&i_this->mEnemyFire);
    }

    if (i_this->mType != 3 && i_this->mType != 2) {
        g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &i_this->current.pos, &i_this->tevStr);
    }
}

/* 00000310-000004DC       .text daOQ_Draw__FP8oq_class */
static BOOL daOQ_Draw(oq_class* i_this) {
    if (i_this->mType == 3 || i_this->mType == 2) {
        return TRUE;
    }

    J3DModel* model = i_this->mpMorf->getModel();
    g_env_light.setLightTevColorType(model, &i_this->tevStr);

#if VERSION == VERSION_DEMO
    dSnap_RegistFig(DSNAP_TYPE_UNKBA, i_this, 1.0f, 1.0f, 1.0f);
#else
    if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        dSnap_RegistFig(DSNAP_TYPE_UNKBA, i_this, 2.75f, 2.75f, 2.75f);
    }
    if (i_this->mType == 0) {
        dSnap_RegistFig(DSNAP_TYPE_UNKBA, i_this, 1.0f, 1.0f, 1.0f);
    }
#endif

    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        if (i_this->mEnemyIce.mFreezeTimer > 20) {
            dMat_control_c::iceEntryDL(i_this->mpMorf, -1, NULL);
            return TRUE;
        }
    }
    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        if (i_this->mType != 0) {
            i_this->mpBrk->entry(model->getModelData());
            i_this->mpBrk->setFrame(i_this->mBrkFrame);
        }
        i_this->mpMorf->entryDL();
        if (i_this->mType != 0) {
            i_this->mpBrk->remove(model->getModelData());
        }
    } else if (i_this->mType == 6) {
        i_this->mpMorf->updateDL();
        dComIfGd_setSimpleShadow2(
            &i_this->current.pos,
            i_this->mAcch.GetGroundH(),
            40.0f,
            i_this->mAcch.m_gnd,
            0,
            1.0f,
            dDlst_shadowControl_c::getSimpleTex()
        );
    }
    return TRUE;
}

/* 000004DC-00000608       .text anm_init__FP8oq_classifUcfi */
void anm_init(oq_class* i_this, int bckFileIdx, float morf, unsigned char loopMode, float speed, int soundFileIdx) {
    i_this->mBckIdx = bckFileIdx;
    if (soundFileIdx >= 0) {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("OQ", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, dComIfG_getObjectRes("OQ", soundFileIdx));
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("OQ", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
}

/* 00000608-00000D50       .text body_atari_check__FP8oq_class */
BOOL body_atari_check(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);

    i_this->mStts.Move();
    if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        if (i_this->mBckIdx == dRes_INDEX_OQ_BCK_SHIP_HIT_e) {
            if (i_this->mpMorf->isStop()) {
                if (i_this->mMode != 0xA && i_this->mMode != 0xB) {
                    i_this->mAction = 0xA;
                    i_this->mMode = 0xA;
                }
            }
        } else if (i_this->mBodyAtCyl.ChkAtHit()) {
            fopAc_ac_c* atHit = i_this->mBodyAtCyl.GetAtHitAc();
            if (atHit != NULL && (atHit == (fopAc_ac_c*)dComIfGp_getShipActor() || atHit == player)) {
                if (i_this->mBckIdx != dRes_INDEX_OQ_BCK_SHIP_HIT_e) {
                    i_this->m30C = 300.0f + REG8_F(17);
                    anm_init(i_this, dRes_INDEX_OQ_BCK_SHIP_HIT_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                }
            }
        }
        cLib_addCalc2(&i_this->m30C, 175.0f + REG8_F(12), 1.0f, 5.0f);
    }

    i_this->mHitType = 0;
    if (i_this->mBodyCoCyl.ChkTgHit()) {
        cCcD_Obj* hitObj = i_this->mBodyCoCyl.GetTgHitObj();
        if (hitObj == NULL) {
            return FALSE;
        }

    CcAtInfo atInfo;
    atInfo.pParticlePos = NULL;
    u8 skipAtCheck = 0;
    switch (hitObj->GetAtType()) {
    case AT_TYPE_GRAPPLING_HOOK:
        if (actor->stealItemLeft > 0) {
            s8 oldHealth = actor->health;
            actor->health = 10;
            atInfo.mpObj = i_this->mBodyCoCyl.GetTgHitObj();
            cc_at_check(actor, &atInfo);
            actor->health = oldHealth;
        }
        dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &actor->attention_info.position);
        skipAtCheck = 1;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x20);
        break;
    case AT_TYPE_SWORD: {
        fopAcM_seStart(actor, JA_SE_LK_SW_HIT_S, 0x20);
        i_this->mHitType = 0;
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
        break;
    sword_smash:
        i_this->mHitType = 1;
        break;
    }
    case AT_TYPE_WIND:
        skipAtCheck = 1;
        i_this->mHitType = 3;
        break;
    case AT_TYPE_BOOMERANG:
        i_this->mHitType = 4;
        // fallthrough
    case AT_TYPE_BOKO_STICK:
        fopAcM_seStart(actor, JA_SE_LK_W_WEP_HIT, 0x20);
        break;
    case AT_TYPE_SKULL_HAMMER:
        fopAcM_seStart(actor, JA_SE_LK_HAMMER_HIT, 0x20);
        i_this->mHitType = 7;
        if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
            i_this->mHitType = 8;
        }
        break;
    case AT_TYPE_BOMB:
        i_this->mHitType = 6;
        break;
    case AT_TYPE_FIRE:
    case AT_TYPE_FIRE_ARROW:
        i_this->mEnemyFire.mFireDuration = 100;
        i_this->mHitType = 5;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x20);
        break;
    case AT_TYPE_ICE_ARROW:
        i_this->mEnemyIce.mFreezeDuration = 200;
        skipAtCheck = 1;
        i_this->mHitType = 5;
        actor->attention_info.flags = 0;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x20);
        break;
    case AT_TYPE_LIGHT_ARROW:
        i_this->mEnemyIce.mLightShrinkTimer = 1;
        i_this->mEnemyIce.mParticleScale = 1.0f;
        i_this->mEnemyIce.mYOffset = 80.0f;
        actor->attention_info.flags = 0;
        // fallthrough
    case AT_TYPE_NORMAL_ARROW:
        i_this->mHitType = 5;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x20);
        break;
    default:
        i_this->mHitType = 0;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x20);
        break;
    }

    if (skipAtCheck == 0) {
        cXyz hitPos = *i_this->mBodyCoCyl.GetTgHitPosP();
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hitPos);
        cXyz scale;
        scale.setall(2.0f);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hitPos, &player->shape_angle, &scale);
        fopAcM_monsSeStart(actor, JA_SE_CV_OQ_DIE, 0);
        CcAtInfo atInfo2;
        atInfo2.pParticlePos = NULL;
        atInfo2.mpObj = i_this->mBodyCoCyl.GetTgHitObj();
        cc_at_check(actor, &atInfo2);
        i_this->mAction = 0x14;
        i_this->mMode = 0x1E;
    }
        return TRUE;
    }
    return FALSE;
}

/* 00000D8C-00000EC4       .text BG_check__FP8oq_class */
void BG_check(oq_class* i_this) {
    u8 do_crr = 0;
    if (i_this->mType == 0) {
        i_this->mAcchCir.SetWall(80.0f + REG8_F(10), 100.0f + REG8_F(11));
        do_crr = 1;
    }
    if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
#if VERSION == VERSION_DEMO
        i_this->mAcchCir.SetWall(40.0f, 90.0f);
#else
        i_this->mAcchCir.SetWall(40.0f + REG8_F(10), 90.0f + REG8_F(11));
#endif
        do_crr = 0;
    } else if (i_this->mType == 6) {
        i_this->mAcchCir.SetWall(40.0f, 30.0f);
        do_crr = 1;
    }
    if (do_crr) {
        i_this->current.pos.y -= i_this->mCorrectionOffsetY;
        i_this->old.pos.y -= i_this->mCorrectionOffsetY;
        i_this->mAcch.CrrPos(*dComIfG_Bgsp());
        i_this->current.pos.y += i_this->mCorrectionOffsetY;
        i_this->old.pos.y += i_this->mCorrectionOffsetY;
    }
}

/* 00000EC4-000011D8       .text Line_check__FP8oq_class4cXyz */
BOOL Line_check(oq_class* i_this, cXyz destPos) {
    fopAc_ac_c* actor = i_this;
    dBgS_LinChk linChk;
    cXyz start = actor->current.pos;
    start.y += 40.0f;
    linChk.Set(&start, &destPos, actor);
    if (dComIfG_Bgsp()->LineCross(&linChk)) {
        return TRUE;
    }
    return FALSE;
}

/* 00001610-000016AC       .text shibuki_set__FP8oq_class4cXyzf */
void shibuki_set(oq_class* i_this, cXyz pos, float scale) {
    if (daSea_ChkArea(i_this->current.pos.x, i_this->current.pos.z)) {
        pos.y = daSea_calcWave(i_this->current.pos.x, i_this->current.pos.z);
    } else if (i_this->mAcch.MaskWaterIn()) {
        pos.y = i_this->mAcch.m_wtr.GetHeight();
    }

    fopKyM_createWpillar(&pos, scale, 1.0f, 0);
}

/* 000016AC-00001930       .text sea_water_check__FP8oq_class */
BOOL sea_water_check(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;
    u8 hit = 0;
    f32 bob;
    i_this->mRipplePos = actor->current.pos;
#if VERSION > VERSION_DEMO
    i_this->mRipplePos.y = i_this->mHomePos.y;
#endif
    actor->gravity = -3.0f;

    if (daSea_ChkArea(actor->current.pos.x, actor->current.pos.z)) {
        f32 waveY = daSea_calcWave(actor->current.pos.x, actor->current.pos.z);
        i_this->mRipplePos.y = waveY;
        if (actor->current.pos.y < 40.0f + waveY) {
            actor->gravity = 0.0f;
            i_this->mBobAngle += 0x800;
            bob = 45.0f + 15.0f * cM_ssin(i_this->mBobAngle);
            cLib_addCalc2(&actor->current.pos.y, waveY - bob, 1.0f, 30.0f);
            hit = 1;
        }
    } else if (i_this->mAcch.MaskWaterIn()) {
        hit = 2;
        f32 f3 = 20.0f + i_this->mAcch.m_wtr.GetHeight();
        i_this->mRipplePos.y = i_this->mAcch.m_wtr.GetHeight();
        actor->gravity = -3.0f;
        if (actor->current.pos.y < f3) {
            actor->gravity = 0.0f;
            i_this->mBobAngle += 0x800;
            bob = 45.0f + 15.0f * cM_ssin(i_this->mBobAngle);
            cLib_addCalc2(&actor->current.pos.y, f3 - bob, 1.0f, 30.0f);
            hit = 1;
        }
    }
#if VERSION > VERSION_DEMO
    else if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        actor->gravity = 0.0f;
    }
#endif

    if (hit) {
        if (hit == 1) {
            if (i_this->mMadeWaterSplash == 0) {
                if (i_this->mType != 5) {
                    cXyz scale(1.0f, 1.0f, 1.0f);
                    i_this->mRippleCb.end();
                    dComIfGp_particle_setShipTail(dPa_name::ID_AK_JN_HAMON00, &i_this->mRipplePos, NULL, &scale, 0xFF, &i_this->mRippleCb);
                    i_this->mRippleCb.setRate(0.0f);
                }
                i_this->mMadeWaterSplash = 1;
                shibuki_set(i_this, i_this->current.pos, i_this->mSplashScale);
            }
        }
        return TRUE;
    }
    return FALSE;
}

/* 00001930-0000198C       .text search_y_check__FP8oq_classs */
void search_y_check(oq_class* i_this, short maxStep) {
    s16 step = maxStep;
    cLib_addCalcAngleS2(&i_this->current.angle.y, i_this->mTargetAngleY, 1, step);
    cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 1, step);
}

/* 0000198C-00001B24       .text moguru_check__FP8oq_class */
BOOL moguru_check(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif
    f32 range;

    switch (i_this->mType) {
    case 0:
        if (i_this->m2C2 == 0) {
            range = 2000.0f;
        } else {
            range = 5000.0f;
        }
        if (i_this->mAcch.ChkWallHit()) {
            fopAcM_seStart(actor, JA_SE_LK_LAST_HIT, 0);
            fopAcM_monsSeStart(actor, JA_SE_CV_OQ_DIE, 0);
            i_this->mAction = 0x14;
            i_this->mMode = 0x1E;
            return TRUE;
        }
        break;
    case 1:
    case 4:
        range = 5000.0f;
        break;
    case 5:
        range = 65.0f + REG12_F(13);
        range *= 100.0f;
        break;
    }
#if VERSION == VERSION_DEMO
    if (i_this->m2C8 != 0 || fopAcM_searchActorDistance(actor, player) > range) {
#else
    if (i_this->m2C8 != 0 || fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) > range) {
#endif
        i_this->mMode = 0xA;
        return TRUE;
    }
    return FALSE;
}

/* 00001B24-00002270       .text action_dousa__FP8oq_class */
void action_dousa(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    u8 inRange = 0;
    f32 range;

    switch (i_this->mMode) {
        case 0:
            for (int i = 0; i < 6; i++) {
                i_this->mTimers[6 + i] = 0;
            }
            i_this->mTimers[4] = (s16)(300.0f + REG8_F(11));
            i_this->mMadeWaterSplash = 1;
            i_this->mSplashScale = 2.75f + REG8_F(19);
            i_this->mMode++;
            // fallthrough
        case 1:
            switch (i_this->mType) {
            case 0:
                i_this->mSplashScale = 1.0f;
                i_this->mTimers[4] = (s16)(300.0f + REG8_F(11));
                if (i_this->m2C2 == 0) {
                    range = 2000.0f;
                } else {
                    range = 5000.0f;
                }
                break;
            case 1:
                range = 5000.0f;
                i_this->mTimers[4] = (s16)(300.0f + REG8_F(11));
                break;
            case 5:
                range = 60.0f + REG12_F(11);
                range *= 100.0f;
                break;
            case 4:
                inRange = 1;
                break;
            }
            if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range) {
                inRange = 1;
                if (i_this->mType == 0) {
                    range = 400.0f;
                    if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range) {
                        break;
                    }
                    f32 roofY = i_this->mAcch.GetRoofHeight();
                    if (roofY != G_CM3D_F_INF) {
                        if (roofY < 150.0f + REG8_F(8)) {
                            break;
                        }
                    }
                }
            }
            if (i_this->mTimers[4] == 0 || inRange) {
                i_this->mMadeWaterSplash = 0;
                i_this->mBodyCoCyl.OnTgSetBit();
                i_this->mBodyCoCyl.OnCoSetBit();
                fopAcM_OnStatus(actor, fopAcStts_SHOWMAP_e);
                actor->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
                anm_init(i_this, dRes_INDEX_OQ_BCK_START_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                fopAcM_seStart(actor, JA_SE_CM_OQ_OUT_WATER, 0);
                i_this->mMode++;
            }
            if (i_this->m2C8 != 0) {
                fopAcM_delete(actor);
            }
            break;
        case 2:
            cLib_addCalc2(&actor->scale.x, i_this->mSplashScale, 1.0f, 0.2f);
            actor->scale.y = actor->scale.z = actor->scale.x;
            if (i_this->mpMorf->isStop()) {
                i_this->mBodyAtCyl.OnAtSetBit();
                i_this->mBodyAtCyl.OnAtHitBit();
                f32 splash = i_this->mSplashScale;
                actor->scale.x = splash;
                actor->scale.y = splash;
                actor->scale.z = splash;
                if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_UMI_NEW_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                } else {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_NOM_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                }
                i_this->mMode++;
            }
            break;
        case 3:
            if (moguru_check(i_this)) {
                return;
            }
            switch (i_this->mType) {
            case 4:
                if (i_this->mTimers[0] == 0) {
                    i_this->mAction = 0xB;
                    i_this->mMode = 0x14;
                }
                break;
            case 0:
                range = 400.0f;
                if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range) {
                    if (i_this->mAcch.ChkWaterIn()) {
                        i_this->mMode = 0xA;
                    }
                } else {
                    range = 4500.0f;
                    if (i_this->m2C2 == 0) {
                        range = 1800.0f;
                    }
                    if (i_this->mTimers[0] == 0) {
                        if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range) {
                            f32 dy = actor->current.pos.y - player->current.pos.y;
                            if (std::fabsf(dy) < 200.0f) {
                                i_this->mAction = 0xB;
                                i_this->mMode = 0x14;
                            }
                        }
                    }
                }
                break;
            case 1:
                range = 4500.0f;
                break;
            case 5:
                range = 55.0f + REG12_F(12);
                range *= 100.0f;
                break;
            }
            if (i_this->mType == 1 || i_this->mType == 5) {
                f32 range2 = 1700.0f;
                if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range2) {
                    if (i_this->mBckIdx != dRes_INDEX_OQ_BCK_IKAKU_e && i_this->mBckIdx != dRes_INDEX_OQ_BCK_SHIP_HIT_e) {
                        anm_init(i_this, dRes_INDEX_OQ_BCK_IKAKU_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                    }
                } else if (i_this->mTimers[0] == 0) {
                    if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range) {
                        i_this->mAction = 0xB;
                        i_this->mMode = 0x14;
                    }
                }
            }
            break;
        case 0xA:
            i_this->mBodyCoCyl.OffAtSetBit();
            i_this->mBodyAtCyl.OffAtSetBit();
            i_this->mBodyCoCyl.OffTgSetBit();
            i_this->mBodyCoCyl.OffCoSetBit();
            i_this->mBodyCoCyl.ClrTgHit();
            i_this->mMadeWaterSplash = 0;
            actor->attention_info.flags = 0;
            fopAcM_seStart(actor, JA_SE_CM_OQ_IN_WATER, 0);
            anm_init(i_this, dRes_INDEX_OQ_BCK_MOGURU_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode++;
            // fallthrough
        case 0xB:
            cLib_addCalc2(&i_this->mDrawOffsetY, -(600.0f + REG8_F(6)), 1.0f, 50.0f + REG8_F(7));
            if (i_this->mDrawOffsetY < -(600.0f + REG8_F(6) - 1.0f)) {
                actor->scale.setall(0.0f);
                fopAcM_OffStatus(actor, fopAcStts_SHOWMAP_e);
                if (i_this->mType == 4 || i_this->mType == 5) {
                    fopAcM_delete(actor);
                } else {
                    i_this->mDrawOffsetY = 0.0f;
                    i_this->mMode = 0;
                }
            }
            break;
        }

    i_this->mTargetAngleY = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    if (i_this->mMode >= 3) {
        body_atari_check(i_this);
    }
    sea_water_check(i_this);
    search_y_check(i_this, 0x500);
}

/* 00002270-00002BEC       .text action_kougeki__FP8oq_class */
void action_kougeki(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    switch (i_this->mMode) {
        case 0x14:
            for (int i = 0; i < 6; i++) {
                i_this->mTimers[6 + i] = 0;
            }
            search_y_check(i_this, 0x1000);
            i_this->mMode = 0x16;
            fopAcM_seStart(actor, JA_SE_CM_OQ_JUMP, 0);
            if (i_this->mMadeWaterSplash != 0) {
                if (actor->current.pos.y > 100.0f + player->current.pos.y) {
                    shibuki_set(i_this, actor->current.pos, 1.5f);
                }
                i_this->mMadeWaterSplash = 0;
                i_this->mRippleCb.end();
            }
            if (i_this->m2C2 == 0) {
                if (actor->current.pos.y <= 100.0f + player->current.pos.y) {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_AATTACK1_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                    actor->gravity = 0.0f;
                    i_this->mTimers[6] = 1;
                    i_this->mMode = 0x15;
                }
            } else {
                actor->gravity = -3.0f;
                actor->speed.y = 40.0f;
                i_this->mTimers[6] = 1;
            }
            break;
        case 0x15:
            search_y_check(i_this, 0x1000);
            cLib_addCalc2(&actor->current.pos.y, 30.0f + player->current.pos.y, 1.0f, 30.0f);
            if (std::fabsf(actor->current.pos.y - (30.0f + player->current.pos.y)) > 2.0f) {
                break;
            }
            i_this->mMode++;
            // fallthrough
        case 0x16:
            anm_init(i_this, dRes_INDEX_OQ_BCK_AATTACK2_FUKU_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode++;
            break;
        case 0x17:
            if (i_this->mpMorf->checkFrame(1.0f)) {
                if (REG8_S(5) == 0 && i_this->m2C2 == 0) {
                    u32 params = ((u32)i_this->m2C2 << 8) | 6;
                    cXyz scale;
                    scale.setall(0.5f);
                    fopAcM_create(fpcNm_OQ_e, params, &i_this->mMouthPos, fopAcM_GetRoomNo(actor), &actor->current.angle, &scale, 0, NULL);
                    csXyz angle = actor->current.angle;
                    angle.y += 0x4000;
                    dComIfGp_particle_set(dPa_name::ID_AK_JN_OQROCKATTACK00, &i_this->mMouthPos2, &angle);
                    fopAcM_seStart(actor, JA_SE_CM_OQ_SPIT_ROCK, 0);
                } else if (REG8_S(5) == 0) {
#if VERSION == VERSION_DEMO
                    cXyz offset;
                    cXyz vel;
#endif
                    cXyz scale;
                    scale.setall(2.25f + REG8_F(18));
                    daBomb_c* bomb = (daBomb_c*)fopAcM_fastCreate(
                        fpcNm_BOMB_e,
                        daBomb_c::prm_make(daBomb_c::STATE_4, true, true),
                        &i_this->mMouthPos,
                        fopAcM_GetRoomNo(actor),
                        &actor->current.angle,
                        &scale,
                        -1,
                        NULL
                    );
                    f32 dx = player->current.pos.x - actor->current.pos.x;
                    f32 dy = player->current.pos.y - actor->current.pos.y;
                    f32 dz = player->current.pos.z - actor->current.pos.z;
                    f32 distXZ = std::sqrtf(dx * dx + dz * dz);
                    s16 pitch = -cM_atan2s(dy, distXZ);
#if VERSION == VERSION_DEMO
                    s16 yaw = actor->current.angle.y;
                    if (!dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
                        yaw += (s16)cM_rndFX(8000.0f);
                    }
                    mDoMtx_YrotS(*calc_mtx, yaw);
                    mDoMtx_XrotM(*calc_mtx, pitch);
                    offset.x = 0.0f;
                    offset.y = 0.0f;
                    offset.z = 60.0f + REG12_F(15);
                    MtxPosition(&offset, &vel);
                    if (vel.y > 0.0f) {
                        vel.y = -3.0f;
                    }
                    bomb->speedF = 40.0f;
                    bomb->gravity = vel.y;
                    bomb->speed.y = 40.0f + REG12_F(16);
#else
                    if (!dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
                        cM_rndFX(8000.0f);
                    }
                    bomb->current.angle.x = pitch + cM_rndFX(3000.0f + REG12_F(15));
                    bomb->speedF = 40.0f + REG12_F(16);
                    bomb->speed.y = 70.0f + cM_rndF(30.0f + REG12_F(17));
#endif
                    fopAcM_seStart(actor, JA_SE_CM_OQ_SPIT_BOMB, 0);
                }
                i_this->mMode++;
            }
            break;
        case 0x18:
            if (i_this->m2C2 == 0) {
                if (i_this->mTimers[6] != 0) {
                    actor->gravity = -3.0f;
                    actor->speed.y = 30.0f;
                }
            }
            i_this->mMadeWaterSplash = 0;
            i_this->mMode++;
            // fallthrough
        case 0x19:
            if (i_this->mBckIdx == dRes_INDEX_OQ_BCK_AATTACK2_FUKU_e) {
                if (!i_this->mpMorf->isStop()) {
                    break;
                }
                if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_UMI_NEW_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                } else {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_NOM_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                }
            }
            if (i_this->mTimers[6] != 0) {
                if (sea_water_check(i_this)) {
                    if (i_this->mMadeWaterSplash != 0) {
                        anm_init(i_this, dRes_INDEX_OQ_BCK_AATTACK3_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                        i_this->mMode = 0x1A;
                        fopAcM_seStart(actor, JA_SE_CM_OQ_LANDING, 0);
                    }
                } else if (i_this->mAcch.ChkGroundHit()) {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_AATTACK3_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                    i_this->mMode = 0x1A;
                }
            } else {
                if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_UMI_NEW_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                } else {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_NOM_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                }
                i_this->mMode = 0x1A;
            }
            break;
        case 0x1A:
            sea_water_check(i_this);
            if (i_this->mTimers[6] == 0 || i_this->mpMorf->isStop()) {
                if (i_this->m2C2 == 0) {
                    i_this->mTimers[0] = 0x3C;
                } else {
                    i_this->mTimers[0] = 0x64;
                }
                if (i_this->mType == 1 || i_this->mType == 5) {
                    i_this->mTimers[0] = 0x64;
                }
                i_this->mTimers[0] = i_this->mTimers[0] + cM_rndF(i_this->mTimers[0]);
                actor->speed.setall(0.0f);
                if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
                    if (i_this->mBckIdx != dRes_INDEX_OQ_BCK_UMI_NEW_WAIT_e) {
                        anm_init(i_this, dRes_INDEX_OQ_BCK_UMI_NEW_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                    }
                } else if (i_this->mBckIdx != dRes_INDEX_OQ_BCK_NOM_WAIT_e) {
                    anm_init(i_this, dRes_INDEX_OQ_BCK_NOM_WAIT_e, 15.0f + REG8_F(3), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                }
                i_this->mAction = 0xA;
                i_this->mMode = 3;
            }
            break;
        }

    if (!body_atari_check(i_this)) {
        if (i_this->mMode >= 0x1A) {
            f32 range = 400.0f;
            if (i_this->mType == 0 && fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < range) {
                i_this->mAction = 0xA;
                i_this->mMode = 0xA;
            }
        }
    }
}

/* 00002BEC-000032A4       .text action_tama_shoot__FP8oq_class */
void action_tama_shoot(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;

    switch (i_this->mMode) {
    case 0x28:
        for (int i = 0; i < 6; i++) {
            i_this->mTimers[6 + i] = 0;
        }
        i_this->mRollAngle = roll_dt[(s16)cM_rndF(3.99f)];
        i_this->mTimers[0] = 0xC8;
        i_this->mMode++;
        break;
    case 0x29:
        i_this->mTamaAtSph.OnAtVsEnemyBit();
        if (i_this->mTimers[6] == 0) {
            if (i_this->mTamaTgSph.ChkAtShieldHit()) {
                cXyz hitPos = *i_this->mTamaTgSph.GetAtHitPosP();
                i_this->mTamaTgSph.OffAtSetBit();
                i_this->mTamaTgSph.OffAtSetBit();
                i_this->mTimers[0] = 0xC8;
                i_this->mTamaAtSph.OffAtVsPlayerBit();
                i_this->mTimers[6] = 1;
                actor->speed.setall(0.0f);
                actor->speedF *= -1.0f;
                dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, &hitPos);
                fopAcM_seStart(actor, JA_SE_LK_HAMMER_HIT, 0x40);
            } else if (i_this->mTamaTgSph.ChkTgHit()) {
                cCcD_Obj* hitObj = i_this->mTamaTgSph.GetTgHitObj();
                fopAc_ac_c* hitAc = i_this->mTamaTgSph.GetTgHitAc();
                if (hitObj != NULL && hitObj->GetAtType() == AT_TYPE_SWORD) {
                    cXyz hitPos = *i_this->mTamaTgSph.GetTgHitPosP();
                    i_this->mTamaTgSph.OffAtSetBit();
                    i_this->mTamaTgSph.OffAtSetBit();
                    i_this->mTimers[0] = 0xC8;
                    fopAcM_seStart(actor, JA_SE_LK_HAMMER_HIT, 0x40);
                    dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, &hitPos);
                    i_this->mTamaAtSph.OffAtVsPlayerBit();
                    i_this->mTimers[6] = 1;
                    actor->speed.setall(0.0f);
                    actor->speedF *= -1.0f;
                } else {
                    if (hitAc != NULL && fopAcM_GetName(hitAc) == fopAcM_GetName(actor) && ((oq_class*)hitAc)->mType == 6) {
                        fopAcM_delete(hitAc);
                    }
#if VERSION > VERSION_DEMO
                    fopAcM_seStart(actor, JA_SE_OBJ_BREAK_ROCK, 0);
#endif
                    dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, &actor->current.pos);
                    fopAcM_delete(actor);
                }
            } else if (i_this->mTamaAtSph.ChkAtHit()) {
#if VERSION > VERSION_DEMO
                fopAcM_seStart(actor, JA_SE_OBJ_BREAK_ROCK, 0);
#endif
                dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, &actor->current.pos);
                fopAcM_delete(actor);
            }
        }
        if (i_this->mTimers[0] == 0) {
            i_this->mTamaAtSph.OffAtSetBit();
            i_this->mTamaAtSph.OffAtSetBit();
            i_this->mTamaTgSph.OffAtSetBit();
            i_this->mTamaTgSph.OffAtSetBit();
            i_this->mTamaTgSph.OffTgSetBit();
            i_this->mTamaTgSph.ClrTgHit();
            actor->gravity = -3.0f;
            i_this->mMode++;
        }
        break;
    case 0x2A:
        cLib_addCalc0(&actor->scale.x, 1.0f, 0.1f);
        actor->scale.y = actor->scale.z = actor->scale.x;
        if (actor->scale.x < 0.1f) {
            fopAcM_delete(actor);
        }
        break;
    }

    actor->shape_angle += i_this->mRollAngle;
    if (daSea_ChkArea(actor->current.pos.x, actor->current.pos.z)) {
        f32 waveY = daSea_calcWave(actor->current.pos.x, actor->current.pos.z);
        if (actor->current.pos.y < 40.0f + waveY) {
            cXyz pos = actor->current.pos;
            pos.y = waveY;
            shibuki_set(i_this, pos, 0.4f);
            fopAcM_delete(actor);
            return;
        }
    }

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cXyz offset;
    offset.x = 0.0f;
    offset.y = -40.0f;
    offset.z = 80.0f;
    cXyz dest;
    MtxPosition(&offset, &dest);
    dest += actor->current.pos;
    if (i_this->mAcch.ChkGroundHit() || i_this->mAcch.ChkWallHit() || Line_check(i_this, dest)) {
#if VERSION > VERSION_DEMO
        fopAcM_seStart(actor, JA_SE_OBJ_BREAK_ROCK, 0);
#endif
        dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, &actor->current.pos);
        fopAcM_delete(actor);
    }
}

/* 000032A4-0000369C       .text action_itai__FP8oq_class */
void action_itai(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;

    switch (i_this->mMode) {
    case 0x1E:
        for (int i = 0; i < 6; i++) {
            i_this->mTimers[6 + i] = 0;
        }
        i_this->mBodyAtCyl.OffAtSetBit();
        i_this->mBodyAtCyl.OffAtSetBit();
        i_this->mBodyCoCyl.OffTgSetBit();
        i_this->mBodyCoCyl.OffCoSetBit();
        i_this->mBodyCoCyl.ClrTgHit();
        actor->gravity = 0.0f;
        actor->speed.setall(0.0f);
        i_this->mRippleCb.end();
        anm_init(i_this, dRes_INDEX_OQ_BCK_DEAD1_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode++;
        break;
    case 0x1F:
        sea_water_check(i_this);
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_OQ_BCK_DEAD2_e, 0.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            fopAcM_seStart(actor, JA_SE_CM_OQ_DIE, 0);
#if VERSION == VERSION_DEMO
            i_this->mpEmitter = dComIfGp_particle_set(dPa_name::ID_AK_JN_OQDEADSPLASH00, &actor->current.pos);
#else
            if (i_this->mFollowCb.getEmitter() == NULL) {
                dComIfGp_particle_set(dPa_name::ID_AK_JN_OQDEADSPLASH00, &actor->current.pos, NULL, NULL, 0xFF, &i_this->mFollowCb);
            }
#endif
            i_this->mRollAngle.y = 0x1388;
            if (fopAcM_GetID(actor) & 1) {
                i_this->mRollAngle.y = -0x1388;
            }
            i_this->mTimers[0] = 10;
            i_this->mMadeWaterSplash = 0;
            i_this->mMode++;
        }
        break;
    case 0x20:
#if VERSION == VERSION_DEMO
        if (i_this->mpEmitter != NULL) {
            i_this->mpEmitter->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(0));
        }
#else
        if (i_this->mFollowCb.getEmitter() != NULL) {
            i_this->mFollowCb.getEmitter()->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(0));
        }
#endif
        actor->gravity = 0.5f;
        actor->shape_angle.y += i_this->mRollAngle.y;
        if (i_this->mTimers[0] == 0) {
            anm_init(i_this, dRes_INDEX_OQ_BCK_DEAD3_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode++;
        }
        break;
    case 0x21:
#if VERSION == VERSION_DEMO
        if (i_this->mpEmitter != NULL) {
            i_this->mpEmitter->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(0));
        }
#else
        if (i_this->mFollowCb.getEmitter() != NULL) {
            i_this->mFollowCb.getEmitter()->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(0));
        }
#endif
        actor->shape_angle.y += i_this->mRollAngle.y;
        if (i_this->mpMorf->isStop()) {
#if VERSION == VERSION_DEMO
            if (i_this->mpEmitter != NULL) {
                i_this->mpEmitter->becomeInvalidEmitter();
            }
#else
            i_this->mFollowCb.remove();
#endif
            if (i_this->mType == 5) {
                oq_class* parent = (oq_class*)fopAcM_SearchByID(i_this->mRelatedId);
                if (parent != NULL) {
                    parent->m2E8++;
                }
            }
            if (i_this->mType == 1 || i_this->mType == 5) {
                dSv_event_c* pEvent = &g_dComIfG_gameInfo.save.getSavedata().mEvent;
                int n = pEvent->getEventReg(dSv_event_flag_c::UNK_7EFF) + 1;
                n = cLib_maxLimit<int>(n, 0xFF) & 0xFF;
                pEvent->setEventReg(dSv_event_flag_c::UNK_7EFF, n);
            }
            fopAcM_createDisappear(actor, &actor->eyePos, 5, 0, 0xFF);
            fopAcM_delete(actor);
#if VERSION > VERSION_DEMO
            i_this->m2C9 = 1;
#endif
        }
        break;
    }

    cLib_addCalc0(&actor->scale.x, 1.0f, 0.01f);
    f32 scl = actor->scale.x;
    actor->scale.z = scl;
    actor->scale.y = scl;
}

/* 0000369C-00003BC4       .text action_wakidasi__FP8oq_class */
void action_wakidasi(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);

    if ((f32)i_this->m2E8 > 16.0f + REG8_F(0)) {
        fopAcM_delete(actor);
        return;
    }

    switch (i_this->mMode) {
    case 0x32:
        for (int i = 0; i < 6; i++) {
            i_this->mTimers[6 + i] = 0;
        }
        for (int i = 0; i < 6; i++) {
            i_this->mChildIds[i] = fpcM_ERROR_PROCESS_ID_e;
        }
        fopAcM_OffStatus(actor, fopAcStts_CULL_e);
        i_this->mMode++;
        // fallthrough
    case 0x33:
        if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < i_this->mSearchRange) {
            i_this->m2E6 = 0;
            i_this->mTimers[0] = 0;
            i_this->mMode++;
        }
        break;
    case 0x34:
        if (i_this->mTimers[0] == 0 && i_this->mTimers[1] == 0) {
            if (fopAcM_searchActorDistance(actor, player) > 500.0f + i_this->mSearchRange) {
                i_this->mMode = 0x33;
            } else {
                if (i_this->m2E6 == 0) {
                    i_this->m2E6 = (s16)(2.0f + REG12_F(4));
                    i_this->m2E6 += (s16)cM_rndF(1.99f + REG12_F(5));
                }
                int i;
                fpc_ProcID packed[6];
                int packedCount;
                for (i = 0, packedCount = 0; i < 6; i++) {
                    packed[i] = fpcM_ERROR_PROCESS_ID_e;
                    if (i_this->mChildIds[i] != fpcM_ERROR_PROCESS_ID_e) {
                        fopAc_ac_c* child = fopAcM_SearchByID(i_this->mChildIds[i]);
                        if (child == NULL) {
                            i_this->mChildIds[i] = fpcM_ERROR_PROCESS_ID_e;
                        } else {
                            packed[packedCount] = i_this->mChildIds[i];
                            packedCount++;
                        }
                    }
                }
                for (int i = 0; i < 6; i++) {
                    i_this->mChildIds[i] = packed[i];
                }
                i_this->mMode++;
            }
        }
        break;
    case 0x35: {
        const s16 playerY = player->shape_angle.y;
        s16 angle = playerY;
        angle += (s16)cM_rndFX(7000.0f + REG12_F(1));
        cMtx_YrotS(*calc_mtx, angle);
        cXyz offset;
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = (3500.0f + REG12_F(2)) + cM_rndF(2500.0f + REG12_F(3));
        cXyz pos;
        MtxPosition(&offset, &pos);
        pos += player->current.pos;
        pos.y -= 100.0f + REG12_F(9);
        f32 dx = pos.x - actor->current.pos.x;
        f32 dz = pos.z - actor->current.pos.z;
        if (std::sqrtf(dx * dx + dz * dz) > i_this->mSearchRange) {
            i_this->mTimers[1] = REG8_S(3) + 0x64;
            i_this->mMode = 0x34;
        } else {
            int i;
            for (i = 0; i < 6; i++) {
                if (i_this->mChildIds[i] == fpcM_ERROR_PROCESS_ID_e) {
                    u32 prm = 5;
                    prm |= 0x100;
                    fpc_ProcID id = fopAcM_createChild(
                        fpcNm_OQ_e,
                        fopAcM_GetID(actor),
                        prm,
                        &pos,
                        fopAcM_GetRoomNo(actor),
                        &actor->current.angle,
                        NULL,
                        -1,
                        NULL
                    );
                    if (id != fpcM_ERROR_PROCESS_ID_e) {
                        i_this->mChildIds[i] = id;
                        i_this->m2E6--;
                        if (i_this->m2E6 == 0) {
                            i_this->mTimers[0] = (s16)(90.0f + REG12_F(7));
                            i_this->mTimers[0] += (s16)cM_rndF(50.0f + REG12_F(8));
                        }
                    }
                    i_this->mTimers[1] = REG8_S(3) + 0x64;
                    i_this->mMode = 0x34;
                    return;
                }
            }
            oq_class* first = (oq_class*)fopAcM_SearchByID(i_this->mChildIds[0]);
            if (first != NULL) {
                first->m2C8 = 1;
            }
            i_this->mTimers[1] = REG8_S(3) + 0x64;
            i_this->mMode = 0x34;
        }
        break;
    }
    }
}

/* 00003BC4-00004274       .text daOQ_Execute__FP8oq_class */
static BOOL daOQ_Execute(oq_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        fopAcM_setGbaName(actor, 0x2D, 0x41, 0x42);
    }

    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        if (enemy_ice(&i_this->mEnemyIce)) {
            J3DModel* model = i_this->mpMorf->getModel();
            model->setBaseTRMtx(mDoMtx_stack_c::get());
            i_this->mpMorf->calc();
            enemy_fire_remove(&i_this->mEnemyFire);
            return TRUE;
        }
    }

    for (int i = 0; i < 6; i++) {
        if (i_this->mTimers[i] != 0) {
            i_this->mTimers[i]--;
        }
    }

    switch (i_this->mAction) {
    case 0xA:
        action_dousa(i_this);
        break;
    case 0xB:
        action_kougeki(i_this);
        break;
    case 0x14:
        action_itai(i_this);
        break;
    case 0x1E:
        action_tama_shoot(i_this);
        break;
    case 0x64:
        action_wakidasi(i_this);
        break;
    }

    if (i_this->mType == 3 || i_this->mType == 2) {
        return TRUE;
    }
#if VERSION > VERSION_DEMO
    if (i_this->m2C9 != 0) {
        return TRUE;
    }
#endif

    i_this->mpMorf->play(NULL, 0, 0);

    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        if (i_this->mType != 0) {
            if (dKy_daynight_check()) {
                i_this->mBrkFrame += 1.0f;
                if (i_this->mBrkFrame > 89.0f) {
                    i_this->mBrkFrame = 0.0f;
                }
            } else {
                cLib_addCalc0(&i_this->mBrkFrame, 1.0f, 10.0f);
            }
        } else if (i_this->mBckIdx == dRes_INDEX_OQ_BCK_NOM_WAIT_e && i_this->mAcch.ChkWaterIn()) {
            if (i_this->mpMorf->checkFrame(18.0f)) {
                csXyz angle = actor->current.angle;
                dComIfGp_particle_set(dPa_name::ID_AK_JN_OQWAITSPLASH00, &actor->current.pos, &angle);
                angle.y += 0x8000;
                dComIfGp_particle_set(dPa_name::ID_AK_JN_OQWAITSPLASH00, &actor->current.pos, &angle);
            }
        }
    }

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    cXyz speedOffs;
    speedOffs.x = 0.0f;
    speedOffs.y = 0.0f;
    speedOffs.z = actor->speedF;
    cXyz speedXZ;
    MtxPosition(&speedOffs, &speedXZ);
    actor->speed.x = speedXZ.x;
    actor->speed.z = speedXZ.z;
    actor->speed.y += actor->gravity;
    if (actor->speed.y < -100.0f) {
        actor->speed.y = -100.0f;
    }

    if (i_this->mBodyCoCyl.ChkCoSet()) {
        fopAcM_posMove(actor, i_this->mStts.GetCCMoveP());
    } else {
        fopAcM_posMove(actor, NULL);
    }
    BG_check(i_this);

    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        actor->attention_info.position = actor->current.pos;
        actor->attention_info.position.y += 250.0f;
        actor->eyePos = actor->current.pos;
        actor->eyePos.y += 130.0f;
        i_this->mBodyCoCyl.SetC(actor->current.pos);
        if (i_this->mType == 1 || i_this->mType == 5) {
            actor->attention_info.position = actor->current.pos;
            actor->attention_info.position.y += 550.0f;
            actor->eyePos = actor->current.pos;
            actor->eyePos.y += 250.0f;
            f32 atR = 175.0f + REG8_F(13);
            if (dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
                if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < 250.0f + REG8_F(14)) {
                    if (i_this->mTimers[11] == 0) {
                        i_this->mTimers[11] = 1;
                        i_this->mTimers[5] = (s16)(30.0f + REG8_F(15));
                    }
                } else {
                    i_this->mTimers[11] = 0;
                }
            }
            if (i_this->mTimers[5] != 0) {
                atR = 250.0f + REG8_F(16);
            }
            f32 coR = i_this->m30C;
            i_this->mBodyAtCyl.SetC(actor->current.pos);
            i_this->mBodyAtCyl.SetH(600.0f);
            i_this->mBodyAtCyl.SetR(atR);
            dComIfG_Ccsp()->Set(&i_this->mBodyAtCyl);
            i_this->mBodyCoCyl.SetH(600.0f);
            i_this->mBodyCoCyl.SetR(coR);
            dComIfG_Ccsp()->Set(&i_this->mBodyCoCyl);
        } else {
            i_this->mBodyCoCyl.SetH(170.0f);
            i_this->mBodyCoCyl.SetR(80.0f);
            dComIfG_Ccsp()->Set(&i_this->mBodyCoCyl);
        }
    } else if (i_this->mType == 6) {
        actor->eyePos = actor->current.pos;
        actor->attention_info.position = actor->current.pos;
        if (i_this->m2C2 == 0) {
            i_this->mTamaAtSph.SetC(actor->current.pos);
            if (i_this->mTimers[6] == 0) {
                i_this->mTamaAtSph.SetR(15.0f);
            } else {
                i_this->mTamaAtSph.SetR(40.0f);
            }
            dComIfG_Ccsp()->Set(&i_this->mTamaAtSph);
            i_this->mTamaTgSph.SetC(actor->current.pos);
            i_this->mTamaTgSph.SetR(60.0f);
            dComIfG_Ccsp()->Set(&i_this->mTamaTgSph);
        }
    }

    draw_SUB(i_this);
    return TRUE;
}

/* 00004274-0000427C       .text daOQ_IsDelete__FP8oq_class */
static BOOL daOQ_IsDelete(oq_class*) {
    return TRUE;
}

/* 0000427C-00004300       .text daOQ_Delete__FP8oq_class */
static BOOL daOQ_Delete(oq_class* i_this) {
#if VERSION == VERSION_DEMO
    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        i_this->mRippleCb.end();
    }
    if (i_this->mpEmitter != NULL) {
        i_this->mpEmitter->becomeInvalidEmitter();
    }
    dComIfG_deleteObjectRes("OQ");
#else
    dComIfG_resDelete(&i_this->mPhase, "OQ");
    i_this->mRippleCb.end();
    i_this->mFollowCb.remove();
#endif
    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        enemy_fire_remove(&i_this->mEnemyFire);
    }
    return TRUE;
}

/* 00004300-000046C0       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* i_ac) {
    oq_class* i_this = (oq_class*)i_ac;

    if (i_this->mType != 6) {
        J3DModelData* modelData;
        if (i_this->mType == 0) {
            modelData = (J3DModelData*)dComIfG_getObjectRes("OQ", dRes_INDEX_OQ_BMD_OQ_e);
        } else {
            modelData = (J3DModelData*)dComIfG_getObjectRes("OQ", dRes_INDEX_OQ_BMD_RED_OQ_e);
        }
        i_this->mpMorf = new mDoExt_McaMorf(
            modelData, NULL, NULL,
            (J3DAnmTransform*)dComIfG_getObjectRes("OQ", dRes_INDEX_OQ_BCK_START_e),
            J3DFrameCtrl::EMode_RESET, 1.0f, 0, -1, 1,
            NULL, 0, 0x11020203
        );
        if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) {
            return FALSE;
        }
        J3DModel* model = i_this->mpMorf->getModel();
        model->setUserArea((u32)i_this);
        for (u16 i = 0; i < i_this->mpMorf->getModel()->getModelData()->getJointNum(); i++) {
            i_this->mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
        }
        if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
            model = i_this->mpMorf->getModel();
            i_this->mpBrk = new mDoExt_brkAnm();
            if (i_this->mpBrk == NULL) {
                return FALSE;
            }
            if (i_this->mpBrk->init(
                    model->getModelData(),
                    (J3DAnmTevRegKey*)dComIfG_getObjectRes("OQ", dRes_INDEX_OQ_BRK_RED_OQ_e),
                    TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
                ) == 0)
            {
                return FALSE;
            }
        }
    } else {
        i_this->mpMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("OQ", dRes_INDEX_OQ_BDL_ISI_e),
            NULL, NULL, NULL,
            J3DFrameCtrl::EMode_RESET, 0.0f, 0, -1, 1,
            NULL, 0, 0x11020203
        );
        if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) {
            return FALSE;
        }
    }

    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
        if (i_this->mInvisibleModel.create(i_this->mpMorf->getModel()) == 0) {
            return FALSE;
        }
    }
#if VERSION == VERSION_DEMO
    {
#else
    if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
#endif
        static Vec head0_offset[] = {
            {-25.0f, 0.0f, 0.0f},
        };
        static Vec head1_offset_tbl[] = {
            {20.0f, 0.0f, 0.0f},
            {60.0f, 0.0f, 0.0f},
        };
        static Vec head2_offset[] = {
            {5.0f, 0.0f, 0.0f},
        };
        static __jnt_hit_data_c search_data[] = {
            {
                /* mShapeType  */ JntHitType_SPH_e,
                /* mJointIndex */ OQ_JNT_FACE1_e,
                /* mRadius     */ 50.0f,
                /* mpOffsets   */ head0_offset,
            },
            {
                /* mShapeType  */ JntHitType_CYL_e,
                /* mJointIndex */ OQ_JNT_FACE2_e,
                /* mRadius     */ 32.0f,
                /* mpOffsets   */ head1_offset_tbl,
            },
            {
                /* mShapeType  */ JntHitType_SPH_e,
                /* mJointIndex */ OQ_JNT_HEAD2_e,
                /* mRadius     */ 43.0f,
                /* mpOffsets   */ head2_offset,
            },
        };
        i_this->mpJntHit = JntHit_create(i_this->mpMorf->getModel(), search_data, 3);
        if (i_this->mpJntHit) {
            fopAcM_SetJntHit(i_ac, i_this->mpJntHit);
        }
#if VERSION > VERSION_DEMO
        else {
            return FALSE;
        }
#endif
    }
    return TRUE;
}

/* 00004708-00004DBC       .text daOQ_Create__FP10fopAc_ac_c */
static cPhs_State daOQ_Create(fopAc_ac_c* i_actor) {
    static dCcD_SrcCyl body_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
            /* SrcGObjAt Se      */ dCcG_SE_METAL,
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
        // cM3dGCylS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
            /* Height */ 0.0f,
        }},
    };
    static dCcD_SrcCyl body_at_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_STALFOS_MACE,
            /* SrcObjAt  Atp     */ 2,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ 0,
            /* SrcObjTg  SPrm    */ 0,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ dCcG_SE_METAL,
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
        // cM3dGCylS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
            /* Height */ 0.0f,
        }},
    };
    static dCcD_SrcSph tama_at_co_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_STALFOS_MACE,
            /* SrcObjAt  Atp     */ 1,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ 0,
            /* SrcObjTg  SPrm    */ 0,
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
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e | dCcG_TgSPrm_NoHitMark_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
        }},
    };
    static dCcD_SrcSph tama_tg_co_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_STALFOS_MACE,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_NoTgHitInfSet_e,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e | cCcD_TgSPrm_IsPlayer_e,
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
        OQ_JNT_FACE1_e,
        OQ_JNT_FACE2_e,
        OQ_JNT_HEAD_e,
        OQ_JNT_HEAD2_e,
        OQ_JNT_KUTI_e,
        OQ_JNT_KUTI2_e,
        OQ_JNT_KATAL_e,
        OQ_JNT_HANDL_e,
        OQ_JNT_KATAR_e,
        OQ_JNT_HANDR_e,
    };
    static f32 fire_sc[] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    oq_class* i_this = (oq_class*)i_actor;
    u32 heapSize = 0x3C80;
    i_this->mType = fopAcM_GetParam(i_this);
    i_this->m2C2 = fopAcM_GetParam(i_this) >> 8;
    i_this->mSearchRange = (f32)((fopAcM_GetParam(i_this) >> 16) & 0xFF);
    fopAcM_ct(i_this, oq_class);

    if (i_this->mType == 0xFF) {
        i_this->mType = 0;
    }
    if (i_this->mSearchRange == 255.0f || i_this->mSearchRange == 0.0f) {
        i_this->mSearchRange = 80000.0f;
    } else {
        i_this->mSearchRange *= 1000.0f;
    }

    if (i_this->mType == 3) {
#if VERSION > VERSION_DEMO
        i_this->group = 0;
#endif
        i_this->mAction = 0x64;
        i_this->mMode = 0x32;
        return cPhs_COMPLEATE_e;
    }

    cPhs_State phase_state = dComIfG_resLoad(&i_this->mPhase, "OQ");
    if (phase_state == cPhs_COMPLEATE_e) {
        if (i_this->mType == 0) {
            heapSize = 0x2E00;
        }
        if (i_this->mType == 6) {
            heapSize = 0x9E0;
        }
        if (i_this->m2C2 == 0xFF || i_this->m2C2 > 1) {
            i_this->m2C2 = 0;
        }
        if (i_this->mType != 3 && i_this->mType != 2) {
            if (!fopAcM_entrySolidHeap(i_actor, useHeapInit, heapSize)) {
                return cPhs_ERROR_e;
            }
        }
        if (i_this->mType == 2) {
            fopAc_ac_c* player = dComIfGp_getPlayer(0);
            u32 prm = 4;
            prm |= 0x100;
            hazure_shoot_timer = 0x64;
            cXyz offset;
            cXyz pos;
            for (int i = 0; i < 1; i++) {
                mDoMtx_YrotS(*calc_mtx, 0);
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 1000.0f;
                MtxPosition(&offset, &pos);
                pos += player->current.pos;
                pos.y -= 40.0f;
            }
            pos.x += cM_rndFX(200.0f);
            pos.z += cM_rndFX(200.0f);
            csXyz angle;
            angle.x = i_this->current.angle.x;
            angle.y = i_this->current.angle.y;
            angle.z = i_this->current.angle.z;
            angle.y = cM_atan2s(pos.x - player->current.pos.x, pos.z - player->current.pos.z);
            fopAcM_create(fpcNm_OQ_e, prm, &pos, fopAcM_GetRoomNo(i_this), &angle, &i_this->scale, 0, NULL);
            return cPhs_ERROR_e;
        }

        i_this->max_health = 1;
        i_this->health = 1;
        fopAcM_SetMtx(i_this, i_this->mpMorf->getModel()->getBaseTRMtx());
        i_this->attention_info.flags = 0;
#if VERSION > VERSION_DEMO
        i_this->mHomePos = i_this->current.pos;
#endif
        i_this->mAcch.Set(
            fopAcM_GetPosition_p(i_this),
            fopAcM_GetOldPosition_p(i_this),
            i_this,
            1,
            &i_this->mAcchCir,
            fopAcM_GetSpeed_p(i_this)
        );
        i_this->mStts.Init(0xC8, 1, i_this);
        i_this->mCorrectionOffsetY = 50.0f;

        if (i_this->mType != 6) {
            if (i_this->mType == 0 || i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
                i_this->gbaName = 0x1B;
                i_this->mEnemyIce.mpActor = i_this;
                i_this->mEnemyIce.mWallRadius = 170.0f;
                i_this->mEnemyIce.mCylHeight = 80.0f;
                i_this->mEnemyFire.mpMcaMorf = i_this->mpMorf;
                i_this->mEnemyFire.mpActor = i_this;
                for (int i = 0; i < (int)ARRAY_SIZE(i_this->mEnemyFire.mFlameJntIdxs); i++) {
                    i_this->mEnemyFire.mFlameJntIdxs[i] = fire_j[i];
                    i_this->mEnemyFire.mParticleScale[i] = fire_sc[i];
                }
            }
            if (i_this->mType == 4) {
                i_this->mStts.SetWeight(0xFE);
                i_this->mTimers[0] = hazure_shoot_timer;
                i_this->mTimers[0] += (s16)cM_rndF(50.0f);
                hazure_shoot_timer += 0x64;
            }
            if (i_this->mType == 1 || i_this->mType == 4 || i_this->mType == 5) {
                i_this->m30C = 175.0f + REG8_F(12);
                i_this->m2C2 = 1;
            }
            if (i_this->mType == 5) {
                i_this->mRelatedId = i_this->parentActorID;
                if (i_this->mRelatedId == fpcM_ERROR_PROCESS_ID_e) {
                    return cPhs_ERROR_e;
                }
                i_this->attention_info.distances[2] = 4;
            }
            if (REG8_S(9) != 0) {
                i_this->m2C2 ^= 1;
            }
            i_this->gravity = -3.0f;
            fopAcM_setCullSizeBox(i_this, -250.0f, 0.0f, -80.0f, 250.0f, 200.0f, 80.0f);
            i_this->mBodyCoCyl.Set(body_co_cyl_src);
            i_this->mBodyCoCyl.SetStts(&i_this->mStts);
            i_this->mBodyAtCyl.Set(body_at_cyl_src);
            i_this->mBodyAtCyl.SetStts(&i_this->mStts);
            i_this->mBodyCoCyl.OffTgSetBit();
            i_this->mBodyCoCyl.OffCoSetBit();
            i_this->mBodyCoCyl.ClrTgHit();
            i_this->mBodyAtCyl.OffAtSetBit();
            i_this->mBodyAtCyl.ClrAtSet();
            if (i_this->mType == 0) {
                i_this->mBodyCoCyl.SetTgGrp(cCcD_TgSPrm_IsEnemy_e);
                i_this->stealItemLeft = 1;
                i_this->mAcch.ClrRoofNone();
                i_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("Oq", 0);
                i_this->attention_info.distances[2] = 3;
            } else {
                i_this->gbaName = 0x41;
                i_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("Oqw", 0);
                i_this->attention_info.distances[2] = 0x2A;
            }
            i_this->scale.setall(0.0f);
            i_this->mAction = 0xA;
            i_this->mMode = 0;
        } else if (i_this->m2C2 == 0) {
            i_this->mTamaAtSph.Set(tama_at_co_sph_src);
            i_this->mTamaAtSph.SetStts(&i_this->mStts);
            i_this->mTamaTgSph.Set(tama_tg_co_sph_src);
            i_this->mTamaTgSph.SetStts(&i_this->mStts);
            fopAcM_setCullSizeBox(i_this, -50.0f, -50.0f, -50.0f, 50.0f, 50.0f, 50.0f);
            i_this->speedF = 40.0f;
            i_this->mAction = 0x1E;
            i_this->mMode = 0x28;
        }

        i_this->mTargetAngleY = i_this->current.angle.y;
        if (i_this->mType != 3 && i_this->mType != 2) {
            draw_SUB(i_this);
        }
    }
    return phase_state;
}

static actor_method_class l_daOQ_Method = {
    (process_method_func)daOQ_Create,
    (process_method_func)daOQ_Delete,
    (process_method_func)daOQ_Execute,
    (process_method_func)daOQ_IsDelete,
    (process_method_func)daOQ_Draw,
};

actor_process_profile_definition g_profile_OQ = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_OQ_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(oq_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_OQ_e,
    /* Actor SubMtd */ &l_daOQ_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
