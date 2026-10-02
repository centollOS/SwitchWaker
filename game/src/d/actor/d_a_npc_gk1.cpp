/**
 * d_a_npc_gk1.cpp
 * NPC - Mila's Father (poor)
 */
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_gk1.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Gk.h"
#include "m_Do/m_Do_lib.h"
static daNpc_Gk1_HIO_c l_HIO;
// Unused actor search workspace retained by the original module.
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;
/* 000000EC-00000150       .text __ct__15daNpc_Gk1_HIO_cFv */
daNpc_Gk1_HIO_c::daNpc_Gk1_HIO_c() {
    static hio_prm_c a_prm_tbl = {0x2000, 0xFA0, (s16)0xEE6C, (s16)0xF060, 0, 0xDAC, -1000, (s16)0xF254, 0x578, 1000, 200.0f, 0};
    memcpy(&mPrm, &a_prm_tbl, sizeof(mPrm));
    mNo = -1;
    mCount = -1;
}
daNpc_Gk1_HIO_c::~daNpc_Gk1_HIO_c() {}

static char* l_evn_tbl[] = {"dummy"};
/* 00000198-000001E4       .text nodeCB_Head__FP7J3DNodei */
static BOOL nodeCB_Head(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Gk1_c* npc = (daNpc_Gk1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_Head(node, model);
        }
    }
    return TRUE;
}

/* 000001E4-000002D8       .text _nodeCB_Head__11daNpc_Gk1_cFP7J3DNodeP8J3DModel */
void daNpc_Gk1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_offst(30.0f, 30.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mHeadPos.x = mDoMtx_stack_c::get()[0][3];
    mHeadPos.y = mDoMtx_stack_c::get()[1][3];
    mHeadPos.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::multVec(&a_eye_pos_offst, &mEyeLocal);
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000314-00000360       .text nodeCB_Neck__FP7J3DNodei */
static BOOL nodeCB_Neck(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Gk1_c* npc = (daNpc_Gk1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_Neck(node, model);
        }
    }
    return TRUE;
}

/* 00000360-00000400       .text _nodeCB_Neck__11daNpc_Gk1_cFP7J3DNodeP8J3DModel */
void daNpc_Gk1_c::_nodeCB_Neck(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000400-0000044C       .text nodeCB_BackBone__FP7J3DNodei */
static BOOL nodeCB_BackBone(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Gk1_c* npc = (daNpc_Gk1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_BackBone(node, model);
        }
    }
    return TRUE;
}

/* 0000044C-000004EC       .text _nodeCB_BackBone__11daNpc_Gk1_cFP7J3DNodeP8J3DModel */
void daNpc_Gk1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 000004EC-0000050C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Gk1_c*)actor)->CreateHeap();
}

/* 0000050C-0000058C       .text init_GK1_0__11daNpc_Gk1_cFv */
bool daNpc_Gk1_c::init_GK1_0() {
    if (dComIfGs_isEventBit(0x2D01)) {
        set_action(&daNpc_Gk1_c::wait_action1, NULL);
        return true;
    }
    return false;
}

/* 0000058C-00000720       .text createInit__11daNpc_Gk1_cFv */
bool daNpc_Gk1_c::createInit() {
    for (int i = 0; i < 1; i++) {
        mEventIds[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    }
    mEventCut.setActorInfo2("Gk1", this);
    attention_info.flags = 0xA;
    fopAcM_setCullSizeFar(this, 12000.0f / mDoLib_clipper::getFar());
    switch (mSubtype) {
    case 0:
    default:
        break;
    }
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA9;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;
    gravity = -4.5f;
    mAnmIndex = 5;
    bool result;
    switch (mSubtype) {
    case 0:
        result = init_GK1_0();
        break;
    default:
        result = false;
        break;
    }
    if (!result) {
        return false;
    }
    mModelAngle = current.angle;
    shape_angle = mModelAngle;
    mStts.Init(0xFF, 0xFF, this);
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

/* 00000720-000007C8       .text play_animation__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::play_animation() {
    u32 sound = 0;
    play_btp_anm();
    if (mObjAcch.ChkGroundHit()) {
        sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    }
    mAnmEnded = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    if (mpMorf->getFrame() < mLastFrame) {
        mAnmEnded = 1;
    }
    mLastFrame = mpMorf->getFrame();
}

/* 000007C8-00000930       .text setMtx__11daNpc_Gk1_cFb */
void daNpc_Gk1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(mModelAngle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    if (mpItemModel != NULL) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
        mpItemModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mpItemModel->calc();
    }
    if (mpHatModel != NULL) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
        mpHatModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mpHatModel->calc();
    }
    setAttention(force);
}

/* 00000930-00000944       .text bckResID__11daNpc_Gk1_cFi */
int daNpc_Gk1_c::bckResID(int index) {
    static const int a_resID_tbl[] = {dRes_ID_GK_BCK_GK_WAIT01_e, dRes_ID_GK_BCK_GK_WALK_e, dRes_ID_GK_BCK_GK_TALK01_e, dRes_ID_GK_BCK_GK_TALK02_e, dRes_ID_GK_BCK_GK_TALK03_e};
    return a_resID_tbl[index];
}

/* 00000944-00000958       .text btpResID__11daNpc_Gk1_cFi */
int daNpc_Gk1_c::btpResID(int index) {
    static const int a_resID_tbl[] = {dRes_ID_GK_BTP_GK_e};
    return a_resID_tbl[index];
}

/* 00000958-00000A5C       .text setBtp__11daNpc_Gk1_cFScb */
bool daNpc_Gk1_c::setBtp(s8 index, bool modify) {
    J3DModel* model = mpMorf->getModel();
    if (index < 0) {
        return false;
    }
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes(mArcName, btpResID(index));
    JUT_ASSERT(451, a_btp != 0);
    mTexIndex = index;
    mBtpFrame = 0;
    mBlinkTimer = 0;
    return mBtp.init(model->getModelData(), a_btp, TRUE, 0, 1.0f, 0, -1, modify, FALSE) != FALSE;
}

/* 00000A5C-00000A7C       .text init_texPttrnAnm__11daNpc_Gk1_cFScb */
bool daNpc_Gk1_c::init_texPttrnAnm(s8 index, bool modify) {
    return setBtp(index, modify);
}

/* 00000A7C-00000B18       .text play_btp_anm__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::play_btp_anm() {
    u8 max = mBtp.getBtpAnm()->getFrameMax();
    if (mTexIndex != 0 || cLib_calcTimer(&mBlinkTimer) == 0) {
        mBtpFrame++;
        if (mBtpFrame >= max) {
            if (mTexIndex != 0) {
                mBtpFrame = max;
            } else {
                mBlinkTimer = cLib_getRndValue<int>(60, 90);
                mBtpFrame = 0;
            }
        }
    }
}

/* 00000B18-00000BB4       .text setAnm_anm__11daNpc_Gk1_cFPQ211daNpc_Gk1_c9anm_prm_c */
void daNpc_Gk1_c::setAnm_anm(anm_prm_c* prm) {
    if (prm->mAnm < 0 || mAnmIndex == prm->mAnm) {
        return;
    }
    int res = bckResID(prm->mAnm);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, res, -1, mArcName);
    mAnmIndex = prm->mAnm;
    mAnmEnded = 0;
    mAnmRepeat = 0;
    mLastFrame = 0.0f;
}

/* 00000BB4-00000C20       .text setAnm__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {{-1, -1, 0, 0, -1}, {0, 0, 8.0f, 1.0f, 2}, {-1, -1, 0, 0, -1}};
    init_texPttrnAnm(a_anm_prm_tbl[mState].mTex, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

/* 00000C20-00000C24       .text chngAnmTag__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::chngAnmTag() {
}

/* 00000C24-00000C28       .text ctrlAnmTag__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::ctrlAnmTag() {
}

/* 00000C28-00000C68       .text chngAnmAtr__11daNpc_Gk1_cFUc */
void daNpc_Gk1_c::chngAnmAtr(u8 attr) {
    if (attr == mAnmAttr || attr > 5) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR();
}

/* 00000C68-00000C74       .text ctrlAnmAtr__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 5:
    default:
        break;
    }
}

/* 00000C74-00000CD8       .text setAnm_ATR__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {{0, 0, 8.0f, 1.0f, 2}, {1, 0, 8.0f, 1.0f, 2}, {2, 0, 8.0f, 1.0f, 2}, {3, 0, 8.0f, 1.0f, 2}, {4, 0, 8.0f, 1.0f, 2}};
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mTex, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00000CD8-00000D94       .text anmAtr__11daNpc_Gk1_cFUs */
void daNpc_Gk1_c::anmAtr(u16 status) {
    switch (status) {
    case 6:
        if (mMessageState == 0) {
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
        break;
    case 14:
        mMessageState = 0;
        break;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

/* 00000D94-00000EAC       .text next_msgStatus__11daNpc_Gk1_cFPUl */
u16 daNpc_Gk1_c::next_msgStatus(u32* msg) {
    u16 result = 15;
    switch (*msg) {
    case 0x28A1:
        *msg = 0x28A2;
        break;
    case 0x28A2:
        *msg = 0x28A3;
        break;
    case 0x28A3:
        *msg = 0x28A4;
        break;
    case 0x28A4:
        *msg = 0x28A5;
        break;
    case 0x28A6:
        *msg = 0x28A7;
        break;
    case 0x28A7:
        *msg = 0x28A8;
        break;
    case 0x28A8:
        if (dComIfGs_isEventBit(0x1640)) {
            result = 16;
        } else {
            *msg = 0x28A9;
        }
        break;
    case 0x28A9:
        *msg = 0x28AA;
        break;
    case 0x28AB:
        *msg = 0x28AC;
        break;
    case 0x28AC:
        *msg = 0x28AD;
        break;
    case 0x28AD:
        *msg = 0x28AE;
        break;
    case 0x28AF:
        *msg = 0x28B0;
        break;
    default:
        result = 16;
        break;
    }
    return result;
}

/* 00000EAC-00000F50       .text getMsg_GK1_0__11daNpc_Gk1_cFv */
u32 daNpc_Gk1_c::getMsg_GK1_0() {
    if (dComIfGs_isEventBit(0xB02) && !dComIfGs_isEventBit(0x1680)) {
        return 0x28A1;
    }
    if (dKy_daynight_check() == 0) {
        return dComIfGs_isEventBit(0x1640) ? 0x28A7 : 0x28A6;
    }
    return dComIfGs_isEventBit(0xE08) ? 0x28AF : 0x28AB;
}

/* 00000F50-00000F8C       .text getMsg__11daNpc_Gk1_cFv */
u32 daNpc_Gk1_c::getMsg() {
    u32 result = 0;
    switch (mSubtype) {
    case 0:
        result = getMsg_GK1_0();
        break;
    }
    return result;
}

/* 00000F8C-00001018       .text eventOrder__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::eventOrder() {
    if (mEventOrder == 1 || mEventOrder == 2) {
        eventInfo.onCondition(1);
        if (mEventOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (mEventOrder >= 3) {
        mEventIndex = mEventOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIds[mEventIndex], 0xFF, 0xFFFF, 0, 1);
    }
}

/* 00001018-000010CC       .text checkOrder__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEventIds[mEventIndex]) && mEventOrder >= 3) {
            mEventOrder = 0;
            mAnmAttr = 0xFF;
            mAnmTag = 0xFF;
        }
    } else if (eventInfo.checkCommandTalk() && (mEventOrder == 1 || mEventOrder == 2)) { mEventOrder = 0; mTalking = true; }
}

/* 000010CC-00001164       .text chk_talk__11daNpc_Gk1_cFv */
bool daNpc_Gk1_c::chk_talk() {
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mPresentItem = dComIfGp_event_getPreItemNo();
            return true;
        }
        return false;
    }
    mPresentItem = 0xFF;
    return true;
}

/* 00001164-000011A4       .text chk_parts_notMov__11daNpc_Gk1_cFv */
bool daNpc_Gk1_c::chk_parts_notMov() {
    return mOldHeadY != m_jnt.getHead_y() || mOldBackY != m_jnt.getBackbone_y() || mOldActorY != current.angle.y;
}

/* 000011A4-000011F8       .text searchByID__11daNpc_Gk1_cFUiPi */
fopAc_ac_c* daNpc_Gk1_c::searchByID(fpc_ProcID id, int* missing) {
    fopAc_ac_c* actor = NULL;
    *missing = 0;
    if (!fopAcM_SearchByID(id, &actor)) {
        *missing = 1;
    }
    return actor;
}

/* 000011F8-000013BC       .text lookBack__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::lookBack() {
    mOldHeadY = m_jnt.getHead_y();
    mOldBackY = m_jnt.getBackbone_y();
    mOldActorY = current.angle.y;
    cXyz target;
    cXyz source = current.pos;
    source.y = eyePos.y;
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
    case 4:
        int missing;
        fopAc_ac_c* actor = searchByID(mPartnerID, &missing);
        if (actor != NULL && !missing) {
            mLookPos = actor->current.pos;
            mLookPos.y = actor->eyePos.y;
            target = mLookPos;
            targetP = &target;
        }
        break;
    }
    m_jnt.lookAtTarget_2(&current.angle.y, targetP, source, angle, l_HIO.mPrm.mTurnSpeed, noTurn);
}

/* 000013BC-0000143C       .text chkAttention__11daNpc_Gk1_cFv */
bool daNpc_Gk1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 0000143C-00001494       .text setAttention__11daNpc_Gk1_cFb */
void daNpc_Gk1_c::setAttention(bool force) {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionYOffset, current.pos.z);
    if (mUpdateEye || force) {
        eyePos.set(mEyeLocal.x, mEyeLocal.y, mEyeLocal.z);
    }
}

/* 00001494-0000151C       .text decideType__11daNpc_Gk1_cFi */
bool daNpc_Gk1_c::decideType(int) {
    if (mType > 0) {
        return true;
    }
    mType = 1;
    mSubtype = 0;
    strcpy(mArcName, "Gk");
    return mType != -1 && (s8)mSubtype != -1;
}

/* 0000151C-000015BC       .text privateCut__11daNpc_Gk1_cFi */
void daNpc_Gk1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"dummy"};
    if (staff != -1) {
        mCutIndex = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, 0);
        if (mCutIndex == -1) {
            dComIfGp_evmng_cutEnd(staff);
        } else {
            if (dComIfGp_evmng_getIsAddvance(staff)) {
                switch (mCutIndex) {
                case 0:
                    break;
                }
            }
            bool result;
            switch (mCutIndex) {
            case 0:
            default:
                result = true;
                break;
            }
            if (result) {
                dComIfGp_evmng_cutEnd(staff);
            }
        }
    }
}

/* 000015BC-000015E0       .text endEvent__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
}

/* 000015E0-00001618       .text isEventEntry__11daNpc_Gk1_cFv */
int daNpc_Gk1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName());
}

/* 00001618-0000169C       .text event_proc__11daNpc_Gk1_cFi */
void daNpc_Gk1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIds[mEventIndex])) {
        endEvent();
    } else if (!mEventCut.cutProc()) {
        privateCut(staff);
    }
}

/* 0000169C-00001748       .text set_action__11daNpc_Gk1_cFM11daNpc_Gk1_cFPCvPvPv_iPv */
bool daNpc_Gk1_c::set_action(int (daNpc_Gk1_c::*action)(void*), void* arg) {
    if (mAction != action) {
        if (mAction != NULL) {
            mActionState = 9;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

/* 00001748-000017C8       .text setStt__11daNpc_Gk1_cFSc */
void daNpc_Gk1_c::setStt(s8 state) {
    s8 previous = mState;
    mState = state;
    switch (mState) {
    case 0:
        break;
    case 1:
        mEventOrder = 0;
        speedF = 0.0f;
        break;
    case 2:
        mEventOrder = 0;
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        mMessageState = 0;
        mPreviousState = previous;
        break;
    }
    setAnm();
}

/* 000017C8-000019E0       .text chk_attn__11daNpc_Gk1_cFv */
bool daNpc_Gk1_c::chk_attn() {
    f32 distance = (current.pos - dComIfGp_getPlayer(0)->current.pos).absXZ();
    f32 height = current.pos.y - dComIfGp_getPlayer(0)->current.pos.y;
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getPlayer(0)->current.pos) - current.angle.y;
    if (mLookMode == 1) {
        return distance < 200.0f && abs(angle) / 182.04445f < 90.0f && std::fabsf(height) < 300.0f;
    }
    return distance < 200.0f && abs(angle) / 182.04445f < 60.0f && std::fabsf(height) < 300.0f;
}

/* 000019E0-00001B74       .text wait_1__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::wait_1() {
    f32 distance = (current.pos - dComIfGp_getPlayer(0)->current.pos).absXZ();
    mReturnAngle = distance > 300.0f;
    if (mReturnAngle) {
        cLib_addCalcAngleS(&current.angle.y, mInitialAngle.y, 4, 0x800, 0x80);
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
    mEventOrder = 2;
    mNoTurn = true;
    if (chk_attn()) {
        mLookMode = 1;
        return TRUE;
    }
    mLookMode = 0;
    return TRUE;
}

/* 00001B74-00001C74       .text talk_1__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::talk_1() {
    BOOL result = chk_parts_notMov();
    talk(1);
    if (mpCurrMsg == NULL) {
        return TRUE;
    }
    switch (mpCurrMsg->mStatus) {
    case 0x13:
        switch (mCurrMsgNo) {
        case 0x28A5:
            dComIfGs_onEventBit(0x1680);
            break;
        case 0x28AA:
            dComIfGs_onEventBit(0x1640);
            break;
        }
        mPresentItem = 0xFF;
        mTalking = false;
        setStt(mPreviousState);
        mWaitTimer = cLib_getRndValue<int>(15, 30);
        endEvent();
        break;
    case 2:
    case 6:
        break;
    }
    return result;
}

/* 00001C74-00001D30       .text wait_action1__11daNpc_Gk1_cFPv */
int daNpc_Gk1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        setStt(1);
        mReturnAngle = true;
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        mHasAttention = chkAttention();
        switch (mState) {
        case 1:
            mUpdateEye = wait_1();
            break;
        case 2:
            mUpdateEye = talk_1();
            break;
        }
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00001D30-00001E70       .text demo__11daNpc_Gk1_cFv */
u8 daNpc_Gk1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) {
            mDemo = 0;
        }
    } else {
        if (!mDemo) {
            mDemo = 1;
            mFreezeAngle = 0;
            m_jnt.setHead_y(0);
            m_jnt.setHead_x(0);
            m_jnt.setBackBone_y(0);
            m_jnt.setBackBone_x(0);
        }
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (mBtp.getBtpAnm() != NULL) {
            u8 max = mBtp.getBtpAnm()->getFrameMax();
            mBtpFrame++;
            if (mBtpFrame >= max) {
                mBtpFrame = max;
            }
        }
        J3DAnmTexPattern* pattern = actor->getP_BtpData(mArcName);
        if (pattern != NULL) {
            mBtp.init(mpMorf->getModel()->getModelData(), pattern, TRUE, 0, 1.0f, 0, -1, true, FALSE);
            mTexIndex = 1;
            mBtpFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, mArcName);
    }
    return mDemo;
}

/* 00001E70-00001F48       .text shadowDraw__11daNpc_Gk1_cFv */
void daNpc_Gk1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId != 0) {
        if (mpItemModel != NULL) {
            dComIfGd_addRealShadow(mShadowId, mpItemModel);
        }
        if (mpHatModel != NULL) {
            dComIfGd_addRealShadow(mShadowId, mpHatModel);
        }
    }
}
/* 00001F48-00002064       .text _draw__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor red = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue = {0x00, 0x00, 0xFF, 0x80};
    GXColor yellow = {0xFF, 0xFF, 0x00, 0x80};
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    if (mHidden || mNoDraw) {
        return TRUE;
    }
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtp.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtp.remove(data);
    if (mpItemModel != NULL) {
        g_env_light.setLightTevColorType(mpItemModel, &tevStr);
        mDoExt_modelEntryDL(mpItemModel);
    }
    if (mpHatModel != NULL) {
        g_env_light.setLightTevColorType(mpHatModel, &tevStr);
        mDoExt_modelEntryDL(mpHatModel);
    }
    shadowDraw();
    dSnap_RegistFig(DSNAP_TYPE_UNK59, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 00002064-00002280       .text _execute__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::_execute() {
    if (!mInitialSet) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mInitialSet = 1;
    }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackX, l_HIO.mPrm.mMaxBackY, l_HIO.mPrm.mMinBackX, l_HIO.mPrm.mMinBackY, l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY, l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurn);
    if (mHidden && demoActorID == 0) {
        return TRUE;
    }
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == FALSE) {
            staff = isEventEntry();
        }
        if (staff >= 0) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        lookBack();
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        mObjAcch.CrrPos(*dComIfG_Bgsp());
        play_animation();
    } else mHidden = 0;
    eventOrder();
    mModelAngle = current.angle;
    if (!mFreezeAngle) {
        shape_angle = current.angle;
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    setMtx(false);
    if (!mDemo) {
        setCollision(90.0f, 200.0f);
    }
    return TRUE;
}

/* 00002280-000022D4       .text _delete__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::_delete() {
#if VERSION == VERSION_DEMO
    if (mLoaded) {
        l_HIO.removeHIO();
        dComIfG_resDelete(&mPhase, mArcName);
        if (mpMorf != NULL) {
            mpMorf->stopZelAnime();
        }
    }
#else
    dComIfG_resDelete(&mPhase, mArcName);
    if (heap != NULL && mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return TRUE;
}

/* 000022D4-00002520       .text _create__11daNpc_Gk1_cFv */
cPhs_State daNpc_Gk1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Gk1_c);
#endif
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) {
        return cPhs_ERROR_e;
    }
    cPhs_State phase = dComIfG_resLoad(&mPhase, mArcName);
    mLoaded = phase == cPhs_COMPLEATE_e;
    if (!mLoaded) {
        return phase;
    }
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("貧乏ム−ルの父");
    fopAcM_SetupActor(this, daNpc_Gk1_c);
#endif
    static u32 a_siz_tbl[] = {0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) {
#if VERSION == VERSION_DEMO
        mLoaded = false;
#endif
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -90.0f, -20.0f, -80.0f, 90.0f, 200.0f, 80.0f);
    if (createInit()) {
        return phase;
    }
    return cPhs_ERROR_e;
}

/* 000027EC-00002AE8       .text bodyCreateHeap__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_GK_BDL_GK_e);
    JUT_ASSERT(DEMO_SELECT(1548, 1553), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, NULL, -1, 1.0f, 0, -1, TRUE, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL) {
        return FALSE;
    }
    if (mpMorf->getModel() == NULL) {
        mpMorf = NULL;
        return FALSE;
    }
    if (!init_texPttrnAnm(0, false)) {
        mpMorf = NULL;
        return FALSE;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(1568, 1573), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(1570, 1575), m_bbone_jnt_num >= 0);
    m_nck_jnt_num = a_mdl_dat->getJointName()->getIndex("neck");
    JUT_ASSERT(DEMO_SELECT(1572, 1577), m_nck_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_nck_jnt_num)->setCallBack(nodeCB_Neck);
    mpMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}

/* 00002AE8-00002BA8       .text itemCreateHeap__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::itemCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_GK_BDL_GK_HAIR_e);
    JUT_ASSERT(DEMO_SELECT(1593, 1598), a_mdl_dat != 0);
    mpItemModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
    if (mpItemModel == NULL) {
        return FALSE;
    }
    return TRUE;
}

/* 00002BA8-00002C68       .text hat_CreateHeap__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::hat_CreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_GK_BDL_GK_HAT_e);
    JUT_ASSERT(DEMO_SELECT(1609, 1614), a_mdl_dat != 0);
    mpHatModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
    if (mpHatModel == NULL) {
        return FALSE;
    }
    return TRUE;
}

/* 00002C68-00002D2C       .text CreateHeap__11daNpc_Gk1_cFv */
BOOL daNpc_Gk1_c::CreateHeap() {
    if (!bodyCreateHeap()) {
        return FALSE;
    }
    if (!itemCreateHeap()) {
        mpMorf = NULL;
        return FALSE;
    }
    if (!hat_CreateHeap()) {
        mpMorf = NULL;
        return FALSE;
    }
    mAcchCir.SetWall(30.0f, 90.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return TRUE;
}

/* 00002D2C-00002D4C       .text daNpc_Gk1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Gk1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Gk1_c*)i_this)->_create();
}

/* 00002D4C-00002D6C       .text daNpc_Gk1_Delete__FP11daNpc_Gk1_c */
static BOOL daNpc_Gk1_Delete(daNpc_Gk1_c* i_this) {
    return ((daNpc_Gk1_c*)i_this)->_delete();
}

/* 00002D6C-00002D8C       .text daNpc_Gk1_Execute__FP11daNpc_Gk1_c */
static BOOL daNpc_Gk1_Execute(daNpc_Gk1_c* i_this) {
    return ((daNpc_Gk1_c*)i_this)->_execute();
}

/* 00002D8C-00002DAC       .text daNpc_Gk1_Draw__FP11daNpc_Gk1_c */
static BOOL daNpc_Gk1_Draw(daNpc_Gk1_c* i_this) {
    return ((daNpc_Gk1_c*)i_this)->_draw();
}

/* 00002DAC-00002DB4       .text daNpc_Gk1_IsDelete__FP11daNpc_Gk1_c */
static BOOL daNpc_Gk1_IsDelete(daNpc_Gk1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Gk1_Method = {
    (process_method_func)daNpc_Gk1_Create,
    (process_method_func)daNpc_Gk1_Delete,
    (process_method_func)daNpc_Gk1_Execute,
    (process_method_func)daNpc_Gk1_IsDelete,
    (process_method_func)daNpc_Gk1_Draw,
};
actor_process_profile_definition g_profile_NPC_GK1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_GK1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Gk1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_GK1_e,
    /* Actor SubMtd */ &l_daNpc_Gk1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
