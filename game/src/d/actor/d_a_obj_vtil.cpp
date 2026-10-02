/**
 * d_a_obj_vtil.cpp
 * Object - Tingle statues
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_vtil.h"
#include "d/actor/d_a_agbsw0.h"
#include "d/d_com_inf_game.h"
#include "d/d_camera.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera.h"
#include "f_op/f_op_kankyo_mng.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Vtil.h"

const int daObjVtil_c::l_daObjVtil_bdl_idx_table[5] = {
    dRes_INDEX_VTIL_BDL_VTIL1_e, dRes_INDEX_VTIL_BDL_VTIL2_e,
#if VERSION == VERSION_DEMO
    dRes_INDEX_VTIL_BDL_VTIL3_e, dRes_INDEX_VTIL_BDL_VTIL4_e,
    dRes_INDEX_VTIL_BDL_VTIL5_e,
#else
    dRes_INDEX_VTIL_BDL_VTIL3_e, dRes_INDEX_VTIL_BDL_VTIL5_e,
    dRes_INDEX_VTIL_BDL_VTIL4_e,
#endif
};
const int daObjVtil_c::l_daObjVtil_scene_no_table[5] = {3, 4, 5, 7, 6};
const dCcD_SrcCyl daObjVtil_c::M_co_cyl_data = {
    {
        /* Flags             */ 0,
        /* SrcObjAt Type     */ AT_TYPE_UNK8,
        /* SrcObjAt Atp      */ 1,
        /* SrcObjAt SPrm     */ 0xB,
        /* SrcObjTg Type     */ 0xFF1DFEFF,
        /* SrcObjTg SPrm     */ 9,
        /* SrcObjCo SPrm     */ 0x79,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ 0,
        /* SrcGObjAt Spl     */ 0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 52.0f,
        /* Height */ 160.0f,
    }},
};
const char daObjVtil_c::M_arcname[] = "Vtil";


/* 00000078-00000098       .text solidHeapCB__11daObjVtil_cFP10fopAc_ac_c */
BOOL daObjVtil_c::solidHeapCB(fopAc_ac_c* actor) {
    return static_cast<daObjVtil_c*>(actor)->create_heap();
}

/* 00000098-0000016C       .text create_heap__11daObjVtil_cFv */
BOOL daObjVtil_c::create_heap() {
    BOOL result = FALSE;
    J3DModelData* mdl_data = static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, l_daObjVtil_bdl_idx_table[mType]));
    JUT_ASSERT(DEMO_SELECT(0x143, 0x145), mdl_data != 0);
    if (mdl_data != 0) {
        mpModel = mDoExt_J3DModel__create(mdl_data, 0, 0x11020203);
        if (mpModel != 0) {
            result = TRUE;
        }
    }
    return result;
}

/* 0000016C-000003C0       .text _create__11daObjVtil_cFv */
cPhs_State daObjVtil_c::_create() {
    cPhs_State phase = cPhs_ERROR_e;
    fopAcM_SetupActor(this, daObjVtil_c);
    mType = prm_get_type();
    if (mType == 0xF || mType == -1) {
        mType = 0;
    }
    if (check_ev_bit()) {
        phase = dComIfG_resLoad(&mPhase, M_arcname);
        if (phase == cPhs_COMPLEATE_e) {
            phase = cPhs_ERROR_e;
            if (fopAcM_entrySolidHeap(this, solidHeapCB, 0xCC0)) {
                fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
                init_mtx();
                fopAcM_setCullSizeBox(this, -50.0f, 0.0f, -50.0f, 50.0f, 160.0f, 50.0f);
                init_co();
                init_bgc();
                gravity = -6.0f;
                renew_attention_pos();
                eyePos = current.pos;
                eyePos.y += 110.0f;
                attention_info.flags |= fopAc_Attn_ACTION_CARRY_e;
                attention_info.distances[fopAc_Attn_TYPE_CARRY_e] = 0x17;
                mFirstLanding = 1;
                to_wait_mode();
                dKy_plight_set(&mLight);
                model = mpModel;
                phase = cPhs_COMPLEATE_e;
            }
        }
    }
    return phase;
}

/* 000006CC-00000718       .text _delete__11daObjVtil_cFv */
bool daObjVtil_c::_delete() {
    dKy_plight_cut(&mLight);
    model = NULL;
    dComIfG_resDelete(&mPhase, M_arcname);
    return true;
}

/* 00000718-00000750       .text check_ev_bit__11daObjVtil_cCFv */
BOOL daObjVtil_c::check_ev_bit() const {
    return dComIfGs_isStageTbox(l_daObjVtil_scene_no_table[mType], 15);
}

/* 00000750-00000770       .text tell_agb_attack__11daObjVtil_cFv */
void daObjVtil_c::tell_agb_attack() {
    daAgbsw0_c::incTclBeat();
}

/* 00000770-00000790       .text tell_agb_sink__11daObjVtil_cFv */
void daObjVtil_c::tell_agb_sink() {
    daAgbsw0_c::incTclDispose();
}

/* 00000790-0000082C       .text init_mtx__11daObjVtil_cFv */
void daObjVtil_c::init_mtx() {
    mpModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpModel->calc();
}

/* 0000082C-000008CC       .text init_co__11daObjVtil_cFv */
void daObjVtil_c::init_co() {
    mStts.Init(200, 0xff, this);
    mCyl.Set(M_co_cyl_data);
    mCyl.SetStts(&mStts);
    mCyl.SetC(current.pos);
    *mCyl.GetAtVecP() = cXyz::Zero;
    *mCyl.GetTgVecP() = cXyz::Zero;
    mCyl.OnTgShield();
}

/* 000008CC-000009A4       .text init_bgc__11daObjVtil_cFv */
void daObjVtil_c::init_bgc() {
    mAcchCir.SetWall(30.0f, 56.0f);
#if VERSION == VERSION_DEMO
    mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), fopAcM_GetAngle_p(this), fopAcM_GetShapeAngle_p(this));
#else
    mAcch.Set(&current.pos, &old.pos, this, 1, &mAcchCir, &speed, &current.angle, &shape_angle);
#endif
    mAcch.ClrWaterNone();
    mAcch.ClrRoofNone();
    mAcch.SetRoofCrrHeight(160.0f);
    mAcch.OnSeaCheckOn();
    mAcch.OnSeaWaterHeight();
    fopAcM_posMoveF(this, NULL);
    mAcch.CrrPos(*dComIfG_Bgsp());
    mAcch.ClrGroundLanding();
}

/* 000009A4-000009CC       .text renew_attention_pos__11daObjVtil_cFv */
void daObjVtil_c::renew_attention_pos() {
    attention_info.position.x = current.pos.x;
    attention_info.position.y = 160.0f + current.pos.y;
    attention_info.position.z = current.pos.z;
}

/* 000009CC-000009F8       .text make_smoke__11daObjVtil_cFv */
void daObjVtil_c::make_smoke() {
    daObj::make_land_effect(this, &mAcch.m_gnd, 1.0f);
}

/* 000009F8-00000A88       .text se_smoke__11daObjVtil_cFv */
void daObjVtil_c::se_smoke() {
    u32 sound = dComIfG_Bgsp()->GetMtrlSndId(mAcch.m_gnd);
    mDoAud_seStart(JA_SE_OBJ_PUT_STONE, &eyePos, sound, dComIfGp_getReverb(DEMO_SELECT(fopAcM_GetRoomNo(this), current.roomNo)));
}

/* 00000A88-00000AD8       .text make_splash__11daObjVtil_cFv */
void daObjVtil_c::make_splash() {
    cXyz pos(current.pos.x, mAcch.GetSeaHeight(), current.pos.z);
    fopKyM_createWpillar(&pos, 1.0f, 0.75f, 0);
}

/* 00000AD8-00000BCC       .text se_splash__11daObjVtil_cFv */
void daObjVtil_c::se_splash() {
    cBgS_PolyInfo* polys[2] = {NULL, NULL};
    polys[0] = &mAcch.m_wtr;
    polys[1] = &mAcch.m_gnd;
    u32 sound = 19;
    for (int i = 0; i < 2; i++) {
        int index = polys[i]->GetBgIndex();
        if (index >= 0 && index < 256) {
            sound = dComIfG_Bgsp()->GetMtrlSndId(*polys[i]);
            break;
        }
    }
    mDoAud_seStart(JA_SE_OBJ_FALL_WATER_S, &eyePos, sound, dComIfGp_getReverb(DEMO_SELECT(fopAcM_GetRoomNo(this), current.roomNo)));
    set_sound(125, 5);
}

/* 00000BCC-00000C20       .text set_sound__11daObjVtil_cCFii */
void daObjVtil_c::set_sound(int radius, int type) const {
    dKy_Sound_set(current.pos, radius, fopAcM_GetID((void*)this), type);
}

/* 00000C20-00000CA0       .text to_wait_mode__11daObjVtil_cFv */
void daObjVtil_c::to_wait_mode() {
    mCyl.OffAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OnCoSetBit();
    speedF = 0.0f;
    gravity = -6.0f;
    mStts.Init(200, 0xff, this);
    mMode = 0;
}

/* 00000CA0-00000D70       .text mode_wait__11daObjVtil_cFv */
void daObjVtil_c::mode_wait() {
    if (fopAcM_checkCarryNow(this)) {
        to_carry_mode();
    } else {
        if (mAcch.ChkGroundLanding()) {
            if (mFirstLanding == 0) {
                make_smoke();
                se_smoke();
                make_vib();
            } else {
                mFirstLanding = 0;
            }
            mAcch.ClrGroundLanding();
        }
        if (mAcch.ChkGroundHit()) {
            attention_info.flags |= fopAc_Attn_ACTION_CARRY_e;
        } else {
            attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
        }
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
    }
    mPrevSpeedY = speed.y;
    mAcch.CrrPos(*dComIfG_Bgsp());
}

/* 00000D70-00000DAC       .text to_carry_mode__11daObjVtil_cFv */
void daObjVtil_c::to_carry_mode() {
    mCyl.OffAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OffCoSetBit();
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    mMode = 1;
}

/* 00000DAC-00000E7C       .text mode_carry__11daObjVtil_cFv */
void daObjVtil_c::mode_carry() {
    cXyz pos = current.pos;
    cXyz prevSpeed = speed;
    if (!fopAcM_checkCarryNow(this)) {
        if (speedF > 0.0f) {
            to_throw_mode();
        } else {
            to_wait_mode();
        }
    }
    mPrevSpeedY = speed.y;
    mAcch.CrrPos(*dComIfG_Bgsp());
    current.pos = pos;
    speed = prevSpeed;
}

/* 00000E7C-00000F14       .text to_throw_mode__11daObjVtil_cFv */
void daObjVtil_c::to_throw_mode() {
    mCyl.OnAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OnCoSetBit();
    mAcch.ClrRoofNone();
    mAcch.ClrWallNone();
    mAcch.ClrGrndNone();
    mAcch.ClrWaterNone();
    mAcch.OnLineCheck();
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    speed.y = 27.0f;
    speedF = 36.0f;
    gravity = -6.0f;
    mMode = 2;
}

/* 00000F14-00000FDC       .text calc_throw__11daObjVtil_cCFPfPfPf */
void daObjVtil_c::calc_throw(float* grav, float* drag, float* drag2) const {
    if (mAcch.ChkSeaIn()) {
        f32 sea_height = mAcch.GetSeaHeight();
        f32 distance = current.pos.y - sea_height;
        f32 depth;
        if (distance >= 0.0f) {
            depth = 0.0f;
        } else if (distance <= -160.0f) {
            depth = 0.5f;
        } else {
            depth = 0.003125f * -distance;
        }
        f32 above = 1.0f - depth;
        *drag = 0.2f * depth + 0.002f * above;
        *drag2 = 0.02f * depth + 0.0002f * above;
        *grav = -6.0f + 4.0f * depth;
    } else {
        *drag = 0.002f;
        *drag2 = 0.0002f;
        *grav = -6.0f;
    }
}

/* 00000FDC-00001054       .text mode_throw__11daObjVtil_cFv */
void daObjVtil_c::mode_throw() {
    f32 grav, drag, drag2;
    calc_throw(&grav, &drag, &drag2);
    gravity = grav;
    daObj::posMoveF_stream(this, mStts.GetCCMoveP(), &cXyz::Zero, drag, drag2);
    mPrevSpeedY = speed.y;
    mAcch.CrrPos(*dComIfG_Bgsp());
}

/* 00001054-000011C8       .text to_sink_mode__11daObjVtil_cFv */
void daObjVtil_c::to_sink_mode() {
    mCyl.OffAtSetBit();
    mCyl.OnTgSetBit();
    mCyl.OnCoSetBit();
    mAcch.SetRoofNone();
    mAcch.ClrWallNone();
    mAcch.ClrGrndNone();
    mAcch.ClrWaterNone();
    mAcch.OnLineCheck();
    gravity = -2.0f;
    f32 magnitude = std::sqrtf(speed.y * speed.y + speedF * speedF);
    if (magnitude > 30.0) {
        f32 ratio = 30.0f / magnitude;
        speed *= ratio;
        speedF *= ratio;
    }
    make_splash();
    se_splash();
    attention_info.flags &= ~fopAc_Attn_ACTION_CARRY_e;
    mMode = 3;
}

/* 000011C8-0000122C       .text mode_sink__11daObjVtil_cFv */
void daObjVtil_c::mode_sink() {
    daObj::posMoveF_stream(this, mStts.GetCCMoveP(), &cXyz::Zero, 0.2f, 0.02f);
    mPrevSpeedY = speed.y;
    mAcch.CrrPos(*dComIfG_Bgsp());
}

/* 0000122C-000012F8       .text hit_co__11daObjVtil_cFv */
void daObjVtil_c::hit_co() {
    mStts.Move();
    if (mCyl.ChkAtHit()) {
        mCyl.ClrAtHit();
        speedF *= 0.8f;
    } else if (mCyl.ChkTgHit()) {
        tell_agb_attack();
        daObj::HitSeStart(&eyePos, current.roomNo, &mCyl, 13);
        set_sound(150, 5);
        daObj::HitEff_hibana(this, &mCyl);
        mCyl.ClrTgHit();
    }
}

/* 000012F8-00001330       .text check_sink__11daObjVtil_cFv */
bool daObjVtil_c::check_sink() {
    bool result = false;
    if (mAcch.ChkSeaIn() && mAcch.GetSeaHeight() > 70.0f + current.pos.y) {
        result = true;
    }
    return result;
}

/* 00001330-00001374       .text check_sink_end__11daObjVtil_cFv */
bool daObjVtil_c::check_sink_end() {
    bool result = false;
    if (mAcch.ChkSeaIn() && mAcch.GetSeaHeight() > 50.0f + (160.0f + current.pos.y)) {
        result = true;
    }
    return result;
}

inline void daObjVtil_c::camera_off() {
    dComIfGp_getCamera(0)->mCamera.ForceLockOff(base.base.mBsPcId);
}

/* 00001374-000014E0       .text hit_bg__11daObjVtil_cFv */
void daObjVtil_c::hit_bg() {
    bool ground = mAcch.ChkGroundHit();
    bool sinking = check_sink();
    if (mMode == 0) {
        if (sinking) {
            to_sink_mode();
        }
    } else if (mMode == 2) {
        if (ground || mAcch.ChkWallHit() || mAcch.ChkRoofHit()) {
#if VERSION == VERSION_DEMO
            if (mPrevSpeedY < fopAcM_GetGravity(this) - 1.0f) {
#else
            if (mPrevSpeedY < gravity - 1.0f) {
#endif
                speedF *= 0.6f;
            } else {
                if (ground) {
                    make_smoke();
                    se_smoke();
                    make_vib();
                }
                speedF = 0.0f;
                to_wait_mode();
                camera_off();
            }
        } else if (sinking) {
            to_sink_mode();
        }
    } else if (mMode == 3) {
        if (check_sink_end()) {
            tell_agb_sink();
            mDeleteState = 1;
        } else if (!sinking) {
            to_wait_mode();
            camera_off();
        }
    }
}

/* 000014E0-0000153C       .text make_vib__11daObjVtil_cFv */
void daObjVtil_c::make_vib() {
    int strength = check_circle() & 1;
    dComIfGp_getVibration().StartShock(strength + 1, 1, cXyz(0.0f, 1.0f, 0.0f));
}

/* 0000153C-00001634       .text check_circle__11daObjVtil_cFv */
BOOL daObjVtil_c::check_circle() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz delta = player->current.pos - current.pos;
    f32 distance = delta.absXZ();
    BOOL result = FALSE;
    if (distance <= 190.0f) {
        result = TRUE;
    }
    return result;
}

/* 00001634-000017CC       .text _execute__11daObjVtil_cFv */
bool daObjVtil_c::_execute() {
    static void (daObjVtil_c::*mode_exe[])() = {
        &daObjVtil_c::mode_wait, &daObjVtil_c::mode_carry,
        &daObjVtil_c::mode_throw, &daObjVtil_c::mode_sink,
    };
    hit_co();
    hit_bg();
    if (mDeleteState == 0) {
        (this->*mode_exe[mMode])();
        renew_attention_pos();
        init_mtx();
        mCyl.MoveCAtTg(current.pos);
        dComIfG_Ccsp()->Set(&mCyl);
        mLight.mPos = current.pos;
        mLight.mPos.y += 70.0f;
        mLight.mColor.r = 148;
        mLight.mColor.g = 127;
        mLight.mColor.b = 52;
        mLight.mPower = 230.0f;
        mLight.mFluctuation = 250.0f;
    }
    if (mDeleteState == 1) {
        fopAcM_delete(this);
        mDeleteState = 2;
    }
    return true;
}

/* 000017CC-00001864       .text _draw__11daObjVtil_cFv */
bool daObjVtil_c::_draw() {
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mDoExt_modelUpdateDL(mpModel);
    if (!fopAcM_checkCarryNow(this)) {
        dComIfGd_setSimpleShadow2(&current.pos, mAcch.GetGroundH(), 57.0f, mAcch.m_gnd);
    }
    return true;
}

namespace {
/* 00001864-00001884       .text Mthd_Create__26@unnamed@d_a_obj_vtil_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjVtil_c*)i_this)->_create();
}

/* 00001884-000018A8       .text Mthd_Delete__26@unnamed@d_a_obj_vtil_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjVtil_c*)i_this)->_delete();
}

/* 000018A8-000018CC       .text Mthd_Execute__26@unnamed@d_a_obj_vtil_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjVtil_c*)i_this)->_execute();
}

/* 000018CC-000018F0       .text Mthd_Draw__26@unnamed@d_a_obj_vtil_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjVtil_c*)i_this)->_draw();
}

/* 000018F0-000018F8       .text Mthd_IsDelete__26@unnamed@d_a_obj_vtil_cpp@FPv */
BOOL Mthd_IsDelete(void*) {
    return TRUE;
}

static actor_method_class Vtil_Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace

actor_process_profile_definition g_profile_Obj_Vtil = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0008,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Vtil_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjVtil_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Vtil_e,
    /* Actor SubMtd */ &Vtil_Mthd_Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
