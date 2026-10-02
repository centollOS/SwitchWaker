#ifndef D_A_NPC_DS1_H
#define D_A_NPC_DS1_H

#include "d/d_npc.h"
#include "d/d_lib.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_hostIO.h"
#include "d/d_shop.h"

class daNpc_Ds1_c : public fopAc_ac_c {
public:
    typedef int (daNpc_Ds1_c::*ActionFunc)(void*);
    static char m_arcname[];
    bool checkAction(ActionFunc action) {
        return mAction == action;
    }
    s8 getBackboneJntNum() {
        return m_backbone_jnt_num;
    }
    s16 getBackbone_x() {
        return mJnt.getBackbone_x();
    }
    s16 getBackbone_y() {
        return mJnt.getBackbone_y();
    }
    int getHeadJntNum() {
        return m_head_jnt_num;
    }
    s16 getHead_x() {
        return mJnt.getHead_x();
    }
    s16 getHead_y() {
        return mJnt.getHead_y();
    }
    void incAttnSetCount() {
        if (mAttnSetCount != 255) {
            mAttnSetCount++;
        }
    }
    void setAction(ActionFunc action, void* arg) {
        if (mAction != action) {
            if (mAction) {
                mActionState = -1;
                (this->*mAction)(NULL);
            }
            mAction = action;
            mActionState = 0;
            (this->*mAction)(arg);
        }
    }
    void setAttentionBasePos(cXyz pos) {
        mAttentionBasePos = pos;
    }
    void setEyePos(cXyz pos) {
        eyePos = pos;
    }
    s16 XyEventCB(int);
    BOOL initTexPatternAnm(bool);
    void playTexPatternAnm();
    void setAnm(signed char, float);
    void setTexAnm(signed char);
    void setAnmFromMsgTag();
    BOOL chkAttention(cXyz, short);
    void eventOrder();
    void checkOrder();
    u16 next_msgStatus(unsigned long*);
    u32 getMsg();
    void setCollision();
    void talkInit();
    u16 normal_talk();
    u16 shop_talk();
    u16 talk();
    BOOL CreateInit();
    void setAttention(bool);
    void lookBack();
    bool wait01();
    bool talk01();
    int getdemo_action(void*);
    int createdemo_action(void*);
    int privateCut();
    int evn_setAnm_init(int);
    int evn_Anm();
    int evn_talk_init(int);
    int evn_continue_talk_init(int);
    int evn_talk();
    int evn_jnt_lock_init(int);
    int evn_player_hide_init(int);
    int evn_head_swing_init(int);
    int evn_ItemModel_init(int);
    int evn_move_pos_init(int);
    int evn_move_pos();
    int evn_init_pos_init(int);
    int event_action(void*);
    int wait_action(void*);
    int dummy_action(void*);
    void RoomEffectSet();
    void RoomEffectDelete();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL CreateHeap();
public:
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ mDoExt_McaMorf* mpMorf;
    /* 0x29C */ u32 mShadow;
    /* 0x2A0 */ u32 m2A0;
    /* 0x2A4 */ J3DAnmTexPattern* m_head_tex_pattern;
    /* 0x2A8 */ mDoExt_btpAnm mBtp;
    /* 0x2BC */ u8 mTexFrame;
    /* 0x2BE */ s16 mBlinkTimer;
    /* 0x2C0 */ J3DModel* mpItemL;
    /* 0x2C4 */ J3DModel* mpItemR;
    /* 0x2C8 */ dBgS_ObjAcch mAcch;
    /* 0x48C */ dBgS_AcchCir mAcchCir;
    /* 0x4CC */ dCcD_Stts mStts;
    /* 0x508 */ dCcD_Cyl mCyl;
    /* 0x638 */ s8 m_head_jnt_num;
    /* 0x639 */ s8 m_backbone_jnt_num;
    /* 0x63A */ s8 m_handL_jnt_num;
    /* 0x63B */ s8 m_handR_jnt_num;
    /* 0x63C */ dNpc_EventCut_c mEventCut;
    /* 0x6A8 */ dNpc_JntCtrl_c mJnt;
    /* 0x6DC */ dNpc_HeadAnm_c mHeadAnm;
    /* 0x700 */ cXyz mMovePos;
    /* 0x70C */ s16 mMoveAngle;
    /* 0x710 */ STControl mStick;
    /* 0x738 */ s8 m738;
    /* 0x739 */ u8 m739[15];
    /* 0x748 */ cXyz mAttentionBasePos;
    /* 0x754 */ s16 mTurnSpeed;
    /* 0x756 */ csXyz mInitialAngle;
    /* 0x75C */ u16 m75C;
    /* 0x75E */ u8 mAnmEnd;
    /* 0x75F */ u8 mAttnSetCount;
    /* 0x760 */ u8 mAttention;
    /* 0x761 */ u8 m761;
    /* 0x764 */ f32 mFrame;
    /* 0x768 */ JPABaseEmitter* mpEmitters[9];
    /* 0x78C */ J3DModel* mpRoomModel;
    /* 0x790 */ mDoExt_btkAnm mRoomBtk;
    /* 0x7A4 */ LIGHT_INFLUENCE mLight;
    /* 0x7C4 */ u32 mCurrMsg;
    u32 mNextMsg;
    u32 m7CC;
    /* 0x7D0 */ u32 m7D0;
    /* 0x7D4 */ u8 m7D4;
    /* 0x7D5 */ u8 mItemFlags;
    /* 0x7D6 */ u8 mRoomEffects;
    /* 0x7D7 */ u8 mSoundTimer;
    /* 0x7D8 */ ActionFunc mAction;
    /* 0x7E4 */ ShopCam_action_c mShopCam;
    /* 0x83C */ ShopItems_c mShopItems;
    /* 0x880 */ ShopCursor_c* mpShopCursor;
    /* 0x884 */ s8 mTexAnm;
    /* 0x885 */ s8 mAnm;
    /* 0x886 */ s8 mAnmCount;
    /* 0x887 */ s8 mOrder;
    /* 0x888 */ int mTalkMode;
    /* 0x88C */ s16 mEventIdx[4]; // CREATE_DRUG, GET_DRUG, PUT_ITEM, PUT_ITEM_FAIL (indexed by mTalkMode)
    /* 0x894 */ s8 mLookMode;
    /* 0x895 */ u8 m895;
    /* 0x896 */ s8 mType;
    /* 0x897 */ u8 m897;
    /* 0x898 */ s8 mActionState;
    /* 0x899 */ s8 m899;
    /* 0x89A */ u8 m89A;
    /* 0x89B */ u8 mJellyItem;
    /* 0x89C */ u32 m89C;
};

STATIC_ASSERT(sizeof(daNpc_Ds1_c) == 0x8A0);

class daNpc_Ds1_childHIO_c : public JORReflexible {
public:
    daNpc_Ds1_childHIO_c();
    inline virtual ~daNpc_Ds1_childHIO_c();
    dNpc_HIO_c mNpc;
    f32 mAttnWidth, mAttnDepth;
    f32 mCursorMin, mCursorMax, mCursorSpeed, mCursorY, mCursorZ;
};

class daNpc_Ds1_HIO_c : public JORReflexible {
public:
    daNpc_Ds1_HIO_c();
    inline virtual ~daNpc_Ds1_HIO_c();
    s8 mNo;
    int mSelected;
    daNpc_Ds1_childHIO_c mChild[1];
};

#endif
