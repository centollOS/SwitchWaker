/**
* d_a_npc_uk.cpp
* NPC - Jin, Jan, & Jun-Roberto
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_uk.h"
#include "m_Do/m_Do_ext.h"
#include "res/Object/Uk.h"
#include "d/actor/d_a_npc_mk.h"
#include "d/actor/d_a_player_main.h"
#include "m_Do/m_Do_lib.h"
#include "d/d_snap.h"

static fpc_ProcID l_msgId;

static msg_class* l_msg;

/* 000000EC-000000F8       .text getType__10daNpc_Uk_cFv */
int daNpc_Uk_c::getType() {
    return fopAcM_GetParam(this) & 0xFF;
}

/* 000000F8-00000104       .text getPath__10daNpc_Uk_cFv */
int daNpc_Uk_c::getPath() {
    return fopAcM_GetParam(this) >> 8 & 0xFF;
}

/* 00000104-00000110       .text getShapeType__10daNpc_Uk_cFv */
int daNpc_Uk_c::getShapeType() {
    return fopAcM_GetParam(this) >> 16 & 0xF;
}

/* 00000110-0000015C       .text getCaughtFlag__10daNpc_Uk_cFv */
u16 daNpc_Uk_c::getCaughtFlag() {
    switch (getShapeType()) {
    case 1:
        return 0x8;
    case 2:
        return 0x4;
    default:
        return 0x10;
    }
}

/* 0000015C-000001A8       .text getFoundFlag__10daNpc_Uk_cFv */
u16 daNpc_Uk_c::getFoundFlag() {
    switch (getShapeType()) {
    case 1:
        return 0x180;
    case 2:
        return 0x140;
    default:
        return 0x1;
    }
}

/* 000001A8-000001F4       .text getFirstTalk__10daNpc_Uk_cFv */
u16 daNpc_Uk_c::getFirstTalk() {
    switch (getShapeType()) {
    case 0:
        return 0x1202;
    case 1:
        return 0x1204;
    default:
        return 0x1201;
    }
}

/* 000001F4-0000026C       .text chkGameStart__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::chkGameStart() {
    if (dComIfGs_isTmpBit(0x40) && !dComIfGs_isTmpBit(getCaughtFlag())) {
        return TRUE;
    }

    return FALSE;
}

/* 0000026C-00000420       .text chkPositioning__10daNpc_Uk_cFfffss */
BOOL daNpc_Uk_c::chkPositioning(f32 distance, f32 minY, f32 maxY, s16 minAngle, s16 maxAngle) {
    daPy_lk_c* player = daPy_getPlayerLinkActorClass();

    cXyz delta = player->current.pos - current.pos;

    if (delta.absXZ() > distance) {
        return FALSE;
    }

    if (delta.y < minY || delta.y > maxY) {
        return FALSE;
    }

    s16 angle = cLib_targetAngleY(&current.pos, &player->current.pos) - current.angle.y;

    if (minAngle > maxAngle) {
        if (angle < minAngle && angle > maxAngle) {
            return TRUE;
        }
    } else {
        if (angle < minAngle || angle > maxAngle) {
            return TRUE;
        }
    }

    return FALSE;
}

/* 0000045C-000008B0       .text nextVisitMode__10daNpc_Uk_cFv */
u8 daNpc_Uk_c::nextVisitMode() {
    cXyz delta;
    fopAc_ac_c* leader = fopAcM_SearchByID(mLeaderID);
    daPy_lk_c* player = daPy_getPlayerLinkActorClass();
    u8 mode;

    if (leader == NULL) {
        mVisitMode = 0;
    } else {
        mode = static_cast<daNpc_Mk_c*>(leader)->mVisitMode;
    }

    if (mVisitMode == 10 && mWaitTimer != 0) {
        mWaitTimer--;
        return 10;
    }

    if (mVisitMode == 0) {
        leader = fopAcM_searchFromName("Mk", 0xFF, 1);

        if (leader != NULL) {
            mLeaderID = fopAcM_GetID(leader);
            mode = static_cast<daNpc_Mk_c*>(leader)->mVisitMode;
        } else {
            return 0;
        }
    }

    if (mode == 2 || mode == 9 || mode == 8) {
        delta = leader->current.pos - current.pos;

        switch (mVisitMode) {
        case 4:
            if (delta.abs2XZ() > 10000.0f) {
                return 3;
            }
            return 4;
        case 3:
            if (delta.abs2XZ() > 40000.0f) {
                return 2;
            }

            if (delta.abs2XZ() < 6400.0f) {
                return 4;
            }
            return 3;
        default:
            if (delta.abs2XZ() < 22500.0f) {
                return 3;
            }
            return 2;
        }
    }

    if (mVisitMode == 9) {
        if (mWaitTimer != 0) {
            mWaitTimer--;
            return 9;
        }

        delta = player->current.pos - current.pos;

        if (delta.abs2XZ() < 22500.0f) {
            return 7;
        }

        if (mode == 4 || mode == 5) {
            return 5;
        }

        s16 angle = fopAcM_searchActorAngleY(this, leader);
        angle -= shape_angle.y;

        if (angle < 0) {
            angle = -angle;
        }

        if (angle < 0x3800) {
            return 2;
        }

        return 9;
    }

    if (mode == 6) {
        switch (mVisitMode) {
        case 8:
            if (mWaitTimer != 0) {
                mWaitTimer--;
                return 8;
            }
            return 9;
        default:
            return 8;
        }
    }

    if (mode == 7) {
        return 10;
    }

    delta = player->current.pos - current.pos;

    switch (mVisitMode) {
    case 6:
    case 7:
        if (delta.abs2XZ() > 32400.0f) {
            return 5;
        }

        if (mVisitMode == 7) {
            if (mAroundTimer != 0) {
                mAroundTimer--;
            } else {
                return 6;
            }
        }

        return mVisitMode;
    case 5:
        if (delta.abs2XZ() < 22500.0f) {
            return 7;
        }
        return 5;
    default:
        if (delta.abs2XZ() < 22500.0f) {
            return 7;
        }
        return 2;
    }
}

/* 000008B0-00000930       .text approachRun__10daNpc_Uk_cFP10fopAc_ac_c */
void daNpc_Uk_c::approachRun(fopAc_ac_c* actor) {
    s16 angle;

    dNpc_calc_DisXZ_AngY(current.pos, actor->current.pos, NULL, &angle);

    cLib_addCalcAngleS2(&current.angle.y, angle, 8, 0x800);
}

/* 00000930-00000964       .text aroundWalk__10daNpc_Uk_cFP10fopAc_ac_c */
void daNpc_Uk_c::aroundWalk(fopAc_ac_c* actor) {
    mMkStatic.aroundWalk(this, actor, mAroundTimer);
}

/* 00000964-00000AD4       .text surrender__10daNpc_Uk_cFv */
void daNpc_Uk_c::surrender() {
    daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

    if (leader == NULL) {
        return;
    }

    if (leader->speedF <= 1.0f) {
        return;
    }

    cXyz delta = leader->current.pos - current.pos;

    if (delta.abs2XZ() > 7225.0f) {
        return;
    }

    s16 angle = fopAcM_searchActorAngleY(leader, this);
    angle -= leader->shape_angle.y;

    if (angle > 0x2AAA || angle < -0x2AAA) {
        return;
    }

    if (angle >= 0) {
        angle = leader->shape_angle.y + 0x4000;
    } else {
        angle = leader->shape_angle.y - 0x4000;
    }
    speed.x = speed.x + 0.5f * cM_ssin(angle);

    speed.z = speed.z + 0.5f * cM_scos(angle);

    setFlag(8);
}

/* 00000AD4-00000D88       .text runawayInit__10daNpc_Uk_cFv */
void daNpc_Uk_c::runawayInit() {
    switch (mMkStatic.m0) {
    case 1:
        case 2: case 5:
        if (mAnmIdx == 5) {
            switch (getShapeType()) {
            case 1:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_C_FOUND, 0);
                break;
            case 2:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_D_FOUND, 0);
                break;
            default:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_B_FOUND, 0);
                break;
            }
        }

        speedF = 0.0f;
        setAnm(4, 0);
        break;
    case 3:
        speedF = 0.0f;
        setAnm(5, 0);
        break;
    case 4:
        speed.y = 25.0f;
        gravity = -3.3f;
        speedF = 8.0f;

        setFlag(0x200);
        setAnm(8, 0);
        mState = 11;
        current.angle.y = mTargetAngle;

        switch (getShapeType()) {
        case 1:
            fopAcM_seStart(this, JA_SE_CV_TR_KO_C_CAUGHT, 0);
            break;
        case 2:
            fopAcM_seStart(this, JA_SE_CV_TR_KO_D_CAUGHT, 0);
            break;
        default:
            fopAcM_seStart(this, JA_SE_CV_TR_KO_B_CAUGHT, 0);
            break;
        }
        break;
    }
}

static const int l_bck_ix_tbl[] = {
    dRes_INDEX_UK_BCK_UK_WAIT_e, dRes_INDEX_UK_BCK_UK_TALK01_e,
    dRes_INDEX_UK_BCK_UK_TALK02_e, dRes_INDEX_UK_BCK_UK_WALK_e,
    dRes_INDEX_UK_BCK_UK_RUN_e, dRes_INDEX_UK_BCK_UK_KEIKAI_e,
    dRes_INDEX_UK_BCK_UK_JIDA01_e, dRes_INDEX_UK_BCK_UK_JIDA02_e,
    dRes_INDEX_UK_BCK_UK_DA_e, dRes_INDEX_UK_BCK_UK_JUMP_e,
    dRes_INDEX_UK_BCK_UK_LAND_e,
};

static const int l_btp_ix_tbl[] = {
    dRes_INDEX_UK_BTP_UK_MABA_C_e };

/* 00000D88-00000F48       .text nodeCallBack_Uk__FP7J3DNodei */
static BOOL nodeCallBack_Uk(J3DNode* node, int calcTiming) {
    cXyz temp;

    cXyz temp2;

    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();

        daNpc_Uk_c* i_this = (daNpc_Uk_c*)model->getUserArea();

        J3DJoint* joint = (J3DJoint*)node;

        int jntNo = joint->getJntNo();

        if(i_this != NULL) {
            MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);

            if (jntNo == i_this->getHeadJntNum()) {
                temp.setall(0.0f);

                cMtx_XrotM(*calc_mtx, (s16)i_this->getHead_y());

                cMtx_ZrotM(*calc_mtx, (s16)-i_this->getHead_x());

                MtxPosition(&temp, &temp2);

                i_this->setAttentionBasePos(temp2);

                temp.set(20.0f, -20.0f, 0.0f);

                MtxPosition(&temp, &temp2);

                i_this->setEyePos(temp2);

                i_this->incAttnSetCount();
            } else if (jntNo == i_this->getBackboneJntNum()) {
                cMtx_XrotM(*calc_mtx, (s16)i_this->getBackbone_y());

                cMtx_ZrotM(*calc_mtx, (s16)i_this->getBackbone_x());
            }

            cMtx_copy(*calc_mtx, J3DSys::mCurrentMtx);

            model->setAnmMtx(jntNo, *calc_mtx);
        }
    }

    return TRUE;
}

/* 00000F48-00001070       .text initTexPatternAnm__10daNpc_Uk_cFb */
BOOL daNpc_Uk_c::initTexPatternAnm(bool i_modify) {
    J3DModelData* modelData = mpHeadModel->getModelData();

    if (getShapeType() != 1) {
#if VERSION == VERSION_DEMO
        return FALSE;
#else
        return TRUE;
#endif
    }

    m_maba_tex_pattern = (J3DAnmTexPattern*)dComIfG_getObjectRes("Uk", l_btp_ix_tbl[mTexPatternIdx]);

    JUT_ASSERT(679, m_maba_tex_pattern != 0);

    if (!mBtpAnm.init(modelData, m_maba_tex_pattern, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, i_modify, FALSE)) {
        return FALSE;
    }

    mBlinkFrame = 0;

    mBlinkTimer = 0;

    return TRUE;
}

/* 00001070-00001108       .text playTexPatternAnm__10daNpc_Uk_cFv */
void daNpc_Uk_c::playTexPatternAnm() {
    if (getShapeType() == 1 && cLib_calcTimer(&mBlinkTimer) == 0) {
        if (mBlinkFrame >= m_maba_tex_pattern->getFrameMax()) {
            mBlinkFrame -= m_maba_tex_pattern->getFrameMax();

            mBlinkTimer = cM_rndF(100.0f) + 30.0f;
        } else {
            mBlinkFrame++;
        }
    }
}

/* 00001108-000011C0       .text setAnm__10daNpc_Uk_cFScUc */
void daNpc_Uk_c::setAnm(s8 index, u8 flags) {
    f32 morf = 8.0f;

    if (index == 4 && mAnmIdx == 10) {
        morf = 0.0f;
    }

    if (flags & 1) {
        morf = 0.0f;
        mAnmIdx = 11;
    }

    if (index != mAnmIdx && index != -1) {
        mAnmIdx = index;
        mAnmTimer = 0.0f;

        dNpc_setAnm(mpMorf, -1, morf, 1.0f, l_bck_ix_tbl[mAnmIdx], -1, "Uk");
    }
}

/* 000011C0-00001274       .text chkAttentionLocal__10daNpc_Uk_cFv */
bool daNpc_Uk_c::chkAttentionLocal() {
    dAttention_c& attention = dComIfGp_getAttention();

    if (chkFlag(0x1)) {
        return true;
    }

    if (mAttentionTimer != 0) {
        mAttentionTimer--;

        return true;
    }

    if(attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }

    return this == attention.ActionTarget(0);
}

/* 00001274-00001310       .text chkAttention__10daNpc_Uk_cFv */
void daNpc_Uk_c::chkAttention() {
    bool temp = chkFlag(0x4);

    if(chkAttentionLocal()) {
        setFlag(0x4);
    } else {
        clrFlag(0x4);
    }

    if(temp != chkFlag(0x4) && temp == true) {
        m_jnt.setTrn();
    }
}

/* 00001310-00001378       .text setAnmStatus__10daNpc_Uk_cFv */
void daNpc_Uk_c::setAnmStatus() {
    switch (mState) {
    case 6:
        case 7: case 8: setAnm(5, 0);
        break;
    case 12:
        case 13: break;
    default:
        setAnm(0, 0);
        break;
    }
}

/* 00001378-000013BC       .text eventOrder__10daNpc_Uk_cFv */
void daNpc_Uk_c::eventOrder() {
    if (ChkOrder(3)) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);

        if(ChkOrder(2)){
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 000013BC-00001420       .text checkOrder__10daNpc_Uk_cFv */
void daNpc_Uk_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
    } else if (eventInfo.checkCommandTalk() && ChkOrder(3)) {
        setFlag(1);
        talkInit();
    }

    ClrOrder(0xFF);
}

/* 00001420-00001464       .text next_msgStatus__10daNpc_Uk_cFPUl */
u16 daNpc_Uk_c::next_msgStatus(u32* msg) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;

    switch (*msg) {
    case 0x26BF:
        case 0x26C1: case 0x26C3: case 0x26D8: *msg += 1;
        break;
    default:
        status = fopMsgStts_MSG_ENDS_e;
        break;
    }

    return status;
}

/* 00001464-000015E0       .text getMsg__10daNpc_Uk_cFv */
u32 daNpc_Uk_c::getMsg() {
    u32 msg = 0;

    switch (mType) {
    case TYPE_NORMAL:
        if (mState == 3 || mState == 4) {
            msg = mMsgNo;
        } else if (dComIfGs_isTmpBit(0x40)) {
            msg = getShapeType() + 0x26B6;
        } else if (dComIfGs_isEventBit(0x1340)) {
            msg = getShapeType() + 0x26AE;
        } else if (dComIfGs_isEventBit(getFirstTalk())) {
            msg = getShapeType() + 0x26BA;
        } else {
            msg = getShapeType() * 2 + 0x26BF;
            dComIfGs_onEventBit(getFirstTalk());
        }
        break;
    case TYPE_MINIGAME:
        if (dComIfGs_isTmpBit(getCaughtFlag())) {
            msg = getShapeType() + 0x26B6;
        } else {
            msg = getShapeType() + 0x26B2;
            dComIfGs_onTmpBit(getCaughtFlag());
        }
        break;
    }

    return msg;
}

/* 000015E0-00001660       .text setCollision__10daNpc_Uk_cFv */
void daNpc_Uk_c::setCollision() {
    cXyz center = current.pos;

    f32 r = 40.0f;
    f32 h = 80.0f;
    mCyl.SetC(center);
    mCyl.SetR(r);
    mCyl.SetH(h);

    dComIfG_Ccsp()->Set(&mCyl);
}

/* 00001660-00001730       .text msgAnm__10daNpc_Uk_cFUc */
void daNpc_Uk_c::msgAnm(u8 index) {
    if (mMsgAnmIdx != index) {
        mMsgAnmIdx = index;

        switch (mMsgAnmIdx) {
        case 0:
            setAnm(0, 0);
            break;
        case 1:
            setAnm(1, 0);
            break;
        case 2:
            setAnm(2, 0);
            break;
        case 3:
            setAnm(3, 0);
            break;
        case 4:
            setAnm(4, 0);
            break;
        case 5:
            setAnm(6, 0);
            break;
        case 6:
            setAnm(5, 0);
            break;
        case 7:
            setAnm(8, 0);
            break;
        }
    }
}

/* 00001730-00001744       .text talkInit__10daNpc_Uk_cFv */
void daNpc_Uk_c::talkInit() {
    mTalkState = TALK_INIT;

    mMsgAnmIdx = 0xFF;
}

/* 00001744-000018F0       .text talk__10daNpc_Uk_cFv */
u16 daNpc_Uk_c::talk() {
    u16 status = 0xFF;

    if (mTalkState == TALK_INIT) {
        l_msgId = fpcM_ERROR_PROCESS_ID_e;

        mCurrMsgNo = getMsg();
        mTalkState = TALK_MSG_CREATE;
    } else if (mTalkState != TALK_FINISHED) {
        if (l_msgId == fpcM_ERROR_PROCESS_ID_e) {
            l_msgId = fopMsgM_messageSet(mCurrMsgNo, this);
        } else {
            if (!chkFlag(0x400)) {
                msgAnm(dComIfGp_getMesgAnimeAttrInfo());
            }

            switch (mTalkState) {
            case TALK_MSG_CREATE:
                l_msg = (msg_class*)fopMsgM_SearchByID(l_msgId);

                if (l_msg != NULL) {
                    mTalkState = TALK_ACTIVE;

                    if (chkFlag(0x80)) {
                        dComIfGp_event_setTalkPartner(this);
                        clrFlag(0x80);
                    }
                }
                break;
            case TALK_ACTIVE:
                status = l_msg->mStatus;

                if (status == fopMsgStts_MSG_DISPLAYED_e) {
                    l_msg->mStatus = next_msgStatus(&mCurrMsgNo);

                    if (l_msg->mStatus == fopMsgStts_MSG_CONTINUES_e) {
                        fopMsgM_messageSet(mCurrMsgNo);
                    }
                } else if (status == fopMsgStts_BOX_CLOSED_e) {
                    l_msg->mStatus = fopMsgStts_MSG_DESTROYED_e;
                    mTalkState = TALK_FINISHED;
                }
                break;
            }
        }
    }

    return status;
}

/* 000018F0-00001DE4       .text init__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::init() {
    cXyz unused(0.0f, 0.0f, 0.0f);

    attention_info.flags = fopAc_Attn_ACTION_SPEAK_e | fopAc_Attn_LOCKON_TALK_e;

    attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA9;

    attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xA9;

    gravity = -30.0f;

    J3DModel* model = mpMorf->getModel();

    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);

    model->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();

    mStts.Init(100, 0xFF, this);
    mCyl.Set(dNpc_cyl_src);
    mCyl.SetStts(&mStts);

    fopAcM_SetStatusMap(this, 0x27);
    fopAcM_OnStatus(this, fopAcStts_SHOWMAP_e);

    switch (mType) {
    case TYPE_NONE:
        setAction(&daNpc_Uk_c::wait_action, NULL);
        break;
    case TYPE_NORMAL:
        if (chkGameStart()) {
            setAction(&daNpc_Uk_c::hind_action, NULL);
            setFlag(0x10);
            fopAcM_OffStatus(this, fopAcStts_SHOWMAP_e);
        } else {
            setAction(&daNpc_Uk_c::visit_action, NULL);
        }
        break;
    case TYPE_MINIGAME:
        mStts.SetWeight(0xFE);
        mPath.setInf(getPath(), fopAcM_GetRoomNo(this), true);

        if (mPath.getPath() == NULL) {
            return FALSE;
        }

        if (chkGameStart()) {
            setAction(&daNpc_Uk_c::seek_action, NULL);
        } else {
            setFlag(0x10);
            fopAcM_OffStatus(this, fopAcStts_SHOWMAP_e);
            setAction(&daNpc_Uk_c::hind_action, NULL);
        }

        fopAcM_OnStatus(this, fopAcStts_UNK40_e);
        break;
    }

    mAttnBasePos = current.pos;
    mEyePos = current.pos;

    if (mDoLib_clipper::getFar() > 1.0f) {
        fopAcM_setCullSizeFar(this, 5000.0f / mDoLib_clipper::getFar());
    }

    mMtrlSndId = 0;
    mReverb = dComIfGp_getReverb(fopAcM_GetRoomNo(this));

    return TRUE;
}

/* 00001DE4-00001E38       .text setAttention__10daNpc_Uk_cFb */
void daNpc_Uk_c::setAttention(bool force) {
    if (force || mAttnSetCount < 2) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);

        attention_info.position.set(mAttnBasePos.x, mAttnBasePos.y + 45.0f, mAttnBasePos.z);
    }
}

/* 00001E38-00001F5C       .text getLookBackMode__10daNpc_Uk_cFv */
u8 daNpc_Uk_c::getLookBackMode() {
    daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

    if (leader != NULL && mState == 2) {
        if (mVisitMode == 8 || mVisitMode == 4) {
            return 3;
        }

        bool ignore = leader->mVisitMode == 8 || leader->mVisitMode == 9;

        if (ignore) {
            return 2;
        }
    }

    if (chkFlag(0x40)) {
        fopAc_ac_c* partner = fopAcM_getTalkEventPartner(daPy_getPlayerLinkActorClass());

        if (partner != this) {
            return 4;
        }

        return 2;
    }

    if (chkFlag(0x20)) {
        return 0;
    }

    if (chkFlag(4)) {
        return 1;
    }

    if (mState == 2 && (mVisitMode == 6 || mVisitMode == 7)) {
        return 1;
    }

    return 2;
}

/* 00001F5C-0000218C       .text lookBack__10daNpc_Uk_cFv */
void daNpc_Uk_c::lookBack() {
    daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

    cXyz target;

    cXyz source(0.0f, 0.0f, 0.0f);

    cXyz* targetP = NULL;

    s16 angle = current.angle.y;

    bool noTurn = false;

    u8 mode = getLookBackMode();

    source = current.pos;
    source.y = eyePos.y;

    switch (mode) {
    case 0:
        m_jnt.setTrn();
        target = dNpc_playerEyePos(-20.0f);
        targetP = &target;
        break;
    case 1:
        noTurn = true;
        target = dNpc_playerEyePos(-20.0f);
        targetP = &target;
        break;
    case 2:
        angle = current.angle.y;
        noTurn = true;
        break;
    case 3:
        m_jnt.setTrn();
        target = leader->current.pos;
        target.y = leader->eyePos.y;
        targetP = &target;
        break;
    case 4: {
            fopAc_ac_c* actor = fopAcM_getTalkEventPartner(daPy_getPlayerLinkActorClass());

            if (actor == NULL) {
                actor = daPy_getPlayerLinkActorClass();
            }

            m_jnt.setTrn();
            target = actor->eyePos;
            targetP = &target;

            source = current.pos;
            source.y = eyePos.y;
            break;
        }
    }

    if (m_jnt.trnChk()) {
        cLib_addCalcAngleS2(&mMaxHeadTurnVelocity, 1500, 4, 0x800);
    } else {
        mMaxHeadTurnVelocity = 0;
    }

    m_jnt.lookAtTarget(&current.angle.y, targetP, source, angle, mMaxHeadTurnVelocity, noTurn);
}

/* 0000218C-00002254       .text getStaffName__10daNpc_Uk_cFv */
char* daNpc_Uk_c::getStaffName() {
    if (mType == TYPE_MINIGAME) {
        switch (getShapeType()) {
        case 0:
            return "UkB2";
        case 1:
            return "UkC2";
        case 2:
            return "UkD2";
        }
    } else {
        switch (getShapeType()) {
        case 1:
            return "UkC";
        case 2:
            return "UkD";
        }
    }

    return "UkB";
}

/* 00002254-000022C4       .text checkDemoStart__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::checkDemoStart() {
    mStaffIdx = dComIfGp_evmng_getMyStaffId(getStaffName());

    if(mStaffIdx != -1) {
        mEventAction = getNowEventAction();

        return TRUE;
    }

    return FALSE;
}

/* 000022C4-0000230C       .text getNowEventAction__10daNpc_Uk_cFv */
s32 daNpc_Uk_c::getNowEventAction() {
    static char* action_table[] = {
        "WAIT", "TALK", "TALK2", "HOME", "RUN", "RUN3", "HIND", "DISP", "SPEAK", "LOOK_P", "WARNING", "JUMP", "JUMP2", "TURN", "WARP", "TURN" };

    return dComIfGp_evmng_getMyActIdx(mStaffIdx, action_table, 15, FALSE, 0);
}

/* 0000230C-00002494       .text visitInit__10daNpc_Uk_cFUc */
void daNpc_Uk_c::visitInit(u8 mode) {
    switch (mode) {
    case 2:
        setAnm(4, 0);
        break;
    case 5:
        setAnm(4, 0);

        if (mVisitMode == 9) {
            mApproachTimer = 45;
        } else {
            mApproachTimer = 0;
        }
        break;
    case 3:
        case 6: setAnm(3, 0);
        break;
    case 7:
        if (mVisitMode == 9 || mVisitMode == 8 || mVisitMode == 2) {
            mAroundTimer = 45;
        } else {
            mAroundTimer = mApproachTimer;
        }

        setAnm(4, 0);
        break;
    case 8:
        setAnm(0, 0);
        mWaitTimer = 5;
        speedF = 0.0f;
        break;
    case 9:
        setAnm(0, 0);
        mWaitTimer = 15;
        speedF = 0.0f;
        break;
    case 10:
        setAnm(0, 0);
        mWaitTimer = 30;
        speedF = 0.0f;
        break;
    case 4:
        setAnm(0, 0);
        speedF = 0.0f;
        break;
    default:
        speedF = 0.0f;
        break;
    }

    mVisitMode = mode;
}

/* 00002494-000025A4       .text visitProc__10daNpc_Uk_cFv */
void daNpc_Uk_c::visitProc() {
    daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

    daPy_lk_c* player = daPy_getPlayerLinkActorClass();

    switch (mVisitMode) {
    case 2:
        approachRun(leader);
        break;
    case 5:
        if (mApproachTimer < 45) {
            mApproachTimer++;
        }
        approachRun(player);
        break;
    case 3:
        approachRun(leader);
        break;
    case 6:
        case 7: aroundWalk(player);
        break;
    case 4:
        surrender();
        break;
    case 8:
        surrender();
        break;
    case 9:
        surrender();
        break;
    case 10:
        surrender();
        break;
    }

    switch (mVisitMode) {
    case 9:
        case 10: setFlag(0x20);
        break;
    }
}

/* 000025A4-00002628       .text wait01__10daNpc_Uk_cFv */
bool daNpc_Uk_c::wait01() {
    if (chkFlag(1)) {
        mPrevState = mState;
        mState = 1;
        setAnmStatus();
    } else {
        SetOrder(1);
    }

    return mpMorf->isMorf();
}

/* 00002628-000026CC       .text talk01__10daNpc_Uk_cFv */
bool daNpc_Uk_c::talk01() {
    if (talk() == fopMsgStts_BOX_CLOSED_e) {
        mState = mPrevState;
        setAnmStatus();
        dComIfGp_event_reset();
        clrFlag(1);
        mAttentionTimer = 5;
    }

    setFlag(0x20);
    return mpMorf->isMorf();
}

/* 000026CC-0000280C       .text talk02__10daNpc_Uk_cFv */
bool daNpc_Uk_c::talk02() {
    if (talk() == fopMsgStts_BOX_CLOSED_e) {
        if (!mMkStatic.chkGameSet()) {
            mState = mPrevState;
            setAnmStatus();
            dComIfGp_event_reset();
        } else {
            mEventIdx = dComIfGp_evmng_getEventIdx("MK_GAMESET");

            fopAcM_orderChangeEventId(this, mEventIdx, 0, 0xFFFF);

            mDoAud_seStart(JA_SE_END_WHISTLE);
            mState = 4;
            mPrevState = 5;
        }

        clrFlag(1);
        mAttentionTimer = 5;
    }

    setFlag(0x20);
    return mpMorf->isMorf();
}

/* 0000280C-00002968       .text visitTalkInit__10daNpc_Uk_cFv */
u8 daNpc_Uk_c::visitTalkInit() {
    if (dComIfGs_isEventBit(0x1F80) && !dComIfGs_isEventBit(0x1E02)) {
        mEventIdx = eventInfo.getEventId();

        daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

        dComIfGp_event_setTalkPartner(leader);
        mState = 4;
        demo02();
        dComIfGs_onEventBit(0x1E04);
        return 4;
    }

    if (dComIfGs_isEventBit(0x1208) && !dComIfGs_checkGetItem(0x23) && !dComIfGs_checkGetItem(0x26)) {
        mEventIdx = eventInfo.getEventId();

        daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

        dComIfGp_event_setTalkPartner(leader);
        mState = 4;
        demo02();
        dComIfGs_onEventBit(0x1602);
        return 4;
    }

    return 1;
}

/* 00002968-00002A44       .text visitSetEvent__10daNpc_Uk_cFv */
void daNpc_Uk_c::visitSetEvent() {
    if (dComIfGs_isEventBit(0x1F80) && !dComIfGs_isEventBit(0x1E02)) {
        eventInfo.setEventName("MK_TALK3");
    } else if (dComIfGs_isEventBit(0x1208) && !dComIfGs_checkGetItem(0x23) && !dComIfGs_checkGetItem(0x26)) {
        eventInfo.setEventName("MK_TALK2");
    } else {
        eventInfo.setEventName("MK_TALK");
    }
}

/* 00002A44-00002C80       .text visit01__10daNpc_Uk_cFv */
bool daNpc_Uk_c::visit01() {
    if (chkFlag(1)) {
        mPrevState = mState;
        mState = visitTalkInit();
        setAnmStatus();
        speedF = 0.0f;
        mVisitMode = 1;
    } else if (checkDemoStart()) {
        mPrevState = mState;
        mState = 3;
        setAnmStatus();
        speedF = 0.0f;
        mVisitMode = 1;
        demo01();
    } else {
        u8 mode = nextVisitMode();
        if (mode != mVisitMode) {
            visitInit(mode);
        }

        visitProc();

        f32 targetSpeed;

        if (mAnmIdx == 3) {
            targetSpeed = mVisitMode == 6 ? 2.5f : mWalkSpeed;
        } else if (mAnmIdx == 4) {
            targetSpeed = mRunSpeed;
            if (mVisitMode == 7) {
                targetSpeed -= 2.5f;
            }
        }

        if (speedF > targetSpeed) {
            cLib_chaseF(&speedF, targetSpeed, 2.8f);
        } else {
            cLib_chaseF(&speedF, targetSpeed, 1.1f);
        }

        SetOrder(1);

        if (mVisitMode == 6 || mVisitMode == 7) {
            daPy_lk_c* player = daPy_getPlayerLinkActorClass();

            s16 angle = fopAcM_searchActorAngleY(player, this);
            angle -= player->shape_angle.y;

            if (angle < 0) {
                angle = -angle;
            }

            if (angle > 0x1800) {
                ClrOrder(1);
            }
        }

        daNpc_Mk_c* leader = (daNpc_Mk_c*)fopAcM_SearchByID(mLeaderID);

        if (leader != NULL) {
            bool ignore = leader->mVisitMode == 8 || leader->mVisitMode == 9;

            if (ignore) {
                ClrOrder(1);
            }
        }

        if (ChkOrder(1)) {
            visitSetEvent();
        }
    }

    return true;
}

/* 00002C80-00002D20       .text warningB__10daNpc_Uk_cFv */
bool daNpc_Uk_c::warningB() {
    if (chkPositioning(300.0f, -10.0f, 80.0f, 15000, -15000)) {
        mState = 9;
        mEventIdx = dComIfGp_evmng_getEventIdx("UkB_FOUND");
        fopAcM_orderOtherEventId(this, mEventIdx);
    }

    return false;
}

/* 00002D20-00002DC0       .text warningC__10daNpc_Uk_cFv */
bool daNpc_Uk_c::warningC() {
    if (chkPositioning(180.0f, -10.0f, 80.0f, 15000, -5000)) {
        mState = 9;
        mEventIdx = dComIfGp_evmng_getEventIdx("UkC_FOUND");
        fopAcM_orderOtherEventId(this, mEventIdx);
    }

    return false;
}

/* 00002DC0-00002ED8       .text warningD__10daNpc_Uk_cFv */
bool daNpc_Uk_c::warningD() {
    if (chkPositioning(150.0f, -10.0f, 80.0f, 18000, 0)) {
        mState = 9;
        mEventIdx = dComIfGp_evmng_getEventIdx("UkD_FOUND");
        fopAcM_orderOtherEventId(this, mEventIdx);
    }

    if (chkPositioning(150.0f, -10.0f, 80.0f, 0, -18000)) {
        mState = 9;
        mEventIdx = dComIfGp_evmng_getEventIdx("UkD_FOUND2");
        fopAcM_orderOtherEventId(this, mEventIdx);
    }

    return false;
}

/* 00002ED8-00002F74       .text found__10daNpc_Uk_cFv */
bool daNpc_Uk_c::found() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        mPrevState = 10;
        mState = 4;
        mMkStatic.init(0x50, 300);
        dComIfGs_onTmpBit(getFoundFlag());
        return demo02();
    }

    fopAcM_orderOtherEventId(this, mEventIdx);
    return false;
}

/* 00002F74-00003148       .text runaway__10daNpc_Uk_cFv */
bool daNpc_Uk_c::runaway() {
    u8 mode = mMkStatic.runAwayProc(this, &mPath, &mCyl, &mTargetAngle);

    if (mode != mMkStatic.m0) {
        if (((mode == 1 && mMkStatic.m0 == 2) || (mode == 2 && mMkStatic.m0 == 1)) && cM_rndF(1.0f) < 0.5f) {
            switch (getShapeType()) {
            case 1:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_C_TURN, 0);
                break;
            case 2:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_D_TURN, 0);
                break;
            default:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_B_TURN, 0);
                break;
            }
        }

        mMkStatic.m0 = mode;
        runawayInit();
    }

    if (mode == 3) {
        setFlag(0x20);
    }

    if (mAnmIdx == 4) {
        cLib_chaseF(&speedF, mMkStatic.getSpeedF(15.0f, 18.0f), 2.8f);
    }

    return false;
}

/* 00003148-000031E8       .text jump__10daNpc_Uk_cFv */
bool daNpc_Uk_c::jump() {
    if (!chkFlag(0x200)) {
        setAnm(0, 0);
        gravity = -30.0f;
        speedF = 0.0f;

        if (mpMorf->getMorfRatio() < 1.0f) {
            mState = 12;
            setAnm(6, 0);
            SetOrder(2);
        }
    }

    return true;
}

/* 000031E8-00003228       .text jitanda01__10daNpc_Uk_cFv */
bool daNpc_Uk_c::jitanda01() {
    if (chkFlag(1)) {
        setFlag(0x400);
        mPrevState = 13;
        mState = 1;
    } else {
        SetOrder(2);
    }

    return true;
}

/* 00003228-000032AC       .text jitanda02__10daNpc_Uk_cFv */
bool daNpc_Uk_c::jitanda02() {
    if (chkFlag(1)) {
        mPrevState = mState;
        mState = 1;
    }

    if (checkDemoStart()) {
        mPrevState = mState;
        mState = 3;
        demo01();
    } else {
        SetOrder(1);
    }

    return true;
}

/* 000032AC-00003300       .text demo01__10daNpc_Uk_cFv */
bool daNpc_Uk_c::demo01() {
    if (!checkDemoStart()) {
        mState = mPrevState;
        setAnmStatus();
        return true;
    }

    return demoProc();
}

/* 00003300-000033D4       .text demo02__10daNpc_Uk_cFv */
bool daNpc_Uk_c::demo02() {
    if (dComIfGp_evmng_endCheck(mEventIdx)) {
        mState = mPrevState;
        setAnmStatus();
        dComIfGp_event_reset();
        clrFlag(1);
        mAttentionTimer = 5;
        return true;
    }

    if (!checkDemoStart()) {
        JUT_ASSERT(1921, 0);
    }

    return demoProc();
}

/* 000033D4-00003648       .text demoInitCom__10daNpc_Uk_cFv */
void daNpc_Uk_c::demoInitCom() {
    int* sound = dComIfGp_evmng_getMyIntegerP(mStaffIdx, "SOUND");

    if (sound != NULL) {
        switch (*sound) {
        case 1:
            switch (getShapeType()) {
            case 1:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_C_RUN_AWAY, 0);
                break;
            case 2:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_D_RUN_AWAY, 0);
                break;
            default:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_B_RUN_AWAY, 0);
                break;
            }
            break;
        case 2:
            switch (getShapeType()) {
            case 1:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_C_FOUND, 0);
                break;
            case 2:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_D_FOUND, 0);
                break;
            default:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_B_FOUND, 0);
                break;
            }
            break;
        }
    }
}

/* 00003648-000040E0       .text demoProc__10daNpc_Uk_cFv */
bool daNpc_Uk_c::demoProc() {
    BOOL immediate = FALSE;

    if (dComIfGp_evmng_getIsAddvance(mStaffIdx)) {
        demoInitCom();

        switch (mEventAction) {
        case 0:
            speedF = 0.0f;
            setAnm(0, 0);
            break;
        case 1:
            case 9: speedF = 0.0f;
            setAnm(0, 0);
            break;
        case 2:
            speedF = 0.0f;
            setAnm(2, 0);
            break;
        case 3:
            speedF = 0.0f;
            old.pos = home.pos;
            current.pos = home.pos;
            current.angle.y = home.angle.y;
            break;
        case 4:
            case 5: {
                if (mAnmIdx == 4) {
                    speedF = mRunSpeed;
                } else {
                    setAnm(4, 0);
                }

                cXyz* a_xyz = (cXyz*)dComIfGp_evmng_getMyXyzP(mStaffIdx, "Pos");

                JUT_ASSERT(2015, a_xyz);

                mStartPos = current.pos;
                mTargetPos = *a_xyz;

                int* timer = dComIfGp_evmng_getMyIntegerP(mStaffIdx, "Timer");

                if (timer != NULL) {
                    mMoveTimer = *timer;
                } else {
                    mMoveTimer = -1;
                }
                break;
            }
        case 13: {
                cXyz* a_xyz = (cXyz*)dComIfGp_evmng_getMyXyzP(mStaffIdx, "Pos");

                JUT_ASSERT(2027, a_xyz);

                mStartPos = current.pos;
                mTargetPos = *a_xyz;

                mTargetAngle = cLib_targetAngleY(&mStartPos, &mTargetPos);
                setAnm(0, 0);
                break;
            }
        case 6:
            speedF = 0.0f;
            setFlag(0x10);
            fopAcM_OffStatus(this, fopAcStts_SHOWMAP_e);
            mPrevState = 5;
            break;
        case 7:
            mCurrActionFunc = NULL;
            setAction(&daNpc_Uk_c::visit_action, NULL);

            mPrevState = mState;
            mState = 3;
            clrFlag(0x10);
            fopAcM_OnStatus(this, fopAcStts_SHOWMAP_e);
            break;
        case 8: {
                speedF = 0.0f;
                talkInit();
                setFlag(0x80);

                int* a_intP = dComIfGp_evmng_getMyIntegerP(mStaffIdx, "MsgNo");

                JUT_ASSERT(2054, a_intP);

                mMsgNo = *a_intP;

                switch (getShapeType()) {
                case 0:
                    setAnm(0, 0);
                    break;
                case 1:
                    setAnm(1, 0);
                    break;
                default:
                    setAnm(2, 0);
                    break;
                }

                immediate = TRUE;
                break;
            }
        case 10:
            speedF = 0.0f;
            setFlag(2);
            speedF = 0.0f;
            setAnm(5, 1);
            break;
        case 11:
            case 12:
            setFlag(2);
            gravity = -3.3f;
            speed.y = 28.0f;
            speedF = 12.0f;
            setFlag(0x200);
            setAnm(9, 0);

            switch (getShapeType()) {
            case 1:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_C_RUN_AWAY, 0);
                break;
            case 2:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_D_RUN_AWAY, 0);
                break;
            default:
                fopAcM_seStart(this, JA_SE_CV_TR_KO_B_RUN_AWAY, 0);
                break;
            }
            break;
        case 14: {
                speedF = 0.0f;

                cXyz* a_xyz = (cXyz*)dComIfGp_evmng_getMyXyzP(mStaffIdx, "Pos");

                JUT_ASSERT(2095, a_xyz);

                current.pos = *a_xyz;
                old.pos = *a_xyz;

                int* a_intP = dComIfGp_evmng_getMyIntegerP(mStaffIdx, "Angle");

                JUT_ASSERT(2101, a_intP);

                current.angle.y = *a_intP;
                shape_angle.y = *a_intP;
                immediate = TRUE;
                break;
            }
        }
    }

    switch (mEventAction) {
    case 4:
        case 5: {
            f32 distance;
            s16 angle;

            dNpc_calc_DisXZ_AngY(current.pos, mTargetPos, &distance, &angle);

            if (distance < mRunSpeed || mMkStatic.chkPointPass(&mStartPos, &mTargetPos, &current.pos)) {
                dComIfGp_evmng_cutEnd(mStaffIdx);

                if (mEventAction == 5) {
                    current.pos.x = mTargetPos.x;
                    current.pos.z = mTargetPos.z;
                    speedF = 0.0f;
                }
            } else {
                cLib_addCalcAngleS(&current.angle.y, angle, 4, 0x2000, 0x400);

                cLib_chaseF(&speedF, mRunSpeed, 1.1f);

                if (mMoveTimer > 0) {
                    mMoveTimer--;
                }

                if (mMoveTimer == 0) {
                    dComIfGp_evmng_cutEnd(mStaffIdx);
                }
            }
            break;
        }
    case 13:
        cLib_addCalcAngleS(&current.angle.y, mTargetAngle, 1, 0x800, 0x800);

        if (current.angle.y == mTargetAngle) {
            dComIfGp_evmng_cutEnd(mStaffIdx);
        }
        break;
    case 1:
        case 2: setFlag(0x20);
        dComIfGp_evmng_cutEnd(mStaffIdx);
        break;
    case 9:
        setFlag(0x40);
        dComIfGp_evmng_cutEnd(mStaffIdx);
        break;
    case 8: {
            u16 status = talk();

            if (status == fopMsgStts_BOX_CLOSED_e || status == 0xFE) {
                dComIfGp_evmng_cutEnd(mStaffIdx);
            }

            setFlag(0x20);

            if (immediate) {
                return true;
            }

            return mpMorf->isMorf();
        }
    case 10:
        if (chkFlag(2)) {
            if (mAnmEnded) {
                dComIfGp_evmng_cutEnd(mStaffIdx);
                clrFlag(2);
            }
        } else {
            dComIfGp_evmng_cutEnd(mStaffIdx);
        }
        break;
    case 11:
        case 12:
        if (!chkFlag(0x200)) {
            setAnm(10, 0);
            gravity = -30.0f;
            speedF = 0.0f;

            if (chkFlag(2)) {
                clrFlag(2);
                dComIfGp_getVibration().StartShock(4, -33, cXyz(0.0f, 1.0f, 0.0f));

                if (mEventAction == 12) {
                    static cXyz scale(0.6f, 0.6f, 0.6f);

                    JPABaseEmitter* emitter = dComIfGp_particle_set(0x23, &current.pos, &current.angle, &scale);

                    if (emitter != NULL) {
                        emitter->setRate(18.0f);
                        emitter->setSpread(1.0f);
                        emitter->setMaxFrame(1);
                    }
                }
            } else if (mAnmEnded) {
                dComIfGp_evmng_cutEnd(mStaffIdx);
            }
        }
        break;
    default:
        dComIfGp_evmng_cutEnd(mStaffIdx);
        break;
    }

    return true;
}

/* 000040E0-000041B0       .text wait_action__10daNpc_Uk_cFPv */
BOOL daNpc_Uk_c::wait_action(void*) {
    if (mActionStatus == ACTION_STARTING) {
        mState = 0;
        setAnmStatus();
        mActionStatus++;
    } else if (mActionStatus != ACTION_ENDING) {
        chkAttention();
        clrFlag(0x60);

        bool result;

        switch (mState) {
        case 0:
            result = wait01();
            break;
        case 1:
            result = talk01();
            break;
        default:
            result = false;
            break;
        }

        lookBack();
        setAttention(result);
    }

    return TRUE;
}

/* 000041B0-0000440C       .text seek_action__10daNpc_Uk_cFPv */
BOOL daNpc_Uk_c::seek_action(void*) {
    if (mActionStatus == ACTION_STARTING) {
        if (dComIfGs_isTmpBit(getFoundFlag())) {
            mState = 10;
            mMkStatic.init(0x50, 300);
            mMkStatic.setRndPathPos(this, &mPath);
        } else {
            mState = getShapeType() + 6;
        }

        setAnmStatus();
        mActionStatus++;
    } else if (mActionStatus != ACTION_ENDING) {
        chkAttention();
        clrFlag(0x60);

        bool result;

        switch (mState) {
        case 6:
            result = warningB();
            break;
        case 7:
            result = warningC();
            break;
        case 8:
            result = warningD();
            break;
        case 9:
            result = found();
            break;
        case 10:
            result = runaway();
            break;
        case 11:
            result = jump();
            break;
        case 12:
            result = jitanda01();
            break;
        case 13:
            result = jitanda02();
            break;
        case 3:
            result = demo01();
            break;
        case 4:
            result = demo02();
            break;
        case 1:
            result = talk02();
            break;
        case 5:
            result = false;
            setAction(&daNpc_Uk_c::hind_action, NULL);
            break;
        default:
            result = false;
            break;
        }

        lookBack();
        setAttention(result);
    }

    return TRUE;
}

/* 0000440C-0000457C       .text hind_action__10daNpc_Uk_cFPv */
BOOL daNpc_Uk_c::hind_action(void*) {
    if (mActionStatus == ACTION_STARTING) {
        mActionStatus++;
    } else if (mActionStatus != ACTION_ENDING) {
        clrFlag(0x60);

        if (mType == TYPE_MINIGAME && chkGameStart()) {
            setAction(&daNpc_Uk_c::seek_action, NULL);
            clrFlag(0x10);
            fopAcM_OnStatus(this, fopAcStts_SHOWMAP_e);
        }

        if ((mType == TYPE_NORMAL || mType == TYPE_MINIGAME) && checkDemoStart()) {
            bool result = demoProc();
            lookBack();
            setAttention(result);
        }
    }

    return TRUE;
}

/* 0000457C-00004774       .text visit_action__10daNpc_Uk_cFPv */
BOOL daNpc_Uk_c::visit_action(void*) {
    if (mActionStatus == ACTION_STARTING) {
        if (dComIfGs_isTmpBit(0x40)) {
            mState = 13;
            setAnm(6, 0);
            mStts.SetWeight(0xFE);
        } else {
            mState = 2;
            setAnmStatus();
            mStts.SetWeight(100);
        }

        mActionStatus++;
        mVisitMode = 0;
    } else if (mActionStatus != ACTION_ENDING) {
        chkAttention();
        clrFlag(0x60);

        bool result;

        switch (mState) {
        case 3:
            result = demo01();
            break;
        case 4:
            result = demo02();
            break;
        case 2:
            result = visit01();
            break;
        case 1:
            result = talk01();
            break;
        case 13:
            result = jitanda02();
            break;
        case 5:
            result = false;
            setAction(&daNpc_Uk_c::hind_action, NULL);
            break;
        default:
            result = false;
            break;
        }

        lookBack();
        setAttention(result);
    }

    return TRUE;
}

/* 00004774-00004A20       .text _draw__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::_draw() {
    J3DModel* model = mpMorf->getModel();

    J3DModelData* modelData = model->getModelData();

    J3DModelData* headData = mpHeadModel->getModelData();

    if (chkFlag(0x10)) {
        return TRUE;
    }

    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);

    g_env_light.setLightTevColorType(model, &tevStr);

    g_env_light.setLightTevColorType(mpHeadModel, &tevStr);

    J3DTexture* texture;

    JUTNameTab* textureName;

    if (getShapeType() != 0) {
        static int table_bmt[] = {
            -1, dRes_INDEX_UK_BMT_UK_C_e, dRes_INDEX_UK_BMT_UK_D_e };

        texture = modelData->getTexture();
        textureName = modelData->getTextureName();

        J3DMaterialTable* table = (J3DMaterialTable*)dComIfG_getObjectRes("Uk", table_bmt[getShapeType()]);

        modelData->setMaterialTable(table, (J3DMaterialCopyFlag)2);
    }

    if (getShapeType() == 1) {
        mBtpAnm.entry(headData, mBlinkFrame);
    }

    mpMorf->entryDL();

    mpHeadModel->setBaseTRMtx(model->getAnmMtx(getHeadJntNum()));

    mDoExt_modelUpdateDL(mpHeadModel);

    if (getShapeType() == 1) {
        mBtpAnm.remove(headData);
    }

    if (getShapeType() != 0) {
        modelData->setTexture(texture);
        modelData->setTextureName(textureName);
    }

    cXyz shadowPos(current.pos.x, current.pos.y + 50.0f, current.pos.z);

    mShadowId = dComIfGd_setShadow(mShadowId, 1, model, &shadowPos, 300.0f, 20.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr);

    if (mShadowId != 0) {
        dComIfGd_setShadow(mShadowId, 1, mpHeadModel, &shadowPos, 300.0f, 20.0f, current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr);
    }

    switch (getShapeType()) {
    case 0:
        dSnap_RegistFig(DSNAP_TYPE_NPC_MK, this, 1.0f, 1.0f, 1.0f);
        break;
    case 1:
        dSnap_RegistFig(DSNAP_TYPE_NPC_MK, this, 1.0f, 1.0f, 1.0f);
        break;
    case 2:
        dSnap_RegistFig(DSNAP_TYPE_NPC_MK, this, 1.0f, 1.0f, 1.0f);
        break;
    }

    return TRUE;
}

/* 00004A20-00004CE4       .text _execute__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::_execute() {
    m_jnt.setParam(
    /* mMaxBackboneX */ 0,
    /* mMaxBackboneY */ 7300,
    /* mMinBackboneX */ 0,
    /* mMinBackboneY */ -7300,
    /* mMaxHeadX */ 8000,
    /* mMaxHeadY */ 9000,
    /* mMinHeadX */ -2000,
    /* mMinHeadY */ -9000,
    /* mMaxTurnStep */ 1000
    );

    playTexPatternAnm();

    mAnmEnded = mpMorf->play(&eyePos, mMtrlSndId, mReverb);

    if(mpMorf->getFrame() < mAnmTimer) {
        mAnmEnded = 1;
    }

    mAnmTimer = mpMorf->getFrame();

    if (mAnmEnded != 0) {
        switch(mAnmIdx) {
        case 6:
            if(cM_rnd() < 0.4f) {
                setAnm(7, 0);
            }
            break;
        case 7:
            setAnm(6, 0);
            break;
        }
    }

    checkOrder();

    (this->*mCurrActionFunc)(NULL);

    eventOrder();

    if(!chkFlag(0x100)) {
        shape_angle.y = current.angle.y;
    }

    clrFlag(0x100);

    mMtrlSndId = 0;

    if(!chkFlag(0x10)) {
        if (chkFlag(8)) {
            speed.y += fopAcM_GetGravity(this);

            if (speed.y < fopAcM_GetMaxFallSpeed(this)) {
                speed.y = fopAcM_GetMaxFallSpeed(this);
            }

            fopAcM_posMove(this, mStts.GetCCMoveP());
        } else {
            fopAcM_posMoveF(this, mStts.GetCCMoveP());
        }

        mObjAcch.CrrPos(*dComIfG_Bgsp());

        if (mObjAcch.ChkGroundHit()) {
            clrFlag(0x200);

            mMtrlSndId = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
        } else {
            setFlag(0x200);
        }

        tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);

        tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    } else {
        setFlag(0x200);
    }

    clrFlag(8);

    J3DModel* pModel = mpMorf->getModel();

    mDoMtx_stack_c::transS(current.pos);

    mDoMtx_stack_c::YrotM(shape_angle.y);

    pModel->setBaseTRMtx(mDoMtx_stack_c::get());

    mpMorf->calc();

    if(chkFlag(0x10)) {
        mCyl.OffCoSetBit();
    } else {
        mCyl.OnCoSetBit();

        setCollision();
    }

    return TRUE;
}

/* 00004CE4-00004D34       .text _delete__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::_delete() {
    dComIfG_resDelete(&mPhs, "Uk");

    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }

    return TRUE;
}

/* 00004D34-00004D54       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Uk_c*)actor)->CreateHeap();
}

/* 00004D54-00005028       .text _create__10daNpc_Uk_cFv */
cPhs_State daNpc_Uk_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Uk");

    if (phase == cPhs_COMPLEATE_e) {
        fopAcM_SetupActor(this, daNpc_Uk_c);

#else
    fopAcM_SetupActor(this, daNpc_Uk_c);

    cPhs_State phase = dComIfG_resLoad(&mPhs, "Uk");

    if (phase == cPhs_COMPLEATE_e) {
#endif
        switch (fopAcM_GetName(this)) {
        case fpcNm_NPC_UK_e:
            switch (getType()) {
            case TYPE_NORMAL:
                mType = TYPE_NORMAL;

                switch (getShapeType()) {
                case 0:
                    mWalkSpeed = 2.5f;
                    mRunSpeed = 10.0f;
                    break;
                case 1:
                    mWalkSpeed = 2.3f;
                    mRunSpeed = 8.5f;
                    break;
                case 2:
                    mWalkSpeed = 2.0f;
                    mRunSpeed = 7.0f;
                    break;
                }
                break;
            case TYPE_MINIGAME:
                mType = TYPE_MINIGAME;
                argument = getShapeType() + 5;
                mRunSpeed = 14.0f;
                break;
            default:
                mType = TYPE_NONE;
                break;
            }
            break;
        default:
            return cPhs_ERROR_e;
        }

        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, 0xB7B0)) {
#if VERSION > VERSION_DEMO
            mpMorf = NULL;
#endif
            return cPhs_ERROR_e;
        }

        tevStr.mRoomNo = current.roomNo;

        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());

        fopAcM_setCullSizeBox(this, -35.0f, -10.0f, -35.0f, 35.0f, 100.0f, 35.0f);

        if (!init()) {
#if VERSION > VERSION_DEMO
            mpMorf = NULL;
#endif
            return cPhs_ERROR_e;
        }
    }

    return phase;
}

/* 000053D8-00005728       .text CreateHeap__10daNpc_Uk_cFv */
BOOL daNpc_Uk_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("Uk", dRes_INDEX_UK_BDL_UK_e);

    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL,
    (J3DAnmTransformKey*)dComIfG_getObjectRes("Uk", dRes_INDEX_UK_BCK_UK_WAIT_e),
    J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x15021222);

    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
        return FALSE;
    }

    m_jnt.setHeadJntNum(modelData->getJointName()->getIndex("head"));

    JUT_ASSERT(DEMO_SELECT(2770, 2772), m_jnt.getHeadJntNum() >= 0);

    m_jnt.setBackboneJntNum(modelData->getJointName()->getIndex("backbone"));

    JUT_ASSERT(DEMO_SELECT(2775, 2777), m_jnt.getBackboneJntNum() >= 0);

    static int head_bdl_table[] = {
        dRes_INDEX_UK_BDL_UKHEAD_B_e, dRes_INDEX_UK_BDL_UKHEAD_C_e, dRes_INDEX_UK_BDL_UKHEAD_D_e };

    J3DModelData* headModelData = (J3DModelData*)dComIfG_getObjectRes("Uk", head_bdl_table[getShapeType()]);

    JUT_ASSERT(DEMO_SELECT(2788, 2790), headModelData);

    mpHeadModel = mDoExt_J3DModel__create(headModelData, 0x80000, 0x11020022);

    if (mpHeadModel == NULL) {
        return FALSE;
    }

    mTexPatternIdx = 0;

#if VERSION == VERSION_DEMO
    initTexPatternAnm(false);
#else
    if (!initTexPatternAnm(false)) {
        return FALSE;
    }
#endif

    for (u16 i = 0; i < modelData->getJointNum(); i++) {
        if (i == getHeadJntNum() || i == getBackboneJntNum()) {
            mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Uk);
        }
    }

    mpMorf->getModel()->setUserArea((u32)this);

    mAcchCir.SetWall(60.0f, 30.0f);

    mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));

    mObjAcch.SetGroundCheckOffset(100.0f);

    fopAcM_SetMaxFallSpeed(this, -90.0f);
    mLeaderID = fpcM_ERROR_PROCESS_ID_e;

    return TRUE;
}

/* 00005728-00005748       .text daNpc_Uk_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Uk_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Uk_c*)i_this)->_create();
}

/* 00005748-00005768       .text daNpc_Uk_Delete__FP10daNpc_Uk_c */
static BOOL daNpc_Uk_Delete(daNpc_Uk_c* i_this) {
    return ((daNpc_Uk_c*)i_this)->_delete();
}

/* 00005768-00005788       .text daNpc_Uk_Execute__FP10daNpc_Uk_c */
static BOOL daNpc_Uk_Execute(daNpc_Uk_c* i_this) {
    return ((daNpc_Uk_c*)i_this)->_execute();
}

/* 00005788-000057A8       .text daNpc_Uk_Draw__FP10daNpc_Uk_c */
static BOOL daNpc_Uk_Draw(daNpc_Uk_c* i_this) {
    return ((daNpc_Uk_c*)i_this)->_draw();
}

/* 000057A8-000057B0       .text daNpc_Uk_IsDelete__FP10daNpc_Uk_c */
static BOOL daNpc_Uk_IsDelete(daNpc_Uk_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Uk_Method = {
    (process_method_func)daNpc_Uk_Create,
    (process_method_func)daNpc_Uk_Delete,
    (process_method_func)daNpc_Uk_Execute,
    (process_method_func)daNpc_Uk_IsDelete,
    (process_method_func)daNpc_Uk_Draw,
};

actor_process_profile_definition g_profile_NPC_UK = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_UK_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Uk_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_UK_e,
    /* Actor SubMtd */ &l_daNpc_Uk_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
