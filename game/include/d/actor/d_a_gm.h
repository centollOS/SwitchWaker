#ifndef D_A_GM_H
#define D_A_GM_H

#include "c/c_damagereaction.h"
#include "d/d_bg_s_acch.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"
#include "f_op/f_op_actor.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"

class gm_class : public fopEn_enemy_c {
public:
    /* 0x02AC */ request_of_phase_process_class mPhs;
    /* 0x02B4 */ mDoExt_McaMorf* mpMorf;
    /* 0x02B8 */ u8 m2B8[0x02BC - 0x02B8];
    /* 0x02BC */ mDoExt_brkAnm* mpBrkGm;
    /* 0x02C0 */ mDoExt_brkAnm* mpBrkAtack;
    /* 0x02C4 */ mDoExt_brkAnm* mpBrkDamage;
    /* 0x02C8 */ mDoExt_brkAnm* mpBrkDead;
    /* 0x02CC */ u8 mWingBits;
    /* 0x02CD */ u8 m2CD;
    /* 0x02CE */ u8 m2CE;
    /* 0x02CF */ u8 mSwitchNo;
    /* 0x02D0 */ u8 m2D0;
    /* 0x02D1 */ u8 m2D1;
    /* 0x02D2 */ u8 m2D2;
    /* 0x02D3 */ u8 m2D3;
    /* 0x02D4 */ u8 m2D4;
    /* 0x02D5 */ u8 m2D5;
    /* 0x02D6 */ u8 mSwitchEnable;
    /* 0x02D7 */ u8 m2D7;
    /* 0x02D8 */ u8 m2D8[0x02DA - 0x02D8];
    /* 0x02DA */ s16 m2DA;
    /* 0x02DC */ s16 mAction;
    /* 0x02DE */ s16 m2DE;
    /* 0x02E0 */ s16 m2E0;
    /* 0x02E2 */ s16 m2E2;
    /* 0x02E4 */ s16 m2E4;
    /* 0x02E6 */ s16 m2E6;
    /* 0x02E8 */ s16 m2E8;
    /* 0x02EA */ s16 m2EA;
    /* 0x02EC */ s16 m2EC;
    /* 0x02EE */ s16 m2EE;
    /* 0x02F0 */ s16 mTimers[23];
    /* 0x031E */ s16 m31E;
    /* 0x0320 */ s16 m320;
    /* 0x0322 */ s16 m322;
    /* 0x0324 */ s16 mBrkMode;
    /* 0x0326 */ s16 m326;
    /* 0x0328 */ f32 mCorrectionY;
    /* 0x032C */ f32 mWallH;
    /* 0x0330 */ f32 mWallR;
    /* 0x0334 */ f32 m334;
    /* 0x0338 */ f32 m338;
    /* 0x033C */ f32 m33C;
    /* 0x0340 */ f32 m340;
    /* 0x0344 */ int mBckIdx;
    /* 0x0348 */ cXyz m348;
    /* 0x0354 */ cXyz mSpawnPos;
    /* 0x0360 */ cXyz m360[3];
    /* 0x0384 */ cXyz m384[3];
    /* 0x03A8 */ cXyz mAtamaOffsetPos;
    /* 0x03B4 */ cXyz mDrawOffset;
    /* 0x03C0 */ cXyz mFuwafuwa;
    /* 0x03CC */ cXyz mLineStart;
    /* 0x03D8 */ cXyz mLineEnd;
    /* 0x03E4 */ cXyz mAtamaPos;
    /* 0x03F0 */ cXyz mMunePos;
    /* 0x03FC */ cXyz mHaraPos;
    /* 0x0408 */ f32 m408;
    /* 0x040C */ f32 m40C;
    /* 0x0410 */ f32 m410;
    /* 0x0414 */ csXyz mWingAngle[4];
    /* 0x042C */ csXyz mExtraJointRot;
    /* 0x0432 */ u8 m432[0x0434 - 0x0432];
    /* 0x0434 */ s16 m434;
    /* 0x0436 */ s16 m436;
    /* 0x0438 */ csXyz mMuneRot;
    /* 0x043E */ csXyz m43E;
    /* 0x0444 */ J3DModel* mpShadowModel;
    /* 0x0448 */ u32 mShadowId;
    /* 0x044C */ s16 m44C;
    /* 0x044E */ u8 m44E[0x0450 - 0x044E];
    /* 0x0450 */ cXyz m450;
    /* 0x045C */ cXyz m45C;
    /* 0x0468 */ u8 m468[0x0478 - 0x0468];
    /* 0x0478 */ f32 m478;
    /* 0x047C */ dBgS_AcchCir mAcchCir;
    /* 0x04BC */ dBgS_ObjAcch mAcch;
    /* 0x0680 */ dCcD_Stts mStts;
    /* 0x06BC */ dCcD_Cyl mBodyCyl;
    /* 0x07EC */ dCcD_Cyl mWingCyl;
    /* 0x091C */ dCcD_Cyl mWindCyl;
    /* 0x0A4C */ dCcD_Sph mWeaponSph;
    /* 0x0B78 */ JPABaseEmitter* mpEmitters[4];
    /* 0x0B88 */ u8 mB88[0x0B9C - 0x0B88];
    /* 0x0B9C */ enemyice mEnemyIce;
    /* 0x0F54 */ enemyfire mEnemyFire;
    /* 0x117C */ mDoExt_invisibleModel mInvisibleModel;
};  // Size: 0x1184

STATIC_ASSERT(sizeof(gm_class) == 0x1184);

#endif /* D_A_GM_H */
