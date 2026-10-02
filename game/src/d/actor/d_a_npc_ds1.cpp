/**
* d_a_npc_ds1.cpp
* NPC - Doc Bandam (Windfall Potion Shop shopkeeper)
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_ds1.h"
#include "d/d_cc_d.h"
#include "res/Object/Ds.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "m_Do/m_Do_controller_pad.h"
#include "d/actor/d_a_player.h"
#include "d/d_snap.h"
#include "d/d_s_play.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"

static fpc_ProcID l_msgId;

static msg_class* l_msg;

static daNpc_Ds1_HIO_c l_HIO;

#if VERSION > VERSION_DEMO
static cXyz se_pos[] = {cXyz(-158.0f, 160.0f, -663.0f), cXyz(18.0f, 160.0f, -652.0f), cXyz(-220.0f, 105.0f, -590.0f), cXyz(-225.0f, 260.0f, -590.0f)};
#endif

static dCcD_SrcCyl l_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0, /* SrcObjAt  Type    */ 0, /* SrcObjAt  Atp     */ 0, /* SrcObjAt  SPrm    */ 0, /* SrcObjTg  Type    */ AT_TYPE_ALL, /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e, /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e, /* SrcGObjAt Se      */ 0, /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e, /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0, /* SrcGObjAt Mtrl    */ 0, /* SrcGObjAt SPrm    */ 0, /* SrcGObjTg Se      */ 0, /* SrcGObjTg HitMark */ 0, /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0, /* SrcGObjTg Mtrl    */ 0, /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e, /* SrcGObjCo SPrm    */ 0, }, // cM3dGCylS
    {{
            /* Center */ {0.0f, 0.0f, 0.0f}, /* Radius */ 30.0f, /* Height */ 80.0f, }}, };

/* 000000EC-00000108       .text __ct__20daNpc_Ds1_childHIO_cFv */
daNpc_Ds1_childHIO_c::daNpc_Ds1_childHIO_c() {
}

/* 00000108-00000194       .text daNpc_Ds1_checkCreateDrugChuchu__FUc */
static bool daNpc_Ds1_checkCreateDrugChuchu(u8 item) {
    if (item == 0x49 || (item == 0x4A && dComIfGs_isEventBit(0xD04)) || (item == 0x4B && dComIfGs_isEventBit(0xD02))) {
        return true;
    }
    return false;
}

/* 00000194-00000298       .text __ct__15daNpc_Ds1_HIO_cFv */
daNpc_Ds1_HIO_c::daNpc_Ds1_HIO_c() {
    mChild[0].mNpc.m04 = -80.0f;
    mChild[0].mNpc.mMaxHeadX = 8190;
    mChild[0].mNpc.mMaxHeadY = 14560;
    mChild[0].mNpc.mMaxBackboneX = 0;
    mChild[0].mNpc.mMaxBackboneY = 7280;
    mChild[0].mNpc.mMinHeadX = -8190;
    mChild[0].mNpc.mMinHeadY = 0;
    mChild[0].mNpc.mMinBackboneX = 0;
    mChild[0].mNpc.mMinBackboneY = 0;
    mChild[0].mNpc.mMaxTurnStep = 4096;
    mChild[0].mNpc.mMaxHeadTurnVel = 2048;
    mChild[0].mNpc.mAttnYOffset = 60.0f;
    mChild[0].mNpc.m22 = 0;
    mChild[0].mNpc.mMaxAttnDistXZ = 300.0f;
    mChild[0].mAttnWidth = 300.0f;
    mChild[0].mAttnDepth = 300.0f;
    mChild[0].mCursorMin = 0.65f;
    mChild[0].mCursorMax = 0.9f;
    mChild[0].mCursorSpeed = 0.5f;
    mChild[0].mCursorY = 27.0f;
    mChild[0].mCursorZ = 20.0f;
    mNo = -1;
    mSelected = -1;
}

inline daNpc_Ds1_childHIO_c::~daNpc_Ds1_childHIO_c() {
}

static const int l_bck_ix_tbl[] = {
    dRes_INDEX_DS_BCK_WAIT01_e,
    dRes_INDEX_DS_BCK_TALK01_e,
    dRes_INDEX_DS_BCK_AMAZED_e,
    dRes_INDEX_DS_BCK_WAIT01_e,
    dRes_INDEX_DS_BCK_FLUT_e,
    dRes_INDEX_DS_BCK_WALK_e,
    dRes_INDEX_DS_BCK_MAKE_e,
    dRes_INDEX_DS_BCK_WAIT02_e,
    dRes_INDEX_DS_BCK_MEGANE_e,
    dRes_INDEX_DS_BCK_FLUT_e,
};

static const int l_btp_ix_tbl[] = {dRes_INDEX_DS_BTP_MABA01_e, dRes_INDEX_DS_BTP_MABA02_e};

/* 000002F8-00000318       .text daNpc_Ds1_XyEventCB__FPvi */
static s16 daNpc_Ds1_XyEventCB(void* actor, int index) {
    return ((daNpc_Ds1_c*)actor)->XyEventCB(index);
}

/* 00000318-00000550       .text XyEventCB__11daNpc_Ds1_cFi */
s16 daNpc_Ds1_c::XyEventCB(int index) {
    s16 event = -1;
    u8 item = dComIfGp_getSelectItem(index);
    if (item == 0x49 || item == 0x4A || item == 0x4B) {
        if (!daNpc_Ds1_checkCreateDrugChuchu(item) && ((item == 0x49 && dComIfGs_getBeastNum(4) >= 10) || (item == 0x4A && dComIfGs_getBeastNum(5) >= DEMO_SELECT(20, 15)) || (item == 0x4B && dComIfGs_getBeastNum(6) >= DEMO_SELECT(20, 15)))) {
            event = mEventIdx[2];
            mOrder = 5;
            mTalkMode = 2;
            setAction(&daNpc_Ds1_c::event_action, NULL);
        } else if (!daNpc_Ds1_checkCreateDrugChuchu(item)) {
            event = mEventIdx[3];
            mOrder = 6;
            mTalkMode = 3;
            setAction(&daNpc_Ds1_c::event_action, NULL);
        }
        mJellyItem = item;
    } else {
        mTalkMode = 4;
    }
    return event;
}

/* 00000550-00000718       .text nodeCallBack_Ds__FP7J3DNodei */
static BOOL nodeCallBack_Ds(J3DNode* node, int phase) {
    if (phase == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Ds1_c* actor = (daNpc_Ds1_c*)model->getUserArea();
        int joint = ((J3DJoint*)node)->getJntNo();
        if (actor) {
#if VERSION == VERSION_DEMO
            MTXCopy(model->getAnmMtx(joint), *calc_mtx);
#else
            cMtx_copy(model->getAnmMtx(joint), *calc_mtx);
#endif
            if (joint == actor->getHeadJntNum()) {
                cXyz offset(0.0f, 0.0f, 0.0f), pos;
#if VERSION == VERSION_DEMO
                cMtx_YrotM(*calc_mtx, -actor->getHead_y());
                cMtx_ZrotM(*calc_mtx, -actor->getHead_x());
#else
                mDoMtx_YrotM(*calc_mtx, -actor->getHead_y());
                mDoMtx_ZrotM(*calc_mtx, -actor->getHead_x());
#endif
                MtxPosition(&offset, &pos);
                actor->setAttentionBasePos(pos);
                offset.set(28.0f, 20.0f, 0.0f);
                MtxPosition(&offset, &pos);
                actor->setEyePos(pos);
                actor->incAttnSetCount();
            } else if (joint == actor->getBackboneJntNum()) {
#if VERSION == VERSION_DEMO
                cMtx_XrotM(*calc_mtx, actor->getBackbone_y());
                cMtx_ZrotM(*calc_mtx, actor->getBackbone_x());
#else
                mDoMtx_XrotM(*calc_mtx, actor->getBackbone_y());
                mDoMtx_ZrotM(*calc_mtx, actor->getBackbone_x());
#endif
            }
            cMtx_copy(*calc_mtx, J3DSys::mCurrentMtx);
            model->setAnmMtx(joint, *calc_mtx);
        }
    }
    return TRUE;
}

char daNpc_Ds1_c::m_arcname[] = "Ds";

/* 00000718-00000824       .text initTexPatternAnm__11daNpc_Ds1_cFb */
BOOL daNpc_Ds1_c::initTexPatternAnm(bool modify) {
    J3DModelData* data = mpMorf->getModel()->getModelData();
    m_head_tex_pattern = (J3DAnmTexPattern*)dComIfG_getObjectRes(m_arcname, l_btp_ix_tbl[mTexAnm]);
    JUT_ASSERT(DEMO_SELECT(0x1DE, 0x1E8), m_head_tex_pattern != 0);
    if (!mBtp.init(data, m_head_tex_pattern, 1, 2, 1.0f, 0, -1, modify, 0)) {
        return FALSE;
    }
    mTexFrame = 0;
    mBlinkTimer = 0;
    return TRUE;
}

/* 00000824-000008B0       .text playTexPatternAnm__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::playTexPatternAnm() {
    if (!cLib_calcTimer(&mBlinkTimer)) {
        if (mTexFrame >= m_head_tex_pattern->getFrameMax()) {
            mTexFrame -= m_head_tex_pattern->getFrameMax();
            mBlinkTimer = 30.0f + cM_rndF(100.0f);
        } else {
            mTexFrame++;
        }
    }
}

/* 000008B0-000009A8       .text setAnm__11daNpc_Ds1_cFScf */
void daNpc_Ds1_c::setAnm(s8 anm, f32 morf) {
    static int play_mode_tbl[] = {2, 2, 2, 2, 2, 2, 0, 2, 0, 2};
    static f32 morf_frame_tbl[] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
    static f32 play_speed_tbl[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    if (anm != mAnm || mAnm == -1) {
        mAnm = anm;
        if (morf < 0.0f) {
            morf = morf_frame_tbl[anm];
        }
        dNpc_setAnm_2(mpMorf, play_mode_tbl[anm], morf, play_speed_tbl[anm], l_bck_ix_tbl[anm], -1, m_arcname);
        if (anm == 4 || anm == 6 || anm == 8 || anm == 9 || anm == 7) {
            mJnt.onHeadLock();
            mJnt.onBackBoneLock();
        } else {
            mJnt.offHeadLock();
            mJnt.offBackBoneLock();
        }
    }
}

/* 000009A8-000009EC       .text setTexAnm__11daNpc_Ds1_cFSc */
void daNpc_Ds1_c::setTexAnm(s8 anm) {
    if (mTexAnm != anm || mTexAnm == -1) {
        mTexAnm = anm;
        initTexPatternAnm(true);
    }
}

/* 000009EC-00000CAC       .text setAnmFromMsgTag__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::setAnmFromMsgTag() {
    switch (dComIfGp_getMesgAnimeAttrInfo()) {
    case 0:
        setAnm(0, -1.0f);
        setTexAnm(0);
        break;
    case 1:
        setAnm(1, -1.0f);
        setTexAnm(0);
        break;
    case 2:
        setAnm(2, -1.0f);
        setTexAnm(1);
        break;
    case 3:
        setAnm(3, -1.0f);
        setTexAnm(0);
        break;
    case 4:
        setAnm(4, -1.0f);
        setTexAnm(1);
        break;
    case 5:
        setAnm(9, -1.0f);
        setTexAnm(1);
        break;
    }
    if (mAnm == 1 && mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) {
        if (++mAnmCount > 1) {
            setAnm(0, -1.0f);
            mAnmCount = 0;
        }
    }
    if (mAnm == 9 && mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) {
        if (++mAnmCount > 4) {
            setAnm(0, -1.0f);
            mAnmCount = 0;
        }
    }
    if (mAnm == 3 && mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) {
        if (++mAnmCount > 1) {
            setAnm(2, -1.0f);
            mAnmCount = 0;
        }
    }
    dComIfGp_clearMesgAnimeAttrInfo();
}

/* 00000CAC-00000DB0       .text chkAttention__11daNpc_Ds1_cF4cXyzs */
BOOL daNpc_Ds1_c::chkAttention(cXyz pos, s16 angle) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 depth = l_HIO.mChild[mType].mAttnDepth, width = l_HIO.mChild[mType].mAttnWidth;
    cXyz delta = player->current.pos - pos;
    cXyz forward(0.0f, 0.0f, 1.0f), side(1.0f, 0.0f, 0.0f);
    f32 z = forward.inprod(delta);
    f32 x = side.inprod(delta);
    f32 absX = std::fabsf(x);
    BOOL result = FALSE;
    if (depth > z && width > absX) {
        result = TRUE;
    }
    return result;
}

/* 00000DB0-00000EAC       .text eventOrder__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::eventOrder() {
    if (mOrder == 3) {
        fopAcM_orderOtherEventId(this, mEventIdx[1], 255, 65535, 0, 1);
    } else if (mOrder == 4) {
        fopAcM_orderOtherEventId(this, mEventIdx[0], 255, 65535, 0, 1);
    } else if (mOrder == 5) {
        fopAcM_orderOtherEventId(this, mEventIdx[2], 255, 65535, 0, 1);
    } else if (mOrder == 6) {
        fopAcM_orderOtherEventId(this, mEventIdx[3], 255, 65535, 0, 1);
    } else if (mOrder == 1 || mOrder == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        eventInfo.onCondition(dEvtCnd_CANTALKITEM_e);
        if (mOrder == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    }
}

/* 00000EAC-000011F8       .text checkOrder__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (mOrder == 3) {
            mOrder = 0;
            mShopCam.setCamAction(NULL);
            setAction(&daNpc_Ds1_c::getdemo_action, NULL);
        } else if (mOrder == 4) {
            mOrder = 0;
            mShopCam.setCamAction(NULL);
            setAction(&daNpc_Ds1_c::event_action, NULL);
        } else if (mOrder == 5) {
            mOrder = 0;
        } else if (mOrder == 5) {
            mOrder = 0;
        }
    } else if (eventInfo.checkCommandTalk() && (mOrder == 1 || mOrder == 2)) {
        if (dComIfGp_event_chkTalkXY()) {
            mOrder = 0;
            if (mTalkMode != 2) {
                m761 = 1;
                talkInit();
            }
            mJellyItem = dComIfGp_event_getPreItemNo();
        } else {
            if (dComIfGs_isEventBit(0x1120)) {
                mShopCam.shop_cam_action_init();
                cXyz pos(10.0f, 0.0f, 100.0f);
                ((daPy_py_c*)dComIfGp_getPlayer(0))->setPlayerPosAndAngle(&pos, -0x6000);
            } else {
                mShopCam.ds_normal_cam_action_init();
                cXyz pos(-115.0f, 0.0f, 100.0f);
                ((daPy_py_c*)dComIfGp_getPlayer(0))->setPlayerPosAndAngle(&pos, -0x6000);
            }
            mOrder = 0;
            m761 = 1;
            talkInit();
        }
    }
}

/* 000011F8-00001698       .text next_msgStatus__11daNpc_Ds1_cFPUl */
u16 daNpc_Ds1_c::next_msgStatus(u32* msg) {
    u16 status = 15;
    u32 price;
    u32 msgNo = *msg;
    switch (msgNo) {
    case 0x1DB1:
    case 0x1DB2: case 0x1DB3: case 0x1DB5: case 0x1DB6: case 0x1DB7: case 0x1DB8: case 0x1DBB: case 0x1DBC: case 0x1DC9: case 0x1DCE: (*msg)++;
        break;
    case 0x1DCF:
        if (mJellyItem == 0x49) {
            *msg = 0x1DD0;
        } else {
            *msg = 0x1DDE;
        }
        break;
    case 0x1DC0:
        if (CPad_CHECK_TRIG_B(0)) {
            *msg = 0x1DDC;
        } else {
            dComIfGp_setDoStatusForce(0x17);
            dComIfGp_setAStatusForce(0x27);
            status = 14;
        }
        break;
    case 0x1DC1:
        *msg = 0x1DC0;
        break;
    case 0x1DDB:
        *msg = 0x1DC0;
        break;
    case 0x1DD1:
    case 0x1DD2: case 0x1DD3: if (CPad_CHECK_TRIG_B(0)) *msg = 0x1DDC;
        else {
            dComIfGp_setDoStatusForce(0x17);
            dComIfGp_setAStatusForce(0x27);
            *msg += 3;
        }
        break;
    case 0x1DC3:
        if (!daNpc_Ds1_checkCreateDrugChuchu(mJellyItem)) {
            *msg = 0x1DC8;
        } else if ((mJellyItem == 0x49 && dComIfGs_getBeastNum(4) >= 5) || (mJellyItem == 0x4A && dComIfGs_getBeastNum(5) >= 5) || (mJellyItem == 0x4B && dComIfGs_getBeastNum(6) >= 5)) {
            *msg = 0x1DC4;
        } else {
            *msg = 0x1DC7;
        }
        break;
    case 0x1DC4:
        if (dComIfGs_checkEmptyBottle()) {
            status = 16;
        } else {
            *msg = 0x1DC5;
        }
        break;
    case 0x1DC8:
        if ((mJellyItem == 0x49 && dComIfGs_getBeastNum(4) >= 10) || (mJellyItem == 0x4A && dComIfGs_getBeastNum(5) >= DEMO_SELECT(20, 15)) || (mJellyItem == 0x4B && dComIfGs_getBeastNum(6) >= DEMO_SELECT(20, 15))) {
            *msg = 0x1DC9;
        } else {
            *msg = 0x1DCE;
        }
        break;
    case 0x1DD7:
    case 0x1DD8: case 0x1DD9: case 0x1DDA: *msg = 0x1DC0;
        break;
    case 0x1DCB:
        if (dComIfGs_checkEmptyBottle()) {
            status = 16;
        } else {
            *msg = 0x1DCC;
        }
        break;
    case 0x1DD4:
    case 0x1DD5: case 0x1DD6: if (dComIfGp_checkMesgCancelButton()) *msg = msgNo - 3;
        else if (l_msg->mSelectNum == 0) {
            price = dComIfGp_getMessageRupee();
            u8 error = dShop_BoughtErrorStatus(&mShopItems, 0, price);
            if (error&8) {
                *msg = 0x1DD7;
            } else if (error&16) {
                *msg = 0x1DD8;
            } else if (error&32) {
                *msg = 0x1DD9;
            } else if (!checkItemGet((int)mShopItems.getSelectItemNo(), 0)) {
                mDoAud_seStart(0x87F, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
                mShopItems.hideSelectItem();
                m7D4 = mShopItems.getSelectItemNo();
                dComIfGp_setItemRupeeCount(-price);
                mOrder = 3;
                mTalkMode = 4;
                status = 16;
            } else {
                mDoAud_seStart(0x87F, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
                mShopItems.hideSelectItem();
                m7D4 = mShopItems.getSelectItemNo();
                execItemGet((int)mShopItems.getSelectItemNo());
                dComIfGp_setItemRupeeCount(-price);
                *msg = 0x1DDA;
            }
        } else {
            *msg -= 3;
        }
        break;
    case 0x1DB9:
        if (l_msg->mSelectNum == 0) {
            *msg = 0x1DBB;
        } else {
            *msg = 0x1DBA;
        }
        break;
    default:
        status = 16;
        break;
    }
    return status;
}

/* 00001698-00001794       .text getMsg__11daNpc_Ds1_cFv */
u32 daNpc_Ds1_c::getMsg() {
    u32 msg;
    if (m7D0) {
        msg = m7D0;
        m7D0 = 0;
    } else if (dComIfGp_event_chkTalkXY()) {
        if (mJellyItem == 0x49 || mJellyItem == 0x4A || mJellyItem == 0x4B) {
            dComIfGs_onEventBit(0x1120);
            msg = 0x1DC3;
        } else if (!dComIfGs_isEventBit(0x1120)) {
            msg = 0x1DDD;
        } else {
            msg = 0x1DC2;
        }
    } else if (dComIfGs_isEventBit(0x1120)) {
        msg = 0x1DDB;
    } else {
        msg = 0x1DB1;
    }
    return msg;
}

/* 00001794-00001850       .text setCollision__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::setCollision() {
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

/* 00001850-0000185C       .text talkInit__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::talkInit() {
    m899 = 0;
}

/* 0000185C-00001884       .text daNpc_Ds1_shopMsgCheck__FUl */
static BOOL daNpc_Ds1_shopMsgCheck(u32 msg) {
    if ((msg >= 0x1DD1 && msg <= 0x1DDA) || msg == 0x1DC0) {
        return TRUE;
    }
    return FALSE;
}

/* 00001884-000018AC       .text daNpc_Ds1_shopStickMoveMsgCheck__FUl */
static BOOL daNpc_Ds1_shopStickMoveMsgCheck(u32 msg) {
    if ((msg >= 0x1DD1 && msg <= 0x1DD3) || msg == 0x1DC0) {
        return TRUE;
    }
    return FALSE;
}

/* 000018AC-00001B04       .text normal_talk__11daNpc_Ds1_cFv */
u16 daNpc_Ds1_c::normal_talk() {
    u16 status = l_msg->mStatus;
    if (status == 14) {
        l_msg->mStatus = next_msgStatus(&mCurrMsg);
        if (l_msg->mStatus == 15) {
            fopMsgM_messageSet(mCurrMsg);
        }
    } else if (status == 18) {
        if (mCurrMsg == 0x1DCA) {
            int index;
            s16 amount;
            if (mJellyItem == 0x49) {
                index = 4;
                amount = -10;
            } else if (mJellyItem == 0x4A) {
                index = 5;
                amount = DEMO_SELECT(-20, -15);
            } else {
                index = 6;
                amount = DEMO_SELECT(-20, -15);
            }
            dComIfGp_setItemBeastNumCount(index, amount);
        } else if (mCurrMsg == 0x1DC4) {
            int index;
            if (mJellyItem == 0x49) {
                index = 4;
            } else if (mJellyItem == 0x4A) {
                index = 5;
            } else {
                index = 6;
            }
            dComIfGp_setItemBeastNumCount(index, -5);
        }
        if (mCurrMsg == 0x1DCA) {
            mOrder = 4;
        } else if (mCurrMsg == 0x1DCB || mCurrMsg == 0x1DC4) {
            mOrder = 3;
        } else if (mCurrMsg == 0x1DCD || mCurrMsg == 0x1DCC) {
            mOrder = 0;
            if (mJellyItem == 0x49) {
            } else if (mJellyItem == 0x4A) {
                dComIfGs_onEventBit(0xD04);
            } else {
                dComIfGs_onEventBit(0xD02);
            }
            dComIfGp_setNextStage("Pdrgsh", 1, fopAcM_GetRoomNo(this), -1, 0, 0, 1, 0);
        }
        l_msg->mStatus = 19;
    } else if (status == 1) {
        fopMsgM_demoMsgFlagOn();
    }
    cXyz pos = mShopCam.getItemZoomPos(100.0f);
    mShopItems.Item_ZoomUp(pos);
    mpShopCursor->hide();
    return status;
}

/* 00001B04-00001C4C       .text shop_talk__11daNpc_Ds1_cFv */
u16 daNpc_Ds1_c::shop_talk() {
    mpShopCursor->show();
    if (dShop_now_triggercheck(l_msg, &mStick, &mShopItems, &mCurrMsg, NULL, NULL)) {
        m738 = 1;
        mNextMsg = 0;
    }
    u16 status = l_msg->mStatus;
    if (status == 14 || status == 15) {
        if (m738) {
            m738 = 0;
        } else {
            mNextMsg = mCurrMsg;
            l_msg->mStatus = next_msgStatus(&mNextMsg);
            if (l_msg->mStatus == 15) {
                fopMsgM_messageSet(mNextMsg);
            }
        }
    } else if (status == 18) {
        l_msg->mStatus = 19;
        if (mCurrMsg == 0x1DD4 || mCurrMsg == 0x1DD5 || mCurrMsg == 0x1DD6 || mCurrMsg == 0x1DC4) {
            mOrder = 3;
        }
        mShopItems.mSelectedItemIdx = -1;
    } else if (status == 1) {
        fopMsgM_demoMsgFlagOn();
    }
    return status;
}

/* 00001C4C-00001E84       .text talk__11daNpc_Ds1_cFv */
u16 daNpc_Ds1_c::talk() {
    u16 status = 255;
    if (m899 == 0) {
        l_msgId = fpcM_ERROR_PROCESS_ID_e;
        l_msg = NULL;
        mCurrMsg = getMsg();
        mNextMsg = 0;
        m899 = 1;
        m7D4 = 255;
    } else if (m899 != -1) {
        if (l_msgId == fpcM_ERROR_PROCESS_ID_e) {
            if (dComIfGp_event_chkTalkXY() && !dComIfGp_evmng_ChkPresentEnd()) {
                return 255;
            }
            l_msgId = fopMsgM_messageSet(mCurrMsg, this);
        } else if (!l_msg) {
            l_msg = fopMsgM_SearchByID(l_msgId);
            if (l_msg) {
                if (mCurrMsg != 0x1DC0) {
                    m899 = 2;
                } else {
                    m899 = 3;
                }
            }
        } else {
            setAnmFromMsgTag();
            switch (m899) {
            case 2:
                status = normal_talk();
                break;
            case 3:
                status = shop_talk();
                break;
            }
            if (dComIfGp_checkMesgSendButton()) {
                mCurrMsg = l_msg->mMsgNo;
                if (!daNpc_Ds1_shopStickMoveMsgCheck(mCurrMsg)) {
                    if (!daNpc_Ds1_shopMsgCheck(mCurrMsg)) {
                        mShopItems.mSelectedItemIdx = -1;
                        if (mShopItems.isHide()) {
                            mShopItems.showItem();
                        }
                    }
                    m899 = 2;
                } else {
                    if (mCurrMsg == 0x1DC0) {
                        mShopItems.mSelectedItemIdx = -1;
                        if (mShopItems.isHide()) {
                            mShopItems.showItem();
                        }
                    }
                    m899 = 3;
                }
            }
        }
    }
    mShopCam.m54 = mShopItems.mSelectedItemIdx;
    return status;
}

/* 00001E84-00002158       .text CreateInit__11daNpc_Ds1_cFv */
BOOL daNpc_Ds1_c::CreateInit() {
    cXyz zero(0.0f, 0.0f, 0.0f);
    mInitialAngle = current.angle;
    attention_info.flags = 10;
    gravity = -30.0f;
    switch ((u8)mType) {
    case 0:
        setAction(&daNpc_Ds1_c::wait_action, NULL);
        break;
    }
    mAttentionBasePos = current.pos;
    mStts.Init(255, 255, this);
    mCyl.Set(l_cyl_src);
    mCyl.SetStts(&mStts);
    m89A = 0;
    m738 = 0;
    m7D0 = 0;
    m7D4 = 255;
    mAnmCount = 0;
    mShopCam.setCamDataIdx(0);
    mShopItems.setItemDataIdx(0);
    mShopCam.setCamAction(NULL);
    mShopItems.setItemSetDataList();
    mShopItems.createItem(3, fopAcM_GetRoomNo(this));
    eventInfo.setXyEventCB(daNpc_Ds1_XyEventCB);
    mEventCut.setActorInfo("Ds1", this);
    mEventCut.setJntCtrlPtr(&mJnt);
    mTalkMode = 4;
    mEventIdx[0] = dComIfGp_evmng_getEventIdx("CREATE_DRUG", 255);
    mEventIdx[1] = dComIfGp_evmng_getEventIdx("GET_DRUG", 255);
    mEventIdx[2] = dComIfGp_evmng_getEventIdx("PUT_ITEM", 255);
    mEventIdx[3] = dComIfGp_evmng_getEventIdx("PUT_ITEM_FAIL", 255);
    if (mRoomEffects) {
        mpRoomModel->setBaseTRMtx(mDoMtx_getIdentity());
        RoomEffectSet();
    }
    mLight.mPos.set(-60.0f, 123.0f, -555.0f);
    mLight.mColor.r = 0;
    mLight.mColor.g = 0;
    mLight.mColor.b = 0;
    mLight.mPower = 0.0f;
    mLight.mFluctuation = 20.0f;
    dKy_plight_set(&mLight);
    return TRUE;
}

/* 00002158-000021A8       .text setAttention__11daNpc_Ds1_cFb */
void daNpc_Ds1_c::setAttention(bool force) {
    if (force || mAttnSetCount < 2) {
        attention_info.position.set(mAttentionBasePos.x, mAttentionBasePos.y + l_HIO.mChild[mType].mNpc.mAttnYOffset, mAttentionBasePos.z);
    }
}

/* 000021A8-00002524       .text lookBack__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::lookBack() {
    cXyz target, origin(0.0f, 0.0f, 0.0f);
    cXyz* ptr = NULL;
    s16 angle = current.angle.y;
    switch (mLookMode) {
    case 1:
        if (mAttention) {
            target = dNpc_playerEyePos(l_HIO.mChild[mType].mNpc.m04);
            ptr = &target;
            origin = current.pos;
            origin.y = eyePos.y;
            setTexAnm(0);
        } else {
            ptr = NULL;
            setTexAnm(0);
        }
        break;
    case 2:
        if (mShopCam.checkCamAction(NULL)) {
            target = dNpc_playerEyePos(l_HIO.mChild[mType].mNpc.m04);
        } else if (mShopItems.mSelectedItemIdx == -1) {
            target = mShopCam.getItemZoomPos(100.0f);
        } else {
            cXyz base = mShopItems.getSelectItemBasePos();
            target = mShopItems.getSelectItemPos();
            mpShopCursor->setPos(base);
            mpShopCursor->setScale(l_HIO.mChild[mType].mCursorMin, l_HIO.mChild[mType].mCursorMax, l_HIO.mChild[mType].mCursorSpeed, l_HIO.mChild[mType].mCursorY, l_HIO.mChild[mType].mCursorZ);
            mpShopCursor->anm_play();
        }
        ptr = &target;
        origin = current.pos;
        origin.y = eyePos.y;
        break;
    }
    if (checkAction(&daNpc_Ds1_c::event_action)) {
        target = dNpc_playerEyePos(l_HIO.mChild[mType].mNpc.m04);
        ptr = &target;
        origin = current.pos;
        origin.y = eyePos.y;
    }
    if (mJnt.trnChk()) {
        cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mChild[mType].mNpc.mMaxHeadTurnVel, 4, 2048);
    } else {
        mTurnSpeed = 0;
    }
    mJnt.lookAtTarget(&current.angle.y, ptr, origin, angle, mTurnSpeed, true);
}

/* 00002524-00002578       .text wait01__11daNpc_Ds1_cFv */
bool daNpc_Ds1_c::wait01() {
    if (m761) {
        m895 = mLookMode;
        mLookMode = 2;
    } else if (mAttention) {
        mOrder = 2;
    } else if (mOrder == 2) {
        mOrder = 0;
    }
    return true;
}

/* 00002578-00002760       .text talk01__11daNpc_Ds1_cFv */
bool daNpc_Ds1_c::talk01() {
    u16 status = talk();
    if (status == 18) {
        daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
        mLookMode = m895;
        if (mCurrMsg != 0x1DCD && mCurrMsg != 0x1DCC) {
            dComIfGp_event_reset();
            if (mOrder != 3) {
                mShopCam.setCamAction(NULL);
            }
            player->offPlayerNoDraw();
        } else {
            setAction(&daNpc_Ds1_c::dummy_action, NULL);
        }
        m761 = 0;
        setAnm(0, -1.0f);
        if (mCurrMsg == 0x1DB4) {
            dComIfGs_onEventBit(0x1120);
        }
    } else if (status == 8 && daNpc_Ds1_shopMsgCheck(mCurrMsg) && mShopItems.getSelectItemBuyMsg() == mCurrMsg) {
        dComIfGp_setDoStatusForce(0x17);
        dComIfGp_setAStatusForce(0x27);
    }
    return mpMorf->isMorf();
}

/* 00002760-00002A04       .text getdemo_action__11daNpc_Ds1_cFPv */
int daNpc_Ds1_c::getdemo_action(void*) {
#if VERSION == VERSION_DEMO
    dEvent_manager_c* mgr = &g_dComIfG_gameInfo.play.getEvtManager();
    int staff = mgr->getMyStaffId("Ds1", NULL, 0);
#else
    int staff = dComIfGp_evmng_getMyStaffId("Ds1", NULL, 0);
    dEvent_manager_c* mgr = &g_dComIfG_gameInfo.play.getEvtManager();
#endif
    if (mActionState == 0) {
        u8 item;
        if (m7D4 != 255) {
            item = m7D4;
        } else if (mJellyItem == 0x49) {
            item = 0x51;
        } else if (mJellyItem == 0x4A) {
            item = 0x52;
        } else {
            item = 0x53;
        }
        ((daPy_py_c*)dComIfGp_getPlayer(0))->offPlayerNoDraw();
        mLookMode = m895;
        mShopCam.setCamAction(NULL);
        fpc_ProcID id = fopAcM_createItemForPresentDemo(&current.pos, item, 0, -1, DEMO_SELECT(-1, fopAcM_GetRoomNo(this)), NULL, NULL);
        if (id != fpcM_ERROR_PROCESS_ID_e) {
            dComIfGp_event_setItemPartnerId(id);
        }
        mgr->cutEnd(staff);
        mActionState++;
    } else if (mActionState != -1) {
        fopMsgM_demoMsgFlagOn();
        mgr->cutEnd(staff);
#if VERSION == VERSION_DEMO
        if (dComIfGp_evmng_endCheck(mEventIdx[1])) {
#else
        if (mgr->endCheck(mEventIdx[1])) {
#endif
            mOrder = 1;
            if (m7D4 != 255) {
                m7D0 = 0x1DDA;
                mShopItems.showItem();
                m7D4 = 255;
                mShopItems.mSelectedItemIdx = -1;
            } else if (daNpc_Ds1_checkCreateDrugChuchu(mJellyItem)) {
                m7D0 = 0x1DC6;
            } else {
                m7D0 = 0x1DCD;
            }
            dComIfGp_event_reset();
            if (mJellyItem == 0x4A) {
                dComIfGs_onEventBit(0xD04);
            } else if (mJellyItem == 0x4B) {
                dComIfGs_onEventBit(0xD02);
            }
            setAction(&daNpc_Ds1_c::wait_action, NULL);
        }
    }
    return 1;
}

// Unused; stripped by the linker. Name/signature from d_a_npc_ds1.map; body reconstructed from its leftover
// &daNpc_Ds1_c::wait_action pointer-to-member constant in .data. It runs the CREATE_DRUG event (mEventIdx[0]) the way
// getdemo_action runs GET_DRUG.
int daNpc_Ds1_c::createdemo_action(void*) {
    int staff = dComIfGp_evmng_getMyStaffId("Ds1", NULL, 0);
    dEvent_manager_c* mgr = &g_dComIfG_gameInfo.play.getEvtManager();
    if (mActionState == 0) {
        ((daPy_py_c*)dComIfGp_getPlayer(0))->offPlayerNoDraw();
        mLookMode = m895;
        mShopCam.setCamAction(NULL);
        mgr->cutEnd(staff);
        mActionState++;
    } else if (mActionState != -1) {
        mgr->cutEnd(staff);
        if (mgr->endCheck(mEventIdx[0])) {
            mOrder = 1;
            dComIfGp_event_reset();
            setAction(&daNpc_Ds1_c::wait_action, NULL);
        }
    }
    return 1;
}

/* 00002A04-00002BD0       .text privateCut__11daNpc_Ds1_cFv */
int daNpc_Ds1_c::privateCut() {
    int cut;
    char* name = mEventCut.getActorName();
    int staff = dComIfGp_evmng_getMyStaffId(name, NULL, 0);
    if (staff == -1) {
        return 0;
    }
    dEvent_manager_c* mgr = &g_dComIfG_gameInfo.play.getEvtManager();
    static const char* cut_name_tbl[] = {"TALKMSG", "CONTINUE_TALK", "SHOWITEM", "HEADSWING", "SOUND", "SETANM", "MOVEPOS", "INITPOS", "JNTLOCK", "PLAYER_HIDE"};
    cut = mgr->getMyActIdx(staff, cut_name_tbl, 10, 1, 0);
    if (cut == -1) {
        mgr->cutEnd(staff);
    } else {
        if (mgr->getIsAddvance(staff)) {
            switch (cut) {
            case 0:
                evn_talk_init(staff);
                break;
            case 1:
                evn_continue_talk_init(staff);
                break;
            case 2:
                evn_ItemModel_init(staff);
                break;
            case 3:
                evn_head_swing_init(staff);
                break;
            case 5:
                evn_setAnm_init(staff);
                break;
            case 6:
                evn_move_pos_init(staff);
                break;
            case 7:
                evn_init_pos_init(staff);
                break;
            case 8:
                evn_jnt_lock_init(staff);
                break;
            case 9:
                evn_player_hide_init(staff);
                break;
            }
        }
        int done;
        switch (cut) {
        case 0:
        case 1: done = evn_talk();
            break;
        case 5:
            done = evn_Anm();
            break;
        case 6:
            done = evn_move_pos();
            break;
        default:
            done = 1;
            break;
        }
        if (done) {
            mgr->cutEnd(staff);
        }
    }
    return 1;
}

/* 00002BD0-00002E00       .text evn_setAnm_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_setAnm_init(int staff) {
    int* anm = dComIfGp_evmng_getMyIntegerP(staff, "AnmNo");
    int* count = dComIfGp_evmng_getMyIntegerP(staff, "count");
    f32* morf = dComIfGp_evmng_getMyFloatP(staff, "hokan");
    if (anm) {
        f32 blend = -1.0f;
        if (morf) {
            blend = *morf;
        }
        setAnm(*anm, blend);
        if (count) {
            mAnmCount = *count;
        } else {
            mAnmCount = 0;
        }
    }
    if (mAnm == 6) {
        static GXColor ef_prmColor[] = {{188, 39, 64, 255}, {39, 188, 64, 255}, {39, 99, 188, 255}};
        static GXColor ef_envColor[] = {{222, 128, 56, 255}, {52, 222, 56, 255}, {56, 222, 128, 255}};
        int color;
        if (mJellyItem == 0x49) {
            color = 0;
        } else if (mJellyItem == 0x4A) {
            color = 1;
        } else {
            color = 2;
        }
        if (!mpEmitters[0]) {
            mpEmitters[0] = dComIfGp_particle_set(dPa_name::ID_AK_SN_CCDRUGSTORE00, &current.pos, NULL, NULL, 255, NULL, -1, &ef_prmColor[color], &ef_envColor[color]);
            mLight.mColor.r = ef_prmColor[color].r << 2;
            mLight.mColor.g = ef_prmColor[color].g << 2;
            mLight.mColor.b = ef_prmColor[color].b << 2;
        }
        if (!mpEmitters[1]) {
            mpEmitters[1] = dComIfGp_particle_set(dPa_name::ID_AK_SN_CCDRUGSTORE01, &current.pos, NULL, NULL, 255, NULL, -1, &ef_prmColor[color], &ef_envColor[color]);
        }
    }
    return 1;
}

/* 00002E00-000030A0       .text evn_Anm__11daNpc_Ds1_cFv */
int daNpc_Ds1_c::evn_Anm() {
    if (mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) {
        mAnmCount--;
    }
    if (mAnmCount <= 0) {
        return 1;
    }
    if (mAnm == 6) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_handL_jnt_num));
        cXyz pos;
        pos.x = mDoMtx_stack_c::get()[0][3];
        pos.y = mDoMtx_stack_c::get()[1][3];
        pos.z = mDoMtx_stack_c::get()[2][3];
        if (mpEmitters[0]) {
            mpEmitters[0]->setGlobalTranslation(pos);
        }
        if (mpEmitters[1]) {
            mpEmitters[1]->setGlobalTranslation(pos.x, pos.y, pos.z);
        }
        if (mpMorf->checkFrame(mpMorf->getEndFrame() - 1.0f)) {
            if (mpEmitters[0]) {
                mpEmitters[0]->becomeInvalidEmitter();
                mpEmitters[0] = NULL;
            }
            if (mpEmitters[1]) {
                mpEmitters[1]->becomeInvalidEmitter();
                mpEmitters[1] = NULL;
            }
        }
        if (mpMorf->checkFrame(135.0f)) {
            dComIfGp_getVibration().StartShock(5, -17, cXyz(0.0f, 1.0f, 0.0f));
            mDoAud_seStart(JA_SE_OBJ_CK_CHEMICAL_BOMB, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            mLight.mPower = 100.0f;
#if VERSION == VERSION_DEMO
        } else if (mpMorf->checkFrame(10.0f + REG10_F(7))) {
#else
        } else if (mpMorf->checkFrame(10.0f)) {
#endif
            mItemFlags |= 2;
        }
    } else if (mAnm == 8 && mpMorf->checkFrame(128.0f)) {
        mItemFlags |= 1;
    }
    return 0;
}

/* 000030A0-00003168       .text evn_talk_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_talk_init(int staff) {
    int* msg = dComIfGp_evmng_getMyIntegerP(staff, "MsgNo");
    int* end = dComIfGp_evmng_getMyIntegerP(staff, "EndMsgNo");
    l_msgId = fpcM_ERROR_PROCESS_ID_e;
    l_msg = NULL;
    if (msg) {
        mCurrMsg = *msg;
    } else {
        mCurrMsg = 0;
    }
    if (end) {
        m7CC = *end;
    } else {
        m7CC = 0;
    }
    return 1;
}

/* 00003168-000031D0       .text evn_continue_talk_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_continue_talk_init(int staff) {
    int* end = dComIfGp_evmng_getMyIntegerP(staff, "EndMsgNo");
    if (end) {
        m7CC = *end;
    } else {
        m7CC = 0;
    }
    return 1;
}

/* 000031D0-000033AC       .text evn_talk__11daNpc_Ds1_cFv */
int daNpc_Ds1_c::evn_talk() {
    if (l_msgId == fpcM_ERROR_PROCESS_ID_e) {
        l_msgId = fopMsgM_messageSet(mCurrMsg, &eyePos);
    } else if (!l_msg) {
        l_msg = fopMsgM_SearchByID(l_msgId);
        fopMsgM_demoMsgFlagOn();
    } else {
        setAnmFromMsgTag();
        if (l_msg->mStatus == 14) {
            l_msg->mStatus = next_msgStatus(&mCurrMsg);
            if (l_msg->mStatus == 15) {
                fopMsgM_messageSet(mCurrMsg);
            }
        } else if (l_msg->mStatus == 18) {
            if (mCurrMsg == 0x1DCA) {
                int index;
                s16 amount;
                if (mJellyItem == 0x49) {
                    index = 4;
                    amount = -10;
                } else if (mJellyItem == 0x4A) {
                    index = 5;
                    amount = DEMO_SELECT(-20, -15);
                } else {
                    index = 6;
                    amount = DEMO_SELECT(-20, -15);
                }
                dComIfGp_setItemBeastNumCount(index, amount);
            } else if (mCurrMsg == 0x1DC4) {
                int index;
                if (mJellyItem == 0x49) {
                    index = 4;
                } else if (mJellyItem == 0x4A) {
                    index = 5;
                } else {
                    index = 6;
                }
                dComIfGp_setItemBeastNumCount(index, DEMO_SELECT(-10, -5));
            }
            l_msg->mStatus = 19;
            l_msg = NULL;
            l_msgId = fpcM_ERROR_PROCESS_ID_e;
            return 1;
        } else if ((l_msg->mStatus == 2 || l_msg->mStatus == 6) && mCurrMsg == m7CC) {
            m7CC = 0;
            return 1;
        }
    }
    return 0;
}

/* 000033AC-00003478       .text evn_jnt_lock_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_jnt_lock_init(int staff) {
    int* value = dComIfGp_evmng_getMyIntegerP(staff, "prm");
    int prm = value ? *value : 0;
    switch (prm) {
    case 0:
        mJnt.offHeadLock();
        mJnt.offBackBoneLock();
        break;
    case 1:
        mJnt.onHeadLock();
        mJnt.offBackBoneLock();
        break;
    case 2:
        mJnt.offHeadLock();
        mJnt.onBackBoneLock();
        break;
    case 3:
        mJnt.onHeadLock();
        mJnt.onBackBoneLock();
        break;
    }
    return 1;
}

/* 00003478-0000350C       .text evn_player_hide_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_player_hide_init(int staff) {
    int* value = dComIfGp_evmng_getMyIntegerP(staff, "prm");
    int prm = value ? *value : 0;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    switch (prm) {
    case 0:
        player->offPlayerNoDraw();
        break;
    case 1:
        player->onPlayerNoDraw();
        break;
    }
    return 1;
}

/* 0000350C-000035B8       .text evn_head_swing_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_head_swing_init(int staff) {
    int* value = dComIfGp_evmng_getMyIntegerP(staff, "prm");
    int prm = value ? *value : 0;
    switch (prm) {
    case 0:
        mHeadAnm.swing_horizone_init(2, 4096, 4096, 1);
        break;
    case 1:
        mHeadAnm.swing_vertical_init(2, 4096, 2048, 1);
        break;
    }
    return 1;
}

/* 000035B8-0000361C       .text evn_ItemModel_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_ItemModel_init(int staff) {
    int* value = dComIfGp_evmng_getMyIntegerP(staff, "prm");
    mItemFlags = value ? *value : 0;
    return 1;
}

/* 0000361C-000036D4       .text evn_move_pos_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_move_pos_init(int staff) {
    cXyz* pos = dComIfGp_evmng_getMyXyzP(staff, "pos");
    int* angle = dComIfGp_evmng_getMyIntegerP(staff, "angle");
    if (pos) {
        mMovePos = *pos;
    } else {
        mMovePos = home.pos;
    }
    return 1;
}

/* 000036D4-000037F0       .text evn_move_pos__11daNpc_Ds1_cFv */
int daNpc_Ds1_c::evn_move_pos() {
    cLib_addCalcPos2(&current.pos, mMovePos, 0.25f, 5.0f);
    cXyz delta = mMovePos - current.pos;
    if (delta.abs() < 1.0f) {
        return 1;
    }
    mMoveAngle = cLib_targetAngleY(&current.pos, &mMovePos);
    cLib_addCalcAngleS2(&current.angle.y, mMoveAngle, 4, 4096);
    return 0;
}

/* 000037F0-000038F4       .text evn_init_pos_init__11daNpc_Ds1_cFi */
int daNpc_Ds1_c::evn_init_pos_init(int staff) {
    cXyz* pos = dComIfGp_evmng_getMyXyzP(staff, "pos");
    int* angle = dComIfGp_evmng_getMyIntegerP(staff, "angle");
    if (pos) {
        old.pos = *pos;
        current.pos = old.pos;
    } else {
        old.pos = home.pos;
        current.pos = old.pos;
    }
    if (angle) {
        current.angle.y = *angle;
    } else {
        current.angle.y = home.angle.y;
    }
    return 1;
}

/* 000038F4-00003AF8       .text event_action__11daNpc_Ds1_cFPv */
int daNpc_Ds1_c::event_action(void*) {
    if (mActionState == 0) {
        dComIfGp_evmng_getMyStaffId("Ds1", NULL, 0);
        ((daPy_py_c*)dComIfGp_getPlayer(0))->offPlayerNoDraw();
        mLookMode = m895;
        mShopCam.setCamAction(NULL);
        mOrder = 0;
        mActionState++;
    } else if (mActionState != -1) {
        privateCut();
        if (dComIfGp_evmng_endCheck(mEventIdx[mTalkMode])) {
            if (mTalkMode == 2) {
                mOrder = 4;
                mTalkMode = 0;
            } else if (mTalkMode == 3 && mCurrMsg == 0x1DC4) {
                mOrder = 3;
                mTalkMode = 4;
            } else if (mTalkMode == 0) {
                mOrder = 1;
                m7D0 = 0x1DCB;
                mTalkMode = 4;
            }
            dComIfGp_event_reset();
            setAction(&daNpc_Ds1_c::wait_action, NULL);
        }
        lookBack();
    }
    return 1;
}

/* 00003AF8-00003BF0       .text wait_action__11daNpc_Ds1_cFPv */
int daNpc_Ds1_c::wait_action(void*) {
    bool force;
    if (mActionState == 0) {
        mLookMode = 1;
        mActionState++;
    } else if (mActionState != -1) {
        s16 angle = current.angle.y + mJnt.getHead_y() + mJnt.getBackbone_y();
        mAttention = chkAttention(current.pos, angle);
        switch (mLookMode) {
        case 1:
            force = wait01();
            break;
        case 2:
            force = talk01();
            break;
        default:
            force = false;
            break;
        }
        lookBack();
        setAttention(force);
    }
    return 1;
}

/* 00003BF0-00003C0C       .text dummy_action__11daNpc_Ds1_cFPv */
int daNpc_Ds1_c::dummy_action(void*) {
    if (mActionState == 0) {
        mActionState++;
    }
    return 1;
}

/* 00003C0C-00003EE4       .text RoomEffectSet__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::RoomEffectSet() {
    cXyz pos[] = {cXyz::Zero, cXyz(-158.0f, 160.0f, -663.0f), cXyz(18.0f, 160.0f, -652.0f), cXyz(-220.0f, 105.0f, -590.0f)};
    GXColor colors[] = {{65, 77, 111, 255}, {144, 141, 86, 255}, {120, 70, 90, 255}};
    mpEmitters[2] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_LAMP00, &pos[0]);
    mpEmitters[3] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_SMOKEA00, &pos[0]);
    mpEmitters[4] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_SMOKEB00, &pos[0]);
    mpEmitters[5] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_SMOKEC00, &pos[0]);
    mpEmitters[6] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_FUTTOU00, &pos[1], NULL, NULL, 255, NULL, -1, &colors[0]);
    mpEmitters[7] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_FUTTOU00, &pos[2], NULL, NULL, 255, NULL, -1, &colors[1]);
    if (mpEmitters[7]) {
        mpEmitters[7]->setLifeTime(13);
    }
    mpEmitters[8] = dComIfGp_particle_set(dPa_name::ID_IT_SN_TYDS_FUTTOU00, &pos[3], NULL, NULL, 255, NULL, -1, &colors[2]);
}

/* 00003EE4-00003F20       .text RoomEffectDelete__11daNpc_Ds1_cFv */
void daNpc_Ds1_c::RoomEffectDelete() {
    int i = 0;
    while (i++ < 7) {
        if (mpEmitters[i + 2]) {
            mpEmitters[i + 2]->becomeInvalidEmitter();
        }
    }
}

/* 00003F20-000041D0       .text _draw__11daNpc_Ds1_cFv */
BOOL daNpc_Ds1_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* data = model->getModelData();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    mBtp.entry(data, mTexFrame);
    mpMorf->updateDL();
    mBtp.remove(data);
    if (mItemFlags&1) {
        g_env_light.setLightTevColorType(mpItemL, &tevStr);
        mpItemL->setBaseTRMtx(model->getAnmMtx(m_handL_jnt_num));
        mDoExt_modelUpdateDL(mpItemL);
    } else {
        g_env_light.setLightTevColorType(mpItemL, &tevStr);
#if VERSION == VERSION_DEMO
        mDoMtx_stack_c::transS(-90.0f + REG10_F(0), 95.0f + REG10_F(1), -580.0f + REG10_F(2));
#else
        mDoMtx_stack_c::transS(-90.0f, 95.0f, -580.0f);
#endif
        mDoMtx_stack_c::ZXYrotM(1900, 0, 0);
        mpItemL->setBaseTRMtx(mDoMtx_stack_c::get());
        mDoExt_modelUpdateDL(mpItemL);
    }
    if (mItemFlags&2) {
        g_env_light.setLightTevColorType(mpItemR, &tevStr);
        mpItemR->setBaseTRMtx(model->getAnmMtx(m_handR_jnt_num));
        mDoExt_modelUpdateDL(mpItemR);
    } else {
        g_env_light.setLightTevColorType(mpItemR, &tevStr);
#if VERSION == VERSION_DEMO
        mDoMtx_stack_c::transS(-11.0f + REG10_F(3), 101.0f + REG10_F(4), -545.0f + REG10_F(5));
        mDoMtx_stack_c::ZXYrotM(0x8000 + REG10_S(3), 15500 + REG10_S(4), REG10_S(5));
#else
        mDoMtx_stack_c::transS(-11.0f, 101.0f, -545.0f);
        mDoMtx_stack_c::ZXYrotM(-32768, 15500, 0);
#endif
        mpItemR->setBaseTRMtx(mDoMtx_stack_c::get());
        mDoExt_modelUpdateDL(mpItemR);
    }
    cXyz pos(current.pos.x, current.pos.y + 130.0f, current.pos.z);
    mShadow = dComIfGd_setShadow(mShadow, 1, mpMorf->getModel(), &pos, 800.0f, 20.0f, current.pos.y, mAcch.GetGroundH(), mAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    if (mShopItems.mSelectedItemIdx >= 0) {
        mpShopCursor->draw();
    }
    if (mRoomEffects) {
        J3DModelData* roomData = mpRoomModel->getModelData();
        mRoomBtk.entry(roomData, mRoomBtk.getFrame());
        mDoExt_modelUpdateDL(mpRoomModel);
    }
    dSnap_RegistFig(93, this, current.pos, current.angle.y, 1.0f, 1.0f, 1.0f);
    return TRUE;
}

/* 000041D0-00004554       .text _execute__11daNpc_Ds1_cFv */
BOOL daNpc_Ds1_c::_execute() {
    s16 maxHeadY, maxBackY, minHeadY, minBackY;
    if (!checkAction(&daNpc_Ds1_c::event_action)) {
        maxHeadY = l_HIO.mChild[mType].mNpc.mMaxHeadY;
        maxBackY = l_HIO.mChild[mType].mNpc.mMaxBackboneY;
        minHeadY = l_HIO.mChild[mType].mNpc.mMinHeadY;
        minBackY = l_HIO.mChild[mType].mNpc.mMinBackboneY;
    } else {
        maxHeadY = 14560;
        maxBackY = 7280;
        minHeadY = -7280;
        minBackY = -14560;
    }
    mJnt.setParam(l_HIO.mChild[mType].mNpc.mMaxBackboneX, maxBackY, l_HIO.mChild[mType].mNpc.mMinBackboneX, minBackY, l_HIO.mChild[mType].mNpc.mMaxHeadX, maxHeadY, l_HIO.mChild[mType].mNpc.mMinHeadX, minHeadY, l_HIO.mChild[mType].mNpc.mMaxTurnStep);
    playTexPatternAnm();
    s8 room = fopAcM_GetRoomNo(this);
    u32 mtrl = mAcch.ChkGroundHit() ? dComIfG_Bgsp()->GetMtrlSndId(mAcch.m_gnd) : 0;
    mAnmEnd = mpMorf->play(&eyePos, mtrl, dComIfGp_getReverb(room));
    if (mpMorf->getFrame() < mFrame) {
        mAnmEnd = 1;
    }
    mFrame = mpMorf->getFrame();
    checkOrder();
    (this->*mAction)(NULL);
    mShopCam.move();
    mShopItems.Item_Move();
    eventOrder();
    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    mAcch.CrrPos(*dComIfG_Bgsp());
    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mAcch.m_gnd);
    J3DModel* model = mpMorf->getModel();
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    setCollision();
    if (mRoomEffects) {
#if VERSION == VERSION_DEMO
        static cXyz se_pos[] = {cXyz(-158.0f, 160.0f, -663.0f), cXyz(18.0f, 160.0f, -652.0f), cXyz(-220.0f, 105.0f, -590.0f)};
        mDoAud_seStart(JA_SE_OBJ_CK_BOIL_L, &se_pos[0], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        mDoAud_seStart(JA_SE_OBJ_CK_BOIL_C, &se_pos[1], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        mDoAud_seStart(JA_SE_OBJ_CK_BOIL_R, &se_pos[2], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        if (mSoundTimer++ >= 50) {
            mDoAud_seStart(JA_SE_OBJ_CK_FLASK_STEAM, &cXyz(-225.0f, 260.0f, -590.0f), 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            mSoundTimer = 0;
        }
#else
        mDoAud_seStart(JA_SE_OBJ_CK_BOIL_L, &se_pos[0], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        mDoAud_seStart(JA_SE_OBJ_CK_BOIL_C, &se_pos[1], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        mDoAud_seStart(JA_SE_OBJ_CK_BOIL_R, &se_pos[2], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        if (mSoundTimer++ >= 50) {
            mDoAud_seStart(JA_SE_OBJ_CK_FLASK_STEAM, &se_pos[3], 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
            mSoundTimer = 0;
        }
#endif
        mRoomBtk.play();
    }
    cLib_addCalc0(&mLight.mPower, 0.25f, 20.0f);
    return TRUE;
}

/* 00004554-0000465C       .text _delete__11daNpc_Ds1_cFv */
BOOL daNpc_Ds1_c::_delete() {
    dKy_plight_cut(&mLight);
#if VERSION == VERSION_DEMO
    dComIfG_deleteObjectRes(m_arcname);
    if (mpMorf) {
        mpMorf->stopZelAnime();
    }
#else
    dComIfG_resDelete(&mPhase, m_arcname);
    if (heap && mpMorf) {
        mpMorf->stopZelAnime();
    }
    mDoAud_seDeleteObject(&se_pos[0]);
    mDoAud_seDeleteObject(&se_pos[1]);
    mDoAud_seDeleteObject(&se_pos[2]);
    mDoAud_seDeleteObject(&se_pos[3]);
#endif
    RoomEffectDelete();
    if (l_HIO.mSelected >= 0 && --l_HIO.mSelected < 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
    }
    return TRUE;
}

/* 0000465C-0000467C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daNpc_Ds1_c*)actor)->CreateHeap();
}

/* 0000467C-000047BC       .text _create__11daNpc_Ds1_cFv */
cPhs_State daNpc_Ds1_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daNpc_Ds1_c);
#endif
    cPhs_State phase = dComIfG_resLoad(&mPhase, m_arcname);
    if (phase == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_SetupActor(this, daNpc_Ds1_c);
#endif
        mType = (fopAcM_GetParam(this) >> 20)&15;
        switch ((u8)mType) {
        case 0:
            mType = 0;
            break;
        default:
            mType = 0;
            break;
        }
        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, 0x8340)) {
            return cPhs_ERROR_e;
        }
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        if (l_HIO.mSelected < 0) {
            l_HIO.mNo = mDoHIO_createChild("薬屋店員", &l_HIO);
        }
        l_HIO.mSelected++;
        if (!CreateInit()) {
            return cPhs_ERROR_e;
        }
    }
    return phase;
}

/* 00004E08-000052D4       .text CreateHeap__11daNpc_Ds1_cFv */
BOOL daNpc_Ds1_c::CreateHeap() {
    J3DModelData* data;
    data = (J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BDL_CK_e);
    mpMorf = new mDoExt_McaMorf(data, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BCK_WAIT01_e), 2, 1.0f, 0, -1, 1, NULL, 0, 0x11020203);
    if (!mpMorf || !mpMorf->getModel()) {
#if VERSION > VERSION_DEMO
        mpMorf = NULL;
#endif
        return FALSE;
    }
    m_head_jnt_num = data->getJointName()->getIndex("head");
    JUT_ASSERT(DEMO_SELECT(0x9CE, 0x9DF), m_head_jnt_num >= 0);
    m_backbone_jnt_num = data->getJointName()->getIndex("backbone");
    JUT_ASSERT(DEMO_SELECT(0x9D1, 0x9E2), m_backbone_jnt_num >= 0);
    m_handL_jnt_num = data->getJointName()->getIndex("handL");
    JUT_ASSERT(DEMO_SELECT(0x9D4, 0x9E5), m_handL_jnt_num >= 0);
    m_handR_jnt_num = data->getJointName()->getIndex("handR");
    JUT_ASSERT(DEMO_SELECT(0x9D6, 0x9E7), m_handR_jnt_num >= 0);
    switch ((u8)mType) {
    case 0:
        mTexAnm = 1;
        break;
    }
#if VERSION == VERSION_DEMO
    initTexPatternAnm(false);
#else
    if (!initTexPatternAnm(false)) {
        return FALSE;
    }
#endif
    {
        J3DModelData* model_data = static_cast<J3DModelData*>(dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BDL_GTYDS00_e));
        J3DAnmTextureSRTKey* btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BTK_GTYDS00_e);
        mpRoomModel = mDoExt_J3DModel__create(model_data, 0, 0x11020203);
        mRoomBtk.init(model_data, btk, 1, 2, 1.0f, 0, -1, false, 0);
    }
    mRoomEffects = 1;
    J3DModelData* left = (J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BDL_CK_FLASCO_e);
    J3DModelData* right = (J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BDL_CK_JAR_e);
    if (!left || !right) {
        return FALSE;
    }
    mpItemL = mDoExt_J3DModel__create(left, 0, 0x11020203);
    mpItemR = mDoExt_J3DModel__create(right, 0, 0x11020203);
    if (!mpItemL || !mpItemR) {
        return FALSE;
    }
    for (u16 i = 0;i < data->getJointNum();i++) if (i == m_head_jnt_num || i == m_backbone_jnt_num) mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Ds);
    mpMorf->getModel()->setUserArea((u32)this);
    mAcchCir.SetWall(30.0f, 0.0f);
    mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
    J3DAnmTevRegKey* brk = static_cast<J3DAnmTevRegKey*>(dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BRK_SHOP_CURSOR01_e));
    data = (J3DModelData*)dComIfG_getObjectRes(m_arcname, dRes_INDEX_DS_BMD_SHOP_CURSOR01_e);
    mpShopCursor = ShopCursor_create(data, brk, l_HIO.mChild[mType].mCursorMin);
    if (mpShopCursor) {
        return TRUE;
    }
    return FALSE;
}

/* 000052D4-000052F4       .text daNpc_Ds1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Ds1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Ds1_c*)i_this)->_create();
}

/* 000052F4-00005314       .text daNpc_Ds1_Delete__FP11daNpc_Ds1_c */
static BOOL daNpc_Ds1_Delete(daNpc_Ds1_c* i_this) {
    return ((daNpc_Ds1_c*)i_this)->_delete();
}

/* 00005314-00005334       .text daNpc_Ds1_Execute__FP11daNpc_Ds1_c */
static BOOL daNpc_Ds1_Execute(daNpc_Ds1_c* i_this) {
    return ((daNpc_Ds1_c*)i_this)->_execute();
}

/* 00005334-00005354       .text daNpc_Ds1_Draw__FP11daNpc_Ds1_c */
static BOOL daNpc_Ds1_Draw(daNpc_Ds1_c* i_this) {
    return ((daNpc_Ds1_c*)i_this)->_draw();
}

/* 00005354-0000535C       .text daNpc_Ds1_IsDelete__FP11daNpc_Ds1_c */
static BOOL daNpc_Ds1_IsDelete(daNpc_Ds1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Ds1_Method = {
    (process_method_func)daNpc_Ds1_Create, (process_method_func)daNpc_Ds1_Delete, (process_method_func)daNpc_Ds1_Execute, (process_method_func)daNpc_Ds1_IsDelete, (process_method_func)daNpc_Ds1_Draw, };

actor_process_profile_definition g_profile_NPC_DS1 = {
    /* Layer ID     */ fpcLy_CURRENT_e, /* List ID      */ 0x0007, /* List Prio    */ fpcPi_CURRENT_e, /* Proc Name    */ fpcNm_NPC_DS1_e, /* Proc SubMtd  */ &g_fpcLf_Method.base, /* Size         */ sizeof(daNpc_Ds1_c), /* Size Other   */ 0, /* Parameters   */ 0, /* Leaf SubMtd  */ &g_fopAc_Method.base, /* Draw Prio    */ fpcDwPi_NPC_DS1_e, /* Actor SubMtd */ &l_daNpc_Ds1_Method, /* Status       */ fopAcStts_UNK40000_e, /* Group        */ fopAc_ACTOR_e, /* Cull Type    */ fopAc_CULLBOX_12_e, };

daNpc_Ds1_HIO_c::~daNpc_Ds1_HIO_c() {
}
