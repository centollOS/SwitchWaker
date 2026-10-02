/**
 * d_a_obj_ashut.cpp
 * Object - Metal bars (Pirate Ship rope minigame)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_ashut.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Ashut.h"

namespace daObjAshut {
namespace {
struct Attr_c {
    f32 height;
    f32 fallAccel;
    f32 fallDrag;
    u8 fallBounces;
    f32 fallBounce;
    f32 fallMaxBounce;
    f32 riseAccel;
    f32 riseDrag;
    f32 riseSlowDrag;
    u8 riseTime;
    u8 riseSlowTime;
    u8 riseBounces;
    f32 riseBounce;
    f32 riseMaxBounce;
};
static const Attr_c L_attr = {
    250.0f, -2.5f, 0.05f, 5, -0.5f, 7.0f,
    0.8f, 0.1f, 0.4f, 14, 4, 5, -0.6f, -4.0f,
};
inline const Attr_c& attr() { return L_attr; }
}
}

const char daObjAshut::Act_c::M_arcname[] = "Ashut";
Mtx daObjAshut::Act_c::M_tmp_mtx;

inline BOOL daObjAshut::Act_c::is_switch() {
    return fopAcM_isSwitch(this, prm_get_swSave());
}

/* 00000078-0000012C       .text CreateHeap__Q210daObjAshut5Act_cFv */
BOOL daObjAshut::Act_c::CreateHeap() {
    J3DModelData* mdl_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_ASHUT_BDL_ASHUT_e);
    JUT_ASSERT(249, mdl_data != NULL);
    mModel = mDoExt_J3DModel__create(mdl_data, 0x80000, 0x11000022);
    return mModel != NULL;
}

/* 0000012C-00000224       .text Create__Q210daObjAshut5Act_cFv */
BOOL daObjAshut::Act_c::Create() {
    mpBgW->SetPriority((cBgW::PRIORITY)1);
    fopAcM_SetMtx(this, mModel->getBaseTRMtx());
    init_mtx();
    fopAcM_setCullSizeBox(this, -80.0f, -5.0f, -30.0f, 80.0f, 250.0f, 30.0f);
    mDemoAccepted = false;
    mEventIdx = dComIfGp_evmng_getEventIdx(NULL, prm_get_evId());
    mDemoNext = Mode_NONE;
    if (is_switch()) mode_upper_init();
    else mode_lower_init();
    return TRUE;
}

/* 00000224-00000388       .text Mthd_Create__Q210daObjAshut5Act_cFv */
cPhs_State daObjAshut::Act_c::Mthd_Create() {
    fopAcM_SetupActor(this, Act_c);
    cPhs_State phase_state = dComIfG_resLoad(&mPhs, M_arcname);
    if (phase_state == cPhs_COMPLEATE_e) {
        phase_state = MoveBGCreate(M_arcname, dRes_INDEX_ASHUT_DZB_ASHUT_e, NULL, DEMO_SELECT(0x8000, 0x760));
        JUT_ASSERT(312, (phase_state == cPhs_COMPLEATE_e) || (phase_state == cPhs_ERROR_e));
        if (is_switch() && mpBgW->ChkUsed()) dComIfG_Bgsp()->Release(mpBgW);
    }
    return phase_state;
}

/* 00000388-00000390       .text Delete__Q210daObjAshut5Act_cFv */
BOOL daObjAshut::Act_c::Delete() {
    return TRUE;
}

/* 00000390-000003DC       .text Mthd_Delete__Q210daObjAshut5Act_cFv */
BOOL daObjAshut::Act_c::Mthd_Delete() {
    BOOL ret = MoveBGDelete();
    dComIfG_resDeleteDemo(&mPhs, M_arcname);
    return ret;
}

/* 000003DC-0000046C       .text set_mtx__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::set_mtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    MTXCopy(mDoMtx_stack_c::get(), M_tmp_mtx);
    mDoMtx_stack_c::get()[1][3] += mOffset;
    mModel->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 0000046C-000004A8       .text init_mtx__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::init_mtx() {
    mModel->setBaseScale(scale);
    set_mtx();
}

/* 000004A8-000005A4       .text chk_safe_area__Q210daObjAshut5Act_cCFv */
bool daObjAshut::Act_c::chk_safe_area() const {
    s16 angle = -shape_angle.y;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz delta = player->current.pos - current.pos;
    cXyz local;
    mDoMtx_stack_c::YrotS(angle);
    mDoMtx_stack_c::multVecSR(&delta, &local);
    return (std::fabsf(local.y) < 100.0f && std::fabsf(local.z) < 55.0f) && std::fabsf(local.x) < 105.0f;
}

/* 000005A4-000005BC       .text mode_upper_init__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_upper_init() {
    mMode = Mode_UPPER;
    mOffset = attr().height;
}

/* 000005BC-0000066C       .text mode_upper__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_upper() {
    if (!is_switch() && !chk_safe_area()) {
        if (!mpBgW->ChkUsed()) dComIfG_Bgsp()->Regist(mpBgW, this);
        mode_demoreq_init(Mode_U_L);
    }
}

/* 0000066C-00000700       .text mode_u_l_init__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_u_l_init() {
    mMode = Mode_U_L;
    mSpeed = 0.0f;
    mBounces = attr().fallBounces;
    mDoAud_seStart(JA_SE_OBJ_P_SHIP_SHTR_CL, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

/* 00000700-000007C0       .text mode_u_l__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_u_l() {
    mSpeed += attr().fallAccel;
    mSpeed *= 1.0f - attr().fallDrag;
    mOffset += mSpeed;
    if (mOffset <= 0.0f) {
        if (mBounces == 0) mode_lower_init();
        else {
            mBounces--;
            mOffset *= attr().fallBounce;
            mSpeed *= attr().fallBounce;
            if (mSpeed > attr().fallMaxBounce) mSpeed = attr().fallMaxBounce;
        }
    }
}

/* 000007C0-000007D8       .text mode_lower_init__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_lower_init() {
    mMode = Mode_LOWER;
    mOffset = 0.0f;
}

/* 000007D8-00000838       .text mode_lower__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_lower() {
    if (is_switch()) mode_demoreq_init(Mode_L_U);
}

/* 00000838-000008D4       .text mode_l_u_init__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_l_u_init() {
    mMode = Mode_L_U;
    mSpeed = 0.0f;
    mBounces = attr().riseBounces;
    mTimer = attr().riseTime;
    mDoAud_seStart(JA_SE_OBJ_P_SHIP_SHTR_OP, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

/* 000008D4-00000A50       .text mode_l_u__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_l_u() {
    if (mTimer > 0) mTimer--;
    mSpeed += attr().riseAccel;
    if (mTimer > 0 && mTimer < attr().riseSlowTime) mSpeed *= 1.0f - attr().riseSlowDrag;
    else mSpeed *= 1.0f - attr().riseDrag;
    mOffset += mSpeed;
    if (mOffset > 150.0f && mpBgW->ChkUsed()) dComIfG_Bgsp()->Release(mpBgW);
    if (mOffset >= attr().height) {
        if (mBounces == 0) mode_upper_init();
        else {
            mBounces--;
            mOffset = attr().height + (mOffset - attr().height) * attr().riseBounce;
            mSpeed *= attr().riseBounce;
            if (mSpeed < attr().riseMaxBounce) mSpeed = attr().riseMaxBounce;
        }
    }
}

/* 00000A50-00000B0C       .text mode_demoreq_init__Q210daObjAshut5Act_cFQ310daObjAshut5Act_c6Mode_e */
void daObjAshut::Act_c::mode_demoreq_init(daObjAshut::Act_c::Mode_e i_demo_next) {
    JUT_ASSERT(DEMO_SELECT(544, 546), (i_demo_next == Mode_U_L) || (i_demo_next == Mode_L_U));
    if (mDemoAccepted) {
        if (i_demo_next == Mode_U_L) mode_u_l_init();
        else mode_l_u_init();
    } else {
        mMode = Mode_DEMOREQ;
        mDemoNext = i_demo_next;
    }
}

/* 00000B0C-00000BE4       .text mode_demoreq__Q210daObjAshut5Act_cFv */
void daObjAshut::Act_c::mode_demoreq() {
    bool start = false;
    if (dComIfGp_evmng_existence(mEventIdx)) {
        if (eventInfo.checkCommandDemoAccrpt()) {
            start = true;
            mDemoAccepted = true;
        } else {
            fopAcM_orderOtherEventId(this, mEventIdx, prm_get_evId());
            eventInfo.onCondition(dEvtCnd_UNK2_e);
        }
    } else {
        start = true;
    }
    if (start) {
        if (mDemoNext == Mode_U_L) mode_u_l_init();
        else mode_l_u_init();
    }
}

/* 00000BE4-00000D0C       .text Execute__Q210daObjAshut5Act_cFPPA3_A4_f */
BOOL daObjAshut::Act_c::Execute(Mtx** mtx) {
    static void (Act_c::*mode_proc[])() = {
        &Act_c::mode_upper, &Act_c::mode_u_l, &Act_c::mode_lower,
        &Act_c::mode_l_u, &Act_c::mode_demoreq,
    };
    (this->*mode_proc[mMode])();
    eyePos.set(current.pos.x, current.pos.y + mOffset, current.pos.z);
    set_mtx();
    *mtx = &M_tmp_mtx;
    return TRUE;
}

/* 00000D0C-00000D6C       .text Draw__Q210daObjAshut5Act_cFv */
BOOL daObjAshut::Act_c::Draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mModel, &tevStr);
    mDoExt_modelUpdateDL(mModel);
    return TRUE;
}

namespace daObjAshut {
namespace {
/* 00000D6C-00000D8C       .text Mthd_Create__Q210daObjAshut27@unnamed@d_a_obj_ashut_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjAshut::Act_c*)i_this)->Mthd_Create();
}

/* 00000D8C-00000DAC       .text Mthd_Delete__Q210daObjAshut27@unnamed@d_a_obj_ashut_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjAshut::Act_c*)i_this)->Mthd_Delete();
}

/* 00000DAC-00000DCC       .text Mthd_Execute__Q210daObjAshut27@unnamed@d_a_obj_ashut_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjAshut::Act_c*)i_this)->MoveBGExecute();
}

/* 00000DCC-00000DF8       .text Mthd_Draw__Q210daObjAshut27@unnamed@d_a_obj_ashut_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjAshut::Act_c*)i_this)->MoveBGDraw();
}

/* 00000DF8-00000E24       .text Mthd_IsDelete__Q210daObjAshut27@unnamed@d_a_obj_ashut_cpp@FPv */
BOOL Mthd_IsDelete(void* i_this) {
    return ((daObjAshut::Act_c*)i_this)->MoveBGIsDelete();
}

static actor_method_class Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace
}; // namespace daObjAshut

actor_process_profile_definition g_profile_Obj_Ashut = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Ashut_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjAshut::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Ashut_e,
    /* Actor SubMtd */ &daObjAshut::Mthd_Table,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
