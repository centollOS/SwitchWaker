/**
 * d_a_mozo.cpp
 * Enemy - Moblin Statue / モ石像 (Mo Sekizou)
 */

// This TU does not use the dolzel_rel precompiled header (no weak @3569 .bss object, no .rodata pooling).
#include "weak_bss_936_to_1036.h" // IWYU pragma: keep
#include "d/actor/d_a_mozo.h"
#include "d/actor/d_a_beam.h"
#include "d/actor/d_a_player.h"
#include "d/d_a_obj.h"
#include "d/d_s_play.h"
#include "res/Object/Mozo.h"
#include "f_op/f_op_actor_mng.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_lib.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_hostIO.h"
#include "d/d_cc_d.h"

static dCcD_SrcCps cps_src = {
    // dCcD_SrcGObjInf
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_FIRE,
        /* SrcObjAt  Atp     */ 1,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e | cCcD_AtSPrm_VsOther_e,
        /* SrcObjTg  Type    */ AT_TYPE_ALL,
        /* SrcObjTg  SPrm    */ 0,
        /* SrcObjCo  SPrm    */ 0,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
        /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK8,
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
        /* Start  */ {0.0f, 0.0f, 0.0f},
        /* End    */ {0.0f, 0.0f, 0.0f},
        /* Radius */ 50.0f,
    }},
};

class daMozo_childHIO_c {
public:
    virtual ~daMozo_childHIO_c() {}

    /* 0x04 */ f32 m04;
    /* 0x08 */ f32 m08;
    /* 0x0C */ s16 m0C;
    /* 0x0E */ s16 m0E;
    /* 0x10 */ u8 m10;
    /* 0x11 */ u8 m11[0x14 - 0x11];
};  // Size: 0x14

STATIC_ASSERT(sizeof(daMozo_childHIO_c) == 0x14);

class daMozo_BeamChildHIO_c : public daMozo_childHIO_c {
public:
    virtual ~daMozo_BeamChildHIO_c() {}
};

class daMozo_FireChildHIO_c : public daMozo_childHIO_c {
public:
    virtual ~daMozo_FireChildHIO_c() {}
};

class daMozo_HIO_c : public JORReflexible {
public:
    daMozo_HIO_c();
    virtual ~daMozo_HIO_c() {}

    void genMessage(JORMContext* ctx) { UNUSED(ctx); }

public:
    /* 0x04 */ s8 mNo;
    /* 0x08 */ int m08;
    /* 0x0C */ daMozo_childHIO_c* mpChild[2];
    /* 0x14 */ daMozo_BeamChildHIO_c mBeamChild;
    /* 0x28 */ daMozo_FireChildHIO_c mFireChild;
    /* 0x3C */ cXyz mTargetOffset;
};  // Size: 0x48

STATIC_ASSERT(sizeof(daMozo_HIO_c) == 0x48);

/* 000000EC-000001D0       .text __ct__12daMozo_HIO_cFv */
daMozo_HIO_c::daMozo_HIO_c() {
    mTargetOffset.x = 0.0f;
    mTargetOffset.y = -300.0f;
    mTargetOffset.z = 600.0f;
    mNo = -1;
    m08 = -1;
    mpChild[0] = &mBeamChild;
    mpChild[1] = &mFireChild;
    mpChild[0]->m04 = 1000.0f;
    mpChild[0]->m08 = 1200.0f;
    mpChild[0]->m0C = 0x2000;
    mpChild[0]->m0E = 0x2800;
    mpChild[0]->m10 = 0;
    mpChild[1]->m04 = 1000.0f;
    mpChild[1]->m08 = 1200.0f;
    mpChild[1]->m0C = 0x2000;
    mpChild[1]->m0E = 0x2800;
    mpChild[1]->m10 = 0;
}

static daMozo_HIO_c l_HIO;
u8 daMozo_c::m_event_flag;

/* 000002D0-00000568       .text daMozo_nodeCallBackBeam__FP8daMozo_cP8J3DModelP7J3DNodei */
static BOOL daMozo_nodeCallBackBeam(daMozo_c* i_this, J3DModel* model, J3DNode* node, int calcTiming) {
    J3DJoint* joint = (J3DJoint*)node;
    int jntNo = joint->getJntNo();
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        static cXyz a_beam_start(60.0f, -20.0f, 0.0f);
        static cXyz a_beam_end(1250.0f, -250.0f, 0.0f);
        static cXyz a_beamL_start(60.0f, -20.0f, 12.5f);
        static cXyz a_beamR_start(60.0f, -20.0f, -12.5f);

        mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));
        mDoMtx_stack_c::multVec(&a_beam_start, &i_this->mHeadPos);
        mDoMtx_stack_c::multVec(&a_beam_end, &i_this->mHeadEndPos);

        Mtx mtx;
        mDoMtx_copy(mDoMtx_stack_c::get(), mtx);
        cXyz trans(mtx[0][3], mtx[1][3], mtx[2][3]);
        mtx[2][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[0][3] = 0.0f;
        mDoMtx_stack_c::transS(trans.x, trans.y, trans.z);
        mDoMtx_stack_c::quatM(&i_this->mQuatRotation);
        mDoMtx_stack_c::concat(mtx);

        mDoMtx_stack_c::multVec(&a_beam_start, &i_this->mBeamStart);
        mDoMtx_stack_c::multVec(&a_beam_end, &i_this->mBeamEnd);
        mDoMtx_stack_c::multVec(&a_beamL_start, &i_this->mBeamLStart);
        mDoMtx_stack_c::multVec(&a_beamR_start, &i_this->mBeamRStart);

        model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
        mDoMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    }
    return TRUE;
}

/* 00000568-00000728       .text daMozo_nodeCallBackFire__FP8daMozo_cP8J3DModelP7J3DNodei */
static BOOL daMozo_nodeCallBackFire(daMozo_c* i_this, J3DModel* model, J3DNode* node, int calcTiming) {
    J3DJoint* joint = (J3DJoint*)node;
    int jntNo = joint->getJntNo();
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        static cXyz a_fire_start(0.0f, 0.0f, 0.0f);
        static cXyz a_fire_end(5000.0f, 0.0f, 0.0f);

        mDoMtx_stack_c::copy(model->getAnmMtx(jntNo));
        mDoMtx_stack_c::multVec(&a_fire_start, &i_this->mHeadPos);
        mDoMtx_stack_c::multVec(&a_fire_end, &i_this->mHeadEndPos);

        Mtx mtx;
        mDoMtx_copy(mDoMtx_stack_c::get(), mtx);
        cXyz trans(mtx[0][3], mtx[1][3], mtx[2][3]);
        mtx[2][3] = 0.0f;
        mtx[1][3] = 0.0f;
        mtx[0][3] = 0.0f;
        mDoMtx_stack_c::transS(trans.x, trans.y, trans.z);
        mDoMtx_stack_c::quatM(&i_this->mQuatRotation);
        mDoMtx_stack_c::concat(mtx);

        mDoMtx_stack_c::multVec(&a_fire_start, &i_this->mFireStart);
        mDoMtx_stack_c::multVec(&a_fire_end, &i_this->mFireEnd);

        model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
        mDoMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    }
    return TRUE;
}

/* 00000728-0000078C       .text daMozo_nodeCallBack__FP7J3DNodei */
static BOOL daMozo_nodeCallBack(J3DNode* node, int calcTiming) {
    J3DModel* model = j3dSys.getModel();
    daMozo_c* i_this = (daMozo_c*)model->getUserArea();

    switch (i_this->mType) {
    case 0:
        return daMozo_nodeCallBackBeam(i_this, model, node, calcTiming);
    case 1:
        return daMozo_nodeCallBackFire(i_this, model, node, calcTiming);
    }

    return TRUE;
}

/* 0000078C-000007AC       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* i_this) {
    return static_cast<daMozo_c*>(i_this)->CreateHeap();
}

/* 000007AC-00000A24       .text CreateHeap__8daMozo_cFv */
BOOL daMozo_c::CreateHeap() {
    J3DModelData* mdlData = (J3DModelData*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BDL_MOZ_e);
    
    mDoExt_McaMorf* newMorf =  new mDoExt_McaMorf(
        mdlData,
        0,
        0,
        static_cast<J3DAnmTransformKey*>(dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BCK_MOZ_e)),
        J3DFrameCtrl::EMode_LOOP,
        1.0f,
        0,
        -1,
        0,
        0,
        0,
        0x11020203
    );
    
    mAnimMorf = newMorf;

    m_brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BRK_MOZ_e);
    JUT_ASSERT(0x16A, m_brk != NULL);

    m_btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BTK_MOZ_e);
    JUT_ASSERT(0x16D, m_btk != NULL);

    int brkInitResult = mBrkAnm.init(mdlData, m_brk, true, J3DFrameCtrl::EMode_NONE);
    int btkInitResult = mBtkAnm.init(mdlData, m_btk, true, J3DFrameCtrl::EMode_NONE);

    return mdlData != 0 && mAnimMorf != 0 && mAnimMorf->getModel() != 0 && brkInitResult != 0 && btkInitResult != 0;
}

/* 00000A24-00000AAC       .text set_mtx__8daMozo_cFv */
void daMozo_c::set_mtx() {
    J3DModel* mdl = mAnimMorf->getModel();
    mdl->setBaseScale(scale);

    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
        
    mdl->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00000AAC-00000C38       .text anime_proc__8daMozo_cFv */
void daMozo_c::anime_proc() {
    mAnimMorf->play(NULL, 0, 0);
    mBrkAnm.play();
    mBtkAnm.play();

    if (mAnimMorf->getFrame() < 24.0f) {
        fopAcM_seStart(this, JA_SE_OBJ_MO_STATUE_MOVE, 0);
    }

    if (mAnimMorf->checkFrame(35.0f)) {
        if (mAnimMorf->getPlaySpeed() > 0.0f) {
            fopAcM_seStart(this, JA_SE_OBJ_MO_STATUE_OPEN, 0);
        } else {
            mDoAud_seStopActor(&eyePos, JA_SE_OBJ_MO_STATUE_OPEN);
            fopAcM_seStart(this, JA_SE_OBJ_MO_STATUE_CLOSE, 0);
        }
    }
}

/* 00000C38-00000C90       .text wait_proc_init__8daMozo_cFv */
void daMozo_c::wait_proc_init() {
    setAnm(0, 0.0f);
    setProcess(&daMozo_c::wait_proc);
}

/* 00000C90-00000D58       .text wait_proc__8daMozo_cFv */
void daMozo_c::wait_proc() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    if (checkRange(0) && !player->checkPlayerFly()) {
        switch (mType) {
        case 0:
            search_beam_proc_init();
            break;
        case 1:
            search_fire_proc_init();
            break;
        case 2:
            search_beam_proc_init();
            break;
        }

        if (m_event_flag == 0) {
            m377 = 1;
        }
    }
}

/* 00000D58-00000DE0       .text search_beam_proc_init__8daMozo_cFv */
void daMozo_c::search_beam_proc_init() {
    setAnm(1, 0.0f);
    setProcess(&daMozo_c::search_beam_proc);
    mBrkAnm.setFrame(0.0f);
    mBtkAnm.setFrame(0.0f);
    mBrkAnm.setPlaySpeed(1.0f);
    mBtkAnm.setPlaySpeed(1.0f);
}

/* 00000DE0-00001230       .text search_beam_proc__8daMozo_cFv */
void daMozo_c::search_beam_proc() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (mAnimMorf->getFrame() > 30.0f) {
        cXyz toPlayer = player->current.pos - mHeadPos;
        cXyz headDir = mHeadEndPos - mHeadPos;
        Quaternion quat;
        daObj::quat_rotVec(&quat, headDir, toPlayer);
        C_QUATSlerp(&mQuatRotation, &quat, &mQuatRotation, 0.2f);
    }

    anime_proc();

    if (mAnimMorf->getFrame() > mAnimMorf->getEndFrame() - 1.0f) {
        daBeam_c* beam1 = (daBeam_c*)getBeamActor(mBeamID1);
        daBeam_c* beam2 = (daBeam_c*)getBeamActor(mBeamID2);
        if (beam1 != NULL && beam2 != NULL) {
            cXyz dir = mBeamEnd - mBeamStart;
            csXyz angle = csXyz::Zero;
            angle.y = cM_atan2s(dir.x, dir.z);
            angle.x = cM_atan2s(-dir.y, dir.absXZ());

            beam1->beamOn();
            beam1->setPosAngle(mBeamLStart, angle);
            beam1->m694 = 15.0f;

            beam2->beamOn();
            beam2->setPosAngle(mBeamRStart, angle);
            beam2->m694 = 15.0f;
        }
    }

    if (!checkRange(1)) {
        towait_proc_init();
    }
}

/* 00001230-000012C0       .text search_fire_proc_init__8daMozo_cFv */
void daMozo_c::search_fire_proc_init() {
    setAnm(1, 0.0f);
    setProcess(&daMozo_c::search_fire_proc);
    mBrkAnm.setFrame(0.0f);
    mBtkAnm.setFrame(0.0f);
    mBrkAnm.setPlaySpeed(1.0f);
    mBtkAnm.setPlaySpeed(1.0f);
    mFireTimer = 0;
}

/* 000012C0-000017F4       .text search_fire_proc__8daMozo_cFv */
void daMozo_c::search_fire_proc() {
    if (mAnimMorf->getFrame() > 30.0f || mAnm == 4 || mAnm == 3) {
        mDoMtx_stack_c::YrotS(current.angle.y);
        cXyz target;
        mDoMtx_stack_c::multVec(&l_HIO.mTargetOffset, &target);
        target += current.pos;
        target -= mHeadPos;
        cXyz headDir = mHeadEndPos - mHeadPos;
        Quaternion quat;
        daObj::quat_rotVec(&quat, headDir, target);
        C_QUATSlerp(&mQuatRotation, &quat, &mQuatRotation, 0.2f);
    }

    anime_proc();

    if (mAnm == 1 || mAnm == 3) {
        if (mAnimMorf->getFrame() > DEMO_SELECT(REG10_F(10), 0.0f) + (mAnimMorf->getEndFrame() - 50.0f)) {
            if (mFireEmitter0 == NULL) {
                mFireEmitter0 = dComIfGp_particle_set(dPa_name::ID_AK_SN_MOZFIRE00, &current.pos);
            }
            if (mFireEmitter1 == NULL) {
                mFireEmitter1 = dComIfGp_particle_setToon(dPa_name::ID_AK_SN_MOZFIRE01, &current.pos);
            }

            mDoMtx_stack_c::copy(mAnimMorf->getModel()->getAnmMtx(MOZ_JNT_ATAMA_J_e));
#if VERSION == VERSION_DEMO
            mDoMtx_stack_c::XYZrotM(0x640 + REG10_S(0), 0x4000 + REG10_S(1), REG10_S(2));
            mDoMtx_stack_c::transM(0.0f, 50.0f + REG10_F(0), 52.0f + REG10_F(1));
#else
            mDoMtx_stack_c::XYZrotM(0x640, 0x4000, 0);
            mDoMtx_stack_c::transM(0.0f, 50.0f, 52.0f);
#endif
            if (mFireEmitter0 != NULL) {
                mFireEmitter0->setGlobalRTMatrix(mDoMtx_stack_c::get());
            }
            if (mFireEmitter1 != NULL) {
                mFireEmitter1->setGlobalRTMatrix(mDoMtx_stack_c::get());
            }

            cXyz dir = mFireEnd - mFireStart;
            if (!dir.normalizeRS()) {
                dir = cXyz::Zero;
            }
            cXyz scaled = dir;
#if VERSION == VERSION_DEMO
            f32 length = mFireTimer * (45.0f + REG10_F(5));
            f32 radius = mFireTimer * (3.0f + REG10_F(7));
            length = cLib_maxLimit<f32>(length, 600.0f + REG10_F(6));
            radius = cLib_maxLimit<f32>(radius, 80.0f + REG10_F(8));
#else
            f32 length = 45.0f * mFireTimer;
            f32 radius = 3.0f * mFireTimer;
            length = cLib_maxLimit<f32>(length, 600.0f);
            radius = cLib_maxLimit<f32>(radius, 80.0f);
#endif
            scaled *= length;
            scaled += mFireStart;
            mCps.cM3dGCps::Set(mFireStart, scaled, radius);
            mCps.SetAtVec(dir);
            dComIfG_Ccsp()->Set(&mCps);
            mSePos = scaled;
            mDoAud_seStart(JA_SE_OBJ_MO_STATUE_FIRE, &mSePos, 0, 0);

            if (mFireTimer++ > 0x3C) {
                setAnm(4, 0.0f);
                if (mFireEmitter0 != NULL) {
                    mFireEmitter0->setRate(0.0f);
                    mFireEmitter0->becomeInvalidEmitter();
                    mFireEmitter0 = NULL;
                }
                if (mFireEmitter1 != NULL) {
                    mFireEmitter1->setRate(0.0f);
                    mFireEmitter1->becomeInvalidEmitter();
                    mFireEmitter1 = NULL;
                }
                towait_proc_init();
            }
        }
    } else if (mAnm == 4) {
        if (mAnimMorf->getFrame() < 30.0f) {
            setAnm(3, 8.0f);
            mFireTimer = 0;
        }
    }
}

/* 000017F4-00001874       .text towait_proc_init__8daMozo_cFv */
void daMozo_c::towait_proc_init() {
    setAnm(2, 0.0f);
    mAnimMorf->setPlaySpeed(-0.5f);
    setProcess(&daMozo_c::towait_proc);
    mBrkAnm.setPlaySpeed(-1.0f);
    mBtkAnm.setPlaySpeed(-1.0f);
}

/* 00001874-00001B3C       .text towait_proc__8daMozo_cFv */
void daMozo_c::towait_proc() {
    daBeam_c* beam1 = (daBeam_c*)getBeamActor(mBeamID1);
    daBeam_c* beam2 = (daBeam_c*)getBeamActor(mBeamID2);
    C_QUATSlerp(&mQuatRotation, &ZeroQuat, &mQuatRotation, 0.05f);
    anime_proc();

    if (mType == 0) {
        if (beam1 == NULL) {
            return;
        }
        if (beam2 == NULL) {
            return;
        }

        BOOL done1 = beam1->beamOff();
        BOOL done2 = beam2->beamOff();
        if (done1 && done2 && mQuatRotation.w > 0.99f && mAnimMorf->getFrame() < 25.0f &&
            mBrkAnm.getFrame() < 1.0f && mBtkAnm.getFrame() < 1.0f)
        {
            wait_proc_init();
        }
    } else if (mType == 1) {
        if (mQuatRotation.w > 0.99f && mAnimMorf->getFrame() < 25.0f && mBrkAnm.getFrame() < 1.0f &&
            mBtkAnm.getFrame() < 1.0f)
        {
            wait_proc_init();
        }
    }
}

/* 00001B3C-00001D8C       .text checkRange__8daMozo_cFi */
BOOL daMozo_c::checkRange(int param) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (param == 0 && ((daPy_py_c*)player)->checkGrabWear()) {
        return FALSE;
    }

    cXyz delta = player->current.pos - current.pos;
    f32 dist = delta.absXZ();
    if (delta.y > -280.0f) {
        return FALSE;
    }

    f32 range;
    s16 maxAngle;
    if (param == 0) {
        range = l_HIO.mpChild[mType]->m04;
        maxAngle = l_HIO.mpChild[mType]->m0C;
    } else {
        range = l_HIO.mpChild[mType]->m08;
        maxAngle = l_HIO.mpChild[mType]->m0E;
    }

    cXyz dir(cM_ssin(current.angle.y), 0.0f, cM_scos(current.angle.y));
    f32 inprod = delta.inprod(dir);
    if (cLib_distanceAngleS(fopAcM_searchActorAngleY(this, dComIfGp_getPlayer(0)), current.angle.y) < maxAngle &&
        dist < range && inprod > 200.0f)
    {
        return TRUE;
    }
    return FALSE;
}

/* 00001D8C-00001F70       .text setAnm__8daMozo_cFif */
void daMozo_c::setAnm(int i_anm, float i_morf) {
    mAnm = i_anm;
    J3DAnmTransform* bck;
    f32 start;
    f32 end;
    f32 speed;
    int mode;
    switch (i_anm) {
    case 0:
        bck = (J3DAnmTransform*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BCK_MOZ_e);
        mode = J3DFrameCtrl::EMode_NONE;
        start = 0.0f;
        end = -1.0f;
        speed = 0.0f;
        break;
    case 1:
        bck = (J3DAnmTransform*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BCK_MOZ_e);
        mode = J3DFrameCtrl::EMode_NONE;
        start = 0.0f;
        end = -1.0f;
        speed = 1.0f;
        break;
    case 2:
        bck = (J3DAnmTransform*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BCK_MOZ_e);
        mode = J3DFrameCtrl::EMode_NONE;
        start = 24.0f;
        end = 36.0f;
        speed = -0.25f;
        break;
    case 3:
        bck = (J3DAnmTransform*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BCK_MOZ_e);
        mode = J3DFrameCtrl::EMode_NONE;
        start = 25.0f;
        end = -1.0f;
        speed = 1.0f;
        break;
    case 4:
        bck = (J3DAnmTransform*)dComIfG_getObjectRes("Mozo", dRes_INDEX_MOZO_BCK_MOZ_e);
        mode = J3DFrameCtrl::EMode_NONE;
        start = 32.0f;
        end = 36.0f;
        speed = -0.25f;
        break;
    default:
        return;
    }
    mAnimMorf->setAnm(bck, mode, i_morf, speed, start, end, NULL);
}

/* 00001F70-00002228       .text CreateInit__8daMozo_cFv */
cPhs_State daMozo_c::CreateInit() {
    J3DModelData* mdlData = mAnimMorf->getModel()->getModelData();

    u8 temp = fopAcM_GetParam(this) & 0xFF;
    if (temp == 0xFF) {
        temp = 0;
    }
    mType = cLib_minMaxLimit<u8>(temp, 0, 2);

    mAnimMorf->getModel()->setUserArea((u32)this);
    for (u16 i = 0; i < mdlData->getJointNum(); i++) {
        if (i == MOZ_JNT_ATAMA_J_e) {
            mdlData->getJointNodePointer(i)->setCallBack(daMozo_nodeCallBack);
        }
    }

    mQuatRotation = ZeroQuat;

    fopAcM_SetMin(this, -1000.0f, -1000.0f, -1000.0f);
    fopAcM_SetMax(this, 1000.0f, 1000.0f, 1000.0f);
    fopAcM_SetMtx(this, mAnimMorf->getModel()->getBaseTRMtx());

    mBeamLStart = current.pos;
    mBeamRStart = current.pos;

    if (mType == 0) {
        cXyz beamScale(1.5f, 1.5f, 20.0f);
        mBeamID1 = fopAcM_createChild(
            fpcNm_Beam_e, fopAcM_GetID(this), 0, &mBeamLStart, tevStr.mRoomNo, NULL, &beamScale, -1, NULL
        );
        mBeamID2 = fopAcM_createChild(
            fpcNm_Beam_e, fopAcM_GetID(this), 0x30000000, &mBeamRStart, tevStr.mRoomNo, NULL, &beamScale, -1, NULL
        );
    } else {
        mStts.Init(0xFF, 0xFF, this);
        mCps.Set(cps_src);
        mCps.SetStts(&mStts);
        mCps.cM3dGCps::Set(current.pos, current.pos, 25.0f);
    }

    set_mtx();
    mAnimMorf->calc();
    wait_proc_init();

    if (l_HIO.mNo < 0) {
        l_HIO.mNo = mDoHIO_createChild("モ石像", &l_HIO); // "Mo Statue" (mo sekizou)
    }

    return cPhs_COMPLEATE_e;
}

/* 00002228-000023B0       .text _create__8daMozo_cFv */
cPhs_State daMozo_c::_create() {
#if VERSION == VERSION_DEMO
    cPhs_State result = dComIfG_resLoad(&mPhs, "Mozo");

    if (result == cPhs_COMPLEATE_e) {
        fopAcM_SetupActor(this, daMozo_c);
#else
    fopAcM_ct(this, daMozo_c);

    cPhs_State result = dComIfG_resLoad(&mPhs, "Mozo");

    if (result == cPhs_COMPLEATE_e) {
#endif
        if (fopAcM_entrySolidHeap(this, CheckCreateHeap, 0x1AA0)) {
            result = CreateInit();
            _execute();
        } else {
            return cPhs_ERROR_e;
        }
    }

    return result;
}

/* 000023B0-0000242C       .text _delete__8daMozo_cFv */
bool daMozo_c::_delete() {
    dComIfG_resDeleteDemo(&mPhs, "Mozo");
    mDoAud_seDeleteObject(&mSePos);
    if (l_HIO.mNo >= 0) {
        mDoHIO_deleteChild(l_HIO.mNo);
        l_HIO.mNo = -1;
    }
    return true;
}

/* 0000242C-00002498       .text getBeamActor__8daMozo_cFUi */
fopAc_ac_c* daMozo_c::getBeamActor(fpc_ProcID apid) {
    fopAc_ac_c* ac = fopAcM_SearchByID(apid);
    if (ac == NULL)
        return NULL;
    if (fopAcM_IsActor(ac) && fopAcM_GetProfName(ac) == fpcNm_Beam_e)
        return ac;
    return NULL;
}

/* 00002498-00002588       .text event_move__8daMozo_cFv */
void daMozo_c::event_move() {
    if (m_event_flag == 2) {
        return;
    }

    if (eventInfo.checkCommandDemoAccrpt()) {
        m377 = 0;
        m_event_flag = 1;
    }

    if (m_event_flag == 1) {
        if (dComIfGp_evmng_endCheck("MOZO_CAM")) {
            dComIfGp_event_reset();
            m_event_flag = 2;
        }
    }

    if (m_event_flag == 0 && m377 == 1) {
        fopAcM_orderOtherEvent2(this, "MOZO_CAM", dEvtFlag_NOPARTNER_e, 0xFFFF);
        eventInfo.onCondition(dEvtCnd_UNK2_e);
    }
}

/* 00002588-000025DC       .text _execute__8daMozo_cFv */
bool daMozo_c::_execute() {
    mAnimMorf->calc();

    (this->*mCurrentProc)();
    event_move();
    set_mtx();

    return true;
}

/* 000025DC-0000267C       .text _draw__8daMozo_cFv */
bool daMozo_c::_draw() {
    J3DModelData* mdlData = mAnimMorf->getModel()->getModelData();
    g_env_light.settingTevStruct(TEV_TYPE_BG0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mAnimMorf->getModel(), &tevStr);

    mBrkAnm.entry(mdlData, mBrkAnm.getFrame());
    mBtkAnm.entry(mdlData, mBtkAnm.getFrame());
    mAnimMorf->entryDL();

    return true;
}

/* 0000267C-000026A0       .text daMozo_Draw__FP8daMozo_c */
static BOOL daMozo_Draw(daMozo_c* i_this) {
    return i_this->_draw();
}

/* 000026A0-000026C4       .text daMozo_Execute__FP8daMozo_c */
static BOOL daMozo_Execute(daMozo_c* i_this) {
    return i_this->_execute();
}

/* 000026C4-000026CC       .text daMozo_IsDelete__FP8daMozo_c */
static BOOL daMozo_IsDelete(daMozo_c*) {
    return TRUE;
}

/* 000026CC-000026F0       .text daMozo_Delete__FP8daMozo_c */
static BOOL daMozo_Delete(daMozo_c* i_this) {
    return i_this->_delete();
}

/* 000026F0-00002710       .text daMozo_Create__FP10fopAc_ac_c */
static cPhs_State daMozo_Create(fopAc_ac_c* i_this) {
    return static_cast<daMozo_c*>(i_this)->_create();
}

static actor_method_class l_daMozo_Method = {
    (process_method_func)daMozo_Create,
    (process_method_func)daMozo_Delete,
    (process_method_func)daMozo_Execute,
    (process_method_func)daMozo_IsDelete,
    (process_method_func)daMozo_Draw,
};

actor_process_profile_definition g_profile_MOZO = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_MOZO_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daMozo_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_MOZO_e,
    /* Actor SubMtd */ &l_daMozo_Method,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
