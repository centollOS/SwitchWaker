#ifndef D_A_NPC_SV_H
#define D_A_NPC_SV_H

#include "d/d_npc.h"
#include "d/d_cc_d.h"

struct sSvAnmDat {
    u8 mAnm;
    u8 mMorf;
    s8 mCount;
};

class daNpcSv_c : public fopNpc_npc_c {
public:
    u8 getMsgPtn() { return mMsgPattern; }
    u8 getNpcNo() { return mNpcNo; }
    request_of_phase_process_class* getPhaseP() { return &mPhase; }
    void setResFlag(u8 flag) { mResFlag = flag; }

    daNpcSv_c();
    cPhs_State _create();
    BOOL createHeap();
    cPhs_State createInit();
    bool _delete();
    bool _draw();
    bool _execute();
    BOOL executeCommon();
    void executeSetMode(unsigned char);
    u8 executeWaitInit();
    void executeWait();
    u8 executeTalkInit();
    void executeTalk();
    void checkOrder();
    void eventOrder();
    void eventMove();
    void privateCut();
    void eventMesSetInit(int);
    BOOL eventMesSet();
    void eventGetItemInit(int);
    BOOL eventGetItem();
    void eventSetAngleInit();
    void eventAttentionInit(int);
    BOOL eventAttention();
    void eventTurnOkInit();
    u16 talk2(int, fopAc_ac_c*);
    virtual u16 next_msgStatus(unsigned long*);
    virtual u32 getMsg();
    void setMessage(unsigned long);
    void setAnmFromMsgTag();
    u8 getPrmNpcNo();
    void setMtx();
    void chkAttention();
    void lookBack();
    void playAnm();
    void setAnm(unsigned char, int, float);
    bool setAnmTbl(sSvAnmDat*);
    void setCollision(dCcD_Cyl*, cXyz, float, float);
    u8 getTalkNo();
    BOOL isTalkOK();

public:
    static const char m_arcname[];
    /* 0x6C4 */ request_of_phase_process_class mPhase;
    /* 0x6CC */ request_of_phase_process_class mCreatePhase;
    /* 0x6D4 */ u32 mShadowId;
    /* 0x6D8 */ fopAc_ac_c* mpAttentionActor;
    /* 0x6DC */ u8 m6DC[0x10];
    /* 0x6EC */ cXyz mLookPos;
    /* 0x6F8 */ u8 m6F8[0xC];
    /* 0x704 */ sSvAnmDat* mpAnm;
    /* 0x708 */ u32* mpMsg;
    /* 0x70C */ f32 mMorf;
    /* 0x710 */ f32 mAttentionDistance;
    /* 0x714 */ int mItem;
    /* 0x718 */ int mCraneFrame;
    /* 0x71C */ bool mNoTurn;
    /* 0x71E */ s16 mEventIds[3];
    /* 0x724 */ s16 mLookTimer;
    /* 0x726 */ s16 mAttentionAngle;
    /* 0x728 */ s16 mInitialAngle;
    /* 0x72A */ s16 mEventTimer;
    /* 0x72C */ s16 mEventAngle;
    /* 0x72E */ s16 mTurnSpeed;
    /* 0x730 */ s16 mCurrentTurnSpeed;
    /* 0x732 */ s16 mLookAngle;
    /* 0x734 */ u16 mTalkFlags;
    /* 0x736 */ u8 mTalking;
    /* 0x737 */ u8 mHasAttention;
    /* 0x738 */ u8 mEventOrder;
    /* 0x739 */ u8 mMode;
    /* 0x73A */ u8 mResFlag;
    /* 0x73B */ u8 mNpcNo;
    /* 0x73C */ u8 mEventFlags;
    /* 0x73D */ u8 mAnmIndex;
    /* 0x73E */ u8 mAnmFlags;
    /* 0x73F */ s8 mAnmCount;
    /* 0x740 */ s8 mCut;
    /* 0x741 */ s8 mLookMode;
    /* 0x742 */ u8 mTalkEnabled;
    /* 0x743 */ u8 mCanTurn;
    /* 0x744 */ u8 mCanLook;
    /* 0x745 */ u8 mNearPlayer;
    /* 0x746 */ u8 mTalkNo;
    /* 0x747 */ u8 mMsgPattern;
    /* 0x748 */ u8 mCraneWait;
    /* 0x749 */ u8 mStopped;
    /* 0x74A */ u8 mPreviousNearPlayer;
};

STATIC_ASSERT(sizeof(daNpcSv_c) == 0x74C);

#endif /* D_A_NPC_SV_H */
