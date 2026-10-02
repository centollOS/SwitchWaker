/**
 * d_a_npc_aj1.cpp
 * NPC - Sturgeon
 */
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_aj1.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Aj.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include <string.h>
#include <math.h>

STATIC_ASSERT(sizeof(daNpc_Aj1_c) == 0x7C4);
static daNpc_Aj1_HIO_c l_HIO;

daNpc_Aj1_HIO_c::daNpc_Aj1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x2000, 8000, 500, -13000, 0, 7280, -1500, -150,
        1024, 1024, 150.0f, false, 1500.0f, 21000, 550.0f, 25000, 30, 20,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    m04 = -1;
    m08 = -1;
}
daNpc_Aj1_HIO_c::~daNpc_Aj1_HIO_c() {}
static char* l_evn_tbl[] = {"angry"};
static BOOL nodeCB_Head(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0)
        ((daNpc_Aj1_c*)j3dSys.getModel()->getUserArea())->_nodeCB_Head(node, j3dSys.getModel());
    return TRUE;
}
void daNpc_Aj1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(24.0f, -16.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mHeadPos.x=mDoMtx_stack_c::get()[0][3];
    mHeadPos.y=mDoMtx_stack_c::get()[1][3];
    mHeadPos.z=mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::YrotM(-m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyePos);
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}
static BOOL nodeCB_BackBone(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0)
        ((daNpc_Aj1_c*)j3dSys.getModel()->getUserArea())->_nodeCB_BackBone(node, j3dSys.getModel());
    return TRUE;
}
void daNpc_Aj1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(m_jnt.getBackbone_x());
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}
static BOOL CheckCreateHeap(fopAc_ac_c* actor) { return ((daNpc_Aj1_c*)actor)->CreateHeap(); }
static s16 daNpc_Aj1_XyCheck_CB(void* actor, int button) { return ((daNpc_Aj1_c*)actor)->_XyCheckCB(button); }
s16 daNpc_Aj1_c::_XyCheckCB(int button) { return dComIfGp_getSelectItem(button) == 0x48; }
bool daNpc_Aj1_c::init_AJ1_0() {
    if (!dComIfGs_isEventBit(0xE20) && !dComIfGs_isEventBit(0x502)) {
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 26;
        set_action(&daNpc_Aj1_c::wait_action1, NULL);
        return true;
    }
    if (mSwitch != 0xFF && !dComIfGs_isSwitch(mSwitch, fopAcM_GetRoomNo(this)))
        dComIfGs_onSwitch(mSwitch, fopAcM_GetRoomNo(this));
    return false;
}
bool daNpc_Aj1_c::init_AJ1_1() {
    if (!dComIfGs_isEventBit(0x520)) {
        dComIfGs_onEventBit(0x502);
        set_action(&daNpc_Aj1_c::wait_action2, NULL);
        fopAcM_OffStatus(this, 0x80);
#if VERSION > VERSION_DEMO
        fopAcM_OnStatus(this, 0x4000);
#endif
        return true;
    }
    return false;
}
bool daNpc_Aj1_c::init_AJ1_2() {
    if (dComIfGs_isEventBit(0x520)) {
        eventInfo.setXyCheckCB(daNpc_Aj1_XyCheck_CB);
        set_action(&daNpc_Aj1_c::wait_action2, NULL);
        return true;
    }
    return false;
}
bool daNpc_Aj1_c::createInit() {
    mSwitch = (fopAcM_GetParam(this) >> 8) & 0xFF;
    for (int i = 0; i < 1; i++) mEventIdx[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    mEventCut.setActorInfo2("Aj1", this);
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 173;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 173;
    mAnmNo = 9;
    bool result;
    switch (mSubType) {
    case 0: result = init_AJ1_0(); break;
    case 1: result = init_AJ1_1(); break;
    case 2: result = init_AJ1_2(); break;
    default: result = false; break;
    }
    if (!result) return false;
    mModelAngle = current.angle;
    shape_angle = mModelAngle;
    gravity = -4.5f;
    mStts.Init(255, 255, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    play_animation();
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}
void daNpc_Aj1_c::play_animation() {
    u32 surface = 0;
    play_texPttrnAnm();
    if (mObjAcch.ChkGroundHit()) surface = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    mAnmEnd = mpMorf->play(&eyePos, surface, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    if (mpMorf->getFrame() < mPrevFrame) mAnmEnd = 1;
    mPrevFrame = mpMorf->getFrame();
}
void daNpc_Aj1_c::ctrl_WAITanm() {
    switch (mAnmNo) {
    case 0: if (cLib_calcTimer(&mWaitAnmTimer) == 0 && mAnmEnd && chk_parts_notMov()) setAnm_NUM(1, 1); break;
    case 1: if (mAnmEnd) { setAnm_NUM(0, 1); mWaitAnmTimer = cLib_getRndValue<int>(90, 180); } break;
    }
}
void daNpc_Aj1_c::ctrl_TIREanm() {
    if (mAnmNo == 6) {
        if (mAnmEnd && cLib_calcTimer(&mTireCycles) == 0) { setAnm_NUM(0, 1); mpMorf->setMorf(20.0f); }
    } else mTireCycles = 0;
}
void daNpc_Aj1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(mModelAngle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    if (mpItemModel) { mpItemModel->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(m_hnd_L_jnt_num)); mpItemModel->calc(); }
    setAttention(force);
}
int daNpc_Aj1_c::bckResID(int n) {
    static const int a_res_id_tbl[] = {dRes_ID_AJ_BCK_WAIT01_e, dRes_ID_AJ_BCK_WAIT02_e, dRes_ID_AJ_BCK_ANGRY_e, dRes_ID_AJ_BCK_JAMP_e, dRes_ID_AJ_BCK_SHOCK_e, dRes_ID_AJ_BCK_LOOK_e, dRes_ID_AJ_BCK_TIRE_e, dRes_ID_AJ_BCK_CALL_e, dRes_ID_AJ_BCK_NOBI_e};
    return a_res_id_tbl[n];
}
int daNpc_Aj1_c::btpResID(int n) { static const int a_res_id_tbl[] = {dRes_ID_AJ_BTP_MABA_e}; return a_res_id_tbl[n]; }
bool daNpc_Aj1_c::init_texPttrnAnm(s8 number, bool modify) {
    J3DModel* model = mpMorf->getModel();
    if (number < 0) return false;
    int res = btpResID(number);
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Aj", res);
    JUT_ASSERT(DEMO_SELECT(0x212, 0x213), a_btp != 0);
    mBtpNo = number; mBtpFrame = 0; mBtpTimer = 0;
    return mBtpAnm.init(model->getModelData(), a_btp, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, modify, 0) != FALSE;
}
void daNpc_Aj1_c::play_texPttrnAnm() {
    if (mBtpNo != 0 || cLib_calcTimer(&mBtpTimer) == 0) {
        mBtpFrame++;
        s16 end = mBtpAnm.getBtpAnm()->getFrameMax();
        if (mBtpFrame >= end) {
            if (mBtpNo != 0) mBtpFrame = end;
            else { mBtpTimer = cLib_getRndValue<int>(60, 90); mBtpFrame = 0; }
        }
    }
}
void daNpc_Aj1_c::setAnm_anm(anm_prm_c* prm) {
    if (prm->mAnm < 0 || mAnmNo == prm->mAnm) return;
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bckResID(prm->mAnm), -1, "Aj");
    mAnmNo = prm->mAnm; mAnmEnd = 0; m759 = 0; mPrevFrame = 0.0f;
    switch (mAnmNo) {
    case 2: set_pa_pun(); set_pa_aka(); set_pa_don(); break;
    default:
        if (mpRed) { mpRed->stopCreateParticle(); mRedFading = true; }
        del_pa(&mpSteam); del_pa(&mpImpact); break;
    }
}
void daNpc_Aj1_c::setAnm_NUM(int n, int texture) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0,0,0,1,2}, {1,0,0,1,2}, {2,0,0,1,2}, {3,0,8,1,0}, {4,0,0,1,0},
        {5,0,5,1,0}, {6,0,0,1,2}, {7,0,8,1,2}, {8,0,8,1,0}
    };
    if (texture) init_texPttrnAnm(a_anm_prm_tbl[n].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[n]);
}
void daNpc_Aj1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {{-1,-1,0,0,-1},{0,0,0,1,2},{7,0,8,1,2},{-1,-1,0,0,-1},{0,0,8,1,2}};
    init_texPttrnAnm(a_anm_prm_tbl[mState].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}
void daNpc_Aj1_c::chngAnmTag() {}
void daNpc_Aj1_c::ctrlAnmTag() {}
void daNpc_Aj1_c::chngAnmAtr(u8 attr) {
    if (attr == mAnmAttr || attr > 9) return;
    mAnmAttr = attr; setAnm_ATR();
    switch (mAnmAttr) { case 8: mTireCycles = 3; break; }
}
void daNpc_Aj1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 2: ctrl_WAITanm(); break;
    case 5: if (mAnmEnd) { mAnmAttr = 0; setAnm_NUM(0, 1); } break;
    case 0: case 1: case 3: case 4: case 6: case 7: case 8: break;
    }
    ctrl_TIREanm();
}
void daNpc_Aj1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {{0,0,0,1,2},{1,0,0,1,2},{0,0,8,1,2},{2,0,0,1,2},{3,0,8,1,0},{1,0,8,1,2},{4,0,0,1,0},{5,0,5,1,0},{6,0,0,1,2}};
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}
void daNpc_Aj1_c::anmAtr(u16 status) {
    switch (status) {
    case 6: {
        if (!mMsgAnmStarted) { chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo()); mMsgAnmStarted++; }
        u8 tag = dComIfGp_getMesgAnimeTagInfo();
        if (tag != 0xFF && tag != mAnmTag) { dComIfGp_clearMesgAnimeTagInfo(); mAnmTag = tag; chngAnmTag(); }
        break;
    }
    case 14: mMsgAnmStarted = 0; break;
    }
    ctrlAnmAtr(); ctrlAnmTag();
}
u16 daNpc_Aj1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x9C6: *msg = 0x9C7; break;
    case 0x9C7: *msg = 0x9C8; break;
    case 0x9C9: *msg = 0x9CA; break;
    case 0x9C8: case 0x9CA:
        if (dComIfGs_isEventBit(1) && !dComIfGs_isEventBit(0x3704)) *msg = 0x9DA; else status = 16; break;
    case 0x9DB:
        if (!dComIfGs_isEventBit(0x504)) { if (dComIfGs_isEventBit(0x2A80)) *msg = 0x9DC; else *msg = 0x9CB; }
        else *msg = 0x9CF;
        break;
    case 0x9CB: case 0x9DC: *msg = 0x9CC; break;
    case 0x9CD: *msg = 0x9CE; break;
    case 0x9CF:
        if (dComIfGs_isEventBit(1) && !dComIfGs_isEventBit(0x3704)) *msg = 0x9DD; else status = 16; break;
    case 0x9D0:
        if (!dComIfGs_isEventBit(0x2A20)) *msg = 0x9D1;
        else if (dKy_daynight_check() == 0) *msg = 0x9D2; else *msg = 0x9D3;
        break;
    default: status = 16; break;
    }
    return status;
}
u32 daNpc_Aj1_c::getMsg_AJ1_0() { return dComIfGs_isEventBit(0x510) ? 0x9C9 : 0x9C6; }
u32 daNpc_Aj1_c::getMsg_AJ1_1() {
    if (dComIfGs_isEventBit(0xE20) && !dComIfGs_isEventBit(0x3702)) { dComIfGs_onEventBit(0x3702); return 0x9DB; }
    if (dComIfGs_isEventBit(0x504)) return 0x9CF;
    return dComIfGs_isEventBit(0x2A80) ? 0x9DC : 0x9CB;
}
u32 daNpc_Aj1_c::getMsg_AJ1_2() {
    if (mItemNo == 0x48) {
        if (!dComIfGs_isEventBit(0x3701)) return 0x9D7;
        return dComIfGs_isEventBit(0xB20) ? 0x9D8 : 0x9D9;
    }
    if (!dComIfGs_isEventBit(0x3708)) return 0x9D0;
    if (!dComIfGs_isEventBit(0x2A20)) return 0x9D4;
    return dKy_daynight_check() == 0 ? 0x9D5 : 0x9D6;
}
u32 daNpc_Aj1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) { case 0: msg=getMsg_AJ1_0(); break; case 1: msg=getMsg_AJ1_1(); break; case 2: msg=getMsg_AJ1_2(); break; }
    return msg;
}
void daNpc_Aj1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mSubType == 2) eventInfo.onCondition(0x20);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    } else if (mOrder >= 3) {
        mEventNo = mOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIdx[mEventNo]);
    }
}
void daNpc_Aj1_c::checkOrder() {
    if (eventInfo.getCommand() == dEvtCmd_INDEMO_e) {
        if (dComIfGp_evmng_startCheck(mEventIdx[mEventNo])) {
#if VERSION > VERSION_DEMO
            switch (mEventNo) { case 0: fopAcM_OffStatus(this, 0x4000); break; }
#endif
            mOrder = 0;
        }
    } else if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mOrder == 1 || mOrder == 2)) { mOrder = 0; mTalking = true; }
}
bool daNpc_Aj1_c::chk_talk() {
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) { mItemNo = dComIfGp_event_getPreItemNo(); return true; }
        return false;
    }
    mItemNo = 0xFF; return true;
}
bool daNpc_Aj1_c::chk_parts_notMov() { return mPreviousHeadY == m_jnt.getHead_y() && mPreviousBackboneY == m_jnt.getBackbone_y() && mPreviousAngleY == current.angle.y; }
void daNpc_Aj1_c::lookBack() {
    mPreviousHeadY = m_jnt.getHead_y(); mPreviousBackboneY = m_jnt.getBackbone_y(); mPreviousAngleY = current.angle.y;
    cXyz target;
    cXyz base = current.pos; base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 1: target = dNpc_playerEyePos(-20.0f); targetP = &target; break;
    case 2: target = mLookPos; targetP = &target; break;
    case 3: angle = mLookAngle; break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mPrm.mTurnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) mTurnSpeed = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, noTurn);
}
bool daNpc_Aj1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) return this == attention.LockonTarget(0);
    return this == attention.ActionTarget(0);
}
void daNpc_Aj1_c::setAttention(bool force) {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionOffsetY, current.pos.z);
    if (mUpdateEye || force) eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
}
bool daNpc_Aj1_c::decideType(int type) {
    mType = 0; mSubType = -1;
    switch (type) { case 0:mSubType=0;break;case 1:mSubType=1;break;case 2:mSubType=2;break; }
    return mType != -1 && mSubType != -1;
}
void daNpc_Aj1_c::cut_init_AJ1_TLK() { mAnmAttr=0xFF; mAnmTag=0xFF; mMsgAnmStarted=0; }
bool daNpc_Aj1_c::cut_move_AJ1_TLK() {
    if (talk(1) == 18) { mAnmAttr=0xFF; mAnmTag=0xFF; mMsgAnmStarted=0; return true; }
    return false;
}
void daNpc_Aj1_c::cut_init_INI_ANGRY() { setAnm_NUM(0,1); mpMorf->setMorf(8.0f); }
bool daNpc_Aj1_c::cut_move_INI_ANGRY() { return true; }
void daNpc_Aj1_c::cut_init_VIVRATE() {}
bool daNpc_Aj1_c::cut_move_VIVRATE() {
    if (mAnmEnd) {
        mDoAud_seStart(JA_SE_CM_JI_BODY_ATTACK, NULL, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        dComIfGp_getVibration().StartShock(5, -17, cXyz(0.0f, 1.0f, 0.0f));
        setAnm_NUM(3,1); return true;
    }
    return false;
}
void daNpc_Aj1_c::cut_init_JMP() {}
bool daNpc_Aj1_c::cut_move_JMP() {
    if (mAnmEnd) { dComIfGp_getDetect().set_quake(NULL); mCutTimer=20; return true; }
    return false;
}
void daNpc_Aj1_c::cut_init_SPPRISE() {}
bool daNpc_Aj1_c::cut_move_SPPRISE() {
    if (cLib_calcTimer(&mCutTimer) == 0) { setAnm_NUM(4,1); mCutTimer=l_HIO.mPrm.mLookDelay; return true; }
    return false;
}
void daNpc_Aj1_c::cut_init_LOK() {}
bool daNpc_Aj1_c::cut_move_LOK() {
    if (mAnmEnd && cLib_calcTimer(&mCutTimer) == 0) {
        setAnm_NUM(5,1);
        mDoAud_seStart(JA_SE_CV_AJ_ANGRY, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        mCutTimer=l_HIO.mPrm.mAngryDelay; return true;
    }
    return false;
}
void daNpc_Aj1_c::cut_init_DAN() {}
bool daNpc_Aj1_c::cut_move_DAN() {
    if (mAnmEnd && cLib_calcTimer(&mCutTimer) == 0) { setAnm_NUM(2,1); return true; }
    return false;
}
void daNpc_Aj1_c::cut_init_INVIT() { mDoAud_seStart(JA_SE_CV_AJ_INVITE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this))); }
bool daNpc_Aj1_c::cut_move_INVIT() { return true; }
void daNpc_Aj1_c::privateCut(int staff) {
    if (staff == -1) return;
    dEvent_manager_c& evm = g_dComIfG_gameInfo.play.getEvtManager();
    static const char* a_cut_tbl[] = {"AJ1_TLK","INI_ANGRY","VIVRATE","JMP","SPPRISE","LOK","DAN","INVITE"};
    mCut = evm.getMyActIdx(staff, a_cut_tbl, 8, TRUE, FALSE);
    if (mCut == -1) { evm.cutEnd(staff); return; }
    if (evm.getIsAddvance(staff)) {
        switch(mCut) {
        case 0:cut_init_AJ1_TLK();break;case 1:cut_init_INI_ANGRY();break;
        case 2:cut_init_VIVRATE();break;case 3:cut_init_JMP();break;
        case 4:cut_init_SPPRISE();break;case 5:cut_init_LOK();break;
        case 6:cut_init_DAN();break;case 7:cut_init_INVIT();break;
        }
    }
    bool done;
    switch(mCut) {
    case 0:done=cut_move_AJ1_TLK();break;case 1:done=cut_move_INI_ANGRY();break;
    case 2:done=cut_move_VIVRATE();break;case 3:done=cut_move_JMP();break;
    case 4:done=cut_move_SPPRISE();break;case 5:done=cut_move_LOK();break;
    case 6:done=cut_move_DAN();break;case 7:done=cut_move_INVIT();break;
    default:done=true;break;
    }
    if (done) evm.cutEnd(staff);
}
void daNpc_Aj1_c::endEvent() { dComIfGp_event_reset(); mAnmAttr=0xFF; mAnmTag=0xFF; }
int daNpc_Aj1_c::isEventEntry() { return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0); }
void daNpc_Aj1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIdx[mEventNo])) {
        switch(mEventNo) { case 0: dComIfGs_onEventBit(0x508); dComIfGs_onEventBit(0x504); break; }
        endEvent();
    } else { if (!mEventCut.cutProc()) privateCut(staff); lookBack(); }
}
void daNpc_Aj1_c::set_pa_pun() {
    mpSteam=dComIfGp_particle_set(0x8113, &current.pos, NULL, NULL, 0xFF, NULL, fopAcM_GetRoomNo(this));
    if (mpSteam) mSteamTimer=0;
}
void daNpc_Aj1_c::set_pa_aka() {
    if (mpRed) mpRed->becomeInvalidEmitter();
    mpRed=dComIfGp_particle_set(0x811F, &current.pos, NULL, NULL, 0xFF, NULL, fopAcM_GetRoomNo(this));
}
void daNpc_Aj1_c::set_pa_don() {
    cXyz offset(37.3f,0.0f,13.8f);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mDoMtx_stack_c::multVec(&offset, &mImpactPos);
    mpImpact=dComIfGp_particle_set(0x8114, &mImpactPos, NULL, NULL, 0xFF, NULL, fopAcM_GetRoomNo(this));
}
void daNpc_Aj1_c::set_pa_smk() {
    GXColor color={160,160,128,200};
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_fot_L_jnt_num));
    mSmokePos.x=mDoMtx_stack_c::get()[0][3];
    mSmokePos.y=mDoMtx_stack_c::get()[1][3];
    mSmokePos.z=mDoMtx_stack_c::get()[2][3];
    mSmokeCallback.end();
    mpSmoke=dComIfGp_particle_setToon(0x2027, &mSmokePos, &current.angle, NULL, 200, &mSmokeCallback, fopAcM_GetRoomNo(this));
    if (mpSmoke) {
        mpSmoke->setGlobalParticleScale(0.3f,0.3f);
        mpSmoke->setGlobalDynamicsScale(JGeometry::TVec3<f32>(0.1f,0.1f,0.1f));
        mpSmoke->setLifeTime(40);
        mpSmoke->setRate(3.0f);
        mpSmoke->setMaxFrame(1);
        mpSmoke->setDirectionalSpeed(120.0f);
        mpSmoke->setEmitterScale(1.0f,0.1f,1.0f);
        mSmokeCallback.setColor(color);
    }
}
void daNpc_Aj1_c::flw_pa_pun() {
    if (mpSteam) {
        mpSteam->setGlobalRTMatrix(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
        if (cLib_calcTimer(&mSteamTimer) == 0) {
            mDoAud_seStart(JA_SE_CM_AJ_ANGRY_STEAM, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            mSteamTimer=5;
        }
    }
}
void daNpc_Aj1_c::flw_pa_aka() { if (mpRed) mpRed->setGlobalRTMatrix(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num)); }
void daNpc_Aj1_c::del_pa_aka() {
    if (mRedFading) {
        if (mpRed) {
            if (mpRed->getParticleNumber() == 0) { mpRed->becomeInvalidEmitter(); mRedFading=false; mpRed=NULL; }
        } else mRedFading=false;
    }
}
void daNpc_Aj1_c::del_pa(JPABaseEmitter** emitter) {
    if (*emitter) { (*emitter)->becomeInvalidEmitter(); *emitter=NULL; }
}
void daNpc_Aj1_c::setSmoke() {
    if (mAnmNo == 2) {
        if (mpMorf->checkFrame(0.0f) || mpMorf->checkFrame(9.0f)) set_pa_smk();
        if (mpMorf->checkFrame(0.0f) || mpMorf->checkFrame(10.0f))
            mDoAud_seStart(JA_SE_CM_AJ_STICK, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}
bool daNpc_Aj1_c::set_action(ActionFunc action, void* arg) {
    if (mAction != action) {
        if (mAction) { mActionState=9; (this->*mAction)(arg); }
        mAction=action; mActionState=0; (this->*mAction)(arg);
    }
    return true;
}
void daNpc_Aj1_c::setStt(s8 state) {
    s8 previous=mState; mState=state;
    switch(mState) {
    case 0: break;
    case 1: mOrder=0; mFarTimer=cLib_getRndValue<int>(90,120); break;
    case 2: mOrder=0; break;
    case 3: mOrder=0; mAnmAttr=0xFF; mAnmTag=0xFF; mMsgAnmStarted=0; mPreviousState=previous; break;
    case 4:
        mOrder=0; mWaitAnmTimer=cLib_getRndValue<int>(90,180);
        if (mAnmNo == 1 || mAnmNo == 6) return;
        break;
    }
    setAnm();
}
bool daNpc_Aj1_c::chk_areaIN(f32 distance, s16 angle, cXyz pos) {
    f32 dist=(dComIfGp_getPlayer(0)->current.pos-pos).absXZ();
    f32 height=dComIfGp_getPlayer(0)->current.pos.y-pos.y;
    int angleTo=cLib_targetAngleY(&current.pos,&dComIfGp_getPlayer(0)->current.pos);
    s16 diff=angleTo-mInitialAngle.y;
    if (dist<distance && (f32)fabs(height)<500.0f && abs(diff)<angle) return true;
    return false;
}
BOOL daNpc_Aj1_c::FARwai() {
    if (chk_areaIN(l_HIO.mPrm.mFarDistance,l_HIO.mPrm.mFarAngle,current.pos)) {
        setStt(2); mLookMode=1; mNoTurn=false; m_jnt.setTrn(); return TRUE;
    }
    if (mAnmNo == 8) {
        if (mAnmEnd) { setAnm_NUM(0,1); mpMorf->setMorf(8.0f); }
        return TRUE;
    }
    if (cLib_calcTimer(&mFarTimer)==0) { setAnm_NUM(8,1); mFarTimer=cLib_getRndValue<int>(90,120); }
    return TRUE;
}
BOOL daNpc_Aj1_c::call_1() {
    if (mTalking) {
        if (chk_talk()) { setStt(3); mLookMode=1; mNoTurn=false; m_jnt.setTrn(); }
        return TRUE;
    }
    bool outside=!chk_areaIN(l_HIO.mPrm.mFarDistance,l_HIO.mPrm.mFarAngle,current.pos);
    if (outside) {
        setStt(1); mpMorf->setMorf(15.0f); mLookMode=3; mLookAngle=mInitialAngle.y;
        mNoTurn=false; m_jnt.setTrn(); return TRUE;
    }
    m_jnt.setTrn();
    if (chk_areaIN(l_HIO.mPrm.mTalkDistance,l_HIO.mPrm.mTalkAngle,current.pos)) {
        mOrder=2;
        if (mHasAttention) {
            if (mAnmNo!=0) { setAnm_NUM(0,1); mpMorf->setMorf(15.0f); }
            return TRUE;
        }
        if (mAnmNo!=7) setAnm_NUM(7,1);
    }
    return TRUE;
}
BOOL daNpc_Aj1_c::wait_1() {
    cLib_addCalcAngleS(&current.angle.y,mInitialAngle.y,4,0x400,0);
    ctrl_WAITanm();
    if (mOrder==1 || mOrder>=3) return TRUE;
#if VERSION == VERSION_DEMO
    if (!strcmp(dComIfGp_getStartStageName(),"Ojhous2") && mSubType==1 && !dComIfGs_isEventBit(0x508)) { mOrder=3; return TRUE; }
#endif
    ctrl_TIREanm();
    if (mTalking) {
        if (chk_talk()) { setStt(3); mLookMode=1; mNoTurn=false; m_jnt.setTrn(); }
        return TRUE;
    }
    mOrder=2;
    if (mAnmNo!=1 && mHasAttention) { mLookMode=1; return TRUE; }
    mLookMode=3; mLookAngle=mInitialAngle.y; return TRUE;
}
BOOL daNpc_Aj1_c::talk_1() {
    BOOL result=chk_parts_notMov();
    talk(1);
    if (!mpCurrMsg) return result;
    switch(mpCurrMsg->mStatus) {
    case 2: case 6: break;
    case 19:
        switch(mCurrMsgNo) {
        case 0x9DA: dComIfGs_onEventBit(0x3704);
        case 0x9C8: dComIfGs_onEventBit(0x510); break;
        case 0x9DD: dComIfGs_onEventBit(0x3704); break;
        case 0x9D1: case 0x9D2: case 0x9D3: dComIfGs_onEventBit(0x3708); break;
        case 0x9D7: dComIfGs_onEventBit(0x3701); break;
        }
        mItemNo=0xFF; mTalking=false; setStt(mPreviousState);
        mTalkTimer=cLib_getRndValue<int>(15,30); endEvent(); break;
    }
    return result;
}
int daNpc_Aj1_c::wait_action1(void*) {
    switch(mActionState) {
    case 0: setStt(1); mActionState++; break;
    case 1: case 2: case 3:
        mHasAttention=chkAttention();
        switch(mState) { case 1:mUpdateEye=FARwai();break;case 2:mUpdateEye=call_1();break;case 3:mUpdateEye=talk_1();break; }
        lookBack(); break;
    case 9: break;
    }
    return TRUE;
}
int daNpc_Aj1_c::wait_action2(void*) {
    switch(mActionState) {
    case 0:
        setStt(4);
#if VERSION > VERSION_DEMO
        if (!strcmp(dComIfGp_getStartStageName(),"Ojhous2") && mSubType==1 && !dComIfGs_isEventBit(0x508)) mOrder=3;
#endif
        mActionState++; break;
    case 1: case 2: case 3:
        mHasAttention=chkAttention();
        switch(mState) { case 4:mUpdateEye=wait_1();break;case 3:mUpdateEye=talk_1();break; }
        lookBack(); break;
    case 9: break;
    }
    return TRUE;
}
u8 daNpc_Aj1_c::demo() {
    if (demoActorID==0) { if (mDemo) mDemo=false; }
    else {
        if (!mDemo) {
            mDemo=true; mKeepShapeAngle=false;
            m_jnt.setHead_y(0); m_jnt.setHead_x(0); m_jnt.setBackBone_y(0); m_jnt.setBackBone_x(0);
        }
        dDemo_actor_c* actor=dComIfGp_demo_getActor(demoActorID);
        if (mBtpAnm.getBtpAnm()) {
            mBtpFrame++;
            s16 end=mBtpAnm.getBtpAnm()->getFrameMax();
            if (mBtpFrame>=end) mBtpFrame=end;
        }
        J3DAnmTexPattern* btp=actor->getP_BtpData("Aj");
        if (btp) {
            mBtpAnm.init(mpMorf->getModel()->getModelData(),btp,TRUE,0,1.0f,0,-1,true,0);
            mBtpNo=1; mBtpFrame=0;
        }
        dDemo_setDemoData(this,0x6A,mpMorf,"Aj");
    }
    return mDemo;
}
void daNpc_Aj1_c::shadowDraw() {
    cXyz pos(current.pos.x,150.0f+current.pos.y,current.pos.z);
    mShadowId=dComIfGd_setShadow(mShadowId,1,mpMorf->getModel(),&pos,800.0f,40.0f,current.pos.y,mObjAcch.GetGroundH(),mObjAcch.m_gnd,&tevStr,0,1.0f,dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId && mpItemModel) dComIfGd_addRealShadow(mShadowId,mpItemModel);
}
BOOL daNpc_Aj1_c::_draw() {
    J3DModel* model=mpMorf->getModel();
    J3DModelData* data=model->getModelData();
    if (mHidden || mNoDraw) return TRUE;
    g_env_light.settingTevStruct(0,&current.pos,&tevStr);
    g_env_light.setLightTevColorType(model,&tevStr);
    mBtpAnm.entry(data,mBtpFrame);
    mpMorf->entryDL();
    mBtpAnm.remove(data);
    if (mpItemModel) { g_env_light.setLightTevColorType(mpItemModel,&tevStr); mDoExt_modelEntryDL(mpItemModel); }
    shadowDraw();
    if (l_HIO.mPrm.mDebug) {
        cXyz pos=current.pos; pos.y=eyePos.y;
        GXColor red={255,0,0,128};
        GXColor blue={0,0,255,128};
        GXColor yellow={255,255,0,128};
    }
    dSnap_RegistFig(0x4C,this,1.0f,1.0f,1.0f);
    return TRUE;
}
BOOL daNpc_Aj1_c::_execute() {
    if (!mSavedInitialPos) { mInitialPos=current.pos; mInitialAngle=current.angle; mSavedInitialPos=true; }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackBoneX,l_HIO.mPrm.mMaxBackBoneY,l_HIO.mPrm.mMinBackBoneX,l_HIO.mPrm.mMinBackBoneY,l_HIO.mPrm.mMaxHeadX,l_HIO.mPrm.mMaxHeadY,l_HIO.mPrm.mMinHeadX,l_HIO.mPrm.mMinHeadY,l_HIO.mPrm.mMaxTurnStep);
    if (mHidden && demoActorID==0) return TRUE;
    checkOrder();
    if (!demo()) {
        int staff=-1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) staff=isEventEntry();
        if (staff>=0) event_proc(staff); else (this->*mAction)(NULL);
        fopAcM_posMoveF(this,mStts.GetCCMoveP()); play_animation(); mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    eventOrder(); mModelAngle=current.angle;
    if (!mKeepShapeAngle) shape_angle=current.angle;
    tevStr.mRoomNo=dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride=dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    setMtx(false); flw_pa_pun(); del_pa_aka(); flw_pa_aka(); setSmoke();
    if (!mDemo) setCollision(60.0f,140.0f);
    return TRUE;
}
BOOL daNpc_Aj1_c::_delete() {
#if VERSION == VERSION_DEMO
    dComIfG_resDelete(&mPhs,"Aj");
    del_pa(&mpSteam); del_pa(&mpRed); del_pa(&mpImpact); mSmokeCallback.end();
    if (mpMorf) mpMorf->stopZelAnime();
    l_HIO.removeHIO();
    return TRUE;
#else
    dComIfG_resDelete(&mPhs,"Aj");
    if (heap && mpMorf) mpMorf->stopZelAnime();
    del_pa(&mpSteam); del_pa(&mpRed); del_pa(&mpImpact); mSmokeCallback.end();
    return TRUE;
#endif
}
cPhs_State daNpc_Aj1_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase=dComIfG_resLoad(&mPhs,"Aj");
    if (phase!=cPhs_COMPLEATE_e) return phase;
    int type=fopAcM_GetParam(this)&0xFF;
    if (!decideType(type)) return cPhs_ERROR_e;
    l_HIO.entryHIO("青爺");
    fopAcM_SetupActor(this,daNpc_Aj1_c);
#else
    fopAcM_SetupActor(this,daNpc_Aj1_c);
    cPhs_State phase=dComIfG_resLoad(&mPhs,"Aj");
    if (phase!=cPhs_COMPLEATE_e) return phase;
    if (!decideType(fopAcM_GetParam(this)&0xFF)) return cPhs_ERROR_e;
#endif
    static u32 a_siz_tbl[]={0};
    if (!fopAcM_entrySolidHeap(this,CheckCreateHeap,a_siz_tbl[mType])) return cPhs_ERROR_e;
    fopAcM_SetMtx(this,mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this,-60.0f,-20.0f,-60.0f,80.0f,260.0f,100.0f);
    return createInit() ? phase : cPhs_ERROR_e;
}
BOOL daNpc_Aj1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat=(J3DModelData*)dComIfG_getObjectIDRes("Aj",dRes_ID_AJ_BDL_AJ_e);
    JUT_ASSERT(DEMO_SELECT(0x8A9, 0x8BE),a_mdl_dat != 0);
    mpMorf=new mDoExt_McaMorf(a_mdl_dat,NULL,NULL,NULL,-1,1.0f,0,-1,1,NULL,0x80000,0x11020022);
    if (!mpMorf) return FALSE;
    if (!mpMorf->getModel()) { mpMorf=NULL; return FALSE; }
    if (!init_texPttrnAnm(0,false)) { mpMorf=NULL; return FALSE; }
    m_hed_jnt_num=a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x8BE, 0x8D3),m_hed_jnt_num >= 0);
    m_bbone_jnt_num=a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x8C0, 0x8D5),m_bbone_jnt_num >= 0);
    m_hnd_L_jnt_num=a_mdl_dat->getJointName()->getIndex("handL");
    JUT_ASSERT(DEMO_SELECT(0x8C2, 0x8D7),m_hnd_L_jnt_num >= 0);
    m_fot_L_jnt_num=a_mdl_dat->getJointName()->getIndex("footL");
    JUT_ASSERT(DEMO_SELECT(0x8C4, 0x8D9),m_fot_L_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}
BOOL daNpc_Aj1_c::itemCreateHeap() {
    J3DModelData* a_mdl_dat=(J3DModelData*)dComIfG_getObjectIDRes("Aj",dRes_ID_AJ_BDL_AJ_STICK_e);
    JUT_ASSERT(DEMO_SELECT(0x8DE, 0x8F3),a_mdl_dat != 0);
    mpItemModel=mDoExt_J3DModel__create(a_mdl_dat,0x80000,0x11000022);
    if (!mpItemModel) return FALSE;
    return TRUE;
}
BOOL daNpc_Aj1_c::CreateHeap() {
    if (!bodyCreateHeap()) return FALSE;
    if (!itemCreateHeap()) { mpMorf=NULL; return FALSE; }
    mAcchCir.SetWall(30.0f,60.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this),fopAcM_GetOldPosition_p(this),this,1,&mAcchCir,fopAcM_GetSpeed_p(this),NULL,NULL);
    return TRUE;
}
/* 00003FFC-0000401C       .text daNpc_Aj1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Aj1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Aj1_c*)i_this)->_create();
}

/* 0000401C-0000403C       .text daNpc_Aj1_Delete__FP11daNpc_Aj1_c */
static BOOL daNpc_Aj1_Delete(daNpc_Aj1_c* i_this) {
    return ((daNpc_Aj1_c*)i_this)->_delete();
}

/* 0000403C-0000405C       .text daNpc_Aj1_Execute__FP11daNpc_Aj1_c */
static BOOL daNpc_Aj1_Execute(daNpc_Aj1_c* i_this) {
    return ((daNpc_Aj1_c*)i_this)->_execute();
}

/* 0000405C-0000407C       .text daNpc_Aj1_Draw__FP11daNpc_Aj1_c */
static BOOL daNpc_Aj1_Draw(daNpc_Aj1_c* i_this) {
    return ((daNpc_Aj1_c*)i_this)->_draw();
}

/* 0000407C-00004084       .text daNpc_Aj1_IsDelete__FP11daNpc_Aj1_c */
static BOOL daNpc_Aj1_IsDelete(daNpc_Aj1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Aj1_Method = {
    (process_method_func)daNpc_Aj1_Create,
    (process_method_func)daNpc_Aj1_Delete,
    (process_method_func)daNpc_Aj1_Execute,
    (process_method_func)daNpc_Aj1_IsDelete,
    (process_method_func)daNpc_Aj1_Draw,
};

actor_process_profile_definition g_profile_NPC_AJ1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_AJ1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Aj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_AJ1_e,
    /* Actor SubMtd */ &l_daNpc_Aj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
