/**
 * d_a_obj_balancelift.cpp
 * Object - Tower of the Gods - Pair of scales
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_balancelift.h"
#include "d/d_bg_w.h"
#include "d/d_cc_d.h"
#include "d/d_a_obj.h"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_hostIO.h"
#include "res/Object/Hten1.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DDrawBuffer.h"

class daBalancelift_HIO_c : public JORReflexible {
public:
    daBalancelift_HIO_c();
    virtual ~daBalancelift_HIO_c() {}
    void genMessage(JORMContext*) {}
    s8 mNo;
    f32 mAngle;
    f32 mSpring;
    f32 mRideForce;
    f32 mDamping;
    f32 mHitForce;
    f32 mChainDamping;
    f32 mChainSpring;
    f32 mTravel;
    f32 mMaxLength;
    cXyz mScale;
};
static daBalancelift_HIO_c l_HIO;
dChain_packet_c* dChain_packet_create(int, dKy_tevstr_c*, f32);
extern "C" void PSQUATInverse(const Quaternion*, Quaternion*);


static dCcD_SrcCyl l_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_BOOMERANG,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_GrpAll_e,
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
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCylS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 30.0f,
        /* Height */ 200.0f,
    }},
};


/* 000000EC-00000160       .text __ct__19daBalancelift_HIO_cFv */
daBalancelift_HIO_c::daBalancelift_HIO_c() {
    mNo = -1;
    mAngle = 45.0f;
    mSpring = 0.05f;
    mRideForce = 0.005f;
    mDamping = 0.9f;
    mHitForce = 25.0f;
    mChainDamping = 0.65f;
    mChainSpring = 0.9f;
    mTravel = 440.0f;
    mMaxLength = 1800.0f;
    mScale.setall(2.0f);
}

/* 0000019C-00000264       .text daObjBlift_ride_actor_check__FP10fopAc_ac_c */
static int daObjBlift_ride_actor_check(fopAc_ac_c* actor) {
    int weight = 0;
    if (fopAc_IsActor(actor)) {
        if (fopAcM_GetName(actor) == fpcNm_Obj_Try_e) {
            weight = 1;
        } else if (fopAcM_GetName(actor) == fpcNm_PLAYER_e) {
            daPy_py_c* player = static_cast<daPy_py_c*>(actor);
            ++weight;
            fpc_ProcID id = player->getGrabActorID();
            if (id != fpcM_ERROR_PROCESS_ID_e) {
                fopAc_ac_c* held = fopAcM_SearchByID(id);
                if (held) {
                    weight = daObjBlift_ride_actor_check(held) + 1;
                }
            }
        } else if (fopAcM_GetName(actor) == fpcNm_AM2_e ||
                   fopAcM_GetName(actor) == fpcNm_NPC_OS_e ||
                   fopAcM_GetName(actor) == fpcNm_NPC_CB1_e) {
            weight = 1;
        }
    }
    return weight;
}

/* 00000264-0000033C       .text ride_call_back__FP4dBgWP10fopAc_ac_cP10fopAc_ac_c */
static void ride_call_back(dBgW*, fopAc_ac_c* lift, fopAc_ac_c* rider) {
    daBalancelift_c* self = static_cast<daBalancelift_c*>(lift);
    int weight = daObjBlift_ride_actor_check(rider);
    cXyz pos = rider->current.pos;
    cXyz force = pos - self->mSwingPos;
    force *= l_HIO.mRideForce;
    self->mSwingVelocity += force;
    if (self->mSide == 0) {
        *self->mpWeight += weight;
    } else {
        *self->mpWeight -= weight;
    }
}

const char daBalancelift_c::M_arcname[] = "Hten1";

/* 0000033C-00000510       .text calc_quat__15daBalancelift_cFv */
void daBalancelift_c::calc_quat() {
    cXyz pos = current.pos;
    pos.y -= mLength;
    cXyz force = pos - mSwingPos;
    force *= l_HIO.mSpring;
    mSwingVelocity += force;
    mSwingVelocity *= l_HIO.mDamping;
    mSwingPos += mSwingVelocity;
    cXyz top = mTop;
    cXyz anchor = current.pos;
    top -= mSwingPos;
    anchor -= mSwingPos;
    Quaternion target, result;
    daObj::quat_rotBaseY2(&target, anchor);
    PSQUATInverse(&target, &target);
    f32 t1 = 0.1f + REG10_F(23);
    C_QUATSlerp(&mSwingQuat, &target, &result, t1);
    mSwingQuat = result;
    daObj::quat_rotBaseY2(&target, top);
    PSQUATInverse(&target, &target);
    f32 t2 = 0.15f + REG10_F(24);
    C_QUATSlerp(&mPlatformQuat, &target, &result, t2);
    mPlatformQuat = result;
}

/* 00000510-00000670       .text set_mtx__15daBalancelift_cFv */
void daBalancelift_c::set_mtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::quatM(&mSwingQuat);
    mDoMtx_stack_c::transM(0.0f, -mLength, 0.0f);
    cXyz top(0.0f, 300.0f, 0.0f);
    mDoMtx_stack_c::multVec(&top, &mTop);
    mDoMtx_stack_c::multVec(&mChainOffset, &mChainJoint);
    cXyz origin(0.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVec(&origin, &mPlatformPos);
    mDoMtx_stack_c::quatM(&mPlatformQuat);
    mDoMtx_stack_c::scaleM(l_HIO.mScale);
    M_mdl->setBaseTRMtx(mDoMtx_stack_c::get());
    MTXCopy(mDoMtx_stack_c::get(), mBgMtx);
    cXyz* points = m_chain->mPt;
    points[0] = mPlatformPos;
    points[1] = mChainJoint;
    points[2] = current.pos;
}

/* 00000670-00000984       .text calc_weight__15daBalancelift_cFv */
void daBalancelift_c::calc_weight() {
    f32 target;
    if (mSide == 0) {
        if (*mpWeight == 0) {
            target = l_HIO.mMaxLength - 0.5f * l_HIO.mTravel;
        } else if (*mpWeight > 0) {
            target = l_HIO.mMaxLength;
        } else {
            target = l_HIO.mMaxLength - l_HIO.mTravel;
        }
        *mpUpdateFlags |= 1;
    } else {
        if (*mpWeight == 0) {
            target = l_HIO.mMaxLength - 0.5f * l_HIO.mTravel;
        } else if (*mpWeight > 0) {
            target = l_HIO.mMaxLength - l_HIO.mTravel;
        } else {
            target = l_HIO.mMaxLength;
        }
        *mpUpdateFlags |= 2;
    }
    if (*mpUpdateFlags == 3) {
        *mpWeight = 0;
        *mpUpdateFlags = 0;
    }
    f32 length = std::sqrtf(target * target - mChainOffset.abs2XZ());
    mLengthVelocity += 0.005f * (length - mLength);
    mLengthVelocity *= 0.92f;
    mLength += mLengthVelocity;
    cXyz force = mChainTarget - mChainOffset;
    force *= l_HIO.mChainSpring;
    mChainVelocity += force;
    mChainVelocity *= l_HIO.mChainDamping;
    mChainOffset += mChainVelocity;
    // The original applies fabsf to the comparison result.
    if (std::fabsf(mLengthVelocity > 1.0f)) {
        mDoAud_seStart(JA_SE_OBJ_TENBIN_LIFT_MOVE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 00000984-000009A4       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return static_cast<daBalancelift_c*>(actor)->CreateHeap();
}

/* 000009A4-00000BA0       .text CreateHeap__15daBalancelift_cFv */
BOOL daBalancelift_c::CreateHeap() {
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_HTEN1_BDL_HTEN1_e));
    JUT_ASSERT(0x1a3, modelData != 0);
    M_mdl = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
    JUT_ASSERT(0x1a5, M_mdl != 0);
    m_chain = dChain_packet_create(3, &tevStr, 2.0f);
    JUT_ASSERT(0x1a7, m_chain != 0);
    pm_bgw = new dBgW;
    pm_bgw->Set(static_cast<cBgD_t*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_HTEN1_DZB_HTEN1_e)), 1, &mBgMtx);
    pm_bgw->SetCrrFunc(dBgS_MoveBGProc_Typical);
    JUT_ASSERT(0x1b0, pm_bgw != 0);
    return M_mdl != 0 && m_chain != 0 && pm_bgw != 0;
}

/* 00000BA0-00000F34       .text CreateInit__15daBalancelift_cFv */
cPhs_State daBalancelift_c::CreateInit() {
    u32 path = (fopAcM_GetParam(this) >> 16) & 0xff;
    if (path != 0xff) {
        mpPath = dPath_GetRoomPath(path, fopAcM_GetRoomNo(this));
        if (mpPath && mpPath->m_num != 0) {
            current.pos = mpPath->m_points[0].m_position;
            home.pos = current.pos;
            if (mpPath->m_num > 1) {
                cXyz pos = mpPath->m_points[1].m_position;
                parentActorID = fopAcM_createChild(fpcNm_Balancelift_e, fopAcM_GetID(this), -1, &pos, tevStr.mRoomNo, NULL, NULL, -1, NULL);
            }
        }
        mWeight = 0;
        mSide = 0;
        mpWeight = &mWeight;
        mUpdateFlags = 0;
        mpUpdateFlags = &mUpdateFlags;
        mLength = l_HIO.mMaxLength - 0.5f * l_HIO.mTravel;
    } else {
        mWeight = 0;
        mUpdateFlags = 0;
        if (parentActorID != fpcM_ERROR_PROCESS_ID_e) {
            daBalancelift_c* parent = static_cast<daBalancelift_c*>(fopAcM_SearchByID(parentActorID));
            if (parent) {
                mSide = 1;
                mpWeight = &parent->mWeight;
                mpUpdateFlags = &parent->mUpdateFlags;
            } else {
                mSide = 0;
                mpWeight = &mWeight;
                mpUpdateFlags = &mUpdateFlags;
            }
        } else {
            mSide = 0;
            mpWeight = &mWeight;
            mpUpdateFlags = &mUpdateFlags;
        }
        mLength = l_HIO.mMaxLength - 0.5f * l_HIO.mTravel;
    }
    mLengthVelocity = 0.0f;
    dComIfG_Bgsp()->Regist(pm_bgw, this);
    mSwingPos = current.pos;
    mSwingPos.y -= mLength;
    mSwingVelocity = cXyz::Zero;
    mChainVelocity = cXyz::Zero;
    mChainOffset = cXyz::Zero;
    mChainTarget = cXyz::Zero;
    mSwingQuat = ZeroQuat;
    mPlatformQuat = ZeroQuat;
    scale.setall(2.0f);
    set_mtx();
    pm_bgw->SetRideCallback(ride_call_back);
    mStts.Init(0xc0, 0xff, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    mCyl.OnTgShield();
    mCyl.OnTgNoConHit();
    if (l_HIO.mNo < 0) {
        l_HIO.mNo = mDoHIO_createChild("天秤リフト", &l_HIO);
    }
    return cPhs_COMPLEATE_e;
}

/* 00000F34-000010E0       .text daBalanceliftCreate__FPv */
inline cPhs_State daBalancelift_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        fopAcM_SetupActor(this, daBalancelift_c);
#else
    fopAcM_SetupActor(this, daBalancelift_c);
    cPhs_State phase = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
#endif
        if (fopAcM_entrySolidHeap(this, CheckCreateHeap, 0xe40)) {
            phase = CreateInit();
        } else {
            return cPhs_ERROR_e;
        }
        fopAcM_SetMtx(this, M_mdl->getBaseTRMtx());
        fopAcM_setCullSizeBox(this, -150.0f, -150.0f, -150.0f, 150.0f, 1000.0f, 150.0f);
        MTXCopy(M_mdl->getBaseTRMtx(), mBgMtx);
    }
    return phase;
}

static cPhs_State daBalanceliftCreate(void* actor) {
    return static_cast<daBalancelift_c*>(actor)->_create();
}

/* 0000129C-00001318       .text daBalanceliftDelete__FPv */
static BOOL daBalanceliftDelete(void* actor) {
    daBalancelift_c* self = static_cast<daBalancelift_c*>(actor);
    dComIfG_Bgsp()->Release(self->pm_bgw);
#if VERSION == VERSION_DEMO
    dComIfG_deleteObjectRes(daBalancelift_c::M_arcname);
#else
    dComIfG_resDelete(&self->mPhase, daBalancelift_c::M_arcname);
#endif
    if (l_HIO.mNo >= 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
        l_HIO.mNo = -1;
    }
    return TRUE;
}

/* 00001318-0000133C       .text daBalanceliftExecute__FPv */
static BOOL daBalanceliftExecute(void* i_this) {
    return ((daBalancelift_c*)i_this)->_execute();
}

/* 0000133C-000016F0       .text _execute__15daBalancelift_cFv */
bool daBalancelift_c::_execute() {
    calc_weight();
    calc_quat();
    set_mtx();
    pm_bgw->Move();
    mStts.Move();
    if (mCyl.ChkTgHit()) {
        fopAc_ac_c* actor = mCyl.GetTgHitAc();
        cCcD_Obj* obj = mCyl.GetTgHitObj();
        if (fopAc_IsActor(actor) && fopAcM_GetProfName(actor) == fpcNm_BOMB_e) {
            cXyz pos = mPlatformPos;
            pos.y -= 50.0f;
            cXyz dir = pos - actor->current.pos;
            if (!dir.normalizeRS()) {
                dir.set(0.0f, 0.0f, 1.0f);
            }
            mSwingVelocity += dir * -20.0f;
            mLengthVelocity += 2.0f * -dir.y;
        } else if (obj) {
            cXyz dir = *mCyl.GetTgRVecP();
            if (!dir.normalizeRS()) {
                dir.set(0.0f, 0.0f, 1.0f);
            }
            if (obj->ChkAtType(AT_TYPE_WIND)) {
                mSwingVelocity += dir * -12.0f;
                mLengthVelocity += 2.0f * -dir.y;
            } else if (obj->ChkAtType(AT_TYPE_SWORD)) {
                mSwingVelocity += dir * -8.0f;
                mLengthVelocity += 2.0f * -dir.y;
                cXyz* hitPos = mCyl.GetTgHitPosP();
                if (hitPos) {
                    cXyz delta = *hitPos - mPlatformPos;
                    f32 distance = delta.abs();
                    mChainVelocity += dir * l_HIO.mHitForce;
                    mChainOffset.y = mChainTarget.y = distance;
                    dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, hitPos);
                }
                mDoAud_seStart(JA_SE_OBJ_COL_SWS_NMTLP, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            }
        }
    }
    mCyl.SetC(mPlatformPos);
    dComIfG_Ccsp()->Set(&mCyl);
    return false;
}

/* 000016F0-00001774       .text daBalanceliftDraw__FPv */
inline bool daBalancelift_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(M_mdl, &tevStr);
    mDoExt_modelUpdateDL(M_mdl);
    j3dSys.getDrawBuffer(0)->entryImm(m_chain, 0);
    return true;
}

static BOOL daBalanceliftDraw(void* actor) {
    return static_cast<daBalancelift_c*>(actor)->_draw();
}

/* 00001774-0000177C       .text daBalanceliftIsDelete__FPv */
static BOOL daBalanceliftIsDelete(void*) {
    return TRUE;
}

static actor_method_class daBalanceliftMethodTable = {
    (process_method_func)daBalanceliftCreate,
    (process_method_func)daBalanceliftDelete,
    (process_method_func)daBalanceliftExecute,
    (process_method_func)daBalanceliftIsDelete,
    (process_method_func)daBalanceliftDraw,
};

actor_process_profile_definition g_profile_Balancelift = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Balancelift_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daBalancelift_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Balancelift_e,
    /* Actor SubMtd */ &daBalanceliftMethodTable,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
