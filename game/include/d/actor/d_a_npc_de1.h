#ifndef D_A_NPC_DE1_H
#define D_A_NPC_DE1_H

#include "d/d_npc.h"
#include "d/d_bg_w_deform.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_hostIO.h"

class daNpc_De1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm;
        f32 mMorf;
        f32 mSpeed;
        int mLoop;
    };

    enum { ANM_END = 7 };
    typedef int (daNpc_De1_c::*ActionFunc)(void*);
    bool createInit();
    void setMtx();
    bool anmResID(int, int*, int*);
    bool setAnm_anm(anm_prm_c*);
    void setAnm_NUM(int);
    void setAnm();
    void chngAnmTag();
    void ctrlAnmTag();
    void chngAnmAtr(unsigned char);
    void ctrlAnmAtr();
    void setAnm_ATR();
    virtual void anmAtr(unsigned short);
    void setStt(signed char);
    virtual u16 next_msgStatus(unsigned long*);
    virtual u32 getMsg();
    void eventOrder();
    void checkOrder();
    bool chkAttention();
    void setAttention();
    fopAc_ac_c* searchByID(fpc_ProcID);
    void setDemoStartCenter();
    bool partner_srch();
    void ccCreate();
    void cc_set();
    void set_pa_happa();
    void del_pa_happa();
    void followPa_happa();
    bool decideType(int);
    void event_actionInit(int);
    bool event_action();
    void privateCut();
    void endEvent();
    void event_proc();
    bool set_action(int (daNpc_De1_c::*)(void*), void*);
    int wait01();
    int wait02();
    int wait03();
    int wait04();
    int wait05();
    int talk01();
    int talk02();
    int wait_action1(void*);
    int wait_action2(void*);
    u8 demo();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    int CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhase;
    /* 0x6CC */ s8 m_head_jnt_num;
    /* 0x6CD */ s8 m_branchL_jnt_num;
    /* 0x6D0 */ dBgWDeform* mpBgW;
    /* 0x6D4 */ ActionFunc mAction;
    /* 0x6E0 */ dNpc_EventCut_c mEventCut;
    /* 0x74C */ csXyz mInitialAngle;
    /* 0x754 */ cXyz mInitialPos;
    /* 0x760 */ cXyz mHeadPos;
    /* 0x76C */ cXyz mEyePos;
    /* 0x778 */ cXyz mHomePos;
    /* 0x784 */ cXyz mDemoCenter;
    /* 0x790 */ f32 mFrame;
    /* 0x794 */ u8 m794[4];
    /* 0x798 */ s16 mSoundTimer;
    /* 0x79A */ s16 mEventTimer;
    /* 0x79C */ s16 m79C;
    /* 0x79E */ s16 mChuIndex;
    /* 0x7A0 */ u8 m7A0[4];
    /* 0x7A4 */ s8 mAnmEnd;
    /* 0x7A5 */ s8 mAnmLoops;
    /* 0x7A6 */ u8 mItemNo;
    /* 0x7A7 */ u8 mMet;
    /* 0x7A8 */ u8 mSavedInitial;
    /* 0x7A9 */ u8 m7A9[3];
    /* 0x7AC */ int m7AC;
    /* 0x7B0 */ u8 m7B0[4];
    /* 0x7B4 */ u8 mAttention;
    /* 0x7B5 */ u8 mTalking;
    /* 0x7B6 */ u8 m7B6;
    /* 0x7B7 */ u8 mDemo;
    /* 0x7B8 */ u8 m7B8;
    /* 0x7B9 */ u8 m7B9[3];
#if VERSION == VERSION_DEMO
    /* 0x7BC */ JPABaseEmitter* mpLeavesEmitter;
#endif
    /* 0x7BC */ dPa_followEcallBack mLeaves;
    /* 0x7D0 */ cXyz mLeavesPos;
    /* 0x7DC */ fpc_ProcID m_cc_ID[10];
    /* 0x804 */ s8 m_c0_jnt_num[10];
    /* 0x810 */ fpc_ProcID mLiftID;
    /* 0x814 */ s8 mCut;
    /* 0x815 */ s8 mEventAction;
    /* 0x816 */ u8 mAnmAttr;
    /* 0x817 */ u8 mAnmTag;
    /* 0x818 */ s8 mAnmNo;
    /* 0x819 */ s8 mOrder;
    /* 0x81A */ s8 mState;
    /* 0x81B */ s8 mPreviousState;
    /* 0x81C */ s8 mType;
    /* 0x81D */ s8 mSubType;
    /* 0x81E */ s8 mActionState;
    /* 0x81F */ s8 mMsgAnmStarted;
};

STATIC_ASSERT(sizeof(daNpc_De1_c) == DEMO_SELECT(0x824, 0x820));

class daNpc_De1_HIO_c : public JORReflexible {
public:
    daNpc_De1_HIO_c();

public:
    inline virtual ~daNpc_De1_HIO_c();
    struct hio_prm_c {
        f32 mAttentionOffset, mAttentionRadius, mScale, mDemoRadius;
        Vec mDemoOffset;
        u8 mDebug;
    };
    s8 mChild;
    int mCount;
    hio_prm_c mPrm;
};

#endif /* D_A_NPC_DE1_H */
