#ifndef D_A_GY_H
#define D_A_GY_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_bg_s_acch.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_hostIO.h"
#include "SSystem/SComponent/c_phase.h"
#include "dolphin/mtx/quat.h"
#include "c/c_damagereaction.h"

class daGy_Ctrl_c;
class JntHit_c;
class J3DNode;
class J3DModel;

#if VERSION == VERSION_DEMO
class daGy_c : public fopAc_ac_c {
#else
class daGy_c : public fopEn_enemy_c {
#endif
public:
    void _nodeControl(J3DNode*, J3DModel*);
    BOOL _createHeap();
    void setMtx();
    void setAnm();
#if VERSION > VERSION_DEMO
    void setAtCollision();
#endif
    void setCollision();
    void setAimSpeedF();
    void modeDiveInit();
    void modeDive();
    void modeCircleInit();
    void modeCircle();
    void modeWithCircleInit();
    void modeWithCircle();
    void modeAttackInit();
    void modeWithAttackInit();
    void modeWithAttack();
    void modeAttack();
    void modeAttackPlayerInit();
    void modeAttackPlayer();
    void modeAttackBackInit();
    void modeAttackBack();
    void modeDamageInit();
    void modeDamage();
    void modeDeleteInit();
    void modeDelete();
    void modeDeleteBombInit();
    void modeDeleteBomb();
    void modeProcCall();
#if VERSION > VERSION_DEMO
    void createWave();
#endif
    void setWave();
    void lineCheck(cXyz*, cXyz*);
    void checkTgHit();
    f32 getWaterY();
    bool _execute();
    void drawDebug();
    bool _draw();
    void createInit();
    cPhs_State _create();
    bool _delete();

    static const u32 m_heapsize;
    static const char m_arc_name[];

public:
    /* 0x2AC */ s32 m2AC;
    /* 0x2B0 */ s32 m2B0;
    /* 0x2B4 */ s32 m2B4;
    /* 0x2B8 */ daGy_Ctrl_c* mpCtrl;
    /* 0x2BC */ cXyz m2BC;
    /* 0x2C8 */ request_of_phase_process_class mPhs;
    /* 0x2D0 */ u8 m2D0[0x2D4 - 0x2D0];
    /* 0x2D4 */ mDoExt_McaMorf* mpMorf;
    /* 0x2D8 */ mDoExt_invisibleModel m2D8;
    /* 0x2E0 */ dBgS_ObjAcch mAcch;
    /* 0x4A4 */ dBgS_AcchCir mAcchCir;
    /* 0x4E4 */ f32 m4E4;
    /* 0x4E8 */ f32 m4E8;
    /* 0x4EC */ f32 m4EC;
    /* 0x4F0 */ f32 mAimSpeedF;
    /* 0x4F4 */ u8 m4F4[0x4F8 - 0x4F4];
    /* 0x4F8 */ s32 m4F8;
    /* 0x4FC */ int m4FC;
    /* 0x500 */ int m500;
    /* 0x504 */ u8 m504;
    /* 0x505 */ u8 m505[0x508 - 0x505];
    /* 0x508 */ f32 m508;
#if VERSION == VERSION_DEMO
    /* 0x4F0 */ dCcD_Sph mSph[6];
#else
    /* 0x50C */ dCcD_Sph mHeadSph;
    /* 0x638 */ dCcD_Sph mSph;
#endif
    /* 0x764 */ dCcD_Cps mCps;
    /* 0x89C */ cXyz m89C;
    /* 0x8A8 */ dCcD_Stts mStts;
#if VERSION == VERSION_DEMO
    /* 0xD78 */ cXyz mD78[6];
#endif
    /* 0x8E4 */ u32 m8E4;
    /* 0x8E8 */ u8 m8E8[0x8EC - 0x8E8];
    /* 0x8EC */ int m8EC;
#if VERSION > VERSION_DEMO
    /* 0x8F0 */ int m8F0;
#endif
    /* 0x8F4 */ s16 m8F4;
    /* 0x8F6 */ u8 m8F6[0x8F8 - 0x8F6];
    /* 0x8F8 */ cXyz m8F8;
    /* 0x904 */ cXyz m904;
    /* 0x910 */ u8 m910[0x914 - 0x910];
    /* 0x914 */ int m914;
    /* 0x918 */ f32 m918;
    /* 0x91C */ f32 m91C;
    /* 0x920 */ f32 m920;
    /* 0x924 */ s32 m924;
    /* 0x928 */ s32 m928;
    /* 0x92C */ enemyice mEnemyIce;
    /* 0xCE4 */ JntHit_c* mpJntHit;
    /* 0xCE8 */ u8 mCE8;
    /* 0xCE9 */ u8 mCE9[0xCEC - 0xCE9];
#if VERSION == VERSION_DEMO
    /* 0x11C4 */ cXyz m11C4[15];
#endif
    /* 0xCEC */ cXyz mCEC;
    /* 0xCF8 */ Quaternion mCF8;
    /* 0xD08 */ cXyz mD08;
    /* 0xD14 */ s8 mAnmIdx;
    /* 0xD15 */ s8 mPrmIdx;
    /* 0xD16 */ s8 mOldPrmIdx;
    /* 0xD18 */ dPa_waveEcallBack mD18;
    /* 0xD7C */ dPa_waveEcallBack mD7C;
    /* 0xDE0 */ dPa_splashEcallBack mDE0;
    /* 0xDFC */ cXyz mDFC;
    /* 0xE08 */ csXyz mE08;
    /* 0xE0E */ u8 mE0E[0xE10 - 0xE0E];
    /* 0xE10 */ f32 mE10;
    /* 0xE14 */ dBgS_ObjLinChk mLinChk;
    /* 0xE80 */ u8 mE80;
    /* 0xE81 */ u8 mE81[0xE84 - 0xE81];
    /* 0xE84 */ cXyz mE84;
}; // size = 0xE90

class daGy_HIO_c : public mDoHIO_entry_c {
public:
    daGy_HIO_c();
    virtual ~daGy_HIO_c() {}

public:
    /* 0x004 */ f32 m04;
    /* 0x008 */ f32 m08;
    /* 0x00C */ s16 m0C;
    /* 0x00E */ u8 m0E[0x010 - 0x00E];
    /* 0x010 */ f32 m10;
    /* 0x014 */ f32 m14;
    /* 0x018 */ f32 m18;
    /* 0x01C */ f32 m1C;
    /* 0x020 */ f32 m20;
    /* 0x024 */ f32 m24;
    /* 0x028 */ cXyz m28;
    /* 0x034 */ cXyz m34;
    /* 0x040 */ f32 m40;
    /* 0x044 */ f32 m44;
    /* 0x048 */ f32 m48;
    /* 0x04C */ f32 m4C;
    /* 0x050 */ f32 m50;
    /* 0x054 */ f32 m54;
    /* 0x058 */ f32 m58;
    /* 0x05C */ f32 m5C;
    /* 0x060 */ f32 m60;
    /* 0x064 */ f32 m64;
    /* 0x068 */ f32 m68;
    /* 0x06C */ f32 m6C;
    /* 0x070 */ f32 m70;
    /* 0x074 */ f32 m74[5];
    /* 0x088 */ f32 m88;
    /* 0x08C */ f32 m8C;
    /* 0x090 */ f32 m90;
    /* 0x094 */ u8 m94;
    /* 0x095 */ u8 m95;
    /* 0x096 */ u8 m96;
    /* 0x097 */ u8 m97;
    /* 0x098 */ u8 m98;
    /* 0x099 */ u8 m99;
    /* 0x09A */ s16 m9A;
    /* 0x09C */ f32 m9C;
    /* 0x0A0 */ f32 mA0;
    /* 0x0A4 */ f32 mA4;
    /* 0x0A8 */ f32 mA8;
    /* 0x0AC */ f32 mAC;
    /* 0x0B0 */ f32 mB0;
    /* 0x0B4 */ f32 mB4;
    /* 0x0B8 */ cXyz mB8;
    /* 0x0C4 */ f32 mC4;
    /* 0x0C8 */ f32 mC8;
    /* 0x0CC */ f32 mCC;
    /* 0x0D0 */ f32 mD0;
    /* 0x0D4 */ f32 mD4;
    /* 0x0D8 */ f32 mD8;
    /* 0x0DC */ f32 mDC;
    /* 0x0E0 */ f32 mE0;
    /* 0x0E4 */ f32 mE4;
    /* 0x0E8 */ f32 mE8;
    /* 0x0EC */ f32 mEC;
    /* 0x0F0 */ f32 mF0;
    /* 0x0F4 */ f32 mF4;
    /* 0x0F8 */ f32 mF8;
    /* 0x0FC */ f32 mFC;
    /* 0x100 */ f32 m100;
    /* 0x104 */ f32 m104;
    /* 0x108 */ f32 m108;
    /* 0x10C */ s16 m10C;
    /* 0x10E */ s16 m10E;
    /* 0x110 */ s16 m110;
    /* 0x112 */ s16 m112;
    /* 0x114 */ s16 m114;
    /* 0x116 */ s16 m116;
    /* 0x118 */ s16 m118;
    /* 0x11A */ u8 m11A[0x12C - 0x11A];
    /* 0x12C */ f32 m12C;
    /* 0x130 */ s16 m130;
    /* 0x132 */ s16 m132;
    /* 0x134 */ f32 m134;
    /* 0x138 */ f32 m138;
    /* 0x13C */ f32 m13C;
    /* 0x140 */ s16 m140;
    /* 0x142 */ s16 m142;
    /* 0x144 */ f32 m144;
    /* 0x148 */ f32 m148;
    /* 0x14C */ f32 m14C;
    /* 0x150 */ f32 m150;
    /* 0x154 */ f32 m154;
    /* 0x158 */ f32 m158;
    /* 0x15C */ f32 m15C;
    /* 0x160 */ f32 m160;
    /* 0x164 */ s16 m164;
    /* 0x166 */ u8 m166[0x168 - 0x166];
    /* 0x168 */ f32 m168;
    /* 0x16C */ f32 m16C;
    /* 0x170 */ f32 m170;
    /* 0x174 */ f32 m174;
    /* 0x178 */ f32 m178;
    /* 0x17C */ f32 m17C;
    /* 0x180 */ s16 m180;
    /* 0x182 */ u8 m182[0x184 - 0x182];
    /* 0x184 */ f32 m184;
    /* 0x188 */ f32 m188;
    /* 0x18C */ f32 m18C;
    /* 0x190 */ f32 m190;
    /* 0x194 */ f32 m194;
    /* 0x198 */ s16 m198;
    /* 0x19A */ u8 m19A[0x19C - 0x19A];
}; // size = 0x19C

STATIC_ASSERT(sizeof(daGy_c) == DEMO_SELECT(0x141C, 0xE90));
STATIC_ASSERT(sizeof(daGy_HIO_c) == DEMO_SELECT(0x1A0, 0x19C));

#endif /* D_A_GY_H */
