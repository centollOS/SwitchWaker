/**
 * d_a_obj_trap.cpp
 * Enemy - Blade Trap (normal)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_trap.h"
#include "d/actor/d_a_obj_movebox.h"
#include "d/d_com_inf_game.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_a_obj.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Trap.h"

const f32 daObjTrap_c::M_speed_table[3] = {50.0f, 80.0f, 100.0f};
const s16 daObjTrap_c::M_wait_f_table[3] = {30, 10, 0};
const char daObjTrap_c::M_arcname[] = "Trap";

static dCcD_SrcCyl l_daObjTrap_cyl_data = {
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_SPIKE,
        /* SrcObjAt  Atp     */ 1,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
        /* SrcObjTg  Type    */ 0,
        /* SrcObjTg  SPrm    */ 0,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ 0,
        /* SrcGObjAt Spl     */ 1,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, -40.0f, 0.0f}, 155.0f, 100.0f}},
};

/* 000000EC-0000010C       .text solidHeapCB__11daObjTrap_cFP10fopAc_ac_c */
BOOL daObjTrap_c::solidHeapCB(fopAc_ac_c* actor) {
    return ((daObjTrap_c*)actor)->create_heap();
}

/* 0000010C-000002A4       .text create_heap__11daObjTrap_cFv */
BOOL daObjTrap_c::create_heap() {
    J3DModelData* mdl_data;
    BOOL success = FALSE;

    mdl_data = static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_TRAP_BDL_HTORA1_e));
    JUT_ASSERT(0x163, mdl_data != NULL);

    if (mdl_data != NULL) {
        mpModel = mDoExt_J3DModel__create(mdl_data, 0x80000, 0x11000222);
        if (mpModel != NULL) {
            J3DAnmTextureSRTKey* btk_data = static_cast<J3DAnmTextureSRTKey*>(dComIfG_getObjectRes(M_arcname, dRes_INDEX_TRAP_BTK_HTORA1_e));
            JUT_ASSERT(0x16C, btk_data != NULL);
            if (btk_data != NULL && mBtk.init(mdl_data, btk_data, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) {
                mpBgW = dBgW_NewSet((cBgD_t*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_TRAP_DZB_HTORA1_e), cBgW::MOVE_BG_e, &mpModel->getBaseTRMtx());
                if (mpBgW != NULL) {
                    success = TRUE;
                }
            }
        }
    }

    return success;
}

/* 000002A4-00000510       .text _create__11daObjTrap_cFv */
cPhs_State daObjTrap_c::_create() {
    fopAcM_SetupActor(this, daObjTrap_c);
    cPhs_State phase = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        phase = cPhs_ERROR_e;
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0)) {
            mPathNo = fopAcM_GetParam(this);
            if (mPathNo != 0xFF) {
                mpPath = dPath_GetRoomPath(mPathNo, fopAcM_GetRoomNo(this));
                if (mpPath != NULL && mpPath->m_points != NULL) {
                    mSpeedType = (fopAcM_GetParam(this) >> 8) & 0xF;
                    if (mSpeedType == 0xF) {
                        mSpeedType = 0;
                    }
                    mMoveSpeed = M_speed_table[mSpeedType];
                    mWaitFrames = M_wait_f_table[mSpeedType];
                    current.pos.set(mpPath->m_points[0].m_position.x, current.pos.y, mpPath->m_points[0].m_position.z);
                    set_move_info();
                    cXyz delta = mTarget - mStart;
                    mPathLength = delta.absXZ();
                    mNextPos = current.pos;
                    init_mtx();
                    mStts.Init(0, 255, this);
                    mCyl.SetStts(&mStts);
                    mCyl.Set(l_daObjTrap_cyl_data);
                    set_co_pos();
                    dComIfG_Bgsp()->Regist(mpBgW, this);
                    get_ground();
                    phase = cPhs_COMPLEATE_e;
                } else {
                    mPathNo = 0xFF;
                    phase = cPhs_ERROR_e;
                }
            }
        }
    }
    return phase;
}

/* 00000DF0-00000E84       .text _delete__11daObjTrap_cFv */
bool daObjTrap_c::_delete() {
#if VERSION == VERSION_DEMO
    if (mpBgW != NULL && mpBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpBgW);
    }
#else
    if (heap != NULL && mpBgW != NULL && mpBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpBgW);
        mpBgW = NULL;
    }
#endif
    dComIfG_resDelete(&mPhase, M_arcname);
    return true;
}

/* 00000E84-00000F20       .text init_mtx__11daObjTrap_cFv */
void daObjTrap_c::init_mtx() {
    mpModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpModel->calc();
}

/* 00000F20-00000F70       .text set_co_pos__11daObjTrap_cFv */
void daObjTrap_c::set_co_pos() {
    cXyz pos = current.pos;
    pos.y -= 40.0f;
    mCyl.SetC(pos);
}

/* 00000F70-00000FF4       .text get_ground__11daObjTrap_cFv */
void daObjTrap_c::get_ground() {
    cXyz pos(current.pos.x, 50.0f + current.pos.y, current.pos.z);
    mGndChk.SetPos(&pos);
    mGndChk.SetActorPid(fopAcM_GetID(this));
    mGroundY = dComIfG_Bgsp()->GroundCross(&mGndChk);
}

/* 00000FF4-0000112C       .text circle_search__11daObjTrap_cFv */
BOOL daObjTrap_c::circle_search() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz delta = player->current.pos - current.pos;
    f32 distance = delta.absXZ();
    BOOL found = FALSE;
    if (distance <= 400.0f && mDirectionValid == 1 && mDirection.x * delta.x + mDirection.z * delta.z >= 0.0f) {
        found = TRUE;
    }
    return found;
}

/* 0000112C-0000122C       .text set_move_info__11daObjTrap_cFv */
void daObjTrap_c::set_move_info() {
    mStart.set(mpPath->m_points[mPathPoint].m_position.x, current.pos.y, mpPath->m_points[mPathPoint].m_position.z);
#if VERSION == VERSION_DEMO
    int next = (mPathPoint + 1) & 1;
    mTarget.set(mpPath->m_points[next].m_position.x, current.pos.y, mpPath->m_points[next].m_position.z);
#else
    mTarget.set(mpPath->m_points[(mPathPoint + 1) & 1].m_position.x, current.pos.y, mpPath->m_points[(mPathPoint + 1) & 1].m_position.z);
#endif
    mDirection = mTarget - mStart;
    mDirectionValid = mDirection.normalizeRS();
    mMove = cXyz::Zero;
    mFastMove = mDirection * 100.0f;
}

/* 0000122C-000013E4       .text check_arrival__11daObjTrap_cFv */
BOOL daObjTrap_c::check_arrival() {
    cXyz progress = mNextPos - mStart;
    cXyz distance = mTarget - mStart;
    return progress.absXZ() >= distance.absXZ();
}

/* 000013E4-000018E4       .text check_wall__11daObjTrap_cFv */
cXyz daObjTrap_c::check_wall() {
    static dBgS_ObjLinChk wall_work;
    static const s16 angleY[3] = {0, 0x4000, -0x4000};
    static const f32 transA[3] = {0.0f, 145.0f, 145.0f};
    cXyz forward = mDirection * 150.0f;
    cXyz start, end, hit, offset;
    cXyz height = cXyz::BaseY * 75.0f;
    cXyz result = cXyz::Zero;
    for (int i = 0; i < 3; ++i) {
        mDoMtx_stack_c::YrotS(angleY[i]);
        mDoMtx_stack_c::multVec(&mDirection, &offset);
        offset *= transA[i];
        offset += height;
        start = current.pos + offset;
        end = start + mMove;
        end += forward;
        wall_work.Set(&start, &end, this);
        wall_work.SetActorPid(fopAcM_GetID(this));
        if (dComIfG_Bgsp()->LineCross(&wall_work)) {
            hit = wall_work.GetCross();
            hit -= start;
            if (result == cXyz::Zero || result.absXZ() > hit.absXZ()) {
                result = hit + current.pos - forward;
            }
        }
    }
    return result;
}

/* 00001C88-00001D7C       .text check_block_target_pos__11daObjTrap_cFP4cXyz */
BOOL daObjTrap_c::check_block_target_pos(cXyz* pos) {
    cXyz delta = *pos - current.pos;
    cXyz side;
    f32 distance = mDirection.x * delta.x + mDirection.z * delta.z;
    BOOL found = FALSE;
    if (distance >= 0.0f && distance < 150.0f + mPathLength) {
        mDoMtx_stack_c::YrotS(0x4000);
        mDoMtx_stack_c::multVec(&mDirection, &side);
        if (std::fabsf(side.x * delta.x + side.z * delta.z) < 225.0f) {
            found = TRUE;
        }
    }
    return found;
}

/* 00001D7C-000023D4       .text check_block__11daObjTrap_cF4cXyz */
cXyz daObjTrap_c::check_block(cXyz initial) {
    static dBgS_ObjLinChk wall_work;
    static s16 angleY[2] = {0x4000, -0x4000};
    cXyz forward = mDirection * 150.0f;
    cXyz start, end, hit, offset;
    cXyz result = initial;
    cXyz boxTarget;
    cXyz height = cXyz::BaseY * 75.0f;
    for (int i = 0; i < 2; ++i) {
        mDoMtx_stack_c::YrotS(angleY[i]);
        mDoMtx_stack_c::multVec(&mDirection, &offset);
        offset *= 153.0f;
        offset += height;
        start = current.pos + offset;
        end = start + mMove;
        end += forward;
        wall_work.Set(&start, &end, this);
        wall_work.SetActorPid(fopAcM_GetID(this));
        if (dComIfG_Bgsp()->LineCross(&wall_work)) {
            fopAc_ac_c* actor = dComIfG_Bgsp()->GetActorPointer(wall_work);
            if (actor != NULL && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_Obj_Movebox_e) {
                daObjMovebox::Act_c* box = (daObjMovebox::Act_c*)actor;
                if (box->chk_walk(&boxTarget) && check_block_target_pos(&boxTarget)) {
                    hit = wall_work.GetCross();
                    hit -= start;
                    if (result == cXyz::Zero || result.absXZ() > hit.absXZ()) {
                        result = hit + current.pos - forward;
                    }
                }
            }
        }
    }
    return result;
}

/* 000023D4-0000250C       .text set_vib_mode__11daObjTrap_cFv */
void daObjTrap_c::set_vib_mode() {
    mOriginalPitch = shape_angle.x;
    mVibrateTimer = 6;
    mBounceAmplitude = 35.0f;
    mBounceTimer = 16;
    mBounceOffset = cXyz::Zero;
    mMode = 2;
    cXyz pos = mDirection * 150.0f;
    pos += current.pos;
    cXyz direction = mDirection * -1.0f;
    pos.y += 50.0f;
    daObj::HitEff_hibana(&pos, &direction);
    if (mVibrateTimer != 0) {
        vibrate();
        --mVibrateTimer;
    }
    if (mBounceTimer != 0) {
        bound();
        --mBounceTimer;
    }
}

/* 0000250C-0000255C       .text vibrate__11daObjTrap_cFv */
void daObjTrap_c::vibrate() {
    s16 angle = mVibrateTimer * 0x5555;
    shape_angle.x = 288.0f * cM_ssin(angle);
}

/* 0000255C-00002678       .text bound__11daObjTrap_cFv */
void daObjTrap_c::bound() {
    cXyz offset = mDirection * -1.0f;
    mNextPos -= mBounceOffset;
    s16 angle = mBounceTimer * 0x4000;
    offset *= std::fabsf((s16)(mBounceAmplitude * cM_ssin(angle)));
    mNextPos += offset;
    mBounceOffset = offset;
    cLib_addCalc(&mBounceAmplitude, 0.0f, 0.17f, 35.0f, 1.0f);
}

/* 00002678-0000270C       .text set_shine__11daObjTrap_cFv */
void daObjTrap_c::set_shine() {
    start_shine();
    if (cXyz::BaseX.inprod(mDirection) >= 0.0f) {
        mBtk.setFrame(35.0f);
        mBtk.setPlaySpeed(1.0f);
    } else {
        mBtk.setFrame(46.0f);
        mBtk.setPlaySpeed(-1.0f);
    }
}

/* 0000270C-00002758       .text shine_move__11daObjTrap_cFv */
void daObjTrap_c::shine_move() {
    if (check_shine() && mBtk.play()) {
        stop_shine();
    }
}

/* 00002758-00002CB0       .text _execute__11daObjTrap_cFv */
bool daObjTrap_c::_execute() {
    mNextPos = current.pos;
    cXyz collision;
    switch (mMode) {
    case 0: {
        int arrived = cLib_chasePosXZ(&mNextPos, mTarget, mMoveSpeed);
        mMove = mNextPos - current.pos;
        collision = check_wall();
        collision = check_block(collision);
        if (collision != cXyz::Zero) {
            mNextPos = collision;
            set_vib_mode();
            set_shine();
            mDoAud_seStart(JA_SE_OBJ_WDUN_TRAP_STOP, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        } else if (arrived == 1) {
            set_vib_mode();
            set_shine();
            mDoAud_seStart(JA_SE_OBJ_WDUN_TRAP_STOP, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        } else if (circle_search() == 1) {
            mMode = 1;
        } else {
            mDoAud_seStart(JA_SE_OBJ_WDUN_TRAP_MOVE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        }
        break;
    }
    case 1:
        cLib_addCalcPosXZ(&mMove, mFastMove, 0.06f, 100.0f, 1.0f);
        mNextPos += mMove;
        collision = check_wall();
        collision = check_block(collision);
        if (collision != cXyz::Zero) {
            mNextPos = collision;
            set_vib_mode();
            set_shine();
            mDoAud_seStart(JA_SE_OBJ_WDUN_TRAP_STOP, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        } else if (check_arrival() == 1) {
            mNextPos = mTarget;
            set_vib_mode();
            set_shine();
            mDoAud_seStart(JA_SE_OBJ_WDUN_TRAP_STOP, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        } else {
            mDoAud_seStart(JA_SE_OBJ_WDUN_TRAP_MOVE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        }
        break;
    case 2:
        if (mVibrateTimer != 0) {
            vibrate();
            --mVibrateTimer;
        }
        if (mBounceTimer != 0) {
            bound();
            --mBounceTimer;
        }
        if (mVibrateTimer == 0) {
            shape_angle.x = mOriginalPitch;
            if (mBounceTimer == 0) {
                mWaitTimer = mWaitFrames;
                mMode = 3;
            }
        }
        break;
    case 3:
        if (mWaitTimer > 0) {
            --mWaitTimer;
        }
        if (mWaitTimer == 0) {
            mPathPoint = (mPathPoint + 1) & 1;
            set_move_info();
            mMode = 0;
        }
        break;
    }
    current.pos = mNextPos;
    shine_move();
    get_ground();
    init_mtx();
    set_co_pos();
    dComIfG_Ccsp()->Set(&mCyl);
#if VERSION == VERSION_DEMO
    if (mpBgW != NULL && mpBgW->ChkUsed()) {
#else
    if (heap != NULL && mpBgW != NULL && mpBgW->ChkUsed()) {
#endif
        mpBgW->Move();
    }
    return true;
}

/* 00002CB0-00002D54       .text _draw__11daObjTrap_cFv */
bool daObjTrap_c::_draw() {
    g_env_light.settingTevStruct(1, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mBtk.entry(mpModel->getModelData());
    mDoExt_modelUpdateDL(mpModel);
    dComIfGd_setSimpleShadow2(&current.pos, mGroundY, 150.0f, mGndChk, 0, 1.0f);
    return true;
}

namespace {
/* 00002D54-00002D74       .text Mthd_Create__26@unnamed@d_a_obj_trap_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjTrap_c*)i_this)->_create();
}

/* 00002D74-00002D98       .text Mthd_Delete__26@unnamed@d_a_obj_trap_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjTrap_c*)i_this)->_delete();
}

/* 00002D98-00002DBC       .text Mthd_Execute__26@unnamed@d_a_obj_trap_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjTrap_c*)i_this)->_execute();
}

/* 00002DBC-00002DE0       .text Mthd_Draw__26@unnamed@d_a_obj_trap_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjTrap_c*)i_this)->_draw();
}

/* 00002DE0-00002DE8       .text Mthd_IsDelete__26@unnamed@d_a_obj_trap_cpp@FPv */
BOOL Mthd_IsDelete(void*) {
    return TRUE;
}

static actor_method_class Trap_Mthd_Table = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace

actor_process_profile_definition g_profile_Obj_Trap = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Trap_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjTrap_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Trap_e,
    /* Actor SubMtd */ &Trap_Mthd_Table,
    /* Status       */ fopAcStts_SHOWMAP_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
