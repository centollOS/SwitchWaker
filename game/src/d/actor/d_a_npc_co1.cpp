/**
 * d_a_npc_co1.cpp
 * NPC - Prince Komali (before Dragon Roost Cavern)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_co1.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Co.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include <string.h>
#include <math.h>

STATIC_ASSERT(sizeof(daNpc_Co1_c) == 0x864);
static daNpc_Co1_HIO_c l_HIO;


/* 000000EC-00000150       .text __ct__15daNpc_Co1_HIO_cFv */
daNpc_Co1_HIO_c::daNpc_Co1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x2000, 9000, -0x2000, -9000, 0, 0, 0, 0,
        0x400, 0x400, 91.0f, false, 255, 150, 0, 40.0f, 20.0f, 0x200, 64,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(hio_prm_c));
    m04 = -1;
    m08 = -1;
}

daNpc_Co1_HIO_c::~daNpc_Co1_HIO_c() {}

static const char* l_evn_tbl[] = {"Prl_Flw", "Contact", "Red_Ltr"};

/* 00000198-000001E4       .text nodeCallBack_Co1__FP7J3DNodei */
static BOOL nodeCallBack_Co1(J3DNode* node, int timing) {
    if (timing == J3DNodeCBCalcTiming_In && j3dSys.getModel()->getUserArea() != 0) {
        ((daNpc_Co1_c*)j3dSys.getModel()->getUserArea())->nodeCo1Control(node, j3dSys.getModel());
    }
    return TRUE;
}

/* 000001E4-00000334       .text nodeCo1Control__11daNpc_Co1_cFP7J3DNodeP8J3DModel */
void daNpc_Co1_c::nodeCo1Control(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(14.0f, 18.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
        mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyePos);
    }
    if (joint == m_bbone_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    }
    MTXCopy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000370-00000410       .text init_CO1_0__11daNpc_Co1_cFv */
bool daNpc_Co1_c::init_CO1_0() {
    bool result = dComIfGs_isSymbol(1) == FALSE;
    if (result) {
#if VERSION > VERSION_DEMO
        fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
        fopAcM_OnStatus(this, fopAcStts_UNK4000_e);
#endif
        set_action(&daNpc_Co1_c::wait_action1, NULL);
    }
    return result;
}

/* 00000410-00000588       .text createInit__11daNpc_Co1_cFv */
bool daNpc_Co1_c::createInit() {
    for (int i = 0; i < 3; i++) mEventIdx[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 70;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 69;
    gravity = -4.5f;
    mLookPos = current.pos;
    mNoTurn = true;
    mEventCut.setActorInfo2("Co1", this);
    mAnmNo = 11;
    bool result;
    switch (mSubType) {
    case 0: result = init_CO1_0(); break;
    default: result = false; break;
    }
    if (result) shape_angle = current.angle;
    else return false;
    mStts.Init(255, 255, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

/* 00000588-000008E4       .text setMtx__11daNpc_Co1_cFb */
void daNpc_Co1_c::setMtx(bool force) {
    if (!mDemo) {
        plyTexPttrnAnm();
        mAnmEnd = mpMorf->play(&eyePos, 0, 0);
        mpPearlMorf->setFrame(mpMorf->getFrame());
        if (mpMorf->getFrame() < mFrame) mAnmEnd = 1;
        mFrame = mpMorf->getFrame();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
        switch (mAnmNo) {
        case 8: mDrawLetter = mpMorf->getFrame() >= 15.0f; break;
        case 4: mDrawLetter = true; break;
        default: mDrawLetter = false; break;
        }
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_bbone_jnt_num));
    mpPearlMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpPearlMorf->calc();
    if (mDrawLetter) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hnd_R_jnt_num));
        mpLetterModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mpLetterModel->calc();
    }
    mDoMtx_stack_c::copy(mpPearlMorf->getModel()->getAnmMtx(m_prl_jnt_num));
    mPearlPos.x = mDoMtx_stack_c::get()[0][3];
    mPearlPos.y = mDoMtx_stack_c::get()[1][3];
    mPearlPos.z = mDoMtx_stack_c::get()[2][3];
    mLight.mPos = mPearlPos;
    mLight.mColor.r = l_HIO.mPrm.mLightR;
    mLight.mColor.g = l_HIO.mPrm.mLightG;
    mLight.mColor.b = l_HIO.mPrm.mLightB;
    mLightAngle += l_HIO.mPrm.mLightSpeed;
    mLightWave = fabs(cM_ssin(mLightAngle));
    f32 range = cLib_minLimit(l_HIO.mPrm.mMaxLightPower - l_HIO.mPrm.mMinLightPower, 0.0f);
    mLight.mPower = cLib_minLimit(l_HIO.mPrm.mMinLightPower + range * mLightWave, 0.001f);
    setAttention(force);
}

/* 000008E4-000008F8       .text anmNum_toResID__11daNpc_Co1_cFi */
int daNpc_Co1_c::anmNum_toResID(int index) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_CO_BCK_CO_WAIT01_e, dRes_ID_CO_BCK_CO_WAIT02_e, dRes_ID_CO_BCK_CO_MIGAMAE_e, dRes_ID_CO_BCK_CO_OKIRU_e, dRes_ID_CO_BCK_CO_READ_e, dRes_ID_CO_BCK_CO_TALK01_e, dRes_ID_CO_BCK_CO_TALK02_e, dRes_ID_CO_BCK_CO_SWAIT01_e, dRes_ID_CO_BCK_CO_UKETORI_e, dRes_ID_CO_BCK_CO_OKIRU_e, dRes_ID_CO_BCK_CO_MIGAMAE_e
    };
    return a_bck_resID_tbl[index];
}

/* 000008F8-0000090C       .text anmNum_toResID_prl__11daNpc_Co1_cFi */
int daNpc_Co1_c::anmNum_toResID_prl(int index) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_CO_BCK_COPEAL_WAIT01_e, dRes_ID_CO_BCK_COPEAL_WAIT02_e, dRes_ID_CO_BCK_COPEAL_MIGAMAE_e, dRes_ID_CO_BCK_COPEAL_OKIRU_e, dRes_ID_CO_BCK_COPEAL_READ_e, dRes_ID_CO_BCK_COPEAL_TALK01_e, dRes_ID_CO_BCK_COPEAL_TALK02_e, dRes_ID_CO_BCK_COPEAL_SWAIT01_e, dRes_ID_CO_BCK_COPEAL_UKETORI_e, dRes_ID_CO_BCK_COPEAL_OKIRU_e, dRes_ID_CO_BCK_COPEAL_MIGAMAE_e
    };
    return a_bck_resID_tbl[index];
}

/* 0000090C-00000920       .text btpNum_toResID__11daNpc_Co1_cFi */
int daNpc_Co1_c::btpNum_toResID(int index) {
    static const int a_btp_resID_tbl[] = {
        dRes_ID_CO_BTP_FUAN_e, dRes_ID_CO_BTP_KIRI_e, dRes_ID_CO_BTP_NIKO_e, dRes_ID_CO_BTP_OKORI_e, dRes_ID_CO_BTP_NAGAME_e, dRes_ID_CO_BTP_OKIRU_e, dRes_ID_CO_BTP_MIGAMAE_e
    };
    return a_btp_resID_tbl[index];
}

/* 00000920-00000A30       .text setBtp__11daNpc_Co1_cFbi */
bool daNpc_Co1_c::setBtp(bool modify, int number) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    m_hed_tex_pttrn = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Co", btpNum_toResID(number));
    JUT_ASSERT(DEMO_SELECT(0x1CC, 0x1CE), m_hed_tex_pttrn != 0);
    bool result = mBtpAnm.init(data, m_hed_tex_pttrn, true, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, modify, 0) == TRUE;
    if (result) {
        mBtpFrame = 0;
        mBtpTimer = 0;
    }
    return result;
}

/* 00000A30-00000B30       .text setBtk__11daNpc_Co1_cFb */
bool daNpc_Co1_c::setBtk(bool modify) {
    J3DModelData* data = mpPearlMorf->getModel()->getModelData();
    m_prl_btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectIDRes("Co", dRes_ID_CO_BTK_CO_PEAL_e);
    JUT_ASSERT(DEMO_SELECT(0x1E8, 0x1EA), m_prl_btk != 0);
    bool result = mBtkAnm.init(data, m_prl_btk, true, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, modify, 0) == TRUE;
    if (result) mBtkFrame = 0;
    return result;
}

/* 00000B30-00000B58       .text iniTexPttrnAnm__11daNpc_Co1_cFb */
bool daNpc_Co1_c::iniTexPttrnAnm(bool modify) {
    return setBtp(modify, mBtpNo);
}

/* 00000B58-00000C44       .text plyTexPttrnAnm__11daNpc_Co1_cFv */
void daNpc_Co1_c::plyTexPttrnAnm() {
    mBtkFrame++;
    if (mBtkFrame >= m_prl_btk->getFrameMax()) mBtkFrame = 0;
    if (mBtpNo == 6 || mBtpNo == 5 || cLib_calcTimer(&mBtpTimer) == 0) {
        mBtpFrame++;
        if (mBtpFrame >= m_hed_tex_pttrn->getFrameMax()) {
            if (mBtpNo == 6 || mBtpNo == 5) mBtpFrame = m_hed_tex_pttrn->getFrameMax();
            else {
                mBtpTimer = 30.0f + cM_rndF(60.0f);
                mBtpFrame = 0;
            }
        }
    }
}

/* 00000C44-00000C80       .text setAnm_tex__11daNpc_Co1_cFSc */
void daNpc_Co1_c::setAnm_tex(s8 number) {
    if (mBtpNo != number) {
        mBtpNo = number;
        iniTexPttrnAnm(true);
    }
}

/* 00000C80-00000D64       .text setAnm_anm__11daNpc_Co1_cFPQ211daNpc_Co1_c9anm_prm_c */
bool daNpc_Co1_c::setAnm_anm(anm_prm_c* prm) {
    if (mAnmNo == prm->mAnm) return true;
    mAnmNo = prm->mAnm;
    int resource = anmNum_toResID(mAnmNo);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, resource, -1, "Co");
    resource = anmNum_toResID_prl(mAnmNo);
    dNpc_setAnmIDRes(mpPearlMorf, prm->mLoop, 0.0f, 0.0f, resource, -1, "Co");
    mFrame = 0.0f;
    m809 = 0;
    mAnmEnd = 0;
    return true;
}

/* 00000D64-00000DD0       .text setAnm_NUM__11daNpc_Co1_cFii */
void daNpc_Co1_c::setAnm_NUM(int number, int texture) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, 6, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {3, 5, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {4, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {6, 3, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {7, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {8, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {9, 5, 8.0f, -1.0f, J3DFrameCtrl::EMode_NONE},
        {10, 6, 8.0f, -1.0f, J3DFrameCtrl::EMode_NONE},
        {7, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
    };
    if (texture) setAnm_tex(a_anm_prm_tbl[number].mBtp);
    setAnm_anm(&a_anm_prm_tbl[number]);
}

/* 00000DD0-00000E50       .text setAnm__11daNpc_Co1_cFv */
bool daNpc_Co1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {1, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {3, 5, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {7, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {4, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {10, 6, 8.0f, -1.0f, J3DFrameCtrl::EMode_NONE},
    };
    if (a_anm_prm_tbl[mState].mBtp >= 0) setAnm_tex(a_anm_prm_tbl[mState].mBtp);
    if (a_anm_prm_tbl[mState].mAnm >= 0) setAnm_anm(&a_anm_prm_tbl[mState]);
    return true;
}

/* 00000E50-00000E54       .text chg_anmTag__11daNpc_Co1_cFv */
void daNpc_Co1_c::chg_anmTag() {

}

/* 00000E54-00000E58       .text control_anmTag__11daNpc_Co1_cFv */
void daNpc_Co1_c::control_anmTag() {

}

/* 00000E58-00000EF8       .text chg_anmAtr__11daNpc_Co1_cFUc */
void daNpc_Co1_c::chg_anmAtr(u8 attr) {
    if (attr >= 14 || attr == mAnmAttr) return;
    mLookMode = 1;
    switch (attr) {
    case 3: case 4: case 5: case 7: case 11: mLookMode = 0; break;
    case 13: set_target(1); break;
    }
    mAnmAttr = attr;
    setAnm_ATR(1);
}

/* 00000EF8-00000FF4       .text control_anmAtr__11daNpc_Co1_cFv */
void daNpc_Co1_c::control_anmAtr() {
    switch (mAnmAttr) {
    case 0: case 1: break;
    case 2:
        if (mAnmEnd) { setAnm_NUM(7, 1); mAnmAttr = 6; }
        break;
    case 3:
        if (mAnmEnd) { setAnm_NUM(4, 1); mAnmAttr = 10; }
        break;
    case 4: case 5: case 6: break;
    case 7:
        if (mAnmEnd) { setAnm_NUM(6, 1); mAnmAttr = 5; }
        break;
    case 8:
        if (mAnmEnd) { setAnm_NUM(7, 1); mAnmAttr = 6; }
        break;
    case 9:
        if (mAnmEnd) { setAnm_NUM(1, 1); mAnmAttr = 1; }
        break;
    case 10: case 11: case 12: case 13: break;
    }
}

/* 00000FF4-0000105C       .text setAnm_ATR__11daNpc_Co1_cFi */
void daNpc_Co1_c::setAnm_ATR(int texture) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {3, 5, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {8, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {5, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {6, 3, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {7, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, 6, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {10, 6, 8.0f, -1.0f, J3DFrameCtrl::EMode_NONE},
        {9, 5, 8.0f, -1.0f, J3DFrameCtrl::EMode_NONE},
        {4, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, 4, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {6, 3, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
    };
    if (texture) setAnm_tex(a_anm_prm_tbl[mAnmAttr].mBtp);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 0000105C-00001120       .text anmAtr__11daNpc_Co1_cFUs */
void daNpc_Co1_c::anmAtr(u16 status) {
    switch (status) {
    case 6: {
        if (mMsgAnmStarted == 0) {
            mAnmAttr = 0xFF;
            chg_anmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        u8 tag = dComIfGp_getMesgAnimeTagInfo();
        dComIfGp_clearMesgAnimeTagInfo();
        if (tag != 0xFF && mAnmTag != tag) {
            mAnmTag = tag;
            chg_anmTag();
        }
        break;
    }
    case 14: mMsgAnmStarted = 0; break;
    }
    control_anmTag();
    control_anmAtr();
}

/* 00001120-000011B8       .text eventOrder__11daNpc_Co1_cFv */
void daNpc_Co1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        eventInfo.onCondition(0x20);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    } else if (mOrder >= 3) {
        mEventNo = mOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIdx[mEventNo]);
    }
}

/* 000011B8-00001288       .text checkOrder__11daNpc_Co1_cFv */
void daNpc_Co1_c::checkOrder() {
    if (eventInfo.getCommand() == dEvtCmd_INDEMO_e) {
        if (dComIfGp_evmng_startCheck(mEventIdx[mEventNo])) {
            switch (mEventNo) {
            case 0: break;
            case 1:
#if VERSION > VERSION_DEMO
                fopAcM_OffStatus(this, fopAcStts_UNK4000_e);
#endif
                break;
            case 2: mLookMode = 0; break;
            }
            mOrder = 0;
        }
    } else if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mOrder == 1 || mOrder == 2)) {
        mOrder = 0;
        mTalking = true;
    }
}

/* 00001288-000013AC       .text setCollision_SP___11daNpc_Co1_cFv */
void daNpc_Co1_c::setCollision_SP_() {
    cXyz pos;
    cXyz offset;
    f32 radius, height;
    if (!mTalking) {
        if (mAnmNo == 1) {
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::YrotM(current.angle.y);
            offset.set(0.0f, 0.0f, -20.0f);
            radius = 70.0f;
            height = 60.0f;
            mDoMtx_stack_c::multVec(&offset, &pos);
        } else {
            pos.set(current.pos.x, current.pos.y, current.pos.z);
            radius = 40.0f;
            height = 90.0f;
        }
        mCyl.SetC(pos);
        mCyl.SetR(radius);
        mCyl.SetH(height);
        dComIfG_Ccsp()->Set(&mCyl);
    }
}

/* 000013AC-000014B8       .text set_target__11daNpc_Co1_cFi */
void daNpc_Co1_c::set_target(int mode) {
    switch (mode) {
    case 0: mLookMode = 0; break;
    case 1: {
        cXyz offset = dNpc_playerEyePos(-20.0f);
        f32 distance = fopAcM_searchActorDistance(this, dComIfGp_getPlayer(0));
        s16 angle = fopAcM_searchActorAngleY(this, dComIfGp_getPlayer(0));
        angle -= 0x2000;
        mDoMtx_stack_c::transS(current.pos.x, offset.y, current.pos.z);
        mDoMtx_stack_c::YrotM(angle);
        offset.set(0.0f, 0.0f, distance);
        mDoMtx_stack_c::multVec(&offset, &mLookPos);
        mLookMode = 2;
        break;
    }
    }
}

/* 000014B8-00001554       .text chk_talk__11daNpc_Co1_cFv */
bool daNpc_Co1_c::chk_talk() {
    bool result = true;
    mItemNo = 0xFF;
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) mItemNo = dComIfGp_event_getPreItemNo();
        else result = false;
    }
    return result;
}

/* 00001554-00001594       .text chk_partsNotMove__11daNpc_Co1_cFv */
bool daNpc_Co1_c::chk_partsNotMove() {
    return mPreviousHeadY == m_jnt.getHead_y() && mPreviousBackboneY == m_jnt.getBackbone_y() && mPreviousAngleY == current.angle.y;
}

/* 00001594-00001734       .text lookBack__11daNpc_Co1_cFv */
void daNpc_Co1_c::lookBack() {
    cXyz target;
    cXyz base;
    mPreviousHeadY = m_jnt.getHead_y();
    mPreviousBackboneY = m_jnt.getBackbone_y();
    mPreviousAngleY = current.angle.y;
    base = current.pos;
    base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
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
    case 3: angle = mLookAngle; break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mPrm.mTurnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) mTurnSpeed = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, noTurn);
}

/* 00001734-000017EC       .text next_msgStatus__11daNpc_Co1_cFPUl */
u16 daNpc_Co1_c::next_msgStatus(u32* msg) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    switch (*msg) {
    case 0x183E: *msg = 0x183F; break;
    case 0x183F: *msg = 0x1840; break;
    case 0x1841: *msg = 0x1842; break;
    case 0x1842: *msg = 0x1843; break;
    case 0x1843: *msg = 0x1844; break;
    case 0x1844: *msg = 0x1845; break;
    case 0x1845: *msg = 0x1846; break;
    case 0x1846: *msg = 0x1847; break;
    case 0x1847: *msg = 0x1848; break;
    case 0x1849: *msg = 0x184A; break;
    case 0x184A: *msg = 0x184B; break;
    default: status = fopMsgStts_MSG_ENDS_e; break;
    }
    return status;
}

/* 000017EC-00001894       .text getMsg_CO1_0__11daNpc_Co1_cFv */
u32 daNpc_Co1_c::getMsg_CO1_0() {
    if (mItemNo == 0x98) return 0x183C;
    if (mItemNo != 0xFF) return 0x183B;
    if (mReadLetter) return 0x183E;
    if (dNpc_chkLetterPassed()) return dComIfGs_isEventBit(0xF04) ? 0x1849 : 0x1841;
    return dComIfGs_isEventBit(0xF08) ? 0x183A : 0x1839;
}

/* 00001894-000018D0       .text getMsg__11daNpc_Co1_cFv */
u32 daNpc_Co1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0: msg = getMsg_CO1_0(); break;
    }
    return msg;
}

/* 000018D0-00001950       .text chkAttention__11daNpc_Co1_cFv */
bool daNpc_Co1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) return this == attention.LockonTarget(0);
    return this == attention.ActionTarget(0);
}

/* 00001950-00001A44       .text setAttention__11daNpc_Co1_cFb */
void daNpc_Co1_c::setAttention(bool force) {
    if (mAnmNo == 1) {
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y);
        cXyz offset(0.0f, 60.0f, -50.0f);
        mDoMtx_stack_c::multVec(&offset, &attention_info.position);
    } else {
        f32 ofs = l_HIO.mPrm.mAttentionOffsetY;
        attention_info.position.set(current.pos.x, current.pos.y + ofs, current.pos.z);
    }
    if (mUpdateEye || force) eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
}

/* 00001A44-00001A60       .text charDecide__11daNpc_Co1_cFi */
bool daNpc_Co1_c::charDecide(int type) {
    mType = 0;
    mSubType = -1;
    mSubType = 0;
    return true;
}

/* 00001A60-00001A9C       .text eInit_MDR___11daNpc_Co1_cFv */
void daNpc_Co1_c::eInit_MDR_() {
    setAnm_NUM(10, 1);
    mLookMode = 0;
}

/* 00001A9C-00001AC4       .text eInit_RED_LTR___11daNpc_Co1_cFv */
void daNpc_Co1_c::eInit_RED_LTR_() {
    setAnm_NUM(8, 1);
}

/* 00001AC4-00001B50       .text event_actionInit__11daNpc_Co1_cFi */
void daNpc_Co1_c::event_actionInit(int staff) {
    int* action = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    if (action != NULL) {
        mEventAction = *action;
        switch (mEventAction) {
        case 0: eInit_MDR_(); break;
        case 1: eInit_RED_LTR_(); break;
        }
    }
}

/* 00001B50-00001BA8       .text eMove_MDR___11daNpc_Co1_cFv */
bool daNpc_Co1_c::eMove_MDR_() {
    bool result = (int)mpMorf->getFrame() == 0;
    if (result) setAnm_NUM(11, 1);
    return result;
}

/* 00001BA8-00001C34       .text eMove_RED_LTR___11daNpc_Co1_cFv */
bool daNpc_Co1_c::eMove_RED_LTR_() {
    bool result = false;
    if (mAnmEnd) {
        switch (mAnmNo) {
        case 8: setAnm_NUM(4, 1); break;
        case 4:
            mPreviousState = 7;
            setStt(2);
            result = true;
            break;
        default: setAnm_NUM(8, 1); break;
        }
    }
    return result;
}

/* 00001C34-00001C84       .text event_action__11daNpc_Co1_cFv */
bool daNpc_Co1_c::event_action() {
    switch (mEventAction) {
    case 0: return eMove_MDR_();
    case 1: return eMove_RED_LTR_();
    default: return true;
    }
}

/* 00001C84-00001D70       .text privateCut__11daNpc_Co1_cFi */
void daNpc_Co1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"ACTION"};
    if (staff == -1) return;
    mCut = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, FALSE);
    if (mCut == -1) { dComIfGp_evmng_cutEnd(staff); return; }
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch (mCut) {
        case 0: event_actionInit(staff); break;
        }
    }
    bool result;
    switch (mCut) {
    case 0: result = event_action(); break;
    default: result = true; break;
    }
    if (result) dComIfGp_evmng_cutEnd(staff);
}

/* 00001D70-00001D90       .text endEvent__11daNpc_Co1_cFv */
void daNpc_Co1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

/* 00001D90-00001DC8       .text isEventEntry__11daNpc_Co1_cFv */
int daNpc_Co1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

/* 00001DC8-00001EC0       .text event_proc__11daNpc_Co1_cFi */
void daNpc_Co1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIdx[mEventNo])) {
        switch (mEventNo) {
        case 0: dComIfGs_onEventBit(0xF04); setStt(4); break;
        case 1: dComIfGs_onEventBit(0x1810); break;
        case 2: mOrder = 1; break;
        }
        endEvent();
    } else {
        if (!mEventCut.cutProc()) privateCut(staff);
        lookBack();
    }
}

/* 00001EC0-00001F6C       .text set_action__11daNpc_Co1_cFM11daNpc_Co1_cFPCvPvPv_iPv */
bool daNpc_Co1_c::set_action(ActionFunc action, void* arg) {
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

/* 00001F6C-00001FD4       .text setStt__11daNpc_Co1_cFSc */
void daNpc_Co1_c::setStt(s8 state) {
    mLookTimer = 0;
    mState = state;
    switch (mState) {
    case 1: break;
    case 2: mAnmAttr = 0xFF; mLookMode = 1; break;
    case 7: mLookMode = 0; break;
    }
    setAnm();
}

/* 00001FD4-0000205C       .text wait_1__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::wait_1() {
    if (mOrder == 1 || mOrder >= 3) return TRUE;
    if (mTalking) {
        if (chk_talk()) { mPreviousState = 4; setStt(3); }
        return TRUE;
    }
    mOrder = 2;
    return TRUE;
}

/* 0000205C-00002114       .text wait_2__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::wait_2() {
    if (mTalking) {
        if (chk_talk()) {
            if (dNpc_chkLetterPassed()) mPreviousState = 7;
            else mPreviousState = 4;
            setStt(2);
        }
        return TRUE;
    }
    mOrder = 2;
    if (mHasAttention) mLookTimer = 60;
    if (cLib_calcTimer(&mLookTimer)) mLookMode = 1;
    else mLookMode = 0;
    return TRUE;
}

/* 00002114-00002150       .text wakeup__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::wakeup() {
    if (mAnmEnd) { mPreviousState = 4; setStt(2); }
    return TRUE;
}

/* 00002150-0000234C       .text talk_1__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::talk_1() {
    BOOL notMoving = chk_partsNotMove();
    if (mOrder == 1 || mOrder >= 3) return TRUE;
    if (dNpc_chkLetterPassed() && !mReadLetter && !dComIfGs_isEventBit(0xF04)) {
        if (mOrder != 3) {
            mItemNo = 0xFF;
            mTalking = false;
            endEvent();
            mOrder = 3;
        }
        return TRUE;
    }
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case 19:
            switch (mCurrMsgNo) {
            case 0x1839: dComIfGs_onEventBit(0xF08); break;
            case 0x1848: dComIfGs_onEventBit(0xF04); break;
            case 0x1840: mReadLetter = 0; break;
            }
            if (mItemNo == 0x98) {
                mOrder = 5;
                dComIfGp_evmng_CancelPresent();
                dComIfGs_setReserveItemEmpty();
                endEvent();
                mReadLetter = 1;
            } else {
                setStt(mPreviousState);
                if (mPreviousState != 7) { mTalking = false; endEvent(); }
            }
            mItemNo = 0xFF;
            mLookTimer = 60;
            break;
        case 2: case 6:
            if (mCurrMsgNo == 0x184B) mLookMode = 0;
            break;
        }
    }
    return notMoving;
}

/* 0000234C-000023BC       .text toru_1__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::toru_1() {
    if (mTalking) {
        if (mItemNo == 0x98) { mItemNo = 0xFF; setAnm_NUM(8, 1); }
        if (mAnmEnd) setStt(6);
    }
    return FALSE;
}

/* 000023BC-000023F8       .text read_1__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::read_1() {
    if (mAnmEnd) { mPreviousState = 7; setStt(2); }
    return FALSE;
}

/* 000023F8-00002458       .text modoru__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::modoru() {
    if ((int)mpMorf->getFrame() == 0) {
        setStt(4);
        mTalking = false;
        endEvent();
    }
    return TRUE;
}

/* 00002458-000025DC       .text wait_action1__11daNpc_Co1_cFPv */
int daNpc_Co1_c::wait_action1(void* arg) {
    switch (mActionState) {
    case 0:
        if (dNpc_chkLetterPassed()) setStt(4);
        else {
            if (!dComIfGs_isEventBit(0x1810)) mOrder = 4;
            setStt(1);
        }
        mLight.mPos = current.pos;
        mLight.mPower = 0.0f;
        dKy_plight_set(&mLight);
        mActionState++;
        break;
    case 1: case 2: case 3:
        mHasAttention = chkAttention();
        switch (mState) {
        case 0: break;
        case 1: mUpdateEye = wait_1(); break;
        case 2: mUpdateEye = talk_1(); break;
        case 3: mUpdateEye = wakeup(); break;
        case 4: mUpdateEye = wait_2(); break;
        case 5: mUpdateEye = toru_1(); break;
        case 6: mUpdateEye = read_1(); break;
        case 7: mUpdateEye = modoru(); break;
        }
        lookBack();
        break;
    case 9: break;
    }
    return 1;
}

/* 000025DC-0000271C       .text demo__11daNpc_Co1_cFv */
u8 daNpc_Co1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) mDemo = false;
    } else {
        mDemo = true;
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (m_hed_tex_pttrn != NULL) {
            mBtpFrame++;
            if (mBtpFrame >= m_hed_tex_pttrn->getFrameMax()) mBtpFrame = m_hed_tex_pttrn->getFrameMax();
        }
        J3DAnmTexPattern* btp = actor->getP_BtpData("Co");
        if (btp != NULL) {
            m_hed_tex_pttrn = btp;
            if (mBtpAnm.init(mpMorf->getModel()->getModelData(), m_hed_tex_pttrn, true, 2, 1.0f, 0, -1, true, 0)) {
                mBtpNo = 7;
                mBtpFrame = 0;
            }
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Co");
    }
    return mDemo;
}

/* 0000271C-000027AC       .text shadowDraw__11daNpc_Co1_cFv */
void daNpc_Co1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
}

/* 000027AC-0000295C       .text _draw__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::_draw() {
    // Unused colors, needed for the .rodata section to match.
    GXColor red = {0xFF, 0x00, 0x00, 0x80};
    GXColor blue = {0x00, 0x00, 0xFF, 0x80};
    GXColor green = {0x00, 0xFF, 0x00, 0x80};
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    J3DModel* pearl = mpPearlMorf->getModel();
    J3DModelData* pearlData = pearl->getModelData();
    if (mHidden || mNoDraw) return TRUE;
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    dKy_tevstr_c* tev = &tevStr;
    u8 intensity = l_HIO.mPrm.mMinLightIntensity;
    u8 wave = (255 - intensity) * mLightWave;
    tev->mLightObj.mInfo.mColor.r = wave + intensity;
    g_env_light.setLightTevColorType(model, tev);
    mBtpAnm.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtpAnm.remove(data);
    g_env_light.setLightTevColorType(pearl, &tevStr);
    mBtkAnm.entry(pearlData, (f32)mBtkFrame);
    mpPearlMorf->entryDL();
    mBtkAnm.remove(pearlData);
    if (mDrawLetter) {
        g_env_light.setLightTevColorType(mpLetterModel, &tevStr);
        mDoExt_modelEntryDL(mpLetterModel);
    }
    shadowDraw();
    dSnap_RegistFig(0x8B, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 0000295C-00002B08       .text _execute__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::_execute() {
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
    mNoMove = false;
    mHidden = false;
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) staff = isEventEntry();
        if (staff >= 0) event_proc(staff);
        else (this->*mAction)(NULL);
        if (!mNoMove) fopAcM_posMoveF(this, mStts.GetCCMoveP());
        if (!mKeepShapeAngle) shape_angle = current.angle;
    }
    eventOrder();
    setMtx(false);
    if (!mDemo) setCollision_SP_();
    return TRUE;
}

/* 00002B08-00002B6C       .text _delete__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::_delete() {
    dComIfG_resDelete(&mPhs, "Co");
    dKy_plight_cut(&mLight);
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) mpMorf->stopZelAnime();
    l_HIO.removeHIO();
#else
    if (heap != NULL && mpMorf != NULL) mpMorf->stopZelAnime();
#endif
    return TRUE;
}

/* 00002B6C-00002B8C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Co1_c*)actor)->CreateHeap();
}

/* 00002B8C-00002CAC       .text _create__11daNpc_Co1_cFv */
cPhs_State daNpc_Co1_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Co");
    if (phase != cPhs_COMPLEATE_e) return phase;
    int type = fopAcM_GetParam(this) & 0xFF;
    if (!charDecide(type)) return cPhs_ERROR_e;
    l_HIO.entryHIO("コモリ");
    fopAcM_SetupActor(this, daNpc_Co1_c);
#else
    fopAcM_SetupActor(this, daNpc_Co1_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Co");
    if (phase != cPhs_COMPLEATE_e) return phase;
    if (!charDecide(fopAcM_GetParam(this) & 0xFF)) return cPhs_ERROR_e;
#endif
    static u32 a_size_tbl[] = {0x272E0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_size_tbl[mType])) return cPhs_ERROR_e;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -40.0f, -20.0f, -100.0f, 40.0f, 100.0f, 60.0f);
    if (!createInit()) return cPhs_ERROR_e;
    return phase;
}

/* 00003150-000033C0       .text create_Anm__11daNpc_Co1_cFv */
J3DModelData* daNpc_Co1_c::create_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Co", dRes_ID_CO_BDL_CO_e);
    JUT_ASSERT(DEMO_SELECT(0x81C, 0x81F), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Co", dRes_ID_CO_BCK_CO_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL) return NULL;
    if (mpMorf->getModel() == NULL) { mpMorf = NULL; return NULL; }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x830, 0x833), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x833, 0x836), m_bbone_jnt_num >= 0);
    m_hnd_R_jnt_num = a_mdl_dat->getJointName()->getIndex("handR");
    JUT_ASSERT(DEMO_SELECT(0x836, 0x839), m_hnd_R_jnt_num >= 0);
    return a_mdl_dat;
}

/* 000033C0-00003578       .text create_prl_Anm__11daNpc_Co1_cFv */
J3DModelData* daNpc_Co1_c::create_prl_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Co", dRes_ID_CO_BDL_CO_PEAL_e);
    JUT_ASSERT(DEMO_SELECT(0x845, 0x848), a_mdl_dat != 0);
    mpPearlMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Co", dRes_ID_CO_BCK_COPEAL_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x15021222);
    if (mpPearlMorf == NULL) return NULL;
    if (mpPearlMorf->getModel() == NULL) { mpPearlMorf = NULL; return NULL; }
    m_prl_jnt_num = a_mdl_dat->getJointName()->getIndex("co_pearl");
    JUT_ASSERT(DEMO_SELECT(0x859, 0x85C), m_prl_jnt_num >= 0);
    return a_mdl_dat;
}

/* 00003578-00003634       .text create_itm_Mdl__11daNpc_Co1_cFv */
bool daNpc_Co1_c::create_itm_Mdl() {
    mpLetterModel = NULL;
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Co", dRes_ID_CO_BDL_CO_LETTER_e);
    JUT_ASSERT(DEMO_SELECT(0x86B, 0x86E), a_mdl_dat != 0);
    mpLetterModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000002);
    return true;
}

/* 00003634-000037C8       .text CreateHeap__11daNpc_Co1_cFv */
BOOL daNpc_Co1_c::CreateHeap() {
    J3DModelData* data = create_Anm();
    if (data == NULL) return FALSE;
    mBtpNo = 6;
    if (!iniTexPttrnAnm(false)) { mpMorf = NULL; return FALSE; }
    if (create_prl_Anm() == NULL) { mpMorf = NULL; return FALSE; }
    if (!setBtk(false)) { mpMorf = NULL; mpPearlMorf = NULL; return FALSE; }
    if (create_itm_Mdl()) {
        for (u16 i = 0; i < data->getJointNum(); i++) {
            if (i == m_hed_jnt_num || i == m_bbone_jnt_num) {
                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Co1);
            }
        }
        mpMorf->getModel()->setUserArea((u32)this);
        mAcchCir.SetWall(30.0f, 40.0f);
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
        return TRUE;
    }
    mpMorf = NULL;
    mpPearlMorf = NULL;
    return FALSE;
}

/* 000037C8-000037E8       .text daNpc_Co1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Co1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Co1_c*)i_this)->_create();
}

/* 000037E8-00003808       .text daNpc_Co1_Delete__FP11daNpc_Co1_c */
static BOOL daNpc_Co1_Delete(daNpc_Co1_c* i_this) {
    return ((daNpc_Co1_c*)i_this)->_delete();
}

/* 00003808-00003828       .text daNpc_Co1_Execute__FP11daNpc_Co1_c */
static BOOL daNpc_Co1_Execute(daNpc_Co1_c* i_this) {
    return ((daNpc_Co1_c*)i_this)->_execute();
}

/* 00003828-00003848       .text daNpc_Co1_Draw__FP11daNpc_Co1_c */
static BOOL daNpc_Co1_Draw(daNpc_Co1_c* i_this) {
    return ((daNpc_Co1_c*)i_this)->_draw();
}

/* 00003848-00003850       .text daNpc_Co1_IsDelete__FP11daNpc_Co1_c */
static BOOL daNpc_Co1_IsDelete(daNpc_Co1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Co1_Method = {
    (process_method_func)daNpc_Co1_Create,
    (process_method_func)daNpc_Co1_Delete,
    (process_method_func)daNpc_Co1_Execute,
    (process_method_func)daNpc_Co1_IsDelete,
    (process_method_func)daNpc_Co1_Draw,
};

actor_process_profile_definition g_profile_NPC_CO1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_CO1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Co1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_CO1_e,
    /* Actor SubMtd */ &l_daNpc_Co1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
