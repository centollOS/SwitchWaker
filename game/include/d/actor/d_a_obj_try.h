#ifndef D_A_OBJ_TRY_H
#define D_A_OBJ_TRY_H

#include "d/d_a_obj.h"
#include "f_op/f_op_actor.h"
#include "d/d_bg_s_acch.h"
#include "d/d_particle.h"
#include "m_Do/m_Do_ext.h"

namespace daObjTry {
    enum Type_e {
        UNK_2 = 2,
        UNK_3 = 3,
        UNK_4 = 4,
        UNK_5 = 5,
        UNK_6 = 6,
        UNK_7 = 7,
        UNK_8 = 8,
        UNK_9 = 9,
        UNK_10 = 10,
        UNK_11 = 11,
        UNK_12 = 12,
    };

    class Act_c : public fopAc_ac_c {
    public:
        enum Prm_e {
            PRM_SWSAVE_S = 8,
            PRM_SWSAVE_W = 8,

            PRM_TYPE_S = 0,
            PRM_TYPE_W = 4,
            PRM_DUMMY_S = 31,
            PRM_DUMMY_W = 1,
        };

        struct Attr_c {
            f32 mGravity, mWaterOffset, mAttentionY;
            u8 mWeight;
            f32 mThrowSpeedY, mThrowSpeedF, mPlayerSpeedRate;
            f32 mAirDrag, mAirDrag2, mBuoyancy, mWaterDrag, mWaterDrag2;
            f32 mMaxWaterSpeed;
            s16 m34, m36;
            f32 m38, m3C;
            u8 m40, m41, mHitSoundRadius, mHitSoundTime, mWaterSoundRadius, mWaterSoundTime;
            s16 mModelRes, mBrkRes;
            u8 mShadowSize, mRadius, mHeight, mRoofHeight;
            u32 mBreakSound, mLandSound, mWaterSound, mHitSound, mHeapSize;
            f32 m64, m68, m6C;
            u8 mCheckSwitch, mUnique, mClearSwitch, mCorrectPos, mLockPos, mAlwaysMove;
        };
        static const char M_arcname[];
        static const dCcD_SrcCyl M_cyl_src;
        static const Attr_c M_attr[13];
        static bool M_restart;
        static bool M_bingo;

        const Attr_c& attr() const { return M_attr[mType]; }
        bool prm_chk_type_tryStatue() const { return mType == UNK_5 || mType == UNK_6; }
        int prm_get_dummy() const { return daObj::PrmAbstract(this, PRM_DUMMY_W, PRM_DUMMY_S); }
        int prm_get_swSave() const {
            return daObj::PrmAbstract(this, PRM_SWSAVE_W, PRM_SWSAVE_S);
        }
        int prm_get_type() const {
            return daObj::PrmAbstract(this, PRM_TYPE_W, PRM_TYPE_S);
        }
        void to_correct_pos(const cXyz* param_1, short param_2, bool param_3, bool param_4) {
            field_0x63C.set(*param_1);
            field_0x648 = param_2;
            field_0x64C = param_3;
            field_0x64D = param_4;
        }

        void prm_set_swSave(int);
        static BOOL solidHeapCB(fopAc_ac_c*);
        bool create_heap();
        void init_cc();
        static void* search_sameType(void*, void*);
        bool chk_appear() const;
        cPhs_State _create();
        bool _delete();
        void mode_restart_init();
        void mode_restart();
        void mode_wait_init();
        void mode_wait();
        void mode_carry_init();
        void mode_carry();
        void mode_drop_init();
        void mode_drop();
        void mode_sink_init();
        void mode_sink();
        bool mode_proc_call();
        void cull_set_draw();
        void cull_set_move();
        void damaged();
        bool damage_cc_proc();
        bool damage_bg_proc();
        void damage_bg_proc_directly();
        bool chk_sink_water();
        bool chk_sinkdown_water();
        void calc_drop_param(float*, float*, float*) const;
        bool bound();
        void se_fall_water();
        void set_senv(int, int) const;
        void cam_lockoff() const;
        void set_mtx();
        void init_mtx();
        void eff_set_bingo(bool, bool);
        void eff_clr_bingo();
        void eff_land_smoke();
        void eff_hit_water_splash();
        void make_vib();
        bool check_circle();
        f32 get_water_h();
        bool _execute();
        bool _draw();

    public:
        /* 0x290 */ request_of_phase_process_class mPhase;
        /* 0x298 */ J3DModel* mpModel;
        /* 0x29C */ mDoExt_brkAnm mBrk;
        /* 0x2B4 */ dBgS_ObjAcch mAcch;
        /* 0x478 */ dBgS_AcchCir mAcchCir;
        /* 0x4B8 */ dCcD_Stts mStts;
        /* 0x4F4 */ dCcD_Cyl mCyl;
        /* 0x624 */ int mType;
        /* 0x628 */ int mMode;
        /* 0x62C */ int mPrevMode;
        /* 0x630 */ s16 mTimer;
        /* 0x632 */ s16 mLandTimer;
        /* 0x634 */ u8 mLanded;
        /* 0x635 */ u8 mMoveBG;
        /* 0x636 */ u8 mMassTimer;
        /* 0x638 */ f32 mPrevSpeedY;
        /* 0x63C */ cXyz field_0x63C;
        /* 0x648 */ s16 field_0x648;
        /* 0x64A */ s16 field_0x64A;
        /* 0x64C */ bool field_0x64C;
        /* 0x64D */ bool field_0x64D;
        /* 0x64E */ u8 field_0x64E;
        /* 0x64F */ u8 mLeftHomeRoom;
        /* 0x650 */ u8 mAppear;
        /* 0x651 */ u8 mBingoEffect;
        /* 0x652 */ u8 field_0x652;
        /* 0x653 */ u8 mSplashed;
        /* 0x654 */ dPa_followEcallBack mFollow;
        /* 0x668 */ JPABaseEmitter* mpEmitter;

        Act_c() : mpEmitter(NULL) {}
    };  // Size: 0x66C
};

STATIC_ASSERT(sizeof(daObjTry::Act_c::Attr_c) == 0x78);
STATIC_ASSERT(sizeof(daObjTry::Act_c) == 0x66C);

#endif /* D_A_OBJ_TRY_H */
