/**
 * d_a_obj_ekskz.cpp
 * Object - Stone statue blowing strong gust of wind (Gale Isle)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_ekskz.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Ekskz.h"

const char daObjEkskz::Act_c::M_arcname[] = "Ekskz";
Mtx daObjEkskz::Act_c::M_tmp_mtx;

static dCcD_SrcCyl cyl_check_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_SKULL_HAMMER,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ 0,
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
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 100.0f,
        /* Height */ 300.0f,
    }},
};


/* 00000078-00000368       .text CreateHeap__Q210daObjEkskz5Act_cFv */
BOOL daObjEkskz::Act_c::CreateHeap() {
#if VERSION == VERSION_DEMO
    J3DModelData* model_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BDL_EKSKZ_e);
    JUT_ASSERT(144, model_data != NULL);
    mpModel = mDoExt_J3DModel__create(model_data, 0, 0x11020203);
    if (mpModel == NULL) return FALSE;
    J3DModelData* model_data_eff = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BDL_YOCWD00_e);
    JUT_ASSERT(155, model_data_eff != NULL);
    mpModelEff = mDoExt_J3DModel__create(model_data_eff, 0, 0x11020203);
    J3DAnmTransform* bck = (J3DAnmTransform*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BCK_YOCWD00_e);
    JUT_ASSERT(161, bck != NULL);
    if (!mBckAnm.init(model_data_eff, bck, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false)) return FALSE;
    J3DAnmTextureSRTKey* btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BTK_YOCWD00_e);
    JUT_ASSERT(168, btk != NULL);
    if (!mBtkAnm.init(model_data_eff, btk, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0)) return FALSE;
    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BRK_YOCWD00_e);
    JUT_ASSERT(175, brk != NULL);
    if (!mBrkAnm.init(model_data_eff, brk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) return FALSE;
    return TRUE;
#else
    J3DModelData* model_data_eff;
    J3DAnmTransform* bck;
    J3DAnmTextureSRTKey* btk;
    J3DAnmTevRegKey* brk;
    {
        J3DModelData* model_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BDL_EKSKZ_e);
        JUT_ASSERT(144, model_data != NULL);
        mpModel = mDoExt_J3DModel__create(model_data, 0, 0x11020203);
        if (mpModel == NULL) return FALSE;
    }
    model_data_eff = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BDL_YOCWD00_e);
    JUT_ASSERT(155, model_data_eff != NULL);
    mpModelEff = mDoExt_J3DModel__create(model_data_eff, 0, 0x11020203);
    if (mpModelEff == NULL) return FALSE;
    bck = (J3DAnmTransform*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BCK_YOCWD00_e);
    JUT_ASSERT(164, bck != NULL);
    if (!mBckAnm.init(model_data_eff, bck, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false)) return FALSE;
    btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BTK_YOCWD00_e);
    JUT_ASSERT(171, btk != NULL);
    if (!mBtkAnm.init(model_data_eff, btk, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0)) return FALSE;
    brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_EKSKZ_BRK_YOCWD00_e);
    JUT_ASSERT(178, brk != NULL);
    if (!mBrkAnm.init(model_data_eff, brk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) return FALSE;
    return TRUE;
#endif
}

/* 00000368-0000054C       .text Create__Q210daObjEkskz5Act_cFv */
BOOL daObjEkskz::Act_c::Create() {
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    init_mtx();
    fopAcM_setCullSizeBox(this, -4000.0f, -500.0f, -4000.0f, 4000.0f, 500.0f, 4000.0f);
    mStts.Init(0xFF, 0xFF, this);
    mCyl.Set(cyl_check_src);
    mCyl.SetC(current.pos);
    mCyl.SetStts(&mStts);
    m480 = 0;
    dKy_tevstr_init(&mTevStr, home.roomNo, 0xFF);
    s8 roomNo = fopAcM_GetRoomNo(this);
#if VERSION == VERSION_DEMO
    mpEmitter0 = dComIfGp_particle_setToon(dPa_name::ID_AK_ST_WINDOCTOPUS00, &current.pos, &current.angle, NULL, 160, &mSmokeCallback[0], roomNo);
    roomNo = fopAcM_GetRoomNo(this);
    mpEmitter1 = dComIfGp_particle_setToon(dPa_name::ID_AK_ST_WINDOCTOPUS01, &current.pos, &current.angle, NULL, 160, &mSmokeCallback[1], roomNo);
    if (mpEmitter0 != NULL) {
        mpEmitter0->setGlobalPrmColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
        mpEmitter0->setGlobalEnvColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
    }
    if (mpEmitter1 != NULL) {
        mpEmitter1->setGlobalPrmColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
        mpEmitter1->setGlobalEnvColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
    }
#else
    dComIfGp_particle_setToon(dPa_name::ID_AK_ST_WINDOCTOPUS00, &current.pos, &current.angle, NULL, 160, &mSmokeCallback[0], roomNo);
    roomNo = fopAcM_GetRoomNo(this);
    dComIfGp_particle_setToon(dPa_name::ID_AK_ST_WINDOCTOPUS01, &current.pos, &current.angle, NULL, 160, &mSmokeCallback[1], roomNo);
    if (mSmokeCallback[0].getEmitter() != NULL) {
        mSmokeCallback[0].getEmitter()->setGlobalPrmColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
        mSmokeCallback[0].getEmitter()->setGlobalEnvColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
    }
    if (mSmokeCallback[1].getEmitter() != NULL) {
        mSmokeCallback[1].getEmitter()->setGlobalPrmColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
        mSmokeCallback[1].getEmitter()->setGlobalEnvColor(mTevStr.mColorK0.r, mTevStr.mColorK0.g, mTevStr.mColorK0.b);
    }
    attention_info.distances[0] = 0x31;
    attention_info.position.y = 200.0f + current.pos.y;
    eyePos.y = attention_info.position.y;
#endif
    return TRUE;
}

/* 0000054C-00000684       .text Mthd_Create__Q210daObjEkskz5Act_cFv */
cPhs_State daObjEkskz::Act_c::Mthd_Create() {
    fopAcM_SetupActor(this, Act_c);
    mpEmitter0 = NULL;
    mpEmitter1 = NULL;
    mpSmoke0 = NULL;
    mpEmitter3 = NULL;
    if (fopAcM_isSwitch(this, prm_get_swSave())) return cPhs_STOP_e;
    cPhs_State phase_state = dComIfG_resLoad(&mPhs, M_arcname);
    if (phase_state == cPhs_COMPLEATE_e) {
        phase_state = MoveBGCreate(M_arcname, dRes_INDEX_EKSKZ_DZB_EKSKZ_e, NULL, 0x28A0);
        JUT_ASSERT(DEMO_SELECT(273, 318), (phase_state == cPhs_COMPLEATE_e) || (phase_state == cPhs_ERROR_e));
    }
    return phase_state;
}

/* 00000C00-00000C64       .text Delete__Q210daObjEkskz5Act_cFv */
BOOL daObjEkskz::Act_c::Delete() {
    for (int i = 0; i < 4; i++) mSmokeCallback[i].remove();
    return TRUE;
}

/* 00000C64-00000CBC       .text Mthd_Delete__Q210daObjEkskz5Act_cFv */
BOOL daObjEkskz::Act_c::Mthd_Delete() {
    BOOL result = MoveBGDelete();
    if (base.base.mCreateResult != cPhs_STOP_e) dComIfG_resDelete(&mPhs, M_arcname);
    return result;
}

/* 00000CBC-00000D68       .text set_mtx__Q210daObjEkskz5Act_cFv */
void daObjEkskz::Act_c::set_mtx() {
    shape_angle = current.angle;
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpModelEff->setBaseTRMtx(mDoMtx_stack_c::get());
    MTXCopy(mDoMtx_stack_c::get(), M_tmp_mtx);
}

/* 00000D68-00000DC0       .text init_mtx__Q210daObjEkskz5Act_cFv */
void daObjEkskz::Act_c::init_mtx() {
    mpModel->setBaseScale(scale);
    mpModelEff->setBaseScale(scale);
    set_mtx();
}

/* 00000DC0-00000FFC       .text Execute__Q210daObjEkskz5Act_cFPPA3_A4_f */
BOOL daObjEkskz::Act_c::Execute(Mtx** mtx) {
#if VERSION > VERSION_DEMO
    attention_info.flags |= 1;
#endif
    mBckAnm.play();
    mBtkAnm.play();
    dComIfG_Ccsp()->Set(&mCyl);
    if (m480 == 0 && mCyl.ChkTgHit()) {
        mDoAud_seStart(JA_SE_READ_RIDDLE_1);
        fopAcM_onSwitch(this, prm_get_swSave());
        m480 = 1;
#if VERSION == VERSION_DEMO
        if (mpEmitter0 != NULL) mpEmitter0->becomeInvalidEmitter();
        if (mpEmitter1 != NULL) mpEmitter1->becomeInvalidEmitter();
#else
        mSmokeCallback[0].remove();
        mSmokeCallback[1].remove();
#endif
        const u8 r = tevStr.mColorK0.r;
        const u8 g = tevStr.mColorK0.g;
        const u8 b = tevStr.mColorK0.b;
        mpSmoke0 = dComIfGp_particle_set(dPa_name::ID_AK_SN_BREAKOCTOPUSHAHEN00, &current.pos, &current.angle);
        if (mpSmoke0 != NULL) mpSmoke0->setGlobalPrmColor(r, g, b);
        mSmokeCallback[3].onWindOff();
        mSmokeCallback[3].setFollowOff();
        s8 roomNo = fopAcM_GetRoomNo(this);
#if VERSION == VERSION_DEMO
        mpEmitter3 = dComIfGp_particle_setToon(dPa_name::ID_AK_ST_BREAKOCTOPUSSMOKE00, &current.pos, &current.angle, NULL, 128, &mSmokeCallback[3], roomNo);
#else
        dComIfGp_particle_setToon(dPa_name::ID_AK_ST_BREAKOCTOPUSSMOKE00, &current.pos, &current.angle, NULL, 128, &mSmokeCallback[3], roomNo);
#endif
        mSmokeCallback[3].setTevStr(&mTevStr);
    }
    if (m480 == 1) {
        mBrkAnm.play();
        if (mBrkAnm.isStop()) fopAcM_delete(this);
    }
    set_mtx();
    *mtx = &M_tmp_mtx;
    return TRUE;
}

/* 00000FFC-00001124       .text Draw__Q210daObjEkskz5Act_cFv */
BOOL daObjEkskz::Act_c::Draw() {
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &mTevStr);
    g_env_light.setLightTevColorType(mpModelEff, &mTevStr);
    mBckAnm.entry(mpModelEff->getModelData());
    mBtkAnm.entry(mpModelEff->getModelData());
    mBrkAnm.entry(mpModelEff->getModelData());
    mDoExt_modelUpdateDL(mpModelEff);
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    dComIfGd_setListBG();
    if (m480 == 0) mDoExt_modelUpdateDL(mpModel);
    dComIfGd_setList();
    return TRUE;
}

namespace daObjEkskz {
namespace {
/* 00001124-00001144       .text Mthd_Create__Q210daObjEkskz27@unnamed@d_a_obj_ekskz_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjEkskz::Act_c*)i_this)->Mthd_Create();
}

/* 00001144-00001164       .text Mthd_Delete__Q210daObjEkskz27@unnamed@d_a_obj_ekskz_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjEkskz::Act_c*)i_this)->Mthd_Delete();
}

/* 00001164-00001184       .text Mthd_Execute__Q210daObjEkskz27@unnamed@d_a_obj_ekskz_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjEkskz::Act_c*)i_this)->MoveBGExecute();
}

/* 00001184-000011B0       .text Mthd_Draw__Q210daObjEkskz27@unnamed@d_a_obj_ekskz_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjEkskz::Act_c*)i_this)->MoveBGDraw();
}

/* 000011B0-000011DC       .text Mthd_IsDelete__Q210daObjEkskz27@unnamed@d_a_obj_ekskz_cpp@FPv */
BOOL Mthd_IsDelete(void* i_this) {
    return ((daObjEkskz::Act_c*)i_this)->MoveBGIsDelete();
}

static actor_method_class Mthd_Ekskz = {
    (process_method_func)Mthd_Create,
    (process_method_func)Mthd_Delete,
    (process_method_func)Mthd_Execute,
    (process_method_func)Mthd_IsDelete,
    (process_method_func)Mthd_Draw,
};
}; // namespace
}; // namespace daObjEkskz

actor_process_profile_definition g_profile_Obj_Ekskz = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Ekskz_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjEkskz::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Ekskz_e,
    /* Actor SubMtd */ &daObjEkskz::Mthd_Ekskz,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
