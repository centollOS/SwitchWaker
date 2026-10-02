#ifndef D_A_OBJ_MAGMAROCK_H
#define D_A_OBJ_MAGMAROCK_H

#include "f_op/f_op_actor.h"
#include "d/d_bg_w.h"
#include "m_Do/m_Do_ext.h"
#include "JSystem/JParticle/JPAEmitter.h"

class dBgW;

namespace daObjMagmarock {
    static void ride_call_back(dBgW*, fopAc_ac_c*, fopAc_ac_c*);
    static BOOL CheckCreateHeap(fopAc_ac_c*);
    
    class Act_c : public fopAc_ac_c {
    public:
        void MeltDownRequest() {}
        inline cPhs_State _create();
        inline bool _delete();
        inline bool _draw();
        inline bool _execute();
        BOOL checkProcess(void (Act_c::*proc)()) { return (mProcess == proc) ? 1 : 0; }
        void setProcess(void (Act_c::*proc)()) { mProcess = proc; }
    
        void set_mtx();
        void demo_move();
        void ControlEffect();
        void play_anim();
        void appear_proc_init();
        void appear_proc();
        void wait_proc_init();
        void wait_proc();
        void stay_proc_init();
        void stay_proc();
        void quake_proc_init();
        void quake_proc();
        void vanish_proc_init();
        void vanish_proc();
        BOOL CreateHeap();
        BOOL CreateInit();
        virtual BOOL LiftUpRequest(cXyz&);
        virtual BOOL BeforeLiftRequest(cXyz&);
        void calc_ground_quat();
    
    public:
        static const char M_arcname[];
        static GXColor default_color;
        /* 0x294 */ u32 mUnused294;
        /* 0x298 */ s16 mRideAngle;
        /* 0x29A */ s16 mUnused29A;
        /* 0x29C */ s16 mRidden;
        /* 0x29E */ u8 mHasRider;
        /* 0x29F */ u8 mType;
        /* 0x2A0 */ JPABaseEmitter* mpSmoke0;
        /* 0x2A4 */ JPABaseEmitter* mpSmoke1;
        /* 0x2A8 */ JPABaseEmitter* mpBeforeLiftEffect;
        /* 0x2AC */ JPABaseEmitter* mpLiftEffect;
        /* 0x2B0 */ Quaternion mRideQuat;
        /* 0x2C0 */ Quaternion mRideTargetQuat;
        /* 0x2D0 */ Quaternion mGroundQuat;
        /* 0x2E0 */ void (Act_c::*mProcess)();
        /* 0x2EC */ request_of_phase_process_class mPhase;
        /* 0x2F4 */ J3DModel* mpModel;
        /* 0x2F8 */ J3DAnmTevRegKey* M_brk;
        /* 0x2FC */ mDoExt_brkAnm mBrk;
        /* 0x314 */ J3DAnmTransform* M_bck;
        /* 0x318 */ mDoExt_bckAnm mBck;
        /* 0x328 */ Mtx mBgMtx;
        /* 0x358 */ dBgW* mpBgW;
        /* 0x35C */ dKy_tevstr_c mEffectTev;
        /* 0x40C */ cXyz mGroundPoints[3];
        /* 0x430 */ f32 mQuakeAmplitude;
        /* 0x434 */ f32 mBckFrame;
        /* 0x438 */ f32 mBrkFrame;
        /* 0x43C */ cXyz mLiftTarget;
        /* 0x448 */ s32 mTimer;
        /* 0x44C */ s32 mAnimTimer;
        /* 0x450 */ s32 mUnused450;
        /* 0x454 */ s16 mQuakeAngle;
        /* 0x456 */ s16 mQuakeSpeed;
        /* 0x458 */ s16 mUnused458;
        /* 0x45A */ s16 mDemoState;
        /* 0x45C */ s16 mLiftRequested;
        /* 0x45E */ s16 mBeforeLiftRequested;
    };
    
    namespace Method {
        cPhs_State Create(void*);
        BOOL Delete(void*);
        BOOL Execute(void*);
        BOOL Draw(void*);
        BOOL IsDelete(void*);
        extern actor_method_class Table;
    };
};

STATIC_ASSERT(sizeof(daObjMagmarock::Act_c) == 0x460);

#endif /* D_A_OBJ_MAGMAROCK_H */
