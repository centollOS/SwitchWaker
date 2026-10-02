/**
 * d_a_npc_ac1.cpp
 * NPC - Prince Komali (after Dragon Roost Cavern)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_ac1.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Ac.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include <string.h>
#include <math.h>

STATIC_ASSERT(sizeof(daNpc_Ac1_c) == 0x87C);
static daNpc_Ac1_HIO_c l_HIO;

daNpc_Ac1_HIO_c::daNpc_Ac1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x2000, 10000, -0x1000, -10000, 0, 6000, 0, -6000,
        1800, 1400, 150.0f, false, 6, 4, 0.5f, 3.0f, 0.1f, 40.0f,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    m04 = -1;
    m08 = -1;
}

daNpc_Ac1_HIO_c::~daNpc_Ac1_HIO_c() {}

static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;

static BOOL nodeCallBack_Wng(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0)
        ((daNpc_Ac1_c*)j3dSys.getModel()->getUserArea())->nodeWngControl(node, j3dSys.getModel());
    return TRUE;
}
void daNpc_Ac1_c::nodeWngControl(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_wngL1_jnt_num) {
        MTXCopy(mLeftShoulder, j3dSys.mCurrentMtx);
        model->setAnmMtx(joint, mLeftShoulder);
    }
    if (joint == m_wngR1_jnt_num) {
        MTXCopy(mRightShoulder, j3dSys.mCurrentMtx);
        model->setAnmMtx(joint, mRightShoulder);
    }
}
static BOOL nodeCallBack_Arm(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0)
        ((daNpc_Ac1_c*)j3dSys.getModel()->getUserArea())->nodeArmControl(node, j3dSys.getModel());
    return TRUE;
}
void daNpc_Ac1_c::nodeArmControl(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_armL1_jnt_num) {
        MTXCopy(mLeftShoulder, j3dSys.mCurrentMtx);
        model->setAnmMtx(joint, mLeftShoulder);
    }
    if (joint == m_armR1_jnt_num) {
        MTXCopy(mRightShoulder, j3dSys.mCurrentMtx);
        model->setAnmMtx(joint, mRightShoulder);
    }
}
static BOOL nodeCallBack_Ac1(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0) {
        ((daNpc_Ac1_c*)j3dSys.getModel()->getUserArea())->nodeAc1Control(node, j3dSys.getModel());
    }
    return TRUE;
}

void daNpc_Ac1_c::nodeAc1Control(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(20.0f, 18.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
        mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyePos);
    }
    if (joint == m_bbone_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    }
    if (joint == m_arm_L_jnt_num) MTXCopy(mDoMtx_stack_c::get(), mLeftShoulder);
    if (joint == m_arm_R_jnt_num) MTXCopy(mDoMtx_stack_c::get(), mRightShoulder);
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

bool daNpc_Ac1_c::init_AC1_0() {
    bool result = dComIfGs_isEventBit(0x2E04) != FALSE;
    if (result) { dComIfGs_isEventBit(0x1580); set_action(&daNpc_Ac1_c::wait_action1, NULL); }
    return result;
}

bool daNpc_Ac1_c::createInit() {
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 169;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 169;
    gravity = -4.5f;
    mLookPos = current.pos;
    u8 path = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (path != 0xFF) {
        mPathRun.setInf(path, fopAcM_GetRoomNo(this), 1);
        if (mPathRun.getPath() != NULL) fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
        else return false;
    }
    mEventCut.setActorInfo2("Ac1", this);
    mAnmNo = 4;
    bool result;
    switch (mSubType) {
    case 0: result = init_AC1_0(); break;
    default: result = false; break;
    }
    if (result) shape_angle = current.angle;
    else return false;
    mStts.Init(255, 255, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    if (mArms) mpArmMorf->setMorf(0.0f);
    else mpWingMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

void daNpc_Ac1_c::setMtx(bool force) {
    if (!mDemo) {
        plyTexPttrnAnm();
        mAnmEnd = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mFrame) mAnmEnd = 1;
        mFrame = mpMorf->getFrame();
        if (mArms) mpArmMorf->play(&eyePos, 0, 0);
        else mpWingMorf->play(&eyePos, 0, 0);
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    if (!mArms) mpWingMorf->calc();
    else mpArmMorf->calc();
    if (mpItemModel != NULL && mArms) {
        mpItemModel->setBaseTRMtx(mpArmMorf->getModel()->getAnmMtx(m_hnd_R_jnt_num));
        mpItemModel->calc();
    }
    setAttention(force);
}

int daNpc_Ac1_c::anmNum_toResID(int index) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_AC_BCK_AC_WAIT01_e, dRes_ID_AC_BCK_AC_TALK01_e, dRes_ID_AC_BCK_AC_TALK02_e, dRes_ID_AC_BCK_AC_WAIT02_e,
    };
    return a_bck_resID_tbl[index];
}

int daNpc_Ac1_c::wingAnmNum_toResID(int index) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_AC_BCK_ACARM_WAIT01_e, dRes_ID_AC_BCK_ACARM_TALK01_e, dRes_ID_AC_BCK_ACARM_TALK02_e, dRes_ID_AC_BCK_ACARM_WAIT02_e,
    };
    return a_bck_resID_tbl[index];
}

int daNpc_Ac1_c::btpNum_toResID(int index) {
    static const int a_btp_resID_tbl[] = {
        dRes_ID_AC_BTP_MABA_e, dRes_ID_AC_BTP_KOMARI_e, dRes_ID_AC_BTP_WARAI_e,
    };
    return a_btp_resID_tbl[index];
}

bool daNpc_Ac1_c::setBtp(bool modify, int number) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    m_hed_tex_pttrn = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Ac", btpNum_toResID(number));
    JUT_ASSERT(0x216, m_hed_tex_pttrn != 0);
    bool result = mBtpAnm.init(data, m_hed_tex_pttrn, true, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, modify, 0) == TRUE;
    if (result) {
        mBtpFrame = 0;
        mBtpTimer = 0;
    }
    return result;
}

bool daNpc_Ac1_c::iniTexPttrnAnm(bool modify) {
    return setBtp(modify, mBtpNo);
}

void daNpc_Ac1_c::plyTexPttrnAnm() {
    if (mBtpNo != 0 || cLib_calcTimer(&mBtpTimer) == 0) {
        mBtpFrame++;
        if (mBtpFrame >= m_hed_tex_pttrn->getFrameMax()) {
            if (mBtpNo != 0) mBtpFrame = m_hed_tex_pttrn->getFrameMax();
            else {
                mBtpTimer = 30.0f + cM_rndF(60.0f);
                mBtpFrame = 0;
            }
        }
    }
}

void daNpc_Ac1_c::setAnm_tex(s8 number) {
    if (mBtpNo != number) {
        mBtpNo = number;
        iniTexPttrnAnm(true);
    }
}

bool daNpc_Ac1_c::setAnm_anm(anm_prm_c* prm) {
    if (mAnmNo == prm->mAnm) return true;
    mAnmNo = prm->mAnm;
    int resource = anmNum_toResID(mAnmNo);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, resource, -1, "Ac");
    resource = wingAnmNum_toResID(mAnmNo);
    mArms = prm->mArms == 1;
    if (mArms) dNpc_setAnmIDRes(mpArmMorf, prm->mLoop, prm->mMorf, prm->mSpeed, resource, -1, "Ac");
    else dNpc_setAnmIDRes(mpWingMorf, prm->mLoop, prm->mMorf, prm->mSpeed, resource, -1, "Ac");
    mFrame = 0.0f; m859 = 0; mAnmEnd = 0;
    return true;
}

bool daNpc_Ac1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, -1, -1},
        {0, 0, 8.0f, 1.0f, 2, 1},
        {-1, -1, 0.0f, 0.0f, -1, -1}
    };
    if (a_anm_prm_tbl[mState].mBtp >= 0) setAnm_tex(a_anm_prm_tbl[mState].mBtp);
    if (a_anm_prm_tbl[mState].mAnm >= 0) setAnm_anm(&a_anm_prm_tbl[mState]);
    return true;
}

void daNpc_Ac1_c::chg_anmTag() {

}

void daNpc_Ac1_c::control_anmTag() {

}

void daNpc_Ac1_c::chg_anmAtr(u8 attr) {
    if (attr >= 6 || attr == mAnmAttr) return;
    mAnmAttr = attr; setAnm_ATR(1);
}

void daNpc_Ac1_c::control_anmAtr() {
    switch (mAnmAttr) {
    case 6: break;
    }
}

void daNpc_Ac1_c::setAnm_ATR(int texture) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0,0,8.0f,1.0f,2,1},
        {1,0,8.0f,1.0f,2,1},
        {2,1,8.0f,1.0f,2,1},
        {3,0,8.0f,1.0f,2,1},
        {1,2,8.0f,1.0f,2,1},
        {1,1,8.0f,1.0f,2,1}
    };
    if (texture) setAnm_tex(a_anm_prm_tbl[mAnmAttr].mBtp);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

void daNpc_Ac1_c::anmAtr(u16 status) {
    switch (status) {
    case 6: {
        if (mMsgAnmStarted == 0) {
            mAnmAttr = 0xFF;
            chg_anmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        u8 tag = dComIfGp_getMesgAnimeTagInfo();
        dComIfGp_clearMesgAnimeTagInfo();
        if (tag != 0xFF && mAnmTag != tag) {
            mAnmTag = tag;
            chg_anmTag();
        }
        break;
    }
    case 14: mMsgAnmStarted = 0; break;
    }
    control_anmTag();
    control_anmAtr();
}

void daNpc_Ac1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    }
}

void daNpc_Ac1_c::checkOrder() {
    if (eventInfo.getCommand() == dEvtCmd_INDEMO_e) {
    } else if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0; mTalking = true;
    }
}

bool daNpc_Ac1_c::chk_talk() {
    bool result = true;
    mItemNo = 0xFF;
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) mItemNo = dComIfGp_event_getPreItemNo();
        else result = false;
    }
    return result;
}

bool daNpc_Ac1_c::chk_partsNotMove() {
    return mPreviousHeadY == m_jnt.getHead_y() && mPreviousBackboneY == m_jnt.getBackbone_y() && mPreviousAngleY == current.angle.y;
}

void daNpc_Ac1_c::lookBack() {
    cXyz target;
    cXyz base;
    mPreviousHeadY = m_jnt.getHead_y();
    mPreviousBackboneY = m_jnt.getBackbone_y();
    mPreviousAngleY = current.angle.y;
    base = current.pos;
    base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 1:
        target = dNpc_playerEyePos(-20.0f);
        targetP = &target;
        base = current.pos;
        base.y = eyePos.y;
        break;
    case 2:
        target = mLookPos;
        targetP = &target;
        base = current.pos;
        base.y = eyePos.y;
        break;
    case 3: angle = mLookAngle; break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mPrm.mTurnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) mTurnSpeed = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, noTurn);
}

u16 daNpc_Ac1_c::next_msgStatus(u32* msg) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    switch (*msg) {
    case 0x184C: *msg = 0x184D; break;
    case 0x184D: *msg = 0x184E; break;
    case 0x1850: *msg = 0x1851; break;
    case 0x1851: *msg = 0x1852; break;
    default: status = fopMsgStts_MSG_ENDS_e; break;
    }
    return status;
}
s8 daNpc_Ac1_c::getBitMask() {
    s8 mask = 0;
    switch (mSubType) { case 0: mask = 0x10; break; }
    return mask;
}

u32 daNpc_Ac1_c::getMsg_AC1_0() {
    s8 reg = dComIfGs_getEventReg(0xB8FF);
    s8 mask = getBitMask();
    if (dComIfGs_isEventBit(0x1580)) {
        if (mask & reg) return 0x1853;
        int flags = reg;
        flags |= mask;
        dComIfGs_setEventReg(0xB8FF, flags);
        return 0x1850;
    } else {
        if (mask & reg) return 0x184F;
        int flags = reg;
        flags |= mask;
        dComIfGs_setEventReg(0xB8FF, flags);
        return 0x184C;
    }
}

u32 daNpc_Ac1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0: msg = getMsg_AC1_0(); break;
    }
    return msg;
}

bool daNpc_Ac1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) return this == attention.LockonTarget(0);
    return this == attention.ActionTarget(0);
}

void daNpc_Ac1_c::setAttention(bool force) {
#if VERSION == VERSION_DEMO
    f32 offset = l_HIO.mPrm.mAttentionOffsetY;
    attention_info.position.set(current.pos.x, current.pos.y + offset, current.pos.z);
#else
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionOffsetY, current.pos.z);
#endif
    if (mUpdateEye || force) eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
}

bool daNpc_Ac1_c::charDecide(int type) {
    mType = 0;
    mSubType = -1;
    mSubType = 0;
    return true;
}

void daNpc_Ac1_c::event_actionInit(int staff) {
    int* action = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    if (action != NULL) mEventAction = *action;
}

bool daNpc_Ac1_c::event_action() {
    switch (mEventAction) {
    case 0: return true;
    default: return true;
    }
}

void daNpc_Ac1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"ACTION"};
    if (staff == -1) return;
    mCut = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, FALSE);
    if (mCut == -1) { dComIfGp_evmng_cutEnd(staff); return; }
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch (mCut) {
        case 0: event_actionInit(staff); break;
        }
    }
    bool result;
    switch (mCut) {
    case 0: result = event_action(); break;
    default: result = true; break;
    }
    if (result) dComIfGp_evmng_cutEnd(staff);
}

void daNpc_Ac1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

int daNpc_Ac1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

void daNpc_Ac1_c::event_proc(int staff) {
    if (!mEventCut.cutProc()) privateCut(staff);
    lookBack();
}

bool daNpc_Ac1_c::set_action(ActionFunc action, void* arg) {
    if (mAction != action) {
        if (mAction) {
            mActionState = 9;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

void daNpc_Ac1_c::setStt(s8 state) {
    s8 previous = mState;
    mLookTimer = 0; mState = state;
    switch (mState) {
    case 2: mAnmAttr = 0xFF; mLookMode = 1; mPreviousState = previous; m_jnt.setTrn(); break;
    case 1:
    default: setAnm(); break;
    }
}

BOOL daNpc_Ac1_c::wait_1() {
    if (mOrder == 1 || mOrder >= 3) return TRUE;
    if (mTalking) { if (chk_talk()) setStt(2); return TRUE; }
    mOrder = 2;
    if (mHasAttention) mLookTimer = 60;
    if (cLib_calcTimer(&mLookTimer)) mLookMode = 1;
    else { mLookMode = 3; mLookAngle = mInitialAngle.y; m_jnt.setTrn(); }
    return TRUE;
}

BOOL daNpc_Ac1_c::talk_1() {
    BOOL notMoving = chk_partsNotMove();
    if (mpCurrMsg != NULL) mPreviousMsgStatus = mpCurrMsg->mStatus;
    else mPreviousMsgStatus = 0;
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case 19:
            dComIfGs_onEventBit(0x3F02); mItemNo = 0xFF; mTalking = false;
            setStt(mPreviousState); mLookTimer = 60; endEvent(); break;
        case 2: case 6: break;
        }
    }
    return notMoving;
}

int daNpc_Ac1_c::wait_action1(void* arg) {
    switch (mActionState) {
    case 0: setStt(1); mActionState++; break;
    case 1: case 2: case 3:
        mHasAttention = chkAttention();
        switch (mState) { case 1: mUpdateEye = wait_1(); break; case 2: mUpdateEye = talk_1(); break; }
        lookBack(); break;
    case 9: break;
    }
    return 1;
}

u8 daNpc_Ac1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) mDemo = false;
    } else {
        mDemo = true;
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (m_hed_tex_pttrn != NULL) {
            mBtpFrame++;
            if (mBtpFrame >= m_hed_tex_pttrn->getFrameMax()) mBtpFrame = m_hed_tex_pttrn->getFrameMax();
        }
        J3DAnmTexPattern* btp = actor->getP_BtpData("Ac");
        if (btp != NULL) {
            m_hed_tex_pttrn = btp;
            if (mBtpAnm.init(mpMorf->getModel()->getModelData(), m_hed_tex_pttrn, true, 2, 1.0f, 0, -1, true, 0)) {
                mBtpNo = 3;
                mBtpFrame = 0;
            }
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Ac");
    }
    return mDemo;
}

void daNpc_Ac1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId != 0) {
        if (mArms) dComIfGd_addRealShadow(mShadowId, mpArmMorf->getModel());
        else dComIfGd_addRealShadow(mShadowId, mpWingMorf->getModel());
        if (mpItemModel != NULL) dComIfGd_addRealShadow(mShadowId, mpItemModel);
    }
}

BOOL daNpc_Ac1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor red = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue = {0x00, 0x00, 0xFF, 0x80};
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    if (mHidden || mNoDraw) return TRUE;
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtpAnm.entry(data, mBtpFrame); mpMorf->entryDL(); mBtpAnm.remove(data);
    if (mArms) { g_env_light.setLightTevColorType(mpArmMorf->getModel(), &tevStr); mpArmMorf->entryDL(); }
    else { g_env_light.setLightTevColorType(mpWingMorf->getModel(), &tevStr); mpWingMorf->entryDL(); }
    if (mpItemModel != NULL && mArms) { g_env_light.setLightTevColorType(mpItemModel, &tevStr); mDoExt_modelEntryDL(mpItemModel); }
    shadowDraw(); dSnap_RegistFig(0x8B, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

BOOL daNpc_Ac1_c::_execute() {
    if (!mSavedInitialPos) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mSavedInitialPos = true;
    }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackBoneX, l_HIO.mPrm.mMaxBackBoneY,
        l_HIO.mPrm.mMinBackBoneX, l_HIO.mPrm.mMinBackBoneY,
        l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY,
        l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurnStep);
    if (mHidden && demoActorID == 0) return TRUE;
    mNoMove = false;
    mHidden = false;
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) staff = isEventEntry();
        if (staff >= 0) event_proc(staff);
        else (this->*mAction)(NULL);
        if (!mNoMove) fopAcM_posMoveF(this, mStts.GetCCMoveP());
        if (!mKeepShapeAngle) shape_angle = current.angle;
    }
    eventOrder();
    setMtx(false);
    if (!mDemo) setCollision(50.0f, 140.0f);
    return TRUE;
}

BOOL daNpc_Ac1_c::_delete() {
    dComIfG_resDelete(&mPhs, "Ac");
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) mpMorf->stopZelAnime();
    if (mpWingMorf != NULL) mpWingMorf->stopZelAnime();
    if (mpArmMorf != NULL) mpArmMorf->stopZelAnime();
    l_HIO.removeHIO();
#else
    if (heap != NULL) {
        if (mpMorf != NULL) mpMorf->stopZelAnime();
        if (mpWingMorf != NULL) mpWingMorf->stopZelAnime();
        if (mpArmMorf != NULL) mpArmMorf->stopZelAnime();
    }
#endif
    return TRUE;
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Ac1_c*)actor)->CreateHeap();
}

cPhs_State daNpc_Ac1_c::_create() {
    fopAcM_ct_Retail(this, daNpc_Ac1_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Ac");
    if (phase != cPhs_COMPLEATE_e) return phase;
    u32 prm = fopAcM_GetParam(this) & 0xFF;
    if (!charDecide(prm)) return cPhs_ERROR_e;
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("コモリ成長");
    fopAcM_ct(this, daNpc_Ac1_c);
#endif
    static u32 a_size_tbl[] = {0x272E0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_size_tbl[mType])) return cPhs_ERROR_e;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 140.0f, 50.0f);
    if (!createInit()) return cPhs_ERROR_e;
    return phase;
}

J3DModelData* daNpc_Ac1_c::create_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BDL_AC_e);
    JUT_ASSERT(0x6be, a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BCK_AC_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL) return NULL;
    if (mpMorf->getModel() == NULL) { mpMorf = NULL; return NULL; }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(0x6d2, m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(0x6d5, m_bbone_jnt_num >= 0);
    m_arm_L_jnt_num = a_mdl_dat->getJointName()->getIndex("armL");
    JUT_ASSERT(0x6d8, m_arm_L_jnt_num >= 0);
    m_arm_R_jnt_num = a_mdl_dat->getJointName()->getIndex("armR");
    JUT_ASSERT(0x6db, m_arm_R_jnt_num >= 0);
    return a_mdl_dat;
}

J3DModelData* daNpc_Ac1_c::create_wng_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BDL_ACWING_e);
    JUT_ASSERT(0x6ea, a_mdl_dat != 0);
    mpWingMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BCK_ACARM_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000002);
    if (mpWingMorf == NULL) return NULL;
    if (mpWingMorf->getModel() == NULL) { mpWingMorf = NULL; return NULL; }
    m_wngL1_jnt_num = a_mdl_dat->getJointName()->getIndex("wingL_loc");
    JUT_ASSERT(0x6ff, m_wngL1_jnt_num >= 0);
    m_wngR1_jnt_num = a_mdl_dat->getJointName()->getIndex("wingR_loc");
    JUT_ASSERT(0x702, m_wngR1_jnt_num >= 0);
    return a_mdl_dat;
}

J3DModelData* daNpc_Ac1_c::create_arm_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BDL_ACARM_e);
    JUT_ASSERT(0x711, a_mdl_dat != 0);
    mpArmMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BCK_ACARM_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000002);
    if (mpArmMorf == NULL) return NULL;
    if (mpArmMorf->getModel() == NULL) { mpArmMorf = NULL; return NULL; }
    m_hnd_R_jnt_num = a_mdl_dat->getJointName()->getIndex("handR");
    JUT_ASSERT(0x726, m_hnd_R_jnt_num >= 0);
    m_armL1_jnt_num = a_mdl_dat->getJointName()->getIndex("armL_loc");
    JUT_ASSERT(0x729, m_armL1_jnt_num >= 0);
    m_armR1_jnt_num = a_mdl_dat->getJointName()->getIndex("armR_loc");
    JUT_ASSERT(0x72c, m_armR1_jnt_num >= 0);
    return a_mdl_dat;
}

bool daNpc_Ac1_c::create_itm_Mdl() {
    mpItemModel = NULL;
    J3DModelData* a_mdl_dat;
    if (dComIfGs_isEventBit(0x1580)) a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BDL_AC_FLOWER02_e);
    else a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ac", dRes_ID_AC_BDL_AC_FLOWER_e);
    JUT_ASSERT(0x744, a_mdl_dat != 0);
    mpItemModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
    return true;
}

BOOL daNpc_Ac1_c::CreateHeap() {
    J3DModelData* data = create_Anm();
    if (data == NULL) return FALSE;
    mBtpNo = 0;
    if (!iniTexPttrnAnm(false)) { mpMorf = NULL; return FALSE; }
    J3DModelData* wing = create_wng_Anm();
    if (wing == NULL) { mpMorf = NULL; return FALSE; }
    J3DModelData* arm = create_arm_Anm();
    if (arm != NULL && create_itm_Mdl()) {
        for (u16 i = 0; i < wing->getJointNum(); i++) {
            if (i == m_wngL1_jnt_num || i == m_wngR1_jnt_num)
                mpWingMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Wng);
        }
        mpWingMorf->getModel()->setUserArea((u32)this);
        for (u16 i = 0; i < arm->getJointNum(); i++) {
            if (i == m_armL1_jnt_num || i == m_armR1_jnt_num)
                mpArmMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Arm);
        }
        mpArmMorf->getModel()->setUserArea((u32)this);
        for (u16 i = 0; i < data->getJointNum(); i++) {
            if (i == m_hed_jnt_num || i == m_bbone_jnt_num || i == m_arm_L_jnt_num || i == m_arm_R_jnt_num)
                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Ac1);
        }
        mpMorf->getModel()->setUserArea((u32)this);
        mAcchCir.SetWall(30.0f, 50.0f);
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
        return TRUE;
    }
    mpMorf = NULL; mpWingMorf = NULL; return FALSE;
}

static cPhs_State daNpc_Ac1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Ac1_c*)i_this)->_create();
}

static BOOL daNpc_Ac1_Delete(daNpc_Ac1_c* i_this) {
    return ((daNpc_Ac1_c*)i_this)->_delete();
}

static BOOL daNpc_Ac1_Execute(daNpc_Ac1_c* i_this) {
    return ((daNpc_Ac1_c*)i_this)->_execute();
}

static BOOL daNpc_Ac1_Draw(daNpc_Ac1_c* i_this) {
    return ((daNpc_Ac1_c*)i_this)->_draw();
}

static BOOL daNpc_Ac1_IsDelete(daNpc_Ac1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Ac1_Method = {
    (process_method_func)daNpc_Ac1_Create,
    (process_method_func)daNpc_Ac1_Delete,
    (process_method_func)daNpc_Ac1_Execute,
    (process_method_func)daNpc_Ac1_IsDelete,
    (process_method_func)daNpc_Ac1_Draw,
};

actor_process_profile_definition g_profile_NPC_AC1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_AC1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Ac1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_AC1_e,
    /* Actor SubMtd */ &l_daNpc_Ac1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
