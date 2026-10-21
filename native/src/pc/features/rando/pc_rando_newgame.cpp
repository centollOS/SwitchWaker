// The pure part of the randomizer's new-game state (docs/RANDOMIZER_PLAN.md step R3.1;
// pc_rando_newgame.h). Tables transcribed from wwrando's custom_funcs.asm init_save_with_tweaks
// (lines 5-320); the game side is game_hooks/pc_rando_hooks_newgame.cpp.
#include "pc_rando_newgame.h"

#include <cstring>

extern "C" {

const uint16_t pcr_newgame_story_bits[PCR_NEWGAME_STORY_BITS] = {
    0x3510, // HAS_SEEN_INTRO
    0x2A80, // HAS_HEROS_CLOTHES (set even with casual clothes)
    0x0280, // SAW_TETRA_IN_FOREST_OF_FAIRIES
    0x0520, // GOSSIP_STONE_AT_FF1 (Aryll and the pirates leave Outset)
    0x2E01, // WATCHED_MEETING_KORL_CUTSCENE (Windfall music when warping there)
    0x0F80, // KORL_UNLOCKED_AND_SPAWN_ON_WINDFALL
    0x0908, // SAIL_INTRODUCTION_TEXT_AND_MAP_UNLOCKED
    0x2A08, // ENTER_KORL_FOR_THE_FIRST_TIME_AND_SPAWN_ANYWHERE
    0x0902, // SAW_DRAGON_ROOST_ISLAND_INTRO
    0x1F40, // SAW_QUILL_CUTSCENE_ON_DRI
    0x0A80, // KORL_DINS_PEARL_TEXT_ALLOWING_YOU_TO_ENTER_HIM
    0x0901, // TRIGGERED_MAP_FISH
    0x0A20, // WATCHED_FOREST_HAVEN_INTRO_CUTSCENE
    0x1801, // WATCHED_DEKU_TREE_CUTSCENE
    0x0A08, // TALKED_TO_KORL_AFTER_LEAVING_FH
    0x0808, // exiting the pirate ship goes to Windfall, not the tutorial
    0x1F02, // TALKED_TO_KORL_AFTER_GETTING_BOMBS
    0x2F20, // talked to KoRL after getting Nayru's Pearl
    0x3840, // TALKED_TO_KORL_POST_TOWER_CUTSCENE
    0x2D04, // MASTER_SWORD_CUTSCENE
    0x3802, // COLORS_IN_HYRULE
    0x2D01, // ANIMATION_SET_2 (Aryll rescued before Helmaroc King)
    0x2D02, // TETRA_TO_ZELDA_CUTSCENE
    0x3201, // KoRL told you about the sages
    0x3380, // KoRL told you about the Triforce shards
    0x1001, // WATCHED_FIRE_AND_ICE_ARROWS_CUTSCENE
    0x2E04, // MEDLI_IN_EARTH_TEMPLE_ENTRANCE
    0x2920, // MEDLI_IN_EARTH_TEMPLE
    0x1620, // Medli is in dungeon mode and can be lifted/called
    0x3304, // saw Medli call from within jail
    0x2910, // MAKAR_IN_WIND_TEMPLE
    0x1610, // Makar is in dungeon mode and can be lifted/called
    0x3440, // saw Makar call from within jail
    0x3A20, // Fishman and KoRL talked about Forsaken Fortress after Molgera
    0x2D08, // HYRULE_3_WARP_CUTSCENE
    0x3980, // HYRULE_3_ELECTRICAL_BARRIER_CUTSCENE_1
    0x3B02, // saw the cutscene before the Puppet Ganon fight
    0x4002, // saw the cutscene before the Ganondorf fight
};

// dSv_save_c::SaveStageTbl: 0 sea, 3 DRC, 4 FW, 5 TotG, 6 ET, 8 GT, 9 Hyrule, 11 misc (Outset).
const PcrNewgameSwitch pcr_newgame_switches[PCR_NEWGAME_SWITCHES] = {
    // Outset (0x803C5114): word +4 = 0x4008038B, +8 = 0xC0000000
    {11, 0x00}, {11, 0x01}, {11, 0x03}, {11, 0x07}, // Fairy Woods Tetra events
    {11, 0x08},                                     // pond unclogged
    {11, 0x09},                                     // Rito Aerie entry, Delivery Bag
    {11, 0x13},                                     // camera pan to the Windfall jail chest
    {11, 0x1E},                                     // Forest Haven interior intro
    {11, 0x3E}, {11, 0x3F},                         // ET and WT entrance song tablets unlocked
    // sea (0x803C4F88): +4 = 0x02000000, +8 = 0x80000000, +0xC = 0x41010080
    {0, 0x19},  // Greatfish Isle: the endless night never starts
    {0, 0x3F},  // Windfall intro
    {0, 0x47},  // Forest Haven switch to the Nintendo Gallery hatch
    {0, 0x50},  // Flight Control Platform camera pan
    {0, 0x58},  // first entry to Forsaken Fortress 2
    {0, 0x5E},  // Nintendo Gallery ladder lowered
    // DRC (0x803C4FF4): +4 = 0x0200, +8 = 0x0002, +0xC = 0x0040
    {3, 0x09},  // camera pan up to Valoo
    {3, 0x21},  // KoRL's rat-bait gossip stone
    {3, 0x46},  // first-entry camera pan
    // FW (0x803C5018): +8 = 0x00400000
    {4, 0x36},  // first room camera pan
    // TotG (0x803C503C): +8 = 0x2000, +0x10 = 0x0008
    {5, 0x2D},  // camera pan outside at the top
    {5, 0x63},  // first room camera pan
    // ET (0x803C5060): +8 = 0x0400
    {6, 0x2A},  // KoRL's gossip stone about Medli on the compass
    // Hyrule (0x803C50CC): +4 = 0x00040040
    {9, 0x06},  // Triforce pushable block puzzle done
    {9, 0x12},  // first-entry camera pan
    // Ganon's Tower (0x803C50A8): +4 = 0xF0042000
    {8, 0x0D},  // first-entry camera pan
    {8, 0x12},  // after the Puppet Ganon fight
    {8, 0x1C},  // the 4 lights room
    {8, 0x1D},  // the 4 boomerang switches room
    {8, 0x1E},  // before the Puppet Ganon fight
    {8, 0x1F},  // before the Ganondorf fight
};

const uint16_t pcr_newgame_rematch_bits[PCR_NEWGAME_REMATCH_BITS] = {
    0x3904, // Recollection Gohma defeated
    0x3902, // Recollection Kalle Demos defeated
    0x3901, // Recollection Jalhalla defeated
    0x3A80, // Recollection Molgera defeated
};

uint32_t pc_rando_newgame_switch_word(int stage, int word) {
    uint32_t w = 0;
    for (const PcrNewgameSwitch& s : pcr_newgame_switches) {
        if (s.stage == stage && (s.sw >> 5) == word) {
            w |= 1u << (s.sw & 0x1F);
        }
    }
    return w;
}

void pc_rando_newgame_plan(const PcRandoConfig* cfg, PcRandoNewGamePlan* plan) {
    PcRandoConfig defaults;
    if (cfg == nullptr) {
        pc_rando_config_defaults(&defaults);
        cfg = &defaults;
    }
    memset(plan, 0, sizeof *plan);
    // sword_mode: 0 start with sword, 2 swordless (barrier down), anything else nothing.
    if (cfg->sword_mode == PC_RANDO_SWORD_START) {
        plan->give_sword = 1;
    } else if (cfg->sword_mode == PC_RANDO_SWORD_SWORDLESS) {
        plan->barrier_bits[0] = PCR_NEWGAME_BARRIER_DOWN;
        plan->barrier_bits[1] = PCR_NEWGAME_BARRIER_DOWN2;
        plan->n_barrier_bits = 2;
    }
    plan->magic = cfg->starting_magic;
    // init_starting_gear: up to the 0xFF end marker (the array always holds one at index 86).
    for (int i = 0; i < PC_RANDO_MAX_STARTING_GEAR && cfg->starting_gear[i] != PC_RANDO_ITEM_END; i++) {
        plan->gear[plan->n_gear++] = cfg->starting_gear[i];
    }
    plan->pigs = cfg->pigs_bitfield;
    plan->targeting_mode = cfg->targeting_mode;
    // li r0, 1; slw r4, r0, r5; subi r4, r4, 1; stb: the low byte of (1 << n) - 1 (slw gives 0 for
    // shift amounts 32-63).
    const unsigned n = cfg->num_triforce_shards;
    plan->triforce = (uint8_t)((n < 32 ? (1u << n) : 0u) - 1u);
    // cmpwi r5, 8; blt: 0x3D04 from 8 shards up.
    if (n >= 8) {
        plan->end_bits[plan->n_end_bits++] = PCR_NEWGAME_TRIFORCE_REFUSE;
    }
    // cmpwi r5, 1; bne: only the value 1 skips the rematches.
    if (cfg->skip_rematch_bosses == 1) {
        for (uint16_t bit : pcr_newgame_rematch_bits) {
            plan->end_bits[plan->n_end_bits++] = bit;
        }
    }
    plan->reveal_chart = cfg->reveal_full_sea_chart != 0 ? 1 : 0;
}

} // extern "C"
