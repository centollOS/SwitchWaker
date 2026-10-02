/**
 * d_a_obj_hami2.cpp
 * Object - Rotating gate
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_hami2.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Hami2.h"
#include <string.h>

const char daObjHami2::Act_c::M_arcname[] = "Hami2";
const char daObjHami2::Act_c::M_evname[] = "ami_cam";
Mtx daObjHami2::Act_c::M_tmp_mtx;

/* 00000078-0000012C       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int timing) {
    if (timing == 0) {
        int jointNo = ((J3DJoint*)node)->getJntNo();
        J3DModel* model = j3dSys.getModel();
        daObjHami2::Act_c* actor = (daObjHami2::Act_c*)model->getUserArea();
        if (actor != NULL) {
            MTXCopy(model->getAnmMtx(jointNo), *calc_mtx);
            cMtx_YrotM(*calc_mtx, actor->mGateAngle);
            model->setAnmMtx(jointNo, *calc_mtx);
            MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 0000012C-0000032C       .text CreateHeap__Q210daObjHami25Act_cFv */
BOOL daObjHami2::Act_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_HAMI2_BDL_HAMI2_e);
    JUT_ASSERT(DEMO_SELECT(103, 104), modelData != NULL);
    mModel = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
    if (mModel != NULL) {
        const char* jointName;
        JUTNameTab* names = mModel->getModelData()->getJointName();
        for (u16 i = 0; i < mModel->getModelData()->getJointNum(); i++) {
            jointName = names->getName(i);
            if (strcmp("mono1", jointName) == 0) {
                mModel->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
                break;
            }
        }
        mModel->setUserArea((u32)this);
    } else {
        return FALSE;
    }
    BOOL result = TRUE;
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::scaleM(scale);
    MTXCopy(mDoMtx_stack_c::get(), mStaticMtx);
    mStaticBg = new dBgW();
    if (mStaticBg == NULL || mStaticBg->Set((cBgD_t*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_HAMI2_DZB_HAMI2_e), dBgW::MOVE_BG_e, &mStaticMtx)) {
        result = FALSE;
    }
    if (result != TRUE) return FALSE;
    return TRUE;
}

/* 0000032C-0000042C       .text Create__Q210daObjHami25Act_cFv */
BOOL daObjHami2::Act_c::Create() {
    fopAcM_SetMtx(this, mModel->getBaseTRMtx());
    if (fopAcM_isSwitch(this, prm_get_swSave())) {
        mState = 3;
        mGateAngle = 0x4000;
    } else {
        mState = 0;
        mGateAngle = 0;
    }
    init_mtx();
    fopAcM_setCullSizeBox(this, -1200.0f, -40000.0f, -1200.0f, 1200.0f, 40000.0f, 1200.0f);
    mOpenEvent = dComIfGp_evmng_getEventIdx("AMI2_OPEN");
    mCloseEvent = dComIfGp_evmng_getEventIdx("AMI2_CLOSE");
    return TRUE;
}

/* 0000042C-00000540       .text Mthd_Create__Q210daObjHami25Act_cFv */
cPhs_State daObjHami2::Act_c::Mthd_Create() {
    fopAcM_SetupActor(this, Act_c);
    cPhs_State phase_state = dComIfG_resLoad(&mPhs, M_arcname);
    if (phase_state == cPhs_COMPLEATE_e) {
        phase_state = MoveBGCreate(M_arcname, dRes_INDEX_HAMI2_DZB_HAMI2B_e, dBgS_MoveBGProc_TypicalRotY, 0x34E0);
        dComIfG_Bgsp()->Regist(mStaticBg, this);
        JUT_ASSERT(DEMO_SELECT(199, 200), (phase_state == cPhs_COMPLEATE_e) || (phase_state == cPhs_ERROR_e));
    }
    return phase_state;
}

/* 00000540-00000548       .text Delete__Q210daObjHami25Act_cFv */
BOOL daObjHami2::Act_c::Delete() {
    return TRUE;
}

/* 00000548-000005E8       .text Mthd_Delete__Q210daObjHami25Act_cFv */
BOOL daObjHami2::Act_c::Mthd_Delete() {
#if VERSION == VERSION_DEMO
    dComIfG_Bgsp()->Release(mStaticBg);
#else
    if (heap != NULL && mStaticBg != NULL && mStaticBg->ChkUsed()) {
        dComIfG_Bgsp()->Release(mStaticBg);
    }
#endif
    BOOL result = MoveBGDelete();
    dComIfG_resDeleteDemo(&mPhs, M_arcname);
    return result;
}

/* 000005E8-00000678       .text set_mtx__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::set_mtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoMtx_stack_c::YrotM(mGateAngle);
    MTXCopy(mDoMtx_stack_c::get(), M_tmp_mtx);
}

/* 00000678-000006B4       .text init_mtx__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::init_mtx() {
    mModel->setBaseScale(scale);
    set_mtx();
}

/* 000006B4-00000730       .text daObjHami2_close_stop__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::daObjHami2_close_stop() {
    if (fopAcM_isSwitch(this, prm_get_swSave())) {
        fopAcM_orderOtherEventId(this, mOpenEvent);
        mState = 1;
    }
}

/* 00000730-00000810       .text daObjHami2_open_demo_wait__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::daObjHami2_open_demo_wait() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        mState = 2;
        fopAcM_seStartCurrent(this, JA_SE_OBJ_KAITEN_AMI_OPEN, 0);
        mDoAud_seStart(JA_SE_READ_RIDDLE_1);
    } else {
        fopAcM_orderOtherEventId(this, mOpenEvent);
    }
}

/* 00000810-000008A0       .text daObjHami2_open_demo__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::daObjHami2_open_demo() {
    mGateAngle += 0x100;
    if (mGateAngle >= 0x4000) {
        mGateAngle = 0x4000;
        mState = 3;
#if VERSION > VERSION_DEMO
        dComIfGp_getVibration().StartShock(4, -33, cXyz(0.0f, 1.0f, 0.0f));
#endif
        dComIfGp_event_reset();
    }
}

/* 000008A0-0000091C       .text daObjHami2_open_stop__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::daObjHami2_open_stop() {
    if (!fopAcM_isSwitch(this, prm_get_swSave())) {
        fopAcM_orderOtherEventId(this, mCloseEvent);
        mState = 4;
    }
}

/* 0000091C-0000096C       .text daObjHami2_close_demo_wait__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::daObjHami2_close_demo_wait() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        mState = 5;
    } else {
        fopAcM_orderOtherEventId(this, mCloseEvent);
    }
}

/* 0000096C-00000A08       .text daObjHami2_close_demo__Q210daObjHami25Act_cFv */
void daObjHami2::Act_c::daObjHami2_close_demo() {
    mGateAngle -= 0x100;
    if (mGateAngle <= 0) {
        mGateAngle = 0;
#if VERSION > VERSION_DEMO
        dComIfGp_getVibration().StartShock(4, -33, cXyz(0.0f, 1.0f, 0.0f));
#endif
        dComIfGp_event_reset();
        mState = 0;
    }
}

/* 00000A08-00000AB8       .text Execute__Q210daObjHami25Act_cFPPA3_A4_f */
BOOL daObjHami2::Act_c::Execute(Mtx** mtx) {
    switch (mState) {
    case 0: daObjHami2_close_stop(); break;
    case 1: daObjHami2_open_demo_wait(); break;
    case 2: daObjHami2_open_demo(); break;
    case 3: daObjHami2_open_stop(); break;
    case 4: daObjHami2_close_demo_wait(); break;
    case 5: daObjHami2_close_demo(); break;
    }
    set_mtx();
    *mtx = &M_tmp_mtx;
    return TRUE;
}

/* 00000AB8-00000B58       .text Draw__Q210daObjHami25Act_cFv */
BOOL daObjHami2::Act_c::Draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mModel, &tevStr);
    dComIfGd_setListBG();
    mDoExt_modelUpdateDL(mModel);
    dComIfGd_setList();
    return TRUE;
}

namespace daObjHami2 {
namespace {
/* 00000B58-00000B78       .text Mthd_Create__Q210daObjHami227@unnamed@d_a_obj_hami2_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjHami2::Act_c*)i_this)->Mthd_Create();
}

/* 00000B78-00000B98       .text Mthd_Delete__Q210daObjHami227@unnamed@d_a_obj_hami2_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjHami2::Act_c*)i_this)->Mthd_Delete();
}

/* 00000B98-00000BB8       .text Mthd_Execute__Q210daObjHami227@unnamed@d_a_obj_hami2_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjHami2::Act_c*)i_this)->MoveBGExecute();
}

/* 00000BB8-00000BE4       .text Mthd_Draw__Q210daObjHami227@unnamed@d_a_obj_hami2_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjHami2::Act_c*)i_this)->Draw();
}

/* 00000BE4-00000C10       .text Mthd_IsDelete__Q210daObjHami227@unnamed@d_a_obj_hami2_cpp@FPv */
BOOL Mthd_IsDelete(void* i_this) {
    return ((daObjHami2::Act_c*)i_this)->IsDelete();
}

static actor_method_class Mthd_Hami2 = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace
}; // namespace daObjHami2

actor_process_profile_definition g_profile_Obj_Hami2 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Hami2_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjHami2::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Hami2_e,
    /* Actor SubMtd */ &daObjHami2::Mthd_Hami2,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
