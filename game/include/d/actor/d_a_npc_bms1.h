#ifndef D_A_NPC_BMS1_H
#define D_A_NPC_BMS1_H

#include "f_op/f_op_actor.h"
#include "d/d_npc.h"
#include "d/d_shop.h"
#include "d/d_lib.h"
#include "m_Do/m_Do_hostIO.h"

class daNpc_Bms1_c : public fopAc_ac_c {
public:
    typedef int (daNpc_Bms1_c::*ActionFunc)(void*);
    s8 getBackboneJntNum() { return m_backbone_jnt_num; }
    s16 getBackbone_x() { return mJnt.getBackbone_x(); }
    s16 getBackbone_y() { return mJnt.getBackbone_y(); }
    s8 getHairLJntNum() { return m_hairL_jnt_num; }
    s8 getHairRJntNum() { return m_hairR_jnt_num; }
    s8 getHeadJntNum() { return m_head_jnt_num; }
    s16 getHead_x() { return mJnt.getHead_x(); }
    s16 getHead_y() { return mJnt.getHead_y(); }
    void setAction(ActionFunc action, void* arg) {
        if (mAction != action) {
            if (mAction) { mActionState = -1; (this->*mAction)(arg); }
            mAction = action; mActionState = 0; (this->*mAction)(arg);
        }
    }
    void setAttentionBasePos(cXyz pos) { mAttentionBasePos = pos; }
    void setEyePos(cXyz pos) { eyePos = pos; }

    void set_mtx();
    BOOL initTexPatternAnm(bool);
    void playTexPatternAnm();
    void setAnm(signed char, float);
    void setTexAnm(signed char);
    void setAnmFromMsgTag();
    bool chkAttention(cXyz, short);
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
    BOOL checkPlayerLanding();
    void lookBack();
    u8 wait01();
    u8 talk01();
    int getdemo_action(void*);
    int wait_action(void*);
    int event_action(void*);
    BOOL evn_talk_init(int);
    BOOL evn_continue_talk_init(int);
    BOOL evn_talk();
    BOOL evn_viblation_init(int);
    BOOL evn_head_swing_init(int);
    BOOL privateCut();
    BOOL demo_move();
    void demo_end_init();
    BOOL _draw();
    BOOL _execute();
    BOOL _delete();
    cPhs_State _create();
    BOOL CreateHeap();

public:
    static char m_arcname[4];
    dNpc_HeadAnm_c mHeadAnm;
    cXyz mHairScaleL, mHairScaleR;
    f32 mHairStretchL, mHairStretchR;
    cXyz mHairPosL, mHairPosR, mHairVelocityL, mHairVelocityR;
    Quaternion mHairQuatL, mHairQuatR;
    request_of_phase_process_class mPhs;
    mDoExt_McaMorf* mpMorf;
    u32 mShadowId;
    J3DAnmTexPattern* m_head_tex_pattern;
    mDoExt_btpAnm mBtpAnm;
    u8 mBtpFrame;
    s16 mBtpTimer;
    J3DModel *mpHeadModel, *mpModel354, *mpModel358, *mpLegModel, *mpModel360;
    dBgS_ObjAcch mAcch;
    dBgS_AcchCir mAcchCir;
    dCcD_Stts mStts;
    dCcD_Cyl mCyl;
    s8 m_head_jnt_num, m_backbone_jnt_num, m_hairL_jnt_num, m_hairR_jnt_num, m_leg_jnt_num;
    u8 m6D9[3];
    dNpc_JntCtrl_c mJnt;
    dNpc_EventCut_c mEventCut;
    STControl mStick;
    s8 mItemChanged;
    cXyz m7A8, mAttentionBasePos;
    s16 mTurnSpeed;
    csXyz mInitialAngle;
    s16 m7C8;
    u8 mAnmEnd, mAttnState;
    bool mHasAttention, mTalking;
    u8 m7CE[2];
    f32 mPreviousFrame, mPlayerHeight;
    u32 mMsgNo, mNextMsgNo, mEndMsgNo, mOverrideMsgNo;
    u8 mItemNo, mDemo;
    ActionFunc mAction;
    ShopCam_action_c mShopCam;
    ShopItems_c mShopItems;
    ShopCursor_c* mpShopCursor;
    s8 mBtpNo, mAnmNo, mAnmTimer, mOrder, mState, mPreviousState, mType, m89F, mActionState, mTalkState;
    u8 m8A2, mEventItem, m8A4, mSkipped;

};

#endif
