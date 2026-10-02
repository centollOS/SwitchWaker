/**
 * d_a_bigelf.cpp
 * NPC - Great Fairy
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_bigelf.h"
#include "d/actor/d_a_npc_fa1.h"
#include "d/actor/d_a_ship.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_particle.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "JSystem/J3DGraphAnimator/J3DJoint.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DNode.h"
#include "JSystem/J3DGraphAnimator/J3DSkinDeform.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "res/Object/bigelf.h"

static fpc_ProcID l_msgId;
static msg_class* l_msg;

static const int l_bck_ix_tbl[] = {
    dRes_INDEX_BIGELF_BCK_WAIT01_e,
    dRes_INDEX_BIGELF_BCK_ENTER_e,
    dRes_INDEX_BIGELF_BCK_FL_DEMO_e,
    dRes_INDEX_BIGELF_BCK_ENTER_e,
};

static const u16 pa_name_flower[] = {
    dPa_name::ID_AK_SN_BIGFAIRYTYPE1FLOWER00,
    dPa_name::ID_AK_SN_BIGFAIRYTYPE2FLOWER00,
    dPa_name::ID_AK_SN_BIGFAIRYTYPE3FLOWER00,
    dPa_name::ID_AK_SN_BIGFAIRYTYPE4FLOWER00,
};

static const u16 pa_name_flower2[] = {
    dPa_name::ID_AK_SN_BIGFAIRYTYPE1FLOWER01,
    dPa_name::ID_AK_SN_BIGFAIRYTYPE2FLOWER01,
    dPa_name::ID_AK_SN_BIGFAIRYTYPE3FLOWER01,
    dPa_name::ID_AK_SN_BIGFAIRYTYPE4FLOWER01,
};

/* 00000078-0000016C       .text oct_delete__10daBigelf_cFv */
void daBigelf_c::oct_delete() {
    fopAc_ac_c* octa = fopAcM_SearchByID(mOctId);
    daShip_c* ship = dComIfGp_getShipActor();
    if (chkFlag(0x40)) {
        if (mOctTimer > 0) {
            mOctTimer--;
        } else {
            if (octa != NULL) {
                if (ship != NULL) {
                    cXyz offset(0.0f, 0.0f, 1570.0f);
                    cXyz pos;
                    fpoAcM_absolutePos(octa, &offset, &pos);
                    pos.y = ship->current.pos.y;
                    s16 angle = cLib_targetAngleY(&current.pos, &pos);
                    angle += 0x4000;
                    ship->initStartPos(&pos, angle);
                }
                fopAcM_delete(octa);
            }
            clrFlag(0x40);
        }
    }
}

/* 0000016C-00000338       .text nodeCallBack__10daBigelf_cFP7J3DNode */
BOOL daBigelf_c::nodeCallBack(J3DNode* node) {
    J3DJoint* joint = (J3DJoint*)node;
    J3DModel* model = j3dSys.getModel();
    s32 jntNo = joint->getJntNo();
    MTXCopy(model->getAnmMtx(jntNo), *calc_mtx);
    cXyz offset;
    cXyz pos;
    if (jntNo == getHeadJntNum()) {
        s16 target;
        if (mAnm == 0) {
            target = getHead_x();
        } else {
            target = 0;
        }
        cLib_addCalcAngleS(&mHeadZAngle, target, 8, 0x400, 0x100);
        cMtx_ZrotM(*calc_mtx, -mHeadZAngle);

        offset.set(0.0f, 0.0f, 0.0f);
        MtxPosition(&offset, &pos);
        setAttentionBasePos(pos);

        offset.set(20.0f, -20.0f, 0.0f);
        MtxPosition(&offset, &pos);
        setEyePos(pos);
        incAttnSetCount();
    } else if (jntNo != getBackboneJntNum()) {
        if (jntNo == m_fl_jnt) {
            offset.set(0.0f, 0.0f, 0.0f);
            MtxPosition(&offset, &mHandPos);
        }
    }
    MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
    model->setAnmMtx(jntNo, *calc_mtx);
    return TRUE;
}

/* 00000338-00000384       .text nodeCallBack_Bigelf__FP7J3DNodei */
static BOOL nodeCallBack_Bigelf(J3DNode* node, int calcTiming) {
    if (calcTiming == J3DNodeCBCalcTiming_In) {
        J3DModel* model = j3dSys.getModel();
        daBigelf_c* i_this = (daBigelf_c*)model->getUserArea();
        if (i_this != NULL) {
            i_this->nodeCallBack(node);
        }
    }
    return TRUE;
}

/* 00000384-00000438       .text lightInit__10daBigelf_cFP4cXyz */
void daBigelf_c::lightInit(cXyz* pos) {
    mLight.mPos = *pos;
    mLightPos2 = mLight.mPos;
    if (!mIsLightShining) {
        mIsLightShining = true;
        mLight.mColor.r = 0xFF;
        mLight.mColor.g = 0xFF;
        mLight.mColor.b = 0xFF;
        mLight.mPower = 0.0f;
        mLight.mFluctuation = 0.0f;
        mLightPos2 = mLight.mPos;
        mLightColor2.r = 0xFF;
        mLightColor2.g = 0xFF;
        mLightColor2.b = 0xFF;
        mLightPower2 = 0.0f;
        mLightFluctuation2 = 0.0f;
        dKy_plight_priority_set(&mLight);
    }
}

/* 00000438-00000470       .text lightEnd__10daBigelf_cFv */
void daBigelf_c::lightEnd() {
    if (mIsLightShining) {
        mIsLightShining = false;
        dKy_plight_cut(&mLight);
    }
}

/* 00000470-0000047C       .text lightProc__10daBigelf_cFv */
void daBigelf_c::lightProc() {
    if (mIsLightShining) {
        return;
    }
}

/* 0000047C-00000488       .text darkInit__10daBigelf_cFv */
void daBigelf_c::darkInit() {
    mIsDark = true;
}

/* 00000488-000004D0       .text darkEnd__10daBigelf_cFv */
void daBigelf_c::darkEnd() {
    mIsDark = false;
    dKy_set_actcol_ratio(1.0f);
    dKy_set_bgcol_ratio(1.0f);
    dKy_set_vrboxcol_ratio(1.0f);
}

/* 000004D0-00000574       .text darkProc__10daBigelf_cFv */
void daBigelf_c::darkProc() {
    if (mIsDark) {
        cLib_addCalc2(&mDarkRatio, mDarkTarget, 0.1f, 1.0f);
        dKy_set_actcol_ratio(0.3f + 0.7f * mDarkRatio);
        f32 bg = 0.4f + 0.6f * mDarkRatio;
        dKy_set_bgcol_ratio(bg);
        dKy_set_vrboxcol_ratio(bg);
    }
}

/* 00000574-00000588       .text demoInitFlDelete__10daBigelf_cFv */
void daBigelf_c::demoInitFlDelete() {
    mTimer = 0;
    mAlpha = 0xFF;
}

/* 00000588-00000708       .text demoProcFlDelete__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFlDelete() {
    mTimer++;
    if (mTimer == 0x1B) {
        fopAc_ac_c* player = dComIfGp_getLinkPlayer();
        dComIfGp_particle_set(pa_name_flower2[mPaNo], &player->current.pos);
        mDarkTarget = 1.0f;
    }
    if (mTimer >= 0x1B) {
        if (mpEmitter3E4 != NULL) {
            if (mAlpha > 10) {
                mAlpha -= 10;
                mpEmitter3E4->setGlobalAlpha(mAlpha);
            } else {
                mAlpha = 0;
                mpEmitter3E4->setGlobalAlpha(0);
                mpEmitter3E4->becomeInvalidEmitter();
                mpEmitter3E4 = NULL;
            }
        } else {
            lightEnd();
            dComIfGp_evmng_cutEnd(mStaffId);
            if (chkFlag(0x1)) {
                clrFlag(0x1);
                if (getType() == 6) {
                    dComIfGp_setItemMaxMagicCount(0x20);
                    dComIfGp_setItemMagicCount(0x20);
                }
            }
        }
    }
    return TRUE;
}

/* 00000708-000007E4       .text demoInitFlLink__10daBigelf_cFv */
void daBigelf_c::demoInitFlLink() {
    fopAc_ac_c* player = dComIfGp_getLinkPlayer();
    cXyz offset(0.0f, 300.0f, 400.0f);
    cXyz pos;
    fpoAcM_absolutePos(player, &offset, &pos);
    mpEmitter3E4 = dComIfGp_particle_set(pa_name_flower[mPaNo], &pos, &shape_angle);
    lightInit(&pos);
    mLight.mPower = 1000.0f;
    demoInitWait();
}

/* 000007E4-00000808       .text demoProcFlLink__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFlLink() {
    demoProcWait();
    return TRUE;
}

/* 00000808-000008F0       .text demoInitFlDmAf__10daBigelf_cFv */
void daBigelf_c::demoInitFlDmAf() {
    cXyz pos = mHandPos;
    pos.y += 20.0f;
    mpEmitter3E0 = dComIfGp_particle_set(pa_name_flower[mPaNo], &pos, &shape_angle);
    mDoAud_seStart(JA_SE_OBJ_DY_HANAFUBUKI, NULL);
}

/* 000008F0-000009C0       .text demoProcFlDmAf__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFlDmAf() {
    if (mAnmEnd || !chkFlag(0x1)) {
        dComIfGp_evmng_cutEnd(mStaffId);
        clrFlag(0x1);
        if (mpEmitter3E0 != NULL) {
            mpEmitter3E0->becomeInvalidEmitter();
            mpEmitter3E0 = NULL;
        }
        return TRUE;
    }
    f32 frame = mpMorf->getFrame();
    if (frame >= 116.0f && frame <= 173.0f) {
        setFlag(0x8);
    }
    return TRUE;
}

/* 000009C0-00000A20       .text demoInitFlDmMd__10daBigelf_cFv */
void daBigelf_c::demoInitFlDmMd() {
    darkInit();
    mDarkRatio = 1.0f;
    mDarkTarget = 1.0f;
    clrFlag(0x80);
    setFlag(0x200);
    setFlag(0x400);
}

/* 00000A20-00000CEC       .text demoProcFlDmMd__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFlDmMd() {
    f32 frame = mpMorf->getFrame();
    if (!chkFlag(0x1)) {
        dComIfGp_evmng_cutEnd(mStaffId);
        return TRUE;
    }
    if (!(frame < 16.0f)) {
        if (frame < 116.0f) {
            mDarkTarget = 0.01f * (116.0f - frame);
        } else {
            mDarkTarget = 0.0f;
        }
    }
    if (!chkFlag(0x80)) {
        if (frame >= 116.0f) {
            setFlag(0x80);
            cXyz pos = mHandPos;
            pos.x += 60.0f * cM_ssin(shape_angle.y);
            pos.z += 60.0f * cM_scos(shape_angle.y);
            lightInit(&pos);
            mLight.mPower = 850.0f;
            fopAcM_seStart(this, JA_SE_OBJ_DY_FLOWER, 0);
        }
    }
    if (chkFlag(0x200) && frame >= 126.0f) {
        clrFlag(0x200);
        fopAcM_seStart(this, JA_SE_CV_DY_BREATH_IN, 0);
    }
    if (chkFlag(0x400) && frame >= 154.0f) {
        clrFlag(0x400);
        fopAcM_seStart(this, JA_SE_CV_DY_BREATH_OUT, 0);
    }
    if (frame >= 166.0f) {
        dComIfGp_evmng_cutEnd(mStaffId);
        clrFlag(0x1);
    }
    if (frame >= 116.0f && frame <= 173.0f) {
        setFlag(0x8);
    }
    return TRUE;
}

/* 00000CEC-00000CF0       .text demoInitFlDmBf__10daBigelf_cFv */
void daBigelf_c::demoInitFlDmBf() {
}

/* 00000CF0-00000D7C       .text demoProcFlDmBf__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFlDmBf() {
    if (!chkFlag(0x1)) {
        dComIfGp_evmng_cutEnd(mStaffId);
        return TRUE;
    }
    if (mAnmEnd) {
        setAnm(2);
        dComIfGp_evmng_cutEnd(mStaffId);
        clrFlag(0x1);
    }
    return TRUE;
}

/* 00000D7C-00000D88       .text demoInitFlDemo__10daBigelf_cFv */
void daBigelf_c::demoInitFlDemo() {
    mAlpha = 0;
}

/* 00000D88-00000FB8       .text demoProcFlDemo__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFlDemo() {
    if (mAnmEnd) {
        if (chkFlag(0x1)) {
            switch (mAnm) {
            case 0:
                setAnm(2);
                break;
            case 2:
                clrFlag(0x1);
                dComIfGp_evmng_cutEnd(mStaffId);
                break;
            }
        } else {
            dComIfGp_evmng_cutEnd(mStaffId);
        }
    }
    if (mAnm == 2) {
        f32 frame = mpMorf->getFrame();
        if (frame >= 116.0f && frame <= 173.0f) {
            setFlag(0x8);
        }
        switch (mAlpha) {
        case 0: {
            cXyz pos = mHandPos;
            pos.y += 20.0f;
            if (frame >= 167.0f) {
                mAlpha++;
                mpEmitter3E0 = dComIfGp_particle_set(pa_name_flower[mPaNo], &pos, &shape_angle);
                mDoAud_seStart(JA_SE_OBJ_DY_HANAFUBUKI, NULL);
            }
            break;
        }
        case 1:
            if (!chkFlag(0x1) && mpEmitter3E0 != NULL) {
                mAlpha++;
                mpEmitter3E0->becomeInvalidEmitter();
                mpEmitter3E0 = NULL;
            }
            break;
        }
    }
    return TRUE;
}

/* 00000FB8-0000115C       .text demoInitExit__10daBigelf_cFv */
void daBigelf_c::demoInitExit() {
    if (mpEmitter3CC != NULL) {
        mpEmitter3CC->becomeInvalidEmitter();
        mpEmitter3CC = NULL;
    }
    setFlag(0x10);
    mTimer = 0;
    mHeightOffset = 250.0f;
    m3EC = 1.0f;
    m3F0 = 1.0f;
    cXyz pos;
    cXyz scale(1.0f, 1.0f, 1.0f);
    pos = current.pos;
    pos.y += mHeightOffset;
    dComIfGp_particle_set(dPa_name::ID_AK_JN_ENEMYFLASH00, &pos, NULL, &scale);
    tevStr.mFogColor.b = 0xFF;
    tevStr.mFogColor.g = 0xFF;
    tevStr.mFogColor.r = 0xFF;
    tevStr.mFogStartZ = 0.0f;
    tevStr.mFogEndZ = 2000.0f;
    fopAcM_seStart(this, JA_SE_CM_DY_GO_AWAY, 0);
    fopAcM_seStart(this, JA_SE_CV_DY_GO_AWAY, 0);
    darkEnd();
}

/* 0000115C-000012D4       .text demoProcExit__10daBigelf_cFv */
BOOL daBigelf_c::demoProcExit() {
    mTimer++;
    cLib_addCalc2(&tevStr.mFogEndZ, 10.0f, 1.0f, 80.0f);
    if (mTimer < 0x46) {
        cLib_addCalc0(&m3EC, 0.1f, 0.01f);
        cLib_addCalc0(&m3F0, 0.1f, 0.01f);
        return TRUE;
    }
    if (mTimer == 0x46) {
        fopAcM_seStart(this, JA_SE_CM_L_ARROW_PASS_AWAY, 0);
        if (getType() == 6) {
            mDoAud_bgmStop(0x2D);
        }
    }
    cLib_addCalc2(&m3F0, 5.0f, 0.1f, 1.0f);
    cLib_addCalc0(&m3EC, 0.1f, 0.05f);
    if (mTimer >= 0x5A) {
        dComIfGp_evmng_cutEnd(mStaffId);
        setFlag(0x2);
        clrFlag(0x10);
        if (getType() != 6) {
            makeFa1S();
        }
    }
    return TRUE;
}

/* 000012D4-000013C0       .text demoInitTalk__10daBigelf_cFv */
void daBigelf_c::demoInitTalk() {
    talkInit();
    int* a_intP = dComIfGp_evmng_getMyIntegerP(mStaffId, "MsgNo");
    JUT_ASSERT(0x325, a_intP);
    mCurrentMessageId = *a_intP;
    if (mCurrentMessageId == 0x2EEA) {
        dComIfGp_setItemLifeCount(dComIfGs_getMaxLife());
        dComIfGp_setItemMagicCount(dComIfGs_getMaxMagic());
    }
}

/* 000013C0-00001418       .text demoProcTalk__10daBigelf_cFv */
BOOL daBigelf_c::demoProcTalk() {
    u16 status = talk();
    if (status == fopMsgStts_BOX_CLOSED_e || status == 0xFE) {
        dComIfGp_evmng_cutEnd(mStaffId);
    }
    return TRUE;
}

/* 00001418-0000163C       .text demoInitAppear__10daBigelf_cFv */
void daBigelf_c::demoInitAppear() {
    static u16 p_name0[] = {
        dPa_name::ID_AK_SN_BIGFAIRYTYPE1APP00,
        dPa_name::ID_AK_SN_BIGFAIRYTYPE2APP00,
        dPa_name::ID_AK_SN_BIGFAIRYTYPE3APP00,
        dPa_name::ID_AK_SN_BIGFAIRYTYPE4APP00,
    };
    static u16 p_name1[] = {
        dPa_name::ID_AK_SN_BIGFAIRYTYPE1APP01,
        dPa_name::ID_AK_SN_BIGFAIRYTYPE2APP01,
        dPa_name::ID_AK_SN_BIGFAIRYTYPE3APP01,
        dPa_name::ID_AK_SN_BIGFAIRYTYPE4APP01,
    };

    fopAc_ac_c* fa1 = fopAcM_SearchByID(mFa1Id);
    if (fa1 != NULL) {
        dComIfGp_event_setTalkPartner(this);
        fopAcM_delete(fa1);
    }
    cXyz pos = current.pos;
    pos.y += 70.0f;
    dComIfGp_getVibration().StartShock(5, -0x21, cXyz(0.0f, 1.0f, 0.0f));
    dComIfGp_particle_set(p_name0[mPaNo], &pos, NULL, &scale);
    dComIfGp_particle_set(p_name1[mPaNo], &pos, NULL, &scale);
    if (getType() == 6) {
        mAttnTimer = 0xF;
    }
    mAlpha = 0xF;
    m3A8 = 0.5f;
    if (getType() == 6) {
        fopAcM_seStart(this, JA_SE_CM_DY_ENTER_DO, 0);
        mDoAud_bgmStart(0x80000053);
    }
    setFlag(0x100);
}

/* 0000163C-000017B4       .text demoProcAppear__10daBigelf_cFv */
BOOL daBigelf_c::demoProcAppear() {
    if (mAlpha != 0) {
        mAlpha--;
        if (mAlpha == 0) {
            clrFlag(0x2);
            setAnm(1);
            if (mpEmitter3CC == NULL) {
                mpEmitter3CC = dComIfGp_particle_set(dPa_name::ID_AK_SN_BIGFAIRYWAIT00, &current.pos);
            }
        }
        return TRUE;
    }
    if (chkFlag(0x100) && mpMorf->getFrame() >= 87.0f) {
        clrFlag(0x100);
        fopAcM_seStart(this, JA_SE_CV_DY_ENTER, 0);
    }
    if (mAnmEnd) {
        setAnm(0);
        dComIfGp_evmng_cutEnd(mStaffId);
    }
    scale.z = scale.y = scale.x;
    return TRUE;
}

/* 000017B4-00001848       .text demoInitFa1__10daBigelf_cFv */
void daBigelf_c::demoInitFa1() {
    daNpc_Fa1_c* fa1 = (daNpc_Fa1_c*)fopAcM_SearchByID(mFa1Id);
    if (fa1 != NULL) {
        fa1->init_bigelf_change();
        fopAcM_seStart(this, JA_SE_CM_DY_ENTER, 0);
    }
}

/* 00001848-000018D4       .text demoProcFa1__10daBigelf_cFv */
BOOL daBigelf_c::demoProcFa1() {
    fopAc_ac_c* fa1 = fopAcM_SearchByID(mFa1Id);
    if (fa1 != NULL) {
        f32 target = current.pos.y + 70.0f;
        cLib_addCalc2(&fa1->current.pos.y, target, 0.2f, 100.0f);
    }
    dComIfGp_evmng_cutEnd(mStaffId);
    return TRUE;
}

/* 000018D4-00001948       .text demoInitWait__10daBigelf_cFv */
void daBigelf_c::demoInitWait() {
    int* a_intP = dComIfGp_evmng_getMyIntegerP(mStaffId, "Timer");
    if (a_intP != NULL) {
        mTimer = *a_intP;
    } else {
        mTimer = 0;
    }
    setAnm(0);
}

/* 00001948-00001998       .text demoProcWait__10daBigelf_cFv */
BOOL daBigelf_c::demoProcWait() {
    if (mTimer > 0) {
        mTimer--;
    } else {
        dComIfGp_evmng_cutEnd(mStaffId);
    }
    return FALSE;
}

/* 00001998-00001A74       .text demoInitCom__10daBigelf_cFv */
void daBigelf_c::demoInitCom() {
    setFlag(0x1);
    if (dComIfGp_evmng_getMyIntegerP(mStaffId, "Ship") != NULL) {
        daShip_c* ship = dComIfGp_getShipActor();
        cXyz pos, offset;
        if (ship != NULL) {
            offset.set(0, 0, 800);
            fpoAcM_absolutePos(this, &offset, &pos);
            pos.y = ship->current.pos.y;
            s16 angle = cLib_targetAngleY(&current.pos, &pos);
            angle += 0x4000;
            ship->initStartPos(&pos, angle);
        }
        setFlag(0x20);
    }
}

/* 00001A74-00001ACC       .text demoProcCom__10daBigelf_cFv */
BOOL daBigelf_c::demoProcCom() {
    if (getType() != 6) {
        dKy_custom_colset(4, 0, m3A8);
    }
    lightProc();
    darkProc();
}

/* 00001ACC-00001B14       .text getNowEventAction__10daBigelf_cFv */
s32 daBigelf_c::getNowEventAction() {
    static char* action_table[] = {
        "WAIT",
        "FA1",
        "APPEAR",
        "TALK",
        "EXIT",
        "FL_DEMO",
        "FL_LINK",
        "FL_DELETE",
        "FL_DM_BF",
        "FL_DM_MD",
        "FL_DM_AF",
    };
    return dComIfGp_evmng_getMyActIdx(mStaffId, action_table, ARRAY_SIZE(action_table), FALSE, 1);
}

/* 00001B14-00001CCC       .text demoProc__10daBigelf_cFv */
BOOL daBigelf_c::demoProc() {
    int action = getNowEventAction();
    if (dComIfGp_evmng_getIsAddvance(mStaffId)) {
        demoInitCom();
        switch (action) {
        case 0: demoInitWait(); break;
        case 1: demoInitFa1(); break;
        case 2: demoInitAppear(); break;
        case 3: demoInitTalk(); break;
        case 4: demoInitExit(); break;
        case 5: demoInitFlDemo(); break;
        case 6: demoInitFlLink(); break;
        case 7: demoInitFlDelete(); break;
        case 8: demoInitFlDmBf(); break;
        case 9: demoInitFlDmMd(); break;
        case 10: demoInitFlDmAf(); break;
        }
    }
    switch (action) {
    case 0: demoProcWait(); break;
    case 1: demoProcFa1(); break;
    case 2: demoProcAppear(); break;
    case 3: demoProcTalk(); break;
    case 4: demoProcExit(); break;
    case 5: demoProcFlDemo(); break;
    case 6: demoProcFlLink(); break;
    case 7: demoProcFlDelete(); break;
    case 8: demoProcFlDmBf(); break;
    case 9: demoProcFlDmMd(); break;
    case 10: demoProcFlDmAf(); break;
    default: dComIfGp_evmng_cutEnd(mStaffId); break;
    }
    demoProcCom();
}

/* 00001CCC-00001CD8       .text getType__10daBigelf_cFv */
u8 daBigelf_c::getType() {
    return fopAcM_GetParam(this) & 0xFF;
}

/* 00001CD8-00001CE4       .text getSwbit__10daBigelf_cFv */
u8 daBigelf_c::getSwbit() {
    return fopAcM_GetParam(this) >> 8 & 0xFF;
}

/* 00001CE4-00001CF0       .text getSwbit2__10daBigelf_cFv */
u8 daBigelf_c::getSwbit2() {
    return fopAcM_GetParam(this) >> 16 & 0xFF;
}

/* 00001CF0-00001D70       .text getEventFlag__10daBigelf_cFv */
u16 daBigelf_c::getEventFlag() {
    switch (getType()) {
    case 0: return dSv_event_flag_c::UNK_3020;
    case 1: return dSv_event_flag_c::UNK_3010;
    case 2: return dSv_event_flag_c::UNK_3008;
    case 3: return dSv_event_flag_c::UNK_3004;
    case 4: return dSv_event_flag_c::UNK_3002;
    case 5: return dSv_event_flag_c::UNK_3001;
    case 6: return dSv_event_flag_c::UNK_3180;
    default: return 0;
    }
}

/* 00001D70-00001E20       .text makeFa1S__10daBigelf_cFv */
void daBigelf_c::makeFa1S() {
    cXyz pos = current.pos;
    csXyz angle;
    angle.x = current.angle.x;
    angle.y = current.angle.y;
    angle.z = current.angle.z;
    pos.y += mHeightOffset;
    for (int i = 0; i < 10; i++) {
        fopAcM_create(fpcNm_NPC_FA1_e, 4, &pos, fopAcM_GetRoomNo(this), &angle, NULL, -1, NULL);
        angle.y += 0x2710;
    }
}

/* 00001E20-00001EB4       .text makeFa1__10daBigelf_cFv */
void daBigelf_c::makeFa1() {
    cXyz pos = current.pos;
    csXyz angle;
    angle.x = current.angle.x;
    angle.y = current.angle.y;
    angle.z = current.angle.z;
    pos.y += 100.0f;
    mFa1Id = fopAcM_create(fpcNm_NPC_FA1_e, 6, &pos, fopAcM_GetRoomNo(this), &angle, NULL, -1, NULL);
}

/* 00001EB4-0000200C       .text setAnm__10daBigelf_cFSc */
void daBigelf_c::setAnm(signed char idx) {
    f32 morf = 8.0f;
    int loopMode = -1;
    f32 speed = 1.0f, start = 0.0f;
    f32 end = -1.0f;
    switch (idx) {
    case 1:
    case 2:
        morf = 0.0f;
        break;
    case 3:
        loopMode = 3;
        start = 0.0f;
        speed = -1.0f;
        end = 100.0f;
        break;
    }
    switch (mAnm) {
    case 1:
        morf = 0.0f;
    }
    if (idx != mAnm && idx != -1) {
        mAnm = idx;
        mOldAnmFrame = 0;
        mAnmEnd = 0;
        J3DAnmTransform* bck = static_cast<J3DAnmTransform*>(dComIfG_getObjectRes("bigelf", l_bck_ix_tbl[mAnm]));
        mpMorf->setAnm(bck, loopMode, morf, speed, start, end, NULL);
    }
}

/* 0000200C-00002030       .text setAnmStatus__10daBigelf_cFv */
void daBigelf_c::setAnmStatus() {
    setAnm(0);
}

/* 00002030-000021A4       .text next_msgStatus__10daBigelf_cFPUl */
u16 daBigelf_c::next_msgStatus(unsigned long* pMsgNo) {
    u16 status = fopMsgStts_MSG_CONTINUES_e;
    switch (*pMsgNo) {
    case 0x2EE8:
        (*pMsgNo)++;
        switch (getType()) {
        case 0:
        case 1:
            if (dComIfGs_getWalletSize() == 0) {
                mGivenItem = dItemNo_MAX_RUPEE_UP1_e;
            } else {
                mGivenItem = dItemNo_MAX_RUPEE_UP2_e;
            }
            break;
        case 2:
        case 3:
            if (dComIfGs_getBombMax() <= 30) {
                mGivenItem = dItemNo_MAX_BOMB_UP1_e;
            } else {
                mGivenItem = dItemNo_MAX_BOMB_UP2_e;
            }
            break;
        case 4:
        case 5:
            if (dComIfGs_getArrowMax() <= 30) {
                mGivenItem = dItemNo_MAX_ARROW_UP1_e;
            } else {
                mGivenItem = dItemNo_MAX_ARROW_UP2_e;
            }
            break;
        default:
            mGivenItem = dItemNo_RED_RUPEE_e;
            break;
        }
        dComIfGp_event_setGtItm(mGivenItem);
        break;
    case 0x2EEB:
    case 0x2EEC:
        (*pMsgNo)++;
        break;
    case 0x2EEF:
    case 0x2EF0:
        (*pMsgNo)++;
        break;
    case 0x2EF1:
        *pMsgNo = 0x2EEE;
        break;
    default:
        status = fopMsgStts_MSG_ENDS_e;
        break;
    }
    return status;
}

/* 000021A4-000021D4       .text getMsg__10daBigelf_cFv */
u32 daBigelf_c::getMsg() {
    if (mCurrentMessageId == 0x2EEE) {
        if (dComIfGs_getItem(dInvSlot_BOW_e) == dItemNo_BOW_e) {
            mCurrentMessageId = 0x2EEF;
        }
    }
    return mCurrentMessageId;
}

/* 000021D4-000021D8       .text msgPushButton__10daBigelf_cFv */
void daBigelf_c::msgPushButton() {
}

/* 000021D8-000021DC       .text msgAnm__10daBigelf_cFUc */
void daBigelf_c::msgAnm(unsigned char) {
}

/* 000021DC-000021F0       .text talkInit__10daBigelf_cFv */
void daBigelf_c::talkInit() {
    mTalkState = 0;
    m344 = 0xFF;
}

/* 000021F0-0000236C       .text talk__10daBigelf_cFv */
u16 daBigelf_c::talk() {
    u16 status = 0xFF;
    if (mTalkState == TALK_INIT) {
        l_msgId = fpcM_ERROR_PROCESS_ID_e;
        mCurrMsgNo = getMsg();
        mTalkState = TALK_MSG_CREATE;
    } else if (mTalkState != TALK_FINISHED) {
        if (l_msgId == fpcM_ERROR_PROCESS_ID_e) {
            l_msgId = fopMsgM_messageSet(mCurrMsgNo, this);
        } else {
            if (!chkFlag(0x4)) {
                msgAnm(dComIfGp_getMesgAnimeAttrInfo());
            }
            switch (mTalkState) {
            case TALK_MSG_CREATE:
                l_msg = fopMsgM_SearchByID(l_msgId);
                if (l_msg != NULL) {
                    mTalkState = TALK_ACTIVE;
                }
                break;
            case TALK_ACTIVE:
                status = l_msg->mStatus;
                if (status == fopMsgStts_MSG_DISPLAYED_e) {
                    msgPushButton();
                    l_msg->mStatus = next_msgStatus(&mCurrMsgNo);
                    if (l_msg->mStatus == fopMsgStts_MSG_CONTINUES_e) {
                        fopMsgM_messageSet(mCurrMsgNo);
                    }
                } else if (status == fopMsgStts_BOX_CLOSED_e) {
                    l_msg->mStatus = fopMsgStts_MSG_DESTROYED_e;
                    mTalkState = TALK_FINISHED;
                }
                break;
            }
        }
    }
    return status;
}

/* 0000236C-00002534       .text init__10daBigelf_cFv */
BOOL daBigelf_c::init() {
    switch (mNpcType) {
    case 0:
        setAction(&daBigelf_c::wait_action, NULL);
        break;
    }
    current.pos.y = home.pos.y + 30.0f;
    mAttnBasePos = current.pos;
    mAttnBasePos.y += 100.0f;
    mEyePos = mAttnBasePos;
    eyePos = mEyePos;
    attention_info.position.set(mAttnBasePos.x, mAttnBasePos.y + 50.0f, mAttnBasePos.z);
    mFa1Id = fpcM_ERROR_PROCESS_ID_e;
    if (getType() != 6) {
        if (dComIfGs_isEventBit(getEventFlag())) {
            makeFa1S();
        } else {
            makeFa1();
        }
    }
    setFlag(0x2);
    return TRUE;
}

/* 00002534-000025A0       .text setAttention__10daBigelf_cFb */
void daBigelf_c::setAttention(bool force) {
    if (mAttnTimer > 0) {
        mAttnTimer--;
        return;
    }
    if (force || mAttnSetCount < 2) {
        eyePos.set(mEyePos.x, mEyePos.y, mEyePos.z);
        attention_info.position.set(mAttnBasePos.x, mAttnBasePos.y + 50.0f, mAttnBasePos.z);
    }
}

/* 000025A0-000025F4       .text lookBack__10daBigelf_cFv */
void daBigelf_c::lookBack() {
    fopAc_ac_c* player = dComIfGp_getLinkPlayer();
    if (!chkFlag(0x20)) {
        current.angle.y = fopAcM_searchActorAngleY(this, player);
        shape_angle.y = current.angle.y;
    }
}

/* 000025F4-000026C0       .text hunt__10daBigelf_cFv */
bool daBigelf_c::hunt() {
    fopAc_ac_c* fa1 = fopAcM_SearchByID(mFa1Id);
    fopAc_ac_c* player = dComIfGp_getLinkPlayer();
    if (fa1 == NULL) {
        mMode = 3;
        return false;
    }
    if (fopAcM_searchActorDistanceXZ(this, player) < 900.0f) {
        mMode = 1;
        m3C8 = dComIfGp_evmng_getEventIdx("BIGELF_ARRIVAL", 0xFF);
        fopAcM_orderOtherEventId(this, m3C8, 0xFF, 0xFFFF, 0, 1);
    }
    return true;
}

/* 000026C0-00002730       .text oct_search__10daBigelf_cFv */
bool daBigelf_c::oct_search() {
    fopAc_ac_c* octa = fopAcM_searchFromName("Daiocta", 0, 0);
    if (octa != NULL) {
        mOctId = fopAcM_GetID(octa);
        mMode = 5;
        mOctTimer = 10;
    }
    return true;
}

/* 00002730-000028E8       .text oct__10daBigelf_cFv */
bool daBigelf_c::oct() {
    fopAc_ac_c* octa = fopAcM_SearchByID(mOctId);
    if (octa != NULL) {
        current.pos = octa->current.pos;
        current.pos.y = home.pos.y;
        attention_info.position = current.pos;
        eyePos = current.pos;
        current.angle.y = octa->shape_angle.y;
        cXyz zero(0.0f, 0.0f, 0.0f);
        cXyz offset(-820.0f, 0.0f, 1340.0f);
        current.angle.y += cLib_targetAngleY(&zero, &offset);
        shape_angle.y = current.angle.y;
        setFlag(0x20);
    } else {
        clrFlag(0x20);
    }
    if (dComIfGs_isSwitch(getSwbit(), fopAcM_GetRoomNo(this))) {
        if (mOctTimer > 0) {
            mOctTimer--;
        } else {
            fopAc_ac_c* player = dComIfGp_getLinkPlayer();
            mMode = 1;
            m3C8 = dComIfGp_evmng_getEventIdx("BIGELF_ARRIVAL2", 0xFF);
            fopAcM_orderChangeEventId(this, player, m3C8, 0, 0xFFFF);
            mOctTimer = 0x1E;
            setFlag(0x40);
        }
    }
    return true;
}

/* 000028E8-000029A0       .text ready0__10daBigelf_cFv */
bool daBigelf_c::ready0() {
    fopAcM_SearchByID(mFa1Id);
    if (eventInfo.checkCommandDemoAccrpt()) {
        mMode = 2;
        mStaffId = dComIfGp_evmng_getMyStaffId("BigElf", NULL, 0);
        m3A8 = 1.0f;
        demoProc();
    } else {
        fopAcM_orderOtherEventId(this, m3C8, 0xFF, 0xFFFF, 0, 1);
    }
    return true;
}

/* 000029A0-00002A78       .text event0__10daBigelf_cFv */
bool daBigelf_c::event0() {
    if (dComIfGp_evmng_endCheck(m3C8)) {
        dComIfGs_onEventBit(getEventFlag());
        mMode = 3;
        dComIfGp_event_reset();
#if VERSION > VERSION_DEMO
        if (getType() == 6) {
            if (getSwbit2() != 0xFF) {
                dComIfGs_onSwitch(getSwbit2(), fopAcM_GetRoomNo(this));
            }
        }
#endif
    } else {
        demoProc();
    }
    return true;
}

/* 00002A78-00002A80       .text dead__10daBigelf_cFv */
bool daBigelf_c::dead() {
    return true;
}

/* 00002A80-00002C8C       .text wait_action__10daBigelf_cFPv */
BOOL daBigelf_c::wait_action(void*) {
    if (mActionStatus == ACTION_STARTING) {
        if (dComIfGs_isEventBit(getEventFlag())) {
            mMode = 3;
        } else if (getType() == 6) {
            if (dComIfGs_isSwitch(getSwbit(), fopAcM_GetRoomNo(this))) {
#if VERSION > VERSION_DEMO
                if (getSwbit2() != 0xFF) {
                    dComIfGs_onSwitch(getSwbit2(), fopAcM_GetRoomNo(this));
                }
#endif
                mMode = 3;
            } else {
                mMode = 4;
            }
        } else {
            mMode = 0;
        }
        setAnmStatus();
        mActionStatus++;
    } else if (mActionStatus != ACTION_ENDING) {
        bool force;
        switch (mMode) {
        case 0: force = hunt(); break;
        case 1: force = ready0(); break;
        case 2: force = event0(); break;
        case 3: force = dead(); break;
        case 4: force = oct_search(); break;
        case 5: force = oct(); break;
        default: force = false; break;
        }
        lookBack();
        setAttention(force);
        if (chkFlag(0x2)) {
            attention_info.position = current.pos;
            eyePos = current.pos;
        }
        oct_delete();
    }
    return TRUE;
}

/* 00002C8C-00002DB4       .text _draw__10daBigelf_cFv */
BOOL daBigelf_c::_draw() {
    J3DModel* model = mpMorf->getModel();
    J3DModelData* modelData = model->getModelData();
    J3DModelData* flowerModelData = mpFlowerModel->getModelData();
    if (chkFlag(0x2)) {
        return FALSE;
    }
    if (!chkFlag(0x10)) {
        g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    }
    g_env_light.setLightTevColorType(model, &tevStr);
    g_env_light.setLightTevColorType(mpFlowerModel, &tevStr);
    mBrk.entry(modelData);
    mBtk.entry(modelData);
    mpMorf->entry();
    if (chkFlag(0x8)) {
        mFlowerBrk.entry(flowerModelData);
        MtxP mtx = model->getAnmMtx(m_fl_jnt);
        MTXCopy(mtx, mpFlowerModel->getBaseTRMtx());
        mDoExt_modelUpdateDL(mpFlowerModel);
    }
    return TRUE;
}

/* 00002DB4-00002F5C       .text _execute__10daBigelf_cFv */
BOOL daBigelf_c::_execute() {
    m_jnt.setParam(0, 0, 0, 0, 0xFA0, 0x2328, -0x7D0, -0xFA0, 0x1000);
    if (!chkFlag(0x10)) {
        mAnmEnd = mpMorf->play(&eyePos, 0, 0);
        if (mpMorf->getFrame() < mOldAnmFrame && mAnm != 3) {
            mAnmEnd = TRUE;
        }
        mOldAnmFrame = mpMorf->getFrame();
    }
    clrFlag(0x8);
    (this->*mCurrActionFunc)(NULL);
    shape_angle.y = current.angle.y;
    tevStr.mRoomNo = current.roomNo;
    J3DModel* model = mpMorf->getModel();
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    model->setBaseScale(scale);
    if (chkFlag(0x10)) {
        mDoMtx_stack_c::transM(0.0f, mHeightOffset, 0.0f);
        mDoMtx_stack_c::scaleM(m3EC, m3F0, m3EC);
        mDoMtx_stack_c::transM(0.0f, -mHeightOffset, 0.0f);
    }
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();
    mBtk.play();
    return TRUE;
}

/* 00002F5C-00002FAC       .text _delete__10daBigelf_cFv */
BOOL daBigelf_c::_delete() {
    dComIfG_resDelete(&mPhase, "bigelf");
    if (mpMorf != NULL) {
        mpMorf->stopZelAnime();
    }
    return TRUE;
}

/* 00002FAC-00002FCC       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* i_this) {
    return ((daBigelf_c*)i_this)->CreateHeap();
}

/* 00002FCC-00003124       .text _create__10daBigelf_cFv */
cPhs_State daBigelf_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daBigelf_c);
#endif
    cPhs_State phase_state = dComIfG_resLoad(&mPhase, "bigelf");
    if (phase_state == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_SetupActor(this, daBigelf_c);
#endif
        switch (fopAcM_GetName(this)) {
        case fpcNm_BIGELF_e:
            mNpcType = 0;
            break;
        default:
            return cPhs_ERROR_e;
        }
        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, 0xB7B0)) {
#if VERSION > VERSION_DEMO
            mpMorf = NULL;
#endif
            return cPhs_ERROR_e;
        }
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        if (!init()) {
#if VERSION > VERSION_DEMO
            mpMorf = NULL;
#endif
            return cPhs_ERROR_e;
        }
    }
    return phase_state;
}

/* 00003224-00003808       .text CreateHeap__10daBigelf_cFv */
BOOL daBigelf_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("bigelf", dRes_INDEX_BIGELF_BDL_DY_e);
    JUT_ASSERT(DEMO_SELECT(0x7c3, 0x7d4), modelData);

    mpMorf = new mDoExt_McaMorf(
        modelData,
        NULL,
        NULL,
        (J3DAnmTransformKey*)dComIfG_getObjectRes("bigelf", dRes_INDEX_BIGELF_BCK_WAIT01_e),
        J3DFrameCtrl::EMode_LOOP,
        1.0f,
        0,
        -1,
        1,
        NULL,
        0x80000,
        0x11000222
    );
    if (mpMorf == NULL || mpMorf->getModel() == NULL) {
        return FALSE;
    }

    J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)dComIfG_getObjectRes("bigelf", dRes_INDEX_BIGELF_BRK_DY_BODY_e);
    if (!mBrk.init(modelData, brk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }

    J3DAnmTextureSRTKey* btk = (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("bigelf", dRes_INDEX_BIGELF_BTK_DY_BODY_e);
    if (!mBtk.init(modelData, btk, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }

    J3DSkinDeform* deform = new J3DSkinDeform();
    if (deform == NULL) {
        return FALSE;
    }
    s32 err = mpMorf->getModel()->setSkinDeform(deform, 1);
    switch (err) {
    case J3DErrType_Success:
        break;
    case J3DErrType_OutOfMemory:
        return FALSE;
    default:
        JUT_ASSERT(DEMO_SELECT(0x800, 0x811), 0);
        break;
    }

    J3DModel* pModel = mpMorf->getModel();
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(current.angle.y);
    pModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->calc();

    m_jnt.setHeadJntNum(modelData->getJointName()->getIndex("head"));
    JUT_ASSERT(DEMO_SELECT(0x813, 0x824), m_jnt.getHeadJntNum() >= 0);
    m_jnt.setBackboneJntNum(modelData->getJointName()->getIndex("backbone"));
    JUT_ASSERT(DEMO_SELECT(0x818, 0x829), m_jnt.getBackboneJntNum() >= 0);
    m_fl_jnt = modelData->getJointName()->getIndex("handRB");
    JUT_ASSERT(DEMO_SELECT(0x81c, 0x82d), m_fl_jnt >= 0);

    for (u16 i = 0; i < modelData->getJointNum(); i++) {
        if (i == getHeadJntNum() || i == getBackboneJntNum() || i == m_fl_jnt) {
            mpMorf->getModel()->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack_Bigelf);
        }
    }
    mpMorf->getModel()->setUserArea((u32)this);

    J3DModelData* flModelData = (J3DModelData*)dComIfG_getObjectRes("bigelf", dRes_INDEX_BIGELF_BDL_DY_FL_e);
    JUT_ASSERT(DEMO_SELECT(0x831, 0x842), flModelData);
    mpFlowerModel = mDoExt_J3DModel__create(flModelData, 0x80000, 0x1000000);
    if (mpFlowerModel == NULL) {
        return FALSE;
    }

    J3DAnmTevRegKey* flowerBrk = (J3DAnmTevRegKey*)dComIfG_getObjectRes("bigelf", dRes_INDEX_BIGELF_BRK_DY_FL_e);
    if (!mFlowerBrk.init(flModelData, flowerBrk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0)) {
        return FALSE;
    }

    mPaNo = 0;
    switch (getType()) {
    case 2:
    case 3:
        mBrk.setFrame(1.0f);
        mFlowerBrk.setFrame(1.0f);
        mPaNo = 1;
        break;
    case 4:
    case 5:
        mBrk.setFrame(2.0f);
        mFlowerBrk.setFrame(2.0f);
        mPaNo = 2;
        break;
    case 6:
        mBrk.setFrame(3.0f);
        mFlowerBrk.setFrame(3.0f);
        mPaNo = 3;
        break;
    }
    return TRUE;
}

/* 00003808-00003828       .text daBigelf_Create__FP10fopAc_ac_c */
static cPhs_State daBigelf_Create(fopAc_ac_c* i_this) {
    return ((daBigelf_c*)i_this)->_create();
}

/* 00003828-00003848       .text daBigelf_Delete__FP10daBigelf_c */
static BOOL daBigelf_Delete(daBigelf_c* i_this) {
    return ((daBigelf_c*)i_this)->_delete();
}

/* 00003848-00003868       .text daBigelf_Execute__FP10daBigelf_c */
static BOOL daBigelf_Execute(daBigelf_c* i_this) {
    return ((daBigelf_c*)i_this)->_execute();
}

/* 00003868-00003888       .text daBigelf_Draw__FP10daBigelf_c */
static BOOL daBigelf_Draw(daBigelf_c* i_this) {
    return ((daBigelf_c*)i_this)->_draw();
}

/* 00003888-00003890       .text daBigelf_IsDelete__FP10daBigelf_c */
static BOOL daBigelf_IsDelete(daBigelf_c*) {
    return TRUE;
}

static actor_method_class l_daBigelf_Method = {
    (process_method_func)daBigelf_Create,
    (process_method_func)daBigelf_Delete,
    (process_method_func)daBigelf_Execute,
    (process_method_func)daBigelf_IsDelete,
    (process_method_func)daBigelf_Draw,
};

actor_process_profile_definition g_profile_BIGELF = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_BIGELF_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daBigelf_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_BIGELF_e,
    /* Actor SubMtd */ &l_daBigelf_Method,
    /* Status       */ fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_12_e,
};
