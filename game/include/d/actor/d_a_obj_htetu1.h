#ifndef D_A_OBJ_HTETU1_H
#define D_A_OBJ_HTETU1_H

#include "f_op/f_op_actor.h"
#include "d/d_particle.h"
#include "d/d_bg_w.h"

class daObjHtetu1Splash_c {
public:
    bool chk_stop() { return !mPlaying; }
    void delete_s() {
#if VERSION == VERSION_DEMO
        if (mpEmitter != NULL) {
            mSplashCb.remove();
            mpEmitter = NULL;
            mPlaying = false;
        }
#else
        if (mSplashCb.getEmitter() != NULL) {
            mSplashCb.remove();
            mPlaying = false;
        }
#endif
    }
    s16 get_timer() { return mTimer; }
    void play_particle() {
#if VERSION == VERSION_DEMO
        if (mpEmitter != NULL) {
            mpEmitter->playCreateParticle();
            mPlaying = true;
        }
#else
        if (mSplashCb.getEmitter() != NULL) {
            mSplashCb.getEmitter()->playCreateParticle();
            mPlaying = true;
        }
#endif
    }
    void set_pos_y(float y) { mPosition.y = y; }
    void stop_particle() {
#if VERSION == VERSION_DEMO
        if (mpEmitter != NULL) {
            mpEmitter->stopCreateParticle();
            mPlaying = false;
        }
#else
        if (mSplashCb.getEmitter() != NULL) {
            mSplashCb.getEmitter()->stopCreateParticle();
            mPlaying = false;
        }
#endif
    }
    void sub_timer() { mTimer--; }
    void timer_play_particle(short timer) {
#if VERSION == VERSION_DEMO
        if (mpEmitter != NULL) {
            mpEmitter->playCreateParticle();
            mPlaying = true;
            mTimer = timer;
        }
#else
        if (mSplashCb.getEmitter() != NULL) {
            mSplashCb.getEmitter()->playCreateParticle();
            mPlaying = true;
            mTimer = timer;
        }
#endif
    }
    void create_s(unsigned short, cXyz*, csXyz*, dKy_tevstr_c*);

#if VERSION == VERSION_DEMO
    /* 0x00 */ JPABaseEmitter* mpEmitter;
#endif
    /* 0x00 */ dPa_followEcallBack mSplashCb;
    /* 0x14 */ cXyz mPosition;
    /* 0x20 */ csXyz mAngle;
    /* 0x26 */ s16 mTimer;
    /* 0x28 */ bool mPlaying;
};

class daObjHtetu1_c : public fopAc_ac_c {
public:
    BOOL check_sw();
    static BOOL solidHeapCB(fopAc_ac_c*);
    BOOL create_heap();
    cPhs_State _create();
    bool _delete();
    void init_mtx();
    void unlock();
    f32 get_water_h();
    void splash_manager();
    bool _execute();
    bool _draw();
    static const char M_arcname[];

public:
    /* 0x290 */ J3DModel* mpModel;
    /* 0x294 */ request_of_phase_process_class mPhase;
    /* 0x29C */ s32 mSwitch;
    /* 0x2A0 */ cXyz mNextPos;
    /* 0x2AC */ cXyz mShakeOffset;
    /* 0x2B8 */ f32 mBottomY;
    /* 0x2BC */ f32 mShakeAmplitude;
    /* 0x2C0 */ u16 mShakeTimer;
    /* 0x2C2 */ u8 mMoveState;
    /* 0x2C4 */ s16 mEventIdx;
    /* 0x2C6 */ u8 mEventState;
    /* 0x2C8 */ s16 mQuakeTimer;
    /* 0x2CC */ dBgW* mpBgW;
    /* 0x2D0 */ daObjHtetu1Splash_c mSplash[2];
};

#endif /* D_A_OBJ_HTETU1_H */
