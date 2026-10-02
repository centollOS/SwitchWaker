/**
 * d_a_obj_vyasi.cpp
 * Object - Palm tree blowing in the wind (Gale Isle)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_vyasi.h"
#include "d/d_lib.h"
#include "d/d_kankyo_wether.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "res/Object/Vyasi.h"
#include <math.h>

namespace daObjVyasi {
namespace {
struct Attr_c {
    f32 scale;
    f32 radius;
    f32 height;
    Vec offset;
    f32 quaternionRate;
    f32 leafSpeed;
    s16 count;
    Vec leafAngle;
    f32 reserved[21];
};
const Attr_c L_attr = {1.0f, 79.0f, 250.0f, {0.0f, 0.0f, 0.0f}, 0.4f, 1.5f, 2, {700.0f, 1700.0f, 1700.0f}, {0.0f}};
}
static const dCcD_SrcCyl M_cyl_src = {
    // dCcD_SrcGObjInf
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

static const dCcD_SrcCps M_cps_src = {
    // dCcD_SrcGObjInf
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
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK1,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 100.0f}},
};

static const dCcD_SrcSph M_sph_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ 0,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ 0,
        /* SrcObjTg  Type    */ 0,
        /* SrcObjTg  SPrm    */ 0,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK1,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ 0,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, 0.0f, 0.0f}, 100.0f}},
};

static u8 joint_kind_table[] = {2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
}

/* 000000EC-0000015C       .text SetStopJointAnimation__Q210daObjVyasi5Act_cFP18J3DAnmTransformKeyff */
BOOL daObjVyasi::Act_c::SetStopJointAnimation(J3DAnmTransformKey* animation, f32 speed, f32 morph) {
    if (animation) {
        mpMorf->setAnm(animation, 0, morph, speed, 0.0f, -1.0f, NULL);
        m19C4 = 1;
        return TRUE;
    }
    return FALSE;
}

/* 0000015C-00000194       .text PlayStopJointAnimation__Q210daObjVyasi5Act_cFv */
BOOL daObjVyasi::Act_c::PlayStopJointAnimation() {
    return mpMorf->play(NULL, 0, 0) ? FALSE : TRUE;
}

/* 00000194-0000021C       .text set_first_process__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::set_first_process() {
    int sw = daObj::PrmAbstract<int>(this, 8, 0);
    bool is_sw = dComIfGs_isSwitch(sw, fopAcM_GetHomeRoomNo(this));
    int state = is_sw ? 4 : 1;
    process_init(state);
    mNormalCounter = 0;
    m19D4 = 1.0f;
    shape_angle.y += 0x8000;
}

/* 0000021C-000005B8       .text set_collision__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::set_collision() {
    if (mCyl.ChkTgHit()) {
        mCyl.GetTgHitObj();
        daObj::HitSeStart(&current.pos, fopAcM_GetRoomNo(this), &mCyl, 7);
        daObj::HitEff_kikuzu(this, &mCyl);
        dKy_Sound_set(current.pos, 4, fopAcM_GetID(this), 100);
        mCyl.ClrTgHit();
    } else {
        mCyl.SetR(79.0f);
        mCyl.SetH(250.0f);
        mCyl.SetC(current.pos);
        dComIfG_Ccsp()->Set(&mCyl);
    }
    for (int i = 0; i < 5; i++) {
        if (mCps[i].ChkTgHit()) {
            mCps[i].GetTgHitObj();
            daObj::HitSeStart(&current.pos, fopAcM_GetRoomNo(this), &mCps[i], 7);
            dKy_Sound_set(current.pos, 4, fopAcM_GetID(this), 100);
            mCps[i].ClrTgHit();
        } else {
            int next = i + 1;
            mCpsPos[i].mStart = m0400[i];
            mCpsPos[i].mEnd = m0400[next];
            mCpsPos[i].mRadius = 47.4f;
            mCps[i].cM3dGCps::Set(mCpsPos[i]);
            dComIfG_Ccsp()->Set(&mCps[i]);
        }
    }
    for (int i = 0; i < 8; i += 2) {
        int joint = (i >> 1) + 1;
        int next = (i >> 1) + 2;
        cXyz delta(0.33333f * (m0400[next].x - m0400[joint].x), 0.33333f * (m0400[next].y - m0400[joint].y), 0.33333f * (m0400[next].z - m0400[joint].z));
        cXyz pos;
        pos.x = m0400[joint].x + delta.x;
        pos.y = m0400[joint].y + delta.y;
        pos.z = m0400[joint].z + delta.z;
        mSph[i].SetC(pos);
        mSph[i].SetR(47.4f);
        dComIfG_Ccsp()->Set(&mSph[i]);
        pos.x = m0400[joint].x + 2.0f * delta.x;
        pos.y = m0400[joint].y + 2.0f * delta.y;
        pos.z = m0400[joint].z + 2.0f * delta.z;
        mSph[i + 1].SetC(pos);
        mSph[i + 1].SetR(47.4f);
        dComIfG_Ccsp()->Set(&mSph[i + 1]);
    }
}

/* 000005F4-000009B8       .text JointNodeCallBack__10daObjVyasiFP7J3DNodei */
BOOL daObjVyasi::JointNodeCallBack(J3DNode* node, int phase) {
    J3DModel* model = j3dSys.getModel();
    int joint = ((J3DJoint*)node)->getJntNo();
    Act_c* actor = (Act_c*)model->getUserArea();
    if (phase == 0) {
        mDoMtx_stack_c::copy(model->getAnmMtx(joint));
        Mtx matrix;
        MTXCopy(model->getAnmMtx(joint), matrix);
        cXyz pos(matrix[0][3], matrix[1][3], matrix[2][3]);
        matrix[0][3] = matrix[1][3] = matrix[2][3] = 0.0f;
        mDoMtx_stack_c::transS(pos);
        mDoMtx_stack_c::quatM(&actor->mJointQuat[joint]);
        MTXConcat(mDoMtx_stack_c::get(), matrix, mDoMtx_stack_c::get());
        model->setAnmMtx(joint, mDoMtx_stack_c::get());
        MTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        csXyz angles[] = {
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
            csXyz(0, 0, 0),
        };
        csXyz angle = angles[joint];
        angle += actor->m03AC[joint];
        mDoMtx_stack_c::copy(model->getAnmMtx(joint));
        mDoMtx_stack_c::ZXYrotM(angle);
        if (joint_kind_table[joint] == 0) {
            mDoMtx_stack_c::scaleM(actor->m04A8, actor->m04AC, actor->m04B0);
        }
        model->setAnmMtx(joint, mDoMtx_stack_c::get());
        MTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        cXyz zero(0.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::multVec(&zero, &actor->m0400[joint]);
    }
    return TRUE;
}

/* 000009F4-000009FC       .text process_none_init__Q210daObjVyasi5Act_cFv */
BOOL daObjVyasi::Act_c::process_none_init() {
    return TRUE;
}

/* 000009FC-00000A00       .text process_none_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::process_none_main() {
}

/* 00000A00-00000A64       .text process_sag_init__Q210daObjVyasi5Act_cFv */
BOOL daObjVyasi::Act_c::process_sag_init() {
    if (SetStopJointAnimation(M_bck_data, 1.0f, 0.0f)) {
        mpMorf->setPlaySpeed(0.0f);
        return TRUE;
    }
    return FALSE;
}

/* 00000A64-00000AD8       .text process_sag_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::process_sag_main() {
    fopAc_ac_c* actor = fopAcM_SearchByName(fpcNm_Obj_Ekskz_e);
    if (actor) {
        mEkszsPos = actor->current.pos;
        mEkszsRotY = actor->shape_angle.y;
        process_init(2);
    }
}

/* 00000AD8-00000CC0       .text process_sagWind_init__Q210daObjVyasi5Act_cFv */
BOOL daObjVyasi::Act_c::process_sagWind_init() {
    if (SetStopJointAnimation(M_bck_data, 1.0f, 3.0f)) {
        f32 distance = mEkszsPos.abs(current.pos);
        distance = distance > 2800.0f ? 2800.0f : distance;
        distance = distance < 1000.0f ? 1000.0f : distance;
        m0504 = (distance - 2800.0f) / -1800.0f;
        f32 speed = 5000.0f + 7000.0f * m0504;
        for (int i = 0; i < 14; i++) {
            if (joint_kind_table[i] == 0) {
                if ((i & 1) == 0) {
                    m0524[i] = speed + cM_rndF(2000.0f);
                } else {
                    m0524[i] = -(speed + cM_rndF(2000.0f));
                }
            } else m0524[i] = 0.5f * speed;
        }
        mpMorf->setPlaySpeed(0.0f);
        return TRUE;
    }
    return FALSE;
}

/* 00000CC0-00000D20       .text process_sagWind_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::process_sagWind_main() {
    int sw = daObj::PrmAbstract<int>(this, 8, 0);
    if (dComIfGs_isSwitch(sw, fopAcM_GetHomeRoomNo(this))) {
        process_init(3);
    }
}

/* 00000D20-00000D54       .text process_toNormal_init__Q210daObjVyasi5Act_cFv */
BOOL daObjVyasi::Act_c::process_toNormal_init() {
    return SetStopJointAnimation(M_bck_data, 1.0f, 0.0f);
}

/* 00000D54-00000E10       .text process_toNormal_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::process_toNormal_main() {
    if (m19C4 == 0) {
        if ((f32)fabs(m19CC) <= 0.1f) {
            if (process_init(4)) {
                m19CC = 0.0f;
                m19D4 = 0.0f;
                mNormalCounter = 2;
            }
        }
        m19CC *= 0.85f;
        m19D0 += 0x3000;
    } else {
        m19CC = -1792.0f * m0504;
        m19D0 = 0;
    }
}

/* 00000E10-00000E74       .text process_normal_init__Q210daObjVyasi5Act_cFv */
BOOL daObjVyasi::Act_c::process_normal_init() {
    if (SetStopJointAnimation(M_bck_data, -1.0f, 0.0f)) {
        mpMorf->setPlaySpeed(0.0f);
        return TRUE;
    }
    return FALSE;
}

/* 00000E74-00000ED0       .text process_normal_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::process_normal_main() {
    if (mNormalCounter == 0 || mNormalCounter == 1) {
        mNormalCounter++;
    }
    cLib_addCalc(&m19D4, 1.0f, 0.01f, 1.0f, 0.007f);
}

/* 00000ED0-00000FE4       .text process_init__Q210daObjVyasi5Act_cFi */
BOOL daObjVyasi::Act_c::process_init(int state) {
    typedef BOOL (Act_c::*Init)();
    static Init init[] = {&Act_c::process_none_init, &Act_c::process_sag_init, &Act_c::process_sagWind_init, &Act_c::process_toNormal_init, &Act_c::process_normal_init};
    if (state >= 0 && state < 5 && (this->*init[state])()) {
        mState = state;
        return TRUE;
    }
    return FALSE;
}

/* 00000FE4-000010C8       .text process_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::process_main() {
    typedef void (Act_c::*Main)();
    static Main main[] = {&Act_c::process_none_main, &Act_c::process_sag_main, &Act_c::process_sagWind_main, &Act_c::process_toNormal_main, &Act_c::process_normal_main};
    if (mState >= 0 && mState < 5) {
        (this->*main[mState])();
    }
}

const char daObjVyasi::Act_c::M_arcname[] = "Vyasi";
/* 000010C8-000010EC       .text solidHeapCB__Q210daObjVyasi5Act_cFP10fopAc_ac_c */
BOOL daObjVyasi::Act_c::solidHeapCB(fopAc_ac_c* actor) {
    return static_cast<Act_c*>(actor)->create_heap();
}

/* 000010EC-00001290       .text create_heap__Q210daObjVyasi5Act_cFv */
bool daObjVyasi::Act_c::create_heap() {
    J3DModelData* mdl_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VYASI_BDL_VYASI_e);
    JUT_ASSERT(1146, mdl_data != 0);
    M_bck_data = (J3DAnmTransformKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_VYASI_BCK_VYASI_e);
    JUT_ASSERT(1151, M_bck_data != 0);
#if VERSION == VERSION_DEMO
    mpMorf = new mDoExt_McaMorf(mdl_data, NULL, NULL, M_bck_data, 0, 1.0f, 0, -1, TRUE, NULL, 0, 0x11000002);
#else
    if (M_bck_data && mdl_data) {
        mpMorf = new mDoExt_McaMorf(mdl_data, NULL, NULL, M_bck_data, 0, 1.0f, 0, -1, TRUE, NULL, 0, 0x11000002);
    }
#endif
    return bool(M_bck_data && mpMorf) && mpMorf->getModel();
}

/* 00001290-000016E0       .text _create__Q210daObjVyasi5Act_cFv */
cPhs_State daObjVyasi::Act_c::_create() {
    fopAcM_SetupActor(this, Act_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0)) {
            set_first_process();
            set_mtx();
            fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
            fopAcM_setCullSizeBox(this, -2000.0f, 0.0f, -2000.0f, 2000.0f, 2000.0f, 2000.0f);
            fopAcM_setCullSizeFar(this, 2.0f);
            mStts.Init(255, 255, this);
            mCyl.Set(M_cyl_src);
            mCyl.SetStts(&mStts);
            *mCyl.GetTgVecP() = cXyz::Zero;
            mCyl.OnTgNoHitMark();
            for (int i = 0; i < 5; i++) {
                mCpsStts[i].Init(100, 255, this);
                mCps[i].Set(M_cps_src);
                mCps[i].SetStts(&mCpsStts[i]);
                mCpsPos[i].mStart = current.pos;
                mCpsPos[i].mEnd = current.pos;
                mCpsPos[i].mRadius = 100.0f;
            }
            for (int i = 0; i < 8; i++) {
                mSphStts[i].Init(100, 255, this);
                mSph[i].Set(M_sph_src);
                mSph[i].SetStts(&mSphStts[i]);
                *mCyl.GetTgVecP() = cXyz::Zero;
                mCyl.OnTgNoHitMark();
            }
            J3DModel* model = mpMorf->getModel();
            J3DModelData* data = model->getModelData();
            model->setUserArea((u32)this);
            for (u16 i = 0; i < model->getModelData()->getJointNum(); i++) {
                data->getJointNodePointer(i)->setCallBack(JointNodeCallBack);
            }
            for (u16 i = 0; i < 14; i++) {
                mJointQuat[i] = ZeroQuat;
            }
            m04A8 = 1.0f;
            m04AC = 1.0f;
            m04B0 = 1.0f;
        } else phase = cPhs_ERROR_e;
    }
    return phase;
}

/* 00001D8C-00001DBC       .text _delete__Q210daObjVyasi5Act_cFv */
bool daObjVyasi::Act_c::_delete() {
    dComIfG_resDelete(&mPhs, M_arcname);
    return true;
}

/* 00001DBC-00001E5C       .text set_mtx__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::set_mtx() {
    mpMorf->getModel()->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    MTXCopy(mDoMtx_stack_c::get(), m04BC);
}

/* 00001E5C-000025A8       .text calc_dif_angle__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::calc_dif_angle() {
    for (int i = 0; i < 14; i++) {
        csXyz angle(0, 0, 0);
        s16 rate = 2;
        if (mState == 2) {
            if (joint_kind_table[i] == 2) {
                f32 sine = m0504 * cM_ssin(m0508[i]);
                angle.x = 20.0f * sine;
                angle.y = 40.0f * sine;
                angle.z = 40.0f * sine;
            } else if (joint_kind_table[i] == 1) {
                f32 sine = m0504 * cM_ssin(m0508[i]);
                angle.x = 120.0f * sine;
                angle.y = 180.0f * sine;
                angle.z = 220.0f * sine;
                if (i == 1) {
                    angle.z += (s16)(-3200.0f + 3200.0f * m0504);
                }
            } else if (joint_kind_table[i] == 0) {
                static csXyz angle_table[] = {
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 5000),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, 0),
                    csXyz(0, 0, -5000),
                    csXyz(0, 0, -7000),
                    csXyz(0, 0, -2700),
                };
                f32 sine = m0504 * cM_ssin(m0508[i]);
                angle.x = 700.0f * sine;
                angle.y = 1700.0f * sine;
                angle.z = 1700.0f * sine;
                angle.x += angle_table[i].x;
                angle.y += angle_table[i].y;
                angle.z += angle_table[i].z;
                rate = 1;
            }
        } else if (mState == 3 && m19C4 == 0) {
            if (i == 0 || i == 1 || i == 6) {
                angle.z = m19CC * cM_ssin(m19D0);
            }
        }
        cLib_addCalcAngleS2(&m03AC[i].x, angle.x, rate, 0x4000);
        cLib_addCalcAngleS2(&m03AC[i].y, angle.y, rate, 0x4000);
        cLib_addCalcAngleS2(&m03AC[i].z, angle.z, rate, 0x4000);
        if (joint_kind_table[i] == 0) {
            m0508[i] += (s16)(1.5f * m0524[i]);
        } else {
            m0508[i] += m0524[i];
        }
    }
}

/* 000025A8-00002880       .text quaternion_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::quaternion_main() {
    for (int i = 0; i < 14; i++) {
        Quaternion target;
        target = ZeroQuat;
        if (mState == 4 && joint_kind_table[i] == 0) {
            cXyz up(0.0f, 1.0f, 0.0f), wind;
            cMtx_YrotS(*calc_mtx, -current.angle.y);
            MtxPosition(dKyw_get_wind_vec(), &wind);
            f32 power = dKyw_get_wind_pow();
            cXyz axis = up.outprod(wind);
            s16 angle = 1400.0f * power * m19D4;
            f32 sine = cM_ssin(angle);
            Quaternion windQuat;
            windQuat.x = sine * axis.x;
            windQuat.y = sine * axis.y;
            windQuat.z = sine * axis.z;
            windQuat.w = cM_scos(angle);
            s16 amplitude = 360.0f * power * m19D4;
            amplitude = amplitude > 220 ? 220 : amplitude;
            cLib_addCalcAngleS2(&m02B0[i], amplitude, 4, 32);
            m0294[i] += (s16)(2048.0f * power * m19D4 + cM_rndFX(256.0f));
            f32 wave = cM_ssin(m02B0[i]);
            Quaternion waveQuat;
            waveQuat.x = wave * cM_ssin(m0294[i]);
            waveQuat.y = 0.0f;
            waveQuat.z = wave * cM_ssin(m0294[i]);
            waveQuat.w = cM_scos(m02B0[i]);
            mDoMtx_quatMultiply(&windQuat, &waveQuat, &target);
        }
        if (mNormalCounter == 1) {
            mJointQuat[i] = target;
        } else {
            C_QUATSlerp(&mJointQuat[i], &target, &mJointQuat[i], 0.4f);
        }
    }
}

/* 00002880-00002938       .text leaf_scale_main__Q210daObjVyasi5Act_cFv */
void daObjVyasi::Act_c::leaf_scale_main() {
    cXyz target(1.0f, 1.0f, 1.0f);
    if (mState == 2) {
        target.x = 1.0f + (1.35f - 1.0f) * m0504;
        target.y = 1.0f + -0.5f * m0504;
        target.z = 1.0f + -0.5f * m0504;
    }
    cLib_addCalc2(&m04A8, target.x, 0.5f, 0.5f);
    cLib_addCalc2(&m04AC, target.y, 0.5f, 0.5f);
    cLib_addCalc2(&m04B0, target.z, 0.5f, 0.5f);
}

/* 00002938-000029BC       .text _execute__Q210daObjVyasi5Act_cFv */
bool daObjVyasi::Act_c::_execute() {
    if (mState != 0) {
        m19C4 = PlayStopJointAnimation();
        process_main();
        set_collision();
        quaternion_main();
        calc_dif_angle();
        leaf_scale_main();
        set_mtx();
        fopAcM_rollPlayerCrash(this, 79.0f, 7);
    }
    return true;
}

/* 000029BC-00002A6C       .text _draw__Q210daObjVyasi5Act_cFv */
bool daObjVyasi::Act_c::_draw() {
    if (mState != 0) {
        g_env_light.settingTevStruct(1, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(mpMorf->getModel(), &tevStr);
        dComIfGd_setListBG();
        mpMorf->updateDL();
        dComIfGd_setList();
    }
    return true;
}
namespace daObjVyasi {
namespace {
/* 00002A6C-00002A8C       .text Mthd_Create__Q210daObjVyasi27@unnamed@d_a_obj_vyasi_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjVyasi::Act_c*)i_this)->_create();
}

/* 00002A8C-00002AB0       .text Mthd_Delete__Q210daObjVyasi27@unnamed@d_a_obj_vyasi_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjVyasi::Act_c*)i_this)->_delete();
}

/* 00002AB0-00002AD4       .text Mthd_Execute__Q210daObjVyasi27@unnamed@d_a_obj_vyasi_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjVyasi::Act_c*)i_this)->_execute();
}

/* 00002AD4-00002AF8       .text Mthd_Draw__Q210daObjVyasi27@unnamed@d_a_obj_vyasi_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjVyasi::Act_c*)i_this)->_draw();
}

/* 00002AF8-00002B00       .text Mthd_IsDelete__Q210daObjVyasi27@unnamed@d_a_obj_vyasi_cpp@FPv */
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
}; // namespace daObjVyasi

actor_process_profile_definition g_profile_Obj_Vyasi = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Vyasi_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjVyasi::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Vyasi_e,
    /* Actor SubMtd */ &daObjVyasi::Mthd_Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e | fopAcStts_UNK200000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
