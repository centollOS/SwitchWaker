/**
 * d_a_klft.cpp
 * Object - Forbidden Woods - Lift (moving wooden platform)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_klft.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_cc_uty.h"
#include "d/d_path.h"
#include "d/d_bg_s_movebg_actor.h"
#include "res/Object/Klft.h"
#include "d/d_kankyo_wether.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_controller_pad.h"
#include "JSystem/JUtility/JUTAssert.h"

static cXyz* wind_vec;
static s16 wy;
static f32* wp;

/* 00000078-00000374       .text ride_call_back__FP4dBgWP10fopAc_ac_cP10fopAc_ac_c */
static void ride_call_back(dBgW*, fopAc_ac_c* base, fopAc_ac_c* rider) {
    klft_class* actor = (klft_class*)base;
    cXyz delta, pos, oldPos;
#if VERSION == VERSION_DEMO
    cMtx_YrotS(*calc_mtx, -base->current.angle.y);
#else
    mDoMtx_YrotS(*calc_mtx, -base->current.angle.y);
#endif
    delta = rider->current.pos - actor->mRidePos;
    MtxPosition(&delta, &pos);
    delta = rider->old.pos - actor->mRidePos;
    MtxPosition(&delta, &oldPos);
    if (fopAcM_GetName(rider) == fpcNm_PLAYER_e) {
        actor->mRideTimer = 10;
    }
    actor->mSinkTarget = -50.0f;
    f32 distance = std::sqrtf(pos.x * pos.x + pos.z * pos.z);
    cLib_addCalcAngleS2(&actor->mRideTilt, distance * ((30.0f + REG0_F(0)) / base->scale.z), 10, 0x800);
    s16 angle_target = cM_atan2s(pos.x, pos.z);
    cLib_addCalcAngleS2(&actor->mRideAngle, angle_target, 2, 0x2000);
    f32 stickX = CPad_GET_STICK_POS_X(0);
    f32 stickY = CPad_GET_STICK_POS_Y(0);
    if (std::fabsf(stickX) + std::fabsf(stickY) > 0.1f || std::fabsf(actor->mMoveSpeed) > 0.0001f) {
        f32 tilt = (100.0f + REG0_F(4)) * std::fabsf(pos.z - oldPos.z);
        if (tilt > 200.0f + REG0_F(6)) {
            actor->mTiltTarget.x = tilt;
        }
        tilt = (100.0f + REG0_F(4)) * std::fabsf(pos.x - oldPos.x);
        if (tilt > 200.0f + REG0_F(6)) {
            actor->mTiltTarget.z = tilt;
        }
        f32 sway = (2.0f + REG0_F(8)) * std::fabsf(pos.x - oldPos.x);
        if (sway > 10.0f && actor->mSway.x < sway) {
            actor->mSwayTarget.x = sway;
        }
        sway = (2.0f + REG0_F(8)) * std::fabsf(pos.z - oldPos.z);
        if (sway > 10.0f && actor->mSway.z < sway) {
            actor->mSwayTarget.z = sway;
        }
    }
}

/* 000003B0-0000046C       .text nodeCallBack_main__FP7J3DNodei */
static BOOL nodeCallBack_main(J3DNode* node, int phase) {
    if (phase == 0) {
        int jointNo = ((J3DJoint*)node)->getJntNo();
        J3DModel* model = j3dSys.getModel();
        klft_class* actor = (klft_class*)model->getUserArea();
        if (actor != NULL && jointNo == 1) {
            MTXCopy(model->getAnmMtx(jointNo), *calc_mtx);
#if VERSION == VERSION_DEMO
            cMtx_XrotM(*calc_mtx, actor->mPlatformAngle);
            model->setAnmMtx(jointNo, *calc_mtx);
#else
            mDoMtx_XrotM(*calc_mtx, actor->mPlatformAngle);
            MTXCopy(*calc_mtx, model->getAnmMtx(jointNo));
#endif
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 0000046C-00000520       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int phase) {
    if (phase == 0) {
        int jointNo = ((J3DJoint*)node)->getJntNo();
        J3DModel* model = j3dSys.getModel();
        klft_class* actor = (klft_class*)model->getUserArea();
        if (actor != NULL) {
            MTXCopy(model->getAnmMtx(jointNo), *calc_mtx);
#if VERSION == VERSION_DEMO
            cMtx_YrotM(*calc_mtx, actor->mPulleyAngle);
            model->setAnmMtx(jointNo, *calc_mtx);
#else
            mDoMtx_YrotM(*calc_mtx, actor->mPulleyAngle);
            MTXCopy(*calc_mtx, model->getAnmMtx(jointNo));
#endif
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 00000520-000005A0       .text himo_Draw__FP10klft_class */
void himo_Draw(klft_class* actor) {
    actor->mRope.update(20, (GXColor){150, 150, 150, 255}, &actor->tevStr);
    dComIfGd_set3DlineMat(&actor->mRope);
}

/* 000005A0-00000688       .text daKlft_Draw__FP10klft_class */
static BOOL daKlft_Draw(klft_class* actor) {
    g_env_light.settingTevStruct(1, &actor->current.pos, &actor->tevStr);
    g_env_light.setLightTevColorType(actor->mpModel, &actor->tevStr);
    dComIfGd_setListBG();
    mDoExt_modelUpdateDL(actor->mpModel);
    dComIfGd_setList();
    himo_Draw(actor);
    for (int i = 0; i < 2; ++i) {
        g_env_light.setLightTevColorType(actor->mpPulley[i]->getModel(), &actor->tevStr);
        actor->mpPulley[i]->updateDL();
    }
    return TRUE;
}

/* 00000688-00000F24       .text klft_move__FP10klft_class */
void klft_move(klft_class* actor) {
    fopAc_ac_c* base = actor;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz vec, result;
    actor->mCounter++;
    s16 angle;
    f32 power;
    if (actor->mRideTimer != 0) {
        angle = player->shape_angle.y;
        power = 1.0f;
    } else {
        angle = player->shape_angle.y + 0x8000;
        power = 0.5f;
    }
    f32 accel = 0.01f;
    if (actor->mWindSph.ChkTgHit() || actor->mPulleySph[0].ChkTgHit() || actor->mPulleySph[1].ChkTgHit()) {
        if (actor->mWindSph.ChkTgHit()) {
#if VERSION == VERSION_DEMO
            cMtx_YrotS(*calc_mtx, angle - base->current.angle.y);
#else
            mDoMtx_YrotS(*calc_mtx, angle - base->current.angle.y);
#endif
            vec.z = (0.5f + REG0_F(13)) * power;
        } else {
            if (actor->mPulleySph[0].ChkTgHit()) {
#if VERSION == VERSION_DEMO
                cMtx_YrotS(*calc_mtx, -0x8000);
#else
                mDoMtx_YrotS(*calc_mtx, -0x8000);
#endif
            } else {
#if VERSION == VERSION_DEMO
                cMtx_YrotS(*calc_mtx, 0);
#else
                mDoMtx_YrotS(*calc_mtx, 0);
#endif
            }
            vec.z = (0.85f + REG0_F(16)) * power;
            accel = 0.1f;
        }
        vec.y = vec.x = 0.0f;
        MtxPosition(&vec, &result);
        actor->mMoveSpeedTarget = result.z;
        vec.x = (500.0f + REG0_F(15)) * power;
        vec.z = vec.y = 0.0f;
        MtxPosition(&vec, &actor->mTiltTarget);
        actor->mWindTimer = REG0_S(5) + 60;
#if VERSION == VERSION_DEMO
        cMtx_YrotS(*calc_mtx, angle);
#else
        mDoMtx_YrotS(*calc_mtx, angle);
#endif
        vec.y = vec.x = 0.0f;
        vec.z = (20.0f + REG0_F(16)) * power;
        MtxPosition(&vec, &actor->mSwayTarget);
    }
    if (actor->mWindTimer == 40) {
#if VERSION == VERSION_DEMO
        mDoAud_seStart(JA_SE_OBJ_KASSHA_LIFT_SWING, &base->current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(base)));
#else
        mDoAud_seStart(JA_SE_OBJ_KASSHA_LIFT_SWING, &base->current.pos, 0, dComIfGp_getReverb(base->current.roomNo));
#endif
    }
    cLib_addCalc2(&actor->mMoveSpeed, actor->mMoveSpeedTarget, 1.0f, accel);
    cLib_addCalc0(&actor->mMoveSpeedTarget, 1.0f, 0.001f + REG0_F(14));
    actor->mProgress += actor->mMoveSpeed;
    if (actor->mProgress > 80.0f) {
        actor->mProgress = 80.0f;
        actor->mMoveSpeed = 0.0f;
    } else if (actor->mProgress < 20.0f) {
        actor->mProgress = 20.0f;
        actor->mMoveSpeed = 0.0f;
    }
    actor->mPlatformAngle += (s16)(-2500.0f * actor->mMoveSpeed);
    if (std::fabsf(actor->mMoveSpeed) > 0.01f) {
        u32 volume = (500.0f + REG0_F(0)) * std::fabsf(actor->mMoveSpeed);
        if (volume > 100) {
            volume = 100;
        }
        for (int i = 0; i < 2; ++i) {
#if VERSION == VERSION_DEMO
            mDoAud_seStart(JA_SE_OBJ_KM_WINDMILL, &actor->mPulleyPos[i], volume, dComIfGp_getReverb(fopAcM_GetRoomNo(base)));
#else
            mDoAud_seStart(JA_SE_OBJ_KM_WINDMILL, &actor->mPulleyPos[i], volume, dComIfGp_getReverb(base->current.roomNo));
#endif
        }
        actor->mMoveSoundTimer += std::fabsf(actor->mMoveSpeed);
        if (actor->mMoveSoundTimer > 3.0f + REG0_F(1)) {
            actor->mMoveSoundTimer -= 3.0f + REG0_F(1);
#if VERSION == VERSION_DEMO
            mDoAud_seStart(JA_SE_OBJ_KASSHA_LIFT_MOVE, &base->current.pos, volume, dComIfGp_getReverb(fopAcM_GetRoomNo(base)));
#else
            mDoAud_seStart(JA_SE_OBJ_KASSHA_LIFT_MOVE, &base->current.pos, volume, dComIfGp_getReverb(base->current.roomNo));
#endif
        }
    }
    for (int i = 0; i < 2; ++i) {
        if (actor->mPulleyCyl[i].ChkTgHit()) {
            def_se_set(base, actor->mPulleyCyl[i].GetTgHitObj(), 11);
        }
    }
    cLib_addCalcAngleS2(&actor->mRideTilt, 0, 10, 0x200);
    cLib_addCalcAngleS2(&base->current.angle.x, 0, 10, 0x200);
    cLib_addCalcAngleS2(&base->current.angle.z, 0, 10, 0x200);
    actor->mShakeAngle.x = actor->mTilt.x * cM_ssin(actor->mCounter * 1500);
    actor->mShakeAngle.z = actor->mTilt.z * cM_ssin(actor->mCounter * 1300);
    f32 step;
    if (actor->mWindTimer != 0) {
        step = 10.0f;
    } else if (std::fabsf(actor->mTiltTarget.x) > 100.0f + REG0_F(2)) {
        step = 10.0f + REG0_F(4);
    } else {
        step = 2.0f + REG0_F(3);
    }
    cLib_addCalc2(&actor->mTilt.x, actor->mTiltTarget.x, 1.0f, step);
    if (actor->mWindTimer != 0) {
        step = 40.0f;
    } else if (std::fabsf(actor->mTiltTarget.z) > 100.0f + REG0_F(2)) {
        step = 10.0f + REG0_F(4);
    } else {
        step = 2.0f + REG0_F(3);
    }
    cLib_addCalc2(&actor->mTilt.z, actor->mTiltTarget.z, 1.0f, step);
    if (actor->mWindTimer == 0) {
        actor->mTiltTarget.x = actor->mTiltTarget.z = 0.0f;
    }
    f32 sine = cM_ssin(cM_rad2s(M_PI * (0.01f * actor->mProgress)));
    actor->mShake.x = sine * (actor->mSway.x * cM_ssin(actor->mCounter * 750));
    actor->mShake.z = sine * (actor->mSway.z * cM_ssin(actor->mCounter * 900));
    actor->mShake.y = sine * ((1.0f + REG0_F(1)) * ((actor->mSway.x + actor->mSway.z) * cM_ssin(actor->mCounter * 1500)));
    if (actor->mWindTimer != 0) {
        step = 10.0f;
    } else if (std::fabsf(actor->mSwayTarget.x) > 10.0f) {
        step = 1.2f;
    } else {
        step = 0.25f;
    }
    cLib_addCalc2(&actor->mSway.x, actor->mSwayTarget.x, 1.0f, step);
    if (actor->mWindTimer != 0) {
        step = 10.0f;
    } else if (std::fabsf(actor->mSwayTarget.z) > 10.0f) {
        step = 1.2f;
    } else {
        step = 0.25f;
    }
    cLib_addCalc2(&actor->mSway.z, actor->mSwayTarget.z, 1.0f, step);
    if (actor->mWindTimer == 0) {
        actor->mSwayTarget.z = actor->mSwayTarget.x = 0.0f;
    }
    base->shape_angle = base->current.angle + actor->mShakeAngle;
    base->home.pos = actor->mEnds[0] + actor->mPathDelta * actor->mProgress * 0.01f;
    base->home.pos.y += (-100.0f + REG0_F(2)) + sine * ((-100.0f + actor->mSink) + REG0_F(0));
    cLib_addCalc2(&actor->mSink, actor->mSinkTarget, 0.1f, 5.0f);
    actor->mSinkTarget = 0.0f;
    base->current.pos = base->home.pos + actor->mShake;
}

/* 00000F24-00001140       .text himo_move__FP10klft_class */
void himo_move(klft_class* actor) {
    cXyz* pos = actor->mRope.getPos(0);
    u8* size = actor->mRope.getSize(0);
    cXyz delta0 = actor->current.pos - actor->mEnds[0];
    cXyz delta1 = actor->current.pos - actor->mEnds[1];
#if VERSION > VERSION_DEMO
    delta0.y -= 25.0f;
    delta1.y -= 25.0f;
#endif
    for (int i = 0; i < 20; ++i, ++pos, ++size) {
        f32 t;
        if (i < 10) {
            t = i / 9.0f;
            *pos = actor->mEnds[0] + delta0 * t;
        } else {
            t = (19 - i) / 9.0f;
            *pos = actor->mEnds[1] + delta1 * t;
        }
        pos->y += (-20.0f + REG0_F(7)) * cM_ssin(cM_rad2s(M_PI * t));
        *size = 8;
    }
}

/* 00001140-00001444       .text daKlft_Execute__FP10klft_class */
static BOOL daKlft_Execute(klft_class* actor) {
    if (actor->mWindTimer != 0) {
        --actor->mWindTimer;
    }
    if (actor->mRideTimer != 0) {
        --actor->mRideTimer;
    }
    wind_vec = dKyw_get_wind_vec();
    wy = cM_atan2s(wind_vec->x, wind_vec->z);
    wp = dKyw_get_wind_power();
    klft_move(actor);
    himo_move(actor);
    MtxTrans(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z, 0);
#if VERSION == VERSION_DEMO
    cMtx_YrotM(*calc_mtx, actor->shape_angle.y);
    cMtx_YrotM(*calc_mtx, actor->mRideAngle);
    cMtx_XrotM(*calc_mtx, actor->mRideTilt);
    cMtx_YrotM(*calc_mtx, -actor->mRideAngle);
    cMtx_XrotM(*calc_mtx, actor->shape_angle.x);
    cMtx_ZrotM(*calc_mtx, actor->shape_angle.z);
#else
    mDoMtx_YrotM(*calc_mtx, actor->shape_angle.y);
    mDoMtx_YrotM(*calc_mtx, actor->mRideAngle);
    mDoMtx_XrotM(*calc_mtx, actor->mRideTilt);
    mDoMtx_YrotM(*calc_mtx, -actor->mRideAngle);
    mDoMtx_XrotM(*calc_mtx, actor->shape_angle.x);
    mDoMtx_ZrotM(*calc_mtx, actor->shape_angle.z);
#endif
    actor->mpModel->setBaseTRMtx(*calc_mtx);
    MTXCopy(*calc_mtx, actor->mBgMtx);
    MtxTrans(0.0f, REG0_F(7) - 400.0f, 0.0f, 1);
    cXyz vec(0.0f, 0.0f, 0.0f);
    MtxPosition(&vec, &actor->mRidePos);
    actor->mStts.Move();
    actor->mWindSph.SetC(actor->mRidePos);
    dComIfG_Ccsp()->Set(&actor->mWindSph);
    for (int i = 0; i < 2; ++i) {
        vec = actor->mPulleyPos[i];
        vec.y += 35.0f + REG0_F(11);
        actor->mPulleySph[i].SetC(vec);
        dComIfG_Ccsp()->Set(&actor->mPulleySph[i]);
        actor->mPulleyCyl[i].SetC(vec);
        dComIfG_Ccsp()->Set(&actor->mPulleyCyl[i]);
    }
    actor->pm_bgw->Move();
    actor->mPulleyAngle += (s16)(actor->mMoveSpeed * (8000.0f + REG0_F(8)));
    for (int i = 0; i < 2; ++i) {
        MtxTrans(actor->mPulleyPos[i].x, actor->mPulleyPos[i].y, actor->mPulleyPos[i].z, 0);
        actor->mpPulley[i]->getModel()->setBaseTRMtx(*calc_mtx);
    }
    return TRUE;
}

/* 00001444-0000144C       .text daKlft_IsDelete__FP10klft_class */
static BOOL daKlft_IsDelete(klft_class*) {
    return TRUE;
}

/* 0000144C-00001520       .text daKlft_Delete__FP10klft_class */
static BOOL daKlft_Delete(klft_class* actor) {
#if VERSION == VERSION_DEMO
    dComIfG_deleteObjectRes("Klft");
    dComIfG_Bgsp()->Release(actor->pm_bgw);
    if (actor->mSwitch != 0) {
        if (actor->mProgress >= 50.0f) {
            dComIfGs_onSwitch(actor->mSwitch, fopAcM_GetRoomNo(actor));
        } else {
            dComIfGs_offSwitch(actor->mSwitch, fopAcM_GetRoomNo(actor));
        }
    }
#else
    dComIfG_resDelete(&actor->mPhase, "Klft");
    if (actor->heap != NULL) {
        dComIfG_Bgsp()->Release(actor->pm_bgw);
    }
    if (actor->mSwitch != 0) {
        if (actor->mProgress >= 50.0f) {
            dComIfGs_onSwitch(actor->mSwitch, actor->current.roomNo);
        } else {
            dComIfGs_offSwitch(actor->mSwitch, actor->current.roomNo);
        }
    }
#endif
    mDoAud_seDeleteObject(&actor->mPulleyPos[0]);
    mDoAud_seDeleteObject(&actor->mPulleyPos[1]);
    return TRUE;
}

/* 00001520-000017C4       .text CallbackCreateHeap__FP10fopAc_ac_c */
static BOOL CallbackCreateHeap(fopAc_ac_c* base) {
    klft_class* actor = (klft_class*)base;
    J3DModelData* data = (J3DModelData*)dComIfG_getObjectRes("Klft", dRes_INDEX_KLFT_BDL_LIFT_00_e);
    actor->mpModel = mDoExt_J3DModel__create(data, 0, 0x11020203);
    if (actor->mpModel == NULL) {
        return FALSE;
    }
    actor->mpModel->setUserArea((u32)actor);
    for (u16 i = 0; i < data->getJointNum(); ++i) {
        if (i == 1) {
            data->getJointNodePointer(i)->setCallBack(nodeCallBack_main);
        }
    }
    actor->pm_bgw = new dBgW;
    JUT_ASSERT(DEMO_SELECT(0x339, 0x343), actor->pm_bgw != 0);
    actor->pm_bgw->Set((cBgD_t*)dComIfG_getObjectRes("Klft", dRes_INDEX_KLFT_DZB_LIFT_00_e), 1, &actor->mBgMtx);
    actor->pm_bgw->SetCrrFunc(dBgS_MoveBGProc_Typical);
    actor->pm_bgw->SetRideCallback(ride_call_back);
    if (!actor->mRope.init(1, 20, (ResTIMG*)dComIfG_getObjectRes("Klft", dRes_INDEX_KLFT_BTI_K_ROPE_01_e), TRUE)) {
        return FALSE;
    }
    for (int i = 0; i < 2; ++i) {
        actor->mpPulley[i] = new mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("Klft", dRes_INDEX_KLFT_BDL_VPBOT_00_e), NULL, NULL, NULL, 2, 1.0f, 0, -1, 0, NULL, 0, 0x11020203);
        J3DModel* model = actor->mpPulley[i]->getModel();
        model->setUserArea((u32)actor);
        for (u16 j = 0; j < model->getModelData()->getJointNum(); ++j) {
            if (j == 3) {
                model->getModelData()->getJointNodePointer(j)->setCallBack(nodeCallBack);
            }
        }
    }
    return TRUE;
}

/* 000017C4-00001C78       .text daKlft_Create__FP10fopAc_ac_c */
static cPhs_State daKlft_Create(fopAc_ac_c* base) {
    static dCcD_SrcSph utiwa_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
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
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 250.0f,
        }},
    };
    static dCcD_SrcCyl p_co_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
#if VERSION == VERSION_USA
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_BOOMERANG & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
#else
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
#endif
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_GrpAll_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ dCcG_SE_UNK6,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGCylS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 40.0f,
            /* Height */ 60.0f,
        }},
    };
    klft_class* actor = (klft_class*)base;
    fopAcM_SetupActor(base, klft_class);
    u32 pathNo;
    cPhs_State phase;
    phase = dComIfG_resLoad(&actor->mPhase, "Klft");
    if (phase == cPhs_COMPLEATE_e) {
        actor->mType = fopAcM_GetParam(actor);
        pathNo = (fopAcM_GetParam(actor) >> 16) & 0xFF;
        actor->mSwitch = actor->current.angle.z;
        actor->current.angle.z = 0;
        if (actor->mSwitch == 0xFF) {
            actor->mSwitch = 0;
        }
        if (actor->mSwitch != 0) {
#if VERSION == VERSION_DEMO
            if (dComIfGs_isSwitch(actor->mSwitch, fopAcM_GetRoomNo(actor))) {
#else
            if (dComIfGs_isSwitch(actor->mSwitch, actor->current.roomNo)) {
#endif
                actor->mProgress = 80.0f;
            } else {
                actor->mProgress = 20.0f;
            }
        }
        if (actor->mType == 0xFF) {
            actor->mType = 0;
        }
        if (!fopAcM_entrySolidHeap(base, CallbackCreateHeap, 0x10000)) {
            return cPhs_ERROR_e;
        }
        if (actor->pm_bgw != NULL && dComIfG_Bgsp()->Regist(actor->pm_bgw, actor)) {
            return cPhs_ERROR_e;
        }
        if (pathNo == 0xFF) {
            return cPhs_ERROR_e;
        }
#if VERSION == VERSION_DEMO
        dPath* path = dPath_GetRoomPath(pathNo, fopAcM_GetRoomNo(actor));
#else
        dPath* path = dPath_GetRoomPath(pathNo, actor->current.roomNo);
#endif
        if (path != NULL) {
            dPnt* points = path->m_points;
            actor->mEnds[0] = points[0].m_position;
            actor->mEnds[1] = points[1].m_position;
            cXyz delta = actor->mEnds[1] - actor->mEnds[0];
            actor->home.angle.y = cM_atan2s(delta.x, delta.z) + 0x8000;
            actor->current.angle.y = cM_atan2s(delta.x, delta.z) + 0x8000;
            actor->mPathDelta = delta;
            actor->mPulleyPos[0] = points[2].m_position;
            actor->mPulleyPos[1] = points[3].m_position;
        } else {
            return cPhs_ERROR_e;
        }
        switch (actor->mType) {
        case 1:
            actor->scale.x = 0.9f;
            actor->scale.z = 0.9f;
            break;
        case 2:
            actor->scale.x = 0.8f;
            actor->scale.z = 0.8f;
            break;
        case 3:
            actor->scale.x = 0.7f;
            actor->scale.z = 0.7f;
            break;
        default:
            actor->scale.x = actor->scale.z = 1.0f;
            break;
        }
        actor->scale.y = 1.0f;
        actor->mpModel->setBaseScale(actor->scale);
        actor->mStts.Init(255, 255, actor);
        actor->mWindSph.Set(utiwa_sph_src);
        actor->mWindSph.SetStts(&actor->mStts);
        for (int i = 0; i < 2; ++i) {
            actor->mPulleySph[i].Set(utiwa_sph_src);
            actor->mPulleySph[i].SetStts(&actor->mStts);
            actor->mPulleySph[i].SetR(130.0f + REG0_F(11));
            actor->mPulleyCyl[i].Set(p_co_cyl_src);
            actor->mPulleyCyl[i].SetStts(&actor->mStts);
        }
        daKlft_Execute(actor);
    }
    return phase;
}

static actor_method_class l_daKlft_Method = {
    (process_method_func)daKlft_Create,
    (process_method_func)daKlft_Delete,
    (process_method_func)daKlft_Execute,
    (process_method_func)daKlft_IsDelete,
    (process_method_func)daKlft_Draw,
};

actor_process_profile_definition g_profile_KLFT = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_KLFT_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(klft_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_KLFT_e,
    /* Actor SubMtd */ &l_daKlft_Method,
    /* Status       */ fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
