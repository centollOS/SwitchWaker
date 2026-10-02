#ifndef D_A_NPC_PF1_H
#define D_A_NPC_PF1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Pf1_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Pf1_c::*ActionFunc)(void*);
    struct anm_prm_c {
        s8 mAnm;
        s8 mBtp;
        f32 mMorf;
        f32 mSpeed;
        int mLoop;
    };

    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_PF1_0();
    bool createInit();
    void play_animation();
    void setMtx(bool);
    int bckResID(int);
    int btpResID(int);
    bool setBtp(signed char, bool);
    bool init_texPttrnAnm(signed char, bool);
    void play_btp_anm();
    void setAnm_anm(anm_prm_c*);
    void setAnm_NUM(int, int);
    void setAnm();
    void chngAnmTag();
    void ctrlAnmTag();
    void chngAnmAtr(unsigned char);
    void ctrlAnmAtr();
    void setAnm_ATR();
    void anmAtr(unsigned short);
    u16 next_msgStatus(unsigned long*);
    u32 getMsg_PF1_0();
    u32 getMsg();
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    bool chk_parts_notMov();
    fopAc_ac_c* searchByID(fpc_ProcID, int*);
    void lookBack();
    bool chkAttention();
    void setAttention(bool);
    bool decideType(int);
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Pf1_c::*)(void*), void*);
    void setStt(signed char);
    BOOL wait_1();
    BOOL regret();
    BOOL attk_1();
    BOOL walk_1();
    BOOL wait_2();
    BOOL wait_3();
    void createTama(f32);
    bool chk_areaIN(f32, cXyz);
    bool endEvent_check();
    bool startEvent_check();
    void set_pthPoint(u8);
    bool chk_attn();
    void setBikon(cXyz);
    void delBikon();
    BOOL talk_1();
    int wait_action1(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL bodyCreateHeap();
    BOOL CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhs;
    /* 0x6CC */ s8 m_hed_jnt_num;
    /* 0x6CD */ s8 m_bbone_jnt_num;
    /* 0x6CE */ char mArcName[3];
    /* 0x6D4 */ u32 mShadowId;
    /* 0x6D8 */ mDoExt_btpAnm mBtpAnm;
    /* 0x6EC */ u8 mBtpFrame;
    /* 0x6EE */ s16 mBtpTimer;
    /* 0x6F0 */ ActionFunc mAction;
    /* 0x6FC */ fpc_ProcID m6FC;
    /* 0x700 */ fpc_ProcID mPartnerId;
    /* 0x704 */ dNpc_PathRun_c mPath;
    /* 0x70C */ cXyz mInitialPos;
    /* 0x718 */ csXyz mInitialAngle;
    /* 0x71E */ csXyz mModelAngle;
    /* 0x724 */ u8 m724[8];
    /* 0x72C */ cXyz m72C;
    /* 0x738 */ cXyz mLookPos;
    /* 0x744 */ cXyz m744;
    /* 0x750 */ cXyz m750;
    /* 0x75C */ cXyz m75C;
    /* 0x768 */ cXyz mGroundNormal;
    /* 0x774 */ f32 mFrame;
    /* 0x778 */ f32 m778;
    /* 0x77C */ csXyz mPreviousAngles;
    /* 0x784 */ BOOL mUpdateEye;
    /* 0x788 */ s16 mEventIdx[1];
    /* 0x78A */ s16 mEventNo;
    /* 0x78C */ u8 m78C[10];
    /* 0x796 */ s16 mAttackTimer;
    /* 0x798 */ s16 mStateTimer;
    /* 0x79A */ u8 m79A[4];
    /* 0x79E */ s16 mLookAngle;
    /* 0x7A0 */ s8 mAnmEnd;
    /* 0x7A1 */ s8 mAnmLoops;
    /* 0x7A2 */ u8 mItemNo;
    /* 0x7A3 */ u8 m7A3;
    /* 0x7A4 */ u8 m7A4;
    /* 0x7A5 */ u8 mMet;
    /* 0x7A6 */ u8 m7A6;
    /* 0x7A7 */ bool mLoaded;
    /* 0x7A8 */ bool mHidden;
    /* 0x7A9 */ u8 m7A9;
    /* 0x7AA */ bool mKeepShapeAngle;
    /* 0x7AB */ bool mNoDraw;
    /* 0x7AC */ bool mSavedInitialPos;
    /* 0x7AD */ bool mHasAttention;
    /* 0x7AE */ bool mTalking;
    /* 0x7AF */ bool mNoTurn;
    /* 0x7B0 */ bool mReturnAngle;
    /* 0x7B1 */ bool mDemo;
#if VERSION == VERSION_DEMO
    /* 0x7B4 */ JPABaseEmitter* mpBikonEmitter;
#endif
    /* 0x7B2 */ s8 mCut;
    /* 0x7B3 */ u8 mAnmAttr;
    /* 0x7B4 */ u8 mAnmTag;
    /* 0x7B5 */ s8 mBtpNo;
    /* 0x7B6 */ s8 mAnmNo;
    /* 0x7B7 */ s8 mOrder;
    /* 0x7B8 */ s8 mState;
    /* 0x7B9 */ s8 mPreviousState;
    /* 0x7BA */ s8 mLookMode;
    /* 0x7BB */ s8 mType;
    /* 0x7BC */ u8 mSubType;
    /* 0x7BD */ s8 mActionState;
    /* 0x7BE */ s8 mMsgAnmStarted;

};

class daNpc_Pf1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackBoneX, mMaxBackBoneY, mMinBackBoneX, mMinBackBoneY;
        s16 mMaxTurnStep, mTurnSpeed;
        f32 mAttentionOffsetY;
        u8 m18;
        u8 m19;
        s16 mMoveTurnRate;
        s16 mMoveTurnSpeed;
        f32 mWalkAnmSpeed;
        f32 mWalkSpeed;
        f32 mWalkAccel;
        f32 mAttackAnmSpeed;
        f32 mAttackSpeed;
        f32 mAttackAccel;
        f32 mStartRadius;
        f32 mEndRadius;
    };
    daNpc_Pf1_HIO_c();
    inline virtual ~daNpc_Pf1_HIO_c();
    void genMessage(JORMContext*) {}
    s8 m04;
    s32 m08;
    hio_prm_c mPrm;
};

#endif /* D_A_NPC_PF1_H */
