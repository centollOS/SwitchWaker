// cos_rando_test_R2_2 (docs/RANDOMIZER_PLAN.md step R2.2): the ACTR patcher
// (features/rando/pc_rando_place_actr.h) on synthetic room files, without the game:
//   - d_a_item: the placed item in params & 0xFF, behavior_type 0 -> 3, other behaviours kept;
//   - d_a_tsubo: the item in z_rot & 0xFF00 and dropped_item_id 0x3F (0 with item 0);
//   - d_a_obj_homen: the item in x_rot & 0xFF and dropped_item_id 0x3F;
//   - fix_deku_leaf_model on both Omori/Room0 layers, then the Forest Haven item;
//   - make_dungeon_joy_pendant_locations_flexible (pickup flags 0x16, destroyed switch 0xFF);
//   - D9: a wrong name or a wrong field value leaves the entry vanilla (record MISMATCH), a missing
//     entry gives BAD_INDEX, boss items (step R2.5) are not touched;
//   - with pc_rando() 0 nothing changes; the init registers the patcher once.
// The params values are synthetic (only the actor names and item ids of the key table are real).
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc/pc_rando_locs.h"
#include "pc_rando_dzx.h"
#include "pc_rando_place_actr.h"
#include "pc_rando_resources.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

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
uint32_t be32(const uint8_t* p) { return pc_rando_dzx_be32(p); }
uint16_t be16(const uint8_t* p) { return pc_rando_dzx_be16(p); }

struct Ent {
    const char* name;
    uint32_t params;
    uint16_t xrot, zrot;
};

// An ACTR-like entry list: `n` filler entries ("filler") with `set` entries at given indexes.
struct Chunk {
    const char* tag;
    uint32_t n;
    std::vector<std::pair<uint32_t, Ent>> set;
};

Bytes makeDzx(const std::vector<Chunk>& chunks) {
    size_t total = 4 + 12 * chunks.size();
    for (const Chunk& c : chunks) {
        total += c.n * 0x20;
    }
    Bytes b(total, 0);
    put32(&b[0], (uint32_t)chunks.size());
    size_t off = 4 + 12 * chunks.size();
    for (size_t i = 0; i < chunks.size(); i++) {
        const Chunk& c = chunks[i];
        uint8_t* row = &b[4 + 12 * i];
        memcpy(row, c.tag, 4);
        put32(row + 4, c.n);
        put32(row + 8, (uint32_t)off);
        for (uint32_t k = 0; k < c.n; k++) {
            memcpy(&b[off + k * 0x20], "filler", 6);
        }
        for (const auto& s : c.set) {
            uint8_t* e = &b[off + s.first * 0x20];
            memset(e, 0, 8);
            memcpy(e, s.second.name, strlen(s.second.name));
            put32(e + 0x08, s.second.params);
            put16(e + 0x18, s.second.xrot);
            put16(e + 0x1C, s.second.zrot);
        }
        off += c.n * 0x20;
    }
    return b;
}

void patch(Bytes& b, const char* stage, const char* arc) {
    PcRandoResFile f = {};
    f.dir = "";
    f.stage = stage;
    f.arc = arc;
    f.room = strncmp(arc, "Room", 4) == 0 ? atoi(arc + 4) : -1;
    f.name = f.room >= 0 ? "room.dzr" : "stage.dzs";
    f.type = f.room >= 0 ? PCR_RES_DZR : PCR_RES_DZS;
    f.data = b.data();
    f.size = b.size();
    pc_rando_place_actr_patch(&f);
}

const uint8_t* entry(Bytes& b, const char* tag, uint32_t i) {
    PcRandoDzx d;
    if (pc_rando_dzx_open(&d, b.data(), b.size()) != PCR_DZX_OK) {
        return nullptr;
    }
    return pc_rando_dzx_entry(&d, tag, i);
}

const PcRandoActrRecord* findRecord(const char* stage, const char* arc, const char* tag, uint16_t index,
                                     int kind = PCR_ACTR_LOCATION) {
    for (int i = 0; i < pc_rando_place_actr_count(); i++) {
        const PcRandoActrRecord* r = pc_rando_place_actr_record(i);
        if (r->kind == kind && strcmp(r->stage, stage) == 0 && strcmp(r->arc, arc) == 0 && strcmp(r->tag, tag) == 0 &&
            r->index == index) {
            return r;
        }
    }
    return nullptr;
}

uint8_t sItems[PCR_LOC_COUNT];

} // namespace

int main() {
    memset(sItems, 0xFF, sizeof sItems);
    sItems[PCR_LOC_HEADSTONE_ISLAND_TOP_OF_THE_ISLAND] = 0x0C;
    sItems[PCR_LOC_EARTH_TEMPLE_CASKET_IN_SECOND_CRYPT] = 0x0D;
    sItems[PCR_LOC_DRAGON_ROOST_CAVERN_POT_ON_UPPER_SHELF_IN_POT_ROOM] = 0x4E;
    sItems[PCR_LOC_FORBIDDEN_WOODS_HIGHEST_POT_IN_VINE_MAZE] = 0x00;
    sItems[PCR_LOC_WIND_TEMPLE_BEHIND_STONE_HEAD_IN_HIDDEN_UPPER_ROOM] = 0x22;
    sItems[PCR_LOC_FOREST_HAVEN_ON_TREE_BRANCH] = 0x23;
    sItems[PCR_LOC_EARTH_TEMPLE_BEHIND_CURTAIN_IN_WARP_POT_ROOM] = 0x30;
    sItems[PCR_LOC_EARTH_TEMPLE_BEHIND_CURTAIN_NEXT_TO_HAMMER_BUTTON] = 0x31;
    sItems[PCR_LOC_TOWER_OF_THE_GODS_POT_BEHIND_BOMBABLE_WALLS] = 0x32;
    sItems[PCR_LOC_EARTH_TEMPLE_JALHALLA_HEART_CONTAINER] = 0x33;

    // the init registers once
    const int before = pc_rando_res_patcher_count();
    pc_rando_place_actr_init();
    pc_rando_place_actr_init();
    RT_CHECK_EQ(pc_rando_res_patcher_count(), before + 1);

    // --- pc_rando() 0: nothing changes
    {
        RT_CHECK_EQ(pc_rando(), 0);
        Bytes b = makeDzx({{"ACTR", 20, {{19, {"item", 0x02AA1107u, 0, 0x00FF}}}}});
        const Bytes orig = b;
        patch(b, "sea", "Room45");
        RT_CHECK(b == orig);
        RT_CHECK_EQ(pc_rando_place_actr_count(), 0);
    }

    pc_rando_set_loc_items(sItems, PCR_LOC_COUNT);
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando(), 1);

    // --- d_a_item: Headstone (behaviour 2 kept), Earth Temple casket (behaviour 0 -> 3)
    {
        Bytes b = makeDzx({{"ACTR", 20, {{19, {"item", 0x02AA1107u, 0, 0x00FF}}}}});
        patch(b, "sea", "Room45");
        RT_CHECK_EQ(be32(entry(b, "ACTR", 19) + 8), 0x02AA110Cu);
        const PcRandoActrRecord* r = findRecord("sea", "Room45", "ACTR", 19);
        RT_CHECK(r != nullptr);
        if (r != nullptr) {
            RT_CHECK_EQ(r->result, (int)PCR_DZX_OK);
            RT_CHECK_EQ(r->loc, (int)PCR_LOC_HEADSTONE_ISLAND_TOP_OF_THE_ISLAND);
            RT_CHECK_EQ(r->item, 0x0C);
            RT_CHECK_EQ(pc_rando_place_actr_entry_item(r), 0x0C);
            RT_CHECK(memcmp(r->entry, entry(b, "ACTR", 19), 0x20) == 0);
        }
        // the other entries are untouched
        RT_CHECK(memcmp(entry(b, "ACTR", 18), "filler", 6) == 0);
        RT_CHECK_EQ(be32(entry(b, "ACTR", 18) + 8), 0u);
    }
    {
        Bytes b = makeDzx({{"ACTR", 9, {{8, {"item", 0x00123415u, 0, 0x8000}}}}});
        patch(b, "M_Dai", "Room8");
        RT_CHECK_EQ(be32(entry(b, "ACTR", 8) + 8), 0x0312340Du);
        RT_CHECK_EQ(be16(entry(b, "ACTR", 8) + 0x1C), 0x8000);
    }

    // --- d_a_tsubo: item in z_rot high byte, dropped 0x3F; with item 0 dropped 0 (+ the FW pickup flag)
    {
        Bytes b = makeDzx({{"ACTR", 36, {{35, {"ootubo1", 0x71235A1Fu, 0, 0x00FF}}}}});
        patch(b, "M_NewD2", "Room6");
        const uint8_t* e = entry(b, "ACTR", 35);
        RT_CHECK_EQ(be16(e + 0x1C), 0x4EFF);
        RT_CHECK_EQ(be32(e + 8), 0x71235A3Fu);
        const PcRandoActrRecord* r = findRecord("M_NewD2", "Room6", "ACTR", 35);
        RT_CHECK(r != nullptr && pc_rando_place_actr_entry_item(r) == 0x4E);
    }
    {
        Bytes b = makeDzx({{"ACTR", 225, {{224, {"ootubo1", 0x717FFF1Fu, 0, 0x00FF}}}}});
        patch(b, "kindan", "Room7");
        const uint8_t* e = entry(b, "ACTR", 224);
        RT_CHECK_EQ(be16(e + 0x1C), 0x00FF);
        RT_CHECK_EQ(be32(e + 8), 0x7116FF00u);
    }

    // --- d_a_obj_homen: item in x_rot low byte, dropped 0x3F, destroyed switch 0xFF (tweak)
    {
        Bytes b = makeDzx({{"ACTR", 17, {{16, {"Homen2", 0x8B3DF100u, 0x0000, 0x0000}}}}});
        patch(b, "kaze", "Room1");
        const uint8_t* e = entry(b, "ACTR", 16);
        RT_CHECK_EQ(be16(e + 0x18), 0x0022);
        RT_CHECK_EQ(be32(e + 8), 0xFF3FF100u);
        const PcRandoActrRecord* r = findRecord("kaze", "Room1", "ACTR", 16);
        RT_CHECK(r != nullptr && r->kind == PCR_ACTR_LOCATION && pc_rando_place_actr_entry_item(r) == 0x22);
        const PcRandoActrRecord* t = findRecord("kaze", "Room1", "ACTR", 16, PCR_ACTR_HOMEN_SWITCH);
        RT_CHECK(t != nullptr && t->result == PCR_DZX_OK && t->entry[8] == 0xFF);
    }

    // --- fix_deku_leaf_model on both layers, then the placed item
    {
        Bytes b = makeDzx({{"ACTR", 3, {}},
                           {"ACT0", 1, {{0, {"itemDek", 0x00000002u, 0, 0x1200}}}},
                           {"ACT1", 12, {{11, {"itemDek", 0x00000002u, 0, 0x0000}}}}});
        patch(b, "Omori", "Room0");
        for (const char* tag : {"ACT0", "ACT1"}) {
            const uint8_t* e = entry(b, tag, strcmp(tag, "ACT0") == 0 ? 0 : 11);
            RT_CHECK(memcmp(e, "item\0\0\0\0", 8) == 0);
            RT_CHECK_EQ(be32(e + 8), 0x01FF0223u);
            RT_CHECK_EQ(be16(e + 0x1C) & 0xFF, 0xFF);
        }
        RT_CHECK_EQ(be16(entry(b, "ACT0", 0) + 0x1C), 0x12FF);
        RT_CHECK(memcmp(entry(b, "ACTR", 0), "filler", 6) == 0);
        RT_CHECK(findRecord("Omori", "Room0", "ACT0", 0, PCR_ACTR_DEKU) != nullptr);
        const PcRandoActrRecord* r = findRecord("Omori", "Room0", "ACT1", 11);
        RT_CHECK(r != nullptr && r->kind == PCR_ACTR_LOCATION && r->result == PCR_DZX_OK && r->item == 0x23);
    }

    // --- the Tingle Bomb rupee of DRC: pickup flag 0x16, nothing else
    {
        Bytes b = makeDzx({{"ACTR", 0x1D, {{0x1C, {"item", 0x03AB0605u, 0, 0x00FF}}}}});
        patch(b, "M_NewD2", "Room0");
        RT_CHECK_EQ(be32(entry(b, "ACTR", 0x1C) + 8), 0x03AB1605u);
        const PcRandoActrRecord* r = findRecord("M_NewD2", "Room0", "ACTR", 0x1C, PCR_ACTR_JOY_FLAG);
        RT_CHECK(r != nullptr && r->result == PCR_DZX_OK);
        RT_CHECK(findRecord("M_NewD2", "Room0", "ACTR", 0x1C) == nullptr);
    }
    // a tweak whose entry has another name is not applied
    {
        Bytes b = makeDzx({{"ACTR", 0x2F, {{0x2E, {"Homen1", 0x973DF101u, 0, 0}}}}});
        const Bytes orig = b;
        patch(b, "kaze", "Room7");
        RT_CHECK(b == orig);
        const PcRandoActrRecord* r = findRecord("kaze", "Room7", "ACTR", 0x2E, PCR_ACTR_HOMEN_SWITCH);
        RT_CHECK(r != nullptr && r->result == PCR_DZX_MISMATCH);
        const PcRandoActrRecord* l = findRecord("kaze", "Room7", "ACTR", 0x2E);
        RT_CHECK(l != nullptr && l->result == PCR_DZX_MISMATCH);
    }

    // --- D9 and the other refusals
    {
        // wrong name
        Bytes b = makeDzx({{"ACTR", 17, {{16, {"kotubo", 0x0382001Fu, 0, 0}}}}});
        const Bytes orig = b;
        patch(b, "M_Dai", "Room2");
        RT_CHECK(b == orig);
        const PcRandoActrRecord* r = findRecord("M_Dai", "Room2", "ACTR", 16);
        RT_CHECK(r != nullptr && r->result == PCR_DZX_MISMATCH && r->item == 0x1F);
    }
    {
        // right name, other item than the disc's
        Bytes b = makeDzx({{"ACTR", 27, {{26, {"item", 0x03900120u, 0, 0}}}}});
        const Bytes orig = b;
        patch(b, "M_Dai", "Room13");
        RT_CHECK(b == orig);
        const PcRandoActrRecord* r = findRecord("M_Dai", "Room13", "ACTR", 26);
        RT_CHECK(r != nullptr && r->result == PCR_DZX_MISMATCH);
    }
    {
        // the entry is missing
        Bytes b = makeDzx({{"ACTR", 2, {}}});
        const Bytes orig = b;
        patch(b, "Siren", "Room3");
        RT_CHECK(b == orig);
        const PcRandoActrRecord* r = findRecord("Siren", "Room3", "ACTR", 4);
        RT_CHECK(r != nullptr && r->result == PCR_DZX_BAD_INDEX);
    }
    {
        // a boss item (R2.5) and a stage.dzs of another stage are not touched
        Bytes b = makeDzx({{"ACTR", 2, {{1, {"Bitem", 0x00000000u, 0, 0}}}}});
        const Bytes orig = b;
        patch(b, "M_DaiB", "Room0");
        RT_CHECK(b == orig);
        RT_CHECK(findRecord("M_DaiB", "Room0", "ACTR", 1) == nullptr);
        Bytes s = makeDzx({{"ACTR", 20, {{19, {"item", 0x02AA1107u, 0, 0}}}}});
        const Bytes sorig = s;
        patch(s, "sea", "Stage");
        RT_CHECK(s == sorig);
    }

    // a second load of the same room replaces its record
    {
        const int n = pc_rando_place_actr_count();
        Bytes b = makeDzx({{"ACTR", 20, {{19, {"item", 0x02AA1107u, 0, 0x00FF}}}}});
        patch(b, "sea", "Room45");
        RT_CHECK_EQ(pc_rando_place_actr_count(), n);
    }

    pc_rando_deactivate();
    pc_rando_set_loc_items(nullptr, 0);
    pc_rando_place_actr_reset();
    RT_CHECK_EQ(pc_rando_place_actr_count(), 0);
    return rt::finish();
}
