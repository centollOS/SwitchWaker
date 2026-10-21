// COS_SMOKE=rando COS_RANDO_SMOKE=start (docs/RANDOMIZER_PLAN.md step R3.2, the plan's target
// "rando-start"): a real new game with a pending seed starts on Outset's dock with the default
// preset's health, sword and clothes, and the boat next to the dock.
//   native/tools/run.sh rando --input native/check/input/new-game-skip-intro.txt \
//       --env COS_RANDO_SMOKE=start
// No --stage: the real boot. The run has its own memory card (pc_rando_smoke_wants_run_card), which is
// empty, so the input script creates the save file, picks file 1 and confirms the default name; R0.7's
// NameInMain hook consumes the pending seed (COS_RANDO_START_FIXTURE, default the TestSeed1 placement:
// start with a sword, 3 hearts, Hero's clothes), and R3.11 skips the opening scene. Not with
// COS_RANDO_FORCE.
//
// 1. At the first frame (file select is still ahead, pc_rando() is 0): the pending seed is set, and
//    the file-select HP hook gives a blank slot the seed's starting HP (rando arm: pending seed) and a
//    used slot its saved HP.
// 2. Once the PLAY scene runs in sea room 44 with the player spawned and no event has run for
//    kFreeFrames frames: pc_rando() is 1; the start point and the save's return place are spawn 0
//    (skip_wakeup_intro_and_start_at_dock); HP is 12 of 12 (set_starting_health); the sword is
//    equipped; Link wears the Hero's clothes (check_player_in_casual_clothes); the sea stage's Ship
//    ACTR was patched, and the Ship actor stands at sea/Room44's SHIP 0 x/z (start_ship_at_outset;
//    the y of prevent_fire_mountain_lava_softlock was undone by the sea actor, so it is on the
//    surface), near the player.
// 3. In the same frame, direct calls: the load hook refills magic and caps HP at whole hearts
//    (13 of 13 -> 12); a config with 13 quarter hearts gives a new file 13 max and 12 current HP and
//    the game-over HP rule; dSv_info_c::reinit (New Game+) under rando leaves Lenzo's register, the
//    jail chest, the Deluxe Picto Box and the two camera owned bits alone and keeps spawn 0, and
//    after pc_rando_deactivate sets all of them and spawn 206 again (vanilla).
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_start.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/actor/d_a_player_main.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "start";
constexpr const char* kDefaultFixture = "native/check/rando/fixtures/R0_6_TestSeed1.json";
constexpr unsigned int kFreeFrames = 60;
constexpr float kShipTolerance = 150.0f;
constexpr float kNearPlayer = 3000.0f;

bool sStarted = false;
unsigned int sFree = 0;
PcRandoSeed sSeed;

std::string resolve(const char* path) {
    if (path[0] == '/' || access(path, R_OK) == 0) {
        return path;
    }
    const char* user = pc_user_path();
    return std::string(user != nullptr ? user : ".") + "/../../../" + path;
}

void begin() {
    if (!randoSmokeCheck(kName, "not forced (COS_RANDO_FORCE is unset)", !pc_rando_forced())) {
        randoSmokeEnd(kName);
    }
    const char* env = getenv("COS_RANDO_START_FIXTURE");
    const std::string fixture = resolve(env != nullptr && env[0] != '\0' ? env : kDefaultFixture);
    std::string err;
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &sSeed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke start: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    pc_rando_slot_set_pending(sSeed);
    randoSmokeCheck(kName, "boot: pc_rando() 0, a pending seed", pc_rando() == 0 && pc_rando_slot_pending() != nullptr);
    randoSmokeCheck(kName, "file select: a blank slot shows the seed's 12 of 12 quarter hearts",
                    pc_rando_start_file_select_life(0, 40, 0) == 12 && pc_rando_start_file_select_life(0, 40, 1) == 12);
    randoSmokeCheck(kName, "file select: a used slot shows its saved HP",
                    pc_rando_start_file_select_life('L', 7, 0) == 7 && pc_rando_start_file_select_life('L', 40, 1) == 40);
}

float dist2(const cXyz& a, float x, float z) {
    const float dx = a.x - x, dz = a.z - z;
    return std::sqrt(dx * dx + dz * dz);
}

void startChecks() {
    randoSmokeCheck(kName, "pc_rando() 1 after the new game", pc_rando() == 1);
    randoSmokeCheck(kName, "the record is in mReserve", pc_rando_slot_record_is_rando(dComIfGs_getPReserve()->mReserve) != 0);
    randoSmokeCheck(kName, "start point 0 (not the wakeup spawn 206)", dComIfGp_getStartStagePoint() == 0);
    randoSmokeCheck(kName, "return place sea room 44 point 0",
                    strcmp(dComIfGs_getpPlayerReturnPlace()->getName(), "sea") == 0 &&
                        dComIfGs_getpPlayerReturnPlace()->getRoomNo() == 44 &&
                        dComIfGs_getpPlayerReturnPlace()->getPoint() == 0);
    writef(STDERR_FILENO, "[cos] rando-smoke start: HP %u of %u, magic %u of %u\n", dComIfGs_getLife(),
           dComIfGs_getMaxLife(), dComIfGs_getMagic(), dComIfGs_getMaxMagic());
    randoSmokeCheck(kName, "3 hearts: HP 12 of 12", dComIfGs_getLife() == 12 && dComIfGs_getMaxLife() == 12);
    randoSmokeCheck(kName, "sword equipped (sword mode: start with a sword)",
                    pc_rando_cfg()->sword_mode == PC_RANDO_SWORD_START && dComIfGs_getSelectEquip(0) == dItemNo_SWORD_e);
    daPy_lk_c* link = daPy_getPlayerLinkActorClass();
    randoSmokeCheck(kName, "Hero's clothes (should_start_with_heros_clothes 1)",
                    link != nullptr && link->checkNoResetFlg1(daPy_lk_c::daPyFlg1_CASUAL_CLOTHES) == 0);

    float sx = 0, sz = 0;
    uint16_t syRot = 0;
    int patched = 0;
    const bool haveSpawn = pc_rando_start_ship_spawn(&sx, &sz, &syRot, &patched) != 0;
    randoSmokeCheck(kName, "sea/Room44 SHIP 0 read from the disc", haveSpawn);
    randoSmokeCheck(kName, "the sea stage's Ship ACTR was patched", patched >= 1);
    fopAc_ac_c* ship = fopAcM_SearchByName(fpcNm_SHIP_e);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!randoSmokeCheck(kName, "the Ship actor exists", ship != nullptr) || player == nullptr) {
        return;
    }
    writef(STDERR_FILENO,
           "[cos] rando-smoke start: Ship at (%.1f, %.1f, %.1f) yaw 0x%04X; Room44 SHIP 0 x/z (%.1f, %.1f) y_rot 0x%04X; "
           "player at (%.1f, %.1f, %.1f)\n",
           ship->current.pos.x, ship->current.pos.y, ship->current.pos.z, (unsigned)(uint16_t)ship->shape_angle.y, sx,
           sz, syRot, player->current.pos.x, player->current.pos.y, player->current.pos.z);
    randoSmokeCheck(kName, "the Ship stands at Room44's SHIP 0 x/z", haveSpawn && dist2(ship->current.pos, sx, sz) < kShipTolerance);
    randoSmokeCheck(kName, "the Ship floats (not left at y -500000)", ship->current.pos.y > -1000.0f);
    randoSmokeCheck(kName, "the Ship is next to the player on the dock",
                    dist2(player->current.pos, ship->current.pos.x, ship->current.pos.z) < kNearPlayer);
}

void directChecks() {
    // the load hook (FileSelectMainNormal)
    dSv_player_status_a_c& st = g_dComIfG_gameInfo.save.getPlayer().getPlayerStatusA();
    const PcRandoConfig cfg = *pc_rando_cfg();
    st.setMaxLife(13);
    st.setLife(13);
    dComIfGs_setMagic(0);
    pc_rando_start_on_load();
    randoSmokeCheck(kName, "load: magic refilled, HP 13 of 13 capped to 12",
                    dComIfGs_getMagic() == dComIfGs_getMaxMagic() && dComIfGs_getLife() == 12);
    st.setLife(5);
    pc_rando_start_on_load();
    randoSmokeCheck(kName, "load: lower HP is kept", dComIfGs_getLife() == 5);
    randoSmokeCheck(kName, "game over: min(12, whole hearts of the max)",
                    pc_rando_start_gameover_life(13) == 12 && pc_rando_start_gameover_life(11) == 8);

    // set_starting_health with 13 quarter hearts
    PcRandoConfig c = cfg;
    c.starting_quarter_hearts = 13;
    pc_rando_set_active(pc_rando_seed(), &c);
    st.init();
    randoSmokeCheck(kName, "new file with 13 quarter hearts: max 13, HP 12", st.getMaxLife() == 13 && st.getLife() == 12);
    randoSmokeCheck(kName, "file select (active seed): blank slot 12 of 13",
                    pc_rando_start_file_select_life(0, 0, 0) == 12 && pc_rando_start_file_select_life(0, 0, 1) == 13);
    pc_rando_set_active(pc_rando_seed(), &cfg);

    // New Game+ (make_game_nonlinear.asm reinit)
    auto ngPlus = [](bool rando) {
        dComIfGs_setEventReg(dSv_event_flag_c::UNK_C407, 0);
        g_dComIfG_gameInfo.save.reinit();
        const bool lenzo = dComIfGs_getEventReg(dSv_event_flag_c::UNK_C407) == 7;
        const bool jail = dComIfGs_isSaveTbox(dSv_save_c::STAGE_MISC, 0) != FALSE;
        const bool deluxe = dComIfGs_getItem(dInvSlot_CAMERA_e) == dItemNo_DELUXE_PICTO_BOX_e;
        const bool owned0 = dComIfGs_isGetItem(dInvSlot_CAMERA_e, 0) != FALSE;
        const bool owned1 = dComIfGs_isGetItem(dInvSlot_CAMERA_e, 1) != FALSE;
        const int point = dComIfGs_getpPlayerReturnPlace()->getPoint();
        writef(STDERR_FILENO, "[cos] rando-smoke start: reinit (%s): Lenzo %d jail %d deluxe %d owned %d/%d point %d\n",
               rando ? "rando" : "vanilla", lenzo, jail, deluxe, owned0, owned1, point);
        if (rando) {
            return !lenzo && !jail && !deluxe && !owned0 && !owned1 && point == 0;
        }
        return lenzo && jail && deluxe && owned0 && owned1 && point == 206;
    };
    randoSmokeCheck(kName, "New Game+ under rando: 5 no-ops, spawn 0", ngPlus(true));
    pc_rando_deactivate();
    randoSmokeCheck(kName, "deactivated: pc_rando() 0", pc_rando() == 0);
    randoSmokeCheck(kName, "New Game+ vanilla: all 5 set, spawn 206", ngPlus(false));
    st.init();
    randoSmokeCheck(kName, "vanilla new file: HP 12 of 12", st.getMaxLife() == 12 && st.getLife() == 12);
}

} // namespace

void randoSmokeStart(unsigned int frames) {
    (void)frames;
    if (!sStarted) {
        sStarted = true;
        begin();
    }
    if (!randoSmokeInPlay(1)) {
        sFree = 0;
        return;
    }
    const char* stage = dComIfGp_getStartStageName();
    const bool here = stage != nullptr && strcmp(stage, "sea") == 0 && dComIfGp_roomControl_getStayNo() == 44;
    if (!here || dComIfGp_event_runCheck()) {
        sFree = 0;
        return;
    }
    if (++sFree < kFreeFrames) {
        return;
    }
    startChecks();
    directChecks();
    randoSmokeEnd(kName);
}

} // namespace pc
