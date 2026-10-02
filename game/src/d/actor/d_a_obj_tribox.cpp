/**
 * d_a_obj_tribox.cpp
 * Object - Hyrule Castle - Pushable triangle blocks
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_obj_tribox.h"
#include "d/actor/d_a_player.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_iter.h"
#include "f_pc/f_pc_priority.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JAZelAudio/JAIZelBasic.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/MtryB.h"
#include "SSystem/SComponent/c_math.h"
#include "math.h"

namespace daObjTribox {
namespace {
    struct Attr_c {
        /* 0x00 */ f32 mSinkAcc;
        /* 0x04 */ f32 mSinkDec;
        /* 0x08 */ f32 mSinkMid;
        /* 0x0C */ s16 mSinkT0;
        /* 0x0E */ s16 mSinkT1;
        /* 0x10 */ f32 mSinkMidRate;
        /* 0x14 */ s16 mSinkT2;
        /* 0x16 */ s16 mSinkT3;
        /* 0x18 */ s16 mLowerTimer;
        /* 0x1A */ s16 mPad;
    };

    const Attr_c L_attr = {
        -0.8f,
        0.82f,
        0.5f,
        5,
        12,
        0.4f,
        65,
        74,
        50,
        0,
    };

    inline const Attr_c& attr() { return L_attr; }

    const f32 L_r_in = (125.0f / 3.0f) * std::sqrtf(3.0f);
    const f32 L_r_out = 2.0f * L_r_in;
}
}

const char daObjTribox::Act_c::M_arcname[] = "MtryB";

namespace daObjTribox {
cXyz Act_c::M_post[3] = {
    cXyz(0.0f, 0.0f, -L_r_out),
    cXyz(-125.0f, 0.0f, L_r_in),
    cXyz(125.0f, 0.0f, L_r_in),
};
}

dBgS_ObjLinChk daObjTribox::Act_c::M_lin;

#if VERSION == VERSION_DEMO
int daObjTribox::Act_c::M_correct_cnt;
bool daObjTribox::Act_c::M_correct_flag;
int daObjTribox::Act_c::M_b_cont_cnt;
int daObjTribox::Act_c::M_c_cont_cnt;
u8 daObjTribox::Act_c::M_sink_start;
cXyz daObjTribox::Act_c::M_sound_pos = cXyz::Zero;
#endif

/* 000000EC-000001A4       .text set_state__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::set_state() {
    int type = prm_get_type();
    bool sw = is_switch();
    if (type == 0) {
        if (sw) {
            mState = State_BLOCK_AFTER_e;
        } else {
            mState = State_BLOCK_BEFORE_e;
        }
    } else {
        if (sw) {
            mState = State_CORRECT_AFTER_e;
        } else {
            mState = State_CORRECT_BEFORE_e;
        }
    }
}

/* 000001A4-000001C8       .text solidHeapCB__Q211daObjTribox5Act_cFP10fopAc_ac_c */
BOOL daObjTribox::Act_c::solidHeapCB(fopAc_ac_c* i_this) {
    return static_cast<Act_c*>(i_this)->create_heap();
}

/* 000001C8-000004B0       .text create_heap__Q211daObjTribox5Act_cFv */
u8 daObjTribox::Act_c::create_heap() {
    J3DModelData* model_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_MTRYB_BDL_MTRYB_e);
    JUT_ASSERT(DEMO_SELECT(0x14e, 0x140), model_data != 0);

    mpModel = mDoExt_J3DModel__create(model_data, 0x80000, 0x11000022);

    cBgD_t* bgw_data = (cBgD_t*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_MTRYB_DZB_MTRYB_e);
    JUT_ASSERT(DEMO_SELECT(0x157, 0x149), bgw_data != 0);

    if (mpModel != NULL) {
        mpBgW = dBgW_NewSet(bgw_data, cBgW::MOVE_BG_e, &mpModel->getBaseTRMtx());
    } else {
        mpBgW = NULL;
    }

    J3DModelData* model_ytfbl_data = (J3DModelData*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_MTRYB_BDL_YTFBL00_e);
    JUT_ASSERT(DEMO_SELECT(0x167, 0x159), model_ytfbl_data != 0);

    mpYtfbl = mDoExt_J3DModel__create(model_ytfbl_data, 0x80000, 0x11000022);

    J3DAnmTevRegKey* brk_app_data = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_MTRYB_BRK_YTFBL00_APP_e);
    JUT_ASSERT(DEMO_SELECT(0x171, 0x163), brk_app_data != 0);

    BOOL brk_app = mBrkApp.init(model_ytfbl_data, brk_app_data, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, false, 0);

    J3DAnmTevRegKey* brk_cmn_data = (J3DAnmTevRegKey*)dComIfG_getObjectRes(M_arcname, dRes_INDEX_MTRYB_BRK_YTFBL00_CMN_e);
    JUT_ASSERT(DEMO_SELECT(0x17b, 0x16d), brk_cmn_data != 0);

    BOOL brk_cmn = mBrkCmn.init(model_ytfbl_data, brk_cmn_data, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, false, 0);

#if VERSION == VERSION_DEMO
    u8 success = false;
#else
    bool success = false;
#endif
    if (mpModel != NULL && mpBgW != NULL && mpYtfbl != NULL && brk_app && brk_cmn) {
        success = true;
    }
#if VERSION > VERSION_DEMO
    if (!success) {
        mpBgW = NULL;
    }
#endif
    return success;
}

/* 000004B0-000005C0       .text block_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::block_init() {
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    init_mtx();
    fopAcM_setCullSizeSphere(this, 0.0f, 125.0f, 0.0f, 200.0f);
    dComIfG_Bgsp()->Regist(mpBgW, this);
    mpBgW->SetCrrFunc(dBgS_MoveBGProc_TypicalRotY);
    mpBgW->Move();
    mpBgW->SetPushPullCallback(push_pullCB);
    M_correct_flag = false;
    mPPActive = 0;
    mFace = 0;
    mSide = 0;
    mPushPull = 0;
    mPPActivePrev = 0;
    mFacePrev = 0;
    mSidePrev = 0;
    mPushPullPrev = 0;
    mPPActiveWalk = 0;
    mFaceWalk = 0;
    mSideWalk = 0;
    mPushPullWalk = 0;
    mWalkPos = current.pos;
    mWalkRot = 0;
    mDir = 0;
    mSign = 1;
    mLightState = 0;
    mode_block_wait_init();
}

/* 000005C0-00000604       .text chk_light__Q211daObjTribox5Act_cCFv */
u8 daObjTribox::Act_c::chk_light() const {
    return dComIfGs_isEventBit(dSv_event_flag_c::MOVED_HYRULE_STATUE) ? 2 : 0;
}

/* 00000604-0000068C       .text correct_before_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::correct_before_init() {
    fopAcM_setCullSizeSphere(this, 0.0f, 125.0f, 0.0f, 200.0f);
    if (mControll) {
        mEventIdx = dComIfGp_evmng_getEventIdx("MtryB_sink", 0xFF);
    } else {
        mEventIdx = -1;
    }
    mode_correct_off_init();
}

/* 0000068C-00000748       .text correct_after_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::correct_after_init() {
    current.pos.y = -245.0f + home.pos.y;
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    init_mtx();
    fopAcM_setCullSizeSphere(this, 0.0f, 125.0f, 0.0f, 200.0f);
    dComIfG_Bgsp()->Regist(mpBgW, this);
    mpBgW->SetCrrFunc(NULL);
    mpBgW->Move();
    mpBgW->SetPushPullCallback(NULL);
    mLightState = chk_light();
    mode_correct_dummy_init();
}

/* 00000748-00000840       .text controll_set__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::controll_set() {
    if (fpcM_IsFirstCreating(this)) {
        if (prm_get_type() == 1) {
            fpcPi_Change(&base.base.mPi, fpcLy_CURRENT_e, 2, fpcPi_CURRENT_e);
            if (M_c_cont_cnt == 0) {
                mControll = 1;
                M_sink_start = 0;
                M_correct_flag = false;
#if VERSION > VERSION_JPN
                M_correct_cnt = 0;
#endif
            } else {
                mControll = 0;
            }
            M_c_cont_cnt++;
            sound_pos_init();
        } else {
            if (M_b_cont_cnt == 0) {
                mControll = 1;
            } else {
                mControll = 0;
            }
            M_b_cont_cnt++;
        }
    }
}

/* 00000840-00000904       .text controll_clear__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::controll_clear() {
    if (prm_get_type() == 1) {
        --M_c_cont_cnt;
        JUT_ASSERT(VERSION_SELECT(0x21D, 0x213, 0x214, 0x214), M_c_cont_cnt >= 0);
    } else {
        --M_b_cont_cnt;
        JUT_ASSERT(VERSION_SELECT(0x220, 0x216, 0x217, 0x217), M_b_cont_cnt >= 0);
    }
}

/* 00000904-00000980       .text create_block_before__Q211daObjTribox5Act_cFv */
cPhs_State daObjTribox::Act_c::create_block_before() {
    cPhs_State phase = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, solidHeapCB, DEMO_SELECT(0x4000, 0x11C0))) {
            block_init();
        } else {
            phase = cPhs_ERROR_e;
        }
    }
    return phase;
}

/* 00000980-00000988       .text create_block_after__Q211daObjTribox5Act_cFv */
cPhs_State daObjTribox::Act_c::create_block_after() {
    return cPhs_ERROR_e;
}

/* 00000988-000009AC       .text create_correct_before__Q211daObjTribox5Act_cFv */
cPhs_State daObjTribox::Act_c::create_correct_before() {
    correct_before_init();
    return cPhs_COMPLEATE_e;
}

/* 000009AC-00000A28       .text create_correct_after__Q211daObjTribox5Act_cFv */
cPhs_State daObjTribox::Act_c::create_correct_after() {
    cPhs_State phase = dComIfG_resLoad(&mPhase, M_arcname);
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(this, solidHeapCB, 0x4000)) {
            correct_after_init();
        } else {
            phase = cPhs_ERROR_e;
        }
    }
    return phase;
}

/* 00000A28-00000BC4       .text _create__Q211daObjTribox5Act_cFv */
cPhs_State daObjTribox::Act_c::_create() {
    fopAcM_SetupActor(this, Act_c);
    set_state();
    controll_set();

    typedef cPhs_State (Act_c::*CreateFunc)();
    static CreateFunc proc[] = {
        &Act_c::create_block_before,
        &Act_c::create_block_after,
        &Act_c::create_correct_before,
        &Act_c::create_correct_after,
    };
    return (this->*proc[mState])();
}

/* 00000CA4-00000D68       .text _delete__Q211daObjTribox5Act_cFv */
bool daObjTribox::Act_c::_delete() {
    sound_pos_delete();
    controll_clear();
#if VERSION > VERSION_DEMO
    eff_smoke_remove();
    eff_sink_smoke_remove();
#endif
    if (mState == State_BLOCK_BEFORE_e || mState == State_CORRECT_AFTER_e) {
#if VERSION == VERSION_DEMO
        eff_smoke_remove();
        eff_sink_smoke_remove();
#endif
        if (mpBgW != NULL) {
            mpBgW->SetPushPullCallback(NULL);
        }
        if (mpBgW != NULL && mpBgW->ChkUsed()) {
            dComIfG_Bgsp()->Release(mpBgW);
        }
        dComIfG_resDelete(&mPhase, M_arcname);
    }
    return true;
}

/* 00000D68-00000DFC       .text set_mtx__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::set_mtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoMtx_stack_c::now[1][3] += 251.0f;
    mpYtfbl->setBaseTRMtx(mDoMtx_stack_c::get());
}

/* 00000DFC-00000E54       .text init_mtx__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::init_mtx() {
    mpModel->setBaseScale(scale);
    mpYtfbl->setBaseScale(scale);
    set_mtx();
}

/* 00000E54-00000F8C       .text push_pullCB__Q211daObjTribox5Act_cFP10fopAc_ac_cP10fopAc_ac_csQ24dBgW13PushPullLabel */
fopAc_ac_c* daObjTribox::Act_c::push_pullCB(fopAc_ac_c* actor, fopAc_ac_c* other, s16 angle, dBgW::PushPullLabel i_pp_label) {
    Act_c* i_this = static_cast<Act_c*>(actor);
    dBgW::PushPullLabel pp_label = cLib_checkBit(i_pp_label, (dBgW::PushPullLabel)(dBgW::PPLABEL_PUSH | dBgW::PPLABEL_PULL));
    if (pp_label) {
        const int pp_field = dBgW::PPLABEL_PUSH | dBgW::PPLABEL_PULL;
        JUT_ASSERT(VERSION_SELECT(0x2B9, 0x2AF, 0x2B0, 0x2B0), pp_label != pp_field);

        if (cLib_checkBit(pp_label, dBgW::PPLABEL_PUSH)) {
            i_this->mPushPull = 0;
        } else {
            i_this->mPushPull = 1;
        }

        s16 diff = (s16)(angle - 0x8000) - i_this->shape_angle.y;
        if (diff >= -0x2AAA && diff < 0x2AAA) {
            i_this->mFace = 0;
        } else if (diff >= 0x2AAA) {
            i_this->mFace = 1;
        } else {
            i_this->mFace = 2;
        }

        static const s16 face_ang_offset[] = {0, 0x5555, (s16)0xAAAB};
        s16 search = fopAcM_searchActorAngleY(actor, other);
        s16 face_ang = i_this->shape_angle.y + face_ang_offset[i_this->mFace];
        if ((s16)(face_ang - search) >= 0) {
            i_this->mSide = 0;
        } else {
            i_this->mSide = 1;
        }
        i_this->mPPActive = 1;
    }
    return actor;
}

/* 00000F8C-00000FF4       .text line_cross__Q211daObjTribox5Act_cCFPC4cXyzPC4cXyz */
bool daObjTribox::Act_c::line_cross(const cXyz* p0, const cXyz* p1) const {
    fopAc_ac_c* actor = const_cast<Act_c*>(this);
    M_lin.Set(const_cast<cXyz*>(p0), const_cast<cXyz*>(p1), actor);
    M_lin.SetActorPid(fopAcM_GetID(actor));
    return dComIfG_Bgsp()->LineCross(&M_lin);
}

/* 00000FF4-000012A0       .text chk_wall__Q211daObjTribox5Act_cCFi */
bool daObjTribox::Act_c::chk_wall(int num) const {
    JUT_ASSERT(VERSION_SELECT(0x315, 0x30B, 0x30C, 0x30C), (num == 1) || (num == 2));

    int idx;
    if (mSign >= 0) {
        idx = (mDir + 1) % 3;
    } else {
        idx = (mDir + 2) % 3;
    }

    mDoMtx_stack_c::YrotS(shape_angle.y);
    cXyz dir;
    mDoMtx_stack_c::multVecSR(&M_post[idx], &dir);

    cXyz start(current.pos.x, 5.0f + current.pos.y, current.pos.z);
    cXyz end = start - dir * 0.99f;
    if (line_cross(&start, &end)) {
        return true;
    }

    if (num == 2) {
        start = start - dir;
        s16 rot = mSign >= 0 ? 0x2AAA : (s16)-0x2AAA;
        mDoMtx_YrotM(mDoMtx_stack_c::now, rot);
        mDoMtx_stack_c::multVecSR(&M_post[idx], &dir);
        end = start - dir * 0.49f;
        if (line_cross(&start, &end)) {
            return true;
        }
        start.y += 30.0f;
        end = start - dir * 0.99f;
        if (line_cross(&start, &end)) {
            return true;
        }
    }
    return false;
}

/* 000012A0-000012E8       .text chk_space__Q211daObjTribox5Act_cCFv */
bool daObjTribox::Act_c::chk_space() const {
    int n = mPushPullWalk == 1 ? 1 : 0;
    if (chk_wall(n + 1)) {
        return false;
    } else {
        return true;
    }
}

/* 000012E8-00001378       .text eff_flash__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_flash() {
    cXyz pos = current.pos;
    pos.y += 251.0f;
    dComIfGp_particle_setP1(dPa_name::ID_AK_SN_TRIFORCEBLOCKFLASH00, &pos, &shape_angle);
}

/* 00001378-00001464       .text eff_smoke_start__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_smoke_start() {
    static cXyz scl(0.6f, 0.6f, 0.6f);
    eff_smoke_pos();
    JPABaseEmitter* emitter = dComIfGp_particle_setToon(
        dPa_name::ID_AK_JT_ELEMENTSMOKE00, &mSmokePos, &mSmokeAngle, &scl, 0xB9, &mSmokeCb, fopAcM_GetRoomNo(this)
    );
    if (emitter != NULL) {
        emitter->setRate(1.0f);
        emitter->setDirectionalSpeed(15.0f);
        emitter->setSpread(0.15f);
        emitter->setLifeTime(30);
    }
}

/* 00001464-000015C8       .text eff_smoke_pos__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_smoke_pos() {
    int i0;
    int i1;
    if (mSign >= 0) {
        i0 = mDir + 1;
        i1 = mDir + 2;
    } else {
        i0 = mDir + 2;
        i1 = mDir + 1;
    }
    i0 %= 3;
    i1 %= 3;

    mDoMtx_stack_c::YrotS(shape_angle.y);
    cXyz v0;
    mDoMtx_stack_c::multVecSR(&M_post[i0], &v0);
    cXyz v1;
    mDoMtx_stack_c::multVecSR(&M_post[i1], &v1);
    cXyz dir = v1 - v0;
    dir.normalizeZP();
    dir *= 10.0f;
    mSmokePos = v0 + dir + current.pos;
    mSmokeAngle.x = 0;
    mSmokeAngle.y = cLib_targetAngleY(&v0, &v1);
    mSmokeAngle.z = 0;
}

/* 000015C8-000015F4       .text eff_smoke_end__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_smoke_end() {
    mSmokeCb.end();
}

/* 000015F4-00001620       .text eff_smoke_remove__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_smoke_remove() {
    mSmokeCb.remove();
}

/* 00001620-0000162C       .text eff_sink_smoke_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_sink_smoke_init() {
    mSinkSmokeState = 0;
}

/* 0000162C-00001740       .text eff_sink_smoke_start__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_sink_smoke_start() {
    if (mSinkSmokeState == 0) {
        mSinkSmokeState = 1;
        dPa_levelEcallBack* cbs[3] = { &mSinkSmoke0, &mSinkSmoke1, &mSinkSmoke2 };
        csXyz dummy = shape_angle;
        for (int i = 0; i < 3; i++) {
            csXyz ang = shape_angle;
            ang.y += i * 0x5555;
            dComIfGp_particle_setToon(dPa_name::ID_AK_ST_TRIFORCEBLOCKSMOKE00, &current.pos, &ang, NULL, 0xA0, cbs[i]);
        }
    }
}

/* 00001740-000017B8       .text eff_sink_smoke_end__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_sink_smoke_end() {
    if (mSinkSmokeState != 0) {
        mSinkSmokeState = 0;
        mSinkSmoke0.end();
        mSinkSmoke1.end();
        mSinkSmoke2.end();
    }
}

/* 000017B8-0000181C       .text eff_sink_smoke_remove__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::eff_sink_smoke_remove() {
    mSinkSmoke0.remove();
    mSinkSmoke1.remove();
    mSinkSmoke2.remove();
}

/* 0000181C-00001828       .text vib_sink_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::vib_sink_init() {
    mVibState = 0;
}

/* 00001828-000018FC       .text vib_sink_start__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::vib_sink_start() {
    if (mVibState == 0) {
        mVibState = 1;
        dVibration_c& vibration = dComIfGp_getVibration();
        vibration.StartShock(2, 1, cXyz(0.0f, 1.0f, 0.0f));
        vibration.StartShock(1, 6, cXyz(0.0f, 1.0f, 0.0f));
        vibration.StartQuake(3, 0xF, cXyz(0.0f, 1.0f, 0.0f));
    }
}

/* 000018FC-0000197C       .text vib_sink_end__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::vib_sink_end() {
    if (mVibState != 0) {
        mVibState = 0;
        dVibration_c& vib = dComIfGp_getVibration();
        vib.StopQuake(-1);
        vib.StartShock(3, 0xF, cXyz(0.0f, 1.0f, 0.0f));
    }
}

/* 0000197C-00001A08       .text search_block__Q211daObjTribox5Act_cFPvPv */
void* daObjTribox::Act_c::search_block(void* ptr, void* data) {
    Act_c* other = static_cast<Act_c*>(ptr);
    Act_c* self = static_cast<Act_c*>(data);
    if (fopAc_IsActor(other) && fopAcM_GetName(other) == fpcNm_Obj_Tribox_e) {
        if (other->prm_get_type() == 0) {
            if (fopAcM_searchActorDistance2(self, other) < 225.0f) {
                return other;
            }
        }
    }
    return NULL;
}

/* 00001A08-00001A90       .text sound_pos_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::sound_pos_init() {
    if (mControll) {
        M_sound_pos = home.pos * (1.0f / 3.0f);
    } else {
        M_sound_pos += home.pos * (1.0f / 3.0f);
    }
}

/* 00001A90-00001AC4       .text sound_pos_delete__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::sound_pos_delete() {
    mDoAud_seDeleteObject(&M_sound_pos);
}

/* 00001AC4-00001D0C       .text sound_get_mapinfo__Q211daObjTribox5Act_cFRC4cXyz */
u32 daObjTribox::Act_c::sound_get_mapinfo(const cXyz& pos) {
    dBgS_ObjGndChk gndChk;
    cXyz p = pos;
    p.y += 50.0f;
    gndChk.SetPos(&p);
    dComIfG_Bgsp()->GroundCross(&gndChk);
    int idx = gndChk.GetBgIndex();
    u32 result = 0xD;
    if (idx >= 0 && idx < 0x100) {
        result = dComIfG_Bgsp()->GetMtrlSndId(gndChk);
    }
    return result;
}

/* 000021CC-00002268       .text sound_sink_down_block__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::sound_sink_down_block() {
    if (mControll) {
        mDoAud_seStart(JA_SE_OBJ_TRIF_BL_GO_DOWN, &M_sound_pos, sound_get_mapinfo(M_sound_pos), dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 00002268-00002304       .text sound_sink_stop_block__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::sound_sink_stop_block() {
    if (mControll) {
        mDoAud_seStart(JA_SE_OBJ_TRIF_BL_STOP, &M_sound_pos, sound_get_mapinfo(M_sound_pos), dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 00002304-000023A0       .text sound_flash_shine__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::sound_flash_shine() {
    if (mControll) {
        mDoAud_seStart(JA_SE_OBJ_TRIF_FL_SHINE, &M_sound_pos, sound_get_mapinfo(M_sound_pos), dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 000023A0-0000243C       .text sound_flash_light__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::sound_flash_light() {
    if (mControll) {
        mDoAud_seStart(JA_SE_OBJ_TRIF_FL_LIGHT, &M_sound_pos, sound_get_mapinfo(M_sound_pos), dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 0000243C-00002458       .text mode_block_wait_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_wait_init() {
    fopAcM_OnStatus(this, fopAcStts_NOCULLEXEC_e);
    mPushCount = 0;
    mMode = Mode_BLOCK_WAIT_e;
}

/* 00002458-000025C0       .text mode_block_wait__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_wait() {
    if (M_correct_flag) {
        mPushCount = 0;
        mode_block_sink_init();
        return;
    }

    if (mPPActive && mFace == mFacePrev && mSide == mSidePrev && mPushPull == mPushPullPrev) {
        mPushCount++;
    } else {
        mPushCount = 0;
    }

    if (mPushCount >= 4) {
        if ((mSide == 0 && mPushPull == 1) || (mSide == 1 && mPushPull == 0)) {
            mSign = 1;
        } else {
            mSign = -1;
        }
        if (mSide == 0) {
            mDir = (mFace + 2) % 3;
        } else {
            mDir = (mFace + 1) % 3;
        }
        mPPActiveWalk = mPPActive;
        mFaceWalk = mFace;
        mSideWalk = mSide;
        mPushPullWalk = mPushPull;
        if (chk_space()) {
            mode_block_walk_init();
        }
    }
}

/* 000025C0-0000262C       .text mode_block_walk_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_walk_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_BLOCK_WALK_e;
    mTimer = 0x14;
    mWalkPos = current.pos;
    eff_smoke_start();
    daPy_getPlayerActorClass()->onPushPullKeep();
}

/* 0000262C-00002B34       .text mode_block_walk__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_walk() {
    bool done = --mTimer <= 0;

    f32 c = (f32)cos(0.157079637f * mTimer);
    f32 s = (f32)mSign * (0.5f * (1.0f + c));
    f32 rot0 = cM_s2rad(home.angle.y) + 1.04719758f * mWalkRot;
    f32 rot1 = 1.04719758f * s;
    MTXRotRad(mDoMtx_stack_c::now, 'Y', rot0);
    cXyz v0;
    mDoMtx_stack_c::multVecSR(&M_post[mDir], &v0);
    mDoMtx_stack_c::rYrotM(rot1);
    cXyz v1;
    mDoMtx_stack_c::multVecSR(&M_post[mDir], &v1);
    current.pos = mWalkPos + v0 - v1;
    shape_angle.y = 0.5f + 10430.378f * (rot0 + rot1);

    if (done) {
        eff_smoke_end();
    } else {
        eff_smoke_pos();
    }

    if (done) {
        daPy_getPlayerActorClass()->offPushPullKeep();
        mWalkRot = (mWalkRot + mSign) % 6;
        if (chk_wall(1)) {
            u32 mapinfo = dComIfG_Bgsp()->GetMtrlSndId(M_lin);
            mDoAud_seStart(JA_SE_LK_MOVE_ROCK_LIMIT, &eyePos, mapinfo, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        }
        mode_block_wait_init();
    } else {
        dBgS_ObjGndChk gndChk;
        cXyz p(current.pos.x, current.pos.y + 50.0f, current.pos.z);
        gndChk.SetPos(&p);
        dComIfG_Bgsp()->GroundCross(&gndChk);
        int idx = gndChk.GetBgIndex();
        u32 mapinfo = 0;
        if (idx >= 0 && idx < 0x100) {
            mapinfo = dComIfG_Bgsp()->GetMtrlSndId(gndChk);
        }
        mDoAud_seStart(JA_SE_LK_MOVE_ROCK, &eyePos, mapinfo, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }
}

/* 00002B34-00002B90       .text mode_block_sink_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_sink_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_BLOCK_SINK_e;
    mSinkVel = 0.0f;
    mTimer = 0;
    eff_sink_smoke_init();
    vib_sink_init();
}

/* 00002B90-00002CE4       .text mode_block_sink__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_sink() {
    if (M_sink_start) {
        eff_sink_smoke_start();
        vib_sink_start();
        f32 dest_y = -245.0f + home.pos.y;
        mSinkVel += attr().mSinkAcc;
        if (mTimer >= attr().mSinkT0 && mTimer < attr().mSinkT1) {
            mSinkVel *= attr().mSinkMid;
        } else if (mTimer >= attr().mSinkT2 && mTimer < attr().mSinkT3) {
            mSinkVel *= attr().mSinkMidRate;
        } else {
            mSinkVel *= attr().mSinkDec;
        }
        current.pos.y += mSinkVel;
        if (current.pos.y < dest_y) {
            current.pos.y = dest_y;
            eff_sink_smoke_end();
            vib_sink_end();
            sound_sink_stop_block();
            mode_block_lower_init();
        } else {
            sound_sink_down_block();
            mTimer++;
        }
    } else {
        mSinkVel = 0.0f;
        mTimer = 0;
    }
}

/* 00002CE4-00002D14       .text mode_block_lower_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_lower_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_BLOCK_LOWER_e;
    mTimer = attr().mLowerTimer;
    mFlashTimer = 0x5E;
}

/* 00002D14-00002DA8       .text mode_block_lower__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_block_lower() {
    if (mTimer > 0) {
        mTimer--;
        if (mTimer == 0) {
            eff_flash();
            mLightState = 1;
        }
    }
    if (mTimer == 0) {
        if (mFlashTimer > 0) {
            mFlashTimer--;
            if (mFlashTimer == 0) {
                sound_flash_shine();
            }
        }
    } else {
        mFlashTimer = 0x5E;
    }
}

/* 00002DA8-00002DC0       .text mode_correct_off_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_off_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_CORRECT_OFF_e;
}

/* 00002DC0-00002E1C       .text mode_correct_off__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_off() {
    if (fopAcIt_Judge(search_block, this) != NULL) {
        M_correct_cnt++;
        mode_correct_on_init();
    }
}

/* 00002E1C-00002E34       .text mode_correct_on_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_on_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_CORRECT_ON_e;
}

/* 00002E34-00002EEC       .text mode_correct_on__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_on() {
    if (fopAcIt_Judge(search_block, this) != NULL) {
        if (mControll) {
            if (M_correct_cnt >= 3) {
                M_correct_flag = true;
            }
        }
        if (M_correct_flag) {
            if (mControll) {
                mode_correct_demoreq_init();
            } else {
                mode_correct_end_init();
            }
        }
    } else {
        M_correct_cnt--;
        mode_correct_off_init();
    }
}

/* 00002EEC-00002F0C       .text mode_correct_demoreq_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_demoreq_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_CORRECT_DEMOREQ_e;
    mTimer = 0xA;
}

/* 00002F0C-00002FB4       .text mode_correct_demoreq__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_demoreq() {
    bool next = true;
    if (dComIfGp_evmng_existence(mEventIdx)) {
        if (eventInfo.checkCommandDemoAccrpt()) {
            mDemoAccept = 1;
        } else {
            fopAcM_orderOtherEventId(this, mEventIdx, 0xFF, 0xFFFF, 0, 1);
            eventInfo.onCondition(dEvtCnd_UNK2_e);
            next = false;
        }
    }
    if (next) {
        mode_correct_demorun_init();
    }
}

/* 00002FB4-00003028       .text mode_correct_demorun_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_demorun_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_CORRECT_DEMORUN_e;
    mTimer = 0x32;
    mDoAud_seStart(JA_SE_READ_RIDDLE_1);
}

/* 00003028-00003104       .text mode_correct_demorun__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_demorun() {
    if (mTimer > 0) {
        mTimer--;
        if (mTimer == 0) {
            on_switch();
            M_sink_start = 1;
        }
    } else {
        bool next = true;
        if (mDemoAccept) {
            if (dComIfGp_evmng_endCheck(mEventIdx)) {
                dComIfGp_event_reset();
                mDemoAccept = 0;
            } else {
                next = false;
            }
        }
        if (next) {
            mode_correct_end_init();
        }
    }
}

/* 00003104-0000311C       .text mode_correct_end_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_end_init() {
    fopAcM_OnStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_CORRECT_END_e;
}

/* 0000311C-00003120       .text mode_correct_end__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_end() {}

/* 00003120-00003138       .text mode_correct_dummy_init__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_dummy_init() {
    fopAcM_OffStatus(this, fopAcStts_NOCULLEXEC_e);
    mMode = Mode_CORRECT_DUMMY_e;
}

/* 00003138-0000313C       .text mode_correct_dummy__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_correct_dummy() {}

/* 0000313C-00003288       .text mode_proc_call__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::mode_proc_call() {
    typedef void (Act_c::*ModeFunc)();
    static ModeFunc mode_proc[] = {
        &Act_c::mode_block_wait,
        &Act_c::mode_block_walk,
        &Act_c::mode_block_sink,
        &Act_c::mode_block_lower,
        &Act_c::mode_correct_off,
        &Act_c::mode_correct_on,
        &Act_c::mode_correct_demoreq,
        &Act_c::mode_correct_demorun,
        &Act_c::mode_correct_end,
        &Act_c::mode_correct_dummy,
    };
    (this->*mode_proc[mMode])();
}

/* 00003288-00003304       .text execute_block__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::execute_block() {
    mode_proc_call();
    mPPActivePrev = mPPActive;
    mFacePrev = mFace;
    mSidePrev = mSide;
    mPushPullPrev = mPushPull;
    mPPActive = 0;
    eyePos = current.pos;
    set_mtx();
    mpBgW->Move();
}

/* 00003304-00003324       .text execute_correct__Q211daObjTribox5Act_cFv */
void daObjTribox::Act_c::execute_correct() {
    mode_proc_call();
}

/* 00003324-000033D8       .text _execute__Q211daObjTribox5Act_cFv */
bool daObjTribox::Act_c::_execute() {
    if (prm_get_type() == 0) {
        execute_block();
    } else {
        execute_correct();
    }
    if (mpYtfbl != NULL) {
        if (mLightState == 1) {
            if (mBrkApp.play()) {
                mLightState = 2;
            }
        } else if (mLightState == 2) {
            mBrkCmn.play();
        }
        if (mLightState == 1 || mLightState == 2) {
            sound_flash_light();
        }
    }
    return true;
}

/* 000033D8-00003538       .text _draw__Q211daObjTribox5Act_cFv */
bool daObjTribox::Act_c::_draw() {
    if (mState == State_BLOCK_BEFORE_e || mState == State_CORRECT_AFTER_e) {
        g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &current.pos, &tevStr);
        g_env_light.setLightTevColorType(mpModel, &tevStr);
        if (mLightState != 0) {
            g_env_light.setLightTevColorType(mpYtfbl, &tevStr);
            if (mLightState == 1) {
                mBrkApp.entry(mpYtfbl->getModelData());
            } else {
                mBrkCmn.entry(mpYtfbl->getModelData());
            }
        }
        if (mLightState != 0) {
            dComIfGd_setListP1();
            mDoExt_modelUpdateDL(mpYtfbl);
            dComIfGd_setList();
        }
        dComIfGd_setListBG();
        mDoExt_modelUpdateDL(mpModel);
        dComIfGd_setList();
    }
    return true;
}

namespace daObjTribox {
namespace {
/* 00003538-00003558       .text Mthd_Create__Q211daObjTribox28@unnamed@d_a_obj_tribox_cpp@FPv */
cPhs_State Mthd_Create(void* i_this) {
    return ((daObjTribox::Act_c*)i_this)->_create();
}

/* 00003558-0000357C       .text Mthd_Delete__Q211daObjTribox28@unnamed@d_a_obj_tribox_cpp@FPv */
BOOL Mthd_Delete(void* i_this) {
    return ((daObjTribox::Act_c*)i_this)->_delete();
}

/* 0000357C-000035A0       .text Mthd_Execute__Q211daObjTribox28@unnamed@d_a_obj_tribox_cpp@FPv */
BOOL Mthd_Execute(void* i_this) {
    return ((daObjTribox::Act_c*)i_this)->_execute();
}

/* 000035A0-000035C4       .text Mthd_Draw__Q211daObjTribox28@unnamed@d_a_obj_tribox_cpp@FPv */
BOOL Mthd_Draw(void* i_this) {
    return ((daObjTribox::Act_c*)i_this)->_draw();
}

/* 000035C4-000035CC       .text Mthd_IsDelete__Q211daObjTribox28@unnamed@d_a_obj_tribox_cpp@FPv */
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
}; // namespace daObjTribox

actor_process_profile_definition g_profile_Obj_Tribox = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Tribox_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjTribox::Act_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Tribox_e,
    /* Actor SubMtd */ &daObjTribox::Mthd_Table,
    /* Status       */ fopAcStts_CULL_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLSPHERE_CUSTOM_e,
};
