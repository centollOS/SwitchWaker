#ifndef D_A_OBJ_FLAME_H
#define D_A_OBJ_FLAME_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_a_obj.h"
#include "SSystem/SComponent/c_phase.h"

class JPABaseEmitter;
class mDoExt_btkAnm;
class mDoExt_brkAnm;

namespace daObjFlame {
    class Act_c : public fopAc_ac_c {
    public:
        inline cPhs_State _create();
        inline bool _delete();
        inline bool _draw();
        inline bool _execute();
        inline bool _is_delete();
        struct AttrBase_c {
            int kiInterval;
            s16 largeRotation, rotation, liftRadius, liftBottom, liftTop, liftOffset, liftExtra;
        };
        struct AttrScale_c {
            f32 radius, height, modelXZ, modelY;
            int model, btk, brk;
            u32 heapSize;
            f32 animationSpeed, cycleSpeed;
            int footEffect;
            bool simple, hideCovered;
            f32 headXZ, headY, bodyXZ, bodyY, footScale, unused;
            s16 cullMinX, cullMinY, cullMinZ, cullMaxX, cullMaxY, cullMaxZ;
            f32 collisionEnd;
        };
        enum Prm_e {
            PRM_SCH_W = 8, PRM_SCH_S = 0,
            PRM_KINUM_W = 5, PRM_KINUM_S = 8,
            PRM_KICYCLE_W = 2, PRM_KICYCLE_S = 13,
            PRM_SWITCH_W = 8, PRM_SWITCH_S = 16,
            PRM_SCALE_W = 2, PRM_SCALE_S = 28,
            PRM_HAZE_W = 1, PRM_HAZE_S = 31,
        };
        static const AttrBase_c M_attr_base;
        static const dCcD_SrcCps M_cps_src;
        static const AttrScale_c M_attr_scl[4];
        static char M_arcname[];
        const AttrBase_c& attr_base() const { return M_attr_base; }
        const AttrScale_c& attr_scl() const { return M_attr_scl[mType]; }
        int prm_get_haze() const { return daObj::PrmAbstract(this, PRM_HAZE_W, PRM_HAZE_S); }
        int prm_get_kiCycle() const { return daObj::PrmAbstract(this, PRM_KICYCLE_W, PRM_KICYCLE_S); }
        int prm_get_kiNum() const { int n = daObj::PrmAbstract(this, PRM_KINUM_W, PRM_KINUM_S) + 1; if (n == 32) return 0; return n; }
        u8 prm_get_sch() const { return daObj::PrmAbstract(this, PRM_SCH_W, PRM_SCH_S); }
        int prm_get_scl() const { return daObj::PrmAbstract(this, PRM_SCALE_W, PRM_SCALE_S); }
        int prm_get_swSave() const { return daObj::PrmAbstract(this, PRM_SWITCH_W, PRM_SWITCH_S); }

        void set_switch();
        static BOOL solidHeapCB(fopAc_ac_c*);
        bool create_heap();
        void create_mode_init();
        void set_mtx();
        void init_mtx();
        void em_position();
        void em_simple_set();
        void em_simple_inv();
        void em_manual_set();
        void em_manual_inv();
        void ki_init();
        void ki_make();
        void eff_hase();
        void se_fireblast_omen();
        static void* liftup_magmarock(void*, void*);
        static void* liftup_mflft(void*, void*);
        void mode_wait();
        void mode_wait2();
        void mode_l_before();
        void mode_l_u();
        void mode_u();
        void mode_u_l();
        void mode_l_after();
        void mode_proc_call();
    
    public:
        /* 0x290 */ request_of_phase_process_class mPhs;
        /* 0x298 */ J3DModel* mpModel;
        /* 0x29C */ mDoExt_btkAnm* mpBtkAnm;
        /* 0x2A0 */ mDoExt_brkAnm* mpBrkAnm;
        /* 0x2A4 */ dCcD_Stts mStts;
        /* 0x2E0 */ dCcD_Cps mCps;
        /* 0x418 */ cXyz mCpsP0;
        /* 0x424 */ cXyz mCpsP1;
        /* 0x430 */ f32 mCpsRad;
        /* 0x434 */ bool m434;
        /* 0x435 */ u8 m435[0x438 - 0x435];
        /* 0x438 */ int mType;
        /* 0x43C */ int mModeProc;
        /* 0x440 */ f32 mTimer;
        /* 0x444 */ f32 mHeight;
        /* 0x448 */ JPABaseEmitter* mpEmitter0;
        /* 0x44C */ JPABaseEmitter* mpEmitter1;
        /* 0x450 */ JPABaseEmitter* mpEmitter2;
        /* 0x454 */ s8 mEm0State;
        /* 0x455 */ s8 mEm1State;
        /* 0x456 */ s8 mEm2State;
        /* 0x457 */ s8 m457;
        /* 0x458 */ s8 m458;
        /* 0x459 */ u8 m459;
        /* 0x45A */ u8 m45A;
        /* 0x45B */ u8 m45B[0x45C - 0x45B];
        /* 0x45C */ int m45C;
        /* 0x460 */ int m460;
        /* 0x464 */ int m464;
        /* 0x468 */ s16 mRotY;
        /* 0x46A */ u8 m46A[0x46C - 0x46A];
        /* 0x46C */ f32 m46C;
        /* 0x470 */ f32 mExtraScaleY;
        /* 0x474 */ cXyz mOrigScale;
    };
    
    STATIC_ASSERT(sizeof(Act_c) == 0x480);

    namespace Method {
        cPhs_State Create(void*);
        BOOL Delete(void*);
        BOOL Execute(void*);
        BOOL Draw(void*);
        BOOL IsDelete(void*);
        extern actor_method_class Table;
    };
};

#endif /* D_A_OBJ_FLAME_H */
