/**
 * d_a_obj_kanoke.cpp
 * Object - Earth Temple - Coffin
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_kanoke.h"
#include "d/d_cc_d.h"
#include "d/d_a_obj.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "res/Object/Mkanoke.h"

static dCcD_SrcCps l_cps_src_body = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_LIGHT,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
        /* SrcObjCo  SPrm    */ 0,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ dCcG_AtSPrm_NoConHit_e,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCpsS
    {{
        /* Start  */ {0.0f, 50.0f, -100.0f},
        /* End    */ {0.0f, 50.0f, 100.0f},
        /* Radius */ 100.0f,
    }},
};


static dCcD_SrcCps l_cps_src_huta = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_UNK800,
        /* SrcObjAt  Atp     */ 1,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_GrpAll_e,
        /* SrcObjTg  Type    */ 0,
        /* SrcObjTg  SPrm    */ 0,
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
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCpsS
    {{
        /* Start  */ {0.0f, 50.0f, -100.0f},
        /* End    */ {0.0f, 50.0f, 100.0f},
        /* Radius */ 25.0f,
    }},
};


static cXyz daObjKanoke_Yoko_pfs[] = {cXyz(100.0f, 0.0f, 0.0f)};
static cXyz daObjKanoke_Tate_pfs[3][2] = {
    {cXyz(50.0f, 0.0f, -175.0f), cXyz(50.0f, 0.0f, 175.0f)},
    {cXyz(0.0f, 0.0f, -175.0f), cXyz(0.0f, 0.0f, 175.0f)},
    {cXyz(-50.0f, 0.0f, -175.0f), cXyz(-50.0f, 0.0f, 175.0f)},
};
static void (daObjKanoke_c::*moveProc[])() = {
    &daObjKanoke_c::executeNormal,
    &daObjKanoke_c::executeYureYoko,
    &daObjKanoke_c::executeOpenYoko,
    &daObjKanoke_c::executeEffectYoko,
    &daObjKanoke_c::executeYureTate,
    &daObjKanoke_c::executeOpenTate,
    &daObjKanoke_c::executeEffectTate,
    &daObjKanoke_c::executeWait,
};

/* 000000EC-000002F4       .text __ct__13daObjKanoke_cFv */
daObjKanoke_c::daObjKanoke_c() {
    mType = getPrmType();
    mSearch = getPrmSearch();
    mSwitch = getPrmSwNo();
    mSwitch2 = getPrmSwNo2();
    if (mType == 0) {
        mPivot.set(0.0f, 0.0f, 0.0f);
    } else {
        current.pos.y = 200.0f + home.pos.y;
        shape_angle.x = 0x4000;
        mPivot.set(0.0f, 35.0f, 200.0f);
    }
    mLidOffset.set(0.0f, 75.0f, 0.0f);
    mLidAngle.set(0, 0, 0);
    mAngularSpeed = 0;
    for (int i = 0; i < 2; ++i) {
        mpEmitter[i] = NULL;
    }
    mSmoke.setRateOff(0);
    mMode = 0;
    mHideFlags = 0;
    mLightTimer = 0;
    setMtxHontai();
    MTXCopy(mDoMtx_stack_c::get(), mBodyMtx);
    setMtxHuta(&current.pos);
    MTXCopy(mDoMtx_stack_c::get(), mLidMtx);
}

/* 00000644-00000664       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return static_cast<daObjKanoke_c*>(actor)->createHeap();
}

/* 00000664-00000700       .text _create__13daObjKanoke_cFv */
cPhs_State daObjKanoke_c::_create() {
    fopAcM_SetupActor(this, daObjKanoke_c);
    cPhs_State phase = dComIfG_resLoad(&mPhase, "Mkanoke");
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, CheckCreateHeap, 0x2400)) {
            return createInit();
        }
#if VERSION > VERSION_DEMO
        mpBodyBgW = mpLidBgW = NULL;
#endif
        return cPhs_ERROR_e;
    }
    return phase;
}

/* 00000700-000008BC       .text createHeap__13daObjKanoke_cFv */
BOOL daObjKanoke_c::createHeap() {
    J3DModelData* data = (J3DModelData*)dComIfG_getObjectRes("Mkanoke", dRes_INDEX_MKANOKE_BDL_MOKE1_e);
    if (!data) {
        return FALSE;
    }
    mpBodyModel = mDoExt_J3DModel__create(data, 0, 0x11020203);
    if (!mpBodyModel) {
        return FALSE;
    }
    mpBodyBgW = new dBgW;
    if (!mpBodyBgW) {
        return FALSE;
    }
    if (mpBodyBgW->Set((cBgD_t*)dComIfG_getObjectRes("Mkanoke", dRes_INDEX_MKANOKE_DZB_MOKE1_e), 1, &mBodyMtx) == 1) {
        return FALSE;
    }
    data = (J3DModelData*)dComIfG_getObjectRes("Mkanoke", dRes_INDEX_MKANOKE_BDL_MOKE2_e);
    if (!data) {
        return FALSE;
    }
    mpLidModel = mDoExt_J3DModel__create(data, 0, 0x11020203);
    if (!mpLidModel) {
        return FALSE;
    }
    mpLidBgW = new dBgW;
    if (!mpLidBgW) {
        return FALSE;
    }
    if (mpLidBgW->Set((cBgD_t*)dComIfG_getObjectRes("Mkanoke", dRes_INDEX_MKANOKE_DZB_MOKE2_e), 1, &mLidMtx) == 1) {
        return FALSE;
    }
    return TRUE;
}

/* 000008BC-00000B28       .text createInit__13daObjKanoke_cFv */
cPhs_State daObjKanoke_c::createInit() {
    if (dComIfG_Bgsp()->Regist(mpBodyBgW, this)) {
        return cPhs_ERROR_e;
    }
    if (mSwitch != 0xff) {
        if (dComIfGs_isSwitch(mSwitch, fopAcM_GetHomeRoomNo(this))) {
            mMode = 7;
            if (mType == 0) {
                if (dComIfG_Bgsp()->Regist(mpLidBgW, this)) {
                    return cPhs_ERROR_e;
                }
                mLidOffset.set(148.0f, 75.0f, 0.0f);
                mPivot.set(-48.0f, 0.0f, 0.0f);
                mLidAngle.z = -5600;
            } else {
                mHideFlags |= 2;
            }
        } else {
            if (dComIfG_Bgsp()->Regist(mpLidBgW, this)) {
                return cPhs_ERROR_e;
            }
        }
    } else {
        if (dComIfG_Bgsp()->Regist(mpLidBgW, this)) {
            return cPhs_ERROR_e;
        }
    }
    mStts.Init(0xff, 0xff, this);
    mBodyCps.Set(l_cps_src_body);
    mBodyCps.SetStts(&mStts);
    mBodyCps.SetStartEnd(current.pos, current.pos);
    for (int i = 0; i < 3; ++i) {
        mLidCps[i].Set(l_cps_src_huta);
        mLidCps[i].SetStts(&mStts);
        mLidCps[i].SetStartEnd(current.pos, current.pos);
    }
    fopAcM_SetMtx(this, mpBodyModel->getBaseTRMtx());
    setMtx();
    if (mType == 0) {
        fopAcM_setCullSizeBox(this, -110.0f, 0.0f, -210.0f, 310.0f, 120.0f, 210.0f);
    } else {
        fopAcM_setCullSizeBox(this, -110.0f, 0.0f, -210.0f, 110.0f, 520.0f, 210.0f);
    }
    return cPhs_COMPLEATE_e;
}

/* 00000B28-00000C0C       .text _delete__13daObjKanoke_cFv */
BOOL daObjKanoke_c::_delete() {
#if VERSION == VERSION_DEMO
    if (mpBodyBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpBodyBgW);
    }
    if (mpLidBgW->ChkUsed()) {
        dComIfG_Bgsp()->Release(mpLidBgW);
    }
    for (int i = 0; i < 2; ++i) {
    }
#else
    if (heap) {
        if (mpBodyBgW && mpBodyBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpBodyBgW);
        }
        if (mpLidBgW && mpLidBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpLidBgW);
        }
    }
#endif
    mSmoke.end();
    dComIfG_resDeleteDemo(&mPhase, "Mkanoke");
    return TRUE;
}

/* 00000C0C-00000CE0       .text _draw__13daObjKanoke_cFv */
BOOL daObjKanoke_c::_draw() {
    g_env_light.settingTevStruct(1, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpBodyModel, &tevStr);
    g_env_light.setLightTevColorType(mpLidModel, &tevStr);
    dComIfGd_setListBG();
    if (!(mHideFlags & 1)) {
        mDoExt_modelUpdateDL(mpBodyModel);
    }
    if (!(mHideFlags & 2)) {
        mDoExt_modelUpdateDL(mpLidModel);
    }
    dComIfGd_setList();
    return TRUE;
}

/* 00000CE0-00000E7C       .text _execute__13daObjKanoke_cFv */
BOOL daObjKanoke_c::_execute() {
    cXyz start(0.0f, 0.0f, -100.0f);
    cXyz end(0.0f, 0.0f, 100.0f);
    mDoMtx_stack_c::YrotS(shape_angle.y);
    mDoMtx_stack_c::XrotM(shape_angle.x);
    mDoMtx_stack_c::multVec(&start, &start);
    mDoMtx_stack_c::multVec(&end, &end);
    start += current.pos;
    end += current.pos;
    mBodyCps.SetStartEnd(start, end);
    mBodyCps.SetR(140.0f);
    dComIfG_Ccsp()->Set(&mBodyCps);
    (this->*moveProc[mMode])();
    setMtx();
    if (mpBodyBgW->ChkUsed()) {
        mpBodyBgW->Move();
    }
    if (mpLidBgW->ChkUsed()) {
        mpLidBgW->Move();
    }
    return TRUE;
}

/* 00000E7C-0000122C       .text executeNormal__13daObjKanoke_cFv */
void daObjKanoke_c::executeNormal() {
    bool open = false;
    if (mSwitch != 0xff && dComIfGs_isSwitch(mSwitch, fopAcM_GetHomeRoomNo(this))) {
        open = true;
    } else if (dComIfGp_getDetect().chk_light(&current.pos) || mBodyCps.ChkTgHit()) {
        ++mLightTimer;
        if (mLightTimer > 20) {
            mBodyCps.SetTgType(0);
            open = true;
        }
    } else {
        mLightTimer = 0;
        if (mSearch != 0) {
            fopAc_ac_c* player = dComIfGp_getPlayer(0);
            cXyz delta = home.pos - player->current.pos;
            if (delta.abs() < 100.0f * mSearch) {
                open = true;
            }
        }
    }
    if (open) {
        mBodyCps.ClrTgHit();
        if (mSwitch != 0xff) {
            dComIfGs_onSwitch(mSwitch, fopAcM_GetHomeRoomNo(this));
        }
        mAngularSpeed = 0;
        if (getPrmYure()) {
            mTimer = 0;
            if (mType == 0) {
                fopAcM_seStartCurrent(this, 0x695b, 0);
                mMode = 1;
                mLidAngle.z = 0;
                mShakeAmplitude = 0x100;
            } else {
                fopAcM_seStartCurrent(this, 0x6959, 0);
                mMode = 4;
                mLidAngle.y = 0;
                mShakeAmplitude = 0x400;
            }
        } else {
            if (mType == 0) {
                fopAcM_seStartCurrent(this, 0x695c, 0);
                mMode = 2;
                mLidAngle.z = 0;
            } else {
                fopAcM_seStartCurrent(this, 0x695a, 0);
                mMode = 5;
                if (mpLidBgW->ChkUsed()) {
                    dComIfG_Bgsp()->Release(mpLidBgW);
                }
                mLidAngle.x = 0;
            }
        }
    }
}

/* 0000122C-00001358       .text executeYureYoko__13daObjKanoke_cFv */
void daObjKanoke_c::executeYureYoko() {
    mTimer += 0x2000;
    mShakeAmplitude -= 8;
    mLidAngle.z = mShakeAmplitude * cM_ssin(mTimer);
    if (mLidAngle.z < 0) {
        mPivot.x = 100.0f;
    } else {
        mPivot.x = -100.0f;
    }
    if (mShakeAmplitude <= 0) {
        fopAcM_seStartCurrent(this, 0x695c, 0);
        mMode = 2;
        mLidAngle.z = 0;
        mPivot.x = 0.0f;
    }
}

/* 00001358-00001544       .text executeOpenYoko__13daObjKanoke_cFv */
void daObjKanoke_c::executeOpenYoko() {
    f32 slide_max = 100.0f;
    s16 angle_min = -5600;
    mLidOffset.x += 4.0f;
    mPivot.x = slide_max - mLidOffset.x;
    if (mLidOffset.x > slide_max) {
        mLidAngle.z += mAngularSpeed;
        mAngularSpeed -= 100;
        if (mLidAngle.z <= angle_min) {
            mLidAngle.z = angle_min;
            mMode = 3;
            mDoMtx_stack_c::YrotS(shape_angle.y);
            mDoMtx_stack_c::transM(100.0f, 75.0f, 0.0f);
            mDoMtx_stack_c::ZrotM(mLidAngle.z);
            mDoMtx_stack_c::transM(-100.0f, -75.0f, 0.0f);
            Mtx mtx;
            MTXCopy(mDoMtx_stack_c::get(), mtx);
            mSmokeAngle.set(0, shape_angle.y, 0);
            mAlpha = 180.0f;
            cXyz pos = mLidOffset + daObjKanoke_Yoko_pfs[0];
            MTXMultVec(mtx, &pos, &pos);
            mSmokePos = pos + current.pos;
#if VERSION == VERSION_DEMO
            dComIfGp_particle_setToon(0xa181, &mSmokePos, &mSmokeAngle, NULL, mAlpha, &mSmoke);
#else
            if (!mSmoke.getEmitter()) {
                dComIfGp_particle_setToon(0xa181, &mSmokePos, &mSmokeAngle, NULL, mAlpha, &mSmoke);
            }
#endif
            if (mSmoke.getEmitter()) {
                mSmoke.getEmitter()->becomeImmortalEmitter();
            }
            mTimer = 60;
        }
    }
}

/* 00001544-000015F8       .text executeEffectYoko__13daObjKanoke_cFv */
void daObjKanoke_c::executeEffectYoko() {
    --mTimer;
    if (mTimer != 0) {
        if (mTimer <= 50) {
            mAlpha -= 3.6f;
            if (mAlpha < 0.0f) {
                mAlpha = 0.0f;
            }
            if (mSmoke.getEmitter()) {
                mSmoke.getEmitter()->setGlobalAlpha(mAlpha);
            }
        }
    } else {
        mSmoke.end();
        mMode = 7;
    }
}

/* 000015F8-00001764       .text executeYureTate__13daObjKanoke_cFv */
void daObjKanoke_c::executeYureTate() {
    mTimer += 0x2000;
    mShakeAmplitude -= 32;
    mLidAngle.y = mShakeAmplitude * cM_ssin(mTimer);
    if (mLidAngle.y < 0) {
        mPivot.x = 100.0f;
    } else {
        mPivot.x = -100.0f;
    }
    if (mShakeAmplitude <= 0) {
        fopAcM_seStartCurrent(this, 0x695a, 0);
        mMode = 5;
        if (mpLidBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpLidBgW);
        }
        mLidAngle.x = 0;
        mLidAngle.y = 0;
        mPivot.x = 0.0f;
    }
}

/* 00001764-00001A6C       .text executeOpenTate__13daObjKanoke_cFv */
void daObjKanoke_c::executeOpenTate() {
    mLidAngle.x += mAngularSpeed;
    mAngularSpeed += 100;
    if (mLidAngle.x >= 0x4000) {
        dComIfGp_getVibration().StartShock(4, -33, cXyz(0.0f, 1.0f, 0.0f));
        mLidAngle.x = 0x4000;
        mMode = 6;
        mHideFlags |= 2;
        mSmokePos.x = mLidMtx[0][3];
        mSmokePos.y = mLidMtx[1][3];
        mSmokePos.z = mLidMtx[2][3];
        mSmokeAngle.set(0, shape_angle.y, 0);
        mpEmitter[0] = dComIfGp_particle_set(0x817f, &mSmokePos, &mSmokeAngle, NULL, 0xff, NULL, -1, &tevStr.mColorK0, &tevStr.mColorK0);
        mAlpha = 180.0f;
#if VERSION == VERSION_DEMO
        dComIfGp_particle_setToon(0xa180, &mSmokePos, &mSmokeAngle, NULL, mAlpha, &mSmoke);
#else
        if (!mSmoke.getEmitter()) {
            dComIfGp_particle_setToon(0xa180, &mSmokePos, &mSmokeAngle, NULL, mAlpha, &mSmoke);
        }
#endif
        if (mSmoke.getEmitter()) {
            mSmoke.getEmitter()->becomeImmortalEmitter();
        }
        mTimer = 60;
    } else {
        mDoMtx_stack_c::YrotS(shape_angle.y);
        mDoMtx_stack_c::XrotM(shape_angle.x);
        mDoMtx_stack_c::transM(0.0f, 110.0f, 200.0f);
        mDoMtx_stack_c::XrotM(mLidAngle.x);
        mDoMtx_stack_c::transM(0.0f, -110.0f, -200.0f);
        for (int i = 0; i < 3; ++i) {
            cXyz start = daObjKanoke_Tate_pfs[i][0] + mLidOffset;
            cXyz end = daObjKanoke_Tate_pfs[i][1] + mLidOffset;
            mDoMtx_stack_c::multVec(&start, &start);
            mDoMtx_stack_c::multVec(&end, &end);
            start += current.pos;
            end += current.pos;
            mLidCps[i].SetStartEnd(start, end);
            dComIfG_Ccsp()->Set(&mLidCps[i]);
        }
    }
}

/* 00001A6C-00001B24       .text executeEffectTate__13daObjKanoke_cFv */
void daObjKanoke_c::executeEffectTate() {
    --mTimer;
    if (mTimer != 0) {
        if (mSmoke.getEmitter() && mTimer <= 50) {
            mAlpha -= 3.6f;
            if (mAlpha < 0.0f) {
                mAlpha = 0.0f;
            }
            mSmoke.getEmitter()->setGlobalAlpha(mAlpha);
        }
    } else {
        mSmoke.end();
        mMode = 7;
    }
}

/* 00001B24-00001B28       .text executeWait__13daObjKanoke_cFv */
void daObjKanoke_c::executeWait() {
}

/* 00001B28-00001B54       .text getPrmType__13daObjKanoke_cFv */
u8 daObjKanoke_c::getPrmType() {
    return daObj::PrmAbstract(this, PRM_TYPE_W, PRM_TYPE_S);
}

/* 00001B54-00001B80       .text getPrmSearch__13daObjKanoke_cFv */
u8 daObjKanoke_c::getPrmSearch() {
    return daObj::PrmAbstract(this, PRM_SEARCH_W, PRM_SEARCH_S);
}

/* 00001B80-00001BAC       .text getPrmYure__13daObjKanoke_cFv */
u8 daObjKanoke_c::getPrmYure() {
    return daObj::PrmAbstract(this, PRM_YURE_W, PRM_YURE_S);
}

/* 00001BAC-00001BD8       .text getPrmSwNo__13daObjKanoke_cFv */
u8 daObjKanoke_c::getPrmSwNo() {
    return daObj::PrmAbstract(this, PRM_SW_W, PRM_SW_S);
}

/* 00001BD8-00001C04       .text getPrmSwNo2__13daObjKanoke_cFv */
u8 daObjKanoke_c::getPrmSwNo2() {
    return daObj::PrmAbstract(this, PRM_SW2_W, PRM_SW2_S);
}

/* 00001C04-00001C9C       .text setMtx__13daObjKanoke_cFv */
void daObjKanoke_c::setMtx() {
    if (!(mHideFlags & 1)) {
        setMtxHontai();
        mpBodyModel->setBaseTRMtx(mDoMtx_stack_c::get());
        MTXCopy(mDoMtx_stack_c::get(), mBodyMtx);
    }
    if (!(mHideFlags & 2)) {
        setMtxHuta(&current.pos);
        mpLidModel->setBaseTRMtx(mDoMtx_stack_c::get());
        MTXCopy(mDoMtx_stack_c::get(), mLidMtx);
    }
}

/* 00001C9C-00001D38       .text setMtxHontai__13daObjKanoke_cFv */
void daObjKanoke_c::setMtxHontai() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::XrotM(shape_angle.x);
    mDoMtx_stack_c::transM(mPivot);
    mDoMtx_stack_c::YrotM(mLidAngle.y);
    mDoMtx_stack_c::transM(-mPivot.x, -mPivot.y, -mPivot.z);
}

/* 00001D38-00001E4C       .text setMtxHuta__13daObjKanoke_cFP4cXyz */
void daObjKanoke_c::setMtxHuta(cXyz* pos) {
    cXyz offset;
    mDoMtx_stack_c::YrotS(shape_angle.y);
    mDoMtx_stack_c::XrotM(shape_angle.x);
    mDoMtx_stack_c::multVec(&mLidOffset, &offset);
    mDoMtx_stack_c::transS(pos->x + offset.x, pos->y + offset.y, pos->z + offset.z);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::XrotM(shape_angle.x);
    mDoMtx_stack_c::transM(mPivot);
    mDoMtx_stack_c::XrotM(mLidAngle.x);
    mDoMtx_stack_c::YrotM(mLidAngle.y);
    mDoMtx_stack_c::ZrotM(mLidAngle.z);
    mDoMtx_stack_c::transM(-mPivot.x, -mPivot.y, -mPivot.z);
}

/* 00001E4C-00001E6C       .text daObjKanokeCreate__FPv */
static cPhs_State daObjKanokeCreate(void* i_this) {
    return ((daObjKanoke_c*)i_this)->_create();
}

/* 00001E6C-00001E8C       .text daObjKanokeDelete__FPv */
static BOOL daObjKanokeDelete(void* i_this) {
    return ((daObjKanoke_c*)i_this)->_delete();
}

/* 00001E8C-00001EAC       .text daObjKanokeExecute__FPv */
static BOOL daObjKanokeExecute(void* i_this) {
    return ((daObjKanoke_c*)i_this)->_execute();
}

/* 00001EAC-00001ECC       .text daObjKanokeDraw__FPv */
static BOOL daObjKanokeDraw(void* i_this) {
    return ((daObjKanoke_c*)i_this)->_draw();
}

/* 00001ECC-00001ED4       .text daObjKanokeIsDelete__FPv */
static BOOL daObjKanokeIsDelete(void*) {
    return TRUE;
}

static actor_method_class daObjKanokeMethodTable = {
    (process_method_func)daObjKanokeCreate,
    (process_method_func)daObjKanokeDelete,
    (process_method_func)daObjKanokeExecute,
    (process_method_func)daObjKanokeIsDelete,
    (process_method_func)daObjKanokeDraw,
};

actor_process_profile_definition g_profile_Obj_Kanoke = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Kanoke_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjKanoke_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Kanoke_e,
    /* Actor SubMtd */ &daObjKanokeMethodTable,
    /* Status       */ 0x04 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
