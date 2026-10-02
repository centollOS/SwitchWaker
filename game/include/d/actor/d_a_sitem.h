#ifndef D_A_SITEM_H
#define D_A_SITEM_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_bg_s_acch.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"

struct sitem_s {
    cXyz pos;
    cXyz speed;
    f32 size;
};

class sitem_class : public fopAc_ac_c {
public:
    /* 0x290 */ u8 mUnknown[0x1C];
    /* 0x2AC */ request_of_phase_process_class mPhase;
    /* 0x2B4 */ J3DModel* mpModel;
    /* 0x2B8 */ u8 mType;
    /* 0x2B9 */ u8 mLengthType;
    /* 0x2BA */ u8 mItemBit;
    /* 0x2BB */ u8 mItemTable;
    /* 0x2BC */ s16 mFrame;
    /* 0x2BE */ s16 mUnknown2BE;
    /* 0x2C0 */ s16 mMode;
    /* 0x2C2 */ s16 mTimers[2];
    /* 0x2C6 */ s16 mHitTimer;
    /* 0x2C8 */ cXyz mPos;
    /* 0x2D4 */ cXyz mOldPos;
    /* 0x2E0 */ cXyz mModelPos;
    /* 0x2EC */ csXyz mModelAngle;
    /* 0x2F4 */ f32 mSegmentLength;
    /* 0x2F8 */ f32 mUnknown2F8;
    /* 0x2FC */ f32 mExtension;
    /* 0x300 */ f32 mWave;
    /* 0x304 */ f32 mGroundY;
    /* 0x308 */ f32 mCutLength;
    /* 0x30C */ mDoExt_3DlineMat0_c mLine;
    /* 0x328 */ sitem_s mSegments[10];
    /* 0x440 */ mDoExt_3DlineMat0_c mCutLine;
    /* 0x45C */ sitem_s mCutSegments[5];
    /* 0x4E8 */ dCcD_Stts mStts;
    /* 0x524 */ dCcD_Sph mSph[4];
    /* 0x9D4 */ dCcD_Sph mBoomerangSph;
    /* 0xB00 */ cXyz mCollisionPos;
    /* 0xB0C */ f32 mHitSpeed;
    /* 0xB10 */ s16 mHitAngle;
    /* 0xB14 */ dBgS_AcchCir mAcchCir;
    /* 0xB54 */ dBgS_ObjAcch mAcch;
#if VERSION == VERSION_DEMO
    /* 0xD18 */ JPABaseEmitter* mpSmokeEmitter[2];
#endif
    /* 0xD18 */ dPa_followEcallBack mSmoke[2];
    /* 0xD40 */ csXyz mSmokeAngle[2];
    /* 0xD4C */ s8 mSmokeActive;
};

STATIC_ASSERT(sizeof(sitem_class) == DEMO_SELECT(0xD58, 0xD50));
#endif /* D_A_SITEM_H */
