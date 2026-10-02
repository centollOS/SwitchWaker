/**
 * d_a_npc_pf1.cpp
 * NPC - Maggie's Father (poor)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_pf1.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Pf.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include <string.h>
#include "d/actor/d_a_tama.h"
#include "c/c_dylink.h"
#include "SSystem/SComponent/c_counter.h"
#include "m_Do/m_Do_audio.h"

STATIC_ASSERT(sizeof(daNpc_Pf1_c) == DEMO_SELECT(0x7C8, 0x7C0));
static daNpc_Pf1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;


daNpc_Pf1_HIO_c::daNpc_Pf1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        8192, 14560, -5000, -14560, 0, 7280, 0, -7280,
        1024, 1024, 70.0f, 0, 0, 8, 1024,
        1.0f, 0.9f, 0.3f, 0.2f, 9.5f, 0.6f, 900.0f, 1050.0f,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    m04 = -1;
    m08 = -1;
}

daNpc_Pf1_HIO_c::~daNpc_Pf1_HIO_c() {}

static const char* l_evn_tbl[] = {"dummy"};

static BOOL nodeCB_Head(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0) {
        ((daNpc_Pf1_c*)j3dSys.getModel()->getUserArea())->_nodeCB_Head(node, j3dSys.getModel());
    }
    return TRUE;
}

void daNpc_Pf1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(20.0f, -30.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    m750.x = mDoMtx_stack_c::get()[0][3];
    m750.y = mDoMtx_stack_c::get()[1][3];
    m750.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &m72C);
    mDoMtx_stack_c::YrotM(-m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

static BOOL nodeCB_BackBone(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0) {
        ((daNpc_Pf1_c*)j3dSys.getModel()->getUserArea())->_nodeCB_BackBone(node, j3dSys.getModel());
    }
    return TRUE;
}

void daNpc_Pf1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(m_jnt.getBackbone_x());
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Pf1_c*)actor)->CreateHeap();
}

bool daNpc_Pf1_c::init_PF1_0() {
    if (!dComIfGs_isEventBit(0x2D01)) {
        set_action(&daNpc_Pf1_c::wait_action1, NULL);
        return true;
    }
    return false;
}

bool daNpc_Pf1_c::createInit() {
    mEventIdx[0] = dComIfGp_getPEvtManager()->getEventIdx(l_evn_tbl[0], 0xFF);
    mEventCut.setActorInfo2("Pf1", this);
    int weight = 255;
    u8 path = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (path != 0xFF) {
        mPath.setInf(path, fopAcM_GetRoomNo(this), 1);
        if (mPath.isPath()) {
            fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
            weight = 217;
            set_pthPoint(0);
        } else return false;
    }
    if (mPath.isPath() == false) return false;
    attention_info.flags = 10;
    switch (mSubType) {
    case 0: default:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 169;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 169;
        gravity = -4.5f;
        break;
    }
    mAnmNo = 8;
    bool result;
    switch (mSubType) {
    case 0: result = init_PF1_0(); break;
    default: result = false; break;
    }
    if (!result) return false;
    mModelAngle = current.angle;
    shape_angle = mModelAngle;
    mStts.Init(weight, 255, this);
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

void daNpc_Pf1_c::play_animation() {
    u32 sound = 0;
    play_btp_anm();
    if (mObjAcch.ChkGroundHit()) sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    mAnmEnd = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    if (mpMorf->getFrame() < mFrame) mAnmEnd = 1;
    mFrame = mpMorf->getFrame();
}

void daNpc_Pf1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(mModelAngle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    setAttention(force);
}

int daNpc_Pf1_c::bckResID(int index) {
    static const int a_resID_tbl[] = {
        dRes_ID_PF_BCK_WAIT01_e, dRes_ID_PF_BCK_WALK01_e,
        dRes_ID_PF_BCK_WALK02_e, dRes_ID_PF_BCK_CRY_e,
        dRes_ID_PF_BCK_REGRET_e, dRes_ID_PF_BCK_SPIT_e,
        dRes_ID_PF_BCK_WISH_e, dRes_ID_PF_BCK_TALK01_e,
    };
    return a_resID_tbl[index];
}

int daNpc_Pf1_c::btpResID(int index) {
    static const int a_resID_tbl[] = {dRes_ID_PF_BTP_MABA01_e, dRes_ID_PF_BTP_MABA01_e};
    return a_resID_tbl[index];
}

bool daNpc_Pf1_c::setBtp(s8 number, bool modify) {
    J3DModel* model = mpMorf->getModel();
    if (number < 0) return false;
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes(mArcName, btpResID(number));
    JUT_ASSERT(0x1CE, a_btp != 0);
    mBtpNo = number;
    if (mBtpNo == 1) mBtpFrame = 2;
    else mBtpFrame = 0;
    mBtpTimer = 0;
    return mBtpAnm.init(model->getModelData(), a_btp, true, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, modify, 0) != 0;
}

bool daNpc_Pf1_c::init_texPttrnAnm(s8 number, bool modify) {
    return setBtp(number, modify);
}

void daNpc_Pf1_c::play_btp_anm() {
    u8 end = mBtpAnm.getBtpAnm()->getFrameMax();
    if (mBtpNo != 1 && (mBtpNo != 0 || cLib_calcTimer(&mBtpTimer) == 0)) {
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

void daNpc_Pf1_c::setAnm_anm(anm_prm_c* prm) {
    s8 number = prm->mAnm;
    if (number < 0 || mAnmNo == number) return;
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bckResID(number), -1, mArcName);
    mAnmNo = prm->mAnm;
    mAnmEnd = 0;
    mAnmLoops = 0;
    mFrame = 0.0f;
}

void daNpc_Pf1_c::setAnm_NUM(int number, int texture) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2}, {1, 0, 8.0f, 1.0f, 2},
        {2, 0, 8.0f, 1.0f, 2}, {3, 1, 8.0f, 1.0f, 2},
        {4, 0, 8.0f, 1.0f, 0}, {5, 0, 8.0f, 1.0f, 0},
        {6, 0, 8.0f, 1.0f, 2}, {7, 0, 8.0f, 1.0f, 2},
    };
    if (texture) init_texPttrnAnm(a_anm_prm_tbl[number].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[number]);
}

void daNpc_Pf1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, -1}, {0, 0, 8.0f, 1.0f, 2},
        {-1, -1, 0.0f, 0.0f, -1}, {1, 0, 8.0f, 1.0f, 2},
        {4, 0, 8.0f, 1.0f, 0}, {0, 0, 4.0f, 0.0f, 2},
        {0, 0, 8.0f, 1.0f, 2}, {0, 0, 8.0f, 1.0f, 2},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mState].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

void daNpc_Pf1_c::chngAnmTag() {

}

void daNpc_Pf1_c::ctrlAnmTag() {

}

void daNpc_Pf1_c::chngAnmAtr(u8 attr) {
    if (attr == mAnmAttr || attr > 6) return;
    mAnmAttr = attr;
    setAnm_ATR();
}

void daNpc_Pf1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 4:
        if (mAnmEnd && ++mAnmLoops > 2) {
            mAnmAttr = 6;
            setAnm_NUM(0, 1);
        }
        break;
    case 0: case 1: case 2: case 3: case 5: case 6: break;
    }
}

void daNpc_Pf1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2}, {3, 1, 8.0f, 1.0f, 2},
        {5, 0, 8.0f, 1.0f, 0}, {6, 0, 8.0f, 1.0f, 2},
        {6, 0, 8.0f, 1.0f, 2}, {7, 0, 8.0f, 1.0f, 2},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

void daNpc_Pf1_c::anmAtr(u16 status) {
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

u16 daNpc_Pf1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x1B59:
        switch (mpCurrMsg->mSelectNum) {
        case 0: *msg = 0x1B5A; break;
        case 1: *msg = 0x1B5D; break;
        }
        mMet = 1;
        break;
    case 0x1B5A: *msg = 0x1B61; break;
    case 0x1B61: *msg = 0x1B5B; break;
    case 0x1B5B: *msg = 0x1B62; break;
    case 0x1B62: dComIfGs_onEventBit(0xB04); status = 16; break;
    case 0x1B60: m7A4 = 1; status = 16; break;
    case 0x1B5C: *msg = 0x1B5F; break;
    case 0x1B5E: *msg = 0x1B60; break;
    default: status = 16; break;
    }
    return status;
}

u32 daNpc_Pf1_c::getMsg_PF1_0() {
    if (dComIfGs_isEventBit(0xB04)) return 0x1B5C;
    return mMet ? 0x1B5E : 0x1B59;
}

u32 daNpc_Pf1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0: msg = getMsg_PF1_0(); break;
    }
    return msg;
}

void daNpc_Pf1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    } else if (mOrder >= 3) {
        mEventNo = mOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIdx[mEventNo]);
    }
}

void daNpc_Pf1_c::checkOrder() {
    if (eventInfo.getCommand() == dEvtCmd_INDEMO_e) {
        if (dComIfGp_evmng_startCheck(mEventIdx[mEventNo]) && mOrder >= 3) {
            switch (mEventNo) {
            case 0:
                break;
            }
            mOrder = 0;
            mAnmAttr = 0xFF;
            mAnmTag = 0xFF;
        }
        return;
    }
    if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0;
        mTalking = true;
    }
}

bool daNpc_Pf1_c::chk_talk() {
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

bool daNpc_Pf1_c::chk_parts_notMov() {
    return mPreviousAngles.y != m_jnt.getHead_y() ||
        mPreviousAngles.z != m_jnt.getBackbone_y() || mPreviousAngles.x != current.angle.y;
}

fopAc_ac_c* daNpc_Pf1_c::searchByID(fpc_ProcID id, int* deleted) {
    fopAc_ac_c* actor = NULL;
    *deleted = FALSE;
    if (!fopAcM_SearchByID(id, &actor)) *deleted = TRUE;
    return actor;
}

void daNpc_Pf1_c::lookBack() {
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

bool daNpc_Pf1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) return this == attention.LockonTarget(0);
    return this == attention.ActionTarget(0);
}

void daNpc_Pf1_c::setAttention(bool force) {
    attention_info.position.set(m750.x, m750.y + l_HIO.mPrm.mAttentionOffsetY, m750.z);
    if (mUpdateEye || force) eyePos.set(m72C.x, m72C.y, m72C.z);
}

bool daNpc_Pf1_c::decideType(int type) {
    if (mType > 0) return true;
    mType = 1;
    mSubType = 0;
    strcpy(mArcName, "Pf");
    return mType != -1 && (s8)mSubType != -1;
}

void daNpc_Pf1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"DUMMY"};
    if (staff == -1) return;
    mCut = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, FALSE);
    if (mCut == -1) {
        dComIfGp_evmng_cutEnd(staff);
        return;
    }
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch (mCut) {
        case 0: break;
        }
    }
    bool done;
    switch (mCut) {
    case 0:
    default: done = true; break;
    }
    if (done) dComIfGp_evmng_cutEnd(staff);
}

void daNpc_Pf1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
}

int daNpc_Pf1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

void daNpc_Pf1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIdx[mEventNo])) {
        switch (mEventNo) {
        case 0: default:
            endEvent();
            break;
        }
    } else if (!mEventCut.cutProc()) {
        privateCut(staff);
    }
}

bool daNpc_Pf1_c::set_action(ActionFunc action, void* arg) {
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

void daNpc_Pf1_c::setStt(s8 state) {
    s8 previous = mState;
    mState = state;
    switch (mState) {
    case 0: break;
    case 1: case 4: case 6: case 7:
        mOrder = 0;
        speedF = 0.0f;
        break;
    case 2:
        mOrder = 0;
        speedF = 0.0f;
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        mMsgAnmStarted = 0;
        mPreviousState = previous;
        break;
    case 3:
        mOrder = 0;
        mStateTimer = cLib_getRndValue(180, 90);
        break;
    case 5:
        mOrder = 0;
        mAttackTimer = 30;
        mStateTimer = 0;
        break;
    }
    setAnm();
}


void daNpc_Pf1_c::createTama(f32 distance) {
    csXyz angle(0, 0, 0);
    cXyz target;
    cXyz pos = eyePos;
    pos.y = 15.0f + eyePos.y;
    target = dNpc_playerEyePos(-20.0f);
    f32 targetDistance = (target - eyePos).absXZ();
    angle.y = cLib_targetAngleY(&pos, &target);
    angle.x = cLib_targetAngleX(&pos, &target);
    daTama_c* shot = (daTama_c*)fopAcM_fastCreate(fpcNm_TAMA_e, 0, &eyePos, fopAcM_GetRoomNo(this), &angle, NULL, -1, NULL, NULL);
    if (shot != NULL) {
        shot->setPartnerID(fopAcM_GetID(this));
        shot->setDis(distance);
        shot->setSpd(50.0f);
    }
}

bool daNpc_Pf1_c::chk_areaIN(f32 radius, cXyz pos) {
    bool inside = (dComIfGp_getLinkPlayer()->current.pos - pos).absXZ() < radius;
    if (inside && g_Counter.mCounter0 % 3 == 0) createTama(radius);
    return inside;
}

bool daNpc_Pf1_c::endEvent_check() {
    if (mMet) return !dComIfGs_isEventBit(0xB04);
    return false;
}

bool daNpc_Pf1_c::startEvent_check() {
    cXyz pos(mInitialPos.x, 100.0f + mInitialPos.y, mInitialPos.z);
    if (chk_areaIN(l_HIO.mPrm.mStartRadius, pos)) {
        if (current.pos.abs(dComIfGp_getLinkPlayer()->current.pos) < 210.0f || field_0x6ba != 0) return true;
    }
    return false;
}

void daNpc_Pf1_c::set_pthPoint(u8 point) {
    if (mPath.isPath() != false) {
        mPath.setIdx(point);
        current.pos = mPath.getPoint(mPath.getIdx());
        if (mPath.nextIdx()) {
            cXyz pos = mPath.getPoint(mPath.getIdx());
            current.angle.y = cLib_targetAngleY(&current.pos, &pos);
        }
    }
}

bool daNpc_Pf1_c::chk_attn() {
    f32 distance = (current.pos - dComIfGp_getLinkPlayer()->current.pos).absXZ();
    f32 height = current.pos.y - dComIfGp_getLinkPlayer()->current.pos.y;
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos) - current.angle.y;
    if (mLookMode == 1) {
        return distance < 200.0f && abs(angle) / 182.0444488525390625f < 90.0f && std::fabsf(height) < 300.0f;
    }
    return distance < 200.0f && abs(angle) / 182.0444488525390625f < 60.0f && std::fabsf(height) < 300.0f;
}

void daNpc_Pf1_c::setBikon(cXyz offset) {
    delBikon();
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    cXyz pos;
    mDoMtx_stack_c::multVec(&offset, &pos);
#if VERSION == VERSION_DEMO
    mpBikonEmitter = dComIfGp_particle_set(0x8152, &pos);
#else
    dComIfGp_particle_set(0x8152, &pos);
#endif
}

void daNpc_Pf1_c::delBikon() {
#if VERSION == VERSION_DEMO
    if (mpBikonEmitter != NULL) {
        mpBikonEmitter->becomeInvalidEmitter();
        mpBikonEmitter = NULL;
    }
#endif
}

BOOL daNpc_Pf1_c::wait_1() { return TRUE; }

BOOL daNpc_Pf1_c::regret() {
    if (mTalking) {
#if VERSION == VERSION_DEMO
        if (chk_talk()) {
            setStt(2);
            mLookMode = 1;
            mNoTurn = false;
            m_jnt.setTrn();
        }
#else
        if (chk_talk()) setStt(2);
#endif
        return TRUE;
    }
    if (!endEvent_check()) {
        mOrder = 0;
        if (startEvent_check()) setStt(5);
    } else mOrder = 2;
    mLookMode = 0;
    mNoTurn = true;
    if (mAnmEnd) setStt(3);
    return TRUE;
}

BOOL daNpc_Pf1_c::attk_1() {
    f32 distance = (current.pos - dComIfGp_getLinkPlayer()->current.pos).absXZ();
    if (mTalking) {
        if (chk_talk()) {
            setStt(6);
            setStt(2);
#if VERSION == VERSION_DEMO
            mLookMode = 1;
            mNoTurn = false;
            m_jnt.setTrn();
#endif
        }
        return TRUE;
    }
    mLookMode = 0;
    mNoTurn = true;
    if (mAnmNo == 0) {
        if (mStateTimer == 0) {
            s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos);
            cLib_addCalcAngleS(&current.angle.y, angle, 4, 0x1000, 0x80);
            if (abs((s16)(current.angle.y - angle)) < 0x800) {
                cXyz offset(0.0f, -40.0f, 60.0f);
                mDoAud_seStart(JA_SE_CM_CMN_NOTICE, &current.pos, 0, 0);
                setBikon(offset);
                mStateTimer = 20;
            }
        } else if (cLib_calcTimer(&mStateTimer) == 0) setAnm_NUM(2, 1);
        return TRUE;
    }
    if (mOrder != 1 && distance < 210.0f) mOrder = 1;
    if (cLib_calcTimer(&mAttackTimer) == 0 && (bool)(chk_areaIN(l_HIO.mPrm.mEndRadius, mInitialPos) == 0)) {
        setStt(6);
        return TRUE;
    }
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos);
    cLib_addCalcAngleS(&current.angle.y, angle, l_HIO.mPrm.mMoveTurnRate, l_HIO.mPrm.mMoveTurnSpeed, 0x80);
    f32 target = l_HIO.mPrm.mAttackSpeed;
    cLib_chaseF(&speedF, target, l_HIO.mPrm.mAttackAccel);
    f32 speed = speedF * l_HIO.mPrm.mAttackAnmSpeed;
    f32 rate = speed < 0.5f ? 0.5f : speed;
    mpMorf->setPlaySpeed(rate);
    return TRUE;
}

BOOL daNpc_Pf1_c::walk_1() {
    if (mPath.isPath() == false) return TRUE;
    if (dPath_ChkClose(mPath.getPath())) {
        if (mPath.chkPointPass(current.pos, (u8)(mPath.getDir() != 0))) mPath.nextIdxAuto();
    } else return TRUE;
    cXyz pos = mPath.getPoint(mPath.getIdx());
    s16 angle = cLib_targetAngleY(&current.pos, &pos);
    cLib_addCalcAngleS(&current.angle.y, angle, l_HIO.mPrm.mMoveTurnRate, l_HIO.mPrm.mMoveTurnSpeed, 0x80);
    f32 speed = l_HIO.mPrm.mWalkSpeed;
    if (cLib_calcTimer(&mStateTimer) == 0 || mTalking) speed = 0.0f;
    if (!endEvent_check() && startEvent_check()) {
        speedF = 0.0f;
        mStateTimer = 0;
        setStt(5);
        mLookMode = 0;
        mNoTurn = true;
        return TRUE;
    }
    cLib_chaseF(&speedF, speed, l_HIO.mPrm.mWalkAccel);
    f32 anmSpeed = speedF * l_HIO.mPrm.mWalkAnmSpeed;
    f32 rate = anmSpeed < 0.5f ? 0.5f : anmSpeed;
    mpMorf->setPlaySpeed(rate);
    if ((int)(0.5f + speed) == 0 && (int)speedF == 0) {
        if (mTalking) {
            if (chk_talk()) {
                setStt(2);
#if VERSION == VERSION_DEMO
                mLookMode = 1;
                mNoTurn = false;
                m_jnt.setTrn();
#endif
            }
            return TRUE;
        }
        setStt(4);
        return TRUE;
    }
    mOrder = 0;
    if (endEvent_check()) mOrder = 2;
    mLookMode = 0;
    mNoTurn = true;
    return TRUE;
}

BOOL daNpc_Pf1_c::wait_2() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
#if VERSION == VERSION_DEMO
            mLookMode = 1;
            mNoTurn = false;
            m_jnt.setTrn();
#endif
        }
        return TRUE;
    }
    if (!dComIfGs_isEventBit(0xB04) || (bool)(chk_areaIN(l_HIO.mPrm.mEndRadius, mInitialPos) == 0)) {
        mPath.setNearPathIndx(&current.pos, 0.0f);
        setStt(3);
        return TRUE;
    }
    mOrder = 2;
    mNoTurn = true;
    if (chk_attn()) {
        mLookMode = 1;
        return TRUE;
    }
    mLookMode = 0;
    return TRUE;
}

BOOL daNpc_Pf1_c::wait_3() {
    f32 distance = (current.pos - dComIfGp_getLinkPlayer()->current.pos).absXZ();
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
#if VERSION == VERSION_DEMO
            mLookMode = 1;
            mNoTurn = false;
            mReturnAngle = false;
            m_jnt.setTrn();
#else
            mReturnAngle = false;
#endif
        }
        return TRUE;
    }
    mReturnAngle = distance > 300.0f;
    if (mReturnAngle) {
        setStt(3);
        mLookMode = 0;
        mNoTurn = true;
        return TRUE;
    }
    mOrder = 2;
    mNoTurn = true;
    if (chk_attn()) {
        mLookMode = 1;
        return TRUE;
    }
    mLookMode = 0;
    return TRUE;
}

BOOL daNpc_Pf1_c::talk_1() {
    BOOL moved = chk_parts_notMov();
#if VERSION > VERSION_DEMO
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos);
    cLib_addCalcAngleS(&current.angle.y, angle, 4, l_HIO.mPrm.mTurnSpeed, 0x80);
#endif
    u16 status = talk(1);
    if (mpCurrMsg == NULL) return TRUE;
    if (status == 10 && mAnmEnd && mCurrMsgNo == 0x1B60) fopMsgM_messageSendOn();
    switch (mpCurrMsg->mStatus) {
    case 2: case 6: break;
    case 19:
        mItemNo = 0xFF;
        mTalking = false;
        setStt(mPreviousState);
        endEvent();
        break;
    }
    return moved;
}

int daNpc_Pf1_c::wait_action1(void* arg) {
    switch (mActionState) {
    case 0:
        setStt(3);
        mActionState++;
        break;
    case 1: case 2: case 3:
        mHasAttention = chkAttention();
        switch (mState) {
        case 1: mUpdateEye = wait_1(); break;
        case 2: mUpdateEye = talk_1(); break;
        case 3: mUpdateEye = walk_1(); break;
        case 4: mUpdateEye = regret(); break;
        case 5: mUpdateEye = attk_1(); break;
        case 6: mUpdateEye = wait_2(); break;
        case 7: mUpdateEye = wait_3(); break;
        }
        break;
    case 9: break;
    }
    return 1;
}

u8 daNpc_Pf1_c::demo() {
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
            mBtpNo = 2;
            mBtpFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, mArcName);
    }
    return mDemo;
}

void daNpc_Pf1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
}

BOOL daNpc_Pf1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor green = {0x00, 0xFF, 0x00, 0x80};
    GXColor yellow = {0xFF, 0xFF, 0x00, 0x80};
    GXColor red = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue = {0x00, 0x00, 0xFF, 0x80};
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    if (mHidden || mNoDraw) return TRUE;
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtpAnm.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtpAnm.remove(data);
    shadowDraw();
    dSnap_RegistFig(0x5B, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

BOOL daNpc_Pf1_c::_execute() {
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
        if (staff >= 0) event_proc(staff);
        else (this->*mAction)(NULL);
        field_0x6ba = 0;
        lookBack();
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        mObjAcch.CrrPos(*dComIfG_Bgsp());
        if (mObjAcch.GetGroundH() != DEMO_SELECT(-G_CM3D_F_INF, -1000000000.0f)) {
            cM3dGPla* plane = dComIfG_Bgsp()->GetTriPla(mObjAcch.m_gnd);
            if (plane != NULL) mGroundNormal = *plane->GetNP();
        }
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
    if (!mDemo) setCollision(100.0f, 200.0f);
    return TRUE;
}

BOOL daNpc_Pf1_c::_delete() {
#if VERSION == VERSION_DEMO
    if (mLoaded) {
        l_HIO.removeHIO();
        dComIfG_resDelete(&mPhs, mArcName);
        cDyl_Unlink(fpcNm_TAMA_e);
        delBikon();
        if (mpMorf != NULL) mpMorf->stopZelAnime();
    }
    return TRUE;
#else
    cDyl_Unlink(fpcNm_TAMA_e);
    dComIfG_resDelete(&mPhs, mArcName);
    delBikon();
    if (heap != NULL && mpMorf != NULL) mpMorf->stopZelAnime();
    return TRUE;
#endif
}

cPhs_State daNpc_Pf1_c::_create() {
#if VERSION == VERSION_DEMO
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) return cPhs_ERROR_e;
    cPhs_State phase = dComIfG_resLoad(&mPhs, mArcName);
    mLoaded = phase == cPhs_COMPLEATE_e;
    if (!mLoaded) return phase;
    l_HIO.entryHIO("貧乏マギ−の父");
    fopAcM_SetupActor(this, daNpc_Pf1_c);
    static u32 a_siz_tbl[] = {0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) {
        mLoaded = false;
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -100.0f, -20.0f, -80.0f, 100.0f, 180.0f, 140.0f);
    if (!createInit()) return cPhs_ERROR_e;
    cDyl_Link(fpcNm_TAMA_e);
    return phase;
#else
    fopAcM_SetupActor(this, daNpc_Pf1_c);
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) return cPhs_ERROR_e;
    cPhs_State phase = dComIfG_resLoad(&mPhs, mArcName);
    mLoaded = phase == cPhs_COMPLEATE_e;
    if (!mLoaded) return phase;
    int linkPhase = cDyl_LinkASync(fpcNm_TAMA_e);
    if (linkPhase != cPhs_COMPLEATE_e) return (cPhs_State)linkPhase;
    static u32 a_siz_tbl[] = {0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) return cPhs_ERROR_e;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -100.0f, -20.0f, -80.0f, 100.0f, 180.0f, 140.0f);
    if (!createInit()) return cPhs_ERROR_e;
    return phase;
#endif
}

BOOL daNpc_Pf1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_PF_BDL_PF_e);
    JUT_ASSERT(DEMO_SELECT(0x7F8, 0x803), a_mdl_dat != 0);
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
    JUT_ASSERT(DEMO_SELECT(0x80C, 0x817), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone1");
    JUT_ASSERT(DEMO_SELECT(0x80E, 0x819), m_bbone_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}

BOOL daNpc_Pf1_c::CreateHeap() {
    if (!bodyCreateHeap()) return FALSE;
    mAcchCir.SetWall(30.0f, 100.0f);
    cXyz* speed_p = &speed;
    cXyz* old_pos_p = &old.pos;
    cXyz* current_pos_p = &current.pos;
    mObjAcch.Set(current_pos_p, old_pos_p, this, 1, &mAcchCir, speed_p, NULL, NULL);
    return TRUE;
}

static cPhs_State daNpc_Pf1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Pf1_c*)i_this)->_create();
}

static BOOL daNpc_Pf1_Delete(daNpc_Pf1_c* i_this) {
    return ((daNpc_Pf1_c*)i_this)->_delete();
}

static BOOL daNpc_Pf1_Execute(daNpc_Pf1_c* i_this) {
    return ((daNpc_Pf1_c*)i_this)->_execute();
}

static BOOL daNpc_Pf1_Draw(daNpc_Pf1_c* i_this) {
    return ((daNpc_Pf1_c*)i_this)->_draw();
}

static BOOL daNpc_Pf1_IsDelete(daNpc_Pf1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Pf1_Method = {
    (process_method_func)daNpc_Pf1_Create,
    (process_method_func)daNpc_Pf1_Delete,
    (process_method_func)daNpc_Pf1_Execute,
    (process_method_func)daNpc_Pf1_IsDelete,
    (process_method_func)daNpc_Pf1_Draw,
};

actor_process_profile_definition g_profile_NPC_PF1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_PF1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Pf1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_PF1_e,
    /* Actor SubMtd */ &l_daNpc_Pf1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
