#ifndef D_A_NPC_AUCTION_H
#define D_A_NPC_AUCTION_H

#include "f_op/f_op_actor.h"
#include "d/d_npc.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"
#include "JSystem/JParticle/JPAEmitter.h"

struct sAnmDat {
    /* 0x00 */ u8 mAnm;
    /* 0x01 */ u8 mMorf;
    /* 0x02 */ s8 mCount;
}; // Size: 0x03

class daNpcAuction_c : public fopNpc_npc_c {
public:
    typedef void (daNpcAuction_c::*ActionFunc)();

    daNpcAuction_c();

    inline cPhs_State _create();
    inline BOOL _delete();
    inline BOOL _draw();
    inline BOOL _execute();

    u8 getMdlNo() { return mMdlNo; }
    u8 getNpcNo() { return mNpcNo; }
    request_of_phase_process_class* getPhaseP() { return &mPhs; }
    f32 getPiconOfsY() { return mPiconOfsY; }
    void setAction(ActionFunc func) { mAction = func; }
    void setBmtNo(int no) { mBmtNo = no; }
    void setBtpNo(int no) { mBtpNo = no; }
    void setMdlNo(u8 no) { mMdlNo = no; }
    void setNpcNo(u8 no) { mNpcNo = no; }

    BOOL createHeap();
    cPhs_State createInit();
    void wait_action_init();
    void wait_action();
    void checkOrder();
    void eventOrder();
    void eventMove();
    void privateCut();
    void eventMainInit();
    bool eventMain();
    void eventMesSetInit(int);
    bool eventMesSet();
    s16 XyCheckCB(int);
    s16 XyEventCB(int);
    u16 next_msgStatus(u32*);
    u32 getMsg();
    void setAnmFromMsgTag();
    u8 getPrmNpcNo();
    void setMtx();
    void lookBack();
    BOOL initTexPatternAnm(bool);
    void playTexPatternAnm();
    void playAnm();
    void setAnm(u8, int, f32);
    void setAnm2(u8, int, f32);
    void setAnmTbl(sAnmDat*);
    BOOL isExecute();
    int getRand(int);
    void clrEmitter();

    static const char m_arcname[4];

public:
    /* 0x6C4 */ ActionFunc mAction;
    /* 0x6D0 */ request_of_phase_process_class mPhs;
    /* 0x6D8 */ request_of_phase_process_class mPhs2;
    /* 0x6E0 */ J3DModel* mpHeadModel;
    /* 0x6E4 */ J3DAnmTexPattern* m_head_tex_pattern;
    /* 0x6E8 */ mDoExt_btpAnm mBtpAnm;
    /* 0x6FC */ u32 mShadowId;
    /* 0x700 */ JPABaseEmitter* mpEmitter;
    /* 0x704 */ void* m704;
    /* 0x708 */ cXyz mLookPos;
    /* 0x714 */ u8 m714[0x71C - 0x714];
    /* 0x71C */ sAnmDat* mpAnmDat;
    /* 0x720 */ f32 mMaxAttnDistXZ;
    /* 0x724 */ f32 mPiconOfsY;
    /* 0x728 */ u32 mSeNo;
    /* 0x72C */ int mBtpNo;
    /* 0x730 */ int mBmtNo;
    /* 0x734 */ s16 mBtpTimer;
    /* 0x736 */ s16 mCurrentTurnSpeed;
    /* 0x738 */ s16 mEventIdx;
    /* 0x73A */ s16 mSeTimer;
    /* 0x73C */ u8 mTalking;
    /* 0x73D */ u8 m73D;
    /* 0x73E */ u8 mEventOrder;
    /* 0x73F */ u8 mBtpFrame;
    /* 0x740 */ u8 mNpcNo;
    /* 0x741 */ u8 mMdlNo;
    /* 0x742 */ u8 m742;
    /* 0x743 */ u8 m743;
    /* 0x744 */ u8 m744;
    /* 0x745 */ u8 m745;
    /* 0x746 */ u8 mAnmNo;
    /* 0x747 */ u8 m747;
    /* 0x748 */ s8 mAnmCount;
    /* 0x749 */ u8 m749;
    /* 0x74A */ s8 mCutAct;
    /* 0x74B */ u8 m74B;
}; // Size: 0x74C

STATIC_ASSERT(sizeof(daNpcAuction_c) == 0x74C);

#endif /* D_A_NPC_AUCTION_H */
