#ifndef D_A_WZ_H
#define D_A_WZ_H

#include "f_op/f_op_actor.h"
#include "c/c_damagereaction.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/d_bg_s_acch.h"
#include "d/d_cc_d.h"
#include "d/d_kankyo.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"

class mDoExt_McaMorf;
class mDoExt_brkAnm;
class mDoExt_btkAnm;
struct dPath;

enum daWZ_BehaviorType {
    WZ_TYPE_NORMAL = 0,
    WZ_TYPE_1 = 1,
    WZ_TYPE_MINIBOSS_PARAM = 2,
    WZ_TYPE_3 = 3,
    WZ_TYPE_DAMAGE_BALL_FIRE = 0xA,
    WZ_TYPE_DAMAGE_BALL_ICE = 0xB,
    WZ_TYPE_SUMMON_DOOR = 0xC,
    WZ_TYPE_SUMMON_DOOR_2 = 0xD,
};

class wz_class : public fopEn_enemy_c {
public:
    /* 0x2AC */ request_of_phase_process_class mPhase;
    /* 0x2B4 */ mDoExt_McaMorf* mpMorf;
    /* 0x2B8 */ mDoExt_McaMorf* mpRodMorf;
    /* 0x2BC */ mDoExt_McaMorf* mpMiniMorf;
    /* 0x2C0 */ mDoExt_brkAnm* mpBrk;
    /* 0x2C4 */ mDoExt_btkAnm* mpBtk;
    /* 0x2C8 */ u8 m2C8[0x2CC - 0x2C8];
    /* 0x2CC */ Mtx mBallMtx;
    /* 0x2FC */ cXyz mStaffPos;
    /* 0x308 */ cXyz mRodScale;
    /* 0x314 */ cXyz mTamaTarget;
    /* 0x320 */ cXyz mRodTipPos;
    /* 0x32C */ cXyz mHomePos;
    /* 0x338 */ cXyz mPathTarget;
    /* 0x344 */ fpc_ProcID mRelatedId;
    /* 0x348 */ u8 mBehaviorType;
    /* 0x349 */ u8 mEnemySummonTableIndex;
    /* 0x34A */ u8 mEnableSpawnSwitch;
    /* 0x34B */ u8 mDisableSpawnOnDeathSwitch;
    /* 0x34C */ u8 mAction;
    /* 0x34D */ u8 mMode;
    /* 0x34E */ u8 m34E;
    /* 0x34F */ u8 m34F;
    /* 0x350 */ bool mIsMiniBoss;
    /* 0x351 */ bool m351;
    /* 0x352 */ u8 m352;
    /* 0x353 */ u8 m353;
    /* 0x354 */ u8 mHasChildActor;
    /* 0x355 */ u8 m355[0x358 - 0x355];
    /* 0x358 */ int mPathPointIdx;
    /* 0x35C */ fpc_ProcID mChildIds[20];
    /* 0x3AC */ u8 mChildAlive[20];
    /* 0x3C0 */ u8 mChildHidden[20];
    /* 0x3D4 */ s16 mTimers[5];
    /* 0x3DE */ s16 m3DE[4];
    /* 0x3E6 */ s16 mTargetAngleY;
    /* 0x3E8 */ s16 mFuwafuwaAngle;
    /* 0x3EA */ s16 mAlpha;
    /* 0x3EC */ int mSummonTableType;
    /* 0x3F0 */ int mSummonWave;
    /* 0x3F4 */ s16 mBallAlpha;
    /* 0x3F6 */ s16 mSummonCount;
    /* 0x3F8 */ u8 m3F8;
    /* 0x3F9 */ u8 m3F9[0x3FC - 0x3F9];
    /* 0x3FC */ u32 mShadowId;
    /* 0x400 */ int mBckIdx;
    /* 0x404 */ f32 mCorrectionOffsetY;
    /* 0x408 */ f32 mWallHeight;
    /* 0x40C */ f32 mWallRadius;
    /* 0x410 */ f32 mBallRadius;
    /* 0x414 */ dPath* mpRoomPath;
    /* 0x418 */ u8 m418[0x425 - 0x418];
    /* 0x425 */ u8 mPathIndex;
    /* 0x426 */ u8 m426[0x428 - 0x426];
    /* 0x428 */ dPath* mpPath;
    /* 0x42C */ u8 m42C[0x439 - 0x42C];
    /* 0x439 */ u8 m439;
    /* 0x43A */ u8 m43A[0x43C - 0x43A];
    /* 0x43C */ cXyz mCamEye;
    /* 0x448 */ cXyz mCamCenter;
    /* 0x454 */ u8 m454[0x464 - 0x454];
    /* 0x464 */ f32 mCamFov;
    /* 0x468 */ dBgS_AcchCir mAcchCir;
    /* 0x4A8 */ dBgS_ObjAcch mAcch;
    /* 0x66C */ dCcD_Stts mStts;
    /* 0x6A8 */ dCcD_Cyl mBodyCyl;
    /* 0x7D8 */ dCcD_Sph mBallSph;
    /* 0x904 */ LIGHT_INFLUENCE mPLight;
    /* 0x924 */ enemyice mEnemyIce;
    /* 0xCDC */ enemyfire mEnemyFire;
    /* 0xF04 */ cXyz mEffectPos;
    /* 0xF10 */ csXyz mEffectAngle;
    /* 0xF16 */ u8 mF16[0xF18 - 0xF16];
    /* 0xF18 */ dPa_followEcallBack mParticleCb0;
    /* 0xF2C */ dPa_followEcallBack mParticleCb1;
    /* 0xF40 */ dPa_followEcallBack mParticleCb2;
    /* 0xF54 */ dPa_followEcallBack mParticleCb3;
    /* 0xF68 */ dPa_followEcallBack mParticleCb4;
    /* 0xF7C */ mDoExt_invisibleModel mInvisibleModel;
    /* 0xF84 */ mDoExt_invisibleModel mMiniInvisibleModel;
};  // Size: 0xF8C

STATIC_ASSERT(sizeof(wz_class) == 0xF8C);

#endif /* D_A_WZ_H */
