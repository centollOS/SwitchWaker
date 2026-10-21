// R3.5 (docs/RANDOMIZER_PLAN.md): the pure part of the key bag text (pc_rando_keybag.h):
//  1. text command -> dungeon stage (0x4B-0x4F -> 3-7, anything else -1) and the current/saved choice;
//  2. the expansion: decimal count, then " +Big" or five spaces (exec_curr_num_keys_text_command);
//  3. messages 403/603 (tweaks.py implement_key_bag): 603 encodes to plain ASCII with one 5-byte tag
//     1A 05 00 00 <code> per dungeon, in order, and pc_rando_keybag_set_messages puts both texts in
//     the overlay as text-only records (the BMG entries are kept).
#include "rando_test.h"

#include "pc_rando_keybag.h"
#include "pc_rando_msg.h"

#include <cstring>
#include <string>
#include <vector>

int main() {
    // 1.
    for (int code = 0x4B; code <= 0x4F; code++) {
        RT_CHECK_EQ(pc_rando_keybag_stage(code), code - 0x48);
    }
    RT_CHECK_EQ(pc_rando_keybag_stage(0x4A), -1);
    RT_CHECK_EQ(pc_rando_keybag_stage(0x50), -1);
    RT_CHECK_EQ(pc_rando_keybag_stage(0), -1);
    RT_CHECK_EQ(pc_rando_keybag_reads_current(3, 3), 1);
    RT_CHECK_EQ(pc_rando_keybag_reads_current(0, 3), 0);
    RT_CHECK_EQ(pc_rando_keybag_reads_current(4, 3), 0);

    // 2.
    char buf[32];
    RT_CHECK_EQ(pc_rando_keybag_format(0, 0, buf, sizeof buf), 6);
    RT_CHECK(std::strcmp(buf, "0     ") == 0);
    RT_CHECK_EQ(pc_rando_keybag_format(2, 1, buf, sizeof buf), 6);
    RT_CHECK(std::strcmp(buf, "2 +Big") == 0);
    RT_CHECK_EQ(pc_rando_keybag_format(12, 0, buf, sizeof buf), 7);
    RT_CHECK(std::strcmp(buf, "12     ") == 0);
    RT_CHECK_EQ(pc_rando_keybag_format(255, 1, buf, sizeof buf), 8);
    RT_CHECK(std::strcmp(buf, "255 +Big") == 0);
    RT_CHECK_EQ(pc_rando_keybag_format(1, 1, buf, 6), -1); // "1 +Big" needs 7 bytes
    RT_CHECK_EQ(buf[0], '\0');
    RT_CHECK_EQ(pc_rando_keybag_format(1, 1, buf, 7), 6);
    RT_CHECK_EQ(pc_rando_keybag_format(-1, 0, buf, sizeof buf), -1);
    RT_CHECK_EQ(pc_rando_keybag_format(1, 0, nullptr, 8), -1);

    // 3.
    RT_CHECK(std::strcmp(pc_rando_keybag_name_text(), "Key Bag") == 0);
    size_t len = 0;
    RT_CHECK_EQ(pc_rando_msg_encode(pc_rando_keybag_desc_text(), nullptr, 0, &len), PCR_MSG_ERR_SPACE);
    std::vector<uint8_t> bytes(len);
    RT_CHECK_EQ(pc_rando_msg_encode(pc_rando_keybag_desc_text(), bytes.data(), bytes.size(), &len), PCR_MSG_OK);
    // Expand each tag with a stand-in and compare with the text wwrando shows.
    std::string shown;
    int tags = 0;
    for (size_t i = 0; i + 1 < len; i++) {
        if (bytes[i] == 0x1A) {
            RT_CHECK(i + 5 <= len);
            RT_CHECK_EQ(bytes[i + 1], 5);
            RT_CHECK_EQ(bytes[i + 2], 0);
            RT_CHECK_EQ(bytes[i + 3], 0);
            RT_CHECK_EQ(bytes[i + 4], 0x4B + tags);
            shown += "<" + std::to_string(bytes[i + 4] - 0x48) + ">";
            tags++;
            i += 4;
        } else {
            RT_CHECK(bytes[i] >= 0x0A && bytes[i] < 0x80);
            shown += (char)bytes[i];
        }
    }
    RT_CHECK_EQ(bytes[len - 1], 0);
    RT_CHECK_EQ(tags, 5);
    RT_CHECK(shown == "A handy bag for holding your keys!\n"
                      "Here's how many you've got with you:\n"
                      "DRC: <3>    FW: <4>    TotG: <5>\n"
                      "ET: <6>      WT: <7>");

    pc_rando_msg_clear();
    RT_CHECK_EQ(pc_rando_keybag_set_messages(), 1);
    const PcRandoMsg* name = pc_rando_msg_find(PCR_KEYBAG_MSG_NAME);
    RT_CHECK(name != nullptr && !name->has_entry && name->len == 8 &&
             std::memcmp(name->bytes, "Key Bag", 8) == 0);
    const PcRandoMsg* desc = pc_rando_msg_find(PCR_KEYBAG_MSG_DESC);
    RT_CHECK(desc != nullptr && !desc->has_entry && desc->len == len &&
             std::memcmp(desc->bytes, bytes.data(), len) == 0);
    RT_CHECK_EQ(pc_rando_msg_count(), 2);
    pc_rando_msg_clear();
    return rt::finish();
}
