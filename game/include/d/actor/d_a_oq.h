#ifndef D_A_OQ_H
#define D_A_OQ_H

#include "f_op/f_op_actor.h"
#include "c/c_damagereaction.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/d_bg_s_acch.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"

class mDoExt_McaMorf;
class mDoExt_brkAnm;
class JntHit_c;

class oq_class : public fopEn_enemy_c {
public:
    /* 0x2AC */ request_of_phase_process_class mPhase;
    /* 0x2B4 */ mDoExt_McaMorf* mpMorf;
    /* 0x2B8 */ u8 m2B8[0x2BC - 0x2B8];
    /* 0x2BC */ mDoExt_brkAnm* mpBrk;
    /* 0x2C0 */ u8 mType;
    /* 0x2C1 */ u8 m2C1;
    /* 0x2C2 */ u8 m2C2;
    /* 0x2C3 */ u8 mAction;
    /* 0x2C4 */ u8 mMode;
    /* 0x2C5 */ u8 mHitType;
    /* 0x2C6 */ u8 m2C6;
    /* 0x2C7 */ u8 mMadeWaterSplash;
    /* 0x2C8 */ u8 m2C8;
    /* 0x2C9 */ u8 m2C9;
    /* 0x2CA */ s16 mTimers[12];
    /* 0x2E2 */ s16 mTargetAngleY;
    /* 0x2E4 */ s16 mBobAngle;
    /* 0x2E6 */ s16 m2E6;
    /* 0x2E8 */ s16 m2E8;
    /* 0x2EC */ fpc_ProcID mChildIds[6];
    /* 0x304 */ int mBckIdx;
    /* 0x308 */ u8 m308[0x30C - 0x308];
    /* 0x30C */ f32 m30C;
    /* 0x310 */ f32 mCorrectionOffsetY;
    /* 0x314 */ f32 mSearchRange;
    /* 0x318 */ f32 mBrkFrame;
    /* 0x31C */ f32 mSplashScale;
    /* 0x320 */ f32 mDrawOffsetY;
    /* 0x324 */ fpc_ProcID mRelatedId;
    /* 0x328 */ cXyz mMouthPos;
    /* 0x334 */ cXyz mMouthPos2;
    /* 0x340 */ cXyz mHomePos;
    /* 0x34C */ csXyz mRollAngle;
    /* 0x352 */ u8 m352[0x354 - 0x352];
    /* 0x354 */ dPa_rippleEcallBack mRippleCb;
#if VERSION == VERSION_DEMO
    /* 0x368 */ cXyz mRipplePos;
    /* 0x374 */ u8 m374[0x37C - 0x374];
    /* 0x37C */ JPABaseEmitter* mpEmitter;
#else
    /* 0x368 */ dPa_followEcallBack mFollowCb;
    /* 0x37C */ cXyz mRipplePos;
    /* 0x388 */ u8 m388[0x390 - 0x388];
#endif
    /* 0x390 */ dBgS_AcchCir mAcchCir;
    /* 0x3D0 */ dBgS_ObjAcch mAcch;
    /* 0x594 */ dCcD_Stts mStts;
    /* 0x5D0 */ dCcD_Cyl mBodyCoCyl;
    /* 0x700 */ dCcD_Cyl mBodyAtCyl;
    /* 0x830 */ dCcD_Sph mTamaAtSph;
    /* 0x95C */ dCcD_Sph mTamaTgSph;
    /* 0xA88 */ enemyice mEnemyIce;
    /* 0xE40 */ enemyfire mEnemyFire;
    /* 0x1068 */ mDoExt_invisibleModel mInvisibleModel;
    /* 0x1070 */ JntHit_c* mpJntHit;
};  // Size: 0x1074

STATIC_ASSERT(sizeof(oq_class) == DEMO_SELECT(0x1064, 0x1074));

#endif /* D_A_OQ_H */
