#ifndef D_A_NPC_SO_H
#define D_A_NPC_SO_H

#include "d/d_npc.h"
#include "d/d_particle.h"
#include "d/d_cc_d.h"
#include "d/d_jnt_hit.h"
#include "d/d_lib.h"
#include "m_Do/m_Do_hostIO.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"

class J3DNode;
class J3DModel;

class daNpc_So_c : public fopNpc_npc_c {
public:
    enum Proc_e {
        PROC_INIT_e = 0,
        PROC_EXEC_e = 1,
    };

    enum Mode_e {
        MODE_WAIT_e = 0,
        MODE_HIDE_e = 1,
        MODE_JUMP_e = 2,
        MODE_SWIM_e = 3,
        MODE_NEAR_SWIM_e = 4,
        MODE_EVENT_FIRST_WAIT_e = 5,
        MODE_EVENT_FIRST_e = 6,
        MODE_EVENT_FIRST_END_e = 7,
        MODE_EVENT_ESA_e = 8,
        MODE_EVENT_MAPOPEN_e = 9,
        MODE_EVENT_BOW_e = 10,
        MODE_TALK_e = 11,
        MODE_DISAPPEAR_e = 12,
        MODE_DEBUG_e = 13,
        MODE_GET_RUPEE_e = 14,
        MODE_EVENT_TRIFORCE_e = 15,
    };

    int getMiniGameRestArrow() { return 10 - mB78; }
    void isAnm(signed char) {}
    void modeProcInit(int newMode) { modeProc(PROC_INIT_e, newMode); }

    fopAc_ac_c* _searchEsa(fopAc_ac_c*);
    void _nodeControl(J3DNode*, J3DModel*);
    fopAc_ac_c* _searchTagSo(fopAc_ac_c*);
    fopAc_ac_c* _searchMinigameTagSo(fopAc_ac_c*);
    s16 XyCheckCB(int);
    s16 XyEventCB(int);
    BOOL _createHeap();
    bool jntHitCreateHeap();
    bool checkTgHit();
    void offsetZero();
    void offsetDive();
    void offsetSwim();
    void offsetAppear();
    virtual u32 getMsg();
    virtual u16 next_msgStatus(u32*);
    void lookBack();
    void setAttention();
    void setAnm(signed char, bool);
    void setAnmSwimSpeed();
    void setMtx();
    void modeWaitInit();
    void modeWait();
    void modeHideInit();
    void modeHide();
    void modeJumpInit();
    void modeJump();
    void modeSwimInit();
    void modeSwim();
    void modeNearSwimInit();
    void modeNearSwim();
    void modeEventFirstWaitInit();
    void modeEventFirstWait();
    void modeEventFirstInit();
    void modeEventFirst();
    void modeEventFirstEndInit();
    void modeEventFirstEnd();
    void modeEventEsaInit();
    void modeEventEsa();
    void modeEventMapopenInit();
    void modeEventMapopen();
    void modeEventBowInit();
    void modeEventBow();
    void modeTalkInit();
    void modeTalk();
    void modeDisappearInit();
    void modeDisappear();
    void modeDebugInit();
    void modeDebug();
    void modeGetRupeeInit();
    void modeGetRupee();
    void modeEventTriForceInit();
    void modeEventTriForce();
    void modeProc(Proc_e, int);
    void eventOrder();
    void checkOrder();
    void setScale();
    bool _execute();
    void debugDraw();
    void hudeDraw();
    bool _draw();
    void createInit();
    void getArg();
    cPhs_State _create();
    bool _delete();

    void cutAppearProc();
    void cutAppearStart();
    void cutDisappearProc();
    void cutDisappearStart();
    void cutDiveProc();
    void cutDiveStart();
    void cutEatesaFirstProc();
    void cutEatesaFirstStart();
    void cutEatesaProc();
    void cutEatesaStart();
    void cutEffectProc();
    void cutEffectStart();
    void cutEquipProc();
    void cutEquipStart();
    void cutJumpMapopenProc();
    void cutJumpMapopenStart();
    void cutJumpProc();
    void cutJumpStart();
    void cutMiniGameEndProc();
    void cutMiniGameEndStart();
    void cutMiniGamePlTurnProc();
    void cutMiniGamePlTurnStart();
    void cutMiniGamePlUpProc();
    void cutMiniGamePlUpStart();
    void cutMiniGameProc();
    void cutMiniGameReturnProc();
    void cutMiniGameReturnStart();
    void cutMiniGameStart();
    void cutMiniGameWaitProc();
    void cutMiniGameWaitStart();
    void cutMiniGameWarpProc();
    void cutMiniGameWarpStart();
    void cutPartnerShipProc();
    void cutPartnerShipStart();
    void cutProc();
    void cutSetAnmProc();
    void cutSetAnmStart();
    void cutSwimProc();
    void cutSwimStart();
    void cutTurnProc();
    void cutTurnStart();
    void cutUnequipProc();
    void cutUnequipStart();
    void initCam();
    void moveCam();

    static const u32 m_heapsize;
    static const char m_arc_name[];
    static const dCcD_SrcSph m_sph_src;

public:
    /* 0x6C4 */ u8 m6C4[0x6CC - 0x6C4];
    /* 0x6CC */ int mCurMode;
    /* 0x6D0 */ s16 mPrmAngleX;
    /* 0x6D2 */ s8 m6D2;
    /* 0x6D3 */ s8 mType;
    /* 0x6D4 */ s8 m6D4;
    /* 0x6D5 */ u8 m6D5[0x6D8 - 0x6D5];
    /* 0x6D8 */ int mHitTimer;
    /* 0x6DC */ dCcD_Stts mStts2;
    /* 0x718 */ dCcD_Sph mSph;
    /* 0x844 */ request_of_phase_process_class mPhs;
    /* 0x84C */ mDoExt_McaMorf* mpMorf2;
    /* 0x850 */ J3DModel* mpHudeModel;
    /* 0x854 */ mDoExt_btpAnm mBtpAnm;
#if VERSION > VERSION_DEMO
    /* 0x868 */ int mBtpTimer;
    /* 0x86C */ s16 mBtpFrame;
    /* 0x86E */ u8 m86E[0x870 - 0x86E];
#endif
    /* 0x870 */ dBgS_ObjAcch mAcch2;
    /* 0xA34 */ dBgS_AcchCir mAcchCir2;
    /* 0xA74 */ u32 mShadowId;
    /* 0xA78 */ u8 mHudeFlag;
    /* 0xA79 */ u8 mTagId;
    /* 0xA7A */ u8 mA7A[0xA7C - 0xA7A];
    /* 0xA7C */ f32 mTagRadius;
    /* 0xA80 */ cXyz mHidePos;
    /* 0xA8C */ u8 mA8C[0xA90 - 0xA8C];
    /* 0xA90 */ int mA90;
    /* 0xA94 */ u8 mA94[0xA9C - 0xA94];
    /* 0xA9C */ int mTimer;
    /* 0xAA0 */ u8 mAA0[0xAA8 - 0xAA0];
    /* 0xAA8 */ JntHit_c* mpJntHit;
    /* 0xAAC */ cXyz mAAC;
    /* 0xAB8 */ u8 mAB8[0xAE8 - 0xAB8];
    /* 0xAE8 */ dPa_rippleEcallBack mRipple;
    /* 0xAFC */ f32 mSpeed;
    /* 0xB00 */ f32 mB00;
    /* 0xB04 */ f32 mB04;
    /* 0xB08 */ f32 mB08;
    /* 0xB0C */ u8 mB0C;
    /* 0xB0D */ u8 mB0D[0xB10 - 0xB0D];
    /* 0xB10 */ dLib_circle_path_c mCirclePath;
    /* 0xB34 */ f32 mB34;
    /* 0xB38 */ cXyz mOffset;
    /* 0xB44 */ cXyz mLookAt;
    /* 0xB50 */ s16 mB50;
    /* 0xB52 */ u8 mB52[0xB54 - 0xB52];
    /* 0xB54 */ cXyz mSoEyePos;
    /* 0xB60 */ cXyz mSoAttnPos;
    /* 0xB6C */ int mStaffIdx;
    /* 0xB70 */ u8 mB70;
    /* 0xB71 */ u8 mB71[0xB74 - 0xB71];
    /* 0xB74 */ int mB74;
    /* 0xB78 */ int mB78;
    /* 0xB7C */ int mB7C;
    /* 0xB80 */ int mB80;
    /* 0xB84 */ u8 mB84;
    /* 0xB85 */ u8 mB85[0xB88 - 0xB85];
    /* 0xB88 */ f32 mB88;
    /* 0xB8C */ f32 mB8C;
    /* 0xB90 */ cXyz mMinigamePos;
    /* 0xB9C */ s16 mMinigameAngle;
    /* 0xB9E */ u8 mB9E[0xBA0 - 0xB9E];
    /* 0xBA0 */ cXyz mBA0;
    /* 0xBAC */ s16 mBAC;
    /* 0xBAE */ u8 mMinigameTagFound;
    /* 0xBAF */ u8 mBAF;
    /* 0xBB0 */ cXyz mBB0;
    /* 0xBBC */ int mBBC;
    /* 0xBC0 */ cXyz mBC0;
    /* 0xBCC */ cXyz mBCC;
    /* 0xBD8 */ u8 mBD8;
    /* 0xBD9 */ u8 mBD9;
    /* 0xBDA */ bool mBDA;
    /* 0xBDB */ u8 mBDB;
    /* 0xBDC */ s16 mEventIdx;
    /* 0xBDE */ u8 mBDE;
    /* 0xBDF */ u8 mBDF;
#if VERSION > VERSION_DEMO
    /* 0xBE0 */ int mBE0;
#endif
};  // Size: 0xBE4

STATIC_ASSERT(sizeof(daNpc_So_c) == DEMO_SELECT(0xBD8, 0xBE4));

#endif /* D_A_NPC_SO_H */
