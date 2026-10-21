// The game side of the randomizer's Hurricane Spin (docs/RANDOMIZER_PLAN.md step R2.4;
// flexible_item_locations.asm "Hurricane Spin split from Orca's event", custom_funcs.asm
// hurricane_spin_item_func):
//   - the item-get override of 0xAA (R0.8 registry), registered at static-init time: it sets the
//     unused event bit 0x6901 (plan D17) instead of the vanilla noentry function, so Hurricane Spin
//     can be given by any location;
//   - pc_rando_hurricane_spin_owned, which daPy_lk_c::procCutTurnMove_init asks under pc_rando()
//     instead of the bit of Orca's teaching event (0x0B20). The hunk's call also keeps this object
//     file linked (cos_pc is a static library), so the static registration runs.
// The event placement itself is the pure features/rando/pc_rando_place_event.cpp; the Hurricane
// Spin field and get models are R2.11's (plan D12: the Wind Waker's resource).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

namespace {

// custom_funcs.asm hurricane_spin_item_func: onEventBit(0x6901).
void hurricaneSpinItemGet() {
    dComIfGs_onEventBit(PC_RANDO_BIT_HURRICANE_SPIN);
}

// flexible_item_locations.asm: item_func_ptr[0xAA] (0x80388B70) = hurricane_spin_item_func.
struct RegisterHurricaneSpin {
    RegisterHurricaneSpin() { pc_rando_item_override(dItemNo_HURRICANE_SPIN_e, hurricaneSpinItemGet); }
} sRegisterHurricaneSpin;

} // namespace

extern "C" int pc_rando_hurricane_spin_owned(void) {
    return dComIfGs_isEventBit(PC_RANDO_BIT_HURRICANE_SPIN) ? 1 : 0;
}
