/*
 * pc_rando_field_items.h - "any item anywhere": every item can be a field item under the randomizer
 * (docs/RANDOMIZER_PLAN.md step R2.11; a port of wwrando 1.10.0 tweaks.py
 * allow_all_items_to_be_field_items, which wwrando always applies).
 *
 * Most items have no field model (dItem_data::field_item_res is empty for them), so wwrando copies the
 * item-get model of item_resource into field_item_res for the ids of data/items_without_field_models.txt:
 *   - Master Swords 0x39/0x3A/0x3E copy the Hero's Sword 0x38, songs 0x6D-0x72 the Wind Waker 0x22,
 *     Magic Meter 0xB1/0xB2 the Green Potion 0x52, and their item_resource (the get model) is fixed the
 *     same way;
 *   - Hurricane Spin 0xAA: wwrando uses its own Vscroll.arc; the port ships no wwrando asset (plan D12)
 *     and uses the Wind Waker 0x22 for both models;
 *   - every other id copies its own item_resource entry.
 * Copied: the arc name and the model/animation indices with mTevFrm (wwrando's +8 0xD bytes into
 * field +4), and mUnknown (+0x1C into field +0x14). Not copied, as in wwrando: the field heap size.
 * item_info: a collision height or radius of 0 becomes 0x28, for all 256 ids.
 *
 * The edits go through R0.8's table-edit registry (pc_rando_items.h), so they are in the tables only
 * while pc_rando() is 1 and the boot bytes come back at deactivation. The code sites (d_a_item.cpp
 * itemGetExecute, mode_wait, getYOffset) are guarded hunks; mode_wait asks
 * pc_rando_field_item_uses_arrow.
 *
 * Pure (cos_rando_core): uses only the struct layouts of d/d_item_data.h, never its tables; the game
 * side passes them in (game_hooks/pc_rando_hooks_field_items.cpp). Game thread only.
 */
#ifndef PC_RANDO_FIELD_ITEMS_H
#define PC_RANDO_FIELD_ITEMS_H

#include <stdint.h>

struct dItem_data_item_resource;
struct dItem_data_field_item_res;
struct dItem_data_item_info;

/* The ids of wwrando's data/items_without_field_models.txt (171), ascending. */
const uint8_t* pc_rando_field_item_ids(int* count);

/* The item whose item_resource entry gives id's field model, or -1 when id is not in the list.
   *fixes_get_model (may be NULL) is set to 1 when id's own item_resource entry is rewritten too. */
int pc_rando_field_item_source(uint8_t id, int* fixes_get_model);

/* 1 for the 16 listed ids with no name in wwrando's file (0x2B, 0x2E, 0x54, 0x6C, 0xA0-0xA2, 0xB9-0xC1):
   unused item numbers, never in the item pool. Their entries are edited like the others, but their
   resources do not hold the models the tables name, so they cannot be created (smokes skip them). */
int pc_rando_field_item_unnamed(uint8_t id);

/* mode_wait under pc_rando(): 1 when field item `id` moves with itemActionForArrow (the physics of
   heart pieces), which wwrando gives ids 0x20-0x44 and every id above the switch's 0x4B, instead of
   the default itemActionForRupee. Other ids keep their vanilla case. */
int pc_rando_field_item_uses_arrow(uint8_t id);

/* getYOffset's value under pc_rando() for the switch's default case and for the Big Key (23.0; the
   original is 0.0). */
#define PC_RANDO_FIELD_ITEM_Y_OFFSET 23.0f
/* item_info's collision height/radius used where the original is 0. */
#define PC_RANDO_FIELD_ITEM_MIN_HR 0x28

/* Registers the table edits (pc_rando_table_edit), computed from the tables as they are now (the boot
   bytes): `res`, `field` and `info` are the 0x100-entry item_resource, field_item_res and item_info.
   Returns the number of edits registered, or -1 when the registry refused one (it is then incomplete
   and logged). Calling it again replaces the same edits (same keys). */
int pc_rando_field_items_register(const struct dItem_data_item_resource* res,
                                  const struct dItem_data_field_item_res* field,
                                  const struct dItem_data_item_info* info);

#endif /* PC_RANDO_FIELD_ITEMS_H */
