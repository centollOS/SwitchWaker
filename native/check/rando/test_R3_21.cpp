// R3.21 (docs/RANDOMIZER_PLAN.md): the seed-dependent message texts (pc_rando_posttext.h).
//  1. pcr::buildPostTexts on the made-up BMG, font and item names of R3_21_posttext.txt
//     (native/tools/rando/make_posttext_fixture.py, wwrando's own tweaks.py functions) gives every
//     text wwrando leaves in those messages, byte for byte (a chart message with nothing to replace
//     is left out, as its text is unchanged);
//  2. the options: swift_sail off leaves 463 out, post_randomization off leaves the shop, auction,
//     Battlesquid and letter messages out; a missing font, item name or BMG text skips just the
//     messages that need it, and reports them;
//  3. applyPostTexts puts the texts in the overlay, encoded as pc_rando_msg_encode does;
//  4. postTextMessageNumbers covers every message built.
#include "rando_test.h"

#include "pc_rando_data.h"
#include "pc_rando_msg.h"
#include "pc_rando_posttext.h"

#include <algorithm>
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

struct Fixture {
    pcr::GlyphWidths widths{};
    bool haveWidths = false;
    std::map<uint16_t, std::pair<uint8_t, std::string>> bmg; // msg -> (box type, text)
    std::map<int, std::string> items;                         // location index -> item name
    std::vector<std::pair<uint16_t, std::string>> expected;   // S lines in order
    bool dropItems = false;                                   // item_name returns "" (case 2)
    std::set<uint16_t> hidden;                                // vanilla() fails for these
};

std::string itemName(void* ctx, int loc) {
    const Fixture* f = (const Fixture*)ctx;
    if (f->dropItems) {
        return std::string();
    }
    auto it = f->items.find(loc);
    return it != f->items.end() ? it->second : std::string();
}

bool vanilla(void* ctx, uint16_t msg_no, std::string* text, uint8_t* type) {
    const Fixture* f = (const Fixture*)ctx;
    if (f->hidden.count(msg_no) != 0) {
        return false;
    }
    auto it = f->bmg.find(msg_no);
    if (it == f->bmg.end()) {
        return false;
    }
    *type = it->second.first;
    *text = it->second.second;
    return true;
}

bool load(Fixture* fx) {
    std::string text;
    if (!RT_CHECK(rt::readFixture("R3_21_posttext.txt", text))) {
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
        } else if (f[0] == "V" && RT_CHECK_EQ(f.size(), (size_t)4)) {
            fx->bmg[(uint16_t)std::stoi(f[1])] = {(uint8_t)std::stoi(f[2]), unhex(f[3])};
        } else if (f[0] == "L" && RT_CHECK_EQ(f.size(), (size_t)3)) {
            const std::string loc = unhex(f[1]);
            const int idx = pcr::location_index(loc.c_str());
            if (RT_CHECK(idx >= 0)) {
                fx->items[idx] = unhex(f[2]);
            }
        } else if (f[0] == "S" && RT_CHECK_EQ(f.size(), (size_t)3)) {
            fx->expected.push_back({(uint16_t)std::stoi(f[1]), unhex(f[2])});
        }
    }
    return RT_CHECK(fx->haveWidths) && RT_CHECK(fx->expected.size() > 50) && RT_CHECK_EQ(fx->items.size(), (size_t)10);
}

pcr::PostTextSource source(Fixture* fx) {
    pcr::PostTextSource src;
    src.widths = &fx->widths;
    src.item_name = itemName;
    src.vanilla = vanilla;
    src.ctx = fx;
    return src;
}

std::map<uint16_t, std::string> byMsg(const std::vector<pcr::PostText>& v) {
    std::map<uint16_t, std::string> m;
    for (const pcr::PostText& t : v) {
        RT_CHECK(m.count(t.msg_no) == 0); // each message once
        m[t.msg_no] = t.text;
    }
    return m;
}

void checkAll(Fixture& fx) {
    std::vector<pcr::PostText> out;
    std::vector<uint16_t> skipped;
    pcr::buildPostTexts(source(&fx), &out, &skipped);
    RT_CHECK(skipped.empty());
    const std::map<uint16_t, std::string> got = byMsg(out);
    int compared = 0, unchanged = 0;
    for (const auto& [msg, want] : fx.expected) {
        auto it = got.find(msg);
        if (it == got.end()) {
            // wwrando wrote the text back unchanged: only allowed for a chart message
            RT_CHECK(fx.bmg.count(msg) != 0 && fx.bmg[msg].second == want);
            unchanged++;
            continue;
        }
        if (!RT_CHECK_EQ(it->second, want)) {
            std::fprintf(stderr, "  message %u\n", (unsigned)msg);
        }
        compared++;
    }
    RT_CHECK_EQ(got.size(), (size_t)compared); // nothing built that wwrando does not write
    RT_CHECK(unchanged > 0 && unchanged < 30);  // the fixture's "nothing to replace" chart messages
    // wwrando's order: the fixed tweaks first, then the post-randomization ones
    RT_CHECK_EQ(out.front().msg_no, (uint16_t)463);
    RT_CHECK_EQ(out.back().msg_no, (uint16_t)3325);
    std::printf("compared %d messages (%d unchanged chart messages)\n", compared, unchanged);

    // 3. the overlay
    pc_rando_msg_clear();
    RT_CHECK_EQ(pcr::applyPostTexts(out), (int)out.size());
    RT_CHECK_EQ(pc_rando_msg_count(), (int)out.size());
    for (const pcr::PostText& t : out) {
        const PcRandoMsg* r = pc_rando_msg_find(t.msg_no);
        size_t len = 0;
        RT_CHECK_EQ(pc_rando_msg_encode(t.text.c_str(), nullptr, 0, &len), PCR_MSG_ERR_SPACE);
        std::vector<uint8_t> bytes(len);
        RT_CHECK_EQ(pc_rando_msg_encode(t.text.c_str(), bytes.data(), len, nullptr), PCR_MSG_OK);
        if (RT_CHECK(r != nullptr && r->bytes != nullptr && !r->has_entry)) {
            RT_CHECK(r->len == len && std::equal(bytes.begin(), bytes.end(), r->bytes));
        }
    }
    pc_rando_msg_clear();

    // 4. the message list
    const std::vector<uint16_t> all = pcr::postTextMessageNumbers();
    const std::set<uint16_t> allSet(all.begin(), all.end());
    RT_CHECK_EQ(allSet.size(), all.size());
    for (const auto& [msg, want] : fx.expected) {
        RT_CHECK(allSet.count(msg) != 0);
    }
    RT_CHECK_EQ(all.size(), fx.expected.size());
    RT_CHECK_EQ(pcr::chartMessageNumbers().size(), (size_t)49); // 41 treasure charts + 8 triforce charts
}

void checkOptions(Fixture& fx) {
    pcr::PostTextSource src = source(&fx);
    std::vector<pcr::PostText> out;
    std::vector<uint16_t> skipped;
    std::map<uint16_t, std::string> full;
    pcr::buildPostTexts(src, &out, nullptr);
    full = byMsg(out);

    src.swift_sail = false;
    out.clear();
    pcr::buildPostTexts(src, &out, &skipped);
    std::map<uint16_t, std::string> m = byMsg(out);
    RT_CHECK(m.count(463) == 0 && m.size() + 1 == full.size() && skipped.empty());

    src.swift_sail = true;
    src.post_randomization = false;
    out.clear();
    pcr::buildPostTexts(src, &out, &skipped);
    m = byMsg(out);
    for (uint16_t n : {3906, 3909, 12106, 12107, 12108, 12109, 12110, 12111, 7440, 7441, 7442, 7443, 7520, 7521, 3325}) {
        RT_CHECK(m.count(n) == 0);
    }
    RT_CHECK(m.count(463) == 1 && m.count(3443) == 1 && m.count(3957) == 1 && m.size() + 15 == full.size());
    RT_CHECK(skipped.empty());

    // no font: the wrapped messages are skipped and reported, the rest unchanged
    src = source(&fx);
    src.widths = nullptr;
    out.clear();
    skipped.clear();
    pcr::buildPostTexts(src, &out, &skipped);
    m = byMsg(out);
    RT_CHECK(m.count(7520) == 0 && m.count(7521) == 0 && m.count(3325) == 0 && m.size() + 3 == full.size());
    RT_CHECK_EQ(skipped.size(), (size_t)3);
    for (const auto& [msg, text] : m) {
        RT_CHECK(full[msg] == text);
    }

    // no item names: only the fixed tweaks remain; every post-randomization message is reported
    src = source(&fx);
    fx.dropItems = true;
    out.clear();
    skipped.clear();
    pcr::buildPostTexts(src, &out, &skipped);
    fx.dropItems = false;
    m = byMsg(out);
    RT_CHECK(m.size() + 15 == full.size());
    RT_CHECK_EQ(skipped.size(), (size_t)15);

    // missing BMG texts: Beedle, the letter and one chart message are skipped and reported
    const uint16_t chart = pcr::chartMessageNumbers().front();
    fx.hidden = {3957, 3325, chart};
    out.clear();
    skipped.clear();
    pcr::buildPostTexts(src, &out, &skipped);
    fx.hidden.clear();
    m = byMsg(out);
    RT_CHECK(m.count(3957) == 0 && m.count(3325) == 0 && m.count(chart) == 0);
    std::sort(skipped.begin(), skipped.end());
    std::vector<uint16_t> want = {chart, 3325, 3957};
    std::sort(want.begin(), want.end());
    RT_CHECK(skipped == want);
}

void checkLetterShape(Fixture& fx) {
    // The hint takes lines 8-11 (four lines here) and every non-empty hint line starts with two spaces.
    const std::string& vanillaLetter = fx.bmg[3325].second;
    const std::string s = pcr::rockSpireLetter(fx.widths, 7, vanillaLetter, "Empty Bottle", "Orange Rupee", "Joy Pendant");
    const std::vector<std::string> lines = split(s, '\n');
    const std::vector<std::string> before = split(vanillaLetter, '\n');
    RT_CHECK(lines.size() >= before.size());
    for (size_t i = 0; i < 8; i++) {
        RT_CHECK_EQ(lines[i], before[i]);
    }
    RT_CHECK(lines[8].rfind("  Do you have need of an ", 0) == 0);
    RT_CHECK_EQ(lines.back(), before.back());
}

} // namespace

int main() {
    Fixture fx;
    if (load(&fx)) {
        checkAll(fx);
        checkOptions(fx);
        checkLetterShape(fx);
    }
    return rt::finish();
}
