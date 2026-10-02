/**
 * d_a_npc_kk1.cpp
 * NPC - Mila (poor)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_kk1.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_tama.h"
#include "d/actor/d_a_swc00.h"
#include "d/actor/d_a_obj_roten.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_kankyo.h"
#include "d/d_path.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "d/d_particle_name.h"
#include "f_op/f_op_actor_mng.h"
#include "c/c_dylink.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "res/Object/Kk.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_counter.h"
#include <string.h>

static daNpc_Kk1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;

/* 000000EC-00000150       .text __ct__15daNpc_Kk1_HIO_cFv */
daNpc_Kk1_HIO_c::daNpc_Kk1_HIO_c() {
    static hio_prm_c a_prm_tbl = {
        0x2000, 0x251C, (s16)0xE002, (s16)0xDAE4,
        0, 0x2AF8, 0, (s16)0xD508,
        0x640, 0x4B0,
        150.0f,
        0,
        0xD2,
        0x28, 0x5A, 0x1E, 0x14, 4, 0x1000,
        40.0f, 3.0f, 0.2f, 0.5f, 13.0f, 1.0f, 0.1f, 10.0f, 1.0f, 0.2f, 1000.0f,
    };
    memcpy(&mPrm, &a_prm_tbl, sizeof(mPrm));
    mNo = -1;
    mCount = -1;
}

daNpc_Kk1_HIO_c::~daNpc_Kk1_HIO_c() {}

static char* l_evn_tbl[] = {
    "run_start",
#if VERSION > VERSION_JPN
    "run_start_2",
#endif
    "catch",
    "get_empty_btl",
    "bye",
    "otoboke",
    "runaway",
    "bye_2",
};

/* 00000198-000001E4       .text nodeCB_Head__FP7J3DNodei */
static BOOL nodeCB_Head(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Kk1_c* npc = (daNpc_Kk1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_Head(node, model);
        }
    }
    return TRUE;
}

/* 000001E4-00000304       .text _nodeCB_Head__11daNpc_Kk1_cFP7J3DNodeP8J3DModel */
void daNpc_Kk1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(14.0f, 18.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    m750.x = mDoMtx_stack_c::get()[0][3];
    m750.y = mDoMtx_stack_c::get()[1][3];
    m750.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &m72C);
    mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000340-0000038C       .text nodeCB_BackBone__FP7J3DNodei */
static BOOL nodeCB_BackBone(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Kk1_c* npc = (daNpc_Kk1_c*)model->getUserArea();
        if (npc != NULL) {
            npc->_nodeCB_BackBone(node, model);
        }
    }
    return TRUE;
}

/* 0000038C-0000042C       .text _nodeCB_BackBone__11daNpc_Kk1_cFP7J3DNodeP8J3DModel */
void daNpc_Kk1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
    MTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 0000042C-0000044C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* i_this) {
    return ((daNpc_Kk1_c*)i_this)->CreateHeap();
}

/* 0000044C-000004EC       .text searchActor_SWC00__FPvPv */
static void* searchActor_SWC00(void* i_actor, void* i_data) {
    fopAc_ac_c* actor = (fopAc_ac_c*)i_actor;
    daNpc_Kk1_c* i_this = (daNpc_Kk1_c*)i_data;
    if (l_check_wrk < 20) {
        if (fopAcM_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_SWC00_e) {
            if (daSwc00_getType((swc00_class*)actor) == 0 && daSwc00_getSw1No((swc00_class*)actor) == i_this->getSWbit()) {
                l_check_inf[l_check_wrk] = actor;
                l_check_wrk++;
            }
        }
    }
    return NULL;
}

/* 000004EC-00000598       .text init_KK1_0__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::init_KK1_0() {
    if (dComIfGs_isEventBit(0x2D01)) {
        if (!dKy_daynight_check() || !dComIfGs_isEventBit(0xE08)) {
            set_action(&daNpc_Kk1_c::wait_action1, NULL);
            return true;
        }
    }
    return false;
}

/* 00000598-000007B0       .text createInit__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::createInit() {
    for (int i = 0; i < VERSION_SELECT(7, 7, 8, 8); i++) {
        mEventIds[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i]);
    }
    mEventCut.setActorInfo2("Kk1", this);
    mSwNo = (fopAcM_GetParam(this) >> 8) & 0xFF;
#if VERSION == VERSION_DEMO
    int weight = 0xFF;
#endif
    u8 path = (fopAcM_GetParam(this) >> 16) & 0xFF;
    if (path != 0xFF) {
        mPath.setInf(path, fopAcM_GetRoomNo(this), 1);
        if (mPath.getPath()) {
            fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
#if VERSION == VERSION_DEMO
            weight = 0xD9;
#endif
            set_pthPoint(0);
        } else {
            return false;
        }
    }
    if (mPath.isPath() == false) {
        return false;
    }
    attention_info.flags = 0xA;
    switch (mSubType) {
    case 0:
    default:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA9;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;
        break;
    }
    gravity = -4.5f;
    mAnmNo = 12;
    bool result;
    switch (mSubType) {
    case 0:
        result = init_KK1_0();
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
    mStts.Init(DEMO_SELECT(weight, 0xFF), 0xFF, this);
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

/* 000007B0-00000918       .text play_animation__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::play_animation() {
    u32 sound = 0;
    play_btp_anm();
    if (mObjAcch.ChkGroundHit()) {
        sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
    }
    mAnmEnd = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    if (mpMorf->getFrame() < mFrame) {
        mAnmEnd = 1;
    }
    mFrame = mpMorf->getFrame();
    cXyz offset;
    switch (mAnmNo) {
    case 8:
        if (mpMorf->checkFrame(4.0f)) {
            offset.set(0.0f, -50.0f, -15.0f);
            setBikon(offset);
        }
        break;
    case 9:
        if (mpMorf->checkFrame(4.0f)) {
            offset.set(0.0f, -50.0f, 0.0f);
            setBikon(offset);
        }
        break;
    }
    play_eff_anm();
}

/* 00000918-00000A98       .text setMtx__11daNpc_Kk1_cFb */
void daNpc_Kk1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(mModelAngle.x, mModelAngle.y, mModelAngle.z);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    cXyz offset(0.0f, 120.0f, 30.0f);
    mBckAnm.entry(mpEffModel->getModelData(), mBckFrame);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mDoMtx_stack_c::transM(offset.x, offset.y, offset.z);
    mpEffModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpEffModel->calc();
    mpEffModel->getModelData()->getJointNodePointer(0)->setMtxCalc(NULL);
    flwAse();
    setAttention(force);
}

/* 00000A98-00000AAC       .text bckResID__11daNpc_Kk1_cFi */
int daNpc_Kk1_c::bckResID(int index) {
    static const int a_resID_tbl[] = {
        dRes_ID_KK_BCK_KK_WAIT01_e,
        dRes_ID_KK_BCK_KK_WAIT02_e,
        dRes_ID_KK_BCK_KK_WAIT03_e,
        dRes_ID_KK_BCK_KK_WALK_e,
        dRes_ID_KK_BCK_KK_RUN_e,
        dRes_ID_KK_BCK_KK_TALK01_e,
        dRes_ID_KK_BCK_KK_TALK02_e,
        dRes_ID_KK_BCK_KK_TALK03_e,
        dRes_ID_KK_BCK_KK_FURIMUKI_e,
        dRes_ID_KK_BCK_KK_FURIMUKI02_e,
        dRes_ID_KK_BCK_KK_OJIGI_e,
        dRes_ID_KK_BCK_KK_TEBYOUSHI_e,
    };
    return a_resID_tbl[index];
}

/* 00000AAC-00000AC0       .text btpResID__11daNpc_Kk1_cFi */
int daNpc_Kk1_c::btpResID(int index) {
    static const int a_resID_tbl[] = {
        dRes_ID_KK_BTP_KK_e,
    };
    return a_resID_tbl[index];
}

/* 00000AC0-00000BC4       .text setBtp__11daNpc_Kk1_cFScb */
bool daNpc_Kk1_c::setBtp(s8 number, bool modify) {
    J3DModel* model = mpMorf->getModel();
    if (number < 0) {
        return false;
    }
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes(mArcName, btpResID(number));
    JUT_ASSERT(VERSION_SELECT(0x233, 0x233, 0x234, 0x234), a_btp != 0);
    mBtpNo = number;
    mBtpFrame = 0;
    mBtpTimer = 0;
    return mBtpAnm.init(model->getModelData(), a_btp, true, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, modify, 0) != 0;
}

/* 00000BC4-00000BE4       .text init_texPttrnAnm__11daNpc_Kk1_cFScb */
bool daNpc_Kk1_c::init_texPttrnAnm(s8 number, bool modify) {
    return setBtp(number, modify);
}

/* 00000BE4-00000C80       .text play_btp_anm__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::play_btp_anm() {
    u8 end = mBtpAnm.getBtpAnm()->getFrameMax();
    if (mBtpNo != 0 || cLib_calcTimer(&mBtpTimer) == 0) {
        mBtpFrame++;
        if (mBtpFrame >= end) {
            if (mBtpNo != 0) {
                mBtpFrame = end;
            } else {
                mBtpTimer = cLib_getRndValue(60, 90);
                mBtpFrame = 0;
            }
        }
    }
}

/* 00000C80-00000CFC       .text play_eff_anm__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::play_eff_anm() {
    s16 bpkMax = 0x1E;
    mBpkFrame++;
    if (mBpkFrame >= bpkMax) {
        mBpkFrame = bpkMax - 1;
    }
    int frameMax = mBtkAnm.getBtkAnm()->getFrameMax();
    mBtkFrame++;
    if (mBtkFrame >= (s16)frameMax) {
        mBtkFrame = frameMax - 1;
    }
    frameMax = mBckAnm.getBckAnm()->getFrameMax();
    mBckFrame++;
    if (mBckFrame >= (s16)frameMax) {
        mBckFrame = 0x3B;
    }
}

/* 00000CFC-00000DDC       .text setAnm_anm__11daNpc_Kk1_cFPQ211daNpc_Kk1_c9anm_prm_c */
void daNpc_Kk1_c::setAnm_anm(daNpc_Kk1_c::anm_prm_c* prm) {
    s8 number = prm->mAnm;
    if (number < 0 || mAnmNo == number) {
        return;
    }
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, bckResID(number), -1, mArcName);
    mAnmNo = prm->mAnm;
    delBikon();
    delAse();
    m7BE = 1;
    switch (mAnmNo) {
    case 1:
        setAse();
        mBckFrame = 0;
        mBtkFrame = 0;
        mBpkFrame = 0;
        m7BE = 0;
        break;
    }
    mAnmEnd = 0;
    mAnmRepeat = 0;
    mFrame = 0.0f;
}

/* 00000DDC-00000E4C       .text setAnm_NUM__11daNpc_Kk1_cFii */
void daNpc_Kk1_c::setAnm_NUM(int index, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {3, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {4, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {6, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {7, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {8, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {9, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {10, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {11, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {0, 0, 0.0f, 0.0f, J3DFrameCtrl::EMode_NONE},
        {0, 0, 0.0f, 0.0f, J3DFrameCtrl::EMode_NONE},
    };
    if (tex) {
        init_texPttrnAnm(a_anm_prm_tbl[index].mBtp, true);
    }
    setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 00000E4C-00000EB8       .text setAnm__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {3, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {-1, -1, 0.0f, 0.0f, J3DFrameCtrl::EMode_NULL},
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mState].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mState]);
}

/* 00000EB8-00000EBC       .text chngAnmTag__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::chngAnmTag() {}

/* 00000EBC-00000EC0       .text ctrlAnmTag__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::ctrlAnmTag() {}

/* 00000EC0-00000FA0       .text chngAnmAtr__11daNpc_Kk1_cFUc */
void daNpc_Kk1_c::chngAnmAtr(unsigned char attr) {
    switch (mCurrMsgNo) {
    case 0x1CAC: {
        int deleted;
        fopAc_ac_c* actor = searchByID(mPartnerId, &deleted);
        if (actor != NULL && deleted == 0) {
            mLookPos = actor->current.pos;
            mLookPos.y += 200.0f + l_HIO.mPrm.mAttentionOffsetY;
            m81E = 2;
            m7C4 = 1;
            m7A2 = l_HIO.mPrm.m22;
        }
        break;
    }
    }
    if (attr == mAnmAttr || attr > 0xD) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR();
}

/* 00000FA0-00001080       .text ctrlAnmAtr__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::ctrlAnmAtr() {
    switch (mAnmAttr) {
    case 8:
        if (mAnmEnd) {
            current.angle.y += 0x8000;
            setAnm_NUM(0, 1);
            mpMorf->setMorf(0.0f);
            mAnmAttr = 0;
        }
        break;
    case 0xB:
        if (mAnmEnd) {
            setAnm_NUM(6, 1);
            mAnmAttr = 6;
        }
        break;
    case 0xA:
        if (mAnmEnd) {
            setAnm_NUM(0, 1);
            mAnmAttr = 0;
        }
        break;
    case 7:
        break;
    case 0xC:
        break;
    }
}

/* 00001080-000010E4       .text setAnm_ATR__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {2, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {3, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {4, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {6, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {7, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {8, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {9, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {10, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {10, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {11, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 000010E4-000011A0       .text anmAtr__11daNpc_Kk1_cFUs */
void daNpc_Kk1_c::anmAtr(unsigned short status) {
    switch (status) {
    case 6:
        if (mMsgAnmStarted == 0) {
            chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
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
        mMsgAnmStarted = 0;
        break;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

/* 000011A0-0000149C       .text next_msgStatus__11daNpc_Kk1_cFPUl */
u16 daNpc_Kk1_c::next_msgStatus(unsigned long* pMsg) {
    u16 result = fopMsgStts_MSG_CONTINUES_e;
    switch (*pMsg) {
    case 0x1C85:
        *pMsg = 0x1C86;
        break;
    case 0x1C86:
        *pMsg = 0x1CA7;
        break;
    case 0x1CA7:
        *pMsg = 0x1CA8;
        break;
    case 0x1C88:
        *pMsg = 0x1C89;
        break;
    case 0x1C89:
        *pMsg = 0x1C8A;
        break;
    case 0x1C8A:
        *pMsg = 0x1CA9;
        break;
    case 0x1C8B:
        *pMsg = 0x1CAC;
        m7B8 = 1;
        break;
    case 0x1C8D:
        *pMsg = 0x1C8E;
        break;
    case 0x1C8E:
        *pMsg = 0x1C8F;
        break;
    case 0x1C8F:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *pMsg = 0x1C90;
            break;
        case 1:
            *pMsg = 0x1CA5;
            mLookAngle++;
            break;
        }
        break;
    case 0x1CA5:
        *pMsg = 0x1C8F;
        break;
    case 0x1C90:
        *pMsg = 0x1C91;
        break;
    case 0x1C91:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *pMsg = 0x1C93;
            break;
        case 1:
            *pMsg = 0x1C92;
            mLookAngle++;
            break;
        }
        break;
    case 0x1C92:
        *pMsg = 0x1C91;
        break;
    case 0x1C93:
        *pMsg = 0x1C94;
        break;
    case 0x1C94:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *pMsg = 0x1C96;
            break;
        case 1:
            *pMsg = 0x1C95;
            mLookAngle++;
            break;
        }
        break;
    case 0x1C95:
        *pMsg = 0x1C96;
        break;
    case 0x1C96:
        *pMsg = 0x1C97;
        break;
    case 0x1C97:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *pMsg = 0x1C98;
            break;
        case 1:
            *pMsg = 0x1C9B + (mLookAngle > 1 ? -1 : 0);
            break;
        }
        break;
    case 0x1C98:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *pMsg = 0x1C99;
            break;
        case 1:
            result = fopMsgStts_MSG_ENDS_e;
            break;
        }
        break;
    case 0x1C99:
        switch (mpCurrMsg->mSelectNum) {
        case 0:
            *pMsg = 0x1C9D + (mLookAngle > 1 ? -1 : 0);
            break;
        case 1:
            *pMsg = 0x1C9A;
            break;
        }
        break;
    case 0x1C9B:
        *pMsg = 0x1C9D;
        break;
    case 0x1C9D:
        *pMsg = 0x1C9E;
        break;
    case 0x1C9E:
        *pMsg = 0x1CAA;
        break;
    case 0x1CAA:
        *pMsg = 0x1C9F;
        break;
    case 0x1C9F:
        result = fopMsgStts_MSG_ENDS_e;
        break;
    case 0x1CA1:
        *pMsg = 0x1CA2;
        break;
    case 0x1CA2:
        *pMsg = 0x1CA3;
        break;
    case 0x1CA3:
        *pMsg = 0x1CA4;
        break;
    case 0x1CA4:
        *pMsg = 0x1CAB;
        break;
    default:
        result = fopMsgStts_MSG_ENDS_e;
        break;
    }
    return result;
}

/* 0000149C-00001534       .text getMsg_KK1_0__11daNpc_Kk1_cFv */
u32 daNpc_Kk1_c::getMsg_KK1_0() {
    if (dKy_daynight_check() == 1) {
        if (m7BA != 0) {
            m7BA = 0;
            return 0x1CA1;
        }
        return 0x1C8B;
    }
    if (!dComIfGs_isEventBit(0xE08)) {
        return 0x1C88 + (dComIfGs_isEventBit(0xE10) ? -1 : 0);
    }
    return 0x1C85;
}

/* 00001534-00001570       .text getMsg__11daNpc_Kk1_cFv */
u32 daNpc_Kk1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0:
        msg = getMsg_KK1_0();
        break;
    }
    return msg;
}

/* 00001570-000015FC       .text eventOrder__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (mOrder >= 3) {
        mEventIndex = mOrder - 3;
        fopAcM_orderOtherEventId(this, mEventIds[mEventIndex], 0xFF, 0xFFFF, 0, 1);
    }
}

/* 000015FC-000016DC       .text checkOrder__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEventIds[mEventIndex]) && mOrder >= 3) {
            switch (mEventIndex) {
#if VERSION > VERSION_JPN
            case 4:
                setAnm_NUM(0, 1);
                break;
            case 0:
            case 1:
            case 2:
            case 3:
            case 5:
            case 6:
            case 7:
                break;
#else
            case 3:
                setAnm_NUM(0, 1);
                break;
            case 0:
            case 1:
            case 2:
            case 4:
            case 5:
            case 6:
                break;
#endif
            }
            mOrder = 0;
            mAnmAttr = 0xFF;
            mAnmTag = 0xFF;
        }
    } else if (checkCommandTalk()) {
        if (mOrder == 1 || mOrder == 2) {
            mOrder = 0;
            m7C3 = 1;
        }
    }
}

/* 000016DC-00001774       .text chk_talk__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::chk_talk() {
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mItemNo = dComIfGp_event_getPreItemNo();
            return true;
        }
        return false;
    }
    mItemNo = 0xFF;
    return true;
}

/* 00001774-000017B4       .text chk_parts_notMov__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::chk_parts_notMov() {
    return mPreviousAngles.y != m_jnt.getHead_y() || mPreviousAngles.z != m_jnt.getBackbone_y() || mPreviousAngles.x != current.angle.y;
}

/* 000017B4-00001808       .text searchByID__11daNpc_Kk1_cFUiPi */
fopAc_ac_c* daNpc_Kk1_c::searchByID(fpc_ProcID id, int* deleted) {
    fopAc_ac_c* actor = NULL;
    *deleted = FALSE;
    if (!fopAcM_SearchByID(id, &actor)) {
        *deleted = TRUE;
    }
    return actor;
}

/* 00001808-000018B8       .text partner_search_sub__11daNpc_Kk1_cFPFPvPv_Pv */
bool daNpc_Kk1_c::partner_search_sub(void* (*judge)(void*, void*)) {
    bool result = false;
    mPartnerId = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < ARRAY_SSIZE(l_check_inf); i++) {
        l_check_inf[i] = NULL;
    }
    fpcM_Search(judge, this);
    if (l_check_wrk != 0) {
        mPartnerId = fopAcM_GetID(l_check_inf[0]);
        result = true;
    }
    return result;
}

/* 000018B8-00001924       .text partner_search__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::partner_search() {
    bool result = false;
    if (mActionState == 1) {
        switch (mSubType) {
        case 0:
            result = partner_search_sub(searchActor_SWC00);
            break;
        default:
            result = true;
            break;
        }
        if (result) {
            mActionState++;
        }
    }
}

/* 00001924-00001B10       .text lookBack__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::lookBack() {
    mPreviousAngles.y = m_jnt.getHead_y();
    mPreviousAngles.z = m_jnt.getBackbone_y();
    mPreviousAngles.x = current.angle.y;
    cXyz target;
    cXyz base = current.pos;
    base.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* targetP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = m7C4;
    switch ((s8)m81E) {
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
        angle = m7AC;
        break;
    case 4: {
        int deleted;
        fopAc_ac_c* actor = searchByID(m700, &deleted);
        if (actor != NULL && deleted == 0) {
            mLookPos = actor->current.pos;
            mLookPos.y = actor->eyePos.y;
            target = mLookPos;
            targetP = &target;
        }
        break;
    }
    case 5:
        kyorokyoro();
        target = mLookPos;
        targetP = &target;
        break;
    }
    m_jnt.lookAtTarget_2(&current.angle.y, targetP, base, angle, l_HIO.mPrm.mTurnSpeed, noTurn);
}

/* 00001B10-00001B90       .text chkAttention__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 00001B90-00001BE8       .text setAttention__11daNpc_Kk1_cFb */
void daNpc_Kk1_c::setAttention(bool force) {
    attention_info.position.set(current.pos.x, current.pos.y + l_HIO.mPrm.mAttentionOffsetY, current.pos.z);
    if (!mUpdateEye && !force) {
        return;
    }
    eyePos.set(m72C.x, m72C.y, m72C.z);
}

/* 00001BE8-00001C70       .text decideType__11daNpc_Kk1_cFi */
bool daNpc_Kk1_c::decideType(int) {
    if (mType > 0) {
        return true;
    }
    mType = 1;
    mSubType = 0;
    strcpy(mArcName, "Kk");
    bool result = false;
    if (mType != -1 && mSubType != -1) {
        result = true;
    }
    return result;
}

/* 00001C70-00001D10       .text cut_init_RUN_START__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_RUN_START(int) {
    int deleted;
    fopAc_ac_c* a_actor = searchByID(mPartnerId, &deleted);
    JUT_ASSERT(VERSION_SELECT(0x54D, 0x54D, 0x54F, 0x54F), a_actor != 0);
    dComIfGp_event_setItemPartner(a_actor);
    mPath.nextIdxAuto();
}

/* 00001D10-00001DD0       .text cut_move_RUN_START__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_RUN_START() {
    cXyz pos = mPath.getPoint(mPath.getIdx());
    s16 target = cLib_targetAngleY(&current.pos, &pos);
    cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
    if (current.angle.y == target) {
        daPy_getPlayerLinkActorClass()->cancelOriginalDemo();
        return true;
    }
    return false;
}

/* 00001DD0-00001E58       .text cut_init_RUN__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_RUN(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    m798 = -1;
    if (timer != NULL) {
        m798 = *timer;
    }
    setAnm_NUM(4, 1);
    m7B6 = 1;
    m815 = 2;
    m816 = 0;
}

/* 00001E58-00001EAC       .text cut_move_RUN__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_RUN() {
    bool result = event_move(false);
    if (m798 < 0) {
        return result;
    } else {
        return cLib_calcTimer(&m798) == 0;
    }
}

/* 00001EAC-00001F08       .text cut_init_CATCH_START__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_CATCH_START(int) {
    daPy_getPlayerLinkActorClass()->onPlayerNoDraw();
    setAnm_NUM(8, 1);
    mpMorf->setMorf(0.0f);
}

/* 00001F08-00001F2C       .text cut_move_CATCH_START__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_CATCH_START() {
    if (mAnmEnd) {
        mUpdateEye = FALSE;
        return true;
    }
    return false;
}

/* 00001F2C-00001FAC       .text cut_init_CATCH_END__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_CATCH_END(int) {
    daPy_getPlayerLinkActorClass()->offPlayerNoDraw();
    current.angle.y += 0x8000;
    setAnm_NUM(0, 1);
    mpMorf->setMorf(0.0f);
    m81E = 1;
    m7C4 = 0;
    m_jnt.setTrn();
}

/* 00001FAC-00001FB4       .text cut_move_CATCH_END__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_CATCH_END() {
    return true;
}

/* 00001FB4-000022BC       .text cut_init_TRN__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_TRN(int) {
    if (m816 != 5) {
        u8 origIdx = mPath.getIdx();
        if (origIdx >= 0x19 && origIdx <= 0x28) {
            u8 a_idx_tbl[] = {
                0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21,
                0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
            };
            mPath.setIdx(a_idx_tbl[origIdx - 0x19]);
            m81E = 0;
            m7C4 = 1;
            return;
        }

        u8 selfIdx;
        int maxPt;
        s16 half;
        s16 diff;
        u8 playerIdx;
        mPath.setNearPathIndx(&dComIfGp_getLinkPlayer()->current.pos, 100.0f);
        playerIdx = mPath.getIdx();
        mPath.setNearPathIndx(&current.pos, 100.0f);
        selfIdx = mPath.getIdx();
        maxPt = mPath.maxPoint();
        half = 0.5f + maxPt / 2.0f;
        if (selfIdx > playerIdx) {
            playerIdx += mPath.maxPoint();
        }
        diff = (s16)(playerIdx - selfIdx);
        if (diff > half) {
            diff -= maxPt;
        }
        mPath.setIdx(origIdx);

        bool playerCloser = false;
        if (diff == 0) {
            cXyz pos = mPath.getPoint(mPath.getIdx());
            f32 distNpc = (current.pos - pos).absXZ();
            f32 distPlayer = (dComIfGp_getLinkPlayer()->current.pos - pos).absXZ();
            playerCloser = distPlayer < distNpc;
        }
        if (diff > 0 || playerCloser) {
            mPath.decIdxAuto();
            mPath.decIdxAuto();
            mPath.turnDir();
        }
    }
    m81E = 0;
    m7C4 = 1;
}

/* 000022BC-00002364       .text cut_move_TRN__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_TRN() {
    cXyz pos = mPath.getPoint(mPath.getIdx());
    s16 target = cLib_targetAngleY(&current.pos, &pos);
    cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
    if (current.angle.y == target) {
        shape_angle.y = current.angle.y;
        return true;
    }
    return false;
}

/* 00002364-00002388       .text cut_init_BYE_START__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_BYE_START(int) {
    daPy_getPlayerLinkActorClass()->onPlayerNoDraw();
    m7BB = 1;
}

/* 00002388-00002390       .text cut_move_BYE_START__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_BYE_START() {
    return true;
}

/* 00002390-00002490       .text cut_init_BYE__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_BYE(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
#if VERSION > VERSION_DEMO
    int* delay = dComIfGp_evmng_getMyIntegerP(staff, "Delay");
#endif
    int* prm0 = dComIfGp_evmng_getMyIntegerP(staff, "prm_0");
    m798 = 30;
    if (timer != NULL) {
        m798 = *timer;
    }
#if VERSION > VERSION_DEMO
    m79A = -1;
    if (delay != NULL) {
        m79A = *delay;
    }
#endif
    m7B0 = 0;
    if (prm0 != NULL) {
        m7B0 = *prm0;
    }
    setAnm_NUM(4, 1);
    m815 = 2;
    m7B6 = 1;
}

/* 00002490-00002568       .text cut_move_BYE__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_BYE() {
    event_move(false);
#if VERSION > VERSION_DEMO
    if (m79A > 0 && cLib_calcTimer(&m79A) == 0) {
        daPy_getPlayerLinkActorClass()->setPlayerPosAndAngle(&daPy_getPlayerLinkActorClass()->current.pos, -0x3217);
        daPy_getPlayerLinkActorClass()->changeOriginalDemo();
        daPy_getPlayerLinkActorClass()->mDemo.setDemoMode(daPy_demo_c::DEMO_LDAM_e);
        daPy_getPlayerLinkActorClass()->mDemo.setParam0(0x3217);
    }
#endif
    if (cLib_calcTimer(&m798) == 0) {
        if (m7B0 == 0) {
            speedF = 0.0f;
        }
        return true;
    }
    return false;
}

/* 00002568-000025C8       .text cut_init_BYE_CONTINUE__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_BYE_CONTINUE(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    m798 = 30;
    if (timer != NULL) {
        m798 = *timer;
    }
}

/* 000025C8-00002620       .text cut_move_BYE_CONTINUE__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_BYE_CONTINUE() {
    event_move(false);
    if (cLib_calcTimer(&m798) == 0) {
        speedF = 0.0f;
        return true;
    }
    return false;
}

/* 00002620-000026BC       .text cut_init_BYE_END__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_BYE_END(int) {
    daPy_getPlayerLinkActorClass()->changeOriginalDemo();
    daPy_getPlayerLinkActorClass()->mDemo.setDemoMode(daPy_demo_c::DEMO_INIT_WAIT_e);
    s16 angle = cLib_targetAngleY(&daPy_getPlayerLinkActorClass()->current.pos, &current.pos);
    daPy_getPlayerLinkActorClass()->setPlayerPosAndAngle(&daPy_getPlayerLinkActorClass()->current.pos, angle);
    daPy_getPlayerLinkActorClass()->offPlayerNoDraw();
    m7C0 = 1;
}

/* 000026BC-000026C4       .text cut_move_BYE_END__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_BYE_END() {
    return true;
}

/* 000026C4-000026C8       .text cut_init_PLYER_TRN__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_PLYER_TRN(int) {}

/* 000026C8-000026D0       .text cut_move_PLYER_TRN__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_PLYER_TRN() {
    return true;
}

/* 000026D0-00002744       .text cut_init_OTOBOKE__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_OTOBOKE(int) {
    daPy_getPlayerLinkActorClass()->changeOriginalDemo();
    daPy_getPlayerLinkActorClass()->mDemo.setDemoMode(daPy_demo_c::DEMO_INIT_WAIT_e);
    daPy_getPlayerLinkActorClass()->setPlayerPosAndAngle(&daPy_getPlayerLinkActorClass()->current.pos, current.angle.y);
    m798 = 2;
}

/* 00002744-00002798       .text cut_move_OTOBOKE__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_OTOBOKE() {
    if (cLib_calcTimer(&m798) == 0) {
        daPy_getPlayerLinkActorClass()->cancelOriginalDemo();
        return true;
    }
    return false;
}

/* 00002798-000028A4       .text cut_init_PLYER_MOV__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_PLYER_MOV(int staff) {
#if VERSION == VERSION_DEMO
    s16 diff;
    int* prm0 = dComIfGp_evmng_getMyIntegerP(staff, "prm_0");
    int prm = 0;
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos);
    if (prm0 != NULL) {
        prm = *prm0;
    }
    if (prm != 0) {
        s16 d = cLib_targetAngleY(&daPy_getPlayerLinkActorClass()->current.pos, &current.pos);
        s16 s = (s16)(d - daPy_getPlayerLinkActorClass()->current.angle.y) > 0 ? 0x2800 : -0x2800;
        diff = s + daPy_getPlayerLinkActorClass()->current.angle.y;
        daPy_getPlayerLinkActorClass()->setPlayerPosAndAngle(&daPy_getPlayerLinkActorClass()->current.pos, diff);
        daPy_getPlayerLinkActorClass()->changeOriginalDemo();
        daPy_getPlayerLinkActorClass()->mDemo.setDemoMode(daPy_demo_c::DEMO_LDAM_e);
        daPy_getPlayerLinkActorClass()->changeDemoParam0(-diff);
        return;
    }
    diff = angle - current.angle.y;
    if (abs(diff) > 0x2000) {
        dComIfGp_evmng_setGoal(&dComIfGp_getLinkPlayer()->current.pos);
    } else {
        cXyz dst;
        cXyz offset(0.0f, 0.0f, 0.0f);
        diff = diff > 0 ? 0x2800 : -0x2800;
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(current.angle.y + diff);
        offset.z = 150.0f;
        mDoMtx_stack_c::multVec(&offset, &dst);
        dComIfGp_evmng_setGoal(&dst);
    }
#else
    s16 diff = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos) - current.angle.y;
    if (abs(diff) > 0x2000) {
        dComIfGp_evmng_setGoal(&dComIfGp_getLinkPlayer()->current.pos);
    } else {
        cXyz dst;
        cXyz offset(0.0f, 0.0f, 0.0f);
        diff = diff > 0 ? 0x2800 : -0x2800;
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(current.angle.y + diff);
        offset.z = 150.0f;
        mDoMtx_stack_c::multVec(&offset, &dst);
        dComIfGp_evmng_setGoal(&dst);
    }
#endif
}

/* 000028A4-000028AC       .text cut_move_PLYER_MOV__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_PLYER_MOV() {
    return true;
}

/* 000028AC-00002A40       .text cut_init_RUNAWAY_START__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_RUNAWAY_START(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    daPy_getPlayerLinkActorClass()->changeOriginalDemo();
    daPy_getPlayerLinkActorClass()->mDemo.setDemoMode(daPy_demo_c::DEMO_INIT_WAIT_e);
    s16 angle = cLib_targetAngleY(&daPy_getPlayerLinkActorClass()->current.pos, &current.pos);
    daPy_getPlayerLinkActorClass()->setPlayerPosAndAngle(&daPy_getPlayerLinkActorClass()->current.pos, angle);
    s16 playerAngle = cLib_targetAngleY(&current.pos, &daPy_getPlayerLinkActorClass()->current.pos);
    int shapeY = playerAngle + 0x8000;
    shape_angle.y = shapeY;
    m7BF = 1;
    speedF = 0.0f;
    m_jnt.setHead_y(0);
    m_jnt.setHead_x(0);
    m_jnt.setBackBone_y(0);
    m_jnt.setBackBone_x(0);
    m81E = 0;
    s16 angDiff = playerAngle - current.angle.y;
    if (abs(angDiff) > 0x3800) {
        current.angle.y = shapeY;
        setAnm_NUM(8, 1);
    } else {
        cXyz bikon(0.0f, -50.0f, 0.0f);
        m798 = 0x26;
        if (timer != NULL) {
            m798 = *timer;
        }
        current.angle.y = playerAngle;
        setAnm_NUM(0, 1);
        setBikon(bikon);
    }
}

/* 00002A40-00002AD0       .text cut_move_RUNAWAY_START__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_RUNAWAY_START() {
    if (mAnmNo == 8) {
        if (mAnmEnd) {
            current.angle.y = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos);
            setAnm_NUM(0, 1);
            mpMorf->setMorf(0.0f);
            return true;
        }
        return false;
    }
    return true;
}

/* 00002AD0-00002B08       .text cut_init_RUNAWAY_END__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::cut_init_RUNAWAY_END(int) {
    daPy_getPlayerLinkActorClass()->offPlayerNoDraw();
    m7B6 = 0;
    speedF = 0.0f;
    m7C0 = 1;
}

/* 00002B08-00002B10       .text cut_move_RUNAWAY_END__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::cut_move_RUNAWAY_END() {
    return true;
}

/* 00002B10-00002D98       .text privateCut__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {
        "RUN_START",
        "RUN",
        "CATCH_START",
        "CATCH_END",
        "TRN",
        "BYE_START",
        "BYE",
        "BYE_END",
        "PLYER_TRN",
        "OTOBOKE",
        "PLYER_MOV",
        "RUNAWAY_START",
        "RUNAWAY_END",
        "BYE_CONTINUE",
    };
    if (staff == -1) {
        return;
    }
    m814 = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 14, TRUE, FALSE);
    if ((s8)m814 == -1) {
        dComIfGp_evmng_cutEnd(staff);
        return;
    }
    if (dComIfGp_evmng_getIsAddvance(staff)) {
        switch ((s8)m814) {
        case 0:
            cut_init_RUN_START(staff);
            break;
        case 1:
            cut_init_RUN(staff);
            break;
        case 2:
            cut_init_CATCH_START(staff);
            break;
        case 3:
            cut_init_CATCH_END(staff);
            break;
        case 4:
            cut_init_TRN(staff);
            break;
        case 5:
            cut_init_BYE_START(staff);
            break;
        case 6:
            cut_init_BYE(staff);
            break;
        case 7:
            cut_init_BYE_END(staff);
            break;
        case 8:
            cut_init_PLYER_TRN(staff);
            break;
        case 9:
            cut_init_OTOBOKE(staff);
            break;
        case 10:
            cut_init_PLYER_MOV(staff);
            break;
        case 11:
            cut_init_RUNAWAY_START(staff);
            break;
        case 12:
            cut_init_RUNAWAY_END(staff);
            break;
        case 13:
            cut_init_BYE_CONTINUE(staff);
            break;
        }
    }
    bool done;
    switch ((s8)m814) {
    case 0:
        done = cut_move_RUN_START();
        break;
    case 1:
        done = cut_move_RUN();
        break;
    case 2:
        done = cut_move_CATCH_START();
        break;
    case 3:
        done = cut_move_CATCH_END();
        break;
    case 4:
        done = cut_move_TRN();
        break;
    case 5:
        done = cut_move_BYE_START();
        break;
    case 6:
        done = cut_move_BYE();
        break;
    case 7:
        done = cut_move_BYE_END();
        break;
    case 8:
        done = cut_move_PLYER_TRN();
        break;
    case 9:
        done = cut_move_OTOBOKE();
        break;
    case 10:
        done = cut_move_PLYER_MOV();
        break;
    case 11:
        done = cut_move_RUNAWAY_START();
        break;
    case 12:
        done = cut_move_RUNAWAY_END();
        break;
    case 13:
        done = cut_move_BYE_CONTINUE();
        break;
    default:
        done = true;
        break;
    }
    if (done) {
        dComIfGp_evmng_cutEnd(staff);
    }
}

/* 00002D98-00002DBC       .text endEvent__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
    mAnmTag = 0xFF;
}

/* 00002DBC-00002DF4       .text isEventEntry__11daNpc_Kk1_cFv */
int daNpc_Kk1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

/* 00002DF4-00002F80       .text event_proc__11daNpc_Kk1_cFi */
void daNpc_Kk1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIds[mEventIndex])) {
        switch (mEventIndex) {
        case 0:
#if VERSION > VERSION_JPN
        case 1:
#endif
            setStt(5);
            break;
        case VERSION_SELECT(1, 1, 2, 2):
            eventInfo.setEventId(-1);
            switch (mCurrMsgNo) {
            case 0x1C9A:
                mOrder = VERSION_SELECT(9, 9, 10, 10);
                m81E = 0;
                m7C4 = 1;
                break;
            case 0x1C98:
            case 0x1C9C:
                mOrder = VERSION_SELECT(6, 6, 7, 7);
                m81E = 1;
                m7C4 = 1;
                break;
            case 0x1C9F:
                setStt(6);
                mOrder = VERSION_SELECT(5, 5, 6, 6);
                m81E = 0;
                m7C4 = 1;
                break;
            }
            break;
        case VERSION_SELECT(2, 2, 3, 3):
            mOrder = 1;
            dComIfGs_onEventBit(0xE08);
            m7BA = 1;
            break;
        case VERSION_SELECT(3, 3, 4, 4):
        case VERSION_SELECT(5, 5, 6, 6):
        case VERSION_SELECT(6, 6, 7, 7):
            fopAcM_delete(this);
            break;
        }
        endEvent();
    } else if (!mEventCut.cutProc()) {
        privateCut(staff);
    }
}

/* 00002F80-00002FB8       .text checkCommandTalk__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::checkCommandTalk() {
    bool result = false;
    if (eventInfo.checkCommandTalk()) {
        result = true;
        if (m816 == 5 && m7A4 != 0) {
            result = false;
        }
    }
    return result;
}

/* 00002FB8-00003064       .text set_action__11daNpc_Kk1_cFM11daNpc_Kk1_cFPCvPvPv_iPv */
bool daNpc_Kk1_c::set_action(ActionFunc action, void* arg) {
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

/* 00003064-0000311C       .text setStt__11daNpc_Kk1_cFSc */
void daNpc_Kk1_c::setStt(signed char state) {
    s8 previous = mState;
    mState = state;
    switch (mState) {
    case 0:
        break;
    case 1:
    case 4:
    case 6:
    case 7:
        mOrder = 0;
        m7A4 = 0;
        speedF = 0.0f;
        break;
    case 2:
        mOrder = 0;
        mAnmAttr = 0xFF;
        mAnmTag = 0xFF;
        mMsgAnmStarted = 0;
        m7A2 = 0;
        mPreviousState = previous;
        break;
    case 3:
        mOrder = 0;
        break;
    case 5:
        mOrder = 0;
        m815 = 2;
        m816 = 0;
        m7B6 = 1;
        break;
    }
    setAnm();
}

/* 0000311C-000032D8       .text createTama__11daNpc_Kk1_cFf */
void daNpc_Kk1_c::createTama(float distance) {
    csXyz angle(0, 0, 0);
    cXyz target;
    cXyz pos = eyePos;
    pos.y = 15.0f + eyePos.y;
    target = dNpc_playerEyePos(-20.0f);
    f32 dist = (target - eyePos).absXZ();
    angle.y = cLib_targetAngleY(&pos, &target);
    angle.x = cLib_targetAngleX(&pos, &target);
    daTama_c* shot = (daTama_c*)fopAcM_fastCreate(fpcNm_TAMA_e, 0, &eyePos, fopAcM_GetRoomNo(this), &angle, NULL, -1, NULL, NULL);
    if (shot != NULL) {
        shot->setPartnerID(fopAcM_GetID(this));
        shot->setDis(distance);
        shot->setSpd(50.0f);
    }
}

/* 000032D8-0000345C       .text chk_areaIN__11daNpc_Kk1_cFf4cXyz */
bool daNpc_Kk1_c::chk_areaIN(float radius, cXyz pos) {
    f32 dist = (dComIfGp_getLinkPlayer()->current.pos - pos).absXZ();
    s16 diff = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos) - current.angle.y;
    f32 tamaRadius = radius;
    if (abs(diff) > 0x4E38) {
        tamaRadius = radius / 2.0f;
    }
    bool inside = dist < radius;
    if (inside && g_Counter.mCounter0 % 3 == 0) {
        createTama(tamaRadius);
    }
    return inside;
}

/* 0000345C-00003578       .text startEvent_check__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::startEvent_check() {
    if (chk_areaIN(l_HIO.mPrm.m50, current.pos)) {
        if (current.pos.abs(dComIfGp_getLinkPlayer()->current.pos) < 210.0f + REG9_F(0) || field_0x6ba != 0) {
            return true;
        }
    }
    return false;
}

/* 00003578-00003600       .text chkHitPlayer__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::chkHitPlayer() {
    bool result = false;
    if (mCyl.ChkCoHit()) {
        cCcD_Obj* obj = mCyl.GetCoHitObj();
        if (obj != NULL) {
            fopAc_ac_c* actor = dCc_GetAc(obj->GetAc());
            if (actor != NULL) {
                result = fopAcM_GetName(actor) == fpcNm_PLAYER_e;
            }
        }
    }
    return result;
}

/* 00003600-000036A8       .text set_pthPoint__11daNpc_Kk1_cFUc */
void daNpc_Kk1_c::set_pthPoint(unsigned char point) {
    bool has = mPath.getPath() != NULL;
    if (has) {
        mPath.setIdx(point);
        current.pos = mPath.getPoint(mPath.getIdx());
        if (mPath.nextIdx()) {
            cXyz pos = mPath.getPoint(mPath.getIdx());
            current.angle.y = cLib_targetAngleY(&current.pos, &pos);
        }
    }
}

/* 000036A8-00003940       .text event_move__11daNpc_Kk1_cFb */
bool daNpc_Kk1_c::event_move(bool useArg) {
    if (mPath.isPath() == false) {
        return true;
    }
    if (!dPath_ChkClose(mPath.getPath())) {
        return true;
    }
    if (m7B6) {
        if (mPath.chkPointPass(current.pos, (u8)(mPath.getDir() != 0))) {
            mPath.nextIdxAuto();
            if (useArg) {
                s8 arg = (s8)mPath.pointArg(mPath.getIdx());
                if (arg >= 0) {
                    arg++;
                }
                if (arg != 2 && arg != 3) {
                    m7B6 = 0;
                } else {
                    m815 = arg;
                    m816 = 0;
                    m7B6 = 1;
                }
            }
        }
    }
    cXyz pos = mPath.getPoint(mPath.getIdx());
    s16 target = cLib_targetAngleY(&current.pos, &pos);
    s16 prevAngle = current.angle.y;
    cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
    f32 targetSpeed;
    f32 playMul;
    f32 chase;
    int mode = m815;
    if (mode == 2) {
        if (m7B6 == 0) {
            targetSpeed = 0.0f;
        } else {
            targetSpeed = l_HIO.mPrm.m38;
        }
        playMul = speedF * l_HIO.mPrm.m40;
        chase = l_HIO.mPrm.m3C;
    } else {
        if (m7B6 == 0) {
            targetSpeed = 0.0f;
        } else {
            targetSpeed = l_HIO.mPrm.m44;
        }
        playMul = speedF * l_HIO.mPrm.m4C;
        chase = l_HIO.mPrm.m48;
    }
    cLib_chaseF(&speedF, targetSpeed, chase);
    f32 playSpeed = playMul < 0.5f ? 0.5f : playMul;
    mpMorf->setPlaySpeed(playSpeed);
    if ((s32)targetSpeed == 0) {
        current.angle.y = prevAngle;
        if ((s32)speedF == 0) {
            speedF = 0.0f;
            s8 arg = (s8)mPath.pointArg(mPath.getIdx());
            if (arg >= 0) {
                arg++;
            }
            m816 = arg;
            return true;
        }
    }
    return false;
}

/* 00003940-000039F0       .text kyoroPos__11daNpc_Kk1_cFi */
cXyz daNpc_Kk1_c::kyoroPos(int idx) {
    static Vec a_tgt_offst[] = {
        {0.0f, 0.0f, 0.0f},
        {-100.0f, 0.0f, 0.0f},
        {100.0f, 0.0f, 0.0f},
        {-100.0f, 0.0f, 100.0f},
        {100.0f, 0.0f, 100.0f},
        {0.0f, 0.0f, 100.0f},
        {-100.0f, 40.0f, 0.0f},
        {100.0f, 40.0f, 0.0f},
        {-100.0f, 40.0f, 100.0f},
        {100.0f, 40.0f, 100.0f},
        {0.0f, 40.0f, 100.0f},
        {-40.0f, 10.0f, 40.0f},
    };
    cXyz src(a_tgt_offst[idx].x, a_tgt_offst[idx].y, a_tgt_offst[idx].z);
    mDoMtx_stack_c::transS(eyePos.x, eyePos.y, eyePos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    cXyz dst;
    mDoMtx_stack_c::multVec(&src, &dst);
    return dst;
}

/* 000039F0-00003A84       .text kyorokyoro__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::kyorokyoro() {
    if (cLib_calcTimer(&m794) != 0) {
        mLookPos = kyoroPos(m796);
        return TRUE;
    }
    m796 = cLib_getRndValue(1, 10);
    m794 = l_HIO.mPrm.m1C;
    m792 = l_HIO.mPrm.m1E;
    return FALSE;
}

/* 00003A84-00003C9C       .text chk_attn__11daNpc_Kk1_cFv */
bool daNpc_Kk1_c::chk_attn() {
    f32 distance = (current.pos - dComIfGp_getLinkPlayer()->current.pos).absXZ();
    f32 height = current.pos.y - dComIfGp_getLinkPlayer()->current.pos.y;
    s16 angle = cLib_targetAngleY(&current.pos, &dComIfGp_getLinkPlayer()->current.pos) - current.angle.y;
    if (m81E == 1) {
        return distance < 200.0f && abs(angle) / 182.04445f < 90.0f && std::fabsf(height) < 300.0f;
    }
    return distance < 200.0f && abs(angle) / 182.04445f < 60.0f && std::fabsf(height) < 300.0f;
}

/* 00003C9C-00003D9C       .text setBikon__11daNpc_Kk1_cF4cXyz */
void daNpc_Kk1_c::setBikon(cXyz offset) {
    delBikon();
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    cXyz pos;
    mDoMtx_stack_c::multVec(&offset, &pos);
#if VERSION == VERSION_DEMO
    mpBikon = dComIfGp_particle_set(dPa_name::ID_IT_SN_PF_BIKON00, &pos);
    if (mpBikon != NULL) {
#else
    JPABaseEmitter* emitter = dComIfGp_particle_set(dPa_name::ID_IT_SN_PF_BIKON00, &pos);
    if (emitter != NULL) {
#endif
        mDoAud_seStart(JA_SE_CM_CMN_NOTICE, &current.pos);
    }
}

/* 00003D9C-00003DA0       .text delBikon__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::delBikon() {
#if VERSION == VERSION_DEMO
    if (mpBikon != NULL) {
        mpBikon->becomeInvalidEmitter();
        mpBikon = NULL;
    }
#endif
}

/* 00003DA0-00003E18       .text setAse__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::setAse() {
    delAse();
    mpAse = dComIfGp_particle_set(dPa_name::ID_IT_SN_MURU_ASE00, &current.pos, &current.angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
}

/* 00003E18-00003EA4       .text flwAse__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::flwAse() {
    if (mpAse != NULL) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
        cXyz pos;
        mDoMtx_stack_c::multVecZero(&pos);
        mpAse->setGlobalTranslation(pos.x, pos.y, pos.z);
    }
}

/* 00003EA4-00003ED0       .text delAse__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::delAse() {
    if (mpAse != NULL) {
        mpAse->becomeInvalidEmitter();
        mpAse = NULL;
    }
}

/* 00003ED0-0000415C       .text wait_1__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::wait_1() {
    cXyz pos;
    if (m7C3) {
        if (chk_talk()) {
            setStt(2);
            setAnm_NUM(0, 1);
            m81E = 1;
            m7C4 = 0;
            m_jnt.setTrn();
        }
        return TRUE;
    }
    mOrder = 2;
    m81E = 0;
    m7C4 = 1;
    if (m7B6) {
        pos = mPath.getPoint(mPath.getIdx());
        s16 target = cLib_targetAngleY(&current.pos, &pos);
        cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
        if (abs((s16)(target - current.angle.y)) < 0x1800) {
            setStt(3);
            m7B7 = 0;
        }
        return TRUE;
    }
    if (mAnmNo == 0xB) {
        if (mAnmEnd) {
            setAnm_NUM(0, 1);
            m7A4 = cLib_getRndValue<int>(0x3C, 0x1E);
            m7B7 = 1;
        }
        return TRUE;
    }
    cXyz offset(-100.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(mInitialAngle.y);
    mDoMtx_stack_c::multVec(&offset, &pos);
    s16 target = cLib_targetAngleY(&current.pos, &pos);
    cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
    s16 diff = target - current.angle.y;
    if (!m7B7) {
        if (diff == 0) {
            setAnm_NUM(0xB, 1);
        }
        return TRUE;
    }
    if (cLib_calcTimer(&m7A4) == 0) {
        if (mPath.getPath() && mPath.maxPoint() > 2 && daObj_Roten_c::getCreateCount() > 1) {
            m7B6 = 1;
            return TRUE;
        }
        m7B7 = 0;
    }
    return TRUE;
}

/* 0000415C-0000449C       .text walk_1__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::walk_1() {
    s16 prevAngle;
    cXyz pos = mPath.getPoint(mPath.getIdx());
    if (dPath_ChkClose(mPath.getPath())) {
        return TRUE;
    }
    f32 dist = (current.pos - pos).absXZ();
    if (m7B6 && dist < l_HIO.mPrm.m28) {
        bool atEnd = mPath.nextIdxAuto() == 0;
        if (!atEnd) {
            if (mPath.getDir() != 0 && mPath.getIdx() >= daObj_Roten_c::getCreateCount()) {
                mPath.decIdx();
                mPath.decIdx();
                atEnd = true;
            }
        }
        if (atEnd) {
            m7B6 = 0;
            mPath.turnDir();
        }
    }
    s16 target = cLib_targetAngleY(&current.pos, &pos);
    prevAngle = current.angle.y;
    cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
    f32 targetSpeed = l_HIO.mPrm.m2C;
    if (!m7B6 || m7C3 || chk_attn()) {
        targetSpeed = 0.0f;
    }
    cLib_chaseF(&speedF, targetSpeed, l_HIO.mPrm.m30);
    f32 playMul = speedF * l_HIO.mPrm.m34;
    f32 playSpeed = playMul < 0.5f ? 0.5f : playMul;
    mpMorf->setPlaySpeed(playSpeed);
    if ((s32)targetSpeed == 0) {
        current.angle.y = prevAngle;
        if ((s32)speedF == 0) {
            if (m7C3) {
                if (chk_talk()) {
                    setStt(1);
                    setAnm_NUM(0, 1);
                    m81E = 1;
                    m7C4 = 0;
                    m_jnt.setTrn();
                }
                return TRUE;
            }
            if (!m7B6) {
                setStt(1);
                m7B7 = 0;
                m7B6 = 0;
                return TRUE;
            }
            setStt(7);
            m7B7 = 0;
            m7B6 = 0;
            return TRUE;
        }
    }
    mOrder = 2;
    m81E = 0;
    m7C4 = 1;
    return TRUE;
}

/* 0000449C-00004614       .text wait_2__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::wait_2() {
    cLib_addCalcAngleS(&current.angle.y, mInitialAngle.y, 4, 0x800, 0x80);
    if (m7C3) {
        if (chk_talk()) {
            setStt(2);
            m81E = 1;
            m7C4 = 0;
            m7C5 = 0;
            m_jnt.setTrn();
        }
        return TRUE;
    }
    m81E = 0;
    m7C4 = 1;
#if VERSION > VERSION_JPN
    if (mOrder == 3 || mOrder == 4) {
        return TRUE;
    }
    if (m7B8 && mSwNo != 0xFF && dComIfGs_isSwitch(mSwNo, current.roomNo)) {
        int deleted;
        fopAc_ac_c* actor = searchByID(mPartnerId, &deleted);
        if (actor != NULL && deleted == 0) {
            s16 diff = cLib_targetAngleY(&actor->current.pos, &dComIfGp_getLinkPlayer()->current.pos) - actor->current.angle.y;
            if (abs(diff) < 0x4000) {
                mOrder = 4;
            } else {
                mOrder = 3;
            }
            mStts.SetWeight(0xD9);
            return TRUE;
        }
    }
#else
    if (mOrder == 3) {
        return TRUE;
    }
    if (m7B8 && mSwNo != 0xFF && dComIfGs_isSwitch(mSwNo, fopAcM_GetRoomNo(this))) {
        mOrder = 3;
#if VERSION > VERSION_DEMO
        mStts.SetWeight(0xD9);
#endif
        return TRUE;
    }
#endif
    mOrder = 2;
    return TRUE;
}

/* 00004614-0000466C       .text init_CMT_WAI__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::init_CMT_WAI() {
    m7A4 = cLib_getRndValue(0x5A, 0xB4);
    m81E = 5;
    m7C4 = 1;
    setAnm_NUM(0, 1);
}

/* 0000466C-00004754       .text move_CMT_WAI__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::move_CMT_WAI() {
    if (cLib_calcTimer(&m7A4) == 0) {
        if (mOrder != 1 && mOrder < 3 && chk_areaIN(l_HIO.mPrm.m50, current.pos)) {
            mOrder = VERSION_SELECT(7, 7, 8, 8);
        } else {
            m816 = 0;
            setAnm_NUM(3, 1);
            m7B6 = 1;
        }
    } else if (mOrder != 1 && mOrder < 3) {
        if (startEvent_check()) {
            mOrder = VERSION_SELECT(8, 8, 9, 9);
        }
    }
}

/* 00004754-000047D4       .text init_CMT_TRN__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::init_CMT_TRN() {
    m7AA = current.angle.y;
    m7A4 = cLib_getRndValue(0x5A, 0xB4);
    m798 = l_HIO.mPrm.m20;
    m794 = l_HIO.mPrm.m1C;
    m792 = l_HIO.mPrm.m1E;
    m81E = 0;
    m7C4 = 1;
    setAnm_NUM(0, 1);
}

/* 000047D4-00004A14       .text move_CMT_TRN__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::move_CMT_TRN() {
    s16 targetAngle = m7AA + 0x8000;
    s16 prevAngle = current.angle.y;
    if (cLib_calcTimer(&m798) != 0) {
        if (mOrder != 1 && mOrder < 3) {
            if (startEvent_check()) {
                mOrder = VERSION_SELECT(8, 8, 9, 9);
            }
        }
    } else if (m7A4 == 0) {
        cXyz pos = mPath.getPoint(mPath.getIdx());
        targetAngle = cLib_targetAngleY(&current.pos, &pos);
        cLib_addCalcAngleS(&current.angle.y, targetAngle, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
        if (mOrder != 1 && mOrder < 3) {
            if (startEvent_check()) {
                mOrder = VERSION_SELECT(8, 8, 9, 9);
            } else if (current.angle.y == targetAngle) {
                m816 = 0;
                setAnm_NUM(3, 1);
                m7B6 = 1;
            }
        }
    } else {
        cLib_addCalcAngleS(&current.angle.y, targetAngle, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
        if (current.angle.y == targetAngle) {
            if (current.angle.y != prevAngle) {
                m81E = 5;
                m7C4 = 1;
            }
            if (cLib_calcTimer(&m7A4) == 0) {
                if (mOrder != 1 && mOrder < 3) {
                    if (chk_areaIN(l_HIO.mPrm.m50, current.pos)) {
                        mOrder = VERSION_SELECT(7, 7, 8, 8);
                    }
                }
                m81E = 0;
                m7C4 = 1;
            }
        }
        if (mOrder != 1 && mOrder < 3) {
            if (startEvent_check()) {
                mOrder = VERSION_SELECT(8, 8, 9, 9);
            }
        }
    }
}

/* 00004A14-00004A84       .text init_CMT_PCK__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::init_CMT_PCK() {
    setAnm_NUM(1, 1);
    m7A4 = l_HIO.mPrm.m1A;
    m81E = 0;
    m7C4 = 1;
    mEventIndex = VERSION_SELECT(1, 1, 2, 2);
    eventInfo.setEventId(mEventIds[mEventIndex]);
}

/* 00004A84-00004C34       .text move_CMT_PCK__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::move_CMT_PCK() {
    if (m7A4 == 0) {
        cXyz pos = mPath.getPoint(mPath.getIdx());
        s16 target = cLib_targetAngleY(&current.pos, &pos);
        cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
        if (mOrder != 1 && mOrder < 3) {
            if (startEvent_check()) {
                mOrder = VERSION_SELECT(8, 8, 9, 9);
            } else if (current.angle.y == target) {
                m816 = 0;
                setAnm_NUM(3, 1);
                m7B6 = 1;
            }
        }
    } else if (!m7C3) {
        if (mOrder != 1 && mOrder < 3 && chkHitPlayer()) {
            mOrder = 1;
        } else if (cLib_calcTimer(&m7A4) == 0) {
            setAnm_NUM(0, 1);
            eventInfo.setEventId(-1);
            mOrder = 0;
        } else {
            mDoAud_seStart(JA_SE_CM_KK_HEARTBEAT, &current.pos);
            mOrder = 2;
        }
    }
}

/* 00004C34-00004DD0       .text cmmt_1__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::cmmt_1() {
    switch (m816) {
    case 1:
        move_CMT_WAI();
        return TRUE;
    case 4:
        move_CMT_TRN();
        return TRUE;
    case 5:
        move_CMT_PCK();
        return TRUE;
    case 0:
    case 2:
    case 3:
        break;
    }

    if (mOrder != 1 && mOrder < 3 && mAnmNo != 1) {
        if (startEvent_check()) {
            mOrder = VERSION_SELECT(8, 8, 9, 9);
        }
    }
    m81E = 0;
    m7C4 = 1;
    if (event_move(true)) {
        if (mOrder == 1 || mOrder >= 3) {
            return TRUE;
        }
        switch (m816) {
        case 1:
            init_CMT_WAI();
            break;
        case 4:
            init_CMT_TRN();
            break;
        case 5:
            init_CMT_PCK();
            break;
        case 0:
        case 2:
        case 3:
        default:
            m816 = 0;
            setAnm_NUM(3, 1);
            m7B6 = 1;
            break;
        }
    }
    if (mOrder == 1 || mOrder >= 3) {
        return TRUE;
    }
    mOrder = 0;
    return TRUE;
}

/* 00004DD0-00004F74       .text wait_3__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::wait_3() {
    f32 dist = (current.pos - dComIfGp_getLinkPlayer()->current.pos).absXZ();
    m7C5 = dist > 300.0f;
    if (m7C5) {
        cLib_addCalcAngleS(&current.angle.y, mInitialAngle.y, 4, 0x800, 0x80);
    }
    if (m7C3) {
        if (chk_talk()) {
            setStt(2);
            m81E = 1;
            m7C4 = 0;
            m7C5 = 0;
            m_jnt.setTrn();
        }
        return TRUE;
    }
    m81E = 0;
    m7C4 = 1;
    if (chk_attn()) {
        m81E = 1;
    }
    if (mOrder != 1 && mOrder < 3) {
        mOrder = 2;
    }
    return TRUE;
}

/* 00004F74-00005170       .text wait_4__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::wait_4() {
    f32 dist = (current.pos - dComIfGp_getLinkPlayer()->current.pos).absXZ();
    if (m7C3) {
        if (chk_talk()) {
            setStt(2);
            setAnm_NUM(0, 1);
            m81E = 1;
            m7C4 = 0;
            m_jnt.setTrn();
        }
        return TRUE;
    }
    mOrder = 2;
    m81E = 1;
    m7C4 = 0;
    m7B6 = dist > 300.0f;
    if (m7B6) {
        cXyz pos = mPath.getPoint(mPath.getIdx());
        s16 target = cLib_targetAngleY(&current.pos, &pos);
        cLib_addCalcAngleS(&current.angle.y, target, l_HIO.mPrm.m24, l_HIO.mPrm.m26, 0x80);
        if (abs((s16)(target - current.angle.y)) < 0x1800) {
            setStt(3);
            m7B7 = 0;
        }
        m81E = 0;
        m7C4 = 1;
    }
    return TRUE;
}

/* 00005170-000052B4       .text talk_1__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::talk_1() {
    BOOL result = chk_parts_notMov();
    talk(1);
    if (mpCurrMsg == NULL) {
        return TRUE;
    }
    switch (mpCurrMsg->mStatus) {
    case 0x13:
        mItemNo = 0xFF;
        m7C3 = 0;
        setStt(mPreviousState);
        m79E = cLib_getRndValue<int>(15, 30);
        switch (mCurrMsgNo) {
        case DEMO_SELECT(0x1C88, 0x1CA9):
            dComIfGs_onEventBit(0xE10);
            break;
        case 0x1CAB:
            mOrder = VERSION_SELECT(6, 6, 7, 7);
            break;
        case 0x1CAC:
            m81E = 1;
            m7C4 = 0;
            m7A2 = 0;
            break;
        }
        endEvent();
        break;
    case 2:
    case 6:
        break;
    }
    if (cLib_calcTimer(&m7A2) != 0 && m7A2 == 1) {
        m81E = 1;
        m7C4 = 0;
    }
    return result;
}

/* 000052B4-000053F4       .text wait_action1__11daNpc_Kk1_cFPv */
int daNpc_Kk1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        if (!dKy_daynight_check()) {
            m7B7 = 0;
            setStt(1);
            mActionState++;
        } else {
            setStt(4);
            mActionState++;
        }
        break;
    case 1:
    case 2:
    case 3:
        m7C2 = chkAttention();
        switch (mState) {
        case 1:
            mUpdateEye = wait_1();
            break;
        case 2:
            mUpdateEye = talk_1();
            break;
        case 3:
            mUpdateEye = walk_1();
            break;
        case 4:
            mUpdateEye = wait_2();
            break;
        case 5:
            mUpdateEye = cmmt_1();
            break;
        case 6:
            mUpdateEye = wait_3();
            break;
        case 7:
            mUpdateEye = wait_4();
            break;
        }
        break;
    case 9:
        break;
    }
    return TRUE;
}

/* 000053F4-00005534       .text demo__11daNpc_Kk1_cFv */
u8 daNpc_Kk1_c::demo() {
    if (demoActorID == 0) {
        if (m7C6) {
            m7C6 = 0;
        }
    } else {
        if (!m7C6) {
            m7C6 = 1;
            m7BF = 0;
            m_jnt.setHead_y(0);
            m_jnt.setHead_x(0);
            m_jnt.setBackBone_y(0);
            m_jnt.setBackBone_x(0);
        }
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (mBtpAnm.getBtpAnm() != NULL) {
            u8 max = mBtpAnm.getBtpAnm()->getFrameMax();
            mBtpFrame++;
            if (mBtpFrame >= max) {
                mBtpFrame = max;
            }
        }
        J3DAnmTexPattern* pattern = actor->getP_BtpData(mArcName);
        if (pattern != NULL) {
            mBtpAnm.init(mpMorf->getModel()->getModelData(), pattern, TRUE, 0, 1.0f, 0, -1, true, FALSE);
            mBtpNo = 1;
            mBtpFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, mArcName);
    }
    return m7C6;
}

/* 00005534-000055C4       .text shadowDraw__11daNpc_Kk1_cFv */
void daNpc_Kk1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
}

/* 000055C4-00005798       .text _draw__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    if (m7BD || m7C0) {
        return TRUE;
    }
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtpAnm.entry(data, mBtpFrame);
    mpMorf->entryDL();
    mBtpAnm.remove(data);
    if (!m7BE) {
        mBpkAnm.entry(mpEffModel->getModelData(), mBpkFrame);
        mBtkAnm.entry(mpEffModel->getModelData(), mBtkFrame);
        mDoExt_modelEntryDL(mpEffModel);
        mBtkAnm.remove(mpEffModel->getModelData());
        mBpkAnm.remove(mpEffModel->getModelData());
    }
    shadowDraw();
    dSnap_RegistFig(0x58, this, 1.0f, 1.0f, 1.0f);
    if (l_HIO.mPrm.mDebug) {
        GXColor unused[] = {
            {0x00, 0xFF, 0x00, 0x80},
            {0xFF, 0xFF, 0x00, 0x80},
            {0xFF, 0x00, 0x00, 0x80},
            {0x00, 0x00, 0xFF, 0x80},
            {0xFF, 0xFF, 0x00, 0x80},
            {0x00, 0xFF, 0x00, 0x80},
        }; // Unused colors, needed for the .rodata section to match.

        cXyz pos = current.pos;
        pos.y = eyePos.y;
        pos = mPath.getPoint(mPath.getIdx());
    }
    return TRUE;
}

/* 00005798-000059EC       .text _execute__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::_execute() {
    if (!m7C1) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle;
        m7C1 = 1;
    }
    m_jnt.setParam(
        l_HIO.mPrm.mMaxBackBoneX, l_HIO.mPrm.mMaxBackBoneY,
        l_HIO.mPrm.mMinBackBoneX, l_HIO.mPrm.mMinBackBoneY,
        l_HIO.mPrm.mMaxHeadX, l_HIO.mPrm.mMaxHeadY,
        l_HIO.mPrm.mMinHeadX, l_HIO.mPrm.mMinHeadY,
        l_HIO.mPrm.mMaxTurnStep);
    if (m7BD && demoActorID == 0) {
        return TRUE;
    }
    partner_search();
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && !checkCommandTalk()) {
            staff = isEventEntry();
        }
        if (staff >= 0) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        field_0x6ba = 0;
        lookBack();
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
        mObjAcch.CrrPos(*dComIfG_Bgsp());
        play_animation();
    } else {
        m7BD = 0;
    }
    eventOrder();
    mModelAngle = current.angle;
    if (!m7BF) {
        shape_angle = current.angle;
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    setMtx(false);
    if (!m7C6 && !m7BB) {
        f32 radius = mAnmNo == 1 ? 60.0f : 40.0f;
        setCollision(radius, 140.0f);
    }
    return TRUE;
}

/* 000059EC-00005A58       .text _delete__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::_delete() {
#if VERSION == VERSION_DEMO
    if (m7BC) {
        l_HIO.removeHIO();
        dComIfG_resDelete(&mPhs, mArcName);
        cDyl_Unlink(fpcNm_TAMA_e);
        delBikon();
        delAse();
        if (mpMorf != NULL) {
            mpMorf->stopZelAnime();
        }
    }
    return TRUE;
#else
    cDyl_Unlink(fpcNm_TAMA_e);
    dComIfG_resDelete(&mPhs, mArcName);
    delBikon();
    delAse();
    if (heap != NULL && mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    return TRUE;
#endif
}

/* 00005A58-00005B98       .text _create__11daNpc_Kk1_cFv */
cPhs_State daNpc_Kk1_c::_create() {
#if VERSION == VERSION_DEMO
    fopAcM_ct_Retail(this, daNpc_Kk1_c);
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) {
        return cPhs_ERROR_e;
    }
    cPhs_State phase = dComIfG_resLoad(&mPhs, mArcName);
    m7BC = phase == cPhs_COMPLEATE_e;
    if (!m7BC) {
        return phase;
    }
    l_HIO.entryHIO("貧乏ム−ル");
    fopAcM_ct_Demo(this, daNpc_Kk1_c);
    static u32 a_siz_tbl[] = {0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) {
        m7BC = false;
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 140.0f, 50.0f);
    if (!createInit()) {
        return cPhs_ERROR_e;
    }
    cDyl_Link(fpcNm_TAMA_e);
    return phase;
#else
    fopAcM_SetupActor(this, daNpc_Kk1_c);
    if (!decideType(fopAcM_GetParam(this) & 0xFF)) {
        return cPhs_ERROR_e;
    }
    cPhs_State phase = dComIfG_resLoad(&mPhs, mArcName);
    m7BC = phase == cPhs_COMPLEATE_e;
    if (!m7BC) {
        return phase;
    }
    int linkPhase = cDyl_LinkASync(fpcNm_TAMA_e);
    if (linkPhase != cPhs_COMPLEATE_e) {
        return (cPhs_State)linkPhase;
    }
    static u32 a_siz_tbl[] = {0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mType])) {
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 140.0f, 50.0f);
    if (createInit()) {
        return phase;
    }
    return cPhs_ERROR_e;
#endif
}

/* 00006118-0000638C       .text bodyCreateHeap__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KK_BDL_KK_e);
    JUT_ASSERT(VERSION_SELECT(0xD95, 0xDBD, 0xDD6, 0xDD6), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, NULL, -1, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11020022);
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
    JUT_ASSERT(VERSION_SELECT(0xDA9, 0xDD1, 0xDEA, 0xDEA), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(VERSION_SELECT(0xDAB, 0xDD3, 0xDEC, 0xDEC), m_bbone_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->setUserArea((u32)this);
    return TRUE;
}

/* 0000638C-000065E0       .text effcCreateHeap__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::effcCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KK_BMD_GFKDS00_e);
    mpEffModel = mDoExt_J3DModel__create(a_mdl_dat, 0, 0x11020203);
    if (mpEffModel != NULL) {
        J3DAnmColor* a_bpk = (J3DAnmColor*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KK_BPK_GFKDS00_e);
        JUT_ASSERT(VERSION_SELECT(0xDC0, 0xDE8, 0xE01, 0xE01), 0 != a_bpk);
        if (!mBpkAnm.init(mpEffModel->getModelData(), a_bpk, TRUE, J3DFrameCtrl::EMode_NONE, 0.0f, 0, -1, false, 0)) {
            return FALSE;
        }
        J3DAnmTextureSRTKey* a_btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KK_BTK_GFKDS00_e);
        JUT_ASSERT(VERSION_SELECT(0xDC8, 0xDF0, 0xE09, 0xE09), 0 != a_btk);
        if (!mBtkAnm.init(mpEffModel->getModelData(), a_btk, TRUE, J3DFrameCtrl::EMode_NONE, 0.0f, 0, -1, false, 0)) {
            return FALSE;
        }
        J3DAnmTransform* a_bck = (J3DAnmTransform*)dComIfG_getObjectIDRes(mArcName, dRes_ID_KK_BCK_GFKDS00_e);
        JUT_ASSERT(VERSION_SELECT(0xDD0, 0xDF8, 0xE11, 0xE11), 0 != a_bck);
        if (!mBckAnm.init(mpEffModel->getModelData(), a_bck, TRUE, J3DFrameCtrl::EMode_NONE, 0.0f, 0, -1, false)) {
            return FALSE;
        }
        mBpkFrame = 0;
        mBtkFrame = 0;
        mBckFrame = 0;
    }
    return TRUE;
}

/* 000065E0-00006684       .text CreateHeap__11daNpc_Kk1_cFv */
BOOL daNpc_Kk1_c::CreateHeap() {
    if (!bodyCreateHeap()) {
        return FALSE;
    }
    if (!effcCreateHeap()) {
        mpMorf = NULL;
        return FALSE;
    }
    mAcchCir.SetWall(30.0f, 40.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
    return TRUE;
}

/* 00006684-000066A4       .text daNpc_Kk1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Kk1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Kk1_c*)i_this)->_create();
}

/* 000066A4-000066C4       .text daNpc_Kk1_Delete__FP11daNpc_Kk1_c */
static BOOL daNpc_Kk1_Delete(daNpc_Kk1_c* i_this) {
    return ((daNpc_Kk1_c*)i_this)->_delete();
}

/* 000066C4-000066E4       .text daNpc_Kk1_Execute__FP11daNpc_Kk1_c */
static BOOL daNpc_Kk1_Execute(daNpc_Kk1_c* i_this) {
    return ((daNpc_Kk1_c*)i_this)->_execute();
}

/* 000066E4-00006704       .text daNpc_Kk1_Draw__FP11daNpc_Kk1_c */
static BOOL daNpc_Kk1_Draw(daNpc_Kk1_c* i_this) {
    return ((daNpc_Kk1_c*)i_this)->_draw();
}

/* 00006704-0000670C       .text daNpc_Kk1_IsDelete__FP11daNpc_Kk1_c */
static BOOL daNpc_Kk1_IsDelete(daNpc_Kk1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Kk1_Method = {
    (process_method_func)daNpc_Kk1_Create,
    (process_method_func)daNpc_Kk1_Delete,
    (process_method_func)daNpc_Kk1_Execute,
    (process_method_func)daNpc_Kk1_IsDelete,
    (process_method_func)daNpc_Kk1_Draw,
};

actor_process_profile_definition g_profile_NPC_KK1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_KK1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Kk1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_KK1_e,
    /* Actor SubMtd */ &l_daNpc_Kk1_Method,
    /* Status       */ 0x08 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
