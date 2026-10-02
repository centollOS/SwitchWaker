/**
 * d_a_gm.cpp
 * Mini-Boss - Mothula / Enemy - Mothula Larva
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_gm.h"
#include "d/actor/d_a_player.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_material.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "d/d_particle_name.h"
#include "d/d_camera.h"
#include "f_op/f_op_camera.h"
#include "d/d_bg_s_lin_chk.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "c/c_damagereaction.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/Gm.h"

/* 000000EC-0000032C       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        gm_class* i_this = (gm_class*)model->getUserArea();
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        if (i_this != NULL && i_this->m2CD == 0) {
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
            cXyz offset;

            switch (jntNo) {
            case GM_JNT_HARAC_e:
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mHaraPos);
                break;
            case GM_JNT_MUNE_e:
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mMunePos);
                cMtx_YrotM(*calc_mtx, i_this->mMuneRot.y);
                cMtx_XrotM(*calc_mtx, i_this->mMuneRot.x);
                cMtx_ZrotM(*calc_mtx, i_this->mMuneRot.z);
                break;
            case GM_JNT_ATAMA_e:
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = 0.0f;
                MtxPosition(&offset, &i_this->mAtamaPos);
                offset.x = 70.0f;
                MtxPosition(&offset, &i_this->mAtamaOffsetPos);
                break;
            case GM_JNT_HANESLA_e:
            case GM_JNT_HANESRA_e:
            case GM_JNT_HANEULA_e:
            case GM_JNT_HANEURA_e: {
                s32 wingNo = (jntNo - GM_JNT_HANESLA_e) >> 1;
                cMtx_YrotM(*calc_mtx, i_this->mWingAngle[wingNo].y);
                cMtx_XrotM(*calc_mtx, i_this->mWingAngle[wingNo].x);
                cMtx_ZrotM(*calc_mtx, i_this->mWingAngle[wingNo].z);
                break;
            }
            }

            if (jntNo != GM_JNT_HANESLA_e && jntNo != GM_JNT_HANESRA_e &&
                jntNo != GM_JNT_HANEULA_e && jntNo != GM_JNT_HANEURA_e)
            {
                cMtx_YrotM(*calc_mtx, i_this->mExtraJointRot.y);
                cMtx_XrotM(*calc_mtx, i_this->mExtraJointRot.x);
                cMtx_ZrotM(*calc_mtx, i_this->mExtraJointRot.z);
            }
#if VERSION == VERSION_DEMO

            model->setAnmMtx(jntNo, *calc_mtx);
#else

            MTXCopy(*calc_mtx, model->getAnmMtx(jntNo));
#endif
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 00000368-0000048C       .text draw_SUB__FP8gm_class */
void draw_SUB(gm_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    model->setBaseScale(i_this->scale);
    mDoMtx_stack_c::transS(
        i_this->current.pos.x + i_this->mDrawOffset.x,
        i_this->current.pos.y + i_this->mDrawOffset.y,
        i_this->current.pos.z + i_this->mDrawOffset.z
    );
    mDoMtx_stack_c::XrotM(i_this->m43E.x);
    mDoMtx_stack_c::ZrotM(i_this->m43E.z);
    mDoMtx_stack_c::YrotM(i_this->shape_angle.y);
    mDoMtx_stack_c::XrotM(i_this->shape_angle.x);
    mDoMtx_stack_c::YrotM(i_this->m320);
    mDoMtx_stack_c::ZrotM(i_this->shape_angle.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());

    if (i_this->m2CD == 0) {
        i_this->mpMorf->calc();
        enemy_fire(&i_this->mEnemyFire);
    }

    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &i_this->current.pos, &i_this->tevStr);
}

/* 0000048C-00000908       .text daGM_Draw__FP8gm_class */
static BOOL daGM_Draw(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    J3DModel* model = i_this->mpMorf->getModel();
    g_env_light.setLightTevColorType(model, &actor->tevStr);
    dSnap_RegistFig(DSNAP_TYPE_UNKBE, actor, actor->attention_info.position, actor->shape_angle.y, 1.0f, 1.0f, 1.0f);

    if (i_this->m2CD == 0 && i_this->mEnemyIce.mFreezeTimer > 20) {
        dMat_control_c::iceEntryDL(i_this->mpMorf, -1, &i_this->mInvisibleModel);
        return TRUE;
    }

    if (i_this->m2CD == 0) {
        int shapeIdx = 0;
        for (u16 i = 0; i < model->getModelData()->getJointNum(); i++) {
            J3DMaterial* mat = model->getModelData()->getJointNodePointer(i)->getMesh();
            while (mat != NULL) {
                J3DShape* shape = mat->getShape();
                switch (shapeIdx) {
                case 2:
                    if ((i_this->mWingBits & 0x08) == 0) {
                        shape->show();
                    } else {
                        shape->hide();
                    }
                    break;
                case 3:
                    if ((i_this->mWingBits & 0x02) == 0) {
                        shape->show();
                    } else {
                        shape->hide();
                    }
                    break;
                case 4:
                    if ((i_this->mWingBits & 0x01) == 0) {
                        shape->show();
                    } else {
                        shape->hide();
                    }
                    break;
                case 5:
                    if ((i_this->mWingBits & 0x04) == 0) {
                        shape->show();
                    } else {
                        shape->hide();
                    }
                    break;
                default:
                    shape->show();
                    break;
                }
                mat = mat->getNext();
                shapeIdx++;
            }
        }

        switch (i_this->mBrkMode) {
        case 1:
            i_this->mpBrkDamage->entry(model->getModelData());
            break;
        case 2:
            i_this->mpBrkAtack->entry(model->getModelData());
            break;
        case 3:
            i_this->mpBrkDead->entry(model->getModelData());
            break;
        default:
            i_this->mpBrkGm->entry(model->getModelData());
            break;
        }

        i_this->mpMorf->entryDL();

        switch (i_this->mBrkMode) {
        case 1:
            i_this->mpBrkDamage->remove(model->getModelData());
            break;
        case 2:
            i_this->mpBrkAtack->remove(model->getModelData());
            break;
        case 3:
            i_this->mpBrkDead->remove(model->getModelData());
            break;
        default:
            i_this->mpBrkGm->remove(model->getModelData());
            break;
        }
    } else {
        i_this->mpMorf->updateDL();
    }

    J3DModelData* modelData = model->getModelData();
    J3DModel* shadowModel = i_this->mpShadowModel;
    int i;
    for (i = 0; i < modelData->getJointNum(); i++) {
#if VERSION == VERSION_DEMO
        shadowModel->setAnmMtx(i, model->getAnmMtx(i));
#else
        MTXCopy(model->getAnmMtx(i), shadowModel->getAnmMtx(i));
#endif
    }
    for (i = 0; i < modelData->getWEvlpMtxNum(); i++) {
#if VERSION == VERSION_DEMO
        shadowModel->setWeightAnmMtx(i, model->getWeightAnmMtx(i));
#else
        MTXCopy(model->getWeightAnmMtx(i), shadowModel->getWeightAnmMtx(i));
#endif
    }

    f32 posZ = actor->current.pos.z + i_this->mDrawOffset.z;
    cXyz shadowPos(
        actor->current.pos.x + i_this->mDrawOffset.x,
        REG8_F(18) + (10.0f + (actor->current.pos.y + i_this->mDrawOffset.y)),
        posZ
    );
    i_this->mShadowId = dComIfGd_setRealShadow(
        i_this->mShadowId,
        1,
        i_this->mpShadowModel,
        &shadowPos,
        900.0f + REG8_F(19),
        (actor->current.pos.y + i_this->mDrawOffset.y) - i_this->mAcch.GetGroundH(),
        NULL
    );

    if (REG8_S(1) == 0) {
        return TRUE;
    }

    if (i_this->m360[0].x || i_this->m360[0].z) {
        if (i_this->m360[1].x || i_this->m360[1].z) {
            int j;
            for (j = 0; j < 3; j++) {
                cXyz tmp = actor->current.pos + i_this->mDrawOffset;
            }
        }
    }

    return TRUE;
}

/* 00000908-00000A34       .text anm_init__FP8gm_classifUcfi */
void anm_init(gm_class* i_this, int bckFileIdx, float morf, unsigned char loopMode, float speed, int soundFileIdx) {
    i_this->mBckIdx = bckFileIdx;
    if (soundFileIdx >= 0) {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("GM", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, dComIfG_getObjectRes("GM", soundFileIdx));
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("GM", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
}

/* 00000A34-00000D00       .text wing_cut_stat__FP8gm_class */
BOOL wing_cut_stat(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    static cXyz wing_birth_pos_dt[] = {
        cXyz(94.0f, -72.0f, -30.0f),
        cXyz(-94.0f, -72.0f, -30.0f),
        cXyz(144.0f, 72.0f, -30.0f),
        cXyz(-144.0f, 72.0f, -30.0f),
    };

    u32 parameters;
    int i = 0;
    int bit = 1;
    while (i < 4) {
        if ((i_this->mWingBits & bit) == 0) {
            parameters = (i + 1) | 0x100;
            if (i_this->m2D0 == 4) {
                parameters = (i + 1) | 0x200;
            }

            cMtx_YrotS(*calc_mtx, (int)actor->shape_angle.y);

            cXyz offset = wing_birth_pos_dt[i];
            cXyz pos;
            MtxPosition(&offset, &pos);
            pos += actor->current.pos + i_this->mDrawOffset;

            csXyz angle;
            angle.x = actor->shape_angle.x;
            angle.y = actor->shape_angle.y;
            angle.z = actor->shape_angle.z;
            angle.y += (s16)0x8000;

            fopAcM_create(
                fpcNm_GM_e,
                parameters,
                &pos,
                fopAcM_GetRoomNo(actor),
                &angle,
                &actor->scale,
                0,
                NULL
            );
            dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &pos, &player->shape_angle);
            i_this->mWingBits |= bit;

            if (actor->health > 0) {
                if (i_this->m2D1 != 10) {
                    return TRUE;
                }
            }
        }

        bit <<= 1;
        i++;
    }

    if (actor->health <= 0 || i_this->m2D1 == 10) {
        return TRUE;
    }
    return FALSE;
}

/* 00000D3C-0000170C       .text body_atari_check__FP8gm_class */
BOOL body_atari_check(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    i_this->mStts.Move();
    if (i_this->m2E8 != 0) {
        return FALSE;
    }
    if (i_this->mAction == 90 || i_this->mAction == 91) {
        return FALSE;
    }
    if (!i_this->mBodyCyl.ChkTgHit()) {
        i_this->m2D2 = 0;
        return FALSE;
    }
    if (i_this->m2D2 != 0) {
        return FALSE;
    }

    i_this->m2D1 = 0;
    cCcD_Obj* hitObj = i_this->mBodyCyl.GetTgHitObj();
    cXyz hitPos = *i_this->mBodyCyl.GetTgHitPosP();
    i_this->m2D2 = 1;
    if (hitObj == NULL) {
        return FALSE;
    }

    i_this->m2E8 = 8;
    CcAtInfo atInfo;
    atInfo.pParticlePos = NULL;
    u8 skipAtCheck = 0;

    switch (hitObj->GetAtType()) {
    case AT_TYPE_GRAPPLING_HOOK:
        if (actor->stealItemLeft > 0) {
            s8 oldHealth = actor->health;
            actor->health = 10;
            atInfo.mpObj = i_this->mBodyCyl.GetTgHitObj();
            cc_at_check(actor, &atInfo);
            i_this->m2D7++;
            actor->health = oldHealth;
        }
        dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &actor->attention_info.position);
        skipAtCheck = 1;
        i_this->m2D1 = 3;
        break;
    case AT_TYPE_SWORD: {
        fopAcM_seStart(actor, JA_SE_LK_SW_CRT_HIT, 0x20);
        i_this->m2D1 = 0;
        if (player->getCutType() == daPy_py_c::CUT_TYPE_CUT_EA ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_EB ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_TURN ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_ROLL ||
            player->getCutType() == daPy_py_c::CUT_TYPE_JUMPCUT_SWORD ||
            player->getCutType() == daPy_py_c::CUT_TYPE_JUMPCUT_STICK ||
            player->getCutType() == daPy_py_c::CUT_TYPE_JUMPCUT_MACHETE ||
            player->getCutType() == daPy_py_c::CUT_TYPE_BT_JUMPCUT ||
            player->getCutType() == daPy_py_c::CUT_TYPE_BT_ROLLCUT ||
            player->getCutType() == daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT ||
            player->getCutType() == daPy_py_c::CUT_TYPE_JUMPCUT_CLUB ||
            player->getCutType() == daPy_py_c::CUT_TYPE_JUMPCUT_DN_SWORD ||
            player->getCutType() == daPy_py_c::CUT_TYPE_JUMPCUT_SPEAR ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_EXA ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_EXB ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_EXMJ ||
            player->getCutType() == daPy_py_c::CUT_TYPE_CUT_KESA)
        {
            i_this->m2D1 = 1;
        }
        break;
    }
    case AT_TYPE_WIND:
        skipAtCheck = 1;
        i_this->m2D1 = 3;
        if (i_this->m2D0 != 2 && (i_this->mWingBits & 0xF) != 0xF) {
            if (i_this->mAction == 34) {
                i_this->m2D1 = 9;
            }
            i_this->mWallR = 250.0f;
            i_this->mAction = 20;
            i_this->m2D0 = 2;
        }
        break;
    case AT_TYPE_BOOMERANG:
        skipAtCheck = 1;
        i_this->m2D1 = 4;
        dComIfGp_particle_set(dPa_name::ID_IT_JN_PIYOHIT00, &actor->attention_info.position);
        // fallthrough
    case AT_TYPE_BOKO_STICK:
        fopAcM_seStart(actor, JA_SE_LK_W_WEP_CRT_HIT, 0x20);
        break;
    case AT_TYPE_SKULL_HAMMER:
        fopAcM_seStart(actor, JA_SE_LK_HAMMER_HIT, 0x20);
        i_this->m2D1 = 7;
        if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
            i_this->m2D1 = 8;
        }
        break;
    case AT_TYPE_BOMB:
        i_this->m2D1 = 6;
        break;
    case AT_TYPE_HOOKSHOT:
        skipAtCheck = 1;
        i_this->m2D1 = 10;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_CRT_HIT, 0x20);
        break;
    case AT_TYPE_FIRE:
    case AT_TYPE_FIRE_ARROW:
        i_this->mEnemyFire.mFireDuration = 100;
        actor->health = 0;
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_DIE, 0);
        i_this->m2D1 = 5;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_CRT_HIT, 0x20);
        break;
    case AT_TYPE_LIGHT_ARROW:
        i_this->mEnemyIce.mLightShrinkTimer = 1;
        i_this->mEnemyIce.mParticleScale = 1.0f;
        i_this->mEnemyIce.mYOffset = 80.0f;
        actor->health = 0;
        actor->attention_info.flags = 0;
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_DIE, 0);
        i_this->m2D1 = 5;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_CRT_HIT, 0x20);
        break;
    case AT_TYPE_ICE_ARROW:
        i_this->mEnemyIce.mFreezeDuration = 200;
        enemy_fire_remove(&i_this->mEnemyFire);
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_DIE, 0);
        actor->health = 0;
        skipAtCheck = 1;
        // fallthrough
    case AT_TYPE_NORMAL_ARROW:
        i_this->m2D1 = 5;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_CRT_HIT, 0x20);
        break;
    default:
        i_this->m2D1 = 0;
        fopAcM_seStart(actor, JA_SE_LK_MS_WEP_CRT_HIT, 0x20);
        break;
    }

    if (skipAtCheck == 0) {
        atInfo.mpObj = i_this->mBodyCyl.GetTgHitObj();
        cc_at_check(actor, &atInfo);
        if (i_this->m2D1 == 1 || i_this->m2D1 == 7 || i_this->m2D1 == 8 || actor->health <= 0) {
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hitPos);
            cXyz scale;
            scale.setall(2.0f);
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hitPos, &player->shape_angle, &scale);
        } else {
            dComIfGp_particle_set(dPa_name::ID_AK_JN_OK, &hitPos, &player->shape_angle);
        }
    }

    u32 atType = hitObj->GetAtType();
    if (atType == AT_TYPE_LIGHT_ARROW || atType == AT_TYPE_ICE_ARROW) {
        return TRUE;
    }
    if (i_this->m2D1 != 3 && i_this->m2D1 != 9) {
        if ((i_this->mWingBits & 0xF) != 0xF) {
            if (i_this->m2CD == 0) {
                i_this->mWallR = 250.0f;
                if (wing_cut_stat(i_this)) {
                    if ((i_this->mWingBits & 0xF) != 0xF) {
                        fopAcM_monsSeStart(actor, JA_SE_CV_GM_DAMAGE, 0);
                        i_this->setBtNowFrame(1000.0f);
                        if (i_this->m2D0 != 4) {
                            i_this->mAction = 50;
                        } else {
                            i_this->mAction = 52;
                        }
                        i_this->m2D0 = 5;
                    } else if (i_this->m2D0 != 10) {
                        fopAcM_monsSeStart(actor, JA_SE_CV_GM_LOST_WING, 0);
                        i_this->setBtNowFrame(1000.0f);
                        if (i_this->m2D0 != 4) {
                            i_this->mAction = 60;
                        } else {
                            i_this->mAction = 61;
                        }
                        i_this->mWallH = 60.0f;
                        i_this->mWallR = 150.0f;
                        i_this->m2E6 = 600.0f + cM_rndF(600.0f);
                        i_this->m2D0 = 10;
                    }
                }
            }
        }
    }
    return TRUE;
}

/* 0000170C-000017D8       .text BG_check__FP8gm_class */
void BG_check(gm_class* i_this) {
    i_this->mAcchCir.SetWall(i_this->mWallH, i_this->mWallR);

    i_this->current.pos += i_this->mDrawOffset;
    i_this->old.pos.y += i_this->mDrawOffset.y;
    i_this->current.pos.y -= i_this->mCorrectionY;
    i_this->old.pos.y -= i_this->mCorrectionY;
    i_this->mAcch.CrrPos(*dComIfG_Bgsp());
    i_this->current.pos.y += i_this->mCorrectionY;
    i_this->old.pos.y += i_this->mCorrectionY;
    i_this->current.pos -= i_this->mDrawOffset;
    i_this->old.pos.y -= i_this->mDrawOffset.y;
}

/* 000017D8-00001B84       .text Line_check__FP8gm_class4cXyz */
BOOL Line_check(gm_class* i_this, cXyz destPos) {
    static GXColor all_line_color_dt[] = {
        {0xFF, 0x00, 0x00, 0xFF},
        {0x00, 0xFF, 0x00, 0xFF},
        {0x00, 0x00, 0xFF, 0xFF},
        {0xFF, 0x00, 0xFF, 0xFF},
    };
    fopAc_ac_c* actor = i_this;
    dBgS_LinChk linChk;

    cMtx_XrotS(*calc_mtx, i_this->m43E.x);
    cMtx_ZrotM(*calc_mtx, i_this->m43E.z);
    cXyz offset;
    offset.x = 0.0f;
    offset.y = 100.0f;
    offset.z = 0.0f;
    cXyz pos;
    MtxPosition(&offset, &pos);
    pos += i_this->mAtamaPos;
    pos += i_this->mDrawOffset;
    pos.y += 100.0f;
    destPos.y += 100.0f;
    linChk.Set(&pos, &destPos, actor);
    if (!dComIfG_Bgsp()->LineCross(&linChk)) {
        i_this->mLineStart = pos;
        i_this->mLineEnd = destPos;
        return FALSE;
    }
    return TRUE;
}

/* 00001FBC-0000212C       .text ks_set_rtn__FP8gm_class */
BOOL ks_set_rtn(gm_class* i_this) {
    u32 parameters = 4;
    if (i_this->m2CE == 1) {
        if (i_this->m31E >= 10) {
            return TRUE;
        }
    } else {
        if (i_this->m31E >= 20) {
            return TRUE;
        }
    }

    if (i_this->mAction == 83) {
        parameters = 5;
    }

    fopAcM_seStart(i_this, JA_SE_CM_GM_KUROBO_OUT, 0);

    csXyz angle = i_this->current.angle;
    angle.y = angle.y + cM_rndFX(4096.0f);

    fpc_ProcID id = fopAcM_createChild(
        fpcNm_KS_e,
        fopAcM_GetID(i_this),
        parameters,
        &i_this->mHaraPos,
        fopAcM_GetRoomNo(i_this),
        &angle,
        &i_this->scale,
        0,
        NULL
    );
    if (id != fpcM_ERROR_PROCESS_ID_e) {
        i_this->m31E++;
        return FALSE;
    }
    return TRUE;
}

/* 0000212C-00002204       .text wing_ret_set__FP8gm_class */
void wing_ret_set(gm_class* i_this) {
    s16 target = 0;
    for (int i = 0; i < 4; i++) {
        cLib_addCalcAngleS2(&i_this->mWingAngle[i].y, target, 1, 0x200);
        cLib_addCalcAngleS2(&i_this->mWingAngle[i].x, target, 1, 0x200);
        cLib_addCalcAngleS2(&i_this->mWingAngle[i].z, target, 1, 0x200);
    }
    cLib_addCalcAngleS2(&i_this->mExtraJointRot.y, target, 1, 0x200);
    cLib_addCalcAngleS2(&i_this->mExtraJointRot.x, target, 1, 0x200);
    cLib_addCalcAngleS2(&i_this->mExtraJointRot.z, target, 1, 0x200);
    cLib_addCalcAngleS2(&i_this->mMuneRot.y, target, 1, 0x200);
}

/* 00002204-000022C4       .text fuwafuwa_set__FP8gm_class */
void fuwafuwa_set(gm_class* i_this) {
    if (i_this->current.pos.y < 150.0f + dComIfGp_getPlayer(0)->current.pos.y) {
        return;
    }

    i_this->mTimers[0] += 1000;
    i_this->mFuwafuwa.y = 100.0f * cM_ssin(i_this->mTimers[0]);
    if (i_this->mAction == 3 || i_this->mAction == 4) {
        i_this->mFuwafuwa.x = 0.0f;
        i_this->mFuwafuwa.y = 0.0f;
        i_this->mFuwafuwa.z = 0.0f;
    }
    cLib_addCalc2(&i_this->mDrawOffset.y, i_this->mFuwafuwa.y, 1.0f, 10.0f);
}

/* 000022C4-00002474       .text fly_move__FP8gm_class */
BOOL fly_move(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    s16 ang = 0x2000;

    if (i_this->m2E2 == 0) {
        if (cM_rnd() < 0.5f) {
            ang = -0x2000;
        }

        s16 camY = dCam_getAngleY((camera_class*)dComIfGp_getCamera(0));
        for (int i = 0; i < 2; i++) {
            cMtx_YrotS(*calc_mtx, camY + ang);
            cXyz offset;
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 300.0f;
            cXyz pos;
            MtxPosition(&offset, &pos);
            pos += player->current.pos;
            pos.y = actor->current.pos.y + i_this->mDrawOffset.y;

            if (!Line_check(i_this, pos)) {
                f32 dx = pos.x - (actor->current.pos.x + i_this->mDrawOffset.x);
                f32 dz = pos.z - (actor->current.pos.z + i_this->mDrawOffset.z);
                actor->speedF = 10.0f;
                i_this->m2E2 = 20;
                i_this->m2E2 += (s16)cM_rndF(20.0f);
                i_this->m334 = 0.0f;
                i_this->m434 = cM_atan2s(dx, dz);
                return TRUE;
            }

            ang += 0x8000;
        }
    }

    return FALSE;
}

/* 00002474-00002AB8       .text action_dousa__FP8gm_class */
void action_dousa(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 dx = player->current.pos.x - (actor->current.pos.x + i_this->mDrawOffset.x);
    f32 dz = player->current.pos.z - (actor->current.pos.z + i_this->mDrawOffset.z);

    switch (i_this->mAction) {
    case 0: {
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        for (int i = 0; i < 3; i++) {
            i_this->m360[i].x = 0.0f;
            i_this->m360[i].y = 0.0f;
            i_this->m360[i].z = 0.0f;
        }
        i_this->mpBrkGm->setFrame(0.0f);
        i_this->mBrkMode = 0;
        i_this->mWallH = 0.0f;
        i_this->m338 = -120.0f;
        i_this->m33C = 220.0f;
        i_this->m340 = 60.0f;
        i_this->mWallR = 250.0f;
        i_this->m2E4 = 30.0f + cM_rndF(30.0f);
        i_this->m2E2 = 0;
        if (i_this->mBckIdx != dRes_INDEX_GM_BCK_FLY_e) {
            anm_init(i_this, dRes_INDEX_GM_BCK_FLY_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        i_this->mAction++;
        /* fallthrough */
    }
    case 1:
        i_this->m348.y = 250.0f + player->current.pos.y;
        if (fly_move(i_this)) {
            break;
        }
        if (std::fabsf(actor->current.pos.y - i_this->m348.y) > 40.0f) {
            break;
        }
        {
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            if (dist > 300.0f) {
                break;
            }
        }
        if (i_this->m2E4 != 0) {
            break;
        }
        {
            f32 chance = 0.1f;
            s16 bits = i_this->mWingBits;
            for (int i = 0; i < 4; i++) {
                s16 tmp = bits;
                if (tmp & 1) {
                    chance += 0.1f;
                    if (chance > 0.3f) {
                        chance = 0.3f;
                    }
                    bits = tmp >> 1;
                }
            }
            i_this->mAction = 2;
            if (cM_rnd() < chance && (i_this->mWingBits & 7) != 7) {
                actor->speedF = 0.0f;
                i_this->m2D0 = 3;
                i_this->mAction = 30;
            }
        }
        break;
    case 2:
        anm_init(i_this, dRes_INDEX_GM_BCK_ATACK01_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_ATTACK, 0);
        i_this->mpBrkAtack->setFrame(0.0f);
        i_this->mBrkMode = 2;
        actor->speedF = 0.0f;
        i_this->m2E4 = 20.0f + cM_rndF(20.0f);
        i_this->mAction++;
        /* fallthrough */
    case 3:
        i_this->m348.y = 100.0f + player->current.pos.y;
        if (std::fabsf(actor->current.pos.y - i_this->m348.y) > 40.0f) {
            break;
        }
        if (i_this->m2E4 != 0) {
            break;
        }
        i_this->m434 = actor->shape_angle.y;
        actor->current.angle.y = i_this->m434;
        actor->speedF = 50.0f;
        i_this->mWeaponSph.OnAtSetBit();
        i_this->mWeaponSph.OnAtHitBit();
        anm_init(i_this, dRes_INDEX_GM_BCK_ATACK02_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GM_BAS_ATACK02_e);
        i_this->mAction++;
        break;
    case 4:
        if (i_this->mTimers[3] == 0) {
            if (i_this->mWeaponSph.ChkAtShieldHit()) {
                i_this->mWeaponSph.OffAtSetBit();
                i_this->mWeaponSph.OffAtSetBit();
                actor->speedF = -25.0f;
                i_this->mTimers[3] = 1;
            } else {
                if (((daPy_py_c*)player)->getDamageWaitTimer()) {
                    actor->speedF = -50.0f;
                    i_this->mTimers[3] = 1;
                }
            }
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 5.0f);
        if (i_this->mpMorf->isStop()) {
            actor->speedF = 0.0f;
            i_this->m2E4 = 40.0f + cM_rndF(20.0f);
            anm_init(i_this, dRes_INDEX_GM_BCK_ATACK01_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->mWeaponSph.OffAtSetBit();
            i_this->mWeaponSph.OffAtSetBit();
            i_this->mAction = 0;
        }
        break;
    }

    fuwafuwa_set(i_this);
    if ((i_this->mWingBits & 7) == 7) {
        s16 dang = cLib_distanceAngleS(actor->shape_angle.z, 0);
        if (dang < 0x100) {
            i_this->m436 = -0x4000;
        }
    }
    if (i_this->mAction != 4) {
        cLib_addCalcAngleS2(&actor->current.angle.y, i_this->m434, 1, i_this->m334);
        i_this->m334 = 1700.0f;
        cLib_addCalc2(&i_this->m334, 1700.0f, 1.0f, 3.0f);
        int yawTarget = cM_atan2s(dx, dz);
        cLib_addCalcAngleS2(&actor->shape_angle.y, yawTarget, 1, 0x800);
    }
    cLib_addCalc2(&actor->current.pos.y, i_this->m348.y, 1.0f, 3.0f);
    s16 angTarget = 0;
    cLib_addCalcAngleS2(&i_this->m320, angTarget, 1, 0x200);
    cLib_addCalcAngleS2(&actor->shape_angle.x, angTarget, 1, 0x200);
    fopAcM_seStart(actor, JA_SE_CM_GM_FLYING, 0);
}

/* 00002AB8-00002C9C       .text action_hane_rakka__FP8gm_class */
void action_hane_rakka(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;

    switch (i_this->mAction) {
    case 10: {
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        actor->gravity = -1.0f;
        actor->speed.y = 7.0f;
        if (i_this->m2CD != 2) {
            actor->speedF = 40.0f;
            if (i_this->mWingBits == 1 || i_this->mWingBits == 3) {
                actor->current.angle.y += (s16)(16384.0f + cM_rndFX(4096.0f));
                i_this->mTimers[0] = 0x1000;
            } else {
                actor->current.angle.y -= (s16)(16384.0f + cM_rndFX(4096.0f));
                i_this->mTimers[0] = -0x1000;
            }
            i_this->mTimers[1] = 0x300;
        }
        i_this->mAction++;
        /* fallthrough */
    }
    case 11:
        if (i_this->mAcch.ChkGroundHit() || i_this->mSpawnPos.y - 1000.0f > actor->current.pos.y) {
            actor->shape_angle.x = 0;
            i_this->mTimers[0] = 0;
            i_this->mTimers[1] = 0;
            i_this->mAction++;
        }
        break;
    case 12:
        if (actor->scale.x < 0.1f) {
            fopAcM_delete(actor);
        }
        break;
    }

    cLib_addCalc0(&actor->scale.x, 1.0f, 0.02f);
    f32 scale = actor->scale.x;
    actor->scale.y = scale;
    actor->scale.z = scale;
    cLib_addCalc0(&actor->speedF, 1.0f, 5.0f);
    if (actor->speed.y < 0.0f) {
        actor->shape_angle.y += i_this->mTimers[0];
        actor->shape_angle.x += i_this->mTimers[1];
    }
}

static csXyz wing_trans_angle_dt[4] = {
    csXyz(10000, -12000, -2000),
    csXyz(-10000, 12000, -2000),
    csXyz(-2000, -12000, 2000),
    csXyz(2000, 12000, 2000),
};

/* 00002C9C-000030FC       .text action_uchiwa_dousa__FP8gm_class */
void action_uchiwa_dousa(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif

    switch (i_this->mAction) {
    case 20: {
        anm_init(i_this, dRes_INDEX_GM_BCK_PAUSE_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
        fopAcM_seStart(actor, JA_SE_CM_GM_HIT_WIND, 0);
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_HIT_WIND, 0);
        i_this->mWallR = 250.0f;
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        actor->speedF = 60.0f;
        actor->speed.y = 24.0f;
        if (i_this->m2D1 == 3) {
            actor->speedF = 20.0f;
            actor->speed.y = 22.0f;
        }
        i_this->mTimers[3] = 5000;
        i_this->mpMorf->setPlaySpeed(0.0f);
        i_this->mAction++;
        /* fallthrough */
    }
    case 21: {
        cLib_addCalc0(&actor->speedF, 1.0f, 3.0f);
        cLib_addCalc0(&actor->speed.y, 1.0f, 2.0f);
        for (int i = 0; i < 4; i++) {
            i_this->mWingAngle[i].y = wing_trans_angle_dt[i].y;
            i_this->mWingAngle[i].x = wing_trans_angle_dt[i].x;
            i_this->mWingAngle[i].z = wing_trans_angle_dt[i].z;
        }
        i_this->mTimers[0] += 400;
        i_this->mTimers[1] += 400;
        i_this->mTimers[2] += 400;
        i_this->mExtraJointRot.y = 4000.0f * cM_ssin(i_this->mTimers[0]);
        i_this->mExtraJointRot.x = 4000.0f * cM_scos(i_this->mTimers[1]);
        i_this->mExtraJointRot.z = 4000.0f * cM_scos(i_this->mTimers[2]);
        if (i_this->m2D1 != 3) {
            actor->shape_angle.z += i_this->mTimers[3];
            s16 target = 0;
            cLib_addCalcAngleS2(&i_this->mTimers[3], target, 1, 0x100);
        }
        if (actor->speed.y < 0.1f && actor->speedF < 0.1f) {
            actor->speedF = 0.0f;
            i_this->mAction++;
        }
        break;
    }
    case 22:
        actor->gravity = -0.08f;
        i_this->mTimers[0] += 400;
        i_this->mTimers[1] += 400;
        i_this->mTimers[2] += 400;
        i_this->mExtraJointRot.y = 4000.0f * cM_ssin(i_this->mTimers[0]);
        i_this->mExtraJointRot.x = 4000.0f * cM_scos(i_this->mTimers[1]);
        i_this->mExtraJointRot.z = 4000.0f * cM_scos(i_this->mTimers[2]);
        if (actor->current.pos.y < 150.0f + i_this->mAcch.GetGroundH()) {
            actor->speed.y = 0.0f;
            actor->gravity = 0.0f;
            i_this->m2D0 = 0;
            i_this->mAction = 0;
        }
        break;
    }

    s16 muneTarget = 0x2000;
    cLib_addCalcAngleS2(&i_this->mMuneRot.y, muneTarget, 1, 0x1000);
}

/* 000030FC-00003E54       .text action_totugeki__FP8gm_class */
void action_totugeki(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    dBgS_LinChk linChk;
    cXyz offset;
    cXyz pos;
    cXyz base;
    int i;
    base.x = 0.0f;
    base.y = 0.0f;
    base.z = 0.0f;

    switch (i_this->mAction) {
    case 30: {
        for (i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        for (i = 0; i < 3; i++) {
            i_this->m360[i].x = 0.0f;
            i_this->m360[i].y = 0.0f;
            i_this->m360[i].z = 0.0f;
        }
        i_this->mpBrkAtack->setFrame(0.0f);
        i_this->mBrkMode = 2;
        i_this->m322 = 0;
        for (i = 0; i < 3; i++) {
            i_this->m360[i].x = 0.0f;
            i_this->m360[i].y = 0.0f;
            i_this->m360[i].z = 0.0f;
        }
        if (i_this->mBckIdx != dRes_INDEX_GM_BCK_FLY_e) {
            anm_init(i_this, dRes_INDEX_GM_BCK_FLY_e, 1.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        i_this->speedF = 0.0f;
        i_this->current.angle.y = i_this->shape_angle.y;
        i_this->mFuwafuwa.x = 0.0f;
        i_this->mFuwafuwa.y = 0.0f;
        i_this->mFuwafuwa.z = 0.0f;
        i_this->mAction++;
        /* fallthrough */
    }
    case 31: {
        s16 pitchTarget = 0;
        cLib_addCalcAngleS2(&i_this->shape_angle.x, pitchTarget, 1, 0x200);
        s16 yawTarget = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
        cLib_addCalcAngleS2(&i_this->shape_angle.y, yawTarget, 1, 0x1000);
        f32 targetY = 400.0f;
        targetY += player->current.pos.y;
        cLib_addCalc2(&i_this->current.pos.y, targetY, 1.0f, 20.0f);
        i_this->current.angle.y = i_this->shape_angle.y;
        if (std::fabsf(i_this->current.pos.y - targetY) < 2.0f) {
            i_this->mpMorf->setPlaySpeed(2.0f);
            i_this->m2E2 = 45;
            i_this->mAction++;
        }
        fopAcM_seStart(i_this, JA_SE_CM_GM_FLYING, 0);
        break;
    }
    case 32: {
        fopAcM_seStart(i_this, JA_SE_CM_GM_FLYING, 0);
        if (i_this->m2E2 > 15) {
            i_this->current.angle.y = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
        }
        if (fopAcM_searchActorDistance(i_this, dComIfGp_getPlayer(0)) < 500.0f) {
#if VERSION == VERSION_DEMO
            s16 rot = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0)) + 0x8000;
            cMtx_YrotS(*calc_mtx, rot);
#else
            s16 rot = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
            cMtx_YrotS(*calc_mtx, rot + 0x8000);
#endif
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 500.0f;
            MtxPosition(&offset, &pos);
            pos += player->current.pos;
            if (std::fabsf(i_this->current.pos.x - pos.x) > 20.0f) {
                cLib_addCalc2(&i_this->current.pos.x, pos.x, 1.0f, 10.0f);
            }
            if (std::fabsf(i_this->current.pos.z - pos.z) > 20.0f) {
                cLib_addCalc2(&i_this->current.pos.z, pos.z, 1.0f, 10.0f);
            }
        }
        cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 1, 0x1000);
        if (i_this->m2E2 != 0) {
            break;
        }
        if (Line_check(i_this, player->current.pos)) {
            break;
        }
        {
            s16 dang = cLib_distanceAngleS(i_this->shape_angle.y, i_this->current.angle.y);
            if (dang > 0x500) {
                break;
            }
        }
        i_this->shape_angle.y = i_this->current.angle.y;
        i_this->mAction++;
        /* fallthrough */
    }
    case 33:
        i_this->m322++;
        i_this->mpMorf->setPlaySpeed(1.0f);
        anm_init(i_this, dRes_INDEX_GM_BCK_JETATACK_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->mWingCyl.OnAtSetBit();
        i_this->mWingCyl.OnAtHitBit();
        i_this->mBodyCyl.SetCoVsGrp(cCcD_CoSPrm_VsPlayer_e);
        i_this->m320 = 0;
        i_this->shape_angle.x = 0;
        i_this->speedF = 30.0f + REG8_F(10);
        i_this->gravity = 1.0f + REG8_F(11);
        i_this->speed.y = -85.0f + REG8_F(12);
        i_this->m2DA = 0;
        i_this->m348 = player->current.pos;
        i_this->m348.y = 350.0f + player->current.pos.y;
        i_this->mAction++;
        /* fallthrough */
    case 34: {
        fopAcM_seStart(i_this, JA_SE_CM_GM_JET, 0);
        i_this->setBtNowFrame(1000.0f);
        i_this->m2DA++;
        if (i_this->m2DA > (s16)REG8_F(7) && i_this->m2DA < (s16)(62.0f + REG8_F(8))) {
            i_this->setBtNowFrame(5.0f);
        }
        if (i_this->speed.y > 0.0f) {
            if (i_this->current.pos.y + i_this->mDrawOffset.y > i_this->m348.y) {
                i_this->mTimers[3] = 1;
            }
        }
        if (i_this->mTimers[3] != 0) {
            cLib_addCalc0(&i_this->gravity, 1.0f, 5.0f);
            cLib_addCalc0(&i_this->speed.y, 1.0f, 5.0f);
        }
        if (i_this->current.pos.y + i_this->mDrawOffset.y < base.y) {
            i_this->current.pos.y = base.y;
        }
        if (i_this->current.pos.y < 100.0f + i_this->mAcch.GetGroundH()) {
            i_this->current.pos.y = 100.0f + i_this->mAcch.GetGroundH();
        }

        static f32 check_x[] = {-1.0f, 1.0f, 0.0f};
        static f32 check_y[] = {30.0f, 30.0f, 30.0f};
        static f32 check_z[] = {400.0f, 400.0f, 410.0f};
        for (i = 0; i < 3; i++) {
            i_this->m384[i].x = 0.0f;
            i_this->m384[i].y = 0.0f;
            i_this->m384[i].z = 0.0f;
            cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
            offset.x = check_x[i];
            offset.y = check_y[i];
            offset.z = check_z[i];
            MtxPosition(&offset, &i_this->m360[i]);
            i_this->m360[i] += DEMO_SELECT(actor, i_this)->current.pos;
            i_this->m360[i] += i_this->mDrawOffset;
            base = i_this->current.pos + i_this->mDrawOffset;
            linChk.Set(&base, &i_this->m360[i], actor);
            if (dComIfG_Bgsp()->LineCross(&linChk)) {
                i_this->m384[i] = linChk.GetCross();
            }
        }

        if (i_this->mAcch.ChkWallHit()) {
            i_this->setBtNowFrame(1000.0f);
            i_this->mBodyCyl.SetCoVsGrp(cCcD_CoSPrm_VsGrpAll_e);
            i_this->mAction = 37;
            return;
        }

        if (i_this->m384[0].x || i_this->m384[0].z || i_this->m384[1].x || i_this->m384[1].z) {
            if ((i_this->m384[0].x || i_this->m384[0].z) && (i_this->m384[1].x || i_this->m384[1].z)) {
                i_this->speedF = 0.0f;
                i_this->gravity = 0.0f;
                i_this->speed.y = 0.0f;
                i_this->setBtNowFrame(1000.0f);
                i_this->mBodyCyl.SetCoVsGrp(cCcD_CoSPrm_VsGrpAll_e);
                i_this->mAction = 40;
                i_this->m2D0 = 4;
                break;
            }

            offset = i_this->m384[1] - i_this->current.pos;
            if (i_this->m384[0].x || i_this->m384[0].z) {
                offset = i_this->m384[0] - i_this->current.pos;
            }
            i_this->shape_angle.y = cM_atan2s(offset.x, offset.z) + 0x4000;
            cLib_addCalcAngleS2(&i_this->shape_angle.y, i_this->current.angle.y, 1, 0x1000);
        }
        break;
    }
    case 37: {
        for (i = 0; i < 3; i++) {
            i_this->m360[i].x = 0.0f;
            i_this->m360[i].y = 0.0f;
            i_this->m360[i].z = 0.0f;
        }
        i_this->gravity = 0.0f;
        i_this->speed.y = 0.0f;
        i_this->m2EE = 0;
        anm_init(i_this, dRes_INDEX_GM_BCK_UMINIGE_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->speedF = 20.0f;
        i_this->mAction++;
        /* fallthrough */
    }
    case 38: {
        fopAcM_seStart(i_this, JA_SE_CM_GM_FLYING, 0);
        f32 targetY = 500.0f;
        targetY += player->current.pos.y;
        cLib_addCalc2(&i_this->current.pos.y, targetY, 1.0f, 20.0f);
        f32 dx = i_this->mSpawnPos.x - (i_this->current.pos.x + i_this->mDrawOffset.x);
        f32 dz = i_this->mSpawnPos.z - (i_this->current.pos.z + i_this->mDrawOffset.z);
        i_this->current.angle.y = cM_atan2s(dx, dz);
        s16 yawTarget = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
        cLib_addCalcAngleS2(&i_this->shape_angle.y, yawTarget, 1, 0x1000);
        if (i_this->mpMorf->checkFrame(4.0f) || i_this->mpMorf->checkFrame(8.0f) ||
            i_this->mpMorf->checkFrame(12.0f) || i_this->mpMorf->checkFrame(16.0f))
        {
            ks_set_rtn(i_this);
        }
        f32 dist = std::sqrtf(dx * dx + dz * dz);
        if (dist < 40.0f) {
            i_this->mAction = 30;
        }
        break;
    }
    }

    cLib_addCalc0(&i_this->mDrawOffset.x, 1.0f, 5.0f);
    cLib_addCalc0(&i_this->mDrawOffset.y, 1.0f, 5.0f);
    cLib_addCalc0(&i_this->mDrawOffset.z, 1.0f, 5.0f);
}

/* 00003E54-00004204       .text action_kabehari__FP8gm_class */
void action_kabehari(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz offset;
    cXyz pos;
    pos.x = 0.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;

    switch (i_this->mAction) {
    case 40:
        i_this->mWallR = 70.0f;
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        i_this->mWingCyl.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        i_this->m348 = player->current.pos;
        i_this->m348.y = 450.0f + player->current.pos.y;
        i_this->m320 = 0;
        i_this->m2E4 = 50;
        i_this->mAction++;
        /* fallthrough */
    case 41: {
        s16 pitchTarget = -0x4000;
        cLib_addCalcAngleS2(&actor->shape_angle.x, pitchTarget, 1, 0x200);
        f32 dx = actor->current.pos.x - pos.x;
        f32 dy = actor->current.pos.y - i_this->m348.y;
        f32 dz = actor->current.pos.z - pos.z;
        f32 dist = std::sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < 4.0f || i_this->mAcch.ChkWallHit() || i_this->m2E4 == 0) {
            if (i_this->mBckIdx != dRes_INDEX_GM_BCK_TOMARU_e) {
                anm_init(i_this, dRes_INDEX_GM_BCK_TOMARU_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            }
            i_this->mWallR = 10.0f;
            actor->shape_angle.x = -0x4000;
            i_this->mAction = 42;
        }
        break;
    }
    case 42: {
        s16 spinTarget = -0x8000;
        cLib_addCalcAngleS2(&i_this->m320, spinTarget, 1, 0x500);
        s16 dang = cLib_distanceAngleS(i_this->m320, spinTarget);
        if (dang < 0x100) {
            i_this->m2E2 = 10;
            i_this->mAction = 43;
        }
        break;
    }
    case 43: {
        s16 spinTarget = -0x8000;
        cLib_addCalcAngleS2(&i_this->m320, spinTarget, 1, 0x500);
        if (i_this->m2E2 == 0) {
            if (i_this->m322 <= 1 && (i_this->mWingBits & 7) != 7) {
                actor->shape_angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
                actor->current.angle.y = actor->shape_angle.y;
                i_this->m2D0 = 3;
                i_this->mAction = 33;
            } else {
                i_this->m2D0 = 0;
                i_this->mAction = 0;
            }
        }
        break;
    }
    }

    if (i_this->m2D0 == 4) {
        s16 rot = actor->current.angle.y + 0x8000;
        cMtx_YrotS(*calc_mtx, rot);
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = 60.0f;
        MtxPosition(&offset, &pos);
        pos += i_this->m384[2];
        cLib_addCalc2(&actor->current.pos.x, pos.x, 1.0f, 10.0f);
        cLib_addCalc2(&actor->current.pos.y, i_this->m348.y, 1.0f, 10.0f);
        cLib_addCalc2(&actor->current.pos.z, pos.z, 1.0f, 10.0f);
        offset = i_this->m384[0] - i_this->m384[1];
        actor->current.angle.y = cM_atan2s(offset.x, offset.z) + 0x4000;
        cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x1000);
    }
}

/* 00004204-000046F0       .text action_fly_damage__FP8gm_class */
void action_fly_damage(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif

    switch (i_this->mAction) {
    case 50:
        i_this->mBodyCyl.SetCoVsGrp(cCcD_CoSPrm_VsGrpAll_e);
        if (i_this->m2D1 != 4) {
            i_this->mpBrkDamage->setFrame(0.0f);
            i_this->mBrkMode = 1;
        } else {
            i_this->mpBrkGm->setFrame(0.0f);
            i_this->mBrkMode = 0;
        }
        anm_init(i_this, dRes_INDEX_GM_BCK_PAUSE_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        actor->speedF = 30.0f;
        i_this->m320 = 0;
        /* fallthrough */
    case 52: {
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
        f32 sign = 1.0f;
        if (cM_rnd() < 0.5f) {
            sign = -1.0f;
        }
        i_this->mTimers[16] += (s16)(10000.0f * sign);
        sign = 1.0f;
        if (cM_rnd() < 0.5f) {
            sign = -1.0f;
        }
        i_this->mTimers[17] += (s16)(10000.0f * sign);
        sign = 1.0f;
        if (cM_rnd() < 0.5f) {
            sign = -1.0f;
        }
        i_this->mTimers[18] += (s16)(2000.0f * sign);
        i_this->mTimers[19] = 5000;
        i_this->mTimers[20] = 5000;
        i_this->mTimers[21] = 7000;
        actor->speed.setall(0.0f);
        actor->gravity = 0.0f;
        i_this->mAction = 51;
        /* fallthrough */
    }
    case 51: {
        int i, j;
        for (i = 0, j = 4; i < 4; i++, j += 3) {
            i_this->mTimers[j] += i_this->mTimers[16];
            i_this->mTimers[j + 1] += i_this->mTimers[17];
            i_this->mTimers[j + 2] += i_this->mTimers[18];
            i_this->mWingAngle[i].y = 5000.0f * cM_ssin(i_this->mTimers[j]);
            i_this->mWingAngle[i].x = 5000.0f * cM_scos(i_this->mTimers[j + 1]);
            i_this->mWingAngle[i].z = 3000.0f * cM_scos(i_this->mTimers[j + 2]);
        }
        i_this->mTimers[0] += i_this->mTimers[19];
        i_this->mTimers[1] += i_this->mTimers[20];
        i_this->mTimers[2] += i_this->mTimers[21];
        i_this->mExtraJointRot.y = -3500.0f * cM_ssin(i_this->mTimers[0]);
        i_this->mExtraJointRot.x = -3500.0f * cM_scos(i_this->mTimers[1]);
        i_this->mExtraJointRot.z = -2000.0f * cM_scos(i_this->mTimers[2]);
        for (int i = 0; i < 6; i++) {
            s16 target = 0;
            cLib_addCalcAngleS2(&i_this->mTimers[16 + i], target, 1, 80);
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 2.0f);
        if (actor->speedF < 0.2f) {
            actor->speedF = 0.0f;
            i_this->mpBrkGm->setFrame(0.0f);
            i_this->mBrkMode = 0;
            if (i_this->mBckIdx == dRes_INDEX_GM_BCK_PAUSE_e && (i_this->mWingBits & 7) != 7) {
                i_this->m2D0 = 3;
                i_this->mAction = 37;
            } else {
                i_this->m2D0 = 0;
                i_this->mAction = 0;
            }
        }
        break;
    }
    }

    if (actor->current.pos.y < 100.0f + i_this->mAcch.GetGroundH()) {
        cLib_addCalc2(&actor->current.pos.y, 100.0f + i_this->mAcch.GetGroundH(), 1.0f, 3.0f);
    }
}

/* 000046F0-00005C78       .text action_ground_attack__FP8gm_class */
void action_ground_attack(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz offset;
    cXyz pos;
    f32 dx = player->current.pos.x - (actor->current.pos.x + i_this->mDrawOffset.x);
    f32 dz = player->current.pos.z - (actor->current.pos.z + i_this->mDrawOffset.z);

    switch (i_this->mAction) {
    case 60:
        i_this->mBodyCyl.SetCoVsGrp(cCcD_CoSPrm_VsGrpAll_e);
        i_this->mWeaponSph.SetAtSpl(dCcG_At_Spl_UNK0);
        actor->current.angle.y = actor->shape_angle.y;
        i_this->m436 = 0;
        actor->speedF = 10.0f;
        i_this->mDrawOffset.x = 0.0f;
        i_this->mDrawOffset.y = 0.0f;
        i_this->mDrawOffset.z = 0.0f;
        i_this->mFuwafuwa.x = 0.0f;
        i_this->mFuwafuwa.y = 0.0f;
        i_this->mFuwafuwa.z = 0.0f;
        /* fallthrough */
    case 61:
        i_this->mpBrkGm->setFrame(0.0f);
        i_this->mBrkMode = 0;
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        i_this->mWingCyl.OffAtSetBit();
        i_this->initBt(120.0f + REG8_F(4), 100.0f + REG8_F(5));
        i_this->setBtAttackData(0.0f, 10.0f, 800.0f + REG8_F(6), 1);
        i_this->setBtNowFrame(1000.0f);
        i_this->mWallR = 250.0f;
        if (actor->health <= 0) {
            if (i_this->m2CE != 0) {
                anm_init(i_this, dRes_INDEX_GM_BCK_G_DEAD01_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                i_this->mAction = 100;
                break;
            }
            i_this->m2D0 = 20;
            i_this->mAction = 200;
            return;
        }
        actor->speed.setall(0.0f);
        actor->speed.y = 10.0f;
        actor->gravity = -3.0f;
        i_this->mAction = 62;
        /* fallthrough */
    case 62:
        actor->shape_angle.x += 0x100;
        if (actor->current.pos.y < 80.0f + i_this->mAcch.GetGroundH()) {
            if (actor->health <= 0) {
                anm_init(i_this, dRes_INDEX_GM_BCK_G_DEAD02_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            } else {
                anm_init(i_this, dRes_INDEX_GM_BCK_G_CHAKUCHI_e, 10.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            }
            i_this->m338 = -20.0f;
            i_this->m33C = 60.0f;
            i_this->m340 = 100.0f;
            actor->speedF = 0.0f;
            i_this->mAction++;
        }
        break;
    case 63:
        if (!i_this->mAcch.ChkGroundHit()) {
            break;
        }
        fopAcM_seStart(actor, JA_SE_CM_GM_LANDING, 0);
        i_this->mAction = 70;
        break;
    case 70:
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        anm_init(i_this, dRes_INDEX_GM_BCK_G_WAIT_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        actor->speedF = 0.0f;
        i_this->mAction++;
        /* fallthrough */
    case 71:
        if (!i_this->mpMorf->checkFrame(23.0f)) {
            break;
        }
        i_this->mTimers[3]++;
        if (i_this->mTimers[3] <= (s16)(REG8_S(5) + 1)) {
            break;
        }
        i_this->mAction++;
        break;
    case 72: {
        i_this->mTimers[7] = 20.0f + cM_rndF(2.0f * (fopAcM_GetID(actor) & 7));
        i_this->mTimers[2] = 0;
        cMtx_XrotS(*calc_mtx, i_this->m43E.x);
        cMtx_ZrotM(*calc_mtx, i_this->m43E.z);
        offset.x = 0.0f;
        offset.y = 100.0f;
        offset.z = 0.0f;
        MtxPosition(&offset, &pos);
        pos += player->current.pos;
        if (!Line_check(i_this, pos)) {
            actor->current.angle.y = cM_atan2s(dx, dz);
        } else if (i_this->m2CE != 1) {
            if (i_this->mBckIdx == dRes_INDEX_GM_BCK_G_WAIT_e) {
                break;
            }
            anm_init(i_this, dRes_INDEX_GM_BCK_G_WAIT_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            break;
        } else {
            actor->speedF = 30.0f;
            i_this->m326 = 0;
            i_this->m326 = REG8_S(6) + 0x4000;
            if (cM_rnd() < 0.5f) {
                i_this->m326 = -(REG8_S(6) + 0x4000);
            }
            actor->current.angle.y = cM_atan2s(dx, dz);
            actor->current.angle.y += i_this->m326;
        }
        if (i_this->m2CE == 1) {
            i_this->mWallH = 60.0f;
            i_this->mWallR = 70.0f;
        }
        anm_init(i_this, dRes_INDEX_GM_BCK_G_WALK_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->mAction++;
        /* fallthrough */
    }
    case 73: {
        fopAcM_seStart(actor, JA_SE_CM_GM_FOOTNOTE, 0);
        s16 dang = cLib_distanceAngleS(actor->current.angle.y, actor->shape_angle.y);
        if (dang < 0x100) {
            actor->speedF = 30.0f;
            i_this->mTimers[2]++;
            if (i_this->mTimers[2] > i_this->mTimers[7]) {
                i_this->mAction = 70;
                break;
            }
            if (i_this->m2CE != 1) {
                cMtx_YrotS(*calc_mtx, actor->current.angle.y);
                cMtx_XrotM(*calc_mtx, i_this->m43E.x);
                cMtx_ZrotM(*calc_mtx, i_this->m43E.z);
                offset.x = 0.0f;
                offset.y = 100.0f;
                offset.z = 200.0f;
                MtxPosition(&offset, &pos);
                pos += i_this->mAtamaPos;
                if (Line_check(i_this, pos)) {
                    i_this->mAction = 70;
                    break;
                }
            } else {
                actor->current.angle.y = cM_atan2s(dx, dz);
            }
        }
        {
            f32 dist = std::sqrtf(dx * dx + dz * dz);
            if (dist > 400.0f) {
                break;
            }
        }
        if (i_this->m2CE == 1) {
            cMtx_YrotS(*calc_mtx, actor->current.angle.y);
            cMtx_XrotM(*calc_mtx, i_this->m43E.x);
            cMtx_ZrotM(*calc_mtx, i_this->m43E.z);
            offset.x = 0.0f;
            offset.y = 100.0f;
            offset.z = 200.0f;
            MtxPosition(&offset, &pos);
            pos += i_this->mAtamaPos;
            if (Line_check(i_this, pos)) {
                i_this->mAction = 70;
                break;
            }
        }
        i_this->m2DA = 0;
        i_this->mAction++;
        /* fallthrough */
    }
    case 74:
        anm_init(i_this, dRes_INDEX_GM_BCK_G_ATACK01_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_ATTACK, 0);
        i_this->mBrkMode = 2;
        actor->speedF = 0.0f;
        i_this->mAction++;
        break;
    case 75:
        fopAcM_seStart(actor, JA_SE_CM_GM_FOOTNOTE, 0);
        actor->current.angle.y = cM_atan2s(
            player->current.pos.x - (actor->current.pos.x + i_this->mDrawOffset.x),
            player->current.pos.z - (actor->current.pos.z + i_this->mDrawOffset.z)
        );
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        anm_init(i_this, dRes_INDEX_GM_BCK_G_ATACK02_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mWeaponSph.OnAtSetBit();
        i_this->mWeaponSph.OnAtHitBit();
        i_this->mBrkMode = 0;
        actor->speedF = 35.0f;
        actor->gravity = -3.0f;
        actor->speed.y = 20.0f;
        i_this->mAction++;
        break;
    case 76:
        if (i_this->mTimers[4] == 0 && i_this->mWeaponSph.ChkAtShieldHit()) {
            i_this->mWeaponSph.OffAtSetBit();
            i_this->mWeaponSph.OffAtSetBit();
            actor->speedF = -35.0f;
            i_this->mTimers[4] = 1;
        }
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        anm_init(i_this, dRes_INDEX_GM_BCK_G_ATACK03_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GM_BAS_G_ATACK03_e);
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mTimers[4] = 0;
        i_this->mAction++;
        break;
    case 77:
        if (!i_this->mAcch.ChkGroundHit()) {
            break;
        }
        actor->speedF = 0.0f;
        i_this->mAction = 70;
        break;
    case 80:
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWeaponSph.OffAtSetBit();
        if (i_this->m2D1 != 4) {
            fopAcM_monsSeStart(actor, JA_SE_CV_GM_DAMAGE, 0);
            i_this->mpBrkDamage->setFrame(0.0f);
            i_this->mBrkMode = 1;
            i_this->m2EC = 0;
            anm_init(i_this, dRes_INDEX_GM_BCK_G_DAMAGE_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            actor->speedF = 35.0f;
        } else {
            i_this->m2EC = 45;
            actor->speedF = 0.0f;
            i_this->mpBrkGm->setFrame(0.0f);
            i_this->mBrkMode = 0;
        }
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
        if (i_this->m2D1 == 1) {
            actor->gravity = -3.0f;
            actor->speed.y = 20.0f;
        }
        i_this->mAction++;
        break;
    case 81:
        cLib_addCalc0(&actor->speedF, 1.0f, 1.5f);
        if (i_this->m2EC == (s16)(REG8_S(7) + 44)) {
            enemy_piyo_set(actor);
            fopAcM_seStart(actor, JA_SE_CM_MD_PIYO, 0);
        }
        if ((i_this->m2D1 != 4 && i_this->mpMorf->isStop()) || (i_this->m2D1 == 4 && i_this->m2EC == 0)) {
            anm_init(i_this, dRes_INDEX_GM_BCK_G_WALK_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 2.0f, -1);
            i_this->mpBrkGm->setFrame(0.0f);
            i_this->mBrkMode = 0;
            i_this->mAction++;
        }
        break;
    case 82:
        fopAcM_seStart(actor, JA_SE_CM_GM_FOOTNOTE, 0);
        actor->current.angle.y = cM_atan2s(dx, dz) + 0x8000;
        {
            s16 dang = cLib_distanceAngleS(actor->current.angle.y, actor->shape_angle.y);
            if (dang < 0x100) {
                anm_init(i_this, dRes_INDEX_GM_BCK_G_MACHINGUN_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                i_this->m2E2 = 4;
                i_this->mTimers[5] = cM_atan2s(dx, dz) + 0x8000;
                i_this->mTimers[6] = 0x4000;
                i_this->mAction++;
            }
        }
        break;
    case 83:
        if (i_this->m2E2 == 0) {
            actor->current.angle.y = i_this->mTimers[5] + i_this->mTimers[6];
            i_this->m2E2 = 4;
            i_this->mTimers[6] ^= 0x4000;
        }
        if (ks_set_rtn(i_this)) {
            i_this->mAction = 70;
        }
        break;
    case 84:
        anm_init(i_this, dRes_INDEX_GM_BCK_G_UCHIWA_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
        actor->speedF = 20.0f;
        i_this->mWeaponSph.OffAtSetBit();
        i_this->mWeaponSph.OffAtSetBit();
        fopAcM_seStart(actor, JA_SE_CM_GM_HIT_WIND, 0);
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_HIT_WIND, 0);
        i_this->mAction++;
        break;
    case 85:
        cLib_addCalc0(&actor->speedF, 1.0f, 3.0f);
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        i_this->mAction = 82;
        break;
    case 90:
        i_this->mWingBits = 0;
        anm_init(i_this, dRes_INDEX_GM_BCK_G_HANE_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GM_BAS_G_HANE_e);
        actor->speedF = 0.0f;
        i_this->mAction++;
        break;
    case 91:
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        actor->speed.y = 0.0f;
        actor->gravity = 0.0f;
        i_this->m43E.x = 0;
        i_this->m43E.y = 0;
        i_this->m43E.z = 0;
        i_this->mWeaponSph.SetAtSpl(dCcG_At_Spl_UNK5);
        i_this->initBt(200.0f + REG8_F(4), 100.0f + REG8_F(5));
        i_this->setBtAttackData(0.0f, 10.0f, 880.0f + REG8_F(6), 1);
        i_this->setBtNowFrame(1000.0f);
        i_this->m2D0 = 3;
        i_this->mAction = 37;
        break;
    case 100:
        i_this->m2EC = 0;
        actor->gravity = -3.0f;
        i_this->mpBrkGm->setFrame(0.0f);
        i_this->mBrkMode = 0;
        i_this->mStts.SetWeight(0xFF);
        fopAcM_monsSeStart(actor, JA_SE_CV_GM_DIE, 0);
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        if (i_this->m2D1 == 7) {
            actor->speedF = 0.0f;
            anm_init(i_this, dRes_INDEX_GM_BCK_G_HAMMER_e, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        } else {
            actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
            actor->speedF = 45.0f;
            actor->gravity = -3.0f;
            actor->speed.y = 45.0f;
#if VERSION > VERSION_DEMO
            i_this->m2E2 = 200;
#endif
            anm_init(i_this, dRes_INDEX_GM_BCK_G_DEAD01_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        }
        i_this->mAction++;
        break;
    case 101:
        if (i_this->m2D1 == 7) {
            if (!i_this->mpMorf->isStop()) {
                break;
            }
            fopAcM_createDisappear(actor, &actor->current.pos, 10, daDisItem_IBALL_e, actor->stealItemBitNo);
            fopAcM_delete(actor);
            fopAcM_onActor(actor);
            if (i_this->mSwitchNo != 0xFF && i_this->mSwitchEnable != 0) {
                dComIfGs_onSwitch(i_this->mSwitchNo, DEMO_SELECT(fopAcM_GetRoomNo(actor), actor->current.roomNo));
            }
            break;
        }
        cLib_addCalc0(&actor->speedF, 1.0f, 1.5f);
        switch (i_this->mTimers[0]) {
        case 0:
#if VERSION == VERSION_DEMO
            if (!i_this->mAcch.ChkGroundHit()) {
#else
            if (i_this->m2E2 != 0 && !i_this->mAcch.ChkGroundHit()) {
#endif
                break;
            }
            fopAcM_seStart(actor, JA_SE_CM_GM_DIE_LANDING, 0);
            anm_init(i_this, dRes_INDEX_GM_BCK_G_DEAD02_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->mTimers[0] = 1;
            actor->speedF = 0.0f;
            break;
        case 1:
            if (!i_this->mpMorf->isStop()) {
                break;
            }
            i_this->mTimers[0]++;
            i_this->mpBrkDead->setFrame(0.0f);
            i_this->mBrkMode = 3;
            break;
        case 2:
            if (!i_this->mpBrkDead->isStop()) {
                break;
            }
            i_this->mTimers[0]++;
            break;
        case 3:
#if VERSION == VERSION_DEMO
            if (!i_this->mAcch.ChkGroundHit()) {
#else
            if (i_this->m2E2 != 0 && !i_this->mAcch.ChkGroundHit()) {
#endif
                break;
            }
            i_this->mTimers[1]++;
            if (i_this->mTimers[1] <= 15) {
                break;
            }
            fopAcM_createDisappear(actor, &actor->current.pos, 10, daDisItem_IBALL_e, actor->stealItemBitNo);
            if (i_this->mSwitchNo != 0xFF && i_this->mSwitchEnable != 0) {
                dComIfGs_onSwitch(i_this->mSwitchNo, DEMO_SELECT(fopAcM_GetRoomNo(actor), actor->current.roomNo));
            }
            fopAcM_delete(actor);
            fopAcM_onActor(actor);
            i_this->mTimers[0]++;
            break;
        }
        break;
    }

    i_this->setBtNowFrame(1000.0f);
    if (i_this->mAction >= 74 && i_this->mAction <= 77) {
        i_this->m2DA++;
        if (i_this->m2DA > (s16)REG8_F(7) && i_this->m2DA < (s16)(17.0f + REG8_F(8))) {
            i_this->setBtNowFrame(5.0f);
        }
    }
    {
        s16 angTarget = 0;
        cLib_addCalcAngleS2(&i_this->m320, angTarget, 1, 0x800);
        cLib_addCalcAngleS2(&actor->shape_angle.x, angTarget, 1, 0x800);
    }
    if (i_this->mAction != 81 && i_this->mAction != 101 && i_this->mAction != 85) {
        cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x800);
    }
    if (i_this->mBckIdx != dRes_INDEX_GM_BCK_G_DEAD01_e && i_this->mBckIdx != dRes_INDEX_GM_BCK_G_DEAD02_e) {
        if (body_atari_check(i_this)) {
            i_this->m2D0 = 10;
            if (i_this->m2D1 == 7) {
                actor->health = 0;
            }
            if (actor->health > 0) {
                i_this->mAction = 80;
                if (i_this->m2D1 == 3) {
                    i_this->mAction = 84;
                }
                return;
            }
            if (i_this->m2CE != 0) {
                i_this->mAction = 100;
            } else {
                i_this->m2D0 = 20;
                i_this->mAction = 200;
            }
            return;
        }
    }
    if (i_this->mAction == 71 || i_this->mAction == 72 || i_this->mAction == 73) {
        if (i_this->m2CE != 1 && i_this->m2E6 == 0) {
            i_this->mAction = 90;
        }
    }
    if (i_this->m2D0 == 10 && i_this->mAcch.ChkGroundHit()) {
        fopAcM_getGroundAngle(actor, &i_this->m43E);
    }
}

/* 00005C78-00006A98       .text action_demo__FP8gm_class */
void action_demo(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player_ac = dComIfGp_getPlayer(0);
#endif
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    dBgS_CamLinChk_NorWtr linChk;
    cXyz offset;
    cXyz pos;

    switch (i_this->mAction) {
    case 200:
        for (int i = 0; i < 23; i++) {
            i_this->mTimers[i] = 0;
        }
        actor->attention_info.flags = 0;
        if (actor->health > 0) {
            actor->current.pos.x = -100.0f;
            actor->current.pos.y = 570.0f;
            actor->current.pos.z = -1350.0f;
            actor->current.angle.y = 0;
            actor->shape_angle.y = actor->current.angle.y;
            actor->shape_angle.x = -0x4000;
            i_this->m44C = 1;
            anm_init(i_this, dRes_INDEX_GM_BCK_TOMARU_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m320 = -0x8000;
        }
        i_this->mAction++;
        /* fallthrough */
    case 201:
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
        i_this->m478 = 50.0f;
        if (actor->health > 0) {
            i_this->mAction++;
            actor->scale.setall(1.0f);
            break;
        }
        i_this->mpBrkGm->setFrame(0.0f);
        i_this->mBrkMode = 0;
        actor->current.angle.y = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
        actor->speedF = 20.0f;
        actor->gravity = -3.0f;
        actor->speed.y = 45.0f;
        player->changeDemoMode(daPy_demo_c::DEMO_UNK_029_e);
        anm_init(i_this, dRes_INDEX_GM_BCK_G_DEAD01_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mAction = 220;
        break;
    case 202: {
        pos.x = 73.0f + REG12_F(5);
        pos.y = DEMO_SELECT(player_ac, player)->current.pos.y;
        pos.z = 1284.0f + REG12_F(6);
        player->setPlayerPosAndAngle(&pos, -0x8000);
        i_this->m45C.x = 74.0f + REG12_F(7);
        i_this->m45C.y = 107.0f + REG12_F(8);
        i_this->m45C.z = 1095.0f + REG12_F(9);
        i_this->m450.x = 71.0f + REG12_F(10);
        i_this->m450.y = 124.0f + REG12_F(11);
        i_this->m450.z = 800.0f + REG12_F(12);
        i_this->m2E2 = 10;
        i_this->mAction++;
        /* fallthrough */
    }
    case 203:
        if (i_this->m2E2 != 0) {
            break;
        }
        player->changeDemoMode(daPy_demo_c::DEMO_N_WALK_e);
        i_this->m44C = 2;
        player->changeDemoMoveAngle(-0x8000);
        actor->current.pos.x = 100.0f;
        actor->current.pos.y = 450.0f + REG12_F(15);
        actor->current.pos.z = 1150.0f;
        mDoAud_bgmAllMute(30);
        i_this->m2E2 = 60;
        i_this->mAction = 205;
        break;
    case 205:
        if (i_this->m2E2 != 0) {
            break;
        }
        player->changeDemoMode(daPy_demo_c::DEMO_LOOKUP_e);
        i_this->m2E2 = 50;
        i_this->mAction++;
        break;
    case 206:
        if (i_this->m2E2 != 0) {
            break;
        }
        player->changeDemoMode(daPy_demo_c::DEMO_TBACK_e);
        i_this->m2E2 = 80;
        actor->current.pos.x = 100.0f;
        actor->current.pos.y = 570.0f;
        actor->current.pos.z = 1350.0f;
        i_this->mAction++;
        break;
    case 207: {
        f32 step = 0.1f * std::fabsf(i_this->m45C.x - 80.0f);
        cLib_addCalc2(&i_this->m45C.x, 80.0f + REG12_F(1), 1.0f, step);
        step = 0.1f * std::fabsf(i_this->m45C.y - 605.0f);
        cLib_addCalc2(&i_this->m45C.y, 605.0f + REG12_F(2), 1.0f, step);
        step = 0.1f * std::fabsf(i_this->m45C.z - 877.0f);
        cLib_addCalc2(&i_this->m45C.z, 877.0f + REG12_F(3), 1.0f, step);
        step = 0.1f * std::fabsf(i_this->m450.x - 76.0f);
        cLib_addCalc2(&i_this->m450.x, 76.0f + REG12_F(4), 1.0f, step);
        step = 0.1f * std::fabsf(i_this->m450.y - 621.0f);
        cLib_addCalc2(&i_this->m450.y, 621.0f + REG12_F(5), 1.0f, step);
        step = 0.1f * std::fabsf(i_this->m450.z - 582.0f);
        cLib_addCalc2(&i_this->m450.z, 582.0f + REG12_F(6), 1.0f, step);
        f32 target;
        if (i_this->m2E2 != 0) {
            break;
        }
        anm_init(i_this, dRes_INDEX_GM_BCK_JETATACK_e, 0.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        step = i_this->mSpawnPos.x - (actor->current.pos.x + i_this->mDrawOffset.x);
        target = i_this->mSpawnPos.z - (actor->current.pos.z + i_this->mDrawOffset.z);
        actor->current.angle.y = cM_atan2s(step, target);
        i_this->m320 = 0;
        actor->shape_angle.x = 0x4000;
        actor->shape_angle.y += (s16)-0x8000;
        i_this->m2E2 = 47;
        actor->speedF = 22.0f;
        actor->gravity = 1.0f;
        actor->speed.y = -65.0f;
        player->changeDemoMode(daPy_demo_c::DEMO_UNK00_e);
        i_this->mAction++;
        break;
    }
    case 208: {
        fopAcM_seStart(actor, JA_SE_CM_GM_JET, 0);
        {
#if VERSION == VERSION_DEMO
            if (actor->current.pos.y < 220.0f + player_ac->current.pos.y) {
                actor->gravity = 0.0f;
                actor->speed.y = 0.0f;
                cLib_addCalc2(&actor->current.pos.y, 220.0f + player_ac->current.pos.y, 1.0f, 50.0f);
            }
#else
            f32 target = 220.0f;
            if (actor->current.pos.y < target + player->current.pos.y) {
                actor->gravity = 0.0f;
                actor->speed.y = 0.0f;
                cLib_addCalc2(&actor->current.pos.y, target + player->current.pos.y, 1.0f, 50.0f);
            }
#endif
        }
        cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 1, 0x1000);
        cLib_addCalcAngleS2(&actor->shape_angle.x, 0, 1, 1280.0f + REG12_F(16));
        f32 step = 50.0f + REG12_F(17);
        cLib_addCalc2(&i_this->m45C.x, actor->current.pos.x, 1.0f, step);
        cLib_addCalc2(&i_this->m45C.y, actor->current.pos.y + REG12_F(10), 1.0f, step);
        cLib_addCalc2(&i_this->m45C.z, actor->current.pos.z, 1.0f, step);
        cLib_addCalc2(&i_this->m450.x, 67.0f + REG12_F(7), 1.0f, step);
        cLib_addCalc2(&i_this->m450.y, 95.0f + REG12_F(8), 1.0f, step);
        cLib_addCalc2(&i_this->m450.z, 1.0f + REG12_F(9), 1.0f, step);
        if (i_this->m2E2 != 0) {
            break;
        }
        dComIfGp_getVibration().StopQuake(0x20);
        camera->mCamera.Reset(i_this->m45C, i_this->m450);
        camera->mCamera.Start();
        camera->mCamera.SetTrimSize(0);
        dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
        actor->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
        actor->attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 4;
        actor->current.angle.x = 0;
        i_this->m44C = 0;
        mDoAud_subBgmStart(JA_BGM_MBOSS);
        fopAcM_OnStatus(actor, fopAcStts_SHOWMAP_e);
        fopAcM_OffStatus(actor, fopAcStts_UNK4000_e);
        i_this->m2D0 = 0;
        i_this->mAction = 0;
        break;
    }
    case 220:
        mDoAud_subBgmStop();
#if VERSION > VERSION_DEMO
        i_this->m2E2 = 200;
#endif
        i_this->mAction++;
        /* fallthrough */
    case 221: {
        f32 speed = actor->speedF;
        if (speed) {
#if VERSION == VERSION_DEMO
            if (i_this->mAcch.ChkGroundHit()) {
#else
            if (i_this->m2E2 == 0 || i_this->mAcch.ChkGroundHit()) {
#endif
                if (i_this->mBckIdx != dRes_INDEX_GM_BCK_G_DEAD02_e) {
                    anm_init(i_this, dRes_INDEX_GM_BCK_G_DEAD02_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                }
                actor->speedF = 0.0f;
            }
        }
        {
            s16 ang = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            player->setPlayerPosAndAngle(&DEMO_SELECT(player_ac, player)->current.pos, ang + 0x8000);
        }
        if (REG8_S(8) != 0) {
            break;
        }
        speed = actor->speedF;
        if (speed) {
            break;
        }
        if (i_this->mBckIdx != dRes_INDEX_GM_BCK_G_DEAD02_e) {
            break;
        }
        if (!i_this->mpMorf->isStop()) {
            break;
        }
        i_this->mpBrkDead->setFrame(0.0f);
        i_this->mBrkMode = 3;
        i_this->mAction++;
        break;
    }
    case 222:
        if (!i_this->mpBrkDead->isStop()) {
            break;
        }
        fopAcM_createDisappear(actor, &actor->current.pos, 10, daDisItem_IBALL_e, actor->stealItemBitNo);
        actor->scale.setall(0.0f);
        i_this->m2E2 = 100;
        i_this->mAction++;
        break;
    case 223:
        if (i_this->m2E2 != 0) {
            break;
        }
        dComIfGp_getVibration().StopQuake(0x20);
        camera->mCamera.Reset(i_this->m45C, i_this->m450);
        camera->mCamera.Start();
        camera->mCamera.SetTrimSize(0);
        player->cancelOriginalDemo();
        dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
        if (i_this->mSwitchNo != 0xFF && i_this->mSwitchEnable != 0) {
            dComIfGs_onSwitch(i_this->mSwitchNo, DEMO_SELECT(fopAcM_GetRoomNo(actor), actor->current.roomNo));
        }
        fopAcM_delete(actor);
        fopAcM_onActor(actor);
        break;
    }

    if (i_this->mAction >= 221) {
        i_this->m45C.x = actor->current.pos.x;
        i_this->m45C.y = actor->current.pos.y;
        i_this->m45C.z = actor->current.pos.z;
        cMtx_YrotS(*calc_mtx, fopAcM_searchActorAngleY(DEMO_SELECT(player_ac, player), actor));
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = 40.0f;
        MtxPosition(&offset, &pos);
        pos += DEMO_SELECT(player_ac, player)->current.pos;
        i_this->m450.x = pos.x;
        i_this->m450.y = 200.0f + pos.y;
        i_this->m450.z = pos.z;
    }
    if (i_this->mAction >= 203 && i_this->mAction != 220) {
        camera->mCamera.Set(i_this->m45C, i_this->m450, i_this->m478, 0);
    }
}

/* 00006A98-00007770       .text daGM_Execute__FP8gm_class */
static BOOL daGM_Execute(gm_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (i_this->m2CD == 0) {
        fopAcM_setGbaName(actor, dItemNo_BOOMERANG_e, 9, 38);

        if (i_this->m44C != 1) {
            if ((i_this->mWingBits & 4) == 0) {
                switch (i_this->m2D3) {
                case 0:
                    i_this->mpEmitters[0] = dComIfGp_particle_set(
                        dPa_name::ID_AK_SN_GMRINPUN00,
                        &actor->current.pos,
                        &actor->shape_angle
                    );
                    if (i_this->mpEmitters[0] != NULL) {
                        i_this->m2D3++;
                    }
                    break;
                case 1:
                    if (i_this->mpEmitters[0] != NULL && i_this->m2D0 == 5) {
                        i_this->m2DE = 0;
                        i_this->m2D3++;
                    }
                    break;
                case 2:
                    if (i_this->mpEmitters[0] != NULL) {
                        i_this->mpEmitters[0]->setRate(80.0f);
                        i_this->mpEmitters[0]->setRandomDirectionSpeed(80.0f);
                        i_this->mpEmitters[0]->setGlobalPrmColor(255, 0, 0);
                        i_this->m2D3++;
                    }
                    break;
                case 3:
                    if (i_this->mpEmitters[0] != NULL) {
                        i_this->mpEmitters[0]->setRate(3.0f);
                        i_this->mpEmitters[0]->setRandomDirectionSpeed(0.0f);
                        i_this->m2D3++;
                    }
                    break;
                case 4:
                    if (i_this->mpEmitters[0] != NULL) {
                        i_this->m2DE += 17;
                        if (i_this->m2DE > 255) {
                            i_this->m2DE = 255;
                            i_this->m2D3 = 1;
                        }
                        i_this->mpEmitters[0]->setGlobalPrmColor(255, i_this->m2DE, i_this->m2DE);
                    }
                    break;
                }

                if (i_this->mpEmitters[0] != NULL) {
                    i_this->mpEmitters[0]->setGlobalRTMatrix(
                        i_this->mpMorf->getModel()->getAnmMtx(GM_JNT_HANEULB_e)
                    );
                }
            } else if (i_this->mpEmitters[0] != NULL) {
                i_this->mpEmitters[0]->becomeInvalidEmitter();
                i_this->m2D3 = 0;
                i_this->mpEmitters[0] = NULL;
            }

            if ((i_this->mWingBits & 8) == 0) {
                switch (i_this->m2D4) {
                case 0:
                    i_this->mpEmitters[1] = dComIfGp_particle_set(
                        dPa_name::ID_AK_SN_GMRINPUN00,
                        &actor->current.pos,
                        &actor->shape_angle
                    );
                    if (i_this->mpEmitters[1] != NULL) {
                        i_this->m2D4++;
                    }
                    break;
                case 1:
                    if (i_this->mpEmitters[1] != NULL && i_this->m2D0 == 5) {
                        i_this->m2E0 = 0;
                        i_this->m2D4++;
                    }
                    break;
                case 2:
                    if (i_this->mpEmitters[1] != NULL) {
                        i_this->mpEmitters[1]->setRate(80.0f);
                        i_this->mpEmitters[1]->setRandomDirectionSpeed(80.0f);
                        i_this->mpEmitters[1]->setGlobalPrmColor(255, 0, 0);
                        i_this->m2D4++;
                    }
                    break;
                case 3:
                    if (i_this->mpEmitters[1] != NULL) {
                        i_this->mpEmitters[1]->setRate(3.0f);
                        i_this->mpEmitters[1]->setRandomDirectionSpeed(0.0f);
                        i_this->m2D4++;
                    }
                    break;
                case 4:
                    if (i_this->mpEmitters[1] != NULL) {
                        i_this->m2E0 += 17;
                        if (i_this->m2E0 > 255) {
                            i_this->m2E0 = 255;
                            i_this->m2D4 = 1;
                        }
                        i_this->mpEmitters[1]->setGlobalPrmColor(255, i_this->m2E0, i_this->m2E0);
                    }
                    break;
                }

                if (i_this->mpEmitters[1] != NULL) {
                    i_this->mpEmitters[1]->setGlobalRTMatrix(
                        i_this->mpMorf->getModel()->getAnmMtx(GM_JNT_HANEURB_e)
                    );
                    i_this->mpEmitters[1]->setEmitterRotation(JGeometry::TVec3<s16>(0x471C, -0x1555, 0));
                    JGeometry::TVec3<f32> emTrans(40.0f, 0.0f, -30.0f);
                    i_this->mpEmitters[1]->setEmitterTranslation(emTrans);
                }
            } else if (i_this->mpEmitters[1] != NULL) {
                i_this->mpEmitters[1]->becomeInvalidEmitter();
                i_this->m2D4 = 0;
                i_this->mpEmitters[1] = NULL;
            }
        }

        if (i_this->m2EE != 0 || i_this->mAction == 34 || i_this->mAction == 208) {
            switch (i_this->m2D5) {
            case 0:
                i_this->mpEmitters[2] = dComIfGp_particle_set(
                    dPa_name::ID_AK_SN_GMJETATTACKSMOKE00,
                    &actor->current.pos,
                    &actor->shape_angle
                );
                if (i_this->mpEmitters[2] != NULL) {
                    i_this->m2D5++;
                }
                break;
            case 1:
                i_this->mpEmitters[3] = dComIfGp_particle_set(
                    dPa_name::ID_AK_SN_GMJETATTACKFIRE00,
                    &actor->current.pos,
                    &actor->shape_angle
                );
                if (i_this->mpEmitters[3] != NULL) {
                    i_this->m2D5++;
                }
                break;
            }

            if (i_this->mAction == 34) {
                i_this->m2EE = 45;
            }

            if (i_this->mpEmitters[2] != NULL) {
                i_this->mpEmitters[2]->setGlobalRTMatrix(
                    i_this->mpMorf->getModel()->getAnmMtx(GM_JNT_HARAC_e)
                );
            }
            if (i_this->mpEmitters[3] != NULL) {
                i_this->mpEmitters[3]->setGlobalRTMatrix(
                    i_this->mpMorf->getModel()->getAnmMtx(GM_JNT_HARAC_e)
                );
            }
        } else {
            if (i_this->mpEmitters[2] != NULL) {
                i_this->mpEmitters[2]->becomeInvalidEmitter();
                i_this->m2D5 = 0;
                i_this->mpEmitters[2] = NULL;
            }
            if (i_this->mpEmitters[3] != NULL) {
                i_this->mpEmitters[3]->becomeInvalidEmitter();
                i_this->m2D5 = 0;
                i_this->mpEmitters[3] = NULL;
            }
        }

        if (i_this->m2CD == 0) {
            if (enemy_ice(&i_this->mEnemyIce)) {
                J3DModel* model = i_this->mpMorf->getModel();
                model->setBaseTRMtx(mDoMtx_stack_c::get());
                i_this->mpMorf->calc();
                actor->attention_info.position = i_this->mAtamaPos;
                actor->attention_info.position.y += 100.0f;
                actor->eyePos = i_this->mAtamaPos;
                return TRUE;
            }
        }
    }

    for (s32 i = 0; i < 7; i++) {
        if ((&i_this->m2E2)[i] != 0) {
            (&i_this->m2E2)[i]--;
        }
    }

    switch (i_this->m2D0) {
    case 0:
        action_dousa(i_this);
        break;
    case 1:
        action_hane_rakka(i_this);
        break;
    case 2:
        action_uchiwa_dousa(i_this);
        break;
    case 3:
        action_totugeki(i_this);
        break;
    case 4:
        action_kabehari(i_this);
        break;
    case 5:
        action_fly_damage(i_this);
        break;
    case 10:
        action_ground_attack(i_this);
        break;
    case 20:
        action_demo(i_this);
        break;
    }

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    cXyz offset;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = actor->speedF;
    cXyz pos;
    MtxPosition(&offset, &pos);
    actor->speed.x = pos.x;
    actor->speed.z = pos.z;
    actor->speed.y += actor->gravity;
    if (actor->speed.y < -100.0f) {
        actor->speed.y = -100.0f;
    }

    if (i_this->m2CD == 0) {
        if (i_this->m2D0 != 20) {
            if (i_this->m2D0 != 10) {
                body_atari_check(i_this);
            }
            if (i_this->m2D0 != 5 && i_this->m2D0 != 2) {
                wing_ret_set(i_this);
            }
            if (i_this->m2D0 != 2) {
                cLib_addCalcAngleS2(&actor->shape_angle.z, i_this->m436, 1, 0x100);
                s16 zero = 0;
                cLib_addCalcAngleS2(&i_this->m436, zero, 1, 0x100);
            }
        }

        if (i_this->m2EC == 0) {
            s8 roomNo = fopAcM_GetRoomNo(actor);
            u32 mtrlSndId = i_this->mAcch.ChkGroundHit() ? dComIfG_Bgsp()->GetMtrlSndId(i_this->mAcch.m_gnd) : 0;
            i_this->mpMorf->play(&actor->eyePos, mtrlSndId, dComIfGp_getReverb(roomNo));
        }

        switch (i_this->mBrkMode) {
        case 1:
            i_this->mpBrkDamage->play();
            break;
        case 2:
            i_this->mpBrkAtack->play();
            break;
        case 3:
            i_this->mpBrkDead->play();
            break;
        default:
            i_this->mpBrkGm->play();
            break;
        }

        cXyz cylPos = actor->current.pos + i_this->mDrawOffset;
        if (i_this->m2D0 != 20) {
            if (i_this->m2D0 == 10) {
                actor->attention_info.position = i_this->mAtamaPos;
                actor->attention_info.position.y += 50.0f;
                actor->eyePos = i_this->mAtamaPos;
                cylPos = i_this->mMunePos;
            } else {
                actor->attention_info.position = cylPos;
                actor->attention_info.position.y += 100.0f;
                actor->eyePos = cylPos;
            }

            cylPos.y += i_this->m338;
            if (i_this->mAction != 34) {
                i_this->mBodyCyl.SetC(cylPos);
                i_this->mBodyCyl.SetH(i_this->m33C);
                i_this->mBodyCyl.SetR(i_this->m340);
                dComIfG_Ccsp()->Set(&i_this->mBodyCyl);
            } else {
                cylPos = actor->current.pos + i_this->mDrawOffset;
                cylPos.y -= 10.0f;
                i_this->mBodyCyl.SetC(cylPos);
                i_this->mBodyCyl.SetH(60.0f);
                i_this->mBodyCyl.SetR(375.0f);
                dComIfG_Ccsp()->Set(&i_this->mBodyCyl);
            }

            cylPos = actor->current.pos + i_this->mDrawOffset;
            cylPos.y -= 40.0f;
            i_this->mWingCyl.SetC(cylPos);
            i_this->mWingCyl.SetH(60.0f);
            i_this->mWingCyl.SetR(185.0f);
            dComIfG_Ccsp()->Set(&i_this->mWingCyl);
            dComIfG_Ccsp()->SetMass(&i_this->mWingCyl, 3);
            i_this->mWeaponSph.SetC(i_this->mAtamaOffsetPos);
            i_this->mWeaponSph.SetR(55.0f);
            dComIfG_Ccsp()->Set(&i_this->mWeaponSph);
        }

        if (i_this->m44C == 0 && (i_this->mWingBits & 0xF) != 0xF && i_this->mAction != 34) {
            cylPos = actor->current.pos + i_this->mDrawOffset;
            i_this->mTimers[22] += 1000;
            i_this->m408 = 300.0f * cM_scos(i_this->mTimers[22]);
            i_this->m410 = 300.0f * cM_scos(i_this->mTimers[22]);
            cylPos.x += i_this->m408;
            cylPos.y = i_this->mAcch.GetGroundH();
            cylPos.z += i_this->m410;
            i_this->mWindCyl.SetC(cylPos);
            i_this->mWindCyl.SetH(60.0f);
            i_this->mWindCyl.SetR(200.0f);
            dComIfG_Ccsp()->Set(&i_this->mWindCyl);
            if (i_this->m2D0 != 5 && i_this->m2D0 != 2) {
                dComIfG_Ccsp()->SetMass(&i_this->mWindCyl, 3);
            }
        }
    }

    if (i_this->mAction != 34 && i_this->m2D0 != 20) {
        fopAcM_posMove(actor, i_this->mStts.GetCCMoveP());
    } else {
        fopAcM_posMove(actor, NULL);
    }

    BG_check(i_this);
    draw_SUB(i_this);
    return TRUE;
}

/* 00007770-00007778       .text daGM_IsDelete__FP8gm_class */
static BOOL daGM_IsDelete(gm_class*) {
    return TRUE;
}

/* 00007778-000078C4       .text daGM_Delete__FP8gm_class */
static BOOL daGM_Delete(gm_class* i_this) {
    dComIfG_resDeleteDemo(&i_this->mPhs, "GM");
#if VERSION > VERSION_DEMO
    if (i_this->heap != NULL) {
        i_this->mpMorf->stopZelAnime();
    }
#endif
    enemy_fire_remove(&i_this->mEnemyFire);
    if (i_this->health == -128) {
        if (i_this->mSwitchNo != 0xFF && i_this->mSwitchEnable != 0) {
            dComIfGs_onSwitch(i_this->mSwitchNo, fopAcM_GetRoomNo(i_this));
        }
        fopAcM_onActor(i_this);
    }
    if (i_this->mpEmitters[0] != NULL) {
        i_this->mpEmitters[0]->becomeInvalidEmitter();
        i_this->mpEmitters[0] = NULL;
    }
    if (i_this->mpEmitters[1] != NULL) {
        i_this->mpEmitters[1]->becomeInvalidEmitter();
        i_this->mpEmitters[1] = NULL;
    }
    if (i_this->mpEmitters[2] != NULL) {
        i_this->mpEmitters[2]->becomeInvalidEmitter();
        i_this->mpEmitters[2] = NULL;
    }
    if (i_this->mpEmitters[3] != NULL) {
        i_this->mpEmitters[3]->becomeInvalidEmitter();
        i_this->mpEmitters[3] = NULL;
    }
    return TRUE;
}

/* 000078C4-00007E74       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* a_this) {
    gm_class* i_this = (gm_class*)a_this;

    if (i_this->m2CD == 0) {
        i_this->mpMorf = new mDoExt_McaMorf(
            (J3DModelData*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BDL_GM_e),
            NULL, NULL, NULL,
            J3DFrameCtrl::EMode_RESET, 1.0f, 0, -1, 1,
            NULL, 0x80000, 0x37441422
        );
    } else {
        switch (i_this->mWingBits) {
        case 1:
        case 2:
            i_this->mpMorf = new mDoExt_McaMorf(
                (J3DModelData*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BDL_HANESL_e),
                NULL, NULL, NULL,
                J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 0,
                NULL, 0, 0x11020203
            );
            break;
        case 3:
        case 4:
            i_this->mpMorf = new mDoExt_McaMorf(
                (J3DModelData*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BDL_HANEUL_e),
                NULL, NULL, NULL,
                J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 0,
                NULL, 0, 0x11020203
            );
            break;
        }
    }

    if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) {
        return FALSE;
    }

    if (i_this->m2CD == 0) {
        i_this->mpMorf->getModel()->setUserArea((u32)i_this);
        for (u16 i = 0; i < i_this->mpMorf->getModel()->getModelData()->getJointNum(); i++) {
            i_this->mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
        }
    }

    J3DModel* model = i_this->mpMorf->getModel();
    J3DModelData* modelData = model->getModelData();
    if (i_this->m2CD == 0) {
        i_this->mpBrkGm = new mDoExt_brkAnm();
        if (i_this->mpBrkGm == NULL) {
            return FALSE;
        }
        if (i_this->mpBrkGm->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BRK_GM_e),
                TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, FALSE
            ) == 0)
        {
            return FALSE;
        }

        i_this->mpBrkAtack = new mDoExt_brkAnm();
        if (i_this->mpBrkAtack == NULL) {
            return FALSE;
        }
        if (i_this->mpBrkAtack->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BRK_ATACK_e),
                TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, FALSE
            ) == 0)
        {
            return FALSE;
        }

        i_this->mpBrkDamage = new mDoExt_brkAnm();
        if (i_this->mpBrkDamage == NULL) {
            return FALSE;
        }
        if (i_this->mpBrkDamage->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BRK_DAMAGE_e),
                TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
            ) == 0)
        {
            return FALSE;
        }

        i_this->mpBrkDead = new mDoExt_brkAnm();
        if (i_this->mpBrkDead == NULL) {
            return FALSE;
        }
        if (i_this->mpBrkDead->init(
                model->getModelData(),
                (J3DAnmTevRegKey*)dComIfG_getObjectRes("GM", dRes_INDEX_GM_BRK_DEAD_e),
                TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
            ) == 0)
        {
            return FALSE;
        }

        if (!i_this->mInvisibleModel.create(i_this->mpMorf->getModel())) {
            return FALSE;
        }
    }

    i_this->mpShadowModel = mDoExt_J3DModel__create(modelData, 0x20000, 0x11020203);
    if (i_this->mpShadowModel == NULL) {
        return FALSE;
    }
    return TRUE;
}

/* 00007EBC-0000842C       .text daGM_Create__FP10fopAc_ac_c */
static cPhs_State daGM_Create(fopAc_ac_c* a_this) {
    static dCcD_SrcSph weapon_co_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_UNK800,
            /* SrcObjAt  Atp     */ 1,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_VsOther_e,
            /* SrcObjTg  Type    */ 0,
            /* SrcObjTg  SPrm    */ 0,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ dCcG_SE_UNK2,
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
    static dCcD_SrcCyl body_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
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
    static dCcD_SrcCyl wing_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
            /* SrcObjAt  Atp     */ 2,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_VsOther_e,
            /* SrcObjTg  Type    */ 0,
            /* SrcObjTg  SPrm    */ 0,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ dCcG_SE_METAL,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK6,
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
    static dCcD_SrcCyl wind_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_WIND,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_VsOther_e | cCcD_AtSPrm_NoTgHitInfSet_e,
            /* SrcObjTg  Type    */ 0,
            /* SrcObjTg  SPrm    */ 0,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_VsOther_e,
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
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    };
    static f32 fire_sc[] = {
        2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
    };

    gm_class* i_this = (gm_class*)a_this;
#if VERSION > VERSION_DEMO
    fopAcM_ct(a_this, gm_class);
#endif

    cPhs_State phase_state = dComIfG_resLoad(&i_this->mPhs, "GM");
    if (phase_state == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_ct(a_this, gm_class);
#endif
        i_this->mWingBits = fopAcM_GetParam(a_this);
        i_this->m2CD = fopAcM_GetParam(a_this) >> 8;
        i_this->m2CE = fopAcM_GetParam(a_this) >> 16;
        i_this->mSwitchNo = fopAcM_GetParam(a_this) >> 24;
        if (i_this->mWingBits == 0xFF) {
            i_this->mWingBits = 0;
        }
        if (i_this->m2CD == 0xFF) {
            i_this->m2CD = 0;
        }
        if (i_this->m2CE == 0xFF) {
            i_this->m2CE = 0;
        }

        if (i_this->m2CD == 0) {
            i_this->mSwitchEnable = a_this->current.angle.z;
            a_this->current.angle.z = 0;
            a_this->shape_angle.z = 0;
            a_this->current.angle.x = 0;
            a_this->shape_angle.x = 0;
        }

        if (i_this->mSwitchNo != 0xFF) {
            if (dComIfGs_isSwitch(i_this->mSwitchNo, dComIfGp_roomControl_getStayNo())) {
                return cPhs_ERROR_e;
            }
        }

        s16 reg8s9 = REG8_S(9);
        if (reg8s9 != 0) {
            i_this->m2CE = reg8s9 - 1;
        }

        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, 0x7400)) {
            return cPhs_ERROR_e;
        }

        a_this->max_health = 12;
        a_this->health = 12;
        fopAcM_SetMtx(a_this, i_this->mpMorf->getModel()->getBaseTRMtx());
        fopAcM_setCullSizeBox(a_this, -200.0f, -150.0f, -200.0f, 200.0f, 250.0f, 150.0f);

        i_this->mAcch.Set(
            fopAcM_GetPosition_p(a_this), fopAcM_GetOldPosition_p(a_this),
            a_this, 1, &i_this->mAcchCir,
            fopAcM_GetSpeed_p(a_this), NULL, NULL
        );
        i_this->mStts.Init(0x64, 1, a_this);
        i_this->mCorrectionY = 0.0f;
        i_this->m434 = a_this->current.angle.y;
        i_this->mSpawnPos = a_this->current.pos;

        a_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("gmos", 0);
        if (i_this->m2CD == 0) {
            i_this->mBodyCyl.Set(body_co_cyl_src);
            i_this->mBodyCyl.SetStts(&i_this->mStts);
            i_this->mWeaponSph.Set(weapon_co_sph_src);
            i_this->mWeaponSph.SetStts(&i_this->mStts);
            i_this->mWingCyl.Set(wing_co_cyl_src);
            i_this->mWingCyl.SetStts(&i_this->mStts);
            i_this->mWindCyl.Set(wind_co_cyl_src);
            i_this->mWindCyl.SetStts(&i_this->mStts);
            i_this->mWeaponSph.OffAtSetBit();
            i_this->mWeaponSph.OffAtSetBit();
            i_this->mWingCyl.OffAtSetBit();
            i_this->mWingCyl.OffAtSetBit();

            a_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
            a_this->attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 4;

            if (i_this->m2CE == 1) {
                i_this->initBt(120.0f + REG8_F(4), 100.0f + REG8_F(5));
                i_this->setBtAttackData(0.0f, 10.0f, 800.0f + REG8_F(6), 1);
                a_this->attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 3;
                a_this->stealItemLeft = 1;
            } else {
                i_this->initBt(200.0f + REG8_F(4), 100.0f + REG8_F(5));
                i_this->setBtAttackData(0.0f, 10.0f, 880.0f + REG8_F(6), 1);
                a_this->stealItemLeft = 3;
                i_this->mWeaponSph.SetAtSpl(dCcG_At_Spl_UNK5);
            }

            i_this->setBtNowFrame(1000.0f);
            a_this->scale.setall(0.0f);
            i_this->m2D0 = 20;
            i_this->mAction = 200;

            i_this->mEnemyIce.mpActor = a_this;
            i_this->mEnemyIce.mWallRadius = 40.0f;
            i_this->mEnemyIce.mCylHeight = 40.0f;
            i_this->mEnemyIce.m1B0 = 1;
            i_this->mEnemyFire.mpMcaMorf = i_this->mpMorf;
            i_this->mEnemyFire.mpActor = a_this;
            for (int i = 0; i < 10; i++) {
                i_this->mEnemyFire.mFlameJntIdxs[i] = fire_j[i];
                i_this->mEnemyFire.mParticleScale[i] = fire_sc[i];
            }

            draw_SUB(i_this);

            if (i_this->m2CE == 0) {
                a_this->actor_status |= fopAcStts_UNK4000_e;
                a_this->actor_status |= fopAcStts_BOSS_e;
                a_this->actor_status &= ~fopAcStts_SHOWMAP_e;
            } else if (i_this->m2CE == 2) {
                a_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("gmos_f", 0);
                a_this->scale.setall(1.0f);
                i_this->m2D0 = 0;
                i_this->mAction = 0;
            } else if (i_this->m2CE == 1) {
                a_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("gmos_g", 0);
                i_this->mWallH = 60.0f;
                i_this->mWallR = 70.0f;
                a_this->scale.setall(1.0f);
                i_this->mWingBits = 15;
                a_this->max_health = 2;
                a_this->health = 2;
                i_this->m2D0 = 10;
                i_this->mAction = 60;
            }
        } else {
            a_this->attention_info.flags = 0;
            i_this->mWallR = 100.0f;
            if (i_this->mWingBits == 2 || i_this->mWingBits == 4) {
                a_this->shape_angle.y += (s16)0x8000;
            }
            i_this->m2D0 = 1;
            i_this->mAction = 10;
        }
    }

    return phase_state;
}

static actor_method_class l_daGM_Method = {
    (process_method_func)daGM_Create,
    (process_method_func)daGM_Delete,
    (process_method_func)daGM_Execute,
    (process_method_func)daGM_IsDelete,
    (process_method_func)daGM_Draw,
};

actor_process_profile_definition g_profile_GM = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_GM_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(gm_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_GM_e,
    /* Actor SubMtd */ &l_daGM_Method,
    /* Status       */ fopAcStts_SHOWMAP_e | fopAcStts_UNK40000_e | fopAcStts_UNK80000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
