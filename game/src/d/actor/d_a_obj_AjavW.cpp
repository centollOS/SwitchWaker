/**
 * d_a_obj_AjavW.cpp
 * Object - Water inside Jabun's cave
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_AjavW.h"
#include "res/Object/AjavW.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "d/d_bg_w.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_hostIO.h"

#if VERSION == VERSION_DEMO
class daObjAjavW_HIO_c : public JORReflexible {
public:
    daObjAjavW_HIO_c();
    virtual ~daObjAjavW_HIO_c() {}

    void genMessage(JORMContext* ctx) { UNUSED(ctx); }

public:
    /* 0x04 */ s8 mNo;
    /* 0x05 */ u8 m05;
}; // size = 0x08
#endif

namespace {
    static const char l_arcname[] = "AjavW";
};

#if VERSION == VERSION_DEMO
static daObjAjavW_HIO_c l_HIO;

/* 000000EC-0000010C       .text __ct__16daObjAjavW_HIO_cFv */
daObjAjavW_HIO_c::daObjAjavW_HIO_c() {
    mNo = -1;
    m05 = 0;
}
#endif

/* 00000078-0000009C       .text solidHeapCB__12daObjAjavW_cFP10fopAc_ac_c */
BOOL daObjAjavW_c::solidHeapCB(fopAc_ac_c* i_this) {
    return ((daObjAjavW_c*)i_this)->create_heap();
}

/* 0000009C-00000208       .text create_heap__12daObjAjavW_cFv */
bool daObjAjavW_c::create_heap() {
    bool ret = true;

    J3DModelData* pModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(l_arcname, dRes_INDEX_AJAVW_BDL_AJAVW_e));
    J3DAnmTextureSRTKey * pBtk = (J3DAnmTextureSRTKey *)dComIfG_getObjectRes(l_arcname, dRes_INDEX_AJAVW_BTK_AJAVW_e);

    if (!pModelData || !pBtk) {
        JUT_ASSERT(DEMO_SELECT(0xa3, 0xa7), FALSE);
        ret = false;
    } else {
        mpModel = mDoExt_J3DModel__create(pModelData, 0x80000, 0x11000222);
        s32 btkRet = mBtkAnm.init(pModelData, pBtk, 1, J3DFrameCtrl::EMode_LOOP);
        mpBgW = dBgW_NewSet((cBgD_t*)dComIfG_getObjectRes(l_arcname, dRes_INDEX_AJAVW_DZB_AJAVW_e), cBgW::MOVE_BG_e, &mpModel->getBaseTRMtx());

        if (!mpModel || !btkRet || !mpBgW)
            ret = false;
    }

    return ret;
}

/* 00000208-00000350       .text _create__12daObjAjavW_cFv */
cPhs_State daObjAjavW_c::_create() {
    fopAcM_ct(this, daObjAjavW_c);

    cPhs_State ret = dComIfG_resLoad(&mPhs, l_arcname);

    if (ret == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0x8c0) == 1) {
            if (dComIfG_Bgsp()->Regist(mpBgW, this)) {
                ret = cPhs_ERROR_e;
            } else {
                fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
                mpModel->setBaseScale(scale);
                mDoMtx_stack_c::transS(current.pos);
                mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
                fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
            }
        } else {
            ret = cPhs_ERROR_e;
        }
    }

#if VERSION == VERSION_DEMO
    if (l_HIO.mNo < 0) {
        l_HIO.mNo = mDoHIO_createChild("ジャブ洞窟の水面", &l_HIO);
    }
#endif

    return ret;
}

/* 000003F4-00000488       .text _delete__12daObjAjavW_cFv */
bool daObjAjavW_c::_delete() {
    dComIfG_resDelete(&mPhs, l_arcname);

#if VERSION == VERSION_DEMO
    if (mpBgW != NULL && mpBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpBgW);
    }

    if (l_HIO.mNo >= 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
        l_HIO.mNo = -1;
    }
#else
    if (heap != NULL && mpBgW != NULL) {
        if (mpBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpBgW);
        }

        mpBgW = NULL;
    }
#endif

    return true;
}

/* 00000488-000004F4       .text _execute__12daObjAjavW_cFv */
bool daObjAjavW_c::_execute() {
    mBtkAnm.play();
    if (mpBgW != NULL && mpBgW->ChkUsed())
        mpBgW->Move();
    return true;
}

/* 000004F4-0000056C       .text _draw__12daObjAjavW_cFv */
bool daObjAjavW_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG1, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mBtkAnm.entry(mpModel->getModelData());
    mDoExt_modelUpdateDL(mpModel);
    return true;
}

/* 0000056C-0000058C       .text daObjAjavW_Create__FP12daObjAjavW_c */
static cPhs_State daObjAjavW_Create(daObjAjavW_c* i_this) {
    return i_this->_create();
}

/* 0000058C-000005B0       .text daObjAjavW_Delete__FP12daObjAjavW_c */
static BOOL daObjAjavW_Delete(daObjAjavW_c* i_this) {
    return i_this->_delete();
}

/* 000005B0-000005D4       .text daObjAjavW_Execute__FP12daObjAjavW_c */
static BOOL daObjAjavW_Execute(daObjAjavW_c* i_this) {
    return i_this->_execute();
}

/* 000005D4-000005F8       .text daObjAjavW_Draw__FP12daObjAjavW_c */
static BOOL daObjAjavW_Draw(daObjAjavW_c* i_this) {
    return i_this->_draw();
}

/* 000005F8-00000600       .text daObjAjavW_IsDelete__FP12daObjAjavW_c */
static BOOL daObjAjavW_IsDelete(daObjAjavW_c* i_this) {
    UNUSED(i_this);
    return TRUE;
}

static actor_method_class l_daObjAjavW_Method = {
    (process_method_func)daObjAjavW_Create,
    (process_method_func)daObjAjavW_Delete,
    (process_method_func)daObjAjavW_Execute,
    (process_method_func)daObjAjavW_IsDelete,
    (process_method_func)daObjAjavW_Draw,
};

actor_process_profile_definition g_profile_Obj_AjavW = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_AjavW_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjAjavW_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_AjavW_e,
    /* Actor SubMtd */ &l_daObjAjavW_Method,
    /* Status       */ fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
