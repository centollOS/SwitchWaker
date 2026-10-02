#ifndef D_A_OBJ_VTIL_H
#define D_A_OBJ_VTIL_H

#include "d/d_cc_d.h"
#include "f_op/f_op_actor.h"
#include "d/d_a_obj.h"
#include "d/d_bg_s_acch.h"
#include "d/d_cc_d.h"

class daObjVtil_c : public fopAc_ac_c {
public:
    enum Prm_e { PRM_TYPE_S = 0, PRM_TYPE_W = 4 };
    inline void camera_off();
    inline BOOL check_ev_bit() const;
    int prm_get_type() const { return daObj::PrmAbstract(this, PRM_TYPE_W, PRM_TYPE_S); }

    static BOOL solidHeapCB(fopAc_ac_c*);
    BOOL create_heap();
    cPhs_State _create();
    bool _delete();
    void tell_agb_attack();
    void tell_agb_sink();
    void init_mtx();
    void init_co();
    void init_bgc();
    void renew_attention_pos();
    void make_smoke();
    void se_smoke();
    void make_splash();
    void se_splash();
    void set_sound(int, int) const;
    void to_wait_mode();
    void mode_wait();
    void to_carry_mode();
    void mode_carry();
    void to_throw_mode();
    void calc_throw(float*, float*, float*) const;
    void mode_throw();
    void to_sink_mode();
    void mode_sink();
    void hit_co();
    bool check_sink();
    bool check_sink_end();
    void hit_bg();
    void make_vib();
    BOOL check_circle();
    bool _execute();
    bool _draw();

public:
    static const int l_daObjVtil_bdl_idx_table[5];
    static const int l_daObjVtil_scene_no_table[5];
    static const dCcD_SrcCyl M_co_cyl_data;
    static const char M_arcname[];
    /* 0x290 */ J3DModel* mpModel;
    /* 0x294 */ request_of_phase_process_class mPhase;
    /* 0x29C */ mutable dBgS_ObjAcch mAcch;
    /* 0x460 */ dBgS_AcchCir mAcchCir;
    /* 0x4A0 */ dCcD_Stts mStts;
    /* 0x4DC */ dCcD_Cyl mCyl;
    /* 0x60C */ int mType;
    /* 0x610 */ f32 mPrevSpeedY;
    /* 0x614 */ u8 mMode;
    /* 0x615 */ u8 mDeleteState;
    /* 0x616 */ u8 mFirstLanding;
    /* 0x617 */ u8 mUnused[5];
    /* 0x61C */ LIGHT_INFLUENCE mLight;
};

STATIC_ASSERT(sizeof(daObjVtil_c) == 0x63C);

#endif /* D_A_OBJ_VTIL_H */
