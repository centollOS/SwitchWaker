#ifndef D_A_NPC_CO1_H
#define D_A_NPC_CO1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Co1_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Co1_c::*ActionFunc)(void*);
    struct anm_prm_c {
        s8 mAnm, mBtp;
        f32 mMorf, mSpeed;
        int mLoop;
    };

    void nodeCo1Control(J3DNode*, J3DModel*);
    bool init_CO1_0();
    bool createInit();
    void setMtx(bool);
    int anmNum_toResID(int);
    int anmNum_toResID_prl(int);
    int btpNum_toResID(int);
    bool setBtp(bool, int);
    bool setBtk(bool);
    bool iniTexPttrnAnm(bool);
    void plyTexPttrnAnm();
    void setAnm_tex(signed char);
    bool setAnm_anm(anm_prm_c*);
    void setAnm_NUM(int, int);
    bool setAnm();
    void chg_anmTag();
    void control_anmTag();
    void chg_anmAtr(unsigned char);
    void control_anmAtr();
    void setAnm_ATR(int);
    void anmAtr(unsigned short);
    void eventOrder();
    void checkOrder();
    void setCollision_SP_();
    void set_target(int);
    bool chk_talk();
    bool chk_partsNotMove();
    void lookBack();
    u16 next_msgStatus(unsigned long*);
    u32 getMsg_CO1_0();
    u32 getMsg();
    bool chkAttention();
    void setAttention(bool);
    bool charDecide(int);
    void eInit_MDR_();
    void eInit_RED_LTR_();
    void event_actionInit(int);
    bool eMove_MDR_();
    bool eMove_RED_LTR_();
    bool event_action();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Co1_c::*)(void*), void*);
    void setStt(signed char);
    BOOL wait_1();
    BOOL wait_2();
    BOOL wakeup();
    BOOL talk_1();
    BOOL toru_1();
    BOOL read_1();
    BOOL modoru();
    int wait_action1(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    J3DModelData* create_Anm();
    J3DModelData* create_prl_Anm();
    bool create_itm_Mdl();
    BOOL CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhs;
    /* 0x6CC */ s8 m_hed_jnt_num;
    /* 0x6CD */ s8 m_bbone_jnt_num;
    /* 0x6CE */ s8 m_hnd_R_jnt_num;
    /* 0x6D0 */ mDoExt_McaMorf* mpPearlMorf;
    /* 0x6D4 */ J3DAnmTextureSRTKey* m_prl_btk;
    /* 0x6D8 */ s8 m_prl_jnt_num;
    /* 0x6DC */ mDoExt_btkAnm mBtkAnm;
    /* 0x6F0 */ u8 mBtkFrame;
    /* 0x6F4 */ J3DModel* mpLetterModel;
    /* 0x6F8 */ u32 mShadowId;
    /* 0x6FC */ J3DAnmTexPattern* m_hed_tex_pttrn;
    /* 0x700 */ mDoExt_btpAnm mBtpAnm;
    /* 0x714 */ u8 mBtpFrame;
    /* 0x716 */ s16 mBtpTimer;
    /* 0x718 */ ActionFunc mAction;
    /* 0x724 */ u8 m724[8];
    /* 0x72C */ dNpc_EventCut_c mEventCut;
    /* 0x798 */ u32 m798;
    /* 0x79C */ cXyz mInitialPos;
    /* 0x7A8 */ csXyz mInitialAngle;
    /* 0x7B0 */ cXyz mEyePos;
    /* 0x7BC */ cXyz mLookPos;
    /* 0x7C8 */ cXyz m7C8;
    /* 0x7D4 */ f32 mFrame;
    /* 0x7D8 */ u8 m7D8[0x14];
    /* 0x7EC */ s16 mPreviousHeadY;
    /* 0x7EE */ s16 mPreviousBackboneY;
    /* 0x7F0 */ s16 mPreviousAngleY;
    /* 0x7F2 */ s16 mEventIdx[3];
    /* 0x7F8 */ s16 mEventNo;
    /* 0x7FA */ s16 m7FA;
    /* 0x7FC */ s16 mLookTimer;
    /* 0x7FE */ s16 m7FE;
    /* 0x800 */ s16 m800;
    /* 0x802 */ s16 mTurnSpeed;
    /* 0x804 */ s16 mLookAngle;
    /* 0x806 */ s16 m806;
    /* 0x808 */ s8 mAnmEnd;
    /* 0x809 */ s8 m809;
    /* 0x80A */ s8 mReadLetter;
    /* 0x80B */ u8 mItemNo;
    /* 0x80C */ u8 m80C;
    /* 0x80D */ bool mHidden;
    /* 0x80E */ u8 m80E;
    /* 0x80F */ bool mKeepShapeAngle;
    /* 0x810 */ bool mNoMove;
    /* 0x811 */ bool mNoDraw;
    /* 0x812 */ u8 m812;
    /* 0x813 */ bool mDrawLetter;
    /* 0x814 */ u8 m814;
    /* 0x815 */ bool mSavedInitialPos;
    /* 0x816 */ u8 m816[2];
    /* 0x818 */ BOOL mUpdateEye;
    /* 0x81C */ bool mHasAttention;
    /* 0x81D */ bool mTalking;
    /* 0x81E */ bool mNoTurn;
    /* 0x81F */ bool mDemo;
    /* 0x820 */ LIGHT_INFLUENCE mLight;
    /* 0x840 */ cXyz mPearlPos;
    /* 0x84C */ f32 mLightWave;
    /* 0x850 */ s16 mLightAngle;
    /* 0x852 */ s16 m852;
    /* 0x854 */ s8 mCut;
    /* 0x855 */ s8 mEventAction;
    /* 0x856 */ u8 mAnmAttr;
    /* 0x857 */ u8 mAnmTag;
    /* 0x858 */ s8 mBtpNo;
    /* 0x859 */ s8 mAnmNo;
    /* 0x85A */ u8 m85A[2];
    /* 0x85C */ s8 mOrder;
    /* 0x85D */ s8 mState;
    /* 0x85E */ s8 mPreviousState;
    /* 0x85F */ s8 mLookMode;
    /* 0x860 */ s8 mType;
    /* 0x861 */ s8 mSubType;
    /* 0x862 */ s8 mActionState;
    /* 0x863 */ s8 mMsgAnmStarted;
};

class daNpc_Co1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        s16 mMaxHeadX, mMaxHeadY, mMinHeadX, mMinHeadY;
        s16 mMaxBackBoneX, mMaxBackBoneY, mMinBackBoneX, mMinBackBoneY;
        s16 mMaxTurnStep, mTurnSpeed;
        f32 mAttentionOffsetY;
        bool m18;
        s16 mLightR, mLightG, mLightB;
        f32 mMaxLightPower, mMinLightPower;
        s16 mLightSpeed, mMinLightIntensity;
    };
    daNpc_Co1_HIO_c();
    inline virtual ~daNpc_Co1_HIO_c();
    void genMessage(JORMContext*) {}
    s8 m04;
    s32 m08;
    hio_prm_c mPrm;
};

#endif /* D_A_NPC_CO1_H */
