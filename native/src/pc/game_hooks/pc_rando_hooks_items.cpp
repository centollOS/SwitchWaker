// The game side of the randomizer's item dispatch and table snapshots (docs/RANDOMIZER_PLAN.md step
// R0.8; the registries are features/rando/pc_rando_items.{h,cpp}, which stay free of game code for the
// unit tests):
//   - pc_rando_exec_item_get / pc_rando_check_item_get, which execItemGet / checkItemGet
//     (game/src/d/d_item.cpp) call under pc_rando(): a registered override, else the vanilla table;
//   - the binding of the real item and shop tables, at static-init time, so the first activation
//     (pc_rando.h listeners) can snapshot them before any edit.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "d/d_item_data.h"
#include "d/d_shop.h"

// d_item.cpp's dispatch tables (no header declares them; global names are not type-mangled).
extern void (*item_func_ptr[0x100])();
extern int (*item_getcheck_func_ptr[0x100])();

extern "C" void pc_rando_exec_item_get(u8 itemNo) {
    PcRandoItemFunc fn = pc_rando_item_override_get(itemNo);
    if (fn != nullptr) {
        fn();
        return;
    }
    item_func_ptr[itemNo]();
}

extern "C" BOOL pc_rando_check_item_get(u8 itemNo, BOOL defaultVal) {
    PcRandoItemCheckFunc fn = pc_rando_item_getcheck_override_get(itemNo);
    int result = fn != nullptr ? fn() : item_getcheck_func_ptr[itemNo]();
    if (result == -1) {
        return defaultVal;
    }
    return result;
}

namespace {

void* sItemResource[0x100];
void* sFieldItemRes[0x100];
void* sItemInfo[0x100];

void* const sShopItem[PCR_SHOP_ITEM_COUNT] = {
    &shopItemData_Feedbag,      &shopItemData_FoodAll,      &shopItemData_FoodHyoi,
    &shopItemData_arrow10,      &shopItemData_arrow30,      &shopItemData_red_bottle,
    &shopItemData_blue_bottle,  &shopItemData_green_bottle, &shopItemData_bomb10,
    &shopItemData_bomb20,       &shopItemData_bomb30,       &shopItemData_emptybottle,
    &shopItemData_kakera_heart, &shopItemData_map,          &shopItemData_RotenItem0,
    &shopItemData_RotenItem1,   &shopItemData_RotenItem2,   &shopItemData_RotenItem3,
    &shopItemData_RotenItem4,   &shopItemData_RotenItem5,   &shopItemData_RotenItem6,
    &shopItemData_RotenItem7,   &shopItemData_RotenItem8,   &shopItemData_RotenItem9,
    &shopItemData_RotenItem10,  &shopItemData_RotenItem11,
};

void* const sShopSet[PCR_SHOP_SET_COUNT] = {
    &shopItems_setData_Bomb10_exp,    &shopItems_setData_Bomb20_exp,     &shopItems_setData_Bomb30_exp,
    &shopItems_setData_Bomb10,        &shopItems_setData_Bomb20,         &shopItems_setData_Bomb30,
    &shopItems_setData_Feedbag,       &shopItems_setData_FoodAll,        &shopItems_setData_FoodHyoi,
    &shopItems_setData_Bomb30Bs,      &shopItems_setData_arrow10,        &shopItems_setData_arrow30,
    &shopItems_setData_red_bottleBs,  &shopItems_setData_Bomb30Bs2,      &shopItems_setData_arrow30Bs2,
    &shopItems_setData_red_bottleBs2, &shopItems_setData_emptybottle,    &shopItems_setData_kakera_heart,
    &shopItems_setData_map,           &shopItems_setData_red_bottleDs,   &shopItems_setData_green_bottleDs,
    &shopItems_setData_blue_bottleDs, &shopItems_setData_RotenItem0,     &shopItems_setData_RotenItem1,
    &shopItems_setData_RotenItem2,    &shopItems_setData_RotenItem3,     &shopItems_setData_RotenItem4,
    &shopItems_setData_RotenItem5,    &shopItems_setData_RotenItem6,     &shopItems_setData_RotenItem7,
    &shopItems_setData_RotenItem8,    &shopItems_setData_RotenItem9,     &shopItems_setData_RotenItem10,
    &shopItems_setData_RotenItem11,
};

// Binds the tables before main: the data are constant-initialized, so they hold their boot bytes
// here, and the first activation always finds them bound.
struct BindTables {
    BindTables() {
        for (int i = 0; i < 0x100; i++) {
            sItemResource[i] = &dItem_data::item_resource[i];
            sFieldItemRes[i] = &dItem_data::field_item_res[i];
            sItemInfo[i] = &dItem_data::item_info[i];
        }
        PcRandoTableDesc t[PCR_TBL_COUNT] = {};
        t[PCR_TBL_ITEM_RESOURCE] = {sItemResource, 0x100, sizeof(dItem_data_item_resource)};
        t[PCR_TBL_FIELD_ITEM_RES] = {sFieldItemRes, 0x100, sizeof(dItem_data_field_item_res)};
        t[PCR_TBL_ITEM_INFO] = {sItemInfo, 0x100, sizeof(dItem_data_item_info)};
        t[PCR_TBL_SHOP_ITEM_DATA] = {sShopItem, PCR_SHOP_ITEM_COUNT, sizeof(ShopItems_c__ItemData)};
        t[PCR_TBL_SHOP_SET_DATA] = {sShopSet, PCR_SHOP_SET_COUNT, sizeof(__shop_items_set_data)};
        pc_rando_items_bind_tables(t, PCR_TBL_COUNT);
        pc_rando_dungeon_items_register(); // R3.4: links and registers the dungeon item IDs
        pc_rando_posttext_register(); // R3.21: links and registers the post-randomization texts
    }
} sBindTables;

} // namespace
