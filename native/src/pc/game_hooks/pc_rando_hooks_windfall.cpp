// The game side of the randomizer's Windfall NPC code locations (docs/RANDOMIZER_PLAN.md step R2.7,
// docs/RANDO_HOOKS.md "## R2.7"): the townspeople's two item tables in d_a_npc_people
// (l_get_item_no, l_item_id_sa3) mapped to their locations, and the event bits wwrando's
// flexible_item_locations.asm sets when Pompie and Vera, Minenco and Kamo give their item
// (create_item_and_set_event_bit_for_townsperson). The other R2.7 sites (Lenzo, Maggie, Salvage Corp,
// the Rito postman) call pc_rando_loc_item and dComIfGs_onEventBit directly in their hunks.
// Only called under pc_rando().
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "d/d_com_inf_game.h"

namespace {

// l_get_item_no (d_a_npc_people.cpp): index -> location, and the event bit wwrando stores in the
// upper halfword of the table word (d_a_npc_people.rel 0xC54C / 0xC550 / 0xC55C).
struct PeopleGift {
    int loc;
    uint16_t bit;
};
constexpr PeopleGift kPeople[] = {
    {PCR_LOC_WINDFALL_ISLAND_SAM_DECORATE_THE_TOWN, 0},
    {PCR_LOC_WINDFALL_ISLAND_POMPIE_AND_VERA_SECRET_MEETING_PHOTO, PC_RANDO_BIT_POMPIE_VERA},
    {PCR_LOC_WINDFALL_ISLAND_MINENCO_MISS_WINDFALL_PHOTO, PC_RANDO_BIT_MINENCO},
    {PCR_LOC_WINDFALL_ISLAND_LINDA_AND_ANTON, 0},
    {PCR_LOC_WINDFALL_ISLAND_KREEB_LIGHT_UP_LIGHTHOUSE, 0},
    {PCR_LOC_WINDFALL_ISLAND_KAMO_FULL_MOON_PHOTO, PC_RANDO_BIT_KAMO},
};
constexpr int kPeopleCount = sizeof kPeople / sizeof kPeople[0];

// l_item_id_sa3 (d_a_npc_people.cpp): Kane's four gate rewards.
constexpr int kKane[] = {
    PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SHOP_GURU_STATUE_ON_GATE,
    PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_POSTMAN_STATUE_ON_GATE,
    PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_FLAGS_ON_GATE,
    PCR_LOC_WINDFALL_ISLAND_KANE_PLACE_SIX_IDOLS_ON_GATE,
};
constexpr int kKaneCount = sizeof kKane / sizeof kKane[0];

} // namespace

extern "C" int pc_rando_people_item(int idx, int vanilla) {
    if (idx < 0 || idx >= kPeopleCount) {
        return vanilla;
    }
    return pc_rando_loc_item(kPeople[idx].loc, (uint8_t)vanilla);
}

extern "C" uint16_t pc_rando_people_item_bit(int idx) {
    return idx >= 0 && idx < kPeopleCount ? kPeople[idx].bit : 0;
}

extern "C" void pc_rando_people_item_given(int idx) {
    const uint16_t bit = pc_rando_people_item_bit(idx);
    if (bit != 0) {
        dComIfGs_onEventBit(bit);
    }
}

extern "C" int pc_rando_kane_item(int idx, int vanilla) {
    if (idx < 0 || idx >= kKaneCount) {
        return vanilla;
    }
    return pc_rando_loc_item(kKane[idx], (uint8_t)vanilla);
}
