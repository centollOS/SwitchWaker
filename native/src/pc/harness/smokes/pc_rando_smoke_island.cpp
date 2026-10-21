// COS_SMOKE=rando COS_RANDO_SMOKE=island (docs/RANDOMIZER_PLAN.md step R6.7, "rando-start with the
// option"): a real new game with a randomize_starting_island seed starts on that seed's island.
//   native/tools/run.sh rando --input native/check/input/new-game-skip-intro.txt \
//       --env COS_RANDO_SMOKE=island
// No --stage: the real boot, with its own empty run card (pc_rando_smoke_wants_run_card), as the R3.2
// start smoke. The pending seed is COS_RANDO_ISLAND_FIXTURE, by default R6_7_TestSeed1_island.json:
// cos_rando_cli TestSeed1 randomize_starting_island=true (golden against wwrando: sea room 15, Western
// Fairy Island). R0.7's NameInMain hook consumes it.
//
// 1. At the first frame: the fixture's starting_island_room is not 44 (else the run proves nothing),
//    the pending seed is set and pc_rando() is 0.
// 2. Once the PLAY scene runs in sea room <island> with the player spawned and no event has run for
//    kFreeFrames frames: pc_rando() is 1, pc_rando_start_room() is the island, the start point and the
//    save's return place are sea <island> point 0 (starting_island.py _save:
//    set_new_game_starting_spawn_id(0), set_new_game_starting_room(room)), the sea stage's Ship ACTR
//    was patched from that room's SHIP 0 (change_ship_starting_island(room)) and the Ship actor stands
//    there, afloat, near the player.
// 3. Direct calls: after pc_rando_deactivate, pc_rando_start_room() is 44 and a vanilla
//    dSv_player_return_place_c::init gives sea room 44 point 206.
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"
#include "features/rando/pc_rando_start.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
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

constexpr const char* kName = "island";
constexpr const char* kDefaultFixture = "native/check/rando/fixtures/R6_7_TestSeed1_island.json";
constexpr unsigned int kFreeFrames = 60;
constexpr float kShipTolerance = 150.0f;
constexpr float kNearPlayer = 5000.0f;

bool sStarted = false;
unsigned int sFree = 0;
int sIsland = 0;
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
    const char* env = getenv("COS_RANDO_ISLAND_FIXTURE");
    const std::string fixture = resolve(env != nullptr && env[0] != '\0' ? env : kDefaultFixture);
    std::string err;
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &sSeed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke island: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    sIsland = sSeed.starting_island_room;
    writef(STDERR_FILENO, "[cos] rando-smoke island: the seed starts on sea room %d\n", sIsland);
    if (!randoSmokeCheck(kName, "the fixture's starting island is not Outset (2-49, not 44)",
                         sIsland >= 2 && sIsland <= 49 && sIsland != 44)) {
        randoSmokeEnd(kName);
    }
    pc_rando_slot_set_pending(sSeed);
    randoSmokeCheck(kName, "boot: pc_rando() 0, a pending seed, start room 44",
                    pc_rando() == 0 && pc_rando_slot_pending() != nullptr && pc_rando_start_room() == PCR_START_ROOM);
}

float dist2(const cXyz& a, float x, float z) {
    const float dx = a.x - x, dz = a.z - z;
    return std::sqrt(dx * dx + dz * dz);
}

void islandChecks() {
    randoSmokeCheck(kName, "pc_rando() 1 after the new game", pc_rando() == 1);
    randoSmokeCheck(kName, "pc_rando_start_room() is the seed's island", pc_rando_start_room() == sIsland);
    randoSmokeCheck(kName, "start point 0", dComIfGp_getStartStagePoint() == 0);
    dSv_player_return_place_c* rp = dComIfGs_getpPlayerReturnPlace();
    writef(STDERR_FILENO, "[cos] rando-smoke island: return place %s room %d point %d\n", rp->getName(),
           rp->getRoomNo(), rp->getPoint());
    randoSmokeCheck(kName, "return place: sea, the island's room, point 0",
                    strcmp(rp->getName(), "sea") == 0 && rp->getRoomNo() == sIsland && rp->getPoint() == 0);

    float sx = 0, sz = 0;
    uint16_t syRot = 0;
    int patched = 0;
    const bool haveSpawn = pc_rando_start_ship_spawn(&sx, &sz, &syRot, &patched) != 0;
    randoSmokeCheck(kName, "the island room's SHIP 0 read from the disc", haveSpawn);
    randoSmokeCheck(kName, "the sea stage's Ship ACTR was patched", patched >= 1);
    fopAc_ac_c* ship = fopAcM_SearchByName(fpcNm_SHIP_e);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (!randoSmokeCheck(kName, "the Ship actor exists", ship != nullptr) ||
        !randoSmokeCheck(kName, "the player exists", player != nullptr)) {
        return;
    }
    writef(STDERR_FILENO,
           "[cos] rando-smoke island: Ship at (%.1f, %.1f, %.1f) yaw 0x%04X; Room%d SHIP 0 x/z (%.1f, %.1f) y_rot 0x%04X; "
           "player at (%.1f, %.1f, %.1f)\n",
           ship->current.pos.x, ship->current.pos.y, ship->current.pos.z, (unsigned)(uint16_t)ship->shape_angle.y,
           sIsland, sx, sz, syRot, player->current.pos.x, player->current.pos.y, player->current.pos.z);
    randoSmokeCheck(kName, "the Ship stands at the island's SHIP 0 x/z",
                    haveSpawn && dist2(ship->current.pos, sx, sz) < kShipTolerance);
    randoSmokeCheck(kName, "the Ship floats (not left at y -500000)", ship->current.pos.y > -1000.0f);
    randoSmokeCheck(kName, "the Ship is near the player",
                    dist2(player->current.pos, ship->current.pos.x, ship->current.pos.z) < kNearPlayer);
}

void vanillaChecks() {
    dSv_player_return_place_c rp;
    pc_rando_deactivate();
    randoSmokeCheck(kName, "deactivated: pc_rando() 0, start room 44", pc_rando() == 0 && pc_rando_start_room() == 44);
    rp.init();
    randoSmokeCheck(kName, "vanilla return place: sea room 44 point 206",
                    strcmp(rp.getName(), "sea") == 0 && rp.getRoomNo() == 44 && rp.getPoint() == 206);
}

} // namespace

void randoSmokeIsland(unsigned int frames) {
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
    const bool here = stage != nullptr && strcmp(stage, "sea") == 0 && dComIfGp_roomControl_getStayNo() == sIsland;
    if (!here || dComIfGp_event_runCheck()) {
        sFree = 0;
        return;
    }
    if (++sFree < kFreeFrames) {
        return;
    }
    islandChecks();
    vanillaChecks();
    randoSmokeEnd(kName);
}

} // namespace pc
