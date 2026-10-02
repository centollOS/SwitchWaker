// The original name of this file is unknown, but it must exist because d_a_npc_p2's "cut" functions
// are in a separate .text section from the rest of its functions.

/* 00005238-00005528       .text cutProc__10daNpc_P2_cFv */
void daNpc_P2_c::cutProc() {
    static char* action_table[15] = {
        "TALK",
        "RIDE_SWITCH",
        "RUN_WAIT",
        "JUMP_TO_LIFT",
        "LIFT_TO_ROPE",
        "ROPE_TALK",
        "ROPE_TO_LIFT",
        "JUMP_TO_GOAL",
        "SET_ANM",
        "JUMP",
        "SW_ON",
        "SW_OFF",
        "SURPRISE",
        "OMAMORI_INIT",
        "OMAMORI_END",
    };

    int staffIdx = dComIfGp_evmng_getMyStaffId("P2b", NULL, 0);
    if (staffIdx == -1) {
        m7D7 = 0;
        return;
    }

    int actIdx = dComIfGp_evmng_getMyActIdx(staffIdx, action_table, ARRAY_SSIZE(action_table), TRUE, 0);
    if (actIdx == -1) {
        m7D7 = 0;
        dComIfGp_evmng_cutEnd(staffIdx);
        return;
    }

    m7D7 = 1;
    if (dComIfGp_evmng_getIsAddvance(staffIdx)) {
        m72C = 0;
        switch (actIdx) {
        case 0:
            cutTalkStart(staffIdx);
            break;
        case 1:
            cutRideSwitchStart(staffIdx);
            break;
        case 2:
            cutRunWaitStart(staffIdx);
            break;
        case 3:
            cutJumpToLiftStart(staffIdx);
            break;
        case 4:
            cutLiftToRopeStart(staffIdx);
            break;
        case 5:
            cutRopeTalkStart(staffIdx);
            break;
        case 6:
            cutRopeToLiftStart(staffIdx);
            break;
        case 7:
            cutJumpToGoalStart(staffIdx);
            break;
        case 8:
            cutSetAnmStart(staffIdx);
            break;
        case 9:
            cutJumpStart(staffIdx);
            break;
        case 10:
            cutSwOnStart(staffIdx);
            break;
        case 11:
            cutSwOffStart(staffIdx);
            break;
        case 12:
            cutSurpriseStart(staffIdx);
            break;
        case 13:
            cutOmamoriInitStart(staffIdx);
            break;
        case 14:
            cutOmamoriEndStart(staffIdx);
            break;
        }
    }

    switch (actIdx) {
    case 0:
        cutTalkProc(staffIdx);
        break;
    case 1:
        cutRideSwitchProc(staffIdx);
        break;
    case 2:
        cutRunWaitProc(staffIdx);
        break;
    case 3:
        cutJumpToLiftProc(staffIdx);
        break;
    case 4:
        cutLiftToRopeProc(staffIdx);
        break;
    case 5:
        cutRopeTalkProc(staffIdx);
        break;
    case 6:
        cutRopeToLiftProc(staffIdx);
        break;
    case 7:
        cutJumpToGoalProc(staffIdx);
        break;
    case 8:
        cutSetAnmProc(staffIdx);
        break;
    case 9:
        cutJumpProc(staffIdx);
        break;
    case 10:
        cutSwOnProc(staffIdx);
        break;
    case 11:
        cutSwOffProc(staffIdx);
        break;
    case 12:
        cutSurpriseProc(staffIdx);
        break;
    case 13:
        cutOmamoriInitProc(staffIdx);
        break;
    case 14:
        cutOmamoriEndProc(staffIdx);
        break;
    }
}

/* 00005528-000055DC       .text cutTalkStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutTalkStart(int staffIdx) {
    int* pMsgNum = dComIfGp_evmng_getMyIntegerP(staffIdx, "MsgNum");
    if (pMsgNum == NULL) {
        mMsgNo = 0;
    } else {
        mMsgNo = *pMsgNum;
    }
    int* pAttn = dComIfGp_evmng_getMyIntegerP(staffIdx, "Attention");
    if (pAttn == NULL) {
        m72C = 0;
    } else {
        m72C = 1;
    }
    talkInit();
}

/* 000055DC-0000562C       .text cutTalkProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutTalkProc(int staffIdx) {
    if (talk(true) == fopMsgStts_BOX_CLOSED_e) {
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

/* 0000562C-000056F4       .text cutRideSwitchStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutRideSwitchStart(int staffIdx) {
    f32* pSpeedY = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed_y");
    f32* pGravity = dComIfGp_evmng_getMyFloatP(staffIdx, "Gravity");
    if (pSpeedY == NULL) {
        speed.y = 16.0f;
    } else {
        speed.y = *pSpeedY;
    }
    if (pGravity == NULL) {
        gravity = -2.0f;
    } else {
        gravity = *pGravity;
    }
    m72C = 1;
    mAnmNo = 9;
}

/* 000056F4-0000589C       .text cutRideSwitchProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutRideSwitchProc(int staffIdx) {
    fopAc_ac_c* actor = fopAcM_SearchByName(fpcNm_Obj_Swpush_e);
    s16 other = fopAcM_searchActorAngleY(this, actor);
    f32 max_step = 24.0f;
    cXyz local_28 = actor->current.pos;
    f32 local_34 = (local_28 - current.pos).absXZ();

    if (mAnmNo != 10) {
        cLib_addCalc2(&current.pos.x, local_28.x, 0.1f, max_step);
        cLib_addCalc2(&current.pos.z, local_28.z, 0.1f, max_step);
    }
    if (mAcch.ChkGroundLanding()) {
        m7F4 = 0x11;
        mAnmNo = 0xA;
    }
    if (!cLib_calcTimer(&m7F4) && mAnmNo == 0xA) {
        speed.y = 0.0f;
        gravity = -9.0f;
        mAnmNo = 1;
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

/* 0000589C-00005914       .text cutRunWaitStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutRunWaitStart(int staffIdx) {
    int* pTimer = dComIfGp_evmng_getMyIntegerP(staffIdx, "Timer");
    if (pTimer == NULL) {
        m7F4 = 0;
    } else {
        m7F4 = (s16)*pTimer;
    }
    mAnmNo = 6;
    m72C = 1;
}

/* 00005914-000059B0       .text cutRunWaitProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutRunWaitProc(int staffIdx) {
    mSavedPos = current.pos;
    mSavedAngle = current.angle;
    if (mAnmNo == 6) {
        if (mpMorf->isStop()) {
            dComIfGp_evmng_cutEnd(staffIdx);
        }
    }
}

/* 000059B0-00005BD0       .text searchNearLift__10daNpc_P2_cFPvPv */
void* daNpc_P2_c::searchNearLift(void* i_actor, void* i_data) {
    daNpc_P2_c* search_actor = (daNpc_P2_c*)i_actor;
    daNpc_P2_c* i_this = (daNpc_P2_c*)i_data;
    if (fopAcM_IsActor(i_actor) && fopAcM_GetName(i_actor) == fpcNm_Obj_Hlift_e) {
        if (i_this->mClosestPos.abs() == 0.0f) {
            i_this->mClosestPos = search_actor->current.pos;
        } else {
            f32 current_dist = (i_this->mClosestPos - i_this->current.pos).abs();
            f32 new_dist = (search_actor->current.pos - i_this->current.pos).abs();
            if (new_dist < current_dist) {
                i_this->mClosestPos = search_actor->current.pos;
                i_this->mActor = search_actor;
            }
        }
    }
    return NULL;
}

/* 00005BD0-00005CD4       .text cutJumpToLiftStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutJumpToLiftStart(int staffIdx) {
    f32* pSpeed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
    f32* pSpeedY = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed_y");
    f32* pGravity = dComIfGp_evmng_getMyFloatP(staffIdx, "Gravity");
    if (pSpeed == NULL) {
        m7FC = 10.0f;
    } else {
        m7FC = *pSpeed;
    }
    if (pSpeedY == NULL) {
        m7F8 = 15.0f;
    } else {
        m7F8 = *pSpeedY;
    }
    if (pGravity == NULL) {
        m800 = -2.5f;
    } else {
        m800 = *pGravity;
    }
    mAnmNo = 5;
    m72C = 1;
}

/* 00005CD4-00006064       .text cutJumpToLiftProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutJumpToLiftProc(int staffIdx) {
    fopAcM_Search(searchNearLift, this);
    f32 maxSpeed = m7FC;
    fopAc_ac_c* actor = mActor;
    cXyz local_2c = actor->current.pos;
    f32 dist_xz = (local_2c - current.pos).absXZ();
    if (mAcch.m_flags & dBgS_Acch::GROUND_AWAY) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_JUMP_UP, 0);
        speed.y = m7F8;
        gravity = m800;
        speedF = m7FC;
        mAnmNo = 8;
    } else {
        if (mAcch.ChkGroundHit() && (mAnmNo == 9 || mAnmNo == 10)) {
            speedF = 0.0f;
            m7DC.set(0.0f, -100.0f, 0.0f);
            mAnmNo = 10;
        } else {
            if (speed.y <= 0.0f && mAnmNo == 8) {
                m7DC.set(0.0f, -50.0f, 0.0f);
                mAnmNo = 9;
            }
        }
    }
    if (mAnmNo == 5) {
        smoke_set();
    } else if (mAcch.ChkGroundLanding()) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_LANDING, 0);
        m7F4 = 0x11;
        smoke_set();
    } else {
        mSmoke.end();
    }
    mSavedPos = current.pos;
    mSavedAngle = current.angle;
    if (mAcch.ChkGroundHit() && mAnmNo == 5) {
        speedF = maxSpeed * 2.0f;
    } else if (mAcch.ChkGroundHit() && (mAnmNo != 10 && mAnmNo != 1)) {
        cLib_addCalc2(&current.pos.x, local_2c.x, 0.1f, maxSpeed);
        cLib_addCalc2(&current.pos.z, local_2c.z, 0.1f, maxSpeed);
    } else {
        if (!cLib_calcTimer(&m7F4) && mAnmNo == 10) {
            mAnmNo = 1;
            m7DC.setall(0.0f);
            mClosestPos.setall(0.0f);
            speed.y = 0.0f;
            gravity = -9.0f;
            speedF = 0.0f;
            dComIfGp_evmng_cutEnd(staffIdx);
        }
    }
}

/* 00006064-00006284       .text searchNearRope__10daNpc_P2_cFPvPv */
void* daNpc_P2_c::searchNearRope(void* i_actor, void* i_data) {
    daNpc_P2_c* search_actor = (daNpc_P2_c*)i_actor;
    daNpc_P2_c* i_this = (daNpc_P2_c*)i_data;
    if (fopAcM_IsActor(i_actor) && fopAcM_GetName(i_actor) == fpcNm_HIMO3_e) {
        if (i_this->mClosestPos.abs() == 0.0f) {
            i_this->mClosestPos = search_actor->current.pos;
        } else {
            f32 current_dist = (i_this->mClosestPos - i_this->current.pos).abs();
            f32 new_dist = (search_actor->current.pos - i_this->current.pos).abs();
            if (new_dist < current_dist) {
                i_this->mClosestPos = search_actor->current.pos;
                i_this->mActor = search_actor;
            }
        }
    }
    return NULL;
}

/* 00006284-00006388       .text cutLiftToRopeStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutLiftToRopeStart(int staffIdx) {
    f32* pSpeed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
    f32* pSpeedY = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed_y");
    f32* pGravity = dComIfGp_evmng_getMyFloatP(staffIdx, "Gravity");
    if (pSpeed == NULL) {
        m7FC = 10.0f;
    } else {
        m7FC = *pSpeed;
    }
    if (pSpeedY == NULL) {
        m7F8 = 15.0f;
    } else {
        m7F8 = *pSpeedY;
    }
    if (pGravity == NULL) {
        m800 = -2.5f;
    } else {
        m800 = *pGravity;
    }
    mAnmNo = 5;
    m72C = 1;
}

/* 00006388-000065B8       .text cutLiftToRopeProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutLiftToRopeProc(int staffIdx) {
    fopAcM_Search(searchNearRope, this);
    f32 speed_f = m7FC;
    cXyz ropePos = mActor->current.pos;
    f32 dist = (ropePos - current.pos).absXZ();
    if (mAcch.m_flags & dBgS_Acch::GROUND_AWAY) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_JUMP_UP, 0);
        speed.y = m7F8;
        gravity = m800;
        speedF = m7FC;
        mAnmNo = 8;
    }

    if (mAnmNo == 5) {
        smoke_set();
    } else {
        mSmoke.end();
    }

    if (mAcch.ChkGroundHit()) {
        speedF = speed_f * 2.0f;
    }
    if (mCyl.ChkCoHit()) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_GRAB, 0);
        gravity = 0.0f;
        speed.y = 0.0f;
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

/* 000065B8-00006B08       .text cutRopeTalkStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutRopeTalkStart(int staffIdx) {
    cXyz local_70;
    cXyz local_64;
    cXyz local_58;

    int* msgNum = dComIfGp_evmng_getMyIntegerP(staffIdx, "MsgNum");
    if (msgNum == NULL) {
        mMsgNo = 0;
    } else {
        mMsgNo = *msgNum;
    }

    if (dComIfGp_evmng_getMyIntegerP(staffIdx, "Attention") == NULL) {
        m72C = 0;
    } else {
        m72C = 1;
    }

    mAnmNo = 0xC;
    talkInit();

    m7C0 = 1;

    local_70.set(REG12_F(0), REG12_F(1), REG12_F(2));
    fopAcM_Search(searchNearRope, this);
    mpHimo3 = (himo3_class*)mActor;
    local_64 = mActor->current.pos - current.pos;
    m798 = local_64.abs();
    m79C = std::sqrtf(2.0f / m798);
    f32 fVar12 = M_PI / 2.0f / m79C;
    m7A8 = 0x2EE0;
    f32 fVar1 = cM_ssin(current.angle.y);
    f32 fVar2 = cM_scos(current.angle.y);
    local_58.x = fVar2 * local_64.x - fVar1 * local_64.z;
    local_58.y = local_64.y;
    local_58.z = fVar1 * local_64.x + fVar2 * local_64.z;
    s16 sVar4 = cM_atan2s(-local_58.z, local_58.y);
    if (sVar4 > m7A8) {
        sVar4 = m7A8;
    } else if (sVar4 < -m7A8) {
        sVar4 = -m7A8;
    }

    f32 dVar8 = (f32)sVar4 / m7A8;
    m7A0 = cM_atan2f(dVar8, std::sqrtf(1.0f - (dVar8 * dVar8))) / m79C;
    m7AA = cM_atan2s(-local_58.x, std::sqrtf(local_58.y * local_58.y + local_58.z * local_58.z));
    if (m7AA > 0) {
        m7A4 = fVar12;
    } else {
        m7A4 = -fVar12;
    }

    himo3_class* himo3 = mpHimo3;
    local_64 = himo3->actor.current.pos - current.pos;
    dVar8 = (local_64.abs() - 80.0f);
    if (dVar8 > himo3->m15FC) {
        dVar8 = himo3->m15FC;
    }
    local_64.normalize();
    current.pos = (mpHimo3->actor.current.pos - (local_64 * dVar8));

    speed.y = 0.0f;
    gravity = 0.0f;

    mSoundPos = mpHimo3->actor.current.pos;
    m7AC = 0;
}

/* 00006B08-00007314       .text cutRopeTalkProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutRopeTalkProc(int staffIdx) {
    J3DModel* model_p = mpMorf->getModel();
    himo3_class* rope = mpHimo3;
    fopAcM_SetParam(mpHimo3, 2);
    mSoundPos = mpHimo3->actor.current.pos;
    cXyz cStack_b4;
    cXyz cStack_c0;
    cXyz local_cc;
    dLib_getPosFromMtx(model_p->getAnmMtx(20), &cStack_b4);
    f32 angle = m7A0 * m79C;
    f32 vel = m7A4 * m79C;
    mDoMtx_stack_c::ZrotS(-current.angle.z);
    mDoMtx_stack_c::XrotM(-current.angle.x);
    mDoMtx_stack_c::YrotM(-current.angle.y);
    mDoMtx_stack_c::transM(-current.pos.x, -current.pos.y, -current.pos.z);
    mDoMtx_stack_c::multVec(&cStack_b4, &cStack_c0);

    f32 delta;
    if (0.0f <= angle && angle <= M_PI / 2.0f) {
        delta = -0.45f * cM_fsin(angle);
    } else if (angle <= -M_PI / 2.0f) {
        delta = 0.45f * cM_fsin(angle);
    } else {
        delta = 0.0f;
    }
    m7A0 += delta + 1.0f;

    if (0.0f <= vel && vel <= M_PI / 2.0f) {
        delta = -0.45f * cM_fsin(vel);
    } else if (vel <= -M_PI / 2.0f) {
        delta = 0.45f * cM_fsin(vel);
    } else {
        delta = 0.0f;
    }
    m7A4 += delta + 1.0f;

    f32 dVar14 = m7A0 * m79C;
    f32 dVar13_o = m7A4 * m79C;

    if (dVar14 >= (f32)M_PI) {
        dVar14 = dVar14 - 2.0f * M_PI;
        m7A0 = dVar14 / m79C;
    }
    if (dVar13_o >= M_PI) {
        dVar13_o = dVar13_o - 2.0f * M_PI;
        m7A4 = dVar13_o / m79C;
    }
    cLib_chaseS(&m7AA, 0, 0x20);
    m7A8 += std::fabsf(cM_fcos(std::fabsf(dVar14))) * 64.0f;
    if (m7A8 > 12000) {
        m7A8 = 12000;
    }

    s16 iVar1 = (-m7A8 * cM_fsin(dVar14));
    s16 iVar2 = (m7AA * cM_fsin(dVar13_o));

    s16 target = (-m7A8 * cM_fsin(dVar14 - DEG_TO_RAD(36.0f)));
    cLib_addCalcAngleS(&current.angle.x, target, 8, 0xC00, 0x100);

    f32 dVarCos = cM_fcos(dVar13_o);

    cLib_addCalcAngleS(
        &current.angle.z,
        m7AA * cM_fsin(dVar13_o - DEG_TO_RAD(63.0f)),
        8,
        dVarCos * 2048.0f + 1024.0f,
        dVarCos * 128.0f + 128.0f
    );

    local_cc.set(0.0f, -m798, 0.0f);
    mDoMtx_stack_c::transS(mSoundPos.x, mSoundPos.y, mSoundPos.z);
    mDoMtx_stack_c::ZXYrotM(iVar1, current.angle.y, iVar2);

    mDoMtx_stack_c::multVec(&local_cc, &current.pos);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(-m7A8 * cM_fsin(dVar14 + m79C), current.angle.y, iVar2);
    cXyz cStack_d8;
    mDoMtx_stack_c::multVec(&cStack_c0, &cStack_d8);
    if (m7A4 > 0.0f && m7A0 > 0.0f && mAnmNo == 0xB) {
        mDoAud_seStart(0x201E, &mSoundPos);
    }
    if (talk(true) == fopMsgStts_BOX_CLOSED_e && m7A4 > 0.0f && m7A0 > 0.0f && mAnmNo == 0xB) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_JUMP_DOWN, 0);
        mAnmNo = 9;
        m7C0 = 0;
        fopAcM_SetParam(rope, 3);
        dComIfGp_evmng_cutEnd(staffIdx);
    } else {
        if ((m7A4 > 0.0f) && (m7A0 > 0.0f)) {
            mAnmNo = 0xC;
            return;
        }
        if ((m7A4 > 0.0f) && (m7A0 < 0.0f)) {
            mAnmNo = 0xB;
            return;
        }
        if ((m7A4 < 0.0f) && (m7A0 > 0.0f)) {
            mAnmNo = 0xB;
            return;
        }
        if ((m7A4 < 0.0f) && (m7A0 < 0.0f)) {
            mAnmNo = 0xC;
        }
    }
}

/* 00007314-0000743C       .text cutRopeToLiftStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutRopeToLiftStart(int staffIdx) {
    f32* pSpeed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
    f32* pSpeedY = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed_y");
    f32* pGravity = dComIfGp_evmng_getMyFloatP(staffIdx, "Gravity");
    if (pSpeed == NULL) {
        m7FC = 10.0f;
    } else {
        m7FC = *pSpeed;
    }
    if (pSpeedY == NULL) {
        m7F8 = 6.0f;
    } else {
        m7F8 = *pSpeedY;
    }
    if (pGravity == NULL) {
        m800 = -0.5f;
    } else {
        m800 = *pGravity;
    }
    current.angle.x = 0;
    current.angle.z = 0;
    m72C = 1;
    mAnmNo = 9;
    speed.y = m7F8;
    gravity = m800;
    speedF = m7FC;
}

/* 0000743C-00007694       .text cutRopeToLiftProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutRopeToLiftProc(int staffIdx) {
    fopAcM_Search(searchNearLift, this);
    cXyz ropePos = mActor->current.pos;
    f32 dist = (ropePos - current.pos).absXZ();
    mSavedPos = current.pos;
    mSavedAngle = current.angle;
    if (mAcch.ChkGroundLanding()) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_LANDING, 0);
        m7F4 = 0x11;
        smoke_set();
    } else {
        mSmoke.end();
    }

    if (mAcch.ChkGroundHit() && (mAnmNo == 9 || mAnmNo == 10)) {
        speedF = 0.0f;
        speed.y = 0.0f;
        gravity = -9.0f;
        m7DC.set(0.0f, -100.0f, 0.0f);
        mAnmNo = 10;
    }
    if (cLib_calcTimer(&m7F4) == 0 && mAnmNo == 10) {
        mAnmNo = 1;
        m7DC.set(0.0f, 0.0f, 0.0f);
        mClosestPos.set(0.0f, 0.0f, 0.0f);
        mSmoke.end();
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

/* 00007694-00007808       .text cutJumpToGoalStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutJumpToGoalStart(int staffIdx) {
    f32* pSpeed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
    f32* pSpeedY = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed_y");
    f32* pGravity = dComIfGp_evmng_getMyFloatP(staffIdx, "Gravity");
    cXyz* pPos = dComIfGp_evmng_getMyXyzP(staffIdx, "Pos");
    if (pPos == NULL) {
        m7E8 = l_HIO.mChild[mType].m30;
    } else {
        m7E8 = *pPos;
    }
    if (pSpeed == NULL) {
        m7FC = 10.0f;
    } else {
        m7FC = *pSpeed;
    }
    if (pSpeedY == NULL) {
        m7F8 = 15.0f;
    } else {
        m7F8 = *pSpeedY;
    }
    if (pGravity == NULL) {
        m800 = -2.5f;
    } else {
        m800 = *pGravity;
    }
    mAnmNo = 5;
    m72C = 1;
}

/* 00007808-00007B3C       .text cutJumpToGoalProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutJumpToGoalProc(int staffIdx) {
    cXyz ropePos = m7E8;
    f32 dist = (ropePos - current.pos).absXZ();
    f32 maxSpeed = m7FC;
    if (mAcch.m_flags & dBgS_Acch::GROUND_AWAY) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_JUMP_UP, 0);
        speed.y = m7F8;
        gravity = m800;
        speedF = m7FC;
        mAnmNo = 8;
    }

    if (mAcch.ChkGroundHit()) {
        if (mAnmNo != 10 && mAnmNo != 1) {
            cLib_addCalc2(&current.pos.x, ropePos.x, 0.1f, maxSpeed);
            cLib_addCalc2(&current.pos.z, ropePos.z, 0.1f, maxSpeed);
        }
    }
    if (mAcch.ChkGroundHit() && (mAnmNo == 9 || mAnmNo == 10)) {
        m7DC.set(0.0f, -100.0f, 0.0f);
        mAnmNo = 10;
    } else {
        if (speed.y <= 0.0f) {
            if (mAnmNo == 8) {
                m7DC.set(0.0f, -50.0f, 0.0f);
                mAnmNo = 9;
            }
        }
    }
    if (mAcch.ChkGroundLanding()) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_LANDING, 0);
        m7F4 = 0x11;
        speedF = 0.0f;
        smoke_set();
    } else {
        mSmoke.end();
    }
    mSavedPos = current.pos;
    mSavedAngle = current.angle;
    if (!cLib_calcTimer(&m7F4) && mAnmNo == 10) {
        m7DC.set(0.0f, 0.0f, 0.0f);
        speed.y = 0.0f;
        gravity = -9.0f;
        speedF = 0.0f;
        dComIfGp_evmng_cutEnd(staffIdx);
        mAnmNo = 1;
    }
}

/* 00007B3C-00007D08       .text cutJumpStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutJumpStart(int staffIdx) {
    f32* pSpeed = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed");
    f32* pSpeedY = dComIfGp_evmng_getMyFloatP(staffIdx, "Speed_y");
    f32* pGravity = dComIfGp_evmng_getMyFloatP(staffIdx, "Gravity");
    cXyz* pPos = dComIfGp_evmng_getMyXyzP(staffIdx, "Pos");
    if (pPos == NULL) {
        m7E8 = l_HIO.mChild[mType].m30;
    } else {
        m7E8 = *pPos;
    }
    if (pSpeed == NULL) {
        m7FC = 10.0f;
    } else {
        m7FC = *pSpeed;
    }
    if (pSpeedY == NULL) {
        m7F8 = 15.0f;
    } else {
        m7F8 = *pSpeedY;
    }
    if (pGravity == NULL) {
        m800 = -2.5f;
    } else {
        m800 = *pGravity;
    }
    m72C = 1;
    fopAcM_monsSeStart(this, JA_SE_CV_NK_JUMP_UP, 0);
    speed.y = m7F8;
    gravity = m800;
    speedF = m7FC;
    mAnmNo = 8;
}

/* 00007D08-00007F70       .text cutJumpProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutJumpProc(int staffIdx) {
    cXyz goalPos = m7E8;
    f32 dist = (goalPos - current.pos).absXZ();

    if (mAcch.ChkGroundHit() && (mAnmNo == 9 || mAnmNo == 10)) {
        m7DC.set(0.0f, -100.0f, 0.0f);
        mAnmNo = 10;
    } else if (speed.y <= 0.0f && mAnmNo == 8) {
        m7DC.set(0.0f, -50.0f, 0.0f);
        mAnmNo = 9;
    }

    if (mAcch.ChkGroundLanding()) {
        fopAcM_monsSeStart(this, JA_SE_CV_NK_LANDING, 0);
        m7F4 = 0x11;
        speedF = 0.0f;
        smoke_set();
    } else {
        mSmoke.end();
    }

    mSavedPos = current.pos;
    mSavedAngle = current.angle;

    if (cLib_calcTimer(&m7F4) == 0 && mAnmNo == 10) {
        m7DC.set(0.0f, 0.0f, 0.0f);
        speed.y = 0.0f;
        gravity = -9.0f;
        speedF = 0.0f;
        dComIfGp_evmng_cutEnd(staffIdx);
        mAnmNo = 1;
    }
}

/* 00007F70-000080B4       .text cutSetAnmStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutSetAnmStart(int staffIdx) {
    char* name = dComIfGp_evmng_getMyStringP(staffIdx, "Name");
    if (name != NULL && strcmp(name, "KYORO") == 0) {
        mAnmNo = 0x11;
    } else if (name != NULL && strcmp(name, "SURPRISE") == 0) {
        mAnmNo = 7;
    } else if (name != NULL && strcmp(name, "THINK") == 0) {
        mAnmNo = 0x12;
    } else if (name != NULL && strcmp(name, "NOD") == 0) {
        mAnmNo = 0x13;
    } else if (name != NULL && strcmp(name, "WAIT01") == 0) {
        mAnmNo = 1;
    } else {
        mAnmNo = 1;
    }
}

/* 000080B4-00008114       .text cutSetAnmProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutSetAnmProc(int staffIdx) {
    if (mpMorf->isStop()) {
        dComIfGp_evmng_cutEnd(staffIdx);
    }
}

/* 00008114-00008154       .text cutSwOnStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutSwOnStart(int) {
    if (mSwitchNo != 0xFF) {
        fopAcM_onSwitch(this, mSwitchNo);
    }
}

/* 00008154-000081B4       .text cutSwOnProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutSwOnProc(int staffIdx) {
    if (mSwitchNo != 0xFF) {
        fopAcM_onSwitch(this, mSwitchNo);
    }
    dComIfGp_evmng_cutEnd(staffIdx);
}

/* 000081B4-000081F4       .text cutSwOffStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutSwOffStart(int) {
    if (mSwitchNo != 0xFF) {
        fopAcM_offSwitch(this, mSwitchNo);
    }
}

/* 000081F4-00008254       .text cutSwOffProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutSwOffProc(int staffIdx) {
    if (mSwitchNo != 0xFF) {
        fopAcM_offSwitch(this, mSwitchNo);
    }
    dComIfGp_evmng_cutEnd(staffIdx);
}

/* 00008254-000082CC       .text cutSurpriseStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutSurpriseStart(int) {
    mDoAud_seStart(JA_SE_ITM_OMAMORI_BLINK);
    daPy_getPlayerActorClass()->voiceStart(0x1C);
}

/* 000082CC-000082F8       .text cutSurpriseProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutSurpriseProc(int staffIdx) {
    dComIfGp_evmng_cutEnd(staffIdx);
}

/* 000082F8-00008350       .text cutOmamoriInitStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutOmamoriInitStart(int) {
    mDoAud_seStart(JA_SE_ITM_OMAMORI_TETLA);
}

/* 00008350-0000837C       .text cutOmamoriInitProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutOmamoriInitProc(int staffIdx) {
    dComIfGp_evmng_cutEnd(staffIdx);
}

/* 0000837C-000083D4       .text cutOmamoriEndStart__10daNpc_P2_cFi */
void daNpc_P2_c::cutOmamoriEndStart(int) {
    mDoAud_seStart(JA_SE_ITM_OMAMORI_ED_TALK);
}

/* 000083D4-00008400       .text cutOmamoriEndProc__10daNpc_P2_cFi */
void daNpc_P2_c::cutOmamoriEndProc(int staffIdx) {
    dComIfGp_evmng_cutEnd(staffIdx);
}
