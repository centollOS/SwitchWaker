#ifndef D_A_NPC_KP1_H
#define D_A_NPC_KP1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class daNpc_Kp1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm, mTex;
        f32 mMorf, mSpeed;
        int mLoop;
    };

    enum { ANM_END = 6, TEXPATTERN_END = 2 };
    typedef int (daNpc_Kp1_c::*ActionFunc)(void*);
    cXyz* getAttPos() { return &mHeadPos; }
    cXyz* getEyePos() { return &mEyePos; }
    s8 getHeadJntNum() { return m_head_jnt_num; }
    s8 getBackboneJntNum() { return m_backbone_jnt_num; }
    s16 getHead_x() { return m_jnt.getHead_x(); }
    s16 getHead_y() { return m_jnt.getHead_y(); }
    s16 getBackbone_x() { return m_jnt.getBackbone_x(); }
    s16 getBackbone_y() { return m_jnt.getBackbone_y(); }

    bool createInit();
    void setMtx();
    bool anmResID(int, int*, int*);
    void BtpNum2ResID(int, int*);
    void setAnm_tex(signed char);
    bool init_btp(bool, int);
    bool initTexPatternAnm(bool);
    void playTexPatternAnm();
    bool setAnm_anm(anm_prm_c*);
    void setAnm_NUM(int, int);
    void setAnm();
    void chngAnmTag();
    void ctrlAnmTag();
    void chngAnmAtr(unsigned char);
    void ctrlAnmAtr();
    void setAnm_ATR(int);
    virtual void anmAtr(u16);
    void setStt(signed char);
    virtual u16 next_msgStatus(u32*);
    virtual u32 getMsg();
    void eventOrder();
    void checkOrder();
    void lookBack();
    bool chkAttention();
    void setAttention();
    bool chk_talk();
    bool decideType(int);
    void event_actionInit(int);
    bool event_action();
    void privateCut();
    void endEvent();
    void event_proc();
    bool set_action(int (daNpc_Kp1_c::*)(void*), void*);
    int wait01();
    int talk01();
    int wait_action1(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    int CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhase;
    /* 0x6CC */ s8 m_head_jnt_num;
    /* 0x6CD */ s8 m_backbone_jnt_num;
    /* 0x6CE */ s8 m_handL_jnt_num;
    /* 0x6D0 */ J3DModel* mpLetterModel;
    /* 0x6D4 */ J3DModel* mpOpenLetterModel;
    /* 0x6D8 */ u8 mLetterMode;
    /* 0x6DC */ u32 mShadow;
    /* 0x6E0 */ J3DAnmTexPattern* m_head_tex_pattern;
    /* 0x6E4 */ mDoExt_btpAnm mBtp;
    /* 0x6F8 */ u8 mBtpFrame;
    /* 0x6FA */ s16 mBlinkTimer;
    /* 0x6FC */ ActionFunc mAction;
    /* 0x708 */ dNpc_EventCut_c mEventCut;
    /* 0x774 */ csXyz mInitialAngle;
    /* 0x77C */ cXyz mInitialPos;
    /* 0x788 */ cXyz mHeadPos;
    /* 0x794 */ cXyz mEyePos;
    /* 0x7A0 */ cXyz mLookPos;
    /* 0x7AC */ f32 mFrame;
    /* 0x7B0 */ u8 m7B0[8];
    /* 0x7B8 */ s16 mTurnSpeed;
    /* 0x7BA */ s16 mLookAngle;
    /* 0x7BC */ s8 mAnmEnd;
    /* 0x7BD */ s8 mAnmLoops;
    /* 0x7BE */ u8 mItemNo;
    /* 0x7BF */ u8 mGaveLetter;
    /* 0x7C0 */ u8 mGaveHeart;
    /* 0x7C1 */ u8 mRefused;
    /* 0x7C2 */ u8 mCancelPresent;
    /* 0x7C3 */ u8 mSavedInitial;
    /* 0x7C4 */ int mUpdateEye;
    /* 0x7C8 */ int mActionResult;
    /* 0x7CC */ u8 mAttention;
    /* 0x7CD */ u8 mTalking;
    /* 0x7CE */ bool mNoTurn;
    /* 0x7CF */ u8 mDemo;
    /* 0x7D0 */ u8 m7D0;
    /* 0x7D1 */ s8 mCut;
    /* 0x7D2 */ s8 mEventAction;
    /* 0x7D3 */ u8 mAnmAttr;
    /* 0x7D4 */ u8 mAnmTag;
    /* 0x7D5 */ s8 mTexNo;
    /* 0x7D6 */ s8 mAnmNo;
    /* 0x7D7 */ s8 mOrder;
    /* 0x7D8 */ s8 mState;
    /* 0x7D9 */ s8 mPreviousState;
    /* 0x7DA */ s8 mLookMode;
    /* 0x7DB */ s8 mType;
    /* 0x7DC */ s8 mSubType;
    /* 0x7DD */ s8 mActionState;
    /* 0x7DE */ s8 mMsgAnmStarted;
};

STATIC_ASSERT(sizeof(daNpc_Kp1_c) == 0x7E0);

class daNpc_Kp1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackX, mMaxBackY, mMinBackX, mMinBackY;
        s16 mMaxTurn, mTurnSpeed;
        f32 mAttentionYOffset;
        u8 mDebug;
    };
    daNpc_Kp1_HIO_c();
    inline virtual ~daNpc_Kp1_HIO_c();
    s8 mNo;
    int mCount;
    hio_prm_c mPrm;
};

#endif /* D_A_NPC_KP1_H */
