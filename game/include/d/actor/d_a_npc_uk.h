#ifndef D_A_NPC_UK_H
#define D_A_NPC_UK_H

#include "f_op/f_op_actor.h"
#include "d/d_npc.h"
#include "d/actor/d_a_npc_mk_static.h"

class daNpc_Uk_c : public fopAc_ac_c {
public:
    enum Types { TYPE_NONE, TYPE_NORMAL, TYPE_MINIGAME };
    enum TalkStates { TALK_FINISHED = -1, TALK_INIT, TALK_MSG_CREATE, TALK_ACTIVE };
    enum ActionStatus { ACTION_ENDING = -1, ACTION_STARTING, ACTION_ONGOING };

    typedef BOOL (daNpc_Uk_c::*ActionFunc)(void*);

    BOOL ChkOrder(u8 flag) { return mOrderFlags & flag; }
    void ClrOrder(u8 flag) { mOrderFlags &= ~flag; }
    void SetOrder(u8 flag) { mOrderFlags |= flag; }

    bool chkFlag(u16 flag) { return (mFlags & flag) == flag; }
    void clrFlag(u16 flag) { mFlags &= ~flag; }
    s8 getBackboneJntNum() { return m_jnt.getBackboneJntNum(); }
    s8 getHeadJntNum() { return m_jnt.getHeadJntNum(); }
    s16 getBackbone_x() { return m_jnt.getBackbone_x(); }
    s16 getBackbone_y() { return m_jnt.getBackbone_y(); }
    s16 getHead_x() { return m_jnt.getHead_x(); }
    s16 getHead_y() { return m_jnt.getHead_y(); }

    void incAttnSetCount() {
        if (mAttnSetCount != 0xff) {
            mAttnSetCount++;
        }
    }

    BOOL setAction(ActionFunc actionFunc, void* arg) {
        if (mCurrActionFunc != actionFunc) {
            if (mCurrActionFunc != NULL) {
                mActionStatus = ACTION_ENDING;
                (this->*mCurrActionFunc)(arg);
            }
            mCurrActionFunc = actionFunc;
            mActionStatus = ACTION_STARTING;
            (this->*mCurrActionFunc)(arg);
        }
        return TRUE;
    }

    void setAttentionBasePos(cXyz i_attnBasePos) { mAttnBasePos = i_attnBasePos; }
    void setEyePos(cXyz i_eyePos) { mEyePos = i_eyePos; }
    void setFlag(u16 flag) { mFlags |= flag; }

    int getType();
    int getPath();
    int getShapeType();
    u16 getCaughtFlag();
    u16 getFoundFlag();
    u16 getFirstTalk();
    BOOL chkGameStart();
    BOOL chkPositioning(f32 distance, f32 minY, f32 maxY, s16 minAngle, s16 maxAngle);
    u8 nextVisitMode();
    void approachRun(fopAc_ac_c* actor);
    void aroundWalk(fopAc_ac_c* actor);
    void surrender();
    void runawayInit();
    BOOL initTexPatternAnm(bool i_modify);
    void playTexPatternAnm();
    void setAnm(s8 index, u8 flags);
    bool chkAttentionLocal();
    void chkAttention();
    void setAnmStatus();
    void eventOrder();
    void checkOrder();
    u16 next_msgStatus(u32* msg);
    u32 getMsg();
    void setCollision();
    void msgAnm(u8 index);
    void talkInit();
    u16 talk();
    BOOL init();
    void setAttention(bool force);
    u8 getLookBackMode();
    void lookBack();
    char* getStaffName();
    BOOL checkDemoStart();
    s32 getNowEventAction();
    void visitInit(u8 mode);
    void visitProc();
    bool wait01();
    bool talk01();
    bool talk02();
    u8 visitTalkInit();
    void visitSetEvent();
    bool visit01();
    bool warningB();
    bool warningC();
    bool warningD();
    bool found();
    bool runaway();
    bool jump();
    bool jitanda01();
    bool jitanda02();
    bool demo01();
    bool demo02();
    void demoInitCom();
    bool demoProc();
    BOOL wait_action(void*);
    BOOL seek_action(void*);
    BOOL hind_action(void*);
    BOOL visit_action(void*);
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL CreateHeap();

public:
    /* 0x290 */ request_of_phase_process_class mPhs;
    /* 0x298 */ mDoExt_McaMorf* mpMorf;
    /* 0x29C */ J3DModel* mpHeadModel;
    /* 0x2A0 */ u32 mShadowId;
    /* 0x2A4 */ J3DAnmTexPattern* m_maba_tex_pattern;
    /* 0x2A8 */ mDoExt_btpAnm mBtpAnm;
    /* 0x2BC */ u8 mBlinkFrame;
    /* 0x2BE */ s16 mBlinkTimer;
    /* 0x2C0 */ dBgS_ObjAcch mObjAcch;
    /* 0x484 */ dBgS_AcchCir mAcchCir;
    /* 0x4C4 */ dCcD_Stts mStts;
    /* 0x500 */ dCcD_Cyl mCyl;
    /* 0x630 */ dNpc_JntCtrl_c m_jnt;
    /* 0x664 */ cXyz mEyePos;
    /* 0x670 */ cXyz mAttnBasePos;
    /* 0x67C */ s16 mMaxHeadTurnVelocity;
    /* 0x67E */ s8 mAnmEnded;
    /* 0x67F */ u8 mAttnSetCount;
    /* 0x680 */ f32 mAnmTimer;
    /* 0x684 */ u32 mCurrMsgNo;
    /* 0x688 */ u16 mFlags;
    /* 0x68A */ u8 mAttentionTimer;
    /* 0x68C */ fpc_ProcID mLeaderID;
    /* 0x690 */ f32 mWalkSpeed;
    /* 0x694 */ f32 mRunSpeed;
    /* 0x698 */ dNpc_PathRun_c mPath;
    /* 0x6A0 */ ActionFunc mCurrActionFunc;
    /* 0x6AC */ s8 mTexPatternIdx;
    /* 0x6AD */ s8 mAnmIdx;
    /* 0x6B0 */ int mStaffIdx;
    /* 0x6B4 */ s32 mEventAction;
    /* 0x6B8 */ u8 mOrderFlags;
    /* 0x6B9 */ s8 mState;
    /* 0x6BA */ s8 mPrevState;
    /* 0x6BB */ u8 mVisitMode;
    /* 0x6BC */ u8 mWaitTimer;
    /* 0x6BD */ u8 mAroundTimer;
    /* 0x6BE */ u8 mApproachTimer;
    /* 0x6BF */ s8 mType;
    /* 0x6C0 */ s8 mActionStatus;
    /* 0x6C1 */ s8 mTalkState;
    /* 0x6C2 */ u8 mMsgAnmIdx;
    /* 0x6C4 */ u32 mMtrlSndId;
    /* 0x6C8 */ s8 mReverb;
    /* 0x6CC */ cXyz mStartPos;
    /* 0x6D8 */ cXyz mTargetPos;
    /* 0x6E4 */ s16 mMoveTimer;
    /* 0x6E6 */ s16 mEventIdx;
    /* 0x6E8 */ s16 mTargetAngle;
    /* 0x6EC */ u32 mMsgNo;
    /* 0x6F0 */ daNpc_Mk_Static_c mMkStatic;
}; // Size: 0x700

#endif /* D_A_NPC_UK_H */
