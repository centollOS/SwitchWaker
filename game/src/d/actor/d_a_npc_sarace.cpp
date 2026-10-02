/**
 * d_a_npc_sarace.cpp
 * NPC - Loot the Sailor (Boating Course)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_sarace.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "d/actor/d_a_obj_barrel2.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Sarace.h"

class daNpc_Sarace_HIO_c : public JORReflexible {
public:
    daNpc_Sarace_HIO_c();
    virtual ~daNpc_Sarace_HIO_c() {}
    void genMessage(JORMContext*) {}
    s8 mNo;
    dNpc_HIO_c mPrm;
    f32 mTalkMorf;
    f32 mOtherMorf;
};

static daNpc_Sarace_HIO_c l_HIO;

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

/* 000000EC-00000198       .text __ct__18daNpc_Sarace_HIO_cFv */
daNpc_Sarace_HIO_c::daNpc_Sarace_HIO_c() {
    mPrm.m04 = -20.0f;
    mPrm.mMaxHeadX = 4000;
    mPrm.mMaxHeadY = 6000;
    mPrm.mMaxBackboneX = 0;
    mPrm.mMaxBackboneY = 7000;
    mPrm.mMinHeadX = -2000;
    mPrm.mMinHeadY = -6000;
    mPrm.mMinBackboneX = -2000;
    mPrm.mMinBackboneY = -7000;
    mPrm.mMaxTurnStep = 1000;
    mPrm.mMaxHeadTurnVel = 1600;
    mPrm.mAttnYOffset = 45.0f;
    mPrm.mMaxAttnAngleY = 0x4000;
    mPrm.m22 = 0;
    mPrm.mMaxAttnDistXZ = 400.0f;
    mTalkMorf = 11.0f;
    mOtherMorf = 8.0f;
    mNo = -1;
}

static const int l_bck_ix_tbl[] = {
    dRes_INDEX_SARACE_BCK_SA_WAIT01_e, dRes_INDEX_SARACE_BCK_SA_TALK01_e, dRes_INDEX_SARACE_BCK_SA_TALK02_e,
};
static const int l_bck_head_ix_tbl[] = {
    dRes_INDEX_SARACE_BCK_SA01HEAD_WAIT01_e, dRes_INDEX_SARACE_BCK_SA01HEAD_TALK01_e, dRes_INDEX_SARACE_BCK_SA01HEAD_TALK02_e,
};
static const int l_btp_ix_tbl[] = {dRes_INDEX_SARACE_BTP_SA01_MABA_e};

/* 00000198-00000328       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Sarace_c* npc = (daNpc_Sarace_c*)model->getUserArea();
        if (npc != NULL) {
            int joint = ((J3DJoint*)node)->getJntNo();
            mDoMtx_stack_c::copy(model->getAnmMtx(joint));
            if (joint == npc->m_jnt.getHeadJntNum()) {
                static cXyz offset(0.0f, 0.0f, 0.0f);
                cXyz eyeOffset(24.0f, 14.0f, 0.0f);
                mDoMtx_stack_c::multVec(&offset, npc->getAttentionBasePos());
                mDoMtx_stack_c::XrotM((s16)npc->m_jnt.getHead_y());
                mDoMtx_stack_c::ZrotM(-npc->m_jnt.getHead_x());
                mDoMtx_stack_c::multVec(&eyeOffset, npc->getEyePos());
            } else if (joint == npc->m_jnt.getBackboneJntNum()) {
                mDoMtx_stack_c::XrotM((s16)npc->m_jnt.getBackbone_y());
                mDoMtx_stack_c::ZrotM(-npc->m_jnt.getBackbone_x());
            }
            cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
            model->setAnmMtx(joint, mDoMtx_stack_c::get());
        }
    }
    return TRUE;
}

/* 00000364-00000478       .text initTexPatternAnm__14daNpc_Sarace_cFb */
BOOL daNpc_Sarace_c::initTexPatternAnm(bool modify) {
    J3DModelData* modelData = mpHeadMorf->getModel()->getModelData();
    m_btp = (J3DAnmTexPattern*)dComIfG_getObjectRes("Sarace", l_btp_ix_tbl[mTexIndex]);
    JUT_ASSERT(249, m_btp != 0);
    if (!mBtp.init(modelData, m_btp, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, modify, FALSE)) {
        return FALSE;
    }
    mBtpFrame = 0;
    mBlinkTimer = 0;
    return TRUE;
}

/* 00000478-00000504       .text playTexPatternAnm__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::playTexPatternAnm() {
    if (cLib_calcTimer(&mBlinkTimer) == 0) {
        if (mBtpFrame >= m_btp->getFrameMax()) {
            mBtpFrame -= m_btp->getFrameMax();
            mBlinkTimer = 30.0f + cM_rndF(100.0f);
        } else {
            mBtpFrame++;
        }
    }
}

/* 00000504-0000060C       .text setAnm__14daNpc_Sarace_cFScf */
void daNpc_Sarace_c::setAnm(s8 index, f32 morf) {
    static int a_play_mode_tbl[] = {2, 2, 2};
    static f32 a_morf_frame_tbl[] = {8.0f, 8.0f, 8.0f};
    static f32 a_play_speed_tbl[] = {1.0f, 1.0f, 1.0f};
    if (morf < 0.0f) {
        morf = a_morf_frame_tbl[index];
    }
    if (index != mAnmIndex && mAnmIndex != -1) {
        mAnmIndex = index;
        dNpc_setAnm(mpMorf, a_play_mode_tbl[mAnmIndex], morf, a_play_speed_tbl[mAnmIndex], l_bck_ix_tbl[mAnmIndex], -1, "Sarace");
        dNpc_setAnm(mpHeadMorf, a_play_mode_tbl[mAnmIndex], morf, a_play_speed_tbl[mAnmIndex], l_bck_head_ix_tbl[mAnmIndex], -1, "Sarace");
    }
}

/* 0000060C-00000760       .text chkAttention__14daNpc_Sarace_cF4cXyzs */
bool daNpc_Sarace_c::chkAttention(cXyz pos, s16 angle) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 maxDist = l_HIO.mPrm.mMaxAttnDistXZ;
    int maxAngle = l_HIO.mPrm.mMaxAttnAngleY;
    cXyz delta;
    delta.x = player->current.pos.x - pos.x;
    delta.z = player->current.pos.z - pos.z;
    f32 distance = std::sqrtf(delta.x * delta.x + delta.z * delta.z);
    s16 targetAngle = cM_atan2s(delta.x, delta.z);
    if (mHasAttention) {
        maxDist += 40.0f;
        maxAngle += 0x71C;
    }
    targetAngle -= angle;
    return maxAngle > abs(targetAngle) && maxDist > distance;
}

/* 00000760-000007D8       .text eventOrder__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::eventOrder() {
    if (mEventOrder == 3) {
        fopAcM_orderOtherEvent2(this, "SARACE_EXPCAM", 1, 0xFFFF);
    } else if (mEventOrder == 1 || mEventOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mEventOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 000007D8-000008E8       .text checkOrder__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (mEventOrder == 3) {
            setAction(&daNpc_Sarace_c::event_endCheck_action, NULL);
            mEventOrder = 0;
        }
    } else if (eventInfo.checkCommandTalk()) {
        if (mEventOrder == 1 || mEventOrder == 2) {
            mEventOrder = 0;
            mTalking = true;
        }
    }
}

/* 000008E8-00000A6C       .text next_msgStatus__14daNpc_Sarace_cFPUl */
u16 daNpc_Sarace_c::next_msgStatus(u32* msg) {
    u16 result = 15;
    switch (*msg) {
    case 0xFA1:
    case 0xFA2:
    case 0xFA5:
    case 0xFA7:
    case 0xFB1:
    case 0xFB2:
    case 0xFB4:
    case 0xFB5:
    case 0xFB7:
        ++*msg;
        break;
    case 0xFB8:
        *msg = 0xFB3;
        break;
    case 0xFB3:
        *msg = 0xFA3;
        break;
    case 0xFA3:
        if (mpCurrMsg->mSelectNum == 0) {
            *msg = 0xFA4;
        } else if (mpCurrMsg->mSelectNum == 1) {
            if (dComIfGs_getRupee() < 30U) {
                *msg = 0xFAF;
            } else {
                dComIfGp_setItemRupeeCount(-30);
                dComIfGs_offEventBit(0x2820);
                *msg = 0xFB0;
            }
        } else if (!dComIfGs_isEventBit(0x2808)) {
            dComIfGs_onEventBit(0x2808);
            *msg = 0xFAD;
        } else {
            *msg = 0xFAE;
        }
        break;
    case 0xFA8:
        if (dComIfGs_isEventBit(0x2820)) {
            *msg = 0xFAA;
        } else {
            *msg = 0xFA9;
        }
        break;
    case 0xFAA:
        *msg = 0xFA9;
        break;
    case 0xFA9:
        *msg = 0xFA3;
        break;
    default:
        result = 16;
        break;
    }
    return result;
}

/* 00000A6C-00000B50       .text getMsg__14daNpc_Sarace_cFv */
u32 daNpc_Sarace_c::getMsg() {
    u32 result;
    if (mNextMsg != 0) {
        if (mNextMsg == 0xFB4) {
            if (ship_race_result == 1) {
                mNextMsg = 0xFB7;
            } else if (ship_race_result == 3) {
                mNextMsg = 0xFB1;
            } else {
                dComIfGp_setMessageCountNumber(ship_race_rupee);
            }
        }
        result = mNextMsg;
        mNextMsg = 0;
    } else if (!dComIfGs_isEventBit(0x2810)) {
        dComIfGs_onEventBit(0x2810);
        result = 0xFA1;
    } else if (!dComIfGs_isEventBit(0x2840)) {
        result = 0xFA1;
    } else {
        result = 0xFA2;
    }
    return result;
}

/* 00000B50-00000BEC       .text anmAtr__14daNpc_Sarace_cFUs */
void daNpc_Sarace_c::anmAtr(u16) {
    switch (dComIfGp_getMesgAnimeAttrInfo()) {
    case 0:
        setAnm(0, -1.0f);
        break;
    case 1:
        setAnm(1, l_HIO.mTalkMorf);
        break;
    case 2:
        setAnm(2, l_HIO.mOtherMorf);
        break;
    }
    dComIfGp_clearMesgAnimeAttrInfo();
}

/* 00000BEC-00000E24       .text CreateInit__14daNpc_Sarace_cFv */
BOOL daNpc_Sarace_c::CreateInit() {
    mInitialAngle = current.angle;
    attention_info.flags = fopAc_Attn_LOCKON_TALK_e | fopAc_Attn_ACTION_SPEAK_e;
    gravity = -30.0f;
    setAction(&daNpc_Sarace_c::wait_action, NULL);
    mAttentionBasePos = current.pos;
    mEyePos = current.pos;
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(60.0f, 150.0f);
    m730 = 0;
    mEventCut.setActorInfo2("Sarace", this);
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAD;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAD;
    set_mtx();
    mNextMsg = 0;
    mHBarrelId = fpcM_ERROR_PROCESS_ID_e;
    mVBarrelId = fpcM_ERROR_PROCESS_ID_e;
    setAnm(0, -1.0f);
    if (dComIfGp_getStartStagePoint() == 1 && dComIfGp_getStartStageRoomNo() == 0x30
#if VERSION > VERSION_DEMO
        && ship_race_result != 0
#endif
    ) {
        mEventOrder = 1;
        mNextMsg = 0xFB4;
        fopAcM_orderSpeakEvent(this);
    }
    mpMorf->play(&eyePos, 0, 0);
    mpMorf->calc();
    mpHeadMorf->play(NULL, 0, 0);
    mpHeadMorf->calc();
    return TRUE;
}

/* 00000E24-00000E68       .text setAttention__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::setAttention() {
    eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
    attention_info.position.set(mAttentionBasePos.x, mAttentionBasePos.y + l_HIO.mPrm.mAttnYOffset, mAttentionBasePos.z);
}

/* 00000E68-00000FF4       .text lookBack__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::lookBack() {
    cXyz target;
    cXyz source(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool flag = true;
    switch (mState) {
    case 1:
    case 2:
        if (mState == 2) {
            m_jnt.setTrn();
            if (!mHasAttention) {
                target = dNpc_playerEyePos(l_HIO.mPrm.m04);
                cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &target), 4, 0x1800);
            }
        }
        if (mHasAttention) {
            target = dNpc_playerEyePos(l_HIO.mPrm.m04);
            targetP = &target;
            source = current.pos;
            source.y = eyePos.y;
        }
        break;
    }
    if (m_jnt.trnChk()) {
        flag = false;
    } else {
        mTurn = 0;
    }
    m_jnt.lookAtTarget(&current.angle.y, targetP, source, angle, l_HIO.mPrm.mMaxHeadTurnVel, flag);
}

/* 00000FF4-00001024       .text wait01__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::wait01() {
    if (mTalking) {
        mState = 2;
    } else if (mEventOrder == 0) {
        mEventOrder = 2;
    }
}

/* 00001024-000014B8       .text talk01__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::talk01() {
    if (talk(1) == 0x12) {
        mState = 1;
        if (mCurrMsgNo == 0xFA4) {
            static cXyz barrelPos[] = {cXyz(175875.0f, 50.0f, 276520.0f), cXyz(176030.0f, 50.0f, 278884.0f)};
            dComIfGp_event_reset();
            mEventOrder = 3;
            mHBarrelId = fopAcM_create(fpcNm_Obj_Barrel2_e, daObjBarrel2::Act_c::make_prm(daObjBarrel2::Type_01_e, 1, true, false, daObjBuoyflag::Texture_00_e), &barrelPos[0], -1, NULL, NULL, -1, NULL);
            mVBarrelId = fopAcM_create(fpcNm_Obj_Barrel2_e, daObjBarrel2::Act_c::make_prm(daObjBarrel2::Type_00_e, 1, true, false, daObjBuoyflag::Texture_00_e), &barrelPos[1], -1, NULL, NULL, -1, NULL);
        } else if (mCurrMsgNo == 0xFB0) {
            dComIfGs_onEventBit(0x2840);
            dComIfGp_setNextStage("Ocean", 1, 0, 0, 0.0f, 0, 1, 0);
            if (dComIfGs_getSelectItem(0) != 1 && dComIfGs_getSelectItem(1) != 1 && dComIfGs_getSelectItem(2) != 1) {
                dComIfGs_setSelectItem(1, 1);
                dComIfGp_setSelectItem(1);
            }
            setAction(&daNpc_Sarace_c::dummy_action, NULL);
        } else {
            dComIfGp_event_reset();
            ship_race_result = 0;
            ship_race_rupee = 0;
            setAnm(0, -1.0f);
        }
        mTalking = false;
    } else if (mCurrMsgNo == 0xFB6 && dComIfGp_checkMesgSendButton() == 1) {
        dComIfGp_setItemRupeeCount(ship_race_rupee);
        u8 previous = dComIfGs_getEventReg(0xAAFF);
        u8 count = previous + 1 > 12 ? 12 : previous + 1;
        dComIfGs_setEventReg(0xAAFF, count);
        if (previous != count) {
            dComIfGs_onEventBit(0x2820);
        }
    }
}

/* 000014B8-000014E0       .text dummy_action__14daNpc_Sarace_cFPv */
int daNpc_Sarace_c::dummy_action(void*) {
    if (mActionState == 0) {
        mState = 1;
        mActionState++;
    }
    return 1;
}

/* 000014E0-000015BC       .text wait_action__14daNpc_Sarace_cFPv */
int daNpc_Sarace_c::wait_action(void*) {
    if (mActionState == 0) {
        mState = 1;
        mActionState++;
    } else if (mActionState != -1) {
        s16 angle = current.angle.y + m_jnt.getHead_y() + m_jnt.getBackbone_y();
        mHasAttention = chkAttention(current.pos, angle);
        switch (mState) {
        case 1:
            wait01();
            break;
        case 2:
            talk01();
            break;
        }
        lookBack();
        setAttention();
    }
    return 1;
}

/* 000015BC-0000173C       .text event_endCheck_action__14daNpc_Sarace_cFPv */
int daNpc_Sarace_c::event_endCheck_action(void*) {
    if (mActionState == 0) {
        mActionState++;
    } else if (mActionState != -1 && dComIfGp_evmng_endCheck("SARACE_EXPCAM")) {
        mNextMsg = 0xFA7;
        dComIfGp_event_reset();
        mEventOrder = 1;
        setAction(&daNpc_Sarace_c::wait_action, NULL);
        daObjBarrel2::Act_c* h = (daObjBarrel2::Act_c*)fopAcM_SearchByID(mHBarrelId);
        daObjBarrel2::Act_c* v = (daObjBarrel2::Act_c*)fopAcM_SearchByID(mVBarrelId);
        if (h != NULL) {
            h->m475 = 1;
        }
        if (v != NULL) {
            v->m475 = 1;
        }
    }
    return 1;
}

/* 0000173C-000017E0       .text set_mtx__14daNpc_Sarace_cFv */
void daNpc_Sarace_c::set_mtx() {
    J3DModel* model = mpMorf->getModel();
    J3DModel* headModel = mpHeadMorf->getModel();
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
#if VERSION > VERSION_DEMO
    mpMorf->calc();
#endif
    headModel->setBaseTRMtx(model->getAnmMtx(m_jnt.getHeadJntNum()));
#if VERSION > VERSION_DEMO
    mpHeadMorf->calc();
#endif
}

/* 000017E0-00001938       .text _draw__14daNpc_Sarace_cFv */
BOOL daNpc_Sarace_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModel* headModel = mpHeadMorf->getModel();
    J3DModelData* headData = headModel->getModelData();
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    g_env_light.setLightTevColorType(headModel, &tevStr);
    mBtp.entry(headData, mBtpFrame);
    mpMorf->entryDL();
    mpHeadMorf->entryDL();
    mBtp.remove(headData);
    cXyz shadowPos(current.pos.x, current.pos.y + 130.0f, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &shadowPos, 800.0f, 20.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId != 0) {
        dComIfGd_addRealShadow(mShadowId, mpHeadMorf->getModel());
    }
    dSnap_RegistFig(0x82, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 00001938-00001A68       .text _execute__14daNpc_Sarace_cFv */
BOOL daNpc_Sarace_c::_execute() {
    m_jnt.setParam(l_HIO.mPrm.mMaxBackboneX, l_HIO.mPrm.mMaxBackboneY,
        l_HIO.mPrm.mMinBackboneX, l_HIO.mPrm.mMinBackboneY,
        l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY, l_HIO.mPrm.mMinHeadX,
        l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurnStep);
    playTexPatternAnm();
#if VERSION == VERSION_DEMO
    mpMorf->play(&eyePos, 0, 0);
    mpMorf->calc();
    mpHeadMorf->play(NULL, 0, 0);
    mpHeadMorf->calc();
#else
    mpMorf->play(NULL, 0, 0);
    mpHeadMorf->play(NULL, 0, 0);
#endif
    checkOrder();
    (this->*mAction)(NULL);
    mEventCut.cutProc();
    eventOrder();
    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    set_mtx();
    setCollision(60.0f, 150.0f);
    return TRUE;
}

/* 00001A68-00001AE0       .text _delete__14daNpc_Sarace_cFv */
BOOL daNpc_Sarace_c::_delete() {
    dComIfG_resDeleteDemo(&mPhase, "Sarace");
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    if (l_HIO.mNo >= 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
        l_HIO.mNo = -1;
    }
    return TRUE;
}

/* 00001AE0-00001B00       .text CallbackCreateHeap__FP10fopAc_ac_c */
static BOOL CallbackCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Sarace_c*)actor)->CreateHeap();
}

/* 00001B00-00001D1C       .text _create__14daNpc_Sarace_cFv */
cPhs_State daNpc_Sarace_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Sarace_c);
#endif
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Sarace");
    if (phase == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_SetupActor(this, daNpc_Sarace_c);
#endif
        if (!fopAcM_entrySolidHeap(this, CallbackCreateHeap, 0x2760)) {
            return cPhs_ERROR_e;
        }
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        if (l_HIO.mNo < 0) {
            l_HIO.mNo = mDoHIO_createChild("船乗りレースゲーム専用", &l_HIO);
        }
        if (!CreateInit()) {
            return cPhs_ERROR_e;
        }
    }
    return phase;
}

/* 000020CC-00002498       .text CreateHeap__14daNpc_Sarace_cFv */
BOOL daNpc_Sarace_c::CreateHeap() {
    J3DModelData* headModelData;
    J3DModelData* modelData;
    modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes("Sarace", dRes_INDEX_SARACE_BDL_SA_e));
    JUT_ASSERT(DEMO_SELECT(999, 1008), modelData != 0);
    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("Sarace", dRes_INDEX_SARACE_BCK_SA_WAIT01_e),
        2, 1.0f, 0, -1, 1, NULL, 0, 0x11020203);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
#if VERSION > VERSION_DEMO
        mpMorf = NULL;
#endif
        return FALSE;
    }
    m_jnt.setHeadJntNum(modelData->getJointName()->getIndex("head"));
    JUT_ASSERT(DEMO_SELECT(1012, 1024), m_jnt.getHeadJntNum() >= 0);
    m_jnt.setBackboneJntNum(modelData->getJointName()->getIndex("backbone"));
    JUT_ASSERT(DEMO_SELECT(1014, 1026), m_jnt.getBackboneJntNum() >= 0);
    headModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes("Sarace", dRes_INDEX_SARACE_BDL_SA01_HEAD_e));
    JUT_ASSERT(DEMO_SELECT(1022, 1034), headModelData != 0);
    mpHeadMorf = new mDoExt_McaMorf(headModelData, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("Sarace", dRes_INDEX_SARACE_BCK_SA01HEAD_WAIT01_e),
        2, 1.0f, 0, -1, 1, NULL, 0, 0x11020203);
    if (mpHeadMorf == NULL || mpHeadMorf->getModel() == NULL) {
        return FALSE;
    }
    mTexIndex = 0;
    if (!initTexPatternAnm(false)) {
        return FALSE;
    }
    for (u16 i = 0; i < modelData->getJointNum(); i++) {
        if (i == m_jnt.getHeadJntNum() || i == m_jnt.getBackboneJntNum()) {
            mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
        }
    }
    mpMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 0.0f);
#if VERSION == VERSION_DEMO
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
#else
    mObjAcch.Set(&current.pos, &old.pos, this, 1, &mAcchCir, &speed, NULL, NULL);
#endif
    return TRUE;
}

/* 00002498-000024B8       .text daNpc_Sarace_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Sarace_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Sarace_c*)i_this)->_create();
}

/* 000024B8-000024D8       .text daNpc_Sarace_Delete__FP14daNpc_Sarace_c */
static BOOL daNpc_Sarace_Delete(daNpc_Sarace_c* i_this) {
    return ((daNpc_Sarace_c*)i_this)->_delete();
}

/* 000024D8-000024F8       .text daNpc_Sarace_Execute__FP14daNpc_Sarace_c */
static BOOL daNpc_Sarace_Execute(daNpc_Sarace_c* i_this) {
    return ((daNpc_Sarace_c*)i_this)->_execute();
}

/* 000024F8-00002518       .text daNpc_Sarace_Draw__FP14daNpc_Sarace_c */
static BOOL daNpc_Sarace_Draw(daNpc_Sarace_c* i_this) {
    return ((daNpc_Sarace_c*)i_this)->_draw();
}

/* 00002518-00002520       .text daNpc_Sarace_IsDelete__FP14daNpc_Sarace_c */
static BOOL daNpc_Sarace_IsDelete(daNpc_Sarace_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Sarace_Method = {
    (process_method_func)daNpc_Sarace_Create,
    (process_method_func)daNpc_Sarace_Delete,
    (process_method_func)daNpc_Sarace_Execute,
    (process_method_func)daNpc_Sarace_IsDelete,
    (process_method_func)daNpc_Sarace_Draw,
};
actor_process_profile_definition g_profile_NPC_SARACE = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_SARACE_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Sarace_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_SARACE_e,
    /* Actor SubMtd */ &l_daNpc_Sarace_Method,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_12_e,
};
