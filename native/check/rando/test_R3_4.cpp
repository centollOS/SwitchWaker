// R3.4 (docs/RANDOMIZER_PLAN.md): the per-dungeon item IDs, pure part (pc_rando_dungeon_items.h).
//  1. the 22-entry table matches tweaks.py allow_dungeon_items_to_appear_anywhere: IDs in order, the
//     dungeon's stage (custom_funcs.asm wrappers), kind, base item, the "DRC Small Key" names;
//  2. each message text is tweaks.py's description string (built here independently from
//     Logic.DUNGEON_NAMES of R0.3's table and get_indefinite_article) with only some spaces turned
//     into line breaks, at most 2 lines (one ITEM_GET box);
//  3. the decisions of generic_on_dungeon_bit and generic_small_key_item_get_func;
//  4. the 22 overlay messages: number 101 + id, ITEM_GET box, slow draw, the item's image, the
//     encoded text decodes back to the table's text; all cleared when pc_rando() turns 0.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_data.h"
#include "pc_rando_dungeon_items.h"
#include "pc_rando_msg.h"

#include <cstring>
#include <set>
#include <string>

namespace {

struct Expect {
    uint8_t id;
    const char* dungeon;
    const char* base;
};

// tweaks.py dungeon_items, verbatim order.
const Expect kExpect[PCR_DITEM_COUNT] = {
    {0x13, "DRC", "Small Key"},  {0x14, "DRC", "Big Key"},    {0x1B, "DRC", "Dungeon Map"},
    {0x1C, "DRC", "Compass"},    {0x1D, "FW", "Small Key"},   {0x40, "FW", "Big Key"},
    {0x41, "FW", "Dungeon Map"}, {0x5A, "FW", "Compass"},     {0x5B, "TotG", "Small Key"},
    {0x5C, "TotG", "Big Key"},   {0x5D, "TotG", "Dungeon Map"}, {0x5E, "TotG", "Compass"},
    {0x5F, "FF", "Dungeon Map"}, {0x60, "FF", "Compass"},     {0x73, "ET", "Small Key"},
    {0x74, "ET", "Big Key"},     {0x75, "ET", "Dungeon Map"}, {0x76, "ET", "Compass"},
    {0x77, "WT", "Small Key"},   {0x81, "WT", "Big Key"},     {0x84, "WT", "Dungeon Map"},
    {0x85, "WT", "Compass"},
};

int stageOf(const std::string& d) {
    if (d == "FF") return 2;
    if (d == "DRC") return 3;
    if (d == "FW") return 4;
    if (d == "TotG") return 5;
    if (d == "ET") return 6;
    if (d == "WT") return 7;
    return -1;
}

const char* dungeonName(const char* shortName) {
    for (const pcr::DungeonName& d : pcr::dungeon_names()) {
        if (strcmp(d.short_name, shortName) == 0) {
            return d.name;
        }
    }
    return nullptr;
}

std::string description(const char* dungeon, const std::string& base) {
    const std::string name = dungeon;
    if (base == "Small Key") {
        const char c = (char)tolower((unsigned char)name[0]);
        const char* article = strchr("aeiou", c) != nullptr ? "an" : "a";
        return std::string("\\{1A 05 00 00 01}You got ") + article + " \\{1A 06 FF 00 00 01}" + name +
               " small key\\{1A 06 FF 00 00 00}!";
    }
    return "\\{1A 05 00 00 01}You got the \\{1A 06 FF 00 00 01}" + name + " " + base + "\\{1A 06 FF 00 00 00}!";
}

void checkTable() {
    std::set<int> ids;
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* e = pc_rando_dungeon_item(i);
        const Expect& x = kExpect[i];
        RT_CHECK(e != nullptr);
        if (e == nullptr) {
            continue;
        }
        RT_CHECK_EQ((int)e->id, (int)x.id);
        RT_CHECK(strcmp(e->short_name, x.dungeon) == 0);
        RT_CHECK_EQ((int)e->stage, stageOf(x.dungeon));
        RT_CHECK(std::string(e->name) == std::string(x.dungeon) + " " + x.base);
        const std::string base = x.base;
        const int kind = base == "Small Key" ? PCR_DITEM_SMALL_KEY
                         : base == "Big Key" ? PCR_DITEM_BIG_KEY
                         : base == "Dungeon Map" ? PCR_DITEM_MAP
                                                 : PCR_DITEM_COMPASS;
        const int baseId = base == "Small Key" ? 0x15 : base == "Big Key" ? 0x4E : base == "Dungeon Map" ? 0x4C : 0x4D;
        RT_CHECK_EQ((int)e->kind, kind);
        RT_CHECK_EQ((int)e->base_id, baseId);
        RT_CHECK(pc_rando_dungeon_item_find(e->id) == e);
        ids.insert(e->id);

        // The text is the description with some spaces made line breaks (wwrando's word wrap).
        const char* dn = dungeonName(x.dungeon);
        RT_CHECK(dn != nullptr);
        if (dn == nullptr) {
            continue;
        }
        const std::string want = description(dn, base);
        std::string got = e->text;
        RT_CHECK_EQ(got.size(), want.size());
        int lines = 1;
        bool sameButBreaks = got.size() == want.size();
        for (size_t k = 0; sameButBreaks && k < got.size(); k++) {
            if (got[k] == '\n') {
                lines++;
                sameButBreaks = want[k] == ' ';
            } else {
                sameButBreaks = got[k] == want[k];
            }
        }
        RT_CHECK(sameButBreaks);
        RT_CHECK(lines <= 2);
    }
    RT_CHECK_EQ(ids.size(), (size_t)PCR_DITEM_COUNT);
    RT_CHECK(pc_rando_dungeon_item(-1) == nullptr);
    RT_CHECK(pc_rando_dungeon_item(PCR_DITEM_COUNT) == nullptr);
    RT_CHECK(pc_rando_dungeon_item_find(0x15) == nullptr); // the vanilla small key stays vanilla
    RT_CHECK(pc_rando_dungeon_item_find(0x4E) == nullptr);
    // Texts the wrap must keep on one line, and two it must break (wwrando's output for the USA font).
    RT_CHECK(strchr(pc_rando_dungeon_item_find(0x73)->text, '\n') == nullptr);
    RT_CHECK(strstr(pc_rando_dungeon_item_find(0x13)->text, "Cavern\nsmall key") != nullptr);
    RT_CHECK(strstr(pc_rando_dungeon_item_find(0x84)->text, "Dungeon\nMap") != nullptr);
}

void checkDecisions() {
    // generic_on_dungeon_bit
    RT_CHECK_EQ(pc_rando_dungeon_bit_target(3, 3), PCR_DITEM_TARGET_CURRENT);
    RT_CHECK_EQ(pc_rando_dungeon_bit_target(0, 3), PCR_DITEM_TARGET_SAVED);
    RT_CHECK_EQ(pc_rando_dungeon_bit_target(4, 3), PCR_DITEM_TARGET_SAVED);
    RT_CHECK_EQ(pc_rando_dungeon_bit_target(-1, 2), PCR_DITEM_TARGET_SAVED);
    // generic_small_key_item_get_func
    RT_CHECK_EQ(pc_rando_dungeon_key_target(3, 3, 1), PCR_DITEM_KEY_ITEM_FUNC);
    RT_CHECK_EQ(pc_rando_dungeon_key_target(3, 3, 0), PCR_DITEM_TARGET_CURRENT);
    RT_CHECK_EQ(pc_rando_dungeon_key_target(0, 3, 1), PCR_DITEM_TARGET_SAVED);
    RT_CHECK_EQ(pc_rando_dungeon_key_target(0, 3, 0), PCR_DITEM_TARGET_SAVED);
    RT_CHECK_EQ(pc_rando_dungeon_key_target(4, 3, 1), PCR_DITEM_TARGET_SAVED);
}

void checkMessages() {
    pc_rando_set_active(nullptr, nullptr);
    pc_rando_msg_clear();
    RT_CHECK_EQ(pc_rando_dungeon_items_add_messages(), PCR_DITEM_COUNT);
    RT_CHECK_EQ(pc_rando_msg_count(), PCR_DITEM_COUNT);
    for (int i = 0; i < PCR_DITEM_COUNT; i++) {
        const PcRandoDungeonItem* e = pc_rando_dungeon_item(i);
        const PcRandoMsg* m = pc_rando_msg_find((uint16_t)(101 + e->id));
        RT_CHECK(m != nullptr);
        if (m == nullptr) {
            continue;
        }
        PcRandoMsgEntry def;
        pc_rando_msg_entry_defaults(&def);
        RT_CHECK(m->has_entry);
        RT_CHECK_EQ((int)m->entry.text_box_type, 9);
        RT_CHECK_EQ((int)m->entry.initial_draw_type, 2);
        RT_CHECK_EQ((int)m->entry.display_item_id, (int)e->id);
        RT_CHECK_EQ((int)m->entry.text_box_position, (int)def.text_box_position);
        RT_CHECK_EQ((int)m->entry.num_lines_per_box, (int)def.num_lines_per_box);
        char back[256];
        pc_rando_msg_decode(m->bytes, m->len, back, sizeof back);
        RT_CHECK(strcmp(back, e->text) == 0);
        const char* hook = pc_rando_msg_hook_text((uint16_t)(101 + e->id));
        RT_CHECK(hook != nullptr && memcmp(hook, m->bytes, m->len) == 0);
    }
    // Adding again replaces, it does not duplicate.
    RT_CHECK_EQ(pc_rando_dungeon_items_add_messages(), PCR_DITEM_COUNT);
    RT_CHECK_EQ(pc_rando_msg_count(), PCR_DITEM_COUNT);
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando_msg_count(), 0);
}

} // namespace

int main() {
    checkTable();
    checkDecisions();
    checkMessages();
    return rt::finish();
}
