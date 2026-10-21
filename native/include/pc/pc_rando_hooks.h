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

/* --- R2.5: boss Heart Containers (flexible_item_locations.asm; game_hooks/pc_rando_hooks_boss.cpp,
   table in features/rando/pc_rando_boss_items.h) --- */
/* The item fopAcM_createItemForBoss spawns under pc_rando(): the item placed at the Heart Container
   location of the boss of stage `key` when it is a boss stage number (2-7, passed by daBossItem),
   else of the loaded stage's save table (the disappear cloud and Gohdan/Molgera pass 0; the sea
   stands for Forsaken Fortress). 0x08 (Heart Container) when that stage has no boss or no placement
   is set. */
uint8_t pc_rando_boss_item(int key);

/* --- R2.7: code locations B, Windfall NPCs (flexible_item_locations.asm; bodies in
   game_hooks/pc_rando_hooks_windfall.cpp) --- */
/* The unused event bits wwrando sets and checks instead of "does the player own the vanilla item"
   (plan D17), so an NPC knows it gave its randomized item. */
#define PC_RANDO_BIT_POMPIE_VERA 0x6904  /* Pompie and Vera's gift (vanilla Treasure Chart 24) */
#define PC_RANDO_BIT_MINENCO 0x6908      /* Minenco's gift (vanilla Treasure Chart 33) */
#define PC_RANDO_BIT_KAMO 0x6910         /* Kamo's gift (vanilla Treasure Chart 31) */
#define PC_RANDO_BIT_LENZO_DELUXE 0x6920 /* Lenzo's Deluxe Picto Box slot */
#define PC_RANDO_BIT_SALVAGE_CORP 0x6980 /* the Salvage Corp's gift */
#define PC_RANDO_BIT_MAGGIE 0x6A01       /* Maggie's free item (vanilla Maggie's Letter) */
#define PC_RANDO_BIT_POSTMAN 0x6A02      /* the Windfall cafe postman's item (vanilla Moblin's Letter) */
/* d_a_npc_people l_get_item_no[idx], read in daNpcPeople_c::eventGetItemInit: the item placed at that
   townsperson's location (0 Sam, 1 Pompie and Vera, 2 Minenco, 3 Linda and Anton, 4 Kreeb, 5 Kamo);
   `vanilla` for another index. */
int pc_rando_people_item(int idx, int vanilla);
/* create_item_and_set_event_bit_for_townsperson: after the gift of l_get_item_no[idx] is created,
   sets its event bit (PC_RANDO_BIT_POMPIE_VERA/MINENCO/KAMO for idx 1/2/5; nothing for the others). */
void pc_rando_people_item_given(int idx);
/* The event bit of l_get_item_no[idx] (0 when it has none). */
uint16_t pc_rando_people_item_bit(int idx);
/* d_a_npc_people l_item_id_sa3[idx], read in daNpcPeople_c::next_msgStatus: the item placed at Kane's
   gate location idx (0 Shop Guru, 1 Postman, 2 Six Flags, 3 Six Idols); `vanilla` for another index. */
int pc_rando_kane_item(int idx, int vanilla);

/* --- R2.8: code locations C (features/rando/pc_rando_code_c.h holds the site table; the item
   creation of the Withered Trees is in game_hooks/pc_rando_hooks_code_c.cpp) --- */
/* Sites of pc_rando_code_c_item; the index is what the vanilla code indexes its item table with. */
#define PC_RANDO_CODE_C_AUCTION 0            /* d_a_auction l_item_dat[0..3] */
#define PC_RANDO_CODE_C_BATTLESQUID 1        /* d_a_npc_kg1 items[0..1] */
#define PC_RANDO_CODE_C_BATTLESQUID_RECORD 2 /* d_a_npc_kg1 rec_items[0] */
#define PC_RANDO_CODE_C_BARREL_SHOOTING 3    /* d_a_npc_kg2: event register 0xB703 (0..2) */
#define PC_RANDO_CODE_C_BIRD_MAN 4           /* d_a_npc_bmcon1: 0 */
#define PC_RANDO_CODE_C_GORON_TRADE 5        /* d_a_npc_roten l_get_item_no[0] */
#define PC_RANDO_CODE_C_MAILBOX 6            /* d_a_obj_toripost m_letter[0..12] */
#define PC_RANDO_CODE_C_WITHERED_TREE 7      /* d_a_obj_ftree: 0 launch, 1 place */
#define PC_RANDO_CODE_C_ZUNARI 8             /* d_a_npc_rsh1: 0 talk01, 1 getdemo compare, 2 slot */
#define PC_RANDO_CODE_C_DOC_BANDAM 9         /* d_a_npc_ds1: the potion item id (0x52, 0x53) */
#define PC_RANDO_CODE_C_SITE_COUNT 10
/* The item placed at code location (site, index), or `vanilla` when pc_rando() is 0, no placement is
   set, or that index is not a location. Pure (pc_rando_code_c.cpp). */
uint8_t pc_rando_code_c_item(int site, int index, uint8_t vanilla);
/* custom_createItem (custom_funcs.asm): fopAcM_createItem with type 3 (no fade) and action 5 (the
   "ding"), which loads the item's field model, unlike fopAcM_fastCreateItem. `pos` is a cXyz*,
   `angle` a csXyz*, `scale` a cXyz*. Returns the process id, or -1 (fpcM_ERROR_PROCESS_ID_e) for item
   0xFF or a failed create. */
uint32_t pc_rando_custom_create_item(void* pos, int item_no, int room_no, void* angle, void* scale,
                                     int item_bit_no);
/* withered_tree_item_try_give_momentum: on the first frame the tree's heart part item `item` (a
   fopAc_ac_c*, non-NULL) exists, gives it the launch speeds of the vanilla fastCreateItem call (1.75
   forward, 30 up, gravity -2.1) and actor status bit 0x40, and sets *done; does nothing when *done is
   set (also set by place_heart_part, whose item keeps still). */
void pc_rando_withered_tree_item_momentum(void* item, uint8_t* done);

/* --- R2.9: Beedle shops (game_hooks/pc_rando_hooks_shop.cpp) --- */
/* The 4 shop locations, in this order: Great Sea shop ship 20 Rupee slot (Bait Bag), Rock Spire
   special shop ship 500 (Empty Bottle), 950 (Piece of Heart) and 900 Rupee (Treasure Chart 4) slots.
   Their items reach d_shop.cpp's shopItemData_* through R0.8 table edits, which a listener registers
   at every activation together with remove_shop_item_forced_uniqueness_bit. */
#define PCR_SHOP_SLOT_COUNT 4
/* flexible_item_locations.asm set_shop_item_in_bait_bag_slot_sold_out: event bit 0x6902 on, after
   daNpc_Bs1_c::next_msgStatus sold out the Bait Bag slot. */
void pc_rando_shop_bait_slot_set_sold_out(void);
/* check_shop_item_in_bait_bag_slot_sold_out: 1 when event bit 0x6902 is set (createShopList then
   shows the slot's replacement, as vanilla does once the Bait Bag is owned). */
int pc_rando_shop_bait_slot_sold_out(void);
/* For the smoke: slot -> PCR_LOC_* (-1 when out of range), its vanilla item (0xFF), and the edits
   registered again by hand (1 when the registry took all 8). */
int pc_rando_shop_slot_loc(int slot);
uint8_t pc_rando_shop_slot_vanilla(int slot);
int pc_rando_shop_register_edits(void);

/* --- R3.1: new-game state (custom_funcs.asm init_save_with_tweaks, reveal_sea_chart.asm;
   game_hooks/pc_rando_hooks_newgame.cpp, tables in features/rando/pc_rando_newgame.h) --- */
/* Under pc_rando(), right after dComIfGs_init of a randomized new file: what init_save_with_tweaks
   adds after dSv_save_c::init (sword mode, Sail, Wind Waker, Wind's Requiem, Pirate's Charm, 30
   arrows and bombs, starting magic, starting gear through convert_progressive_item_id and
   execItemGet, story event bits, pigs register, targeting mode, stage switches, boss-intro bits,
   spoils/bait ever owned, Triforce Shards, rematch bits) and, with reveal_full_sea_chart, every
   sector charted. Uses pc_rando_cfg(). Called by R0.7's pc_rando_slot_new_game_end and
   pc_rando_slot_debug_boot_end (pc_rando_hooks_slot.cpp) after the record is written; no game/ hunk
   calls it. A no-op when pc_rando() is 0. */
void pc_rando_new_game_init(void);

/* --- R3.5: key bag text (misc_rando_features.asm check_run_new_text_commands /
   exec_curr_num_keys_text_command; game_hooks/pc_rando_hooks_keybag.cpp, pure part in
   features/rando/pc_rando_keybag.h) --- */
/* The new text commands of fopMsgM_msgDataProc_c::stringSet (group 0, codes 0x4B-0x4F): for one of
   them writes its expansion (the dungeon's small keys, plus " +Big" or five spaces) NUL-terminated
   into `out` and returns 1; for any other code returns 0 and leaves `out` alone. Called only under
   pc_rando(). The same file adds messages 403/603 to the overlay at every activation. */
int pc_rando_text_command(int code, char* out, size_t cap);

/* --- R3.15: stage tweaks A (features/rando/pc_rando_tweak_stages_a.h; game_hooks/
   pc_rando_hooks_stages_a.cpp) --- */
/* Registers the dzx patcher pc_rando_stages_a_patch and the dzb patcher below (listed in
   features/rando/pc_rando_patchers.def; also called by the smoke). Idempotent. */
void pc_rando_stages_a_init(void);
/* tweaks.py fix_forsaken_fortress_door_softlock: sea/Room1 room.dzb (after cBgS::ConvDzb), face
   0x1493's property index becomes 0x11. A res patcher for 'DZB ' files. */
struct PcRandoResFile;
void pc_rando_stages_a_patch_dzb(struct PcRandoResFile* file);

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

/* --- R2.4: event locations and Hurricane Spin (placement in features/rando/pc_rando_place_event.h;
   game_hooks/pc_rando_hooks_event.cpp) --- */
/* custom_funcs.asm hurricane_spin_item_func: the unused event bit (plan D17) the Hurricane Spin item
   (0xAA) sets under pc_rando(), through an item-get override registered at static init. */
#define PC_RANDO_BIT_HURRICANE_SPIN 0x6901
/* flexible_item_locations.asm 0x80158C08: 1 when the player owns Hurricane Spin (event bit 0x6901).
   daPy_lk_c::procCutTurnMove_init asks this under pc_rando() instead of 0x0B20, the bit of Orca's
   teaching event. */
int pc_rando_hurricane_spin_owned(void);

/* --- R2.10: added chests (features/rando/pc_rando_tweak_added_chests.h; the stage edits are an
   archive patcher, pc_rando_patchers.def) --- */
/* daDemo00_c's cutscene item l_itemNo[argID] (d_a_demo00.cpp): argID 0 Master Sword Chamber, 5
   Jabun's Cave, 8 Inside Mother Isle give their placed items; any other argID, and pc_rando() 0,
   give `vanilla`. */
uint8_t pc_rando_added_chests_demo_item(int argID, uint8_t vanilla);

/* --- R3.2: health, magic, clothes, start spawn (misc_rando_features.asm set_starting_health and the
   file-select / load / game-over HP patches; tweaks.py skip_wakeup_intro_and_start_at_dock,
   start_ship_at_outset, prevent_fire_mountain_lava_softlock; game_hooks/pc_rando_hooks_start.cpp,
   pure part in features/rando/pc_rando_start.h) --- */
/* dSv_player_status_a_c::init under pc_rando(): the new file's current HP (starting_quarter_hearts
   rounded down to a whole heart; the max is starting_quarter_hearts itself). */
uint16_t pc_rando_start_new_life(void);
/* dGameover_c::_execute under pc_rando(): the continue HP, min(12, max rounded down to a heart). */
uint16_t pc_rando_start_gameover_life(uint16_t maxLife);
/* dFile_select_c::makeRecInfo (get_current/max_health_for_file_select_screen): the HP a slot shows.
   `saved` is the slot's saved mLife (isMax 0) or mMaxLife (isMax 1); a slot is blank when the first
   byte of its player name is 0. Returns `saved` unless the slot is blank and the rando arm is on
   (pc_rando(), or a pending seed that the next New Game consumes: at file select pc_rando() is 0),
   then the starting HP of that config (current rounded down to a whole heart). */
uint16_t pc_rando_start_file_select_life(char firstNameChar, uint16_t saved, int isMax);
/* dScnName_c::FileSelectMainNormal after card_to_memory and R0.7's load hook, under pc_rando():
   fully_refill_magic_meter_and_cap_health_on_load_save (magic = max magic, HP capped at the max
   rounded down to a whole heart). */
void pc_rando_start_on_load(void);
/* Registers the sea/Stage stage.dzs patcher (the Ship ACTR; listed in pc_rando_patchers.def).
   Idempotent. */
void pc_rando_start_init(void);
/* For the smoke: the island room's SHIP 0 spawn that the patcher copies (read from the disc once).
   Returns 1 and fills x/z/yRot when it was read. `patched` (may be NULL) gets how often the Ship ACTR
   was patched since boot. */
int pc_rando_start_ship_spawn(float* x, float* z, uint16_t* yRot, int* patched);

/* --- R3.17: the pirate ship at Windfall (tweaks.py add_pirate_ship_to_windfall; the stage and event
   edits are archive patchers in features/rando/pc_rando_tweak_pirate_ship.h; game side in
   game_hooks/pc_rando_hooks_pirate_ship.cpp) --- */
/* JAIZelBasic::setScene under pc_rando(): the second dynamic scene wave of stage BGM entry `sceneNum`
   (`vanilla`: m_scene_info[sceneNum].field_0x3). For Asoko (0x0C) the free slot 0x0E, filled with
   IsleLink_0.aw (0x19, Aryll's voice) on first use (main.dol 0x8039C30C + 0xC*4 + 3 and
   0x8039C2E4 + 0xE*2); `vanilla` for every other entry, or when the slot is not free. */
uint8_t pc_rando_pirate_ship_scene_wave2(int sceneNum, uint8_t vanilla);
/* Puts messages 3008, 849 and 850 into the overlay from the message font and the BMG's entry of 3008.
   Returns the number set (3), 0 while the font or the BMG is not loaded. The archive patcher calls it
   after each activation; exposed for the smoke. */
int pc_rando_pirate_ship_build_messages(void);
/* Registers the patchers (listed in pc_rando_patchers.def). Idempotent. */
void pc_rando_pirate_ship_init(void);

/* --- R6.5: trap chests (asm/actors/d_a_tbox.cpp demoProc SPRING_TRAP; game_hooks/pc_rando_hooks_trap.cpp,
   the file edits in features/rando/pc_rando_trap.h) --- */
/* daTbox_c::demoProc, SPRING_TRAP's first frame under pc_rando(): the trap timer (mOpenTimer) back to
   0 and the smoke (particle 0x029D) on the chest. `tbox` is the daTbox_c. */
void pc_rando_tbox_spring_trap_init(void* tbox);
/* daTbox_c::demoProc, every SPRING_TRAP frame under pc_rando() (demoProcSpringTrap): frame 5 freezes
   the player and takes 2 quarter hearts, 0x30 stops the smoke, 0x60 thaws him, 0x64 restores his
   ground checks and ends the cut. */
void pc_rando_tbox_spring_trap(void* tbox);

/* --- R6.3: sword modes (swordless.asm, tweaks.py update_text_for_swordless;
   game_hooks/pc_rando_hooks_sword.cpp). The pool and the new-game parts are R1.4's and R3.1's. --- */
/* 1 while pc_rando() is on and the seed's sword_mode is Swordless. */
int pc_rando_swordless(void);
/* phase_4 (d_s_play.cpp) under pc_rando(), in its GTower branch after the bow is taken
   (give_temporary_sword_during_ganondorf_fight_in_swordless, main.dol 0x80235F10): in a Swordless
   seed with no sword equipped, equips the Hero's Sword (0x38) for the Ganondorf fight. */
void pc_rando_sword_ganondorf_stage(void);
/* phase_4 under pc_rando(), in the branch for every other stage, before its bow check
   (give_temporary_sword_in_orcas_house_in_swordless 0x8023607C and
   remove_temporary_sword_when_loading_stage_in_swordless 0x80236084). In a Swordless seed: in
   Ojhous with no sword equipped, equips the Hero's Sword and returns 0 (the asm then skips the bow
   check); elsewhere (or in Ojhous with a sword equipped) unequips the sword when no sword is owned
   (collect byte 0 is 0) and returns 1. In other seeds returns 1 and changes nothing. */
int pc_rando_sword_stage_load(void);
/* Messages 1128 and 1590 in the overlay iff the seed is Swordless (update_text_for_swordless).
   Called by an activation listener; exposed for the smoke. Returns 1 when they are set. */
int pc_rando_sword_sync_texts(void);
/* --- R6.6: charts (charts.py ChartRandomizer._save; features/rando/pc_rando_charts.h, which also
   holds the sea/Room1-49 salvage SCOB patcher) --- */
/* dMenu_Fmap_c::aramCmapDatRead under pc_rando(): patches the CmapDat.bin just read (big-endian disc
   bytes; its size is its record count's, as the archive's getResSize gives the packed size) for the
   active seed's charts. A second call on the same, already patched buffer changes nothing. */
void pc_rando_charts_cmapdat_loaded(void* data);
/* --- R6.7: starting island (randomizers/starting_island.py _save: tweaks.py
   set_new_game_starting_room, change_ship_starting_island; game_hooks/pc_rando_hooks_start.cpp) --- */
/* The active seed's starting island: a sea room 1-49 (starting_island_room), PCR_START_ROOM (44,
   Outset) when no seed is active or its room is out of range. The new game's return-place room and
   the island whose SHIP 0 the sea stage's Ship ACTR copies. */
int pc_rando_start_room(void);
/* --- R4.4: file-select display (D15; game_hooks/pc_rando_hooks_fileselect.cpp, API in
   features/rando/pc_rando_fileselect.h). Like R0.7's hooks it runs whatever pc_rando() is (the file
   select runs with it off) and only reads: a vanilla slot shows nothing. --- */
/* dFile_select_c::_draw, every frame the file select draws: `saveData` the 3 card_gamedata of the
   card (mSaveDataPtr, may be NULL), `selectNum` the selected slot, `proc` its DataSelProc index. Works
   out the line the overlay draws this frame for that slot: its seed (a rando record), a missing
   sidecar, or the pending seed (a blank slot). */
void pc_rando_file_select_draw(const uint8_t* saveData, uint8_t selectNum, uint8_t proc);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_HOOKS_H */
