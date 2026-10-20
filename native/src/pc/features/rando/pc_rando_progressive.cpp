// Progressive items of the built-in randomizer, pure part (docs/RANDOMIZER_PLAN.md step R3.3; see
// pc_rando_progressive.h). A port of custom_funcs.asm convert_progressive_item_id and of the tier
// choice of the 8 progressive_*_item_func; the game side is game_hooks/pc_rando_hooks_progressive.cpp.
// No game header or game call here: cos_rando_core links this file into the unit tests.
#include "pc_rando_progressive.h"
#include "pc_rando_msg.h"

namespace {

struct Family {
    uint8_t ids[5];   // the ids whose item function is the family's (tweaks.py order)
    int nids;
    uint8_t states[4]; // owned-state values the asm compares against, in order
    uint8_t tiers[4];  // the id of the next tier for each state
    int ntiers;
};

// custom_funcs.asm convert_progressive_*_id / progressive_*_item_func. The base id (fallback of the
// conversion) is tiers[0] for every family.
const Family kFamilies[PCR_PROG_COUNT] = {
    {{0x38, 0x39, 0x3A, 0x3D, 0x3E}, 5, {0, 1, 3, 7}, {0x38, 0x39, 0x3A, 0x3E}, 4}, // sword
    {{0x3B, 0x3C}, 2, {0, 1}, {0x3B, 0x3C}, 2},                                     // shield
    {{0x27, 0x35, 0x36}, 3, {0, 1, 3}, {0x27, 0x35, 0x36}, 3},                      // bow
    {{0xAB, 0xAC}, 2, {0, 1}, {0xAB, 0xAC}, 2},                                     // wallet
    {{0xAD, 0xAE}, 2, {30, 60}, {0xAD, 0xAE}, 2},                                   // bomb bag
    {{0xAF, 0xB0}, 2, {30, 60}, {0xAF, 0xB0}, 2},                                   // quiver
    {{0x23, 0x26}, 2, {0, 1}, {0x23, 0x26}, 2},                                     // picto box
    {{0xB1, 0xB2}, 2, {0, 16}, {0xB1, 0xB2}, 2},                                    // magic meter
};

uint8_t stateOf(int family, const PcRandoProgState* st) {
    switch (family) {
    case PCR_PROG_SWORD:
        return st->swords;
    case PCR_PROG_SHIELD:
        return st->shields;
    case PCR_PROG_BOW:
        return st->bow;
    case PCR_PROG_WALLET:
        return st->wallet;
    case PCR_PROG_BOMB_BAG:
        return st->bomb_max;
    case PCR_PROG_QUIVER:
        return st->arrow_max;
    case PCR_PROG_PICTO_BOX:
        return st->picto;
    case PCR_PROG_MAGIC_METER:
    default:
        return st->max_magic;
    }
}

} // namespace

extern "C" {

int pc_rando_progressive_family(uint8_t id) {
    for (int f = 0; f < PCR_PROG_COUNT; f++) {
        for (int i = 0; i < kFamilies[f].nids; i++) {
            if (kFamilies[f].ids[i] == id) {
                return f;
            }
        }
    }
    return -1;
}

uint8_t pc_rando_progressive_id(int i) {
    for (int f = 0; f < PCR_PROG_COUNT && i >= 0; f++) {
        if (i < kFamilies[f].nids) {
            return kFamilies[f].ids[i];
        }
        i -= kFamilies[f].nids;
    }
    return 0xFF;
}

int pc_rando_progressive_tier(int family, const PcRandoProgState* st) {
    if (family < 0 || family >= PCR_PROG_COUNT || st == nullptr) {
        return -1;
    }
    const Family& fam = kFamilies[family];
    const uint8_t s = stateOf(family, st);
    for (int t = 0; t < fam.ntiers; t++) {
        if (fam.states[t] == s) {
            return t;
        }
    }
    return -1;
}

uint8_t pc_rando_progressive_convert(uint8_t id, const PcRandoProgState* st) {
    const int family = pc_rando_progressive_family(id);
    if (family < 0 || st == nullptr) {
        return id;
    }
    const int tier = pc_rando_progressive_tier(family, st);
    // "Invalid state; this shouldn't happen so just return the base id" (custom_funcs.asm).
    return kFamilies[family].tiers[tier < 0 ? 0 : tier];
}

int pc_rando_progressive_tier_count(int family) {
    return family >= 0 && family < PCR_PROG_COUNT ? kFamilies[family].ntiers : 0;
}

uint8_t pc_rando_progressive_tier_id(int family, int tier) {
    if (family < 0 || family >= PCR_PROG_COUNT || tier < 0 || tier >= kFamilies[family].ntiers) {
        return 0xFF;
    }
    return kFamilies[family].tiers[tier];
}

int pc_rando_progressive_add_messages(void) {
    PcRandoMsgEntry e;
    pc_rando_msg_entry_defaults(&e);
    e.text_box_type = 9;     // TextBoxType.ITEM_GET
    e.initial_draw_type = 2; // slow initial message speed
    e.display_item_id = 0xB1;
    return pc_rando_msg_add(PCR_PROG_MAGIC_MSG_NO, &e, PCR_PROG_MAGIC_MSG_TEXT);
}

} // extern "C"
