#ifndef D_A_NPC_AC1_H
#define D_A_NPC_AC1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Ac1_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Ac1_c::*ActionFunc)(void*);
    struct anm_prm_c {
        s8 mAnm, mBtp;
        f32 mMorf, mSpeed;
        int mLoop, mArms;
    };

    void nodeWngControl(J3DNode* node, J3DModel* model);
    void nodeArmControl(J3DNode* node, J3DModel* model);
    void nodeAc1Control(J3DNode* node, J3DModel* model);
    bool init_AC1_0();
    bool createInit();
    void setMtx(bool force);
    int anmNum_toResID(int index);
    int wingAnmNum_toResID(int index);
    int btpNum_toResID(int index);
    bool setBtp(bool modify, int number);
    bool iniTexPttrnAnm(bool modify);
    void plyTexPttrnAnm();
    void setAnm_tex(s8 number);
    bool setAnm_anm(anm_prm_c* prm);
    bool setAnm();
    void chg_anmTag();
    void control_anmTag();
    void chg_anmAtr(u8 attr);
    void control_anmAtr();
    void setAnm_ATR(int texture);
    void anmAtr(u16 status);
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    bool chk_partsNotMove();
    void lookBack();
    u16 next_msgStatus(u32* msg);
    s8 getBitMask();
    u32 getMsg_AC1_0();
    u32 getMsg();
    bool chkAttention();
    void setAttention(bool force);
    bool charDecide(int type);
    void event_actionInit(int staff);
    bool event_action();
    void privateCut(int staff);
    void endEvent();
    int isEventEntry();
    void event_proc(int staff);
    bool set_action(ActionFunc action, void* arg);
    void setStt(s8 state);
    BOOL wait_1();
    BOOL talk_1();
    int wait_action1(void* arg);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    J3DModelData* create_Anm();
    J3DModelData* create_wng_Anm();
    J3DModelData* create_arm_Anm();
    bool create_itm_Mdl();
    BOOL CreateHeap();
public:
    request_of_phase_process_class mPhs;
    s8 m_hed_jnt_num, m_bbone_jnt_num, m_arm_L_jnt_num, m_arm_R_jnt_num;
    s8 m_hnd_R_jnt_num;
    J3DModel* mpItemModel;
    u32 mShadowId;
    J3DAnmTexPattern* m_hed_tex_pttrn;
    mDoExt_btpAnm mBtpAnm;
    u8 mBtpFrame;
    s16 mBtpTimer;
    u32 m6F8;
    mDoExt_McaMorf* mpWingMorf;
    s8 m_wngL1_jnt_num, m_wngR1_jnt_num;
    u8 m702[6];
    mDoExt_McaMorf* mpArmMorf;
    s8 m_armL1_jnt_num, m_armR1_jnt_num;
    ActionFunc mAction;
    Mtx mLeftShoulder, mRightShoulder;
    dNpc_PathRun_c mPathRun;
    dNpc_EventCut_c mEventCut;
    u32 m7F0;
    cXyz mInitialPos;
    csXyz mInitialAngle;
    cXyz mEyePos, mLookPos, m820;
    f32 mFrame;
    u8 m830[0x14];
    s16 mPreviousHeadY, mPreviousBackboneY, mPreviousAngleY;
    s16 m84A, mLookTimer, m84E, m850, mTurnSpeed, mLookAngle, mPreviousMsgStatus;
    s8 mAnmEnd, m859;
    u8 m85A, mItemNo, m85C;
    bool mHidden, mArms, mKeepShapeAngle, mNoMove, mNoDraw;
    u8 m862;
    bool mSavedInitialPos;
    BOOL mUpdateEye;
    bool mHasAttention, mTalking, mNoTurn, mDemo;
    s8 mCut, mEventAction;
    u8 mAnmAttr, mAnmTag;
    s8 mBtpNo, mAnmNo;
    u8 m872[2];
    s8 mOrder, mState, mPreviousState, mLookMode, mType, mSubType, mActionState, mMsgAnmStarted;
};

class daNpc_Ac1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackBoneX, mMaxBackBoneY, mMinBackBoneX, mMinBackBoneY;
        s16 mMaxTurnStep, mTurnSpeed;
        f32 mAttentionOffsetY;
        bool m18;
        s16 m1A;
        u8 m1C;
        f32 m20, m24, m28, m2C;
    };
    daNpc_Ac1_HIO_c();
    inline virtual ~daNpc_Ac1_HIO_c();
    void genMessage(JORMContext*) {}
    s8 m04;
    s32 m08;
    hio_prm_c mPrm;
};

#endif /* D_A_NPC_AC1_H */
