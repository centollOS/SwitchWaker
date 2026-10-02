#ifndef D_A_OBJ_BALANCELIFT_H
#define D_A_OBJ_BALANCELIFT_H

#include "f_op/f_op_actor.h"
#include "d/d_bg_w.h"
#include "d/d_cc_d.h"
#include "d/d_chain.h"
#include "d/d_path.h"
#include "m_Do/m_Do_ext.h"

class daBalancelift_c : public fopAc_ac_c {
public:
    inline cPhs_State _create();
    inline bool _draw();
    inline bool _execute();
    void calc_quat();
    void set_mtx();
    void calc_weight();
    BOOL CreateHeap();
    cPhs_State CreateInit();
    static const char M_arcname[];

    /* 0x290 */ cXyz mTop;
    /* 0x29C */ cXyz mChainOffset;
    /* 0x2A8 */ cXyz mChainTarget;
    /* 0x2B4 */ cXyz mChainVelocity;
    /* 0x2C0 */ cXyz mChainJoint;
    /* 0x2CC */ Quaternion mSwingQuat;
    /* 0x2DC */ Quaternion mPlatformQuat;
    /* 0x2EC */ cXyz mSwingPos;
    /* 0x2F8 */ cXyz mSwingVelocity;
    /* 0x304 */ cXyz mPlatformPos;
    /* 0x310 */ f32 mLength;
    /* 0x314 */ s16 mWeight;
    /* 0x318 */ s16* mpWeight;
    /* 0x31C */ s16 mSide;
    /* 0x31E */ s16 mUpdateFlags;
    /* 0x320 */ s16* mpUpdateFlags;
    /* 0x324 */ request_of_phase_process_class mPhase;
    /* 0x32C */ J3DModel* M_mdl;
    /* 0x330 */ Mtx mBgMtx;
    /* 0x360 */ dPath* mpPath;
    /* 0x364 */ dBgW* pm_bgw;
    /* 0x368 */ f32 mLengthVelocity;
    /* 0x36C */ cXyz mLinePoints[4];
    /* 0x39C */ mDoExt_3DlineMat0_c mLine;
    /* 0x3B8 */ dChain_packet_c* m_chain;
    /* 0x3BC */ dCcD_Stts mStts;
    /* 0x3F8 */ dCcD_Cyl mCyl;
};

STATIC_ASSERT(sizeof(daBalancelift_c) == 0x528);

#endif /* D_A_OBJ_BALANCELIFT_H */
