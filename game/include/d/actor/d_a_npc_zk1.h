#ifndef D_A_NPC_ZK1_H
#define D_A_NPC_ZK1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Zk1_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Zk1_c::*ActionFunc)(void*);
    struct anm_prm_c {
        s8 mAnm;
        s8 mBtp;
        f32 mMorf;
        f32 mSpeed;
        int mLoop;
    };

    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_ZK1_0();
    bool createInit();
    void play_animation();
    void setMtx(bool);
    int bckResID(int);
    int btpResID(int);
    bool setBtp(signed char, bool);
    bool init_texPttrnAnm(signed char, bool);
    void play_btp_anm();
    void setAnm_anm(anm_prm_c*);
    void setAnm();
    void chngAnmTag();
    void ctrlAnmTag();
    void chngAnmAtr(unsigned char);
    void ctrlAnmAtr();
    void setAnm_ATR();
    void anmAtr(unsigned short);
    u16 next_msgStatus(unsigned long*);
    u32 getMsg_ZK1_0();
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
    bool set_action(int (daNpc_Zk1_c::*)(void*), void*);
    void setStt(signed char);
    BOOL wait_1();
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
    /* 0x704 */ u8 m704[8];
    /* 0x70C */ cXyz mInitialPos;
    /* 0x718 */ csXyz mInitialAngle;
    /* 0x71E */ csXyz mModelAngle;
    /* 0x724 */ u8 m724[8];
    /* 0x72C */ cXyz m72C;
    /* 0x738 */ cXyz mLookPos;
    /* 0x744 */ cXyz m744;
    /* 0x750 */ cXyz m750;
    /* 0x75C */ cXyz m75C;
    /* 0x768 */ f32 mFrame;
    /* 0x76C */ f32 m76C;
    /* 0x770 */ csXyz mPreviousAngles;
    /* 0x778 */ BOOL mUpdateEye;
    /* 0x77C */ s16 mEventIdx[1];
    /* 0x77E */ s16 mEventNo;
    /* 0x780 */ u8 m780[8];
    /* 0x788 */ s16 mLookTimer;
    /* 0x78A */ u8 m78A[8];
    /* 0x792 */ s16 mLookAngle;
    /* 0x794 */ s8 mAnmEnd;
    /* 0x795 */ s8 m795;
    /* 0x796 */ u8 mItemNo;
    /* 0x797 */ bool mHadEvent1802;
    /* 0x798 */ bool mHadEvent1C01;
    /* 0x799 */ u8 m799;
    /* 0x79A */ bool mEventActive;
    /* 0x79B */ bool mLoaded;
    /* 0x79C */ bool mHidden;
    /* 0x79D */ bool mKeepShapeAngle;
    /* 0x79E */ bool mNoDraw;
    /* 0x79F */ bool mSavedInitialPos;
    /* 0x7A0 */ bool mHasAttention;
    /* 0x7A1 */ bool mTalking;
    /* 0x7A2 */ bool mNoTurn;
    /* 0x7A3 */ bool mReturnAngle;
    /* 0x7A4 */ bool mDemo;
    /* 0x7A5 */ s8 mCut;
    /* 0x7A6 */ u8 mAnmAttr;
    /* 0x7A7 */ u8 mAnmTag;
    /* 0x7A8 */ s8 mBtpNo;
    /* 0x7A9 */ s8 mAnmNo;
    /* 0x7AA */ s8 mOrder;
    /* 0x7AB */ s8 mState;
    /* 0x7AC */ s8 mPreviousState;
    /* 0x7AD */ s8 mLookMode;
    /* 0x7AE */ s8 mType;
    /* 0x7AF */ u8 mSubType;
    /* 0x7B0 */ s8 mActionState;
    /* 0x7B1 */ s8 mMsgAnmStarted;

};

class daNpc_Zk1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackBoneX, mMaxBackBoneY, mMinBackBoneX, mMinBackBoneY;
        s16 mMaxTurnStep, mTurnSpeed;
        f32 mAttentionOffsetY;
        bool m18;
    };
    daNpc_Zk1_HIO_c();
    inline virtual ~daNpc_Zk1_HIO_c();
    void genMessage(JORMContext*) {}
    s8 m04;
    s32 m08;
    hio_prm_c mPrm;
};

#endif /* D_A_NPC_ZK1_H */
