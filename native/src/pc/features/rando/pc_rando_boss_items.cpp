// pc_rando_boss_items.cpp - stage number -> boss Heart Container location (step R2.5; see the header).
#include "pc_rando_boss_items.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"

namespace {

// Indexed by stage number - 2 (dSv_save_c::STAGE_FF .. STAGE_WT; boss_reqs.py BOSS_NAME_TO_STAGE_ID).
constexpr int kBossLocs[6] = {
    PCR_LOC_FORSAKEN_FORTRESS_HELMAROC_KING_HEART_CONTAINER, // 2 Helmaroc King
    PCR_LOC_DRAGON_ROOST_CAVERN_GOHMA_HEART_CONTAINER,       // 3 Gohma
    PCR_LOC_FORBIDDEN_WOODS_KALLE_DEMOS_HEART_CONTAINER,     // 4 Kalle Demos
    PCR_LOC_TOWER_OF_THE_GODS_GOHDAN_HEART_CONTAINER,        // 5 Gohdan
    PCR_LOC_EARTH_TEMPLE_JALHALLA_HEART_CONTAINER,           // 6 Jalhalla
    PCR_LOC_WIND_TEMPLE_MOLGERA_HEART_CONTAINER,             // 7 Molgera
};

} // namespace

extern "C" int pc_rando_boss_loc(int stage) {
    if (stage < 2 || stage > 7) {
        return -1;
    }
    return kBossLocs[stage - 2];
}

extern "C" uint8_t pc_rando_boss_item_for_stage(int stage) {
    const int loc = pc_rando_boss_loc(stage);
    if (loc < 0) {
        return PC_RANDO_BOSS_VANILLA_ITEM;
    }
    return pc_rando_loc_item(loc, PC_RANDO_BOSS_VANILLA_ITEM);
}
