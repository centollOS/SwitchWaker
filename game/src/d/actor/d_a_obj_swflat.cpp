/**
 * d_a_obj_swflat.cpp
 * Object - Tower of the Gods - Floor switch (requires statues to stay active)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_swflat.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Hfbot.h"
#include "m_Do/m_Do_hostIO.h"

class daObjSwflat_HIO_c : public JORReflexible {
public:
    daObjSwflat_HIO_c();
    virtual ~daObjSwflat_HIO_c() {}
    void genMessage(JORMContext* ctx) { UNUSED(ctx); }

public:
    /* 0x04 */ s8 mNo;
    /* 0x08 */ s32 mUsers;
    /* 0x0C */ f32 mRadius;
    /* 0x10 */ f32 mHeight;
    /* 0x14 */ s16 mEventTime;
    /* 0x16 */ s16 mFinishTime;
    /* 0x18 */ u8 mTune;
    /* 0x19 */ u8 mResetSwitches;
};

static daObjSwflat_HIO_c l_HIO;

namespace daObjSwflat {
static dCcD_SrcCyl l_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ 0,
        /* SrcObjTg  SPrm    */ 0,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e | cCcD_CoSPrm_NoCrr_e | cCcD_CoSPrm_NoCoHitInfSet_e,
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
        /* Radius */ 10.0f,
        /* Height */ 20.0f,
    }},
};
}


/* 000000EC-00000138       .text __ct__17daObjSwflat_HIO_cFv */
daObjSwflat_HIO_c::daObjSwflat_HIO_c() {
    mNo = -1;
    mUsers = 0;
    mTune = mResetSwitches = 0;
    mRadius = 10.0f;
    mHeight = 20.0f;
    mEventTime = 30;
    mFinishTime = 30;
}

const char daObjSwflat::Act_c::M_arcname[] = "Hfbot";
Mtx daObjSwflat::Act_c::M_tmp_mtx;

/* 00000138-00000280       .text CreateHeap__Q211daObjSwflat5Act_cFv */
BOOL daObjSwflat::Act_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_HFBOT_BDL_HFBOT1_e);
#if VERSION == VERSION_DEMO
    JUT_ASSERT(238, modelData != 0);
#else
    if (modelData == NULL) return FALSE;
#endif
    mpModel = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_HFBOT_BRK_HFBOT1_e);
#if VERSION == VERSION_DEMO
    JUT_ASSERT(245, brk != 0);
#else
    if (brk == NULL) return FALSE;
#endif
    mBrk = new mDoExt_brkAnm;
#if VERSION == VERSION_DEMO
    JUT_ASSERT(248, mBrk != 0);
#else
    if (mBrk == NULL) return FALSE;
#endif
    BOOL result = mBrk->init(modelData, brk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0);
    return mpModel != NULL && result;
}

/* 000002C8-00000630       .text Create__Q211daObjSwflat5Act_cFv */
BOOL daObjSwflat::Act_c::Create() {
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    init_mtx();
    fopAcM_setCullSizeSphere(this, 0.0f, 0.0f, 0.0f, 90.0f);
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    mType = prmGetType();
    mSwitch = prmGetSwNo();
    mSwitch2 = prmGetSwNo2();
    mFrame = 0;
    mActiveFrame = mBrk->getEndFrame() - 5.0f;
    mParticlePos = current.pos;
    mFinished = 0;
    if (l_HIO.mResetSwitches == 1) {
        if (mSwitch != 0xFF) fopAcM_offSwitch(this, mSwitch);
        if (mSwitch2 != 0xFF) fopAcM_offSwitch(this, mSwitch2);
    }
    u8 sw = mSwitch;
    if (mType == 2) {
        if (fopAcM_isSwitch(this, mSwitch2)) {
            mType = 0;
            sw = mSwitch2;
        } else {
            mType = 1;
        }
    }
    if (fopAcM_isSwitch(this, sw)) {
        mActive = mPrevActive = 1;
        if (mType != 0) {
            mpOnEmitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_SIRENBUTTONON, &mParticlePos, &shape_angle);
            mOnAlpha = 250;
        } else {
            mpOnEmitter = NULL;
            mOnAlpha = 0;
            mFinished = 1;
            mBrk->setFrame(mBrk->getEndFrame());
        }
        mpOffEmitter = NULL;
        mOffAlpha = 0;
    } else {
        mActive = mPrevActive = 0;
        mpOffEmitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_SIRENBUTTONOFF, &mParticlePos, &shape_angle);
        mOffAlpha = 250;
        mpOnEmitter = NULL;
        mOnAlpha = 0;
    }
    mEventState = 0;
    mEventTimer = 0;
    mFinishTimer = 0;
    if (l_HIO.mNo < 0) l_HIO.mNo = mDoHIO_createChild("フラットスイッチ", &l_HIO);
    l_HIO.mUsers++;
    return TRUE;
}

/* 00000630-000007BC       .text Mthd_Create__Q211daObjSwflat5Act_cFv */
cPhs_State daObjSwflat::Act_c::Mthd_Create() {
    fopAcM_SetupActor(this, Act_c);
    cPhs_State phase_state = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase_state == cPhs_COMPLEATE_e) {
        phase_state = MoveBGCreate(M_arcname, dRes_INDEX_HFBOT_DZB_HFBOT1_e, NULL, -1);
        JUT_ASSERT(385, (phase_state == cPhs_COMPLEATE_e) || (phase_state == cPhs_ERROR_e));
    }
    return phase_state;
}

/* 00000974-0000097C       .text Delete__Q211daObjSwflat5Act_cFv */
BOOL daObjSwflat::Act_c::Delete() {
    return TRUE;
}

/* 0000097C-00000A64       .text Mthd_Delete__Q211daObjSwflat5Act_cFv */
BOOL daObjSwflat::Act_c::Mthd_Delete() {
#if VERSION > VERSION_DEMO
    if (heap != NULL)
#endif
    {
        if (mpOffEmitter != NULL) {
            mpOffEmitter->becomeInvalidEmitter();
            mpOffEmitter = NULL;
        }
        if (mpOnEmitter != NULL) {
            mpOnEmitter->becomeInvalidEmitter();
            mpOnEmitter = NULL;
        }
    }
    if (l_HIO.mNo >= 0 && --l_HIO.mUsers == 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
        l_HIO.mNo = -1;
    }
    BOOL result = MoveBGDelete();
#if VERSION == VERSION_DEMO
    dComIfG_deleteObjectRes(M_arcname);
#else
    dComIfG_resDelete(&mPhase, M_arcname);
#endif
    return result;
}

/* 00000A64-00000AE4       .text set_mtx__Q211daObjSwflat5Act_cFv */
void daObjSwflat::Act_c::set_mtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    cMtx_copy(mDoMtx_stack_c::get(), M_tmp_mtx);
}

/* 00000AE4-00000B20       .text init_mtx__Q211daObjSwflat5Act_cFv */
void daObjSwflat::Act_c::init_mtx() {
    mpModel->setBaseScale(scale);
    set_mtx();
}

/* 00000B20-0000121C       .text Execute__Q211daObjSwflat5Act_cFPPA3_A4_f */
BOOL daObjSwflat::Act_c::Execute(Mtx** mtx) {
    bool playerHit = false;
    mCyl.SetC(current.pos);
    if (l_HIO.mTune != 0) {
        mCyl.SetR(l_HIO.mRadius);
        mCyl.SetH(l_HIO.mHeight);
    }
    dComIfG_Ccsp()->Set(&mCyl);
    bool hit = false;
    if (mCyl.ChkCoHit()) {
        fopAc_ac_c* actor = mCyl.GetCoHitAc();
        if (actor != NULL && fopAcM_CheckStatus(actor, fopAcStts_FREEZE_e)) {
            hit = true;
            if (fopAcM_GetProfName(actor) == fpcNm_PLAYER_e) playerHit = true;
        }
    }
    if (mEventTimer == 0) {
        if (hit) {
            mActive = 1;
            mCyl.SetR(15.0f);
        } else if (mType == 1) {
            mActive = 0;
            mCyl.SetR(10.0f);
        }
    }
    if (prmGetType() == 2 && fopAcM_isSwitch(this, mSwitch2)) {
        mType = 0;
        if (mFinished == 0) {
            mFinishTimer = l_HIO.mFinishTime;
            mFinished = 1;
        }
    }
    bool activated = false;
    if (mActive == 1) {
        if (mPrevActive == 0) mDoAud_seStart(JA_SE_OBJ_FLAT_SW_ON, &current.pos);
        if (mFinished == 0) mDoAud_seStart(JA_SE_OBJ_FLAT_SW_LIGHT, &current.pos);
        if (mFrame < mActiveFrame) {
            mFrame++;
            mBrk->setFrame(mFrame);
        }
        if (!playerHit) fopAcM_onSwitch(this, mSwitch);
        if (mFinished == 0) {
            if (mpOnEmitter == NULL) {
                mpOnEmitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_SIRENBUTTONON, &mParticlePos, &shape_angle);
                mOnAlpha = 250;
                activated = true;
                if (prmGetType() == 0) {
                    mFinishTimer = l_HIO.mFinishTime;
                    mFinished = 1;
                }
            } else if (mOnAlpha < 250) {
                mOnAlpha += 25;
                mpOnEmitter->setGlobalAlpha(mOnAlpha);
                fopAcM_onSwitch(this, mSwitch);
            }
        }
        if (mpOffEmitter != NULL) {
            mOffAlpha -= 25;
            mpOffEmitter->setGlobalAlpha(mOffAlpha);
            if (mOffAlpha == 0) {
                mpOffEmitter->becomeInvalidEmitter();
                mpOffEmitter = NULL;
            }
        }
    } else {
        if (mFrame > 0) {
            mFrame--;
            mBrk->setFrame(mFrame);
        }
        fopAcM_offSwitch(this, mSwitch);
        if (mpOffEmitter == NULL) {
            mpOffEmitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_SIRENBUTTONOFF, &mParticlePos, &shape_angle);
            mOffAlpha = 250;
        } else if (mOffAlpha < 250) {
            mOffAlpha += 25;
            mpOffEmitter->setGlobalAlpha(mOffAlpha);
        }
        if (mpOnEmitter != NULL) {
            mOnAlpha -= 25;
            mpOnEmitter->setGlobalAlpha(mOnAlpha);
            if (mOnAlpha == 0) {
                mpOnEmitter->becomeInvalidEmitter();
                mpOnEmitter = NULL;
            }
        }
    }
    switch (mEventState) {
    case 0:
        mEventTimer = 0;
        if (activated && playerHit) {
            mEventState = 1;
            mEventTimer = 30;
        }
        break;
    case 1:
        if (eventInfo.checkCommandDemoAccrpt()) {
            mEventState = 2;
            if (l_HIO.mTune != 0) mEventTimer = l_HIO.mEventTime;
        } else {
            fopAcM_orderPotentialEvent(this, 1, 0, 0);
            eventInfo.onCondition(2);
        }
        break;
    case 2:
        mEventTimer--;
        if (mEventTimer <= 1) {
            dComIfGp_event_reset();
            mEventState = 0;
            fopAcM_onSwitch(this, mSwitch);
        }
        break;
    }
    if (mFinished != 0) {
        mActive = 1;
        if (mFinishTimer != 0) {
            mFinishTimer--;
        } else {
            if (mpOnEmitter != NULL) {
                mOnAlpha -= 25;
                mpOnEmitter->setGlobalAlpha(mOnAlpha);
                if (mOnAlpha == 0) {
                    mpOnEmitter->becomeInvalidEmitter();
                    mpOnEmitter = NULL;
                }
            }
            if (mpOffEmitter != NULL) {
                mOffAlpha -= 25;
                mpOffEmitter->setGlobalAlpha(mOffAlpha);
                if (mOffAlpha == 0) {
                    mpOffEmitter->becomeInvalidEmitter();
                    mpOffEmitter = NULL;
                }
            }
            if (mFrame < mActiveFrame + 5) {
                mFrame++;
                mBrk->setFrame(mFrame);
            }
        }
    }
    set_mtx();
    *mtx = &M_tmp_mtx;
    mPrevActive = mActive;
    return TRUE;
}

/* 0000121C-000012D4       .text Draw__Q211daObjSwflat5Act_cFv */
BOOL daObjSwflat::Act_c::Draw() {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mBrk->entry(mpModel->getModelData());
    dComIfGd_setListBG();
    mDoExt_modelUpdateDL(mpModel);
    dComIfGd_setList();
    return TRUE;
}

/* 000012D4-000012DC       .text IsDelete__Q211daObjSwflat5Act_cFv */
BOOL daObjSwflat::Act_c::IsDelete() {
    return TRUE;
}

namespace daObjSwflat {
namespace {
/* 000012DC-000012FC       .text Mthd_Create__Q211daObjSwflat28@unnamed@d_a_obj_swflat_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjSwflat::Act_c*)i_this)->Mthd_Create();
}

/* 000012FC-0000131C       .text Mthd_Delete__Q211daObjSwflat28@unnamed@d_a_obj_swflat_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjSwflat::Act_c*)i_this)->Mthd_Delete();
}

/* 0000131C-0000133C       .text Mthd_Execute__Q211daObjSwflat28@unnamed@d_a_obj_swflat_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjSwflat::Act_c*)i_this)->MoveBGExecute();
}

/* 0000133C-00001368       .text Mthd_Draw__Q211daObjSwflat28@unnamed@d_a_obj_swflat_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjSwflat::Act_c*)i_this)->Draw();
}

/* 00001368-00001394       .text Mthd_IsDelete__Q211daObjSwflat28@unnamed@d_a_obj_swflat_cpp@FPv */
BOOL Mthd_IsDelete(void* i_this) {
    return ((daObjSwflat::Act_c*)i_this)->IsDelete();
}

static actor_method_class Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace
}; // namespace daObjSwflat

actor_process_profile_definition g_profile_Obj_Swflat = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0002,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Swflat_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjSwflat::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Swflat_e,
    /* Actor SubMtd */ &daObjSwflat::Mthd_Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLSPHERE_CUSTOM_e,
};
