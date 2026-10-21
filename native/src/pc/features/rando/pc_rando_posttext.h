// The randomizer's message texts that depend on the seed (docs/RANDOMIZER_PLAN.md step R3.21): the
// texts that wwrando writes into zel_00.bmg once it knows the placement, plus the fixed rewrites of
// vanilla messages that its apply_necessary_tweaks makes for every seed. They are built at every
// activation (game_hooks/pc_rando_hooks_posttext.cpp) and go into the message overlay
// (pc_rando_msg.h, decision D10).
//
// Ports (wwrando 1.10.0_9775811 tweaks.py, quirks included; strings in wwrando's string syntax):
//   randomizer.py apply_necessary_post_randomization_tweaks (only when items are randomized):
//     update_shop_item_descriptions        3906, 3909, 12106-12111
//     update_auction_item_names            7440-7443
//     update_battlesquid_item_names        7520, 7521 (word wrapped with the message font)
//     update_item_names_in_letter_advertising_rock_spire_shop   3325 (wrapped; from the BMG's text)
//   randomizer.py apply_necessary_tweaks (every seed):
//     update_korl_dialogue                 3443
//     add_chart_number_to_item_get_messages   101 + id of every "Treasure Chart N" and
//                                          "Triforce Chart N" item (from the BMG's text)
//     update_beedle_spoil_selling_text     3957 (from the BMG's text)
//   make_sail_behave_like_swift_sail, when the seed's swift_sail option is on:   463 "Swift Sail"
//
// The swordless texts (update_text_for_swordless: 1128, 1590) belong to the sword mode step, not here.
//
// Pure: no game headers and no game calls (cos_rando_core and the unit tests link it). The game side
// passes the BMG's vanilla texts, the item names of the placement and the font's glyph widths in
// a PostTextSource.
#pragma once

#include "pc_rando_wrap.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace pcr {

struct PostTextSource {
    // The message font's widths; nullptr skips the wrapped messages (7520, 7521, 3325).
    const GlyphWidths* widths = nullptr;
    // apply_necessary_post_randomization_tweaks (wwrando's randomize_items; always on for a seed).
    bool post_randomization = true;
    // The seed's swift_sail option (message 463).
    bool swift_sail = true;
    // done_item_locations[location]: the logic item name placed at location `loc` (an index into
    // pcr::locations()). An empty name skips the messages that name it.
    std::string (*item_name)(void* ctx, int loc) = nullptr;
    // The BMG's own text of msg_no in wwrando's string syntax and its INF1 text box type. False when
    // the BMG has no such message (or is not loaded): the messages built from it are skipped.
    bool (*vanilla)(void* ctx, uint16_t msg_no, std::string* text, uint8_t* text_box_type) = nullptr;
    void* ctx = nullptr;
};

struct PostText {
    uint16_t msg_no;
    std::string text; // wwrando string syntax
};

// Builds every text in wwrando's order (the fixed tweaks first, then the post-randomization ones).
// A message that cannot be built (missing BMG text, item name or font) is left out and its number
// appended to *skipped (when not NULL). A chart message whose text has nothing to replace is left
// out without being counted as skipped (wwrando writes it back unchanged).
void buildPostTexts(const PostTextSource& src, std::vector<PostText>* out, std::vector<uint16_t>* skipped);

// Every message number buildPostTexts may write, so the caller can drop old overlay records before
// it reads the BMG's text.
std::vector<uint16_t> postTextMessageNumbers();

// The item-get message numbers of the numbered charts (101 + item id, item_names.txt order).
std::vector<uint16_t> chartMessageNumbers();

// Puts each text in the overlay (pc_rando_msg_set_text). Returns how many were accepted.
int applyPostTexts(const std::vector<PostText>& texts);

// tweaks.py update_item_names_in_letter_advertising_rock_spire_shop on its own: the letter's text
// with lines 8-11 replaced by the three item names' hint, wrapped two spaces narrower and indented by
// two spaces.
std::string rockSpireLetter(const GlyphWidths& w, uint8_t text_box_type, const std::string& vanilla,
                            const std::string& item1, const std::string& item2, const std::string& item3);

} // namespace pcr
