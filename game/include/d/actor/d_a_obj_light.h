#ifndef D_A_OBJ_LIGHT_H
#define D_A_OBJ_LIGHT_H

#include "f_op/f_op_actor.h"
#include "d/d_bg_w.h"
#include "d/d_cc_d.h"
#include "d/d_particle.h"

namespace daObjLight {
    class Act_c : public fopAc_ac_c {
    public:
        enum {
            LIGHT_LIGHT_BASE,
            LIGHT_BGW_NUM,
        };

        static BOOL solidHeapCB(fopAc_ac_c*);
        bool create_heap();
        void init_collision();
        void set_collision();
        cPhs_State _create();
        bool _delete();
        BOOL set_fire(int);
        void draw_fire();
        void exe_fire();
        void delete_fire();
        bool now_event(s16);
        BOOL set_event(s16);
        void exe_event();
        void set_mtx();
        void renew_angle();
        void control_light();
        void control_treasure();
        bool _execute();
        bool _draw();
        
        static bool renew_light_angle();
        static s16 get_light_angle();
        static s16 get_light_dif_angle();
        static BOOL set_light_dif_angle_LOD(s16);
        static bool set_light_dif_angle_FRRS(s16);

        static s16 M_S_light_angle;
        static unsigned int M_S_pre_set_frame_LOD;
        static unsigned int M_S_pre_set_frame_FRRS;
        static u8 M_S_lod_access;

public:
        static const char M_arcname[];
        static const dCcD_SrcCyl M_cyl_src;
        /* 0x290 */ request_of_phase_process_class mPhase;
        /* 0x298 */ J3DModel* mpModel[3];
        /* 0x2A4 */ dBgW* M_bgw[LIGHT_BGW_NUM];
        /* 0x2A8 */ Mtx mBgMtx[LIGHT_BGW_NUM];
        /* 0x2D8 */ s16 mLightAngle;
        /* 0x2DA */ s16 mLightTimer;
        /* 0x2DC */ s16 mLit;
        /* 0x2DE */ s16 mTreasureTimer;
        /* 0x2E0 */ dCcD_Stts mStts;
        /* 0x31C */ dCcD_Cyl mCyl;
        /* 0x44C */ dPa_followEcallBack mFireCallback;
        /* 0x460 */ s16 mFirePhase;
        /* 0x462 */ s16 mFireAngle;
        /* 0x464 */ u8 mFireAlpha;
        /* 0x468 */ f32 mFireScale;
        /* 0x46C */ Mtx mFireMtx;
        /* 0x49C */ s16 mEventIdx;
        /* 0x49E */ s16 mEventState;
        /* 0x4A0 */ s16 mLightEvent;
    };
};

#endif /* D_A_OBJ_LIGHT_H */
