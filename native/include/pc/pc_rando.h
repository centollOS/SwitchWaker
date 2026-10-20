/*
 * pc_rando.h - the built-in randomizer's public C API (docs/RANDOMIZER_PLAN.md, step R0.1).
 *
 * pc_rando() is the one switch every randomizer hunk in game/ tests. It is 0 for a file that is not
 * randomized, and then every hunk takes its original branch: such a file plays exactly as the
 * original game (plan "Goal and scope"). It becomes 1 when a randomized slot is loaded, at New Game
 * with a pending seed, or in a debug boot with a placement (D7: pc_rando_set_active), and 0 again at
 * the title and at file select (pc_rando_deactivate).
 *
 * pc_rando_cfg() holds the seed-time values that wwrando's custom_funcs.asm keeps as data in main.dol
 * (sword_mode, starting_gear, ...) plus the tweak options of D14. While no seed is active it holds the
 * defaults of wwrando 1.10.0_9775811's default preset (pc_rando_config_defaults).
 *
 * Dev override, read once from the environment:
 *   COS_RANDO_FORCE=1   pc_rando() is 1 from the start with the default config and no placement, so
 *                       a step can test its code-only hunks before randomized slots exist
 *                       (pc_rando_loc_item then still returns the vanilla item). It survives
 *                       pc_rando_deactivate.
 *
 * Game thread only (the menu's seed generation runs the pure generator on a worker and hands the
 * result back to the game thread).
 */
#ifndef PC_RANDO_H
#define PC_RANDO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The seed file (placement, options, hashes), defined by R0.6 in features/rando/pc_rando_seedfile.h.
   Opaque here. */
struct PcRandoSeed;

/* wwrando's sword_mode byte (custom_funcs.asm). */
#define PC_RANDO_SWORD_START 0     /* Start with Hero's Sword (default) */
#define PC_RANDO_SWORD_NO_START 1  /* No Starting Sword */
#define PC_RANDO_SWORD_SWORDLESS 2 /* Swordless */

/* custom_funcs.asm starting_gear: .space 86 plus the 0xFF end marker (tweaks.py
   MAXIMUM_ADDITIONAL_STARTING_ITEMS). */
#define PC_RANDO_MAX_STARTING_GEAR 86
#define PC_RANDO_ITEM_END 0xFF

typedef struct PcRandoConfig {
    /* custom_funcs.asm data (defaults in parentheses: wwrando's default preset) */
    uint8_t sword_mode;                /* PC_RANDO_SWORD_* (START) */
    uint8_t skip_rematch_bosses;       /* 1 */
    uint8_t starting_gear[PC_RANDO_MAX_STARTING_GEAR + 1]; /* item ids, PC_RANDO_ITEM_END-terminated
                                          (Ballad of Gales 0x6E, Progressive Shield 0x3B, Song of
                                          Passing 0x72: sorted DEFAULT_STARTING_ITEMS without the
                                          magic meter, which is starting_magic) */
    uint16_t starting_quarter_hearts;  /* starting_hcs * 4 + starting_pohs (12) */
    uint8_t starting_magic;            /* 16 per Progressive Magic Meter (16) */
    uint8_t num_triforce_shards;       /* num_starting_triforce_shards (0) */
    uint8_t heros_clothes;             /* should_start_with_heros_clothes (1) */
    uint8_t pigs_bitfield;             /* captured_prologue_pigs_bitfield (0x04, black pig) */
    uint8_t targeting_mode;            /* option_targeting_mode: 0 hold, 1 switch (0) */
    float damage_multiplier;           /* hero_mode.asm damage_multiplier: 1.0, 4.0 in hero mode */
    /* D14: the option-backed tweaks follow the seed's option under rando */
    uint8_t swift_sail;                /* 1 */
    uint8_t instant_text_boxes;        /* 1 */
    uint8_t reveal_full_sea_chart;     /* 1 */
} PcRandoConfig;

/* 1 while a randomized file is active (or COS_RANDO_FORCE=1), else 0. */
int pc_rando(void);
/* The active seed's config; the defaults while none is active. Never NULL. */
const PcRandoConfig* pc_rando_cfg(void);
/* Fills *cfg with wwrando's default-preset values. */
void pc_rando_config_defaults(PcRandoConfig* cfg);

/* The item placed at location `loc` (PCR_LOC_*, R0.5), or `vanilla` when pc_rando() is 0, no
   placement is set (COS_RANDO_FORCE) or loc is out of range. */
uint8_t pc_rando_loc_item(int loc, uint8_t vanilla);

/* Activation (D7). `cfg` NULL means the defaults; `seed` may be NULL (debug boots, tests). The config
   is copied; the seed must stay alive until pc_rando_deactivate. Listeners run after the change. */
void pc_rando_set_active(const struct PcRandoSeed* seed, const PcRandoConfig* cfg);
void pc_rando_deactivate(void);
/* The active seed (NULL when none, or when activated without one). */
const struct PcRandoSeed* pc_rando_seed(void);
/* The placement pc_rando_loc_item reads: `count` item ids indexed by location; NULL clears it. Not
   copied: the caller (the seed's owner, R0.7) keeps it alive while active. Set it before
   pc_rando_set_active, so listeners see it; pc_rando_deactivate clears it. */
void pc_rando_set_loc_items(const uint8_t* items, int count);

/* 1 when COS_RANDO_FORCE=1 forces pc_rando() on. */
int pc_rando_forced(void);

/* Called after every activation change with the new pc_rando() value (e.g. R0.8 snapshots and
   restores item tables). Returns 0 when the list (16 entries) is full. A listener is added once
   however often it is passed; one added while pc_rando() is already 1 (COS_RANDO_FORCE) is called
   with 1 at once. */
typedef void (*PcRandoListener)(int active);
int pc_rando_add_listener(PcRandoListener fn);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_H */
