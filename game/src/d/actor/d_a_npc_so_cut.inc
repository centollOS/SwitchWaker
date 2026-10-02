// The original name of this file is unknown, but it must exist because d_a_npc_so's "cut" functions
// are in a separate .text section from the rest of its functions.

#include "d/actor/d_a_npc_so.h"

/* 00004CE8-000051E8       .text cutProc__10daNpc_So_cFv */
void daNpc_So_c::cutProc() {
    typedef void (daNpc_So_c::*cutFunc)();
    struct cut_entry_t {
        cutFunc start;
        cutFunc proc;
    };

    mStaffIdx = dComIfGp_evmng_getMyStaffId("NpcSo");
    if (mStaffIdx == -1) {
        return;
    }

    static char* action_table[] = {
        "SWIM",
        "JUMP",
        "APPEAR",
        "DIVE",
        "DISAPPEAR",
        "SET_ANM",
        "EFFECT",
        "EQUIP",
        "UNEQUIP",
        "EATESA",
        "EATESA1ST",
        "JUMP_MAPOPEN",
        "MINIGAME",
        "TURN",
        "MINIGAME_WARP",
        "MINIGAME_RETURN",
        "PARTNER_SHIP",
        "MINIGAME_WAIT",
        "MINIGAME_END",
        "MINIGAME_PL_TURN",
        "MINIGAME_PL_UP",
    };

    static cut_entry_t cut_tbl[] = {
        {&daNpc_So_c::cutSwimStart, &daNpc_So_c::cutSwimProc},
        {&daNpc_So_c::cutJumpStart, &daNpc_So_c::cutJumpProc},
        {&daNpc_So_c::cutAppearStart, &daNpc_So_c::cutAppearProc},
        {&daNpc_So_c::cutDiveStart, &daNpc_So_c::cutDiveProc},
        {&daNpc_So_c::cutDisappearStart, &daNpc_So_c::cutDisappearProc},
        {&daNpc_So_c::cutSetAnmStart, &daNpc_So_c::cutSetAnmProc},
        {&daNpc_So_c::cutEffectStart, &daNpc_So_c::cutEffectProc},
        {&daNpc_So_c::cutEquipStart, &daNpc_So_c::cutEquipProc},
        {&daNpc_So_c::cutUnequipStart, &daNpc_So_c::cutUnequipProc},
        {&daNpc_So_c::cutEatesaStart, &daNpc_So_c::cutEatesaProc},
        {&daNpc_So_c::cutEatesaFirstStart, &daNpc_So_c::cutEatesaFirstProc},
        {&daNpc_So_c::cutJumpMapopenStart, &daNpc_So_c::cutJumpMapopenProc},
        {&daNpc_So_c::cutMiniGameStart, &daNpc_So_c::cutMiniGameProc},
        {&daNpc_So_c::cutTurnStart, &daNpc_So_c::cutTurnProc},
        {&daNpc_So_c::cutMiniGameWarpStart, &daNpc_So_c::cutMiniGameWarpProc},
        {&daNpc_So_c::cutMiniGameReturnStart, &daNpc_So_c::cutMiniGameReturnProc},
        {&daNpc_So_c::cutPartnerShipStart, &daNpc_So_c::cutPartnerShipProc},
        {&daNpc_So_c::cutMiniGameWaitStart, &daNpc_So_c::cutMiniGameWaitProc},
        {&daNpc_So_c::cutMiniGameEndStart, &daNpc_So_c::cutMiniGameEndProc},
        {&daNpc_So_c::cutMiniGamePlTurnStart, &daNpc_So_c::cutMiniGamePlTurnProc},
        {&daNpc_So_c::cutMiniGamePlUpStart, &daNpc_So_c::cutMiniGamePlUpProc},
    };

    int actIdx = dComIfGp_evmng_getMyActIdx(mStaffIdx, action_table, ARRAY_SIZE(action_table), TRUE, 0);
    if (actIdx == -1) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
        return;
    }

    if (dComIfGp_evmng_getIsAddvance(mStaffIdx)) {
        (this->*cut_tbl[actIdx].start)();
    }
    (this->*cut_tbl[actIdx].proc)();
}

/* 000051E8-000051EC       .text cutSwimStart__10daNpc_So_cFv */
void daNpc_So_c::cutSwimStart() {}

/* 000051EC-00005364       .text cutSwimProc__10daNpc_So_cFv */
void daNpc_So_c::cutSwimProc() {
    cXyz look = mEventCut.getAttnPos();
    mSpeed = 7.0f + REG12_F(10);
    f32 dist = (current.pos - look).absXZ();
    s16 target = cLib_targetAngleY(&current.pos, &look);
    cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
    if (dist < 150.0f + REG12_F(7)) {
        mSpeed = 0.0f;
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00005364-000054A4       .text cutJumpStart__10daNpc_So_cFv */
void daNpc_So_c::cutJumpStart() {
    mTimer = 0xF;
    current.pos.y = dLib_getWaterY(current.pos, mAcch2);
    f32* speedY = dComIfGp_evmng_getMyFloatP(mStaffIdx, "Speed_y");
    f32* speedFP = dComIfGp_evmng_getMyFloatP(mStaffIdx, "SpeedF");
    shape_angle.x = l_HIO.m68;
    if (speedY != NULL) {
        speed.y = *speedY;
    } else {
        speed.y = 40.0f;
    }
    if (speedFP != NULL) {
        mSpeed = *speedFP;
    } else {
        mSpeed = 5.0f;
    }
    offsetSwim();
    speedF = mSpeed;
    setAnm(4, false);
    fopAcM_seStart(this, JA_SE_CM_SO_JUMP_L, 0);
}

/* 000054A4-00005598       .text cutJumpProc__10daNpc_So_cFv */
void daNpc_So_c::cutJumpProc() {
    f32 waterY = dLib_getWaterY(current.pos, mAcch2);
    if (cLib_calcTimer(&mTimer) == 0 && current.pos.y <= waterY) {
        fopAcM_seStart(this, JA_SE_CM_SO_LANDING_L, 0);
        fopKyM_createWpillar(&current.pos, 1.4f * scale.x, 1.4f, 0);
        mSpeed = 0.0f;
        speedF = mSpeed;
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00005598-00005610       .text cutAppearStart__10daNpc_So_cFv */
void daNpc_So_c::cutAppearStart() {
    fopAcM_seStart(this, JA_SE_CM_SO_RISE, 0);
    offsetAppear();
}

/* 00005610-00005668       .text cutAppearProc__10daNpc_So_cFv */
void daNpc_So_c::cutAppearProc() {
    if (std::fabsf(mB34 - mOffset.x) < 10.0f) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00005668-00005688       .text cutDiveStart__10daNpc_So_cFv */
void daNpc_So_c::cutDiveStart() {
    offsetDive();
}

/* 00005688-000056E0       .text cutDiveProc__10daNpc_So_cFv */
void daNpc_So_c::cutDiveProc() {
    if (std::fabsf(mB34 - mOffset.x) < 10.0f) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 000056E0-00005778       .text cutDisappearStart__10daNpc_So_cFv */
void daNpc_So_c::cutDisappearStart() {
    offsetDive();
    fopAcM_seStart(this, JA_SE_CM_SO_DIVE, 0);
    fopKyM_createWpillar(&current.pos, scale.x * 1.2f, 1.4f, 0);
}

/* 00005778-000057D0       .text cutDisappearProc__10daNpc_So_cFv */
void daNpc_So_c::cutDisappearProc() {
    if (std::fabsf(mB34 - mOffset.x) < 10.0f) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 000057D0-000058D0       .text cutSetAnmStart__10daNpc_So_cFv */
void daNpc_So_c::cutSetAnmStart() {
    char* name = dComIfGp_evmng_getMyStringP(mStaffIdx, "Name");
    if (name != NULL) {
        if (strcmp(name, "WAIT") == 0) {
            mType = 1;
        } else if (strcmp(name, "TALK") == 0) {
            mType = 3;
        } else if (strcmp(name, "JUMP") == 0) {
            mType = 4;
        } else if (strcmp(name, "SWIM") == 0) {
            mType = 2;
        }
    } else {
        mType = 1;
    }
}

/* 000058D0-0000599C       .text cutSetAnmProc__10daNpc_So_cFv */
void daNpc_So_c::cutSetAnmProc() {
    dComIfGp_evmng_getMyStringP(mStaffIdx, "Name");
    if (mType == 1 || mType == 4 || mType == 3 || mType == 2) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
    if (mpMorf2->isStop()) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 0000599C-00005AA8       .text cutEffectStart__10daNpc_So_cFv */
void daNpc_So_c::cutEffectStart() {
    cXyz pos = current.pos;
    pos.y -= 80.0f;
    cXyz particleScale;
    particleScale.setall(1.0f);
    JPABaseEmitter* pEmitter = dComIfGp_particle_set(dPa_name::ID_IT_SN_PF_BIKON00, &pos, NULL, &particleScale);
    pEmitter->setGlobalParticleScale(0.62f, 0.6f);
    fopAcM_seStart(this, JA_SE_CM_CMN_NOTICE, 0);
}

/* 00005AA8-00005ADC       .text cutEffectProc__10daNpc_So_cFv */
void daNpc_So_c::cutEffectProc() {
    dComIfGp_evmng_cutEnd(mStaffIdx);
}

/* 00005ADC-00005AE0       .text cutEquipStart__10daNpc_So_cFv */
void daNpc_So_c::cutEquipStart() {}

/* 00005AE0-00005B1C       .text cutEquipProc__10daNpc_So_cFv */
void daNpc_So_c::cutEquipProc() {
    mHudeFlag = 1;
    dComIfGp_evmng_cutEnd(mStaffIdx);
}

/* 00005B1C-00005B20       .text cutUnequipStart__10daNpc_So_cFv */
void daNpc_So_c::cutUnequipStart() {}

/* 00005B20-00005B5C       .text cutUnequipProc__10daNpc_So_cFv */
void daNpc_So_c::cutUnequipProc() {
    mHudeFlag = 0;
    dComIfGp_evmng_cutEnd(mStaffIdx);
}

/* 00005B5C-00005B98       .text cutEatesaStart__10daNpc_So_cFv */
void daNpc_So_c::cutEatesaStart() {
    offsetZero();
    mSpeed = 0.0f;
    speedF = 0.0f;
}

/* 00005B98-00005BF8       .text cutEatesaProc__10daNpc_So_cFv */
void daNpc_So_c::cutEatesaProc() {
    esa_class* esa = (esa_class*)fopAcIt_Judge(searchEsa_CB, this);
    if (esa != NULL) {
        esa->field_0x298 = 1;
    } else {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00005BF8-00005C6C       .text cutEatesaFirstStart__10daNpc_So_cFv */
void daNpc_So_c::cutEatesaFirstStart() {
    offsetZero();
    daShip_c* ship = dComIfGp_getShipActor();
    mCirclePath.mRadius = 650.0f + REG8_F(11);
    mCirclePath.mAngle = ship->shape_angle.y;
#if VERSION > VERSION_DEMO
    mBDA = 1;
    mSpeed = 0.0f;
    speedF = 0.0f;
#endif
}

/* 00005C6C-000060FC       .text cutEatesaFirstProc__10daNpc_So_cFv */
#if VERSION == VERSION_DEMO
void daNpc_So_c::cutEatesaFirstProc() {
    fopAc_ac_c* actor = dComIfGp_getLinkPlayer();
    daShip_c* ship = dComIfGp_getShipActor();
    cXyz pos = actor->current.pos;
    if (ship != NULL) {
        int tmp = ship->shape_angle.y + REG12_S(6);
        int ang = tmp - 0x4000;
        pos = ship->current.pos;
        f32 r = 260.0f;
        pos.x += r * cM_ssin(ang);
        pos.z += r * cM_scos(ang);
    }

    f32 dist = (current.pos - pos).absXZ();
    if (dist < 700.0f + REG8_F(11)) {
        cLib_addCalc2(&mCirclePath.mRadius, 30.0f, 0.05f, 8.0f + REG8_F(10));
    }
    mCirclePath.mAngleSpeed = 0x300;
    mCirclePath.mTranslation = pos;
    mCirclePath.mTranslation.y = dLib_getWaterY(mCirclePath.mTranslation, mAcch2);
    dLib_setCirclePath(&mCirclePath);
    mCirclePath.mPos.y = dLib_getWaterY(mCirclePath.mPos, mAcch2);
    cXyz path = mCirclePath.mPos;
    f32 step = (current.pos - path).absXZ();
    s16 target = cLib_targetAngleY(&mSoEyePos, &path);
    cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
    if (step < 100.0f + REG12_F(7)) {
        mSpeed = 0.0f;
        if (speedF < 1.0f) {
            esa_class* esa = (esa_class*)fopAcIt_Judge(searchEsa_CB, this);
            if (esa != NULL) {
                esa->field_0x298 = 1;
            } else {
                dComIfGp_evmng_cutEnd(mStaffIdx);
            }
        }
    } else if (step < 200.0f + REG12_F(7)) {
        mSpeed = 12.0f;
    } else {
        mSpeed = 20.0f + REG12_F(10);
    }
}
#else
void daNpc_So_c::cutEatesaFirstProc() {
    fopAc_ac_c* actor = dComIfGp_getLinkPlayer();
    daShip_c* ship = dComIfGp_getShipActor();
    cXyz pos = actor->current.pos;
    if (ship != NULL) {
        s16 ang = ship->shape_angle.y - 0x4000 + REG12_S(6);
        pos = ship->current.pos;
        pos.x += 260.0f * cM_ssin(ang);
        pos.z += 260.0f * cM_scos(ang);
    }

    f32 dist = (current.pos - pos).absXZ();
    if (dist < 700.0f + REG8_F(11)) {
        cLib_addCalc2(&mCirclePath.mRadius, 30.0f, 0.05f, 8.0f + REG8_F(10));
    }
    mCirclePath.mAngleSpeed = 0x300;
    mCirclePath.mTranslation = pos;
    mCirclePath.mTranslation.y = dLib_getWaterY(mCirclePath.mTranslation, mAcch2);
    dLib_setCirclePath(&mCirclePath);
    mCirclePath.mPos.y = dLib_getWaterY(mCirclePath.mPos, mAcch2);
    cXyz path = mCirclePath.mPos;
    path.y -= 100.0f;

    f32 step = (current.pos - path).absXZ();
    f32 dist2 = (current.pos - pos).absXZ();

    mEventCut.setAttnFlag(false);
    mEventCut.setAttnPos(path);
    mLookAt = path;

    if (dist2 < 100.0f + REG12_F(7)) {
        step = 2.0f;
        if (dist2 <= 50.0f) {
            step = 0.0f;
            esa_class* esa = (esa_class*)fopAcIt_Judge(searchEsa_CB, this);
            if (esa != NULL) {
                esa->field_0x298 = 1;
            } else {
                dComIfGp_evmng_cutEnd(mStaffIdx);
                mBDA = false;
                mB04 = step;
            }
        }
    } else if (step < 200.0f + REG12_F(7)) {
        step = 12.0f;
    } else {
        step = 20.0f + REG12_F(10);
    }

    if (step) {
        s16 target = cLib_targetAngleY(&mSoEyePos, &path);
        cLib_addCalcAngleS2(&shape_angle.y, target, 3, 0x1200);
    }
    cLib_addCalcPos2(&current.pos, path, 0.1f, step);
    current.pos.y = dLib_getWaterY(current.pos, mAcch2) - 100.0f;
}
#endif

/* 000060FC-00006148       .text cutJumpMapopenStart__10daNpc_So_cFv */
void daNpc_So_c::cutJumpMapopenStart() {
    mBBC = 0;
    offsetZero();
    setAnm(2, false);
    mBDA = 1;
}

/* 00006148-000064B8       .text cutJumpMapopenProc__10daNpc_So_cFv */
void daNpc_So_c::cutJumpMapopenProc() {
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship == NULL) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
        return;
    }

    if (mBBC == 0) {
        mB34 = mOffset.x;
        gravity = l_HIO.m80;
        speed.y = l_HIO.m84;
        mB00 = speed.y;
        speedF = 0.0f;
        mSpeed = speedF;
        shape_angle.x = l_HIO.m68;
        fopAcM_seStart(this, JA_SE_CM_SO_JUMP_DRAW, 0);
        mBBC++;
    } else if (mBBC == 1) {
        s16 ang = ship->shape_angle.y + 0x4000;
        mBB0 = ship->current.pos;
        f32 dist = -l_HIO.m88;
        mBB0.x += dist * cM_ssin(ang);
        mBB0.z += dist * cM_scos(ang);
        f32 savedY = current.pos.y;
        if (speed.y < 1.0) {
            speed.y = 1.0f;
        }
        if (cLib_chasePosXZ(&current.pos, mBB0, l_HIO.m90)) {
            current.pos.y = savedY;
            if (l_HIO.m94 == 0) {
                g_dComIfG_gameInfo.play.mFmapOpen = 2;
            }
            shape_angle.y += 0x8000;
            mBBC++;
        }
    } else if (mBBC == 2) {
        s16 ang = ship->shape_angle.y + 0x4000;
        mBB0 = ship->current.pos;
        f32 dist = -l_HIO.m8C;
        mBB0.x += dist * cM_ssin(ang);
        mBB0.z += dist * cM_scos(ang);
        f32 savedY = current.pos.y;
        if (cLib_chasePosXZ(&current.pos, mBB0, l_HIO.m90)) {
            current.pos.y = savedY;
            fopAcM_seStart(this, JA_SE_CM_SO_DIVE, 0);
            fopKyM_createWpillar(&current.pos, 1.4f * scale.x, 1.4f, 0);
            mSpeed = 0.0f;
            speedF = 0.0f;
            speed.y = 0.0f;
            mB00 = speed.y;
            dComIfGp_evmng_cutEnd(mStaffIdx);
            offsetDive();
            mBDA = false;
        }
    }
}

/* 000064B8-00006710       .text cutMiniGameStart__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameStart() {
    mB78 = 0;
    mB80 = 0;
    mB7C = 0;
    mB74 = 0;
    offsetDive();
    dComIfGp_startMiniGame(8);
    JAIZelBasic::getInterface()->seStart(JA_SE_START_WHISTLE);
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    if (REG8_S(1) == 0) {
#if VERSION == VERSION_DEMO
        f32 centerDist = 100.0f;
        f32 eyeDist = -200.0f;
        mBCC = player->current.pos;
        mBCC.x += centerDist * cM_ssin(player->shape_angle.y);
        mBCC.z += centerDist * cM_scos(player->shape_angle.y);
        mBCC.y += 50.0f;
        mBC0 = player->current.pos;
        mBC0.x += eyeDist * cM_ssin(player->shape_angle.y);
        mBC0.z += eyeDist * cM_scos(player->shape_angle.y);
        mBC0.y += 100.0f;
#else
        mBCC = player->current.pos;
        mBCC.x += 100.0f * cM_ssin(player->shape_angle.y);
        mBCC.z += 100.0f * cM_scos(player->shape_angle.y);
        mBCC.y += 50.0f;
        mBC0 = player->current.pos;
        mBC0.x += -200.0f * cM_ssin(player->shape_angle.y);
        mBC0.z += -200.0f * cM_scos(player->shape_angle.y);
        mBC0.y += 100.0f;
#endif
        camera->mCamera.Reset(mBCC, mBC0);
        camera->mCamera.Start();
    }
    player->changeOriginalDemo();
    player->changeDemoMode(daPy_demo_c::DEMO_BOW_MINIGAME_e);
    mBDB = 0;
    mBDE = 0;
}

/* 00006710-00006FE4       .text cutMiniGameProc__10daNpc_So_cFv */
#if VERSION == VERSION_DEMO
void daNpc_So_c::cutMiniGameProc() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    s16 ang = player->shape_angle.y;
    cXyz pos = player->current.pos;
    bool finished = false;
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship != NULL) {
        int tmp = ship->shape_angle.y + REG12_S(6);
        ang = tmp - 0x4000;
        pos = ship->current.pos;
    }

    if (player->checkArrowShoot()) {
        mB78++;
    }
    if (mB78 >= 10) {
        finished = true;
        if (mB74 <= 6) {
            dComIfGp_endMiniGame(8);
            speedF = 0.0f;
            mSpeed = 0.0f;
            speed.y = 0.0f;
            mB00 = 0.0f;
            dComIfGp_evmng_cutEnd(mStaffIdx);
            return;
        }
    }

    switch (mB74) {
    case 0:
        setAnm(2, false);
        mTimer = 0x1E;
        mB84 = 0;
        mB74++;
        mSpeed = 0.0f;
        speedF = mSpeed;
        offsetSwim();
        break;
    case 1:
        if (cLib_calcTimer(&mTimer) == 0) {
            mB74++;
        }
        break;
    case 2: {
        setAnm(2, false);
        f32 mag = 1000.0f;
        mB8C = cM_rndF(400.0f) + 1000.0f;
        mB88 = -mag / 2.0f + cM_rndF(mag);
        cXyz offset(mB88, 0.0f, mB8C);
        cXyz out(0.0f, 0.0f, 0.0f);
        mDoMtx_YrotS(mDoMtx_stack_c::now, ang);
        PSMTXMultVec(mDoMtx_stack_c::now, &offset, &out);
        out += pos;
        mEventCut.setAttnPos(out);
        mB74++;
        offsetSwim();
        break;
    }
    case 3: {
        cXyz attn = mEventCut.getAttnPos();
        mSpeed = 12.0f + REG12_F(10);
        f32 dist = (current.pos - attn).absXZ();
        s16 target = cLib_targetAngleY(&current.pos, &attn);
        cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
        if (dist < 150.0f + REG12_F(7)) {
            mSpeed = 0.0f;
            mB74++;
        }
        break;
    }
    case 4: {
        f32 mag = 300.0f;
        mB88 += -mag / 2.0f + cM_rndF(mag);
        cXyz offset(mB88, 0.0f, mB8C);
        cXyz out(0.0f, 0.0f, 0.0f);
        mDoMtx_YrotS(mDoMtx_stack_c::now, ang);
        PSMTXMultVec(mDoMtx_stack_c::now, &offset, &out);
        out += pos;
        mEventCut.setAttnPos(out);
        mSpeed = 0.0f;
        speedF = mSpeed;
        mTimer = 0x1E;
        mB74++;
        offsetSwim();
        break;
    }
    case 5: {
        cXyz attn = mEventCut.getAttnPos();
        mSpeed = 0.0f;
        f32 dist = (current.pos - attn).absXZ();
        s16 target = cLib_targetAngleY(&current.pos, &attn);
        cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
        if (cLib_calcTimer(&mTimer) == 0) {
            mB74++;
        }
        break;
    }
    case 6:
        current.pos.y = dLib_getWaterY(current.pos, mAcch2);
        mSpeed = mB08 * (cM_rndF(5.0f) + 5.0f);
        speedF = mSpeed;
        speed.y = 18.0f * mB08 + 4.0f * speedF;
        if (speed.y > l_HIO.m50) {
            speed.y = l_HIO.m50;
        }
        gravity = -0.8f;
        setAnm(4, false);
        shape_angle.x = l_HIO.m68;
        offsetZero();
        mB74++;
        fopAcM_seStart(this, JA_SE_CM_SO_JUMP_L, 0);
        break;
    case 7: {
        if (mB84 == 0) {
            setCollision(70.0f, 140.0f);
            mSph.SetR(l_HIO.m3C);
            mSph.SetC(current.pos);
            dComIfG_Ccsp()->Set(&mSph);
            if (checkTgHit()) {
                mB7C++;
            }
        }
        if (current.pos.y <= dLib_getWaterY(current.pos, mAcch2)) {
            fopAcM_seStart(this, JA_SE_CM_SO_LANDING_L, 0);
            fopKyM_createWpillar(&current.pos, 1.4f * scale.x, 1.4f, 0);
            mSpeed = 0.0f;
            speedF = mSpeed;
            mB80++;
            if (mB80 >= 10 || finished) {
                player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
                player->changeDemoParam0(1);
                JAIZelBasic::getInterface()->seStart(JA_SE_END_WHISTLE);
                initCam();
                moveCam();
                dComIfGp_endMiniGame(8);
                speedF = 0.0f;
                mSpeed = 0.0f;
                dComIfGp_evmng_cutEnd(mStaffIdx);
            } else {
                mB74 = 0;
            }
        }
        break;
    }
    }
}
#else
void daNpc_So_c::cutMiniGameProc() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    s16 ang = player->shape_angle.y;
    cXyz pos = player->current.pos;
    bool finished = false;
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship != NULL) {
        ang = ship->shape_angle.y - 0x4000 + REG12_S(6);
        pos = ship->current.pos;
    }

    if (player->checkArrowShoot()) {
        mB78++;
    }
    if (mB78 >= 10) {
        finished = true;
        if (mB74 <= 6) {
            player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
            player->changeDemoParam0(1);
            initCam();
            moveCam();
            JAIZelBasic::getInterface()->seStart(JA_SE_END_WHISTLE);
            dComIfGp_endMiniGame(8);
            speedF = 0.0f;
            mSpeed = 0.0f;
            dComIfGp_evmng_cutEnd(mStaffIdx);
            return;
        }
    }

    switch (mB74) {
    case 0:
        setAnm(2, false);
        mTimer = 0x1E;
        mB84 = 0;
        mB74++;
        mSpeed = 0.0f;
        speedF = mSpeed;
        offsetSwim();
        break;
    case 1:
        if (cLib_calcTimer(&mTimer) == 0) {
            mB74++;
        }
        break;
    case 2: {
        setAnm(2, false);
        f32 mag = 1000.0f;
        mB8C = cM_rndF(400.0f) + mag;
        mB88 = -mag / 2.0f + cM_rndF(mag);
        cXyz offset(mB88, 0.0f, mB8C);
        cXyz out(0.0f, 0.0f, 0.0f);
        mDoMtx_YrotS(mDoMtx_stack_c::now, ang);
        PSMTXMultVec(mDoMtx_stack_c::now, &offset, &out);
        out += pos;
        mEventCut.setAttnPos(out);
        mB74++;
        offsetSwim();
        break;
    }
    case 3: {
        cXyz attn = mEventCut.getAttnPos();
        mSpeed = 12.0f + REG12_F(10);
        f32 dist = (current.pos - attn).absXZ();
        s16 target = cLib_targetAngleY(&current.pos, &attn);
        cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
        if (dist < 150.0f + REG12_F(7)) {
            mSpeed = 0.0f;
            mB74++;
        }
        break;
    }
    case 4: {
        f32 mag = 300.0f;
        mB88 += -mag / 2.0f + cM_rndF(mag);
        cXyz offset(mB88, 0.0f, mB8C);
        cXyz out(0.0f, 0.0f, 0.0f);
        mDoMtx_YrotS(mDoMtx_stack_c::now, ang);
        PSMTXMultVec(mDoMtx_stack_c::now, &offset, &out);
        out += pos;
        mEventCut.setAttnPos(out);
        mSpeed = 0.0f;
        speedF = mSpeed;
        mTimer = 0x1E;
        mB74++;
        offsetSwim();
        break;
    }
    case 5: {
        cXyz attn = mEventCut.getAttnPos();
        mSpeed = 0.0f;
        f32 dist = (current.pos - attn).absXZ();
        s16 target = cLib_targetAngleY(&current.pos, &attn);
        cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
        if (cLib_calcTimer(&mTimer) == 0) {
            mB74++;
        }
        break;
    }
    case 6:
        current.pos.y = dLib_getWaterY(current.pos, mAcch2);
        mSpeed = mB08 * (cM_rndF(5.0f) + 5.0f);
        speedF = mSpeed;
        speed.y = 18.0f * mB08 + 4.0f * speedF;
        if (speed.y > l_HIO.m50) {
            speed.y = l_HIO.m50;
        }
        gravity = -0.8f;
        setAnm(4, false);
        shape_angle.x = l_HIO.m68;
        offsetZero();
        mB74++;
        fopAcM_seStart(this, JA_SE_CM_SO_JUMP_L, 0);
        break;
    case 7: {
        f32 waterY = dLib_getWaterY(current.pos, mAcch2);
        if (mB84 == 0 && mB7C < 10) {
            if (mHitTimer == 0) {
                if (current.pos.y >= waterY + 50.0f) {
                    mSph.SetR(l_HIO.m3C);
                    mSph.SetC(current.pos);
                    dComIfG_Ccsp()->Set(&mSph);
                } else {
                    cXyz far(0.0f, 30000.0f, 0.0f);
                    mSph.SetR(0.0f);
                    mSph.SetC(far);
                    dComIfG_Ccsp()->Set(&mSph);
                }
            }
            if (checkTgHit()) {
                mB7C++;
                if (mB7C >= 10) {
                    mB7C = 10;
                }
            }
        }
        if (current.pos.y <= waterY) {
            fopAcM_seStart(this, JA_SE_CM_SO_LANDING_L, 0);
            fopKyM_createWpillar(&current.pos, 1.4f * scale.x, 1.4f, 0);
            mSpeed = 0.0f;
            speedF = mSpeed;
            mB80++;
            if (mB80 >= 10 || finished) {
                player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
                player->changeDemoParam0(1);
                initCam();
                moveCam();
                JAIZelBasic::getInterface()->seStart(JA_SE_END_WHISTLE);
                dComIfGp_endMiniGame(8);
                speedF = 0.0f;
                mSpeed = 0.0f;
                dComIfGp_evmng_cutEnd(mStaffIdx);
            } else {
                mB74 = 0;
            }
        }
        break;
    }
    }
}
#endif

/* 00006FE4-00006FF0       .text cutTurnStart__10daNpc_So_cFv */
void daNpc_So_c::cutTurnStart() {
    mTimer = 0x1E;
}

/* 00006FF0-00007114       .text cutTurnProc__10daNpc_So_cFv */
void daNpc_So_c::cutTurnProc() {
    daShip_c* ship = dComIfGp_getShipActor();
    if (ship == NULL) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
        return;
    }

    s16 ang = ship->shape_angle.y + 0x4000;
    cXyz pos = ship->current.pos;
#if VERSION == VERSION_DEMO
    f32 r = -300.0f;
    pos.x += r * cM_ssin(ang);
    pos.z += r * cM_scos(ang);
#else
    pos.x += -300.0f * cM_ssin(ang);
    pos.z += -300.0f * cM_scos(ang);
#endif
    s16 target = cLib_targetAngleY(&current.pos, &pos);
    cLib_addCalcAngleS2(&shape_angle.y, target, 8, 0x400);
    int dist = cLib_distanceAngleS(shape_angle.y, target);
    if (cLib_calcTimer(&mTimer) == 0 || dist <= 0x400) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00007114-00007324       .text cutMiniGameWarpStart__10daNpc_So_cFv */
#if VERSION == VERSION_DEMO
void daNpc_So_c::cutMiniGameWarpStart() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    player->changeOriginalDemo();
    daShip_c* ship = dComIfGp_getShipActor();
    mBA0 = ship->current.pos;
    mBAC = ship->shape_angle.y;
    ship->initStartPos(&mMinigamePos, mMinigameAngle + 0x4000);
    csXyz shipShape = ship->shape_angle;
    s16 playerAng = mMinigameAngle + REG8_S(6);
    player->setPlayerPosAndAngle(&mMinigamePos, playerAng);
    mSpeed = 0.0f;
    speedF = mSpeed;
    int tmp = ship->shape_angle.y + REG12_S(6);
    int ang = tmp - 0x4000;
    f32 r = 300.0f;
    current.pos = mMinigamePos;
    current.pos.x += r * cM_ssin(ang);
    current.pos.z += r * cM_scos(ang);
    mBDB = 1;
    mTimer = REG12_S(1) + 0xA;
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    camera->mCamera.Reset(mBCC, mBC0);
    camera->mCamera.Start();
}
#else
void daNpc_So_c::cutMiniGameWarpStart() {
#if VERSION > VERSION_JPN
    JAIZelBasic::getInterface()->field_0x00bf = 1;
#endif
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    player->changeOriginalDemo();
    daShip_c* const ship = dComIfGp_getShipActor();
    mMinigamePos.y = dLib_getWaterY(mMinigamePos, mAcch2);
    mBA0 = ship->current.pos;
    mBAC = ship->shape_angle.y;
    s16 shipAng = mMinigameAngle + 0x4000;
    ship->initStartPos(&mMinigamePos, shipAng);
    csXyz shipShape = ship->shape_angle;
    s16 playerAng = mMinigameAngle + REG8_S(6);
    player->setPlayerPosAndAngle(&mMinigamePos, playerAng);
    mSpeed = 0.0f;
    speedF = mSpeed;
    s16 ang = ship->shape_angle.y - 0x4000 + REG12_S(6);
    current.pos = mMinigamePos;
    current.pos.x += 300.0f * cM_ssin(ang);
    current.pos.z += 300.0f * cM_scos(ang);
    mBDB = 1;
    mTimer = REG12_S(1) + 0xA;
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    camera->mCamera.Reset(mBCC, mBC0);
    camera->mCamera.Start();
}
#endif

/* 00007324-00007438       .text cutMiniGameWarpProc__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameWarpProc() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    player->changeOriginalDemo();
    daShip_c* ship = dComIfGp_getShipActor();
    player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
    player->changeDemoParam0(1);
#if VERSION == VERSION_DEMO
    int tmp = ship->shape_angle.y + REG12_S(6);
    int ang = tmp - 0x4000;
    f32 r = 300.0f;
    current.pos = ship->current.pos;
    current.pos.x += r * cM_ssin(ang);
    current.pos.z += r * cM_scos(ang);
#else
    s16 ang = ship->shape_angle.y - 0x4000 + REG12_S(6);
    current.pos = ship->current.pos;
    current.pos.x += 300.0f * cM_ssin(ang);
    current.pos.z += 300.0f * cM_scos(ang);
#endif
    if (cLib_calcTimer(&mTimer) == 0) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00007438-00007524       .text cutMiniGameReturnStart__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameReturnStart() {
#if VERSION > VERSION_JPN
    JAIZelBasic::getInterface()->field_0x00bf = 0;
#endif
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    player->changeOriginalDemo();
    daShip_c* ship = (daShip_c*)fopAcM_SearchByName(fpcNm_SHIP_e);
    s16 playerAng = mMinigameAngle + REG8_S(6);
    player->setPlayerPosAndAngle(&mBA0, playerAng);
    ship->initStartPos(&mBA0, mBAC);
    mSpeed = 0.0f;
    speedF = mSpeed;
    mBDB = 1;
    setAnm(2, false);
    offsetDive();
}

/* 00007524-0000781C       .text cutMiniGameReturnProc__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameReturnProc() {
    daShip_c* ship = dComIfGp_getShipActor();
#if VERSION == VERSION_DEMO
    int tmp = ship->shape_angle.y + REG12_S(6);
    int ang = tmp - 0x4000;
    f32 r = 300.0f;
    current.pos = ship->current.pos;
    current.pos.x += r * cM_ssin(ang);
    current.pos.z += r * cM_scos(ang);
#else
    s16 ang = ship->shape_angle.y - 0x4000 + REG12_S(6);
    current.pos = ship->current.pos;
    current.pos.x += 300.0f * cM_ssin(ang);
    current.pos.z += 300.0f * cM_scos(ang);
#endif
    shape_angle.x = 0;
    f32 dist = (mBA0 - current.pos).absXZ();
    if (dist < 400.0f) {
        daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
        player->changeOriginalDemo();
        player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
        player->changeDemoParam0(1);
        cXyz offset;
        offset.setall(0.0f);
        cXyz pos;
        pos.setall(0.0f);
#if VERSION == VERSION_DEMO
        cMtx_YrotS(*calc_mtx, player->shape_angle.y);
        offset.set(-50.0f + REG0_F(4), 50.0f + REG0_F(5), 100.0f + REG0_F(6));
#else
        mDoMtx_YrotS(*calc_mtx, player->shape_angle.y);
        offset.x = -50.0f + REG0_F(4);
        offset.y = 50.0f + REG0_F(5);
        offset.z = 100.0f + REG0_F(6);
#endif
        MtxPosition(&offset, &pos);
        mBC0 = player->current.pos + pos;
        mBCC = player->current.pos;
        mBCC.y += 90.0f + REG0_F(7);
        camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
        camera->mCamera.Reset(mBCC, mBC0);
        camera->mCamera.Start();
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 0000781C-00007820       .text cutPartnerShipStart__10daNpc_So_cFv */
void daNpc_So_c::cutPartnerShipStart() {}

/* 00007820-00007878       .text cutPartnerShipProc__10daNpc_So_cFv */
void daNpc_So_c::cutPartnerShipProc() {
    daShip_c* ship = dComIfGp_getShipActor();
    dComIfGp_event_setTalkPartner(ship);
    dComIfGp_evmng_cutEnd(mStaffIdx);
}

/* 00007878-000078E0       .text cutMiniGameWaitStart__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameWaitStart() {
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    if (REG8_S(1) == 0) {
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(1);
    }
}

/* 000078E0-00007914       .text cutMiniGameWaitProc__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameWaitProc() {
    dComIfGp_evmng_cutEnd(mStaffIdx);
}

/* 00007914-00007934       .text cutMiniGameEndStart__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameEndStart() {
    mTimer = REG12_S(0) + 0x2D;
    mBDE = 0;
}

/* 00007934-0000798C       .text cutMiniGameEndProc__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGameEndProc() {
    initCam();
    moveCam();
    if (cLib_calcTimer(&mTimer) == 0) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 0000798C-000079AC       .text cutMiniGamePlTurnStart__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGamePlTurnStart() {
    mTimer = REG12_S(2) + 0xA;
    mBDE = 0;
}

/* 000079AC-00007AC0       .text cutMiniGamePlTurnProc__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGamePlTurnProc() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    player->changeOriginalDemo();
    daShip_c* ship = dComIfGp_getShipActor();
    player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
    player->changeDemoParam0(1);
#if VERSION == VERSION_DEMO
    int tmp = ship->shape_angle.y + REG12_S(6);
    int ang = tmp - 0x4000;
    f32 r = 300.0f;
    current.pos = ship->current.pos;
    current.pos.x += r * cM_ssin(ang);
    current.pos.z += r * cM_scos(ang);
#else
    s16 ang = ship->shape_angle.y - 0x4000 + REG12_S(6);
    current.pos = ship->current.pos;
    current.pos.x += 300.0f * cM_ssin(ang);
    current.pos.z += 300.0f * cM_scos(ang);
#endif
    if (cLib_calcTimer(&mTimer) == 0) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00007AC0-00007B1C       .text cutMiniGamePlUpStart__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGamePlUpStart() {
    mTimer = 0x1E;
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    camera->mCamera.Stop();
    camera->mCamera.SetTrimSize(1);
}

/* 00007B1C-00007CBC       .text cutMiniGamePlUpProc__10daNpc_So_cFv */
void daNpc_So_c::cutMiniGamePlUpProc() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getLinkPlayer();
    player->changeDemoMode(daPy_demo_c::DEMO_N_TALK_e);
    cXyz offset;
    offset.setall(0.0f);
    cXyz pos;
    pos.setall(0.0f);
#if VERSION == VERSION_DEMO
    cMtx_YrotS(*calc_mtx, player->shape_angle.y);
    offset.set(-50.0f + REG0_F(4), 50.0f + REG0_F(5), 100.0f + REG0_F(6));
#else
    mDoMtx_YrotS(*calc_mtx, player->shape_angle.y);
    offset.x = -50.0f + REG0_F(4);
    offset.y = 50.0f + REG0_F(5);
    offset.z = 100.0f + REG0_F(6);
#endif
    MtxPosition(&offset, &pos);
    mBC0 = player->current.pos + pos;
    mBCC = player->current.pos;
    mBCC.y += 90.0f + REG0_F(7);
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    camera->mCamera.Set(mBCC, mBC0);
    if (cLib_calcTimer(&mTimer) == 0) {
        dComIfGp_evmng_cutEnd(mStaffIdx);
    }
}

/* 00007CBC-00007E74       .text initCam__10daNpc_So_cFv */
void daNpc_So_c::initCam() {
    if (mBDE == 1) {
        return;
    }
    mBDE = 1;
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    if (REG8_S(1) == 0) {
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(1);
        fopAc_ac_c* actor = dComIfGp_getLinkPlayer();
#if VERSION == VERSION_DEMO
        f32 centerDist = 100.0f;
        f32 eyeDist = -200.0f;
        mBCC = actor->current.pos;
        mBCC.x += centerDist * cM_ssin(actor->shape_angle.y);
        mBCC.z += centerDist * cM_scos(actor->shape_angle.y);
        mBCC.y += 50.0f;
        mBC0 = actor->current.pos;
        mBC0.x += eyeDist * cM_ssin(actor->shape_angle.y);
        mBC0.z += eyeDist * cM_scos(actor->shape_angle.y);
        mBC0.y += 100.0f;
#else
        mBCC = actor->current.pos;
        mBCC.x += 100.0f * cM_ssin(actor->shape_angle.y);
        mBCC.z += 100.0f * cM_scos(actor->shape_angle.y);
        mBCC.y += 50.0f;
        mBC0 = actor->current.pos;
        mBC0.x += -200.0f * cM_ssin(actor->shape_angle.y);
        mBC0.z += -200.0f * cM_scos(actor->shape_angle.y);
        mBC0.y += 100.0f;
#endif
    }
}

/* 00007E74-00008010       .text moveCam__10daNpc_So_cFv */
void daNpc_So_c::moveCam() {
    if (REG8_S(1) != 0) {
        return;
    }
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    fopAc_ac_c* actor = dComIfGp_getLinkPlayer();
    cXyz eye;
#if VERSION == VERSION_DEMO
    f32 centerDist = 150.0f;
    f32 eyeDist = -400.0f;
    cXyz center = actor->current.pos;
    center.x += centerDist * cM_ssin(actor->shape_angle.y);
    center.z += centerDist * cM_scos(actor->shape_angle.y);
    center.y += 100.0f;
    eye = actor->current.pos;
    eye.x += eyeDist * cM_ssin(actor->shape_angle.y);
    eye.z += eyeDist * cM_scos(actor->shape_angle.y);
    eye.y += 300.0f;
#else
    cXyz center = actor->current.pos;
    center.x += 150.0f * cM_ssin(actor->shape_angle.y);
    center.z += 150.0f * cM_scos(actor->shape_angle.y);
    center.y += 100.0f;
    eye = actor->current.pos;
    eye.x += -400.0f * cM_ssin(actor->shape_angle.y);
    eye.z += -400.0f * cM_scos(actor->shape_angle.y);
    eye.y += 300.0f;
#endif
    cLib_addCalcPos2(&mBC0, eye, 0.05f, 5.0f);
    cLib_addCalcPos2(&mBCC, center, 0.05f, 5.0f);
    camera->mCamera.Set(mBCC, mBC0);
}
