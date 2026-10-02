/**
 * d_a_swpropeller.cpp
 * Object - Wind-powered propeller switch
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_swpropeller.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_a_obj.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Hpbot1.h"
#include "res/Object/Vpbot_00.h"
#include <string.h>
#include <stdlib.h>

const char* daSwProp_c::m_arcname[2] = {"Hpbot1", "Vpbot_00"};
const s16 daSwProp_c::m_bdlidx[2] = {dRes_INDEX_HPBOT1_BDL_HPBOT1_e, dRes_INDEX_VPBOT_00_BDL_VPBOT_00_e};
const u32 daSwProp_c::m_heapsize[2] = {0x880, 0x8C0};

static BOOL nodeCallBack(J3DNode*, int);

static dCcD_SrcCyl l_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_BOOMERANG & ~AT_TYPE_FAN_SWING,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ dCcG_AtSPrm_NoConHit_e,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCylS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 100.0f,
        /* Height */ 80.0f,
    }},
};


/* 00000078-000000B8       .text _delete__10daSwProp_cFv */
bool daSwProp_c::_delete() {
    dComIfG_resDeleteDemo(&mPhs, m_arcname[mType]);
    return true;
}

/* 000000B8-000000D8       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daSwProp_c*)actor)->CreateHeap();
}

/* 000000D8-000001B8       .text CreateHeap__10daSwProp_cFv */
BOOL daSwProp_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname[mType], m_bdlidx[mType]);
    JUT_ASSERT(DEMO_SELECT(255, 257), modelData != NULL);
    mpModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
    if (mpModel == NULL) return FALSE;
    mpModel->setUserArea((u32)this);
    return TRUE;
}

/* 000001B8-00000350       .text CreateInit__10daSwProp_cFv */
void daSwProp_c::CreateInit() {
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -150.0f, -100.0f, -150.0f, 150.0f, 150.0f, 150.0f);
    cullSizeFar = 1.0f;
    mAcchCir.SetWall(30.0f, 30.0f);
    mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    mAcch.SetWallNone();
    mAcch.SetWaterNone();
    mAcch.SetRoofNone();
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    m648 = fopAcM_GetParam(this) & 0xFF;
    set_mtx();
    JUTNameTab* names = mpModel->getModelData()->getJointName();
    for (u16 i = 0; i < mpModel->getModelData()->getJointNum(); i++) {
        if (!strcmp("kaiten", names->getName(i))) {
            mpModel->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
            break;
        }
    }
    mpModel->calc();
}

/* 00000350-00000404       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int timing) {
    if (timing == 0) {
        int jointNo = ((J3DJoint*)node)->getJntNo();
        J3DModel* model = j3dSys.getModel();
        daSwProp_c* actor = (daSwProp_c*)model->getUserArea();
        if (actor != NULL) {
            actor->mRotY += actor->mRotYVel;
            mDoMtx_stack_c::copy(model->getAnmMtx(jointNo));
            mDoMtx_stack_c::YrotM(actor->mRotY);
            model->setAnmMtx(jointNo, mDoMtx_stack_c::get());
            MTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 00000404-00000590       .text _create__10daSwProp_cFv */
cPhs_State daSwProp_c::_create() {
    fopAcM_SetupActor(this, daSwProp_c);
    mType = (fopAcM_GetParam(this) >> 8) & 0xF;
    cPhs_State phase = dComIfG_resLoad(&mPhs, m_arcname[mType]);
    if (phase == cPhs_COMPLEATE_e) {
        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, m_heapsize[mType])) return cPhs_ERROR_e;
        CreateInit();
    }
    return phase;
}

/* 000007B8-00000838       .text set_mtx__10daSwProp_cFv */
void daSwProp_c::set_mtx() {
    mpModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00000838-00000B60       .text _execute__10daSwProp_cFv */
bool daSwProp_c::_execute() {
#if VERSION == VERSION_DEMO
    s16 maxRotYVel = 0x1000;
#endif
    u8 windHit = 0;
    mAcch.CrrPos(*dComIfG_Bgsp());
    if (mCyl.ChkTgHit()) {
        cCcD_Obj* hit = mCyl.GetTgHitObj();
        if (hit != NULL) {
            if (hit->ChkAtType(AT_TYPE_WIND)) {
                windHit = 1;
                mRebounding = 0;
                mDoAud_seStart(JA_SE_OBJ_PROP_SW_ON, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            } else if (hit->ChkAtType(0xFF1DFEFF)) {
                mRotYVel = 0x300;
                mTargetRotYVel = -0.45f * mRotYVel;
                mRebounding = 1;
                if (hit->ChkAtType(AT_TYPE_SWORD) || hit->ChkAtType(AT_TYPE_SKULL_HAMMER) ||
                    hit->ChkAtType(AT_TYPE_MOBLIN_SPEAR) || hit->ChkAtType(AT_TYPE_MACHETE)) {
                    if (mType == 1) daObj::HitSeStart(&current.pos, fopAcM_GetRoomNo(this), &mCyl, 0xB);
                    else if (mType == 0) daObj::HitSeStart(&current.pos, fopAcM_GetRoomNo(this), &mCyl, 0x11);
                }
            }
        }
    }
    if (windHit) {
        mRotYVel = DEMO_SELECT(maxRotYVel, 0x1000);
        mTargetRotYVel = 0;
    } else if (windHit != mWindHit) {
        fopAcM_revSwitch(this, m648);
    }
    s16 rate = 30;
    if (mRebounding) rate = 10;
    s16 result = cLib_addCalcAngleS(&mRotYVel, mTargetRotYVel, rate, 100, 10);
    if (mRebounding && result == 0) {
        mTargetRotYVel = -0.6f * mTargetRotYVel;
        if (abs(mTargetRotYVel) < 0x20) {
            mTargetRotYVel = 0;
            mRebounding = 0;
        }
    }
    if (mType == 1) mDoAud_seStart(JA_SE_OBJ_KM_WINDMILL, &current.pos, 100.0f * (mRotYVel / DEMO_SELECT((f32)maxRotYVel, 4096.0f)));
    mWindHit = windHit;
    set_mtx();
    cXyz center = current.pos;
    center.y += 50.0f;
    mCyl.SetC(center);
    dComIfG_Ccsp()->Set(&mCyl);
    return true;
}

/* 00000B60-00000C00       .text _draw__10daSwProp_cFv */
bool daSwProp_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mDoExt_modelUpdateDL(mpModel);
    f32 groundY = mAcch.GetGroundH();
    if (groundY != DEMO_SELECT(-G_CM3D_F_INF, -1.0e9f)) {
        dComIfGd_setSimpleShadow2(&current.pos, groundY, 65.0f, mAcch.m_gnd,
            0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }
    return true;
}

/* 00000C00-00000C20       .text daSwProp_Create__FPv */
static cPhs_State daSwProp_Create(void* i_this) {
    return ((daSwProp_c*)i_this)->_create();
}

/* 00000C20-00000C44       .text daSwProp_Delete__FPv */
static BOOL daSwProp_Delete(void* i_this) {
    return ((daSwProp_c*)i_this)->_delete();
}

/* 00000C44-00000C68       .text daSwProp_Draw__FPv */
static BOOL daSwProp_Draw(void* i_this) {
    return ((daSwProp_c*)i_this)->_draw();
}

/* 00000C68-00000C8C       .text daSwProp_Execute__FPv */
static BOOL daSwProp_Execute(void* i_this) {
    return ((daSwProp_c*)i_this)->_execute();
}

/* 00000C8C-00000C94       .text daSwProp_IsDelete__FPv */
static BOOL daSwProp_IsDelete(void*) {
    return TRUE;
}

static actor_method_class daSwPropMethodTable = {
    (process_method_func)daSwProp_Create,
    (process_method_func)daSwProp_Delete,
    (process_method_func)daSwProp_Execute,
    (process_method_func)daSwProp_IsDelete,
    (process_method_func)daSwProp_Draw,
};

actor_process_profile_definition g_profile_SW_PROPELLER = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_SW_PROPELLER_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daSwProp_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_SW_PROPELLER_e,
    /* Actor SubMtd */ &daSwPropMethodTable,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
