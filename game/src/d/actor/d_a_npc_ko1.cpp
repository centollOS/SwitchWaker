/**
 * d_a_npc_ko1.cpp
 * NPC - Joel & Zill (Outset Island)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_ko1.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_kankyo.h"
#include "d/d_snap.h"
#include "d/d_particle_name.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_counter.h"
#include "res/Object/Ko.h"
#include <string.h>

static daNpc_Ko1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;

/* 000000EC-00000108       .text __ct__20daNpc_Ko1_childHIO_cFv */
daNpc_Ko1_childHIO_c::daNpc_Ko1_childHIO_c() {}

/* 00000150-0000020C       .text __ct__15daNpc_Ko1_HIO_cFv */
daNpc_Ko1_HIO_c::daNpc_Ko1_HIO_c() {
    static daNpc_Ko1_childHIO_c::hio_prm_c a_prm_tbl[] = {
        {
            0x2000, 0x1F40, (s16)0xE0C0, (s16)0xE0C0,
            0x0000, 0x1F40, 0x0000, (s16)0xE0C0,
            0x07D0, 0x0640,
            90.0f,
            0,
            4, 0x0800, 0,
            0.9f, 1.0f, 0.5f, 21.0f, 0.1f, 10.0f, 0.3f, 40.0f,
            80.0f, 200.0f, 300.0f, 150.0f, 1100.0f, 1500.0f,
        },
        {
            0x2000, 0x1F40, (s16)0xE0C0, (s16)0xE0C0,
            0x0000, 0x1F40, 0x0000, (s16)0xE0C0,
            0x07D0, 0x0640,
            90.0f,
            0,
            6, 0x0800, 0,
            0.3f, 6.0f, 0.4f, 21.0f, 0.1f, 12.0f, 1.5f, 100.0f,
            0.0f, 350.0f, 500.0f, 150.0f, 0.0f, 0.0f,
        },
    };
    for (int i = 0; i < 2; i++) {
        mChild[i].mIndex = i;
        memcpy(&mChild[i].mPrm, &a_prm_tbl[i], sizeof(daNpc_Ko1_childHIO_c::hio_prm_c));
    }
    mNo = -1;
    mSelected = -1;
}

inline daNpc_Ko1_childHIO_c::~daNpc_Ko1_childHIO_c() {}

/* 00000268-000002E0       .text searchActor_Ko_Hna__FPvPv */
static void* searchActor_Ko_Hna(void* actor, void*) {
    if (l_check_wrk < 20 && fopAcM_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_NPC_KO1_e) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

/* 000002E0-00000358       .text searchActor_Ko_Bou__FPvPv */
static void* searchActor_Ko_Bou(void* actor, void*) {
    if (l_check_wrk < 20 && fopAcM_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_NPC_KO2_e) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

/* 00000358-000003D0       .text searchActor_Ob__FPvPv */
static void* searchActor_Ob(void* actor, void*) {
    if (l_check_wrk < 20 && fopAcM_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_NPC_OB1_e) {
        l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
        l_check_wrk++;
    }
    return NULL;
}

/* 000003D0-0000041C       .text nodeCallBack_Hed__FP7J3DNodei */
static BOOL nodeCallBack_Hed(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ko1_c* i_this = (daNpc_Ko1_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->nodeHedControl(node, model);
        }
    }
    return TRUE;
}

/* 0000041C-00000488       .text nodeHedControl__11daNpc_Ko1_cFP7J3DNodeP8J3DModel */
void daNpc_Ko1_c::nodeHedControl(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_2_jnt_num) {
        cMtx_copy(mDoMtx_stack_c::get(), mHeadMtx);
    }
}

/* 00000488-000004D4       .text nodeCallBack_Bln__FP7J3DNodei */
static BOOL nodeCallBack_Bln(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ko1_c* i_this = (daNpc_Ko1_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->nodeBlnControl(node, model);
        }
    }
    return TRUE;
}

/* 000004D4-00000554       .text nodeBlnControl__11daNpc_Ko1_cFP7J3DNodeP8J3DModel */
void daNpc_Ko1_c::nodeBlnControl(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_bln_loc_jnt_num) {
        cMtx_copy(mHeadMtx, J3DSys::mCurrentMtx);
        model->setAnmMtx(joint, mHeadMtx);
    }
}

/* 00000554-000005A0       .text nodeCallBack_Ko1__FP7J3DNodei */
static BOOL nodeCallBack_Ko1(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ko1_c* i_this = (daNpc_Ko1_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->nodeKo1Control(node, model);
        }
    }
    return TRUE;
}

/* 000005A0-000006E8       .text nodeKo1Control__11daNpc_Ko1_cFP7J3DNodeP8J3DModel */
void daNpc_Ko1_c::nodeKo1Control(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(18.0f, 20.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
        mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyePos);
    }
    if (joint == m_bbone_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
        mDoMtx_stack_c::YrotM(m_jnt.getBackbone_x());
    }
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000724-000007CC       .text init_HNA_0__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_HNA_0() {
    if (dComIfGs_isEventBit(0x2A80)) {
        if (!dComIfGs_isEventBit(0xE20)) {
            mpBalloonMorf = NULL;
            set_action(&daNpc_Ko1_c::hana_action1, NULL);
            return true;
        }
    }
    return false;
}

/* 000007CC-00000860       .text init_HNA_1__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_HNA_1() {
    if (dComIfGs_isEventBit(0xE20)) {
        mpBalloonMorf = NULL;
        actor_status &= ~fopAcStts_NOCULLEXEC_e;
        set_action(&daNpc_Ko1_c::hana_action2, NULL);
        return true;
    }
    return false;
}

/* 00000860-000008E8       .text init_HNA_2__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_HNA_2() {
    if (!dComIfGs_isEventBit(0x2A80)) {
        mpBalloonMorf = NULL;
        set_action(&daNpc_Ko1_c::hana_action3, NULL);
        return true;
    }
    return false;
}

/* 000008E8-00000994       .text init_HNA_3__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_HNA_3() {
    if (dComIfGs_isEventBit(0x520)) {
        if (dKy_daynight_check() == 0) {
            actor_status &= ~fopAcStts_NOCULLEXEC_e;
            actor_status |= fopAcStts_UNK4000_e;
            mpBalloonMorf = NULL;
            set_action(&daNpc_Ko1_c::hana_action4, NULL);
            return true;
        }
    }
    return false;
}

/* 00000994-00000A20       .text init_HNA_4__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_HNA_4() {
    if (dComIfGs_isEventBit(0x520)) {
        if (dKy_daynight_check() == 1) {
            set_action(&daNpc_Ko1_c::hana_action5, NULL);
            return true;
        }
    }
    return false;
}

/* 00000A20-00000AAC       .text init_BOU_0__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_BOU_0() {
    if (!dComIfGs_isEventBit(0xE20)) {
        set_action(&daNpc_Ko1_c::wait_action1, NULL);
        actor_status &= ~fopAcStts_NOCULLEXEC_e;
        return true;
    }
    return false;
}

/* 00000AAC-00000B38       .text init_BOU_1__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_BOU_1() {
    if (dComIfGs_isEventBit(0xE20)) {
        set_action(&daNpc_Ko1_c::wait_action2, NULL);
        actor_status &= ~fopAcStts_NOCULLEXEC_e;
        return true;
    }
    return false;
}

/* 00000B38-00000BE4       .text init_BOU_2__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_BOU_2() {
    if (dComIfGs_isEventBit(0x520)) {
        if (dKy_daynight_check() == 0) {
            actor_status &= ~fopAcStts_NOCULLEXEC_e;
            actor_status |= fopAcStts_UNK4000_e;
            set_action(&daNpc_Ko1_c::wait_action3, NULL);
            mpItemModel = NULL;
            return true;
        }
    }
    return false;
}

/* 00000BE4-00000C78       .text init_BOU_3__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::init_BOU_3() {
    if (dComIfGs_isEventBit(0x520)) {
        if (dKy_daynight_check() == 1) {
            set_action(&daNpc_Ko1_c::wait_action4, NULL);
            mpItemModel = NULL;
            return true;
        }
    }
    return false;
}

/* 00000C78-00000F14       .text createInit__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::createInit() {
    static char* a_staff_tbl[] = {
        "Ko1", "Ko1", "Ko1", "Ko1", "Ko1",
        "Ko2", "Ko2", "Ko2", "Ko2",
    };

    attention_info.flags = 0xA;
    switch (mType) {
    case 0:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA9;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;
        break;
    case 1:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA7;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;
        break;
    }

    gravity = -4.5f;
    mHomeAngle = current.angle;
    mHomePos = current.pos;
    mTargetPos = mHomePos;
    mLookPos = mTargetPos;

    int weight = 0xFF;
    u8 pathIdx = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (pathIdx != 0xFF) {
        mPath.setInf(pathIdx, fopAcM_GetRoomNo(this), 1);
        if (mPath.isPath() == false) {
            return false;
        }
        actor_status &= ~fopAcStts_NOCULLEXEC_e;
        weight = 0xF0;
    }

    mCut.setActorInfo2(a_staff_tbl[mStaff], this);
    mAnmIndex = 0xE;

    bool result;
    switch (mStaff) {
    case 0:
        result = init_HNA_0();
        break;
    case 1:
        result = init_HNA_1();
        weight = 0xF0;
        break;
    case 2:
        result = init_HNA_2();
        weight = 0xF0;
        break;
    case 3:
        result = init_HNA_3();
        break;
    case 4:
        result = init_HNA_4();
        break;
    case 5:
        result = init_BOU_0();
        break;
    case 6:
        result = init_BOU_1();
        weight = 0xF0;
        break;
    case 7:
        result = init_BOU_2();
        break;
    case 8:
        result = init_BOU_3();
        break;
    default:
        result = false;
        break;
    }
    if (result) {
        mModelAngle = current.angle;
        shape_angle = mModelAngle;
    } else {
        return false;
    }
    mStts.Init(weight, 0xFF, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    mpHeadMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

/* 00000F14-000011C4       .text setMtx__11daNpc_Ko1_cFb */
void daNpc_Ko1_c::setMtx(bool param) {
    if (mDemo == 0) {
        u32 sound = 0;
        plyTexPttrnAnm();
        if (mObjAcch.ChkGroundHit()) {
            sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
        }
        mAnmEnded = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        if (mpMorf->getFrame() < mLastFrame) {
            mAnmEnded = 1;
        }
        mLastFrame = mpMorf->getFrame();
        if (mAnmIndex == 4) {
            if (mpMorf->checkFrame(10.0f)) {
                fopAcM_seStart(this, JA_SE_CM_GAKI_HANASUSURI, 0);
            }
        }
        mpHeadMorf->play(&eyePos, 0, 0);
        if (mpBalloonMorf != NULL && m865 < 2) {
            mpBalloonMorf->setFrame(mpMorf->getFrame());
        }
        m86C = mObjAcch.ChkGroundHit();
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(mModelAngle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    mpHeadMorf->getModel()->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
    mpHeadMorf->calc();
    if (mpBalloonMorf != NULL) {
        mpBalloonMorf->calc();
    }
    if (mpItemModel != NULL) {
        mpItemModel->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(m_armR2_jnt_num));
        mpItemModel->calc();
    }
    setAttention(param);
}

/* 000011C4-000011D8       .text anmNum_toResID__11daNpc_Ko1_cFi */
int daNpc_Ko1_c::anmNum_toResID(int idx) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_KO_BCK_KO_WAIT01_e,
        dRes_ID_KO_BCK_KO_RUN01_e,
        dRes_ID_KO_BCK_KO_WALK01_e,
        dRes_ID_KO_BCK_KO_TALK01_e,
        dRes_ID_KO_BCK_KO_SUSURI_e,
        dRes_ID_KO_BCK_KO_BUTUKARI_e,
        dRes_ID_KO_BCK_KO_WAIT02_e,
        dRes_ID_KO_BCK_KO_SLEEP01_e,
        dRes_ID_KO_BCK_KO_SLEEP02_e,
        dRes_ID_KO_BCK_KO_SLEEP03_e,
        dRes_ID_KO_BCK_KO_SLEEP04_e,
        dRes_ID_KO_BCK_KO_OISI_e,
        dRes_ID_KO_BCK_KO_BANG_e,
        dRes_ID_KO_BCK_KO_OHOHO_e,
    };
    return a_bck_resID_tbl[idx];
}

/* 000011D8-00001200       .text headAnmNum_toResID__11daNpc_Ko1_cFi */
int daNpc_Ko1_c::headAnmNum_toResID(int idx) {
    if (mType == 1) {
        return dRes_ID_KO_BCK_KOHEAD02_WAIT01_e;
    }
    static const int a_bck_resID_tbl[] = {
        dRes_ID_KO_BCK_KOHEAD01_WAIT01_e,
        dRes_ID_KO_BCK_KOHEAD01_RUN01_e,
        dRes_ID_KO_BCK_KOHEAD01_WALK01_e,
        dRes_ID_KO_BCK_KOHEAD01_TALK01_e,
        dRes_ID_KO_BCK_KOHEAD01_SUSURI_e,
        dRes_ID_KO_BCK_KOHEAD01_BUTUKARI_e,
        dRes_ID_KO_BCK_KOHEAD01_WAIT01_e,
        dRes_ID_KO_BCK_KOHEAD01_WAIT01_e,
        dRes_ID_KO_BCK_KOHEAD01_WAIT01_e,
        dRes_ID_KO_BCK_KOHEAD01_SLEEP03_e,
        dRes_ID_KO_BCK_KOHEAD01_SLEEP04_e,
        dRes_ID_KO_BCK_KOHEAD01_OISI_e,
        dRes_ID_KO_BCK_KOHEAD01_BANG_e,
        dRes_ID_KO_BCK_KOHEAD01_WAIT01_e,
    };
    return a_bck_resID_tbl[idx];
}

/* 00001200-00001214       .text balloon_anmNum_toResID__11daNpc_Ko1_cFi */
int daNpc_Ko1_c::balloon_anmNum_toResID(int idx) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_KO_BCK_BALLOON_SLEEP04_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
        dRes_ID_KO_BCK_BALLOON_BANG_e,
    };
    return a_bck_resID_tbl[idx];
}

/* 00001214-0000129C       .text btpNum_toResID__11daNpc_Ko1_cFi */
int daNpc_Ko1_c::btpNum_toResID(int idx) {
    static const int a_btp_resID_tbl[] = {
        dRes_ID_KO_BTP_KOHEAD01_e,
        dRes_ID_KO_BTP_KOHEAD01_SUSURI_e,
        dRes_ID_KO_BTP_KOHEAD01_SLEEP_e,
        dRes_ID_KO_BTP_KOHEAD01_SLEEP04_e,
    };
    int btp = a_btp_resID_tbl[idx];
    switch (btp) {
    case dRes_ID_KO_BTP_KOHEAD01_e:
        switch (mType) {
        case 0:
            return dRes_ID_KO_BTP_KOHEAD01_e;
        case 1:
            return dRes_ID_KO_BTP_KOHEAD02_e;
        }
        break;
    case dRes_ID_KO_BTP_KOHEAD01_SLEEP_e:
        switch (mType) {
        case 0:
            return dRes_ID_KO_BTP_KOHEAD01_SLEEP_e;
        case 1:
            return dRes_ID_KO_BTP_KOHEAD02_SLEEP_e;
        }
        break;
    default:
        return btp;
    }
}

/* 0000129C-000013AC       .text setBtp__11daNpc_Ko1_cFbi */
bool daNpc_Ko1_c::setBtp(bool modify, int index) {
    J3DModelData* data = mpHeadMorf->getModel()->getModelData();
    m_hed_tex_pttrn = (J3DAnmTexPattern*)dComIfG_getObjectIDRes("Ko", btpNum_toResID(index));
    JUT_ASSERT(0x3AA, m_hed_tex_pttrn != 0);
    if (mBtp.init(data, m_hed_tex_pttrn, TRUE, 2, 1.0f, 0, -1, modify, FALSE) != 0) {
        mBtpFrame = 0;
        mBlinkTimer = 0;
        return true;
    }
    return false;
}

/* 000013AC-000013D4       .text iniTexPttrnAnm__11daNpc_Ko1_cFb */
bool daNpc_Ko1_c::iniTexPttrnAnm(bool modify) {
    return setBtp(modify, mTexIndex);
}

/* 000013D4-00001480       .text plyTexPttrnAnm__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::plyTexPttrnAnm() {
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

/* 00001480-000014BC       .text setAnm_tex__11daNpc_Ko1_cFSc */
void daNpc_Ko1_c::setAnm_tex(s8 idx) {
    if (mTexIndex != idx) {
        mTexIndex = idx;
        iniTexPttrnAnm(true);
    }
}

/* 000014BC-000015B4       .text setAnm_anm__11daNpc_Ko1_cFPQ211daNpc_Ko1_c9anm_prm_c */
BOOL daNpc_Ko1_c::setAnm_anm(daNpc_Ko1_c::anm_prm_c* prm) {
    if (mAnmIndex == prm->mAnm) {
        return TRUE;
    }
    mAnmIndex = prm->mAnm;
    int res = anmNum_toResID(mAnmIndex);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, res, -1, "Ko");
    int headRes = headAnmNum_toResID(mAnmIndex);
    dNpc_setAnmIDRes(mpHeadMorf, prm->mLoop, prm->mMorf, prm->mSpeed, headRes, -1, "Ko");
    mLastFrame = mAnmRepeat = mAnmEnded = 0;
    return TRUE;
}

/* 000015B4-00001674       .text set_balloonAnm_anm__11daNpc_Ko1_cFPQ211daNpc_Ko1_c9anm_prm_c */
BOOL daNpc_Ko1_c::set_balloonAnm_anm(daNpc_Ko1_c::anm_prm_c* prm) {
    if (mBalloonAnmIndex == prm->mAnm) {
        return TRUE;
    }
    mBalloonAnmIndex = prm->mAnm;
    int res = balloon_anmNum_toResID(mBalloonAnmIndex);
    dNpc_setAnmIDRes(mpBalloonMorf, prm->mLoop, prm->mMorf, prm->mSpeed, res, -1, "Ko");
    mBalloonLastFrame = mBalloonAnmRepeat = mBalloonAnmEnded = 0;
    return TRUE;
}

/* 00001674-000016A4       .text set_balloonAnm_NUM__11daNpc_Ko1_cFi */
void daNpc_Ko1_c::set_balloonAnm_NUM(int idx) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, -1, 0.0f, 0.0f, 2, 0},
        {1, -1, 0.0f, 0.0f, 0, 0},
    };
    set_balloonAnm_anm(&a_anm_prm_tbl[idx]);
}

/* 000016A4-00001710       .text setAnm_NUM__11daNpc_Ko1_cFii */
void daNpc_Ko1_c::setAnm_NUM(int index, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2, 0},
        {1, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {3, 0, 8.0f, 1.0f, 2, 0},
        {4, 1, 8.0f, 1.0f, 2, 0},
        {5, 0, 8.0f, 1.0f, 0, 0},
        {6, 0, 8.0f, 1.0f, 2, 0},
        {7, 2, 8.0f, 1.0f, 2, 0},
        {8, 2, 8.0f, 1.0f, 2, 0},
        {9, 2, 8.0f, 1.0f, 2, 0},
        {0xA, 3, 8.0f, 1.0f, 2, 0},
        {0xB, 0, 8.0f, 1.0f, 2, 0},
        {0xC, 2, 8.0f, 1.0f, 0, 0},
        {0xD, 0, 8.0f, 1.0f, 2, 0},
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[index].mTex);
    }
    setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 00001710-00001790       .text setAnm__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, -1, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {6, 0, 8.0f, 1.0f, 2, 0},
        {-1, -1, 0.0f, 0.0f, -1, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {4, 1, 8.0f, 1.0f, 2, 0},
        {1, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {5, 0, 8.0f, 1.0f, 0, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {1, 0, 8.0f, 1.0f, 2, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {5, 0, 8.0f, 1.0f, 0, 0},
        {1, 0, 8.0f, 1.0f, 2, 0},
        {5, 0, 8.0f, 1.0f, 0, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {-1, -1, 0.0f, 0.0f, -1, 0},
        {-1, -1, 0.0f, 0.0f, -1, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {1, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {5, 0, 8.0f, 1.0f, 0, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
        {7, 2, 8.0f, 1.0f, 2, 0},
        {9, 2, 8.0f, 1.0f, 2, 0},
        {0, 0, 8.0f, 1.0f, 2, 0},
    };
    if (a_anm_prm_tbl[mState].mTex >= 0) {
        setAnm_tex(a_anm_prm_tbl[mState].mTex);
    }
    if (a_anm_prm_tbl[mState].mAnm >= 0) {
        setAnm_anm(&a_anm_prm_tbl[mState]);
    }
    return TRUE;
}

/* 00001790-000017A4       .text setPlaySpd__11daNpc_Ko1_cFf */
void daNpc_Ko1_c::setPlaySpd(float speed) {
    mpHeadMorf->setPlaySpeed(speed);
    mpMorf->setPlaySpeed(speed);
}

/* 000017A4-000017DC       .text chg_anmTag__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::chg_anmTag() {
    switch (mAnmTag) {
    case 0x0A:
        setAnm_NUM(4, 1);
        break;
    }
}

/* 000017DC-00001824       .text control_anmTag__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::control_anmTag() {
    switch (mAnmTag) {
    case 0x0A:
        if (mAnmEnded) {
            mAnmTag = 0xFF;
            setAnm_ATR(1);
        }
        break;
    }
}

/* 00001824-000018D8       .text chg_anmAtr__11daNpc_Ko1_cFUc */
void daNpc_Ko1_c::chg_anmAtr(u8 attr) {
    if (mPairedMsgNo != 0xFFFFFFFF) {
        switch (mPairedMsgNo) {
        case 0xB03:
        case 0xB0A:
        case 0xB64:
        case 0xB65:
            mLookMode = 1;
            mNoTurn = false;
            m_jnt.setTrn();
            break;
        }
    }
    if (attr >= 0xD || attr == mAnmAttr) {
        return;
    }
    mAnmAttr = attr;
    if (mAnmAttr == 0xB) {
        mLookMode = 1;
        mNoTurn = false;
        m_jnt.setTrn();
    }
    setAnm_ATR(1);
}

/* 000018D8-0000192C       .text control_anmAtr__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::control_anmAtr() {
    switch (mAnmAttr) {
    case 7:
    case 8:
    case 9:
    case 10:
        break;
    case 0xB:
        if (mAnmEnded) {
            mAnmAttr = 0;
            setAnm_NUM(0, 1);
        }
        break;
    case 0xC:
        break;
    default:
        break;
    }
}

/* 0000192C-00001994       .text setAnm_ATR__11daNpc_Ko1_cFi */
void daNpc_Ko1_c::setAnm_ATR(int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2, 0},
        {1, 0, 8.0f, 1.0f, 2, 0},
        {2, 0, 8.0f, 1.0f, 2, 0},
        {3, 0, 8.0f, 1.0f, 2, 0},
        {4, 1, 8.0f, 1.0f, 2, 0},
        {5, 0, 8.0f, 1.0f, 0, 0},
        {6, 0, 8.0f, 1.0f, 2, 0},
        {7, 0, 8.0f, 1.0f, 2, 0},
        {8, 0, 8.0f, 1.0f, 2, 0},
        {9, 0, 8.0f, 1.0f, 2, 0},
        {0xA, 3, 8.0f, 1.0f, 2, 0},
        {0xB, 0, 8.0f, 1.0f, 2, 0},
        {0xD, 0, 8.0f, 1.0f, 2, 0},
    };
    if (tex) {
        setAnm_tex(a_anm_prm_tbl[mAnmAttr].mTex);
    }
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00001994-00001A9C       .text anmAtr__11daNpc_Ko1_cFUs */
void daNpc_Ko1_c::anmAtr(u16 status) {
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

/* 00001A9C-00001AEC       .text eventOrder__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::eventOrder() {
    if (mEventOrder == 1 || mEventOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mEventOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 00001AEC-00001B2C       .text checkOrder__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        return;
    }
    if (eventInfo.getCommand() == dEvtCmd_INTALK_e && (mEventOrder == 1 || mEventOrder == 2)) {
        mEventOrder = 0;
        mTalking = true;
    }
}

/* 00001B2C-00001BCC       .text chk_talk__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::chk_talk() {
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

/* 00001BCC-00001E0C       .text chk_manzai_1__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::chk_manzai_1() {
    int ready = 0;
    for (int i = 0; i < mPartnerNum; i++) {
        fopAc_ac_c* a_actor = searchByID(mPartnerId[i]);
        JUT_ASSERT(0x582, 0 != a_actor);
        daNpc_Ko1_c* npc = (daNpc_Ko1_c*)a_actor;
        if (npc->field_0x6bc[0] == 2) {
            ready++;
        } else {
            npc->actor_status |= fopAcStts_UNK4000_e;
            npc->mManzaiPartnerId = fopAcM_GetID(this);
            npc->field_0x6bc[0] = 1;
        }
    }
    bool result = ready == mPartnerNum;
    if (result) {
        fopAc_ac_c* partner = searchByID(mPartnerId[0]);
        switch (mStaff) {
        case 1:
            dComIfGp_setMesgCameraInfoActor(partner, this, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            break;
        case 3:
            dComIfGp_setMesgCameraInfoActor(partner, searchByID(mPartnerId[1]), this, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            break;
        case 6:
            dComIfGp_setMesgCameraInfoActor(this, partner, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            break;
        case 7:
            dComIfGp_setMesgCameraInfoActor(this, searchByID(mPartnerId[1]), partner, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            break;
        }
        field_0x6bc[0] = 2;
    }
    return result;
}

/* 00001E0C-00001E4C       .text chk_partsNotMove__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::chk_partsNotMove() {
    return mOldHeadY == m_jnt.getHead_y() && mOldBackY == m_jnt.getBackbone_y() && mOldActorY == current.angle.y;
}

/* 00001E4C-00001FFC       .text lookBack__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::lookBack() {
    mOldHeadY = m_jnt.getHead_y();
    mOldBackY = m_jnt.getBackbone_y();
    mOldActorY = current.angle.y;
    cXyz target;
    cXyz base = current.pos;
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
    case 3:
        angle = mLookAngle;
        break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mChild[mType].mPrm.mTurnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) {
        mTurnSpeed = 0;
    }
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, noTurn);
}

/* 00001FFC-00002204       .text next_msgStatus__11daNpc_Ko1_cFPUl */
u16 daNpc_Ko1_c::next_msgStatus(unsigned long* pMsg) {
    u16 result = 15;
    switch (*pMsg) {
    case 0xAF4:
        *pMsg = 0xB57;
        break;
    case 0xAF5:
        *pMsg = 0xB58;
        break;
    case 0xB59:
        *pMsg = 0xB5A;
        break;
    case 0xB5A:
        *pMsg = 0xB5B;
        break;
    case 0xB5B:
        *pMsg = 0xB5C;
        break;
    case 0xB5C:
        *pMsg = 0xB5D;
        break;
    case 0xB5D:
        *pMsg = 0xB64;
        break;
    case 0xB5E:
        *pMsg = 0xB5F;
        break;
    case 0xB5F:
        *pMsg = 0xB60;
        break;
    case 0xB60:
        *pMsg = 0xB61;
        break;
    case 0xB61:
        *pMsg = 0xB65;
        break;
    case 0xAF6:
        *pMsg = 0xAF7;
        break;
    case 0xAF7:
        *pMsg = 0xAF8;
        break;
    case 0xAF8:
        *pMsg = 0xAF9;
        break;
    case 0xAF9:
        *pMsg = 0xAFA;
        break;
    case 0xAFA:
        *pMsg = 0xAFB;
        break;
    case 0xAFB:
        if (bitCount(dComIfGs_getEventReg(0xBFFF))) {
            *pMsg = 0xAFC;
        } else {
            *pMsg = 0xAFD;
        }
        break;
    case 0xAFC:
    case 0xAFD:
        *pMsg = 0xAFE;
        break;
    case 0xAFE:
        *pMsg = 0xAFF;
        break;
    case 0xB00:
    case 0xB07:
        *pMsg = 0xB01;
        break;
    case 0xB01:
        *pMsg = 0xB02;
        break;
    case 0xB02:
        *pMsg = 0xB03;
        break;
    case 0xB03:
        *pMsg = 0xB04;
        break;
    case 0xB04:
        *pMsg = 0xB05;
        break;
    case 0xB05:
        *pMsg = 0xB06;
        break;
    case 0xB08:
        *pMsg = 0xB09;
        break;
    case 0xB09:
        *pMsg = 0xB0A;
        break;
    case 0xB0A:
        *pMsg = 0xB0B;
        break;
    case 0xB0B:
        *pMsg = 0xB0C;
        break;
    case 0xB0C:
        *pMsg = 0xB0D;
        break;
    default:
        result = 16;
        break;
    }
    return result;
}

/* 00002204-00002240       .text getMsg_HNA_0__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_HNA_0() {
    return 0xB55 + (dComIfGs_isEventBit(0x220) ? 1 : 0);
}

/* 00002240-0000227C       .text getMsg_HNA_1__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_HNA_1() {
    return 0xAF4 + (dComIfGs_isEventBit(0x240) ? 1 : 0);
}

/* 0000227C-000022B8       .text getMsg_HNA_2__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_HNA_2() {
    return 0xB62 + (dComIfGs_isEventBit(0x3101) ? 1 : 0);
}

/* 000022B8-00002318       .text getMsg_HNA_3__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_HNA_3() {
    if (dComIfGs_isEventBit(0x2C04)) {
        return m86D ? 0xB5E : 0xB5A;
    }
    return 0xB59;
}

/* 00002318-0000237C       .text getMsg_BOU_0__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_BOU_0() {
    if (dComIfGs_isEventBit(0x104)) {
        return 0xAF3;
    }
    return 0xAF1 + (dComIfGs_isEventBit(0x210) ? 1 : 0);
}

/* 0000237C-000023B8       .text getMsg_BOU_1__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_BOU_1() {
    return 0xAF4 + (dComIfGs_isEventBit(0x208) ? 1 : 0);
}

/* 000023B8-000023E8       .text bitCount__11daNpc_Ko1_cFUc */
s8 daNpc_Ko1_c::bitCount(u8 bits) {
    s8 count = 0;
    for (int i = 0; i < 8; i++) {
        if (bits & 1) {
            count++;
        }
        bits = bits >> 1;
    }
    return count;
}

/* 000023E8-0000248C       .text getMsg_BOU_2__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg_BOU_2() {
    if (dComIfGs_isEventBit(0x3801)) {
        if (dComIfGs_isEventBit(0x3340)) {
            return 0xB08;
        }
        if (bitCount(dComIfGs_getEventReg(0xBFFF)) >= 2) {
            return 0xB07;
        }
        return 0xB00;
    }
    return 0xAF6;
}

/* 0000248C-00002528       .text getMsg__11daNpc_Ko1_cFv */
u32 daNpc_Ko1_c::getMsg() {
    u32 msg = 0;
    switch (mStaff) {
    case 0:
        msg = getMsg_HNA_0();
        break;
    case 1:
        msg = getMsg_HNA_1();
        break;
    case 2:
        msg = getMsg_HNA_2();
        break;
    case 3:
        msg = getMsg_HNA_3();
        break;
    case 4:
        break;
    case 5:
        msg = getMsg_BOU_0();
        break;
    case 6:
        msg = getMsg_BOU_1();
        break;
    case 7:
        msg = getMsg_BOU_2();
        break;
    case 8:
        break;
    }
    return msg;
}

/* 00002528-000025A8       .text chkAttention__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 000025A8-00002610       .text setAttention__11daNpc_Ko1_cFb */
void daNpc_Ko1_c::setAttention(bool force) {
    f32 ofs = l_HIO.mChild[mType].mPrm.mAttentionOffsetY;
    attention_info.position.set(current.pos.x, current.pos.y + ofs, current.pos.z);
    if (mUpdateEye != 0 || force) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
    }
}

/* 00002610-00002644       .text searchByID__11daNpc_Ko1_cFUi */
fopAc_ac_c* daNpc_Ko1_c::searchByID(fpc_ProcID id) {
    fopAc_ac_c* actor = NULL;
    fopAcM_SearchByID(id, &actor);
    return actor;
}

/* 00002644-000026DC       .text partner_srch_sub__11daNpc_Ko1_cFPFPvPv_Pv */
fpc_ProcID daNpc_Ko1_c::partner_srch_sub(void* (*judge)(void*, void*)) {
    fpc_ProcID id = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < ARRAY_SSIZE(l_check_inf); i++) {
        l_check_inf[i] = NULL;
    }
    fpcM_Search(judge, this);
    if (l_check_wrk != 0) {
        id = fopAcM_GetID(l_check_inf[0]);
    }
    return id;
}

/* 000026DC-000027CC       .text partner_srch__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::partner_srch() {
    if (mActionState == 1) {
        bool found = false;
        switch (mStaff) {
        case 0:
            break;
        case 1:
            mPartnerId[0] = partner_srch_sub(searchActor_Ko_Bou);
            mPartnerNum = 1;
            break;
        case 2:
            break;
        case 3:
            mPartnerId[0] = partner_srch_sub(searchActor_Ko_Bou);
            mPartnerId[1] = partner_srch_sub(searchActor_Ob);
            mPartnerNum = 2;
            break;
        case 4:
        case 5:
            break;
        case 6:
            mPartnerId[0] = partner_srch_sub(searchActor_Ko_Hna);
            mPartnerNum = 1;
            break;
        case 7:
            mPartnerId[0] = partner_srch_sub(searchActor_Ko_Hna);
            mPartnerId[1] = partner_srch_sub(searchActor_Ob);
            mPartnerNum = 2;
            break;
        case 8:
            break;
        }
        if (found) {
            mActionState++;
        }
    }
}

/* 000027CC-00002900       .text check_landOn__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::check_landOn() {
    fopAc_ac_c* player = dComIfGp_getLinkPlayer();
    cXyz pos(-198160.0f, 150.0f, 319690.0f);
    f32 dist = (player->current.pos - pos).absXZ();
    if (player->current.pos.y == pos.y && dist < 280.0f && ((daPy_py_c*)player)->getAutoJumpLand()) {
        dComIfGs_onEventBit(0x104);
        return true;
    }
    return false;
}

/* 00002900-0000299C       .text ko_setPthPos__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::ko_setPthPos() {
    if (mPath.getPath() != NULL) {
        current.pos = mPath.getPoint(mPath.getIdx());
        mPath.nextIdxAuto();
        cXyz pos = mPath.getPoint(mPath.getIdx());
        current.angle.y = cLib_targetAngleY(&current.pos, &pos);
    }
}

/* 0000299C-00002AA4       .text set_tgtPos__11daNpc_Ko1_cF4cXyz */
cXyz daNpc_Ko1_c::set_tgtPos(cXyz pos) {
    cXyz offset(0.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::transS(pos.x, pos.y, pos.z);
    mDoMtx_stack_c::YrotM(dComIfGp_getLinkPlayer()->current.angle.y);
    f32 s = cM_ssin(mTgtAngle);
    offset.x = 80.0f * s;
    f32 c = cM_scos(mTgtAngle);
    offset.z = 40.0f * c;
    mTgtAngle += 0x400;
    cXyz result;
    mDoMtx_stack_c::multVec(&offset, &result);
    return result;
}

/* 00002AA4-00002C14       .text ko_movPass__11daNpc_Ko1_cFv */
int daNpc_Ko1_c::ko_movPass() {
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

/* 00002C14-00002D50       .text ko_clcMovSpd__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::ko_clcMovSpd() {
    f32 dist2 = (mMoveTarget - current.pos).abs2XZ();
    s16 angle = cLib_targetAngleY(&current.pos, &mMoveTarget);
    if (routeCheck(dist2, &angle)) {
        if (mState == 4 || mState == 0xB || mState == 0x18) {
            if (cLib_calcTimer(&m850) == 0) {
                m864 ^= 1;
                m850 = cLib_getRndValue<int>(8, 0x14);
            }
            angle += (m864 != 0) ? -0x2000 : 0x2000;
        }
        cLib_chaseAngleS(&current.angle.y, angle, l_HIO.mChild[mType].mPrm.m1C);
        cLib_chaseF(&speedF, mTargetSpeed, mSpeedStep);
    }
}

/* 00002D50-00002E3C       .text ko_clcSwmSpd__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::ko_clcSwmSpd() {
    f32 dist2 = (mMoveTarget - current.pos).abs2XZ();
    s16 angle = cLib_targetAngleY(&current.pos, &mMoveTarget);
    if (routeCheck(dist2, &angle)) {
        cLib_chaseAngleS(&current.angle.y, angle, l_HIO.mChild[mType].mPrm.m1C);
        cLib_chaseF(&speed.y, m830, 1.6f);
        cLib_chaseF(&speedF, 0.0f, 0.4f);
    }
}

/* 00002E3C-00003028       .text ko_nMove__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::ko_nMove() {
    switch (m898) {
    case 1:
    case 2: {
        ko_clcMovSpd();
        f32 playSpeed;
        if (m898 == 1) {
            playSpeed = speedF * l_HIO.mChild[mType].mPrm.m20;
            playSpeed = (playSpeed > l_HIO.mChild[mType].mPrm.m24 * l_HIO.mChild[mType].mPrm.m20) ? l_HIO.mChild[mType].mPrm.m24 * l_HIO.mChild[mType].mPrm.m20 : playSpeed;
        } else {
            playSpeed = speedF * l_HIO.mChild[mType].mPrm.m30;
            playSpeed = (playSpeed > l_HIO.mChild[mType].mPrm.m34 * l_HIO.mChild[mType].mPrm.m30) ? l_HIO.mChild[mType].mPrm.m34 * l_HIO.mChild[mType].mPrm.m30 : playSpeed;
        }
        if (playSpeed < 0.5f) {
            playSpeed = 0.5f;
        }
        setPlaySpd(playSpeed);
        switch (ko_movPass()) {
        case 1:
            mArrived = 1;
            break;
        case 2:
            mArrived = 1;
            m898 = 0;
            break;
        }
        break;
    }
    case 3:
        ko_clcSwmSpd();
        switch (ko_movPass()) {
        case 1:
            mArrived = 1;
            break;
        case 2:
            mArrived = 1;
            m898 = 0;
            break;
        }
        break;
    case 4:
        cLib_chaseF(&speedF, 0.1f, mSpeedStep);
        if (m86C == 0 && mObjAcch.ChkGroundHit()) {
            speed.y = 0.0f;
            speedF = 0.0f;
            gravity = -4.5f;
            m898 = m899;
            mArrived = 1;
        }
        break;
    }
    if (mArrived) {
        mStopMove = 1;
    }
}

/* 00003028-000030E8       .text chk_routeAngle__11daNpc_Ko1_cFR4cXyzPs */
void daNpc_Ko1_c::chk_routeAngle(cXyz& vec, s16* angleP) {
    cXyz cross = mMoveDir.outprod(vec);
    s16 angle = cM_atan2s(cross.x, cross.z);
    if ((mMoveDir.y >= 0.999f && cLib_distanceAngleS(angle, *angleP) > 0x4000) || (mMoveTarget.y - current.pos.y) * cross.y < 0.0f) {
        angle += 0x8000;
    }
    *angleP = angle;
}

/* 000030E8-0000334C       .text routeWallCheck__11daNpc_Ko1_cFR4cXyzR4cXyzPs */
void daNpc_Ko1_c::routeWallCheck(cXyz& i_start, cXyz& i_end, short* angleP) {
    dBgS_LinChk linChk;
    linChk.Set(&i_start, &i_end, NULL);
    if (dComIfG_Bgsp()->LineCross(&linChk)) {
        cM3dGPla* pla = dComIfG_Bgsp()->GetTriPla(linChk);
        if (pla) {
            chk_routeAngle(pla->mNormal, angleP);
        }
    }
}

/* 00003784-00003A04       .text chk_ForwardGroundY__11daNpc_Ko1_cFs */
f32 daNpc_Ko1_c::chk_ForwardGroundY(s16 angle) {
    cM3dGPla* pla = dComIfG_Bgsp()->GetTriPla(mAcchCir);
    if (pla != NULL) {
        if (cLib_distanceAngleS(angle, cM_atan2s(pla->mNormal.x, pla->mNormal.z)) > 0x4000) {
            dBgS_GndChk gndChk;
            gndChk.OffWall();
            cXyz pos(
                current.pos.x + cM_ssin(angle) * 80.0f,
                current.pos.y + 80.0f,
                current.pos.z + cM_scos(angle) * 80.0f
            );
            gndChk.SetPos(&pos);
            return dComIfG_Bgsp()->GroundCross(&gndChk);
        }
    }
    return -10000000.0f;
}

/* 00003B9C-00003C54       .text chk_wallJump__11daNpc_Ko1_cFs */
f32 daNpc_Ko1_c::chk_wallJump(s16 angle) {
    f32 y = chk_ForwardGroundY(angle);
    if (0.0f < y && y < 100.0f) {
        return std::sqrtf(y) * 3.2f;
    }
    return -1.0f;
}

/* 00003C54-00003D34       .text routeCheck__11daNpc_Ko1_cFfPs */
BOOL daNpc_Ko1_c::routeCheck(f32, s16* pAngle) {
    if (m898 != 3) {
        if (mObjAcch.ChkWallHit()) {
            chk_wallJump(*pAngle);
        }
    }
    cXyz start(current.pos.x, current.pos.y + 80.0f, current.pos.z);
    f32 z = current.pos.z + 80.0f * cM_scos(*pAngle);
    f32 x = current.pos.x + 80.0f * cM_ssin(*pAngle);
    cXyz end(x, current.pos.y + 80.0f, z);
    routeWallCheck(start, end, pAngle);
    return TRUE;
}

/* 00003D34-00003DE8       .text chk_start_swim__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::chk_start_swim() {
    bool result = false;
    if (mObjAcch.ChkWaterIn()) {
        result = mObjAcch.m_wtr.GetHeight() - mObjAcch.GetGroundH() > 62.0f;
        if (result) {
            if (mState != 7) {
                setPrtcl_Hamon(1.0f, 0.0f);
            }
        } else {
            if (mState == 7) {
                setPrtcl_Hamon(0.5f, 1.0f);
            }
        }
    } else {
        mRippleCallback.end();
        mpHamonEmitter = NULL;
    }
    return result;
}

/* 00003DE8-00003E64       .text get_crsActorID__11daNpc_Ko1_cFv */
fpc_ProcID daNpc_Ko1_c::get_crsActorID() {
    if (mCyl.ChkCoHit()) {
        cCcD_Obj* obj = mCyl.GetCoHitObj();
        if (obj != NULL) {
            fopAc_ac_c* actor = dCc_GetAc(obj->GetAc());
            return fopAcM_GetID(actor);
        }
    }
    return fpcM_ERROR_PROCESS_ID_e;
}

/* 00003E64-00003F50       .text chk_areaIn__11daNpc_Ko1_cFf4cXyz */
bool daNpc_Ko1_c::chk_areaIn(float radius, cXyz pos) {
    return (dComIfGp_getLinkPlayer()->current.pos - pos).absXZ() < radius;
}

/* 00003F50-00003FF0       .text setPrtcl_Hamon__11daNpc_Ko1_cFff */
void daNpc_Ko1_c::setPrtcl_Hamon(float scale, float rate) {
    cXyz scl(scale, scale, scale);
    mRippleCallback.end();
    mpHamonEmitter = dComIfGp_particle_setShipTail(dPa_name::ID_AK_JN_HAMON00, &current.pos, NULL, &scl, 0xFF, &mRippleCallback);
    if (mpHamonEmitter != NULL) {
        mRippleCallback.setRate(rate);
    }
}

/* 00003FF0-000040F8       .text setPrtcl_HanaPachi__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::setPrtcl_HanaPachi() {
    mDoMtx_stack_c::copy(mpBalloonMorf->getModel()->getAnmMtx(m_bln_jnt_num));
    cXyz pos;
    mDoMtx_stack_c::multVecZero(&pos);
    mpHanaEmitterA = dComIfGp_particle_set(
        dPa_name::ID_IT_SN_KO_HANAPACHIA,
        &pos,
        &current.angle,
        NULL,
        0xFF,
        NULL,
        fopAcM_GetRoomNo(this)
    );
    mpHanaEmitterB = dComIfGp_particle_set(
        dPa_name::ID_IT_SN_KO_HANAPACHIB,
        &pos,
        &current.angle,
        NULL,
        0xFF,
        NULL,
        fopAcM_GetRoomNo(this)
    );
}

/* 000040F8-0000420C       .text charDecide__11daNpc_Ko1_cFi */
bool daNpc_Ko1_c::charDecide(int param) {
    mType = -1;
    mStaff = -1;
    switch (fopAcM_GetName(this)) {
    case fpcNm_NPC_KO1_e:
        mType = 0;
        switch (param) {
        case 0:
            mStaff = 0;
            break;
        case 1:
            mStaff = 1;
            break;
        case 2:
            mStaff = 2;
            break;
        case 3:
            mStaff = 3;
            break;
        case 4:
            mStaff = 4;
            break;
        default:
            return false;
        }
        break;
    case fpcNm_NPC_KO2_e:
        mType = 1;
        switch (param) {
        case 0:
            mStaff = 5;
            break;
        case 1:
            mStaff = 6;
            break;
        case 2:
            mStaff = 7;
            break;
        case 3:
            mStaff = 8;
            break;
        default:
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

/* 0000420C-00004264       .text event_actionInit__11daNpc_Ko1_cFi */
void daNpc_Ko1_c::event_actionInit(int staff) {
    int* action = dComIfGp_evmng_getMyIntegerP(staff, "ActNo");
    if (action != NULL) {
        mEventAction = *action;
    }
}

/* 00004264-00004274       .text event_action__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::event_action() {
    switch (mEventAction) {
    case 0:
        break;
    }
    return TRUE;
}

/* 00004274-00004360       .text privateCut__11daNpc_Ko1_cFi */
void daNpc_Ko1_c::privateCut(int staff) {
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
                done = TRUE;
                break;
            }
            if (done) {
                dComIfGp_evmng_cutEnd(staff);
            }
        }
    }
}

/* 00004360-00004384       .text endEvent__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
}

/* 00004384-000043BC       .text isEventEntry__11daNpc_Ko1_cFv */
int daNpc_Ko1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mCut.getActorName());
}

/* 000043BC-00004414       .text event_proc__11daNpc_Ko1_cFi */
void daNpc_Ko1_c::event_proc(int staff) {
    if (!mCut.cutProc()) {
        privateCut(staff);
    }
    lookBack();
}

/* 00004414-000044C0       .text set_action__11daNpc_Ko1_cFM11daNpc_Ko1_cFPCvPvPv_iPv */
bool daNpc_Ko1_c::set_action(int (daNpc_Ko1_c::*action)(void*), void* arg) {
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

/* 000044C0-000044E8       .text clrSpd__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::clrSpd() {
    mTargetSpeed = 0.0f;
    mSpeedStep = 0.0f;
    speed.y = 0.0f;
    speedF = 0.0f;
    gravity = -4.5f;
}

/* 000044E8-00004B18       .text setStt__11daNpc_Ko1_cFSc */
void daNpc_Ko1_c::setStt(signed char state) {
    fopAc_ac_c* a_partner = searchByID(mPartnerId[0]);
    s8 prev = mState;
    m84E = 0;
    mState = state;
    switch (mState) {
    case 1:
    case 2:
    case 12:
    case 22:
    case 29:
        switch (mState) {
        case 1:
            mNeruTimer = 90.0f + cM_rndF(90.0f);
            break;
        case 2:
            mWaitTimer = (g_Counter.mCounter0 & 3) + 1;
            break;
        }
        if (prev != 3) {
            mLookMode = 3;
            mLookAngle = mHomeAngle.y;
            m_jnt.setTrn();
            mNoTurn = false;
        }
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
        break;
    case 3:
        mLookMode = 1;
        m_jnt.setTrn();
        mNoTurn = false;
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        m8A4 = prev;
        break;
    case 4:
    case 11:
        switch (mState) {
        case 4:
            mNeruTimer = 180.0f + cM_rndF(180.0f);
            break;
        case 11:
            if (mpSavedPath != NULL) {
                mPath.setInfDrct(mpSavedPath);
                mPath.setNearPathIndx(&current.pos, 0.0f);
                mpSavedPath = NULL;
            }
            break;
        }
        mMoveTarget = mPath.getPoint(mPath.getIdx());
        mLookMode = 0;
        mNoTurn = true;
        mEventOrder = 0;
        m898 = 1;
        mArrived = 0;
        goto walk_spd;
    case 5:
        mLookMode = 0;
        mNoTurn = false;
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
        break;
    case 6:
    case 13:
    case 23:
        switch (mState) {
        case 6:
            if (mPath.getPath() != NULL) {
                mpSavedPath = mPath.getPath();
                mPath.setInfDrct(NULL);
            }
            break;
        }
        mMoveTarget = dComIfGp_getLinkPlayer()->current.pos;
        mLookMode = 1;
        mNoTurn = true;
        mEventOrder = 0;
        m898 = 2;
        mArrived = 0;
        goto run_spd;
    case 7:
        if (mPath.getPath() != NULL) {
            mpSavedPath = mPath.getPath();
            mPath.setInfDrct(NULL);
        }
        mMoveTarget = dComIfGp_getLinkPlayer()->current.pos;
        mLookMode = 1;
        mNoTurn = true;
        goto swim_setup;
    case 8:
        if (mpSavedPath != NULL) {
            mPath.setInfDrct(mpSavedPath);
            mPath.setNearPathIndx(&current.pos, 0.0f);
            mpSavedPath = NULL;
        }
        mMoveTarget = mPath.getPoint(mPath.getIdx());
        mLookMode = 0;
        mNoTurn = true;
    swim_setup:
        mEventOrder = 0;
        m898 = 3;
        mArrived = 0;
        m830 = -4.0f;
        speed.y = m830;
        speedF = 0.0f;
        mSpeedStep = 0.0f;
        mTargetSpeed = 0.0f;
        gravity = 0.0f;
        mArriveDistance = l_HIO.mChild[mType].mPrm.m40;
        break;
    case 9:
    case 16:
    case 18:
    case 25:
        mLookMode = 0;
        mNoTurn = false;
        mEventOrder = 0;
        m899 = m898;
        m898 = 4;
        m86C = 1;
        speed.y = 10.0f;
        speedF = -3.0f;
        gravity = -1.6f;
        mSpeedStep = 0.1f;
        break;
    case 10:
    case 14:
    case 26:
        mLookMode = 1;
        mNoTurn = false;
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
        break;
    case 15:
    case 24:
        mMoveTarget = mHomePos;
        mLookMode = 0;
        mNoTurn = true;
        mEventOrder = 0;
        m898 = 1;
        mArrived = 0;
    walk_spd:
        gravity = -4.5f;
        mTargetSpeed = l_HIO.mChild[mType].mPrm.m24;
        mSpeedStep = l_HIO.mChild[mType].mPrm.m28;
        mArriveDistance = l_HIO.mChild[mType].mPrm.m2C;
        break;
    case 17:
        JUT_ASSERT(0xA73, a_partner != 0);
        mMoveTarget = a_partner->current.pos;
        mLookMode = 2;
        mLookPos = mMoveTarget;
        mLookPos.y = a_partner->eyePos.y;
        mNoTurn = true;
        mEventOrder = 0;
        m898 = 2;
        mArrived = 0;
    run_spd:
        gravity = -4.5f;
        mTargetSpeed = l_HIO.mChild[mType].mPrm.m34;
        mSpeedStep = l_HIO.mChild[mType].mPrm.m38;
        mArriveDistance = l_HIO.mChild[mType].mPrm.m3C;
        break;
    case 19:
        if (a_partner != NULL) {
            mLookMode = 2;
            mLookPos = a_partner->current.pos;
            mLookPos.y = a_partner->eyePos.y;
        } else {
            mLookMode = 0;
        }
        mNoTurn = false;
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
        break;
    case 20:
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        m8A4 = prev;
        break;
    case 27:
    case 28:
        mNeruTimer = 180.0f + cM_rndF(180.0f);
        mWaitTimer = cLib_getRndValue<int>(3, 10);
        m865 = 0;
        break;
    }
    setAnm();
}

/* 00004B18-00004C70       .text wait_1__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_1() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(3);
        }
        return TRUE;
    }
    mEventOrder = 2;
    mLookMode = 3;
    mLookAngle = mHomeAngle.y;
    if (mAnmIndex == 6) {
        if (mAnmEnded) {
            cLib_calcTimer(&mWaitTimer);
        }
        if (mWaitTimer == 0 || m874 != 0) {
            mWaitTimer = (g_Counter.mCounter0 & 3) + 1;
            mNeruTimer = 90.0f + cM_rndF(90.0f);
            setAnm_NUM(0, 1);
        }
        return TRUE;
    }
    if (m874) {
        m84E = 0x3C;
    }
    if (cLib_calcTimer(&m84E)) {
        mLookMode = 1;
        return TRUE;
    }
    if (cLib_calcTimer(&mNeruTimer) == 0) {
        setAnm_NUM(6, 1);
    }
    m_jnt.setTrn();
    return TRUE;
}

/* 00004C70-00004CE8       .text wait_2__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_2() {
    if (mAnmEnded) {
        cLib_calcTimer(&mWaitTimer);
        if (mWaitTimer == 0 || m874 != 0 || mTalking != 0) {
            setStt(1);
            return TRUE;
        }
    }
    return TRUE;
}

/* 00004CE8-00004D1C       .text wait_3__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_3() {
    if (mAnmEnded) {
        setStt(4);
    }
    return TRUE;
}

/* 00004D1C-00004E64       .text wait_4__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_4() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(3);
        }
        return TRUE;
    }
    if (chk_start_swim()) {
        setStt(7);
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m4C, current.pos) == 0) {
        if (chk_areaIn(l_HIO.mChild[mType].mPrm.m54, mTargetPos) == 0) {
            setStt(0xB);
            return TRUE;
        }
        setStt(6);
        return TRUE;
    }
    mEventOrder = 2;
    return TRUE;
}

/* 00004E64-00004F30       .text wait_5__11daNpc_Ko1_cFSc */
BOOL daNpc_Ko1_c::wait_5(signed char stt) {
#if VERSION > VERSION_DEMO
    if (field_0x6bc[0] == 1) {
        field_0x6bc[0] = 2;
        setStt(0x14);
        mLookMode = 1;
        mNoTurn = false;
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
        return TRUE;
    }
#endif
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m44, mTargetPos)) {
        setStt(stt);
    }
    return TRUE;
}

/* 00004F30-00005088       .text wait_6__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_6() {
    if (mTalking) {
        if (chk_talk()) {
            if (chk_manzai_1()) {
                setStt(3);
            }
        }
        return TRUE;
    }
    if (field_0x6bc[0] == 1) {
        field_0x6bc[0] = 2;
        setStt(0x14);
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m4C, current.pos) == 0) {
        if (chk_areaIn(l_HIO.mChild[mType].mPrm.m48, mTargetPos) == 0) {
            setStt(0xF);
            return TRUE;
        }
        setStt(0xD);
        return TRUE;
    }
    mEventOrder = 2;
    return TRUE;
}

/* 00005088-000052D4       .text wait_7__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_7() {
    fopAc_ac_c* a_partner = searchByID(mPartnerId[0]);
    JUT_ASSERT(DEMO_SELECT(0xB5E, 0xB6B), a_partner != 0);
    if (mTalking) {
        if (chk_talk()) {
            if (chk_manzai_1()) {
                setStt(3);
            }
        }
        return TRUE;
    }
    if (field_0x6bc[0] == 1) {
        field_0x6bc[0] = 2;
        setStt(0x14);
#if VERSION > VERSION_DEMO
        mLookMode = 1;
        mNoTurn = false;
        mEventOrder = 0;
        m898 = 0;
        clrSpd();
#endif
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m4C, current.pos)) {
        mLookMode = 1;
    } else {
        mLookMode = 2;
        mLookPos = a_partner->current.pos;
        mLookPos.y = a_partner->eyePos.y;
    }
    f32 dist = (a_partner->current.pos - current.pos).absXZ();
    if (dist < l_HIO.mChild[mType].mPrm.m4C) {
        mEventOrder = 2;
    } else {
        setStt(0x11);
    }
    return TRUE;
}

/* 000052D4-000053F8       .text wait_9__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_9() {
    if (mTalking) {
        if (chk_talk()) {
            setStt(3);
        }
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m4C, current.pos) == 0) {
        if (chk_areaIn(l_HIO.mChild[mType].mPrm.m48, mTargetPos) == 0) {
            setStt(0x18);
            return TRUE;
        }
        setStt(0x17);
        return TRUE;
    }
    mEventOrder = 2;
    return TRUE;
}

/* 000053F8-00005524       .text wait_a__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::wait_a() {
    fopAc_ac_c* a_partner = searchByID(mPartnerId[0]);
    JUT_ASSERT(DEMO_SELECT(0xBAF, 0xBC3), a_partner != 0);
    if (mTalking) {
        if (chk_talk()) {
            if (chk_manzai_1()) {
                setStt(3);
            }
        }
        return TRUE;
    }
    if (field_0x6bc[0] == 1) {
        field_0x6bc[0] = 2;
        setStt(0x14);
        return TRUE;
    }
    mEventOrder = 2;
    if (m874) {
        m84E = 0x3C;
    }
    if (cLib_calcTimer(&m84E)) {
        mLookMode = 1;
    } else {
        mLookMode = 3;
        mLookAngle = mHomeAngle.y;
        m_jnt.setTrn();
    }
    return TRUE;
}

/* 00005524-000055F8       .text walk_1__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::walk_1() {
    mArrived = 0;
    mMoveTarget = mPath.getPoint(mPath.getIdx());
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m50, mTargetPos)) {
        setStt(6);
        return TRUE;
    }
    if (cLib_calcTimer(&mNeruTimer) == 0) {
        setStt(5);
    }
    return TRUE;
}

/* 000055F8-000056D4       .text walk_2__11daNpc_Ko1_cFScSc */
BOOL daNpc_Ko1_c::walk_2(signed char stt1, signed char stt2) {
    if (mArrived != 0) {
        setStt(stt1);
        return TRUE;
    }
#if VERSION > VERSION_DEMO
    if (field_0x6bc[0] == 1) {
        setStt(0xE);
        return TRUE;
    }
#endif
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m44, mTargetPos)) {
        setStt(stt2);
        return TRUE;
    }
    mMoveTarget = mHomePos;
    return TRUE;
}

/* 000056D4-000057B8       .text walk_3__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::walk_3() {
    if (mArrived != 0) {
        setStt(4);
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m50, mTargetPos)) {
        setStt(6);
        return TRUE;
    }
    mMoveTarget = mPath.getPoint(mPath.getIdx());
    if (chk_start_swim()) {
        setStt(8);
    }
    return TRUE;
}

/* 000057B8-000058EC       .text swim_1__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::swim_1() {
    f32 waterDiff = mObjAcch.m_wtr.GetHeight() - current.pos.y;
    if (waterDiff < 49.0f) {
        m830 = -4.0f;
        if (mArrived == 0) {
            speedF = 8.0f;
        }
    } else if (waterDiff > 58.0f) {
        m830 = 4.0f;
    }
    if (!chk_start_swim()) {
        setStt(6);
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m54, mTargetPos) == 0) {
        setStt(8);
        return TRUE;
    }
    mArrived = 0;
    mMoveTarget = dComIfGp_getLinkPlayer()->current.pos;
    return TRUE;
}

/* 000058EC-00005A1C       .text swim_2__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::swim_2() {
    f32 waterDiff = mObjAcch.m_wtr.GetHeight() - current.pos.y;
    if (waterDiff < 49.0f) {
        m830 = -4.0f;
        if (mArrived == 0) {
            speedF = 8.0f;
        }
    } else if (waterDiff > 58.0f) {
        m830 = 4.0f;
    }
    if (!chk_start_swim()) {
        setStt(0xB);
        return TRUE;
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m50, mTargetPos)) {
        setStt(7);
        return TRUE;
    }
    mArrived = 0;
    mMoveTarget = mPath.getPoint(mPath.getIdx());
    return TRUE;
}

/* 00005A1C-00005B64       .text attk_1__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::attk_1() {
    mArrived = 0;
    mCrsActorId = get_crsActorID();
    if (mCrsActorId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = searchByID(mCrsActorId);
        if (actor != NULL) {
            s16 name = fopAcM_GetName(actor);
            if (name == fpcNm_PLAYER_e) {
                setStt(9);
                return TRUE;
            }
        }
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m54, mTargetPos) == 0) {
        setStt(0xB);
        return TRUE;
    }
    mMoveTarget = set_tgtPos(dComIfGp_getLinkPlayer()->current.pos);
    if (chk_start_swim()) {
        setStt(7);
    }
    return TRUE;
}

/* 00005B64-00005C94       .text attk_2__11daNpc_Ko1_cFScSc */
BOOL daNpc_Ko1_c::attk_2(signed char param_0, signed char param_1) {
    mArrived = 0;
    if (field_0x6bc[0] == 1) {
        setStt(0xE);
        return TRUE;
    }
    mCrsActorId = get_crsActorID();
    if (mCrsActorId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = searchByID(mCrsActorId);
        if (actor != NULL) {
            s16 name = fopAcM_GetName(actor);
            if (name == fpcNm_PLAYER_e) {
                setStt(param_0);
                return TRUE;
            }
        }
    }
    if (chk_areaIn(l_HIO.mChild[mType].mPrm.m48, mTargetPos) == 0) {
        setStt(param_1);
        return TRUE;
    }
    mMoveTarget = dComIfGp_getLinkPlayer()->current.pos;
    return TRUE;
}

/* 00005C94-00005DEC       .text attk_3__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::attk_3() {
    fopAc_ac_c* a_partner = searchByID(mPartnerId[0]);
    JUT_ASSERT(DEMO_SELECT(0xCB7, 0xCD2), a_partner != 0);
    mArrived = 0;
    if (field_0x6bc[0] == 1) {
        setStt(0x13);
        return TRUE;
    }
    mCrsActorId = get_crsActorID();
    if (mCrsActorId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = searchByID(mCrsActorId);
        if (actor != NULL) {
            s16 name = fopAcM_GetName(actor);
            if (name == fpcNm_NPC_KO2_e || name == fpcNm_PLAYER_e) {
                setStt(0x12);
                return TRUE;
            }
        }
    }
    mMoveTarget = set_tgtPos(a_partner->current.pos);
    mLookPos = mMoveTarget;
    mLookPos.y = a_partner->eyePos.y;
    return TRUE;
}

/* 00005DEC-00005E1C       .text down_1__11daNpc_Ko1_cFSc */
BOOL daNpc_Ko1_c::down_1(s8 stt) {
    if (mArrived != 0) {
        setStt(stt);
    }
    return TRUE;
}

/* 00005E1C-0000609C       .text talk_1__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::talk_1() {
    BOOL result = chk_partsNotMove();
    if (mpCurrMsg != NULL) {
        mManzaiMsgStatus = mpCurrMsg->mStatus;
    } else {
        mManzaiMsgStatus = 0;
    }
    if (field_0x6bc[0] == 2 && mCurrMsgBsPcId == fpcM_ERROR_PROCESS_ID_e) {
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
            switch (mCurrMsgNo) {
            case 0xAF1:
                dComIfGs_onEventBit(0x210);
                break;
            case 0xAFF:
                dComIfGs_onEventBit(0x3801);
                break;
            case 0xB06:
                dComIfGs_onEventBit(0x3340);
                break;
            case 0xB55:
                dComIfGs_onEventBit(0x220);
                break;
            case 0xB57:
                dComIfGs_onEventBit(0x208);
                dComIfGs_onEventBit(0x240);
                break;
            case 0xB62:
                dComIfGs_onEventBit(0x3101);
                break;
            case 0xB64:
                dComIfGs_onEventBit(0x2C04);
                m86D = 1;
                break;
            }
            mPresentItem = 0xFF;
            mTalking = false;
            switch (mStaff) {
            case 1:
            case 3:
            case 6:
            case 7:
                setStt(0x15);
                break;
            default:
                setStt(m8A4);
                m84E = 0x3C;
                endEvent();
                break;
            }
            break;
        case 2:
        case 6:
            break;
        }
    }
    return result;
}

/* 0000609C-00006184       .text talk_2__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::talk_2() {
    int ready = 0;
    for (int i = 0; i < mPartnerNum; i++) {
        fopAc_ac_c* a_actor = searchByID(mPartnerId[i]);
        JUT_ASSERT(DEMO_SELECT(0xD3E, 0xD59), 0 != a_actor);
        daNpc_Ko1_c* npc = (daNpc_Ko1_c*)a_actor;
        if (npc->field_0x6bc[0] != 0) {
            npc->field_0x6bc[0] = 3;
        } else {
            ready++;
        }
    }
    bool all = ready == mPartnerNum;
    if (all) {
        field_0x6bc[0] = 0;
        setStt(m8A4);
        endEvent();
    }
    return TRUE;
}

/* 00006184-000062B4       .text manzai__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::manzai() {
    dComIfG_MesgCamInfo_c* info = dComIfGp_getMesgCameraInfo();
    switch (field_0x6bc[0]) {
    case 2: {
        fopNpc_npc_c* partner = (fopNpc_npc_c*)searchByID(mManzaiPartnerId);
        if (this != info->mActor[info->mBasicID - 1]) {
            if (mAnmAttr != 0xFF) {
                if (mStaff == 1 || mStaff == 6) {
                    mLookMode = 1;
                    m_jnt.setTrn();
                } else {
                    mLookMode = 3;
                    mLookAngle = mHomeAngle.y;
                    m_jnt.setTrn();
                    mState = m8A4;
                    setAnm();
                    mState = 0x14;
                }
                mAnmAttr = 0xFF;
            }
        } else {
            mPairedMsgNo = partner->mCurrMsgNo;
            anmAtr(partner->mManzaiMsgStatus);
        }
        break;
    }
    case 3:
        actor_status &= ~fopAcStts_UNK4000_e;
        setStt(m8A4);
        field_0x6bc[0] = 0;
        break;
    }
    return TRUE;
}

/* 000062B4-00006358       .text neru_1__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::neru_1() {
    int a_anm_tbl[] = {7, 8};
    if (cLib_calcTimer(&mNeruTimer) == 0) {
        m865 ^= 1;
        setAnm_NUM(a_anm_tbl[m865], 1);
        mNeruTimer = 180.0f + cM_rndF(180.0f);
    }
    return TRUE;
}

/* 00006358-00006524       .text neru_2__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::neru_2() {
    int a_anm_tbl[] = {9, 12, 10};
    switch (m865) {
    case 0:
        if (cLib_calcTimer(&mNeruTimer) == 0) {
            if (cLib_calcTimer(&mWaitTimer) == 0) {
                m865 = 1;
                setAnm_NUM(a_anm_tbl[m865], 1);
                set_balloonAnm_NUM(1);
                mWaitTimer = cLib_getRndValue<int>(3, 10);
            }
            mNeruTimer = 180.0f + cM_rndF(180.0f);
        }
        break;
    case 1:
        if (mAnmEnded) {
            m865 = 2;
            setAnm_NUM(a_anm_tbl[m865], 1);
        } else if (mpMorf->checkFrame(2.0f)) {
            setPrtcl_HanaPachi();
            mDoAud_seStart(JA_SE_CM_KO_HANA_BREAK, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        }
        break;
    case 2:
        if (mAnmEnded) {
            m865 = 0;
            setAnm_NUM(a_anm_tbl[m865], 1);
            set_balloonAnm_NUM(0);
        }
        break;
    }
    return TRUE;
}

/* 00006524-00006668       .text hana_action1__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::hana_action1(void*) {
    switch (mActionState) {
    case 0:
        ko_setPthPos();
        setStt(4);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 4:
            mUpdateEye = walk_1();
            break;
        case 11:
            mUpdateEye = walk_3();
            break;
        case 5:
            mUpdateEye = wait_3();
            break;
        case 10:
            mUpdateEye = wait_4();
            break;
        case 6:
            mUpdateEye = attk_1();
            break;
        case 7:
            mUpdateEye = swim_1();
            break;
        case 8:
            mUpdateEye = swim_2();
            break;
        case 9:
            mUpdateEye = down_1(0xA);
            break;
        case 3:
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

/* 00006668-00006774       .text hana_action2__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::hana_action2(void*) {
    switch (mActionState) {
    case 0:
        setStt(0x13);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0x13:
            mUpdateEye = wait_7();
            break;
        case 0x11:
            mUpdateEye = attk_3();
            break;
        case 0x12:
            mUpdateEye = down_1(0x13);
            break;
        case 3:
            mUpdateEye = talk_1();
            break;
        case 0x15:
            mUpdateEye = talk_2();
            break;
        case 0x14:
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

/* 00006774-00006894       .text hana_action3__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::hana_action3(void*) {
    switch (mActionState) {
    case 0:
        setStt(0x16);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0x16:
            mUpdateEye = wait_5(0x17);
            break;
        case 0x17:
            mUpdateEye = attk_2(0x19, 0x18);
            break;
        case 0x18:
            mUpdateEye = walk_2(0x16, 0x17);
            break;
        case 0x1A:
            mUpdateEye = wait_9();
            break;
        case 0x19:
            mUpdateEye = down_1(0x1A);
            break;
        case 3:
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

/* 00006894-00006988       .text hana_action4__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::hana_action4(void*) {
    switch (mActionState) {
    case 0:
        setStt(0x1D);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0x1D:
            mUpdateEye = wait_a();
            break;
        case 3:
            mUpdateEye = talk_1();
            break;
        case 0x15:
            mUpdateEye = talk_2();
            break;
        case 0x14:
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

/* 00006988-00006A24       .text hana_action5__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::hana_action5(void*) {
    switch (mActionState) {
    case 0:
        setStt(0x1C);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0x1C:
            mUpdateEye = neru_2();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00006A24-00006B24       .text wait_action1__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        setStt(1);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        if (!dComIfGs_isEventBit(0x104)) {
            check_landOn();
        }
        m874 = chkAttention();
        switch (mState) {
        case 1:
            mUpdateEye = wait_1();
            break;
        case 2:
            mUpdateEye = wait_2();
            break;
        case 3:
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

/* 00006B24-00006C64       .text wait_action2__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::wait_action2(void*) {
    switch (mActionState) {
    case 0:
        setStt(0xC);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0xC:
            mUpdateEye = wait_5(0xD);
            break;
        case 0xD:
            mUpdateEye = attk_2(0x10, 0xF);
            break;
        case 0xF:
            mUpdateEye = walk_2(0xC, 0xD);
            break;
        case 0xE:
            mUpdateEye = wait_6();
            break;
        case 0x10:
            mUpdateEye = down_1(0xE);
            break;
        case 3:
            mUpdateEye = talk_1();
            break;
        case 0x15:
            mUpdateEye = talk_2();
            break;
        case 0x14:
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

/* 00006C64-00006D58       .text wait_action3__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::wait_action3(void*) {
    switch (mActionState) {
    case 0:
        setStt(0x1D);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0x1D:
            mUpdateEye = wait_a();
            break;
        case 3:
            mUpdateEye = talk_1();
            break;
        case 0x15:
            mUpdateEye = talk_2();
            break;
        case 0x14:
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

/* 00006D58-00006DF4       .text wait_action4__11daNpc_Ko1_cFPv */
int daNpc_Ko1_c::wait_action4(void*) {
    switch (mActionState) {
    case 0:
        setStt(0x1B);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m874 = chkAttention();
        switch (mState) {
        case 0x1B:
            mUpdateEye = neru_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00006DF4-00006F34       .text demo__11daNpc_Ko1_cFv */
u8 daNpc_Ko1_c::demo() {
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
        J3DAnmTexPattern* pattern = actor->getP_BtpData("Ko");
        if (pattern != NULL) {
            m_hed_tex_pttrn = pattern;
            if (mBtp.init(mpHeadMorf->getModel()->getModelData(), m_hed_tex_pttrn, TRUE, 2, 1.0f, 0, -1, true, FALSE)) {
                mTexIndex = 4;
                mBtpFrame = 0;
            }
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, "Ko");
    }
    return mDemo;
}

/* 00006F34-00007008       .text shadowDraw__11daNpc_Ko1_cFv */
void daNpc_Ko1_c::shadowDraw() {
    cXyz pos(current.pos.x, current.pos.y + 150.0f, current.pos.z);
    mShadowId = dComIfGd_setShadow(
        mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y,
        mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f,
        dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId != 0) {
        if (mpItemModel != NULL) {
            dComIfGd_addRealShadow(mShadowId, mpItemModel);
        }
        dComIfGd_addRealShadow(mShadowId, mpHeadMorf->getModel());
    }
}

/* 00007008-0000728C       .text _draw__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::_draw() {
    J3DModel* headModel = mpHeadMorf->getModel();
    J3DModelData* headData = headModel->getModelData();
    J3DModel* model = mpMorf->getModel();
    if (mHidden || mNoDraw) {
        return TRUE;
    }
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    switch (mType) {
    case 0:
        mpMorf->entryDL();
        break;
    case 1:
        mpMorf->entryDL((J3DMaterialTable*)dComIfG_getObjectIDRes("Ko", dRes_ID_KO_BMT_KO02_e));
        break;
    }
    mBtp.entry(headData, mBtpFrame);
    mpHeadMorf->entryDL();
    mBtp.remove(headData);
    g_env_light.setLightTevColorType(headModel, &tevStr);
    if (mpBalloonMorf != NULL) {
        mpBalloonMorf->entryDL();
        g_env_light.setLightTevColorType(mpBalloonMorf->getModel(), &tevStr);
    }
    if (mpItemModel != NULL) {
        g_env_light.setLightTevColorType(mpItemModel, &tevStr);
        mDoExt_modelEntryDL(mpItemModel);
    }
    shadowDraw();
    switch (mType) {
    case 0:
        dSnap_RegistFig(DSNAP_TYPE_UNK52, this, 1.0f, 1.0f, 1.0f);
        break;
    case 1:
        dSnap_RegistFig(DSNAP_TYPE_UNK51, this, 1.0f, 1.0f, 1.0f);
        break;
    }
    if (l_HIO.mChild[mType].mPrm.mDebug) {
        GXColor unused[] = {
            {0xFF, 0x00, 0x00, 0x80},
            {0x00, 0x00, 0xFF, 0x80},
            {0x00, 0xFF, 0x00, 0x80},
            {0xFF, 0x00, 0x00, 0x80},
            {0x00, 0xFF, 0x00, 0x80},
            {0xFF, 0x00, 0x00, 0x80},
            {0x00, 0xFF, 0x00, 0x80},
            {0xFF, 0xFF, 0x00, 0x80},
        }; // Unused colors, needed for the .rodata section to match.

        cXyz pos = mMoveTarget;
        pos.y = eyePos.y;
        if (mStaff != 0) {
            pos = mTargetPos;
            pos.y = eyePos.y;
        } else {
            pos = mTargetPos;
            pos.y = eyePos.y;
        }
        pos = current.pos;
        pos.y = eyePos.y;
    }
    return TRUE;
}

/* 0000728C-0000756C       .text _execute__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::_execute() {
    if (!mInitialized) {
        mHomePos = current.pos;
        mHomeAngle = current.angle;
        mInitialized = 1;
    }
    m_jnt.setParam(
        l_HIO.mChild[mType].mPrm.mMaxBackBoneX, l_HIO.mChild[mType].mPrm.mMaxBackBoneY,
        l_HIO.mChild[mType].mPrm.mMinBackBoneX, l_HIO.mChild[mType].mPrm.mMinBackBoneY,
        l_HIO.mChild[mType].mPrm.mMaxHeadX, l_HIO.mChild[mType].mPrm.mMaxHeadY,
        l_HIO.mChild[mType].mPrm.mMinHeadX, l_HIO.mChild[mType].mPrm.mMinHeadY,
        l_HIO.mChild[mType].mPrm.mMaxTurnStep);
    if (mHidden && demoActorID == 0) {
        return TRUE;
    }
    mStopMove = 0;
    mHidden = 0;
    partner_srch();
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == 0) {
            staff = isEventEntry();
        }
        if (staff >= 0) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        if (!mStopMove) {
            ko_nMove();
            fopAcM_posMoveF(this, mStts.GetCCMoveP());
        }
        cM3dGPla* pla = dComIfG_Bgsp()->GetTriPla(mObjAcch.m_gnd);
        if (pla != NULL) {
            mMoveDir = pla->mNormal;
        }
        if (!mFreezeAngle) {
            mModelAngle = current.angle;
            shape_angle = mModelAngle;
        }
#if VERSION > VERSION_DEMO
        if ((current.pos - mTargetPos).absXZ() > 3000.0f) {
            fopAcM_delete(this);
            return TRUE;
        }
#endif
    }
    eventOrder();
    setMtx(false);
    if (!mDemo) {
        setCollision(30.0f, 80.0f);
    }
    return TRUE;
}

/* 0000756C-000075F0       .text _delete__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::_delete() {
    dComIfG_resDelete(&mPhs, "Ko");
#if VERSION > VERSION_DEMO
    if (heap != NULL)
#endif
    {
        if (mpMorf != NULL) {
            mpMorf->stopZelAnime();
        }
        if (mpHeadMorf != NULL) {
            mpHeadMorf->stopZelAnime();
        }
        if (mpBalloonMorf != NULL) {
            mpBalloonMorf->stopZelAnime();
        }
    }
    mRippleCallback.end();
#if VERSION == VERSION_DEMO
    l_HIO.removeHIO();
#endif
    return TRUE;
}

/* 000075F0-00007610       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* i_this) {
    return ((daNpc_Ko1_c*)i_this)->CreateHeap();
}

/* 00007610-00007730       .text _create__11daNpc_Ko1_cFv */
cPhs_State daNpc_Ko1_c::_create() {
    fopAcM_ct_Retail(this, daNpc_Ko1_c);
    static u32 a_size_tbl[] = {0x272E0, 0x272E0};
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Ko");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    u32 prm = fopAcM_GetParam(this) & 0xFF;
    if (!charDecide(prm)) {
        return cPhs_ERROR_e;
    }
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("こども");
    fopAcM_ct(this, daNpc_Ko1_c);
#endif
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_size_tbl[mType])) {
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 120.0f, 50.0f);
    if (!createInit()) {
        return cPhs_ERROR_e;
    }
    return phase;
}

/* 00007C2C-00007E9C       .text create_Anm__11daNpc_Ko1_cFv */
J3DModelData* daNpc_Ko1_c::create_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ko", dRes_ID_KO_BDL_KO_e);
    JUT_ASSERT(DEMO_SELECT(0x107E, 0x10A6), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(
        a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Ko", dRes_ID_KO_BCK_KO_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x15021222);
    if (mpMorf == NULL) {
        return NULL;
    }
    if (mpMorf->getModel() == NULL) {
        mpMorf = NULL;
        return NULL;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x1092, 0x10BA), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x1095, 0x10BD), m_bbone_jnt_num >= 0);
    m_armR2_jnt_num = a_mdl_dat->getJointName()->getIndex("armR2");
    JUT_ASSERT(DEMO_SELECT(0x1098, 0x10C0), m_armR2_jnt_num >= 0);
    return a_mdl_dat;
}

/* 00007E9C-00008090       .text create_hed_Anm__11daNpc_Ko1_cFv */
J3DModelData* daNpc_Ko1_c::create_hed_Anm() {
    static int a_hed_bdl_resID_tbl[] = {
        dRes_ID_KO_BDL_KOHEAD01_e,
        dRes_ID_KO_BDL_KOHEAD02_e,
    };
    static int a_hed_bck_resID_tbl[] = {
        dRes_ID_KO_BCK_KOHEAD01_WAIT01_e,
        dRes_ID_KO_BCK_KOHEAD02_WAIT01_e,
    };
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ko", a_hed_bdl_resID_tbl[mType]);
    JUT_ASSERT(DEMO_SELECT(0x10B0, 0x10D8), a_mdl_dat != 0);
    mpHeadMorf = new mDoExt_McaMorf(
        a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Ko", a_hed_bck_resID_tbl[mType]),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
    if (mpHeadMorf == NULL) {
        return NULL;
    }
    if (mpHeadMorf->getModel() == NULL) {
        mpHeadMorf = NULL;
        return NULL;
    }
    if (mType == 0) {
        m_hed_2_jnt_num = a_mdl_dat->getJointName()->getIndex("head2");
        JUT_ASSERT(DEMO_SELECT(0x10C5, 0x10ED), m_hed_2_jnt_num >= 0);
    }
    return a_mdl_dat;
}

/* 00008090-000082A4       .text create_bln_Anm__11daNpc_Ko1_cFv */
J3DModelData* daNpc_Ko1_c::create_bln_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ko", dRes_ID_KO_BDL_KO_BALLOON_e);
    JUT_ASSERT(DEMO_SELECT(0x10D5, 0x10FD), a_mdl_dat != 0);
    mpBalloonMorf = new mDoExt_McaMorf(
        a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Ko", dRes_ID_KO_BCK_BALLOON_SLEEP04_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000022);
    if (mpBalloonMorf == NULL) {
        return NULL;
    }
    if (mpBalloonMorf->getModel() == NULL) {
        mpBalloonMorf = NULL;
        return NULL;
    }
    m_bln_loc_jnt_num = a_mdl_dat->getJointName()->getIndex("balloon_loc");
    JUT_ASSERT(DEMO_SELECT(0x10E9, 0x1111), m_bln_loc_jnt_num >= 0);
    m_bln_jnt_num = a_mdl_dat->getJointName()->getIndex("ko_balloon");
    JUT_ASSERT(DEMO_SELECT(0x10EC, 0x1114), m_bln_jnt_num >= 0);
    return a_mdl_dat;
}

/* 000082A4-00008380       .text create_itm_Mdl__11daNpc_Ko1_cFv */
bool daNpc_Ko1_c::create_itm_Mdl() {
    if (mType != 1) {
        return true;
    }
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Ko", dRes_ID_KO_BDL_KOEDA_e);
    JUT_ASSERT(DEMO_SELECT(0x10FE, 0x1126), a_mdl_dat != 0);
    mpItemModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
    if (mpItemModel == NULL) {
        return false;
    }
    return true;
}

/* 00008380-00008604       .text CreateHeap__11daNpc_Ko1_cFv */
BOOL daNpc_Ko1_c::CreateHeap() {
    J3DModelData* a_mdl_dat = create_Anm();
    if (a_mdl_dat == NULL) {
        return FALSE;
    }
    J3DModelData* a_hed_dat = create_hed_Anm();
    if (a_hed_dat == NULL) {
        mpMorf = NULL;
        return FALSE;
    }
    s8 a_tex_pattern_num_tbl[] = {0, 0};
    mTexIndex = a_tex_pattern_num_tbl[mType];
    if (!iniTexPttrnAnm(false)) {
        mpMorf = NULL;
        mpHeadMorf = NULL;
        return FALSE;
    }
    if (mType == 0) {
        if (create_bln_Anm() == NULL) {
            mpMorf = NULL;
            mpHeadMorf = NULL;
            return FALSE;
        }
    }
    if (create_itm_Mdl()) {
        if (mType == 0) {
            for (u16 i = 0; i < a_hed_dat->getJointNum(); i++) {
                if (i == m_bln_loc_jnt_num) {
                    mpBalloonMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Bln);
                }
            }
            mpBalloonMorf->getModel()->setUserArea((u32)this);
            for (u16 i = 0; i < a_hed_dat->getJointNum(); i++) {
                if (i == m_hed_2_jnt_num) {
                    mpHeadMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Hed);
                }
            }
            mpHeadMorf->getModel()->setUserArea((u32)this);
        }
        for (u16 i = 0; i < a_mdl_dat->getJointNum(); i++) {
            if (i == m_hed_jnt_num || i == m_bbone_jnt_num) {
                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Ko1);
            }
        }
        mpMorf->getModel()->setUserArea((u32)this);
        mAcchCir.SetWall(30.0f, 30.0f);
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
        return TRUE;
    }
    mpMorf = NULL;
    mpHeadMorf = NULL;
    mpBalloonMorf = NULL;
    return FALSE;
}

/* 00008604-00008624       .text daNpc_Ko1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Ko1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Ko1_c*)i_this)->_create();
}

/* 00008624-00008644       .text daNpc_Ko1_Delete__FP11daNpc_Ko1_c */
static BOOL daNpc_Ko1_Delete(daNpc_Ko1_c* i_this) {
    return ((daNpc_Ko1_c*)i_this)->_delete();
}

/* 00008644-00008664       .text daNpc_Ko1_Execute__FP11daNpc_Ko1_c */
static BOOL daNpc_Ko1_Execute(daNpc_Ko1_c* i_this) {
    return ((daNpc_Ko1_c*)i_this)->_execute();
}

/* 00008664-00008684       .text daNpc_Ko1_Draw__FP11daNpc_Ko1_c */
static BOOL daNpc_Ko1_Draw(daNpc_Ko1_c* i_this) {
    return ((daNpc_Ko1_c*)i_this)->_draw();
}

/* 00008684-0000868C       .text daNpc_Ko1_IsDelete__FP11daNpc_Ko1_c */
static BOOL daNpc_Ko1_IsDelete(daNpc_Ko1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Ko1_Method = {
    (process_method_func)daNpc_Ko1_Create,
    (process_method_func)daNpc_Ko1_Delete,
    (process_method_func)daNpc_Ko1_Execute,
    (process_method_func)daNpc_Ko1_IsDelete,
    (process_method_func)daNpc_Ko1_Draw,
};

actor_process_profile_definition g_profile_NPC_KO1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_KO1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Ko1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_KO1_e,
    /* Actor SubMtd */ &l_daNpc_Ko1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_KO2 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_KO2_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Ko1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_KO2_e,
    /* Actor SubMtd */ &l_daNpc_Ko1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

inline daNpc_Ko1_HIO_c::~daNpc_Ko1_HIO_c() {}
