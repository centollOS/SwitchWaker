// The game side of the randomizer's Tingle Statues (docs/RANDOMIZER_PLAN.md step R3.6; tables and
// texts in features/rando/pc_rando_tingle.{h,cpp}, which stay free of game code for the unit tests):
//   - the five statue item functions (custom_funcs.asm *_tingle_statue_item_get_func), registered as
//     item-get overrides of 0xA3-0xA7 (R0.8) at static-init time; execItemGet only consults them
//     under pc_rando();
//   - check_tingle_statue_owned and get_num_owned_tingle_statues, which the R3.6 hunks in game/ call
//     instead of dComIfGs_isStageTbox(stage, 0xF) / dComIfGs_getCollectMapNum() under pc_rando();
//   - a pc_rando listener that puts the quest-screen texts 503/703 in the message overlay at each
//     activation (the overlay is cleared at deactivation).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_tingle.h"
#include "d/d_com_inf_game.h"

namespace {

void giveStatue(int index) {
    dComIfGs_onEventBit(pc_rando_tingle_bit(index));
}

void dragonStatueItemGet() { giveStatue(0); }
void forbiddenStatueItemGet() { giveStatue(1); }
void goddessStatueItemGet() { giveStatue(2); }
void earthStatueItemGet() { giveStatue(3); }
void windStatueItemGet() { giveStatue(4); }

constexpr PcRandoItemFunc kItemFuncs[PCR_TINGLE_STATUE_COUNT] = {
    dragonStatueItemGet, forbiddenStatueItemGet, goddessStatueItemGet, earthStatueItemGet,
    windStatueItemGet,
};

void onRando(int active) {
    if (active) {
        pc_rando_tingle_set_messages();
    }
}

// tweaks.py update_tingle_statue_item_get_funcs: item_func_ptr[0xA3..0xA7] = the custom functions.
struct RegisterTingle {
    RegisterTingle() {
        for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
            pc_rando_item_override((uint8_t)(PCR_TINGLE_FIRST_ITEM + i), kItemFuncs[i]);
        }
        pc_rando_add_listener(onRando);
    }
} sRegisterTingle;

} // namespace

extern "C" BOOL pc_rando_tingle_statue_owned(int stage, int flag) {
    const uint16_t bit = pc_rando_tingle_bit_for_stage(stage, flag);
    if (bit == 0) {
        return FALSE;
    }
    return dComIfGs_isEventBit(bit) ? TRUE : FALSE;
}

extern "C" int pc_rando_tingle_statue_count(void) {
    int n = 0;
    for (int i = 0; i < PCR_TINGLE_STATUE_COUNT; i++) {
        n += pc_rando_tingle_statue_owned(PCR_TINGLE_FIRST_STAGE + i, PCR_TINGLE_CHEST_FLAG) ? 1 : 0;
    }
    return n;
}
