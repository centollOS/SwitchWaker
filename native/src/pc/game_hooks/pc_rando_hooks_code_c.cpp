// The randomizer's code locations C, game side (docs/RANDOMIZER_PLAN.md step R2.8): custom_createItem
// and withered_tree_item_try_give_momentum (wwrando custom_funcs.asm, flexible_item_locations.asm
// d_a_obj_ftree). Declared in native/include/pc/pc_rando_hooks.h; called only under pc_rando() from
// the d_a_obj_ftree.cpp hunks listed in docs/RANDO_HOOKS.md, section R2.8. The item lookups of the
// other sites are pure (features/rando/pc_rando_code_c.cpp).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "f_op/f_op_actor_mng.h"

uint32_t pc_rando_custom_create_item(void* pos, int item_no, int room_no, void* angle, void* scale,
                                     int item_bit_no) {
    // custom_createItem: item 0xFF creates nothing and returns -1 (createItem would too).
    if (item_no == 0xFF) {
        return (uint32_t)fpcM_ERROR_PROCESS_ID_e;
    }
    // createItem(pos, item, itemBitNo, roomNo, type 3 "don't fade out", angle, action 5 "ding", scale)
    return (uint32_t)fopAcM_createItem((cXyz*)pos, item_no, item_bit_no, room_no, 3, (csXyz*)angle, 5,
                                       (cXyz*)scale);
}

void pc_rando_withered_tree_item_momentum(void* item, uint8_t* done) {
    if (item == nullptr || done == nullptr || *done != 0) {
        return;
    }
    // withered_tree_item_speeds: the speeds the vanilla fastCreateItem call passed.
    fopAc_ac_c* a = (fopAc_ac_c*)item;
    a->speedF = 1.75f;   // +0x254
    a->speed.y = 30.0f;  // +0x224
    a->gravity = -2.1f;  // +0x258
    a->actor_status |= 0x40; // +0x1C4: keep moving while an event runs
    *done = 1;
}
