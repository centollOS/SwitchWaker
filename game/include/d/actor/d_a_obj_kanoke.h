#ifndef D_A_OBJ_KANOKE_H
#define D_A_OBJ_KANOKE_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"
#include "d/d_bg_w.h"

class daObjKanoke_c : public fopAc_ac_c {
public:
    enum Prm_e {
        PRM_TYPE_W = 1, PRM_TYPE_S = 0,
        PRM_SEARCH_W = 5, PRM_SEARCH_S = 1,
        PRM_YURE_W = 1, PRM_YURE_S = 6,
        PRM_SW_W = 8, PRM_SW_S = 8,
        PRM_SW2_W = 8, PRM_SW2_S = 16,
    };

    daObjKanoke_c();
    cPhs_State _create();
    BOOL createHeap();
    cPhs_State createInit();
    BOOL _delete();
    BOOL _draw();
    BOOL _execute();
    void executeNormal();
    void executeYureYoko();
    void executeOpenYoko();
    void executeEffectYoko();
    void executeYureTate();
    void executeOpenTate();
    void executeEffectTate();
    void executeWait();
    u8 getPrmType();
    u8 getPrmSearch();
    u8 getPrmYure();
    u8 getPrmSwNo();
    u8 getPrmSwNo2();
    void setMtx();
    void setMtxHontai();
    void setMtxHuta(cXyz*);

public:
    /* 0x290 */ request_of_phase_process_class mPhase;
    /* 0x298 */ J3DModel* mpBodyModel;
    /* 0x29C */ J3DModel* mpLidModel;
    /* 0x2A0 */ dBgW* mpBodyBgW;
    /* 0x2A4 */ dBgW* mpLidBgW;
    /* 0x2A8 */ Mtx mBodyMtx;
    /* 0x2D8 */ Mtx mLidMtx;
    /* 0x308 */ dCcD_Stts mStts;
    /* 0x344 */ dCcD_Cps mBodyCps;
    /* 0x47C */ dCcD_Cps mLidCps[3];
    /* 0x824 */ JPABaseEmitter* mpEmitter[2];
    /* 0x82C */ dPa_smokeEcallBack mSmoke;
    /* 0x84C */ cXyz mSmokePos;
    /* 0x858 */ csXyz mSmokeAngle;
    /* 0x860 */ cXyz mLidOffset;
    /* 0x86C */ cXyz mPivot;
    /* 0x878 */ f32 mAlpha;
    /* 0x87C */ csXyz mLidAngle;
    /* 0x882 */ s16 mAngularSpeed;
    /* 0x884 */ s16 mTimer;
    /* 0x886 */ s16 mShakeAmplitude;
    /* 0x888 */ s16 mLightTimer;
    /* 0x88A */ u8 mType;
    /* 0x88B */ u8 mMode;
    /* 0x88C */ u8 mSearch;
    /* 0x88D */ u8 mSwitch;
    /* 0x88E */ u8 mSwitch2;
    /* 0x88F */ u8 mHideFlags;
};

STATIC_ASSERT(sizeof(daObjKanoke_c) == 0x890);

#endif /* D_A_OBJ_KANOKE_H */
