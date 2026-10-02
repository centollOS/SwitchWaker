#ifndef D_A_NPC_KG2_H
#define D_A_NPC_KG2_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"
#include "JAZelAudio/JAIZelBasic.h"
#include "d/d_com_inf_game.h"

class J3DNode;

class daNpc_Kg2_c : public fopNpc_npc_c {
public:
    typedef int (daNpc_Kg2_c::*ActionFunc)(void*);
    void seStart(u32 sound) { mDoAud_seStart(sound, &eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(this))); }
    void setAction(ActionFunc action, void* arg) {
        if (mAction != action) {
            if (mAction) { mActionState = -1; (this->*mAction)(arg); }
            mAction = action; mActionState = 0; (this->*mAction)(arg);
        }
    }
    cXyz& getEyePos() { return mEyePos; }
    cXyz& getAttentionBasePos() { return mAttentionBasePos; }
    static void setCanonGameResult(u8 result) { canon_game_result = result; }
    static void init() { canon_game_result = 0; }
    static s8 canon_game_result;
    static daNpc_Kg2_c* l_kg2_pointer;
    void set_mtx();
    BOOL initTexPatternAnm(bool modify);
    void playTexPatternAnm();
    void setAnm(s8 anm, f32 morf);
    void subAnm();
    bool chkAttention(cXyz pos, s16 angle);
    void eventOrder();
    void checkOrder();
    u32 getMsg();
    u16 next_msgStatus(u32* msg);
    void anmAtr(u16 status);
    void setAttention();
    void lookBack();
    BOOL CreateHeap();
    BOOL CreateInit();
    void wait01();
    void talk01();
    BOOL evn_setAnm_init(int staff);
    BOOL evn_setAnm();
    BOOL evn_jnt_lock_init(int staff);
    BOOL evn_talk_init(int staff);
    BOOL evn_talk();
    BOOL evn_createItem_init(int staff);
    BOOL privateCut();
    BOOL processMove();
    int wait_action(void* arg);
    int event_wait_action(void* arg);
    cPhs_State _create();
    BOOL _delete();
    BOOL _execute();
    BOOL _draw();
public:
    /* 0x6C4 */ s8 m_handL_num;
    /* 0x6C8 */ request_of_phase_process_class mPhs;
    /* 0x6D0 */ u32 mShadowId;
    /* 0x6D4 */ J3DModel* mpPlateModel;
    /* 0x6D8 */ mDoExt_btpAnm mPlateBtp;
    /* 0x6EC */ J3DAnmTexPattern* m_btp;
    /* 0x6F0 */ mDoExt_btpAnm mBtpAnm;
    /* 0x704 */ u8 mBtpFrame;
    /* 0x706 */ s16 mBtpTimer;
    /* 0x708 */ cXyz mEyePos;
    /* 0x714 */ cXyz mAttentionBasePos;
    /* 0x720 */ s16 mTurnSpeed;
    /* 0x722 */ s16 m722;
    /* 0x724 */ csXyz mInitialAngle;
    /* 0x72A */ u8 m72A[2];
    /* 0x72C */ bool mHasAttention;
    /* 0x72D */ bool mTalking;
    /* 0x72E */ u8 m72E[6];
    /* 0x734 */ u8 mGreeted;
    /* 0x735 */ u8 mPlayed;
    /* 0x736 */ bool mDrawPlate;
    /* 0x737 */ u8 mPlateFrame;
    /* 0x738 */ u8 mSleeping;
    /* 0x739 */ s8 mAnmLoops;
    /* 0x73A */ u8 m73A[2];
    /* 0x73C */ ActionFunc mAction;
    /* 0x748 */ s8 mBtpNo;
    /* 0x749 */ s8 mAnmNo;
    /* 0x74C */ u32 mNextMsg;
    /* 0x750 */ s8 mOrder;
    /* 0x754 */ int mEventNo;
    /* 0x758 */ s16 mEventIdx[4];
    /* 0x760 */ s8 mState;
    /* 0x761 */ u8 m761[2];
    /* 0x763 */ s8 mActionState;
};

class daNpc_Kg2_HIO_c : public JORReflexible {
public:
    daNpc_Kg2_HIO_c();
    inline virtual ~daNpc_Kg2_HIO_c();
    void genMessage(JORMContext*) {}
    s8 mChild;
    dNpc_HIO_c mNpc;
    u8 m30;
};

#endif /* D_A_NPC_KG2_H */
