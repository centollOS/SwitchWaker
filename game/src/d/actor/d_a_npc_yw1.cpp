/**
 * d_a_npc_yw1.cpp
 * NPC - Sue-Belle
 */
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_yw1.h"
#include "d/actor/d_a_npc_bm1.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_snap.h"
#include "d/d_kankyo_wether.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Yw.h"
#include <string.h>
#include <math.h>

static daNpc_Yw1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;
daNpc_Yw1_childHIO_c::daNpc_Yw1_childHIO_c() {
}

daNpc_Yw1_HIO_c::daNpc_Yw1_HIO_c() {
    static Yw1Prm a_prm_tbl[] = {
        {
            8192, 7800, -8192, -7800, 0, 8400, 0, -8400, 1600, 1200, 170.0f, 0, 300.0f, 6, 1024, 0.95f, 1.2f, 0.5f
        }
    };
    for (int i = 0; i < 1; i++) {
        mChild[i].mIndex = i;
        memcpy(&mChild[i].mPrm, &a_prm_tbl[i], sizeof(Yw1Prm));
    }
    mNo = -1;
    mSelected = -1;
}

inline daNpc_Yw1_childHIO_c::~daNpc_Yw1_childHIO_c() {
}

#if VERSION > VERSION_JPN
static BOOL nodeCB_Hair(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Yw1_c* actor = (daNpc_Yw1_c*) model->getUserArea();
        if (actor) actor->_nodeCB_Hair(node, model);
    }
    return TRUE;
}

void daNpc_Yw1_c::_nodeCB_Hair(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(18.0f, 20.0f, 0.0f);
    int joint = ((J3DJoint*) node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (m_hair1 == joint) {
        mDoMtx_stack_c::YrotM(mHairY);
        mDoMtx_stack_c::ZrotM((mHairX + mNeckX) + mHairWave1);
    } else if (m_hair2 == joint) {
        mDoMtx_stack_c::YrotM(mHair2Y);
        mDoMtx_stack_c::ZrotM(mHair2X + mHairWave2);
    } else if (m_hair3 == joint) {
        mDoMtx_stack_c::YrotM(mHair3Y);
        mDoMtx_stack_c::ZrotM(mHair3X + mHairWave3);
    }
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    MTXCopy(mDoMtx_stack_c::get(), model->getAnmMtx(joint));
}
#endif

static BOOL nodeCB_Head(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Yw1_c* actor = (daNpc_Yw1_c*) model->getUserArea();
        if (actor) actor->_nodeCB_Head(node, model);
    }
    return TRUE;
}

void daNpc_Yw1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(18.0f, 20.0f, 0.0f);
    int joint = ((J3DJoint*) node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mHeadPos.x = mDoMtx_stack_c::get()[0][3];
    mHeadPos.y = mDoMtx_stack_c::get()[1][3];
    mHeadPos.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyePos);
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

static BOOL nodeCB_BackBone(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Yw1_c* actor = (daNpc_Yw1_c*) model->getUserArea();
        if (actor) actor->_nodeCB_BackBone(node, model);
    }
    return TRUE;
}

void daNpc_Yw1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*) node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    s16 angle = m_jnt.getBackbone_y();
    mDoMtx_stack_c::XrotM(angle);
    mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return((daNpc_Yw1_c*) actor)->CreateHeap();
}

static void* searchActor_Bm1(void* actor, void*) {
    if (l_check_wrk < 20 && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_NPC_BM1_e) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*) actor;
        l_check_wrk++;
    }
    return NULL;
}

bool daNpc_Yw1_c::init_YW1_0() {
    if (!dComIfGs_isEventBit(0x520) && !dComIfGs_isEventBit(1)) {
        if (mPath.isPath() == false) return false;
        mChangePotAnm = true;
        u32 prm = 0x7F063F;
        mPotID = fopAcM_create(fpcNm_TSUBO_e, prm, &current.pos, fopAcM_GetRoomNo(this), NULL, NULL, -1, NULL);
        set_action(&daNpc_Yw1_c::wait_action1, NULL);
        set_pthPoint(0);
        return mPotID != fpcM_ERROR_PROCESS_ID_e;
    }
    return false;
}

bool daNpc_Yw1_c::init_YW1_1() {
    if (!dComIfGs_isEventBit(0x520)) {
#if VERSION > VERSION_DEMO
        if (!dComIfGs_isEventBit(1)) fopAcM_SetStatus(this, actor_status & ~ 0x3F);
#endif
        set_action(&daNpc_Yw1_c::wait_action2, NULL);
        mHidden = true;
        return true;
    }
    return false;
}

bool daNpc_Yw1_c::init_YW1_2() {
    if (dComIfGs_isEventBit(0x520) && (dKy_daynight_check() == 1 || !dComIfGs_isEventBit(0x2A20))) {
        set_action(&daNpc_Yw1_c::wait_action2, NULL);
        return true;
    }
    return false;
}

bool daNpc_Yw1_c::init_YW1_3() {
    if (dComIfGs_isEventBit(0x520) && dKy_daynight_check() == 0) {
        if (mPath.isPath() == false) return false;
        if (dComIfGs_isEventBit(0x2A20)) {
            mChangePotAnm = true;
            u32 prm = 0x7F063F;
            mPotID = fopAcM_create(fpcNm_TSUBO_e, prm, &current.pos, fopAcM_GetRoomNo(this), NULL, NULL, -1, NULL);
            set_action(&daNpc_Yw1_c::wait_action1, NULL);
            set_pthPoint(0);
            return mPotID != fpcM_ERROR_PROCESS_ID_e;
        }
    }
    return false;
}

bool daNpc_Yw1_c::createInit() {
    mEventCut.setActorInfo2("Yw1", this);
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 171;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 171;
    int weight = 255;
    u8 path = (fopAcM_GetParam(this) >> 16) & 255;
    if (path != 255) {
        mPath.setInf(path, fopAcM_GetRoomNo(this), 1);
        if (mPath.isPath()) {
            fopAcM_OffStatus(this, 0x80);
            weight = 240;
        } else return false;
    }
    mAnmNo = 7;
    bool result;
    switch (mSubType) {
    case 0:
        result = init_YW1_0();
        break;
    case 1:
        result = init_YW1_1();
        break;
    case 2:
        result = init_YW1_2();
        break;
    case 3:
        result = init_YW1_3();
        break;
    default:
        result = false;
        break;
    }
    if (!result) return false;
    mModelAngle = current.angle;
    shape_angle = mModelAngle;
    gravity = -4.5f;
    mStts.Init(weight, 255, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    play_animation();
    mObjAcch.CrrPos(* dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

void daNpc_Yw1_c::play_animation() {
    u32 sound = 0;
    play_texPttrnAnm();
    if (mObjAcch.ChkGroundHit()) sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    mAnmEnd = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    if (mpMorf->getFrame() < mFrame) mAnmEnd = 1;
    mFrame = mpMorf->getFrame();
}

static cXyz l_hed_front(0.0f, 1.0f, 0.0f);
bool daNpc_Yw1_c::upLift() {
    int missing;
    fopAc_ac_c* pot = searchByID(mPotID, &missing);
    mPotMissing = missing == 1;
    if (pot) {
        if (!fopAcM_checkCarryNow(pot)) fopAcM_setCarryNow(pot, false);
        cXyz front;
        cXyz offset(34.0f, -4.0f, 0.0f);
        MTXMultVecSR(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num), &l_hed_front, &front);
        pot->shape_angle.y = cM_atan2s(front.x, front.z);
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
        mDoMtx_stack_c::multVec(&offset, &pot->current.pos);
    }
    return mPotMissing;
}

void daNpc_Yw1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(mModelAngle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
#if VERSION > VERSION_JPN
    setHairAngle();
#endif
    mpHeadModel->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
    mpHeadModel->calc();
    upLift();
    setAttention(force);
}

int daNpc_Yw1_c::bckResID(int index) {
    static const int a_res_id_tbl[] = {
        dRes_ID_YW_BCK_WAIT01_e,
        dRes_ID_YW_BCK_YW_TWAIT_e,
        dRes_ID_YW_BCK_YW_TWALK_e,
        dRes_ID_YW_BCK_YW_TTALK_e,
        dRes_ID_YW_BCK_YW_TALK01_e,
        dRes_ID_YW_BCK_YW_WAIT02_e,
        dRes_ID_YW_BCK_YW_TALK02_e
    };
    return a_res_id_tbl[index];
}

int daNpc_Yw1_c::btpResID(int index) {
    static const int a_res_id_tbl[] = {
        dRes_ID_YW_BTP_YWHEAD01_e
    };
    return a_res_id_tbl[index];
}

bool daNpc_Yw1_c::init_texPttrnAnm(s8 index, bool modify) {
    J3DModel* model = mpHeadModel;
    if (index < 0) return false;
    int res = btpResID(index);
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*) dComIfG_getObjectIDRes("Yw", res);
    JUT_ASSERT(VERSION_SELECT(0x25E, 0x25F, 0x28A, 0x28A), a_btp != 0);
    mBtpNo = index;
    mTexFrame = 0;
    mBlinkTimer = 0;
    return mBtpAnm.init(model->getModelData(), a_btp, TRUE, 0, 1.0f, 0, -1, modify, 0) != 0;
}

void daNpc_Yw1_c::play_texPttrnAnm() {
    if (mBtpNo == 0 && cLib_calcTimer(&mBlinkTimer) != 0) return;
    mTexFrame++;
    s16 end = mBtpAnm.getBtpAnm()->getFrameMax();
    if (mTexFrame >= end) {
        if (mBtpNo != 0) mTexFrame = end;
        else {
            mBlinkTimer = cLib_getRndValue < int > (60, 90);
            mTexFrame = 0;
        }
    }
}

void daNpc_Yw1_c::setAnm_anm(anm_prm_c* anm) {
    if (anm->mAnm < 0 || mAnmNo == anm->mAnm) return;
    int res = bckResID(anm->mAnm);
    dNpc_setAnmIDRes(mpMorf, anm->mLoop, anm->mMorf, anm->mSpeed, res, -1, "Yw");
    mAnmNo = anm->mAnm;
    mAnmEnd = 0;
    mAnmLoops = 0;
    mFrame = 0.0f;
}

void daNpc_Yw1_c::setAnm_NUM(int index, int btp) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {
            0, 0, 8, 1, 2
        }, {
            1, 0, 8, 1, 2
        }, {
            2, 0, 8, 1, 2
        }, {
            3, 0, 8, 1, 2
        }, {
            4, 0, 8, 1, 2
        }, {
            5, 0, 8, 1, 2
        }, {
            6, 0, 8, 1, 2
        }
    };
    if (btp) init_texPttrnAnm(a_anm_prm_tbl[index].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[index]);
}

void daNpc_Yw1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {
            - 1, -1, 0, 0, -1
        }, {
            5, 0, 8, 1, 2
        }, {
            - 1, -1, 0, 0, -1
        }, {
            2, 0, 8, 1, 2
        }, {
            5, 0, 8, 1, 2
        }, {
            2, 0, 8, 1, 2
        }, {
            5, 0, 8, 1, 2
        }
    };
    init_texPttrnAnm(a_anm_prm_tbl[mState].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

void daNpc_Yw1_c::chngAnmTag() {
}

void daNpc_Yw1_c::ctrlAnmTag() {
}

void daNpc_Yw1_c::chngAnmAtr(u8 attr) {
    if (attr == mAnmAttr || attr > 7) return;
    mAnmAttr = attr;
    setAnm_ATR();
}

void daNpc_Yw1_c::ctrlAnmAtr() {
}

void daNpc_Yw1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {
            0, 0, 8, 1, 2
        }, {
            1, 0, 8, 1, 2
        }, {
            2, 0, 8, 1, 2
        }, {
            3, 0, 8, 1, 2
        }, {
            4, 0, 8, 1, 2
        }, {
            5, 0, 8, 1, 2
        }, {
            6, 0, 8, 1, 2
        }
    };
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

void daNpc_Yw1_c::anmAtr(u16 status) {
    switch (status) {
    case 6:
        if (mMsgAnmStarted == 0) {
            chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        {
            u8 tag = dComIfGp_getMesgAnimeTagInfo();
            if (tag != 255 && tag != mAnmTag) {
                dComIfGp_clearMesgAnimeTagInfo();
                mAnmTag = tag;
                chngAnmTag();
            }
        }
        break;
    case 14:
        mMsgAnmStarted = 0;
        break;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

u16 daNpc_Yw1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x8A3:
        if (!dComIfGs_isEventBit(0x2A20)) * msg = 0x8A4;
        else if (dKy_daynight_check() == 0) * msg = 0x8A6;
        else * msg = 0x8A5;
        break;
    default:
        status = 16;
        break;
    }
    return status;
}

u32 daNpc_Yw1_c::getMsg_YW1_0() {
    if (mPaidPot) return 0x8A0;
    if (mPotMissing) return 0x89F;
    if (dComIfGs_isEventBit(1)) return 0x89B + (dComIfGs_isEventBit(0x140) ? 1 : 0);
    if (mPath.isPath() && mPathEnd == 1) return 0x8A1 + (mPath.getDir() == 1 ? 1 : 0);
    if (!dComIfGs_isEventBit(0x180)) return dComIfGs_isEventBit(0x2A80) ? 0x8AA : 0x899;
    return 0x89A;
}

u32 daNpc_Yw1_c::getMsg_YW1_1() {
    if (dComIfGs_isEventBit(0xE20)) return 0x89D + (dComIfGs_isEventBit(0x120) ? 1 : 0);
    return 0x89B + (dComIfGs_isEventBit(0x140) ? 1 : 0);
}

u32 daNpc_Yw1_c::getMsg_YW1_2() {
    if (!dComIfGs_isEventBit(0x3A40)) return 0x8A3;
    if (!dComIfGs_isEventBit(0x2A20)) return 0x8A7;
    return 0x8A8 + (dKy_daynight_check() == 0 ? 1 : 0);
}

u32 daNpc_Yw1_c::getMsg_YW1_3() {
    if (mPaidPot) return 0x8A0;
    if (mPotMissing) return 0x89F;
    if (mPath.isPath() && mPathEnd == 1) return 0x8A1 + (mPath.getDir() == 1 ? 1 : 0);
    return getMsg_YW1_2();
}

u32 daNpc_Yw1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0:
        msg = getMsg_YW1_0();
        break;
    case 1:
        msg = getMsg_YW1_1();
        break;
    case 2:
        msg = getMsg_YW1_2();
        break;
    case 3:
        msg = getMsg_YW1_3();
        break;
    }
    return msg;
}

void daNpc_Yw1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(1);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    }
}

void daNpc_Yw1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) return;
    if (eventInfo.checkCommandTalk() && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0;
        mTalking = true;
    }
}

// Unused; stripped by the linker. Name/signature from d_a_npc_yw1.map; body reconstructed from its leftover
// 0x7FFF / 180.0f .rodata literal (degrees to s16 angle), cf. daNpc_Bj1_c::chk_drct.
bool daNpc_Yw1_c::chk_direct(f32 i_deg) {
    s16 target = fopAcM_searchPlayerAngleY(this) - current.angle.y;
    s16 limit = i_deg * (0x7FFF / 180.0f);
    return abs(target) < limit;
}

bool daNpc_Yw1_c::chk_talk() {
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mItem = dComIfGp_event_getPreItemNo();
            return true;
        }
        return false;
    }
    mItem = 255;
    return true;
}

bool daNpc_Yw1_c::chk_parts_notMov() {
    return mOldHead == m_jnt.getHead_y() && mOldBackbone == m_jnt.getBackbone_y() && mOldAngle == current.angle.y;
}

fopAc_ac_c* daNpc_Yw1_c::searchByID(fpc_ProcID id, int* missing) {
    fopAc_ac_c* actor = NULL;
    if (missing) * missing = 0;
    if (!fopAcM_SearchByID(id, &actor) && missing) * missing = 1;
    return actor;
}

bool daNpc_Yw1_c::partner_search_sub(void* (* search)(void*, void*)) {
    bool result = false;
    mPartnerID = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < 20; i++) l_check_inf[i] = NULL;
    fpcEx_Search(search, this);
    if (l_check_wrk != 0) {
        mPartnerID = fopAcM_GetID(l_check_inf[0]);
        result = true;
    }
    return result;
}

void daNpc_Yw1_c::partner_search() {
    if (mActionState == 1) {
        bool result;
        switch (mSubType) {
        case 0:
            result = partner_search_sub(searchActor_Bm1);
            break;
        default:
            result = true;
            break;
        }
        if (result) mActionState++;
    }
}

void daNpc_Yw1_c::lookBack() {
    mOldHead = m_jnt.getHead_y();
    mOldBackbone = m_jnt.getBackbone_y();
    mOldAngle = current.angle.y;
    cXyz target;
    cXyz base = current.pos;
    base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz * targetP = NULL;
    s16 angle = current.angle.y;
    bool turn = mTurnBody;
    switch (mLookMode) {
    case 1:
        target = dNpc_playerEyePos(-20.0f);
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
        {
            fopAc_ac_c* actor = searchByID(mLookID, NULL);
            if (actor) {
                mLookPos = actor->current.pos;
                mLookPos.y = actor->eyePos.y;
                target = mLookPos;
                targetP = &target;
            }
        }
        break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mChild[mType].mPrm.turnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) mTurnSpeed = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, turn);
}

bool daNpc_Yw1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) return this == attention.LockonTarget(0);
    return this == attention.ActionTarget(0);
}

void daNpc_Yw1_c::setAttention(bool force) {
    f32 height = l_HIO.mChild[mType].mPrm.attentionHeight;
    cXyz pos = current.pos;
    attention_info.position.set(pos.x, pos.y + height, pos.z);
    if (mSetEyePos || force) eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
}

bool daNpc_Yw1_c::decideType(int type) {
    mType = 0;
    mSubType = -1;
    switch (type) {
    case 0:
        mSubType = 0;
        break;
    case 1:
        mSubType = 1;
        break;
    case 2:
        mSubType = 2;
        break;
    case 3:
        mSubType = 3;
        break;
    default:
        return false;
    }
    bool result = false;
    if (mType != -1 && mSubType != -1) result = true;
    return result;
}

void daNpc_Yw1_c::privateCut(int staff) {
    if (staff != -1) {
        static char* a_cut_tbl[] = {
            "DUMMY"
        };
        mCut = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, FALSE);
        if (mCut == -1) dComIfGp_evmng_cutEnd(staff);
        else {
            dComIfGp_evmng_getIsAddvance(staff);
            bool done;
            switch (mCut) {
            default:
                done = true;
                break;
            }
            if (done) dComIfGp_evmng_cutEnd(staff);
        }
    }
}

void daNpc_Yw1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 255;
    mAnmTag = 255;
}

int daNpc_Yw1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

void daNpc_Yw1_c::event_proc(int staff) {
    if (!mEventCut.cutProc()) privateCut(staff);
    lookBack();
}

bool daNpc_Yw1_c::set_action(ActionFunc action, void* arg) {
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

void daNpc_Yw1_c::setStt(s8 state) {
    s8 old = mState;
    mState = state;
    switch (mState) {
    case 2:
        mAnmAttr = 255;
        mAnmTag = 255;
        mMsgAnmStarted = 0;
        mPreviousState = old;
        break;
    case 3:
    case 5:
        mTurnBody = true;
        break;
    case 4:
    case 6:
        break;
    }
    setAnm();
    chngTsuboAnm();
}

void daNpc_Yw1_c::chngTsuboAnm() {
    if (mChangePotAnm) {
        if (!mPotMissing) {
            switch (mAnmNo) {
            case 4:
                setAnm_NUM(3, 1);
                break;
            case 5:
                setAnm_NUM(1, 1);
                break;
            }
        } else {
            switch (mAnmNo) {
            case 1:
                setAnm_NUM(5, 1);
                break;
            case 3:
                setAnm_NUM(4, 1);
                break;
            }
            mChangePotAnm = false;
        }
    }
}

bool daNpc_Yw1_c::chk_areaIN(f32 radius, f32 height, s16 angle, cXyz pos) {
    f32 distance = (dComIfGp_getLinkPlayer()->current.pos - pos).absXZ();
    f32 dy = daPy_getPlayerLinkActorClass()->current.pos.y - pos.y;
    int targetAngle = cLib_targetAngleY(&current.pos, &daPy_getPlayerLinkActorClass()->current.pos);
    s16 da = targetAngle - current.angle.y;
    if (distance < radius && std::fabsf(dy) < height && abs(da) < angle) return true;
    return false;
}

void daNpc_Yw1_c::set_pthPoint(u8 index) {
    if (mPath.isPath() != false) {
        mPath.setIdx(index);
        current.pos = mPath.getPoint(mPath.getIdx());
        if (mPath.nextIdx()) {
            cXyz pos = mPath.getPoint(mPath.getIdx());
            current.angle.y = cLib_targetAngleY(&current.pos, &pos);
        }
    }
}

#if VERSION > VERSION_JPN
static const Vec l_eye_offset = {
    15.0f, 10.0f, 0.0f
};

static const Vec l_head_center_offset = {
    15.0f, 0.0f, 0.0f
};

static const Vec l_neck_front = {
    0.0f, 1.0f, 0.0f
};

static const Vec l_neck_top = {
    1.0f, 0.0f, 0.0f
};

void daNpc_Yw1_c::setHairAngle() {
    cXyz front, top, center, eye, forward, pos, delta, wind;
    MtxP head = mpMorf->getModel()->getAnmMtx(m_hed_jnt_num);
    MTXMultVec(head, &l_head_center_offset, &center);
    MTXMultVec(head, &l_eye_offset, &eye);
    forward = eye - center;
    s16 angle = cM_atan2s(forward.x, forward.z);
    f32 sin = cM_ssin(angle), cos = cM_scos(angle);
    MtxP matrix = mpMorf->getModel()->getAnmMtx(m_hed_jnt_num);
    pos.x = matrix[0][3];
    pos.y = matrix[1][3];
    pos.z = matrix[2][3];
    f32 power;
    dKyw_get_AllWind_vec(&pos, &wind, &power);
    f32 windPower = power * power;
    MTXMultVecSR(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num), &l_neck_front, &front);
    MTXMultVecSR(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num), &l_neck_top, &top);
    s16 oldNeckX = mNeckX, oldNeckY = mNeckY;
    if (top.y < 0.0f) {
        mNeckX = cM_atan2s(front.y, -front.absXZ());
        mNeckY = cM_atan2s(front.x, front.z) + 0x8000;
    } else {
        mNeckX = cM_atan2s(front.y, front.absXZ());
        mNeckY = cM_atan2s(front.x, front.z);
    }
    if (std::fabsf(front.y) > 0.7f) mNeckY = oldNeckY;
    s16 neckDX = (s16)(mNeckX - oldNeckX) / 2, neckDY = (s16)(mNeckY - oldNeckY) / 2;
    if (neckDX > 0x200) neckDX = 0x200;
    else if (neckDX < -0x200) neckDX = -0x200;
    if (neckDY > 0x800) neckDY = 0x800;
    else if (neckDY < -0x800) neckDY = -0x800;
    delta.x = mOldHeadPos.x - pos.x;
    delta.y = mOldHeadPos.y - pos.y - 7.5f;
    delta.z = mOldHeadPos.z - pos.z;
    delta += wind * windPower;
    if (std::fabsf(delta.x) < 0.01f) delta.x = 0.0f;
    if (std::fabsf(delta.z) < 0.01f) delta.z = 0.0f;
    s16 oldX = mHairX, oldY = mHairY;
    f32 projected = delta.z * cos + delta.x * sin;
    s16 targetX = cM_atan2s(-projected, -delta.y);
    if (mHairX < 0) targetX = 0;
    else if (targetX < 0 && targetX > -0x7800) targetX = 0;
    else if (targetX > 0x7800 || targetX <= -0x7800) targetX = 0x7800;
    cLib_addCalcAngleS2(&mHairX, targetX, 5, 0x400);
    mHairX = mHairVelX + neckDX + mHairX;
    s16 targetY = cM_atan2s(-(delta.x * cos - delta.z * sin), std::sqrtf(projected * projected + delta.y * delta.y));
    if (targetY > 0x3800) targetY = 0x3800;
    else if (targetY < -0x3800) targetY = -0x3800;
    cLib_addCalcAngleS2(&mHairY, targetY, 5, 0x400);
    mHairY += mHairVelY - neckDY;
    mHairVelX = 0.2f * (s16)(mHairX - oldX);
    mHairVelY = 0.2f * (s16)(mHairY - oldY);
    oldX = mHairX - oldX;
    oldY = mHairY - oldY;
    mHair2X -= oldX;
    mHair2Y -= oldY;
    oldX = mHair2X;
    oldY = mHair2Y;
    cLib_addCalcAngleS2(&mHair2X, 0, 5, 0x400);
    cLib_addCalcAngleS2(&mHair2Y, 0, 5, 0x400);
    mHair2X = mHair2VelX + neckDX + mHair2X;
    mHair2Y += mHair2VelY - neckDY;
    mHair2VelX = 0.2f * (s16)(mHair2X - oldX);
    mHair2VelY = 0.2f * (s16)(mHair2Y - oldY);
    oldX = mHair2X - oldX;
    oldY = mHair2Y - oldY;
    mHair3X -= oldX;
    mHair3Y -= oldY;
    oldX = mHair3X;
    oldY = mHair3Y;
    cLib_addCalcAngleS2(&mHair3X, 0, 5, 0x400);
    cLib_addCalcAngleS2(&mHair3Y, 0, 5, 0x400);
    mHair3X = mHair3VelX + neckDX + mHair3X;
    mHair3Y += mHair3VelY - neckDY;
    mHair3VelX = 0.2f * (s16)(mHair3X - oldX);
    mHair3VelY = 0.2f * (s16)(mHair3Y - oldY);
    f32 distance = mOldHeadPos.abs(pos);
    f32 wave = (windPower + 0.65f * distance) / 4.0f;
    if (wave > 1.0f) wave = 1.0f;
    s16 phaseStep = 1500.0f + 4096.0f * wave;
    mHairPhase += phaseStep;
    mHairWave1 = 2280.0f * wave * cM_scos(mHairPhase);
    mHairWave2 = 3908.0f * wave * cM_scos(mHairPhase - 3.0f * phaseStep);
    mHairWave3 = 7568.0f * wave * cM_scos(mHairPhase - 6.0f * phaseStep);
    mOldHeadPos = pos;
}
#endif

bool daNpc_Yw1_c::chk_brkTsubo() {
    if (mPotMissing) {
        setStt(4);
        speedF = 0.0f;
        mLookMode = 0;
        mTurnBody = false;
    }
    return mPotMissing;
}

bool daNpc_Yw1_c::chk_bm1Odoroki() {
    daNpc_Bm1_c* actor = (daNpc_Bm1_c*) searchByID(mPartnerID, NULL);
    if (actor) return actor->getOdoroki();
    return false;
}

int daNpc_Yw1_c::wait_1() {
    if (chk_brkTsubo()) return 1;
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
            mLookMode = 1;
            mTurnBody = false;
            m_jnt.setTrn();
        }
        return 1;
    }
    if (chk_bm1Odoroki()) {
        mLookMode = 4;
        mLookID = mPartnerID;
        return 1;
    }
    mOrder = 2;
    chngTsuboAnm();
    bool outside = !chk_areaIN(100.0f + l_HIO.mChild[mType].mPrm.areaRadius, 100.0f, 0x4400, current.pos);
    if (outside) {
        if (cLib_calcTimer(&mLookTimer) == 0) {
            setStt(3);
            mLookMode = 0;
            mTurnBody = true;
            return 1;
        }
    } else mLookTimer = cLib_getRndValue < int > (10, 20);
    mLookMode = 1;
    return 1;
}

int daNpc_Yw1_c::wait_2() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
            mLookMode = 1;
            mTurnBody = false;
            m_jnt.setTrn();
        }
        return 1;
    }
    if (chk_bm1Odoroki()) {
        mLookMode = 4;
        mLookID = mPartnerID;
        mTurnBody = false;
        m_jnt.setTrn();
        return 1;
    }
    mOrder = 2;
    mTurnBody = true;
    cLib_addCalcAngleS(&current.angle.y, mInitialAngle.y, 4, 0x800, 0x80);
    if (mAttention) mLookTimer = cLib_getRndValue < int > (10, 20);
    if (cLib_calcTimer(&mLookTimer) != 0) {
        mLookMode = 1;
        return 1;
    }
    mLookMode = 0;
    return 1;
}

int daNpc_Yw1_c::wait_3() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
            mLookMode = 1;
            mTurnBody = false;
            m_jnt.setTrn();
        }
        return 1;
    }
    mOrder = 2;
    mTurnBody = true;
    cLib_addCalcAngleS(&current.angle.y, mInitialAngle.y, 4, 0x800, 0x80);
    if (mAttention) mLookTimer = cLib_getRndValue < int > (10, 20);
    if (cLib_calcTimer(&mLookTimer) != 0) {
        mLookMode = 1;
        return 1;
    }
    mLookMode = 0;
    return 1;
}

int daNpc_Yw1_c::walk_1() {
    if (chk_brkTsubo()) return 1;
    if (!mPathEnd && mPath.chkPointPass(current.pos, (bool) mPath.getDir())) mPathEnd = !mPath.nextIdx();
    f32 targetSpeed;
    if (!mPathEnd && chk_areaIN(l_HIO.mChild[mType].mPrm.areaRadius, 100.0f, 0x4000, current.pos) == false && !mTalking && !chk_bm1Odoroki()) {
        cXyz pos = mPath.getPoint(mPath.getIdx());
        int angle = cLib_targetAngleY(&current.pos, &pos);
        cLib_addCalcAngleS(&current.angle.y, angle, l_HIO.mChild[mType].mPrm.walkTurnRate, l_HIO.mChild[mType].mPrm.walkTurnSpeed, 0);
        targetSpeed = l_HIO.mChild[mType].mPrm.walkSpeed;
    } else {
        targetSpeed = 0.0f;
    }
    cLib_chaseF(&speedF, targetSpeed, l_HIO.mChild[mType].mPrm.acceleration);
    f32 rate = speedF * l_HIO.mChild[mType].mPrm.animationSpeed;
    f32 playSpeed = rate < 0.5f ? 0.5f : rate;
    mpMorf->setPlaySpeed(playSpeed);
    if ((int) targetSpeed == 0 && (int) speedF == 0) {
        speedF = 0.0f;
        if (mTalking) {
            if (chk_talk()) {
                setStt(2);
                mLookMode = 1;
                mTurnBody = false;
                m_jnt.setTrn();
            }
            return 1;
        }
        if (mPathEnd) {
            setStt(5);
            mLookMode = 0;
            return 1;
        }
        setStt(1);
        mLookMode = 1;
        mTurnBody = false;
        m_jnt.setTrn();
        return 1;
    }
    mOrder = 2;
    mLookMode = 0;
    return 1;
}

int daNpc_Yw1_c::turn_1() {
    if (chk_brkTsubo()) return 1;
    if (mTalking) {
        if (chk_talk()) setStt(2);
        return 1;
    }
    mOrder = 2;
    cXyz pos = mPath.getPoint(mPath.getIdx());
    int angle = cLib_targetAngleY(&current.pos, &pos);
    cLib_chaseAngleS(&current.angle.y, angle, l_HIO.mChild[mType].mPrm.walkTurnSpeed);
    if (current.angle.y == angle) {
        setStt(3);
        mLookMode = 0;
        mPath.turnDir();
        mPathEnd = false;
        return 1;
    }
    return 1;
}

int daNpc_Yw1_c::talk_1() {
    int still = chk_parts_notMov();
    talk(1);
    if (!mpCurrMsg) return still;
    switch (mpCurrMsg->mStatus) {
    case 2:
    case 6:
        break;
    case 19:
        switch (mCurrMsgNo) {
        case 0x899:
        case 0x8AA:
            dComIfGs_onEventBit(0x180);
            break;
        case 0x89B:
            dComIfGs_onEventBit(0x140);
            break;
        case 0x89D:
            dComIfGs_onEventBit(0x120);
            break;
        case 0x89F:
            dComIfGp_setItemRupeeCount(-10);
            mPaidPot = true;
            break;
        case 0x8A4:
        case 0x8A5:
        case 0x8A6:
            dComIfGs_onEventBit(0x3A40);
            break;
        }
        mItem = 255;
        mTalking = false;
        setStt(mPreviousState);
        mLookTimer = cLib_getRndValue < int > (10, 20);
        endEvent();
        break;
    }
    return still;
}

int daNpc_Yw1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        setStt(3);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        mAttention = chkAttention();
        switch (mState) {
        case 1:
            mSetEyePos = wait_1();
            break;
        case 4:
            mSetEyePos = wait_2();
            break;
        case 3:
            mSetEyePos = walk_1();
            break;
        case 5:
            mSetEyePos = turn_1();
            break;
        case 2:
            mSetEyePos = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    if (dComIfGs_isEventBit(1) && mSubType == 0) {
        int missing;
        fopAc_ac_c* pot = searchByID(mPotID, &missing);
        if (missing == 0 && pot) fopAcM_delete(pot);
        fopAcM_delete(this);
    }
    return 1;
}

int daNpc_Yw1_c::wait_action2(void*) {
    switch (mActionState) {
    case 0:
        setStt(6);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        mAttention = chkAttention();
        switch (mState) {
        case 6:
            mSetEyePos = wait_3();
            break;
        case 2:
            mSetEyePos = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

u8 daNpc_Yw1_c::demo() {
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
        if (mBtpAnm.getBtpAnm()) {
            mTexFrame++;
            s16 end = mBtpAnm.getBtpAnm()->getFrameMax();
            if (mTexFrame >= end) mTexFrame = end;
        }
        J3DAnmTexPattern* btp = actor->getP_BtpData("Yw");
        if (btp) {
            mBtpAnm.init(mpHeadModel->getModelData(), btp, TRUE, 0, 1.0f, 0, -1, true, 0);
            mBtpNo = 1;
            mTexFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Yw");
    }
    return mDemo;
}

void daNpc_Yw1_c::shadowDraw() {
    int missing;
    fopAc_ac_c* pot = searchByID(mPotID, &missing);
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadow = dComIfGd_setShadow(mShadow, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadow) {
#if VERSION == VERSION_DEMO
        if (missing == 0 && pot) dComIfGd_addRealShadow(mShadow, pot->model);
#else
        if (missing == 0 && pot && pot->model) dComIfGd_addRealShadow(mShadow, pot->model);
#endif
        dComIfGd_addRealShadow(mShadow, mpHeadModel);
    }
}

BOOL daNpc_Yw1_c::_draw() {
    J3DModel* head = mpHeadModel;
    J3DModelData* data = head->getModelData();
    J3DModel* body = mpMorf->getModel();
    if (mHidden || m779) return TRUE;
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(body, &tevStr);
    g_env_light.setLightTevColorType(head, &tevStr);
    mpMorf->entryDL();
    mBtpAnm.entry(data, mTexFrame);
    mDoExt_modelEntryDL(head);
    data->getMaterialTable().removeTexNoAnimator(mBtpAnm.getBtpAnm());
    shadowDraw();
    dSnap_RegistFig(0x4B, this, 1.0f, 1.0f, 1.0f);
    if (l_HIO.mChild[mType].mPrm.debug) {
        cXyz pos = current.pos;
        pos.y = eyePos.y;
        GXColor red = {
            255, 0, 0, 128
        };
        GXColor blue = {
            0, 0, 255, 128
        };
        GXColor yellow = {
            255, 255, 0, 128
        };
    }
    return TRUE;
}

BOOL daNpc_Yw1_c::_execute() {
    if (!mInitialized) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mInitialized = true;
    }
    m_jnt.setParam(l_HIO.mChild[mType].mPrm.maxBackX, l_HIO.mChild[mType].mPrm.maxBackY, l_HIO.mChild[mType].mPrm.minBackX, l_HIO.mChild[mType].mPrm.minBackY, l_HIO.mChild[mType].mPrm.maxHeadX, l_HIO.mChild[mType].mPrm.maxHeadY, l_HIO.mChild[mType].mPrm.minHeadX, l_HIO.mChild[mType].mPrm.minHeadY, l_HIO.mChild[mType].mPrm.maxTurn);
    if (mHidden && demoActorID == 0) {
        if (dComIfGs_isEventBit(1)) {
#if VERSION > VERSION_DEMO
            if (mSubType == 1) fopAcM_SetStatus(this, (actor_status & ~ 0x3F) | 0x28);
#endif
            mHidden = false;
        } else return TRUE;
    }
    partner_search();
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) staff = isEventEntry();
        if (staff >= 0) event_proc(staff);
        else (this->*mAction)(NULL);
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        play_animation();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    eventOrder();
    mModelAngle = current.angle;
    if (!mKeepShapeAngle) shape_angle = current.angle;
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    setMtx(false);
    setCollision(30.0f, 150.0f);
    return TRUE;
}

BOOL daNpc_Yw1_c::_delete() {
    dComIfG_resDelete(&mPhs, "Yw");
#if VERSION == VERSION_DEMO
    if (mpMorf) mpMorf->stopZelAnime();
    l_HIO.removeHIO();
#else
    if (heap && mpMorf) mpMorf->stopZelAnime();
#endif
    return TRUE;
}

cPhs_State daNpc_Yw1_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Yw");
    if (phase != cPhs_COMPLEATE_e) return phase;
    int type = fopAcM_GetParam(this) & 255;
    if (!decideType(type)) return cPhs_ERROR_e;
    l_HIO.entryHIO("壷娘");
    fopAcM_SetupActor(this, daNpc_Yw1_c);
#else
    fopAcM_SetupActor(this, daNpc_Yw1_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Yw");
    if (phase != cPhs_COMPLEATE_e) return phase;
    if (!decideType(fopAcM_GetParam(this) & 255)) return cPhs_ERROR_e;
#endif
    static u32 a_siz_tbl[] = {
        0
    };
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) return cPhs_ERROR_e;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -40.0f, -20.0f, -40.0f, 40.0f, 210.0f, 40.0f);
    if (!createInit()) return cPhs_ERROR_e;
    return phase;
}

int daNpc_Yw1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*) dComIfG_getObjectIDRes("Yw", dRes_ID_YW_BDL_YW_e);
    JUT_ASSERT(VERSION_SELECT(0x873, 0x87A, 0x96F, 0x96F), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, NULL, -1, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
    if (!mpMorf) return 0;
    if (!mpMorf->getModel()) {
        mpMorf = NULL;
        return 0;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(VERSION_SELECT(0x881, 0x888, 0x97D, 0x97D), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(VERSION_SELECT(0x883, 0x88A, 0x97F, 0x97F), m_bbone_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->setUserArea((u32) this);
    return 1;
}

int daNpc_Yw1_c::headCreateHeap() {
    static int a_hed_bdl_resID_tbl[] = {
        dRes_ID_YW_BDL_YWHEAD01_e
    };
    static s8 a_tex_pttrn_num_tbl[] = {
        0
    };
    J3DModelData* a_mdl_dat = (J3DModelData*) dComIfG_getObjectIDRes("Yw", a_hed_bdl_resID_tbl[mType]);
    JUT_ASSERT(VERSION_SELECT(0x8A0, 0x8A7, 0x99C, 0x99C), a_mdl_dat != 0);
    mpHeadModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x15020022);
    if (!mpHeadModel) return 0;
    if (!init_texPttrnAnm(a_tex_pttrn_num_tbl[mType], false)) return 0;
#if VERSION > VERSION_JPN
    m_hair1 = a_mdl_dat->getJointName()->getIndex("hair1");
    JUT_ASSERT(0x9AD, m_hair1 >= 0);
    m_hair2 = a_mdl_dat->getJointName()->getIndex("hair2");
    JUT_ASSERT(0x9AF, m_hair2 >= 0);
    m_hair3 = a_mdl_dat->getJointName()->getIndex("hair3");
    JUT_ASSERT(0x9B1, m_hair3 >= 0);
    mpHeadModel->getModelData()->getJointNodePointer(m_hair1)->setCallBack(nodeCB_Hair);
    mpHeadModel->getModelData()->getJointNodePointer(m_hair2)->setCallBack(nodeCB_Hair);
    mpHeadModel->getModelData()->getJointNodePointer(m_hair3)->setCallBack(nodeCB_Hair);
    mpHeadModel->setUserArea((u32) this);
#endif
    return 1;
}

int daNpc_Yw1_c::CreateHeap() {
    if (!bodyCreateHeap()) return 0;
    if (!headCreateHeap()) {
        mpMorf = NULL;
        return 0;
    }
    mAcchCir.SetWall(30.0f, 30.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return 1;
}

inline daNpc_Yw1_HIO_c::~daNpc_Yw1_HIO_c() {
}

/* 000049D0-000049F0       .text daNpc_Yw1_Create__FP10fopAc_ac_c*/
static cPhs_State daNpc_Yw1_Create(fopAc_ac_c* i_this) {
    return((daNpc_Yw1_c*) i_this)->_create();
}

/* 000049F0-00004A10       .text daNpc_Yw1_Delete__FP11daNpc_Yw1_c*/
static BOOL daNpc_Yw1_Delete(daNpc_Yw1_c* i_this) {
    return((daNpc_Yw1_c*) i_this)->_delete();
}

/* 00004A10-00004A30       .text daNpc_Yw1_Execute__FP11daNpc_Yw1_c*/
static BOOL daNpc_Yw1_Execute(daNpc_Yw1_c* i_this) {
    return((daNpc_Yw1_c*) i_this)->_execute();
}

/* 00004A30-00004A50       .text daNpc_Yw1_Draw__FP11daNpc_Yw1_c*/
static BOOL daNpc_Yw1_Draw(daNpc_Yw1_c* i_this) {
    return((daNpc_Yw1_c*) i_this)->_draw();
}

/* 00004A50-00004A58       .text daNpc_Yw1_IsDelete__FP11daNpc_Yw1_c*/
static BOOL daNpc_Yw1_IsDelete(daNpc_Yw1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Yw1_Method = {
    (process_method_func) daNpc_Yw1_Create, (process_method_func) daNpc_Yw1_Delete, (process_method_func) daNpc_Yw1_Execute, (process_method_func) daNpc_Yw1_IsDelete, (process_method_func) daNpc_Yw1_Draw,
};

actor_process_profile_definition g_profile_NPC_YW1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_YW1_e,
    /* Proc SubMtd  */ & g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Yw1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ & g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_YW1_e,
/* Actor SubMtd*/
    & l_daNpc_Yw1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
