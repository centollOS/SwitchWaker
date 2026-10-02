#ifndef D_A_OBJ_ASHUT_H
#define D_A_OBJ_ASHUT_H

#include "d/d_bg_s_movebg_actor.h"

namespace daObjAshut {
    class Act_c : public dBgS_MoveBgActor {
    public:
        enum Mode_e { Mode_UPPER, Mode_U_L, Mode_LOWER, Mode_L_U, Mode_DEMOREQ, Mode_NONE };
        enum Prm_e { PRM_SWSAVE_W = 8, PRM_SWSAVE_S = 0, PRM_EVID_W = 8, PRM_EVID_S = 8 };
    
        BOOL is_switch();
        u8 prm_get_evId() const { return daObj::PrmAbstract(this, PRM_EVID_W, PRM_EVID_S); }
        s32 prm_get_swSave() const { return daObj::PrmAbstract(this, PRM_SWSAVE_W, PRM_SWSAVE_S); }
    
        virtual BOOL CreateHeap();
        virtual BOOL Create();
        cPhs_State Mthd_Create();
        virtual BOOL Delete();
        BOOL Mthd_Delete();
        void set_mtx();
        void init_mtx();
        bool chk_safe_area() const;
        void mode_upper_init();
        void mode_upper();
        void mode_u_l_init();
        void mode_u_l();
        void mode_lower_init();
        void mode_lower();
        void mode_l_u_init();
        void mode_l_u();
        void mode_demoreq_init(Mode_e);
        void mode_demoreq();
        virtual BOOL Execute(Mtx**);
        virtual BOOL Draw();
    
    public:
        static const char M_arcname[];
        static Mtx M_tmp_mtx;
        /* 0x2C8 */ request_of_phase_process_class mPhs;
        /* 0x2D0 */ J3DModel* mModel;
        /* 0x2D4 */ Mode_e mMode;
        /* 0x2D8 */ f32 mOffset;
        /* 0x2DC */ f32 mSpeed;
        /* 0x2E0 */ u8 mBounces;
        /* 0x2E2 */ s16 mTimer;
        /* 0x2E4 */ bool mDemoAccepted;
        /* 0x2E6 */ s16 mEventIdx;
        /* 0x2E8 */ Mode_e mDemoNext;
    };
};

#endif /* D_A_OBJ_ASHUT_H */
