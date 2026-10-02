/**
 * d_a_lamp.cpp
 * Object - Generic wall lamp
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_lamp.h"
#include "res/Object/Lamp.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_mtx.h"
#include "d/d_s_play.h"

/* 000000EC-00000158       .text daLamp_Draw__FP10lamp_class */
static BOOL daLamp_Draw(lamp_class* i_this) {
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &i_this->current.pos, &i_this->tevStr);
    J3DModel* pModel = i_this->mModel;
    g_env_light.setLightTevColorType(pModel, &i_this->tevStr);
    mDoExt_modelUpdateDL(pModel);
    return TRUE;
}

/* 00000158-000005C8       .text daLamp_Execute__FP10lamp_class */
static BOOL daLamp_Execute(lamp_class* i_this) {
    i_this->mCycleCtr += 1;
    MtxTrans(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z, 0);
#if VERSION == VERSION_DEMO
    cMtx_YrotM(*calc_mtx, i_this->current.angle.y);
#else
    mDoMtx_YrotM(*calc_mtx, i_this->current.angle.y);
#endif
    if (i_this->mParameters == 0) {
        i_this->mLength = 0.1f;
    } else {
        if (dComIfGp_getVibration().CheckQuake() || fopAcM_otoCheck(i_this, 1000.0f) > 400) {
            i_this->mOto = 0x14;
        }
        if (i_this->mOto) {
            i_this->mOto--;
            cLib_addCalc2(&i_this->mLength, 0.15f, 1.0f, 0.015f);
        } else {
            cLib_addCalc2(&i_this->mLength, 0.01f, 1.0f, 0.0005f);
        }
    }
    f32 oppDist = cM_ssin(i_this->mCycleCtr * 0x320) * i_this->mLength;
    MtxRotX(oppDist, 1);
    f32 adjDist = cM_scos(i_this->mCycleCtr * 0x2bc) * i_this->mLength;
    MtxRotZ(adjDist, 1);
    MtxScale(0.4f + DEMO_SELECT(REG0_F(9), 0.0f), 0.4f + DEMO_SELECT(REG0_F(9), 0.0f), 0.4f + DEMO_SELECT(REG0_F(9), 0.0f), 1);
    i_this->mModel->setBaseTRMtx(*calc_mtx);
    MtxTrans(10.0f + DEMO_SELECT(REG0_F(6), 0.0f), -140.0f + DEMO_SELECT(REG0_F(7), 0.0f), -15.0f + DEMO_SELECT(REG0_F(8), 0.0f), 1);

    cXyz offset;
    offset.x = offset.y = offset.z = 0.0f;
    MtxPosition(&offset, &i_this->mPos);

    if (!i_this->mParticleInit) {
        static cXyz fire_scale(0.5f, 0.5f, 0.5f);

#if VERSION == VERSION_DEMO
        i_this->mpEmitter = dComIfGp_particle_set(dPa_name::ID_AK_JN_TORCH, &i_this->mPos, NULL, &fire_scale, 0xFF, &i_this->mPa);
#else
        dComIfGp_particle_set(dPa_name::ID_AK_JN_TORCH, &i_this->mPos, NULL, &fire_scale, 0xFF, &i_this->mPa);
#endif
        i_this->mParticleInit = 1;
        i_this->mParticlePower = 1.0f;
    }

    if (DEMO_SELECT(i_this->mpEmitter, i_this->mPa.getEmitter())) {
        cXyz whitePartPos = i_this->mPos;
        whitePartPos.y += 20.0f;
        dComIfGp_particle_setSimple(dPa_name::ID_AK_JP_O_KAGEROU00, &whitePartPos);
        cLib_addCalc2(&i_this->mParticlePower, cM_rndF(0.2f) + 1.0f, 0.5f, 0.02f);
    } else {
        i_this->mParticlePower = 0.0f;
    }
    i_this->mInf.mPos = i_this->mPos;
    i_this->mInf.mColor.r = 600;
    i_this->mInf.mColor.g = 400;
    i_this->mInf.mColor.b = 120;

    s16 power = i_this->mParticlePower * 150.0f;
    i_this->mInf.mPower = power;
    i_this->mInf.mFluctuation = 250.0f;

    i_this->mSph.SetC(i_this->mPos);
    dComIfG_Ccsp()->Set(&i_this->mSph);
    if (!i_this->mHitTimeoutLeft) {
        if (i_this->mSph.ChkTgHit()) {
            cCcD_Obj* pHitObj = i_this->mSph.GetTgHitObj();
            if (pHitObj && pHitObj->ChkAtType(AT_TYPE_WIND | AT_TYPE_UNK400000)) {
                i_this->mHitAngle = dComIfGp_getPlayer(0)->shape_angle.y;
                i_this->mHitTimeoutLeft = 0x28 + DEMO_SELECT(REG0_S(2), 0);
            }
        }
    } else {
        i_this->mHitTimeoutLeft--;
        if (DEMO_SELECT(i_this->mpEmitter, i_this->mPa.getEmitter())) {
            float tgtZ;
            if (i_this->mHitTimeoutLeft > DEMO_SELECT((s16)(10 + REG0_S(3)), 10)) {
                tgtZ = 4.0f + DEMO_SELECT(REG0_F(5), 0.0f);
            } else {
                tgtZ = 0.0f;
            }
            cLib_addCalc2(&i_this->mHitReactCurZ, tgtZ, 1.0f, 0.5f + DEMO_SELECT(REG0_F(8), 0.0f));
            cMtx_YrotS(*calc_mtx, i_this->mHitAngle);
            cXyz offset;
            offset.set(0.0f, 1.0f, i_this->mHitReactCurZ);
            cXyz rotOffset;
            MtxPosition(&offset, &rotOffset);
            JGeometry::TVec3<f32> dir(rotOffset);
            DEMO_SELECT(i_this->mpEmitter, i_this->mPa.getEmitter())->setDirection(dir);
#if VERSION == VERSION_DEMO
            if (i_this->mHitTimeoutLeft == 15) {
                i_this->mpEmitter->becomeInvalidEmitter();
                i_this->mpEmitter = NULL;
            }
#endif
        }
    }

    return TRUE;
}

/* 00000604-00000634       .text daLamp_IsDelete__FP10lamp_class */
static BOOL daLamp_IsDelete(lamp_class* i_this) {
    i_this->mPa.remove();
    return TRUE;
}

/* 00000634-00000678       .text daLamp_Delete__FP10lamp_class */
static BOOL daLamp_Delete(lamp_class* i_this) {
#if VERSION == VERSION_DEMO
    dKy_plight_cut(&i_this->mInf);
    dComIfG_deleteObjectRes("Lamp");
#else
    dComIfG_resDelete(&i_this->mPhs, "Lamp");
    dKy_plight_cut(&i_this->mInf);
#endif
    return TRUE;
}

/* 00000678-0000073C       .text useHeapInit__FP10lamp_class */
static BOOL useHeapInit(lamp_class* i_this) {
    J3DModelData* modelData = static_cast<J3DModelData*>(dComIfG_getObjectRes("Lamp", dRes_INDEX_LAMP_BMD_LAMP_00_e));
    JUT_ASSERT(DEMO_SELECT(0x16d, 0x170), modelData != NULL);

    i_this->mModel = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
    if (i_this->mModel == 0) {
        return DEMO_SELECT(cPhs_ERROR_e, FALSE);
    } else {
        return DEMO_SELECT(cPhs_COMPLEATE_e, TRUE);
    }
}

/* 0000073C-0000075C       .text daLamp_solidHeapCB__FP10fopAc_ac_c */
static BOOL daLamp_solidHeapCB(fopAc_ac_c* i_ac) {
    return useHeapInit((lamp_class*)i_ac);
}

/* 0000075C-00000914       .text daLamp_Create__FP10fopAc_ac_c */
static cPhs_State daLamp_Create(fopAc_ac_c* i_ac) {
    fopAcM_ct(i_ac, lamp_class);
    lamp_class* i_this = (lamp_class*)i_ac;

    cPhs_State phase_state = dComIfG_resLoad(&i_this->mPhs, "Lamp");
    if (phase_state == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(i_this, &daLamp_solidHeapCB, 0x6040)) {
            i_this->mParameters = fopAcM_GetParam(i_this);
            if (i_this->mParameters == 0xFF) {
                i_this->mParameters = 0;
            }
            i_this->mStts.Init(0xff, 0xff, i_this);

            static dCcD_SrcSph sph_src = {
                // dCcD_SrcGObjInf
                {
                    /* Flags             */ 0,
                    /* SrcObjAt  Type    */ 0,
                    /* SrcObjAt  Atp     */ 0,
                    /* SrcObjAt  SPrm    */ 0,
                    /* SrcObjTg  Type    */ AT_TYPE_FIRE | AT_TYPE_UNK20000 | AT_TYPE_FIRE_ARROW | AT_TYPE_WIND | AT_TYPE_UNK400000,
                    /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
                    /* SrcObjCo  SPrm    */ 0,
                    /* SrcGObjAt Se      */ 0,
                    /* SrcGObjAt HitMark */ 0,
                    /* SrcGObjAt Spl     */ 0,
                    /* SrcGObjAt Mtrl    */ 0,
                    /* SrcGObjAt SPrm    */ 0,
                    /* SrcGObjTg Se      */ 0,
                    /* SrcGObjTg HitMark */ 0,
                    /* SrcGObjTg Spl     */ 0,
                    /* SrcGObjTg Mtrl    */ 0,
                    /* SrcGObjTg SPrm    */ 0,
                    /* SrcGObjCo SPrm    */ 0,
                },
                // cM3dGSphS
                {{
                    /* Center */ {0.0f, 0.0f, 0.0f},
                    /* Radius */ 30.0f,
                }},
            };

            i_this->mSph.Set(sph_src);
            i_this->mSph.SetStts(&i_this->mStts);

            i_this->mCycleCtr = cM_rndFX(0x8000);

            for (int i = 0; i < 2; i++) {
                daLamp_Execute(i_this);
            }

            dKy_plight_set(&i_this->mInf);
        } else {
            phase_state = cPhs_ERROR_e;
        }
    }

    return phase_state;
}

static actor_method_class l_daLamp_Method = {
    (process_method_func)daLamp_Create,
    (process_method_func)daLamp_Delete,
    (process_method_func)daLamp_Execute,
    (process_method_func)daLamp_IsDelete,
    (process_method_func)daLamp_Draw,
};

actor_process_profile_definition g_profile_LAMP = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_LAMP_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(lamp_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_LAMP_e,
    /* Actor SubMtd */ &l_daLamp_Method,
    /* Status       */ fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
