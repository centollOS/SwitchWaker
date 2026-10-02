#ifndef D_A_NPC_YW1_H
#define D_A_NPC_YW1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Yw1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c { s8 mAnm, mBtp; f32 mMorf, mSpeed; int mLoop; };
    typedef int (daNpc_Yw1_c::*ActionFunc)(void*);

    void _nodeCB_Hair(J3DNode*, J3DModel*);
    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_YW1_0();
    bool init_YW1_1();
    bool init_YW1_2();
    bool init_YW1_3();
    bool createInit();
    void play_animation();
    bool upLift();
    void setMtx(bool);
    int bckResID(int);
    int btpResID(int);
    bool init_texPttrnAnm(signed char, bool);
    void play_texPttrnAnm();
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
    u32 getMsg_YW1_0();
    u32 getMsg_YW1_1();
    u32 getMsg_YW1_2();
    u32 getMsg_YW1_3();
    virtual u32 getMsg();
    void eventOrder();
    void checkOrder();
    bool chk_direct(f32);
    bool chk_talk();
    bool chk_parts_notMov();
    fopAc_ac_c* searchByID(fpc_ProcID, int*);
    bool partner_search_sub(void* (*)(void*, void*));
    void partner_search();
    void lookBack();
    bool chkAttention();
    void setAttention(bool);
    bool decideType(int);
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Yw1_c::*)(void*), void*);
    void setStt(signed char);
    void chngTsuboAnm();
    bool chk_areaIN(float, float, short, cXyz);
    void set_pthPoint(unsigned char);
    void setHairAngle();
    bool chk_brkTsubo();
    bool chk_bm1Odoroki();
    int wait_1();
    int wait_2();
    int wait_3();
    int walk_1();
    int turn_1();
    int talk_1();
    int wait_action1(void*);
    int wait_action2(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    int bodyCreateHeap();
    int headCreateHeap();
    int CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhs;
#if VERSION > VERSION_JPN
    /* 0x6CC */ s8 m_hed_jnt_num, m_bbone_jnt_num, m_hair1, m_hair2, m_hair3;
#else
    /* 0x6CC */ s8 m_hed_jnt_num, m_bbone_jnt_num;
#endif
    /* 0x6D4 */ u32 mShadow;
    /* 0x6D8 */ J3DModel* mpHeadModel;
    /* 0x6DC */ mDoExt_btpAnm mBtpAnm;
    /* 0x6F0 */ u8 mTexFrame;
    /* 0x6F2 */ s16 mBlinkTimer;
    /* 0x6F4 */ ActionFunc mAction;
    /* 0x700 */ fpc_ProcID mPartnerID, mPotID, mLookID;
    /* 0x70C */ dNpc_PathRun_c mPath;
    /* 0x714 */ cXyz mInitialPos;
    /* 0x720 */ csXyz mInitialAngle, mModelAngle;
    /* 0x72C */ cXyz mEyePos, mLookPos, mHeadPos;
    /* 0x750 */ f32 mFrame, m754;
    /* 0x758 */ s16 mOldAngle, mOldHead, mOldBackbone, m75E;
    /* 0x760 */ int mSetEyePos;
    /* 0x764 */ s16 m764, mLookTimer, m768, m76A, mTurnSpeed, mLookAngle;
    /* 0x770 */ s8 mAnmEnd, mAnmLoops;
    /* 0x772 */ u8 mItem;
    /* 0x773 */ bool mChangePotAnm, mPaidPot, mPotMissing, mPathEnd, mHidden, mKeepShapeAngle, m779, mInitialized, mAttention, mTalking, mTurnBody, mDemo;
#if VERSION > VERSION_JPN
    /* 0x77F */ u8 m77F;
    /* 0x780 */ s16 mNeckX, mNeckY, mHairX, mHairY;
    /* 0x788 */ cXyz m788, mOldHeadPos;
    /* 0x7A0 */ s16 mHairVelX, mHairVelY, mHair2X, mHair2Y, mHair2VelX, mHair2VelY, mHair3X, mHair3Y, mHair3VelX, mHair3VelY, mHairPhase, mHairWave1, mHairWave2, mHairWave3;
#endif
    /* 0x7BC */ s8 mCut;
    /* 0x7BD */ u8 mAnmAttr, mAnmTag;
    /* 0x7BF */ s8 mBtpNo, mAnmNo, mOrder, mState, mPreviousState, mLookMode, mType, mSubType, mActionState, mMsgAnmStarted;
};
#if VERSION > VERSION_JPN
STATIC_ASSERT(sizeof(daNpc_Yw1_c) == 0x7CC);
#else
STATIC_ASSERT(sizeof(daNpc_Yw1_c) == 0x788);
#endif
struct Yw1Prm {
    s16 maxHeadX, maxHeadY, minHeadX, minHeadY;
    s16 maxBackX, maxBackY, minBackX, minBackY;
    s16 maxTurn, turnSpeed;
    f32 attentionHeight;
    u8 debug;
    f32 areaRadius;
    s16 walkTurnRate, walkTurnSpeed;
    f32 animationSpeed, walkSpeed, acceleration;
};
class daNpc_Yw1_childHIO_c : public mDoHIO_entry_c {
public:
    daNpc_Yw1_childHIO_c();
    inline virtual ~daNpc_Yw1_childHIO_c();
    Yw1Prm mPrm;
    int mIndex;
};
class daNpc_Yw1_HIO_c : public mDoHIO_entry_c {
public:
    daNpc_Yw1_HIO_c();
    inline virtual ~daNpc_Yw1_HIO_c();
    s8 mNo;
    int mSelected;
    daNpc_Yw1_childHIO_c mChild[1];
};
#endif
