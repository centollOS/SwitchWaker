/**
 * d_a_wz.cpp
 * Enemy - Wizzrobe / Mini-Boss - Wizzrobe (Wind Temple)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_wz.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_sea.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_material.h"
#include "d/d_particle_name.h"
#include "d/d_path.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera.h"
#include "f_pc/f_pc_manager.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DNode.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "c/c_damagereaction.h"
#include "res/Object/Wz.h"
#include "res/Object/Wzb.h"

enum daWZ_Joint {
    WZ_JNT_ROD = 0x11,
    WZ_JNT_MINI = 0x13,
};

enum daWZ_RodJoint {
    WZ_ROD_JNT_TIP = 0x2,
};

static u8 come_flag;

/* 00000078-00000140       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        wz_class* i_this = (wz_class*)model->getUserArea();
        if (i_this != NULL) {
            if (jntNo == WZ_JNT_ROD) {
                MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
                cXyz offset;
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mStaffPos);
#if VERSION == VERSION_DEMO
                model->setAnmMtx(jntNo, *calc_mtx);
#else
                MTXCopy(*calc_mtx, model->getAnmMtx(jntNo));
#endif
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }
    return TRUE;
}

/* 00000140-00000208       .text rod_nodeCallBack__FP7J3DNodei */
static BOOL rod_nodeCallBack(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        wz_class* i_this = (wz_class*)model->getUserArea();
        if (i_this != NULL) {
            if (jntNo == WZ_ROD_JNT_TIP) {
                MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
                cXyz offset;
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mRodTipPos);
#if VERSION == VERSION_DEMO
                model->setAnmMtx(jntNo, *calc_mtx);
#else
                MTXCopy(*calc_mtx, model->getAnmMtx(jntNo));
#endif
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }
    return TRUE;
}

/* 00000208-00000360       .text draw_SUB__FP8wz_class */
void draw_SUB(wz_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    model->setBaseScale(i_this->scale);
    mDoMtx_stack_c::transS(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z);
    mDoMtx_stack_c::YrotM(i_this->shape_angle.y);
    mDoMtx_stack_c::XrotM(i_this->shape_angle.x);
    mDoMtx_stack_c::ZrotM(i_this->shape_angle.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());

    i_this->mpMorf->calc();

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        if (i_this->mIsMiniBoss) {
            MtxP miniSrc = i_this->mpMorf->getModel()->getAnmMtx(WZ_JNT_MINI);
            MTXCopy(miniSrc, i_this->mpMiniMorf->getModel()->getBaseTRMtx());
            i_this->mpMiniMorf->calc();
        }

        i_this->mpRodMorf->getModel()->setBaseScale(i_this->mRodScale);
        MtxP rodSrc = i_this->mpMorf->getModel()->getAnmMtx(WZ_JNT_ROD);
        MTXCopy(rodSrc, i_this->mpRodMorf->getModel()->getBaseTRMtx());
        i_this->mpRodMorf->calc();
        enemy_fire(&i_this->mEnemyFire);
    }

    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &i_this->current.pos, &i_this->tevStr);
}

/* 00000360-000006D0       .text hontai_draw__FP8wz_class */
void hontai_draw(wz_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    J3DModelData* modelData = model->getModelData();
    J3DModel* rodModel = i_this->mpRodMorf->getModel();

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        if (i_this->mEnableSpawnSwitch != 0xFF) {
            if (!dComIfGs_isSwitch(i_this->mEnableSpawnSwitch, dComIfGp_roomControl_getStayNo())) {
                return;
            }
        }
#if VERSION <= VERSION_JPN
        dSnap_RegistFig(DSNAP_TYPE_UNKC4, i_this, 1.0f, 1.0f, 1.0f);
#else
        if (!i_this->mIsMiniBoss) {
            if (i_this->mAlpha >= 0x80) {
                dSnap_RegistFig(DSNAP_TYPE_UNKC4, i_this, 1.0f, 1.0f, 1.0f);
            }
        }
#endif
    }

    g_env_light.setLightTevColorType(model, &i_this->tevStr);
    g_env_light.setLightTevColorType(rodModel, &i_this->tevStr);

    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        modelData->getMaterialNodePointer(i)->getTevKColor(3)->mColor.a = i_this->mAlpha;
    }

    if (i_this->mIsMiniBoss) {
        J3DModel* miniModel = i_this->mpMiniMorf->getModel();
        J3DModelData* miniModelData = miniModel->getModelData();
        g_env_light.setLightTevColorType(miniModel, &i_this->tevStr);
        for (u16 i = 0; i < miniModelData->getMaterialNum(); i++) {
            miniModelData->getMaterialNodePointer(i)->getTevKColor(3)->mColor.a = i_this->mAlpha;
        }
#if VERSION <= VERSION_JPN
        dSnap_RegistFig(DSNAP_TYPE_UNKC5, i_this, 1.0f, 1.0f, 1.0f);
#else
        if (i_this->mAlpha >= 0x80) {
            dSnap_RegistFig(DSNAP_TYPE_UNKC5, i_this, 1.0f, 1.0f, 1.0f);
        }
#endif
    }

    if (i_this->mEnemyIce.mFreezeTimer > 20) {
        dMat_control_c::iceEntryDL(i_this->mpMorf, -1, NULL);
        if (i_this->mIsMiniBoss) {
            dMat_control_c::iceEntryDL(i_this->mpMiniMorf, -1, NULL);
        }
        return;
    }

    i_this->mpBrk->entry(model->getModelData());
    f32 frame = i_this->mBehaviorType + i_this->mIsMiniBoss;
    if (i_this->m351) {
        frame = 3.0f;
    }
    i_this->mpBrk->setFrame(frame);
    i_this->mpMorf->entryDL();
#if VERSION <= VERSION_JPN
    if (REG8_S(1) != 0 && i_this->mMode == 0x32) {
        GXColor color = {0xFF, 0x00, 0xFF, 0xFF};
        cXyz pos = i_this->current.pos;
        pos.y += 20.0f;
    }
#endif
    if (i_this->mIsMiniBoss) {
        i_this->mpMiniMorf->entryDL();
        i_this->mMiniInvisibleModel.entry();
    }
    i_this->mpBrk->remove(model->getModelData());
    i_this->mInvisibleModel.entry();

    if (i_this->mAlpha != 0) {
        i_this->mpRodMorf->entryDL();
        cXyz shadowPos;
        shadowPos.set(i_this->current.pos.x, i_this->current.pos.y + REG8_F(18), i_this->current.pos.z);
        i_this->mShadowId = dComIfGd_setShadow(
            i_this->mShadowId,
            1,
            i_this->mpMorf->getModel(),
            &shadowPos,
            700.0f + REG8_F(19),
            40.0f,
            i_this->current.pos.y,
            i_this->mAcch.GetGroundH(),
            i_this->mAcch.m_gnd,
            &i_this->tevStr,
            0,
            1.0f,
            dDlst_shadowControl_c::getSimpleTex()
        );
        if (i_this->mShadowId != 0) {
            dComIfGd_addRealShadow(i_this->mShadowId, i_this->mpRodMorf->getModel());
            if (i_this->mIsMiniBoss) {
                dComIfGd_addRealShadow(i_this->mShadowId, i_this->mpMiniMorf->getModel());
            }
        }
    }
}

/* 000006D0-0000074C       .text summon_door_draw__FP8wz_class */
void summon_door_draw(wz_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    g_env_light.setLightTevColorType(model, &i_this->tevStr);
    i_this->mpBtk->entry(model->getModelData());
    i_this->mpMorf->updateDL();
    i_this->mpBtk->remove(model->getModelData());
}

/* 0000074C-00000814       .text damage_ball_draw__FP8wz_class */
static void damage_ball_draw(wz_class* i_this) {
    if (i_this->mBallAlpha != 0) {
        MtxTrans(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z, false);
        MtxScale(0.6f, 0.6f, 0.6f, true);
        cMtx_YrotM(*calc_mtx, i_this->shape_angle.y);
        cMtx_copy(*calc_mtx, i_this->mBallMtx);
        GXColor color;
        color.r = 0xEB;
        color.g = 0x50;
        color.b = 0;
        color.a = 0;
        dComIfGd_setAlphaModelColor(color);
        dComIfGd_setAlphaModel(dDlst_alphaModel_c::TYPE_TWO_SPHERES, i_this->mBallMtx, i_this->mBallAlpha);
    }
}

/* 00000814-000008C0       .text daWZ_Draw__FP8wz_class */
static BOOL daWZ_Draw(wz_class* i_this) {
    if (i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_FIRE || i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_ICE) {
        if (i_this->scale.x) {
            dComIfGd_setSimpleShadow2(
                &i_this->current.pos,
                i_this->mAcch.GetGroundH(),
                30.0f,
                i_this->mAcch.m_gnd,
                0,
                1.0f,
                dDlst_shadowControl_c::getSimpleTex()
            );
        }
        if (i_this->mBallAlpha != 0) {
            damage_ball_draw(i_this);
        }
        return TRUE;
    }

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        hontai_draw(i_this);
    } else {
        summon_door_draw(i_this);
    }
    return TRUE;
}

/* 000008C0-000009EC       .text anm_init__FP8wz_classifUcfi */
void anm_init(wz_class* i_this, int bckFileIdx, float morf, unsigned char loopMode, float speed, int soundFileIdx) {
    i_this->mBckIdx = bckFileIdx;
#if VERSION == VERSION_DEMO
    if (soundFileIdx >= 0) {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("WZ", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, dComIfG_getObjectRes("WZ", soundFileIdx));
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("WZ", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
#else
    if (soundFileIdx >= 0) {
        void* soundAnm = dComIfG_getObjectRes("WZ", soundFileIdx);
        J3DAnmTransform* bckAnm = (J3DAnmTransform*)dComIfG_getObjectRes("WZ", bckFileIdx);
        i_this->mpMorf->setAnm(bckAnm, loopMode, morf, speed, 0.0f, -1.0f, soundAnm);
    } else {
        J3DAnmTransform* bckAnm = (J3DAnmTransform*)dComIfG_getObjectRes("WZ", bckFileIdx);
        i_this->mpMorf->setAnm(bckAnm, loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
#endif
}

/* 000009EC-00000A58       .text rod_size_set__FP8wz_classUc */
void rod_size_set(wz_class* i_this, unsigned char param) {
    if (param == 0) {
        cLib_addCalc2(&i_this->mRodScale.x, 1.0f, 0.2f, 0.5f);
    } else {
        cLib_addCalc0(&i_this->mRodScale.x, 0.2f, 0.5f);
    }
    f32 rodX = i_this->mRodScale.x;
    i_this->mRodScale.z = rodX;
    i_this->mRodScale.y = rodX;
}

/* 00000A58-00001258       .text body_atari_check__FP8wz_class */
BOOL body_atari_check(wz_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    i_this->mStts.Move();
    i_this->m34E = 0;
    if (i_this->mBodyCyl.ChkTgHit()) {
        cCcD_Obj* hitObj = i_this->mBodyCyl.GetTgHitObj();
        if (hitObj == NULL) {
            return FALSE;
        }
        if (i_this->m34F != 0) {
            return FALSE;
        }

        i_this->m34F = 1;
        CcAtInfo atInfo;
        atInfo.pParticlePos = NULL;
        u8 skipAtCheck = 0;
        switch (hitObj->GetAtType()) {
        case AT_TYPE_GRAPPLING_HOOK:
            if (i_this->stealItemLeft > 0) {
                s8 oldHealth = i_this->health;
                i_this->health = 10;
                atInfo.mpObj = i_this->mBodyCyl.GetTgHitObj();
                cc_at_check(i_this, &atInfo);
                i_this->m353++;
                i_this->health = oldHealth;
            }
            dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &i_this->attention_info.position);
            skipAtCheck = 1;
            i_this->m34E = 9;
            fopAcM_seStart(actor, JA_SE_LK_W_WEP_HIT, 0x44);
            fopAcM_monsSeStart(actor, JA_SE_CV_WZ_NO_DAMAGE, 0);
            break;
        case AT_TYPE_SWORD: {
            fopAcM_seStart(actor, JA_SE_LK_SW_HIT_S, 0x20);
            i_this->m34E = 0;
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
            i_this->m34E = 1;
            break;
        }
        case AT_TYPE_WIND:
            skipAtCheck = 1;
            i_this->m34E = 3;
            fopAcM_seStart(actor, JA_SE_LK_W_WEP_HIT, 0x44);
            fopAcM_monsSeStart(actor, JA_SE_CV_WZ_NO_DAMAGE, 0);
            break;
        case AT_TYPE_BOOMERANG:
            skipAtCheck = 1;
            i_this->m34E = 4;
            dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &i_this->attention_info.position);
            fopAcM_seStart(actor, JA_SE_LK_W_WEP_HIT, 0x44);
            fopAcM_monsSeStart(actor, JA_SE_CV_WZ_NO_DAMAGE, 0);
            break;
        case AT_TYPE_BOKO_STICK:
            fopAcM_seStart(actor, JA_SE_LK_W_WEP_HIT, 0x20);
            break;
        case AT_TYPE_SKULL_HAMMER:
            fopAcM_seStart(actor, JA_SE_LK_HAMMER_HIT, 0x20);
            i_this->m34E = 7;
            if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
                i_this->m34E = 8;
            }
            break;
        case AT_TYPE_BOMB:
            i_this->m34E = 6;
            break;
        case AT_TYPE_FIRE:
        case AT_TYPE_FIRE_ARROW:
            i_this->mEnemyFire.mFireDuration = 100;
            break;
        case AT_TYPE_ICE_ARROW:
            i_this->mParticleCb2.remove();
            i_this->mParticleCb3.remove();
            i_this->mParticleCb4.remove();
#if VERSION > VERSION_DEMO
            i_this->mRodScale.setall(0.0f);
#endif
            i_this->mEnemyIce.m00C = 2;
            i_this->mEnemyIce.mFreezeDuration = 200;
            i_this->attention_info.flags = 0;
            skipAtCheck = 1;
            break;
        case AT_TYPE_LIGHT_ARROW:
            i_this->mEnemyIce.mLightShrinkTimer = 1;
            i_this->mEnemyIce.mParticleScale = 1.0f;
            i_this->mEnemyIce.mYOffset = 80.0f;
            i_this->attention_info.flags = 0;
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            rod_size_set(i_this, 1);
            i_this->mParticleCb2.remove();
            i_this->mParticleCb3.remove();
            i_this->mParticleCb4.remove();
            break;
        case AT_TYPE_NORMAL_ARROW:
            i_this->m34E = 5;
        default:
            i_this->m34E = 0;
            fopAcM_seStart(actor, JA_SE_LK_MS_WEP_HIT, 0x20);
            break;
        }

        if (skipAtCheck == 0) {
            cXyz hitPos = *i_this->mBodyCyl.GetTgHitPosP();
            cXyz scale;
            atInfo.mpObj = i_this->mBodyCyl.GetTgHitObj();
            cc_at_check(i_this, &atInfo);
            if (i_this->m34E == 1 || i_this->m34E == 7 || i_this->m34E == 8 || i_this->health <= 0) {
                dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hitPos);
                scale.setall(2.0f);
                dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hitPos, &player->shape_angle, &scale);
            } else {
                dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &hitPos, &player->shape_angle);
            }
        }

        i_this->mAction = 1;
        i_this->mMode = 0xA;
        return TRUE;
    }

    i_this->m34F = 0;
    return FALSE;
}

/* 00001258-000012F0       .text BG_check__FP8wz_class */
void BG_check(wz_class* i_this) {
    i_this->mAcchCir.SetWall(i_this->mWallHeight, i_this->mWallRadius);
    i_this->mCorrectionOffsetY = 20.0f;
    i_this->current.pos.y -= i_this->mCorrectionOffsetY;
    i_this->old.pos.y -= i_this->mCorrectionOffsetY;
    i_this->mAcch.CrrPos(*dComIfG_Bgsp());
    i_this->current.pos.y += i_this->mCorrectionOffsetY;
    i_this->old.pos.y += i_this->mCorrectionOffsetY;
}

// Unused; stripped by the linker. Name/signature from d_a_wzD.map; body reconstructed from its leftover
// 100.0f literal and dBgS_LinChk weak destructors (same size as d_a_oq's Line_check in both demo maps).
BOOL Line_check(wz_class* i_this, cXyz destPos) {
    fopAc_ac_c* actor = i_this;
    dBgS_LinChk linChk;
    cXyz start = actor->current.pos;
    start.y += 100.0f;
    linChk.Set(&start, &destPos, actor);
    if (dComIfG_Bgsp()->LineCross(&linChk)) {
        return TRUE;
    }
    return FALSE;
}

/* 00001728-00001780       .text fuwafuwa_calc__FP8wz_class */
void fuwafuwa_calc(wz_class* i_this) {
    i_this->mFuwafuwaAngle += 0x7D0;
    i_this->current.pos.y = 15.0f + i_this->mHomePos.y + 5.0f * cM_ssin(i_this->mFuwafuwaAngle);
}

/* 00001780-00001B38       .text weapon_shoot__FP8wz_classUc */
void weapon_shoot(wz_class* i_this, unsigned char type) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 dx, dy, dz;
#if VERSION == VERSION_DEMO
    u32 params = 0xFFFFFF00;
#endif
    csXyz angle = i_this->shape_angle;
    angle = i_this->shape_angle;

    fopAcM_monsSeStart(actor, JA_SE_CV_WZ_ATTACK, 0);

    switch (type) {
    case 0: {
#if VERSION == VERSION_DEMO
        params |= 0xA;
#endif
        dx = player->current.pos.x - i_this->mRodTipPos.x;
        dy = 50.0f + player->current.pos.y - i_this->mRodTipPos.y;
        dz = player->current.pos.z - i_this->mRodTipPos.z;
        angle.x = -cM_atan2s(dy, std::sqrtf(dx * dx + dz * dz));

        s16 yawOff = -0xBB8;
        for (int i = 0; i < 3; i++) {
            angle.y = yawOff + cM_atan2s(dx, dz);
            fopAcM_create(
                fpcNm_WZ_e,
                DEMO_SELECT(params, 0xFFFFFF0A),
                &i_this->mRodTipPos,
                fopAcM_GetRoomNo(actor),
                &angle,
                NULL,
                -1,
                NULL
            );
            yawOff += 0xBB8;
        }
        fopAcM_seStart(actor, JA_SE_CM_WZ_ST_FIREBALL, 0);
        break;
    }
    case 1:
        if (i_this->m439 != 0xFF) {
            if (i_this->mpPath != NULL) {
                cXyz scale = i_this->scale;
#if VERSION == VERSION_DEMO
                params |= 0xB;
#endif
                dx = i_this->mPathTarget.x - i_this->mRodTipPos.x;
                dy = 300.0f + i_this->mPathTarget.y - i_this->mRodTipPos.y;
                dz = i_this->mPathTarget.z - i_this->mRodTipPos.z;
                angle.x = -cM_atan2s(dy, std::sqrtf(dx * dx + dz * dz));
                angle.y = cM_atan2s(dx, dz);
                scale.setall(1.0f);
                fopAcM_seStart(actor, JA_SE_CM_WZ_ST_SUMMONBALL, 0);
                fpc_ProcID id = fopAcM_createChild(
                    fpcNm_WZ_e,
                    fopAcM_GetID(actor),
                    DEMO_SELECT(params, 0xFFFFFF0B),
                    &i_this->mRodTipPos,
                    fopAcM_GetRoomNo(actor),
                    &angle,
                    &scale,
                    0
                );
                if (id != fpcM_ERROR_PROCESS_ID_e) {
                    i_this->mHasChildActor = 1;
                }
            }
        }
        break;
    }
}

/* 00001B38-00002528       .text action_dousa__FP8wz_class */
void action_dousa(wz_class* i_this) {
    static GXColor eff_prm_color_dt[] = {
        {0xFF, 0x47, 0x00, 0xFF},
        {0xFF, 0xFF, 0x08, 0xFF},
        {0xFF, 0xFF, 0xA0, 0xFF},
    };
    static GXColor eff_env_color_dt[] = {
        {0xFF, 0x00, 0x00, 0xFF},
        {0x7D, 0x48, 0x38, 0xFF},
        {0x4B, 0xBA, 0xFF, 0xFF},
    };

    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 frame;
#endif

    switch (i_this->mMode) {
    case 0:
#if VERSION == VERSION_DEMO
        f32 dist = 7500.0f;
        if (fopAcM_searchActorDistance(actor, player) > dist) {
#else
        if (fopAcM_searchPlayerDistance(actor) > 7500.0f) {
#endif
            break;
        }
        for (int i = 0; i < 4; i++) {
            i_this->m3DE[i] = 0;
        }
        if (i_this->mBehaviorType == 1) {
            i_this->mPathTarget = i_this->mpPath->m_points[i_this->mPathPointIdx].m_position;
            i_this->m3F8 = 1;
        }
        actor->scale.setall(1.0f);
        fopAcM_seStart(actor, JA_SE_CM_WZ_APPEAR, 0);
        anm_init(i_this, dRes_INDEX_WZ_BCK_SYUTUGEN1_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        fopAcM_OnStatus(actor, fopAcStts_SHOWMAP_e);
        actor->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
        i_this->mMode++;
        // Fall-through
    case 1:
        i_this->mAlpha += 8;
        if (i_this->mAlpha < 0xFF) {
            break;
        }
        i_this->mAlpha = 0xFF;
        i_this->mMode++;
        // Fall-through
    case 2:
        i_this->mTimers[0] = 0x5A;
        i_this->m3F8 = 0;
        if (i_this->mBehaviorType == 1 && i_this->mHasChildActor == 0) {
            i_this->m3F8 = 1;
            i_this->mTargetAngleY = cM_atan2s(
                i_this->mPathTarget.x - actor->current.pos.x,
                i_this->mPathTarget.z - actor->current.pos.z
            );
        }
        switch (i_this->m3F8) {
        case 0:
            anm_init(i_this, dRes_INDEX_WZ_BCK_TAME2_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            break;
        case 1:
            anm_init(i_this, dRes_INDEX_WZ_BCK_TAME1_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            break;
        }
        dComIfGp_particle_set(
            dPa_name::ID_AK_SN_WIZROD00,
            &i_this->mRodTipPos,
            NULL,
            NULL,
            0xFF,
            &i_this->mParticleCb2
        );
        if (i_this->mParticleCb2.getEmitter() != NULL) {
            i_this->mParticleCb2.getEmitter()->setGlobalSRTMatrix(
                i_this->mpRodMorf->getModel()->getAnmMtx(WZ_ROD_JNT_TIP)
            );
        }
        i_this->mBodyCyl.OnTgSetBit();
        i_this->mMode++;
        break;
    case 3:
        if (i_this->mParticleCb2.getEmitter() != NULL) {
            i_this->mParticleCb2.getEmitter()->setGlobalSRTMatrix(
                i_this->mpRodMorf->getModel()->getAnmMtx(WZ_ROD_JNT_TIP)
            );
        }
        rod_size_set(i_this, 0);
        if (i_this->mTimers[0] != 0) {
            break;
        }
        i_this->mMode++;
        i_this->mParticleCb2.remove();
        break;
    case 4:
        anm_init(i_this, dRes_INDEX_WZ_BCK_ATTACK1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        if (i_this->mParticleCb3.getEmitter() == NULL) {
            dComIfGp_particle_set(
                dPa_name::ID_AK_SN_WIZCASTMAGIC00,
                &i_this->mRodTipPos,
                NULL,
                NULL,
                0xFF,
                &i_this->mParticleCb3
            );
        }
        if (i_this->mParticleCb3.getEmitter() != NULL) {
            i_this->mParticleCb3.getEmitter()->setGlobalPrmColor(
                eff_prm_color_dt[i_this->m3F8].r,
                eff_prm_color_dt[i_this->m3F8].g,
                eff_prm_color_dt[i_this->m3F8].b
            );
            i_this->mParticleCb3.getEmitter()->setGlobalEnvColor(
                eff_env_color_dt[i_this->m3F8].r,
                eff_env_color_dt[i_this->m3F8].g,
                eff_env_color_dt[i_this->m3F8].b
            );
            i_this->mParticleCb3.getEmitter()->setGlobalSRTMatrix(
                i_this->mpRodMorf->getModel()->getAnmMtx(WZ_ROD_JNT_TIP)
            );
        }
        if (i_this->mParticleCb4.getEmitter() == NULL) {
            dComIfGp_particle_set(
                dPa_name::ID_AK_SN_WIZCASTMAGIC01,
                &i_this->mRodTipPos,
                NULL,
                NULL,
                0xFF,
                &i_this->mParticleCb3
            );
        }
        if (i_this->mParticleCb4.getEmitter() != NULL) {
            i_this->mParticleCb4.getEmitter()->setGlobalPrmColor(
                eff_prm_color_dt[i_this->m3F8].r,
                eff_prm_color_dt[i_this->m3F8].g,
                eff_prm_color_dt[i_this->m3F8].b
            );
            i_this->mParticleCb4.getEmitter()->setGlobalEnvColor(
                eff_env_color_dt[i_this->m3F8].r,
                eff_env_color_dt[i_this->m3F8].g,
                eff_env_color_dt[i_this->m3F8].b
            );
            i_this->mParticleCb4.getEmitter()->setGlobalSRTMatrix(
                i_this->mpRodMorf->getModel()->getAnmMtx(WZ_ROD_JNT_TIP)
            );
        }
        switch (i_this->m3F8) {
        case 0:
            fopAcM_seStart(actor, JA_SE_CM_WZ_PRE_FIREBALL, 0);
            break;
        case 1:
            fopAcM_seStart(actor, JA_SE_CM_WZ_PRE_SUMMONBALL, 0);
            break;
        }
        i_this->mMode++;
        break;
    case 5:
#if VERSION == VERSION_DEMO
        frame = 38.0f;
#endif
        if (i_this->mParticleCb3.getEmitter() != NULL) {
            i_this->mParticleCb3.getEmitter()->setGlobalSRTMatrix(
                i_this->mpRodMorf->getModel()->getAnmMtx(WZ_ROD_JNT_TIP)
            );
        }
        if (i_this->mParticleCb4.getEmitter() != NULL) {
            i_this->mParticleCb4.getEmitter()->setGlobalSRTMatrix(
                i_this->mpRodMorf->getModel()->getAnmMtx(WZ_ROD_JNT_TIP)
            );
        }
        if (i_this->m3F8 != 1) {
#if VERSION == VERSION_DEMO
            if (i_this->mpMorf->getFrame() < frame) {
#else
            if (i_this->mpMorf->getFrame() < 38.0f) {
#endif
                i_this->mTargetAngleY = fopAcM_searchPlayerAngleY(actor);
            }
        }
        if (i_this->mpMorf->checkFrame(35.0f)) {
            switch (i_this->m3F8) {
            case 0:
                weapon_shoot(i_this, 0);
                break;
            case 1:
                if (i_this->mHasChildActor == 0) {
                    weapon_shoot(i_this, 1);
                } else {
                    weapon_shoot(i_this, 0);
                }
                break;
            }
        }
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        fopAcM_monsSeStart(actor, JA_SE_CV_WZ_LAUGH, 0);
        i_this->mMode = 6;
        i_this->mParticleCb3.remove();
        i_this->mParticleCb4.remove();
        break;
    case 6:
#if VERSION > VERSION_DEMO
        actor->speed.setall(0.0f);
        actor->speedF = 0.0f;
        fopAcM_cancelCarryNow(actor);
        actor->attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
        actor->shape_angle.x = 0;
        actor->current.angle.x = 0;
        actor->shape_angle.z = 0;
        actor->current.angle.z = 0;
#endif
        anm_init(i_this, dRes_INDEX_WZ_BCK_SYUTUGEN1_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        fopAcM_OffStatus(actor, fopAcStts_SHOWMAP_e);
        actor->attention_info.flags = 0;
        i_this->mBodyCyl.OffTgSetBit();
        i_this->mBodyCyl.ClrTgHit();
        actor->current.angle.y = actor->shape_angle.y;
        i_this->mMode++;
        // Fall-through
    case 7: {
        s16 rot = REG8_S(3) + 0x700;
        actor->current.angle.y -= rot;
        i_this->mTargetAngleY = actor->current.angle.y;
        actor->shape_angle.y = i_this->mTargetAngleY;
        rod_size_set(i_this, 1);
        i_this->mAlpha -= 8;
        if (i_this->mAlpha > 0) {
            break;
        }
        if (i_this->mHasChildActor != 0) {
            i_this->mTimers[1] = (int)(100.0f + cM_rndF(100.0f));
        }
        actor->scale.setall(0.0f);
        i_this->mAlpha = 0;
        i_this->mMode++;
        // Fall-through
    }
    case 8:
        if (i_this->mTimers[1] != 0) {
            break;
        }
        if (i_this->mPathIndex != 0xFF && i_this->mpRoomPath != NULL) {
            i_this->mPathPointIdx = (int)cM_rndF(i_this->mpRoomPath->m_num);
            if (i_this->mPathPointIdx == i_this->mpRoomPath->m_num) {
                i_this->mPathPointIdx--;
            }
            actor->current.pos = i_this->mpRoomPath->m_points[i_this->mPathPointIdx].m_position;
            actor->old.pos = actor->current.pos;
            i_this->mHomePos = actor->current.pos;
        } else {
            actor->current.pos.x = i_this->mHomePos.x + cM_rndFX(200.0f);
            actor->current.pos.y = i_this->mHomePos.y;
            actor->current.pos.z = i_this->mHomePos.z + cM_rndFX(200.0f);
        }
        i_this->mRodScale.setall(0.0f);
        i_this->mMode = 0;
        break;
    }

    if (i_this->m3F8 != 1 && i_this->mMode < 4) {
        i_this->mTargetAngleY = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    }
    body_atari_check(i_this);
}

/* 00002528-00002E3C       .text action_itai__FP8wz_class */
void action_itai(wz_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz pos = actor->current.pos;
#if VERSION == VERSION_DEMO
    pos.y += 160.0f + REG12_F(17);
#endif

    switch (i_this->mMode) {
    case 0xA:
        i_this->mTargetAngleY = player->shape_angle.y;
        actor->current.angle.y = i_this->mTargetAngleY;
        actor->shape_angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        i_this->mParticleCb2.remove();
        i_this->mParticleCb3.remove();
        i_this->mParticleCb4.remove();
        for (int i = 0; i < 4; i++) {
            i_this->m3DE[i] = 0;
        }
        switch (i_this->m34E) {
        case 7:
            actor->speedF = 0.0f;
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            i_this->mRodScale.setall(0.0f);
            actor->attention_info.flags = 0;
            anm_init(i_this, dRes_INDEX_WZ_BCK_PRESS1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode = 0x14;
            break;
        case 1:
        case 8:
            anm_init(i_this, dRes_INDEX_WZ_BCK_DAMAGE1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            actor->speedF = 48.0f;
            i_this->mMode = 0xB;
            break;
        case 3:
            anm_init(i_this, dRes_INDEX_WZ_BCK_BATABATA1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            actor->speedF = 32.0f;
            i_this->mMode = 0xC;
            break;
        case 4:
            enemy_piyo_set(actor);
            fopAcM_seStart(actor, JA_SE_CM_MD_PIYO, 0);
            anm_init(i_this, dRes_INDEX_WZ_BCK_KIZETU1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            actor->speedF = 28.0f;
            i_this->mTimers[0] = 0x4B;
            i_this->mMode = 0xD;
            break;
        case 9:
            anm_init(i_this, dRes_INDEX_WZ_BCK_KIZETU1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            actor->speedF = 28.0f;
            i_this->mMode = 0xC;
            break;
        default:
            anm_init(i_this, dRes_INDEX_WZ_BCK_DAMAGE1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            actor->speedF = 32.0f;
            i_this->mMode = 0xB;
            break;
        }
        if (i_this->m34E != 7 && actor->health <= 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_WZ_DIE, 0);
            i_this->mMode = 0x28;
            if (i_this->mIsMiniBoss) {
                i_this->mAction = 3;
                i_this->mMode = 0x46;
            }
        } else {
            fopAcM_monsSeStart(actor, JA_SE_CV_WZ_DAMAGE, 0);
        }
        break;
    case 0xB:
        cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
        if (std::fabsf(actor->speedF) < 0.2f) {
            actor->speedF = 0.0f;
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            i_this->mAction = 0;
            i_this->mMode = 6;
        }
        break;
    case 0xC:
        cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
        if (i_this->mpMorf->isStop()) {
            actor->speedF = 0.0f;
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            i_this->mAction = 0;
            i_this->mMode = 6;
        }
        break;
    case 0xD:
        cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
        if (i_this->mTimers[0] == 0) {
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            i_this->mAction = 0;
            i_this->mMode = 6;
        }
        break;
    case 0x14:
        if (i_this->mpMorf->isStop()) {
            if (actor->health <= 0) {
                fopAcM_monsSeStart(actor, JA_SE_CV_WZ_DIE, 0);
                i_this->mMode = 0x2A;
                if (i_this->mIsMiniBoss) {
                    i_this->mAction = 3;
                    i_this->mMode = 0x46;
                }
            } else {
                anm_init(i_this, dRes_INDEX_WZ_BCK_PRESS2_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                i_this->mMode = 0x15;
            }
        }
        break;
    case 0x15:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_WZ_BCK_PRESS3_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode = 0x16;
        }
        break;
    case 0x16:
        i_this->mAlpha -= 8;
        if (i_this->mAlpha < 0) {
            i_this->mAlpha = 0;
        }
        if (i_this->mpMorf->isStop()) {
            i_this->mAlpha = 0;
            i_this->mAction = 0;
            i_this->mMode = 6;
        }
        break;
    case 0x28:
        i_this->mTargetAngleY = player->shape_angle.y;
        actor->current.angle.y = i_this->mTargetAngleY;
        actor->shape_angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        actor->speedF = 40.0f;
#if VERSION == VERSION_DEMO
        if (REG12_F(13) + (100.0f + i_this->mAcch.GetGroundH()) > actor->current.pos.y) {
            anm_init(i_this, dRes_INDEX_WZ_BCK_DOWN1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        } else {
            anm_init(i_this, dRes_INDEX_WZ_BCK_AIRDOWN1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        }
#else
        anm_init(i_this, dRes_INDEX_WZ_BCK_AIRDOWN1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
#endif
        i_this->mMode++;
        // Fall-through
    case 0x29:
        rod_size_set(i_this, 1);
        cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        actor->speedF = 0.0f;
        i_this->mMode++;
        // Fall-through
    case 0x2A:
        if (i_this->mHasChildActor == 0) {
#if VERSION == VERSION_DEMO
            fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
            fopAcM_delete(actor);
            if (REG12_S(2) == 0) {
                if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                    dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(actor));
                }
                if (strcmp(dComIfGp_getStartStageName(), "kazeMB") == 0 && ++come_flag >= 2) {
                    mDoAud_subBgmStop();
                }
                fopAcM_onActor(actor);
            }
#else
            if (i_this->mBckIdx == dRes_INDEX_WZ_BCK_AIRDOWN1_e) {
                pos.y += 160.0f;
                fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
            } else {
                pos.y += 20.0f;
                fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
            }
            fopAcM_delete(actor);
            if (strcmp(dComIfGp_getStartStageName(), "kazeMB") != 0) {
                if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                    dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(actor));
                }
                fopAcM_onActor(actor);
            }
#endif
        } else {
            actor->speedF = 0.0f;
            i_this->mAlpha = 0;
#if VERSION == VERSION_DEMO
            fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
#else
            if (i_this->mBckIdx == dRes_INDEX_WZ_BCK_AIRDOWN1_e) {
                pos.y += 160.0f;
                fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
            } else {
                pos.y += 20.0f;
                fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
            }
#endif
            actor->attention_info.flags = 0;
            fopAcM_OffStatus(actor, fopAcStts_SHOWMAP_e);
            actor->scale.setall(0.0f);
            i_this->mRodScale.setall(0.0f);
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            i_this->mMode++;
        }
        break;
    case 0x2B:
        if (i_this->mHasChildActor == 0) {
            fopAcM_delete(actor);
#if VERSION == VERSION_DEMO
            if (REG12_S(2) == 0) {
                if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                    dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(actor));
                }
                if (strcmp(dComIfGp_getStartStageName(), "kazeMB") == 0 && ++come_flag >= 2) {
                    mDoAud_subBgmStop();
                }
                fopAcM_onActor(actor);
            }
#else
            if (strcmp(dComIfGp_getStartStageName(), "kazeMB") != 0) {
                if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                    dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(actor));
                }
                fopAcM_onActor(actor);
            }
#endif
        }
        break;
    }

    if (actor->health > 0) {
        body_atari_check(i_this);
    }
}

/* 00002E3C-000047C8       .text action_demo__FP8wz_class */
void action_demo(wz_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player_ac = daPy_getPlayerActorClass();
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    cXyz mtx_off;
    cXyz dst;
    cXyz offset;
    cXyz pos = i_this->current.pos;
    cXyz scale;
    f32 mag;
    pos.y += 160.0f + REG12_F(17);

    switch (i_this->mMode) {
    case 0x32:
        if (fopAcM_searchActorDistance(i_this, player_ac) > 1250.0f + REG11_F(1)) {
            break;
        }
        actor->eyePos = player_ac->current.pos;
        fopAcM_seStart(actor, JA_SE_CM_WZ_APPEAR, 0);
        i_this->mTargetAngleY = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        actor->current.angle.y = i_this->mTargetAngleY;
        actor->shape_angle.y = i_this->mTargetAngleY;
        mDoAud_bgmAllMute(0x1E);
        actor->current.pos.setall(0.0f);
        i_this->mHomePos = actor->current.pos;
        i_this->mMode = 0x46;
        break;
    case 0x3C:
        switch (i_this->m3DE[0]) {
        case 0:
            if (come_flag == 0) {
                break;
            }
            anm_init(i_this, dRes_INDEX_WZ_BCK_SYUTUGEN1_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            actor->current.pos.x = -250.0f;
            actor->current.pos.y = 20.0f;
            actor->current.pos.z = 100.0f;
            i_this->mTargetAngleY = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            actor->current.angle.y = i_this->mTargetAngleY;
            actor->shape_angle.y = i_this->mTargetAngleY;
            come_flag = 0;
            i_this->m3DE[0]++;
            break;
        case 1:
            i_this->mAlpha += 8;
            if (i_this->mAlpha <= 0xFF) {
                break;
            }
            i_this->mAlpha = 0xFF;
            i_this->m3DE[0]++;
            break;
        case 2:
            if (come_flag == 0) {
                break;
            }
            come_flag = 0;
            fopAcM_OffStatus(actor, fopAcStts_UNK4000_e);
            i_this->mAction = 0;
            i_this->mMode = 6;
            break;
        }
        break;
    case 0x46:
        fopAcM_OnStatus(actor, fopAcStts_UNK4000_e);
        if (!actor->eventInfo.checkCommandDemoAccrpt()) {
            dComIfGp_event_onEventFlag(dEvtFlag_NOPARTNER_e);
            fopAcM_orderPotentialEvent(actor, 0, 0xFFFF, 0);
            actor->eventInfo.onCondition(dEvtCnd_UNK2_e);
            break;
        }
        player->changeOriginalDemo();
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(2);
        i_this->mCamFov = 50.0f;
        if (actor->health <= 0) {
            actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
            actor->shape_angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            for (int i = 0; i < 20; i++) {
                if (i_this->mChildAlive[i] != 0) {
                    fopAc_ac_c* child = fopAcM_SearchByID(i_this->mChildIds[i]);
                    if (child != NULL && !fopAcM_CheckStatus(child, fopAcStts_UNK4000_e)) {
                        fopAcM_OffStatus(child, fopAcStts_UNK4000_e);
                        i_this->mChildHidden[i] = 1;
                    }
                }
            }
            s16 ang = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            player->setPlayerPosAndAngle(
                &player_ac->current.pos,
                DEMO_SELECT(ang + 0x8000, (s16)(ang + 0x8000))
            );
            if (i_this->mBckIdx != dRes_INDEX_WZ_BCK_PRESS1_e) {
                anm_init(i_this, DEMO_SELECT(dRes_INDEX_WZ_BCK_DOWN1_e, dRes_INDEX_WZ_BCK_AIRDOWN1_e), 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                actor->speedF = 40.0f;
                i_this->mMode = 0x5A;
            } else {
                i_this->mMode = 0x5B;
            }
        } else {
            player->changeDemoMode(daPy_demo_c::DEMO_SURPRISED_e);
            i_this->mTimers[0] = (s16)(45.0f + REG11_F(2));
            actor->current.pos.setall(0.0f);
            i_this->mMode = 0x50;
        }
        break;
    case 0x50:
        if (i_this->mTimers[0] != 0) {
            break;
        }
        cLib_addCalc2(&i_this->mCamFov, 50.0f + REG11_F(3), 1.0f, 0.5f + REG12_F(14));
        offset.x = 50.0f;
        offset.y = 0.0f;
        offset.z = 850.0f;
        {
            s16 ang = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            player->setPlayerPosAndAngle(
                &player_ac->current.pos,
                DEMO_SELECT(ang + 0x8000, (s16)(ang + 0x8000))
            );
        }
        i_this->mCamEye.x = REG11_F(4);
        i_this->mCamEye.y = 125.0f + REG11_F(5);
        i_this->mCamEye.z = -29.0f + REG11_F(6);
        i_this->mCamCenter.x = 1470.0f + REG11_F(7);
        i_this->mCamCenter.y = 917.0f + REG11_F(8);
        i_this->mCamCenter.z = -222.0f + REG11_F(9);
        i_this->mTimers[0] = (s16)(20.0f + REG11_F(10));
        anm_init(i_this, dRes_INDEX_WZ_BCK_S_DEMO_WAIT1_e, 0.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->mMode++;
        // Fall-through
    case 0x51:
        if (REG0_S(8) != 0) {
            i_this->mCamEye.x = REG11_F(4);
            i_this->mCamEye.y = 125.0f + REG11_F(5);
            i_this->mCamEye.z = -29.0f + REG11_F(6);
            i_this->mCamCenter.x = 1470.0f + REG11_F(7);
            i_this->mCamCenter.y = 917.0f + REG11_F(8);
            i_this->mCamCenter.z = -222.0f + REG11_F(9);
            i_this->mTimers[0] = (s16)(20.0f + REG11_F(10));
            break;
        }
        if (i_this->mTimers[0] == 0) {
            switch (i_this->m3DE[0]) {
            case 0:
                player->changeDemoMode(daPy_demo_c::DEMO_L_AROUND2_e);
                i_this->mTimers[0] = (s16)(60.0f + REG11_F(11));
#if VERSION == VERSION_DEMO
                cMtx_YrotS(*calc_mtx, player_ac->shape_angle.y);
                mtx_off.x = 0.0f;
                mtx_off.y = 0.0f;
                mtx_off.z = 40000.0f + REG11_F(12);
#else
                mDoMtx_YrotS(*calc_mtx, player_ac->shape_angle.y);
                mtx_off.set(0.0f, 0.0f, 40000.0f + REG11_F(12));
#endif
                MtxPosition(&mtx_off, &dst);
                actor->eyePos = dst + player_ac->current.pos;
                fopAcM_seStart(actor, JA_SE_CM_WZ_APPEAR, 0);
                i_this->m3DE[0]++;
                break;
            case 1:
                player->changeDemoMode(daPy_demo_c::DEMO_TBACK_e);
                i_this->mTimers[0] = (s16)(45.0f + REG11_F(13));
#if VERSION == VERSION_DEMO
                cMtx_YrotS(*calc_mtx, player_ac->shape_angle.y + 0x8000);
                mtx_off.x = 0.0f;
                mtx_off.y = 0.0f;
                mtx_off.z = 40000.0f + REG11_F(14);
#else
                mDoMtx_YrotS(*calc_mtx, (s16)(player_ac->shape_angle.y + 0x8000));
                mtx_off.set(0.0f, 0.0f, 40000.0f + REG11_F(14));
#endif
                MtxPosition(&mtx_off, &dst);
                actor->eyePos = dst + player_ac->current.pos;
                fopAcM_seStart(actor, JA_SE_CM_WZ_APPEAR, 0);
                i_this->m3DE[0]++;
                break;
            }
        }
        cLib_addCalc2(&i_this->mCamFov, 50.0f + REG11_F(15), 1.0f, 0.5f + REG12_F(14));
        mag = std::fabsf(i_this->mCamEye.x - (150.0f + REG11_F(16)));
        mag = mag * (0.01f + REG11_F(19));
        cLib_addCalc2(&i_this->mCamEye.x, 150.0f + REG11_F(16), 1.0f, mag);
        mag = std::fabsf(i_this->mCamEye.y - (158.0f + REG11_F(17)));
        mag = mag * (0.01f + REG11_F(19));
        cLib_addCalc2(&i_this->mCamEye.y, 158.0f + REG11_F(17), 1.0f, mag);
        mag = std::fabsf(i_this->mCamEye.z - (114.0f + REG11_F(18)));
        mag = mag * (0.01f + REG11_F(19));
        cLib_addCalc2(&i_this->mCamEye.z, 114.0f + REG11_F(18), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.x - (f32)(REG11_S(0) + 0x492));
        mag = mag * (0.01f + REG11_F(19));
        cLib_addCalc2(&i_this->mCamCenter.x, (f32)(REG11_S(0) + 0x492), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.y - (f32)(REG11_S(1) + 0x2CD));
        mag = mag * (0.01f + REG11_F(19));
        cLib_addCalc2(&i_this->mCamCenter.y, (f32)(REG11_S(1) + 0x2CD), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.z - (f32)(REG11_S(2) - 0x336));
        mag = mag * (0.01f + REG11_F(19));
        cLib_addCalc2(&i_this->mCamCenter.z, (f32)(REG11_S(2) - 0x336), 1.0f, mag);
        if (i_this->m3DE[0] >= 2) {
            if (i_this->mTimers[0] <= (s16)(REG11_S(3) + 0x14)) {
                if (i_this->m3DE[0] == 2) {
                    fopAcM_seStart(actor, JA_SE_CM_WZ_APPEAR, 0);
                    i_this->m3DE[0] = 3;
                }
                i_this->mAlpha += (s16)(REG11_S(4) + 0xA);
                if (i_this->mAlpha > 0xFF) {
                    i_this->mAlpha = 0xFF;
                }
            }
            if (REG0_S(9) == 0 && i_this->mTimers[0] == 0) {
                player->changeDemoMode(daPy_demo_c::DEMO_SURPRISED_e);
                i_this->mTimers[0] = REG11_S(5) + 0x1E;
                i_this->mMode++;
            }
        }
        break;
    case 0x52:
        i_this->mAlpha += (s16)(REG11_S(6) + 5);
        if (i_this->mAlpha > 0xFF) {
            i_this->mAlpha = 0xFF;
        }
        if (i_this->mTimers[0] == 0) {
            i_this->mMode++;
        }
        break;
    case 0x53:
        i_this->mHomePos.x = pos.x;
        i_this->mHomePos.y = actor->current.pos.y;
        i_this->mHomePos.z = actor->current.pos.z;
        i_this->mHomePos.y += 15.0f;
        anm_init(i_this, dRes_INDEX_WZ_BCK_S_DEMO1_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode++;
        // Fall-through
    case 0x54: {
        i_this->mTargetAngleY = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        actor->current.angle.y = i_this->mTargetAngleY;
        actor->shape_angle.y = i_this->mTargetAngleY;
        dst.x = 50.0f;
        dst.y = 0.0f;
        dst.z = 850.0f;
        player->setPlayerPosAndAngle(&dst, fopAcM_searchActorAngleY(player_ac, actor));
        cLib_addCalc2(&i_this->mCamFov, 50.0f + REG8_F(1), 1.0f, 0.5f + REG12_F(14));
        i_this->mCamEye.x = -108.0f + REG8_F(2);
        i_this->mCamEye.y = 174.0f + REG8_F(3);
        i_this->mCamEye.z = 231.0f + REG8_F(4);
        i_this->mCamCenter.x = 199.0f + REG8_F(5);
        i_this->mCamCenter.y = 50.0f + REG8_F(6);
        i_this->mCamCenter.z = 1092.0f + REG8_F(7);
        i_this->mAlpha += (s16)(5.0f + REG8_F(8));
        if (i_this->mAlpha < 0xFF) {
            break;
        }
        i_this->mAlpha = 0xFF;
        if (REG0_S(9) != 0) {
            i_this->mpMorf->setPlaySpeed(0.0f);
            break;
        }
        i_this->mTimers[0] = (s16)REG12_F(12);
        i_this->mpMorf->setPlaySpeed(1.0f);
        i_this->mMode++;
        // Fall-through
    }
    case 0x55:
        if (i_this->mTimers[0] != 0) {
            break;
        }
        i_this->mpMorf->setPlaySpeed(1.0f);
        i_this->mMode++;
        // Fall-through
    case 0x56:
        cLib_addCalc2(&i_this->mCamFov, 45.0f + REG8_F(10), 1.0f, 0.2f + REG12_F(15));
        mag = std::fabsf(i_this->mCamEye.x - REG8_F(11));
        mag = mag * (0.025f + REG8_F(17));
        cLib_addCalc2(&i_this->mCamEye.x, REG8_F(11), 1.0f, mag);
        mag = std::fabsf(i_this->mCamEye.y - (135.0f + REG8_F(12)));
        mag = mag * (0.025f + REG8_F(17));
        cLib_addCalc2(&i_this->mCamEye.y, 135.0f + REG8_F(12), 1.0f, mag);
        mag = std::fabsf(i_this->mCamEye.z - (-60.0f + REG8_F(13)));
        mag = mag * (0.025f + REG8_F(17));
        cLib_addCalc2(&i_this->mCamEye.z, -60.0f + REG8_F(13), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.x - (50.0f + REG8_F(14)));
        mag = mag * (0.025f + REG8_F(17));
        cLib_addCalc2(&i_this->mCamCenter.x, 50.0f + REG8_F(14), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.y - (110.0f + REG8_F(15)));
        mag = mag * (0.025f + REG8_F(17));
        cLib_addCalc2(&i_this->mCamCenter.y, 110.0f + REG8_F(15), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.z - (330.0f + REG8_F(16)));
        mag = mag * (0.025f + REG8_F(17));
        cLib_addCalc2(&i_this->mCamCenter.z, 330.0f + REG8_F(16), 1.0f, mag);
        if (i_this->mpMorf->checkFrame(159.0f)) {
            if (REG0_S(9) != 0) {
                i_this->mpMorf->setPlaySpeed(0.0f);
            } else {
                actor->speedF = -(10.0f + REG12_F(10));
                i_this->m3DE[0] = 0;
                i_this->m3DE[1] = 0;
                i_this->mTimers[0] = 2;
                i_this->mMode++;
            }
        }
        break;
    case 0x57:
        cLib_addCalc0(&actor->speedF, 1.0f, 0.3f + REG12_F(11));
        if (REG8_S(4) != 0) {
            i_this->mTimers[0] = 1;
            REG8_S(4) = 0;
        }
        if (i_this->m3DE[0] == 0 && i_this->mTimers[0] == 0) {
            scale = actor->scale;
#if VERSION == VERSION_DEMO
            u32 params = 0xFFFFFF00;
            params |= 0xD;
#endif
            scale.setall(4.0f);
            offset.x = 215.0f;
            offset.y = 180.0f;
            offset.z = 100.0f;
            fpc_ProcID id = fopAcM_createChild(
                fpcNm_WZ_e,
                fopAcM_GetID(actor),
                DEMO_SELECT(params, 0xFFFFFF0D),
                &offset,
                fopAcM_GetRoomNo(actor),
                &actor->current.angle,
                &scale,
                0
            );
            if (id == fpcM_ERROR_PROCESS_ID_e) {
                break;
            }
            offset.x = -250.0f;
            offset.y = 180.0f;
            offset.z = 100.0f;
            id = fopAcM_createChild(
                fpcNm_WZ_e,
                fopAcM_GetID(actor),
                DEMO_SELECT(params, 0xFFFFFF0D),
                &offset,
                fopAcM_GetRoomNo(actor),
                &actor->current.angle,
                &scale,
                0
            );
            if (id == fpcM_ERROR_PROCESS_ID_e) {
                break;
            }
            i_this->m3DE[0] = 1;
            i_this->mTimers[1] = 0x28;
            i_this->mTimers[2] = (s16)(70.0f + REG12_F(9));
        }
        if (i_this->mTimers[2] == 1) {
            mDoAud_subBgmStart(JA_BGM_MBOSS);
        }
        if (i_this->m3DE[0] != 0 && i_this->m3DE[1] == 0 && i_this->mTimers[1] == 0) {
            offset.x = 215.0f;
            offset.y = 0.0f;
            offset.z = 100.0f;
            i_this->mRelatedId = fopAcM_create(
                fpcNm_TN_e,
                0xFFFFFF2C,
                &offset,
                fopAcM_GetRoomNo(actor),
                &actor->current.angle,
                NULL,
                -1,
                NULL
            );
            if (i_this->mRelatedId != fpcM_ERROR_PROCESS_ID_e) {
                i_this->m3DE[1] = 1;
                come_flag = 1;
            }
        }
        if (i_this->mRelatedId != fpcM_ERROR_PROCESS_ID_e) {
            fopAc_ac_c* related = fopAcM_SearchByID(i_this->mRelatedId);
            if (related != NULL) {
                related->speedF = 0.0f;
                fopAcM_OnStatus(related, fopAcStts_UNK4000_e);
            }
        }
        cLib_addCalc2(&i_this->mCamFov, 65.0f + REG12_F(0), 1.0f, 2.5f + REG12_F(16));
        mag = std::fabsf(i_this->mCamEye.x - REG12_F(1));
        mag = mag * (0.3f + REG12_F(7));
        cLib_addCalc2(&i_this->mCamEye.x, REG12_F(1), 1.0f, mag);
        mag = std::fabsf(i_this->mCamEye.y - (90.0f + REG12_F(2)));
        mag = mag * (0.3f + REG12_F(7));
        cLib_addCalc2(&i_this->mCamEye.y, 90.0f + REG12_F(2), 1.0f, mag);
        mag = std::fabsf(i_this->mCamEye.z - (440.0f + REG12_F(3)));
        mag = mag * (0.3f + REG12_F(7));
        cLib_addCalc2(&i_this->mCamEye.z, 440.0f + REG12_F(3), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.x - REG12_F(4));
        mag = mag * (0.3f + REG12_F(7));
        cLib_addCalc2(&i_this->mCamCenter.x, REG12_F(4), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.y - (50.0f + REG12_F(5)));
        mag = mag * (0.3f + REG12_F(7));
        cLib_addCalc2(&i_this->mCamCenter.y, 50.0f + REG12_F(5), 1.0f, mag);
        mag = std::fabsf(i_this->mCamCenter.z - (570.0f + REG12_F(6)));
        mag = mag * (0.3f + REG12_F(7));
        cLib_addCalc2(&i_this->mCamCenter.z, 570.0f + REG12_F(6), 1.0f, mag);
        if (!i_this->mpMorf->isStop()) {
            if (!i_this->mpMorf->checkFrame(274.0f + REG12_F(8))) {
                break;
            }
        }
        if (REG0_S(9) != 0) {
            break;
        }
        dComIfGp_getVibration().StopQuake(0x20);
        camera->mCamera.Reset(i_this->mCamEye, i_this->mCamCenter);
        camera->mCamera.Start();
        camera->mCamera.SetTrimSize(0);
        player->cancelOriginalDemo();
        dComIfGp_event_reset();
        fopAcM_monsSeStart(actor, JA_SE_CV_WZ_LAUGH, 0);
        {
            fopAc_ac_c* related = fopAcM_SearchByID(i_this->mRelatedId);
            if (related != NULL) {
                fopAcM_OffStatus(related, fopAcStts_UNK4000_e);
            }
        }
        if (REG12_S(9) == 0) {
            i_this->mHasChildActor = 1;
            i_this->m352 = 1;
        } else {
            i_this->mRelatedId = fpcM_ERROR_PROCESS_ID_e;
        }
        come_flag = 1;
        fopAcM_OffStatus(actor, fopAcStts_UNK4000_e);
        i_this->mAction = 0;
        i_this->mMode = 6;
        break;
    case 0x5A:
        rod_size_set(i_this, 1);
        {
            s16 ang = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            player->setPlayerPosAndAngle(
                &player_ac->current.pos,
                DEMO_SELECT(ang + 0x8000, (s16)(ang + 0x8000))
            );
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        actor->speedF = 0.0f;
        i_this->mMode++;
        // Fall-through
    case 0x5B:
        if (REG12_S(4) != 0) {
            break;
        }
        for (int i = 0; i < 20; i++) {
            if (i_this->mChildHidden[i] != 0) {
                fopAc_ac_c* child = fopAcM_SearchByID(i_this->mChildIds[i]);
                if (child != NULL) {
                    fopAcM_OnStatus(child, fopAcStts_UNK4000_e);
                }
                i_this->mChildHidden[i] = 0;
            }
        }
        fopAcM_createDisappear(actor, &pos, 5, 0, actor->stealItemBitNo);
        actor->speedF = 0.0f;
        i_this->mAlpha = 0;
        actor->attention_info.flags = 0;
        fopAcM_OffStatus(actor, fopAcStts_SHOWMAP_e);
        actor->scale.setall(0.0f);
        i_this->mRodScale.setall(0.0f);
        i_this->mBodyCyl.OffTgSetBit();
        i_this->mBodyCyl.ClrTgHit();
        i_this->mTimers[0] = REG12_S(3) + 0x1E;
        i_this->mMode++;
        // Fall-through
    case 0x5C:
        if (i_this->mTimers[0] != 0) {
            break;
        }
        dComIfGp_getVibration().StopQuake(0x20);
        camera->mCamera.Reset(i_this->mCamEye, i_this->mCamCenter);
        camera->mCamera.Start();
        camera->mCamera.SetTrimSize(0);
        player->cancelOriginalDemo();
        dComIfGp_event_reset();
#if VERSION == VERSION_DEMO
        if (i_this->mHasChildActor == 0) {
            fopAcM_delete(actor);
            if (REG12_S(2) == 0) {
                if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                    dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(actor));
                }
                if (strcmp(dComIfGp_getStartStageName(), "kazeMB") == 0 && ++come_flag >= 2) {
                    mDoAud_subBgmStop();
                }
                fopAcM_onActor(actor);
            }
        } else {
#else
        if (strcmp(dComIfGp_getStartStageName(), "kazeMB") == 0) {
            if (i_this->mIsMiniBoss != 0) {
                mDoAud_subBgmStop();
            }
        }
        if (i_this->mHasChildActor == 0) {
            fopAcM_delete(actor);
            if (strcmp(dComIfGp_getStartStageName(), "kazeMB") != 0) {
                if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                    dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(actor));
                }
                fopAcM_onActor(actor);
            }
        } else {
#endif
            i_this->mAction = 1;
            i_this->mMode = 0x2B;
        }
        break;
    }

    if (actor->health <= 0) {
        i_this->mCamEye = actor->current.pos;
        i_this->mCamEye.y += 50.0f;
        mDoMtx_YrotS(*calc_mtx, fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)));
        mtx_off.set(0.0f, 0.0f, 400.0f);
        MtxPosition(&mtx_off, &dst);
        dst += actor->current.pos;
        i_this->mCamCenter.x = dst.x - 100.0f;
        i_this->mCamCenter.y = 400.0f + dst.y;
        i_this->mCamCenter.z = dst.z - 300.0f;
    }
    if (i_this->mBckIdx == dRes_INDEX_WZ_BCK_S_DEMO1_e) {
        if (i_this->mpMorf->checkFrame(70.0f)) {
            fopAcM_monsSeStart(actor, JA_SE_CV_WZ_LAUGH, 0);
        }
    }
    s8 health = actor->health;
    if ((health > 0 && i_this->mMode >= 0x51) || (health <= 0 && i_this->mMode >= 0x5A)) {
        camera->mCamera.Set(i_this->mCamEye, i_this->mCamCenter, i_this->mCamFov, 0);
    }
}

/* 000047C8-00004810       .text next_tama_move__FP8wz_class4cXyz */
void next_tama_move(wz_class* i_this, cXyz pos) {
    i_this->mTamaTarget = pos;
    i_this->speedF = 0.0f;
    i_this->mBallSph.OffAtSPrmBit(cCcD_AtSPrm_Set_e);
    i_this->mBallSph.ClrAtSet();
    i_this->mMode = 0x68;
}

/* 00004810-00004BF4       .text summon_call_sub__FP8wz_class */
void summon_call_sub(wz_class* i_this) {
    static s16 enemy_name_dt[] = {
        fpcNm_KI_e, 0x7FFF, fpcNm_MT_e, 0x7FFF, fpcNm_MT_e, fpcNm_KI_e, fpcNm_KI_e, 0x7FFF,
        fpcNm_KI_e, 0x7FFF, fpcNm_BL_e, 0x7FFF, fpcNm_KI_e, 0x7FFF, fpcNm_BL_e, fpcNm_KI_e,
        fpcNm_BL_e, 0x7FFF, fpcNm_BL_e, 0x7FFF, fpcNm_BL_e, 0x7FFF, fpcNm_BL_e, 0x7FFF,
        fpcNm_KS_e, 0x7FFF, fpcNm_KS_e, 0x7FFF, fpcNm_KS_e, 0x7FFF, fpcNm_KS_e, 0x7FFF,
        fpcNm_CC_e, 0x7FFF, fpcNm_CC_e, 0x7FFF, fpcNm_CC_e, 0x7FFF, fpcNm_CC_e, 0x7FFF,
        fpcNm_BL_e, 0x7FFF, fpcNm_BL_e, 0x7FFF, fpcNm_BL_e, fpcNm_BL_e, fpcNm_PW_e, fpcNm_BL_e,
        fpcNm_ST_e, 0x7FFF, fpcNm_RD_e, 0x7FFF, fpcNm_ST_e, 0x7FFF, fpcNm_RD_e, 0x7FFF,
        fpcNm_MO2_e, 0x7FFF, fpcNm_TN_e, 0x7FFF, fpcNm_MO2_e, 0x7FFF, fpcNm_TN_e, 0x7FFF,
        fpcNm_PH_e, 0x7FFF, fpcNm_KI_e, 0x7FFF, fpcNm_PH_e, 0x7FFF, fpcNm_BB_e, 0x7FFF,
        fpcNm_KI_e, 0x7FFF, fpcNm_MT_e, 0x7FFF, fpcNm_MT_e, fpcNm_KI_e, fpcNm_KI_e, 0x7FFF,
        fpcNm_KI_e, 0x7FFF, fpcNm_BL_e, 0x7FFF, fpcNm_KI_e, 0x7FFF, fpcNm_BL_e, fpcNm_KI_e,
        fpcNm_KS_e, 0x7FFF, fpcNm_KS_e, 0x7FFF, fpcNm_KS_e, 0x7FFF, fpcNm_KS_e, 0x7FFF,
        fpcNm_CC_e, 0x7FFF, fpcNm_CC_e, 0x7FFF, fpcNm_CC_e, 0x7FFF, fpcNm_CC_e, 0x7FFF,
        fpcNm_BK_e, 0x7FFF, fpcNm_BK_e, fpcNm_MO2_e, fpcNm_BK_e, fpcNm_BK_e, fpcNm_BK_e, fpcNm_MO2_e,
        fpcNm_BB_e, 0x7FFF, fpcNm_KI_e, 0x7FFF, fpcNm_CC_e, fpcNm_CC_e, fpcNm_KI_e, fpcNm_KS_e,
    };
    static u32 enemy_arg_dt[] = {
        0xFFFFFF03, 0x00000000, 0xFFFFFF00, 0x00000000, 0xFFFFFF00, 0xFFFFFF03, 0xFFFFFF03, 0x00000000,
        0xFFFFFF03, 0x00000000, 0x0100FF00, 0x00000000, 0xFFFFFF03, 0x00000000, 0x0100FF00, 0xFFFFFF03,
        0x0100FF01, 0x00000000, 0x0100FF01, 0x00000000, 0x0100FF01, 0x00000000, 0x0100FF01, 0x00000000,
        0x00000003, 0x00000000, 0x00000003, 0x00000000, 0x00000003, 0x00000000, 0x00000003, 0x00000000,
        0xFFFF0000, 0x00000000, 0xFFFF0000, 0x00000000, 0xFFFF0000, 0x00000000, 0xFFFF0000, 0x00000000,
        0x0100FF00, 0x00000000, 0x0100FF00, 0x00000000, 0x0100FF01, 0x0100FF00, 0xFF000000, 0x0100FF01,
        0xFFFFFF00, 0x00000000, 0xFFFFFFFE, 0x00000000, 0xFFFFFF00, 0x00000000, 0xFFFFFFFE, 0x00000000,
        0xFFFFFF00, 0x00000000, 0xFFFFFF20, 0x00000000, 0xFFFFFF00, 0x00000000, 0xFFFFFF20, 0x00000000,
        0xFFF0F000, 0x00000000, 0xFFFF0003, 0x00000000, 0xFFF0F000, 0x00000000, 0xFFFFFF03, 0x00000000,
        0xFFFFFF03, 0x00000000, 0xFFFFFF00, 0x00000000, 0xFFFFFF00, 0xFFFFFF03, 0xFFFFFF03, 0x00000000,
        0xFFFFFF03, 0x00000000, 0x0100FF00, 0x00000000, 0xFFFFFF03, 0x00000000, 0x0100FF00, 0xFFFFFF03,
        0x00000003, 0x00000000, 0x00000003, 0x00000000, 0x00000003, 0x00000000, 0x00000003, 0x00000000,
        0xFFFF0000, 0x00000000, 0xFFFF0000, 0x00000000, 0xFFFF0000, 0x00000000, 0xFFFF0000, 0x00000000,
        0xFFFFFF00, 0x00000000, 0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF00, 0xFFFFFF20, 0xFFFFFF00, 0xFFFFFF00,
        0xFFFFFF03, 0x00000000, 0xFFFFFF03, 0x00000000, 0xFFFF0A00, 0xFFFF0B00, 0xFFFF0003, 0x00000003,
    };
    static u32 birth_dt[] = {
        8, 0, 2, 0, 1, 3, 6, 0,
        8, 0, 2, 0, 6, 0, 1, 3,
        2, 0, 1, 0, 1, 0, 1, 0,
        20, 0, 15, 0, 15, 0, 20, 0,
        8, 0, 10, 0, 8, 0, 8, 0,
        2, 0, 2, 0, 1, 1, 1, 2,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        2, 0, 6, 0, 3, 0, 2, 0,
        4, 0, 1, 0, 1, 2, 3, 0,
        4, 0, 1, 0, 3, 0, 1, 2,
        10, 0, 8, 0, 8, 0, 10, 0,
        4, 0, 5, 0, 4, 0, 4, 0,
        1, 0, 1, 1, 1, 1, 1, 1,
        2, 0, 4, 0, 2, 2, 4, 8,
    };
    static u32 cc_birth_dt[] = {0, 1, 4, 3};

    fopAc_ac_c* actor = i_this;
    int arg;
    int i;
    int spawned;
    int birthNum;
    cXyz pos = actor->current.pos;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* parent_ac = fopAcM_SearchByID(i_this->mRelatedId);
    wz_class* parent = (wz_class*)parent_ac;
    if (parent_ac == NULL) {
        return;
    }
#else
    wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
    if (parent == NULL) {
        return;
    }
#endif
    if (i_this->mTimers[0] != 1) {
        return;
    }

    int tableIndex = parent->mSummonTableType << 3;
    tableIndex += parent->mSummonWave << 1;
    birthNum = birth_dt[tableIndex];
    csXyz angle;
    angle.setall(0);
    angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    pos = actor->current.pos;
    if (tableIndex == 0xC0) {
        angle.x = 0x80;
    }

    for (i = 0, spawned = 0; i < 20 && spawned < birthNum; i++) {
        if (i_this->mChildIds[i] == fpcM_ERROR_PROCESS_ID_e) {
            arg = enemy_arg_dt[tableIndex];
            if (enemy_name_dt[tableIndex] == fpcNm_CC_e) {
                if ((arg & 0xA00) == 0) {
                    int rnd = (int)cM_rndF(2.99f);
                    arg |= (cc_birth_dt[rnd] + 0xA) << 8;
                }
            }
            i_this->mChildIds[i] = fopAcM_create(
                enemy_name_dt[tableIndex],
                arg,
                &pos,
                fopAcM_GetRoomNo(actor),
                &angle,
                NULL,
                -1,
                NULL
            );
            if (i_this->mChildIds[i] != fpcM_ERROR_PROCESS_ID_e) {
                i_this->mChildAlive[i] = 1;
                pos = actor->current.pos;
                spawned++;
                pos.x += cM_rndFX(100.0f);
                pos.y += cM_rndFX(100.0f);
                pos.z += cM_rndFX(100.0f);
            }
        }
    }

    parent->mSummonWave++;
    parent->mSummonWave &= 3;
    if (parent->mSummonTableType == 7 || parent->mSummonTableType == 13) {
        parent->mSummonCount++;
        if (parent->mSummonCount > 7) {
            parent->mSummonTableType = 10;
        }
    }

#if VERSION == VERSION_DEMO
    tableIndex++;
    if (enemy_name_dt[tableIndex] == 0x7FFF) {
        return;
    }
    spawned = birth_dt[tableIndex];
    angle.setall(0);
    angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    pos = actor->current.pos;

    for (i = 0, arg = 0; i < 20 && arg < spawned; i++) {
        if (i_this->mChildIds[i] == fpcM_ERROR_PROCESS_ID_e) {
            birthNum = enemy_arg_dt[tableIndex];
            if (tableIndex == fpcNm_CC_e) {
                birthNum |= 0xA00;
                int rnd = (int)cM_rndF(3.19f);
                birthNum |= cc_birth_dt[rnd] << 8;
            }
            i_this->mChildIds[i] = fopAcM_create(
                enemy_name_dt[tableIndex],
                birthNum,
                &pos,
                fopAcM_GetRoomNo(actor),
                &angle,
                NULL,
                -1,
                NULL
            );
#else
    int nextIndex = tableIndex + 1;
    if (enemy_name_dt[nextIndex] == 0x7FFF) {
        return;
    }
    spawned = birth_dt[nextIndex];
    angle.setall(0);
    angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    pos = actor->current.pos;

    for (i = 0, arg = 0; i < 20 && arg < spawned; i++) {
        if (i_this->mChildIds[i] == fpcM_ERROR_PROCESS_ID_e) {
            birthNum = enemy_arg_dt[nextIndex];
            if (tableIndex + 1 == fpcNm_CC_e) {
                birthNum |= 0xA00;
                int rnd = (int)cM_rndF(3.19f);
                birthNum |= cc_birth_dt[rnd] << 8;
            }
            i_this->mChildIds[i] = fopAcM_create(
                enemy_name_dt[nextIndex],
                birthNum,
                &pos,
                fopAcM_GetRoomNo(actor),
                &angle,
                NULL,
                -1,
                NULL
            );
#endif
            if (i_this->mChildIds[i] != fpcM_ERROR_PROCESS_ID_e) {
                i_this->mChildAlive[i] = 1;
                pos = actor->current.pos;
                arg++;
                pos.x += cM_rndFX(100.0f);
                pos.y += cM_rndFX(100.0f);
                pos.z += cM_rndFX(100.0f);
            }
        }
    }
}

/* 00004BF4-00004D18       .text sea_water_check__FP8wz_class */
BOOL sea_water_check(wz_class* i_this) {
    u8 hit = 0;
    i_this->mEffectPos = i_this->current.pos;

    if (daSea_ChkArea(i_this->current.pos.x, i_this->current.pos.z)) {
        f32 waveY = daSea_calcWave(i_this->current.pos.x, i_this->current.pos.z);
        if (i_this->current.pos.y < 50.0f + waveY) {
            hit = 1;
        }
    } else if (i_this->mAcch.MaskWaterIn()) {
        f32 waterY = i_this->mAcch.m_wtr.GetHeight();
        if (i_this->current.pos.y < 50.0f + waterY) {
            hit = 1;
        }
    }

    if (hit) {
        cXyz scale;
        scale.setall(2.0f);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_EVAPORATION00, &i_this->current.pos, &i_this->current.angle, &scale);
        return TRUE;
    }
    return FALSE;
}

/* 00004D18-00005760       .text action_tama_dousa__FP8wz_class */
void action_tama_dousa(wz_class* i_this) {
    fopAc_ac_c* actor = i_this;
    csXyz angle = actor->current.angle;
    cXyz scale = actor->scale;
    cXyz pos;
#if VERSION == VERSION_DEMO
    u32 params = 0xFFFFFF00;
#endif

    switch (i_this->mMode) {
    case 0x64:
        for (int i = 0; i < 4; i++) {
            i_this->m3DE[i] = 0;
        }
        i_this->mBallRadius = 25.0f;
        i_this->mEffectPos = actor->current.pos;
        i_this->mEffectAngle = actor->shape_angle;
        switch (i_this->mBehaviorType) {
        case WZ_TYPE_DAMAGE_BALL_FIRE:
            if (i_this->mParticleCb0.getEmitter() == NULL) {
                dComIfGp_particle_set(
                    dPa_name::ID_AK_SN_WIZFIRE00,
                    &i_this->mEffectPos,
                    &i_this->mEffectAngle,
                    NULL,
                    0xFF,
                    &i_this->mParticleCb0
                );
            }
            if (i_this->mParticleCb1.getEmitter() == NULL) {
                dComIfGp_particle_set(
                    dPa_name::ID_AK_SN_WIZFIRE01,
                    &i_this->mEffectPos,
                    &i_this->mEffectAngle,
                    NULL,
                    0xFF,
                    &i_this->mParticleCb1
                );
            }
            break;
        case WZ_TYPE_DAMAGE_BALL_ICE:
            if (i_this->mParticleCb0.getEmitter() == NULL) {
                dComIfGp_particle_set(
                    dPa_name::ID_AK_SN_WIZSUMMONSHOT00,
                    &i_this->mEffectPos,
                    &i_this->mEffectAngle,
                    NULL,
                    0xFF,
                    &i_this->mParticleCb0
                );
            }
            if (i_this->mParticleCb1.getEmitter() == NULL) {
                dComIfGp_particle_set(
                    dPa_name::ID_AK_SN_WIZSUMMONSHOT01,
                    &i_this->mEffectPos,
                    &i_this->mEffectAngle,
                    NULL,
                    0xFF,
                    &i_this->mParticleCb1
                );
            }
            break;
        }
        actor->speedF = 45.0f;
        i_this->mMode++;
        // Fall-through
    case 0x65:
        if (i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_ICE) {
            fopAcM_seStart(actor, JA_SE_OBJ_WZ_SUMMON_B_FLY, 0);
            f32 dx = i_this->mPathTarget.x - actor->current.pos.x;
            f32 dy = 300.0f + i_this->mPathTarget.y - actor->current.pos.y;
            f32 dz = i_this->mPathTarget.z - actor->current.pos.z;
            f32 dist = std::sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist < 50.0f) {
                actor->speedF = 0.0f;
                i_this->mMode = 0x66;
                break;
            }
        } else {
            fopAcM_seStart(actor, JA_SE_OBJ_WZ_FIRE_B_FLY, 0);
            if (sea_water_check(i_this)) {
                i_this->mParticleCb0.remove();
                i_this->mParticleCb1.remove();
                fopAcM_delete(actor);
            }
        }
        if (i_this->mAcch.ChkGroundHit()) {
            if (i_this->mBehaviorType != WZ_TYPE_DAMAGE_BALL_ICE && !dComIfG_Bgsp()->ChkMoveBG(i_this->mAcch.m_gnd)) {
                i_this->mBallRadius = 40.0f;
                actor->speedF = 0.0f;
                i_this->mMode = 0x66;
                break;
            }
            next_tama_move(i_this, actor->current.pos);
            break;
        }
        if (i_this->mTimers[2] == 0 || i_this->mAcch.ChkWallHit()) {
            next_tama_move(i_this, actor->current.pos);
            break;
        }
        if (i_this->mBallSph.ChkAtShieldHit() || i_this->mBallSph.ChkAtHit()) {
            pos = *i_this->mBallSph.GetAtHitPosP();
            next_tama_move(i_this, pos);
            break;
        }
        if (i_this->mBallSph.ChkTgHit()) {
            pos = *i_this->mBallSph.GetTgHitPosP();
            next_tama_move(i_this, pos);
            break;
        }
        break;
    case 0x66:
        pos = actor->current.pos;
        pos.y = i_this->mAcch.GetGroundH();
        angle.x = 0;
        angle.z = 0;
        switch (i_this->mBehaviorType) {
        case WZ_TYPE_DAMAGE_BALL_FIRE:
            dComIfGp_particle_set(dPa_name::ID_AK_SN_WIZFLOORFIRE01, &pos, &angle);
            dComIfGp_particle_set(dPa_name::ID_AK_SN_WIZFLOORFIRE02, &pos, &angle);
            dComIfGp_particle_set(dPa_name::ID_AK_SN_WIZHITFIRE00, &pos, &actor->current.angle);
            if (i_this->mTimers[1] == 0) {
                i_this->mTimers[1] = (int)cM_rndF(5.0f);
                i_this->mBallAlpha = (int)(8.0f + cM_rndF(5.0f));
            }
#if VERSION > VERSION_DEMO
            if (strcmp(dComIfGp_getStartStageName(), "sea") != 0)
#endif
            {
                dKy_arrowcol_chg_on(NULL, 0);
            }
            fopAcM_OnStatus(actor, fopAcStts_UNK4000_e);
            i_this->mTimers[0] = 0x50;
            fopAcM_seStart(actor, JA_SE_OBJ_WZ_FIRE_B_IMP, 0);
            break;
        case WZ_TYPE_DAMAGE_BALL_ICE:
#if VERSION == VERSION_DEMO
            params |= 0xC;
#endif
            scale.setall(5.0f);
            pos = actor->current.pos;
            {
                int n = 0;
                fpc_ProcID id;
                do {
                    id = fopAcM_createChild(
                        fpcNm_WZ_e,
                        fopAcM_GetID(actor),
                        DEMO_SELECT(params, 0xFFFFFF0C),
                        &pos,
                        fopAcM_GetRoomNo(actor),
                        &actor->current.angle,
                        &scale,
                        0
                    );
                    if (id != fpcM_ERROR_PROCESS_ID_e) {
                        break;
                    }
                    n++;
                } while (n < 10);
                if (id == fpcM_ERROR_PROCESS_ID_e) {
                    wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                    if (parent != NULL) {
                        parent->mHasChildActor = 0;
                    }
                }
            }
            i_this->mBallSph.OffAtSetBit();
            i_this->mBallSph.OffAtSetBit();
            i_this->mBallSph.OffTgSetBit();
            i_this->mBallSph.ClrTgHit();
            actor->scale.setall(0.0f);
            i_this->mTimers[0] = REG0_S(0) + 0x1E;
            break;
        }
        i_this->mParticleCb0.remove();
        i_this->mParticleCb1.remove();
        i_this->mMode++;
        break;
    case 0x67:
        if (i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_FIRE) {
            actor->shape_angle.y += 0x100;
            if (i_this->mTimers[0] > 0xA) {
                if (i_this->mTimers[1] == 0) {
                    i_this->mTimers[1] = (int)cM_rndF(5.0f);
                    i_this->mBallAlpha = (int)(8.0f + cM_rndF(5.0f));
                }
            } else if (i_this->mBallAlpha > 0) {
                i_this->mBallAlpha--;
            }
        }
        if (i_this->mTimers[0] == 0) {
            fopAcM_delete(actor);
            break;
        }
        if (i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_FIRE) {
            fopAcM_seStart(actor, JA_SE_OBJ_BAR_FRAME_BURN, 0);
        }
        break;
    case 0x68:
        switch (i_this->mBehaviorType) {
        case WZ_TYPE_DAMAGE_BALL_FIRE:
            dComIfGp_particle_set(
                dPa_name::ID_AK_SN_WIZHITFIRE00,
                &i_this->mTamaTarget,
                &actor->current.angle
            );
            break;
        case WZ_TYPE_DAMAGE_BALL_ICE:
            dComIfGp_particle_set(
                dPa_name::ID_AK_SN_WIZHITSUMMON00,
                &i_this->mTamaTarget,
                &actor->current.angle
            );
            {
                wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                if (parent != NULL) {
                    parent->mHasChildActor = 0;
                }
            }
            break;
        }
        i_this->mParticleCb0.remove();
        i_this->mParticleCb1.remove();
        fopAcM_delete(actor);
        break;
    }
}

/* 00005760-00005B64       .text action_summon_dousa__FP8wz_class */
void action_summon_dousa(wz_class* i_this) {
    fopAc_ac_c* actor = i_this;
    u8 deadCount = 0;

    switch (i_this->mMode) {
    case 0xC8: {
        for (int i = 0; i < 4; i++) {
            i_this->m3DE[i] = 0;
        }
        for (int i = 0; i < 5; i++) {
            JPABaseEmitter* emitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_WIZSUMMONWIND00, &actor->current.pos);
            JGeometry::TVec3<f32> scale;
            scale.set(actor->scale.x, actor->scale.y, actor->scale.z);
            if (emitter != NULL) {
                emitter->setGlobalScale(scale);
            }
        }
        fopAcM_seStart(actor, JA_SE_OBJ_WZ_SUMMON_B_SMN, 0);
        {
            JPABaseEmitter* emitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_WIZSUMMONSMOKE00, &actor->current.pos);
            JGeometry::TVec3<f32> scale;
            scale.set(actor->scale.x, actor->scale.y, actor->scale.z);
            if (emitter != NULL) {
                emitter->setGlobalScale(scale);
            }
        }
        i_this->mTimers[0] = 0x28;
        for (int i = 0; i < 20; i++) {
            i_this->mChildIds[i] = fpcM_ERROR_PROCESS_ID_e;
            i_this->mChildAlive[i] = 0;
        }
        i_this->mMode++;
        break;
    }
    case 0xC9:
        if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR) {
            summon_call_sub(i_this);
        }
        {
            BOOL stopped = i_this->mpBtk->isStop();
            if (stopped) {
                i_this->mMode++;
                if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR_2) {
                    fopAcM_delete(actor);
                }
            }
        }
        break;
    case 0xCA:
        for (int i = 0; i < 20; i++) {
            if (i_this->mChildAlive[i] != 0) {
                fopAc_ac_c* child = fopAcM_SearchByID(i_this->mChildIds[i]);
#if VERSION <= VERSION_JPN
                if (child == NULL) {
#else
                s16 outOfBounds = 0;
                if (child != NULL) {
                    if (strcmp(dComIfGp_getStartStageName(), "kazeMB") == 0) {
                        f32 x = child->current.pos.x;
                        f32 z = child->current.pos.z;
                        f32 distXZ = std::sqrtf(x * x + z * z);
                        if (distXZ > 1800.0f || child->current.pos.y < -200.0f || child->current.pos.y > 2100.0f) {
                            outOfBounds = 1;
                        }
                    }
                }
                if (child == NULL || outOfBounds != 0) {
#endif
                    i_this->mChildAlive[i] = 0;
                    i_this->mChildIds[i] = fpcM_ERROR_PROCESS_ID_e;
                }
            } else {
                deadCount++;
            }
        }
        if (deadCount == 20) {
            wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
            if (parent != NULL) {
                parent->mHasChildActor = 0;
            }
            fopAcM_delete(actor);
        }
        break;
    }

    i_this->mpMorf->play(NULL, 0, 0);
    i_this->mpBtk->play();
}

/* 00005B64-00006108       .text daWZ_Execute__FP8wz_class */
static BOOL daWZ_Execute(wz_class* i_this) {
    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        if (i_this->mEnableSpawnSwitch != 0xFF) {
            if (!dComIfGs_isSwitch(i_this->mEnableSpawnSwitch, dComIfGp_roomControl_getStayNo())) {
                return TRUE;
            }
        }
    }

    if (REG12_S(9) == 0) {
        if (i_this->m352 != 0) {
            fopAc_ac_c* related = fopAcM_SearchByID(i_this->mRelatedId);
            if (related == NULL) {
                i_this->mHasChildActor = 0;
                i_this->mRelatedId = fpcM_ERROR_PROCESS_ID_e;
                i_this->m352 = 0;
            }
        }
    }

    i_this->mPLight.mPos = i_this->current.pos;
    switch (i_this->mBehaviorType) {
    case WZ_TYPE_DAMAGE_BALL_FIRE:
        i_this->mPLight.mColor.r = 300.0f + REG0_F(3);
        i_this->mPLight.mColor.g = 50.0f + REG0_F(4);
        i_this->mPLight.mColor.b = REG0_F(5);
#if VERSION > VERSION_JPN
        i_this->mPLight.mPower = 550.0f + REG0_F(9);
        i_this->mPLight.mFluctuation = 200.0f + REG0_F(10);
#endif
        break;
    case WZ_TYPE_DAMAGE_BALL_ICE:
        i_this->mPLight.mColor.r = 300.0f + REG0_F(6);
        i_this->mPLight.mColor.g = 300.0f + REG0_F(7);
        i_this->mPLight.mColor.b = 20.0f + REG0_F(8);
#if VERSION > VERSION_JPN
        i_this->mPLight.mPower = 550.0f + REG0_F(9);
        i_this->mPLight.mFluctuation = 200.0f + REG0_F(10);
#endif
        break;
    }
#if VERSION <= VERSION_JPN
    i_this->mPLight.mPower = 550.0f + REG0_F(9);
    i_this->mPLight.mFluctuation = 200.0f + REG0_F(10);
#endif

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        fopAcM_setGbaName(i_this, 0x3C, 0x11, 0x2F);
        if (enemy_ice(&i_this->mEnemyIce)) {
            i_this->mpMorf->getModel()->setBaseTRMtx(DEMO_SELECT(mDoMtx_stack_c::now, mDoMtx_stack_c::get()));
            i_this->mpMorf->calc();
            if (i_this->mIsMiniBoss) {
                MtxP miniSrc = i_this->mpMorf->getModel()->getAnmMtx(WZ_JNT_MINI);
                MTXCopy(miniSrc, i_this->mpMiniMorf->getModel()->getBaseTRMtx());
                i_this->mpMiniMorf->calc();
            }
            enemy_fire_remove(&i_this->mEnemyFire);
            rod_size_set(i_this, 1);
#if VERSION > VERSION_DEMO
            BG_check(i_this);
#endif
            return TRUE;
        }
    }

    for (int i = 0; i < 5; i++) {
        if (i_this->mTimers[i] != 0) {
            i_this->mTimers[i]--;
        }
    }

    switch (i_this->mAction) {
    case 0:
        action_dousa(i_this);
        break;
    case 0x64:
        action_tama_dousa(i_this);
        break;
    case 1:
        action_itai(i_this);
        break;
    case 0xC8:
        action_summon_dousa(i_this);
        break;
    case 3:
        action_demo(i_this);
        break;
    }

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        if (i_this->mAction != 1 && i_this->mAction != 3) {
            cLib_addCalcAngleS2(&i_this->current.angle.y, i_this->mTargetAngleY, 1, 0x500);
            cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 1, 0x500);
        }
        i_this->mpMorf->play(NULL, 0, 0);
    }

    cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
    cMtx_XrotM(*calc_mtx, i_this->current.angle.x);
    cXyz offset;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->speedF;
    cXyz moved;
    MtxPosition(&offset, &moved);
    i_this->speed.x = moved.x;
    if (i_this->mBehaviorType >= WZ_TYPE_DAMAGE_BALL_FIRE) {
        i_this->speed.y = moved.y;
    }
    i_this->speed.z = moved.z;
    i_this->speed.y += i_this->gravity;
    if (i_this->speed.y < -100.0f) {
        i_this->speed.y = -100.0f;
    }

    fopAcM_posMove(i_this, NULL);
    i_this->eyePos = i_this->current.pos;

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        BG_check(i_this);
        if (i_this->mAction != 1 && i_this->mAction != 3
#if VERSION > VERSION_DEMO
            && i_this->mMode != 7
#endif
        ) {
            fuwafuwa_calc(i_this);
        }
        i_this->attention_info.position = i_this->current.pos;
        i_this->attention_info.position.y += 190.0f;
        i_this->eyePos.y += 120.0f + REG12_F(18);
        i_this->mBodyCyl.SetC(i_this->current.pos);
        i_this->mBodyCyl.SetH(170.0f);
        i_this->mBodyCyl.SetR(50.0f);
        dComIfG_Ccsp()->Set(&i_this->mBodyCyl);
    } else if (i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR && i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR_2) {
        BG_check(i_this);
        i_this->mBallSph.SetC(i_this->current.pos);
        i_this->mBallSph.SetR(i_this->mBallRadius);
        dComIfG_Ccsp()->Set(&i_this->mBallSph);
        dComIfG_Ccsp()->SetMass(&i_this->mBallSph, 1);
    }

    draw_SUB(i_this);
    i_this->mEffectPos = i_this->current.pos;
    i_this->mEffectAngle = i_this->shape_angle;
    return TRUE;
}

/* 00006108-00006110       .text daWZ_IsDelete__FP8wz_class */
static BOOL daWZ_IsDelete(wz_class*) {
    return TRUE;
}

/* 00006110-0000627C       .text daWZ_Delete__FP8wz_class */
static BOOL daWZ_Delete(wz_class* i_this) {
    if (i_this->health == -128) {
        if (i_this->mIsMiniBoss) {
            mDoAud_subBgmStop();
        }
#if VERSION == VERSION_DEMO
        if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
            dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(i_this));
        }
        fopAcM_onActor(i_this);
#else
        if (strcmp(dComIfGp_getStartStageName(), "kazeMB") != 0) {
            if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                dComIfGs_onSwitch(i_this->mDisableSpawnOnDeathSwitch, fopAcM_GetRoomNo(i_this));
            }
        }
#endif
    }

    if (i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR && i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR_2) {
        dComIfG_resDeleteDemo(&i_this->mPhase, "WZ");
    } else {
        dComIfG_resDeleteDemo(&i_this->mPhase, "WZB");
    }

    if (i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_FIRE || i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_ICE) {
        dKy_plight_cut(&i_this->mPLight);
    }

    i_this->mParticleCb0.remove();
    i_this->mParticleCb1.remove();
    i_this->mParticleCb2.remove();
    i_this->mParticleCb3.remove();
    i_this->mParticleCb4.remove();

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
        enemy_fire_remove(&i_this->mEnemyFire);
    }
    return TRUE;
}

/* 0000627C-00006684       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* i_this) {
    wz_class* a_this = (wz_class*)i_this;

    a_this->mpMorf = new mDoExt_McaMorf(
        (J3DModelData*)dComIfG_getObjectRes("WZ", dRes_INDEX_WZ_BDL_WZ_e),
        NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("WZ", dRes_INDEX_WZ_BCK_TAME1_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1,
        NULL,
        0x80000,
        0x37441422
    );
    J3DModel* model;
    if (a_this->mpMorf == NULL || (model = a_this->mpMorf->getModel()) == NULL) {
        return FALSE;
    }

    model->setUserArea((u32)a_this);
    for (u16 i = 0; i < a_this->mpMorf->getModel()->getModelData()->getJointNum(); i++) {
        a_this->mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
    }

    if (a_this->mBehaviorType > WZ_TYPE_DAMAGE_BALL_FIRE) {
        return TRUE;
    }

    if (a_this->mIsMiniBoss) {
        a_this->mpMiniMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("WZ", dRes_INDEX_WZ_BDL_WZ_KAMEN_e),
            NULL, NULL,
            NULL,
            J3DFrameCtrl::EMode_NONE, 0.0f, 0, -1, 1,
            NULL,
            0x80000,
            0x37441422
        );
        J3DModel* miniModel;
        if (a_this->mpMiniMorf == NULL || (miniModel = a_this->mpMiniMorf->getModel()) == NULL) {
            return FALSE;
        }
        if (!a_this->mMiniInvisibleModel.create(miniModel)) {
            return FALSE;
        }
    }

    a_this->mpRodMorf = new mDoExt_McaMorf(
        (J3DModelData*)dComIfG_getObjectRes("WZ", dRes_INDEX_WZ_BDL_WZ_STICK_e),
        NULL, NULL,
        NULL,
        J3DFrameCtrl::EMode_LOOP, 0.0f, 0, -1, 1,
        NULL,
        0,
        0x11020203
    );
    J3DModel* rodModel;
    if (a_this->mpRodMorf == NULL || (rodModel = a_this->mpRodMorf->getModel()) == NULL) {
        return FALSE;
    }

    rodModel->setUserArea((u32)a_this);
    for (u16 i = 0; i < a_this->mpRodMorf->getModel()->getModelData()->getJointNum(); i++) {
        a_this->mpRodMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(rod_nodeCallBack);
    }

    J3DModel* brkModel = a_this->mpMorf->getModel();
    a_this->mpBrk = new mDoExt_brkAnm();
    if (a_this->mpBrk == NULL) {
        return FALSE;
    }
    if (a_this->mpBrk->init(
            brkModel->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("WZ", dRes_INDEX_WZ_BRK_WZ_e),
            TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    return a_this->mInvisibleModel.create(a_this->mpMorf->getModel()) != 0 ? TRUE : FALSE;
}

/* 000066CC-00006888       .text useHeapInit2__FP10fopAc_ac_c */
static BOOL useHeapInit2(fopAc_ac_c* i_this) {
    wz_class* a_this = (wz_class*)i_this;

    a_this->mpMorf = new mDoExt_McaMorf(
        (J3DModelData*)dComIfG_getObjectRes("WZB", dRes_INDEX_WZB_BDL_YSUMN00_e),
        NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("WZB", dRes_INDEX_WZB_BCK_YSUMN00_e),
        J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, 1,
        NULL,
        0x80000,
        0x19000222
    );
    if (a_this->mpMorf == NULL || a_this->mpMorf->getModel() == NULL) {
        return FALSE;
    }
    J3DModel* model = a_this->mpMorf->getModel();

    a_this->mpBtk = new mDoExt_btkAnm();
    if (a_this->mpBtk == NULL) {
        return FALSE;
    }
    return a_this->mpBtk->init(
        model->getModelData(),
        (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("WZB", dRes_INDEX_WZB_BTK_YSUMN00_e),
        TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
    ) != 0 ? TRUE : FALSE;
}

/* 00006888-000071C4       .text daWZ_Create__FP10fopAc_ac_c */
static cPhs_State daWZ_Create(fopAc_ac_c* i_actor) {
    static dCcD_SrcCyl body_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
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
    static u8 fire_j[] = {
        0x01, 0x02, 0x06, 0x07, 0x08, 0x0C, 0x0D, 0x11, 0x13, 0x15,
    };
    static f32 fire_sc[] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    static dCcD_SrcSph ball_co_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_FIRE,
            /* SrcObjAt  Atp     */ 2,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
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
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
        }},
    };
    wz_class* i_this = (wz_class*)i_actor;
#if VERSION > VERSION_DEMO
    fopAcM_ct(i_this, wz_class);
#endif

    i_this->mBehaviorType = fopAcM_GetParam(i_this);
    i_this->mDisableSpawnOnDeathSwitch = fopAcM_GetParam(i_this) >> 8;
    i_this->mEnableSpawnSwitch = fopAcM_GetParam(i_this) >> 16;
    i_this->mPathIndex = fopAcM_GetParam(i_this) >> 24;
    i_this->mEnemySummonTableIndex = i_this->current.angle.z;
    if (i_this->mBehaviorType == 0xFF) {
        i_this->mBehaviorType = 0;
    }
#if VERSION > VERSION_JPN
    if (i_this->mBehaviorType == 2) {
        i_this->mBehaviorType = 1;
        i_this->mIsMiniBoss = true;
    }
    if (i_this->mBehaviorType == 3) {
        i_this->mBehaviorType = 0;
        i_this->m351 = true;
    }
    if (i_this->mEnemySummonTableIndex == 0xFF) {
        i_this->mEnemySummonTableIndex = 0;
    }
#endif

    cPhs_State phase_state;
    if (i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR && i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR_2) {
        phase_state = dComIfG_resLoad(&i_this->mPhase, "WZ");
    } else {
        phase_state = dComIfG_resLoad(&i_this->mPhase, "WZB");
    }

#if VERSION <= VERSION_JPN
    if (phase_state == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_ct(i_this, wz_class);
        if (REG8_S(9) != 0) {
            if (i_this->mBehaviorType == 1) {
                i_this->mBehaviorType = 2;
            }
            if (i_this->mBehaviorType == 0) {
                i_this->mBehaviorType = 3;
            }
        }
#endif
        if (i_this->mBehaviorType == 2) {
            i_this->mBehaviorType = 1;
            i_this->mIsMiniBoss = true;
        }
        if (i_this->mBehaviorType == 3) {
            i_this->mBehaviorType = 0;
            i_this->m351 = true;
        }
#else
#if VERSION == VERSION_PAL
    if (phase_state == cPhs_COMPLEATE_e) {
#endif
    if (i_this->mBehaviorType > WZ_TYPE_DAMAGE_BALL_FIRE) {
        i_this->mBallSph.SetAtType(AT_TYPE_MACHETE);
        i_this->mRelatedId = fopAcM_GetLinkId(i_this);
        if (i_this->mRelatedId == fpcM_ERROR_PROCESS_ID_e) {
            return cPhs_ERROR_e;
        }
        wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
        if (parent == NULL) {
            return cPhs_ERROR_e;
        }
        if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR) {
            i_this->mRelatedId = parent->mRelatedId;
            if (fopAcM_SearchByID(i_this->mRelatedId) == NULL) {
                return cPhs_ERROR_e;
            }
        }
        i_this->mPathTarget = parent->mPathTarget;
        i_this->mPathPointIdx = parent->mPathPointIdx;
        i_this->mHasChildActor = parent->mHasChildActor;
        i_this->mSummonWave = parent->mSummonWave;
        i_this->mSummonTableType = parent->mSummonTableType;
    }

#if VERSION != VERSION_PAL
    if (phase_state != cPhs_COMPLEATE_e) {
        if (phase_state == cPhs_ERROR_e) {
            if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR) {
                wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                parent->mHasChildActor = 0;
            }
        }
        return phase_state;
    }
#endif
#endif

    if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
            if (i_this->mDisableSpawnOnDeathSwitch != 0xFF) {
                if (dComIfGs_isSwitch(i_this->mDisableSpawnOnDeathSwitch, dComIfGp_roomControl_getStayNo())) {
                    return cPhs_ERROR_e;
                }
            }
        }

        if (i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR && i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR_2) {
            u32 heapSize = 0x3440;
            if (i_this->mBehaviorType >= WZ_TYPE_DAMAGE_BALL_FIRE) {
                heapSize = 0x3640;
            }
            if (!fopAcM_entrySolidHeap(i_this, useHeapInit, heapSize)) {
                return cPhs_ERROR_e;
            }
        } else {
            if (!fopAcM_entrySolidHeap(i_this, useHeapInit2, 0xCC0)) {
#if VERSION > VERSION_JPN
                wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                parent->mHasChildActor = 0;
#endif
                return cPhs_ERROR_e;
            }
            if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR_2) {
                fopAcM_OnStatus(i_this, fopAcStts_UNK4000_e);
            }
            J3DModelData* modelData = i_this->mpMorf->getModel()->getModelData();
            J3DTexture* texture = modelData->getTexture();
            if (texture == NULL) {
#if VERSION > VERSION_JPN
                wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                parent->mHasChildActor = 0;
#endif
                return cPhs_ERROR_e;
            }
            JUTNameTab* textureName = modelData->getTextureName();
            if (textureName == NULL) {
#if VERSION > VERSION_JPN
                wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                parent->mHasChildActor = 0;
#endif
                return cPhs_ERROR_e;
            }
            for (u16 i = 0; i < texture->getNum(); i++) {
                if (strcmp(textureName->getName(i), "__dummy") == 0) {
                    texture->setResTIMG(i, *mDoGph_gInf_c::getFrameBufferTimg());
                }
            }
            mDoExt_modelTexturePatch(modelData);
        }

        i_this->current.angle.z = 0;
        i_this->shape_angle.z = 0;
#if VERSION <= VERSION_JPN
        if (i_this->mEnemySummonTableIndex == 0xFF) {
            i_this->mEnemySummonTableIndex = 0;
        }
#endif
        i_this->attention_info.flags = 0;
        fopAcM_SetMtx(i_this, i_this->mpMorf->getModel()->getBaseTRMtx());
        fopAcM_setCullSizeBox(i_this, -100.0f, -50.0f, -50.0f, 100.0f, 200.0f, 100.0f);

        if (i_this->mBehaviorType < WZ_TYPE_DAMAGE_BALL_FIRE) {
            i_this->stealItemLeft = 3;
            i_this->max_health = 4;
            i_this->health = 4;
            i_this->attention_info.distances[2] = 4;
            switch (i_this->mBehaviorType) {
            case 0:
                i_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("wiz_r", 0);
                break;
            case 1:
                i_this->max_health = 8;
                i_this->health = 8;
                i_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("wiz_s", 0);
                i_this->mHasChildActor = 0;
                i_this->mSummonTableType = i_this->mEnemySummonTableIndex;
#if VERSION <= VERSION_JPN
                if (REG8_S(6) != 0) {
                    i_this->mSummonTableType = (s16)(REG8_S(6) - 1);
                }
#endif
                if (i_this->mIsMiniBoss == true) {
                    i_this->mSummonTableType = 7;
                    fopAcM_OnStatus(i_this, fopAcStts_BOSS_e);
                    i_this->max_health = 12;
                    i_this->health = 12;
                } else {
                    i_this->mSummonWave = (s16)cM_rndF(3.99f);
                    i_this->mSummonWave &= 3;
                }
                break;
            }
#if VERSION <= VERSION_JPN
            if (REG12_S(2) != 0) {
                i_this->max_health = 1;
                i_this->health = 1;
            }
            if (REG12_S(8) != 0) {
                i_this->max_health = 0x32;
                i_this->health = 0x32;
            }
#else
            if (REG8_S(1) != 0) {
                i_this->max_health = 0x7F;
                i_this->health = 0x7F;
            }
#endif
            if (i_this->mPathIndex != 0xFF) {
                i_this->mpRoomPath = dPath_GetRoomPath(i_this->mPathIndex, fopAcM_GetRoomNo(i_this));
                if (i_this->mpRoomPath != NULL) {
                    int rnd = (int)cM_rndF(i_this->mpRoomPath->m_num);
                    if (rnd == i_this->mpRoomPath->m_num) {
                        rnd--;
                    }
                    i_this->current.pos = i_this->mpRoomPath->m_points[rnd].m_position;
                    i_this->mpPath = dPath_GetNextRoomPath(i_this->mpRoomPath, fopAcM_GetRoomNo(i_this));
                }
            }
            i_this->mHomePos = i_this->current.pos;
            draw_SUB(i_this);
            i_this->mBodyCyl.Set(body_co_cyl_src);
            i_this->mBodyCyl.SetStts(&i_this->mStts);
            i_this->mBodyCyl.OffTgSetBit();
            i_this->mBodyCyl.ClrTgHit();
            i_this->mTargetAngleY = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
            i_this->current.angle.y = i_this->mTargetAngleY;
            i_this->shape_angle.y = i_this->mTargetAngleY;
            i_this->mWallHeight = 100.0f;
            i_this->mWallRadius = 110.0f;
            i_this->mEnemyIce.mpActor = i_this;
            i_this->mEnemyIce.mWallRadius = 80.0f;
            i_this->mEnemyIce.mCylHeight = 50.0f;
            i_this->mEnemyFire.mpMcaMorf = i_this->mpMorf;
            i_this->mEnemyFire.mpActor = i_this;
            for (int i = 0; i < (int)ARRAY_SIZE(i_this->mEnemyFire.mFlameJntIdxs); i++) {
                i_this->mEnemyFire.mFlameJntIdxs[i] = fire_j[i];
                i_this->mEnemyFire.mParticleScale[i] = fire_sc[i];
            }
            i_this->mAlpha = 0;
            i_this->mRodScale.setall(0.0f);
            if (strcmp(dComIfGp_getStartStageName(), "kazeMB") == 0) {
                i_this->mAction = 3;
                if (i_this->mIsMiniBoss) {
                    i_this->current.pos.setall(0.0f);
                    come_flag = 0;
                    i_this->mMode = 0x32;
                } else {
                    fopAcM_OnStatus(i_this, fopAcStts_UNK4000_e);
                    i_this->mMode = 0x3C;
                }
            } else {
                i_this->mAction = 0;
                i_this->mMode = 0;
            }
        } else if (i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR && i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR_2) {
            i_this->mWallHeight = 0.0f;
            i_this->mWallRadius = 40.0f;
            i_this->mBallSph.Set(ball_co_sph_src);
            i_this->mBallSph.SetStts(&i_this->mStts);
            i_this->mTimers[2] = 0x12C;
            i_this->mAction = 0x64;
            i_this->mMode = 0x64;
        } else {
            i_this->mAction = 0xC8;
            i_this->mMode = 0xC8;
        }

#if VERSION <= VERSION_JPN
        if (i_this->mBehaviorType > WZ_TYPE_DAMAGE_BALL_FIRE) {
            i_this->mBallSph.SetAtType(AT_TYPE_MACHETE);
            i_this->mRelatedId = fopAcM_GetLinkId(i_this);
            if (i_this->mRelatedId == fpcM_ERROR_PROCESS_ID_e) {
                return cPhs_ERROR_e;
            }
            wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
            if (parent != NULL) {
                if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR) {
                    i_this->mRelatedId = parent->mRelatedId;
                    wz_class* summoner = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
                    if (summoner != NULL) {
                        parent = summoner;
                    } else {
                        return cPhs_ERROR_e;
                    }
                }
                i_this->mPathTarget = parent->mPathTarget;
                i_this->mPathPointIdx = parent->mPathPointIdx;
                i_this->mHasChildActor = parent->mHasChildActor;
                i_this->mSummonWave = parent->mSummonWave;
                i_this->mSummonTableType = parent->mSummonTableType;
            } else {
                return cPhs_ERROR_e;
            }
        }
#endif

        if (i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR && i_this->mBehaviorType != WZ_TYPE_SUMMON_DOOR_2) {
            i_this->mAcch.Set(
                fopAcM_GetPosition_p(i_this),
                fopAcM_GetOldPosition_p(i_this),
                i_this,
                1,
                &i_this->mAcchCir,
                fopAcM_GetSpeed_p(i_this)
            );
            i_this->mStts.Init(0xFE, 1, i_actor);
            BG_check(i_this);
        }

        if (i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_FIRE || i_this->mBehaviorType == WZ_TYPE_DAMAGE_BALL_ICE) {
            dKy_plight_set(&i_this->mPLight);
        }
#if VERSION == VERSION_PAL || VERSION <= VERSION_JPN
    }
#endif
#if VERSION == VERSION_PAL

    if (phase_state == cPhs_ERROR_e) {
        if (i_this->mBehaviorType == WZ_TYPE_SUMMON_DOOR) {
            wz_class* parent = (wz_class*)fopAcM_SearchByID(i_this->mRelatedId);
            if (parent != NULL) {
                parent->mHasChildActor = 0;
            }
        }
    }
#endif

    return phase_state;
}


static actor_method_class l_daWZ_Method = {
    (process_method_func)daWZ_Create,
    (process_method_func)daWZ_Delete,
    (process_method_func)daWZ_Execute,
    (process_method_func)daWZ_IsDelete,
    (process_method_func)daWZ_Draw,
};

actor_process_profile_definition g_profile_WZ = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_WZ_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(wz_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_WZ_e,
    /* Actor SubMtd */ &l_daWZ_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
