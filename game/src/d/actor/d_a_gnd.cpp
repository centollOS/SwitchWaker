/**
 * d_a_gnd.cpp
 * Boss - Ganondorf
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#include "d/d_camera.h"
#include "f_op/f_op_camera.h"
#include "d/actor/d_a_gnd.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_s_play.h"
#include "d/d_snap.h"
#include "m_Do/m_Do_graphic.h"
#include "d/d_demo.h"
#include "d/d_meter.h"
#include "d/d_particle_name.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_arrow.h"
#include "d/actor/d_a_pz.h"
#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_hostIO.h"
#include "m_Do/m_Do_controller_pad.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JUtility/JUTReport.h"
#include "res/Object/Gnd.h"

static cXyz non_pos(-20000.0f, -20000.0f, -20000.0f);
static bool hio_set;
static daGnd_HIO_c l_HIO;

static u32 attack_eff_joint_d[] = {
    0x1A, 0x2C, 0x04, 0x07, 0x1A, 0x2C,
};

static u32 attack_eff_id[] = {
    dPa_name::ID_AK_SN_GNDAATTACK1HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDAATTACK1HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDAATTACK1FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDAATTACK1FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDAATTACK1BLURL00,
    dPa_name::ID_AK_SP_GNDAATTACK1BLURR00,
    dPa_name::ID_AK_SN_GNDAATTACK2HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDAATTACK2HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDAATTACK2FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDAATTACK2FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDAATTACK2BLURL00,
    dPa_name::ID_AK_SP_GNDAATTACK2BLURR00,
    dPa_name::ID_AK_SN_GNDAATTACK3HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDAATTACK3HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDAATTACK3FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDAATTACK3FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDAATTACK3BLURL00,
    dPa_name::ID_AK_SP_GNDAATTACK3BLURR00,
    dPa_name::ID_AK_SN_GNDBATTACK1HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDBATTACK1HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDBATTACK1FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDBATTACK1FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDBATTACK1BLURL00,
    dPa_name::ID_AK_SP_GNDBATTACK1BLURR00,
    dPa_name::ID_AK_SN_GNDBATTACK2HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDBATTACK2HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDBATTACK2FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDBATTACK2FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDBATTACK2BLURL00,
    dPa_name::ID_AK_SP_GNDBATTACK2BLURR00,
    0x8396, 0x8397, 0x8398, 0x8399,
    dPa_name::ID_AK_SP_GNDBATTACK3TBLURL00,
    dPa_name::ID_AK_SP_GNDBATTACK3TBLURR00,
    0x839A, 0x839B, 0x839C, 0x839D,
    dPa_name::ID_AK_SP_GNDBATTACK3ABLURL00,
    dPa_name::ID_AK_SP_GNDBATTACK3ABLURR00,
    0x839E, 0x839F, 0x83A0, 0x83A1,
    dPa_name::ID_AK_SP_GNDBATTACK3EBLURL00,
    dPa_name::ID_AK_SP_GNDBATTACK3EBLURR00,
    dPa_name::ID_AK_SN_GNDJATTACKTHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKTHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDJATTACKTFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKTFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDJATTACKTBLURL00,
    dPa_name::ID_AK_SP_GNDJATTACKTBLURR00,
    dPa_name::ID_AK_SN_GNDJATTACKJHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKJHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDJATTACKJFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKJFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDJATTACKJBLURL00,
    dPa_name::ID_AK_SP_GNDJATTACKJBLURR00,
    dPa_name::ID_AK_SN_GNDJATTACKAHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKAHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDJATTACKAFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKAFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDJATTACKABLURL00,
    dPa_name::ID_AK_SP_GNDJATTACKABLURR00,
    dPa_name::ID_AK_SN_GNDJATTACKEHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKEHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDJATTACKEFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDJATTACKEFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDJATTACKEBLURL00,
    dPa_name::ID_AK_SP_GNDJATTACKEBLURR00,
    dPa_name::ID_AK_SN_GNDNATTACK1HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDNATTACK1HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDNATTACK1FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDNATTACK1FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDNATTACK1BLURL00,
    dPa_name::ID_AK_SP_GNDNATTACK1BLURR00,
    dPa_name::ID_AK_SN_GNDNATTACK2HANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDNATTACK2HANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDNATTACK2FOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDNATTACK2FOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDNATTACK2BLURL00,
    dPa_name::ID_AK_SP_GNDNATTACK2BLURR00,
    dPa_name::ID_AK_SN_GNDCOUNTERLHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDCOUNTERLHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDCOUNTERLFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDCOUNTERLFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDCOUNTERLBLURL00,
    dPa_name::ID_AK_SP_GNDCOUNTERLBLURR00,
    dPa_name::ID_AK_SN_GNDCOUNTERRHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDCOUNTERRHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDCOUNTERRFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDCOUNTERRFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDCOUNTERRBLURL00,
    dPa_name::ID_AK_SP_GNDCOUNTERRBLURR00,
    dPa_name::ID_AK_SN_GNDCOUNTERRHANDSPLASHL00,
    dPa_name::ID_AK_SN_GNDCOUNTERRHANDSPLASHR00,
    dPa_name::ID_AK_SN_GNDCOUNTERRFOOTSPLASHL00,
    dPa_name::ID_AK_SN_GNDCOUNTERRFOOTSPLASHR00,
    dPa_name::ID_AK_SP_GNDCOUNTERRBLURL00,
    dPa_name::ID_AK_SP_GNDCOUNTERRBLURR00,
    0, 0, 0, dPa_name::ID_AK_SN_GNDKICKSPLASH00, 0, 0,
};

static u32 ke_set_index[] = {0x1B, 0x1B, 0x2D, 0x2D};
static f32 ke_set_offsetxz[] = {-4.0f, 4.0f, -4.0f, 4.0f};

/* 000000EC-0000023C       .text __ct__11daGnd_HIO_cFv */
daGnd_HIO_c::daGnd_HIO_c() {
    mNo = -1;
    m05 = 0;
    m06 = 0;
    m07 = 1;
    m08 = 45.0f;
    m0C = 1.0f;
    m10 = 0;
    m11 = 0;
    m12 = 1;
    m14 = 5.0f;
    m18 = 629.0f;
    m1C = 1200.0f;
    m20 = 25.0f;
    m24 = 11.0f;
    m28 = -0.5f;
    m2C = 100.0f;
    m30 = -30.0f;
    m34 = 20.0f;
    m38 = -2.0f;
    m3C = 630.0f;
    m40 = 400.0f;
    m44 = 0;
    m45 = 1;
    m46 = 1;
    m48 = 35.0f;
    m4C = 0.0f;
    m50 = 5.0f;
    m54 = 15.0f;
    m58 = 0.0f;
    m5C = 30.0f;
    m60 = -2.0f;
    m64 = 8.0f;
    m68 = 30;
    m6A = 30;
    m6C = 0;
    m6E = 15;
    m70 = 10;
    m72 = 0;
    m74 = 10;
    m76 = 10;
    m78 = 600.0f;
    m7C = 10;
    m7E = 1;
    m80 = 30;
    m82 = 3;
    m84 = 6;
    m86 = 70;
    m88 = 0;
    m89 = 0;
}

/* 0000023C-000003CC       .text j_demo__FP9gnd_class */
static u8 j_demo(gnd_class* i_this) {
    if (i_this->demoActorID == 0) {
        if (i_this->m15B0 != 0) {
            i_this->m15B0 = 0;
        }
    } else {
        i_this->m15B0 = 1;
        dDemo_actor_c* demo_actor = dComIfGp_demo_getActor(i_this->demoActorID);

        J3DAnmTexPattern* btp = demo_actor->getP_BtpData("Gnd");
        if (btp != NULL) {
            i_this->mpBtpAnm->init(i_this->mpMorf->getModel()->getModelData(), btp, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0);
        }

        J3DAnmTextureSRTKey* btk = demo_actor->getP_BtkData("Gnd");
        if (btk != NULL) {
            i_this->mpBtkAnm->init(i_this->mpMorf->getModel()->getModelData(), btk, TRUE, J3DFrameCtrl::EMode_NONE, 1.0f, 0, -1, true, 0);
        }

        J3DAnmTevRegKey* brk = (J3DAnmTevRegKey*)demo_actor->getP_BrkData("Gnd");
        if (brk != NULL) {
            i_this->mpBrkAnm->init(i_this->mpMorf->getModel()->getModelData(), brk, TRUE, J3DFrameCtrl::EMode_LOOP, 1.0f, 0, -1, true, 0);
        }

        dDemo_setDemoData(i_this, 0x6A, i_this->mpMorf, "Gnd");
    }
    return i_this->m15B0;
}

/* 000003CC-000003EC       .text checkGround__FP9gnd_classf */
static BOOL checkGround(gnd_class* i_this, f32 param_2) {
    return i_this->current.pos.y < 1.0f + param_2;
}

/* 000003EC-00000510       .text setRipple__FP9gnd_class */
// The demo never calls this, so the linker dead-strips its code out of the REL. Its function-scope
// static and its pooled float constant survive in .bss/.rodata, so it has to stay for the demo too.
static void setRipple(gnd_class* i_this) {
    if (checkGround(i_this, 0.0f)) {
        if (i_this->mAcch.ChkGroundHit() &&
            dComIfG_Bgsp()->GetAttributeCode(i_this->mAcch.m_gnd) == dBgS_Attr_WATER_e &&
            i_this->mRippleCb.getEmitter() == NULL)
        {
            static cXyz ripple_scale(1.0f, 1.0f, 1.0f);
            dComIfGp_particle_setShipTail(
                dPa_name::ID_AK_JN_HAMON00, &i_this->current.pos, NULL, &ripple_scale, 0xFF, &i_this->mRippleCb
            );
            if (i_this->mRippleCb.getEmitter() != NULL) {
                i_this->mRippleCb.setRate(DEMO_SELECT(1.8f, 0.0f));
            }
        }
    } else {
        i_this->mRippleCb.end();
    }
}

static fopAc_ac_c* pz;

/* 0000054C-000005DC       .text splash_set__FP9gnd_class */
static void splash_set(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;

    JPABaseEmitter* emitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDLANDINGSPLASH00, &actor->current.pos);
    if (emitter != NULL) {
        emitter->setGlobalPrmColor(i_this->mTevStr2.mColorC0.r, i_this->mTevStr2.mColorC0.g, i_this->mTevStr2.mColorC0.b);
    }
}

/* 000005DC-000006B4       .text attack_eff_set__FP9gnd_classi */
static void attack_eff_set(gnd_class* i_this, int param_2) {
    fopAc_ac_c* actor = i_this;

    for (int i = 0; i < 6; i++) {
        if (attack_eff_id[param_2 * 6 + i] != 0) {
            i_this->mpAttackEff[i] = dComIfGp_particle_set(attack_eff_id[param_2 * 6 + i], &actor->current.pos);
            if (i <= 3) {
                i_this->mpAttackEff[i]->setGlobalPrmColor(i_this->mTevStr2.mColorC0.r, i_this->mTevStr2.mColorC0.g, i_this->mTevStr2.mColorC0.b);
            }
        }
    }
}

/* 000006B4-00000738       .text attack_eff_move__FP9gnd_class */
static void attack_eff_move(gnd_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();
    for (int i = 0; i < 6; i++) {
        if (i_this->mpAttackEff[i] != NULL) {
            i_this->mpAttackEff[i]->setGlobalRTMatrix(model->getAnmMtx(attack_eff_joint_d[i]));
        }
    }
}

/* 00000738-0000077C       .text attack_eff_remove__FP9gnd_class */
static void attack_eff_remove(gnd_class* i_this) {
    for (int i = 0; i < 6; i++) {
        if (i_this->mpAttackEff[i] != NULL) {
            i_this->mpAttackEff[i]->becomeInvalidEmitter();
            i_this->mpAttackEff[i] = NULL;
        }
    }
}

/* 0000077C-00000900       .text anm_init__FP9gnd_classifUcfi */
static void anm_init(gnd_class* i_this, int bckFileIdx, f32 morf, u8 loopMode, f32 speed, int soundFileIdx) {
    if (soundFileIdx >= 0) {
        i_this->mpMorf->setAnm(
            (J3DAnmTransform*)dComIfG_getObjectRes("Gnd", bckFileIdx),
            loopMode,
            morf,
            speed,
            0.0f,
            -1.0f,
            dComIfG_getObjectRes("Gnd", soundFileIdx)
        );
    } else {
        i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("Gnd", bckFileIdx), loopMode, morf, speed, 0.0f, -1.0f, NULL);
    }

    attack_eff_remove(i_this);

    int attack_anm[18] = {
        dRes_INDEX_GND_BCK_AATTACK1_e,
        dRes_INDEX_GND_BCK_AATTACK2_e,
        dRes_INDEX_GND_BCK_AATTACK3_e,
        dRes_INDEX_GND_BCK_BATTACK1_e,
        dRes_INDEX_GND_BCK_BATTACK2_e,
        dRes_INDEX_GND_BCK_BATTACK3_TAME_e,
        dRes_INDEX_GND_BCK_BATTACK3_ATTACK_e,
        dRes_INDEX_GND_BCK_BATTACK3_END_e,
        dRes_INDEX_GND_BCK_JATTACK_TAME_e,
        dRes_INDEX_GND_BCK_JATTACK_JUMP_e,
        dRes_INDEX_GND_BCK_JATTACK_ATTACK_e,
        dRes_INDEX_GND_BCK_JATTACK_END_e,
        dRes_INDEX_GND_BCK_NOM_ATTACK1_e,
        dRes_INDEX_GND_BCK_NOM_ATTACK2_e,
        dRes_INDEX_GND_BCK_COUNTERL_e,
        dRes_INDEX_GND_BCK_COUNTERR_e,
        dRes_INDEX_GND_BCK_COMBO_GANON_e,
        dRes_INDEX_GND_BCK_KERI2_e,
    };

    for (int i = 0; i < 18; i++) {
        if (bckFileIdx == attack_anm[i]) {
            attack_eff_set(i_this, i);
            break;
        }
    }
}

/* 00000900-0000094C       .text z_s_sub__FPvPv */
static void* z_s_sub(void* param_1, void*) {
    if (fopAcM_IsActor(param_1) && fopAcM_GetName(param_1) == fpcNm_PZ_e) {
        return param_1;
    }
    return NULL;
}

/* 0000094C-00000C38       .text daGnd_Draw__FP9gnd_class */
static BOOL daGnd_Draw(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;

#if VERSION > VERSION_DEMO
    s16 blure = i_this->m155C;
    if (blure > 1) {
        mDoGph_gInf_c::setBlureRate(blure);
        mDoGph_gInf_c::onBlure();
    } else if (blure == 1) {
#if VERSION > VERSION_DEMO
        i_this->m155C = 0;
#endif
        mDoGph_gInf_c::offBlure();
    }

#endif

    J3DModel* model = i_this->mpMorf->getModel();
    g_env_light.settingTevStruct(TEV_TYPE_ACTOR, &actor->current.pos, &actor->tevStr);

#if VERSION > VERSION_DEMO
    s16 add = i_this->m142C;
    if (add != 0) {
        s16 fog_r = actor->tevStr.mFogColor.r + i_this->m1426;
        if (fog_r > 0xFF) {
            fog_r = 0xFF;
        }
        s16 fog_g = actor->tevStr.mFogColor.g + i_this->m1426;
        if (fog_g > 0xFF) {
            fog_g = 0xFF;
        }
        s16 fog_b = actor->tevStr.mFogColor.g + (i_this->m1426 / 2);
        if (fog_b > 0xFF) {
            fog_b = 0xFF;
        }

        s16 limit = REG8_S(5) + 0x14;
        if (add > limit) {
            cLib_addCalcAngleS2(&i_this->m1426, 0x118, 1, 0x1E);
            cLib_addCalc2(&i_this->m1428, -50000.0f, 1.0f, 5000.0f);
        } else {
            cLib_addCalcAngleS2(&i_this->m1426, 0, 1, 0xE);
            cLib_addCalc0(&i_this->m1428, 1.0f, 2500.0f);
        }

        actor->tevStr.mFogColor.r = (u8)fog_r;
        actor->tevStr.mFogColor.g = (u8)fog_g;
        actor->tevStr.mFogColor.b = (u8)fog_b;
        actor->tevStr.mFogStartZ += i_this->m1428;
    }
#endif

    g_env_light.setLightTevColorType(model, &actor->tevStr);
    i_this->mpBrkAnm->entry(model->getModelData());
    i_this->mpBtkAnm->entry(model->getModelData());
    i_this->mpBtpAnm->entry(model->getModelData());
    i_this->mpMorf->entryDL();

    cXyz shadow_pos(actor->current.pos.x, 400.0f + actor->current.pos.y + REG0_F(18), actor->current.pos.z);
    i_this->mShadowId = dComIfGd_setShadow(
        i_this->mShadowId,
        1,
        model,
        &shadow_pos,
        1300.0f + REG0_F(19),
        200.0f,
        actor->current.pos.y,
        i_this->mAcch.GetGroundH(),
        i_this->mAcch.m_gnd,
        &actor->tevStr,
        0,
        1.0f,
        dDlst_shadowControl_c::getSimpleTex()
    );

    dSnap_RegistFig(DSNAP_TYPE_GND, actor, 1.0f, 1.0f, 1.0f);

    if (l_HIO.m07 != 0) {
        i_this->mLineMat.update(20, 2.25f + REG0_F(3), (GXColor){0xFF, 0x64, 0x00, 0xFF}, 2, &actor->tevStr);
        dComIfGd_set3DlineMat(&i_this->mLineMat);
    }

    return TRUE;
}

/* 00000C38-00000C6C       .text player_view_check__FP9gnd_classs */
static BOOL player_view_check(gnd_class* i_this, s16 param_2) {
    s16 diff = i_this->shape_angle.y - param_2;
    if (diff < 0) {
        diff = -diff;
    }
    if ((u16)diff < 0x4000) {
        return TRUE;
    }
    return FALSE;
}

/* 00000C6C-00000F24       .text ke_control__FP9gnd_classP8gnd_ke_sf */
static void ke_control(gnd_class* i_this, gnd_ke_s* param_2, f32 param_3) {
    cXyz local_118;
    cXyz local_124;
    s32 i;
    f32 x;
    f32 ground_y = 3.0f + i_this->mAcch.GetGroundH();
    cXyz* pos = &param_2->mPos[1];
    cXyz* vel = &param_2->mVel[1];

    local_118.x = 0.0f;
    local_118.y = 0.0f;
    local_118.z = l_HIO.m08 * param_3 * 0.5f;
    if (param_3 <= 0.05f) {
        local_118.z = 0.0f;
    }

    f32 gravity = -5.0f + REG0_F(1);
    f32 damp = 0.73f + REG0_F(2);
    for (i = 1; i < 20; i++, pos++, vel++) {
        x = vel->x + (pos->x - pos[-1].x);
        f32 y = pos->y + vel->y + gravity;
        if (y < ground_y) {
            y = ground_y;
        }
        f32 y_diff = y - pos[-1].y;
        f32 z = vel->z + (pos->z - pos[-1].z);
        s16 angX = -cM_atan2s(y_diff, z);
        s32 angY = cM_atan2s(x, std::sqrtf(SQUARE(y_diff) + SQUARE(z)));
        cMtx_XrotS(*calc_mtx, angX);
        cMtx_YrotM(*calc_mtx, angY);
        MtxPosition(&local_118, &local_124);
        *vel = *pos;
        pos->x = pos[-1].x + local_124.x;
        pos->y = pos[-1].y + local_124.y;
        pos->z = pos[-1].z + local_124.z;
        vel->x = damp * (pos->x - vel->x);
        vel->y = damp * (pos->y - vel->y);
        vel->z = damp * (pos->z - vel->z);
    }
}

/* 00000F24-00000F68       .text ke_pos_set__FP9gnd_classP8gnd_ke_si */
static void ke_pos_set(gnd_class* i_this, gnd_ke_s* param_2, int param_3) {
    cXyz* pos = i_this->mLineMat.getPos(param_3);
    for (s32 i = 0; i < 20; pos++, i++) {
        *pos = param_2->mPos[i];
    }
}

/* 00000F68-00001140       .text ke_move__FP9gnd_class */
static void ke_move(gnd_class* i_this) {
    cXyz offset;
    cXyz axis;
    gnd_ke_s* ke = i_this->mKe;
    J3DModel* model = i_this->mpMorf->getModel();
    for (s32 i = 0; i < 4; i++, ke++) {
        MTXCopy(model->getAnmMtx(ke_set_index[i]), *calc_mtx);
        axis.set((*calc_mtx)[0][0], (*calc_mtx)[1][0], (*calc_mtx)[2][0]);
        f32 len = axis.abs();
        if (dComIfGp_evmng_startCheck("endhr") && dComIfGp_demo_get()->getFrameNoMsg() >= 0x12C) {
            len = 0.0f;
        }
#if VERSION == VERSION_DEMO
        offset.set(-20.0f + REG0_F(4), ke_set_offsetxz[i], ke_set_offsetxz[i]);
#else
        offset.x = -20.0f + REG0_F(4);
        offset.y = ke_set_offsetxz[i];
        offset.z = ke_set_offsetxz[i];
#endif
        MtxPosition(&offset, &ke->mPos[0]);
        ke_control(i_this, ke, len);
        ke_pos_set(i_this, ke, i);
    }
}

/* 00001140-00001278       .text pos_move__FP9gnd_classSc */
static void pos_move(gnd_class* i_this, s8 param_2) {
    fopAc_ac_c* actor = i_this;
    cXyz vec;

    if (param_2 == 0) {
        vec = i_this->m2D4 - actor->current.pos;
        s16 angY = cM_atan2s(vec.x, vec.z);
        cLib_addCalcAngleS2(&actor->current.angle.y, angY, DEMO_SELECT(REG0_S(3) + 5, 5), i_this->m2F0 * i_this->m2F4);
        cLib_addCalc2(&i_this->m2F4, 1.0f, 1.0f, 0.05f);
    }

    cLib_addCalc2(&actor->speedF, i_this->m2F8, 1.0f, 6.0f);
    vec.x = 0.0f;
    vec.y = 0.0f;
    vec.z = actor->speedF;
    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    cXyz sp;
    MtxPosition(&vec, &sp);
    actor->speed.x = sp.x;
    actor->speed.z = sp.z;
    actor->current.pos += actor->speed;
    actor->current.pos.y += actor->speed.y;
    actor->speed.y += actor->gravity;
    actor->gravity = -5.0f;
}

/* 00001278-00001334       .text wait_set__FP9gnd_class */
static void wait_set(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;

    if (i_this->m3D8 <= 1) {
        anm_init(i_this, dRes_INDEX_GND_BCK_WAIT_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_LAUGH_WAIT, 0);
    } else {
        anm_init(i_this, dRes_INDEX_GND_BCK_WAIT2_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
    }
}

/* 00001334-00001974       .text move0__FP9gnd_class */
static void move0(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    f32 dist = fopAcM_searchActorDistance(actor, player);
    int frame = (int)i_this->mpMorf->getFrame();
    s8 track = 0;

    switch (i_this->m2D0) {
    case 0:
        anm_init(i_this, dRes_INDEX_GND_BCK_WALK_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, dRes_INDEX_GND_BAS_WALK_e);
        i_this->m2D0++;
        break;
    case 1:
        i_this->m2F8 = l_HIO.m14;
        track = 1;
        if (dist < l_HIO.m18) {
            anm_init(i_this, dRes_INDEX_GND_BCK_WAIT2_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m2D0 = 2;
        } else if (dist > l_HIO.m1C) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_START_e, 10.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_HI_JUMP_START_e);
            i_this->m2D0 = 3;
            actor->speedF = 0.0f;
            i_this->m2F8 = 0.0f;
        }
        break;
    case 2:
        track = 1;
        i_this->m2F8 = 0.0f;
        if (player_view_check(i_this, fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)))) {
            if (dist > l_HIO.m3C && i_this->m302[1] == 0) {
                i_this->m2D0 = 0;
            } else if (dist < l_HIO.m2C) {
                i_this->m2D0 = 20;
            } else if (i_this->m302[1] == 0) {
                if (cM_rndF(100.0f) < l_HIO.m48) {
                    i_this->m2CE = 2;
                    if (cM_rndF(1.0f) < 0.5f) {
                        i_this->m2D0 = 0;
                    } else {
                        i_this->m2D0 = 1;
                    }
                } else {
                    i_this->m2CE = 1;
                    if (i_this->m3D8 == 0 || l_HIO.m11 == 1) {
                        i_this->m2D0 = 0;
                    } else if (i_this->m3D8 == 1 || l_HIO.m11 == 2) {
                        i_this->m2D0 = 5;
                    } else if (cM_rndF(1.0f) < 0.5f) {
                        i_this->m2D0 = 0;
                    } else {
                        i_this->m2D0 = 5;
                    }
                }
            }
        }
        break;
    case 3:
        actor->gravity = l_HIO.m28;
        if (frame == 12) {
            actor->speed.y = l_HIO.m24;
            f32 jump_speed = l_HIO.m20;
            actor->speedF = jump_speed;
            i_this->m2F8 = jump_speed;
            splash_set(i_this);
        }
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_LOOP_e, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m2D0++;
        }
        break;
    case 4:
        mDoAud_seStart(JA_SE_CM_GN_ROBE, &actor->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(actor)));
        actor->gravity = l_HIO.m28;
        if (actor->speed.y < 0.0f) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_END1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0++;
        }
        break;
    case 5:
        mDoAud_seStart(JA_SE_CM_GN_ROBE, &actor->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(actor)));
        actor->gravity = l_HIO.m28;
        i_this->m2F0 = 0.0f;
        if (checkGround(i_this, 0.0f)) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_END2_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0 = 6;
            splash_set(i_this);
        }
        break;
    case 6:
        i_this->m2F8 = 0.0f;
        i_this->m2F0 = 0.0f;
        if (i_this->mpMorf->isStop()) {
            i_this->m2D0 = 2;
            anm_init(i_this, dRes_INDEX_GND_BCK_WAIT2_e, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
        }
        break;
    case 20:
        anm_init(i_this, dRes_INDEX_GND_BCK_JUMP1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_JUMP1_e);
        i_this->m2D0 = 21;
        break;
    case 21:
        track = 1;
        i_this->m2F8 = 0.0f;
        actor->speedF = 0.0f;
        if (i_this->mpMorf->isStop()) {
            actor->speed.y = l_HIO.m34;
            f32 jump_speed = l_HIO.m30;
            actor->speedF = jump_speed;
            i_this->m2F8 = jump_speed;
            anm_init(i_this, dRes_INDEX_GND_BCK_JUMP2_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0 = 22;
        }
        break;
    case 22:
        actor->gravity = l_HIO.m38;
        track = 1;
        if (checkGround(i_this, 0.0f)) {
            anm_init(i_this, dRes_INDEX_GND_BCK_JUMP3_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_JUMP3_e);
            i_this->m2D0 = 23;
        }
        break;
    case 23:
        i_this->m2F8 = 0.0f;
        if (i_this->mpMorf->isStop()) {
            i_this->m2D0 = 2;
            wait_set(i_this);
            i_this->m302[1] = 20;
        }
        break;
    }

    if (track == 1) {
        i_this->m2D4.x = player->current.pos.x;
        i_this->m2D4.y = player->current.pos.y;
        i_this->m2D4.z = player->current.pos.z;
    }
    pos_move(i_this, 0);
    i_this->m2F0 = 2000.0f;
    cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 2, 0x1000);
}

/* 00001974-000028D0       .text attack0__FP9gnd_class */
static void attack0(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    f32 dist = fopAcM_searchActorDistance(actor, player);
    int frame = (int)i_this->mpMorf->getFrame();
    s8 track = 0;
    s8 turn_player = 0;
    s8 done = 0;
    s8 lerp_xz = 0;
    s8 no_parry = 0;

    switch (i_this->m2D0) {
    case 0:
        anm_init(i_this, dRes_INDEX_GND_BCK_AATTACK1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_AATTACK1_e);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_1, 0);
        i_this->m2D0++;
        no_parry = 1;
        break;
    case 1:
        track = 1;
        no_parry = 1;
        if (frame < 0x17) {
            i_this->m2F8 = l_HIO.m54;
        } else {
            i_this->m2F8 = l_HIO.m4C;
        }
        if (frame >= 0xD && frame <= 0x10) {
            i_this->m13B8[0] = 1;
        }
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_AATTACK2_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_AATTACK2_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_2, 0);
            i_this->m2D0++;
        }
        break;
    case 2:
        track = 1;
        no_parry = 1;
        if (frame < 0xC) {
            i_this->m2F8 = l_HIO.m50;
        } else {
            i_this->m2F8 = l_HIO.m4C;
        }
        if (frame >= 5 && frame <= 0xA) {
            i_this->m13B8[0] = 1;
        }
        if (i_this->mpMorf->isStop()) {
            if (i_this->m3D4 != 0) {
                i_this->m3D4 = 0;
                goto keri_init;
            }
            anm_init(i_this, dRes_INDEX_GND_BCK_AATTACK3_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_AATTACK3_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_3, 0);
            i_this->m2D0++;
        }
        if (frame > l_HIO.m6A) {
            i_this->mNextParryOpeningType = 1;
        }
        break;
    case 3:
        no_parry = 1;
        if (frame < 0x19) {
            i_this->m2F8 = l_HIO.m50;
        } else {
            i_this->m2F8 = l_HIO.m4C;
        }
        if (frame >= 6 && frame <= 0x16) {
            i_this->m13B8[0] = 1;
            i_this->m13B8[1] = 1;
        }
        if (frame >= l_HIO.m6C && frame <= l_HIO.m6E) {
            i_this->mNextParryOpeningType = 1;
        }
        if (i_this->mpMorf->isStop()) {
            i_this->m2CE = 0;
            i_this->m2D0 = 0;
        }
        break;
    case 5:
        anm_init(i_this, dRes_INDEX_GND_BCK_BATTACK1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BATTACK1_e);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_1, 0);
        i_this->m2D0++;
        no_parry = 1;
        break;
    case 6:
        no_parry = 1;
        if (frame >= 9 && frame < 0x14) {
            i_this->m2F8 = l_HIO.m54;
        } else {
            i_this->m2F8 = l_HIO.m4C;
        }
        if (frame >= 0xD && frame <= 0x11) {
            i_this->m13B8[1] = 1;
        }
        track = 1;
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_BATTACK2_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BATTACK2_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_3, 0);
            i_this->m2D0 = 7;
        }
        break;
    case 7:
        track = 1;
        no_parry = 1;
        i_this->m2F8 = l_HIO.m50;
        if (frame >= 8 && frame <= 0x17) {
            i_this->m13B8[0] = 1;
            i_this->m13B8[1] = 1;
        }
        if (i_this->mpMorf->isStop()) {
            if (i_this->m3D4 != 0) {
                i_this->m3D4 = 0;
                goto keri_init;
            }
            anm_init(i_this, dRes_INDEX_GND_BCK_BATTACK3_TAME_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BATTACK3_TAME_e);
            i_this->m2D0 = 8;
        }
        break;
    case 8:
        no_parry = 1;
        i_this->m2F8 = 0.0f;
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_BATTACK3_ATTACK_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BATTACK3_ATTACK_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_2, 0);
            i_this->m2D0 = 9;
            actor->speed.y = l_HIO.m5C;
            i_this->m2F8 = dist * (0.015f + REG0_F(9));
            splash_set(i_this);
        }
        break;
    case 9:
        no_parry = 1;
        if (actor->speed.y > 0.0f) {
            i_this->m2E0 = actor->speed;
        }
        lerp_xz = 1;
        turn_player = 1;
        if (frame > l_HIO.m70) {
            i_this->mNextParryOpeningType = 2;
        }
        actor->gravity = l_HIO.m60;
        i_this->m2F0 = 0.0f;
        if (checkGround(i_this, 0.0f)) {
            anm_init(i_this, dRes_INDEX_GND_BCK_BATTACK3_END_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BATTACK3_END_e);
            i_this->m2D0 = 10;
            splash_set(i_this);
        }
        break;
    case 10:
        no_parry = 1;
        if (i_this->m3D8 >= 2) {
            i_this->m3D7 = 2;
        }
        actor->current.angle.y = actor->shape_angle.y;
        if (frame >= l_HIO.m72 && frame <= l_HIO.m74) {
            i_this->mNextParryOpeningType = 2;
        }
        i_this->m2F8 = 0.0f;
        if (frame >= 3 && frame <= 5) {
            i_this->m13B8[0] = 1;
            i_this->m13B8[1] = 1;
            i_this->m13C4[1] = 60.0f;
            i_this->m13C4[0] = 60.0f;
        }
        if (i_this->mpMorf->isStop()) {
            i_this->m2D0 = 11;
            i_this->m302[0] = l_HIO.m76;
        }
        break;
    case 11:
        if (i_this->m3D8 >= 2) {
            i_this->m3D7 = 2;
        }
        actor->current.angle.y = actor->shape_angle.y;
        if (i_this->m302[0] == 0) {
            done = 1;
        }
        break;
    case 15:
        track = 1;
        i_this->m2F0 = 8000.0f;
        i_this->m2F8 = 0.0f;
        if (i_this->m302[0] == 0) {
            anm_init(i_this, dRes_INDEX_GND_BCK_JATTACK_TAME_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_JATTACK_TAME_e);
            i_this->m2D0++;
        }
        break;
    case 16:
        track = 1;
        i_this->m2F0 = 8000.0f;
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_JATTACK_JUMP_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_4, 0);
            i_this->m2D0++;
            actor->speed.y = 30.0f + REG0_F(9);
            i_this->m2F8 = dist * (0.025f + REG0_F(10));
            splash_set(i_this);
        }
        break;
    case 17:
        actor->gravity = -2.0f + REG0_F(11);
        i_this->m2F0 = 5000.0f;
        if (actor->speed.y <= 0.0f) {
            anm_init(i_this, dRes_INDEX_GND_BCK_JATTACK_ATTACK_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_JATTACK_ATTACK_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_5, 0);
            i_this->m2D0++;
            i_this->m2E0 = actor->speed;
            daPy_lk_c* link = daPy_getPlayerLinkActorClass();
            link->offModeFlg(daPy_lk_c::ModeFlg_DAMAGE);
            link->mDamageWaitTimer = 0;
        }
        break;
    case 18:
        lerp_xz = 1;
        i_this->m13B8[0] = 4;
        i_this->m13B8[1] = 4;
        if (checkGround(i_this, 0.0f)) {
            anm_init(i_this, dRes_INDEX_GND_BCK_JATTACK_END_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_JATTACK_END_e);
            i_this->m2D0++;
            actor->speedF = 0.0f;
            i_this->m2F8 = 0.0f;
            splash_set(i_this);
        }
        break;
    case 19:
        if (frame <= 4) {
            i_this->m13B8[0] = 4;
            i_this->m13B8[1] = 4;
        }
        if (i_this->mpMorf->isStop()) {
            done = 1;
        }
        break;
    case 20:
    keri_init:
        no_parry = 1;
        anm_init(i_this, dRes_INDEX_GND_BCK_KERI1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_KERI1_e);
        i_this->m2D0 = 0x15;
        break;
    case 21:
        no_parry = 1;
        i_this->m2F0 = 5000.0f;
        i_this->m2F8 = l_HIO.m64;
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_KERI2_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_4, 0);
            i_this->m2D0 = 0x16;
        }
        break;
    case 22:
        no_parry = 1;
        if (frame >= 0 && frame <= 4) {
            i_this->m13B8[1] = 3;
            i_this->m13C4[1] = 170.0f + REG0_F(12);
        }
        if (frame == 0x14) {
            if (i_this->m3D4 == 0) {
                i_this->m13D1 = REG0_S(4) + 0xD;
            }
        }
        if (i_this->mpMorf->isStop()) {
            if (i_this->m3D4 != 0) {
                i_this->m3D4 = 0;
                if (i_this->m3D8 == 0) {
                    i_this->m2F8 = 0.0f;
                    wait_set(i_this);
                    i_this->m2D0 = 0x17;
                    i_this->m302[0] = 0x96;
                } else {
                    i_this->m2D0 = 0xF;
                    i_this->m302[0] = REG0_S(7);
                }
            } else {
                no_parry = 1;
                i_this->m2D0 = 0x1A;
                wait_set(i_this);
                i_this->m302[0] = REG0_S(5) + 0x2D;
            }
        }
        break;
    case 23:
        if (i_this->m302[0] == 0) {
            anm_init(i_this, dRes_INDEX_GND_BCK_KUZUKU_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0 = 0x18;
        }
        break;
    case 24:
        if (i_this->mpMorf->isStop()) {
            if (player_view_check(i_this, fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)))) {
                done = 1;
            } else {
                anm_init(i_this, dRes_INDEX_GND_BCK_FURIMUKU_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                fopAcM_monsSeStart(actor, JA_SE_CV_GN_NOTICE, 0);
                i_this->m2D0 = 0x19;
                i_this->m302[0] = 0x14;
            }
        }
        break;
    case 25:
        turn_player = 1;
        if (i_this->m302[0] == 0) {
            done = 1;
        }
        break;
    case 26:
        i_this->m2F8 = 0.0f;
        no_parry = 1;
        if (i_this->m302[0] == 0) {
            done = 1;
        }
        break;
    }

    actor->speedF = i_this->m2F8;
    if (track != 0) {
        i_this->m2D4.x = player->current.pos.x;
        i_this->m2D4.y = player->current.pos.y;
        i_this->m2D4.z = player->current.pos.z;
        pos_move(i_this, 0);
    } else {
        pos_move(i_this, 1);
    }
    i_this->m2F0 = 2000.0f;
    if (turn_player == 0) {
        cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 4, 0x2000);
    } else {
        s16 target = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        cLib_addCalcAngleS2(&actor->shape_angle.y, target, 4, (s16)(turn_player << 12));
    }
    if (lerp_xz != 0) {
        cLib_addCalc2(&actor->current.pos.x, player->current.pos.x, 0.8f, std::fabsf(i_this->m2E0.x));
        cLib_addCalc2(&actor->current.pos.z, player->current.pos.z, 0.8f, std::fabsf(i_this->m2E0.z));
    }
    if (done != 0) {
        i_this->m2CE = 0;
        i_this->m2D0 = 0;
        actor->current.angle.y = actor->shape_angle.y;
        i_this->m302[1] = l_HIO.m68;
    }
    if (l_HIO.m44 == 0 && i_this->m3D8 < 2 && no_parry != 0) {
        i_this->m13CE = 0;
        i_this->m3D7 = 5;
    }
}

/* 000028D0-00002B68       .text attack1__FP9gnd_class */
static void attack1(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    int frame = (int)i_this->mpMorf->getFrame();
    s8 done = 0;

    f32 rad = 110.0f + REG0_F(13);
    i_this->m13C4[1] = rad;
    i_this->m13C4[0] = rad;

    switch (i_this->m2D0) {
    case 0:
        anm_init(i_this, dRes_INDEX_GND_BCK_NOM_ATTACK1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_NOM_ATTACK1_e);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_4, 0);
        i_this->m2D0 = 5;
        break;
    case 1:
        anm_init(i_this, dRes_INDEX_GND_BCK_NOM_ATTACK2_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_NOM_ATTACK2_e);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_6, 0);
        i_this->m2D0 = 6;
        break;
    case 5:
        if (frame >= 0xF && frame <= 0x12) {
            i_this->m13B8[0] = 1;
        } else {
            i_this->m2D4.x = player->current.pos.x;
            i_this->m2D4.y = player->current.pos.y;
            i_this->m2D4.z = player->current.pos.z;
        }
        if (i_this->mpMorf->isStop()) {
            done = 1;
        }
        break;
    case 6:
        if (frame >= 0xC && frame <= 0x22) {
            i_this->m13B8[1] = 1;
        } else {
            i_this->m2D4.x = player->current.pos.x;
            i_this->m2D4.y = player->current.pos.y;
            i_this->m2D4.z = player->current.pos.z;
        }
        if (i_this->mpMorf->isStop()) {
            done = 1;
        }
        break;
    }

    i_this->m2F8 = l_HIO.m14;
    pos_move(i_this, 0);
    cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 2, 0x1000);
    if (done) {
        i_this->m2CE = 0;
        i_this->m2D0 = 0;
        actor->current.angle.y = actor->shape_angle.y;
        i_this->m302[1] = 0;
    }
}

/* 00002B68-00002D7C       .text attack2__FP9gnd_class */
static void attack2(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    int frame = (int)i_this->mpMorf->getFrame();
    s8 done = 0;

    f32 rad = 110.0f + REG0_F(13);
    i_this->m13C4[1] = rad;
    i_this->m13C4[0] = rad;
    i_this->m13CE = 0;

    switch (i_this->m2D0) {
    case 0:
        anm_init(i_this, dRes_INDEX_GND_BCK_COUNTERL_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_COUNTERL_e);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_4, 0);
        i_this->m2D0 = 2;
        break;
    case 1:
        anm_init(i_this, dRes_INDEX_GND_BCK_COUNTERR_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_COUNTERR_e);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_1, 0);
        i_this->m2D0 = 3;
        break;
    case 2:
        if (frame >= 4) {
            i_this->m13B8[0] = 1;
        }
        if (i_this->mpMorf->isStop()) {
            done = 1;
        }
        break;
    case 3:
        if (frame >= 6) {
            i_this->m13B8[1] = 1;
        }
        if (i_this->mpMorf->isStop()) {
            done = 1;
        }
        break;
    }

    if (done) {
        i_this->m2CE = 0;
        i_this->m2D0 = 0;
        actor->current.angle.y = actor->shape_angle.y;
        i_this->m302[1] = 0;
    }
}

/* 00002D7C-00003688       .text attackPZ__FP9gnd_class */
static void attackPZ(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    fopAc_ac_c* zelda = pz;
    cXyz offset;
    cXyz result;
    cXyz delta = zelda->current.pos - actor->current.pos;
    f32 dist = delta.abs();
    s16 ang = cM_atan2s(actor->current.pos.x - pz->current.pos.x, actor->current.pos.z - pz->current.pos.z);

    i_this->mAction = 5;
    i_this->m13CE = 0;
    int frame = (int)i_this->mpMorf->getFrame();
    s8 done = 0;

    switch (i_this->m2D0) {
    case 0:
        if (dist > 1000.0f + REG6_F(1)) {
            cMtx_YrotS(*calc_mtx, ang);
#if VERSION == VERSION_DEMO
            offset.set(0.0f, 0.0f, -(1000.0f + REG0_F(1)));
#else
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = -(1000.0f + REG0_F(1));
#endif
            MtxPosition(&offset, &result);
            pz->current.pos = actor->current.pos + result;
        }
        pz->shape_angle.y = ang;
        pz->current.angle.y = ang;
        i_this->m31A = ang;
        i_this->m2D0 = 1;
        i_this->m155E = 1;
        i_this->m302[0] = 20;
        ((daPz_c*)zelda)->m073F = 1;
        actor->speedF = 0.0f;
        i_this->m2F8 = 0.0f;
        mDoAud_changeBgmStatus(1);
    case 1:
        i_this->m2F0 = 0.0f;
        if (i_this->m302[0] == 0) {
            anm_init(i_this, dRes_INDEX_GND_BCK_KIZUKUL_e, 10.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_NOTICE, 0);
            i_this->mpBtkAnm->init(
                i_this->mpMorf->getModel()->getModelData(),
                (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BTK_KIZUKU_EYE_e),
                TRUE,
                J3DFrameCtrl::EMode_NONE,
                1.0f,
                0,
                -1,
                true,
                0
            );
            i_this->m2D0 = 2;
        }
        break;
    case 2:
        i_this->m2F0 = 0.0f;
        break;
    case 3:
        i_this->m2D0 = 5;
    case 5:
        anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_START_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_HI_JUMP_START_e);
        i_this->m2D0++;
        break;
    case 4:
        break;
    case 6:
        i_this->m2F0 = 8000.0f;
        actor->gravity = -0.5f + REG0_F(17);
        if (frame == 12) {
            actor->speed.y = 10.0f + REG0_F(19);
            i_this->m2F8 = dist * (0.018f + REG0_F(18));
            splash_set(i_this);
        }
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_LOOP_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m2D0++;
        }
        break;
    case 7:
        mDoAud_seStart(JA_SE_CM_GN_ROBE, &actor->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(actor)));
        actor->gravity = -0.5f + REG0_F(17);
        if (actor->speed.y < 0.0f) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_END1_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0++;
        }
        break;
    case 8:
        mDoAud_seStart(JA_SE_CM_GN_ROBE, &actor->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(actor)));
        actor->gravity = -0.5f + REG0_F(17);
        i_this->m2F0 = 0.0f;
        if (checkGround(i_this, 0.0f)) {
            anm_init(i_this, dRes_INDEX_GND_BCK_HI_JUMP_END2_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0++;
            i_this->m2E0 = actor->speed;
            splash_set(i_this);
        }
        break;
    case 9:
        done = 1;
        i_this->m2F8 = 0.0f;
        i_this->m2F0 = 0.0f;
        if (i_this->mpMorf->isStop()) {
            i_this->m2D0++;
            anm_init(i_this, dRes_INDEX_GND_BCK_BINTA1_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BINTA1_e);
        }
        break;
    case 10:
        done = 1;
        if (i_this->mpMorf->isStop()) {
            i_this->m2D0++;
            anm_init(i_this, dRes_INDEX_GND_BCK_BINTA2_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BINTA2_e);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_ZELDA, 0);
            i_this->mp13EC = dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDBINTASPLASH00, &actor->current.pos);
            if (i_this->mp13EC != NULL) {
                i_this->mp13EC->setGlobalPrmColor(i_this->mTevStr2.mColorC0.r, i_this->mTevStr2.mColorC0.g, i_this->mTevStr2.mColorC0.b);
            }
        }
        break;
    case 11:
        if (frame == REG0_S(5) + 2) {
#if VERSION > VERSION_DEMO
            i_this->m155C = 150;
#endif
            ((daPz_c*)zelda)->mTalkState = 1;
            fopAc_ac_c* p = pz;
            mDoAud_seStart(JA_SE_LK_LAST_HIT, &p->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(p)));
            dComIfGp_getVibration().StartShock(REG0_S(2) + 4, -0x21, cXyz(0.0f, 1.0f, 0.0f));
        }
        if (frame == REG0_S(6) + 2) {
            i_this->m155E = 4;
            i_this->m1560 = 0;
        }
        if (i_this->mp13EC != NULL) {
            if (i_this->mpMorf->isStop()) {
                i_this->mp13EC->becomeInvalidEmitter();
                i_this->mp13EC = NULL;
            } else {
                i_this->mp13EC->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(GND_JNT_GND_UDE_R3_e));
            }
        }
        break;
    }

    actor->speedF = i_this->m2F8;
    cMtx_YrotS(*calc_mtx, i_this->m31A);
#if VERSION == VERSION_DEMO
    offset.set(-20.0f + REG0_F(16), 0.0f, 100.0f + REG0_F(17));
#else
    offset.x = -20.0f + REG0_F(16);
    offset.y = 0.0f;
    offset.z = 100.0f + REG0_F(17);
#endif
    MtxPosition(&offset, &result);
    i_this->m2D4 = pz->current.pos + result;
    pos_move(i_this, 0);
    cLib_addCalcAngleS2(&actor->shape_angle.y, actor->current.angle.y, 4, 0x2000);
    if (done != 0) {
        cLib_addCalc2(&actor->current.pos.x, i_this->m2D4.x, 0.2f, std::fabsf(i_this->m2E0.x));
        cLib_addCalc2(&actor->current.pos.z, i_this->m2D4.z, 0.2f, std::fabsf(i_this->m2E0.z));
    }
}

/* 00003688-000038B0       .text attack_last__FP9gnd_class */
static void attack_last(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    int frame = (int)i_this->mpMorf->getFrame();

    i_this->m13C4[1] = 110.0f + REG0_F(13);
    i_this->m13CE = 0;

    switch (i_this->m2D0) {
    case 0:
        anm_init(i_this, dRes_INDEX_GND_BCK_LAST_ATTACK_TAME_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_ATTACK_7, 0);
        i_this->m2D0 = 1;
        break;
    case 1:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_LAST_ATTACK_ATTACK_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_LAST_ATTACK_ATTACK_e);
            i_this->m2D0 = 2;
        }
        if (frame > l_HIO.m7C) {
            i_this->mNextParryOpeningType = 4;
        }
        break;
    case 2:
        if (frame >= l_HIO.m7E && frame <= l_HIO.m80) {
            i_this->mNextParryOpeningType = 4;
        }
        if (frame >= 6 && frame <= 9) {
            i_this->m13B8[1] = 1;
        }
        if (i_this->mpMorf->isStop()) {
            i_this->m2CE = 0;
            i_this->m2D0 = 0;
            actor->current.angle.y = actor->shape_angle.y;
            i_this->m302[1] = 0;
        }
        break;
    }

    if (player->checkLastComboWait()) {
        i_this->m2CE = 30;
        i_this->m2D0 = 0;
        i_this->mAction = 5;
        i_this->m13CE = 0;
    }
}

static u32 bougyo_d[] = {
    dRes_INDEX_GND_BCK_BOUGYO1_e,
    dRes_INDEX_GND_BCK_BOUGYO2_e,
    dRes_INDEX_GND_BCK_BOUGYO4_e,
    dRes_INDEX_GND_BCK_BOUGYO6_e,
    dRes_INDEX_GND_BCK_BOUGYO7_e,
    dRes_INDEX_GND_BCK_BOUGYO5_e,
};

/* 000038B0-000042D0       .text defence0__FP9gnd_class */
static void defence0(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    s8 done = 0;

    i_this->m3D7 = 10;
    f32 dist = fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0));

    if (actor->current.pos.y > 200.0f + i_this->mAcch.GetGroundH() + REG0_F(11) && i_this->m2D0 < 15) {
        i_this->m2D0 = 20;
    }

    switch (i_this->m2D0) {
    case 0:
#if VERSION > VERSION_DEMO
        if (i_this->m3D8 == 2 && player->current.pos.y - i_this->mAcch.GetGroundH() > 300.0f + REG0_F(19)) {
            i_this->m3DC = 0;
            i_this->m3D5 = 2;
        } else
#endif
        if ((player->getCutType() == 0xF && cM_rndF(1.0f) < 0.8f) ||
            (i_this->m3DB == 1 && cM_rndF(1.0f) < 0.05f && i_this->m3D8 == 2))
        {
            i_this->m3DC = 5;
            s16 ang = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
            ang += REG0_S(4);
            i_this->m3DE = ang + 0x7800;
        }
#if VERSION == VERSION_DEMO
        else if (player->current.pos.y - i_this->mAcch.GetGroundH() > 50.0f) {
            i_this->m3DC = 0;
        }
        anm_init(i_this, bougyo_d[i_this->m3DC], 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
#else
        anm_init(i_this, bougyo_d[i_this->m3DC], 4.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
    defence_jump_init:
#endif
        i_this->m3DA = 0;
        if (l_HIO.m46 != 0 && i_this->m3DC != 5 && i_this->m3D5 >= 3 && cM_rndF(1.0f) < 0.2f) {
            if (cM_rndF(1.0f) < 0.5f) {
                i_this->m3E0 = 65536.0f;
            } else {
                i_this->m3E0 = -65536.0f;
            }
            if (l_HIO.m12 == 1) {
                actor->speed.y = 20.0f + REG0_F(7);
                i_this->m2F8 = -20.0f + REG0_F(8);
                i_this->m3DA = REG0_S(2) + 10;
            } else {
                actor->speed.y = 15.0f + REG0_F(9);
                i_this->m2F8 = -5.0f + REG0_F(10);
            }
            i_this->m310 = 0.0f;
            splash_set(i_this);
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_DEFENCE_3, 0);
        } else {
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_DEFENCE_1, 0);
        }
        i_this->m2D0 = 10;
        break;
    case 10:
#if VERSION > VERSION_DEMO
        if (i_this->m3D8 == 2) {
            f32 dy = player->current.pos.y - i_this->mAcch.GetGroundH();
            if (dy > 300.0f + REG0_F(19) && i_this->m3DC != 0) {
                i_this->m3DC = 0;
                i_this->m3D5 = 2;
                anm_init(i_this, bougyo_d[i_this->m3DC], 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                goto defence_jump_init;
            }
        }
#endif
        actor->gravity = -4.0f + REG0_F(11);
        if (checkGround(i_this, 0.0f)) {
            i_this->m2F8 = 0.0f;
            if (l_HIO.m12 == 0) {
                if (dist < 400.0f + REG0_F(2) && i_this->m3DC != 5 && i_this->m302[1] == (s16)(REG0_S(5) + 1)) {
                    i_this->m2CE = 3;
                    if (cM_rndF(1.0f) < 0.5f) {
                        i_this->m2D0 = 0;
                    } else {
                        i_this->m2D0 = 1;
                    }
                    i_this->m13CE = 0;
                }
            } else if (i_this->m3DA != 0) {
                i_this->m3DA--;
                if (i_this->m3DA == 0) {
                    i_this->m2CE = 3;
                    if (cM_rndF(1.0f) < 0.5f) {
                        i_this->m2D0 = 0;
                    } else {
                        i_this->m2D0 = 1;
                    }
                    i_this->m13CE = 0;
                }
            }
        }
        if (i_this->m302[0] == 0 && i_this->m302[1] == 0 && checkGround(i_this, 0.0f) && i_this->m3DA == 0) {
            done = 1;
            break;
        }
        if (i_this->m3D6 != 0) {
            i_this->m3D6 = 0;
            s8 changed = 0;
            if (checkGround(i_this, 0.0f)) {
                if (i_this->m3D5 >= 8 && cM_rndF(1.0f) < 0.3f) {
                    i_this->m2D0 = 15;
                    i_this->m3D5 = 0;
                    changed = 1;
                } else if (i_this->m3D5 >= 4 && cM_rndF(1.0f) < 0.5f) {
                    i_this->m13D0 = REG0_S(6) + 0xD;
                    changed = 1;
                }
            }
            if (changed == 0) {
                i_this->m2D0 = 0;
                if (i_this->m3D5 <= 1) {
                    i_this->m3DC = 0;
                } else {
                    s32 i = 0;
                    do {
                        int rnd = cM_rndF(4.99f);
                        if ((s8)rnd != i_this->m3DC) {
                            i_this->m3DB = i_this->m3DC;
                            i_this->m3DC = rnd;
                            break;
                        }
                        i++;
                    } while (i < 20);
                }
            }
            i_this->m302[1] = REG0_S(4) + 0x19;
            if (i_this->m2D0 < 2) {
                fopAcM_monsSeStart(actor, JA_SE_CV_TN_GUARD, 0);
            } else {
                fopAcM_monsSeStart(actor, JA_SE_CV_TN_JUMP, 0);
            }
        }
        break;
    case 15:
        anm_init(i_this, dRes_INDEX_GND_BCK_BOUGYO3_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BOUGYO3_e);
        i_this->m2D0++;
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_DEFENCE_2, 0);
        actor->speed.y = 33.0f + REG0_F(5);
        f32 jump_speed = 30.0f + REG0_F(6);
        actor->speedF = jump_speed;
        i_this->m2F8 = jump_speed;
        actor->current.angle.y = cM_atan2s(-actor->current.pos.x, -actor->current.pos.z);
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->m302[1] = 100;
        } else {
            i_this->m302[1] = 0;
        }
        i_this->m13CE = 0;
        splash_set(i_this);
        i_this->m31C = 0;
        break;
    case 16:
        actor->gravity = -2.0f + REG0_F(7);
        if (checkGround(i_this, 0.0f)) {
            if (i_this->m31C == 0) {
                splash_set(i_this);
                i_this->m31C = 1;
            }
            i_this->m2F8 = 0.0f;
            if (i_this->mpMorf->isStop()) {
                if (i_this->m302[1] != 0) {
                    done = 1;
                } else {
                    done = 2;
                }
            }
        } else {
            i_this->m13CE = 0;
        }
        break;
    case 20:
        anm_init(i_this, dRes_INDEX_GND_BCK_SURETIGAI_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->m2D0++;
        break;
    case 21:
        if (i_this->mpMorf->isStop()) {
            done = 1;
        } else if (checkGround(i_this, 0.0f)) {
            i_this->m2F8 = 0.0f;
            anm_init(i_this, dRes_INDEX_GND_BCK_BOUGYO4_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
            i_this->m2D0++;
        }
        break;
    case 22:
        if (i_this->mpMorf->isStop()) {
            done = 1;
        }
        break;
    }

    if (i_this->m3DC != 5) {
        s16 target = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0));
        cLib_addCalcAngleS2(&actor->shape_angle.y, target, 2, (s16)(REG0_S(8) + 0x2000));
    } else {
        cLib_addCalcAngleS2(&actor->shape_angle.y, i_this->m3DE, 2, (s16)(REG0_S(8) + 0x2000));
    }
    i_this->m13B8[0] = 10;
    i_this->m13B8[1] = 11;
    pos_move(i_this, 1);
    if (done != 0) {
        actor->current.angle.y = actor->shape_angle.y;
        if (done == 1) {
            i_this->m2CE = 0;
            i_this->m2D0 = 0;
        } else {
            i_this->m2CE = 1;
            anm_init(i_this, dRes_INDEX_GND_BCK_BATTACK3_TAME_e, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f, dRes_INDEX_GND_BAS_BATTACK3_TAME_e);
            i_this->m2D0 = 8;
        }
    }
}

/* 000042D0-00004330       .text finish__FP9gnd_class */
static void finish(gnd_class* i_this) {
    i_this->mAction = 5;
    i_this->m13CE = 0;
    switch (i_this->m2D0) {
    case 0:
        i_this->m155E = 100;
        i_this->m2D0 = 1;
        break;
    case 1:
        i_this->m2D0 = 2;
        break;
    case 2:
        i_this->m2D0 = 3;
        break;
    case 3:
        break;
    }
}

/* 00004330-00004760       .text damage__FP9gnd_class */
static void damage(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
#if VERSION == VERSION_DEMO
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#endif

    i_this->m13CE = 0;
    if (i_this->m3D9 != 0) {
        if (i_this->m3D9 == 1) {
            i_this->m2CE = 4;
            i_this->m2D0 = 0;
            i_this->m13CE = 0;
        } else {
            i_this->m155E = 10;
        }
        i_this->m3D9 = 0;
        return;
    }

    switch (i_this->m2D0) {
    case 0:
        anm_init(i_this, dRes_INDEX_GND_BCK_DAMAGEA_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        i_this->m2D0 = 1;
        i_this->m302[0] = l_HIO.m86;
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_DAMAGE_3, 0);
        // fallthrough
    case 1:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_DAMAGEA_LOOP_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m2D0 = 9;
            i_this->m302[1] = 10;
        }
        break;
    case 5:
        anm_init(i_this, dRes_INDEX_GND_BCK_DAMAGEB_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_DAMAGE_2, 0);
        i_this->m302[0] = l_HIO.m86;
        i_this->m2D0 = 6;
        // fallthrough
    case 6:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, dRes_INDEX_GND_BCK_DAMAGEB_LOOP_e, 2.0f, J3DFrameCtrl::EMode_LOOP, 1.0f, -1);
            i_this->m2D0 = 10;
            i_this->m302[1] = 15;
        }
        break;
    case 9:
        if (i_this->m302[1] == 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_UMEKI, 0);
            i_this->m302[1] = 20;
        }
        goto stun_common;
    case 10:
        if (i_this->m302[1] == 0) {
            fopAcM_monsSeStart(actor, JA_SE_CV_GN_UMEKI, 0);
            i_this->m302[1] = 30;
        }
    stun_common:
#if VERSION == VERSION_DEMO
        if (actor->health < 30 && i_this->m3D8 == 2) {
#else
        if (i_this->m155E == 0 && i_this->m302[0] < 30 && actor->health < 0x19 && i_this->m3D8 == 2) {
#endif
            if (player_view_check(i_this, fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)))) {
                f32 dist = fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0));
                if (dist < l_HIO.m78) {
                    i_this->m2CE = 5;
                    i_this->m2D0 = 0;
                    i_this->m13CE = 1;
                    i_this->m30E = 30;
                    return;
                }
            }
        }
        if (i_this->m302[0] == 0) {
            i_this->m2CE = 0;
            i_this->m2D0 = 0;
            i_this->m13CE = 1;
            i_this->m30E = 30;
        }
        break;
    case 20:
        anm_init(i_this, dRes_INDEX_GND_BCK_DAMAGEC_e, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        fopAcM_monsSeStart(actor, JA_SE_CV_GN_DAMAGE_1, 0);
        i_this->m2D0 = 10;
        break;
    }

    pos_move(i_this, 1);
    i_this->m2F8 = 0.0f;
}

/* 00004760-0000501C       .text damage_check__FP9gnd_class */
static void damage_check(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    CcAtInfo atInfo;
    cXyz scale;
    cXyz scale2;

    i_this->mStts.Move();

    for (int i = 0; i < 2; i++) {
        if (i_this->mWeponSph[i].ChkTgHit()) {
            def_se_set(actor, i_this->mWeponSph[i].GetTgHitObj(), 0x40);
            i_this->m310 = 30.0f + REG0_F(19);
            i_this->m314 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
            i_this->m3D5++;
            i_this->m3D6 = 1;
            dKy_SordFlush_set(*i_this->mWeponSph[i].GetTgHitPosP(), 0);
            scale.setall(2.0f);
            dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, i_this->mWeponSph[i].GetTgHitPosP(), &actor->shape_angle, &scale);
            dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDHITNG00, i_this->mWeponSph[i].GetTgHitPosP(), &actor->shape_angle, NULL);
        }

        if (i_this->mWeponSph[i].ChkAtHit()) {
            if (fopAcM_GetName(i_this->mWeponSph[i].GetAtHitObj()->GetAc()) == fpcNm_PLAYER_e) {
                if (i_this->m2D0 == 0x16) {
                    i_this->m3D4 = 1;
                    dComIfGp_getVibration().StartShock(REG0_S(2) + 6, -0x21, cXyz(0.0f, 1.0f, 0.0f));
                } else if (player->checkPlayerGuard()) {
                    dComIfGp_getVibration().StartShock(REG0_S(2) + 3, -0x21, cXyz(0.0f, 1.0f, 0.0f));
                    if (i_this->m2D0 == 2 || i_this->m2D0 == 7) {
                        i_this->m3D4 = 1;
                    }
                }
            }
        }
    }

    if (i_this->mAction != 0) {
        return;
    }

    if (i_this->mChestSph.ChkTgHit()) {
        s8 ignore_arrow = 0;
        atInfo.mpObj = i_this->mChestSph.GetTgHitObj();
#if VERSION > VERSION_JPN
        if (atInfo.mpObj == NULL) {
            return;
        }
#endif

        fopAc_ac_c* hit_ac = atInfo.mpObj->GetAc();
        if (hit_ac != NULL && fopAcM_GetName(hit_ac) == fpcNm_ARROW_e) {
            daArrow_c* arrow = (daArrow_c*)hit_ac;
            if (arrow->mbSetByZelda) {
                s16 ang_diff = actor->shape_angle.y - arrow->current.angle.y;
                if (ang_diff < 0) {
                    ang_diff = -ang_diff;
                }
                if (((u16)ang_diff < 0x4000 && i_this->m3D8 < 2) || arrow->mbLinkReflect != 0) {
                    ignore_arrow = 1;
                } else {
                    i_this->m13CE = 2;
                }
            }
        }

        if (ignore_arrow == 0 && i_this->m13CE != 0) {
            i_this->m2CE = 10;
            if (i_this->m13CE == 1) {
                def_se_set(actor, i_this->mChestSph.GetTgHitObj(), 0x40);
                i_this->m310 = 30.0f + REG0_F(19);
                i_this->m314 = fopAcM_searchActorAngleY(actor, dComIfGp_getPlayer(0)) + 0x8000;
                dKy_SordFlush_set(*i_this->mChestSph.GetTgHitPosP(), 0);
                scale2.setall(2.0f);
                dComIfGp_particle_set(dPa_name::ID_AK_JN_NG, i_this->mChestSph.GetTgHitPosP(), &actor->shape_angle, &scale2);
                dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDHITNG00, i_this->mChestSph.GetTgHitPosP(), &actor->shape_angle, NULL);
                fopAcM_monsSeStart(actor, JA_SE_CV_GN_DEFENCE_1, 0);
                if (cM_rndF(1.0f) < 0.5f) {
                    anm_init(i_this, dRes_INDEX_GND_BCK_BOUGYO1_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                } else {
                    anm_init(i_this, dRes_INDEX_GND_BCK_BOUGYO2_e, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
                }
                i_this->m302[0] = 20;
                i_this->m2D0 = 10;
                return;
            }

            i_this->m2D0 = 15;
            i_this->m3D5 = 0;
            return;
        }
    }

    if (i_this->m30E != 0) {
        if (!i_this->mChestSph.ChkTgHit()) {
            return;
        }
    }

    s8 head_hit = 0;
    if (!i_this->mBodyCyl.ChkTgHit() && !i_this->mHeadSph.ChkTgHit() && !i_this->mChestSph.ChkTgHit()) {
        return;
    }

    i_this->mAction = 7;
    s8 light_arrow = 0;
    if (i_this->mHeadSph.ChkTgHit()) {
        atInfo.mpObj = i_this->mHeadSph.GetTgHitObj();
        atInfo.pParticlePos = i_this->mHeadSph.GetTgHitPosP();
        head_hit = 1;
    } else if (i_this->mChestSph.ChkTgHit()) {
        atInfo.mpObj = i_this->mChestSph.GetTgHitObj();
        atInfo.pParticlePos = i_this->mChestSph.GetTgHitPosP();
        light_arrow = 1;
    } else {
        atInfo.mpObj = i_this->mBodyCyl.GetTgHitObj();
        atInfo.pParticlePos = i_this->mBodyCyl.GetTgHitPosP();
    }

    atInfo.mpActor = cc_at_check(actor, &atInfo);
    if (light_arrow != 0 && atInfo.mpActor != NULL) {
        dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDLIGHTARROW00, atInfo.pParticlePos, &atInfo.mpActor->current.angle, NULL);
        dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDLIGHTARROW01, atInfo.pParticlePos, &atInfo.mpActor->current.angle, NULL);
#if VERSION > VERSION_DEMO
        i_this->m142E = 1;
        i_this->m142C = REG8_S(4) + 30;
        for (int j = 0; j < 15; j++) {
            i_this->m14A8[j].x = cM_rndFX(30.0f);
            i_this->m14A8[j].y = cM_rndFX(30.0f);
            i_this->m14A8[j].z = cM_rndFX(30.0f);
        }
#endif
#if VERSION > VERSION_DEMO
        i_this->m15AC = 20.0f + REG0_F(19);
#endif
    } else if (player->getCutType() == 5 || player->getCutType() == 0xF) {
        dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDASPECIALHIT00, atInfo.pParticlePos, &actor->shape_angle, NULL);
    } else {
        JPABaseEmitter* emitter = dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDHITOK00, atInfo.pParticlePos, &actor->shape_angle, NULL);
        if (emitter != NULL) {
            emitter->setGlobalPrmColor(i_this->mTevStr2.mColorC0.r, i_this->mTevStr2.mColorC0.g, i_this->mTevStr2.mColorC0.b);
        }
    }

#if VERSION == VERSION_DEMO
    if (actor->health <= 30) {
        if (l_HIO.m10 == 1) {
            actor->health = 30;
            l_HIO.m10 = 2;
            i_this->m3D9 = 2;
        }
    } else if (actor->health <= 60) {
        if (l_HIO.m10 == 0) {
            if (fopAcM_GetName(atInfo.mpActor) == fpcNm_ARROW_e) {
                JUTReport(440, 240, "YA HIT2 ");
                if (((daArrow_c*)atInfo.mpActor)->mbSetByZelda) {
                    i_this->m3D9 = 1;
                    JUTReport(440, 260, "YA HIT3 ");
                }
            } else {
                actor->health = 60;
            }
        }
    }
#else
    if (actor->health <= 0x19) {
        if (i_this->m3D8 == 1) {
            actor->health = 0x19;
            i_this->m3D8 = 2;
            i_this->m3D9 = 2;
        }
    } else if (actor->health <= 0x32) {
        if (i_this->m3D8 == 0) {
            if (fopAcM_GetName(atInfo.mpActor) == fpcNm_ARROW_e) {
                if (((daArrow_c*)atInfo.mpActor)->mbSetByZelda) {
                    i_this->m3D9 = 1;
                }
            } else {
                actor->health = 0x32;
            }
        }
    }

    if (actor->health < 0) {
        actor->health = 0;
    }
#endif

    if (i_this->m2CE != 11) {
        i_this->m2CE = 11;
        if (head_hit != 0) {
            i_this->m2D0 = 5;
        } else {
            i_this->m2D0 = 0;
        }
        i_this->m13CE = 0;
    } else {
        i_this->m2D0 = 20;
    }

    fopAcM_monsSeStart(actor, JA_SE_CV_MO_DAMAGE, 0);
}

/* 0000501C-0000509C       .text shot_s_sub__FPvPv */
static void* shot_s_sub(void* param_1, void*) {
    if ((fopAcM_IsActor(param_1) && fopAcM_GetName(param_1) == fpcNm_HIMO2_e) ||
        fopAcM_GetName(param_1) == fpcNm_BOOMERANG_e ||
        fopAcM_GetName(param_1) == fpcNm_HOOKSHOT_e ||
        fopAcM_GetName(param_1) == fpcNm_ARROW_e)
    {
        if (((fopAc_ac_c*)param_1)->speedF >= 10.0f) {
            return param_1;
        }
    }
    return NULL;
}

/* 0000509C-000050B0       .text demowait__FP9gnd_class */
static void demowait(gnd_class* i_this) {
    i_this->mAction = 5;
    i_this->m13CE = 0;
}

#if VERSION > VERSION_DEMO
/* 000050B0-000050C8       .text yawait__FP9gnd_class */
static void yawait(gnd_class* i_this) {
    i_this->current.pos.y = 0.0f;
    i_this->m13CE = 0;
}
#endif

/* 000050C8-000054F0       .text gnd_move__FP9gnd_class */
static void gnd_move(gnd_class* i_this) {
    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    cXyz vec;
    cXyz out;

    switch (i_this->m2CE) {
    case 0:
        move0(i_this);
        break;
    case 1:
        attack0(i_this);
        break;
    case 2:
        attack1(i_this);
        break;
    case 3:
        attack2(i_this);
        break;
    case 4:
        attackPZ(i_this);
        break;
    case 5:
        attack_last(i_this);
        break;
    case 10:
        defence0(i_this);
        break;
    case 11:
        damage(i_this);
        break;
    case 20:
        demowait(i_this);
        break;
#if VERSION > VERSION_DEMO
    case 21:
        yawait(i_this);
        break;
#endif
    case 30:
        finish(i_this);
        return;
    }

    if (i_this->m3D8 >= 2 && i_this->m2CE != 11) {
        i_this->m30E = 3;
    }

    if (player->getCutType() == 5 || player->getCutType() == 0xF) {
        if (i_this->m3D8 < 2) {
            i_this->m13CE = 0;
            i_this->m3D7 = 0;
        }
    }

    if (i_this->m13CE) {
        s8 found_near = 0;
        fopAc_ac_c* shot = (fopAc_ac_c*)fpcEx_Search(shot_s_sub, i_this);
        if (shot != NULL) {
            vec = shot->current.pos - actor->eyePos;
            if (fopAcM_GetName(shot) == fpcNm_ARROW_e) {
                f32 dist = vec.abs();
                if (dist < 10.0f * shot->speedF) {
                    if (((daArrow_c*)shot)->mbSetByZelda && i_this->m3D8 < 2) {
                        s16 ang_diff = actor->shape_angle.y - shot->current.angle.y;
                        if (ang_diff < 0) {
                            ang_diff = -ang_diff;
                        }
                        if ((u16)ang_diff < 0x4000) {
                            i_this->m13CE = 0;
                        } else {
                            i_this->m2CE = 10;
                            if (checkGround(i_this, 50.0f)) {
                                i_this->m2D0 = 15;
                            } else {
                                i_this->m2D0 = 0;
                            }
                            i_this->mAction = 30;
                            i_this->m3DB = 0;
                        }
                    }
                }
            } else {
                f32 dist = vec.abs();
                if (dist < 10.0f * shot->speedF) {
                    found_near = 1;
                }
            }
        }

        f32 player_dist = fopAcM_searchActorDistance(actor, dComIfGp_getPlayer(0));
        if (found_near || (player->getCutType() != 0 && player_dist < l_HIO.m40)) {
            if (i_this->m2CE != 10) {
                i_this->m2CE = 10;
                i_this->m2D0 = 0;
                i_this->m3D5 = 0;
                i_this->m3DB = 0;
                i_this->m3DC = 0;
            }
            i_this->m302[0] = DEMO_SELECT(REG0_S(3) + 10, 10);
        }
    }

    damage_check(i_this);
    i_this->m13CE = 1;

    cXyz* cc_move = i_this->mStts.GetCCMoveP();
    if (cc_move != NULL) {
        actor->current.pos.x += cc_move->x;
        actor->current.pos.y += cc_move->y;
        actor->current.pos.z += cc_move->z;
    }

    if (i_this->m310 > 0.01f) {
        vec.x = 0.0f;
        vec.y = 0.0f;
        vec.z = i_this->m310;
        cMtx_YrotS(*calc_mtx, i_this->m314);
        MtxPosition(&vec, &out);
        actor->current.pos += out;
        cLib_addCalc0(&i_this->m310, 1.0f, 7.0f + REG0_F(12));
    }
}

static f32 f_fovy[] = {
    66.75f, 64.25f, 64.25f, 44.25f, 44.25f,
};
#if VERSION > VERSION_DEMO
static u32 eff_d[] = {
    0x2F, 0x3C, 0x3D, 0x3E, 0x09, 0x0B, 0x0E, 0x11, 0x0D, 0x10, 0x1D, 0x20, 0x23, 0x1F, 0x22,
};
static u32 fl_check_d[] = {
    0x0A, 0x02, 0x02, 0x05, 0x05, 0x0A, 0x0F, 0x14, 0x0D, 0x12, 0x0A, 0x0F, 0x14, 0x0D, 0x12,
};
static f32 fl_scale[] = {
    1.0f, 2.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f, 0.8f, 0.8f, 0.8f, 1.0f, 1.0f, 0.8f, 0.8f, 0.8f,
};
#endif

static cXyz f_ctr[] = {
    cXyz(-34.5f, 129.7f, -26.3f),
    cXyz(-37.9f, 202.6f, -77.5f),
    cXyz(-11.5f, 446.0f, -235.0f),
    cXyz(7.3f, 293.0f, 55.9f),
    cXyz(-7.3f, -51.6f, 116.5f),
};
static cXyz f_eye[] = {
    cXyz(101.9f, 38.0f, 550.0f),
    cXyz(-10.0f, 7.0f, -644.0f),
    cXyz(-10.0f, 7.0f, -644.0f),
    cXyz(83.6f, 870.4f, -88.0f),
    cXyz(89.0f, 519.0f, -43.3f),
};

/* 000054F0-00006B04       .text demo_camera__FP9gnd_class */
static void demo_camera(gnd_class* i_this) {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    daPz_c* zelda = (daPz_c*)pz;
    cXyz offset;
    cXyz sp;
    cXyz zero;
    s8 end_demo = 0;
    f32 bank_extra;
    zero.setall(0.0f);
    bank_extra = 0.0f;

    switch (i_this->m155E) {
    case 0:
        break;
    case 1:
        if (!i_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(i_this, dEvtCnd_UNK2_e, 0xFFFF, 0);
            i_this->eventInfo.onCondition(dEvtCnd_UNK2_e);
            return;
        }
        i_this->m155E++;
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(2);
        i_this->m1560 = 0;
        i_this->m15A4 = 55.0f;
        i_this->m15A0 = 0.0f;
        player->changeOriginalDemo();
#if VERSION > VERSION_DEMO
        i_this->m15AC = 0.0f;
#endif
        i_this->m15A4 = 55.0f + REG0_F(6);
        i_this->m157C.x = 400.0f + REG0_F(0);
        i_this->m157C.y = 100.0f + REG0_F(1);
        i_this->m157C.z = REG0_F(2) - 200.0f;
        i_this->m1588.x = REG0_F(3);
        i_this->m1588.y = 200.0f + REG0_F(4);
        i_this->m1588.z = REG0_F(5);
        // fallthrough
    case 2: {
        s16 ang = cM_atan2s(i_this->current.pos.x - pz->current.pos.x, i_this->current.pos.z - pz->current.pos.z);
        i_this->shape_angle.y = ang + 0x4000 + REG0_S(0);
        i_this->current.angle.y = i_this->shape_angle.y;
        if (i_this->m1560 > REG0_S(1) + 0x19) {
#if VERSION > VERSION_DEMO
            i_this->m155C = 0x82;
#endif
            cLib_addCalc2(&i_this->m157C.x, 130.0f + REG0_F(6), 0.6f, 270.0f * i_this->m15A0);
            cLib_addCalc2(&i_this->m157C.y, 230.0f + REG0_F(7), 0.6f, 80.0f * i_this->m15A0);
            cLib_addCalc2(&i_this->m157C.z, REG0_F(8), 0.6f, 100.0f * i_this->m15A0);
            cLib_addCalc2(&i_this->m1588.y, 280.0f + REG0_F(9), 0.6f, 80.0f * i_this->m15A0);
            cLib_addCalc2(&i_this->m15A0, 1.0f, 1.0f, 0.02f * (1.0f + REG0_F(7)));
        } else {
            i_this->m157C.x = 400.0f + REG0_F(0);
            i_this->m157C.y = 150.0f + REG0_F(1);
            i_this->m157C.z = REG0_F(2);
            i_this->m1588.x = REG0_F(3);
            i_this->m1588.y = 200.0f + REG0_F(4);
            i_this->m1588.z = REG0_F(5);
        }
        MtxTrans(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z, 0);
        DEMO_SELECT(cMtx_YrotM, mDoMtx_YrotM)(*calc_mtx, i_this->shape_angle.y);
        MtxPosition(&i_this->m157C, &i_this->m1564);
        MtxPosition(&i_this->m1588, &i_this->m1570);
        if (i_this->m1560 > REG0_S(2) + 0x3C) {
            i_this->m155E++;
            i_this->m1560 = 0;
#if VERSION > VERSION_DEMO
            i_this->m155C = 0x32;
#endif
        }
        break;
    }
    case 3:
        if (i_this->m1560 == DEMO_SELECT(REG0_S(3) + 10, 10)) {
            i_this->m2D0++;
        }
        MtxTrans(pz->current.pos.x, pz->current.pos.y, pz->current.pos.z, 0);
        DEMO_SELECT(cMtx_YrotM, mDoMtx_YrotM)(*calc_mtx, pz->shape_angle.y);
        i_this->m157C.x = 100.0f + REG0_F(10);
        i_this->m157C.y = 50.0f + REG0_F(11);
        i_this->m157C.z = REG0_F(12) - 300.0f;
        MtxPosition(&i_this->m157C, &i_this->m1564);
        i_this->m1570 = i_this->current.pos;
        i_this->m1570.y += 150.0f + REG0_F(13);
        {
            s16 pang = fopAcM_searchActorAngleY(i_this, dComIfGp_getPlayer(0));
            player->setPlayerPosAndAngle(&player->current.pos, pang + 0x8000);
        }
        break;
    case 4:
        cLib_addCalc2(&i_this->m1570.y, 100.0f, 0.8f, 10.0f);
        if (i_this->m1560 < 15) {
            break;
        }
        i_this->m155E = 5;
        i_this->m1560 = 0;
        i_this->m15A4 = 75.0f;
        // fallthrough
    case 5:
        cMtx_YrotS(*calc_mtx, player->shape_angle.y);
#if VERSION == VERSION_DEMO
        offset.set(-50.0f + REG0_F(4), 50.0f + REG0_F(5), 100.0f + REG0_F(6));
#else
        offset.x = -50.0f + REG0_F(4);
        offset.y = 50.0f + REG0_F(5);
        offset.z = 100.0f + REG0_F(6);
#endif
        MtxPosition(&offset, &sp);
        i_this->m1564 = player->current.pos + sp;
        i_this->m1570 = player->current.pos;
        i_this->m1570.y += 90.0f + REG0_F(7);
        cLib_addCalc2(&i_this->m15A4, 55.0f, 0.8f, 10.0f);
#if VERSION > VERSION_DEMO
        i_this->m155C = 0x32;
#endif
        if (l_HIO.m88 == 0) {
            if (i_this->m1560 == 5) {
                player->changeDemoMode(daPy_demo_c::DEMO_S_SURP_e);
            }
            if (i_this->m1560 == 0x19) {
                end_demo = 1;
            }
        } else {
            if (i_this->m1560 == 5) {
                player->changeDemoMode(daPy_demo_c::DEMO_SMILE_e);
            }
            if (i_this->m1560 == 0x3C) {
                player->changeDemoMode(daPy_demo_c::DEMO_UNK_029_e);
            }
            if (i_this->m1560 == 0x96) {
                end_demo = 1;
            }
        }
        if (end_demo != 0) {
            i_this->mpBtkAnm->init(
                i_this->mpMorf->getModel()->getModelData(),
                (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BTK_SAMPLE_e),
                TRUE,
                J3DFrameCtrl::EMode_NONE,
                1.0f,
                0,
                -1,
                true
            );
            i_this->m2CE = 0;
            i_this->m2D0 = 0;
            i_this->current.angle.y = i_this->shape_angle.y;
            i_this->m302[1] = l_HIO.m68;
            i_this->m155E = 0x96;
#if VERSION == VERSION_DEMO
            if (l_HIO.m10 == 0) {
                l_HIO.m10 = 1;
            }
#else
            i_this->m3D8 = 1;
#endif
        }
        break;
    case 10:
        if (i_this->m2CE != 0 || !checkGround(i_this, 0.0f)) {
            return;
        }
        if (!i_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(i_this, dEvtCnd_UNK2_e, 0xFFFF, 0);
            i_this->eventInfo.onCondition(dEvtCnd_UNK2_e);
            return;
        }
        i_this->m155E++;
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(2);
        i_this->m1560 = 0;
        i_this->m15A4 = 55.0f;
        i_this->m15A0 = 0.0f;
        player->changeOriginalDemo();
        cMtx_YrotS(*calc_mtx, pz->shape_angle.y);
#if VERSION == VERSION_DEMO
        offset.set(150.0f + REG0_F(4), 50.0f + REG0_F(5), 150.0f + REG0_F(6));
#else
        offset.x = 150.0f + REG0_F(4);
        offset.y = 50.0f + REG0_F(5);
        offset.z = 150.0f + REG0_F(6);
#endif
        MtxPosition(&offset, &sp);
        i_this->m1564 = pz->current.pos + sp;
        i_this->m1570 = pz->eyePos;
        i_this->m1570.y += REG0_F(7) - 30.0f;
        zelda->mTalkState = 2;
        i_this->m2CE = 0x14;
#if VERSION > VERSION_DEMO
        i_this->m15AC = 0.0f;
#endif
        // fallthrough
    case 11: {
        cLib_addCalc2(&i_this->m1570.x, pz->eyePos.x, 0.1f, 20.0f);
        sp.y = pz->eyePos.y + REG0_F(7) - 30.0f;
        if (sp.y < 40.0f) {
            sp.y = 40.0f;
        }
        cLib_addCalc2(&i_this->m1570.y, sp.y, 0.1f, 20.0f);
        cLib_addCalc2(&i_this->m1570.z, pz->eyePos.z, 0.1f, 20.0f);
        if (i_this->m1560 >= REG0_S(0) + 0x64) {
            i_this->m2CE = 0;
            i_this->m2D0 = 0;
            i_this->current.angle.y = i_this->shape_angle.y;
            i_this->m302[1] = l_HIO.m68;
            i_this->m155E = 0x96;
            zelda->m0740 = 1;
#if VERSION == VERSION_DEMO
            l_HIO.m10 = 2;
#else
            i_this->m3D8 = 2;
#endif
        }
        break;
    }
#if VERSION > VERSION_DEMO
    case 0x14:
        if (!i_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(i_this, dEvtCnd_CANTALK_e, 0xFFFF, 0);
            i_this->eventInfo.onCondition(dEvtCnd_UNK2_e);
            return;
        }
        i_this->m155E++;
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(2);
        i_this->m1560 = 0;
        i_this->m15A4 = 55.0f;
        i_this->m15A0 = 0.0f;
        player->changeOriginalDemo();
        dComIfGp_getVibration().StartShock(REG0_S(2) + 8, -0x21, cXyz(0.0f, 1.0f, 0.0f));
#if VERSION > VERSION_DEMO
        i_this->m15AC = 5.0f + REG0_F(18);
#endif
#if VERSION > VERSION_DEMO
        i_this->m155C = 0x64;
#endif
        // fallthrough
    case 0x15:
        i_this->m1570 = player->current.pos;
        i_this->m1570.y += 50.0f + REG8_F(10);
        cMtx_YrotS(*calc_mtx, player->shape_angle.y);
        offset.x = 200.0f + REG8_F(1);
        offset.y = 30.0f + REG8_F(2);
        offset.z = 300.0f + REG8_F(3);
        MtxPosition(&offset, &sp);
        i_this->m1564 = sp + player->current.pos;
        if (i_this->m1560 > (s16)(REG12_S(0) + 0xF)) {
            i_this->m155E++;
            i_this->m1560 = 0;
        } else {
            bank_extra = REG8_F(13);
            break;
        }
        // fallthrough
    case 0x16:
        cLib_addCalc2(&i_this->m1570.x, i_this->current.pos.x, 0.5f, 70.0f + REG8_F(6));
        cLib_addCalc2(&i_this->m1570.y, i_this->eyePos.y + REG8_F(4), 0.1f, 50.0f);
        cLib_addCalc2(&i_this->m1570.z, i_this->current.pos.z + REG8_F(5), 0.5f, 70.0f + REG8_F(6));
        cMtx_YrotS(*calc_mtx, i_this->shape_angle.y);
        offset.x = -300.0f + REG8_F(7);
        offset.y = 100.0f + REG8_F(8);
        offset.z = 500.0f + REG8_F(9);
        MtxPosition(&offset, &sp);
        i_this->m1564 = sp + i_this->current.pos;
        if (i_this->m1560 > 0x1E) {
            i_this->m1560 = 0;
            if (i_this->m2CE != 0xB) {
                i_this->m2CE = 0;
                i_this->m2D0 = 0;
                i_this->current.angle.y = i_this->shape_angle.y;
                i_this->m302[1] = l_HIO.m68;
            }
            i_this->m155E = 0x96;
        }
        break;
#endif
    case 0x64:
        if (!i_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(i_this, dEvtCnd_UNK2_e, 0xFFFF, 0);
            i_this->eventInfo.onCondition(dEvtCnd_UNK2_e);
            return;
        }
        i_this->m155E++;
        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(2);
        // fallthrough
    case 0x65:
        i_this->m15A4 = 55.0f;
        i_this->m15A0 = 0.0f;
        i_this->m1560 = 0;
        player->changeOriginalDemo();
        i_this->mBodyCyl.OffCoSetBit();
        anm_init(i_this, dRes_INDEX_GND_BCK_COMBO_GANON_e, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f, -1);
        player->changeDemoMode(daPy_demo_c::DEMO_UNK_070_e);
        zelda->m073F = 1;
        i_this->m155E++;
        player->setPlayerPosAndAngle(&zero, (s16)0);
        i_this->current.pos = zero;
        i_this->shape_angle.y = 0;
        i_this->current.angle.y = 0;
        pz->current.pos = zero;
        pz->current.pos.x += 400.0f;
        i_this->mp13F0[0] = dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDFINISHOFF00, &i_this->current.pos);
        i_this->mp13F0[1] = dComIfGp_particle_set(dPa_name::ID_AK_SN_GNDFINISHOFF01, &i_this->current.pos);
        i_this->m1562 = 0;
        // fallthrough
    case 0x66: {
        for (int i = 0; i < 2; i++) {
            if (i_this->mp13F0[i] != NULL) {
                if (i_this->mpMorf->isStop()) {
                    i_this->mp13F0[i]->becomeInvalidEmitter();
                    i_this->mp13F0[i] = NULL;
                } else {
                    i_this->mp13F0[i]->setGlobalRTMatrix(i_this->mpMorf->getModel()->getAnmMtx(GND_JNT_GND_DEKO_1_e));
                }
            }
        }
        if (i_this->m1560 == 0x22) {
            fopAcM_monsSeStart(i_this, JA_SE_CV_GN_LAST_HIT_1, 0);
        }
        if (i_this->m1560 == 0x4A) {
            fopAcM_monsSeStart(i_this, JA_SE_CV_GN_LAST_HIT_2, 0);
        }
        if (i_this->m1560 == 0x1E) {
            mDoAud_seStart(JA_SE_LK_SW_CRT_HIT, &i_this->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(i_this)));
            dComIfGp_getVibration().StartShock(REG0_S(2) + 4, -0x21, cXyz(0.0f, 1.0f, 0.0f));
        }
        if (i_this->m1560 == DEMO_SELECT(REG0_S(3) + 0x13, 0x13) || i_this->m1560 == REG0_S(4) + 0x27 ||
            i_this->m1560 == REG0_S(5) + 0x43 || i_this->m1560 == REG0_S(6) + 0x45)
        {
            i_this->m1562++;
            if (i_this->m1562 == 2 || i_this->m1562 == 4) {
                i_this->m1588.x = std::fabsf(f_ctr[i_this->m1562].x - f_ctr[i_this->m1562 - 1].x);
                i_this->m1588.y = std::fabsf(f_ctr[i_this->m1562].y - f_ctr[i_this->m1562 - 1].y);
                i_this->m1588.z = std::fabsf(f_ctr[i_this->m1562].z - f_ctr[i_this->m1562 - 1].z);
                i_this->m157C.x = std::fabsf(f_eye[i_this->m1562].x - f_eye[i_this->m1562 - 1].x);
                i_this->m157C.y = std::fabsf(f_eye[i_this->m1562].y - f_eye[i_this->m1562 - 1].y);
                i_this->m157C.z = std::fabsf(f_eye[i_this->m1562].z - f_eye[i_this->m1562 - 1].z);
                i_this->m15A8 = std::fabsf(f_fovy[i_this->m1562] - f_fovy[i_this->m1562 - 1]);
            }
        }
        if (i_this->m1562 == 0 || i_this->m1562 == 1 || i_this->m1562 == 3) {
            i_this->m1570 = f_ctr[i_this->m1562];
            i_this->m1564 = f_eye[i_this->m1562];
            i_this->m15A4 = f_fovy[i_this->m1562];
        } else {
            i_this->m15A0 = 0.1f + REG0_F(0);
            f32 rate = 0.4f + REG0_F(1);
            cLib_addCalc2(&i_this->m1570.x, f_ctr[i_this->m1562].x, rate, i_this->m1588.x * i_this->m15A0);
            cLib_addCalc2(&i_this->m1570.y, f_ctr[i_this->m1562].y, rate, i_this->m1588.y * i_this->m15A0);
            cLib_addCalc2(&i_this->m1570.z, f_ctr[i_this->m1562].z, rate, i_this->m1588.z * i_this->m15A0);
            cLib_addCalc2(&i_this->m1564.x, f_eye[i_this->m1562].x, rate, i_this->m157C.x * i_this->m15A0);
            cLib_addCalc2(&i_this->m1564.y, f_eye[i_this->m1562].y, rate, i_this->m157C.y * i_this->m15A0);
            cLib_addCalc2(&i_this->m1564.z, f_eye[i_this->m1562].z, rate, i_this->m157C.z * i_this->m15A0);
            cLib_addCalc2(&i_this->m15A4, f_fovy[i_this->m1562], rate, i_this->m15A8 * i_this->m15A0);
        }
        if (i_this->m1560 == 0x4A) {
            mDoAud_seStart(JA_SE_CM_GN_SW_SPIKE_HEAD, &i_this->eyePos, 0, dComIfGp_getReverb(fopAcM_GetRoomNo(i_this)));
            mDoAud_bgmStop(10);
            dComIfGp_getVibration().StartShock(REG0_S(2) + 8, -0x21, cXyz(0.0f, 1.0f, 0.0f));
        }
        if (i_this->m1560 == 0x4A) {
            mDoGph_gInf_c::startFadeOut((s8)(REG0_S(8) + 0x32));
            mDoGph_gInf_c::setFadeColor((JUtility::TColor&)g_saftyWhiteColor);
        }
        if (i_this->m1560 == 0x7C) {
            dComIfGp_setNextStage("GTower", 2, 0, 9, 0.0f, 0, 1, 7);
        }
#if VERSION == VERSION_DEMO
        if (CPad_CHECK_TRIG_DOWN(0)) {
            player->cancelOriginalDemo();
            i_this->m155E--;
        }
#endif
        break;
    }
    case 0x96:
        camera->mCamera.SetTrimSize(0);
        camera->mCamera.Start();
        dMeter_mtrShow();
        dComIfGp_event_onEventFlag(8);
        i_this->m155E = 0;
        player->cancelOriginalDemo();
#if VERSION > VERSION_DEMO
        i_this->m155C = 1;
#endif
        break;
    }

    if (i_this->m155E != 0) {
#if VERSION == VERSION_DEMO
        camera->mCamera.Set(i_this->m1570, i_this->m1564, i_this->m15A4, 0);
#else
        f32 shake_x = i_this->m15AC * cM_ssin(i_this->m1560 * 0x3500);
        f32 shake_y = i_this->m15AC * cM_scos(i_this->m1560 * 0x3900);
        cXyz eye;
        eye.x = i_this->m1564.x + shake_x;
        eye.y = i_this->m1564.y + shake_y;
        eye.z = i_this->m1564.z;
        cXyz center;
        center.x = i_this->m1570.x + shake_x;
        center.y = i_this->m1570.y + shake_y;
        center.z = i_this->m1570.z;
        s16 bank = (s16)(bank_extra + 7.5f * (i_this->m15AC * cM_scos(i_this->m2CC * 0x1C00)));
        camera->mCamera.Set(center, eye, bank, i_this->m15A4);
        cLib_addCalc0(&i_this->m15AC, 1.0f, 1.0f + REG0_F(16));
#endif
        JUTReport(0x19A, 0x1AE, "K SUB  COUNT  %d", i_this->m1560);
        i_this->m1560++;
    }
}

#if VERSION > VERSION_DEMO
/* 00006B04-00006D0C       .text body_flash__FP9gnd_class */
static void body_flash(gnd_class* i_this) {
    J3DModel* model = i_this->mpMorf->getModel();

    if (i_this->m142E != 0) {
        i_this->m142E++;
        if (i_this->m142E > 100) {
            i_this->m142E = 0;
        }
    }

    for (int i = 0; i < 15; i++) {
        if (i_this->m142E == (int)fl_check_d[i]) {
            i_this->mFlashTimer[i] = 100;
        }

        if (i_this->mFlashTimer[i] != 0) {
            i_this->mFlashTimer[i]--;
            MTXCopy(model->getAnmMtx(eff_d[i]), *calc_mtx);
            cXyz pos;
            MtxPosition(&i_this->m14A8[i], &pos);
            f32 scale = (0.01f + REG8_F(0)) * (i_this->mFlashTimer[i] * fl_scale[i]);
            if (i_this->mpFlashEff[i] == NULL) {
                i_this->mpFlashEff[i] = dComIfGp_particle_set(dPa_name::ID_AK_JN_CCTHUNDER00, &pos);
            } else {
                i_this->mpFlashEff[i]->setGlobalTranslation(pos.x, pos.y, pos.z);
                JGeometry::TVec3<f32> gscale(scale, scale, scale);
                i_this->mpFlashEff[i]->setGlobalScale(gscale);
            }
        } else if (i_this->mpFlashEff[i] != NULL) {
            i_this->mpFlashEff[i]->becomeInvalidEmitter();
            i_this->mpFlashEff[i] = NULL;
        }
    }
}
#endif

/* 00006D0C-000076B8       .text daGnd_Execute__FP9gnd_class */
static BOOL daGnd_Execute(gnd_class* i_this) {
    static u32 w_d[] = {0x1B, 0x2D};
    static u32 w_asi[] = {0x04, 0x07};
    static int at_spl_d[] = {0, 1, 2, 5, 6, 7, 10};

    fopAc_ac_c* actor = i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    cXyz offset;
    cXyz pos;

    if (i_this->m2B4 == 0) {
        i_this->m2CE = 20;
    }

    if (j_demo(i_this)) {
        i_this->mpMorf->calc();
        i_this->mpBrkAnm->play();
        i_this->mpBtkAnm->play();
        i_this->mpBtpAnm->play();

        J3DModel* model = i_this->mpMorf->getModel();
        model->setBaseScale(actor->scale);
        mDoMtx_stack_c::transS(actor->current.pos);
        mDoMtx_stack_c::ZXYrotM(actor->shape_angle);
        model->setBaseTRMtx(mDoMtx_stack_c::now);
        ke_move(i_this);
        return TRUE;
    }

#if VERSION > VERSION_DEMO
    setRipple(i_this);
#endif

    if (pz == NULL) {
        pz = (fopAc_ac_c*)fpcEx_Search(z_s_sub, i_this);
    }

#if VERSION == VERSION_DEMO
    i_this->m3D8 = l_HIO.m10;
#else
    if (l_HIO.m10 != 0) {
        i_this->m3D8 = l_HIO.m10 - 1;
    }
#endif

    if (actor->stealItemBitNo == 0x23) {
        actor->stealItemBitNo++;
        mDoAud_changeBgmStatus(2);
    }

#if VERSION > VERSION_DEMO
    if ((u8)fopAcM_GetParam(actor) == 0x23) {
        i_this->m2CE = 0x15;
        i_this->m155E = 0x14;
        fopAcM_SetParam(actor, 0);
    }
#endif

    i_this->m2CC++;

    for (int i = 0; i < 5; i++) {
        if (i_this->m302[i] != 0) {
            i_this->m302[i]--;
        }
    }
    if (i_this->mAction != 0) {
        i_this->mAction--;
    }
    if (i_this->m30E != 0) {
        i_this->m30E--;
    }
    if (i_this->m318 != 0) {
        i_this->m318--;
    }
#if VERSION > VERSION_DEMO
    if (i_this->m142C != 0) {
        i_this->m142C--;
    }
#endif

    if (l_HIO.m05 == 0) {
        gnd_move(i_this);
        i_this->mAcch.CrrPos(*dComIfG_Bgsp());
        g_env_light.settingTevStruct(2, &actor->current.pos, &i_this->mTevStr2);
        i_this->mpMorf->play(&actor->eyePos, 0, 0);
        if (i_this->mpMorf->isStop()) {
            attack_eff_remove(i_this);
        }
        i_this->mpBrkAnm->play();
        i_this->mpBtkAnm->play();
        i_this->mpBtpAnm->play();
    }

    J3DModel* model = i_this->mpMorf->getModel();
    f32 scale = l_HIO.m0C;
    actor->scale.z = scale;
    actor->scale.y = scale;
    actor->scale.x = scale;
    model->setBaseScale(actor->scale);

    mDoMtx_stack_c::transS(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z);
    cLib_addCalc0(&i_this->m3E0, 0.5f + REG0_F(17), 6053.5f + REG0_F(18));
    mDoMtx_stack_c::YrotM(actor->shape_angle.y + (s16)i_this->m3E0);
    mDoMtx_stack_c::XrotM(actor->shape_angle.x);
    mDoMtx_stack_c::ZrotM(actor->shape_angle.z);
    model->setBaseTRMtx(mDoMtx_stack_c::now);
    i_this->mpMorf->calc();

    offset.setall(0.0f);
    MTXCopy(model->getAnmMtx(0x30), *calc_mtx);
    MtxPosition(&offset, &actor->eyePos);

    actor->attention_info.position = actor->current.pos;
    actor->attention_info.position.y += 290.0f;

    i_this->mBodyCyl.SetC(actor->current.pos);
    i_this->mBodyCyl.SetR(80.0f);
    i_this->mBodyCyl.SetH(220.0f);
    dComIfG_Ccsp()->Set(&i_this->mBodyCyl);

    i_this->mHeadSph.SetC(actor->eyePos);
    i_this->mHeadSph.SetR((70.0f + REG0_F(11)) * l_HIO.m0C);
    dComIfG_Ccsp()->Set(&i_this->mHeadSph);

    MTXCopy(model->getAnmMtx(9), *calc_mtx);
    MtxPosition(&offset, &pos);
    pos.y -= 20.0f + REG0_F(10);
    i_this->mChestSph.SetC(pos);
    i_this->mChestSph.SetR((170.0f + REG0_F(11)) * l_HIO.m0C);
    dComIfG_Ccsp()->Set(&i_this->mChestSph);

    if (i_this->m3D7 != 0) {
        i_this->m3D7--;
        i_this->mBodyCyl.OffTgSetBit();
        i_this->mHeadSph.OffTgSetBit();
    } else {
        i_this->mBodyCyl.OnTgSetBit();
        i_this->mHeadSph.OnTgSetBit();
    }

    for (int i = 0; i < 2; i++) {
        MTXCopy(model->getAnmMtx(w_asi[i]), *calc_mtx);
        offset.setall(0.0f);
        MtxPosition(&offset, &i_this->m15C8[i]);

        if (i_this->m13B8[i] != 0 &&
            (player->getCutType() != daPy_py_c::CUT_TYPE_BT_JUMPCUT || i_this->m3D8 >= 2))
        {
            i_this->mWeponSph[i].SetAtHitMark(dCcG_AtHitMark_Nrm_e);

            if (i_this->m13B8[i] == 3) {
                MtxTrans(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z, 0);
#if VERSION == VERSION_DEMO
                cMtx_YrotM(*calc_mtx, actor->shape_angle.y);
                offset.set(REG0_F(14), 100.0f + REG0_F(15), 300.0f + REG0_F(16));
#else
                mDoMtx_YrotM(*calc_mtx, actor->shape_angle.y);
                offset.x = REG0_F(14);
                offset.y = 100.0f + REG0_F(15);
                offset.z = 300.0f + REG0_F(16);
#endif
                i_this->m13BC[i] = at_spl_d[l_HIO.m84];
            } else if (i_this->m13B8[i] >= 10) {
                MtxTrans(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z, 0);
#if VERSION == VERSION_DEMO
                if (i_this->m13B8[i] == 10) {
                    offset.set(0.0f, 60.0f + REG0_F(15), 0.0f);
                } else {
                    offset.set(0.0f, 230.0f + REG0_F(16), 0.0f);
                }
#else
                if (i_this->m13B8[i] == 10) {
                    offset.x = 0.0f;
                    offset.y = 60.0f + REG0_F(15);
                    offset.z = 0.0f;
                } else {
                    offset.x = 0.0f;
                    offset.y = 230.0f + REG0_F(16);
                    offset.z = 0.0f;
                }
#endif
                i_this->m13C4[i] = 100.0f + REG0_F(17);
                if (l_HIO.m45 != 0) {
                    i_this->mWeponSph[i].OffTgShield();
                    i_this->mWeponSph[i].OnTgNoHitMark();
                } else {
                    i_this->mWeponSph[i].OnTgShield();
                    i_this->mWeponSph[i].OffTgNoHitMark();
                }
            } else {
                MTXCopy(model->getAnmMtx(w_d[i]), *calc_mtx);
#if VERSION == VERSION_DEMO
                offset.set(50.0f + REG0_F(9), REG0_F(10), REG0_F(11));
#else
                offset.x = 50.0f + REG0_F(9);
                offset.y = REG0_F(10);
                offset.z = REG0_F(11);
#endif
                i_this->m13BC[i] = at_spl_d[l_HIO.m82];
                if (i_this->m13B8[i] == 4) {
                    i_this->mWeponSph[i].SetAtHitMark(dCcG_AtHitMark_Big_e);
                }
            }

            i_this->mWeponSph[i].SetR(i_this->m13C4[i]);
            i_this->mWeponSph[i].SetAtSpl((dCcG_At_Spl)i_this->m13BC[i]);
            MtxPosition(&offset, &i_this->m13A0[i]);
            i_this->mWeponSph[i].SetC(i_this->m13A0[i]);

            cMtx_YrotS(*calc_mtx, actor->shape_angle.y);
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 1.0f;
            MtxPosition(&offset, &pos);
            i_this->mWeponSph[i].SetAtVec(pos);

            if (i_this->m13B8[i] < 10) {
                if (player->current.pos.y - i_this->mAcch.GetGroundH() < 50.0f) {
                    i_this->mWeponSph[i].OnAtSetBit();
                } else {
                    i_this->mWeponSph[i].OffAtSetBit();
                }
            } else {
                i_this->mWeponSph[i].OffAtSetBit();
            }

            i_this->m13B8[i] = 0;
            i_this->m13C4[i] = 150.0f + REG0_F(13);
        } else {
            i_this->m13CC[i] = 0;
            i_this->mWeponSph[i].SetC(non_pos);
        }

        dComIfG_Ccsp()->Set(&i_this->mWeponSph[i]);
    }

    if (i_this->m13D0 != 0) {
        i_this->m13D0--;
        i_this->mNextParryOpeningType = 1;
    }
    if (i_this->m13D1 != 0) {
        i_this->m13D1--;
        i_this->mNextParryOpeningType = 2;
    }

    i_this->setBtNowFrame(15.0f);
    u8 opening = i_this->mNextParryOpeningType;
    i_this->setBtStartFrame(10.0f);
    i_this->setBtEndFrame(20.0f);
    i_this->setBtMaxDis(10000.0f);
    i_this->setBtAttackType(opening);
    i_this->setBtMaxDis(600.0f + REG0_F(4));
    i_this->mNextParryOpeningType = 0;

    if (l_HIO.m89 != 0) {
        l_HIO.m89 = 0;
        i_this->m155E = 10;
    }

    demo_camera(i_this);
    ke_move(i_this);
    attack_eff_move(i_this);
#if VERSION > VERSION_DEMO
    body_flash(i_this);
#endif
    return TRUE;
}

/* 000076B8-000076C0       .text daGnd_IsDelete__FP9gnd_class */
static BOOL daGnd_IsDelete(gnd_class*) {
    return TRUE;
}

/* 000076C0-00007738       .text daGnd_Delete__FP9gnd_class */
static BOOL daGnd_Delete(gnd_class* i_this) {
    dComIfG_resDeleteDemo(&i_this->mPhase, "Gnd");
    i_this->mRippleCb.end();
    if (i_this->mHioSet != 0) {
        hio_set = false;
        mDoHIO_deleteChild(l_HIO.mNo);
    }
    return TRUE;
}

/* 00007738-00007A78       .text useHeapInit__FP10fopAc_ac_c */
static BOOL useHeapInit(fopAc_ac_c* a_this) {
    gnd_class* i_this = (gnd_class*)a_this;

    i_this->mpMorf = new mDoExt_McaMorf(
        (J3DModelData*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BDL_GND_e),
        NULL,
        NULL,
        (J3DAnmTransform*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BCK_SAMPLE_e),
        J3DFrameCtrl::EMode_LOOP,
        1.0f,
        0,
        -1,
        1,
        NULL,
        0,
        0x11020203
    );
    if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) {
        return FALSE;
    }

    i_this->mpBrkAnm = new mDoExt_brkAnm();
    if (i_this->mpBrkAnm == NULL) {
        return FALSE;
    }
    if (i_this->mpBrkAnm->init(
            i_this->mpMorf->getModel()->getModelData(),
            (J3DAnmTevRegKey*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BRK_SAMPLE_e),
            TRUE,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            false,
            0
        ) == 0)
    {
        return FALSE;
    }

    i_this->mpBtkAnm = new mDoExt_btkAnm();
    if (i_this->mpBtkAnm == NULL) {
        return FALSE;
    }
    if (i_this->mpBtkAnm->init(
            i_this->mpMorf->getModel()->getModelData(),
            (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BTK_SAMPLE_e),
            TRUE,
            J3DFrameCtrl::EMode_NONE,
            1.0f,
            0,
            -1,
            false,
            0
        ) == 0)
    {
        return FALSE;
    }

    i_this->mpBtpAnm = new mDoExt_btpAnm();
    if (i_this->mpBtpAnm == NULL) {
        return FALSE;
    }
    if (i_this->mpBtpAnm->init(
            i_this->mpMorf->getModel()->getModelData(),
            (J3DAnmTexPattern*)dComIfG_getObjectRes("Gnd", dRes_INDEX_GND_BTP_SAMPLE_e),
            TRUE,
            J3DFrameCtrl::EMode_LOOP,
            1.0f,
            0,
            -1,
            false,
            0
        ) == 0)
    {
        return FALSE;
    }

    return i_this->mLineMat.init(4, 20, FALSE) != 0 ? TRUE : FALSE;
}

/* 00007AC0-00007CFC       .text daGnd_Create__FP10fopAc_ac_c */
static cPhs_State daGnd_Create(fopAc_ac_c* a_this) {
    static dCcD_SrcCyl cc_cyl_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_SWORD,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e,
            /* SrcObjCo  SPrm    */ cCcD_CoSPrm_Set_e | cCcD_CoSPrm_IsPlayer_e | cCcD_CoSPrm_VsGrpAll_e,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_None_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK0,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ 0,
            /* SrcGObjTg Se      */ 0,
            /* SrcGObjTg HitMark */ dCcG_TgHitMark_Nrm_e,
            /* SrcGObjTg Spl     */ dCcG_Tg_Spl_UNK0,
            /* SrcGObjTg Mtrl    */ 0,
            /* SrcGObjTg SPrm    */ dCcG_TgSPrm_NoConHit_e,
            /* SrcGObjCo SPrm    */ 0,
        },
        // cM3dGCylS
        {{
            /* Center */ {0.0f, -150.0f, 0.0f},
            /* Radius */ 100.0f,
            /* Height */ 100.0f,
        }},
    };
    static dCcD_SrcSph head_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_SWORD,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e | cCcD_TgSPrm_IsOther_e,
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
            /* Radius */ 15.0f,
        }},
    };
    static dCcD_SrcSph chest_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ 0,
            /* SrcObjAt  Atp     */ 0,
            /* SrcObjAt  SPrm    */ 0,
            /* SrcObjTg  Type    */ AT_TYPE_NORMAL_ARROW | AT_TYPE_LIGHT_ARROW,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e | cCcD_TgSPrm_IsOther_e,
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
            /* Radius */ 100.0f,
        }},
    };
    static dCcD_SrcSph wepon_sph_src = {
        // dCcD_SrcGObjInf
        {
            /* Flags             */ 0,
            /* SrcObjAt  Type    */ AT_TYPE_UNK8,
            /* SrcObjAt  Atp     */ 4,
            /* SrcObjAt  SPrm    */ cCcD_AtSPrm_Set_e | cCcD_AtSPrm_GrpAll_e,
            /* SrcObjTg  Type    */ AT_TYPE_ALL & ~AT_TYPE_WATER & ~AT_TYPE_UNK20000 & ~AT_TYPE_WIND & ~AT_TYPE_UNK400000 & ~AT_TYPE_LIGHT,
            /* SrcObjTg  SPrm    */ cCcD_TgSPrm_Set_e | cCcD_TgSPrm_IsEnemy_e | cCcD_TgSPrm_IsOther_e,
            /* SrcObjCo  SPrm    */ 0,
            /* SrcGObjAt Se      */ 0,
            /* SrcGObjAt HitMark */ dCcG_AtHitMark_Nrm_e,
            /* SrcGObjAt Spl     */ dCcG_At_Spl_UNK6,
            /* SrcGObjAt Mtrl    */ 0,
            /* SrcGObjAt SPrm    */ dCcG_AtSPrm_NoConHit_e,
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
            /* Radius */ 70.0f,
        }},
    };

    gnd_class* i_this = (gnd_class*)a_this;
    fopAcM_ct(a_this, gnd_class);

    cPhs_State phase = dComIfG_resLoad(&i_this->mPhase, "Gnd");
    if (phase == cPhs_COMPLEATE_e) {
        pz = NULL;
        i_this->m2B4 = fopAcM_GetParam(a_this) & 0xF;
        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, 0x96000)) {
            return cPhs_ERROR_e;
        }

        a_this->attention_info.flags = fopAc_Attn_LOCKON_BATTLE_e;
        a_this->attention_info.distances[fopAc_Attn_TYPE_BATTLE_e] = 4;

        if (!hio_set) {
            i_this->mHioSet = 1;
            hio_set = true;
            l_HIO.mNo = mDoHIO_createChild("ガノンＤ", &l_HIO);
        }

        i_this->initBt(400.0f + REG0_F(6), 150.0f + REG0_F(5));
        i_this->mAcch.Set(
            fopAcM_GetPosition_p(a_this),
            fopAcM_GetOldPosition_p(a_this),
            a_this,
            1,
            &i_this->mAcchCir,
            fopAcM_GetSpeed_p(a_this),
            NULL,
            NULL
        );
        i_this->mAcch.OffSameActorChk();
        i_this->mAcchCir.SetWall(200.0f, 200.0f);
        i_this->mStts.Init(0xFF, 0xFF, a_this);
        i_this->mBodyCyl.Set(cc_cyl_src);
        i_this->mBodyCyl.SetStts(&i_this->mStts);
        i_this->mHeadSph.Set(head_sph_src);
        i_this->mHeadSph.SetStts(&i_this->mStts);
        i_this->mChestSph.Set(chest_sph_src);
        i_this->mChestSph.SetStts(&i_this->mStts);
        for (int i = 0; i < 2; i++) {
            i_this->mWeponSph[i].Set(wepon_sph_src);
            i_this->mWeponSph[i].SetStts(&i_this->mStts);
        }

        daGnd_Execute(i_this);
        a_this->max_health = DEMO_SELECT(90, 0x4B);
        a_this->health = DEMO_SELECT(90, 0x4B);
        if (i_this->m2B4 != 0) {
            mDoAud_bgmStart(JA_BGM_GANON_BATTLE);
        }
    }

    return phase;
}

static actor_method_class l_daGnd_Method = {
    (process_method_func)daGnd_Create,
    (process_method_func)daGnd_Delete,
    (process_method_func)daGnd_Execute,
    (process_method_func)daGnd_IsDelete,
    (process_method_func)daGnd_Draw,
};

actor_process_profile_definition g_profile_GND = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 0x0007,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_GND_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(gnd_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_GND_e,
    /* Actor SubMtd */ &l_daGnd_Method,
    /* Status       */ fopAcStts_UNK40000_e | fopAcStts_BOSS_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};
