#ifndef D_A_NPC_KF1_H
#define D_A_NPC_KF1_H

#include "m_Do/m_Do_hostIO.h"
#include "d/d_npc.h"

class daNpc_Kf1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c { s8 mAnm, mTex; f32 mMorf, mSpeed; int mLoop; };

    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_Neck(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_KF1_0();
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
    virtual void anmAtr(unsigned short);
    virtual u16 next_msgStatus(unsigned long*);
    u32 getMsg_KF1_0();
    virtual u32 getMsg();
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    fopAc_ac_c* searchByID(fpc_ProcID, int*);
    BOOL srch_Tsubo();
    void create_rupee(cXyz, int);
    void ready_kutaniCamera(int, int);
    void lookBack();
    bool chkAttention();
    void setAttention(bool);
    bool decideType(int);
    void cut_init_ANGRY_START(int);
    bool cut_move_ANGRY_START();
    void cut_init_BENSYOU_START(int);
    bool cut_move_BENSYOU_START();
    void cut_init_TSUBO_CNT(int);
    bool cut_move_TSUBO_CNT();
    void cut_init_BENSYOU(int);
    bool cut_move_BENSYOU();
    void cut_init_GET_OUT(int);
    bool cut_move_GET_OUT();
    void cut_init_DSP_RUPEE_CNT(int);
    bool cut_move_DSP_RUPEE_CNT();
    void cut_init_PLYER_TRN(int);
    bool cut_move_PLYER_TRN();
    void cut_init_RUPEE_CNT_END(int);
    bool cut_move_RUPEE_CNT_END();
    void cut_init_START_AGE(int);
    bool cut_move_START_AGE();
    void cut_init_PLYER_MOV(int);
    bool cut_move_PLYER_MOV();
    void cut_init_RUPEE_SET(int);
    bool cut_move_RUPEE_SET();
    void cut_init_TSUBO_ATN(int);
    bool cut_move_TSUBO_ATN();
    void cut_init_TLK_MSG(int);
    void cut_init_CONTNUE_TLK(int);
    bool cut_move_TLK_MSG();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Kf1_c::*)(void*), void*);
    void setStt(signed char);
    void set_pthPoint(unsigned char);
    s16 chk_tsubo();
    BOOL orderTsuboEvent();
    BOOL wait_1();
    BOOL walk_1();
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
    BOOL CreateHeap();

public:
    request_of_phase_process_class mPhase;
    s8 m_hed_jnt_num, m_bbone_jnt_num, m_nck_jnt_num;
    J3DModel* mpItemModel;
    char mArcName[4];
    u32 mShadowId;
    mDoExt_btpAnm mBtp;
    u8 mBtpFrame;
    s16 mBlinkTimer;
    int (daNpc_Kf1_c::*mAction)(void*);
    fpc_ProcID mPartnerID, mAttentionActorID;
    dNpc_PathRun_c mPath;
    cXyz mInitialPos;
    csXyz mInitialAngle, mModelAngle;
    u8 m728[8];
    cXyz mEyeLocal, mLookPos;
    u8 m748[12];
    cXyz mHeadPos;
    u8 m760[12];
    f32 mLastFrame;
    u8 m770[4];
    s16 mOldActorY, mOldHeadY, mOldBackY;
    u8 m77A[2];
    int mUpdateEye;
    s16 mEventIds[3], mEventIndex;
    u8 m788[4];
    s16 mCutTimer, mPotCameraIndex, mWaitTimer, mLookTimer, mMoveTimer;
    u8 m796[4];
    s16 mLookAngle;
    s8 mAnmEnded, mAnmRepeat;
    u8 mPresentItem, mSwitch, mOfferAccepted, mGaveRupees, m7A2;
    u8 mWasGrounded, mWasWaterIn, mEventActive;
    bool mLoaded;
    u8 mHidden, mFreezeAngle, mNoDraw, mInitialSet;
    bool mHasAttention, mTalking, mNoTurn;
    u8 mDemo, m7AF;
    fpc_ProcID mRupeeIDs[3], mPotIDs[8], mAttentionPotIDs[3], mDummyRupeeID;
    s16 mPotCount, mBrokenPots;
    u16 mSavedRupees;
    s8 mCutIndex;
    u8 mAnmAttr, mAnmTag;
    s8 mTexIndex, mAnmIndex, mEventOrder, mState, mPreviousState, mLookMode, mType;
    u8 mSubtype;
    s8 mActionState, mMessageState;

};

class daNpc_Kf1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackX, mMaxBackY, mMinBackX, mMinBackY;
        s16 mMaxTurn, mTurnSpeed;
        f32 mAttentionYOffset;
        u8 mDebug;
        s16 mTurnRate, mWalkTurnSpeed;
        f32 mWalkAnmSpeed, mWalkSpeed, mAcceleration, mMorf;
    };
    daNpc_Kf1_HIO_c();
    inline virtual ~daNpc_Kf1_HIO_c();
    s8 mNo;
    int mCount;
    hio_prm_c mPrm;
};
STATIC_ASSERT(sizeof(daNpc_Kf1_c) == 0x800);
#endif /* D_A_NPC_KF1_H */
