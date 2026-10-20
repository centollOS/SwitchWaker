/*
 * pc_rando_hooks.h - the randomizer's hook bodies that game/ calls (docs/RANDOMIZER_PLAN.md,
 * "Conventions every step follows").
 *
 * A hunk in game/ that needs more than a few lines calls a function declared here; its body lives in
 * native/src/pc/game_hooks/pc_rando_hooks_<area>.cpp. Every hook is only called under pc_rando()
 * (pc_rando.h), from a hunk marked "// pc_rando(<step>): ..." and listed in docs/RANDO_HOOKS.md.
 *
 * Append-only registry: each step adds its own section below, headed "R<phase>.<n>", in plan
 * order; on a merge keep both sides. Game thread only.
 */
#ifndef PC_RANDO_HOOKS_H
#define PC_RANDO_HOOKS_H

#include "pc/pc_rando.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- R0.1: no hooks (the switch itself is pc_rando.h) --- */

/* --- R0.8: item dispatch (game_hooks/pc_rando_hooks_items.cpp; registries in
   features/rando/pc_rando_items.h) --- */
/* execItemGet under pc_rando(): the override registered for `item_no`, else item_func_ptr. */
void pc_rando_exec_item_get(uint8_t item_no);
/* checkItemGet under pc_rando(): the getcheck override, else item_getcheck_func_ptr; -1 gives
   `default_val`. */
int pc_rando_check_item_get(uint8_t item_no, int default_val);

/* --- R0.10: the archive-load hook (features/rando/pc_rando_resources.h; set's body is in the pure
   pc_rando_resources.cpp, loaded's in game_hooks/pc_rando_hooks_resources.cpp) --- */
/* dRes_info_c::set under pc_rando(): records the archive directory `arc_path` of `info` (the
   dRes_info_c), so stage files are dispatched by stage directory. */
void pc_rando_res_hook_set(const void* info, const char* arc_path);
/* dRes_info_c::loadResource under pc_rando(), for each file it puts into mRes[]: runs the registered
   patchers on `res` (file `file_name` of type `type`, a four-character code such as 'DZS ', of the
   archive `archive` (a JKRArchive) named `arc_name`) and returns the pointer to store: `res`, or a
   patched copy on the archive heap. */
void* pc_rando_res_hook_loaded(const void* info, const char* arc_name, void* archive, uint32_t type,
                               const char* file_name, void* res);

/* --- R0.12: the message overlay (features/rando/pc_rando_msg.h; bodies in pc_rando_msg.cpp, which
   needs no game code) --- */
/* The overlay's encoded text for msg_no (BMG bytes ending with a NUL), or NULL: keep the BMG's.
   Called by fopMsgM_msgGet_c::getMessage and fopMsgM_itemMsgGet_c::getMessage under pc_rando(). */
const char* pc_rando_msg_hook_text(uint16_t msg_no);
/* Rewrites `entry` (a JMSMesgEntry_c: the 0x18-byte big-endian BMG INF1 entry the BMG gave for
   msg_no, or for the last message when the BMG has none) from the overlay: the record's entry fields
   when it has some; wwrando's new-message defaults when it has only text and the BMG entry is for
   another number; else unchanged. Called by both getMesgEntry under pc_rando(). */
void pc_rando_msg_hook_entry(uint16_t msg_no, void* entry);

/* --- R3.9: nonlinear A (make_game_nonlinear.asm) --- */

/* reset_makar_position / reset_medli_position: when Makar (option 1, d_a_npc_cb1) or Medli (option 2,
   d_a_npc_md) is created in his/her temple, the saved partner position (dSv_player_priest_c, flag
   kept) is set to the spawn near the entry the player came from (dComIfGp_getStartStagePoint, 7
   entries per temple); for any other entry it is set to the player's position, rotation and room.
   The caller then passes it to dComIfGs_setRestartOption as the original code does. */
void pc_rando_reset_partner_position(int option);

/* --- R3.11: cutscene removal (remove_cutscenes.asm; pc_rando_hooks_cutscenes.cpp) --- */
/* check_hyrule_warp_unlocked: 1 when the player owns all 8 Triforce shards (the Hyrule warp,
   d_a_warpdm20, replaces its event-bit 0x2D08 check with this). */
int pc_rando_hyrule_warp_unlocked(void);
/* give_pearl_and_raise_totg_if_necessary, after item_func_pearl1/2/3 gave pearl `symbol`
   (dSymbol_NAYRU_e/DIN_e/FARORE_e): sets the pearl's "placed" bit (0x1410/0x1480/0x1440) and, once
   all three pearls are owned, 0x1E40 (Tower of the Gods raised) and 0x2E80 (its cutscene seen). */
void pc_rando_place_pearl(int symbol);
/* check_play_special_item_get_music: the item-get BGM for `item` (daPy_lk_c::setGetItemSound's
   normal-jingle branch): the pearl music for the 3 pearls, the song music for the 6 songs, else
   JA_BGM_ITEM_GET. */
uint32_t pc_rando_item_get_bgm(uint16_t item);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_HOOKS_H */
