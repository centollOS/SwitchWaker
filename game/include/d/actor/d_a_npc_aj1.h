#ifndef D_A_NPC_AJ1_H
#define D_A_NPC_AJ1_H

#include "d/d_npc.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;
class JPABaseEmitter;

class daNpc_Aj1_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Aj1_c::*ActionFunc)(void*);
    struct anm_prm_c { s8 mAnm, mBtp; f32 mMorf, mSpeed; int mLoop; };

    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    s16 _XyCheckCB(int);
    bool init_AJ1_0();
    bool init_AJ1_1();
    bool init_AJ1_2();
    bool createInit();
    void play_animation();
    void ctrl_WAITanm();
    void ctrl_TIREanm();
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
    void anmAtr(unsigned short);
    u16 next_msgStatus(unsigned long*);
    u32 getMsg_AJ1_0();
    u32 getMsg_AJ1_1();
    u32 getMsg_AJ1_2();
    u32 getMsg();
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    bool chk_parts_notMov();
    void lookBack();
    bool chkAttention();
    void setAttention(bool);
    bool decideType(int);
    void cut_init_AJ1_TLK();
    bool cut_move_AJ1_TLK();
    void cut_init_INI_ANGRY();
    bool cut_move_INI_ANGRY();
    void cut_init_VIVRATE();
    bool cut_move_VIVRATE();
    void cut_init_JMP();
    bool cut_move_JMP();
    void cut_init_SPPRISE();
    bool cut_move_SPPRISE();
    void cut_init_LOK();
    bool cut_move_LOK();
    void cut_init_DAN();
    bool cut_move_DAN();
    void cut_init_INVIT();
    bool cut_move_INVIT();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    void set_pa_pun();
    void set_pa_aka();
    void set_pa_don();
    void set_pa_smk();
    void flw_pa_pun();
    void flw_pa_aka();
    void del_pa_aka();
    void del_pa(JPABaseEmitter**);
    void setSmoke();
    bool set_action(int (daNpc_Aj1_c::*)(void*), void*);
    void setStt(signed char);
    bool chk_areaIN(float, short, cXyz);
    BOOL FARwai();
    BOOL call_1();
    BOOL wait_1();
    BOOL talk_1();
    int wait_action1(void*);
    int wait_action2(void*);
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
    request_of_phase_process_class mPhs;
    s8 m_hed_jnt_num, m_bbone_jnt_num, m_hnd_L_jnt_num, m_fot_L_jnt_num;
    J3DModel* mpItemModel;
    u32 mShadowId;
    mDoExt_btpAnm mBtpAnm;
    u8 mBtpFrame;
    s16 mBtpTimer;
    ActionFunc mAction;
    cXyz mInitialPos;
    csXyz mInitialAngle, mModelAngle;
    cXyz mEyePos, mLookPos, mHeadPos;
    s16 mPreviousAngleY, mPreviousHeadY, mPreviousBackboneY;
    f32 mPrevFrame;
    s16 mEventIdx[1], mEventNo, mWaitAnmTimer, mSteamTimer, mCutTimer, mTalkTimer, mFarTimer, m752, mTurnSpeed, mLookAngle;
    s8 mAnmEnd, m759;
    u8 mItemNo, mSwitch, mTireCycles, m75D;
    bool mHidden, mKeepShapeAngle, mNoDraw, mSavedInitialPos;
    u8 m762[2];
    BOOL mUpdateEye;
    bool mHasAttention, mTalking, mNoTurn, mDemo;
    dPa_smokeEcallBack mSmokeCallback;
    JPABaseEmitter *mpSmoke, *mpSteam, *mpRed, *mpImpact;
    cXyz mSmokePos, mImpactPos;
    bool mRedFading;
    s8 mCut;
    u8 mAnmAttr, mAnmTag;
    s8 mBtpNo, mAnmNo, mOrder, mState, mPreviousState, mLookMode, mType, mSubType, mActionState, mMsgAnmStarted;

};

class daNpc_Aj1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackBoneX, mMaxBackBoneY, mMinBackBoneX, mMinBackBoneY;
        s16 mMaxTurnStep, mTurnSpeed;
        f32 mAttentionOffsetY;
        bool mDebug;
        f32 mFarDistance;
        s16 mFarAngle;
        f32 mTalkDistance;
        s16 mTalkAngle, mLookDelay, mAngryDelay;
    };
    daNpc_Aj1_HIO_c();
    inline virtual ~daNpc_Aj1_HIO_c();
    void genMessage(JORMContext*) {}
    s8 m04;
    s32 m08;
    hio_prm_c mPrm;
};
#endif
