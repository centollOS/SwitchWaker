/*
 * pc_rando_lockeys.h - runtime addressing of the 320 item locations (docs/RANDOMIZER_PLAN.md, R0.5).
 *
 * Every wwrando item path (logic/item_locations.txt "Paths", randomizers/items.py change_item) has one
 * key, in pcr::loc_paths() order: key i is path i of R0.3's table, so location_keys(loc) is the
 * slice [first_path, first_path + num_paths). The table is generated from wwrando and the user's
 * disc by native/tools/rando/gen_location_keys.py into generated/rando_location_keys.inc; the ids
 * are PCR_LOC_* (pc/pc_rando_locs.h).
 *
 * Key kinds (plan R0.5):
 *   Dzx    an entry of a stage.dzs/room.dzr chunk: stage dir, arc ("Stage" | "Room<n>"), chunk tag
 *          (TRES/TRE0-b, ACTR/ACT0-b, SCOB/SCO0-b) and index in that chunk, the item field of the
 *          entry's actor class (big-endian word at word_off, width 2 or 4, mask), and the D9
 *          fingerprint: fp_name (8-byte actor name as on the disc) plus disc_byte (the field's value
 *          on the disc; the vanilla item when KEY_FP_ITEM, else whatever the unused field holds).
 *   Event  an action of event_list.dat: event name/index, actor slot, action ordinal (wwrando's
 *          path), plus the resolved global action index, the integer-list slot of its prm0, and the
 *          vanilla action name and prm0 (fingerprint; songs use 059get_dance with item - 0x6D).
 *   Code   a byte wwrando patches in a REL, main.dol or its own asm (CustomSymbol). The port reads
 *          these through pc_rando_loc_item(PCR_LOC_*, vanilla) at the decompiled site, so the key
 *          only names the source; disc_byte is the REL/DOL byte (KEY_FP_ITEM when it is the item).
 *   Added  a chest wwrando adds (PCR_ADDED_*): stage, arc, the TRES index it will have.
 *
 * Pure: no game headers, no allocation (cos_rando_core).
 */
#ifndef PC_RANDO_LOCKEYS_H
#define PC_RANDO_LOCKEYS_H

#include <stddef.h>
#include <stdint.h>

#include "pc/pc_rando_locs.h"
#include "pc_rando_data.h"

namespace pcr {

enum class KeyKind : uint8_t { Dzx, Event, Code, Added };

// The item field of the actor class at a Dzx key (wwrando data/actor_parameters.txt item_id).
enum class DzxField : uint8_t {
    None,
    TboxItem,    // d_a_tbox        z_rot  & 0xFF00
    ItemItem,    // d_a_item        params & 0x000000FF
    BossItem,    // d_a_boss_item   params & 0x0000FF00 (field added by wwrando)
    TsuboItem,   // d_a_tsubo       z_rot  & 0xFF00     (added; drop params & 0x3F becomes 0x3F)
    HomenItem,   // d_a_obj_homen   x_rot  & 0x00FF     (added; drop params & 0x3F000 becomes 0x3F)
    SalvageItem, // d_a_salvage     params & 0x00000FF0
    TagKbItem,   // d_a_tag_kb_item params & 0x000000FF
    DekuItem,    // d_a_deku_item: tweaks.py turns it into an "item" actor (params & 0xFF)
};

enum class CodeSource : uint8_t { None, Rel, MainDol, CustomSymbol };

// LocKey::flags
enum : uint8_t {
    KEY_FP_ITEM = 1u << 0, // disc_byte (Dzx/Code) or prm0 (Event) is the location's vanilla item
};

// Location classes (the locations report's per-location classes are these bits joined by '+').
enum : uint8_t {
    PCR_CLASS_CHEST = 1u << 0,
    PCR_CLASS_ADDED = 1u << 1,
    PCR_CLASS_ACTR = 1u << 2,
    PCR_CLASS_SCOB = 1u << 3,
    PCR_CLASS_EVENT = 1u << 4,
    PCR_CLASS_CODE = 1u << 5,
    PCR_CLASS_CUSTOM = 1u << 6,
};

struct LocKey {
    KeyKind kind;
    uint16_t loc;        // PCR_LOC_*
    uint8_t vanilla;     // the location's original item id
    uint8_t flags;       // KEY_FP_*
    // Dzx, Event, Added
    const char* stage;   // stage dir, "sea", "majroom", ...
    const char* arc;     // "Stage" or "Room<n>"
    // Dzx, Added
    const char* tag;     // chunk fourcc
    uint16_t index;      // index in that chunk
    DzxField field;
    uint8_t word_off;    // offset of the big-endian field word in the entry
    uint8_t width;       // 2 or 4 bytes
    uint32_t mask;       // field bits in that word
    uint8_t fp_name[8];  // Dzx: the entry's name bytes on the disc
    uint8_t disc_byte;   // Dzx: field value on the disc; Code: REL/DOL byte
    // Event
    const char* event;   // event name
    uint16_t event_index;
    uint16_t actor;      // actor slot in the event
    uint16_t action;     // action ordinal along the actor's chain
    uint16_t gaction;    // global action index in event_list.dat
    uint16_t int_index;  // integer-list slot of the action's prm0
    const char* action_name; // vanilla action name ("011get_item", "059get_dance", "046pget")
    int32_t prm0;        // vanilla prm0
    // Code
    CodeSource src;
    const char* file;    // "rels/<x>.rel", "main.dol", or the CustomSymbol name
    uint32_t offset;     // REL offset or main.dol address
    // Added
    uint8_t added;       // PCR_ADDED_*
};

struct LocInfo {
    const char* enum_name; // "PCR_LOC_OUTSET_ISLAND_UNDERNEATH_LINKS_HOUSE"
    uint8_t classes;       // PCR_CLASS_*
};

// All keys, loc_paths() order (511).
Span<LocKey> location_keys();
// The keys of one location (PCR_LOC_*); empty when out of range.
Span<LocKey> location_keys(int loc);
// PCR_LOC_* name and classes; nullptr when out of range.
const LocInfo* location_info(int loc);
// "CHEST", "ACTR+CODE", ... (the locations report's notation) into buf; returns buf.
const char* location_class_name(int loc, char* buf, size_t size);

// Dzx field access on one raw (big-endian, disc-format) chunk entry of at least word_off + width
// bytes. Read gives the field value (shifted down); write puts `value` in the field bits only.
uint32_t dzx_key_read(const LocKey& key, const uint8_t* entry);
void dzx_key_write(const LocKey& key, uint8_t* entry, uint32_t value);
// D9: the entry is the one the key was made from (same 8-byte name and same field value as on the
// disc). False means replaced content: leave the location vanilla and log it.
bool dzx_key_fingerprint_ok(const LocKey& key, const uint8_t* entry);

} // namespace pcr

#endif
