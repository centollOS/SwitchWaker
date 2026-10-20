/*
 * pc_rando_items.h - item dispatch overrides and item/shop table snapshots of the built-in randomizer
 * (docs/RANDOMIZER_PLAN.md step R0.8).
 *
 * Two registries that later steps fill, both seed-independent unless a step re-registers on
 * activation (a listener, pc_rando.h):
 *
 * 1. Item-get overrides. wwrando rewrites item_func_ptr[] for 51 ids (progressive items 19, dungeon
 *    items 27, Tingle statues 5, plus Hurricane Spin 0xAA: tweaks.py make_items_progressive,
 *    allow_dungeon_items_to_appear_anywhere, update_tingle_statue_item_get_funcs). The port keeps the
 *    tables untouched and dispatches instead: under pc_rando(), execItemGet/checkItemGet (d_item.cpp)
 *    call pc_rando_exec_item_get/pc_rando_check_item_get (pc_rando_hooks.h), which call the override
 *    registered here, else the vanilla table entry.
 *
 * 2. Table edits. Byte edits to dItem_data::item_resource[], field_item_res[], item_info[] and the
 *    shop item data of d_shop.cpp, registered with pc_rando_table_edit. The first activation snapshots
 *    every bound table, then the edits are applied; deactivation restores the snapshot, so a vanilla
 *    file after a randomized one sees the boot bytes again. An edit registered while active is
 *    applied at once. The game side (game_hooks/pc_rando_hooks_items.cpp) binds the real tables at
 *    static-init time; the unit test binds fake ones.
 *
 * Offsets are HOST struct offsets: use offsetof() on the port's structs (pointers are 8 bytes on a
 * 64-bit host), never the GameCube offsets of tweaks.py.
 *
 * This file and pc_rando_items.cpp use no game header or game symbol (cos_rando_core links them into
 * the unit tests). Game thread only.
 */
#ifndef PC_RANDO_ITEMS_H
#define PC_RANDO_ITEMS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- 1. Item-get overrides ---------------------------------------------------------------- */

typedef void (*PcRandoItemFunc)(void);     /* item_func_ptr's ItemGetFunc */
typedef int (*PcRandoItemCheckFunc)(void); /* item_getcheck_func_ptr's ItemGetCheckFunc (-1 = default) */

/* Registers (or with NULL removes) the function execItemGet calls for `id` under pc_rando(). */
void pc_rando_item_override(uint8_t id, PcRandoItemFunc fn);
/* Same for checkItemGet. */
void pc_rando_item_getcheck_override(uint8_t id, PcRandoItemCheckFunc fn);
/* The registered functions, NULL when none. */
PcRandoItemFunc pc_rando_item_override_get(uint8_t id);
PcRandoItemCheckFunc pc_rando_item_getcheck_override_get(uint8_t id);
/* Number of ids with an item-get override (for logs and tests). */
int pc_rando_item_override_count(void);

/* --- 2. Table snapshots and edits ----------------------------------------------------------- */

typedef enum PcRandoTable {
    PCR_TBL_ITEM_RESOURCE = 0, /* dItem_data::item_resource[0x100] (d_item_data.cpp) */
    PCR_TBL_FIELD_ITEM_RES,    /* dItem_data::field_item_res[0x100] */
    PCR_TBL_ITEM_INFO,         /* dItem_data::item_info[0x100] */
    PCR_TBL_SHOP_ITEM_DATA,    /* d_shop.cpp ShopItems_c__ItemData shopItemData_*, PCR_SHOP_ITEM_* order */
    PCR_TBL_SHOP_SET_DATA,     /* d_shop.cpp __shop_items_set_data shopItems_setData_*, PCR_SHOP_SET_* order */
    PCR_TBL_COUNT
} PcRandoTable;

/* Index of each shopItemData_* entry in PCR_TBL_SHOP_ITEM_DATA (declaration order of d_shop.cpp). */
enum {
    PCR_SHOP_ITEM_FEEDBAG = 0, PCR_SHOP_ITEM_FOOD_ALL, PCR_SHOP_ITEM_FOOD_HYOI, PCR_SHOP_ITEM_ARROW10,
    PCR_SHOP_ITEM_ARROW30, PCR_SHOP_ITEM_RED_BOTTLE, PCR_SHOP_ITEM_BLUE_BOTTLE, PCR_SHOP_ITEM_GREEN_BOTTLE,
    PCR_SHOP_ITEM_BOMB10, PCR_SHOP_ITEM_BOMB20, PCR_SHOP_ITEM_BOMB30, PCR_SHOP_ITEM_EMPTYBOTTLE,
    PCR_SHOP_ITEM_KAKERA_HEART, PCR_SHOP_ITEM_MAP, PCR_SHOP_ITEM_ROTEN0, /* RotenItem0..11 follow */
    PCR_SHOP_ITEM_COUNT = PCR_SHOP_ITEM_ROTEN0 + 12
};

/* Index of each shopItems_setData_* entry in PCR_TBL_SHOP_SET_DATA (declaration order of d_shop.cpp). */
enum {
    PCR_SHOP_SET_BOMB10_EXP = 0, PCR_SHOP_SET_BOMB20_EXP, PCR_SHOP_SET_BOMB30_EXP, PCR_SHOP_SET_BOMB10,
    PCR_SHOP_SET_BOMB20, PCR_SHOP_SET_BOMB30, PCR_SHOP_SET_FEEDBAG, PCR_SHOP_SET_FOOD_ALL,
    PCR_SHOP_SET_FOOD_HYOI, PCR_SHOP_SET_BOMB30_BS, PCR_SHOP_SET_ARROW10, PCR_SHOP_SET_ARROW30,
    PCR_SHOP_SET_RED_BOTTLE_BS, PCR_SHOP_SET_BOMB30_BS2, PCR_SHOP_SET_ARROW30_BS2,
    PCR_SHOP_SET_RED_BOTTLE_BS2, PCR_SHOP_SET_EMPTYBOTTLE, PCR_SHOP_SET_KAKERA_HEART, PCR_SHOP_SET_MAP,
    PCR_SHOP_SET_RED_BOTTLE_DS, PCR_SHOP_SET_GREEN_BOTTLE_DS, PCR_SHOP_SET_BLUE_BOTTLE_DS,
    PCR_SHOP_SET_ROTEN0, /* RotenItem0..11 follow */
    PCR_SHOP_SET_COUNT = PCR_SHOP_SET_ROTEN0 + 12
};

/* Longest single edit, in bytes. */
#define PC_RANDO_TABLE_EDIT_MAX 32

/* One bound table: `count` entries of `entry_size` bytes. The entries of a table need not be
   contiguous (the shop data are separate globals): `entries[i]` points at entry i. */
typedef struct PcRandoTableDesc {
    void* const* entries;
    int count;
    size_t entry_size;
} PcRandoTableDesc;

/* Binds the tables (indexed by PcRandoTable; a NULL `entries` leaves that table unbound) and adds the
   activation listener. Called once by the game side; again (the tests) re-binds. Returns 0 when a
   table is too large for the snapshot buffer. */
int pc_rando_items_bind_tables(const PcRandoTableDesc* tables, int count);

/* Registers an edit: `len` bytes at host offset `offset` of entry `index` of `table`. An edit of the
   same (table, index, offset, len) replaces the earlier one. Applied at every activation; applied at
   once when already active. Returns 0 on a bad table, index or range, or a full registry. */
int pc_rando_table_edit(int table, int index, size_t offset, const void* bytes, size_t len);
/* Drops every registered edit; while active, the tables go back to their snapshot. */
void pc_rando_table_edits_clear(void);
/* Number of registered edits. */
int pc_rando_table_edit_count(void);
/* 1 while the tables hold the snapshot plus the edits (between activation and deactivation). */
int pc_rando_tables_applied(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_ITEMS_H */
