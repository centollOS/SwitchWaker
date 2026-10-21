// The randomizer's sword modes, game side (docs/RANDOMIZER_PLAN.md step R6.3): wwrando 1.10.0_9775811
// asm/patches/swordless.asm (the main.dol part: a temporary Hero's Sword for the Ganondorf fight and in
// Orca's house, removed at the next stage load) and tweaks.py update_text_for_swordless (messages
// 1128, 1590). The d_a_fganon part (hammer damage) is a hunk in d_a_fganon.cpp's damage_check. The
// pool (R1.4) and the new-game sword and barrier bits (R3.1) are earlier steps'. Declared in
// native/include/pc/pc_rando_hooks.h (section R6.3); called only under pc_rando() from the hunks
// listed in docs/RANDO_HOOKS.md, section R6.3. The two texts go in the message overlay at every
// activation of a Swordless seed (a listener, as pc_rando_hooks_keybag.cpp does; the overlay is cleared
// when pc_rando() turns 0). The phase_4 hunks reference this file, so the linker keeps the listener.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_msg.h"

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"

#include <cstdio>
#include <cstring>

namespace {

// tweaks.py update_text_for_swordless, strings as wwrando writes them.
const char kText1128[] = PCR_MSG_PLAYER_NAME ", you may not have the\nMaster Sword, but do not be afraid!\n\n\n"
                         "The hammer of the dead is all you\nneed to crush your foe...\n\n\n"
                         "Even as his ball of fell magic bears down\non you, you can " PCR_MSG_COLOR(01)
                         "knock it back\nwith an empty bottle" PCR_MSG_COLOR(00) "!\n\n"
                         "...I am sure you will have a shot at victory!";
const char kText1590[] = PCR_MSG_PLAYER_NAME "! Do not run! Trust in the\npower of the Skull Hammer!";

// dSv_player_status_a_c::mSelectEquip[0] (0x803C4C08 + 0xE): the equipped sword; 0xFF none.
bool swordEquipped() {
    return dComIfGs_getSelectEquip(0) != dItemNo_NONE_e;
}

void equipHerosSword() {
    dComIfGs_setSelectEquip(0, dItemNo_SWORD_e); // li r0, 0x38; stb r0, 0xE (r4)
}

void onRando(int active) {
    if (active) {
        pc_rando_sword_sync_texts();
    }
}

// Registered before main; under COS_RANDO_FORCE=1 the listener runs at once (default config: no texts).
struct Register {
    Register() { pc_rando_add_listener(onRando); }
} sRegister;

} // namespace

extern "C" int pc_rando_swordless(void) {
    return pc_rando() && pc_rando_cfg()->sword_mode == PC_RANDO_SWORD_SWORDLESS ? 1 : 0;
}

extern "C" int pc_rando_sword_sync_texts(void) {
    if (!pc_rando_swordless()) {
        // A re-activation with another config (a forced boot, a smoke) keeps the overlay; no other step
        // writes these two messages.
        pc_rando_msg_remove(1128);
        pc_rando_msg_remove(1590);
        return 0;
    }
    const PcRandoMsg* a = pc_rando_msg_find(1128);
    const PcRandoMsg* b = pc_rando_msg_find(1590);
    if (a != nullptr && a->bytes != nullptr && b != nullptr && b->bytes != nullptr) {
        return 1;
    }
    const int ra = pc_rando_msg_set_text(1128, kText1128);
    const int rb = pc_rando_msg_set_text(1590, kText1590);
    if (ra != PCR_MSG_OK || rb != PCR_MSG_OK) {
        std::fprintf(stderr, "[cos] rando: swordless texts not set (1128: %d, 1590: %d)\n", ra, rb);
        return 0;
    }
    return 1;
}

extern "C" void pc_rando_sword_ganondorf_stage(void) {
    if (!pc_rando_swordless()) {
        return;
    }
    // give_temporary_sword_during_ganondorf_fight_in_swordless: any equipped sword is kept.
    if (!swordEquipped()) {
        equipHerosSword();
    }
}

extern "C" int pc_rando_sword_stage_load(void) {
    if (!pc_rando_swordless()) {
        return 1;
    }
    // give_temporary_sword_in_orcas_house_in_swordless: strcmp(current stage, "Ojhous").
    if (std::strcmp(dComIfGp_getStartStageName(), "Ojhous") == 0 && !swordEquipped()) {
        equipHerosSword();
        // The asm returns past both the removal below and the bow check's load of the bow slot; r0 then
        // holds 0x38, so the "bow is 0xFF" test fails and the bow is not given back on this load.
        return 0;
    }
    // remove_temporary_sword_when_loading_stage_in_swordless: lbz r0, 0xB4 (r29) is the owned swords
    // bitfield, dSv_player_collect_c::mCollect[0].
    if (g_dComIfG_gameInfo.save.getPlayer().getCollect().checkCollect(0) == 0) {
        dComIfGs_setSelectEquip(0, dItemNo_NONE_e);
    }
    return 1;
}
