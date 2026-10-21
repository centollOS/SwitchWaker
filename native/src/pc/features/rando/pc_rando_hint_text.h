// The hint texts (docs/RANDOMIZER_PLAN.md step R5.2): the message strings wwrando 1.10.0_9775811
// writes for the hints R5.1 generated (pc_rando_gen_hints.h), plain or cryptic, with or without
// item importance, in BMG colour codes, word wrapped with the message font.
//
// Ports (randomizers/hints.py, quirks included; strings in wwrando's string syntax, pc_rando_msg.h):
//   Hint.formatted_place / formatted_reward, get_hint_item_name, get_formatted_item_name,
//   get_cryptic_item_hint, get_formatted_hint_text, and the text part of HintsRandomizer._save:
//     update_savage_labyrinth_hint_tablet          837
//     update_big_octo_great_fairy_item_name_hint   12015, 12016, 12017
//     update_fishmen_hints                         13026 + island (13027-13075)
//     update_hoho_hints                            14001 + Ho Ho index (14001-14010)
//     update_korl_hints                            1502, 3443-3448
// Colours (the BMG's \{1A 06 FF 00 00 nn} tags): 01 red (items, location names, Savage title),
// 02 required, 04 possibly required, 07 not required, 03 barren zones, 05 path zones and the octo
// fairy's text colour, 00 white.
//
// Not here (R5.3): putting the texts in the overlay at activation, the flexible_hint_locations
// hunk, rotate_hoho_to_face_hint (HintsResult::hohoLookIsland already holds his target island) and
// the order against R3.21's 3443 (wwrando's KoRL hints overwrite update_korl_dialogue's text:
// apply_necessary_tweaks runs before the randomizers save).
//
// Pure: no game headers and no game calls (cos_rando_core and the unit tests link it). The game side
// passes the font's glyph widths and the BMG's text box type of each message in a HintTextSource.
#pragma once

#include "pc_rando_gen_hints.h"
#include "pc_rando_wrap.h"

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace pcr {

// The message numbers.
constexpr uint16_t kMsgSavageTablet = 837;
constexpr uint16_t kMsgOctoFairyPlace = 12015, kMsgOctoFairyItem = 12016, kMsgOctoFairyAfter = 12017;
constexpr uint16_t kMsgFishmanBase = 13026; // + island number 1..49
constexpr uint16_t kMsgHohoBase = 14001;    // + Ho Ho index 0..9
extern const uint16_t kMsgKorlHints[7];     // 1502, 3443, 3444, 3445, 3446, 3447, 3448

// get_hint_item_name: "Triforce Chart 3" -> "Triforce Chart", "... Tingle Statue" -> "Tingle Statue",
// "DRC Small Key" -> "Small Key" and so on; any other name unchanged.
std::string hintItemName(std::string_view item);
// get_formatted_item_name: a dungeon key, map or compass gets its dungeon's full name ("Dragon Roost
// Cavern small key", "Wind Temple Big Key"), then add_article_before_item_name. False (and *err)
// where Python raises (a dungeon abbreviation Logic.DUNGEON_NAMES does not hold).
bool formattedItemName(std::string_view item, std::string* out, std::string* err = nullptr);
// get_cryptic_item_hint: progress_item_hints.txt's text for hintItemName(item). False (and *err) when
// the file has none (Python's KeyError; wwrando only asks for hintable progress items).
bool crypticItemHint(std::string_view item, std::string* out, std::string* err = nullptr);
// Hint.formatted_place: the place itself, or when cryptic zone_name_hints.txt's text (path, barren,
// item) or location_hints.txt's text (location). False (and *err) where Python raises (a missing
// entry; a cryptic fixed-location hint).
bool formattedPlace(const Hint& h, bool cryptic, std::string* out, std::string* err = nullptr);
// Hint.formatted_reward: path and barren hints give the reward as is (a barren hint's None is
// "None"), item and fixed-location hints the cryptic text or the formatted name, location hints
// always the formatted name.
bool formattedReward(const Hint& h, bool cryptic, std::string* out, std::string* err = nullptr);
// get_formatted_hint_text with its defaults ("They say that ", ".", 30 frames of wait).
bool formattedHintText(const Hint& h, bool cryptic, bool importance, std::string* out, std::string* err = nullptr,
                       std::string_view prefix = "They say that ", std::string_view suffix = ".", int delay = 30);

// check_item_can_be_hinted_at for a Savage Labyrinth floor hint, from the hint alone: its reward
// is hintable when it is a progress item (the hint's importance is set exactly then:
// get_importance_for_location), not a dungeon item unless key-lunacy, and not a trap. Equal to R5.1's
// checkItemCanBeHintedAt for the floors (test_R5_2 checks it on wwrando's seeds).
bool savageHintValid(const Hint& h, bool keylunacy);

struct HintTextSource {
    // The message font's widths. nullptr: nothing is built (every message is word wrapped).
    const GlyphWidths* widths = nullptr;
    // The seed's options.
    bool cryptic = true;      // cryptic_hints
    bool importance = false;  // hint_importance
    bool instant_text = true; // instant_text_boxes
    // check_item_can_be_hinted_at of the two Savage Labyrinth floors' rewards (savageHintValid).
    bool floor30_valid = false;
    bool floor50_valid = false;
    // The BMG's INF1 text box type of msg_no. False when the BMG has no such message: that message
    // is skipped.
    bool (*box_type)(void* ctx, uint16_t msg_no, uint8_t* type) = nullptr;
    void* ctx = nullptr;
};

struct HintText {
    uint16_t msg_no;
    std::string text; // wwrando string syntax
};

// Builds every hint text in HintsRandomizer._save's order: the Savage Labyrinth tablet; then, when
// the hints have an octo fairy hint (else wwrando leaves the rest vanilla), the octo fairy's three
// messages and each placement in h.placements order (fishmen by island, Ho Ho by index, KoRL's seven
// messages). A message whose box type is unknown is left out and appended to *skipped (when not
// NULL); with no font everything is skipped.
// False (and *err) where wwrando raises (a missing hint text, an octo fairy hint without importance
// when importance is shown, KoRL hints on with no hint: update_korl_hints's assert); the texts
// built before that stay in *out.
bool buildHintTexts(const HintsResult& h, const HintTextSource& src, std::vector<HintText>* out,
                    std::vector<uint16_t>* skipped, std::string* err = nullptr);

// Every message number buildHintTexts may write (so the caller can drop old overlay records).
std::vector<uint16_t> hintTextMessageNumbers();

// Puts each text in the overlay (pc_rando_msg_set_text). Returns how many were accepted.
int applyHintTexts(const std::vector<HintText>& texts);

} // namespace pcr
