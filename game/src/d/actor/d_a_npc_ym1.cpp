/**
* d_a_npc_ym1.cpp
* NPC - Mesa & Abe
*/
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_ym1.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "d/d_snap.h"
#include "d/d_demo.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "res/Object/Ym.h"
#include "JAZelAudio/JAZelAudio_SE.h"
#include <string.h>
static daNpc_Ym1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;
/* 000000EC-00000108       .text __ct__20daNpc_Ym1_childHIO_cFv */
daNpc_Ym1_childHIO_c::daNpc_Ym1_childHIO_c() {}

/* 00000150-0000020C       .text __ct__15daNpc_Ym1_HIO_cFv */
daNpc_Ym1_HIO_c::daNpc_Ym1_HIO_c() {
    static Ym1Prm a_prm_tbl[] = {
        {8192, 8000, -8192, -8000, 0, 7280, 0, -7280, 1600, 1200, 190.0f, 0, 400.0f, 14.0f},
        {8192, 8000, -8192, -8000, 0, 7000, 0, -7000, 1600, 1200, 190.0f, 0, 0.0f, 0.0f},
    };
    for (int i = 0; i < 2; i++) {
        mChild[i].mIndex = i;
        memcpy(&mChild[i].mPrm, &a_prm_tbl[i], sizeof(Ym1Prm));
    }
    mNo = -1;
    mSelected = -1;
}
inline daNpc_Ym1_childHIO_c::~daNpc_Ym1_childHIO_c() {}

/* 00000268-00000290       .text area_check__FP10fopAc_ac_cP4cXyzUl */
static void area_check(fopAc_ac_c* actor, cXyz*, unsigned long flag) {
    if (flag == 0) {
        ((daNpc_Ym1_c*)actor)->setKariFlg();
    }
}

/* 00000290-0000029C       .text setKariFlg__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::setKariFlg() {
    mKariFlag = 1;
}

/* 0000029C-000002E8       .text nodeCB_Head__FP7J3DNodei */
static BOOL nodeCB_Head(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ym1_c* actor = (daNpc_Ym1_c*)model->getUserArea();
        if (actor) {
            actor->_nodeCB_Head(node, model);
        }
    }
    return TRUE;
}

/* 000002E8-00000400       .text _nodeCB_Head__11daNpc_Ym1_cFP7J3DNodeP8J3DModel */
void daNpc_Ym1_c::_nodeCB_Head(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(26.0f, -25.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mHeadPos.x = mDoMtx_stack_c::get()[0][3];
    mHeadPos.y = mDoMtx_stack_c::get()[1][3];
    mHeadPos.z = mDoMtx_stack_c::get()[2][3];
    mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
    mDoMtx_stack_c::ZrotM(m_jnt.getHead_x());
    mDoMtx_stack_c::multVec(&a_eye_pos_off, &mEyePos);
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 0000043C-00000488       .text nodeCB_BackBone__FP7J3DNodei */
static BOOL nodeCB_BackBone(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ym1_c* actor = (daNpc_Ym1_c*)model->getUserArea();
        if (actor) {
            actor->_nodeCB_BackBone(node, model);
        }
    }
    return TRUE;
}

/* 00000488-00000520       .text _nodeCB_BackBone__11daNpc_Ym1_cFP7J3DNodeP8J3DModel */
void daNpc_Ym1_c::_nodeCB_BackBone(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
    mDoMtx_stack_c::ZrotM(m_jnt.getBackbone_x());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000520-00000540       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Ym1_c*)actor)->CreateHeap();
}

/* 00000540-000005C0       .text init_YM1_0__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YM1_0() {
    if (!dComIfGs_isEventBit(0x520)) {
        set_action(&daNpc_Ym1_c::wait_action1, NULL);
        return true;
    }
    return false;
}

/* 000005C0-00000640       .text init_YM1_1__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YM1_1() {
    if (dComIfGs_isEventBit(0x520)) {
        set_action(&daNpc_Ym1_c::wait_action4, NULL);
        return true;
    }
    return false;
}

/* 00000640-000006E0       .text init_YM2_0__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YM2_0() {
    if (!dComIfGs_isEventBit(0x520) && !dComIfGs_isEventBit(0xE20)) {
        set_action(&daNpc_Ym1_c::wait_action3, NULL);
        return true;
    }
    return false;
}

/* 000006E0-00000780       .text init_YM2_1__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YM2_1() {
    if (!dComIfGs_isEventBit(0x520) && dComIfGs_isEventBit(0xE20)) {
        set_action(&daNpc_Ym1_c::wait_action2, NULL);
        return true;
    }
    return false;
}

/* 00000780-0000080C       .text init_YM2_2__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YM2_2() {
    if (dComIfGs_isEventBit(0x520) && dKy_daynight_check() == 0) {
        set_action(&daNpc_Ym1_c::wait_action2, NULL);
        return true;
    }
    return false;
}

/* 0000080C-00000898       .text init_YM2_3__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YM2_3() {
    if (dComIfGs_isEventBit(0x520) && dKy_daynight_check() == 1) {
        set_action(&daNpc_Ym1_c::wait_action2, NULL);
        return true;
    }
    return false;
}

/* 00000898-000008E4       .text init_YMx_error__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::init_YMx_error() {
    set_action(&daNpc_Ym1_c::demo_action1, NULL);
    return true;
}

/* 000008E4-00000B98       .text createInit__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::createInit() {
    static u8 a_att_dis_TBL[][2] = {{0, 0}, {170, 170}, {170, 170}};
    static char* a_staff_tbl[] = {"Ym1", "Ym1", "Ym2", "Ym2", "Ym2", "Ym2", "Ym1", "Ym2"};
    mEventCut.setActorInfo2(a_staff_tbl[mStaff], this);
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = a_att_dis_TBL[mSubType][0];
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = a_att_dis_TBL[mSubType][1];
    m8AB = 13;
    bool ok;
    switch (mStaff) {
    case 0:
        ok = init_YM1_0();
        break;
    case 1:
        ok = init_YM1_1();
        break;
    case 2:
        ok = init_YM2_0();
        break;
    case 3:
        ok = init_YM2_1();
        break;
    case 4:
        ok = init_YM2_2();
        break;
    case 5:
        ok = init_YM2_3();
        break;
    case 6:
    case 7:
        ok = init_YMx_error();
        break;
    default:
        ok = false;
        break;
    }
    if (!ok) {
        return false;
    }
    shape_angle = m846 = current.angle;
    gravity = -4.5f;
    mStts.Init(255, 255, this);
    fopNpc_npc_c::mCyl.SetStts(&mStts);
    fopNpc_npc_c::mCyl.Set(dNpc_cyl_src);
    if (mStaff == 0) {
        cXyz offset(0.0f, 0.0f, 80.0f), pos;
        mCyl.SetStts(&mStts);
        mCyl.Set(dNpc_cyl_src);
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y);
        mDoMtx_stack_c::multVec(&offset, &pos);
        mCyl.SetC(pos);
        mCyl.SetR(50.0f);
        mCyl.SetH(30.0f);
        dComIfG_Ccsp()->SetMass(&mCyl, 3);
    }
    play_animation();
    if (mStaff != 1) {
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

/* 00000B98-00000C04       .text play_animation__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::play_animation() {
    play_texPttrnAnm();
    mAnmEnd = mpMorf->play(&eyePos, 0, 0);
    if (mpMorf->getFrame() < mFrame) {
        mAnmEnd = 1;
    }
    mFrame = mpMorf->getFrame();
}

/* 00000C04-00000D34       .text setMtx__11daNpc_Ym1_cFb */
void daNpc_Ym1_c::setMtx(bool force) {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(m846);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    mpHeadModel->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
    mpHeadModel->calc();
    if (m6D0) {
        m6D0->setBaseTRMtx(mpMorf->getModel()->getAnmMtx(m_hnd_R_jnt_num));
        m6D0->calc();
    }
    setAttention(force);
}

/* 00000D34-00000D48       .text bckResID__11daNpc_Ym1_cFi */
int daNpc_Ym1_c::bckResID(int index) {
    static const int a_res_id_tbl[] = {dRes_ID_YM_BCK_WAIT01_e, dRes_ID_YM_BCK_YM_KARI01_e, dRes_ID_YM_BCK_YM_KARI2WAIT_e, dRes_ID_YM_BCK_YM_KWAIT_e, dRes_ID_YM_BCK_YM_KTALK_e, dRes_ID_YM_BCK_YM_NBTWAIT_e, dRes_ID_YM_BCK_YM_NBTLOOK_e, dRes_ID_YM_BCK_YM_NBTTALK_e, dRes_ID_YM_BCK_YM_KTALK02_e, dRes_ID_YM_BCK_YM_WAIT02_e, dRes_ID_YM_BCK_YM_SIT_e, dRes_ID_YM_BCK_YM_SITTALK_e, dRes_ID_YM_BCK_YM_TALK01_e};
    return a_res_id_tbl[index];
}

/* 00000D48-00000D98       .text btpResID__11daNpc_Ym1_cFi */
int daNpc_Ym1_c::btpResID(int index) {
    static const int a_res_id_tbl[] = {dRes_ID_YM_BTP_YMHEAD01_e};
    int id = a_res_id_tbl[index];
    if (id == dRes_ID_YM_BTP_YMHEAD01_e) {
        switch (mSubType) {
        case 1:
            return dRes_ID_YM_BTP_YMHEAD01_e;
        case 2:
            return dRes_ID_YM_BTP_YMHEAD02_e;
        }
    }
    return id;
}

/* 00000D98-00000E98       .text init_texPttrnAnm__11daNpc_Ym1_cFScb */
bool daNpc_Ym1_c::init_texPttrnAnm(s8 index, bool modify) {
    J3DModel* model = mpHeadModel;
    if (index < 0) {
        return false;
    }
    int res = btpResID(index);
    J3DAnmTexPattern* a_btp = (J3DAnmTexPattern*)dComIfG_getObjectIDRes(mArcName, res);
    JUT_ASSERT(0x270, a_btp != 0);
    mBtpNo = index;
    mTexFrame = 0;
    mBlinkTimer = 0;
    return mBtpAnm.init(model->getModelData(), a_btp, 1, 0, 1.0f, 0, -1, modify, 0) != 0;
}

/* 00000E98-00000F28       .text play_texPttrnAnm__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::play_texPttrnAnm() {
    if (mBtpNo == 0 && cLib_calcTimer(&mBlinkTimer)) {
        return;
    }
    mTexFrame++;
    s16 end = mBtpAnm.getBtpAnm()->getFrameMax();
    if (mTexFrame >= end) {
        if (mBtpNo != 0) {
            mTexFrame = end;
        } else {
            mBlinkTimer = cLib_getRndValue(60, 90);
            mTexFrame = 0;
        }
    }
}

/* 00000F28-00000FF4       .text setAnm_anm__11daNpc_Ym1_cFPQ211daNpc_Ym1_c9anm_prm_c */
void daNpc_Ym1_c::setAnm_anm(anm_prm_c* anm) {
    if (anm->mAnm < 0 || m8AB == anm->mAnm) {
        return;
    }
    int res = bckResID(anm->mAnm);
    dNpc_setAnmIDRes(mpMorf, anm->mLoop, anm->mMorf, anm->mSpeed, res, -1, mArcName);
    m8AB = anm->mAnm;
    mAnmEnd = 0;
    mAnmLoops = 0;
    mFrame = 0.0f;
    if (m8AB == 8) {
        mpMorf->setMorf(l_HIO.mChild[mSubType - 1].mPrm.kariMorf);
    }
}

/* 00000FF4-00001064       .text setAnm_NUM__11daNpc_Ym1_cFii */
void daNpc_Ym1_c::setAnm_NUM(int index, int tex) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2},
        {1, 0, 8.0f, 1.0f, 2},
        {2, 0, 8.0f, 1.0f, 0},
        {3, 0, 8.0f, 1.0f, 2},
        {4, 0, 8.0f, 1.0f, 2},
        {5, 0, 8.0f, 1.0f, 2},
        {6, 0, 8.0f, 1.0f, 0},
        {7, 0, 8.0f, 1.0f, 2},
        {8, 0, 8.0f, 1.0f, 2},
        {9, 0, 8.0f, 1.0f, 2},
        {10, 0, 8.0f, 1.0f, 2},
        {11, 0, 8.0f, 1.0f, 2},
        {12, 0, 8.0f, 1.0f, 2},
    };
    if (tex) {
        init_texPttrnAnm(a_anm_prm_tbl[index].mBtp, true);
    }
    setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 00001064-000010D0       .text setAnm__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, -1, 0.0f, 0.0f, -1},
        {1, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, 1.0f, 2},
        {-1, -1, 0.0f, 0.0f, -1},
        {5, 0, 8.0f, 1.0f, 2},
        {-1, -1, 0.0f, 0.0f, -1},
        {0, 0, 8.0f, 1.0f, 2},
        {-1, -1, 0.0f, 0.0f, -1},
        {9, 0, 8.0f, 1.0f, 2},
        {10, 0, 8.0f, 1.0f, 2},
    };
    init_texPttrnAnm(a_anm_prm_tbl[m8AD].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[m8AD]);
}

/* 000010D0-000010D4       .text chngAnmTag__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::chngAnmTag() {
}

/* 000010D4-000010D8       .text ctrlAnmTag__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::ctrlAnmTag() {
}

/* 000010D8-00001118       .text chngAnmAtr__11daNpc_Ym1_cFUc */
void daNpc_Ym1_c::chngAnmAtr(u8 attr) {
    if (attr == mAnmAttr || attr > 13) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR();
}

/* 00001118-0000111C       .text ctrlAnmAtr__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::ctrlAnmAtr() {
}

/* 0000111C-00001180       .text setAnm_ATR__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2},
        {1, 0, 8.0f, 1.0f, 2},
        {2, 0, 8.0f, 1.0f, 0},
        {3, 0, 8.0f, 1.0f, 2},
        {4, 0, 8.0f, 1.0f, 2},
        {5, 0, 8.0f, 1.0f, 2},
        {6, 0, 8.0f, 1.0f, 0},
        {7, 0, 8.0f, 1.0f, 2},
        {8, 0, 8.0f, 1.0f, 2},
        {9, 0, 8.0f, 1.0f, 2},
        {10, 0, 8.0f, 1.0f, 2},
        {11, 0, 8.0f, 1.0f, 2},
        {12, 0, 8.0f, 1.0f, 2},
    };
    init_texPttrnAnm(a_anm_prm_tbl[mAnmAttr].mBtp, true);
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00001180-0000123C       .text anmAtr__11daNpc_Ym1_cFUs */
void daNpc_Ym1_c::anmAtr(u16 status) {
    switch (status) {
    case 6:
        {
            if (!mMsgAnmStarted) {
                chngAnmAtr(dComIfGp_getMesgAnimeAttrInfo());
                mMsgAnmStarted++;
            }
            u8 tag = dComIfGp_getMesgAnimeTagInfo();
            if (tag != 0xFF && tag != mAnmTag) {
                dComIfGp_clearMesgAnimeTagInfo();
                mAnmTag = tag;
                chngAnmTag();
            }
            break;
        }
    case 14:
        mMsgAnmStarted = 0;
        break;
    }
    ctrlAnmAtr();
    ctrlAnmTag();
}

/* 0000123C-000014F0       .text next_msgStatus__11daNpc_Ym1_cFPUl */
u16 daNpc_Ym1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x8FD:
        *msg=0x8FE;
        break;
    case 0x8FF:
        *msg=0x900;
        break;
    case 0x901:
        *msg=0x902;
        break;
    case 0x90A:
        *msg=0x90B;
        break;
    case 0x904:
        *msg=0x905;
        break;
    case 0x905:
        *msg=0x906;
        break;
    case 0x907:
        *msg=0x908;
        break;
    case 0x908:
        if (dComIfGs_isEventBit(0x3004)) {
            status = 16;
        } else {
            *msg=0x909;
        }
        break;
    case 0xA2F:
        if (dKy_daynight_check() == 1) {
            *msg=0xA30;
        } else if (dComIfGs_getEventReg(0xBFFF)) {
            if (dComIfGs_isEventBit(0x3402)) {
                *msg=0xA31;
            } else {
                *msg=0xA33;
            }
        } else {
            if (dComIfGs_isEventBit(0x3402)) {
                *msg=0xA37;
            } else {
                *msg=0xA35;
            }
        }
        break;
    case 0xA31:
        *msg=0xA32;
        break;
    case 0xA33:
        *msg=0xA34;
        break;
    case 0xA35:
        *msg=0xA36;
        break;
    case 0xA37:
        *msg=0xA38;
        break;
    case 0xA3B:
        if (dComIfGs_isEventBit(0x3402)) {
            *msg=0xA3C;
        } else {
            *msg=0xA3D;
        }
        break;
    case 0xA3E:
        if (dComIfGs_isEventBit(0x3402)) {
            *msg=0xA40;
        } else {
            *msg=0xA3F;
        }
        break;
    case 0xA41:
        *msg=0xA42;
        break;
    default:
        status = 16;
        break;
    }
    return status;
}

/* 000014F0-00001584       .text getMsg_YM1_0__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg_YM1_0() {
    if (dComIfGs_checkCollect(0)) {
        return dComIfGs_isEventBit(0x10) ? 0x903 : 0x901;
    }
    if (dComIfGs_isEventBit(0x20)) {
        return 0x8FF;
    }
    return dComIfGs_isEventBit(0x2A80) ? 0x8FD : 0x90A;
}

/* 00001584-000015C4       .text getMsg_YM1_1__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg_YM1_1() {
    return dComIfGs_isEventBit(0x2904) ? 0x907 : 0x904;
}

/* 000015C4-00001630       .text getMsg_YM2_0__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg_YM2_0() {
    if (chk_BlackPig()) {
        return 0xA2B + (dComIfGs_isEventBit(8) ? 1 : 0);
    }
    return 0xA29 + (dComIfGs_isEventBit(0x80) ? 1 : 0);
}

/* 00001630-0000166C       .text getMsg_YM2_1__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg_YM2_1() {
    return 0xA2D + (dComIfGs_isEventBit(0xB01) ? 1 : 0);
}

/* 0000166C-00001718       .text getMsg_YM2_2__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg_YM2_2() {
    if (!dComIfGs_isEventBit(0x3140)) {
        return 0xA2F;
    }
    if (dKy_daynight_check() == 1) {
        return 0xA39 + (dComIfGs_isEventBit(0x3402) ? 1 : 0);
    }
    if (!dComIfGs_isEventBit(0x3580)) {
        return 0xA3B;
    }
    return !dComIfGs_isEventBit(0x3540) ? 0xA3E : 0xA41;
}

/* 00001718-00001738       .text getMsg_YM2_3__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg_YM2_3() {
    return getMsg_YM2_2();
}

/* 00001738-000017DC       .text getMsg__11daNpc_Ym1_cFv */
u32 daNpc_Ym1_c::getMsg() {
    u32 msg = 0;
    switch (mStaff) {
    case 0:
        msg = getMsg_YM1_0();
        break;
    case 1:
        msg = getMsg_YM1_1();
        break;
    case 2:
        msg = getMsg_YM2_0();
        break;
    case 3:
        msg = getMsg_YM2_1();
        break;
    case 4:
        msg = getMsg_YM2_2();
        break;
    case 5:
        msg = getMsg_YM2_3();
        break;
    }
    return msg;
}

/* 000017DC-0000182C       .text eventOrder__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::eventOrder() {
    if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 0000182C-0000186C       .text checkOrder__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        return;
    }
    if (eventInfo.checkCommandTalk()) {
        if (mOrder == 1 || mOrder == 2) {
            mOrder = 0;
            m8A3 = 1;
        }
    }
}

/* 0000186C-00001A08       .text set_collision_sp__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::set_collision_sp() {
    cXyz pos, offset;
    f32 height, radius;
    if (m8A3) {
        return;
    }
    switch (m8AB) {
    case 1:
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y);
        offset.set(0.0f, 0.0f, 20.0f);
        radius = 60.0f;
        height = 140.0f;
        mDoMtx_stack_c::multVec(&offset, &pos);
        break;
    case 5:
    case 6:
    case 7:
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y);
        offset.set(0.0f, 0.0f, 60.0f);
        radius = 80.0f;
        height = 150.0f;
        mDoMtx_stack_c::multVec(&offset, &pos);
        break;
    default:
        pos.set(current.pos.x, current.pos.y, current.pos.z);
        radius = 60.0f;
        height = 180.0f;
        break;
    }
    fopNpc_npc_c::mCyl.SetC(pos);
    fopNpc_npc_c::mCyl.SetR(radius);
    fopNpc_npc_c::mCyl.SetH(height);
    dComIfG_Ccsp()->Set(&this->fopNpc_npc_c::mCyl);
}

/* 00001A08-00001B24       .text set_cutGrass__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::set_cutGrass() {
    if (!m8A6 && m8AB == 1 && mpMorf->checkFrame(34.0f)) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hnd_L_jnt_num));
        cXyz pos;
        pos.x = mDoMtx_stack_c::get()[0][3];
        pos.y = mDoMtx_stack_c::get()[1][3];
        pos.z = mDoMtx_stack_c::get()[2][3];
        dComIfGp_particle_setSimple(0x3DA, &pos);
        mDoAud_seStart(JA_SE_CM_CUT_GRASS, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 00001B24-00001B5C       .text chk_BlackPig__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::chk_BlackPig() {
    return (dComIfGs_getEventReg(0xBFFF) & 4) != 0;
}

/* 00001B5C-00001B8C       .text chk_nbt_attn__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::chk_nbt_attn() {
    bool result = false;
    if (m8AB == 5 || m8AB == 6 || m8AB == 7) {
        result = true;
    }
    return result;
}

/* 00001B8C-00001C24       .text chk_talk__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::chk_talk() {
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mItem = dComIfGp_event_getPreItemNo();
            return true;
        } return false;
    }
    mItem = 255;
    return true;
}

/* 00001C24-00001C64       .text chk_parts_notMov__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::chk_parts_notMov() {
    return mOldHead == m_jnt.getHead_y() && mOldBackbone == m_jnt.getBackbone_y() && mOldAngle == current.angle.y;
}

/* 00001C64-00001DD0       .text lookBack__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::lookBack() {
    mOldHead = m_jnt.getHead_y();
    mOldBackbone = m_jnt.getBackbone_y();
    mOldAngle = current.angle.y;
    cXyz target;
    cXyz origin = current.pos;
    origin.y = eyePos.y;
    target.set(0.0f, 0.0f, 0.0f);
    cXyz* pTarget = NULL;
    s16 angle = current.angle.y;
    bool turn = m8A4;
    switch (m8AF) {
    case 1:
        mLookPos = dNpc_playerEyePos(-20.0f);
        target = mLookPos;
        pTarget = &target;
        break;
    case 2:
        target = mLookPos;
        pTarget = &target;
        break;
    case 3:
        angle = mLookAngle;
        break;
    }
    m_jnt.lookAtTarget_2(&current.angle.y, pTarget, origin, angle, l_HIO.mChild[mSubType - 1].mPrm.turnSpeed, turn);
}

/* 00001DD0-00001E50       .text chkAttention__11daNpc_Ym1_cFv */
bool daNpc_Ym1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 00001E50-00001F5C       .text setAttention__11daNpc_Ym1_cFb */
void daNpc_Ym1_c::setAttention(bool force) {
    f32 height = l_HIO.mChild[mSubType - 1].mPrm.attentionHeight;
    cXyz pos = current.pos;
    if (chk_nbt_attn()) {
        if (m8AB == 6) {
            height = mHeadPos.y-210.0f;
        } else {
            height = 116.0f;
        }
        pos = mAttentionPos;
    }
    attention_info.position.set(pos.x, pos.y+height, pos.z);
    if (m888 != 0 || force) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
    }
}

/* 00001F5C-00002088       .text decideType__11daNpc_Ym1_cFi */
bool daNpc_Ym1_c::decideType(int type) {
    if (mSubType > 0) {
        return true;
    }
    mSubType = -1;
    mStaff = -1;
    switch (fopAcM_GetName(this)) {
    case fpcNm_NPC_YM1_e:
        mSubType = 1;
        switch (type) {
        case 0:
            mStaff = 0;
            break;
        case 1:
            mStaff = 1;
            break;
        } break;
    case fpcNm_NPC_YM2_e:
        mSubType = 2;
        switch (type) {
        case 0:
            mStaff = 2;
            break;
        case 1:
            mStaff = 3;
            break;
        case 2:
            mStaff = 4;
            break;
        case 3:
            mStaff = 5;
            break;
        } break;
    }
    strcpy(mArcName, "Ym");
    return mSubType != -1 && mStaff != -1;
}

/* 00002088-00002128       .text privateCut__11daNpc_Ym1_cFi */
void daNpc_Ym1_c::privateCut(int staff) {
    static char* a_cut_tbl[] = {"DUMMY"};
    if (staff != -1) {
        mCut = dComIfGp_evmng_getMyActIdx(staff, a_cut_tbl, 1, 1, 0);
        if (mCut == -1) {
            dComIfGp_evmng_cutEnd(staff);
            return;
        }
        bool end = true;
        dComIfGp_evmng_getIsAddvance(staff);
        if (end) {
            dComIfGp_evmng_cutEnd(staff);
        }
    }
}

/* 00002128-0000214C       .text endEvent__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 255;
    mAnmTag = 255;
}

/* 0000214C-00002184       .text isEventEntry__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

/* 00002184-000021D4       .text event_proc__11daNpc_Ym1_cFi */
void daNpc_Ym1_c::event_proc(int staff) {
    if (!mEventCut.cutProc()) {
        privateCut(staff);
    }
}

/* 000021D4-00002280       .text set_action__11daNpc_Ym1_cFM11daNpc_Ym1_cFPCvPvPv_iPv */
bool daNpc_Ym1_c::set_action(ActionFunc action, void* arg) {
    if (mAction != action) {
        if (mAction) {
            mActionState = 9;
            (this->*mAction)(arg);
        } mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return true;
}

/* 00002280-00002324       .text setStt__11daNpc_Ym1_cFSc */
void daNpc_Ym1_c::setStt(s8 state) {
    s8 previous = m8AD;
    m8AD = state;
    switch (m8AD) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 8:
    case 10:
    case 11:
        mOrder = 0;
        break;
    case 5:
    case 7:
    case 9:
        mOrder = 0;
        mAnmAttr = 255;
        mAnmTag = 255;
        mMsgAnmStarted = 0;
        mPreviousState = previous;
        break;
    case 6:
        mOrder = 0;
        mKariTimer = cLib_getRndValue(90, 180);
        break;
    }
    setAnm();
}

/* 00002324-00002448       .text chk_areaIN__11daNpc_Ym1_cFf4cXyz */
bool daNpc_Ym1_c::chk_areaIN(f32 radius, cXyz pos) {
    f32 distance = (dComIfGp_getLinkPlayer()->current.pos-pos).absXZ();
    f32 height = dComIfGp_getLinkPlayer()->current.pos.y-pos.y;
    if (distance < radius && std::fabsf(height) < 300.0f) {
        return true;
    }
    return false;
}

/* 00002448-000025B8       .text kari_1__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::kari_1() {
    f32 distance = (dComIfGp_getLinkPlayer()->current.pos-current.pos).absXZ();
    if (cLib_calcTimer(&mKariTimer)) {
        return 1;
    }
    if (chk_areaIN(l_HIO.mChild[mSubType - 1].mPrm.areaRadius, current.pos) || !m89C) {
        setStt(2);
        m8AF = 0;
        m8A4 = 1;
        setAnm_NUM(2, 1);
    }
    return 1;
}

/* 000025B8-00002770       .text wait_1__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::wait_1() {
    if (m8AB == 2) {
        if (mAnmEnd) {
            setAnm_NUM(3, 1);
            m8AF = 0;
            m8A4 = 1;
            m88E = cLib_getRndValue(15, 30);
        }
        return 1;
    }
    if (m8A5) {
        cLib_addCalcAngleS(&current.angle.y, mRotYTarget, 4, l_HIO.mChild[mSubType - 1].mPrm.turnSpeed, 128);
    }
    if (m8A3) {
        if (chk_talk()) {
            setStt(5);
            m8AF = 1;
            m8A4 = 0;
            m8A5 = 0;
            m_jnt.setTrn();
        }
        return 1;
    }
    mOrder = 2;
    m8AF = 1;
    m8A4 = 1;
    if (m8A2) {
        m88E = cLib_getRndValue(15, 30);
    }
    u8 outside = !chk_areaIN(50.0f+l_HIO.mChild[mSubType - 1].mPrm.areaRadius, current.pos);
    if (outside) {
        if (!cLib_calcTimer(&m88E)) {
            setStt(3);
            m8AF = 0;
            m8A4 = 1;
        }
        return 1;
    }
    return 1;
}

/* 00002770-00002958       .text wait_2__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::wait_2() {
    s16 angle = 0;
    if (m8A5) {
        cLib_addCalcAngleS(&current.angle.y, mRotYTarget, 4, l_HIO.mChild[mSubType - 1].mPrm.turnSpeed, 128);
        angle = mRotYTarget-current.angle.y;
    }
    if (m8A3) {
        if (chk_talk()) {
            setStt(5);
            m8AF = 1;
            m8A4 = 0;
            m8A5 = 0;
            m_jnt.setTrn();
        }
        return 1;
    }
    mOrder = 2;
    m8AF = 0;
    m8A4 = 1;
    if (angle == 0) {
        f32 width = mSubType == 1 ? 120.0f : 92.0f;
        f32 distance = mSubType == 1 ? 250.0f : 200.0f;
        if (m8A2) {
            m88E = cLib_getRndValue(15, 30);
        }
        if (cLib_calcTimer(&m88E)) {
            m8AF = 1;
        }
        if (dNpc_chkAttn(this, dComIfGp_getLinkPlayer()->current.pos, distance, 50.0f, width, m8AF == 1)) {
            return 1;
        }
        m8AF = 0;
        m8A5 = 1;
    }
    return 1;
}

/* 00002958-00002BA0       .text talk_1__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::talk_1() {
    int stopped = chk_parts_notMov();
    talk(1);
    if (!mpCurrMsg) {
        return stopped;
    }
    switch (mpCurrMsg->mStatus) {
    case 2:
    case 6:
        break;
    case 19:
        switch (mCurrMsgNo) {
        case 0x902:
            dComIfGs_onEventBit(0x10);
            break;
        case 0x906:
            dComIfGs_onEventBit(0x2904);
            break;
        case 0xA29:
            dComIfGs_onEventBit(0x80);
            break;
        case 0xA2B:
            dComIfGs_onEventBit(8);
            break;
        case 0xA2D:
            dComIfGs_onEventBit(0xB01);
            break;
        case 0xA40:
            dComIfGs_onEventBit(0x3540);
            break;
        case 0x8FE:
        case 0x90B:
            dComIfGs_onEventBit(0x20);
            break;
        case 0xA30:
        case 0xA32:
        case 0xA34:
        case 0xA36:
        case 0xA38:
            dComIfGs_onEventBit(0x3140);
            break;
        case 0xA3C:
        case 0xA3D:
            dComIfGs_onEventBit(0x3580);
            break;
        }
        mItem = 255;
        m8A3 = 0;
        setStt(mPreviousState);
        m88E = cLib_getRndValue(15, 30);
        endEvent();
        break;
    }
    return stopped;
}

/* 00002BA0-00002C78       .text turn_1__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::turn_1() {
    cLib_addCalcAngleS(&current.angle.y, mRotYTarget, 4, l_HIO.mChild[mSubType - 1].mPrm.turnSpeed, 128);
    if ((s16)(mRotYTarget-current.angle.y) == 0) {
        if (m89C) {
            setStt(1);
            mpMorf->setMorf(10.0f);
            m8AF = 0;
            m8A4 = 1;
            mKariTimer = cLib_getRndValue(30, 60);
        } else {
            setStt(4);
            m8AF = 0;
            m8A4 = 1;
        }
    }
    return 1;
}

/* 00002C78-00002DD4       .text NBTwai__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::NBTwai() {
    if (m8A3) {
        if (chk_talk()) {
            setStt(7);
            m8AF = 0;
            m8A4 = 1;
        } return 1;
    }
    mOrder = 2;
    m8AF = 0;
    m8A4 = 1;
    if (chk_BlackPig()) {
        setStt(8);
        m8AF = 0;
        m8A4 = 1;
        m8A5 = 1;
        return 1;
    }
    if (m8AB == 6) {
        if (mAnmEnd || m8A2) {
            setAnm_NUM(5, 1);
            mKariTimer = cLib_getRndValue(90, 180);
        } return 1;
    }
    if (m8A2) {
        m88E = cLib_getRndValue(15, 30);
    }
    if (cLib_calcTimer(&m88E)) {
        m8AF = 1;
        return 1;
    }
    if (!cLib_calcTimer(&mKariTimer)) {
        setAnm_NUM(6, 1);
    }
    return 1;
}

/* 00002DD4-00002EFC       .text SITwai__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::SITwai() {
    if (m8A3) {
        if (chk_talk()) {
            setStt(5);
            m8AF = 1;
            m8A4 = 1;
        } return 1;
    }
    mOrder = 2;
    m8AF = 0;
    m8A4 = 1;
    if (m8A2) {
        m88E = cLib_getRndValue(15, 30);
    }
    if (cLib_calcTimer(&m88E)) {
        m8AF = 1;
    }
    if (dNpc_chkAttn(this, dComIfGp_getLinkPlayer()->current.pos, 250.0f, 50.0f, 120.0f, m8AF == 1)) {
        return 1;
    }
    m8AF = 0;
    return 1;
}

/* 00002EFC-00002FFC       .text wait_action1__11daNpc_Ym1_cFPv */
int daNpc_Ym1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        setStt(1);
        m8A5 = 1;
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m8A2 = chkAttention();
        switch (m8AD) {
        case 1:
            m888 = kari_1();
            break;
        case 2:
            m888 = wait_1();
            break;
        case 4:
            m888 = wait_2();
            break;
        case 3:
            m888 = turn_1();
            break;
        case 5:
            m888 = talk_1();
            break;
        }
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00002FFC-00003100       .text wait_action2__11daNpc_Ym1_cFPv */
int daNpc_Ym1_c::wait_action2(void*) {
    switch (mActionState) {
    case 0:
        if (mStaff == 5) { setStt(10); m8A5 = 1; mActionState++; } else { setStt(8); m8A5 = 1; mActionState++; } break;
    case 1:
    case 2:
    case 3:
        m8A2 = chkAttention();
        switch (m8AD) {
        case 8:
            m888 = wait_2();
            break;
        case 10:
            m888 = wait_2();
            break;
        case 5:
            m888 = talk_1();
            break;
        }
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00003100-00003270       .text wait_action3__11daNpc_Ym1_cFPv */
int daNpc_Ym1_c::wait_action3(void*) {
    cXyz offset(0.0f, 0.0f, 110.0f);
    switch (mActionState) {
    case 0:
        if (chk_BlackPig()) {
            setStt(8);
            m8A5 = 1;
            mActionState++;
        } else {
            setStt(6);
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::YrotM(m846.y);
            mDoMtx_stack_c::multVec(&offset, &mAttentionPos);
            mActionState++;
        } break;
    case 1:
    case 2:
    case 3:
        m8A2 = chkAttention();
        switch (m8AD) {
        case 6:
            m888 = NBTwai();
            break;
        case 7:
            m888 = talk_1();
            break;
        case 8:
            m888 = wait_2();
            break;
        case 5:
            m888 = talk_1();
            break;
        }
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00003270-00003324       .text wait_action4__11daNpc_Ym1_cFPv */
int daNpc_Ym1_c::wait_action4(void*) {
    switch (mActionState) {
    case 0:
        setStt(11);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m8A2 = chkAttention();
        switch (m8AD) {
        case 11:
            m888 = SITwai();
            break;
        case 5:
            m888 = talk_1();
            break;
        }
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00003324-00003390       .text demo_action1__11daNpc_Ym1_cFPv */
int daNpc_Ym1_c::demo_action1(void*) {
    switch (mActionState) {
    case 0:
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m8A2 = chkAttention();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00003390-000034CC       .text demo__11daNpc_Ym1_cFv */
u8 daNpc_Ym1_c::demo() {
    if (demoActorID == 0) {
        if (m8A6) {
            m8A6 = 0;
        }
    } else {
        if (!m8A6) {
            m8A6 = 1;
            m89F = 0;
            m_jnt.setHead_y(0);
            m_jnt.setHead_x(0);
            m_jnt.setBackBone_y(0);
            m_jnt.setBackBone_x(0);
        }
        dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
        if (mBtpAnm.getBtpAnm()) {
            mTexFrame++;
            s16 end = mBtpAnm.getBtpAnm()->getFrameMax();
            if (mTexFrame >= end) {
                mTexFrame = end;
            }
        }
        J3DAnmTexPattern* btp = actor->getP_BtpData(mArcName);
        if (btp) {
            mBtpAnm.init(mpHeadModel->getModelData(), btp, 1, 0, 1.0f, 0, -1, true, 0);
            mBtpNo = 1;
            mTexFrame = 0;
        }
        dDemo_setDemoData(this, 0x6A, mpMorf, mArcName);
    }
    return m8A6;
}

/* 000034CC-0000359C       .text shadowDraw__11daNpc_Ym1_cFv */
void daNpc_Ym1_c::shadowDraw() {
    cXyz pos(current.pos.x, current.pos.y+150.0f, current.pos.z);
    mShadow = dComIfGd_setShadow(mShadow, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadow) {
        if (m6D0) {
            dComIfGd_addRealShadow(mShadow, m6D0);
        }
        dComIfGd_addRealShadow(mShadow, mpHeadModel);
    }
}

/* 0000359C-000037A0       .text _draw__11daNpc_Ym1_cFv */
BOOL daNpc_Ym1_c::_draw() {
    J3DModel* head = mpHeadModel;
    J3DModelData* data = head->getModelData();
    J3DModel* body = mpMorf->getModel();
    if (m89E || m8A0) {
        return TRUE;
    }
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(body, &tevStr);
    g_env_light.setLightTevColorType(head, &tevStr);
    switch (mSubType) {
    case 1:
        mpMorf->entryDL();
        break;
    case 2:
        mpMorf->entryDL((J3DMaterialTable*)dComIfG_getObjectIDRes(mArcName, dRes_ID_YM_BMT_YM2_e));
        break;
    }
    mBtpAnm.entry(data, mTexFrame);
    mDoExt_modelEntryDL(head);
    mBtpAnm.remove(data);
    if (m6D0) {
        g_env_light.setLightTevColorType(m6D0, &tevStr);
        mDoExt_modelEntryDL(m6D0);
    }
    shadowDraw();
    switch (mSubType) {
    case 1:
        dSnap_RegistFig(80, this, 1.0f, 1.0f, 1.0f);
        break;
    case 2:
#if VERSION == VERSION_DEMO
        dSnap_RegistFig(78, this, 1.0f, 1.0f, 1.0f);
#else
        dSnap_RegistFig(78, this, eyePos, shape_angle.y, 1.0f, 1.0f, 1.0f);
#endif
        break;
    }
    if (l_HIO.mChild[mSubType - 1].mPrm.debug) {
        cXyz pos = current.pos;
        pos.y = eyePos.y;
        GXColor red = {255, 0, 0, 128};
        GXColor blue = {0, 0, 255, 128};
        GXColor yellow = {255, 255, 0, 128};
    }
    m89C = mKariFlag;
    mKariFlag = 0;
    return TRUE;
}

/* 000037A0-00003A5C       .text _execute__11daNpc_Ym1_cFv */
BOOL daNpc_Ym1_c::_execute() {
    if (!m8A1) {
        mInitialPos = current.pos;
        mInitialAngle = current.angle.x;
        mRotYTarget = current.angle.y;
        m844 = current.angle.z;
        m8A1 = 1;
    }
    if (chk_nbt_attn()) {
        m_jnt.setParam(0, 0, 0, 0, 8192, 14560, -8192, -14560, l_HIO.mChild[mSubType - 1].mPrm.maxTurn);
    } else {
        m_jnt.setParam(l_HIO.mChild[mSubType - 1].mPrm.maxBackX, l_HIO.mChild[mSubType - 1].mPrm.maxBackY, l_HIO.mChild[mSubType - 1].mPrm.minBackX, l_HIO.mChild[mSubType - 1].mPrm.minBackY, l_HIO.mChild[mSubType - 1].mPrm.maxHeadX, l_HIO.mChild[mSubType - 1].mPrm.maxHeadY, l_HIO.mChild[mSubType - 1].mPrm.minHeadX, l_HIO.mChild[mSubType - 1].mPrm.minHeadY, l_HIO.mChild[mSubType - 1].mPrm.maxTurn);
    }
    if (m89E && !demoActorID) {
        return TRUE;
    }
    checkOrder();
    if (!demo()) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == FALSE) {
            staff = isEventEntry();
        }
        if (staff >= 0) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        lookBack();
        if (mStaff != 1) {
            fopAcM_posMoveF(this, mStts.GetCCMoveP());
            mObjAcch.CrrPos(*dComIfG_Bgsp());
        }
        play_animation();
    } else m89E = 0;
    eventOrder();
    m846 = current.angle;
    if (!m89F) {
        shape_angle = current.angle;
    }
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    setMtx(false);
    set_cutGrass();
    if (mStaff == 0) {
        dComIfG_Ccsp()->SetMassAreaChk(&mCyl, 3, area_check);
    }
    if (!m8A6) {
        set_collision_sp();
    }
    return TRUE;
}

/* 00003A5C-00003AB0       .text _delete__11daNpc_Ym1_cFv */
BOOL daNpc_Ym1_c::_delete() {
#if VERSION == VERSION_DEMO
    if (m89D) {
        l_HIO.removeHIO();
        dComIfG_resDelete(&mPhs, mArcName);
        if (mpMorf) {
            mpMorf->stopZelAnime();
        }
    }
#else
    dComIfG_resDelete(&mPhs, mArcName);
    if (heap && mpMorf) {
        mpMorf->stopZelAnime();
    }
#endif
    return TRUE;
}

/* 00003AB0-00003BDC       .text _create__11daNpc_Ym1_cFv */
cPhs_State daNpc_Ym1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Ym1_c);
#endif
    if (!decideType(fopAcM_GetParam(this)&255)) {
        return cPhs_ERROR_e;
    }
    cPhs_State phase = dComIfG_resLoad(&mPhs, mArcName);
    m89D = phase == cPhs_COMPLEATE_e;
    if (!m89D) {
        return phase;
    }
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("若者");
    fopAcM_SetupActor(this, daNpc_Ym1_c);
#endif
    static u32 a_siz_tbl[] = {0, 0, 0};
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_siz_tbl[mSubType])) {
#if VERSION == VERSION_DEMO
        m89D = false;
#endif
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -80.0f, -20.0f, -40.0f, 80.0f, 180.0f, 130.0f);
    return createInit() ? phase : cPhs_ERROR_e;
}

/* 0000405C-00004360       .text bodyCreateHeap__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::bodyCreateHeap() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_YM_BDL_YM_e);
    JUT_ASSERT(DEMO_SELECT(0x96E, 0x971), a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(a_mdl_dat, NULL, NULL, NULL, -1, 1.0f, 0, -1, 1, NULL, 0x80000, 0x15021222);
    if (!mpMorf) {
        return 0;
    }
    if (!mpMorf->getModel()) {
        mpMorf = NULL;
        return 0;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x97C, 0x97F), m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x97E, 0x981), m_bbone_jnt_num >= 0);
    m_hnd_L_jnt_num = a_mdl_dat->getJointName()->getIndex("handL");
    JUT_ASSERT(DEMO_SELECT(0x980, 0x983), m_hnd_L_jnt_num >= 0);
    m_hnd_R_jnt_num = a_mdl_dat->getJointName()->getIndex("handR");
    JUT_ASSERT(DEMO_SELECT(0x982, 0x985), m_hnd_R_jnt_num >= 0);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_hed_jnt_num)->setCallBack(nodeCB_Head);
    mpMorf->getModel()->getModelData()->getJointNodePointer(m_bbone_jnt_num)->setCallBack(nodeCB_BackBone);
    mpMorf->getModel()->setUserArea((u32)this);
    return 1;
}

/* 00004360-00004460       .text headCreateHeap__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::headCreateHeap() {
    static int a_hed_bdl_resID_tbl[] = {0, dRes_ID_YM_BDL_YMHEAD01_e, dRes_ID_YM_BDL_YMHEAD02_e};
    static s8 a_tex_pttrn_num_tbl[] = {0, 0, 0};
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, a_hed_bdl_resID_tbl[mSubType]);
    JUT_ASSERT(DEMO_SELECT(0x9A1, 0x9A4), a_mdl_dat != 0);
    mpHeadModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x15020022);
    if (!mpHeadModel) {
        return 0;
    }
    return init_texPttrnAnm(a_tex_pttrn_num_tbl[mSubType], false) ? 1 : 0;
}

/* 00004460-00004534       .text itemCreateHeap__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::itemCreateHeap() {
    if (mStaff == 0) {
        J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes(mArcName, dRes_ID_YM_BDL_YMKAMA_e);
        JUT_ASSERT(DEMO_SELECT(0x9C3, 0x9C6), a_mdl_dat != 0);
        m6D0 = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
        if (!m6D0) {
            return 0;
        }
    } else m6D0 = NULL;
    return 1;
}

/* 00004534-000045F8       .text CreateHeap__11daNpc_Ym1_cFv */
int daNpc_Ym1_c::CreateHeap() {
    if (!bodyCreateHeap()) {
        return 0;
    }
    if (!headCreateHeap()) {
        mpMorf = NULL;
        return 0;
    }
    if (!itemCreateHeap()) {
        mpMorf = NULL;
        return 0;
    }
    mAcchCir.SetWall(30.0f, 60.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
    return 1;
}

/* 000045F8-00004618       .text daNpc_Ym1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Ym1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Ym1_c*)i_this)->_create();
}

/* 00004618-00004638       .text daNpc_Ym1_Delete__FP11daNpc_Ym1_c */
static BOOL daNpc_Ym1_Delete(daNpc_Ym1_c* i_this) {
    return ((daNpc_Ym1_c*)i_this)->_delete();
}

/* 00004638-00004658       .text daNpc_Ym1_Execute__FP11daNpc_Ym1_c */
static BOOL daNpc_Ym1_Execute(daNpc_Ym1_c* i_this) {
    return ((daNpc_Ym1_c*)i_this)->_execute();
}

/* 00004658-00004678       .text daNpc_Ym1_Draw__FP11daNpc_Ym1_c */
static BOOL daNpc_Ym1_Draw(daNpc_Ym1_c* i_this) {
    return ((daNpc_Ym1_c*)i_this)->_draw();
}

/* 00004678-00004680       .text daNpc_Ym1_IsDelete__FP11daNpc_Ym1_c */
static BOOL daNpc_Ym1_IsDelete(daNpc_Ym1_c*) {
    return TRUE;
}
static actor_method_class l_daNpc_Ym1_Method = {
    (process_method_func)daNpc_Ym1_Create,
    (process_method_func)daNpc_Ym1_Delete,
    (process_method_func)daNpc_Ym1_Execute,
    (process_method_func)daNpc_Ym1_IsDelete,
    (process_method_func)daNpc_Ym1_Draw,
};
actor_process_profile_definition g_profile_NPC_YM1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_YM1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Ym1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_YM1_e,
    /* Actor SubMtd */ &l_daNpc_Ym1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
actor_process_profile_definition g_profile_NPC_YM2 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_YM2_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Ym1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_YM2_e,
    /* Actor SubMtd */ &l_daNpc_Ym1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

inline daNpc_Ym1_HIO_c::~daNpc_Ym1_HIO_c() {}
