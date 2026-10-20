// cos_rando_test_R0_8 (docs/RANDOMIZER_PLAN.md step R0.8): the item-override and table-edit registries
// of features/rando/pc_rando_items.h without a game, on fake tables:
//   - overrides: register, read back, count, remove;
//   - edits: validation, replacement of the same key, snapshot at activation, edits applied, a late
//     edit applied at once, restore at deactivation (byte-identical), clear while active;
//   - re-activation while applied starts again from the boot bytes.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_items.h"

#include <cstddef>
#include <cstring>

namespace {

struct Res {
    const char* arc;
    short bmd;
    unsigned char frm;
};
struct Shop {
    unsigned int itemNo;
    int mustNot;
};

Res sRes[4] = {{"a", 1, 2}, {"b", 3, 4}, {"c", 5, 6}, {"d", 7, 8}};
Shop sShop0 = {0x10, -1};
Shop sShop1 = {0x11, 0x11};
Res sBootRes[4];
Shop sBoot0, sBoot1;

void* sResPtrs[4] = {&sRes[0], &sRes[1], &sRes[2], &sRes[3]};
void* const sShopPtrs[2] = {&sShop0, &sShop1};

int sCalls = 0;
void itemFn() {
    sCalls++;
}
int checkFn() {
    return 9;
}

bool atBoot() {
    return memcmp(sRes, sBootRes, sizeof sRes) == 0 && memcmp(&sShop0, &sBoot0, sizeof sShop0) == 0 &&
           memcmp(&sShop1, &sBoot1, sizeof sShop1) == 0;
}

} // namespace

int main() {
    memcpy(sBootRes, sRes, sizeof sRes);
    sBoot0 = sShop0;
    sBoot1 = sShop1;

    // Overrides.
    RT_CHECK_EQ(pc_rando_item_override_count(), 0);
    pc_rando_item_override(0xA3, itemFn);
    pc_rando_item_getcheck_override(0xA3, checkFn);
    RT_CHECK(pc_rando_item_override_get(0xA3) == itemFn);
    RT_CHECK(pc_rando_item_getcheck_override_get(0xA3) == checkFn);
    RT_CHECK(pc_rando_item_override_get(0xA4) == nullptr);
    RT_CHECK_EQ(pc_rando_item_override_count(), 1);
    pc_rando_item_override_get(0xA3)();
    RT_CHECK_EQ(sCalls, 1);
    pc_rando_item_override(0xA3, nullptr);
    RT_CHECK_EQ(pc_rando_item_override_count(), 0);

    // Binding.
    PcRandoTableDesc t[PCR_TBL_COUNT] = {};
    t[PCR_TBL_ITEM_RESOURCE] = {sResPtrs, 4, sizeof(Res)};
    t[PCR_TBL_SHOP_ITEM_DATA] = {sShopPtrs, 2, sizeof(Shop)};
    RT_CHECK_EQ(pc_rando_items_bind_tables(t, PCR_TBL_COUNT), 1);
    RT_CHECK_EQ(pc_rando_tables_applied(), 0);

    // Validation.
    const short bmd = 0x55;
    const unsigned int item = 0x99;
    RT_CHECK(!pc_rando_table_edit(-1, 0, 0, &bmd, 2));
    RT_CHECK(!pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 4, 0, &bmd, 2));                 // index
    RT_CHECK(!pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 0, sizeof(Res) - 1, &bmd, 2));    // range
    RT_CHECK(!pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 0, 0, &bmd, 0));                 // empty
    RT_CHECK(!pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 0, 0, &bmd, PC_RANDO_TABLE_EDIT_MAX + 1));
    RT_CHECK_EQ(pc_rando_table_edit_count(), 0);

    // Edits wait for activation; the same key replaces.
    const short bmdOld = 0x44;
    RT_CHECK(pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 2, offsetof(Res, bmd), &bmdOld, sizeof bmdOld));
    RT_CHECK(pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 2, offsetof(Res, bmd), &bmd, sizeof bmd));
    RT_CHECK(pc_rando_table_edit(PCR_TBL_SHOP_ITEM_DATA, 1, offsetof(Shop, itemNo), &item, sizeof item));
    RT_CHECK_EQ(pc_rando_table_edit_count(), 2);
    RT_CHECK(atBoot());

    // Activation: snapshot, then edits.
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(pc_rando_tables_applied(), 1);
    RT_CHECK_EQ(sRes[2].bmd, bmd);
    RT_CHECK_EQ(sRes[1].bmd, sBootRes[1].bmd);
    RT_CHECK_EQ(sShop1.itemNo, item);
    RT_CHECK_EQ(sShop0.itemNo, sBoot0.itemNo);

    // A late edit applies at once.
    const unsigned char frm = 0xEE;
    RT_CHECK(pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, 0, offsetof(Res, frm), &frm, 1));
    RT_CHECK_EQ(sRes[0].frm, frm);

    // Re-activation starts from boot again (same result, edits not stacked on edited bytes).
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK_EQ(sRes[2].bmd, bmd);
    RT_CHECK_EQ(sRes[0].frm, frm);

    // Deactivation restores the boot bytes.
    pc_rando_deactivate();
    RT_CHECK_EQ(pc_rando_tables_applied(), 0);
    RT_CHECK(atBoot());

    // Clearing the edits while active restores too.
    pc_rando_set_active(nullptr, nullptr);
    RT_CHECK(!atBoot());
    pc_rando_table_edits_clear();
    RT_CHECK_EQ(pc_rando_table_edit_count(), 0);
    RT_CHECK(atBoot());
    pc_rando_deactivate();
    RT_CHECK(atBoot());

    return rt::finish();
}
