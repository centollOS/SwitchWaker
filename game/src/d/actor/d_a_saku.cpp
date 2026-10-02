/**
 * d_a_saku.cpp
 * Object - Brown wooden barricade
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_saku.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_hostIO.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/KsakuCo.h"
#include "res/Object/Ksaku_00.h"
#include "res/Object/Knsak_00.h"
#include <math.h>

class sakuHIO_c : public JORReflexible {
public:
    sakuHIO_c() {
        mNo = -1;
        m06 = 70;
        m08 = 70;
        m0A = 65;
        m0C = 7;
        m0E = 1;
        mDebris = 1;
        mAlphaSpeed = 5;
        mDustAlpha = 180;
        mDustR = 105;
        mDustG = 91;
        mDustB = 48;
        mZThreshold = 100;
    }
    virtual ~sakuHIO_c() {}
    void genMessage(JORMContext*) {}
    s8 mNo;
    s16 m06, m08, m0A, m0C;
    u8 m0E, mDebris;
    s16 mAlphaSpeed;
    u8 mDustAlpha, mDustR, mDustG, mDustB, mZThreshold;
};

static sakuHIO_c l_sakuHIO;

BOOL matAlphaAnim(J3DModelData*, u8, bool);
static void changeXluMaterialAlpha(J3DMaterial*, u8, bool);

/* 000000EC-00000200       .text CreateInit__8daSaku_cFv */
void daSaku_c::CreateInit() {
    for (int i = 0; i < 2; i++) {
        mParticleTimer[i] = 0;
#if VERSION == VERSION_DEMO
        mSmokeEmitter[i] = NULL;
#else
        mSmokeActive[i] = 0;
#endif
        mAlpha[i][0] = 255;
        mAlpha[i][1] = 0;
        mCollisionTimer[i] = -1;
        mHeapDeleteTimer[i] = 2;
    }
    mBurning = 0;
    mFireTimer = 0;
    fopAcM_SetMtx(this, mModel[0][0]->getBaseTRMtx());
    mStts.Init(255, 255, this);
    setCol();
    setMtx();
#if VERSION > VERSION_DEMO
    for (int i = 0; i < 2; i++) {
        mSmoke[i].setColor(dust_color);
        mSmoke[i].setRateOff(1);
    }
#endif
}

/* 00000200-000003A8       .text saku_draw_sub__8daSaku_cFi */
BOOL daSaku_c::saku_draw_sub(int id) {
    bool zWrite = true;
    if (mAlpha[id][0] < l_sakuHIO.mZThreshold) {
        zWrite = false;
    }
    if (m_heap[id][0] && mModel[id][0] && mAlpha[id][0]) {
        g_env_light.settingTevStruct(0, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(mModel[id][0], &tevStr);
        matAlphaAnim(mModel[id][0]->getModelData(), (u8)mAlpha[id][0], zWrite);
        dComIfGd_setListBG();
        mDoExt_modelUpdateDL(mModel[id][0]);
        dComIfGd_setList();
        matAlphaAnim(mModel[id][0]->getModelData(), 255, true);
    }
    if (m_heap[id][1] && mModel[id][1] && mAlpha[id][1]) {
        g_env_light.settingTevStruct(0, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(mModel[id][1], &tevStr);
        zWrite = !zWrite;
        matAlphaAnim(mModel[id][1]->getModelData(), 255, zWrite);
        mDoExt_modelUpdateDL(mModel[id][1]);
        matAlphaAnim(mModel[id][1]->getModelData(), 255, true);
    }
    return TRUE;
}

/* 000003A8-00000590       .text mode_break_none__8daSaku_cFi */
BOOL daSaku_c::mode_break_none(int id) {
    u32 fire = 0, brokenHit = 0;
    for (int i = 0; i < 3; i++) {
#if VERSION > VERSION_JPN
        dCcD_Cyl* const cyl = &mCyl[id][i];
        if (cyl->ChkTgHit()) {
            cCcD_Obj* const hit = cyl->GetTgHitObj();
#else
        if (mCyl[id][i].ChkTgHit()) {
            cCcD_Obj* const hit = mCyl[id][i].GetTgHitObj();
#endif
            if (hit) {
#if VERSION == VERSION_DEMO
                if (mSturdinessType == 0) {
                    brokenHit |= (hit->ChkAtType(AT_TYPE_SWORD) ||
                                  hit->ChkAtType(AT_TYPE_UNK8) ||
                                  hit->ChkAtType(AT_TYPE_BOMB) ||
                                  hit->ChkAtType(AT_TYPE_MACHETE) ||
                                  hit->ChkAtType(AT_TYPE_UNK800) ||
                                  hit->ChkAtType(AT_TYPE_DARKNUT_SWORD) ||
                                  hit->ChkAtType(AT_TYPE_MOBLIN_SPEAR) ||
                                  hit->ChkAtType(AT_TYPE_SKULL_HAMMER));
                } else if (mSturdinessType == 1) {
                    brokenHit |= (hit->ChkAtType(AT_TYPE_MACHETE) ||
                                  hit->ChkAtType(AT_TYPE_BOMB) ||
                                  hit->ChkAtType(AT_TYPE_UNK800) ||
                                  hit->ChkAtType(AT_TYPE_DARKNUT_SWORD));
                }
                if (brokenHit) {
                    dComIfGp_getVibration().StartShock(4, -33, cXyz(0.0f, 1.0f, 0.0f));
                }
                fire |= (hit->ChkAtType(AT_TYPE_FIRE) || hit->ChkAtType(AT_TYPE_UNK20000) || hit->ChkAtType(AT_TYPE_FIRE_ARROW));
#else
                if (mSturdinessType == 0) {
                    brokenHit |= bool(hit->ChkAtType(AT_TYPE_SWORD) ||
                                      hit->ChkAtType(AT_TYPE_UNK8) ||
                                      hit->ChkAtType(AT_TYPE_BOMB) ||
                                      hit->ChkAtType(AT_TYPE_MACHETE) ||
                                      hit->ChkAtType(AT_TYPE_UNK800) ||
                                      hit->ChkAtType(AT_TYPE_DARKNUT_SWORD) ||
                                      hit->ChkAtType(AT_TYPE_MOBLIN_SPEAR) ||
                                      hit->ChkAtType(AT_TYPE_SKULL_HAMMER));
                } else if (mSturdinessType == 1) {
                    brokenHit |= bool(hit->ChkAtType(AT_TYPE_MACHETE) ||
                                      hit->ChkAtType(AT_TYPE_BOMB) ||
                                      hit->ChkAtType(AT_TYPE_UNK800) ||
                                      hit->ChkAtType(AT_TYPE_DARKNUT_SWORD));
                }
                if (brokenHit) {
                    dComIfGp_getVibration().StartShock(4, -33, cXyz(0.0f, 1.0f, 0.0f));
                }
                fire |= bool(hit->ChkAtType(AT_TYPE_FIRE) || hit->ChkAtType(AT_TYPE_UNK20000) || hit->ChkAtType(AT_TYPE_FIRE_ARROW));
#endif
                if (fire) {
                    break;
                }
            }
        }
    }
    if (fire) {
        return burn();
    }
    if (brokenHit) {
        if (id == 1 && mState[0] == 1) {
            broken(0);
        }
        return broken(id);
    }
    return TRUE;
}

/* 000005CC-000006A8       .text mode_break_fire__8daSaku_cFi */
BOOL daSaku_c::mode_break_fire(int id) {
    if (mParticleTimer[0] > m_saku_alpha_out_time) {
        cLib_chaseUC(&mAlpha[id][1], 255, l_sakuHIO.mAlphaSpeed);
        if (cLib_chaseUC(&mAlpha[id][0], 0, l_sakuHIO.mAlphaSpeed)) {
            if (m_heap[id][0]) {
                deleteOriginalHeap(id);
            }
        }
    }
    return TRUE;
}

/* 000006A8-0000083C       .text mode_break_throw_obj__8daSaku_cFi */
BOOL daSaku_c::mode_break_throw_obj(int id) {
    if (m_heap[id][0] && m_heap[id][1] && mHeapDeleteTimer[id]) {
        mHeapDeleteTimer[id]--;
        if (!mHeapDeleteTimer[id]) {
            mDoExt_destroySolidHeap(m_heap[id][0]);
            m_heap[id][0] = NULL;
            mModel[id][0] = NULL;
        }
    }
#if VERSION == VERSION_DEMO
    if (mParticleTimer[id] >= m_alpha_start_time && mSmokeEmitter[id]) {
        f32 step = l_sakuHIO.mDustAlpha / (255.0f * m_fade_time);
        cLib_chaseF(&mSmokeAlpha[id], 0.0f, step);
        mSmokeAlpha[id] = fabs(mSmokeAlpha[id]);
        u8 alpha = 255.0f * mSmokeAlpha[id];
        mSmokeEmitter[id]->setGlobalAlpha(alpha);
        if (alpha == 0) {
            mSmoke.end();
            mSmokeEmitter[id] = NULL;
        }
    }
#else
    if (mParticleTimer[id] >= m_alpha_start_time && mSmoke[id].getEmitter()) {
        cLib_chaseF(&mSmokeAlpha[id], 0.0f, l_sakuHIO.mDustAlpha / (255.0f * m_fade_time));
        mSmokeAlpha[id] = fabs(mSmokeAlpha[id]);
        u8 alpha = 255.0f * mSmokeAlpha[id];
        mSmoke[id].getEmitter()->setGlobalAlpha(alpha);
        if (alpha == 0) {
            mSmoke[id].end();
            mSmokeActive[id] = 0;
        }
    }
#endif
    return TRUE;
}

/* 0000083C-000008EC       .text RecreateHeap__8daSaku_cFii */
BOOL daSaku_c::RecreateHeap(int heap_id, int saku_id) {
    JUT_ASSERT(DEMO_SELECT(878, 869), m_heap[saku_id][heap_id] != 0);
    m_heap[saku_id][heap_id]->freeAll();
    JKRHeap* old = mDoExt_setCurrentHeap(m_heap[saku_id][heap_id]);
    CreateHeap(1, saku_id);
    mDoExt_setCurrentHeap(old);
    return TRUE;
}

/* 000008EC-000009B0       .text CreateHeap__8daSaku_cFii */
BOOL daSaku_c::CreateHeap(int heap_id, int saku_id) {
    int model_id;
    switch (mState[saku_id]) {
    case 1:
        model_id = 0;
        break;
    case 3:
        model_id = 2;
        break;
    case 2:
        model_id = 1;
        break;
    }
    if (!loadModel(model_id, heap_id, saku_id)) {
        return FALSE;
    }
    if (!loadMoveBG(GetDzbId(saku_id), heap_id, saku_id)) {
        return FALSE;
    }
    return TRUE;
}

/* 000009B0-00000A4C       .text GetDzbId__8daSaku_cFi */
int daSaku_c::GetDzbId(int id) {
    int state = mState[id];
    int result;
    if (id == 1 || mState[1] == 0) {
        if (state == 1) {
            result = 0;
        } else {
            result = 1;
        }
    } else if (state == 3 || state == 2) {
        result = 3;
    } else if (DEMO_SELECT(fopAcM_isSwitch(this, mTopHalfDestroyedSwitch), dComIfGs_isSwitch(mTopHalfDestroyedSwitch, home.roomNo))) {
        result = 4;
    } else {
        result = 2;
    }
    return result;
}

/* 00000A4C-00000ADC       .text CreateDummyHeap__8daSaku_cFi */
BOOL daSaku_c::CreateDummyHeap(int id) {
    int model_id;
    if (mSturdinessType == 0) {
        model_id = 0;
    } else if (mSturdinessType == 1) {
        model_id = 1;
    }
    if (!loadModel(model_id, 1, id)) {
        return FALSE;
    }
    if (!loadMoveBG(1, 1, id)) {
        return FALSE;
    }
    return TRUE;
}

/* 00000ADC-00000BE8       .text loadMoveBG__8daSaku_cFiii */
BOOL daSaku_c::loadMoveBG(int resource, int heap_id, int saku_id) {
    int ids[] = {dRes_INDEX_KSAKUCO_DZB_KSAKU_00_e, dRes_INDEX_KSAKUCO_DZB_KSAKU_01_e, dRes_INDEX_KSAKUCO_DZB_KSAKU_02_e, dRes_INDEX_KSAKUCO_DZB_KSAKU_03_e, dRes_INDEX_KSAKUCO_DZB_KSAKU_00_e};
    mBgW[saku_id][heap_id] = new dBgW();
    if (mBgW[saku_id][heap_id]) {
        if (mBgW[saku_id][heap_id]->Set((cBgD_t*)dComIfG_getObjectRes(m_arcname[0], ids[resource]), 1, &mBgMtx[saku_id]) == 1) {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}

/* 00000BE8-00000D7C       .text loadModel__8daSaku_cFiii */
BOOL daSaku_c::loadModel(int resource, int heap_id, int saku_id) {
    int wooden[] = {dRes_INDEX_KSAKU_00_BDL_KSAKU_00_e, dRes_INDEX_KSAKU_00_BDL_KSAKU_04_e, dRes_INDEX_KSAKU_00_BDL_KSAKU_02_e, dRes_INDEX_KSAKU_00_BDL_KSAKU_01_e, dRes_INDEX_KSAKU_00_BDL_KSAKU_03_e, dRes_INDEX_KSAKU_00_BDL_KSAKU_05_e};
    int sturdy[] = {dRes_INDEX_KNSAK_00_BDL_KNSAK_00_e, dRes_INDEX_KNSAK_00_BDL_KNSAK_02_e, dRes_INDEX_KNSAK_00_BDL_KNSAK_01_e, dRes_INDEX_KNSAK_00_BDL_KNSAK_03_e, dRes_INDEX_KNSAK_00_BDL_KNSAK_05_e, dRes_INDEX_KNSAK_00_BDL_KNSAK_04_e};
    if (saku_id == 1) {
        resource += 3;
    }
    J3DModelData* modelData;
    if (mSturdinessType == 0) {
        modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname[1], wooden[resource]);
    } else if (mSturdinessType == 1) {
        modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname[2], sturdy[resource]);
    }
    JUT_ASSERT(DEMO_SELECT(1094, 1085), modelData != 0);
    mModel[saku_id][heap_id] = mDoExt_J3DModel__create(modelData, 0, 0x11020203);
    if (!mModel[saku_id][heap_id]) {
        return FALSE;
    }
    return TRUE;
}

/* 00000D7C-00000E8C       .text burn__8daSaku_cFv */
BOOL daSaku_c::burn() {
    if (!mBurning) {
        if (mState[0] == 1) {
            mState[0] = 2;
            RecreateHeap(1, 0);
            mCollisionTimer[0] = 50;
        }
        if (mState[1] == 1) {
            mState[1] = 2;
            RecreateHeap(1, 1);
            mCollisionTimer[1] = 50;
        }
        if (mModel[0][1]) {
            fopAcM_SetMtx(this, mModel[0][1]->getBaseTRMtx());
        } else if (mModel[1][1]) {
            fopAcM_SetMtx(this, mModel[1][1]->getBaseTRMtx());
        }
        setEffFire(0);
        mFireTimer = 90;
        DEMO_SELECT(fopAcM_onSwitch(this, mBottomHalfDestroyedSwitch), dComIfGs_onSwitch(mBottomHalfDestroyedSwitch, home.roomNo));
        if (mState[1] != 0) {
            DEMO_SELECT(fopAcM_onSwitch(this, mTopHalfDestroyedSwitch), dComIfGs_onSwitch(mTopHalfDestroyedSwitch, home.roomNo));
        }
        mBurning = 1;
    }
    return TRUE;
}

/* 00000E8C-00000F60       .text broken__8daSaku_cFi */
BOOL daSaku_c::broken(int id) {
    setEffBreak(id);
    mState[id] = 3;
    mCollisionTimer[id] = 0;
    if (id == 0) {
        DEMO_SELECT(fopAcM_onSwitch(this, mBottomHalfDestroyedSwitch), dComIfGs_onSwitch(mBottomHalfDestroyedSwitch, home.roomNo));
    } else {
        DEMO_SELECT(fopAcM_onSwitch(this, mTopHalfDestroyedSwitch), dComIfGs_onSwitch(mTopHalfDestroyedSwitch, home.roomNo));
    }
    RecreateHeap(1, id);
    if (id == 0) {
        fopAcM_SetMtx(this, mModel[id][1]->getBaseTRMtx());
    }
    mAlpha[id][0] = 0;
    mAlpha[id][1] = 255;
    return TRUE;
}

/* 00000F60-00000FF4       .text changeCollision__8daSaku_cFi */
BOOL daSaku_c::changeCollision(int id) {
    if (mState[id] == 0) {
        return FALSE;
    }
    if (mCollisionTimer[id] >= 0) {
        if (mCollisionTimer[id] == 0) {
            dComIfG_Bgsp()->Release(mActiveBgW[id]);
            MoveBGResist(1, id);
        }
        mCollisionTimer[id]--;
    }
    return TRUE;
}

/* 00000FF4-0000113C       .text setMtx__8daSaku_cFv */
void daSaku_c::setMtx() {
    for (int i = 0; i < 2; i++) {
        J3DModel* model = mModel[0][i];
        if (model) {
            model->setBaseScale(scale);
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::ZXYrotM(shape_angle);
            model->setBaseTRMtx(mDoMtx_stack_c::get());
        }
    }
    if (mState[1] != 0) {
        for (int i = 0; i < 2; i++) {
            J3DModel* model = mModel[1][i];
            if (model) {
                model->setBaseScale(scale);
                mDoMtx_stack_c::transS(current.pos.x, 200.0f + current.pos.y, current.pos.z);
                mDoMtx_stack_c::ZXYrotM(shape_angle);
                model->setBaseTRMtx(mDoMtx_stack_c::get());
            }
        }
    }
}

/* 0000113C-0000120C       .text setMoveBGMtx__8daSaku_cFv */
void daSaku_c::setMoveBGMtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::scaleM(scale);
    MTXCopy(mDoMtx_stack_c::get(), mBgMtx[0]);
    if (mState[1] != 0) {
        mDoMtx_stack_c::transS(current.pos.x, 200.0f + current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(shape_angle.y);
        mDoMtx_stack_c::scaleM(scale);
        MTXCopy(mDoMtx_stack_c::get(), mBgMtx[1]);
    }
}

/* 0000120C-0000134C       .text checkCol__8daSaku_cFv */
void daSaku_c::checkCol() {
    if (mFireTimer != 0) {
        for (int i = 0; i < 3; i++) {
            mAtCyl[i].SetC(mCylPos[0][i]);
            dComIfG_Ccsp()->Set(&mAtCyl[i]);
        }
    }
    if (mState[0] == 1) {
        for (int i = 0; i < 3; i++) {
            mCyl[0][i].SetC(mCylPos[0][i]);
            dComIfG_Ccsp()->Set(&mCyl[0][i]);
        }
    }
    if (mState[1] != 0) {
        if (mState[1] == 1) {
            for (int i = 0; i < 3; i++) {
                mCyl[1][i].SetC(mCylPos[1][i]);
                dComIfG_Ccsp()->Set(&mCyl[1][i]);
            }
        }
    }
}

/* 0000134C-00001510       .text setCol__8daSaku_cFv */
void daSaku_c::setCol() {
    mCylPos[0][0].set(0.0f, 20.0f, 0.0f);
    mCylPos[0][1].set(-100.0f, 20.0f, 0.0f);
    mCylPos[0][2].set(100.0f, 20.0f, 0.0f);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    for (int i = 0; i < 3; i++) {
        mDoMtx_stack_c::multVec(&mCylPos[0][i], &mCylPos[0][i]);
        mCyl[0][i].Set(m_cyl_src);
        mCyl[0][i].SetStts(&mStts);
    }
    if (mState[1] != 0) {
        mCylPos[1][0].set(0.0f, 220.0f, 0.0f);
        mCylPos[1][1].set(-100.0f, 220.0f, 0.0f);
        mCylPos[1][2].set(100.0f, 220.0f, 0.0f);
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::ZXYrotM(shape_angle);
        for (int i = 0; i < 3; i++) {
            mDoMtx_stack_c::multVec(&mCylPos[1][i], &mCylPos[1][i]);
            mCyl[1][i].Set(m_cyl_src);
            mCyl[1][i].SetStts(&mStts);
        }
    }
}

/* 00001510-00001598       .text MoveBGResist__8daSaku_cFii */
BOOL daSaku_c::MoveBGResist(int heap_id, int saku_id) {
    if (dComIfG_Bgsp()->Regist(mBgW[saku_id][heap_id], this)) {
        return FALSE;
    }
    mActiveBgW[saku_id] = mBgW[saku_id][heap_id];
    mActiveBgW[saku_id]->Move();
    return TRUE;
}

/* 00001598-000016C0       .text setEffFire__8daSaku_cFi */
BOOL daSaku_c::setEffFire(int) {
    cXyz pos = current.pos;
    dComIfGp_particle_set(0x045c, &pos, &current.angle);
#if VERSION == VERSION_DEMO
    g_dComIfG_gameInfo.play.getParticle()->set(0, 0x245e, &pos, &current.angle, NULL, m_smoke_alpha, NULL, -1, NULL, NULL, NULL);
#else
    dComIfGp_particle_set(0x245e, &pos, &current.angle, NULL, m_smoke_alpha);
#endif
    mParticleTimer[0] = mParticleTimer[1] = 1;
    fopAcM_seStart(this, JA_SE_OBJ_BURN_WRAILING, 0);
    return TRUE;
}

/* 000016C0-000019AC       .text setEffBreak__8daSaku_cFi */
BOOL daSaku_c::setEffBreak(int id) {
    cXyz pos = current.pos;
    pos.y += 100.0f;
    if (id == 1) {
        pos.y += 200.0f;
    }
    if (l_sakuHIO.mDebris) {
        dComIfGp_particle_set(0x045d, &pos, &current.angle, &scale, 255, NULL, -1, &tevStr.mColorK0, &tevStr.mColorK0);
    }
    mSmokeAlpha[id] = l_sakuHIO.mDustAlpha / 255.0f;
    dust_color.r = l_sakuHIO.mDustR;
    dust_color.g = l_sakuHIO.mDustG;
    dust_color.b = l_sakuHIO.mDustB;
    mSmokePos[id] = pos;
#if VERSION == VERSION_DEMO
    mSmokeEmitter[id] = dComIfGp_particle_setToon(0x2027, &mSmokePos[id], &current.angle, NULL, l_sakuHIO.mDustAlpha, &mSmoke, fopAcM_GetRoomNo(this));
    if (mSmokeEmitter[id]) {
        mSmokeEmitter[id]->setGlobalAlpha(255.0f * mSmokeAlpha[id]);
        mSmokeEmitter[id]->becomeImmortalEmitter();
        cXyz dynamics(2.0f, 2.0f, 2.0f), emitter(1.0f, 0.5f, 0.7f);
        mSmokeEmitter[id]->setGlobalParticleScale(3.2f, 3.2f);
        mSmokeEmitter[id]->setGlobalDynamicsScale(dynamics);
        mSmokeEmitter[id]->setEmitterScale(emitter);
        mSmokeEmitter[id]->setRate(40.0f);
        mSmokeEmitter[id]->setMaxFrame(1);
    }
#else
    dComIfGp_particle_setToon(0x2027, &mSmokePos[id], &current.angle, NULL, l_sakuHIO.mDustAlpha, &mSmoke[id], fopAcM_GetRoomNo(this));
    if (mSmoke[id].getEmitter()) {
        mSmoke[id].getEmitter()->setGlobalAlpha(255.0f * mSmokeAlpha[id]);
        mSmoke[id].getEmitter()->becomeImmortalEmitter();
        cXyz dynamics(2.0f, 2.0f, 2.0f), emitter(1.0f, 0.5f, 0.7f);
        mSmoke[id].getEmitter()->setGlobalParticleScale(3.2f, 3.2f);
        mSmoke[id].getEmitter()->setGlobalDynamicsScale(dynamics);
        mSmoke[id].getEmitter()->setEmitterScale(emitter);
        mSmoke[id].getEmitter()->setRate(40.0f);
        mSmoke[id].getEmitter()->setMaxFrame(1);
    }
#endif
    if (mSturdinessType == 0) {
        fopAcM_seStart(this, JA_SE_OBJ_COL_BRK_WRAILING, 0);
    } else if (mSturdinessType == 1) {
        fopAcM_seStart(this, JA_SE_OBJ_BRK_WRAIL_K, 0);
    }
    mParticleTimer[id] = 1;
    return TRUE;
}

/* 000019AC-00001A50       .text matAlphaAnim__FP12J3DModelDataUcb */
BOOL matAlphaAnim(J3DModelData* modelData, u8 alpha, bool zWrite) {
    JUT_ASSERT(DEMO_SELECT(1520, 1489), modelData != 0);
    for (u16 i = 0; i < modelData->getMaterialNum(); i++) {
        changeXluMaterialAlpha(modelData->getMaterialNodePointer(i), alpha, zWrite);
    }
    return TRUE;
}

/* 00001A50-00001B98       .text changeXluMaterialAlpha__FP11J3DMaterialUcb */
void changeXluMaterialAlpha(J3DMaterial* i_material, u8 alpha, bool zWrite) {
    static J3DBlendInfo l_blendInfo = {GX_BM_BLEND, GX_BL_SRC_ALPHA, GX_BL_INV_SRC_ALPHA, GX_LO_OR};
    static J3DZModeInfo l_zmodeInfo = {GX_TRUE, GX_LEQUAL, GX_FALSE};
    static J3DZModeInfo l_zmodeInfo2 = {GX_TRUE, GX_LEQUAL, GX_TRUE};
    JUT_ASSERT(DEMO_SELECT(1566, 1535), i_material != 0);
    J3DPEBlock* pe = i_material->getPEBlock();
    i_material->getTevKColor(3)->mColor.a = alpha;
    pe->getBlend()->setBlendInfo(l_blendInfo);
    if (zWrite) {
        pe->getZMode()->setZModeInfo(l_zmodeInfo2);
    } else {
        pe->getZMode()->setZModeInfo(l_zmodeInfo);
    }
}

inline BOOL daSaku_c::_daSaku_execute() {
    int i;
    for (i = 0; i < 2; i++) {
        if (mParticleTimer[i] != 0 && mParticleTimer[i] < daSaku_c::m_max_particle_timer) {
            mParticleTimer[i]++;
        }
    }
    if (mFireTimer != 0) {
        mFireTimer--;
    }
    mFrame++;
    for (i = 0; i < 2; i++) {
        switch (mState[i]) {
        case 0:
            break;
        case 1:
            mode_break_none(i);
            break;
        case 3:
            mode_break_throw_obj(i);
            break;
        case 2:
            mode_break_fire(i);
            break;
        }
    }
    for (i = 0; i < 2; i++) {
        changeCollision(i);
    }
    setMtx();
    checkCol();
    return TRUE;
}

const char* daSaku_c::m_arcname[] = {"KsakuCo", "Ksaku_00", "Knsak_00"};
u8 daSaku_c::m_smoke_alpha = 230;
int daSaku_c::m_alpha_start_time = 10;
int daSaku_c::m_saku_alpha_out_time = 10;
int daSaku_c::m_fade_time = 40;
GXColor daSaku_c::dust_color = {105, 91, 48, 255};
const int daSaku_c::m_max_particle_timer = 2000;
const u8 daSaku_c::m_start_alpha = 180;
const s16 daSaku_c::m_alpha_spd = 5;
const f32 daSaku_c::m_saku_height = 200.0f;
const dCcD_SrcCyl daSaku_c::m_at_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_FIRE | AT_TYPE_UNK20000,
        /* SrcObjAt  Atp     */ 1,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_GrpAll_e,
        /* SrcObjTg  Type    */ 0,
        /* SrcObjTg  SPrm    */ 0,
        /* SrcObjCo  SPrm    */ 0,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ 0,
        /* SrcGObjAt Spl     */ 0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 1,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ 6,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, 0.0f, 0.0f}, 50.0f, 400.0f}},
};
dCcD_SrcCyl daSaku_c::m_cyl_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ AT_TYPE_SWORD | AT_TYPE_UNK8 | AT_TYPE_BOMB | AT_TYPE_FIRE | AT_TYPE_MACHETE | AT_TYPE_UNK800 | AT_TYPE_SKULL_HAMMER | AT_TYPE_UNK20000 | AT_TYPE_FIRE_ARROW,
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
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoHitMark_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCylS
    {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 50.0f,
            /* Height */ 200.0f,
        }},
};
/* 00001B98-00001BB8       .text daSaku_Create__FP10fopAc_ac_c */
static cPhs_State daSaku_Create(fopAc_ac_c* i_this) {
    return ((daSaku_c*)i_this)->_daSaku_create();
}

/* 00001BB8-00001F28       .text _daSaku_create__8daSaku_cFv */
cPhs_State daSaku_c::_daSaku_create() {
    u32 heapSize = 0;
    fopAcM_SetupActor(this, daSaku_c);
    mSturdinessType = (fopAcM_GetParam(this) >> 4) & 15;
    int arc = 2;
    if (mSturdinessType == 0) {
        arc = 1;
    }
#if VERSION == VERSION_DEMO
    cPhs_State modelPhase = dComIfG_resLoad(&mModelPhase, m_arcname[arc]);
    cPhs_State collisionPhase = dComIfG_resLoad(&mCollisionPhase, m_arcname[0]);
    if (modelPhase == cPhs_COMPLEATE_e) {
        setFlag(1);
    }
    if (collisionPhase == cPhs_COMPLEATE_e) {
        setFlag(2);
    }
    if (modelPhase == cPhs_ERROR_e || collisionPhase == cPhs_ERROR_e) {
        return cPhs_ERROR_e;
    }
    if (modelPhase != cPhs_COMPLEATE_e) {
        return modelPhase;
    }
    if (collisionPhase != cPhs_COMPLEATE_e) {
        return collisionPhase;
    }
#else
    cPhs_State phase = dComIfG_resLoad(&mModelPhase, m_arcname[arc]);
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    phase = dComIfG_resLoad(&mCollisionPhase, m_arcname[0]);
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
#endif
    mBottomHalfDestroyedSwitch = (fopAcM_GetParam(this) >> 8) & 255;
    mTopHalfDestroyedSwitch = (fopAcM_GetParam(this) >> 16) & 255;
    mState[0] = 1;
    if (DEMO_SELECT(fopAcM_isSwitch(this, mBottomHalfDestroyedSwitch), dComIfGs_isSwitch(mBottomHalfDestroyedSwitch, home.roomNo))) {
        mState[0] = 3;
    }
    mState[1] = 0;
    if ((fopAcM_GetParam(this) & 15) != 0) {
        mState[1] = 1;
        if (DEMO_SELECT(fopAcM_isSwitch(this, mTopHalfDestroyedSwitch), dComIfGs_isSwitch(mTopHalfDestroyedSwitch, home.roomNo))) {
            mState[1] = 3;
        }
    }
    setMoveBGMtx();
    for (int i = 0; i < 2; i++) {
        if (mState[i] != 0) {
            m_heap[i][0] = mDoExt_createSolidHeapFromGameToCurrent(0x2280, 0x20);
            if (!m_heap[i][0]) {
                return cPhs_ERROR_e;
            }
            BOOL ok = CreateHeap(0, i);
            if (ok) {
                MoveBGResist(0, i);
            }
            mDoExt_restoreCurrentHeap();
            heapSize += mDoExt_adjustSolidHeap(m_heap[i][0]);
            if (!ok) {
                return cPhs_ERROR_e;
            }
            mModel[i][1] = NULL;
            if (mState[i] == 1) {
                m_heap[i][1] = mDoExt_createSolidHeapFromGameToCurrent(0x2280, 0x20);
                if (!m_heap[i][1]) {
                    return cPhs_ERROR_e;
                }
                ok = CreateDummyHeap(i);
                mModel[i][1] = NULL;
                mDoExt_restoreCurrentHeap();
                heapSize += mDoExt_adjustSolidHeap(m_heap[i][1]);
                if (!ok) {
                    return cPhs_ERROR_e;
                }
            }
        }
    }
    CreateInit();
    if (l_sakuHIO.mNo < 0) {
        l_sakuHIO.mNo = mDoHIO_createChild("柵", &l_sakuHIO);
    }
    return cPhs_COMPLEATE_e;
}

/* 00002264-000023D8       .text daSaku_Delete__FP8daSaku_c */
inline BOOL daSaku_c::_daSaku_delete() {
    if (l_sakuHIO.mNo >= 0) {
        mDoHIO_deleteChild(l_sakuHIO.mNo);
        l_sakuHIO.mNo = -1;
    }
#if VERSION == VERSION_DEMO
    mSmoke.end();
#else
    for (int i = 0; i < 2; i++) {
        mSmoke[i].end();
    }
#endif
    for (int i = 0; i < 2; i++) {
        if (mState[i] != 0) {
            dComIfG_Bgsp()->Release(mActiveBgW[i]);
        }
    }
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            if (m_heap[i][j]) {
                mDoExt_destroySolidHeap(m_heap[i][j]);
                m_heap[i][j] = NULL;
                mModel[i][j] = NULL;
            }
        }
    }
#if VERSION == VERSION_DEMO
    if (chkFlag(2)) {
        dComIfG_deleteObjectRes(daSaku_c::m_arcname[0]);
    }
    if (chkFlag(1)) {
        if (mSturdinessType == 0) {
            dComIfG_deleteObjectRes(daSaku_c::m_arcname[1]);
        } else if (mSturdinessType == 1) {
            dComIfG_deleteObjectRes(daSaku_c::m_arcname[2]);
        }
    }
#else
    dComIfG_resDelete(&mCollisionPhase, daSaku_c::m_arcname[0]);
    if (mSturdinessType == 0) {
        dComIfG_resDelete(&mModelPhase, daSaku_c::m_arcname[1]);
    } else {
        dComIfG_resDelete(&mModelPhase, daSaku_c::m_arcname[2]);
    }
#endif
    return TRUE;
}

static BOOL daSaku_Delete(daSaku_c* i_this) {
    return i_this->_daSaku_delete();
}

/* 000023D8-000023E0       .text daSaku_IsDelete__FP8daSaku_c */
static BOOL daSaku_IsDelete(daSaku_c*) {
    return TRUE;
}

/* 000023E0-0000242C       .text daSaku_Draw__FP8daSaku_c */
static BOOL daSaku_Draw(daSaku_c* i_this) {
    i_this->saku_draw_sub(0);
    if (i_this->mState[1] != 0) {
        i_this->saku_draw_sub(1);
    }
    return TRUE;
}

/* 0000242C-00002560       .text daSaku_Execute__FP8daSaku_c */
static BOOL daSaku_Execute(daSaku_c* i_this) {
    return i_this->_daSaku_execute();
}

static actor_method_class l_daSaku_Method = {
    (process_method_func)daSaku_Create,
    (process_method_func)daSaku_Delete,
    (process_method_func)daSaku_Execute,
    (process_method_func)daSaku_IsDelete,
    (process_method_func)daSaku_Draw,
};

actor_process_profile_definition g_profile_SAKU = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_SAKU_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daSaku_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_SAKU_e,
    /* Actor SubMtd */ &l_daSaku_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_9_e,
};
