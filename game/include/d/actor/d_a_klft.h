#ifndef D_A_KLFT_H
#define D_A_KLFT_H

#include "f_op/f_op_actor.h"
#include "d/d_bg_w.h"
#include "d/d_cc_d.h"
#include "m_Do/m_Do_ext.h"

class klft_class : public fopAc_ac_c {
public:
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ s16 mCounter;
    /* 0x29C */ J3DModel* mpModel;
    /* 0x2A0 */ mDoExt_McaMorf* mpPulley[2];
    /* 0x2A8 */ cXyz mPulleyPos[2];
    /* 0x2C0 */ s16 mPulleyAngle;
    /* 0x2C2 */ u8 mType;
    /* 0x2C3 */ u8 mSwitch;
    /* 0x2C4 */ cXyz mShake;
    /* 0x2D0 */ cXyz mSway;
    /* 0x2DC */ cXyz mSwayTarget;
    /* 0x2E8 */ cXyz mTiltTarget;
    /* 0x2F4 */ cXyz mTilt;
    /* 0x300 */ csXyz mShakeAngle;
    /* 0x306 */ u8 mUnused306[6];
    /* 0x30C */ Mtx mBgMtx;
    /* 0x33C */ dBgW* pm_bgw;
    /* 0x340 */ mDoExt_3DlineMat1_c mRope;
    /* 0x37C */ cXyz mEnds[2];
    /* 0x394 */ cXyz mPathDelta;
    /* 0x3A0 */ cXyz mRidePos;
    /* 0x3AC */ s16 mRideAngle;
    /* 0x3AE */ s16 mRideTilt;
    /* 0x3B0 */ s16 mPlatformAngle;
    /* 0x3B2 */ s16 mWindTimer;
    /* 0x3B4 */ f32 mMoveSoundTimer;
    /* 0x3B8 */ f32 mSink;
    /* 0x3BC */ f32 mSinkTarget;
    /* 0x3C0 */ f32 mProgress;
    /* 0x3C4 */ f32 mMoveSpeed;
    /* 0x3C8 */ f32 mMoveSpeedTarget;
    /* 0x3CC */ dCcD_Stts mStts;
    /* 0x408 */ dCcD_Sph mWindSph;
    /* 0x534 */ dCcD_Cyl mPulleyCyl[2];
    /* 0x794 */ dCcD_Sph mPulleySph[2];
    /* 0x9EC */ s16 mRideTimer;
};

STATIC_ASSERT(sizeof(klft_class) == 0x9F0);

#endif /* D_A_KLFT_H */
