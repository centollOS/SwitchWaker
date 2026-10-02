#ifndef D_A_NPC_BJ1_H
#define D_A_NPC_BJ1_H

#include "JSystem/JParticle/JPAEmitter.h"
#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"

class J3DNode;

class daNpc_Bj1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        /* 0x00 */ s8 mAnm;
        /* 0x01 */ s8 mBtp;
        /* 0x04 */ f32 mMorf;
        /* 0x08 */ f32 mSpeed;
        /* 0x0C */ int mLoop;
    };

    typedef int (daNpc_Bj1_c::*ActionFunc)(void*);

    void nodeBj1Control(J3DNode*, J3DModel*);
    void nodePrpControl(J3DNode*, J3DModel*);
    bool init_BJ4_0();
    bool init_BJ6_0();
    bool init_BJ7_0();
    bool init_BJX_0();
    bool init_BJX_1();
    bool createInit();
    void setMtx_anmProc();
    void setMtx(bool);
    int anmNum_toResID(int);
    BOOL setAnm_anm(anm_prm_c*);
    void setAnm_NUM(int);
    BOOL setAnm();
    void setAnm_prp(signed char);
    void chg_anmTag();
    void control_anmTag();
    void chg_anmAtr(unsigned char);
    void control_anmAtr();
    void setAnm_ATR();
    virtual void anmAtr(unsigned short);
    void eventOrder();
    void checkOrder();
    bool chk_talk();
    bool chk_drct(float);
    bool chk_partsNotMove();
    void lookBack();
    bool getMaskInf(unsigned char*);
    bool chkReg(unsigned short);
    void setReg(unsigned short);
    virtual u16 next_msgStatus(unsigned long*);
    u32 getMsg_BJ1_0();
    u32 getMsg_BJ2_0();
    u32 getMsg_BJ3_0();
    u32 getMsg_BJ4_0();
    u32 getMsg_BJ5_0();
    u32 getMsg_BJ6_0();
    u32 getMsg_BJ7_0();
    u32 getMsg_BJ8_0();
    u32 getMsg_BJ9_0();
    u32 getMsg_Corog();
    virtual u32 getMsg();
    bool chkAttention();
    void setAttention(bool);
    fopAc_ac_c* searchByID(fpc_ProcID);
    bool partner_srch_sub(void* (*)(void*, void*));
    void partner_srch();
    void setCollision_SP_();
    void set_pthPoint(unsigned char);
    void bj_clcFlySpd();
    s32 bj_movPass(bool);
    bool bj_flyMove();
    void bj_clcMovSpd();
    void bj_nMove();
    void setPrtcl_drugPot_1();
    void setPrtcl_drugPot_2();
    void delPrtcl_drugPot();
    void setPrtcl_danceLR();
    void flwPrtcl_danceLR();
    void delPrtcl_danceLR();
    void setPrtcl_peraProOpen();
    bool createSeed();
    bool deleteSeed();
    bool charDecide(int);
    void eInit_setLocFlag(int*);
    void eInit_setShapeAngleY(int*, short);
    void eInit_setEvTimer(int*);
    cXyz eInit_calcRelativPos(cXyz*, int*);
    f32 eInit_prmFloat(float*, float);
    void eInit_ATTENTION_(int*, int*, int*, cXyz*, int*, int*, int*);
    void eInit_PLYER_MOV_1_();
    void eInit_MOV_(float*, float*, float*, int*);
    void eInit_JMP_(float*, float*);
    void eInit_CHG_PTH_(int*, int*);
    void eInit_END_MOV_();
    void eInit_SET_TNE_();
    void eInit_DEL_TNE_();
    void eInit_SET_ANM_(int*, float*);
    void event_actionInit(int);
    bool eMove_ATTENTION_();
    bool eMove_MOV_();
    bool eMove_JMP_();
    bool eMove_SET_TNE_();
    bool eMove_PTH_MOV_();
    bool event_action();
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    BOOL set_action(ActionFunc, void*);
    void setStt(signed char);
    int wait_1();
    int wait_2();
    int wait_3();
    int wait_4();
    int flyMov();
    int fall01();
    int talk_1();
    int walk_1();
    int wait_action1(void*);
    int wait_action2(void*);
    int wait_action3(void*);
    int wait_action4(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    daNpc_Bj1_c();
    J3DModelData* create_Anm();
    J3DModelData* create_prp_Anm();
    bool create_itm_Mdl();
    BOOL CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhs;
    /* 0x6CC */ s8 m_hed_jnt_num;
    /* 0x6CD */ s8 m_bbone_jnt_num;
    /* 0x6CE */ s8 m_armL2_jnt_num;
    /* 0x6CF */ s8 m_armR2_jnt_num;
    /* 0x6D0 */ J3DModel* mpFaceModel;
    /* 0x6D4 */ J3DModel* mpPlantLModel;
    /* 0x6D8 */ J3DModel* mpPlantRModel;
    /* 0x6DC */ s8 m_plant_l_jnt_num;
    /* 0x6DD */ s8 m_plant_r_jnt_num;
    /* 0x6DE */ u8 pad6DE[0x6E0 - 0x6DE];
    /* 0x6E0 */ u32 mShadowId;
    /* 0x6E4 */ mDoExt_McaMorf* mpPrpMorf;
    /* 0x6E8 */ s8 m_cen_jnt_num;
    /* 0x6E9 */ s8 m_stalk_jnt_num;
    /* 0x6EA */ u8 pad6EA[0x6F0 - 0x6EA];
    /* 0x6F0 */ s16 mPrpRotY;
    /* 0x6F2 */ s16 mPrpRotSpd;
    /* 0x6F4 */ u8 pad6F4[0x6F8 - 0x6F4];
    /* 0x6F8 */ ActionFunc mAction;
    /* 0x704 */ Mtx mArmMtx;
    /* 0x734 */ dNpc_PathRun_c mPathRun;
    /* 0x73C */ dNpc_EventCut_c mEventCut;
    /* 0x7A8 */ fpc_ProcID mPartnerID;
    /* 0x7AC */ cXyz m7AC;
    /* 0x7B8 */ csXyz m7B8;
    /* 0x7BE */ u8 pad7BE[0x7C0 - 0x7BE];
    /* 0x7C0 */ cXyz m7C0;
    /* 0x7CC */ cXyz mHomePos;
    /* 0x7D8 */ cXyz m7D8;
    /* 0x7E4 */ cXyz m7E4;
    /* 0x7F0 */ cXyz mHeadScale;
    /* 0x7FC */ cXyz mBackScale;
    /* 0x808 */ f32 mFrame;
    /* 0x80C */ f32 m80C;
    /* 0x810 */ u8 pad810[0x814 - 0x810];
    /* 0x814 */ f32 m814;
    /* 0x818 */ u8 pad818[0x81C - 0x818];
    /* 0x81C */ f32 m81C;
    /* 0x820 */ s16 mPreviousHeadY;
    /* 0x822 */ s16 mPreviousBackboneY;
    /* 0x824 */ s16 mPreviousAngleY;
    /* 0x826 */ s16 mEventIdTable[3];
    /* 0x82C */ s16 mEventIdx;
    /* 0x82E */ s16 m82E;
    /* 0x830 */ s16 mEvTimer;
    /* 0x832 */ s16 m832;
    /* 0x834 */ s16 m834;
    /* 0x836 */ u8 pad836[0x838 - 0x836];
    /* 0x838 */ s16 mTurnSpeed;
    /* 0x83A */ s16 mLookAngle;
    /* 0x83C */ s8 mAnmEnd;
    /* 0x83D */ s8 mAnmLoops;
    /* 0x83E */ u8 mPresentItem;
    /* 0x83F */ u8 mPathIdx;
    /* 0x840 */ u8 m840;
    /* 0x841 */ u8 m841;
    /* 0x842 */ u8 m842;
    /* 0x843 */ u8 m843;
    /* 0x844 */ u8 mHidden;
    /* 0x845 */ u8 mNoDraw;
    /* 0x846 */ bool mKeepShapeAngle;
    /* 0x847 */ u8 mStopMove;
    /* 0x848 */ u8 m848;
    /* 0x849 */ bool m849;
    /* 0x84A */ u8 m84A;
    /* 0x84B */ u8 m84B;
    /* 0x84C */ u8 m84C;
    /* 0x84D */ u8 m84D;
    /* 0x84E */ u8 mInitialized;
    /* 0x84F */ u8 pad84F;
    /* 0x850 */ int m850;
    /* 0x854 */ u8 m854;
    /* 0x855 */ u8 mTalking;
    /* 0x856 */ bool mNoTurn;
    /* 0x857 */ u8 m857;
    /* 0x858 */ csXyz m858;
    /* 0x85E */ u8 pad85E[0x860 - 0x85E];
    /* 0x860 */ cXyz m860;
    /* 0x86C */ cXyz m86C;
    /* 0x878 */ JPABaseEmitter* mpPeraEmitter;
    /* 0x87C */ JPABaseEmitter* mpDrugPotEmitter[3];
    /* 0x888 */ JPABaseEmitter* m888;
    /* 0x88C */ JPABaseEmitter* mpDanceEmitterL;
    /* 0x890 */ JPABaseEmitter* mpDanceEmitterR;
    /* 0x894 */ s16 m894;
    /* 0x896 */ u8 pad896[0x898 - 0x896];
    /* 0x898 */ fpc_ProcID mSeedID[4];
    /* 0x8A8 */ s8 m8A8;
    /* 0x8A9 */ s8 m8A9;
    /* 0x8AA */ s8 m8AA;
    /* 0x8AB */ s8 mCutIdx;
    /* 0x8AC */ s8 mActNo;
    /* 0x8AD */ u8 mAnmAttr;
    /* 0x8AE */ u8 mAnmTag;
    /* 0x8AF */ s8 mAnmNo;
    /* 0x8B0 */ s8 m8B0;
    /* 0x8B1 */ s8 mState;
    /* 0x8B2 */ s8 mPreviousState;
    /* 0x8B3 */ s8 mLookMode;
    /* 0x8B4 */ s8 mType;
    /* 0x8B5 */ s8 mSubType;
    /* 0x8B6 */ s8 mActionState;
    /* 0x8B7 */ s8 mMsgAnmStarted;
};

STATIC_ASSERT(sizeof(daNpc_Bj1_c) == 0x8B8);

class daNpc_Bj1_childHIO_c : public mDoHIO_entry_c {
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
        /* 0x18 */ u8 m18;
        /* 0x1C */ f32 m1C;
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
        /* 0x40 */ s16 m40;
        /* 0x42 */ s16 m42;
        /* 0x44 */ s16 m44;
        /* 0x46 */ s16 m46;
        /* 0x48 */ f32 m48;
        /* 0x4C */ s16 m4C;
        /* 0x4E */ s16 m4E;
        /* 0x50 */ s16 m50;
        /* 0x52 */ s16 m52;
        /* 0x54 */ f32 m54;
        /* 0x58 */ f32 m58;
        /* 0x5C */ f32 m5C;
        /* 0x60 */ f32 m60;
    };  // Size: 0x64

    daNpc_Bj1_childHIO_c();
    inline virtual ~daNpc_Bj1_childHIO_c();

    /* 0x04 */ hio_prm_c mPrm;
    /* 0x68 */ int mIndex;
};

STATIC_ASSERT(sizeof(daNpc_Bj1_childHIO_c) == DEMO_SELECT(0x70, 0x6C));

class daNpc_Bj1_HIO_c : public mDoHIO_entry_c {
public:
    daNpc_Bj1_HIO_c();
    inline virtual ~daNpc_Bj1_HIO_c();

    /* 0x04 */ s8 mNo;
    /* 0x08 */ int mSelected;
    /* 0x0C */ daNpc_Bj1_childHIO_c mChild[9];
};

STATIC_ASSERT(sizeof(daNpc_Bj1_HIO_c) == DEMO_SELECT(0x400, 0x3D8));

#endif /* D_A_NPC_BJ1_H */
