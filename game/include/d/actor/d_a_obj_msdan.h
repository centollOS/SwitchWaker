#ifndef D_A_OBJ_MSDAN_H
#define D_A_OBJ_MSDAN_H

#include "f_op/f_op_actor.h"
#include "d/d_a_obj.h"

namespace daObjMsdan {
    class Act_c : public fopAc_ac_c {
    public:
        enum Prm_e {
            PRM_SWSAVE_W = 8,
            PRM_SWSAVE_S = 0,
            PRM_SIZE_W = 1,
            PRM_SIZE_S = 16,
            PRM_SOUND_W = 1,
            PRM_SOUND_S = 18,
            PRM_EVENT_W = 8,
            PRM_EVENT_S = 24,
        };
        u8 prm_get_evId() const { return daObj::PrmAbstract(this, PRM_EVENT_W, PRM_EVENT_S); }
        s32 prm_get_size() const { return daObj::PrmAbstract(this, PRM_SIZE_W, PRM_SIZE_S); }
        u8 prm_get_sound() const { return daObj::PrmAbstract(this, PRM_SOUND_W, PRM_SOUND_S); }
        s32 prm_get_swSave() const { return daObj::PrmAbstract(this, PRM_SWSAVE_W, PRM_SWSAVE_S); }

        static const char M_arcname[];
        static const char M_evname[];
    
        cPhs_State Mthd_Create();
        BOOL Mthd_Execute();
        BOOL Mthd_Delete();
    
    public:
        /* 0x290 */ request_of_phase_process_class mPhs;
        /* 0x298 */ s16 mEventIdx;
        /* 0x29C */ int mState;
    };
};

#endif /* D_A_OBJ_MSDAN_H */
