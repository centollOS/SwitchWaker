#ifndef D_A_KDDOOR_H
#define D_A_KDDOOR_H

#include "f_op/f_op_actor.h"
#include "d/d_door.h"
#include "d/d_cc_d.h"
#include "d/d_bg_w.h"

class dDoor_ssk_sub_c {
public:
    void init();
    void end();
    void openInit();
    BOOL openProc(dDoor_info_c*);
    void closeInit();
    BOOL closeProc(dDoor_info_c*);
    dDoor_ssk_sub_c();
    ~dDoor_ssk_sub_c() {}
    BOOL drawSet();
    void calcMtx(dDoor_info_c*, float, float, unsigned char);

public:
    /* 0x000 */ mDoExt_McaMorf* mpMorf;
    /* 0x004 */ mDoExt_McaMorf* mpMorf2;
    /* 0x008 */ dPa_smokeEcallBack mSmoke;
    /* 0x028 */ dCcD_Stts mStts;
    /* 0x064 */ dCcD_Cyl mCyl;
    /* 0x194 */ u8 mEnabled;
    /* 0x196 */ s16 mAngle;
    /* 0x198 */ s16 mAngleSpeed;
    /* 0x19A */ s16 mRotation;
    /* 0x19C */ s16 mDelay;
    /* 0x19E */ s16 mWaveSpeed;
    /* 0x1A0 */ cXyz mScale;
    /* 0x1AC */ cXyz mScale2;
    /* 0x1B8 */ cXyz mPos;
    /* 0x1C4 */ u8 mTriggered;
    /* 0x1C5 */ u8 mSoundTimer;
};

class dDoor_ssk_c {
public:
    void init(dDoor_info_c*);
    void end();
    void calcMtx(dDoor_info_c*);
    void execute(dDoor_info_c*);
    void draw(dDoor_info_c*);
    void closeInit();
    BOOL closeProc(dDoor_info_c*);
    void openInit();
    BOOL openProc(dDoor_info_c*);

public:
    /* 0x000 */ dDoor_ssk_sub_c mSub[3];
    /* 0x558 */ u8 mEnabled;
    /* 0x559 */ u8 mFrontCheck;
    /* 0x55A */ u8 mOtherEnabled;
    /* 0x55B */ u8 mProcessing;
    /* 0x55C */ dKy_tevstr_c mTevStr;
};

class daKddoor_c : public dDoor_info_c {
public:
    bool checkFlag(unsigned short flag) { return mFlags & flag; }
    inline BOOL execute();
    void offFlag(unsigned short flag) { mFlags &= ~flag; }
    void onFlag(unsigned short flag) { mFlags |= flag; }
    void setAction(u8 action) { mAction = action; }

    BOOL chkMakeKey();
    void setKey();
    BOOL chkMakeStop();
    s32 chkStopF();
    s32 chkStopB();
    void setStop();
    BOOL chkGenocideCase();
    BOOL chkFeelerCase();
    BOOL chkStopOpen();
    void setStopDemo();
    BOOL chkStopClose();
    const char* getBmdName();
    const char* getBmdName2();
    const char* getDzbName();
    f32 getSize2X();
    BOOL CreateHeap();
    void setEventPrm();
    void openInit();
    BOOL openProc();
    void openEnd();
    void closeInit();
    BOOL closeProc();
    void closeEnd();
    void calcMtx();
    BOOL CreateInit();
    cPhs_State create();
    void demoProc();
    BOOL draw();

public:
    static const char M_arcname[];

    /* 0x2D0 */ request_of_phase_process_class mPhase;
    /* 0x2D8 */ dDoor_smoke_c mSmoke;
    /* 0x310 */ dDoor_key2_c mKey;
    /* 0x334 */ dDoor_ssk_c mStop;
    /* 0x940 */ J3DModel* mpModel;
    /* 0x944 */ dBgW* mpBgW;
    /* 0x948 */ u8 mAction;
    /* 0x94A */ u16 mFlags;
    /* 0x94C */ f32 mOffsetY;
};

STATIC_ASSERT(sizeof(dDoor_ssk_sub_c) == 0x1C8);
STATIC_ASSERT(sizeof(daKddoor_c) == 0x950);

#endif /* D_A_KDDOOR_H */
