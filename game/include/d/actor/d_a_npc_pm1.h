#ifndef D_A_NPC_PM1_H
#define D_A_NPC_PM1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class daNpc_Pm1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm;
        s8 mTex;
        f32 mMorf;
        f32 mSpeed;
        int mLoop;
    };

    cXyz* getAttPos() { return &mAttPos; }
    s8 getBackboneJntNum() { return m_backbone_jnt_num; }
    s16 getBackbone_x() { return m_jnt.getBackbone_x(); }
    s16 getBackbone_y() { return m_jnt.getBackbone_y(); }
    cXyz* getEyePos() { return &mEyePos; }
    s8 getHeadJntNum() { return m_head_jnt_num; }
    s16 getHead_x() { return m_jnt.getHead_x(); }
    s16 getHead_y() { return m_jnt.getHead_y(); }

    bool createInit();
    void setMtx();
    BOOL anmResID(int, int*, int*);
    void BtpNum2ResID(int, int*);
    void setAnm_tex(signed char);
    bool init_btp(bool, int);
    bool initTexPatternAnm(bool);
    void playTexPatternAnm();
    BOOL setAnm_anm(anm_prm_c*);
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
    bool decideType(int);
    void event_actionInit(int);
    bool event_action();
    void privateCut();
    void endEvent();
    void event_proc();
    bool set_action(int (daNpc_Pm1_c::*)(void*), void*);
    BOOL wait01();
    BOOL talk01();
    int wait_action1(void*);
    u8 demo();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhase;
    /* 0x6CC */ s8 m_head_jnt_num;
    /* 0x6CD */ s8 m_backbone_jnt_num;
    /* 0x6D0 */ u32 mShadowId;
    /* 0x6D4 */ J3DModel* mpShadowModel;
    /* 0x6D8 */ J3DAnmTexPattern* m_head_tex_pattern;
    /* 0x6DC */ mDoExt_btpAnm mBtp;
    /* 0x6F0 */ u8 mBtpFrame;
    /* 0x6F2 */ s16 mBlinkTimer;
    /* 0x6F4 */ int (daNpc_Pm1_c::*mAction)(void*);
    /* 0x700 */ dNpc_EventCut_c mCut;
    /* 0x76C */ csXyz mInitialAngle;
    /* 0x774 */ cXyz mInitialPos;
    /* 0x780 */ cXyz mAttPos;
    /* 0x78C */ cXyz mEyePos;
    /* 0x798 */ cXyz mLookPos;
    /* 0x7A4 */ f32 mFrame;
    /* 0x7A8 */ u8 m7A8[8];
    /* 0x7B0 */ s16 mTurnSpeed;
    /* 0x7B2 */ s16 mLookAngle;
    /* 0x7B4 */ u8 mAnmEnded;
    /* 0x7B5 */ u8 m7B5;
    /* 0x7B6 */ u8 m7B6;
    /* 0x7B7 */ u8 mPresentItem;
    /* 0x7B8 */ bool mInitialPosSet;
    /* 0x7B9 */ u8 m7B9[3];
    /* 0x7BC */ int mFirstFrame;
    /* 0x7C0 */ int mUpdateEye;
    /* 0x7C4 */ bool mHasAttention;
    /* 0x7C5 */ bool mTalking;
    /* 0x7C6 */ bool mLookFlag;
    /* 0x7C7 */ u8 mDemo;
    /* 0x7C8 */ u8 m7C8;
    /* 0x7C9 */ u8 mCutIndex;
    /* 0x7CA */ u8 mEventAction;
    /* 0x7CB */ u8 mAnmAttr;
    /* 0x7CC */ u8 mAnmTag;
    /* 0x7CD */ s8 mTexIndex;
    /* 0x7CE */ s8 mAnmIndex;
    /* 0x7CF */ s8 mEventOrder;
    /* 0x7D0 */ s8 mState;
    /* 0x7D1 */ s8 mPreviousState;
    /* 0x7D2 */ s8 mLookMode;
    /* 0x7D3 */ s8 mType;
    /* 0x7D4 */ u8 m7D4;
    /* 0x7D5 */ s8 mActionState;
    /* 0x7D6 */ s8 mMessageState;
};

STATIC_ASSERT(sizeof(daNpc_Pm1_c) == 0x7D8);

#endif /* D_A_NPC_PM1_H */
