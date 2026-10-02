/**
 * d_a_sss.cpp
 * Enemy - Dexivine (Forbidden Woods, Wind Temple)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_sss.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_bg_s_gnd_chk.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_mtx.h"
#include "res/Object/Sss.h"

static cXyz non_pos;
static f32 size_d[] = {10.0f, 10.0f, 9.5f, 9.0f, 8.5f, 8.0f, 7.5f, 7.0f, 6.5f, 6.5f};

/* 000000EC-000001F0       .text hand_draw__FP9sss_class */
static void hand_draw(sss_class* i_this) {
    fopAc_ac_c* actor = i_this;
    g_env_light.setLightTevColorType(i_this->mpMorf->getModel(), &actor->tevStr);
    i_this->mpMorf->updateDL();
    i_this->mLine.update(10, (GXColor){255, 255, 255, 255}, &actor->tevStr);
    dComIfGd_set3DlineMat(&i_this->mLine);
    if (i_this->mCutLength > 0.1f) {
        i_this->mCutLine.update(5, (GXColor){255, 255, 255, 255}, &actor->tevStr);
        dComIfGd_set3DlineMat(&i_this->mCutLine);
    }
}

/* 000001F0-00000248       .text daSss_Draw__FP9sss_class */
static BOOL daSss_Draw(sss_class* i_this) {
    if (i_this->mType != 0) {
        g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &i_this->eyePos, &i_this->tevStr);
        hand_draw(i_this);
    }
    return TRUE;
}

/* 00000248-000002C8       .text hand_close__FP9sss_class */
static void hand_close(sss_class* i_this) {
    i_this->mpMorf->setAnm(static_cast<J3DAnmTransform*>(dComIfG_getObjectRes("Sss", dRes_INDEX_SSS_BCK_SSS_TOJIRU_e)),
                         J3DFrameCtrl::EMode_NONE, 1.0f, 1.0f, 0.0f, -1.0f, NULL);
}

/* 000002C8-00000348       .text hand_open__FP9sss_class */
static void hand_open(sss_class* i_this) {
    i_this->mpMorf->setAnm(static_cast<J3DAnmTransform*>(dComIfG_getObjectRes("Sss", dRes_INDEX_SSS_BCK_SSS_HIRAKU_e)),
                         J3DFrameCtrl::EMode_NONE, 1.0f, 1.0f, 0.0f, -1.0f, NULL);
}

/* 00000348-00000444       .text hand_mtx_set__FP9sss_class */
static void hand_mtx_set(sss_class* i_this) {
    MtxTrans(i_this->mHandPos.x, i_this->mHandPos.y, i_this->mHandPos.z, false);
    cMtx_XrotM(*calc_mtx, i_this->mHandAngle.x);
    cMtx_YrotM(*calc_mtx, i_this->mHandAngle.y);
    cMtx_XrotM(*calc_mtx, REG12_S(1) - 0x4000);
    MtxScale(0.5f, 0.2f + REG0_F(0), 0.5f, true);
    MtxTrans(0.0f, -130.0f + REG12_F(3), 0.0f, true);
    J3DModel* model = i_this->mpMorf->getModel();
    model->setBaseTRMtx(*calc_mtx);
}

/* 00000444-000004CC       .text control3__FP9sss_class */
static void control3(sss_class* i_this) {
    sss_s* segment = i_this->mSegments;
    for (int i = 0; i < 10; i++, segment++) {
        f32 size = 0.8f + 0.1f * cM_ssin(i_this->mFrame * 500 + i * 100);
        segment->mSize = size * size_d[i];
    }
}

/* 000004CC-00000804       .text control1__FP9sss_class */
static void control1(sss_class* i_this) {
    int i;
    f32 x, y, z, scale;
    int angleY, angleX;
    static f32 g_d[] = {50.0f, 50.0f, 35.0f, 25.0f, 15.0f, 9.0f, 6.0f, 6.0f, 6.0f, 6.0f};
    cXyz offset, movement, stretch, rotated;
    i_this->mSegments[0].mPos = i_this->current.pos;
    sss_s* segment = &i_this->mSegments[1];
    cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
    cMtx_XrotM(*calc_mtx, i_this->current.angle.x);
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->mStretch;
    MtxPosition(&offset, &stretch);
    offset.z = i_this->mSegmentLength;
    f32 sway = i_this->mSway;
    for (i = 1; i < 9; i++, segment++) {
        movement.x = sway * cM_ssin(i_this->mFrame * (REG0_S(5) + 1100) + i * (REG0_S(6) + 4000));
        movement.y = g_d[i];
        movement.z = sway * cM_scos(i_this->mFrame * (REG0_S(7) + 800) + i * (REG0_S(8) + 4000));
        MtxPosition(&movement, &rotated);
        scale = 1.0f;
        x = rotated.x * scale + (segment->mPos.x - segment[-1].mPos.x + stretch.x * scale);
        y = rotated.y * scale + (segment->mPos.y - segment[-1].mPos.y + stretch.y * scale);
        z = rotated.z * scale + (segment->mPos.z - segment[-1].mPos.z + stretch.z * scale);
        angleY = cM_atan2s(x, z);
        angleX = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        MtxPush();
        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        MtxPosition(&offset, &movement);
        MtxPull();
        segment->mPos = segment[-1].mPos + movement;
    }
}

/* 00000840-00000B0C       .text control2__FP9sss_class */
static void control2(sss_class* i_this) {
    int i;
    f32 x, y, z;
    s16 angleX;
    int angleY;
    cXyz offset, movement;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->mSegmentLength;
    i_this->mSegments[9].mPos = i_this->mEndPos;
    sss_s* segment = &i_this->mSegments[8];
    for (i = 8; i >= 1; i--, segment--) {
        x = segment->mPos.x - segment[1].mPos.x;
        y = segment->mPos.y - segment[1].mPos.y;
        z = segment->mPos.z - segment[1].mPos.z;
        angleY = cM_atan2s(x, z);
        angleX = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        if (i == 8) {
            offset.z = i_this->mSegmentLength - 10.0f;
            if (offset.z < 0.0f) offset.z = 0.0f;
        } else {
            offset.z = i_this->mSegmentLength;
        }
        MtxPosition(&offset, &movement);
        segment->mPos = segment[1].mPos + movement;
    }
    i_this->mHandPos = i_this->mSegments[9].mPos;
    offset = i_this->mSegments[8].mPos - i_this->mSegments[9].mPos;
    i_this->mHandAngle.x = -cM_atan2s(offset.y, offset.z);
    i_this->mHandAngle.y = cM_atan2s(offset.x, std::sqrtf(offset.y * offset.y + offset.z * offset.z));
    hand_mtx_set(i_this);
}

/* 00000B0C-00000F78       .text cut_control1__FP9sss_class */
static void cut_control1(sss_class* i_this) {
    int i;
    f32 x, y, z;
    int angleY, angleX;
    cXyz offset, movement, rotated;
    i_this->mCutSegments[0].mPos = i_this->current.pos;
    sss_s* segment = &i_this->mCutSegments[1];
    cMtx_YrotS(*calc_mtx, i_this->current.angle.y);
    cMtx_XrotM(*calc_mtx, i_this->current.angle.x);
    // The original only initializes the longitudinal component here.
    offset.z = i_this->mCutLength;
    f32 sway = 50.0f + REG0_F(18);
    for (i = 1; i < 5; i++, segment++) {
        movement.x = sway * cM_ssin(i_this->mFrame * (REG0_S(5) + 4100) + i * (REG0_S(6) + 10000));
        movement.y = 50.0f + REG0_F(17);
        movement.z = sway * cM_scos(i_this->mFrame * (REG0_S(7) + 4400) + i * (REG0_S(8) + 10000));
        MtxPosition(&movement, &rotated);
        x = rotated.x + (segment->mPos.x - segment[-1].mPos.x);
        y = rotated.y + (segment->mPos.y - segment[-1].mPos.y);
        z = rotated.z + (segment->mPos.z - segment[-1].mPos.z);
        angleY = cM_atan2s(x, z);
        angleX = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        MtxPush();
        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        MtxPosition(&offset, &movement);
        MtxPull();
        segment->mPos = segment[-1].mPos + movement;
    }
    if (i_this->mpCutEmitter == NULL && i_this->mCutEffect) {
        i_this->mpCutEmitter = dComIfGp_particle_set(dPa_name::ID_IT_SN_TSURU_TAIEKI00, &i_this->mCutSegments[4].mPos);
        i_this->mTimers[1] = 60;
    }
    if (i_this->mpCutEmitter) {
        offset = i_this->mCutSegments[4].mPos - i_this->mCutSegments[3].mPos;
        int angleY = cM_atan2s(offset.x, offset.z);
        int angleX = (s16)-cM_atan2s(offset.y, std::sqrtf(offset.x * offset.x + offset.z * offset.z));
        i_this->mpCutEmitter->setGlobalTranslation(i_this->mCutSegments[4].mPos.x, i_this->mCutSegments[4].mPos.y, i_this->mCutSegments[4].mPos.z);
        i_this->mpCutEmitter->setGlobalRotation(JGeometry::TVec3<s16>(angleX, angleY, 0));
        if (i_this->mTimers[1] == 1) {
            i_this->mpCutEmitter->becomeInvalidEmitter();
            i_this->mpCutEmitter = NULL;
        }
    }
}

/* 00000F78-000014F4       .text cut_control2__FP9sss_class */
static void cut_control2(sss_class* i_this) {
    int i;
    f32 x, y, z;
    int angleY, angleX;
    cXyz offset, movement, swayOffset;
    offset.x = 0.0f;
    offset.y = 0.0f;
    offset.z = i_this->mSegmentLength;
    i_this->mSegments[9].mPos = i_this->mEndPos;
    sss_s* segment = &i_this->mSegments[8];
    f32 sway = i_this->mSway;
    for (i = 8; i >= 0; i--, segment--) {
        swayOffset.x = sway * cM_ssin(i_this->mFrame * (REG0_S(5) + 2500) + i * (REG0_S(6) + 3000));
        swayOffset.y = sway * cM_ssin(i_this->mFrame * (REG0_S(5) + 2950) + i * (REG0_S(6) + 4000));
        swayOffset.z = sway * cM_scos(i_this->mFrame * (REG0_S(7) + 2800) + i * (REG0_S(8) + 3500));
        x = swayOffset.x + (segment->mPos.x - segment[1].mPos.x);
        f32 height = (segment->mPos.y - 10.0f) + swayOffset.y;
        if (height < 5.0f + i_this->mGroundY) height = 5.0f + i_this->mGroundY;
        y = height - segment[1].mPos.y;
        z = swayOffset.z + (segment->mPos.z - segment[1].mPos.z);
        angleY = cM_atan2s(x, z);
        angleX = (s16)-cM_atan2s(y, std::sqrtf(x * x + z * z));
        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        if (i == 8) {
            offset.z = i_this->mSegmentLength - 10.0f;
            if (offset.z < 0.0f) offset.z = 0.0f;
        } else {
            offset.z = i_this->mSegmentLength;
        }
        MtxPosition(&offset, &movement);
        segment->mPos = segment[1].mPos + movement;
    }
    i_this->mHandPos = i_this->mSegments[9].mPos;
    offset = i_this->mSegments[8].mPos - i_this->mSegments[9].mPos;
    i_this->mHandAngle.x = -cM_atan2s(offset.y, offset.z);
    i_this->mHandAngle.y = cM_atan2s(offset.x, std::sqrtf(offset.y * offset.y + offset.z * offset.z));
    hand_mtx_set(i_this);
    if (i_this->mpEndEmitter == NULL && i_this->mCutEffect) {
        i_this->mpEndEmitter = dComIfGp_particle_set(dPa_name::ID_IT_SN_TSURU_TAIEKI00, &i_this->mSegments[0].mPos);
    }
    if (i_this->mpEndEmitter) {
        offset = i_this->mSegments[0].mPos - i_this->mSegments[1].mPos;
        int angleY = cM_atan2s(offset.x, offset.z);
        int angleX = (s16)-cM_atan2s(offset.y, std::sqrtf(offset.x * offset.x + offset.z * offset.z));
        i_this->mpEndEmitter->setGlobalTranslation(i_this->mSegments[0].mPos.x, i_this->mSegments[0].mPos.y, i_this->mSegments[0].mPos.z);
        i_this->mpEndEmitter->setGlobalRotation(JGeometry::TVec3<s16>(angleX, angleY, 0));
        if (i_this->mTimers[1] == 1) {
            i_this->mpEndEmitter->becomeInvalidEmitter();
            i_this->mpEndEmitter = NULL;
            i_this->mCutEffect = 0;
        }
    }
}

/* 000014F4-00002614       .text hand_move__FP9sss_class */
// NONMATCHING - demo regalloc: link, cut and the collider loop counters get different registers (r19-r22/r28)
static void hand_move(sss_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    s8 emerged;
    daPy_py_c* link = (daPy_py_c*)dComIfGp_getLinkPlayer();
    cXyz offset, rotated, target, center;
    dBgS_GndChk ground;
    bool turn = false;
    bool cut = false;
    f32 rate = 0.1f;
    f32 targetSpeed = 8.0f;
    f32 speedStep = 0.1f;
    f32 stretch = 0.0f;
    f32 length = 30.0f;
    f32 lengthStep = 1.0f;
    f32 baseOffset = 0.0f;
    f32 distance = fopAcM_searchPlayerDistance(actor);
    f32 range;
    f32 sway = 5.0f;
    if (i_this->mRange != 0xFF) range = 10.0f * i_this->mRange;
    else range = 1000.0f;
    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cMtx_XrotM(*calc_mtx, actor->current.angle.x);
    emerged = 0;
    switch (i_this->mAction) {
    case 0:
        target = actor->current.pos;
        length = 0.0f;
        lengthStep = 0.5f;
        baseOffset = -20.0f;
        if (i_this->mTimers[0] == 0) {
            if (i_this->mSwitch != 0) {
                if (dComIfGs_isSwitch(i_this->mSwitch, fopAcM_GetRoomNo(actor)) || REG0_S(0) != 0) {
                    i_this->mAction = 1;
                    i_this->mTimers[0] = 30;
                    hand_open(i_this);
                    emerged = 1;
                }
            } else if (distance < range) {
                i_this->mAction = 1;
                i_this->mTimers[0] = 30;
                hand_open(i_this);
                emerged = 1;
            }
        }
        if (std::fabsf(i_this->mEndPos.y - actor->current.pos.y) < 5.0f && emerged) {
            if (i_this->mType == 1) fopAcM_seStart(actor, JA_SE_OBJ_SVINE_OUT_WATER, 0);
            else fopAcM_seStart(actor, JA_SE_OBJ_SVINE_OUT, 0);
        }
        break;
    case 1:
        offset.x = cM_ssin(i_this->mFrame * 600) * (50.0f + REG0_F(13));
        offset.y = 250.0f;
        offset.z = cM_ssin(i_this->mFrame * 700) * (50.0f + REG0_F(13));
        MtxPosition(&offset, &rotated);
        target = actor->current.pos + rotated;
        if (i_this->mTimers[0] == 0 && distance < 300.0f + REG0_F(14)) i_this->mAction = 2;
        if (distance > 100.0f + range) i_this->mAction = 0;
        break;
    case 2:
        targetSpeed = 15.0f;
        stretch = 10.0f;
        speedStep = 0.5f;
        turn = true;
        target = player->current.pos;
        target.y += 70.0f;
        if (distance > 450.0f + REG0_F(14)) {
            i_this->mAction = 1;
            actor->speedF = 0.0f;
        }
        offset = target - i_this->mEndPos;
        if (offset.abs() < 20.0f && player == link) {
            i_this->mAction = 3;
            hand_close(i_this);
            fopAcM_seStart(actor, JA_SE_OBJ_SVINE_GRASP, 0);
        } else {
            break;
        }
        // fallthrough
    case 3:
        player->onVineCatch();
        player->setFace(daPy_py_c::daPyFace_TIYAYA);
        if (dComIfGs_getMagic() != 0) {
            dComIfGp_onMagicGaugeBlink();
            fopAcM_seStart(actor, JA_SE_OBJ_ATK_VINE_MP_SUCK, 0);
            if ((i_this->mFrame & 31) == 0) dComIfGp_setItemMagicCount(-1);
        }
        if (distance > 400.0f + REG6_F(2)) {
            f32 power = (0.1f + REG6_F(0)) * (distance - (400.0f + REG6_F(2)));
            if (power > 100.0f + REG6_F(1)) power = 100.0f + REG6_F(1);
            player->setOutPower(power, fopAcM_searchPlayerAngleY(actor) + 0x8000, 0);
        }
        target = player->current.pos;
        target.y += 70.0f;
        targetSpeed = 200.0f;
        rate = 1.0f;
        speedStep = 5.0f;
        i_this->mSway = 10.0f + REG0_F(11);
        if (distance > 800.0f) {
            i_this->mAction = 1;
            actor->speedF = 30.0f;
            hand_open(i_this);
        }
        break;
    case 5:
        sway = 50.0f;
        i_this->mSway = sway;
        cut = true;
        length = 25.0f;
        lengthStep = 1.0f;
        i_this->mEndPos += actor->speed;
        actor->speed.y -= 3.0f;
        i_this->mInvulnerabilityTimer = 5;
        Vec pos;
        pos.x = i_this->mEndPos.x;
        pos.y = i_this->mEndPos.y;
        pos.z = i_this->mEndPos.z;
        pos.y += 200.0f;
        ground.SetPos(&pos);
        i_this->mGroundY = dComIfG_Bgsp()->GroundCross(&ground);
        if (i_this->mGroundY == -G_CM3D_F_INF || i_this->mEndPos.y <= 10.0f + i_this->mGroundY) {
            i_this->mEndPos.y = 10.0f + i_this->mGroundY;
            i_this->mTimers[0] = 100;
            i_this->mAction = 6;
        }
        break;
    case 6:
        cut = true;
        sway = 0.0f;
        i_this->mInvulnerabilityTimer = 10;
        if (i_this->mTimers[0] < 40) {
            length = 0.0f;
            lengthStep = 1.0f;
            i_this->mEndPos.y -= 2.0f;
        }
        if (i_this->mTimers[0] == 0) {
            i_this->mEndPos = actor->current.pos;
            i_this->mAction = 0;
            i_this->mSegmentLength = 0.0f;
            i_this->mCutLength = 0.0f;
        }
        break;
    }
    cLib_addCalc2(&i_this->mSegmentLength, length, 0.5f, lengthStep);
    cLib_addCalc2(&i_this->mStretch, stretch, 1.0f, 0.2f);
    cLib_addCalc2(&i_this->mSway, sway, 1.0f, 1.5f);
    if (!cut) {
        cLib_addCalc2(&actor->speedF, targetSpeed, 1.0f, speedStep);
        if (i_this->mKnockback > 1.0f && i_this->mAction != 3) {
            cMtx_YrotS(*calc_mtx, i_this->mKnockbackAngle);
            offset.x = 0.0f;
            offset.y = 100.0f + REG6_F(9);
            offset.z = i_this->mKnockback;
            MtxPosition(&offset, &rotated);
            target = actor->current.pos + rotated;
            rate = 0.1f;
            actor->speedF = 0.2f * i_this->mKnockback;
            if (actor->speedF > 30.0f + REG6_F(8)) actor->speedF = 30.0f + REG6_F(8);
        }
        cLib_addCalc0(&i_this->mKnockback, 1.0f, 5.0f + REG6_F(7));
        cLib_addCalc2(&i_this->mEndPos.x, target.x, rate, actor->speedF);
        cLib_addCalc2(&i_this->mEndPos.y, target.y, rate, actor->speedF);
        cLib_addCalc2(&i_this->mEndPos.z, target.z, rate, actor->speedF);
        cLib_addCalc2(&actor->current.pos.y, actor->home.pos.y + baseOffset, 0.5f, 0.5f);
        if (turn && actor->current.angle.x == 0) {
            cLib_addCalcAngleS2(&actor->current.angle.y, fopAcM_searchPlayerAngleY(actor), 16, 0x800);
        }
        control1(i_this);
        control2(i_this);
    } else {
        cut_control1(i_this);
        cut_control2(i_this);
        sss_s* segment = i_this->mCutSegments;
        cXyz* positions = i_this->mCutLine.getPos(0);
        u8* sizes = i_this->mCutLine.getSize(0);
        for (int i = 0; i < 5; i++, segment++, positions++, sizes++) {
            *positions = segment->mPos;
            *sizes = segment->mSize;
        }
        cLib_addCalc0(&i_this->mCutLength, 1.0f, 1.0f + REG0_F(1));
    }
    control3(i_this);
    i_this->mpMorf->play(NULL, 0, 0);
    sss_s* segment = i_this->mSegments;
    cXyz* positions = i_this->mLine.getPos(0);
    u8* sizes = i_this->mLine.getSize(0);
    for (int i = 0; i < 10; i++, segment++, positions++, sizes++) {
        *positions = segment->mPos;
        *sizes = segment->mSize;
    }
    positions = i_this->mLine.getPos(0);
    actor->eyePos = positions[5];
    actor->attention_info.position = actor->eyePos;
    i_this->mStts.Move();
    if (!cut) i_this->mBoomerangSphere.SetC(actor->eyePos);
    else i_this->mBoomerangSphere.SetC(non_pos);
    u8 hit = 0;
    dComIfG_Ccsp()->Set(&i_this->mBoomerangSphere);
    for (int i = 0; i < 3; i++) {
        int index = (i_this->mFrame & 3) + i * 2;
        index %= 10;
        center = positions[index];
        if (!cut) i_this->mSpheres[i].SetC(center);
        else i_this->mSpheres[i].SetC(non_pos);
        if (i_this->mAction == 3) i_this->mSpheres[i].OffCoSetBit();
        else i_this->mSpheres[i].OnCoSetBit();
        dComIfG_Ccsp()->Set(&i_this->mSpheres[i]);
    }
    for (int i = 0; i < 3; i++) {
        if (i_this->mSpheres[i].ChkTgHit()) {
            hit = i + 1;
            break;
        }
    }
    if ((hit || i_this->mBoomerangSphere.ChkTgHit()) && i_this->mInvulnerabilityTimer == 0) {
        CcAtInfo atInfo;
        atInfo.pParticlePos = NULL;
        i_this->mInvulnerabilityTimer = 20;
        if (hit == 0) {
            atInfo.mpObj = i_this->mBoomerangSphere.GetTgHitObj();
            atInfo.pParticlePos = i_this->mBoomerangSphere.GetTgHitPosP();
            at_power_check(&atInfo);
            if (atInfo.mResultingAttackType == 8) {
                i_this->mKnockback = 300.0f + REG6_F(6);
                i_this->mKnockbackAngle = fopAcM_searchPlayerAngleY(actor) + 0x8000;
                return;
            }
        } else {
            atInfo.mpObj = i_this->mSpheres[hit - 1].GetTgHitObj();
            atInfo.pParticlePos = i_this->mSpheres[hit - 1].GetTgHitPosP();
        }
        fopAcM_seStart(actor, JA_SE_LK_LAST_HIT, 0);
        fopAcM_seStart(actor, JA_SE_OBJ_SVINE_CRASH, 0);
        i_this->mAction = 5;
        actor->speed.x = cM_rndFX(10.0f);
        actor->speed.y = 30.0f + cM_rndF(10.0f);
        actor->speed.z = cM_rndFX(10.0f);
        cXyz effectScale;
        effectScale.set(0.3f, 0.3f, 0.3f);
        dComIfGp_particle_set(dPa_name::ID_AK_JN_SIBOUFLASH, &actor->eyePos, NULL, &effectScale);
        i_this->mCutEffect = 1;
        sss_s* source = i_this->mSegments;
        sss_s* dest = i_this->mCutSegments;
        for (int i = 0; i < 5; i++, source++, dest++) {
            dest->mPos = source->mPos;
            dest->mSize = source->mSize;
            if (i == 4) {
                cXyz delta = dest->mPos - dest[-1].mPos;
                i_this->mCutLength = (1.5f + REG0_F(2)) * delta.abs();
            }
        }
        hand_open(i_this);
    }
}

/* 00002614-00002618       .text hand_main__FP9sss_class */
static void hand_main(sss_class* i_this) {}

/* 00002618-0000269C       .text daSss_Execute__FP9sss_class */
static BOOL daSss_Execute(sss_class* i_this) {
    i_this->mFrame++;
    for (int i = 0; i < 2; i++) {
        if (i_this->mTimers[i] != 0) i_this->mTimers[i]--;
    }
    if (i_this->mInvulnerabilityTimer != 0) i_this->mInvulnerabilityTimer--;
    if (i_this->mType == 0) hand_main(i_this);
    else hand_move(i_this);
    return TRUE;
}

/* 0000269C-000026A4       .text daSss_IsDelete__FP9sss_class */
static BOOL daSss_IsDelete(sss_class*) {
    return TRUE;
}

/* 000026A4-00002720       .text daSss_Delete__FP9sss_class */
static BOOL daSss_Delete(sss_class* i_this) {
    dComIfG_resDeleteDemo(&i_this->mPhase, "Sss");
    if (i_this->mpCutEmitter) i_this->mpCutEmitter->becomeInvalidEmitter();
    if (i_this->mpEndEmitter) i_this->mpEndEmitter->becomeInvalidEmitter();
    return TRUE;
}

/* 00002720-000028A0       .text useHeapInit__FP9sss_class */
static BOOL useHeapInit(sss_class* i_this) {
    i_this->mpMorf = new mDoExt_McaMorf(
        static_cast<J3DModelData*>(dComIfG_getObjectRes("Sss", dRes_INDEX_SSS_BMD_SSS_HAND_e)),
        NULL, NULL, static_cast<J3DAnmTransform*>(dComIfG_getObjectRes("Sss", dRes_INDEX_SSS_BCK_SSS_HIRAKU_e)),
        J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, 1, NULL, 0, 0x11020203);
    if (i_this->mpMorf->getModel() == NULL) return FALSE;
    if (!i_this->mLine.init(1, 10, static_cast<ResTIMG*>(dComIfG_getObjectRes("Sss", dRes_INDEX_SSS_BTI_SSS_e)), TRUE)) return FALSE;
    if (!i_this->mCutLine.init(1, 5, static_cast<ResTIMG*>(dComIfG_getObjectRes("Sss", dRes_INDEX_SSS_BTI_SSS_e)), TRUE)) return FALSE;
    return TRUE;
}

/* 000028A0-000028C0       .text daSss_solidHeapCB__FP10fopAc_ac_c */
static BOOL daSss_solidHeapCB(fopAc_ac_c* i_this) {
    return useHeapInit(static_cast<sss_class*>(i_this));
}

/* 000028C0-00002BB4       .text daSss_Create__FP10fopAc_ac_c */
static cPhs_State daSss_Create(fopAc_ac_c* actor) {
    static dCcD_SrcSph tg_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
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
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_VsPlayer_e,
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
    fopAcM_SetupActor(actor, sss_class);
    sss_class* i_this = static_cast<sss_class*>(actor);
    cPhs_State phase = dComIfG_resLoad(&i_this->mPhase, "Sss");
    if (phase == cPhs_COMPLEATE_e) {
        if (fopAcM_entrySolidHeap(actor, daSss_solidHeapCB, 0x3040)) {
            non_pos.x = 0.0f;
            non_pos.y = 30000.0f;
            non_pos.z = -20000.0f;
            i_this->mType = fopAcM_GetParam(actor);
            if (i_this->mType == 0xFF) i_this->mType = 0;
            if (i_this->mType != 1) i_this->mType = 35;
            i_this->mRange = fopAcM_GetParam(actor) >> 8;
            i_this->mSwitch = fopAcM_GetParam(actor) >> 16;
            if (i_this->mSwitch == 0xFF) i_this->mSwitch = 0;
            actor->health = 2;
            i_this->mFrame = cM_rndF(10000.0f);
            i_this->mStts.Init(0xFF, 0xFF, actor);
            for (int i = 0; i < 3; i++) {
                i_this->mSpheres[i].Set(tg_sph_src);
                i_this->mSpheres[i].SetStts(&i_this->mStts);
            }
            i_this->mBoomerangSphere.Set(bm_sph_src);
            i_this->mBoomerangSphere.SetStts(&i_this->mStts);
            i_this->mEndPos = actor->current.pos;
            if (i_this->mSwitch == 0) i_this->mEndPos.y += 230.0f;
            daSss_Execute(i_this);
        } else {
            phase = cPhs_ERROR_e;
        }
    }
    return phase;
}

static actor_method_class l_daSss_Method = {
    (process_method_func)daSss_Create,
    (process_method_func)daSss_Delete,
    (process_method_func)daSss_Execute,
    (process_method_func)daSss_IsDelete,
    (process_method_func)daSss_Draw,
};

actor_process_profile_definition g_profile_SSS = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_SSS_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(sss_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_SSS_e,
    /* Actor SubMtd */ &l_daSss_Method,
    /* Status       */ fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
