// COS_SMOKE=rando COS_RANDO_SMOKE=wrap (docs/RANDOMIZER_PLAN.md step R3.20): the randomizer's word
// wrap with the game's real message font, e.g.
//   native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=wrap
// Once the play scene has run 10 frames:
//   1. pc_rando_font_widths() reads the message font's glyph widths (rock_24_20_4i_usa.bfn) and
//      gives a plausible table (space and letters non-zero, 'W' wider than 'i'), the same one on a
//      second call, and the font archive's BFN read with a temporary JUTResFont (the fallback when
//      the font is not loaded) gives the same table;
//   2. the three samples of native/check/rando/fixtures/R3_20_wrap_font.txt (made by
//      native/tools/rando/make_wrap_fixture.py --disc with gclib's BFN.word_wrap_string on the
//      disc's font) wrap to exactly gclib's output at their text box's width;
//   3. construct_string_from_parts on the samples gives whole 4-line boxes.
// The fixture is found from this source file's path (native/check/rando/fixtures), or in
// COS_RANDO_FIXTURES. Exit 0 when all hold, else 1. pc_rando() is not needed: the helpers are pure.
#include "pc_rando_smokes.h"

#include "features/rando/pc_rando_wrap.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace pc {

namespace {

constexpr const char* kName = "wrap";
constexpr const char* kFixture = "R3_20_wrap_font.txt";

// Candidate fixture folders: COS_RANDO_FIXTURES, else native/check/rando/fixtures found from this
// source file's path. __FILE__ may be relative to the build folder (ninja's ../../native/...), so it
// is also tried from the build folder that holds run.sh's run dir (<build>/runs/<run>).
std::vector<std::string> fixtureDirs() {
    std::vector<std::string> out;
    const char* env = std::getenv("COS_RANDO_FIXTURES");
    if (env != nullptr && env[0] != '\0') {
        out.push_back(env);
        return out;
    }
    // .../native/src/pc/harness/smokes/<this file> -> .../native/check/rando/fixtures
    std::string dir = __FILE__;
    for (int up = 0; up < 5; up++) {
        const size_t slash = dir.find_last_of('/');
        if (slash == std::string::npos) {
            return out;
        }
        dir.resize(slash);
    }
    dir += "/check/rando/fixtures";
    out.push_back(dir);
    const char* run = std::getenv("COS_RUN_DIR");
    if (dir[0] != '/' && run != nullptr && run[0] != '\0') {
        out.push_back(std::string(run) + "/../../" + dir);
    }
    return out;
}

std::string unhex(const std::string& h) {
    std::string out;
    if (h == "-") {
        return out;
    }
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back((char)std::strtol(h.substr(i, 2).c_str(), nullptr, 16));
    }
    return out;
}

struct Sample {
    uint8_t box;
    std::string in;
    std::string want;
};

bool readSamples(std::vector<Sample>& out) {
    FILE* f = nullptr;
    for (const std::string& dir : fixtureDirs()) {
        const std::string path = dir + "/" + kFixture;
        f = std::fopen(path.c_str(), "rb");
        if (f != nullptr) {
            break;
        }
        std::fprintf(stderr, "[cos] rando-smoke wrap: cannot read %s\n", path.c_str());
    }
    if (f == nullptr) {
        return false;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
        text.append(buf, n);
    }
    std::fclose(f);
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) {
            eol = text.size();
        }
        const std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (line.size() < 2 || line[0] != 'M') {
            continue;
        }
        const size_t a = line.find(' ', 2);
        const size_t b = a == std::string::npos ? a : line.find(' ', a + 1);
        if (b == std::string::npos) {
            return false;
        }
        out.push_back({(uint8_t)std::atoi(line.substr(2, a - 2).c_str()), unhex(line.substr(a + 1, b - a - 1)),
                       unhex(line.substr(b + 1))});
    }
    return true;
}

int countLines(const std::string& s) {
    int n = 0;
    for (char c : s) {
        n += c == '\n';
    }
    return n;
}

unsigned int sEndFrame = 0;

} // namespace

void randoSmokeWrap(unsigned int frames) {
    if (!randoSmokeInPlay(10)) {
        return;
    }
    if (sEndFrame != 0) {
        if (frames >= sEndFrame) {
            randoSmokeEnd(kName);
        }
        return;
    }
    const pcr::GlyphWidths* w = pc_rando_font_widths();
    if (!randoSmokeCheck(kName, "font widths read", w != nullptr)) {
        randoSmokeEnd(kName);
    }
    std::fprintf(stderr, "[cos] rando-smoke wrap: widths ' '=%d i=%d W=%d a=%d e-acute=%d replacement=%d\n",
                 w->code[' '], w->code['i'], w->code['W'], w->code['a'], w->code[0xE9], w->replacement);
    randoSmokeCheck(kName, "widths plausible",
                    w->code[' '] > 0 && w->code['a'] > 0 && w->code['W'] > w->code['i']);
    randoSmokeCheck(kName, "widths cached", pc_rando_font_widths() == w);
    pcr::GlyphWidths fromArchive{};
    randoSmokeCheck(kName, "archive font gives the same widths",
                    pc_rando_font_widths_from_archive(&fromArchive) &&
                        std::memcmp(&fromArchive, w, sizeof fromArchive) == 0);

    std::vector<Sample> samples;
    randoSmokeCheck(kName, "fixture read (3 samples)", readSamples(samples) && samples.size() == 3);
    std::vector<std::string> parts;
    for (size_t i = 0; i < samples.size(); i++) {
        const Sample& s = samples[i];
        const std::string got = pcr::wordWrapMessage(*w, s.box, s.in);
        char what[64];
        std::snprintf(what, sizeof what, "sample %zu (box %u) wraps as gclib", i + 1, s.box);
        if (!randoSmokeCheck(kName, what, got == s.want)) {
            std::fprintf(stderr, "  got:  %s\n  want: %s\n", got.c_str(), s.want.c_str());
        }
        parts.push_back(s.in);
    }
    const std::string boxes = pcr::constructStringFromParts(*w, 0, parts);
    randoSmokeCheck(kName, "parts fill whole 4-line boxes", !parts.empty() && countLines(boxes) % 4 == 0 &&
                                                                 countLines(boxes) >= 4 * (int)parts.size());
    sEndFrame = frames + 30;
}

} // namespace pc
