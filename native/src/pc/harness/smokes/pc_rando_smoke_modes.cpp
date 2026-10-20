// COS_SMOKE=rando COS_RANDO_SMOKE=modes (docs/RANDOMIZER_PLAN.md step R6.15): the small modes' hunks
// in game/ (native/include/pc/pc_rando_modes.h), each once with pc_rando() off (vanilla, with every
// mode set, to show they are ignored) and once on with hero mode, switch targeting, 1 starting heart,
// remove_music and the inverted compass:
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=modes      (no COS_RANDO_FORCE: it needs "off")
//   run.sh rando --preset sailing --env COS_RANDO_SMOKE=modes    (also the sea compass, on the boat)
// Once the play scene has run 10 frames, in one frame:
//   1. daPy_lk_c::setDamagePoint(-1): the pending life change is -1, -4 in hero mode;
//   2. getItemNoByLife with health missing: a heart stays a heart, becomes a green rupee in hero mode
//      (a triple heart a yellow rupee);
//   3. dSv_player_config_c::init: attention type 0, 1 with switch targeting;
//   4. daPy_lk_c::checkRestHPAnime with 1 heart of 1: true vanilla, false with the low-health patch
//      (when the player is idle on land, where half a heart of 1 gives true; else skipped); the beep of
//      1-heart beep of JAIZelBasic::processHeartGaugeSound is dropped at full health (pc_rando_mute_low_health_beep);
//   5. JAIZelBasic::bgmStart / subBgmStart (with the no-op number 0xFFFFFFFF): vanilla clears
//      field_0x00be at entry, remove_music returns before it; bgmStreamPlay leaves field_0x00b8 alone
//      and bgmNowBattle returns (remove_music only, a vanilla call would start music);
// then over the next frames:
//   6. a smoke cloud with the Keese drop type (fopAcM_createDisappear, daDisItem_HEART_e) with health
//      missing drops a heart, a green rupee in hero mode (daDisappear_Execute's ki_item_d);
//   7. on the boat only (--preset sailing): the meter's compass needle pane is rotated by
//      -(ship angle + 0x8000) * 180 / 0x8000, by +(...) with the inverted compass.
// Health, attention type and the audio fields are restored. Exit 0 when all hold, else 1. The hunks in
// d_a_ki (Puppet Ganon's Keese) and d_a_bl (Jalhalla's skulls) need those fights; their decisions
// (pc_rando_hero_mode, pc_rando_hero_drop) are checked here and in cos_rando_test_R6_15.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_modes.h"

#include "JAZelAudio/JAIZelBasic.h"
#include "d/actor/d_a_item.h"
#include "d/actor/d_a_player_main.h"
#include "d/actor/d_a_ship.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_meter.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "SSystem/SComponent/c_math.h"

#include <cmath>
#include <cstdio>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "modes";
constexpr unsigned int kDropFrames = 120;  // the cloud drops after 58 frames
constexpr unsigned int kCompassFrames = 4;

enum State { kStart, kDropVanilla, kDropHero, kCompassVanilla, kCompassInverted, kEnd };
State sState = kStart;
unsigned int sSince = 0;
unsigned int sEndFrame = 0;
cXyz sDropPos;
int sDropItem = -1;
u16 sSavedLife = 0;

void* isPlayScene(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_PLAY_SCENE_e ? proc : nullptr;
}
void* isMeter(void* proc, void*) {
    return fpcM_GetName(proc) == fpcNm_METER_e ? proc : nullptr;
}
void* nearDropItem(void* proc, void*) {
    if (fpcM_GetName(proc) != fpcNm_ITEM_e) {
        return nullptr;
    }
    fopAc_ac_c* ac = (fopAc_ac_c*)proc;
    const f32 dx = ac->current.pos.x - sDropPos.x;
    const f32 dz = ac->current.pos.z - sDropPos.z;
    return dx * dx + dz * dz < 250.0f * 250.0f ? proc : nullptr;
}

void activeConfig() {
    PcRandoConfig cfg;
    pc_rando_config_defaults(&cfg);
    pc_rando_config_set_hero_mode(&cfg, 1);
    cfg.targeting_mode = 1;
    cfg.starting_quarter_hearts = 4;
    pc_rando_set_active(nullptr, &cfg);
}

void allModes() {
    PcRandoModes m;
    pc_rando_modes_defaults(&m);
    m.remove_music = 1;
    m.invert_sea_compass_x_axis = 1;
    pc_rando_modes_set(&m);
}

// Steps 1-5 with pc_rando() as it is now; `on` says which answers to expect.
void frameChecks(bool on) {
    char what[160];
    auto check = [&](const char* fmt, bool ok) {
        snprintf(what, sizeof what, "%s: %s", on ? "rando" : "vanilla", fmt);
        randoSmokeCheck(kName, what, ok);
    };
    daPy_lk_c* link = (daPy_lk_c*)dComIfGp_getLinkPlayer();
    const u16 life = dComIfGs_getLife();
    const u16 maxLife = dComIfGs_getMaxLife();

    // 1. damage
    dComIfGp_clearItemLifeCount();
    link->setDamagePoint(-1.0f);
    const f32 dmg = dComIfGp_getItemLifeCount();
    dComIfGp_clearItemLifeCount();
    check("setDamagePoint(-1) life change", dmg == (on ? -4.0f : -1.0f));

    // 2. drops with health missing
    dComIfGs_setLife((u16)(maxLife - 4));
    check("getItemNoByLife(heart), health missing",
          getItemNoByLife(dItemNo_HEART_e) == (on ? dItemNo_GREEN_RUPEE_e : dItemNo_HEART_e));
    check("getItemNoByLife(triple heart), health missing",
          getItemNoByLife(dItemNo_TRIPLE_HEART_e) == (on ? dItemNo_YELLOW_RUPEE_e : dItemNo_TRIPLE_HEART_e));
    dComIfGs_setLife(maxLife);
    check("getItemNoByLife(heart), full health", getItemNoByLife(dItemNo_HEART_e) == dItemNo_GREEN_RUPEE_e);

    // 3. new file attention type
    dSv_player_config_c* config = dComIfGs_getpConfig();
    const u8 savedAttention = config->getAttentionType();
    config->init();
    check("config init attention type", config->getAttentionType() == (on ? 1 : 0));
    config->setAttentionType(savedAttention);

    // 4. low health with one heart
    dComIfGs_setMaxLife(4);
    dComIfGs_setLife(4);
    const BOOL fullAnim = link->checkRestHPAnime();
    dComIfGs_setLife(2);
    const BOOL halfAnim = link->checkRestHPAnime();
    dComIfGs_setMaxLife((u8)maxLife);
    dComIfGs_setLife(life);
    if (halfAnim) {
        check("checkRestHPAnime, 1 heart of 1", fullAnim == (on ? FALSE : TRUE));
    } else {
        // Not idle on land (e.g. on the boat): the animation is off whatever the health, and the hunk
        // (which only turns a true into false) is not reached.
        writef(STDERR_FILENO, "[cos] rando-smoke %s: checkRestHPAnime false at half a heart (not idle on land): skipped\n",
               kName);
        check("checkRestHPAnime, 1 heart of 1, not idle on land", fullAnim == FALSE);
    }
    check("1-heart beep dropped at 4/4", pc_rando_mute_low_health_beep(4, 4) == (on ? 1 : 0));

    // 5. music
    JAIZelBasic* zel = JAIZelBasic::getInterface();
    const u8 saved_be = zel->field_0x00be;
    zel->field_0x00be = 1;
    zel->bgmStart(0xFFFFFFFF, 0, 0);
    check("bgmStart entry", zel->field_0x00be == (on ? 1 : 0));
    zel->field_0x00be = 1;
    zel->subBgmStart(0xFFFFFFFF);
    check("subBgmStart entry", zel->field_0x00be == (on ? 1 : 0));
    zel->field_0x00be = saved_be;
    if (on) {
        const u8 saved_b8 = zel->field_0x00b8;
        const u32 mainBgm = zel->mMainBgmNum;
        zel->field_0x00b8 = 0;
        zel->bgmStreamPlay();
        check("bgmStreamPlay returns at once", zel->field_0x00b8 == 0);
        zel->field_0x00b8 = saved_b8;
        zel->bgmNowBattle(100.0f);
        check("bgmNowBattle returns, main bgm unchanged", zel->mMainBgmNum == mainBgm);
    }
}

void spawnDrop(unsigned int frames) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const s16 yaw = player->shape_angle.y;
    sDropPos.set(player->current.pos.x + 150.0f * cM_ssin(yaw), player->current.pos.y + 30.0f,
                 player->current.pos.z + 150.0f * cM_scos(yaw));
    sDropItem = -1;
    sSavedLife = dComIfGs_getLife();
    dComIfGs_setLife((u16)(dComIfGs_getMaxLife() - 4)); // a heart missing: hearts stay hearts
    layer_class* saved = fpcLy_CurrentLayer();
    fpcLy_SetCurrentLayer(&((process_node_class*)fpcM_Search(isPlayScene, nullptr))->mLayer);
    fopAcM_createDisappear(player, &sDropPos, 5, daDisItem_HEART_e);
    fpcLy_SetCurrentLayer(saved);
    writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u: smoke cloud (Keese drop) at (%.0f, %.0f, %.0f)\n",
           kName, frames, sDropPos.x, sDropPos.y, sDropPos.z);
}

// True when the drop was seen or the wait ran out.
bool pollDrop(unsigned int frames, bool hero) {
    if (sDropItem < 0) {
        daItem_c* item = (daItem_c*)fpcM_Search(nearDropItem, nullptr);
        if (item != nullptr) {
            sDropItem = (int)item->getItemNo();
            writef(STDERR_FILENO, "[cos] rando-smoke %s: frame %u: the cloud dropped item 0x%02X\n", kName, frames,
                   sDropItem);
            fopAcM_delete(item); // out of the way of the next cloud's search
        }
    }
    if (sDropItem < 0 && sSince < kDropFrames) {
        return false;
    }
    dComIfGs_setLife(sSavedLife);
    randoSmokeCheck(kName, hero ? "rando: Keese cloud drops a green rupee" : "vanilla: Keese cloud drops a heart",
                    sDropItem == (hero ? dItemNo_GREEN_RUPEE_e : dItemNo_HEART_e));
    return true;
}

// The needle pane's rotation now and what dMeter_compassGetOnProc gives for each sign.
bool compassCheck(bool inverted) {
    daShip_c* ship = dComIfGp_getShipActor();
    sub_meter_class* meter = (sub_meter_class*)fpcM_Search(isMeter, nullptr);
    if (ship == nullptr || meter == nullptr || meter->field_0x1248.pane == nullptr) {
        randoSmokeCheck(kName, "compass: ship and meter", false);
        return false;
    }
    const s16 angle = ship->shape_angle.y + 0x8000;
    const f32 vanilla = (-angle * 180.0f) / 0x8000;
    const f32 flipped = (angle * 180.0f) / 0x8000;
    const f32 rot = meter->field_0x1248.pane->getRotate();
    writef(STDERR_FILENO, "[cos] rando-smoke %s: compass: ship angle 0x%04X, needle %.3f (vanilla %.3f, inverted %.3f)\n",
           kName, (unsigned)(u16)ship->shape_angle.y, rot, vanilla, flipped);
    const f32 want = inverted ? flipped : vanilla;
    return randoSmokeCheck(kName, inverted ? "rando: compass needle inverted" : "vanilla: compass needle",
                           std::fabs(rot - want) < 0.01f && (vanilla == flipped || std::fabs(rot - (inverted ? vanilla : flipped)) > 0.01f));
}

} // namespace

void randoSmokeModes(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            pc_rando_deactivate();
            pc_rando_modes_set(nullptr);
            randoSmokeEnd(kName);
        }
        return;
    }
    ++sSince;
    switch (sState) {
    case kStart:
        if (!randoSmokeCheck(kName, "not forced (the smoke needs pc_rando() off)", !pc_rando_forced())) {
            sEndFrame = frames + 1;
            return;
        }
        allModes();
        frameChecks(false);
        activeConfig();
        frameChecks(true);
        pc_rando_deactivate();
        spawnDrop(frames);
        sState = kDropVanilla;
        sSince = 0;
        return;
    case kDropVanilla:
        if (pollDrop(frames, false)) {
            activeConfig();
            spawnDrop(frames);
            sState = kDropHero;
            sSince = 0;
        }
        return;
    case kDropHero:
        if (pollDrop(frames, true)) {
            pc_rando_deactivate();
            if (dComIfGp_getShipActor() == nullptr) {
                writef(STDERR_FILENO, "[cos] rando-smoke %s: no boat: compass not checked (use --preset sailing)\n", kName);
                sEndFrame = frames + 30;
                return;
            }
            sState = kCompassVanilla;
            sSince = 0;
        }
        return;
    case kCompassVanilla:
        if (sSince >= kCompassFrames) {
            compassCheck(false);
            activeConfig();
            sState = kCompassInverted;
            sSince = 0;
        }
        return;
    case kCompassInverted:
        if (sSince >= kCompassFrames) {
            compassCheck(true);
            pc_rando_deactivate();
            sState = kEnd;
            sEndFrame = frames + 30;
        }
        return;
    case kEnd:
        return;
    }
}

} // namespace pc
