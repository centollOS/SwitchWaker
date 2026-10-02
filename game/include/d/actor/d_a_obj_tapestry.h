#ifndef D_A_OBJ_TAPESTRY_H
#define D_A_OBJ_TAPESTRY_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"
#include "d/d_a_obj.h"
#include "d/d_kankyo.h"
#include "JSystem/J3DGraphBase/J3DPacket.h"
#include "m_Do/m_Do_hostIO.h"
#include "SSystem/SComponent/c_phase.h"

class J3DModel;
class dBgW;
class daObjTapestry_c;
class JPABaseEmitter;

struct daObjTapestry_Attr_c {
    /* 0x00 */ f32 m00;
    /* 0x04 */ f32 m04;
    /* 0x08 */ f32 m08;
    /* 0x0C */ f32 m0C;
    /* 0x10 */ f32 m10;
    /* 0x14 */ f32 m14;
    /* 0x18 */ f32 m18;
    /* 0x1C */ f32 m1C;
    /* 0x20 */ f32 m20;
    /* 0x24 */ f32 m24;
    /* 0x28 */ f32 mGravity;
    /* 0x2C */ f32 m2C;
    /* 0x30 */ f32 m30;
    /* 0x34 */ f32 m34;
    /* 0x38 */ f32 m38;
    /* 0x3C */ f32 m3C;
    /* 0x40 */ u8 m40;
    /* 0x41 */ u8 m41;
    /* 0x44 */ f32 m44;
    /* 0x48 */ u8 m48;
    /* 0x4C */ f32 m4C;
    /* 0x50 */ f32 m50;
    /* 0x54 */ f32 m54;
    /* 0x58 */ f32 m58;
};  // Size: 0x5C

STATIC_ASSERT(sizeof(daObjTapestry_Attr_c) == 0x5C);

class daObjTapestryPLight_c {
public:
    void plight_delete();
    void plight_move(cXyz, csXyz);
    void plight_make();
    void setPointLight(cXyz, csXyz);

public:
    /* 0x00 */ u8 mActive;
    /* 0x04 */ LIGHT_INFLUENCE mLight;
    /* 0x24 */ JPABaseEmitter* mpEmitter;
    /* 0x28 */ f32 mIntensity;
};  // Size: 0x2C

STATIC_ASSERT(sizeof(daObjTapestryPLight_c) == 0x2C);

class daObjTapestryFireEff_c : public dPa_followEcallBack {
public:
    daObjTapestryFireEff_c() : dPa_followEcallBack(0, 0) {
        mPos = cXyz::Zero;
        mSpd = cXyz::Zero;
    }
    virtual ~daObjTapestryFireEff_c() {}

    cXyz* get_pos() { return &mPos; }
    void set_pos(const cXyz& pos) { mPos = pos; }
    void set_spd(const cXyz& spd) { mSpd = spd; }

    virtual void execute(JPABaseEmitter*);

public:
    /* 0x14 */ cXyz mPos;
    /* 0x20 */ cXyz mSpd;
};  // Size: 0x2C

STATIC_ASSERT(sizeof(daObjTapestryFireEff_c) == 0x2C);

class daObjTapestrySmokeCB_c : public dPa_followEcallBack {
public:
    daObjTapestrySmokeCB_c() : dPa_followEcallBack(0, 0) {}
};  // Size: 0x14

STATIC_ASSERT(sizeof(daObjTapestrySmokeCB_c) == 0x14);

class daObjTapestryDrawVtx_c {
public:
    /* 0x000 */ cXyz pos[8][6];
    /* 0x240 */ cXyz nrm[8][6];
    /* 0x480 */ cXyz backNrm[8][6];
};  // Size: 0x6C0

STATIC_ASSERT(sizeof(daObjTapestryDrawVtx_c) == 0x6C0);

class daObjTapestryWork_c {
public:
    /* 0x000 */ cXyz spd[8][6];
    /* 0x240 */ u8 flag0[8][6];
    /* 0x270 */ u8 flag1[8][6];
    /* 0x2A0 */ u8 alpha[8][6];
};  // Size: 0x2D0

STATIC_ASSERT(sizeof(daObjTapestryWork_c) == 0x2D0);

class daObjTapestryDrawData_c {
public:
    daObjTapestryDrawData_c();

    u8* dl() { return m_dl; }
    const u8* dl() const { return m_dl; }
    u32 dl_size() const { return 0x185; }
    f32* tex_coord() { return &mTex[0][0][0]; }

    void ct_tex();
    void ct_dl();

public:
    /* 0x000 */ f32 mTex[8][6][2];
    /* 0x180 */ u8 m_dl[0x1A0] __attribute__((aligned(32)));
};  // Size: 0x320

STATIC_ASSERT(sizeof(daObjTapestryDrawData_c) == 0x320);

class daObjTapestryPacket_c : public J3DPacket {
public:
    daObjTapestryPacket_c();
    virtual void draw();
    virtual ~daObjTapestryPacket_c() {}

    void set_wind_fun(const cXyz& v) { m13C8 = v; }

    void init(daObjTapestry_c*);
    void update();
    void calc_acc_spring_sub(const cXyz*, const cXyz*, float, float);
    void calc_acc_spring(int, int);
    void calc_acc_gravity();
    void calc_acc_wave(int, int);
    void calc_acc_hit(int, int);
    void calc_spd(int, int);
    void calc_pos_crr(int, int);
    void calc_pos();
    void calc_nrm();
    void calc_wind();
    void calc_hit();
    void calc_fire_leap(int, int);
    void calc_fire();
    void calc(daObjTapestry_c*);
    void set_hit(cXyz, cXyz, float, float, bool);
    cXyz* get_now_pos(int, int);
    u8 eff_start(int, int);
    u8 eff_start_chk(int, int);
    void eff_end();
    void eff_pos();
    void eff_delete();
    void smokeCallback_init();
    void smoke_set();
    void smoke_move(fopAc_ac_c*);
    void setup_vtx(daObjTapestryDrawVtx_c*);
    void load_tex();
    void setup_rendar();
    void setup_tev_stage();
    void setup_tevColReg(daObjTapestry_c*);

    static daObjTapestryDrawData_c m_draw_data;

public:
    /* 0x0010 */ daObjTapestryDrawVtx_c mDraw[2];
    /* 0x0D90 */ daObjTapestryWork_c mWork;
    /* 0x1060 */ int mBuffer;
    /* 0x1064 */ daObjTapestryFireEff_c mFire[0x10];
    /* 0x1324 */ int mFireCount;
    /* 0x1328 */ cXyz mAcc;
    /* 0x1334 */ Mtx mMtx;
    /* 0x1364 */ Mtx mViewMtx;
    /* 0x1394 */ Mtx mInvMtx;
    /* 0x13C4 */ u8 mInvOk;
    /* 0x13C8 */ cXyz m13C8;
    /* 0x13D4 */ cXyz m13D4;
    /* 0x13E0 */ s16 m13E0;
    /* 0x13E4 */ cXyz m13E4;
    /* 0x13F0 */ s16 mPhase[3];
    /* 0x13F8 */ cXyz m13F8;
    /* 0x1404 */ cXyz m1404;
    /* 0x1410 */ cXyz m1410;
    /* 0x141C */ cXyz m141C;
    /* 0x1428 */ cXyz m1428;
    /* 0x1434 */ cXyz m1434;
    /* 0x1440 */ cXyz m1440;
    /* 0x144C */ f32 m144C;
    /* 0x1450 */ f32 m1450;
    /* 0x1454 */ bool m1454;
    /* 0x1458 */ f32 m1458;
    /* 0x145C */ f32 m145C;
    /* 0x1460 */ f32 m1460;
    /* 0x1464 */ u8 m1464;
    /* 0x1465 */ u8 m1465;
    /* 0x1466 */ u8 m1466;
    /* 0x1468 */ daObjTapestrySmokeCB_c mSmoke;
    /* 0x147C */ cXyz mSmokePos;
    /* 0x1488 */ csXyz mSmokeAngle;
    /* 0x1490 */ int mSmokeTimer;
    /* 0x1494 */ daObjTapestryPLight_c mPLight;
};  // Size: 0x14C0

STATIC_ASSERT(sizeof(daObjTapestryPacket_c) == 0x14C0);

class daObjTapestry_c : public fopAc_ac_c {
public:
    enum Param_e {
        PRM_SWSAVE_W = 8,
        PRM_SWSAVE_S = 0,
        PRM_EVID_W = 8,
        PRM_EVID_S = 8,
    };

    bool is_switch() const;
    void on_switch() const;
    int param_get_evId() const { return daObj::PrmAbstract(this, PRM_EVID_W, PRM_EVID_S); }
    int param_get_swSave() const { return daObj::PrmAbstract(this, PRM_SWSAVE_W, PRM_SWSAVE_S); }

    bool chk_appear();
    void set_mtx();
    void init_mtx();
    static BOOL solidHeapCB(fopAc_ac_c*);
    bool create_heap();
    cPhs_State create_res_load();
    void init_cc();
    void set_cc_pos();
    bool checkCollision();
    void set_eye_pos();
    cPhs_State _create();
    bool _delete();
    void wait_act_proc();
    void demo_request_act_proc();
    void burn_act_proc();
    void burn_act_init_proc();
    void fine_act_init_proc();
    void setup_action(int);
    bool _execute();
    bool _draw();

public:
    /* 0x0290 */ request_of_phase_process_class mPhase;
#if VERSION == VERSION_DEMO
    /* 0x0298 */ request_of_phase_process_class mClothPhase;
#endif
    /* 0x0298 */ daObjTapestryPacket_c mPacket;
    /* 0x1758 */ J3DModel* mpModel;
    /* 0x175C */ dBgW* mpBgW;
    /* 0x1760 */ Mtx mMtx;
    /* 0x1790 */ dCcD_Tri mTri[2];
    /* 0x1A30 */ dCcD_Stts mStts[2];
    /* 0x1AA8 */ s16 mTimer;
    /* 0x1AAA */ u8 m1AAA;
    /* 0x1AAC */ s16 mEventIdx;
    /* 0x1AB0 */ f32 m1AB0;
    /* 0x1AB4 */ void (daObjTapestry_c::*mActProc)();
    /* 0x1AC0 */ int mAction;
};  // Size: 0x1AC4

STATIC_ASSERT(sizeof(daObjTapestry_c) == DEMO_SELECT(0x1ACC, 0x1AC4));

#endif /* D_A_OBJ_TAPESTRY_H */
