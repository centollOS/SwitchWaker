/*
 * pc_rando_dungeon_items.h - the randomizer's per-dungeon item IDs (docs/RANDOMIZER_PLAN.md step R3.4,
 * asm report group G3; wwrando tweaks.py allow_dungeon_items_to_appear_anywhere and
 * custom_funcs.asm generic_on_dungeon_bit / generic_small_key_item_get_func).
 *
 * wwrando always gives each dungeon's small key, Big Key, map and compass an item ID of its own (22
 * IDs that are unused or junk in the original game), so they can be placed anywhere: the item-get
 * function adds the key or sets the dungeon bit for *that* dungeon, wherever the player is. This file
 * holds the pure part:
 *   - the 22-entry table (id, dungeon stage, kind, base item, item-get message text);
 *   - the decisions of the two generic functions, as pure functions of the save/stage state;
 *   - the 22 item-get messages (101 + id) for the message overlay (pc_rando_msg.h).
 * The game side (game_hooks/pc_rando_hooks_dungeon_items.cpp) registers the 22 item-get overrides
 * (pc_rando_items.h), copies the base items' item_resource / field_item_res / item_info entries and
 * adds the messages at every activation.
 *
 * No game headers, no game calls (it links into cos_rando_core and the unit tests).
 */
#ifndef PC_RANDO_DUNGEON_ITEMS_H
#define PC_RANDO_DUNGEON_ITEMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What a dungeon item gives. MAP/COMPASS/BIG_KEY are the dSv_memBit_c dungeon-item bit indexes. */
#define PCR_DITEM_MAP 0       /* dSv_memBit_c::MAP */
#define PCR_DITEM_COMPASS 1   /* dSv_memBit_c::COMPASS */
#define PCR_DITEM_BIG_KEY 2   /* dSv_memBit_c::BOSS_KEY */
#define PCR_DITEM_SMALL_KEY 3 /* +1 small key (not a bit) */

/* The vanilla items whose models, icons and info the new IDs copy (tweaks.py base_item_id). */
#define PCR_DITEM_BASE_SMALL_KEY 0x15 /* dItemNo_SMALL_KEY_e */
#define PCR_DITEM_BASE_MAP 0x4C       /* dItemNo_MAP_e */
#define PCR_DITEM_BASE_COMPASS 0x4D   /* dItemNo_COMPASS_e */
#define PCR_DITEM_BASE_BIG_KEY 0x4E   /* dItemNo_BOSS_KEY_e */

#define PCR_DITEM_COUNT 22

typedef struct PcRandoDungeonItem {
    uint8_t id;             /* the item ID (tweaks.py dungeon_items) */
    uint8_t stage;          /* the dungeon's save stage number: FF 2, DRC 3, FW 4, TotG 5, ET 6, WT 7 */
    uint8_t kind;           /* PCR_DITEM_* */
    uint8_t base_id;        /* PCR_DITEM_BASE_* */
    const char* short_name; /* "DRC" (Logic.DUNGEON_NAMES key) */
    const char* name;       /* "DRC Small Key" (register_renamed_item) */
    const char* text;       /* the item-get message in wwrando string syntax, word-wrapped as wwrando
                               wraps it (BFN rock_24_20_4i_usa, ITEM_GET width 419) */
} PcRandoDungeonItem;

/* Entry i (0..PCR_DITEM_COUNT-1, tweaks.py order), or NULL. */
const PcRandoDungeonItem* pc_rando_dungeon_item(int i);
/* The entry for item `id`, or NULL when id is not a dungeon item. */
const PcRandoDungeonItem* pc_rando_dungeon_item_find(uint8_t id);

/* generic_on_dungeon_bit: which stage info gets the bit. */
#define PCR_DITEM_TARGET_CURRENT 0 /* the loaded stage's memory (dSv_info_c::mMemory) */
#define PCR_DITEM_TARGET_SAVED 1   /* the saved stage info of the item's dungeon (mSave[stage]) */
/* generic_small_key_item_get_func: how the key is added. */
#define PCR_DITEM_KEY_ITEM_FUNC 2  /* item_func_small_key (the meter's key counter adds it) */

/* The target of a map/compass/Big Key bit: CURRENT when the current stage (dSv_danBit_c::mStageNo)
   is the item's dungeon, else SAVED. */
int pc_rando_dungeon_bit_target(int cur_stage, int item_stage);
/* The target of a small key: KEY_ITEM_FUNC in the item's dungeon in a room whose STAG shows the key
   counter (dStage_stagInfo_ChkKeyDisp); CURRENT (mKeyNum += 1 directly) in the item's dungeon
   elsewhere (a boss room, or no stage info); SAVED (mSave[stage].mKeyNum += 1) in any other stage. */
int pc_rando_dungeon_key_target(int cur_stage, int item_stage, int key_disp);

/* Adds the 22 item-get messages (101 + id: ITEM_GET box, slow draw, the item's image) to the message
   overlay. Returns the number added (PCR_DITEM_COUNT when all went in). */
int pc_rando_dungeon_items_add_messages(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_DUNGEON_ITEMS_H */
