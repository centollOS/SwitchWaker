/**
 * d_a_npc_ob1.cpp
 * NPC - Rose
 */
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_ob1.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_kb.h"
#include "d/d_demo.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Ob.h"

static daNpc_Ob1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;
static u8 l_check_flg;

/* 000000EC-00000150       .text __ct__15daNpc_Ob1_HIO_cFv */
daNpc_Ob1_HIO_c::daNpc_Ob1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x2000, 5000, -1000, -5000, 1500, 8000, -2000, -8000,
        1200, 1200, 170.0f, 0, 6, 0x400, 0.5f, 3.0f, 0.1f, 40.0f,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(mPrm));
    mNo = -1;
    mCount = -1;
}

daNpc_Ob1_HIO_c::~daNpc_Ob1_HIO_c() {}

static char* l_evn_tbl[] = {"Get_Rupee"};

/* 00000198-00000228       .text searchActor_Kb__FPvPv */
static void* searchActor_Kb(void* actor, void*) {
    if (l_check_wrk < 20 && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_KB_e) {
        if (((kb_class*)actor)->m405 != 0) {
            l_check_flg |= ((kb_class*)actor)->m405;
        }
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

/* 00000228-00000274       .text nodeCallBack_Ob1__FP7J3DNodei */
static BOOL nodeCallBack_Ob1(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ob1_c* npc = (daNpc_Ob1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->nodeOb1Control(node, model);
        }
    }
    return TRUE;
}

/* 00000274-000003C4       .text nodeOb1Control__11daNpc_Ob1_cFP7J3DNodeP8J3DModel */
void daNpc_Ob1_c::nodeOb1Control(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_offst(20.0f, -20.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_jnt_num) {
        mDoMtx_stack_c::YrotM(-m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
        mDoMtx_stack_c::multVec(&a_eye_pos_offst, &mEyeLocal);
    }
    if (joint == m_bbone_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM(m_jnt.getBackbone_x());
    }
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000400-00000480       .text init_OB1_0__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::init_OB1_0() {
    if (!dComIfGs_isEventBit(0x0520)) {
        set_action(&daNpc_Ob1_c::wait_action1, NULL);
        return true;
    }
    return false;
}

/* 00000480-00000518       .text init_OB1_1__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::init_OB1_1() {
    if (dComIfGs_isEventBit(0x0520) && dKy_daynight_check() == 0) {
        fopAcM_OffStatus(this, 0x80);
        set_action(&daNpc_Ob1_c::wait_action2, NULL);
        return true;
    }
    return false;
}

/* 00000518-000005A4       .text init_OB1_2__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::init_OB1_2() {
    if (dComIfGs_isEventBit(0x0520) && dKy_daynight_check() == 1) {
        set_action(&daNpc_Ob1_c::wait_action2, NULL);
        return true;
    }
    return false;
}

/* 000005A4-000007AC       .text createInit__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::createInit() {
    for (int i = 0; i < 1; i++) {
        mEventIds[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    }
    attention_info.flags = 0xA;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAC;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAC;
    gravity = -4.5f;
    mInitialAngle = current.angle;
    mInitialPos = current.pos;
    mLookPos = mInitialPos;
    int weight = 0xFF;
    u8 pathNo = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (pathNo != 0xFF) {
        mPath.setInf(pathNo, fopAcM_GetRoomNo(this), 1);
        if (mPath.isPath() == false) {
            return false;
        }
        fopAcM_OffStatus(this, 0x80);
        weight = 0xD9;
    }
    mCut.setActorInfo2("Ob1", this);
    mAnmIndex = 8;
    bool result;
    switch (mSubtype) {
    case 0:
        result = init_OB1_0();
        break;
    case 1:
        result = init_OB1_1();
        break;
    case 2:
        result = init_OB1_2();
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

/* 000007AC-00000900       .text setMtx__11daNpc_Ob1_cFb */
void daNpc_Ob1_c::setMtx(bool force) {
    if (!mDemo) {
        plyTexPttrnAnm();
        mAnmEnded = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mLastFrame) {
            mAnmEnded = 1;
        }
        mLastFrame = mpMorf->getFrame();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(mModelAngle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    MtxP headMtx = mpMorf->getModel()->getAnmMtx(m_hed_jnt_num);
    MTXCopy(headMtx, mpHeadModel->getBaseTRMtx());
    mpHeadModel->calc();
    setAttention(force);
}

/* 00000900-00000914       .text anmNum_toResID__11daNpc_Ob1_cFi */
int daNpc_Ob1_c::anmNum_toResID(int index) {
    static const int a_anm_num_tbl[] = {dRes_ID_OB_BCK_WAIT_e, dRes_ID_OB_BCK_TALK_e, dRes_ID_OB_BCK_THANK_e, dRes_ID_OB_BCK_WALK_e, dRes_ID_OB_BCK_THANK02_e, dRes_ID_OB_BCK_LAUGH_e, dRes_ID_OB_BCK_SHUTUP01_e, dRes_ID_OB_BCK_SHUTUP02_e};
    return a_anm_num_tbl[index];
}

/* 00000914-00000928       .text btpNum_toResID__11daNpc_Ob1_cFi */
int daNpc_Ob1_c::btpNum_toResID(int index) {
    static const int a_btp_num_tbl[] = {dRes_ID_OB_BTP_MABA_A_e, dRes_ID_OB_BTP_MABA_A_e};
    return a_btp_num_tbl[index];
}

/* 00000928-00000A34       .text setBtp__11daNpc_Ob1_cFbi */
bool daNpc_Ob1_c::setBtp(bool modify, int index) {
    J3DModelData* data = mpHeadModel->getModelData();
    m_hed_tex_pttrn = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Ob", btpNum_toResID(index));
    JUT_ASSERT(511, m_hed_tex_pttrn != 0);
    if (mBtp.init(data, m_hed_tex_pttrn, TRUE, 2, 1.0f, 0, -1, modify, FALSE) != 0) {
        mBtpFrame = 0;
        mBlinkTimer = 0;
        return true;
    }
    return false;
}

/* 00000A34-00000A5C       .text iniTexPttrnAnm__11daNpc_Ob1_cFb */
bool daNpc_Ob1_c::iniTexPttrnAnm(bool modify) {
    return setBtp(modify, mTexIndex);
}

/* 00000A5C-00000B08       .text plyTexPttrnAnm__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::plyTexPttrnAnm() {
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

/* 00000B08-00000B44       .text setAnm_tex__11daNpc_Ob1_cFSc */
void daNpc_Ob1_c::setAnm_tex(s8 index) {
    if (mTexIndex != index) {
        mTexIndex = index;
        iniTexPttrnAnm(true);
    }
}

/* 00000B44-00000C04       .text setAnm_anm__11daNpc_Ob1_cFPQ211daNpc_Ob1_c9anm_prm_c */
BOOL daNpc_Ob1_c::setAnm_anm(anm_prm_c* prm) {
    if (mAnmIndex == prm->mAnm) {
        return TRUE;
    }
    mAnmIndex = prm->mAnm;
    int res = anmNum_toResID(mAnmIndex);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, res, -1, "Ob");
    mLastFrame = mAnmRepeat = mAnmEnded = 0;
    return TRUE;
}

/* 00000C04-00000C70       .text setAnm_NUM__11daNpc_Ob1_cFii */
BOOL daNpc_Ob1_c::setAnm_NUM(int index, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0,0,8.0f,1.0f,2,0},
        {1,0,8.0f,1.0f,2,0},
        {2,0,8.0f,1.0f,0,0},
        {3,0,8.0f,1.0f,2,0},
        {4,0,8.0f,1.0f,2,0},
        {5,0,8.0f,1.0f,2,0},
        {6,0,8.0f,1.0f,0,0},
        {7,0,8.0f,1.0f,2,0}
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[index].mTex);
    }
    return setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 00000C70-00000CF0       .text setAnm__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1,-1,0.0f,0.0f,-1,0},
        {0,0,8.0f,1.0f,2,0},
        {-1,-1,0.0f,0.0f,-1,0},
        {3,0,8.0f,1.0f,2,0},
        {0,0,8.0f,1.0f,2,0},
        {1,0,8.0f,1.0f,2,0},
        {-1,-1,0.0f,0.0f,-1,0}
    };
    if (a_anm_prm_tbl[mState].mTex >= 0) {
        setAnm_tex(a_anm_prm_tbl[mState].mTex);
    }
    if (a_anm_prm_tbl[mState].mAnm >= 0) {
        setAnm_anm(&a_anm_prm_tbl[mState]);
    }
    return TRUE;
}

/* 00000CF0-00000CFC       .text chg_anmTag__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::chg_anmTag() {
    switch (mAnmTag) {
    case 0xFF:
    default:
        break;
    }
}

/* 00000CFC-00000D08       .text control_anmTag__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::control_anmTag() {
    switch (mAnmTag) {
    case 0xFF:
    default:
        break;
    }
}

/* 00000D08-00000E00       .text chg_anmAtr__11daNpc_Ob1_cFUc */
void daNpc_Ob1_c::chg_anmAtr(u8 attr) {
    if (mPairedMsgNo != 0xFFFFFFFF) {
        switch (mPairedMsgNo) {
        case 0xAFC:
        case 0xAFD:
        case 0xAFF:
        case 0xB02:
        case 0xB05:
        case 0xB06:
        case 0xB09:
        case 0xB0C:
        case 0xB0D:
        case 0xB5D:
        case 0xB61:
        case 0xB64:
        case 0xB65:
            mLookMode = 1;
            mNoTurn = false;
            m_jnt.setTrn();
            break;
        }
    }
    if (attr >= 7 || attr == mAnmAttr) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR(1);
}

/* 00000E00-00000E8C       .text control_anmAtr__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::control_anmAtr() {
    switch (mAnmAttr) {
    case 0:
    case 1:
    case 2:
        break;
    case 3:
        if (mAnmEnded) {
            setAnm_NUM(4, 1);
            mAnmAttr = 7;
        } break;
    case 6:
        if (mAnmEnded) {
            setAnm_NUM(7, 1);
            mAnmAttr = 7;
        } break;
    }
}

/* 00000E8C-00000EF4       .text setAnm_ATR__11daNpc_Ob1_cFi */
BOOL daNpc_Ob1_c::setAnm_ATR(int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0,0,8.0f,1.0f,2,0},
        {1,0,8.0f,1.0f,2,0},
        {2,1,8.0f,1.0f,0,0},
        {2,0,8.0f,1.0f,0,0},
        {4,0,8.0f,1.0f,2,0},
        {5,0,8.0f,1.0f,2,0},
        {6,0,8.0f,1.0f,0,0}
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[mAnmAttr].mTex);
    }
    return setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00000EF4-00000FFC       .text anmAtr__11daNpc_Ob1_cFUs */
void daNpc_Ob1_c::anmAtr(u16 status) {
    dComIfG_MesgCamInfo_c* info = dComIfGp_getMesgCameraInfo();
    if (field_0x6bc[0] == 2 && this != info->mActor[info->mBasicID - 1]) {
        control_anmTag();
        control_anmAtr();
        return;
    }
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

/* 00000FFC-00001088       .text eventOrder__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::eventOrder() {
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

/* 00001088-00001120       .text checkOrder__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEventIds[mEventIndex])) {
            mEventOrder = 0;
        }
    } else if (eventInfo.checkCommandTalk() && (mEventOrder == 1 || mEventOrder == 2)) {
        mEventOrder = 0;
        mTalking = true;
    }
}

/* 00001120-000011C0       .text chk_talk__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::chk_talk() {
    bool result = false;
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mPresentItem = dComIfGp_event_getPreItemNo();
            result = true;
        }
    } else {
        mPresentItem = 0xFF;
        result = true;
    }
    return result;
}

/* 000011C0-00001200       .text chk_partsNotMove__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::chk_partsNotMove() {
#if VERSION == VERSION_DEMO
    return mOldHeadY == m_jnt.getHead_y() && mOldBackY == m_jnt.getBackbone_y() && mOldActorY == current.angle.y;
#else
    bool result = false;
    if (mOldHeadY == m_jnt.getHead_y() && mOldBackY == m_jnt.getBackbone_y() && mOldActorY == current.angle.y) {
        result = true;
    }
    return result;
#endif
}

/* 00001200-000013A0       .text lookBack__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::lookBack() {
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

/* 000013A0-000014CC       .text next_msgStatus__11daNpc_Ob1_cFPUl */
u16 daNpc_Ob1_c::next_msgStatus(u32* msg) {
    u16 result = 15;
    switch (*msg) {
    case 0xA8D:
        *msg = 0xA8E;
        break;
    case 0xA90:
        *msg = 0xA91;
        break;
    case 0xA93:
        *msg = 0xA94;
        break;
    case 0xA97:
        *msg = 0xA98;
        break;
    case 0xA92:
    case 0xA96:
    case 0xA9B:
    case 0xA9D:
    case 0xAA0:
    case 0xAA2:
    case 0xAA3:
    case 0xAA4:
    case 0xAA5:
    case 0xAA6:
        if (dComIfGs_isEventBit(0xE20) && !dComIfGs_isEventBit(0x308)) {
            *msg = 0xAA7;
            break;
        }
        goto end_message;
    case 0xAA8:
        if (dKy_daynight_check() == 0) {
            *msg = 0xAA9;
        } else {
            *msg = 0xAAA;
        }
        break;
    case 0xAAD:
    case 0xAAE:
        if (dComIfGs_isEventBit(0x304)) {
            *msg = 0xA8F;
        } else {
            *msg = 0xA8D;
        }
        break;
    default:
    end_message:
        result = 16;
        break;
    }
    return result;
}

/* 000014CC-000017F0       .text getMsg_OB1_0__11daNpc_Ob1_cFv */
u32 daNpc_Ob1_c::getMsg_OB1_0() {
    u32 msg = 0;
    if (mPresentPigCount != 0 || mSavedPigCount != 0) {
        if (dComIfGs_isEventBit(0x302)) {
            if (mNewPigCount != 0) {
                if (mSavedPigCount == 2) {
                    if (mMessageCycle != 0) {
                        msg = 0xAA2;
                        mMessageCycle = 0;
                    } else {
                        msg = 0xAA1;
                        mMessageCycle = 1;
                    }
                } else {
                    switch (mNewPigCount) {
                    case 1:
                        if (mMessageCycle == 1) {
                            msg = 0xA9D;
                            mMessageCycle = 0;
                        } else {
                            msg = 0xA9C;
                            mMessageCycle = 1;
                        } break;
                    case 2:
                        if (mMessageCycle == 2) {
                            msg = 0xA9F;
                            mMessageCycle = 1;
                        } else if (mMessageCycle == 1) {
                            msg = 0xAA0;
                            mMessageCycle = 0;
                        } else {
                            msg = 0xA9E;
                            mMessageCycle = 2;
                        } break;
                    }
                }
            } else if (mSavedPigCount > mPresentPigCount) {
                msg = 0xAA3;
            } else {
                switch (mPresentPigCount) {
                case 1:
                    msg = 0xAA4;
                    break;
                case 2:
                    msg = 0xAA5;
                    break;
                default:
                    msg = 0xAA6;
                    break;
                }
            }
        } else {
            switch (mNewPigCount) {
            case 1:
                if (mMessageCycle == 1) {
                    msg = 0xA92;
                    mMessageCycle = 0;
                } else {
                    msg = 0xA90;
                    mMessageCycle = 1;
                } break;
            case 2:
                if (mMessageCycle == 2) {
                    msg = 0xA95;
                    mMessageCycle = 1;
                } else if (mMessageCycle == 1) {
                    msg = 0xA96;
                    mMessageCycle = 0;
                } else {
                    msg = 0xA93;
                    mMessageCycle = 2;
                } break;
            case 3:
                if (mMessageCycle == 3) {
                    msg = 0xA99;
                    mMessageCycle = 2;
                } else if (mMessageCycle == 2) {
                    msg = 0xA9A;
                    mMessageCycle = 1;
                } else if (mMessageCycle == 1) {
                    msg = 0xA9B;
                    mMessageCycle = 0;
                } else {
                    msg = 0xA97;
                    mMessageCycle = 3;
                } break;
            }
            mSetPigEvent = 1;
        }
        return msg;
    }
    if (dComIfGs_isEventBit(0x2A80)) {
        if (!dComIfGs_isEventBit(0x2C40)) {
            dComIfGs_onEventBit(0x2C40);
            return 0xAAD;
        }
    } else if (!dComIfGs_isEventBit(0x2C80)) {
        dComIfGs_onEventBit(0x2C80);
        return 0xAAE;
    }
    return dComIfGs_isEventBit(0x304) ? 0xA8F : 0xA8D;
}

/* 000017F0-0000185C       .text getMsg_OB1_1__11daNpc_Ob1_cFv */
u32 daNpc_Ob1_c::getMsg_OB1_1() {
    if (!dComIfGs_isEventBit(0x2C20)) {
        return 0xAA8;
    }
    return dComIfGs_getEventReg(0xB6FF) != 0 ? 0xAAB : 0xAAC;
}

/* 0000185C-0000187C       .text getMsg_OB1_2__11daNpc_Ob1_cFv */
u32 daNpc_Ob1_c::getMsg_OB1_2() {
    return getMsg_OB1_1();
}

/* 0000187C-000018EC       .text getMsg__11daNpc_Ob1_cFv */
u32 daNpc_Ob1_c::getMsg() {
    u32 result = 0;
    switch (mSubtype) {
    case 0:
        result = getMsg_OB1_0();
        break;
    case 1:
        result = getMsg_OB1_1();
        break;
    case 2:
        result = getMsg_OB1_2();
        break;
    }
    return result;
}

/* 000018EC-0000196C       .text chkAttention__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 0000196C-000019C4       .text setAttention__11daNpc_Ob1_cFb */
void daNpc_Ob1_c::setAttention(bool force) {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionYOffset, current.pos.z);
    if (mUpdateEye || force) {
        eyePos.set(mEyeLocal.x, mEyeLocal.y, mEyeLocal.z);
    }
}

/* 000019C4-000019F8       .text searchByID__11daNpc_Ob1_cFUi */
fopAc_ac_c* daNpc_Ob1_c::searchByID(fpc_ProcID id) {
    fopAc_ac_c* actor = NULL;
    fopAcM_SearchByID(id, &actor);
    return actor;
}

/* 000019F8-00001A04       .text partner_srch__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::partner_srch() {
#if VERSION == VERSION_DEMO
    if (mActionState == 1) {
        bool found = false;
        switch (mSubtype) {
        case 0:
            break;
        }
        if (found) {
            mActionState++;
        }
    }
#else
    switch (mActionState) {
    case 1:
        break;
    }
#endif
}

/* 00001A04-00001A34       .text bitCount__11daNpc_Ob1_cFUc */
s8 daNpc_Ob1_c::bitCount(u8 bits) {
    s8 count = 0;
    for (int i = 0; i < 8; i++) {
        if (bits & 1) {
            count++;
        }
        bits = bits >> 1;
    }
    return count;
}

/* 00001A34-00001B14       .text set_pigCnt__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::set_pigCnt() {
    mSavedPigCount = bitCount(dComIfGs_getEventReg(0xB6FF));
    l_check_wrk = 0;
    l_check_flg = 0;
    for (int i = 0; i < 20; i++) {
        l_check_inf[i] = NULL;
    }
    fpcEx_Search(searchActor_Kb, this);
    mPresentPigCount = bitCount(l_check_flg);
    mNewPigCount = bitCount(l_check_flg & ~dComIfGs_getEventReg(0xB6FF));
    mMessageCycle = 0;
}

/* 00001B14-00001BB0       .text ob_setPthPos__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::ob_setPthPos() {
    if (mPath.isPath()) {
        current.pos = mPath.getPoint(mPath.getIdx());
        mPath.nextIdxAuto();
        cXyz next = mPath.getPoint(mPath.getIdx());
        current.angle.y = cLib_targetAngleY(&current.pos, &next);
    }
}

/* 00001BB0-00001C50       .text get_attPos__11daNpc_Ob1_cFv */
cXyz daNpc_Ob1_c::get_attPos() {
    dPath* path = mPath.nextPath(fopAcM_GetRoomNo(this));
    cXyz pos;
    if (path != NULL) {
        u8 index = mPath.getIdx();
        if (index == 0) {
            index = mPath.maxPoint();
        }
        index--;
        pos.set(path->m_points[index].m_position.x, path->m_points[index].m_position.y, path->m_points[index].m_position.z);
    }
    return pos;
}

/* 00001C50-00001DC0       .text ob_movPass__11daNpc_Ob1_cFv */
int daNpc_Ob1_c::ob_movPass() {
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

/* 00001DC0-00001E50       .text ob_clcMovSpd__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::ob_clcMovSpd() {
    f32 dist2 = (mMoveTarget - current.pos).abs2XZ();
    s16 angle = cLib_targetAngleY(&current.pos, &mMoveTarget);
    cLib_chaseAngleS(&current.angle.y, angle, l_HIO.mPrm.mAngleMax);
    cLib_chaseF(&speedF, mTargetSpeed, mSpeedStep);
}

/* 00001E50-00001F28       .text ob_nMove__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::ob_nMove() {
    switch (mMoveMode) {
    case 1:
        ob_clcMovSpd();
        f32 playSpeed;
        if (mMoveMode == 1) {
            playSpeed = speedF * l_HIO.mPrm.mAnimSpeedScale;
        }
        if (playSpeed < 0.5f) {
            playSpeed = 0.5f;
        }
        mpMorf->setPlaySpeed(playSpeed);
        switch (ob_movPass()) {
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
    if (mArrived) {
        mStopMove = 1;
    }
}

/* 00001F28-00001F78       .text charDecide__11daNpc_Ob1_cFi */
bool daNpc_Ob1_c::charDecide(int type) {
    mType = 0;
    mSubtype = -1;
    switch (type) {
    case 1:
        mSubtype = 1;
        break;
    case 2:
        mSubtype = 2;
        break;
    default:
        mSubtype = 0;
        break;
    }
    return true;
}

/* 00001F78-00001FD0       .text event_actionInit__11daNpc_Ob1_cFi */
void daNpc_Ob1_c::event_actionInit(int staff) {
    int* action = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    if (action != NULL) {
        mEventAction = *action;
    }
}

/* 00001FD0-00001FE0       .text event_action__11daNpc_Ob1_cFv */
bool daNpc_Ob1_c::event_action() {
    switch (mEventAction) {
    case 0:
    default:
        return true;
    }
}

/* 00001FE0-000020CC       .text privateCut__11daNpc_Ob1_cFi */
void daNpc_Ob1_c::privateCut(int staff) {
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

/* 000020CC-000020EC       .text endEvent__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

/* 000020EC-00002124       .text isEventEntry__11daNpc_Ob1_cFv */
int daNpc_Ob1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mCut.getActorName());
}

/* 00002124-000021B8       .text event_proc__11daNpc_Ob1_cFi */
void daNpc_Ob1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIds[mEventIndex])) {
        mEventOrder = 1;
        endEvent();
    } else {
        if (!mCut.cutProc()) {
            privateCut(staff);
        }
        lookBack();
    }
}

/* 000021B8-00002264       .text set_action__11daNpc_Ob1_cFM11daNpc_Ob1_cFPCvPvPv_iPv */
bool daNpc_Ob1_c::set_action(int (daNpc_Ob1_c::*action)(void*), void* arg) {
    if (mAction != action) {
        if (mAction != NULL) {
            mActionState = 0;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

/* 00002264-00002288       .text clrSpd__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::clrSpd() {
    mTargetSpeed = 0.0f;
    mSpeedStep = 0.0f;
    speedF = 0.0f;
    gravity = -4.5f;
}

/* 00002288-00002488       .text setStt__11daNpc_Ob1_cFSc */
void daNpc_Ob1_c::setStt(s8 state) {
    searchByID(mPartnerID);
    s8 previous = mState;
    mAttentionTimer = 0;
    mState = state;
    switch (mState) {
    case 0:
        break;
    case 4:
        mWaitTimer = cLib_getRndValue<int>(90, 180);
    case 1:
    case 5:
        if (previous != 2) {
            switch (mState) {
            case 1:
                mLookMode = 3;
                mLookAngle = mInitialAngle.y;
                break;
            case 4:
                mLookMode = 2;
                mLookPos = get_attPos();
                break;
            case 5:
                mLookMode = 1;
                break;
            }
            m_jnt.setTrn();
            mNoTurn = false;
        }
        mEventOrder = 0;
        mMoveMode = 0;
        clrSpd();
        break;
    case 2:
        mLookMode = 1;
        m_jnt.setTrn();
        mNoTurn = false;
        mEventOrder = 0;
        mMoveMode = 0;
        clrSpd();
        mAnmAttr = 0xFF;
        if (previous != 5) {
            mPreviousState = previous;
        }
        set_pigCnt();
        break;
    case 3:
        mMoveTarget = mPath.getPoint(mPath.getIdx());
        mLookMode = 0;
        mNoTurn = true;
        mEventOrder = 0;
        mMoveMode = 1;
        mArrived = 0;
        gravity = -4.5f;
        mTargetSpeed = l_HIO.mPrm.mTargetSpeed;
        mSpeedStep = l_HIO.mPrm.mSpeedStep;
        mArriveDistance = l_HIO.mPrm.mArriveDistance;
        break;
    case 6:
        mAnmAttr = 0xFF;
        mPreviousState = previous;
        return;
    }
    setAnm();
}

/* 00002488-00002564       .text wait_1__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::wait_1() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
        }
        return TRUE;
    }
    if (field_0x6bc[0] == 1) {
        field_0x6bc[0] = 2;
        setStt(6);
        mLookMode = 3;
        mLookAngle = mInitialAngle.y;
        return TRUE;
    }
    mEventOrder = 2;
    if (mHasAttention) {
        mAttentionTimer = 60;
    }
    if (cLib_calcTimer(&mAttentionTimer) != 0) {
        mLookMode = 1;
    } else {
        mLookMode = 3;
        mLookAngle = mInitialAngle.y;
        m_jnt.setTrn();
    }
    return TRUE;
}

/* 00002564-0000264C       .text wait_2__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::wait_2() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
        }
        return TRUE;
    }
    mEventOrder = 2;
    if (mHasAttention) {
        mAttentionTimer = 60;
    }
    if (cLib_calcTimer(&mAttentionTimer) != 0) {
        mLookMode = 1;
    } else {
        if (cLib_calcTimer(&mWaitTimer) == 0) {
            setStt(3);
            return TRUE;
        }
        mLookMode = 2;
        mLookPos = get_attPos();
        m_jnt.setTrn();
    }
    return TRUE;
}

/* 0000264C-000026F0       .text wait_3__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::wait_3() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(2);
        }
        return TRUE;
    }
    if (mAnmEnded) {
        mAnmRepeat++;
        if (mAnmRepeat > 3) {
            setStt(mPreviousState);
            return TRUE;
        }
    }
    mEventOrder = 2;
    mLookMode = 1;
    m_jnt.setTrn();
    return TRUE;
}

/* 000026F0-00002808       .text walk_1__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::walk_1() {
    if (mTalking) {
        setAnm_NUM(0, 1);
        speedF = 0.0f;
        if (chk_talk()) {
            setStt(2);
        }
        return TRUE;
    }
    mEventOrder = 2;
    if (mHasAttention) {
        mAttentionTimer = 20;
    }
    if (cLib_calcTimer(&mAttentionTimer) != 0) {
        mLookMode = 1;
    } else {
        mLookMode = 0;
    }
    if (mArrived) {
        if (mPath.getIdx() == 0) {
            mPath.nextIdxAuto();
        }
        setStt(4);
        return TRUE;
    }
    mArrived = 0;
    mMoveTarget = mPath.getPoint(mPath.getIdx());
    return TRUE;
}

/* 00002808-00002A58       .text talk_1__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::talk_1() {
    BOOL result = chk_partsNotMove();
    if (mEventOrder == 1 || mEventOrder >= 3) {
        return TRUE;
    }
    if (mpCurrMsg != NULL) {
        mPreviousMsgStatus = mpCurrMsg->mStatus;
    } else {
        mPreviousMsgStatus = 0;
    }
    if (mUsePartnerMessage && mCurrMsgBsPcId == fpcM_ERROR_PROCESS_ID_e) {
        mCurrMsgNo = getMsg();
        mCurrMsgBsPcId = fopMsgM_messageSet(mCurrMsgNo, dComIfGp_getMesgCameraInfo()->mActor[0]);
        mpCurrMsg = NULL;
    } else {
        mPairedMsgNo = 0xFFFFFFFF;
        talk(1);
    }
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case 0x13:
            if (mMessageCycle != 0) {
                mEventOrder = 3;
                setAnm_NUM(0, 1);
                endEvent();
                return result;
            }
            l_check_flg |= dComIfGs_getEventReg(0xB6FF);
            dComIfGs_setEventReg(0xB6FF, l_check_flg);
            switch (mCurrMsgNo) {
            case 0xA8E:
                dComIfGs_onEventBit(0x304);
                break;
            case 0xAA7:
                dComIfGs_onEventBit(0x308);
                break;
            case 0xAA9:
            case 0xAAA:
            case 0xAAB:
            case 0xAAC:
                dComIfGs_onEventBit(0x2C20);
                break;
            }
            if (mSetPigEvent) {
                dComIfGs_onEventBit(0x302);
            }
            mPresentItem = 0xFF;
            mTalking = false;
            switch (mSubtype) {
            case 0:
                setStt(5);
                break;
            default:
                setStt(mPreviousState);
                mAttentionTimer = 60;
                break;
            }
            endEvent();
            break;
        case 2:
        case 6:
            break;
        }
    }
    return result;
}

/* 00002A58-00002B60       .text manzai__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::manzai() {
    dComIfG_MesgCamInfo_c* info = dComIfGp_getMesgCameraInfo();
    switch (field_0x6bc[0]) {
    case 2:
        {
            fopNpc_npc_c* partner = (fopNpc_npc_c*)searchByID(mManzaiPartnerId);
            if (this != info->mActor[info->mBasicID - 1]) {
                if (mAnmAttr != 0xFF) {
                    mLookMode = 3;
                    mLookAngle = mInitialAngle.y;
                    m_jnt.setTrn();
                    mState = mPreviousState;
                    setAnm();
                    mState = 6;
                    mAnmAttr = 0xFF;
                }
            } else {
                mPairedMsgNo = partner->mCurrMsgNo;
                anmAtr(partner->mManzaiMsgStatus);
            }
            break;
        }
    case 3:
        fopAcM_OffStatus(this, 0x4000);
        setStt(mPreviousState);
        field_0x6bc[0] = 0;
        break;
    }
    return TRUE;
}

/* 00002B60-00002C54       .text wait_action1__11daNpc_Ob1_cFPv */
int daNpc_Ob1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        ob_setPthPos();
        setStt(3);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        mHasAttention = chkAttention();
        switch (mState) {
        case 4:
            mUpdateEye = wait_2();
            break;
        case 5:
            mUpdateEye = wait_3();
            break;
        case 3:
            mUpdateEye = walk_1();
            break;
        case 2:
            mUpdateEye = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00002C54-00002D2C       .text wait_action2__11daNpc_Ob1_cFPv */
int daNpc_Ob1_c::wait_action2(void*) {
    switch (mActionState) {
    case 0:
        setStt(1);
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
        case 6:
            mUpdateEye = manzai();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00002D2C-00002E68       .text demo__11daNpc_Ob1_cFv */
u8 daNpc_Ob1_c::demo() {
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
        J3DAnmTexPattern* pattern = actor->getP_BtpData("Ob");
        if (pattern != NULL) {
            m_hed_tex_pttrn = pattern;
            if (mBtp.init(mpHeadModel->getModelData(), m_hed_tex_pttrn, TRUE, 2, 1.0f, 0, -1, true, FALSE)) {
                mTexIndex = 2;
                mBtpFrame = 0;
            }
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Ob");
    }
    return mDemo;
}

/* 00002E68-00002F18       .text shadowDraw__11daNpc_Ob1_cFv */
void daNpc_Ob1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId != 0) {
        dComIfGd_addRealShadow(mShadowId, mpHeadModel);
    }
}

/* 00002F18-0000300C       .text _draw__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::_draw() {
    J3DModel* head = mpHeadModel;
    J3DModelData* data = head->getModelData();
    J3DModel* model = mpMorf->getModel();
    if (mHidden || mNoDraw) {
        return TRUE;
    }
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mpMorf->entryDL();
    mBtp.entry(data, mBtpFrame);
    mDoExt_modelEntryDL(head);
    mBtp.remove(data);
    g_env_light.setLightTevColorType(head, &tevStr);
    shadowDraw();
    dSnap_RegistFig(DSNAP_TYPE_UNK4F, this, 1.0f, 1.0f, 1.0f);
    GXColor red = {255, 0, 0, 128};
    GXColor blue = {0, 0, 255, 128};
    GXColor green = {0, 255, 0, 128}; // Unused colors, needed for the .rodata section to match.
    return TRUE;
}

/* 0000300C-000031F0       .text _execute__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::_execute() {
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
            ob_nMove();
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
        setCollision(80.0f, 160.0f);
    }
    return TRUE;
}

/* 000031F0-0000324C       .text _delete__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::_delete() {
    dComIfG_resDelete(&mPhase, "Ob");
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    l_HIO.removeHIO();
#else
    if (heap != NULL && mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return TRUE;
}

/* 0000324C-0000326C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Ob1_c*)actor)->CreateHeap();
}

/* 0000326C-0000338C       .text _create__11daNpc_Ob1_cFv */
cPhs_State daNpc_Ob1_c::_create() {
    fopAcM_ct_Retail(this, daNpc_Ob1_c);
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Ob");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    u32 prm = fopAcM_GetParam(this) & 0xFF;
    if (!charDecide(prm)) {
        return cPhs_ERROR_e;
    }
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("おばさん");
    fopAcM_ct(this, daNpc_Ob1_c);
#endif
    static u32 a_size_tbl[] = {0x272E0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_size_tbl[mType])) {
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -60.0f, -20.0f, -60.0f, 60.0f, 170.0f, 60.0f);
    if (createInit()) {
        return phase;
    }
    return cPhs_ERROR_e;
}

/* 000037BC-000039D0       .text create_Anm__11daNpc_Ob1_cFv */
J3DModelData* daNpc_Ob1_c::create_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ob", dRes_ID_OB_BDL_OB_e);
    JUT_ASSERT(DEMO_SELECT(2420, 2423), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectIDRes("Ob", dRes_ID_OB_BCK_WAIT_e), 2, 1.0f, 0, -1, TRUE, NULL, 0x80000, 0x11020022);
    if (mpMorf == NULL) {
        return NULL;
    }
    if (mpMorf->getModel() == NULL) {
        mpMorf = NULL;
        return NULL;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(2440, 2443), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(2443, 2446), m_bbone_jnt_num >= 0);
    return a_mdl_dat;
}

/* 000039D0-00003A9C       .text create_hed_Mdl__11daNpc_Ob1_cFv */
J3DModelData* daNpc_Ob1_c::create_hed_Mdl() {
    static int a_hed_mdl_resID_tbl[] = {dRes_ID_OB_BDL_OBA_HEAD_e};
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ob", a_hed_mdl_resID_tbl[mType]);
    JUT_ASSERT(DEMO_SELECT(2462, 2465), a_mdl_dat != 0);
    mpHeadModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11020022);
    return a_mdl_dat;
}

/* 00003A9C-00003BE8       .text CreateHeap__11daNpc_Ob1_cFv */
BOOL daNpc_Ob1_c::CreateHeap() {
    J3DModelData* data = create_Anm();
    if (data == NULL) {
        return FALSE;
    }
    if (create_hed_Mdl() == NULL) {
        mpMorf = NULL;
        return FALSE;
    }
    s8 a_tex_pattern_num_tbl[] = {0};
    mTexIndex = a_tex_pattern_num_tbl[0];
    if (!iniTexPttrnAnm(false)) {
        mpMorf = NULL;
        return FALSE;
    }
    for (u16 i = 0; i < data->getJointNum(); i++) {
        if (i == m_hed_jnt_num || i == m_bbone_jnt_num) {
            mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Ob1);
        }
    }
    mpMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 80.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return TRUE;
}

/* 00003BE8-00003C08       .text daNpc_Ob1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Ob1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Ob1_c*)i_this)->_create();
}

/* 00003C08-00003C28       .text daNpc_Ob1_Delete__FP11daNpc_Ob1_c */
static BOOL daNpc_Ob1_Delete(daNpc_Ob1_c* i_this) {
    return ((daNpc_Ob1_c*)i_this)->_delete();
}

/* 00003C28-00003C48       .text daNpc_Ob1_Execute__FP11daNpc_Ob1_c */
static BOOL daNpc_Ob1_Execute(daNpc_Ob1_c* i_this) {
    return ((daNpc_Ob1_c*)i_this)->_execute();
}

/* 00003C48-00003C68       .text daNpc_Ob1_Draw__FP11daNpc_Ob1_c */
static BOOL daNpc_Ob1_Draw(daNpc_Ob1_c* i_this) {
    return ((daNpc_Ob1_c*)i_this)->_draw();
}

/* 00003C68-00003C70       .text daNpc_Ob1_IsDelete__FP11daNpc_Ob1_c */
static BOOL daNpc_Ob1_IsDelete(daNpc_Ob1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Ob1_Method = {
    (process_method_func)daNpc_Ob1_Create,
    (process_method_func)daNpc_Ob1_Delete,
    (process_method_func)daNpc_Ob1_Execute,
    (process_method_func)daNpc_Ob1_IsDelete,
    (process_method_func)daNpc_Ob1_Draw,
};
actor_process_profile_definition g_profile_NPC_OB1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_OB1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Ob1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_OB1_e,
    /* Actor SubMtd */ &l_daNpc_Ob1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
