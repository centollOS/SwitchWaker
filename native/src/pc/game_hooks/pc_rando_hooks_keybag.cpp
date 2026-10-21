// The game side of the randomizer's key bag text (docs/RANDOMIZER_PLAN.md step R3.5; the pure part is
// features/rando/pc_rando_keybag.{h,cpp}):
//   - pc_rando_text_command, called by the stringSet hunk (docs/RANDO_HOOKS.md, section R3.5) for the
//     new text commands 0x4B-0x4F: exec_curr_num_keys_text_command (misc_rando_features.asm);
//   - messages 403 ("Key Bag") and 603 (the description with the five commands), added to the message
//     overlay at every activation (tweaks.py implement_key_bag; the overlay is cleared when pc_rando()
//     turns 0).
// The stringSet hunk references pc_rando_text_command, so the linker keeps this file and its listener.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_rando.h"
#include "pc/pc_rando_hooks.h"
#include "features/rando/pc_rando_keybag.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"

#include <cstdio>
#include <unistd.h>

int pc_rando_text_command(int code, char* out, size_t cap) {
    const int stage = pc_rando_keybag_stage(code);
    if (stage < 0 || out == nullptr || cap == 0) {
        return 0;
    }
    // 0x803C53A4 is the current save stage (dSv_info_c's mDan.mStageNo); in that dungeon the count is
    // read from the loaded stage memory (0x803C5380), else from the dungeon's saved stage info
    // (0x803C4F88 + stage * 0x24). Byte 0x20 is the key count, bit 0x04 of byte 0x21 the Big Key.
    const int cur = g_dComIfG_gameInfo.save.getDan().mStageNo;
    dSv_memBit_c& mem = pc_rando_keybag_reads_current(cur, stage)
                            ? g_dComIfG_gameInfo.save.getMemory().getBit()
                            : g_dComIfG_gameInfo.save.getSavedata().getSave(stage).getBit();
    if (pc_rando_keybag_format(mem.getKeyNum(), mem.isDungeonItemBossKey() ? 1 : 0, out, cap) < 0) {
        return 0;
    }
    return 1;
}

namespace {

void onRando(int active) {
    if (active && !pc_rando_keybag_set_messages()) {
        static const char kMsg[] = "[cos] rando: key bag messages 403/603 not added\n";
        (void)!write(STDERR_FILENO, kMsg, sizeof kMsg - 1);
    }
}

// Registered before main, as pc_rando_hooks_misc.cpp does; under COS_RANDO_FORCE=1 the listener runs at
// once.
struct Register {
    Register() { pc_rando_add_listener(onRando); }
} sRegister;

} // namespace
