// The game side of the randomizer's progressive items (docs/RANDOMIZER_PLAN.md step R3.3, asm group
// G2; the tier tables are the pure features/rando/pc_rando_progressive.cpp):
//   - pc_rando_convert_progressive_item_id (custom_funcs.asm convert_progressive_item_id), which the
//     R3.3 hunks in game/ call under pc_rando() and R3.1's starting gear loop uses;
//   - the 8 progressive item functions and normal_magic_meter_item_func (custom_funcs.asm), registered
//     on the 20 progressive ids with R0.8's override registry (tweaks.py make_items_progressive's
//     item_func_ptr writes). The registry is only consulted under pc_rando(), so registering them at
//     static-init time changes nothing for a game that is not randomized;
//   - message 101+0xB1 (the normal magic meter's item-get text), added to the overlay at every
//     activation (the overlay is cleared when pc_rando() turns 0).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_items.h"
#include "features/rando/pc_rando_progressive.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"

namespace {

PcRandoProgState readState() {
    dSv_player_c& player = g_dComIfG_gameInfo.save.getPlayer();
    PcRandoProgState st;
    st.swords = dComIfGs_checkCollect(0);
    st.shields = dComIfGs_checkCollect(1);
    st.bow = player.getGetItem().mItemFlags[dInvSlot_BOW_e];
    st.wallet = dComIfGs_getWalletSize();
    st.bomb_max = dComIfGs_getBombMax();
    st.arrow_max = dComIfGs_getArrowMax();
    st.picto = player.getGetItem().mItemFlags[dInvSlot_CAMERA_e];
    st.max_magic = dComIfGs_getMaxMagic();
    return st;
}

int tierOf(int family) {
    const PcRandoProgState st = readState();
    return pc_rando_progressive_tier(family, &st);
}

// custom_funcs.asm normal_magic_meter_item_func: vanilla item_func_magic_power is an empty stub. The
// asm stores 16 into the two "magic to add" counters (0x803CA780 current, 0x803CA784 max); the port's
// setters add, so they add the difference.
void normalMagicMeterItemFunc() {
    dComIfGp_setItemMagicCount(16 - dComIfGp_getItemMagicCount());
    dComIfGp_setItemMaxMagicCount(16 - dComIfGp_getItemMaxMagicCount());
}

void progressiveSwordItemFunc() {
    switch (tierOf(PCR_PROG_SWORD)) {
    case 0: item_func_sword(); break;
    case 1: item_func_master_sword(); break;
    case 2: item_func_lv3_sword(); break;
    case 3: item_func_master_sword_ex(); break;
    default: break;
    }
}

void progressiveShieldItemFunc() {
    switch (tierOf(PCR_PROG_SHIELD)) {
    case 0: item_func_shield(); break;
    case 1: item_func_mirror_shield(); break;
    default: break;
    }
}

void progressiveBowItemFunc() {
    switch (tierOf(PCR_PROG_BOW)) {
    case 0: item_func_bow(); break; // its 30-arrow writes are R3.3 no-ops under pc_rando()
    case 1: item_func_magic_arrow(); break;
    case 2: item_func_light_arrow(); break;
    default: break;
    }
}

void progressiveWalletItemFunc() {
    switch (tierOf(PCR_PROG_WALLET)) {
    case 0: dComIfGs_setWalletSize(1); break;
    case 1: dComIfGs_setWalletSize(2); break;
    default: break;
    }
}

void progressiveBombBagItemFunc() {
    u8 n = 0;
    switch (tierOf(PCR_PROG_BOMB_BAG)) {
    case 0: n = 60; break;
    case 1: n = 99; break;
    default: return;
    }
    dComIfGs_setBombNum(n);
    dComIfGs_setBombMax(n);
}

void progressiveQuiverItemFunc() {
    u8 n = 0;
    switch (tierOf(PCR_PROG_QUIVER)) {
    case 0: n = 60; break;
    case 1: n = 99; break;
    default: return;
    }
    dComIfGs_setArrowNum(n);
    dComIfGs_setArrowMax(n);
}

void progressivePictoBoxItemFunc() {
    switch (tierOf(PCR_PROG_PICTO_BOX)) {
    case 0: item_func_camera(); break;
    case 1: item_func_camera2(); break;
    default: break;
    }
}

void progressiveMagicMeterItemFunc() {
    switch (tierOf(PCR_PROG_MAGIC_METER)) {
    case 0: normalMagicMeterItemFunc(); break;
    case 1: item_func_max_mp_up1(); break;
    default: break;
    }
}

const PcRandoItemFunc kFuncs[PCR_PROG_COUNT] = {
    progressiveSwordItemFunc,   progressiveShieldItemFunc,   progressiveBowItemFunc,
    progressiveWalletItemFunc,  progressiveBombBagItemFunc,  progressiveQuiverItemFunc,
    progressivePictoBoxItemFunc, progressiveMagicMeterItemFunc,
};

void onRando(int active) {
    if (active) {
        pc_rando_progressive_add_messages();
    }
}

struct Register {
    Register() {
        for (int i = 0; i < PCR_PROG_ID_COUNT; i++) {
            const u8 id = pc_rando_progressive_id(i);
            pc_rando_item_override(id, kFuncs[pc_rando_progressive_family(id)]);
        }
        pc_rando_add_listener(onRando); // called with 1 at once under COS_RANDO_FORCE=1
    }
} sRegister;

} // namespace

extern "C" int pc_rando_convert_progressive_item_id(int item_no) {
    if (item_no < 0 || item_no > 0xFF) {
        return item_no;
    }
    const PcRandoProgState st = readState();
    return pc_rando_progressive_convert((uint8_t)item_no, &st);
}
