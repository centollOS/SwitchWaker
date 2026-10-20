// COS_SMOKE=rando COS_RANDO_SMOKE=null (docs/RANDOMIZER_PLAN.md step R0.1): the randomizer's switch in
// a booted game, e.g. run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=null. Once the play scene has
// run 10 frames:
//   1. a plain boot is not randomized: pc_rando() is 0 (1 with COS_RANDO_FORCE=1), the config holds
//      the defaults and pc_rando_loc_item gives the vanilla item;
//   2. pc_rando_set_active turns it on, calls a listener with 1 and loc_item reads the placement;
//   3. pc_rando_deactivate turns it off again (unless forced), calls the listener with the new value,
//      drops the placement and restores the default config;
//   4. the game then runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "pc/pc_rando.h"

#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "null";
int sLastNotify = -1;
int sNotifyCount = 0;
unsigned int sEndFrame = 0;

void listener(int active) {
    sLastNotify = active;
    sNotifyCount++;
}

bool defaultConfig() {
    PcRandoConfig d;
    pc_rando_config_defaults(&d);
    return memcmp(&d, pc_rando_cfg(), sizeof d) == 0;
}

} // namespace

void randoSmokeNull(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    const int forced = pc_rando_forced();
    randoSmokeCheck(kName, forced ? "forced boot: pc_rando() is 1" : "plain boot: pc_rando() is 0",
                    pc_rando() == forced);
    randoSmokeCheck(kName, "default config", defaultConfig());
    randoSmokeCheck(kName, "no seed", pc_rando_seed() == nullptr);
    randoSmokeCheck(kName, "loc_item gives vanilla", pc_rando_loc_item(0, 0x42) == 0x42);

    pc_rando_add_listener(listener);
    sNotifyCount = 0;
    static const uint8_t kPlacement[3] = {0x50, 0x51, 0x52};
    pc_rando_set_loc_items(kPlacement, 3);
    PcRandoConfig cfg;
    pc_rando_config_defaults(&cfg);
    cfg.sword_mode = PC_RANDO_SWORD_SWORDLESS;
    pc_rando_set_active(nullptr, &cfg);
    randoSmokeCheck(kName, "set_active: pc_rando() is 1", pc_rando() == 1);
    randoSmokeCheck(kName, "set_active: listener called with 1", sNotifyCount == 1 && sLastNotify == 1);
    randoSmokeCheck(kName, "set_active: config copied", pc_rando_cfg()->sword_mode == PC_RANDO_SWORD_SWORDLESS);
    randoSmokeCheck(kName, "set_active: loc_item reads the placement",
                    pc_rando_loc_item(1, 0x42) == 0x51 && pc_rando_loc_item(3, 0x42) == 0x42 &&
                        pc_rando_loc_item(-1, 0x42) == 0x42);

    pc_rando_deactivate();
    randoSmokeCheck(kName, "deactivate: pc_rando() back", pc_rando() == forced);
    randoSmokeCheck(kName, "deactivate: listener called", sNotifyCount == 2 && sLastNotify == forced);
    randoSmokeCheck(kName, "deactivate: default config", defaultConfig());
    randoSmokeCheck(kName, "deactivate: placement dropped", pc_rando_loc_item(1, 0x42) == 0x42);
    sEndFrame = frames + 30;
}

} // namespace pc
