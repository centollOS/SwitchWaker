/**
 * d_a_npc_kg2.cpp
 * NPC - Salvatore (Cannon Minigame)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_kg2.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "d/d_com_inf_game.h"
#include "d/d_lib.h"
#include "d/d_snap.h"
#include "d/d_s_play.h"
#include "d/actor/d_a_player_main.h"
#include "res/Object/Kg.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JAZelAudio/JAIZelBasic.h"
#include <math.h>
#include "d/d_cc_d.h"

static dCcD_SrcCyl l_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_ALL,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCylS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 30.0f,
        /* Height */ 80.0f,
    }},
};



STATIC_ASSERT(sizeof(daNpc_Kg2_c) == 0x764);
static daNpc_Kg2_HIO_c l_HIO;

daNpc_Kg2_HIO_c::daNpc_Kg2_HIO_c() {
    mNpc.m04 = -20.0f;
    mNpc.mMaxHeadX = 2500;
    mNpc.mMaxHeadY = 7000;
    mNpc.mMaxBackboneX = 2000;
    mNpc.mMaxBackboneY = 8000;
    mNpc.mMinHeadX = -2500;
    mNpc.mMinHeadY = -7000;
    mNpc.mMinBackboneX = -2000;
    mNpc.mMinBackboneY = -8000;
    mNpc.mMaxTurnStep = 0x1000;
    mNpc.mMaxHeadTurnVel = 1000;
    mNpc.mAttnYOffset = 35.0f;
    mNpc.mMaxAttnAngleY = 0x4000;
    mNpc.m22 = 0;
    mNpc.mMaxAttnDistXZ = 400.0f;
    m30 = 0;
    mChild = -1;
}
daNpc_Kg2_HIO_c::~daNpc_Kg2_HIO_c() {}

static const int l_bck_ix_tbl[] = {
    dRes_INDEX_KG_BCK_KG_WAIT01_e,
    dRes_INDEX_KG_BCK_KG_WAIT02_e,
    dRes_INDEX_KG_BCK_KG_TALK01_e,
    dRes_INDEX_KG_BCK_KG_KAMAE_e,
    dRes_INDEX_KG_BCK_KG_KWAIT_e,
    dRes_INDEX_KG_BCK_KG_CAPTAIN_e,
    dRes_INDEX_KG_BCK_KG_CAPTAIN02_e,
    dRes_INDEX_KG_BCK_KG_KIDS_e,
    dRes_INDEX_KG_BCK_KG_RADIO_e,
    dRes_INDEX_KG_BCK_KG_SASIKAE_e,
    dRes_INDEX_KG_BCK_KG_GONZO_e,
    dRes_INDEX_KG_BCK_KG_TETORA_e,
    dRes_INDEX_KG_BCK_KG_KAMAE_e,
    dRes_INDEX_KG_BCK_KG_KWAIT_e,
    dRes_INDEX_KG_BCK_KG_TETORA02_e,
};
static const int l_btp_ix_tbl[] = {
    dRes_INDEX_KG_BTP_KG_e,
    dRes_INDEX_KG_BTP_MOPEN_e,
    dRes_INDEX_KG_BTP_WARAI_e,
    dRes_INDEX_KG_BTP_RADIO_e,
    dRes_INDEX_KG_BTP_SLEEP_e,
};

static BOOL nodeCallBack(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Kg2_c* actor = (daNpc_Kg2_c*)model->getUserArea();
        if (actor != NULL) {
            int joint = ((J3DJoint*)node)->getJntNo();
            mDoMtx_stack_c::copy(model->getAnmMtx(joint));
            if (joint == actor->m_jnt.getHeadJntNum()) {
                cXyz offset(24.0f + REG10_F(0), 5.0f + REG10_F(1), REG10_F(2));
                static cXyz l_offsetEyePos(24.0f, -16.0f, 0.0f);
                mDoMtx_stack_c::multVec(&offset, &actor->getAttentionBasePos());
                mDoMtx_stack_c::XrotM((s16)actor->m_jnt.getHead_y());
                mDoMtx_stack_c::ZrotM(-actor->m_jnt.getHead_x());
                mDoMtx_stack_c::multVec(&l_offsetEyePos, &actor->getEyePos());
                mDoMtx_stack_c::multVec(&offset, &actor->attention_info.position);
                actor->attention_info.position.y += l_HIO.mNpc.mAttnYOffset;
            } else if (joint == actor->m_jnt.getBackboneJntNum()) {
                mDoMtx_stack_c::XrotM((s16)actor->m_jnt.getBackbone_y());
                mDoMtx_stack_c::ZrotM(-actor->m_jnt.getBackbone_x());
            }
            MTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
            model->setAnmMtx(joint, mDoMtx_stack_c::get());
        }
    }
    return TRUE;
}

void daNpc_Kg2_c::set_mtx() {
    J3DModel* model = mpMorf->getModel();
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
#if VERSION > VERSION_DEMO
    mpMorf->calc();
#endif
    if (mDrawPlate) {
        mDoMtx_stack_c::copy(model->getAnmMtx(m_handL_num));
        mDoMtx_stack_c::transM(23.46f, -22.26f, -47.05f);
#if VERSION == VERSION_DEMO
        // The demo passes the unwrapped value 0xB100 here, which only a float -> s16 conversion reproduces.
        mDoMtx_stack_c::XYZrotM(0x1F4B, cM_deg2s(248.90625f), 0x1F4B);
#else
        mDoMtx_stack_c::XYZrotM(0x1F4B, -0x4F00, 0x1F4B);
#endif
        mpPlateModel->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

BOOL daNpc_Kg2_c::initTexPatternAnm(bool modify) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    m_btp = (J3DAnmTexPattern*)dComIfG_getObjectRes("Kg", l_btp_ix_tbl[mBtpNo]);
    JUT_ASSERT(DEMO_SELECT(0x12A, 0x12B), m_btp != 0);
    if (!mBtpAnm.init(data, m_btp, true, 2, 1.0f, 0, -1, modify, 0)) return FALSE;
    mBtpFrame = 0; mBtpTimer = 0;
    return TRUE;
}

void daNpc_Kg2_c::playTexPatternAnm() {
    if (mSleeping == 1 && mBtpNo == 0 && mBtpFrame == 2) {
        mBtpFrame = 0; mBtpNo = 4; initTexPatternAnm(true);
    } else if (cLib_calcTimer(&mBtpTimer) == 0) {
        if (mBtpNo == 4) {
            if (mBtpFrame == 0) { mBtpFrame = 1; mBtpTimer = 150.0f + cM_rndF(150.0f); }
            else { mBtpFrame = 0; mBtpTimer = (s16)(150.0f + cM_rndF(150.0f)) * 2; }
        } else {
            if (mBtpFrame >= m_btp->getFrameMax()) {
                mBtpFrame -= m_btp->getFrameMax();
                mBtpTimer = 30.0f + cM_rndF(100.0f);
            } else mBtpFrame++;
        }
    }
}

void daNpc_Kg2_c::setAnm(s8 anm, f32 morf) {
    static int a_play_mode_tbl[] = {2,2,2,0,2,2,2,2,2,0,2,2,0,0,2};
    static f32 a_morf_frame_tbl[] = {8,8,8,8,8,8,8,8,8,8,8,8,8,8,8};
    static f32 a_play_speed_tbl[] = {1,1,1,1,1,1,1,1,1,1,1,1,-1,1,1};
    if (morf < 0.0f) morf = a_morf_frame_tbl[anm];
    if (anm != mAnmNo && mAnmNo != -1) {
        mAnmNo = anm;
        dNpc_setAnm(mpMorf, a_play_mode_tbl[mAnmNo], morf, a_play_speed_tbl[mAnmNo], l_bck_ix_tbl[mAnmNo], -1, "Kg");
        if (mAnmNo == 12) {
            mpMorf->setFrame(mpMorf->getEndFrame() - 1.0f);
            mpMorf->setPlaySpeed(-1.0f);
        }
    }
    if (mAnmNo == 2) { m_jnt.offHeadLock(); m_jnt.onBackBoneLock(); }
    else { m_jnt.onHeadLock(); m_jnt.onBackBoneLock(); }
}

void daNpc_Kg2_c::subAnm() {
    if (mBtpNo == 4) seStart(JA_SE_CV_KG_SLEEP);
    if ((mAnmNo == 3 || mAnmNo == 13) && mpMorf->checkFrame(1.0f)) {
        mDrawPlate = true;
        if (mAnmNo == 3) mPlateFrame = 3;
        else mPlateFrame = 4;
    } else if (mAnmNo == 9 && mpMorf->checkFrame(30.0f)) {
        mDrawPlate = true; mPlateFrame = 2;
    } else if (mAnmNo == 12 && mpMorf->checkFrame(1.0f)) {
        mDrawPlate = false; setAnm(1, -1.0f);
    } else if (mAnmNo == 13 && mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) setAnm(4, -1.0f);
}

bool daNpc_Kg2_c::chkAttention(cXyz pos, s16 angle) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 distance = l_HIO.mNpc.mMaxAttnDistXZ;
    int range = l_HIO.mNpc.mMaxAttnAngleY;
    cXyz delta;
    delta.x = player->current.pos.x - pos.x;
    delta.z = player->current.pos.z - pos.z;
    f32 length = std::sqrtf(delta.x * delta.x + delta.z * delta.z);
    s16 target = cM_atan2s(delta.x, delta.z);
    if (mHasAttention) { distance += 40.0f; range += 0x71C; }
    target -= angle;
    bool result = false;
    if (range > abs(target) && distance > length) result = true;
    return result;
}

void daNpc_Kg2_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    } else if (mOrder == 3) fopAcM_orderOtherEventId(this, mEventIdx[mEventNo]);
}
void daNpc_Kg2_c::checkOrder() {
    if (eventInfo.getCommand() == dEvtCmd_INDEMO_e) {
        setAction(&daNpc_Kg2_c::event_wait_action, NULL); mOrder = 0;
    } else if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0; mTalking = true;
    }
}

u32 daNpc_Kg2_c::getMsg() {
    if (mNextMsg != 0) {
        u8 wins = dComIfGs_getEventReg(0xB703);
        if (mNextMsg == 0x3145) {
            if (mPlayed == 1) mNextMsg = 0x314B;
            else if (dComIfGs_isEventBit(0x2508)) mNextMsg = 0x314C;
        } else if (mNextMsg == 0x3150) {
            if (wins != 0) mNextMsg = 0x3154;
        } else if (mNextMsg == 0x3152) {
            if (wins == 1) mNextMsg = 0x3156;
            else if (wins >= 2) mNextMsg = 0x3158;
        }
        u32 msg = mNextMsg; mNextMsg = 0; return msg;
    }
    if (mSleeping == 1) return 0x315B;
    if (dComIfGs_isEventBit(0x2540) && !dComIfGs_isEventBit(0x2520)) {
        dComIfGs_onEventBit(0x2520); return 0x3140;
    }
    if (mGreeted == 0) { mGreeted = 1; return 0x3139; }
    return 0x313A;
}

u16 daNpc_Kg2_c::next_msgStatus(u32* msg) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    switch (*msg) {
    case 0x3140: case 0x3145: case 0x3146: case 0x3147: case 0x3148:
    case 0x314C: case 0x314E: case 0x3150: case 0x3152: case 0x3154: case 0x3156: case 0x3158:
        (*msg)++; break;
    case 0x314D: *msg = 0x3147; break;
    case 0x3139:
        if (dComIfGs_getEventReg(0xB703) == 0) *msg = 0x313E;
        else if ((int)dLib_getIplDaysFromSaveTime() < 4) *msg = 0x313B;
        else if (dComIfGs_isTmpBit(0x102)) *msg = 0x313C;
        else { dComIfGs_onTmpBit(0x102); *msg = 0x313D; }
        break;
    case 0x3149:
        if (mpCurrMsg->mSelectNum == 0) { dComIfGs_onEventBit(0x2508); mPlayed = 1; *msg = 0x314B; }
        else *msg = 0x314A;
        break;
    case 0x314A: *msg = 0x3148; break;
    case 0x313A: case 0x313B: case 0x313C: case 0x313D: case 0x313E: *msg = 0x313F; break;
    case 0x313F: case 0x314F:
        if (mpCurrMsg->mSelectNum == 0) {
            int cost = 50;
            if (dComIfGs_getRupee() < cost) *msg = 0x3143;
            else { dComIfGp_setItemRupeeCount(-cost); dComIfGp_setAStatusForce(25); *msg = 0x3144; }
        } else *msg = 0x3142;
        break;
    case 0x3141:
        if (mGreeted == 0) { mGreeted = 1; *msg = 0x3139; }
        else *msg = 0x313A;
        break;
    default: status = fopMsgStts_MSG_ENDS_e; break;
    }
    return status;
}

void daNpc_Kg2_c::anmAtr(u16 status) {
    u8 attr = dComIfGp_getMesgAnimeAttrInfo();
    if (mSleeping == 1) return;
    switch (attr) {
    case 0: setAnm(0, -1.0f); mBtpNo = 0; initTexPatternAnm(true); break;
    case 1: setAnm(1, -1.0f); mBtpNo = 0; initTexPatternAnm(true); break;
    case 2: setAnm(2, -1.0f); mBtpNo = 1; initTexPatternAnm(true); break;
    case 3: setAnm(3, -1.0f); mBtpNo = 0; initTexPatternAnm(true); break;
    case 4: setAnm(5, -1.0f); mBtpNo = 1; initTexPatternAnm(true); break;
    case 5: setAnm(6, -1.0f); mBtpNo = 1; initTexPatternAnm(true); break;
    case 6: setAnm(7, -1.0f); mBtpNo = 2; initTexPatternAnm(true); break;
    case 7: setAnm(8, -1.0f); mBtpNo = 3; initTexPatternAnm(true); break;
    case 8: setAnm(9, -1.0f); mBtpNo = 0; initTexPatternAnm(true); break;
    case 9: setAnm(10, -1.0f); mBtpNo = 1; initTexPatternAnm(true); break;
    case 10: setAnm(11, -1.0f); mBtpNo = 2; initTexPatternAnm(true); break;
    case 11: setAnm(14, -1.0f); mBtpNo = 2; initTexPatternAnm(true); break;
    }
    if (mAnmNo == 3 && mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) {
        setAnm(4, -1.0f); mBtpNo = 0; initTexPatternAnm(true);
    }
    dComIfGp_clearMesgAnimeAttrInfo();
}

void daNpc_Kg2_c::setAttention() {
    eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
}
void daNpc_Kg2_c::lookBack() {
    cXyz target;
    cXyz base(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    switch (mState) {
    case 1: case 2:
        if (mState == 2) m_jnt.setTrn();
        if (mHasAttention) {
            target = dNpc_playerEyePos(l_HIO.mNpc.m04);
            targetP = &target;
            base = current.pos; base.y = eyePos.y;
        }
        break;
    }
    if (m_jnt.trnChk()) cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mNpc.mMaxHeadTurnVel, 4, 0x800);
    else mTurnSpeed = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, true);
}

static BOOL CallbackCreateHeap(fopAc_ac_c* actor) { return ((daNpc_Kg2_c*)actor)->CreateHeap(); }
BOOL daNpc_Kg2_c::CreateHeap() {
    J3DModelData* modelData;
    J3DAnmTexPattern* plateBtp;
    J3DModelData* data;
    modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes("Kg", dRes_INDEX_KG_BDL_KG_e));
    JUT_ASSERT(DEMO_SELECT(0x390, 0x391), modelData != 0);
    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("Kg", dRes_INDEX_KG_BCK_KG_WAIT02_e),
        2, 1.0f, 0, -1, 1, NULL, 0, 0x11020203);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) return FALSE;
    m_jnt.setHeadJntNum(modelData->getJointName()->getIndex("head"));
    JUT_ASSERT(DEMO_SELECT(0x3A0, 0x3A1), m_jnt.getHeadJntNum() >= 0);
    m_jnt.setBackboneJntNum(modelData->getJointName()->getIndex("backbone2"));
    JUT_ASSERT(DEMO_SELECT(0x3A5, 0x3A6), m_jnt.getBackboneJntNum() >= 0);
    m_handL_num = modelData->getJointName()->getIndex("handL");
    JUT_ASSERT(DEMO_SELECT(0x3A9, 0x3AA), m_handL_num >= 0);
    mBtpNo = 0;
    if (!initTexPatternAnm(false)) return FALSE;
    // The plate has its own texture animation for the faces Salvatore holds up.
    {
        J3DModelData* model_data = static_cast<J3DModelData*>(dComIfG_getObjectRes("Kg", dRes_INDEX_KG_BDL_KG_PLATE_e));
        mpPlateModel = mDoExt_J3DModel__create(model_data, 0x80000, 0x11020002);
        if (mpPlateModel == NULL) return FALSE;
        plateBtp = static_cast<J3DAnmTexPattern*>(dComIfG_getObjectRes("Kg", dRes_INDEX_KG_BTP_KG_PLATE_e));
        if (!mPlateBtp.init(model_data, plateBtp, true, 2, 0.0f, 0, -1, false, 0)) return FALSE;
    }
    data = mpMorf->getModel()->getModelData();
    data->getJointNodePointer(m_jnt.getHeadJntNum())->setCallBack(nodeCallBack);
    data->getJointNodePointer(m_jnt.getBackboneJntNum())->setCallBack(nodeCallBack);
    mpMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 0.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return TRUE;
}

BOOL daNpc_Kg2_c::CreateInit() {
    mInitialAngle = current.angle;
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 110;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 110;
    gravity = -30.0f;
    setAction(&daNpc_Kg2_c::wait_action, NULL);
    mEyePos = current.pos;
    mStts.Init(255, 255, this);
    mCyl.Set(l_cyl_src); mCyl.SetStts(&mStts);
    setCollision(60.0f, 150.0f);
    mEventCut.setActorInfo2("Kg2", this);
    mEventCut.setJntCtrlPtr(&m_jnt);
    mGreeted = 0; mPlayed = 0; mDrawPlate = false; mPlateFrame = 0; mNextMsg = 0;
    m_jnt.onHeadLock(); m_jnt.onBackBoneLock(); mOrder = 0; mEventNo = 4;
    mEventIdx[0] = dComIfGp_evmng_getEventIdx("KG2_START_DEMO");
    mEventIdx[1] = dComIfGp_evmng_getEventIdx("KG2_CLEAR_DEMO");
    mEventIdx[2] = dComIfGp_evmng_getEventIdx("KG2_GETDEMO");
    mEventIdx[3] = dComIfGp_evmng_getEventIdx("CANON_GAME");
    set_mtx();
    if (dComIfGs_getTime() >= 105.0f && dComIfGs_getTime() < 300.0f) { mSleeping = 0; setAnm(1, -1.0f); }
    else { mSleeping = 1; setAnm(13, 0.0f); }
    l_kg2_pointer = this;
    return TRUE;
}

void daNpc_Kg2_c::wait01() {
    if (dComIfGs_getTime() >= 105.0f && dComIfGs_getTime() < 300.0f) {
        if (mSleeping == 1) { mSleeping = 0; setAnm(12, -1.0f); mBtpNo = 0; initTexPatternAnm(true); }
    } else if (mSleeping == 0) { mSleeping = 1; setAnm(13, -1.0f); mBtpNo = 0; initTexPatternAnm(true); }
    if (mTalking) mState = 2;
    else {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        f32 height = fabs(current.pos.y - player->current.pos.y);
        if (mHasAttention && height < 50.0f && mAnmNo != 13 && mAnmNo != 12) mOrder = 2;
    }
}
void daNpc_Kg2_c::talk01() {
    if (talk(1) == 18) {
        mState = 1; dComIfGp_event_reset(); mTalking = false;
        if (mCurrMsgNo == 0x3144) { mOrder = 3; mEventNo = 0; }
        else if (mSleeping == 0) { setAnm(1, -1.0f); mBtpNo = 0; initTexPatternAnm(true); }
        else setAnm(4, -1.0f);
    }
}
BOOL daNpc_Kg2_c::evn_setAnm_init(int staff) {
    int* anm = dComIfGp_evmng_getMyIntegerP(staff, "AnmNo");
    int* count = dComIfGp_evmng_getMyIntegerP(staff, "count");
    f32* morf = dComIfGp_evmng_getMyFloatP(staff, "hokan");
    if (anm != NULL) {
        f32 blend = -1.0f;
        if (morf != NULL) blend = *morf;
        setAnm(*anm, blend);
        if (count != NULL) mAnmLoops = *count;
        else mAnmLoops = 0;
    }
    return TRUE;
}
BOOL daNpc_Kg2_c::evn_setAnm() {
    if (mAnmNo == 12) { if (mpMorf->checkFrame(1.0f)) mAnmLoops--; }
    else if (mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) mAnmLoops--;
    return mAnmLoops <= 0 ? TRUE : FALSE;
}
BOOL daNpc_Kg2_c::evn_jnt_lock_init(int staff) {
    int* value = dComIfGp_evmng_getMyIntegerP(staff, "prm");
    int mode = 0;
    if (value != NULL) mode = *value;
    switch (mode) {
    case 0: m_jnt.offHeadLock(); m_jnt.offBackBoneLock(); break;
    case 1: m_jnt.onHeadLock(); m_jnt.offBackBoneLock(); break;
    case 2: m_jnt.offHeadLock(); m_jnt.onBackBoneLock(); break;
    case 3: m_jnt.onHeadLock(); m_jnt.onBackBoneLock(); break;
    }
    return TRUE;
}
BOOL daNpc_Kg2_c::evn_talk_init(int staff) {
    int* msg = dComIfGp_evmng_getMyIntegerP(staff, "msg_num");
    mCurrMsgBsPcId = fpcM_ERROR_PROCESS_ID_e; mpCurrMsg = NULL;
    if (msg != NULL) mNextMsg = *msg;
    else mNextMsg = 0;
    return TRUE;
}
BOOL daNpc_Kg2_c::evn_talk() { return talk(1) == 18 ? TRUE : FALSE; }
BOOL daNpc_Kg2_c::evn_createItem_init(int staff) {
    u8 item;
    switch (dComIfGs_getEventReg(0xB703)) {
    case 0:
    case 1:
        item = 7;
        break;
    case 2:
        item = 0xD7;
        break;
    case 3:
    default:
        item = 6;
        break;
    }
    fpc_ProcID id = fopAcM_createItemForPresentDemo(&current.pos, item, 0, -1, DEMO_SELECT(-1, fopAcM_GetRoomNo(this)), NULL, NULL);
    if (id != fpcM_ERROR_PROCESS_ID_e) dComIfGp_event_setItemPartnerId(id);
    return TRUE;
}
BOOL daNpc_Kg2_c::privateCut() {
    static char* cut_name_tbl[] = {"SETANM", "JNTLOCK", "SHOWITEM", "KGTALK", "CREATEITEM"};
    int staff = dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
    if (staff == -1) return FALSE;
    int cut = dComIfGp_evmng_getMyActIdx(staff, cut_name_tbl, 5, TRUE, FALSE);
    if (cut == -1) dComIfGp_evmng_cutEnd(staff);
    else {
        if (dComIfGp_evmng_getIsAddvance(staff)) {
            switch (cut) {
            case 0: evn_setAnm_init(staff); break;
            case 1: evn_jnt_lock_init(staff); break;
            case 2: break;
            case 3: evn_talk_init(staff); break;
            case 4: evn_createItem_init(staff); break;
            }
        }
        BOOL done;
        switch (cut) {
        case 0: done = evn_setAnm(); break;
        case 2: done = TRUE; break;
        case 3: done = evn_talk(); break;
        default: done = TRUE; break;
        }
        if (done) dComIfGp_evmng_cutEnd(staff);
    }
    return TRUE;
}
BOOL daNpc_Kg2_c::processMove() {
    (this->*mAction)(NULL);
    if (mEventCut.cutProc() || privateCut()) return TRUE;
    return FALSE;
}
int daNpc_Kg2_c::wait_action(void* arg) {
    if (mActionState == 0) { mState = 1; mActionState++; }
    else if (mActionState != -1) {
        s16 angle = current.angle.y + m_jnt.getHead_y() + m_jnt.getBackbone_y();
        mHasAttention = chkAttention(current.pos, angle);
        mOrder = 0;
        switch (mState) { case 1: wait01(); break; case 2: talk01(); break; }
        lookBack(); setAttention();
    }
    return 1;
}
int daNpc_Kg2_c::event_wait_action(void* arg) {
    if (mActionState == 0) { mState = 1; mActionState++; }
    else if (mActionState != -1) {
        s16 angle = current.angle.y + m_jnt.getHead_y() + m_jnt.getBackbone_y();
        mHasAttention = chkAttention(current.pos, angle);
        lookBack(); setAttention();
        if (dComIfGp_evmng_endCheck(mEventIdx[mEventNo])) {
            if (mEventNo == 0) { mOrder = 3; mEventNo = 3; }
            else if (mEventNo == 3) {
                if ((u8)canon_game_result == 1) { mOrder = 3; mEventNo = 1; }
                else { mOrder = 1; mNextMsg = 0x314E; }
            } else if (mEventNo == 1) {
                mOrder = 3; mEventNo = 2;
                int wins = dComIfGs_getEventReg(0xB703) + 1;
                dComIfGs_setEventReg(0xB703, cLib_maxLimit(wins, 3));
            } else if (mEventNo == 2) { mOrder = 1; mNextMsg = 0x315A; }
            else mEventNo = 4;
            dComIfGp_event_reset();
            setAction(&daNpc_Kg2_c::wait_action, NULL);
        }
    }
    return 1;
}

cPhs_State daNpc_Kg2_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Kg");
    if (phase == cPhs_COMPLEATE_e) {
        fopAcM_SetupActor(this, daNpc_Kg2_c);
#else
    fopAcM_SetupActor(this, daNpc_Kg2_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Kg");
    if (phase == cPhs_COMPLEATE_e) {
#endif
        if (!fopAcM_entrySolidHeap(this, CallbackCreateHeap, 0x2D00)) return cPhs_ERROR_e;
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        if (l_HIO.mChild < 0) l_HIO.mChild = mDoHIO_createChild("海戦ゲーム親父２", &l_HIO);
        if (!CreateInit()) return cPhs_ERROR_e;
    }
    return phase;
}
BOOL daNpc_Kg2_c::_delete() {
    dComIfG_resDeleteDemo(&mPhs, "Kg");
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) mpMorf->stopZelAnime();
#else
    if (heap != NULL && mpMorf != NULL) mpMorf->stopZelAnime();
#endif
    l_kg2_pointer = NULL;
    if (l_HIO.mChild >= 0) { mDoHIO_deleteChild(l_HIO.mChild); l_HIO.mChild = -1; }
    return TRUE;
}
BOOL daNpc_Kg2_c::_execute() {
    m_jnt.setParam(l_HIO.mNpc.mMaxBackboneX, l_HIO.mNpc.mMaxBackboneY, l_HIO.mNpc.mMinBackboneX, l_HIO.mNpc.mMinBackboneY,
        l_HIO.mNpc.mMaxHeadX, l_HIO.mNpc.mMaxHeadY, l_HIO.mNpc.mMinHeadX, l_HIO.mNpc.mMinHeadY, l_HIO.mNpc.mMaxTurnStep);
    playTexPatternAnm();
    mpMorf->play(&eyePos, 0, 0); mpMorf->calc();
    checkOrder(); processMove(); eventOrder(); subAnm();
    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    set_mtx(); setCollision(60.0f, 150.0f);
    return TRUE;
}
BOOL daNpc_Kg2_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtpAnm.entry(data, mBtpFrame); mpMorf->entryDL(); mBtpAnm.remove(data);
    if (mDrawPlate) {
        J3DModelData* plateData = mpPlateModel->getModelData();
        g_env_light.setLightTevColorType(mpPlateModel, &tevStr);
        mPlateBtp.entry(plateData, mPlateFrame); mDoExt_modelUpdateDL(mpPlateModel); mPlateBtp.remove(plateData);
    }
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 20.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    dSnap_RegistFig(0x81, this, current.pos, current.angle.y, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 00002E74-00002E94       .text daNpc_Kg2_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Kg2_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Kg2_c*)i_this)->_create();
}

/* 00002E94-00002EB4       .text daNpc_Kg2_Delete__FP11daNpc_Kg2_c */
static BOOL daNpc_Kg2_Delete(daNpc_Kg2_c* i_this) {
    return ((daNpc_Kg2_c*)i_this)->_delete();
}

/* 00002EB4-00002ED4       .text daNpc_Kg2_Execute__FP11daNpc_Kg2_c */
static BOOL daNpc_Kg2_Execute(daNpc_Kg2_c* i_this) {
    return ((daNpc_Kg2_c*)i_this)->_execute();
}

/* 00002ED4-00002EF4       .text daNpc_Kg2_Draw__FP11daNpc_Kg2_c */
static BOOL daNpc_Kg2_Draw(daNpc_Kg2_c* i_this) {
    return ((daNpc_Kg2_c*)i_this)->_draw();
}

/* 00002EF4-00002EFC       .text daNpc_Kg2_IsDelete__FP11daNpc_Kg2_c */
static BOOL daNpc_Kg2_IsDelete(daNpc_Kg2_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Kg2_Method = {
    (process_method_func)daNpc_Kg2_Create,
    (process_method_func)daNpc_Kg2_Delete,
    (process_method_func)daNpc_Kg2_Execute,
    (process_method_func)daNpc_Kg2_IsDelete,
    (process_method_func)daNpc_Kg2_Draw,
};

actor_process_profile_definition g_profile_NPC_KG2 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_KG2_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Kg2_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_KG2_e,
    /* Actor SubMtd */ &l_daNpc_Kg2_Method,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_12_e,
};
