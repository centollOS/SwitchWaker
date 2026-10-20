// The randomizer's per-dungeon item IDs, pure part (features/rando/pc_rando_dungeon_items.h,
// docs/RANDOMIZER_PLAN.md step R3.4). No game headers or game calls: cos_rando_core links this file
// into the unit tests. The game side is game_hooks/pc_rando_hooks_dungeon_items.cpp.
#include "pc_rando_dungeon_items.h"
#include "pc_rando_msg.h"

#include <cstddef>

namespace {

// tweaks.py allow_dungeon_items_to_appear_anywhere, in its order. The stage numbers are the ones
// custom_funcs.asm's 22 wrappers pass (li r3, <stage>). The texts are the description strings of
// that function after msg.word_wrap_string(self.bfn): wwrando wraps them with the USA font's glyph
// widths (rock_24_20_4i_usa.bfn) at the ITEM_GET width 419. The line breaks were produced by
// native/tools/rando/gen_dungeon_item_msgs.py (gclib's BFN.word_wrap_string on the font from the
// player's disc); only wwrando's output text is kept here, no disc bytes.
const PcRandoDungeonItem kItems[PCR_DITEM_COUNT] = {
    {0x13, 3, PCR_DITEM_SMALL_KEY, PCR_DITEM_BASE_SMALL_KEY, "DRC", "DRC Small Key",
     PCR_MSG_INSTANT "You got a " PCR_MSG_COLOR_RED "Dragon Roost Cavern\nsmall key" PCR_MSG_COLOR_DEFAULT "!"},
    {0x14, 3, PCR_DITEM_BIG_KEY, PCR_DITEM_BASE_BIG_KEY, "DRC", "DRC Big Key",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Dragon Roost Cavern\nBig Key" PCR_MSG_COLOR_DEFAULT "!"},
    {0x1B, 3, PCR_DITEM_MAP, PCR_DITEM_BASE_MAP, "DRC", "DRC Dungeon Map",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Dragon Roost Cavern\nDungeon Map" PCR_MSG_COLOR_DEFAULT "!"},
    {0x1C, 3, PCR_DITEM_COMPASS, PCR_DITEM_BASE_COMPASS, "DRC", "DRC Compass",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Dragon Roost Cavern\nCompass" PCR_MSG_COLOR_DEFAULT "!"},
    {0x1D, 4, PCR_DITEM_SMALL_KEY, PCR_DITEM_BASE_SMALL_KEY, "FW", "FW Small Key",
     PCR_MSG_INSTANT "You got a " PCR_MSG_COLOR_RED "Forbidden Woods small\nkey" PCR_MSG_COLOR_DEFAULT "!"},
    {0x40, 4, PCR_DITEM_BIG_KEY, PCR_DITEM_BASE_BIG_KEY, "FW", "FW Big Key",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Forbidden Woods Big\nKey" PCR_MSG_COLOR_DEFAULT "!"},
    {0x41, 4, PCR_DITEM_MAP, PCR_DITEM_BASE_MAP, "FW", "FW Dungeon Map",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Forbidden Woods\nDungeon Map" PCR_MSG_COLOR_DEFAULT "!"},
    {0x5A, 4, PCR_DITEM_COMPASS, PCR_DITEM_BASE_COMPASS, "FW", "FW Compass",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Forbidden Woods\nCompass" PCR_MSG_COLOR_DEFAULT "!"},
    {0x5B, 5, PCR_DITEM_SMALL_KEY, PCR_DITEM_BASE_SMALL_KEY, "TotG", "TotG Small Key",
     PCR_MSG_INSTANT "You got a " PCR_MSG_COLOR_RED "Tower of the Gods small\nkey" PCR_MSG_COLOR_DEFAULT "!"},
    {0x5C, 5, PCR_DITEM_BIG_KEY, PCR_DITEM_BASE_BIG_KEY, "TotG", "TotG Big Key",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Tower of the Gods Big\nKey" PCR_MSG_COLOR_DEFAULT "!"},
    {0x5D, 5, PCR_DITEM_MAP, PCR_DITEM_BASE_MAP, "TotG", "TotG Dungeon Map",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Tower of the Gods\nDungeon Map" PCR_MSG_COLOR_DEFAULT "!"},
    {0x5E, 5, PCR_DITEM_COMPASS, PCR_DITEM_BASE_COMPASS, "TotG", "TotG Compass",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Tower of the Gods\nCompass" PCR_MSG_COLOR_DEFAULT "!"},
    {0x5F, 2, PCR_DITEM_MAP, PCR_DITEM_BASE_MAP, "FF", "FF Dungeon Map",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Forsaken Fortress\nDungeon Map" PCR_MSG_COLOR_DEFAULT "!"},
    {0x60, 2, PCR_DITEM_COMPASS, PCR_DITEM_BASE_COMPASS, "FF", "FF Compass",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Forsaken Fortress\nCompass" PCR_MSG_COLOR_DEFAULT "!"},
    {0x73, 6, PCR_DITEM_SMALL_KEY, PCR_DITEM_BASE_SMALL_KEY, "ET", "ET Small Key",
     PCR_MSG_INSTANT "You got an " PCR_MSG_COLOR_RED "Earth Temple small key" PCR_MSG_COLOR_DEFAULT "!"},
    {0x74, 6, PCR_DITEM_BIG_KEY, PCR_DITEM_BASE_BIG_KEY, "ET", "ET Big Key",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Earth Temple Big Key" PCR_MSG_COLOR_DEFAULT "!"},
    {0x75, 6, PCR_DITEM_MAP, PCR_DITEM_BASE_MAP, "ET", "ET Dungeon Map",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Earth Temple Dungeon\nMap" PCR_MSG_COLOR_DEFAULT "!"},
    {0x76, 6, PCR_DITEM_COMPASS, PCR_DITEM_BASE_COMPASS, "ET", "ET Compass",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Earth Temple Compass" PCR_MSG_COLOR_DEFAULT "!"},
    {0x77, 7, PCR_DITEM_SMALL_KEY, PCR_DITEM_BASE_SMALL_KEY, "WT", "WT Small Key",
     PCR_MSG_INSTANT "You got a " PCR_MSG_COLOR_RED "Wind Temple small key" PCR_MSG_COLOR_DEFAULT "!"},
    {0x81, 7, PCR_DITEM_BIG_KEY, PCR_DITEM_BASE_BIG_KEY, "WT", "WT Big Key",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Wind Temple Big Key" PCR_MSG_COLOR_DEFAULT "!"},
    {0x84, 7, PCR_DITEM_MAP, PCR_DITEM_BASE_MAP, "WT", "WT Dungeon Map",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Wind Temple Dungeon\nMap" PCR_MSG_COLOR_DEFAULT "!"},
    {0x85, 7, PCR_DITEM_COMPASS, PCR_DITEM_BASE_COMPASS, "WT", "WT Compass",
     PCR_MSG_INSTANT "You got the " PCR_MSG_COLOR_RED "Wind Temple Compass" PCR_MSG_COLOR_DEFAULT "!"},
};

} // namespace

extern "C" {

const PcRandoDungeonItem* pc_rando_dungeon_item(int i) {
    return i >= 0 && i < PCR_DITEM_COUNT ? &kItems[i] : nullptr;
}

const PcRandoDungeonItem* pc_rando_dungeon_item_find(uint8_t id) {
    for (const PcRandoDungeonItem& e : kItems) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

// custom_funcs.asm generic_on_dungeon_bit: compares the byte at 0x803C53A4 (dSv_info_c::mDan.mStageNo)
// with the dungeon's stage; equal uses the loaded stage info 0x803C5380 (mMemory), else
// 0x803C4F88 + stage * 0x24 (dSv_save_c::mSave[stage]).
int pc_rando_dungeon_bit_target(int cur_stage, int item_stage) {
    return cur_stage == item_stage ? PCR_DITEM_TARGET_CURRENT : PCR_DITEM_TARGET_SAVED;
}

// custom_funcs.asm generic_small_key_item_get_func: in the right dungeon, the STAG's "is dungeon" bit
// (byte 9 bit 0, dStage_stagInfo_ChkKeyDisp) picks item_func_small_key; without it (a boss room, where
// the meter's key counter that turns mItemKeyNumCount into keys is off) the current memory's key count
// is raised directly. In another stage, the dungeon's saved key count is raised.
int pc_rando_dungeon_key_target(int cur_stage, int item_stage, int key_disp) {
    if (cur_stage != item_stage) {
        return PCR_DITEM_TARGET_SAVED;
    }
    return key_disp ? PCR_DITEM_KEY_ITEM_FUNC : PCR_DITEM_TARGET_CURRENT;
}

// tweaks.py: msg = bmg.add_new_message(101 + item_id); text_box_type = ITEM_GET;
// initial_draw_type = 2 (slow); display_item_id = item_id; string = the wrapped description.
int pc_rando_dungeon_items_add_messages(void) {
    int added = 0;
    for (const PcRandoDungeonItem& e : kItems) {
        PcRandoMsgEntry entry;
        pc_rando_msg_entry_defaults(&entry);
        entry.text_box_type = 9; // TextBoxType.ITEM_GET
        entry.initial_draw_type = 2;
        entry.display_item_id = e.id;
        if (pc_rando_msg_add((uint16_t)(101 + e.id), &entry, e.text) == PCR_MSG_OK) {
            added++;
        }
    }
    return added;
}

} // extern "C"
