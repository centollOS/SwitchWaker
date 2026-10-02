/**
 *d_a_npc_bms1.cpp
 *NPC - Bomb-Master Cannon (Windfall Bomb Shop shopkeeper)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_bms1.h"
#include "d/d_cc_d.h"
#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_snap.h"
#include "d/d_item.h"
#include "d/d_a_obj.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_controller_pad.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Bms.h"

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
        /* SrcGObjAt HitMark*/ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark*/ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCylS
    {{
        /* Center*/ {0.0f, 0.0f, 0.0f},
        /* Radius*/ 30.0f,
        /* Height*/ 80.0f,
    }},
};

STATIC_ASSERT(sizeof(daNpc_Bms1_c) == 0x8A8);
static fpc_ProcID l_msgId;
static msg_class* l_msg;

class daNpc_Bms1_childHIO_c : public JORReflexible {
public:
    daNpc_Bms1_childHIO_c();
    virtual ~daNpc_Bms1_childHIO_c() {}
    void genMessage(JORMContext*) {}
    dNpc_HIO_c mNpc;
    f32 mCursorScale, mCursor48, mCursor4C, mCursor50, mCursor38;
    f32 mSpring, mDamping, mBlend, mStretch;
};

class daNpc_Bms1_HIO_c : public JORReflexible {
public:
    daNpc_Bms1_HIO_c();
    virtual ~daNpc_Bms1_HIO_c() {}
    void genMessage(JORMContext*) {}
    s8 m04;
    s32 m08;
    daNpc_Bms1_childHIO_c mChild[1];
};

static daNpc_Bms1_HIO_c l_HIO;
char daNpc_Bms1_c::m_arcname[] = "Bms";
daNpc_Bms1_childHIO_c::daNpc_Bms1_childHIO_c() {
}

static BOOL daNpc_Bms1_shopMsgCheck(u32 msg) {
    if ((msg >= 0x2788 && msg <= 0x2790) || (msg >= 0x277A && msg <= 0x2781) || msg == 0x2783 || msg == 0x2776) return TRUE;
    return FALSE;
}

static BOOL daNpc_Bms1_shopStickMoveMsgCheck(u32 msg) {
    if ((msg >= 0x2788 && msg <= 0x278A) || (msg >= 0x277A && msg <= 0x277C) || msg == 0x2783 || msg == 0x2776) return TRUE;
    return FALSE;
}

daNpc_Bms1_HIO_c::daNpc_Bms1_HIO_c() {
    mChild[0].mNpc.m04 = -50.0f;
    mChild[0].mNpc.mMaxHeadX = 5000;
    mChild[0].mNpc.mMaxHeadY = 10000;
    mChild[0].mNpc.mMaxBackboneX = 0;
    mChild[0].mNpc.mMaxBackboneY = 2100;
    mChild[0].mNpc.mMinHeadX = -8190;
    mChild[0].mNpc.mMinHeadY = 0;
    mChild[0].mNpc.mMinBackboneX = 0;
    mChild[0].mNpc.mMinBackboneY = 0;
    mChild[0].mNpc.mMaxTurnStep = 0x1000;
    mChild[0].mNpc.mMaxHeadTurnVel = 0x800;
    mChild[0].mNpc.mAttnYOffset = 40.0f;
    mChild[0].mNpc.m22 = 0;
    mChild[0].mNpc.mMaxAttnDistXZ = 300.0f;
    mChild[0].mCursorScale = 0.65f;
    mChild[0].mCursor48 = 0.9f;
    mChild[0].mCursor4C = 0.5f;
    mChild[0].mCursor50 = 27.0f;
    mChild[0].mCursor38 = 20.0f;
    mChild[0].mSpring = 0.45f;
    mChild[0].mDamping = 0.8f;
    mChild[0].mBlend = 0.9f;
    mChild[0].mStretch = 0.00005f;
    m04 = -1;
    m08 = -1;
}

static const int l_bck_ix_tbl[] = {
    dRes_INDEX_BMS_BCK_BY1_WAIT01_e,
    dRes_INDEX_BMS_BCK_BY1_WAIT02_e,
    dRes_INDEX_BMS_BCK_BY1_TALK01_e,
    dRes_INDEX_BMS_BCK_BY1_TALK02_e,
    dRes_INDEX_BMS_BCK_BY1_BOW_e,
    dRes_INDEX_BMS_BCK_BY1_TALK01_e,
    dRes_INDEX_BMS_BCK_BY1_TALK02_e
};

static const int l_btp_ix_tbl[] = {
    dRes_INDEX_BMS_BTP_MABA01_e,
    dRes_INDEX_BMS_BTP_BY_T01_e
};

static BOOL nodeCallBack_Bms(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Bms1_c* actor = (daNpc_Bms1_c*) model->getUserArea();
        int joint = ((J3DJoint*) node)->getJntNo();
        if (actor) {
            MTXCopy(model->getAnmMtx(joint), *calc_mtx);
            if (joint == actor->getHeadJntNum()) {
                cXyz offset(0.0f, 0.0f, 0.0f), pos;
#if VERSION == VERSION_DEMO
                cMtx_YrotM(*calc_mtx, -actor->getHead_y() - actor->mHeadAnm.field_0x02);
                cMtx_ZrotM(*calc_mtx, -actor->getHead_x() - actor->mHeadAnm.field_0x00);
#else
                mDoMtx_YrotM(*calc_mtx, -actor->getHead_y() - actor->mHeadAnm.field_0x02);
                mDoMtx_ZrotM(*calc_mtx, -actor->getHead_x() - actor->mHeadAnm.field_0x00);
#endif
                MtxPosition(&offset, &pos);
                actor->setAttentionBasePos(pos);
                offset.set(28.0f, -20.0f, 0.0f);
                MtxPosition(&offset, &pos);
                actor->setEyePos(pos);
            } else if (joint == actor->getBackboneJntNum()) {
#if VERSION == VERSION_DEMO
                cMtx_XrotM(*calc_mtx, actor->getBackbone_y());
                cMtx_ZrotM(*calc_mtx, actor->getBackbone_x());
#else
                mDoMtx_XrotM(*calc_mtx, actor->getBackbone_y());
                mDoMtx_ZrotM(*calc_mtx, actor->getBackbone_x());
#endif
            }
            MTXCopy(*calc_mtx, j3dSys.mCurrentMtx);
            model->setAnmMtx(joint, *calc_mtx);
        }
    }
    return TRUE;
}

static BOOL nodeCallBack_BmsHead(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Bms1_c* actor = (daNpc_Bms1_c*) model->getUserArea();
        int joint = ((J3DJoint*) node)->getJntNo();
        if (actor) {
            static cXyz zero(cXyz::Zero);
            static cXyz tip(40.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::copy(model->getAnmMtx(joint));
            Mtx matrix;
            cXyz trans, origin, end, previous;
            Quaternion rotation;
            if (joint == actor->getHairLJntNum()) {
                cXyz scale = actor->mHairScaleL;
                scale.x *= actor->mHairStretchL;
                mDoMtx_stack_c::scaleM(scale.x, scale.y, scale.z);
                MTXCopy(mDoMtx_stack_c::get(), matrix);
                trans.x = matrix[0][3];
                trans.y = matrix[1][3];
                trans.z = matrix[2][3];
                matrix[0][3] = matrix[1][3] = matrix[2][3] = 0.0f;
                mDoMtx_stack_c::transS(trans);
                mDoMtx_stack_c::quatM(&actor->mHairQuatL);
                mDoMtx_stack_c::concat(matrix);
                mDoMtx_stack_c::multVec(&tip, &end);
                mDoMtx_stack_c::multVec(&zero, &origin);
                model->setAnmMtx(joint, mDoMtx_stack_c::get());
                if (actor->mHairPosL.isZero()) actor->mHairPosL = end;
                cXyz acceleration = (end - actor->mHairPosL) * l_HIO.mChild[0].mSpring;
                actor->mHairVelocityL += acceleration;
                actor->mHairVelocityL *= l_HIO.mChild[0].mDamping;
                actor->mHairPosL += actor->mHairVelocityL;
                previous = actor->mHairPosL;
                end -= origin;
                previous -= origin;
                daObj::quat_rotVec(&rotation, end, previous);
                f32 blend = l_HIO.mChild[0].mBlend;
                C_QUATSlerp(&actor->mHairQuatL, &rotation, &actor->mHairQuatL, blend);
                actor->mHairStretchL = 1.0f - l_HIO.mChild[0].mStretch * previous.inprod(actor->mHairVelocityL);
                actor->mHairStretchL = cLib_minMaxLimit(actor->mHairStretchL, 0.5f, 1.5f);
            } else if (joint == actor->getHairRJntNum()) {
                cXyz scale = actor->mHairScaleR;
                scale.y *= actor->mHairStretchR;
                mDoMtx_stack_c::scaleM(scale.x, scale.y, scale.z);
                MTXCopy(mDoMtx_stack_c::get(), matrix);
                trans.x = matrix[0][3];
                trans.y = matrix[1][3];
                trans.z = matrix[2][3];
                matrix[0][3] = matrix[1][3] = matrix[2][3] = 0.0f;
                mDoMtx_stack_c::transS(trans);
                mDoMtx_stack_c::quatM(&actor->mHairQuatR);
                mDoMtx_stack_c::concat(matrix);
                mDoMtx_stack_c::multVec(&tip, &end);
                mDoMtx_stack_c::multVec(&zero, &origin);
                model->setAnmMtx(joint, mDoMtx_stack_c::get());
                if (actor->mHairPosR.isZero()) actor->mHairPosR = end;
                cXyz acceleration = (end - actor->mHairPosR) * l_HIO.mChild[0].mSpring;
                actor->mHairVelocityR += acceleration;
                actor->mHairVelocityR *= l_HIO.mChild[0].mDamping;
                actor->mHairPosR += actor->mHairVelocityR;
                previous = actor->mHairPosR;
                end -= origin;
                previous -= origin;
                daObj::quat_rotVec(&rotation, end, previous);
                f32 blend = l_HIO.mChild[0].mBlend;
                C_QUATSlerp(&actor->mHairQuatR, &rotation, &actor->mHairQuatR, blend);
                actor->mHairStretchR = 1.0f - l_HIO.mChild[0].mStretch * previous.inprod(actor->mHairVelocityR);
                actor->mHairStretchR = cLib_minMaxLimit(actor->mHairStretchR, 0.5f, 1.5f);
            }
        }
    }
    return TRUE;
}

void daNpc_Bms1_c::set_mtx() {
    J3DModel* model = mpMorf->getModel();
    if (mType == 0) {
        cXyz offset(0.0f, 100.0f, 35.0f);
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y);
        mDoMtx_stack_c::transM(offset.x, offset.y, offset.z);
    } else {
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(current.angle.y);
    }
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoMtx_stack_c::copy(model->getAnmMtx(m_head_jnt_num));
    mpHeadModel->setBaseTRMtx(mDoMtx_stack_c::get());
    if (mType == 1) {
        mDoMtx_stack_c::copy(model->getAnmMtx(m_head_jnt_num));
        mpModel354->setBaseTRMtx(mDoMtx_stack_c::get());
        mDoMtx_stack_c::copy(model->getAnmMtx(m_head_jnt_num));
        mpModel358->setBaseTRMtx(mDoMtx_stack_c::get());
    } else if (dComIfGs_isEventBit(0xA02)) {
        mDoMtx_stack_c::copy(model->getAnmMtx(m_head_jnt_num));
        mpModel354->setBaseTRMtx(mDoMtx_stack_c::get());
    }
    if (mpLegModel) {
        mDoMtx_stack_c::copy(model->getAnmMtx(m_leg_jnt_num));
        mpLegModel->setBaseTRMtx(mDoMtx_stack_c::get());
    }
    if (mpModel360) {
        mDoMtx_stack_c::transS(current.pos);
        mpModel360->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

BOOL daNpc_Bms1_c::initTexPatternAnm(bool modify) {
    J3DModelData* data = mpHeadModel->getModelData();
    m_head_tex_pattern = (J3DAnmTexPattern*) dComIfG_getObjectRes(m_arcname, l_btp_ix_tbl[mBtpNo]);
    JUT_ASSERT(0x23E, m_head_tex_pattern != 0);
    if (!mBtpAnm.init(data, m_head_tex_pattern, TRUE, 2, 1.0f, 0, -1, modify, 0)) return FALSE;
    mBtpFrame = 0;
    mBtpTimer = 0;
    return TRUE;
}

void daNpc_Bms1_c::playTexPatternAnm() {
    if (cLib_calcTimer(&mBtpTimer) == 0) {
        if (mBtpFrame >= m_head_tex_pattern->getFrameMax()) {
            mBtpFrame -= m_head_tex_pattern->getFrameMax();
            mBtpTimer = 30.0f + cM_rndF(100.0f);
        } else mBtpFrame++;
    }
}

void daNpc_Bms1_c::setAnm(s8 number, f32 morf) {
    static int play_mode_tbl[] = {
        2, 2, 2, 2, 2, 2, 2
    };
    static f32 morf_frame_tbl[] = {
        8, 8, 8, 8, 8, 8, 8
    };
    static f32 play_speed_tbl[] = {
        1, 1, 1, 1, 1, 1, 1
    };
    if (morf < 0.0f) morf = morf_frame_tbl[number];
    if (number != mAnmNo || mAnmNo == -1) {
        mAnmNo = number;
        dNpc_setAnm_2(mpMorf, play_mode_tbl[number], morf, play_speed_tbl[number], l_bck_ix_tbl[number], 0, m_arcname);
    }
}

void daNpc_Bms1_c::setTexAnm(s8 number) {
    if (mBtpNo != number || mBtpNo == -1) {
        mBtpNo = number;
        initTexPatternAnm(true);
    }
}

void daNpc_Bms1_c::setAnmFromMsgTag() {
    if (mType == 1) return;
    switch (dComIfGp_getMesgAnimeAttrInfo()) {
    case 0:
        setAnm(0, -1.0f);
        break;
    case 1:
        setAnm(1, -1.0f);
        break;
    case 2:
        setAnm(2, -1.0f);
        break;
    case 3:
        setAnm(3, -1.0f);
        break;
    case 4:
        setAnm(4, -1.0f);
        mAnmTimer = 1;
        break;
    case 5:
        setAnm(5, -1.0f);
        mAnmTimer = 3;
        break;
    case 6:
        setAnm(6, -1.0f);
        mAnmTimer = 3;
        break;
    }
    if (mAnmNo == 5) {
        if (mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f) &&-- mAnmTimer <= 0) setAnm(0, 13.0f);
    } else if (mAnmNo == 6) {
        if (mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f) &&-- mAnmTimer <= 0) setAnm(1, -1.0f);
    } else if (mAnmNo == 4) {
        if (mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f) &&-- mAnmTimer <= 0) setAnm(1, -1.0f);
    }
    dComIfGp_clearMesgAnimeAttrInfo();
}

bool daNpc_Bms1_c::chkAttention(cXyz pos, s16) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 distance = l_HIO.mChild[0].mNpc.mMaxAttnDistXZ;
    cXyz delta = player->current.pos - pos;
    cXyz forward(cM_ssin(current.angle.y), 0.0f, cM_scos(current.angle.y));
    return distance > forward.inprod(delta);
}

void daNpc_Bms1_c::eventOrder() {
    if (mOrder == 4) fopAcM_orderOtherEvent2(this, "BMS_LAND_DEMO", 1, 0xFFFF);
    else if (mOrder == 3) fopAcM_orderOtherEvent2(this, "BMS_GET_DEMO", 1, 0xFFFF);
    else if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mOrder == 1) fopAcM_orderSpeakEvent(this);
    }
}

void daNpc_Bms1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (mOrder == 4) {
            mOrder = 0;
            mShopCam.Reset();
            setAction(&daNpc_Bms1_c::event_action, NULL);
        } else if (mOrder == 3) {
            mOrder = 0;
            mShopCam.Reset();
            setAction(&daNpc_Bms1_c::getdemo_action, NULL);
        }
    } else if (eventInfo.checkCommandTalk()) {
        if (mOrder == 1 || mOrder == 2) {
            mOrder = 0;
            mTalking = true;
            talkInit();
            mShopCam.shop_cam_action_init();
            cXyz pos(- 70.0f, 0.0f, 150.0f);
            daPy_getPlayerActorClass()->setPlayerPosAndAngle(&pos, -0x7000);
        }
    } else mShopCam.Save();
}

u16 daNpc_Bms1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    u32 previous = *msg;
    switch (previous) {
    case 0x2775:
    case 0x2777:
    case 0x2780:
    case 0x2782:
        ++*msg;
        break;
    case 0x2784:
        *msg = 0x2786;
        break;
    case 0x2776:
        if (CPad_CHECK_TRIG_B(0)) * msg = 0x2779;
        else {
            dComIfGp_setDoStatusForce(0x17);
            dComIfGp_setAStatusForce(0x27);
            status = 14;
        }
        break;
    case 0x2783:
        if (CPad_CHECK_TRIG_B(0)) * msg = 0x2787;
        else {
            dComIfGp_setDoStatusForce(0x17);
            dComIfGp_setAStatusForce(0x27);
            status = 14;
        }
        break;
    case 0x277A:
    case 0x277B:
    case 0x277C:
        if (CPad_CHECK_TRIG_B(0)) * msg = 0x2779;
        else {
            dComIfGp_setDoStatusForce(0x17);
            dComIfGp_setAStatusForce(0x27);
            *msg += 3;
        }
        break;
    case 0x2788:
    case 0x2789:
    case 0x278A:
        if (CPad_CHECK_TRIG_B(0)) * msg = 0x2787;
        else {
            dComIfGp_setDoStatusForce(0x17);
            dComIfGp_setAStatusForce(0x27);
            *msg += 3;
        }
        break;
    case 0x2778:
    case 0x2781:
        *msg = 0x2776;
        break;
    case 0x2786:
    case 0x278E:
    case 0x278F:
        *msg = 0x2783;
        break;
    case 0x277D:
    case 0x277E:
    case 0x277F:
        if (dComIfGp_checkMesgCancelButton()) * msg = previous - 3;
        else if (l_msg->mSelectNum == 0) * msg = 0x2780;
        else * msg -= 3;
        break;
    case 0x278B:
    case 0x278C:
    case 0x278D:
        if (dComIfGp_checkMesgCancelButton()) * msg = previous - 3;
        else if (l_msg->mSelectNum == 0) {
            int price = dComIfGp_getMessageRupee();
            u8 error = dShop_BoughtErrorStatus(&mShopItems, 0, DEMO_SELECT(-1, price));
            if (error & 0x20) * msg = 0x278E;
            else if (error & 4) * msg = 0x278F;
            else {
                mDoAud_seStart(JA_SE_SHOP_BOUGHT, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
                mShopItems.hideSelectItem();
                mItemNo = mShopItems.getSelectItemNo();
                dComIfGp_setItemRupeeCount(- price);
                if (!checkItemGet((int)mShopItems.getSelectItemNo(), 1)) {
                    mOrder = 3;
                    status = 16;
                } else {
                    execItemGet((int)mShopItems.getSelectItemNo());
                    *msg = 0x2790;
                }
            }
        } else * msg -= 3;
        break;
    case 0x2790:
        *msg = 0x2783;
        break;
    default:
        status = 16;
        break;
    }
    return status;
}

u32 daNpc_Bms1_c::getMsg() {
    u32 msg;
    if (mOverrideMsgNo) {
        msg = mOverrideMsgNo;
        mOverrideMsgNo = 0;
    } else if (!dComIfGs_isEventBit(0xA02)) msg = 0x2775;
    else msg = 0x2782;
    return msg;
}

void daNpc_Bms1_c::setCollision() {
    cXyz offset(0.0f, 0.0f, 0.0f), pos;
    offset.z = -16.0f;
    MtxTrans(current.pos.x, current.pos.y, current.pos.z, 0);
    cMtx_YrotM(*calc_mtx, mInitialAngle.y);
    MtxPosition(&offset, &pos);
    f32 r = 46.0f;
    f32 h = 130.0f;
    mCyl.SetC(pos);
    mCyl.SetR(r);
    mCyl.SetH(h);
    dComIfG_Ccsp()->Set(&mCyl);
}

void daNpc_Bms1_c::talkInit() {
    mTalkState = 0;
}

u16 daNpc_Bms1_c::normal_talk() {
    u16 status = l_msg->mStatus;
    if (status == 14) {
        l_msg->mStatus = next_msgStatus(&mMsgNo);
        if (l_msg->mStatus == 15) fopMsgM_messageSet(mMsgNo);
    } else if (status == 18) l_msg->mStatus = 19;
    else if (status == 1) fopMsgM_demoMsgFlagOn();
    cXyz pos = mShopCam.getItemZoomPos(100.0f);
    mShopItems.Item_ZoomUp(pos);
    mpShopCursor->hide();
    if (dComIfGp_checkMesgSendButton()) mMsgNo = l_msg->mMsgNo;
    return status;
}

u16 daNpc_Bms1_c::shop_talk() {
    mpShopCursor->show();
    if (dShop_now_triggercheck(l_msg, &mStick, &mShopItems, &mMsgNo, NULL, NULL)) {
        mItemChanged = 1;
        mNextMsgNo = 0;
        if (mShopItems.mSelectedItemIdx >= 0 && !checkItemGet(0x69, 1)) mHeadAnm.swing_vertical_init(1, 0x1800, 0x1000, 1);
    }
    u16 status = l_msg->mStatus;
    if (status == 14 || status == 15) {
        if (mItemChanged) mItemChanged = 0;
        else {
            mNextMsgNo = mMsgNo;
            l_msg->mStatus = next_msgStatus(&mNextMsgNo);
            if (l_msg->mStatus == 15) fopMsgM_messageSet(mNextMsgNo);
        }
    } else if (status == 18) {
        l_msg->mStatus = 19;
        mShopItems.mSelectedItemIdx = -1;
    } else if (status == 1) fopMsgM_demoMsgFlagOn();
    return status;
}

u16 daNpc_Bms1_c::talk() {
    u16 status = 255;
    if (mTalkState == 0) {
        l_msgId = fpcM_ERROR_PROCESS_ID_e;
        l_msg = NULL;
        mMsgNo = getMsg();
        mNextMsgNo = 0;
        mTalkState = 1;
        mItemNo = 0xFF;
    } else if (mTalkState != -1) {
        if (l_msgId == fpcM_ERROR_PROCESS_ID_e) l_msgId = fopMsgM_messageSet(mMsgNo, this);
        else if (!l_msg) {
            l_msg = fopMsgM_SearchByID(l_msgId);
            if (l_msg) {
                if (!daNpc_Bms1_shopStickMoveMsgCheck(mMsgNo)) mTalkState = 2;
                else mTalkState = 3;
            }
        } else {
            setAnmFromMsgTag();
            switch (mTalkState) {
            case 2:
                status = normal_talk();
                break;
            case 3:
                status = shop_talk();
                break;
            }
            if (dComIfGp_checkMesgSendButton()) {
                mMsgNo = l_msg->mMsgNo;
                if (!daNpc_Bms1_shopStickMoveMsgCheck(mMsgNo)) {
                    if (!daNpc_Bms1_shopMsgCheck(mMsgNo)) {
                        mShopItems.mSelectedItemIdx = -1;
                        mShopItems.showItem();
                    }
                    mTalkState = 2;
                } else {
                    if (mMsgNo == 0x2776 || mMsgNo == 0x2783) {
                        mShopItems.mSelectedItemIdx = -1;
                        mShopItems.showItem();
                    }
                    mTalkState = 3;
                }
            }
        }
    }
    mShopCam.m54 = mShopItems.mSelectedItemIdx;
    return status;
}

BOOL daNpc_Bms1_c::CreateInit() {
    cXyz zero(0.0f, 0.0f, 0.0f);
    mInitialAngle = current.angle;
    attention_info.flags = 10;
    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 173;
    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 173;
    gravity = -30.0f;
    switch (mType) {
    case 0:
        setAction(&daNpc_Bms1_c::wait_action, NULL);
        break;
    case 1:
        setAction(&daNpc_Bms1_c::wait_action, NULL);
        break;
    }
    mAttentionBasePos = current.pos;
    mStts.Init(255, 255, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    m8A2 = 0;
    mItemChanged = 0;
    mOverrideMsgNo = 0;
    mItemNo = 255;
    int shop = checkItemGet(0x69, 1) != 0 ? 2 : 1;
    if (mType == 1) {
        mHairScaleL.set(1.2f, 0.9f, 0.9f);
        mHairScaleR.set(0.7f, 0.6f, 0.6f);
    } else if (checkItemGet(0x69, 1)) {
        mHairScaleL.set(0.4f, 0.4f, 0.4f);
        mHairScaleR.set(0.4f, 0.4f, 0.4f);
    } else {
        mHairScaleL.set(1.2f, 0.9f, 0.9f);
        mHairScaleR.set(0.7f, 0.6f, 0.6f);
    }
    mHairStretchL = 1.0f;
    mHairStretchR = 1.0f;
    mHairQuatL = ZeroQuat;
    mHairQuatR = ZeroQuat;
    mShopCam.setCamDataIdx(shop);
    mShopItems.setItemDataIdx(shop);
    mShopCam.setCamAction(NULL);
    mShopItems.setItemSetDataList();
    mShopItems.createItem(3, fopAcM_GetRoomNo(this));
    mDemo = 0;
    mPlayerHeight = current.pos.y;
    mEventCut.setActorInfo("Bms1", this);
    mEventCut.setJntCtrlPtr(&mJnt);
    mpMorf->calc();
    set_mtx();
    return TRUE;
}

void daNpc_Bms1_c::setAttention(bool force) {
    if (force || mAttnState < 2) attention_info.position.set(mAttentionBasePos.x, mAttentionBasePos.y + l_HIO.mChild[0].mNpc.mAttnYOffset, mAttentionBasePos.z);
}

BOOL daNpc_Bms1_c::checkPlayerLanding() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    f32 height = player->current.pos.y;
    BOOL result = FALSE;
    if (mPlayerHeight - height > 200.0f && !player->checkPlayerFly()) result = TRUE;
    if (!player->checkPlayerFly()) mPlayerHeight = height;
    return result;
}

void daNpc_Bms1_c::lookBack() {
    cXyz target, base(0.0f, 0.0f, 0.0f);
    cXyz * targetP = NULL;
    s16 angle = current.angle.y;
    switch (mState) {
    case 1:
        if (mHasAttention) {
            target = dNpc_playerEyePos(l_HIO.mChild[0].mNpc.m04);
            targetP = &target;
            base = current.pos;
            base.y = eyePos.y;
        } else targetP = NULL;
        break;
    case 2:
        if (mShopItems.mSelectedItemIdx == -1) target = mShopCam.getItemZoomPos(100.0f);
        else {
            cXyz itemPos = mShopItems.getSelectItemBasePos();
            target = mShopItems.getSelectItemPos();
            mpShopCursor->setPos(itemPos);
            mpShopCursor->setScale(l_HIO.mChild[0].mCursorScale, l_HIO.mChild[0].mCursor48, l_HIO.mChild[0].mCursor4C, l_HIO.mChild[0].mCursor50, l_HIO.mChild[0].mCursor38);
            mpShopCursor->anm_play();
        }
        targetP = &target;
        base = current.pos;
        base.y = eyePos.y;
        break;
    }
    if (mJnt.trnChk()) cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mChild[0].mNpc.mMaxHeadTurnVel, 4, 0x800);
    else mTurnSpeed = 0;
    mJnt.lookAtTarget(&current.angle.y, targetP, base, angle, mTurnSpeed, true);
}

u8 daNpc_Bms1_c::wait01() {
    if (mTalking) {
        mPreviousState = mState;
        mState = 2;
    } else if (mHasAttention) {
        if (mType == 1) mOrder = 0;
        else mOrder = 2;
    }
    return(bool) mpMorf->isMorf();
}

u8 daNpc_Bms1_c::talk01() {
    u16 status = talk();
    if (status == 18) {
        daPy_py_c* player = daPy_getPlayerActorClass();
        mState = mPreviousState;
        dComIfGp_event_reset();
        mShopCam.Reset();
        player->offPlayerNoDraw();
        mTalking = false;
    } else if (daNpc_Bms1_shopMsgCheck(mMsgNo) && status == 8 && mMsgNo == mShopItems.getSelectItemBuyMsg()) {
        dComIfGp_setDoStatusForce(0x17);
        dComIfGp_setAStatusForce(0x27);
    }
    return(bool) mpMorf->isMorf();
}

int daNpc_Bms1_c::getdemo_action(void*) {
    int staff = dComIfGp_evmng_getMyStaffId("Bms1", NULL, 0);
    if (mActionState == 0) {
        daPy_getPlayerActorClass()->offPlayerNoDraw();
        mState = mPreviousState;
        mShopCam.Reset();
        fpc_ProcID item = fopAcM_createItemForPresentDemo(&current.pos, mShopItems.getSelectItemNo(), 0, -1, DEMO_SELECT(-1, fopAcM_GetRoomNo(this)), NULL, NULL);
        if (item != fpcM_ERROR_PROCESS_ID_e) dComIfGp_event_setItemPartnerId(item);
        dComIfGp_evmng_cutEnd(staff);
        mActionState++;
    } else if (mActionState != -1) {
        fopMsgM_demoMsgFlagOn();
        dComIfGp_evmng_cutEnd(staff);
        if (dComIfGp_evmng_endCheck("BMS_GET_DEMO")) {
            mOrder = 1;
            dComIfGp_event_reset();
            mOverrideMsgNo = 0x2790;
            setAction(&daNpc_Bms1_c::wait_action, NULL);
        }
    }
    return TRUE;
}

int daNpc_Bms1_c::wait_action(void*) {
    if (mActionState == 0) {
        mState = 1;
        mActionState++;
    } else if (mActionState != -1) {
        s16 angle = current.angle.y + mJnt.getHead_y() + mJnt.getBackbone_y();
        mHasAttention = chkAttention(current.pos, angle);
        if (mType == 1 && mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f) && cLib_calcTimer(&mAnmTimer) == 0) {
            mAnmTimer = 3;
            mDoAud_seStart(JA_SE_CV_BO_STRUGGLE, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        }
        switch (mState) {
        case 1:
            wait01();
            break;
        case 2:
            talk01();
            break;
        }
        lookBack();
        if (checkPlayerLanding() && mType != 1) mOrder = 4;
        setAttention(true);
    }
    return TRUE;
}

int daNpc_Bms1_c::event_action(void*) {
    if (mActionState == 0) {
        if (mType != 1) {
            if (dComIfGs_isEventBit(0xA02)) setAnm(1, -1.0f);
            else setAnm(0, -1.0f);
        }
        mActionState++;
    } else if (mActionState != -1) {
        privateCut();
        if (dComIfGp_evmng_endCheck("BMS_LAND_DEMO")) {
            mOrder = 0;
            dComIfGp_event_reset();
            setAction(&daNpc_Bms1_c::wait_action, NULL);
        }
    }
    return TRUE;
}

BOOL daNpc_Bms1_c::evn_talk_init(int staff) {
    int* msg = dComIfGp_evmng_getMyIntegerP(staff, "MsgNo");
    int* end = dComIfGp_evmng_getMyIntegerP(staff, "EndMsgNo");
    l_msgId = fpcM_ERROR_PROCESS_ID_e;
    l_msg = NULL;
    if (msg) {
        mMsgNo = *msg;
        if (mMsgNo == 0x2791 && dComIfGs_isEventBit(0xA02)) mMsgNo = 0x2792;
    } else mMsgNo = 0;
    if (end) mEndMsgNo = * end;
    else mEndMsgNo = 0;
    return TRUE;
}

BOOL daNpc_Bms1_c::evn_continue_talk_init(int staff) {
    int* end = dComIfGp_evmng_getMyIntegerP(staff, "EndMsgNo");
    if (end) mEndMsgNo = * end;
    else mEndMsgNo = 0;
    return TRUE;
}

BOOL daNpc_Bms1_c::evn_talk() {
    if (l_msgId == fpcM_ERROR_PROCESS_ID_e) l_msgId = fopMsgM_messageSet(mMsgNo, &eyePos);
    else if (!l_msg) {
        l_msg = fopMsgM_SearchByID(l_msgId);
        fopMsgM_demoMsgFlagOn();
    } else {
        setAnmFromMsgTag();
        if (l_msg->mStatus == 14) {
            l_msg->mStatus = next_msgStatus(&mMsgNo);
            if (l_msg->mStatus == 15) fopMsgM_messageSet(mMsgNo);
        } else if (l_msg->mStatus == 18) {
            if (mMsgNo == 0x1DCA) {
                int slot;
                s16 count;
                if (mEventItem == 0x49) {
                    slot = 4;
                    count = -10;
                } else if (mEventItem == 0x4A) {
                    slot = 5;
                    count = -20;
                } else {
                    slot = 6;
                    count = -30;
                }
                dComIfGp_setItemBeastNumCount(slot, count);
            } else if (mMsgNo == 0x1DC4) {
                int slot;
                if (mEventItem == 0x49) slot = 4;
                else if (mEventItem == 0x4A) slot = 5;
                else slot = 6;
                dComIfGp_setItemBeastNumCount(slot, -10);
            }
            l_msg->mStatus = 19;
            l_msg = NULL;
            l_msgId = fpcM_ERROR_PROCESS_ID_e;
            return TRUE;
        } else if ((l_msg->mStatus == 2 || l_msg->mStatus == 6) && mMsgNo == mEndMsgNo) {
            mEndMsgNo = 0;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL daNpc_Bms1_c::evn_viblation_init(int) {
    dComIfGp_getVibration().StartShock(5, -17, cXyz(0.0f, 1.0f, 0.0f));
    return TRUE;
}

BOOL daNpc_Bms1_c::evn_head_swing_init(int) {
    mHeadAnm.swing_vertical_init(1, 0x1800, 0x1000, 1);
    return TRUE;
}

BOOL daNpc_Bms1_c::privateCut() {
    const char* name = mEventCut.getActorName();
    int staff = dComIfGp_evmng_getMyStaffId(name, NULL, 0);
    if (staff == -1) return FALSE;
    dEvent_manager_c& evm = g_dComIfG_gameInfo.play.getEvtManager();
    static const char* cut_name_tbl[] = {
        "TALKMSG", "CONTINUE_TALK", "VIBLATE", "HEADSWING"
    };
    int cut = evm.getMyActIdx(staff, cut_name_tbl, 4, TRUE, FALSE);
    if (cut == -1) evm.cutEnd(staff);
    else {
        if (evm.getIsAddvance(staff)) {
            switch (cut) {
            case 0:
                evn_talk_init(staff);
                break;
            case 1:
                evn_continue_talk_init(staff);
                break;
            case 2:
                evn_viblation_init(staff);
                break;
            case 3:
                evn_head_swing_init(staff);
                break;
            }
        }
        BOOL done;
        switch (cut) {
        case 0:
        case 1:
            done = evn_talk();
            break;
        default:
            done = TRUE;
            break;
        }
        if (done) evm.cutEnd(staff);
    }
    return TRUE;
}

BOOL daNpc_Bms1_c::demo_move() {
    dDemo_actor_c* actor = dComIfGp_demo_getActor(demoActorID);
    if (!actor) {
        if (mDemo == 1) demo_end_init();
        return FALSE;
    }
    mDemo = 1;
    J3DAnmTexPattern* btp = actor->getP_BtpData("Bms");
    if (btp) mBtpAnm.init(mpHeadModel->getModelData(), btp, TRUE, 2, 1.0f, 0, -1, true, 0);
    dDemo_setDemoData(this, 0x6A, mpMorf, "Bms");
    return TRUE;
}

void daNpc_Bms1_c::demo_end_init() {
    mDemo = 0;
}

BOOL daNpc_Bms1_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* head = mpHeadModel->getModelData();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtpAnm.entry(head, mBtpFrame);
    g_env_light.setLightTevColorType(mpHeadModel, &tevStr);
    mpMorf->entryDL();
    mDoExt_modelUpdateDL(mpHeadModel);
    if (mType == 1) {
        g_env_light.setLightTevColorType(mpModel354, &tevStr);
        mDoExt_modelUpdateDL(mpModel354);
        g_env_light.setLightTevColorType(mpModel358, &tevStr);
        mDoExt_modelUpdateDL(mpModel358);
    } else if (dComIfGs_isEventBit(0xA02)) {
        g_env_light.setLightTevColorType(mpModel354, &tevStr);
        mDoExt_modelUpdateDL(mpModel354);
    }
    if (mpLegModel) {
        g_env_light.setLightTevColorType(mpLegModel, &tevStr);
        mDoExt_modelUpdateDL(mpLegModel);
    }
    if (mpModel360) {
        g_env_light.setLightTevColorType(mpModel360, &tevStr);
        mDoExt_modelUpdateDL(mpModel360);
    }
    mBtpAnm.remove(head);
    cXyz shadow(current.pos.x, 130.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(mShadowId, 1, mpMorf->getModel(), &shadow, 800.0f, 20.0f, current.pos.y, mAcch.GetGroundH(), mAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShadowId) dComIfGd_addRealShadow(mShadowId, mpHeadModel);
    if (mShopItems.mSelectedItemIdx >= 0) mpShopCursor->draw();
    cXyz pos = current.pos;
    if (mType == 1) pos.y -= 90.0f;
#if VERSION == VERSION_DEMO
    dSnap_RegistFig(0x5C, this, pos.y, current.angle.y, 1.0f);
#else
    dSnap_RegistFig(0x5C, this, pos, current.angle.y, 1.0f, 1.0f, 1.0f);
#endif
    return TRUE;
}

BOOL daNpc_Bms1_c::_execute() {
    dNpc_HIO_c & hio = l_HIO.mChild[0].mNpc;
    mJnt.setParam(hio.mMaxBackboneX, hio.mMaxBackboneY, hio.mMinBackboneX, hio.mMinBackboneY, hio.mMaxHeadX, hio.mMaxHeadY, hio.mMinHeadX, hio.mMinHeadY, hio.mMaxTurnStep);
    playTexPatternAnm();
    mAnmEnd = mpMorf->play(&eyePos, 0, 0);
    mpMorf->calc();
    if (mpMorf->getFrame() < mPreviousFrame) mAnmEnd = 1;
    mPreviousFrame = mpMorf->getFrame();
    if (!demo_move()) {
        checkOrder();
        (this->*mAction)(NULL);
        mShopCam.move();
        mShopItems.Item_Move();
        eventOrder();
    }
    mHeadAnm.move();
    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    mAcch.CrrPos(*dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mAcch.m_gnd);
    set_mtx();
    setCollision();
    return TRUE;
}

BOOL daNpc_Bms1_c::_delete() {
    if (mSkipped == 1) return TRUE;
    dComIfG_resDeleteDemo(&mPhs, m_arcname);
#if VERSION > VERSION_DEMO
    if (heap)
#endif
    if (mpMorf) mpMorf->stopZelAnime();
    if (l_HIO.m08 >= 0 &&-- l_HIO.m08 < 0) mDoHIO_deleteChild(l_HIO.m04);
    return TRUE;
}

static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return((daNpc_Bms1_c*) actor)->CreateHeap();
}

cPhs_State daNpc_Bms1_c::_create() {
    fopAcM_ct_Retail(this, daNpc_Bms1_c);
    mType = fopAcM_GetParam(this) >> 24;
    if (mType == 0) {
        if (dComIfGs_isEventBit(0xA02) && !checkItemGet(0x69, 1)) {
            mSkipped = 1;
            return cPhs_ERROR_e;
        }
        mSkipped = 0;
    } else {
        if (dComIfGs_isEventBit(0xA02) && !checkItemGet(0x69, 1)) mSkipped = 0;
        else {
            mSkipped = 1;
            return cPhs_ERROR_e;
        }
    }
    cPhs_State phase = dComIfG_resLoad(&mPhs, m_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        fopAcM_ct_Demo(this, daNpc_Bms1_c);
        switch (mType) {
        case 0:
            mType = 0;
            break;
        default:
            mType = 1;
            break;
        }
        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, 0x18000)) return cPhs_ERROR_e;
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        if (l_HIO.m08 < 0) l_HIO.m04 = mDoHIO_createChild("ボム屋店員", &l_HIO);
        l_HIO.m08++;
        if (!CreateInit()) return cPhs_ERROR_e;
    }
    return phase;
}

BOOL daNpc_Bms1_c::CreateHeap() {
    J3DModelData* body;
    J3DAnmTransform* bck;
    if (mType == 0) {
        body = (J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY1_e);
        if (!checkItemGet(0x69, 1)) bck = (J3DAnmTransform*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BCK_BY1_WAIT01_e);
        else bck = (J3DAnmTransform*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BCK_BY1_WAIT02_e);
    } else {
        body = (J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY2_e);
        bck = (J3DAnmTransform*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BCK_BY2_WAIT00_e);
        mAnmTimer = 3;
    }
    mpMorf = new mDoExt_McaMorf(body, NULL, NULL, bck, 2, 1.0f, 0, -1, 1, NULL, 0, 0x11020203);
    if (!mpMorf || !mpMorf->getModel()) {
#if VERSION > VERSION_DEMO
        mpMorf = NULL;
#endif
        return FALSE;
    }
    m_head_jnt_num = body->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x820, 0x823), m_head_jnt_num >= 0);
    m_backbone_jnt_num = body->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x823, 0x826), m_backbone_jnt_num >= 0);
    if (mType == 0) {
        m_leg_jnt_num = body->getJointName()->getIndex("center");
        JUT_ASSERT(DEMO_SELECT(0x828, 0x82B), m_leg_jnt_num >= 0);
    }
    J3DModelData* head = (J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY_HEAD_e);
    mpHeadModel = mDoExt_J3DModel__create(head, 0x80000, 0x11020002);
    if (!mpHeadModel) return FALSE;
    m_hairL_jnt_num = head->getJointName()->getIndex("hairL");
    JUT_ASSERT(DEMO_SELECT(0x83B, 0x83E), m_hairL_jnt_num >= 0);
    m_hairR_jnt_num = head->getJointName()->getIndex("hairR");
    JUT_ASSERT(DEMO_SELECT(0x83E, 0x841), m_hairR_jnt_num >= 0);
    mBtpNo = 1;
#if VERSION == VERSION_DEMO
    initTexPatternAnm(false);
#else
    if (!initTexPatternAnm(false)) return FALSE;
#endif
    switch (mType) {
    case 0:
        if (dComIfGs_isEventBit(0xA02)) setTexAnm(1);
        else setTexAnm(0);
        break;
    case 1:
        setTexAnm(1);
        break;
    }
    mpModel354 = mDoExt_J3DModel__create((J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY_EYE_e), 0, 0x11020203);
    if (!mpModel354) return FALSE;
    mpModel358 = mDoExt_J3DModel__create((J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY_MASK_e), 0, 0x11020203);
    if (mType == 0) {
        J3DModelData* leg;
        if (dComIfGs_isEventBit(0xA02)) leg = (J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY_LOW2_e);
        else leg = (J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_BY_LOW1_e);
        mpLegModel = mDoExt_J3DModel__create(leg, 0, 0x11020203);
        if (!mpLegModel) return FALSE;
        mpModel360 = mDoExt_J3DModel__create((J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BDL_PTARU_e), 0, 0x11020203);
        if (!mpModel360) return FALSE;
    } else mpLegModel = NULL;
    for (u16 i = 0; i < body->getJointNum(); i++) if (i == m_head_jnt_num || i == m_backbone_jnt_num) mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Bms);
    mpMorf->getModel()->setUserArea((u32) this);
    for (u16 i = 0; i < mpHeadModel->getModelData()->getJointNum(); i++) if (i == m_hairL_jnt_num || i == m_hairR_jnt_num) mpHeadModel->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_BmsHead);
    mpHeadModel->setUserArea((u32) this);
    mAcchCir.SetWall(30.0f, 0.0f);
    mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BRK_SHOP_CURSOR01_e);
    mpShopCursor = ShopCursor_create((J3DModelData*) dComIfG_getObjectRes(m_arcname, dRes_INDEX_BMS_BMD_SHOP_CURSOR01_e), brk, l_HIO.mChild[0].mCursorScale);
    return mpShopCursor ? TRUE : FALSE;
}

/* 000043B8-000043D8       .text daNpc_Bms1_Create__FP10fopAc_ac_c*/
static cPhs_State daNpc_Bms1_Create(fopAc_ac_c* i_this) {
    return((daNpc_Bms1_c*) i_this)->_create();
}

/* 000043D8-000043F8       .text daNpc_Bms1_Delete__FP12daNpc_Bms1_c*/
static BOOL daNpc_Bms1_Delete(daNpc_Bms1_c* i_this) {
    return((daNpc_Bms1_c*) i_this)->_delete();
}

/* 000043F8-00004418       .text daNpc_Bms1_Execute__FP12daNpc_Bms1_c*/
static BOOL daNpc_Bms1_Execute(daNpc_Bms1_c* i_this) {
    return((daNpc_Bms1_c*) i_this)->_execute();
}

/* 00004418-00004438       .text daNpc_Bms1_Draw__FP12daNpc_Bms1_c*/
static BOOL daNpc_Bms1_Draw(daNpc_Bms1_c* i_this) {
    return((daNpc_Bms1_c*) i_this)->_draw();
}

/* 00004438-00004440       .text daNpc_Bms1_IsDelete__FP12daNpc_Bms1_c*/
static BOOL daNpc_Bms1_IsDelete(daNpc_Bms1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Bms1_Method = {
    (process_method_func) daNpc_Bms1_Create, (process_method_func) daNpc_Bms1_Delete, (process_method_func) daNpc_Bms1_Execute, (process_method_func) daNpc_Bms1_IsDelete, (process_method_func) daNpc_Bms1_Draw,
};

actor_process_profile_definition g_profile_NPC_BMS1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BMS1_e,
    /* Proc SubMtd  */ & g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bms1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ & g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BMS1_e,
/* Actor SubMtd*/
    & l_daNpc_Bms1_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_12_e,
};
