/*
 * pc_rando_tingle.h - the Tingle Statues of the built-in randomizer (docs/RANDOMIZER_PLAN.md step
 * R3.6, asm report group G4).
 *
 * In the original game the five Tingle Statues (items 0xA3-0xA7) have empty item functions: owning
 * one means having opened its dungeon's chest, opened flag 0xF of stage 3-7. The Tingle Island
 * statues (daObjVtil), Tingle and his brothers (daNpc_Tc), the Tingle Tuner (daAgb_c::FlagsSend) and
 * the tuner regions (daAgbsw0_c::MoveCheck) all test dComIfGs_isStageTbox(stage, 0xF). wwrando
 * randomizes the statues, so it gives each one an item function that sets an unused event bit
 * (custom_funcs.asm *_tingle_statue_item_get_func) and makes those sites call
 * check_tingle_statue_owned(stage, flag) instead (flexible_item_locations.asm:852-933). The quest
 * screen's treasure-chart counter shows the number of owned statues
 * (misc_rando_features.asm get_num_owned_tingle_statues, tweaks.py
 * show_number_of_tingle_statues_on_quest_status_screen).
 *
 * This file and pc_rando_tingle.cpp hold the pure part (no game header, no game call; they link into
 * cos_rando_core and the unit tests): the item/stage to event-bit tables and the quest-screen texts.
 * The game side (item functions, the owned check, their registration) is
 * native/src/pc/game_hooks/pc_rando_hooks_tingle.cpp; the hooks are declared in pc/pc_rando_hooks.h.
 *
 *   item  statue     stage (dSv_save_c)  event bit (plan D17)
 *   0xA3  Dragon     3 STAGE_DRC         0x6A04
 *   0xA4  Forbidden  4 STAGE_FW          0x6A08
 *   0xA5  Goddess    5 STAGE_TOTG        0x6A10
 *   0xA6  Earth      6 STAGE_ET          0x6A20
 *   0xA7  Wind       7 STAGE_WT          0x6A40
 */
#ifndef PC_RANDO_TINGLE_H
#define PC_RANDO_TINGLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCR_TINGLE_STATUE_COUNT 5
#define PCR_TINGLE_FIRST_ITEM 0xA3 /* dItemNo_TINGLE_STATUE_1_e */
#define PCR_TINGLE_FIRST_STAGE 3   /* dSv_save_c::STAGE_DRC */
#define PCR_TINGLE_CHEST_FLAG 0xF  /* the opened flag of the vanilla statue chests */

/* Quest status screen (tweaks.py show_number_of_tingle_statues_on_quest_status_screen). */
#define PCR_TINGLE_MSG_NAME 503 /* "Treasure Chart" label at the bottom of the screen */
#define PCR_TINGLE_MSG_DESC 703 /* its description */
#define PCR_TINGLE_QUEST_ICON "tingle_figure.bti" /* replaces "cmap_treasure2.bti" */

/* The event bit statue `index` (0 Dragon .. 4 Wind) sets; 0 for another index. */
uint16_t pc_rando_tingle_bit(int index);
/* The event bit item `item` sets (0xA3-0xA7), else 0. */
uint16_t pc_rando_tingle_bit_for_item(uint8_t item);
/* check_tingle_statue_owned's mapping: the bit for stage 3-7 with flag 0xF, else 0 (the function
   then returns false). */
uint16_t pc_rando_tingle_bit_for_stage(int stage, int flag);

/* wwrando's texts for messages 503 and 703 (wwrando string syntax; 703 as word_wrap_string wraps it
   with the USA font rock_24_20_4i_usa.bfn). */
const char* pc_rando_tingle_msg_text(uint16_t msg_no);
/* Puts 503 and 703 in the message overlay (pc_rando_msg.h). Returns 1 when both were set. */
int pc_rando_tingle_set_messages(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TINGLE_H */
