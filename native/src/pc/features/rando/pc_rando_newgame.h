/*
 * pc_rando_newgame.h - the new-game state of the built-in randomizer (docs/RANDOMIZER_PLAN.md step
 * R3.1, asm report group G1 core).
 *
 * wwrando replaces the call to dSv_save_c::init in dSv_info_c::init (misc_rando_features.asm
 * 0x8005D618) with init_save_with_tweaks (custom_funcs.asm:5-320): the plain init, then the state
 * every randomized file starts with - the sword by sword_mode, the Sail, the Wind Waker, Wind's
 * Requiem and the Pirate's Charm, 30 arrows and bombs, the starting magic, the starting gear (through
 * convert_progressive_item_id and execItemGet), 44 story event bits, the prologue pigs register, the
 * targeting mode, switch words of eight stages, the boss-intro dungeon bit of stages 3-7, the spoils
 * and bait "ever owned" bytes, the Triforce Shards and the boss rematch bits. reveal_sea_chart.asm
 * (0x8005B2CC, in dSv_player_map_c::init) charts every sector of the new save.
 *
 * This file and pc_rando_newgame.cpp hold the pure part (no game header, no game call; they link into
 * cos_rando_core and the unit tests): the tables of the asm and the plan a config gives. The game side
 * (pc_rando_new_game_init, which applies the plan to the save right after dComIfGs_init) is
 * native/src/pc/game_hooks/pc_rando_hooks_newgame.cpp, declared in pc/pc_rando_hooks.h.
 */
#ifndef PC_RANDO_NEWGAME_H
#define PC_RANDO_NEWGAME_H

#include "pc/pc_rando.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The 38 story event bits init_save_with_tweaks always sets, in the asm's order (HAS_SEEN_INTRO
   0x3510 .. 0x4002, saw the cutscene before the Ganondorf fight). The asm has 46 onEventBit calls in
   all (the plan's "44 event bits" are the 46 without the two swordless ones): these 38, the two
   swordless barrier bits, the mTmp bit 0x0310, 0x3D04 and the four rematch bits. */
#define PCR_NEWGAME_STORY_BITS 38
extern const uint16_t pcr_newgame_story_bits[PCR_NEWGAME_STORY_BITS];

/* A switch of a stage's save memory (dSv_save_c::mMemory[stage].mBit.onSwitch(sw)). */
typedef struct PcrNewgameSwitch {
    uint8_t stage; /* dSv_save_c::SaveStageTbl */
    uint8_t sw;    /* 0..127 */
} PcrNewgameSwitch;
/* The switches whose words init_save_with_tweaks stores (Outset, sea, DRC, FW, TotG, ET, Hyrule,
   Ganon's Tower), as single switches, in the asm's order. The asm stores whole u32 words over the zeroes dSv_save_c::init
   left; setting these switches gives the same words (pc_rando_newgame_switch_word). */
#define PCR_NEWGAME_SWITCHES 31
extern const PcrNewgameSwitch pcr_newgame_switches[PCR_NEWGAME_SWITCHES];
/* mSwitch[word] of `stage` (word 0..3, the asm's offsets 4/8/0xC/0x10 of the stage info) after the
   switches above are set on a cleared stage: the u32 the asm stores, 0 where it stores nothing. */
uint32_t pc_rando_newgame_switch_word(int stage, int word);

/* generic_on_dungeon_bit(stage, 5) for stages 3-7 (DRC, FW, TotG, ET, WT): "seen the boss intro"
   (dSv_memBit_c::STAGE_BOSS_DEMO). */
#define PCR_NEWGAME_BOSS_DEMO_BIT 5
#define PCR_NEWGAME_BOSS_DEMO_FIRST_STAGE 3
#define PCR_NEWGAME_BOSS_DEMO_LAST_STAGE 7

/* Fixed values of init_save_with_tweaks. */
#define PCR_NEWGAME_ARROWS_BOMBS 30        /* current and max arrows and bombs */
#define PCR_NEWGAME_PIGS_REG 0xBFFF        /* setEventReg: which pigs were brought to Rose */
#define PCR_NEWGAME_EVER_OWNED 0xFF        /* spoils (mBeastFlags) and bait (mBaitFlags) ever owned */
#define PCR_NEWGAME_TRIFORCE_REFUSE 0x3D04 /* saw the Triforce refuse: with 8 starting shards */
#define PCR_NEWGAME_BARRIER_DOWN 0x2C02    /* swordless: the Hyrule barrier is down */
#define PCR_NEWGAME_BARRIER_DOWN2 0x3B08   /* swordless: the other bit the barrier sets */
/* The bit init_save_with_tweaks sets in dSv_info_c::mTmp (0x803C5D60): "saw Grandma give the Hero's
   Clothes". dSv_info_c::init runs mTmp.init() right after the replaced dSv_save_c::init call, which
   clears it again, so in wwrando this write has no effect; the port does not repeat it. */
#define PCR_NEWGAME_TMP_BIT_CLEARED 0x0310
/* The four rematch bosses (skip_rematch_bosses): Gohma, Kalle Demos, Jalhalla, Molgera. */
#define PCR_NEWGAME_REMATCH_BITS 4
extern const uint16_t pcr_newgame_rematch_bits[PCR_NEWGAME_REMATCH_BITS];

/* What init_save_with_tweaks does with a given config, in the asm's order. The game side runs it as:
   reveal chart (part of dSv_save_c::init in wwrando), give_sword or barrier_bits, the four fixed
   item functions, arrows and bombs, magic, the gear loop, then the save writes. */
typedef struct PcRandoNewGamePlan {
    uint8_t give_sword;       /* sword_mode START: item_func_sword */
    uint8_t n_barrier_bits;   /* sword_mode SWORDLESS: 2 (0x2C02, 0x3B08), else 0 */
    uint16_t barrier_bits[2];
    uint8_t magic;            /* max and current magic (starting_magic) */
    uint8_t n_gear;           /* starting_gear up to its 0xFF end marker */
    uint8_t gear[PC_RANDO_MAX_STARTING_GEAR];
    uint8_t pigs;             /* setEventReg(0xBFFF, pigs) */
    uint8_t targeting_mode;   /* dSv_player_config_c::mAttentionType */
    uint8_t triforce;         /* mCollect.mTriforce: (1 << shards) - 1 */
    uint8_t n_end_bits;       /* 0x3D04 with 8 shards, then the rematch bits */
    uint16_t end_bits[1 + PCR_NEWGAME_REMATCH_BITS];
    uint8_t reveal_chart;     /* every sector's charted bit (mFmapBits bit 0) */
} PcRandoNewGamePlan;

/* Fills *plan from *cfg (NULL: the defaults of pc_rando_config_defaults). */
void pc_rando_newgame_plan(const PcRandoConfig* cfg, PcRandoNewGamePlan* plan);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_NEWGAME_H */
