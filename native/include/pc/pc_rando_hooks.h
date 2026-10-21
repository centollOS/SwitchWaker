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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- R0.1: no hooks (the switch itself is pc_rando.h) --- */

/* --- R0.7: slot binding (game_hooks/pc_rando_hooks_slot.cpp; pure part in
   features/rando/pc_rando_slot.h). Unlike the other hooks these run whatever pc_rando() is, since
   they decide it (D7); with no rando record and no pending seed each is a no-op, so a vanilla file
   is unchanged. --- */
/* dScnName_c::NameInMain, right before dComIfGs_init of a new file: with a pending seed
   (pc_rando_slot_set_pending), writes its sidecar into the card directory and activates it, so the
   init functions dComIfGs_init runs already see pc_rando() == 1; else turns the randomizer off. */
void pc_rando_slot_new_game_begin(void);
/* Right after that dComIfGs_init: a randomized new file gets its record in mReserve; a vanilla one
   gets a stale rando record (left in memory by a slot loaded before) cleared (D4). */
void pc_rando_slot_new_game_end(void);
/* dScnName_c::FileSelectMainNormal, right after dComIfGs_setCardToMemory: a slot whose mReserve holds
   a record activates the sidecar it names from the card directory; any other slot plays vanilla. */
void pc_rando_slot_card_loaded(void);
/* dScnTitle_Create and dScnName_Create (file select, not the end-of-game NAMEEX save scene): turns
   the randomizer off. */
void pc_rando_slot_scene_entry(void);
/* pcBootStage (debug stage boot), around its dComIfGs_init: with COS_RANDO_PLACEMENT=<placement
   json> the file is a randomized new file as with a pending seed (record flag DEBUG_BOOT); exits 2 if
   the file does not load. Without it: as a vanilla new game. */
void pc_rando_slot_debug_boot_begin(void);
void pc_rando_slot_debug_boot_end(void);
/* The memory card A folder (Aurora's GCI folder: <userPath>/USA/Card A, or the run's card in harness
   runs), without a trailing slash. Returns the length, 0 when unknown. */
size_t pc_rando_slot_card_dir(char* buf, size_t cap);
/* <exe dir>/user (Switch: <COS_SWITCH_ROOT>/user), defined in runtime/pc_main.cpp. */
const char* pc_user_path(void);
/* 1 for COS_SMOKE=rando with COS_RANDO_SMOKE=slot, which needs its own card (pc_main.cpp then calls
   prepareRunCard before CARDInit); defined in harness/smokes/pc_rando_smoke_slot.cpp. */
int pc_rando_smoke_wants_run_card(void);

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

/* --- R2.11: any item anywhere (game_hooks/pc_rando_hooks_field_items.cpp; the table edits and the
   id list in features/rando/pc_rando_field_items.h) --- */
/* daItem_c::mode_wait under pc_rando(): 1 when field item `item_no` moves with itemActionForArrow
   (wwrando: ids 0x20-0x44 and every id above 0x4B), else its vanilla case. */
int pc_rando_field_item_wait_arrow(uint8_t item_no);

/* --- R3.3: progressive items (make_items_progressive.asm; pc_rando_hooks_progressive.cpp, tier
   tables in features/rando/pc_rando_progressive.h) --- */
/* convert_progressive_item_id: for a progressive id, the id of the tier the save gets next (sword
   0x38/39/3A/3E, shield, bow, wallet, bomb bag, quiver, picto box, magic meter); any other id
   unchanged. Used by the R3.3 hunks (demo item, field item, get-item demo, shop item, Beedle's
   item-get check) and by R3.1's starting gear loop. Like the asm, it compares the whole value, so a
   value outside 0..0xFF comes back unchanged. */
int pc_rando_convert_progressive_item_id(int item_no);

/* --- R3.6: Tingle Statues (asm group G4; game_hooks/pc_rando_hooks_tingle.cpp, tables in
   features/rando/pc_rando_tingle.h) --- */
/* check_tingle_statue_owned (custom_funcs.asm): replaces dComIfGs_isStageTbox(stage, 0xF) at the
   statue sites (d_a_obj_vtil, d_a_npc_tc, d_a_agb, d_a_agbsw0). 1 when the event bit of the statue of
   `stage` (3-7) is set; 0 for another stage or a flag other than 0xF. Same type as
   dComIfGs_isStageTbox, so a hunk can pick either function. */
int pc_rando_tingle_statue_owned(int stage, int flag);
/* get_num_owned_tingle_statues (misc_rando_features.asm): the number of owned statues (0-5), which
   the quest screen shows instead of dComIfGs_getCollectMapNum(). */
int pc_rando_tingle_statue_count(void);

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

/* --- R3.4: dungeon items (game_hooks/pc_rando_hooks_dungeon_items.cpp; table and decisions in
   features/rando/pc_rando_dungeon_items.h) --- */
/* Registers the 22 per-dungeon item-get overrides (pc_rando_items.h) and the activation listener that
   adds their table edits and item-get messages. Called once at static init by R0.8's table binding
   (pc_rando_hooks_items.cpp), which also makes the linker keep the file. No game/ hunk calls it. */
void pc_rando_dungeon_items_register(void);

/* --- R3.13: misc features (misc_rando_features.asm, tweaks.py add_new_bog_warp and
   fix_message_closing_sound_on_quest_status_screen; game_hooks/pc_rando_hooks_misc.cpp) --- */
/* custom_isTbox_for_unloaded_stage_save_info: 1 when chest flag `no` (0-31) is set in the SAVED stage
   memory of stage `stage_no` (dSv_save_c's mMemory[stage_no], not the loaded stage's copy), else 0.
   Outset's alternate BGM tests (13, 5): the Pirate Ship chest that held the bombs. */
int pc_rando_isTbox_unloaded(int stage_no, int no);
/* The two new messages, added to the message overlay at every activation (pc_rando_add_listener):
   848 "Warp to Forsaken Fortress?" (the 10th Ballad of Gales warp's confirmation) and 704, the blank
   description of the quest screen's Options button. Exposed for the smoke; returns 1 when both were
   added. */
int pc_rando_misc_add_messages(void);

/* --- R3.21: post-randomization texts (tweaks.py update_shop_item_descriptions and the other
   seed-dependent message rewrites; game_hooks/pc_rando_hooks_posttext.cpp, texts built by
   features/rando/pc_rando_posttext.h) --- */
/* Registers the activation listener that builds the texts. Called once at static init by R0.8's table
   binding (pc_rando_hooks_items.cpp), which also makes the linker keep the file. No game/ hunk calls
   it. */
void pc_rando_posttext_register(void);
/* Builds the texts for the active seed from the BMG, the placement and the message font and puts them
   in the message overlay (the listener calls it at every activation; exposed for the smoke). Returns
   the number of messages set. */
int pc_rando_posttext_build(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_HOOKS_H */
