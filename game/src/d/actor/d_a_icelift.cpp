/**
 * d_a_icelift.cpp
 * Object - Moving ice platform
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_icelift.h"
#include "d/d_bg_w.h"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Ylsic.h"
#include "res/Object/Yllic.h"

const char* daIlift_c::m_arcname[2] = {"Ylsic", "Yllic"};
const int daIlift_c::m_bmdidx[2] = {dRes_INDEX_YLSIC_BDL_YLSIC_e, dRes_INDEX_YLLIC_BDL_YLLIC_e};
const int daIlift_c::m_dzbidx[2] = {dRes_INDEX_YLSIC_DZB_YLSIC_e, dRes_INDEX_YLLIC_DZB_YLLIC_e};
const u32 daIlift_c::m_heapsize[2] = {0xAE0, 0x1B80};
const f32 daIlift_c::m_down_param = 4.0f;
const f32 daIlift_c::m_max_speed = 4.0f;

static void rideCallBack(dBgW*, fopAc_ac_c*, fopAc_ac_c*);

/* 00000078-000000E0       .text _delete__9daIlift_cFv */
bool daIlift_c::_delete() {
#if VERSION == VERSION_DEMO
    dComIfG_Bgsp()->Release(mBgW);
#else
    if (heap != NULL) dComIfG_Bgsp()->Release(mBgW);
#endif
    dComIfG_resDelete(&mPhs, m_arcname[mType]);
    return true;
}

/* 000000E0-00000100       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daIlift_c*)actor)->CreateHeap();
}

/* 00000100-00000284       .text CreateHeap__9daIlift_cFv */
BOOL daIlift_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname[mType], m_bmdidx[mType]);
    JUT_ASSERT(DEMO_SELECT(233, 235), modelData != NULL);
    mModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
    if (mModel == NULL) return FALSE;
    mBgW = new dBgW();
    if (mBgW != NULL) {
        if (mBgW->Set((cBgD_t*)dComIfG_getObjectRes(m_arcname[mType], m_dzbidx[mType]), dBgW::MOVE_BG_e, &mMtx) == true) return FALSE;
        mBgW->SetCrrFunc(dBgS_MoveBGProc_TypicalRotY);
        mBgW->SetRideCallback(rideCallBack);
    } else return FALSE;
    return TRUE;
}

/* 00000284-00000480       .text rideCallBack__FP4dBgWP10fopAc_ac_cP10fopAc_ac_c */
static void rideCallBack(dBgW*, fopAc_ac_c* actor, fopAc_ac_c* rider) {
    daIlift_c* lift = (daIlift_c*)actor;
    cXyz axis = rider->current.pos - lift->current.pos;
    lift->mRideOffset = axis;
    if (fopAcM_GetName(rider) == fpcNm_PLAYER_e) {
        lift->mRidden = 1;
        lift->mWaveTimer = 0;
        cXyz up(0.0f, 1.0f, 0.0f);
        axis = axis.outprod(up);
        lift->mRideDistance = axis.abs();
        if (axis.normalizeRS()) {
            f32 factor = 4.0f;
            f32 tilt = -lift->mRideDistance;
            lift->mTiltTarget = tilt * factor;
            cLib_addCalcAngleS2(&lift->mTiltAngle, lift->mTiltTarget, 8, 0x200);
            f32 sine = cM_ssin(lift->mTiltAngle);
            lift->mTargetQuat.x = axis.x * sine;
            lift->mTargetQuat.y = axis.y * sine;
            lift->mTargetQuat.z = axis.z * sine;
            lift->mTargetQuat.w = cM_scos(lift->mTiltAngle);
            lift->mRideStrength = 10.0f;
        }
    }
}

/* 00000480-00000680       .text CreateInit__9daIlift_cFv */
void daIlift_c::CreateInit() {
    fopAcM_SetMtx(this, mModel->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -200.0f, -250.0f, -200.0f, 200.0f, 250.0f, 200.0f);
    cullSizeFar = 1.0f;
    mAcchCir.SetWall(30.0f, 30.0f);
    mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    mAcch.ClrWaterNone();
    mAcch.ClrRoofNone();
    gravity = -5.0f;
    mTargetQuat = ZeroQuat;
    mQuat = mTargetQuat;
    mPathId = (fopAcM_GetParam(this) >> 4) & 0xFF;
    if (mPathId != 0xFF) {
        mPath = dPath_GetRoomPath(mPathId, fopAcM_GetRoomNo(this));
        if (mPath != NULL) {
            mPathDirection = 1;
            mPointIndex = 1;
            dPnt* point = &mPath->m_points[mPointIndex];
            mTargetPos.x = point->m_position.x;
            mTargetPos.y = point->m_position.y;
            mTargetPos.z = point->m_position.z;
            mPreviousTarget = mTargetPos;
            current.pos.x = mPath->m_points[0].m_position.x;
            current.pos.y = mPath->m_points[0].m_position.y;
            current.pos.z = mPath->m_points[0].m_position.z;
        } else mPathId = 0xFF;
    }
    mSwitch = (fopAcM_GetParam(this) >> 12) & 0xFF;
    dComIfG_Bgsp()->Regist(mBgW, this);
    set_mtx();
    mBgW->Move();
}

/* 00000680-00000794       .text _create__9daIlift_cFv */
cPhs_State daIlift_c::_create() {
    fopAcM_SetupActor(this, daIlift_c);
    mType = fopAcM_GetParam(this) & 0xF;
    if (mType >= 2) return cPhs_ERROR_e;
    cPhs_State phase = dComIfG_resLoad(&mPhs, m_arcname[mType]);
    if (phase == cPhs_COMPLEATE_e) {
        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, m_heapsize[mType])) return cPhs_ERROR_e;
        CreateInit();
    }
    return phase;
}

/* 00000804-0000089C       .text set_mtx__9daIlift_cFv */
void daIlift_c::set_mtx() {
    mModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::quatM(&mQuat);
    mModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoMtx_stack_c::scaleM(scale.x, scale.y, scale.x);
    MTXCopy(mDoMtx_stack_c::get(), mMtx);
}

/* 0000089C-000009C8       .text _execute__9daIlift_cFv */
bool daIlift_c::_execute() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    mMaxSpeed = 4.0f;
    mFrame++;
    mWaveTimer++;
    path_move();
    lift_wave();
    C_QUATSlerp(&mQuat, &mTargetQuat, &mQuat, 0.25f);
    mWasRidden = mRidden;
    mRidden = 0;
    if (mSwitch != 0xFF) {
        if (!fopAcM_isSwitch(this, mSwitch)) {
            mBgW->ChangeAttributeCodeByPathPntNo(0x40, 0x15);
            mBgW->ChangeAttributeCodeByPathPntNo(0x41, 9);
        } else {
            mBgW->ChangeAttributeCodeByPathPntNo(0x40, 0xF);
            mBgW->ChangeAttributeCodeByPathPntNo(0x41, 0);
        }
    }
    set_mtx();
    mBgW->Move();
    mPreviousPlayerPos = player->current.pos;
    return true;
}

/* 000009C8-00000C8C       .text lift_wave__9daIlift_cFv */
void daIlift_c::lift_wave() {
    cXyz up(0.0f, 1.0f, 0.0f);
    f32 strength = 10.0f;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!mRidden && mWasRidden) {
        f32 distance = (mPreviousPlayerPos - player->current.pos).absXZ();
        mTiltTarget = (distance / 18.0f) * (strength * (4.0f * -mRideDistance));
        mRideDistance = 0.0f;
    }
    if (!mRidden) {
        s16 target = (mTiltTarget * cM_scos(mWaveTimer * 0x400)) / mWaveTimer;
        cLib_addCalcAngleS(&mTiltAngle, target, 4, 0x800, 0x400);
        f32 sine = cM_ssin(mTiltAngle);
        cXyz axis = mRideOffset.outprod(up);
        axis = axis.normZP();
        if (axis != cXyz::Zero) {
            mTargetQuat.x = axis.x * sine;
            mTargetQuat.y = axis.y * sine;
            mTargetQuat.z = axis.z * sine;
            mTargetQuat.w = cM_scos(mTiltAngle);
        }
        s16 maxWaveTimer = 120;
        if (mWaveTimer > maxWaveTimer) mTiltTarget = 0;
    }
}

/* 00000C8C-00000CB8       .text path_move__9daIlift_cFv */
void daIlift_c::path_move() {
    if (mPathId != 0xFF) lift_normal_move();
}

/* 00000CB8-00000E5C       .text lift_normal_move__9daIlift_cFv */
void daIlift_c::lift_normal_move() {
    f32 near_dist = 50.0f;
    f32 div = 4.0f;
    f32 step = 1.0f;
    f32 decel = 2.2f;
    switch (mMoveState) {
    case 0:
        mMoveState = 1;
        mMoveTimer = 0;
        set_next_pnt();
    case 1:
        mMoveTimer++;
        if (cLib_addCalc(&mMoveSpeed, mMaxSpeed, 1.0f / div, step, 1.0f) == 0.0f) mMoveState = 2;
        break;
    case 2:
        mMoveTimer = 0;
        if ((current.pos - mTargetPos).abs() < near_dist) mMoveState = 3;
        break;
    case 3:
        mMoveTimer++;
        if (cLib_addCalc(&mMoveSpeed, mMaxSpeed / decel, step / div, step, step) == 0.0f) mMoveState = 0;
        break;
    }
    cLib_addCalcPos2(&current.pos, mTargetPos, 1.0f, mMoveSpeed);
}

/* 00000E5C-00000F58       .text set_next_pnt__9daIlift_cFv */
void daIlift_c::set_next_pnt() {
    if (mPathId != 0xFF) {
        mPointIndex += mPathDirection;
        if (dPath_ChkClose(mPath)) {
            if (mPointIndex > (s8)mPath->m_num - 1) mPointIndex = 0;
            else if (mPointIndex < 0) mPointIndex = mPath->m_num - 1;
        } else {
            if (mPointIndex > mPath->m_num - 1) {
                mPathDirection = -1;
                mPointIndex = mPath->m_num - 2;
            } else if (mPointIndex < 0) {
                mPathDirection = 1;
                mPointIndex = 1;
            }
        }
        mPreviousTarget = mTargetPos;
        dPnt* point = &mPath->m_points[mPointIndex];
        mTargetPos.x = point->m_position.x;
        mTargetPos.y = point->m_position.y;
        mTargetPos.z = point->m_position.z;
    }
}

/* 00000F58-00000FF8       .text _draw__9daIlift_cFv */
bool daIlift_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mModel, &tevStr);
    dComIfGd_setListBG();
    mDoExt_modelUpdateDL(mModel);
    dComIfGd_setList();
    return true;
}

/* 00000FF8-00001018       .text daIlift_Create__FPv */
static cPhs_State daIlift_Create(void* i_this) {
    return ((daIlift_c*)i_this)->_create();
}

/* 00001018-0000103C       .text daIlift_Delete__FPv */
static BOOL daIlift_Delete(void* i_this) {
    return ((daIlift_c*)i_this)->_delete();
}

/* 0000103C-00001060       .text daIlift_Draw__FPv */
static BOOL daIlift_Draw(void* i_this) {
    return ((daIlift_c*)i_this)->_draw();
}

/* 00001060-00001084       .text daIlift_Execute__FPv */
static BOOL daIlift_Execute(void* i_this) {
    return ((daIlift_c*)i_this)->_execute();
}

/* 00001084-0000108C       .text daIlift_IsDelete__FPv */
static BOOL daIlift_IsDelete(void*) {
    return TRUE;
}

static actor_method_class daIliftMethodTable = {
    (process_method_func)daIlift_Create,
    (process_method_func)daIlift_Delete,
    (process_method_func)daIlift_Execute,
    (process_method_func)daIlift_IsDelete,
    (process_method_func)daIlift_Draw,
};

actor_process_profile_definition g_profile_ICE_LIFT = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_ICE_LIFT_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daIlift_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_ICE_LIFT_e,
    /* Actor SubMtd */ &daIliftMethodTable,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
