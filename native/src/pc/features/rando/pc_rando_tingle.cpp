// The Tingle Statues' pure tables and quest-screen texts (docs/RANDOMIZER_PLAN.md step R3.6; see
// pc_rando_tingle.h). No game code: this file links into cos_rando_core.
#include "pc_rando_tingle.h"

#include "pc_rando_msg.h"

namespace {

// custom_funcs.asm dragon/forbidden/goddess/earth/wind_tingle_statue_item_get_func.
constexpr uint16_t kBits[PCR_TINGLE_STATUE_COUNT] = {0x6A04, 0x6A08, 0x6A10, 0x6A20, 0x6A40};

// tweaks.py show_number_of_tingle_statues_on_quest_status_screen. 703 is the output of gclib's
// Message.word_wrap_string with the USA game font (text box type 11), as wwrando writes it.
constexpr const char kName[] = "Tingle Statues";
constexpr const char kDesc[] =
    "Golden statues of a mysterious dashing\n"
    "figure. They can be traded to " PCR_MSG_COLOR_RED "Ankle" PCR_MSG_COLOR_DEFAULT " on\n"
    PCR_MSG_COLOR_RED "Tingle Island" PCR_MSG_COLOR_DEFAULT " for a reward!";

} // namespace

extern "C" {

uint16_t pc_rando_tingle_bit(int index) {
    return index >= 0 && index < PCR_TINGLE_STATUE_COUNT ? kBits[index] : 0;
}

uint16_t pc_rando_tingle_bit_for_item(uint8_t item) {
    return pc_rando_tingle_bit((int)item - PCR_TINGLE_FIRST_ITEM);
}

uint16_t pc_rando_tingle_bit_for_stage(int stage, int flag) {
    // check_tingle_statue_owned: any flag but 0xF, or a stage outside 3-7, is "invalid" (false).
    if (flag != PCR_TINGLE_CHEST_FLAG) {
        return 0;
    }
    return pc_rando_tingle_bit(stage - PCR_TINGLE_FIRST_STAGE);
}

const char* pc_rando_tingle_msg_text(uint16_t msg_no) {
    switch (msg_no) {
    case PCR_TINGLE_MSG_NAME:
        return kName;
    case PCR_TINGLE_MSG_DESC:
        return kDesc;
    default:
        return nullptr;
    }
}

int pc_rando_tingle_set_messages(void) {
    const int a = pc_rando_msg_set_text(PCR_TINGLE_MSG_NAME, kName);
    const int b = pc_rando_msg_set_text(PCR_TINGLE_MSG_DESC, kDesc);
    return a == PCR_MSG_OK && b == PCR_MSG_OK ? 1 : 0;
}

} // extern "C"
