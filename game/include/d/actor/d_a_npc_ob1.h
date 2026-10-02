#ifndef D_A_NPC_OB1_H
#define D_A_NPC_OB1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Ob1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm, mTex;
        f32 mMorf, mSpeed;
        int mLoop, mFrame;
    };

    void nodeOb1Control(J3DNode*, J3DModel*);
    bool init_OB1_0();
    bool init_OB1_1();
    bool init_OB1_2();
    bool createInit();
    void setMtx(bool);
    int anmNum_toResID(int);
    int btpNum_toResID(int);
    bool setBtp(bool, int);
    bool iniTexPttrnAnm(bool);
    void plyTexPttrnAnm();
    void setAnm_tex(signed char);
    BOOL setAnm_anm(anm_prm_c*);
    BOOL setAnm_NUM(int, int);
    BOOL setAnm();
    void chg_anmTag();
    void control_anmTag();
    void chg_anmAtr(unsigned char);
    void control_anmAtr();
    BOOL setAnm_ATR(int);
    virtual void anmAtr(unsigned short);
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    bool chk_partsNotMove();
    void lookBack();
    virtual u16 next_msgStatus(unsigned long*);
    u32 getMsg_OB1_0();
    u32 getMsg_OB1_1();
    u32 getMsg_OB1_2();
    virtual u32 getMsg();
    bool chkAttention();
    void setAttention(bool);
    fopAc_ac_c* searchByID(fpc_ProcID);
    void partner_srch();
    s8 bitCount(unsigned char);
    void set_pigCnt();
    void ob_setPthPos();
    cXyz get_attPos();
    int ob_movPass();
    void ob_clcMovSpd();
    void ob_nMove();
    bool charDecide(int);
    void event_actionInit(int);
    bool event_action();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Ob1_c::*)(void*), void*);
    void clrSpd();
    void setStt(signed char);
    BOOL wait_1();
    BOOL wait_2();
    BOOL wait_3();
    BOOL walk_1();
    BOOL talk_1();
    BOOL manzai();
    int wait_action1(void*);
    int wait_action2(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    J3DModelData* create_Anm();
    J3DModelData* create_hed_Mdl();
    BOOL CreateHeap();

public:
    request_of_phase_process_class mPhase;
    s8 m_hed_jnt_num, m_bbone_jnt_num;
    u32 mShadowId;
    J3DModel* mpHeadModel;
    J3DAnmTexPattern* m_hed_tex_pttrn;
    mDoExt_btpAnm mBtp;
    u8 mBtpFrame;
    s16 mBlinkTimer;
    int (daNpc_Ob1_c::*mAction)(void*);
    dNpc_PathRun_c mPath;
    dNpc_EventCut_c mCut;
    fpc_ProcID mPartnerID;
    cXyz mInitialPos;
    csXyz mInitialAngle, mModelAngle;
    cXyz mEyeLocal, mLookPos, mMoveTarget;
    f32 mLastFrame, mTargetSpeed, mSpeedStep, mArriveDistance;
    s16 mOldHeadY, mOldBackY, mOldActorY;
    u8 m7CA[2];
    u32 mPairedMsgNo;
    s16 mEventIds[1], mEventIndex;
    u8 m7D4[2];
    s16 mAttentionTimer;
    u8 m7D8[2];
    s16 mWaitTimer;
    u8 m7DC[2];
    s16 mTurnSpeed, mLookAngle;
    u16 mPreviousMsgStatus;
    s8 mAnmEnded, mAnmRepeat;
    u8 m7E6;
    s8 mSavedPigCount, mPresentPigCount, mNewPigCount, mMessageCycle;
    u8 mPresentItem, mSetPigEvent, mUsePartnerMessage, mHidden, mFreezeAngle, mStopMove, mNoDraw, mArrived, mInitialSet;
    int mUpdateEye;
    bool mHasAttention, mTalking, mNoTurn;
    u8 mDemo;
    s8 mMoveMode;
    u8 m7FD;
    s8 mCutIndex;
    u8 mEventAction, mAnmAttr, mAnmTag;
    s8 mTexIndex, mAnmIndex, mEventOrder, mState, mPreviousState, mLookMode, mType, mSubtype, mActionState, mMessageState;
};

class daNpc_Ob1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackX, mMaxBackY, mMinBackX, mMinBackY;
        s16 mMaxTurn, mTurnSpeed;
        f32 mAttentionYOffset;
        u8 mDebug;
        s16 mAngleScale, mAngleMax;
        f32 mAnimSpeedScale, mTargetSpeed, mSpeedStep, mArriveDistance;
    };
    daNpc_Ob1_HIO_c();
    inline virtual ~daNpc_Ob1_HIO_c();

public:
    s8 mNo;
    int mCount;
    hio_prm_c mPrm;
};

STATIC_ASSERT(sizeof(daNpc_Ob1_c) == 0x80C);

#endif /* D_A_NPC_OB1_H */
