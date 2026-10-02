#ifndef D_A_OBJ_VDS_H
#define D_A_OBJ_VDS_H

#include "f_op/f_op_actor.h"
#include "m_Do/m_Do_ext.h"
#include "d/d_bg_w.h"

class J3DAnmTransformKey;

namespace daObjVds {
    static void* ds_search_switchCB(void*, void*);

    class Act_c : public fopAc_ac_c {
    public:
        virtual ~Act_c() {}
        inline BOOL is_switch() const;
    
        BOOL SetLoopJointAnimation(J3DAnmTransformKey*, J3DAnmTransformKey*, float, float);
        BOOL PlayLoopJointAnimation();
        void set_first_process();
        void* search_switchCB(fopAc_ac_c*);
        void get_eye_set_info(cXyz*, short*, int);
        BOOL process_off_init();
        void process_off_main();
        BOOL process_on_init();
        void process_on_main();
        BOOL process_init(int);
        void process_main();
        void process_common();
        void create_point_light(int, cXyz*);
        void execute_point_light();
        void delete_point_light();
        void Event_init();
        void Event_exe();
        static BOOL solidHeapCB(fopAc_ac_c*);
        bool create_heap();
        cPhs_State _create();
        bool _delete();
        void set_mtx();
        bool _execute();
        bool _draw();
    
    public:
        static const char M_arcname[];
        /* 0x294 */ request_of_phase_process_class mPhase;
        /* 0x29C */ Mtx mBgMtx;
        /* 0x2CC */ mDoExt_McaMorf* M_anm0;
        /* 0x2D0 */ J3DAnmTransformKey* M_bck_data0;
        /* 0x2D4 */ mDoExt_brkAnm mBrk0;
        /* 0x2EC */ J3DAnmTevRegKey* M_brk_data0;
        /* 0x2F0 */ mDoExt_McaMorf* M_anm1;
        /* 0x2F4 */ J3DAnmTransformKey* M_bck_data1;
        /* 0x2F8 */ mDoExt_brkAnm mBrk1;
        /* 0x310 */ J3DAnmTevRegKey* M_brk_data1;
        /* 0x314 */ dBgW* mpBgW;
        /* 0x318 */ s32 mLoopAnimation;
        /* 0x31C */ s32 mProcess;
        /* 0x320 */ s32 mLightState;
        /* 0x324 */ fpc_ProcID mSwitchId[2];
        /* 0x32C */ f32 mIntensity[2];
        /* 0x334 */ s16 mEventIdx;
        /* 0x336 */ s16 mEventState;
        /* 0x338 */ s16 mVdsEvent;
        /* 0x33C */ LIGHT_INFLUENCE mLight[2];
        /* 0x37C */ cXyz mLightPos[2];
    };
};

#endif /* D_A_OBJ_VDS_H */
