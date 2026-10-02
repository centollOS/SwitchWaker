/**
 * d_a_obj_htetu1.cpp
 * Object - Tower of the Gods - Large yellow gate (blocks room containing Small Key)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_htetu1.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_func.h"
#include "d/d_bg_s_wtr_chk.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "m_Do/m_Do_ext.h"
#include "res/Object/Htetu1.h"

static const u16 l_daObjHtetu1_splash_id_table[] = {
    dPa_name::ID_AK_SN_SIRENWATERGATE00,
    dPa_name::ID_AK_SN_SIRENWATERGATE01,
};
const char daObjHtetu1_c::M_arcname[] = "Htetu1";

/* 00000078-00000178       .text create_s__19daObjHtetu1Splash_cFUsP4cXyzP5csXyzP12dKy_tevstr_c */
void daObjHtetu1Splash_c::create_s(unsigned short id, cXyz* pos, csXyz* angle, dKy_tevstr_c* tev) {
    mPosition = *pos;
    mAngle = *angle;
#if VERSION == VERSION_DEMO
    mpEmitter = dComIfGp_particle_set(id, &mPosition, &mAngle, NULL, 255, &mSplashCb);
    mpEmitter->setGlobalPrmColor(tev->mColorC0.r, tev->mColorC0.g, tev->mColorC0.b);
#else
    dComIfGp_particle_set(id, &mPosition, &mAngle, NULL, 255, &mSplashCb);
    if (mSplashCb.getEmitter() != NULL) mSplashCb.getEmitter()->setGlobalPrmColor(tev->mColorC0.r, tev->mColorC0.g, tev->mColorC0.b);
#endif
    stop_particle();
    mPlaying = false;
    mTimer = -2;
}

/* 00000178-00000198       .text solidHeapCB__13daObjHtetu1_cFP10fopAc_ac_c */
BOOL daObjHtetu1_c::solidHeapCB(fopAc_ac_c* actor) {
    return static_cast<daObjHtetu1_c*>(actor)->create_heap();
}

/* 00000198-00000298       .text create_heap__13daObjHtetu1_cFv */
BOOL daObjHtetu1_c::create_heap() {
    BOOL result = TRUE;
    J3DModelData* mdl_data = static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_HTETU1_BDL_HTETU1_e));
    JUT_ASSERT(DEMO_SELECT(279, 281), mdl_data != NULL);
    if (mdl_data == NULL) {
        result = FALSE;
    } else {
        mpModel = mDoExt_J3DModel__create(mdl_data, 0, 0x11020203);
        mpBgW = dBgW_NewSet((cBgD_t*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_HTETU1_DZB_HTETU1_e), cBgW::MOVE_BG_e, &mpModel->getBaseTRMtx());
        if (mpBgW == NULL) {
            result = FALSE;
        }
    }
    return result;
}

/* 00000298-00000460       .text _create__13daObjHtetu1_cFv */
cPhs_State daObjHtetu1_c::_create() {
    fopAcM_SetupActor(this, daObjHtetu1_c);
    cPhs_State phase = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        phase = cPhs_ERROR_e;
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0xAE0)) {
            fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
            mSwitch = fopAcM_GetParam(this) & 0xFF;
            mBottomY = current.pos.y - 2300.0f;
            if (check_sw()) {
                current.pos.y = mBottomY;
                mEventState = 2;
            }
            mQuakeTimer = -1;
            init_mtx();
            g_env_light.settingTevStruct(TEV_TYPE_BG1, &current.pos, &tevStr);
            for (int i = 0; i < 2; i++) mSplash[i].create_s(l_daObjHtetu1_splash_id_table[i], &current.pos, &current.angle, &tevStr);
            fopAcM_setCullSizeBox(this, -950.0f, -1000.0f, -100.0f, 950.0f, 1300.0f, 100.0f);
            dComIfG_Bgsp()->Regist(mpBgW, this);
            mEventIdx = dComIfGp_evmng_getEventIdx("htetu1_open");
            phase = cPhs_COMPLEATE_e;
        }
    }
    return phase;
}

/* 00000508-00000610       .text _delete__13daObjHtetu1_cFv */
bool daObjHtetu1_c::_delete() {
    if (mQuakeTimer > 0) {
        dComIfGp_getVibration().StopQuake(-1);
        mQuakeTimer = -1;
    }
    for (int i = 0; i < 2; i++) mSplash[i].delete_s();
#if VERSION == VERSION_DEMO
    if (mpBgW != NULL && mpBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpBgW);
    }
#else
    if (heap != NULL && mpBgW != NULL && mpBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpBgW);
        mpBgW = NULL;
    }
#endif
    dComIfG_resDeleteDemo(&mPhase, M_arcname);
    return true;
}

/* 00000610-00000648       .text check_sw__13daObjHtetu1_cFv */
inline BOOL daObjHtetu1_c::check_sw() {
    return fopAcM_isSwitch(this, mSwitch);
}

/* 00000648-000006E4       .text init_mtx__13daObjHtetu1_cFv */
void daObjHtetu1_c::init_mtx() {
    mpModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpModel->calc();
}

/* 000006E4-000007F8       .text unlock__13daObjHtetu1_cFv */
void daObjHtetu1_c::unlock() {
    cXyz offset = cXyz::BaseY;
    mNextPos -= mShakeOffset;
    s16 angle = mShakeTimer * 0x859;
    offset *= std::fabsf((s16)(mShakeAmplitude * cM_ssin(angle)));
    mNextPos += offset;
    mShakeOffset = offset;
    cLib_addCalc(&mShakeAmplitude, 0.0f, 0.13f, 50.0f, 1.0f);
}

/* 000007F8-0000098C       .text get_water_h__13daObjHtetu1_cFv */
f32 daObjHtetu1_c::get_water_h() {
    dBgS_WtrChk chk;
    cXyz pos = home.pos;
    f32 height = current.pos.y;
    cXyz offset;
    mDoMtx_stack_c::YrotS(current.angle.y);
    mDoMtx_stack_c::multVec(&cXyz::BaseZ, &offset);
    offset *= 400.0f;
    pos += offset;
    if (dBgS_SplGrpChk_In_ObjGnd(pos, &chk, 1.0f)) height = chk.GetHeight();
    return height;
}

/* 00000AB0-00000BCC       .text splash_manager__13daObjHtetu1_cFv */
void daObjHtetu1_c::splash_manager() {
    f32 height = get_water_h();
    for (int i = 0; i < 2; i++) {
        mSplash[i].set_pos_y(height);
        s16 timer = mSplash[i].get_timer();
        if (timer == 0) {
            if (!mSplash[i].chk_stop()) mSplash[i].stop_particle();
        } else if (timer > 0 || timer == -1) {
            if (1400.0f + current.pos.y > height) {
                if (mSplash[i].chk_stop()) mSplash[i].play_particle();
            } else {
                if (!mSplash[i].chk_stop()) mSplash[i].stop_particle();
            }
            if (timer > 0) mSplash[i].sub_timer();
        }
    }
}

/* 00000BCC-0000101C       .text _execute__13daObjHtetu1_cFv */
bool daObjHtetu1_c::_execute() {
    int i;
    switch (mEventState) {
    case 0:
        if (check_sw()) {
            if (!eventInfo.checkCommandDemoAccrpt()) {
                fopAcM_orderOtherEventId(this, mEventIdx);
                eventInfo.onCondition(2);
            } else {
                mShakeAmplitude = 50.0f;
                mShakeTimer = 70;
                mShakeOffset = cXyz::BaseY * 50.0f;
                mDoAud_seStart(JA_SE_READ_RIDDLE_1);
                mMoveState = 1;
                mEventState = 1;
                dComIfGp_getVibration().StartShock(5, -33, cXyz(0.0f, 1.0f, 0.0f));
                for (i = 0; i < 2; i++) mSplash[i].timer_play_particle(30);
            }
        }
        break;
    case 1:
        if (dComIfGp_evmng_endCheck(mEventIdx)) {
            dComIfGp_event_reset();
            mEventState = 2;
        }
        break;
    case 2:
        break;
    }
    mNextPos = current.pos;
    switch (mMoveState) {
    case 0:
        break;
    case 1:
        unlock();
        if (mShakeTimer != 0) {
            mShakeTimer--;
            mDoAud_seStart(JA_SE_OBJ_ST_KOUSHI_MOVE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        } else {
            dComIfGp_getVibration().StartQuake(6, 3, cXyz(0.0f, 1.0f, 0.0f));
            mMoveState = 2;
            mQuakeTimer = 200;
            for (i = 0; i < 2; i++) mSplash[i].timer_play_particle(-1);
        }
        break;
    case 2:
        mNextPos.y -= 5.0f;
        mDoAud_seStart(JA_SE_OBJ_ST_KOUSHI_MOVE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        if (mNextPos.y <= mBottomY) {
            for (i = 0; i < 2; i++) mSplash[i].delete_s();
            mNextPos.y = mBottomY;
            mMoveState = 0;
        }
        break;
    }
    current.pos = mNextPos;
    init_mtx();
    splash_manager();
    s16 timer = mQuakeTimer;
    if (timer == 0) {
        dComIfGp_getVibration().StopQuake(-1);
        mQuakeTimer = -1;
    } else if (timer > 0) {
        mQuakeTimer--;
    }
#if VERSION == VERSION_DEMO
    if (mpBgW != NULL && mpBgW->ChkUsed()) mpBgW->Move();
#else
    if (heap != NULL && mpBgW != NULL && mpBgW->ChkUsed()) mpBgW->Move();
#endif
    return true;
}

/* 0000101C-0000107C       .text _draw__13daObjHtetu1_cFv */
bool daObjHtetu1_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mDoExt_modelUpdateDL(mpModel);
    return true;
}

namespace {
/* 0000107C-0000109C       .text Mthd_Create__28@unnamed@d_a_obj_htetu1_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjHtetu1_c*)i_this)->_create();
}

/* 0000109C-000010C0       .text Mthd_Delete__28@unnamed@d_a_obj_htetu1_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjHtetu1_c*)i_this)->_delete();
}

/* 000010C0-000010E4       .text Mthd_Execute__28@unnamed@d_a_obj_htetu1_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjHtetu1_c*)i_this)->_execute();
}

/* 000010E4-00001108       .text Mthd_Draw__28@unnamed@d_a_obj_htetu1_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjHtetu1_c*)i_this)->_draw();
}

/* 00001108-00001110       .text Mthd_IsDelete__28@unnamed@d_a_obj_htetu1_cpp@FPv */
BOOL Mthd_IsDelete(void*) {
    return TRUE;
}

static actor_method_class Htetu1_Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace

actor_process_profile_definition g_profile_Obj_Htetu1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Htetu1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjHtetu1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Htetu1_e,
    /* Actor SubMtd */ &Htetu1_Mthd_Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
