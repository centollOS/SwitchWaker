/**
 * d_a_obj_try.cpp
 * Object - Tower of the Gods - Glowing blue/pink statue/pillar
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_try.h"
#include "d/actor/d_a_player.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_func.h"
#include "f_op/f_op_actor_iter.h"
#include "f_op/f_op_camera.h"
#include "f_op/f_op_kankyo_mng.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Hseki.h"

namespace daObjTry {
namespace {
const f32 L_attrBase[] = {-0.6f, 15.0f, 15.0f};
}

const char Act_c::M_arcname[] = "Hseki";
const dCcD_SrcCyl Act_c::M_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_UNK8,
        /* SrcObjAt  Atp     */ 1,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsEnemy_e | cCcD_AtSPrm_VsOther_e,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsEnemy_e | cCcD_CoSPrm_VsPlayer_e | cCcD_CoSPrm_VsOther_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ 0,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCylS
    {{{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f}},
};

const Act_c::Attr_c Act_c::M_attr[13] = {
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0x1240,
        1.0f, 0.75f, 1.12f,
        0, 0, 0, 0, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0x1240,
        1.0f, 0.75f, 1.12f,
        0, 0, 0, 0, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 0, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 0, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 0, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, dRes_INDEX_HSEKI_BRK_HMON1_e,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0x1BE0,
        1.0f, 0.75f, 1.12f,
        1, 1, 0, 1, 1, 1,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON2_e, dRes_INDEX_HSEKI_BRK_HMON2_e,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0x1BE0,
        1.0f, 0.75f, 1.12f,
        1, 1, 0, 1, 1, 1,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 1, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 1, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 1, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 1, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 1, 0, 0,
    },
    {
        -6.0f, 70.0f, 150.0f, 200,
        27.0f, 36.0f, 0.0f,
        0.002f, 0.0002f, 4.0f, 0.2f, 0.02f,
        30.0f, 15, 15, 0.1f, 0.02f,
        150, 5, 100, 4, 125, 5,
        dRes_INDEX_HSEKI_BDL_HMON1_e, -1,
        72, 56, 150, 150,
        JA_SE_OBJ_BREAK_STONE, JA_SE_OBJ_PUT_STONE, JA_SE_OBJ_FALL_WATER_S, 0xD, 0xFFFF,
        1.0f, 0.75f, 1.12f,
        0, 0, 1, 1, 0, 0,
    },
};

bool Act_c::M_bingo;
bool Act_c::M_restart;

/* 00000078-00000090       .text prm_set_swSave__Q28daObjTry5Act_cFi */
void Act_c::prm_set_swSave(int value) {
    u32 params = fopAcM_GetParam(this);
    u32 mask = 0xFF;
    u32 v = value & mask;
    params &= ~(mask << 8);
    params |= v << 8;
    fopAcM_SetParam(this, params);
}

/* 00000090-000000B4       .text solidHeapCB__Q28daObjTry5Act_cFP10fopAc_ac_c */
BOOL Act_c::solidHeapCB(fopAc_ac_c* actor) {
    return static_cast<Act_c*>(actor)->create_heap();
}

/* 000000B4-0000026C       .text create_heap__Q28daObjTry5Act_cFv */
bool Act_c::create_heap() {
    bool success = false;
    J3DModelData* mdl_data = static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, attr().mModelRes));
    JUT_ASSERT(0x3F1, mdl_data != 0);
    mpModel = mDoExt_J3DModel__create(mdl_data, 0, 0x11020203);
    BOOL animOK = TRUE;
    if (attr().mBrkRes >= 0) {
        J3DAnmTevRegKey* brk = static_cast<J3DAnmTevRegKey*>(dComIfG_getObjectRes(M_arcname, attr().mBrkRes));
        JUT_ASSERT(0x3FB, brk != 0);
        animOK = mBrk.init(mdl_data, brk, true, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0);
        if (!prm_get_dummy()) {
            mBrk.setFrame(mBrk.getEndFrame());
        }
    }
    if (mpModel && animOK) {
        success = true;
    }
    return success;
}

/* 0000026C-0000038C       .text init_cc__Q28daObjTry5Act_cFv */
void Act_c::init_cc() {
    mStts.Init(attr().mWeight, 0xFF, this);
    mCyl.Set(M_cyl_src);
    mCyl.SetStts(&mStts);
    mCyl.SetR(attr().mRadius);
    mCyl.SetH(attr().mHeight);
    mCyl.SetAtVec(*(cXyz*)&cXyz::Zero);
    mCyl.SetTgVec(*(cXyz*)&cXyz::Zero);
    mCyl.OnTgShield();
}

/* 0000038C-00000428       .text search_sameType__Q28daObjTry5Act_cFPvPv */
void* Act_c::search_sameType(void* actor, void* self) {
    Act_c* me = static_cast<Act_c*>(self);
    Act_c* other;
    if ((other = static_cast<Act_c*>(actor)) != NULL && fopAc_IsActor(other) &&
        fopAcM_GetName(other) == fpcNm_Obj_Try_e && other != me) {
        if (other->prm_get_type() == me->prm_get_type() && other->mAppear) {
            return other;
        }
    }
    return NULL;
}

/* 00000428-00000528       .text chk_appear__Q28daObjTry5Act_cCFv */
bool Act_c::chk_appear() const {
    bool appear = true;
    if (attr().mCheckSwitch) {
        u32 dummy = bool(prm_get_dummy());
        int swSave = prm_get_swSave();
        bool sw = dComIfGs_isSwitch(swSave, fopAcM_GetHomeRoomNo(const_cast<Act_c*>(this))) != 0;
        if ((!sw && dummy) || (sw && !dummy)) {
            appear = false;
        }
    }
    if (attr().mUnique && fopAcIt_Judge(search_sameType, const_cast<Act_c*>(this))) {
        appear = false;
    }
    return appear;
}

/* 00000528-000008D8       .text _create__Q28daObjTry5Act_cFv */
cPhs_State Act_c::_create() {
    fopAcM_SetupActor(this, Act_c);
    mType = prm_get_type();
    mAppear = chk_appear();
    cPhs_State phase = cPhs_ERROR_e;
    if (mAppear) {
        phase = dComIfG_resLoad(&mPhase, M_arcname);
        if (phase == cPhs_COMPLEATE_e) {
            if (fopAcM_entrySolidHeap(this, solidHeapCB, attr().mHeapSize)) {
                mAcchCir.SetWall(30.0f, attr().mRadius);
                mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), fopAcM_GetAngle_p(this), fopAcM_GetShapeAngle_p(this));
                mAcch.ClrWaterNone();
                mAcch.ClrRoofNone();
                mAcch.SetRoofCrrHeight(attr().mRoofHeight);
                init_cc();
                fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
                cull_set_draw();
                gravity = attr().mGravity;
                if (mType == UNK_5 || mType == UNK_6) {
                    fopAcM_OnStatus(this, 0x02000000);
                }
                fopAcM_posMoveF(this, NULL);
                mAcch.CrrPos(*dComIfG_Bgsp());
                mAcch.ClrGroundLanding();
                mLanded = 1;
                mLandTimer = 20;
                attention_info.flags |= fopAc_Attn_ACTION_CARRY_e;
                attention_info.distances[fopAc_Attn_TYPE_CARRY_e] = 0x17;
                attention_info.position.x = current.pos.x;
                attention_info.position.y = current.pos.y + attr().mAttentionY;
                attention_info.position.z = current.pos.z;
                mMoveBG = 1;
                mMassTimer = 0;
                mPrevSpeedY = 0.0f;
                mTimer = 0;
                field_0x64A = 0;
                init_mtx();
                mBingoEffect = 0;
                field_0x64C = 0;
                field_0x64D = 0;
                field_0x64E = 1;
                mLeftHomeRoom = 0;
                field_0x652 = 0;
                mode_wait_init();
                model = mpModel;
            } else {
                phase = cPhs_ERROR_e;
            }
        }
    }
    return phase;
}

/* 00000C88-00000D5C       .text _delete__Q28daObjTry5Act_cFv */
bool Act_c::_delete() {
    if (mAppear) {
        mFollow.remove();
        if (mpEmitter) {
            mpEmitter->becomeInvalidEmitter();
        }
        if (mType == UNK_5) {
            dComIfGs_offTmpBit(dSv_event_tmp_flag_c::UNK_0108);
        } else if (mType == UNK_6) {
            dComIfGs_offTmpBit(dSv_event_tmp_flag_c::UNK_0110);
        }
        model = NULL;
        dComIfG_deleteObjectRes(M_arcname);
    }
    return true;
}

/* 00000D5C-00000EA8       .text mode_restart_init__Q28daObjTry5Act_cFv */
void Act_c::mode_restart_init() {
    mCyl.OffAtSetBit();
    mCyl.OffTgSetBit();
    mCyl.OnCoSetBit();
    mAcch.SetRoofNone();
    mAcch.SetWallNone();
    mAcch.ClrGrndNone();
    mAcch.SetWaterNone();
    mAcch.OffLineCheck();
    speedF = 0.0f;
    mTimer = 110;
    current.pos.set(home.pos.x, home.pos.y + (-10.0f - attr().mRoofHeight), home.pos.z);
    shape_angle.set(home.angle.x, home.angle.y - 0x8000, home.angle.z);
    current.angle = shape_angle;
    old.pos = current.pos;
    old.angle = current.angle;
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    mMode = 0;
}

/* 00000EA8-00000FFC       .text mode_restart__Q28daObjTry5Act_cFv */
void Act_c::mode_restart() {
    mTimer--;
    if (mTimer < 80) {
        M_restart = true;
    }
    mLandTimer = 20;
    mLanded = 1;
    if (mTimer == 0) {
        shape_angle.y = home.angle.y;
        current.angle.y = shape_angle.y;
        current.pos.y = home.pos.y;
        mode_wait_init();
    } else if (mTimer < 50) {
        s16 ang = 655.36f * mTimer;
        f32 factor = 0.5f * (1.0f - cM_scos(ang));
        current.pos.y = home.pos.y + factor * (-10.0f - attr().mRoofHeight);
        shape_angle.y = s16(-32768.0f * factor) + home.angle.y;
        current.angle.y = shape_angle.y;
    }
}

/* 00000FFC-00001074       .text mode_wait_init__Q28daObjTry5Act_cFv */
void Act_c::mode_wait_init() {
    mCyl.OffAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OnCoSetBit();
    mAcch.ClrRoofNone();
    mAcch.ClrWallNone();
    mAcch.ClrGrndNone();
    mAcch.ClrWaterNone();
    mAcch.OffLineCheck();
    speedF = 0.0f;
    mMode = 1;
}

/* 00001074-000012C4       .text mode_wait__Q28daObjTry5Act_cFv */
void Act_c::mode_wait() {
    bool locked = false;
    if (attr().mLockPos && field_0x64C) {
        locked = true;
    }
    if (attr().mCorrectPos && field_0x64D) {
        f32 y = current.pos.y;
        cLib_chasePos(&current.pos, field_0x63C, 10.0f);
        current.pos.y = y;
        if (field_0x63C.abs2XZ(current.pos) < 25.0f) {
            current.pos.x = field_0x63C.x;
            current.pos.z = field_0x63C.z;
            if (field_0x63C.abs2(current.pos) < 1.0f) {
                M_bingo = true;
            }
        }
        s16 angle = s16(shape_angle.y - field_0x648);
        field_0x64A = ((angle + 0x2000) & 0xC000) + field_0x648;
        cLib_addCalcAngleS(&shape_angle.y, field_0x64A, 3, 0x1800, 0x800);
    } else if (!locked) {
        bound();
    }
    bool ground = mAcch.ChkGroundHit();
    if (ground && !locked) {
        attention_info.flags |= fopAc_Attn_ACTION_CARRY_e;
    } else {
        attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    }
    if (locked) {
        field_0x64E = 1;
    }
    const cXyz* ccMove = (ground && (locked || field_0x64D)) ? &cXyz::Zero : mStts.GetCCMoveP();
    if (mAcch.ChkGroundHit()) {
        gravity = attr().mGravity;
        fopAcM_posMoveF(this, ccMove);
    } else {
        f32 g, linear, quadratic;
        calc_drop_param(&g, &linear, &quadratic);
        gravity = g;
        daObj::posMoveF_stream(this, ccMove, &cXyz::Zero, linear, quadratic);
    }
}

/* 000012C4-00001344       .text mode_carry_init__Q28daObjTry5Act_cFv */
void Act_c::mode_carry_init() {
    mCyl.OffAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OffCoSetBit();
    mAcch.ClrRoofNone();
    mAcch.ClrWallNone();
    mAcch.ClrGrndNone();
    mAcch.ClrWaterNone();
    mAcch.OffLineCheck();
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    mTimer = 8;
    mMode = 2;
}

/* 00001344-000013D4       .text mode_carry__Q28daObjTry5Act_cFv */
void Act_c::mode_carry() {
    if (mTimer > 0) {
        mTimer--;
    }
    speed.y = 0.0f;
    if (!fopAcM_checkCarryNow(this)) {
        if (fopAcM_GetSpeedF(this) > 0.0f) {
            mode_drop_init();
            mode_drop();
        } else {
            daObj::SetCurrentRoomNo(this, &mAcch.m_gnd);
            mMassTimer = 2;
            mode_wait_init();
        }
    }
}

/* 000013D4-000014A8       .text mode_drop_init__Q28daObjTry5Act_cFv */
void Act_c::mode_drop_init() {
#if VERSION == VERSION_DEMO
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 forward = attr().mThrowSpeedF + fopAcM_GetSpeedF(player) * attr().mPlayerSpeedRate;
#else
    f32 forward = attr().mThrowSpeedF + dComIfGp_getPlayer(0)->speedF * attr().mPlayerSpeedRate;
#endif
    mCyl.OnAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OnCoSetBit();
    mAcch.ClrRoofNone();
    mAcch.ClrWallNone();
    mAcch.ClrGrndNone();
    mAcch.ClrWaterNone();
    mAcch.OnLineCheck();
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    speed.y = attr().mThrowSpeedY;
    speedF = forward;
    gravity = attr().mGravity;
    mMode = 3;
}

/* 000014A8-00001518       .text mode_drop__Q28daObjTry5Act_cFv */
void Act_c::mode_drop() {
    daObj::SetCurrentRoomNo(this, &mAcch.m_gnd);
    bound();
    f32 g, linear, quadratic;
    calc_drop_param(&g, &linear, &quadratic);
    gravity = g;
    daObj::posMoveF_stream(this, mStts.GetCCMoveP(), &cXyz::Zero, linear, quadratic);
}

/* 00001518-000016A0       .text mode_sink_init__Q28daObjTry5Act_cFv */
void Act_c::mode_sink_init() {
    mCyl.OffAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OnCoSetBit();
    mAcch.SetRoofNone();
    mAcch.ClrWallNone();
    mAcch.ClrGrndNone();
    mAcch.ClrWaterNone();
    mAcch.OnLineCheck();
    gravity = attr().mGravity + attr().mBuoyancy;
    f32 magnitude = std::sqrtf(speed.y * speed.y + speedF * speedF);
    if (magnitude > attr().mMaxWaterSpeed) {
        f32 factor = attr().mMaxWaterSpeed / magnitude;
        speed *= factor;
        speedF *= factor;
    }
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    mMode = 4;
}

/* 000016A0-0000177C       .text mode_sink__Q28daObjTry5Act_cFv */
void Act_c::mode_sink() {
    f32 water = get_water_h();
    f32 ground = mAcch.GetGroundH();
    daObj::SetCurrentRoomNo(this, &mAcch.m_gnd);
    if (speed.y != 0.0f && current.pos.y + attr().mWaterOffset > water && water - attr().mWaterOffset > ground) {
        current.pos.y = water - attr().mWaterOffset;
    }
    daObj::posMoveF_stream(this, mStts.GetCCMoveP(), &cXyz::Zero, attr().mWaterDrag, attr().mWaterDrag2);
}

/* 0000177C-00001B58       .text mode_proc_call__Q28daObjTry5Act_cFv */
bool Act_c::mode_proc_call() {
    typedef void (Act_c::*ModeProc)();
    static ModeProc mode_proc[] = {
        &Act_c::mode_restart, &Act_c::mode_wait, &Act_c::mode_carry, &Act_c::mode_drop, &Act_c::mode_sink,
    };
    if (fopAcM_checkCarryNow(this) && mMode != 2) {
        mode_carry_init();
    }
    M_bingo = false;
    M_restart = false;
    (this->*mode_proc[mMode])();
    if (attr().mBrkRes >= 0) {
        if (M_bingo || M_restart) {
            eff_set_bingo(M_bingo, M_restart);
        } else {
            eff_clr_bingo();
        }
        if (M_bingo || M_restart || mMode == 2) {
            mBrk.setPlaySpeed(1.0f);
            mBrk.setPlayMode(J3DFrameCtrl::EMode_LOOP);
            fopAcM_seStart(this, JA_SE_OBJ_RES_ST_BLINK, 0);
        } else {
            mBrk.setPlayMode(J3DFrameCtrl::EMode_NONE);
        }
        if (mType == UNK_5) {
            if (mMode == 2) {
                dComIfGs_onTmpBit(dSv_event_tmp_flag_c::UNK_0108);
            } else {
                dComIfGs_offTmpBit(dSv_event_tmp_flag_c::UNK_0108);
            }
        } else if (mType == UNK_6) {
            if (mMode == 2) {
                dComIfGs_onTmpBit(dSv_event_tmp_flag_c::UNK_0110);
            } else {
                dComIfGs_offTmpBit(dSv_event_tmp_flag_c::UNK_0110);
            }
        }
        mBrk.play();
    }
    mPrevSpeedY = speed.y;
    cXyz pos = current.pos;
    cXyz savedSpeed = speed;
    mAcch.CrrPos(*dComIfG_Bgsp());
    if (dComIfG_Bgsp()->ChkMoveBG(mAcch.m_gnd)) {
        mMoveBG = 1;
    }
    if (mMode == 0 || mMode == 2) {
        current.pos = pos;
        speed = savedSpeed;
    }
    damage_bg_proc_directly();
    if (mMode != 2) {
        tevStr.mRoomNo = current.roomNo;
        tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mAcch.m_gnd);
    }
    if (current.roomNo != home.roomNo) {
        mLeftHomeRoom = 1;
        if (attr().mClearSwitch && prm_get_swSave() != 0xFF) {
            prm_set_swSave(0xFF);
        }
    }
    if (dComIfGp_event_runCheck()) {
        field_0x64E = 1;
    }
    return true;
}

/* 00001B58-00001B90       .text cull_set_draw__Q28daObjTry5Act_cFv */
void Act_c::cull_set_draw() { fopAcM_setCullSizeSphere(this, 0.0f, 65.0f, 0.0f, 100.0f); }
/* 00001B90-00001BC8       .text cull_set_move__Q28daObjTry5Act_cFv */
void Act_c::cull_set_move() { fopAcM_setCullSizeSphere(this, 0.0f, 65.0f, 0.0f, 300.0f); }
/* 00001BC8-00001BE8       .text damaged__Q28daObjTry5Act_cFv */
void Act_c::damaged() { fopAcM_cancelCarryNow(this); }

/* 00001BE8-00001CD8       .text damage_cc_proc__Q28daObjTry5Act_cFv */
bool Act_c::damage_cc_proc() {
    if (mCyl.ChkAtHit()) {
        mCyl.ClrAtHit();
        speedF *= 0.3f;
    } else if (mCyl.ChkTgHit()) {
        mCyl.GetTgHitObj();
        bool shield = false;
        if (!shield) {
            daObj::HitSeStart(&eyePos, current.roomNo, &mCyl, attr().mHitSound);
            set_senv(attr().mHitSoundRadius, attr().mHitSoundTime);
            daObj::HitEff_hibana(this, &mCyl);
        }
        mCyl.ClrTgHit();
    }
    return false;
}

/* 00001CD8-00001E98       .text damage_bg_proc__Q28daObjTry5Act_cFv */
bool Act_c::damage_bg_proc() {
    int homeRoom = fopAcM_GetHomeRoomNo(this);
    int stayRoom = dComIfGp_roomControl_getStayNo();
    bool ground = mAcch.ChkGroundHit();
    bool water = chk_sink_water();
    bool remove = false;
    bool restart = false;
    if (mMode == 1) {
        if (water) {
            se_fall_water();
            if (speed.y != 0.0f) {
                eff_hit_water_splash();
                mSplashed = 1;
            } else {
                mSplashed = 0;
            }
            mode_sink_init();
        }
    } else if (mMode == 3) {
        if (ground) {
            mode_wait_init();
        } else if (water) {
            se_fall_water();
            eff_hit_water_splash();
            mSplashed = 1;
            mode_sink_init();
        }
    } else if (mMode == 4) {
        if (chk_sinkdown_water()) {
            restart = true;
            if (!mSplashed) {
                eff_hit_water_splash();
            }
        } else if (!water) {
            mode_wait_init();
        }
    }
    if (ground && dComIfG_Bgsp()->GetGroundCode(mAcch.m_gnd) == 4) {
        restart = true;
    }
    if (restart) {
        if (homeRoom == stayRoom && (attr().mUnique || !mLeftHomeRoom)) {
            mode_restart_init();
        } else {
            damaged();
            remove = true;
        }
    }
    return remove;
}

/* 00001E98-00001FEC       .text damage_bg_proc_directly__Q28daObjTry5Act_cFv */
void Act_c::damage_bg_proc_directly() {
    bool ground = mAcch.ChkGroundHit();
    if (mMode == 3 && (mAcch.ChkRoofHit() || chk_sink_water() || ground)) {
        cam_lockoff();
    }
    if (mLandTimer > 0) {
        mLandTimer--;
    } else if (ground) {
        if (!mLanded && (mMode == 1 || mMode == 3)) {
            fopAcM_seStart(this, attr().mLandSound, dComIfG_Bgsp()->GetMtrlSndId(mAcch.m_gnd));
            if (mPrevMode != 4) {
                eff_land_smoke();
            }
            mLanded = 1;
            mLandTimer = 20;
            make_vib();
        }
    } else {
        mLanded = 0;
    }
}

/* 00001FEC-00002034       .text chk_sink_water__Q28daObjTry5Act_cFv */
bool Act_c::chk_sink_water() {
    return mAcch.ChkWaterHit() && mAcch.m_wtr.GetHeight() > current.pos.y + attr().mWaterOffset;
}

/* 00002034-000020AC       .text chk_sinkdown_water__Q28daObjTry5Act_cFv */
bool Act_c::chk_sinkdown_water() {
    return mAcch.ChkWaterHit() && mAcch.m_wtr.GetHeight() > 50.0f + (current.pos.y + attr().mRoofHeight);
}

/* 000020AC-00002218       .text calc_drop_param__Q28daObjTry5Act_cCFPfPfPf */
void Act_c::calc_drop_param(f32* g, f32* linear, f32* quadratic) const {
    bool waterHit = const_cast<Act_c*>(this)->mAcch.ChkWaterHit();
    if (waterHit) {
        f32 waterY = const_cast<Act_c*>(this)->mAcch.m_wtr.GetHeight();
        f32 distance = current.pos.y - waterY;
        f32 depth;
        if (distance >= 0.0f) {
            depth = 0.0f;
        } else if (distance <= -attr().mRoofHeight) {
            depth = 0.5f;
        } else {
            depth = -distance * (0.5f / attr().mRoofHeight);
        }
        f32 air = 1.0f - depth;
        *linear = depth * attr().mWaterDrag + air * attr().mAirDrag;
        *quadratic = depth * attr().mWaterDrag2 + air * attr().mAirDrag2;
        *g = depth * attr().mBuoyancy + attr().mGravity;
    } else {
        *linear = attr().mAirDrag;
        *quadratic = attr().mAirDrag2;
        *g = attr().mGravity;
    }
}

/* 00002218-000022F4       .text bound__Q28daObjTry5Act_cFv */
bool Act_c::bound() {
    bool ground = mAcch.ChkGroundHit();
    if (mAcch.ChkWallHit()) {
        speedF *= 0.3f;
        current.angle.y = 2 * mAcchCir.GetWallAngleY() - (current.angle.y + 0x8000);
    }
    if (mAcch.ChkGroundLanding()) {
        f32 bounce = std::fabsf(mPrevSpeedY * L_attrBase[0]);
        if (bounce > L_attrBase[1]) {
            if (bounce > L_attrBase[2]) {
                speed.y = L_attrBase[2];
            } else {
                speed.y = bounce;
            }
            ground = false;
        }
    } else if (mAcch.ChkGroundHit()) {
        cLib_addCalc(&speedF, 0.0f, 0.5f, 5.0f, 1.0f);
    }
    return ground;
}

/* 000022F4-0000240C       .text se_fall_water__Q28daObjTry5Act_cFv */
void Act_c::se_fall_water() {
    cBgS_PolyInfo* polys[2] = {NULL, NULL};
    polys[0] = &mAcch.m_wtr;
    polys[1] = &mAcch.m_gnd;
    int material = 19;
    for (int i = 0; i < 2; i++) {
        if (polys[i]->GetBgIndex() >= 0 && polys[i]->GetBgIndex() < 0x100) {
            material = dComIfG_Bgsp()->GetMtrlSndId(*polys[i]);
            break;
        }
    }
    fopAcM_seStart(this, attr().mWaterSound, material);
    set_senv(attr().mWaterSoundRadius, attr().mWaterSoundTime);
}

/* 0000240C-00002460       .text set_senv__Q28daObjTry5Act_cCFii */
void Act_c::set_senv(int radius, int time) const {
    dKy_Sound_set(current.pos, radius, fopAcM_GetID(const_cast<Act_c*>(this)), time);
}

/* 00002460-00002498       .text cam_lockoff__Q28daObjTry5Act_cCFv */
void Act_c::cam_lockoff() const { dComIfGp_getCamera(0)->mCamera.ForceLockOff(base.base.mBsPcId); }

/* 00002498-00002504       .text set_mtx__Q28daObjTry5Act_cFv */
void Act_c::set_mtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00002504-00002540       .text init_mtx__Q28daObjTry5Act_cFv */
void Act_c::init_mtx() {
    mpModel->setBaseScale(scale);
    set_mtx();
}

/* 00002540-00002718       .text eff_set_bingo__Q28daObjTry5Act_cFbb */
void Act_c::eff_set_bingo(bool bingo, bool restart) {
    static GXColor prm0[] = {{12, 24, 72, 0}, {72, 12, 24, 0}};
    static GXColor env0[] = {{66, 73, 202, 0}, {103, 62, 202, 0}};
    static GXColor prm1[] = {{2, 12, 56, 0}, {56, 2, 12, 0}};
    static GXColor env1[] = {{8, 62, 27, 0}, {42, 62, 98, 0}};
    if (!mBingoEffect) {
        int color = mType == UNK_5 ? 0 : 1;
        dComIfGp_particle_set(dPa_name::ID_AK_SN_SIRENKEY00, &current.pos, &shape_angle, NULL, 0xFF, &mFollow, -1, &prm0[color], &env0[color]);
        const cXyz* pos = restart ? &home.pos : &current.pos;
        csXyz angle(0, field_0x64A, 0);
        mpEmitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_SIRENKEY01, pos, &angle, NULL, 0xFF, NULL, -1, &prm1[color], &env1[color]);
        if (bingo) {
            fopAcM_seStart(this, JA_SE_OBJ_KEY_STATUE_SET, 0);
        }
        mBingoEffect = 1;
    } else {
        fopAcM_seStart(this, JA_SE_OBJ_KEY_ST_LIGHT, 0);
    }
}

/* 00002718-00002790       .text eff_clr_bingo__Q28daObjTry5Act_cFv */
void Act_c::eff_clr_bingo() {
    if (mBingoEffect) {
        mFollow.remove();
        if (mpEmitter) {
            mpEmitter->becomeInvalidEmitter();
#if VERSION > VERSION_DEMO
            mpEmitter = NULL;
#endif
        }
        mBingoEffect = 0;
    }
}

/* 00002790-000027BC       .text eff_land_smoke__Q28daObjTry5Act_cFv */
void Act_c::eff_land_smoke() { daObj::make_land_effect(this, &mAcch.m_gnd, 1.0f); }

/* 000027BC-0000280C       .text eff_hit_water_splash__Q28daObjTry5Act_cFv */
void Act_c::eff_hit_water_splash() {
    cXyz pos(current.pos.x, mAcch.m_wtr.GetHeight(), current.pos.z);
    fopKyM_createWpillar(&pos, 1.0f, 0.75f, 0);
}

/* 0000280C-00002868       .text make_vib__Q28daObjTry5Act_cFv */
void Act_c::make_vib() {
    int close = check_circle() & 1;
    dComIfGp_getVibration().StartShock(close + 1, 1, cXyz(0.0f, 1.0f, 0.0f));
}

/* 00002868-00002960       .text check_circle__Q28daObjTry5Act_cFv */
bool Act_c::check_circle() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz delta = player->current.pos - current.pos;
    f32 distance = delta.absXZ();
    bool close = false;
    if (distance <= 190.0f) {
        close = true;
    }
    return close;
}

/* 00002960-00002A90       .text get_water_h__Q28daObjTry5Act_cFv */
f32 Act_c::get_water_h() {
    dBgS_WtrChk chk;
    f32 height = current.pos.y;
    if (dBgS_SplGrpChk_In_ObjGnd(current.pos, &chk, 100.0f)) {
        height = chk.GetHeight();
    }
    return height;
}

/* 00002BB4-00002DA8       .text _execute__Q28daObjTry5Act_cFv */
bool Act_c::_execute() {
    cull_set_move();
    if (mMoveBG || mMode != 1 || mAcch.ChkGroundHit() == false || mAcch.ChkGroundLanding() || attr().mAlwaysMove || !fopAcM_cullingCheck(this)) {
        mMoveBG = 0;
        BOOL remove = TRUE;
        if (!damage_cc_proc() && !damage_bg_proc()) {
            if (mMassTimer) {
                mMassTimer--;
            }
            field_0x64E = 0;
            if (mode_proc_call()) {
                remove = FALSE;
                set_mtx();
                mStts.SetRoomId(current.roomNo);
                mCyl.MoveCAtTg(current.pos);
                dComIfG_Ccsp()->Set(&mCyl);
                if (mMode == 3 || mMode == 4 || mMassTimer) {
                    dComIfG_Ccsp()->SetMass(&mCyl, 3);
                }
                attention_info.position.x = current.pos.x;
                attention_info.position.y = current.pos.y + attr().mAttentionY;
                attention_info.position.z = current.pos.z;
                eyePos = attention_info.position;
                mPrevMode = mMode;
            }
        }
        if (remove) {
            fopAcM_delete(this);
        }
    }
    if (!attr().mLockPos) {
        field_0x64C = 0;
    }
    field_0x64D = 0;
    cull_set_draw();
    return true;
}

/* 00002DA8-00002EA4       .text _draw__Q28daObjTry5Act_cFv */
bool Act_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    if (attr().mBrkRes >= 0) {
        mBrk.entry(mpModel->getModelData());
    }
    mDoExt_modelUpdateDL(mpModel);
    if (!fopAcM_checkCarryNow(this)) {
        dComIfGd_setSimpleShadow2(&current.pos, mAcch.GetGroundH(), attr().mShadowSize, mAcch.m_gnd, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }
    return true;
}

} // namespace daObjTry

namespace daObjTry {
namespace {
/* 00002EA4-00002EC4       .text Mthd_Create__Q28daObjTry25@unnamed@d_a_obj_try_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjTry::Act_c*)i_this)->_create();
}

/* 00002EC4-00002EE8       .text Mthd_Delete__Q28daObjTry25@unnamed@d_a_obj_try_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjTry::Act_c*)i_this)->_delete();
}

/* 00002EE8-00002F0C       .text Mthd_Execute__Q28daObjTry25@unnamed@d_a_obj_try_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjTry::Act_c*)i_this)->_execute();
}

/* 00002F0C-00002F30       .text Mthd_Draw__Q28daObjTry25@unnamed@d_a_obj_try_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjTry::Act_c*)i_this)->_draw();
}

/* 00002F30-00002F38       .text Mthd_IsDelete__Q28daObjTry25@unnamed@d_a_obj_try_cpp@FPv */
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
}; // namespace daObjTry

actor_process_profile_definition g_profile_Obj_Try = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0008,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Try_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjTry::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Try_e,
    /* Actor SubMtd */ &daObjTry::Mthd_Table,
    /* Status       */ 0x04 | fopAcStts_SHOWMAP_e | fopAcStts_CULL_e | fopAcStts_FREEZE_e | fopAcStts_UNK4000_e | fopAcStts_UNK40000_e | fopAcStts_UNK8000000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLSPHERE_CUSTOM_e,
};
