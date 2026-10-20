// cos_rando_test_R4_2 (docs/RANDOMIZER_PLAN.md step R4.2): the text input's pure part
// (features/rando/pc_rando_text.h) without a game:
//   - the seed and permalink character sets and wwrando's sanitize_seed;
//   - the D-pad grid: layout, wrapping, case, delete, OK / cancel, and the key sequence of the
//     in-game smoke (native/check/input/rando-text.txt) typing "TestSeed1";
//   - permalinks: wwrando's own (fixtures/R4_2_permalinks.txt) parse to their seed and option bytes,
//     another version, bad base64 / characters / format, the file import (BOM, CRLF, wrapping).
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_text.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <unistd.h>

using namespace pc::rando;

namespace {

std::string hex(const std::string& bytes) {
    std::string out;
    char b[3];
    for (unsigned char c : bytes) {
        std::snprintf(b, sizeof b, "%02x", c);
        out += b;
    }
    return out;
}

void charsets() {
    const char* seedOk = "-_'%.azAZ09";
    for (const char* p = seedOk; *p; p++) {
        RT_CHECK(textCharAllowed(TextKind::Seed, *p));
    }
    for (const char* p = " +/=!\"#$&()*,:;<>?@[\\]^`{|}~\t"; *p; p++) {
        RT_CHECK(!textCharAllowed(TextKind::Seed, *p));
    }
    for (const char* p = "+/=azAZ09"; *p; p++) {
        RT_CHECK(textCharAllowed(TextKind::Permalink, *p));
    }
    for (const char* p = "-_'%. "; *p; p++) {
        RT_CHECK(!textCharAllowed(TextKind::Permalink, *p));
    }
    RT_CHECK(!textCharAllowed(TextKind::Seed, '\0'));
    RT_CHECK_EQ(textMaxLength(TextKind::Seed), 42);
    // sanitize_seed: strip, filter, cut to 42
    RT_CHECK_EQ(textSanitize(TextKind::Seed, "  Test Seed!1 \n"), std::string("TestSeed1"));
    RT_CHECK_EQ(textSanitize(TextKind::Seed, "x-_'%.Z9"), std::string("x-_'%.Z9"));
    RT_CHECK_EQ(textSanitize(TextKind::Seed, nullptr), std::string());
    const std::string longSeed(50, 'q');
    RT_CHECK_EQ(textSanitize(TextKind::Seed, longSeed.c_str()), std::string(42, 'q'));
    RT_CHECK_EQ(textSanitize(TextKind::Permalink, " MS4x\nMC4w ="), std::string("MS4xMC4w="));
}

void layout() {
    TextGrid g(TextKind::Seed);
    RT_CHECK_EQ(g.rows(), 6);
    RT_CHECK_EQ(g.rowLength(0), 10);
    RT_CHECK_EQ(g.rowLength(2), 6);
    RT_CHECK_EQ(g.rowLength(3), 10);
    RT_CHECK_EQ(g.rowLength(4), 5);
    RT_CHECK_EQ(g.rowLength(5), 4);
    RT_CHECK_EQ(g.cellAt(0, 0).ch, 'a');
    RT_CHECK_EQ(g.cellAt(1, 9).ch, 't');
    RT_CHECK_EQ(g.cellAt(2, 5).ch, 'z');
    RT_CHECK_EQ(g.cellAt(3, 1).ch, '1');
    RT_CHECK_EQ(g.cellAt(4, 4).ch, '.');
    RT_CHECK(g.cellAt(5, 0).action == TextAction::Shift);
    RT_CHECK(g.cellAt(5, 1).action == TextAction::Delete);
    RT_CHECK(g.cellAt(5, 2).action == TextAction::Ok);
    RT_CHECK(g.cellAt(5, 3).action == TextAction::Cancel);
    RT_CHECK(g.cellAt(2, 6).action == TextAction::Char && g.cellAt(2, 6).ch == '\0'); // past the row
    TextGrid p(TextKind::Permalink);
    RT_CHECK_EQ(p.rowLength(4), 3);
    RT_CHECK_EQ(p.cellAt(4, 0).ch, '+');
    RT_CHECK_EQ(p.cellAt(4, 2).ch, '=');

    // wrapping and the column clamp
    g.press(kTextUp);
    RT_CHECK(g.row() == 5 && g.col() == 0);
    g.press(kTextLeft);
    RT_CHECK(g.row() == 5 && g.col() == 3);
    g.press(kTextDown);
    RT_CHECK(g.row() == 0 && g.col() == 3);
    g.press(kTextLeft);
    g.press(kTextLeft);
    g.press(kTextLeft);
    g.press(kTextLeft);
    RT_CHECK(g.row() == 0 && g.col() == 9);
    g.press(kTextDown);
    g.press(kTextDown);
    RT_CHECK(g.row() == 2 && g.col() == 5); // clamped to the 6-letter row
    g.press(kTextShiftL);
    RT_CHECK(g.upper());
    RT_CHECK_EQ(g.cellAt(2, 5).ch, 'Z');
    RT_CHECK(g.text().empty() && g.state() == TextState::Editing);
}

// The key sequence of native/check/input/rando-text.txt (one entry per pressed frame), in order.
const uint32_t kSmokeKeys[] = {
    // T: R (upper), down, left (wraps to t), A, R (lower)
    kTextShiftR, kTextDown, kTextLeft, kTextConfirm, kTextShiftR,
    // e: up, left x5, A
    kTextUp, kTextLeft, kTextLeft, kTextLeft, kTextLeft, kTextLeft, kTextConfirm,
    // s: down, right x4, A; t: right, A
    kTextDown, kTextRight, kTextRight, kTextRight, kTextRight, kTextConfirm, kTextRight, kTextConfirm,
    // S: R, left, A, R
    kTextShiftR, kTextLeft, kTextConfirm, kTextShiftR,
    // e e: up, left x4, A, A; d: left, A
    kTextUp, kTextLeft, kTextLeft, kTextLeft, kTextLeft, kTextConfirm, kTextConfirm, kTextLeft, kTextConfirm,
    // a typo: c (left, A), then B deletes it
    kTextLeft, kTextConfirm, kTextBack,
    // 1: down x3, left, A
    kTextDown, kTextDown, kTextDown, kTextLeft, kTextConfirm,
    // OK: down x2, right, A
    kTextDown, kTextDown, kTextRight, kTextConfirm,
};

void smokeSequence() {
    TextGrid g(TextKind::Seed);
    std::string afterTypo;
    int i = 0;
    for (uint32_t k : kSmokeKeys) {
        g.press(k);
        if (++i == 35) {
            afterTypo = g.text(); // right after the typo's A
        }
    }
    RT_CHECK_EQ(afterTypo, std::string("TestSeedc"));
    RT_CHECK_EQ(g.text(), std::string("TestSeed1"));
    RT_CHECK(g.state() == TextState::Done);
    const unsigned v = g.version();
    g.press(kTextConfirm); // finished: ignored
    RT_CHECK(g.version() == v && g.state() == TextState::Done);
}

void editing() {
    TextGrid g(TextKind::Seed, " ab c ");
    RT_CHECK_EQ(g.text(), std::string("abc"));
    g.press(kTextBack);
    g.press(kTextBack);
    g.press(kTextBack);
    RT_CHECK(g.text().empty() && g.state() == TextState::Editing);
    g.press(kTextBack); // B with no text: cancel
    RT_CHECK(g.state() == TextState::Cancelled);

    TextGrid c(TextKind::Seed, "x");
    c.pressCell(5, 3); // Cancel
    RT_CHECK(c.state() == TextState::Cancelled && c.text() == "x");

    TextGrid m(TextKind::Seed);
    m.pressCell(1, 2); // m
    m.pressCell(5, 0); // Shift
    m.pressCell(0, 0); // A
    m.pressCell(9, 0); // out of range: nothing
    m.pressCell(4, 5);
    RT_CHECK_EQ(m.text(), std::string("mA"));
    m.pressCell(5, 1); // Delete
    RT_CHECK_EQ(m.text(), std::string("m"));
    const unsigned v = m.version();
    m.setText("m");
    RT_CHECK_EQ(m.version(), v); // unchanged text: no new version
    m.setText("New Seed?");
    RT_CHECK(m.text() == "NewSeed" && m.version() == v + 1);
    m.pressCell(5, 2); // OK
    RT_CHECK(m.state() == TextState::Done);

    TextGrid full(TextKind::Seed, std::string(42, 'z').c_str());
    full.press(kTextConfirm); // 'a' past the maximum: not typed
    RT_CHECK_EQ(full.text().size(), (size_t)42);
}

void permalinks() {
    std::string fx;
    RT_CHECK(rt::readFixture("R4_2_permalinks.txt", fx));
    std::istringstream in(fx);
    std::string line;
    int vectors = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ls(line);
        std::string link, seed, opts;
        ls >> link >> seed >> opts;
        const PermalinkInfo info = permalinkParse(link.c_str());
        RT_CHECK(info.status == PermalinkStatus::Ok);
        RT_CHECK_EQ(info.version, std::string(kPermalinkVersion));
        RT_CHECK_EQ(info.seed, seed);
        RT_CHECK_EQ(hex(info.options), opts);
        RT_CHECK_EQ(info.text, link);
        // the same permalink wrapped over lines with spaces around
        const std::string wrapped = "  " + link.substr(0, 20) + "\r\n" + link.substr(20) + "\n";
        RT_CHECK(permalinkParse(wrapped.c_str()).status == PermalinkStatus::Ok);
        RT_CHECK_EQ(permalinkParse(wrapped.c_str()).text, link);
        // a seed typed on the grid takes its permalink's seed as is
        RT_CHECK_EQ(textSanitize(TextKind::Seed, info.seed.c_str()), seed);
        vectors++;
    }
    RT_CHECK_EQ(vectors, 4);

    // base64 of "1.9.0\0abc\0\x01": another version
    const PermalinkInfo old = permalinkParse("MS45LjAAYWJjAAE=");
    RT_CHECK(old.status == PermalinkStatus::OtherVersion);
    RT_CHECK(old.version == "1.9.0" && old.seed == "abc" && old.options == std::string("\x01", 1));
    RT_CHECK(permalinkParse("").status == PermalinkStatus::Empty);
    RT_CHECK(permalinkParse(" \r\n\t").status == PermalinkStatus::Empty);
    RT_CHECK(permalinkParse(nullptr).status == PermalinkStatus::Empty);
    RT_CHECK(permalinkParse("ab$c").status == PermalinkStatus::BadChars);
    RT_CHECK(permalinkParse("abc").status == PermalinkStatus::BadBase64);
    RT_CHECK(permalinkParse("ab=c").status == PermalinkStatus::BadBase64);
    RT_CHECK(permalinkParse("a===").status == PermalinkStatus::BadBase64);
    RT_CHECK(permalinkParse("bm9OdWw=").status == PermalinkStatus::BadFormat); // "noNul"
    RT_CHECK(permalinkParse("YQBi").status == PermalinkStatus::BadFormat);     // "a\0b": one NUL
    RT_CHECK(permalinkParse(std::string(404, 'A').c_str()).status == PermalinkStatus::TooLong);
    std::string bytes;
    RT_CHECK(base64Decode("TWE=", &bytes) && bytes == "Ma");
    RT_CHECK(base64Decode("TWFu", &bytes) && bytes == "Man");
    RT_CHECK(base64Decode("TQ==", &bytes) && bytes == "M");
    RT_CHECK(base64Decode("", &bytes) && bytes.empty());
    RT_CHECK(!base64Decode("TQ=a", &bytes));
    RT_CHECK(!base64Decode("TQ==TWFu", &bytes));

    // the file import
    char path[] = "/tmp/cos_rando_test_R4_2_XXXXXX";
    const int fd = mkstemp(path);
    RT_CHECK(fd >= 0);
    if (fd >= 0) {
        const char data[] = "\xEF\xBB\xBFMS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAEkQUBgAAPsCyCUA\r\nAAAAAAEAQQCAAA==\r\n";
        RT_CHECK(write(fd, data, sizeof data - 1) == (ssize_t)(sizeof data - 1));
        close(fd);
        const PermalinkInfo f = permalinkReadFile(path);
        RT_CHECK(f.status == PermalinkStatus::Ok);
        RT_CHECK_EQ(f.seed, std::string("TestSeed1"));
        RT_CHECK_EQ(f.text, std::string("MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAEkQUBgAAPsCyCUAAAAAAAEAQQCAAA=="));
        unlink(path);
    }
    RT_CHECK(permalinkReadFile(path).status == PermalinkStatus::NoFile);
    RT_CHECK(permalinkReadFile(nullptr).status == PermalinkStatus::NoFile);
    RT_CHECK(std::strcmp(permalinkStatusName(PermalinkStatus::OtherVersion), "another wwrando version") == 0);
}

} // namespace

int main() {
    charsets();
    layout();
    smokeSequence();
    editing();
    permalinks();
    return rt::finish();
}
