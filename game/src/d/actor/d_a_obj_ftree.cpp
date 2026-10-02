/**
 * d_a_obj_ftree.cpp
 * Object - Forest Trees
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_ftree.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_cc_d.h"
#include "d/d_kankyo.h"
#include "d/d_a_obj.h"
#include "d/d_particle.h"
#include "d/d_particle_name.h"
#include "d/actor/d_a_player.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "SSystem/SComponent/c_math.h"
#include "SSystem/SComponent/c_lib.h"
#include "dolphin/mtx/vec.h"
#include "res/Object/Vmr.h"

namespace daObjFtree {
namespace {
struct Attr_c {
    /* 0x00 */ f32 m00;
    /* 0x04 */ f32 m04;
    /* 0x08 */ f32 m08;
    /* 0x0C */ f32 m0C;
    /* 0x10 */ f32 m10;
    /* 0x14 */ f32 m14;
    /* 0x18 */ f32 m18;
    /* 0x1C */ f32 m1C;
    /* 0x20 */ f32 m20;
    /* 0x24 */ s16 m24;
    /* 0x26 */ s16 m26;
    /* 0x28 */ f32 m28;
    /* 0x2C */ f32 m2C;
    /* 0x30 */ f32 m30;
    /* 0x34 */ f32 m34;
    /* 0x38 */ f32 m38;
    /* 0x3C */ s16 m3C;
    /* 0x3E */ s16 m3E;
    /* 0x40 */ f32 m40;
    /* 0x44 */ f32 m44;
    /* 0x48 */ f32 m48;
    /* 0x4C */ s16 m4C;
    /* 0x4E */ s16 m4E;
    /* 0x50 */ f32 m50;
    /* 0x54 */ f32 m54;
    /* 0x58 */ s16 m58;
    /* 0x5A */ s16 m5A;
    /* 0x5C */ s16 m5C;
    /* 0x5E */ s16 m5E;
};
const Attr_c L_attr = {
    61.0f, 96.0f, 25.0f, 52.0f, 132.0f, 950.0f, 72.0f, 37.0f, -37.0f,
    0x14, 0, 0.2f, 0.2f, 0.02f, 50.0f, 20.0f, 0x8C, 0, 130.0f, 1.75f, 30.0f,
    0x2328, 0, 860.0f, -2.1f, 0x2F, 0, 0x1E, 0,
};
}

static const dCcD_SrcCyl M_cyl_src = {
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ ~(AT_TYPE_WATER | AT_TYPE_UNK20000 | AT_TYPE_WIND | AT_TYPE_UNK400000 | AT_TYPE_LIGHT),
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_GrpAll_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, 0.0f, 0.0f}, 100.0f, 200.0f}},
};

static const dCcD_SrcCyl M_cyl_srcW = {
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_WATER,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_GrpAll_e,
        /* SrcObjCo  SPrm    */ 0,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ 0,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, 0.0f, 0.0f}, 100.0f, 200.0f}},
};

} // namespace daObjFtree

/* 00000078-000000DC       .text search_heart_part__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::search_heart_part() {
    if (mSpawnedHeartPieceProcessId != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor;
        if (!fopAcM_SearchByID(mSpawnedHeartPieceProcessId, &actor)) {
            dComIfGs_onEventBit(dSv_event_flag_c::UNK_2E20);
            mSpawnedHeartPieceProcessId = fpcM_ERROR_PROCESS_ID_e;
        }
    }
}

/* 000000DC-000001F0       .text launch_heart_part__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::launch_heart_part() {
    csXyz angle = shape_angle;
    cXyz scale(1.0f, 1.0f, 1.0f);
    cXyz pos = current.pos;
    angle.y += 0x2328;
    pos.y += 860.0f;
    fopAc_ac_c* item = (fopAc_ac_c*)fopAcM_fastCreateItem(
        &pos, dItemNo_HEART_PIECE_e, fopAcM_GetRoomNo(this), &angle, &scale, 1.75f, 30.0f, -2.1f, -1, NULL);
    if (item != NULL) {
        mSpawnedHeartPieceProcessId = fopAcM_GetID(item);
        dComIfGs_setEventReg(dSv_event_flag_c::UNK_9B07, param_get_tree_idx());
        item->actor_status = 0x4040;
        mHeartPlaced = 1;
        return TRUE;
    }
    return FALSE;
}

/* 000001F0-00000394       .text place_heart_part__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::place_heart_part() {
    if (mXyEventIdx == -1 && dComIfGs_isEventBit(dSv_event_flag_c::UNK_0102) == TRUE &&
        !dComIfGs_isEventBit(dSv_event_flag_c::UNK_2E20) &&
        param_get_tree_idx() == dComIfGs_getEventReg(dSv_event_flag_c::UNK_9B07))
    {
        cXyz offset(0.0f, 1.0f, 310.0f);
        s16 ang = shape_angle.y + 0x2328;
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::YrotM(ang);
        cXyz dest;
        Mtx mtx;
        mDoMtx_copy(mDoMtx_stack_c::get(), mtx);
        mDoMtx_multVec(mtx, &offset, &dest);
        cXyz scale(1.0f, 1.0f, 1.0f);
        csXyz angle(0, 0, 0);
        fopAc_ac_c* item = (fopAc_ac_c*)fopAcM_fastCreateItem(
            &dest, dItemNo_HEART_PIECE_e, fopAcM_GetRoomNo(this), &angle, &scale, 0.0f, 0.0f, -2.1f, -1, NULL);
        if (item != NULL) {
            mSpawnedHeartPieceProcessId = fopAcM_GetID(item);
            dComIfGs_setEventReg(dSv_event_flag_c::UNK_9B07, param_get_tree_idx());
            return TRUE;
        }
    }
    return FALSE;
}

/* 00000394-00000454       .text Ftree_get_water_pos__10daObjFtreeFP4cXyzP4cXyz */
void daObjFtree::Ftree_get_water_pos(cXyz* out, cXyz* src) {
    cXyz offset(0.0f, 0.0f, 50.0f);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    s16 angle = cM_atan2s(src->x - player->current.pos.x, src->z - player->current.pos.z);
    mDoMtx_stack_c::transS(player->current.pos);
    mDoMtx_stack_c::YrotM(angle);
    Mtx mtx;
    mDoMtx_copy(mDoMtx_stack_c::get(), mtx);
    mDoMtx_multVec(mtx, &offset, out);
}

/* 00000454-000004B8       .text estimate_water__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::estimate_water() {
    cXyz pos;
    Ftree_get_water_pos(&pos, &current.pos);
    f32 dist2 = PSVECSquareDistance(&pos, &current.pos);
    f32 r = 20.0f + mCrashRadius;
#if VERSION == VERSION_DEMO
    return dist2 <= r * r ? TRUE : FALSE;
#else
    return dist2 <= r * r;
#endif
}

/* 000004B8-000004DC       .text Ftree_checkXyEventCallBack__10daObjFtreeFPvi */
s16 daObjFtree::Ftree_checkXyEventCallBack(void*, int i_itemBtn) {
    return dComIfGp_getSelectItem(i_itemBtn) == dItemNo_FOREST_WATER_e ? TRUE : FALSE;
}

/* 000004DC-000004FC       .text Ftree_XyEventCallBack__10daObjFtreeFPvi */
s16 daObjFtree::Ftree_XyEventCallBack(void* i_this, int i_itemBtn) {
    return static_cast<Act_c*>(i_this)->XyEventCallBack(i_itemBtn);
}

/* 000004FC-000005E4       .text XyEventCallBack__Q210daObjFtree5Act_cFi */
s16 daObjFtree::Act_c::XyEventCallBack(int i_itemBtn) {
    if (dComIfGp_getSelectItem(i_itemBtn) == dItemNo_FOREST_WATER_e && !is_brought() && estimate_water()) {
        if (!iam_last()) {
            if (XyEvent_set(mEventCam1Idx)) {
                m652 = 0x8C;
                return mEventCam1Idx;
            }
        } else {
            if (XyEvent_set(mEventCam2Idx)) {
                dComIfGs_onEventBit(dSv_event_flag_c::UNK_0102);
                m652 = 0x8C;
                return mEventCam2Idx;
            }
        }
    }
    if (XyEvent_set(mEventCam0Idx)) {
        return mEventCam0Idx;
    }
    return -1;
}

/* 000005E4-000005F8       .text XyEvent_init__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::XyEvent_init() {
    mXyEventIdx = -1;
    mXyEventState = 0;
}

/* 000005F8-0000062C       .text XyEvent_set__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::XyEvent_set(s16 idx) {
    if (mXyEventState == 0 && idx != -1) {
        mXyEventIdx = idx;
        mXyEventState = 1;
        return TRUE;
    }
    return FALSE;
}

/* 0000062C-00000764       .text XyEvent_exe__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::XyEvent_exe() {
    switch (mXyEventState) {
    case 1:
        if (eventInfo.checkCommandTalk()) {
            mXyEventState = 2;
            daPy_py_c* player = daPy_getPlayerActorClass();
            cXyz offset(0.0f, 1.0f, 108.0f);
            s16 ang = shape_angle.y + 0x2328;
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::YrotM(ang);
            cXyz dest;
            Mtx mtx;
            mDoMtx_copy(mDoMtx_stack_c::get(), mtx);
            mDoMtx_multVec(mtx, &offset, &dest);
            player->setPlayerPosAndAngle(&dest, (s16)(ang + 0x8000));
        }
        break;
    case 2:
        if (dComIfGp_evmng_endCheck(mXyEventIdx)) {
            dComIfGp_event_reset();
            XyEvent_init();
        }
        break;
    }
}

/* 00000764-000007A8       .text param_get_tree_idx__Q210daObjFtree5Act_cCFv */
namespace daObjFtree {
const u8 ret_tree_no[10] = {0xF, 0, 1, 2, 3, 4, 5, 0xF, 6, 7};
u32 message_table[] = {
    0, 0x149F, 0x14A0, 0x14A1, 0, 0, 0, 0, 0, 0, 0x149F, 0x14A0, 0x14A1,
};
} // namespace daObjFtree

inline int daObjFtree::Act_c::param_get_tree_idx() const {
    static const u8 ret_num = 10;
    int idx = daObj::PrmAbstract(this, 4, 0);
    if (idx < ret_num) {
        return ret_tree_no[idx];
    }
    return 0xF;
}

/* 000007A8-000008D8       .text SetJointAnimation__Q210daObjFtree5Act_cFiffi */
BOOL daObjFtree::Act_c::SetJointAnimation(int resIdx, float speed, float morf, int param) {
    J3DAnmTransform* anm = (J3DAnmTransform*)dComIfG_getObjectRes(M_arcname, resIdx);
    param = param > 0 ? 0 : param;
    if (anm != NULL) {
        if (param == 0) {
            mpMorf->setAnm(anm, 0, morf, speed, 0.0f, -1.0f, NULL);
        } else {
            mpMorf->setAnm(anm, 0, morf, speed, 0.0f, param + (f32)anm->getFrameMax(), NULL);
        }
        return TRUE;
    }
    return FALSE;
}

/* 000008D8-00000910       .text PlayStopJointAnimation__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::PlayStopJointAnimation() {
    return mpMorf->play(NULL, 0, 0) ? TRUE : FALSE;
}

/* 00000910-0000093C       .text PlayStopColorAnimation__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::PlayStopColorAnimation() {
    return mBrkAnm.play() ? TRUE : FALSE;
}

/* 0000093C-00000A1C       .text set_first_stat__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::set_first_stat() {
    if (dComIfGs_isSymbol(2)) {
        if (dComIfGs_isEventBit(dSv_event_flag_c::UNK_0102)) {
            process_init(3, 0);
            mScaleMul = 1.0f;
        } else if (!is_brought()) {
            process_init(1, 0);
            mScaleMul = 0.2f;
        } else {
            process_init(2, 0);
            mScaleMul = 0.2f;
        }
    } else {
        process_init(0, 0);
    }
}

/* 00000A1C-00000C1C       .text set_collision__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::set_collision() {
    if (mModelS == 1 || mModelL == 1) {
        if (mCyl.ChkTgHit()) {
            if (mModelS == 1) {
                daObj::HitSeStart(&current.pos, fopAcM_GetRoomNo(this), &mCyl, 8);
            } else {
                daObj::HitSeStart(&current.pos, fopAcM_GetRoomNo(this), &mCyl, 7);
                daObj::HitEff_kikuzu(this, &mCyl);
            }
            dKy_Sound_set(current.pos, 4, fopAcM_GetID(this), 100);
            mCyl.ClrTgHit();
        } else if (mModelS == 1) {
            cXyz pos = current.pos;
            pos.y -= 50.0f;
            mCylW.SetR(61.0f);
            mCylW.SetH(96.0f);
            mCylW.SetC(pos);
            dComIfG_Ccsp()->Set(&mCylW);
            mCrashRadius = 61.0f;
            mCyl.SetR(61.0f);
            mCyl.SetH(96.0f);
            mCyl.SetC(current.pos);
            dComIfG_Ccsp()->Set(&mCyl);
        } else if (mModelL == 1) {
            mCrashRadius = 132.0f * mScaleMul;
            mCyl.SetR(mCrashRadius);
            mCyl.SetH(950.0f * mScaleMul);
            mCyl.SetC(current.pos);
            dComIfG_Ccsp()->Set(&mCyl);
        }
    }
}

/* 00000C1C-00000D50       .text talk_ct__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::talk_ct() {
    mMsgID = fpcM_ERROR_PROCESS_ID_e;
    mpMsg = NULL;
    cXyz attn = current.pos;
    cXyz eye = current.pos;
    if (mMode >= 0 && mMode < 13) {
        switch (message_table[mMode]) {
        case 0x149F:
            eye.y += 80.0f;
            attn.y += 105.0f;
            attention_info.flags = fopAc_Attn_TALKFLAG_LOOK_e | fopAc_Attn_LOCKON_TALK_e | fopAc_Attn_ACTION_SPEAK_e;
            break;
        case 0x14A0:
            eye.y += 90.0f;
            attn.y += 195.0f;
            attention_info.flags = fopAc_Attn_TALKFLAG_LOOK_e | fopAc_Attn_LOCKON_TALK_e | fopAc_Attn_ACTION_SPEAK_e;
            break;
        case 0x14A1:
            eye.y += 250.0f;
            attn.y += 200.0f;
            attention_info.flags = fopAc_Attn_TALKFLAG_LOOK_e | fopAc_Attn_ACTION_SPEAK_e;
            break;
        }
    }
    attention_info.position = attn;
    eyePos = eye;
}

/* 00000D50-00000F14       .text talk_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::talk_main() {
    if (mXyEventIdx == -1 && eventInfo.checkCommandTalk()) {
        if (mMsgID == fpcM_ERROR_PROCESS_ID_e) {
            mMsgID = fopMsgM_messageSet(message_table[mMode], this);
            mpMsg = NULL;
        } else if (mpMsg == NULL) {
            mpMsg = fopMsgM_SearchByID(mMsgID);
        } else {
            switch (mpMsg->mStatus) {
            case fopMsgStts_MSG_DISPLAYED_e:
                mpMsg->mStatus = fopMsgStts_MSG_ENDS_e;
                break;
            case fopMsgStts_BOX_CLOSED_e:
                mpMsg->mStatus = fopMsgStts_MSG_DESTROYED_e;
                dComIfGp_event_onEventFlag(8);
                talk_ct();
                break;
            }
        }
    } else if (mMode >= 0 && mMode < 13 && message_table[mMode] != 0) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        f32 dist2 = PSVECSquareDistance(&current.pos, &player->current.pos);
        f32 r = 130.0f + mCrashRadius;
        if (dist2 <= r * r) {
            talk_ct();
            player = dComIfGp_getPlayer(0);
            if (player != NULL) {
                cXyz diff = current.pos - player->current.pos;
                s16 ang = cM_atan2s(diff.x, diff.z);
                s16 d = ang - player->shape_angle.y;
                int abs_d = d <= 0 ? -d : d;
                if (abs_d <= 0x4000) {
                    eventInfo.onCondition(dEvtCnd_CANTALK_e);
                }
            }
            eventInfo.onCondition(dEvtCnd_CANTALKITEM_e);
        }
    }
}

/* 00000F14-00000FC4       .text get_tev_material0_color__Q210daObjFtree5Act_cFP12J3DModelDataUlPsPsPs */
void daObjFtree::Act_c::get_tev_material0_color(J3DModelData* mdl, unsigned long idx, s16* r, s16* g, s16* b) {
    J3DMaterial* mat = mdl->getJointNodePointer(0)->getMesh();
    if (mat != NULL) {
        *r = mat->getTevColor(idx)->mColor.r;
        *g = mat->getTevColor(idx)->mColor.g;
        *b = mat->getTevColor(idx)->mColor.b;
    }
}

/* 00000FC4-00001070       .text set_tev_color__Q210daObjFtree5Act_cFP12J3DModelDataUlsss */
void daObjFtree::Act_c::set_tev_color(J3DModelData* mdl, unsigned long idx, s16 r, s16 g, s16 b) {
    J3DMaterial* mat = mdl->getJointNodePointer(0)->getMesh();
    while (mat != NULL) {
        mat->getTevColor(idx)->mColor.r = r;
        mat->getTevColor(idx)->mColor.g = g;
        mat->getTevColor(idx)->mColor.b = b;
        mat = mat->getNext();
    }
}

/* 00001070-000010F0       .text is_broughtID__10daObjFtreeFi */
BOOL daObjFtree::is_broughtID(int id) {
    int bit = id & 7;
    bool ret = ((dComIfGs_getEventReg(dSv_event_flag_c::UNK_9EFF) >> bit) & 1) ||
               dComIfGs_isEventBit(dSv_event_flag_c::UNK_0102);
    return ret;
}

/* 000010F0-00001138       .text is_brought__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::is_brought() {
    return is_broughtID(param_get_tree_idx());
}

/* 00001138-000011FC       .text set_broughtID__Q210daObjFtree5Act_cFi */
void daObjFtree::Act_c::set_broughtID(int id) {
#if VERSION == VERSION_DEMO
    int bit = id & 7;
    u8 val = dComIfGs_getEventReg(dSv_event_flag_c::UNK_9EFF);
    val |= (1 << bit);
    dComIfGs_setEventReg(dSv_event_flag_c::UNK_9EFF, val);
    val = dComIfGs_getEventReg(dSv_event_flag_c::UNK_9AFF);
    val |= (1 << bit);
#else
    u8 val = dComIfGs_getEventReg(dSv_event_flag_c::UNK_9EFF);
    val |= (1 << (id & 7));
    dComIfGs_setEventReg(dSv_event_flag_c::UNK_9EFF, val);
    val = dComIfGs_getEventReg(dSv_event_flag_c::UNK_9AFF);
    val |= (1 << (id & 7));
#endif
    dComIfGs_setEventReg(dSv_event_flag_c::UNK_9AFF, val);
    _ftree_seach_info_ info;
    get_ftree_info(&info);
    if (info.mBrought == info.mTotal) {
        dComIfGs_onEventBit(dSv_event_flag_c::UNK_0102);
    }
}

/* 000011FC-00001260       .text set_brought__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::set_brought() {
    int no = param_get_tree_idx();
    set_broughtID(no);
    mBroughtSession = 1;
}

/* 00001260-000012D0       .text unset_broughtID__Q210daObjFtree5Act_cFi */
void daObjFtree::Act_c::unset_broughtID(int id) {
    int bit = id & 7;
    u8 val = dComIfGs_getEventReg(dSv_event_flag_c::UNK_9EFF);
    val &= ~(1 << bit);
    dComIfGs_setEventReg(dSv_event_flag_c::UNK_9EFF, val);
}

/* 000012D0-00001334       .text unset_brought__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::unset_brought() {
    int no = param_get_tree_idx();
    unset_broughtID(no);
    mBroughtSession = 0;
}

/* 00001334-000013A0       .text get_ftree_info__Q210daObjFtree5Act_cFPQ210daObjFtree18_ftree_seach_info_ */
void daObjFtree::Act_c::get_ftree_info(_ftree_seach_info_* info) {
    info->mTotal = 8;
    info->mBrought = 0;
    for (unsigned i = 0; i < 8; i++) {
        if (is_broughtID(i)) {
            info->mBrought++;
        }
    }
}

/* 000013A0-00001400       .text iam_last__Q210daObjFtree5Act_cFv */
BOOL daObjFtree::Act_c::iam_last() {
    _ftree_seach_info_ info;
    get_ftree_info(&info);
    if (!is_brought() && info.mBrought >= info.mTotal - 1) {
        return TRUE;
    }
    return FALSE;
}

/* 00001400-00001414       .text action_none_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_none_init(s16) {
    mModelS = 0;
    mModelL = 0;
    return TRUE;
}

/* 00001414-00001418       .text action_none_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_none_main() {}

/* 00001418-000014E8       .text action_waitS_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_waitS_init(s16) {
    mModelS = 1;
    mModelL = 0;
    unset_brought();
    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BRK_VMRMZ_e);
    if (brk != NULL) {
        if (mBrkAnm.init(mpMorf->getModel()->getModelData(), brk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0)) {
            return SetJointAnimation(dRes_INDEX_VMR_BCK_VMRTY_e, 1.0f, 0.0f, 0);
        }
    }
    return FALSE;
}

/* 000014E8-00001608       .text action_waitS_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_waitS_main() {
    if (m652 == 1) {
        BOOL ok;
        if (mXyEventIdx == mEventCam2Idx) {
            ok = process_init(4, 0);
        } else {
            ok = process_init(8, 0);
        }
        if (ok) {
            m652 = 0;
            return;
        }
    }
    BOOL coHit = mCyl.ChkCoHit();
    if (mPrevCoHit == 0 && coHit == TRUE && process_init(10, 1)) {
        coHit = FALSE;
    } else if ((int)cM_rndF(100.0f) == 0 && mMode == 1) {
        process_init(10, (s16)(1.0f + cM_rndF(1.0f) * 4.0f));
    }
    mPrevCoHit = coHit;
}

/* 00001608-0000173C       .text action_waitM_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_waitM_init(s16) {
    set_brought();
    mModelS = 1;
    mModelL = 0;
    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BRK_VMRMZ_e);
    if (brk != NULL) {
        if (mBrkAnm.init(mpMorf->getModel()->getModelData(), brk, TRUE, J3DFrameCtrl::EMode_NONE, -1.0f, 0, -1, true, 0)) {
            if (SetJointAnimation(dRes_INDEX_VMR_BCK_VMRMZ_e, -1.0f, 0.0f, 0)) {
                mpMorf->setFrame(mpMorf->getEndFrame() - 1.0f);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* 0000173C-00001878       .text action_waitM_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_waitM_main() {
    if (dComIfGs_getFwaterTimer() == 0) {
        _ftree_seach_info_ info;
        get_ftree_info(&info);
        if (info.mTotal != info.mBrought) {
            process_init(9, 0);
            return;
        }
    }
    if (mMode == 2) {
        BOOL coHit = mCyl.ChkCoHit();
        if (dKy_rain_check()) {
            process_init(11, -1);
        } else if (mPrevCoHit == 0 && coHit == TRUE && process_init(11, 0x78)) {
            coHit = FALSE;
        } else if ((int)cM_rndF(90.0f) == 0) {
            process_init(11, (s16)(30.0f + cM_rndF(1.0f) * 90.0f));
        }
        mPrevCoHit = coHit;
    }
}

/* 00001878-000018AC       .text action_waitL_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_waitL_init(s16) {
    mModelL = 1;
    mModelS = 0;
    set_brought();
    return TRUE;
}

/* 000018AC-000019BC       .text action_waitL_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_waitL_main() {
    if (mMode == 3) {
        if (dKy_rain_check()) {
            process_init(12, -1);
        } else if ((int)cM_rndF(30.0f) == 0) {
            process_init(12, (s16)(100.0f + cM_rndF(1.0f) * 80.0f));
        } else if (mXyEventIdx != -1) {
            process_init(12, -1);
        }
    } else if (mMode == 12) {
        if (!dKy_rain_check() && m69A == -1 && mXyEventIdx == -1) {
            process_init(3, 0);
        }
    }
}

/* 000019BC-00001A4C       .text action_pikupikuS_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_pikupikuS_init(s16 param) {
    param = param > 0 ? param : 1;
    mModelS = 1;
    mModelL = 0;
    mPikuSMax = param;
    mPikuSCount = 0;
    f32 speed = 0.7f + cM_rndF(1.0f) * 0.50000005f;
    return SetJointAnimation(dRes_INDEX_VMR_BCK_VMRTY_e, speed, 10.0f, 0);
}

/* 00001A4C-00001AF4       .text action_pikupikuS_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_pikupikuS_main() {
    if (PlayStopJointAnimation()) {
        if (mPikuSCount < mPikuSMax) {
            f32 speed = 0.7f + cM_rndF(1.0f) * 0.3f;
            SetJointAnimation(dRes_INDEX_VMR_BCK_VMRTY_e, speed, 10.0f, 0);
        } else {
            process_init(1, 0);
        }
        mPikuSCount++;
    }
    action_waitS_main();
}

/* 00001AF4-00001B4C       .text action_pikupikuM_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_pikupikuM_init(s16 param) {
    set_brought();
    m690 = param;
    m692 = 0;
    m694 = 0;
    mModelS = 1;
    mModelL = 0;
    return TRUE;
}

/* 00001B4C-00001C44       .text action_pikupikuM_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_pikupikuM_main() {
    if (m692 < m690 || m690 == -1) {
        mJointTargetMX = (s16)(100.0f * cM_ssin(m694));
        mJointTargetMY = (s16)(300.0f * cM_ssin(m694));
        m694 += 0xFA0;
    } else if (m692 >= m690) {
        process_init(2, 0);
    }
    m692++;
    action_waitM_main();
}

/* 00001C44-00001C9C       .text action_pikupikuL_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_pikupikuL_init(s16 param) {
    set_brought();
    m69A = param;
    m69C = 0;
    m69E = 0;
    mModelS = 0;
    mModelL = 1;
    return TRUE;
}

/* 00001C9C-00001D94       .text action_pikupikuL_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_pikupikuL_main() {
    if (m69C < m69A || m69A == -1) {
        mJointTargetLY = (s16)(30.0f * cM_ssin(m69E));
        mJointTargetLZ = (s16)(36.0f * cM_ssin(m69E));
        m69E += 0xBB8;
    } else if (m69C >= m69A) {
        process_init(3, 0);
    }
    m69C++;
    action_waitL_main();
}

/* 00001D94-00001EAC       .text action_changeSL_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_changeSL_init(s16) {
    mModelS = 1;
    mModelL = 0;
    J3DAnmTevRegKey* brk = static_cast<J3DAnmTevRegKey*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BRK_VMRMZ_e));
    if (brk != NULL) {
        if (mBrkAnm.init(mpMorf->getModel()->getModelData(), brk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0)) {
            if (SetJointAnimation(dRes_INDEX_VMR_BCK_VMRMZ_e, 1.0f, 10.0f, 0)) {
                set_brought();
                mDoAud_seStart(JA_SE_OBJ_DK_TREE_RECOVER, &current.pos);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* 00001EAC-00001F0C       .text action_changeSL_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_changeSL_main() {
    BOOL joint = PlayStopJointAnimation();
    BOOL color = PlayStopColorAnimation();
    if (joint == TRUE && color == TRUE) {
        process_init(5, 0);
    }
}

/* 00001F0C-00001FB4       .text action_changeSL2_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_changeSL2_init(s16) {
    mModelS = 1;
    mModelL = 0;
    mEffectFlag = 1;
    m640 = 0.0f;
    mGrowTimer = 0x1E;
    mScaleMul = 0.2f;
    get_tev_material0_color(mpMorf->getModel()->getModelData(), 2, &mColorSWork.r, &mColorSWork.g, &mColorSWork.b);
    mColorSBase = mColorSWork;
    m69E = 0;
    return TRUE;
}

/* 00001FB4-00002300       .text action_changeSL2_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_changeSL2_main() {
    if (mModelS != 0) {
        cLib_addCalc(&m640, 1.0f, 0.2f, 1.0f, 0.007f);
        mColorSBase.r = (s16)((f32)mColorSWork.r + (255.0f - (f32)mColorSWork.r) * m640);
        mColorSBase.g = (s16)((f32)mColorSWork.g + (255.0f - (f32)mColorSWork.g) * m640);
        mColorSBase.b = (s16)((f32)mColorSWork.b + (255.0f - (f32)mColorSWork.b) * m640);
        if (m640 == 1.0f) {
            if (mGrowTimer > 0) {
                mGrowTimer--;
            }
            if (mGrowTimer == 0) {
                mModelS = 0;
                mModelL = 1;
            }
        }
    }
    if (mModelL != 0) {
        f32 ratio = mScaleMul <= 0.2f ? 0.0f : (mScaleMul >= 1.0f ? 1.0f : (mScaleMul - 0.2f) / 0.8f);
        cLib_addCalc(&mScaleMul, 1.0f, 0.02f, 1.0f, 0.005f);
        mColorLBase.r = (s16)(255.0f + ((f32)mColorL.r - 255.0f) * ratio);
        mColorLBase.g = (s16)(255.0f + ((f32)mColorL.g - 255.0f) * ratio);
        mColorLBase.b = (s16)(255.0f + ((f32)mColorL.b - 255.0f) * ratio);
        if (mScaleMul == 1.0f) {
            if (process_init(3, 0)) {
                mLaunchHeart = 0x2F;
            }
        }
        mJointTargetLY = (s16)(30.0f * cM_ssin(m69E));
        mJointTargetLZ = (s16)(36.0f * cM_ssin(m69E));
        m69E += 0xBB8;
    }
}

/* 00002300-00002344       .text action_changeLS_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_changeLS_init(s16) {
    mModelS = 0;
    mModelL = 1;
    m640 = 1.0f;
    mScaleMul = 1.0f;
    unset_brought();
    return TRUE;
}

/* 00002344-000025DC       .text action_changeLS_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_changeLS_main() {
    if (mModelL != 0) {
        f32 ratio = mScaleMul <= 0.2f ? 0.0f : (mScaleMul >= 1.0f ? 1.0f : (mScaleMul - 0.2f) / 0.8f);
        cLib_addCalc(&mScaleMul, 0.2f, 0.02f, 1.0f, 0.005f);
        mColorLBase.r = (s16)(255.0f + ((f32)mColorL.r - 255.0f) * ratio);
        mColorLBase.g = (s16)(255.0f + ((f32)mColorL.g - 255.0f) * ratio);
        mColorLBase.b = (s16)(255.0f + ((f32)mColorL.b - 255.0f) * ratio);
        if (mScaleMul == 0.2f) {
            mModelS = 1;
            mModelL = 0;
        }
    }
    if (mModelS != 0) {
        cLib_addCalc(&m640, 0.0f, 0.2f, 1.0f, 0.007f);
        mColorSBase.r = (s16)((f32)mColorSWork.r + (255.0f - (f32)mColorSWork.r) * m640);
        mColorSBase.g = (s16)((f32)mColorSWork.g + (255.0f - (f32)mColorSWork.g) * m640);
        mColorSBase.b = (s16)((f32)mColorSWork.b + (255.0f - (f32)mColorSWork.b) * m640);
        if (m640 == 0.0f) {
            process_init(7, 0);
        }
    }
}

/* 000025DC-000026AC       .text action_changeLS2_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_changeLS2_init(s16) {
    mModelS = 1;
    mModelL = 0;
    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BRK_VMRMZ_e);
    if (brk != NULL) {
        if (mBrkAnm.init(mpMorf->getModel()->getModelData(), brk, TRUE, J3DFrameCtrl::EMode_NONE, -1.0f, 0, -1, true, 0)) {
            return SetJointAnimation(dRes_INDEX_VMR_BCK_VMRMZ_e, -0.7f, 10.0f, -0x14);
        }
    }
    return FALSE;
}

/* 000026AC-0000270C       .text action_changeLS2_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_changeLS2_main() {
    BOOL joint = PlayStopJointAnimation();
    BOOL color = PlayStopColorAnimation();
    if (joint == TRUE && color == TRUE) {
        process_init(1, 0);
    }
}

/* 0000270C-00002794       .text action_changeSM_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_changeSM_init(s16) {
    set_brought();
    if (action_changeSL_init(0) == TRUE) {
        mDoAud_seStart(JA_SE_OBJ_DK_TREE_RECOVER, &current.pos);
        return TRUE;
    }
    return FALSE;
}

/* 00002794-000027F4       .text action_changeSM_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_changeSM_main() {
    BOOL joint = PlayStopJointAnimation();
    BOOL color = PlayStopColorAnimation();
    if (joint == TRUE && color == TRUE) {
        process_init(2, 0);
    }
}

/* 000027F4-00002840       .text action_changeMS_init__Q210daObjFtree5Act_cFs */
BOOL daObjFtree::Act_c::action_changeMS_init(s16) {
    if (action_changeLS2_init(0)) {
        unset_brought();
        return TRUE;
    }
    return FALSE;
}

/* 00002840-00002860       .text action_changeMS_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::action_changeMS_main() {
    action_changeLS2_main();
}

/* 00002860-00002A4C       .text process_init__Q210daObjFtree5Act_cFis */
BOOL daObjFtree::Act_c::process_init(int proc, s16 param) {
    typedef BOOL (Act_c::*InitFunc)(s16);
    static InitFunc init_table[] = {
        &Act_c::action_none_init,
        &Act_c::action_waitS_init,
        &Act_c::action_waitM_init,
        &Act_c::action_waitL_init,
        &Act_c::action_changeSL_init,
        &Act_c::action_changeSL2_init,
        &Act_c::action_changeLS_init,
        &Act_c::action_changeLS2_init,
        &Act_c::action_changeSM_init,
        &Act_c::action_changeMS_init,
        &Act_c::action_pikupikuS_init,
        &Act_c::action_pikupikuM_init,
        &Act_c::action_pikupikuL_init,
    };
    if (proc >= 0 && proc < 13 && (this->*init_table[proc])(param)) {
        mJointTargetMX = 0;
        mJointTargetMY = 0;
        mJointTargetLY = 0;
        mJointTargetLZ = 0;
        mMode = proc;
        return TRUE;
    }
    return FALSE;
}

/* 00002A4C-00002BF0       .text process_main__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::process_main() {
    typedef void (Act_c::*MainFunc)();
    static MainFunc main_table[] = {
        &Act_c::action_none_main,
        &Act_c::action_waitS_main,
        &Act_c::action_waitM_main,
        &Act_c::action_waitL_main,
        &Act_c::action_changeSL_main,
        &Act_c::action_changeSL2_main,
        &Act_c::action_changeLS_main,
        &Act_c::action_changeLS2_main,
        &Act_c::action_changeSM_main,
        &Act_c::action_changeMS_main,
        &Act_c::action_pikupikuS_main,
        &Act_c::action_pikupikuM_main,
        &Act_c::action_pikupikuL_main,
    };
    if (mMode >= 0 && mMode < 13) {
        (this->*main_table[mMode])();
    }
}

/* 00002BF0-00002C14       .text solidHeapCB__Q210daObjFtree5Act_cFP10fopAc_ac_c */
BOOL daObjFtree::Act_c::solidHeapCB(fopAc_ac_c* i_this) {
    return static_cast<Act_c*>(i_this)->create_heap();
}

/* 00002C14-00002D94       .text NodeCallBack_Effect__Q210daObjFtree5Act_cFP7J3DNodei */
BOOL daObjFtree::Act_c::NodeCallBack_Effect(J3DNode* node, int timing) {
    if (mEffectFlag == 1 && timing == 0) {
        J3DJoint* joint = (J3DJoint*)node;
        cXyz offset(0.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::copy(j3dSys.getModel()->getAnmMtx(joint->getJntNo()));
        Mtx mtx;
        mDoMtx_copy(mDoMtx_stack_c::get(), mtx);
        cXyz pos;
        mDoMtx_multVec(mtx, &offset, &pos);
        mDoAud_seStart(JA_SE_OBJ_DK_TREE_GROW_UP, &current.pos);
#if VERSION == VERSION_DEMO
        if (mpEmitter1 == NULL) {
            mpEmitter1 = dComIfGp_particle_set(dPa_name::ID_AK_SN_HANASAKAFLASH00, &pos);
        }
        if (mpEmitter2 == NULL) {
            mpEmitter2 = dComIfGp_particle_set(dPa_name::ID_AK_SN_HANASAKAFLASH01, &pos);
        }
        if (mpEmitter1 != NULL && mpEmitter2 != NULL) {
            mEffectFlag = 0;
        }
#else
        JPABaseEmitter* e1 = dComIfGp_particle_set(dPa_name::ID_AK_SN_HANASAKAFLASH00, &pos);
        JPABaseEmitter* e2 = dComIfGp_particle_set(dPa_name::ID_AK_SN_HANASAKAFLASH01, &pos);
        if (e1 != NULL && e2 != NULL) {
            mEffectFlag = 0;
        }
#endif
    }
    return TRUE;
}

/* 00002D94-00002DDC       .text Ftree_NodeCallBack_Effect__10daObjFtreeFP7J3DNodei */
BOOL daObjFtree::Ftree_NodeCallBack_Effect(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        Act_c* i_this = (Act_c*)model->getUserArea();
        return i_this->NodeCallBack_Effect(node, timing);
    }
    return TRUE;
}

/* 00002DDC-00002E9C       .text Ftree_NodeCallBack_M__10daObjFtreeFP7J3DNodei */
BOOL daObjFtree::Ftree_NodeCallBack_M(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        Act_c* i_this = (Act_c*)model->getUserArea();
        if (i_this != NULL) {
            csXyz rot(i_this->mJointRotMX, i_this->mJointRotMY, 0);
            mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));
            mDoMtx_stack_c::ZXYrotM(rot);
            model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
            cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 00002E9C-00002F5C       .text Ftree_NodeCallBack_L__10daObjFtreeFP7J3DNodei */
BOOL daObjFtree::Ftree_NodeCallBack_L(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DJoint* joint = (J3DJoint*)node;
        s32 jntNo = joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        Act_c* i_this = (Act_c*)model->getUserArea();
        if (i_this != NULL) {
            csXyz rot(0, i_this->mJointRotLY, i_this->mJointRotLZ);
            mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));
            mDoMtx_stack_c::ZXYrotM(rot);
            model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
            cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

/* 00002F5C-00003354       .text create_heap__Q210daObjFtree5Act_cFv */
#if VERSION == VERSION_DEMO
bool daObjFtree::Act_c::create_heap() {
    J3DAnmTransform* tmp_bck_data = (J3DAnmTransform*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BCK_VMRTY_e);
    JUT_ASSERT(2100, tmp_bck_data != 0);
    J3DModelData* mdl_dataS = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BDL_VMRTY_e);
    JUT_ASSERT(2106, mdl_dataS != 0);
    mpMorf = new mDoExt_McaMorf(mdl_dataS, NULL, NULL, tmp_bck_data, 0, 1.0f, 0, -1, TRUE, NULL, 0, 0x11020203);
    J3DModel* modelS = mpMorf->getModel();
    modelS->setUserArea((u32)this);
    mdl_dataS->getJointNodePointer(2)->setCallBack(Ftree_NodeCallBack_Effect);
    mdl_dataS->getJointNodePointer(3)->setCallBack(Ftree_NodeCallBack_M);
    mdl_dataS->getJointNodePointer(4)->setCallBack(Ftree_NodeCallBack_M);
    mdl_dataS->getJointNodePointer(5)->setCallBack(Ftree_NodeCallBack_M);

    J3DModelData* mdl_dataL = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BDL_VMRTO_e);
    JUT_ASSERT(2130, mdl_dataL != 0);
    mpModel = mDoExt_J3DModel__create(mdl_dataL, 0, 0x11020203);
    mpModel->setUserArea((u32)this);
    mdl_dataL->getJointNodePointer(1)->setCallBack(Ftree_NodeCallBack_L);

    J3DAnmTevRegKey* brk_data = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BRK_VMRMZ_e);
    JUT_ASSERT(2140, brk_data != 0);
    mBrkAnm.init(mdl_dataS, brk_data, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0);
    get_tev_material0_color(mpMorf->getModel()->getModelData(), 2, &mColorS.r, &mColorS.g, &mColorS.b);
    mColorSBase = mColorS;
    get_tev_material0_color(mpModel->getModelData(), 2, &mColorL.r, &mColorL.g, &mColorL.b);
    mColorLBase = mColorL;
    return bool(mpMorf && mpMorf->getModel()) && mdl_dataL && brk_data;
}
#else
bool daObjFtree::Act_c::create_heap() {
    J3DAnmTransform* tmp_bck_data = (J3DAnmTransform*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BCK_VMRTY_e);
    JUT_ASSERT(2102, tmp_bck_data != 0);
    J3DModelData* mdl_dataS = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BDL_VMRTY_e);
    JUT_ASSERT(2108, mdl_dataS != 0);
    if (mdl_dataS == NULL) {
        return false;
    }
    mpMorf = new mDoExt_McaMorf(mdl_dataS, NULL, NULL, tmp_bck_data, 0, 1.0f, 0, -1, TRUE, NULL, 0, 0x11020203);
    if (mpMorf == NULL) {
        return false;
    }
    J3DModel* modelS = mpMorf->getModel();
    modelS->setUserArea((u32)this);
    mdl_dataS->getJointNodePointer(2)->setCallBack(Ftree_NodeCallBack_Effect);
    mdl_dataS->getJointNodePointer(3)->setCallBack(Ftree_NodeCallBack_M);
    mdl_dataS->getJointNodePointer(4)->setCallBack(Ftree_NodeCallBack_M);
    mdl_dataS->getJointNodePointer(5)->setCallBack(Ftree_NodeCallBack_M);

    J3DModelData* mdl_dataL = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BDL_VMRTO_e);
    JUT_ASSERT(2139, mdl_dataL != 0);
    if (mdl_dataL == NULL) {
        return false;
    }
    mpModel = mDoExt_J3DModel__create(mdl_dataL, 0, 0x11020203);
    if (mpModel == NULL) {
        return false;
    }
    mpModel->setUserArea((u32)this);
    mdl_dataL->getJointNodePointer(1)->setCallBack(Ftree_NodeCallBack_L);

    J3DAnmTevRegKey* brk_data = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VMR_BRK_VMRMZ_e);
    JUT_ASSERT(2155, brk_data != 0);
    if (brk_data == NULL) {
        return false;
    }
    BOOL brk = mBrkAnm.init(mdl_dataS, brk_data, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0);
    get_tev_material0_color(mpMorf->getModel()->getModelData(), 2, &mColorS.r, &mColorS.g, &mColorS.b);
    mColorSBase = mColorS;
    get_tev_material0_color(mpModel->getModelData(), 2, &mColorL.r, &mColorL.g, &mColorL.b);
    mColorLBase = mColorL;
    return bool(mdl_dataS && mpMorf) && mpMorf->getModel() && mdl_dataL && brk_data && brk;
}
#endif

/* 00003354-000035F4       .text _create__Q210daObjFtree5Act_cFv */
cPhs_State daObjFtree::Act_c::_create() {
    fopAcM_SetupActor(this, Act_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0)) {
            mTreeIdx = param_get_tree_idx();
            mHeartPlaced = 0;
            mSpawnedHeartPieceProcessId = fpcM_ERROR_PROCESS_ID_e;
            set_first_stat();
            talk_ct();
            cXyz pos(current.pos.x, current.pos.y + 100.0f, current.pos.z);
            mGndChk.SetPos(&pos);
            mGndChk.SetActorPid(fopAcM_GetID(this));
            mGroundY = dComIfG_Bgsp()->GroundCross(&mGndChk);
            set_mtx();
            fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
            fopAcM_setCullSizeBox(this, -300.0f, 0.0f, -300.0f, 300.0f, 1000.0f, 300.0f);
            mSttsW.Init(0xFF, 0xFF, this);
            mCylW.Set(M_cyl_srcW);
            mCylW.SetStts(&mSttsW);
            *mCylW.GetTgVecP() = cXyz::Zero;
            mCylW.OnTgNoHitMark();
            mStts.Init(0xFF, 0xFF, this);
            mCyl.Set(M_cyl_src);
            mCyl.SetStts(&mStts);
            *mCyl.GetTgVecP() = cXyz::Zero;
            mCyl.OnTgNoHitMark();
            XyEvent_init();
            eventInfo.setXyCheckCB(Ftree_checkXyEventCallBack);
            eventInfo.setXyEventCB(Ftree_XyEventCallBack);
            mEventCam0Idx = dComIfGp_evmng_getEventIdx("morinoki_camera0");
            mEventCam1Idx = dComIfGp_evmng_getEventIdx("morinoki_camera1");
            mEventCam2Idx = dComIfGp_evmng_getEventIdx("morinoki_camera2");
        } else {
            phase = cPhs_ERROR_e;
        }
    }
    return phase;
}

/* 00003FB8-00004004       .text _delete__Q210daObjFtree5Act_cFv */
bool daObjFtree::Act_c::_delete() {
    if (mBroughtSession) {
        set_brought();
    }
#if VERSION == VERSION_DEMO
    if (mpEmitter1 != NULL) {
        mpEmitter1->becomeInvalidEmitter();
    }
    if (mpEmitter2 != NULL) {
        mpEmitter2->becomeInvalidEmitter();
    }
#endif
    dComIfG_resDelete(&mPhs, M_arcname);
    return true;
}

/* 00004004-00004144       .text set_mtx__Q210daObjFtree5Act_cFv */
void daObjFtree::Act_c::set_mtx() {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    cMtx_copy(mDoMtx_stack_c::get(), mMtx);

    cXyz lscale(scale.x * mScaleMul, scale.y * mScaleMul, scale.z * mScaleMul);
    mpModel->setBaseScale(lscale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    cMtx_copy(mDoMtx_stack_c::get(), mMtx);
    mpModel->calc();
}

/* 00004144-000042C4       .text _execute__Q210daObjFtree5Act_cFv */
bool daObjFtree::Act_c::_execute() {
    if (mMode == 0) {
        return true;
    }
    search_heart_part();
    talk_main();
    set_mtx();
    process_main();
    set_collision();
    fopAcM_rollPlayerCrash(this, mCrashRadius, 7);
    s16 step = mMode == 11 ? 1 : 0x1E;
    cLib_addCalcAngleS2(&mJointRotMX, mJointTargetMX, step, 0x1000);
    cLib_addCalcAngleS2(&mJointRotMY, mJointTargetMY, step, 0x1000);
    s16 stepL = (mMode == 12 || mMode == 5) ? 1 : 0xA;
    cLib_addCalcAngleS2(&mJointRotLY, mJointTargetLY, stepL, 0x1000);
    cLib_addCalcAngleS2(&mJointRotLZ, mJointTargetLZ, stepL, 0x1000);
    XyEvent_exe();
    if (mHeartPlaced == 0) {
        mHeartPlaced = place_heart_part();
    }
    if (mLaunchHeart == 1) {
        if (launch_heart_part()) {
            mLaunchHeart = 0;
        }
    }
    if (m652 > 1) {
        m652--;
    }
    if (mLaunchHeart > 1) {
        mLaunchHeart--;
    }
    return true;
}

/* 000042C4-0000455C       .text _draw__Q210daObjFtree5Act_cFv */
bool daObjFtree::Act_c::_draw() {
    if (mMode == 0) {
        return true;
    }
    if (mModelS == 1) {
        g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(mpMorf->getModel(), &tevStr);
        dComIfGd_setListBG();
        if (mMode == 5 || mMode == 6) {
            mBrkAnm.remove(mpMorf->getModel()->getModelData());
            set_tev_color(mpMorf->getModel()->getModelData(), 2, mColorSBase.r, mColorSBase.g, mColorSBase.b);
        } else {
            mBrkAnm.entry(mpMorf->getModel()->getModelData());
        }
        mpMorf->updateDL();
        dComIfGd_setList();
        cXyz pos(current.pos.x, current.pos.y, current.pos.z);
        dComIfGd_setSimpleShadow2(&pos, mGroundY, 75.0f, mGndChk, shape_angle.y, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }
    if (mModelL == 1) {
        g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(mpModel, &tevStr);
        dComIfGd_setListBG();
        set_tev_color(mpModel->getModelData(), 2, mColorLBase.r, mColorLBase.g, mColorLBase.b);
        mDoExt_modelUpdateDL(mpModel);
        dComIfGd_setList();
        f32 ratio = mScaleMul <= 0.2f ? 0.0f : (mScaleMul >= 1.0f ? 1.0f : (mScaleMul - 0.2f) / 0.8f);
        f32 size = 1.0f + 2.5f * ratio;
        cXyz pos(current.pos.x, current.pos.y, current.pos.z);
        dComIfGd_setSimpleShadow2(&pos, mGroundY, 75.0f * size, mGndChk, shape_angle.y, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }
    return true;
}

const char daObjFtree::Act_c::M_arcname[] = "Vmr";

namespace daObjFtree {
namespace {
/* 0000455C-0000457C       .text Mthd_Create__Q210daObjFtree27@unnamed@d_a_obj_ftree_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjFtree::Act_c*)i_this)->_create();
}

/* 0000457C-000045A0       .text Mthd_Delete__Q210daObjFtree27@unnamed@d_a_obj_ftree_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjFtree::Act_c*)i_this)->_delete();
}

/* 000045A0-000045C4       .text Mthd_Execute__Q210daObjFtree27@unnamed@d_a_obj_ftree_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjFtree::Act_c*)i_this)->_execute();
}

/* 000045C4-000045E8       .text Mthd_Draw__Q210daObjFtree27@unnamed@d_a_obj_ftree_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjFtree::Act_c*)i_this)->_draw();
}

/* 000045E8-000045F0       .text Mthd_IsDelete__Q210daObjFtree27@unnamed@d_a_obj_ftree_cpp@FPv */
BOOL Mthd_IsDelete(void*) {
    return TRUE;
}

static actor_method_class Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace
}; // namespace daObjFtree

actor_process_profile_definition g_profile_Obj_Ftree = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Ftree_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjFtree::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Ftree_e,
    /* Actor SubMtd */ &daObjFtree::Mthd_Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
