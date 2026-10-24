// Game side of the cosmetic tweaks wwrando always applies (docs/RANDOMIZER_PLAN.md step R7.5a;
// features/rando/pc_rando_tweak_cosmetic.h for the pure part).
//
// make_tingle_statue_reward_rupee_rainbow_colored: item_resource[0xB8].mTevFrm becomes 7 (tweaks.py
// writes the u8 at item_resource + 0xB8*0x24 + 0x14), registered with R0.8's pc_rando_table_edit at
// each activation, so it is applied with the other table edits and undone at deactivation. The
// daItemBase_c::animEntry hunk (d_a_itembase.cpp) asks pc_rando_cosmetic_brk_frame for the frame.
// The register_renamed_item(0xB8, "Rainbow Rupee") half is the generator's item name, not game data.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_tweak_cosmetic.h"
#include "d/d_item_data.h"

#include <cstddef>
#include <unistd.h>

namespace {

int sEditOk = 0;

void onRando(int active) {
    if (!active) {
        return;
    }
    const s8 tevFrm = PCR_COSMETIC_RAINBOW_TEV_FRM;
    sEditOk = pc_rando_table_edit(PCR_TBL_ITEM_RESOURCE, PCR_COSMETIC_RAINBOW_ITEM,
                                  offsetof(dItem_data_item_resource, mTevFrm), &tevFrm, sizeof tevFrm);
    if (!sEditOk) {
        static const char kMsg[] = "[cos] rando cosmetic: the Rainbow Rupee table edit was refused\n";
        (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
    }
}

struct RegisterCosmetic {
    RegisterCosmetic() {
        if (!pc_rando_add_listener(onRando)) {
            static const char kMsg[] = "[cos] rando cosmetic: listener list full, the Rainbow Rupee stays vanilla\n";
            (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
        }
    }
} sRegisterCosmetic;

} // namespace

extern "C" float pc_rando_cosmetic_brk_frame(int tev_frm) {
    // check_animate_rainbow_rupee_color: cmpwi r0, 7; beq animate (else the vanilla int -> float).
    if (tev_frm == PCR_COSMETIC_RAINBOW_TEV_FRM) {
        return pc_rando_cosmetic_rainbow_step();
    }
    return (f32)tev_frm;
}

extern "C" int pc_rando_cosmetic_edit_ok(void) {
    return sEditOk;
}
