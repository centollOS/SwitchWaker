#ifndef D_MINIGAME_TERMINATER_H
#define D_MINIGAME_TERMINATER_H

#include "d/d_drawlist.h"
#include "f_op/f_op_msg.h"
#include "f_op/f_op_msg_mng.h"

class JKRArchive;
class J2DPane;
class J2DScreen;
struct fopMsgM_pane_class;

class dDlst_TerminaterScrnDraw_c : public dDlst_base_c {
public:
    dDlst_TerminaterScrnDraw_c() {
        mAnimTimer1 = 0;
        mAnimTimer2 = 0;
        mAnimTimer3 = 0;
    }

    f32 acc(s16 param_0, s16 param_1, s16 param_2) {
        return SQUARE((f32)(param_1 - param_2)) / SQUARE((f32)(param_0 - param_2));
    }

    void setScreen(const char*, JKRArchive*);
    void setScrnFailed();
    void setScrnSuccess(int, int);
    void changeNumberTexture(J2DPane*, int);
    const char* getNumber(int);
    void setAlphaFailed();
    void setAlphaSuccess();
    void getRotate(fopMsgM_pane_class*);
    void setRotate(fopMsgM_pane_class*, f32);
    void setScale(fopMsgM_pane_class*, f32);
    BOOL animeF1(int);
    BOOL animeF2(int);
    BOOL animeF3(int);
    BOOL animeS1();
    BOOL animeS2(fopMsgM_pane_class*);
    BOOL animeS3();
    BOOL animeFailed1();
    BOOL animeFailed2();
    BOOL animeSuccess();
    BOOL animePerfect();

    virtual ~dDlst_TerminaterScrnDraw_c() {}
    virtual void draw();

public:
    /* 0x004 */ J2DScreen* mpScrn;
#if VERSION <= VERSION_JPN
    /* 0x008 */ fopMsgM_pane_class mPerfect[8];
    /* 0x1C8 */ fopMsgM_pane_class mPerfectNt[10];
    /* 0x3F8 */ fopMsgM_pane_class mPerfectNk[10];
    /* 0x628 */ fopMsgM_pane_class mDmpe;
    /* 0x660 */ fopMsgM_pane_class mFailed[9];
    /* 0x858 */ fopMsgM_pane_class mYouGot[6];
    /* 0x9A8 */ fopMsgM_pane_class mRemain[13];
    /* 0xC80 */ fopMsgM_pane_class mRemainK[13];
    /* 0xF58 */ fopMsgM_pane_class mNum[8];
    /* 0x1118 */ fopMsgM_pane_class mNumK[8];
    /* 0x12D8 */ fopMsgM_pane_class mRpx;
    /* 0x1310 */ fopMsgM_pane_class mRpxk;
    /* 0x1348 */ fopMsgM_pane_class mNt00;
    /* 0x1380 */ fopMsgM_pane_class mNk00;
    /* 0x13B8 */ fopMsgM_pane_class mDummy;
    /* 0x13F0 */ J2DPane* mRupeePane[6];
    /* 0x1408 */ s16 mAnimTimer1;
    /* 0x140A */ s16 mAnimTimer2;
    /* 0x140C */ s16 mAnimTimer3;
#else
    /* 0x008 */ fopMsgM_pane_class mPerfect[10];
    /* 0x238 */ fopMsgM_pane_class mPerfectNt[10];
    /* 0x468 */ fopMsgM_pane_class mPerfectNk[10];
    /* 0x698 */ fopMsgM_pane_class mDmpe;
    /* 0x6D0 */ fopMsgM_pane_class mFailed[17];
    /* 0xA88 */ fopMsgM_pane_class mYouGot[13];
    /* 0xD60 */ fopMsgM_pane_class mRemain[17];
    /* 0x1118 */ fopMsgM_pane_class mRemainK[17];
    /* 0x14D0 */ fopMsgM_pane_class mNum[8];
    /* 0x1690 */ fopMsgM_pane_class mNumK[8];
    /* 0x1850 */ fopMsgM_pane_class mRpx;
    /* 0x1888 */ fopMsgM_pane_class mRpxk;
    /* 0x18C0 */ fopMsgM_pane_class mNt00;
    /* 0x18F8 */ fopMsgM_pane_class mNk00;
    /* 0x1930 */ fopMsgM_pane_class mDummy;
    /* 0x1968 */ J2DPane* mRupeePane[6];
    /* 0x1980 */ s16 mAnimTimer1;
    /* 0x1982 */ s16 mAnimTimer2;
    /* 0x1984 */ s16 mAnimTimer3;
#endif
};  // Size: JPN 0x1410, others 0x1988

STATIC_ASSERT(sizeof(dDlst_TerminaterScrnDraw_c) == VERSION_SELECT(0x1410, 0x1410, 0x1988, 0x1988));

class dMinigame_Terminater_c : public msg_class {
public:
    int getResultRupee() { return mResultRupee; }
    int getResultTime() { return mResultTime; }
    void setResultRupee(int rupee) { mResultRupee = rupee; }
    void setResultTime(int time) { mResultTime = time; }

    cPhs_State _create();
    BOOL _execute();
    BOOL _draw();
    BOOL _delete();

    /* 0x0FC */ dDlst_TerminaterScrnDraw_c* mScrn;
    /* 0x100 */ request_of_phase_process_class mPhs;
    /* 0x108 */ JKRSolidHeap* mHeap;
    /* 0x10C */ u32 mResultTime;
    /* 0x110 */ u32 mResultRupee;
    /* 0x114 */ s16 mTimer;
    /* 0x116 */ u16 field_0x116;
    /* 0x118 */ s16 mFinishType;
    /* 0x11A */ s16 field_0x11a;
    /* 0x11C */ u8 mStatus;
};  // Size: 0x120

STATIC_ASSERT(sizeof(dMinigame_Terminater_c) == 0x120);

#endif /* D_MINIGAME_TERMINATER_H */
