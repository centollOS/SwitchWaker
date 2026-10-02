// Vine core and its severable tentacles.
#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/actor/d_a_ss.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#include "d/d_cc_uty.h"
#include "d/d_bg_s_lin_chk.h"
#include "d/d_bg_s_gnd_chk.h"
#include "res/Object/Ss.h"

static GXColor col;

static BOOL nodeCallBack(J3DNode* node, int phase) {
    if (phase == 0) {
        int joint = ((J3DJoint*)node)->getJntNo();
        J3DModel* model = j3dSys.getModel();
        ss_class* i_this = (ss_class*)model->getUserArea();
        if (i_this != NULL && joint == SW_JNT_EYE_e) {
            cXyz unused;
            PSMTXCopy(model->getAnmMtx(joint), *calc_mtx);
            if (i_this->mWall != 0) cMtx_XrotM(*calc_mtx, 0x4000);
            cMtx_YrotM(*calc_mtx, i_this->mEyeAngle.y);
            cMtx_XrotM(*calc_mtx, i_this->mEyeAngle.x);
            model->setAnmMtx(joint, *calc_mtx);
            PSMTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
        }
    }
    return TRUE;
}

static void hand_draw(ss_class* i_this) {
    i_this->mLine.update(20, col, &i_this->tevStr);
    dComIfGd_set3DlineMat(&i_this->mLine);
}

static BOOL daSs_Draw(ss_class* i_this) {
    g_env_light.settingTevStruct(0, &i_this->eyePos, &i_this->tevStr);
    if (i_this->mMode < 50) {
        g_env_light.setLightTevColorType(i_this->mpMorf->getModel(), &i_this->tevStr);
        i_this->mpMorf->entryDL();
    }
    hand_draw(i_this);
    return TRUE;
}

static void anm_init(ss_class* i_this, int anm, float morph, unsigned char mode, float speed, int sound) {
    if (sound >= 0) {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("Ss", anm), mode, morph, speed, 0.0f, -1.0f, dComIfG_getObjectRes("Bb", sound));
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("Ss", anm), mode, morph, speed, 0.0f, -1.0f, NULL);
    }
}

static void hand_1_set(ss_class* i_this, ss_s* hand) {
    cXyz step, world, end, start, delta;
    fopAc_ac_c* actor = i_this;
    ss_s_s* segment;
    s16 angle;
    int i;
    dBgS_LinChk line;
    segment = hand->segments;
    cMtx_YrotS(*calc_mtx, hand->angle.y);
    step.x = 0.0f;
    step.y = 0.0f;
    step.z = 30.0f + REG0_F(7) - 10.0f;
    start = hand->pos;
    end = hand->pos;
    end.y -= 300.0f + REG0_F(6);
    angle = 0;
    for (i = 0; i < 20; i++, segment++) {
        segment->size = 0;
        line.Set(&start, &end, actor);
        if (dComIfG_Bgsp()->LineCross(&line)) {
            segment->pos = line.GetCross();
            delta = start - segment->pos;
            MtxPush();
            s16 yaw = cM_atan2s(delta.x, delta.z);
            cMtx_YrotS(*calc_mtx, yaw);
            cMtx_XrotM(*calc_mtx, -cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z)));
            delta.x = 0.0f;
            delta.y = 0.0f;
            delta.z = 5.0f + REG8_F(8);
            MtxPosition(&delta, &world);
            segment->pos += world;
            MtxPull();
        }
        MtxPush();
        cMtx_YrotM(*calc_mtx, angle);
        angle += (s16)cM_rndFX(6000.0f);
        MtxPosition(&step, &world);
        MtxPull();
        start += world;
        step.z += 2.0f + REG8_F(9);
    }
}

static void hand_1_set_2(ss_class* i_this, ss_s* hand) {
    cXyz step, world, start, delta;
    ss_s_s* segment;
    s16 angle;
    fopAc_ac_c* actor = i_this;
    int i;
    dBgS_LinChk line;
    segment = hand->segments;
    cMtx_YrotS(*calc_mtx, hand->angle.y);
    step.x = 0.0f;
    step.y = 0.0f;
    step.z = -250.0f + REG12_F(6);
    MtxPosition(&step, &start);
    start += hand->pos;
    cMtx_ZrotM(*calc_mtx, hand->angle.z);
    step.x = 0.0f;
    step.y = 30.0f + REG12_F(7) - 10.0f;
    step.z = 0.0f;
    angle = 0;
    for (i = 0; i < 20; i++, segment++) {
        segment->size = 0;
        line.Set(&hand->pos, &start, actor);
        if (dComIfG_Bgsp()->LineCross(&line)) {
            segment->pos = line.GetCross();
            delta = start - segment->pos;
            MtxPush();
            s16 yaw = cM_atan2s(delta.x, delta.z);
            cMtx_YrotS(*calc_mtx, yaw);
            cMtx_XrotM(*calc_mtx, -cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z)));
            delta.x = 0.0f;
            delta.y = 0.0f;
            delta.z = -5.0f + REG12_F(8);
            MtxPosition(&delta, &world);
            segment->pos += world;
            MtxPull();
        }
        MtxPush();
        cMtx_ZrotM(*calc_mtx, angle);
        angle += (s16)cM_rndFX(6000.0f);
        MtxPosition(&step, &world);
        MtxPull();
        start += world;
        step.y += 2.0f + REG12_F(9);
    }
}

static void hand_1_move(ss_class* i_this, ss_s* hand) {
    fopAc_ac_c* actor = i_this;
    ss_s_s* segment;
    int length;
    s8 len;
    int i;
    int collider;
    CcAtInfo hit;
    cXyz speed;
    u8 damage;
    segment = hand->segments;
    length = len = hand->length;
    if ((i_this->mFrame & 3) == 0 && i_this->mMode < 50 && length < 20) hand->length = len + 1;
    collider = 0;
    for (i = 0; i < 20; i++, segment++) {
        if (i >= length - 1) segment->size = 0;
        else if (i == length - 2) segment->size = 1;
        else if (i == length - 3) segment->size = 2;
        else if (i == length - 4) segment->size = 3;
        else segment->size = 4;
        if (i_this->mMode < 50) {
            int phase = i_this->mFrame & 3;
            if (i == phase + 2 || i == phase + 7 || i == phase + 12 || i == phase + 17) {
                hand->sph[collider].SetC(segment->pos);
                dComIfG_Ccsp()->Set(&hand->sph[collider]);
                collider++;
            }
        }
    }
    damage = 0;
    for (i = 0; i < 4; i++) {
        if (hand->sph[i].ChkTgHit()) {
            hit.mpObj = hand->sph[i].GetTgHitObj();
            if (hit.mpObj != NULL) {
                hit.mpActor = at_power_check(&hit);
                if (hit.mResultingAttackType == 5) damage = 2;
                else if (hit.mpObj != NULL && hit.mpObj->ChkAtType(AT_TYPE_GRAPPLING_HOOK)) damage = 0;
                else damage = 1;
            }
            break;
        }
    }
    if (damage != 0 || i_this->mMode >= 50) {
        dComIfGs_onEventBit(0x2B20);
        hand->mode = 2;
        hand->pos = segment[-1].pos;
        hand->sink = 0.0f;
        if (damage != 0) def_se_set(actor, hit.mpObj, 0x21);
        hand->gravity = 20.0f + REG8_F(3);
        hand->wave = 0.0f;
        hand->phase = cM_rndF(65536.0f);
        hand->frequency = 3000.0f + cM_rndF(3000.0f);
        hand->rotation = cM_rndFX(2000.0f);
        if (damage == 2) {
            hand->burning = 1;
            speed.y = 5.0f;
            speed.z = 5.0f;
            hand->cutTimer = 2;
        } else {
            speed.y = 25.0f + REG8_F(12) + cM_rndF(7.0f);
            speed.z = 7.0f + REG8_F(13) + cM_rndF(5.0f);
            hand->cutTimer = REG8_S(7) + 8;
        }
        speed.x = 0.0f;
        cMtx_YrotS(*calc_mtx, hand->angle.y);
        MtxPosition(&speed, &hand->speed);
    }
}

static void hand_1_cut(ss_class* i_this, ss_s* hand) {
    fopAc_ac_c* actor = i_this;
    cXyz step, delta, world, wave;
    Vec pos;
    f32 y, groundY;
    s16 rot_target;
    step.y = 0.0f;
    step.x = 0.0f;
    step.z = 15.0f + REG8_F(11);
    if (hand->cutTimer != 0) {
        hand->cutTimer--;
        hand->segments[0].pos = actor->home.pos;
        ss_s_s* segment;
        int i = 1;
        segment = &hand->segments[1];
        for (; i < 20; i++, segment++) {
            delta = segment->pos - segment[-1].pos;
            int yaw = (s16)cM_atan2s(delta.x, delta.z);
            cMtx_YrotS(*calc_mtx, yaw);
            cMtx_XrotM(*calc_mtx, -cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z)));
            MtxPosition(&step, &delta);
            segment->pos = segment[-1].pos + delta;
        }
    } else {
        cLib_addCalc2(&hand->wave, 10.0f + REG8_F(15), 1.0f, 1.0f + REG8_F(16));
        hand->phase += 100;
    }
    hand->segments[19].pos = hand->pos;
    ss_s_s* segment = &hand->segments[18];
    dBgS_GndChk ground;
    cLib_addCalc2(&hand->gravity, -20.0f + REG8_F(2), 1.0f, 1.0f + REG8_F(4));
    wave.y = 0.0f;
    for (int i = 18; i >= 0; i--, segment--) {
        pos.x = segment->pos.x;
        pos.y = segment->pos.y;
        pos.z = segment->pos.z;
        pos.y += 50.0f;
        ground.SetPos(&pos);
        groundY = 5.0f + dComIfG_Bgsp()->GroundCross(&ground);
        y = segment->pos.y + hand->gravity;
        if (y < groundY + hand->sink) y = groundY + hand->sink;
        wave.z = -2.0f + REG8_F(17);
        wave.x = hand->wave * cM_ssin(hand->phase + i * hand->frequency);
        cMtx_YrotS(*calc_mtx, hand->angle.y);
        MtxPosition(&wave, &world);
        delta.x = world.x + (segment->pos.x - segment[1].pos.x);
        delta.y = y - segment[1].pos.y;
        delta.z = world.z + (segment->pos.z - segment[1].pos.z);
        int yaw = (s16)cM_atan2s(delta.x, delta.z);
        cMtx_YrotS(*calc_mtx, yaw);
        cMtx_XrotM(*calc_mtx, -cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z)));
        MtxPosition(&step, &delta);
        if (hand->cutTimer != 0 && i == 0) segment->pos = actor->home.pos;
        else segment->pos = segment[1].pos + delta;
        if ((i & 3) == 0 && hand->burning != 0) dComIfGp_particle_setSimple(1, &segment->pos);
    }
    hand->pos += hand->speed;
    hand->speed.y -= 3.0f + REG8_F(14);
    pos.x = hand->pos.x;
    pos.y = hand->pos.y;
    pos.z = hand->pos.z;
    pos.y += 50.0f;
    ground.SetPos(&pos);
    groundY = 5.0f + dComIfG_Bgsp()->GroundCross(&ground);
    if (hand->groundTimer == 0) {
        if (hand->speed.y < -300.0f) {
            hand->mode = 0;
            hand->length = 0;
        }
        if (hand->pos.y <= groundY + hand->sink) hand->groundTimer = 40.0f + cM_rndF(20.0f);
    } else {
        cLib_addCalc0(&hand->speed.x, 1.0f, 0.4f + REG8_F(1));
        cLib_addCalc0(&hand->speed.z, 1.0f, 0.4f + REG8_F(1));
        hand->angle.y += hand->rotation;
        rot_target = 0;
        cLib_addCalcAngleS2(&hand->rotation, rot_target, 1, 30);
        hand->groundTimer--;
        if (hand->groundTimer == 0) {
            hand->mode = 0;
            hand->burning = 0;
            hand->length = 0;
            fopAcM_seStart(actor, JA_SE_OBJ_VINE_S_RECOVER, 0);
        }
    }
    if (hand->pos.y < groundY + hand->sink) {
        hand->pos.y = groundY + hand->sink;
        hand->speed.y = -5.0f;
        cLib_addCalc2(&hand->sink, -10.0f + REG0_F(18), 0.05f, 0.2f + REG0_F(19));
    }
}

static cXyz non_pos;

static void hand_move(ss_class* i_this) {
    fopAc_ac_c* actor = i_this;
    dBgS_LinChk line;
    cXyz offset, world;
    int i, j;
    ss_s* hand;
    non_pos.set(0.0f, -10000.0f, 0.0f);
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 4; j++) {
            i_this->mHands[i].sph[j].SetC(non_pos);
            dComIfG_Ccsp()->Set(&i_this->mHands[i].sph[j]);
        }
        hand = &i_this->mHands[i];
        switch (i_this->mHands[i].mode) {
        case 0:
            if (i_this->mWall == 1) {
                i_this->mHands[i].angle.y = i_this->current.angle.y + 0x8000;
                cMtx_YrotS(*calc_mtx, i_this->mHands[i].angle.y);
                offset.x = 0.0f;
                offset.y = 0.0f;
                offset.z = -1000.0f;
                MtxPosition(&offset, &world);
                world += i_this->current.pos;
                line.Set(&i_this->current.pos, &world, actor);
                if (dComIfG_Bgsp()->LineCross(&line)) {
                    i_this->mHands[i].pos = line.GetCross();
                    i_this->home.pos = i_this->mHands[i].pos;
                    offset.z = 200.0f + REG12_F(12);
                    MtxPosition(&offset, &world);
                    i_this->mHands[i].pos += world;
                }
                i_this->mHands[i].angle.z = i * 0x1999;
                hand_1_set_2(i_this, &i_this->mHands[i]);
            } else {
                i_this->mHands[i].pos = i_this->current.pos;
                world = i_this->current.pos;
                world.y -= 1000.0f;
                line.Set(&i_this->mHands[i].pos, &world, actor);
                if (dComIfG_Bgsp()->LineCross(&line)) {
                    i_this->mHands[i].pos = line.GetCross();
                    i_this->home.pos = i_this->mHands[i].pos;
                    i_this->mHands[i].pos.y += 50.0f;
                }
                i_this->mHands[i].angle.y = i * 0x1999;
                hand_1_set(i_this, &i_this->mHands[i]);
            }
            i_this->mHands[i].mode++;
            break;
        case 1:
            hand_1_move(i_this, &i_this->mHands[i]);
            break;
        case 2:
            hand_1_cut(i_this, &i_this->mHands[i]);
            break;
        case 3:
            break;
        default:
            break;
        }
    }
}

static void core_move(ss_class* i_this) {
    f32 distance = fopAcM_searchActorDistance(i_this, dComIfGp_getPlayer(0));
    f32 range = 350.0f;
    if (i_this->mType == 3) range = 1500.0f;
    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, dRes_INDEX_SS_BCK_WAIT_OPEN_e, 1.0f, 2, 1.0f, -1);
        i_this->mMode++;
    case 1:
        i_this->mClosed = 0;
        break;
    case 10:
        anm_init(i_this, dRes_INDEX_SS_BCK_WAIT_CLOSE_e, 1.0f, 2, 1.0f, -1);
        i_this->mMode++;
    case 11:
        i_this->mClosed = 1;
        break;
    case 19:
        anm_init(i_this, dRes_INDEX_SS_BCK_WAIT_OPEN_e, 5.0f, 2, 1.0f, -1);
        i_this->mMode = 22;
        break;
    case 20:
        anm_init(i_this, dRes_INDEX_SS_BCK_OPEN_e, 1.0f, 0, 1.0f, -1);
        i_this->mMode++;
        break;
    case 21:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_SS_BCK_WAIT_OPEN_e, 1.0f, 2, 1.0f, -1);
            i_this->mMode++;
        }
    case 22:
        i_this->mClosed = 0;
        if (distance < range) {
            i_this->mMode = 30;
            fopAcM_seStart(i_this, JA_SE_OBJ_VINE_CORE_CLOSE, 0);
        }
        break;
    case 29:
        i_this->mMode = 32;
        anm_init(i_this, dRes_INDEX_SS_BCK_WAIT_CLOSE_e, 5.0f, 2, 1.0f, -1);
        break;
    case 30:
        anm_init(i_this, dRes_INDEX_SS_BCK_CLOSE_e, 1.0f, 0, 1.0f, -1);
        i_this->mMode++;
        break;
    case 31:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_SS_BCK_WAIT_CLOSE_e, 1.0f, 2, 1.0f, -1);
            i_this->mMode++;
        }
    case 32:
        i_this->mClosed = 1;
        if (distance > 20.0f + range) {
            i_this->mMode = 20;
            fopAcM_seStart(i_this, JA_SE_OBJ_VINE_CORE_OPEN, 0);
        }
        break;
    case 40:
        if (i_this->mClosed != 0) {
            anm_init(i_this, dRes_INDEX_SS_BCK_NO_DAMAGE_e, 1.0f, 0, 1.0f, -1);
        } else {
            anm_init(i_this, dRes_INDEX_SS_BCK_DAMAGE_e, 1.0f, 0, 1.0f, -1);
            i_this->mShake = 20;
        }
        i_this->mMode++;
        break;
    case 41:
        if (i_this->mpMorf->isStop()) {
            i_this->mMode = i_this->mPreviousMode;
            if (i_this->mMode > 30) i_this->mMode = 29;
            else if (i_this->mMode > 20) i_this->mMode = 20;
            else if (i_this->mMode > 10) i_this->mMode = 10;
            else i_this->mMode = 0;
        }
        break;
    case 50:
        if (i_this->mTimers[0] == 69) {
            cXyz scale(0.5f, 0.5f, 0.5f);
            dComIfGp_particle_set(0x13, &i_this->eyePos, NULL, &scale);
            dComIfGp_particle_set(0x16, &i_this->eyePos, NULL, &scale);
            fopAcM_seStart(i_this, JA_SE_OBJ_ENM_VAPORIZE, 0);
        }
        if (i_this->mTimers[1] != 0) {
            dComIfGp_particle_setSimple(1, &i_this->eyePos);
            for (int i = 0; i < 10; i++) {
                cXyz* pos = i_this->mLine.getPos(i);
                for (int j = 0; j < 20; j += REG0_S(7) + 2, pos += REG0_S(7) + 2) {
                    if (cM_rndF(1.0f) < 0.2f + REG0_F(15)) dComIfGp_particle_setSimple(1, pos);
                }
                i_this->mHands[i].mode = 1;
                if ((i_this->mFrame & 1) == 0 && i_this->mHands[i].length != 0) i_this->mHands[i].length--;
            }
        }
        if (i_this->mTimers[0] == 0) {
            if (i_this->mSwitch != 0) dComIfGs_onSwitch(i_this->mSwitch, fopAcM_GetRoomNo(i_this));
            fopAcM_delete(i_this);
        }
        break;
    }
    if (i_this->mSph.ChkTgHit() && i_this->mHitTimer == 0) {
        CcAtInfo hit;
        i_this->mHitTimer = 10;
        hit.mpObj = i_this->mSph.GetTgHitObj();
        hit.mpActor = at_power_check(&hit);
        if (hit.mpObj != NULL && hit.mpObj->ChkAtType(AT_TYPE_GRAPPLING_HOOK)) return;
        i_this->mPreviousMode = i_this->mMode;
        i_this->mMode = 40;
        if (i_this->mClosed == 0 || hit.mResultingAttackType == 5) {
            hit.mpObj = i_this->mSph.GetTgHitObj();
            hit.pParticlePos = i_this->mSph.GetTgHitPosP();
            at_power_check(&hit);
            if (hit.mResultingAttackType == 10 || hit.mResultingAttackType == 3) i_this->health = 0;
            if (hit.mResultingAttackType == 5) {
                i_this->health = 0;
                i_this->mTimers[1] = 50;
                fopAcM_seStart(i_this, JA_SE_OBJ_VINE_S_BURN, 0);
            } else cc_at_check(i_this, &hit);
            if (i_this->health <= 0) {
                i_this->mHitTimer = 100;
                i_this->mMode = 50;
                i_this->mTimers[0] = 70;
            }
        } else fopAcM_seStart(i_this, JA_SE_OBJ_SVINE_REBOUND, 0);
    }
    if (i_this->mMode >= 50) i_this->eyePos.set(10000.0f, 10000.0f, 10000.0f);
    i_this->mSph.SetC(i_this->eyePos);
    i_this->mSph.SetR(35.0f + REG0_F(0));
    if (i_this->mClosed == 0) i_this->mSph.OffTgShield();
    else i_this->mSph.OnTgShield();
    dComIfG_Ccsp()->Set(&i_this->mSph);
}

static BOOL daSs_Execute(ss_class* i_this) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    col.r = REG8_S(3);
    col.g = REG8_S(4) + 180;
    col.b = REG8_S(5) + 50;
    col.a = 255;
    i_this->mFrame++;
    for (int i = 0; i < 4; i++) if (i_this->mTimers[i] != 0) i_this->mTimers[i]--;
    if (i_this->mHitTimer != 0) i_this->mHitTimer--;
    core_move(i_this);
    hand_move(i_this);
    for (int i = 0; i < 10; i++) {
        cXyz* pos = i_this->mLine.getPos(i);
        u8* size = i_this->mLine.getSize(i);
        ss_s_s* segment = i_this->mHands[i].segments;
        for (int j = 0; j < 20; j++, segment++, pos++, size++) {
            *pos = segment->pos;
            *size = segment->size;
        }
    }
    i_this->mpMorf->play(&i_this->eyePos, 0, 0);
    MtxTrans(i_this->home.pos.x, i_this->home.pos.y, i_this->home.pos.z, false);
    cMtx_YrotM(*calc_mtx, i_this->current.angle.y);
    if (i_this->mWall != 0) cMtx_XrotM(*calc_mtx, REG8_S(5) - 0x4000);
    J3DModel* model = i_this->mpMorf->getModel();
    model->setBaseTRMtx(*calc_mtx);
    cXyz delta(0.0f, 45.0f, 0.0f);
    MtxPosition(&delta, &i_this->eyePos);
    delta = player->eyePos - i_this->eyePos;
    int yaw = (i_this->mShake * 200) * cM_ssin(i_this->mFrame * 0x2F00);
    int pitch = (i_this->mShake * 200) * cM_scos(i_this->mFrame * 0x2C00);
    i_this->mEyeAngle.y = (REG0_S(0) + 1) * cM_atan2s(delta.x, delta.z) - i_this->current.angle.y + yaw;
    i_this->mEyeAngle.x = -((REG0_S(2) + 1) * cM_atan2s(delta.y, std::sqrtf(delta.x * delta.x + delta.z * delta.z)) + pitch);
    if (i_this->mShake != 0) i_this->mShake--;
    i_this->mpMorf->calc();
    return TRUE;
}

static BOOL daSs_IsDelete(ss_class*) { return TRUE; }

static BOOL daSs_Delete(ss_class* i_this) {
    dComIfG_resDeleteDemo(&i_this->mPhase, "Ss");
    return TRUE;
}

static BOOL useHeapInit(fopAc_ac_c* actor) {
    ss_class* i_this = (ss_class*)actor;
    if (!i_this->mLine.init(10, 20, TRUE)) return FALSE;
    i_this->mpMorf = new mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("Ss", dRes_INDEX_SS_BDL_SW_e), NULL, NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("Ss", dRes_INDEX_SS_BCK_WAIT_CLOSE_e), 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000022);
    if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) return FALSE;
    J3DModel* model = i_this->mpMorf->getModel();
    for (u16 i = 0; i < model->getModelData()->getJointNum(); i++) {
        if (i == SW_JNT_EYE_e) model->getModelData()->getJointNodePointer(i)->setCallBack(nodeCallBack);
    }
    model->setUserArea((u32)i_this);
    return TRUE;
}

static cPhs_State daSs_Create(fopAc_ac_c* actor) {
    static dCcD_SrcSph tg_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ dCcG_SE_METAL,
            /* SrcGObjTg HitMark */ dCcg_TgHitMark_Purple_e,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_Shield_e | dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGSphS
        {{
            /* Center */ {0.0f, 0.0f, 0.0f},
            /* Radius */ 15.0f,
        }},
    };
    fopAcM_SetupActor(actor, ss_class);
    ss_class* i_this = static_cast<ss_class*>(actor);
    cPhs_State phase = dComIfG_resLoad(&i_this->mPhase, "Ss");
    if (phase == cPhs_ERROR_e) return cPhs_ERROR_e;
    if (phase != cPhs_COMPLEATE_e) return phase;
    i_this->mWall = fopAcM_GetParam(i_this);
    i_this->mType = fopAcM_GetParam(i_this) >> 8;
    i_this->mSwitch = fopAcM_GetParam(i_this) >> 24;
    if (i_this->mSwitch == 0xFF) i_this->mSwitch = 0;
    if (i_this->mSwitch != 0 && dComIfGs_isSwitch(i_this->mSwitch, fopAcM_GetRoomNo(i_this))) return cPhs_ERROR_e;
    if (i_this->mType == 1) i_this->mMode = 10;
    else if (i_this->mType == 2 || i_this->mType == 3) i_this->mMode = 20;
    else i_this->mMode = 0;
    BOOL success = fopAcM_entrySolidHeap(i_this, useHeapInit, 0x7240);
    if (!success) return cPhs_ERROR_e;
    i_this->health = 2;
    i_this->mFrame = cM_rndF(10000.0f);
    i_this->max_health = i_this->health = 2;
    i_this->mStts.Init(50, 0, i_this);
    i_this->mSph.Set(tg_sph_src);
    i_this->mSph.SetStts(&i_this->mStts);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) {
            i_this->mHands[i].sph[j].Set(tg_sph_src);
            i_this->mHands[i].sph[j].SetStts(&i_this->mStts);
            i_this->mHands[i].sph[j].OffTgShield();
        }
    }
    return cPhs_COMPLEATE_e;
}

static actor_method_class l_daSs_Method = {
    (process_method_func)daSs_Create,
    (process_method_func)daSs_Delete,
    (process_method_func)daSs_Execute,
    (process_method_func)daSs_IsDelete,
    (process_method_func)daSs_Draw,
};

actor_process_profile_definition g_profile_SS = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_SS_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(ss_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_SS_e,
    /* Actor SubMtd */ &l_daSs_Method,
    /* Status       */ fopAcStts_UNK4000_e | fopAcStts_UNK40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};
