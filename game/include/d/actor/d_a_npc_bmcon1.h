#ifndef D_A_NPC_BMCON1_H
#define D_A_NPC_BMCON1_H

#include "d/d_npc.h"
#include "d/d_cc_d.h"

class J3DNode;

struct sBmconAnmDat {
    u8 mAnm, mMorf;
    s8 mLoops;
};

class daNpcBmcon_c : public fopNpc_npc_c {
public:
    daNpcBmcon_c();

    enum Prm_e { PRM_RAIL_W = 8, PRM_RAIL_S = 16 };
    static const char m_arcname[];
    u8 getNpcNo() { return mNpcNo; }
    request_of_phase_process_class* getPhaseP() { return &mResPhase; }
    s8 get_arm_L_jnt_num() { return m_arm_L_jnt_num; }
    s8 get_arm_R_jnt_num() { return m_arm_R_jnt_num; }
    s8 get_nec_jnt_num() { return m_nec_jnt_num; }
    void setResFlag(u8 flag) { mResFlag = flag; }

    void nodeArmControl(J3DNode*, J3DModel*);
    cPhs_State _create();
    int createHeap();
    cPhs_State createInit();
    bool _delete();
    bool _draw();
    bool _execute();
    bool executeCommon();
    void executeSetMode(unsigned char);
    u8 executeWaitInit();
    void executeWait();
    u8 executeTalkInit();
    void executeTalk();
    u8 executeWalkInit();
    void executeWalk();
    u8 executeTurnInit();
    void executeTurn();
    void checkOrder();
    void eventOrder();
    void eventMove();
    void privateCut();
    void eventMesSetInit(int);
    bool eventMesSet();
    void eventGetItemInit();
    u16 talk2(int);
    virtual u16 next_msgStatus(u32*);
    virtual u32 getMsg();
    void chkMsg();
    void setMessage(unsigned long);
    void setAnmFromMsgTag();
    s8 getPrmNpcNo();
    u8 getPrmRailID();
    void setMtx();
    void chkAttention();
    void lookBack();
    void playAnm();
    void setAnm(unsigned char, int, float);
    bool setAnmTbl(sBmconAnmDat*);
    s16 XyCheckCB(int);
    void setCollision(dCcD_Cyl*, cXyz, float, float);
    cXyz calcFlyDist();
    s16 getFlyDistMax();
    void setFlyDistMax(short);
    s16 getFlyDistNow();
    void setFlyDistNow(short);
    int chkEndEvent();
    int isClear();

public:
    /* 0x6C4 */ Mtx mArmLMtx;
    /* 0x6F4 */ Mtx mArmRMtx;
    /* 0x724 */ request_of_phase_process_class mResPhase;
    /* 0x72C */ request_of_phase_process_class mPhase;
    /* 0x734 */ mDoExt_McaMorf* mpArmMorf;
    /* 0x738 */ J3DModel* mpHeadModel;
    /* 0x73C */ J3DAnmTexPattern* m_head_tex_pattern;
    /* 0x740 */ mDoExt_btpAnm mBtp;
    /* 0x754 */ u32 mShadow;
    /* 0x758 */ dNpc_PathRun_c mPath;
    /* 0x760 */ cXyz m760;
    /* 0x76C */ cXyz mLookPos;
    /* 0x778 */ sBmconAnmDat* mpAnm;
    /* 0x77C */ u32* mpMessage;
    /* 0x780 */ f32 mTargetSpeed;
    /* 0x784 */ f32 mMorfOverride;
    /* 0x788 */ f32 mAttentionDist;
    /* 0x78C */ int mItemNo;
    /* 0x790 */ bool mNoTurn;
    /* 0x792 */ s16 mEvent1;
    /* 0x794 */ s16 mEvent2;
    /* 0x796 */ s16 mWalkTimer;
    /* 0x798 */ s16 mLookTimer;
    /* 0x79A */ s16 mAttentionAngle;
    /* 0x79C */ s16 mDefaultAngle;
    /* 0x79E */ u16 mPreviousMsgStatus;
    /* 0x7A0 */ s16 m7A0;
    /* 0x7A2 */ s16 mFlyDist;
    /* 0x7A4 */ s16 m7A4;
    /* 0x7A6 */ s16 mMaxTurnSpeed;
    /* 0x7A8 */ s16 mTurnSpeed;
    /* 0x7AA */ s16 mLookAngle;
    /* 0x7AC */ bool mTalking;
    /* 0x7AD */ u8 mAttention;
    /* 0x7AE */ u8 mOrder;
    /* 0x7AF */ u8 m7AF;
    /* 0x7B0 */ s8 m_nec_jnt_num;
    /* 0x7B1 */ s8 m_arm_L_jnt_num;
    /* 0x7B2 */ s8 m_arm_R_jnt_num;
    /* 0x7B3 */ s8 m_armL1_jnt_num;
    /* 0x7B4 */ s8 m_armR1_jnt_num;
    /* 0x7B5 */ u8 mMode;
    /* 0x7B6 */ u8 mResFlag;
    /* 0x7B7 */ u8 mNpcNo;
    /* 0x7B8 */ u8 mRewardFlags;
    /* 0x7B9 */ u8 mAnmNo;
    /* 0x7BA */ u8 mAnmFlags;
    /* 0x7BB */ s8 mAnmLoops;
    /* 0x7BC */ s8 mCut;
    /* 0x7BD */ u8 m7BD;
    /* 0x7BE */ s8 mLookMode;
    /* 0x7BF */ u8 mCanTurn;
    /* 0x7C0 */ u8 mCanLook;
    /* 0x7C1 */ u8 mNearPlayer;
    /* 0x7C2 */ u8 mStartFlight;
    /* 0x7C3 */ u8 mChangingStage;
    /* 0x7C4 */ u8 mFlightResult;
    /* 0x7C5 */ u8 mSoundFlags;
};

STATIC_ASSERT(sizeof(daNpcBmcon_c) == 0x7C8);

#endif /* D_A_NPC_BMCON1_H */
