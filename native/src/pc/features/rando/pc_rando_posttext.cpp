// The randomizer's seed-dependent message texts (docs/RANDOMIZER_PLAN.md step R3.21). See
// pc_rando_posttext.h.
#include "pc_rando_posttext.h"

#include "pc_rando_data.h"
#include "pc_rando_msg.h"
#include "pc/pc_rando_locs.h"

#include <stdio.h>
#include <string.h>

namespace pcr {

namespace {

// tweaks.py's colour tags, as written there.
constexpr const char* kRed = "\\{1A 06 FF 00 00 01}";
constexpr const char* kWhite = "\\{1A 06 FF 00 00 00}";

constexpr uint16_t kMsgSwiftSail = 463;
constexpr uint16_t kMsgKorl = 3443;
constexpr uint16_t kMsgBeedleSpoils = 3957;
constexpr uint16_t kMsgShop20 = 3906, kMsgShop20Buy = 3909;
constexpr uint16_t kMsgShop500 = 12106, kMsgShop950 = 12107, kMsgShop900 = 12108;
constexpr uint16_t kMsgShop500Buy = 12109, kMsgShop950Buy = 12110, kMsgShop900Buy = 12111;
constexpr uint16_t kMsgAuction40 = 7440, kMsgAuction5 = 7441, kMsgAuction60 = 7442, kMsgAuction80 = 7443;
constexpr uint16_t kMsgSquid1 = 7520, kMsgSquid2 = 7521;
constexpr uint16_t kMsgLetter = 3325;

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        const size_t e = s.find('\n', pos);
        if (e == std::string::npos) {
            out.push_back(s.substr(pos));
            return out;
        }
        out.push_back(s.substr(pos, e - pos));
        pos = e + 1;
    }
}

// "\n".join(lines[from:to]) with Python's slice clamping.
std::string joinLines(const std::vector<std::string>& lines, size_t from, size_t to) {
    std::string out;
    to = to < lines.size() ? to : lines.size();
    for (size_t i = from; i < to; i++) {
        if (i > from) {
            out += '\n';
        }
        out += lines[i];
    }
    return out;
}

// str.replace(old, new): every non-overlapping occurrence, left to right.
std::string replaceAll(const std::string& s, const std::string& from, const std::string& to) {
    std::string out;
    size_t pos = 0;
    while (true) {
        const size_t e = s.find(from, pos);
        if (e == std::string::npos) {
            out.append(s, pos, std::string::npos);
            return out;
        }
        out.append(s, pos, e - pos);
        out += to;
        pos = e + from.size();
    }
}

bool startsWith(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

struct Builder {
    const PostTextSource& src;
    std::vector<PostText>* out;
    std::vector<uint16_t>* skipped;

    void put(uint16_t msg_no, std::string text) { out->push_back(PostText{msg_no, std::move(text)}); }
    void skip(uint16_t msg_no) {
        if (skipped != nullptr) {
            skipped->push_back(msg_no);
        }
    }
    std::string item(int loc) const { return src.item_name != nullptr ? src.item_name(src.ctx, loc) : std::string(); }
    bool vanilla(uint16_t msg_no, std::string* text, uint8_t* type) const {
        return src.vanilla != nullptr && src.vanilla(src.ctx, msg_no, text, type);
    }

    // make_sail_behave_like_swift_sail: the pause menu name of the sail.
    void swiftSail() {
        if (src.swift_sail) {
            put(kMsgSwiftSail, "Swift Sail");
        }
    }

    // update_korl_dialogue.
    void korl() {
        std::string s = "\\{1A 05 00 00 00}, the sea is all yours.\n";
        s += "Make sure you explore every corner\n";
        s += "in search of items to help you. Remember\n";
        s += "that your quest is to defeat Ganondorf.";
        put(kMsgKorl, std::move(s));
    }

    // add_chart_number_to_item_get_messages.
    void charts() {
        for (const ItemName& it : item_names()) {
            const char* tag;
            if (startsWith(it.name, "Treasure Chart ")) {
                tag = "Treasure Chart";
            } else if (startsWith(it.name, "Triforce Chart ") && strstr(it.name, "deciphered") == nullptr) {
                tag = "Triforce Chart";
            } else {
                continue;
            }
            const uint16_t msg_no = (uint16_t)(101 + it.id);
            std::string text;
            uint8_t type = 0;
            if (!vanilla(msg_no, &text, &type)) {
                skip(msg_no);
                continue;
            }
            const std::string from = std::string("a ") + kRed + tag;
            const std::string to = std::string(kRed) + it.name;
            std::string now = replaceAll(text, from, to);
            if (now != text) {
                put(msg_no, std::move(now));
            }
        }
    }

    // update_beedle_spoil_selling_text: line 3 of the refusal.
    void beedleSpoils() {
        std::string text;
        uint8_t type = 0;
        if (!vanilla(kMsgBeedleSpoils, &text, &type)) {
            skip(kMsgBeedleSpoils);
            return;
        }
        std::vector<std::string> lines = splitLines(text);
        if (lines.size() < 3) { // Python would raise; leave the vanilla text
            skip(kMsgBeedleSpoils);
            return;
        }
        lines[2] = "And no Blue Chu Jelly, either!";
        put(kMsgBeedleSpoils, joinLines(lines, 0, lines.size()));
    }

    // update_shop_item_descriptions.
    void shop() {
        char buf[1024];
        std::string name = item(PCR_LOC_THE_GREAT_SEA_BEEDLES_SHOP_SHIP_20_RUPEE_ITEM);
        if (name.empty()) {
            skip(kMsgShop20);
            skip(kMsgShop20Buy);
        } else {
            const char* n = name.c_str();
            snprintf(buf, sizeof buf, "%s%s  %d Rupees%s", kRed, n, 20, kWhite);
            put(kMsgShop20, buf);
            snprintf(buf, sizeof buf, "%s   %d Rupees\nWill you buy it?\n\\{1A 05 00 00 08}I'll buy it\nNo thanks", n, 20);
            put(kMsgShop20Buy, buf);
        }

        name = item(PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM);
        if (name.empty()) {
            skip(kMsgShop500);
            skip(kMsgShop500Buy);
        } else {
            const char* n = name.c_str();
            snprintf(buf, sizeof buf, "%s%s  %d Rupees\n%sThis is my last one.", kRed, n, 500, kWhite);
            put(kMsgShop500, buf);
            snprintf(buf, sizeof buf,
                     "This %s%s%s is a mere %s%d Rupees%s!\nBuy it! Buy it! Buy buy buy!\n\\{1A 05 00 00 08}I'll buy it\nNo thanks",
                     kRed, n, kWhite, kRed, 500, kWhite);
            put(kMsgShop500Buy, buf);
        }

        name = item(PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM);
        if (name.empty()) {
            skip(kMsgShop950);
            skip(kMsgShop950Buy);
        } else {
            const char* n = name.c_str();
            snprintf(buf, sizeof buf, "%s%s  %d Rupees\n%sThis is my last one of these, too.", kRed, n, 950, kWhite);
            put(kMsgShop950, buf);
            snprintf(buf, sizeof buf,
                     "This %s%s%s is only %s%d Rupees%s!\nBuy it! Buy it! Buy buy buy!\n\\{1A 05 00 00 08}I'll buy it\nNo thanks",
                     kRed, n, kWhite, kRed, 950, kWhite);
            put(kMsgShop950Buy, buf);
        }

        name = item(PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM);
        if (name.empty()) {
            skip(kMsgShop900);
            skip(kMsgShop900Buy);
        } else {
            const char* n = name.c_str();
            snprintf(buf, sizeof buf,
                     "%s%s  %d Rupees\n%sThe price may be high, but it'll pay\noff handsomely in the end!", kRed, n, 900,
                     kWhite);
            put(kMsgShop900, buf);
            // (the space before the first colour change is wwrando's)
            snprintf(buf, sizeof buf,
                     "This %s%s %sis just %s%d Rupees!%s\nBuy it! Buy it! Buy buy buy!\n\\{1A 05 00 00 08}I'll buy it\nNo thanks",
                     kRed, n, kWhite, kRed, 900, kWhite);
            put(kMsgShop900Buy, buf);
        }
    }

    // update_auction_item_names (wwrando's order: 7441, 7440, 7442, 7443).
    void auction() {
        const struct {
            int loc;
            uint16_t msg_no;
        } kAuctions[] = {
            {PCR_LOC_WINDFALL_ISLAND_5_RUPEE_AUCTION, kMsgAuction5},
            {PCR_LOC_WINDFALL_ISLAND_40_RUPEE_AUCTION, kMsgAuction40},
            {PCR_LOC_WINDFALL_ISLAND_60_RUPEE_AUCTION, kMsgAuction60},
            {PCR_LOC_WINDFALL_ISLAND_80_RUPEE_AUCTION, kMsgAuction80},
        };
        for (const auto& a : kAuctions) {
            const std::string name = item(a.loc);
            if (name.empty()) {
                skip(a.msg_no);
                continue;
            }
            put(a.msg_no, kRed + name);
        }
    }

    // msg.word_wrap_string(self.bfn): wrapped at the message's own text box width.
    void putWrapped(uint16_t msg_no, const std::string& text) {
        std::string unused;
        uint8_t type = 0; // DIALOG when the BMG cannot tell (both messages are dialog boxes)
        (void)vanilla(msg_no, &unused, &type);
        put(msg_no, wordWrapMessage(*src.widths, type, text));
    }

    // update_battlesquid_item_names (the 20-shots prize names no item in its text).
    void battlesquid() {
        std::string name = item(PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_FIRST_PRIZE);
        if (name.empty() || src.widths == nullptr) {
            skip(kMsgSquid1);
        } else {
            putWrapped(kMsgSquid1, std::string("\\{1A 05 01 00 8E}Hoorayyy! Yayyy! Yayyy!\nOh, thank you, Mr. Sailor!\n\n\n") +
                                       "Please take this " + kRed + name + kWhite +
                                       " as a sign of our gratitude. You are soooooo GREAT!");
        }
        name = item(PCR_LOC_WINDFALL_ISLAND_BATTLESQUID_SECOND_PRIZE);
        if (name.empty() || src.widths == nullptr) {
            skip(kMsgSquid2);
        } else {
            putWrapped(kMsgSquid2,
                       std::string("\\{1A 05 01 00 8E}Hoorayyy! Yayyy! Yayyy!\nOh, thank you so much, Mr. Sailor!\n\n\n") +
                           "This is our thanks to you! It's been passed down on our island for many years, so don't "
                           "tell the island elder, OK? " +
                           "Here..." + kRed + "\\{1A 05 00 00 39} " + kWhite + "Please accept this " + kRed + name +
                           kWhite + "!");
        }
    }

    // update_item_names_in_letter_advertising_rock_spire_shop.
    void letter() {
        const std::string a = item(PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_500_RUPEE_ITEM);
        const std::string b = item(PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_950_RUPEE_ITEM);
        const std::string c = item(PCR_LOC_ROCK_SPIRE_ISLE_BEEDLES_SPECIAL_SHOP_SHIP_900_RUPEE_ITEM);
        std::string text;
        uint8_t type = 0;
        if (a.empty() || b.empty() || c.empty() || src.widths == nullptr || !vanilla(kMsgLetter, &text, &type)) {
            skip(kMsgLetter);
            return;
        }
        put(kMsgLetter, rockSpireLetter(*src.widths, type, text, a, b, c));
    }
};

} // namespace

std::string rockSpireLetter(const GlyphWidths& w, uint8_t text_box_type, const std::string& vanilla,
                            const std::string& item1, const std::string& item2, const std::string& item3) {
    const std::vector<std::string> lines = splitLines(vanilla);
    const std::string before = joinLines(lines, 0, 8) + "\n";
    const std::string after = lines.size() > 12 ? joinLines(lines, 12, lines.size()) : std::string();

    std::string hint = std::string("Do you have need of ") + indefiniteArticle(item1) + " " + kRed + item1 + kWhite + ", ";
    hint += std::string(indefiniteArticle(item2)) + " " + kRed + item2 + kWhite + ", ";
    hint += std::string("or ") + indefiniteArticle(item3) + " " + kRed + item3 + kWhite + "? ";
    hint += "We have them at special bargain prices.";

    // Letters have 2 spaces at the start of each line: wrap 2 spaces narrower, then indent.
    const int space = glyphWidth(w, ' ');
    hint = wordWrapMessage(w, text_box_type, hint, -2 * space);
    hint = padStringToNext4Lines(hint);
    std::string indented;
    const std::vector<std::string> hintLines = splitLines(hint);
    for (size_t i = 0; i < hintLines.size(); i++) {
        if (i > 0) {
            indented += '\n';
        }
        if (!hintLines[i].empty()) {
            indented += "  ";
        }
        indented += hintLines[i];
    }
    return before + indented + after;
}

void buildPostTexts(const PostTextSource& src, std::vector<PostText>* out, std::vector<uint16_t>* skipped) {
    Builder b{src, out, skipped};
    // apply_necessary_tweaks (randomizer.py order: swift sail, KoRL, charts, Beedle)
    b.swiftSail();
    b.korl();
    b.charts();
    b.beedleSpoils();
    // apply_necessary_post_randomization_tweaks
    if (src.post_randomization) {
        b.shop();
        b.auction();
        b.battlesquid();
        b.letter();
    }
}

std::vector<uint16_t> chartMessageNumbers() {
    std::vector<uint16_t> out;
    for (const ItemName& it : item_names()) {
        if (startsWith(it.name, "Treasure Chart ") ||
            (startsWith(it.name, "Triforce Chart ") && strstr(it.name, "deciphered") == nullptr)) {
            out.push_back((uint16_t)(101 + it.id));
        }
    }
    return out;
}

std::vector<uint16_t> postTextMessageNumbers() {
    std::vector<uint16_t> out = {kMsgSwiftSail, kMsgKorl, kMsgBeedleSpoils, kMsgShop20, kMsgShop20Buy,
                                 kMsgShop500, kMsgShop500Buy, kMsgShop950, kMsgShop950Buy, kMsgShop900,
                                 kMsgShop900Buy, kMsgAuction40, kMsgAuction5, kMsgAuction60, kMsgAuction80,
                                 kMsgSquid1, kMsgSquid2, kMsgLetter};
    for (uint16_t n : chartMessageNumbers()) {
        out.push_back(n);
    }
    return out;
}

int applyPostTexts(const std::vector<PostText>& texts) {
    int ok = 0;
    for (const PostText& t : texts) {
        const int rc = pc_rando_msg_set_text(t.msg_no, t.text.c_str());
        if (rc == PCR_MSG_OK) {
            ok++;
        } else {
            fprintf(stderr, "[cos] rando: post text: message %u refused (%d)\n", (unsigned)t.msg_no, rc);
        }
    }
    return ok;
}

} // namespace pcr
