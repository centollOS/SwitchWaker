#ifndef D_A_NPC_KG1_H
#define D_A_NPC_KG1_H

#include "d/d_npc.h"
#include "m_Do/m_Do_hostIO.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"

class daNpc_Kg1_c : public fopNpc_npc_c {
public:
    inline cPhs_State _create();
    inline bool _delete();
    inline bool _draw();
    inline bool _execute();
    void getAttentionBasePos() { attention_info.position.set(current.pos.x, current.pos.y + 190.0f, current.pos.z); }
    void getEyePos() { eyePos.set(current.pos.x, current.pos.y + 150.0f, current.pos.z); }
    void setAction(void (daNpc_Kg1_c::*action)()) { mAction = action; }
    cXyz* getHeadPosP() { return &mHeadPos; }
    cXyz* getLookPosP() { return &mLookPos; }

    void lookBack();
    BOOL chkAttention();
    BOOL initTexPatternAnm(int, bool);
    void playTexPatternAnm();
    void set_mtx();
    BOOL CreateHeap();
    cPhs_State CreateInit();
    void eventOrder();
    void checkOrder();
    void kg1_talk_camera();
    void wait_action_init();
    void wait_action();
    void clr_seq_flag();
    virtual u32 getMsg();
    virtual u16 next_msgStatus(u32*);
    virtual void anmAtr(u16);
    void setAnm();

    static const u8 m_highscore;
    static const char m_arcname[3];
    static const f32 m_camera_fovy;
    static cXyz m_camera_ctr;
    static cXyz m_camera_eye;

public:
    /* 0x6C4 */ J3DModel* m6C4;
    /* 0x6C8 */ mDoExt_btpAnm mBtpAnm;
    /* 0x6DC */ request_of_phase_process_class mPhs;
    /* 0x6E4 */ void (daNpc_Kg1_c::*mAction)();
    /* 0x6F0 */ u32 m6F0;
    /* 0x6F4 */ J3DAnmTexPattern* m_eye_tex_pattern;
    /* 0x6F8 */ mDoExt_btpAnm m6F8;
    /* 0x70C */ mDoExt_btpAnm m70C;
    /* 0x720 */ u8 m720;
    /* 0x721 */ u8 m721[3];
    /* 0x724 */ u32 mShadowId;
    /* 0x728 */ u32 m728;
    /* 0x72C */ s16 m72C;
    /* 0x72E */ s16 m72E;
    /* 0x730 */ u8 m730;
    /* 0x731 */ u8 m731;
    /* 0x732 */ u8 m732;
    /* 0x733 */ u8 m733;
    /* 0x734 */ cXyz mLookPos;
    /* 0x740 */ cXyz mHeadPos;
    /* 0x74C */ s8 mAnm;
    /* 0x74D */ s8 m74D;
    /* 0x74E */ s8 m74E;
    /* 0x74F */ u8 mDrawProp;
    /* 0x750 */ u8 m750;
    /* 0x751 */ u8 m751;
    /* 0x752 */ u8 m752[0x770 - 0x752];
    /* 0x770 */ u8 mMet;
    /* 0x771 */ u8 mExplained;
    /* 0x772 */ u8 mStartGame;
    /* 0x773 */ u8 mbDidClearGame;
    /* 0x774 */ u8 mbGameEnd;
    /* 0x775 */ u8 mPrizeGiven;
    /* 0x776 */ u8 mRecordPrizeGiven;
    /* 0x777 */ u8 mNewRecord;
    /* 0x778 */ u8 m778;
    /* 0x779 */ u8 m779;
    /* 0x77A */ u8 mGameBoardScore;
    /* 0x77B */ u8 mEndGameTimer;
    /* 0x77C */ fpc_ProcID mItemId;
    /* 0x780 */ int mWaitMode;
    /* 0x784 */ s16 m784;
    /* 0x786 */ s16 m786;
    /* 0x788 */ s16 m788;
    /* 0x78A */ u8 m78A[0x78C - 0x78A];
};

STATIC_ASSERT(sizeof(daNpc_Kg1_c) == 0x78C);

#endif /* D_A_NPC_KG1_H */
