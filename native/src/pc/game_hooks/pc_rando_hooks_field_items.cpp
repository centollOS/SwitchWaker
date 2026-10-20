// The game side of "any item anywhere" (docs/RANDOMIZER_PLAN.md step R2.11; the pure part is
// features/rando/pc_rando_field_items.{h,cpp}):
//   - pc_rando_field_item_wait_arrow, which daItem_c::mode_wait (d_a_item.cpp) calls under pc_rando();
//   - the registration of the field_item_res / item_resource / item_info edits (R0.8's registry), at
//     static-init time from the real tables. They are constant-initialized (d_item_data.cpp has no
//     dynamic initializer), so they hold their boot bytes here whatever the order of static
//     constructors. The edits are seed-independent (wwrando applies the tweak to every seed) and reach
//     the tables only while pc_rando() is 1.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_field_items.h"
#include "d/d_item_data.h"

#include <cstdio>
#include <unistd.h>

extern "C" int pc_rando_field_item_wait_arrow(uint8_t item_no) {
    return pc_rando_field_item_uses_arrow(item_no);
}

namespace {

struct RegisterFieldItems {
    RegisterFieldItems() {
        if (pc_rando_field_items_register(dItem_data::item_resource, dItem_data::field_item_res,
                                          dItem_data::item_info) < 0) {
            static const char kMsg[] = "[cos] rando: field items: not every table edit was registered\n";
            (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
        }
    }
} sRegisterFieldItems;

} // namespace
