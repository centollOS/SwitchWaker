#ifndef D_A_SSS_H
#define D_A_SSS_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "m_Do/m_Do_ext.h"

class JPABaseEmitter;

struct sss_s {
    /* 0x00 */ cXyz mPos;
    /* 0x0C */ cXyz m0C;
    /* 0x18 */ f32 mSize;
};

class sss_class : public fopAc_ac_c {
public:
    /* 0x290 */ u8 m290[0x1C];
    /* 0x2AC */ request_of_phase_process_class mPhase;
    /* 0x2B4 */ mDoExt_McaMorf* mpMorf;
    /* 0x2B8 */ u8 mType;
    /* 0x2B9 */ u8 mRange;
    /* 0x2BA */ u8 mSwitch;
    /* 0x2BC */ s16 mFrame;
    /* 0x2BE */ s16 m2BE;
    /* 0x2C0 */ s16 mAction;
    /* 0x2C2 */ s16 mTimers[2];
    /* 0x2C6 */ s16 mInvulnerabilityTimer;
    /* 0x2C8 */ cXyz mEndPos;
    /* 0x2D4 */ cXyz mHandPos;
    /* 0x2E0 */ csXyz mHandAngle;
    /* 0x2E8 */ f32 mSegmentLength;
    /* 0x2EC */ f32 m2EC;
    /* 0x2F0 */ f32 mStretch;
    /* 0x2F4 */ f32 mSway;
    /* 0x2F8 */ f32 mGroundY;
    /* 0x2FC */ f32 mCutLength;
    /* 0x300 */ mDoExt_3DlineMat1_c mLine;
    /* 0x33C */ sss_s mSegments[10];
    /* 0x454 */ mDoExt_3DlineMat1_c mCutLine;
    /* 0x490 */ sss_s mCutSegments[5];
    /* 0x51C */ dCcD_Stts mStts;
    /* 0x558 */ dCcD_Sph mSpheres[3];
    /* 0x8DC */ dCcD_Sph mBoomerangSphere;
    /* 0xA08 */ f32 mKnockback;
    /* 0xA0C */ s16 mKnockbackAngle;
    /* 0xA10 */ JPABaseEmitter* mpCutEmitter;
    /* 0xA14 */ JPABaseEmitter* mpEndEmitter;
    /* 0xA18 */ s8 mCutEffect;
};

STATIC_ASSERT(sizeof(sss_s) == 0x1C);
STATIC_ASSERT(sizeof(sss_class) == 0xA1C);

#endif /* D_A_SSS_H */
