// COS_SMOKE=rando COS_RANDO_SMOKE=extrastart (docs/RANDOMIZER_PLAN.md step R6.4): a new game of a seed
// with extra starting items, starting Pieces of Heart and Triforce Shards:
//   native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=extrastart
// The seed is fixtures/R6_4_extra_start.json (ww_export.py R64Seed1 num_extra_starting_items=2
// starting_pohs=7 num_starting_triforce_shards=8; extra items Bait Bag and Skull Hammer). It is
// activated directly (pc_rando_slot_activate, no slot record and no sidecar, so the run needs no card
// of its own), then the new-game state is made as R0.7's hook makes it (dComIfGs_init, then
// pc_rando_new_game_init). Not with COS_RANDO_FORCE.
//
// Once the play scene has run 10 frames:
//   1. vanilla: dComIfGs_init gives 12 of 12 HP, no shards, no Bait Bag, no Skull Hammer;
//   2. the seed: 19 max HP (3 Heart Containers and 7 Pieces of Heart, set_starting_health) and 16 HP
//      (whole hearts), the 8 shards (mTriforce 0xFF, event bit 0x3D04), the Bait Bag and the Skull
//      Hammer in their slots (update_starting_gear with starting_gear + the extra items), the default
//      gear (Ballad of Gales, Song of Passing, Hero's Shield) still given;
//   3. 30 frames later the play scene still shows the same HP, max HP and shards (the HUD reads them);
//   4. pc_rando_deactivate, and a new game is vanilla again (step 1's values).
// Exit 0 when every check held, else 1.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_newgame.h"
#include "features/rando/pc_rando_seedfile.h"
#include "features/rando/pc_rando_slot.h"
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_save.h"

#include <cstdio>
#include <string>
#include <unistd.h>

namespace pc {

namespace {

constexpr const char* kName = "extrastart";
constexpr const char* kFixture = "native/check/rando/fixtures/R6_4_extra_start.json";
constexpr unsigned int kLaterFrames = 30;

std::string resolve(const char* path) {
    if (path[0] == '/' || access(path, R_OK) == 0) {
        return path;
    }
    const char* user = pc_user_path();
    return std::string(user != nullptr ? user : ".") + "/../../../" + path;
}

uint8_t triforce() {
    return g_dComIfG_gameInfo.save.getSavedata().getPlayer().getCollect().mTriforce;
}

bool hasItem(int slot, int item) {
    return dComIfGs_getItem(slot) == item;
}

void vanillaChecks(const char* tag) {
    char what[160];
    auto check = [&](const char* name, bool ok) {
        snprintf(what, sizeof what, "%s: %s", tag, name);
        randoSmokeCheck(kName, what, ok);
    };
    check("pc_rando() 0", pc_rando() == 0);
    check("12 of 12 HP", dComIfGs_getMaxLife() == 12 && dComIfGs_getLife() == 12);
    check("no shards", triforce() == 0 && dComIfGs_isEventBit(PCR_NEWGAME_TRIFORCE_REFUSE) == FALSE);
    check("no Bait Bag", !hasItem(dInvSlot_BAIT_BAG_e, dItemNo_BAIT_BAG_e));
    check("no Skull Hammer", !hasItem(dInvSlot_SKULL_HAMMER_e, dItemNo_SKULL_HAMMER_e));
}

void seedStateChecks(const char* tag) {
    char what[160];
    auto check = [&](const char* name, bool ok) {
        snprintf(what, sizeof what, "%s: %s", tag, name);
        randoSmokeCheck(kName, what, ok);
    };
    const u16 maxLife = dComIfGs_getMaxLife();
    const u16 life = dComIfGs_getLife();
    if (maxLife != 19 || life != 16) {
        writef(STDERR_FILENO, "[cos] rando-smoke extrastart: %s: HP %u of %u\n", tag, life, maxLife);
    }
    check("19 max HP (3 HCs + 7 PoHs), 16 HP", maxLife == 19 && life == 16);
    check("8 shards (mTriforce 0xFF) and 0x3D04",
          triforce() == 0xFF && dComIfGs_isEventBit(PCR_NEWGAME_TRIFORCE_REFUSE) != FALSE);
}

bool sStarted = false;
unsigned int sStartFrame = 0;

void start(unsigned int frames) {
    if (!randoSmokeCheck(kName, "not forced (COS_RANDO_FORCE is unset)", !pc_rando_forced())) {
        randoSmokeEnd(kName);
    }
    // 1. vanilla
    dComIfGs_init();
    pc_rando_new_game_init();
    vanillaChecks("vanilla");

    // 2. the seed
    const std::string fixture = resolve(kFixture);
    PcRandoSeed seed;
    std::string err;
    if (!randoSmokeCheck(kName, ("fixture " + fixture + " loads").c_str(),
                         pc_rando_seed_load_file(fixture.c_str(), &seed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke extrastart: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    randoSmokeCheck(kName, "fixture: extra items Bait Bag, Skull Hammer",
                    seed.extra_starting_items.size() == 2 && seed.extra_starting_items[0] == "Bait Bag" &&
                        seed.extra_starting_items[1] == "Skull Hammer");
    if (!randoSmokeCheck(kName, "seed activates", pc_rando_slot_activate(seed, &err))) {
        writef(STDERR_FILENO, "[cos] rando-smoke extrastart: %s\n", err.c_str());
        randoSmokeEnd(kName);
    }
    randoSmokeCheck(kName, "seed: pc_rando() 1", pc_rando() == 1);
    const PcRandoConfig& cfg = *pc_rando_cfg();
    randoSmokeCheck(kName, "seed: config 19 quarter hearts, 8 shards",
                    cfg.starting_quarter_hearts == 19 && cfg.num_triforce_shards == 8);
    dComIfGs_init();
    pc_rando_new_game_init();
    seedStateChecks("seed");
    randoSmokeCheck(kName, "seed: extra item Bait Bag",
                    hasItem(dInvSlot_BAIT_BAG_e, dItemNo_BAIT_BAG_e) && dComIfGs_isGetItem(dInvSlot_BAIT_BAG_e, 0));
    randoSmokeCheck(kName, "seed: extra item Skull Hammer",
                    hasItem(dInvSlot_SKULL_HAMMER_e, dItemNo_SKULL_HAMMER_e) &&
                        dComIfGs_isGetItem(dInvSlot_SKULL_HAMMER_e, 0));
    randoSmokeCheck(kName, "seed: gear Ballad of Gales and Song of Passing",
                    dComIfGs_isTact(1) != FALSE && dComIfGs_isTact(5) != FALSE);
    randoSmokeCheck(kName, "seed: gear Progressive Shield -> Hero's Shield",
                    dComIfGs_isCollect(1, 0) && dComIfGs_getSelectEquip(1) == dItemNo_SHIELD_e);
    sStarted = true;
    sStartFrame = frames;
}

} // namespace

void randoSmokeExtrastart(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (!sStarted) {
        start(frames);
        return;
    }
    if (frames - sStartFrame < kLaterFrames) {
        return;
    }
    // 3. the play scene kept the state
    seedStateChecks("30 frames later");
    // 4. back to vanilla
    pc_rando_deactivate();
    dComIfGs_init();
    pc_rando_new_game_init();
    vanillaChecks("deactivated");
    randoSmokeEnd(kName);
}

} // namespace pc
