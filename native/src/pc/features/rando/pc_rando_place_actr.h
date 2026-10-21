/*
 * pc_rando_place_actr.h - the ACTR part of the placement (docs/RANDOMIZER_PLAN.md step R2.2): field
 * items, pots and stone heads, plus the three tweaks wwrando always applies to those actors.
 *
 * One dzx patcher (pc_rando_place_actr_init, listed in pc_rando_patchers.def) edits stage.dzs /
 * room.dzr while dRes_info_c::loadResource still holds the disc's bytes (plan D8). For a file of the
 * stage and archive of a Dzx key (pc_rando_lockeys.h) on an ACTR/ACT0-b chunk it writes the placed
 * item (pc_rando_loc_item) as randomizers/items.py change_actor_item does:
 *   - d_a_item (DzxField::ItemItem): params & 0xFF = item; behavior_type (params & 0x03000000) 0
 *     ("fades out") becomes 3 ("does not fade out");
 *   - d_a_tsubo (TsuboItem): z_rot & 0xFF00 = item (the parameter wwrando adds, read by the
 *     daTsubo::Act_c::damaged hunk); dropped_item_id (params & 0x3F) = 0x3F, or 0 with item 0;
 *   - d_a_obj_homen (HomenItem): x_rot & 0x00FF = item (read by the daObjHomen::Act_c::make_item
 *     hunk); dropped_item_id (params & 0x3F000) = 0x3F, or 0 with item 0;
 *   - the Forest Haven Deku Leaf (DekuItem): see fix_deku_leaf_model below; then as d_a_item.
 * Boss items (DzxField::BossItem) belong to step R2.5 and are not touched.
 *
 * Tweaks (tweaks.py, applied for every randomized file whatever the placement):
 *   - fix_deku_leaf_model: every "itemDek" ACTR (all layers) of Omori/Room0 becomes an "item" with
 *     params 0x01FF0000 | pickup flag 2 << 8 | Deku Leaf (0x34) and z_rot & 0xFF = 0xFF
 *     (enable_activation_switch), so any item can be placed there with a field model;
 *   - make_dungeon_joy_pendant_locations_flexible: M_NewD2/Room0 ACTR[0x1C] (d_a_item) pickup flag
 *     0x16; kindan/Room7 ACTR[0xE0] (d_a_tsubo) pickup flag 0x16; kaze/Room1 ACTR[0x10] and
 *     kaze/Room7 ACTR[0x2E] (d_a_obj_homen) destroyed_switch 0xFF.
 * (fix_stone_head_bugs is a hunk in daObjHomen::Act_c::_create; see docs/RANDO_HOOKS.md.)
 *
 * Fingerprints (plan D9): a location's entry must hold the key's 8-byte name and disc field value;
 * a tweak's entry must hold the expected name. On a mismatch the entry is left as it is and logged.
 * The Deku keys are checked after the Deku tweak: name "item" and item 0x34, the values the tweak
 * wrote (the key itself records "itemDek" and the pickup flag 2, the disc's field byte).
 *
 * Every write is recorded (pc_rando_place_actr_record), so smokes can find the patched entries and
 * their live actors. Pure: no game headers (cos_rando_core).
 */
#ifndef PC_RANDO_PLACE_ACTR_H
#define PC_RANDO_PLACE_ACTR_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the patcher (pc_rando_patchers.def). Safe to call more than once. */
void pc_rando_place_actr_init(void);
/* The patcher itself (tests call it directly). Does nothing while pc_rando() is 0. */
void pc_rando_place_actr_patch(PcRandoResFile* file);

enum PcRandoActrKind {
    PCR_ACTR_LOCATION = 0, /* a location's item */
    PCR_ACTR_DEKU,         /* fix_deku_leaf_model */
    PCR_ACTR_JOY_FLAG,     /* make_dungeon_joy_pendant_locations_flexible: pickup flag 0x16 */
    PCR_ACTR_HOMEN_SWITCH, /* make_dungeon_joy_pendant_locations_flexible: destroyed switch 0xFF */
};

/* One patched (or refused) entry and kind (an entry can have a tweak and a location record). A later
   load of the same entry replaces its records. */
typedef struct PcRandoActrRecord {
    int kind;          /* PcRandoActrKind */
    int loc;           /* PCR_LOC_* for PCR_ACTR_LOCATION, else -1 */
    int key;           /* index in pcr::location_keys() for PCR_ACTR_LOCATION, else -1 */
    char stage[16];
    char arc[16];      /* "Stage" or "Room<n>" */
    char tag[5];       /* "ACTR", "ACT0".. */
    uint16_t index;    /* entry index in that chunk */
    int result;        /* PCR_DZX_OK, PCR_DZX_MISMATCH, PCR_DZX_BAD_INDEX, ... */
    uint8_t item;      /* PCR_ACTR_LOCATION: the item written (the vanilla one when not OK) */
    uint8_t entry[0x20]; /* the entry's bytes after the patch (disc format) */
} PcRandoActrRecord;

int pc_rando_place_actr_count(void);
const PcRandoActrRecord* pc_rando_place_actr_record(int i);
/* Forgets every record (tests). */
void pc_rando_place_actr_reset(void);

/* The live actor's item parameter of a record, as the hunks read it: d_a_item params & 0xFF,
   d_a_tsubo z_rot >> 8, d_a_obj_homen x_rot & 0xFF (from the recorded entry). */
uint8_t pc_rando_place_actr_entry_item(const PcRandoActrRecord* rec);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_PLACE_ACTR_H */
