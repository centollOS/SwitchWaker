#ifndef D_A_TAG_KF1_H
#define D_A_TAG_KF1_H

#include "d/d_npc.h"

class daTag_Kf1_c : public fopNpc_npc_c {
public:
    BOOL createInit();
    void setStt(signed char);
    u16 next_msgStatus(unsigned long*);
    void eventOrder();
    void checkOrder();
    BOOL chkAttention(cXyz);
    BOOL partner_srch();
    s16 checkPartner();
    void goto_nextStage();
    void event_talkInit(int);
    bool event_mesSet();
    bool event_mesEnd();
    void bensyoInit();
    bool event_bensyo();
    void event_cntTsubo();
    void privateCut();
    void event_proc();
    BOOL set_action(int (daTag_Kf1_c::*)(void*), void*);
    BOOL wait01();
    BOOL wait02();
    int wait_action1(void*);
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();

public:
    /* 0x6C4 */ int (daTag_Kf1_c::*mAction)(void*);
    /* 0x6D0 */ dNpc_EventCut_c mCut;
    /* 0x73C */ u8 mInRange;
    /* 0x73E */ s16 mBrokenCount;
    /* 0x740 */ u8 mUnused740[2];
    /* 0x742 */ u16 mRupees;
    /* 0x744 */ fpc_ProcID mPotIds[8];
    /* 0x764 */ s16 mPotCount;
    /* 0x766 */ s8 mCutIdx;
    /* 0x767 */ s8 mOrder;
    /* 0x768 */ s8 mState;
    /* 0x769 */ u8 mType;
    /* 0x76A */ s8 mActionState;
};

#endif /* D_A_TAG_KF1_H */
