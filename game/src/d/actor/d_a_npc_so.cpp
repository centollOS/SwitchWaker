/**
 * d_a_npc_so.cpp
 * NPC - Fishman (blue fish)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_so.h"
#include "d/actor/d_a_esa.h"
#include "d/actor/d_a_tag_so.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_cc_d.h"
#include "d/d_lib.h"
#include "d/d_kankyo.h"
#include "d/d_s_play.h"
#include "d/d_camera.h"
#include "d/actor/d_a_ship.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "f_op/f_op_kankyo_mng.h"
#include "f_op/f_op_camera.h"
#include "m_Do/m_Do_mtx.h"
#include "JAZelAudio/JAIZelBasic.h"
#include "d/d_snap.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_lib.h"
#include "res/Object/So.h"
#include <string.h>

class daNpc_So_HIO_c : public mDoHIO_entry_c {
public:
    daNpc_So_HIO_c();
    virtual ~daNpc_So_HIO_c() {}

public:
    /* 0x04 */ dNpc_HIO_c mNpc;
    /* 0x2C */ u8 m2C;
    /* 0x2D */ u8 m2D;
    /* 0x2E */ u8 m2E;
    /* 0x2F */ u8 m2F;
    /* 0x30 */ u8 m30;
    /* 0x31 */ u8 m31;
    /* 0x32 */ u8 m32[0x34 - 0x32];
    /* 0x34 */ f32 m34;
    /* 0x38 */ f32 m38;
    /* 0x3C */ f32 m3C;
    /* 0x40 */ f32 m40;
    /* 0x44 */ f32 m44;
    /* 0x48 */ f32 m48;
    /* 0x4C */ f32 m4C;
    /* 0x50 */ f32 m50;
    /* 0x54 */ f32 m54;
    /* 0x58 */ f32 m58;
    /* 0x5C */ f32 m5C;
    /* 0x60 */ f32 m60;
    /* 0x64 */ s16 m64;
    /* 0x66 */ s16 m66;
    /* 0x68 */ s16 m68;
    /* 0x6A */ s16 m6A;
    /* 0x6C */ f32 m6C;
    /* 0x70 */ f32 m70;
    /* 0x74 */ f32 m74;
    /* 0x78 */ f32 m78;
    /* 0x7C */ s16 m7C;
    /* 0x7E */ u8 m7E[0x80 - 0x7E];
    /* 0x80 */ f32 m80;
    /* 0x84 */ f32 m84;
    /* 0x88 */ f32 m88;
    /* 0x8C */ f32 m8C;
    /* 0x90 */ f32 m90;
    /* 0x94 */ u8 m94;
    /* 0x95 */ u8 m95[0x98 - 0x95];
    /* 0x98 */ JntHit_HIO_c mJntHit;
};  // Size: 0xC4

STATIC_ASSERT(sizeof(daNpc_So_HIO_c) == DEMO_SELECT(0xC8, 0xC4));

static daNpc_So_HIO_c l_HIO;

const u32 daNpc_So_c::m_heapsize = DEMO_SELECT(0x1CC0, 0x1C00);
const char daNpc_So_c::m_arc_name[] = "So";

const dCcD_SrcSph daNpc_So_c::m_sph_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_UNK8 & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsEnemy_e | cCcD_CoSPrm_NoCrr_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ dCcG_SE_UNK23,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e | dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGSphS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 60.0f,
    }},
};

/* 000000EC-000002A4       .text __ct__14daNpc_So_HIO_cFv */
daNpc_So_HIO_c::daNpc_So_HIO_c() {
    m2C = 0;
    m2D = 0;
    m2E = 0;
    m2F = 0;
    m30 = 0;
    m31 = 0;
    m34 = 3.0f;
    m38 = 0.9f;
    m3C = 100.0f;
    m40 = 2000.0f;
    m54 = 20000.0f;
    m5C = 30.0f;
    m44 = 0.7f;
    m48 = 2000.0f;
    m4C = 2.0f;
    m50 = 150.0f;
    m64 = 0x1F40;
    m66 = 0xFA0;
    m68 = -0x1F40;
    m6A = -0xFA0;
    m6C = 10.0f;
    mNpc.m04 = -33.0f;
    mNpc.mMaxHeadX = 0x1FFE;
    mNpc.mMaxHeadY = 0x1000;
    mNpc.mMaxBackboneX = 0x1000;
    mNpc.mMaxBackboneY = 0x2000;
    mNpc.mMinHeadX = -0xBB8;
    mNpc.mMinHeadY = -0x1000;
    mNpc.mMinBackboneX = -0x1000;
    mNpc.mMinBackboneY = -0x2000;
    mNpc.mMaxTurnStep = 0x250;
    mNpc.mMaxHeadTurnVel = 0x150;
    mNpc.mAttnYOffset = 50.0f;
    mNpc.mMaxAttnAngleY = 0x7FFF;
    mNpc.m22 = 0;
    mNpc.mMaxAttnDistXZ = 400.0f;
    m58 = 250.0f;
    m60 = 700.0f;
    m70 = 600.0f;
    m74 = 300.0f;
    m78 = 100.0f;
    m7C = 5;
    m80 = -1.4f;
    m84 = 18.0f;
    m88 = 120.0f;
    m8C = 300.0f;
    m90 = 15.0f;
    m94 = 0;
}

/* 000002A4-000002D0       .text searchEsa_CB__FPvPv */
static void* searchEsa_CB(void* i_actor, void* i_this) {
    return static_cast<daNpc_So_c*>(i_this)->_searchEsa(static_cast<fopAc_ac_c*>(i_actor));
}

/* 000002D0-000002F8       .text _searchEsa__10daNpc_So_cFP10fopAc_ac_c */
fopAc_ac_c* daNpc_So_c::_searchEsa(fopAc_ac_c* actor) {
    if (fopAcM_GetName(actor) == fpcNm_ESA_e) {
        esa_class* esa = static_cast<esa_class*>(actor);
        if (esa->field_0x298 == 0) {
            return actor;
        }
    }
    return NULL;
}

/* 000002F8-00000344       .text nodeControl_CB__FP7J3DNodei */
static BOOL nodeControl_CB(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daNpc_So_c* i_this = reinterpret_cast<daNpc_So_c*>(model->getUserArea());
        if (i_this) {
            i_this->_nodeControl(node, model);
        }
    }
    return TRUE;
}

/* 00000344-00000474       .text _nodeControl__10daNpc_So_cFP7J3DNodeP8J3DModel */
void daNpc_So_c::_nodeControl(J3DNode* node, J3DModel* model) {
    J3DJoint* joint = static_cast<J3DJoint*>(node);
    int jntNo = joint->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));

    if (m_jnt.getHeadJntNum() == jntNo) {
        cXyz offsetAtt(0.0f, 0.0f, 0.0f);
#if VERSION == VERSION_DEMO
        cXyz offsetEye(24.0f, -16.0f, 0.0f);
        offsetEye.y = -16.0f + REG12_F(1);
        mDoMtx_stack_c::multVec(&offsetAtt, &mSoAttnPos);
        if (REG12_S(1) != 0) {
            mDoMtx_stack_c::XrotM(REG12_S(2));
            mDoMtx_stack_c::YrotM(REG12_S(3));
            mDoMtx_stack_c::ZrotM(REG12_S(4));
        } else {
            mDoMtx_stack_c::YrotM((int)m_jnt.getHead_y());
            mDoMtx_stack_c::ZrotM((int)m_jnt.getHead_x());
        }
        mDoMtx_stack_c::multVec(&offsetEye, &mSoEyePos);
    } else if (m_jnt.getBackboneJntNum() == jntNo) {
        if (REG12_S(0) != 0) {
            mDoMtx_stack_c::XrotM(REG12_S(2));
            mDoMtx_stack_c::YrotM(REG12_S(3));
            mDoMtx_stack_c::ZrotM(REG12_S(4));
        } else {
            mDoMtx_stack_c::XrotM((int)m_jnt.getBackbone_y());
            mDoMtx_stack_c::ZrotM((int)m_jnt.getBackbone_x());
        }
    }
#else
        cXyz offsetEye(24.0f, -16.0f, 0.0f);
        mDoMtx_stack_c::multVec(&offsetAtt, &mSoAttnPos);
        mDoMtx_stack_c::YrotM((int)m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM((int)m_jnt.getHead_x());
        mDoMtx_stack_c::multVec(&offsetEye, &mSoEyePos);
    } else if (m_jnt.getBackboneJntNum() == jntNo) {
        mDoMtx_stack_c::XrotM((int)m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM((int)m_jnt.getBackbone_x());
    }
#endif

    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
}

/* 000004B0-000004DC       .text searchTagSo_CB__FPvPv */
static void* searchTagSo_CB(void* i_actor, void* i_this) {
    return static_cast<daNpc_So_c*>(i_this)->_searchTagSo(static_cast<fopAc_ac_c*>(i_actor));
}

/* 000004DC-0000054C       .text _searchTagSo__10daNpc_So_cFP10fopAc_ac_c */
fopAc_ac_c* daNpc_So_c::_searchTagSo(fopAc_ac_c* actor) {
    if (fopAcM_GetName(actor) == fpcNm_TAG_SO_e) {
        daTag_So_c* tag = static_cast<daTag_So_c*>(actor);
#if VERSION == VERSION_DEMO
        u8 id = tag->m290;
        if (mTagId == id && tag->m298 != 1) {
#else
        if (mTagId == tag->m290 && tag->m298 != 1) {
#endif
            mTagRadius = tag->mRadius;
            mHidePos = tag->current.pos;
            current.pos = mHidePos;
            return actor;
        }
    }
    return NULL;
}

/* 0000054C-00000578       .text searchMinigameTagSo_CB__FPvPv */
static void* searchMinigameTagSo_CB(void* i_actor, void* i_this) {
    return static_cast<daNpc_So_c*>(i_this)->_searchMinigameTagSo(static_cast<fopAc_ac_c*>(i_actor));
}

/* 00000578-000005C8       .text _searchMinigameTagSo__10daNpc_So_cFP10fopAc_ac_c */
fopAc_ac_c* daNpc_So_c::_searchMinigameTagSo(fopAc_ac_c* actor) {
    if (fopAcM_GetName(actor) == fpcNm_TAG_SO_e) {
        daTag_So_c* tag = static_cast<daTag_So_c*>(actor);
        if (tag->m298 == 1) {
            mMinigamePos = tag->current.pos;
            mMinigameAngle = tag->shape_angle.y;
            mMinigameTagFound = 1;
            return actor;
        }
    }
    return NULL;
}

/* 000005C8-000005E8       .text daNpc_So_XyCheckCB__FPvi */
static s16 daNpc_So_XyCheckCB(void* i_this, int i_itemBtn) {
    return static_cast<daNpc_So_c*>(i_this)->XyCheckCB(i_itemBtn);
}

/* 000005E8-00000680       .text XyCheckCB__10daNpc_So_cFi */
s16 daNpc_So_c::XyCheckCB(int i_itemBtn) {
    if (fopAcIt_Judge(searchEsa_CB, this) != NULL) {
        return FALSE;
    }
    s32 type = mType;
    if (type != 2) {
        return FALSE;
    }
    if (dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e) &&
        dComIfGp_getSelectItem(i_itemBtn) == dItemNo_BIRD_BAIT_5_e)
    {
        return TRUE;
    }
    return FALSE;
}

/* 00000680-000006A0       .text daNpc_So_XyEventCB__FPvi */
static s16 daNpc_So_XyEventCB(void* i_this, int i_itemBtn) {
    return static_cast<daNpc_So_c*>(i_this)->XyEventCB(i_itemBtn);
}

/* 000006A0-000006EC       .text XyEventCB__10daNpc_So_cFi */
s16 daNpc_So_c::XyEventCB(int) {
    mEventIdx = dComIfGp_evmng_getEventIdx("SO_ESA_XY", 0xFF);
    return mEventIdx;
}

/* 000006EC-0000070C       .text createHeap_CB__FP10fopAc_ac_c */
static BOOL createHeap_CB(fopAc_ac_c* i_this) {
    return static_cast<daNpc_So_c*>(i_this)->_createHeap();
}

/* 0000070C-00000A20       .text _createHeap__10daNpc_So_cFv */
BOOL daNpc_So_c::_createHeap() {
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_SO_BDL_SO_e));
    JUT_ASSERT(DEMO_SELECT(0x219, 0x1FD), modelData != 0);

    mpMorf2 = new mDoExt_McaMorf(
        modelData,
        NULL, NULL, NULL,
        -1, 1.0f, 0, -1, 1,
        NULL,
        0x80000,
        0x11020022
    );
    if (mpMorf2 == NULL || mpMorf2->getModel() == NULL) {
        return FALSE;
    }
    mpMorf2->getModel()->setUserArea((u32)this);

#if VERSION > VERSION_DEMO
    J3DAnmTexPattern* btp = static_cast<J3DAnmTexPattern*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_SO_BTP_SO_e));
    JUT_ASSERT(0x210, btp != 0);

    if (!mBtpAnm.init(modelData, btp, TRUE, 0, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }
#endif

    m_jnt.setHeadJntNum(SO_JNT_HEAD_e);
    JUT_ASSERT(DEMO_SELECT(0x22C, 0x215), m_jnt.getHeadJntNum() >= 0);

    m_jnt.setBackboneJntNum(SO_JNT_BACKBONE_e);
    JUT_ASSERT(DEMO_SELECT(0x22E, 0x217), m_jnt.getBackboneJntNum() >= 0);

    modelData->getJointNodePointer(SO_JNT_HEAD_e)->setCallBack(nodeControl_CB);
    modelData->getJointNodePointer(SO_JNT_BACKBONE_e)->setCallBack(nodeControl_CB);

    modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_SO_BDL_SO_FUDE_e));
    JUT_ASSERT(DEMO_SELECT(0x238, 0x221), modelData != 0);

    mpHudeModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
    if (mpHudeModel == NULL) {
        return FALSE;
    }

    return jntHitCreateHeap() ? TRUE : FALSE;
}

/* 00000A20-00000A84       .text jntHitCreateHeap__10daNpc_So_cFv */
bool daNpc_So_c::jntHitCreateHeap() {
    static Vec sph_offset[] = {
        {0.0f, 0.0f, 0.0f},
    };
    static Vec cyl_offset_A[] = {
        {20.0f, -10.0f, 0.0f},
        {-20.0f, -10.0f, 0.0f},
    };
    static Vec cyl_offset_B[] = {
        {15.0f, 0.0f, 0.0f},
        {-15.0f, 0.0f, 0.0f},
    };
    static __jnt_hit_data_c search_data[] = {
#if VERSION == VERSION_DEMO
        {JntHitType_CYL_e, 1, 20.0f, cyl_offset_A},
        {JntHitType_CYL_e, 2, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 3, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 4, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 6, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 7, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 8, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 0x11, 15.0f, cyl_offset_A},
        {JntHitType_CYL_e, 0x12, 15.0f, cyl_offset_B},
        {JntHitType_CYL_e, 0x13, 5.0f, cyl_offset_B},
#else
        {JntHitType_CYL_e, 4, 2.0f, cyl_offset_B},
        {JntHitType_CYL_e, 8, 2.0f, cyl_offset_B},
#endif
    };

    mpJntHit = JntHit_create(mpMorf2->getModel(), search_data, ARRAY_SIZE(search_data));
    if (mpJntHit) {
        fopAcM_SetJntHit(this, mpJntHit);
    } else {
        return FALSE;
    }
    return TRUE;
}

#include "d/actor/d_a_npc_so_cut.inc"

/* 00000A84-00000C8C       .text checkTgHit__10daNpc_So_cFv */
bool daNpc_So_c::checkTgHit() {
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    mStts2.Move();
    if (cLib_calcTimer(&mHitTimer) == 0 && mSph.ChkTgHit()) {
        cXyz* hitPos = mSph.GetTgHitPosP();
        cCcD_Obj* hitObj = mSph.GetTgHitObj();
        mHitTimer = l_HIO.m7C;
        if (hitObj == NULL) {
            return FALSE;
        }
        switch (hitObj->GetAtType()) {
        case AT_TYPE_NORMAL_ARROW:
            fopAcM_seStart(this, JA_SE_LK_ARROW_HIT, 0x20);
            break;
        }
#if VERSION == VERSION_DEMO
        bool hit = true;
        if (hit) {
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, hitPos);
            cXyz scale(2.0f, 2.0f, 2.0f);
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, hitPos, &actor->shape_angle, &scale);
            fopAcM_seStart(this, JA_SE_LK_LAST_HIT, 0);
        }
#else
        fopAcM_monsSeStart(this, JA_SE_CV_SO_DAMAGE, 0);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, hitPos);
        cXyz scale(2.0f, 2.0f, 2.0f);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, hitPos, &actor->shape_angle, &scale);
        fopAcM_seStart(this, JA_SE_LK_LAST_HIT, 0);
#endif
        return TRUE;
    }
    return FALSE;
}

/* 00000C8C-00000CB0       .text offsetZero__10daNpc_So_cFv */
void daNpc_So_c::offsetZero() {
    mOffset.z = 0.1f;
    mOffset.y = 20.0f;
    mOffset.x = -1.0f;
}

/* 00000CB0-00000CD4       .text offsetDive__10daNpc_So_cFv */
void daNpc_So_c::offsetDive() {
    mOffset.z = 0.3f;
    mOffset.y = 10.0f;
    mOffset.x = -150.0f;
}

/* 00000CD4-00000CF8       .text offsetSwim__10daNpc_So_cFv */
void daNpc_So_c::offsetSwim() {
    mOffset.z = 0.3f;
    mOffset.y = 10.0f;
    mOffset.x = -40.0f;
}

/* 00000CF8-00000D1C       .text offsetAppear__10daNpc_So_cFv */
void daNpc_So_c::offsetAppear() {
    mOffset.z = 0.4f;
    mOffset.y = 22.0f;
#if VERSION == VERSION_DEMO
    mOffset.x = 110.0f + REG12_F(0);
#else
    mOffset.x = 110.0f;
#endif
}

/* 00000D1C-00000E40       .text getMsg__10daNpc_So_cFv */
u32 daNpc_So_c::getMsg() {
    if (mB0C != 0) {
#if VERSION == VERSION_DEMO
        if (l_HIO.m2E != 0 || mB7C == 10) {
#else
        if (l_HIO.m2E != 0 || mB7C >= 10) {
#endif
            if (mBD9 != 0) {
                return 0x32E2;
            }
            return 0x32DD;
        }
        if (mB7C == 0) {
            return 0x32E1;
        }
        if (mB7C == 1) {
            return 0x32E0;
        }
        dComIfGp_setMessageCountNumber(mB7C);
        return 0x32DF;
    }
    if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0901)) {
        if (strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo == 0xD) {
            if (mBD8 != 0) {
                return mPrmAngleX;
            }
            return 0x32CA;
        }
    }
    if (mBD8 != 0) {
        return mPrmAngleX;
    }
    return 0x32D0;
}

/* 00000E40-00001214       .text next_msgStatus__10daNpc_So_cFPUl */
u16 daNpc_So_c::next_msgStatus(u32* pMsgNo) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    if (*pMsgNo == (u32)mPrmAngleX) {
        if (mBD8 != 0) {
            if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0901) &&
                strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo == 0xD)
            {
                *pMsgNo = 0x32CE;
            } else {
                *pMsgNo = 0x32D2;
            }
        } else {
            *pMsgNo = 0x32D6;
        }
        return fopMsgStts_MSG_CONTINUES_e;
    }

    switch (*pMsgNo) {
    case 0x32CA:
        *pMsgNo = 0x32CB;
        break;
    case 0x32CB:
        *pMsgNo = 0x32CC;
        break;
    case 0x32CC:
        *pMsgNo = 0x32CD;
        break;
    case 0x32CE:
        status = fopMsgStts_MSG_ENDS_e;
        modeProcInit(MODE_EVENT_FIRST_END_e);
        break;
    case 0x32D0:
        if (dComIfGs_isSaveArriveGrid(current.roomNo - 1) || l_HIO.m2F != 0) {
            *pMsgNo = 0x32D4;
        } else {
            *pMsgNo = 0x32D1;
        }
        break;
    case 0x32CD:
    case 0x32D1:
        status = fopMsgStts_MSG_ENDS_e;
        modeProcInit(MODE_EVENT_MAPOPEN_e);
        break;
    case 0x32D4:
        *pMsgNo = mPrmAngleX;
        break;
    case 0x32D2:
        *pMsgNo = 0x32D3;
        break;
    case 0x32D6:
        fopAcIt_Judge(searchMinigameTagSo_CB, this);
        if (l_HIO.m30 != 0 || mMinigameTagFound != 0) {
            if (dComIfGs_getItem(dInvSlot_BOW_e) != 0xFF && dComIfG_getTimerPtr() == NULL) {
#if VERSION == VERSION_DEMO
                *pMsgNo = dLib_setFirstMsg(dSv_event_flag_c::UNK_3A10, 0x32D8, 0x32DC);
#else
                if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_3A10)) {
                    *pMsgNo = 0x32D8;
                } else {
                    *pMsgNo = 0x32DC;
                }
#endif
            } else {
                *pMsgNo = 0x32D7;
            }
        } else {
            *pMsgNo = 0x32D7;
        }
        break;
    case 0x32D8:
        if (mpCurrMsg->mSelectNum == 0) {
            *pMsgNo = 0x32DA;
        } else {
            *pMsgNo = 0x32D9;
        }
        break;
    case 0x32DC:
        if (mpCurrMsg->mSelectNum == 0) {
            *pMsgNo = 0x32DB;
        } else {
            *pMsgNo = 0x32D9;
        }
        break;
    case 0x32DA:
        *pMsgNo = 0x32DB;
        break;
    case 0x32DB:
        status = fopMsgStts_MSG_ENDS_e;
        modeProcInit(MODE_EVENT_BOW_e);
        break;
    case 0x32DD:
        *pMsgNo = 0x32DE;
        break;
    case 0x32DE:
        status = fopMsgStts_MSG_ENDS_e;
        modeProcInit(MODE_GET_RUPEE_e);
        break;
    case 0x32DF:
    case 0x32E0:
        dComIfGp_setItemRupeeCount(mB7C * 10);
        *pMsgNo = 0x32E2;
        break;
    case 0x32E1:
        *pMsgNo = 0x32E2;
        break;
    case 0x633:
        if (dComIfGs_getTriforceNum() == 8) {
            *pMsgNo = 0x635;
        } else {
            *pMsgNo = 0x634;
        }
        break;
    case 0x32D9:
        *pMsgNo = 0x32D7;
        break;
    default:
        status = fopMsgStts_MSG_ENDS_e;
        break;
    }
    return status;
}

/* 00001214-000013A0       .text lookBack__10daNpc_So_cFv */
void daNpc_So_c::lookBack() {
#if VERSION == VERSION_DEMO
    if (mEventCut.getAttnFlag()) {
        if (mType == 4) {
            m_jnt.clrTrn();
        } else {
            m_jnt.setTrn();
            mLookAt = mEventCut.getAttnPos();
        }
    } else {
        mLookAt = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }
#else
    if (dComIfGp_event_runCheck()) {
        if (mEventCut.getAttnFlag()) {
            if (mType == 4) {
                m_jnt.clrTrn();
            } else {
                m_jnt.setTrn();
                mLookAt = mEventCut.getAttnPos();
            }
        } else {
            mLookAt = dNpc_playerEyePos(l_HIO.mNpc.m04);
        }
    } else {
#if VERSION > VERSION_JPN
        m_jnt.clrTrn();
#endif
        mLookAt = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }
#endif

    if (m_jnt.trnChk()) {
        s16 maxVel = l_HIO.mNpc.mMaxHeadTurnVel;
        s16 turnSpd = mEventCut.getTurnSpeed();
        if (turnSpd != 0) {
            maxVel = turnSpd;
        }
        cLib_addCalcAngleS2(&mB50, maxVel, 4, 0x800);
    } else {
        mB50 = 0;
    }

    cXyz src = mSoEyePos;
#if VERSION == VERSION_DEMO
    src.y += 200.0f + REG12_F(0);
#else
    src.y += 200.0f;
#endif
    m_jnt.lookAtTarget(&shape_angle.y, &mLookAt, src, shape_angle.y, mB50, mBDA);
}

/* 000013A0-00001430       .text setAttention__10daNpc_So_cFv */
void daNpc_So_c::setAttention() {
    attention_info.position = mSoAttnPos;
    attention_info.position.y += l_HIO.mNpc.mAttnYOffset;
    f32 waterY = dLib_getWaterY(attention_info.position, mAcch2);
    if (attention_info.position.y <= waterY) {
        attention_info.position.y = waterY;
    }
    eyePos = mSoEyePos;
}

/* 00001430-00001524       .text setAnm__10daNpc_So_cFScb */
void daNpc_So_c::setAnm(signed char idx, bool force) {
    static const int a_anm_bcks_tbl[] = {
        0x09,
        0x06,
        0x07,
        0x05,
        0x08,
    };
    static const dLib_anm_prm_c a_anm_prm_tbl[] = {
        {0, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {0, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {3, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {4, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
    };

#if VERSION == VERSION_DEMO
    if (REG12_S(6) != 0) {
        idx = REG12_S(7);
    }
#endif
    if (idx != 6) {
        mType = idx;
    }
    if (mpMorf2->getFrame() >= mpMorf2->getEndFrame() - 1.0f) {
        if (cM_rndF(100.0f) < l_HIO.m5C) {
            if (mType == 5) {
                mType = 3;
            } else if (mType == 3) {
                mType = 5;
            }
        }
    }
    dLib_bcks_setAnm(m_arc_name, mpMorf2, &m6D2, &mType, &m6D4, a_anm_bcks_tbl, a_anm_prm_tbl, force);
}

/* 00001524-00001644       .text setAnmSwimSpeed__10daNpc_So_cFv */
void daNpc_So_c::setAnmSwimSpeed() {
    if (mType == 2) {
        f32 dist = (current.pos - old.pos).abs();
#if VERSION == VERSION_DEMO
        f32 playSpeed = dist / (10.0f + REG12_F(6));
#else
        f32 playSpeed = dist / 10.0f;
#endif
        if (playSpeed <= 0.0f) {
            playSpeed = 0.0f;
        } else if (playSpeed >= 1.0f) {
            playSpeed = 1.0f;
        }
        playSpeed *= l_HIO.m34;
        mpMorf2->setPlaySpeed(playSpeed < l_HIO.m38 ? l_HIO.m38 : playSpeed);
    }
}

/* 00001644-000016E8       .text setMtx__10daNpc_So_cFv */
void daNpc_So_c::setMtx() {
    J3DModel* model = mpMorf2->getModel();
    model->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle.x, shape_angle.y, shape_angle.z);
    mDoMtx_stack_c::transM(0.0f, mB34, 0.0f);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 000016E8-00001718       .text modeWaitInit__10daNpc_So_cFv */
void daNpc_So_c::modeWaitInit() {
    speedF = 0.0f;
    mSpeed = 0.0f;
    offsetAppear();
}

/* 00001718-0000171C       .text modeWait__10daNpc_So_cFv */
void daNpc_So_c::modeWait() {}

/* 0000171C-0000175C       .text modeHideInit__10daNpc_So_cFv */
void daNpc_So_c::modeHideInit() {
#if VERSION == VERSION_DEMO
    offsetDive();
#else
    mBDB = 1;
    offsetDive();
    mTagRadius = 0.0f;
#endif
}

/* 0000175C-00001778       .text modeHide__10daNpc_So_cFv */
void daNpc_So_c::modeHide() {
    current.pos = mHidePos;
}

/* 00001778-00001880       .text modeJumpInit__10daNpc_So_cFv */
void daNpc_So_c::modeJumpInit() {
    mSpeed = mB08 * (5.0f + cM_rndF(5.0f));
    speedF = mSpeed;
    speed.y = 30.0f * mB08 + 4.0f * speedF;
    if (speed.y > l_HIO.m50) {
        speed.y = l_HIO.m50;
    }
    mB00 = speed.y;
    shape_angle.x = l_HIO.m68;
    setAnm(4, false);
    m_jnt.onBackBoneLock();
    fopAcM_seStart(this, JA_SE_CM_SO_JUMP_L, 0);
}

/* 00001880-000019F0       .text modeJump__10daNpc_So_cFv */
void daNpc_So_c::modeJump() {
    f32 waterY = dLib_getWaterY(current.pos, mAcch2);
    if (current.pos.y < waterY) {
        fopAcM_seStart(this, JA_SE_CM_SO_LANDING_L, 0);
        fopKyM_createWpillar(&current.pos, 1.4f * scale.x, 1.4f, 0);
#if VERSION > VERSION_DEMO
        if ((mHidePos - current.pos).absXZ() > mTagRadius) {
            current.pos = mHidePos;
        }
#endif
        modeProcInit(MODE_SWIM_e);
    }
}

/* 000019F0-00001A6C       .text modeSwimInit__10daNpc_So_cFv */
void daNpc_So_c::modeSwimInit() {
#if VERSION == VERSION_DEMO
    mA90 = REG12_F(1) + (cM_rndF(90.0f) + 30.0f);
    setAnm(2, false);
    offsetSwim();
    m_jnt.onBackBoneLock();
#else
    attention_info.flags = fopAc_Attn_TALKFLAG_NOTALK_e | fopAc_Attn_ACTION_SPEAK_e | fopAc_Attn_LOCKON_TALK_e;
    mA90 = cM_rndF(90.0f) + 30.0f;
    setAnm(2, false);
    m_jnt.onBackBoneLock();
    offsetDive();
#endif
}

/* 00001A6C-00001DB8       .text modeSwim__10daNpc_So_cFv */
void daNpc_So_c::modeSwim() {
    mBDB = 0;
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    f32 distPlayer = (actor->current.pos - mHidePos).absXZ();
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship != NULL) {
        cLib_addCalc2(&mCirclePath.mRadius, 200.0f, 0.1f, 10.0f);
        mCirclePath.mWobbleAmplitude = 50.0f;
        mCirclePath.mAngleSpeed = 0x150;
        mCirclePath.mTranslation = mHidePos;
        mCirclePath.mTranslation.y = dLib_getWaterY(mCirclePath.mTranslation, mAcch2);
        dLib_setCirclePath(&mCirclePath);
        mCirclePath.mPos.y = dLib_getWaterY(mCirclePath.mPos, mAcch2);
        mCirclePath.mPos.y += mB34;
        f32 distPath = (current.pos - mCirclePath.mPos).absXZ();
#if VERSION == VERSION_DEMO
        if (distPath > 150.0f + REG12_F(7) || ship->speedF > 10.0f) {
            mSpeed = 12.0f + REG12_F(10);
#else
        if (distPath > 150.0f || ship->speedF > 10.0f) {
            mSpeed = 12.0f;
#endif
            s16 target = cLib_targetAngleY(&current.pos, &mCirclePath.mPos);
            cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
            mB04 = 0.0f;
        } else {
            cLib_addCalc2(&mB04, 1.0f, 0.01f, 0.05f);
            cXyz pathDelta = mCirclePath.mPos - current.pos;
            current.pos = current.pos + pathDelta * mB04;
#if VERSION == VERSION_DEMO
            cLib_addCalcAngleS2(&shape_angle.y, mCirclePath.mAngle + 0x8000, 4, 0x400);
#else
            s16 target = mCirclePath.mAngle + 0x8000;
            cLib_addCalcAngleS2(&shape_angle.y, target, 4, 0x400);
#endif
        }
        if (distPlayer < mTagRadius && dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e)) {
            modeProcInit(MODE_NEAR_SWIM_e);
        } else if (cLib_calcTimer(&mA90) == 0) {
            modeProcInit(MODE_JUMP_e);
        }
    }
}

/* 00001DB8-00001DFC       .text modeNearSwimInit__10daNpc_So_cFv */
void daNpc_So_c::modeNearSwimInit() {
    setAnm(2, false);
    offsetDive();
    m_jnt.onBackBoneLock();
}

/* 00001DFC-00002144       .text modeNearSwim__10daNpc_So_cFv */
void daNpc_So_c::modeNearSwim() {
    dCam_getBody()->SetTypeForce("BoatBattle", NULL);
    mB70 = 2;
    daShip_c* const ship = dComIfGp_getShipActor();
    if (ship != NULL) {
        fopAc_ac_c* actor = dComIfGp_getPlayer(0);
        f32 distPlayer = (actor->current.pos - mHidePos).absXZ();
        cLib_addCalc2(&mCirclePath.mRadius, 400.0f, 0.1f, 10.0f);
        mCirclePath.mWobbleAmplitude = 50.0f;
        mCirclePath.mAngleSpeed = 0x100;
        mCirclePath.mTranslation = actor->current.pos;
        mCirclePath.mTranslation.y = dLib_getWaterY(mCirclePath.mTranslation, mAcch2);
        dLib_setCirclePath(&mCirclePath);
        mCirclePath.mPos.y = dLib_getWaterY(mCirclePath.mPos, mAcch2);
        mCirclePath.mPos.y += mB34;
        f32 distPath = (current.pos - mCirclePath.mPos).absXZ();
#if VERSION == VERSION_DEMO
        if (distPath > 150.0f + REG12_F(7) || ship->speedF > 10.0f) {
            mSpeed = 12.0f + REG12_F(11);
#else
        if (distPath > 150.0f || ship->speedF > 10.0f) {
            mSpeed = 12.0f;
#endif
            s16 target = cLib_targetAngleY(&current.pos, &mCirclePath.mPos);
            cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
            mB04 = 0.0f;
            current.pos.y += mB34;
        } else {
            cLib_addCalc2(&mB04, 1.0f, 0.01f, 0.05f);
            cXyz pathDelta = mCirclePath.mPos - current.pos;
            current.pos = current.pos + pathDelta * mB04;
#if VERSION == VERSION_DEMO
            cLib_addCalcAngleS2(&shape_angle.y, mCirclePath.mAngle + 0x8000, 4, 0x400);
#else
            s16 target = mCirclePath.mAngle + 0x8000;
            cLib_addCalcAngleS2(&shape_angle.y, target, 4, 0x400);
#endif
            current.pos.y += mB34;
        }
        if (distPlayer >= mTagRadius) {
            modeProcInit(MODE_SWIM_e);
        }
    }
}

/* 00002144-0000217C       .text modeEventFirstWaitInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventFirstWaitInit() {
    mA90 = 150;
    mSpeed = 0.0f;
    speedF = 0.0f;
    offsetDive();
}

/* 0000217C-000022D0       .text modeEventFirstWait__10daNpc_So_cFv */
void daNpc_So_c::modeEventFirstWait() {
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship != NULL) {
        current.pos = ship->current.pos;
        f32 dist = (ship->current.pos - mAAC).absXZ();
        if (dist >= l_HIO.m54) {
            if (dComIfGs_checkGetItem(dItemNo_SAIL_e) && dComIfGs_isStageBossEnemy(3) &&
                dComIfGp_checkPlayerStatus0(0, daPyStts0_SHIP_RIDE_e))
            {
                modeProcInit(MODE_EVENT_FIRST_e);
            }
        }
    }
}

/* 000022D0-00002314       .text modeEventFirstInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventFirstInit() {
    offsetSwim();
    mSpeed = 0.0f;
    speedF = 0.0f;
    m_jnt.offBackBoneLock();
}

/* 00002314-000023C8       .text modeEventFirst__10daNpc_So_cFv */
void daNpc_So_c::modeEventFirst() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (!mEventCut.cutProc()) {
            cutProc();
        }
        if (dComIfGp_evmng_endCheck("SO_1ST_MEET")) {
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            mB70 = 1;
            modeProcInit(MODE_TALK_e);
            attention_info.flags = fopAc_Attn_TALKFLAG_NOTALK_e | fopAc_Attn_ACTION_SPEAK_e | fopAc_Attn_LOCKON_TALK_e;
        }
    } else {
        mB70 = 3;
    }
}

/* 000023C8-0000240C       .text modeEventFirstEndInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventFirstEndInit() {
    offsetAppear();
    mSpeed = 0.0f;
    speedF = 0.0f;
    m_jnt.offBackBoneLock();
}

/* 0000240C-000024C0       .text modeEventFirstEnd__10daNpc_So_cFv */
void daNpc_So_c::modeEventFirstEnd() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (!mEventCut.cutProc()) {
            cutProc();
        }
        if (dComIfGp_evmng_endCheck("SO_1ST_MEET_END")) {
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            modeProcInit(MODE_DISAPPEAR_e);
        }
    } else {
        if (talk(1) == fopMsgStts_BOX_CLOSED_e) {
            mB70 = 4;
        }
    }
}

/* 000024C0-00002504       .text modeEventEsaInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventEsaInit() {
    offsetSwim();
    mSpeed = 0.0f;
    speedF = 0.0f;
    m_jnt.offBackBoneLock();
}

/* 00002504-00002594       .text modeEventEsa__10daNpc_So_cFv */
void daNpc_So_c::modeEventEsa() {
    if (!mEventCut.cutProc()) {
        cutProc();
    }
    if (dComIfGp_evmng_endCheck(mEventIdx)) {
        dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
        mEventIdx = -1;
        mB70 = 1;
        modeProcInit(MODE_TALK_e);
    }
}

/* 00002594-000025B0       .text modeEventMapopenInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventMapopenInit() {
    mSpeed = 0.0f;
    speedF = 0.0f;
    m_jnt.offBackBoneLock();
}

/* 000025B0-00002670       .text modeEventMapopen__10daNpc_So_cFv */
void daNpc_So_c::modeEventMapopen() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (!mEventCut.cutProc()) {
            cutProc();
        }
        if (dComIfGp_evmng_endCheck("SO_MAPOPEN")) {
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            mB70 = 1;
            mBD8 = 1;
            modeProcInit(MODE_TALK_e);
        }
    } else {
        if (talk(1) == fopMsgStts_BOX_CLOSED_e) {
            mB70 = 5;
        }
    }
}

/* 00002670-000026B4       .text modeEventBowInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventBowInit() {
    offsetSwim();
    mSpeed = 0.0f;
    speedF = 0.0f;
    m_jnt.offBackBoneLock();
}

/* 000026B4-000027F8       .text modeEventBow__10daNpc_So_cFv */
void daNpc_So_c::modeEventBow() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (!mEventCut.cutProc()) {
            cutProc();
        }
        if (dComIfGp_evmng_endCheck("SO_BOW")) {
            mB0C = 1;
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            mBDB = 0;
#if VERSION > VERSION_DEMO
            dComIfGs_onEventBit(dSv_event_flag_c::UNK_3A10);
#endif
            camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
            camera->mCamera.Reset(mBCC, mBC0);
            camera->mCamera.Start();
            mB70 = 1;
            modeProcInit(MODE_TALK_e);
        }
    } else {
        if (talk(1) == fopMsgStts_BOX_CLOSED_e) {
            mB70 = 6;
        }
    }
}

/* 000027F8-0000283C       .text modeTalkInit__10daNpc_So_cFv */
void daNpc_So_c::modeTalkInit() {
    offsetAppear();
    setAnm(3, false);
    m_jnt.offBackBoneLock();
}

/* 0000283C-0000289C       .text modeTalk__10daNpc_So_cFv */
void daNpc_So_c::modeTalk() {
    if (talk(1) == fopMsgStts_BOX_CLOSED_e) {
#if VERSION == VERSION_DEMO
        if (mBD8 != 0 || mB0C != 0 || dComIfGs_isSaveArriveGrid(current.roomNo - 1)) {
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            modeProcInit(MODE_DISAPPEAR_e);
        } else {
            modeProcInit(MODE_WAIT_e);
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
        }
#else
        dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
        modeProcInit(MODE_DISAPPEAR_e);
#endif
    }
}

/* 0000289C-0000296C       .text modeDisappearInit__10daNpc_So_cFv */
void daNpc_So_c::modeDisappearInit() {
    if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0901)) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0901);
    }
    offsetDive();
    fopAcM_seStart(this, JA_SE_CM_SO_DIVE, 0);
    fopKyM_createWpillar(&current.pos, scale.x * 1.2f, 1.4f, 0);
    m_jnt.offBackBoneLock();
}

/* 0000296C-00002A90       .text modeDisappear__10daNpc_So_cFv */
void daNpc_So_c::modeDisappear() {
    if (std::fabsf(mB34 - mOffset.x) < 10.0f) {
        int oldTag = mTagId;
        fopAc_ac_c* tag;
        do {
            mTagId = cM_rndF(4.9f);
        } while ((int)mTagId == oldTag || (tag = (fopAc_ac_c*)fopAcIt_Judge(searchTagSo_CB, this)) == NULL);
        current.pos = tag->current.pos;
        mB74 = 0;
        mB78 = 0;
        mB7C = 0;
        mB80 = 0;
        mB84 = 0;
        mB88 = 0.0f;
        mB8C = 0.0f;
        mMinigamePos.setall(0.0f);
        mMinigameAngle = 0;
        mBA0.setall(0.0f);
        mBAC = 0;
        mB0C = 0;
        mBD9 = 0;
        mBD8 = 0;
        mBDB = 1;
        modeProcInit(MODE_HIDE_e);
#if VERSION == VERSION_DEMO
        modeProcInit(MODE_SWIM_e);
#endif
#if VERSION > VERSION_JPN
        gravity = -2.5f;
#endif
    }
}

/* 00002A90-00002B60       .text modeDebugInit__10daNpc_So_cFv */
void daNpc_So_c::modeDebugInit() {
    speedF = 0.0f;
    mSpeed = 0.0f;
    setAnm(1, false);
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    cXyz pos = actor->current.pos;
    pos.x += 100.0f * cM_scos(actor->shape_angle.y);
    pos.z += 100.0f * cM_ssin(actor->shape_angle.y);
    current.pos = pos;
    offsetAppear();
}

/* 00002B60-00002B64       .text modeDebug__10daNpc_So_cFv */
void daNpc_So_c::modeDebug() {}

/* 00002B64-00002B68       .text modeGetRupeeInit__10daNpc_So_cFv */
void daNpc_So_c::modeGetRupeeInit() {}

/* 00002B68-00002C6C       .text modeGetRupee__10daNpc_So_cFv */
void daNpc_So_c::modeGetRupee() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (!mEventCut.cutProc()) {
            cutProc();
        }
        if (dComIfGp_evmng_endCheck("SO_GET_RUPEE")) {
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            mB70 = 1;
            mBD9 = 1;
            modeProcInit(MODE_TALK_e);
        }
    } else {
        if (talk(1) == fopMsgStts_BOX_CLOSED_e) {
#if VERSION > VERSION_DEMO
            if (REG12_S(9) != 0) {
                dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            }
            daPy_getPlayerActorClass()->cancelOriginalDemo();
#endif
            mB70 = 7;
        }
    }
}

/* 00002C6C-00002CC0       .text modeEventTriForceInit__10daNpc_So_cFv */
void daNpc_So_c::modeEventTriForceInit() {
    mSpeed = 0.0f;
    speedF = 0.0f;
    m_jnt.offBackBoneLock();
    offsetAppear();
    setAnm(1, false);
}

/* 00002CC0-00002D80       .text modeEventTriForce__10daNpc_So_cFv */
void daNpc_So_c::modeEventTriForce() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (!mEventCut.cutProc()) {
            cutProc();
        }
        if (dComIfGp_evmng_endCheck("SO_TRIFORCE_CHECK")) {
            attention_info.flags = fopAc_Attn_TALKFLAG_NOTALK_e | fopAc_Attn_ACTION_SPEAK_e | fopAc_Attn_LOCKON_TALK_e;
            dComIfGp_event_onEventFlag(dEvtFlag_UNK8_e);
            modeProcInit(MODE_DISAPPEAR_e);
            dComIfGs_onEventBit(dSv_event_flag_c::UNK_3A20);
        }
    } else {
        mB70 = 8;
    }
}

/* 00002D80-00003110       .text modeProc__10daNpc_So_cFQ210daNpc_So_c6Proc_ei */
void daNpc_So_c::modeProc(daNpc_So_c::Proc_e proc, int newMode) {
    typedef void (daNpc_So_c::*mode_func_t)(void);
    struct mode_entry_t {
        mode_func_t init;
        mode_func_t run;
        const char* name;
    };

    static mode_entry_t mode_tbl[] = {
        {&daNpc_So_c::modeWaitInit, &daNpc_So_c::modeWait, "WAIT"},
        {&daNpc_So_c::modeHideInit, &daNpc_So_c::modeHide, "HIDE"},
        {&daNpc_So_c::modeJumpInit, &daNpc_So_c::modeJump, "JUMP"},
        {&daNpc_So_c::modeSwimInit, &daNpc_So_c::modeSwim, "SWIM"},
        {&daNpc_So_c::modeNearSwimInit, &daNpc_So_c::modeNearSwim, "NEAR_SWIM"},
        {&daNpc_So_c::modeEventFirstWaitInit, &daNpc_So_c::modeEventFirstWait, "EVENT_FIRST_WAIT"},
        {&daNpc_So_c::modeEventFirstInit, &daNpc_So_c::modeEventFirst, "EVENT_FIRST"},
        {&daNpc_So_c::modeEventFirstEndInit, &daNpc_So_c::modeEventFirstEnd, "EVENT_FIRST_END"},
        {&daNpc_So_c::modeEventEsaInit, &daNpc_So_c::modeEventEsa, "EVENT_ESA"},
        {&daNpc_So_c::modeEventMapopenInit, &daNpc_So_c::modeEventMapopen, "EVENT_MAPOPEN"},
        {&daNpc_So_c::modeEventBowInit, &daNpc_So_c::modeEventBow, "EVENT_BOW"},
        {&daNpc_So_c::modeTalkInit, &daNpc_So_c::modeTalk, "TALK"},
        {&daNpc_So_c::modeDisappearInit, &daNpc_So_c::modeDisappear, "DISAPPEAR"},
        {&daNpc_So_c::modeDebugInit, &daNpc_So_c::modeDebug, "DEBUG"},
        {&daNpc_So_c::modeGetRupeeInit, &daNpc_So_c::modeGetRupee, "GETRUPEE"},
        {&daNpc_So_c::modeEventTriForceInit, &daNpc_So_c::modeEventTriForce, "EVENT_TRIFORCE"},
    };

    if (proc == PROC_INIT_e) {
        mCurMode = newMode;
        (this->*mode_tbl[mCurMode].init)();
    } else if (proc == PROC_EXEC_e) {
        (this->*mode_tbl[mCurMode].run)();
    }
}

/* 00003110-00003264       .text eventOrder__10daNpc_So_cFv */
void daNpc_So_c::eventOrder() {
    static char* a_demo_name_tbl[] = {
        "SO_1ST_MEET",
        "SO_1ST_MEET_END",
        "SO_MAPOPEN",
        "SO_BOW",
        "SO_GET_RUPEE",
        "SO_TRIFORCE_CHECK",
    };

    if (mB70 == 1 || mB70 == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        eventInfo.onCondition(dEvtCnd_CANTALKITEM_e);
        if (mB70 == 1) {
            fopAcM_orderSpeakEvent(this);
        }
#if VERSION == VERSION_DEMO
    } else if (mB70 == 5 || mB70 == 4 || mB70 == 6 || mB70 == 7) {
        fopAcM_orderChangeEvent(this, a_demo_name_tbl[mB70 - 3], dEvtFlag_NOPARTNER_e, 0xFFFF);
#else
    } else if (mB70 == 5 || mB70 == 4 || mB70 == 6) {
        fopAcM_orderChangeEvent(this, a_demo_name_tbl[mB70 - 3], 0, 0xFFFF);
    } else if (mB70 == 7) {
        if (REG12_S(9) == 0) {
            fopAcM_orderChangeEvent(this, a_demo_name_tbl[mB70 - 3], 0, 0xFFFF);
            eventInfo.onCondition(dEvtCnd_CANGETITEM_e);
        } else {
            fopAcM_orderOtherEvent2(this, a_demo_name_tbl[mB70 - 3], dEvtFlag_NOPARTNER_e, 0xFFFF);
        }
#endif
    } else if (mB70 >= 3) {
        fopAcM_orderOtherEvent2(this, a_demo_name_tbl[mB70 - 3], dEvtFlag_NOPARTNER_e, 0xFFFF);
    }
}

/* 00003264-0000330C       .text checkOrder__10daNpc_So_cFv */
void daNpc_So_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        mB70 = 0;
    } else if (eventInfo.checkCommandTalk()) {
        if (mB70 == 1 || mB70 == 2) {
            if (dComIfGp_event_chkTalkXY()) {
                modeProcInit(MODE_EVENT_ESA_e);
            }
            mB70 = 0;
        }
    }
}

/* 0000330C-000033F4       .text setScale__10daNpc_So_cFv */
void daNpc_So_c::setScale() {
    f32 dist = fopAcM_searchPlayerDistanceXZ(this);
    if (dist > l_HIO.m48) {
#if VERSION == VERSION_DEMO
        f32 over = dist - l_HIO.m48;
        f32 step = (100000.0f - l_HIO.m48) / l_HIO.m4C;
        mB08 = 1.0f + over / step;
#else
        mB08 = 1.0f + (dist - l_HIO.m48) / ((100000.0f - l_HIO.m48) / l_HIO.m4C);
#endif
        if (mB08 > l_HIO.m4C) {
            mB08 = l_HIO.m4C;
        }
    } else {
        mB08 = l_HIO.m44;
    }
#if VERSION > VERSION_DEMO
    if (mCurMode == MODE_EVENT_TRIFORCE_e) {
        mB08 = 1.0f;
        scale.setall(mB08);
    }
#endif
    cXyz target;
    target.setall(mB08);
    cLib_addCalcPos2(&scale, target, 0.1f, 0.5f);
}

/* 000033F4-00003844       .text _execute__10daNpc_So_cFv */
bool daNpc_So_c::_execute() {
    fopAcM_setCullSizeBox(this, -100.0f * scale.x, -100.0f * scale.x, -100.0f * scale.x, 100.0f * scale.x, 100.0f * scale.x, 100.0f * scale.x);
    if (!dComIfGp_event_runCheck() && mAcch2.ChkGroundHit()) {
        current.pos.y = 0.0f;
        speedF = 0.0f;
        mSpeed = 0.0f;
        speed.y = 0.0f;
        mSpeed = 0.0f;
        modeProcInit(MODE_HIDE_e);
        return true;
    }

    m_jnt.setParam(
        l_HIO.mNpc.mMaxBackboneX, l_HIO.mNpc.mMaxBackboneY,
        l_HIO.mNpc.mMinBackboneX, l_HIO.mNpc.mMinBackboneY,
        l_HIO.mNpc.mMaxHeadX, l_HIO.mNpc.mMaxHeadY,
        l_HIO.mNpc.mMinHeadX, l_HIO.mNpc.mMinHeadY,
        l_HIO.mNpc.mMaxTurnStep
    );
    current.angle.y = shape_angle.y;
    if (mCurMode == MODE_HIDE_e) {
        if (mTagRadius == 0.0f) {
            fopAcIt_Judge(searchTagSo_CB, this);
        } else {
            modeProcInit(MODE_SWIM_e);
        }
    }
#if VERSION > VERSION_DEMO
    else {
        if (mTagRadius == 0.0f) {
            fopAcIt_Judge(searchTagSo_CB, this);
        }
    }
#endif

#if VERSION > VERSION_DEMO
    if (cLib_calcTimer(&mBtpTimer) == 0) {
        mBtpFrame++;
        if ((f32)mBtpFrame > (f32)mBtpAnm.getFrameCtrl()->getEnd()) {
            mBtpTimer = (s16)(cM_rndF(100.0f) + 100.0f);
            mBtpFrame = 0;
        }
    }
#endif

    setScale();
    setAttention();
    cLib_addCalc2(&speedF, mSpeed, 0.3f, 4.0f);
    cLib_addCalc2(&mB34, mOffset.x, mOffset.z, mOffset.y);
    lookBack();
    checkOrder();
    modeProc(PROC_EXEC_e, 0x10);
    eventOrder();

    s16 targetX = 0;
    f32 waterY = dLib_getWaterY(current.pos, mAcch2);
    if (current.pos.y < waterY) {
        current.pos.y = dLib_getWaterY(current.pos, mAcch2);
        if (mB34 > 0.0f && mRipple.getEmitter() == NULL) {
            static cXyz rippleScale(0.8f, 0.8f, 0.8f);
            dComIfGp_particle_setShipTail(dPa_name::ID_AK_JN_HAMON00, &current.pos, NULL, &rippleScale, 0xFF, &mRipple);
            if (mRipple.getEmitter() != NULL) {
                mRipple.setRate(0.0f);
            }
        }
    } else {
        f32 spdY = speed.y;
        f32 thresh = mB00 / 4.0f;
        if (spdY < -thresh) {
            if (spdY < -(mB00 / 2.0f)) {
                targetX = l_HIO.m64;
            } else {
                targetX = l_HIO.m66;
            }
        } else if (spdY > thresh) {
            if (spdY > mB00 / 2.0f) {
                targetX = l_HIO.m68;
            } else {
                targetX = l_HIO.m6A;
            }
        } else {
            targetX = 0;
        }
        mRipple.end();
    }
    cLib_addCalcAngleS2(&shape_angle.x, targetX, 4, 0x800);

    if (mCurMode != MODE_HIDE_e && mCurMode != MODE_EVENT_FIRST_WAIT_e && mBDB == 0
#if VERSION > VERSION_DEMO
        && cLib_calcTimer(&mBE0) == 0
#endif
    ) {
        fopAcM_posMoveF(this, NULL);
        mAcch2.CrrPos(*dComIfG_Bgsp());
    }

    mpMorf2->play(NULL, 0, 0);
    mpMorf2->calc();
    setMtx();
    setAnm(6, false);
    setAnmSwimSpeed();
    current.angle.y = shape_angle.y;
    return false;
}

/* 00003844-000038E0       .text debugDraw__10daNpc_So_cFv */
void daNpc_So_c::debugDraw() {
    GXColor unused[] = {
        {0x00, 0xFF, 0x00, 0x80},
        {0xFF, 0x00, 0x00, 0x80},
        {0x00, 0xFF, 0x00, 0x80},
        {0xFF, 0x00, 0x00, 0x80},
        {0x00, 0xFF, 0x00, 0x80},
        {0xFF, 0x00, 0x00, 0x80},
        {0x00, 0xFF, 0xFF, 0x80},
        {0xFF, 0xFF, 0x00, 0x80},
        {0xFF, 0xFF, 0x00, 0x80},
        {0x00, 0x00, 0xFF, 0x80},
        {0x00, 0x00, 0xFF, 0x80},
    }; // Unused colors, needed for the .rodata section to match.

    cXyz hide = mHidePos;
    hide.y += 20.0f;
    fopAc_ac_c* actor = dComIfGp_getPlayer(0);
    cXyz playerPos = actor->current.pos;
    playerPos.y += 20.0f;
    cXyz pos = current.pos;
    pos.y = playerPos.y;
    cXyz aac = mAAC;
    aac.y += 20.0f;
}

/* 000038E0-00003954       .text hudeDraw__10daNpc_So_cFv */
void daNpc_So_c::hudeDraw() {
    g_env_light.setLightTevColorType(mpHudeModel, &tevStr);
    mDoMtx_stack_c::copy(mpMorf2->getModel()->getAnmMtx(11));
    mpHudeModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoExt_modelUpdateDL(mpHudeModel);
}

/* 00003954-00003B00       .text _draw__10daNpc_So_cFv */
bool daNpc_So_c::_draw() {
    if (l_HIO.mNpc.m22 != 0) {
        debugDraw();
    }
    if (mCurMode == MODE_EVENT_FIRST_WAIT_e) {
        return true;
    }
    if (l_HIO.m31 == 0) {
        J3DModel* model = mpMorf2->getModel();
        J3DModelData* modelData = model->getModelData();
        g_env_light.settingTevStruct(0, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(model, &tevStr);
#if VERSION > VERSION_DEMO
        mBtpAnm.entry(modelData, mBtpFrame);
#endif
        mpMorf2->entryDL();
#if VERSION > VERSION_DEMO
        modelData->getMaterialTable().removeTexNoAnimator(mBtpAnm.getBtpAnm());
#endif
        if (mHudeFlag != 0 || l_HIO.m2C != 0) {
            hudeDraw();
        }
    }
    cXyz figPos = current.pos;
    figPos.y += mB34;
    dSnap_RegistFig(DSNAP_TYPE_UNK7D, this, figPos, shape_angle.y, 1.0f, 1.0f, 1.0f);
    cXyz shadowPos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(
        mShadowId,
        0,
        mpMorf2->getModel(),
        &shadowPos,
        800.0f,
        40.0f,
        current.pos.y + mB34,
        mAcch2.GetGroundH(),
        mAcch2.m_gnd,
        &tevStr,
        0,
        1.0f,
        dDlst_shadowControl_c::getSimpleTex()
    );
    return true;
}

/* 00003B00-00003DF8       .text createInit__10daNpc_So_cFv */
void daNpc_So_c::createInit() {
    mBDA = false;
    mStts.Init(DEMO_SELECT(100, 0xFF), DEMO_SELECT(100, 0xFF), this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);
    mStts2.Init(DEMO_SELECT(200, 0xFF), DEMO_SELECT(0, 0xFF), this);
    mSph.Set(m_sph_src);
    mSph.SetStts(&mStts2);
#if VERSION > VERSION_DEMO
    current.pos.y -= 500.0f;
#endif
    setMtx();
    mpMorf2->calc();
    mAAC = current.pos;
    offsetZero();
    setAnm(1, false);
    mTagId = cM_rndF(4.9f);
    if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0901) &&
        strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo == 0xD &&
        dComIfGs_isStageBossEnemy(3))
    {
        modeProcInit(MODE_EVENT_FIRST_WAIT_e);
    } else if (strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo == 0x4 &&
               dComIfGs_isStageBossEnemy(7) && dComIfGs_isCollect(0, 3) &&
               !dComIfGs_isEventBit(dSv_event_flag_c::UNK_3A20))
    {
        modeProcInit(MODE_EVENT_TRIFORCE_e);
    } else {
        attention_info.flags = fopAc_Attn_TALKFLAG_NOTALK_e | fopAc_Attn_ACTION_SPEAK_e | fopAc_Attn_LOCKON_TALK_e;
        modeProcInit(MODE_HIDE_e);
    }
#if VERSION > VERSION_DEMO
    mBE0 = 0x1E;
#endif
    mAcchCir2.SetWall(30.0f, 30.0f);
    mAcch2.Set(
        fopAcM_GetPosition_p(this),
        fopAcM_GetOldPosition_p(this),
        this,
        1,
        &mAcchCir2,
        fopAcM_GetSpeed_p(this),
        NULL,
        NULL
    );
    mAcch2.SetWallNone();
    mAcch2.SetRoofNone();
    fopAcM_SetMtx(this, mpMorf2->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(
        this,
        -100.0f * scale.x,
        -100.0f * scale.x,
        -100.0f * scale.x,
        100.0f * scale.x,
        100.0f * scale.x,
        100.0f * scale.x
    );
    cullSizeFar = 10.0f;
    gravity = -2.5f;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0x22;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0x22;
    eventInfo.setXyCheckCB(daNpc_So_XyCheckCB);
    eventInfo.setXyEventCB(daNpc_So_XyEventCB);
    mEventCut.setActorInfo2("NpcSo", this);
}

/* 00003DF8-00003E24       .text getArg__10daNpc_So_cFv */
void daNpc_So_c::getArg() {
    mPrmAngleX = home.angle.x;
    if (mPrmAngleX == 0xFFFF || mPrmAngleX == 0) {
        mPrmAngleX = 1;
    }
}

/* 00003E24-00003F38       .text _create__10daNpc_So_cFv */
cPhs_State daNpc_So_c::_create() {
    fopAcM_SetupActor(this, daNpc_So_c);

    cPhs_State result = dComIfG_resLoad(&mPhs, m_arc_name);
    if (result == cPhs_COMPLEATE_e) {
        getArg();
#if VERSION == VERSION_DEMO
        if (strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo == 0x1A &&
            !dComIfGs_isEventBit(dSv_event_flag_c::UNK_1E40))
        {
            return cPhs_ERROR_e;
        }
        if ((strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo != 0xD &&
             !dComIfGs_checkGetItem(dItemNo_SAIL_e)) ||
            !dComIfGs_isStageBossEnemy(3))
        {
            return cPhs_ERROR_e;
        }
#else
        if (strcmp(dComIfGp_getStartStageName(), "sea") == 0 && current.roomNo == 0x1A &&
            !dComIfGs_isEventBit(dSv_event_flag_c::UNK_1E40))
        {
            return cPhs_ERROR_e;
        }
        if (!dComIfGs_isStageBossEnemy(3)) {
            return cPhs_ERROR_e;
        }
#endif
        if (!fopAcM_entrySolidHeap(this, createHeap_CB, m_heapsize)) {
            return cPhs_ERROR_e;
        }
        createInit();
    }
    return result;
}

/* 000046E4-00004754       .text _delete__10daNpc_So_cFv */
bool daNpc_So_c::_delete() {
    if (dComIfGp_getMiniGameType() == 8) {
        dComIfGp_endMiniGame(8);
    }
    dComIfG_resDelete(&mPhs, m_arc_name);
    mRipple.end();
    return true;
}

/* 00004754-00004774       .text daNpc_SoCreate__FPv */
static cPhs_State daNpc_SoCreate(void* i_this) {
    return ((daNpc_So_c*)i_this)->_create();
}

/* 00004774-00004798       .text daNpc_SoDelete__FPv */
static BOOL daNpc_SoDelete(void* i_this) {
    return ((daNpc_So_c*)i_this)->_delete();
}

/* 00004798-000047BC       .text daNpc_SoExecute__FPv */
static BOOL daNpc_SoExecute(void* i_this) {
    return ((daNpc_So_c*)i_this)->_execute();
}

/* 000047BC-000047E0       .text daNpc_SoDraw__FPv */
static BOOL daNpc_SoDraw(void* i_this) {
    return ((daNpc_So_c*)i_this)->_draw();
}

/* 000047E0-000047E8       .text daNpc_SoIsDelete__FPv */
static BOOL daNpc_SoIsDelete(void*) {
    return TRUE;
}

static actor_method_class daNpc_SoMethodTable = {
    (process_method_func)daNpc_SoCreate,
    (process_method_func)daNpc_SoDelete,
    (process_method_func)daNpc_SoExecute,
    (process_method_func)daNpc_SoIsDelete,
    (process_method_func)daNpc_SoDraw,
};

actor_process_profile_definition g_profile_NPC_SO = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_SO_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_So_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_SO_e,
    /* Actor SubMtd */ &daNpc_SoMethodTable,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
