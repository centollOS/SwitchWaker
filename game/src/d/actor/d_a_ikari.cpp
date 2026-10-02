/**
 * d_a_ikari.cpp
 * Object - Forsaken Fortress - Anchors
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_ikari.h"
#include "res/Object/Ikari.h"
#include "d/d_kankyo_wether.h"
#include "d/d_s_play.h"
#include "m_Do/m_Do_mtx.h"

#if VERSION > VERSION_DEMO
static daObjIkariHIO_c l_HIO;
#endif

const char daIkari_c::M_arcname[6] = "Ikari";

#if VERSION == VERSION_DEMO
/* 00000078-00000098       .text CallbackCreateHeap__FP10fopAc_ac_c */
static BOOL CallbackCreateHeap(fopAc_ac_c* i_this) {
    return ((daIkari_c*)i_this)->CreateHeap();
}

/* 00000098-00000158       .text set_mtx__9daIkari_cFv */
void daIkari_c::set_mtx() {
    mpMorf->getModel()->setBaseScale(scale);

    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::XrotM(current.angle.x);
    mDoMtx_stack_c::YrotM(-shape_angle.y);
    mDoMtx_stack_c::YrotM(current.angle.y);

    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00000158-00000254       .text CreateHeap__9daIkari_cFv */
BOOL daIkari_c::CreateHeap() {
    static int ikari_bmd[5] = {
        dRes_INDEX_IKARI_BMD_S_IKARI2_e,
        dRes_INDEX_IKARI_BMD_S_IKARI2_e,
        dRes_INDEX_IKARI_BMD_S_IKARI2_e,
        dRes_INDEX_IKARI_BMD_S_IKARI3_e,
        dRes_INDEX_IKARI_BMD_S_IKARI4_e,
    };

    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(M_arcname, ikari_bmd[mModelType]);
    if (modelData == NULL) {
        return FALSE;
    }

    mpMorf = new mDoExt_McaMorf(modelData, NULL, NULL, NULL, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 0, NULL, 0, 0x11020203);
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
        return FALSE;
    }
    return TRUE;
}

/* 00000254-000002BC       .text SetArgData__9daIkari_cFv */
void daIkari_c::SetArgData() {
    u32 param = fopAcM_GetParam(this);

    mModelType = fopAcM_GetParamBit(param, 8, 16);
    mEnvType = param;

    if (mModelType == 0xff) {
        mModelType = 0;
    }

    if (mEnvType == 0xff) {
        scale.x = 1.0f;
        scale.y = 1.0f;
        scale.z = 1.0f;
    } else if (mEnvType == 0x01) {
        scale.x = 1.27f;
        scale.y = 1.27f;
        scale.z = 1.27f;
    }
}

/* 000002BC-000003DC       .text _create__9daIkari_cFv */
cPhs_State daIkari_c::_create() {
    cPhs_State phase = dComIfG_resLoad(&mPhs, M_arcname);

    if (phase == cPhs_COMPLEATE_e) {
        fopAcM_ct(this, daIkari_c);
        SetArgData();

        if (!fopAcM_entrySolidHeap(this, CallbackCreateHeap, 0x345C)) {
            return cPhs_ERROR_e;
        }

        set_mtx();

        f32 scaleX = scale.x;

        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        fopAcM_setCullSizeBox(this,
            -160.0f * scaleX, -2500.0f * scaleX, -600.0f * scaleX,
            160.0f * scaleX, 100.0f * scaleX, 600.0f * scaleX);
        fopAcM_setCullSizeFar(this, 10.0f);

        mTimer = (s16)cM_rndF(0x8000);
    }

    return phase;
}

/* 000003DC-0000041C       .text _delete__9daIkari_cFv */
bool daIkari_c::_delete() {
    dComIfG_resDeleteDemo(&mPhs, M_arcname);
    return true;
}

/* 0000041C-000004F4       .text _execute__9daIkari_cFv */
bool daIkari_c::_execute() {
    mpMorf->calc();
    mTimer++;

    cXyz* windVec = dKyw_get_wind_vec();
    s16 windAngle = cM_atan2s(windVec->x, windVec->z);
    f32* windPow = dKyw_get_wind_power();

    shape_angle.y = windAngle;

    f32 rotX = 0.1f * (10000.0f * *windPow);

    current.angle.x =
        rotX + rotX * JMASSin(mTimer * (REG0_S(5) + 500) & 0xffff);

    set_mtx();

    return true;
}

/* 000004F4-000005C4       .text _draw__9daIkari_cFv */
bool daIkari_c::_draw() {
    if (mEnvType == 1) {
        dScnKy_env_light_c& env = dKy_getEnvlight();
        env.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
        tevStr.mColorC0.r = env.mActorC0.r;
        tevStr.mColorC0.g = env.mActorC0.g;
        tevStr.mColorC0.b = env.mActorC0.b;
        tevStr.mColorC0.a = env.mActorC0.a;
        tevStr.mColorK0.r = env.mActorK0.r;
        tevStr.mColorK0.g = env.mActorK0.g;
        tevStr.mColorK0.b = env.mActorK0.b;
    } else {
        g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    }
    g_env_light.setLightTevColorType(mpMorf->getModel(), &tevStr);
    mpMorf->entryDL();
    return true;
}
#else

/* 000000EC-0000010C       .text createHeap_CB__FP10fopAc_ac_c */
static BOOL createHeap_CB(fopAc_ac_c* i_this) {
    return ((daIkari_c*)i_this)->_createHeap();
}

/* 0000010C-000001C4       .text setMtx__9daIkari_cFv */
void daIkari_c::setMtx() {
    mpModel->setBaseScale(scale);

    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::XrotM(current.angle.x);
    mDoMtx_stack_c::YrotM(-shape_angle.y);
    mDoMtx_stack_c::YrotM(current.angle.y);

    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 000001C4-00000290       .text _createHeap__9daIkari_cFv */
BOOL daIkari_c::_createHeap() {
    static int ikari_bdl[5] = {
#if VERSION == VERSION_DEMO
        dRes_INDEX_IKARI_BMD_S_IKARI2_e,
        dRes_INDEX_IKARI_BMD_S_IKARI2_e,
        dRes_INDEX_IKARI_BMD_S_IKARI2_e,
        dRes_INDEX_IKARI_BMD_S_IKARI3_e,
        dRes_INDEX_IKARI_BMD_S_IKARI4_e,
#else
        dRes_INDEX_IKARI_BDL_S_IKARI2_e,
        dRes_INDEX_IKARI_BDL_S_IKARI2_e,
        dRes_INDEX_IKARI_BDL_S_IKARI2_e,
        dRes_INDEX_IKARI_BDL_S_IKARI3_e,
        dRes_INDEX_IKARI_BDL_S_IKARI4_e,
#endif
    };

    J3DModelData* modelData =
        static_cast<J3DModelData*>(dComIfG_getObjectRes(M_arcname, ikari_bdl[mModelType]));
    JUT_ASSERT(0x7e, modelData != NULL);
    mpModel = mDoExt_J3DModel__create(modelData, 0x00080000, 0x11000022);

    if (mpModel == NULL) {
        return false;
    }
    return true;
}

/* 00000290-000002F8       .text getArg__9daIkari_cFv */
void daIkari_c::getArg() {
    u32 param = fopAcM_GetParam(this);

    mModelType = fopAcM_GetParamBit(param, 8, 16);
    mEnvType = param;

    if (mModelType == 0xff) {
        mModelType = 0;
    }

    if (mEnvType == 0xff) {
        scale.x = 1.0f;
        scale.y = 1.0f;
        scale.z = 1.0f;
    } else if (mEnvType == 0x01) {
        scale.x = 1.27f;
        scale.y = 1.27f;
        scale.z = 1.27f;
    }
}

/* 000002F8-000003CC       .text _execute__9daIkari_cFv */
bool daIkari_c::_execute() {
    mTimer++;

    cXyz* windVec = dKyw_get_wind_vec();
    s16 windAngle = cM_atan2s(windVec->x, windVec->z);
    f32* windPow = dKyw_get_wind_power();

    shape_angle.y = windAngle;

    f32 rotX = *windPow * 10000.0f * l_HIO.mWindPowerScale;

    current.angle.x =
        rotX + rotX * JMASSin(mTimer * (REG0_S(5) + 500) & 0xffff);

    setMtx();

    return true;
}

/* 000003CC-00000494       .text _draw__9daIkari_cFv */
bool daIkari_c::_draw() {
    if (mEnvType == 1) {
        g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
        tevStr.mColorC0.r = g_env_light.mActorC0.r;
        tevStr.mColorC0.g = g_env_light.mActorC0.g;
        tevStr.mColorC0.b = g_env_light.mActorC0.b;
        tevStr.mColorC0.a = g_env_light.mActorC0.a;
        tevStr.mColorK0.r = g_env_light.mActorK0.r;
        tevStr.mColorK0.g = g_env_light.mActorK0.g;
        tevStr.mColorK0.b = g_env_light.mActorK0.b;
    } else {
        g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    }
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mDoExt_modelUpdateDL(mpModel);
    return true;
}

/* 00000494-000005B0       .text _create__9daIkari_cFv */
cPhs_State daIkari_c::_create() {
    cPhs_State phase = dComIfG_resLoad(&mPhs, M_arcname);

    fopAcM_ct(this, daIkari_c);

    if (phase == cPhs_COMPLEATE_e) {
        getArg();

        if (!fopAcM_entrySolidHeap(this, createHeap_CB, 0xC20)) {
            return cPhs_ERROR_e;
        } else {
            setMtx();

            f32 scaleX = scale.x;

            fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
            fopAcM_setCullSizeBox(this,
                -160.0f * scaleX, -2500.0f * scaleX, -600.0f * scaleX,
                160.0f * scaleX, 100.0f * scaleX, 600.0f * scaleX);
            fopAcM_setCullSizeFar(this, 10.0f);

            mTimer = (s16)(int)cM_rndF(0x8000);
        }
    }

    return phase;
}

/* 000005B0-000005E0       .text _delete__9daIkari_cFv */
bool daIkari_c::_delete() {
    dComIfG_resDelete(&mPhs, M_arcname);
    return true;
}

#endif

/* 000005E0-00000600       .text daIkariCreate__FPv */
static cPhs_State daIkariCreate(void* i_this) {
    return ((daIkari_c*)i_this)->_create();
}

/* 00000600-00000624       .text daIkariDelete__FPv */
static BOOL daIkariDelete(void* i_this) {
    return ((daIkari_c*)i_this)->_delete();
}

/* 00000624-00000648       .text daIkariExecute__FPv */
static BOOL daIkariExecute(void* i_this) {
    return ((daIkari_c*)i_this)->_execute();
}

/* 00000648-0000066C       .text daIkariDraw__FPv */
static BOOL daIkariDraw(void* i_this) {
    return ((daIkari_c*)i_this)->_draw();
}

/* 0000066C-00000674       .text daIkariIsDelete__FPv */
static BOOL daIkariIsDelete(void* i_this) {
    UNUSED(i_this);
    return TRUE;
}

static actor_method_class daIkariMethodTable = {
    (process_method_func)daIkariCreate,
    (process_method_func)daIkariDelete,
    (process_method_func)daIkariExecute,
    (process_method_func)daIkariIsDelete,
    (process_method_func)daIkariDraw,
};

actor_process_profile_definition g_profile_IKARI = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_IKARI_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daIkari_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_IKARI_e,
    /* Actor SubMtd */ &daIkariMethodTable,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
