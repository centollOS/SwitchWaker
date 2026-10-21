// The game side of the randomizer's Beedle shops (docs/RANDOMIZER_PLAN.md step R2.9): the 4 shop
// locations (Great Sea shop ship 20 Rupee slot, Rock Spire special shop ship 500/950/900 Rupee slots)
// and the shop item data tweaks.
//   - At each activation (a pc_rando listener) the placed items are written into the mItemNo of the
//     4 shopItemData_* entries of d_shop.cpp as R0.8 table edits (wwrando writes the item byte at
//     main.dol 0x80375E1F / 0x80375ECF / 0x80375EDF / 0x80375EEF), together with
//     tweaks.py remove_shop_item_forced_uniqueness_bit (mBuyConditions bit 0x02 cleared for shop item
//     indexes 0, 0xB, 0xC, 0xD). The edits are re-registered at every activation, so a new seed (or
//     a smoke that cleared the registry) gets the right bytes; deactivation restores the boot tables.
//   - The Bait Bag slot's sold-out state lives in event bit 0x6902 (flexible_item_locations.asm
//     set/check_shop_item_in_bait_bag_slot_sold_out), which daNpc_Bs1_c's R2.9 hunks set and test.
//   - The other hunks (slot item ids in daNpc_Bs1_c::next_msgStatus and createShopList, the shop item
//     Y offset in daShopItem_c::set_mtx) are inline in game/ and call pc_rando_loc_item.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "pc/pc_rando_locs.h"
#include "features/rando/pc_rando_items.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_shop.h"

#include <cstddef>
#include <cstdio>
#include <unistd.h>

namespace {

constexpr uint16_t kBaitSlotSoldOutBit = 0x6902; // D17: reserved for wwrando, unused by vanilla

struct ShopSlot {
    int loc;          // PCR_LOC_*
    int index;        // PCR_SHOP_ITEM_* (R0.8's PCR_TBL_SHOP_ITEM_DATA)
    uint8_t vanilla;  // the item on the disc
};

constexpr ShopSlot kSlots[PCR_SHOP_SLOT_COUNT] = {
    {PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM, PCR_SHOP_ITEM_FEEDBAG, dItemNo_BAIT_BAG_e},
    {PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM, PCR_SHOP_ITEM_EMPTYBOTTLE,
     dItemNo_EMPTY_BOTTLE_e},
    {PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM, PCR_SHOP_ITEM_KAKERA_HEART,
     dItemNo_HEART_PIECE_e},
    {PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM, PCR_SHOP_ITEM_MAP,
     dItemNo_COLLECT_MAP_30_e},
};

// The boot mBuyConditions of the 4 entries (d_shop.cpp), less Shop_BuyCond_MUST_NOT_OWN_ITEM_e.
constexpr uint8_t kBuyConditions[PCR_SHOP_SLOT_COUNT] = {
    (Shop_BuyCond_RUPEES_e | Shop_BuyCond_MUST_NOT_OWN_ITEM_e) & ~Shop_BuyCond_MUST_NOT_OWN_ITEM_e, // Feedbag
    Shop_BuyCond_RUPEES_e & ~Shop_BuyCond_MUST_NOT_OWN_ITEM_e,                                       // emptybottle
    (Shop_BuyCond_RUPEES_e | Shop_BuyCond_MUST_NOT_OWN_ITEM_e) & ~Shop_BuyCond_MUST_NOT_OWN_ITEM_e, // kakera_heart
    (Shop_BuyCond_RUPEES_e | Shop_BuyCond_MUST_NOT_OWN_ITEM_e) & ~Shop_BuyCond_MUST_NOT_OWN_ITEM_e, // map
};

int sEditsFailed = 0;

void registerEdits() {
    int failed = 0;
    for (int i = 0; i < PCR_SHOP_SLOT_COUNT; i++) {
        const u32 item = pc_rando_loc_item(kSlots[i].loc, kSlots[i].vanilla);
        failed += !pc_rando_table_edit(PCR_TBL_SHOP_ITEM_DATA, kSlots[i].index,
                                       offsetof(ShopItems_c__ItemData, mItemNo), &item, sizeof item);
        const u8 cond = kBuyConditions[i];
        failed += !pc_rando_table_edit(PCR_TBL_SHOP_ITEM_DATA, kSlots[i].index,
                                       offsetof(ShopItems_c__ItemData, mBuyConditions), &cond, sizeof cond);
    }
    sEditsFailed = failed;
    if (failed != 0) {
        char buf[96];
        const int n = snprintf(buf, sizeof buf, "[cos] rando shop: %d table edit(s) refused\n", failed);
        (void)!write(STDERR_FILENO, buf, (size_t)n);
    }
}

void onRando(int active) {
    if (active) {
        registerEdits();
    }
}

struct RegisterShop {
    RegisterShop() {
        if (!pc_rando_add_listener(onRando)) {
            static const char kMsg[] = "[cos] rando shop: listener list full, shop slots stay vanilla\n";
            (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
        }
    }
} sRegisterShop;

} // namespace

extern "C" void pc_rando_shop_bait_slot_set_sold_out(void) {
    dComIfGs_onEventBit(kBaitSlotSoldOutBit);
}

extern "C" int pc_rando_shop_bait_slot_sold_out(void) {
    return dComIfGs_isEventBit(kBaitSlotSoldOutBit) ? 1 : 0;
}

extern "C" int pc_rando_shop_slot_loc(int slot) {
    return slot >= 0 && slot < PCR_SHOP_SLOT_COUNT ? kSlots[slot].loc : -1;
}

extern "C" uint8_t pc_rando_shop_slot_vanilla(int slot) {
    return slot >= 0 && slot < PCR_SHOP_SLOT_COUNT ? kSlots[slot].vanilla : 0xFF;
}

extern "C" int pc_rando_shop_register_edits(void) {
    registerEdits();
    return sEditsFailed == 0 ? 1 : 0;
}
