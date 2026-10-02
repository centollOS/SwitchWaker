/**
 * d_a_npc_kf1.cpp
 * NPC - Mila's Father (rich)
 */
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_kf1.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_snap.h"
#include "d/d_a_obj.h"
#include "d/actor/d_a_tsubo.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Kf.h"
#include "SSystem/SComponent/c_counter.h"

static daNpc_Kf1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;

/* 000000EC-00000150       .text __ct__15daNpc_Kf1_HIO_cFv */
daNpc_Kf1_HIO_c::daNpc_Kf1_HIO_c() {
    static hio_prm_c a_prm_tbl = {0x2000, 2000, -1000, -2000, 0, 5000, -4000, -5000, 1500, 1200, 200.0f, 0, 10, 0x400, 0.5f, 2.0f, 0.2f, 8.0f};
    memcpy(&mPrm, &a_prm_tbl, sizeof(mPrm));
    mNo = -1;
    mCount = -1;
}
daNpc_Kf1_HIO_c::~daNpc_Kf1_HIO_c() {
}
static char* l_evn_tbl[] = {"angry", "rupee_age", "bensyou"};

/* 00000198-000001E4       .text nodeCB_Head__FP7J3DNodei */
static BOOL nodeCB_Head(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Kf1_c* npc = (daNpc_Kf1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_Head(node, model);
        }
    }
    return TRUE;
}

/* 000001E4-000002D8       .text _nodeCB_Head__11daNpc_Kf1_cFP7J3DNodeP8J3DModel */
void daNpc_Kf1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(30.0f, 30.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mHeadPos.x = mDoMtx_stack_c::get()[0][3];
    mHeadPos.y = mDoMtx_stack_c::get()[1][3];
    mHeadPos.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyeLocal);
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000314-00000360       .text nodeCB_Neck__FP7J3DNodei */
static BOOL nodeCB_Neck(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Kf1_c* npc = (daNpc_Kf1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_Neck(node, model);
        }
    }
    return TRUE;
}

/* 00000360-00000400       .text _nodeCB_Neck__11daNpc_Kf1_cFP7J3DNodeP8J3DModel */
void daNpc_Kf1_c::_nodeCB_Neck(J3DNode* node, J3DModel* model) {
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
        daNpc_Kf1_c* npc = (daNpc_Kf1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_BackBone(node, model);
        }
    }
    return TRUE;
}

/* 0000044C-000004EC       .text _nodeCB_BackBone__11daNpc_Kf1_cFP7J3DNodeP8J3DModel */
void daNpc_Kf1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 000004EC-0000050C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Kf1_c*)actor)->CreateHeap();
}

/* 0000050C-0000059C       .text searchActor_Tsubo__FPvPv */
void* searchActor_Tsubo(void* actor, void*) {
    if (l_check_wrk < 20 && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_TSUBO_e && ((daTsubo::Act_c*)actor)->prm_get_type() == 14) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

/* 0000059C-0000061C       .text init_KF1_0__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::init_KF1_0() {
    if (!dComIfGs_isEventBit(0x2D01)) {
        set_action(&daNpc_Kf1_c::wait_action1, NULL);
        return true;
    }
    return false;
}

/* 0000061C-0000083C       .text createInit__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::createInit() {
    for (int i = 0; i < 3; i++) {
        mEventIds[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    }
    mEventCut.setActorInfo2("Kf1", this);
    mSwitch = (fopAcM_GetParam(this) >> 8) & 0xFF;
    int weight = 0xFF;
    u8 path = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (path != 0xFF) {
        mPath.setInf(path, fopAcM_GetRoomNo(this), 1);
        if (mPath.isPath()) {
            fopAcM_OffStatus(this, 0x80);
            weight = 0xD9;
            set_pthPoint(0);
        } else {
            return false;
        }
    }
    if (mPath.isPath() == false) {
        return false;
    }
    attention_info.flags = 0xA;
    switch (mSubtype) {
    case 0:
    default:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAB;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAB;
        break;
    }
    gravity = -4.5f;
    mAnmIndex = 10;
    bool result;
    switch (mSubtype) {
    case 0:
        result = init_KF1_0();
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
    mStts.Init(weight, 0xFF, this);
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

/* 0000083C-000008E4       .text play_animation__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::play_animation() {
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

/* 000008E4-00000A20       .text setMtx__11daNpc_Kf1_cFb */
void daNpc_Kf1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(mModelAngle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    if (mpItemModel != NULL) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
        mDoMtx_stack_c::transM(33.87f, 3.26f, 0.0f);
        mDoMtx_stack_c::XYZrotM(-0x4000, -0x4000, 0);
        mpItemModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mpItemModel->calc();
    }
    setAttention(force);
}

/* 00000A20-00000A34       .text bckResID__11daNpc_Kf1_cFi */
int daNpc_Kf1_c::bckResID(int index) {
    static const int a_resID_tbl[] = {dRes_ID_KF_BCK_KF_WAIT01_e, dRes_ID_KF_BCK_KF_WALK_e, dRes_ID_KF_BCK_KF_TALK01_e, dRes_ID_KF_BCK_KF_TALK02_e, dRes_ID_KF_BCK_KF_TALK03_e, dRes_ID_KF_BCK_KF_TALK04_e, dRes_ID_KF_BCK_KF_OKORU_e, dRes_ID_KF_BCK_KF_BAKUHATU_e, dRes_ID_KF_BCK_KF_BARAMAKU_e, dRes_ID_KF_BCK_KF_WAIT02_e}; return a_resID_tbl[index];
}

/* 00000A34-00000A48       .text btpResID__11daNpc_Kf1_cFi */
int daNpc_Kf1_c::btpResID(int index) {
    static const int a_resID_tbl[] = {
        dRes_ID_KF_BTP_KF_e, dRes_ID_KF_BTP_CLOSE_e
    };
    return a_resID_tbl[index];
}

/* 00000A48-00000B4C       .text setBtp__11daNpc_Kf1_cFScb */
bool daNpc_Kf1_c::setBtp(s8 index, bool modify) {
    J3DModel* model = mpMorf->getModel();
    if (index < 0) {
        return false;
    }
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes(mArcName, btpResID(index));
    JUT_ASSERT(522, a_btp != 0);
    mTexIndex = index;
    mBtpFrame = 0;
    mBlinkTimer = 0;
    return mBtp.init(model->getModelData(), a_btp, TRUE, 0, 1.0f, 0, -1, modify, FALSE) != FALSE;
}

/* 00000B4C-00000B6C       .text init_texPttrnAnm__11daNpc_Kf1_cFScb */
bool daNpc_Kf1_c::init_texPttrnAnm(s8 index, bool modify) {
    return setBtp(index, modify);
}

/* 00000B6C-00000C08       .text play_btp_anm__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::play_btp_anm() {
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

/* 00000C08-00000CA4       .text setAnm_anm__11daNpc_Kf1_cFPQ211daNpc_Kf1_c9anm_prm_c */
void daNpc_Kf1_c::setAnm_anm(anm_prm_c* prm) {
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

/* 00000CA4-00000D14       .text setAnm_NUM__11daNpc_Kf1_cFii */
void daNpc_Kf1_c::setAnm_NUM(int index, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 15.0f, 1.0f, 2}, {1, 1, 8.0f, 1.0f, 2}, {2, 0, 8.0f, 1.0f, 2}, {3, 0, 8.0f, 1.0f, 2}, {4, 0, 8.0f, 1.0f, 2}, {5, 0, 8.0f, 1.0f, 2}, {6, 0, 8.0f, 1.0f, 2}, {7, 0, 8.0f, 1.0f, 0}, {8, 0, 8.0f, 1.0f, 0}, {9, 1, 8.0f, 1.0f, 2}
    };
    if (tex) {
        init_texPttrnAnm(a_anm_prm_tbl[index].mTex, true);
    }
    setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 00000D14-00000D80       .text setAnm__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0, 0, -1}, {-1, -1, 0, 0, -1}, {-1, -1, 0, 0, -1}, {1, 1, 8, 1, 2}
    };
    init_texPttrnAnm(a_anm_prm_tbl[mState].mTex, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

/* 00000D80-00000D84       .text chngAnmTag__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::chngAnmTag() {
}

/* 00000D84-00000D88       .text ctrlAnmTag__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::ctrlAnmTag() {
}

/* 00000D88-00000DEC       .text chngAnmAtr__11daNpc_Kf1_cFUc */
void daNpc_Kf1_c::chngAnmAtr(u8 attr) {
    switch (mCurrMsgNo) {
    case 0x1C2E:
        dComIfGp_event_offHindFlag(0x80);
        break;
    }
    if (attr == mAnmAttr || attr>11) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR();
}

/* 00000DEC-00000E60       .text ctrlAnmAtr__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 8: case 10:
        break;
    case 7:
    case 9:
        if (mAnmEnded) {
            setAnm_NUM(0, 1);
            mAnmAttr = 0;
        }
        break;
    }
}

/* 00000E60-00000EC4       .text setAnm_ATR__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 15.0f, 1.0f, 2}, {1, 1, 8.0f, 1.0f, 2}, {2, 0, 8.0f, 1.0f, 2}, {3, 0, 8.0f, 1.0f, 2}, {4, 0, 8.0f, 1.0f, 2}, {5, 0, 8.0f, 1.0f, 2}, {6, 0, 8.0f, 1.0f, 2}, {7, 0, 8.0f, 1.0f, 0}, {8, 0, 8.0f, 1.0f, 0}, {3, 0, 8, 1, 2}, {9, 1, 8, 1, 2}
    };
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mTex, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00000EC4-00000F80       .text anmAtr__11daNpc_Kf1_cFUs */
void daNpc_Kf1_c::anmAtr(u16 status) {
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

/* 00000F80-000010F4       .text next_msgStatus__11daNpc_Kf1_cFPUl */
u16 daNpc_Kf1_c::next_msgStatus(u32* msg) {
    u16 result = 15;
    switch (*msg) {
    case 0x1C23:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *msg = 0x1C24;
            break;
        case 1:
            *msg = 0x1C25;
            break;
        }
        break;
    case 0x1C28:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *msg = 0x1C2A;
            break;
        case 1:
            *msg = 0x1C29;
            break;
        }
        break;
    case 0x1C24:
        dComIfGs_onEventBit(0xB02);
        result = 16;
        break;
    case 0x1C2B:
        mOfferAccepted = 1;
        result = 16;
        break;
    case 0x1C21:
        *msg = 0x1C22;
        break;
    case 0x1C22:
        *msg = 0x1C34;
        break;
    case 0x1C34:
        *msg = 0x1C23;
        break;
    case 0x1C27:
        *msg = 0x1C28;
        break;
    case 0x1C2A:
        *msg = 0x1C2B;
        break;
    case 0x1C30:
        *msg = 0x1C31;
        break;
    case 0x1C2D:
        *msg = 0x1C2E;
        break;
    case 0x1C33:
        *msg = 0x1C30 + (mSavedRupees < mBrokenPots * 10 ? 0 : -1);
        break;
    case 0x1C36:
        *msg = 0x1C37;
        break;
    default:
        result = 16;
        break;
    }
    return result;
}

/* 000010F4-000011DC       .text getMsg_KF1_0__11daNpc_Kf1_cFv */
u32 daNpc_Kf1_c::getMsg_KF1_0() {
    if (mGaveRupees) {
        return 0x1C38;
    }
    if (dComIfGs_isEventBit(0xA02) && !dComIfGs_isSymbol(0)) {
        return 0x1C3B;
    }
    if (dComIfGs_isEventBit(0xB02)) {
        u8 flags = dComIfGs_getEventReg(0xBCFF);
        if (dKy_daynight_check() == 1 || !dComIfGs_isEventBit(0x2780) || flags&1) {
            return 0x1C26;
        }
        return 0x1C27;
    }
    return 0x1C21;
}

/* 000011DC-00001218       .text getMsg__11daNpc_Kf1_cFv */
u32 daNpc_Kf1_c::getMsg() {
    u32 result = 0;
    switch (mSubtype) {
    case 0:
        result = getMsg_KF1_0();
        break;
    }
    return result;
}

/* 00001218-000012A4       .text eventOrder__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::eventOrder() {
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

/* 000012A4-00001380       .text checkOrder__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEventIds[mEventIndex]) && mEventOrder >= 3) {
            switch (mEventIndex) {
            case 0:
                ((daPy_py_c*)dComIfGp_getPlayer(0))->changeDemoMoveAngle(dComIfGp_getPlayer(0)->current.angle.y);
                break;
            case 1:
            case 2:
                break;
            }
            mEventOrder = 0;
            mAnmAttr = 0xFF;
            mAnmTag = 0xFF;
        }
    } else if (eventInfo.checkCommandTalk() && (mEventOrder == 1 || mEventOrder == 2)) {
        mEventOrder = 0;
        mTalking = true;
    }
}

/* 00001380-00001418       .text chk_talk__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::chk_talk() {
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

/* 00001418-0000146C       .text searchByID__11daNpc_Kf1_cFUiPi */
fopAc_ac_c* daNpc_Kf1_c::searchByID(fpc_ProcID id, int* missing) {
    fopAc_ac_c* actor = NULL;
    *missing = 0;
    if (!fopAcM_SearchByID(id, &actor)) {
        *missing = 1;
    }
    return actor;
}

/* 0000146C-0000156C       .text srch_Tsubo__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::srch_Tsubo() {
    BOOL result = FALSE;
    if (mActionState != 1) {
        return result;
    }
    mPartnerID = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < 20; i++) {
        l_check_inf[i] = NULL;
    }
    fpcEx_Search(searchActor_Tsubo, this);
    if (l_check_wrk >= 8) {
        mPotCount = 0;
        for (int i = 0; i < 8; i++) {
            mPotIDs[i] = fopAcM_GetID(l_check_inf[i]);
            mPotCount++;
        }
        result = TRUE;
        mActionState++;
    }
    return result;
}

/* 0000156C-000017F4       .text create_rupee__11daNpc_Kf1_cF4cXyzi */
void daNpc_Kf1_c::create_rupee(cXyz pos, int count) {
    cXyz itemScale(0.2f, 0.2f, 0.2f);
    csXyz angle(0, 0, 0);
    fopAc_ac_c* a_actor_p;
    s8 room;
    int i;
    int counter;
    f32 random;
    counter = g_Counter.mCounter0;
    for (i = 0; i < count; i++, counter++) {
        f32 offsets[] = {-30.0f, 0.0f, 30.0f};
        random = offsets[counter % 3] + (cM_rndF(30.0f) - 15.0f);
        s16 scaled = random;
        scaled = scaled * (0x7FFF / 180.0f);
        angle.y = current.angle.y + scaled;
        room = current.roomNo;
        a_actor_p = fopAcM_createItemForKP2(&pos, 4, room, NULL, NULL, 13.0f + cM_rndFX(2.0f), 31.0f + cM_rndFX(4.0f), -2.0f, 1);
        JUT_ASSERT(1042, a_actor_p != 0);
        if (a_actor_p) {
            fopAcM_OnStatus(a_actor_p, 0x4000);
            fopAcM_OffStatus(a_actor_p, 0x80);
            a_actor_p->scale = itemScale;
            a_actor_p->shape_angle = angle;
            a_actor_p->current.angle = a_actor_p->shape_angle;
            mRupeeIDs[i] = fopAcM_GetID(a_actor_p);
        }
    }
}

/* 000017F4-00001A0C       .text ready_kutaniCamera__11daNpc_Kf1_cFii */
void daNpc_Kf1_c::ready_kutaniCamera(int index, int create) {
    int i_flg;
    fopAc_ac_c* a_actor = searchByID(mDummyRupeeID, &i_flg);
    if (a_actor && !i_flg) {
        fopAcM_delete(a_actor);
    }
    if (create) {
        cXyz itemScale(0.1f, 0.1f, 0.1f);
        csXyz angle(0, 0, 0);
        a_actor = searchByID(mAttentionPotIDs[index], &i_flg);
        JUT_ASSERT(1074, 0 != a_actor && 0 == i_flg);
        dComIfGp_event_setItemPartner(a_actor);
        angle.y = a_actor->current.angle.y;
        cXyz pos = a_actor->current.pos;
        pos.y = 180.0f+pos.y;
        a_actor = fopAcM_createItemForKP2(&pos, 4, fopAcM_GetRoomNo(this), NULL, NULL, 0.0f, 0.0f, -4.0f, 1);
        JUT_ASSERT(1086, 0 != a_actor);
        fopAcM_OnStatus(a_actor, 0x4000);
        fopAcM_OffStatus(a_actor, 0x80);
        a_actor->scale = itemScale;
        a_actor->current.angle = angle;
        mDummyRupeeID = fopAcM_GetID(a_actor);
    }
}

/* 00001A0C-00001BD0       .text lookBack__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::lookBack() {
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
        fopAc_ac_c* actor = searchByID(mAttentionActorID, &missing);
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

/* 00001BD0-00001C50       .text chkAttention__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 00001C50-00001CA8       .text setAttention__11daNpc_Kf1_cFb */
void daNpc_Kf1_c::setAttention(bool force) {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionYOffset, current.pos.z);
    if (mUpdateEye || force) {
        eyePos.set(mEyeLocal.x, mEyeLocal.y, mEyeLocal.z);
    }
}

/* 00001CA8-00001D30       .text decideType__11daNpc_Kf1_cFi */
bool daNpc_Kf1_c::decideType(int) {
    if (mType > 0) {
        return true;
    }
    mType = 1;
    mSubtype = 0;
    strcpy(mArcName, "Kf");
    return mType != -1 && (s8)mSubtype != -1;
}

/* 00001D30-00001E04       .text cut_init_ANGRY_START__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_ANGRY_START(int staff) {
    cXyz* pos = dComIfGp_evmng_getMyXyzP(staff, "Pos");
    if (pos) {
        current.pos.set(pos->x, pos->y, pos->z);
        mObjAcch.SetOld();
    }
    mLookPos.set(0, eyePos.y, 0);
    mLookMode = 2;
    current.angle.y = cLib_targetAngleY(&current.pos, &mLookPos);
    speedF = 0;
    setAnm_NUM(0, 1);
}

/* 00001E04-00001E0C       .text cut_move_ANGRY_START__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_ANGRY_START() {
    return true;
}

/* 00001E0C-00001E5C       .text cut_init_BENSYOU_START__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_BENSYOU_START(int staff) {
    cut_init_ANGRY_START(staff);
    cXyz goal(0, 0, 700);
    dComIfGp_evmng_setGoal(&goal);
}

/* 00001E5C-00001E64       .text cut_move_BENSYOU_START__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_BENSYOU_START() {
    return true;
}

/* 00001E64-00001EB4       .text cut_init_TSUBO_CNT__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_TSUBO_CNT(int) {
    mBrokenPots = mPotCount-chk_tsubo();
    m_jnt.setBackBone_x(0);
    m_jnt.setBackBone_y(0);
    m_jnt.setHead_x(0);
    m_jnt.setHead_y(0);
    mLookMode = 0;
}

/* 00001EB4-00001EBC       .text cut_move_TSUBO_CNT__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_TSUBO_CNT() {
    return true;
}

/* 00001EBC-00001EE8       .text cut_init_BENSYOU__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_BENSYOU(int) {
    mSavedRupees = dComIfGs_getRupee();
    dComIfGp_setItemRupeeCount(-(mBrokenPots*10));
}

/* 00001EE8-00001EF0       .text cut_move_BENSYOU__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_BENSYOU() {
    return true;
}

/* 00001EF0-00001FE4       .text cut_init_GET_OUT__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_GET_OUT(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff,"Timer");
    mCutTimer = 0;
    if (timer) {
        mCutTimer = *timer;
    }
    ((daPy_py_c*)dComIfGp_getPlayer(0))->changeDemoMoveAngle(0);
    ((daPy_py_c*)dComIfGp_getPlayer(0))->changeOriginalDemo();
    if (mSavedRupees >= mBrokenPots*10) {
        cXyz pos(0, 0, 999);
        cLib_targetAngleY(&dComIfGp_getPlayer(0)->current.pos, &pos);
        ((daPy_py_c*)dComIfGp_getPlayer(0))->changeDemoMode(3);
    } else {
        ((daPy_py_c*)dComIfGp_getPlayer(0))->changeDemoParam0(1);
        ((daPy_py_c*)dComIfGp_getPlayer(0))->changeDemoMode(9);
    }
}

/* 00001FE4-00002044       .text cut_move_GET_OUT__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_GET_OUT() {
    if (cLib_calcTimer(&mCutTimer) == 0) {
        dComIfGp_setNextStage("sea", 3, 11, -1, 0, 0, 1, 0);
    }
    return false;
}

/* 00002044-0000205C       .text cut_init_DSP_RUPEE_CNT__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_DSP_RUPEE_CNT(int) {
    dComIfGp_event_offHindFlag(0x80);
}

/* 0000205C-00002064       .text cut_move_DSP_RUPEE_CNT__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_DSP_RUPEE_CNT() {
    return true;
}

/* 00002064-000020AC       .text cut_init_PLYER_TRN__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_PLYER_TRN(int) {
    ((daPy_py_c*)dComIfGp_getPlayer(0))->changeDemoMoveAngle(cLib_targetAngleY(&dComIfGp_getPlayer(0)->current.pos, &current.pos));
}

/* 000020AC-000020B4       .text cut_move_PLYER_TRN__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_PLYER_TRN() {
    return true;
}

/* 000020B4-000020B8       .text cut_init_RUPEE_CNT_END__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_RUPEE_CNT_END(int) {
}

/* 000020B8-000020EC       .text cut_move_RUPEE_CNT_END__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_RUPEE_CNT_END() {
    if ((u32)dComIfGs_getRupee() == dComIfGp_getItemNowRupee()) {
        dComIfGp_event_onHindFlag(0x80);
        return true;
    }
    return false;
}

/* 000020EC-00002168       .text cut_init_START_AGE__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_START_AGE(int) {
    mLookPos.set(0, eyePos.y, 0);
    shape_angle.y = cLib_targetAngleY(&current.pos, &mLookPos);
    mFreezeAngle = 1;
    mLookMode = 3;
    mLookAngle = shape_angle.y;
    m_jnt.setTrn();
    setAnm_NUM(0, 1);
}

/* 00002168-00002178       .text cut_move_START_AGE__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_START_AGE() {
    return !m_jnt.trnChk();
}

/* 00002178-00002284       .text cut_init_PLYER_MOV__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_PLYER_MOV(int) {
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getPlayer(0)->current.pos) - current.angle.y;
    if (abs(angle) > 0x2000) {
        dComIfGp_evmng_setGoal(&dComIfGp_getPlayer(0)->current.pos);
    } else {
        cXyz pos, offset(0, 0, 0);
        angle = angle > 0 ? 0x2800 : -0x2800;
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y + angle);
        offset.z = 150;
        mDoMtx_stack_c::multVec(&offset, &pos);
        dComIfGp_evmng_setGoal(&pos);
    }
}

/* 00002284-0000228C       .text cut_move_PLYER_MOV__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_PLYER_MOV() {
    return true;
}

/* 0000228C-000023E8       .text cut_init_RUPEE_SET__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_RUPEE_SET(int) {
#if VERSION == VERSION_DEMO
    fopAc_ac_c* a_tsubo_actor;
    int index;
    int i;
    if (mPotCount == 8) {
        u8 selected[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (i = 0; i < 3; i++) {
            do {
                index = cM_rndF(32.0f) / 4.0f;
            } while (selected[index]);
            a_tsubo_actor = fopAcM_SearchByID(mPotIDs[index]);
            JUT_ASSERT(1475, 0 != a_tsubo_actor);
            ((daTsubo::Act_c*)a_tsubo_actor)->prm_set_itemNo(dItemNo_RED_RUPEE_e);
            dComIfGp_event_setItemPartner(a_tsubo_actor);
            mAttentionPotIDs[i] = mPotIDs[index];
            selected[index] = 1;
        }
    }
#else
    if (mPotCount == 8) {
        u8 selected[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        int index;
        for (int i = 0; i < 3; i++) {
            do {
                index = cM_rndF(32.0f) / 4.0f;
            } while (selected[index]);
            fopAc_ac_c* a_tsubo_actor = fopAcM_SearchByID(mPotIDs[index]);
            JUT_ASSERT(1475, 0 != a_tsubo_actor);
            fopAcM_SetParam(a_tsubo_actor, (fopAcM_GetParam(a_tsubo_actor) & ~0x3F) | 4);
            dComIfGp_event_setItemPartner(a_tsubo_actor);
            mAttentionPotIDs[i] = mPotIDs[index];
            selected[index] = 1;
        }
    }
#endif
    setAnm_NUM(8, 1);
}

/* 000023E8-0000260C       .text cut_move_RUPEE_SET__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_RUPEE_SET() {
    int i;
    int count = 0, missing;
    if (!mAnmEnded) {
        if (mpMorf->checkFrame(68.0f)) {
            cXyz offset(0, 40, 40), pos;
            mDoAud_seStart(JA_SE_OBJ_LUPY_OUT, NULL, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::YrotM(current.angle.y);
            mDoMtx_stack_c::multVec(&offset, &pos);
            create_rupee(pos, 3);
        } else if (68.0f<mpMorf->getFrame()) {
            for (int i = 0; i < 3; i++) {
                fopAc_ac_c* actor = searchByID(mRupeeIDs[i], &missing);
                if (actor && !missing) {
                    eyePos = actor->current.pos;
                }
            }
            mUpdateEye = 0;
        }
        return false;
    }
    for (i = 0; i < 3; i++) {
        fopAc_ac_c* actor = searchByID(mRupeeIDs[i], &missing);
        if (actor) {
            fopAcM_delete(actor);
            count++;
        }
        else if (missing)count++;
    }
    if (count == i) {
        ready_kutaniCamera(0, 1);
        mUpdateEye = 1;
        setAnm_NUM(0, 1);
        return true;
    }
    return false;
}

/* 0000260C-000026B4       .text cut_init_TSUBO_ATN__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_TSUBO_ATN(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff,"Timer");
    int* count = dComIfGp_evmng_getMyIntegerP(staff,"Count");
    mCutTimer = 0;
    if (timer) {
        mCutTimer = *timer;
    }
    mPotCameraIndex = 0;
    if (count) {
        mPotCameraIndex = *count;
    }
}

/* 000026B4-00002794       .text cut_move_TSUBO_ATN__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_TSUBO_ATN() {
    if (cLib_calcTimer(&mCutTimer) == 0) {
        if (mPotCameraIndex >= 0&&mPotCameraIndex <= 3) {
            switch (mPotCameraIndex) {
            case 0:
            case 1:
            case 2:
                ready_kutaniCamera(mPotCameraIndex, 1);
                break;
            case 3:
            default:
                ready_kutaniCamera(0, 0);
                break;
            }
        }
        return true;
    }
    if (mCutTimer == 20)mDoAud_seStart(JA_SE_OBJ_LUPY_IN_POT, NULL, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    return false;
}

/* 00002794-000028F4       .text cut_init_TLK_MSG__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_TLK_MSG(int staff) {
    int* msg = dComIfGp_evmng_getMyIntegerP(staff, "MsgNum");
    int* end = dComIfGp_evmng_getMyIntegerP(staff,"EndMsg");
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
    mMessageState = 0;
    mCurrMsgNo = 0;
    mEndMsgNo = -1;
    if (end) {
        mEndMsgNo = *end;
    }
    if (msg) {
        mCurrMsgNo = *msg;
        switch (mCurrMsgNo) {
        case 0x1C2D:
            dComIfGp_setMessageCountNumber(mBrokenPots*10);
            break;
        case 0x1C2E:
            break;
        case 0x1C2F:
        case 0x1C30:
            mCurrMsgNo = 0x1C30 + (mSavedRupees < mBrokenPots * 10 ? 0 : -1);
            break;
        case 0x1C39:
            dComIfGp_getVibration().StartShock(5, -33, cXyz(0, 1, 0));
            break;
        }
    }
    mCurrMsgBsPcId = fpcM_ERROR_PROCESS_ID_e;
}

/* 000028F4-00002954       .text cut_init_CONTNUE_TLK__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::cut_init_CONTNUE_TLK(int staff) {
    int* end = dComIfGp_evmng_getMyIntegerP(staff,"EndMsg");
    mEndMsgNo = -1;
    if (end) {
        mEndMsgNo = *end;
    }
}

/* 00002954-000029D8       .text cut_move_TLK_MSG__11daNpc_Kf1_cFv */
bool daNpc_Kf1_c::cut_move_TLK_MSG() {
    u16 status = talk(0);
    if (status == 18) {
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        mMessageState = 0;
        return true;
    }
    if (status == 2||status == 6)return mCurrMsgNo == mEndMsgNo;
    return false;
}

/* 000029D8-00002C60       .text privateCut__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {
        "ANGRY_START","BENSYOU_START","TSUBO_CNT","BENSYOU","GET_OUT","DSP_RUPEE_CNT","PLYER_TRN","RUPEE_CNT_END","START_AGE","PLYER_MOV","RUPEE_SET","TSUBO_ATN","TLK_MSG","CONTNUE_TLK"
    };
    if (staff != -1) {
        mCutIndex = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 14, TRUE, 0);
        if (mCutIndex == -1) {
            dComIfGp_evmng_cutEnd(staff);
            return;
        }
        if (dComIfGp_evmng_getIsAddvance(staff)) {
            switch (mCutIndex) {
            case 0:
                cut_init_ANGRY_START(staff);
                break;
            case 1:
                cut_init_BENSYOU_START(staff);
                break;
            case 2:
                cut_init_TSUBO_CNT(staff);
                break;
            case 3:
                cut_init_BENSYOU(staff);
                break;
            case 4:
                cut_init_GET_OUT(staff);
                break;
            case 5:
                cut_init_DSP_RUPEE_CNT(staff);
                break;
            case 6:
                cut_init_PLYER_TRN(staff);
                break;
            case 7:
                cut_init_RUPEE_CNT_END(staff);
                break;
            case 8:
                cut_init_START_AGE(staff);
                break;
            case 9:
                cut_init_PLYER_MOV(staff);
                break;
            case 10:
                cut_init_RUPEE_SET(staff);
                break;
            case 11:
                cut_init_TSUBO_ATN(staff);
                break;
            case 12:
                cut_init_TLK_MSG(staff);
                break;
            case 13:
                cut_init_CONTNUE_TLK(staff);
                break;
            }
        }
        bool result;
        switch (mCutIndex) {
        case 0:
            result = cut_move_ANGRY_START();
            break;
        case 1:
            result = cut_move_BENSYOU_START();
            break;
        case 2:
            result = cut_move_TSUBO_CNT();
            break;
        case 3:
            result = cut_move_BENSYOU();
            break;
        case 4:
            result = cut_move_GET_OUT();
            break;
        case 5:
            result = cut_move_DSP_RUPEE_CNT();
            break;
        case 6:
            result = cut_move_PLYER_TRN();
            break;
        case 7:
            result = cut_move_RUPEE_CNT_END();
            break;
        case 8:
            result = cut_move_START_AGE();
            break;
        case 9:
            result = cut_move_PLYER_MOV();
            break;
        case 10:
            result = cut_move_RUPEE_SET();
            break;
        case 11:
            result = cut_move_TSUBO_ATN();
            break;
        case 12:
            result = cut_move_TLK_MSG();
            break;
        case 13:
            result = cut_move_TLK_MSG();
            break;
        default:
            result = true;
            break;
        }
        if (result)dComIfGp_evmng_cutEnd(staff);
    }
}

/* 00002C60-00002C84       .text endEvent__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
}

/* 00002C84-00002CBC       .text isEventEntry__11daNpc_Kf1_cFv */
int daNpc_Kf1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName());
}

/* 00002CBC-00002E14       .text event_proc__11daNpc_Kf1_cFi */
void daNpc_Kf1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIds[mEventIndex])) {
        switch (mEventIndex) {
        case 0:
            dComIfGs_onEventBit(0x2780);
            mPotCount = chk_tsubo();
            break;
        case 1:
            {
                u8 flags = dComIfGs_getEventReg(0xBCFF);
                flags |= 1;
                dComIfGs_setEventReg(0xBCFF, flags);
            }
            mFreezeAngle = 0;
            setStt(1);
            setAnm_NUM(0, 1);
            mLookTimer = cLib_getRndValue<int>(30, 60);
            mWaitTimer = cLib_getRndValue<int>(15, 30);
            mGaveRupees = 1;
            break;
        case 2:
            break;
        }
        mLookTimer = cLib_getRndValue<int>(30, 60);
        endEvent();
    }
    else if (!mEventCut.cutProc())privateCut(staff);
}

/* 00002E14-00002EC0       .text set_action__11daNpc_Kf1_cFM11daNpc_Kf1_cFPCvPvPv_iPv */
bool daNpc_Kf1_c::set_action(int (daNpc_Kf1_c::*action)(void*), void* arg) {
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

/* 00002EC0-00002F88       .text setStt__11daNpc_Kf1_cFSc */
void daNpc_Kf1_c::setStt(s8 state) {
    s8 previous = mState;
    mState = state;
    switch (mState) {
    case 0:
        break;
    case 1:
        mEventOrder = 0;
        mMoveTimer = cLib_getRndValue<int>(60, 90);
        speedF = 0;
        break;
    case 2:
        mEventOrder = 0;
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        mMessageState = 0;
        mPreviousState = previous;
        break;
    case 3:
        mEventOrder = 0;
        mMoveTimer = cLib_getRndValue<int>(90, 90);
        break;
    }
    setAnm();
}

/* 00002F88-00003030       .text set_pthPoint__11daNpc_Kf1_cFUc */
void daNpc_Kf1_c::set_pthPoint(u8 point) {
    if (mPath.isPath() != false) {
        mPath.setIdx(point);
        current.pos = mPath.getPoint(mPath.getIdx());
        if (mPath.nextIdx()) {
            cXyz pos = mPath.getPoint(mPath.getIdx());
            current.angle.y = cLib_targetAngleY(&current.pos, &pos);
        }
    }
}

/* 00003030-000030A8       .text chk_tsubo__11daNpc_Kf1_cFv */
s16 daNpc_Kf1_c::chk_tsubo() {
    s16 count = 0;
    for (int i = 0; i < 8; i++) {
        int missing;
        searchByID(mPotIDs[i], &missing);
        if (!missing) {
            count++;
        }
    }
    return count;
}

/* 000030A8-0000317C       .text orderTsuboEvent__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::orderTsuboEvent() {
    if (mTalking) {
        return FALSE;
    }
    if (!dComIfGs_isEventBit(0x2780)) {
        if (mPotCount>chk_tsubo()) {
            mEventOrder = 3;
            return TRUE;
        }
    }
    else if (mSwitch != 0xFF && dComIfGs_isSwitch(mSwitch, fopAcM_GetRoomNo(this))) {
        if (mPotCount>chk_tsubo()) {
            mEventOrder = 5;
            return TRUE;
        }
    }
    return FALSE;
}

/* 0000317C-0000327C       .text wait_1__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::wait_1() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
            mLookMode = 1;
            mNoTurn = false;
            m_jnt.setTrn();
        }
        return TRUE;
    }
    if (mEventOrder<3) {
        mEventOrder = 2;
    }
    mNoTurn = true;
    if (mAnmIndex == 9) {
        mLookTimer = 0;
    }
    if (cLib_calcTimer(&mLookTimer) == 0) {
        if (mAnmIndex != 9) {
            setAnm_NUM(9, 1);
            mMoveTimer = 0;
        }
        if (cLib_calcTimer(&mMoveTimer) == 0) {
            setStt(3);
            return TRUE;
        }
    }
    mLookMode = 0;
    return TRUE;
}

/* 0000327C-000034C4       .text walk_1__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::walk_1() {
    if (mPath.isPath() == false) {
        return TRUE;
    }
    if (dPath_ChkClose(mPath.getPath())) {
        if (mPath.chkPointPass(current.pos, (u8)(mPath.getDir() != 0))) {
            mPath.nextIdxAuto();
        }
    }
    else return TRUE;
    cXyz pos = mPath.getPoint(mPath.getIdx());
    cLib_addCalcAngleS(&current.angle.y, cLib_targetAngleY(&current.pos, &pos), l_HIO.mPrm.mTurnRate, l_HIO.mPrm.mWalkTurnSpeed, 0x80);
    f32 target = l_HIO.mPrm.mWalkSpeed;
    if (cLib_calcTimer(&mMoveTimer) == 0||mTalking) {
        target = 0;
    }
    cLib_chaseF(&speedF, target, l_HIO.mPrm.mAcceleration);
    f32 rate = speedF*l_HIO.mPrm.mWalkAnmSpeed;
    rate = rate<0.5f?0.5f:rate;
    mpMorf->setPlaySpeed(rate);
    if ((int)target == 0&&(int)speedF == 0) {
        if (mTalking) {
            if (chk_talk()) {
                setStt(1);
                setAnm_NUM(9, 1);
                mLookMode = 1;
                mNoTurn = false;
                m_jnt.setTrn();
            }
            return TRUE;
        }
        setStt(1);
        setAnm_NUM(9, 1);
        return TRUE;
    }
    if (mEventOrder<3) {
        mEventOrder = 2;
    }
    mLookMode = 0;
    mNoTurn = true;
    return TRUE;
}

/* 000034C4-000035CC       .text talk_1__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::talk_1() {
    bool order = false;
    talk(1);
    if (!mpCurrMsg) {
        return TRUE;
    }
    switch (mpCurrMsg->mStatus) {
    case 2:
    case 6:
        break;
    case 19:
        switch (mCurrMsgNo) {
        case 0x1C2B:
            order = true;
            break;
        }
        mPresentItem = 0xFF;
        mTalking = false;
        setStt(mPreviousState);
        setAnm_NUM(0, 1);
        mLookTimer = cLib_getRndValue<int>(30, 60);
        mWaitTimer = cLib_getRndValue<int>(15, 30);
        if (order) {
            mEventOrder = 4;
        }
        endEvent();
        break;
    }
    return mAnmIndex != 4;
}

/* 000035CC-000036B4       .text wait_action1__11daNpc_Kf1_cFPv */
int daNpc_Kf1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        setStt(3);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        mHasAttention = chkAttention();switch (mState) {case 1:
            mUpdateEye = wait_1();
            break;
        case 2:
            mUpdateEye = talk_1();
            break;
        case 3:
            mUpdateEye = walk_1();
            break;
        }
        if (mActionState>1)orderTsuboEvent();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 000036B4-000037F4       .text demo__11daNpc_Kf1_cFv */
u8 daNpc_Kf1_c::demo() {
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
            mTexIndex = 2;
            mBtpFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, mArcName);
    }
    return mDemo;
}

/* 000037F4-00003884       .text shadowDraw__11daNpc_Kf1_cFv */
void daNpc_Kf1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
}

/* 00003884-0000397C       .text _draw__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::_draw() {
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
    shadowDraw();
    dSnap_RegistFig(DSNAP_TYPE_UNK59, this, 1.0f, 1.0f, 1.0f);
    if (l_HIO.mPrm.mDebug) {
        GXColor red = {255, 0, 0, 128};
        GXColor blue = {0, 0, 255, 128};
        GXColor yellow = {255, 255, 0, 128};
    }
    return TRUE;
}

/* 0000397C-00003BC4       .text _execute__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::_execute() {
    if (!mInitialSet) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mInitialSet = 1;
    }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackX, l_HIO.mPrm.mMaxBackY, l_HIO.mPrm.mMinBackX, l_HIO.mPrm.mMinBackY, l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY, l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurn);
    if (mHidden && demoActorID == 0) {
        return TRUE;
    }
    srch_Tsubo();
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) {
            staff = isEventEntry();
        }
        if (staff >= 0 || mEventActive) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        lookBack();
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        mWasGrounded = mObjAcch.ChkGroundHit() != 0;
        mWasWaterIn = mObjAcch.ChkWaterIn() != 0;
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

/* 00003BC4-00003C18       .text _delete__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::_delete() {
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

/* 00003C18-00003E64       .text _create__11daNpc_Kf1_cFv */
cPhs_State daNpc_Kf1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Kf1_c);
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
    l_HIO.entryHIO("金持ちム−ルの父");
    fopAcM_SetupActor(this, daNpc_Kf1_c);
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

/* 00004130-0000442C       .text bodyCreateHeap__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KF_BDL_KF_e);
    JUT_ASSERT(DEMO_SELECT(2427, 2431), a_mdl_dat != 0);
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
    JUT_ASSERT(DEMO_SELECT(2447, 2451), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(2449, 2453), m_bbone_jnt_num >= 0);
    m_nck_jnt_num = a_mdl_dat->getJointName()->getIndex("neck");
    JUT_ASSERT(DEMO_SELECT(2451, 2455), m_nck_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_nck_jnt_num)->setCallBack(nodeCB_Neck);
    mpMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}

/* 0000442C-000044EC       .text itemCreateHeap__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::itemCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KF_BDL_KF_HAIR_e);
    JUT_ASSERT(DEMO_SELECT(2472, 2476), a_mdl_dat != 0);
    mpItemModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
    if (mpItemModel == NULL) {
        return FALSE;
    }
    return TRUE;
}

/* 000044EC-00004590       .text CreateHeap__11daNpc_Kf1_cFv */
BOOL daNpc_Kf1_c::CreateHeap() {
    if (!bodyCreateHeap()) {
        return FALSE;
    }
    if (!itemCreateHeap()) {
        mpMorf = NULL;
        return FALSE;
    }
    mAcchCir.SetWall(30.0f, 90.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return TRUE;
}

/* 00004590-000045B0       .text daNpc_Kf1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Kf1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Kf1_c*)i_this)->_create();
}

/* 000045B0-000045D0       .text daNpc_Kf1_Delete__FP11daNpc_Kf1_c */
static BOOL daNpc_Kf1_Delete(daNpc_Kf1_c* i_this) {
    return ((daNpc_Kf1_c*)i_this)->_delete();
}

/* 000045D0-000045F0       .text daNpc_Kf1_Execute__FP11daNpc_Kf1_c */
static BOOL daNpc_Kf1_Execute(daNpc_Kf1_c* i_this) {
    return ((daNpc_Kf1_c*)i_this)->_execute();
}

/* 000045F0-00004610       .text daNpc_Kf1_Draw__FP11daNpc_Kf1_c */
static BOOL daNpc_Kf1_Draw(daNpc_Kf1_c* i_this) {
    return ((daNpc_Kf1_c*)i_this)->_draw();
}

/* 00004610-00004618       .text daNpc_Kf1_IsDelete__FP11daNpc_Kf1_c */
static BOOL daNpc_Kf1_IsDelete(daNpc_Kf1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Kf1_Method = {
    (process_method_func)daNpc_Kf1_Create, (process_method_func)daNpc_Kf1_Delete, (process_method_func)daNpc_Kf1_Execute, (process_method_func)daNpc_Kf1_IsDelete, (process_method_func)daNpc_Kf1_Draw, };
actor_process_profile_definition g_profile_NPC_KF1 = {
    /* Layer ID     */ fpcLy_CURRENT_e, /* List ID      */ 0x0007, /* List Prio    */ fpcPi_CURRENT_e, /* Proc Name    */ fpcNm_NPC_KF1_e, /* Proc SubMtd  */ &g_fpcLf_Method.base, /* Size         */ sizeof(daNpc_Kf1_c), /* Size Other   */ 0, /* Parameters   */ 0, /* Leaf SubMtd  */ &g_fopAc_Method.base, /* Draw Prio    */ fpcDwPi_NPC_KF1_e, /* Actor SubMtd */ &l_daNpc_Kf1_Method, /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e, /* Group        */ fopAc_ACTOR_e, /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e, };
