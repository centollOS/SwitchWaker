#ifndef D_A_OBJ_TNTRAP_H
#define D_A_OBJ_TNTRAP_H

#include "f_op/f_op_actor.h"
#include "d/d_a_obj.h"
#include "d/d_cc_d.h"
#include "d/d_bg_w.h"
#include "d/d_particle.h"

class daObjTnTrap_c : public fopAc_ac_c {
public:
    enum Param_e { PRM_SW_W = 8, PRM_SW_S = 0, PRM_SW2_W = 8, PRM_SW2_S = 8,
                   PRM_ARG_W = 1, PRM_ARG_S = 16, PRM_MAP_W = 2, PRM_MAP_S = 17 };
    int param_get_arg0() const { return daObj::PrmAbstract(this, PRM_ARG_W, PRM_ARG_S); }
    int param_get_mapType() const { return daObj::PrmAbstract(this, PRM_MAP_W, PRM_MAP_S); }
    int param_get_swSave() const { return daObj::PrmAbstract(this, PRM_SW_W, PRM_SW_S); }
    int param_get_swSave2() const { return daObj::PrmAbstract(this, PRM_SW2_W, PRM_SW2_S); }

    bool chk_appear();
    void set_mtx();
    static BOOL solidHeapCB(fopAc_ac_c*);
    bool create_heap();
    void particle_set(int, float);
    void particle_delete(int);
    void set_se();
    void set_tri(int);
    bool chk_event_flg();
    void set_em_set_offsetY();
    cPhs_State _create();
    bool _delete();
    bool trap_off_wait_act_proc();
    bool trap_on_wait_act_proc();
    bool demo_regist_wait_act_proc();
    bool demo_wait_act_proc();
    bool demo_wait2_act_proc();
    bool demo_end_wait_act_proc();
    bool hide_wait_act_proc();
    void dummy_proc();
    void trap_off_wait_act_init_proc();
    void trap_on_wait_act_init_proc();
    void demo_regist_wait_act_init_proc();
    void demo_wait2_act_init_proc();
    void demo_end_wait_act_init_proc();
    void setup_action(int);
    bool _execute();
    bool _draw();

public:
    typedef bool (daObjTnTrap_c::*Action)();
    typedef void (daObjTnTrap_c::*InitAction)();
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ int mType;
    /* 0x29C */ dCcD_Stts mStts;
    /* 0x2D8 */ dCcD_Tri mTri[2][4];
    /* 0xD58 */ dBgW* mpBgW;
    /* 0xD5C */ Mtx mBgMtx;
    /* 0xD8C */ JPABaseEmitter* mpBall[2][2];
    /* 0xD9C */ JPABaseEmitter* mpThunder[2][3];
    /* 0xDB4 */ Action mAction;
    /* 0xDC0 */ int mActionIdx;
    /* 0xDC4 */ u8 mAppear;
    /* 0xDC8 */ int mSwitch;
    /* 0xDCC */ int mSwitch2;
    /* 0xDD0 */ int mArg;
    /* 0xDD4 */ int mMapType;
    /* 0xDD8 */ s16 mEventIdx;
    /* 0xDDA */ u8 field_0xdda[6];
    /* 0xDE0 */ u8 mParticleSet[2];
    /* 0xDE4 */ f32 mOffsetY[2];
};

STATIC_ASSERT(sizeof(daObjTnTrap_c) == 0xDEC);

#endif /* D_A_OBJ_TNTRAP_H */
