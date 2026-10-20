// The randomizer's misc-feature hooks (docs/RANDOMIZER_PLAN.md step R3.13: wwrando
// misc_rando_features.asm, custom_funcs.asm custom_isTbox_for_unloaded_stage_save_info, tweaks.py
// add_new_bog_warp and fix_message_closing_sound_on_quest_status_screen). Declared in
// native/include/pc/pc_rando_hooks.h; called only under pc_rando() from the hunks listed in
// docs/RANDO_HOOKS.md, section R3.13 (the message listener runs on every activation change).
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando_hooks.h"
#include "d/d_com_inf_game.h"
#include "features/rando/pc_rando_msg.h"

#include <cstdio>
#include <unistd.h>

// custom_isTbox_for_unloaded_stage_save_info: dSv_memBit_c::isTbox on the stage info list entry
// 0x803C4F88 + stage * 0x24, which is dSv_save_c's mMemory[stage] (the saved copy, not the loaded
// stage's dSv_info_c::mMemory).
int pc_rando_isTbox_unloaded(int stage_no, int no) {
    if (stage_no < 0 || stage_no >= dSv_save_c::STAGE_MAX || no < 0 || no >= 32) {
        return 0;
    }
    return g_dComIfG_gameInfo.save.getSavedata().getSave(stage_no).getBit().isTbox(no) ? 1 : 0;
}

int pc_rando_misc_add_messages(void) {
    // tweaks.py add_new_bog_warp: the 10th warp's confirmation.
    PcRandoMsgEntry bog;
    pc_rando_msg_entry_defaults(&bog);
    bog.text_box_type = 0;      // TextBoxType.DIALOG
    bog.initial_draw_type = 1;  // instant
    bog.text_box_position = 2;  // centered
    bog.num_lines_per_box = 2;
    const int a = pc_rando_msg_add(848, &bog,
                                   "Warp to " PCR_MSG_COLOR_RED "Forsaken Fortress" PCR_MSG_COLOR_DEFAULT "?");
    // tweaks.py fix_message_closing_sound_on_quest_status_screen: the quest screen's Options button reads
    // message 704 for its description; with none the BMG returns its last message, which could open and
    // close (and play the closing sound) on the frame the screen loads.
    PcRandoMsgEntry quest;
    pc_rando_msg_entry_defaults(&quest);
    quest.text_box_type = 9; // TextBoxType.ITEM_GET
    const int b = pc_rando_msg_add(704, &quest, "");
    if (a != PCR_MSG_OK || b != PCR_MSG_OK) {
        char buf[128];
        const int n = snprintf(buf, sizeof buf, "[cos] rando: misc messages not added (848: %d, 704: %d)\n", a, b);
        if (n > 0) {
            (void)!write(STDERR_FILENO, buf, (size_t)n);
        }
        return 0;
    }
    return 1;
}

namespace {

// The overlay is cleared when pc_rando() turns 0 (pc_rando_msg.cpp), so the messages are added again at
// every activation.
void onRando(int active) {
    if (active) {
        pc_rando_misc_add_messages();
    }
}

// Registered before main, as pc_rando_hooks_items.cpp binds its tables: pc_rando.cpp's listener list is
// constant-initialized. Under COS_RANDO_FORCE=1 the listener runs at once.
struct Register {
    Register() { pc_rando_add_listener(onRando); }
} sRegister;

} // namespace
