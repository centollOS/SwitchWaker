#ifndef D_A_OBJ_HAMI2_H
#define D_A_OBJ_HAMI2_H

#include "d/d_bg_s_movebg_actor.h"

namespace daObjHami2 {
    class Act_c : public dBgS_MoveBgActor {
    public:
        enum Prm_e { PRM_SWSAVE_W = 8, PRM_SWSAVE_S = 0 };
        s32 prm_get_swSave() const { return daObj::PrmAbstract(this, PRM_SWSAVE_W, PRM_SWSAVE_S); }
    
        virtual BOOL CreateHeap();
        virtual BOOL Create();
        cPhs_State Mthd_Create();
        virtual BOOL Delete();
        BOOL Mthd_Delete();
        void set_mtx();
        void init_mtx();
        void daObjHami2_close_stop();
        void daObjHami2_open_demo_wait();
        void daObjHami2_open_demo();
        void daObjHami2_open_stop();
        void daObjHami2_close_demo_wait();
        void daObjHami2_close_demo();
        virtual BOOL Execute(Mtx**);
        virtual BOOL Draw();
    
    public:
        static const char M_arcname[];
        static const char M_evname[];
        static Mtx M_tmp_mtx;
        /* 0x2C8 */ s16 mGateAngle;
        /* 0x2CC */ request_of_phase_process_class mPhs;
        /* 0x2D4 */ J3DModel* mModel;
        /* 0x2D8 */ dBgW* mStaticBg;
        /* 0x2DC */ Mtx mStaticMtx;
        /* 0x30C */ s32 mState;
        /* 0x310 */ s16 mOpenEvent;
        /* 0x312 */ s16 mCloseEvent;
    };
};

#endif /* D_A_OBJ_HAMI2_H */
