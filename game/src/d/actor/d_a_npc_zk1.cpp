/**
 * d_a_npc_zk1.cpp
 * NPC - Rito Chieftain
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_zk1.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Zk.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include <string.h>

STATIC_ASSERT(sizeof(daNpc_Zk1_c) == 0x7B4);
static daNpc_Zk1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;


/* 000000EC-00000150       .text __ct__15daNpc_Zk1_HIO_cFv */
daNpc_Zk1_HIO_c::daNpc_Zk1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x1FFE, 0x251C, -3000, -9500, 0, 4500, -700, -4500,
        1600, 1200, 240.0f, false,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    m04 = -1;
    m08 = -1;
}

daNpc_Zk1_HIO_c::~daNpc_Zk1_HIO_c() {}

static const char* l_evn_tbl[] = {"dummy"};

/* 00000198-000001E4       .text nodeCB_Head__FP7J3DNodei */
static BOOL nodeCB_Head(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0) {
        ((daNpc_Zk1_c*)j3dSys.getModel()->getUserArea())->_nodeCB_Head(node, j3dSys.getModel());
    }
    return TRUE;
}

/* 000001E4-00000304       .text _nodeCB_Head__11daNpc_Zk1_cFP7J3DNodeP8J3DModel */
void daNpc_Zk1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(14.0f, 18.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    m750.x = mDoMtx_stack_c::get()[0][3];
    m750.y = mDoMtx_stack_c::get()[1][3];
    m750.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &m72C);
    mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000340-0000038C       .text nodeCB_BackBone__FP7J3DNodei */
static BOOL nodeCB_BackBone(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0) {
        ((daNpc_Zk1_c*)j3dSys.getModel()->getUserArea())->_nodeCB_BackBone(node, j3dSys.getModel());
    }
    return TRUE;
}

/* 0000038C-0000042C       .text _nodeCB_BackBone__11daNpc_Zk1_cFP7J3DNodeP8J3DModel */
void daNpc_Zk1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 0000042C-0000044C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Zk1_c*)actor)->CreateHeap();
}

/* 0000044C-00000518       .text init_ZK1_0__11daNpc_Zk1_cFv */
bool daNpc_Zk1_c::init_ZK1_0() {
    if (dComIfGs_isSymbol(1)) {
        set_action(&daNpc_Zk1_c::wait_action1, NULL);
        mHadEvent1802 = dComIfGs_isEventBit(0x1802) == TRUE;
        mHadEvent1C01 = dComIfGs_isEventBit(0x1C01) == TRUE;
        return true;
    }
    return false;
}

/* 00000518-00000698       .text createInit__11daNpc_Zk1_cFv */
bool daNpc_Zk1_c::createInit() {
    for (int i = 0; i < 1; i++) {
        mEventIdx[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    }
    mEventCut.setActorInfo2("Zk1", this);
    attention_info.flags = 10;
    switch (mSubType) {
    case 0: default:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 169;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 169;
        gravity = 0.0f;
        break;
    }
    mAnmNo = 4;
    bool result;
    switch (mSubType) {
    case 0: result = init_ZK1_0(); break;
    default: result = false; break;
    }
    if (!result) return false;
    mModelAngle = current.angle;
    shape_angle = mModelAngle;
    mStts.Init(255, 255, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    play_animation();
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

/* 00000698-00000740       .text play_animation__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::play_animation() {
    u32 sound = 0;
    play_btp_anm();
    if (mObjAcch.ChkGroundHit()) sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    mAnmEnd = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    if (mpMorf->getFrame() < mFrame) mAnmEnd = 1;
    mFrame = mpMorf->getFrame();
}

/* 00000740-000007F0       .text setMtx__11daNpc_Zk1_cFb */
void daNpc_Zk1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(mModelAngle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    setAttention(force);
}

/* 000007F0-00000804       .text bckResID__11daNpc_Zk1_cFi */
int daNpc_Zk1_c::bckResID(int index) {
    static const int a_resID_tbl[] = {
        dRes_ID_ZK_BCK_ZK_WAIT01_e, dRes_ID_ZK_BCK_ZK_WAIT02_e,
        dRes_ID_ZK_BCK_ZK_TALK01_e, dRes_ID_ZK_BCK_ZK_TALK02_e,
    };
    return a_resID_tbl[index];
}

/* 00000804-00000818       .text btpResID__11daNpc_Zk1_cFi */
int daNpc_Zk1_c::btpResID(int index) {
    static const int a_resID_tbl[] = {dRes_ID_ZK_BTP_MABA_e};
    return a_resID_tbl[index];
}

/* 00000818-0000091C       .text setBtp__11daNpc_Zk1_cFScb */
bool daNpc_Zk1_c::setBtp(s8 number, bool modify) {
    J3DModel* model = mpMorf->getModel();
    if (number < 0) return false;
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes(mArcName, btpResID(number));
    JUT_ASSERT(0x18F, a_btp != 0);
    mBtpNo = number;
    mBtpFrame = 0;
    mBtpTimer = 0;
    return mBtpAnm.init(model->getModelData(), a_btp, true, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, modify, 0) != 0;
}

/* 0000091C-0000093C       .text init_texPttrnAnm__11daNpc_Zk1_cFScb */
bool daNpc_Zk1_c::init_texPttrnAnm(s8 number, bool modify) {
    return setBtp(number, modify);
}

/* 0000093C-000009D8       .text play_btp_anm__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::play_btp_anm() {
    u8 end = mBtpAnm.getBtpAnm()->getFrameMax();
    if (mBtpNo != 0 || cLib_calcTimer(&mBtpTimer) == 0) {
        mBtpFrame++;
        if (mBtpFrame >= end) {
            if (mBtpNo != 0) {
                mBtpFrame = end;
            } else {
                mBtpTimer = cLib_getRndValue(60, 90);
                mBtpFrame = 0;
            }
        }
    }
}

/* 000009D8-00000A74       .text setAnm_anm__11daNpc_Zk1_cFPQ211daNpc_Zk1_c9anm_prm_c */
void daNpc_Zk1_c::setAnm_anm(anm_prm_c* prm) {
    s8 number = prm->mAnm;
    if (number < 0 || mAnmNo == number) return;
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bckResID(number), -1, mArcName);
    mAnmNo = prm->mAnm;
    mAnmEnd = 0;
    m795 = 0;
    mFrame = 0.0f;
}

/* 00000A74-00000AE0       .text setAnm__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mState].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

/* 00000AE0-00000AE4       .text chngAnmTag__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::chngAnmTag() {

}

/* 00000AE4-00000AE8       .text ctrlAnmTag__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::ctrlAnmTag() {

}

/* 00000AE8-00000B28       .text chngAnmAtr__11daNpc_Zk1_cFUc */
void daNpc_Zk1_c::chngAnmAtr(u8 attr) {
    if (attr == mAnmAttr || attr > 4) return;
    mAnmAttr = attr;
    setAnm_ATR();
}

/* 00000B28-00000B34       .text ctrlAnmAtr__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 4: break;
    }
}

/* 00000B34-00000B98       .text setAnm_ATR__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP}, {1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP}, {3, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00000B98-00000C54       .text anmAtr__11daNpc_Zk1_cFUs */
void daNpc_Zk1_c::anmAtr(u16 status) {
    switch (status) {
    case 6: {
        if (mMsgAnmStarted == 0) {
            chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        u8 tag = dComIfGp_getMesgAnimeTagInfo();
        if (tag != 0xFF && tag != mAnmTag) {
            dComIfGp_clearMesgAnimeTagInfo();
            mAnmTag = tag;
            chngAnmTag();
        }
        break;
    }
    case 14: mMsgAnmStarted = 0; break;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

/* 00000C54-00000D30       .text next_msgStatus__11daNpc_Zk1_cFPUl */
u16 daNpc_Zk1_c::next_msgStatus(u32* msg) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    switch (*msg) {
    case 0x17A2: *msg = 0x17A3; break;
    case 0x17A3: *msg = 0x17A4; break;
    case 0x17A4: *msg = 0x17A5; break;
    case 0x17A5: *msg = 0x17A6; break;
    case 0x17A8: *msg = 0x17A9; break;
    case 0x17A9: *msg = 0x17AA; break;
    case 0x17AA: *msg = 0x17AB; break;
    case 0x17AD:
        if (dKy_daynight_check()) *msg = 0x17AF;
        else *msg = 0x17AE;
        break;
    default: status = fopMsgStts_MSG_ENDS_e; break;
    }
    return status;
}

/* 00000D30-00000E10       .text getMsg_ZK1_0__11daNpc_Zk1_cFv */
u32 daNpc_Zk1_c::getMsg_ZK1_0() {
    if (!dComIfGs_isEventBit(0x1A80)) {
        return dComIfGs_isEventBit(0x1802) ? 0x17A7 : 0x17A2;
    } else if (!mHadEvent1802) {
        return dComIfGs_isEventBit(0x1802) ? 0x17AC : 0x17A8;
    } else if (!mHadEvent1C01) {
        return dComIfGs_isEventBit(0x1C01) ? 0x17B0 : 0x17AD;
    }
    return dComIfGs_isEventBit(0x1D80) ? 0x17B2 : 0x17B1;
}

/* 00000E10-00000E4C       .text getMsg__11daNpc_Zk1_cFv */
u32 daNpc_Zk1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0: msg = getMsg_ZK1_0(); break;
    }
    return msg;
}

/* 00000E4C-00000ED8       .text eventOrder__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    } else if (mOrder >= 3) {
        mEventNo = mOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIdx[mEventNo]);
    }
}

/* 00000ED8-00000F8C       .text checkOrder__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::checkOrder() {
    if (eventInfo.getCommand() == dEvtCmd_INDEMO_e) {
        if (dComIfGp_evmng_startCheck(mEventIdx[mEventNo]) && mOrder >= 3) {
            mOrder = 0;
            mAnmAttr = 0xFF;
            mAnmTag = 0xFF;
        }
    } else if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0;
        mTalking = true;
    }
}

/* 00000F8C-00001024       .text chk_talk__11daNpc_Zk1_cFv */
bool daNpc_Zk1_c::chk_talk() {
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mItemNo = dComIfGp_event_getPreItemNo();
            return true;
        }
        return false;
    }
    mItemNo = 0xFF;
    return true;
}

/* 00001024-00001064       .text chk_parts_notMov__11daNpc_Zk1_cFv */
bool daNpc_Zk1_c::chk_parts_notMov() {
    return mPreviousAngles.y != m_jnt.getHead_y() ||
        mPreviousAngles.z != m_jnt.getBackbone_y() || mPreviousAngles.x != current.angle.y;
}

/* 00001064-000010B8       .text searchByID__11daNpc_Zk1_cFUiPi */
fopAc_ac_c* daNpc_Zk1_c::searchByID(fpc_ProcID id, int* deleted) {
    fopAc_ac_c* actor = NULL;
    *deleted = FALSE;
    if (!fopAcM_SearchByID(id, &actor)) *deleted = TRUE;
    return actor;
}

/* 000010B8-0000127C       .text lookBack__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::lookBack() {
    cXyz target;
    cXyz base;
    mPreviousAngles.y = m_jnt.getHead_y();
    mPreviousAngles.z = m_jnt.getBackbone_y();
    mPreviousAngles.x = current.angle.y;
    base = current.pos;
    base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 1:
        mLookPos = dNpc_playerEyePos(-20.0f);
        target = mLookPos;
        targetP = &target;
        break;
    case 2:
        target = mLookPos;
        targetP = &target;
        break;
    case 3:
        angle = mLookAngle;
        break;
    case 4: {
        BOOL deleted;
        fopAc_ac_c* actor = searchByID(mPartnerId, &deleted);
        if (actor != NULL && !deleted) {
            mLookPos = actor->current.pos;
            mLookPos.y = actor->eyePos.y;
            target = mLookPos;
            targetP = &target;
        }
        break;
    }
    }
    m_jnt.lookAtTarget_2(&current.angle.y, targetP, base, angle, l_HIO.mPrm.mTurnSpeed, noTurn);
}

/* 0000127C-000012FC       .text chkAttention__11daNpc_Zk1_cFv */
bool daNpc_Zk1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) return this == attention.LockonTarget(0);
    return this == attention.ActionTarget(0);
}

/* 000012FC-00001354       .text setAttention__11daNpc_Zk1_cFb */
void daNpc_Zk1_c::setAttention(bool force) {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionOffsetY, current.pos.z);
    if (mUpdateEye || force) eyePos.set(m72C.x, m72C.y, m72C.z);
}

/* 00001354-000013DC       .text decideType__11daNpc_Zk1_cFi */
bool daNpc_Zk1_c::decideType(int type) {
    if (mType > 0) return true;
    mType = 1;
    mSubType = 0;
    strcpy(mArcName, "Zk");
    return mType != -1 && (s8)mSubType != -1;
}

/* 000013DC-0000147C       .text privateCut__11daNpc_Zk1_cFi */
void daNpc_Zk1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"DUMMY"};
    if (staff == -1) return;
    mCut = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, FALSE);
    if (mCut == -1) {
        dComIfGp_evmng_cutEnd(staff);
        return;
    }
#if VERSION == VERSION_DEMO
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch (mCut) {
        case 0:
            break;
        }
    }
    bool done;
    switch (mCut) {
    case 0:
    default:
        done = true;
        break;
    }
    if (done) {
        dComIfGp_evmng_cutEnd(staff);
    }
#else
    dComIfGp_evmng_getIsAddvance(staff);
    dComIfGp_evmng_cutEnd(staff);
#endif
}

/* 0000147C-000014A0       .text endEvent__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
}

/* 000014A0-000014D8       .text isEventEntry__11daNpc_Zk1_cFv */
int daNpc_Zk1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

/* 000014D8-0000155C       .text event_proc__11daNpc_Zk1_cFi */
void daNpc_Zk1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIdx[mEventNo])) {
        endEvent();
    } else if (!mEventCut.cutProc()) {
        privateCut(staff);
    }
}

/* 0000155C-00001608       .text set_action__11daNpc_Zk1_cFM11daNpc_Zk1_cFPCvPvPv_iPv */
bool daNpc_Zk1_c::set_action(ActionFunc action, void* arg) {
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

/* 00001608-0000167C       .text setStt__11daNpc_Zk1_cFSc */
void daNpc_Zk1_c::setStt(s8 state) {
    s8 previous = mState;
    mState = state;
    switch (mState) {
    case 0: break;
    case 1: mOrder = 0; break;
    case 2:
        mOrder = 0;
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        mMsgAnmStarted = 0;
        mPreviousState = previous;
        break;
    }
    setAnm();
}

/* 0000167C-000017FC       .text wait_1__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::wait_1() {
    s16 angle = 0;
    if (mReturnAngle) {
        cLib_addCalcAngleS(&current.angle.y, mInitialAngle.y, 4, 0x800, 0x80);
        angle = mInitialAngle.y - current.angle.y;
    }
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
            mLookMode = 1;
            mNoTurn = false;
            mReturnAngle = false;
            m_jnt.setTrn();
        }
        return TRUE;
    }
    mOrder = 2;
    mLookMode = 0;
    mNoTurn = true;
    if (angle == 0) {
        if (mHasAttention) mLookTimer = cLib_getRndValue(15, 30);
        if (cLib_calcTimer(&mLookTimer) != 0) mLookMode = 1;
        if (dNpc_chkAttn(this, dComIfGp_getPlayer(0)->current.pos, 200.0f, 50.0f, 76.0f, mLookMode == 1)) return TRUE;
        mLookMode = 0;
        mReturnAngle = true;
    }
    return TRUE;
}

/* 000017FC-00001920       .text talk_1__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::talk_1() {
    BOOL moved = chk_parts_notMov();
    talk(1);
    if (mpCurrMsg == NULL) return TRUE;
    switch (mpCurrMsg->mStatus) {
    case 2: case 6: break;
    case 19:
        switch (mCurrMsgNo) {
        case 0x17A6: case 0x17AB: dComIfGs_onEventBit(0x1802); break;
        case 0x17AE: case 0x17AF: dComIfGs_onEventBit(0x1C01); break;
        case 0x17B1: dComIfGs_onEventBit(0x1D80); break;
        }
        mItemNo = 0xFF;
        mTalking = false;
        setStt(mPreviousState);
        mLookTimer = cLib_getRndValue(15, 30);
        endEvent();
        break;
    }
    return moved;
}

/* 00001920-000019DC       .text wait_action1__11daNpc_Zk1_cFPv */
int daNpc_Zk1_c::wait_action1(void* arg) {
    switch (mActionState) {
    case 0:
        setStt(1);
        mReturnAngle = true;
        mActionState++;
        break;
    case 1: case 2: case 3:
        mHasAttention = chkAttention();
        switch (mState) {
        case 1: mUpdateEye = wait_1(); break;
        case 2: mUpdateEye = talk_1(); break;
        }
        break;
    case 9: break;
    }
    return 1;
}

/* 000019DC-00001B1C       .text demo__11daNpc_Zk1_cFv */
u8 daNpc_Zk1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) mDemo = false;
    } else {
        if (!mDemo) {
            mDemo = true;
            mKeepShapeAngle = false;
            m_jnt.setHead_y(0);
            m_jnt.setHead_x(0);
            m_jnt.setBackBone_y(0);
            m_jnt.setBackBone_x(0);
        }
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (mBtpAnm.getBtpAnm() != NULL) {
            u8 end = mBtpAnm.getBtpAnm()->getFrameMax();
            mBtpFrame++;
            if (mBtpFrame >= end) mBtpFrame = end;
        }
        J3DAnmTexPattern* btp = actor->getP_BtpData(mArcName);
        if (btp != NULL) {
            mBtpAnm.init(mpMorf->getModel()->getModelData(), btp, true, 0, 1.0f, 0, -1, true, 0);
            mBtpNo = 1;
            mBtpFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, mArcName);
    }
    return mDemo;
}

/* 00001B1C-00001BAC       .text shadowDraw__11daNpc_Zk1_cFv */
void daNpc_Zk1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
}

/* 00001BAC-00001C80       .text _draw__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    if (mHidden || mNoDraw) return TRUE;
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtpAnm.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtpAnm.remove(data);
    shadowDraw();
    GXColor red = {255, 0, 0, 128};
    GXColor blue = {0, 0, 255, 128};
    dSnap_RegistFig(DSNAP_TYPE_UNK8C, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 00001C80-00001EA8       .text _execute__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::_execute() {
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
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) staff = isEventEntry();
        if (staff >= 0 || mEventActive) event_proc(staff);
        else (this->*mAction)(NULL);
        lookBack();
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        mObjAcch.CrrPos(*dComIfG_Bgsp());
        play_animation();
    } else {
        mHidden = false;
    }
    eventOrder();
    mModelAngle = current.angle;
    if (!mKeepShapeAngle) shape_angle = current.angle;
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    setMtx(false);
    if (!mDemo) setCollision(80.0f, 220.0f);
    return TRUE;
}

/* 00001EA8-00001EFC       .text _delete__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::_delete() {
#if VERSION == VERSION_DEMO
    if (mLoaded) {
        l_HIO.removeHIO();
        dComIfG_resDelete(&mPhs, mArcName);
        if (mpMorf != NULL) mpMorf->stopZelAnime();
    }
#else
    dComIfG_resDelete(&mPhs, mArcName);
    if (heap != NULL && mpMorf != NULL) mpMorf->stopZelAnime();
#endif
    return TRUE;
}

/* 00001EFC-00002148       .text _create__11daNpc_Zk1_cFv */
cPhs_State daNpc_Zk1_c::_create() {
    fopAcM_ct_Retail(this, daNpc_Zk1_c);
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) return cPhs_ERROR_e;
    cPhs_State phase = dComIfG_resLoad(&mPhs, mArcName);
    mLoaded = phase == cPhs_COMPLEATE_e;
    if (!mLoaded) return phase;
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("親方様");
    fopAcM_ct(this, daNpc_Zk1_c);
#endif
    static u32 a_siz_tbl[] = {0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) {
#if VERSION == VERSION_DEMO
        mLoaded = false;
#endif
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -70.0f, -20.0f, -70.0f, 70.0f, 240.0f, 70.0f);
    if (!createInit()) return cPhs_ERROR_e;
    return phase;
}

/* 00002414-00002688       .text bodyCreateHeap__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_ZK_BDL_ZK_e);
    JUT_ASSERT(DEMO_SELECT(0x5B5, 0x5B3), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, NULL, -1, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL) return FALSE;
    if (mpMorf->getModel() == NULL) {
        mpMorf = NULL;
        return FALSE;
    }
    if (!init_texPttrnAnm(0, false)) {
        mpMorf = NULL;
        return FALSE;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x5C9, 0x5C7), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x5CB, 0x5C9), m_bbone_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}

/* 00002688-0000270C       .text CreateHeap__11daNpc_Zk1_cFv */
BOOL daNpc_Zk1_c::CreateHeap() {
    if (!bodyCreateHeap()) return FALSE;
    mAcchCir.SetWall(30.0f, 80.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return TRUE;
}

/* 0000270C-0000272C       .text daNpc_Zk1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Zk1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Zk1_c*)i_this)->_create();
}

/* 0000272C-0000274C       .text daNpc_Zk1_Delete__FP11daNpc_Zk1_c */
static BOOL daNpc_Zk1_Delete(daNpc_Zk1_c* i_this) {
    return ((daNpc_Zk1_c*)i_this)->_delete();
}

/* 0000274C-0000276C       .text daNpc_Zk1_Execute__FP11daNpc_Zk1_c */
static BOOL daNpc_Zk1_Execute(daNpc_Zk1_c* i_this) {
    return ((daNpc_Zk1_c*)i_this)->_execute();
}

/* 0000276C-0000278C       .text daNpc_Zk1_Draw__FP11daNpc_Zk1_c */
static BOOL daNpc_Zk1_Draw(daNpc_Zk1_c* i_this) {
    return ((daNpc_Zk1_c*)i_this)->_draw();
}

/* 0000278C-00002794       .text daNpc_Zk1_IsDelete__FP11daNpc_Zk1_c */
static BOOL daNpc_Zk1_IsDelete(daNpc_Zk1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Zk1_Method = {
    (process_method_func)daNpc_Zk1_Create,
    (process_method_func)daNpc_Zk1_Delete,
    (process_method_func)daNpc_Zk1_Execute,
    (process_method_func)daNpc_Zk1_IsDelete,
    (process_method_func)daNpc_Zk1_Draw,
};

actor_process_profile_definition g_profile_NPC_ZK1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_ZK1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Zk1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_ZK1_e,
    /* Actor SubMtd */ &l_daNpc_Zk1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
