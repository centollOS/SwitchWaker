#ifndef D_A_ICELIFT_H
#define D_A_ICELIFT_H

#include "f_op/f_op_actor.h"
#include "d/d_bg_s_acch.h"
#include "d/d_path.h"

class daIlift_c : public fopAc_ac_c {
public:
    bool _delete();
    BOOL CreateHeap();
    void CreateInit();
    cPhs_State _create();
    void set_mtx();
    bool _execute();
    void lift_wave();
    void path_move();
    void lift_normal_move();
    void set_next_pnt();
    bool _draw();

public:
    static const char* m_arcname[2];
    static const int m_bmdidx[2];
    static const int m_dzbidx[2];
    static const u32 m_heapsize[2];
    static const f32 m_down_param;
    static const f32 m_max_speed;
    /* 0x290 */ request_of_phase_process_class mPhs;
    /* 0x298 */ J3DModel* mModel;
    /* 0x29C */ dBgW* mBgW;
    /* 0x2A0 */ Mtx mMtx;
    /* 0x2D0 */ dBgS_ObjAcch mAcch;
    /* 0x494 */ dBgS_AcchCir mAcchCir;
    /* 0x4D4 */ u8 mRidden;
    /* 0x4D5 */ u8 mWasRidden;
    /* 0x4D6 */ s16 mTiltAngle;
    /* 0x4D8 */ s16 mTiltTarget;
    /* 0x4DC */ Quaternion mTargetQuat;
    /* 0x4EC */ Quaternion mQuat;
    /* 0x4FC */ cXyz mRideOffset;
    /* 0x508 */ f32 mRideStrength;
    /* 0x50C */ s32 mFrame;
    /* 0x510 */ s32 mWaveTimer;
    /* 0x514 */ u8 m514[4];
    /* 0x518 */ u8 mType;
    /* 0x51C */ cXyz mTargetPos;
    /* 0x528 */ cXyz mPreviousTarget;
    /* 0x534 */ dPath* mPath;
    /* 0x538 */ f32 mMaxSpeed;
    /* 0x53C */ f32 mMoveSpeed;
    /* 0x540 */ u16 mMoveTimer;
    /* 0x542 */ u8 mPathId;
    /* 0x543 */ s8 mPointIndex;
    /* 0x544 */ s8 mPathDirection;
    /* 0x545 */ u8 mMoveState;
    /* 0x548 */ u8 m548[4];
    /* 0x54C */ f32 mRideDistance;
    /* 0x550 */ u8 m550[4];
    /* 0x554 */ s32 mSwitch;
    /* 0x558 */ cXyz mPreviousPlayerPos;
};

#endif /* D_A_ICELIFT_H */
