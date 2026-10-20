// R3.6 (docs/RANDOMIZER_PLAN.md): the pure part of the Tingle Statues (pc_rando_tingle.h):
//  1. item -> event bit (custom_funcs.asm *_tingle_statue_item_get_func) and stage/flag -> event bit
//     (check_tingle_statue_owned, including its "invalid" cases);
//  2. the quest-screen texts 503/703 (tweaks.py show_number_of_tingle_statues_on_quest_status_screen)
//     encode to the bytes gclib's write_string gives, and pc_rando_tingle_set_messages puts them in
//     the message overlay.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_msg.h"
#include "pc_rando_tingle.h"

#include <cstring>
#include <string>

int main() {
    // 1. tables
    const uint16_t bits[5] = {0x6A04, 0x6A08, 0x6A10, 0x6A20, 0x6A40};
    for (int i = 0; i < 5; i++) {
        RT_CHECK_EQ(pc_rando_tingle_bit(i), bits[i]);
        RT_CHECK_EQ(pc_rando_tingle_bit_for_item((uint8_t)(0xA3 + i)), bits[i]);
        RT_CHECK_EQ(pc_rando_tingle_bit_for_stage(3 + i, 0xF), bits[i]);
        RT_CHECK_EQ(pc_rando_tingle_bit_for_stage(3 + i, 0xE), 0);
        RT_CHECK_EQ(pc_rando_tingle_bit_for_stage(3 + i, 15 + 16), 0);
    }
    RT_CHECK_EQ(pc_rando_tingle_bit(-1), 0);
    RT_CHECK_EQ(pc_rando_tingle_bit(5), 0);
    RT_CHECK_EQ(pc_rando_tingle_bit_for_item(0xA2), 0);
    RT_CHECK_EQ(pc_rando_tingle_bit_for_item(0xA8), 0); // Tingle Statue 6, unused
    RT_CHECK_EQ(pc_rando_tingle_bit_for_stage(2, 0xF), 0);
    RT_CHECK_EQ(pc_rando_tingle_bit_for_stage(8, 0xF), 0);
    RT_CHECK_EQ(pc_rando_tingle_bit_for_stage(0, 0xF), 0);
    // The 5 bits lie in the range plan D17 reserves for wwrando (0x6A01-0x6A40) and are distinct.
    for (int i = 0; i < 5; i++) {
        RT_CHECK(bits[i] >= 0x6A01 && bits[i] <= 0x6A40);
    }

    // 2. texts
    RT_CHECK(pc_rando_tingle_msg_text(502) == nullptr);
    RT_CHECK(std::strcmp(pc_rando_tingle_msg_text(503), "Tingle Statues") == 0);
    const std::string red("\x1A\x06\xFF\x00\x00\x01", 6);
    const std::string def("\x1A\x06\xFF\x00\x00\x00", 6);
    const std::string want703 = "Golden statues of a mysterious dashing\nfigure. They can be traded to " + red +
                                "Ankle" + def + " on\n" + red + "Tingle Island" + def + " for a reward!";
    uint8_t buf[512];
    size_t len = 0;
    RT_CHECK_EQ(pc_rando_msg_encode(pc_rando_tingle_msg_text(703), buf, sizeof buf, &len), PCR_MSG_OK);
    RT_CHECK(std::string((const char*)buf, len) == want703 ||
             (len == want703.size() + 1 && std::string((const char*)buf, len - 1) == want703));

    pc_rando_msg_clear();
    RT_CHECK_EQ(pc_rando_tingle_set_messages(), 1);
    const PcRandoMsg* m503 = pc_rando_msg_find(503);
    const PcRandoMsg* m703 = pc_rando_msg_find(703);
    RT_CHECK(m503 != nullptr && m703 != nullptr);
    if (m503 != nullptr && m703 != nullptr) {
        RT_CHECK(!m503->has_entry && !m703->has_entry); // the BMG entries stay
        RT_CHECK(m503->bytes != nullptr && std::strcmp((const char*)m503->bytes, "Tingle Statues") == 0);
        RT_CHECK(m703->bytes != nullptr && m703->len == want703.size() + 1 &&
                 std::memcmp(m703->bytes, want703.data(), want703.size()) == 0);
    }
    pc_rando_msg_clear();
    return rt::finish();
}
