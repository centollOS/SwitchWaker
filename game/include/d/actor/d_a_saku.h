#ifndef D_A_SAKU_H
#define D_A_SAKU_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"
#include "d/d_bg_w.h"
#include "m_Do/m_Do_ext.h"

class daSaku_c : public fopAc_ac_c {
public:
#if VERSION == VERSION_DEMO
    daSaku_c() : mSmoke(dust_color, NULL, 1) {}
#endif
    bool chkFlag(u8 flag) { return mFlags & flag; }
    void setFlag(u8 flag) { mFlags |= flag; }

    inline cPhs_State _daSaku_create();
    inline BOOL _daSaku_delete();
    inline BOOL _daSaku_draw();
    inline BOOL _daSaku_execute();
    inline BOOL _daSaku_isdelete();

    void CreateInit();
    BOOL saku_draw_sub(int);
    BOOL mode_break_none(int);
    BOOL mode_break_fire(int);
    BOOL mode_break_throw_obj(int);
    BOOL RecreateHeap(int, int);
    BOOL CreateHeap(int, int);
    int GetDzbId(int);
    BOOL CreateDummyHeap(int);
    BOOL loadMoveBG(int, int, int);
    BOOL loadModel(int, int, int);
    BOOL burn();
    BOOL broken(int);
    BOOL changeCollision(int);
    void setMtx();
    void setMoveBGMtx();
    void checkCol();
    void setCol();
    BOOL MoveBGResist(int, int);
    BOOL setEffFire(int);
    BOOL setEffBreak(int);

    static dCcD_SrcCyl m_cyl_src;
    static const dCcD_SrcCyl m_at_cyl_src;
    static const char* m_arcname[3];
    static u8 m_smoke_alpha;
    static int m_alpha_start_time;
    static int m_saku_alpha_out_time;
    static int m_fade_time;
    static GXColor dust_color;
    static const int m_max_particle_timer;
    static const u8 m_start_alpha;
    static const s16 m_alpha_spd;
    static const f32 m_saku_height;
    void deleteOriginalHeap(int saku_id) {
        if (m_heap[saku_id][0] && mHeapDeleteTimer[saku_id]) {
            mHeapDeleteTimer[saku_id]--;
            if (!mHeapDeleteTimer[saku_id]) {
                mDoExt_destroySolidHeap(m_heap[saku_id][0]);
                m_heap[saku_id][0] = NULL;
                mModel[saku_id][0] = NULL;
            }
        }
    }

public:
#if VERSION == VERSION_DEMO
    /* 0x290 */ dPa_smokeEcallBack mSmoke;
#else
    /* 0x290 */ dPa_smokeEcallBack mSmoke[2];
#endif
    /* 0x2D0 */ dCcD_Stts mStts;
    /* 0x30C */ dCcD_Cyl mCyl[2][3];
    /* 0xA2C */ cXyz mCylPos[2][3];
    /* 0xA74 */ dCcD_Cyl mAtCyl[3];
    /* 0xE04 */ request_of_phase_process_class mModelPhase;
    /* 0xE0C */ request_of_phase_process_class mCollisionPhase;
    /* 0xE14 */ JKRSolidHeap* m_heap[2][2];
    /* 0xE24 */ J3DModel* mModel[2][2];
    /* 0xE34 */ dBgW* mBgW[2][2];
    /* 0xE44 */ dBgW* mActiveBgW[2];
    /* 0xE4C */ Mtx mBgMtx[2];
#if VERSION == VERSION_DEMO
    /* 0xE8C */ JPABaseEmitter* mSmokeEmitter[2];
#else
    /* 0xEAC */ int mSmokeActive[2];
#endif
    /* 0xEB4 */ f32 mSmokeAlpha[2];
    /* 0xEBC */ int mParticleTimer[2];
    /* 0xEC4 */ cXyz mSmokePos[2];
    /* 0xEDC */ u8 mAlpha[2][2];
    /* 0xEE0 */ int mCollisionTimer[2];
    /* 0xEE8 */ int mFrame;
    /* 0xEEC */ int mFireTimer;
    /* 0xEF0 */ u8 mHeapDeleteTimer[2];
    /* 0xEF2 */ u8 mSturdinessType;
    /* 0xEF3 */ u8 mFlags;
    /* 0xEF4 */ u8 mBurning;
    /* 0xEF8 */ int mState[2];
    /* 0xF00 */ u32 mBottomHalfDestroyedSwitch;
    /* 0xF04 */ u32 mTopHalfDestroyedSwitch;
};

STATIC_ASSERT(sizeof(daSaku_c) == DEMO_SELECT(0xEE8, 0xF08));

#endif /* D_A_SAKU_H */
