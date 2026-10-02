#ifndef D_A_GND_H
#define D_A_GND_H

#include "f_op/f_op_actor.h"
#include "d/d_bg_s_acch.h"
#include "d/d_cc_d.h"
#include "d/d_kankyo.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_hostIO.h"
#include "SSystem/SComponent/c_phase.h"

struct gnd_ke_s {
    /* 0x000 */ cXyz mPos[20];
    /* 0x0F0 */ cXyz mVel[20];
};  // Size: 0x1E0

STATIC_ASSERT(sizeof(gnd_ke_s) == 0x1E0);

class gnd_class : public fopEn_enemy_c {
public:
    /* 0x02AC */ request_of_phase_process_class mPhase;
    /* 0x02B4 */ u8 m2B4;
    /* 0x02B5 */ u8 m2B5[0x02B8 - 0x02B5];
    /* 0x02B8 */ mDoExt_McaMorf* mpMorf;
    /* 0x02BC */ mDoExt_btkAnm* mpBtkAnm;
    /* 0x02C0 */ mDoExt_brkAnm* mpBrkAnm;
    /* 0x02C4 */ mDoExt_btpAnm* mpBtpAnm;
    /* 0x02C8 */ u32 mShadowId;
    /* 0x02CC */ s16 m2CC;
    /* 0x02CE */ s16 m2CE;
    /* 0x02D0 */ s16 m2D0;
    /* 0x02D2 */ u8 m2D2[0x02D4 - 0x02D2];
    /* 0x02D4 */ cXyz m2D4;
    /* 0x02E0 */ cXyz m2E0;
    /* 0x02EC */ u8 m2EC[0x02F0 - 0x02EC];
    /* 0x02F0 */ f32 m2F0;
    /* 0x02F4 */ f32 m2F4;
    /* 0x02F8 */ f32 m2F8;
    /* 0x02FC */ u8 m2FC[0x0302 - 0x02FC];
    /* 0x0302 */ s16 m302[5];
    /* 0x030C */ s16 mAction;
    /* 0x030E */ s16 m30E;
    /* 0x0310 */ f32 m310;
    /* 0x0314 */ s16 m314;
    /* 0x0316 */ s16 m316;
    /* 0x0318 */ s16 m318;
    /* 0x031A */ s16 m31A;
    /* 0x031C */ s8 m31C;
    /* 0x031D */ u8 m31D[0x0320 - 0x031D];
    /* 0x0320 */ dKy_tevstr_c mTevStr2;
    /* 0x03D0 */ u8 m3D0[0x03D4 - 0x03D0];
    /* 0x03D4 */ s8 m3D4;
    /* 0x03D5 */ s8 m3D5;
    /* 0x03D6 */ s8 m3D6;
    /* 0x03D7 */ s8 m3D7;
    /* 0x03D8 */ s8 m3D8;
    /* 0x03D9 */ s8 m3D9;
    /* 0x03DA */ s8 m3DA;
    /* 0x03DB */ s8 m3DB;
    /* 0x03DC */ s8 m3DC;
    /* 0x03DD */ u8 m3DD;
    /* 0x03DE */ s16 m3DE;
    /* 0x03E0 */ f32 m3E0;
    /* 0x03E4 */ gnd_ke_s mKe[4];
    /* 0x0B64 */ mDoExt_3DlineMat0_c mLineMat;
    /* 0x0B80 */ dBgS_AcchCir mAcchCir;
    /* 0x0BC0 */ dBgS_ObjAcch mAcch;
    /* 0x0D84 */ dCcD_Stts mStts;
    /* 0x0DC0 */ dCcD_Cyl mBodyCyl;
    /* 0x0EF0 */ dCcD_Sph mHeadSph;
    /* 0x101C */ dCcD_Sph mChestSph;
    /* 0x1148 */ dCcD_Sph mWeponSph[2];
    /* 0x13A0 */ cXyz m13A0[2];
    /* 0x13B8 */ s8 m13B8[2];
    /* 0x13BA */ u8 m13BA[0x13BC - 0x13BA];
    /* 0x13BC */ int m13BC[2];
    /* 0x13C4 */ f32 m13C4[2];
    /* 0x13CC */ s8 m13CC[2];
    /* 0x13CE */ s8 m13CE;
    /* 0x13CF */ u8 mNextParryOpeningType;
    /* 0x13D0 */ s8 m13D0;
    /* 0x13D1 */ s8 m13D1;
    /* 0x13D2 */ u8 m13D2[0x13D4 - 0x13D2];
    /* 0x13D4 */ JPABaseEmitter* mpAttackEff[6];
    /* 0x13EC */ JPABaseEmitter* mp13EC;
    /* 0x13F0 */ JPABaseEmitter* mp13F0[2];
#if VERSION > VERSION_DEMO
    /* 0x13F8 */ s16 mFlashTimer[15];
    /* 0x1416 */ u8 m1416[0x1426 - 0x1416];
    /* 0x1426 */ s16 m1426;
    /* 0x1428 */ f32 m1428;
    /* 0x142C */ s16 m142C;
    /* 0x142E */ s16 m142E;
    /* 0x1430 */ JPABaseEmitter* mpFlashEff[15];
    /* 0x146C */ u8 m146C[0x14A8 - 0x146C];
    /* 0x14A8 */ cXyz m14A8[15];
    /* 0x155C */ s16 m155C;
#endif
    /* 0x155E */ s16 m155E;
    /* 0x1560 */ s16 m1560;
    /* 0x1562 */ s8 m1562;
    /* 0x1563 */ u8 m1563;
    /* 0x1564 */ cXyz m1564;
    /* 0x1570 */ cXyz m1570;
    /* 0x157C */ cXyz m157C;
    /* 0x1588 */ cXyz m1588;
    /* 0x1594 */ cXyz m1594;
    /* 0x15A0 */ f32 m15A0;
    /* 0x15A4 */ f32 m15A4;
    /* 0x15A8 */ f32 m15A8;
#if VERSION > VERSION_DEMO
    /* 0x15AC */ f32 m15AC;
#endif
    /* 0x15B0 */ u8 m15B0;
    /* 0x15B1 */ u8 m15B1[0x15B4 - 0x15B1];
    /* 0x15B4 */ dPa_rippleEcallBack mRippleCb;
    /* 0x15C8 */ cXyz m15C8[2];
    /* 0x15E0 */ u8 mHioSet;
    /* 0x15E1 */ u8 m15E1[0x15E4 - 0x15E1];
};

STATIC_ASSERT(sizeof(gnd_class) == DEMO_SELECT(0x147C, 0x15E4));

class daGnd_HIO_c : public JORReflexible {
public:
    daGnd_HIO_c();
    virtual ~daGnd_HIO_c() {}

public:
    /* 0x04 */ s8 mNo;
    /* 0x05 */ u8 m05;
    /* 0x06 */ u8 m06;
    /* 0x07 */ u8 m07;
    /* 0x08 */ f32 m08;
    /* 0x0C */ f32 m0C;
    /* 0x10 */ u8 m10;
    /* 0x11 */ u8 m11;
    /* 0x12 */ u8 m12;
    /* 0x14 */ f32 m14;
    /* 0x18 */ f32 m18;
    /* 0x1C */ f32 m1C;
    /* 0x20 */ f32 m20;
    /* 0x24 */ f32 m24;
    /* 0x28 */ f32 m28;
    /* 0x2C */ f32 m2C;
    /* 0x30 */ f32 m30;
    /* 0x34 */ f32 m34;
    /* 0x38 */ f32 m38;
    /* 0x3C */ f32 m3C;
    /* 0x40 */ f32 m40;
    /* 0x44 */ u8 m44;
    /* 0x45 */ u8 m45;
    /* 0x46 */ u8 m46;
    /* 0x48 */ f32 m48;
    /* 0x4C */ f32 m4C;
    /* 0x50 */ f32 m50;
    /* 0x54 */ f32 m54;
    /* 0x58 */ f32 m58;
    /* 0x5C */ f32 m5C;
    /* 0x60 */ f32 m60;
    /* 0x64 */ f32 m64;
    /* 0x68 */ s16 m68;
    /* 0x6A */ s16 m6A;
    /* 0x6C */ s16 m6C;
    /* 0x6E */ s16 m6E;
    /* 0x70 */ s16 m70;
    /* 0x72 */ s16 m72;
    /* 0x74 */ s16 m74;
    /* 0x76 */ s16 m76;
    /* 0x78 */ f32 m78;
    /* 0x7C */ s16 m7C;
    /* 0x7E */ s16 m7E;
    /* 0x80 */ s16 m80;
    /* 0x82 */ s16 m82;
    /* 0x84 */ s16 m84;
    /* 0x86 */ s16 m86;
    /* 0x88 */ u8 m88;
    /* 0x89 */ u8 m89;
};  // Size: 0x8C

STATIC_ASSERT(sizeof(daGnd_HIO_c) == 0x8C);

#endif /* D_A_GND_H */
