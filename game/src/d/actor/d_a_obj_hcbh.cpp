/**
 * d_a_obj_hcbh.cpp
 * Object - Tower of the Gods - Pillar with Companion Statue's face on it.
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_hcbh.h"
#include "d/d_com_inf_game.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "res/Object/Hcbh.h"

namespace {
const char l_arcname[] = "Hcbh";
const dCcD_SrcCyl l_cyl_src = {
    {0, 0, 0, 0, 0x04010028, 9, 0x39, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0},
    {{{0.0f, 0.0f, 0.0f}, 70.0f, 556.0f}},
};
const dCcD_SrcSph l_sph_src = {
    {0, 0x800, 1, 0xF, 0, 0, 0x79, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0},
    {{{0.0f, 0.0f, 0.0f}, 35.0f}},
};
const Vec l_co_sph_offset[6] = {
    {0.0f, 157.0f, 0.0f}, {0.0f, 227.0f, 0.0f},
    {0.0f, 297.0f, 0.0f}, {0.0f, 367.0f, 0.0f},
    {0.0f, 437.0f, 0.0f}, {0.0f, 507.0f, 0.0f},
};
}

#if VERSION == VERSION_DEMO
class daObjHcbh_HIO_c : public JORReflexible {
public:
    daObjHcbh_HIO_c();
    virtual ~daObjHcbh_HIO_c() {}

    void genMessage(JORMContext* ctx) { UNUSED(ctx); }

public:
    /* 0x04 */ s8 mNo;
    /* 0x08 */ f32 mCylR;
    /* 0x0C */ f32 mCylH;
    /* 0x10 */ f32 mFallAccel;
    /* 0x14 */ f32 mAngleSpeedBase;
    /* 0x18 */ f32 mAngleSpeedScale;
    /* 0x1C */ f32 mMaxAngle;
    /* 0x20 */ f32 mGroundOffset;
    /* 0x24 */ f32 mPieceAccel;
    /* 0x28 */ f32 mPieceFloor;
    /* 0x2C */ f32 mPieceTimerScale;
    /* 0x30 */ f32 mPieceTimerBase;
    /* 0x34 */ f32 mPieceAngleSpeedBase;
    /* 0x38 */ f32 mPieceAngleSpeedScale;
    /* 0x3C */ f32 m3C;
    /* 0x40 */ u8 m40;
    /* 0x41 */ u8 mParticle;
    /* 0x42 */ u8 m42;
};  // Size: 0x44

/* 000000EC-0000018C       .text __ct__15daObjHcbh_HIO_cFv */
daObjHcbh_HIO_c::daObjHcbh_HIO_c() {
    mNo = -1;
    mCylR = 70.0f;
    mCylH = 589.0f;
    mFallAccel = -0.011f;
    mAngleSpeedBase = 4.0f;
    mAngleSpeedScale = 127.0f;
    mMaxAngle = 16384.0f;
    mGroundOffset = -10.0f;
    mPieceAccel = -0.01f;
    mPieceFloor = -100.0f;
    mPieceTimerScale = 15.0f;
    mPieceTimerBase = 0.0f;
    mPieceAngleSpeedBase = 4.0f;
    mPieceAngleSpeedScale = 512.0f;
    m3C = 8.0f;
    m40 = 0;
    mParticle = 1;
    m42 = 0;
}

static daObjHcbh_HIO_c l_HIO;
#endif

/* 00000078-000000D8       .text chk_appear__11daObjHcbh_cFv */
bool daObjHcbh_c::chk_appear() {
    s32 sw = param_get_swSave();
    bool appear = true;
    if (sw != 0xFF) {
        appear = fopAcM_isSwitch(this, sw) == FALSE;
    }
    return appear;
}

/* 000000D8-0000029C       .text set_mtx__11daObjHcbh_cFv */
void daObjHcbh_c::set_mtx() {
    f32 offset = 50.0f * cM_ssin(mAngle);
    mDoMtx_stack_c::transS(current.pos.x, mHeight, current.pos.z);
    mDoMtx_stack_c::YrotM(mBreakAngle);
    mDoMtx_stack_c::transM(0.0f, offset, 0.0f);
    mDoMtx_stack_c::XrotM(mAngle);
    mDoMtx_stack_c::transM(0.0f, -offset, 0.0f);
    mDoMtx_stack_c::YrotM(-mBreakAngle);
    mDoMtx_stack_c::XYZrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    for (int i = 0; i < 4; ++i) {
        mDoMtx_stack_c::transS(mPiecePos[i]);
        mDoMtx_stack_c::YrotM(mPieceYAngle[i]);
        mDoMtx_stack_c::XrotM(mPieceAngle[i]);
        mDoMtx_stack_c::YrotM(-mPieceYAngle[i]);
        mDoMtx_stack_c::XYZrotM(shape_angle);
        mpPieces[i]->setBaseTRMtx(mDoMtx_stack_c::get());
    }
}

/* 0000029C-0000030C       .text init_mtx__11daObjHcbh_cFv */
void daObjHcbh_c::init_mtx() {
    mpModel->setBaseScale(scale);
    for (int i = 0; i < 4; ++i) {
        mpPieces[i]->setBaseScale(scale);
    }
    set_mtx();
}

/* 0000030C-00000330       .text solidHeapCB__11daObjHcbh_cFP10fopAc_ac_c */
BOOL daObjHcbh_c::solidHeapCB(fopAc_ac_c* actor) {
    return ((daObjHcbh_c*)actor)->create_heap();
}

/* 00000330-00000500       .text create_heap__11daObjHcbh_cFv */
bool daObjHcbh_c::create_heap() {
    bool success = true;
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(l_arcname, dRes_INDEX_HCBH_BDL_HCBH2_e);
    if (modelData == NULL) {
        JUT_ASSERT(DEMO_SELECT(0x1F0, 0x1F3), 0);
        success = false;
    } else {
        mpModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        static int bdl_table[4] = {dRes_INDEX_HCBH_BDL_HCBH1A_e, dRes_INDEX_HCBH_BDL_HCBH1B_e,
            dRes_INDEX_HCBH_BDL_HCBH1C_e, dRes_INDEX_HCBH_BDL_HCBH1D_e};
        for (int i = 0; i < 4; ++i) {
            modelData = (J3DModelData*)dComIfG_getObjectRes(l_arcname, bdl_table[i]);
            if (modelData == NULL) {
                success = false;
                break;
            }
            mpPieces[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        }
        mpBgW = dBgW_NewSet((cBgD_t*)dComIfG_getObjectRes(l_arcname, dRes_INDEX_HCBH_DZB_HCBH2_e), 1, &mpModel->getBaseTRMtx());
        mpPieceBgW = dBgW_NewSet((cBgD_t*)dComIfG_getObjectRes(l_arcname, dRes_INDEX_HCBH_DZB_HCBH1_e), 1, &mpPieces[0]->getBaseTRMtx());
        if (!mpModel || !mpPieces[0] || !mpPieces[1] || !mpPieces[2] || !mpPieces[3] || !mpBgW || !mpPieceBgW) {
            success = false;
        }
    }
    return success;
}

/* 00000500-000005E8       .text setup_break_condition__11daObjHcbh_cFP10fopAc_ac_c */
void daObjHcbh_c::setup_break_condition(fopAc_ac_c* actor) {
    if (actor != NULL) {
        mBreakAngle = cM_atan2s(mCyl.GetC().x - actor->current.pos.x, mCyl.GetC().z - actor->current.pos.z);
        static s16 set_anglY[4] = {-0x8000, -0x4000, 0x4000, 0};
        for (int i = 0; i < 4; ++i) {
            mPieceYAngle[i] = set_anglY[i];
            mPieceAngle[i] = 0;
#if VERSION == VERSION_DEMO
            mPieceTimer[i] = l_HIO.mPieceTimerBase + l_HIO.mPieceTimerScale * std::fabsf(cM_ssin((set_anglY[i] - mBreakAngle) >> 1));
#else
            mPieceTimer[i] = 15.0f * std::fabsf(cM_ssin((set_anglY[i] - mBreakAngle) >> 1));
#endif
        }
    }
}

/* 000005E8-00000724       .text checkCollision__11daObjHcbh_cFv */
void daObjHcbh_c::checkCollision() {
    if (mCyl.ChkTgHit()) {
        cCcD_Obj* obj = mCyl.GetTgHitObj();
        if (obj != NULL) {
            switch (obj->GetAtType()) {
            case AT_TYPE_BOMB:
                mBreakType = 3;
                setup_break_condition(mCyl.GetTgHitAc());
                break;
            case AT_TYPE_SKULL_HAMMER:
                mBreakType = 2;
                setup_break_condition(mCyl.GetTgHitAc());
                break;
            case AT_TYPE_UNK8:
                fopAc_ac_c* actor = mCyl.GetTgHitAc();
                if (actor != NULL && fopAcM_GetProfName(actor) == fpcNm_TN_e) {
                    mBreakType = 1;
                    setup_break_condition(mCyl.GetTgHitAc());
                }
                break;
            case AT_TYPE_DARKNUT_SWORD:
                mBreakType = 1;
                setup_break_condition(mCyl.GetTgHitAc());
                break;
            }
        }
        mCyl.ClrTgHit();
    }
}

/* 00000724-0000077C       .text co_hitCallback__11daObjHcbh_cFP10fopAc_ac_cP12dCcD_GObjInfP10fopAc_ac_cP12dCcD_GObjInf */
void daObjHcbh_c::co_hitCallback(fopAc_ac_c* actor, dCcD_GObjInf*, fopAc_ac_c* other, dCcD_GObjInf*) {
    if (fopAcM_IsActor(other) && fopAcM_GetProfName(other) == fpcNm_AM_e) {
        ((daObjHcbh_c*)actor)->mBreakType = 4;
    }
}

/* 0000077C-000008F8       .text particle_set__11daObjHcbh_cFv */
void daObjHcbh_c::particle_set() {
    csXyz angle(0, mBreakAngle, 0);
    dComIfGp_particle_set(0x82DD, &current.pos, NULL, NULL, 255, NULL, -1, &tevStr.mColorK0);
    dComIfGp_particle_set(0x82DE, &current.pos, &angle, NULL, 255, NULL, -1, &tevStr.mColorK0);
    if (mBreakType != 3) {
        dComIfGp_particle_set(0x82DF, &current.pos, &angle, NULL, 255, NULL, -1, &tevStr.mColorK0);
    }
    mSmoke.remove();
    dComIfGp_particle_setToon(0xA2E0, &current.pos, &angle, NULL, 160, &mSmoke, -1, &tevStr.mColorK0);
}

/* 000008F8-000009E4       .text make_item__11daObjHcbh_cFv */
void daObjHcbh_c::make_item() {
    int item = param_get_itemNo();
    int saveBit = param_get_itemSaveBitNo();
    cXyz pos = current.pos;
    csXyz angle(0, 0, 0);
    pos.y -= 30.0f;
    fopAcM_createItemFromTable(&pos, item, saveBit, fopAcM_GetHomeRoomNo(this), 0, &angle, 1, NULL);
    s32 sw = param_get_swSave();
    if (sw != 0xFF) {
        fopAcM_onSwitch(this, sw);
    }
}

/* 00000A20-00000E30       .text _create__11daObjHcbh_cFv */
cPhs_State daObjHcbh_c::_create() {
    cPhs_State phase = cPhs_ERROR_e;
    fopAcM_SetupActor(this, daObjHcbh_c);
    if (fopAcM_IsFirstCreating(this)) {
        mHeight = current.pos.y;
        mAngle = 0;
        for (int i = 0; i < 4; ++i) {
            mPiecePos[i] = current.pos;
            mPieceAngle[i] = 0;
            mPieceYAngle[i] = 0;
        }
        mAppear = chk_appear();
    }
    if (mAppear == 1) {
        phase = dComIfG_resLoad(&mPhase, l_arcname);
    }
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0x2D00)) {
            if (dComIfG_Bgsp()->Regist(mpBgW, this)) {
                phase = cPhs_ERROR_e;
            } else if (dComIfG_Bgsp()->Regist(mpPieceBgW, this)) {
                phase = cPhs_ERROR_e;
            } else {
                fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
                init_mtx();
                mAcchCir.SetWall(589.0f, 70.0f);
                mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), fopAcM_GetAngle_p(this), fopAcM_GetShapeAngle_p(this));
                mAcch.ClrWaterNone();
                mAcch.ClrRoofNone();
                mAcch.SetRoofCrrHeight(589.0f);
                mAcch.CrrPos(*dComIfG_Bgsp());
                mAcch.ClrGroundLanding();
                mStts.Init(255, 255, this);
                mCyl.Set(l_cyl_src);
                mCyl.SetStts(&mStts);
                mCyl.SetC(current.pos);
                mCyl.SetCoHitCallback(co_hitCallback);
                for (int i = 0; i < 6; ++i) {
                    mSph[i].Set(l_sph_src);
                    mSph[i].SetStts(&mStts);
                    mSph[i].SetR(70.0f);
#if VERSION == VERSION_DEMO
                    mSph[i].SetC(current.pos.x, current.pos.y + l_co_sph_offset[i].y, current.pos.z);
#else
                    mSph[i].SetC(cXyz(current.pos.x, current.pos.y + l_co_sph_offset[i].y, current.pos.z));
#endif
                }
                mSmoke.setTevStr(&tevStr);
                mSmoke.setRateOff(0);
                mSmoke.setFollowOff();
                mAction = &daObjHcbh_c::wait_act_proc;
                fopAcM_setCullSizeBox(this, -40.0f, 0.0f, -40.0f, 100.0f, 589.0f, 100.0f);
            }
        } else {
            phase = cPhs_ERROR_e;
        }
    }
#if VERSION == VERSION_DEMO
    if (l_HIO.mNo < 0) {
        l_HIO.mNo = mDoHIO_createChild("タートナックの柱", &l_HIO);
    }
#endif
    return phase;
}

/* 000011F4-000012F4       .text _delete__11daObjHcbh_cFv */
bool daObjHcbh_c::_delete() {
    if (mAppear == 1) {
        dComIfG_resDelete(&mPhase, l_arcname);
#if VERSION == VERSION_DEMO
        if (mpBgW != NULL && mpBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpBgW);
        }
        if (mpPieceBgW != NULL && mpPieceBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpPieceBgW);
        }
#else
        if (heap != NULL) {
            if (mpBgW != NULL) {
                if (mpBgW->ChkUsed()) {
                    dComIfG_Bgsp()->Release(mpBgW);
                }
                mpBgW = NULL;
            }
            if (mpPieceBgW != NULL) {
                if (mpPieceBgW->ChkUsed()) {
                    dComIfG_Bgsp()->Release(mpPieceBgW);
                }
                mpPieceBgW = NULL;
            }
        }
#endif
        mSmoke.remove();
    }
#if VERSION == VERSION_DEMO
    if (l_HIO.mNo >= 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
        l_HIO.mNo = -1;
    }
#endif
    return true;
}

/* 000012F4-000013BC       .text wait_act_proc__11daObjHcbh_cFv */
void daObjHcbh_c::wait_act_proc() {
#if VERSION == VERSION_DEMO
    mCyl.SetR(l_HIO.mCylR);
    mCyl.SetH(l_HIO.mCylH);
#endif
    checkCollision();
    if (mBreakType == 0) {
        dComIfG_Ccsp()->Set(&mCyl);
    } else {
#if VERSION == VERSION_DEMO
        if (l_HIO.mParticle == 1) {
            particle_set();
        }
#else
        particle_set();
#endif
        mDoAud_seStart(JA_SE_OBJ_TN_COLUMN_BREAK, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        mCyl.SetCoHitCallback(NULL);
        mAction = &daObjHcbh_c::fall_act_proc;
    }
}

/* 000013BC-000017D8       .text fall_act_proc__11daObjHcbh_cFv */
void daObjHcbh_c::fall_act_proc() {
    if (mAngle < 0) {
        mAngle = -0x8000;
        mAngleSpeed = 0;
    } else {
        if (mAngle > 0x2000) {
            if (mpBgW != NULL && mpBgW->ChkUsed()) {
                dComIfG_Bgsp()->Release(mpBgW);
            }
            if (mpPieceBgW != NULL && mpPieceBgW->ChkUsed()) {
                dComIfG_Bgsp()->Release(mpPieceBgW);
            }
        }
#if VERSION == VERSION_DEMO
        mAngleSpeed += (s16)(l_HIO.mAngleSpeedBase + l_HIO.mAngleSpeedScale * cM_ssin(mAngle));
#else
        mAngleSpeed += (s16)(4.0f + 127.0f * cM_ssin(mAngle));
#endif
    }
    if (mHeight < -10.0f + home.pos.y) {
        make_item();
        dComIfGp_getVibration().StartShock(8, -17, cXyz(0.0f, 1.0f, 0.0f));
        mDoAud_seStart(JA_SE_OBJ_TN_COLUMN_SMASH, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        fopAcM_delete(this);
    } else {
        mFallSpeed += -0.011f;
        f32 offset = 50.0f * cM_ssin(mAngle);
        mDoMtx_stack_c::transS(current.pos.x, mHeight, current.pos.z);
        mDoMtx_stack_c::YrotM(mBreakAngle);
        mDoMtx_stack_c::transM(0.0f, offset, 0.0f);
        mDoMtx_stack_c::XrotM(mAngle);
        mDoMtx_stack_c::transM(0.0f, -offset, 0.0f);
        mDoMtx_stack_c::YrotM(-mBreakAngle);
        mDoMtx_stack_c::XYZrotM(shape_angle);
        for (int i = 0; i < 6; ++i) {
            cXyz pos = l_co_sph_offset[i];
            mDoMtx_stack_c::multVec(&pos, &pos);
            mSph[i].MoveCAt(pos);
            dComIfG_Ccsp()->Set(&mSph[i]);
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (mPieceTimer[i] > 0) {
            --mPieceTimer[i];
        } else {
#if VERSION == VERSION_DEMO
            mPieceSpeed[i] += l_HIO.mPieceAccel;
            mPiecePos[i].y += mPieceSpeed[i];
            if (mPiecePos[i].y < l_HIO.mPieceFloor + home.pos.y) {
                mPiecePos[i].y = l_HIO.mPieceFloor + home.pos.y;
            }
            mPieceAngleSpeed[i] += (s16)(l_HIO.mPieceAngleSpeedBase + l_HIO.mPieceAngleSpeedScale * cM_ssin(mPieceAngle[i]));
#else
            mPieceSpeed[i] += -0.01f;
            mPiecePos[i].y += mPieceSpeed[i];
            if (mPiecePos[i].y < -100.0f + home.pos.y) {
                mPiecePos[i].y = -100.0f + home.pos.y;
            }
            mPieceAngleSpeed[i] += (s16)(4.0f + 512.0f * cM_ssin(mPieceAngle[i]));
#endif
            mPieceAngle[i] += mPieceAngleSpeed[i];
            if (mPieceAngle[i] > 0x4000) {
                mPieceAngle[i] = 0x4000;
            }
        }
    }
}

/* 000017D8-000018D8       .text _execute__11daObjHcbh_cFv */
bool daObjHcbh_c::_execute() {
    set_mtx();
    mStts.Move();
    mAcch.CrrPos(*dComIfG_Bgsp());
    if (mpBgW != NULL && mpBgW->ChkUsed()) {
        mpBgW->Move();
    }
    if (mpPieceBgW != NULL && mpPieceBgW->ChkUsed()) {
        mpPieceBgW->Move();
    }
    mHeight += mFallSpeed;
    mAngle += mAngleSpeed;
#if VERSION == VERSION_DEMO
    if (mAngle >= (s16)l_HIO.mMaxAngle) {
        mAngle = l_HIO.mMaxAngle;
    }
#else
    if (mAngle >= 0x4000) {
        mAngle = 0x4000;
    }
#endif
    (this->*mAction)();
    return true;
}

/* 000018D8-0000197C       .text _draw__11daObjHcbh_cFv */
bool daObjHcbh_c::_draw() {
    g_env_light.settingTevStruct(1, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mDoExt_modelUpdateDL(mpModel);
    for (int i = 0; i < 4; ++i) {
        g_env_light.setLightTevColorType(mpPieces[i], &tevStr);
        mDoExt_modelUpdateDL(mpPieces[i]);
    }
    return true;
}

/* 0000197C-0000199C       .text daObjHcbh_Create__FP10fopAc_ac_c */
static cPhs_State daObjHcbh_Create(fopAc_ac_c* i_this) {
    return ((daObjHcbh_c*)i_this)->_create();
}

/* 0000199C-000019C0       .text daObjHcbh_Delete__FP11daObjHcbh_c */
static BOOL daObjHcbh_Delete(daObjHcbh_c* i_this) {
    return ((daObjHcbh_c*)i_this)->_delete();
}

/* 000019C0-000019E4       .text daObjHcbh_Execute__FP11daObjHcbh_c */
static BOOL daObjHcbh_Execute(daObjHcbh_c* i_this) {
    return ((daObjHcbh_c*)i_this)->_execute();
}

/* 000019E4-00001A08       .text daObjHcbh_Draw__FP11daObjHcbh_c */
static BOOL daObjHcbh_Draw(daObjHcbh_c* i_this) {
    return ((daObjHcbh_c*)i_this)->_draw();
}

/* 00001A08-00001A10       .text daObjHcbh_IsDelete__FP11daObjHcbh_c */
static BOOL daObjHcbh_IsDelete(daObjHcbh_c*) {
    return TRUE;
}

static actor_method_class l_daObjHcbh_Method = {
    (process_method_func)daObjHcbh_Create,
    (process_method_func)daObjHcbh_Delete,
    (process_method_func)daObjHcbh_Execute,
    (process_method_func)daObjHcbh_IsDelete,
    (process_method_func)daObjHcbh_Draw,
};

actor_process_profile_definition g_profile_Obj_Hcbh = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Hcbh_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjHcbh_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Hcbh_e,
    /* Actor SubMtd */ &l_daObjHcbh_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
