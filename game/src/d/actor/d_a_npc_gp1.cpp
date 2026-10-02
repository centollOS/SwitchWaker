/**
 * d_a_npc_gp1.cpp
 * NPC - Maggie's Father (rich)
 */
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_gp1.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "d/d_demo.h"
#include "SSystem/SComponent/c_counter.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Gp.h"
#include "m_Do/m_Do_hostIO.h"

#if VERSION == VERSION_DEMO
class daNpc_Gp1_HIO_c : public JORReflexible {
#else
class daNpc_Gp1_HIO_c : public mDoHIO_entry_c {
#endif
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackX, mMaxBackY, mMinBackX, mMinBackY;
        s16 mMaxTurn, mTurnSpeed;
        f32 mAttentionYOffset;
        u8 mDebug;
        f32 mForceTalkDistance;
        s16 mAngleScale, mAngleMax;
        f32 mAnimSpeedScale, mTargetSpeed, mSpeedStep, mArriveDistance;
    };
    daNpc_Gp1_HIO_c();
    virtual ~daNpc_Gp1_HIO_c() {}
#if VERSION == VERSION_DEMO
    void genMessage(JORMContext*) {}
#endif

public:
    s8 mNo;
    int mCount;
    hio_prm_c mPrm;
};

static daNpc_Gp1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;
/* 000000EC-00000150       .text __ct__15daNpc_Gp1_HIO_cFv */
daNpc_Gp1_HIO_c::daNpc_Gp1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x2000, 0x2A00, (s16)0xF254, -0x2A00,
        0, 0x1C00, 0, -0x1C00, 0x400, 0x400,
        233.0f, 0, 300.0f, 6, 0x400, 0.4f, 3.0f, 0.2f, 20.0f,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(mPrm));
    mNo = -1;
    mCount = -1;
}
static char* l_evn_tbl[] = {"Kaere_Kaere", "Gp1_Get_Itm", "Giv_Rupee"};
/* 00000198-00000210       .text searchActor_Bm__FPvPv */
static void* searchActor_Bm(void* actor, void*) {
    if (l_check_wrk < 20 && fopAcM_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_NPC_BM4_e) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

/* 00000210-0000025C       .text nodeCallBack_Gp1__FP7J3DNodei */
static BOOL nodeCallBack_Gp1(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Gp1_c* npc = (daNpc_Gp1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->nodeGp1Control(node, model);
        }
    }
    return TRUE;
}

/* 0000025C-000003AC       .text nodeGp1Control__11daNpc_Gp1_cFP7J3DNodeP8J3DModel */
void daNpc_Gp1_c::nodeGp1Control(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(24.0f, -24.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_jnt_num) {
        mDoMtx_stack_c::YrotM(-m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
        mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyeLocal);
    }
    if (joint == m_bbone_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM(m_jnt.getBackbone_x());
    }
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 000003E8-00000468       .text init_GP1_0__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::init_GP1_0() {
    if (dComIfGs_isEventBit(0x2D01)) {
        set_action(&daNpc_Gp1_c::wait_action1, NULL);
        return true;
    }
    return false;
}

/* 00000468-00000638       .text createInit__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::createInit() {
    bool result;
    int weight;
    int i;
    for (i = 0; i < 3; i++) {
        mEventIds[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    }
    attention_info.flags = 0xA;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAB;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAB;
    gravity = -4.5f;
    mLookPos = current.pos;
    weight = 0xFF;
    u8 pathNo = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (pathNo != 0xFF) {
        mPath.setInf(pathNo, fopAcM_GetRoomNo(this), 1);
        if (mPath.isPath()) {
            actor_status &= ~fopAcStts_NOCULLEXEC_e;
            weight = DEMO_SELECT(0xF0, 0xD9);
        } else return false;
    }
    mCut.setActorInfo2("Gp1", this);
    mAnmIndex = 8;
    switch (mSubtype) {
    case 0:
        result = init_GP1_0();
        break;
        default:
        result = false;
        break;
    }
    if (result) {
        mModelAngle = current.angle;
        shape_angle = mModelAngle;
    } else return false;
    mStts.Init(weight, 0xFF, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

/* 00000638-00000758       .text setMtx__11daNpc_Gp1_cFb */
void daNpc_Gp1_c::setMtx(bool force) {
    if (!mDemo) {
        plyTexPttrnAnm();
        mAnmEnded = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mLastFrame) {
            mAnmEnded = 1;
        }
        mLastFrame = mpMorf->getFrame();
        ctrl_WAITanm();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(mModelAngle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    setAttention(force);
}

/* 00000758-0000076C       .text anmNum_toResID__11daNpc_Gp1_cFi */
int daNpc_Gp1_c::anmNum_toResID(int index) {
    static const int a_bck_resID_tbl[] = {dRes_ID_GP_BCK_WAIT01_e, dRes_ID_GP_BCK_WAIT01_e, dRes_ID_GP_BCK_TALK02_e, dRes_ID_GP_BCK_TALK03_e, dRes_ID_GP_BCK_HAIR_e, dRes_ID_GP_BCK_GRIN_e, dRes_ID_GP_BCK_GIVE_e, dRes_ID_GP_BCK_WALK_e};
    return a_bck_resID_tbl[index];
}

/* 0000076C-00000780       .text btpNum_toResID__11daNpc_Gp1_cFi */
int daNpc_Gp1_c::btpNum_toResID(int index) {
    static const int a_btp_resID_tbl[] = {dRes_ID_GP_BTP_MABA01_e};
    return a_btp_resID_tbl[index];
}

/* 00000780-00000890       .text setBtp__11daNpc_Gp1_cFbi */
bool daNpc_Gp1_c::setBtp(bool modify, int index) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    m_hed_tex_pttrn = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Gp", btpNum_toResID(index));
    JUT_ASSERT(435, m_hed_tex_pttrn != 0);
    bool result = mBtp.init(data, m_hed_tex_pttrn, TRUE, 2, 1.0f, 0, -1, modify, FALSE) == TRUE;
    if (result) {
        mBtpFrame = 0;
        mBlinkTimer = 0;
    }
    return result;
}

/* 00000890-000008B8       .text iniTexPttrnAnm__11daNpc_Gp1_cFb */
bool daNpc_Gp1_c::iniTexPttrnAnm(bool modify) {
    return setBtp(modify, mTexIndex);
}

/* 000008B8-00000964       .text plyTexPttrnAnm__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::plyTexPttrnAnm() {
    if (mTexIndex != 0 || cLib_calcTimer(&mBlinkTimer) == 0) {
        mBtpFrame++;
        if (mBtpFrame >= m_hed_tex_pttrn->getFrameMax()) {
            if (mTexIndex != 0) {
                mBtpFrame = m_hed_tex_pttrn->getFrameMax();
            } else {
                mBlinkTimer = 30 + cM_rndF(60.0f);
                mBtpFrame = 0;
            }
        }
    }
}

/* 00000964-000009A0       .text setAnm_tex__11daNpc_Gp1_cFSc */
void daNpc_Gp1_c::setAnm_tex(s8 index) {
    if (mTexIndex != index) {
        mTexIndex = index;
        iniTexPttrnAnm(true);
    }
}

/* 000009A0-00000A78       .text setAnm_anm__11daNpc_Gp1_cFPQ211daNpc_Gp1_c9anm_prm_c */
BOOL daNpc_Gp1_c::setAnm_anm(anm_prm_c* prm) {
    if (mAnmIndex == prm->mAnm) {
        return TRUE;
    }
    mAnmIndex = prm->mAnm;
    int res = anmNum_toResID(mAnmIndex);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, res, -1, "Gp");
    if (mAnmIndex == 0) {
        mIdleAnimTimer = 180 + cM_rndF(120.0f);
    }
    mLastFrame = 0.0f;
    mAnmRepeat = 0;
    mAnmEnded = 0;
    return TRUE;
}

/* 00000A78-00000AE4       .text setAnm_NUM__11daNpc_Gp1_cFii */
BOOL daNpc_Gp1_c::setAnm_NUM(int index, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0,0,8.0f,1.0f,2}, {1,0,8.0f,1.0f,2}, {2,0,8.0f,1.0f,2},
        {3,0,8.0f,1.0f,2}, {4,0,8.0f,1.0f,0}, {5,0,8.0f,1.0f,2},
        {6,0,8.0f,1.0f,0}, {0}, {0},
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[index].mTex);
    }
    return setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 00000AE4-00000B64       .text setAnm__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1,-1,0,0,-1}, {0,0,8.0f,1.0f,2}, {-1,-1,0,0,-1},
        {0,0,8.0f,1.0f,2}, {7,0,8.0f,1.0f,2}, {4,0,8.0f,1.0f,0},
    };
    if (a_anm_prm_tbl[mState].mTex >= 0) {
        setAnm_tex(a_anm_prm_tbl[mState].mTex);
    }
    if (a_anm_prm_tbl[mState].mAnm >= 0) {
        setAnm_anm(&a_anm_prm_tbl[mState]);
    }
    return TRUE;
}

/* 00000B64-00000B68       .text chg_anmTag__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::chg_anmTag() {
}

/* 00000B68-00000B6C       .text control_anmTag__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::control_anmTag() {
}

/* 00000B6C-00000BB0       .text chg_anmAtr__11daNpc_Gp1_cFUc */
void daNpc_Gp1_c::chg_anmAtr(u8 attr) {
    if (attr >= 7 || attr == mAnmAttr) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR(1);
}

/* 00000BB0-00000D00       .text control_anmAtr__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::control_anmAtr() {
    switch (mAnmAttr) {
    case 0:
        break;
    case 1:
        if (mAnmEnded) {
            mAnmAttr = 0;
            setAnm_NUM(0, 1);
        }
        break;
    case 5:
        if (!mAnmEnded) {
            if (mpMorf->checkFrame(80.0f)) {
                create_rupee();
                mDoAud_seStart(JA_SE_OBJ_LUPY_OUT, NULL, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
                mRupeeCount = 0;
            }
        } else {
            mAnmAttr = 0;
            setAnm_NUM(0, 1);
        }
        break;
    case 6:
        if (mAnmEnded) {
            if (++mAnmRepeat >= 2) {
                mAnmAttr = 0;
                setAnm_NUM(0, 1);
            }
        }
        break;
    }
}

/* 00000D00-00000D68       .text setAnm_ATR__11daNpc_Gp1_cFi */
BOOL daNpc_Gp1_c::setAnm_ATR(int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0,0,8.0f,1.0f,2}, {5,0,8.0f,1.0f,2}, {2,0,8.0f,1.0f,2},
        {3,0,8.0f,1.0f,2}, {5,0,8.0f,1.0f,2}, {6,0,8.0f,1.0f,0}, {2,0,8.0f,1.0f,2},
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[mAnmAttr].mTex);
    }
    return setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00000D68-00000E2C       .text anmAtr__11daNpc_Gp1_cFUs */
void daNpc_Gp1_c::anmAtr(u16 status) {
    switch (status) {
    case 6:
        if (mMessageState == 0) {
            mAnmAttr = 0xFF;
            chg_anmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMessageState++;
        }
        {
            u8 tag = dComIfGp_getMesgAnimeTagInfo();
            dComIfGp_clearMesgAnimeTagInfo();
            if (tag != 0xFF && mAnmTag != tag) {
                mAnmTag = tag;
                chg_anmTag();
            }
        }
        break;
    case 14:
        mMessageState = 0;
        break;
    }
    control_anmTag();
    control_anmAtr();
}

/* 00000E2C-00000EC4       .text eventOrder__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::eventOrder() {
    if (mEventOrder == 1 || mEventOrder == 2) {
        eventInfo.onCondition(1);
        eventInfo.onCondition(0x20);
        if (mEventOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (mEventOrder >= 3) {
        mEventIndex = mEventOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIds[mEventIndex], 0xFF, 0xFFFF, 0, 1);
    }
}

/* 00000EC4-00000F5C       .text checkOrder__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEventIds[mEventIndex])) {
            mEventOrder = 0;
        }
    } else if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mEventOrder == 1 || mEventOrder == 2)) {
        mEventOrder = 0;
        mTalking = true;
    }
}

/* 00000F5C-00001008       .text chk_talk__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::chk_talk() {
    bool result = true;
    mPresentItem = 0xFF;
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mPresentItem = dComIfGp_event_getPreItemNo();
        } else {
            result = false;
        }
        mPresentItem = dComIfGp_event_getPreItemNo();
    }
    return result;
}

// Unused; stripped by the linker. Name/signature from d_a_npc_gp1.map; body reconstructed from its leftover
// DEG2S .rodata literal (0x43360B61), as in daNpc_Ba1_c::chk_drct.
bool daNpc_Gp1_c::chk_drct(f32 i_deg) {
    s16 target = cLib_targetAngleY(&current.pos, &dComIfGp_getPlayer(0)->current.pos) - current.angle.y;
    int diff = abs(target);
    return diff < cM_deg2s(i_deg);
}

/* 00001008-00001048       .text chk_partsNotMove__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::chk_partsNotMove() {
    return mOldHeadY == m_jnt.getHead_y() && mOldBackY == m_jnt.getBackbone_y() && mOldActorY == current.angle.y;
}
/* 00001048-00001168       .text chk_forceTlkArea__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::chk_forceTlkArea() {
    f32 distance = (dComIfGp_getPlayer(0)->current.pos - current.pos).absXZ();
    f32 dy = dComIfGp_getPlayer(0)->current.pos.y - current.pos.y;
    bool result = false;
    if (-1.0f < dy && dy <= 100.0f && distance < l_HIO.mPrm.mForceTalkDistance) {
        result = true;
    }
    return result;
}

/* 00001168-00001308       .text lookBack__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::lookBack() {
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
        target = dNpc_playerEyePos(-20.0f);
        targetP = &target;
        source = current.pos;
        source.y = eyePos.y;
        break;
    case 2:
        target = mLookPos;
        targetP = &target;
        source = current.pos;
        source.y = eyePos.y;
        break;
    case 3:
        angle = mLookAngle;
        break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mPrm.mTurnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) {
        mTurnSpeed = 0;
    }
    m_jnt.lookAtTarget(&current.angle.y, targetP, source, angle, mTurnSpeed, noTurn);
}

/* 00001308-00001504       .text next_msgStatus__11daNpc_Gp1_cFPUl */
u16 daNpc_Gp1_c::next_msgStatus(u32* msg) {
    u16 result = 15;
    s16 count = dComIfGs_getBeastNum(dBeastIdx_SKULL_NECKLACE_e);
    switch (*msg) {
    case 0x1E15:
        *msg = 0x1E16;
        break;
    case 0x1E16:
        *msg = 0x1E17;
        break;
    case 0x1E17:
        *msg = 0x1E18;
        break;
    case 0x1E19:
        *msg = 0x1E1A;
        break;
    case 0x1E1A:
        *msg = 0x1E1B;
        break;
    case 0x1E1B:
        *msg = 0x1E1C;
        break;
    case 0x1E29:
        *msg = 0x1E2A;
        mNecklaceCount = count;
        mConsumeNecklaces = 1;
        break;
    case 0x1E2A:
        if (mNecklaceCount >= 16) {
            *msg = 0x1E2D;
            mRupeeCount = 3;
        } else if (mNecklaceCount >= 6) {
            *msg = 0x1E2C;
            mRupeeCount = 2;
        } else {
            *msg = 0x1E2B;
            mRupeeCount = 1;
        }
        break;
    case 0x1E20:
        *msg = 0x1E21;
        break;
    case 0x1E30:
        *msg = 0x1E23;
        break;
    case 0x1E22:
    case 0x1E28:
        *msg = 0x1E24;
        mConsumeNecklaces = 1;
        mNecklaceCount = 20;
        break;
    case 0x1E24:
        *msg = 0x1E25;
        break;
    case 0x1E26:
        *msg = 0x1E27;
        break;
    case 0x1E21:
        if (count >= 20) {
            *msg = 0x1E22;
        } else {
            *msg = 0x1E30;
        }
        break;
    case 0x1EE6:
        *msg = 0x1EE7;
        break;
        default:
        result = 16;
        break;
    }
    return result;
}

/* 00001504-0000166C       .text getMsg_GP1_0__11daNpc_Gp1_cFv */
u32 daNpc_Gp1_c::getMsg_GP1_0() {
    if (mPresentItem == 0x45) {
        if (dComIfGs_isEventBit(0x1420)) {
            return 0x1E29;
        }
        if (dComIfGs_isEventBit(0x1804)) {
            return (s16)dComIfGs_getBeastNum(dBeastIdx_SKULL_NECKLACE_e) >= 20 ? 0x1E28 : 0x1E30;
        }
        return 0x1E20;
    }
    if (mPresentItem != 0xFF) {
        return 0x1E2F;
    }
    if (mAfterItem) {
        mAfterItem = 0;
        return 0x1E26;
    }
    if (!dComIfGs_isEventBit(0x1501)) {
        return 0x1E15;
    }
    if (dComIfGs_isEventBit(0x1808)) {
        if (dComIfGs_checkReserveItem(0x9B)) {
            return 0x1E1D;
        }
        return dComIfGs_getEventReg(0xC5FF) >= 10 ? 0x1E1E : 0x1E1F;
    }
    return dComIfGs_isEventBit(0x1920) ? 0x1E1B : 0x1E19;
}

/* 0000166C-000016A8       .text getMsg__11daNpc_Gp1_cFv */
u32 daNpc_Gp1_c::getMsg() {
    u32 result = 0;
    switch (mSubtype) {
    case 0:
        result = getMsg_GP1_0();
        break;
    }
    return result;
}

/* 000016A8-00001728       .text chkAttention__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::chkAttention() {
    dAttention_c& attn = dComIfGp_getAttention();
    if (attn.LockonTruth()) {
        return this == attn.LockonTarget(0);
    }
    return this == attn.ActionTarget(0);
}

/* 00001728-00001790       .text setAttention__11daNpc_Gp1_cFb */
void daNpc_Gp1_c::setAttention(bool force) {
    f32 ofs = l_HIO.mPrm.mAttentionYOffset;
    attention_info.position.set(current.pos.x, current.pos.y + ofs, current.pos.z);
    if (mUpdateEye || force) {
        eyePos.set(mEyeLocal.x, mEyeLocal.y, mEyeLocal.z);
        eyePos.y += mEyeYOffset;
    }
}

/* 00001790-000017C4       .text searchByID__11daNpc_Gp1_cFUi */
fopAc_ac_c* daNpc_Gp1_c::searchByID(fpc_ProcID id) {
    fopAc_ac_c* actor = NULL;
    fopAcM_SearchByID(id, &actor);
    return actor;
}

/* 000017C4-00001874       .text partner_srch_sub__11daNpc_Gp1_cFPFPvPv_Pv */
bool daNpc_Gp1_c::partner_srch_sub(void* (*callback)(void*, void*)) {
    bool result = false;
    mPartnerID = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < 20; i++) {
        l_check_inf[i] = NULL;
    }
    fpcM_Search(callback, this);
    if (l_check_wrk != 0) {
        mPartnerID = fopAcM_GetID(l_check_inf[0]);
        result = true;
    }
    return result;
}

/* 00001874-00001918       .text partner_srch__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::partner_srch() {
    if (mActionState == 1) {
        bool found;
        switch (mState) {
        case 3:
            found = partner_srch_sub(searchActor_Bm);
            if (found) {
                fopAc_ac_c* actor = searchByID(mPartnerID);
                if (actor != NULL) {
                    current.angle.y = cLib_targetAngleY(&current.pos, &actor->current.pos);
                }
            }
            break;
            default:
            found = true;
            break;
        }
        if (found) {
            mActionState++;
        }
    }
}

/* 00001918-000019AC       .text ctrl_WAITanm__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::ctrl_WAITanm() {
    switch (mAnmIndex) {
    case 0:
        if (cLib_calcTimer(&mIdleAnimTimer) == 0) {
            setAnm_NUM(4, 1);
        }
        break;
    case 4:
        if (mAnmEnded) {
            setAnm_NUM(0, 1);
            mpMorf->setMorf(0.0f);
        } break;
    }
}

/* 000019AC-00001B1C       .text gp_movPass__11daNpc_Gp1_cFv */
int daNpc_Gp1_c::gp_movPass() {
    int result = 0;
    if (mPath.isPath() && dPath_ChkClose(mPath.getPath())) {
        if (mPath.chkPointPass(current.pos, (u8)(mPath.getDir() != 0))) {
            mPath.nextIdxAuto();
            result = 1;
        }
        return result;
    }
    f32 distance = (mMoveTarget - current.pos).absXZ();
    if (distance <= mArriveDistance) {
        result = 1;
        if (mPath.isPath() && !mPath.nextIdxAuto()) {
            result = 2;
        }
    }
    return result;
}

/* 00001B1C-00001B80       .text gp_clcMovSpd__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::gp_clcMovSpd() {
    cLib_addCalcAngleS(&current.angle.y, cLib_targetAngleY(&current.pos, &mMoveTarget), l_HIO.mPrm.mAngleScale, l_HIO.mPrm.mAngleMax, 0);
    cLib_chaseF(&speedF, mTargetSpeed, mSpeedStep);
}

/* 00001B80-00001C40       .text gp_nMove__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::gp_nMove() {
    if (mArrived) {
        mStopMove = 1;
        return;
    }
    switch (mMoveMode) {
    case 1:
        gp_clcMovSpd();
        f32 playSpeed = speedF * l_HIO.mPrm.mAnimSpeedScale;
        if (playSpeed < 0.5f) {
            playSpeed = 0.5f;
        }
        mpMorf->setPlaySpeed(playSpeed);
        switch (gp_movPass()) {
        case 1:
            mArrived = 1;
            break;
        case 2:
            mArrived = 1;
            mMoveMode = 0;
            break;
        }
        break;
    }
}

/* 00001C40-00001ED8       .text create_rupee__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::create_rupee() {
    cXyz itemScale(0.2f, 0.2f, 0.2f);
    csXyz angle(0, 0, 0);
    cXyz pos;
    fopAc_ac_c* item;
    s8 room;
    int i;
    int counter;
    f32 random;
    s16 angleOffset;
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hnd_L_jnt_num));
    mDoMtx_stack_c::multVecZero(&pos);
    counter = g_Counter.mCounter0;
    for (i = 0; i < mRupeeCount; i++, counter++) {
        f32 offsets[] = {-30.0f, 0.0f, 30.0f};
        random = offsets[counter % 3] + (cM_rndF(30.0f) - 15.0f);
        angleOffset = cM_deg2s((s16)random);
        angle.y = current.angle.y + angleOffset;
        room = fopAcM_GetRoomNo(this);
        item = (fopAc_ac_c*)fopAcM_fastCreateItem(&pos, 4, room, NULL, NULL, 10.0f + cM_rndFX(3.0f), 33.0f + cM_rndFX(6.0f), -2.0f, -1, NULL);
        if (item == NULL) {
            break;
        }
        item->actor_status |= fopAcStts_UNK4000_e;
        item->scale = itemScale;
        item->current.angle = angle;
        item->shape_angle = angle;
    }
    return TRUE;
}

/* 00001ED8-00001EF4       .text charDecide__11daNpc_Gp1_cFi */
bool daNpc_Gp1_c::charDecide(int) {
    mType = 0;
    mSubtype = -1;
    mSubtype = 0;
    return true;
}

/* 00001EF4-00001F40       .text eInit_INI_KAERE_KAERE___11daNpc_Gp1_cFv */
void daNpc_Gp1_c::eInit_INI_KAERE_KAERE_() {
    fopAc_ac_c* actor = searchByID(mPartnerID);
    if (actor != NULL) {
        dComIfGp_event_setItemPartner(actor);
    }
}

/* 00001F40-00001F4C       .text eInit_END_KAERE_KAERE___11daNpc_Gp1_cFv */
void daNpc_Gp1_c::eInit_END_KAERE_KAERE_() {
    current.angle.y = mInitialAngle.y;
}

/* 00001F4C-00001FE0       .text event_actionInit__11daNpc_Gp1_cFi */
void daNpc_Gp1_c::event_actionInit(int staff) {
    int* action = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    if (action != NULL) {
        mEventAction = *action;
        mUpdateEye = 0;
        switch (mEventAction) {
        case 0:
            eInit_INI_KAERE_KAERE_();
            break;
        case 1:
            eInit_END_KAERE_KAERE_();
            break;
        }
    }
}

/* 00001FE0-00001FF4       .text event_action__11daNpc_Gp1_cFv */
bool daNpc_Gp1_c::event_action() {
    switch (mEventAction) {
    case 0:
    case 1:
        default:
        return true;
    }
}

/* 00001FF4-000020E0       .text privateCut__11daNpc_Gp1_cFi */
void daNpc_Gp1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"ACTION"};
    if (staff != -1) {
        mCutIndex = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, TRUE, 0);
        if (mCutIndex == -1) {
            dComIfGp_evmng_cutEnd(staff);
        } else {
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
                default:
                done = true;
                break;
            }
            if (done) {
                dComIfGp_evmng_cutEnd(staff);
            }
        }
    }
}

/* 000020E0-00002100       .text endEvent__11daNpc_Gp1_cFv */
void daNpc_Gp1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

/* 00002100-00002138       .text isEventEntry__11daNpc_Gp1_cFv */
int daNpc_Gp1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mCut.getActorName());
}

/* 00002138-00002268       .text event_proc__11daNpc_Gp1_cFi */
void daNpc_Gp1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIds[mEventIndex])) {
        switch (mEventIndex) {
        case 0:
            dComIfGs_onEventBit(0x1808);
            actor_status |= fopAcStts_NOCULLEXEC_e;
            setStt(1);
            mWaitTimer = 0;
            mHairTimer = (g_Counter.mCounter0 & 3) + 2;
            break;
        case 1:
            dComIfGs_onEventBit(0x1420);
            mEventOrder = 1;
            mAfterItem = 1;
            break;
        case 2:
            setStt(1);
            break;
        }
        endEvent();
    } else {
        if (!mCut.cutProc()) {
            privateCut(staff);
        }
        lookBack();
    }
}

/* 00002268-00002314       .text set_action__11daNpc_Gp1_cFM11daNpc_Gp1_cFPCvPvPv_iPv */
bool daNpc_Gp1_c::set_action(int (daNpc_Gp1_c::*action)(void*), void* arg) {
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

/* 00002314-0000240C       .text setStt__11daNpc_Gp1_cFSc */
void daNpc_Gp1_c::setStt(s8 state) {
    s8 previous = mState;
    mAttentionTimer = 0;
    mState = state;
    switch (mState) {
    case 1:
        mWaitTimer = 90;
        break;
    case 2:
        mAnmAttr = 0xFF;
        mLookMode = 1;
        m_jnt.setTrn();
        mPreviousState = previous;
        return;
    case 4:
        mLookMode = 0;
        mMoveMode = 1;
        mWaitTimer = 180 + cM_rndF(120.0f);
        mTargetSpeed = l_HIO.mPrm.mTargetSpeed;
        mSpeedStep = l_HIO.mPrm.mSpeedStep;
        mArriveDistance = l_HIO.mPrm.mArriveDistance;
        break;
    case 5:
        mLookMode = 0;
        break;
    }
    setAnm();
}

/* 0000240C-0000255C       .text wait_1__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::wait_1() {
    mIdleAnimTimer = 180 + cM_rndF(120.0f);
#if VERSION == VERSION_DEMO
    if (mEventOrder == 1 || mEventOrder >= 3) {
        return TRUE;
    }
#endif
    if (mTalking && mAnmIndex != 4) {
        if (chk_talk()) {
            setStt(2);
            mNoTurn = false;
        }
        return TRUE;
    }
#if VERSION == VERSION_DEMO
    mEventOrder = 2;
#else
    if (mEventOrder != 1 && mEventOrder < 3) {
        mEventOrder = 2;
    }
#endif
    if (chk_forceTlkArea() || mHasAttention) {
        mAttentionTimer = 60;
    }
    if (cLib_calcTimer(&mAttentionTimer) != 0) {
        mLookMode = 1;
        return TRUE;
    }
    if (cLib_calcTimer(&mWaitTimer) == 0) {
        setStt(4);
        return TRUE;
    }
    mLookMode = 2;
    mLookPos.set(-2.0f, eyePos.y, 940.0f);
    m_jnt.setTrn();
    return TRUE;
}

/* 0000255C-00002724       .text talk_1__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::talk_1() {
    BOOL result = chk_partsNotMove();
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case 0x13:
            switch (mCurrMsgNo) {
            case 0x1E18:
                dComIfGs_onEventBit(0x1501);
                break;
            case 0x1E1C:
                dComIfGs_onEventBit(0x1920);
                break;
            case 0x1E23:
                dComIfGs_onEventBit(0x1804);
                break;
            case 0x1E25:
                mEventOrder = 4;
                break;
            case 0x1E2B:
            case 0x1E2C:
            case 0x1E2D:
                mEventOrder = 5;
                break;
            }
            mPresentItem = 0xFF;
            mTalking = false;
            setStt(mPreviousState);
            mAttentionTimer = 60;
            endEvent();
            break;
        case 2:
        case 6:
            if (mConsumeNecklaces) {
                dComIfGp_evmng_CancelPresent();
                if (mNecklaceCount != 0) {
                    int count = dComIfGs_getEventReg(0xC5FF);
                    count &= 0xFF;
                    dComIfGp_setItemBeastNumCount(dBeastIdx_SKULL_NECKLACE_e, -mNecklaceCount);
                    count = cLib_maxLimit<int>(count + mNecklaceCount, 127);
                    dComIfGs_setEventReg(0xC5FF, count);
                }
                mConsumeNecklaces = 0;
            }
            break;
        }
    }
    return result;
}

/* 00002724-0000288C       .text walk_1__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::walk_1() {
    if (mTalking || chk_forceTlkArea() || cLib_calcTimer(&mWaitTimer) == 0) {
        mTargetSpeed = 0.0f;
    }
#if VERSION == VERSION_DEMO
    mEventOrder = 2;
#else
    if (mEventOrder != 1 && mEventOrder < 3) {
        mEventOrder = 2;
    }
#endif
    if (mArrived) {
        mArrived = 0;
        if (mMoveMode == 0) {
            mPath.setIdx(0);
            mMoveMode = 1;
        }
    }
    u8 point = mPath.getIdx();
    mMoveTarget = mPath.getPoint(point);
    if ((int)mTargetSpeed == 0 && (int)speedF == 0) {
        if (mTalking || cLib_calcTimer(&mHairTimer) != 0) {
            setStt(1);
        } else {
            setStt(5);
            mHairTimer = (g_Counter.mCounter0 & 3) + 2;
        }
        speedF = 0.0f;
        mSpeedStep = 0.0f;
        mMoveMode = 0;
    }
    return TRUE;
}

/* 0000288C-00002930       .text hair_1__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::hair_1() {
    if (mAnmEnded) {
        if (mTalking || chk_forceTlkArea()) {
            setStt(1);
            return TRUE;
        }
        setStt(4);
        return TRUE;
    }
#if VERSION == VERSION_DEMO
    if (!mTalking) {
#else
    if (!mTalking && mEventOrder != 1 && mEventOrder < 3) {
#endif
        mEventOrder = 2;
    }
    return TRUE;
}

/* 00002930-00002988       .text wait_2__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::wait_2() {
#if VERSION == VERSION_DEMO
    if (mEventOrder != 3) {
#else
    if (mEventOrder != 1 && mEventOrder < 3) {
#endif
        f32 dy = dComIfGp_getPlayer(0)->current.pos.y - current.pos.y;
        mEventOrder = 0;
        if (dy > -1.0f) {
            mEventOrder = 3;
        }
    }
    return TRUE;
}

/* 00002988-00002B2C       .text wait_action1__11daNpc_Gp1_cFPv */
int daNpc_Gp1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        if (dNpc_chkArasoi()) {
            actor_status &= ~fopAcStts_NOCULLEXEC_e;
            setStt(3);
        } else {
            actor_status |= fopAcStts_NOCULLEXEC_e;
            setStt(1);
            mHairTimer = (g_Counter.mCounter0 & 3) + 2;
        }
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
        case 4:
            mUpdateEye = walk_1();
            break;
        case 5:
            mUpdateEye = hair_1();
            break;
        case 3:
            mUpdateEye = wait_2();
            break;
        }
#if VERSION == VERSION_DEMO
        if (!dComIfGs_isEventBit(0x1501) && !mTalking && chk_forceTlkArea()) {
#else
        if (mEventOrder != 1 && mEventOrder < 3 && !dComIfGs_isEventBit(0x1501) && !mTalking && chk_forceTlkArea()) {
#endif
            mEventOrder = 1;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00002B2C-00002C6C       .text demo__11daNpc_Gp1_cFv */
u8 daNpc_Gp1_c::demo() {
    if (demoActorID == 0) {
        if (mDemo) {
            mDemo = 0;
        }
    } else {
        mDemo = 1;
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (m_hed_tex_pttrn != NULL) {
            mBtpFrame++;
            if (mBtpFrame >= m_hed_tex_pttrn->getFrameMax()) {
                mBtpFrame = m_hed_tex_pttrn->getFrameMax();
            }
        }
        J3DAnmTexPattern* pattern = actor->getP_BtpData("Gp");
        if (pattern != NULL) {
            m_hed_tex_pttrn = pattern;
            if (mBtp.init(mpMorf->getModel()->getModelData(), m_hed_tex_pttrn, TRUE, 2, 1.0f, 0, -1, true, FALSE)) {
                mTexIndex = 1;
                mBtpFrame = 0;
            }
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Gp");
    }
    return mDemo;
}
/* 00002C6C-00002E24       .text _draw__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::_draw() {
    GXColor dummy_1 = {0xFF, 0x00, 0x00, 0x80};
    GXColor dummy_2 = {0x00, 0x00, 0xFF, 0x80};
    GXColor dummy_3 = {0x00, 0xFF, 0x00, 0x80};
    GXColor dummy_4 = {0x00, 0xFF, 0x00, 0x80};
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
    dNpc_setShadowModel(mpShadowModel, data, model);
    cXyz pos(current.pos.x, current.pos.y + 150.0f, current.pos.z);
    mShadowId = dComIfGd_setRealShadow(mShadowId, 1, mpShadowModel, &pos, 800.0f, current.pos.y - mObjAcch.GetGroundH(), NULL);
    if (mShadowId == 0) {
        dComIfGd_setSimpleShadow(&current.pos, mObjAcch.GetGroundH(), 40.0f, &dComIfG_Bgsp()->GetTriPla(mObjAcch.m_gnd)->mNormal, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }
    if (l_HIO.mPrm.mDebug) {
        cXyz debug = current.pos;
        debug.y = eyePos.y;
    }
    dSnap_RegistFig(DSNAP_TYPE_UNK5B, this, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 00002E24-00003008       .text _execute__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::_execute() {
    if (!mInitialSet) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        mInitialSet = 1;
    }
    m_jnt.setParam(l_HIO.mPrm.mMaxBackX, l_HIO.mPrm.mMaxBackY, l_HIO.mPrm.mMinBackX, l_HIO.mPrm.mMinBackY, l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY, l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY, l_HIO.mPrm.mMaxTurn);
    if (mHidden && demoActorID == 0) {
        return TRUE;
    }
    mStopMove = 0;
    mHidden = 0;
    partner_srch();
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == false) {
            staff = isEventEntry();
        }
        if (staff >= 0) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        if (!mStopMove) {
            gp_nMove();
            fopAcM_posMoveF(this, mStts.GetCCMoveP());
        }
        if (!mFreezeAngle) {
            mModelAngle = current.angle;
            shape_angle = mModelAngle;
        }
    }
    eventOrder();
    setMtx(false);
    if (!mDemo) {
        setCollision(70.0f, 230.0f);
    }
    return TRUE;
}

/* 00003008-00003064       .text _delete__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::_delete() {
    dComIfG_resDeleteDemo(&mPhase, "Gp");
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    if (l_HIO.mCount >= 0 && --l_HIO.mCount < 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
    }
#else
    if (heap != NULL && mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return TRUE;
}

/* 00003064-00003084       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Gp1_c*)actor)->CreateHeap();
}

/* 00003084-000031A4       .text _create__11daNpc_Gp1_cFv */
cPhs_State daNpc_Gp1_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Gp");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    int prm = fopAcM_GetParam(this) & 0xFF;
    if (!charDecide(prm)) {
        return cPhs_ERROR_e;
    }
    if (l_HIO.mCount < 0) {
        l_HIO.mNo = mDoHIO_createChild("金持ちマギ−の父", &l_HIO);
    }
    l_HIO.mCount++;
    fopAcM_SetupActor(this, daNpc_Gp1_c);
#else
    fopAcM_SetupActor(this, daNpc_Gp1_c);
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Gp");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    if (!charDecide(fopAcM_GetParam(this) & 0xFF)) {
        return cPhs_ERROR_e;
    }
#endif
    static u32 a_size_tbl[] = {0x272E0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_size_tbl[mType])) {
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -70.0f, -20.0f, -70.0f, 50.0f, 240.0f, 50.0f);
    if (createInit()) {
        return phase;
    }
    return cPhs_ERROR_e;
}

/* 000035D4-00003844       .text create_Anm__11daNpc_Gp1_cFv */
J3DModelData* daNpc_Gp1_c::create_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Gp", dRes_ID_GP_BDL_GP_e);
    JUT_ASSERT(DEMO_SELECT(2148, 2159), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectIDRes("Gp", dRes_ID_GP_BCK_WAIT01_e), 2, 1.0f, 0, -1, TRUE, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL) {
        return NULL;
    }
    if (mpMorf->getModel() == NULL) {
        mpMorf = NULL;
        return NULL;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(2168, 2179), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(2171, 2182), m_bbone_jnt_num >= 0);
    m_hnd_L_jnt_num = a_mdl_dat->getJointName()->getIndex("handL");
    JUT_ASSERT(DEMO_SELECT(2174, 2185), m_hnd_L_jnt_num >= 0);
    return a_mdl_dat;
}

/* 00003844-000039A0       .text CreateHeap__11daNpc_Gp1_cFv */
BOOL daNpc_Gp1_c::CreateHeap() {
    J3DModelData* data = create_Anm();
    if (data == NULL) {
        return FALSE;
    }
    mTexIndex = 0;
    if (!iniTexPttrnAnm(false)) {
        mpMorf = NULL;
        return FALSE;
    }
    mpShadowModel = mDoExt_J3DModel__create(data, 0x20000, 0x11020203);
    if (mpShadowModel != NULL) {
        for (u16 i = 0; i < data->getJointNum(); i++) {
            if (i == m_hed_jnt_num || i == m_bbone_jnt_num) {
                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Gp1);
            }
        }
        mpMorf->getModel()->setUserArea((u32)this);
        mAcchCir.SetWall(30.0f, 70.0f);
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
        return TRUE;
    }
    mpMorf = NULL;
    return FALSE;
}

/* 000039A0-000039C0       .text daNpc_Gp1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Gp1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Gp1_c*)i_this)->_create();
}

/* 000039C0-000039E0       .text daNpc_Gp1_Delete__FP11daNpc_Gp1_c */
static BOOL daNpc_Gp1_Delete(daNpc_Gp1_c* i_this) {
    return ((daNpc_Gp1_c*)i_this)->_delete();
}

/* 000039E0-00003A00       .text daNpc_Gp1_Execute__FP11daNpc_Gp1_c */
static BOOL daNpc_Gp1_Execute(daNpc_Gp1_c* i_this) {
    return ((daNpc_Gp1_c*)i_this)->_execute();
}

/* 00003A00-00003A20       .text daNpc_Gp1_Draw__FP11daNpc_Gp1_c */
static BOOL daNpc_Gp1_Draw(daNpc_Gp1_c* i_this) {
    return ((daNpc_Gp1_c*)i_this)->_draw();
}

/* 00003A20-00003A28       .text daNpc_Gp1_IsDelete__FP11daNpc_Gp1_c */
static BOOL daNpc_Gp1_IsDelete(daNpc_Gp1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Gp1_Method = {
    (process_method_func)daNpc_Gp1_Create,
    (process_method_func)daNpc_Gp1_Delete,
    (process_method_func)daNpc_Gp1_Execute,
    (process_method_func)daNpc_Gp1_IsDelete,
    (process_method_func)daNpc_Gp1_Draw,
};
actor_process_profile_definition g_profile_NPC_GP1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_GP1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Gp1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_GP1_e,
    /* Actor SubMtd */ &l_daNpc_Gp1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
