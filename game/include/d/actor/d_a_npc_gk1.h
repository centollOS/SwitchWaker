#ifndef D_A_NPC_GK1_H
#define D_A_NPC_GK1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Gk1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm, mTex;
        f32 mMorf, mSpeed;
        int mLoop;
    };

    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_Neck(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_GK1_0();
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
    virtual void anmAtr(unsigned short);
    virtual u16 next_msgStatus(unsigned long*);
    u32 getMsg_GK1_0();
    virtual u32 getMsg();
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
    bool set_action(int (daNpc_Gk1_c::*)(void*), void*);
    void setStt(signed char);
    bool chk_attn();
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
    BOOL itemCreateHeap();
    BOOL hat_CreateHeap();
    BOOL CreateHeap();

public:
    request_of_phase_process_class mPhase;
    s8 m_hed_jnt_num, m_bbone_jnt_num, m_nck_jnt_num;
    J3DModel* mpItemModel;
    J3DModel* mpHatModel;
    char mArcName[4];
    u32 mShadowId;
    mDoExt_btpAnm mBtp;
    u8 mBtpFrame;
    s16 mBlinkTimer;
    int (daNpc_Gk1_c::*mAction)(void*);
    u8 m704[4];
    fpc_ProcID mPartnerID;
    u8 m70C[8];
    cXyz mInitialPos;
    csXyz mInitialAngle, mModelAngle;
    u8 m72C[8];
    cXyz mEyeLocal, mLookPos;
    u8 m74C[12];
    cXyz mHeadPos;
    u8 m764[12];
    f32 mLastFrame;
    u8 m774[4];
    s16 mOldActorY, mOldHeadY, mOldBackY;
    u8 m77E[2];
    int mUpdateEye;
    s16 mEventIds[1], mEventIndex;
    u8 m788[8];
    s16 mWaitTimer;
    u8 m792[8];
    s16 mLookAngle;
    s8 mAnmEnded, mAnmRepeat;
    u8 mPresentItem;
    bool mLoaded;
    u8 mHidden, mFreezeAngle, mNoDraw, mInitialSet;
    bool mHasAttention, mTalking, mNoTurn, mReturnAngle;
    u8 mDemo;
    s8 mCutIndex;
    u8 mAnmAttr, mAnmTag;
    s8 mTexIndex, mAnmIndex, mEventOrder, mState, mPreviousState, mLookMode, mType;
    u8 mSubtype;
    s8 mActionState, mMessageState;
};

class daNpc_Gk1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackX, mMaxBackY, mMinBackX, mMinBackY;
        s16 mMaxTurn, mTurnSpeed;
        f32 mAttentionYOffset;
        u8 mDebug;
    };
    daNpc_Gk1_HIO_c();
    inline virtual ~daNpc_Gk1_HIO_c();

public:
    s8 mNo;
    int mCount;
    hio_prm_c mPrm;
};

STATIC_ASSERT(sizeof(daNpc_Gk1_c) == 0x7B8);

#endif /* D_A_NPC_GK1_H */
