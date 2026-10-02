/**
 * d_a_obj_flame.cpp
 * Object - timed flame column
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_flame.h"
#include "d/actor/d_a_obj_magmarock.h"
#include "d/actor/d_a_mflft.h"
#include "d/actor/d_a_ykgr.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_audio.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "res/Object/Yfire_00.h"

const daObjFlame::Act_c::AttrBase_c daObjFlame::Act_c::M_attr_base = {
    6, 400, 400, 200, -100, 1000, 100, 270,
};

const dCcD_SrcCps daObjFlame::Act_c::M_cps_src = {
    {
        /* Flags             */ 0,
        /* SrcObjAt  Type    */ AT_TYPE_FIRE,
        /* SrcObjAt  Atp     */ 1,
        /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
        /* SrcObjTg  Type    */ 0,
        /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsOther_e,
        /* SrcObjCo  SPrm    */ 0,
        /* SrcGObjAt Se      */ 0,
        /* SrcGObjAt HitMark */ 0,
        /* SrcGObjAt Spl     */ 1,
        /* SrcGObjAt Mtrl    */ 0,
        /* SrcGObjAt SPrm    */ 0,
        /* SrcGObjTg Se      */ 0,
        /* SrcGObjTg HitMark */ 0,
        /* SrcGObjTg Spl     */ 0,
        /* SrcGObjTg Mtrl    */ 0,
        /* SrcGObjTg SPrm    */ 0,
        /* SrcGObjCo SPrm    */ 0,
    },
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 30.0f}},
};

const daObjFlame::Act_c::AttrScale_c daObjFlame::Act_c::M_attr_scl[4] = {
    {
        1.0f, 1.0f, 1.0f, 1.0f,
        dRes_INDEX_YFIRE_00_BMD_YFIRE_00_e, dRes_INDEX_YFIRE_00_BTK_YFIRE_00_e, dRes_INDEX_YFIRE_00_BRK_YFIRE_00_e, DEMO_SELECT(0x5740, 0x52e0),
        1.0f, 1.0f, dPa_name::ID_AK_SN_O_FIRESHAFTFOOT, true, true,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        -150, -1, -150, 150, 1500, 150,
        0.7f,
    },
    {
        4.3333335f, 3.3333335f, 1.0f, DEMO_SELECT(0.69f, 0.815f),
        dRes_INDEX_YFIRE_00_BMD_YFIRB_00_e, dRes_INDEX_YFIRE_00_BTK_YFIRB_00_e, -1, DEMO_SELECT(0x6760, 0x62a0),
        1.0f, 0.5f, dPa_name::ID_AK_SN_FIRESHAFTFOOTBIG, false, false,
        4.3333335f, 3.3333335f * DEMO_SELECT(0.69f, 0.815f), 4.3333335f, 3.3333335f * DEMO_SELECT(0.69f, 0.815f), 0.8666667f, 0.6666667f * DEMO_SELECT(0.69f, 0.815f),
        -700, -1, -700, 700, 7500, 700,
        0.0f,
    },
    {
        7.5f, 7.5f, 1.0f, 1.0f,
        dRes_INDEX_YFIRE_00_BMD_YFIRB_00_e, dRes_INDEX_YFIRE_00_BTK_YFIRB_00_e, -1, 0x6760,
        1.0f, 0.5f, dPa_name::ID_AK_SN_FIRESHAFTFOOTBIG, false, false,
        7.5f, 7.5f, 7.5f, 7.5f, 1.0f, 1.0f,
        -150, -1, -150, 150, 1500, 150,
        0.0f,
    },
    {
        0.5f, 0.5f, 0.5f, 0.5f,
        dRes_INDEX_YFIRE_00_BMD_YFIRE_00_e, dRes_INDEX_YFIRE_00_BTK_YFIRE_00_e, dRes_INDEX_YFIRE_00_BRK_YFIRE_00_e, DEMO_SELECT(0x5740, 0x52e0),
        1.0f, 1.0f, dPa_name::ID_AK_SN_O_FIRESHAFTFOOT, false, true,
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        -75, -1, -75, 75, 750, 75,
        0.7f,
    },
};

char daObjFlame::Act_c::M_arcname[] = "Yfire_00";

/* 00000078-00000110       .text set_switch__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::set_switch() {
    if (mType == 1) {
        if (mModeProc == 3 || mModeProc == 4) {
            int sw = prm_get_swSave();
            dComIfGs_onSwitch(sw, fopAcM_GetHomeRoomNo(this));
        } else {
            int sw = prm_get_swSave();
            dComIfGs_offSwitch(sw, fopAcM_GetHomeRoomNo(this));
        }
    }
}

/* 00000110-00000134       .text solidHeapCB__Q210daObjFlame5Act_cFP10fopAc_ac_c */
BOOL daObjFlame::Act_c::solidHeapCB(fopAc_ac_c* actor) {
    return static_cast<Act_c*>(actor)->create_heap();
}

/* 00000134-00000414       .text create_heap__Q210daObjFlame5Act_cFv */
bool daObjFlame::Act_c::create_heap() {
    bool success = false;
    J3DModelData* mdl_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, attr_scl().model);
    JUT_ASSERT(DEMO_SELECT(0x1F2, 0x1F3), mdl_data != 0);
    mpModel = mDoExt_J3DModel__create(mdl_data, 0, 0x11020203);
    J3DAnmTextureSRTKey* btk = static_cast<J3DAnmTextureSRTKey*>(dComIfG_getObjectRes(M_arcname, attr_scl().btk));
    mpBtkAnm = new mDoExt_btkAnm;
    BOOL btkOK = FALSE;
    JUT_ASSERT(DEMO_SELECT(0x1FB, 0x1FC), btk != 0);
    if (mpBtkAnm != NULL) {
        btkOK = mpBtkAnm->init(mdl_data, btk, TRUE, J3DFrameCtrl::EMode_LOOP, attr_scl().animationSpeed, 0, -1, false, 0);
    }
    BOOL brkOK = FALSE;
    if (attr_scl().brk >= 0) {
        J3DAnmTevRegKey* brk = static_cast<J3DAnmTevRegKey*>(dComIfG_getObjectRes(M_arcname, attr_scl().brk));
        mpBrkAnm = new mDoExt_brkAnm;
        JUT_ASSERT(DEMO_SELECT(0x211, 0x212), brk != 0);
        if (mpBrkAnm != NULL) {
            brkOK = mpBrkAnm->init(mdl_data, brk, TRUE, J3DFrameCtrl::EMode_LOOP, attr_scl().animationSpeed, 0, -1, false, 0);
        }
    } else {
        brkOK = TRUE;
    }
    if (mpModel != NULL && btkOK && brkOK) {
        success = true;
    }
    return success;
}

/* 0000045C-000007D8       .text create_mode_init__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::create_mode_init() {
    u8 sch = prm_get_sch();
    u8 currentSch = dKy_get_schbit();
    if (sch != 0 && currentSch != 0) {
        int timer = dKy_get_schbit_timer();
        int period = dStage_stagInfo_GetSchSec(dComIfGp_getStageStagInfo()) * 30;
        u32 bits = sch >= currentSch ? sch : sch << 8;
        int shift = 0;
        for (int mask = currentSch; !(bits & mask); mask <<= 1) {
            ++shift;
        }
        int t = timer + period * shift;
        f32 time = t * attr_scl().cycleSpeed;
        if (mType != 1) {
            time -= 127.0f;
        }
        if (time < 0.0f) {
            mModeProc = 1;
            mTimer = 127.0f + time;
            mHeight = 0.0f;
        } else if (time < 23.0f) {
            mModeProc = 2;
            mTimer = 23.0f - time;
            mHeight = 0.0f;
        } else if (time < 45.0f) {
            mModeProc = 3;
            mTimer = 45.0f - time;
            mHeight = (1.0f / 22.0f) * (22.0f - mTimer);
        } else if (time < 135.0f) {
            mModeProc = 4;
            mTimer = 135.0f - time;
            mHeight = 1.0f;
        } else if (time < 160.0f) {
            mModeProc = 5;
            mTimer = 160.0f - time;
            mHeight = 0.04f * mTimer;
        } else if (time < 180.0f) {
            mModeProc = 6;
            mTimer = 180.0f - time;
            mHeight = 0.0f;
        } else {
            mModeProc = 0;
            mTimer = 0.0f;
            mHeight = 0.0f;
        }
        f32 frame = (1.0f / 180.0f) * time;
        if (mModeProc != 0 && mModeProc != 1) {
            if (mpBtkAnm != NULL) {
                mpBtkAnm->setFrame(1.0f + frame * mpBtkAnm->getEndFrame());
            }
            if (mpBrkAnm != NULL) {
                mpBrkAnm->setFrame(1.0f + frame * mpBrkAnm->getEndFrame());
            }
        }
        if (mModeProc != 0) {
            if (mModeProc == 1 || mModeProc == 2 || mModeProc == 3 || mModeProc == 4) {
                mEm0State = 1;
            }
            if (mModeProc == 1 || mModeProc == 2 || mModeProc == 3 || mModeProc == 4 || mModeProc == 5) {
                mEm1State = 1;
                mEm2State = 1;
            }
            if (!attr_scl().simple) {
                em_manual_set();
            }
            m457 = 1;
        }
    } else {
        mModeProc = 0;
        if (sch != 0) {
            mTimer = 0.0f;
        } else {
            mTimer = 120.0f;
        }
        mHeight = 0.0f;
    }
}

/* 000007D8-00000930       .text set_mtx__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::set_mtx() {
    mpModel->setBaseScale(scale);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mDoMtx_stack_c::YrotM(mRotY);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    fopAcM_setCullSizeBox(this, attr_scl().cullMinX, attr_scl().cullMinY, attr_scl().cullMinZ, attr_scl().cullMaxX, mHeight * attr_scl().cullMaxY, attr_scl().cullMaxZ);
}

/* 00000930-00000950       .text init_mtx__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::init_mtx() {
    set_mtx();
}

/* 00000950-00000B3C       .text em_position__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::em_position() {
    if (m457 != 0) {
        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::ZXYrotM(shape_angle);
        if (!attr_scl().simple) {
            if (mpEmitter0 != NULL) {
                mDoMtx_stack_c::push();
                mDoMtx_stack_c::transM(0.0f, mExtraScaleY * (1500.0f * mHeight * attr_scl().modelY) + mExtraScaleY * (-300.0f * attr_scl().modelY), 0.0f);
                mpEmitter0->setGlobalRTMatrix(mDoMtx_stack_c::get());
                mDoMtx_stack_c::pop();
            }
            if (mpEmitter1 != NULL) {
                mpEmitter1->setGlobalRTMatrix(mDoMtx_stack_c::get());
            }
        }
        mDoMtx_stack_c::transM(0.0f, m46C * (1500.0f * mHeight * attr_scl().height), 0.0f);
        mDoMtx_stack_c::multVec(&cXyz::Zero, &eyePos);
        m434 = 1500.0f * mHeight - 300.0f > 0.0f;
        if (m434) {
            mDoMtx_stack_c::transM(0.0f, m46C * (-300.0f * attr_scl().height), 0.0f);
            mDoMtx_stack_c::multVec(&cXyz::Zero, &mCpsP1);
        }
    }
}

/* 00000B3C-00000CAC       .text em_simple_set__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::em_simple_set() {
    bool visible = !(attr_scl().hideCovered && m459);
    if (mEm0State == 1 && visible) {
        cXyz pos(eyePos.x, eyePos.y + mExtraScaleY * (-300.0f * attr_scl().height), eyePos.z);
        dComIfGp_particle_setSimple(dPa_name::ID_AK_SN_O_FIRESHAFTHEAD, &pos);
    }
    if (mEm1State == 1) {
        dComIfGp_particle_setSimple(dPa_name::ID_AK_SN_O_FIRESHAFTBODY, &eyePos);
    }
    if (mEm2State == 1 && visible) {
        dComIfGp_particle_setSimple(attr_scl().footEffect, &home.pos);
    }
}

/* 00000CAC-00000CEC       .text em_simple_inv__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::em_simple_inv() {
    if (mEm0State == 3) {
        mpEmitter0 = NULL;
    }
    if (mEm1State == 3) {
        mpEmitter1 = NULL;
    }
    if (mEm2State == 3) {
        mpEmitter2 = NULL;
    }
}

/* 00000CEC-00000F04       .text em_manual_set__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::em_manual_set() {
    bool visible = !(attr_scl().hideCovered && m459);
    if (mEm0State == 1 && visible && mType != 1) {
        cXyz scl(attr_scl().headXZ, mExtraScaleY * attr_scl().headY, attr_scl().headXZ);
        mpEmitter0 = dComIfGp_particle_set(dPa_name::ID_AK_SN_O_FIRESHAFTHEAD, &home.pos, &home.angle, &scl);
        mEm0State = 2;
    }
    if (mEm1State == 1) {
        cXyz scl(attr_scl().bodyXZ, mExtraScaleY * attr_scl().bodyY, attr_scl().bodyXZ);
        mpEmitter1 = dComIfGp_particle_set(dPa_name::ID_AK_SN_O_FIRESHAFTBODY, &home.pos, &home.angle, &scl);
        mEm1State = 2;
    }
    if (mEm2State == 1 && visible) {
        cXyz scl(attr_scl().footScale, attr_scl().footScale, attr_scl().footScale);
        mpEmitter2 = dComIfGp_particle_set(attr_scl().footEffect, &home.pos, &home.angle, &scl);
        mEm2State = 2;
    }
}

/* 00000F04-00001010       .text em_manual_inv__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::em_manual_inv() {
    bool covered = false;
    if (attr_scl().hideCovered && m459) {
        covered = true;
    }
    if (mEm0State == 2 && covered) {
        mEm0State = 3;
    }
    if (mEm2State == 2 && covered) {
        mEm2State = 3;
    }
    if (mEm0State == 3 && mpEmitter0 != NULL) {
        mpEmitter0->becomeInvalidEmitter();
        mpEmitter0 = NULL;
    }
    if (mEm1State == 3 && mpEmitter1 != NULL) {
        mpEmitter1->becomeInvalidEmitter();
        mpEmitter1 = NULL;
    }
    if (mEm2State == 3 && mpEmitter2 != NULL) {
        mpEmitter2->becomeInvalidEmitter();
        mpEmitter2 = NULL;
    }
}

/* 00001010-00001070       .text ki_init__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::ki_init() {
    if (prm_get_kiNum() > 0) {
        m460 = 0;
        m464 = 0;
        m45A = 1;
    }
}

/* 00001070-00001194       .text ki_make__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::ki_make() {
    if (m45A) {
        if (m45C <= 0) {
            if (m464 < prm_get_kiNum()) {
                if (--m460 <= 0) {
                    m460 = attr_base().kiInterval - 1;
                    ++m464;
                    csXyz rot(0, cM_rndFX(32768.0f), 0);
                    fopAcM_create(fpcNm_KI_e, 0xFFFF8002, &current.pos, fopAcM_GetRoomNo(this), &rot);
                }
            } else {
                m45A = 0;
                m45C = prm_get_kiCycle();
            }
        } else {
            m45A = 0;
            --m45C;
        }
    }
}

/* 00001194-000011E4       .text eff_hase__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::eff_hase() {
    dComIfGp_particle_setSimple(dPa_name::ID_AK_SP_O_FIREHOLEKAGEROU, &home.pos);
}

/* 000011E4-00001254       .text se_fireblast_omen__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::se_fireblast_omen() {
    mDoAud_seStart(JA_SE_FIREBLAST_OMEN, &current.pos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

/* 00001254-00001610       .text liftup_magmarock__Q210daObjFlame5Act_cFPvPv */
void* daObjFlame::Act_c::liftup_magmarock(void* candidate, void* arg) {
    Act_c* flame = (Act_c*)arg;
    fopAc_ac_c* actor = (fopAc_ac_c*)candidate;
    if (fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_Obj_Magmarock_e && !fpcM_IsCreating(fopAcM_GetID(actor))) {
        f32 radius = 145.0f * flame->attr_scl().radius + flame->attr_base().liftRadius;
        bool lower = flame->eyePos.y < flame->current.pos.y;
        f32 bottom = (lower ? flame->eyePos.y : flame->current.pos.y) + flame->attr_base().liftBottom;
        f32 top = (lower ? flame->current.pos.y : flame->eyePos.y) + flame->attr_base().liftTop;
        cXyz pos, b, a;
        a.set(actor->current.pos.x, 0.0f, actor->current.pos.z);
        b.set(flame->eyePos.x, 0.0f, flame->eyePos.z);
        if (a.abs(b) < radius && actor->current.pos.y > bottom && actor->current.pos.y < top) {
            daObjMagmarock::Act_c* rock = (daObjMagmarock::Act_c*)actor;
            if (flame->mType != 1) {
                f32 offset, extra;
                if (flame->mHeight < 0.1f) {
                    offset = flame->attr_base().liftOffset;
                    extra = flame->mHeight * (10.0f * flame->attr_base().liftExtra);
                } else if (flame->mHeight > 0.9f) {
                    f32 t = 1.0f - flame->mHeight;
                    offset = t * (10.0f * flame->attr_base().liftOffset);
                    extra = t * (10.0f * flame->attr_base().liftExtra);
                } else {
                    offset = flame->attr_base().liftOffset;
                    extra = flame->attr_base().liftExtra;
                }
                f32 scl = flame->m46C * flame->attr_scl().height;
                offset *= scl;
                extra *= scl;
                pos.set(flame->eyePos.x, extra + (flame->eyePos.y + offset), flame->eyePos.z);
                if (flame->mModeProc == 1 || flame->mModeProc == 2) {
                    rock->BeforeLiftRequest(pos);
                } else {
                    rock->LiftUpRequest(pos);
                }
                flame->m459 = 1;
            }
        }
    }
    return NULL;
}

/* 00001610-00001884       .text liftup_mflft__Q210daObjFlame5Act_cFPvPv */
void* daObjFlame::Act_c::liftup_mflft(void* candidate, void* arg) {
    Act_c* flame = (Act_c*)arg;
    fopAc_ac_c* actor = (fopAc_ac_c*)candidate;
    if (fopAc_IsActor(actor) && fopAcM_GetName(actor) == fpcNm_MFLFT_e) {
        mflft_class* lift = (mflft_class*)actor;
        f32 offset, extra;
        f32 targetScale, targetExtra;
        f32 maxHeight;
        if (lift->m29A == 0) {
            targetScale = 1.0f;
            targetExtra = DEMO_SELECT(1.1816812f, 1.0004418f);
            maxHeight = 5000.0f;
        } else {
            targetScale = 1.0f;
            targetExtra = 1.0f;
            maxHeight = 5000.0f;
        }
        if (flame->mHeight < 0.1f) {
            offset = flame->attr_base().liftOffset;
            extra = flame->mHeight * (10.0f * flame->attr_base().liftExtra);
        } else if (flame->mHeight > 0.9f) {
            f32 t = 1.0f - flame->mHeight;
            offset = t * (10.0f * flame->attr_base().liftOffset);
            extra = t * (10.0f * flame->attr_base().liftExtra);
        } else {
            offset = flame->attr_base().liftOffset;
            extra = flame->attr_base().liftExtra;
        }
        f32 scl = flame->m46C * flame->attr_scl().height;
        offset *= scl;
        extra *= scl;
        cXyz pos(flame->eyePos.x, extra + (flame->eyePos.y + offset), flame->eyePos.z);
        if (pos.y > flame->current.pos.y + maxHeight) {
            pos.y = flame->current.pos.y + maxHeight;
        }
        if (flame->mModeProc != 1 && flame->mModeProc != 2) {
            lift->setLiftUp(pos);
        }
        flame->m459 = 1;
        cLib_addCalc(&flame->m46C, targetScale, 0.3f, 0.1f, 0.01f);
        cLib_addCalc(&flame->mExtraScaleY, targetExtra, 0.3f, 0.1f, 0.01f);
    }
    return NULL;
}

/* 00001884-00001988       .text mode_wait__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_wait() {
    u8 sch = prm_get_sch();
    bool start = false;
    if (sch != 0) {
        if ((sch & dKy_get_schbit()) && dKy_get_schbit_timer() == 0) {
            start = true;
        }
    } else {
        if (mTimer <= 0.0f) start = true;
    }
    if (start) {
        if (mType == 1) {
            mpBtkAnm->setFrame(0.0f);
            if (mpBrkAnm != NULL) {
                mpBrkAnm->setFrame(0.0f);
            }
            mModeProc = 2;
            mTimer = 23.0f;
            ki_init();
        } else {
            mModeProc = 1;
            mTimer = 127.0f;
        }
        mEm0State = 1;
        mEm1State = 1;
        mEm2State = 1;
        m457 = 1;
    }
}

/* 00001988-00001A08       .text mode_wait2__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_wait2() {
    se_fireblast_omen();
    if (mTimer <= 0.0f) {
        mpBtkAnm->setFrame(0.0f);
        if (mpBrkAnm != NULL) {
            mpBrkAnm->setFrame(0.0f);
        }
        mModeProc = 2;
        mTimer = 23.0f;
        ki_init();
    }
}

/* 00001A08-00001A98       .text mode_l_before__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_l_before() {
    m457 = 0;
    if (mTimer <= 0.0f) {
        mModeProc = 3;
        mTimer = 22.0f;
        mDoAud_seStart(JA_SE_FIREBLAST_BLOW, &eyePos, 0, m458);
    } else {
        se_fireblast_omen();
    }
}

/* 00001A98-00001AE8       .text mode_l_u__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_l_u() {
    mHeight = (1.0f / 22.0f) * (22.0f - mTimer);
    m457 = 1;
    if (mTimer <= 0.0f) {
        mModeProc = 4;
        mTimer = 90.0f;
    }
}

/* 00001AE8-00001B20       .text mode_u__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_u() {
    if (mTimer <= 0.0f) {
        mModeProc = 5;
        mTimer = 25.0f;
        mEm0State = 3;
    }
}

/* 00001B20-00001B74       .text mode_u_l__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_u_l() {
    mHeight = 0.04f * mTimer;
    m457 = 1;
    if (mTimer <= 0.0f) {
        mModeProc = 6;
        mTimer = 20.0f;
        mEm1State = 3;
        mEm2State = 3;
    }
}

/* 00001B74-00001BEC       .text mode_l_after__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_l_after() {
    if (mTimer <= 0.0f) {
        mModeProc = 0;
        if (prm_get_sch() != 0) {
            mTimer = 0.0f;
        } else {
            mTimer = 120.0f;
        }
    }
}

/* 00001BEC-00001EAC       .text mode_proc_call__Q210daObjFlame5Act_cFv */
void daObjFlame::Act_c::mode_proc_call() {
    typedef void (Act_c::*ModeProc)();
    static ModeProc mode_proc[] = {
        &Act_c::mode_wait,
        &Act_c::mode_wait2,
        &Act_c::mode_l_before,
        &Act_c::mode_l_u,
        &Act_c::mode_u,
        &Act_c::mode_u_l,
        &Act_c::mode_l_after,
    };
    if (mTimer >= -0.1f) {
        if (mModeProc == 0 || mModeProc == 1) {
            mTimer -= 1.0f;
        } else {
            mTimer -= attr_scl().cycleSpeed;
        }
    }
    (this->*mode_proc[mModeProc])();
    if (attr_scl().simple) {
        em_position();
        em_simple_set();
        em_simple_inv();
    } else {
        em_manual_set();
        em_manual_inv();
        em_position();
    }
    if (mModeProc != 0 && mModeProc != 1) {
        mpBtkAnm->play();
        if (mpBrkAnm != NULL) {
            mpBrkAnm->play();
        }
        mDoAud_seStart(JA_SE_FIREBLAST_NOISE, &eyePos, 0, m458);
    }
    if (m434) {
        bool collide = mModeProc == 5 ? mHeight > attr_scl().collisionEnd : true;
        if (collide) {
            mCps.SetStartEnd(mCpsP0, mCpsP1);
            mCps.SetR(mCpsRad);
            dComIfG_Ccsp()->Set(&mCps);
        }
    }
}

/* 00001EAC-00001ECC       .text Create__Q210daObjFlame6MethodFPv */
cPhs_State daObjFlame::Method::Create(void* actor) {
    return ((Act_c*)actor)->_create();
}

/* 00001ECC-00002200       .text _create__Q210daObjFlame5Act_cFv */
cPhs_State daObjFlame::Act_c::_create() {
    fopAcM_SetupActor(this, Act_c);
    cPhs_State phase = dComIfG_resLoad(&mPhs, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        mType = prm_get_scl();
        if (fopAcM_entrySolidHeap(this, solidHeapCB, attr_scl().heapSize)) {
            mOrigScale = scale;
            if (mType == 1) {
                m46C = 1.0f;
                mExtraScaleY = DEMO_SELECT(1.1816812f, 1.0004418f);
            } else {
                m46C = 1.0f;
                mExtraScaleY = 1.0f;
            }
            scale.x *= attr_scl().modelXZ;
            scale.y *= mExtraScaleY * attr_scl().modelY;
            scale.z *= attr_scl().modelXZ;
            mEm0State = 0;
            mEm1State = 0;
            mEm2State = 0;
            create_mode_init();
            set_switch();
            fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
            init_mtx();
            mStts.Init(100, 255, this);
            mCps.Set(M_cps_src);
            mCps.SetStts(&mStts);
            mCpsP0 = current.pos;
            mCpsP1 = current.pos;
            mCpsRad = 145.0f * attr_scl().radius;
            m434 = false;
            em_position();
            m458 = dComIfGp_getReverb(home.roomNo);
            m459 = 0;
            m45C = cM_rndF(prm_get_kiCycle() + 1);
            if (mModeProc != 0 && m45C == 0) {
                m45C = 1;
            }
        } else {
            phase = cPhs_ERROR_e;
        }
    }
    return phase;
}

bool daObjFlame::Act_c::_delete() {
    dComIfG_resDeleteDemo(&mPhs, M_arcname);
    return true;
}
bool daObjFlame::Act_c::_execute() {
    scale.x = mOrigScale.x * attr_scl().modelXZ;
    scale.y = mExtraScaleY * (mOrigScale.y * attr_scl().modelY);
    scale.z = mOrigScale.z * attr_scl().modelXZ;
    mode_proc_call();
    ki_make();
    if (prm_get_haze() == 0) {
        eff_hase();
    }
    if (mType == 1) {
        mRotY += attr_base().largeRotation;
        if (mHeight > 0.01f) {
            daYkgr_c::m_aim_rate = 1.0f;
        }
    } else {
        mRotY += attr_base().rotation;
    }
    m459 = 0;
    if (mModeProc == 1 || mModeProc == 2 || mModeProc == 3 || mModeProc == 4) {
        fopAcIt_Judge(liftup_magmarock, this);
        if (mType == 1) {
            fopAcIt_Judge(liftup_mflft, this);
        }
    }
    set_switch();
    set_mtx();
    return true;
}
bool daObjFlame::Act_c::_draw() {
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpModel, &tevStr);
    mpBtkAnm->entry(mpModel->getModelData());
    if (mpBrkAnm != NULL) {
        mpBrkAnm->entry(mpModel->getModelData());
    }
    dComIfGd_setListFilter();
    mDoExt_modelUpdateDL(mpModel);
    dComIfGd_setList();
    return true;
}
bool daObjFlame::Act_c::_is_delete() {
    if (!attr_scl().simple) {
        if (mpEmitter0 != NULL) {
            mpEmitter0->becomeInvalidEmitter();
            mpEmitter0 = NULL;
        }
        if (mpEmitter1 != NULL) {
            mpEmitter1->becomeInvalidEmitter();
            mpEmitter1 = NULL;
        }
        if (mpEmitter2 != NULL) {
            mpEmitter2->becomeInvalidEmitter();
            mpEmitter2 = NULL;
        }
    }
    return true;
}

/* 00002484-000024B4       .text Delete__Q210daObjFlame6MethodFPv */
BOOL daObjFlame::Method::Delete(void* actor) {
    return ((Act_c*)actor)->_delete();
}

/* 000024B4-00002638       .text Execute__Q210daObjFlame6MethodFPv */
BOOL daObjFlame::Method::Execute(void* actor) {
    return ((Act_c*)actor)->_execute();
}

/* 00002638-00002710       .text Draw__Q210daObjFlame6MethodFPv */
BOOL daObjFlame::Method::Draw(void* actor) {
    return ((Act_c*)actor)->_draw();
}

/* 00002710-000027B0       .text IsDelete__Q210daObjFlame6MethodFPv */
BOOL daObjFlame::Method::IsDelete(void* actor) {
    return ((Act_c*)actor)->_is_delete();
}

actor_method_class daObjFlame::Method::Table = {
    (process_method_func)daObjFlame::Method::Create,
    (process_method_func)daObjFlame::Method::Delete,
    (process_method_func)daObjFlame::Method::Execute,
    (process_method_func)daObjFlame::Method::IsDelete,
    (process_method_func)daObjFlame::Method::Draw,
};

actor_process_profile_definition g_profile_Obj_Flame = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Flame_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjFlame::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Flame_e,
    /* Actor SubMtd */ &daObjFlame::Method::Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
