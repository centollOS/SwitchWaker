#ifndef D_A_OBJ_TRAP_H
#define D_A_OBJ_TRAP_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_bg_w.h"
#include "d/d_path.h"
#include "m_Do/m_Do_ext.h"

class daObjTrap_c : public fopAc_ac_c {
public:
    bool check_shine() const { return mShine == 1; }
    void start_shine() { mShine = 1; }
    void stop_shine() { mShine = 0; }

    static BOOL solidHeapCB(fopAc_ac_c*);
    BOOL create_heap();
    cPhs_State _create();
    bool _delete();
    void init_mtx();
    void set_co_pos();
    void get_ground();
    BOOL circle_search();
    void set_move_info();
    BOOL check_arrival();
    cXyz check_wall();
    BOOL check_block_target_pos(cXyz*);
    cXyz check_block(cXyz);
    void set_vib_mode();
    void vibrate();
    void bound();
    void set_shine();
    void shine_move();
    bool _execute();
    bool _draw();

public:
    static const f32 M_speed_table[3];
    static const s16 M_wait_f_table[3];
    static const char M_arcname[];
    /* 0x290 */ J3DModel* mpModel;
    /* 0x294 */ mDoExt_btkAnm mBtk;
    /* 0x2A8 */ request_of_phase_process_class mPhase;
    /* 0x2B0 */ dCcD_Stts mStts;
    /* 0x2EC */ dCcD_Cyl mCyl;
    /* 0x41C */ dBgS_ObjGndChk mGndChk;
    /* 0x470 */ f32 mGroundY;
    /* 0x474 */ dPath* mpPath;
    /* 0x478 */ cXyz mTarget;
    /* 0x484 */ cXyz mStart;
    /* 0x490 */ cXyz mDirection;
    /* 0x49C */ cXyz mMove;
    /* 0x4A8 */ cXyz mFastMove;
    /* 0x4B4 */ cXyz mNextPos;
    /* 0x4C0 */ cXyz mBounceOffset;
    /* 0x4CC */ f32 mPathLength;
    /* 0x4D0 */ s16 mWaitTimer;
    /* 0x4D2 */ u16 mBounceTimer;
    /* 0x4D4 */ f32 mBounceAmplitude;
    /* 0x4D8 */ u16 mVibrateTimer;
    /* 0x4DA */ s16 mOriginalPitch;
    /* 0x4DC */ u8 mPathPoint;
    /* 0x4DD */ u8 mPathNo;
    /* 0x4DE */ u8 mDirectionValid;
    /* 0x4DF */ u8 mMode;
    /* 0x4E0 */ u8 mShine;
    /* 0x4E1 */ u8 mSpeedType;
    /* 0x4E4 */ f32 mMoveSpeed;
    /* 0x4E8 */ s16 mWaitFrames;
    /* 0x4EC */ dBgW* mpBgW;
};

STATIC_ASSERT(sizeof(daObjTrap_c) == 0x4F0);

#endif /* D_A_OBJ_TRAP_H */
