// The hint texts (docs/RANDOMIZER_PLAN.md step R5.2). See pc_rando_hint_text.h.
#include "pc_rando_hint_text.h"

#include "pc_rando_data.h"
#include "pc_rando_msg.h"

#include <stdio.h>
#include <string.h>

namespace pcr {

const uint16_t kMsgKorlHints[7] = {1502, 3443, 3444, 3445, 3446, 3447, 3448};

namespace {

// hints.py's colour tags, as written there.
constexpr const char* kWhite = "\\{1A 06 FF 00 00 00}";
constexpr const char* kRed = "\\{1A 06 FF 00 00 01}";
constexpr const char* kGreen = "\\{1A 06 FF 00 00 02}";  // required
constexpr const char* kBlue = "\\{1A 06 FF 00 00 03}";   // barren zone
constexpr const char* kYellow = "\\{1A 06 FF 00 00 04}"; // possibly required
constexpr const char* kCyan = "\\{1A 06 FF 00 00 05}";   // path zone; the octo fairy's text
constexpr const char* kGray = "\\{1A 06 FF 00 00 07}";   // not required
// The text speed tag that puts instant text back after a wait command.
constexpr const char* kInstantText = "\\{1A 05 00 00 01}";

bool startsWith(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

bool endsWith(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

bool fail(std::string* err, const std::string& msg) {
    if (err != nullptr) {
        *err = msg;
    }
    return false;
}

const char* findText(Span<NameText> table, std::string_view name) {
    for (const NameText& e : table) {
        if (name == e.name) {
            return e.text;
        }
    }
    return nullptr;
}

bool inList(StrSpan list, std::string_view name) {
    for (const char* s : list) {
        if (name == s) {
            return true;
        }
    }
    return false;
}

// The colour tag of an importance (the three hints.py if/elif branches), nullptr for None.
const char* importanceColour(HintImportance i) {
    switch (i) {
    case HintImportance::Required:
        return kGreen;
    case HintImportance::PossiblyRequired:
        return kYellow;
    case HintImportance::NotRequired:
        return kGray;
    default:
        return nullptr;
    }
}

// " (<colour>required<white>)" as get_formatted_hint_text and the Savage tablet write it; "" for
// None or when importance is not shown.
std::string importanceSuffix(const Hint& h, bool importance) {
    if (!importance) {
        return std::string();
    }
    const char* colour = importanceColour(h.importance);
    if (colour == nullptr) {
        return hintImportanceName(h.importance); // formatted_importance(): ""
    }
    return std::string(" (") + colour + hintImportanceName(h.importance) + kWhite + ")";
}

const char* copulaFor(std::string_view reward) {
    return (reward == "Power Bracelets" || reward == "Iron Boots" || reward == "Bombs") ? "are" : "is";
}

// "\\{1A 07 00 00 07 %02X %02X}".
std::string waitTag(int delay) {
    char buf[40];
    snprintf(buf, sizeof(buf), "\\{1A 07 00 00 07 %02X %02X}", (unsigned)(delay >> 8), (unsigned)(delay & 0xFF));
    return buf;
}

struct Builder {
    const HintTextSource& src;
    std::vector<HintText>* out;
    std::vector<uint16_t>* skipped;
    std::string* err;

    bool boxType(uint16_t msg, uint8_t* type) {
        if (src.box_type == nullptr || !src.box_type(src.ctx, msg, type)) {
            if (skipped != nullptr) {
                skipped->push_back(msg);
            }
            return false;
        }
        return true;
    }

    // msg.string = text; msg.word_wrap_string(bfn)
    void wrapped(uint16_t msg, const std::string& text) {
        uint8_t type = 0;
        if (boxType(msg, &type)) {
            out->push_back({msg, wordWrapMessage(*src.widths, type, text)});
        }
    }

    // msg.construct_string_from_parts(bfn, parts)
    void parts(uint16_t msg, const std::vector<std::string>& lines) {
        uint8_t type = 0;
        if (boxType(msg, &type)) {
            out->push_back({msg, constructStringFromParts(*src.widths, type, lines)});
        }
    }

    // update_savage_labyrinth_hint_tablet.
    bool savage(const Hint& f30, const Hint& f50) {
        const std::string imp30 = importanceSuffix(f30, src.importance);
        const std::string imp50 = importanceSuffix(f50, src.importance);
        std::string r30, r50;
        if (src.floor30_valid && !formattedReward(f30, src.cryptic, &r30, err)) {
            return false;
        }
        if (src.floor50_valid && !formattedReward(f50, src.cryptic, &r50, err)) {
            return false;
        }
        std::string hint;
        if (src.floor30_valid && src.floor50_valid) {
            hint = kRed + r30 + kWhite + imp30 + " and " + kRed + r50 + kWhite + imp50 + " await";
        } else if (src.floor30_valid) {
            hint = kRed + r30 + kWhite + imp30 + " and challenge await";
        } else if (src.floor50_valid) {
            hint = std::string("challenge and ") + kRed + r50 + kWhite + imp50 + " await";
        } else {
            hint = "challenge awaits";
        }
        std::string s = "\\{1A 07 FF 00 01 00 96}\\{1A 06 FF 00 00 01}The Savage Labyrinth\n\\{1A 07 FF 00 01 00 64}\n\n\n";
        s += std::string(kWhite) + "Deep in the never-ending darkness, the way to " + hint + ".";
        wrapped(kMsgSavageTablet, s);
        return true;
    }

    // update_big_octo_great_fairy_item_name_hint.
    bool octoFairy(const Hint& h) {
        if (h.importance == HintImportance::None) {
            return fail(err, "the Big Octo Great Fairy hint has no importance"); // its assert
        }
        std::string place, reward;
        if (!formattedPlace(h, src.cryptic, &place, err) || !formattedReward(h, src.cryptic, &reward, err)) {
            return false;
        }
        wrapped(kMsgOctoFairyPlace,
                std::string(kCyan) + "In " + kRed + place + kCyan + ", you will find an item.");

        reward = upperFirstLetter(reward);
        std::string s;
        if (src.importance) {
            const std::string imp = importanceColour(h.importance) + std::string(hintImportanceName(h.importance)) + kCyan;
            s = std::string(kCyan) + "..." + kRed + reward + kCyan + ", which " + copulaFor(h.reward) + " " + imp +
                " in your quest.";
        } else {
            s = std::string(kCyan) + "..." + kRed + reward + kCyan + ", which may help you on your quest.";
        }
        wrapped(kMsgOctoFairyItem, s);

        if (src.importance && h.importance == HintImportance::NotRequired) {
            wrapped(kMsgOctoFairyAfter,
                    std::string(kCyan) + "Though if you still desire such an item, you must journey to that place.");
        } else {
            wrapped(kMsgOctoFairyAfter,
                    std::string(kCyan) + "When you find you have need of such an item, you must journey to that place.");
        }
        return true;
    }

    // update_fishmen_hints.
    bool fishmen(const std::vector<Hint>& byIsland) {
        for (int island = 1; island < (int)byIsland.size() && island <= 49; island++) {
            const Hint& h = byIsland[island];
            std::vector<std::string> lines(1);
            if (!formattedHintText(h, src.cryptic, src.importance, &lines[0], err, "I've heard from my sources that ",
                                   ".", 60)) {
                return false;
            }
            if (src.cryptic && (h.type == HintType::Item || h.type == HintType::Location)) {
                lines.push_back("Could be worth a try checking that place out. If you know where it is, of course.");
                if (src.instant_text) {
                    lines.back() = kInstantText + lines.back();
                }
            }
            parts((uint16_t)(kMsgFishmanBase + island), lines);
        }
        return true;
    }

    // update_hoho_hints.
    bool hoho(const std::vector<std::vector<Hint>>& byIndex) {
        for (int index = 0; index < (int)byIndex.size() && index < kHohoCount; index++) {
            const std::vector<Hint>& hints = byIndex[index];
            std::vector<std::string> lines;
            for (size_t i = 0; i < hints.size(); i++) {
                const char* prefix = i == 0 ? "\\{1A 05 01 01 03}Ho ho! To think that " : "and that ";
                const char* suffix = i == hints.size() - 1 ? "..." : ",";
                lines.emplace_back();
                if (!formattedHintText(hints[i], src.cryptic, src.importance, &lines.back(), err, prefix, suffix)) {
                    return false;
                }
                if (src.instant_text && i > 0) {
                    lines.back() = kInstantText + lines.back();
                }
            }
            parts((uint16_t)(kMsgHohoBase + index), lines);
        }
        return true;
    }

    // update_korl_hints.
    bool korl(const std::vector<Hint>& hints) {
        if (hints.empty()) {
            return fail(err, "update_korl_hints: no hint for KoRL (wwrando's assert)");
        }
        std::vector<std::string> lines;
        for (size_t i = 0; i < hints.size(); i++) {
            const char* prefix = i == 0 ? "They say that " : "and that ";
            const char* suffix = i == hints.size() - 1 ? "." : ",";
            lines.emplace_back();
            if (!formattedHintText(hints[i], src.cryptic, src.importance, &lines.back(), err, prefix, suffix, 0)) {
                return false;
            }
        }
        for (uint16_t msg : kMsgKorlHints) {
            parts(msg, lines);
        }
        return true;
    }
};

} // namespace

std::string hintItemName(std::string_view item) {
    if (startsWith(item, "Triforce Chart")) {
        return "Triforce Chart";
    }
    if (startsWith(item, "Triforce Shard")) {
        return "Triforce Shard";
    }
    if (startsWith(item, "Treasure Chart")) {
        return "Treasure Chart";
    }
    if (endsWith(item, "Tingle Statue")) {
        return "Tingle Statue";
    }
    if (endsWith(item, "Small Key")) {
        return "Small Key";
    }
    if (endsWith(item, "Big Key")) {
        return "Big Key";
    }
    return std::string(item);
}

bool formattedItemName(std::string_view item, std::string* out, std::string* err) {
    static const char* const kKinds[4][2] = {
        {" Small Key", "%s small key"},
        {" Big Key", "%s Big Key"},
        {" Dungeon Map", "%s Dungeon Map"},
        {" Compass", "%s Compass"},
    };
    std::string name(item);
    for (const auto& kind : kKinds) {
        const std::string_view suffix = kind[0] + 1; // endswith("Small Key"), without the space
        if (!endsWith(item, suffix)) {
            continue;
        }
        // item_name.split(" Small Key")[0]: everything before the first " Small Key".
        const size_t cut = item.find(kind[0]);
        const std::string shortName(item.substr(0, cut == std::string_view::npos ? item.size() : cut));
        const char* dungeon = nullptr;
        for (const DungeonName& d : dungeon_names()) {
            if (shortName == d.short_name) {
                dungeon = d.name;
            }
        }
        if (dungeon == nullptr) {
            return fail(err, "no dungeon named '" + shortName + "' (item '" + name + "')");
        }
        name = std::string(dungeon) + (kind[1] + 2); // "%s small key" % dungeon_name
        break;
    }
    *out = addArticleBeforeItemName(name);
    return true;
}

bool crypticItemHint(std::string_view item, std::string* out, std::string* err) {
    const std::string key = hintItemName(item);
    const char* text = findText(progress_item_hints(), key);
    if (text == nullptr) {
        return fail(err, "no cryptic hint text for item '" + key + "'");
    }
    *out = text;
    return true;
}

bool formattedPlace(const Hint& h, bool cryptic, std::string* out, std::string* err) {
    if (!cryptic) {
        *out = h.place;
        return true;
    }
    switch (h.type) {
    case HintType::Path:
    case HintType::Barren:
    case HintType::Item: {
        const char* text = findText(zone_name_hints(), h.place);
        if (text == nullptr) {
            return fail(err, "no cryptic hint text for zone '" + h.place + "'");
        }
        *out = text;
        return true;
    }
    case HintType::Location:
        for (const LocationHint& e : location_hints()) {
            if (h.place == e.location) {
                *out = e.text;
                return true;
            }
        }
        return fail(err, "no hint text for location '" + h.place + "'");
    default:
        return fail(err, "a fixed-location hint has no cryptic place");
    }
}

bool formattedReward(const Hint& h, bool cryptic, std::string* out, std::string* err) {
    switch (h.type) {
    case HintType::Path:
        *out = h.reward;
        return true;
    case HintType::Barren:
        *out = "None"; // reward is None
        return true;
    case HintType::Item:
    case HintType::FixedLocation:
        return cryptic ? crypticItemHint(h.reward, out, err) : formattedItemName(h.reward, out, err);
    case HintType::Location:
        return formattedItemName(h.reward, out, err);
    }
    return fail(err, "unknown hint type");
}

bool formattedHintText(const Hint& h, bool cryptic, bool importance, std::string* out, std::string* err,
                       std::string_view prefix, std::string_view suffix, int delay) {
    std::string place;
    if (!formattedPlace(h, cryptic, &place, err)) {
        return false;
    }
    if (place == "Mailbox") {
        place = "the mail";
    } else if (place == "Tower of the Gods Sector") {
        place = "the Tower of the Gods sector";
    } else if (place == "Forsaken Fortress Sector") {
        place = "the Forsaken Fortress sector";
    }
    std::string reward;
    if (!formattedReward(h, cryptic, &reward, err)) {
        return false;
    }
    const std::string imp = importanceSuffix(h, importance);
    const std::string pre(prefix), suf(suffix);

    std::string s;
    switch (h.type) {
    case HintType::Path: {
        const char* prep = "at";
        if (place == "the mail" || place == "the Tower of the Gods sector" || place == "the Forsaken Fortress sector") {
            prep = "in";
        }
        s = pre + "an item found " + prep + " " + kCyan + place + kWhite + " is on the path to " + kRed + reward +
            kWhite + suf;
        break;
    }
    case HintType::Barren: {
        const char* verb = place == "the mail" ? "checking" : "visiting";
        if (place == "a location on the open seas") {
            place = "locations on the open seas";
        }
        s = pre + verb + " " + kBlue + place + kWhite + " is a foolish choice" + suf;
        break;
    }
    case HintType::Location:
        s = pre + kRed + place + kWhite + " rewards " + kRed + reward + kWhite + imp + suf;
        break;
    case HintType::Item:
        s = pre + kRed + reward + kWhite + imp + " " + copulaFor(reward) + " located in " + kRed + place + kWhite + suf;
        break;
    default:
        break; // hint_string = ""
    }
    delay = delay < 0 ? 0 : delay > 0xFFFF ? 0xFFFF : delay;
    if (delay > 0) {
        s += waitTag(delay);
    }
    *out = s;
    return true;
}

bool savageHintValid(const Hint& h, bool keylunacy) {
    if (h.importance == HintImportance::None) {
        return false; // not a progress item
    }
    const bool dungeon = inList(dungeon_progress_items(), h.reward) || inList(dungeon_nonprogress_items(), h.reward);
    if (dungeon && !keylunacy) {
        return false;
    }
    return !endsWith(h.reward, " Trap Chest");
}

bool buildHintTexts(const HintsResult& h, const HintTextSource& src, std::vector<HintText>* out,
                    std::vector<uint16_t>* skipped, std::string* err) {
    if (src.widths == nullptr) {
        if (skipped != nullptr) {
            for (uint16_t n : hintTextMessageNumbers()) {
                skipped->push_back(n);
            }
        }
        return true;
    }
    Builder b{src, out, skipped, err};
    if (!b.savage(h.floor30, h.floor50)) {
        return false;
    }
    if (!h.hasOctoFairy) {
        return true; // no hintable progress item: the rest stays vanilla
    }
    if (!b.octoFairy(h.octoFairy)) {
        return false;
    }
    for (const auto& [placement, hints] : h.placements) {
        bool ok = true;
        switch (placement) {
        case HintPlacement::Fishmen:
            ok = b.fishmen(h.fishmen);
            break;
        case HintPlacement::Hoho:
            ok = b.hoho(h.hoho);
            break;
        case HintPlacement::Korl:
            ok = b.korl(hints);
            break;
        }
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::vector<uint16_t> hintTextMessageNumbers() {
    std::vector<uint16_t> v = {kMsgSavageTablet, kMsgOctoFairyPlace, kMsgOctoFairyItem, kMsgOctoFairyAfter};
    for (int island = 1; island <= 49; island++) {
        v.push_back((uint16_t)(kMsgFishmanBase + island));
    }
    for (int i = 0; i < kHohoCount; i++) {
        v.push_back((uint16_t)(kMsgHohoBase + i));
    }
    for (uint16_t n : kMsgKorlHints) {
        v.push_back(n);
    }
    return v;
}

int applyHintTexts(const std::vector<HintText>& texts) {
    int ok = 0;
    for (const HintText& t : texts) {
        const int rc = pc_rando_msg_set_text(t.msg_no, t.text.c_str());
        if (rc == PCR_MSG_OK) {
            ok++;
        } else {
            fprintf(stderr, "[cos] rando: hint text: message %u refused (%d)\n", (unsigned)t.msg_no, rc);
        }
    }
    return ok;
}

} // namespace pcr
