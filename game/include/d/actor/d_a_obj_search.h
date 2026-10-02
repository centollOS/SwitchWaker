#ifndef D_A_OBJ_SEARCH_H
#define D_A_OBJ_SEARCH_H

#include "f_op/f_op_actor.h"
#include "d/d_cc_d.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_lib.h"
#include "d/d_particle.h"
#include "d/d_kankyo.h"
#include "SSystem/SComponent/c_phase.h"
#include "dolphin/mtx/mtx.h"

class J3DNode;
class J3DModel;
class dBgW;
class dPath;

namespace daObj_Search {
    class Act_c;

    class Bgc_c {
    public:
        Bgc_c();
        void wall_pos(const daObj_Search::Act_c*, cXyz*, cXyz*, bool*, float*);

    public:
        /* 0x00 */ dBgS_LinChk mLinChk;
        /* 0x6C */ cXyz mWallPos;
    };  // Size: 0x78

    class Act_c : public fopAc_ac_c {
    public:
        enum Proc_e {
            PROC_INIT_e = 0,
            PROC_EXEC_e = 1,
        };

        enum Mode_e {
            MODE_SEARCH_RND_e = 0,
            MODE_SEARCH_PATH_e = 1,
            MODE_STOP_e = 2,
            MODE_TO_SEARCH_e = 3,
            MODE_TO_STOP_e = 4,
            MODE_FIND_e = 5,
            MODE_FIND_2ND_e = 6,
            MODE_SEARCH_BDK_e = 7,
        };

        struct Attr_c {
            /* 0x00 */ u8 mSkipExecute;
            /* 0x01 */ u8 mSkipDraw;
            /* 0x02 */ s16 m02;
            /* 0x04 */ f32 m04;
            /* 0x08 */ s16 m08;
            /* 0x0A */ u8 m0A[0x10 - 0x0A];
            /* 0x10 */ f32 m10;
            /* 0x14 */ f32 m14;
            /* 0x18 */ f32 m18;
            /* 0x1C */ f32 m1C;
            /* 0x20 */ f32 m20;
            /* 0x24 */ u8 m24;
            /* 0x25 */ u8 m25[0x28 - 0x25];
            /* 0x28 */ f32 mCpsR;
            /* 0x2C */ u8 m2C[0x38 - 0x2C];
            /* 0x38 */ u8 m38;
            /* 0x39 */ u8 m39;
            /* 0x3A */ u8 m3A[2];
            /* 0x3C */ s16 m3C;
            /* 0x3E */ u8 m3E;
            /* 0x3F */ u8 m3F[0x42 - 0x3F];
            /* 0x42 */ u8 m42;
            /* 0x43 */ u8 m43;
            /* 0x44 */ u8 m44;
            /* 0x45 */ u8 m45;
            /* 0x46 */ u8 m46;
            /* 0x47 */ u8 m47;
            /* 0x48 */ u8 m48[0x4C - 0x48];
            /* 0x4C */ f32 m4C;
            /* 0x50 */ s16 m50;
            /* 0x52 */ u8 m52[0x54 - 0x52];
            /* 0x54 */ f32 m54;
            /* 0x58 */ f32 m58;
            /* 0x5C */ s16 m5C;
            /* 0x5E */ s16 m5E;
        };  // Size: 0x60

        static bool getFindFlag() { return m_find_flag == true; }
        static void setFindFlag(bool flag) { m_find_flag = flag; }

        static bool m_find_flag;
        static const char m_arc_name[];
        static const Attr_c m_attr;

        bool isSecond() { return m860 != 0; }
        void modeProcInit(int newMode) { modeProc(PROC_INIT_e, newMode); }
        void setBkControl(bool control) { mBkControl = control; }
        void setChildId(fpc_ProcID id) { mChildId = id; }

        BOOL _createHeap();
        bool searchCreateHeap();
        bool beamCreateHeap(int);
        void _nodeControl(J3DNode*, J3DModel*);
        void modeSearchRndInit();
        void modeSearchRnd();
        void modeSearchPathInit();
        void modeSearchPath();
        void modeStopInit();
        void modeStop();
        void modeToSearchInit();
        void modeToSearch();
        void modeToStopInit();
        void modeToStop();
        void modeFindInit();
        void modeFind();
        void modeFind2ndInit();
        void modeFind2nd();
        void modeSearchBdkInit();
        void modeSearchBdk();
        void modeProc(Proc_e, int);
        const Attr_c* attr() const;
        void SetArgData();
        void CreateInit();
        cPhs_State _create();
        void smoke_set(float, int);
        BOOL _execute();
        void check_bk_control();
        void set_mtx_base();
        void set_mtx_light_A();
        void set_mtx_light_B();
        void set_moveBG_mtx_base();
        void set_moveBG_mtx_light_A();
        void set_moveBG_mtx_light_B();
        void bg_check();
        bool player_check();
        bool set_path_info();
        bool is_path_info();
        BOOL _draw();
        BOOL _delete();
        BOOL _isdelete();

    public:
        /* 0x290 */ int mMode;
        /* 0x294 */ Bgc_c mBgc;
        /* 0x30C */ dCcD_Cps mCps[2];
        /* 0x57C */ dCcD_Stts mStts[2];
        /* 0x5F4 */ J3DModel* mpModel;
        /* 0x5F8 */ J3DModel* mpBeamModel[2];
        /* 0x600 */ cXyz m600;
        /* 0x60C */ cXyz m60C[2];
        /* 0x624 */ cXyz mBeamStart[2];
        /* 0x63C */ cXyz mBeamEnd[2];
        /* 0x654 */ f32 m654;
        /* 0x658 */ f32 m658;
        /* 0x65C */ f32 m65C;
        /* 0x660 */ f32 m660;
        /* 0x664 */ bool mBkControl;
        /* 0x668 */ dBgW* mpBeamBgW[2];
        /* 0x670 */ dBgW* mpBgW;
        /* 0x674 */ Mtx mBeamMtx[2];
        /* 0x6D4 */ Mtx mBgMtx;
        /* 0x704 */ u8 m704[0x706 - 0x704];
        /* 0x706 */ csXyz mLightAng[2];
        /* 0x712 */ u8 m712[0x71C - 0x712];
        /* 0x71C */ Mtx mAlphaMtx[2];
        /* 0x77C */ bool mWallHit[2];
        /* 0x77E */ s16 m77E;
        /* 0x780 */ LIGHT_INFLUENCE mLightInf;
        /* 0x7A0 */ int m7A0;
        /* 0x7A4 */ int m7A4;
        /* 0x7A8 */ s16 m7A8;
        /* 0x7AA */ s16 m7AA;
        /* 0x7AC */ s16 m7AC;
        /* 0x7AE */ s16 m7AE;
        /* 0x7B0 */ s16 m7B0;
        /* 0x7B2 */ s16 m7B2;
        /* 0x7B4 */ s16 m7B4;
        /* 0x7B6 */ u8 mCullBeamA;
        /* 0x7B7 */ u8 mCullBeamB;
        /* 0x7B8 */ u8 mCullBase;
        /* 0x7B9 */ u8 m7B9[0x7BC - 0x7B9];
        /* 0x7BC */ dLib_circle_path_c mCirclePath;
        /* 0x7E0 */ s16 mPlayerTurnY;
        /* 0x7E2 */ u8 m7E2;
        /* 0x7E3 */ u8 m7E3[0x7F4 - 0x7E3];
        /* 0x7F4 */ cXyz mSmokePos;
        /* 0x800 */ csXyz mSmokeRot;
        /* 0x808 */ dPa_smokeEcallBack mSmokeCb;
#if VERSION == VERSION_DEMO
        /* 0x828 */ JPABaseEmitter* mpSmokeEmitter;
#endif
        /* 0x828 */ int mSmokeTimer;
        /* 0x82C */ u8 m82C;
        /* 0x82D */ u8 m82D;
        /* 0x82E */ u8 m82E[0x830 - 0x82E];
        /* 0x830 */ int m830;
        /* 0x834 */ u8 m834;
        /* 0x835 */ u8 m835;
        /* 0x836 */ u8 mScaleType;
        /* 0x837 */ u8 mPathId;
        /* 0x838 */ cXyz mPathPos;
        /* 0x844 */ s8 mPathPntIdx;
        /* 0x848 */ dPath* mpPath;
        /* 0x84C */ fpc_ProcID mChildId;
        /* 0x850 */ int m850;
        /* 0x854 */ request_of_phase_process_class mPhase;
        /* 0x85C */ int mSwSave;
        /* 0x860 */ u8 m860;
        /* 0x864 */ dBgS_ObjLinChk mLinChk;
        /* 0x8D0 */ u8 m8D0;
        /* 0x8D1 */ u8 m8D1[0x8D4 - 0x8D1];
        /* 0x8D4 */ cXyz mDebugFanPos;
        /* 0x8E0 */ s16 mDebugFanAngleY;
        /* 0x8E2 */ u8 m8E2[0x8E4 - 0x8E2];
        /* 0x8E4 */ f32 mDebugFanRadius;
        /* 0x8E8 */ f32 mDebugCircleRadius;
        /* 0x8EC */ s16 mDebugFanSpread;
        /* 0x8EE */ u8 mDebugFanOk;
        /* 0x8F0 */ int mIdleTimer;
        /* 0x8F4 */ cXyz mHoldPos;
        /* 0x900 */ int mSearchSide;
        /* 0x904 */ int mBdkTimer;
    };

    STATIC_ASSERT(sizeof(Bgc_c) == 0x78);
    STATIC_ASSERT(sizeof(Act_c) == DEMO_SELECT(0x90C, 0x908));

    namespace Mthd {
        cPhs_State Create(void*);
        BOOL Delete(void*);
        BOOL Execute(void*);
        BOOL Draw(void*);
        BOOL IsDelete(void*);
        extern actor_method_class Table;
    };
};

#endif /* D_A_OBJ_SEARCH_H */
