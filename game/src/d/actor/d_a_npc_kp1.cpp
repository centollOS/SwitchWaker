/**
* d_a_npc_kp1.cpp
* NPC - Maggie (rich)
*/
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_kp1.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_lib.h"
#include "res/Object/Kp.h"
static daNpc_Kp1_HIO_c l_HIO;
daNpc_Kp1_HIO_c::daNpc_Kp1_HIO_c() {
    static hio_prm_c a_prm_tbl = {0x1FFE, 0x38E0, (s16)0xF448, (s16)0xC720, 0, 0, 0, 0, 1500, 1024, 170.0f, 0};
    memcpy(&mPrm, &a_prm_tbl, sizeof(mPrm));
    mNo = -1;
    mCount = -1;
}

static BOOL nodeCallBack_Kp(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Kp1_c* npc = (daNpc_Kp1_c*)model->getUserArea();
        if (npc != NULL) {
            static cXyz a_att_pos_offst(0.0f, 0.0f, 0.0f);
            static cXyz a_eye_pos_offst(20.0f, -20.0f, 0.0f);
            int joint = ((J3DJoint*)node)->getJntNo();
            mDoMtx_stack_c::copy(model->getAnmMtx(joint));
            if (joint == npc->getHeadJntNum()) {
                mDoMtx_stack_c::multVec(&a_att_pos_offst, npc->getAttPos());
                mDoMtx_stack_c::YrotM(-npc->getHead_y());
                mDoMtx_stack_c::ZrotM(-npc->getHead_x());
                mDoMtx_stack_c::multVec(&a_eye_pos_offst, npc->getEyePos());
            } else if (joint == npc->getBackboneJntNum()) {
                mDoMtx_stack_c::XrotM(npc->getBackbone_y());
                mDoMtx_stack_c::ZrotM(-npc->getBackbone_x());
            }
            cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
            model->setAnmMtx(joint, mDoMtx_stack_c::get());
        }
    }
    return TRUE;
}

bool daNpc_Kp1_c::createInit() {
    if (!dComIfGs_isEventBit(0x2D01)) {
        return false;
    }
    mEventCut.setActorInfo2("Kp1", this);
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 171;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 171;
    gravity = -4.0f;
    mItemNo = 0xFF;
    mLookPos = current.pos;
    set_action(&daNpc_Kp1_c::wait_action1, NULL);
    shape_angle = current.angle;
    mStts.Init(0xFF, 0xFF, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    mUpdateEye = 1;
    setMtx();
    return true;
}

void daNpc_Kp1_c::setMtx() {
    if (!mDemo) {
        playTexPatternAnm();
        mAnmEnd = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mFrame) {
            mAnmEnd = 1;
        }
        mFrame = mpMorf->getFrame();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_handL_jnt_num));
    mpLetterModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpLetterModel->calc();
    mpOpenLetterModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpOpenLetterModel->calc();
    setAttention();
}

bool daNpc_Kp1_c::anmResID(int i_num, int* o_bck_num, int* o_bas_num) {
    static const int a_anm_idx_tbl[][2] = {
        {dRes_ID_KP_BCK_WAIT01_e, -1}, {dRes_ID_KP_BCK_HAPPY_e, -1},
        {dRes_ID_KP_BCK_HAND_e, -1}, {dRes_ID_KP_BCK_READ_e, -1},
        {dRes_ID_KP_BCK_PRAY_e, -1}, {dRes_ID_KP_BCK_THINK_e, -1},
    };
    JUT_ASSERT(0x12A, 0 <= i_num && i_num < ANM_END);
    JUT_ASSERT(0x12B, o_bck_num && o_bas_num);
    *o_bck_num = a_anm_idx_tbl[i_num][0];
    *o_bas_num = a_anm_idx_tbl[i_num][1];
    return true;
}

void daNpc_Kp1_c::BtpNum2ResID(int i_num, int* o_btp_num) {
    static const int a_btp_arc_ix_tbl[] = {dRes_ID_KP_BTP_MABA01_e, dRes_ID_KP_BTP_MABA01_e};
    JUT_ASSERT(0x141, 0 <= i_num && i_num < TEXPATTERN_END);
    *o_btp_num = a_btp_arc_ix_tbl[i_num];
}

void daNpc_Kp1_c::setAnm_tex(s8 tex) {
    if (tex >= 0 && tex != mTexNo) {
        mTexNo = tex;
        initTexPatternAnm(true);
    }
}

bool daNpc_Kp1_c::init_btp(bool modify, int tex) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    if (tex >= 0) {
        int id;
        BtpNum2ResID(tex, &id);
        m_head_tex_pattern = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Kp", id);
        JUT_ASSERT(0x162, m_head_tex_pattern != 0);
        if (!mBtp.init(data, m_head_tex_pattern, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, modify, 0)) {
            return false;
        }
        mBtpFrame = 0;
        mBlinkTimer = 0;
        if (mTexNo == 1) {
            mBtpFrame = 1;
        }
    }
    return true;
}

bool daNpc_Kp1_c::initTexPatternAnm(bool modify) {
    bool result = false;
    if (init_btp(modify, mTexNo)) {
        result = true;
    }
    return result;
}

void daNpc_Kp1_c::playTexPatternAnm() {
    bool advance = true;
    if (mTexNo == 1) {
        return;
    }
    if (mTexNo == 0) {
        advance = cLib_calcTimer(&mBlinkTimer) ? false : true;
    }
    if (advance) {
        s16 end = m_head_tex_pattern->getFrameMax();
        if (++mBtpFrame >= end) {
            if (mTexNo != 0) {
                mBtpFrame = m_head_tex_pattern->getFrameMax();
            } else {
                mBtpFrame = 0;
                mBlinkTimer = 30.0f + cM_rndF(60.0f);
            }
        }
    }
}

bool daNpc_Kp1_c::setAnm_anm(anm_prm_c* prm) {
    bool result = false;
    if (prm->mAnm >= 0 && mAnmNo != prm->mAnm) {
        mAnmNo = prm->mAnm;
        if (mpMorf != NULL) {
            int bck, bas;
            anmResID(mAnmNo, &bck, &bas);
            if (bck >= 0) {
                dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bck, bas, "Kp");
            }
            result = true;
        }
        mFrame = 0.0f;
        mAnmLoops = 0;
        mAnmEnd = 0;
    }
    return result;
}

void daNpc_Kp1_c::setAnm_NUM(int num, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2}, {1, 0, 8.0f, 1.0f, 2}, {2, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, 1.0f, 2}, {4, 0, 8.0f, 1.0f, 2}, {5, 0, 8.0f, 1.0f, 2},
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[num].mTex);
    }
    setAnm_anm(&a_anm_prm_tbl[num]);
}

void daNpc_Kp1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, -1}, {0, 0, 8.0f, 1.0f, 2}, {-1, -1, 0.0f, 0.0f, -1},
    };
    setAnm_tex(a_anm_prm_tbl[mState].mTex);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

void daNpc_Kp1_c::chngAnmTag() {
    switch (mAnmTag) {
    case 0:
        break;
    }
}

void daNpc_Kp1_c::ctrlAnmTag() {
    switch (mAnmAttr) {
    case 0:
        break;
    }
}

void daNpc_Kp1_c::chngAnmAtr(u8 attr) {
    if (attr < 7 && attr != mAnmAttr) {
        mLetterMode = 0;
        if (attr == 2) {
            mLetterMode = 2;
        } else if (attr == 3) {
            mLetterMode = 1;
        }
        mAnmAttr = attr;
        setAnm_ATR(1);
    }
}

void daNpc_Kp1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 5:
        break;
    case 1:
        if (mAnmEnd && ++mAnmLoops >= 2) {
            mAnmAttr = 0;
            setAnm_NUM(0, 1);
        }
        break;
    case 6:
        if (mAnmEnd && ++mAnmLoops >= 1) {
            mAnmAttr = 0;
            setAnm_NUM(0, 1);
        }
        break;
    }
}

void daNpc_Kp1_c::setAnm_ATR(int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2}, {1, 0, 8.0f, 1.0f, 2}, {2, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, 1.0f, 2}, {4, 0, 8.0f, 1.0f, 2}, {5, 0, 8.0f, 1.0f, 2}, {1, 1, 8.0f, 1.0f, 2},
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[mAnmAttr].mTex);
    }
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

void daNpc_Kp1_c::anmAtr(u16 status) {
    if (status == 6) {
        if (!mMsgAnmStarted) {
            mAnmTag = 0xFF;
            chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        u8 tag = dComIfGp_getMesgAnimeTagInfo();
        if (tag != 0xFF && tag != mAnmTag) {
            dComIfGp_clearMesgAnimeTagInfo();
            mAnmTag = tag;
            chngAnmTag();
        }
    } else if (status == 14) {
        mMsgAnmStarted = 0;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

void daNpc_Kp1_c::setStt(s8 state) {
    s8 previous = mState;
    mLetterMode = 0;
    mState = state;
    switch (mState) {
    case 2:
        mLookMode = 1;
        m_jnt.setTrn();
        mAnmAttr = 0xFF;
        mPreviousState = previous;
        mCancelPresent = 0;
        break;
    case 0:
    case 1:
    default: setAnm();
        break;
    }
}

u16 daNpc_Kp1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x1E84:
        *msg = 0x1E85;
        break;
    case 0x1E85:
        *msg = 0x1E86;
        break;
    case 0x1E88:
        *msg = 0x1E89;
        break;
    case 0x1E8A:
        *msg = 0x1E8B;
        break;
    case 0x1E8B:
        *msg = 0x1E8C;
        break;
    case 0x1E8C:
        *msg = 0x1E8D;
        break;
    case 0x1E8D:
        *msg = 0x1E8E;
        break;
    case 0x1E8E:
        *msg = 0x1E8F;
        break;
    case 0x1E8F:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *msg = 0x1E91;
            break;
        case 1:
            *msg = 0x1E90;
            break;
        }
        break;
    case 0x1E96:
        *msg = 0x1E97;
        mCancelPresent = 1;
        break;
    case 0x1E97:
        *msg = 0x1E98;
        break;
    case 0x1E98:
        *msg = 0x1E99;
        break;
    case 0x1E99:
        *msg = 0x1E9A;
        break;
    case 0x1E9A:
        *msg = 0x1E9B;
        break;
    case 0x1E9B:
        *msg = 0x1E9C;
        break;
    case 0x1E9C:
        *msg = 0x1E9D;
        break;
    case 0x1E91:
        mOrder = 3;
        goto end_message;
    case 0x1E9D:
        mOrder = 4;
        goto end_message;
    case 0x1E90: mRefused = 1;
    default:
        end_message:
        status = 16;
        break;
    }
    return status;
}

u32 daNpc_Kp1_c::getMsg() {
    if (mItemNo != 0xFF) {
        if (mItemNo == dItemNo_MOBLINS_LETTER_e) {
            return 0x1E96;
        }
        return mItemNo == dItemNo_SKULL_NECKLACE_e ? 0x1EA0 : 0x1E9F;
    }
    if (mGaveLetter) {
        mGaveLetter = 0;
        return 0x1E92;
    }
    if (mGaveHeart) {
        mGaveHeart = 0;
        return 0x1E9E;
    }
    if (dComIfGs_isGetItemReserve(15) && !dComIfGs_checkReserveItem(dItemNo_MOBLINS_LETTER_e)) {
        return dComIfGs_getEventReg(0xCCFF) >= 1 ? 0x1E84 : 0x1E83;
    }
    if (dComIfGs_isGetItemReserve(14)) {
        return dComIfGs_isGetItemBeast(0) ? 0x1E87 : 0x1E88;
    }
    return mRefused ? 0x1E93 : 0x1E8A;
}

void daNpc_Kp1_c::eventOrder() {
    static char* a_demo_name_tbl[] = {"GET_MAGYS_LTTR", "GET_KAKERA_HRT"};
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        eventInfo.onCondition(dEvtCnd_CANTALKITEM_e);
        if (mOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (mOrder >= 3) {
        fopAcM_orderOtherEvent2(this, a_demo_name_tbl[mOrder - 3], 1, 0xFFFF);
    }
}

void daNpc_Kp1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck("GET_MAGYS_LTTR") && mOrder == 3) {
            mOrder = 0;
        } else if (dComIfGp_evmng_startCheck("GET_KAKERA_HRT") && mOrder == 4) {
            mOrder = 0;
        }
    } else if (eventInfo.checkCommandTalk() && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0;
        mTalking = 1;
    }
}

void daNpc_Kp1_c::lookBack() {
    cXyz target;
    cXyz base = current.pos;
    base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 0:
        break;
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
    case 3:
        angle = mLookAngle;
        break;
    }
    if (m_jnt.trnChk()) {
        cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mPrm.mTurnSpeed, 4, 0x800);
    } else {
        mTurnSpeed = 0;
    }
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, noTurn);
}

bool daNpc_Kp1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

void daNpc_Kp1_c::setAttention() {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionYOffset, current.pos.z);
    if (mUpdateEye || mActionResult) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
        mUpdateEye = 0;
    }
}

bool daNpc_Kp1_c::chk_talk() {
    bool result = true;
    mItemNo = 0xFF;
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mItemNo = dComIfGp_event_getPreItemNo();
        } else {
            result = false;
        }
    }
    return result;
}

bool daNpc_Kp1_c::decideType(int) {
    mType = -1;
    switch (fopAcM_GetName(this)) {
    case fpcNm_NPC_KP1_e:
        mType = 0;
        mSubType = 0;
        break;
    }
    return true;
}

void daNpc_Kp1_c::event_actionInit(int staff) {
    int* act = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    if (act != NULL) {
        mEventAction = *act;
    }
}

bool daNpc_Kp1_c::event_action() {
    bool result = false;
    switch (mEventAction) {
    case 0: {
            fpc_ProcID id = fopAcM_createItemForPresentDemo(&current.pos, dItemNo_MAGGIES_LETTER_e, 0, -1, -1, NULL, NULL);
            if (id != fpcM_ERROR_PROCESS_ID_e) {
                dComIfGp_event_setItemPartnerId(id);
                result = true;
            }
            break;
        }
    case 1: {
            fpc_ProcID id = fopAcM_createItemForPresentDemo(&current.pos, dItemNo_HEART_PIECE_e, 0, -1, -1, NULL, NULL);
            if (id != fpcM_ERROR_PROCESS_ID_e) {
                dComIfGp_event_setItemPartnerId(id);
                result = true;
            }
            break;
        }
    default: result = true;
        break;
    }
    return result;
}

void daNpc_Kp1_c::privateCut() {
    static char* cut_name_tbl[] = {"ACTION"};
    int staff = dComIfGp_evmng_getMyStaffId("Kp1", NULL, 0);
    if (staff == -1) {
        return;
    }
    mCut = dComIfGp_evmng_getMyActIdx(staff, cut_name_tbl, 1, TRUE, FALSE);
    if (mCut == -1) {
        dComIfGp_evmng_cutEnd(staff);
        return;
    }
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch (mCut) {
        case 0:
            event_actionInit(staff);
            break;
        }
    }
    bool done;
    switch (mCut) {
    case 0:
        done = event_action();
        break;
    default: done = true;
        break;
    }
    if (done) {
        dComIfGp_evmng_cutEnd(staff);
    }
}

void daNpc_Kp1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

void daNpc_Kp1_c::event_proc() {
    if (dComIfGp_evmng_endCheck("GET_MAGYS_LTTR")) {
        endEvent();
        mOrder = 1;
        mGaveLetter = 1;
    } else if (dComIfGp_evmng_endCheck("GET_KAKERA_HRT")) {
        dComIfGs_setEventReg(0xCCFF, 0);
        endEvent();
        mOrder = 1;
        mGaveHeart = 1;
    } else {
        if (!mEventCut.cutProc()) {
            privateCut();
        }
        lookBack();
    }
    shape_angle = current.angle;
}

bool daNpc_Kp1_c::set_action(ActionFunc action, void* arg) {
    if (mAction != action) {
        if (mAction) {
            mActionState = -1;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

int daNpc_Kp1_c::wait01() {
    if (mOrder == 3 || mOrder == 4 || mOrder == 1) {
        return 1;
    }
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
        }
    } else {
        mOrder = 2;
        if (mAttention) {
            mLookMode = 1;
            m_jnt.setTrn();
        } else {
            mLookMode = 3;
            mLookAngle = mInitialAngle.y;
            m_jnt.setTrn();
        }
    }
    return 1;
}

int daNpc_Kp1_c::talk01() {
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case 19:
            switch (mCurrMsgNo) {
            case 0x1E9D:
                dComIfGs_setReserveItemEmpty();
                break;
            }
            mItemNo = 0xFF;
            setStt(mPreviousState);
            m7D0 = 0;
            mTalking = 0;
            endEvent();
            break;
        case 2:
        case 6:
            if (mCancelPresent) {
                dComIfGp_evmng_CancelPresent();
                mCancelPresent = 0;
            }
            break;
        }
    }
    return 1;
}

int daNpc_Kp1_c::wait_action1(void*) {
    if (mActionState == 0) {
        setStt(1);
        mActionState++;
    } else if (mActionState != -1) {
        mAttention = chkAttention();
        switch (mState) {
        case 2:
            mActionResult = talk01();
            break;
        case 1:
            mActionResult = wait01();
            break;
        default: mActionResult = 0;
            break;
        }
        lookBack();
    }
    return 1;
}

u8 daNpc_Kp1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) {
            mDemo = 0;
        }
    } else {
        mDemo = 1;
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        s16 end = m_head_tex_pattern->getFrameMax();
        if (++mBtpFrame >= end) {
            mBtpFrame = m_head_tex_pattern->getFrameMax();
        }
        J3DAnmTexPattern* btp = actor->getP_BtpData("Kp");
        if (btp != NULL) {
            m_head_tex_pattern = btp;
            J3DModelData* data = mpMorf->getModel()->getModelData();
            if (mBtp.init(data, m_head_tex_pattern, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, true, 0)) {
                mBtpFrame = 0;
            }
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Kp");
    }
    return mDemo;
}

void daNpc_Kp1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadow = dComIfGd_setShadow(mShadow, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y,
    mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadow != 0) {
        switch (mLetterMode) {
        case 1:
            dComIfGd_addRealShadow(mShadow, mpLetterModel);
            break;
        case 2:
            dComIfGd_addRealShadow(mShadow, mpOpenLetterModel);
            break;
        }
    }
}

BOOL daNpc_Kp1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor red = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue = {0x00, 0x00, 0xFF, 0x80};
    J3DModel* letter;
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtp.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtp.remove(data);
    switch (mLetterMode) {
    case 1:
        letter = mpLetterModel;
        break;
    case 2:
        letter = mpOpenLetterModel;
        break;
    default: letter = NULL;
        break;
    }
    if (letter != NULL) {
        g_env_light.setLightTevColorType(letter, &tevStr);
        mDoExt_modelEntryDL(letter);
    }
    shadowDraw();
    dSnap_RegistFig(0x5A, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

BOOL daNpc_Kp1_c::_execute() {
    if (!mSavedInitial) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mSavedInitial = 1;
    }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackX, l_HIO.mPrm.mMaxBackY, l_HIO.mPrm.mMinBackX, l_HIO.mPrm.mMinBackY,
    l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY, l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurn);
    checkOrder();
    if (!demo()) {
        if (dComIfGp_event_runCheck() && !eventInfo.checkCommandTalk()) {
            event_proc();
        } else {
            (this->*mAction)(NULL);
            shape_angle = current.angle;
        }
    }
    eventOrder();
    if (!mDemo) {
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
    }
    setMtx();
    setCollision(60.0f, 170.0f);
    return TRUE;
}

BOOL daNpc_Kp1_c::_delete() {
    dComIfG_resDelete(&mPhase, "Kp");
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    l_HIO.removeHIO();
#else
    if (heap != NULL && mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return TRUE;
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Kp1_c*)actor)->CreateHeap();
}

cPhs_State daNpc_Kp1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Kp1_c);
#endif
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Kp");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    int type = fopAcM_GetParam(this) & 0xFF;
    if (!decideType(type)) {
        return cPhs_ERROR_e;
    }
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("金持ちマギ−");
    fopAcM_SetupActor(this, daNpc_Kp1_c);
#endif
    static u32 a_heap_size_tbl[] = {0x272E0};
    if (fopAcM_entrySolidHeap(this, CheckCreateHeap, a_heap_size_tbl[mType])) {
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 170.0f, 50.0f);
    } else return cPhs_ERROR_e;
    if (!createInit()) {
        phase = cPhs_ERROR_e;
    }
    return phase;
}

int daNpc_Kp1_c::CreateHeap() {
    J3DModelData* a_mdl_data = (J3DModelData*)dComIfG_getObjectIDRes("Kp", dRes_ID_KP_BDL_KP_e);
    JUT_ASSERT(DEMO_SELECT(0x627, 0x62C), a_mdl_data != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_data, NULL, NULL,
    (J3DAnmTransform*)dComIfG_getObjectIDRes("Kp", dRes_ID_KP_BCK_WAIT01_e),
    J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020002);
    if (mpMorf != NULL) {
        if (mpMorf->getModel() != NULL) {
            m_head_jnt_num = a_mdl_data->getJointName()->getIndex("head");
            JUT_ASSERT(DEMO_SELECT(0x63A, 0x63F), m_head_jnt_num >= 0);
            m_backbone_jnt_num = a_mdl_data->getJointName()->getIndex("backbone");
            JUT_ASSERT(DEMO_SELECT(0x63C, 0x641), m_backbone_jnt_num >= 0);
            m_handL_jnt_num = a_mdl_data->getJointName()->getIndex("handL");
            JUT_ASSERT(DEMO_SELECT(0x63E, 0x643), m_handL_jnt_num >= 0);
            static s8 a_tex_pattern_num_tbl[] = {0};
            mTexNo = a_tex_pattern_num_tbl[mType];
            if (initTexPatternAnm(false)) {
                J3DModelData* a_itm_mdl_data = (J3DModelData*)dComIfG_getObjectIDRes("Kp", dRes_ID_KP_BDL_LT_01_e);
                JUT_ASSERT(DEMO_SELECT(0x650, 0x655), a_itm_mdl_data != 0);
                mpLetterModel = mDoExt_J3DModel__create(a_itm_mdl_data, 0x80000, 0x11000002);
                if (mpLetterModel != NULL) {
                    a_itm_mdl_data = (J3DModelData*)dComIfG_getObjectIDRes("Kp", dRes_ID_KP_BDL_LT_02_e);
                    JUT_ASSERT(DEMO_SELECT(0x65A, 0x65F), a_itm_mdl_data != 0);
                    mpOpenLetterModel = mDoExt_J3DModel__create(a_itm_mdl_data, 0x80000, 0x11000002);
                    if (mpOpenLetterModel != NULL) {
                        for (u16 i = 0; i < a_mdl_data->getJointNum(); i++) {
                            if (i == m_head_jnt_num || i == m_backbone_jnt_num) {
                                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Kp);
                            }
                        }
                        mpMorf->getModel()->setUserArea((u32)this);
                        mAcchCir.SetWall(30.0f, 60.0f);
                        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
                        return TRUE;
                    }
                }
            }
        }
        mpMorf = NULL;
    }
    return FALSE;
}

/* 00002B14-00002B34       .text daNpc_Kp1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Kp1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Kp1_c*)i_this)->_create();
}

/* 00002B34-00002B54       .text daNpc_Kp1_Delete__FP11daNpc_Kp1_c */
static BOOL daNpc_Kp1_Delete(daNpc_Kp1_c* i_this) {
    return ((daNpc_Kp1_c*)i_this)->_delete();
}

/* 00002B54-00002B74       .text daNpc_Kp1_Execute__FP11daNpc_Kp1_c */
static BOOL daNpc_Kp1_Execute(daNpc_Kp1_c* i_this) {
    return ((daNpc_Kp1_c*)i_this)->_execute();
}

/* 00002B74-00002B94       .text daNpc_Kp1_Draw__FP11daNpc_Kp1_c */
static BOOL daNpc_Kp1_Draw(daNpc_Kp1_c* i_this) {
    return ((daNpc_Kp1_c*)i_this)->_draw();
}

/* 00002B94-00002B9C       .text daNpc_Kp1_IsDelete__FP11daNpc_Kp1_c */
static BOOL daNpc_Kp1_IsDelete(daNpc_Kp1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Kp1_Method = {
    (process_method_func)daNpc_Kp1_Create,
    (process_method_func)daNpc_Kp1_Delete,
    (process_method_func)daNpc_Kp1_Execute,
    (process_method_func)daNpc_Kp1_IsDelete,
    (process_method_func)daNpc_Kp1_Draw,
};
actor_process_profile_definition g_profile_NPC_KP1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_KP1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Kp1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_KP1_e,
    /* Actor SubMtd */ &l_daNpc_Kp1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
daNpc_Kp1_HIO_c::~daNpc_Kp1_HIO_c() {}
