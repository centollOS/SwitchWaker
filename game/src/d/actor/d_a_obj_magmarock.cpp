/**
 * d_a_obj_magmarock.cpp
 * Object - Dragon Roost Cavern - Lava slab
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_magmarock.h"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_magma.h"
#include "d/d_s_play.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Kyjim.h"

const char daObjMagmarock::Act_c::M_arcname[] = "Kyjim";
GXColor daObjMagmarock::Act_c::default_color = {255, 255, 255, 255};


/* 00000078-00000128       .text set_mtx__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::set_mtx() {
    mpModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle.x, shape_angle.y, shape_angle.z);
    Quaternion quat;
    mDoMtx_quatMultiply(&mRideQuat, &mGroundQuat, &quat);
    mDoMtx_stack_c::quatM(&quat);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    MTXCopy(mDoMtx_stack_c::get(), mBgMtx);
}

/* 00000128-00000258       .text demo_move__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::demo_move() {
    if (dComIfGs_isEventBit(0x380) || mType != 0) {
        return;
    }
    if (mDemoState == 0) {
        if (eventInfo.checkCommandDemoAccrpt()) {
            mDemoState++;
        } else {
            fopAcM_orderOtherEvent2(this, "magma_cam", 1, 0xffff);
            eventInfo.onCondition(dEvtCnd_UNK2_e);
        }
    } else if (mDemoState == 1) {
        int staffId = dComIfGp_evmng_getMyStaffId("Magrock", NULL, 0);
        if (dComIfGp_evmng_endCheck("magma_cam")) {
            dComIfGp_event_reset();
            mDemoState++;
            dComIfGs_onEventBit(0x380);
        } else {
            dComIfGp_evmng_cutEnd(staffId);
        }
    }
}

/* 00000258-00000410       .text ControlEffect__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::ControlEffect() {
    if (mLiftRequested) {
        if (mBeforeLiftRequested) {
            if (mpBeforeLiftEffect == NULL) {
                mpBeforeLiftEffect = dComIfGp_particle_set(dPa_name::ID_AK_SN_MAGMAISLAND00, &current.pos);
            } else {
                mpBeforeLiftEffect->setGlobalTranslation(current.pos.x, current.pos.y, current.pos.z);
            }
        } else {
            if (mpBeforeLiftEffect) {
                mpBeforeLiftEffect->becomeInvalidEmitter();
                mpBeforeLiftEffect = NULL;
            }
            if (mpLiftEffect == NULL) {
#if VERSION > VERSION_DEMO
                dComIfGp_getVibration().StartShock(4, 1, cXyz(0.0f, 1.0f, 0.0f));
#endif
                mpLiftEffect = dComIfGp_particle_setToon(dPa_name::ID_AK_SN_MAGMAISLAND01, &current.pos);
            } else {
                mpLiftEffect->setGlobalTranslation(current.pos.x, current.pos.y, current.pos.z);
            }
        }
    } else if (mpLiftEffect) {
        mpLiftEffect->becomeInvalidEmitter();
        mpLiftEffect = NULL;
    }
}

/* 0000044C-00000560       .text play_anim__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::play_anim() {
    if (mAnimTimer > 375 && mBrkFrame > 0.0f) {
        mBrkFrame -= 1.0f;
    } else if ((mAnimTimer < 15 || mAnimTimer > 60) && mBrkFrame < M_brk->getFrameMax()) {
        mBrkFrame += 1.0f;
    }
    if (mAnimTimer < 60 && mBckFrame < M_bck->getFrameMax()) {
        mBckFrame += 1.0f;
    } else if (mAnimTimer > 375 && mBckFrame > 0.0f) {
        mBckFrame -= 1.0f;
    }
}

/* 00000560-0000058C       .text appear_proc_init__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::appear_proc_init() {
    mTimer = 30;
    setProcess(&Act_c::appear_proc);
}

/* 0000058C-000005EC       .text appear_proc__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::appear_proc() {
    if (mTimer == 10) {
        dComIfG_Bgsp()->Regist(mpBgW, this);
    }
    if (mTimer == 0) {
        wait_proc_init();
    }
}

/* 000005EC-00000618       .text wait_proc_init__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::wait_proc_init() {
    mTimer = 300;
    setProcess(&Act_c::wait_proc);
}

/* 00000618-00000644       .text wait_proc__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::wait_proc() {
    if (mTimer == 0) {
        quake_proc_init();
    }
}

/* 00000644-000006E0       .text stay_proc_init__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::stay_proc_init() {
    u8 time = fopAcM_GetParam(this) & 0xff;
    if (time == 0xff) {
        time = 0;
    }
    mBrkFrame = 30.0f;
    mBckFrame = 30.0f;
    mTimer = time * 15 + 30;
    mAnimTimer = 330;
    dComIfG_Bgsp()->Regist(mpBgW, this);
    setProcess(&Act_c::stay_proc);
}

/* 000006E0-00000720       .text stay_proc__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::stay_proc() {
    if (mHasRider && mTimer-- == 0) {
        quake_proc_init();
    }
}

/* 00000720-000007B8       .text quake_proc_init__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::quake_proc_init() {
    mDoAud_seStart(JA_SE_ISLE_TO_MAGMA, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    mTimer = 45;
    setProcess(&Act_c::quake_proc);
}

/* 000007B8-0000084C       .text quake_proc__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::quake_proc() {
    mQuakeAngle += mQuakeSpeed;
    cLib_addCalcAngleS2(&mQuakeSpeed, 0x1000, 2, 0x100);
    cLib_addCalc2(&mQuakeAmplitude, 750.0f + REG10_F(10), 0.25f, 50.0f);
    if (mTimer == 0) {
        vanish_proc_init();
    }
}

/* 0000084C-00000878       .text vanish_proc_init__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::vanish_proc_init() {
    mTimer = 90;
    setProcess(&Act_c::vanish_proc);
}

/* 00000878-000008F8       .text vanish_proc__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::vanish_proc() {
    mQuakeAngle += mQuakeSpeed;
    cLib_addCalcAngleS2(&mQuakeSpeed, 0, 4, 0x40);
    if (mTimer == 80) {
        dComIfG_Bgsp()->Release(mpBgW);
    }
    if (mTimer < 0) {
        fopAcM_delete(this);
    }
}

/* 000008F8-00000AEC       .text ride_call_back__14daObjMagmarockFP4dBgWP10fopAc_ac_cP10fopAc_ac_c */
void daObjMagmarock::ride_call_back(dBgW*, fopAc_ac_c* slab, fopAc_ac_c* rider) {
    Act_c* self = static_cast<Act_c*>(slab);
    cXyz dir = rider->current.pos - self->current.pos;
    cXyz up(0.0f, 1.0f, 0.0f);
    dir = dir.outprod(up);
    f32 distance = dir.abs();
    if (dir.normalizeRS()) {
        f32 dy = self->current.pos.y - self->home.pos.y;
        f32 h = 0.001f * dy;
        s16 target = -distance * (2.0f + 4.0f * h);
        cLib_addCalcAngleS2(&self->mRideAngle, target, 8, 0x200);
        self->mRidden = 1;
        self->mHasRider = 1;
        f32 sine = cM_ssin(self->mRideAngle);
        self->mRideTargetQuat.x = dir.x * sine;
        self->mRideTargetQuat.y = dir.y * sine;
        self->mRideTargetQuat.z = dir.z * sine;
        self->mRideTargetQuat.w = cM_scos(self->mRideAngle);
    }
}

/* 00000AEC-00000B0C       .text CheckCreateHeap__14daObjMagmarockFP10fopAc_ac_c */
BOOL daObjMagmarock::CheckCreateHeap(fopAc_ac_c* actor) {
    return static_cast<Act_c*>(actor)->CreateHeap();
}

/* 00000B0C-00000DA0       .text CreateHeap__Q214daObjMagmarock5Act_cFv */
BOOL daObjMagmarock::Act_c::CreateHeap() {
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_KYJIM_BDL_KYJIM_00_e));
    JUT_ASSERT(DEMO_SELECT(0x148, 0x14d), modelData != 0);
    mpModel = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
    M_brk = static_cast<J3DAnmTevRegKey*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_KYJIM_BRK_KYJIM_00_e));
    M_bck = static_cast<J3DAnmTransform*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_KYJIM_BCK_KYJIM_00_e));
    JUT_ASSERT(DEMO_SELECT(0x150, 0x155), M_brk != 0);
    JUT_ASSERT(DEMO_SELECT(0x151, 0x156), M_bck != 0);
    BOOL brkOK = mBrk.init(modelData, M_brk, FALSE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0);
    BOOL bckOK = mBck.init(modelData, M_bck, FALSE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::scaleM(scale.x, scale.y, scale.z);
    MTXCopy(mDoMtx_stack_c::get(), mBgMtx);
    mpBgW = dBgW_NewSet(static_cast<cBgD_t*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_KYJIM_DZB_KYJIM_00_e)), 1, &mBgMtx);
    mpBgW->SetCrrFunc(dBgS_MoveBGProc_Typical);
    if (mpBgW == NULL) {
        return FALSE;
    }
    return mpModel != NULL && brkOK && bckOK;
}

/* 00000DA0-000013B4       .text CreateInit__Q214daObjMagmarock5Act_cFv */
BOOL daObjMagmarock::Act_c::CreateInit() {
    scale.setall(1.0f);
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -200.0f, -30.0f, -200.0f, 200.0f, 30.0f, 200.0f);
    MTXCopy(mpModel->getBaseTRMtx(), mBgMtx);
    mQuakeAmplitude = 0.0f;
    mRideAngle = 0;
    mUnused29A = 0;
    mUnused450 = 0;
    mTimer = 30;
    mAnimTimer = 0;
    mBrkFrame = 0.0f;
    mQuakeAngle = 0;
    mQuakeSpeed = 0;
    mDemoState = 0;
    mHasRider = 0;
    speed.setall(0.0f);
    home.pos = current.pos;
    home.pos.y += 15.0f;
    gravity = -2.5f;
    mRideQuat = mRideTargetQuat = mGroundQuat = ZeroQuat;
    set_mtx();
    mpBgW->SetRideCallback(ride_call_back);
    mType = fopAcM_GetParam(this) >> 24;
    if (mType) {
        stay_proc_init();
    } else {
        appear_proc_init();
        if (mpSmoke0 == NULL) {
            mDoAud_seStart(JA_SE_MAGMA_TO_ISLE, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
#if VERSION > VERSION_DEMO
            dComIfGp_getVibration().StartShock(4, 1, cXyz(0.0f, 1.0f, 0.0f));
#endif
            mEffectTev = tevStr;
            g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &mEffectTev);
            mEffectTev.mColorC0.r = (u8)(mEffectTev.mColorC0.r + (int)(0.12f * (255 - mEffectTev.mColorC0.r)));
            mEffectTev.mColorC0.g = (u8)(mEffectTev.mColorC0.g + (int)(0.12f * (255 - mEffectTev.mColorC0.g)));
            mEffectTev.mColorC0.b = (u8)(mEffectTev.mColorC0.b + (int)(0.12f * (255 - mEffectTev.mColorC0.b)));
            mEffectTev.mColorK0.r = (u8)(mEffectTev.mColorK0.r + (int)(0.12f * (255 - mEffectTev.mColorK0.r)));
            mEffectTev.mColorK0.g = (u8)(mEffectTev.mColorK0.g + (int)(0.12f * (255 - mEffectTev.mColorK0.g)));
            mEffectTev.mColorK0.b = (u8)(mEffectTev.mColorK0.b + (int)(0.12f * (255 - mEffectTev.mColorK0.b)));
            mpSmoke0 = dComIfGp_particle_setToon(dPa_name::ID_AK_SN_YOGANYUGE00, &current.pos, NULL, NULL, 153.0f + 102.0f * REG10_F(25));
            mpSmoke1 = dComIfGp_particle_setToon(dPa_name::ID_AK_SN_YOGANYUGE01, &current.pos, NULL, NULL, 153.0f + 102.0f * REG10_F(26));
            if (mpSmoke0) {
                mpSmoke0->setGlobalPrmColor(default_color.r, default_color.g, default_color.b);
                mpSmoke0->setGlobalEnvColor(default_color.r, default_color.g, default_color.b);
            }
            if (mpSmoke1) {
                mpSmoke1->setGlobalPrmColor(default_color.r, default_color.g, default_color.b);
                mpSmoke1->setGlobalEnvColor(default_color.r, default_color.g, default_color.b);
            }
        }
    }
    return TRUE;
}

/* 000013B4-00001560       .text LiftUpRequest__Q214daObjMagmarock5Act_cFR4cXyz */
BOOL daObjMagmarock::Act_c::LiftUpRequest(cXyz& pos) {
    mLiftTarget = pos;
    if (!checkProcess(&Act_c::wait_proc)) {
        if (checkProcess(&Act_c::appear_proc)) {
            cXyz dir = current.pos - mLiftTarget;
            dir.y = 0.0f;
            if (!dir.normalizeRS()) {
                dir.set(0.0f, 0.0f, 1.0f);
            }
            dir *= 10.0f;
            current.pos += dir;
        }
        return FALSE;
    }
    cLib_addCalcPos2(&current.pos, pos, 0.05f, 5.0f);
    cLib_addCalc2(&mQuakeAmplitude, 750.0f, 0.5f, 40.0f);
    cLib_addCalcAngleS2(&mQuakeSpeed, 0x1200, 4, 0x100);
    mQuakeAngle += mQuakeSpeed;
    cLib_addCalc2(&current.pos.y, pos.y, 0.25f, 150.0f);
    mLiftRequested = 1;
    return TRUE;
}

/* 00001560-0000167C       .text BeforeLiftRequest__Q214daObjMagmarock5Act_cFR4cXyz */
BOOL daObjMagmarock::Act_c::BeforeLiftRequest(cXyz& pos) {
    mLiftTarget = pos;
    if (mLiftTarget.y < 25.0f + home.pos.y) {
        mLiftTarget.y = 25.0f + home.pos.y;
    }
    if (!checkProcess(&Act_c::wait_proc)) {
        return FALSE;
    }
    cLib_addCalcPos2(&current.pos, mLiftTarget, 0.05f, 5.0f);
    cLib_addCalc2(&mQuakeAmplitude, 500.0f, 0.25f, 20.0f);
    cLib_addCalcAngleS2(&mQuakeSpeed, 0xa00, 8, 0x100);
    mQuakeAngle += mQuakeSpeed;
    cLib_addCalc2(&current.pos.y, mLiftTarget.y, 0.25f, 150.0f);
    mLiftRequested = 1;
    mBeforeLiftRequested = 1;
    return TRUE;
}

/* 0000167C-000017DC       .text calc_ground_quat__Q214daObjMagmarock5Act_cFv */
void daObjMagmarock::Act_c::calc_ground_quat() {
    f32 height = dComIfGp_getMagma() ? dComIfGp_getMagma()->checkYpos(current.pos) : current.pos.y - 10.0f;
    if (height > -99999990.0f) {
        home.pos.y = 15.0f + (10.0f + height);
    }
    home.pos.x = current.pos.x;
    home.pos.z = current.pos.z;
    mGroundPoints[0].set(0.0f, 0.0f, 120.0f);
    mGroundPoints[1].set(103.9f, 0.0f, -60.0f);
    mGroundPoints[2].set(-103.9f, 0.0f, -60.0f);
    for (int i = 0; i < 3; i++) {
        mGroundPoints[i] += home.pos;
        height = dComIfGp_getMagma() ? dComIfGp_getMagma()->checkYpos(mGroundPoints[i]) : current.pos.y - 10.0f;
        if (height > -99999990.0f) {
            mGroundPoints[i].y = 15.0f + height;
        }
    }
    dLib_calc_QuatFromTriangle(&mGroundQuat, 0.25f, &mGroundPoints[0], &mGroundPoints[1], &mGroundPoints[2]);
}

/* 000017DC-0000198C       .text Create__Q214daObjMagmarock6MethodFPv */
cPhs_State daObjMagmarock::Method::Create(void* actor) {
#if VERSION == VERSION_DEMO
    cPhs_State phase;
    Act_c* self = static_cast<Act_c*>(actor);
    phase = dComIfG_resLoad(&self->mPhase, Act_c::M_arcname);
    fopAcM_SetupActor(self, Act_c);
#else
    Act_c* self = static_cast<Act_c*>(actor);
    fopAcM_SetupActor(self, Act_c);
    cPhs_State phase = dComIfG_resLoad(&self->mPhase, Act_c::M_arcname);
#endif
    if (phase == cPhs_COMPLEATE_e) {
        if (dComIfGp_getMagma() == NULL) {
            phase = cPhs_INIT_e;
        } else if (!fopAcM_entrySolidHeap(self, CheckCreateHeap, 0x5d40)) {
            phase = cPhs_ERROR_e;
        } else {
            self->CreateInit();
        }
    }
    return phase;
}

/* 00001A90-00001B14       .text Delete__Q214daObjMagmarock6MethodFPv */
BOOL daObjMagmarock::Method::Delete(void* actor) {
    Act_c* self = static_cast<Act_c*>(actor);
    dComIfG_resDeleteDemo(&self->mPhase, Act_c::M_arcname);
#if VERSION == VERSION_DEMO
    if (self->mpBgW->ChkUsed()) {
#else
    if (self->heap && self->mpBgW->ChkUsed()) {
#endif
        dComIfG_Bgsp()->Release(self->mpBgW);
    }
    return TRUE;
}

/* 00001B14-00001B38       .text Execute__Q214daObjMagmarock6MethodFPv */
BOOL daObjMagmarock::Method::Execute(void* actor) {
    return static_cast<Act_c*>(actor)->_execute();
}

/* 00001B38-00001EC0       .text _execute__Q214daObjMagmarock5Act_cFv */
bool daObjMagmarock::Act_c::_execute() {
    calc_ground_quat();
    if (!mLiftRequested) {
        if (!checkProcess(&Act_c::quake_proc) && !checkProcess(&Act_c::vanish_proc)) {
            cLib_addCalc2(&mQuakeAmplitude, 0.0f, 0.2f, 20.0f);
            cLib_addCalcAngleS2(&mQuakeSpeed, 0, 4, 0x100);
        }
        current.pos.y += speed.y;
        speed.y += gravity;
    } else {
        speed.y = 0.0f;
    }
    if (current.pos.y < 100.0f + home.pos.y) {
#if VERSION > VERSION_DEMO
        if (old.pos.y >= 100.0f + home.pos.y) {
            dComIfGp_getVibration().StartShock(4, 1, cXyz(0.0f, 1.0f, 0.0f));
        }
#endif
        if (current.pos.y < home.pos.y) {
            if (current.pos.y < home.pos.y - 30.0f) {
                current.pos.y = home.pos.y - 30.0f;
            }
            speed.y -= (0.4f + REG10_F(26)) * (current.pos.y - home.pos.y);
        }
        speed.y *= 0.65f - REG10_F(25);
    }
    if (!mLiftRequested && !checkProcess(&Act_c::stay_proc)) {
        mTimer--;
        mAnimTimer++;
    }
    set_mtx();
    demo_move();
    ControlEffect();
    mLiftRequested = 0;
    mBeforeLiftRequested = 0;
    (this->*mProcess)();
    play_anim();
    shape_angle.x = mQuakeAmplitude * cM_scos(mQuakeAngle);
    shape_angle.z = mQuakeAmplitude * cM_ssin(mQuakeAngle);
    if (!mRidden) {
        mRideTargetQuat = ZeroQuat;
    }
    Quaternion result;
    C_QUATSlerp(&mRideQuat, &mRideTargetQuat, &result, 0.25f);
    mRideQuat = result;
    mRidden = 0;
    if (mpBgW->ChkUsed()) {
        mpBgW->mIgnorePlaneType |= 4;
        mpBgW->Move();
    }
    return false;
}

inline bool daObjMagmarock::Act_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &mEffectTev);
    mEffectTev.mColorC0.r = (u8)(mEffectTev.mColorC0.r + (int)(0.12f * (255 - mEffectTev.mColorC0.r)));
    mEffectTev.mColorC0.g = (u8)(mEffectTev.mColorC0.g + (int)(0.12f * (255 - mEffectTev.mColorC0.g)));
    mEffectTev.mColorC0.b = (u8)(mEffectTev.mColorC0.b + (int)(0.12f * (255 - mEffectTev.mColorC0.b)));
    mEffectTev.mColorK0.r = (u8)(mEffectTev.mColorK0.r + (int)(0.12f * (255 - mEffectTev.mColorK0.r)));
    mEffectTev.mColorK0.g = (u8)(mEffectTev.mColorK0.g + (int)(0.12f * (255 - mEffectTev.mColorK0.g)));
    mEffectTev.mColorK0.b = (u8)(mEffectTev.mColorK0.b + (int)(0.12f * (255 - mEffectTev.mColorK0.b)));
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mBrk.entry(mpModel->getModelData(), (s16)mBrkFrame);
    mBck.entry(mpModel->getModelData(), (s16)mBckFrame);
    mDoExt_modelUpdateDL(mpModel);
    return true;
}

/* 00001EC0-00002128       .text Draw__Q214daObjMagmarock6MethodFPv */
BOOL daObjMagmarock::Method::Draw(void* actor) {
    return static_cast<Act_c*>(actor)->_draw();
}

/* 00002128-00002130       .text IsDelete__Q214daObjMagmarock6MethodFPv */
BOOL daObjMagmarock::Method::IsDelete(void*) {
    return TRUE;
}

actor_method_class daObjMagmarock::Method::Table = {
    (process_method_func)daObjMagmarock::Method::Create,
    (process_method_func)daObjMagmarock::Method::Delete,
    (process_method_func)daObjMagmarock::Method::Execute,
    (process_method_func)daObjMagmarock::Method::IsDelete,
    (process_method_func)daObjMagmarock::Method::Draw,
};

actor_process_profile_definition g_profile_Obj_Magmarock = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Magmarock_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjMagmarock::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Magmarock_e,
    /* Actor SubMtd */ &daObjMagmarock::Method::Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
