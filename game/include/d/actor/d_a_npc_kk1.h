#ifndef D_A_NPC_KK1_H
#define D_A_NPC_KK1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;
class J3DModel;
class JPABaseEmitter;

class daNpc_Kk1_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Kk1_c::*ActionFunc)(void*);

    struct anm_prm_c {
        /* 0x00 */ s8 mAnm;
        /* 0x01 */ s8 mBtp;
        /* 0x04 */ f32 mMorf;
        /* 0x08 */ f32 mSpeed;
        /* 0x0C */ int mLoop;
    };  // Size: 0x10

    u8 getSWbit() { return mSwNo; }

    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_KK1_0();
    bool createInit();
    void play_animation();
    void setMtx(bool);
    int bckResID(int);
    int btpResID(int);
    bool setBtp(signed char, bool);
    bool init_texPttrnAnm(signed char, bool);
    void play_btp_anm();
    void play_eff_anm();
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
    u32 getMsg_KK1_0();
    u32 getMsg();
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    bool chk_parts_notMov();
    fopAc_ac_c* searchByID(fpc_ProcID, int*);
    bool partner_search_sub(void* (*)(void*, void*));
    void partner_search();
    void lookBack();
    bool chkAttention();
    void setAttention(bool);
    bool decideType(int);
    void cut_init_RUN_START(int);
    bool cut_move_RUN_START();
    void cut_init_RUN(int);
    bool cut_move_RUN();
    void cut_init_CATCH_START(int);
    bool cut_move_CATCH_START();
    void cut_init_CATCH_END(int);
    bool cut_move_CATCH_END();
    void cut_init_TRN(int);
    bool cut_move_TRN();
    void cut_init_BYE_START(int);
    bool cut_move_BYE_START();
    void cut_init_BYE(int);
    bool cut_move_BYE();
    void cut_init_BYE_CONTINUE(int);
    bool cut_move_BYE_CONTINUE();
    void cut_init_BYE_END(int);
    bool cut_move_BYE_END();
    void cut_init_PLYER_TRN(int);
    bool cut_move_PLYER_TRN();
    void cut_init_OTOBOKE(int);
    bool cut_move_OTOBOKE();
    void cut_init_PLYER_MOV(int);
    bool cut_move_PLYER_MOV();
    void cut_init_RUNAWAY_START(int);
    bool cut_move_RUNAWAY_START();
    void cut_init_RUNAWAY_END(int);
    bool cut_move_RUNAWAY_END();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool checkCommandTalk();
    bool set_action(int (daNpc_Kk1_c::*)(void*), void*);
    void setStt(signed char);
    void createTama(float);
    bool chk_areaIN(float, cXyz);
    bool startEvent_check();
    bool chkHitPlayer();
    void set_pthPoint(unsigned char);
    bool event_move(bool);
    cXyz kyoroPos(int);
    BOOL kyorokyoro();
    bool chk_attn();
    void setBikon(cXyz);
    void delBikon();
    void setAse();
    void flwAse();
    void delAse();
    BOOL wait_1();
    BOOL walk_1();
    BOOL wait_2();
    void init_CMT_WAI();
    void move_CMT_WAI();
    void init_CMT_TRN();
    void move_CMT_TRN();
    void init_CMT_PCK();
    void move_CMT_PCK();
    BOOL cmmt_1();
    BOOL wait_3();
    BOOL wait_4();
    BOOL talk_1();
    int wait_action1(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL bodyCreateHeap();
    BOOL effcCreateHeap();
    BOOL CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhs;
    /* 0x6CC */ s8 m_hed_jnt_num;
    /* 0x6CD */ s8 m_bbone_jnt_num;
    /* 0x6CE */ char mArcName[3];
    /* 0x6D4 */ u32 mShadowId;
    /* 0x6D8 */ mDoExt_btpAnm mBtpAnm;
    /* 0x6EC */ u8 mBtpFrame;
    /* 0x6EE */ s16 mBtpTimer;
    /* 0x6F0 */ ActionFunc mAction;
    /* 0x6FC */ fpc_ProcID mPartnerId;
    /* 0x700 */ fpc_ProcID m700;
    /* 0x704 */ dNpc_PathRun_c mPath;
    /* 0x70C */ cXyz mInitialPos;
    /* 0x718 */ csXyz mInitialAngle;
    /* 0x71E */ csXyz mModelAngle;
    /* 0x724 */ u8 m724[8];
    /* 0x72C */ cXyz m72C;
    /* 0x738 */ cXyz mLookPos;
    /* 0x744 */ cXyz m744;
    /* 0x750 */ cXyz m750;
    /* 0x75C */ cXyz m75C;
    /* 0x768 */ f32 mFrame;
    /* 0x76C */ f32 m76C;
    /* 0x770 */ csXyz mPreviousAngles;
    /* 0x778 */ BOOL mUpdateEye;
#if VERSION > VERSION_JPN
    /* 0x77C */ s16 mEventIds[8];
    /* 0x78C */ s16 mEventIndex;
    /* 0x78E */ u8 m78E[0x792 - 0x78E];
#else
    /* 0x77C */ s16 mEventIds[7];
    /* 0x78A */ s16 mEventIndex;
    /* 0x78C */ u8 m78E[0x790 - 0x78C];
#endif
    /* 0x792 */ s16 m792;
    /* 0x794 */ s16 m794;
    /* 0x796 */ s16 m796;
    /* 0x798 */ s16 m798;
#if VERSION > VERSION_DEMO
    /* 0x79A */ s16 m79A;
#endif
    /* 0x79C */ s16 m79C;
    /* 0x79E */ s16 m79E;
    /* 0x7A0 */ s16 m7A0;
    /* 0x7A2 */ s16 m7A2;
    /* 0x7A4 */ s16 m7A4;
    /* 0x7A6 */ u8 m7A6[0x7AA - 0x7A6];
    /* 0x7AA */ s16 m7AA;
    /* 0x7AC */ s16 m7AC;
    /* 0x7AE */ s16 mLookAngle;
    /* 0x7B0 */ s16 m7B0;
    /* 0x7B2 */ s8 mAnmEnd;
    /* 0x7B3 */ s8 mAnmRepeat;
    /* 0x7B4 */ u8 mItemNo;
    /* 0x7B5 */ u8 mSwNo;
    /* 0x7B6 */ u8 m7B6;
    /* 0x7B7 */ u8 m7B7;
    /* 0x7B8 */ u8 m7B8;
    /* 0x7B9 */ u8 m7B9;
    /* 0x7BA */ u8 m7BA;
    /* 0x7BB */ u8 m7BB;
    /* 0x7BC */ bool m7BC;
    /* 0x7BD */ u8 m7BD;
    /* 0x7BE */ u8 m7BE;
    /* 0x7BF */ u8 m7BF;
    /* 0x7C0 */ u8 m7C0;
    /* 0x7C1 */ u8 m7C1;
    /* 0x7C2 */ u8 m7C2;
    /* 0x7C3 */ u8 m7C3;
    /* 0x7C4 */ bool m7C4;
    /* 0x7C5 */ u8 m7C5;
    /* 0x7C6 */ u8 m7C6;
    /* 0x7C7 */ u8 m7C7;
    /* 0x7C8 */ mDoExt_bpkAnm mBpkAnm;
    /* 0x7DC */ mDoExt_btkAnm mBtkAnm;
    /* 0x7F0 */ mDoExt_bckAnm mBckAnm;
    /* 0x800 */ s16 mBpkFrame;
    /* 0x802 */ s16 mBtkFrame;
    /* 0x804 */ s16 mBckFrame;
    /* 0x806 */ u8 m806[0x808 - 0x806];
    /* 0x808 */ J3DModel* mpEffModel;
#if VERSION == VERSION_DEMO
    /* 0x808 */ u8 m808[4];
#endif
    /* 0x80C */ JPABaseEmitter* mpBikon;
    /* 0x810 */ JPABaseEmitter* mpAse;
    /* 0x814 */ u8 m814;
    /* 0x815 */ u8 m815;
    /* 0x816 */ s8 m816;
    /* 0x817 */ u8 mAnmAttr;
    /* 0x818 */ u8 mAnmTag;
    /* 0x819 */ s8 mBtpNo;
    /* 0x81A */ s8 mAnmNo;
    /* 0x81B */ s8 mOrder;
    /* 0x81C */ s8 mState;
    /* 0x81D */ s8 mPreviousState;
    /* 0x81E */ s8 m81E;
    /* 0x81F */ s8 mType;
    /* 0x820 */ s8 mSubType;
    /* 0x821 */ s8 mActionState;
    /* 0x822 */ s8 mMsgAnmStarted;
};  // Size: 0x824

STATIC_ASSERT(sizeof(daNpc_Kk1_c) == 0x824);

class daNpc_Kk1_HIO_c : public mDoHIO_entry_c {
public:
    struct hio_prm_c {
        /* 0x00 */ s16 mMaxHeadX;
        /* 0x02 */ s16 mMaxHeadY;
        /* 0x04 */ s16 mMinHeadX;
        /* 0x06 */ s16 mMinHeadY;
        /* 0x08 */ s16 mMaxBackBoneX;
        /* 0x0A */ s16 mMaxBackBoneY;
        /* 0x0C */ s16 mMinBackBoneX;
        /* 0x0E */ s16 mMinBackBoneY;
        /* 0x10 */ s16 mMaxTurnStep;
        /* 0x12 */ s16 mTurnSpeed;
        /* 0x14 */ f32 mAttentionOffsetY;
        /* 0x18 */ u8 mDebug;
        /* 0x1A */ s16 m1A;
        /* 0x1C */ s16 m1C;
        /* 0x1E */ s16 m1E;
        /* 0x20 */ s16 m20;
        /* 0x22 */ s16 m22;
        /* 0x24 */ s16 m24;
        /* 0x26 */ s16 m26;
        /* 0x28 */ f32 m28;
        /* 0x2C */ f32 m2C;
        /* 0x30 */ f32 m30;
        /* 0x34 */ f32 m34;
        /* 0x38 */ f32 m38;
        /* 0x3C */ f32 m3C;
        /* 0x40 */ f32 m40;
        /* 0x44 */ f32 m44;
        /* 0x48 */ f32 m48;
        /* 0x4C */ f32 m4C;
        /* 0x50 */ f32 m50;
    };  // Size: 0x54

    daNpc_Kk1_HIO_c();
    inline virtual ~daNpc_Kk1_HIO_c();

public:
    /* 0x04 */ s8 mNo;
    /* 0x08 */ s32 mCount;
    /* 0x0C */ hio_prm_c mPrm;
};  // Size: 0x60

STATIC_ASSERT(sizeof(daNpc_Kk1_HIO_c) == DEMO_SELECT(0x64, 0x60));

#endif /* D_A_NPC_KK1_H */
