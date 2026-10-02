#ifndef D_A_NPC_P2_H
#define D_A_NPC_P2_H

#include "d/d_npc.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_hostIO.h"
#include "SSystem/SComponent/c_phase.h"

class himo3_class;

class daNpc_P2_c : public fopAc_ac_c {
public:
    typedef int (daNpc_P2_c::*ActionFunc)(void*);

    cXyz& getAttentionBasePos() { return mAttnPos; }
    s16 getBackbone_x() { return mJnt.getBackbone_x(); }
    s16 getBackbone_y() { return mJnt.getBackbone_y(); }
    cXyz& getEyePos() { return mEyePos; }
    s16 getHead_x() { return mJnt.getHead_x(); }
    s16 getHead_y() { return mJnt.getHead_y(); }
    void incAttnSetCount() {
        if (mAttnSetCount != 0xFF) {
            mAttnSetCount++;
        }
    }
    void setAction(ActionFunc action, void* arg) {
        if (mActionFunc != action) {
            if (mActionFunc) {
                m808 = -1;
                (this->*mActionFunc)(NULL);
            }
            mActionFunc = action;
            m808 = 0;
            (this->*mActionFunc)(arg);
        }
    }
    void setEyeStopFlag() { mEyeStopFlag = true; }

    BOOL initTexPatternAnm(bool);
    void playTexPatternAnm();
    void setAnm();
    void setTexAnm();
    void setAttention();
    bool chkAttention();
    void lookBack();
    void setMtx();
    void setCollision();
    void smoke_set();
    u16 next_msgStatus(unsigned long*);
    u32 getMsg();
    void talkInit();
    void anmAtr(unsigned short);
    u16 talk(bool);
    void eventOrder();
    void checkOrder();
    void demo_wait_2();
    void demo_intro_2();
    void goal_wait_2();
    void demo_goal_2();
    void demo_bomb_get();
    void demo_wait();
    void demo_intro();
    void demo_lift();
    void demo_jump();
    void goal_goalpos_to_talkpos();
    void goal_talkpos_to_goalpos();
    void goal_goalpos_wait();
    void goal_talkpos_wait();
    void demo_goal();
    void goal_talkpos_talk();
    void treasure_wait();
    void treasure_wait_talk();
    void demo_arrive();
    void wait01();
    void zukotelescope();
    void moccowait();
    void talk01();
    int intro_action(void*);
    int wait_action(void*);
    bool _execute();
    void draw_item(J3DModel*, signed char);
    void drawDagger();
    void drawHead();
    void drawShadow();
    void drawP2a();
    void drawP2b();
    void drawP2c();
    bool _draw();
    void getArg();
    BOOL _createHeap();
    void createInit();
    cPhs_State _create();
    bool _delete();
    void cutProc();
    void cutTalkStart(int);
    void cutTalkProc(int);
    void cutRideSwitchStart(int);
    void cutRideSwitchProc(int);
    void cutRunWaitStart(int);
    void cutRunWaitProc(int);
    static void* searchNearLift(void*, void*);
    void cutJumpToLiftStart(int);
    void cutJumpToLiftProc(int);
    static void* searchNearRope(void*, void*);
    void cutLiftToRopeStart(int);
    void cutLiftToRopeProc(int);
    void cutRopeTalkStart(int);
    void cutRopeTalkProc(int);
    void cutRopeToLiftStart(int);
    void cutRopeToLiftProc(int);
    void cutJumpToGoalStart(int);
    void cutJumpToGoalProc(int);
    void cutJumpStart(int);
    void cutJumpProc(int);
    void cutSetAnmStart(int);
    void cutSetAnmProc(int);
    void cutSwOnStart(int);
    void cutSwOnProc(int);
    void cutSwOffStart(int);
    void cutSwOffProc(int);
    void cutSurpriseStart(int);
    void cutSurpriseProc(int);
    void cutOmamoriInitStart(int);
    void cutOmamoriInitProc(int);
    void cutOmamoriEndStart(int);
    void cutOmamoriEndProc(int);

    static const u32 m_heapsize[3];
    static const char m_arc_name[3];

public:
    /* 0x290 */ u8 mType;
    /* 0x291 */ u8 m291;
    /* 0x292 */ u8 mSwitchNo;
    /* 0x293 */ u8 m293;
    /* 0x294 */ request_of_phase_process_class mPhs;
    /* 0x29C */ mDoExt_McaMorf* mpMorf;
    /* 0x2A0 */ mDoExt_McaMorf* mpMorf2;
    /* 0x2A4 */ mDoExt_btpAnm mBtpAnm;
    /* 0x2B8 */ J3DModel* mpHeadModel;
    /* 0x2BC */ J3DModel* mpModel2BC;
    /* 0x2C0 */ J3DModel* mpModel2C0;
    /* 0x2C4 */ J3DModel* mpModel2C4;
    /* 0x2C8 */ u32 mShadowId;
    /* 0x2CC */ dNpc_JntCtrl_c mJnt;
    /* 0x300 */ dNpc_EventCut_c mEventCut;
    /* 0x36C */ u8 m36C[0x370 - 0x36C];
    /* 0x370 */ u8 mbAnimFinished;
    /* 0x371 */ u8 m371[0x374 - 0x371];
    /* 0x374 */ f32 m374;
    /* 0x378 */ u8 mBtpFrame;
    /* 0x379 */ u8 m379;
    /* 0x37A */ s16 mBtpTimer;
    /* 0x37C */ dBgS_ObjAcch mAcch;
    /* 0x540 */ dBgS_AcchCir mAcchCir;
    /* 0x580 */ dCcD_Stts mStts;
    /* 0x5BC */ dCcD_Cyl mCyl;
    /* 0x6EC */ csXyz m6EC;
    /* 0x6F2 */ s16 mMaxHeadVel;
    /* 0x6F4 */ u8 m6F4[0x6F8 - 0x6F4];
    /* 0x6F8 */ cXyz mEyePos;
    /* 0x704 */ cXyz mAttnPos;
    /* 0x710 */ u8 mAttnSetCount;
    /* 0x711 */ u8 m711;
    /* 0x712 */ u16 m712;
    /* 0x714 */ cXyz mClosestPos;
    /* 0x720 */ fopAc_ac_c* mActor;
    /* 0x724 */ u8 m724;
    /* 0x725 */ u8 mHasAttention;
    /* 0x726 */ u8 m726[0x728 - 0x726];
    /* 0x728 */ u32 mMsgNo;
    /* 0x72C */ u8 m72C;
    /* 0x72D */ u8 m72D[0x730 - 0x72D];
    /* 0x730 */ cXyz m730;
    /* 0x73C */ cXyz m73C;
    /* 0x748 */ s16 m748;
    /* 0x74A */ u8 m74A;
    /* 0x74B */ u8 m74B;
    /* 0x74C */ int m74C;
    /* 0x750 */ u8 mEyeStopFlag;
    /* 0x751 */ u8 m751;
    /* 0x752 */ u8 m752[0x754 - 0x752];
    /* 0x754 */ dPa_smokeEcallBack mSmoke;
    /* 0x774 */ cXyz mSavedPos;
    /* 0x780 */ csXyz mSavedAngle;
    /* 0x786 */ u8 m786;
    /* 0x787 */ u8 m787;
    /* 0x788 */ u32 m788;
    /* 0x78C */ cXyz m78C;
    /* 0x798 */ f32 m798;
    /* 0x79C */ f32 m79C;
    /* 0x7A0 */ f32 m7A0;
    /* 0x7A4 */ f32 m7A4;
    /* 0x7A8 */ s16 m7A8;
    /* 0x7AA */ s16 m7AA;
    /* 0x7AC */ s32 m7AC;
    /* 0x7B0 */ cXyz mSoundPos;
    /* 0x7BC */ himo3_class* mpHimo3;
    /* 0x7C0 */ u8 m7C0;
    /* 0x7C1 */ u8 m7C1;
    /* 0x7C2 */ u8 m7C2;
    /* 0x7C3 */ u8 m7C3;
    /* 0x7C4 */ ActionFunc mActionFunc;
    /* 0x7D0 */ s8 m7D0;
    /* 0x7D1 */ s8 m7D1;
    /* 0x7D2 */ u8 m7D2;
    /* 0x7D3 */ s8 mAnmNo;
    /* 0x7D4 */ s8 m7D4;
    /* 0x7D5 */ s8 m7D5;
    /* 0x7D6 */ s8 m7D6;
    /* 0x7D7 */ u8 m7D7;
    /* 0x7D8 */ u8 m7D8[0x7DC - 0x7D8];
    /* 0x7DC */ cXyz m7DC;
    /* 0x7E8 */ cXyz m7E8;
    /* 0x7F4 */ int m7F4;
    /* 0x7F8 */ f32 m7F8;
    /* 0x7FC */ f32 m7FC;
    /* 0x800 */ f32 m800;
    /* 0x804 */ int m804;
    /* 0x808 */ s8 m808;
    /* 0x809 */ s8 mTalkState;
    /* 0x80A */ s8 m80A;
    /* 0x80B */ u8 m80B;
};  // Size: 0x80C

STATIC_ASSERT(sizeof(daNpc_P2_c) == 0x80C);

#endif /* D_A_NPC_P2_H */
