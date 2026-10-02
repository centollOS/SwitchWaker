/**
 * d_a_pt.cpp
 * Enemy - Miniblin
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_pt.h"
#include "c/c_damagereaction.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_sea.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "d/d_particle_name.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera.h"
#include "f_op/f_op_kankyo_mng.h"
#include "JAZelAudio/JAIZelBasic.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_hostIO.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/Pt.h"

static bool hio_set;
static daPt_HIO_c l_HIO;
static cXyz p_check_d[] = {
    cXyz(100.0f, 0.0f, 0.0f),
    cXyz(-100.0f, 0.0f, 0.0f),
    cXyz(0.0f, 100.0f, 0.0f),
    cXyz(0.0f, -100.0f, 0.0f),
    cXyz(0.0f, 0.0f, 100.0f),
    cXyz(0.0f, 0.0f, -100.0f),
};
static cXyz p_off_d[] = {
    cXyz(-3.0f, 0.0f, 0.0f),
    cXyz(3.0f, 0.0f, 0.0f),
    cXyz(0.0f, -3.0f, 0.0f),
    cXyz(0.0f, 3.0f, 0.0f),
    cXyz(0.0f, 0.0f, -3.0f),
    cXyz(0.0f, 0.0f, 3.0f),
};

/* 000000EC-00000110       .text __ct__10daPt_HIO_cFv */
daPt_HIO_c::daPt_HIO_c() {
    mNo = -1;
    m05 = 0;
    m06 = 0;
}

/* 00000110-00000238       .text anm_init__FP8pt_classifUcfi */
void anm_init(pt_class* i_this, int bckFileIdx, float morf, unsigned char loopMode, float speed, int soundFileIdx) {
    if (soundFileIdx >= 0) {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("Pt", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, dComIfG_getObjectRes("Pt", soundFileIdx));
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("Pt", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }
}

/* 00000238-00000360       .text daPt_Draw__FP8pt_class */
static BOOL daPt_Draw(pt_class* i_this) {
    if (i_this->mHide == 0) {
        J3DModel* model = i_this->mpMorf->getModel();
        g_env_light.setLightTevColorType(model, &i_this->tevStr);
        i_this->mpBtp->entry(model->getModelData());
        i_this->mpBrk->entry(model->getModelData());
        i_this->mpMorf->entryDL();

        if (i_this->m326 != 0) {
#if VERSION == VERSION_DEMO
            dComIfGd_setSimpleShadow2(&i_this->current.pos, i_this->mAcch.GetGroundH(), 50.0f, i_this->mAcch.m_gnd);
#else
            cXyz shadow_pos(i_this->current.pos.x, i_this->current.pos.y + 100.0f, i_this->current.pos.z);
            i_this->mShadowId = dComIfGd_setShadow(
                i_this->mShadowId,
                1,
                model,
                &shadow_pos,
                400.0f,
                50.0f,
                i_this->current.pos.y,
                i_this->mAcch.GetGroundH(),
                i_this->mAcch.m_gnd,
                &i_this->tevStr,
                0,
                1.0f,
                dDlst_shadowControl_c::getSimpleTex()
            );
#endif
            i_this->m326 = 0;
        }
    }

    dSnap_RegistFig(DSNAP_TYPE_UNKAB, i_this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 0000039C-00000484       .text smoke_set__FP8pt_classSc */
void smoke_set(pt_class* i_this, signed char param_2) {
    fopAc_ac_c* actor = i_this;
    if (i_this->mSmokeFlag == 0) {
        i_this->mSmokeCb.remove();
        JPABaseEmitter* emitter = dComIfGp_particle_setToon(
            dPa_name::ID_AK_JT_ELEMENTSMOKE00,
            &actor->current.pos,
            &actor->shape_angle,
            NULL,
            0xB9,
            &i_this->mSmokeCb,
            fopAcM_GetRoomNo(actor)
        );
        if (emitter != NULL) {
            emitter->setRate(3.0f);
            emitter->setSpread(0.5f);
            JGeometry::TVec3<f32> scale;
            scale.x = scale.y = scale.z = 1.0f;
            emitter->setGlobalScale(scale);
#if VERSION == VERSION_DEMO
            scale.x = scale.y = scale.z = 1.5f + REG0_F(16);
#else
            scale.x = scale.y = scale.z = 1.5f;
#endif
            emitter->setGlobalParticleScale(scale);
            i_this->mSmokeFlag = param_2;
        }
    }
}

/* 00000484-0000093C       .text damage_check__FP8pt_class */
void damage_check(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    CcAtInfo atInfo;

    if (i_this->m30E == 0) {
        i_this->mStts.Move();

        if (i_this->mAtSph.ChkAtHit()) {
            fopAc_ac_c* hit_actor = i_this->mAtSph.GetAtHitObj()->GetAc();
            if (hit_actor != NULL) {
                if (fopAcM_GetName(hit_actor) == fpcNm_PLAYER_e) {
                    if (player->checkPlayerGuard()) {
                        i_this->m324 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
#if VERSION == VERSION_DEMO
                        i_this->m320 = 40.0f + REG0_F(18);
#else
                        i_this->m320 = 40.0f;
#endif
                    } else {
                        i_this->m2D2 = 3;
                        i_this->mMode = 2;
                        i_this->m30E = 6;
                        return;
                    }
                }
            }
        }

#if VERSION > VERSION_DEMO
        if (player->checkHammerQuake()) {
            cXyz delta = player->current.pos - actor->current.pos;
            if (delta.abs() < 350.0f) {
                i_this->m2D2 = 3;
                i_this->mMode = 0;
                i_this->mTimers[1] = (s16)(20.0f + cM_rndF(30.0f));
                smoke_set(i_this, 3);
                i_this->m30E = 10;
                actor->speed.y = 350.0f;
                return;
            }
        }
#endif

        if (i_this->mSph.ChkTgHit()) {
            i_this->m30E = 6;
            atInfo.mpObj = i_this->mSph.GetTgHitObj();
            atInfo.pParticlePos = i_this->mSph.GetTgHitPosP();

            if (atInfo.mpObj->ChkAtType(AT_TYPE_LIGHT_ARROW)) {
                i_this->mEnemyIce.mLightShrinkTimer = 1;
                enemy_fire_remove(&i_this->mEnemyFire);
                i_this->mSmokeFlag = 0;
                i_this->mSmokeCb.remove();
                return;
            }

            if (atInfo.mpObj->GetAtType() & (AT_TYPE_FIRE | AT_TYPE_FIRE_ARROW)) {
                i_this->mEnemyFire.mFireDuration = DEMO_SELECT(REG0_S(2) + 100, 100);
                i_this->m30E = 50;
            }

            if (atInfo.mpObj->ChkAtType(AT_TYPE_WIND)) {
                i_this->m324 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
                smoke_set(i_this, 5);
#if VERSION == VERSION_DEMO
                i_this->m320 = 100.0f;
                i_this->m2D2 = 3;
                i_this->mMode = 0;
#else
                i_this->m320 = 90.0f + cM_rndF(30.0f);
                i_this->m310 = (s16)cM_rndFX(2000.0f);
                i_this->m2D2 = 3;
                i_this->mMode = 10;
#endif
                smoke_set(i_this, 10);
                fopAcM_monsSeStart(actor, JA_SE_CV_PT_DAMAGE, 0);
                return;
            }

            cc_at_check(actor, &atInfo);
            if (atInfo.mResultingAttackType == 9) {
                if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
                    i_this->m2D2 = 5;
                    smoke_set(i_this, 30);
                } else {
                    i_this->m2D2 = 4;
                }
                i_this->mMode = 0;
                fopAcM_monsSeStart(actor, JA_SE_CV_PT_DIE, 0);
                i_this->m30E = 50;
                return;
            }

            i_this->m324 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            smoke_set(i_this, 10);
            if (actor->health <= 0) {
                i_this->m30E = 50;
                fopAcM_monsSeStart(actor, JA_SE_CV_PT_DAMAGE, 0);
#if VERSION == VERSION_DEMO
                i_this->mTimers[1] = (s16)(REG0_F(5) + (60.0f + cM_rndF(60.0f)));
#else
                i_this->mTimers[1] = (s16)(60.0f + cM_rndF(60.0f));
#endif
                i_this->m320 = 80.0f;
            } else {
                fopAcM_monsSeStart(actor, JA_SE_CV_PT_DIE, 0);
                i_this->m320 = 110.0f;
            }
            i_this->m2D2 = 2;
            i_this->mMode = 1;
        }
    }
}

/* 0000093C-00000D74       .text get_z_ang__FP8pt_class */
s16 get_z_ang(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    cXyz src;
    cXyz p1;
    cXyz p2;
    dBgS_LinChk lin_chk;

    MtxTrans(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z, 0);
    cMtx_YrotM(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);

    s16 z = 0;
    for (int i = 0; i < 16; i++, z += 0x1000) {
        MtxPush();
        mDoMtx_ZrotM(*calc_mtx, z);
        src.x = 10.0f;
        src.y = 2.0f;
        src.z = 0.0f;
        MtxPosition(&src, &p1);
        src.y = -5.0f;
        MtxPosition(&src, &p2);
        lin_chk.Set(&p1, &p2, actor);
        if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
            src.x = -10.0f;
            src.y = 2.0f;
            MtxPosition(&src, &p1);
            src.y = -5.0f;
            MtxPosition(&src, &p2);
            lin_chk.Set(&p1, &p2, actor);
            if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
                MtxPull();
                return z;
            }
        }
        MtxPull();
    }
    return 0xdcf;
}

/* 000011AC-000018EC       .text next_pos_set__FP8pt_class */
BOOL next_pos_set(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    dBgS_LinChk lin_chk;
    cXyz src;
    cXyz p1;
    cXyz p0;
    cXyz off;

    if (actor->current.angle.x < 0x2000 && actor->current.angle.x > -0x2000) {
#if VERSION == VERSION_DEMO
        s16 ang = fopAcM_searchPlayerAngleY(actor);
#else
        s16 ang = i_this->m2FC;
#endif
        if (i_this->mTimers[1] != 0 ||
            (i_this->mEnableSpawnSwitch != 0xFF && !dComIfGs_isSwitch(i_this->mEnableSpawnSwitch, dStage_roomControl_c::getStayNo())) ||
            (i_this->mDisableRespawnSwitch != 0xFF && dComIfGs_isSwitch(i_this->mDisableRespawnSwitch, dStage_roomControl_c::getStayNo())))
        {
            ang += 0x8000;
        }
        ang += (s16)cM_rndFX(4000.0f);
        mDoMtx_YrotS(*calc_mtx, ang);
        cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    } else {
        s16 ang = actor->current.angle.y + (s16)cM_rndFX(4000.0f);
        mDoMtx_YrotS(*calc_mtx, ang);
        cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    }
    cMtx_ZrotM(*calc_mtx, actor->current.angle.z);

    src.x = 0.0f;
#if VERSION == VERSION_DEMO
    src.y = 200.0f + REG6_F(0);
    src.z = 200.0f + REG6_F(1);
#else
    src.y = 200.0f;
    src.z = 200.0f;
#endif
    MtxPosition(&src, &p0);
    p0 += actor->current.pos;
    lin_chk.Set(&actor->current.pos, &p0, actor);
    if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
        i_this->mNextPos = lin_chk.GetCross();
        src.y = -2.0f;
        src.z = -2.0f;
        MtxPosition(&src, &off);
        i_this->mNextPos += off;
        return TRUE;
    }

#if VERSION == VERSION_DEMO
    src.y = -400.0f + REG6_F(2);
    src.z = 200.0f + REG6_F(3);
#else
    src.y = -400.0f;
    src.z = 200.0f;
#endif
    MtxPosition(&src, &p1);
    p1 += actor->current.pos;
    lin_chk.Set(&p0, &p1, actor);
    if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
        i_this->mNextPos = lin_chk.GetCross();
        src.y = 2.0f;
        src.z = 0.0f;
        MtxPosition(&src, &off);
        i_this->mNextPos += off;
        return TRUE;
    }

#if VERSION == VERSION_DEMO
    src.y = -50.0f + REG6_F(2);
    src.z = -50.0f + REG6_F(3);
#else
    src.y = -50.0f;
    src.z = -50.0f;
#endif
    MtxPosition(&src, &p0);
    p0 += actor->current.pos;
    lin_chk.Set(&p1, &p0, actor);
    if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
        i_this->mNextPos = lin_chk.GetCross();
        src.y = 0.0f;
        src.z = 2.0f;
        MtxPosition(&src, &off);
        i_this->mNextPos += off;
        return TRUE;
    }

    return FALSE;
}

/* 000018EC-00001DC0       .text pt_move__FP8pt_class */
void pt_move(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif
    cXyz delta;
#if VERSION == VERSION_DEMO
    cXyz offset(0.0f, 0.0f, 20.0f + REG0_F(0));
#else
    cXyz offset;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = 20.0f;
#endif

    switch (i_this->mMode) {
    case 0:
        if (next_pos_set(i_this)) {
            i_this->mMode = 1;
            delta = i_this->mNextPos - actor->current.pos;
#if VERSION == VERSION_DEMO
            i_this->m31C = (0.1f + REG0_F(2)) * delta.abs();
#else
            i_this->m31C = 0.1f * delta.abs();
#endif
            if (i_this->m31C > 50.0f) {
                i_this->m31C = 50.0f;
            }
        } else {
            i_this->m2D2 = 2;
            i_this->mMode = 0;
            return;
        }
        break;
    case 1: {
        delta = i_this->mNextPos - actor->current.pos;
        actor->current.angle.y = cM_atan2s(delta.x, delta.z);
        actor->current.angle.x = -cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z));
        s16 z = get_z_ang(i_this);
        if (z != 0xdcf) {
            actor->current.angle.z = z;
        }
        if (delta.abs() < 1.5f * offset.z) {
            if (cM_rndF(1.0f) < 0.02f && actor->current.angle.x < 0x1000 && actor->current.angle.x > -0x1000) {
                i_this->m2D2 = 3;
                i_this->mMode = 0;
                smoke_set(i_this, 3);
            } else {
                i_this->mMode = 2;
#if VERSION == VERSION_DEMO
                i_this->mTimers[0] = (s16)(REG0_F(3) + (2.0f + cM_rndF(5.0f)));
#else
                i_this->mTimers[0] = (s16)(2.0f + cM_rndF(5.0f));
#endif
                anm_init(i_this, dRes_INDEX_PT_BCK_WAIT_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                fopAcM_seStart(actor, JA_SE_CM_PT_JUMP, 0);
            }
        } else {
            cMtx_YrotS(*calc_mtx, actor->current.angle.y);
            cMtx_XrotM(*calc_mtx, actor->current.angle.x);
            MtxPosition(&offset, &actor->speed);
            actor->current.pos += actor->speed;
        }
        break;
    }
    case 2: {
        cLib_addCalc2(&actor->current.pos.x, i_this->mNextPos.x, 1.0f, std::fabsf(actor->speed.x));
        cLib_addCalc2(&actor->current.pos.y, i_this->mNextPos.y, 1.0f, std::fabsf(actor->speed.y));
        cLib_addCalc2(&actor->current.pos.z, i_this->mNextPos.z, 1.0f, std::fabsf(actor->speed.z));
        s16 z = get_z_ang(i_this);
        if (z != 0xdcf) {
            actor->current.angle.z = z;
        }
        if (i_this->mTimers[0] == 0) {
            i_this->mMode = 0;
            anm_init(i_this, dRes_INDEX_PT_BCK_JUMP_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_PT_JUMP, 0);
        }
        break;
    }
    }

#if VERSION == VERSION_DEMO
    if (actor->current.pos.y - player->current.pos.y > -50.0f && fopAcM_searchActorDistanceXZ(actor, dComIfGp_getPlayer(0)) < 500.0f + REG0_F(13)) {
#else
    if (actor->current.pos.y - i_this->mpEsaPos->y > -50.0f && i_this->m304 < 500.0f) {
#endif
        i_this->m2D2 = 2;
        i_this->mMode = 0;
    }
}

/* 00001DC0-000020D4       .text view_check__FP8pt_class */
BOOL view_check(pt_class* i_this) {
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    dBgS_LinChk lin_chk;
    lin_chk.Set(&i_this->eyePos, &player->eyePos, i_this);
#else
    cXyz pos(*i_this->mpEsaPos);
    pos.y += 100.0f;

    dBgS_LinChk lin_chk;
    lin_chk.Set(&i_this->eyePos, &pos, i_this);
#endif
    if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
        return TRUE;
    }
    return FALSE;
}

/* 000020D4-00002528       .text pt_attack__FP8pt_class */
#if VERSION == VERSION_DEMO
void pt_attack(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    s16 angle = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    f32 dist = fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0));
    cXyz offset;
    cXyz move;

    if (i_this->mTimers[1] != 0) {
        angle += 0x8000;
    }
    cLib_addCalcAngleS2(&actor->current.angle.y, angle, 4, 0x800);
    cLib_addCalcAngleS2(&actor->current.angle.x, 0, 2, 0x1000);
    cLib_addCalcAngleS2(&actor->current.angle.z, 0, 2, 0x1000);

    if (i_this->mAcch.ChkGroundHit() && i_this->m314 < -100.0f) {
        i_this->m2D2 = 3;
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->mMode = 1;
        } else {
            i_this->mMode = 0;
        }
        smoke_set(i_this, 5);
        return;
    }

    switch (i_this->mMode) {
    case 0:
        if (i_this->mTimers[0] == 0) {
            i_this->mMode = 1;
            if (i_this->mAcch.ChkGroundHit()) {
                anm_init(i_this, dRes_INDEX_PT_BCK_JUMP_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                mDoMtx_YrotS(*calc_mtx, (s16)(angle + (s16)cM_rndFX(4000.0f)));
                offset.x = 0.0f;
                offset.y = REG0_F(7) + (50.0f + cM_rndFX(10.0f));
                offset.z = 30.0f + REG0_F(8);
                MtxPosition(&offset, &actor->speed);
                fopAcM_monsSeStart(actor, JA_SE_CV_PT_JUMP, 0);
            }
            break;
        }
        actor->speed.setall(0.0f);
        break;
    case 1:
        if (i_this->mAcch.ChkGroundHit()) {
            if (cM_rndF(1.0f) < 0.02f) {
                i_this->m2D2 = 3;
                i_this->mMode = 0;
                smoke_set(i_this, 3);
            } else {
                i_this->mMode = 2;
                anm_init(i_this, dRes_INDEX_PT_BCK_BWALK_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                actor->speed.setall(0.0f);
                fopAcM_seStart(actor, JA_SE_CM_PT_JUMP, 0);
            }
        }
        break;
    case 2:
        cMtx_YrotS(*calc_mtx, actor->current.angle.y);
        offset.x = 0.0f;
        offset.y = 0.0f;
        offset.z = 20.0f + REG0_F(9);
        MtxPosition(&offset, &move);
        actor->speed.x = move.x;
        actor->speed.z = move.z;
        if (dist < 300.0f + REG0_F(10) && !view_check(i_this)) {
            i_this->mTimers[2] = i_this->m2CC * 3 + 10;
            i_this->mMode = 3;
        } else if (dist > 450.0f + REG0_F(11)) {
            i_this->mMode = 0;
            i_this->mTimers[0] = (s16)(REG0_F(3) + (2.0f + cM_rndF(10.0f)));
            anm_init(i_this, dRes_INDEX_PT_BCK_WAIT_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        break;
    case 3:
        if (i_this->mTimers[2] == 1) {
            anm_init(i_this, dRes_INDEX_PT_BCK_ATACK_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_PT_ATTACK, 0);
            fopAcM_seStart(actor, JA_SE_CM_PT_ATTACK, 0);
        }
        actor->speed.x *= 0.8f + REG0_F(14);
        actor->speed.z *= 0.8f + REG0_F(14);
        if (i_this->mTimers[2] == 0) {
            f32 frame = i_this->mpMorf->getFrame();
            if (frame >= 11.0f && frame <= 14.0f) {
                i_this->m327 = 1;
            }
        }
        if (i_this->mpMorf->isStop()) {
            i_this->mMode = 1;
        }
        break;
    }

    actor->current.pos += actor->speed;
    actor->speed.y -= 7.0f + REG0_F(12);
    if (actor->speed.y < -120.0f) {
        actor->speed.y = -120.0f;
    }
    i_this->m326 = 1;
    if (i_this->mAcch.ChkGroundHit() && fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) > 700.0f) {
        i_this->m2D2 = 1;
        i_this->mMode = 0;
    }
}
#else
void pt_attack(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    s16 angle = i_this->m2FC;
    f32 dist = i_this->m300;

    if (i_this->mTimers[1] != 0) {
        angle += 0x8000;
    }
    cLib_addCalcAngleS2(&actor->current.angle.y, angle, 4, 0x800);
    cLib_addCalcAngleS2(&actor->current.angle.x, 0, 2, 0x1000);
    cLib_addCalcAngleS2(&actor->current.angle.z, 0, 2, 0x1000);

    if (i_this->mAcch.ChkGroundHit() && i_this->m314 < -100.0f) {
        i_this->m2D2 = 3;
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->mMode = 1;
        } else {
            i_this->mMode = 0;
        }
        smoke_set(i_this, 5);
        return;
    }

    switch (i_this->mMode) {
    case 0:
        if (i_this->mTimers[0] == 0) {
            i_this->mMode = 1;
            break;
        }
        actor->speed.setall(0.0f);
        break;
    case 1:
        if (i_this->mAcch.ChkGroundHit()) {
            if (cM_rndF(1.0f) < 0.02f) {
                i_this->m2D2 = 3;
                i_this->mMode = 0;
                smoke_set(i_this, 3);
            } else {
                i_this->mMode = 2;
                anm_init(i_this, dRes_INDEX_PT_BCK_BWALK_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
                actor->speed.setall(0.0f);
                actor->speedF = 0.0f;
                fopAcM_seStart(actor, JA_SE_CM_PT_JUMP, 0);
            }
        }
        break;
    case 2:
        cLib_addCalc2(&actor->speedF, 20.0f, 1.0f, 4.0f);
        mDoMtx_YrotS(*calc_mtx, actor->current.angle.y);
        {
            cXyz offset;
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = actor->speedF;
            cXyz move;
            MtxPosition(&offset, &move);
            actor->speed.x = move.x;
            actor->speed.z = move.z;
        }
        if (dist < 300.0f) {
            if (!view_check(i_this)) {
                i_this->mTimers[2] = i_this->m2CC * 3 + 10;
                i_this->mMode = 3;
                anm_init(i_this, dRes_INDEX_PT_BCK_WAIT_e, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            }
        }
        break;
    case 3:
        if (i_this->mTimers[2] == 1) {
            anm_init(i_this, dRes_INDEX_PT_BCK_ATACK_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_PT_ATTACK, 0);
            fopAcM_seStart(actor, JA_SE_CM_PT_ATTACK, 0);
        }
        actor->speed.x *= 0.8f;
        actor->speed.z *= 0.8f;
        if (i_this->mTimers[2] == 0) {
            f32 frame = i_this->mpMorf->getFrame();
            if (frame >= 11.0f && frame <= 14.0f) {
                i_this->m327 = 1;
            }
        }
        if (i_this->mpMorf->isStop()) {
            i_this->mMode = 1;
        }
        break;
    }

    actor->current.pos += actor->speed;
    actor->speed.y -= 7.0f;
    if (actor->speed.y < -120.0f) {
        actor->speed.y = -120.0f;
    }
    i_this->m326 = 1;
    if (i_this->mAcch.ChkGroundHit() && i_this->m300 > 700.0f) {
        i_this->m2D2 = 1;
        i_this->mMode = 0;
    }
}
#endif

/* 00002528-00002AC8       .text pt_wait__FP8pt_class */
void pt_wait(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    camera_process_class* const camera = dComIfGp_getCamera(0);
    dBgS_LinChk lin_chk;
    cXyz delta;
    cXyz pos;
    s8 unseen = 0;

    i_this->m30E = 6;
    fopAcM_OffStatus(actor, 0);
    actor->attention_info.flags = 0;

    if (i_this->mInitialSpawnDelay != 0) {
        i_this->mInitialSpawnDelay--;
        return;
    }

    if (l_HIO.m06 != 0) {
        if (i_this->mEnableSpawnSwitch != 0xFF) {
            dComIfGs_onSwitch(i_this->mEnableSpawnSwitch, dStage_roomControl_c::getStayNo());
        }
    }

    if (i_this->mEnableSpawnSwitch != 0xFF) {
        if (!dComIfGs_isSwitch(i_this->mEnableSpawnSwitch, dStage_roomControl_c::getStayNo())) {
            goto switch_fail;
        }
    }
    if (i_this->mDisableRespawnSwitch == 0xFF) {
        goto do_wait;
    }
    if (!dComIfGs_isSwitch(i_this->mDisableRespawnSwitch, dStage_roomControl_c::getStayNo())) {
        goto do_wait;
    }

switch_fail:
    return;

do_wait:
    if (fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0)) < 100.0f * i_this->mNoticeRange) {
        pos = actor->current.pos;
        pos.y += 100.0f;
        lin_chk.Set(&camera->view.mLookat.mEye, &pos, actor);
        if (dComIfG_Bgsp()->LineCross(&lin_chk)) {
            unseen = 1;
        } else {
            delta = camera->view.mLookat.mCenter - camera->view.mLookat.mEye;
            s16 ang = cM_atan2s(delta.x, delta.z);
            delta = pos - camera->view.mLookat.mEye;
            cMtx_YrotS(*calc_mtx, -ang);
            MtxPosition(&delta, &pos);
            if (pos.z < 0.0f) {
                unseen = 1;
            }
        }

        if (i_this->mHide == 0 || unseen != 0) {
            fopAcM_OnStatus(actor, 0x36);
            actor->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
            i_this->m2D2 = 1;
            i_this->mMode = 0;
            i_this->mHide = 0;
        }
    }
}

/* 00002AC8-00002F30       .text pt_koke__FP8pt_class */
void pt_koke(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;

    cLib_addCalcAngleS2(&actor->current.angle.x, 0, 2, 0x2000);
    cLib_addCalcAngleS2(&actor->current.angle.z, 0, 2, 0x2000);

    switch (i_this->mMode) {
    case 0:
        i_this->mMode = 4;
        anm_init(i_this, dRes_INDEX_PT_BCK_KOKE_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mTimers[0] = 0;
        fopAcM_monsSeStart(actor, JA_SE_CV_PT_TUMBLE, 0);
        break;
    case 1:
        i_this->mMode = 5;
        anm_init(i_this, dRes_INDEX_PT_BCK_SIRIMOTI_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->mTimers[0] = 0;
        fopAcM_monsSeStart(actor, JA_SE_CV_PT_TUMBLE, 0);
        fopAcM_seStart(actor, JA_SE_CM_PT_TUMBLE, 0);
        break;
    case 2:
        i_this->mMode = 5;
        anm_init(i_this, dRes_INDEX_PT_BCK_HAPPY_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        i_this->mTimers[0] = 20.0f + cM_rndF(20.0f);
        fopAcM_monsSeStart(actor, JA_SE_CV_PT_HAPPY, 0);
        break;
    case 4:
        if ((s32)i_this->mpMorf->getFrame() == 6) {
            fopAcM_seStart(actor, JA_SE_CM_PT_TUMBLE, 0);
        }
        // fallthrough
    case 5:
#if VERSION == VERSION_DEMO
        if (i_this->mpMorf->isStop() || i_this->mTimers[0] == 1) {
            i_this->m2D2 = 1;
            i_this->mMode = 0;
            anm_init(i_this, dRes_INDEX_PT_BCK_JUMP_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        }
        break;
#else
        if (i_this->mTimers[1] == 0) {
            if (i_this->mpMorf->isStop() || i_this->mTimers[0] == 1) {
                i_this->m2D2 = 1;
                i_this->mMode = 0;
                anm_init(i_this, dRes_INDEX_PT_BCK_JUMP_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            }
        }
        break;
    case 10:
        i_this->mMode = 11;
        anm_init(i_this, dRes_INDEX_PT_BCK_KOKE_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_PT_TUMBLE, 0);
        break;
    case 11:
        actor->current.angle.y += i_this->m310;
        if ((s32)i_this->mpMorf->getFrame() == 6) {
            fopAcM_seStart(actor, JA_SE_CM_PT_TUMBLE, 0);
        }
        if (i_this->m320 < 0.05f) {
            i_this->mMode = 12;
            i_this->mTimers[0] = cM_rndF(30.0f);
        }
        break;
    case 12:
        if (i_this->mTimers[0] == 0) {
            i_this->m2D2 = 1;
            i_this->mMode = 0;
            anm_init(i_this, dRes_INDEX_PT_BCK_JUMP_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        }
        break;
#endif
    }

    i_this->m326 = 1;
#if VERSION > VERSION_DEMO
    actor->current.pos.y += actor->speed.y;
    actor->speed.y -= 7.0f;
#endif
}

/* 00002F30-00003028       .text pt_ples__FP8pt_class */
BOOL pt_ples(pt_class* i_this) {
    i_this->m30E = 5;
    i_this->current.angle.z = 0;
    i_this->current.angle.x = 0;

    switch (i_this->mMode) {
    case 0:
        i_this->mTimers[0] = 0x23;
        i_this->mMode = 1;
        anm_init(i_this, dRes_INDEX_PT_BCK_WAIT_e, 1.0f, J3DFrameCtrl::EMode_NONE, 5.0f, -1);
        // fallthrough
    case 1:
        cLib_addCalc2(&i_this->scale.y, 0.1f, 1.0f, 0.5f);
        cLib_addCalc2(&i_this->scale.x, 1.3f, 0.8f, 0.5f);
        if (i_this->mTimers[0] == 0) {
            return TRUE;
        }
        break;
    }

    i_this->m326 = 1;
#if VERSION > VERSION_DEMO
    i_this->current.pos.y += i_this->speed.y;
    i_this->speed.y -= 7.0f;
#endif
    return FALSE;
}

/* 00003028-000031DC       .text pt_bat__FP8pt_class */
BOOL pt_bat(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    i_this->m30E = 5;

    switch (i_this->mMode) {
    case 0:
        i_this->mTimers[0] = 0x96;
        i_this->mMode = 1;
        anm_init(i_this, dRes_INDEX_PT_BCK_WAIT_e, 1.0f, J3DFrameCtrl::EMode_NONE, 5.0f, -1);
        {
            s16 angle = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            s16 rnd = cM_rndFX(6000.0f);
            i_this->m324 = rnd + angle + 0x8000;
            cMtx_YrotS(*calc_mtx, i_this->m324);
            cXyz offset;
            offset.x = 0.0f;
            offset.y = 50.0f + cM_rndF(20.0f);
            offset.z = 100.0f;
            MtxPosition(&offset, &actor->speed);
            actor->current.angle.y = (s16)cM_rndFX(32768.0f);
        }
        // fallthrough
    case 1:
        actor->current.angle.y += 0x400;
        actor->current.angle.x += 0x300;
        if (i_this->mTimers[0] == 0) {
            return TRUE;
        }
        break;
    }

    i_this->m326 = 1;
    actor->current.pos += actor->speed;
    actor->speed.y -= 5.0f;
    if (actor->speed.y < -120.0f) {
        actor->speed.y = -120.0f;
    }

    if (i_this->mAcch.ChkWallHit() || (actor->speed.y < 0.0f && i_this->mAcch.ChkGroundHit())) {
        return TRUE;
    }
    return FALSE;
}

/* 000031DC-000036C0       .text water_check__FP8pt_class */
BOOL water_check(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    dBgS_ObjGndChk_Spl gnd_chk;
    cXyz pillar_pos;

    Vec pos;
    pos.x = actor->current.pos.x;
    pos.y = actor->current.pos.y;
    pos.z = actor->current.pos.z;
    pos.y += 500.0f;
    gnd_chk.SetPos(&pos);
    f32 cross = dComIfG_Bgsp()->GroundCross(&gnd_chk);
    if (cross != -G_CM3D_F_INF && actor->current.pos.y <= cross) {
        return TRUE;
    }

    if (daSea_ChkArea(actor->current.pos.x, actor->current.pos.z)) {
        f32 wave = daSea_calcWave(actor->current.pos.x, actor->current.pos.z) - 30.0f;
        if (actor->current.pos.y <= wave) {
            actor->current.pos.y = wave;
            pillar_pos = actor->current.pos;
            pillar_pos.y = wave;
#if VERSION == VERSION_DEMO
            fopKyM_createWpillar(&pillar_pos, 1.0f + REG7_F(9), 1.0f + REG7_F(10), 0);
#else
            fopKyM_createWpillar(&pillar_pos, 1.0f, 1.0f, 0);
#endif
            fopAcM_seStart(actor, JA_SE_OBJ_FALL_WATER_S, 0);
            return TRUE;
        }
    }

    return FALSE;
}

#if VERSION > VERSION_DEMO
/* 00003B00-00003B4C       .text esa_s_sub__FPvPv */
void* esa_s_sub(void* i_actor, void*) {
    if (fopAcM_IsActor(i_actor) && fopAcM_GetName(i_actor) == fpcNm_ESA_e) {
        return i_actor;
    }
    return NULL;
}
#endif

/* 00003B4C-00003F5C       .text action__FP8pt_class */
void action(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    cXyz delta;

#if VERSION > VERSION_DEMO
    fopAc_ac_c* esa = (fopAc_ac_c*)fpcM_Search(esa_s_sub, i_this);
    if (esa != NULL) {
        i_this->mpEsaPos = &esa->current.pos;
        delta = esa->current.pos - actor->current.pos;
        i_this->m300 = delta.abs();
        delta.y = 0.0f;
        i_this->m304 = delta.abs();
        i_this->m2FC = cM_atan2s(delta.x, delta.z);
    } else {
        i_this->mpEsaPos = &dComIfGp_getPlayer(0)->current.pos;
        i_this->m300 = fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0));
        i_this->m304 = fopAcM_searchActorDistanceXZ(actor, dComIfGp_getPlayer(0));
        i_this->m2FC = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
    }
#endif

    s32 dead = 0;
    switch (i_this->m2D2) {
    case 0:
        pt_wait(i_this);
        break;
    case 1:
        pt_move(i_this);
        break;
    case 2:
        pt_attack(i_this);
        break;
    case 3:
        pt_koke(i_this);
        break;
    case 4:
        dead = pt_ples(i_this);
        break;
    case 5:
        dead = pt_bat(i_this);
        break;
    }

    damage_check(i_this);
    dead += water_check(i_this);

    if (i_this->m320 > 0.01f) {
        delta.x = 0.0f;
        delta.y = 0.0f;
        delta.z = -i_this->m320;
        cMtx_YrotS(*calc_mtx, i_this->m324);
        cXyz move;
        MtxPosition(&delta, &move);
        actor->current.pos += move;
#if VERSION == VERSION_DEMO
        cLib_addCalc0(&i_this->m320, 1.0f, 7.0f + REG8_F(12));
#else
        cLib_addCalc0(&i_this->m320, 1.0f, 7.0f);
#endif

        if (actor->health <= 0) {
            if (i_this->mAcch.ChkWallHit() || i_this->m320 < 0.1f) {
                dead++;
            }
        }
        i_this->m326 = 1;
    }

    cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 4, 0x2000);
    cLib_addCalcAngleS2(&actor->shape_angle.x, actor->current.angle.x, 4, 0x1000);
    cLib_addCalcAngleS2(&actor->shape_angle.z, actor->current.angle.z, 4, 0x1000);

    i_this->m318 += i_this->m31C;
    i_this->m31C -= 5.0f;
    if (i_this->m318 < 0.0f) {
        i_this->m318 = 0.0f;
    }

    if (i_this->mSmokeFlag != 0) {
        i_this->mSmokeFlag--;
        if (i_this->mSmokeFlag == 0) {
            i_this->mSmokeCb.remove();
        }
    }

    if (dead != 0) {
        fopAcM_createDisappear(actor, &actor->eyePos, 5, daDisItem_UNK4_e, 0xFF);
        fopAcM_delete(actor);
        fopAcM_onActor(actor);
        if (i_this->mBehaviorType == 0) {
            i_this->mDoRespawn = 1;
        } else if (i_this->mDisableRespawnSwitch != 0xFF) {
            dComIfGs_onSwitch(i_this->mDisableRespawnSwitch, dStage_roomControl_c::getStayNo());
        }
    }
}

/* 00003F5C-000043C8       .text daPt_Execute__FP8pt_class */
static BOOL daPt_Execute(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
    cXyz offset(0.0f, 0.0f, 0.0f);

    if (enemy_ice(&i_this->mEnemyIce)) {
        i_this->mpMorf->setPlayMode(J3DFrameCtrl::EMode_NONE);
        i_this->mpMorf->setPlaySpeed(3.0f);
        i_this->mpMorf->play(&actor->eyePos, 0, 0);
        J3DModel* model = i_this->mpMorf->getModel();
        model->setBaseTRMtx(mDoMtx_stack_c::get());
        i_this->mpMorf->calc();
        return TRUE;
    }

    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &actor->current.pos, &actor->tevStr);

    i_this->m2D0++;
    if ((i_this->m2D0 & 0x1F) == 0) {
        if (actor->current.pos.y - actor->home.pos.y < -3000.0f) {
            fopAcM_delete(actor);
            fopAcM_onActor(actor);
            if (i_this->mBehaviorType == 0) {
                i_this->mDoRespawn = 1;
            } else if (i_this->mDisableRespawnSwitch != 0xFF) {
                dComIfGs_onSwitch(i_this->mDisableRespawnSwitch, dStage_roomControl_c::getStayNo());
            }
            return TRUE;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (i_this->mTimers[i] != 0) {
            i_this->mTimers[i]--;
        }
    }
    if (i_this->m30E != 0) {
        i_this->m30E--;
    }

    if (l_HIO.m05 == 0) {
        action(i_this);
        if (i_this->mHide != 0) {
            return TRUE;
        }

        if (i_this->m326 != 0) {
            cXyz* cc_move_p = i_this->mStts.GetCCMoveP();
            if (cc_move_p != NULL) {
                actor->current.pos.x += cc_move_p->x;
                actor->current.pos.y += cc_move_p->y;
                actor->current.pos.z += cc_move_p->z;
            }
            i_this->m314 = actor->speed.y;
            i_this->mAcch.CrrPos(*dComIfG_Bgsp());
        }

        mDoMtx_stack_c::transS(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z);
        if (actor->scale.y < 0.9f) {
            mDoMtx_stack_c::scaleM(actor->scale.x, actor->scale.y, actor->scale.x);
        }
        mDoMtx_stack_c::YrotM(actor->shape_angle.y);
        mDoMtx_stack_c::XrotM(actor->shape_angle.x);
        mDoMtx_stack_c::ZrotM(actor->shape_angle.z);
        mDoMtx_stack_c::transM(0.0f, i_this->m318, 0.0f);

        J3DModel* model = i_this->mpMorf->getModel();
        model->setBaseTRMtx(mDoMtx_stack_c::get());

        if (i_this->m2D6 != 0) {
            i_this->m2D6--;
            if (i_this->m2D6 <= 7) {
                i_this->mpBtp->setFrame(i_this->m2D6);
            }
        } else {
            i_this->m2D6 = 20.0f + cM_rndF(30.0f);
        }

        i_this->mpMorf->play(&actor->eyePos, 0, 0);
        i_this->mpBrk->setFrame(i_this->m2CC);
        i_this->mpMorf->calc();
        enemy_fire(&i_this->mEnemyFire);

        MTXCopy(model->getAnmMtx(PT_JNT_ATAMA_e), *calc_mtx);
        MtxPosition(&offset, &actor->eyePos);
        actor->attention_info.position = actor->eyePos;
#if VERSION == VERSION_DEMO
        i_this->mSph.SetC(actor->eyePos);
        i_this->mSph.SetR(40.0f + REG0_F(11));
#else
        actor->attention_info.position.y += 30.0f;

        i_this->mSph.SetC(actor->eyePos);
        i_this->mSph.SetR(40.0f);
#endif
        dComIfG_Ccsp()->Set(&i_this->mSph);

        cXyz at_pos;
        if (i_this->m327 != 0) {
            MTXCopy(model->getAnmMtx(PT_JNT_FORK_e), *calc_mtx);
#if VERSION == VERSION_DEMO
            offset.x = 50.0f + REG0_F(19);
#else
            offset.x = 50.0f;
#endif
            offset.y = 0.0f;
            offset.z = 0.0f;
            MtxPosition(&offset, &at_pos);
            i_this->m327 = 0;
        } else {
            at_pos.x = 20000.0f;
            at_pos.y = 50000.0f;
            at_pos.z = 20000.0f;
        }
        i_this->mAtSph.SetC(at_pos);
        dComIfG_Ccsp()->Set(&i_this->mAtSph);
    }

    return TRUE;
}

/* 000043C8-000043D0       .text daPt_IsDelete__FP8pt_class */
static BOOL daPt_IsDelete(pt_class*) {
    return TRUE;
}

/* 000043D0-00004540       .text daPt_Delete__FP8pt_class */
static BOOL daPt_Delete(pt_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    dComIfG_deleteObjectRes("Pt");
#else
    dComIfG_resDelete(&i_this->mPhase, "Pt");
#endif
    if (i_this->mHioSet) {
        hio_set = false;
        mDoHIO_deleteChild(l_HIO.mNo);
    }
    i_this->mSmokeCb.remove();
    enemy_fire_remove(&i_this->mEnemyFire);

    if (i_this->mDoRespawn != 0) {
        if (i_this->mEnableSpawnSwitch == 0xFF || dComIfGs_isSwitch(i_this->mEnableSpawnSwitch, fopAcM_GetRoomNo(actor))) {
            if (i_this->mDisableRespawnSwitch == 0xFF || !dComIfGs_isSwitch(i_this->mDisableRespawnSwitch, dStage_roomControl_c::getStayNo())) {
                fopAcM_prm_class* append = fopAcM_CreateAppend();
                append->base.position = actor->home.pos;
                append->base.angle = actor->home.angle;
                if (i_this->mRespawnDelay != 7) {
                    append->base.angle.x = i_this->mRespawnDelay * 0x14 + 0x14;
                }
                append->base.parameters = fopAcM_GetParam(actor) | 0x10;
                append->room_no = fopAcM_GetRoomNo(actor);
                fopAcM_Create(fpcNm_PT_e, NULL, append);
            }
        }
    }
    return TRUE;
}

/* 00004540-000047D4       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* a_this) {
    pt_class* i_this = (pt_class*)a_this;

    i_this->mpMorf = new mDoExt_McaMorf(
        (J3DModelData*)dComIfG_getObjectRes("Pt", dRes_INDEX_PT_BDL_PT_e),
        NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("Pt", dRes_INDEX_PT_BCK_WAIT_e),
        J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, 1,
        NULL,
        0,
        0x11020203
    );
    if ((i_this->mpMorf == NULL) || (i_this->mpMorf->getModel() == NULL)) {
        return FALSE;
    }

    i_this->mpBtp = new mDoExt_btpAnm();
    if (i_this->mpBtp == NULL) {
        return FALSE;
    }
    if (i_this->mpBtp->init(
            i_this->mpMorf->getModel()->getModelData(),
            (J3DAnmTexPattern*)dComIfG_getObjectRes("Pt", dRes_INDEX_PT_BTP_MABATAKI_e),
            TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    i_this->mpBrk = new mDoExt_brkAnm();
    if (i_this->mpBrk == NULL) {
        return FALSE;
    }
    if (i_this->mpBrk->init(
            i_this->mpMorf->getModel()->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("Pt", dRes_INDEX_PT_BRK_PT_e),
            TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, FALSE
        ) == 0)
    {
        return FALSE;
    }

    i_this->m2CC = cM_rndF(3.999f);
    return TRUE;
}

/* 0000481C-00004B1C       .text daPt_Create__FP10fopAc_ac_c */
static cPhs_State daPt_Create(fopAc_ac_c* a_this) {
    static dCcD_SrcSph cc_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
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
            /* Radius */ 40.0f,
        }},
    };
    static dCcD_SrcSph at_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_UNK800,
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
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 25.0f,
        }},
    };
    static u8 fire_j[] = {0x08, 0xFF, 0x07, 0xFA, 0x0B, 0xFF, 0x0D, 0xFF, 0x00, 0x00};
    static f32 fire_sc[] = {1.0f, 0.0f, 0.7f, 0.0f, 0.6f, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f};

    pt_class* i_this = (pt_class*)a_this;
#if VERSION == VERSION_DEMO
    cPhs_State phase_state = dComIfG_resLoad(&i_this->mPhase, "Pt");
    if (phase_state == cPhs_COMPLEATE_e) {
        fopAcM_ct(a_this, pt_class);
#else
    fopAcM_ct(a_this, pt_class);

    cPhs_State phase_state = dComIfG_resLoad(&i_this->mPhase, "Pt");
    if (phase_state == cPhs_COMPLEATE_e) {
#endif
        i_this->mBehaviorType = fopAcM_GetParam(a_this) & 0xF;
        i_this->mRespawnDelay = (fopAcM_GetParam(a_this) >> 5) & 7;
        i_this->mNoticeRange = (fopAcM_GetParam(a_this) >> 8) & 0xFF;
        if (fopAcM_GetParam(a_this) & 0x10) {
            i_this->mHide = 1;
        }
        i_this->mInitialSpawnDelay = a_this->current.angle.x;
        a_this->current.angle.x = 0;
        i_this->mDisableRespawnSwitch = (fopAcM_GetParam(a_this) >> 16) & 0xFF;
        i_this->mEnableSpawnSwitch = fopAcM_GetParam(a_this) >> 24;
        if (i_this->mEnableSpawnSwitch != 0xFF && !dComIfGs_isSwitch(i_this->mEnableSpawnSwitch, dStage_roomControl_c::getStayNo())) {
            i_this->mHide = 1;
        }

        a_this->itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("Puti", 0);
        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, 0x4B000)) {
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            i_this->mHioSet = true;
            hio_set = true;
            l_HIO.mNo = mDoHIO_createChild("プチブリン", &l_HIO);
        }

        fopAcM_OnStatus(a_this, fopAcStts_CULL_e);
        a_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
        i_this->m2D2 = 0;
        fopAcM_SetMtx(a_this, i_this->mpMorf->getModel()->getBaseTRMtx());
        i_this->mAcch.Set(
            fopAcM_GetPosition_p(a_this),
            fopAcM_GetOldPosition_p(a_this),
            a_this,
            1,
            &i_this->mAcchCir,
            fopAcM_GetSpeed_p(a_this),
            NULL,
            NULL
        );
        i_this->mAcchCir.SetWall(50.0f, 50.0f);
        a_this->health = 2;
        a_this->max_health = 2;
        i_this->mStts.Init(0x64, 0, a_this);
        i_this->mSph.Set(cc_sph_src);
        i_this->mSph.SetStts(&i_this->mStts);
        i_this->mAtSph.Set(at_sph_src);
        i_this->mAtSph.SetStts(&i_this->mStts);
        a_this->stealItemLeft = 2;

        i_this->mEnemyIce.mpActor = a_this;
        i_this->mEnemyIce.mWallRadius = REG0_F(4) + 30.0f;
        i_this->mEnemyIce.mCylHeight = REG0_F(5) + 30.0f;
        i_this->mEnemyIce.mParticleScale = REG0_F(8) + 0.6f;
        i_this->mEnemyIce.mYOffset = REG0_F(9) + 40.0f;
        i_this->mEnemyFire.mpMcaMorf = i_this->mpMorf;
        i_this->mEnemyFire.mpActor = a_this;
        for (int i = 0; i < ARRAY_SIZE(i_this->mEnemyFire.mFlameJntIdxs); i++) {
            i_this->mEnemyFire.mFlameJntIdxs[i] = fire_j[i];
            i_this->mEnemyFire.mParticleScale[i] = fire_sc[i];
        }

        daPt_Execute(i_this);
    }
    return phase_state;
}

static actor_method_class l_daPt_Method = {
    (process_method_func)daPt_Create,
    (process_method_func)daPt_Delete,
    (process_method_func)daPt_Execute,
    (process_method_func)daPt_IsDelete,
    (process_method_func)daPt_Draw,
};

actor_process_profile_definition g_profile_PT = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_PT_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(pt_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_PT_e,
    /* Actor SubMtd */ &l_daPt_Method,
    /* Status       */ fopAcStts_UNK40000_e | fopAcStts_UNK80000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
