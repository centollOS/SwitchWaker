// The pirate ship at Windfall, game side (docs/RANDOMIZER_PLAN.md step R3.17; the pure part is
// features/rando/pc_rando_tweak_pirate_ship.{h,cpp}). Declared in native/include/pc/pc_rando_hooks.h
// (section R3.17).
//   - pc_rando_pirate_ship_init (pc_rando_patchers.def): registers the pure patchers and a patcher
//     that builds messages 3008/849/850 (the message font's widths and the BMG's entry of 3008 are
//     game data) at the first archive load after an activation, once the font and the BMG are there;
//   - pc_rando_pirate_ship_scene_wave2: the JAIZelBasic::setScene hunk (Asoko's stage BGM entry takes
//     the free second dynamic scene wave slot 0x0E, which this fills with IsleLink_0.aw).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"

#include "pc/pc_rando.h"
#include "features/rando/pc_rando_msg.h"
#include "features/rando/pc_rando_resources.h"
#include "features/rando/pc_rando_tweak_pirate_ship.h"
#include "features/rando/pc_rando_wrap.h"

#include "JAZelAudio/JAIZelBasic.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_msg_mng.h"

#include <cstdio>
#include <cstring>

namespace {

bool messagesSet() {
    return pc_rando_msg_find(PCR_PS_MSG_ARYLL) != nullptr && pc_rando_msg_find(PCR_PS_MSG_STUCK) != nullptr &&
           pc_rando_msg_find(PCR_PS_MSG_OPEN) != nullptr;
}

// The BMG's own INF1 entry of msg_no (not the overlay's, not QoL's instant-text draw type).
bool bmgEntry(uint16_t msg_no, PcRandoMsgEntry* out) {
    if (dComIfGp_getMsgDtArchive() == nullptr) {
        return false;
    }
    fopMsgM_msgGet_c get;
    mesg_header* head = get.getMesgHeader(msg_no);
    if (head == nullptr) {
        return false;
    }
    get.getMessage(head); // finds the entry's index (mMsgIdx)
    // INF1 (mesg_info, private to f_op_msg_mng.cpp; gclib bmg.py read_inf1): the 8-byte block header,
    // u16 entry count at +0x08, entry size, group, colour, then the entries at +0x10, all big-endian
    // disc bytes.
    static_assert(sizeof(JMSMesgEntry_c) == PCR_MSG_ENTRY_SIZE, "BMG INF1 entry size");
    const uint8_t* info = (const uint8_t*)get.getMesgInfo(head);
    const uint32_t count = ((uint32_t)info[8] << 8) | info[9];
    const uint8_t* raw = info + 0x10 + PCR_MSG_ENTRY_SIZE * get.mMsgIdx;
    if (get.mMsgIdx >= count || (((uint32_t)raw[4] << 8) | raw[5]) != msg_no) {
        return false;
    }
    pc_rando_msg_entry_unpack(raw, out);
    return true;
}

// Runs at every archive load while pc_rando() is 1 (the R0.10 dispatcher only runs then); the
// overlay is cleared at deactivation, so the texts come back at the first load after the next
// activation (a debug boot activates before the logo scene hands over the font and the BMG: the
// next loads retry).
void messagePatcher(PcRandoResFile*) {
    if (!messagesSet()) {
        pc_rando_pirate_ship_build_messages();
    }
}

bool sSlotRefused = false;

} // namespace

extern "C" int pc_rando_pirate_ship_build_messages(void) {
    const pcr::GlyphWidths* widths = pc_rando_font_widths();
    if (widths == nullptr) {
        return 0;
    }
    // The vanilla entry, without this step's own record.
    pc_rando_msg_remove(PCR_PS_MSG_ARYLL);
    PcRandoMsgEntry entry;
    if (!bmgEntry(PCR_PS_MSG_ARYLL, &entry)) {
        return 0;
    }
    const int set = pc_rando_tweak_pirate_ship_messages(*widths, entry);
    std::fprintf(stderr, "[cos] rando: pirate ship: messages %d/%d/%d set (%d)\n", PCR_PS_MSG_ARYLL, PCR_PS_MSG_STUCK,
                 PCR_PS_MSG_OPEN, set);
    return set;
}

extern "C" uint8_t pc_rando_pirate_ship_scene_wave2(int sceneNum, uint8_t vanilla) {
    if (sceneNum != PCR_PS_ASOKO_SCENE || sSlotRefused) {
        return vanilla;
    }
    // wwrando's second main.dol write: the slot's first wave. Slots 0x0E-0x13 are free (no stage or
    // island entry points at them), so the fill stays after a deactivation: a vanilla game never reads
    // it, and a wave loaded through it is still erased when the next scene changes the slot.
    u8* slot = JAIZelBasic::m_dy_wave_set_2nd[PCR_PS_SCENE_WAVE2];
    if (slot[0] != PCR_PS_ISLE_LINK_WAVE || slot[1] != 0) {
        if (slot[0] != 0 || slot[1] != 0) {
            std::fprintf(stderr, "[cos] rando: pirate ship: scene wave slot 0x%02X is not free (%02X %02X); Aryll stays silent\n",
                         PCR_PS_SCENE_WAVE2, slot[0], slot[1]);
            sSlotRefused = true;
            return vanilla;
        }
        slot[0] = PCR_PS_ISLE_LINK_WAVE;
    }
    return PCR_PS_SCENE_WAVE2;
}

// pc_rando_patchers.def (R0.10's archive-load hook calls it before the first dispatch).
extern "C" void pc_rando_pirate_ship_init(void) {
    pc_rando_tweak_pirate_ship_init();
    pc_rando_register_res_patcher(messagePatcher, PCR_RES_ANY);
}
