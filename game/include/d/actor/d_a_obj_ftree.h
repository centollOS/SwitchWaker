#ifndef D_A_OBJ_FTREE_H
#define D_A_OBJ_FTREE_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/d_a_obj.h"
#include "m_Do/m_Do_ext.h"
#include "SSystem/SComponent/c_phase.h"

class J3DNode;
class J3DModelData;
class msg_class;
class JPABaseEmitter;

namespace daObjFtree {
    struct _ftree_seach_info_ {
        /* 0x0 */ int mTotal;
        /* 0x4 */ int mBrought;
    };

    static void Ftree_get_water_pos(cXyz*, cXyz*);
    static s16 Ftree_checkXyEventCallBack(void*, int);
    static s16 Ftree_XyEventCallBack(void*, int);
    static BOOL is_broughtID(int);
    static BOOL Ftree_NodeCallBack_Effect(J3DNode*, int);
    static BOOL Ftree_NodeCallBack_M(J3DNode*, int);
    static BOOL Ftree_NodeCallBack_L(J3DNode*, int);

    class Act_c : public fopAc_ac_c {
    public:
        virtual ~Act_c() {}
        static const char M_arcname[];

        int param_get_tree_idx() const;

        void search_heart_part();
        BOOL launch_heart_part();
        BOOL place_heart_part();
        BOOL estimate_water();
        s16 XyEventCallBack(int);
        void XyEvent_init();
        BOOL XyEvent_set(s16);
        void XyEvent_exe();
        BOOL SetJointAnimation(int, float, float, int);
        BOOL PlayStopJointAnimation();
        BOOL PlayStopColorAnimation();
        void set_first_stat();
        void set_collision();
        void talk_ct();
        void talk_main();
        void get_tev_material0_color(J3DModelData*, unsigned long, s16*, s16*, s16*);
        void set_tev_color(J3DModelData*, unsigned long, s16, s16, s16);
        BOOL is_brought();
        void set_broughtID(int);
        void set_brought();
        void unset_broughtID(int);
        void unset_brought();
        void get_ftree_info(_ftree_seach_info_*);
        BOOL iam_last();
        BOOL action_none_init(s16);
        void action_none_main();
        BOOL action_waitS_init(s16);
        void action_waitS_main();
        BOOL action_waitM_init(s16);
        void action_waitM_main();
        BOOL action_waitL_init(s16);
        void action_waitL_main();
        BOOL action_pikupikuS_init(s16);
        void action_pikupikuS_main();
        BOOL action_pikupikuM_init(s16);
        void action_pikupikuM_main();
        BOOL action_pikupikuL_init(s16);
        void action_pikupikuL_main();
        BOOL action_changeSL_init(s16);
        void action_changeSL_main();
        BOOL action_changeSL2_init(s16);
        void action_changeSL2_main();
        BOOL action_changeLS_init(s16);
        void action_changeLS_main();
        BOOL action_changeLS2_init(s16);
        void action_changeLS2_main();
        BOOL action_changeSM_init(s16);
        void action_changeSM_main();
        BOOL action_changeMS_init(s16);
        void action_changeMS_main();
        BOOL process_init(int, s16);
        void process_main();
        static BOOL solidHeapCB(fopAc_ac_c*);
        BOOL NodeCallBack_Effect(J3DNode*, int);
        bool create_heap();
        cPhs_State _create();
        bool _delete();
        void set_mtx();
        bool _execute();
        bool _draw();

    public:
        /* 0x294 */ s16 mJointRotMX;
        /* 0x296 */ s16 mJointRotMY;
        /* 0x298 */ s16 mJointRotLY;
        /* 0x29A */ s16 mJointRotLZ;
        /* 0x29C */ request_of_phase_process_class mPhs;
#if VERSION == VERSION_DEMO
        /* 0x2A4 */ JPABaseEmitter* mpEmitter1;
        /* 0x2A8 */ JPABaseEmitter* mpEmitter2;
#endif
        /* 0x2A4 */ s16 mEffectFlag;
        /* 0x2A6 */ u8 mModelS;
        /* 0x2A7 */ u8 mModelL;
        /* 0x2A8 */ Mtx mMtx;
        /* 0x2D8 */ mDoExt_McaMorf* mpMorf;
        /* 0x2DC */ J3DModel* mpModel;
        /* 0x2E0 */ mDoExt_brkAnm mBrkAnm;
        /* 0x2F8 */ dBgS_ObjGndChk mGndChk;
        /* 0x34C */ f32 mGroundY;
        /* 0x350 */ s16 mEventCam0Idx;
        /* 0x352 */ s16 mEventCam1Idx;
        /* 0x354 */ s16 mEventCam2Idx;
        /* 0x356 */ s16 mXyEventIdx;
        /* 0x358 */ s16 mXyEventState;
        /* 0x35A */ u8 m35A[0x35C - 0x35A];
        /* 0x35C */ dCcD_Stts mSttsW;
        /* 0x398 */ dCcD_Cyl mCylW;
        /* 0x4C8 */ f32 mCrashRadius;
        /* 0x4CC */ dCcD_Stts mStts;
        /* 0x508 */ dCcD_Cyl mCyl;
        /* 0x638 */ int mBroughtSession;
        /* 0x63C */ f32 mScaleMul;
        /* 0x640 */ f32 m640;
        /* 0x644 */ int mTreeIdx;
        /* 0x648 */ s16 mHeartPlaced;
        /* 0x64A */ u8 m64A[0x64C - 0x64A];
        /* 0x64C */ fpc_ProcID mSpawnedHeartPieceProcessId;
        /* 0x650 */ s16 mLaunchHeart;
        /* 0x652 */ s16 m652;
        /* 0x654 */ GXColorS10 mColorS;
        /* 0x65C */ GXColorS10 mColorSWork;
        /* 0x664 */ GXColorS10 mColorL;
        /* 0x66C */ GXColorS10 mColorSBase;
        /* 0x674 */ GXColorS10 mColorLBase;
        /* 0x67C */ int mPrevCoHit;
        /* 0x680 */ int mMode;
        /* 0x684 */ s16 mGrowTimer;
        /* 0x686 */ u8 m686[0x688 - 0x686];
        /* 0x688 */ int mPikuSMax;
        /* 0x68C */ int mPikuSCount;
        /* 0x690 */ s16 m690;
        /* 0x692 */ s16 m692;
        /* 0x694 */ s16 m694;
        /* 0x696 */ s16 mJointTargetMX;
        /* 0x698 */ s16 mJointTargetMY;
        /* 0x69A */ s16 m69A;
        /* 0x69C */ s16 m69C;
        /* 0x69E */ s16 m69E;
        /* 0x6A0 */ s16 mJointTargetLY;
        /* 0x6A2 */ s16 mJointTargetLZ;
        /* 0x6A4 */ fpc_ProcID mMsgID;
        /* 0x6A8 */ msg_class* mpMsg;
        /* 0x6AC */ u8 m6AC[0x6B0 - 0x6AC];
    };
};

STATIC_ASSERT(sizeof(daObjFtree::Act_c) == DEMO_SELECT(0x6B8, 0x6B0));

#endif /* D_A_OBJ_FTREE_H */
