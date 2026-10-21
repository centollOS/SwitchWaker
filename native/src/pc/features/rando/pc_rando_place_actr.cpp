// pc_rando_place_actr.cpp - the ACTR part of the placement and its tweaks (docs/RANDOMIZER_PLAN.md
// step R2.2; pc_rando_place_actr.h). Pure: no game headers or symbols (cos_rando_core).
#include "pc_rando_place_actr.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_lockeys.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr uint8_t kDekuLeaf = 0x34;          // wwrando item_name_to_id["Deku Leaf"]
constexpr uint8_t kDekuPickupFlag = 2;        // fix_deku_leaf_model: itemDek's own flag
constexpr uint32_t kDekuParams = 0x01FF0000u; // fix_deku_leaf_model: behavior 1, spawn switch 0xFF
constexpr uint8_t kJoyFlag = 0x16;            // make_dungeon_joy_pendant_locations_flexible
constexpr int kMaxRecords = 64;

// ACTR entry layout (wwrando wwlib/dzx.py ACTR): name[8], params u32, x/y/z f32, x_rot, y_rot, z_rot
// u16, enemy number u16.
constexpr uint32_t kParamsOff = 0x08;
constexpr uint32_t kZRotOff = 0x1C;
constexpr uint32_t kEntrySize = 0x20;

PcRandoActrRecord sRecords[kMaxRecords];
int sCount = 0;
bool sRegistered = false;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

uint32_t be32(const uint8_t* p) { return pc_rando_dzx_be32(p); }
uint16_t be16(const uint8_t* p) { return pc_rando_dzx_be16(p); }
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

bool nameIs(const uint8_t* entry, const char* name) {
    char n[8] = {};
    strncpy(n, name, sizeof n);
    return memcmp(entry, n, sizeof n) == 0;
}

// The record of an entry and kind (an existing one for the same entry and kind is reused).
PcRandoActrRecord* recordFor(const PcRandoResFile* f, int kind, const char* tag, uint16_t index) {
    for (int i = 0; i < sCount; i++) {
        PcRandoActrRecord& r = sRecords[i];
        if (r.index == index && r.kind == kind && strcmp(r.tag, tag) == 0 && strcmp(r.arc, f->arc) == 0 &&
            strcmp(r.stage, f->stage) == 0) {
            return &r;
        }
    }
    if (sCount >= kMaxRecords) {
        return nullptr;
    }
    PcRandoActrRecord& r = sRecords[sCount++];
    memset(&r, 0, sizeof r);
    snprintf(r.stage, sizeof r.stage, "%s", f->stage);
    snprintf(r.arc, sizeof r.arc, "%s", f->arc);
    snprintf(r.tag, sizeof r.tag, "%s", tag);
    r.index = index;
    r.kind = kind;
    return &r;
}

void record(const PcRandoResFile* f, int kind, int loc, int key, const char* tag, uint16_t index, int result,
            uint8_t item, const uint8_t* entry) {
    PcRandoActrRecord* r = recordFor(f, kind, tag, index);
    if (r == nullptr) {
        return;
    }
    r->kind = kind;
    r->loc = loc;
    r->key = key;
    r->result = result;
    r->item = item;
    if (entry != nullptr) {
        memcpy(r->entry, entry, kEntrySize);
    } else {
        memset(r->entry, 0, kEntrySize);
    }
}

bool fileIsArc(const PcRandoResFile* f, const char* stage, const char* arc) {
    if (strcmp(f->stage, stage) != 0 || strcmp(f->arc, arc) != 0) {
        return false;
    }
    // stage.dzs lives in Stage.arc, room.dzr in Room<n>.arc
    return strcmp(arc, "Stage") == 0 ? f->type == PCR_RES_DZS : f->type == PCR_RES_DZR;
}

// --- tweaks ---------------------------------------------------------------------------------

struct Tweak {
    int kind;
    const char* stage;
    const char* arc;
    uint16_t index; // default-layer ACTR
    const char* name;
};

// make_dungeon_joy_pendant_locations_flexible (tweaks.py)
const Tweak kTweaks[] = {
    {PCR_ACTR_JOY_FLAG, "M_NewD2", "Room0", 0x1C, "item"},    // the Tingle Bomb purple rupee
    {PCR_ACTR_JOY_FLAG, "kindan", "Room7", 0xE0, "ootubo1"},  // the pot without a pickup flag
    {PCR_ACTR_HOMEN_SWITCH, "kaze", "Room1", 0x10, "Homen2"}, // stone heads: always come back
    {PCR_ACTR_HOMEN_SWITCH, "kaze", "Room7", 0x2E, "Homen2"},
};

void applyTweak(PcRandoResFile* f, PcRandoDzx* dzx, const Tweak& t) {
    uint8_t* e = pc_rando_dzx_entry(dzx, "ACTR", t.index);
    if (e == nullptr) {
        logf("[cos] rando actr: %s/%s ACTR[0x%X] missing (tweak not applied)\n", t.stage, t.arc, t.index);
        record(f, t.kind, -1, -1, "ACTR", t.index, PCR_DZX_BAD_INDEX, 0, nullptr);
        return;
    }
    if (!nameIs(e, t.name)) {
        logf("[cos] rando actr: %s/%s ACTR[0x%X] is %.8s, not %s (D9: tweak not applied)\n", t.stage, t.arc,
             t.index, (const char*)e, t.name);
        record(f, t.kind, -1, -1, "ACTR", t.index, PCR_DZX_MISMATCH, 0, e);
        return;
    }
    uint32_t p = be32(e + kParamsOff);
    if (t.kind == PCR_ACTR_HOMEN_SWITCH) {
        p = (p & 0x00FFFFFFu) | 0xFF000000u; // d_a_obj_homen destroyed_switch
    } else if (strcmp(t.name, "item") == 0) {
        p = (p & ~0x0000FF00u) | ((uint32_t)kJoyFlag << 8); // d_a_item item_pickup_flag
    } else {
        p = (p & ~0x007F0000u) | ((uint32_t)kJoyFlag << 16); // d_a_tsubo item_pickup_flag
    }
    put32(e + kParamsOff, p);
    record(f, t.kind, -1, -1, "ACTR", t.index, PCR_DZX_OK, 0, e);
}

// fix_deku_leaf_model: every itemDek of Omori/Room0, on every layer.
void applyDeku(PcRandoResFile* f, PcRandoDzx* dzx) {
    for (int layer = -1; layer < 12; layer++) {
        char tag[5];
        if (!pc_rando_dzx_tag("ACTR", layer, tag)) {
            continue;
        }
        const uint32_t n = pc_rando_dzx_count(dzx, tag);
        for (uint32_t i = 0; i < n; i++) {
            uint8_t* e = pc_rando_dzx_entry(dzx, tag, i);
            if (e == nullptr || !nameIs(e, "itemDek")) {
                continue;
            }
            memset(e, 0, 8);
            memcpy(e, "item", 4);
            put32(e + kParamsOff, kDekuParams | (uint32_t)kDekuPickupFlag << 8 | kDekuLeaf);
            put16(e + kZRotOff, (uint16_t)((be16(e + kZRotOff) & 0xFF00u) | 0x00FFu)); // enable_activation_switch
            record(f, PCR_ACTR_DEKU, -1, -1, tag, (uint16_t)i, PCR_DZX_OK, kDekuLeaf, e);
        }
    }
}

// --- locations ------------------------------------------------------------------------------

bool isActrTag(const char* tag) { return tag != nullptr && strncmp(tag, "ACT", 3) == 0; }

bool handledField(pcr::DzxField f) {
    return f == pcr::DzxField::ItemItem || f == pcr::DzxField::TsuboItem || f == pcr::DzxField::HomenItem ||
           f == pcr::DzxField::DekuItem;
}

bool fingerprintOk(const pcr::LocKey& key, const uint8_t* e) {
    if (key.field == pcr::DzxField::DekuItem) {
        // checked after fix_deku_leaf_model rewrote the entry (see the header)
        return nameIs(e, "item") && pcr::dzx_key_read(key, e) == kDekuLeaf;
    }
    return pcr::dzx_key_fingerprint_ok(key, e);
}

void writeItem(const pcr::LocKey& key, uint8_t* e, uint8_t item) {
    pcr::dzx_key_write(key, e, item);
    uint32_t p = be32(e + kParamsOff);
    switch (key.field) {
    case pcr::DzxField::ItemItem:
    case pcr::DzxField::DekuItem:
        if (((p >> 24) & 3u) == 0) {
            p |= 0x03000000u; // behavior_type 0 (fades out) -> 3 (does not)
        }
        break;
    case pcr::DzxField::TsuboItem:
        // item 0 is "none" for the added z_rot parameter: the vanilla dropped_item_id gives the heart
        p = (p & ~0x3Fu) | (item == 0 ? 0u : 0x3Fu);
        break;
    case pcr::DzxField::HomenItem:
        p = (p & ~0x3F000u) | (item == 0 ? 0u : 0x3F000u);
        break;
    default:
        break;
    }
    put32(e + kParamsOff, p);
}

void applyLocations(PcRandoResFile* f, PcRandoDzx* dzx) {
    const pcr::Span<pcr::LocKey> keys = pcr::location_keys();
    for (size_t k = 0; k < keys.size; k++) {
        const pcr::LocKey& key = keys[k];
        if (key.kind != pcr::KeyKind::Dzx || !isActrTag(key.tag) || !handledField(key.field) ||
            !fileIsArc(f, key.stage, key.arc)) {
            continue;
        }
        const uint8_t item = pc_rando_loc_item(key.loc, key.vanilla);
        uint8_t* e = pc_rando_dzx_entry(dzx, key.tag, key.index);
        if (e == nullptr) {
            logf("[cos] rando actr: %s/%s %s[%u] missing (location %d left vanilla)\n", key.stage, key.arc, key.tag,
                 key.index, key.loc);
            record(f, PCR_ACTR_LOCATION, key.loc, (int)k, key.tag, key.index, PCR_DZX_BAD_INDEX, key.vanilla, nullptr);
            continue;
        }
        if (!fingerprintOk(key, e)) {
            logf("[cos] rando actr: %s/%s %s[%u] is %.8s field 0x%X (D9: location %d left vanilla)\n", key.stage,
                 key.arc, key.tag, key.index, (const char*)e, pcr::dzx_key_read(key, e), key.loc);
            record(f, PCR_ACTR_LOCATION, key.loc, (int)k, key.tag, key.index, PCR_DZX_MISMATCH, key.vanilla, e);
            continue;
        }
        writeItem(key, e, item);
        record(f, PCR_ACTR_LOCATION, key.loc, (int)k, key.tag, key.index, PCR_DZX_OK, item, e);
    }
}

} // namespace

extern "C" void pc_rando_place_actr_patch(PcRandoResFile* f) {
    if (!pc_rando() || f == nullptr || f->stage == nullptr || f->stage[0] == '\0' || f->arc == nullptr) {
        return;
    }
    if (f->type != PCR_RES_DZS && f->type != PCR_RES_DZR) {
        return;
    }
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, f->data, f->size) != PCR_DZX_OK) {
        return;
    }
    for (const Tweak& t : kTweaks) {
        if (fileIsArc(f, t.stage, t.arc)) {
            applyTweak(f, &dzx, t);
        }
    }
    if (fileIsArc(f, "Omori", "Room0")) {
        applyDeku(f, &dzx);
    }
    applyLocations(f, &dzx);
}

extern "C" void pc_rando_place_actr_init(void) {
    if (sRegistered) {
        return;
    }
    sRegistered = pc_rando_register_dzx_patcher(pc_rando_place_actr_patch) != 0;
}

extern "C" int pc_rando_place_actr_count(void) { return sCount; }

extern "C" const PcRandoActrRecord* pc_rando_place_actr_record(int i) {
    return i >= 0 && i < sCount ? &sRecords[i] : nullptr;
}

extern "C" void pc_rando_place_actr_reset(void) { sCount = 0; }

extern "C" uint8_t pc_rando_place_actr_entry_item(const PcRandoActrRecord* r) {
    if (r == nullptr) {
        return 0xFF;
    }
    if (r->kind == PCR_ACTR_LOCATION && r->key >= 0) {
        const pcr::LocKey& key = pcr::location_keys()[(size_t)r->key];
        return (uint8_t)pcr::dzx_key_read(key, r->entry);
    }
    return r->entry[kParamsOff + 3];
}
