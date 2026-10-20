// COS_SMOKE=rando COS_RANDO_SMOKE=items (docs/RANDOMIZER_PLAN.md step R0.8): item dispatch and the
// item/shop table snapshots in a booted game, e.g.
//   run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=items
// Once the play scene has run 10 frames:
//   1. copies item_resource[], field_item_res[], item_info[] and the shop data as they are at boot;
//   2. registers overrides for id 0x13 (vanilla: item_func_noentry / getcheck -1) and dummy edits; while
//      inactive execItemGet/checkItemGet stay vanilla and the tables are untouched;
//   3. activates: the edits are in the tables, execItemGet/checkItemGet reach the overrides, a
//      non-overridden id still gets the vanilla answer, an edit registered while active applies at once;
//   4. deactivates: the tables are byte-identical to boot and the overrides are no longer reached
//      (under COS_RANDO_FORCE=1 the switch stays on, so dropping the edits restores the tables);
//   5. the game then runs 30 more frames.
// Exit 0 when all hold, else 1.
#include "pc_rando_smokes.h"

#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "features/rando/pc_rando_items.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_shop.h"

#include <cstddef>
#include <cstring>

namespace pc {

namespace {

constexpr const char* kName = "items";
constexpr u8 kId = 0x13; // item_func_noentry, item_getcheck_func_noentry
unsigned int sEndFrame = 0;
int sItemCalls = 0;

void overrideItem() {
    sItemCalls++;
}
int overrideCheck() {
    return 1;
}

dItem_data_item_resource sBootRes[0x100];
dItem_data_field_item_res sBootField[0x100];
dItem_data_item_info sBootInfo[0x100];
ShopItems_c__ItemData sBootShopMap;
__shop_items_set_data sBootSetMap;

void saveBoot() {
    memcpy(sBootRes, dItem_data::item_resource, sizeof sBootRes);
    memcpy(sBootField, dItem_data::field_item_res, sizeof sBootField);
    memcpy(sBootInfo, dItem_data::item_info, sizeof sBootInfo);
    sBootShopMap = shopItemData_map;
    sBootSetMap = shopItems_setData_map;
}

bool tablesAtBoot() {
    return memcmp(sBootRes, dItem_data::item_resource, sizeof sBootRes) == 0 &&
           memcmp(sBootField, dItem_data::field_item_res, sizeof sBootField) == 0 &&
           memcmp(sBootInfo, dItem_data::item_info, sizeof sBootInfo) == 0 &&
           memcmp(&sBootShopMap, &shopItemData_map, sizeof sBootShopMap) == 0 &&
           memcmp(&sBootSetMap, &shopItems_setData_map, sizeof sBootSetMap) == 0;
}

} // namespace

void randoSmokeItems(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    const int forced = pc_rando_forced();
    randoSmokeCheck(kName, forced ? "forced boot: tables snapshotted" : "plain boot: tables not snapshotted",
                    pc_rando_tables_applied() == forced);
    // Drop the seed-independent edits later steps register at boot (R2.11's field items, R3.4's dungeon
    // items), so this smoke sees only its own; under COS_RANDO_FORCE that also puts the snapshot (the boot
    // bytes) back, so the tables hold their boot bytes either way.
    pc_rando_table_edits_clear();
    saveBoot();

    pc_rando_item_override(kId, overrideItem);
    pc_rando_item_getcheck_override(kId, overrideCheck);
    const u8 info = 0x77;
    const s16 bmd = 0x1234;
    const u32 shopItem = dItemNo_TRIFORCE1_e;
    const u32 msg = 0xBEEF;
    bool reg = pc_rando_table_edit(PCR_TBL_ITEM_INFO, 0x01, offsetof(dItem_data_item_info, mShadowSize), &info, 1) &&
               pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 0x01, offsetof(dItem_data_item_resource, mBmdIdx), &bmd,
                                   sizeof bmd) &&
               pc_rando_table_edit(PCR_TBL_SHOP_ITEM_DATA, PCR_SHOP_ITEM_MAP, offsetof(ShopItems_c__ItemData, mItemNo),
                                   &shopItem, sizeof shopItem);
    randoSmokeCheck(kName, "edits registered", reg && pc_rando_table_edit_count() == 3);
    randoSmokeCheck(kName, "bad edits refused",
                    !pc_rando_table_edit(PCR_TBL_COUNT, 0, 0, &info, 1) &&
                        !pc_rando_table_edit(PCR_TBL_ITEM_INFO, 0x100, 0, &info, 1) &&
                        !pc_rando_table_edit(PCR_TBL_ITEM_INFO, 0, 3, &bmd, 2));

    if (!forced) {
        sItemCalls = 0;
        execItemGet(kId);
        randoSmokeCheck(kName, "inactive: execItemGet stays vanilla", sItemCalls == 0);
        randoSmokeCheck(kName, "inactive: checkItemGet stays vanilla", checkItemGet(kId, 7) == 7);
        randoSmokeCheck(kName, "inactive: tables untouched", tablesAtBoot());
    }

    pc_rando_set_active(nullptr, nullptr);
    randoSmokeCheck(kName, "active: tables snapshotted", pc_rando_tables_applied() == 1);
    randoSmokeCheck(kName, "active: item_info edit applied", dItem_data::item_info[0x01].mShadowSize == info &&
                                                                 dItem_data::item_info[0x02].mShadowSize ==
                                                                     sBootInfo[0x02].mShadowSize);
    randoSmokeCheck(kName, "active: item_resource edit applied", dItem_data::item_resource[0x01].mBmdIdx == bmd &&
                                                                     dItem_data::item_resource[0x01].mArcname ==
                                                                         sBootRes[0x01].mArcname);
    randoSmokeCheck(kName, "active: shop edit applied", shopItemData_map.mItemNo == shopItem);
    sItemCalls = 0;
    execItemGet(kId);
    randoSmokeCheck(kName, "active: execItemGet dispatches to the override", sItemCalls == 1);
    randoSmokeCheck(kName, "active: checkItemGet dispatches to the override", checkItemGet(kId, 7) == 1);
    randoSmokeCheck(kName, "active: no override gives the vanilla check", checkItemGet(0x14, 5) == 5);
    reg = pc_rando_table_edit(PCR_TBL_SHOP_SET_DATA, PCR_SHOP_SET_MAP, offsetof(__shop_items_set_data, mShowMsgNo),
                              &msg, sizeof msg);
    randoSmokeCheck(kName, "active: a late edit applies at once", reg && shopItems_setData_map.mShowMsgNo == msg);

    pc_rando_deactivate();
    if (!forced) {
        randoSmokeCheck(kName, "deactivate: tables byte-identical to boot", tablesAtBoot());
        randoSmokeCheck(kName, "deactivate: snapshot released", pc_rando_tables_applied() == 0);
        sItemCalls = 0;
        execItemGet(kId);
        randoSmokeCheck(kName, "deactivate: execItemGet vanilla again", sItemCalls == 0 && checkItemGet(kId, 7) == 7);
    } else {
        randoSmokeCheck(kName, "forced deactivate: edits kept", shopItemData_map.mItemNo == shopItem &&
                                                                   pc_rando_tables_applied() == 1);
    }
    pc_rando_table_edits_clear();
    randoSmokeCheck(kName, "edits cleared: tables byte-identical to boot", tablesAtBoot());
    pc_rando_item_override(kId, nullptr);
    pc_rando_item_getcheck_override(kId, nullptr);
    randoSmokeCheck(kName, "overrides removed", pc_rando_item_override_get(kId) == nullptr &&
                                                    pc_rando_item_getcheck_override_get(kId) == nullptr);
    sEndFrame = frames + 30;
}

} // namespace pc
