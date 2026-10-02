#ifndef D_A_WATERFALL_H
#define D_A_WATERFALL_H

#include "f_op/f_op_actor.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_particle.h"

class daWfall_c : public fopAc_ac_c {
public:
    inline bool _draw();

    bool _delete();
    BOOL CreateHeap();
    void CreateInit();
    cPhs_State _create();
    void set_mtx();
    void set_gate_mtx();
    void set_minamo_mtx();
    bool _execute();
    void mode_proc_call();
    void mode_wtr_on_init();
    void mode_wtr_on();
    void mode_wtr_off_init();
    void mode_wtr_off();
    BOOL setEmitter00Pos();
    BOOL setEmitter01Pos();
    f32 getWaterScaleFromGatePos();
    f32 getWaterHeight();
    void set_se();

public:
    static const char m_arcname[];
    static const u8 m_wait_timer;
    static const s16 m_heapsize[2];
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ J3DModel* mpWaterModel;
    /* 0x29C */ J3DModel* mpGateModel;
    /* 0x2A0 */ J3DModel* mpMinamoModel;
    /* 0x2A4 */ s32 mMode;
    /* 0x2A8 */ s32 mSwitch;
    /* 0x2AC */ mDoExt_btkAnm mWaterBtk;
    /* 0x2C0 */ mDoExt_brkAnm mMinamoBrk;
    /* 0x2D8 */ mDoExt_btkAnm mMinamoBtk;
#if VERSION == VERSION_DEMO
    /* 0x2EC */ JPABaseEmitter* mpEmitter00;
    /* 0x2F0 */ JPABaseEmitter* mpEmitter01;
#endif
    /* 0x2EC */ dPa_followEcallBack mEmitter00;
    /* 0x300 */ dPa_followEcallBack mEmitter01;
    /* 0x314 */ cXyz mEmitter00Pos;
    /* 0x320 */ cXyz mEmitter01Pos;
    /* 0x32C */ cXyz mUnusedPos;
    /* 0x338 */ csXyz mEmitter00Angle;
    /* 0x33E */ csXyz mEmitter01Angle;
    /* 0x344 */ cXyz mGatePos;
    /* 0x350 */ s16 mEvent;
    /* 0x352 */ s16 mTimer;
    /* 0x354 */ u8 mUnused;
    /* 0x355 */ u8 mEventState;
    /* 0x356 */ u8 mType;
};

STATIC_ASSERT(sizeof(daWfall_c) == DEMO_SELECT(0x360, 0x358));

#endif /* D_A_WATERFALL_H */
