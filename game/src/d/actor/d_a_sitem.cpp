// Hanging item and simulated vine.

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_sitem.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_cc_uty.h"
#include "d/d_bg_s_gnd_chk.h"
#include "res/Object/Sitem.h"

static cXyz non_pos;
static f32 size_d[10] = {10.0f, 10.0f, 9.5f, 9.0f, 8.5f, 8.0f, 7.5f, 7.0f, 6.5f, 6.5f};

/* 000000EC-000001FC       .text hand_draw__FP11sitem_class */
static void hand_draw(sitem_class* i_this) {
    if (i_this->mMode < 6) {
        J3DModel* model = i_this->mpModel;
        g_env_light.setLightTevColorType(model, &i_this->tevStr);
        mDoExt_modelUpdateDL(model);
    }
    i_this->mLine.update(10, (GXColor){0, 180, 50, 255}, &i_this->tevStr);
    dComIfGd_set3DlineMat(&i_this->mLine);
    if (i_this->mCutLength > 0.1f) {
        i_this->mCutLine.update(5, (GXColor){0, 180, 50, 255}, &i_this->tevStr);
        dComIfGd_set3DlineMat(&i_this->mCutLine);
    }
}

/* 000001FC-00000248       .text daSitem_Draw__FP11sitem_class */
static BOOL daSitem_Draw(sitem_class* i_this) {
    g_env_light.settingTevStruct(0, &i_this->eyePos, &i_this->tevStr);
    hand_draw(i_this);
    return TRUE;
}

/* 00000248-0000034C       .text hand_mtx_set__FP11sitem_class */
static void hand_mtx_set(sitem_class* i_this) {
    MtxTrans(i_this->mModelPos.x, i_this->mModelPos.y, i_this->mModelPos.z, false);
    cMtx_XrotM(*calc_mtx, i_this->mModelAngle.x);
    cMtx_YrotM(*calc_mtx, i_this->mModelAngle.y);
    cMtx_XrotM(*calc_mtx, REG12_S(1) + 0x4000);
    cXyz offset;
    offset.set(0.0f, -60.0f + REG12_F(4), 0.0f);
    MtxPosition(&offset, &i_this->mCollisionPos);
    MtxTrans(0.0f, -50.0f + REG12_F(3), 0.0f, true);
    J3DModel* model = i_this->mpModel;
    model->setBaseTRMtx(*calc_mtx);
}

/* 00000388-00000410       .text control3__FP11sitem_class */
static void control3(sitem_class* i_this) {
    sitem_s* segment = i_this->mSegments;
    for (int i = 0; i < 10; i++, segment++) {
        f32 size = 0.8f + 0.1f * cM_ssin(i_this->mFrame * 500 + i * 100);
        segment->size = size * size_d[i];
    }
}

/* 00000410-00000748       .text control1__FP11sitem_class */
static void control1(sitem_class* i_this) {
    static f32 g_d[10] = {50.0f, 50.0f, 35.0f, 25.0f, 15.0f, 9.0f, 6.0f, 6.0f, 6.0f, 6.0f};
    int i;
    f32 x, y, z;
    f32 influence;
    cXyz offset, wave, extension, worldWave;
    i_this->mSegments[0].pos = i_this->current.pos;
    sitem_s* segment = &i_this->mSegments[1];
    cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
    cMtx_XrotM(*calc_mtx, i_this->current.angle.x);
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->mExtension;
    MtxPosition(&offset, &extension);
    offset.z = i_this->mSegmentLength;
    f32 amplitude = i_this->mWave;
    for (i = 1; i < 9; i++, segment++) {
        wave.x = amplitude * cM_ssin(i_this->mFrame * (REG0_S(5) + 1100) + i * (REG0_S(6) + 4000));
        wave.y = g_d[i];
        wave.z = amplitude * cM_scos(i_this->mFrame * (REG0_S(7) + 800) + i * (REG0_S(8) + 4000));
        MtxPosition(&wave, &worldWave);
        influence = 1.0f;
        x = worldWave.x * influence + (segment->pos.x - segment[-1].pos.x + extension.x * influence);
        y = worldWave.y * influence + (segment->pos.y - segment[-1].pos.y + extension.y * influence);
        z = worldWave.z * influence + (segment->pos.z - segment[-1].pos.z + extension.z * influence);
        int yaw = (s16)cM_atan2s(x, z);
        int pitch = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        MtxPush();
        mDoMtx_YrotS(*calc_mtx, yaw);
        mDoMtx_XrotM(*calc_mtx, pitch);
        MtxPosition(&offset, &wave);
        MtxPull();
        segment->pos = segment[-1].pos + wave;
    }
}

/* 00000748-000009E8       .text control2__FP11sitem_class */
static void control2(sitem_class* i_this) {
    int i;
    s16 pitch, yaw;
    f32 x, y, z;
    cXyz offset, direction;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->mSegmentLength;
    i_this->mSegments[9].pos = i_this->mPos;
    sitem_s* segment = &i_this->mSegments[8];
    for (i = 8; i >= 1; i--, segment--) {
        x = segment->pos.x - segment[1].pos.x;
        y = segment->pos.y - segment[1].pos.y;
        z = segment->pos.z - segment[1].pos.z;
        yaw = (s16)cM_atan2s(x, z);
        pitch = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        mDoMtx_YrotS(*calc_mtx, yaw);
        mDoMtx_XrotM(*calc_mtx, pitch);
        offset.z = i_this->mSegmentLength;
        MtxPosition(&offset, &direction);
        segment->pos = segment[1].pos + direction;
    }
    i_this->mModelPos = i_this->mSegments[9].pos;
    offset = i_this->mSegments[8].pos - i_this->mSegments[9].pos;
    i_this->mModelAngle.x = -cM_atan2s(offset.y, offset.z);
    i_this->mModelAngle.y = cM_atan2s(offset.x, std::sqrtf(offset.y * offset.y + offset.z * offset.z));
    hand_mtx_set(i_this);
}

/* 000009E8-00000E2C       .text cut_control1__FP11sitem_class */
static void cut_control1(sitem_class* i_this) {
    int i;
    f32 x, y, z;
    cXyz offset, wave, worldWave;
    i_this->mCutSegments[0].pos = i_this->current.pos;
    sitem_s* segment = &i_this->mCutSegments[1];
    cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
    cMtx_XrotM(*calc_mtx, i_this->current.angle.x);
    offset.z = i_this->mCutLength;
    f32 amplitude = 50.0f + REG0_F(18);
    for (i = 1; i < 5; i++, segment++) {
        wave.x = amplitude * cM_ssin(i_this->mFrame * (REG0_S(5) + 4100) + i * (REG0_S(6) + 10000));
        wave.y = 50.0f + REG0_F(17);
        wave.z = amplitude * cM_scos(i_this->mFrame * (REG0_S(7) + 4400) + i * (REG0_S(8) + 10000));
        MtxPosition(&wave, &worldWave);
        x = worldWave.x + (segment->pos.x - segment[-1].pos.x);
        y = worldWave.y + (segment->pos.y - segment[-1].pos.y);
        z = worldWave.z + (segment->pos.z - segment[-1].pos.z);
        int yaw = (s16)cM_atan2s(x, z);
        int pitch = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        MtxPush();
        mDoMtx_YrotS(*calc_mtx, yaw);
        mDoMtx_XrotM(*calc_mtx, pitch);
        MtxPosition(&offset, &wave);
        MtxPull();
        segment->pos = segment[-1].pos + wave;
    }
#if VERSION == VERSION_DEMO
    if (i_this->mpSmokeEmitter[0] == NULL && i_this->mSmokeActive) {
        i_this->mSmoke[0].remove();
        i_this->mpSmokeEmitter[0] = dComIfGp_particle_set(0x8184, &i_this->mCutSegments[4].pos, &i_this->mSmokeAngle[0], NULL, 255, &i_this->mSmoke[0], fopAcM_GetRoomNo(i_this));
        i_this->mHitTimer = 60;
    } else {
        offset = i_this->mCutSegments[4].pos - i_this->mCutSegments[3].pos;
        i_this->mSmokeAngle[0].y = cM_atan2s(offset.x, offset.z);
        i_this->mSmokeAngle[0].x = -cM_atan2s(offset.y, std::sqrtf(offset.x * offset.x + offset.z * offset.z));
        if (i_this->mHitTimer == 1 && i_this->mpSmokeEmitter[0]) {
            i_this->mpSmokeEmitter[0]->becomeInvalidEmitter();
            i_this->mpSmokeEmitter[0] = NULL;
        }
    }
#else
    if (i_this->mSmoke[0].getEmitter() == NULL && i_this->mSmokeActive) {
        i_this->mSmoke[0].remove();
        dComIfGp_particle_set(0x8184, &i_this->mCutSegments[4].pos, &i_this->mSmokeAngle[0], NULL, 255, &i_this->mSmoke[0], fopAcM_GetRoomNo(i_this));
        i_this->mHitTimer = 60;
    } else {
        offset = i_this->mCutSegments[4].pos - i_this->mCutSegments[3].pos;
        i_this->mSmokeAngle[0].y = cM_atan2s(offset.x, offset.z);
        i_this->mSmokeAngle[0].x = -cM_atan2s(offset.y, std::sqrtf(offset.x * offset.x + offset.z * offset.z));
        if (i_this->mHitTimer == 1 && i_this->mSmoke[0].getEmitter()) i_this->mSmoke[0].remove();
    }
#endif
}

/* 00000E2C-00001058       .text my_break__FP11sitem_class */
static void my_break(sitem_class* i_this) {
    i_this->mTimers[0] = 50;
    i_this->mMode = 6;
    csXyz angle(0, i_this->current.angle.y, 0);
    fopAcM_createItemFromTable(&i_this->mPos, i_this->mItemTable, i_this->mItemBit & 0x7F, fopAcM_GetHomeRoomNo(i_this), 0, &angle, 1, NULL);
    if (i_this->mType <= 1) {
        dComIfGp_particle_set(0x8168, &i_this->mPos);
        dComIfGp_particle_set(0x8167, &i_this->mPos, NULL, NULL, 255, NULL, -1, &i_this->tevStr.mColorK0, &i_this->tevStr.mColorK0);
        if (i_this->mType == 1) dComIfGp_particle_set(0x8169, &i_this->mPos, NULL, NULL, 255, NULL, -1, &i_this->tevStr.mColorK0, &i_this->tevStr.mColorK0);
    }
    if (i_this->mType == 2) dComIfGp_particle_set(0x816A, &i_this->mPos, NULL, NULL, 255, NULL, -1, &i_this->tevStr.mColorK0, &i_this->tevStr.mColorK0);
#if VERSION > VERSION_DEMO
    fopAcM_seStart(i_this, JA_SE_OBJ_BREAK_CHAIR, 0);
#endif
    fopAcM_create(fpcNm_Obj_Eff_e, 4, &i_this->mPos, -1, NULL, NULL, -1, NULL);
}

/* 00001094-000015C0       .text cut_control2__FP11sitem_class */
static void cut_control2(sitem_class* i_this) {
    int i;
    f32 x, y, z;
    cXyz offset, direction, wave;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->mSegmentLength;
    i_this->mSegments[9].pos = i_this->mPos;
    sitem_s* segment = &i_this->mSegments[8];
    f32 amplitude = i_this->mWave;
    for (i = 8; i >= 0; i--, segment--) {
        wave.x = amplitude * cM_ssin(i_this->mFrame * (REG0_S(5) + 2500) + i * (REG0_S(6) + 3000));
        wave.y = amplitude * cM_ssin(i_this->mFrame * (REG0_S(5) + 2950) + i * (REG0_S(6) + 4000));
        wave.z = amplitude * cM_scos(i_this->mFrame * (REG0_S(7) + 2800) + i * (REG0_S(8) + 3500));
        x = wave.x + (segment->pos.x - segment[1].pos.x);
        f32 height = segment->pos.y - 10.0f + wave.y;
        if (height < 5.0f + i_this->mGroundY) height = 5.0f + i_this->mGroundY;
        y = height - segment[1].pos.y;
        z = wave.z + (segment->pos.z - segment[1].pos.z);
        int yaw = (s16)cM_atan2s(x, z);
        int pitch = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        mDoMtx_YrotS(*calc_mtx, yaw);
        mDoMtx_XrotM(*calc_mtx, pitch);
        offset.z = i_this->mSegmentLength;
        MtxPosition(&offset, &direction);
        segment->pos = segment[1].pos + direction;
    }
    i_this->mModelPos = i_this->mSegments[9].pos;
    offset = i_this->mSegments[8].pos - i_this->mSegments[9].pos;
    i_this->mModelAngle.x = -cM_atan2s(offset.y, offset.z);
    i_this->mModelAngle.y = cM_atan2s(offset.x, std::sqrtf(offset.y * offset.y + offset.z * offset.z));
    hand_mtx_set(i_this);
#if VERSION == VERSION_DEMO
    if (i_this->mpSmokeEmitter[1] == NULL && i_this->mSmokeActive) {
        i_this->mSmoke[1].remove();
        i_this->mpSmokeEmitter[1] = dComIfGp_particle_set(0x8184, &i_this->mSegments[0].pos, &i_this->mSmokeAngle[1], NULL, 255, &i_this->mSmoke[1], fopAcM_GetRoomNo(i_this));
    } else {
        offset = i_this->mSegments[0].pos - i_this->mSegments[1].pos;
        i_this->mSmokeAngle[1].y = cM_atan2s(offset.x, offset.z);
        i_this->mSmokeAngle[1].x = -cM_atan2s(offset.y, std::sqrtf(offset.x * offset.x + offset.z * offset.z));
        if (i_this->mHitTimer == 1 && i_this->mpSmokeEmitter[1]) {
            i_this->mpSmokeEmitter[1]->becomeInvalidEmitter();
            i_this->mpSmokeEmitter[1] = NULL;
            i_this->mSmokeActive = false;
        }
    }
#else
    if (i_this->mSmoke[1].getEmitter() == NULL && i_this->mSmokeActive) {
        i_this->mSmoke[1].remove();
        dComIfGp_particle_set(0x8184, &i_this->mSegments[0].pos, &i_this->mSmokeAngle[1], NULL, 255, &i_this->mSmoke[1], fopAcM_GetRoomNo(i_this));
    } else {
        offset = i_this->mSegments[0].pos - i_this->mSegments[1].pos;
        i_this->mSmokeAngle[1].y = cM_atan2s(offset.x, offset.z);
        i_this->mSmokeAngle[1].x = -cM_atan2s(offset.y, std::sqrtf(offset.x * offset.x + offset.z * offset.z));
        if (i_this->mHitTimer == 1 && i_this->mSmoke[1].getEmitter()) {
            i_this->mSmoke[1].remove();
            i_this->mSmokeActive = false;
        }
    }
#endif
}

static f32 hr_d[4] = {10.0f, 25.0f, 40.0f, 65.0f};
static f32 max_d[4] = {100.0f, 250.0f, 400.0f, 600.0f};

/* 000015C0-00002304       .text hand_move__FP11sitem_class */
static void hand_move(sitem_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    // Unused, but its dead load leaves &g_dComIfG_gameInfo hoisted into a register as in the target.
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
#endif
    f32 approach, maxSpeed, extension, length, lengthStep, wave;
    cXyz offset, direction, target, center;
    dBgS_GndChk ground;
    bool turn = false;
    bool cut = false;
    approach = 0.1f;
    maxSpeed = 8.0f;
    extension = 0.0f;
    lengthStep = 1.0f;
    wave = 5.0f + REG0_F(11);
    length = REG0_F(5) + hr_d[i_this->mLengthType];
    f32 maxLength = max_d[i_this->mLengthType];
    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    switch (i_this->mMode) {
    case 0:
    case 1:
        offset.x = (25.0f + REG0_F(13)) * cM_ssin(i_this->mFrame * 600);
        offset.y = maxLength;
        offset.z = (25.0f + REG0_F(13)) * cM_ssin(i_this->mFrame * 700);
        MtxPosition(&offset, &direction);
        target = actor->current.pos + direction;
        if (i_this->mMode == 0) {
            i_this->mPos = target;
            i_this->mMode = 1;
        }
        break;
    case 5:
        wave = 50.0f;
        i_this->mWave = wave;
        cut = true;
        length = 25.0f;
        i_this->mOldPos = i_this->mPos;
        i_this->mPos += actor->speed;
        actor->speed.y -= 3.0f;
        if (actor->speed.y < -90.0f) actor->speed.y = -90.0f;
        i_this->mAcch.CrrPos(*dComIfG_Bgsp());
        i_this->mHitTimer = 5;
        if (actor->speed.y < 0.0f) {
            Vec pos;
            pos.x = i_this->mPos.x;
            pos.y = i_this->mPos.y;
            pos.z = i_this->mPos.z;
            pos.y += 100.0f;
            ground.SetPos(&pos);
            i_this->mGroundY = dComIfG_Bgsp()->GroundCross(&ground);
            if (i_this->mGroundY == -G_CM3D_F_INF || i_this->mPos.y <= 30.0f + i_this->mGroundY) {
                i_this->mPos.y = 30.0f + i_this->mGroundY;
                my_break(i_this);
            }
        }
        break;
    case 6:
        cut = true;
        wave = 0.0f;
        i_this->mHitTimer = 10;
        if (i_this->mTimers[0] < 40) {
            lengthStep = 0.05f * length;
            length = 0.0f;
        }
        if (i_this->mTimers[0] == 0) fopAcM_delete(actor);
        break;
    }
    cLib_addCalc2(&i_this->mSegmentLength, length, 0.5f, lengthStep);
    cLib_addCalc2(&i_this->mExtension, extension, 1.0f, 0.2f);
    cLib_addCalc2(&i_this->mWave, wave, 1.0f, 1.5f);
    if (!cut) {
        cLib_addCalc2(&actor->speedF, maxSpeed, 1.0f, approach);
        if (i_this->mHitSpeed > 1.0f && i_this->mMode != 3) {
            cMtx_YrotS(*calc_mtx, i_this->mHitAngle);
            offset.x = 0.0f;
            offset.y = REG6_F(9);
            offset.z = i_this->mHitSpeed;
            MtxPosition(&offset, &direction);
            target += direction;
            approach = 0.1f;
            actor->speedF = 0.2f * i_this->mHitSpeed;
            if (actor->speedF > 30.0f + REG6_F(8)) actor->speedF = 30.0f + REG6_F(8);
        }
        cLib_addCalc0(&i_this->mHitSpeed, 1.0f, 5.0f + REG6_F(7));
        cLib_addCalc2(&i_this->mPos.x, target.x, approach, actor->speedF);
        cLib_addCalc2(&i_this->mPos.y, target.y, approach, actor->speedF);
        cLib_addCalc2(&i_this->mPos.z, target.z, approach, actor->speedF);
#if VERSION == VERSION_DEMO
        if (turn && actor->current.angle.x == 0) {
            cLib_addCalcAngleS2(&actor->current.angle.y, fopAcM_searchPlayerAngleY(actor), 0x10, 0x800);
        }
#endif
        control1(i_this);
        control2(i_this);
    } else {
        cut_control1(i_this);
        cut_control2(i_this);
        sitem_s* segment = i_this->mCutSegments;
        cXyz* pos = i_this->mCutLine.getPos(0);
        u8* size = i_this->mCutLine.getSize(0);
        for (int i = 0; i < 5; i++, segment++, pos++, size++) {
            *pos = segment->pos;
            *size = segment->size;
        }
        cLib_addCalc0(&i_this->mCutLength, 1.0f, 1.0f + REG0_F(1));
    }
    control3(i_this);
    sitem_s* segment = i_this->mSegments;
    cXyz* pos = i_this->mLine.getPos(0);
    u8* size = i_this->mLine.getSize(0);
    for (int i = 0; i < 10; i++, segment++, pos++, size++) {
        *pos = segment->pos;
        *size = segment->size;
    }
    pos = i_this->mLine.getPos(0);
    actor->eyePos = pos[5];
    actor->attention_info.position = actor->eyePos;
    i_this->mStts.Move();
    if (!cut) i_this->mBoomerangSph.SetC(actor->eyePos);
    else i_this->mBoomerangSph.SetC(non_pos);
    u8 hit = 0;
    dComIfG_Ccsp()->Set(&i_this->mBoomerangSph);
    for (int i = 0; i < 3; i++) {
        int index = (i_this->mFrame & 3) + i * 2;
        index %= 10;
        center = pos[index];
        if (!cut) i_this->mSph[i].SetC(center);
        else i_this->mSph[i].SetC(non_pos);
        if (i_this->mMode == 3) i_this->mSph[i].OffCoSetBit();
        else i_this->mSph[i].OnCoSetBit();
        dComIfG_Ccsp()->Set(&i_this->mSph[i]);
    }
    i_this->mSph[3].SetR(50.0f);
    i_this->mSph[3].SetC(i_this->mCollisionPos);
    i_this->mSph[3].OnCoSetBit();
    dComIfG_Ccsp()->Set(&i_this->mSph[3]);
    CcAtInfo atInfo;
    atInfo.mpObj = NULL;
    if (i_this->mHitTimer == 0) {
        if (i_this->mSph[3].ChkTgHit() || (i_this->mHitSpeed > 100.0f && i_this->mSph[3].ChkCoHit())) {
            atInfo.mpObj = i_this->mSph[3].GetTgHitObj();
            if (atInfo.mpObj && atInfo.mpObj->ChkAtType(AT_TYPE_GRAPPLING_HOOK)) return;
            my_break(i_this);
            hit = 3;
        } else {
            for (int i = 0; i < 3; i++) {
                if (i_this->mSph[i].ChkTgHit()) {
                    atInfo.mpObj = i_this->mSph[i].GetTgHitObj();
                    if (atInfo.mpObj && atInfo.mpObj->ChkAtType(AT_TYPE_GRAPPLING_HOOK)) return;
                    hit = 1;
                    break;
                }
            }
            if ((hit || i_this->mBoomerangSph.ChkTgHit()) && i_this->mHitTimer == 0) {
                i_this->mHitTimer = 20;
                if (!hit) {
                    atInfo.mpObj = i_this->mBoomerangSph.GetTgHitObj();
                    at_power_check(&atInfo);
                    if (atInfo.mResultingAttackType == 8) {
                        i_this->mHitSpeed = 300.0f + REG6_F(6);
                        i_this->mHitAngle = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
                        return;
                    }
                }
                i_this->mMode = 5;
                actor->speed.x = cM_rndFX(10.0f);
                actor->speed.y = 10.0f + cM_rndF(5.0f);
                actor->speed.z = cM_rndFX(10.0f);
                cXyz scale;
                scale.set(0.3f, 0.3f, 0.3f);
                dComIfGp_particle_set(0x16, &actor->eyePos, NULL, &scale);
                i_this->mSmokeActive = true;
                hit = 1;
            }
        }
        if (hit) {
            if (atInfo.mpObj) def_se_set(actor, atInfo.mpObj, 0x21);
            sitem_s* from = i_this->mSegments;
            sitem_s* to = i_this->mCutSegments;
            for (int i = 0; i < 5; i++, from++, to++) {
                to->pos = from->pos;
                to->size = from->size;
                if (i == 4) {
                    cXyz delta = to->pos - to[-1].pos;
                    i_this->mCutLength = (1.5f + REG0_F(2)) * delta.abs();
                }
            }
        }
    }
}

/* 00002684-000026F4       .text daSitem_Execute__FP11sitem_class */
static BOOL daSitem_Execute(sitem_class* i_this) {
    i_this->mFrame++;
    for (int i = 0; i < 2; i++) {
        if (i_this->mTimers[i]) i_this->mTimers[i]--;
    }
    if (i_this->mHitTimer) i_this->mHitTimer--;
    hand_move(i_this);
    return TRUE;
}

/* 000026F4-000026FC       .text daSitem_IsDelete__FP11sitem_class */
static BOOL daSitem_IsDelete(sitem_class*) {
    return TRUE;
}

/* 000026FC-00002760       .text daSitem_Delete__FP11sitem_class */
static BOOL daSitem_Delete(sitem_class* i_this) {
    dComIfG_resDeleteDemo(&i_this->mPhase, "Sitem");
    i_this->mSmoke[0].remove();
    i_this->mSmoke[1].remove();
    return TRUE;
}

static u16 bmd_data[3] = {dRes_INDEX_SITEM_BDL_KMI_00_e, dRes_INDEX_SITEM_BDL_KMI_01_e, dRes_INDEX_SITEM_BDL_KMI_02_e};

/* 00002760-00002824       .text useHeapInit__FP11sitem_class */
static BOOL useHeapInit(sitem_class* i_this) {
    i_this->mpModel = mDoExt_J3DModel__create((J3DModelData*)dComIfG_getObjectRes("Sitem", bmd_data[i_this->mType]), 0, 0x11020203);
    if (!i_this->mpModel) return FALSE;
    if (!i_this->mLine.init(1, 10, TRUE)) return FALSE;
    if (!i_this->mCutLine.init(1, 5, TRUE)) return FALSE;
    return TRUE;
}

/* 00002824-00002844       .text daSitem_solidHeapCB__FP10fopAc_ac_c */
static BOOL daSitem_solidHeapCB(fopAc_ac_c* actor) {
    return useHeapInit((sitem_class*)actor);
}

/* 00002844-00002C04       .text daSitem_Create__FP10fopAc_ac_c */
static cPhs_State daSitem_Create(fopAc_ac_c* actor) {
    static dCcD_SrcSph tg_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_UNK800,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_BOOMERANG & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 20.0f,
        }},
    };
    static dCcD_SrcSph bm_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_BOOMERANG | AT_TYPE_WIND,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ 0,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 80.0f,
        }},
    };
    sitem_class* i_this = static_cast<sitem_class*>(actor);
    fopAcM_SetupActor(i_this, sitem_class);
    cPhs_State phase = dComIfG_resLoad(&i_this->mPhase, "Sitem");
    if (phase == cPhs_COMPLEATE_e) {
#if VERSION > VERSION_DEMO
        i_this->mType = fopAcM_GetParam(actor);
        if (i_this->mType == 255) i_this->mType = 0;
        i_this->mLengthType = fopAcM_GetParam(actor) >> 8;
        if (i_this->mLengthType > 3) i_this->mLengthType = 0;
        i_this->mItemTable = fopAcM_GetParam(actor) >> 24;
        if (i_this->mItemTable == 255) i_this->mItemTable = 0;
        i_this->mItemBit = fopAcM_GetParam(actor) >> 16;
#endif
        if (fopAcM_entrySolidHeap(actor, daSitem_solidHeapCB, 0x3040)) {
            non_pos.set(0.0f, 30000.0f, -20000.0f);
#if VERSION == VERSION_DEMO
            i_this->mType = fopAcM_GetParam(actor);
            if (i_this->mType == 255) i_this->mType = 0;
            i_this->mLengthType = fopAcM_GetParam(actor) >> 8;
            if (i_this->mLengthType > 3) i_this->mLengthType = 0;
            i_this->mItemTable = fopAcM_GetParam(actor) >> 24;
            if (i_this->mItemTable == 255) i_this->mItemTable = 0;
            i_this->mItemBit = fopAcM_GetParam(actor) >> 16;
#endif
            actor->health = 2;
            i_this->mFrame = cM_rndF(10000.0f);
            i_this->mStts.Init(255, 255, actor);
            for (int i = 0; i < 4; i++) {
                i_this->mSph[i].Set(tg_sph_src);
                i_this->mSph[i].SetStts(&i_this->mStts);
                if (i_this->mType != 1) i_this->mSph[i].OffAtSetBit();
            }
            i_this->mBoomerangSph.Set(bm_sph_src);
            i_this->mBoomerangSph.SetStts(&i_this->mStts);
            i_this->mPos = actor->current.pos;
            i_this->mAcch.Set(&i_this->mPos, &i_this->mOldPos, actor, 1, &i_this->mAcchCir, fopAcM_GetSpeed_p(actor));
            i_this->mAcchCir.SetWall(30.0f, 50.0f);
            for (int i = 0; i < 2; i++) daSitem_Execute(i_this);
        } else phase = cPhs_ERROR_e;
    }
    return phase;
}

static actor_method_class l_daSitem_Method = {
    (process_method_func)daSitem_Create,
    (process_method_func)daSitem_Delete,
    (process_method_func)daSitem_Execute,
    (process_method_func)daSitem_IsDelete,
    (process_method_func)daSitem_Draw,
};

actor_process_profile_definition g_profile_SITEM = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_SITEM_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(sitem_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_SITEM_e,
    /* Actor SubMtd */ &l_daSitem_Method,
    /* Status       */ fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
