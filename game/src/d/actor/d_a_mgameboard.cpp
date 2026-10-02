/**
 * d_a_mgameboard.cpp
 * Squid-Hunt/"Sploosh Kaboom" Minigame
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_mgameboard.h"
#include "d/d_com_inf_game.h"
#include "d/d_2dnumber.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_controller_pad.h"
#include "SSystem/SComponent/c_lib.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JSystem/JUtility/JUTReport.h"
#if VERSION >= VERSION_USA
#include "res/Object/Kaisen_e.h"
#define MGBOARD_RES(name) dRes_INDEX_KAISEN_E_##name##_e
#else
#include "res/Object/Kaisen.h"
#define MGBOARD_RES(name) dRes_INDEX_KAISEN_##name##_e
#endif
#include <string.h>


#if VERSION > VERSION_JPN
char daMgBoard_c::m_arcname[9] = "";
#else
char daMgBoard_c::m_arcname[] = "Kaisen";
#endif
u8 daMgBoard_c::m_bullet_num = 24;
cXyz daMgBoard_c::m_cur_table[8][8] = {
    {
        cXyz(-87.5f, -87.5f, 0.0f),
        cXyz(-62.5f, -87.5f, 0.0f),
        cXyz(-37.5f, -87.5f, 0.0f),
        cXyz(-12.5f, -87.5f, 0.0f),
        cXyz(12.5f, -87.5f, 0.0f),
        cXyz(37.5f, -87.5f, 0.0f),
        cXyz(62.5f, -87.5f, 0.0f),
        cXyz(87.5f, -87.5f, 0.0f),
    },
    {
        cXyz(-87.5f, -62.5f, 0.0f),
        cXyz(-62.5f, -62.5f, 0.0f),
        cXyz(-37.5f, -62.5f, 0.0f),
        cXyz(-12.5f, -62.5f, 0.0f),
        cXyz(12.5f, -62.5f, 0.0f),
        cXyz(37.5f, -62.5f, 0.0f),
        cXyz(62.5f, -62.5f, 0.0f),
        cXyz(87.5f, -62.5f, 0.0f),
    },
    {
        cXyz(-87.5f, -37.5f, 0.0f),
        cXyz(-62.5f, -37.5f, 0.0f),
        cXyz(-37.5f, -37.5f, 0.0f),
        cXyz(-12.5f, -37.5f, 0.0f),
        cXyz(12.5f, -37.5f, 0.0f),
        cXyz(37.5f, -37.5f, 0.0f),
        cXyz(62.5f, -37.5f, 0.0f),
        cXyz(87.5f, -37.5f, 0.0f),
    },
    {
        cXyz(-87.5f, -12.5f, 0.0f),
        cXyz(-62.5f, -12.5f, 0.0f),
        cXyz(-37.5f, -12.5f, 0.0f),
        cXyz(-12.5f, -12.5f, 0.0f),
        cXyz(12.5f, -12.5f, 0.0f),
        cXyz(37.5f, -12.5f, 0.0f),
        cXyz(62.5f, -12.5f, 0.0f),
        cXyz(87.5f, -12.5f, 0.0f),
    },
    {
        cXyz(-87.5f, 12.5f, 0.0f),
        cXyz(-62.5f, 12.5f, 0.0f),
        cXyz(-37.5f, 12.5f, 0.0f),
        cXyz(-12.5f, 12.5f, 0.0f),
        cXyz(12.5f, 12.5f, 0.0f),
        cXyz(37.5f, 12.5f, 0.0f),
        cXyz(62.5f, 12.5f, 0.0f),
        cXyz(87.5f, 12.5f, 0.0f),
    },
    {
        cXyz(-87.5f, 37.5f, 0.0f),
        cXyz(-62.5f, 37.5f, 0.0f),
        cXyz(-37.5f, 37.5f, 0.0f),
        cXyz(-12.5f, 37.5f, 0.0f),
        cXyz(12.5f, 37.5f, 0.0f),
        cXyz(37.5f, 37.5f, 0.0f),
        cXyz(62.5f, 37.5f, 0.0f),
        cXyz(87.5f, 37.5f, 0.0f),
    },
    {
        cXyz(-87.5f, 62.5f, 0.0f),
        cXyz(-62.5f, 62.5f, 0.0f),
        cXyz(-37.5f, 62.5f, 0.0f),
        cXyz(-12.5f, 62.5f, 0.0f),
        cXyz(12.5f, 62.5f, 0.0f),
        cXyz(37.5f, 62.5f, 0.0f),
        cXyz(62.5f, 62.5f, 0.0f),
        cXyz(87.5f, 62.5f, 0.0f),
    },
    {
        cXyz(-87.5f, 87.5f, 0.0f),
        cXyz(-62.5f, 87.5f, 0.0f),
        cXyz(-37.5f, 87.5f, 0.0f),
        cXyz(-12.5f, 87.5f, 0.0f),
        cXyz(12.5f, 87.5f, 0.0f),
        cXyz(37.5f, 87.5f, 0.0f),
        cXyz(62.5f, 87.5f, 0.0f),
        cXyz(87.5f, 87.5f, 0.0f),
    },
};
cXyz daMgBoard_c::m_sink_table[3] = {
    cXyz(162.5f, 87.5f, 0.0f),
    cXyz(162.5f, 62.5f, 0.0f),
    cXyz(162.5f, 37.5f, 0.0f),
};
cXyz daMgBoard_c::m_bullet_table[24] = {
    cXyz(-150.0f, 90.0f, 0.0f),
    cXyz(-150.0f, 70.0f, 0.0f),
    cXyz(-150.0f, 50.0f, 0.0f),
    cXyz(-150.0f, 30.0f, 0.0f),
    cXyz(-150.0f, 10.0f, 0.0f),
    cXyz(-150.0f, -10.0f, 0.0f),
    cXyz(-150.0f, -30.0f, 0.0f),
    cXyz(-150.0f, -50.0f, 0.0f),
    cXyz(-170.0f, 90.0f, 0.0f),
    cXyz(-170.0f, 70.0f, 0.0f),
    cXyz(-170.0f, 50.0f, 0.0f),
    cXyz(-170.0f, 30.0f, 0.0f),
    cXyz(-170.0f, 10.0f, 0.0f),
    cXyz(-170.0f, -10.0f, 0.0f),
    cXyz(-170.0f, -30.0f, 0.0f),
    cXyz(-170.0f, -50.0f, 0.0f),
    cXyz(-190.0f, 90.0f, 0.0f),
    cXyz(-190.0f, 70.0f, 0.0f),
    cXyz(-190.0f, 50.0f, 0.0f),
    cXyz(-190.0f, 30.0f, 0.0f),
    cXyz(-190.0f, 10.0f, 0.0f),
    cXyz(-190.0f, -10.0f, 0.0f),
    cXyz(-190.0f, -30.0f, 0.0f),
    cXyz(-190.0f, -50.0f, 0.0f),
};

/* 000000EC-0000010C       .text CheckCreateHeap__FP10fopAc_ac_c */
static BOOL CheckCreateHeap(fopAc_ac_c* actor) {
    return ((daMgBoard_c*)actor)->CreateHeap();
}

/* 0000010C-000007BC       .text CreateHeap__11daMgBoard_cFv */
BOOL daMgBoard_c::CreateHeap() {
    J3DModelData* modelData;
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AKBOD));
    JUT_ASSERT(VERSION_SELECT(0x126, 0x12D, 0x133, 0x133), modelData != 0);
    mpBoardModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
    if (mpBoardModel == NULL) {
        return FALSE;
    }
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AKCSR));
    JUT_ASSERT(VERSION_SELECT(0x136, 0x13D, 0x143, 0x143), modelData != 0);
    mpCursorModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
    if (mpCursorModel == NULL) {
        return FALSE;
    }
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AKATR));
    JUT_ASSERT(VERSION_SELECT(0x146, 0x14D, 0x153, 0x153), modelData != 0);
    for (int i = 0; i < 20; ++i) {
        mpHitModel[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        if (mpHitModel[i] == NULL) {
            return FALSE;
        }
    }
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AKHZR));
    JUT_ASSERT(VERSION_SELECT(0x158, 0x15F, 0x165, 0x165), modelData != 0);
    for (int i = 0; i < 32; ++i) {
        mpMissModel[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        if (mpMissModel[i] == NULL) {
            return FALSE;
        }
    }
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AK2SH));
    JUT_ASSERT(VERSION_SELECT(0x16A, 0x171, 0x177, 0x177), modelData != 0);
    for (int i = 0; i < 2; ++i) {
        mpShip2Model[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        if (mpShip2Model[i] == NULL) {
            return FALSE;
        }
    }
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AK3SH));
    JUT_ASSERT(VERSION_SELECT(0x17C, 0x183, 0x189, 0x189), modelData != 0);
    for (int i = 0; i < 2; ++i) {
        mpShip3Model[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        if (mpShip3Model[i] == NULL) {
            return FALSE;
        }
    }
    modelData = (J3DModelData*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BDL_AK4SH));
    JUT_ASSERT(VERSION_SELECT(0x18E, 0x195, 0x19B, 0x19B), modelData != 0);
    for (int i = 0; i < 2; ++i) {
        mpShip4Model[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000022);
        if (mpShip4Model[i] == NULL) {
            return FALSE;
        }
    }
    mpNumber0 = new dDlst_2DNumber_c();
    if (!mpNumber0) {
        return FALSE;
    }
    mpNumber0->init(4, 470, 75, 24, 24, 0);
    mpNumber1 = new dDlst_2DNumber_c();
    if (!mpNumber1) {
        return FALSE;
    }
    mpNumber1->init(4, 100, 80, 24, 24, 0);
    for (int i = 0; i < 3; ++i) {
        mpSquidIcon[i] = new dDlst_2DObject_c();
        if (!mpSquidIcon[i]) {
            return FALSE;
        }
#if VERSION >= VERSION_USA
        mpSquidIcon[i]->init((ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_KAISEN_IKA_01)), (ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_KAISEN_IKA_02)));
#else
        mpSquidIcon[i]->init((ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_GAME_SHIP_NODAMAGE)), (ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_GAME_HUNE_DAMAGE)));
#endif
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 8; ++j) {
            int index = j + i * 8;
            mpBombIcons[index] = new dDlst_2DObject_c();
            if (!mpBombIcons[index]) {
                return FALSE;
            }
            mpBombIcons[index]->init((ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_GAME_BOMB_01)), (ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_GAME_BOMB_02)));
        }
    }
    mpMinigameDList = new dDlst_2DMinigame_c();
    if (!mpMinigameDList) {
        return FALSE;
    }
    mpMinigameDList->init((ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_GAME_BEST_RECORD)), (ResTIMG*)dComIfG_getObjectRes(m_arcname, MGBOARD_RES(BTI_GAME_PAPER)));
    set_2dposition();
    return TRUE;
}

/* 00000804-00000904       .text set_2dposition__11daMgBoard_cFv */
void daMgBoard_c::set_2dposition() {
    f32 x = 523.0f;
    f32 y = 115.0f;
    f32 step = 47.0f;
    for (int i = 0; i < 3; ++i) {
        mpSquidIcon[i]->setPosition(x, y);
        y += step;
    }
    f32 bombX = 95.0f;
    f32 bombY = 120.0f;
    f32 startY = bombY;
    f32 bombStep = 35.0f;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 8; ++j) {
            mpBombIcons[j + i * 8]->setPosition(bombX, bombY);
            bombY += bombStep;
        }
        bombX -= bombStep;
        bombY = startY;
    }
    u8 titleAlpha = 255;
    u8 scoreAlpha = 255;
    s16 tx = 282, ty = 70, sx = 440, sy = 60;
    mpMinigameDList->setTitlePos(tx, ty);
    mpMinigameDList->setScorePos(sx, sy);
    mpMinigameDList->setTitleAlpha(titleAlpha);
    mpMinigameDList->setScoreAlpha(scoreAlpha);
}

/* 00000904-000009E0       .text CreateInit__11daMgBoard_cFv */
void daMgBoard_c::CreateInit() {
    fopAcM_SetMtx(this, mpBoardModel->getBaseTRMtx());
    fopAcM_setCullSizeBox(this, -600.0f, -300.0f, -500.0f, 600.0f, 300.0f, 100.0f);
    mState = 0;
    mMinigameStartIdx = dComIfGp_evmng_getEventIdx("MINIGAME_START");
    mMinigameEndIdx = dComIfGp_evmng_getEventIdx("MINIGAME_END");
    mStickControl.setWaitParm(5, 2, 3, 2, 0.9f, 0.5f, 0, 0x800);
    MiniGameInit();
}

/* 000009E0-00000AE8       .text MiniGameInit__11daMgBoard_cFv */
void daMgBoard_c::MiniGameInit() {
    mBoardPosX = mBoardPosY = 0;
    mLastFirePosX = mLastFirePosY = -100;
    field_0x478 = 0;
    field_0x474 = 0;
    mSeaFightGame.init(m_bullet_num, 3);
    fopAc_ac_c* npc = fopAcM_SearchByName(fpcNm_NPC_KG1_e);
    if (npc) {
        mNPCPos = npc->current.pos;
    }
    set_mtx();
    for (int i = 0; i < 3; ++i) {
        mpSquidIcon[i]->onBeforeTex();
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 8; ++j) {
            mpBombIcons[j + i * 8]->onBeforeTex();
        }
    }
}

/* 00000AE8-00000DEC       .text set_mtx__11daMgBoard_cFv */
void daMgBoard_c::set_mtx() {
    J3DModel* piece;
    {
        J3DModel* model = mpBoardModel;
        mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
        mDoMtx_stack_c::YrotM(current.angle.y);
        model->setBaseTRMtx(mDoMtx_stack_c::get());
    }
    {
        J3DModel* model = mpCursorModel;
        mDoMtx_stack_c::transS(current.pos.x + m_cur_table[mBoardPosY][mBoardPosX].x, current.pos.y + m_cur_table[mBoardPosY][mBoardPosX].y, current.pos.z + m_cur_table[mBoardPosY][mBoardPosX].z);
        mDoMtx_stack_c::YrotM(current.angle.y);
        model->setBaseTRMtx(mDoMtx_stack_c::get());
    }
    u8 x, y;
    mMissModelCount = 0;
    mHitModelCount = 0;
    for (x = 0; x < 8; ++x) {
        for (y = 0; y < 8; ++y) {
            int cell = mSeaFightGame.checkState(x, y);
            piece = NULL;
            if (cell == 3) {
                piece = mpHitModel[mHitModelCount];
                ++mHitModelCount;
            } else if (cell == 1) {
                piece = mpMissModel[mMissModelCount];
                ++mMissModelCount;
            }
            if (piece) {
                mDoMtx_stack_c::transS(current.pos.x + m_cur_table[y][x].x, current.pos.y + m_cur_table[y][x].y, current.pos.z + m_cur_table[y][x].z);
                mDoMtx_stack_c::YrotM(current.angle.y);
                piece->setBaseTRMtx(mDoMtx_stack_c::get());
            }
        }
    }
    int i;
    const u8 count = mSeaFightGame.mAliveShipNum;
    for (i = 0; i < count; ++i) {
        switch (mSeaFightGame.mShips[i].field_0x8) {
        case 2: piece = mpShip2Model[0]; break;
        case 3: piece = mpShip3Model[0]; break;
        case 4: piece = mpShip4Model[0]; break;
        default: piece = NULL; break;
        }
        if (piece) {
            u8 x = mSeaFightGame.getShipStartX(i);
            u8 y = mSeaFightGame.getShipStartY(i);
            mDoMtx_stack_c::transS(current.pos.x + m_cur_table[y][x].x, current.pos.y + m_cur_table[y][x].y, current.pos.z + m_cur_table[y][x].z);
            mDoMtx_stack_c::YrotM(current.angle.y);
            if (mSeaFightGame.getShipVecY(i) == 0) {
                mDoMtx_stack_c::ZrotM(0x4000);
            }
            else {
                mDoMtx_stack_c::ZrotM(-0x8000);
            }
            piece->setBaseTRMtx(mDoMtx_stack_c::get());
        }
    }
}

// Unused; stripped by the linker. Name/signature from d_a_mgameboard.map; body reconstructed from its leftover
// 0.8f .rodata literal and weak cXyz destructor. The demo debug map shows it used XYZrotM, setBaseScale, a cXyz and
// ZXYrotM, and the TU's unused camera eye/center inlines suggest "bb" is billboard: the markers face the camera.
void daMgBoard_c::set_mtx_bb() {
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::XYZrotM(current.angle.x, current.angle.y, current.angle.z);
    mpBoardModel->setBaseScale(scale);
    mpBoardModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mDoMtx_stack_c::transS(current.pos.x + m_cur_table[mBoardPosY][mBoardPosX].x, current.pos.y + m_cur_table[mBoardPosY][mBoardPosX].y, current.pos.z + m_cur_table[mBoardPosY][mBoardPosX].z);
    mDoMtx_stack_c::XYZrotM(current.angle.x, current.angle.y, current.angle.z);
    mpCursorModel->setBaseScale(scale);
    mpCursorModel->setBaseTRMtx(mDoMtx_stack_c::get());
    camera_process_class* camera = dComIfGp_getCamera(0);
    s16 angleX = cLib_targetAngleX(fopCamM_GetCenter_p(camera), fopCamM_GetEye_p(camera));
    s16 angleY = cLib_targetAngleY(fopCamM_GetCenter_p(camera), fopCamM_GetEye_p(camera));
    cXyz markerScale(0.8f, 0.8f, 0.8f);
    J3DModel* piece;
    u8 x, y;
    mMissModelCount = 0;
    mHitModelCount = 0;
    for (x = 0; x < 8; ++x) {
        for (y = 0; y < 8; ++y) {
            int cell = mSeaFightGame.checkState(x, y);
            piece = NULL;
            if (cell == 3) {
                piece = mpHitModel[mHitModelCount];
                ++mHitModelCount;
            } else if (cell == 1) {
                piece = mpMissModel[mMissModelCount];
                ++mMissModelCount;
            }
            if (piece) {
                mDoMtx_stack_c::transS(current.pos.x + m_cur_table[y][x].x, current.pos.y + m_cur_table[y][x].y, current.pos.z + m_cur_table[y][x].z);
                mDoMtx_stack_c::ZXYrotM(angleX, angleY, 0);
                piece->setBaseScale(markerScale);
                piece->setBaseTRMtx(mDoMtx_stack_c::get());
            }
        }
    }
}

// Unused; stripped by the linker. Name/signature from d_a_mgameboard.map; body reconstructed from its leftover
// 0.75f, -0.75f and 0.0f .rodata literals (main stick thresholds; 0.0f is the neutral position).
static BOOL checkStick(s16 i_dir) {
    f32 stickX = CPad_GET_STICK_POS_X(0);
    f32 stickY = CPad_GET_STICK_POS_Y(0);
    switch (i_dir) {
    case 0:
        return stickY > 0.75f;
    case 1:
        return stickY < -0.75f;
    case 2:
        return stickX < -0.75f;
    case 3:
        return stickX > 0.75f;
    default:
        return stickX == 0.0f && stickY == 0.0f;
    }
}

/* 00000E28-00000FD8       .text _execute__11daMgBoard_cFv */
bool daMgBoard_c::_execute() {
    int score;
    int highScore;
    highScore = dComIfGs_getEventReg(0xBEFF);
    score = mSeaFightGame.checkUsedBullet();
    mpNumber0->set(highScore);
    mpNumber1->set(score);
    set_2dposition();
    if (mbForceEnd) {
        execEndGame();
        mbEndGame = false;
        mbForceEnd = false;
    }
    switch (mState) {
    case 0:
        mState = 1;
        mbStartGame = false;
        break;
    case 1:
        if (mbStartGame) {
            mState = 2;
        }
        break;
    case 2:
        mState = 3;
        // Fall through to the first game frame.
    case 3:
        execGameMain();
        if (mbEndGame) {
            mTimer = 30;
            mState = 4;
        }
        break;
    case 4:
        if (cLib_calcTimer(&mTimer) == 0) {
            mState = 0;
            if (!mSeaFightGame.isClearGame()) {
                mDoAud_seStart(JA_SE_PLAYSPOT_END_F, &mNPCPos, 0);
            }
            else {
                mDoAud_seStart(JA_SE_PLAYSPOT_END_S, &mNPCPos, 0);
            }
        }
        break;
    }
    return true;
}

/* 00000FD8-00001040       .text execGameMain__11daMgBoard_cFv */
BOOL daMgBoard_c::execGameMain() {
    MinigameMain();
    bool ended = false;
    if (mSeaFightGame.mAliveShipNum == 0 || mSeaFightGame.mBulletNum == 0) {
        ended = true;
    }
    if (ended) {
        mbEndGame = true;
        return TRUE;
    }
    return FALSE;
}

/* 00001040-00001060       .text execEndGame__11daMgBoard_cFv */
void daMgBoard_c::execEndGame() {
    MiniGameInit();
}

/* 00001060-00001250       .text MinigameMain__11daMgBoard_cFv */
BOOL daMgBoard_c::MinigameMain() {
    if (mDoAud_checkSePlaying(JA_SE_PLAYSPOT_START)) {
        return TRUE;
    }
    CursorMove();
    int bullets = mSeaFightGame.mBulletNum;
    int oldShips = mSeaFightGame.mAliveShipNum;
    if (CPad_CHECK_TRIG_A(0) && bullets > 0) {
        int result = mSeaFightGame.attack(mBoardPosX, mBoardPosY);
        int ships = mSeaFightGame.mAliveShipNum;
        int dead = mSeaFightGame.mDeadShipNum;
        int score = mSeaFightGame.mScore;
        mLastFirePosX = mBoardPosX;
        mLastFirePosY = mBoardPosY;
        if (result >= 0) {
            mDoAud_seStart(JA_SE_PLAYSPOT_RIGHT, &mNPCPos, 0);
            if (oldShips != ships && ships != 0) {
                mDoAud_seStart(JA_SE_PLAYSPOT_DESTROY, &mNPCPos, 0);
            }
            dComIfGp_getVibration().StartShock(7, -33, cXyz(0.0f, 1.0f, 0.0f));
        } else if (result == -1) mDoAud_seStart(JA_SE_PLAYSPOT_WRONG, &mNPCPos, 0);
        if (dead > 0) {
            mpSquidIcon[dead - 1]->offBeforeTex();
        }
        if (score > 0) {
            mpBombIcons[score - 1]->offBeforeTex();
        }
    }
#if VERSION == VERSION_DEMO
    int nearEnemy = mSeaFightGame.getNearEnemy(mLastFirePosX, mLastFirePosY);
    JUTReport(480, 380, "NEAR ENEMY");
    JUTReport(490, 400, "%d\n", nearEnemy);
#endif
    set_mtx();
    return TRUE;
}

/* 00001250-000013C4       .text CursorMove__11daMgBoard_cFv */
void daMgBoard_c::CursorMove() {
    s8 oldX = mBoardPosX;
    s8 oldY = mBoardPosY;
    mStickControl.checkTrigger();
    if (mStickControl.checkLeftTrigger()) {
        --mBoardPosX;
    }
    else if (mStickControl.checkRightTrigger()) {
        ++mBoardPosX;
    }
    if (mStickControl.checkUpTrigger()) {
        ++mBoardPosY;
    }
    else if (mStickControl.checkDownTrigger()) {
        --mBoardPosY;
    }
    if (mBoardPosX > 7) {
        mBoardPosX = 7;
    }
    if (mBoardPosX < 0) {
        mBoardPosX = 0;
    }
    if (mBoardPosY > 7) {
        mBoardPosY = 7;
    }
    if (mBoardPosY < 0) {
        mBoardPosY = 0;
    }
    if (mBoardPosX != oldX || mBoardPosY != oldY) {
        mDoAud_seStart(JA_SE_PLAYSPOT_CURSOR);
    }
}

/* 000013C4-000014C8       .text daMgBoard_Create__FPv */
cPhs_State daMgBoard_c::_create() {
#if VERSION > VERSION_DEMO
    fopAcM_SetupActor(this, daMgBoard_c);
#endif
#if VERSION == VERSION_PAL
    sprintf(m_arcname, "Kaisen_%d\0", dComIfGs_getPalLanguage());
#elif VERSION > VERSION_JPN
    strcpy(m_arcname, "Kaisen_e");
#endif
    cPhs_State phase = dComIfG_resLoad(&mPhase, m_arcname);
    if (phase == cPhs_COMPLEATE_e) {
#if VERSION == VERSION_DEMO
        fopAcM_SetupActor(this, daMgBoard_c);
#endif
        if (!fopAcM_entrySolidHeap(this, CheckCreateHeap, 0x4E000)) {
            phase = cPhs_ERROR_e;
        }
        else {
            CreateInit();
        }
    }
    return phase;
}

static cPhs_State daMgBoard_Create(void* i_this) {
    return ((daMgBoard_c*)i_this)->_create();
}

/* 000014C8-00001518       .text daMgBoard_Delete__FPv */
bool daMgBoard_c::_delete() {
    dComIfG_resDeleteDemo(&mPhase, m_arcname);
#if VERSION > VERSION_DEMO
    mDoAud_seDeleteObject(&mNPCPos);
#endif
    return true;
}

static BOOL daMgBoard_Delete(void* i_this) {
    return ((daMgBoard_c*)i_this)->_delete();
}

/* 00001518-0000153C       .text daMgBoard_Draw__FPv */
static BOOL daMgBoard_Draw(void* i_this) {
    return ((daMgBoard_c*)i_this)->_draw();
}

/* 0000153C-00001850       .text _draw__11daMgBoard_cFv */
bool daMgBoard_c::_draw() {
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpBoardModel, &tevStr);
    mDoExt_modelUpdateDL(mpBoardModel);
    if (!mbDraw) {
        return true;
    }
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType(mpCursorModel, &tevStr);
    dComIfGd_setListMaskOff();
    mDoExt_modelUpdateDL(mpCursorModel);
    dComIfGd_setList();
    dComIfGd_setListMaskOff();
    for (int i = 0; i < mHitModelCount; ++i) {
        g_env_light.setLightTevColorType(mpHitModel[i], &tevStr);
        mDoExt_modelUpdateDL(mpHitModel[i]);
    }
    for (int i = 0; i < mMissModelCount; ++i) {
        g_env_light.setLightTevColorType(mpMissModel[i], &tevStr);
        mDoExt_modelUpdateDL(mpMissModel[i]);
    }
    dComIfGd_setList();
    for (int i = 0; i < 3; ++i) {
        int type = mSeaFightGame.getMaxHP(i);
        s32 bullets = mSeaFightGame.mBulletNum;
        s32 alive = mSeaFightGame.mAliveShipNum;
        bool ended = false;
        if ((alive == 0) | (bullets == 0)) {
            ended = true;
        }
        if (ended) {
            J3DModel* model;
            switch (type) {
            case 2: model = mpShip2Model[0]; break;
            case 3: model = mpShip3Model[0]; break;
            case 4: model = mpShip4Model[0]; break;
            default: model = NULL; break;
            }
            if (model) {
                g_env_light.setLightTevColorType(model, &tevStr);
                dComIfGd_setListMaskOff();
                mDoExt_modelUpdateDL(model);
                dComIfGd_setList();
            }
        }
    }
    dComIfGd_set2DOpa(mpMinigameDList);
    dComIfGd_set2DOpa(mpNumber0);
    dComIfGd_set2DOpa(mpNumber1);
    for (int i = 0; i < 24; ++i) {
        dComIfGd_set2DOpa(mpBombIcons[i]);
    }
    for (int i = 0; i < 3; ++i) {
        dComIfGd_set2DOpa(mpSquidIcon[i]);
    }
    return true;
}

/* 00001850-00001874       .text daMgBoard_Execute__FPv */
static BOOL daMgBoard_Execute(void* i_this) {
    return ((daMgBoard_c*)i_this)->_execute();
}

/* 00001874-0000187C       .text daMgBoard_IsDelete__FPv */
static BOOL daMgBoard_IsDelete(void*) {
    return TRUE;
}

static actor_method_class daMgBoardMethodTable = {
    (process_method_func)daMgBoard_Create,
    (process_method_func)daMgBoard_Delete,
    (process_method_func)daMgBoard_Execute,
    (process_method_func)daMgBoard_IsDelete,
    (process_method_func)daMgBoard_Draw,
};

actor_process_profile_definition g_profile_MGBOARD = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0003,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_MGBOARD_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daMgBoard_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_MGBOARD_e,
    /* Actor SubMtd */ &daMgBoardMethodTable,
    /* Status       */ fopAcStts_NOCULLEXEC_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
