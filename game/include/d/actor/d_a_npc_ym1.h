#ifndef D_A_NPC_YM1_H
#define D_A_NPC_YM1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"
#include "d/d_cc_d.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"

class daNpc_Ym1_c : public fopNpc_npc_c {
public:
    struct anm_prm_c {
        s8 mAnm, mBtp;
        f32 mMorf, mSpeed;
        int mLoop;
    };
    typedef int (daNpc_Ym1_c::*ActionFunc)(void*);

    void setKariFlg();
    void _nodeCB_Head(J3DNode*, J3DModel*);
    void _nodeCB_BackBone(J3DNode*, J3DModel*);
    bool init_YM1_0();
    bool init_YM1_1();
    bool init_YM2_0();
    bool init_YM2_1();
    bool init_YM2_2();
    bool init_YM2_3();
    bool init_YMx_error();
    bool createInit();
    void play_animation();
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
    u32 getMsg_YM1_0();
    u32 getMsg_YM1_1();
    u32 getMsg_YM2_0();
    u32 getMsg_YM2_1();
    u32 getMsg_YM2_2();
    u32 getMsg_YM2_3();
    virtual u32 getMsg();
    void eventOrder();
    void checkOrder();
    void set_collision_sp();
    void set_cutGrass();
    bool chk_BlackPig();
    bool chk_nbt_attn();
    bool chk_talk();
    bool chk_parts_notMov();
    void lookBack();
    bool chkAttention();
    void setAttention(bool);
    bool decideType(int);
    void privateCut(int);
    void endEvent();
    int isEventEntry();
    void event_proc(int);
    bool set_action(int (daNpc_Ym1_c::*)(void*), void*);
    void setStt(signed char);
    bool chk_areaIN(float, cXyz);
    int kari_1();
    int wait_1();
    int wait_2();
    int talk_1();
    int turn_1();
    int NBTwai();
    int SITwai();
    int wait_action1(void*);
    int wait_action2(void*);
    int wait_action3(void*);
    int wait_action4(void*);
    int demo_action1(void*);
    u8 demo();
    void shadowDraw();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    int bodyCreateHeap();
    int headCreateHeap();
    int itemCreateHeap();
    int CreateHeap();

public:
    /* 0x6C4 */ request_of_phase_process_class mPhs;
    /* 0x6CC */ s8 m_hed_jnt_num;
    /* 0x6CD */ s8 m_bbone_jnt_num;
    /* 0x6CE */ s8 m_hnd_L_jnt_num;
    /* 0x6CF */ s8 m_hnd_R_jnt_num;
    /* 0x6D0 */ J3DModel* m6D0;
    /* 0x6D4 */ char mArcName[4];
    /* 0x6D8 */ u32 mShadow;
    /* 0x6DC */ J3DModel* mpHeadModel;
    /* 0x6E0 */ mDoExt_btpAnm mBtpAnm;
    /* 0x6F4 */ u8 mTexFrame;
    /* 0x6F6 */ s16 mBlinkTimer;
    /* 0x6F8 */ ActionFunc mAction;
    /* 0x704 */ dCcD_Cyl mCyl;
    /* 0x834 */ cXyz mInitialPos;
    /* 0x840 */ s16 mInitialAngle;
    /* 0x842 */ s16 mRotYTarget;
    /* 0x844 */ s16 m844;
    /* 0x846 */ csXyz m846;
    /* 0x84C */ cXyz mEyePos;
    /* 0x858 */ cXyz mLookPos;
    /* 0x864 */ cXyz mAttentionPos;
    /* 0x870 */ cXyz mHeadPos;
    /* 0x87C */ f32 mFrame;
    /* 0x880 */ s16 mOldAngle;
    /* 0x882 */ s16 mOldHead;
    /* 0x884 */ s16 mOldBackbone;
    /* 0x886 */ s16 m886;
    /* 0x888 */ int m888;
    /* 0x88C */ s16 m88C;
    /* 0x88E */ s16 m88E;
    /* 0x890 */ s16 mKariTimer;
    /* 0x892 */ s16 m892;
    /* 0x894 */ s16 m894;
    /* 0x896 */ s16 mLookAngle;
    /* 0x898 */ s8 mAnmEnd;
    /* 0x899 */ s8 mAnmLoops;
    /* 0x89A */ u8 mItem;
    /* 0x89B */ u8 mKariFlag;
    /* 0x89C */ u8 m89C;
    /* 0x89D */ bool m89D;
    /* 0x89E */ u8 m89E;
    /* 0x89F */ u8 m89F;
    /* 0x8A0 */ u8 m8A0;
    /* 0x8A1 */ u8 m8A1;
    /* 0x8A2 */ u8 m8A2;
    /* 0x8A3 */ u8 m8A3;
    /* 0x8A4 */ bool m8A4;
    /* 0x8A5 */ u8 m8A5;
    /* 0x8A6 */ u8 m8A6;
    /* 0x8A7 */ s8 mCut;
    /* 0x8A8 */ u8 mAnmAttr;
    /* 0x8A9 */ u8 mAnmTag;
    /* 0x8AA */ s8 mBtpNo;
    /* 0x8AB */ s8 m8AB;
    /* 0x8AC */ s8 mOrder;
    /* 0x8AD */ s8 m8AD;
    /* 0x8AE */ s8 mPreviousState;
    /* 0x8AF */ s8 m8AF;
    /* 0x8B0 */ s8 mSubType;
    /* 0x8B1 */ s8 mStaff;
    /* 0x8B2 */ s8 mActionState;
    /* 0x8B3 */ s8 mMsgAnmStarted;

};

STATIC_ASSERT(sizeof(daNpc_Ym1_c) == 0x8B4);

struct Ym1Prm {
    s16 maxHeadX, maxHeadY, minHeadX, minHeadY;
    s16 maxBackX, maxBackY, minBackX, minBackY;
    s16 maxTurn, turnSpeed;
    f32 attentionHeight;
    u8 debug;
    f32 areaRadius, kariMorf;
};
class daNpc_Ym1_childHIO_c : public mDoHIO_entry_c {
public:
    daNpc_Ym1_childHIO_c();
    inline virtual ~daNpc_Ym1_childHIO_c();
    Ym1Prm mPrm;
    int mIndex;
};
class daNpc_Ym1_HIO_c : public mDoHIO_entry_c {
public:
    daNpc_Ym1_HIO_c();
    inline virtual ~daNpc_Ym1_HIO_c();
    s8 mNo;
    int mSelected;
    daNpc_Ym1_childHIO_c mChild[2];
};
#endif
