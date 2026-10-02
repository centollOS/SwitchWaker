#ifndef D_A_OBJ_AYGR_H
#define D_A_OBJ_AYGR_H

#include "d/d_bg_s_movebg_actor.h"

namespace daObjAygr {
    class Act_c : public dBgS_MoveBgActor {
    public:
        enum Prm_e {
            PRM_MDL_W = 1,
            PRM_MDL_S = 0,
        };
        u8 prm_get_mdl() const { return daObj::PrmAbstract(this, PRM_MDL_W, PRM_MDL_S); }

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
    
    public:
        /* 0x2C8 */ request_of_phase_process_class mPhs;
        /* 0x2D0 */ J3DModel* mModelYagura;
        /* 0x2D4 */ J3DModel* mModelHashigo;
        /* 0x2D8 */ dBgW* mBgWHashigo;
        /* 0x2DC */ Mtx mHashigoMtx;
        /* 0x30C */ bool mHasHashigo;
    };
};

#endif /* D_A_OBJ_AYGR_H */
