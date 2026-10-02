/**
 * d_a_gy.cpp
 * Enemy - Gyorg
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_gy.h"
#include "d/actor/d_a_gy_ctrl.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_sea.h"
#include "d/actor/d_a_ship.h"
#include "d/d_a_obj.h"
#include "d/d_bg_s_func.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_jnt_hit.h"
#include "d/d_lib.h"
#include "d/d_material.h"
#include "d/d_particle_name.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/gy.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"

const u32 daGy_c::m_heapsize = 0x3FA0;
const char daGy_c::m_arc_name[] = "Gy";

static daGy_HIO_c l_HIO;

#if VERSION == VERSION_DEMO
#define mHeadSph mSph[5]
#endif

#if VERSION > VERSION_DEMO
static dCcD_SrcSph l_sph_head_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_UNK800,
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_GrpAll_e,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ dCcG_SE_UNK23,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGSphS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 180.0f,
    }},
};
#endif

static dCcD_SrcSph l_sph_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ DEMO_SELECT(AT_TYPE_UNK800, 0),
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_GrpAll_e,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ dCcG_SE_UNK23,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGSphS
    {{
        /* Center */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 180.0f,
    }},
};


static dCcD_SrcCps l_cps_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ DEMO_SELECT(AT_TYPE_UNK800, 0),
        /* SrcObjAt  Atp     */ 0,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_GrpAll_e,
        /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsOther_e | cCcD_CoSPrm_VsGrpAll_e,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ dCcG_SE_UNK23,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
        /* SrcGObjCo SPrm    */ 0,
    },
    // cM3dGCpsS
    {{
        /* Start  */ {0.0f, 0.0f, 0.0f},
        /* End    */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 60.0f,
    }},
};


/* 000000EC-000003CC       .text __ct__10daGy_HIO_cFv */
daGy_HIO_c::daGy_HIO_c() {
    m0C = 0xC8;
    m04 = 1.0f;
    m08 = 0.9f;
    m10 = 200.0f;
    m14 = 300.0f;
    m18 = 2.0f;
    m1C = 2.0f;
    m24 = 15.0f;
    m20 = 0.0f;
    m28.x = -80.0f;
    m28.y = -50.0f;
    m28.z = -150.0f;
    m34.x = -40.0f;
    m34.y = -100.0f;
    m34.z = -350.0f;
    m48 = 10.0f;
    m4C = 20.0f;
    m50 = 30.0f;
    m54 = 40.0f;
    m58 = 15.0f;
    m5C = 5.0f;
    m60 = 40.0f;
    m64 = 20.0f;
    m68 = 55.0f;
    m6C = 10.0f;
    m70 = 20.0f;
    m198 = 1;
    m40 = 60.0f;
    m44 = 2500.0f;
    m74[0] = 70.0f;
    m74[1] = 70.0f;
    m74[2] = 80.0f;
    m74[3] = 70.0f;
    m74[4] = 70.0f;
    m88 = 110.0f;
    m8C = 100.0f;
    m90 = 1.0f;
    m9C = 5000.0f;
    mA4 = 180.0f;
    mA8 = -400.0f;
    m94 = 0;
    m95 = 0;
    m96 = 0;
    m97 = 0;
    m98 = 0;
    m99 = 0;
    mA0 = -500.0f;
    mAC = -80.0f;
    mB4 = -25.0f;
    mB0 = 20.0f;
    mB8.x = 0.0f;
    mB8.y = 0.0f;
    mB8.z = 0.0f;
    mC4 = 8.0f;
    m178 = 0.5f;
    mC8 = 30.0f;
    mD4 = 80.0f;
    mD8 = 0.0f;
    mDC = 0.0f;
    mE0 = 0.0f;
    mE4 = 0.0f;
    mE8 = 0.0f;
    mEC = 0.0f;
    mF0 = 0.0f;
    mF4 = 120.0f;
    mF8 = 0.0f;
    mFC = 0.0f;
    m100 = 300.0f;
    mCC = 2.0f;
    mD0 = 20.0f;
    m104 = -40.0f;
    m108 = 25.0f;
    m10C = 0x78;
    m10E = 0x3C;
    m110 = 0x78;
    m112 = 0xB4;
    m114 = 0x168;
    m116 = 0x5A;
    m118 = 0xF0;
    m188 = 0.2f;
    m12C = 10.0f;
    m144 = 400.0f;
    m148 = 350.0f;
    m150 = 60.0f;
    m154 = 60.0f;
    m158 = 40.0f;
    for (int i = 2; i < 11; i++) {
        m11A[i] = 0;
    }
    m130 = 0x64;
    m132 = 0x12C;
    m134 = 45.0f;
    m138 = 45.0f;
    m13C = 30.0f;
    m15C = 45.0f;
    m160 = 10.0f;
    m140 = 0x1500;
    m142 = 0x800;
    m164 = 0x5A;
    m168 = -1000.0f;
    m16C = -100.0f;
    m170 = 50.0f;
    m174 = 0.1f;
    m17C = 0.1f;
    m184 = 4000.0f;
    m180 = 0x2000;
    m18C = 3000.0f;
    m190 = 200.0f;
    m194 = 3000.0f;
}

/* 00000450-0000049C       .text nodeControl_CB__FP7J3DNodei */
static BOOL nodeControl_CB(J3DNode* i_node, int i_calcTiming) {
    if (i_calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daGy_c* i_this = (daGy_c*)model->getUserArea();
        if (i_this) {
            i_this->_nodeControl(i_node, model);
        }
    }
    return TRUE;
}

/* 0000049C-00000888       .text _nodeControl__6daGy_cFP7J3DNodeP8J3DModel */
void daGy_c::_nodeControl(J3DNode* i_node, J3DModel* i_model) {
    int jntNo = ((J3DJoint*)i_node)->getJntNo();
    fopAc_ac_c* actor = this;

    if (jntNo == GY_JNT_J_GY_ATAMA1_e) {
        mDoMtx_stack_c::copy(i_model->getAnmMtx(jntNo));
        cXyz offset(0.0f, 0.0f, 0.0f);
        offset.x = l_HIO.mD4;
        offset.y = l_HIO.mD8;
        offset.z = l_HIO.mDC;
        mDoMtx_stack_c::multVec(&offset, &mD08);
#if VERSION == VERSION_DEMO
        mDoMtx_stack_c::multVecZero(&m11C4[jntNo]);
#endif
    }

    Mtx mtx;
    mDoMtx_copy(i_model->getAnmMtx(jntNo), mtx);
    cXyz trans(mtx[0][3], mtx[1][3], mtx[2][3]);
    mtx[2][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[0][3] = 0.0f;
    mDoMtx_stack_c::transS(trans.x, trans.y, trans.z);

    if (jntNo == GY_JNT_J_GY_ATAMA1_e) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        f32 dist = (actor->current.pos - player->current.pos).absXZ();
        int angDiff = cLib_distanceAngleS(actor->shape_angle.y, fopAcM_searchActorAngleY(actor, player));
        if (angDiff < l_HIO.m180 && dist < l_HIO.m184 && mPrmIdx != 8 && mPrmIdx != 9 && mPrmIdx != 6 && mPrmIdx != 0xB && mPrmIdx != 3) {
            mCEC = player->current.pos;
            if (m2B0 == 2) {
                f32 wave = daSea_calcWave(mCEC.x, mCEC.z);
                mCEC.y = wave + l_HIO.mA8;
            }
            if (m2B0 == 3 && m928 == 1) {
                mCEC.y += l_HIO.mA4;
            }
#if VERSION == VERSION_DEMO
            cXyz pos = m11C4[jntNo];
            cXyz to = mCEC - pos;
            cXyz from = mD08 - pos;
#else
            cXyz to = mCEC - mD08;
            cXyz from = mD08 - mD08;
#endif
            Quaternion quat;
            daObj::quat_rotVec(&quat, from, to);
            mDoMtx_quatSlerp(&mCF8, &quat, &mCF8, l_HIO.m188);
        } else {
            mDoMtx_quatSlerp(&mCF8, &ZeroQuat, &mCF8, l_HIO.m188);
        }
        mDoMtx_stack_c::quatM(&mCF8);
    }

    mDoMtx_stack_c::concat(mtx);
#if VERSION == VERSION_DEMO
    mDoMtx_stack_c::multVecZero(&m11C4[jntNo]);
#endif

    if (jntNo == GY_JNT_J_GY_ATAMA1_e) {
        cXyz offset(0.0f, 0.0f, 0.0f);
        offset.x = l_HIO.mD4;
        offset.y = l_HIO.mD8;
        offset.z = l_HIO.mDC;
        mDoMtx_stack_c::multVec(&offset, &mD08);
    }
    if (jntNo == GY_JNT_J_GY_SEBIRE1_e) {
        cXyz offset(100.0f, -100.0f, 0.0f);
        mDoMtx_stack_c::multVec(&offset, &m89C);
    }

    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    i_model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
}

/* 00000888-000008A8       .text createHeap_CB__FP10fopAc_ac_c */
static BOOL createHeap_CB(fopAc_ac_c* i_this) {
    return static_cast<daGy_c*>(i_this)->_createHeap();
}

/* 000008A8-00000AAC       .text _createHeap__6daGy_cFv */
BOOL daGy_c::_createHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(m_arc_name, dRes_INDEX_GY_BMD_GY_e);
    JUT_ASSERT(DEMO_SELECT(0x347, 0x377), modelData != 0);

    mpMorf = new mDoExt_McaMorf(
        modelData,
        NULL, NULL,
#if VERSION == VERSION_DEMO
        NULL,
        -1,
#else
        (J3DAnmTransformKey*)dComIfG_getObjectRes(m_arc_name, dRes_INDEX_GY_BCK_SWIM1_e),
        J3DFrameCtrl::EMode_LOOP,
#endif
        1.0f,
        0,
        -1,
        1,
        NULL,
        0,
        0x11020203
    );
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
        return FALSE;
    }

    mpMorf->getModel()->setUserArea((u32)this);

    if (!m2D8.create(mpMorf->getModel())) {
        return FALSE;
    }

    for (u16 i = 0; i < modelData->getJointNum(); i++) {
        switch (i) {
        case GY_JNT_J_GY_ATAMA1_e:
        case GY_JNT_J_GY_SEBIRE1_e:
        case GY_JNT_J_GY_SEBONE2_e:
        case GY_JNT_J_GY_SEBONE3_e:
        case GY_JNT_J_GY_SEBONE4_e:
        case GY_JNT_J_GY_SEBONE5_e:
            mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeControl_CB);
            break;
        }
    }

    static Vec atama_cyl_offset[] = {
        {0.0f, 0.0f, 0.0f},
        {150.0f, 0.0f, 0.0f},
    };
    static Vec ago_cyl_offset[] = {
        {0.0f, 0.0f, 0.0f},
        {90.0f, 0.0f, 0.0f},
    };
    static Vec body1_cyl_offset[] = {
        {-40.0f, -20.0f, 0.0f},
        {40.0f, -20.0f, 0.0f},
    };
    static Vec body2_cyl_offset[] = {
        {-40.0f, -10.0f, 0.0f},
        {40.0f, -10.0f, 0.0f},
    };
    static Vec sebire1_cyl_offset[] = {
        {-40.0f, 60.0f, 0.0f},
        {150.0f, -100.0f, 0.0f},
    };
    static __jnt_hit_data_c search_data[] = {
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_ATAMA1_e,
            /* mRadius     */ 30.0f,
            /* mpOffsets   */ atama_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_AGO1_e,
            /* mRadius     */ 30.0f,
            /* mpOffsets   */ ago_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_SEBONE1_e,
            /* mRadius     */ 60.0f,
            /* mpOffsets   */ body1_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_SEBIRE1_e,
            /* mRadius     */ 20.0f,
            /* mpOffsets   */ sebire1_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_SEBONE2_e,
            /* mRadius     */ 60.0f,
            /* mpOffsets   */ body1_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_SEBONE3_e,
            /* mRadius     */ 50.0f,
            /* mpOffsets   */ body1_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_SEBONE4_e,
            /* mRadius     */ 30.0f,
            /* mpOffsets   */ body1_cyl_offset,
        },
        {
            /* mShapeType  */ JntHitType_CYL_e,
            /* mJointIndex */ GY_JNT_J_GY_SEBONE5_e,
            /* mRadius     */ 30.0f,
            /* mpOffsets   */ body2_cyl_offset,
        },
    };

    mpJntHit = JntHit_create(mpMorf->getModel(), search_data, ARRAY_SSIZE(search_data));
    if (mpJntHit) {
        fopAcM_SetJntHit(this, mpJntHit);
    } else {
        return FALSE;
    }

    return TRUE;
}

/* 00000AAC-00000C30       .text setMtx__6daGy_cFv */
void daGy_c::setMtx() {
    cXyz sp14;
    cXyz sp08;
    J3DModel* model = mpMorf->getModel();
    model->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle.x, shape_angle.y, shape_angle.z);

    sp14.x = 0.0f;
    sp14.y = 0.0f;
    sp14.z = 500.0f;
    mDoMtx_stack_c::multVec(&sp14, &mE84);

    if (l_HIO.m94 != 0) {
        mDoMtx_stack_c::transM(l_HIO.mB8.x, l_HIO.mB8.y, l_HIO.mB8.z);
    }
    mDoMtx_stack_c::transM(0.0f, m4E4, 0.0f);
    model->setBaseTRMtx(mDoMtx_stack_c::now);

    if (m4E4 <= 10.0f + l_HIO.mAC) {
        sp08.x = l_HIO.mEC;
        sp08.y = l_HIO.mF0;
        sp08.z = l_HIO.mF4;
    } else if (m4E4 > 10.0f + l_HIO.mAC) {
        sp08.x = l_HIO.mF8;
        sp08.y = l_HIO.mFC;
        sp08.z = l_HIO.m100;
    }
    mDoMtx_stack_c::transM(sp08.x, sp08.y, sp08.z);
    mDFC.x = mDoMtx_stack_c::now[0][3];
    mDFC.y = mDoMtx_stack_c::now[1][3];
    mDFC.z = mDoMtx_stack_c::now[2][3];
}

/* 00000C30-00000C7C       .text setAnm__6daGy_cFv */
void daGy_c::setAnm() {
    static const dLib_anm_idx_c a_anm_idx_tbl[] = {
        {dRes_INDEX_GY_BCK_SWIM1_e, -1},
        {dRes_INDEX_GY_BCK_SWIM2_e, -1},
        {dRes_INDEX_GY_BCK_BUTUKARU1_e, -1},
        {dRes_INDEX_GY_BCK_JUMP1_e, -1},
        {dRes_INDEX_GY_BCK_DAMAGE1_e, -1},
        {dRes_INDEX_GY_BCK_DEATH1_e, -1},
        {dRes_INDEX_GY_BCK_DEATH2_e, -1},
        {dRes_INDEX_GY_BCK_ATTACK1_e, -1},
        {dRes_INDEX_GY_BCK_ATTACK2_e, -1},
    };
    static const dLib_anm_prm_c a_anm_prm_tbl[] = {
        {0, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {0, -1, 0, 8.0f, 2.0f, J3DFrameCtrl::EMode_LOOP},
        {1, -1, 0, 8.0f, 2.0f, J3DFrameCtrl::EMode_LOOP},
        {0, -1, 0, 8.0f, 2.5f, J3DFrameCtrl::EMode_LOOP},
        {2, 1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {3, -1, 0, 8.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {4, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {4, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_LOOP},
        {5, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {6, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {7, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
        {8, -1, 0, 2.0f, 1.0f, J3DFrameCtrl::EMode_NONE},
    };
    dLib_setAnm(m_arc_name, mpMorf, &mAnmIdx, &mPrmIdx, &mOldPrmIdx, a_anm_idx_tbl, a_anm_prm_tbl, false);
}

#if VERSION == VERSION_DEMO
/* 00000C7C-00000F3C       .text setCollision__6daGy_cFv */
void daGy_c::setCollision() {
    for (int i = 0; i < 5; i++) {
        int jnt;
        switch (i) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            jnt = 10;
            break;
        }
        mSph[i].SetR(l_HIO.m74[i] * l_HIO.m90);
        mSph[i].SetC(m11C4[jnt]);
        dComIfG_Ccsp()->Set(&mSph[i]);
    }
    mHeadSph.SetR(l_HIO.m88 * l_HIO.m90);
    mHeadSph.SetC(mD08);
    mCps.SetStartEnd(m89C, m11C4[5]);
    mCps.SetR(l_HIO.m8C * l_HIO.m90);
    dComIfG_Ccsp()->Set(&mCps);

    if (m2B0 == 3) {
        if (mPrmIdx == 0xB) {
            mHeadSph.OnAtSetBit();
            mHeadSph.OnAtHitBit();
            mHeadSph.SetAtAtp(2);
            mHeadSph.SetAtSpl(dCcG_At_Spl_UNK1);
            mHeadSph.SetAtSe(6);
            mHeadSph.OffCoSetBit();
        } else {
            mHeadSph.OffAtSetBit();
            mHeadSph.OffAtHitBit();
            mHeadSph.SetAtAtp(0);
            mHeadSph.OnCoSetBit();
        }
    } else if (m2B0 == 2 || m2B0 == 5) {
        if (mpCtrl->m320 == 1) {
            if (mHeadSph.ChkCoHit()) {
                fopAc_ac_c* ac = mHeadSph.GetCoHitAc();
                if (fopAcM_GetName(ac) == fpcNm_SHIP_e) {
                    mHeadSph.OnAtSetBit();
                    mHeadSph.OnAtHitBit();
                    mHeadSph.SetAtAtp(0);
                    mHeadSph.SetAtSpl(dCcG_At_Spl_UNK1);
                    mHeadSph.SetAtSe(6);
                }
            } else {
                mHeadSph.OffAtSetBit();
                mHeadSph.OffAtHitBit();
                mHeadSph.SetAtAtp(0);
            }
            mHeadSph.OnCoSetBit();
        }
    } else {
        mHeadSph.OffAtSetBit();
        mHeadSph.OffAtHitBit();
        mHeadSph.SetAtAtp(0);
        mHeadSph.OnCoSetBit();
        if (mHeadSph.ChkCoHit()) {
            if (m2B0 != 9 && m2B0 != 8) {
                modeDiveInit();
            }
        }
    }

    dComIfG_Ccsp()->Set(&mHeadSph);
}
#else
/* 00000C7C-00000E74       .text setAtCollision__6daGy_cFv */
void daGy_c::setAtCollision() {
    mHeadSph.SetR(l_HIO.m88 * l_HIO.m90);
    mHeadSph.SetC(mD08);

    if (m2B0 == 3) {
        int frame = mpMorf->getFrame();
        if (mPrmIdx == 0xB && frame < 0x12) {
            mHeadSph.OnAtSetBit();
            mHeadSph.OnAtHitBit();
            mHeadSph.SetAtAtp(2);
            mHeadSph.SetAtSpl(dCcG_At_Spl_UNK1);
            mHeadSph.SetAtSe(6);
            mHeadSph.OffCoSetBit();
        } else {
            mHeadSph.OffAtSetBit();
            mHeadSph.OffAtHitBit();
            mHeadSph.SetAtAtp(0);
            mHeadSph.OnCoSetBit();
        }
    } else if (m2B0 == 2 || m2B0 == 5) {
        if (mpCtrl->m320 == 1) {
            if (mHeadSph.ChkCoHit()) {
                fopAc_ac_c* ac = mHeadSph.GetCoHitAc();
                if (fopAcM_GetName(ac) == fpcNm_SHIP_e) {
                    mHeadSph.OnAtSetBit();
                    mHeadSph.OnAtHitBit();
                    mHeadSph.SetAtAtp(2);
                    mHeadSph.SetAtSpl(dCcG_At_Spl_UNK1);
                    mHeadSph.SetAtSe(6);
                }
            } else {
                mHeadSph.OffAtSetBit();
                mHeadSph.OffAtHitBit();
                mHeadSph.SetAtAtp(0);
            }
            mHeadSph.OnCoSetBit();
        }
    } else {
        mHeadSph.OffAtSetBit();
        mHeadSph.OffAtHitBit();
        mHeadSph.SetAtAtp(0);
        mHeadSph.OnCoSetBit();
        if (mHeadSph.ChkCoHit()) {
            if (m2B0 != 9 && m2B0 != 8) {
                modeDiveInit();
            }
        }
    }

    dComIfG_Ccsp()->Set(&mHeadSph);
}

/* 00000E74-00000F3C       .text setCollision__6daGy_cFv */
void daGy_c::setCollision() {
    mSph.SetR(140.0f * l_HIO.m90);
    mSph.SetC(current.pos);
    dComIfG_Ccsp()->Set(&mSph);
    mCps.SetStartEnd(m89C, m89C);
    mCps.SetR(l_HIO.m8C * l_HIO.m90);
    dComIfG_Ccsp()->Set(&mCps);
}
#endif

/* 00000F3C-00000FC4       .text setAimSpeedF__6daGy_cFv */
void daGy_c::setAimSpeedF() {
    if (mpCtrl->m320 == 0) {
        mAimSpeedF = l_HIO.m48;
        f32 frame = mpMorf->getFrame();
        if (frame > l_HIO.mCC) {
            if (frame < l_HIO.mD0) {
                mAimSpeedF = l_HIO.m4C;
            }
        }
    } else {
        mAimSpeedF = l_HIO.m50;
        f32 frame = mpMorf->getFrame();
        if (frame > l_HIO.mCC) {
            if (frame < l_HIO.mD0) {
                mAimSpeedF = l_HIO.m54;
            }
        }
    }
}

/* 00000FC4-00000FDC       .text modeDiveInit__6daGy_cFv */
void daGy_c::modeDiveInit() {
    m2B0 = 0;
    m508 = 0.0f;
}

/* 00000FDC-000012DC       .text modeDive__6daGy_cFv */
void daGy_c::modeDive() {
    if (mPrmIdx == 5) {
        return;
    }

    mPrmIdx = 1;
    daGy_Ctrl_c* ctrl = mpCtrl;
    if (ctrl->m324 == 3 || mE80 != 0) {
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &m2BC), 8, 0x400);
        mAimSpeedF = 0.0f;
        m4E8 = 2.0f * l_HIO.mA0;
        m4EC = l_HIO.mC8;
        fopAcM_OffStatus(this, fopAcStts_SHOWMAP_e);
        attention_info.flags &= ~fopAc_Attn_LOCKON_BATTLE_e;
        return;
    }

    fopAcM_OnStatus(this, fopAcStts_SHOWMAP_e);
    attention_info.flags |= fopAc_Attn_LOCKON_BATTLE_e;
    m4E8 = l_HIO.mA0;
    m4EC = l_HIO.mC8;

    f32 playerDist = fopAcM_searchActorDistance(this, dComIfGp_getPlayer(0));
    if (ctrl->m312[m2AC] == 1 && m4E4 < 10.0f + l_HIO.mA0 && playerDist > l_HIO.m148) {
        modeCircleInit();
        return;
    }

    daShip_c* ship = dComIfGp_getShipActor();
    f32 homeDist = (current.pos - m2BC).absXZ();
    if (ship == NULL) {
        return;
    }

    if (homeDist > l_HIO.m144 || ship->speedF > l_HIO.m6C) {
        setAimSpeedF();
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &m2BC), 8, 0x400);
        m508 = 0.0f;
    } else {
        mAimSpeedF = 0.0f;
        cXyz delta = m2BC - current.pos;
        cLib_addCalc2(&m508, 1.0f, 0.01f, 0.05f);
        current.pos = current.pos + delta * m508;
        cLib_addCalcAngleS2(&current.angle.y, ctrl->m308[m2AC] + 0x8000, 4, 0x400);
    }
}

/* 000012DC-0000141C       .text modeCircleInit__6daGy_cFv */
void daGy_c::modeCircleInit() {
    m2B0 = 1;
    if (mpCtrl->m320 == 0) {
        m914 = (s32)(cM_rndF((f32)(l_HIO.m118 - l_HIO.m116)) + (f32)l_HIO.m116);
    } else if (mpCtrl->m320 == 1) {
        m914 = (s32)(cM_rndF((f32)(l_HIO.m114 - l_HIO.m112)) + (f32)l_HIO.m112);
    }
    m508 = 0.0f;
}

/* 0000141C-000016E4       .text modeCircle__6daGy_cFv */
void daGy_c::modeCircle() {
    if (mPrmIdx == 5) {
        return;
    }

    mPrmIdx = 1;
    m4E8 = l_HIO.mAC;
    m4EC = l_HIO.mC4;
    daGy_Ctrl_c* ctrl = mpCtrl;
    if (ctrl->m312[m2AC] == 0) {
        modeDiveInit();
        return;
    }

    daShip_c* ship = dComIfGp_getShipActor();
    f32 homeDist = (current.pos - m2BC).absXZ();
    if (ship == NULL) {
        return;
    }

    if (homeDist > l_HIO.m144 || ship->speedF > l_HIO.m6C) {
        setAimSpeedF();
        f32 playerDist = fopAcM_searchActorDistance(this, dComIfGp_getPlayer(0));
        if (playerDist < l_HIO.m148) {
            modeDiveInit();
            return;
        }
        if (ship->speedF > l_HIO.m70) {
            modeWithCircleInit();
            return;
        }
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &m2BC), 8, 0x400);
        m508 = 0.0f;
    } else {
        mAimSpeedF = 0.0f;
        cXyz delta = m2BC - current.pos;
        cLib_addCalc2(&m508, 1.0f, 0.01f, 0.05f);
        current.pos = current.pos + delta * m508;
        cLib_addCalcAngleS2(&current.angle.y, ctrl->m308[m2AC] + 0x8000, 4, 0x400);
        if (cLib_calcTimer(&m914) == 0) {
            if (!dComIfGp_checkPlayerStatus0(0, daPyStts0_UNK1000000_e)) {
                if (ctrl->m320 == 0) {
                    modeAttackPlayerInit();
                } else if (ctrl->m320 == 1) {
                    modeAttackInit();
                }
            }
        }
    }
}

/* 000016E4-00001788       .text modeWithCircleInit__6daGy_cFv */
void daGy_c::modeWithCircleInit() {
    m2B0 = 6;
    m4F8 = (s32)cM_rndF((f32)l_HIO.m132);
    if (m4F8 <= l_HIO.m130) {
        m4F8 = l_HIO.m130;
    }
    m914 = l_HIO.m110 + m4F8;
}

/* 00001788-000019AC       .text modeWithCircle__6daGy_cFv */
void daGy_c::modeWithCircle() {
    if (mPrmIdx == 5) {
        return;
    }

    m4E8 = l_HIO.mAC;
    m4EC = l_HIO.mC4;
    if (mpCtrl->m312[m2AC] == 0) {
        modeDiveInit();
        return;
    }

    daShip_c* ship = dComIfGp_getShipActor();
    if (ship == NULL) {
        return;
    }

    if (ship->speedF < l_HIO.m50) {
        modeCircleInit();
    }

    cLib_addCalcAngleS2(&current.angle.y, ship->shape_angle.y, 8, 0x400);
    int angDiff = cLib_distanceAngleS(ship->shape_angle.y, fopAcM_searchActorAngleY(ship, this));
    f32 playerDist = fopAcM_searchActorDistance(this, dComIfGp_getPlayer(0));
    if (angDiff < l_HIO.m142 && playerDist > l_HIO.m9C) {
        mAimSpeedF = ship->speedF - 5.0f;
    } else {
        mAimSpeedF = ship->speedF + l_HIO.m40;
    }

    if (angDiff < l_HIO.m140 && angDiff > l_HIO.m142 && playerDist > l_HIO.m9C) {
        speed.y = l_HIO.m138;
        fopAcM_seStart(this, JA_SE_CM_GY_JUMP_L, 0);
        mPrmIdx = 5;
        f32 s = 2.0f + REG10_F(0);
        cXyz scale(s, s + REG10_F(1), s);
        dComIfGp_particle_set(dPa_name::ID_IT_JN_MIZUSHIBUKI_A, &current.pos, NULL, &scale);
        modeWithAttackInit();
    }
}

/* 000019AC-00001A14       .text modeAttackInit__6daGy_cFv */
void daGy_c::modeAttackInit() {
    m2B0 = 2;
    mPrmIdx = 3;
    m8F8 = dComIfGp_getPlayer(0)->current.pos;
    m914 = l_HIO.m10C;
    m904 = m2BC;
    m928 = 0;
}

/* 00001A14-00001A74       .text modeWithAttackInit__6daGy_cFv */
void daGy_c::modeWithAttackInit() {
    m2B0 = 5;
    m8F8 = dComIfGp_getPlayer(0)->current.pos;
    m914 = l_HIO.m10C;
    m904 = m2BC;
    m928 = 0;
}

/* 00001A74-00001D60       .text modeWithAttack__6daGy_cFv */
void daGy_c::modeWithAttack() {
    if (mPrmIdx != 5) {
        mPrmIdx = 3;
    }
    if (mpCtrl->m320 == 0) {
        modeCircleInit();
        return;
    }

    s16 targetY;
    {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        targetY = cLib_targetAngleY(&current.pos, &player->current.pos);
    }
    f32 playerDist = fopAcM_searchActorDistance(this, dComIfGp_getPlayer(0));

    switch (m928) {
    case 0:
        m4E8 = l_HIO.mAC;
        m4EC = l_HIO.mC4;
        cLib_addCalcAngleS2(&current.angle.y, targetY, 8, 0x400);
        mAimSpeedF = l_HIO.m68;
        if (playerDist < l_HIO.m44) {
            m928 += 1;
            m914 = l_HIO.m10E;
        }
        if (cLib_calcTimer(&m914) == 0) {
            if (l_HIO.m99 == 0) {
                modeWithCircleInit();
            } else {
                m928 += 1;
            }
        }
        break;
    case 1:
        m4E8 = l_HIO.mB0;
        m4EC = l_HIO.mC4;
        mAimSpeedF = l_HIO.m68;
        if (mHeadSph.ChkCoHit()) {
            fopAc_ac_c* ac = mHeadSph.GetCoHitAc();
            if (fopAcM_GetName(ac) == fpcNm_SHIP_e) {
                dComIfGp_getVibration().StartShock(7, -0x21, cXyz(0.0f, 1.0f, 0.0f));
                fopAcM_seStart(this, JA_SE_CM_GY_SHIP_CRASH, 0);
                fopAcM_monsSeStart(this, JA_SE_CV_GY_ATTACK, 0);
                modeAttackBackInit();
                break;
            }
        }
        if (cLib_calcTimer(&m914) == 0) {
            if (l_HIO.m99 == 0) {
                modeWithCircleInit();
            } else {
                m928 += 1;
            }
        }
        break;
    case 2:
        m4E8 = l_HIO.mA0;
        m4EC = l_HIO.mC4;
        if (std::fabsf(m4E4 - m4E8) < 10.0f) {
            fopAcM_delete(this);
        }
        break;
    }
}

/* 00001D60-00001F20       .text modeAttack__6daGy_cFv */
void daGy_c::modeAttack() {
    if (mpCtrl->m320 == 0) {
        modeCircleInit();
        return;
    }

    m4E8 = l_HIO.mB0;
    m4EC = l_HIO.mC4;
    if (cLib_calcTimer(&m914) == 0) {
        modeCircleInit();
    }

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 playerDist = fopAcM_searchActorDistance(this, player);
    if (playerDist < l_HIO.m148) {
        mAimSpeedF = 0.0f;
    } else {
        mAimSpeedF = l_HIO.m64;
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &player->current.pos), 8, 0x400);
    }

    if (mHeadSph.ChkCoHit()) {
        fopAc_ac_c* ac = mHeadSph.GetCoHitAc();
        if (fopAcM_GetName(ac) == fpcNm_SHIP_e) {
            dComIfGp_getVibration().StartShock(7, -0x21, cXyz(0.0f, 1.0f, 0.0f));
            fopAcM_seStart(this, JA_SE_CM_GY_SHIP_CRASH, 0);
            fopAcM_monsSeStart(this, JA_SE_CV_GY_ATTACK, 0);
            modeAttackBackInit();
        }
    }
}

/* 00001F20-00001F40       .text modeAttackPlayerInit__6daGy_cFv */
void daGy_c::modeAttackPlayerInit() {
    m2B0 = 3;
    mPrmIdx = 2;
    m928 = 0;
    m924 = 0;
}

/* 00001F40-000022F8       .text modeAttackPlayer__6daGy_cFv */
void daGy_c::modeAttackPlayer() {
    if (mpCtrl->m320 == 1) {
        modeCircleInit();
        return;
    }

    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    switch (m928) {
    case 0:
        m4E8 = l_HIO.mB4;
        m4EC = l_HIO.mC4;
        mAimSpeedF = l_HIO.m58;
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &player->current.pos), 8, 0x400);
        f32 dist = (mD08 - player->current.pos).absXZ();
        if (dist < l_HIO.m148) {
            mPrmIdx = 0xA;
            m928 += 1;
        }
        break;
    case 1:
        m4E8 = l_HIO.mB4;
        m4EC = l_HIO.mC4;
        mAimSpeedF = l_HIO.m5C;
        if (mPrmIdx == 0xA) {
            if (mpMorf->isStop()) {
                mPrmIdx = 2;
                setAnm();
                mPrmIdx = 0xA;
                m924 += 1;
            }
        }
        if (m924 > l_HIO.m198) {
            mPrmIdx = 0xB;
            m928 += 1;
        }
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &player->current.pos), 8, 0x400);
        break;
    case 2:
        if (mHeadSph.ChkCoHit()) {
            fopAc_ac_c* ac = mHeadSph.GetCoHitAc();
            if (fopAcM_GetName(ac) == fpcNm_PLAYER_e && m504 == 0) {
                fopAcM_monsSeStart(this, JA_SE_CV_GY_ATTACK, 0);
                dComIfGp_getVibration().StartShock(4, -0x21, cXyz(0.0f, 1.0f, 0.0f));
                m504 = 1;
            }
        }
        if (mpMorf->getFrame() > 10.0f + REG12_F(10)) {
            m4E8 = l_HIO.mA0;
            m4EC = l_HIO.mC4;
        } else {
            m4E8 = l_HIO.mB4;
            m4EC = l_HIO.mC4;
        }
        mAimSpeedF = l_HIO.m60;
        if (mPrmIdx == 0xB) {
            if (mpMorf->isStop()) {
                mPrmIdx = 1;
                m504 = 0;
                modeDiveInit();
            }
        }
        break;
    }
}

/* 000022F8-00002400       .text modeAttackBackInit__6daGy_cFv */
void daGy_c::modeAttackBackInit() {
    m2B0 = 4;
    mPrmIdx = 4;
    speedF = l_HIO.m104;
    speed.y = l_HIO.m108;
    fopAcM_seStart(this, JA_SE_CM_GY_JUMP_S, 0);
    f32 s = 2.0f + REG10_F(0);
    cXyz scale(s, s + REG10_F(1), s);
    dComIfGp_particle_set(dPa_name::ID_IT_JN_MIZUSHIBUKI_A, &current.pos, NULL, &scale);
}

/* 00002400-00002560       .text modeAttackBack__6daGy_cFv */
void daGy_c::modeAttackBack() {
    m4E8 = l_HIO.mA0;
    mAimSpeedF = 0.0f;
    cLib_addCalcAngleS2(&m8F4, 0, 4, 0x800);
    f32 dist = (current.pos - m904).absXZ();
    if (dist > l_HIO.m144) {
        setAimSpeedF();
        if (mPrmIdx == 1) {
            modeWithCircleInit();
        }
        cLib_addCalcAngleS2(&current.angle.y, cLib_targetAngleY(&current.pos, &m904), 8, 0x400);
    } else {
        if (mPrmIdx == 1) {
            modeCircleInit();
        }
    }
}

/* 00002560-000025F0       .text modeDamageInit__6daGy_cFv */
void daGy_c::modeDamageInit() {
    m2B0 = 7;
    mPrmIdx = 6;
    speed.y = 30.0f;
    fopAcM_seStart(this, JA_SE_CM_GY_JUMP_S, 0);
}

/* 000025F0-00002754       .text modeDamage__6daGy_cFv */
void daGy_c::modeDamage() {
    mAimSpeedF = 0.0f;
    if (m920 > 0.01f) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        cXyz src(0.0f, 0.0f, -m920);
        cXyz dst(0.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::YrotS(fopAcM_searchActorAngleY(this, player));
        mDoMtx_stack_c::multVec(&src, &dst);
        current.pos.x += dst.x;
        current.pos.y += dst.y;
        current.pos.z += dst.z;
        cLib_addCalc0(&m920, 1.0f, 2.0f);
    }

    if (mPrmIdx == 6) {
        if (mpMorf->isStop()) {
            mPrmIdx = 1;
            switch (mOldPrmIdx) {
            case 6:
                modeWithCircleInit();
                break;
            case 0:
                modeDiveInit();
                break;
            default:
                modeCircleInit();
                break;
            }
        }
    }
}

/* 00002754-000028B8       .text modeDeleteInit__6daGy_cFv */
void daGy_c::modeDeleteInit() {
    fopAcM_OffStatus(this, fopAcStts_SHOWMAP_e);
    attention_info.flags &= ~fopAc_Attn_LOCKON_BATTLE_e;

    int n = dComIfGs_getEventReg(dSv_event_flag_c::UNK_7EFF) + 1;
    n = cLib_maxLimit<int>(n, 0xFF) & 0xFF;
    dComIfGs_setEventReg(dSv_event_flag_c::UNK_7EFF, n);

    m2B0 = 8;
    mPrmIdx = 8;
    mAimSpeedF = 0.0f;
    speedF = 0.0f;
    speed.y = l_HIO.m15C;
    fopAcM_seStart(this, JA_SE_CM_GY_JUMP_L, 0);
    f32 s = 2.0f + REG10_F(0);
    cXyz scale(s, s + REG10_F(1), s);
    dComIfGp_particle_set(dPa_name::ID_IT_JN_MIZUSHIBUKI_A, &current.pos, NULL, &scale);
}

/* 000028B8-00002998       .text modeDelete__6daGy_cFv */
void daGy_c::modeDelete() {
    if (m500 != -1) {
        if (cLib_calcTimer(&m500) == 0) {
            mEnemyIce.mLightShrinkTimer = 1;
        }
    } else if (m4FC != -1) {
        if (cLib_calcTimer(&m4FC) == 0) {
            mEnemyIce.mFreezeDuration = l_HIO.m0C;
        }
    } else if (mPrmIdx == 8) {
        if (mpMorf->isStop()) {
            fopAcM_createDisappear(this, &current.pos, 10, daDisItem_IBALL_e, 0xFF);
            fopAcM_delete(this);
        }
    }
}

/* 00002998-00002A70       .text modeDeleteBombInit__6daGy_cFv */
void daGy_c::modeDeleteBombInit() {
    fopAcM_OffStatus(this, fopAcStts_SHOWMAP_e);
    attention_info.flags &= ~fopAc_Attn_LOCKON_BATTLE_e;
    m2B0 = 9;
    mPrmIdx = 7;
    mAimSpeedF = 0.0f;
    speedF = 0.0f;
    m4E4 = 0.0f;
    speed.y = l_HIO.m15C;
    fopAcM_seStart(this, JA_SE_CM_GY_JUMP_L, 0);
    m4EC = 10.0f * l_HIO.mC8;
}

/* 00002A70-00002CBC       .text modeDeleteBomb__6daGy_cFv */
void daGy_c::modeDeleteBomb() {
    if (m920 > 0.01f) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        cXyz src(0.0f, 0.0f, -m920);
        cXyz dst(0.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::YrotS(fopAcM_searchActorAngleY(this, player));
        mDoMtx_stack_c::multVec(&src, &dst);
        current.pos.x += dst.x;
        current.pos.y += dst.y;
        current.pos.z += dst.z;
        cLib_addCalc0(&m920, 1.0f, 2.0f);
    }

    if (mPrmIdx == 7) {
        m4E8 = l_HIO.m168;
        if (m4E4 <= 10.0f + l_HIO.m168) {
            mPrmIdx = 9;
            m4EC = l_HIO.mC4;
        }
    }

    if (mPrmIdx == 9) {
        if (m4E4 >= l_HIO.m16C - 10.0f) {
            if (m8EC == -1.0f) {
                m8EC = l_HIO.m164;
            }
            m4EC = l_HIO.m178;
            m918 = l_HIO.m17C;
        }

        if (m4E4 <= 10.0f + (l_HIO.m16C - l_HIO.m170)) {
            m4E8 = l_HIO.m16C + l_HIO.m170;
        } else if (m4E4 >= (l_HIO.m16C + l_HIO.m170) - 10.0f) {
            m4E8 = l_HIO.m16C - l_HIO.m170;
        }

        if (m8EC != -1.0f) {
            if (cLib_calcTimer<int>(&m8EC) == 0) {
                fopAcM_createDisappear(this, &current.pos, 10, daDisItem_IBALL_e, 0xFF);
                fopAcM_delete(this);
            }
        }
    }
}

/* 00002CBC-00002E24       .text modeProcCall__6daGy_cFv */
void daGy_c::modeProcCall() {
    typedef void (daGy_c::*modeProcFunc)();

    if (l_HIO.m95 != 0) {
        m2B0 = l_HIO.m9A;
    }

    static modeProcFunc mode_proc[] = {
        &daGy_c::modeDive,
        &daGy_c::modeCircle,
        &daGy_c::modeAttack,
        &daGy_c::modeAttackPlayer,
        &daGy_c::modeAttackBack,
        &daGy_c::modeWithAttack,
        &daGy_c::modeWithCircle,
        &daGy_c::modeDamage,
        &daGy_c::modeDelete,
        &daGy_c::modeDeleteBomb,
    };
    (this->*mode_proc[m2B0])();
}

#if VERSION > VERSION_DEMO
/* 00002E24-00003004       .text createWave__6daGy_cFv */
void daGy_c::createWave() {
    static JGeometry::TVec3<f32> wave_l_direction(0.5f, 1.0f, -0.3f);
    static JGeometry::TVec3<f32> wave_r_direction(-0.5f, 1.0f, -0.3f);

    if (mD18.getEmitter() == NULL) {
        dComIfGp_particle_set(dPa_name::ID_AK_JN_SHIPWAVE00, &mDFC, &mE08, NULL, 0xFF, &mD18);
        if (mD18.getEmitter() != NULL) {
            mD18.getEmitter()->setDirection(wave_l_direction);
        }
    }

    if (mD7C.getEmitter() == NULL) {
        dComIfGp_particle_set(dPa_name::ID_AK_JN_SHIPWAVE00, &mDFC, &mE08, NULL, 0xFF, &mD7C);
        if (mD7C.getEmitter() != NULL) {
            mD7C.getEmitter()->setDirection(wave_r_direction);
        }
    }

    if (mDE0.getEmitter() == NULL) {
        dComIfGp_particle_set(dPa_name::ID_AK_JN_SHIPSPLASH00, &mDFC, &mE08, NULL, 0xFF, &mDE0);
    }
}
#endif

/* 00003004-00003268       .text setWave__6daGy_cFv */
void daGy_c::setWave() {
    f32 splash_target;
    f32 wave_speed;
    f32 max_speed = l_HIO.m24;
    // The demo debug map shows a std::fabsf call here; its result is overwritten (argument is a guess).
    f32 speed_ratio = std::fabsf(speedF);
    speed_ratio = 1.0f;

    if (mPrmIdx == 5 || mPrmIdx == 8 || mPrmIdx == 9 || mPrmIdx == 6) {
        wave_speed = 0.0f;
        splash_target = 0.0f;
    } else {
        wave_speed = l_HIO.m18 * speed_ratio;
        splash_target = l_HIO.m10;
    }

    switch (m2B0) {
    case 1:
        max_speed = l_HIO.m24 * (0.6f + REG12_F(13));
        wave_speed = wave_speed * (0.7f + REG12_F(13));
        break;
    case 0:
        wave_speed = 0.0f;
        splash_target = 0.0f;
        break;
    case 8:
        wave_speed = 0.0f;
        splash_target = 0.0f;
        break;
    }

    switch (mPrmIdx) {
    case 0xA:
        wave_speed = 0.0f;
        splash_target = 0.0f;
        break;
    }

    mDFC.y = daSea_calcWave(mDFC.x, mDFC.z);
    mE08.y = current.angle.y;

    mD7C.setSpeed(wave_speed);
    mD18.setSpeed(wave_speed);
    mD7C.setMaxSpeed(max_speed);
    mD18.setMaxSpeed(max_speed);
    mD7C.setPitch(1.0f + l_HIO.m20);
    mD18.setPitch(1.0f - l_HIO.m20);

    cXyz collapse0 = l_HIO.m28;
    cXyz collapse1 = l_HIO.m34;
    cXyz collapse0_l = collapse0;
    cXyz collapse1_l = collapse1;
    mD7C.setAnchor(&collapse0, &collapse1);
    collapse0_l.x *= -1.0f;
    collapse1_l.x *= -1.0f;
    mD18.setAnchor(&collapse0_l, &collapse1_l);

    mD7C.setMaxDisSpeed(l_HIO.m1C);
    mD18.setMaxDisSpeed(l_HIO.m1C);
    cLib_addCalc2(&mE10, splash_target, 0.1f, 10.0f);
    mDE0.setSpeed(mE10);
    mDE0.setMaxSpeed(l_HIO.m14);
}

/* 00003268-000032E4       .text lineCheck__6daGy_cFP4cXyzP4cXyz */
void daGy_c::lineCheck(cXyz* i_start, cXyz* i_end) {
    mLinChk.Set(i_start, i_end, this);
    if (dComIfG_Bgsp()->LineCross(&mLinChk)) {
        *i_end = mLinChk.GetCross();
        mE80 = 1;
    }
}

/* 000032E4-000038EC       .text checkTgHit__6daGy_cFv */
void daGy_c::checkTgHit() {
    mStts.Move();

    cCcD_Obj* hit_obj = mCps.GetTgHitObj();
    cXyz hit_pos = *mCps.GetTgHitPosP();
#if VERSION == VERSION_DEMO
    if (hit_obj == NULL) {
        for (int i = 0; i < 6; i++) {
            hit_obj = mSph[i].GetTgHitObj();
            hit_pos = *mSph[i].GetTgHitPosP();
            if (hit_obj != NULL) {
                break;
            }
        }
    }
#else
    if (hit_obj == NULL) {
        hit_obj = mHeadSph.GetTgHitObj();
        hit_pos = *mHeadSph.GetTgHitPosP();
    }
    if (hit_obj == NULL) {
        hit_obj = mSph.GetTgHitObj();
        hit_pos = *mSph.GetTgHitPosP();
    }
#endif
    if (hit_obj == NULL) {
        return;
    }

    u8 damaged = 0;
    u32 atType = hit_obj->GetAtType();
    if (atType & AT_TYPE_BOMB) {
        health = 0;
        m920 = l_HIO.m14C;
        damaged = 1;
    } else if (atType & AT_TYPE_NORMAL_ARROW) {
        fopAcM_seStart(this, JA_SE_LK_MS_WEP_HIT, 0x20);
        health -= 2;
        m920 = l_HIO.m150;
        damaged = 1;
    } else if (atType & AT_TYPE_ICE_ARROW) {
        mCE8 = 9;
        fopAcM_seStart(this, JA_SE_LK_MS_WEP_HIT, 0x20);
        m4FC = 10;
#if VERSION > VERSION_DEMO
        mDE0.remove();
        mD18.remove();
        mD7C.remove();
#endif
        damaged = 0;
    } else if (atType & AT_TYPE_FIRE_ARROW) {
        fopAcM_seStart(this, JA_SE_LK_MS_WEP_HIT, 0x20);
        health -= 2;
        m920 = l_HIO.m150;
        damaged = 1;
    } else if (atType & AT_TYPE_LIGHT_ARROW) {
        mCE8 = 0xB;
        fopAcM_seStart(this, JA_SE_LK_MS_WEP_HIT, 0x20);
        m500 = 10;
#if VERSION > VERSION_DEMO
        mDE0.remove();
        mD18.remove();
        mD7C.remove();
#endif
        damaged = 0;
    } else if (atType & AT_TYPE_HOOKSHOT) {
        fopAcM_seStart(this, JA_SE_LK_MS_WEP_HIT, 0x20);
        health -= 2;
        m920 = l_HIO.m154;
        damaged = 1;
    } else if (atType & AT_TYPE_BOOMERANG) {
        fopAcM_seStart(this, JA_SE_LK_W_WEP_HIT, 0x20);
        health -= 2;
        m920 = l_HIO.m158;
        damaged = 1;
    }

    if (damaged == 1) {
        mDoAud_onEnemyDamage();
        daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHITFLASH, &hit_pos);
        if (mCE8 == 0xB) {
            cXyz scale(2.0f, 2.0f, 2.0f);
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hit_pos, &player->shape_angle, &scale);
            fopAcM_seStart(this, JA_SE_LK_LAST_HIT, 0);
            return;
        }
        if (health <= 0) {
            cXyz scale(2.0f, 2.0f, 2.0f);
            dComIfGp_particle_set(dPa_name::ID_AK_JN_CRITICALHIT, &hit_pos, &player->shape_angle, &scale);
            fopAcM_seStart(this, JA_SE_LK_LAST_HIT, 0);
            health = 0;
            fopAcM_monsSeStart(this, JA_SE_CV_GY_DIE, 0);
            if (m920 == l_HIO.m14C) {
                modeDeleteBombInit();
            } else {
                modeDeleteInit();
            }
        } else {
            m2B4 = m2B0;
            fopAcM_monsSeStart(this, JA_SE_CV_GY_DAMAGE, 0);
            modeDamageInit();
        }
    } else {
        if (mCE8 == 9 || mCE8 == 0xB) {
            modeDeleteInit();
        }
    }
}

/* 000038EC-000039AC       .text getWaterY__6daGy_cFv */
f32 daGy_c::getWaterY() {
    f32 waterY;
    if (mAcch.ChkWaterHit()) {
        waterY = mAcch.m_wtr.GetHeight();
        if (current.pos.y > waterY) {
            gravity = -2.5f;
            waterY = current.pos.y;
        } else {
            gravity = 0.0f;
        }
    } else {
        cXyz pos = current.pos;
        pos.y += 1000.0f;
        waterY = dBgS_GetWaterHeight(pos);
        if (current.pos.y > waterY) {
            gravity = -2.5f;
            waterY = current.pos.y;
        } else {
            gravity = 0.0f;
        }
    }
    return waterY;
}

/* 000039AC-00004264       .text _execute__6daGy_cFv */
bool daGy_c::_execute() {
#if VERSION == VERSION_DEMO
    shape_angle = current.angle;
    if (enemy_ice(&mEnemyIce)) {
        J3DModel* model = mpMorf->getModel();
        model->setBaseTRMtx(mDoMtx_stack_c::get());
        mpMorf->calc();
        return true;
    }

    lineCheck(&current.pos, &mE84);
    daGy_Ctrl_c* ctrl = mpCtrl;
    if (ctrl == NULL) {
        fopAcM_delete(this);
        return true;
    }
    if (mE80 != 0) {
        modeDiveInit();
    }
    if (m2B0 != 0 && ctrl->m324 == 3) {
        modeDiveInit();
    }
    m2BC = ctrl->m290[m2AC];

    mpMorf->play(NULL, 0, 0);
    mpMorf->calc();
    checkTgHit();

    if (gravity == 0.0f) {
        cLib_addCalc2(&m4E4, m4E8, m918, m4EC);
    }
    cLib_addCalc2(&speedF, mAimSpeedF, 0.3f, 4.0f);
    setCollision();
    setAnm();
#else
    mpCtrl = NULL;
    fpc_ProcID parent_id = fopAcM_GetLinkId(this);
    if (parent_id != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor;
        if (fopAcM_SearchByID(parent_id, &actor)) {
            if (actor != NULL) {
                if (fopAc_IsActor(actor) && (fopAcM_GetName(actor) == fpcNm_GY_CTRL_e || fopAcM_GetName(actor) == fpcNm_GY_CTRLB_e)) {
                    mpCtrl = (daGy_Ctrl_c*)actor;
                } else {
                    if (cLib_calcTimer(&m8F0) == 0) {
                        fopAcM_delete(this);
                    }
                    return true;
                }
            } else {
                fopAcM_delete(this);
                return true;
            }
        } else {
            fopAcM_delete(this);
            return true;
        }
    } else {
        fopAcM_delete(this);
        return true;
    }

    daGy_Ctrl_c* ctrl = mpCtrl;
    shape_angle = current.angle;
    if (enemy_ice(&mEnemyIce)) {
        J3DModel* model = mpMorf->getModel();
        model->setBaseTRMtx(mDoMtx_stack_c::get());
        mpMorf->calc();
        return true;
    }

    lineCheck(&current.pos, &mE84);
    if (m2B0 != 0 && mE80 != 0) {
        modeDiveInit();
    }
    if (m2B0 != 0 && ctrl->m324 == 3) {
        modeDiveInit();
    }
    m2BC = ctrl->m290[m2AC];

    mpMorf->play(NULL, 0, 0);
    mpMorf->calc();
    if (m2B0 != 0) {
        setAtCollision();
        setCollision();
        checkTgHit();
    }

    if (gravity == 0.0f) {
        cLib_addCalc2(&m4E4, m4E8, m918, m4EC);
    }
    cLib_addCalc2(&speedF, mAimSpeedF, 0.3f, 4.0f);
    setAnm();
#endif

    if (l_HIO.m97 != 0) {
        cXyz pos = current.pos;
        pos.y += 1000.0f;
        current.pos.y = dBgS_GetWaterHeight(pos);
        setMtx();
        setWave();
        return true;
    }

    modeProcCall();
    fopAcM_posMoveF(this, mStts.GetCCMoveP());
    current.pos.y = getWaterY();

    s8 prm = mPrmIdx;
    if (prm == 5) {
        if (speed.y < -5.0f) {
            cLib_addCalcAngleS2(&current.angle.x, REG12_S(1) + 0x2000, 8, 0x400);
        } else if (speed.y > 5.0f) {
            cLib_addCalcAngleS2(&current.angle.x, REG12_S(2) - 0x1000, 8, 0x400);
        } else {
            cLib_addCalcAngleS2(&current.angle.x, 0, 8, 0x800);
        }
        if (gravity == 0.0f && m91C < 0.0f) {
            mPrmIdx = 1;
            f32 s = 2.0f + REG10_F(0);
            cXyz scale(s, s + REG10_F(1), s);
            dComIfGp_particle_set(dPa_name::ID_IT_JT_MIZUBASHIRA, &current.pos, NULL, &scale);
            dComIfGp_particle_set(dPa_name::ID_IT_JN_MIZUSHIBUKI_A, &current.pos, NULL, &scale);
            fopAcM_seStart(this, JA_SE_CM_GY_LANDING_L, 0);
        }
    } else if (prm == 6 || prm == 4) {
        if (m2B0 == 4) {
            cLib_addCalcAngleS2(&current.angle.x, 0, 8, 0x800);
        }
        daSea_calcWave(current.pos.x, current.pos.z);
        if (gravity == 0.0f && m91C < 0.0f) {
            f32 s = 1.0f + REG10_F(0);
            cXyz scale(s, s + REG10_F(1), s);
            dComIfGp_particle_set(dPa_name::ID_IT_JN_MIZUSHIBUKI_A, &current.pos, NULL, &scale);
            fopAcM_seStart(this, JA_SE_CM_GY_LANDING_S, 0);
        }
    } else if (prm == 7 || prm == 9) {
        daSea_calcWave(current.pos.x, current.pos.z);
        if (gravity == 0.0f && m91C < 0.0f) {
            f32 s = 2.0f + REG10_F(0);
            cXyz scale(s, s + REG10_F(1), s);
            dComIfGp_particle_set(dPa_name::ID_IT_JT_MIZUBASHIRA, &current.pos, NULL, &scale);
            dComIfGp_particle_set(dPa_name::ID_IT_JN_MIZUSHIBUKI_A, &current.pos, NULL, &scale);
            fopAcM_seStart(this, JA_SE_CM_GY_LANDING_L, 0);
        }
    } else {
        cLib_addCalcAngleS2(&current.angle.x, 0, 8, 0x800);
    }

    if (m2B0 != 0) {
        mAcch.CrrPos(*dComIfG_Bgsp());
    }
    tevStr.mRoomNo = current.roomNo;
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mAcch.m_gnd);
    setMtx();
    setWave();
    attention_info.position = current.pos;
    attention_info.position.y += 160.0f;
    eyePos = current.pos;
    eyePos.y += 60.0f;
    m91C = gravity;

    if (mPrmIdx != 5 && m2B0 != 0) {
        f32 dist = (current.pos - old.pos).abs();
        f32 f31 = dist / 30.0f;
        if (f31 <= 0.0f) {
            f31 = 0.0f;
        } else if (f31 >= 1.0f) {
            f31 = 1.0f;
        }
        s8 vol = 100.0f * f31;
        fopAcM_seStart(this, JA_SE_CM_GY_CRUISING, vol);
        if (mPrmIdx == 1) {
            f32 play = f31 * l_HIO.m04;
            f32 res;
            if (play < l_HIO.m08) {
                res = l_HIO.m08;
            } else {
                res = play;
            }
            mpMorf->setPlaySpeed(res);
        }
    }

    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    return true;
}

/* 00004264-00004560       .text drawDebug__6daGy_cFv */
void daGy_c::drawDebug() {
    {
        if (l_HIO.m180 != 0x7FFFF || l_HIO.m180 != 0) {
            cXyz pos = current.pos;
            pos.y += 10.0f;
            int angle1 = shape_angle.y + l_HIO.m180;
            cXyz corner1(pos);
            corner1.z += l_HIO.m184 * cM_scos(angle1);
            corner1.x += l_HIO.m184 * cM_ssin(angle1);
            cXyz corner2(pos);
            int angle2 = shape_angle.y - l_HIO.m180;
            corner2.z += l_HIO.m184 * cM_scos(angle2);
            corner2.x += l_HIO.m184 * cM_ssin(angle2);
        }
    }

    {
        if (l_HIO.m140 != 0x7FFFF || l_HIO.m140 != 0) {
            daShip_c* ship = dComIfGp_getShipActor();
            if (ship != NULL) {
                cXyz pos = ship->current.pos;
                pos.y += 100.0f;
                int angle1 = ship->shape_angle.y + l_HIO.m140;
                cXyz corner1(pos);
                corner1.z += 3000.0f * cM_scos(angle1);
                corner1.x += 3000.0f * cM_ssin(angle1);
                cXyz corner2(pos);
                int angle2 = ship->shape_angle.y - l_HIO.m140;
                corner2.z += 3000.0f * cM_scos(angle2);
                corner2.x += 3000.0f * cM_ssin(angle2);
            }
        }
    }

    {
        if (l_HIO.m142 != 0x7FFFF || l_HIO.m142 != 0) {
            daShip_c* ship = dComIfGp_getShipActor();
            if (ship != NULL) {
                cXyz pos = ship->current.pos;
                pos.y += 100.0f;
                int angle1 = ship->shape_angle.y + l_HIO.m142;
                cXyz corner1(pos);
                corner1.z += 3000.0f * cM_scos(angle1);
                corner1.x += 3000.0f * cM_ssin(angle1);
                cXyz corner2(pos);
                int angle2 = ship->shape_angle.y - l_HIO.m142;
                corner2.z += 3000.0f * cM_scos(angle2);
                corner2.x += 3000.0f * cM_ssin(angle2);
            }
        }
    }

    // Leftover debug draw colors (the draw calls are stripped in retail)
    GXColor c1 = {0xFF, 0xFF, 0x00, 0x80};
    GXColor c2 = {0xFF, 0xFF, 0x00, 0x80};
    GXColor c3 = {0xFF, 0xFF, 0x00, 0x80};
    static const GXColor color_ok = {0x00, 0xFF, 0x00, 0x80};
    static const GXColor color_ng = {0xFF, 0x00, 0x00, 0x80};
    GXColor c4 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c5 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c6 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c7 = {0x00, 0xFF, 0x00, 0x80};
    GXColor c8 = {0xFF, 0xFF, 0x00, 0x80};
    GXColor c9 = {0xFF, 0xFF, 0x00, 0x80};
    GXColor c10 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c11 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c12 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c13 = {0xFF, 0x00, 0x00, 0x80};
    GXColor c14 = {0xFF, 0xFF, 0x00, 0x80};
}

/* 00004560-000046C8       .text _draw__6daGy_cFv */
bool daGy_c::_draw() {
    if (l_HIO.m94 != 0) {
        drawDebug();
    }

#if VERSION > VERSION_JPN
    if (m4E4 <= l_HIO.mA0 && mPrmIdx != 5) {
#else
    if (m4E4 <= l_HIO.mA0) {
#endif
        return true;
    }

    if (l_HIO.m96 != 0) {
        return true;
    }

    g_env_light.setLightTevColorType(mpMorf->getModel(), &tevStr);

    if (mEnemyIce.mFreezeTimer > 0x14) {
        dMat_control_c::iceEntryDL(mpMorf, -1, &m2D8);
        return true;
    }

    mpMorf->entryDL();
    dSnap_RegistFig(DSNAP_TYPE_UNKBC, this, 1.0f, 1.0f, 1.0f);

    f32 z = current.pos.z;
    cXyz pos(current.pos.x, 150.0f + current.pos.y, z);
    m8E4 = dComIfGd_setShadow(
        m8E4, 0, mpMorf->getModel(), &pos,
        800.0f, 40.0f, current.pos.y, mAcch.GetGroundH(),
        mAcch.m_gnd, &tevStr, 0, 1.0f,
        dDlst_shadowControl_c::getSimpleTex()
    );
    return true;
}

/* 000046C8-00004920       .text createInit__6daGy_cFv */
#if VERSION == VERSION_DEMO
void daGy_c::createInit() {
    mCF8 = ZeroQuat;
    gbaName = 0x1D;
    scale.setall(l_HIO.m90);
    m4FC = -1;
    m500 = -1;
    mEnemyIce.mpActor = this;
    mEnemyIce.m00C = 1;
    mEnemyIce.mWallRadius = 0.0f;
    mEnemyIce.mCylHeight = 0.0f;
    itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("GyCtrl", 0);
    attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
    attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 0x22;
    mPrmIdx = 1;
    m918 = l_HIO.m174;
    m8EC = -1;

    static JGeometry::TVec3<f32> wave_l_direction(0.5f, 1.0f, -0.3f);
    static JGeometry::TVec3<f32> wave_r_direction(-0.5f, 1.0f, -0.3f);
    dComIfGp_particle_set(dPa_name::ID_AK_JN_SHIPWAVE00, &mDFC, &mE08, NULL, 0xFF, &mD18);
    if (mD18.getEmitter() != NULL) {
        mD18.getEmitter()->setDirection(wave_l_direction);
    }
    dComIfGp_particle_set(dPa_name::ID_AK_JN_SHIPWAVE00, &mDFC, &mE08, NULL, 0xFF, &mD7C);
    if (mD7C.getEmitter() != NULL) {
        mD7C.getEmitter()->setDirection(wave_r_direction);
    }
    dComIfGp_particle_set(dPa_name::ID_AK_JN_SHIPSPLASH00, &mDFC, &mE08, NULL, 0xFF, &mDE0);

    m4E4 = l_HIO.mA0;
    modeDiveInit();
    m2BC = current.pos;

    for (int i = 0; i < mpMorf->getModel()->getModelData()->getJointNum(); i++) {
    }

    setMtx();
    mpMorf->calc();
    mAcchCir.SetWall(30.0f, 30.0f);
    mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
    mAcch.SetWallNone();
    mAcch.SetRoofNone();
    gravity = -2.5f;
    fopAcM_posMoveF(this, NULL);
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());

    fpc_ProcID parent_id = fopAcM_GetLinkId(this);
    if (parent_id != fpcM_ERROR_PROCESS_ID_e) {
        fopAc_ac_c* actor = fopAcM_SearchByID(parent_id);
        if ((actor != NULL && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_GY_CTRL_e) ||
            fopAcM_GetName(actor) == fpcNm_GY_CTRLB_e)
        {
            mpCtrl = (daGy_Ctrl_c*)actor;
            daGy_Ctrl_c* ctrl = mpCtrl;
            m2AC = ctrl->m31C;
            ctrl->m31C += 1;
        }
    }

    mStts.Init(0x64, 0x64, this);
    for (int i = 0; i < 6; i++) {
        mSph[i].Set(l_sph_src);
        mSph[i].SetStts(&mStts);
    }
    mCps.Set(l_cps_src);
    mCps.SetStts(&mStts);
    fopAcM_setStageLayer(this);
    max_health = 4;
    health = max_health;
}
#else
void daGy_c::createInit() {
    mCF8 = ZeroQuat;
    gbaName = 0x1D;
    scale.setall(l_HIO.m90);
    m4FC = -1;
    m500 = -1;
    mEnemyIce.mpActor = this;
    mEnemyIce.m00C = 1;
    mEnemyIce.mWallRadius = 0.0f;
    mEnemyIce.mCylHeight = 0.0f;
    itemTableIdx = dComIfGp_CharTbl()->GetNameIndex("GyCtrl", 0);
    attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
    attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 0x22;
    mPrmIdx = 1;
    m918 = l_HIO.m174;
    m8EC = -1;
    m8F0 = 0x78;
    m4E4 = l_HIO.mA0;
    modeDiveInit();
    m2BC = current.pos;

    for (int i = 0; i < mpMorf->getModel()->getModelData()->getJointNum(); i++) {
    }

    setMtx();
    mpMorf->calc();
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
    mAcchCir.SetWall(30.0f, 30.0f);
    mAcch.Set(&current.pos, &old.pos, this, 1, &mAcchCir, &speed, NULL, NULL);
    mAcch.SetWallNone();
    mAcch.SetRoofNone();
    gravity = -2.5f;
    fopAcM_posMoveF(this, NULL);
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    mStts.Init(0x64, 0x64, this);
    mSph.Set(l_sph_src);
    mSph.SetStts(&mStts);
    mCps.Set(l_cps_src);
    mCps.SetStts(&mStts);
    mHeadSph.Set(l_sph_head_src);
    mHeadSph.SetStts(&mStts);
    max_health = 4;
    health = max_health;
    createWave();
}
#endif

/* 00004920-00004A80       .text _create__6daGy_cFv */
cPhs_State daGy_c::_create() {
    fopAcM_ct(this, daGy_c);

    cPhs_State phase_state = dComIfG_resLoad(&mPhs, m_arc_name);
    if (phase_state == cPhs_COMPLEATE_e) {
        if (!dComIfGs_checkGetItem(dItemNo_BOOMERANG_e)) {
            return cPhs_ERROR_e;
        }

#if VERSION > VERSION_DEMO
        fpc_ProcID parent_id = fopAcM_GetLinkId(this);
        if (parent_id != fpcM_ERROR_PROCESS_ID_e) {
            fopAc_ac_c* actor;
            if (fopAcM_SearchByID(parent_id, &actor)) {
                if (actor != NULL) {
                    if (fopAc_IsActor(actor) && (fopAcM_GetName(actor) == fpcNm_GY_CTRL_e || fopAcM_GetName(actor) == fpcNm_GY_CTRLB_e)) {
                        mpCtrl = (daGy_Ctrl_c*)actor;
                        daGy_Ctrl_c* ctrl = mpCtrl;
                        m2AC = ctrl->m31C;
                        ctrl->m31C += 1;
                        if (fopAcM_GetName(actor) == fpcNm_GY_CTRL_e) {
                            fopAcM_setStageLayer(this);
                        }
                    } else {
                        return cPhs_ERROR_e;
                    }
                } else {
                    return cPhs_ERROR_e;
                }
            } else {
                return cPhs_ERROR_e;
            }
        } else {
            return cPhs_ERROR_e;
        }
#endif

        if (!fopAcM_entrySolidHeap(this, createHeap_CB, 0x3FA0)) {
            return cPhs_ERROR_e;
        }

        createInit();
    }
    return phase_state;
}

/* 00005A80-00005AD4       .text _delete__6daGy_cFv */
bool daGy_c::_delete() {
    dComIfG_resDelete(&mPhs, m_arc_name);
    mD7C.remove();
    mD18.remove();
    mDE0.remove();
    return true;
}

/* 00005AD4-00005AF4       .text daGyCreate__FPv */
static cPhs_State daGyCreate(void* i_this) {
    return ((daGy_c*)i_this)->_create();
}

/* 00005AF4-00005B18       .text daGyDelete__FPv */
static BOOL daGyDelete(void* i_this) {
    return ((daGy_c*)i_this)->_delete();
}

/* 00005B18-00005B3C       .text daGyExecute__FPv */
static BOOL daGyExecute(void* i_this) {
    return ((daGy_c*)i_this)->_execute();
}

/* 00005B3C-00005B60       .text daGyDraw__FPv */
static BOOL daGyDraw(void* i_this) {
    return ((daGy_c*)i_this)->_draw();
}

/* 00005B60-00005B68       .text daGyIsDelete__FPv */
static BOOL daGyIsDelete(void*) {
    return TRUE;
}

static actor_method_class daGyMethodTable = {
    (process_method_func)daGyCreate,
    (process_method_func)daGyDelete,
    (process_method_func)daGyExecute,
    (process_method_func)daGyIsDelete,
    (process_method_func)daGyDraw,
};

actor_process_profile_definition g_profile_GY = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_GY_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daGy_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_GY_e,
    /* Actor SubMtd */ &daGyMethodTable,
    /* Status       */ fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_4_e,
};
