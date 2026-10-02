/**
 * d_a_pw.cpp
 * Enemy - Poe
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_pw.h"
#include "d/actor/d_a_bpw.h"
#include "d/actor/d_a_kantera.h"
#include "d/actor/d_a_player.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_com_inf_game.h"
#include "res/Object/Pw.h"
#include "d/d_kankyo_rain.h"
#include "d/d_material.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera.h"
#include "f_pc/f_pc_manager.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_s_play.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"

enum daPW__BehaviorType {
    VisibleFromStart = 0x0,
    InvisibleAtStart = 0x1,
    OnlyLanternVisibleAtStart = 0x2,
    JalhallaChildA = 0x3,
    JalhallaChildB = 0x4,
    UseDefault = 0xFF
};

static bool TORITUKI_ON = false;

/* 00000078-00000158       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        pw_class* i_this = (pw_class*)model->getUserArea();
        if (i_this != NULL) {
            if (jntNo == PW_JNT_J_PW_ITEM_R1_e) {
                MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
                i_this->m2E4 = i_this->m2CC;
                cXyz offset;
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->m2CC);
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

/* 00000158-00000230       .text draw_SUB__FP8pw_class */
void draw_SUB(pw_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    model->setBaseScale(i_this->scale);
    mDoMtx_stack_c::transS(i_this->current.pos.x, i_this->current.pos.y + i_this->m3A4, i_this->current.pos.z);
    mDoMtx_stack_c::YrotM(i_this->shape_angle.y);
    mDoMtx_stack_c::XrotM(i_this->shape_angle.x);
    mDoMtx_stack_c::ZrotM(i_this->shape_angle.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());

    i_this->mpMorf->calc();
    
    enemy_fire(&i_this->mEnemyFire);

    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &i_this->current.pos, &i_this->tevStr);
}

/* 00000230-000004D4       .text daPW_Draw__FP8pw_class */
static BOOL daPW_Draw(pw_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    J3DModelData* modelData = model->getModelData();
    g_env_light.setLightTevColorType(model, &i_this->tevStr);
    dSnap_RegistFig(DSNAP_TYPE_UNKB8, i_this, 1.0f, 1.0f, 1.0f);

    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        modelData->getMaterialNodePointer(i)->getTevKColor(3)->mColor.a = i_this->m39A;
    }

    dComIfGd_setListMaskOff();
    if (i_this->mEnemyIce.mFreezeTimer > 20) {
        dMat_control_c::iceEntryDL(i_this->mpMorf, -1, &i_this->mInvisibleModel);
#if VERSION > VERSION_JPN
        dComIfGd_setList();
#endif
        return TRUE;
    }

    if (i_this->m33E != 0) {
        i_this->m2C0->entry(model->getModelData());
    } else if (i_this->m33F != 0) {
        i_this->m2C4->entry(model->getModelData());
    } else {
        i_this->m2C8->entry(model->getModelData());
    }

    i_this->m2BC->entry(model->getModelData());
    i_this->m2BC->setFrame(i_this->mColorIndex);
    i_this->mpMorf->entryDL();

    if (i_this->m33E != 0) {
        i_this->m2C0->remove(model->getModelData());
    } else if (i_this->m33F != 0) {
        i_this->m2C4->remove(model->getModelData());
    } else {
        i_this->m2C8->remove(model->getModelData());
    }
    i_this->m2BC->remove(model->getModelData());

    dComIfGd_setList();
    i_this->mInvisibleModel.entryMaskOff();

    if (i_this->mBehaviorType == VisibleFromStart && i_this->mEnemyIce.mLightShrinkTimer == 0) {
        dComIfGd_setSimpleShadow2(
            &i_this->current.pos,
            i_this->mAcch.GetGroundH(),
            50.0f,
            i_this->mAcch.m_gnd,
            0,
            1.0f,
            dDlst_shadowControl_c::getSimpleTex()
        );
    }

    return TRUE;
}

/* 000004D4-00000600       .text anm_init__FP8pw_classifUcfi */
void anm_init(pw_class* i_this, int bckFileIdx, f32 morf, u8 loopMode, f32 speed, int soundFileIdx) {
    i_this->mBckIdx = bckFileIdx;
#if VERSION == VERSION_DEMO
    if (soundFileIdx >= 0) {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("PW", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, dComIfG_getObjectRes("PW", soundFileIdx));
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("PW", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
#else
    if (soundFileIdx >= 0) {
        void* soundAnm = dComIfG_getObjectRes("PW", soundFileIdx);
        J3DAnmTransform* bckAnm = (J3DAnmTransform*)dComIfG_getObjectRes("PW", bckFileIdx);
        i_this->mpMorf->setAnm(bckAnm, loopMode, morf, speed, 0.0f, -1.0f, soundAnm);
    } else {
        J3DAnmTransform* bckAnm = (J3DAnmTransform*)dComIfG_getObjectRes("PW", bckFileIdx);
        i_this->mpMorf->setAnm(bckAnm, loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
#endif
}

/* 00000600-000006F4       .text kantera_break__FP8pw_class */
void kantera_break(pw_class* i_this) {
    fopAc_ac_c* actor;
    fopAc_ac_c* const player = dComIfGp_getPlayer(0);
    i_this->m346 = 0;
    if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e) {
#if VERSION == VERSION_DEMO
        actor = fopAcM_SearchByID(i_this->mKanteraID);
        if (actor != NULL) {
#else
        if (fopAcM_SearchByID(i_this->mKanteraID, &actor) && actor != NULL && fopAcM_GetName(actor) == fpcNm_KANTERA_e) {
#endif
            kantera_class* kantera = (kantera_class*)actor;
            cMtx_YrotS(*calc_mtx, cM_atan2s(actor->current.pos.x - player->current.pos.x, actor->current.pos.z - player->current.pos.z));
            cXyz offset;
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 20.0f;
            MtxPosition(&offset, &actor->speed);
            actor->speed.y = 10.0f;
            kantera->mState = 5;
            i_this->mKanteraID = fpcM_ERROR_PROCESS_ID_e;
        }
    }
}

#if VERSION == VERSION_DEMO
/* 000007CC-000008B0       .text Big_pow_down_check__FP8pw_class */
void Big_pow_down_check(pw_class* i_this) {
    if (i_this->mJalhallaID) {
        fopAc_ac_c* actor = fopAcM_SearchByID(i_this->mJalhallaID);
        if (actor != NULL) {
            actor->health = actor->health - 1;
            if (actor->health <= 0) {
                i_this->m344 = 1;
            }
        }
    }
}
#endif

/* 000006F4-000007CC       .text Big_pow_gattai_check__FP8pw_class */
BOOL Big_pow_gattai_check(pw_class* i_this) {
    if (i_this->mJalhallaID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor;
#if VERSION == VERSION_DEMO
        actor = fopAcM_SearchByID(i_this->mJalhallaID);
        if (actor != NULL) {
#else
        if (fopAcM_SearchByID(i_this->mJalhallaID, &actor) && actor != NULL && fopAcM_GetName(actor) == fpcNm_BPW_e) {
#endif
            bpw_class* bpw = (bpw_class*)actor;
            if (bpw->m3E9 != 0) {
                i_this->m340 = 0;
                i_this->mCyl.OffAtSetBit();
                i_this->mCyl.OffAtSetBit();
                i_this->mCyl.OffTgSetBit();
                i_this->mCyl.ClrTgHit();
                i_this->m38C = fopAcM_searchActorAngleY(i_this, actor);
                i_this->mAction = 5;
                i_this->mMode = 0xAA;
                return TRUE;
            }
        }
    }
    return FALSE;
}

#if VERSION > VERSION_DEMO
/* 000007CC-000008B0       .text Big_pow_down_check__FP8pw_class */
BOOL Big_pow_down_check(pw_class* i_this) {
    if (i_this->health > 0) {
        return FALSE;
    }
    if (i_this->m345 != 0) {
        return FALSE;
    }
    if (i_this->mJalhallaID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor;
        if (fopAcM_SearchByID(i_this->mJalhallaID, &actor) && actor != NULL && fopAcM_GetName(actor) == fpcNm_BPW_e) {
            bpw_class* bpw = (bpw_class*)actor;
            if (bpw->mActionState == 0x6F && bpw->mSomeCountdownTimers[0] > 3) {
                actor->health = actor->health - 1;
                if (actor->health <= 0) {
                    i_this->m344 = 1;
                }
                i_this->m345 = 1;
            } else {
                i_this->health = 4;
                return TRUE;
            }
        }
    }
    return FALSE;
}
#endif

/* 000008B0-0000121C       .text body_atari_check__FP8pw_class */
BOOL body_atari_check(pw_class* i_this) {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    i_this->mStts.Move();
    i_this->mHitByWhat = 0;
    if (i_this->health <= 0) {
        return FALSE;
    }
#if VERSION == VERSION_DEMO
    if (dComIfGp_getDetect().chk_light(&i_this->current.pos)) {
        i_this->mHitByWhat = 9;
        if (i_this->m39A != 0xFF && i_this->mMode != 0x37 && i_this->mMode != 0x3C && i_this->mMode != 0x3D) {
            i_this->mAction = 2;
            i_this->mMode = 0x34;
        }
        return TRUE;
    }
#else
    if (i_this->mAction == 5) {
        return FALSE;
    }
    if (dComIfGp_getDetect().chk_light(&i_this->current.pos)) {
        i_this->mHitByWhat = 9;
        if (i_this->m39A != 0xFF && i_this->m378[5] == 0 && i_this->m346 != -1 && i_this->mMode != 0x37 &&
            i_this->mMode != 0x3C && i_this->mMode != 0x3D && i_this->mJalhallaID == fpcM_ERROR_PROCESS_ID_e)
        {
            i_this->mAction = 2;
            i_this->mMode = 0x34;
            return TRUE;
        }
    }
#endif
    if (i_this->mCyl.ChkTgHit()) {
        cCcD_Obj* hitObj = i_this->mCyl.GetTgHitObj();
        if (hitObj == NULL) {
            return FALSE;
        }

        CcAtInfo atInfo;
        atInfo.pParticlePos = NULL;
        cXyz hitPos = *i_this->mCyl.GetTgHitPosP();
        atInfo.mpObj = i_this->mCyl.GetTgHitObj();
        u8 skipAtCheck = 0;
        if (i_this->m33E == 0) {
            u32 atType = hitObj->GetAtType();
            if (atType & AT_TYPE_LIGHT) {
                i_this->mHitByWhat = 9;
            } else if (atType & AT_TYPE_LIGHT_ARROW) {
                i_this->mHitByWhat = 10;
                i_this->m39A = 0xFF;
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                i_this->mEnemyIce.mParticleScale = 1.0f;
                i_this->mEnemyIce.mYOffset = 80.0f;
                i_this->attention_info.flags = 0;
                Big_pow_down_check(i_this);
                kantera_break(i_this);
            } else {
                skipAtCheck = 1;
            }
        } else {
            switch (hitObj->GetAtType()) {
            case AT_TYPE_GRAPPLING_HOOK:
                if (i_this->stealItemLeft > 0) {
                    s8 oldHealth = i_this->health;
                    i_this->health = 10;
                    atInfo.mpObj = i_this->mCyl.GetTgHitObj();
                    cc_at_check(i_this, &atInfo);
                    i_this->health = oldHealth;
                }
                dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &i_this->attention_info.position);
                skipAtCheck = 1;
                i_this->mHitByWhat = 12;
                fopAcM_seStart(i_this, JA_SE_LK_MS_WEP_HIT, 0x20);
                break;
            case AT_TYPE_SWORD:
            case AT_TYPE_MACHETE:
            case AT_TYPE_UNK800:
            case AT_TYPE_DARKNUT_SWORD:
            case AT_TYPE_MOBLIN_SPEAR: {
                fopAcM_seStart(i_this, JA_SE_LK_SW_HIT_S, 0x20);
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
                i_this->mHitByWhat = 1;
                break;
            }
            case AT_TYPE_HOOKSHOT:
                skipAtCheck = 1;
                fopAcM_seStart(i_this, JA_SE_LK_MS_WEP_HIT, 0x20);
                i_this->mHitByWhat = 11;
                i_this->mMode = 0x1B;
                break;
            case AT_TYPE_WIND:
                i_this->mHitByWhat = 3;
                i_this->mAction = 2;
                i_this->mMode = 0x38;
                return TRUE;
            case AT_TYPE_LIGHT:
                skipAtCheck = 1;
                break;
            case AT_TYPE_BOOMERANG:
                skipAtCheck = 1;
                i_this->mHitByWhat = 4;
                dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &i_this->attention_info.position);
                // fallthrough
            case AT_TYPE_BOKO_STICK:
            case AT_TYPE_STALFOS_MACE:
                fopAcM_seStart(i_this, JA_SE_LK_W_WEP_HIT, 0x20);
                break;
            case AT_TYPE_SKULL_HAMMER:
                fopAcM_seStart(i_this, JA_SE_LK_HAMMER_HIT, 0x20);
                i_this->mHitByWhat = 7;
                if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
                    i_this->mHitByWhat = 8;
                }
                break;
            case AT_TYPE_BOMB:
                i_this->mHitByWhat = 6;
                break;
            case AT_TYPE_ICE_ARROW:
#if VERSION > VERSION_JPN
                skipAtCheck = 1;
#endif
                i_this->mEnemyIce.mFreezeDuration = 200;
                enemy_fire_remove(&i_this->mEnemyFire);
                i_this->mHitByWhat = 5;
                fopAcM_seStart(i_this, JA_SE_LK_MS_WEP_HIT, 0x20);
                i_this->health = 0;
                i_this->attention_info.flags = 0;
#if VERSION == VERSION_DEMO
                Big_pow_down_check(i_this);
                i_this->mAction = 2;
                i_this->mMode = 0x3C;
                return TRUE;
#else
                if (!Big_pow_down_check(i_this)) {
                    i_this->mAction = 2;
                    i_this->mMode = 0x3C;
                    return TRUE;
                }
                break;
#endif
            case AT_TYPE_LIGHT_ARROW:
#if VERSION > VERSION_JPN
                skipAtCheck = 1;
#endif
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                i_this->mEnemyIce.mParticleScale = 1.0f;
                i_this->mEnemyIce.mYOffset = 80.0f;
                i_this->attention_info.flags = 0;
                i_this->mHitByWhat = 5;
                fopAcM_seStart(i_this, JA_SE_LK_MS_WEP_HIT, 0x20);
                Big_pow_down_check(i_this);
                break;
            case AT_TYPE_FIRE:
            case AT_TYPE_FIRE_ARROW:
                i_this->mEnemyFire.mFireDuration = 100;
                // Fall-through
            case AT_TYPE_NORMAL_ARROW:
                i_this->mHitByWhat = 5;
                // Fall-through
            default:
                i_this->mHitByWhat = 0;
                fopAcM_seStart(i_this, JA_SE_LK_MS_WEP_HIT, 0x20);
                break;
            }
        }

        if (skipAtCheck == 0) {
            if (i_this->m33E == 0) {
                if (i_this->mMode != 0x35) {
                    i_this->mAction = 2;
                    i_this->mMode = 0x34;
                }
                return TRUE;
            }
            cc_at_check(i_this, &atInfo);
            cXyz scale;
            if (i_this->mHitByWhat == 1 || i_this->mHitByWhat == 7 || i_this->mHitByWhat == 8 || i_this->health <= 0) {
#if VERSION == VERSION_DEMO
                Big_pow_down_check(i_this);
#else
                if (Big_pow_down_check(i_this)) {
                    return FALSE;
                }
#endif
                dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hitPos);
                scale.setall(2.0f);
                dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hitPos, &player->shape_angle, &scale);
                if (i_this->mHitByWhat == 7) {
                    i_this->mAction = 2;
                    i_this->mMode = 0x3E;
                    i_this->speedF = 0.0f;
                    i_this->gravity = -3.0f;
                    anm_init(i_this, dRes_INDEX_PW_BCK_PRESS1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                    return TRUE;
                }
            } else {
                dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &hitPos, &player->shape_angle);
            }
            if (i_this->mMode != 0x37) {
                i_this->mAction = 2;
                i_this->mMode = 0x36;
            }
            return TRUE;
        }
        if (i_this->mHitByWhat == 4 || i_this->mHitByWhat == 12) {
            i_this->mAction = 2;
            i_this->mMode = 0x3A;
            return TRUE;
        }
    }
    return FALSE;
}

/* 00001258-000016FC       .text kantera_atari_check__FP8pw_class */
BOOL kantera_atari_check(pw_class* i_this) {
    /* Nonmatching */
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#if VERSION == VERSION_DEMO
    if (i_this->m346 == 0) {
        return FALSE;
    }
    if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = fopAcM_SearchByID(i_this->mKanteraID);
        if (actor != NULL) {
#else
    if (i_this->m346 <= 0) {
        return FALSE;
    }
    if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor;
        if (!fopAcM_SearchByID(i_this->mKanteraID, &actor)) {
            return FALSE;
        }
        if (actor == NULL) {
            return FALSE;
        }
        if (fopAcM_GetName(actor) == fpcNm_KANTERA_e) {
#endif
    s16 oldHp = i_this->m398;
    i_this->mStts.Move();
    i_this->m33D = 0;
    if (i_this->mSph.ChkTgHit()) {
        if (i_this->m342 != 0) {
            return FALSE;
        }
        cCcD_Obj* hitObj = i_this->mSph.GetTgHitObj();
        if (hitObj == NULL) {
            return FALSE;
        }

        CcAtInfo atInfo;
        atInfo.pParticlePos = NULL;
        i_this->m342 = 1;
        switch (hitObj->GetAtType()) {
        case AT_TYPE_SWORD:
            fopAcM_seStart(i_this, JA_SE_LK_SW_HIT_S, 0x40);
            i_this->m398--;
            {
                u8 cutType = player->getCutType();
                if (cutType == daPy_py_c::CUT_TYPE_CUT_EA) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_CUT_EB) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_CUT_TURN) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_CUT_ROLL) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_SWORD) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_STICK) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_MACHETE) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_BT_ROLLCUT) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_CLUB) {
                    goto lantern_smash;
                }
                if (cutType == daPy_py_c::CUT_TYPE_JUMPCUT_DN_SWORD) {
                    goto lantern_smash;
                }
                break;
            lantern_smash:
                i_this->m33D = 1;
                i_this->m398--;
            }
            break;
        case AT_TYPE_WIND:
            i_this->m33D = 3;
            break;
        case AT_TYPE_BOOMERANG:
            i_this->m33D = 4;
            // Fall-through
        case AT_TYPE_BOKO_STICK:
            i_this->m398--;
            fopAcM_seStart(i_this, JA_SE_LK_W_WEP_HIT, 0x40);
            break;
        case AT_TYPE_SKULL_HAMMER:
            fopAcM_seStart(i_this, JA_SE_LK_HAMMER_HIT, 0x40);
            i_this->m33D = 7;
            if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
                i_this->m33D = 8;
            }
            i_this->m398 = 0;
            break;
        case AT_TYPE_BOMB:
            i_this->m33D = 6;
            i_this->m398 -= 2;
            break;
        case AT_TYPE_FIRE_ARROW:
        case AT_TYPE_ICE_ARROW:
        case AT_TYPE_LIGHT_ARROW:
            i_this->m398 -= 2;
            // Fall-through
        case AT_TYPE_NORMAL_ARROW:
            i_this->m33D = 5;
            // Fall-through
        default:
            i_this->m398--;
            i_this->m33D = 0;
            fopAcM_seStart(i_this, JA_SE_LK_MS_WEP_HIT, 0x40);
            break;
        }
    } else {
        i_this->m342 = 0;
    }

    if (oldHp != i_this->m398) {
        if (i_this->m398 <= 0) {
            cXyz hitPos = *i_this->mSph.GetTgHitPosP();
            i_this->m398 = 0;
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hitPos);
            cXyz scale;
            scale.setall(1.0f);
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hitPos, &player->shape_angle, &scale);
        }
        i_this->mAction = 2;
        i_this->mMode = 0x32;
        return TRUE;
    }
        }
    }
    return FALSE;
}

/* 000016FC-000018FC       .text kantera_calc__FP8pw_class */
#if VERSION == VERSION_DEMO
int kantera_calc(pw_class* i_this) {
    if (i_this->m346 == 0) {
        return 0;
    }
#else
void kantera_calc(pw_class* i_this) {
    if (i_this->m346 <= 0) {
        return;
    }
    if (i_this->m378[5] != 0) {
        return;
    }
#endif
    if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor;
#if VERSION == VERSION_DEMO
        actor = fopAcM_SearchByID(i_this->mKanteraID);
        if (actor != NULL) {
#else
        if (fopAcM_SearchByID(i_this->mKanteraID, &actor) && actor != NULL && fopAcM_GetName(actor) == fpcNm_KANTERA_e) {
#endif
            kantera_class* kantera = (kantera_class*)actor;
            actor->current.pos = i_this->m2CC;
            actor->current.angle.y = i_this->shape_angle.y;
            switch (i_this->m341) {
            case 0:
                {
                    s16 target = 0xBB8;
                    cLib_addCalcAngleS2(&i_this->m396, target, 1, 0x64);
                    cLib_addCalc2(&i_this->m3B0, 5000.0f, 1.0f, 1000.0f);
                    cLib_addCalc2(&i_this->m3B8, -1000.0f, 1.0f, 1000.0f);
                    break;
                }
            case 1:
                {
                    s16 target = 0xFA0;
                    cLib_addCalcAngleS2(&i_this->m396, target, 1, 0x64);
                    cLib_addCalc2(&i_this->m3B0, 10000.0f, 1.0f, 1000.0f);
                    cLib_addCalc2(&i_this->m3B8, -10000.0f, 1.0f, 1000.0f);
                    break;
                }
            case 2:
                break;
            }
            if (i_this->m341 != 2) {
                i_this->m394 += i_this->m396;
                kantera->mJointRot[1].x = i_this->m3B0 * cM_ssin(i_this->m394);
                kantera->mJointRot[1].z = i_this->m3B8 * cM_scos(i_this->m394);
#if VERSION > VERSION_DEMO
                actor->attention_info.flags = 0;
#endif
            }
            i_this->m2D8 = kantera->mBonPos;
        }
    }
#if VERSION == VERSION_DEMO
    return 0;
#endif
}

/* 000018FC-00001990       .text BG_check__FP8pw_class */
void BG_check(pw_class* i_this) {
    i_this->mAcchCir.SetWall(70.0f, 140.0f);

    i_this->current.pos.y -= i_this->mCorrectionOffsetY;
    i_this->old.pos.y -= i_this->mCorrectionOffsetY;
    i_this->mAcch.CrrPos(*dComIfG_Bgsp());
    i_this->current.pos.y += i_this->mCorrectionOffsetY;
    i_this->old.pos.y += i_this->mCorrectionOffsetY;
}

/* 00001990-00001D60       .text Line_check__FP8pw_class4cXyzUc */
BOOL Line_check(pw_class* i_this, cXyz destPos, u8 r22) {
    fopAc_ac_c* actor = i_this;
    dBgS_LinChk linChk;
    i_this->m314 = destPos;
    if (!r22) {
        cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
        cXyz sp14(0.0f, 0.0f, 200.0f);
        MtxPosition(&sp14, &destPos);
        i_this->m314 = destPos + i_this->current.pos;
    }
    i_this->m314.y += 80.0f;
    destPos = i_this->m314;
    i_this->m308 = i_this->current.pos;
    i_this->m308.y += 80.0f;
    linChk.Set(&i_this->m308, &destPos, actor);
    if (!dComIfG_Bgsp()->LineCross(&linChk)) {
        return FALSE;
    }
    i_this->m38C = i_this->current.angle.y + 0x8000;
    return TRUE;
}

/* 00002198-00002254       .text alpha_anime__FP8pw_class */
void alpha_anime(pw_class* i_this) {
    if (i_this->m33E == 0) {
        i_this->m38E += 0x400;
        cLib_addCalcAngleS2(&i_this->m39C, 0x64, 1, 0xA);
#if VERSION == VERSION_DEMO
        i_this->m39A = i_this->m39C + cM_ssin(i_this->m38E) * 30.0f;
#else
        f32 tmp = i_this->m39C;
        i_this->m39A = tmp + cM_ssin(i_this->m38E) * 30.0f;
#endif
    }
}

/* 00002254-00002400       .text fuwafuwa_calc__FP8pw_class */
void fuwafuwa_calc(pw_class* i_this) {
    fopAc_ac_c* const player = dComIfGp_getPlayer(0);
    f32 f31 = i_this->m2F0.y;
    i_this->m390 += 0x2BC;
    if (i_this->mAcch.GetGroundH() != -G_CM3D_F_INF &&
        dComIfG_Bgsp()->ChkPolySafe(i_this->mAcch.m_gnd) &&
        dComIfG_Bgsp()->GetGroundCode(i_this->mAcch.m_gnd) == 4)
    {
        if (i_this->mAction == 1) {
            f31 = player->current.pos.y;
        }
        i_this->current.pos.y = 30.0f + f31;
        return;
    }

    if (!i_this->mHoversAtInitialYPos) {
        f31 = i_this->mAcch.GetGroundH();
    }
    if (i_this->mMode == 0x21 || i_this->mMode == 0x28) {
        f31 = player->current.pos.y;
    }
    cLib_addCalc2(&i_this->current.pos.y, 30.0f + f31 + 30.0f * cM_ssin(i_this->m390), 1.0f, 3.0f);
    if (i_this->mMode == 0x26) {
        i_this->m392 += 0x3E8;
#if VERSION == VERSION_DEMO
        f32 x = 50.0f + i_this->m320.x + 50.0f * cM_ssin(i_this->m392);
        f31 = 50.0f + i_this->m320.z + 50.0f * cM_ssin(i_this->m392);
        cLib_addCalc2(&i_this->current.pos.x, x, 1.0f, 5.0f);
#else
        f31 = 50.0f + i_this->m320.z + 50.0f * cM_ssin(i_this->m392);
        cLib_addCalc2(&i_this->current.pos.x, 50.0f + i_this->m320.x + 50.0f * cM_ssin(i_this->m392), 1.0f, 5.0f);
#endif
        cLib_addCalc2(&i_this->current.pos.z, f31, 1.0f, 5.0f);
    }
}

/* 00002400-00002560       .text kougen_hani_check__FP8pw_classUc */
BOOL kougen_hani_check(pw_class* i_this, u8 param_2) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz offset;
    cXyz pos = i_this->current.pos;
    if (!param_2) {
        f32 f31 = 0.33f * (i_this->current.pos.x - player->current.pos.x);
        f32 f30 = 0.33f * (i_this->current.pos.z - player->current.pos.z);
        for (int i = 0; i < 3; i++) {
            pos.x += f31;
            pos.z += f30;
            if (dComIfGp_getDetect().chk_light(&pos)) {
                return TRUE;
            }
        }
    } else {
        cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = 200.0f;
        MtxPosition(&offset, &pos);
        pos += i_this->current.pos;
        if (dComIfGp_getDetect().chk_light(&pos)) {
            return TRUE;
        }
    }
    return FALSE;
}

/* 00002560-00002714       .text kyori_sub__FP8pw_class */
void kyori_sub(pw_class* i_this) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    fopAc_ac_c* actor = i_this;
    if (fopAcM_searchActorDistance(actor, player) > 300.0f) {
        return;
    }

    cMtx_YrotS(*calc_mtx, fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)));
    cXyz offset;
    cXyz pos2;
    cXyz pos;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = -300.0f;
    MtxPosition(&offset, &pos);
    pos += player->current.pos;
    if (dComIfGp_getDetect().chk_light(&pos)) {
        return;
    }

    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = -400.0f;
    MtxPosition(&offset, &pos2);
    pos2 += player->current.pos;
    if (dComIfGp_getDetect().chk_light(&pos2)) {
        return;
    }

    f32 f31 = std::fabsf(player->speedF);
    u32 vol = (u32)(4.0f * f31);
    if (vol > 100) {
        vol = 100;
    }
    fopAcM_seStart(actor, JA_SE_CHR_PW_MOVE, vol);
    f31 *= 1.5f;
    cLib_addCalc2(&actor->current.pos.x, pos.x, 1.0f, f31);
    cLib_addCalc2(&actor->current.pos.z, pos.z, 1.0f, f31);
}

/* 00002714-0000289C       .text hani_check__FP8pw_class */
BOOL hani_check(pw_class* i_this) {
#if VERSION == VERSION_DEMO
    if (i_this->mJalhallaID && fopAcM_SearchByID(i_this->mJalhallaID) != NULL) {
        return FALSE;
    }
#else
    if (i_this->mJalhallaID != fpcM_ERROR_PROCESS_ID_e) {
        return FALSE;
    }
#endif

    f32 range;
    f32 dx;
    f32 dz;
    if (i_this->mPathIndex != 0xFF && i_this->mpPath != NULL) {
        range = 500.0f;
        dx = i_this->m2FC.x - i_this->current.pos.x;
        dz = i_this->m2FC.z - i_this->current.pos.z;
    } else {
        range = 1000.0f;
        dx = i_this->m2F0.x - i_this->current.pos.x;
        dz = i_this->m2F0.z - i_this->current.pos.z;
    }

    f32 dist = std::sqrtf(dx * dx + dz * dz);
    if (dist > range) {
        i_this->m38C = cM_atan2s(dx, dz);
        return TRUE;
    }
    if (i_this->mMode != 0x5B) {
        if (kougen_hani_check(i_this, 1)) {
            i_this->m38C = cM_atan2s(dx, dz);
            return TRUE;
        }
    }
    return FALSE;
}

/* 0000289C-000029C8       .text next_dousa_check__FP8pw_class */
void next_dousa_check(pw_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* const player = dComIfGp_getPlayer(0);
#if VERSION == VERSION_DEMO
    f32 range = 500.0f;
#endif
    i_this->mCyl.SetTgType(AT_TYPE_LIGHT | AT_TYPE_LIGHT_ARROW);
    i_this->mCyl.OffTgNoConHit();
    if (!Line_check(i_this, player->current.pos, 1) && !hani_check(i_this) &&
        fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < DEMO_SELECT(range, 500.0f) &&
        std::fabsf(actor->current.pos.y - player->current.pos.y) < 100.0f)
    {
        i_this->mAction = 1;
        i_this->mMode = 0x20;
    } else {
        i_this->m378[2] = cM_rndF(70.0f) + 70.0f;
        if (i_this->mPathIndex == 0xFF) {
            i_this->m2F0.x = actor->current.pos.x;
            i_this->m2F0.z = actor->current.pos.z;
        }
        i_this->mAction = 0;
        i_this->mMode = 0xD;
    }
}

/* 000029C8-00002A54       .text move_sound__FP8pw_class */
void move_sound(pw_class* i_this) {
    fopAcM_seStart(i_this, JA_SE_CHR_PW_MOVE, (u32)(i_this->speedF * 4.0f));
}

/* 00002A54-00002A90       .text first_mode_change__FP8pw_class */
void first_mode_change(pw_class* i_this) {
    i_this->m340 = 0;
    i_this->m39A = 0x64;
    i_this->m39C = 0x64;
    i_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
    fopAcM_OnStatus(i_this, fopAcStts_SHOWMAP_e);
    i_this->m38C = i_this->shape_angle.y;
    i_this->current.angle.y = i_this->m38C;
}

/* 00002A90-00003B08       .text action_dousa__FP8pw_class */
void action_dousa(pw_class* i_this) {
    /* Nonmatching */
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    camera_process_class* camera = dComIfGp_getCamera(0);
    cXyz camfwd;
#if VERSION == VERSION_DEMO
    u32 kanteraParams = 0xFF000001;
    f32 range;
#endif
    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, dRes_INDEX_PW_BCK_WAIT1_e, 7.0, J3DFrameCtrl::EMode_LOOP, 1.0, -1);
#if VERSION > VERSION_DEMO
        i_this->m346 = 0;
#endif
        i_this->mMode += 1;
        break;
    case 1:
#if VERSION == VERSION_DEMO
        range = 500.0f;
        if (fopAcM_searchActorDistance(i_this, player) < range) {
#else
        if (fopAcM_searchActorDistance(i_this, player) < 500.0f) {
#endif
            anm_init(i_this, dRes_INDEX_PW_BCK_DERUA1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m38C = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
            i_this->current.angle.y = i_this->m38C;
            i_this->shape_angle.y = i_this->m38C;
            i_this->m3A4 = -80.0f;
            i_this->m340 = 1;
            i_this->mMode += 1;
        }
        break;
    case 2:
        i_this->shape_angle.y += 0x1000;
        cLib_addCalc0(&i_this->m3A4, 1.0f, 3.0f);
        i_this->m39A += 7;
        if (i_this->m39A > 100) {
            i_this->m39A = 100;
        }
        if (i_this->mpMorf->checkFrame(25.0f)) {
            i_this->m3A4 = 0.0f;
            first_mode_change(i_this);
            i_this->m38C = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
            JPABaseEmitter* particle = dComIfGp_particle_set(dPa_name::ID_AK_SN_POUGETLAMP00, &i_this->m2CC, &i_this->shape_angle);
            if (particle != NULL) {
                particle->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(PW_JNT_J_PW_ITEM_R1_e));
            }
            i_this->attention_info.flags = 0;
            i_this->mMode = 0x0A;
        }
        break;
    case 9:
        i_this->m39A = 0;
        i_this->m39C = 0;
        i_this->mMode += 1;
        // Fall-through
    case 10:
        if (i_this->mBehaviorType == InvisibleAtStart) {
#if VERSION == VERSION_DEMO
            kanteraParams = 0xFF000007;
#endif
            i_this->m38C = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
        }
#if VERSION == VERSION_DEMO
        i_this->mKanteraID = fopAcM_create(fpcNm_KANTERA_e, kanteraParams, &i_this->m2CC, fopAcM_GetRoomNo(i_this));
#else
        i_this->mKanteraID = fopAcM_create(fpcNm_KANTERA_e, 0xFF000001, &i_this->m2CC, i_this->current.roomNo);
#endif
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e) {
#if VERSION > VERSION_DEMO
            i_this->m378[5] = 5;
#endif
            switch (i_this->mBehaviorType) {
            case InvisibleAtStart:
                i_this->mMode = 0xB;
                break;
            case OnlyLanternVisibleAtStart:
#if VERSION == VERSION_DEMO
                i_this->m346 = 1;
#endif
                i_this->m341 = 2;
                i_this->mMode = 8;
                break;
            default:
                i_this->m346 = 1;
                i_this->mMode = 0xD;
                break;
            }
        }
        break;
    case 11:
        dKyr_get_vectle_calc(&i_this->current.pos, &camera->view.mLookat.mEye, &camfwd);
        i_this->mActorPlace.pos.x = i_this->m2CC.x + camfwd.x * 150.0f;
        i_this->mActorPlace.pos.y = i_this->m2CC.y;
        i_this->mActorPlace.pos.z = i_this->m2CC.z + camfwd.z * 150.0f;
        fopAc_ac_c* kantera;
#if VERSION == VERSION_DEMO
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e &&
            (kantera = fopAcM_SearchByID(i_this->mKanteraID)) != NULL)
#else
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e &&
            fopAcM_SearchByID(i_this->mKanteraID, &kantera) && kantera != NULL &&
            fopAcM_GetName(kantera) == fpcNm_KANTERA_e)
#endif
        {
            kantera->current.pos = i_this->m2CC;
            kantera->current.angle.y = i_this->shape_angle.y;
            kantera->scale.setall(1.0f);
            if (i_this->mpMorf->isStop()) {
                i_this->m341 = 0;
                i_this->mBehaviorType = VisibleFromStart;
#if VERSION == VERSION_DEMO
                i_this->m346 = 1;
                first_mode_change(i_this);
#else
                first_mode_change(i_this);
                i_this->m378[5] = 3;
                i_this->m346 = 1;
#endif
                i_this->mMode = 0xD;
            }
        }
        break;
    case 6:
        i_this->m39A += 10;
        if (i_this->m39A > 100) {
            i_this->m39A = 100;
        }
        if (i_this->mpMorf->isStop()) {
            i_this->mBehaviorType = VisibleFromStart;
            i_this->m346 = 1;
            first_mode_change(i_this);
            i_this->mMode = 0xD;
        }
        break;
    case 7:
        if (fopAcM_searchActorDistance(i_this, player) < i_this->mNoticeRange) {
            i_this->m38C = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
            i_this->current.angle.y = i_this->m38C;
            i_this->shape_angle.y = i_this->m38C;
            anm_init(i_this, dRes_INDEX_PW_BCK_DERUB2_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode = 6;
        }
        break;
    case 8:
        i_this->m378[4] = 2;
#if VERSION > VERSION_DEMO
        i_this->m346 = 1;
#endif
        if (fopAcM_searchActorDistance(i_this, dComIfGp_getPlayer(0)) < i_this->m3AC) {
            anm_init(i_this, dRes_INDEX_PW_BCK_DERUB1_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->mMode = 7;
        }
        break;
    case 13:
        for (int i = 0; i < 4; i++) {
            i_this->m384[i] = 0;
        }
        i_this->m340 = 0;
        i_this->m341 = 0;
        i_this->m378[0] = cM_rndF(60.0f) + 60.0f;
        if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
            if (i_this->mBckIdx != dRes_INDEX_PW_BCK_WAIT1_e) {
                anm_init(i_this, dRes_INDEX_PW_BCK_WAIT1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            }
        } else if (i_this->mBckIdx != dRes_INDEX_PW_BCK_WAIT2_e) {
            anm_init(i_this, dRes_INDEX_PW_BCK_WAIT2_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        i_this->mMode += 1;
        // Fall-through
    case 14:
        cLib_addCalc0(&i_this->speedF, 1.0f, 1.0f);
        if (i_this->m378[0]) {
            break;
        }
        i_this->mMode += 1;
        // Fall-through
    case 15:
        i_this->m378[0] = cM_rndF(120.0f) + 120.0f;
        if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
            if (i_this->mBckIdx != dRes_INDEX_PW_BCK_IDOU1_e) {
                anm_init(i_this, dRes_INDEX_PW_BCK_IDOU1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            }
        } else {
            if (i_this->mBckIdx != dRes_INDEX_PW_BCK_IDOU2_e) {
                anm_init(i_this, dRes_INDEX_PW_BCK_IDOU2_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            }
        }
        i_this->m38C += (s16)cM_rndFX(0x4000);
        if (i_this->mPathIndex != 0xFF && i_this->mpPath != NULL) {
            i_this->mMode = 0x14;
        } else {
            i_this->mMode = 0x10;
        }
        break;
    case 16:
        i_this->speedF = 5.0f;
        move_sound(i_this);
        if (i_this->m378[1] == 0) {
            if (Line_check(i_this, i_this->current.pos, 0) || hani_check(i_this)) {
                i_this->m378[1] = 10;
            } else if (i_this->m378[0] == 0) {
                i_this->mMode = 0xF;
            }
        }
        break;
    case 20:
        i_this->m2FC = i_this->current.pos;
        if (i_this->mPathIndex != 0xFF && i_this->mpPath != NULL) {
            i_this->speedF = 5.0f;
            move_sound(i_this);
            dPnt* pnt = &i_this->mpPath->m_points[i_this->mPathPntIdx];
            f32 delta_x = pnt->m_position.x - i_this->current.pos.x;
            f32 delta_z = pnt->m_position.z - i_this->current.pos.z;
            i_this->m38C = cM_atan2s(delta_x, delta_z);
            f32 dist_xz = std::sqrtf(delta_x*delta_x + delta_z*delta_z);
            if (dist_xz < 80.0f) {
                i_this->mPathPntIdx++;
                if (i_this->mPathPntIdx >= i_this->mpPath->m_num) {
                    i_this->mPathPntIdx = 0;
                }
            }
        }
        break;
    case 25:
        i_this->m38C = fopAcM_searchActorAngleY(i_this, player);
        anm_init(i_this, dRes_INDEX_PW_BCK_DAMAGE_K1_e, 9.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->speedF = -20.0f;
        i_this->mMode += 1;
        // Fall-through
    case 26:
        cLib_addCalc0(&i_this->speedF, 1.0f, 1.0f);
        if (std::fabsf(i_this->speedF) < 0.2f) {
            i_this->mAction = 1;
            i_this->mMode = 0x25;
        }
        break;
    case 27:
        anm_init(i_this, dRes_INDEX_PW_BCK_DOKI1_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->speedF = 0.0f;
        i_this->speed.setall(0.0f);
        i_this->mMode += 1;
        // Fall-through
    case 28:
        if (!fopAcM_CheckStatus(i_this, fopAcStts_HOOK_CARRY_e)) {
            i_this->mMode = 0x5A;
        }
        break;
    case 100:
        anm_init(i_this, dRes_INDEX_PW_BCK_JITTAIKA1_e, 6.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode += 1;
        break;
    case 101:
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        i_this->mMode = 0x5A;
        // Fall-through
    case 90:
        i_this->speedF = 12.0f;
        i_this->m378[0] = 0x168;
        if (i_this->mJalhallaID != fpcM_ERROR_PROCESS_ID_e) {
            i_this->speedF = 9.0f + REG0_F(1);
        }
        if (i_this->mBckIdx != dRes_INDEX_PW_BCK_NIGERU1_e) {
            anm_init(i_this, dRes_INDEX_PW_BCK_NIGERU1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, dRes_INDEX_PW_BAS_NIGERU1_e);
        }
#if VERSION > VERSION_DEMO
        i_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
#endif
        i_this->m38C = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0)) + 0x8000;
        i_this->m340 = 0;
        i_this->mMode += 1;
        // Fall-through
    case 91:
        if (i_this->mpMorf->checkFrame(0.0f)) {
            fopAcM_monsSeStart(i_this, JA_SE_CV_PW_RUN, 0);
        }
        if (i_this->m378[3] == 0) {
            i_this->m38C += (s16)cM_rndFX(16384.0f);
            i_this->m378[3] = cM_rndF(15.0f) + 15.0f;
        }
        if (i_this->m378[1] == 0) {
            if (Line_check(i_this, i_this->current.pos, 0) || hani_check(i_this)) {
                i_this->m378[1] = cM_rndF(20.0f) + 20.0f;
                i_this->m378[3] = cM_rndF(20.0f) + 20.0f;
            }
        }
        if (i_this->m378[0] == 0 && i_this->mJalhallaID == fpcM_ERROR_PROCESS_ID_e &&
            !dComIfGp_getDetect().chk_light(&i_this->current.pos))
        {
            i_this->mCyl.OffTgNoConHit();
            anm_init(i_this, dRes_INDEX_PW_BCK_ANSIN1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m33E = 0;
            i_this->mMode += 1;
            if (i_this->m33F != 0) {
                J3DModel* model = i_this->mpMorf->getModel();
                i_this->m33F = 0;
                i_this->m2C8->init(
                    model->getModelData(),
                    (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_DEFAULT_e),
                    TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, FALSE
                );
            }
        }
        Big_pow_gattai_check(i_this);
        break;
    case 92:
        fopAcM_seStart(i_this, JA_SE_CM_PW_BECOME_CLEAR, 0);
        cLib_addCalc0(&i_this->speedF, 1.0f, 1.0f);
        i_this->m39A -= 3;
        if (i_this->m39A < 100) {
            if (i_this->mpMorf->isStop()) {
                i_this->m38E = 0;
                i_this->m39A = 100;
#if VERSION > VERSION_DEMO
                i_this->m39C = 100;
#endif
                next_dousa_check(i_this);
            }
        }
        break;
    case 110:
        i_this->mActorPlace.pos = i_this->current.pos;
        i_this->mActorPlace.angle = i_this->shape_angle;
#if VERSION == VERSION_DEMO
        i_this->m5C0_demo =
#endif
        dComIfGp_particle_set(
            dPa_name::ID_AK_SN_BPWBUNRETUSMOKE01,
            &i_this->mActorPlace.pos,
            &i_this->mActorPlace.angle,
            NULL,
            0xFF,
            &i_this->m5C4
        );
        anm_init(i_this, dRes_INDEX_PW_BCK_ATTACK1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->speed.y = 45.0f + REG0_F(2);
        i_this->speed.y += cM_rndF(15.0f + REG0_F(3));
        i_this->gravity = -2.0f + REG0_F(4);
        i_this->speedF = 10.0f + REG0_F(5);
        i_this->speedF += cM_rndF(10.0f + REG0_F(6));
        if (i_this->mJalhallaID != fpcM_ERROR_PROCESS_ID_e) {
            fopAc_ac_c* actor;
#if VERSION == VERSION_DEMO
            actor = fopAcM_SearchByID(i_this->mJalhallaID);
            if (actor != NULL)
#else
            if (fopAcM_SearchByID(i_this->mJalhallaID, &actor) && actor != NULL &&
                fopAcM_GetName(actor) == fpcNm_BPW_e)
#endif
            {
                bpw_class* bpw = (bpw_class*)actor;
                if (bpw->m3F6 == 2) {
                    i_this->gravity = -2.0f + REG0_F(4);
                    i_this->speedF = 8.0f + REG0_F(5);
                    i_this->speedF += cM_rndF(5.0f + REG0_F(6));
                }
            }
        }
        i_this->m340 = 1;
        i_this->mMode += 1;
        break;
    case 111:
        if (DEMO_SELECT(i_this->m5C0_demo, i_this->m5C4.getEmitter()) != NULL) {
            i_this->mActorPlace.pos = i_this->current.pos;
            i_this->mActorPlace.angle = i_this->shape_angle;
        }
        if (!i_this->mAcch.ChkGroundHit()) {
            break;
        }
#if VERSION == VERSION_DEMO
        if (i_this->m5C0_demo) {
            i_this->m5C0_demo->becomeInvalidEmitter();
            i_this->m5C4.remove();
            i_this->m5C0_demo = NULL;
        }
#else
        i_this->m5C4.remove();
#endif
        i_this->speedF = 0.0f;
        i_this->speed.setall(0.0f);
        i_this->gravity = 0.0f;
        anm_init(i_this, dRes_INDEX_PW_BCK_SIRIMOTI1_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode += 1;
        // Fall-through
    case 112:
        if (i_this->mpMorf->isStop()) {
            i_this->mMode = 0x5A;
        }
        break;
    }
    
    if (i_this->mMode <= 11) {
        return;
    }
    if (i_this->m378[2] == 0 && i_this->mMode < 90) {
        if (i_this->mMode == 14 || i_this->mMode == 16 || i_this->mMode == 20) {
#if VERSION == VERSION_DEMO
            if (!hani_check(i_this)) {
                range = 500.0f;
                if (fopAcM_searchActorDistance(i_this, dComIfGp_getPlayer(0)) < range && std::fabsf(i_this->current.pos.y - player->current.pos.y) < 100.0f) {
                    if (!Line_check(i_this, player->current.pos, 1) && (i_this->m346 != 0 || !TORITUKI_ON)) {
                        i_this->mAction = 1;
                        i_this->mMode = 30;
                    }
                }
            }
#else
            if (!hani_check(i_this) && fopAcM_searchActorDistance(i_this, dComIfGp_getPlayer(0)) < 500.0f && std::fabsf(i_this->current.pos.y - player->current.pos.y) < 100.0f) {
                if (!Line_check(i_this, player->current.pos, 1) && (i_this->m346 == 1 || !TORITUKI_ON)) {
                    i_this->mAction = 1;
                    i_this->mMode = 30;
                }
            }
#endif
        }
    }
    
    if (i_this->mMode >= 10 && i_this->mMode < 90) {
        alpha_anime(i_this);
    }
    
    if (i_this->mJalhallaID == DEMO_SELECT(0, fpcM_ERROR_PROCESS_ID_e)) {
        fuwafuwa_calc(i_this);
    }
}

/* 00003B08-000042B8       .text action_kougeki__FP8pw_class */
void action_kougeki(pw_class* i_this) {
    /* Nonmatching */
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    switch (i_this->mMode) {
    case 30:
        if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
            anm_init(i_this, dRes_INDEX_PW_BCK_KIDUKU1_e, 6.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        } else {
            anm_init(i_this, dRes_INDEX_PW_BCK_KIDUKU2_e, 6.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        }
        i_this->m340 = 0;
        i_this->mMode += 1;
        break;
    case 31:
        kyori_sub(i_this);
        i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        cLib_addCalc0(&actor->speedF, 1.0f, 0.3f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        i_this->mMode += 1;
        // Fall-through
    case 32:
        kyori_sub(i_this);
        for (int i = 0; i < 4; i++) {
            i_this->m384[i] = 0;
        }
        i_this->m340 = 0;
        if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
            anm_init(i_this, dRes_INDEX_PW_BCK_ATTACK1_e, 6.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_PW_BAS_ATTACK1_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_PW_ATTACK, 0);
            i_this->m341 = 1;
            i_this->mMode = 33;
        } else {
            anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI1_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m378[0] = 100;
            i_this->mMode = 40;
        }
        break;
    case 33:
        kyori_sub(i_this);
        cLib_addCalc0(&actor->speedF, 1.0f, 0.3f);
        i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        if (i_this->mpMorf->checkFrame(23.0f)) {
            actor->speedF = 24.0f;
            i_this->mMode += 1;
        }
        break;
    case 34:
        move_sound(i_this);
        if (kougen_hani_check(i_this, 0)) {
            i_this->mMode = 37;
            break;
        }
        if (kougen_hani_check(i_this, 1)) {
            i_this->mMode = 37;
            break;
        }
        if (i_this->mpMorf->checkFrame(28.0f)) {
            i_this->mSph.OnAtSetBit();
            i_this->mSph.OnAtHitBit();
        }
        if (i_this->mpMorf->getFrame() > 28.0f) {
            if (i_this->mSph.ChkAtShieldHit()) {
                i_this->mSph.OffAtSetBit();
                i_this->mSph.ClrAtSet();
                i_this->mAction = 0;
                i_this->mMode = 25;
                return;
            }
        }
        if (i_this->mpMorf->checkFrame(40.0f)) {
            i_this->mSph.OffAtSetBit();
            i_this->mSph.ClrAtSet();
            i_this->mMode += 1;
        }
        break;
    case 35:
        cLib_addCalc0(&actor->speedF, 1.0f, 5.0f);
        if (std::fabsf(actor->speedF) < 0.2f) {
            i_this->mMode = 37;
        }
        break;
    case 40:
        cLib_addCalc0(&actor->speedF, 1.0f, 0.3f);
        i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        if (i_this->m378[0] == 0) {
            i_this->m378[0] = 300;
            anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI2_e, 4.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_PW_GO_CURSE, 0);
            i_this->mMode += 1;
        }
        break;
    case 41:
        i_this->m38C = fopAcM_searchActorAngleY(actor, player);
        if (kougen_hani_check(i_this, 0)) {
            i_this->mMode = 37;
            break;
        }
        if (kougen_hani_check(i_this, 1)) {
            i_this->mMode = 37;
            break;
        }
        cLib_addCalc2(&actor->speedF, 15.0f, 1.0f, 3.0f);
        move_sound(i_this);
        if (i_this->m378[0] == 0) {
            anm_init(i_this, dRes_INDEX_PW_BCK_IKIGIRE1_e, 6.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mMode = 36;
            break;
        }
        if (Line_check(i_this, player->current.pos, 1)) {
            i_this->m378[2] = cM_rndF(70.0f) + 70.0f;
            if (i_this->mPathIndex == 0xFF) {
                i_this->m2F0.x = actor->current.pos.x;
                i_this->m2F0.z = actor->current.pos.z;
            }
            i_this->mAction = 0;
            i_this->mMode = 13;
        }
        break;
    case 36:
        cLib_addCalc0(&actor->speedF, 1.0f, 1.0f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        i_this->mMode = 37;
        break;
    case 37:
        for (int i = 0; i < 4; i++) {
            i_this->m384[i] = 0;
        }
        actor->speedF = 0.0f;
        if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
            if (i_this->mBckIdx != dRes_INDEX_PW_BCK_WAIT1_e) {
                anm_init(i_this, dRes_INDEX_PW_BCK_WAIT1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            }
        } else if (i_this->mBckIdx != dRes_INDEX_PW_BCK_WAIT2_e) {
            anm_init(i_this, dRes_INDEX_PW_BCK_WAIT2_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        i_this->m378[2] = cM_rndF(70.0f) + 70.0f;
        i_this->m320 = actor->current.pos;
        i_this->m392 = 0;
        i_this->m340 = 0;
        i_this->m341 = 0;
        i_this->mMode += 1;
        // Fall-through
    case 38:
        kyori_sub(i_this);
        i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        cLib_addCalc0(&actor->speedF, 1.0f, 1.0f);
        if (i_this->m378[2] == 0) {
            actor->speedF = 0.0f;
            next_dousa_check(i_this);
        }
        break;
    case 39:
        break;
    }

    if (i_this->m346 == 0) {
        if (TORITUKI_ON == true) {
            i_this->m378[2] = cM_rndF(70.0f) + 70.0f;
            if (i_this->mPathIndex == 0xFF) {
                i_this->m2F0.x = actor->current.pos.x;
                i_this->m2F0.z = actor->current.pos.z;
            }
            i_this->mAction = 0;
            i_this->mMode = 13;
        } else if (i_this->mMode == 40 || i_this->mMode == 41 || i_this->mMode == 38) {
            if (i_this->mCyl.ChkAtHit()) {
                fopAc_ac_c* hitac = i_this->mCyl.GetAtHitAc();
                if (hitac != NULL && hitac == player) {
                    actor->speedF = 0.0f;
                    i_this->mCyl.OffAtSetBit();
                    i_this->mCyl.ClrAtSet();
#if VERSION == VERSION_DEMO
                    i_this->mCyl.OffTgSetBit();
                    i_this->mCyl.ClrTgHit();
#else
                    i_this->mCyl.SetTgType(AT_TYPE_LIGHT);
#endif
                    TORITUKI_ON = true;
                    i_this->m343 = 1;
                    i_this->mAction = 3;
                    i_this->mMode = 70;
                }
            }
        }
    }
    alpha_anime(i_this);
    fuwafuwa_calc(i_this);
}

/* 000042B8-00004C50       .text action_itai__FP8pw_class */
void action_itai(pw_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* const player = dComIfGp_getPlayer(0);
    J3DModel* model = i_this->mpMorf->getModel();
    switch (i_this->mMode) {
    case 50:
        i_this->mBehaviorType = VisibleFromStart;
        i_this->m340 = 1;
        i_this->m341 = 1;
        actor->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
        fopAcM_OnStatus(actor, fopAcStts_SHOWMAP_e);
        i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        actor->current.angle.y = i_this->m38C;
        actor->shape_angle.y = i_this->m38C;
        i_this->m38E = 0;
        i_this->m39A = 200;
        i_this->m39C = 200;
        i_this->m378[3] = 3;
        if (i_this->m398 == 0) {
            kantera_break(i_this);
        }
        actor->speedF = -20.0f;
        anm_init(i_this, dRes_INDEX_PW_BCK_DAMAGE_K1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode += 1;
        break;
    case 51:
        cLib_addCalc0(&actor->speedF, 1.0f, 1.0f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        actor->speedF = 0.0f;
        next_dousa_check(i_this);
        break;
    case 52:
        kantera_break(i_this);
        i_this->m340 = 1;
        i_this->m38C = actor->current.angle.y;
        actor->shape_angle.y = i_this->m38C;
        for (int i = 0; i < 4; i++) {
            i_this->m384[i] = 0;
        }
        if (i_this->mBckIdx != dRes_INDEX_PW_BCK_DOKI1_e) {
            anm_init(i_this, dRes_INDEX_PW_BCK_DOKI1_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        i_this->m378[0] = 30;
        actor->speedF = 0.0f;
        i_this->mMode += 1;
        // Fall-through
    case 53:
        if (i_this->mHitByWhat == 9 || i_this->mHitByWhat == 10) {
            i_this->m378[0] = 10;
            i_this->m39A += 5;
            if (i_this->m33F == 0) {
                i_this->m33F = 1;
                i_this->m2C4->init(
                    model->getModelData(),
                    (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_HIT_e),
                    TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, true, FALSE
                );
            }
            if (i_this->mHitByWhat == 10) {
                i_this->m39A = 0xFF;
            }
            if (i_this->m39A >= 0xFF) {
                i_this->m39A = 0xFF;
                i_this->m38E = 0;
                fopAcM_seStart(actor, JA_SE_CM_PW_SOLID_END, 0);
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_APPEAR, 0);
                i_this->mCyl.OnTgNoConHit();
                i_this->m33E = 1;
                i_this->m2C0->init(
                    model->getModelData(),
                    (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_JITTAIKA1_e),
                    TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, FALSE
                );
                i_this->mCyl.SetTgType(AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT);
                i_this->mAction = 0;
                i_this->mMode = 100;
                return;
            }
            fopAcM_seStart(actor, JA_SE_CM_PW_BECOME_SOLID, 0);
        } else {
            if (i_this->m33F != 0) {
                i_this->m33F = 0;
                i_this->m2C8->init(
                    model->getModelData(),
                    (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_DEFAULT_e),
                    TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, FALSE
                );
            }
            i_this->m39C = 0;
            i_this->m39A = 0;
            i_this->m38E = 0;
            next_dousa_check(i_this);
        }
        break;
    case 54:
        i_this->m340 = 1;
        i_this->m378[4] = 0;
#if VERSION == VERSION_DEMO
        if (i_this->m5C0_demo) {
            i_this->m5C0_demo->becomeInvalidEmitter();
            i_this->m5C0_demo = NULL;
        }
#else
        i_this->m5C4.remove();
#endif
        i_this->m38C = actor->shape_angle.y;
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        actor->shape_angle.y = i_this->m38C;
        actor->speedF = -24.0f;
        if (actor->health <= 0) {
            i_this->mCyl.OffAtSetBit();
            i_this->mCyl.OffAtSetBit();
            i_this->mCyl.OffTgSetBit();
            i_this->mCyl.ClrTgHit();
            actor->speedF *= 1.5f;
        }
        fopAcM_monsSeStart(actor, JA_SE_CV_PW_DAMAGE, 0);
        anm_init(i_this, dRes_INDEX_PW_BCK_DAMAGE1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode += 1;
        break;
    case 55:
        if (Big_pow_gattai_check(i_this)) {
            break;
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 1.0f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        actor->speedF = 0.0f;
        if (actor->health > 0) {
            i_this->mAction = 0;
            i_this->mMode = 90;
        } else {
            i_this->mMode = 60;
        }
        break;
    case 56:
        actor->current.angle.y = player->shape_angle.y;
        i_this->m340 = 1;
        actor->speedF = 24.0f;
        i_this->mpMorf->setPlaySpeed(0.25f);
        i_this->mMode += 1;
        // Fall-through
    case 57:
        if (Big_pow_gattai_check(i_this)) {
            break;
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 1.0f);
        if (actor->speedF < 0.2f) {
            actor->speedF = 0.0f;
            actor->current.angle.y = actor->shape_angle.y;
            i_this->m340 = 0;
            i_this->mpMorf->setPlaySpeed(1.0f);
            i_this->mAction = 0;
            i_this->mMode = 90;
        }
        break;
    case 58:
        i_this->m340 = 1;
        actor->speedF = 0.0f;
        if (i_this->mHitByWhat == 4) {
            enemy_piyo_set(actor);
            fopAcM_seStart(actor, JA_SE_CM_MD_PIYO, 0);
#if VERSION > VERSION_DEMO
            i_this->m378[0] = REG0_S(7) + 75;
#endif
        }
        anm_init(i_this, dRes_INDEX_PW_BCK_KIZETU1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode += 1;
        // Fall-through
    case 59:
        if (Big_pow_gattai_check(i_this)) {
            break;
        }
#if VERSION == VERSION_DEMO
        if (!i_this->mpMorf->isStop()) {
            break;
        }
#else
        if (i_this->m378[0] != 0) {
            break;
        }
#endif
        i_this->mAction = 0;
        i_this->mMode = 90;
        break;
    case 60:
        anm_init(i_this, dRes_INDEX_PW_BCK_SYOUTEN1_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->m33E = 0;
        actor->attention_info.flags = 0;
#if VERSION == VERSION_DEMO
        if (i_this->m5C0_demo) {
            i_this->m5C0_demo->becomeInvalidEmitter();
            i_this->m5C0_demo = NULL;
        }
#else
        i_this->m5C4.remove();
#endif
        if (i_this->m384[3] == 0) {
            fopAcM_seStart(actor, JA_SE_CM_PW_GO_HEAVEN, 0);
            fopAcM_monsSeStart(actor, JA_SE_CV_PW_DIE, 0);
        }
        if (i_this->m33F != 0) {
            i_this->m33F = 0;
            i_this->m2C8->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_DEFAULT_e),
                TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, FALSE
            );
        }
        actor->gravity = 0.6f;
        i_this->mMode += 1;
        // Fall-through
    case 61:
        i_this->m39A -= 8;
        if (i_this->m39A < 0) {
            i_this->m38E = 0;
            i_this->m39A = 0;
            if (i_this->m384[3] == 0) {
                cXyz pos = actor->current.pos;
                pos.y += 60.0f;
                fopAcM_createDisappear(actor, &pos, 5, daDisItem_IBALL_e, 0xFF);
            }
            fopAcM_delete(actor);
            fopAcM_onActor(actor);
        }
        break;
    case 62:
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        i_this->mMode = 60;
        break;
    }

    if (i_this->mMode <= 51) {
        if (i_this->m378[3] == 0) {
            alpha_anime(i_this);
        }
        fuwafuwa_calc(i_this);
    }
}

/* 00004C50-00004FFC       .text action_demo__FP8pw_class */
void action_demo(pw_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = daPy_getPlayerActorClass();
    camera_process_class* camera = (camera_process_class*)dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    cXyz pos = player->current.pos;
    switch (i_this->mMode) {
    case 0x46:
        fopAcM_OnStatus(actor, fopAcStts_UNK4000_e);
        i_this->mMode += 1;
        // Fall-through
    case 0x47:
        if (!actor->eventInfo.checkCommandDemoAccrpt()) {
            dComIfGp_event_onEventFlag(dEvtFlag_NOPARTNER_e);
            fopAcM_orderPotentialEvent(actor, dEvtFlag_STAFF_ALL_e, 0xFFFF, 0);
            actor->eventInfo.onCondition(dEvtCnd_UNK2_e);
            break;
        }
        player->changeOriginalDemo();
        player->changeDemoMode(daPy_demo_c::DEMO_N_WAIT_e);
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(2);
        i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        fopAcM_monsSeStart(actor, JA_SE_CV_PW_CURSE_IN, 0);
        fopAcM_seStart(actor, JA_SE_CM_PW_CURSE_START, 0);
#if VERSION == VERSION_DEMO
        i_this->mCyl.OffAtSetBit();
        i_this->mCyl.OffAtSetBit();
        i_this->mCyl.OffTgSetBit();
        i_this->mCyl.ClrTgHit();
#endif
        anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI3_e, 4.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mMode += 1;
        break;
    case 0x48:
        cLib_addCalc2(&i_this->current.pos.x, player->current.pos.x, 1.0f, 5.0f);
        cLib_addCalc2(&i_this->current.pos.z, player->current.pos.z, 1.0f, 5.0f);
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI_WAIT1_e, 4.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            pos.y += 100.0f;
            i_this->m340 = 1;
            i_this->current.pos = pos;
            i_this->attention_info.flags = 0;
            i_this->shape_angle.x = 0;
            i_this->shape_angle.y = 0;
            i_this->shape_angle.z = 0;
            i_this->shape_angle.y = player->shape_angle.y;
            i_this->m378[0] = 30;
            i_this->mMode += 1;
        }
        break;
    case 0x49:
        if (i_this->m378[0] == 0) {
#if VERSION > VERSION_DEMO
            daPy_py_c* player2 = (daPy_py_c*)dComIfGp_getLinkPlayer();
#endif
            dComIfGp_getVibration().StopQuake(0x20);
            camera->mCamera.Start();
            camera->mCamera.SetTrimSize(0);
            player->cancelOriginalDemo();
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            fopAcM_OffStatus(actor, fopAcStts_UNK4000_e);
            i_this->mAction = 4;
            i_this->mMode = 0x50;
#if VERSION > VERSION_DEMO
            if (player != player2) {
                if (i_this->m343 != 0) {
                    i_this->m343 = 0;
                    TORITUKI_ON = false;
                    fopAcM_seStart(actor, JA_SE_CM_PW_CURSE_END, 0);
                }
                i_this->m384[3] = 1;
                i_this->m38E = 0;
                i_this->m39A = 0xFF;
                i_this->mAction = 2;
                i_this->mMode = 0x3C;
            }
#endif
        }
        break;
    }
}

/* 00004FFC-000052B8       .text action_torituku__FP8pw_class */
void action_torituku(pw_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#if VERSION > VERSION_DEMO
    fopAc_ac_c* player2 = dComIfGp_getLinkPlayer();
#endif
    cXyz pos = player->current.pos;
#if VERSION > VERSION_DEMO
    u8 cancel = 0;
#endif
    switch (i_this->mMode) {
    case 0x50:
        for (int i = 0; i < 4; i++) {
            i_this->m384[i] = 0;
        }
        i_this->m378[0] = 300;
        i_this->m33E = 1;
        fopAcM_OnStatus(actor, fopAcStts_UNK4000_e);
        player->onConfuse();
        i_this->mMode += 1;
        // Fall-through
    case 0x51:
        fopAcM_seStart(actor, JA_SE_LK_NOW_CURSE_PW, 0);
        i_this->m384[0] += 700;
        pos.y = 100.0f + player->current.pos.y + 10.0f * cM_ssin(i_this->m384[0]);
        i_this->current.pos = pos;
        i_this->shape_angle.y += 1000;
#if VERSION == VERSION_DEMO
        if (i_this->m378[0] == 0 || dComIfGp_getDetect().chk_light(&i_this->current.pos) ||
            dComIfGp_checkPlayerStatus1(0, daPyStts1_UNK2000_e) || player->getDamageWaitTimer() ||
            player->checkFairyUse())
        {
#else
        if (i_this->m378[0] == 0 || dComIfGp_getDetect().chk_light(&i_this->current.pos) ||
            dComIfGp_checkPlayerStatus1(0, daPyStts1_UNK2000_e) || player->getDamageWaitTimer() ||
            player != player2 || player->checkFairyUse())
        {
            cancel = 1;
        }
        if (i_this->mCyl.ChkTgHit()) {
            cCcD_Obj* hitObj = i_this->mCyl.GetTgHitObj();
            if (hitObj != NULL && (hitObj->GetAtType() & AT_TYPE_LIGHT)) {
                cancel = 1;
            }
        }
        if (cancel) {
#endif
            if (i_this->m343 != 0) {
                i_this->m343 = 0;
                TORITUKI_ON = false;
                fopAcM_seStart(actor, JA_SE_CM_PW_CURSE_END, 0);
            }
            i_this->m384[3] = 1;
            i_this->m38E = 0;
            i_this->m39A = 0xFF;
            player->offConfuse();
            i_this->mAction = 2;
            i_this->mMode = 0x3C;
        }
        break;
    }
    s16 target = 0xFF;
    cLib_addCalcAngleS2(&i_this->m39A, target, 1, 10);
}

/* 000052B8-00005CA4       .text action_big_demo__FP8pw_class */
#if VERSION == VERSION_DEMO
void action_big_demo(pw_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    fopAc_ac_c* bpw_actor = fopAcM_SearchByID(i_this->mJalhallaID);
    cXyz playerPos = player->current.pos;
    {
        bpw_class* bpw = (bpw_class*)bpw_actor;
        switch (i_this->mMode) {
        case 150:
            for (int i = 0; i < 4; i++) {
                i_this->m384[i] = 0;
            }
            i_this->m39A = 0xFF;
            i_this->m38E = 0;
            actor->speedF = 17.0f + REG0_F(1);
            anm_init(i_this, dRes_INDEX_PW_BCK_NIGERU1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, dRes_INDEX_PW_BAS_NIGERU1_e);
            i_this->mMode += 1;
            // Fall-through
        case 151:
            if (i_this->mpMorf->checkFrame(0.0f)) {
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_RUN, 0);
            }
            if (i_this->m378[3] == 0) {
                i_this->m38C += (s16)cM_rndFX(16384.0f);
                i_this->m378[3] = cM_rndF(15.0f) + 15.0f;
            }
            if (i_this->m378[1] == 0) {
                if (Line_check(i_this, actor->current.pos, 0)) {
                    i_this->m378[1] = cM_rndF(20.0f) + 20.0f;
                    i_this->m378[3] = cM_rndF(20.0f) + 20.0f;
                } else {
                    f32 dx = i_this->m2F0.x - actor->current.pos.x;
                    f32 dz = i_this->m2F0.z - actor->current.pos.z;
                    f32 dist = std::sqrtf(dx * dx + dz * dz);
                    if (dist > 1000.0f + REG9_F(6)) {
                        i_this->m38C = cM_atan2s(dx, dz);
                        i_this->m378[1] = cM_rndF(20.0f) + 20.0f;
                        i_this->m378[3] = cM_rndF(20.0f) + 20.0f;
                    }
                }
            }
            if (i_this->mJalhallaID != 0 && bpw_actor != NULL && bpw->m3EA != 0) {
                anm_init(i_this, dRes_INDEX_PW_BCK_WAIT2_e, 2.0f + cM_rndF(7.0f), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                i_this->mMode += 1;
            }
            break;
        case 152:
            cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
            if (i_this->mJalhallaID != 0 && bpw_actor != NULL && bpw->m3EA == 2) {
                i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
                i_this->mMode += 1;
            }
            break;
        case 153:
            if (i_this->mJalhallaID != 0 && bpw_actor != NULL && bpw->m3EA == 3) {
                anm_init(i_this, dRes_INDEX_PW_BCK_KIDUKU2_e, 2.0f + cM_rndF(7.0f), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                i_this->mMode += 1;
            }
            break;
        case 154:
            if (i_this->mJalhallaID != 0 && bpw_actor != NULL && bpw->m3EA == 4) {
                anm_init(i_this, dRes_INDEX_PW_BCK_NIGERU1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, dRes_INDEX_PW_BAS_NIGERU1_e);
                i_this->mMode += 1;
            }
            break;
        case 155:
            if (i_this->mpMorf->checkFrame(0.0f)) {
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_RUN, 0);
            }
            actor->speedF = 10.0f + REG9_F(7);
            i_this->m39A -= 3;
            if (i_this->m39A < 100) {
                i_this->m39A = 100;
            }
            if (i_this->mJalhallaID == 0 || bpw_actor == NULL) {
                break;
            }
            if (bpw->m3EA == 4) {
                cLib_addCalcAngleS2(&i_this->m39E, 2000.0f + REG9_F(8), 1, 100.0f + REG9_F(9));
                actor->speedF = 40.0f + REG9_F(10);
                i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
            } else if (bpw->m3EA == 5) {
                i_this->m39E = (s16)(1000.0f + REG9_F(11));
                i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
                i_this->m38C += -0x8000;
                if (i_this->m384[0] == 0) {
                    i_this->m384[0] = (s16)cM_rndF(40.0f + REG9_F(13));
                }
            } else if (bpw->m3EA == 6) {
                if (i_this->m384[0] > 0) {
                    i_this->m384[0]--;
                    break;
                }
                anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI2_e, 1.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_GO_CURSE, 0);
                fopAcM_seStart(actor, JA_SE_CM_PW_BECOME_CLEAR, 0);
                i_this->m39E = (s16)(2000.0f + REG9_F(12));
                i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
                i_this->mMode = 171;
            }
            break;
        case 170:
            anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI2_e, 4.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_PW_GO_CURSE, 0);
            fopAcM_seStart(actor, JA_SE_CM_PW_BECOME_CLEAR, 0);
            i_this->m33E = 0;
            actor->speedF = 20.0f + REG0_F(7);
            i_this->mMode += 1;
            // Fall-through
        case 171:
            i_this->m39A -= 3;
            if (i_this->m39A < 100) {
                i_this->m39A = 100;
            }
            if (bpw_actor == NULL) {
                break;
            }
            i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
            {
                f32 dx = bpw_actor->current.pos.x - actor->current.pos.x;
                f32 dz = bpw_actor->current.pos.z - actor->current.pos.z;
                f32 dist = std::sqrtf(dx * dx + dz * dz);
                if (dist < 200.0f + REG0_F(9)) {
                    actor->speedF = 0.0f;
                    fopAcM_monsSeStart(actor, JA_SE_CV_PW_CURSE_IN, 0);
                    fopAcM_seStart(actor, JA_SE_CM_PW_CURSE_START, 0);
                    anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI3_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                    i_this->mMode += 1;
                }
            }
            break;
        case 172:
            if (bpw_actor != NULL) {
                cLib_addCalc2(&actor->current.pos.y, 200.0f + bpw_actor->current.pos.y + REG0_F(8), 1.0f, 10.0f);
                cLib_addCalc2(&actor->current.pos.x, bpw_actor->current.pos.x, 1.0f, 10.0f);
                cLib_addCalc2(&actor->current.pos.z, bpw_actor->current.pos.z, 1.0f, 10.0f);
            }
            if (!i_this->mpMorf->isStop()) {
                break;
            }
            {
                fopAc_ac_c* actor2 = bpw_actor;
                if (actor2 != NULL) {
                    bpw_class* p = (bpw_class*)actor2;
                    p->m464 = 1;
                    p->m462 += 1;
                }
            }
            fopAcM_delete(actor);
            break;
        }
    }
}
#else
void action_big_demo(pw_class* i_this) {
    /* Nonmatching */
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    fopAc_ac_c* bpw_actor = fopAcM_SearchByID(i_this->mJalhallaID);
    cXyz playerPos = player->current.pos;
    if (!fopAcM_SearchByID(i_this->mJalhallaID, &bpw_actor) ||
        (bpw_actor != NULL && fopAcM_GetName(bpw_actor) == fpcNm_BPW_e))
    {
        bpw_class* bpw = (bpw_class*)bpw_actor;
        switch (i_this->mMode) {
        case 150:
            for (int i = 0; i < 4; i++) {
                i_this->m384[i] = 0;
            }
            i_this->m39A = 0xFF;
            i_this->m38E = 0;
            actor->speedF = 17.0f + REG0_F(1);
            anm_init(i_this, dRes_INDEX_PW_BCK_NIGERU1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, dRes_INDEX_PW_BAS_NIGERU1_e);
            i_this->mMode += 1;
            // Fall-through
        case 151:
            if (i_this->mpMorf->checkFrame(0.0f)) {
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_RUN, 0);
            }
            if (i_this->m378[3] == 0) {
                i_this->m38C += (s16)cM_rndFX(16384.0f);
                i_this->m378[3] = cM_rndF(15.0f) + 15.0f;
            }
            if (i_this->m378[1] == 0) {
                if (Line_check(i_this, actor->current.pos, 0)) {
                    i_this->m378[1] = cM_rndF(20.0f) + 20.0f;
                    i_this->m378[3] = cM_rndF(20.0f) + 20.0f;
                } else {
                    f32 dx = i_this->m2F0.x - actor->current.pos.x;
                    f32 dz = i_this->m2F0.z - actor->current.pos.z;
                    f32 dist = std::sqrtf(dx * dx + dz * dz);
                    if (dist > 1000.0f + REG9_F(6)) {
                        i_this->m38C = cM_atan2s(dx, dz);
                        i_this->m378[1] = cM_rndF(20.0f) + 20.0f;
                        i_this->m378[3] = cM_rndF(20.0f) + 20.0f;
                    }
                }
            }
            if (bpw->m3EA == 0) {
                break;
            }
            anm_init(i_this, dRes_INDEX_PW_BCK_WAIT2_e, 2.0f + cM_rndF(7.0f), J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->mMode += 1;
            break;
        case 152:
            cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
            if (bpw->m3EA == 2) {
                i_this->m38C = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
                i_this->mMode += 1;
            }
            break;
        case 153:
            if (bpw->m3EA == 3) {
                anm_init(i_this, dRes_INDEX_PW_BCK_KIDUKU2_e, 2.0f + cM_rndF(7.0f), J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                i_this->mMode += 1;
            }
            break;
        case 154:
            if (bpw->m3EA == 4) {
                anm_init(i_this, dRes_INDEX_PW_BCK_NIGERU1_e, 7.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, dRes_INDEX_PW_BAS_NIGERU1_e);
                i_this->mMode += 1;
            }
            break;
        case 155:
            if (i_this->mpMorf->checkFrame(0.0f)) {
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_RUN, 0);
            }
            actor->speedF = 10.0f + REG9_F(7);
            i_this->m39A -= 3;
            if (i_this->m39A < 100) {
                i_this->m39A = 100;
            }
            if (bpw->m3EA == 4) {
                s16 target = (s16)(2000.0f + REG9_F(8));
                s16 maxStep = (s16)(100.0f + REG9_F(9));
                cLib_addCalcAngleS2(&i_this->m39E, target, 1, maxStep);
                actor->speedF = 40.0f + REG9_F(10);
                i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
            } else if (bpw->m3EA == 5) {
                i_this->m39E = (s16)(1000.0f + REG9_F(11));
                i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
                i_this->m38C += -0x8000;
                if (i_this->m384[0] == 0) {
                    i_this->m384[0] = (s16)cM_rndF(40.0f + REG9_F(13));
                }
            } else if (bpw->m3EA == 6) {
                if (i_this->m384[0] > 0) {
                    i_this->m384[0]--;
                    break;
                }
                anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI2_e, 1.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                fopAcM_monsSeStart(actor, JA_SE_CV_PW_GO_CURSE, 0);
                fopAcM_seStart(actor, JA_SE_CM_PW_BECOME_CLEAR, 0);
                i_this->m39E = (s16)(2000.0f + REG9_F(12));
                i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
                i_this->mMode = 171;
            }
            break;
        case 170:
            anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI2_e, 4.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_PW_GO_CURSE, 0);
            fopAcM_seStart(actor, JA_SE_CM_PW_BECOME_CLEAR, 0);
            i_this->m33E = 0;
            actor->speedF = 20.0f + REG0_F(7);
            i_this->mMode += 1;
            // Fall-through
        case 171:
            i_this->m39A -= 3;
            if (i_this->m39A < 100) {
                i_this->m39A = 100;
            }
            if (bpw_actor == NULL) {
                break;
            }
            i_this->m38C = fopAcM_searchActorAngleY(actor, bpw_actor);
            {
                f32 dx = bpw_actor->current.pos.x - actor->current.pos.x;
                f32 dz = bpw_actor->current.pos.z - actor->current.pos.z;
                f32 dist = std::sqrtf(dx * dx + dz * dz);
                if (dist < 200.0f + REG0_F(9)) {
                    actor->speedF = 0.0f;
                    fopAcM_monsSeStart(actor, JA_SE_CV_PW_CURSE_IN, 0);
                    fopAcM_seStart(actor, JA_SE_CM_PW_CURSE_START, 0);
                    anm_init(i_this, dRes_INDEX_PW_BCK_TORITUKI3_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                    i_this->mMode += 1;
                }
            }
            break;
        case 172:
            if (bpw_actor != NULL) {
                cLib_addCalc2(&actor->current.pos.y, 200.0f + bpw_actor->current.pos.y + REG0_F(8), 1.0f, 10.0f);
                cLib_addCalc2(&actor->current.pos.x, bpw_actor->current.pos.x, 1.0f, 10.0f);
                cLib_addCalc2(&actor->current.pos.z, bpw_actor->current.pos.z, 1.0f, 10.0f);
            }
            if (!i_this->mpMorf->isStop()) {
                break;
            }
            {
                fopAc_ac_c* actor2 = bpw_actor;
                if (actor2 != NULL) {
                    bpw_class* p = (bpw_class*)actor2;
                    p->m464 = 1;
                    p->m462 += 1;
                }
            }
            fopAcM_delete(actor);
            break;
        }
    }
}
#endif

/* 00005CA4-000061FC       .text daPW_Execute__FP8pw_class */
static BOOL daPW_Execute(pw_class* i_this) {
    fopAcM_setGbaName(i_this, 0x3C, 0xD, 0x2D);
    if (enemy_ice(&i_this->mEnemyIce)) {
        i_this->mpMorf->getModel()->setBaseTRMtx(DEMO_SELECT(mDoMtx_stack_c::now, mDoMtx_stack_c::get()));
        i_this->mpMorf->calc();
        return TRUE;
    }

    for (int i = 0; i < DEMO_SELECT(5, 6); i++) {
        if (i_this->m378[i] != 0) {
            i_this->m378[i]--;
        }
    }

    switch (i_this->mAction) {
    case 0:
        action_dousa(i_this);
        break;
    case 1:
        action_kougeki(i_this);
        break;
    case 2:
        action_itai(i_this);
        break;
    case 3:
        action_demo(i_this);
        break;
    case 4:
        action_torituku(i_this);
        break;
    case 5:
        action_big_demo(i_this);
        break;
    }

    if (i_this->m340 == 0) {
        s16 maxStep = 0x1000;
        if (i_this->mMode == 0xF || i_this->mMode == 0x10 || i_this->mMode == 0x14) {
            maxStep = 500;
        }
        if (i_this->mAction == 5) {
            maxStep = i_this->m39E;
        }
        cLib_addCalcAngleS2(&i_this->current.angle.y, i_this->m38C, 1, maxStep);
        cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 1, maxStep);
    }

#if VERSION == VERSION_DEMO
    if (i_this->mMode >= 2 && i_this->m378[4] == 0) {
        i_this->mpMorf->play(&i_this->current.pos, 0, 0);
    }
#endif

    if (i_this->m33E != 0) {
        i_this->m2C0->play();
    } else if (i_this->m33F != 0) {
        i_this->m2C4->play();
    } else {
        i_this->m2C8->play();
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
    i_this->speed.z = moved.z;
    i_this->speed.y += i_this->gravity;
    if (i_this->speed.y < -100.0f) {
        i_this->speed.y = -100.0f;
    }

    if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
        fopAc_ac_c* actor;
#if VERSION == VERSION_DEMO
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e && (actor = fopAcM_SearchByID(i_this->mKanteraID)) != NULL)
#else
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(i_this->mKanteraID, &actor) &&
            actor != NULL && fopAcM_GetName(actor) == fpcNm_KANTERA_e)
#endif
        {
            i_this->attention_info.position = i_this->m2D8;
            i_this->attention_info.position.y += 40.0f;
            i_this->eyePos = i_this->m2D8;
        }
    } else {
        i_this->attention_info.position = i_this->current.pos;
        i_this->attention_info.position.y += 200.0f;
        i_this->eyePos = i_this->current.pos;
        i_this->eyePos.y += 100.0f;
    }

    cXyz cylPos = i_this->current.pos;
    i_this->mCyl.SetC(cylPos);
#if VERSION == VERSION_DEMO
    i_this->mCyl.SetH(200.0f);
    i_this->mCyl.SetR(80.0f);
#else
    if (i_this->mAction == 4) {
        i_this->mCyl.SetH(100.0f);
        i_this->mCyl.SetR(40.0f + REG8_F(14));
    } else {
        i_this->mCyl.SetH(200.0f);
        i_this->mCyl.SetR(80.0f);
    }
#endif
    dComIfG_Ccsp()->Set(&i_this->mCyl);

    if (i_this->mAction != 3 && i_this->mAction != 5 && i_this->mAction != 4 &&
#if VERSION > VERSION_DEMO
        i_this->mMode != 0x3D &&
#endif
        i_this->mMode != 0x6E && i_this->mMode != 0x6F)
    {
        body_atari_check(i_this);
    }

    if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
        fopAcM_posMove(i_this, i_this->mStts.GetCCMoveP());
    } else {
        fopAcM_posMove(i_this, NULL);
    }

    if (i_this->mMode != 0x51 && i_this->mMode != 0x3D && i_this->mMode != 8) {
        BG_check(i_this);
    }

#if VERSION > VERSION_DEMO
    if (i_this->mMode >= 2 && i_this->m378[4] == 0) {
        if (i_this->mAcch.GetGroundH() != -G_CM3D_F_INF) {
            s8 roomNo = fopAcM_GetRoomNo(i_this);
            u32 mtrlSndId;
            if (i_this->mAcch.ChkGroundHit()) {
                mtrlSndId = dComIfG_Bgsp()->GetMtrlSndId(i_this->mAcch.m_gnd);
            } else {
                mtrlSndId = 0;
            }
            i_this->mpMorf->play(&i_this->eyePos, mtrlSndId, dComIfGp_getReverb(roomNo));
        } else {
            i_this->mpMorf->play(NULL, 0, 0);
        }
    }
#endif

    draw_SUB(i_this);

    if (DEMO_SELECT(i_this->m346 != 0, i_this->m346 == 1)) {
        kantera_calc(i_this);
#if VERSION > VERSION_DEMO
        if (i_this->m378[5] != 0) {
            return TRUE;
        }
#endif
        kantera_atari_check(i_this);
        fopAc_ac_c* actor;
#if VERSION == VERSION_DEMO
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e && (actor = fopAcM_SearchByID(i_this->mKanteraID)) != NULL)
#else
        if (i_this->mKanteraID != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(i_this->mKanteraID, &actor) &&
            actor != NULL && fopAcM_GetName(actor) == fpcNm_KANTERA_e)
#endif
        {
            i_this->mSph.SetC(i_this->m2D8);
            i_this->mSph.SetR(40.0f);
            dComIfG_Ccsp()->Set(&i_this->mSph);
        }
    }

    return TRUE;
}

/* 000061FC-00006204       .text daPW_IsDelete__FP8pw_class */
static BOOL daPW_IsDelete(pw_class*) {
    return TRUE;
}

/* 00006204-000062B0       .text daPW_Delete__FP8pw_class */
static BOOL daPW_Delete(pw_class* i_this) {
    dComIfG_resDeleteDemo(&i_this->mPhase, "PW");
#if VERSION > VERSION_DEMO
    if (i_this->heap != NULL) {
        i_this->mpMorf->stopZelAnime();
    }
#endif
    if (i_this->m343 != 0) {
        i_this->m343 = 0;
        TORITUKI_ON = false;
    }
#if VERSION == VERSION_DEMO
    if (i_this->m5C0_demo) {
        i_this->m5C0_demo->becomeInvalidEmitter();
        i_this->m5C4.remove();
        i_this->m5C0_demo = NULL;
    }
#else
    i_this->m5C4.remove();
#endif
    enemy_fire_remove(&i_this->mEnemyFire);
    if (i_this->mMode == 0x51) {
        daPy_getPlayerActorClass()->offConfuse();
    }
    return TRUE;
}

/* 000062B0-000066D8       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* i_this) {
    pw_class* a_this = (pw_class*)i_this;

    a_this->mpMorf = new mDoExt_McaMorf(
        (J3DModelData*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BDL_PW_e),
        NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BCK_WAIT1_e),
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
    model = a_this->mpMorf->getModel();

    a_this->m2BC = new mDoExt_btpAnm();
    if (a_this->m2BC == NULL) {
        return FALSE;
    }
    if (a_this->m2BC->init(
            a_this->mpMorf->getModel()->getModelData(),
            (J3DAnmTexPattern*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BTP_IROGAE1_e),
            TRUE, J3DFrameCtrl::EMode_RESET, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    a_this->m2C0 = new mDoExt_brkAnm();
    if (a_this->m2C0 == NULL) {
        return FALSE;
    }
    if (a_this->m2C0->init(
            model->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_JITTAIKA1_e),
            TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    a_this->m2C4 = new mDoExt_brkAnm();
    if (a_this->m2C4 == NULL) {
        return FALSE;
    }
    if (a_this->m2C4->init(
            model->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_HIT_e),
            TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    a_this->m2C8 = new mDoExt_brkAnm();
    if (a_this->m2C8 == NULL) {
        return FALSE;
    }
    if (a_this->m2C8->init(
            model->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("PW", dRes_INDEX_PW_BRK_DEFAULT_e),
            TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    return a_this->mInvisibleModel.create(a_this->mpMorf->getModel()) != 0 ? TRUE : FALSE;
}

/* 00006720-00006BDC       .text daPW_Create__FP10fopAc_ac_c */
static cPhs_State daPW_Create(fopAc_ac_c* i_actor) {
    /* Nonmatching */
    static dCcD_SrcCyl body_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_NoTgHitInfSet_e,
            /* SrcObjTg  Type    */ AT_TYPE_LIGHT_ARROW | AT_TYPE_LIGHT,
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
    static dCcD_SrcSph kantera_co_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
            /* SrcObjAt  Atp     */ 2,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_GrpAll_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_VsGrpAll_e,
            /* SrcGObjAt Se      */ dCcG_SE_METAL,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ dCcg_TgHitMark_Purple_e,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
        }},
    };
#if VERSION > VERSION_DEMO
    fopAcM_ct(i_actor, pw_class);
#endif
    pw_class* i_this = (pw_class*)i_actor;

    static u8 fire_j[] = {
        PW_JNT_PW_SKLROOT_e,
        PW_JNT_J_PW_KOSI1_e,
        PW_JNT_J_PW_ASI_L1_e,
        PW_JNT_J_PW_ASI_R1_e,
        PW_JNT_J_PW_MUNE1_e,
        PW_JNT_J_PW_MUNE2_e,
        PW_JNT_J_PW_ATAMA1_e,
        PW_JNT_J_PW_KAMEN2_e,
        PW_JNT_J_PW_UDE_L2_e,
        PW_JNT_J_PW_UDE_R2_e,
    };
    static f32 fire_sc[] = {
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    cPhs_State phase_state = dComIfG_resLoad(&i_this->mPhase, "PW");
    if (phase_state == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_ct(i_actor, pw_class);
#endif
        i_this->mBehaviorType = fopAcM_GetParam(i_this);
        i_this->mHoversAtInitialYPos = (fopAcM_GetParam(i_this) >> 8) & 1;
        i_this->mNoticeRangeParam = fopAcM_GetParam(i_this) >> 16;
        i_this->mColorIndex = (fopAcM_GetParam(i_this) >> 9) & 0x7F;
        i_this->mPathIndex = fopAcM_GetParam(i_this) >> 24;
        if (i_this->mBehaviorType == 0xFF) {
            i_this->mBehaviorType = VisibleFromStart;
        }
        if (i_this->mNoticeRangeParam == 0xFF) {
            i_this->mNoticeRange = 1000.0f;
        } else {
            i_this->mNoticeRange = 10.0f * i_this->mNoticeRangeParam;
        }
        if (i_this->mColorIndex == 0x7F) {
            i_this->mColorIndex = 0;
        }
        if (!fopAcM_entrySolidHeap(i_this, useHeapInit, 0x2540)) {
            return cPhs_ERROR_e;
        }

        i_this->gbaName = 0xD;
        i_this->m3AC = 5000.0f + i_this->mNoticeRange;
#if VERSION > VERSION_DEMO
        if (REG8_S(9) != 0) {
            i_this->mBehaviorType = REG8_S(9) - 1;
        }
#endif
        if (i_this->mPathIndex != 0xFF) {
            i_this->mpPath = dPath_GetRoomPath(i_this->mPathIndex, fopAcM_GetRoomNo(i_this));
        }
#if VERSION > VERSION_DEMO
        if (i_this->mColorIndex > 5) {
            i_this->mColorIndex = 0;
        }
#endif

        i_this->m2CC = i_this->current.pos;
        i_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("pow", 0);
        i_this->max_health = 4;
        i_this->health = 4;
        i_this->m398 = 4;
        i_this->m2F0 = i_this->current.pos;
        i_this->m39E = 0x1000;
        fopAcM_SetMtx(i_this, i_this->mpMorf->getModel()->getBaseTRMtx());
        fopAcM_setCullSizeBox(i_this, -100.0f, -50.0f, -50.0f, 100.0f, 200.0f, 100.0f);
        i_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
        i_this->mAcch.Set(
            fopAcM_GetPosition_p(i_this),
            fopAcM_GetOldPosition_p(i_this),
            i_this,
            1,
            &i_this->mAcchCir,
            fopAcM_GetSpeed_p(i_this)
        );
        i_this->mStts.Init(0, 1, i_this);

        i_this->mEnemyIce.mpActor = i_this;
        i_this->mEnemyIce.mWallRadius = 50.0f;
        i_this->mEnemyIce.mCylHeight = 200.0f;
        i_this->mEnemyFire.mpMcaMorf = i_this->mpMorf;
        i_this->mEnemyFire.mpActor = i_this;
        for (int i = 0; i < (int)ARRAY_SIZE(i_this->mEnemyFire.mFlameJntIdxs); i++) {
            i_this->mEnemyFire.mFlameJntIdxs[i] = fire_j[i];
            i_this->mEnemyFire.mParticleScale[i] = fire_sc[i];
        }

        i_this->mCyl.Set(body_co_cyl_src);
        i_this->mCyl.SetStts(&i_this->mStts);
        i_this->mSph.Set(kantera_co_sph_src);
        i_this->mSph.SetStts(&i_this->mStts);
        i_this->mSph.OffAtSetBit();
        i_this->mSph.ClrAtSet();
        i_this->m38C = i_this->current.angle.y;
        i_this->stealItemLeft = 1;
        i_this->mAction = 0;
        i_this->mMode = 0xA;
        i_this->mJalhallaID = fpcM_ERROR_PROCESS_ID_e;
#if VERSION > VERSION_DEMO
        i_this->mKanteraID = fpcM_ERROR_PROCESS_ID_e;
#endif
        i_this->mCyl.OffTgNoConHit();
#if VERSION > VERSION_DEMO
        i_this->m2D8 = i_this->current.pos;
        i_this->m346 = -1;
#endif

        switch (i_this->mBehaviorType) {
        case InvisibleAtStart:
            i_this->attention_info.flags = 0;
            fopAcM_OffStatus(i_this, fopAcStts_SHOWMAP_e);
            i_this->mMode = 0;
            break;
        case OnlyLanternVisibleAtStart:
            i_this->attention_info.flags = 0;
            fopAcM_OffStatus(i_this, fopAcStts_SHOWMAP_e);
            i_this->mMode = 9;
            break;
        case JalhallaChildA:
        case JalhallaChildB:
            i_this->mJalhallaID = i_this->parentActorID;
            if (i_this->mJalhallaID == fpcM_ERROR_PROCESS_ID_e) {
                return cPhs_ERROR_e;
            }
            i_this->mCyl.OnTgNoConHit();
            i_this->mCyl.SetTgType(AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT);
            i_this->m39A = 0xFF;
            i_this->m38E = 0;
            i_this->max_health = 4;
            i_this->health = 4;
#if VERSION > VERSION_DEMO
            i_this->m346 = 0;
#endif
            if (i_this->mBehaviorType == JalhallaChildA) {
                i_this->m33E = 1;
                i_this->mAction = 0;
                i_this->mMode = 0x6E;
            } else {
                fopAcM_OnStatus(i_this, fopAcStts_UNK4000_e);
                i_this->m390 = cM_rndFX(16384.0f);
                i_this->current.pos.x += cM_rndFX(150.0f);
                i_this->current.pos.z += cM_rndFX(150.0f);
                i_this->mAction = 5;
                i_this->mMode = 0x96;
            }
            break;
        }

        BG_check(i_this);
        draw_SUB(i_this);
    }

    return phase_state;
}

static actor_method_class l_daPW_Method = {
    (process_method_func)daPW_Create,
    (process_method_func)daPW_Delete,
    (process_method_func)daPW_Execute,
    (process_method_func)daPW_IsDelete,
    (process_method_func)daPW_Draw,
};

actor_process_profile_definition g_profile_PW = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_PW_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(pw_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_PW_e,
    /* Actor SubMtd */ &l_daPW_Method,
    /* Status       */ fopAcStts_SHOWMAP_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e | fopAcStts_UNK80000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
