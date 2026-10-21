// cos_rando_test_R2_5 (docs/RANDOMIZER_PLAN.md step R2.5): the boss Heart Container table
// (features/rando/pc_rando_boss_items.h).
//  1. each of wwrando's 6 bosses (boss_reqs.py BOSS_NAME_TO_STAGE_ID, R0.3's pcr::bosses()) maps by
//     its stage number to the location "<dungeon> - <boss> Heart Container", whose keys (R0.5) hold
//     the vanilla Heart Container and include its Bitem actor with that stage number;
//  2. other stage numbers (sea, sea2, 8+, negative) have no boss location;
//  3. pc_rando_boss_item_for_stage gives 0x08 while pc_rando() is 0 and the placed item once a
//     placement is active, and 0x08 for a non-boss stage either way.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_boss_items.h"
#include "pc_rando_data.h"
#include "pc_rando_lockeys.h"

#include <cstring>
#include <string>

using namespace pcr;

int main() {
    // 1. the six bosses
    RT_CHECK_EQ(bosses().size, (size_t)6);
    for (const Boss& b : bosses()) {
        const int loc = pc_rando_boss_loc(b.stage_id);
        RT_CHECK(loc >= 0 && loc < PCR_LOC_COUNT);
        if (loc < 0 || loc >= (int)locations().size) {
            continue;
        }
        const std::string name = locations()[loc].name;
        const std::string tail = std::string(" - ") + b.name + " Heart Container";
        RT_CHECK(name.size() > tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0);
        RT_CHECK(std::strcmp(locations()[loc].original_item, "Heart Container") == 0);
        bool bitem = false;
        for (const LocKey& k : location_keys(loc)) {
            RT_CHECK_EQ(k.vanilla, PC_RANDO_BOSS_VANILLA_ITEM);
            if (k.kind == KeyKind::Dzx && k.field == DzxField::BossItem) {
                RT_CHECK(std::memcmp(k.fp_name, "Bitem", 6) == 0);
                bitem = true;
            }
        }
        RT_CHECK(bitem);
        // The six locations are distinct.
        for (const Boss& o : bosses()) {
            if (&o != &b) {
                RT_CHECK(pc_rando_boss_loc(o.stage_id) != loc);
            }
        }
    }

    // 2. no boss
    const int none[] = {-1, 0, 1, 8, 9, 13, 0xFF};
    for (int s : none) {
        RT_CHECK_EQ(pc_rando_boss_loc(s), -1);
    }

    // 3. the item
    static uint8_t placement[PCR_LOC_COUNT];
    std::memset(placement, 0x01, sizeof placement);
    for (int s = 2; s <= 7; s++) {
        placement[pc_rando_boss_loc(s)] = (uint8_t)(0x30 + s);
    }
    RT_CHECK_EQ(pc_rando(), 0);
    for (int s = 2; s <= 7; s++) {
        RT_CHECK_EQ(pc_rando_boss_item_for_stage(s), 0x08);
    }
    pc_rando_set_loc_items(placement, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);
    for (int s = 2; s <= 7; s++) {
        RT_CHECK_EQ(pc_rando_boss_item_for_stage(s), 0x30 + s);
    }
    RT_CHECK_EQ(pc_rando_boss_item_for_stage(0), 0x08);
    RT_CHECK_EQ(pc_rando_boss_item_for_stage(8), 0x08);
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando(), 0);
    RT_CHECK_EQ(pc_rando_boss_item_for_stage(3), 0x08);

    return rt::finish();
}
