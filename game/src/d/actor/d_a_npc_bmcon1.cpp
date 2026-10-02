/**
* d_a_npc_bmcon1.cpp
* NPC - Willi & Obli (Flight Control Platform)
*/
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_bmcon1.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_a_obj.h"
#include "d/d_lib.h"
#include "d/d_com_lib_game.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_audio.h"
#include "JAZelAudio/JAZelAudio_BGM.h"
#include "JAZelAudio/JAZelAudio_SE.h"
#include "res/Object/Bmcon1.h"
#include <stdlib.h>
const char daNpcBmcon_c::m_arcname[] = "Bmcon1";
static const int l_bmd_ix_tbl[] = {dRes_ID_BMCON1_BDL_BM_e, dRes_ID_BMCON1_BDL_BM_e};
static const int l_head_bmd_ix_tbl[] = {dRes_ID_BMCON1_BDL_BMHEAD10_e, dRes_ID_BMCON1_BDL_BMHEAD08_e};
static const int l_arm_bmd_ix_tbl[] = {dRes_ID_BMCON1_BDL_BMARM_e, dRes_ID_BMCON1_BDL_BMARM_e};
static const int l_bck_ix_tbl[] = {dRes_ID_BMCON1_BCK_BM_WAIT01_e, dRes_ID_BMCON1_BCK_BM_WAIT02_e, dRes_ID_BMCON1_BCK_BM_TALK01_e, dRes_ID_BMCON1_BCK_BM_TALK02_e, dRes_ID_BMCON1_BCK_BM_TALK03_e, dRes_ID_BMCON1_BCK_BM_WALK_e, dRes_ID_BMCON1_BCK_BM_TRICON01_e, dRes_ID_BMCON1_BCK_BM_TRICON02_e};
static const int l_arm_bck_ix_tbl[] = {dRes_ID_BMCON1_BCK_BMARM_WAIT01_e, dRes_ID_BMCON1_BCK_BMARM_WAIT02_e, dRes_ID_BMCON1_BCK_BMARM_TALK01_e, dRes_ID_BMCON1_BCK_BMARM_TALK02_e, dRes_ID_BMCON1_BCK_BMARM_TALK03_e, dRes_ID_BMCON1_BCK_BMARM_WALK_e, dRes_ID_BMCON1_BCK_BMARM_TRICON01_e, dRes_ID_BMCON1_BCK_BMARM_TRICON02_e};
static const int l_btp_ix_tbl[] = {0};
static char* l_npc_staff_id[] = {"Bmcon1", "Bmcon2"};
static char* l_arcname_tbl[] = {"Bmcon1", "Bmcon1"};
static sBmconAnmDat l_npc_anm_wait = {0, 8, -1};
static sBmconAnmDat l_npc_anm_wait2 = {1, 8, -1};
static sBmconAnmDat l_npc_anm_talk = {2, 8, -1};
static sBmconAnmDat l_npc_anm_talk2 = {3, 8, -1};
static sBmconAnmDat l_npc_anm_talk3 = {4, 8, -1};
static sBmconAnmDat l_npc_anm_walk = {5, 8, -1};
static sBmconAnmDat l_npc_anm_con1 = {6, 8, -1};
static sBmconAnmDat l_npc_anm_con2 = {7, 8, -1};
static sBmconAnmDat* l_npc_anm_tbl[] = {&l_npc_anm_wait, &l_npc_anm_wait2, &l_npc_anm_talk, &l_npc_anm_talk2, &l_npc_anm_talk3, &l_npc_anm_walk, &l_npc_anm_con1, &l_npc_anm_con2};
struct BmconNpcDat {
    s16 maxHeadX, maxHeadY, maxBackX, maxBackY;
    s16 minHeadX, minHeadY, minBackX, minBackY, maxTurn;
    f32 playerEyeOffset, attentionOffset, eyeOffset, attentionDist, nearDist;
    s16 attentionAngle, turnSpeed, walkTurnSpeed;
    f32 radius, walkAnmScale, walkSpeed;
    s16 unused3C, unused3E, unused40, unused42, walkMin, walkMax, lookDelay;
    u8 canTurn, canLook;
};
static BmconNpcDat l_npc_dat[] = {
    {0x2000, 7000, 0, 7800, -4096, -7000, 0, -7800, 1500, 0.0f, 220.0f, 190.0f, 200.0f, 0.0f, 20000, 1300, 400, 60.0f, 0.5f, 3.0f, 6, 1024, 60, 90, 200, 300, 30, 0, 1},
    {0x2000, 7000, 0, 7800, -4096, -7000, 0, -7800, 1500, 25.0f, 220.0f, 190.0f, 220.0f, 0.0f, 32767, 1300, 400, 60.0f, 0.5f, 3.0f, 6, 1024, 60, 90, 200, 300, 30, 0, 1},
};
static u32 l_msg_bmcon1_1st_appear[] = {0x2a96, 0x2a97, 0x2a98, 0x2a99, 0x1};
static u32 l_msg_bmcon1_appear[] = {0x2a9c, 0x2a9d, 0x3};
static u32 l_msg_bmcon1_not_appear[] = {0x2a9a, 0x0};
static u32 l_msg_bmcon1_not_rupee[] = {0x2a9b, 0x0};
static u32 l_msg_bmcon1_2nd[] = {0x2aad, 0x2a9e, 0x2};
static u32 l_msg_bmcon1_cleared[] = {0x2aae, 0x2};
static u32 l_msg_bmcon1_appear2[] = {0x2a9f, 0x3};
static u32 l_msg_bmcon1_not_appear2[] = {0x2aa0, 0x0};
static u32 l_msg_bmcon1_talk[] = {0x2a95, 0x0};
static u32 l_msg_bmcon1_result[] = {0x2aa1, 0x2aa2, 0x2aa3, 0x2aa4, 0x2aa5};
static u32 l_msg_bmcon1_1st_goal[] = {0x2aa7, 0x2aa8, 0x2aa9, 0x4, 0x2aaa, 0x0};
static u32 l_msg_bmcon1_goal[] = {0x2aab, 0x2aac, 0x5, 0x2aaa, 0x0};
static u32 l_msg_bmcon2_2st_talk[] = {0x2af9, 0x0};
static u32 l_msg_bmcon2_cleared[] = {0x2afe, 0x0};
static u32 l_msg_bmcon2_appear[] = {0x2afd, 0x2afb, 0x2afc, 0x0};
static u32 l_msg_bmcon2_talk[] = {0x2afa, 0x0};
static cXyz l_bmcon_pos_tbl[] = {cXyz(297080.0f, 1100.0f, -202920.0f), cXyz(278900.0f, 1100.0f, -221100.0f)};
daNpcBmcon_c::daNpcBmcon_c() {
    mNpcNo = getPrmNpcNo();
    mResFlag = 0;
    mMode = 0;
    mTargetSpeed = 0.0f;
    mWalkTimer = 0;
    mMorfOverride = -1.0f;
    mLookMode = 0;
    mNoTurn = true;
    mDefaultAngle = home.angle.y;
    mAnmNo = 1;
    m7A0 = 0;
    mFlightResult = 1;
    mChangingStage = 0;
}

static BOOL daNpc_Bmcon_nodeCallBack(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpcBmcon_c* npc = (daNpcBmcon_c*)model->getUserArea();
        int joint = ((J3DJoint*)node)->getJntNo();
        MTXCopy(model->getAnmMtx(joint), *calc_mtx);
        if (joint == npc->get_nec_jnt_num()) {
            mDoMtx_XrotM(*calc_mtx, npc->m_jnt.getHead_y());
            cMtx_ZrotM(*calc_mtx, -npc->m_jnt.getHead_x());
        }
        if (joint == npc->m_jnt.getBackboneJntNum()) {
            mDoMtx_XrotM(*calc_mtx, npc->m_jnt.getBackbone_y());
            cMtx_ZrotM(*calc_mtx, -npc->m_jnt.getBackbone_x());
        }
        if (joint == npc->get_arm_L_jnt_num()) {
            cMtx_copy(*calc_mtx, npc->mArmLMtx);
        }
        if (joint == npc->get_arm_R_jnt_num()) {
            cMtx_copy(*calc_mtx, npc->mArmRMtx);
        }
        model->setAnmMtx(joint, *calc_mtx);
        cMtx_copy(*calc_mtx, J3DSys::mCurrentMtx);
    }
    return TRUE;
}

static BOOL daNpc_Arm_nodeCallBack(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        ((daNpcBmcon_c*)model->getUserArea())->nodeArmControl(node, model);
    }
    return TRUE;
}

void daNpcBmcon_c::nodeArmControl(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_armL1_jnt_num) {
        cMtx_copy(mArmLMtx, J3DSys::mCurrentMtx);
        model->setAnmMtx(joint, mArmLMtx);
    }
    if (joint == m_armR1_jnt_num) {
        cMtx_copy(mArmRMtx, J3DSys::mCurrentMtx);
        model->setAnmMtx(joint, mArmRMtx);
    }
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpcBmcon_c*)actor)->createHeap();
}

static cPhs_State phase_1(daNpcBmcon_c* npc) {
    fopAcM_SetupActor(npc, daNpcBmcon_c);
    fopAc_ac_c* player = dComIfGp_getLinkPlayer();
    if (player == NULL || fpcM_IsCreating(fopAcM_GetID(player))) {
        return cPhs_INIT_e;
    }
    npc->setResFlag(1);
    return cPhs_NEXT_e;
}

static cPhs_State phase_2(daNpcBmcon_c* npc) {
    cPhs_State phase = dComIfG_resLoad(npc->getPhaseP(), l_arcname_tbl[npc->getNpcNo()]);
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(npc, CheckCreateHeap, 0)) {
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

cPhs_State daNpcBmcon_c::_create() {
    static cPhs__Handler l_method[] = {(cPhs__Handler)phase_1, (cPhs__Handler)phase_2, NULL};
    return dComLbG_PhaseHandler(&mPhase, l_method, this);
}

int daNpcBmcon_c::createHeap() {
    J3DModelData* body = (J3DModelData*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_bmd_ix_tbl[mNpcNo]);
    mpMorf = new mDoExt_McaMorf(body, NULL, NULL,
    (J3DAnmTransform*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_bck_ix_tbl[mAnmNo]),
    J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000002);
    J3DModelData* arm = (J3DModelData*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_arm_bmd_ix_tbl[mNpcNo]);
    mpArmMorf = new mDoExt_McaMorf(arm, NULL, NULL,
    (J3DAnmTransform*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_arm_bck_ix_tbl[mAnmNo]),
    J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000002);
    if (mpArmMorf == NULL || mpArmMorf->getModel() == NULL) {
        return FALSE;
    }
    mpHeadModel = mDoExt_J3DModel__create((J3DModelData*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_head_bmd_ix_tbl[mNpcNo]), 0x80000, 0x37441422);
    if (mpHeadModel == NULL) {
        return FALSE;
    }
    m_jnt.setHeadJntNum(body->getJointName()->getIndex("head"));
    JUT_ASSERT(DEMO_SELECT(0x3EE, 0x3EF), m_jnt.getHeadJntNum() >= 0);
    m_jnt.setBackboneJntNum(body->getJointName()->getIndex("backbone"));
    JUT_ASSERT(DEMO_SELECT(0x3F2, 0x3F3), m_jnt.getBackboneJntNum() >= 0);
    m_nec_jnt_num = body->getJointName()->getIndex("neck");
    JUT_ASSERT(DEMO_SELECT(0x3F9, 0x3FA), m_nec_jnt_num >= 0);
    m_arm_L_jnt_num = body->getJointName()->getIndex("armL");
    JUT_ASSERT(DEMO_SELECT(0x3FD, 0x3FE), m_arm_L_jnt_num >= 0);
    m_arm_R_jnt_num = body->getJointName()->getIndex("armR");
    JUT_ASSERT(DEMO_SELECT(0x3FF, 0x400), m_arm_R_jnt_num >= 0);
    m_armL1_jnt_num = arm->getJointName()->getIndex("armLloc");
    JUT_ASSERT(DEMO_SELECT(0x403, 0x404), m_armL1_jnt_num >= 0);
    m_armR1_jnt_num = arm->getJointName()->getIndex("armRloc");
    JUT_ASSERT(DEMO_SELECT(0x405, 0x406), m_armR1_jnt_num >= 0);
    for (u16 i = 0; i < body->getJointNum(); i++) {
        if (i == m_jnt.getHeadJntNum() || i == m_jnt.getBackboneJntNum() || i == m_nec_jnt_num || i == m_arm_L_jnt_num || i == m_arm_R_jnt_num)
        body->getJointNodePointer(i)->setCallBack(daNpc_Bmcon_nodeCallBack);
    }
    mpMorf->getModel()->setUserArea((u32)this);
    for (u16 i = 0; i < arm->getJointNum(); i++) {
        if (i == m_armL1_jnt_num || i == m_armR1_jnt_num) {
            arm->getJointNodePointer(i)->setCallBack(daNpc_Arm_nodeCallBack);
        }
    }
    mpArmMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 30.0f);
    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), fopAcM_GetAngle_p(this), fopAcM_GetShapeAngle_p(this));
    return TRUE;
}

static s16 daNpcBmcon_XyCheckCB(void* actor, int button) {
    return ((daNpcBmcon_c*)actor)->XyCheckCB(button);
}

cPhs_State daNpcBmcon_c::createInit() {
    int weight = 0xFF;
    u8 rail = getPrmRailID();
    if (rail != 0xFF) {
        mPath.setInf(rail, fopAcM_GetRoomNo(this), 1);
        if (mPath.getPath() == NULL) {
            return cPhs_ERROR_e;
        }
        dPath_GetNextRoomPath(mPath.getPath(), -1);
        fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
        cXyz pos = mPath.getPoint(mPath.getIdx());
        old.pos = pos;
        current.pos = old.pos;
        mPath.incIdxLoop();
        mWalkTimer = 1;
        weight = 0xFE;
    }
#if VERSION == VERSION_DEMO
    mStts.Init(weight, 0xFF, this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(&mCyl, current.pos, l_npc_dat[mNpcNo].radius, 150.0f);
#endif
    gravity = -9.0f;
    setAnmTbl(l_npc_anm_tbl[mAnmNo]);
    switch (mNpcNo) {
    case 0:
        mEvent1 = dComIfGp_evmng_getEventIdx("BMCON_RESULT");
        mEvent2 = dComIfGp_evmng_getEventIdx("BMCON_GET_ITEM");
        if (dComIfGs_isTmpBit(0x210)) {
            dComIfGs_offTmpBit(0x210);
            daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
            fopAcM_orderChangeEventId(player, this, mEvent1, 0, 0xFFFF);
            player->onNoResetFlg1(daPy_py_c::daPyFlg1_WATER_DROP);
        }
        break;
    case 1:
        mEvent1 = dComIfGp_evmng_getEventIdx("BMCON_END");
        mEvent2 = dComIfGp_evmng_getEventIdx("BMCON_END2");
        break;
    }
    eventInfo.setXyCheckCB(daNpcBmcon_XyCheckCB);
    mEventCut.setActorInfo2(l_npc_staff_id[mNpcNo], this);
    mTurnSpeed = 0;
    mTalking = 0;
    mAttention = 0;
    mNearPlayer = 0;
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -70.0f, 0.0f, -70.0f, 70.0f, 200.0f, 70.0f);
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 169;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 169;
    attention_info.flags = 10;
    m_jnt.setParam(l_npc_dat[mNpcNo].maxBackX, l_npc_dat[mNpcNo].maxBackY, l_npc_dat[mNpcNo].minBackX, l_npc_dat[mNpcNo].minBackY,
    l_npc_dat[mNpcNo].maxHeadX, l_npc_dat[mNpcNo].maxHeadY, l_npc_dat[mNpcNo].minHeadX, l_npc_dat[mNpcNo].minHeadY, l_npc_dat[mNpcNo].maxTurn);
    mCanTurn = l_npc_dat[mNpcNo].canTurn;
    mCanLook = l_npc_dat[mNpcNo].canLook;
    mAttentionDist = l_npc_dat[mNpcNo].attentionDist;
    mAttentionAngle = l_npc_dat[mNpcNo].attentionAngle;
    mObjAcch.CrrPos(*dComIfG_Bgsp());
#if VERSION == VERSION_DEMO
    current.pos.y = home.pos.y = mObjAcch.GetGroundH();
    setMtx();
#else
    if (mObjAcch.GetGroundH() != -1000000000.0f) {
        current.pos.y = home.pos.y = mObjAcch.GetGroundH();
    }
    setMtx();
    mpMorf->getModel()->calc();
    mStts.Init(weight, 0xFF, this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);
    setCollision(&mCyl, current.pos, l_npc_dat[mNpcNo].radius, 150.0f);
#endif
    return cPhs_COMPLEATE_e;
}

bool daNpcBmcon_c::_delete() {
    if (mResFlag) {
#if VERSION == VERSION_DEMO
        dComIfG_deleteObjectRes(l_arcname_tbl[mNpcNo]);
#else
        dComIfG_resDelete(&mResPhase, l_arcname_tbl[mNpcNo]);
#endif
    }
#if VERSION == VERSION_DEMO
    if (mpMorf != NULL)
#else
    if (heap != NULL && mpMorf != NULL)
#endif
    {
        mpMorf->stopZelAnime();
    }
    return true;
}

bool daNpcBmcon_c::_draw() {
    J3DModel* body = mpMorf->getModel();
    J3DModel* arm = mpArmMorf->getModel();
    J3DModel* head = mpHeadModel;
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(body, &tevStr);
    g_env_light.setLightTevColorType(arm, &tevStr);
    g_env_light.setLightTevColorType(head, &tevStr);
    mpMorf->updateDL();
    mpArmMorf->updateDL();
    head->setBaseTRMtx(body->getAnmMtx(m_jnt.getHeadJntNum()));
    mDoExt_modelUpdateDL(mpHeadModel);
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadow = dComIfGd_setShadow(mShadow, 1, mpMorf->getModel(), &pos, 800.0f, 20.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadow) {
        dComIfGd_addRealShadow(mShadow, head);
        dComIfGd_addRealShadow(mShadow, arm);
    }
    switch (mNpcNo) {
    case 0:
        dSnap_RegistFig(DEMO_SELECT(0x94, 0x95), this, 1.0f, 1.0f, 1.0f);
        break;
    case 1:
        dSnap_RegistFig(DEMO_SELECT(0x95, 0x94), this, 1.0f, 1.0f, 1.0f);
        break;
    }
    return true;
}

static u8 (daNpcBmcon_c::*l_execute_init[])() = {&daNpcBmcon_c::executeWaitInit, &daNpcBmcon_c::executeTalkInit, &daNpcBmcon_c::executeWalkInit, &daNpcBmcon_c::executeTurnInit};
static void (daNpcBmcon_c::*moveProc[])() = {&daNpcBmcon_c::executeWait, &daNpcBmcon_c::executeTalk, &daNpcBmcon_c::executeWalk, &daNpcBmcon_c::executeTurn};
bool daNpcBmcon_c::_execute() {
    chkAttention();
    checkOrder();
    if (!dComIfGp_event_runCheck() || eventInfo.checkCommandTalk()) {
        (this->*moveProc[mMode])();
    } else {
        eventMove();
    }
    eventOrder();
    playAnm();
    if (mAnmNo == 5) {
        cLib_chaseF(&speedF, mTargetSpeed, 0.3f);
        f32 rate = speedF * l_npc_dat[mNpcNo].walkAnmScale;
        if (rate < 0.5f) {
            rate = 0.5f;
        }
        mpMorf->setPlaySpeed(rate);
        mpArmMorf->setPlaySpeed(rate);
    } else cLib_chaseF(&speedF, mTargetSpeed, 0.1f);
    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    mObjAcch.CrrPos(*dComIfG_Bgsp());
    setCollision(&mCyl, current.pos, l_npc_dat[mNpcNo].radius, 150.0f);
    attention_info.position.set(current.pos.x, current.pos.y + l_npc_dat[mNpcNo].attentionOffset, current.pos.z);
    eyePos.set(current.pos.x, current.pos.y + l_npc_dat[mNpcNo].eyeOffset, current.pos.z);
    lookBack();
    setMtx();
    return false;
}

bool daNpcBmcon_c::executeCommon() {
    if (mAttention && !mChangingStage) {
        mOrder = 1;
    } else {
        mOrder = 0;
    }
    if (mTalking == 1 && mMode != 1) {
        executeSetMode(1);
    }
    switch (mNpcNo) {
    case 0: break;
    case 1:
        if (dComIfGp_getStartStageLayer() == 2 || dComIfGp_getStartStageLayer() == 3) {
            cXyz dist = calcFlyDist();
            if (!(mSoundFlags & 1) && dist.x > 10.0f) {
                mSoundFlags |= 1;
                mDoAud_bgmStart(JA_BGM_BIRDMAN);
                mDoAud_seStart(JA_SE_BIRDMAN_TAKE_OFF, NULL, 0, 0);
            }
            if (mFlightResult == 1) {
                if (dist.x > 25710.0f && dist.z < 1131.0f && dist.z > -1131.0f) {
                    mFlightResult = 0;
                    dComIfGp_setItemMagicCount(-dComIfGs_getMaxMagic());
                } else if (dist.x < -1948.0f || dist.x > 25710.0f || dist.z < -3534.0f || dist.z > 3534.0f) {
                    mFlightResult = 2;
                    dComIfGp_setItemMagicCount(-dComIfGs_getMaxMagic());
                }
            }
            if (dComIfGp_checkPlayerStatus0(0, 0x100000)) {
                mFlyDist = dist.x / 100.0f;
                if (mFlyDist < 0) {
                    mFlightResult = 3;
                }
                dComIfGs_setTmpReg(0xF903, mFlightResult);
                if (mFlightResult == 0 || mFlightResult == 1) {
                    mOrder = 3;
                } else {
                    mOrder = 4;
                }
                setFlyDistNow(mFlyDist);
                if (mFlightResult != 2 && mFlyDist > getFlyDistMax()) {
                    setFlyDistMax(mFlyDist);
                }
                old.pos = mPath.getPoint(mPath.getIdx());
                current.pos = old.pos;
                mDefaultAngle = -0x6000;
                current.angle.y = -0x6000;
                if (!(mSoundFlags & 2)) {
                    mSoundFlags |= 2;
                    mDoAud_bgmStop(90);
                }
            }
        }
        break;
    }
    return mTalking;
}

void daNpcBmcon_c::executeSetMode(u8 mode) {
    mTargetSpeed = 0.0f;
    mMode = (this->*l_execute_init[mode])();
}

u8 daNpcBmcon_c::executeWaitInit() {
    speedF = 0.0f;
    if (mNpcNo != 0) {
        setAnmTbl(&l_npc_anm_wait2);
    } else {
        setAnmTbl(&l_npc_anm_wait);
    }
    m_jnt.setParam(l_npc_dat[mNpcNo].maxBackX, l_npc_dat[mNpcNo].maxBackY, l_npc_dat[mNpcNo].minBackX, l_npc_dat[mNpcNo].minBackY,
    l_npc_dat[mNpcNo].maxHeadX, l_npc_dat[mNpcNo].maxHeadY, l_npc_dat[mNpcNo].minHeadX, l_npc_dat[mNpcNo].minHeadY, l_npc_dat[mNpcNo].maxTurn);
    return 0;
}

void daNpcBmcon_c::executeWait() {
    if (!executeCommon() && mNpcNo == 1 && mAttention && !dComIfGs_isEventBit(0x2A40)) {
        mOrder = 2;
    }
}

u8 daNpcBmcon_c::executeTalkInit() {
    mCanTurn = 1;
    return 1;
}

void daNpcBmcon_c::executeTalk() {
    executeCommon();
    if (talk2(1) == 18) {
        mTalking = 0;
        executeSetMode(0);
        mCanTurn = l_npc_dat[mNpcNo].canTurn;
        mCanLook = l_npc_dat[mNpcNo].canLook;
        if (mStartFlight) {
            mChangingStage = 1;
            mOrder = 0;
            dComIfGp_setNextStage("sea", 1, 14, 2, 0.0f, 0, 1, 0);
        } else dComIfGp_event_reset();
    } else setAnmFromMsgTag();
}

u8 daNpcBmcon_c::executeWalkInit() {
    setAnmTbl(&l_npc_anm_walk);
    return 2;
}

void daNpcBmcon_c::executeWalk() {
    if (!executeCommon()) {
        bool end = false;
        if (mPath.chkPointPass(current.pos, mPath.getDir()) && !mPath.nextIdxAuto()) {
            end = true;
        }
        if (!end) {
            cXyz point = mPath.getPoint(mPath.getIdx());
            s16 angle;
            dNpc_calc_DisXZ_AngY(current.pos, point, NULL, &angle);
            mLookAngle = mDefaultAngle = angle;
            mNoTurn = false;
            mMaxTurnSpeed = l_npc_dat[mNpcNo].walkTurnSpeed;
            mLookMode = 2;
            m_jnt.setTrn();
            mTargetSpeed = l_npc_dat[mNpcNo].walkSpeed;
        } else {
            mPath.turnDir();
            cXyz point = mPath.getPoint(mPath.getIdx());
            s16 angle;
            dNpc_calc_DisXZ_AngY(current.pos, point, NULL, &angle);
            mDefaultAngle = angle;
            mPath.setInf(0xFF, fopAcM_GetRoomNo(this), 1);
            executeSetMode(0);
        }
    }
}

u8 daNpcBmcon_c::executeTurnInit() {
    cXyz point = mPath.getPoint(mPath.getIdx());
    s16 angle;
    dNpc_calc_DisXZ_AngY(current.pos, point, NULL, &angle);
    if (angle == current.angle.y) {
        setAnmTbl(&l_npc_anm_walk);
        mWalkTimer = l_npc_dat[mNpcNo].walkMin + cM_rndF(l_npc_dat[mNpcNo].walkMax - l_npc_dat[mNpcNo].walkMin);
        return 2;
    }
    return 3;
}

void daNpcBmcon_c::executeTurn() {
    if (!executeCommon()) {
        cXyz point = mPath.getPoint(mPath.getIdx());
        s16 angle;
        dNpc_calc_DisXZ_AngY(current.pos, point, NULL, &angle);
        mLookAngle = angle;
        mNoTurn = false;
        mLookMode = 2;
        m_jnt.setTrn();
        if (current.angle.y == angle) {
            executeSetMode(2);
        }
    }
}

void daNpcBmcon_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEvent1) && mOrder == 3) {
            mOrder = 0;
        } else if (dComIfGp_evmng_startCheck(mEvent2) && mOrder == 4) {
            mOrder = 0;
        } else if (dComIfGp_evmng_startCheck(mEvent2) && mOrder == 6) {
            mOrder = 0;
        }
    } else if (eventInfo.checkCommandTalk() && (mOrder == 2 || mOrder == 1)) { mTalking = 1; executeSetMode(1); }
}

void daNpcBmcon_c::eventOrder() {
    if (mOrder == 2 || mOrder == 1) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        eventInfo.onCondition(dEvtCnd_CANTALKITEM_e);
        if (mOrder == 2) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (mOrder == 3) {
        fopAcM_orderChangeEventId(dComIfGp_getPlayer(0), this, mEvent1, 0, 0xFFFF);
    } else if (mOrder == 4) {
        fopAcM_orderChangeEventId(dComIfGp_getPlayer(0), this, mEvent2, 0, 0xFFFF);
    } else if (mOrder == 6) {
        fopAcM_orderChangeEventId(dComIfGp_getPlayer(0), this, mEvent2, 0, 0xFFFF);
    }
}

void daNpcBmcon_c::eventMove() {
    if (!chkEndEvent()) {
        bool attention = mEventCut.getAttnFlag();
        if (mEventCut.cutProc()) {
            if (!mEventCut.getAttnFlag()) {
                mEventCut.setAttnFlag(attention);
            }
        } else {
            privateCut();
            setAnmFromMsgTag();
        }
    } else executeSetMode(0);
}

void daNpcBmcon_c::privateCut() {
    static char* cut_name_tbl[] = {"MES_SET", "GET_ITEM"};
    int staff = dComIfGp_evmng_getMyStaffId(l_npc_staff_id[mNpcNo]);
    if (staff != -1) {
        mCut = dComIfGp_evmng_getMyActIdx(staff, cut_name_tbl, 2, 1, 0);
        if (mCut == -1) {
            dComIfGp_evmng_cutEnd(staff);
        } else {
            if (dComIfGp_evmng_getIsAddvance(staff)) {
                switch (mCut) {
                case 0:
                    eventMesSetInit(staff);
                    break;
                case 1:
                    eventGetItemInit();
                    break;
                }
            }
            bool done;
            switch (mCut) {
            case 0:
                done = eventMesSet();
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

void daNpcBmcon_c::eventMesSetInit(int staff) {
    int* msg = dComIfGp_evmng_getMyIntegerP(staff, "MsgNo");
    if (msg != NULL) {
        mpMessage = NULL;
        switch (*msg) {
        case 0:
            setMessage(getMsg());
            break;
        case 1:
            switch (mFlightResult) {
            case 0:
                dComIfGp_setMessageCountNumber(mFlyDist);
                setMessage(0x2AB2);
                dComIfGp_getVibration().StartShock(8, 1, cXyz(0.0f, 1.0f, 0.0f));
                dComIfGp_getVibration().StartShock(4, 30, cXyz(0.0f, 1.0f, 0.0f));
                mDoAud_seStart(JA_SE_BIRDMAN_GOAL_END, NULL, 0, 0);
                break;
            case 1:
                dComIfGp_setMessageCountNumber(mFlyDist);
                setMessage(0x2AB3);
                dComIfGp_getVibration().StartShock(8, 1, cXyz(0.0f, 1.0f, 0.0f));
                dComIfGp_getVibration().StartShock(4, 30, cXyz(0.0f, 1.0f, 0.0f));
                mDoAud_seStart(JA_SE_BIRDMAN_FALL_END, NULL, 0, 0);
                break;
            case 2:
                setMessage(0x2AB0);
                break;
            case 3:
                setMessage(0x2AB5);
                break;
            }
            dComIfGs_onEventBit(0x2901);
            dComIfGs_onTmpBit(0x210);
            break;
        case 2:
            switch (mFlightResult) {
            case 0:
                setMessage(0x2AB4);
                break;
            case 1:
                dComIfGp_setMessageCountNumber(mFlyDist);
                setMessage(0x2AA6);
                break;
            case 2:
                setMessage(0x2AB0);
                break;
            case 3:
                setMessage(0x2AB5);
                break;
            }
            break;
        case 10:
            switch (dComIfGs_getTmpReg(0xF903)) {
            case 0:
                if (dComIfGs_isEventBit(0x2B40)) {
                    mpMessage = l_msg_bmcon1_goal;
                } else {
                    dComIfGs_onEventBit(0x2B40);
                    mpMessage = l_msg_bmcon1_1st_goal;
                }
                break;
            case 1:
                if (dComIfGs_isEventBit(0x2B40)) {
                    setMessage(0x2AAF);
                } else {
                    int index = getFlyDistNow() * 6 / 256;
                    if (index > 4) {
                        index = 4;
                    }
                    setMessage(l_msg_bmcon1_result[index]);
                }
                break;
            case 2:
                setMessage(0x2AB1);
                break;
            case 3:
                setMessage(0x2AB6);
                break;
            }
            break;
        default:
            setMessage(*msg);
            break;
        }
        if (mpMessage != NULL) {
            setMessage(*mpMessage);
        }
    } else {
        mpMessage++;
        setMessage(*mpMessage);
    }
}

bool daNpcBmcon_c::eventMesSet() {
    u16 status = talk2(0);
    if (status == 18) {
        if (mRewardFlags & 1) {
            mRewardFlags &= ~1;
            mItemNo = dItemNo_HEART_PIECE_e;
            mOrder = 6;
        } else if (mRewardFlags & 2) {
            mRewardFlags &= ~2;
            mItemNo = dItemNo_PURPLE_RUPEE_e;
            mOrder = 6;
        }
    }
    return status == 18;
}

void daNpcBmcon_c::eventGetItemInit() {
    fpc_ProcID id = fopAcM_createItemForPresentDemo(&current.pos, mItemNo, 0, -1, fopAcM_GetRoomNo(this), NULL, NULL);
    if (id != fpcM_ERROR_PROCESS_ID_e) {
        dComIfGp_event_setItemPartnerId(id);
    }
}

u16 daNpcBmcon_c::talk2(int start) {
    u16 status = 0xFF;
    if (mCurrMsgBsPcId == fpcM_ERROR_PROCESS_ID_e) {
        if (start == 1) {
            mCurrMsgNo = getMsg();
        }
        mCurrMsgBsPcId = fopMsgM_messageSet(mCurrMsgNo, this);
        mpCurrMsg = NULL;
        mPreviousMsgStatus = 0xFFFF;
    } else if (mpCurrMsg != NULL) {
        status = mpCurrMsg->mStatus;
        switch (status) {
        case 14:
            mpCurrMsg->mStatus = next_msgStatus(&mCurrMsgNo);
            if (mpCurrMsg->mStatus == 15) {
                fopMsgM_messageSet(mCurrMsgNo);
            }
            break;
        case 6:
            if (mPreviousMsgStatus == 15 || mPreviousMsgStatus == 2) {
                chkMsg();
            }
            break;
        case 18:
            mpCurrMsg->mStatus = 19;
            mCurrMsgBsPcId = fpcM_ERROR_PROCESS_ID_e;
            break;
        }
        mPreviousMsgStatus = status;
        anmAtr(status);
    } else mpCurrMsg = fopMsgM_SearchByID(mCurrMsgBsPcId);
    return status;
}

u16 daNpcBmcon_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    switch (*msg) {
    case 0x2AB4:
        *msg = 0x2AFF;
        break;
    default:
        if (mpMessage != NULL) {
            mpMessage++;
            switch (*mpMessage) {
            case 0:
                mpMessage = NULL;
                status = 16;
                break;
            case 1:
                if (mpCurrMsg->mSelectNum == 0) {
                    if (dComIfGs_getRupee() < dComIfGp_getMessageRupee()) {
                        mpMessage = l_msg_bmcon1_not_rupee;
                    } else {
                        mpMessage = l_msg_bmcon1_appear;
                        dComIfGp_setItemRupeeCount(-dComIfGp_getMessageRupee());
                    }
                } else mpMessage = l_msg_bmcon1_not_appear;
                *msg = *mpMessage;
                break;
            case 2:
                if (mpCurrMsg->mSelectNum == 0) {
                    if (dComIfGs_getRupee() < dComIfGp_getMessageRupee()) {
                        mpMessage = l_msg_bmcon1_not_rupee;
                    } else {
                        mpMessage = l_msg_bmcon1_appear2;
                        dComIfGp_setItemRupeeCount(-dComIfGp_getMessageRupee());
                    }
                } else mpMessage = l_msg_bmcon1_not_appear2;
                *msg = *mpMessage;
                break;
            case 3:
                dComIfGp_setItemMagicCount(dComIfGs_getMaxMagic());
                mStartFlight = 1;
                mpMessage = NULL;
                status = 16;
                break;
            case 4:
                mRewardFlags |= 1;
                status = 16;
                break;
            case 5:
                mRewardFlags |= 2;
                status = 16;
                break;
            default:
                *msg = *mpMessage;
                break;
            }
        } else status = 16;
        break;
    }
    return status;
}

u32 daNpcBmcon_c::getMsg() {
    u32 msg = 0;
    mpMessage = NULL;
    if (!dComIfGp_event_chkTalkXY()) {
        switch (mNpcNo) {
        case 0:
            if (mStartFlight) {
                mpMessage = l_msg_bmcon1_talk;
            } else if (!dComIfGs_isEventBit(0x2901)) {
                mpMessage = l_msg_bmcon1_1st_appear;
            } else if (isClear()) {
                mpMessage = l_msg_bmcon1_cleared;
                dComIfGp_setMessageCountNumber(getFlyDistMax());
            } else {
                dComIfGp_setMessageCountNumber(getFlyDistMax());
                mpMessage = l_msg_bmcon1_2nd;
            }
            break;
        case 1:
            if (!dComIfGs_isEventBit(0x2A40)) {
                mpMessage = l_msg_bmcon2_2st_talk;
                dComIfGs_onEventBit(0x2A40);
            } else if (isClear()) {
                mpMessage = l_msg_bmcon2_cleared;
            } else if (dComIfGs_isEventBit(0x2901)) {
                dComIfGp_setMessageCountNumber(getFlyDistMax());
                mpMessage = l_msg_bmcon2_appear;
            } else {
                mpMessage = l_msg_bmcon2_talk;
            }
            break;
        }
    }
    if (mpMessage != NULL) {
        msg = *mpMessage;
    }
    return msg;
}

void daNpcBmcon_c::chkMsg() {
    switch (mCurrMsgNo) {
    case 0x2AB2:
        mDoAud_bgmStart(JA_BGM_BIRDMAN_GOAL);
        break;
    case 0x2AB3:
        mDoAud_bgmStart(JA_BGM_BIRDMAN_FAIL);
        break;
    }
}

void daNpcBmcon_c::setMessage(u32 msg) {
    mCurrMsgNo = msg;
}

void daNpcBmcon_c::setAnmFromMsgTag() {
    switch (dComIfGp_getMesgAnimeAttrInfo()) {
    case 0:
        setAnmTbl(&l_npc_anm_wait);
        break;
    case 5:
        setAnmTbl(&l_npc_anm_wait2);
        break;
    case 6:
        setAnmTbl(&l_npc_anm_talk);
        break;
    case 9:
        setAnmTbl(&l_npc_anm_talk2);
        break;
    case 14:
        mCanLook = 0;
        setAnmTbl(&l_npc_anm_talk3);
        break;
    case 18:
        setAnmTbl(&l_npc_anm_con1);
        break;
    case 19:
        setAnmTbl(&l_npc_anm_con2);
        break;
    }
    dComIfGp_clearMesgAnimeAttrInfo();
}

s8 daNpcBmcon_c::getPrmNpcNo() {
    s8 number = argument;
    if (number < 0 || number >= 2) {
        number = 0;
    }
    return number;
}

u8 daNpcBmcon_c::getPrmRailID() {
    return daObj::PrmAbstract(this, PRM_RAIL_W, PRM_RAIL_S);
}

void daNpcBmcon_c::setMtx() {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
}

void daNpcBmcon_c::chkAttention() {
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
        if (!mAttention) {
            mAttention = 1;
        }
    } else {
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        f32 range = mAttentionDist;
        int maxAngle = mAttentionAngle;
        f32 distance;
        s16 angle;
        dNpc_calc_DisXZ_AngY(current.pos, player->current.pos, &distance, &angle);
        if (mAttention) {
            range += 40.0f;
            maxAngle += 0x71C;
        }
        angle -= shape_angle.y;
        if (range > distance && maxAngle > abs(angle) && player->current.pos.y > current.pos.y - 200.0f) {
            mLookPos = dNpc_playerEyePos(l_npc_dat[mNpcNo].playerEyeOffset);
            mLookMode = 1;
            if (mCanTurn) {
                mNoTurn = false;
            } else {
                mNoTurn = true;
            }
            if (!mCanLook) {
                mLookAngle = mDefaultAngle;
                mNoTurn = false;
                mLookMode = 2;
                m_jnt.setTrn();
            }
            if (!mAttention) {
                mAttention = 1;
            }
        } else {
            if (mAttention == 1) {
                mAttention = 0;
                mLookTimer = l_npc_dat[mNpcNo].lookDelay;
            }
            if (l_npc_dat[mNpcNo].nearDist > distance) {
                mLookPos = dNpc_playerEyePos(l_npc_dat[mNpcNo].playerEyeOffset);
                mLookMode = 1;
                if (mCanTurn) {
                    mNoTurn = false;
                } else {
                    mNoTurn = true;
                }
                if (!mCanLook) {
                    mLookAngle = mDefaultAngle;
                    mNoTurn = false;
                    mLookMode = 2;
                    m_jnt.setTrn();
                }
                mNearPlayer = 1;
            } else {
                mLookMode = 0;
                if (mLookTimer != 0) {
                    mLookTimer--;
                } else {
                    mLookAngle = mDefaultAngle;
                    mNoTurn = false;
                    mLookMode = 2;
                    m_jnt.setTrn();
                }
            }
        }
    }
    mMaxTurnSpeed = l_npc_dat[mNpcNo].turnSpeed;
}

void daNpcBmcon_c::lookBack() {
    s16 turn = mMaxTurnSpeed;
    s16 angle = current.angle.y;
    cXyz* targetP = NULL;
    cXyz target;
    cXyz base = eyePos;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 0: break;
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
        s16 eventTurn = mEventCut.getTurnSpeed();
        if (eventTurn != 0) {
            turn = eventTurn;
        }
        cLib_addCalcAngleS2(&mTurnSpeed, turn, 4, 0x800);
    } else mTurnSpeed = 0;
    m_jnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, noTurn);
    shape_angle = current.angle;
}

void daNpcBmcon_c::playAnm() {
    mAnmFlags &= ~1;
    mpArmMorf->play(NULL, 0, 0);
    if (mpMorf->play(NULL, 0, 0) && mpAnm != NULL && mAnmLoops > 0) {
        mAnmLoops--;
        if (mAnmLoops == 0) {
            mpAnm++;
            if (setAnmTbl(mpAnm)) {
                mAnmFlags |= 1;
            }
        } else setAnm(mpAnm->mAnm, 0, 0.0f);
    }
}

void daNpcBmcon_c::setAnm(u8 anm, int loop, f32 morf) {
    if (mMorfOverride >= 0.0f) {
        morf = mMorfOverride;
        mMorfOverride = -1.0f;
    }
    mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_bck_ix_tbl[anm]), loop, morf, 1.0f, 0.0f, -1.0f, NULL);
    mpArmMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectIDRes(l_arcname_tbl[mNpcNo], l_arm_bck_ix_tbl[anm]), loop, morf, 1.0f, 0.0f, -1.0f, NULL);
    mAnmNo = anm;
}

// Retained assertion text from the unused texture-animation setup.
static void dummy() {
    DEAD_STRING("m_head_tex_pattern != 0");
}

bool daNpcBmcon_c::setAnmTbl(sBmconAnmDat* table) {
    if (table->mAnm == 0xFF) {
        mpAnm = NULL;
        return true;
    }
    mpAnm = table;
    mAnmLoops = mpAnm->mLoops;
    int loop = 2;
    if (mAnmLoops > 0) {
        loop = 0;
    }
    if (mAnmNo != mpAnm->mAnm || loop == 0) {
        setAnm(mpAnm->mAnm, loop, mpAnm->mMorf);
    }
    return false;
}

s16 daNpcBmcon_c::XyCheckCB(int) {
    return 0;
}

void daNpcBmcon_c::setCollision(dCcD_Cyl* cyl, cXyz pos, f32 radius, f32 height) {
    cyl->SetC(pos);
    cyl->SetR(radius);
    cyl->SetH(height);
    dComIfG_Ccsp()->Set(cyl);
}

cXyz daNpcBmcon_c::calcFlyDist() {
    cXyz pos = dComIfGp_getLinkPlayer()->current.pos;
    pos -= l_bmcon_pos_tbl[0];
    mDoMtx_stack_c::YrotS(-0x6000);
    cXyz result;
    mDoMtx_stack_c::multVec(&pos, &result);
    return result;
}

s16 daNpcBmcon_c::getFlyDistMax() {
    u8 low = dComIfGs_getEventReg(0xA7FF);
    u8 high = dComIfGs_getEventReg(0xA8FF);
    return low + (high << 8);
}

void daNpcBmcon_c::setFlyDistMax(s16 dist) {
#if VERSION == VERSION_DEMO
    u8 low = dist % 256;
    u8 high = dist / 256;
    dComIfGs_setEventReg(0xA7FF, low);
    dComIfGs_setEventReg(0xA8FF, high);
#else
    dComIfGs_setEventReg(0xA7FF, dist % 256);
    dComIfGs_setEventReg(0xA8FF, dist / 256);
#endif
}

s16 daNpcBmcon_c::getFlyDistNow() {
    u8 low = dComIfGs_getTmpReg(0xFBFF);
    u8 high = dComIfGs_getTmpReg(0xFAFF);
    return low + (high << 8);
}

void daNpcBmcon_c::setFlyDistNow(s16 dist) {
#if VERSION == VERSION_DEMO
    u8 low = dist % 256;
    u8 high = dist / 256;
    dComIfGs_setTmpReg(0xFBFF, low);
    dComIfGs_setTmpReg(0xFAFF, high);
#else
    dComIfGs_setTmpReg(0xFBFF, dist % 256);
    dComIfGs_setTmpReg(0xFAFF, dist / 256);
#endif
}

int daNpcBmcon_c::chkEndEvent() {
    switch (mNpcNo) {
    case 0:
        if (dComIfGp_evmng_endCheck(mEvent1) && dComIfGs_getTmpReg(0xF903)) {
            dComIfGp_event_reset();
            mCanLook = l_npc_dat[mNpcNo].canLook;
            return 1;
        }
        if (dComIfGp_evmng_endCheck(mEvent2)) {
            dComIfGp_event_reset();
            mCanLook = l_npc_dat[mNpcNo].canLook;
            return 1;
        }
        break;
    case 1:
        if (dComIfGp_evmng_endCheck(mEvent1) || dComIfGp_evmng_endCheck(mEvent2)) {
            dComIfGp_event_reset();
            mCanLook = l_npc_dat[mNpcNo].canLook;
            return 1;
        }
        break;
    }
    return 0;
}

int daNpcBmcon_c::isClear() {
    return dComIfGs_isEventBit(0x2B40) ? 1 : 0;
}

/* 00003DFC-00003E1C       .text daNpc_BmconCreate__FPv */
static cPhs_State daNpc_BmconCreate(void* i_this) {
    return ((daNpcBmcon_c*)i_this)->_create();
}

/* 00003E1C-00003E40       .text daNpc_BmconDelete__FPv */
static BOOL daNpc_BmconDelete(void* i_this) {
    return ((daNpcBmcon_c*)i_this)->_delete();
}

/* 00003E40-00003E64       .text daNpc_BmconExecute__FPv */
static BOOL daNpc_BmconExecute(void* i_this) {
    return ((daNpcBmcon_c*)i_this)->_execute();
}

/* 00003E64-00003E88       .text daNpc_BmconDraw__FPv */
static BOOL daNpc_BmconDraw(void* i_this) {
    return ((daNpcBmcon_c*)i_this)->_draw();
}

/* 00003E88-00003E90       .text daNpc_BmconIsDelete__FPv */
static BOOL daNpc_BmconIsDelete(void*) {
    return TRUE;
}

static actor_method_class daNpc_BmconMethodTable = {
    (process_method_func)daNpc_BmconCreate,
    (process_method_func)daNpc_BmconDelete,
    (process_method_func)daNpc_BmconExecute,
    (process_method_func)daNpc_BmconIsDelete,
    (process_method_func)daNpc_BmconDraw,
};
actor_process_profile_definition g_profile_NPC_BMCON1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BMCON1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpcBmcon_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BMCON1_e,
    /* Actor SubMtd */ &daNpc_BmconMethodTable,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
