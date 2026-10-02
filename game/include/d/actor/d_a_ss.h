#ifndef D_A_SS_H
#define D_A_SS_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "m_Do/m_Do_ext.h"

struct ss_s_s {
    cXyz pos;
    u8 size;
};

struct ss_s {
    /* 0x000 */ u8 mode;
    /* 0x004 */ cXyz pos;
    /* 0x010 */ csXyz angle;
    /* 0x018 */ cXyz speed;
    /* 0x024 */ f32 gravity;
    /* 0x028 */ f32 wave;
    /* 0x02C */ f32 sink;
    /* 0x030 */ s16 phase;
    /* 0x032 */ s16 frequency;
    /* 0x034 */ s16 rotation;
    /* 0x036 */ s16 cutTimer;
    /* 0x038 */ s16 groundTimer;
    /* 0x03A */ s8 length;
    /* 0x03B */ s8 burning;
    /* 0x03C */ dCcD_Sph sph[4];
    /* 0x4EC */ ss_s_s segments[20];
};

class ss_class : public fopAc_ac_c {
public:
    /* 0x0290 */ u8 mUnknown[0x1C];
    /* 0x02AC */ request_of_phase_process_class mPhase;
    /* 0x02B4 */ s8 mClosed;
    /* 0x02B8 */ mDoExt_McaMorf* mpMorf;
    /* 0x02BC */ u8 mWall;
    /* 0x02BD */ u8 mType;
    /* 0x02BE */ u8 mSwitch;
    /* 0x02C0 */ s16 mFrame;
    /* 0x02C2 */ s16 mUnknown2C2;
    /* 0x02C4 */ s16 mMode;
    /* 0x02C6 */ s16 mPreviousMode;
    /* 0x02C8 */ s16 mTimers[4];
    /* 0x02D0 */ s16 mHitTimer;
    /* 0x02D2 */ csXyz mEyeAngle;
    /* 0x02D8 */ s8 mShake;
    /* 0x02DC */ ss_s mHands[10];
    /* 0x4094 */ mDoExt_3DlineMat0_c mLine;
    /* 0x40B0 */ dCcD_Stts mStts;
    /* 0x40EC */ dCcD_Sph mSph;
};

STATIC_ASSERT(sizeof(ss_s) == 0x62C);
STATIC_ASSERT(sizeof(ss_class) == 0x4218);
#endif /* D_A_SS_H */
