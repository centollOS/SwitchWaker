// pc_rando_code_c.cpp - code locations C (docs/RANDOMIZER_PLAN.md, step R2.8): the site table and
// pc_rando_code_c_item. See pc_rando_code_c.h. Pure (cos_rando_core).
#include "pc_rando_code_c.h"

#include "pc/pc_rando_locs.h"

namespace pcr {
namespace {

#define AUC "rels/d_a_auction.rel"
#define KG1 "rels/d_a_npc_kg1.rel"
#define KG2 "rels/d_a_npc_kg2.rel"
#define BMC "rels/d_a_npc_bmcon1.rel"
#define ROT "rels/d_a_npc_roten.rel"
#define TPO "rels/d_a_obj_toripost.rel"
#define FTR "rels/d_a_obj_ftree.rel"
#define RSH "rels/d_a_npc_rsh1.rel"

// The vanilla values are the decompiled tables' (game/src/d/actor/...), which are the disc's bytes.
const CodeCSite kSites[] = {
    // d_a_auction.cpp l_item_dat[i].mItemID (8-byte rows; read at fopAcM_create(ShopItem) and
    // createItemForPresentDemo)
    {PC_RANDO_CODE_C_AUCTION, 0, PCR_LOC_WINDFALL_ISLAND_40_RUPEE_AUCTION, 0x1F, AUC, 0x4489},
    {PC_RANDO_CODE_C_AUCTION, 1, PCR_LOC_WINDFALL_ISLAND_5_RUPEE_AUCTION, 0xE4, AUC, 0x4491},
    {PC_RANDO_CODE_C_AUCTION, 2, PCR_LOC_WINDFALL_ISLAND_60_RUPEE_AUCTION, 0xED, AUC, 0x4499},
    {PC_RANDO_CODE_C_AUCTION, 3, PCR_LOC_WINDFALL_ISLAND_80_RUPEE_AUCTION, 0x07, AUC, 0x44A1},
    // d_a_npc_kg1.cpp wait_action items[] (index = event register 0xFE07 - 1; items[2], 0x05, is not a
    // location)
    {PC_RANDO_CODE_C_BATTLESQUID, 0, PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_FIRST_PRIZE, 0x07, KG1, 0x2C28},
    {PC_RANDO_CODE_C_BATTLESQUID, 1, PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_SECOND_PRIZE, 0xCC, KG1, 0x2C29},
    // wait_action rec_items[] (rec_items[1], the orange rupee, is not a location)
    {PC_RANDO_CODE_C_BATTLESQUID_RECORD, 0, PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_UNDER_20_SHOTS_PRIZE, 0xF1, KG1,
     0x2C2C},
    // d_a_npc_kg2.cpp evn_createItem_init: index = event register 0xB703 (0 and 1 share one li)
    {PC_RANDO_CODE_C_BARREL_SHOOTING, 0, PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_FIRST_PRIZE, 0x07, KG2, 0x20FF},
    {PC_RANDO_CODE_C_BARREL_SHOOTING, 1, PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_FIRST_PRIZE, 0x07, KG2, 0x20FF},
    {PC_RANDO_CODE_C_BARREL_SHOOTING, 2, PCR_LOC_SPECTACLE_ISLAND_BARREL_SHOOTING_SECOND_PRIZE, 0xD7, KG2, 0x2107},
    // d_a_npc_bmcon1.cpp eventMesSet (the heart piece; the purple rupee is not a location)
    {PC_RANDO_CODE_C_BIRD_MAN, 0, PCR_LOC_FLIGHT_CONTROL_PLATFORM_BIRD_MAN_CONTEST_FIRST_PRIZE, 0x07, BMC, 0x2B7B},
    // d_a_npc_roten.cpp l_get_item_no[] (index = the event's ItemNo integer)
    {PC_RANDO_CODE_C_GORON_TRADE, 0, PCR_LOC_THE_GREAT_SEA_GORON_TRADING_REWARD, 0x07, ROT, 0x533B},
    // d_a_obj_toripost.cpp m_letter[i].mItemNo (12-byte rows; read in cutPresentProc)
    {PC_RANDO_CODE_C_MAILBOX, 0, PCR_LOC_MAILBOX_LETTER_FROM_BAITOS_MOTHER, 0x07, TPO, 0x2564},
    {PC_RANDO_CODE_C_MAILBOX, 1, PCR_LOC_MAILBOX_LETTER_FROM_KOMALIS_FATHER, 0x07, TPO, 0x2570},
    {PC_RANDO_CODE_C_MAILBOX, 2, PCR_LOC_MAILBOX_LETTER_ADVERTISING_BOMBS_IN_BEEDLES_SHOP, 0xC3, TPO, 0x257C},
    {PC_RANDO_CODE_C_MAILBOX, 3, PCR_LOC_MAILBOX_LETTER_FROM_ORCA, 0x04, TPO, 0x2588},
    {PC_RANDO_CODE_C_MAILBOX, 4, PCR_LOC_MAILBOX_LETTER_FROM_GRANDMA, 0x04, TPO, 0x2594},
    {PC_RANDO_CODE_C_MAILBOX, 5, PCR_LOC_MAILBOX_LETTER_ADVERTISING_ROCK_SPIRE_SHOP_SHIP, 0x01, TPO, 0x25A0},
    {PC_RANDO_CODE_C_MAILBOX, 6, PCR_LOC_MAILBOX_LETTER_FROM_TINGLE, 0xCB, TPO, 0x25AC},
    {PC_RANDO_CODE_C_MAILBOX, 7, PCR_LOC_MAILBOX_LETTER_FROM_ARYLL, 0x04, TPO, 0x25B8},
    {PC_RANDO_CODE_C_MAILBOX, 8, PCR_LOC_MAILBOX_LETTER_FROM_BAITOS_MOTHER, 0x07, TPO, 0x25C4},
    {PC_RANDO_CODE_C_MAILBOX, 9, PCR_LOC_MAILBOX_BEEDLES_SILVER_MEMBERSHIP_REWARD, 0x9D, TPO, 0x25D0},
    {PC_RANDO_CODE_C_MAILBOX, 10, PCR_LOC_MAILBOX_LETTER_FROM_HOSKITS_GIRLFRIEND, 0x3F, TPO, 0x25DC},
    {PC_RANDO_CODE_C_MAILBOX, 11, PCR_LOC_MAILBOX_LETTER_FROM_BAITO, 0x04, TPO, 0x25E8},
    {PC_RANDO_CODE_C_MAILBOX, 12, PCR_LOC_MAILBOX_BEEDLES_GOLD_MEMBERSHIP_REWARD, 0x9E, TPO, 0x25F4},
    // d_a_obj_ftree.cpp: index 0 launch_heart_part, 1 place_heart_part
    {PC_RANDO_CODE_C_WITHERED_TREE, 0, PCR_LOC_THE_GREAT_SEA_WITHERED_TREES, 0x07, FTR, 0x0237},
    {PC_RANDO_CODE_C_WITHERED_TREE, 1, PCR_LOC_THE_GREAT_SEA_WITHERED_TREES, 0x07, FTR, 0x03F3},
    // d_a_npc_rsh1.cpp: index 0 talk01 (mItemNo), 1 getdemo_action (the compare), 2 the slot id
    // zunari_give_item_and_set_magic_armor_event_bit compares the given item with (event bit 0x6940)
    {PC_RANDO_CODE_C_ZUNARI, 0, PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP, 0x2A, RSH,
     0x3117},
    {PC_RANDO_CODE_C_ZUNARI, 1, PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP, 0x2A, RSH,
     0x338F},
    {PC_RANDO_CODE_C_ZUNARI, 2, PCR_LOC_WINDFALL_ISLAND_ZUNARI_STOCK_EXOTIC_FLOWER_IN_ZUNARIS_SHOP, 0x2A,
     "zunari_magic_armor_slot_item_id", 0},
    // d_a_npc_ds1.cpp getdemo_action, message 7627 (a potion made for the first time): index = the
    // potion's item id
    {PC_RANDO_CODE_C_DOC_BANDAM, 0x52, PCR_LOC_WINDFALL_ISLAND_CHU_JELLY_JUICE_SHOP_GIVE_15_GREEN_CHU_JELLY, 0x52,
     "doc_bandam_green_potion_slot_item_id", 0},
    {PC_RANDO_CODE_C_DOC_BANDAM, 0x53, PCR_LOC_WINDFALL_ISLAND_CHU_JELLY_JUICE_SHOP_GIVE_15_BLUE_CHU_JELLY, 0x53,
     "doc_bandam_blue_potion_slot_item_id", 0},
};

const char* const kSiteNames[PC_RANDO_CODE_C_SITE_COUNT] = {
    "auction", "battlesquid", "battlesquid-record", "barrel-shooting", "bird-man",
    "goron-trade", "mailbox", "withered-tree", "zunari", "doc-bandam",
};

} // namespace

Span<CodeCSite> code_c_sites() {
    return {kSites, sizeof kSites / sizeof kSites[0]};
}

const CodeCSite* code_c_find(int site, int index) {
    for (const CodeCSite& s : kSites) {
        if (s.site == site && s.index == index) {
            return &s;
        }
    }
    return nullptr;
}

const char* code_c_site_name(int site) {
    return site >= 0 && site < PC_RANDO_CODE_C_SITE_COUNT ? kSiteNames[site] : "?";
}

} // namespace pcr

extern "C" uint8_t pc_rando_code_c_item(int site, int index, uint8_t vanilla) {
    if (!pc_rando()) {
        return vanilla;
    }
    const pcr::CodeCSite* s = pcr::code_c_find(site, index);
    return s != nullptr ? pc_rando_loc_item(s->loc, vanilla) : vanilla;
}
