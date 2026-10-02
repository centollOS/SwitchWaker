/**
 * d_a_npc_kg1.cpp
 * NPC - Salvatore (Squid-Hunt/"Sploosh Kaboom" Minigame)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_kg1.h"
#include "d/d_lib.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_camera.h"
#include "f_op/f_op_camera.h"
#include "d/d_snap.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_mgameboard.h"
#include "m_Do/m_Do_controller_pad.h"
#include "res/Object/Kg.h"

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
        /* Radius */ 40.0f,
        /* Height */ 160.0f,
    }},
};

class daNpc_Kg1_HIO_c : public JORReflexible {
public:
    daNpc_Kg1_HIO_c();

public:
    virtual ~daNpc_Kg1_HIO_c() {}
    s8 mChild;
    int mCount;
    u8 m0C;
    dNpc_HIO_c mNpc[1];
    u8 m38;
    u8 m39;
};

static daNpc_Kg1_HIO_c l_HIO;

daNpc_Kg1_HIO_c::daNpc_Kg1_HIO_c() {
    mChild = -1;
    mCount = -1;
    mNpc[0].m04 = 0.0f;
    mNpc[0].mMaxHeadX = 2500;
    mNpc[0].mMaxHeadY = 7000;
    mNpc[0].mMaxBackboneX = 2000;
    mNpc[0].mMaxBackboneY = 8000;
    mNpc[0].mMinHeadX = -2500;
    mNpc[0].mMinHeadY = -7000;
    mNpc[0].mMinBackboneX = -2000;
    mNpc[0].mMinBackboneY = -8000;
    mNpc[0].mMaxTurnStep = 1000;
    mNpc[0].mMaxHeadTurnVel = 2000;
    mNpc[0].mAttnYOffset = 35.0f;
    mNpc[0].mMaxAttnAngleY = 0x4000;
    mNpc[0].m22 = 0;
    mNpc[0].mMaxAttnDistXZ = 400.0f;
    m0C = 0;
    m38 = 0;
    m39 = 0;
}

const char daNpc_Kg1_c::m_arcname[3] = "Kg";
const f32 daNpc_Kg1_c::m_camera_fovy = 40.0f;
static const int l_btp_ix_tbl[] = {
    dRes_INDEX_KG_BTP_KG_e,
    dRes_INDEX_KG_BTP_MOPEN_e,
    dRes_INDEX_KG_BTP_WARAI_e,
    dRes_INDEX_KG_BTP_RADIO_e,
};
cXyz daNpc_Kg1_c::m_camera_ctr(-200.0f, 140.0f, 85.0f);
cXyz daNpc_Kg1_c::m_camera_eye(-117.0f, 92.0f, 344.0f);

static BOOL daNpc_Kg1_nodeCallBack(J3DNode* node, int phase) {
    J3DModel* model;
    daNpc_Kg1_c* i_this;
    int joint;
    if (phase == 0) {
        model = j3dSys.getModel();
        i_this = (daNpc_Kg1_c*)model->getUserArea();
        joint = ((J3DJoint*)node)->getJntNo();
        mDoMtx_stack_c::copy(model->getAnmMtx(joint));
        if (joint == i_this->m_jnt.getHeadJntNum()) {
            mDoMtx_stack_c::XrotM((s16)i_this->m_jnt.getHead_y());
            mDoMtx_stack_c::ZrotM(-i_this->m_jnt.getHead_x());
            static cXyz l_offsetAttPos(24.0f, 5.0f, 0.0f);
            static cXyz l_offsetEyePos(24.0f, -16.0f, 0.0f);
            mDoMtx_stack_c::multVec(&l_offsetAttPos, i_this->getHeadPosP());
            mDoMtx_stack_c::XrotM((s16)i_this->m_jnt.getHead_y());
            mDoMtx_stack_c::ZrotM(-i_this->m_jnt.getHead_x());
            mDoMtx_stack_c::multVec(&l_offsetEyePos, i_this->getLookPosP());
            mDoMtx_stack_c::multVec(&l_offsetAttPos, &i_this->attention_info.position);
            i_this->attention_info.position.y += l_HIO.mNpc[0].mAttnYOffset;
        }
        if (joint == i_this->m_jnt.getBackboneJntNum()) {
            mDoMtx_stack_c::XrotM((s16)i_this->m_jnt.getBackbone_y());
            mDoMtx_stack_c::ZrotM(-i_this->m_jnt.getBackbone_x());
        }
        model->setAnmMtx(joint, mDoMtx_stack_c::get());
        PSMTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        if (joint == 8) {
            mDoMtx_stack_c::transM(23.46f, -22.26f, -47.05f);
#if VERSION == VERSION_DEMO
            // The demo passes the unwrapped value 0xB100 here, which only a float -> s16 conversion reproduces.
            mDoMtx_stack_c::XYZrotM(0x1F4B, cM_deg2s(248.90625f), 0x1F4B);
#else
            mDoMtx_stack_c::XYZrotM(0x1F4B, -0x4F00, 0x1F4B);
#endif
            i_this->m6C4->setBaseTRMtx(mDoMtx_stack_c::get());
        }
    }
    return TRUE;
}

void daNpc_Kg1_c::lookBack() {
    cXyz target;
    f32 distance = (dComIfGp_getPlayer(0)->current.pos - current.pos).absXZ();
    cXyz* targetP = NULL;
    if (mEventCut.getAttnFlag()) {
        m_jnt.setTrn();
        target = mEventCut.getAttnPos();
        targetP = &target;
        m731 = 1;
    } else if (m74D == 8) {
        target = m_camera_eye;
        targetP = &target;
    } else if (distance < l_HIO.mNpc[0].mMaxAttnDistXZ) {
        target = dNpc_playerEyePos(l_HIO.mNpc[0].m04);
        targetP = &target;
        m731 = chkAttention();
    } else m731 = 0;
    if (m_jnt.trnChk()) {
        s16 speed = l_HIO.mNpc[0].mMaxHeadTurnVel;
        if (mEventCut.getTurnSpeed() != 0) speed = mEventCut.getTurnSpeed();
        cLib_addCalcAngleS2(&m72E, speed, 4, 0x800);
    } else m72E = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, eyePos, current.angle.y, m72E, true);
}

BOOL daNpc_Kg1_c::chkAttention() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz direction;
    f32 distance = (player->current.pos - current.pos).absXZ();
    f32 maxDist = l_HIO.mNpc[0].mMaxAttnDistXZ;
    s16 limit = l_HIO.mNpc[0].mMaxAttnAngleY;
    if (distance < maxDist) {
        direction.x = player->current.pos.x - current.pos.x;
        direction.z = player->current.pos.z - current.pos.z;
        s16 angle = cM_atan2s(direction.x, direction.z);
        angle -= (s16)(current.angle.y + m_jnt.getHead_y() + m_jnt.getBackbone_y());
        if ((s16)abs(angle) < limit) return TRUE;
    }
    return FALSE;
}

BOOL daNpc_Kg1_c::initTexPatternAnm(int index, bool modify) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    m_eye_tex_pattern = (J3DAnmTexPattern*)dComIfG_getObjectRes(m_arcname, l_btp_ix_tbl[index]);
    JUT_ASSERT(0x1CC, m_eye_tex_pattern != 0);
    if (!m6F8.init(data, m_eye_tex_pattern, TRUE, 2, 1.0f, 0, -1, modify, 0)) return FALSE;
    m720 = 0;
    m72C = 0;
    return TRUE;
}

void daNpc_Kg1_c::playTexPatternAnm() {
    if (cLib_calcTimer(&m72C) == 0) {
        if (m720 >= m_eye_tex_pattern->getFrameMax()) {
            m720 -= m_eye_tex_pattern->getFrameMax();
            m72C = 120;
        }
        else m720++;
    }
}

void daNpc_Kg1_c::set_mtx() {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    m6C4->setBaseScale(scale);
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Kg1_c*)actor)->CreateHeap();
}

BOOL daNpc_Kg1_c::CreateHeap() {
    J3DModelData* data = (J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_KG_BDL_KG_e);
    mpMorf = new mDoExt_McaMorf(data, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_KG_BCK_KG_WAIT01_e), 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) return FALSE;
    m_jnt.setHeadJntNum(data->getJointName()->getIndex("head"));
    JUT_ASSERT(0x223, m_jnt.getHeadJntNum() >= 0);
    m_jnt.setBackboneJntNum(data->getJointName()->getIndex("backbone2"));
    JUT_ASSERT(0x228, m_jnt.getBackboneJntNum() >= 0);
    if (!initTexPatternAnm(3, false)) return FALSE;
    initTexPatternAnm(0, true);
    m6C4 = mDoExt_J3DModel__create((J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_KG_BDL_KG_PLATE_e), 0x80000, 0x11020022);
    if (m6C4 == NULL) return FALSE;
    J3DAnmTexPattern* pattern = (J3DAnmTexPattern*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_KG_BTP_KG_PLATE_e);
    if (!mBtpAnm.init((J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_KG_BDL_KG_PLATE_e), pattern, TRUE, 2, 1.0f, 0, -1, false, 0)) return FALSE;
    for (u16 i = 0; i < data->getJointNum(); i++) {
        if (i == m_jnt.getHeadJntNum() || i == m_jnt.getBackboneJntNum() || i == 8) data->getJointNodePointer(i)->setCallBack(daNpc_Kg1_nodeCallBack);
    }
    mpMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 30.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), fopAcM_GetAngle_p(this), fopAcM_GetShapeAngle_p(this));
    return TRUE;
}

cPhs_State daNpc_Kg1_c::CreateInit() {
    mStts.Init(255, 255, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(60.0f, 150.0f);
    gravity = -9.0f;
    mEventCut.setActorInfo2("Auc0", this);
    set_mtx();
    m72E = 0;
    m730 = 0;
    m731 = 0;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    wait_action_init();
    if (l_HIO.mCount < 0) l_HIO.mChild = mDoHIO_createChild("海戦ゲーム屋", &l_HIO);
    l_HIO.mCount++;
    attention_info.flags = 10;
    m784 = dComIfGp_evmng_getEventIdx("MINIGAME_START");
    m786 = dComIfGp_evmng_getEventIdx("MINIGAME_END");
    m788 = dComIfGp_evmng_getEventIdx("KAISEN_GETITEM");
    m74E = 0;
    m74D = 2;
    setAnm();
    m_jnt.onHeadLock();
    m_jnt.onBackBoneLock();
    m750 = 1;
    mLookPos = current.pos;
    return cPhs_COMPLEATE_e;
}

void daNpc_Kg1_c::eventOrder() {
    if (m732 == 2 || m732 == 1) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (m732 == 2) fopAcM_orderSpeakEvent(this);
    } else if (m732 == 3) {
        fopAcM_orderOtherEventId(this, m784);
        eventInfo.onCondition(dEvtCnd_UNK2_e);
    } else if (m732 == 4) {
        fopAcM_orderOtherEventId(this, m786);
        eventInfo.onCondition(dEvtCnd_UNK2_e);
    } else if (m732 == 5) {
        fopAcM_orderOtherEventId(this, m788);
        eventInfo.onCondition(dEvtCnd_UNK2_e);
    }
}

void daNpc_Kg1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(m784) && m732 == 3) m732 = 0;
        if (dComIfGp_evmng_endCheck(m784)) m732 = 0;
        if (dComIfGp_evmng_startCheck(m788) && m732 == 5) m732 = 0;
    } else if (eventInfo.checkCommandTalk() && (m732 == 2 || m732 == 1)) m730 = 1;
}

void daNpc_Kg1_c::kg1_talk_camera() {
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    if (m751 != 0 && camera != NULL) {
        camera->mCamera.Stay();
        camera->mCamera.Set(m_camera_ctr, m_camera_eye, 40.0f, 0);
        camera->mCamera.Reset();
        camera->mCamera.SetTrimSize(1);
    }
}

void daNpc_Kg1_c::wait_action_init() {
    clr_seq_flag();
    setAction(&daNpc_Kg1_c::wait_action);
}

void daNpc_Kg1_c::wait_action() {
    int staff = dComIfGp_evmng_getMyStaffId("Kg1");
    fopAc_ac_c* board = fopAcM_SearchByName(fpcNm_MGBOARD_e);
    switch (mWaitMode) {
    case 0:
        m751 = 0;
        m_jnt.onHeadLock();
        m_jnt.onBackBoneLock();
        if (board) ((daMgBoard_c*)board)->clrGInfoDraw();
        clr_seq_flag();
        if (m731 != 0) m732 = 1;
        else m732 = 0;
        if (!m730) break;
        mWaitMode = 1;
        mBtpAnm.setFrame(0.0f);
    case 1: {
        u16 status = talk(1);
        if (mCurrMsgNo < 0x1D56 && m74D == 4) m_jnt.offHeadLock();
        else m_jnt.onHeadLock();
        if (mCurrMsgNo == 0x1D5B) m750 = 0;
        else if ((mCurrMsgNo == 0x1D5C && status == 6) || (mCurrMsgNo == 0x1D57 && status == 6)) m750 = 1;
        if (status == 0x12) {
            dComIfGp_event_reset();
            if (mStartGame) {
                m732 = 3;
                mWaitMode = 2;
                m751 = 0;
            }
            else {
                m74D = 2;
                mWaitMode = 0;
            }
            m730 = 0;
        }
        break;
    }
    case 2:
        m751 = 0;
        if (board) {
            ((daMgBoard_c*)board)->reqStartGame();
            ((daMgBoard_c*)board)->setGInfoDraw();
        }
        mWaitMode = 3;
        fopAcM_seStart(this, JA_SE_PLAYSPOT_START, 0);
        mEndGameTimer = 60;
        break;
    case 3:
        if (m732 == 0 && board) {
            mbGameEnd = ((daMgBoard_c*)board)->checkEndGame() != 0;
            if (mbGameEnd && cLib_calcTimer(&mEndGameTimer) == 0) {
                mbDidClearGame = ((daMgBoard_c*)board)->checkClearGame();
                if (mbDidClearGame) mGameBoardScore = ((daMgBoard_c*)board)->getScore();
                else m74D = 4;
                mEndGameTimer = 90;
                m778 = 0;
                mWaitMode = 4;
                dComIfGp_event_offHindFlag(2);
                m779 = 1;
            }
        }
        break;
    case 4:
        if (CPad_CHECK_TRIG_A(0)) {
            m779 = 0;
            dComIfGs_onEventBit(0x2540);
            if (board) ((daMgBoard_c*)board)->mbForceEnd = 1;
            if (board) ((daMgBoard_c*)board)->clrGInfoDraw();
            mWaitMode = 5;
            m732 = 2;
            dComIfGp_evmng_cutEnd(staff);
            dComIfGp_event_reset();
        }
        break;
    case 5:
        if (m730 == 1) {
            m751 = 1;
            if (talk(1) == 0x12) {
                m732 = 0;
                dComIfGp_event_reset();
                m730 = 0;
                if (mbDidClearGame) {
                    m732 = 5;
                    mWaitMode = 6;
                }
                else if (mStartGame) {
                    m732 = 3;
                    mWaitMode = 2;
                }
                else {
                    m74D = 2;
                    mWaitMode = 0;
                }
            }
        }
        break;
    case 6:
        if (m732 == 0) {
            m751 = 0;
            u8 items[] = {0x07, 0xCC, 0x05};
            u8 index = g_dComIfG_gameInfo.save.getEvent().getEventReg(0xFE07) - 1;
            mItemId = fopAcM_createItemForPresentDemo(&current.pos, items[index], 0, -1, fopAcM_GetRoomNo(this), NULL, NULL);
            if (mItemId != fpcM_ERROR_PROCESS_ID_e) dComIfGp_event_setItemPartnerId(mItemId);
            mWaitMode = 7;
        }
        break;
    case 7:
        if (dComIfGp_evmng_endCheck(m788)) {
            dComIfGp_event_reset();
            mPrizeGiven = 1;
            m732 = 2;
            mWaitMode = 8;
            m74D = 11;
        }
        break;
    case 8:
        m_jnt.onBackBoneLock();
        m_jnt.onHeadLock();
        if (m730 == 1) {
            m751 = 1;
            if (talk(1) == 0x12) {
                dComIfGp_event_reset();
                m730 = 0;
                m732 = 0;
                if (mNewRecord) {
                    m732 = 5;
                    mWaitMode = 9;
                }
                else {
                    m74D = 2;
                    mWaitMode = 0;
                }
            }
        }
        break;
    case 9:
        if (m732 == 0) {
            m751 = 0;
            u8 rec_items[] = {dItemNo_COLLECT_MAP_14_e, dItemNo_ORANGE_RUPEE_e};
            u8 index = dComIfGs_getEventReg(0xFF07) - 1;
            if (index > 1) index = 1;
            mItemId = fopAcM_createItemForPresentDemo(&current.pos, rec_items[index], 0, -1, fopAcM_GetRoomNo(this), NULL, NULL);
            if (mItemId != fpcM_ERROR_PROCESS_ID_e) dComIfGp_event_setItemPartnerId(mItemId);
            mWaitMode = 10;
        }
        break;
    case 10:
        if (dComIfGp_evmng_endCheck(m788)) {
            dComIfGp_event_reset();
            mRecordPrizeGiven = 1;
            m732 = 2;
            mWaitMode = 11;
        }
        break;
    case 11:
        if (m730 == 1) {
            m751 = 1;
            if (talk(1) == 0x12) {
                dComIfGp_event_reset();
                m732 = 0;
                m730 = 0;
                mWaitMode = 0;
                m74D = 2;
            }
        }
        break;
    }
    kg1_talk_camera();
}

void daNpc_Kg1_c::clr_seq_flag() {
    mStartGame = 0;
    mbDidClearGame = 0;
    mbGameEnd = 0;
    mPrizeGiven = 0;
    mRecordPrizeGiven = 0;
    mNewRecord = 0;
}

u32 daNpc_Kg1_c::getMsg() {
    u32 msg;
    if (mbGameEnd) {
        if (mbDidClearGame) {
            if (mPrizeGiven && !mRecordPrizeGiven) {
                if (dComIfGs_getEventReg(0xBEFF) > mGameBoardScore) {
                    dComIfGs_onEventBit(0xE04);
                    dComIfGs_setEventReg(0xBEFF, mGameBoardScore);
                    u8 count = dComIfGs_getEventReg(0xFF07);
                    if (count < 3) {
                        count++;
                        g_dComIfG_gameInfo.save.getEvent().setEventReg(0xFF07, count);
                    }
                    mNewRecord = 1;
                    msg = 0x1D63;
                } else msg = 0x1D64;
            } else if (mPrizeGiven && mRecordPrizeGiven) msg = 0x1D64;
            else msg = 0x1D5F;
        } else msg = 0x1D5D;
    } else if (!mMet) msg = 0x1D4D;
    else msg = 0x1D4E;
    return msg;
}

u16 daNpc_Kg1_c::next_msgStatus(u32* msg) {
    u16 status = 0xF;
    daPy_py_c* player = daPy_getPlayerActorClass();
    switch (*msg) {
    case 0x1D4D:
#if VERSION == VERSION_DEMO
        u8 flag = 0;
        if (dComIfGs_isEventBit(0xE04)) {
            if (flag || (s32)dLib_getIplDaysFromSaveTime() < 4) {
                if (dComIfGs_isTmpBit(0x101)) *msg = 0x1D50;
                else {
                    dComIfGs_onTmpBit(0x101);
                    *msg = 0x1D51;
                }
            } else *msg = 0x1D4F;
        } else *msg = 0x1D52;
#else
        if (!dComIfGs_isEventBit(0xE04)) *msg = 0x1D52;
        else if ((s32)dLib_getIplDaysFromSaveTime() < 4) *msg = 0x1D4F;
        else if (dComIfGs_isTmpBit(0x101)) *msg = 0x1D50;
        else {
            dComIfGs_onTmpBit(0x101);
            *msg = 0x1D51;
        }
#endif
        mMet = 1;
        break;
    case 0x1D4E: case 0x1D4F: case 0x1D50: case 0x1D51:
        *msg = 0x1D52;
        break;
    case 0x1D52: case 0x1D5E:
        clr_seq_flag();
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            if ((u32)dComIfGs_getRupee() < 10) *msg = 0x1D54;
            else {
                m74D = 1;
                dComIfGp_setItemRupeeCount(-10);
                m751 = 1;
                m_jnt.onHeadLock();
                cXyz pos(0.0f, 0.0f, 250.0f);
                player->setPlayerPosAndAngle(&pos, 0x2000);
                *msg = 0x1D55;
            }
            break;
        case 1: *msg = 0x1D53; break;
        }
        break;
    case 0x1D55:
        if (!mExplained) *msg = 0x1D56;
        else *msg = 0x1D5C;
        mStartGame = 1;
        break;
    case 0x1D56: case 0x1D57: case 0x1D58: case 0x1D59: case 0x1D5D:
        (*msg)++;
        break;
    case 0x1D5A:
        switch (mpCurrMsg->mSelectNum) {
        case 0: *msg = 0x1D5C; break;
        case 1: *msg = 0x1D5B; break;
        }
        break;
    case 0x1D5B: *msg = 0x1D57; break;
    case 0x1D5F: {
        u8 count = dComIfGs_getEventReg(0xFE07);
        switch (count) {
        case 0: *msg = 0x1D60; break;
        case 1: *msg = 0x1D61; break;
        default: *msg = 0x1D62;
        break;
        }
        if (count < 3) count++;
        dComIfGs_setEventReg(0xFE07, count);
        break;
    }
    case 0x1D5C: mExplained = 1; status = 0x10; break;
    default: status = 0x10;
    break;
    }
    return status;
}

void daNpc_Kg1_c::anmAtr(u16 status) {
    static const u8 anm_atr[] = {1, 2, 4, 5, 6, 7, 8, 9, 10};
    if (status == 6 && dComIfGp_getMesgAnimeAttrInfo() < 9) {
        m74D = anm_atr[dComIfGp_getMesgAnimeAttrInfo()];
        dComIfGp_clearMesgAnimeAttrInfo();
    }
}

static void daNpc_Kg1_bcks_setAnm(const char* arc, mDoExt_McaMorf* morf, s8* anm, s8* index, s8* previous, const int* table, const dLib_anm_prm_c* params) {
    if (*previous != *index && params[*index].mAnmIdx != -1) {
        *anm = params[*index].mAnmIdx;
        morf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes(arc, table[*anm]), params[*index].mLoopMode, params[*index].mMorf, params[*index].mPlaySpeed, 0.0f, -1.0f, NULL);
        if (params[*index].mPlaySpeed < 0.0f) morf->setFrame(morf->getEndFrame());
    }
    *previous = *index;
    if (morf->isStop() && params[*index].mNextPrmIdx != -1 && params[*index].mLoopMode == 0) *index = params[*index].mNextPrmIdx;
}

void daNpc_Kg1_c::setAnm() {
    static const int a_anm_bck_tbl[] = {
        dRes_INDEX_KG_BCK_KG_WAIT01_e,
        dRes_INDEX_KG_BCK_KG_WAIT02_e,
        dRes_INDEX_KG_BCK_KG_KWAIT_e,
        dRes_INDEX_KG_BCK_KG_TALK01_e,
        dRes_INDEX_KG_BCK_KG_KAMAE_e,
        dRes_INDEX_KG_BCK_KG_CAPTAIN_e,
        dRes_INDEX_KG_BCK_KG_CAPTAIN02_e,
        dRes_INDEX_KG_BCK_KG_KIDS_e,
        dRes_INDEX_KG_BCK_KG_RADIO_e,
        dRes_INDEX_KG_BCK_KG_SASIKAE_e,
    };
    static const s16 a_anm_btp_tbl[] = {0,0,0,0,1,0,1,1,2,3,0,0};
    dLib_anm_prm_c params[] = {
        {-1,-1,0,0.0f,0.0f,0}, {0,-1,0,8.0f,1.0f,2}, {1,-1,0,8.0f,1.0f,2},
        {2,-1,0,8.0f,1.0f,2}, {3,-1,0,8.0f,1.0f,2}, {4,3,0,8.0f,1.0f,0},
        {5,-1,0,8.0f,1.0f,2}, {6,-1,0,8.0f,1.0f,2}, {7,-1,0,6.0f,1.0f,2},
        {8,-1,0,6.0f,1.0f,2}, {9,8,0,8.0f,1.0f,0}, {4,3,0,8.0f,-1.0f,0},
    };
    if (m74D != m74E) initTexPatternAnm(a_anm_btp_tbl[m74D], true);
    if (mAnm == 4 || mAnm == 2 || mAnm == 5 || mAnm == 6 || mAnm == 7 || mAnm == 8 || mAnm == 9) {
        mDrawProp = 1;
#if VERSION == VERSION_DEMO
        if (m74D == 10 && mpMorf->getFrame() == 30.0f) mBtpAnm.setFrame(1.0f);
#else
        if (mAnm == 9 && mpMorf->getFrame() == 31.0f) mBtpAnm.setFrame(1.0f);
#endif
    } else mDrawProp = 0;
    daNpc_Kg1_bcks_setAnm(m_arcname, mpMorf, &mAnm, &m74D, &m74E, a_anm_bck_tbl, params);
}

static cPhs_State daNpc_Kg1Create(void* actor) {
    return ((daNpc_Kg1_c*)actor)->_create();
}
cPhs_State daNpc_Kg1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Kg1_c);
#endif
    cPhs_State phase = dComIfG_resLoad(&mPhs, m_arcname);
    if (phase == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_SetupActor(this, daNpc_Kg1_c);
#endif
        if (fopAcM_entrySolidHeap(this, CheckCreateHeap, 0x10000)) return CreateInit();
        return cPhs_ERROR_e;
    }
    return phase;
}

static BOOL daNpc_Kg1Delete(void* actor) {
    daNpc_Kg1_c* i_this = (daNpc_Kg1_c*)actor;
#if VERSION == VERSION_DEMO
    dComIfG_deleteObjectRes(daNpc_Kg1_c::m_arcname);
    if (i_this->mpMorf != NULL) i_this->mpMorf->stopZelAnime();
#else
    dComIfG_resDelete(&i_this->mPhs, daNpc_Kg1_c::m_arcname);
    if (i_this->heap != NULL && i_this->mpMorf != NULL) i_this->mpMorf->stopZelAnime();
#endif
    if (l_HIO.mCount >= 0 && --l_HIO.mCount < 0) mDoHIO_deleteChild(l_HIO.mChild);
    return TRUE;
}

inline bool daNpc_Kg1_c::_execute() {
    m_jnt.setParam(l_HIO.mNpc[0].mMaxBackboneX, l_HIO.mNpc[0].mMaxBackboneY, l_HIO.mNpc[0].mMinBackboneX, l_HIO.mNpc[0].mMinBackboneY, l_HIO.mNpc[0].mMaxHeadX, l_HIO.mNpc[0].mMaxHeadY, l_HIO.mNpc[0].mMinHeadX, l_HIO.mNpc[0].mMinHeadY, l_HIO.mNpc[0].mMaxTurnStep);
    checkOrder();
    (this->*mAction)();
    eventOrder();
    playTexPatternAnm();
    setAnm();
    if (m750 != 0) mpMorf->play(NULL, 0, 0);
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    setCollision(60.0f, 150.0f);
    getAttentionBasePos();
    getEyePos();
    lookBack();
    set_mtx();
    return FALSE;
}

static BOOL daNpc_Kg1Execute(void* actor) {
    return ((daNpc_Kg1_c*)actor)->_execute();
}

inline bool daNpc_Kg1_c::_draw() {
    dSnap_RegistFig(0x81, this, 1.0f, 1.0f, 1.0f);
    J3DModelData* data = mpMorf->getModel()->getModelData();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpMorf->getModel(), &tevStr);
    g_env_light.setLightTevColorType(m6C4, &tevStr);
    m6F8.entry(data, m720);
    mpMorf->updateDL();
    m6F8.remove(data);
    if (mDrawProp) {
        J3DModelData* propData = m6C4->getModelData();
        mBtpAnm.entry(propData, mBtpAnm.getFrame());
        mDoExt_modelUpdateDL(m6C4);
    }
    cXyz pos(current.pos.x, current.pos.y + 150.0f, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 20.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr);
    if (m779) dComIfGp_setDoStatusForce(0x19);
    return TRUE;
}

static BOOL daNpc_Kg1Draw(void* actor) {
    return ((daNpc_Kg1_c*)actor)->_draw();
}

static BOOL daNpc_Kg1IsDelete(void*) {
    return TRUE;
}

static actor_method_class daNpc_Kg1MethodTable = {
    (process_method_func)daNpc_Kg1Create,
    (process_method_func)daNpc_Kg1Delete,
    (process_method_func)daNpc_Kg1Execute,
    (process_method_func)daNpc_Kg1IsDelete,
    (process_method_func)daNpc_Kg1Draw,
};

actor_process_profile_definition g_profile_NPC_KG1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_KG1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Kg1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_KG1_e,
    /* Actor SubMtd */ &daNpc_Kg1MethodTable,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
