#ifndef D_A_BIGELF_H
#define D_A_BIGELF_H

#include "f_op/f_op_actor.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"
#include "d/d_npc.h"

class JPABaseEmitter;

class daBigelf_c : public fopAc_ac_c {
public:
    typedef BOOL (daBigelf_c::*ActionFunc)(void*);

    enum ActionStatus {
        ACTION_STARTING = 0,
        ACTION_ONGOING = 1,
        ACTION_ENDING = -1,
    };

    enum TalkState {
        TALK_FINISHED = -1,
        TALK_INIT = 0,
        TALK_MSG_CREATE = 1,
        TALK_ACTIVE = 2,
    };

    bool chkFlag(unsigned short flg) { return (mFlags & flg) == flg; }
    void clrFlag(unsigned short flg) { mFlags &= ~flg; }
    s8 getBackboneJntNum() { return m_jnt.getBackboneJntNum(); }
    s8 getHeadJntNum() { return m_jnt.getHeadJntNum(); }
    s16 getHead_x() { return m_jnt.getHead_x(); }
    void incAttnSetCount() {
        if (mAttnSetCount != 0xFF) {
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
    void setFlag(unsigned short flg) { mFlags |= flg; }

    void oct_delete();
    BOOL nodeCallBack(J3DNode*);
    void lightInit(cXyz*);
    void lightEnd();
    void lightProc();
    void darkInit();
    void darkEnd();
    void darkProc();
    void demoInitFlDelete();
    BOOL demoProcFlDelete();
    void demoInitFlLink();
    BOOL demoProcFlLink();
    void demoInitFlDmAf();
    BOOL demoProcFlDmAf();
    void demoInitFlDmMd();
    BOOL demoProcFlDmMd();
    void demoInitFlDmBf();
    BOOL demoProcFlDmBf();
    void demoInitFlDemo();
    BOOL demoProcFlDemo();
    void demoInitExit();
    BOOL demoProcExit();
    void demoInitTalk();
    BOOL demoProcTalk();
    void demoInitAppear();
    BOOL demoProcAppear();
    void demoInitFa1();
    BOOL demoProcFa1();
    void demoInitWait();
    BOOL demoProcWait();
    void demoInitCom();
    BOOL demoProcCom();
    s32 getNowEventAction();
    BOOL demoProc();
    u8 getType();
    u8 getSwbit();
    u8 getSwbit2();
    u16 getEventFlag();
    void makeFa1S();
    void makeFa1();
    void setAnm(signed char);
    void setAnmStatus();
    u16 next_msgStatus(unsigned long*);
    u32 getMsg();
    void msgPushButton();
    void msgAnm(unsigned char);
    void talkInit();
    u16 talk();
    BOOL init();
    void setAttention(bool);
    void lookBack();
    bool hunt();
    bool oct_search();
    bool oct();
    bool ready0();
    bool event0();
    bool dead();
    BOOL wait_action(void*);
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL CreateHeap();

public:
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ mDoExt_McaMorf* mpMorf;
    /* 0x29C */ mDoExt_brkAnm mBrk;
    /* 0x2B4 */ mDoExt_btkAnm mBtk;
    /* 0x2C8 */ J3DModel* mpFlowerModel;
    /* 0x2CC */ mDoExt_brkAnm mFlowerBrk;
    /* 0x2E4 */ s8 m_fl_jnt;
    /* 0x2E5 */ u8 m2E5[0x2E8 - 0x2E5];
    /* 0x2E8 */ dNpc_JntCtrl_c m_jnt;
    /* 0x31C */ cXyz mEyePos;
    /* 0x328 */ cXyz mAttnBasePos;
    /* 0x334 */ u8 m334[0x336 - 0x334];
    /* 0x336 */ s8 mAnmEnd;
    /* 0x337 */ u8 mAttnSetCount;
    /* 0x338 */ f32 mOldAnmFrame;
    /* 0x33C */ u32 mCurrMsgNo;
    /* 0x340 */ u16 mFlags;
    /* 0x342 */ u8 m342[0x344 - 0x342];
    /* 0x344 */ u8 m344;
    /* 0x345 */ u8 m345;
    /* 0x346 */ u8 mGivenItem;
    /* 0x347 */ u8 m347;
    /* 0x348 */ u32 mCurrentMessageId;
    /* 0x34C */ fpc_ProcID mFa1Id;
    /* 0x350 */ s16 mHeadZAngle;
    /* 0x352 */ bool mIsLightShining;
    /* 0x353 */ u8 m353[0x354 - 0x353];
    /* 0x354 */ LIGHT_INFLUENCE mLight;
    /* 0x374 */ cXyz mLightPos2;
    /* 0x380 */ GXColorS10 mLightColor2;
    /* 0x388 */ f32 mLightPower2;
    /* 0x38C */ f32 mLightFluctuation2;
    /* 0x390 */ u8 m390[0x39C - 0x390];
    /* 0x39C */ u8 mIsDark;
    /* 0x39D */ u8 m39D[0x3A0 - 0x39D];
    /* 0x3A0 */ f32 mDarkRatio;
    /* 0x3A4 */ f32 mDarkTarget;
    /* 0x3A8 */ f32 m3A8;
    /* 0x3AC */ s16 mOctTimer;
    /* 0x3AE */ u8 m3AE[0x3B0 - 0x3AE];
    /* 0x3B0 */ ActionFunc mCurrActionFunc;
    /* 0x3BC */ s8 mAnm;
    /* 0x3BD */ s8 mMode;
    /* 0x3BE */ u8 m3BE[0x3C0 - 0x3BE];
    /* 0x3C0 */ s16 mTimer;
    /* 0x3C2 */ u8 m3C2[0x3C4 - 0x3C2];
    /* 0x3C4 */ int mStaffId;
    /* 0x3C8 */ s16 m3C8;
    /* 0x3CA */ s16 mAttnTimer;
    /* 0x3CC */ JPABaseEmitter* mpEmitter3CC;
    /* 0x3D0 */ cXyz mHandPos;
    /* 0x3DC */ u8 mAlpha;
    /* 0x3DD */ u8 m3DD[0x3E0 - 0x3DD];
    /* 0x3E0 */ JPABaseEmitter* mpEmitter3E0;
    /* 0x3E4 */ JPABaseEmitter* mpEmitter3E4;
    /* 0x3E8 */ f32 mHeightOffset;
    /* 0x3EC */ f32 m3EC;
    /* 0x3F0 */ f32 m3F0;
    /* 0x3F4 */ u8 mNpcType;
    /* 0x3F5 */ s8 mPaNo;
    /* 0x3F6 */ s8 mActionStatus;
    /* 0x3F7 */ s8 mTalkState;
    /* 0x3F8 */ fpc_ProcID mOctId;
};

STATIC_ASSERT(sizeof(daBigelf_c) == 0x3FC);

#endif /* D_A_BIGELF_H */
