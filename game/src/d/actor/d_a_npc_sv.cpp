/**
 * d_a_npc_sv.cpp
 * NPC - Salvage Corp group members
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_sv.h"
#include "m_Do/m_Do_ext.h"
#include "d/actor/d_a_obj_ikada.h"
#include "d/d_com_inf_game.h"
#include "d/d_com_lib_game.h"
#include "d/actor/d_a_ship.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_audio.h"
#include "res/Object/Sv.h"

const char daNpcSv_c::m_arcname[] = "Sv";
static char* l_npc_staff_id[] = {"Sv0", "Sv1", "Sv2", "Sv3"};
static const char* l_arcname_tbl[] = {"Sv", "Sv", "Sv", "Sv"};
static const int l_bmd_ix_tbl[] = {dRes_ID_SV_BDL_SV_e, dRes_ID_SV_BDL_SV_e, dRes_ID_SV_BDL_SV_e, dRes_ID_SV_BDL_SV_e};
static const int l_bck_ix_tbl[4][6] = {
    {
        dRes_ID_SV_BCK_SV_WAIT01_e, dRes_ID_SV_BCK_SV_TALK01_e, dRes_ID_SV_BCK_SV_TALK02_e,
        dRes_ID_SV_BCK_SV_TALK03_e, dRes_ID_SV_BCK_SV_YA_e, dRes_ID_SV_BCK_SV_MAWASI_e,
    },
    {
        dRes_ID_SV_BCK_SV_WAIT02_e, dRes_ID_SV_BCK_SV_KYORO_e, dRes_ID_SV_BCK_SV_UNAZUKU_e,
        dRes_ID_SV_BCK_SV_HARAU_e, dRes_ID_SV_BCK_SV_BYE_e, dRes_ID_SV_BCK_SV_SHI_e,
    },
    {
        dRes_ID_SV_BCK_SV_WAIT02_e, dRes_ID_SV_BCK_SV_KYORO_e, dRes_ID_SV_BCK_SV_UNAZUKU_e,
        dRes_ID_SV_BCK_SV_HARAU_e, dRes_ID_SV_BCK_SV_BYE_e, dRes_ID_SV_BCK_SV_SHI_e,
    },
    {
        dRes_ID_SV_BCK_SV_WAIT02_e, dRes_ID_SV_BCK_SV_KYORO_e, dRes_ID_SV_BCK_SV_UNAZUKU_e,
        dRes_ID_SV_BCK_SV_HARAU_e, dRes_ID_SV_BCK_SV_BYE_e, dRes_ID_SV_BCK_SV_SHI_e,
    },
};
static sSvAnmDat l_npc_anm_wait[] = {{0, 8, -1}};
static sSvAnmDat l_npc_anm_talk[] = {{1, 8, -1}};
static sSvAnmDat l_npc_anm_talk2[] = {{2, 8, -1}};
static sSvAnmDat l_npc_anm_talk3[] = {{3, 8, -1}};
static sSvAnmDat l_npc_anm_ya1[] = {{4, 8, 1}, {0, 8, -1}};
static sSvAnmDat l_npc_anm_ya2[] = {{4, 8, 1}, {1, 8, -1}};
static sSvAnmDat l_npc_anm_mawasi[] = {{5, 0, -1}};
static sSvAnmDat l_npc_anm_kyoro[] = {{1, 8, -1}};
static sSvAnmDat l_npc_anm_unun[] = {{2, 8, 1}, {255, 8, 0}};
static sSvAnmDat l_npc_anm_sisi[] = {{3, 8, 1}, {255, 8, 0}};
static sSvAnmDat l_npc_anm_byebye[] = {{4, 8, 1}, {255, 8, 0}};
static sSvAnmDat l_npc_anm_shi[] = {{5, 8, 1}, {255, 8, 0}};
static sSvAnmDat* l_npc_anm_sv1_tbl[] = {l_npc_anm_wait, l_npc_anm_kyoro, l_npc_anm_unun, l_npc_anm_sisi, l_npc_anm_byebye, l_npc_anm_shi};
static u32 l_npc_se_sv1_tbl[] = {0, 0, JA_SE_CV_SALV_CRUE_AGREE, JA_SE_CV_SALV_CRUE_SI, JA_SE_CV_SALV_CRUE_BYE, 0};

struct SvNpcDat {
    s16 maxHeadX, maxHeadY, maxBackX, maxBackY;
    s16 minHeadX, minHeadY, minBackX, minBackY;
    s16 maxTurn, unused;
    Vec offset;
    f32 playerEyeOffset, attentionHeight, eyeHeight;
    f32 attentionDistance, lookDistance;
    s16 attentionAngle, turnSpeed;
    f32 radius, height;
    s16 lookTimer;
    u8 canTurn, canLook;
};

static SvNpcDat l_npc_dat[] = {
    {4000, 6000, 0, 8000, 0, -6000, -3000, -8000, 1000, 0, {0.0f, -5.0f, 0.0f}, 0.0f, 185.0f, 150.0f, 600.0f, 1500.0f, 32767, 1000, 60.0f, 150.0f, 30, 1, 1},
    {4096, 0, 0, 0, -4096, 0, 0, 0, 2048, 0, {0.0f, 0.0f, 0.0f}, 0.0f, 180.0f, 150.0f, 0.0f, 0.0f, 32767, 2048, 60.0f, 150.0f, 30, 0, 1},
    {4096, 0, 0, 0, -4096, 0, 0, 0, 2048, 0, {0.0f, 0.0f, 0.0f}, 0.0f, 180.0f, 150.0f, 0.0f, 0.0f, 32767, 2048, 60.0f, 150.0f, 30, 0, 1},
    {4096, 0, 0, 0, -4096, 0, 0, 0, 2048, 0, {0.0f, 0.0f, 0.0f}, 0.0f, 180.0f, 150.0f, 0.0f, 0.0f, 16384, 2048, 60.0f, 150.0f, 30, 0, 1},
};
static u32 l_msg_sv_p1_1st_talk1[] = {0x34BD, 0x34BE, 0x34BF, 0x34C0, 0x2, 0x34C1, 0x34C2, 0x34C3, 0x1, 0x34C4, 0x34E8, 0x2, 0x34C5, 0x4, 0x0};
static u32 l_msg_sv_p1_1st_talk2[] = {0x34BD, 0x34BE, 0x34BF, 0x34C0, 0x2, 0x34C1, 0x34C2, 0x34C3, 0x1, 0x34C6, 0x34E9, 0x2, 0x34C7, 0x4, 0x0};
static u32 l_msg_sv_p1_2nd_talk1[] = {0x34C8, 0x34C9, 0x34EA, 0x2, 0x34CA, 0x4, 0x0};
static u32 l_msg_sv_p1_2nd_talk2[] = {0x34C8, 0x34CB, 0x2, 0x34CC, 0x4, 0x0};
static u32 l_msg_sv_p1_3rd_talk1[] = {0x34CD, 0x34D0, 0x2, 0x34D1, 0x3, 0x0};
static u32 l_msg_sv_p1_3rd_talk2[] = {0x34CD, 0x34CE, 0x2, 0x34CF, 0x3, 0x0};
static u32 l_msg_sv_p2_1st_talk[] = {0x34D9, 0x34DA, 0x34DB, 0x2, 0x34DC, 0x4, 0x0};
static u32 l_msg_sv_p2_2nd_talk[] = {0x34DD, 0x34DE, 0x2, 0x34DF, 0x3, 0x0};
static u32 l_msg_sv_p3_1st_talk[] = {0x34D2, 0x34D3, 0x34D4, 0x2, 0x34D5, 0x4, 0x0};
static u32 l_msg_sv_p3_2nd_talk[] = {0x34D6, 0x34D7, 0x2, 0x34D8, 0x3, 0x0};
static u32 l_msg_sv_p4_1st_talk[] = {0x34E0, 0x34E1, 0x34E2, 0x34E3, 0x34E4, 0x5, 0x0};
static u32 l_msg_sv_p4_2nd_talk[] = {0x34E5, 0x34E6, 0x34E7, 0x5, 0x0};
static u32* l_msg_sv_talk_tbl[] = {
    l_msg_sv_p1_1st_talk1, l_msg_sv_p1_1st_talk2, l_msg_sv_p1_2nd_talk1, l_msg_sv_p1_2nd_talk2, l_msg_sv_p1_3rd_talk1, l_msg_sv_p1_3rd_talk2,
    l_msg_sv_p2_1st_talk, l_msg_sv_p2_2nd_talk, l_msg_sv_p3_1st_talk, l_msg_sv_p3_2nd_talk, l_msg_sv_p4_1st_talk, l_msg_sv_p4_2nd_talk,
};
static u16 l_msg_sv_flag_tbl[] = {1, 1, 2, 2, 0, 0, 4, 0, 8, 0, 16, 0};
static s16 l_event_id_tbl[] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2};
static int l_get_item_no[] = {dItemNo_COLLECT_MAP_26_e};

/* 00000078-0000021C       .text __ct__9daNpcSv_cFv */
daNpcSv_c::daNpcSv_c() {
    mNpcNo = getPrmNpcNo();
    mResFlag = 0;
    mMode = 0;
    mMorf = -1.0f;
    mLookMode = 0;
    mNoTurn = true;
    mTalkEnabled = 1;
    mInitialAngle = home.angle.y;
    mAnmIndex = 0;
    mTalkFlags = 0;
    mCraneFrame = 0;
    if (dComIfGs_checkGetItem(dItemNo_COLLECT_MAP_26_e)) {
        mTalkFlags |= 0x20;
    }
}

/* 00000528-00000640       .text daNpc_People_nodeCallBack__FP7J3DNodei */
static BOOL daNpc_People_nodeCallBack(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpcSv_c* npc = (daNpcSv_c*)model->getUserArea();
        int joint = ((J3DJoint*)node)->getJntNo();
        MTXCopy(model->getAnmMtx(joint), *calc_mtx);
#if VERSION == VERSION_DEMO
        if (joint == npc->m_jnt.getHeadJntNum()) {
            cMtx_XrotM(*calc_mtx, npc->m_jnt.getHead_y());
            cMtx_ZrotM(*calc_mtx, -npc->m_jnt.getHead_x());
        }
        if (joint == npc->m_jnt.getBackboneJntNum()) {
            cMtx_XrotM(*calc_mtx, npc->m_jnt.getBackbone_y());
            cMtx_ZrotM(*calc_mtx, -npc->m_jnt.getBackbone_x());
        }
#else
        if (joint == npc->m_jnt.getHeadJntNum()) {
            mDoMtx_XrotM(*calc_mtx, npc->m_jnt.getHead_y());
            mDoMtx_ZrotM(*calc_mtx, -npc->m_jnt.getHead_x());
        }
        if (joint == npc->m_jnt.getBackboneJntNum()) {
            mDoMtx_XrotM(*calc_mtx, npc->m_jnt.getBackbone_y());
            mDoMtx_ZrotM(*calc_mtx, -npc->m_jnt.getBackbone_x());
        }
#endif
        model->setAnmMtx(joint, *calc_mtx);
        cMtx_copy(*calc_mtx, J3DSys::mCurrentMtx);
    }
    return TRUE;
}

/* 00000640-00000660       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpcSv_c*)actor)->createHeap();
}

/* 00000660-000006B8       .text phase_1__FP9daNpcSv_c */
static cPhs_State phase_1(daNpcSv_c* npc) {
    fopAcM_SetupActor(npc, daNpcSv_c);
    npc->setResFlag(1);
    return cPhs_NEXT_e;
}

/* 000006B8-00000738       .text phase_2__FP9daNpcSv_c */
static cPhs_State phase_2(daNpcSv_c* npc) {
    cPhs_State phase = dComIfG_resLoad(npc->getPhaseP(), l_arcname_tbl[npc->getNpcNo()]);
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(npc, CheckCreateHeap, 0x3800)) {
            phase = npc->createInit();
        } else {
#if VERSION > VERSION_DEMO
            npc->mpMorf = NULL;
#endif
            phase = cPhs_ERROR_e;
        }
    }
    return phase;
}

/* 00000738-00000768       .text _create__9daNpcSv_cFv */
cPhs_State daNpcSv_c::_create() {
    static request_of_phase_process_fn l_method[] = {(request_of_phase_process_fn)phase_1, (request_of_phase_process_fn)phase_2, NULL};
    return dComLbG_PhaseHandler(&mCreatePhase, l_method, this);
}

/* 00000768-00000A04       .text createHeap__9daNpcSv_cFv */
BOOL daNpcSv_c::createHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_bmd_ix_tbl[mNpcNo]);
    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_bck_ix_tbl[mNpcNo][mAnmIndex]),
        2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x37441422);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
        return FALSE;
    }
    m_jnt.setHeadJntNum(modelData->getJointName()->getIndex("head"));
    JUT_ASSERT(DEMO_SELECT(945, 946), m_jnt.getHeadJntNum() >= 0);
    m_jnt.setBackboneJntNum(modelData->getJointName()->getIndex("backbone"));
    JUT_ASSERT(DEMO_SELECT(949, 950), m_jnt.getBackboneJntNum() >= 0);
    for (u16 i = 0; i < modelData->getJointNum(); i++) {
        if (i == m_jnt.getHeadJntNum() || i == m_jnt.getBackboneJntNum()) {
            modelData->getJointNodePointer(i)->setCallBack(daNpc_People_nodeCallBack);
        }
    }
    mpMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 30.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), fopAcM_GetAngle_p(this), fopAcM_GetShapeAngle_p(this));
    return TRUE;
}

/* 00000A04-00000C74       .text createInit__9daNpcSv_cFv */
cPhs_State daNpcSv_c::createInit() {
    gravity = -9.0f;
    setAnmTbl(l_npc_anm_wait);
    mEventIds[0] = dComIfGp_evmng_getEventIdx("SV_TALK_P1_1ST");
    mEventIds[1] = dComIfGp_evmng_getEventIdx("SV_TALK_P1_2ND");
    mEventIds[2] = dComIfGp_evmng_getEventIdx("SV_TALK_P4_1ST");
    eventInfo.setEventId(mEventIds[l_event_id_tbl[getTalkNo()]]);
#if VERSION == VERSION_DEMO
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(&mCyl, current.pos, l_npc_dat[mNpcNo].radius, l_npc_dat[mNpcNo].height);
    mEventCut.setActorInfo2(l_npc_staff_id[mNpcNo], this);
    setMtx();
#else
    mEventCut.setActorInfo2(l_npc_staff_id[mNpcNo], this);
#endif
    mCurrentTurnSpeed = 0;
    mTalking = 0;
    mHasAttention = 0;
    mNearPlayer = 0;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -70.0f, 0.0f, -70.0f, 70.0f, 200.0f, 70.0f);
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xB1;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xB1;
    attention_info.flags = fopAc_Attn_LOCKON_TALK_e | fopAc_Attn_ACTION_SPEAK_e;
    m_jnt.setParam(l_npc_dat[mNpcNo].maxBackX, l_npc_dat[mNpcNo].maxBackY,
        l_npc_dat[mNpcNo].minBackX, l_npc_dat[mNpcNo].minBackY,
        l_npc_dat[mNpcNo].maxHeadX, l_npc_dat[mNpcNo].maxHeadY,
        l_npc_dat[mNpcNo].minHeadX, l_npc_dat[mNpcNo].minHeadY, l_npc_dat[mNpcNo].maxTurn);
    mCanTurn = l_npc_dat[mNpcNo].canTurn;
    mCanLook = l_npc_dat[mNpcNo].canLook;
    mAttentionDistance = l_npc_dat[mNpcNo].attentionDistance;
    mAttentionAngle = l_npc_dat[mNpcNo].attentionAngle;
#if VERSION > VERSION_DEMO
    setMtx();
    mpMorf->getModel()->calc();
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(&mCyl, current.pos, l_npc_dat[mNpcNo].radius, l_npc_dat[mNpcNo].height);
#endif
    return cPhs_COMPLEATE_e;
}

/* 00000C74-00000CD8       .text _delete__9daNpcSv_cFv */
bool daNpcSv_c::_delete() {
#if VERSION == VERSION_DEMO
    if (mResFlag) {
        dComIfG_resDeleteDemo(&mPhase, l_arcname_tbl[mNpcNo]);
    }
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
#else
    dComIfG_resDelete(&mPhase, l_arcname_tbl[mNpcNo]);
    if (heap != NULL && mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
#endif
    return true;
}

/* 00000CD8-00000DAC       .text _draw__9daNpcSv_cFv */
bool daNpcSv_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpMorf->getModel(), &tevStr);
    mpMorf->updateDL();
    cXyz pos(current.pos.x, current.pos.y + 150.0f, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 20.0f,
        current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    return true;
}
static u8 (daNpcSv_c::*l_execute_init[])() = {&daNpcSv_c::executeWaitInit, &daNpcSv_c::executeTalkInit};
static void (daNpcSv_c::*moveProc[])() = {&daNpcSv_c::executeWait, &daNpcSv_c::executeTalk};

/* 00000DAC-00000F90       .text _execute__9daNpcSv_cFv */
bool daNpcSv_c::_execute() {
    daObj_Ikada_c* raft;
    fopAcM_SearchByID(parentActorID, (fopAc_ac_c**)&raft);
    chkAttention();
    checkOrder();
    if (!dComIfGp_event_runCheck()) {
        (this->*moveProc[mMode])();
    } else {
        eventMove();
    }
    eventOrder();
    if (mNpcNo == 0 && mAnmIndex == 5) {
        mpMorf->setFrame(mCraneFrame);
    } else {
        playAnm();
    }
    if (raft != NULL) {
        mCraneFrame = raft->m1154;
    }
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    setCollision(&mCyl, current.pos, l_npc_dat[mNpcNo].radius, l_npc_dat[mNpcNo].height);
    attention_info.position.set(current.pos.x, current.pos.y + l_npc_dat[mNpcNo].attentionHeight, current.pos.z);
    eyePos.set(current.pos.x, current.pos.y + l_npc_dat[mNpcNo].eyeHeight, current.pos.z);
    lookBack();
    setMtx();
    return false;
}

/* 00000F90-00000FB8       .text executeCommon__9daNpcSv_cFv */
BOOL daNpcSv_c::executeCommon() {
    if (mHasAttention) {
        mEventOrder = 1;
    } else {
        mEventOrder = 0;
    }
    return FALSE;
}

/* 00000FB8-00001000       .text executeSetMode__9daNpcSv_cFUc */
void daNpcSv_c::executeSetMode(u8 mode) {
    mMode = (this->*l_execute_init[mode])();
}

/* 00001000-00001080       .text executeWaitInit__9daNpcSv_cFv */
u8 daNpcSv_c::executeWaitInit() {
    setAnmTbl(l_npc_anm_wait);
    m_jnt.setParam(l_npc_dat[mNpcNo].maxBackX, l_npc_dat[mNpcNo].maxBackY,
        l_npc_dat[mNpcNo].minBackX, l_npc_dat[mNpcNo].minBackY,
        l_npc_dat[mNpcNo].maxHeadX, l_npc_dat[mNpcNo].maxHeadY,
        l_npc_dat[mNpcNo].minHeadX, l_npc_dat[mNpcNo].minHeadY, l_npc_dat[mNpcNo].maxTurn);
    return 0;
}

/* 00001080-0000131C       .text executeWait__9daNpcSv_cFv */
void daNpcSv_c::executeWait() {
    daObj_Ikada_c* raft;
    fopAcM_SearchByID(parentActorID, (fopAc_ac_c**)&raft);
    if (raft != NULL) {
        if (mNpcNo == 0) {
            if (mNearPlayer) {
                mEventOrder = 1;
                if (mNearPlayer != mPreviousNearPlayer) {
                    mStopped = 1;
                }
                raft->setStop();
            } else {
                mEventOrder = 0;
                mStopped = 0;
                raft->setStart();
            }
            if (mStopped) {
                setAnmTbl(l_npc_anm_wait);
            } else {
                mLookAngle = raft->shape_angle.y;
                current.angle.x = raft->shape_angle.x;
                current.angle.z = raft->shape_angle.z;
                mLookMode = 2;
                mNoTurn = false;
                m_jnt.setTrn();
                setAnmTbl(l_npc_anm_mawasi);
            }
            mPreviousNearPlayer = mNearPlayer;
        } else {
            mCanTurn = 1;
            if (raft->mCurMode == 8 || raft->mCurMode == 9 || raft->mCurMode == 5) {
                mLookAngle = raft->shape_angle.y;
                mLookMode = 2;
                mNoTurn = false;
                m_jnt.setTrn();
            } else if (raft->mCurMode == 4 || raft->mCurMode == 6) {
                if (raft->m1168 == 0) {
                    mLookAngle = raft->shape_angle.y + 0x4000;
                } else {
                    mLookAngle = raft->shape_angle.y - 0x4000;
                }
                mLookMode = 2;
                mNoTurn = false;
                m_jnt.setTrn();
            } else {
                s16 angle;
                if (raft->m1168 == 0) {
                    angle = raft->shape_angle.y + 0x4000;
                } else {
                    angle = raft->shape_angle.y - 0x4000;
                }
                if (current.angle.y == angle) {
                    mLookPos = raft->getCranePos();
                    mLookMode = 1;
                    mNoTurn = true;
                    mCanTurn = 0;
                } else {
                    mLookAngle = angle;
                    mLookMode = 2;
                    mNoTurn = false;
                    m_jnt.setTrn();
                }
            }
            if (raft->mCurMode == 7) {
                if (mCraneWait == 0) {
                    mCraneWait = 1;
                    setAnmTbl(l_npc_anm_sv1_tbl[1]);
                }
            } else if (mCraneWait == 1) {
                mCraneWait = 0;
                setAnmTbl(l_npc_anm_sv1_tbl[0]);
            }
        }
    }
}

/* 0000131C-00001338       .text executeTalkInit__9daNpcSv_cFv */
u8 daNpcSv_c::executeTalkInit() {
    if (mTalkEnabled) {
        return 1;
    }
    return mMode;
}

/* 00001338-000013B8       .text executeTalk__9daNpcSv_cFv */
void daNpcSv_c::executeTalk() {
    executeCommon();
    if (talk2(1, this) == 0x12) {
        mTalking = 0;
        mStopped = 0;
        executeSetMode(0);
        dComIfGp_event_reset();
    } else {
        setAnmFromMsgTag();
    }
}

/* 000013B8-000013EC       .text checkOrder__9daNpcSv_cFv */
void daNpcSv_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        return;
    }
    if (eventInfo.checkCommandTalk()) {
        if (mEventOrder == 2 || mEventOrder == 1) {
            mTalking = 1;
        }
    }
}

/* 000013EC-00001458       .text eventOrder__9daNpcSv_cFv */
void daNpcSv_c::eventOrder() {
    if ((mEventOrder == 2 || mEventOrder == 1) && isTalkOK()) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mEventOrder == 2) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 00001458-00001570       .text eventMove__9daNpcSv_cFv */
void daNpcSv_c::eventMove() {
    if (dComIfGp_evmng_endCheck(mEventIds[0]) || dComIfGp_evmng_endCheck(mEventIds[1]) || dComIfGp_evmng_endCheck(mEventIds[2])) {
        eventInfo.setEventId(mEventIds[l_event_id_tbl[getTalkNo()]]);
        dComIfGp_event_reset();
        mEventFlags &= ~2;
        mTalking = 0;
        mStopped = 0;
        executeSetMode(0);
    } else {
        bool attention = mEventCut.getAttnFlag();
        if (mEventCut.cutProc()) {
            if (!mEventCut.getAttnFlag()) {
                mEventCut.setAttnFlag(attention);
            }
        } else {
            privateCut();
            if (mNpcNo == 0) {
                setAnmFromMsgTag();
            }
        }
    }
}

/* 00001570-00001744       .text privateCut__9daNpcSv_cFv */
void daNpcSv_c::privateCut() {
    static char* cut_name_tbl[] = {"MES_SET", "GET_ITEM", "SET_ANGLE", "ATTENTION", "TURN_OK"};
    int staff = dComIfGp_evmng_getMyStaffId(l_npc_staff_id[mNpcNo]);
    if (staff != -1) {
        mCut = dComIfGp_evmng_getMyActIdx(staff, cut_name_tbl, 5, 1, 0);
        if (mCut == -1) {
            dComIfGp_evmng_cutEnd(staff);
        } else {
            if (dComIfGp_evmng_getIsAddvance(staff)) {
                switch (mCut) {
                case 0:
                    eventMesSetInit(staff);
                    break;
                case 1:
                    eventGetItemInit(staff);
                    break;
                case 2:
                    eventSetAngleInit();
                    break;
                case 3:
                    eventAttentionInit(staff);
                    break;
                case 4:
                    eventTurnOkInit();
                    break;
                }
            }
            BOOL done;
            switch (mCut) {
            case 0:
                done = eventMesSet();
                break;
            case 1:
                done = eventGetItem();
                break;
            case 3:
                done = eventAttention();
                break;
            default:
                done = TRUE;
                break;
            }
            if ((u8)done) {
                dComIfGp_evmng_cutEnd(staff);
            }
        }
    }
    if (mEventFlags & 2) {
        mLookAngle = mEventAngle;
        mLookMode = 2;
        mNoTurn = false;
        m_jnt.setTrn();
    }
}

/* 00001744-00001814       .text eventMesSetInit__9daNpcSv_cFi */
void daNpcSv_c::eventMesSetInit(int staff) {
    int* msg = dComIfGp_evmng_getMyIntegerP(staff, "MsgNo");
    if (msg != NULL) {
        mpMsg = NULL;
        switch (*msg) {
        case 0:
            setMessage(getMsg());
            break;
        default:
            setMessage(*msg);
            break;
        }
    } else {
        mpMsg++;
        setMessage(*mpMsg);
    }
    dComIfGp_event_setTalkPartner(this);
}

/* 00001814-0000184C       .text eventMesSet__9daNpcSv_cFv */
BOOL daNpcSv_c::eventMesSet() {
    return talk2(0, this) == 0x12 ? TRUE : FALSE;
}

/* 0000184C-000018F0       .text eventGetItemInit__9daNpcSv_cFi */
void daNpcSv_c::eventGetItemInit(int staff) {
    int* item = dComIfGp_evmng_getMyIntegerP(staff, "ItemNo");
    int itemNo = item != NULL ? l_get_item_no[*item] : mItem;
    fpc_ProcID id = fopAcM_createItemForPresentDemo(&current.pos, itemNo, 0, -1, fopAcM_GetRoomNo(this), NULL, NULL);
    if (id != fpcM_ERROR_PROCESS_ID_e) {
        dComIfGp_event_setItemPartnerId(id);
    }
}

/* 000018F0-000018F8       .text eventGetItem__9daNpcSv_cFv */
BOOL daNpcSv_c::eventGetItem() {
    return TRUE;
}

/* 000018F8-000019E0       .text eventSetAngleInit__9daNpcSv_cFv */
void daNpcSv_c::eventSetAngleInit() {
    daObj_Ikada_c* raft;
    fopAcM_SearchByID(parentActorID, (fopAc_ac_c**)&raft);
    if (raft != NULL) {
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        s16 angle;
        dNpc_calc_DisXZ_AngY(raft->current.pos, player->current.pos, NULL, &angle);
        angle -= raft->shape_angle.y;
        if (angle > 0) {
            mEventAngle = raft->shape_angle.y + 0x4000;
        } else {
            mEventAngle = raft->shape_angle.y - 0x4000;
        }
        mInitialAngle = mEventAngle;
    } else {
        mEventAngle = mInitialAngle;
    }
    mEventFlags |= 2;
}

/* 000019E0-00001A74       .text eventAttentionInit__9daNpcSv_cFi */
void daNpcSv_c::eventAttentionInit(int staff) {
    int* timer = dComIfGp_evmng_getMyIntegerP(staff, "Timer");
    if (timer != NULL) {
        mEventTimer = *timer;
    } else {
        mEventTimer = 12;
    }
    mCraneWait = 0;
    if (mNpcNo == 2) {
        dComIfGp_event_setTalkPartner(this);
    }
}

/* 00001A74-00001BC8       .text eventAttention__9daNpcSv_cFv */
BOOL daNpcSv_c::eventAttention() {
    if (mEventTimer != 0) {
        mEventTimer--;
        if (mEventTimer == 0) {
            daNpcSv_c* captain = (daNpcSv_c*)fopAcM_searchFromName(l_npc_staff_id[0], 0, 0);
            if (captain != NULL) {
                setAnmTbl(l_npc_anm_sv1_tbl[captain->getMsgPtn()]);
                if (mNpcNo == 1) {
                    mDoAud_seStart(l_npc_se_sv1_tbl[captain->getMsgPtn()], NULL, 0, 0);
                }
            } else {
                setAnmTbl(l_npc_anm_sv1_tbl[2]);
                mDoAud_seStart(l_npc_se_sv1_tbl[2], NULL, 0, 0);
            }
        }
    } else if (mAnmFlags & 1) {
        setAnmTbl(l_npc_anm_sv1_tbl[0]);
        return TRUE;
    }
    return FALSE;
}

/* 00001BC8-00001BD4       .text eventTurnOkInit__9daNpcSv_cFv */
void daNpcSv_c::eventTurnOkInit() {
    mCanTurn = 1;
}

/* 00001BD4-00001CF0       .text talk2__9daNpcSv_cFiP10fopAc_ac_c */
u16 daNpcSv_c::talk2(int mode, fopAc_ac_c* actor) {
    u16 result = 0xFF;
    if (mCurrMsgBsPcId == fpcM_ERROR_PROCESS_ID_e) {
        if (mode == 1) {
            mCurrMsgNo = getMsg();
        }
        mCurrMsgBsPcId = fopMsgM_messageSet(mCurrMsgNo, actor);
        mpCurrMsg = NULL;
    } else if (mpCurrMsg != NULL) {
        result = mpCurrMsg->mStatus;
        switch (result) {
        case 0xE:
            mpCurrMsg->mStatus = next_msgStatus(&mCurrMsgNo);
            if (mpCurrMsg->mStatus == 0xF) {
                fopMsgM_messageSet(mCurrMsgNo);
            }
            break;
        case 0x12:
            mpCurrMsg->mStatus = 0x13;
            mCurrMsgBsPcId = fpcM_ERROR_PROCESS_ID_e;
            break;
        }
        anmAtr(result);
    } else {
        mpCurrMsg = fopMsgM_SearchByID(mCurrMsgBsPcId);
    }
    return result;
}

/* 00001CF0-00001DC8       .text next_msgStatus__9daNpcSv_cFPUl */
u16 daNpcSv_c::next_msgStatus(u32* msg) {
    u16 result = 0xF;
    switch (*msg) {
    case 0:
        result = 0x10;
        break;
    default:
        if (mpMsg != NULL) {
            mpMsg++;
            switch (*mpMsg) {
            case 1:
                result = 0x10;
                break;
            case 2:
                mMsgPattern = 2;
                result = 0x10;
                break;
            case 3:
                mMsgPattern = 3;
                result = 0x10;
                break;
            case 4:
                mMsgPattern = 4;
                result = 0x10;
                break;
            case 5:
                mMsgPattern = 5;
                result = 0x10;
                break;
            case 0:
                mpMsg = NULL;
                result = 0x10;
                break;
            default:
                *msg = *mpMsg;
                break;
            }
        } else {
            result = 0x10;
        }
        break;
    }
    return result;
}

/* 00001DC8-00001EB4       .text getMsg__9daNpcSv_cFv */
u32 daNpcSv_c::getMsg() {
    u32 msg = 0;
    mpMsg = NULL;
    if (!dComIfGp_event_chkTalkXY()) {
        mpMsg = l_msg_sv_talk_tbl[mTalkNo];
        mTalkFlags |= l_msg_sv_flag_tbl[mTalkNo];
        if (mTalkFlags & 0x10) {
            dComIfGs_onEventBit(0x2F80);
        }
    }
    if (mpMsg != NULL) {
        msg = *mpMsg;
    }
    if (msg == 0) {
        setAnmTbl(l_npc_anm_talk);
    }
    return msg;
}

/* 00001EB4-00001EBC       .text setMessage__9daNpcSv_cFUl */
void daNpcSv_c::setMessage(u32 msg) {
    mCurrMsgNo = msg;
}

/* 00001EBC-00001F74       .text setAnmFromMsgTag__9daNpcSv_cFv */
void daNpcSv_c::setAnmFromMsgTag() {
    switch (dComIfGp_getMesgAnimeAttrInfo()) {
    case 0:
        setAnmTbl(l_npc_anm_wait);
        break;
    case 1:
        setAnmTbl(l_npc_anm_ya1);
        break;
    case 2:
        setAnmTbl(l_npc_anm_talk);
        break;
    case 3:
        setAnmTbl(l_npc_anm_talk2);
        break;
    case 4:
        setAnmTbl(l_npc_anm_talk3);
        break;
    case 5:
        setAnmTbl(l_npc_anm_ya2);
        break;
    }
    dComIfGp_clearMesgAnimeAttrInfo();
}

/* 00001F74-00001F94       .text getPrmNpcNo__9daNpcSv_cFv */
u8 daNpcSv_c::getPrmNpcNo() {
    if (argument >= 0 && argument < 4) {
        return argument;
    }
    return 0;
}

/* 00001F94-0000209C       .text setMtx__9daNpcSv_cFv */
void daNpcSv_c::setMtx() {
    mpMorf->getModel()->setBaseScale(scale);
    cXyz offset(l_npc_dat[mNpcNo].offset.x, l_npc_dat[mNpcNo].offset.y, l_npc_dat[mNpcNo].offset.z);
    mDoMtx_stack_c::YrotS(current.angle.y);
    mDoMtx_stack_c::multVec(&offset, &offset);
    mDoMtx_stack_c::transS(current.pos.x + offset.x, current.pos.y + offset.y, current.pos.z + offset.z);
    mDoMtx_stack_c::XYZrotM(current.angle.x, 0, current.angle.z);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 0000209C-000023D4       .text chkAttention__9daNpcSv_cFv */
void daNpcSv_c::chkAttention() {
    mNearPlayer = 0;
    if (mEventCut.getAttnFlag()) {
        mLookPos = mEventCut.getAttnPos();
        mLookMode = 1;
        if (mCanTurn) {
            mNoTurn = false;
            m_jnt.setTrn();
        } else {
            mNoTurn = true;
        }
        if (mHasAttention == 0) {
            mHasAttention = 1;
        }
    } else {
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        f32 maxDistance = mAttentionDistance;
        int maxAngle = mAttentionAngle;
        f32 distance;
        s16 angle;
        dNpc_calc_DisXZ_AngY(current.pos, player->current.pos, &distance, &angle);
        if (mHasAttention) {
            maxDistance += 100.0f;
            maxAngle += 0x71C;
        }
        angle -= shape_angle.y;
        if (maxDistance > distance && maxAngle > abs(angle)) {
            mLookPos = dNpc_playerEyePos(l_npc_dat[mNpcNo].playerEyeOffset);
            mLookMode = 1;
            if (mCanTurn) {
                mNoTurn = false;
            } else {
                mNoTurn = true;
            }
            if (!mCanLook) {
                mLookAngle = mInitialAngle;
                mNoTurn = false;
                mLookMode = 2;
                m_jnt.setTrn();
            }
            if (mHasAttention == 0) {
                mHasAttention = 1;
            }
            mNearPlayer = 1;
        } else {
            if (mHasAttention == 1) {
                mHasAttention = 0;
                mLookTimer = l_npc_dat[mNpcNo].lookTimer;
            }
            if (l_npc_dat[mNpcNo].lookDistance > distance) {
                mLookPos = dNpc_playerEyePos(l_npc_dat[mNpcNo].playerEyeOffset);
                mLookMode = 1;
                if (mCanTurn) {
                    mNoTurn = false;
                } else {
                    mNoTurn = true;
                }
                if (!mCanLook) {
                    mLookAngle = mInitialAngle;
                    mNoTurn = false;
                    mLookMode = 2;
                    m_jnt.setTrn();
                }
                mNearPlayer = 1;
            } else {
                mLookMode = 0;
                if (mpAttentionActor == NULL) {
                    if (mLookTimer != 0) {
                        mLookTimer--;
                    } else {
                        mLookAngle = mInitialAngle;
                        mNoTurn = false;
                        mLookMode = 2;
                        m_jnt.setTrn();
                    }
                }
            }
        }
    }
    mTurnSpeed = l_npc_dat[mNpcNo].turnSpeed;
}

/* 000023D4-00002520       .text lookBack__9daNpcSv_cFv */
void daNpcSv_c::lookBack() {
    s16 speed = mTurnSpeed;
    s16 angle = current.angle.y;
    cXyz* targetP = NULL;
    cXyz target;
    cXyz source = eyePos;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 0:
        break;
    case 1:
        target = mLookPos;
        targetP = &target;
        break;
    case 2:
        angle = mLookAngle;
        break;
    }
    if (mTalking && mCanTurn) {
        noTurn = false;
        m_jnt.setTrn();
    }
    if (m_jnt.trnChk()) {
        if (mEventCut.getTurnSpeed() != 0) {
            speed = mEventCut.getTurnSpeed();
        }
        cLib_addCalcAngleS2(&mCurrentTurnSpeed, speed, 4, 0x800);
    } else {
        mCurrentTurnSpeed = 0;
    }
    m_jnt.lookAtTarget(&current.angle.y, targetP, source, angle, mCurrentTurnSpeed, noTurn);
    shape_angle = current.angle;
}

/* 00002520-000025E8       .text playAnm__9daNpcSv_cFv */
void daNpcSv_c::playAnm() {
    mAnmFlags &= ~1;
    if (mpMorf->play(NULL, 0, 0) && mpAnm != NULL && mAnmCount > 0) {
        mAnmCount--;
        if (mAnmCount == 0) {
            mpAnm++;
            if (setAnmTbl(mpAnm)) {
                mAnmFlags |= 1;
            }
        } else {
            setAnm(mpAnm->mAnm, 0, 0.0f);
        }
    }
}

/* 000025E8-000026C8       .text setAnm__9daNpcSv_cFUcif */
void daNpcSv_c::setAnm(u8 index, int mode, f32 morf) {
    if (mMorf >= 0.0f) {
        morf = mMorf;
        mMorf = -1.0f;
    }
    J3DAnmTransform* anm = (J3DAnmTransform*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_bck_ix_tbl[mNpcNo][index]);
    mpMorf->setAnm(anm, mode, morf, 1.0f, 0.0f, -1.0f, NULL);
    mAnmIndex = index;
}

/* 000026C8-00002768       .text setAnmTbl__9daNpcSv_cFP9sSvAnmDat */
bool daNpcSv_c::setAnmTbl(sSvAnmDat* data) {
    if (data->mAnm == 0xFF) {
        mpAnm = NULL;
        return true;
    }
    mpAnm = data;
    mAnmCount = mpAnm->mCount;
    int mode = 2;
    if (mAnmCount > 0) {
        mode = 0;
    }
    if (mAnmIndex != mpAnm->mAnm || mode == 0) {
        setAnm(mpAnm->mAnm, mode, mpAnm->mMorf);
    }
    return false;
}

/* 00002768-000027E0       .text setCollision__9daNpcSv_cFP8dCcD_Cyl4cXyzff */
void daNpcSv_c::setCollision(dCcD_Cyl* cyl, cXyz pos, f32 radius, f32 height) {
    cyl->SetC(pos);
    cyl->SetR(radius);
    cyl->SetH(height);
    dComIfG_Ccsp()->Set(cyl);
}

/* 000027E0-0000293C       .text getTalkNo__9daNpcSv_cFv */
u8 daNpcSv_c::getTalkNo() {
    int hasHook;
    if (dComIfGs_checkGetItem(dItemNo_GRAPPLING_HOOK_e)) {
        hasHook = 1;
    } else {
        hasHook = 0;
    }
    mTalkNo = 0;
    if (!(mTalkFlags & 0x20)) {
        if (!(mTalkFlags & 1)) {
            if (hasHook) {
                mTalkNo = 1;
            } else {
                mTalkNo = 0;
            }
        } else if (!(mTalkFlags & 2)) {
            if (hasHook) {
                mTalkNo = 3;
            } else {
                mTalkNo = 2;
            }
        } else {
            if (hasHook) {
                mTalkNo = 5;
            } else {
                mTalkNo = 4;
            }
        }
    } else if (!hasHook) {
        if (!(mTalkFlags & 4)) {
            mTalkNo = 6;
        } else {
            mTalkNo = 7;
        }
#if VERSION > VERSION_JPN
    } else if (!dComIfGs_isCollect(0, 1)) {
#else
    } else if (!dComIfGs_checkGetItem(dItemNo_MASTER_SWORD_1_e)) {
#endif
        if (!(mTalkFlags & 8)) {
            mTalkNo = 8;
        } else {
            mTalkNo = 9;
        }
    } else if (!dComIfGs_isEventBit(0x2F80)) {
        mTalkNo = 10;
    } else {
        mTalkNo = 11;
    }
    return mTalkNo;
}

/* 0000293C-00002958       .text isTalkOK__9daNpcSv_cFv */
BOOL daNpcSv_c::isTalkOK() {
    if (dComIfGp_checkPlayerStatus0(0, daPyStts0_SWIM_e)) {
        return FALSE;
    } else {
        return TRUE;
    }
}

/* 00002958-00002978       .text daNpc_PeopleCreate__FPv */
static cPhs_State daNpc_PeopleCreate(void* i_this) {
    return ((daNpcSv_c*)i_this)->_create();
}

/* 00002978-0000299C       .text daNpc_PeopleDelete__FPv */
static BOOL daNpc_PeopleDelete(void* i_this) {
    return ((daNpcSv_c*)i_this)->_delete();
}

/* 0000299C-000029C0       .text daNpc_PeopleExecute__FPv */
static BOOL daNpc_PeopleExecute(void* i_this) {
    return ((daNpcSv_c*)i_this)->_execute();
}

/* 000029C0-000029E4       .text daNpc_PeopleDraw__FPv */
static BOOL daNpc_PeopleDraw(void* i_this) {
    return ((daNpcSv_c*)i_this)->_draw();
}

/* 000029E4-000029EC       .text daNpc_PeopleIsDelete__FPv */
static BOOL daNpc_PeopleIsDelete(void*) {
    return TRUE;
}
static actor_method_class daNpc_PeopleMethodTable = {
    (process_method_func)daNpc_PeopleCreate,
    (process_method_func)daNpc_PeopleDelete,
    (process_method_func)daNpc_PeopleExecute,
    (process_method_func)daNpc_PeopleIsDelete,
    (process_method_func)daNpc_PeopleDraw,
};
actor_process_profile_definition g_profile_NPC_SV = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_SV_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpcSv_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_SV_e,
    /* Actor SubMtd */ &daNpc_PeopleMethodTable,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
