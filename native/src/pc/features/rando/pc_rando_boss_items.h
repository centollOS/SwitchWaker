/*
 * pc_rando_boss_items.h - the 6 boss Heart Container locations (docs/RANDOMIZER_PLAN.md, step R2.5).
 *
 * wwrando (flexible_item_locations.asm) makes fopAcM_createItemForBoss spawn the item passed in r4
 * and patches each boss's call site and Bitem actor with its location's item. The port keeps one
 * lookup instead: the boss location is keyed by the dungeon's stage number (dSv_save_c::STAGE_FF..
 * STAGE_WT, 2-7: wwrando's BOSS_NAME_TO_STAGE_ID), which daBossItem's params hold and which is the
 * loaded stage's save table for the boss rooms. The game side (which stage is loaded) is
 * pc_rando_boss_item in game_hooks/pc_rando_hooks_boss.cpp.
 *
 * Pure: no game headers (cos_rando_core).
 */
#ifndef PC_RANDO_BOSS_ITEMS_H
#define PC_RANDO_BOSS_ITEMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The vanilla boss drop: dItemNo_HEART_CONTAINER_e. */
#define PC_RANDO_BOSS_VANILLA_ITEM 0x08

/* PCR_LOC_* of the Heart Container of the boss of stage `stage` (2 Helmaroc King .. 7 Molgera), or -1
   for any other stage. */
int pc_rando_boss_loc(int stage);
/* The item placed at that location (pc_rando_loc_item); PC_RANDO_BOSS_VANILLA_ITEM when pc_rando() is
   0, no placement is set, or `stage` is not a boss stage. */
uint8_t pc_rando_boss_item_for_stage(int stage);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_BOSS_ITEMS_H */
