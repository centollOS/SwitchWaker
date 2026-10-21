// R5.2 (docs/RANDOMIZER_PLAN.md): the hint texts (pc_rando_hint_text.h).
//  1. pcr::buildHintTexts, for every run of R5_2_hint_text.txt (fixtures/R5_2_make_fixture.py:
//     wwrando's own HintsRandomizer text functions after real dry runs of the R5.1 seeds, on a
//     made-up BMG and font), writes exactly the messages wwrando writes, byte for byte: plain and
//     cryptic, with and without importance and instant text, every hint type and placement, the
//     four Savage Labyrinth validity combinations;
//  2. savageHintValid equals wwrando's check_item_can_be_hinted_at for the floors of those seeds;
//  3. the helpers on their own (item names, cryptic texts, errors where Python raises);
//  4. a missing font or box type skips and reports; KoRL on with no hint fails as wwrando's assert;
//  5. applyHintTexts puts the texts in the overlay; hintTextMessageNumbers covers every message.
#include "rando_test.h"

#include "pc_rando_hint_text.h"
#include "pc_rando_json.h"
#include "pc_rando_msg.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

std::string unhex(const std::string& h) {
    std::string out;
    if (h == "-") {
        return out;
    }
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char)std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (true) {
        const size_t e = s.find(sep, pos);
        out.push_back(s.substr(pos, e == std::string::npos ? std::string::npos : e - pos));
        if (e == std::string::npos) {
            break;
        }
        pos = e + 1;
    }
    return out;
}

struct Run {
    std::string name;
    bool cryptic, importance, instant, keylunacy, v30, v50, forced;
    pcr::HintsResult hints;
    std::map<uint16_t, int> expected; // msg -> text index
};

struct Fixture {
    pcr::GlyphWidths widths{};
    bool haveWidths = false;
    std::map<uint16_t, uint8_t> boxTypes;
    std::set<uint16_t> hidden; // box_type() fails for these
    std::map<int, std::string> texts;
    std::vector<Run> runs;
};

bool boxType(void* ctx, uint16_t msg, uint8_t* type) {
    const Fixture* f = (const Fixture*)ctx;
    auto it = f->boxTypes.find(msg);
    if (it == f->boxTypes.end() || f->hidden.count(msg) != 0) {
        return false;
    }
    *type = it->second;
    return true;
}

bool load(Fixture* fx) {
    std::string text;
    if (!RT_CHECK(rt::readFixture("R5_2_hint_text.txt", text))) {
        return false;
    }
    for (const std::string& line : split(text, '\n')) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> f = split(line, ' ');
        if (f[0] == "F" && RT_CHECK_EQ(f.size(), (size_t)258)) {
            for (int c = 0; c < 256; c++) {
                fx->widths.code[c] = (uint8_t)std::stoi(f[1 + c]);
            }
            fx->widths.replacement = (uint8_t)std::stoi(f[257]);
            fx->haveWidths = true;
        } else if (f[0] == "B" && RT_CHECK_EQ(f.size(), (size_t)3)) {
            fx->boxTypes[(uint16_t)std::stoi(f[1])] = (uint8_t)std::stoi(f[2]);
        } else if (f[0] == "T" && RT_CHECK_EQ(f.size(), (size_t)3)) {
            fx->texts[std::stoi(f[1])] = unhex(f[2]);
        } else if (f[0] == "K" && RT_CHECK_EQ(f.size(), (size_t)10)) {
            Run r;
            r.name = f[1];
            r.cryptic = f[2] == "1";
            r.importance = f[3] == "1";
            r.instant = f[4] == "1";
            r.keylunacy = f[5] == "1";
            r.v30 = f[6] == "1";
            r.v50 = f[7] == "1";
            r.forced = f[8] == "1";
            pcr::Json j;
            std::string err;
            if (!RT_CHECK(pcr::jsonParse(unhex(f[9]), &j, &err)) || !RT_CHECK(pcr::hintsFromJson(j, r.hints, &err))) {
                std::fprintf(stderr, "  %s: %s\n", r.name.c_str(), err.c_str());
                return false;
            }
            fx->runs.push_back(std::move(r));
        } else if (f[0] == "S" && RT_CHECK_EQ(f.size(), (size_t)3) && RT_CHECK(!fx->runs.empty())) {
            fx->runs.back().expected[(uint16_t)std::stoi(f[1])] = std::stoi(f[2]);
        }
    }
    return RT_CHECK(fx->haveWidths) && RT_CHECK_EQ(fx->boxTypes.size(), (size_t)70) && RT_CHECK(fx->runs.size() >= 30);
}

pcr::HintTextSource source(Fixture* fx, const Run& r) {
    pcr::HintTextSource src;
    src.widths = &fx->widths;
    src.cryptic = r.cryptic;
    src.importance = r.importance;
    src.instant_text = r.instant;
    src.floor30_valid = r.v30;
    src.floor50_valid = r.v50;
    src.box_type = boxType;
    src.ctx = fx;
    return src;
}

std::map<uint16_t, std::string> byMsg(const std::vector<pcr::HintText>& v) {
    std::map<uint16_t, std::string> m;
    for (const pcr::HintText& t : v) {
        RT_CHECK(m.count(t.msg_no) == 0); // each message once
        m[t.msg_no] = t.text;
    }
    return m;
}

// 1 and 2.
void checkRuns(Fixture& fx) {
    int compared = 0, validity = 0;
    std::set<int> typesSeen;
    for (const Run& r : fx.runs) {
        std::vector<pcr::HintText> out;
        std::vector<uint16_t> skipped;
        std::string err;
        if (!RT_CHECK(pcr::buildHintTexts(r.hints, source(&fx, r), &out, &skipped, &err))) {
            std::fprintf(stderr, "  %s: %s\n", r.name.c_str(), err.c_str());
            continue;
        }
        RT_CHECK(skipped.empty());
        const std::map<uint16_t, std::string> got = byMsg(out);
        RT_CHECK_EQ(got.size(), r.expected.size());
        int bad = 0;
        for (const auto& [msg, idx] : r.expected) {
            auto it = got.find(msg);
            if (!RT_CHECK(it != got.end())) {
                std::fprintf(stderr, "  %s: message %u not built\n", r.name.c_str(), (unsigned)msg);
                continue;
            }
            if (!RT_CHECK_EQ(it->second, fx.texts[idx]) && ++bad <= 3) {
                std::fprintf(stderr, "  %s (cryptic %d importance %d instant %d): message %u\n", r.name.c_str(),
                             r.cryptic, r.importance, r.instant, (unsigned)msg);
            }
            compared++;
        }
        if (!out.empty()) {
            RT_CHECK_EQ(out.front().msg_no, pcr::kMsgSavageTablet); // _save's order
        }
        if (!r.forced) {
            RT_CHECK_EQ(pcr::savageHintValid(r.hints.floor30, r.keylunacy), r.v30);
            RT_CHECK_EQ(pcr::savageHintValid(r.hints.floor50, r.keylunacy), r.v50);
            validity += 2;
        }
        for (const auto& [p, hints] : r.hints.placements) {
            for (const pcr::Hint& h : hints) {
                typesSeen.insert((int)h.type);
            }
        }
    }
    // Every placed hint type (path, barren, item, location) is covered.
    RT_CHECK_EQ(typesSeen.size(), (size_t)4);
    std::printf("compared %d messages over %zu runs; %d Savage Labyrinth validity checks\n", compared,
                fx.runs.size(), validity);
}

// 3.
void checkHelpers() {
    RT_CHECK_EQ(pcr::hintItemName("Triforce Shard 3"), std::string("Triforce Shard"));
    RT_CHECK_EQ(pcr::hintItemName("Treasure Chart 17"), std::string("Treasure Chart"));
    RT_CHECK_EQ(pcr::hintItemName("Dragon Tingle Statue"), std::string("Tingle Statue"));
    RT_CHECK_EQ(pcr::hintItemName("DRC Small Key"), std::string("Small Key"));
    RT_CHECK_EQ(pcr::hintItemName("FW Dungeon Map"), std::string("FW Dungeon Map"));
    const char* const names[][2] = {
        {"DRC Small Key", "a Dragon Roost Cavern small key"},
        {"WT Big Key", "the Wind Temple Big Key"},
        {"FW Dungeon Map", "the Forbidden Woods Dungeon Map"},
        {"ET Compass", "the Earth Temple Compass"},
        {"Progressive Sword", "a Progressive Sword"},
        {"Bombs", "Bombs"},
        {"Triforce Shard 3", "Triforce Shard 3"},
        {"Dragon Tingle Statue", "the Dragon Tingle Statue"},
        {"Hurricane Spin", "the Hurricane Spin"},
        {"Joy Pendant", "a Joy Pendant"},
    };
    for (const auto& n : names) {
        std::string s;
        RT_CHECK(pcr::formattedItemName(n[0], &s));
        RT_CHECK_EQ(s, std::string(n[1]));
    }
    std::string s, err;
    RT_CHECK(!pcr::formattedItemName("XYZ Small Key", &s, &err) && !err.empty());
    RT_CHECK(pcr::crypticItemHint("DRC Small Key", &s));
    RT_CHECK_EQ(s, std::string("a key"));
    RT_CHECK(pcr::crypticItemHint("Treasure Chart 17", &s));
    RT_CHECK_EQ(s, std::string("a blue map"));
    err.clear();
    RT_CHECK(!pcr::crypticItemHint("Joy Pendant", &s, &err) && !err.empty()); // Python's KeyError

    pcr::Hint fixed{pcr::HintType::FixedLocation, "Outset Island - Savage Labyrinth - Floor 30", "Joy Pendant",
                    pcr::HintImportance::None};
    RT_CHECK(!pcr::formattedPlace(fixed, true, &s)); // NotImplementedError
    RT_CHECK(pcr::formattedPlace(fixed, false, &s) && s == fixed.place);
    RT_CHECK(!pcr::savageHintValid(fixed, false)); // not a progress item
    pcr::Hint key{pcr::HintType::FixedLocation, fixed.place, "DRC Small Key", pcr::HintImportance::Required};
    RT_CHECK(!pcr::savageHintValid(key, false));
    RT_CHECK(pcr::savageHintValid(key, true));
    pcr::Hint trap{pcr::HintType::FixedLocation, fixed.place, "Rupoor Trap Chest", pcr::HintImportance::NotRequired};
    RT_CHECK(!pcr::savageHintValid(trap, true));

    // get_formatted_hint_text's special places and the wait tag.
    pcr::Hint mail{pcr::HintType::Barren, "Mailbox", "", pcr::HintImportance::None};
    RT_CHECK(pcr::formattedHintText(mail, false, false, &s));
    RT_CHECK_EQ(s, std::string("They say that checking \\{1A 06 FF 00 00 03}the mail\\{1A 06 FF 00 00 00} is a foolish "
                               "choice.\\{1A 07 00 00 07 00 1E}"));
    pcr::Hint path{pcr::HintType::Path, "Tower of the Gods Sector", "Gohdan", pcr::HintImportance::None};
    RT_CHECK(pcr::formattedHintText(path, false, true, &s, nullptr, "P ", "!", 0x1234));
    RT_CHECK_EQ(s, std::string("P an item found in \\{1A 06 FF 00 00 05}the Tower of the Gods sector\\{1A 06 FF 00 00 00}"
                               " is on the path to \\{1A 06 FF 00 00 01}Gohdan\\{1A 06 FF 00 00 00}!"
                               "\\{1A 07 00 00 07 12 34}"));
}

// 4.
void checkSkips(Fixture& fx) {
    const Run* rich = nullptr; // a run with octo fairy, fishmen, Ho Ho and KoRL texts
    for (const Run& r : fx.runs) {
        if (r.expected.size() == 70) {
            rich = &r;
            break;
        }
    }
    if (!RT_CHECK(rich != nullptr)) {
        return;
    }
    std::vector<pcr::HintText> out;
    std::vector<uint16_t> skipped;
    pcr::HintTextSource src = source(&fx, *rich);
    src.widths = nullptr;
    RT_CHECK(pcr::buildHintTexts(rich->hints, src, &out, &skipped));
    RT_CHECK(out.empty());
    RT_CHECK_EQ(skipped.size(), pcr::hintTextMessageNumbers().size());

    fx.hidden = {837, 12016, 13030, 14005, 3445};
    out.clear();
    skipped.clear();
    RT_CHECK(pcr::buildHintTexts(rich->hints, source(&fx, *rich), &out, &skipped));
    fx.hidden.clear();
    const std::set<uint16_t> sk(skipped.begin(), skipped.end());
    RT_CHECK(sk == std::set<uint16_t>({837, 12016, 13030, 14005, 3445}));
    RT_CHECK_EQ(out.size(), (size_t)65);
    const std::map<uint16_t, std::string> got = byMsg(out);
    for (const auto& [msg, text] : got) {
        RT_CHECK_EQ(text, fx.texts[rich->expected.at(msg)]);
    }

    // KoRL on with no hint (wwrando asserts); the texts before it stay.
    pcr::HintsResult h = rich->hints;
    for (auto& [p, hints] : h.placements) {
        if (p == pcr::HintPlacement::Korl) {
            hints.clear();
        }
    }
    out.clear();
    std::string err;
    RT_CHECK(!pcr::buildHintTexts(h, source(&fx, *rich), &out, nullptr, &err) && !err.empty());
    RT_CHECK(!out.empty() && byMsg(out).count(1502) == 0);

    // No octo fairy hint: only the tablet.
    h = rich->hints;
    h.hasOctoFairy = false;
    out.clear();
    RT_CHECK(pcr::buildHintTexts(h, source(&fx, *rich), &out, nullptr));
    RT_CHECK(out.size() == 1 && out[0].msg_no == 837);

    // An octo fairy hint without importance while importance is shown fails (wwrando's assert).
    h = rich->hints;
    h.octoFairy.importance = pcr::HintImportance::None;
    pcr::HintTextSource imp = source(&fx, *rich);
    imp.importance = true;
    out.clear();
    RT_CHECK(!pcr::buildHintTexts(h, imp, &out, nullptr));
}

// 5.
void checkOverlayAndNumbers(Fixture& fx) {
    const std::vector<uint16_t> all = pcr::hintTextMessageNumbers();
    const std::set<uint16_t> allSet(all.begin(), all.end());
    RT_CHECK_EQ(allSet.size(), all.size());
    RT_CHECK_EQ(all.size(), (size_t)70);
    for (const auto& [msg, type] : fx.boxTypes) {
        RT_CHECK(allSet.count(msg) != 0);
    }

    const Run& r = fx.runs.front();
    std::vector<pcr::HintText> out;
    RT_CHECK(pcr::buildHintTexts(r.hints, source(&fx, r), &out, nullptr));
    pc_rando_msg_clear();
    RT_CHECK_EQ(pcr::applyHintTexts(out), (int)out.size());
    RT_CHECK_EQ(pc_rando_msg_count(), (int)out.size());
    for (const pcr::HintText& t : out) {
        const PcRandoMsg* m = pc_rando_msg_find(t.msg_no);
        size_t len = 0;
        RT_CHECK_EQ(pc_rando_msg_encode(t.text.c_str(), nullptr, 0, &len), PCR_MSG_ERR_SPACE);
        std::vector<uint8_t> bytes(len);
        RT_CHECK_EQ(pc_rando_msg_encode(t.text.c_str(), bytes.data(), len, nullptr), PCR_MSG_OK);
        if (RT_CHECK(m != nullptr && m->bytes != nullptr)) {
            RT_CHECK(m->len == len && std::equal(bytes.begin(), bytes.end(), m->bytes));
        }
    }
    pc_rando_msg_clear();
}

} // namespace

int main() {
    Fixture fx;
    if (load(&fx)) {
        checkRuns(fx);
        checkSkips(fx);
        checkOverlayAndNumbers(fx);
    }
    checkHelpers();
    return rt::finish();
}
