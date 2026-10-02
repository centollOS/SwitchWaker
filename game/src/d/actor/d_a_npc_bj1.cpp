/**
 * d_a_npc_bj1.cpp
 * NPC - Generic Koroks
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_npc_bj1.h"
#include "d/actor/d_a_jbo.h"
#include "d/d_com_inf_game.h"
#include "d/d_demo.h"
#include "d/d_kankyo_wether.h"
#include "d/d_path.h"
#include "d/d_snap.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/Bj.h"
#include <string.h>

static daNpc_Bj1_HIO_c l_HIO;
static fopAc_ac_c* l_check_inf[20];
static int l_check_wrk;

/* 000000EC-00000108       .text __ct__20daNpc_Bj1_childHIO_cFv */
daNpc_Bj1_childHIO_c::daNpc_Bj1_childHIO_c() {
}

/* 00000150-0000020C       .text __ct__15daNpc_Bj1_HIO_cFv */
daNpc_Bj1_HIO_c::daNpc_Bj1_HIO_c() {
    static daNpc_Bj1_childHIO_c::hio_prm_c a_prm_tbl[] = {
        {
            5000, 5000, -8192, -5000, 0, 7280, 0, -7280, 1024, 1024, 130.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 5000, -700, -5000, 0, 7280, 0, -7280, 1024, 1024, 100.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 5000, -8192, -5000, 0, 7280, 0, -7280, 1024, 1024, 150.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 7000, -200, -2000, 0, 0, 0, 0, 1024, 1024, 100.0f, 0, 300.0f,
            90, 6, 0x400, 0, 4.0f, 5.0f, 0.2f, 4.0f, 0.2f, 1.0f, 9000, 200, 4096, 5000, 250.0f, 32767, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 3000, 100, -3000, 0, 5000, 0, -5000, 1024, 1024, 110.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 5000, -100, -5000, 0, 2000, 0, -2000, 1024, 1024, 130.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 5000, -8192, -5000, 0, 7280, 0, -7280, 1024, 1024, 125.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 6, 0x400, 0, 0.4f, 5.0f, 0.2f, 40.0f,
        },
        {
            5000, 4000, -2000, -4000, 0, 7280, 0, -7280, 1024, 1024, 120.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
        {
            5000, 5000, -3500, -5000, 0, 7280, 0, -7280, 1024, 1024, 130.0f, 0, 300.0f,
            0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0f, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f,
        },
    };
    for (int i = 0; i < 9; i++) {
        mChild[i].mIndex = i;
        memcpy(&mChild[i].mPrm, &a_prm_tbl[i], sizeof(daNpc_Bj1_childHIO_c::hio_prm_c));
    }
    mNo = -1;
    mSelected = -1;
}

inline daNpc_Bj1_childHIO_c::~daNpc_Bj1_childHIO_c() {
}

static char* l_evn_tbl[] = {
    "Mak_Drg",
    "Get_Blu_Btl",
    "Calling",
};

/* 00000268-000002EC       .text searchActor_Jb__FPvPv */
static void* searchActor_Jb(void* actor, void*) {
    if (l_check_wrk < 20 && fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_JBO_e) {
        if (((jbo_class*)actor)->mType != 0) {
            l_check_inf[l_check_wrk] = (fopAc_ac_c*)actor;
            l_check_wrk++;
        }
    }
    return NULL;
}

/* 000002EC-00000338       .text nodeCallBack_Bj1__FP7J3DNodei */
static BOOL nodeCallBack_Bj1(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Bj1_c* actor = (daNpc_Bj1_c*)model->getUserArea();
        if (actor) {
            actor->nodeBj1Control(node, model);
        }
    }
    return TRUE;
}

/* 00000338-000004C4       .text nodeBj1Control__11daNpc_Bj1_cFP7J3DNodeP8J3DModel */
void daNpc_Bj1_c::nodeBj1Control(J3DNode* node, J3DModel* model) {
    static cXyz a_eye_pos_off(26.0f, 26.0f, 0.0f);
    int joint = ((J3DJoint*)node)->getJntNo();
    mDoMtx_stack_c::copy(model->getAnmMtx(joint));
    if (joint == m_hed_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getHead_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getHead_x());
        mDoMtx_stack_c::scaleM(mHeadScale.x, mHeadScale.y, mHeadScale.z);
        mDoMtx_stack_c::multVec(&m7E4, &m7C0);
    }
    if (joint == m_bbone_jnt_num) {
        mDoMtx_stack_c::XrotM(m_jnt.getBackbone_y());
        mDoMtx_stack_c::ZrotM(-m_jnt.getBackbone_x());
        mDoMtx_stack_c::scaleM(mBackScale.x, mBackScale.y, mBackScale.z);
    }
    if (joint == m_armR2_jnt_num) {
        cMtx_copy(mDoMtx_stack_c::get(), mArmMtx);
    }
    cMtx_copy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
    model->setAnmMtx(joint, mDoMtx_stack_c::get());
}

/* 00000500-0000054C       .text nodeCallBack_Prp__FP7J3DNodei */
static BOOL nodeCallBack_Prp(J3DNode* node, int timing) {
    if (timing == 0) {
        J3DModel* model = j3dSys.getModel();
        daNpc_Bj1_c* actor = (daNpc_Bj1_c*)model->getUserArea();
        if (actor) {
            actor->nodePrpControl(node, model);
        }
    }
    return TRUE;
}

/* 0000054C-00000664       .text nodePrpControl__11daNpc_Bj1_cFP7J3DNodeP8J3DModel */
void daNpc_Bj1_c::nodePrpControl(J3DNode* node, J3DModel* model) {
    int joint = ((J3DJoint*)node)->getJntNo();
    if (joint == m_stalk_jnt_num) {
        mDoMtx_stack_c::copy(mArmMtx);
        mDoMtx_stack_c::transM(-1.5f, 4.5f, -4.2f);
        mDoMtx_stack_c::XYZrotM(-0x4000, -0x5555, 0);
        cMtx_copy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
        model->setAnmMtx(joint, mDoMtx_stack_c::get());
    }
    if (joint == m_cen_jnt_num) {
        mDoMtx_stack_c::YrotS(mPrpRotY);
        mDoMtx_stack_c::revConcat(model->getAnmMtx(joint));
        cMtx_copy(mDoMtx_stack_c::get(), j3dSys.mCurrentMtx);
        model->setAnmMtx(joint, mDoMtx_stack_c::get());
    }
}

/* 00000664-00000710       .text init_BJ4_0__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::init_BJ4_0() {
    bool result = dComIfGs_isSymbol(dSymbol_FARORE_e) == 0;
    if (result) {
        result = dComIfGs_isEventBit(0x1801) != 0;
        if (result) {
            set_action(&daNpc_Bj1_c::wait_action2, NULL);
        }
    }
    return result;
}

/* 00000710-000007D4       .text init_BJ6_0__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::init_BJ6_0() {
    bool result = dComIfGs_isSymbol(dSymbol_FARORE_e) == 0;
    if (result) {
        result = dComIfGs_isEventBit(0x1801) != 0;
        if (result) {
            fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
            fopAcM_OnStatus(this, fopAcStts_UNK4000_e);
            set_action(&daNpc_Bj1_c::wait_action4, NULL);
        }
    }
    return result;
}

/* 000007D4-00000864       .text init_BJ7_0__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::init_BJ7_0() {
    bool result = dComIfGs_isEventBit(0x1801) != 0;
    if (result) {
        setPrtcl_drugPot_1();
        set_action(&daNpc_Bj1_c::wait_action3, NULL);
    }
    return result;
}

/* 00000864-00000910       .text init_BJX_0__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::init_BJX_0() {
    bool result = dComIfGs_isSymbol(dSymbol_FARORE_e) == 0;
    if (result) {
        result = dComIfGs_isEventBit(0x1801) != 0;
        if (result) {
            set_action(&daNpc_Bj1_c::wait_action1, NULL);
        }
    }
    return result;
}

/* 00000910-00000998       .text init_BJX_1__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::init_BJX_1() {
    bool result = dComIfGs_isSymbol(dSymbol_FARORE_e) != 0;
    if (result) {
        set_action(&daNpc_Bj1_c::wait_action1, NULL);
    }
    return result;
}

/* 00000998-00000C90       .text createInit__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::createInit() {
    struct prm_xyz {
        f32 x, y, z;
    };
    static prm_xyz l_bj1_prm_tbl[][3] = {
        {{1.0f, 1.0f, 1.0f}, {1.1f, 1.0f, 1.0f}, {40.0f, 10.0f, 0.0f}},
        {{0.9f, 1.0f, 1.0f}, {0.9f, 1.0f, 1.0f}, {30.0f, 10.0f, 0.0f}},
        {{1.1f, 1.0f, 1.0f}, {1.25f, 1.1f, 1.1f}, {40.0f, 10.0f, 0.0f}},
        {{0.8f, 1.0f, 1.0f}, {0.9f, 1.0f, 1.0f}, {30.0f, 10.0f, 0.0f}},
        {{0.8f, 1.0f, 1.0f}, {0.8f, 1.0f, 1.0f}, {30.0f, 10.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.15f, 1.0f, 1.05f}, {40.0f, 10.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {40.0f, 10.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.1f, 1.2f, 1.05f}, {40.0f, 10.0f, 0.0f}},
        {{1.1f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {40.0f, 10.0f, 0.0f}},
    };
    static char* a_staff_tbl[] = {
        "Bj1", "Bj1",
        "Bj2", "Bj2",
        "Bj3", "Bj3",
        "Bj4", "Bj4",
        "Bj5", "Bj5",
        "Bj6", "Bj6",
        "Bj7",
        "Bj8", "Bj8",
        "Bj9", "Bj9",
    };

    for (int i = 0; i < 3; i++) {
        mEventIdTable[i] = dComIfGp_evmng_getEventIdx(l_evn_tbl[i], 0xFF);
    }

    attention_info.flags = 0xA;
    switch (mType) {
    case 4:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xA7;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAB;
        break;
    case 0:
    case 1:
    case 7:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAB;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAB;
        break;
    case 2:
    case 3:
    case 5:
    case 8:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0xAA;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0xAA;
        break;
    case 6:
        attention_info.distances[fopAc_Attn_TYPE_TALK_e] = 0x5A;
        attention_info.distances[fopAc_Attn_TYPE_SPEAK_e] = 0x5A;
        break;
    default:
        return false;
    }

    prm_xyz* p0 = &l_bj1_prm_tbl[mType][0];
    mHeadScale.x = p0->x;
    mHeadScale.y = p0->y;
    mHeadScale.z = p0->z;
    prm_xyz* p1 = &l_bj1_prm_tbl[mType][1];
    mBackScale.x = p1->x;
    mBackScale.y = p1->y;
    mBackScale.z = p1->z;
    prm_xyz* p2 = &l_bj1_prm_tbl[mType][2];
    m7E4.x = p2->x;
    m7E4.y = p2->y;
    m7E4.z = p2->z;
    gravity = -4.5f;
    mHomePos = current.pos;
    u8 pathIdx = (fopAcM_GetParam(this) >> 16) & 0xFF;
    mPathIdx = pathIdx;
    if (pathIdx != 0xFF) {
        mPathRun.setInf(pathIdx, fopAcM_GetRoomNo(this), 1);
        if (mPathRun.isPath()) {
            fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
        } else {
            return false;
        }
    }

    mEventCut.setActorInfo2(a_staff_tbl[mSubType], this);
    mAnmNo = 0xB;

    bool result;
    switch (mSubType) {
    case 6:
        result = init_BJ4_0();
        break;
    case 10:
        result = init_BJ6_0();
        break;
    case 0:
    case 2:
    case 4:
    case 8:
    case 13:
    case 15:
        result = init_BJX_0();
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
    case 11:
    case 14:
    case 16:
        result = init_BJX_1();
        break;
    case 12:
        result = init_BJ7_0();
        break;
    default:
        result = false;
        break;
    }
    if (result) {
        shape_angle = current.angle;
    } else {
        return false;
    }
    mStts.Init(0xFF, 0xFF, this);
    mCyl.SetStts(&mStts);
    mCyl.Set(dNpc_cyl_src);
    mpMorf->setMorf(0.0f);
    setMtx(true);
    return true;
}

/* 00000C90-00000E00       .text setMtx_anmProc__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setMtx_anmProc() {
    switch (mAnmNo) {
    case 4:
        if (m8A9 != 0) {
            if (speed.y < 0.0f) {
                if (mpMorf->getFrame() < 7.0f) {
                    mpMorf->setFrame(mpMorf->getEndFrame());
                }
            } else if (mpMorf->getFrame() >= 7.0f) {
                mpMorf->setFrame(7.0f);
            }
        }
        break;
    case 7:
        if (mAnmEnd != 0 && m888 == 0) {
            if (cLib_calcTimer(&m894) == 0) {
                mDoAud_seStart(0x69F8, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
                setAnm_NUM(5);
                setPrtcl_drugPot_2();
            }
        }
        break;
    }
}

/* 00000E00-00001098       .text setMtx__11daNpc_Bj1_cFb */
void daNpc_Bj1_c::setMtx(bool force) {
    if (m857 == 0) {
        u32 sound = 0;
        if (mObjAcch.ChkGroundHit()) {
            sound = dComIfG_Bgsp()->GetMtrlSndId(mObjAcch.m_gnd);
        }
        mAnmEnd = mpMorf->play(&eyePos, sound, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        if (mpMorf->getFrame() < mFrame) {
            mAnmEnd = 1;
        }
        mFrame = mpMorf->getFrame();
        setMtx_anmProc();
        mpPrpMorf->play(&eyePos, 0, 0);
        if (m84D != 0) {
            setPrtcl_peraProOpen();
            m84D = 0;
        }
        mObjAcch.CrrPos(*dComIfG_Bgsp());
    }

    tevStr.mRoomNo = dComIfG_Bgsp()->GetRoomId(mObjAcch.m_gnd);
    tevStr.mEnvrIdxOverride = dComIfG_Bgsp()->GetPolyColor(mObjAcch.m_gnd);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    mpPrpMorf->calc();
    if (mpPlantLModel != NULL) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_armL2_jnt_num));
        mpPlantLModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mpPlantLModel->calc();
    }
    if (mpPlantRModel != NULL) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_armR2_jnt_num));
        mpPlantRModel->setBaseTRMtx(mDoMtx_stack_c::get());
        mpPlantRModel->calc();
    }
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_hed_jnt_num));
    mpFaceModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpFaceModel->calc();
    flwPrtcl_danceLR();
    setAttention(force);
}

/* 00001098-000010AC       .text anmNum_toResID__11daNpc_Bj1_cFi */
int daNpc_Bj1_c::anmNum_toResID(int index) {
    static const int a_bck_resID_tbl[] = {
        dRes_ID_BJ_BCK_WAIT01_e,
        dRes_ID_BJ_BCK_FLY01_e,
        dRes_ID_BJ_BCK_TALK01_e,
        dRes_ID_BJ_BCK_TALK02_e,
        dRes_ID_BJ_BCK_HAPPY_e,
        dRes_ID_BJ_BCK_SHOCK_e,
        dRes_ID_BJ_BCK_BOW_e,
        dRes_ID_BJ_BCK_DANCE_e,
        dRes_ID_BJ_BCK_WALK_e,
        dRes_ID_BJ_BCK_THROW_e,
        dRes_ID_BJ_BCK_FLY02_e,
    };
    return a_bck_resID_tbl[index];
}

/* 000010AC-00001188       .text setAnm_anm__11daNpc_Bj1_cFPQ211daNpc_Bj1_c9anm_prm_c */
BOOL daNpc_Bj1_c::setAnm_anm(daNpc_Bj1_c::anm_prm_c* prm) {
    if (mAnmNo == prm->mAnm) {
        return TRUE;
    }
    mAnmNo = prm->mAnm;
    int resource = anmNum_toResID(mAnmNo);
    dNpc_setAnmIDRes(mpMorf, prm->mLoop, prm->mMorf, prm->mSpeed, resource, -1, "Bj");
    switch (mAnmNo) {
    case 7:
        setPrtcl_danceLR();
        m888 = 0;
        m894 = 0x1E;
        break;
    default:
        delPrtcl_danceLR();
        break;
    }
    mFrame = 0.0f;
    mAnmLoops = 0;
    mAnmEnd = 0;
    return TRUE;
}

/* 00001188-000011B8       .text setAnm_NUM__11daNpc_Bj1_cFi */
void daNpc_Bj1_c::setAnm_NUM(int index) {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2},
        {1, 0, 8.0f, 1.0f, 0},
        {2, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, DEMO_SELECT(1.8f, 1.0f), 2},
        {4, 0, 8.0f, 1.0f, 2},
        {5, 0, 8.0f, 1.0f, 0},
        {6, 0, 8.0f, 1.0f, 0},
        {7, 0, 8.0f, 1.0f, 0},
        {8, 0, 8.0f, 1.0f, 2},
        {9, 0, 8.0f, 1.0f, 0},
        {10, 0, 8.0f, 1.0f, 2},
        {0, 0, 0.0f, 0.0f, 0},
    };
    setAnm_anm(&a_anm_prm_tbl[index]);
}

/* 000011B8-000011F4       .text setAnm__11daNpc_Bj1_cFv */
BOOL daNpc_Bj1_c::setAnm() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {-1, 0, 0.0f, 0.0f, -1},
        {0, 0, 8.0f, 1.0f, 2},
        {-1, 0, 0.0f, 0.0f, -1},
        {1, 0, 8.0f, 1.0f, 0},
        {10, 0, 8.0f, 1.0f, 2},
        {0, 0, 8.0f, 1.0f, 2},
        {8, 0, 8.0f, 1.0f, 2},
        {0, 0, 8.0f, 1.0f, 2},
        {0, 0, 8.0f, 1.0f, 2},
    };
    setAnm_anm(&a_anm_prm_tbl[mState]);
    return TRUE;
}

/* 000011F4-00001378       .text setAnm_prp__11daNpc_Bj1_cFSc */
void daNpc_Bj1_c::setAnm_prp(signed char idx) {
    switch (idx) {
    case 1:
        mpPrpMorf->setAnm(
            (J3DAnmTransform*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BCK_OPEN_e),
            J3DFrameCtrl::EMode_NONE,
            0.0f,
            1.0f,
            0.0f,
            -1.0f,
            NULL
        );
        mDoAud_seStart(JA_SE_CM_PRAPELLO_OPEN, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        m84D = 1;
        break;
    case 2:
        mpPrpMorf->setAnm(
            (J3DAnmTransform*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BCK_OPEN_e),
            J3DFrameCtrl::EMode_LOOP,
            0.0f,
            -1.0f,
            0.0f,
            -1.0f,
            NULL
        );
        mpPrpMorf->setFrame(7.0f);
        mDoAud_seStart(JA_SE_CM_PRAPELLO_CLOSE, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        break;
    }
}

/* 00001378-0000137C       .text chg_anmTag__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::chg_anmTag() {
}

/* 0000137C-00001380       .text control_anmTag__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::control_anmTag() {
}

/* 00001380-000013C0       .text chg_anmAtr__11daNpc_Bj1_cFUc */
void daNpc_Bj1_c::chg_anmAtr(unsigned char attr) {
    if (attr >= 0xB || attr == mAnmAttr) {
        return;
    }
    mAnmAttr = attr;
    setAnm_ATR();
}

/* 000013C0-0000143C       .text control_anmAtr__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::control_anmAtr() {
    switch (mAnmAttr) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        break;
    case 6:
    case 7:
        if (mAnmEnd != 0) {
            mAnmAttr = 0;
            setAnm_NUM(0);
        }
        break;
    case 10:
        if (mAnmEnd != 0) {
            mAnmAttr = 2;
            setAnm_NUM(3);
        }
        break;
    }
}

/* 0000143C-00001470       .text setAnm_ATR__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setAnm_ATR() {
    static anm_prm_c a_anm_prm_tbl[] = {
        {0, 0, 8.0f, 1.0f, 2},
        {2, 0, 8.0f, 1.0f, 2},
        {3, 0, 8.0f, 1.0f, 2},
        {4, 0, 8.0f, 1.0f, 2},
        {5, 0, 8.0f, 1.0f, 0},
        {6, 0, 8.0f, 1.0f, 0},
        {6, 0, 8.0f, 1.0f, 0},
        {4, 0, 8.0f, 1.0f, 2},
        {7, 0, 8.0f, 1.0f, 0},
        {8, 0, 8.0f, 1.0f, 2},
        {5, 0, 8.0f, 1.0f, 0},
    };
    setAnm_anm(&a_anm_prm_tbl[mAnmAttr]);
}

/* 00001470-00001534       .text anmAtr__11daNpc_Bj1_cFUs */
void daNpc_Bj1_c::anmAtr(unsigned short status) {
    switch (status) {
    case 6:
        if (mMsgAnmStarted == 0) {
            mAnmAttr = 0xFF;
            chg_anmAtr(dComIfGp_getMesgAnimeAttrInfo());
            mMsgAnmStarted++;
        }
        {
            u8 tag = dComIfGp_getMesgAnimeTagInfo();
            dComIfGp_clearMesgAnimeTagInfo();
            if (tag != 0xFF && mAnmTag != tag) {
                mAnmTag = tag;
                chg_anmTag();
            }
        }
        break;
    case 14:
        mMsgAnmStarted = 0;
        break;
    }
    control_anmTag();
    control_anmAtr();
}

/* 00001534-000015D8       .text eventOrder__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::eventOrder() {
    s8 order = m8B0;
    if (order == 1 || order == 2) {
        eventInfo.onCondition(dEvtCnd_CANTALK_e);
        if (mSubType == 12) {
            eventInfo.onCondition(dEvtCnd_CANTALKITEM_e);
        }
        if (m8B0 == 1) {
            fopAcM_orderSpeakEvent(this);
        }
    } else if (order >= 3) {
        mEventIdx = order - 3;
        fopAcM_orderOtherEventId(this, mEventIdTable[mEventIdx], 0xFF, 0xFFFF, 0, 1);
    }
}

/* 000015D8-00001690       .text checkOrder__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::checkOrder() {
    if (eventInfo.checkCommandDemoAccrpt()) {
        if (dComIfGp_evmng_startCheck(mEventIdTable[mEventIdx])) {
            switch (mEventIdx) {
            case 0:
            case 1:
                break;
            case 2:
                fopAcM_OffStatus(this, fopAcStts_UNK4000_e);
                break;
            }
            m8B0 = 0;
        }
    } else if (eventInfo.checkCommandTalk() && (m8B0 == 1 || m8B0 == 2)) {
        m8B0 = 0;
        mTalking = 1;
    }
}

/* 00001690-0000172C       .text chk_talk__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::chk_talk() {
    bool result = true;
    mPresentItem = 0xFF;
    if (dComIfGp_event_chkTalkXY()) {
        if (dComIfGp_evmng_ChkPresentEnd()) {
            mPresentItem = dComIfGp_event_getPreItemNo();
        } else {
            result = false;
        }
    }
    return result;
}

/* 0000172C-000017C0       .text chk_drct__11daNpc_Bj1_cFf */
bool daNpc_Bj1_c::chk_drct(float max) {
    s16 target = cLib_targetAngleY(&current.pos, &dComIfGp_getPlayer(0)->current.pos);
    int diff = abs((s16)(target - current.angle.y));
    s16 limit = max * (32768.0f / 180.0f);
    return diff < limit;
}

/* 000017C0-000017F0       .text chk_partsNotMove__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::chk_partsNotMove() {
    return mPreviousBackboneY != m_jnt.getBackbone_y() || mPreviousHeadY != m_jnt.getHead_y();
}

/* 000017F0-000019A0       .text lookBack__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::lookBack() {
    mPreviousHeadY = m_jnt.getHead_y();
    mPreviousBackboneY = m_jnt.getBackbone_y();
    mPreviousAngleY = current.angle.y;
    cXyz dst;
    cXyz src;
    src.x = current.pos.x;
    src.y = current.pos.y;
    src.z = current.pos.z;
    src.y = eyePos.y;
    dst.set(0.0f, 0.0f, 0.0f);
    cXyz* dstP = NULL;
    s16 angle = current.angle.y;
    bool noTurn = mNoTurn;
    switch (mLookMode) {
    case 1:
        dst = dNpc_playerEyePos(-20.0f);
        dstP = &dst;
        src = current.pos;
        src.y = eyePos.y;
        break;
    case 2:
        dst = mHomePos;
        dstP = &dst;
        src.x = current.pos.x;
        src.y = current.pos.y;
        src.z = current.pos.z;
        src.y = eyePos.y;
        break;
    case 3:
        angle = mLookAngle;
        break;
    }
    cLib_addCalcAngleS2(&mTurnSpeed, l_HIO.mChild[mType].mPrm.mTurnSpeed, 4, 0x800);
    if (!m_jnt.trnChk()) {
        mTurnSpeed = 0;
    }
    m_jnt.lookAtTarget(&current.angle.y, dstP, src, angle, mTurnSpeed, noTurn);
}

/* 000019A0-00001A44       .text getMaskInf__11daNpc_Bj1_cFPUc */
bool daNpc_Bj1_c::getMaskInf(unsigned char* p) {
    bool valid = p != NULL;
    if (valid) {
        switch (mSubType) {
        case 1:
            *p = 0x01;
            break;
        case 3:
            *p = 0x02;
            break;
        case 5:
            *p = 0x04;
            break;
        case 7:
            *p = 0x08;
            break;
        case 9:
            *p = 0x10;
            break;
        case 11:
            *p = 0x20;
            break;
        case 14:
            *p = 0x40;
            break;
        case 16:
            *p = 0x80;
            break;
        default:
            valid = false;
            break;
        }
    }
    return valid;
}

/* 00001A44-00001AA8       .text chkReg__11daNpc_Bj1_cFUs */
bool daNpc_Bj1_c::chkReg(unsigned short reg) {
    u8 mask;
    bool result = getMaskInf(&mask);
    if (result) {
        u8 regval = dComIfGs_getEventReg(reg);
        result = regval & mask;
    }
    return result;
}

/* 00001AA8-00001B14       .text setReg__11daNpc_Bj1_cFUs */
void daNpc_Bj1_c::setReg(unsigned short reg) {
    u8 mask;
    if (getMaskInf(&mask)) {
        u8 regval = dComIfGs_getEventReg(reg);
        regval |= mask;
        dComIfGs_setEventReg(reg, regval);
    }
}

/* 00001B14-00001E84       .text next_msgStatus__11daNpc_Bj1_cFPUl */
u16 daNpc_Bj1_c::next_msgStatus(unsigned long* msg) {
    u16 result = 15;
    switch (*msg) {
    case 0x13ED:
        *msg = 0x13EE;
        break;
    case 0x13EE:
        *msg = 0x13EF;
        break;
    case 0x13F0:
        *msg = 0x13F1;
        break;
    case 0x13F2:
        *msg = 0x13F3;
        break;
    case 0x13F5:
        *msg = 0x13F6;
        break;
    case 0x13F8:
        *msg = 0x13F9;
        break;
    case 0x13FB:
        *msg = 0x13FC;
        break;
    case 0x13FD:
        *msg = 0x13FE;
        break;
    case 0x13FF:
        *msg = 0x1400;
        break;
    case 0x1400:
        *msg = 0x1401;
        break;
    case 0x1402:
        *msg = 0x1403;
        break;
    case 0x1403:
        *msg = 0x1404;
        break;
    case 0x1406:
        *msg = 0x1407;
        break;
    case 0x1407:
        *msg = 0x1408;
        break;
    case 0x140B:
        *msg = 0x140C;
        break;
    case 0x140E:
        *msg = 0x140F;
        break;
    case 0x1412:
        *msg = 0x1413;
        break;
    case 0x1414:
        *msg = 0x1415;
        break;
    case 0x1416:
        *msg = 0x1417;
        break;
    case 0x1417:
        *msg = 0x1418;
        break;
    case 0x1418:
        *msg = 0x1419;
        break;
    case 0x1419:
        *msg = 0x141A;
        break;
    case 0x141A:
        *msg = 0x141B;
        break;
    case 0x141B:
        *msg = 0x141C;
        break;
    case 0x141C:
        *msg = 0x141D;
        break;
    case 0x141F:
        *msg = 0x1420;
        break;
    case 0x142C:
        *msg = 0x142D;
        break;
    case 0x1431:
        *msg = 0x1432;
        break;
    case 0x1433:
        *msg = 0x1417;
        break;
    case 0x1422:
        *msg = 0x1423;
        break;
    case 0x1423:
        dComIfGs_onEventBit(0x1B80);
        // fallthrough
    case 0x1424:
        if (dComIfGs_getBeastNum(dBeastIdx_BOKOBABA_SEED_e) < 4) {
            if (m840 != 0) {
                *msg = 0x1427;
            } else {
                *msg = 0x1425;
            }
        } else if (dComIfGs_checkEmptyBottle()) {
            *msg = 0x1428;
        } else if (m841 != 0) {
            *msg = 0x1430;
        } else {
            *msg = 0x142E;
        }
        break;
    case 0x1425:
        *msg = 0x1426;
        break;
    case 0x1428:
        *msg = 0x1429;
        break;
    case 0x142E:
        *msg = 0x142F;
        break;
    case 0x1487:
        *msg = 0x1488;
        break;
    case 0x1488:
        *msg = 0x1489;
        break;
    case 0x1482:
        *msg = 0x1483;
        break;
    case 0x148A:
        *msg = 0x148B;
        break;
    case 0x148B:
        *msg = 0x148C;
        break;
    case 0x148C:
        *msg = 0x148D;
        break;
    case 0x148D:
        *msg = 0x148E;
        break;
    case 0x148E:
        *msg = 0x148F;
        break;
    case 0x148F:
        *msg = 0x1490;
        break;
    case 0x1490:
        *msg = 0x1491;
        break;
    case 0x1491:
        *msg = 0x1492;
        break;
    case 0x1492:
        *msg = 0x1493;
        break;
    case 0x1497:
        *msg = 0x1498;
        break;
    case 0x1498:
        *msg = 0x1499;
        break;
    case 0x1499:
        *msg = 0x149A;
        break;
    case 0x149A:
        *msg = 0x149B;
        break;
    case 0x149B:
        *msg = 0x149C;
        break;
    case 0x149C:
        *msg = 0x149D;
        break;
    case 0x1494:
        *msg = 0x1495;
        break;
    case 0x1495:
        *msg = 0x1496;
        break;
    default:
        result = 16;
        break;
    }
    return result;
}

/* 00001E84-00001F14       .text getMsg_BJ1_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ1_0() {
    if (!dComIfGs_isEventBit(0x604)) {
        return 0x140E;
    }
    if (dKyw_get_windsdir() == 1) {
        if (!dComIfGs_isEventBit(0xC80)) {
            return 0x1412;
        }
        return 0x1414;
    }
    return (dComIfGs_isEventBit(0x904) ? 1 : 0) + 0x1410;
}

/* 00001F14-00001F54       .text getMsg_BJ2_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ2_0() {
    if (!dComIfGs_isEventBit(0x920)) {
        return 0x13F2;
    }
    return 0x13F4;
}

/* 00001F54-00001FA8       .text getMsg_BJ3_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ3_0() {
    if (m84B != 0) {
        return 0x13FB;
    }
    if (!dComIfGs_isEventBit(0xC20)) {
        return 0x13F8;
    }
    return 0x13FA;
}

/* 00001FA8-00001FE8       .text getMsg_BJ4_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ4_0() {
    if (!dComIfGs_isEventBit(0xC08)) {
        return 0x13FD;
    }
    return 0x13FF;
}

/* 00001FE8-00002028       .text getMsg_BJ5_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ5_0() {
    if (!dComIfGs_isEventBit(0xC04)) {
        return 0x13F5;
    }
    return 0x13F7;
}

/* 00002028-00002068       .text getMsg_BJ6_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ6_0() {
    if (!dComIfGs_isEventBit(0xC02)) {
        return 0x1402;
    }
    return 0x1405;
}

/* 00002068-000021C4       .text getMsg_BJ7_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ7_0() {
    if (m842 != 0) {
        m842 = 0;
        return 0x142B;
    }
    if (m843 != 0) {
        m843 = 0;
        return 0x142C;
    }
    if (mPresentItem == dItemNo_BOKOBABA_SEED_e) {
        if (!dComIfGs_isEventBit(0x1B80)) {
            return 0x1422;
        }
        return 0x1424;
    }
    if (mPresentItem != 0xFF) {
        return 0x1421;
    }
    if (dComIfGs_isSymbol(dSymbol_FARORE_e)) {
        if (dComIfGs_isEventBit(0xD08) == 0) {
            return 0x1433;
        }
        if (dComIfGs_isEventBit(0x1C80)) {
            return dComIfGs_isEventBit(0x1B80) ? 0x141E : 0x141F;
        }
        return 0x1431;
    }
    if (dComIfGs_isEventBit(0xD08) != 0) {
        return dComIfGs_isEventBit(0x1B80) ? 0x141E : 0x141F;
    }
    return 0x1416;
}

/* 000021C4-00002254       .text getMsg_BJ8_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ8_0() {
    if (!dComIfGs_isEventBit(0xD40)) {
        return 0x1406;
    }
    if (dKyw_get_windsdir() == 7) {
        if (!dComIfGs_isEventBit(0xD10)) {
            return 0x140B;
        }
        return 0x140D;
    }
    return (dComIfGs_isEventBit(0xD20) ? 1 : 0) + 0x1409;
}

/* 00002254-00002294       .text getMsg_BJ9_0__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_BJ9_0() {
    if (!dComIfGs_isEventBit(0xC10)) {
        return 0x13ED;
    }
    return 0x13F0;
}

/* 00002294-0000242C       .text getMsg_Corog__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg_Corog() {
    if (chkReg(0x9AFF)) {
#if VERSION == VERSION_DEMO
        if (!chkReg(0x9EFF)) {
            if (!chkReg(0x98FF)) {
                return 0x1487;
            }
            return 0x149E;
        }
#endif
        if (dComIfGs_isEventBit(0x102)) {
            return (chkReg(0x99FF) ? 1 : 0) + 0x1485;
        }
#if VERSION > VERSION_DEMO
        if (!chkReg(0x9EFF)) {
            if (!chkReg(0x98FF)) {
                return 0x1487;
            }
            return 0x149E;
        }
#endif
        if (chkReg(0x96FF)) {
            return 0x1484;
        }
        u8 bits = dComIfGs_getEventReg(0x9EFF);
        s16 num = 0;
        for (int i = 0; i < 8; i++) {
            if (bits & 1) {
                num++;
            }
            bits = (u8)(bits >> 1);
        }
        dComIfGp_setMessageCountNumber(8 - num);
        return 0x1482;
    }
    if (dComIfGs_isEventBit(0x2E10)) {
        if (!chkReg(0x97FF)) {
            return 0x1497;
        }
        return 0x1494;
    }
    if (!chkReg(0x97FF)) {
        return 0x148A;
    }
    return 0x1494;
}

/* 0000242C-000024EC       .text getMsg__11daNpc_Bj1_cFv */
u32 daNpc_Bj1_c::getMsg() {
    u32 msg = 0;
    switch (mSubType) {
    case 0:
        msg = getMsg_BJ1_0();
        break;
    case 2:
        msg = getMsg_BJ2_0();
        break;
    case 4:
        msg = getMsg_BJ3_0();
        break;
    case 6:
        msg = getMsg_BJ4_0();
        break;
    case 8:
        msg = getMsg_BJ5_0();
        break;
    case 10:
        msg = getMsg_BJ6_0();
        break;
    case 12:
        msg = getMsg_BJ7_0();
        break;
    case 13:
        msg = getMsg_BJ8_0();
        break;
    case 15:
        msg = getMsg_BJ9_0();
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
    case 11:
    case 14:
    case 16:
        msg = getMsg_Corog();
        break;
    }
    return msg;
}

/* 000024EC-0000256C       .text chkAttention__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::chkAttention() {
    dAttention_c& attention = dComIfGp_getAttention();
    if (attention.LockonTruth()) {
        return this == attention.LockonTarget(0);
    }
    return this == attention.ActionTarget(0);
}

/* 0000256C-000025D4       .text setAttention__11daNpc_Bj1_cFb */
void daNpc_Bj1_c::setAttention(bool force) {
    f32 ofs = l_HIO.mChild[mType].mPrm.mAttentionOffsetY;
    attention_info.position.set(current.pos.x, current.pos.y + ofs, current.pos.z);
    if (!m850 && !force) {
        return;
    }
    eyePos.set(m7C0.x, m7C0.y, m7C0.z);
}

/* 000025D4-00002608       .text searchByID__11daNpc_Bj1_cFUi */
fopAc_ac_c* daNpc_Bj1_c::searchByID(fpc_ProcID id) {
    fopAc_ac_c* actor = NULL;
    fopAcM_SearchByID(id, &actor);
    return actor;
}

/* 00002608-000026B8       .text partner_srch_sub__11daNpc_Bj1_cFPFPvPv_Pv */
bool daNpc_Bj1_c::partner_srch_sub(void* (*search)(void*, void*)) {
    bool result = false;
    mPartnerID = fpcM_ERROR_PROCESS_ID_e;
    l_check_wrk = 0;
    for (int i = 0; i < 20; i++) {
        l_check_inf[i] = NULL;
    }
    fpcEx_Search(search, this);
    if (l_check_wrk != 0) {
        mPartnerID = fopAcM_GetID(l_check_inf[0]);
        result = true;
    }
    return result;
}

/* 000026B8-00002724       .text partner_srch__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::partner_srch() {
    bool result = false;
    if (mActionState == 1) {
        switch (mSubType) {
        case 4:
            result = partner_srch_sub(searchActor_Jb);
            break;
        }
        if (result) {
            mActionState++;
        }
    }
}

/* 00002724-000027B0       .text setCollision_SP___11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setCollision_SP_() {
    static struct {
        f32 h;
        f32 r;
    } l_chk_tbl[] = {
        {130.0f, 40.0f},
        {100.0f, 40.0f},
        {150.0f, 40.0f},
        {100.0f, 40.0f},
        {100.0f, 40.0f},
        {130.0f, 40.0f},
        {130.0f, 40.0f},
        {130.0f, 40.0f},
        {130.0f, 40.0f},
    };
    mCyl.SetC(current.pos);
    mCyl.SetR(l_chk_tbl[mType].r);
    mCyl.SetH(l_chk_tbl[mType].h);
    dComIfG_Ccsp()->Set(&mCyl);
}

/* 000027B0-00002840       .text set_pthPoint__11daNpc_Bj1_cFUc */
void daNpc_Bj1_c::set_pthPoint(unsigned char i_idx) {
    if (mPathRun.isPath()) {
        u8 idx = cLib_maxLimit<u8>(i_idx, mPathRun.maxPoint());
        mPathRun.setIdx(idx);
        current.pos = mPathRun.getPoint(idx);
        mPathRun.nextIdxAuto();
    }
}

/* 00002840-00002988       .text bj_clcFlySpd__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::bj_clcFlySpd() {
    switch (m8A8) {
    case 1: {
        s16 target = cLib_targetAngleY(&current.pos, &m7D8);
        cLib_addCalcAngleS(
            &current.angle.y,
            target,
            l_HIO.mChild[mType].mPrm.m22,
            l_HIO.mChild[mType].mPrm.m24,
            0
        );
        m848 = (m7D8.y - current.pos.y) > l_HIO.mChild[mType].mPrm.m28;
        f32 spd = l_HIO.mChild[mType].mPrm.m2C;
        if (m848 == 0) {
            spd *= -1.0f;
        }
        cLib_chaseF(&speed.y, spd, l_HIO.mChild[mType].mPrm.m30);
        break;
    }
    case 2:
        cLib_chaseF(&speed.y, -l_HIO.mChild[mType].mPrm.m2C, -l_HIO.mChild[mType].mPrm.m30);
        break;
    case 3:
        break;
    }
    cLib_chaseF(&speedF, m80C, m814);
}

/* 00002988-00002B80       .text bj_movPass__11daNpc_Bj1_cFb */
s32 daNpc_Bj1_c::bj_movPass(bool i_param) {
    cXyz cmpCoords;
    cXyz idxCoords;
    u8 point_idx;
    s32 o_retval = 0;
    if (mPathRun.isPath() && (point_idx = mPathRun.getIdx(), dPath_ChkClose(mPathRun.getPath()))) {
        if (mPathRun.chkPointPass(current.pos, mPathRun.getDir())) {
            mPathRun.nextIdxAuto();
            o_retval = 1;
            if (i_param) {
                goto block_18c;
            }
        }
        return o_retval;
    }

    f32 dist_xz = (m7D8 - current.pos).absXZ();
    if (dist_xz <= m81C) {
        o_retval = 1;
        if (mPathRun.isPath()) {
            if (!mPathRun.nextIdxAuto()) {
                o_retval = 2;
            }
        }
        if (i_param) {
            goto block_18c;
        }
    }
    return o_retval;

block_18c:
    u8 idx = mPathRun.getIdx();
    idxCoords = mPathRun.getPoint(idx);
    cmpCoords = mPathRun.getPoint(point_idx);
    m848 = cmpCoords.y <= idxCoords.y;
    return o_retval;
}

/* 00002B80-00002EDC       .text bj_flyMove__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::bj_flyMove() {
    if (m8A8 == 0) {
        return false;
    }
    if (m849 != 0) {
        mStopMove = 1;
        return true;
    }
    if (mPathRun.isPath()) {
        m7D8 = mPathRun.getPoint(mPathRun.getIdx());
    }
    if (m8A8 == 1 || m8A8 == 2 || m8A8 == 7) {
        s16 max = l_HIO.mChild[mType].mPrm.m40;
        s16 spd = mPrpRotSpd + l_HIO.mChild[mType].mPrm.m42;
        if (spd > max) {
            spd = max;
        }
        mPrpRotSpd = spd;
    }
    switch (m8A8) {
    case 1:
        bj_clcFlySpd();
        switch (bj_movPass(true)) {
        case 1:
        case 2:
            m849 = 1;
            break;
        }
        break;
    case 2:
        bj_clcFlySpd();
        m849 = (current.pos.y - mObjAcch.GetGroundH()) < l_HIO.mChild[mType].mPrm.m3C;
        if (m849 != 0) {
            setAnm_NUM(1);
            speed.y = 0.0f;
            m8A8 = 3;
        }
        break;
    case 6:
        m849 = mpPrpMorf->isStop() != 0;
        if (m849 != 0) {
            m8A8 = 7;
        }
        break;
    case 7:
        m849 = mPrpRotSpd > l_HIO.mChild[mType].mPrm.m46;
        if (m849 != 0) {
            setAnm_NUM(0xA);
            m82E = l_HIO.mChild[mType].mPrm.m20;
            m8A8 = 1;
        }
        break;
    case 3: {
        s16 tmp = mPrpRotSpd - l_HIO.mChild[mType].mPrm.m42;
        mPrpRotSpd = tmp < 0 ? (s16)0 : tmp;
        m849 = mPrpRotSpd == 0;
        if (m849 != 0) {
            setAnm_prp(2);
            m8A8 = 4;
        }
        break;
    }
    case 4:
        m849 = mpPrpMorf->checkFrame(0.0f) != 0;
        if (m849 != 0) {
            m84C = 0;
            gravity = -4.5f;
            m8A8 = 5;
        }
        break;
    case 5:
        m849 = mObjAcch.ChkGroundHit();
        if (m849 != 0) {
            m8A8 = 0;
        }
        break;
    }
    return true;
}

/* 00002EDC-00002F5C       .text bj_clcMovSpd__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::bj_clcMovSpd() {
    if (m8A9 == 0) {
        s16 target = cLib_targetAngleY(&current.pos, &m7D8);
        cLib_addCalcAngleS(
            &current.angle.y,
            target,
            l_HIO.mChild[mType].mPrm.m4E,
            l_HIO.mChild[mType].mPrm.m50,
            0
        );
    }
    cLib_chaseF(&speedF, m80C, m814);
}

/* 00002F5C-000030A4       .text bj_nMove__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::bj_nMove() {
    if (m84A != 0) {
        mStopMove = 1;
        return;
    }

    s8 nmove = m8A9;
    switch (nmove) {
    case 1:
        m8A9 = 2;
        return;
    case 2:
        if (mObjAcch.ChkGroundHit()) {
            setAnm_NUM(8);
            speedF = 0.0f;
            gravity = -4.5f;
            m8A9 = 0;
        }
        return;
    }

    switch (m8AA) {
    case 1:
        if (nmove != 0) {
            return;
        }
        bj_clcMovSpd();
        f32 playSpd = speedF * l_HIO.mChild[mType].mPrm.m54;
        if (playSpd < 0.5f) {
            playSpd = 0.5f;
        }
        mpMorf->setPlaySpeed(playSpd);
        switch (bj_movPass(false)) {
        case 1:
            m84A = 1;
            break;
        case 2:
            m84A = 1;
            m8AA = 0;
            break;
        }
        break;
    }
}

/* 000030A4-000031F4       .text setPrtcl_drugPot_1__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setPrtcl_drugPot_1() {
    f32 transX;
    f32 transZ;
    s16 angleY;
    if (dComIfGp_getMapTrans(fopAcM_GetRoomNo(this), &transX, &transZ, &angleY)) {
        m858.set(0, angleY, 0);
        m860.set(transX, 0.0f, transZ);
        mpDrugPotEmitter[0] = dComIfGp_particle_set(
            dPa_name::ID_AK_SN_DRUGPOT00,
            &m860,
            &m858,
            NULL,
            0xFF,
            NULL,
            fopAcM_GetRoomNo(this)
        );
        mpDrugPotEmitter[1] = dComIfGp_particle_set(
            dPa_name::ID_AK_SN_DRUGPOT01,
            &m860,
            &m858,
            NULL,
            0xFF,
            NULL,
            fopAcM_GetRoomNo(this)
        );
        mpDrugPotEmitter[2] = dComIfGp_particle_set(
            dPa_name::ID_AK_SN_DRUGPOT02,
            &m860,
            &m858,
            NULL,
            0xFF,
            NULL,
            fopAcM_GetRoomNo(this)
        );
    }
}

/* 000031F4-000032B4       .text setPrtcl_drugPot_2__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setPrtcl_drugPot_2() {
    f32 transX;
    f32 transZ;
    s16 angleY;
    if (dComIfGp_getMapTrans(fopAcM_GetRoomNo(this), &transX, &transZ, &angleY)) {
        m858.set(0, angleY, 0);
        m860.set(transX, 0.0f, transZ);
        m888 = dComIfGp_particle_set(
            dPa_name::ID_AK_SN_DRUGPOT03,
            &m860,
            &m858,
            NULL,
            0xFF,
            NULL,
            fopAcM_GetRoomNo(this)
        );
    }
}

/* 000032B4-00003330       .text delPrtcl_drugPot__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::delPrtcl_drugPot() {
    if (mpDrugPotEmitter[0] != NULL) {
        mpDrugPotEmitter[0]->becomeInvalidEmitter();
        mpDrugPotEmitter[0] = NULL;
    }
    if (mpDrugPotEmitter[1] != NULL) {
        mpDrugPotEmitter[1]->becomeInvalidEmitter();
        mpDrugPotEmitter[1] = NULL;
    }
    if (mpDrugPotEmitter[2] != NULL) {
        mpDrugPotEmitter[2]->becomeInvalidEmitter();
        mpDrugPotEmitter[2] = NULL;
    }
}

/* 00003330-000033F4       .text setPrtcl_danceLR__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setPrtcl_danceLR() {
    delPrtcl_danceLR();
    mpDanceEmitterL = dComIfGp_particle_set(
        dPa_name::ID_AK_SN_NIYATODANCEL00,
        &current.pos,
        NULL,
        NULL,
        0xFF,
        NULL,
        fopAcM_GetRoomNo(this)
    );
    mpDanceEmitterR = dComIfGp_particle_set(
        dPa_name::ID_AK_SN_NIYATODANCER00,
        &current.pos,
        NULL,
        NULL,
        0xFF,
        NULL,
        fopAcM_GetRoomNo(this)
    );
}

/* 000033F4-00003484       .text flwPrtcl_danceLR__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::flwPrtcl_danceLR() {
    if (mpDanceEmitterL != NULL) {
        mpDanceEmitterL->setGlobalRTMatrix(mpMorf->getModel()->getAnmMtx(m_armL2_jnt_num));
    }
    if (mpDanceEmitterR != NULL) {
        mpDanceEmitterR->setGlobalRTMatrix(mpMorf->getModel()->getAnmMtx(m_armR2_jnt_num));
    }
}

/* 00003484-000034D8       .text delPrtcl_danceLR__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::delPrtcl_danceLR() {
    if (mpDanceEmitterL != NULL) {
        mpDanceEmitterL->becomeInvalidEmitter();
        mpDanceEmitterL = NULL;
    }
    if (mpDanceEmitterR != NULL) {
        mpDanceEmitterR->becomeInvalidEmitter();
        mpDanceEmitterR = NULL;
    }
}

/* 000034D8-00003594       .text setPrtcl_peraProOpen__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::setPrtcl_peraProOpen() {
    mDoMtx_stack_c::copy(mpPrpMorf->getModel()->getAnmMtx(m_cen_jnt_num));
    m86C.x = mDoMtx_stack_c::now[0][3];
    m86C.y = mDoMtx_stack_c::now[1][3];
    m86C.z = mDoMtx_stack_c::now[2][3];
    mpPeraEmitter = dComIfGp_particle_set(
        dPa_name::ID_AK_SN_PERAPROOPEN00,
        &m86C,
        &current.angle,
        NULL,
        0xFF,
        NULL,
        fopAcM_GetRoomNo(this)
    );
}

/* 00003594-00003774       .text createSeed__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::createSeed() {
    fopAc_ac_c* actors[4];
    int i;
    for (i = 0; i < 4; i++) {
        mSeedID[i] = fpcM_ERROR_PROCESS_ID_e;
        actors[i] = NULL;
    }
    mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(m_armR2_jnt_num));
    cXyz pos;
    pos.x = mDoMtx_stack_c::now[0][3];
    pos.y = mDoMtx_stack_c::now[1][3];
    pos.z = mDoMtx_stack_c::now[2][3];
    for (i = 0; i < 4; i++) {
        s16 a_ang_tbl[] = {-0x1000, -0xA00, 0xA00, 0x1000};
        csXyz angle(0, 0, 0);
        cXyz scale(0.2f, 0.2f, 0.2f);
        angle.y = current.angle.y + a_ang_tbl[i];
        s8 room = fopAcM_GetRoomNo(this);
        actors[i] = fopAcM_createItemForKP2(
            &pos,
            dItemNo_BOKOBABA_SEED_e,
            room,
            &angle,
            &scale,
            10.0f + cM_rndFX(1.0f),
            14.0f + cM_rndFX(4.0f),
            -2.0f,
            1
        );
        if (actors[i] == NULL) {
            break;
        }
        fopAcM_OnStatus(actors[i], fopAcStts_UNK4000_e);
        mSeedID[i] = fopAcM_GetID(actors[i]);
    }
    return i == 4;
}

/* 00003774-000037E4       .text deleteSeed__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::deleteSeed() {
    for (int i = 0; i < 4; i++) {
        fopAc_ac_c* actor = searchByID(mSeedID[i]);
        if (actor != NULL) {
            mSeedID[i] = fpcM_ERROR_PROCESS_ID_e;
            fopAcM_delete(actor);
        }
    }
    return true;
}

/* 000037E4-00003998       .text charDecide__11daNpc_Bj1_cFi */
bool daNpc_Bj1_c::charDecide(int param) {
    mType = -1;
    mSubType = -1;
    switch (fopAcM_GetName(this)) {
    case fpcNm_NPC_BJ1_e:
        mType = 0;
        switch (param) {
        case 1:
            mSubType = 1;
            break;
        default:
            mSubType = 0;
            break;
        }
        break;
    case fpcNm_NPC_BJ2_e:
        mType = 1;
        switch (param) {
        case 1:
            mSubType = 3;
            break;
        default:
            mSubType = 2;
            break;
        }
        break;
    case fpcNm_NPC_BJ3_e:
        mType = 2;
        switch (param) {
        case 1:
            mSubType = 5;
            break;
        default:
            mSubType = 4;
            break;
        }
        break;
    case fpcNm_NPC_BJ4_e:
        mType = 3;
        switch (param) {
        case 1:
            mSubType = 7;
            break;
        default:
            mSubType = 6;
            break;
        }
        break;
    case fpcNm_NPC_BJ5_e:
        mType = 4;
        switch (param) {
        case 1:
            mSubType = 9;
            break;
        default:
            mSubType = 8;
            break;
        }
        break;
    case fpcNm_NPC_BJ6_e:
        mType = 5;
        switch (param) {
        case 1:
            mSubType = 11;
            break;
        default:
            mSubType = 10;
            break;
        }
        break;
    case fpcNm_NPC_BJ7_e:
        mType = 6;
        mSubType = 12;
        break;
    case fpcNm_NPC_BJ8_e:
        mType = 7;
        switch (param) {
        case 1:
            mSubType = 14;
            break;
        default:
            mSubType = 13;
            break;
        }
        break;
    case fpcNm_NPC_BJ9_e:
        mType = 8;
        switch (param) {
        case 1:
            mSubType = 16;
            break;
        default:
            mSubType = 15;
            break;
        }
        break;
    default:
        return false;
    }
    return true;
}

/* 00003998-000039DC       .text eInit_setLocFlag__11daNpc_Bj1_cFPi */
void daNpc_Bj1_c::eInit_setLocFlag(int* prm) {
    mNoTurn = false;
    if (prm == NULL) {
        return;
    }
    switch (*prm) {
    case 2:
        m_jnt.setTrn();
        break;
    case 1:
        mNoTurn = true;
        break;
    }
}

/* 000039DC-00003A14       .text eInit_setShapeAngleY__11daNpc_Bj1_cFPis */
void daNpc_Bj1_c::eInit_setShapeAngleY(int* prm, short angle) {
    mKeepShapeAngle = false;
    if (prm == NULL) {
        return;
    }
    mKeepShapeAngle = *prm == 1;
    if (!mKeepShapeAngle) {
        return;
    }
    shape_angle.y = angle;
}

/* 00003A14-00003A30       .text eInit_setEvTimer__11daNpc_Bj1_cFPi */
void daNpc_Bj1_c::eInit_setEvTimer(int* prm) {
    mEvTimer = 0;
    if (prm == NULL) {
        return;
    }
    mEvTimer = *prm;
}

/* 00003A30-00003B10       .text eInit_calcRelativPos__11daNpc_Bj1_cFP4cXyzPi */
cXyz daNpc_Bj1_c::eInit_calcRelativPos(cXyz* offset, int* angleP) {
    s16 angle;
    if (angleP) {
        angle = shape_angle.y + (s16)*angleP;
    } else {
        angle = shape_angle.y;
    }
    cXyz src;
    cXyz dst;
    if (offset) {
        src.set(offset->x, offset->y, offset->z);
    } else {
        src.set(0.0f, 0.0f, 0.0f);
    }
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::YrotM(angle);
    mDoMtx_stack_c::multVec(&src, &dst);
    return dst;
}

/* 00003B10-00003B20       .text eInit_prmFloat__11daNpc_Bj1_cFPff */
f32 daNpc_Bj1_c::eInit_prmFloat(float* prm, float def) {
    if (prm == NULL) {
        return def;
    }
    return *prm;
}

/* 00003B20-00003D3C       .text eInit_ATTENTION___11daNpc_Bj1_cFPiPiPiP4cXyzPiPiPi */
void daNpc_Bj1_c::eInit_ATTENTION_(int* prm0, int* prm1, int* prm2, cXyz* offset, int* angleP, int* indexP, int* timerP) {
    if (prm0 == NULL) {
        return;
    }
    s16 target;
    switch (*prm0) {
    case 0:
        break;
    case 1:
        mLookMode = 1;
        target = cLib_targetAngleY(&current.pos, &dComIfGp_getPlayer(0)->current.pos);
        break;
    case 2:
        if (offset == NULL) {
            return;
        }
        mLookMode = 2;
        target = cLib_targetAngleY(&current.pos, offset);
        break;
    case 3:
        mLookMode = 2;
        mHomePos = eInit_calcRelativPos(offset, angleP);
        target = cLib_targetAngleY(&current.pos, &mHomePos);
        break;
    case 4:
        if (angleP == NULL) {
            return;
        }
        mLookMode = 3;
        target = *angleP;
        break;
    case 5: {
        if (mPathRun.isPath() == false) {
            return;
        }
        u8 pointIndex = mPathRun.getIdx();
        if (indexP) {
            pointIndex = *indexP;
        }
        mHomePos = mPathRun.getPoint(pointIndex);
        mLookMode = 2;
        break;
    }
    case 6: {
        fopAc_ac_c* partner = searchByID(mPartnerID);
        if (partner == NULL) {
            return;
        }
        mLookMode = 2;
        mHomePos = partner->eyePos;
        target = cLib_targetAngleY(&current.pos, &mHomePos);
        break;
    }
    case 7:
        mLookMode = 2;
        mHomePos = m860;
        mHomePos.y = eyePos.y;
        target = cLib_targetAngleY(&current.pos, &mHomePos);
        break;
    default:
        mLookMode = 0;
        break;
    }
    eInit_setLocFlag(prm1);
    eInit_setShapeAngleY(prm2, target);
    eInit_setEvTimer(timerP);
}

/* 00003D3C-00003E24       .text eInit_PLYER_MOV_1___11daNpc_Bj1_cFv */
void daNpc_Bj1_c::eInit_PLYER_MOV_1_() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    s16 ang = cLib_targetAngleY(&m7AC, &player->current.pos);
    cXyz src;
    cXyz dst;
    if (abs((s16)(ang - m7B8.y)) > 0x1000) {
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(m7B8.y);
        src.set(0.0f, 0.0f, 120.0f);
        mDoMtx_stack_c::multVec(&src, &dst);
    } else {
        dst.set(player->current.pos.x, player->current.pos.y, player->current.pos.z);
    }
    dComIfGp_evmng_setGoal(&dst);
}

/* 00003E24-00003F04       .text eInit_MOV___11daNpc_Bj1_cFPfPfPfPi */
void daNpc_Bj1_c::eInit_MOV_(float* goalR, float* speed, float* accel, int* timer) {
    u8 idx = mPathRun.getIdx();
    setAnm_NUM(8);
    mLookMode = 0;
    m81C = eInit_prmFloat(goalR, 0.0f);
    m80C = eInit_prmFloat(speed, 0.0f);
    m814 = eInit_prmFloat(accel, 0.0f);
    m7D8 = mPathRun.getPoint(idx);
    eInit_setEvTimer(timer);
    m84A = 0;
    m8AA = 1;
}

/* 00003F04-00003F74       .text eInit_JMP___11daNpc_Bj1_cFPfPf */
void daNpc_Bj1_c::eInit_JMP_(float* spdY, float* grav) {
    m8A9 = 1;
    speed.y = eInit_prmFloat(spdY, 0.0f);
    gravity = eInit_prmFloat(grav, 0.0f);
    setAnm_NUM(4);
}

/* 00003F74-0000406C       .text eInit_CHG_PTH___11daNpc_Bj1_cFPiPi */
void daNpc_Bj1_c::eInit_CHG_PTH_(int* pthNo, int* indexP) {
    u8 idx;
    mPathRun.setInf(mPathIdx, fopAcM_GetRoomNo(this), 1);
    if (mPathRun.isPath() && pthNo != NULL) {
        int n = *pthNo;
        if (n > 0) {
            dPath* path = mPathRun.nextPath(fopAcM_GetRoomNo(this));
            int i = 0;
            n--;
            while (i < n) {
                if (path == NULL) {
                    break;
                }
                path = dPath_GetNextRoomPath(path, fopAcM_GetRoomNo(this));
                i++;
            }
            if (path != NULL) {
                mPathRun.setInfDrct(path);
            }
        }
    }
    if (indexP != NULL) {
        idx = *indexP;
        if (idx >= mPathRun.maxPoint()) {
            idx = mPathRun.maxPoint() - 1;
        }
        mPathRun.setIdx(idx);
    }
}

/* 0000406C-000040BC       .text eInit_END_MOV___11daNpc_Bj1_cFv */
void daNpc_Bj1_c::eInit_END_MOV_() {
    setAnm_NUM(0);
    m8AA = 0;
    m80C = 0.0f;
    speedF = m80C;
    m814 = 0.0f;
}

/* 000040BC-000040F4       .text eInit_SET_TNE___11daNpc_Bj1_cFv */
void daNpc_Bj1_c::eInit_SET_TNE_() {
    setAnm_NUM(9);
    mLookMode = 0;
}

/* 000040F4-00004114       .text eInit_DEL_TNE___11daNpc_Bj1_cFv */
void daNpc_Bj1_c::eInit_DEL_TNE_() {
    deleteSeed();
}

/* 00004114-0000417C       .text eInit_SET_ANM___11daNpc_Bj1_cFPiPf */
void daNpc_Bj1_c::eInit_SET_ANM_(int* anmNo, float* morf) {
    if (anmNo != NULL) {
        s8 anm = *anmNo;
        if (anm >= 0 && anm < 0xB) {
            setAnm_NUM(anm);
            if (morf != NULL) {
                mpMorf->setMorf(*morf);
            }
        }
    }
}

/* 0000417C-000044E4       .text event_actionInit__11daNpc_Bj1_cFi */
void daNpc_Bj1_c::event_actionInit(int staffIdx) {
    int* angle;
    cXyz* offst;
    int* prm0;
    int* prm1;
    f32* accel;
    int* index;
    int* timer;
    int* prm2;
    f32* goalR;
    f32* speed;
    f32* grvty;
    int* pthNo;
    int* anmNo;
    f32* morfc;
    int* actNo = dComIfGp_evmng_getMyIntegerP(staffIdx, "ActNo");
    if (actNo == NULL) {
        return;
    }
    mActNo = *actNo;
    m850 = 0;
    switch (mActNo) {
    case 0: {
        prm0 = dComIfGp_evmng_getMyIntegerP(staffIdx, "prm_0");
        prm1 = dComIfGp_evmng_getMyIntegerP(staffIdx, "prm_1");
        prm2 = dComIfGp_evmng_getMyIntegerP(staffIdx, "prm_2");
        offst = dComIfGp_evmng_getMyXyzP(staffIdx, "Offst");
        angle = dComIfGp_evmng_getMyIntegerP(staffIdx, "Angle");
        index = dComIfGp_evmng_getMyIntegerP(staffIdx, "Index");
        timer = dComIfGp_evmng_getMyIntegerP(staffIdx, "Timer");
        m850 = 1;
        eInit_ATTENTION_(prm0, prm1, prm2, offst, angle, index, timer);
        break;
    }
    case 1:
        eInit_PLYER_MOV_1_();
        break;
    case 2:
    case 8: {
        goalR = dComIfGp_evmng_getMyFloatP(staffIdx, "GoalR");
        speed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
        accel = dComIfGp_evmng_getMyFloatP(staffIdx, "Accel");
        timer = dComIfGp_evmng_getMyIntegerP(staffIdx, "Timer");
        m850 = 1;
        eInit_MOV_(goalR, speed, accel, timer);
        break;
    }
    case 3: {
        speed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
        grvty = dComIfGp_evmng_getMyFloatP(staffIdx, "Grvty");
        m850 = 1;
        eInit_JMP_(speed, grvty);
        break;
    }
    case 4: {
        pthNo = dComIfGp_evmng_getMyIntegerP(staffIdx, "PthNo");
        index = dComIfGp_evmng_getMyIntegerP(staffIdx, "Index");
        eInit_CHG_PTH_(pthNo, index);
        break;
    }
    case 5:
        eInit_END_MOV_();
        break;
    case 6:
        eInit_SET_TNE_();
        break;
    case 7:
        eInit_DEL_TNE_();
        break;
    case 9: {
        anmNo = dComIfGp_evmng_getMyIntegerP(staffIdx, "AnmNo");
        morfc = dComIfGp_evmng_getMyFloatP(staffIdx, "Morfc");
        eInit_SET_ANM_(anmNo, morfc);
        break;
    }
    }
}

/* 000044E4-00004530       .text eMove_ATTENTION___11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::eMove_ATTENTION_() {
    if (mEvTimer >= 0) {
        return cLib_calcTimer(&mEvTimer) == 0;
    }
    return m_jnt.trnChk() == 0;
}

/* 00004530-00004580       .text eMove_MOV___11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::eMove_MOV_() {
    if (mEvTimer >= 0) {
        return cLib_calcTimer(&mEvTimer) == 0;
    }
    return m84A == 1;
}

/* 00004580-00004594       .text eMove_JMP___11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::eMove_JMP_() {
    return m8A9 == 0;
}

/* 00004594-000045F0       .text eMove_SET_TNE___11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::eMove_SET_TNE_() {
    if (mpMorf->checkFrame(20.0f)) {
        createSeed();
    }
    return mAnmEnd != 0;
}

/* 000045F0-00004678       .text eMove_PTH_MOV___11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::eMove_PTH_MOV_() {
    bool ret = m8AA == 0;
    if (!ret) {
        if (m84A != 0) {
            m7D8 = mPathRun.getPoint(mPathRun.getIdx());
        }
        m84A = 0;
    }
    return ret;
}

/* 00004678-000046E8       .text event_action__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::event_action() {
    bool ret;
    switch (mActNo) {
    case 0:
        ret = eMove_ATTENTION_();
        break;
    case 2:
        ret = eMove_MOV_();
        break;
    case 3:
        ret = eMove_JMP_();
        break;
    case 6:
        ret = eMove_SET_TNE_();
        break;
    case 8:
        ret = eMove_PTH_MOV_();
        break;
    case 9:
    default:
        ret = true;
        break;
    }
    return ret;
}

/* 000046E8-000047D4       .text privateCut__11daNpc_Bj1_cFi */
void daNpc_Bj1_c::privateCut(int staffIdx) {
    static char* a_cut_tbl[] = {
        "ACTION",
    };
    if (staffIdx == -1) {
        return;
    }
    mCutIdx = dComIfGp_evmng_getMyActIdx(staffIdx, a_cut_tbl, ARRAY_SIZE(a_cut_tbl), TRUE, 0);
    if (mCutIdx == -1) {
        dComIfGp_evmng_cutEnd(staffIdx);
        return;
    }
    if (dComIfGp_evmng_getIsAddvance(staffIdx)) {
        switch (mCutIdx) {
        case 0:
            event_actionInit(staffIdx);
            break;
        }
    }
    bool ret;
    switch (mCutIdx) {
    case 0:
        ret = event_action();
        break;
    default:
        ret = true;
        break;
    }
    if (ret) {
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

/* 000047D4-000047F4       .text endEvent__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::endEvent() {
    dComIfGp_event_reset();
    mAnmAttr = 0xFF;
}

/* 000047F4-0000482C       .text isEventEntry__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::isEventEntry() {
    return dComIfGp_evmng_getMyStaffId(mEventCut.getActorName(), NULL, 0);
}

/* 0000482C-00004914       .text event_proc__11daNpc_Bj1_cFi */
void daNpc_Bj1_c::event_proc(int staff) {
    if (dComIfGp_evmng_endCheck(mEventIdTable[mEventIdx])) {
        switch (mEventIdx) {
        case 0:
            m8B0 = 1;
            m842 = 1;
            break;
        case 1:
            m8B0 = 1;
            m843 = 1;
            break;
        case 2:
            dComIfGs_onEventBit(0x2902);
            break;
        }
        endEvent();
    } else {
        if (!mEventCut.cutProc()) {
            privateCut(staff);
        }
        lookBack();
    }
}

/* 00004914-000049C0       .text set_action__11daNpc_Bj1_cFM11daNpc_Bj1_cFPCvPvPv_iPv */
BOOL daNpc_Bj1_c::set_action(ActionFunc action, void* arg) {
    if (mAction != action) {
        if (mAction) {
            mActionState = 9;
            (this->*mAction)(arg);
        }
        mAction = action;
        mActionState = 0;
        (this->*mAction)(arg);
    }
    return TRUE;
}

/* 000049C0-00004B60       .text setStt__11daNpc_Bj1_cFSc */
void daNpc_Bj1_c::setStt(signed char state) {
    s8 prev = mState;
    m832 = 0;
    mState = state;
    switch (mState) {
    case 2: {
        if (mSubType == 4) {
            fopAc_ac_c* actor = searchByID(mPartnerID);
            if (actor != NULL) {
                m84B = ((jbo_class*)actor)->m2BA != 0;
            }
        }
        mAnmAttr = 0xFF;
        mLookMode = 1;
        mPreviousState = prev;
        m_jnt.setTrn();
        return;
    }
    case 3:
        mLookMode = 0;
        m80C = l_HIO.mChild[mType].mPrm.m34;
        m814 = l_HIO.mChild[mType].mPrm.m38;
        gravity = 0.0f;
        m849 = 0;
        m8A8 = 6;
        setAnm_prp(1);
        m84C = 1;
        break;
    case 4:
        mLookMode = 1;
        mNoTurn = true;
        m80C = 0.0f;
        break;
    case 5:
        m834 = 0x5A;
        break;
    case 6:
        m80C = l_HIO.mChild[mType].mPrm.m58;
        m814 = l_HIO.mChild[mType].mPrm.m5C;
        m81C = l_HIO.mChild[mType].mPrm.m60;
        m8AA = 1;
        break;
    case 0:
    case 1:
    case 7:
    case 8:
        break;
    }
    setAnm();
}

/* 00004B60-00004C30       .text wait_1__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::wait_1() {
    s8 order = m8B0;
    if (order == 1 || order >= 3) {
        return 1;
    }
    if (mTalking != 0) {
        if (chk_talk()) {
            setStt(2);
        }
        return 1;
    }
    m8B0 = 2;
    if (m854 != 0) {
        m832 = 0x3C;
    }
    if (cLib_calcTimer(&m832) != 0) {
        mLookMode = 1;
        mNoTurn = false;
    } else {
        mLookMode = 3;
        mLookAngle = m7B8.y;
        m_jnt.setTrn();
    }
    return 1;
}

/* 00004C30-00004D10       .text wait_2__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::wait_2() {
    s8 order = m8B0;
    if (order == 1 || order >= 3) {
        return 1;
    }
    if (mTalking != 0) {
        if (chk_talk()) {
            setStt(2);
        }
        return 1;
    }
    m8B0 = 2;
    if (m854 != 0) {
        m832 = 0x3C;
        m834 = 0x5A;
    }
    if (cLib_calcTimer(&m832) != 0) {
        mLookMode = 1;
        mNoTurn = false;
        return 1;
    }
    if (cLib_calcTimer(&m834) == 0) {
        setStt(3);
    }
    return 1;
}

/* 00004D10-00004E2C       .text wait_3__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::wait_3() {
    cLib_addCalcAngleS(&current.angle.y, m7B8.y, l_HIO.mChild[mType].mPrm.m4E, l_HIO.mChild[mType].mPrm.m50, 0);
    s8 order = m8B0;
    if (order == 1 || order >= 3) {
        return 1;
    }
    if (mTalking != 0) {
        if (chk_talk()) {
            setStt(2);
        }
        return 1;
    }
    m8B0 = 2;
    if (m854 != 0) {
        m832 = 0x3C;
    }
    if (cLib_calcTimer(&m832) != 0 && chk_drct(46.0f)) {
        mLookMode = 1;
        mNoTurn = false;
    } else {
        mLookMode = 3;
        mLookAngle = m7B8.y;
        m_jnt.setTrn();
    }
    return 1;
}

/* 00004E2C-00004F48       .text wait_4__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::wait_4() {
    s8 order = m8B0;
    if (order == 1 || order >= 3) {
        return 1;
    }
    if (mTalking != 0) {
        if (chk_talk()) {
            setStt(2);
        }
        return 1;
    }
    if (!dComIfGs_isEventBit(0x2902) && dComIfGs_isGetItem(6, 0)) {
        m8B0 = 5;
        return 1;
    }
    m8B0 = 2;
    if (m854 != 0) {
        m832 = 0x3C;
    }
    if (cLib_calcTimer(&m832) != 0) {
        mLookMode = 1;
        mNoTurn = false;
    } else {
        mLookMode = 3;
        mLookAngle = m7B8.y;
        m_jnt.setTrn();
    }
    return 1;
}

/* 00004F48-000050CC       .text flyMov__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::flyMov() {
    m8B0 = 0;
    m849 = 0;
    if (m8A8 == 1) {
        if (cLib_calcTimer(&m82E) != 0) {
            return 1;
        }
        f32 dist = (current.pos - dComIfGp_getPlayer(0)->current.pos).absXZ();
        s16 targetY = cLib_targetAngleY(&current.pos, &dComIfGp_getPlayer(0)->current.pos);
        s16 diff = abs((s16)(targetY - current.angle.y));
        if (dist < l_HIO.mChild[mType].mPrm.m48 && diff < l_HIO.mChild[mType].mPrm.m4C) {
            setStt(4);
        }
    }
    return 1;
}

/* 000050CC-0000514C       .text fall01__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::fall01() {
    m8B0 = 0;
    m849 = 0;
    if (m8A8 == 0) {
        setStt(5);
        return 1;
    }
    if (m8A8 == 1) {
        if ((s32)speedF == 0) {
            speedF = 0.0f;
            m8A8 = 2;
        }
    }
    return 1;
}

/* 0000514C-00005510       .text talk_1__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::talk_1() {
    u8 parts = chk_partsNotMove();
    talk(1);
    if (mpCurrMsg != NULL) {
        switch (mpCurrMsg->mStatus) {
        case fopMsgStts_BOX_OPENING_e:
        case fopMsgStts_MSG_TYPING_e:
            break;
        case fopMsgStts_MSG_DESTROYED_e:
            switch (mCurrMsgNo) {
            case 0x140F:
                dComIfGs_onEventBit(0x604);
                break;
            case 0x1410:
                dComIfGs_onEventBit(0x904);
                break;
            case 0x1413:
                dComIfGs_onEventBit(0xC80);
                break;
            case 0x13F3:
                dComIfGs_onEventBit(0x920);
                break;
            case 0x13F9:
                dComIfGs_onEventBit(0xC20);
                break;
            case 0x13FE:
                dComIfGs_onEventBit(0xC08);
                break;
            case 0x13F6:
                dComIfGs_onEventBit(0xC04);
                break;
            case 0x1404:
                dComIfGs_onEventBit(0xC02);
                break;
            case 0x1408:
                dComIfGs_onEventBit(0xD40);
                break;
            case 0x141D:
                dComIfGs_onEventBit(0xD08);
                break;
            case 0x1426:
                m840 = 1;
                break;
            case 0x1429:
                dComIfGp_setItemBeastNumCount(dBeastIdx_BOKOBABA_SEED_e, -4);
                m8B0 = 3;
                break;
            case 0x142B:
                m8B0 = 4;
                break;
            case 0x142F:
                m841 = 1;
                break;
            case 0x1432:
                dComIfGs_onEventBit(0x1C80);
                break;
            case 0x140C:
                dComIfGs_onEventBit(0xD10);
                break;
            case 0x1409:
                dComIfGs_onEventBit(0xD20);
                break;
            case 0x13EF:
                dComIfGs_onEventBit(0xC10);
                break;
            case 0x1493:
            case 0x149D:
                dComIfGs_onEventBit(0x2E10);
                dComIfGs_onEventBit(0x1D40);
                setReg(0x97FF);
                break;
            case 0x1483:
                setReg(0x96FF);
                break;
            case 0x1485:
                setReg(DEMO_SELECT(0x98FF, 0x99FF));
                break;
            case 0x1489:
                setReg(DEMO_SELECT(0x99FF, 0x98FF));
                break;
            }
            mPresentItem = 0xFF;
            mTalking = 0;
            setStt(mPreviousState);
            m832 = 0x3C;
            endEvent();
            break;
        }
    }
    return parts;
}

/* 00005510-000055D8       .text walk_1__11daNpc_Bj1_cFv */
int daNpc_Bj1_c::walk_1() {
    m84A = 0;
    if (m8AA == 0) {
        m7AC = current.pos;
        m7B8 = current.angle;
        m80C = 0.0f;
        m814 = 0.0f;
        speedF = m80C;
        setStt(7);
        return 1;
    }
    m7D8 = mPathRun.getPoint(mPathRun.getIdx());
    mNoTurn = true;
    return 1;
}

/* 000055D8-00005694       .text wait_action1__11daNpc_Bj1_cFPv */
int daNpc_Bj1_c::wait_action1(void*) {
    switch (mActionState) {
    case 0:
        setStt(1);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m854 = chkAttention();
        switch (mState) {
        case 1:
            m850 = wait_1();
            break;
        case 2:
            m850 = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00005694-0000577C       .text wait_action2__11daNpc_Bj1_cFPv */
int daNpc_Bj1_c::wait_action2(void*) {
    switch (mActionState) {
    case 0:
        setStt(5);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m854 = chkAttention();
        switch (mState) {
        case 5:
            m850 = wait_2();
            break;
        case 3:
            m850 = flyMov();
            break;
        case 4:
            m850 = fall01();
            break;
        case 2:
            m850 = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 0000577C-00005860       .text wait_action3__11daNpc_Bj1_cFPv */
int daNpc_Bj1_c::wait_action3(void*) {
    switch (mActionState) {
    case 0:
        set_pthPoint(0);
        setStt(6);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m854 = chkAttention();
        switch (mState) {
        case 6:
            m850 = walk_1();
            break;
        case 7:
            m850 = wait_3();
            break;
        case 2:
            m850 = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00005860-00005928       .text wait_action4__11daNpc_Bj1_cFPv */
int daNpc_Bj1_c::wait_action4(void*) {
    switch (mActionState) {
    case 0:
        set_pthPoint(0);
        setStt(8);
        mActionState++;
        break;
    case 1:
    case 2:
    case 3:
        m854 = chkAttention();
        switch (mState) {
        case 8:
            m850 = wait_4();
            break;
        case 2:
            m850 = talk_1();
            break;
        }
        lookBack();
        break;
    case 9:
        break;
    }
    return 1;
}

/* 00005928-000059C4       .text demo__11daNpc_Bj1_cFv */
u8 daNpc_Bj1_c::demo() {
    if (demoActorID == 0) {
        if (m857 != 0) {
            m857 = 0;
        }
    } else {
        m857 = 1;
        dComIfGp_demo_getActor(demoActorID);
        dDemo_setDemoData(this, 0x6A, mpMorf, "Bj");
    }
    return m857;
}

/* 000059C4-00005AC4       .text shadowDraw__11daNpc_Bj1_cFv */
void daNpc_Bj1_c::shadowDraw() {
    cXyz pos(current.pos.x, 150.0f + current.pos.y, current.pos.z);
    mShadowId = dComIfGd_setShadow(
        mShadowId, 1, mpMorf->getModel(), &pos, 800.0f, 40.0f, current.pos.y, mObjAcch.GetGroundH(),
        mObjAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex()
    );
    if (mShadowId != 0) {
        if (mpPlantLModel != NULL) {
            dComIfGd_addRealShadow(mShadowId, mpPlantLModel);
        }
        if (mpPlantRModel != NULL) {
            dComIfGd_addRealShadow(mShadowId, mpPlantRModel);
        }
        if (m84C != 0) {
            dComIfGd_addRealShadow(mShadowId, mpPrpMorf->getModel());
        }
    }
}

/* 00005AC4-00005E38       .text _draw__11daNpc_Bj1_cFv */
BOOL daNpc_Bj1_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    if (mHidden != 0 || mNoDraw != 0) {
        return TRUE;
    }
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(model, &tevStr);
    switch (mType) {
    case 1:
    case 3:
    case 8:
        mpMorf->entryDL((J3DMaterialTable*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BMT_BJ2_e));
        break;
    case 2:
    case 6:
    case 7:
        mpMorf->entryDL((J3DMaterialTable*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BMT_BJ3_e));
        break;
    case 0:
    case 4:
    case 5:
    default:
        mpMorf->entryDL();
        break;
    }
    if (mpPlantLModel != NULL) {
        g_env_light.setLightTevColorType(mpPlantLModel, &tevStr);
        mDoExt_modelEntryDL(mpPlantLModel);
    }
    if (mpPlantRModel != NULL) {
        g_env_light.setLightTevColorType(mpPlantRModel, &tevStr);
        mDoExt_modelEntryDL(mpPlantRModel);
    }
    g_env_light.setLightTevColorType(mpFaceModel, &tevStr);
    mDoExt_modelEntryDL(mpFaceModel);
    if (m84C != 0) {
        g_env_light.setLightTevColorType(mpPrpMorf->getModel(), &tevStr);
        mpPrpMorf->entryDL();
    }
    shadowDraw();
    switch (mType) {
    case 0:
        dSnap_RegistFig(DSNAP_TYPE_UNK9D, this, 1.0f, 1.0f, 1.0f);
        break;
    case 1:
        dSnap_RegistFig(DSNAP_TYPE_UNK9E, this, 1.0f, 1.0f, 1.0f);
        break;
    case 2:
        dSnap_RegistFig(DSNAP_TYPE_UNK9F, this, 1.0f, 1.0f, 1.0f);
        break;
    case 3:
        dSnap_RegistFig(DSNAP_TYPE_UNKA0, this, 1.0f, 1.0f, 1.0f);
        break;
    case 4:
        dSnap_RegistFig(DSNAP_TYPE_UNKA1, this, 1.0f, 1.0f, 1.0f);
        break;
    case 5:
        dSnap_RegistFig(DSNAP_TYPE_UNKA2, this, 1.0f, 1.0f, 1.0f);
        break;
    case 6:
        dSnap_RegistFig(DSNAP_TYPE_UNKA3, this, 1.0f, 1.0f, 1.0f);
        break;
    case 7:
        dSnap_RegistFig(DSNAP_TYPE_UNKA4, this, 1.0f, 1.0f, 1.0f);
        break;
    case 8:
        dSnap_RegistFig(DSNAP_TYPE_UNKA5, this, 1.0f, 1.0f, 1.0f);
        break;
    }
    if (l_HIO.mChild[mType].mPrm.m18 != 0) {
        GXColor unused[] = {
            {0xFF, 0x00, 0x00, 0x80},
            {0x00, 0x00, 0xFF, 0x80},
            {0x00, 0xFF, 0x00, 0x80},
            {0x00, 0xFF, 0x00, 0x80},
            {0xFF, 0xFF, 0x00, 0x80},
        }; // Unused colors, needed for the .rodata section to match.

        cXyz pos;
        if (mSubType == 6) {
            pos = current.pos;
            pos.y = mObjAcch.GetGroundH();
        }
        pos = current.pos;
        pos.y = eyePos.y;
    }
    return TRUE;
}

/* 00005E38-00006128       .text _execute__11daNpc_Bj1_cFv */
BOOL daNpc_Bj1_c::_execute() {
    if (mInitialized == 0) {
        m7AC = current.pos;
        m7B8 = current.angle;
        mInitialized = 1;
    }
    m_jnt.setParam(
        l_HIO.mChild[mType].mPrm.mMaxBackBoneX,
        l_HIO.mChild[mType].mPrm.mMaxBackBoneY,
        l_HIO.mChild[mType].mPrm.mMinBackBoneX,
        l_HIO.mChild[mType].mPrm.mMinBackBoneY,
        l_HIO.mChild[mType].mPrm.mMaxHeadX,
        l_HIO.mChild[mType].mPrm.mMaxHeadY,
        l_HIO.mChild[mType].mPrm.mMinHeadX,
        l_HIO.mChild[mType].mPrm.mMinHeadY,
        l_HIO.mChild[mType].mPrm.mMaxTurnStep
    );
    if (mHidden != 0 && demoActorID == 0) {
        return TRUE;
    }
    mStopMove = 0;
    mHidden = 0;
    partner_srch();
    checkOrder();
    if (demo() == 0) {
        int staff = -1;
        if (dComIfGp_event_runCheck() && eventInfo.checkCommandTalk() == 0) {
            staff = isEventEntry();
        }
        if (staff >= 0) {
            event_proc(staff);
        } else {
            (this->*mAction)(NULL);
        }
        if (!bj_flyMove()) {
            bj_nMove();
        }
        if (mStopMove == 0) {
            fopAcM_posMoveF(this, mStts.GetCCMoveP());
        }
        if (!mKeepShapeAngle) {
            shape_angle = current.angle;
        }
    }
    eventOrder();
    if (m84C != 0) {
        mPrpRotY += mPrpRotSpd;
        f32 vol = 0.0f;
        s16 max = l_HIO.mChild[mType].mPrm.m40;
        if (max != 0) {
            vol = 100.0f * ((f32)mPrpRotSpd / (f32)max);
            vol = vol > 100.0f ? 100.0f : vol;
            vol = vol < 0.0f ? 0.0f : vol;
        }
        mDoAud_seStart(JA_SE_CM_PRAPELLO_ROLLING, &current.pos, (u32)vol, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
    setMtx(false);
    if (m857 == 0) {
        setCollision_SP_();
    }
    return TRUE;
}

/* 00006128-000061A4       .text _delete__11daNpc_Bj1_cFv */
BOOL daNpc_Bj1_c::_delete() {
    dComIfG_resDelete(&mPhs, "Bj");
#if VERSION > VERSION_DEMO
    if (heap != NULL)
#endif
    {
        if (mpMorf != NULL) {
            mpMorf->stopZelAnime();
        }
        if (mpPrpMorf != NULL) {
            mpPrpMorf->stopZelAnime();
        }
    }
    delPrtcl_drugPot();
    delPrtcl_danceLR();
#if VERSION == VERSION_DEMO
    l_HIO.removeHIO();
#endif
    return TRUE;
}

/* 000061A4-000061C4       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* i_this) {
    return ((daNpc_Bj1_c*)i_this)->CreateHeap();
}

/* 000061C4-000062E4       .text _create__11daNpc_Bj1_cFv */
cPhs_State daNpc_Bj1_c::_create() {
    fopAcM_ct_Retail(this, daNpc_Bj1_c);
    static u32 a_size_tbl[] = {
        0x272E0, 0x272E0, 0x272E0, 0x272E0, 0x272E0, 0x272E0, 0x272E0, 0x272E0, 0x272E0,
    };
    cPhs_State phase = dComIfG_resLoad(&mPhs, "Bj");
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }
    u32 prm = fopAcM_GetParam(this) & 0xFF;
    if (!charDecide(prm)) {
        return cPhs_ERROR_e;
    }
#if VERSION == VERSION_DEMO
    l_HIO.entryHIO("コログ族");
    fopAcM_ct(this, daNpc_Bj1_c);
#endif
    if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, a_size_tbl[mType])) {
        return cPhs_ERROR_e;
    }
    fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -50.0f, -20.0f, -50.0f, 50.0f, 160.0f, 50.0f);
    if (!createInit()) {
        return cPhs_ERROR_e;
    }
    return phase;
}

daNpc_Bj1_c::daNpc_Bj1_c() {
}

/* 00006734-00006A00       .text create_Anm__11daNpc_Bj1_cFv */
J3DModelData* daNpc_Bj1_c::create_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BDL_BJ_e);
    JUT_ASSERT(0x10CA, a_mdl_dat != 0);
    mpMorf = new mDoExt_McaMorf(
        a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BCK_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x15021222);
    if (mpMorf == NULL) {
        return NULL;
    }
    if (mpMorf->getModel() == NULL) {
        mpMorf = NULL;
        return NULL;
    }
    m_hed_jnt_num = a_mdl_dat->getJointName()->getIndex("head");
    JUT_ASSERT(0x10DE, m_hed_jnt_num >= 0);
    m_bbone_jnt_num = a_mdl_dat->getJointName()->getIndex("backbone");
    JUT_ASSERT(0x10E1, m_bbone_jnt_num >= 0);
    m_armL2_jnt_num = a_mdl_dat->getJointName()->getIndex("armL2");
    JUT_ASSERT(0x10E4, m_armL2_jnt_num >= 0);
    m_armR2_jnt_num = a_mdl_dat->getJointName()->getIndex("armR2");
    JUT_ASSERT(0x10E7, m_armR2_jnt_num >= 0);
    return a_mdl_dat;
}

/* 00006A00-00006C14       .text create_prp_Anm__11daNpc_Bj1_cFv */
J3DModelData* daNpc_Bj1_c::create_prp_Anm() {
    J3DModelData* a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BDL_PP_e);
    JUT_ASSERT(0x10F6, a_mdl_dat != 0);
    mpPrpMorf = new mDoExt_McaMorf(
        a_mdl_dat, NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectIDRes("Bj", dRes_ID_BJ_BCK_OPEN_e),
        J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000022);
    if (mpPrpMorf == NULL) {
        return NULL;
    }
    if (mpPrpMorf->getModel() == NULL) {
        mpPrpMorf = NULL;
        return NULL;
    }
    m_cen_jnt_num = a_mdl_dat->getJointName()->getIndex("center");
    JUT_ASSERT(0x110A, m_cen_jnt_num >= 0);
    m_stalk_jnt_num = a_mdl_dat->getJointName()->getIndex("stalk");
    JUT_ASSERT(0x110D, m_stalk_jnt_num >= 0);
    return a_mdl_dat;
}

/* 00006C14-00006F54       .text create_itm_Mdl__11daNpc_Bj1_cFv */
bool daNpc_Bj1_c::create_itm_Mdl() {
    static int l_arm_L_bmd_tbl[] = {
        -1, -1, -1, -1, -1, -1, dRes_ID_BJ_BDL_PL_AL_e, -1, -1,
    };
    static int l_arm_R_bmd_tbl[] = {
        -1, -1, -1, -1, -1, -1, dRes_ID_BJ_BDL_PL_AR_e, -1, -1,
    };
    static int l_bmd_tbl[] = {
        dRes_ID_BJ_BDL_BJ1_FACE_e,
        dRes_ID_BJ_BDL_BJ2_FACE_e,
        dRes_ID_BJ_BDL_BJ3_FACE_e,
        dRes_ID_BJ_BDL_BJ4_FACE_e,
        dRes_ID_BJ_BDL_BJ5_FACE_e,
        dRes_ID_BJ_BDL_BJ6_FACE_e,
        dRes_ID_BJ_BDL_BJ7_FACE_e,
        dRes_ID_BJ_BDL_BJ8_FACE_e,
        dRes_ID_BJ_BDL_BJ9_FACE_e,
    };

    mpPlantLModel = NULL;
    mpPlantRModel = NULL;
    mpFaceModel = NULL;

    bool result;
    J3DModelData* a_mdl_dat;
    if (l_arm_L_bmd_tbl[mType] >= 0) {
        a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Bj", l_arm_L_bmd_tbl[mType]);
        JUT_ASSERT(0x1145, a_mdl_dat != 0);
        mpPlantLModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
        result = mpPlantLModel != NULL;
        if (!result) {
            return result;
        }
        if (l_arm_L_bmd_tbl[mType] == dRes_ID_BJ_BDL_PL_AL_e) {
            m_plant_l_jnt_num = a_mdl_dat->getJointName()->getIndex("plant_al");
            JUT_ASSERT(0x114F, m_plant_l_jnt_num >= 0);
        }
    }

    if (l_arm_R_bmd_tbl[mType] >= 0) {
        a_mdl_dat = (J3DModelData*)dComIfG_getObjectIDRes("Bj", l_arm_R_bmd_tbl[mType]);
        JUT_ASSERT(0x1156, a_mdl_dat != 0);
        mpPlantRModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
        result = mpPlantRModel != NULL;
        if (!result) {
            return result;
        }
        if (l_arm_R_bmd_tbl[mType] == dRes_ID_BJ_BDL_PL_AR_e) {
            m_plant_r_jnt_num = a_mdl_dat->getJointName()->getIndex("plant_ar");
            JUT_ASSERT(0x1160, m_plant_r_jnt_num >= 0);
        }
    }

    a_mdl_dat = static_cast<J3DModelData*>(dComIfG_getObjectIDRes("Bj", l_bmd_tbl[mType]));
    JUT_ASSERT(0x1166, a_mdl_dat != 0);
    mpFaceModel = mDoExt_J3DModel__create(a_mdl_dat, 0x80000, 0x11000022);
    return mpFaceModel != NULL;
}

/* 00006F54-00007114       .text CreateHeap__11daNpc_Bj1_cFv */
BOOL daNpc_Bj1_c::CreateHeap() {
    J3DModelData* a_mdl_dat = create_Anm();
    if (a_mdl_dat == NULL) {
        return FALSE;
    }
    J3DModelData* a_prp_dat = create_prp_Anm();
    if (a_prp_dat == NULL) {
        mpMorf = NULL;
        return FALSE;
    }
    if (create_itm_Mdl()) {
        for (u16 i = 0; i < a_prp_dat->getJointNum(); i++) {
            if (i == m_cen_jnt_num || i == m_stalk_jnt_num) {
                mpPrpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Prp);
            }
        }
        mpPrpMorf->getModel()->setUserArea((u32)this);

        for (u16 i = 0; i < a_mdl_dat->getJointNum(); i++) {
            if (i == m_hed_jnt_num || i == m_bbone_jnt_num || i == m_armR2_jnt_num) {
                mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Bj1);
            }
        }
        mpMorf->getModel()->setUserArea((u32)this);

        mAcchCir.SetWall(30.0f, 40.0f);
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this));
        return TRUE;
    }
    mpMorf = NULL;
    mpPrpMorf = NULL;
    return FALSE;
}

inline daNpc_Bj1_HIO_c::~daNpc_Bj1_HIO_c() {
}

/* 00007114-00007134       .text daNpc_Bj1_Create__FP10fopAc_ac_c */
static cPhs_State daNpc_Bj1_Create(fopAc_ac_c* i_this) {
    return ((daNpc_Bj1_c*)i_this)->_create();
}

/* 00007134-00007154       .text daNpc_Bj1_Delete__FP11daNpc_Bj1_c */
static BOOL daNpc_Bj1_Delete(daNpc_Bj1_c* i_this) {
    return ((daNpc_Bj1_c*)i_this)->_delete();
}

/* 00007154-00007174       .text daNpc_Bj1_Execute__FP11daNpc_Bj1_c */
static BOOL daNpc_Bj1_Execute(daNpc_Bj1_c* i_this) {
    return ((daNpc_Bj1_c*)i_this)->_execute();
}

/* 00007174-00007194       .text daNpc_Bj1_Draw__FP11daNpc_Bj1_c */
static BOOL daNpc_Bj1_Draw(daNpc_Bj1_c* i_this) {
    return ((daNpc_Bj1_c*)i_this)->_draw();
}

/* 00007194-0000719C       .text daNpc_Bj1_IsDelete__FP11daNpc_Bj1_c */
static BOOL daNpc_Bj1_IsDelete(daNpc_Bj1_c*) {
    return TRUE;
}

static actor_method_class l_daNpc_Bj1_Method = {
    (process_method_func)daNpc_Bj1_Create,
    (process_method_func)daNpc_Bj1_Delete,
    (process_method_func)daNpc_Bj1_Execute,
    (process_method_func)daNpc_Bj1_IsDelete,
    (process_method_func)daNpc_Bj1_Draw,
};

actor_process_profile_definition g_profile_NPC_BJ1 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ1_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ1_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ2 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ2_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ2_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ3 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ3_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ3_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ4 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ4_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ4_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ5 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ5_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ5_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ6 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ6_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ6_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ7 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ7_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ7_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ8 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ8_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ8_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

actor_process_profile_definition g_profile_NPC_BJ9 = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_NPC_BJ9_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daNpc_Bj1_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_NPC_BJ9_e,
    /* Actor SubMtd */ &l_daNpc_Bj1_Method,
    /* Status       */ 0x07 | fopAcStts_SHOWMAP_e | fopAcStts_NOCULLEXEC_e | fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_NPC_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
