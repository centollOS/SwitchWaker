#ifndef D_A_OBJ_FIREWALL_H
#define D_A_OBJ_FIREWALL_H

#include "f_op/f_op_actor.h"
#include "d/d_a_obj.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"

class daObjFirewall_c : public fopAc_ac_c {
public:
    enum Param_e { PRM_SWSAVE_W = 8, PRM_SWSAVE_S = 0 };
    s32 param_get_swSave() const { return daObj::PrmAbstract(this, PRM_SWSAVE_W, PRM_SWSAVE_S); }

    void init_mtx();
    static BOOL solidHeapCB(fopAc_ac_c*);
    bool create_heap();
    void registCollisionTable();
    void setPointLight();
    void particle_set();
    void particle_delete();
    void seStart(unsigned long);
    void set_se(bool);
    void seDelete();
    void set_pl_se();
    void setup_burn_up();
    void setup_put_the_fire_out();
    cPhs_State _create();
    bool _delete();
    void wait_act_proc();
    void wait2_act_proc();
    void wait3_act_proc();
    void appear_act_proc();
    void demo_end_wait_act_proc();
    void burn_wait_act_proc();
    void retire_act_proc();
    bool _execute();
    bool _draw();

public:
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ dCcD_Stts mStts;
    /* 0x2D4 */ dCcD_Cyl mCyl;
    /* 0x404 */ J3DModel* mpModel;
    /* 0x408 */ dBgW* mpBgW;
    /* 0x40C */ mDoExt_btkAnm mBtk;
    /* 0x420 */ mDoExt_brkAnm mBrk;
    /* 0x438 */ JPABaseEmitter* mpEmitter[6];
    /* 0x450 */ JPABaseEmitter* mpEmitter2[5];
    /* 0x464 */ JPABaseEmitter* mpEmitter3;
    /* 0x468 */ s32 mSwitch;
    /* 0x46C */ LIGHT_INFLUENCE mLights[64];
    /* 0xC6C */ cXyz mLightPos[64];
    /* 0xF6C */ JPABaseEmitter* mpKageroEmitter[64];
    /* 0x106C */ f32 mLightStrength;
    /* 0x1070 */ void (daObjFirewall_c::*mAction)();
    /* 0x107C */ s16 mEvent;
    /* 0x107E */ u8 mSoundOn;
#if VERSION == VERSION_DEMO
    /* 0x107F */ u8 mBurning;
#else
    /* 0x1080 */ cXyz mSoundPos[8];
    /* 0x10E0 */ u8 mSoundInitialized;
    /* 0x10E1 */ u8 mBurning;
#endif
    /* 0x10E4 */ s32 mEventType;
    /* 0x10E8 */ s32 mVoiceIndex;
};

STATIC_ASSERT(sizeof(daObjFirewall_c) == DEMO_SELECT(0x1088, 0x10EC));

#endif /* D_A_OBJ_FIREWALL_H */
