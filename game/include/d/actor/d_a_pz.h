#ifndef D_A_PZ_H
#define D_A_PZ_H

#include "res/Object/Pz.h"
#include "f_op/f_op_actor.h"
#include "d/d_npc.h"
#include "c/c_damagereaction.h"
#include "SSystem/SComponent/c_phase.h"
#include "m_Do/m_Do_hostIO.h"
#include "d/d_particle.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_lib.h"
#include "JSystem/J3DGraphAnimator/J3DMaterialAnm.h"

class J3DNode;
class J3DModel;
class J3DMaterial;
class J3DJoint;
class J3DShape;

class daPz_matAnm_c : public J3DMaterialAnm {
public:
    void clrMoveFlag() { mMoveFlag = false; }
    f32* getNowOffsetX() { return &mOffset.x; }
    f32* getNowOffsetY() { return &mOffset.y; }
    void setMoveFlag() { mMoveFlag = true; }

    daPz_matAnm_c();
    virtual ~daPz_matAnm_c() {}
    virtual void calc(J3DMaterial*) const;

public:
    /* 0x6C */ cXy mOffset;
    /* 0x74 */ cXy m74;
    /* 0x7C */ bool mMoveFlag;
};

class daPz_c : public fopNpc_npc_c {
public:
    enum Proc_e {
        PROC_INIT_e = 0,
        PROC_EXEC_e = 1,
    };

    enum Mode {
        MODE_WAIT = 0,
        MODE_MOVE = 1,
        MODE_ATTACKWAIT = 2,
        MODE_ATTACK = 3,
        MODE_DEFEND = 4,
        MODE_DOWN = 5,
        MODE_AFRAID = 6,
        MODE_SIDE_STEP = 7,
        MODE_BACK_STEP = 8,
        MODE_TALK = 9,
        MODE_FOLLOW = 10,
    };

    typedef void (daPz_c::*ModeFunc)();

    MtxP getRightHandMatrix() { return mpMorf->getModel()->getAnmMtx(PZ_JNT_HAND_R1_e); }
    bool isAnm(s8 idx) { return mAnmPrmIdx == idx; }
    bool isEyeAnm(s8 idx) { return mCurEye == idx; }
    void modeProcInit(int newMode) { modeProc(PROC_INIT_e, newMode); }
    void setAfraid() { modeProcInit(MODE_AFRAID); }
    void setDown() { modeProcInit(MODE_DOWN); }
    void setMove() { modeProcInit(MODE_MOVE); }
    void setStand() { modeProcInit(MODE_WAIT); }

    void _nodeHeadControl(J3DNode*, J3DModel*);
    void _nodeWaistControl(J3DNode*, J3DModel*);
    void _nodeWaist2Control(J3DNode*, J3DModel*);
    void _nodeSkirtControl(J3DNode*, J3DModel*);
    BOOL bodyCreateHeap();
    BOOL bowCreateHeap();
    BOOL _createHeap();
    void getGndPos();
    bool checkEyeArea(cXyz&);
    virtual u32 getMsg();
    virtual u16 next_msgStatus(u32*);
    virtual void anmAtr(u16);
    void eventOrder();
    void checkOrder();
    void setFallSplash();
    void setHeadSplash();
    void setRipple();
    void setJntStatus();
    bool demo();
    bool checkTgHit();
    void getArg();
    void setAttention();
    void setBowAnm(s8, bool);
    void setBowString(bool);
    void setAnm(s8, bool, int);
    void setAnmRunSpeed();
    void setEyeBtp(int);
    void setEyeBtk(int);
    void setEyeAnm(s8);
    void ctrlEye();
    void playEyeAnm();
    void setMtx();
    void modeWaitInit();
    void modeWait();
    void modeMoveInit();
    void modeMove();
    void modeAttackWaitInit();
    void modeAttackWait();
    void modeAttackInit();
    void modeAttack();
    void modeDefendInit();
    void modeDefend();
    void modeDownInit();
    void modeDown();
    void modeAfraidInit();
    void modeAfraid();
    void modeSideStepInit();
    void modeSideStep();
    void modeBackStepInit();
    void modeBackStep();
    void modeTalkInit();
    void modeTalk();
    void modeFollowInit();
    void modeFollow();
    void modeProc(Proc_e, int);
    bool _execute();
    void debugDraw();
    void bowDraw();
    void bodyDraw();
    void drawShadow();
    bool _draw();
    void bodyCreateInit();
    void createInit();
    cPhs_State _create();
    bool _delete();

    static const u32 m_heapsize;
    static const char m_arc_name[];
    static const f32 m_smoke_ef;
    static const f32 m_grass_ef;
    static const f32 m_splash_ef;
    static const dCcD_SrcCyl m_cyl_src;

public:
    /* 0x06C4 */ int mMode;
    /* 0x06C8 */ int mPrevMode;
    /* 0x06CC */ int mAnmAtrFlag;
    /* 0x06D0 */ u8 mInvulnTimer;
    /* 0x06D1 */ u8 mHitType;
    /* 0x06D2 */ s8 mBckIdx;
    /* 0x06D3 */ s8 mAnmPrmIdx;
    /* 0x06D4 */ s8 mOldAnmPrmIdx;
    /* 0x06D5 */ s8 mCurEye;
    /* 0x06D6 */ u8 m06D6[0x06D8 - 0x06D6];
    /* 0x06D8 */ int mEyeTimer;
    /* 0x06DC */ u8 m06DC;
    /* 0x06DD */ s8 mBowBckIdx;
    /* 0x06DE */ s8 mBowAnmPrmIdx;
    /* 0x06DF */ s8 mBowOldAnmPrmIdx;
    /* 0x06E0 */ dPa_rippleEcallBack mRippleCb;
    /* 0x06F4 */ dPa_followEcallBack mFollowCb1;
    /* 0x0708 */ dPa_followEcallBack mFollowCb2;
    /* 0x071C */ cXyz mSplashPos;
    /* 0x0728 */ int mSplashTimer;
    /* 0x072C */ cXyz mHeadPos;
    /* 0x0738 */ u8 m0738[0x073E - 0x0738];
    /* 0x073E */ u8 mbEyesFollowGanondorf;
    /* 0x073F */ u8 m073F;
    /* 0x0740 */ u8 m0740;
    /* 0x0741 */ u8 m0741[0x0744 - 0x0741];
    /* 0x0744 */ int m0744;
    /* 0x0748 */ cXyz mHitPos;
    /* 0x0754 */ int m0754;
    /* 0x0758 */ int m0758;
    /* 0x075C */ u8 m075C;
    /* 0x075D */ u8 m075D[0x0760 - 0x075D];
    /* 0x0760 */ int m0760;
    /* 0x0764 */ int m0764;
    /* 0x0768 */ u8 m0768;
    /* 0x0769 */ u8 m0769[0x076C - 0x0769];
    /* 0x076C */ request_of_phase_process_class mPhs;
    /* 0x0774 */ u32 mShadowId;
    /* 0x0778 */ mDoExt_invisibleModel mInvisibleModel;
    /* 0x0780 */ dKy_tevstr_c mTevstr;
    /* 0x0830 */ cXyz mFollowPos;
    /* 0x083C */ dBgS_ObjLinChk mLinChk;
    /* 0x08A8 */ daPz_matAnm_c* mMatAnm[2];
    /* 0x08B0 */ int mTalkState;
    /* 0x08B4 */ fpc_ProcID mArrowId;
    /* 0x08B8 */ cXyz mAttPos;
    /* 0x08C4 */ cXyz mLookPos;
    /* 0x08D0 */ cXyz mEyeLookPos;
    /* 0x08DC */ cXyz mEyePos;
    /* 0x08E8 */ s16 mLookAngleY;
    /* 0x08EA */ bool m08EA;
    /* 0x08EB */ u8 m08EB;
    /* 0x08EC */ int m08EC;
    /* 0x08F0 */ int m08F0;
    /* 0x08F4 */ int m08F4;
    /* 0x08F8 */ int m08F8;
    /* 0x08FC */ dLib_circle_path_c mCirclePath;
    /* 0x0920 */ int m0920;
    /* 0x0924 */ f32 m0924;
    /* 0x0928 */ enemyice mEventIce;
    /* 0x0CE0 */ enemyfire mEnemyFire;
    /* 0x0F08 */ Mtx mWaistMtx;
    /* 0x0F38 */ cXyz mWaistPos;
    /* 0x0F44 */ f32 m0F44;
    /* 0x0F48 */ u8 m0F48;
    /* 0x0F49 */ u8 m0F49[0x0F4C - 0x0F49];
    /* 0x0F4C */ cXyz mGanondorfPosCurrent;
    /* 0x0F58 */ cXyz mGanondorfPosEye;
    /* 0x0F64 */ u8 mbHasGanondorf;
    /* 0x0F65 */ u8 m0F65;
    /* 0x0F66 */ bool m0F66;
    /* 0x0F67 */ u8 m0F67;
    /* 0x0F68 */ f32 mKnockback;
    /* 0x0F6C */ s16 mHitAngleY;
    /* 0x0F6E */ u8 m0F6E[0x0F70 - 0x0F6E];
    /* 0x0F70 */ f32 m0F70;
    /* 0x0F74 */ u8 m0F74[0x0F78 - 0x0F74];
    /* 0x0F78 */ s16 mWaist2AngleZ;
    /* 0x0F7A */ s16 mWaist2AngleY;
#if VERSION > VERSION_DEMO
    /* 0x0F7C */ int m0F7C;
    /* 0x0F80 */ u8 m0F80;
    /* 0x0F81 */ u8 m0F81;
#endif
    /* 0x0F82 */ u8 mEventOrder;
    /* 0x0F83 */ u8 m0F83;
    /* 0x0F84 */ int mArg;
    /* 0x0F88 */ u8 mbDrawBow;
    /* 0x0F89 */ u8 m0F89[0x0F8C - 0x0F89];
    /* 0x0F8C */ mDoExt_McaMorf* mpBowMcaMorf;
    /* 0x0F90 */ mDoExt_brkAnm mBrkAnm;
    /* 0x0FA8 */ mDoExt_btkAnm mBtkAnm;
    /* 0x0FBC */ mDoExt_btpAnm mBtpAnm;
    /* 0x0FD0 */ J3DJoint* mRootJoint;
    /* 0x0FD4 */ J3DMaterial* mEyeMat[6];
    /* 0x0FEC */ J3DMaterial* mEyebrowMat[6];
    /* 0x1004 */ J3DMaterial* mFaceMat[4];
    /* 0x1014 */ J3DShape* mEyeShape[6];
    /* 0x102C */ J3DShape* mEyebrowShape[6];
    /* 0x1044 */ J3DShape* mFaceShape[4];
    /* 0x1054 */ mDoExt_offCupOnAupPacket mOffCupOnAup1;
    /* 0x1064 */ mDoExt_offCupOnAupPacket mOffCupOnAup2;
    /* 0x1074 */ mDoExt_onCupOffAupPacket mOnCupOffAup1;
    /* 0x1084 */ mDoExt_onCupOffAupPacket mOnCupOffAup2;
};

STATIC_ASSERT(sizeof(daPz_c) == DEMO_SELECT(0x1090, 0x1094));

#endif /* D_A_PZ_H */
