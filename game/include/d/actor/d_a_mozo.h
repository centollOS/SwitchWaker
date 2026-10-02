#ifndef D_A_MOZO_H
#define D_A_MOZO_H

#include "f_op/f_op_actor.h"
#include "SSystem/SComponent/c_phase.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_cc_d.h"
#include "m_Do/m_Do_hostIO.h"

class JPABaseEmitter;

class daMozo_c : public fopAc_ac_c {
public:
    typedef void (daMozo_c::*proc_t)();

    void setProcess(proc_t proc) { mCurrentProc = proc; }

    cPhs_State _create();
    bool _delete();
    bool _draw();
    bool _execute();
    void anime_proc();
    BOOL checkRange(int);
    BOOL CreateHeap();
    cPhs_State CreateInit();
    void event_move();
    fopAc_ac_c* getBeamActor(fpc_ProcID);
    void search_beam_proc_init();
    void search_beam_proc();
    void search_fire_proc_init();
    void search_fire_proc();
    void towait_proc_init();
    void towait_proc();
    void wait_proc_init();
    void wait_proc();
    void set_mtx();
    void setAnm(int, float);

    static u8 m_event_flag;

public:
    /* 0x290 */ proc_t mCurrentProc;
    /* 0x29C */ request_of_phase_process_class mPhs;
    /* 0x2A4 */ mDoExt_McaMorf* mAnimMorf;
    /* 0x2A8 */ mDoExt_brkAnm mBrkAnm;
    /* 0x2C0 */ J3DAnmTevRegKey* m_brk;
    /* 0x2C4 */ mDoExt_btkAnm mBtkAnm;
    /* 0x2D8 */ J3DAnmTextureSRTKey* m_btk;
    /* 0x2DC */ cXyz mBeamStart;
    /* 0x2E8 */ cXyz mBeamEnd;
    /* 0x2F4 */ cXyz mBeamLStart;
    /* 0x300 */ cXyz mBeamRStart;
    /* 0x30C */ cXyz mHeadPos;
    /* 0x318 */ cXyz mHeadEndPos;
    /* 0x324 */ u8 m324[0x330 - 0x324];
    /* 0x330 */ cXyz mFireStart;
    /* 0x33C */ cXyz mFireEnd;
    /* 0x348 */ cXyz mSePos;
    /* 0x354 */ fpc_ProcID mBeamID1;
    /* 0x358 */ fpc_ProcID mBeamID2;
    /* 0x35C */ Quaternion mQuatRotation;
    /* 0x36C */ int mAnm;
    /* 0x370 */ s16 mFireTimer;
    /* 0x372 */ u8 m372[0x376 - 0x372];
    /* 0x376 */ u8 mType;
    /* 0x377 */ u8 m377;
    /* 0x378 */ JPABaseEmitter* mFireEmitter0;
    /* 0x37C */ JPABaseEmitter* mFireEmitter1;
    /* 0x380 */ dCcD_Stts mStts;
    /* 0x3BC */ dCcD_Cps mCps;
};

STATIC_ASSERT(sizeof(daMozo_c) == 0x4F4);

#endif /* D_A_MOZO_H */
