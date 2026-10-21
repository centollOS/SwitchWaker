// The pure part of the randomizer's key bag text (docs/RANDOMIZER_PLAN.md step R3.5); see
// pc_rando_keybag.h.
#include "pc_rando_keybag.h"

#include "pc_rando_msg.h"

#include <cstdio>
#include <cstring>

int pc_rando_keybag_stage(int code) {
    if (code < PCR_KEYBAG_CMD_FIRST || code > PCR_KEYBAG_CMD_LAST) {
        return -1;
    }
    // "The text command ID ranges from 0x4B-0x4F, for DRC, FW, TotG, ET, and WT. The dungeon stage IDs
    // for those same 5 dungeons range from 3-7. So just subtract 0x48."
    return code - 0x48;
}

int pc_rando_keybag_reads_current(int cur_stage, int stage) {
    return cur_stage == stage ? 1 : 0;
}

int pc_rando_keybag_format(int keys, int has_big_key, char* out, size_t cap) {
    if (out == nullptr || cap == 0) {
        return -1;
    }
    out[0] = '\0';
    if (keys < 0) {
        return -1;
    }
    // fopMsgM_int_to_char(buf, keys, false) prints up to five digits with no leading zeros; a key count
    // is a u8, so %d gives the same text. Then key_text_command_has_big_key_text (" +Big") or
    // key_text_command_does_not_have_big_key_text (five spaces).
    const int n = snprintf(out, cap, "%d%s", keys, has_big_key ? " +Big" : "     ");
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return -1;
    }
    return n;
}

const char* pc_rando_keybag_name_text(void) {
    return "Key Bag";
}

const char* pc_rando_keybag_desc_text(void) {
    // tweaks.py implement_key_bag, verbatim.
    return "A handy bag for holding your keys!\n"
           "Here's how many you've got with you:\n"
           "DRC: \\{1A 05 00 00 4B}    "
           "FW: \\{1A 05 00 00 4C}    "
           "TotG: \\{1A 05 00 00 4D}\n"
           "ET: \\{1A 05 00 00 4E}      "
           "WT: \\{1A 05 00 00 4F}";
}

int pc_rando_keybag_set_messages(void) {
    const int a = pc_rando_msg_set_text(PCR_KEYBAG_MSG_NAME, pc_rando_keybag_name_text());
    const int b = pc_rando_msg_set_text(PCR_KEYBAG_MSG_DESC, pc_rando_keybag_desc_text());
    return a == PCR_MSG_OK && b == PCR_MSG_OK ? 1 : 0;
}
