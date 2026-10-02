#ifndef D_A_NPC_GP1_H
#define D_A_NPC_GP1_H

#include "d/d_npc.h"

class J3DNode;

class daNpc_Gp1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm, mTex;
        f32 mMorf, mSpeed;
        int mLoop;
    };

    void nodeGp1Control(J3DNode*, J3DModel*);
    bool init_GP1_0();
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
    bool chk_drct(f32);
    bool chk_partsNotMove();
    bool chk_forceTlkArea();
    void lookBack();
    virtual u16 next_msgStatus(unsigned long*);
    u32 getMsg_GP1_0();
    virtual u32 getMsg();
    bool chkAttention();
    void setAttention(bool);
    fopAc_ac_c* searchByID(fpc_ProcID);
    bool partner_srch_sub(void* (*)(void*, void*));
    void partner_srch();
    void ctrl_WAITanm();
    int gp_movPass();
    void gp_clcMovSpd();
    void gp_nMove();
    BOOL create_rupee();
    bool charDecide(int);
    void eInit_INI_KAERE_KAERE_();
    void eInit_END_KAERE_KAERE_();
    void event_actionInit(int);
    bool event_action();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Gp1_c::*)(void*), void*);
    void setStt(signed char);
    BOOL wait_1();
    BOOL talk_1();
    BOOL walk_1();
    BOOL hair_1();
    BOOL wait_2();
    int wait_action1(void*);
    u8 demo();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    J3DModelData* create_Anm();
    BOOL CreateHeap();

public:
    request_of_phase_process_class mPhase;
    s8 m_hed_jnt_num, m_bbone_jnt_num, m_hnd_L_jnt_num;
    u32 mShadowId;
    J3DModel* mpShadowModel;
    J3DAnmTexPattern* m_hed_tex_pttrn;
    mDoExt_btpAnm mBtp;
    u8 mBtpFrame;
    s16 mBlinkTimer;
    int (daNpc_Gp1_c::*mAction)(void*);
    dNpc_PathRun_c mPath;
    dNpc_EventCut_c mCut;
    fpc_ProcID mPartnerID;
    cXyz mInitialPos;
    csXyz mInitialAngle, mModelAngle;
    cXyz mEyeLocal, mLookPos, mMoveTarget;
    u8 m7B4[12];
    f32 mLastFrame, mEyeYOffset;
    u8 m7C8[4];
    f32 mTargetSpeed, mSpeedStep, mArriveDistance;
    s16 mOldHeadY, mOldBackY, mOldActorY;
    s16 mEventIds[3], mEventIndex;
    u8 m7E6[2];
    s16 mAttentionTimer, mWaitTimer, mHairTimer, mTurnSpeed, mLookAngle;
    s16 mNecklaceCount, mRupeeCount, mIdleAnimTimer;
    s8 mAnmEnded, mAnmRepeat, mAfterItem;
    u8 mPresentItem, mHidden, mFreezeAngle, mStopMove, mNoDraw, mArrived, mConsumeNecklaces, mInitialSet, m803;
    int mUpdateEye;
    bool mHasAttention, mTalking, mNoTurn;
    u8 mDemo;
    s8 mMoveMode, mCutIndex, mEventAction;
    u8 mAnmAttr, mAnmTag;
    s8 mTexIndex, mAnmIndex, mEventOrder, mState, mPreviousState, mLookMode, mType, mSubtype, mActionState, mMessageState;
};

STATIC_ASSERT(sizeof(daNpc_Gp1_c) == 0x81C);

#endif /* D_A_NPC_GP1_H */
