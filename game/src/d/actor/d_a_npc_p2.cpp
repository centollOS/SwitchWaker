/**
 * d_a_npc_p2.cpp
 * NPC - Zuko, Niko, & Mako
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_p2.h"
#include "d/actor/d_a_himo3.h"
#include "d/actor/d_a_obj_timer.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_lib.h"
#include "d/d_particle_name.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "JAZelAudio/JAZelAudio_SE.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/P2.h"
#include <string.h>

const u32 daNpc_P2_c::m_heapsize[3] = { 0x3EE0, 0x37E0, 0x4280 };
const char daNpc_P2_c::m_arc_name[3] = "P2";

class daNpc_P2_childHIO_c {
public:
    daNpc_P2_childHIO_c();
    virtual ~daNpc_P2_childHIO_c() {}

public:
    /* 0x04 */ dNpc_HIO_c mNpc;
    /* 0x2C */ u8 m2C;
    /* 0x30 */ cXyz m30;
    /* 0x3C */ cXyz m3C;
    /* 0x48 */ cXyz m48;
    /* 0x54 */ f32 m54;
    /* 0x58 */ f32 m58;
    /* 0x5C */ f32 m5C;
    /* 0x60 */ f32 m60;
    /* 0x64 */ f32 m64;
    /* 0x68 */ f32 m68;
    /* 0x6C */ u8 m6C;
    /* 0x70 */ f32 m70;
    /* 0x74 */ f32 m74[24];
};  // Size: 0xD4

STATIC_ASSERT(sizeof(daNpc_P2_childHIO_c) == 0xD4);

class daNpc_P2_HIO_c : public mDoHIO_entry_c {
public:
    daNpc_P2_HIO_c();
    virtual ~daNpc_P2_HIO_c() {}

public:
    /* 0x04 */ daNpc_P2_childHIO_c mChild[3];
    /* 0x280 */ f32 m280;
    /* 0x284 */ f32 m284;
};  // Size: 0x288

STATIC_ASSERT(sizeof(daNpc_P2_HIO_c) == DEMO_SELECT(0x28C, 0x288));

static daNpc_P2_HIO_c l_HIO;

/* 000000EC-0000020C       .text __ct__19daNpc_P2_childHIO_cFv */
daNpc_P2_childHIO_c::daNpc_P2_childHIO_c() {
    m2C = 0;
    m48.x = 5.0f;
    m48.y = -545.0f;
    m48.z = 30.0f;
    m54 = 80.0f;
    m58 = 180.0f;
    m30.x = 0.0f;
    m30.y = -550.0f;
    m30.z = -3020.0f;
    m3C.x = 0.0f;
    m3C.y = -550.0f;
    m3C.z = -3400.0f;
    m5C = 200.0f;
    m60 = 300.0f;
    m64 = 30.0f;
    m68 = 300.0f;
    m6C = 0;
    m70 = 5000.0f;
    m74[0] = 0.0f;
    m74[1] = 14.0f;
    m74[2] = 8.0f;
    m74[3] = 8.0f;
    m74[4] = 4.0f;
    m74[5] = 0.0f;
    m74[6] = 4.0f;
    m74[7] = 8.0f;
    m74[8] = 6.0f;
    m74[9] = 6.0f;
    m74[10] = 2.0f;
    m74[11] = 20.0f;
    m74[12] = 20.0f;
    m74[13] = 8.0f;
    m74[14] = 12.0f;
    m74[15] = 8.0f;
    m74[16] = 4.0f;
    m74[17] = 8.0f;
    m74[18] = 8.0f;
    m74[19] = 8.0f;
    m74[20] = 8.0f;
    m74[21] = 8.0f;
    m74[22] = 15.0f;
    m74[23] = 8.0f;
}

/* 00000248-000003E8       .text __ct__14daNpc_P2_HIO_cFv */
daNpc_P2_HIO_c::daNpc_P2_HIO_c() {
    mChild[0].mNpc.m04 = -20.0f;
    mChild[0].mNpc.mMaxHeadX = 0x834;
    mChild[0].mNpc.mMaxHeadY = 0x28A0;
    mChild[0].mNpc.mMaxBackboneX = 0;
    mChild[0].mNpc.mMaxBackboneY = 0x1130;
    mChild[0].mNpc.mMinHeadX = -0x1FFE;
    mChild[0].mNpc.mMinHeadY = -0x28A0;
    mChild[0].mNpc.mMinBackboneX = 0;
    mChild[0].mNpc.mMinBackboneY = -0x1130;
    mChild[0].mNpc.mMaxTurnStep = 0x258;
    mChild[0].mNpc.mMaxHeadTurnVel = 0;
    mChild[0].mNpc.mAttnYOffset = 50.0f;
    mChild[0].mNpc.mMaxAttnAngleY = 0x4000;
    mChild[0].mNpc.m22 = 0;
    mChild[0].mNpc.mMaxAttnDistXZ = 200.0f;

    mChild[1].mNpc.m04 = -26.0f;
    mChild[1].mNpc.mMaxHeadX = 0x1FFE;
    mChild[1].mNpc.mMaxHeadY = 0x4268;
    mChild[1].mNpc.mMaxBackboneX = 0;
    mChild[1].mNpc.mMaxBackboneY = 0xFA0;
    mChild[1].mNpc.mMinHeadX = -0x1FFE;
    mChild[1].mNpc.mMinHeadY = -0x3A98;
    mChild[1].mNpc.mMinBackboneX = 0;
    mChild[1].mNpc.mMinBackboneY = -0xFA0;
    mChild[1].mNpc.mMaxTurnStep = 0x1000;
    mChild[1].mNpc.mMaxHeadTurnVel = 0x800;
    mChild[1].mNpc.mAttnYOffset = 50.0f;
    mChild[1].mNpc.mMaxAttnAngleY = 0x4000;
    mChild[1].mNpc.m22 = 0;
    mChild[1].mNpc.mMaxAttnDistXZ = 200.0f;

    mChild[2].mNpc.m04 = -20.0f;
    mChild[2].mNpc.mMaxHeadX = 0x1FFE;
    mChild[2].mNpc.mMaxHeadY = 0x2134;
    mChild[2].mNpc.mMaxBackboneX = 0;
    mChild[2].mNpc.mMaxBackboneY = 0x8E8;
    mChild[2].mNpc.mMinHeadX = -0x1FFE;
    mChild[2].mNpc.mMinHeadY = -0x2134;
    mChild[2].mNpc.mMinBackboneX = 0;
    mChild[2].mNpc.mMinBackboneY = -0x8E8;
    mChild[2].mNpc.mMaxTurnStep = 0x960;
    mChild[2].mNpc.mMaxHeadTurnVel = 0x7D0;
    mChild[2].mNpc.mAttnYOffset = 50.0f;
    mChild[2].mNpc.mMaxAttnAngleY = 0x4000;
    mChild[2].mNpc.m22 = 0;
    mChild[2].mNpc.mMaxAttnDistXZ = 200.0f;

    m280 = 3.0f;
    m284 = 0.9f;
}

static const int l_btp_ix_tbl[] = {
    dRes_INDEX_P2_BTP_P2HEAD01_e,
    dRes_INDEX_P2_BTP_P2HEAD02_e,
};
static const int l_bmt_ix_tbl[] = {
    -1,
    dRes_INDEX_P2_BMT_P2B_e,
    dRes_INDEX_P2_BMT_P2C_e,
};

/* 00000490-0000068C       .text nodeCallBack__FP7J3DNodei */
static BOOL nodeCallBack(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DJoint* joint = (J3DJoint*)node;
        J3DModel* model = j3dSys.getModel();
        daNpc_P2_c* i_this = (daNpc_P2_c*)model->getUserArea();
        if (i_this != NULL) {
            s32 jntNo = joint->getJntNo();
            mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));
            if (jntNo == 4) {
                static cXyz l_offsetAttPos(0.0f, 0.0f, 0.0f);
                static cXyz l_offsetEyePos(20.0f, 10.0f, 0.0f);
                mDoMtx_stack_c::multVec(&l_offsetAttPos, &i_this->getAttentionBasePos());
                mDoMtx_stack_c::XrotM(i_this->getHead_y());
                mDoMtx_stack_c::ZrotM(-i_this->getHead_x());
                mDoMtx_stack_c::multVec(&l_offsetEyePos, &i_this->getEyePos());
                i_this->incAttnSetCount();
            } else if (jntNo == 2) {
                mDoMtx_stack_c::XrotM(i_this->getBackbone_y());
                mDoMtx_stack_c::ZrotM(i_this->getBackbone_x());
                mDoMtx_stack_c::ZrotM(REG12_S(0));
                mDoMtx_stack_c::XrotM(REG12_S(1));
                mDoMtx_stack_c::YrotM(REG12_S(2));
            }
            cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
            model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
        }
    }
    return TRUE;
}

/* 0000068C-00000788       .text initTexPatternAnm__10daNpc_P2_cFb */
BOOL daNpc_P2_c::initTexPatternAnm(bool i_modify) {
    J3DModelData* modelData = mpHeadModel->getModelData();
    J3DAnmTexPattern* head_tex_pattern = (J3DAnmTexPattern*)dComIfG_getObjectRes(m_arc_name, l_btp_ix_tbl[m7D0]);
    JUT_ASSERT(0x185, head_tex_pattern != 0);
    if (!mBtpAnm.init(modelData, head_tex_pattern, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, i_modify, FALSE)) {
        return FALSE;
    }
    mBtpFrame = 0;
    mBtpTimer = 0;
    return TRUE;
}

/* 00000788-00000810       .text playTexPatternAnm__10daNpc_P2_cFv */
void daNpc_P2_c::playTexPatternAnm() {
    if (cLib_calcTimer(&mBtpTimer) == 0) {
        if (mBtpFrame >= mBtpAnm.getBtpAnm()->getFrameMax()) {
            mBtpFrame -= mBtpAnm.getBtpAnm()->getFrameMax();
            mBtpTimer = cM_rndF(100.0f) + 30.0f;
        } else {
            mBtpFrame++;
        }
    }
}

/* 00000810-00000A90       .text setAnm__10daNpc_P2_cFv */
void daNpc_P2_c::setAnm() {
    static const int a_anm_bck_tbl[] = {
        dRes_INDEX_P2_BCK_P2_WAIT01_e,
        dRes_INDEX_P2_BCK_P2_WALK01_e,
        dRes_INDEX_P2_BCK_P2_RUN_e,
        dRes_INDEX_P2_BCK_P2_JUMP_e,
        dRes_INDEX_P2_BCK_P2_LANDING_e,
        dRes_INDEX_P2_BCK_P2_ROPESWINGF_e,
        dRes_INDEX_P2_BCK_P2_ROPESWINGR_e,
        dRes_INDEX_P2_BCK_P2_NICOWAIT_e,
        dRes_INDEX_P2_BCK_P2_NWAIT02_e,
        dRes_INDEX_P2_BCK_P2_NWAIT03_e,
        dRes_INDEX_P2_BCK_P2_NTALK01_e,
        dRes_INDEX_P2_BCK_P2_NTALK02_e,
        dRes_INDEX_P2_BCK_P2_NTALK03_e,
        dRes_INDEX_P2_BCK_P2_NTALK04_e,
        dRes_INDEX_P2_BCK_P2_NKYORO2_e,
        dRes_INDEX_P2_BCK_P2_NTHINK_e,
        dRes_INDEX_P2_BCK_P2_NNOD_e,
        dRes_INDEX_P2_BCK_P2_NBANZAI_e,
        dRes_INDEX_P2_BCK_P2_N2RUN_e,
        dRes_INDEX_P2_BCK_P2_NODOROKI_e,
        dRes_INDEX_P2_BCK_P2_MWAIT01_e,
        dRes_INDEX_P2_BCK_P2_MWAIT02_e,
        dRes_INDEX_P2_BCK_P2_MTALK01_e,
        dRes_INDEX_P2_BCK_P2_MTALK02_e,
        dRes_INDEX_P2_BCK_P2_ZWAIT01_e,
        dRes_INDEX_P2_BCK_P2_ZNOZOKU_e,
        dRes_INDEX_P2_BCK_P2_ZTALK01_e,
        dRes_INDEX_P2_BCK_P2_ZMIAGE_e,
    };
    static s8 a_anm_num_tbl[3][24] = {
        {-1, 24, -1, -1, 1, 2, -1, -1, 3, 4, 4, 5, 6, 26, -1, -1, -1, -1, -1, -1, -1, -1, 25, 27},
        {-1, 7, 8, 9, 1, 2, 18, 19, 3, 4, 4, 5, 6, 12, 11, 10, 13, 14, 15, 16, 17, -1, -1, -1},
        {-1, 20, -1, -1, 1, 2, -1, -1, 3, 4, 4, 5, 6, 22, 23, -1, -1, -1, -1, -1, -1, 21, -1, -1},
    };
    static int a_play_mode_tbl[] = {
        -1, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 0, 2, 0, 0, 0, 2, 0,
    };
    static f32 a_morf_frame_tbl[] = {
        0.0f, 14.0f, 8.0f, 8.0f, 4.0f, 0.0f, 4.0f, 8.0f, 6.0f, 6.0f, 2.0f, 20.0f,
        20.0f, 8.0f, 12.0f, 8.0f, 4.0f, 8.0f, 8.0f, 8.0f, 8.0f, 8.0f, 15.0f, 8.0f,
    };
    static f32 a_play_speed_tbl[] = {
        0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };

    for (int i = 0; i < 24; i++) {
        a_morf_frame_tbl[i] = l_HIO.mChild[mType].m74[i];
    }

    if (m7D4 != mAnmNo) {
        s8 num = a_anm_num_tbl[mType][mAnmNo];
        if (num != -1) {
            m7D2 = num;
            m374 = 0.0f;
            s8 anmNo = mAnmNo;
            f32 speed = a_play_speed_tbl[anmNo];
            f32 moveSpeed = mEventCut.getMoveSpeed();
            if (0.0f != moveSpeed) {
                switch (mEventCut.getNowCut()) {
                case 2:
                case 4:
                    speed = moveSpeed;
                    speed *= 0.25f;
                    break;
                }
            }
            dNpc_setAnm(mpMorf, a_play_mode_tbl[anmNo], a_morf_frame_tbl[anmNo], speed, a_anm_bck_tbl[(s8)m7D2], -1, m_arc_name);
        }
    }

    if (mpMorf->getFrame() == 1.0f) {
        switch (m7D2) {
        case 0x12:
            fopAcM_monsSeStart(this, JA_SE_CV_NK_BEF_JUMP, 0);
            break;
        }
    }

    m7D4 = mAnmNo;

    if (mpMorf->isStop() && mAnmNo == 0x13) {
        mAnmNo = 3;
    }
    if (mAnmNo == 7 && mpMorf->isStop()) {
        mAnmNo = 1;
    }
    if (mType == 0 && mpMorf->isStop() && mAnmNo == 0x17) {
        mAnmNo = 0x0D;
    }
}

/* 00000A90-00000AFC       .text setTexAnm__10daNpc_P2_cFv */
void daNpc_P2_c::setTexAnm() {
    static s8 a_tex_pattern_num_tbl[2][0x17] = {
        {-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {-1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };

    if (m7D0 != a_tex_pattern_num_tbl[mType][m7D1] && a_tex_pattern_num_tbl[mType][m7D1] != -1 && mType != 2) {
        m7D0 = a_tex_pattern_num_tbl[mType][m7D1];
        initTexPatternAnm(true);
    }
}

/* 00000AFC-00000B90       .text setAttention__10daNpc_P2_cFv */
void daNpc_P2_c::setAttention() {
    u8 isMorf = mpMorf->isMorf() ? TRUE : FALSE;
    if (mEyeStopFlag == 0 || isMorf != 0) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
        attention_info.position.set(
            mAttnPos.x,
            mAttnPos.y + l_HIO.mChild[mType].mNpc.mAttnYOffset,
            mAttnPos.z
        );
    }
    mEyeStopFlag = false;
}

/* 00000B90-00000CEC       .text chkAttention__10daNpc_P2_cFv */
bool daNpc_P2_c::chkAttention() {
    f32 distXZ = fopAcM_searchPlayerDistanceXZ(this);
    s16 maxAttnAngleY = l_HIO.mChild[mType].mNpc.mMaxAttnAngleY;
    s16 lookAngle = current.angle.y + mJnt.getHead_y() + mJnt.getBackbone_y();

    if (mType == 1 && (m7C1 != 0 || m7D6 == 0x15) && distXZ < l_HIO.mChild[mType].mNpc.mMaxAttnDistXZ) {
        return true;
    }
    if (maxAttnAngleY > abs(lookAngle) && distXZ < l_HIO.mChild[mType].mNpc.mMaxAttnDistXZ && dComIfGp_event_runCheck()) {
        return true;
    }

    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 00000CEC-000010E8       .text lookBack__10daNpc_P2_cFv */
void daNpc_P2_c::lookBack() {
    cXyz local_30;
    cXyz lookTarget(0.0f, 0.0f, 0.0f);
    cXyz* dstPos = NULL;
    s16 desiredAngle = current.angle.y;
    bool headOnly = mEventCut.getAttnNoTurnFlag();

    if (m72C || mEventCut.getAttnFlag()) {
        mJnt.setTrn();
        local_30 = mEventCut.getAttnPos() + m7DC;
        dstPos = &local_30;
        lookTarget = current.pos;
        lookTarget.y = eyePos.y;
    } else {
        if (m7D6 == 0xE) {
            mJnt.setTrn();
        }

        switch (m7D6) {
        case 9:
        case 0xA:
            mJnt.setTrn();
            local_30 = m730;
            dstPos = &local_30;
            lookTarget = current.pos;
            lookTarget.y = eyePos.y;
            break;
        case 2:
            mJnt.setTrn();
            local_30 = dNpc_playerEyePos(l_HIO.mChild[mType].mNpc.m04);
            dstPos = &local_30;
            lookTarget = current.pos;
            lookTarget.y = eyePos.y;
            break;
        default:
            if (m724) {
                local_30 = dNpc_playerEyePos(l_HIO.mChild[mType].mNpc.m04);
                dstPos = &local_30;
                lookTarget = current.pos;
                lookTarget.y = eyePos.y;
            }
            break;
        }
    }

    if (mType == 0) {
        if (mAnmNo == 0x17) {
            fopAc_ac_c* actor = fopAcM_SearchByName(fpcNm_NPC_ZL1_e);
            if (actor != NULL) {
                s16 target = fopAcM_searchActorAngleY(this, actor);
                cLib_addCalcAngleS2(&current.angle.y, target, 4, 0x800);
                lookTarget = current.pos;
                lookTarget.y = eyePos.y;
                mJnt.onHeadLock();
                mJnt.onBackBoneLock();
                mJnt.lookAtTarget(&current.angle.y, &actor->current.pos, lookTarget, desiredAngle, mMaxHeadVel, false);
                return;
            }
        } else if (mAnmNo == 0x16) {
            headOnly = false;
            mJnt.clrTrn();
            mJnt.onHeadLock();
            mJnt.onBackBoneLock();
        } else {
            mJnt.offHeadLock();
            mJnt.offBackBoneLock();
        }
    }

    if (m7D6 == 0x10) {
        headOnly = false;
        mJnt.clrTrn();
    }

    if (mType == 1 && m7D6 == 0xD && m291 == 1) {
        mJnt.setTrn();
        local_30 = m73C;
        dstPos = &local_30;
        lookTarget = current.pos;
        lookTarget.y = eyePos.y;
    }

    if (mJnt.trnChk()) {
        cLib_addCalcAngleS2(&mMaxHeadVel, l_HIO.mChild[mType].mNpc.mMaxHeadTurnVel, 4, 0x800);
    } else {
        mMaxHeadVel = 0;
    }
    mJnt.lookAtTarget(&current.angle.y, dstPos, lookTarget, desiredAngle, mMaxHeadVel, headOnly);
}

/* 000010E8-00001160       .text setMtx__10daNpc_P2_cFv */
void daNpc_P2_c::setMtx() {
    J3DModel* model = mpMorf->getModel();
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(current.angle.x, current.angle.y, current.angle.z);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00001160-00001204       .text setCollision__10daNpc_P2_cFv */
void daNpc_P2_c::setCollision() {
    if (m7C1) {
        mCyl.SetC(current.pos);
        mCyl.SetR(90.0f);
        mCyl.SetH(120.0f);
    } else {
        mCyl.SetC(current.pos);
        mCyl.SetR(30.0f);
        mCyl.SetH(120.0f);
    }
    dComIfG_Ccsp()->Set(&mCyl);
}

/* 00001204-00001338       .text smoke_set__10daNpc_P2_cFv */
void daNpc_P2_c::smoke_set() {
    static JGeometry::TVec3<f32> smoke_scale(1.25f, 1.25f, 1.25f);
    JPABaseEmitter* p_smoke_emitter = mSmoke.getEmitter();
    if (mSmoke.getEmitter() == NULL) {
        p_smoke_emitter = dComIfGp_particle_setToon(
            dPa_name::ID_AK_JT_ELEMENTSMOKE00,
            &mSavedPos,
            &mSavedAngle,
            NULL,
            0xB9,
            &mSmoke,
            fopAcM_GetRoomNo(this)
        );
    }
    p_smoke_emitter = mSmoke.getEmitter();
    if (p_smoke_emitter != NULL) {
        p_smoke_emitter->setRate(2.0f);
        mSmoke.getEmitter()->setSpread(0.25f);
        mSmoke.getEmitter()->setAwayFromCenterSpeed(0.0f);
        mSmoke.getEmitter()->setAwayFromAxisSpeed(5.0f);
        mSmoke.getEmitter()->setDirectionalSpeed(20.0f);
        mSmoke.getEmitter()->setGlobalScale(smoke_scale);
    }
}

static fpc_ProcID l_msgId;
static msg_class* l_msg;

/* 00001338-00001534       .text next_msgStatus__10daNpc_P2_cFPUl */
u16 daNpc_P2_c::next_msgStatus(unsigned long* pMsgNo) {
    u16 msgStatus = fopMsgStts_MSG_CONTINUES_e;
    switch (*pMsgNo) {
    case 0x1011:
        *pMsgNo = 0x102F;
        break;
    case 0x102F:
        *pMsgNo = 0x1030;
        break;
    case 0x1012:
        *pMsgNo = 0x1031;
        break;
    case 0x1031:
        *pMsgNo = 0x1032;
        break;
    case 0xC96:
        *pMsgNo = 0xC98;
        break;
    case 0x1018:
        *pMsgNo = 0x1019;
        break;
    case 0x1020:
        *pMsgNo = 0x1021;
        break;
    case 0x1022:
        *pMsgNo = 0x102C;
        break;
    case 0x1023:
        *pMsgNo = 0x1024;
        break;
    case 0x1025:
        *pMsgNo = 0x1026;
        break;
    case 0x1026:
        *pMsgNo = 0x1027;
        break;
    case 0x1027:
        *pMsgNo = 0x102D;
        break;
    case 0x1B20:
        *pMsgNo = 0x1B21;
        break;
    case 0x1B21:
        *pMsgNo = 0x1B22;
        break;
    case 0x1B22:
        *pMsgNo = 0x1B23;
        break;
    case 0x1B23:
        *pMsgNo = 0x1B24;
        break;
    case 0x1B28:
        *pMsgNo = 0x1B29;
        break;
    case 0x1B29:
        *pMsgNo = 0x1B2A;
        break;
    case 0x1B2A:
        *pMsgNo = 0x1B2B;
        break;
    case 0x1B2B:
        *pMsgNo = 0x1B2C;
        break;
    case 0x1B2C:
        *pMsgNo = 0x1B2D;
        break;
    case 0x1B2D:
        *pMsgNo = 0x1B2E;
        break;
    default:
        msgStatus = fopMsgStts_MSG_ENDS_e;
        break;
    }
    return msgStatus;
}

/* 00001534-000016CC       .text getMsg__10daNpc_P2_cFv */
u32 daNpc_P2_c::getMsg() {
    u32 msg = 0;
    switch (mType) {
    case 0:
        if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
            msg = dLib_setFirstMsg(dSv_event_flag_c::UNK_0702, 0x100E, 0x100F);
        } else {
            msg = 0x1010;
        }
        break;
    case 1:
        if (m291 == 1) {
            if (dComIfGs_isEventBit(dSv_event_flag_c::UNK_0F02)) {
                msg = dLib_setFirstMsg(dSv_event_flag_c::UNK_1502, 0x1B35, 0x1B36);
            } else {
                msg = 0x1028;
            }
        } else if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0720)) {
            msg = dLib_setFirstMsg(dSv_event_flag_c::UNK_0940, 0xC96, 0xC97);
        } else if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
            if (dComIfGs_isEventBit(dSv_event_flag_c::UNK_0710)) {
                msg = 0x1028;
            }
        } else {
            msg = dLib_setFirstMsg(dSv_event_flag_c::UNK_0704, 0x1029, 0x102A);
        }
        break;
    case 2:
        if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
            msg = dLib_setFirstMsg(dSv_event_flag_c::UNK_0701, 0x1011, 0x1012);
        } else {
            msg = 0x1013;
        }
        break;
    }
    return msg;
}

/* 000016CC-000016D8       .text talkInit__10daNpc_P2_cFv */
void daNpc_P2_c::talkInit() {
    mTalkState = 0;
}

/* 000016D8-000017AC       .text anmAtr__10daNpc_P2_cFUs */
void daNpc_P2_c::anmAtr(unsigned short status) {
    static const s8 anm_atr[] = {
        0x01, 0x02, 0x03, 0x0F, 0x0E, 0x0D, 0x10, 0x11, 0x12, 0x13, 0x14,
        0x01, 0x02, 0x0D, 0x0E, 0x01, 0x16, 0x0D, 0x17, 0x07, 0x06,
    };

    if (status == 6) {
        u8 attr = dComIfGp_getMesgAnimeAttrInfo();
        if (attr < 0x15) {
            if (mType == 0 && mAnmNo == 0x0D && anm_atr[attr] == 0x17) {
                return;
            }
            if (mType == 1 && anm_atr[attr] == 7) {
                if (m751 == 0) {
                    m751 = 1;
                    mAnmNo = anm_atr[attr];
                }
                return;
            }
            if (mAnmNo != anm_atr[attr]) {
                mAnmNo = anm_atr[attr];
            }
        } else {
            mAnmNo = 1;
        }
    } else if (status == 0x10) {
        mAnmNo = 1;
    }
}

#include "d/actor/d_a_npc_p2_cut.inc"

/* 000017AC-00001918       .text talk__10daNpc_P2_cFb */
u16 daNpc_P2_c::talk(bool i_useMsgNo) {
    u16 mode = 0xFF;
    s8 temp01 = mTalkState;
    if (mTalkState == 0) {
        l_msgId = fpcM_ERROR_PROCESS_ID_e;
        if (!i_useMsgNo) {
            mMsgNo = getMsg();
        }
        mTalkState = 1;
    } else if (mTalkState == -1) {
        mode = fopMsgStts_BOX_CLOSED_e;
        anmAtr(fopMsgStts_BOX_CLOSED_e);
    } else if (l_msgId == fpcM_ERROR_PROCESS_ID_e) {
        l_msgId = fopMsgM_messageSet(mMsgNo, this);
    } else {
        switch (temp01) {
        case 1:
            l_msg = fopMsgM_SearchByID(l_msgId);
            if (l_msg != NULL) {
                mTalkState = 2;
            }
            break;
        case 2:
            mode = l_msg->mStatus;
            if (mode == fopMsgStts_MSG_DISPLAYED_e) {
                l_msg->mStatus = next_msgStatus(&mMsgNo);
                if (l_msg->mStatus == fopMsgStts_MSG_CONTINUES_e) {
                    fopMsgM_messageSet(mMsgNo);
                }
            } else if (mode == fopMsgStts_BOX_CLOSED_e) {
                l_msg->mStatus = fopMsgStts_MSG_DESTROYED_e;
                mTalkState = -1;
                m751 = 0;
            }
            anmAtr(mode);
            break;
        }
    }
    return mode;
}

/* 00001918-00001998       .text eventOrder__10daNpc_P2_cFv */
void daNpc_P2_c::eventOrder() {
    static char* a_demo_name_tbl[] = {
        "P2B_INTRO",
        "P2B_TO_GOAL",
        "P2B_GOAL_WAIT_TALK",
        "P2B_GOAL",
        "P2B_ARRIVE_MAJYU",
        "P2B_INTRO_2",
        "P2B_GOAL_2",
        "P2B_BOMB_GET",
    };

    if (m7D5 == 1 || m7D5 == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (m7D5 == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (m7D5 >= 3) {
        fopAcM_orderOtherEvent2(this, a_demo_name_tbl[m7D5 - 3], 1, 0xFFFF);
    }
}

/* 00001998-00001A00       .text checkOrder__10daNpc_P2_cFv */
void daNpc_P2_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        m7D5 = 0;
    } else if (eventInfo.checkCommandTalk()) {
        if (m7D5 == 1 || m7D5 == 2) {
            m7D5 = 0;
            mHasAttention = true;
            talkInit();
        }
    }
}

/* 00001A00-00001A7C       .text demo_wait_2__10daNpc_P2_cFv */
void daNpc_P2_c::demo_wait_2() {
    if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m48, l_HIO.mChild[mType].m54, l_HIO.mChild[mType].m58)) {
        m7D5 = 8;
        m7D6 = 0x13;
    }
}

/* 00001A7C-00001B00       .text demo_intro_2__10daNpc_P2_cFv */
void daNpc_P2_c::demo_intro_2() {
    if (dComIfGp_evmng_endCheck("P2B_INTRO_2")) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_1A04);
        m7D6 = 0x14;
        m7D5 = 0;
        dComIfGp_event_reset();
    }
}

/* 00001B00-00001BBC       .text goal_wait_2__10daNpc_P2_cFv */
void daNpc_P2_c::goal_wait_2() {
    if (fopAcM_isSwitch(this, mSwitchNo)) {
        if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m3C, l_HIO.mChild[mType].m60, l_HIO.mChild[mType].m68)) {
            m7D6 = 0x15;
            daObjTimer::Act_c* timer = (daObjTimer::Act_c*)fopAcM_SearchByName(fpcNm_Obj_Timer_e);
            if (timer != NULL) {
                timer->stopTimer();
            }
        }
    }
}

/* 00001BBC-00001C74       .text demo_goal_2__10daNpc_P2_cFv */
void daNpc_P2_c::demo_goal_2() {
    if (dComIfGp_evmng_endCheck("P2B_GOAL_2")) {
        m7D6 = 0xD;
        m7D5 = 0;
        m7C1 = 1;
        m73C = mEventCut.getAttnPos() + m7DC;
        dComIfGp_event_reset();
    } else {
        m7D5 = 9;
    }
}

/* 00001C74-00001CFC       .text demo_bomb_get__10daNpc_P2_cFv */
void daNpc_P2_c::demo_bomb_get() {
    if (dComIfGp_evmng_endCheck("P2B_BOMB_GET")) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0F02);
        m7D6 = 1;
        m7D5 = 0;
        m7C1 = 0;
        dComIfGp_event_reset();
    }
}

/* 00001CFC-00001D78       .text demo_wait__10daNpc_P2_cFv */
void daNpc_P2_c::demo_wait() {
    if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m48, l_HIO.mChild[mType].m54, l_HIO.mChild[mType].m58)) {
        m7D5 = 3;
        m7D6 = 4;
    }
}

/* 00001D78-00001DE8       .text demo_intro__10daNpc_P2_cFv */
void daNpc_P2_c::demo_intro() {
    if (dComIfGp_evmng_endCheck("P2B_INTRO")) {
        m7D6 = 5;
        m7D5 = 0;
        dComIfGp_event_reset();
    }
}

/* 00001DE8-00001E4C       .text demo_lift__10daNpc_P2_cFv */
void daNpc_P2_c::demo_lift() {
    if (dComIfGp_evmng_endCheck("Hlift_up")) {
        m7D6 = 6;
        m7D5 = 4;
        m804 = 0;
    }
}

/* 00001E4C-00001EC8       .text demo_jump__10daNpc_P2_cFv */
void daNpc_P2_c::demo_jump() {
    if (dComIfGp_evmng_endCheck("P2B_TO_GOAL")) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0720);
        m7D6 = 9;
        dComIfGp_event_reset();
    }
}

/* 00001EC8-000021C4       .text goal_goalpos_to_talkpos__10daNpc_P2_cFv */
void daNpc_P2_c::goal_goalpos_to_talkpos() {
    m7C1 = 1;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    if ((current.pos.y - player->current.pos.y - 300.0f) < 0.0f) {
        m7D6 = 0xA;
        return;
    }
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0x0;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0x0;

    static cXyz aim_pos = cXyz(0.0f, -450.0f, -2915.0f);
    static cXyz attn_pos = cXyz(0.0f, -450.0f, -2715.0f);

    s16 sVar5 = cLib_targetAngleY(&current.pos, &aim_pos);
    m730 = attn_pos;
    if (std::fabsf(sVar5 - current.angle.y) < 5376.0f) {
        mAnmNo = 0x4;
        cLib_addCalc2(&current.pos.x, aim_pos.x, 0.1f, 4.0f);
        cLib_addCalc2(&current.pos.z, aim_pos.z, 0.1f, 4.0f);
    }
    f32 fVar8 = (aim_pos - current.pos).absXZ();
    if (fVar8 < 5.0f) {
        mAnmNo = 0x1;
        m7D6 = 7;
    }

    if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m30, l_HIO.mChild[mType].m5C, l_HIO.mChild[mType].m64) && !player->checkPlayerFly()) {
        m7D6 = 0xC;
        daObjTimer::Act_c* timer = (daObjTimer::Act_c*)fopAcM_SearchByName(fpcNm_Obj_Timer_e);
        if (timer != NULL) {
            timer->stopTimer();
        }
    }
}

/* 000021C4-000025B8       .text goal_talkpos_to_goalpos__10daNpc_P2_cFv */
void daNpc_P2_c::goal_talkpos_to_goalpos() {
    m7C1 = 1;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    if (fopAcM_searchPlayerDistanceXZ(this) < l_HIO.mChild[mType].m70 &&
        (current.pos.y - player->current.pos.y - 300.0f) > 0.0f)
    {
        m7D6 = 9;
        return;
    }
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0x0;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0x0;
    static cXyz aim_pos = cXyz(0.0f, -450.0f, -3130.0f);
    static cXyz attn_pos = cXyz(0.0f, -450.0f, -3380.0f);
    static cXyz talk_attn_pos = cXyz(0.0f, -450.0f, -2715.0f);

    f32 fVar8 = (aim_pos - current.pos).absXZ();
    if (fVar8 < 5.0f) {
        m730 = talk_attn_pos;
    } else {
        m730 = attn_pos;
    }

    s16 sVar5 = cLib_targetAngleY(&current.pos, &m730);
    if (fVar8 < 5.0f) {
        if (std::fabsf(sVar5 - current.angle.y) < 5376.0f) {
            mAnmNo = 0x1;
            m7D6 = 0xB;
        }
    } else {
        if (std::fabsf(sVar5 - current.angle.y) < 5376.0f) {
            mAnmNo = 4;
            cLib_addCalc2(&current.pos.x, aim_pos.x, 0.1f, 4.0f);
            cLib_addCalc2(&current.pos.z, aim_pos.z, 0.1f, 4.0f);
        }
    }
    if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m30, l_HIO.mChild[mType].m5C, l_HIO.mChild[mType].m64) && !player->checkPlayerFly()) {
        m7D6 = 0xC;
        daObjTimer::Act_c* timer = (daObjTimer::Act_c*)fopAcM_SearchByName(fpcNm_Obj_Timer_e);
        if (timer != NULL) {
            timer->stopTimer();
        }
    }
}

/* 000025B8-000026F4       .text goal_goalpos_wait__10daNpc_P2_cFv */
void daNpc_P2_c::goal_goalpos_wait() {
    m7C1 = 1;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    if (fopAcM_searchPlayerDistanceXZ(this) < l_HIO.mChild[mType].m70 &&
        (current.pos.y - player->current.pos.y - 300.0f) > 0.0f)
    {
        m7D6 = 9;
        return;
    }
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0x2;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0x2;
    if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m30, l_HIO.mChild[mType].m5C, l_HIO.mChild[mType].m64)) {
        if (!player->checkPlayerFly()) {
            m7D6 = 0xC;
            daObjTimer::Act_c* timer = (daObjTimer::Act_c*)fopAcM_SearchByName(fpcNm_Obj_Timer_e);
            if (timer != NULL) {
                timer->stopTimer();
            }
        }
    }
}

/* 000026F4-00002864       .text goal_talkpos_wait__10daNpc_P2_cFv */
void daNpc_P2_c::goal_talkpos_wait() {
    m7C1 = 1;
    daPy_py_c* player = daPy_getPlayerActorClass();
    if ((current.pos.y - player->current.pos.y - 300.0f) < 0.0f) {
        m7D6 = 0xA;
        return;
    }
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0x22;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0x22;
    if (m74A == 0) {
        m7D5 = 1;
        m74A = 1;
    } else {
        if (mHasAttention != 0) {
            m7D6 = 8;
            dComIfGp_event_reset();
            m7D5 = 5;
        } else {
            if (fopAcM_GetSpeedF(player) < 1.0f) {
                m7D5 = 2;
            }
        }

        if (dLib_checkPlayerInCircle(l_HIO.mChild[mType].m30, l_HIO.mChild[mType].m5C, l_HIO.mChild[mType].m64) && !player->checkPlayerFly()) {
            m7D6 = 0xC;
            daObjTimer::Act_c* timer = (daObjTimer::Act_c*)fopAcM_SearchByName(fpcNm_Obj_Timer_e);
            if (timer != NULL) {
                timer->stopTimer();
            }
        }
    }
}

/* 00002864-00002908       .text demo_goal__10daNpc_P2_cFv */
void daNpc_P2_c::demo_goal() {
    m7C1 = 0;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA9;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;
    if (dComIfGp_evmng_endCheck("P2B_GOAL")) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0710);
        m7D6 = 0xD;
        m7D5 = 0;
        dComIfGp_event_reset();
    } else {
        m7D5 = 6;
    }
}

/* 00002908-00002978       .text goal_talkpos_talk__10daNpc_P2_cFv */
void daNpc_P2_c::goal_talkpos_talk() {
    if (dComIfGp_evmng_endCheck("P2B_GOAL_WAIT_TALK")) {
        m7D6 = 7;
        dComIfGp_event_reset();
        mHasAttention = false;
    }
}

/* 00002978-00002A20       .text treasure_wait__10daNpc_P2_cFv */
void daNpc_P2_c::treasure_wait() {
    if (dComIfGp_evmng_endCheck("DEFAULT_TREASURE")) {
        if (m291 == 0) {
            m7D6 = 0xF;
            m7D5 = 7;
        } else if (m291 == 1) {
            m7D6 = 0x16;
            m7D5 = 0xA;
        }
    } else if (mHasAttention) {
        m7D6 = 0xE;
    } else {
        m7D5 = 2;
    }
}

/* 00002A20-00002A88       .text treasure_wait_talk__10daNpc_P2_cFv */
void daNpc_P2_c::treasure_wait_talk() {
    mEyeStopFlag = true;
    if (talk(false) == fopMsgStts_BOX_CLOSED_e) {
        m7D6 = 0xD;
        mHasAttention = false;
        dComIfGp_event_reset();
    }
}

/* 00002A88-00002B18       .text demo_arrive__10daNpc_P2_cFv */
void daNpc_P2_c::demo_arrive() {
    if (dComIfGp_evmng_endCheck("P2B_ARRIVE_MAJYU")) {
        dKy_instant_timechg(300.0f);
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0808);
        m7D6 = 1;
        m7D5 = 0;
        dComIfGp_event_reset();
    }
}

/* 00002B18-00002B3C       .text wait01__10daNpc_P2_cFv */
void daNpc_P2_c::wait01() {
    if (mHasAttention) {
        m7D6 = 2;
    } else {
        m7D5 = 2;
    }
}

/* 00002B3C-00002BE8       .text zukotelescope__10daNpc_P2_cFv */
void daNpc_P2_c::zukotelescope() {
    mAnmNo = 0x16;
    if (parentActorID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = fopAcM_SearchByID(parentActorID);
        cLib_addCalcAngleS2(&current.angle.y, m748 + actor->current.angle.y, 4, 0x800);
    }
    if (mHasAttention) {
        mAnmNo = 1;
        mJnt.offHeadLock();
        mJnt.offBackBoneLock();
        m7D6 = 2;
    } else {
        m7D5 = 2;
    }
}

/* 00002BE8-00002D78       .text moccowait__10daNpc_P2_cFv */
void daNpc_P2_c::moccowait() {
    if (parentActorID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = fopAcM_SearchByID(parentActorID);
        cLib_addCalcAngleS2(&current.angle.y, m748 + actor->current.angle.y, 4, 0x800);
    }

    if (mAnmNo == 0x15) {
        mpMorf2->setFrame(mpMorf->getFrame());
    } else {
        mpMorf2->setFrame(0.0f);
    }

    if (mAnmNo == 1 && cLib_calcTimer(&m74C) == 0) {
        mAnmNo = 0x15;
        m74C = (s16)(cM_rndF(100.0f) + 200.0f);
    }

    if (mAnmNo == 0x15) {
        if (mpMorf->isStop()) {
            mAnmNo = 1;
        }
    }

    if (mHasAttention != 0 && mAnmNo == 1) {
        m7D6 = 2;
    } else {
        m7D5 = 2;
    }
}

/* 00002D78-00002E38       .text talk01__10daNpc_P2_cFv */
void daNpc_P2_c::talk01() {
    if (talk(false) == fopMsgStts_BOX_CLOSED_e) {
        if (mType == 2) {
            mAnmNo = 1;
            m7D6 = 0x10;
        } else if (mType == 0 && !dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
            m7D6 = 0x11;
            mJnt.onHeadLock();
            mJnt.onBackBoneLock();
        } else {
            mAnmNo = 1;
            m7D6 = 1;
        }
        dComIfGp_event_reset();
        mHasAttention = false;
    }
}

/* 00002E38-00003030       .text intro_action__10daNpc_P2_cFPv */
int daNpc_P2_c::intro_action(void*) {
    if (m808 == 0) {
        if (m291 == 0) {
            if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0720)) {
                m7D6 = 3;
            } else {
                m7D6 = 0xA;
            }
        } else {
            if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_1A04)) {
                m7D6 = 0x12;
            } else {
                m7D6 = 0x14;
            }
        }
        m808++;
    } else if (m808 != -1) {
        m724 = chkAttention();
        m7D5 = 0;
        switch ((s8)m7D6) {
        case 1:
            wait01();
            break;
        case 2:
            talk01();
            break;
        case 3:
            demo_wait();
            break;
        case 4:
            demo_intro();
            break;
        case 5:
            demo_lift();
            break;
        case 6:
            demo_jump();
            break;
        case 7:
            goal_talkpos_wait();
            break;
        case 8:
            goal_talkpos_talk();
            break;
        case 9:
            goal_goalpos_to_talkpos();
            break;
        case 0xA:
            goal_talkpos_to_goalpos();
            break;
        case 0xB:
            goal_goalpos_wait();
            break;
        case 0xC:
            demo_goal();
            break;
        case 0xD:
            treasure_wait();
            break;
        case 0xE:
            treasure_wait_talk();
            break;
        case 0xF:
            demo_arrive();
            break;
        case 0x12:
            demo_wait_2();
            break;
        case 0x13:
            demo_intro_2();
            break;
        case 0x14:
            goal_wait_2();
            break;
        case 0x15:
            demo_goal_2();
            break;
        case 0x16:
            demo_bomb_get();
            break;
        }
        lookBack();
        setAttention();
    }
    return TRUE;
}

/* 00003030-00003158       .text wait_action__10daNpc_P2_cFPv */
int daNpc_P2_c::wait_action(void*) {
    if (m808 == 0) {
        if (mType == 2) {
            m7D6 = 0x10;
        } else if (mType == 0 && !dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
            m7D6 = 0x11;
        } else {
            m7D6 = 1;
        }
        m808++;
    } else if (m808 != -1) {
        m724 = chkAttention();
        m7D5 = 0;
        switch ((s8)m7D6) {
        case 1:
            wait01();
            break;
        case 0x10:
            moccowait();
            break;
        case 0x11:
            zukotelescope();
            break;
        case 2:
            talk01();
            break;
        }
        lookBack();
        setAttention();
    }
    return TRUE;
}

/* 00003158-00003520       .text _execute__10daNpc_P2_cFv */
bool daNpc_P2_c::_execute() {
    mJnt.setParam(
        l_HIO.mChild[mType].mNpc.mMaxBackboneX,
        l_HIO.mChild[mType].mNpc.mMaxBackboneY,
        l_HIO.mChild[mType].mNpc.mMinBackboneX,
        l_HIO.mChild[mType].mNpc.mMinBackboneY,
        l_HIO.mChild[mType].mNpc.mMaxHeadX,
        l_HIO.mChild[mType].mNpc.mMaxHeadY,
        l_HIO.mChild[mType].mNpc.mMinHeadX,
        l_HIO.mChild[mType].mNpc.mMinHeadY,
        l_HIO.mChild[mType].mNpc.mMaxTurnStep
    );

    if (mType != 2) {
        playTexPatternAnm();
    }

    mbAnimFinished = mpMorf->play(
        &eyePos,
        mAcch.ChkGroundHit() ? dComIfG_Bgsp()->GetMtrlSndId(mAcch.m_gnd) : 0,
        dComIfGp_getReverb(fopAcM_GetRoomNo(this))
    );

    if (mpMorf->getFrame() < m374) {
        mbAnimFinished = true;
    }

    m374 = mpMorf->getFrame();

    checkOrder();

    (this->*mActionFunc)(NULL);

    if (dComIfGp_event_runCheck() && !eventInfo.checkCommandTalk()) {
        if (!mEventCut.cutProc()) {
            mEventCut.setAttnFlag(false);
            cutProc();
        } else {
            m72C = 0;
            if (mEventCut.getNowCut() != -1) {
                switch (mEventCut.getNowCut()) {
                case 2:
                case 4:
                    if (mEventCut.getMoveSpeed() == 0.0f) {
                        mAnmNo = 1;
                    } else if (mEventCut.getMoveSpeed() > 10.0f) {
                        mAnmNo = 5;
                    } else {
                        mAnmNo = 4;
                        break;
                    }
                    break;
                default:
                    mAnmNo = 1;
                    break;
                }
            } else {
                mAnmNo = 1;
            }
        }
    }

    if (!dComIfGp_event_runCheck()) {
        mEventCut.setAttnNoTurnFlag(false);
        mEventCut.setAttnFlag(false);
        m72C = 0;
    }

    eventOrder();

    setAnm();
    setTexAnm();

    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    mAcch.CrrPos(*dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mAcch.m_gnd);

    setMtx();

    mpMorf->calc();

    if (m7C0) {
        J3DModel* model = mpMorf->getModel();
        himo3_class* himo3 = mpHimo3;
        dLib_getPosFromMtx(model->getAnmMtx(0xC), &m78C);
        himo3->setActorHang(m78C, shape_angle.y);
    }

    setCollision();
    if (mAnmNo == 4) {
        f32 fVar1 = (current.pos - old.pos).abs() / (REG12_F(6) + 10.0f);
        if (fVar1 <= 0.0f) {
            fVar1 = 0.0f;
        } else if (fVar1 >= 1.0f) {
            fVar1 = 1.0f;
        }
        fVar1 *= l_HIO.m280;
        fVar1 = cLib_minLimit(fVar1, l_HIO.m284);
        mpMorf->setPlaySpeed(fVar1);
    }

    return true;
}

/* 00003520-000035A4       .text draw_item__10daNpc_P2_cFP8J3DModelSc */
void daNpc_P2_c::draw_item(J3DModel* model, s8 jntNo) {
    g_env_light.setLightTevColorType(model, &tevStr);
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(jntNo));
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoExt_modelUpdateDL(model);
}

/* 000035A4-000035E8       .text drawDagger__10daNpc_P2_cFv */
void daNpc_P2_c::drawDagger() {
    if (m80A != 0) {
        draw_item(mpModel2BC, 0xC);
    } else {
        draw_item(mpModel2C0, 0xE);
    }
}

/* 000035E8-000036A4       .text drawHead__10daNpc_P2_cFv */
void daNpc_P2_c::drawHead() {
    g_env_light.setLightTevColorType(mpHeadModel, &tevStr);
    if (mType != 2) {
        mBtpAnm.entry(mpHeadModel->getModelData(), mBtpFrame);
        mpHeadModel->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(P2_JNT_HEAD_e));
        mDoExt_modelUpdateDL(mpHeadModel);
        mBtpAnm.remove(mpHeadModel->getModelData());
    } else {
        mpHeadModel->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(P2_JNT_HEAD_e));
        mDoExt_modelUpdateDL(mpHeadModel);
    }
}

/* 000036A4-000037A8       .text drawShadow__10daNpc_P2_cFv */
void daNpc_P2_c::drawShadow() {
    cXyz pos;
    pos.set(current.pos.x, current.pos.y + 150.0f + REG8_F(18), current.pos.z);
    mShadowId = dComIfGd_setShadow(
        mShadowId, 1, mpMorf->getModel(), &pos,
        800.0f + REG8_F(19), 20.0f, current.pos.y,
        mAcch.GetGroundH(), mAcch.m_gnd, &tevStr,
        0, 1.0f, dDlst_shadowControl_c::getSimpleTex()
    );
    if (mShadowId != 0) {
        dComIfGd_addRealShadow(mShadowId, mpHeadModel);
        if (m80A != 0) {
            dComIfGd_addRealShadow(mShadowId, mpModel2BC);
        } else {
            dComIfGd_addRealShadow(mShadowId, mpModel2C0);
        }
    }
}

/* 000037A8-00003890       .text drawP2a__10daNpc_P2_cFv */
void daNpc_P2_c::drawP2a() {
    J3DModel* model = mpMorf->getModel();
    g_env_light.setLightTevColorType(model, &tevStr);
    mpMorf->entryDL();
    drawDagger();
    drawHead();
    if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
        draw_item(mpModel2C4, 0xC);
    }
    drawShadow();
    if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
        if (mShadowId != 0) {
            dComIfGd_addRealShadow(mShadowId, mpModel2C4);
        }
    }
    dSnap_RegistFig(DSNAP_TYPE_UNK76, this, 1.0f, 1.0f, 1.0f);
}

/* 00003890-00003948       .text drawP2b__10daNpc_P2_cFv */
void daNpc_P2_c::drawP2b() {
    J3DModel* model = mpMorf->getModel();
    g_env_light.setLightTevColorType(model, &tevStr);
    J3DMaterialTable* pBmt = (J3DMaterialTable*)dComIfG_getObjectRes(m_arc_name, l_bmt_ix_tbl[mType]);
    mpMorf->entryDL(pBmt);
    drawDagger();
    drawHead();
    drawShadow();
    dSnap_RegistFig(DSNAP_TYPE_UNK77, this, 1.0f, 1.0f, 1.0f);
}

/* 00003948-00003A64       .text drawP2c__10daNpc_P2_cFv */
void daNpc_P2_c::drawP2c() {
    J3DModel* model = mpMorf->getModel();
    g_env_light.setLightTevColorType(model, &tevStr);
    J3DMaterialTable* pBmt = (J3DMaterialTable*)dComIfG_getObjectRes(m_arc_name, l_bmt_ix_tbl[mType]);
    mpMorf->entryDL(pBmt);
    drawDagger();
    drawHead();
    g_env_light.setLightTevColorType(mpMorf2->getModel(), &tevStr);
    mpMorf2->getModel()->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(P2_JNT_BACKBONE_e));
    mpMorf2->updateDL();
    drawShadow();
    if (mShadowId != 0) {
        dComIfGd_addRealShadow(mShadowId, mpMorf2->getModel());
    }
    dSnap_RegistFig(DSNAP_TYPE_UNK78, this, 1.0f, 1.0f, 1.0f);
}

/* 00003A64-00003AF0       .text _draw__10daNpc_P2_cFv */
bool daNpc_P2_c::_draw() {
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    switch (mType) {
    case 0:
        drawP2a();
        break;
    case 1:
        drawP2b();
        break;
    case 2:
        drawP2c();
        break;
    }
    return true;
}

/* 00003AF0-00003B10       .text CreateHeap_CB__FP10fopAc_ac_c */
static BOOL CreateHeap_CB(fopAc_ac_c* i_this) {
    return ((daNpc_P2_c*)i_this)->_createHeap();
}

/* 00003B10-00003B58       .text getArg__10daNpc_P2_cFv */
void daNpc_P2_c::getArg() {
    u32 param = fopAcM_GetParam(this);
    mType = param & 3;
    m291 = (param >> 2) & 0xFF;
    mSwitchNo = (param >> 10) & 0xFF;
    if (mType == 3) {
        mType = 0;
    }
    if (m291 == 0xFF) {
        m291 = 0;
    }
}

/* 00003B58-00004034       .text _createHeap__10daNpc_P2_cFv */
BOOL daNpc_P2_c::_createHeap() {
    static const u32 head_bdl_tbl[3] = {
        dRes_INDEX_P2_BDL_P2HEAD01_e,
        dRes_INDEX_P2_BDL_P2HEAD02_e,
        dRes_INDEX_P2_BDL_P2HEAD03_e,
    };
    static const u8 head_tex_tbl[2] = {0, 1};
    J3DModelData* headModelData;
    J3DModelData* daggerModelData;
    J3DModelData* daggerGripModelData;
    J3DModelData* bookModelData;
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_P2_BDL_P2_e));
    JUT_ASSERT(DEMO_SELECT(0x9F2, 0x9F1), modelData != 0);
    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL, NULL, -1, 1.0f, 0, -1, 1, NULL, 0x80000, 0x15021222);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
#if VERSION > VERSION_DEMO
        mpMorf = NULL;
#endif
        return FALSE;
    } else {
        headModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, head_bdl_tbl[mType]));
        JUT_ASSERT(DEMO_SELECT(0xA08, 0xA0A), headModelData != 0);
        if (mType != 2) {
            mpHeadModel = mDoExt_J3DModel__create(headModelData, 0x80000, 0x11020022);
            if (mpHeadModel == NULL) {
                return FALSE;
            }
            m7D0 = head_tex_tbl[mType];
            if (initTexPatternAnm(false) == 0) {
                return FALSE;
            }
        } else {
            mpHeadModel = mDoExt_J3DModel__create(headModelData, 0, 0x11020203);
            if (mpHeadModel == NULL) {
                return FALSE;
            }
        }
        daggerModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_P2_BDL_P2DAGGER_e));
        JUT_ASSERT(DEMO_SELECT(0xA25, 0xA27), daggerModelData != 0);
        mpModel2BC = mDoExt_J3DModel__create(daggerModelData, 0, 0x11020203);
        if (mpModel2BC == NULL) {
            return FALSE;
        }
        daggerGripModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_P2_BDL_P2_DAGGERGRIP_e));
        JUT_ASSERT(DEMO_SELECT(0xA2C, 0xA2E), daggerModelData != 0);
        mpModel2C0 = mDoExt_J3DModel__create(daggerGripModelData, 0, 0x11020203);
        if (mpModel2C0 == NULL) {
            return FALSE;
        }
        if (mType == 2) {
            bookModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_P2_BDL_P2BOOK_e));
            JUT_ASSERT(DEMO_SELECT(0xA34, 0xA36), bookModelData != 0);
            mpMorf2 = new mDoExt_McaMorf(
                bookModelData, NULL, NULL,
                static_cast<J3DAnmTransform*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_P2_BCK_P2BOOK_WAIT02_e)),
                0, 1.0f, 0, -1, 1, NULL, 0, 0x11020203
            );
            if (mpMorf2 == NULL || mpMorf2->getModel() == NULL) {
                return FALSE;
            }
        }
        if (mType == 0) {
            J3DModelData* telescopeModelData = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arc_name, dRes_INDEX_P2_BDL_P2_TELESCOPE_e));
            JUT_ASSERT(DEMO_SELECT(0xA47, 0xA49), telescopeModelData != 0);
            mpModel2C4 = mDoExt_J3DModel__create(telescopeModelData, 0, 0x11020203);
            if (mpModel2C4 == NULL) {
                return FALSE;
            }
        }
        mpMorf->getModel()->setUserArea((u32)this);
        J3DJointTree& tree = modelData->getJointTree();
        for (u16 i = 0; i < tree.getJointNum(); i++) {
            if (i == 4 || i == 2) {
                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
            }
        }
        return TRUE;
    }
}

/* 00004034-0000476C       .text createInit__10daNpc_P2_cFv */
void daNpc_P2_c::createInit() {
    static char* actor_name_table[] = {"P2a", "P2b", "P2c"};
    if (mType == 1 && !strcmp(dComIfGp_getStartStageName(), "Asoko")) {
        fopAcM_OnStatus(this, 0x4000);
    }

    mpMorf->calc();
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    mAcchCir.SetWall(30.0f, 0.0f);
    mAcch.Set(
        fopAcM_GetPosition_p(this),
        fopAcM_GetOldPosition_p(this),
        this,
        1,
        &mAcchCir,
        fopAcM_GetSpeed_p(this),
        fopAcM_GetAngle_p(this),
        fopAcM_GetShapeAngle_p(this)
    );

    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA9;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;

    if (mType == 1) {
        if (m291 == 0) {
            if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808) &&
                dComIfGs_isEventBit(dSv_event_flag_c::UNK_0720))
            {
                current.pos = l_HIO.mChild[mType].m30;
            }
        } else if (m291 == 1) {
            if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0F02) &&
                dComIfGs_isEventBit(dSv_event_flag_c::UNK_1A04))
            {
                current.pos = l_HIO.mChild[mType].m3C;
            }
        }
    }

    m6EC = current.angle;

    attention_info.flags = fopAc_Attn_LOCKON_TALK_e | fopAc_Attn_ACTION_SPEAK_e;
    gravity = -9.0f;

    mEventCut.setActorInfo(actor_name_table[mType], this);
    if (mType == 1) {
        if (!strcmp(dComIfGp_getStartStageName(), "Asoko")) {
            if (m291 == 0) {
                if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
                    setAction(&daNpc_P2_c::intro_action, NULL);
                } else {
                    setAction(&daNpc_P2_c::wait_action, NULL);
                }
            } else if (m291 == 1) {
                if (!dComIfGs_isEventBit(dSv_event_flag_c::UNK_0F02)) {
                    setAction(&daNpc_P2_c::intro_action, NULL);
                } else {
                    setAction(&daNpc_P2_c::wait_action, NULL);
                }
            }
        } else {
            setAction(&daNpc_P2_c::wait_action, NULL);
        }
    } else {
        setAction(&daNpc_P2_c::wait_action, NULL);
    }

    mAttnPos = current.pos;
    mEyePos = current.pos;

    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);

    m7D1 = 1;
    mAnmNo = 1;
    m7D4 = 0;

    setTexAnm();
    setAnm();

    if (parentActorID != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = fopAcM_SearchByID(parentActorID);
        if (actor != NULL && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_Obj_Pirateship_e) {
            if (mType == 0 && !dComIfGs_isEventBit(dSv_event_flag_c::UNK_0808)) {
                m748 = home.angle.y - actor->home.angle.y;
                mJnt.onHeadLock();
                mJnt.onBackBoneLock();
            } else if (mType == 2) {
                m748 = home.angle.y - actor->home.angle.y;
            }
        }
    }

    m74C = (s16)(cM_rndF(100.0f) + 200.0f);
    m7DC.setall(0.0f);
}

/* 0000476C-0000492C       .text _create__10daNpc_P2_cFv */
cPhs_State daNpc_P2_c::_create() {
    fopAcM_ct_Retail(this, daNpc_P2_c);

    cPhs_State phase_state = dComIfG_resLoad(&mPhs, m_arc_name);
    if (phase_state == cPhs_COMPLEATE_e) {
        fopAcM_ct_Demo(this, daNpc_P2_c);
        getArg();
        if (!fopAcM_entrySolidHeap(this, CreateHeap_CB, m_heapsize[mType])) {
            return cPhs_ERROR_e;
        }
        createInit();
    }
    return phase_state;
}

/* 00004CDC-00004D54       .text _delete__10daNpc_P2_cFv */
bool daNpc_P2_c::_delete() {
    dComIfG_resDelete(&mPhs, m_arc_name);
    mSmoke.remove();
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    if (mpMorf2 != NULL) {
        mpMorf2->stopZelAnime();
    }
#else
    mDoAud_seDeleteObject(&mSoundPos);
    if (heap != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return true;
}

/* 00004D54-00004D74       .text daNpc_P2Create__FPv */
static cPhs_State daNpc_P2Create(void* i_this) {
    return ((daNpc_P2_c*)i_this)->_create();
}

/* 00004D74-00004D98       .text daNpc_P2Delete__FPv */
static BOOL daNpc_P2Delete(void* i_this) {
    return ((daNpc_P2_c*)i_this)->_delete();
}

/* 00004D98-00004DBC       .text daNpc_P2Execute__FPv */
static BOOL daNpc_P2Execute(void* i_this) {
    return ((daNpc_P2_c*)i_this)->_execute();
}

/* 00004DBC-00004DE0       .text daNpc_P2Draw__FPv */
static BOOL daNpc_P2Draw(void* i_this) {
    return ((daNpc_P2_c*)i_this)->_draw();
}

/* 00004DE0-00004DE8       .text daNpc_P2IsDelete__FPv */
static BOOL daNpc_P2IsDelete(void*) {
    return TRUE;
}


static actor_method_class daNpc_P2MethodTable = {
    (process_method_func)daNpc_P2Create,
    (process_method_func)daNpc_P2Delete,
    (process_method_func)daNpc_P2Execute,
    (process_method_func)daNpc_P2IsDelete,
    (process_method_func)daNpc_P2Draw,
};

actor_process_profile_definition g_profile_NPC_P2 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_P2_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_P2_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_P2_e,
    /* Actor SubMtd */ &daNpc_P2MethodTable,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_CULL_e | fopAcStts_FREEZE_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
