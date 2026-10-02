#ifndef D_A_OBJ_SWFLAT_H
#define D_A_OBJ_SWFLAT_H

#include "d/d_bg_s_movebg_actor.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"

namespace daObjSwflat {
    class Act_c : public dBgS_MoveBgActor {
    public:
        enum Prm_e {
            PRM_TYPE_W = 2, PRM_TYPE_S = 0,
            PRM_SW_W = 8, PRM_SW_S = 8,
            PRM_SW2_W = 8, PRM_SW2_S = 16,
        };
        s32 prmGetSwNo() const { return daObj::PrmAbstract(this, PRM_SW_W, PRM_SW_S); }
        s32 prmGetSwNo2() const { return daObj::PrmAbstract(this, PRM_SW2_W, PRM_SW2_S); }
        s32 prmGetType() const { return daObj::PrmAbstract(this, PRM_TYPE_W, PRM_TYPE_S); }
        static const char M_arcname[];
        static Mtx M_tmp_mtx;
    
        virtual BOOL CreateHeap();
        virtual BOOL Create();
        cPhs_State Mthd_Create();
        virtual BOOL Delete();
        BOOL Mthd_Delete();
        void set_mtx();
        void init_mtx();
        virtual BOOL Execute(Mtx**);
        virtual BOOL Draw();
        virtual BOOL IsDelete();
    
    public:
        /* 0x2C8 */ request_of_phase_process_class mPhase;
        /* 0x2D0 */ J3DModel* mpModel;
        /* 0x2D4 */ mDoExt_brkAnm* mBrk;
        /* 0x2D8 */ dCcD_Stts mStts;
        /* 0x314 */ dCcD_Cyl mCyl;
        /* 0x444 */ JPABaseEmitter* mpOffEmitter;
        /* 0x448 */ JPABaseEmitter* mpOnEmitter;
        /* 0x44C */ cXyz mParticlePos;
        /* 0x458 */ s16 mFrame;
        /* 0x45A */ s16 mActiveFrame;
        /* 0x45C */ s16 mEventTimer;
        /* 0x45E */ s16 mFinishTimer;
        /* 0x460 */ u8 mType;
        /* 0x461 */ u8 mActive;
        /* 0x462 */ u8 mPrevActive;
        /* 0x463 */ u8 mSwitch;
        /* 0x464 */ u8 mSwitch2;
        /* 0x465 */ u8 mOffAlpha;
        /* 0x466 */ u8 mOnAlpha;
        /* 0x467 */ u8 mFinished;
        /* 0x468 */ u8 mEventState;
    };
};


#endif /* D_A_OBJ_SWFLAT_H */
