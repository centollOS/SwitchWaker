// R3.20 (docs/RANDOMIZER_PLAN.md): the randomizer's text helpers (pc_rando_wrap.h).
//  1. pcr::wordWrapString gives gclib's BFN.word_wrap_string output for every case of the fixture
//     R3_20_wrap.txt (native/tools/rando/make_wrap_fixture.py: wwrando's tagged strings plus
//     hand-written ones, five line lengths, control codes kept and dropped), on the fixture's fixed
//     width table;
//  2. padStringToNext4Lines and constructStringFromParts against gclib's bmg.py;
//  3. addArticleBeforeItemName, upperFirstLetter and indefiniteArticle against tweaks.py for every
//     name of data/item_names.txt, and itemNameWithArticle for a few ids;
//  4. glyph widths (cp1252 extras, replacement), text box widths and the cases where gclib raises.
#include "rando_test.h"

#include "pc_rando_data.h"
#include "pc_rando_wrap.h"

#include <cstdio>
#include <cstring>
#include <sstream>
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

void fixture() {
    std::string text;
    if (!RT_CHECK(rt::readFixture("R3_20_wrap.txt", text))) {
        return;
    }
    pcr::GlyphWidths w{};
    bool haveWidths = false;
    int nW = 0, nP = 0, nC = 0, nA = 0;
    for (const std::string& line : split(text, '\n')) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> f = split(line, ' ');
        if (f[0] == "F") {
            if (!RT_CHECK_EQ(f.size(), (size_t)258)) {
                return;
            }
            for (int c = 0; c < 256; c++) {
                w.code[c] = (uint8_t)std::stoi(f[1 + c]);
            }
            w.replacement = (uint8_t)std::stoi(f[257]);
            haveWidths = true;
        } else if (f[0] == "W" && RT_CHECK(haveWidths && f.size() == 5)) {
            const std::string in = unhex(f[3]);
            const std::string got = pcr::wordWrapString(w, in, std::stoi(f[1]), f[2] == "1");
            if (!RT_CHECK_EQ(got, unhex(f[4]))) {
                std::fprintf(stderr, "  width %s preserve %s input: %s\n", f[1].c_str(), f[2].c_str(),
                             in.c_str());
            }
            nW++;
        } else if (f[0] == "P" && RT_CHECK(f.size() == 3)) {
            RT_CHECK_EQ(pcr::padStringToNext4Lines(unhex(f[1])), unhex(f[2]));
            nP++;
        } else if (f[0] == "C" && RT_CHECK(haveWidths && f.size() == 5)) {
            std::vector<std::string> parts;
            for (const std::string& p : split(f[3], ',')) {
                parts.push_back(unhex(p));
            }
            RT_CHECK_EQ(pcr::constructStringFromParts(w, (uint8_t)std::stoi(f[1]), parts,
                                                      std::stoi(f[2])),
                        unhex(f[4]));
            nC++;
        } else if (f[0] == "A" && RT_CHECK(f.size() == 5)) {
            const std::string name = unhex(f[1]);
            const std::string art = pcr::addArticleBeforeItemName(name);
            if (!RT_CHECK_EQ(art, unhex(f[2]))) {
                std::fprintf(stderr, "  item: %s\n", name.c_str());
            }
            RT_CHECK_EQ(pcr::upperFirstLetter(art), unhex(f[3]));
            RT_CHECK_EQ(std::string(pcr::indefiniteArticle(name)), f[4]);
            nA++;
        }
    }
    std::printf("fixture: %d wraps, %d pads, %d part sets, %d item names\n", nW, nP, nC, nA);
    RT_CHECK(nW >= 500);
    RT_CHECK_EQ(nP, 8);
    RT_CHECK_EQ(nC, 5);
    RT_CHECK_EQ((size_t)nA, pcr::item_names().size);
}

pcr::GlyphWidths flat(int width) {
    pcr::GlyphWidths w{};
    memset(w.code, width, sizeof w.code);
    w.replacement = (uint8_t)width;
    return w;
}

void unit() {
    pcr::GlyphWidths w = flat(10);
    w.code[0x80] = 33; // cp1252 euro
    w.code[0x97] = 34; // em dash
    w.code[0xE9] = 35; // e acute
    w.replacement = 2;
    RT_CHECK_EQ(pcr::glyphWidth(w, 'A'), 10);
    RT_CHECK_EQ(pcr::glyphWidth(w, 0x20AC), 33);
    RT_CHECK_EQ(pcr::glyphWidth(w, 0x2014), 34);
    RT_CHECK_EQ(pcr::glyphWidth(w, 0xE9), 35);
    RT_CHECK_EQ(pcr::glyphWidth(w, 0x80), 2); // U+0080 is not in cp1252
    RT_CHECK_EQ(pcr::glyphWidth(w, 0x2603), 2);

    RT_CHECK_EQ(pcr::textBoxMaxLineLength(0), 503);
    RT_CHECK_EQ(pcr::textBoxMaxLineLength(9), 419);
    RT_CHECK_EQ(pcr::textBoxMaxLineLength(0xE), 419);
    RT_CHECK_EQ(pcr::textBoxMaxLineLength(0xA), 503);

    // Flat width 10: a line of 50 pixels holds "aaaa" (40) + a space; the space that reaches 50 breaks.
    const pcr::GlyphWidths f = flat(10);
    RT_CHECK_EQ(pcr::wordWrapString(f, "aaaa bbbb cc", 50), std::string("aaaa\nbbbb\ncc"));
    RT_CHECK_EQ(pcr::wordWrapString(f, "aa bb cc", 50), std::string("aa bb\ncc"));
    // A word as wide as a line goes out whole once it passes the line, with its break.
    RT_CHECK_EQ(pcr::wordWrapString(f, "abcdefghij", 50), std::string("abcdef\nghij"));
    RT_CHECK_EQ(pcr::wordWrapString(f, "\\{1A 05 00 00 01}ab cd", 50, false), std::string("ab cd"));
    RT_CHECK_EQ(pcr::wordWrapString(f, "\\{1A 05 00 00 01}ab cd", 50),
                std::string("\\{1A 05 00 00 01}ab cd"));
    // Where gclib raises: a lone backslash is a character, an unclosed tag runs to the end.
    RT_CHECK_EQ(pcr::wordWrapString(f, "a\\b", 50), std::string("a\\b"));
    RT_CHECK_EQ(pcr::wordWrapString(f, "ab \\{1A 05", 50), std::string("ab \\{1A 05"));
    // A multi-byte character counts once.
    pcr::GlyphWidths e = flat(10);
    e.code[0xE9] = 40;
    RT_CHECK_EQ(pcr::wordWrapString(e, "\xC3\xA9\xC3\xA9", 50), std::string("\xC3\xA9\xC3\xA9\n"));
    RT_CHECK_EQ(pcr::wordWrapString(e, "ab \xC3\xA9\xC3\xA9", 50), std::string("ab\n\xC3\xA9\xC3\xA9\n"));

    RT_CHECK_EQ(std::string(pcr::indefiniteArticle("")), std::string("a"));
    RT_CHECK_EQ(std::string(pcr::indefiniteArticle("  Ice Arrow")), std::string("an"));
    RT_CHECK_EQ(pcr::upperFirstLetter(""), std::string(""));
    RT_CHECK_EQ(pcr::upperFirstLetter("\xC3\xA9t\xC3\xA9"), std::string("\xC3\x89t\xC3\xA9"));
    RT_CHECK_EQ(pcr::upperFirstLetter("1st"), std::string("1st"));

    RT_CHECK_EQ(pcr::itemNameWithArticle(0x07), std::string("a Piece of Heart"));
    RT_CHECK_EQ(pcr::itemNameWithArticle(0x07, true), std::string("A Piece of Heart"));
    RT_CHECK_EQ(pcr::itemNameWithArticle((uint8_t)pcr::item_id("Wind Waker"), true),
                std::string("The Wind Waker"));
    RT_CHECK_EQ(pcr::itemNameWithArticle((uint8_t)pcr::item_id("Din's Pearl")),
                std::string("Din's Pearl"));
    RT_CHECK_EQ(pcr::itemNameWithArticle((uint8_t)pcr::item_id("Triforce Shard 1")),
                std::string("Triforce Shard 1"));
    for (int id = 0; id < 256; id++) {
        if (pcr::item_name((uint8_t)id) == nullptr) {
            RT_CHECK_EQ(pcr::itemNameWithArticle((uint8_t)id), std::string());
            break;
        }
    }
}

} // namespace

int main() {
    fixture();
    unit();
    return rt::finish();
}
