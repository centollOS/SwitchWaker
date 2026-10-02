#ifndef D_A_NPC_SARACE_H
#define D_A_NPC_SARACE_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class daNpc_Sarace_c : public fopNpc_npc_c {
public:
    cXyz* getAttentionBasePos() { return &mAttentionBasePos; }
    cXyz* getEyePos() { return &mEyePos; }
    void setAction(int (daNpc_Sarace_c::*action)(void*), void* arg) {
        if (mAction != action) {
            if (mAction != NULL) {
                mActionState = -1;
                (this->*mAction)(arg);
            }
            mAction = action;
            mActionState = 0;
            (this->*mAction)(arg);
        }
    }
    BOOL initTexPatternAnm(bool);
    void playTexPatternAnm();
    void setAnm(s8, f32);
    bool chkAttention(cXyz, s16);
    void eventOrder();
    void checkOrder();
    virtual u16 next_msgStatus(u32*);
    virtual u32 getMsg();
    virtual void anmAtr(u16);
    BOOL CreateInit();
    void setAttention();
    void lookBack();
    void wait01();
    void talk01();
    int dummy_action(void*);
    int wait_action(void*);
    int event_endCheck_action(void*);
    void set_mtx();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL CreateHeap();
    static s32 ship_race_rupee;
    static s32 ship_race_result;

public:
    /* 0x6C4 */ request_of_phase_process_class mPhase;
    /* 0x6CC */ mDoExt_McaMorf* mpHeadMorf;
    /* 0x6D0 */ u32 mShadowId;
    /* 0x6D4 */ u8 m6D4[4];
    /* 0x6D8 */ fpc_ProcID mHBarrelId;
    /* 0x6DC */ fpc_ProcID mVBarrelId;
    /* 0x6E0 */ J3DAnmTexPattern* m_btp;
    /* 0x6E4 */ mDoExt_btpAnm mBtp;
    /* 0x6F8 */ u8 mBtpFrame;
    /* 0x6FA */ s16 mBlinkTimer;
    /* 0x6FC */ cXyz mEyePos;
    /* 0x708 */ cXyz mAttentionBasePos;
    /* 0x714 */ s16 mTurn;
    /* 0x716 */ u8 m716[2];
    /* 0x718 */ csXyz mInitialAngle;
    /* 0x71E */ u8 m71E[6];
    /* 0x724 */ u32 mNextMsg;
    /* 0x728 */ bool mHasAttention;
    /* 0x729 */ bool mTalking;
    /* 0x72A */ u8 m72A[6];
    /* 0x730 */ u8 m730;
    /* 0x734 */ int (daNpc_Sarace_c::*mAction)(void*);
    /* 0x740 */ s8 mTexIndex;
    /* 0x741 */ s8 mAnmIndex;
    /* 0x742 */ s8 mEventOrder;
    /* 0x743 */ s8 mState;
    /* 0x744 */ u8 m744[2];
    /* 0x746 */ s8 mActionState;
};

STATIC_ASSERT(sizeof(daNpc_Sarace_c) == 0x748);
#endif /* D_A_NPC_SARACE_H */
