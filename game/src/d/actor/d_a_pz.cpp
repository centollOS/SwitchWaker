/**
 * d_a_pz.cpp
 * NPC - Princess Zelda
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_pz.h"
#include "d/d_cc_d.h"
#include "f_op/f_op_actor_mng.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s.h"
#include "m_Do/m_Do_mtx.h"
#include "d/d_lib.h"
#include "d/d_material.h"
#include "d/d_snap.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_item.h"
#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_gnd.h"
#include "d/d_demo.h"
#include "d/d_cc_uty.h"
#include "d/d_item_data.h"
#include "d/d_kankyo.h"
#include "d/d_particle_name.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "m_Do/m_Do_audio.h"
#include "dolphin/types.h"

class daPz_HIO_c : public mDoHIO_entry_c {
public:
    daPz_HIO_c();
    virtual ~daPz_HIO_c() {}

public:
    /* 0x004 */ dNpc_HIO_c mNpc;
    /* 0x02C */ u8 m2C;
    /* 0x02D */ u8 m2D;
    /* 0x02E */ u8 m2E;
    /* 0x02F */ u8 m2F;
    /* 0x030 */ u8 m30;
    /* 0x031 */ u8 m31;
    /* 0x032 */ u8 m32;
    /* 0x033 */ u8 m33;
    /* 0x034 */ u8 m34[10];
    /* 0x03E */ u8 m3E[0x040 - 0x03E];
    /* 0x040 */ f32 m40;
    /* 0x044 */ f32 m44;
    /* 0x048 */ f32 m48;
    /* 0x04C */ f32 m4C;
    /* 0x050 */ f32 m50;
    /* 0x054 */ s16 m54;
    /* 0x056 */ u8 m56[0x058 - 0x056];
    /* 0x058 */ f32 m58;
    /* 0x05C */ f32 m5C[3];
    /* 0x068 */ f32 m68[3];
    /* 0x074 */ f32 m74[3];
    /* 0x080 */ s16 m80[3];
    /* 0x086 */ s16 m86[3];
    /* 0x08C */ s16 m8C[3];
    /* 0x092 */ s16 m92[3];
    /* 0x098 */ s16 m98[3];
    /* 0x09E */ u8 m9E[0x0A0 - 0x09E];
    /* 0x0A0 */ f32 mA0[3];
    /* 0x0AC */ f32 mAC;
    /* 0x0B0 */ f32 mB0;
    /* 0x0B4 */ f32 mB4;
    /* 0x0B8 */ f32 mB8;
    /* 0x0BC */ f32 mBC;
    /* 0x0C0 */ f32 mC0;
    /* 0x0C4 */ f32 mC4;
    /* 0x0C8 */ f32 mC8;
    /* 0x0CC */ s16 mCC;
    /* 0x0CE */ u8 mCE[0x0D0 - 0x0CE];
    /* 0x0D0 */ f32 mD0;
    /* 0x0D4 */ f32 mD4;
    /* 0x0D8 */ f32 mD8;
    /* 0x0DC */ s16 mDC;
    /* 0x0DE */ s16 mDE;
    /* 0x0E0 */ s16 mE0;
    /* 0x0E2 */ s16 mE2;
    /* 0x0E4 */ s16 mE4;
    /* 0x0E6 */ s16 mE6;
    /* 0x0E8 */ s16 mE8;
    /* 0x0EA */ u8 mEA[0x0EC - 0x0EA];
    /* 0x0EC */ f32 mEC;
    /* 0x0F0 */ f32 mF0;
    /* 0x0F4 */ f32 mF4;
#if VERSION > VERSION_DEMO
    /* 0x0F8 */ s16 mF8[3];
    /* 0x0FE */ u8 mFE[0x100 - 0x0FE];
    /* 0x100 */ f32 m100[3];
#endif
};

STATIC_ASSERT(sizeof(daPz_HIO_c) == DEMO_SELECT(0xFC, 0x10C));

static daPz_HIO_c l_HIO;

const u32 daPz_c::m_heapsize = 0xA740;
const char daPz_c::m_arc_name[] = "PZ";
const f32 daPz_c::m_smoke_ef = 1.25f;
const f32 daPz_c::m_grass_ef = 1.5f;
const f32 daPz_c::m_splash_ef = 1.0f;

const dCcD_SrcCyl daPz_c::m_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
#if VERSION == VERSION_DEMO
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IGrpAll_e | cCcD_CoSPrm_VsGrpAll_e,
#else
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_VsGrpAll_e,
#endif
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
    // cM3dGCylS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 0.0f,
        /* Height */ 0.0f,
    }},
};

/* 000000EC-00000310       .text __ct__10daPz_HIO_cFv */
daPz_HIO_c::daPz_HIO_c() {
    m2C = 0;
    m2D = 1;
    m32 = 0;
    m33 = 0;
    for (int i = 0; i < 10; i++) {
        m34[i] = 0;
    }
    mNpc.m04 = -20.0f;
    mNpc.mMaxHeadX = 0x1FFE;
    mNpc.mMaxHeadY = 0x2710;
    mNpc.mMaxBackboneX = 0x1B58;
    mNpc.mMaxBackboneY = 0x32C8;
    mNpc.mMinHeadX = -0x9C4;
    mNpc.mMinHeadY = -0x2710;
    mNpc.mMinBackboneX = -0x1B58;
    mNpc.mMinBackboneY = -0x32C8;
    mNpc.mMaxTurnStep = 0x1000;
    mNpc.mMaxHeadTurnVel = 0x800;
    mNpc.mAttnYOffset = 50.0f;
    mNpc.mMaxAttnAngleY = 0x32C8;
    mNpc.m22 = 0;
    mNpc.mMaxAttnDistXZ = 400.0f;
    m40 = 60.0f;
    m2F = 0;
    m54 = 0x2000;
    m58 = 200.0f;
    m44 = 1.2f;
    m4C = 0.9f;
    m48 = 2.0f;
    m50 = 10.0f;
    m31 = 0;
    m5C[0] = 100.0f;
    m68[0] = 0.0f;
    m74[0] = 100.0f;
    m80[0] = 0x1E;
    m86[0] = 0x12C;
    m8C[0] = 0;
    m92[0] = 0x1E;
    m98[0] = 0;
    mA0[0] = 1000.0f;
#if VERSION > VERSION_DEMO
    mF8[0] = 0x3C;
    m100[0] = 3000.0f;
#endif
    m5C[2] = 100.0f;
    m68[2] = 100.0f;
    m74[2] = 80.0f;
    m80[2] = 0x1E;
    m86[2] = 0x64;
    m8C[2] = 0x64;
    m92[2] = 0x1E;
    m98[2] = 0x3C;
    mA0[2] = 1000.0f;
#if VERSION > VERSION_DEMO
    mF8[2] = 0x258;
    m100[2] = 800.0f;
#endif
    mAC = 100.0f;
    mE2 = 4;
    mE4 = 6;
    mE6 = 4;
    mE8 = 6;
    mB0 = -2.5f;
    mB4 = 20.0f;
    mB8 = 20.0f;
    mBC = 20.0f;
    mC0 = 20.0f;
    mC4 = 4.0f;
    mC8 = 15.0f;
    mCC = 0xE;
    mD8 = 30.0f;
    mDC = 0x28;
    mDE = 0x28;
    mE0 = 5;
    mD0 = 600.0f;
    mEC = 800.0f;
    mD4 = 15.0f;
    mF0 = 2.0f;
    mF4 = 1.0f;
}

/* 00000310-00000340       .text stealItem_CB__FPv */
static BOOL stealItem_CB(void* actor) {
    if (actor) {
        daItem_c* item = (daItem_c*)actor;
        item->scale.setall(1.0f);
        item->setFlag(daItem_c::FLAG_HOOK);
    }
    return TRUE;
}

/* 00000340-0000038C       .text nodeHeadControl_CB__FP7J3DNodei */
static BOOL nodeHeadControl_CB(J3DNode* node, int calcTiming) {
    if (calcTiming == 0) {
        J3DModel* model = j3dSys.getModel();
        daPz_c* i_this = (daPz_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->_nodeHeadControl(node, model);
        }
    }
    return TRUE;
}

/* 0000038C-000004FC       .text _nodeHeadControl__6daPz_cFP7J3DNodeP8J3DModel */
void daPz_c::_nodeHeadControl(J3DNode* node, J3DModel* model) {
    int jntNo = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));

    static cXyz l_offsetAttPos(0.0f, 0.0f, 0.0f);
    static cXyz l_offsetEyePos(24.0f, -16.0f, 0.0f);
    mDoMtx_stack_c::multVec(&l_offsetAttPos, &mAttPos);
    mDoMtx_stack_c::YrotM(-m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    mDoMtx_stack_c::multVec(&l_offsetEyePos, &mEyePos);
    mDoMtx_stack_c::multVecZero(&mHeadPos);
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
}

/* 00000538-00000584       .text nodeWaistControl_CB__FP7J3DNodei */
static BOOL nodeWaistControl_CB(J3DNode* node, int calcTiming) {
    if (calcTiming == 0) {
        J3DModel* model = j3dSys.getModel();
        daPz_c* i_this = (daPz_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->_nodeWaistControl(node, model);
        }
    }
    return TRUE;
}

/* 00000584-00000710       .text _nodeWaistControl__6daPz_cFP7J3DNodeP8J3DModel */
void daPz_c::_nodeWaistControl(J3DNode* node, J3DModel* model) {
    J3DJoint* joint = (J3DJoint*)node;
    int jntNo = joint->getJntNo();

    static s16 tmp_angle;
    tmp_angle += 0x1000;

    mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));

    if (isAnm(4) || isAnm(5) || isAnm(6)) {
        mDoMtx_stack_c::ZrotM(0xdac);
        mDoMtx_stack_c::YrotM(0x5dc);
    }

    if (REG12_S(6) != 0) {
        mDoMtx_stack_c::XrotM(tmp_angle);
    } else {
        mDoMtx_XrotM(mDoMtx_stack_c::get(), m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    }

    if (isAnm(4) || isAnm(5) || isAnm(6)) {
        mDoMtx_stack_c::YrotM(-0x5dc);
        mDoMtx_stack_c::ZrotM(-0xdac);
    }

    cMtx_copy(mDoMtx_stack_c::get(), mWaistMtx);
    MtxP now = mDoMtx_stack_c::get();
    mWaistPos.x = now[0][3];
    mWaistPos.y = now[1][3];
    mWaistPos.z = now[2][3];

    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
}

/* 00000710-0000075C       .text nodeWaist2Control_CB__FP7J3DNodei */
static BOOL nodeWaist2Control_CB(J3DNode* node, int calcTiming) {
    if (calcTiming == 0) {
        J3DModel* model = j3dSys.getModel();
        daPz_c* i_this = (daPz_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->_nodeWaist2Control(node, model);
        }
    }
    return TRUE;
}

/* 0000075C-000007F4       .text _nodeWaist2Control__6daPz_cFP7J3DNodeP8J3DModel */
void daPz_c::_nodeWaist2Control(J3DNode* node, J3DModel* model) {
    int jntNo = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));
    mDoMtx_stack_c::YrotM(mWaist2AngleY);
    mDoMtx_stack_c::ZrotM(mWaist2AngleZ);
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
}

/* 000007F4-00000840       .text nodeSkirtControl_CB__FP7J3DNodei */
static BOOL nodeSkirtControl_CB(J3DNode* node, int calcTiming) {
    if (calcTiming == 0) {
        J3DModel* model = j3dSys.getModel();
        daPz_c* i_this = (daPz_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->_nodeSkirtControl(node, model);
        }
    }
    return TRUE;
}

/* 00000840-00000920       .text _nodeSkirtControl__6daPz_cFP7J3DNodeP8J3DModel */
void daPz_c::_nodeSkirtControl(J3DNode* node, J3DModel* model) {
    J3DJoint* joint = (J3DJoint*)node;
    int jntNo = joint->getJntNo();

    mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));

    s16 ang = mWaist2AngleZ;
    if (ang > mWaist2AngleY) {
        ang = mWaist2AngleY;
    }

    mDoMtx_stack_c::XrotM(REG12_S(2));
    mDoMtx_stack_c::YrotM(REG12_S(3));
    mDoMtx_stack_c::ZrotM(-(ang + REG12_S(4)));

    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
}

/* 00000920-00000940       .text createHeap_CB__FP10fopAc_ac_c */
static BOOL createHeap_CB(fopAc_ac_c* i_this) {
    return static_cast<daPz_c*>(i_this)->_createHeap();
}

/* 00000940-00000D54       .text bodyCreateHeap__6daPz_cFv */
BOOL daPz_c::bodyCreateHeap() {
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_PZ_BDL_PZ_e));
    JUT_ASSERT(DEMO_SELECT(0x227, 0x23E), modelData != NULL);

    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        J3DMaterial* material = modelData->getMaterialNodePointer(i);
        material->setMaterialAnm(new daPz_matAnm_c());
    }

    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL, NULL, J3DFrameCtrl::EMode_NULL, 1.0f, 0, -1,
                                TRUE, NULL, 0x00080000, 0x11020222);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
        return FALSE;
    }
    mpMorf->getModel()->setUserArea((u32)this);

    if (!mInvisibleModel.create(mpMorf->getModel())) {
        return FALSE;
    }

    m_jnt.setHeadJntNum(PZ_JNT_HEAD_e);
    modelData->getJointNodePointer(PZ_JNT_HEAD_e)->setCallBack(nodeHeadControl_CB);
    m_jnt.setBackboneJntNum(PZ_JNT_STOMACH_e);
    modelData->getJointNodePointer(PZ_JNT_STOMACH_e)->setCallBack(nodeWaistControl_CB);
    modelData->getJointNodePointer(PZ_JNT_WAIST_e)->setCallBack(nodeWaist2Control_CB);
    modelData->getJointNodePointer(PZ_JNT_SKIRT_2_e)->setCallBack(nodeSkirtControl_CB);

    J3DAnmTexPattern* btp = static_cast<J3DAnmTexPattern*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_PZ_BTP_MABA_A_e));
    JUT_ASSERT(DEMO_SELECT(0x260, 0x277), btp != NULL);
    if (!mBtpAnm.init(modelData, btp, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }

    J3DAnmTextureSRTKey* btk = static_cast<J3DAnmTextureSRTKey*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_PZ_BTK_MABA_A_e));
    JUT_ASSERT(DEMO_SELECT(0x266, 0x27D), btk != NULL);
    if (!mBtkAnm.init(modelData, btk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }

    u16 num = mBtkAnm.getBtkAnm()->getUpdateMaterialNum();
    for (u16 i = 0; i < num; i++) {
        u16 materialID = mBtkAnm.getBtkAnm()->getUpdateMaterialID(i);
        mMatAnm[i] = static_cast<daPz_matAnm_c*>(modelData->getMaterialNodePointer(materialID)->getMaterialAnm());
    }

    J3DAnmTevRegKey* brk = static_cast<J3DAnmTevRegKey*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_PZ_BRK_TRI_TEST_e));
    JUT_ASSERT(DEMO_SELECT(0x277, 0x28E), brk != NULL);
    if (!mBrkAnm.init(modelData, brk, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }

    return TRUE;
}

/* 00000D54-00000E74       .text bowCreateHeap__6daPz_cFv */
BOOL daPz_c::bowCreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(m_arc_name, dRes_INDEX_PZ_BDL_BOW_e);
    JUT_ASSERT(DEMO_SELECT(0x288, 0x29F), modelData != NULL);

    mpBowMcaMorf = new mDoExt_McaMorf(
        modelData, NULL, NULL, NULL,
        J3DFrameCtrl::EMode_NULL, 1.0f, 0, -1, 1,
        NULL, 0x00080000, 0x11000022
    );
    if (mpBowMcaMorf == NULL || mpBowMcaMorf->getModel() == NULL) {
        return FALSE;
    }
    mpBowMcaMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}

/* 00000E74-00000EC0       .text _createHeap__6daPz_cFv */
BOOL daPz_c::_createHeap() {
    if (bodyCreateHeap() == 0) {
        return 0;
    }
    return bowCreateHeap() ? TRUE : FALSE;
}

/* 00000EC0-00000F20       .text __ct__13daPz_matAnm_cFv */
daPz_matAnm_c::daPz_matAnm_c() {
    mMoveFlag = false;
    mOffset.x = 0.0f;
    mOffset.y = 0.0f;
}

/* 00000F20-00000FB4       .text calc__13daPz_matAnm_cCFP11J3DMaterial */
void daPz_matAnm_c::calc(J3DMaterial* mat) const {
    J3DMaterialAnm::calc(mat);

    for (u32 i = 0; i < 8; i++) {
        if (getTexMtxAnm(i)) {
            J3DTexMtx* texMtx = mat->getTexMtx(i);
            if (mMoveFlag) {
                texMtx->getTexMtxInfo().mSRT.mTranslationX = mOffset.x;
                texMtx->getTexMtxInfo().mSRT.mTranslationY = mOffset.y;
            }
        }
    }
}

/* 00000FB4-00001038       .text getGndPos__6daPz_cFv */
void daPz_c::getGndPos() {
    mbHasGanondorf = false;
    fopAc_ac_c* gnd;
    if (fopAcM_SearchByName(fpcNm_GND_e, &gnd)) {
        mGanondorfPosEye = gnd->eyePos;
        mGanondorfPosCurrent = gnd->current.pos;
        mbHasGanondorf = true;
    }
}

/* 00001038-0000114C       .text checkEyeArea__6daPz_cFR4cXyz */
bool daPz_c::checkEyeArea(cXyz& pos) {
    int ang = cLib_distanceAngleS(shape_angle.y, cLib_targetAngleY(&current.pos, &pos));
    f32 dist = (current.pos - pos).absXZ();
    if (ang < l_HIO.m54 && dist < l_HIO.m58) {
        return true;
    }
    return false;
}

/* 0000114C-00001194       .text getMsg__6daPz_cFv */
u32 daPz_c::getMsg() {
    u32 msgNo = 0x3562;
    if (mTalkState == 0) {
        msgNo = 0x3562;
    } else if (mTalkState == 2) {
        if (m0F65 == 0) {
            m0F65 = 1;
            msgNo = 0x3563;
        } else {
            msgNo = 0x3565;
        }
    }
    return msgNo;
}

/* 00001194-00001208       .text next_msgStatus__6daPz_cFPUl */
u16 daPz_c::next_msgStatus(u32* pMsgNo) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    switch (*pMsgNo) {
    case 0x3563:
        *pMsgNo = 0x3564;
        break;
    default:
        if (*pMsgNo == 0x3564) {
            fopAc_ac_c* gnd;
            if (fopAcM_SearchByName(fpcNm_GND_e, &gnd) && gnd->stealItemBitNo == 0) {
                gnd->stealItemBitNo = 0x23;
            }
        }
        status = fopMsgStts_MSG_ENDS_e;
        break;
    }
    return status;
}

/* 00001208-00001288       .text anmAtr__6daPz_cFUs */
void daPz_c::anmAtr(u16 i_msgStatus) {
    static const u8 anm_atr[2] = {12, 13};
    switch (i_msgStatus) {
    case fopMsgStts_MSG_TYPING_e:
        if (mAnmAtrFlag == 0) {
            u8 attr = dComIfGp_getMesgAnimeAttrInfo();
            mAnmAtrFlag = 1;
            setAnm(anm_atr[attr], false, 0xF);
        }
        break;
    case fopMsgStts_MSG_DISPLAYED_e:
        mAnmAtrFlag = 0;
        break;
    }
}

/* 00001288-000012D4       .text eventOrder__6daPz_cFv */
void daPz_c::eventOrder() {
    if (mEventOrder == 1 || mEventOrder == 2) {
        eventInfo.onCondition(1);
        if (mEventOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 000012D4-00001338       .text checkOrder__6daPz_cFv */
void daPz_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        mEventOrder = 0;
    } else if (eventInfo.checkCommandTalk()) {
        if (mEventOrder == 1 || mEventOrder == 2) {
            mEventOrder = 0;
            modeProc(PROC_INIT_e, MODE_TALK);
        }
    }
}

/* 00001338-0000151C       .text setFallSplash__6daPz_cFv */
void daPz_c::setFallSplash() {
    g_env_light.settingTevStruct(2, &current.pos, &mTevstr);
    mSplashPos = mEyePos;
    mSplashPos.y += l_HIO.mD4;
    if (mObjAcch.ChkGroundHit()) {
        if (dComIfG_Bgsp()->GetAttributeCode(mObjAcch.m_gnd) == dBgS_Attr_WATERFALL_e) {
            if (mFollowCb1.getEmitter() == NULL) {
                dComIfGp_particle_set(
                    dPa_name::ID_AK_JN_ELEMENTSHIBUKI00, &mSplashPos, &shape_angle, NULL, 0xFF, &mFollowCb1
                );
                if (mFollowCb1.getEmitter() != NULL) {
                    mFollowCb1.getEmitter()->setRate(4.0f);
                    mFollowCb1.getEmitter()->setSpread(1.0f);
                    mFollowCb1.getEmitter()->setGlobalPrmColor(mTevstr.mColorC0.r, mTevstr.mColorC0.g, mTevstr.mColorC0.b);
                }
            }
        } else {
            mFollowCb1.remove();
        }
        if (cLib_calcTimer(&mSplashTimer) == 0) {
            static Vec fall_ripple_scale = {0.75f, 0.75f, 0.75f};
            cXyz pos;
            MtxP mtx = mpMorf->getModel()->getAnmMtx(PZ_JNT_WORLD_ROOT_e);
            mDoMtx_multVecZero(mtx, &pos);
            dComIfGp_particle_setSingleRipple(dPa_name::ID_IT_JN_WP_HAMON03, &pos, NULL, (const cXyz*)&fall_ripple_scale);
            mSplashTimer = 0xF;
        }
    } else {
        mFollowCb1.remove();
    }
}

/* 0000151C-000015F4       .text setHeadSplash__6daPz_cFv */
void daPz_c::setHeadSplash() {
    g_env_light.settingTevStruct(2, &current.pos, &mTevstr);

    if (mFollowCb2.getEmitter() == NULL) {
        dComIfGp_particle_set(dPa_name::ID_AK_SN_PZSHAKEHEADSPLASH00, &mHeadPos, NULL, NULL, 0xFF, &mFollowCb2);
    }

    if (mFollowCb2.getEmitter() != NULL) {
        mFollowCb2.getEmitter()->setGlobalPrmColor(mTevstr.mColorC0.r, mTevstr.mColorC0.g,
                                                   mTevstr.mColorC0.b);
        mFollowCb2.getEmitter()->setGlobalRTMatrix(mpMorf->getModel()->getAnmMtx(PZ_JNT_HEAD_e));
    }
}

/* 000015F4-00001704       .text setRipple__6daPz_cFv */
void daPz_c::setRipple() {
    if (mObjAcch.ChkGroundHit()) {
        if (dComIfG_Bgsp()->GetAttributeCode(mObjAcch.m_gnd) == dBgS_Attr_WATER_e) {
            if (mRippleCb.getEmitter() == NULL) {
                static cXyz scale(0.8f, 0.8f, 0.8f);
                dComIfGp_particle_setShipTail(dPa_name::ID_AK_JN_HAMON00, &current.pos, NULL, &scale, 0xFF, &mRippleCb);
                if (mRippleCb.getEmitter() != NULL) {
                    mRippleCb.setRate(0.0f);
                }
            }
        } else {
            mRippleCb.remove();
        }
    }
}

/* 00001704-0000175C       .text setJntStatus__6daPz_cFv */
void daPz_c::setJntStatus() {
    m_jnt.setParam(
        l_HIO.mNpc.mMaxBackboneX,
        l_HIO.mNpc.mMaxBackboneY,
        l_HIO.mNpc.mMinBackboneX,
        l_HIO.mNpc.mMinBackboneY,
        l_HIO.mNpc.mMaxHeadX,
        l_HIO.mNpc.mMaxHeadY,
        l_HIO.mNpc.mMinHeadX,
        l_HIO.mNpc.mMinHeadY,
        l_HIO.mNpc.mMaxTurnStep
    );
}

/* 0000175C-00001954       .text demo__6daPz_cFv */
bool daPz_c::demo() {
    if (demoActorID == 0) {
        if (m0F66 != 0) {
            m0F66 = 0;
        }
    } else {
        m0F66 = 1;

        dDemo_actor_c* demoActor = dComIfGp_demo_getActor(demoActorID);

        for (int i = 0; i < 2; i++) {
            if (mMatAnm[i] != NULL) {
                mMatAnm[i]->clrMoveFlag();
            }
        }

        if (mBtpAnm.getBtpAnm() != NULL) {
            u8 maxFrame = (u8)mBtpAnm.getBtpAnm()->getFrameMax();
            m0F67++;
            if (m0F67 >= maxFrame) {
                m0F67 = maxFrame;
            }
        }

        J3DAnmTexPattern* btp = demoActor->getP_BtpData(m_arc_name);
        if (btp != NULL) {
            mBtpAnm.init(mpMorf->getModel()->getModelData(), btp, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0);
            m0F67 = 0;
        }

        J3DAnmTextureSRTKey* btk = demoActor->getP_BtkData(m_arc_name);
        if (btk != NULL) {
            mBtkAnm.init(mpMorf->getModel()->getModelData(), btk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0);
        }

        J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)demoActor->getP_BrkData(m_arc_name);
        if (brk != NULL) {
            mBrkAnm.init(mpMorf->getModel()->getModelData(), brk, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, true, 0);
        }

        dDemo_setDemoData(this, 0x6A, mpMorf, m_arc_name);
    }

    return m0F66;
}

/* 00001954-00001EEC       .text checkTgHit__6daPz_cFv */
bool daPz_c::checkTgHit() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    mStts.Move();
    if (cLib_calcTimer(&mInvulnTimer) == 0) {
        if (mCyl.ChkTgHit()) {
            bool doKnockback = true;
            CcAtInfo at_info;
            cCcD_Obj* hitObj = mCyl.GetTgHitObj();
            mInvulnTimer = l_HIO.mE0;

            if (hitObj == NULL) {
                return false;
            }

            switch (hitObj->GetAtType()) {
            case AT_TYPE_SWORD:
            case AT_TYPE_MACHETE:
            case AT_TYPE_UNK800:
            case AT_TYPE_DARKNUT_SWORD:
            case AT_TYPE_MOBLIN_SPEAR:
                switch (player->getCutType()) {
                case daPy_py_c::CUT_TYPE_BT_JUMPCUT:
                case daPy_py_c::CUT_TYPE_CUT_EA:
                case daPy_py_c::CUT_TYPE_CUT_EB:
                case daPy_py_c::CUT_TYPE_CUT_TURN:
                case daPy_py_c::CUT_TYPE_CUT_ROLL:
                case daPy_py_c::CUT_TYPE_JUMPCUT_SWORD:
                case daPy_py_c::CUT_TYPE_JUMPCUT_STICK:
                case daPy_py_c::CUT_TYPE_JUMPCUT_MACHETE:
                case daPy_py_c::CUT_TYPE_BT_ROLLCUT:
                case daPy_py_c::CUT_TYPE_BT_VERTICALJUMPCUT:
                case daPy_py_c::CUT_TYPE_JUMPCUT_CLUB:
                case daPy_py_c::CUT_TYPE_JUMPCUT_DN_SWORD:
                case daPy_py_c::CUT_TYPE_JUMPCUT_SPEAR:
                case daPy_py_c::CUT_TYPE_CUT_EXA:
                case daPy_py_c::CUT_TYPE_CUT_EXB:
                    mHitType = 1;
                    break;
                default:
                    mHitType = 0;
                    break;
                }
                break;
            case AT_TYPE_WIND:
                doKnockback = false;
                mHitType = 3;
                break;
            case AT_TYPE_BOOMERANG:
            case AT_TYPE_BOKO_STICK:
                mHitType = 4;
                break;
            case AT_TYPE_HOOKSHOT:
                mHitType = 0xC;
                break;
            case AT_TYPE_SKULL_HAMMER:
            case AT_TYPE_STALFOS_MACE:
                mHitType = 7;
                if (player->getCutType() == daPy_py_c::CUT_TYPE_HAMMER_SIDESWING) {
                    mHitType = 8;
                }
                break;
            case AT_TYPE_BOMB:
                mHitType = 6;
                break;
            case AT_TYPE_NORMAL_ARROW:
            case AT_TYPE_FIRE_ARROW:
            case AT_TYPE_ICE_ARROW:
            case AT_TYPE_LIGHT_ARROW:
                mHitType = 5;
                break;
#if VERSION > VERSION_DEMO
            case AT_TYPE_GRAPPLING_HOOK:
                mHitType = 0xE;
                doKnockback = false;
                break;
#endif
            }

            fopAc_ac_c* hitAc = mCyl.GetTgHitAc();
            if (fopAcM_GetName(hitAc) == fpcNm_ARROW_e) {
                daArrow_c* arrow = (daArrow_c*)hitAc;
                if (arrow->isLinkReflect()) {
                    mHitType = 5;
                } else if (arrow->isSetByZelda()) {
                    return false;
                }
            }

            if (doKnockback) {
                cXyz* hitPosP = mCyl.GetTgHitPosP();
                mHitAngleY = fopAcM_searchActorAngleY(this, hitAc);
                at_info.mpObj = mCyl.GetTgHitObj();
                def_se_set(this, mCyl.GetTgHitObj(), 0x41);
                dKy_SordFlush_set(*hitPosP, 0);
                if (mHitType == 1 || mHitType == 7 || mHitType == 8) {
                    mKnockback = 60.0f;
                } else {
                    mKnockback = 40.0f;
                }
                dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, hitPosP, &player->shape_angle, NULL, 0xff, NULL);
                modeProc(PROC_INIT_e, MODE_DEFEND);
                if (fopAcM_GetName(hitAc) == fpcNm_GND_e) {
                    m0744 = 0;
                    mKnockback *= 1.5;
                    m0754 = 0;
                    m075C = 0;
                    m0760++;
                    if (m0760 > l_HIO.mE2) {
                        m0760 = 0;
                        m0764 = l_HIO.mE4;
                        m0768 = 1;
                    }
#if VERSION > VERSION_DEMO
                    if (cM_rndF(100.0f) < 60.0f) {
                        fopAcM_monsSeStart(this, JA_SE_CV_ZL_GN_DAMAGE, 0);
                    }
#endif
                } else {
                    m0744 = 1;
                    m0760 = 0;
                    m0768 = 0;
                    m0754++;
                    if (m0754 > l_HIO.mE6) {
                        m0754 = 0;
                        m0758 = l_HIO.mE8;
                        m075C = 1;
                    }
#if VERSION > VERSION_DEMO
                    if (cM_rndF(100.0f) < 30.0f) {
                        fopAcM_monsSeStart(this, JA_SE_CV_ZL_GN_DAMAGE, 0);
                    }
#endif
                }
                mHitPos = *hitPosP;
            }
#if VERSION > VERSION_DEMO
            else if (mHitType == 0xE) {
                fopAcM_monsSeStart(this, JA_SE_CV_ZL_GN_DAMAGE, 0);
                int itemNo = dItemNo_HEART_e;
                if (dComIfGs_getLife() <= 12) {
                    itemNo = dItemNo_TRIPLE_HEART_e;
                }
                fopAcM_fastCreateItem(&current.pos, itemNo, fopAcM_GetRoomNo(this), &shape_angle, NULL, 0.0f, 0.0f, -6.0f, -1, stealItem_CB);
                modeProc(PROC_INIT_e, MODE_DEFEND);
            }
#endif
            return true;
        }
    }
    return false;
}

/* 00001EEC-00001F10       .text getArg__6daPz_cFv */
void daPz_c::getArg() {
    mArg = fopAcM_GetParamBit(fopAcM_GetParam(this), 8, 8);
    if (mArg == 0xFF) {
        mArg = 0;
    }
}

/* 00001F10-000020B8       .text setAttention__6daPz_cFv */
void daPz_c::setAttention() {
    attention_info.position = mAttPos;
    attention_info.position.y += l_HIO.m40;
    eyePos = mEyePos;

    f32 dist = (mEyePos - mLookPos).absXZ();
    cLib_distanceAngleS(shape_angle.y, cLib_targetAngleY(&mEyePos, &mLookPos));

    s16 target;
    if (m_jnt.trnChk()) {
        if (mEventCut.getTurnSpeed() != 0) {
            target = mEventCut.getTurnSpeed();
        } else {
            target = l_HIO.mNpc.mMaxHeadTurnVel;
        }
    } else {
        target = 0;
    }
    cLib_addCalcAngleS2(&mLookAngleY, target, 4, 0x800);

    m_jnt.lookAtTarget(&shape_angle.y, &mLookPos, mEyePos, shape_angle.y, mLookAngleY, m08EA);
}

/* 000020B8-00002114       .text setBowAnm__6daPz_cFScb */
void daPz_c::setBowAnm(s8 idx, bool param_2) {
    static const int a_anm_bcks_tbl[] = {
        dRes_INDEX_PZ_BCK_ARROW_RELORD_e,
        dRes_INDEX_PZ_BCK_ARROW_WAIT03_e,
        dRes_INDEX_PZ_BCK_ARROW_SHOOT_e,
    };
    static const dLib_anm_prm_c a_anm_prm_tbl[] = {
        {2, -1, 0, 0.0f, 0.0f, 0},
        {2, -1, 0, 0.0f, 1.0f, 0},
        {0, -1, 0, 0.0f, 1.0f, 0},
        {1, -1, 0, 0.0f, 1.0f, 0},
        {2, -1, 0, 0.0f, 1.0f, 0},
    };
    if (idx != 5) {
        mBowAnmPrmIdx = idx;
    }
    dLib_bcks_setAnm(m_arc_name, mpBowMcaMorf, &mBowBckIdx, &mBowAnmPrmIdx, &mBowOldAnmPrmIdx, a_anm_bcks_tbl, a_anm_prm_tbl, param_2);
}

/* 00002114-00002184       .text setBowString__6daPz_cFb */
void daPz_c::setBowString(bool param_1) {
    J3DMaterial* mat = mpBowMcaMorf->getModel()->getModelData()->getJointNodePointer(PZ_BOW_JNT_CL_BOW_e)->getMesh()->getNext();
    J3DMaterial* mat2 = mat->getNext();
    if (param_1) {
        mat->getShape()->show();
        mat2->getShape()->hide();
    } else {
        mat->getShape()->hide();
        mat2->getShape()->show();
    }
}

/* 00002184-0000246C       .text setAnm__6daPz_cFScbi */
void daPz_c::setAnm(s8 idx, bool param_2, int eyeIdx) {
    static const int a_anm_bcks_tbl[] = {
        dRes_INDEX_PZ_BCK_WAIT01_e,
        dRes_INDEX_PZ_BCK_WAIT02_e,
        dRes_INDEX_PZ_BCK_RUN01_e,
        dRes_INDEX_PZ_BCK_RELORD_e,
        dRes_INDEX_PZ_BCK_WAIT03_e,
        dRes_INDEX_PZ_BCK_SHOOT_e,
        dRes_INDEX_PZ_BCK_DAM01_e,
        dRes_INDEX_PZ_BCK_DAM02_e,
        dRes_INDEX_PZ_BCK_STAND_e,
        dRes_INDEX_PZ_BCK_DEFEND_e,
        dRes_INDEX_PZ_BCK_WAIT04_e,
    };
    if (idx != 0xF) {
        mAnmPrmIdx = idx;
    }

    dLib_anm_prm_c a_anm_prm_tbl[] = {
        {0, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {0, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {3, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {4, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {6, -1, 0, 4.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {7, -1, 0, 4.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {8, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {9, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {10, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {4, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
    };
    static const u8 eye_anm[] = {0, 0, 7, 7, 1, 1, 1, 2, 3, 4, 5, 6, 8, 9, 8};

    a_anm_prm_tbl[7].mMorf = l_HIO.mF0;
    a_anm_prm_tbl[8].mMorf = l_HIO.mF4;

    if (mOldAnmPrmIdx != mAnmPrmIdx) {
        if (mAnmPrmIdx == 4) {
            fopAcM_monsSeStart(this, JA_SE_CV_ZL_DRAW_BOW, 0);
            setBowAnm(2, false);
            mArrowId = fopAcM_createChild(
                fpcNm_ARROW_e, fopAcM_GetID(this), 0, &current.pos,
                fopAcM_GetRoomNo(this), NULL, NULL, -1, NULL
            );
        }
        if (mAnmPrmIdx == 5) {
            setBowAnm(3, true);
        }
        if (mAnmPrmIdx == 6) {
            fopAcM_monsSeStart(this, JA_SE_CV_ZL_SHOOT_BOW, 0);
            setBowAnm(4, true);
            fopAc_ac_c* arrow;
            if (fopAcM_SearchByID(mArrowId, &arrow)) {
                fopAcM_SetParam(arrow, 1);
            }
        }
        if (mAnmPrmIdx != 4 && mAnmPrmIdx != 5) {
            fopAc_ac_c* arrow;
            if (fopAcM_SearchByID(mArrowId, &arrow)) {
                fopAcM_SetParam(arrow, 1);
            }
        }
        if (mAnmPrmIdx == 9) {
            setHeadSplash();
        } else {
            mFollowCb2.remove();
        }
        if (mAnmPrmIdx == 4 || mAnmPrmIdx == 5 || mAnmPrmIdx == 6) {
            setBowString(true);
        } else {
            setBowAnm(1, true);
            setBowString(false);
        }
        if (eyeIdx == 0xF) {
            setEyeAnm(eye_anm[mAnmPrmIdx]);
        } else {
            setEyeAnm(eye_anm[eyeIdx]);
        }
    }

    dLib_bcks_setAnm(m_arc_name, mpMorf, &mBckIdx, &mAnmPrmIdx, &mOldAnmPrmIdx, a_anm_bcks_tbl, a_anm_prm_tbl, param_2);
}

/* 0000246C-00002684       .text setAnmRunSpeed__6daPz_cFv */
void daPz_c::setAnmRunSpeed() {
    if (mAnmPrmIdx == 3) {
        f32 dist = (current.pos - old.pos).abs();

        f32 speed = dist / (10.0f + REG12_F(6));
        if (speed <= 0.0f) {
            speed = 0.0f;
        } else if (speed >= 1.0f) {
            speed = 1.0f;
        }

        speed *= l_HIO.m44;
        speed = cLib_minLimit(speed, l_HIO.m4C);
        speed = cLib_maxLimit(speed, l_HIO.m48);

        mpMorf->setPlaySpeed(speed);

        int frame = (int)mpMorf->getFrame();
        if (frame == 7 || frame == 0xE) {
            static JGeometry::TVec3<f32> run_splash_scale(0.6f, 0.6f, 0.6f);

            int unused;
            JPABaseEmitter* emitter = dComIfGp_particle_setSimpleLand(
                mObjAcch.m_gnd, &current.pos, &shape_angle, 1.25f, 1.5f, 1.0f, &tevStr, &unused, 7);
            if (emitter != NULL) {
                emitter->setRate(18.0f);
                emitter->setSpread(1.0f);
                emitter->setGlobalScale(run_splash_scale);
            }
        }
    }
}

/* 00002684-0000274C       .text setEyeBtp__6daPz_cFi */
void daPz_c::setEyeBtp(int idx) {
    J3DModel* model = mpMorf->getModel();
    J3DAnmTexPattern* btp = (J3DAnmTexPattern*)dComIfG_getObjectRes(m_arc_name, idx);
    JUT_ASSERT(DEMO_SELECT(0x636, 0x683), btp != NULL);
    m0F67 = 0;
    mBtpAnm.init(model->getModelData(), btp, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0);
}

/* 0000274C-00002810       .text setEyeBtk__6daPz_cFi */
void daPz_c::setEyeBtk(int idx) {
    J3DModel* model = mpMorf->getModel();
    J3DAnmTextureSRTKey* btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectRes(m_arc_name, idx);
    JUT_ASSERT(DEMO_SELECT(0x644, 0x691), btk != NULL);
    mBtkAnm.init(model->getModelData(), btk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0);
}

/* 00002810-00002888       .text setEyeAnm__6daPz_cFSc */
void daPz_c::setEyeAnm(s8 idx) {
    static const int a_eye_tbl[][2] = {
        {dRes_INDEX_PZ_BTP_MABA_A_e, dRes_INDEX_PZ_BTK_MABA_A_e},
        {dRes_INDEX_PZ_BTP_BOW_WAIT_e, dRes_INDEX_PZ_BTK_BOW_WAIT_e},
        {dRes_INDEX_PZ_BTP_DAM01_e, -1},
        {dRes_INDEX_PZ_BTP_DAM02_e, -1},
        /* BTP_TALK01=0x29, BTP_TALK02=0x2A, BTP_STAND=0x2B in the original binary
         * (the regenerated Pz.h assigns these three indices differently) */
        {0x29, dRes_INDEX_PZ_BTK_STAND_e},
        {dRes_INDEX_PZ_BTP_DEFEND_e, -1},
        {dRes_INDEX_PZ_BTP_WAIT04_e, -1},
        {dRes_INDEX_PZ_BTP_MABA_B_e, dRes_INDEX_PZ_BTK_MABA_A_e},
        {0x2A, -1},
        {0x2B, -1},
    };
    if (a_eye_tbl[idx][0] != -1) {
        setEyeBtp(a_eye_tbl[idx][0]);
    }
    if (a_eye_tbl[idx][1] != -1) {
        setEyeBtk(a_eye_tbl[idx][1]);
    }
    mCurEye = idx;
}

/* 00002888-00002AE0       .text ctrlEye__6daPz_cFv */
void daPz_c::ctrlEye() {
    mEyeLookPos = mLookPos;

    s16 pitchAngle = cLib_targetAngleX(&mEyePos, &mEyeLookPos);

    s16 diffYaw = cLib_targetAngleY(&mEyePos, &mEyeLookPos);
    diffYaw -= (s16)(shape_angle.y + m_jnt.getHead_y() + m_jnt.getBackbone_y());

    s16 diffPitch = pitchAngle - (s16)(m_jnt.getHead_x() + m_jnt.getBackbone_x());

    f32 pitchDeg = diffPitch / 8192.0f;
    f32 yawDeg = diffYaw / 8192.0f;

    f32 min = -0.1f;
    f32 max = 0.1f;

    pitchDeg = max * pitchDeg;
    yawDeg = max * yawDeg;

    u8 in_range = 0;
    if (pitchDeg >= min && pitchDeg <= max) {
        in_range = 1;
    }
    m06DC = in_range != 0;

    in_range = 0;
    if (yawDeg >= min && yawDeg <= max) {
        in_range = 1;
    }
    m06DC = in_range != 0;

    pitchDeg = cLib_minMaxLimit(pitchDeg, min, max);
    yawDeg = cLib_minMaxLimit(yawDeg, min, max);

    if (mMode == MODE_ATTACK) {
        yawDeg = max;
    }

    if (mMatAnm[0] != NULL) {
        cLib_addCalc(mMatAnm[0]->getNowOffsetX(), yawDeg, 0.5f, 0.1f, 0.03f);
        cLib_addCalc(mMatAnm[0]->getNowOffsetY(), pitchDeg, 0.5f, 0.1f, 0.03f);
    }

    yawDeg *= -1.0f;

    if (mMatAnm[1] != NULL) {
        cLib_addCalc(mMatAnm[1]->getNowOffsetX(), yawDeg, 0.5f, 0.1f, 0.03f);
        cLib_addCalc(mMatAnm[1]->getNowOffsetY(), pitchDeg, 0.5f, 0.1f, 0.03f);
    }
}

/* 00002AE0-00002D38       .text playEyeAnm__6daPz_cFv */
void daPz_c::playEyeAnm() {
    bool moveEye = true;
    if (isEyeAnm(0) || isEyeAnm(7)) {
        if (cLib_calcTimer(&mEyeTimer) == 0) {
            m0F67++;
            if (m0F67 > mBtpAnm.getEndFrame()) {
                mEyeTimer = (s16)(100.0f + cM_rndF(100.0f));
                m0F67 = 0;
            }
        }
    } else if (isEyeAnm(1) || isEyeAnm(2) || isEyeAnm(5) || isEyeAnm(6)) {
        m0F67 = 1;
    } else {
        m0F67++;
        if (m0F67 > mBtpAnm.getEndFrame()) {
            mEyeTimer = (s16)(100.0f + cM_rndF(100.0f));
            m0F67 = mBtpAnm.getEndFrame();
        }
    }

    if (isEyeAnm(4)) {
        moveEye = false;
    }
    if (moveEye) {
        for (int i = 0; i < 2; i++) {
            if (mMatAnm[i] != NULL) {
                mMatAnm[i]->setMoveFlag();
            }
        }
        ctrlEye();
    } else {
        for (int i = 0; i < 2; i++) {
            if (mMatAnm[i] != NULL) {
                mMatAnm[i]->clrMoveFlag();
            }
        }
        mBtkAnm.play();
    }
}

/* 00002D38-00002DC8       .text setMtx__6daPz_cFv */
void daPz_c::setMtx() {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle.x, shape_angle.y, shape_angle.z);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00002DC8-00002E2C       .text modeWaitInit__6daPz_cFv */
void daPz_c::modeWaitInit() {
    m0924 = 0.0f;
    setAnm(1, false, 0xF);
    m_jnt.clrTrn();
    m08EA = 1;
    m_jnt.onHeadLock();
    m_jnt.onBackBoneLock();
    m08EC = 0x1E;
}

/* 00002E2C-00002FE8       .text modeWait__6daPz_cFv */
void daPz_c::modeWait() {
    mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);

    dEvent_manager_c& evtMng = g_dComIfG_gameInfo.play.getEvtManager();
    s16 eventIdx = evtMng.getEventIdx("btl_of_swroom", 0xff);
    if (evtMng.getEventData(eventIdx) != NULL) {
        int staffId = evtMng.getMyStaffId("p_zelda", NULL, 0);
        if (staffId != -1) {
            if (strcmp(evtMng.getMyNowCutName(staffId), "Turn") == 0) {
                m08EA = 0;
                m_jnt.setTrn();
                m_jnt.offHeadLock();
                m_jnt.offBackBoneLock();
                if (cLib_calcTimer(&m08EC) == 0) {
                    evtMng.cutEnd(staffId);
                }
                return;
            } else if (strcmp(evtMng.getMyNowCutName(staffId), "Retire") == 0) {
                mEyeTimer = 0;
                if (m0F67 >= REG8_S(0) + 4) {
                    m0F67 = REG8_S(0) + 4;
                    mEventIce.mLightShrinkTimer = 1;
                    mEyeTimer = 1000;
                    evtMng.cutEnd(staffId);
                }
                return;
            } else {
                if (mEventIce.mLightShrinkTimer == 0) {
                    mEyeTimer = 1000;
                }
                m_jnt.clrTrn();
            }
        }
    } else {
        m_jnt.clrTrn();
        m08EA = 1;
        m_jnt.onHeadLock();
        m_jnt.onBackBoneLock();
    }
}

/* 00002FE8-000031E8       .text modeMoveInit__6daPz_cFv */
void daPz_c::modeMoveInit() {
    cLib_onBit<u32>(attention_info.flags, fopAc_Attn_ACTION_SPEAK_e);
    cLib_onBit<u32>(attention_info.flags, fopAc_Attn_LOCKON_TALK_e);
    m08EC = l_HIO.m86[mTalkState] + cM_rndF(l_HIO.m8C[mTalkState]);
    m08F0 = 0xA;
    m08F4 = 0x78;
    setAnm(3, false, 0xF);
    if (cM_rndF(100.0f) < l_HIO.mD8) {
        if (m0920 == 1) {
            m0920 = -1;
        } else {
            m0920 = 1;
        }
    }
    if (l_HIO.m30 == 0) {
        if (mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }
    mCirclePath.mRadius = l_HIO.mA0[mTalkState];
    mCirclePath.mWobbleAmplitude = l_HIO.mAC;
    mCirclePath.mAngleSpeed = (REG12_S(0) + 0x150) * m0920;
    mCirclePath.mTranslation = mLookPos;
    dLib_setCirclePath(&mCirclePath);
    m08EA = 0;
    m_jnt.clrTrn();
    m_jnt.offHeadLock();
    m_jnt.offBackBoneLock();
}

/* 000031E8-0000395C       .text modeMove__6daPz_cFv */
void daPz_c::modeMove() {
    if (l_HIO.m32 != 0) {
        l_HIO.m32 = 0;
        modeProc(PROC_INIT_e, MODE_SIDE_STEP);
        return;
    }

    if (l_HIO.m34[0] != 0) {
        l_HIO.m34[0] = 0;
        modeProc(PROC_INIT_e, MODE_BACK_STEP);
        return;
    }

    bool blocked = false;

    mLinChk.Set(&current.pos, &mCirclePath.mPos, this);
    if (dComIfG_Bgsp()->LineCross(&mLinChk)) {
        blocked = true;
    }
    if (mObjAcch.ChkWallHit()) {
        blocked = true;
    }

    if (cLib_calcTimer(&m08F8) == 0 && blocked) {
        m08F8 = 60;
        if (m0920 == 1) {
            m0920 = -1;
        } else {
            m0920 = 1;
        }
        mCirclePath.mAngleSpeed = (s16)((REG12_S(0) + 0x4000) * m0920);

        if (cLib_calcTimer(&m08F4) == 0) {
            dLib_setCirclePath(&mCirclePath);
            modeProc(PROC_INIT_e, MODE_SIDE_STEP);
        }
    }

    if (checkTgHit()) {
        return;
    }

    if (l_HIO.m30 == 0) {
        if (mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }

    m_jnt.clrTrn();

    cLib_addCalc2(&mCirclePath.mRadius, l_HIO.mA0[mTalkState], 0.1f, 10.0f);

    f32 distToPath = (current.pos - mCirclePath.mPos).absXZ();
    f32 unusedLookDist = (current.pos - mLookPos).absXZ();
    f32 distLookToPath = (mLookPos - mCirclePath.mPos).absXZ();

    if (distToPath > 200.0f + REG12_F(7) || mbEyesFollowGanondorf == 0) {
        m0924 = l_HIO.m50;
    } else {
        m0924 = 0.0f;
    }

    if (blocked || distToPath <= 200.0f ||
        distLookToPath >= 2.0f * l_HIO.mA0[mTalkState] || mbEyesFollowGanondorf) {
        mCirclePath.mWobbleAmplitude = l_HIO.mAC;
        mCirclePath.mAngleSpeed = (s16)((REG12_S(0) + 0x150) * m0920);
        mCirclePath.mTranslation = mLookPos;
        dLib_setCirclePath(&mCirclePath);
    }

    if (cLib_calcTimer(&m08F0) == 0) {
        f32 distGnd = 100000.0f;
        f32 distLook = (current.pos - mLookPos).absXZ();
        if (mbHasGanondorf) {
            distGnd = (current.pos - mGanondorfPosCurrent).absXZ();
        }
        if (mbEyesFollowGanondorf) {
            if (distLook < l_HIO.mD0 || distGnd < l_HIO.mD0) {
                mPrevMode = mMode;
                modeProc(PROC_INIT_e, MODE_BACK_STEP);
                return;
            }
        }
    }

    s16 target = cLib_targetAngleY(&current.pos, &mCirclePath.mPos);
    cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);

    if (speedF < 0.5f) {
        setAnm(2, false, 0xF);
    } else {
        setAnm(3, false, 0xF);
    }

#if VERSION == VERSION_DEMO
    if (l_HIO.m34[1] == 0 && cLib_calcTimer(&m08EC) == 0) {
        modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
    }
#else
    if (mTalkState == 2) {
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        m0F81 = dLib_checkActorInFan(player->current.pos, this, player->shape_angle.y,
                                     0x2500, 30000.0f, 1000.0f);
    }

    if (l_HIO.m34[1] == 0 && cLib_calcTimer(&m08EC) == 0) {
        if (mTalkState == 0) {
            modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
        } else if (mTalkState == 2) {
            if (m0F65 == 0) {
                m0F81 = 1;
            }
            if (m0F81 != 0) {
                modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
            }
        }
    }
#endif

    mEventOrder = 2;
}

/* 0000395C-000039C0       .text modeAttackWaitInit__6daPz_cFv */
void daPz_c::modeAttackWaitInit() {
    m0924 = 0.0f;
    m08EC = 0x14;
    m08F0 = 0xA;
    setAnm(2, false, 0xF);
    m08EA = 0;
    m_jnt.offHeadLock();
    m_jnt.offBackBoneLock();
}

/* 000039C0-00003D88       .text modeAttackWait__6daPz_cFv */
void daPz_c::modeAttackWait() {
    if (checkTgHit()) {
        return;
    }

    if (l_HIO.m30 == 0) {
        if (mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }

    if (cLib_calcTimer(&m08F0) == 0) {
        f32 distLook = (current.pos - mLookPos).absXZ();
        f32 distGnd = 100000.0f;
        if (mbHasGanondorf) {
            distGnd = (current.pos - mGanondorfPosCurrent).absXZ();
        }
        if (mbEyesFollowGanondorf && (distLook < l_HIO.mD0 || distGnd < l_HIO.mD0)) {
            mPrevMode = mMode;
            if (cM_rndF(100.0f) < 50.0f) {
                modeProc(PROC_INIT_e, MODE_BACK_STEP);
            } else {
                modeProc(PROC_INIT_e, MODE_SIDE_STEP);
            }
            return;
        }
    }

#if VERSION == VERSION_DEMO
    if (m075C != 0) {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }
    if (m0768 != 0 && mbHasGanondorf) {
        mLookPos = mGanondorfPosEye;
    }
#else
    if (mTalkState == 0) {
        fopAc_ac_c* gnd;
        if (fopAcM_SearchByName(fpcNm_GND_e, &gnd) && gnd->health > 0x3C && dComIfGs_getLife() >= 12 && m075C != 0) {
            mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
        }
        if (m0768 != 0 && mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        if (m075C != 0) {
            mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
        }
        if (m0768 != 0 && mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    }
#endif

    if (l_HIO.m2D != 0) {
        if (cLib_calcTimer(&m08EC) == 0) {
            modeProc(PROC_INIT_e, MODE_ATTACK);
        }
    }
    m_jnt.setTrn();
}

/* 00003D88-00003F14       .text modeAttackInit__6daPz_cFv */
void daPz_c::modeAttackInit() {
    m08F4 = 0x1E;
    int talkState = mTalkState;
    if (m0F65 == 0) {
        talkState = 0;
    }
    if (!isAnm(4) && !isAnm(5)) {
        m08EC = l_HIO.m92[talkState] + cM_rndF(l_HIO.m98[talkState]);
        m08F0 = l_HIO.m80[talkState];
        setAnm(4, false, 0xF);
    }
    if (cM_rndF(100.0f) < l_HIO.m68[talkState]) {
        m0F48 = 1;
    } else {
        m0F48 = 0;
        if (cM_rndF(100.0f) < l_HIO.m74[talkState]) {
            m0F44 = 0.0f;
        } else {
            m0F44 = cM_rndF(l_HIO.m5C[talkState]);
        }
    }
    m08EA = 0;
    m_jnt.onHeadLock();
    m_jnt.offBackBoneLock();
}

/* 00003F14-0000445C       .text modeAttack__6daPz_cFv */
void daPz_c::modeAttack() {
    if (checkTgHit()) {
        return;
    }

    cXyz aimOff;
    aimOff.setall(m0F44);

    if (m0F48 == 0) {
        if (mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }

#if VERSION == VERSION_DEMO
    if (m075C != 0) {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }
    if (m0768 != 0 && mbHasGanondorf) {
        mLookPos = mGanondorfPosEye;
    }
#else
    if (mTalkState == 0) {
        fopAc_ac_c* gnd;
        if (fopAcM_SearchByName(fpcNm_GND_e, &gnd) && gnd->health > 0x3C && dComIfGs_getLife() >= 12 && m075C != 0) {
            mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
        }
        if (m0768 != 0 && mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        if (m075C != 0) {
            mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
        }
        if (m0768 != 0 && mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    }
#endif

    if (isAnm(4) || isAnm(5)) {
        if (cLib_calcTimer(&m08F4) == 0) {
            f32 distLook = (current.pos - mLookPos).absXZ();
            f32 distGnd = 100000.0f;
            if (mbHasGanondorf) {
                distGnd = (current.pos - mGanondorfPosCurrent).absXZ();
            }
            if (mbEyesFollowGanondorf && (distLook < l_HIO.mD0 || distGnd < l_HIO.mD0)) {
                mPrevMode = mMode;
                if (cM_rndF(100.0f) < 50.0f) {
                    modeProc(PROC_INIT_e, MODE_BACK_STEP);
                } else {
                    modeProc(PROC_INIT_e, MODE_SIDE_STEP);
                }
                return;
            }
        }
    }

    if (isAnm(4) && mpMorf->isStop()) {
        m_jnt.setTrn();
        setAnm(5, false, 0xF);
        return;
    }

    if (isAnm(5)) {
        m_jnt.setTrn();
        if (cLib_calcTimer(&m08EC) == 0) {
            setAnm(6, false, 0xF);
        }
        return;
    }

    if (isAnm(6) && mpMorf->isStop()) {
        m_jnt.clrTrn();
        if (m075C != 0) {
            m0758--;
            if (cLib_calcTimer(&m0758) != 0) {
                modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
            } else {
                m075C = 0;
            }
        } else if (m0768 != 0) {
            m0764--;
            if (cLib_calcTimer(&m0764) != 0) {
                modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
            } else {
                m0768 = 0;
            }
        } else if (cLib_calcTimer(&m08F0) == 0) {
            modeProc(PROC_INIT_e, MODE_MOVE);
        } else {
            m08EA = 1;
        }
    }
}

/* 0000445C-000044B4       .text modeDefendInit__6daPz_cFv */
void daPz_c::modeDefendInit() {
    setAnm(0xA, true, 0xF);
    m08EC = 0x1E;
    m_jnt.clrTrn();
    m_jnt.onHeadLock();
    m_jnt.onBackBoneLock();
    m08EA = 1;
}

/* 000044B4-00004630       .text modeDefend__6daPz_cFv */
void daPz_c::modeDefend() {
    m0924 = 0.0f;
    m_jnt.clrTrn();

    s16 angle = cLib_targetAngleY(&current.pos, &mHitPos);
    cLib_addCalcAngleS2(&shape_angle.y, angle, 4, 0x1000);
    cLib_distanceAngleS(shape_angle.y, angle);

    if (mKnockback > 0.01f) {
        int land_prm;
        cXyz offset(0.0f, 0.0f, -mKnockback);
        cXyz move(0.0f, 0.0f, 0.0f);

        mDoMtx_stack_c::YrotS(mHitAngleY);
        mDoMtx_stack_c::multVec(&offset, &move);
        current.pos += move;

        cLib_addCalc0(&mKnockback, 1.0f, 7.0f);
        dComIfGp_particle_setSimpleLand(mObjAcch.m_gnd, &current.pos, &shape_angle, 1.25f, 1.5f,
                                        1.0f, &tevStr, &land_prm, 7);
    } else if (m075C != 0 || m0768 != 0) {
        modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
    } else if (cLib_calcTimer(&m08EC) == 0) {
        modeProc(PROC_INIT_e, MODE_MOVE);
    }

    if (checkTgHit()) {
        return;
    }
}

/* 00004630-0000470C       .text modeDownInit__6daPz_cFv */
void daPz_c::modeDownInit() {
    cLib_offBit<u32>(attention_info.flags, fopAc_Attn_ACTION_SPEAK_e);
    cLib_offBit<u32>(attention_info.flags, fopAc_Attn_LOCKON_TALK_e);
    setAnm(7, true, 0xF);
    fopAcM_monsSeStart(this, JA_SE_CV_ZL_GN_DAMAGE, 0);
    m08EA = 1;
    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    m_jnt.clrTrn();
    m_jnt.onHeadLock();
    m_jnt.onBackBoneLock();
    speed.y = l_HIO.mC4;
    m0F70 = l_HIO.mC8;
    m08EC = l_HIO.mCC;
}

/* 0000470C-00004C20       .text modeDown__6daPz_cFv */
void daPz_c::modeDown() {
    int land_prm;

    m0924 = 0.0f;
    speedF = 0.0f;
    m_jnt.clrTrn();

    if (isAnm(7) && mpMorf->getMorfRatio() == 1.0f && mObjAcch.ChkGroundHit()) {
        setAnm(8, false, 0xF);
    }

    if (cLib_calcTimer(&m08EC) != 0) {
        cXyz offset(m0F70, 0.0f, 0.0f);
        cXyz move(0.0f, 0.0f, 0.0f);

        mDoMtx_stack_c::YrotS(shape_angle.y);
        mDoMtx_stack_c::multVec(&offset, &move);
        current.pos += move;

        if (isAnm(8) && mpMorf->isStop()) {
            dComIfGp_particle_setSimpleLand(mObjAcch.m_gnd, &current.pos, &shape_angle, 1.25f, 1.5f,
                                            1.0f, &tevStr, &land_prm, 7);
        }
    }

    int frame = (int)mpMorf->getFrame();

    if (isAnm(8)) {
        if ((f32)frame == 1.0f + REG12_F(10)) {
            static JGeometry::TVec3<f32> run_splash_scale(0.6f, 0.6f, 0.6f);
            JPABaseEmitter* emitter = dComIfGp_particle_setSimpleLand(
                mObjAcch.m_gnd, &current.pos, &shape_angle, 2.5f, 3.0f, 2.0f, &tevStr, &land_prm, 7);
            if (emitter != NULL) {
                emitter->setRate(18.0f);
                emitter->setSpread(1.0f);
                emitter->setGlobalScale(run_splash_scale);
            }
        }

        if ((f32)frame == 2.0f + REG12_F(10)) {
            static JGeometry::TVec3<f32> run_splash_scale(0.6f, 0.6f, 0.6f);
            JPABaseEmitter* emitter = dComIfGp_particle_setSimpleLand(
                mObjAcch.m_gnd, &mWaistPos, &shape_angle, 2.5f, 3.0f, 2.0f, &tevStr, &land_prm, 7);
            if (emitter != NULL) {
                emitter->setRate(18.0f);
                emitter->setSpread(1.0f);
                emitter->setGlobalScale(run_splash_scale);
            }
        }

        if ((f32)frame == 3.0f + REG12_F(10)) {
            static JGeometry::TVec3<f32> run_splash_scale(0.6f, 0.6f, 0.6f);
            JPABaseEmitter* emitter = dComIfGp_particle_setSimpleLand(
                mObjAcch.m_gnd, &mEyePos, &shape_angle, 2.5f, 3.0f, 2.0f, &tevStr, &land_prm, 7);
            if (emitter != NULL) {
                emitter->setRate(18.0f);
                emitter->setSpread(1.0f);
                emitter->setGlobalScale(run_splash_scale);
            }
        }
    }

    if (isAnm(8) && mpMorf->isStop()) {
        m08EA = 1;
    }

    if (mTalkState == 1) {
        return;
    }

    if (isAnm(9)) {
        if (mpMorf->isStop()) {
            m08EA = 0;
            setAnm(2, false, 0xF);
        }
    } else if (m0740 != 0) {
        m0740 = 0;
#if VERSION > VERSION_DEMO
        m0F7C = l_HIO.mF8[2];
        m0F80 = 0;
        m075C = 0;
        m0768 = 0;
#endif
        modeProc(PROC_INIT_e, MODE_MOVE);
    } else {
        setAnm(9, false, 0xF);
    }
}

/* 00004C20-00004C78       .text modeAfraidInit__6daPz_cFv */
void daPz_c::modeAfraidInit() {
    setAnm(0xB, true, 0xF);
    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    m_jnt.offHeadLock();
    m_jnt.offBackBoneLock();
}

/* 00004C78-00004E8C       .text modeAfraid__6daPz_cFv */
void daPz_c::modeAfraid() {
    fopAc_ac_c* gnd;

    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    m_jnt.clrTrn();
    m073F = 0;
    m_jnt.offHeadLock();
    m_jnt.offBackBoneLock();

    if (fopAcM_SearchByName(fpcNm_GND_e, &gnd)) {
        if (REG12_S(9) == 1) {
            mLookPos = gnd->current.pos;
            mLookPos.y = mLookPos.y + (1200.0f + REG12_F(10));
        } else if (REG12_S(9) == 2) {
            mLookPos = gnd->eyePos;
            mLookPos.y = mLookPos.y + (800.0f + REG12_F(10));
        } else {
            mLookPos = gnd->current.pos;

            f32 dist = (current.pos - gnd->current.pos).absXZ();
            f32 min_y = 50.0f + gnd->current.pos.y;
            mLookPos.y = 8.0f * (gnd->eyePos.y - dist / 2.0f);

            if (mLookPos.y <= min_y) {
                mLookPos.y = min_y;
            }
        }

        cLib_addCalcAngleS2(&shape_angle.y, cLib_targetAngleY(&current.pos, &gnd->current.pos), 4,
                            0x400);
    }
}

/* 00004E8C-00004F08       .text modeSideStepInit__6daPz_cFv */
void daPz_c::modeSideStepInit() {
    if (mPrevMode != MODE_ATTACKWAIT && mPrevMode != MODE_ATTACK) {
        setAnm(0xB, true, 0xF);
    }
    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    speed.y = l_HIO.mBC;
    m0F70 = l_HIO.mC0;
}

/* 00004F08-00005098       .text modeSideStep__6daPz_cFv */
void daPz_c::modeSideStep() {
    if (checkTgHit()) {
        mWaist2AngleY = 0;
        return;
    }
    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    m_jnt.setTrn();
    m073F = 0;
#if VERSION == VERSION_DEMO
    cLib_addCalcAngleS2(&mWaist2AngleY, m0920 * 0x1194, 4, 0x800);
#else
    s16 target = (s16)(m0920 * 0x1194);
    cLib_addCalcAngleS2(&mWaist2AngleY, target, 4, 0x800);
#endif
    s16 angY = cLib_targetAngleY(&mEyePos, &mLookPos);
    cXyz offset(m0F70 * m0920, 0.0f, 0.0f);
    cXyz dst(0.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::YrotS(angY);
    mDoMtx_stack_c::multVec(&offset, &dst);
    current.pos += dst;
    if (mObjAcch.ChkGroundHit()) {
        mWaist2AngleY = 0;
        int dummy;
        dComIfGp_particle_setSimpleLand(
            mObjAcch.m_gnd, &current.pos, &shape_angle,
            m_smoke_ef, m_grass_ef, 1.0f, &tevStr, &dummy, 7
        );
        if (mPrevMode == MODE_ATTACKWAIT || mPrevMode == MODE_ATTACK) {
            modeProc(PROC_INIT_e, MODE_ATTACK);
        } else {
            modeProc(PROC_INIT_e, MODE_MOVE);
        }
    }
}

/* 00005098-00005114       .text modeBackStepInit__6daPz_cFv */
void daPz_c::modeBackStepInit() {
    if (mPrevMode != MODE_ATTACKWAIT && mPrevMode != MODE_ATTACK) {
        setAnm(0xB, true, 0xF);
    }
    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    speed.y = l_HIO.mB4;
    m0F70 = l_HIO.mB8;
}

/* 00005114-0000527C       .text modeBackStep__6daPz_cFv */
void daPz_c::modeBackStep() {
    if (checkTgHit()) {
        mWaist2AngleZ = 0;
        return;
    }
    m0924 = 0.0f;
    speedF = 0.0f;
    m08EA = 0;
    m_jnt.setTrn();
    m073F = 0;
    cLib_addCalcAngleS2(&mWaist2AngleZ, -0x1194, 4, 0x800);
    s16 angY = cLib_targetAngleY(&current.pos, &mLookPos);
    cXyz offset(0.0f, 0.0f, -m0F70);
    cXyz dst(0.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::YrotS(angY);
    mDoMtx_stack_c::multVec(&offset, &dst);
    current.pos += dst;
    if (mObjAcch.ChkGroundHit()) {
        mWaist2AngleZ = 0;
        int dummy;
        dComIfGp_particle_setSimpleLand(
            mObjAcch.m_gnd, &current.pos, &shape_angle,
            m_smoke_ef, m_grass_ef, 1.0f, &tevStr, &dummy, 7
        );
        if (mPrevMode == MODE_ATTACKWAIT || mPrevMode == MODE_ATTACK) {
            modeProc(PROC_INIT_e, MODE_ATTACK);
        } else {
            modeProc(PROC_INIT_e, MODE_MOVE);
        }
    }
}

/* 0000527C-00005304       .text modeTalkInit__6daPz_cFv */
void daPz_c::modeTalkInit() {
#if VERSION > VERSION_DEMO
    setAnm(2, true, 0xF);
#endif
    speedF = 0.0f;
    m0924 = 0.0f;
    m08EA = 0;
    m_jnt.offHeadLock();
    m_jnt.offBackBoneLock();
#if VERSION > VERSION_DEMO
    if (mTalkState == 0) {
        m0F80 = 1;
    } else {
        m0F7C = l_HIO.mF8[mTalkState];
    }
#endif
}

/* 00005304-0000539C       .text modeTalk__6daPz_cFv */
void daPz_c::modeTalk() {
    mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    m_jnt.setTrn();
    if (talk(1) == fopMsgStts_BOX_CLOSED_e) {
        dComIfGp_event_reset();
        modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
    }
}

/* 0000539C-0000549C       .text modeFollowInit__6daPz_cFv */
void daPz_c::modeFollowInit() {
    cLib_onBit<u32>(attention_info.flags, fopAc_Attn_ACTION_SPEAK_e);
    cLib_onBit<u32>(attention_info.flags, fopAc_Attn_LOCKON_TALK_e);
    m08EC = l_HIO.m86[mTalkState] + cM_rndF(l_HIO.m8C[mTalkState]);
    m08F0 = 0xA;
    m08F4 = 0x78;
    setAnm(3, false, 0xF);
    m08EA = 0;
    m_jnt.clrTrn();
    m_jnt.offHeadLock();
    m_jnt.offBackBoneLock();
}

/* 0000549C-000059B8       .text modeFollow__6daPz_cFv */
void daPz_c::modeFollow() {
    bool blocked = false;
    mLinChk.Set(&current.pos, &mFollowPos, this);
    if (dComIfG_Bgsp()->LineCross(&mLinChk)) {
        blocked = true;
    }
    if (mObjAcch.ChkWallHit()) {
        blocked = true;
    }

    if (checkTgHit()) {
        return;
    }

    if (l_HIO.m30 == 0) {
        if (mbHasGanondorf) {
            mLookPos = mGanondorfPosEye;
        }
    } else {
        mLookPos = dNpc_playerEyePos(l_HIO.mNpc.m04);
    }

    m_jnt.clrTrn();

    f32 distFollow = (current.pos - mFollowPos).absXZ();

    if (distFollow > 200.0f && mbEyesFollowGanondorf == 0 && !blocked) {
        m0924 = l_HIO.m50;
    } else if (mbEyesFollowGanondorf) {
        m0924 = 0.0f;
    } else if (blocked) {
        m0924 = 2.0f;
    } else {
        m0924 = 1.0f;
    }

    f32 radius = l_HIO.mEC;
    fopAc_ac_c* player = dComIfGp_getLinkPlayer();

    mFollowPos = mGanondorfPosCurrent;

    s16 ang = cLib_targetAngleY(&player->current.pos, &mGanondorfPosCurrent);
    cLib_distanceAngleS(ang, shape_angle.y);

    mFollowPos.x += radius * cM_ssin(REG12_S(0) + (ang + 0x4000));
    mFollowPos.z += radius * cM_scos(REG12_S(0) + (ang + 0x4000));
    mFollowPos.y += 200.0f;

    if (cLib_calcTimer(&m08F0) == 0) {
        f32 distGnd = 100000.0f;
        f32 distLook = (current.pos - mLookPos).absXZ();
        if (mbHasGanondorf) {
            distGnd = (current.pos - mGanondorfPosCurrent).absXZ();
        }

        if (mbEyesFollowGanondorf && (distLook < l_HIO.mD0 || distGnd < l_HIO.mD0)) {
            mPrevMode = mMode;
            modeProc(PROC_INIT_e, MODE_BACK_STEP);
            return;
        }
    }

    cLib_addCalcAngleS2(&shape_angle.y, cLib_targetAngleY(&current.pos, &mFollowPos), 8, 0x400);

    if (speedF < 0.5f) {
        setAnm(2, false, 0xF);
    } else {
        setAnm(3, false, 0xF);
    }

    if (l_HIO.m34[1] == 0 && cLib_calcTimer(&m08EC) == 0) {
        modeProc(PROC_INIT_e, MODE_ATTACKWAIT);
    }

    mEventOrder = 2;
}

/* 000059B8-00005C58       .text modeProc__6daPz_cFQ26daPz_c6Proc_ei */
void daPz_c::modeProc(daPz_c::Proc_e proc, int newMode) {
    struct ModeEntry {
        ModeFunc init;
        ModeFunc run;
        const char* name;
    };

    static ModeEntry mode_tbl[] = {
        {&daPz_c::modeWaitInit, &daPz_c::modeWait, "WAIT"},
        {&daPz_c::modeMoveInit, &daPz_c::modeMove, "MOVE"},
        {&daPz_c::modeAttackWaitInit, &daPz_c::modeAttackWait, "ATTACKWAIT"},
        {&daPz_c::modeAttackInit, &daPz_c::modeAttack, "ATTACK"},
        {&daPz_c::modeDefendInit, &daPz_c::modeDefend, "DEFEND"},
        {&daPz_c::modeDownInit, &daPz_c::modeDown, "DOWN"},
        {&daPz_c::modeAfraidInit, &daPz_c::modeAfraid, "AFRAID"},
        {&daPz_c::modeSideStepInit, &daPz_c::modeSideStep, "SIDE_STEP"},
        {&daPz_c::modeBackStepInit, &daPz_c::modeBackStep, "BACK_STEP"},
        {&daPz_c::modeTalkInit, &daPz_c::modeTalk, "TALK"},
        {&daPz_c::modeFollowInit, &daPz_c::modeFollow, "FOLLOW"},
    };

    if (proc == PROC_INIT_e) {
        mMode = newMode;
        (this->*mode_tbl[mMode].init)();
    } else if (proc == PROC_EXEC_e) {
        (this->*mode_tbl[mMode].run)();
    }
}

/* 00005C58-000060D8       .text _execute__6daPz_cFv */
bool daPz_c::_execute() {
    if (l_HIO.m33 != 0) {
        modeProc(PROC_INIT_e, MODE_WAIT);
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        current.pos = player->current.pos;
        current.pos.y += 50.0f;
        current.pos.x -= 200.0f * cM_ssin(player->shape_angle.y);
        current.pos.z -= 200.0f * cM_scos(player->shape_angle.y);
    }

    setFallSplash();
    getGndPos();
    mbEyesFollowGanondorf = false;
    if (mbHasGanondorf) {
        mbEyesFollowGanondorf = checkEyeArea(mGanondorfPosCurrent);
    }
    fopAcM_SetGravity(this, l_HIO.mB0);
    if (mMode != MODE_AFRAID && mMode != MODE_DOWN && m073F != 0) {
        modeProc(PROC_INIT_e, MODE_AFRAID);
    }
    setRipple();
    if (mMode != MODE_DOWN && mTalkState == 1) {
        modeProc(PROC_INIT_e, MODE_DOWN);
    }
    setJntStatus();
    if (demo()) {
        mBtkAnm.play();
        mBrkAnm.play();
        setMtx();
        g_env_light.settingTevStruct(0, &current.pos, &tevStr);
        return true;
    }

    current.angle = shape_angle;
    mpMorf->calc();
    enemy_fire(&mEnemyFire);
    if (enemy_ice(&mEventIce)) {
        J3DModel* model = mpMorf->getModel();
        model->setBaseTRMtx(mDoMtx_stack_c::get());
        return true;
    }

    if (mAnmPrmIdx == 9) {
        setHeadSplash();
    }

#if VERSION > VERSION_DEMO
    if ((mArg != 0 || (mTalkState != 0 && mTalkState != 2)) ||
        m0F80 != 0 || mMode == MODE_ATTACKWAIT || mMode == MODE_ATTACK ||
        mMode == MODE_DOWN || mMode == MODE_TALK) {
        // skip auto-talk trigger
    } else if (cLib_calcTimer(&m0F7C) == 0) {
        fopAc_ac_c* gnd;
        if (fopAcM_SearchByName(fpcNm_GND_e, &gnd)) {
            gnd_class* ganon = (gnd_class*)gnd;
            f32 dist = fopAcM_searchActorDistanceXZ(this, dComIfGp_getPlayer(0));
            if (dist < l_HIO.m100[mTalkState] && ganon->m2CE == 0) {
                mEventOrder = 1;
                modeProc(PROC_INIT_e, MODE_TALK);
            }
        }
    }
#endif

    checkOrder();
    modeProc(PROC_EXEC_e, 0xB);
    eventOrder();
    setAttention();
    cLib_addCalc2(&speedF, m0924, 0.3f, 4.0f);

    s8 roomNo = fopAcM_GetRoomNo(this);
    int mtrlSndId;
    if (mObjAcch.ChkGroundHit() != false) {
        mtrlSndId = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    } else {
        mtrlSndId = 0;
    }
    mpMorf->play(&eyePos, mtrlSndId, dComIfGp_getReverb(roomNo));
    mpBowMcaMorf->play(NULL, 0, 0);
    mBrkAnm.play();
    playEyeAnm();
    if (l_HIO.m2F != 0) {
        setAnm(l_HIO.m2F, false, 0xF);
    }
    fopAcM_posMoveF(this, NULL);
    mObjAcch.CrrPos(*dComIfG_Bgsp());
#if VERSION == VERSION_DEMO
    setCollision(30.0f, 130.0f);
#else
    if (!dComIfGp_event_runCheck()) {
        setCollision(30.0f, 130.0f);
    }
#endif
    setAnmRunSpeed();
    current.angle = shape_angle;
    setMtx();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    return true;
}

/* 00005BC4-00005C84       .text debugDraw__6daPz_cFv */
void daPz_c::debugDraw() {
    static const GXColor color_ok = {0x00, 0xFF, 0x00, 0x80};
    static const GXColor color_ng = {0xFF, 0x00, 0x00, 0x80};
    GXColor color = color_ok;
    if (mbEyesFollowGanondorf) {
        color = color_ng;
    }
    cXyz pos = current.pos;
    pos.y += 20.0f;
    dLib_debugDrawFan(pos, shape_angle.y, l_HIO.m54, l_HIO.m58, color);
    dLib_debugDrawAxis(mWaistMtx, 50.0f);
#if VERSION > VERSION_DEMO
    // Stripped retail debug code left a 10000.0f literal; this statement is a reconstruction.
    dLib_debugDrawAxis(mWaistMtx, 10000.0f);
#endif
    GXColor c1 = {0xFF, 0xFF, 0x00, 0x80};
    GXColor c2 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c3 = {0xFF, 0xFF, 0x00, 0x80};
    GXColor c4 = {0x00, 0x00, 0xFF, 0x80};
    GXColor c5 = {0xFF, 0x00, 0xFF, 0x80};
}

/* 000060D8-00006154       .text bowDraw__6daPz_cFv */
void daPz_c::bowDraw() {
    g_env_light.setLightTevColorType(mpBowMcaMorf->getModel(), &tevStr);
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(PZ_JNT_HAND_L1_e));
    mpBowMcaMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpBowMcaMorf->updateDL();
}

/* 00006154-00006600       .text bodyDraw__6daPz_cFv */
void daPz_c::bodyDraw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* modelData = model->getModelData();

    g_env_light.setLightTevColorType(model, &tevStr);
    mpMorf->calc();
    model->unlock();

    if (mArg != 0) {
        mBrkAnm.entry(modelData);
    }

    dComIfGd_setListP0();
    mBtpAnm.entry(modelData, m0F67);
    mOnCupOffAup2.entryOpa();

    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        J3DMaterial* mat = modelData->getMaterialNodePointer(i);
        if (i != 3 && i != 6 && i != 0xD && i != 0x10) {
            mat->getShape()->hide();
        } else {
            mat->getShape()->show();
        }
    }

    mRootJoint->entryIn();
    mOffCupOnAup2.entryOpa();

    mBtkAnm.entry(modelData);

    mEyeShape[0]->hide();
    mEyeShape[3]->hide();
    mEyeShape[2]->show();
    mEyeShape[5]->show();
    mEyebrowShape[0]->hide();
    mEyebrowShape[3]->hide();
    mEyebrowShape[2]->show();
    mEyebrowShape[5]->show();

    mRootJoint->entryIn();
    mBtkAnm.remove(modelData);

    mEyeShape[2]->hide();
    mEyeShape[5]->hide();
    mEyebrowShape[2]->hide();
    mEyebrowShape[5]->hide();

    for (int i = 0; i < 4; i++) {
        mFaceShape[i]->show();
    }

    mRootJoint->entryIn();
    mOnCupOffAup1.entryOpa();

    for (int i = 0; i < 4; i++) {
        mFaceShape[i]->hide();
    }

    mEyeShape[1]->show();
    mEyeShape[4]->show();
    mEyebrowShape[1]->show();
    mEyebrowShape[4]->show();

    mRootJoint->entryIn();
    mOffCupOnAup1.entryOpa();

    mEyeShape[1]->hide();
    mEyeShape[4]->hide();
    mEyebrowShape[1]->hide();
    mEyebrowShape[4]->hide();

    dComIfGd_setListP1();

    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        if (i == 3) continue;
        if (i == 2) continue;
        if (i == 1) continue;
        if (i == 6) continue;
        if (i == 5) continue;
        if (i == 4) continue;
        if (i == 0xd) continue;
        if (i == 0xc) continue;
        if (i == 0xb) continue;
        if (i == 0x10) continue;
        if (i == 0xf) continue;
        if (i == 0xe) continue;
        if (i == 0x12) continue;
        if (i == 0x13) continue;
        if (i == 8) continue;
        if (i == 7) continue;
        modelData->getMaterialNodePointer(i)->getShape()->show();
    }

    mpMorf->entryDL();
    dComIfGd_setList();

    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        modelData->getMaterialNodePointer(i)->getShape()->show();
    }

    mBtpAnm.remove(modelData);

    if (mArg != 0) {
        mBrkAnm.remove(modelData);
    }
}

/* 00006600-000066D8       .text drawShadow__6daPz_cFv */
void daPz_c::drawShadow() {
    cXyz pos(current.pos.x, current.pos.y + 100.0f + REG8_F(18), current.pos.z);
    mShadowId = dComIfGd_setShadow(
        mShadowId, 1, mpMorf->getModel(), &pos,
        250.0f + REG8_F(19), 40.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr
    );
    if (mbDrawBow && mShadowId != 0) {
        dComIfGd_addRealShadow(mShadowId, mpBowMcaMorf->getModel());
    }
}

/* 000066D8-0000676C       .text _draw__6daPz_cFv */
bool daPz_c::_draw() {
#if VERSION == VERSION_DEMO
    if (l_HIO.mNpc.m22) {
        debugDraw();
    }
#endif
    if (mEventIce.mFreezeTimer > 0x14) {
        dMat_control_c::iceEntryDL(mpMorf, -1, &mInvisibleModel);
    } else {
        bodyDraw();
    }
    if (mbDrawBow) {
        bowDraw();
    }
    drawShadow();
    dSnap_RegistFig(DSNAP_TYPE_UNK87, this, 1.0f, 1.0f, 1.0f);
    return true;
}

/* 0000676C-00006974       .text bodyCreateInit__6daPz_cFv */
void daPz_c::bodyCreateInit() {
    static u8 l_eyeMatNo[] = {3, 2, 1, 6, 5, 4};
    static u8 l_eyeblowMatNo[] = {13, 12, 11, 16, 15, 14};
    static u8 l_faceMatNo[] = {18, 19, 8, 7};

    if (strcmp(dComIfGp_getStartStageName(), "GTower") == 0) {
        eventInfo.setEventId(dComIfGp_evmng_getEventIdx("PZ_TALK", 0xFF));
    }

    J3DModelData* modelData = mpMorf->getModel()->getModelData();
    mRootJoint = modelData->getJointNodePointer(0);

    for (int i = 0; i < 6; i++) {
        mEyeMat[i] = modelData->getMaterialNodePointer(l_eyeMatNo[i]);
        mEyeShape[i] = mEyeMat[i]->getShape();
    }
    for (int i = 0; i < 6; i++) {
        mEyebrowMat[i] = modelData->getMaterialNodePointer(l_eyeblowMatNo[i]);
        mEyebrowShape[i] = mEyebrowMat[i]->getShape();
    }
    for (int i = 0; i < 4; i++) {
        mFaceMat[i] = modelData->getMaterialNodePointer(l_faceMatNo[i]);
        mFaceShape[i] = mFaceMat[i]->getShape();
    }

    J3DMaterialAnm* eyeAnm = mEyeMat[2]->getMaterialAnm();
    mEyeMat[1]->setMaterialAnm(eyeAnm);
    mEyeMat[0]->setMaterialAnm(eyeAnm);
    J3DMaterialAnm* eyeAnm2 = mEyeMat[5]->getMaterialAnm();
    mEyeMat[4]->setMaterialAnm(eyeAnm2);
    mEyeMat[3]->setMaterialAnm(eyeAnm2);

    J3DMaterialAnm* browAnm = mEyebrowMat[2]->getMaterialAnm();
    mEyebrowMat[1]->setMaterialAnm(browAnm);
    mEyebrowMat[0]->setMaterialAnm(browAnm);
    J3DMaterialAnm* browAnm2 = mEyebrowMat[5]->getMaterialAnm();
    mEyebrowMat[4]->setMaterialAnm(browAnm2);
    mEyebrowMat[3]->setMaterialAnm(browAnm2);
}

/* 00006974-00006BAC       .text createInit__6daPz_cFv */
void daPz_c::createInit() {
    static u8 fire_j[] = {7, 2, 12, 13, 17, 18, 22, 23, 25, 26};
    static f32 fire_sc[] = {2.0f, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    max_health = 0x1E;
    health = max_health;
#if VERSION > VERSION_DEMO
    stealItemLeft = 0xA;
#endif

    mEnemyFire.mpMcaMorf = mpMorf;
    mEnemyFire.mpActor = this;
    for (int i = 0; i < 10; i++) {
        mEnemyFire.mFlameJntIdxs[i] = fire_j[i];
        mEnemyFire.mParticleScale[i] = fire_sc[i];
    }

    mEventIce.mpActor = this;
    mEventIce.m00C = 1;
    mEventIce.mWallRadius = 50.0f;
    mEventIce.mCylHeight = 250.0f;

    mTalkState = 0;
#if VERSION > VERSION_DEMO
    m0F7C = l_HIO.mF8[0];
    m0F80 = 0;
#endif
    bodyCreateInit();

    if (mArg == 0) {
        mbDrawBow = 1;
        modeProc(PROC_INIT_e, MODE_MOVE);
        attention_info.distances[1] = DEMO_SELECT(0xB1, 3);
        attention_info.distances[3] = DEMO_SELECT(0xB1, 3);
        attention_info.flags |= fopAc_Attn_ACTION_SPEAK_e;
        attention_info.flags |= fopAc_Attn_LOCKON_TALK_e;
    } else {
        attention_info.flags = fopAc_Attn_ACTION_SPEAK_e | fopAc_Attn_LOCKON_TALK_e;
        modeProc(PROC_INIT_e, MODE_WAIT);
    }

    m0920 = 1;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -60.0f, -50.0f, -60.0f, 60.0f, 1800.0f, 60.0f);
    mAcchCir.SetWall(80.0f, 60.0f);
#if VERSION == VERSION_DEMO
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
#else
    mObjAcch.Set(&current.pos, &old.pos, this, 1, &mAcchCir, &speed, NULL, NULL);
#endif
    mObjAcch.SetRoofNone();
    gravity = l_HIO.mB0;
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(m_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(30.0f, 130.0f);
#if VERSION > VERSION_DEMO
    dKy_tevstr_init(&mTevstr, home.roomNo, 0xFF);
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
#endif
}

/* 00006BAC-00006CB0       .text _create__6daPz_cFv */
cPhs_State daPz_c::_create() {
    cPhs_State phase_state = dComIfG_resLoad(&mPhs, m_arc_name);
    fopAcM_SetupActor(this, daPz_c);
    if (phase_state == cPhs_COMPLEATE_e) {
        getArg();
        if (mArg != DEMO_SELECT(1, 0)) {
            if (strcmp(dComIfGp_getStartStageName(), "kenroom") == 0 && dComIfGs_isEventBit(0x3520) == TRUE) {
                return cPhs_ERROR_e;
            }
        }
        if (!fopAcM_entrySolidHeap(this, createHeap_CB, m_heapsize)) {
            return cPhs_ERROR_e;
        }
        createInit();
    }
    return phase_state;
}

/* 00007DA0-00007E20       .text _delete__6daPz_cFv */
bool daPz_c::_delete() {
    dComIfG_resDelete(&mPhs, m_arc_name);
    mRippleCb.remove();
    mFollowCb1.remove();
    mFollowCb2.remove();
#if VERSION > VERSION_DEMO
    if (heap != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return true;
}

/* 00007E20-00007E40       .text daPzCreate__FPv */
static cPhs_State daPzCreate(void* i_this) {
    return ((daPz_c*)i_this)->_create();
}

/* 00007E40-00007E64       .text daPzDelete__FPv */
static BOOL daPzDelete(void* i_this) {
    return ((daPz_c*)i_this)->_delete();
}

/* 00007E64-00007E88       .text daPzExecute__FPv */
static BOOL daPzExecute(void* i_this) {
    return ((daPz_c*)i_this)->_execute();
}

/* 00007E88-00007EAC       .text daPzDraw__FPv */
static BOOL daPzDraw(void* i_this) {
    return ((daPz_c*)i_this)->_draw();
}

/* 00007EAC-00007EB4       .text daPzIsDelete__FPv */
static BOOL daPzIsDelete(void*) {
    return TRUE;
}

static actor_method_class daPzMethodTable = {
    (process_method_func)daPzCreate,
    (process_method_func)daPzDelete,
    (process_method_func)daPzExecute,
    (process_method_func)daPzIsDelete,
    (process_method_func)daPzDraw,
};

actor_process_profile_definition g_profile_PZ = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_PZ_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daPz_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_PZ_e,
    /* Actor SubMtd */ &daPzMethodTable,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
