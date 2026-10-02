/**
* d_a_npc_pm1.cpp
* NPC - Maggie (poor)
*/
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_pm1.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "d/d_demo.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Pm.h"

class daNpc_Pm1_HIO_c : public JORReflexible {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY;
        s16 mMinHeadX, mMinHeadY;
        s16 mMaxBackboneX, mMaxBackboneY;
        s16 mMinBackboneX, mMinBackboneY;
        s16 mMaxTurnStep, mTurnSpeed;
        f32 mAttentionYOffset, mUnused;
    };
    daNpc_Pm1_HIO_c();
    virtual ~daNpc_Pm1_HIO_c() {}
    void genMessage(JORMContext*) {}

public:
    s8 mNo;
    int mCount;
    hio_prm_c mPrm;
};

static daNpc_Pm1_HIO_c l_HIO;

/* 000000EC-00000144       .text __ct__15daNpc_Pm1_HIO_cFv */
daNpc_Pm1_HIO_c::daNpc_Pm1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x1FFE, 0x38E0, (s16)0xE002, (s16)0xC720,
        0, 0, 0, 0, 0x800, 0x800, 150.0f, 0.0f,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    mNo = -1;
    mCount = -1;
}

/* 00000144-00000300       .text nodeCallBack_Pm__FP7J3DNodei */
static BOOL nodeCallBack_Pm(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Pm1_c* npc = (daNpc_Pm1_c*)model->getUserArea();
        if (npc != NULL) {
            static cXyz a_att_pos_offst(0.0f, 0.0f, 0.0f);
            static cXyz a_eye_pos_offst(20.0f, -25.0f, 0.0f);
            int joint = ((J3DJoint*)node)->getJntNo();
            mDoMtx_stack_c::copy(model->getAnmMtx(joint));
            if (joint == npc->getHeadJntNum()) {
                mDoMtx_stack_c::multVec(&a_att_pos_offst, npc->getAttPos());
                mDoMtx_stack_c::YrotM(-npc->getHead_y());
                mDoMtx_stack_c::ZrotM(-npc->getHead_x());
                mDoMtx_stack_c::multVec(&a_eye_pos_offst, npc->getEyePos());
            } else if (joint == npc->getBackboneJntNum()) {
                mDoMtx_stack_c::XrotM(npc->getBackbone_y());
                mDoMtx_stack_c::ZrotM(npc->getBackbone_x());
            }
            cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
            model->setAnmMtx(joint, mDoMtx_stack_c::get());
        }
    }
    return TRUE;
}

/* 0000033C-0000044C       .text createInit__11daNpc_Pm1_cFv */
bool daNpc_Pm1_c::createInit() {
    mCut.setActorInfo2("Pm1", this);
    attention_info.flags = fopAc_Attn_LOCKON_TALK_e | fopAc_Attn_ACTION_SPEAK_e;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAB;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;
    gravity = -4.0f;
    mLookPos = current.pos;
    set_action(&daNpc_Pm1_c::wait_action1, NULL);
    shape_angle = current.angle;
    mStts.Init(0xFF, 0xFF, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    mFirstFrame = 1;
    setMtx();
    return true;
}

/* 0000044C-0000055C       .text setMtx__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::setMtx() {
    if (!mDemo) {
        playTexPatternAnm();
        mAnmEnded = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mFrame) {
            mAnmEnded = 1;
        }
        mFrame = mpMorf->getFrame();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    setAttention();
}

/* 0000055C-00000664       .text anmResID__11daNpc_Pm1_cFiPiPi */
BOOL daNpc_Pm1_c::anmResID(int i_num, int* o_bck_num, int* o_bas_num) {
    enum {
        ANM_END = 2
    };
    static const int a_anm_idx_tbl[2][2] = {
        {dRes_ID_PM_BCK_WAIT01_e, dRes_ID_PM_BAS_WAIT01_e},
        {dRes_ID_PM_BCK_TALK01_e, dRes_ID_PM_BAS_TALK01_e},
    };
    JUT_ASSERT(283, 0 <= i_num && i_num < ANM_END);
    JUT_ASSERT(284, o_bck_num && o_bas_num);
    *o_bck_num = a_anm_idx_tbl[i_num][0];
    *o_bas_num = a_anm_idx_tbl[i_num][1];
    return TRUE;
}

/* 00000664-00000700       .text BtpNum2ResID__11daNpc_Pm1_cFiPi */
void daNpc_Pm1_c::BtpNum2ResID(int i_num, int* result) {
    enum {
        TEXPATTERN_END = 1
    };
    static const int a_btp_arc_ix_tbl[1] = {dRes_ID_PM_BTP_MABA01_e};
    JUT_ASSERT(305, 0 <= i_num && i_num < TEXPATTERN_END);
    *result = a_btp_arc_ix_tbl[i_num];
}

/* 00000700-00000744       .text setAnm_tex__11daNpc_Pm1_cFSc */
void daNpc_Pm1_c::setAnm_tex(signed char index) {
    if (index >= 0 && index != mTexIndex) {
        mTexIndex = index;
        initTexPatternAnm(true);
    }
}

/* 00000744-00000860       .text init_btp__11daNpc_Pm1_cFbi */
bool daNpc_Pm1_c::init_btp(bool modify, int index) {
    J3DModelData* modelData = mpMorf->getModel()->getModelData();
    if (index >= 0) {
        int res;
        BtpNum2ResID(index, &res);
        m_head_tex_pattern = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Pm", res);
        JUT_ASSERT(338, m_head_tex_pattern != 0);
        if (!mBtp.init(modelData, m_head_tex_pattern, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, modify, FALSE)) {
            return false;
        }
        mBtpFrame = 0;
        mBlinkTimer = 0;
    }
    return true;
}

/* 00000860-000008A4       .text initTexPatternAnm__11daNpc_Pm1_cFb */
bool daNpc_Pm1_c::initTexPatternAnm(bool modify) {
    bool result = false;
    if (init_btp(modify, mTexIndex)) {
        result = true;
    }
    return result;
}

/* 000008A4-00000968       .text playTexPatternAnm__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::playTexPatternAnm() {
    bool advance = true;
    if (mTexIndex == 0) {
        advance = !cLib_calcTimer(&mBlinkTimer);
    }
    if (advance) {
        s16 end = m_head_tex_pattern->getFrameMax();
        if (++mBtpFrame >= end) {
            if (mTexIndex != 0) {
                mBtpFrame = m_head_tex_pattern->getFrameMax();
            } else {
                mBtpFrame = 0;
                mBlinkTimer = 30.0f + cM_rndF(60.0f);
            }
        }
    }
}

/* 00000968-00000A34       .text setAnm_anm__11daNpc_Pm1_cFPQ211daNpc_Pm1_c9anm_prm_c */
BOOL daNpc_Pm1_c::setAnm_anm(daNpc_Pm1_c::anm_prm_c* prm) {
    BOOL result = FALSE;
    if (prm->mAnm >= 0 && mAnmIndex != prm->mAnm) {
        mAnmIndex = prm->mAnm;
        if (mpMorf != NULL) {
            int bck, bas;
            anmResID(mAnmIndex, &bck, &bas);
            if (bck >= 0) {
                dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bck, bas, "Pm");
            }
            result = TRUE;
        }
        mFrame = 0.0f;
        m7B5 = 0;
        mAnmEnded = 0;
    }
    return result;
}

/* 00000A34-00000A9C       .text setAnm__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, -1},
        {0, 0, 8.0f, 1.0f, 2},
        {-1, -1, 0.0f, 0.0f, -1},
    };
    setAnm_tex(a_anm_prm_tbl[mState].mTex);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

/* 00000A9C-00000AA8       .text chngAnmTag__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::chngAnmTag() {
    switch (mAnmTag) {
    case 0:
        break;
    }
}

/* 00000AA8-00000AB4       .text ctrlAnmTag__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::ctrlAnmTag() {
    switch (mAnmAttr) {
    case 0:
        break;
    }
}

/* 00000AB4-00000AF4       .text chngAnmAtr__11daNpc_Pm1_cFUc */
void daNpc_Pm1_c::chngAnmAtr(unsigned char attr) {
    if (attr < 1 && attr != mAnmAttr) {
        mAnmAttr = attr;
        setAnm_ATR(1);
    }
}

/* 00000AF4-00000AF8       .text ctrlAnmAtr__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::ctrlAnmAtr() {}

/* 00000AF8-00000B60       .text setAnm_ATR__11daNpc_Pm1_cFi */
void daNpc_Pm1_c::setAnm_ATR(int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {{0, 0, 8.0f, 1.0f, 2}};
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[mAnmAttr].mTex);
    }
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00000B60-00000C1C       .text anmAtr__11daNpc_Pm1_cFUs */
void daNpc_Pm1_c::anmAtr(u16 status) {
    if (status == fopMsgStts_MSG_TYPING_e) {
        if (mMessageState == 0) {
            mAnmTag = 0xFF;
            chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMessageState++;
        }
        {
            u8 tag = dComIfGp_getMesgAnimeTagInfo();
            if (tag != 0xFF && tag != mAnmTag) {
                dComIfGp_clearMesgAnimeTagInfo();
                mAnmTag = tag;
                chngAnmTag();
            }
        }
    } else if (status == fopMsgStts_MSG_DISPLAYED_e) {
        mMessageState = 0;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

/* 00000C1C-00000C74       .text setStt__11daNpc_Pm1_cFSc */
void daNpc_Pm1_c::setStt(signed char state) {
    u8 previous = mState;
    mState = state;
    switch (mState) {
    case 2:
        mLookMode = 1;
        mAnmAttr = 0xFF;
        mPreviousState = previous;
        break;
    case 0:
    case 1:
    default:
        setAnm();
        break;
    }
}

/* 00000C74-00000C7C       .text next_msgStatus__11daNpc_Pm1_cFPUl */
u16 daNpc_Pm1_c::next_msgStatus(u32*) {
    return fopMsgStts_MSG_ENDS_e;
}

/* 00000C7C-00000C84       .text getMsg__11daNpc_Pm1_cFv */
u32 daNpc_Pm1_c::getMsg() {
    return 0;
}

/* 00000C84-00000CD4       .text eventOrder__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::eventOrder() {
    if (mEventOrder == 1 || mEventOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mEventOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 00000CD4-00000D14       .text checkOrder__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        return;
    }
    if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mEventOrder == 1 || mEventOrder == 2)) {
        mEventOrder = 0;
        mTalking = true;
    }
}

/* 00000D14-00000EA4       .text lookBack__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::lookBack() {
    cXyz target;
    cXyz src = current.pos;
    src.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool flag = mLookFlag;
    switch (mLookMode) {
    case 0:
        break;
    case 1:
        target = dNpc_playerEyePos(-20.0f);
        targetP = &target;
        src = current.pos;
        src.y = eyePos.y;
        break;
    case 2:
        target = mLookPos;
        targetP = &target;
        src = current.pos;
        src.y = eyePos.y;
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
    m_jnt.lookAtTarget(&current.angle.y, targetP, src, angle, mTurnSpeed, flag);
}

/* 00000EA4-00000F24       .text chkAttention__11daNpc_Pm1_cFv */
bool daNpc_Pm1_c::chkAttention() {
    dAttention_c* attention = &dComIfGp_getAttention();
    if (attention->LockonTruth()) {
        return this == attention->LockonTarget(0);
    }
    return this == attention->ActionTarget(0);
}

/* 00000F24-00000F88       .text setAttention__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::setAttention() {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionYOffset, current.pos.z);
    if (mFirstFrame || mUpdateEye) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
        mFirstFrame = 0;
    }
}

/* 00000F88-00000FB4       .text decideType__11daNpc_Pm1_cFi */
bool daNpc_Pm1_c::decideType(int) {
    mType = -1;
    switch (fopAcM_GetName(this)) {
    case fpcNm_NPC_PM1_e:
        mType = 0;
        m7D4 = 0;
        break;
    }
    return true;
}

/* 00000FB4-0000103C       .text event_actionInit__11daNpc_Pm1_cFi */
void daNpc_Pm1_c::event_actionInit(int staff) {
    int* action = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    if (action) {
        mEventAction = *action;
    }
}

/* 0000103C-0000104C       .text event_action__11daNpc_Pm1_cFv */
bool daNpc_Pm1_c::event_action() {
    switch (mEventAction) {
    case 0:
        break;
    }
    return true;
}

/* 0000104C-00001154       .text privateCut__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::privateCut() {
    static char* cut_name_tbl[] = {"ACTION"};
    int staff = dComIfGp_evmng_getMyStaffId("Pm1");
    if (staff != -1) {
        mCutIndex = dComIfGp_evmng_getMyActIdx(staff, cut_name_tbl, 1, TRUE, FALSE);
        if ((s8)mCutIndex == -1) {
            dComIfGp_evmng_cutEnd(staff);
            return;
        }
        if (dComIfGp_evmng_getIsAddvance(staff)) {
            switch (mCutIndex) {
            case 0:
                event_actionInit(staff);
                break;
            }
        }
        bool done;
        switch (mCutIndex) {
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
}

/* 00001154-00001174       .text endEvent__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

/* 00001174-000011D4       .text event_proc__11daNpc_Pm1_cFv */
void daNpc_Pm1_c::event_proc() {
    if (!mCut.cutProc()) {
        privateCut();
    }
    lookBack();
    shape_angle = current.angle;
}

/* 000011D4-00001280       .text set_action__11daNpc_Pm1_cFM11daNpc_Pm1_cFPCvPvPv_iPv */
bool daNpc_Pm1_c::set_action(int (daNpc_Pm1_c::*action)(void*), void* arg) {
    if (mAction != action) {
        if (mAction != NULL) {
            mActionState = -1;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

/* 00001280-00001378       .text wait01__11daNpc_Pm1_cFv */
BOOL daNpc_Pm1_c::wait01() {
    if (mTalking) {
        bool ready = true;
        mPresentItem = 0xFF;
        if (dComIfGp_event_chkTalkXY()) {
            if (dComIfGp_evmng_ChkPresentEnd()) {
                mPresentItem = dComIfGp_event_getPreItemNo();
            } else {
                ready = false;
            }
        }
        if (ready) {
            setStt(2);
        }
    } else {
        mEventOrder = 2;
        if (mHasAttention) {
            mLookMode = 1;
        } else {
            mLookMode = 3;
            mLookAngle = mInitialAngle.y;
            m_jnt.setTrn();
        }
    }
    return TRUE;
}

/* 00001378-00001408       .text talk01__11daNpc_Pm1_cFv */
BOOL daNpc_Pm1_c::talk01() {
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case fopMsgStts_BOX_OPENING_e:
        case fopMsgStts_MSG_TYPING_e:
            break;
        case fopMsgStts_MSG_DESTROYED_e:
            mPresentItem = 0xFF;
            setStt(mPreviousState);
            m7C8 = 0;
            mTalking = false;
            endEvent();
            break;
        }
    }
    return TRUE;
}

/* 00001408-000014BC       .text wait_action1__11daNpc_Pm1_cFPv */
int daNpc_Pm1_c::wait_action1(void*) {
    if (mActionState == 0) {
        setStt(1);
        mActionState++;
    } else if (mActionState != -1) {
        mHasAttention = chkAttention();
        switch (mState) {
        case 2:
            mUpdateEye = talk01();
            break;
        case 1:
            mUpdateEye = wait01();
            break;
            default: mUpdateEye = 0;
            break;
        }
        lookBack();
    }
    return 1;
}

/* 000014BC-00001558       .text demo__11daNpc_Pm1_cFv */
u8 daNpc_Pm1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) {
            mDemo = 0;
        }
    } else {
        mDemo = 1;
        dComIfGp_demo_getActor(demoActorID);
        dDemo_setDemoData(this, 0x6A, mpMorf, "Pm");
    }
    return mDemo;
}
/* 00001558-000016BC       .text _draw__11daNpc_Pm1_cFv */
BOOL daNpc_Pm1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor red = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue = {0x00, 0x00, 0xFF, 0x80};
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtp.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtp.remove(data);
    dNpc_setShadowModel(mpShadowModel, data, model);
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setRealShadow(mShadowId, 1, mpShadowModel, &pos, 800.0f, current.pos.y - mObjAcch.GetGroundH(), NULL);
    if (!mShadowId) {
        dComIfGd_setSimpleShadow(&current.pos, mObjAcch.GetGroundH(), 40.0f, &dComIfG_Bgsp()->GetTriPla(mObjAcch.m_gnd)->mNormal, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }
    dSnap_RegistFig(DSNAP_TYPE_UNK5A, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 000016BC-00001818       .text _execute__11daNpc_Pm1_cFv */
BOOL daNpc_Pm1_c::_execute() {
    if (!mInitialPosSet) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mInitialPosSet = true;
    }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackboneX, l_HIO.mPrm.mMaxBackboneY, l_HIO.mPrm.mMinBackboneX, l_HIO.mPrm.mMinBackboneY,
    l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY, l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurnStep);
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
    setCollision(50.0f, 130.0f);
    return TRUE;
}

/* 00001818-0000189C       .text _delete__11daNpc_Pm1_cFv */
BOOL daNpc_Pm1_c::_delete() {
    dComIfG_resDeleteDemo(&mPhase, "Pm");
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    if (l_HIO.mCount >= 0 && --l_HIO.mCount < 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
    }
    return TRUE;
}

/* 0000189C-000018BC       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Pm1_c*)actor)->CreateHeap();
}

/* 000018BC-00001A2C       .text _create__11daNpc_Pm1_cFv */
cPhs_State daNpc_Pm1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Pm1_c);
#endif
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) {
        return cPhs_ERROR_e;
    }
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Pm");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    if (l_HIO.mCount < 0) {
        l_HIO.mNo = mDoHIO_createChild("貧乏マギ−", &l_HIO);
    }
    l_HIO.mCount++;
#if VERSION == VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Pm1_c);
#endif
    static u32 a_heap_size_tbl[] = {0x272E0};
    if (fopAcM_entrySolidHeap(this, CheckCreateHeap, a_heap_size_tbl[mType])) {
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 150.0f, 50.0f);
    } else {
        return cPhs_ERROR_e;
    }
    if (!createInit()) {
        phase = cPhs_ERROR_e;
    }
    return phase;
}

/* 00001E5C-00002168       .text CreateHeap__11daNpc_Pm1_cFv */
BOOL daNpc_Pm1_c::CreateHeap() {
    J3DModelData* a_mdl_data = (J3DModelData*)dComIfG_getObjectIDRes("Pm", dRes_ID_PM_BDL_PM_e);
    JUT_ASSERT(DEMO_SELECT(1318, 1319), a_mdl_data != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_data, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectIDRes("Pm", dRes_ID_PM_BCK_WAIT01_e), 2, 1.0f, 0, -1, TRUE, NULL, 0x80000, 0x11020002);
    if (mpMorf != NULL) {
        if (mpMorf->getModel() != NULL) {
            m_head_jnt_num = a_mdl_data->getJointName()->getIndex("head");
            JUT_ASSERT(DEMO_SELECT(1337, 1338), m_head_jnt_num >= 0);
            m_backbone_jnt_num = a_mdl_data->getJointName()->getIndex("backbone");
            JUT_ASSERT(DEMO_SELECT(1339, 1340), m_backbone_jnt_num >= 0);
            static s8 a_tex_pattern_num_tbl[] = {0};
            mTexIndex = a_tex_pattern_num_tbl[mType];
            if (initTexPatternAnm(false)) {
                mpShadowModel = mDoExt_J3DModel__create(a_mdl_data, 0x20000, 0x11020203);
                if (mpShadowModel != NULL) {
                    for (u16 i = 0; i < a_mdl_data->getJointNum(); i++) {
                        if (i == m_head_jnt_num || i == m_backbone_jnt_num) {
                            mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Pm);
                        }
                    }
                    mpMorf->getModel()->setUserArea((u32)this);
                    mAcchCir.SetWall(30.0f, 50.0f);
                    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
                    return TRUE;
                }
            }
        }
        mpMorf = NULL;
    }
    return FALSE;
}

/* 00002168-00002188       .text daNpc_Pm1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Pm1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Pm1_c*)i_this)->_create();
}

/* 00002188-000021A8       .text daNpc_Pm1_Delete__FP11daNpc_Pm1_c */
static BOOL daNpc_Pm1_Delete(daNpc_Pm1_c* i_this) {
    return ((daNpc_Pm1_c*)i_this)->_delete();
}

/* 000021A8-000021C8       .text daNpc_Pm1_Execute__FP11daNpc_Pm1_c */
static BOOL daNpc_Pm1_Execute(daNpc_Pm1_c* i_this) {
    return ((daNpc_Pm1_c*)i_this)->_execute();
}

/* 000021C8-000021E8       .text daNpc_Pm1_Draw__FP11daNpc_Pm1_c */
static BOOL daNpc_Pm1_Draw(daNpc_Pm1_c* i_this) {
    return ((daNpc_Pm1_c*)i_this)->_draw();
}

/* 000021E8-000021F0       .text daNpc_Pm1_IsDelete__FP11daNpc_Pm1_c */
static BOOL daNpc_Pm1_IsDelete(daNpc_Pm1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Pm1_Method = {
    (process_method_func)daNpc_Pm1_Create,
    (process_method_func)daNpc_Pm1_Delete,
    (process_method_func)daNpc_Pm1_Execute,
    (process_method_func)daNpc_Pm1_IsDelete,
    (process_method_func)daNpc_Pm1_Draw,
};
actor_process_profile_definition g_profile_NPC_PM1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_PM1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Pm1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_PM1_e,
    /* Actor SubMtd */ &l_daNpc_Pm1_Method,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
