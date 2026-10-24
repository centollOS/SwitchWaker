// R7.6c (docs/RANDOMIZER_PLAN.md): the BMG lookup the European disc's English base texts use
// (pc_rando_bmg.h), on a synthetic BMG built here (no disc bytes):
//  1. a message's INF1 entry and DAT1 string are found and decode to the text written;
//  2. an entry with string offset 0 is skipped, as fopMsgM_msgGet_c::getMessage skips it, and a later
//     entry with the same number is taken;
//  3. sections in another order, an extra section and a larger entry size are read;
//  4. a missing number, a bad magic, a truncated file, an offset past DAT1 and a short entry size are
//     refused.
#include "rando_test.h"

#include "pc_rando_bmg.h"
#include "pc_rando_msg.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

void put32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    v[at] = (uint8_t)(x >> 24);
    v[at + 1] = (uint8_t)(x >> 16);
    v[at + 2] = (uint8_t)(x >> 8);
    v[at + 3] = (uint8_t)x;
}

void put16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
    v[at] = (uint8_t)(x >> 8);
    v[at + 1] = (uint8_t)x;
}

struct Msg {
    uint16_t no;
    const char* text; // wwrando syntax; nullptr: an entry with string offset 0
    uint8_t box;
};

std::vector<uint8_t> section(const char* magic, const std::vector<uint8_t>& body) {
    std::vector<uint8_t> s(8 + body.size());
    std::memcpy(s.data(), magic, 4);
    put32(s, 4, (uint32_t)s.size());
    std::memcpy(s.data() + 8, body.data(), body.size());
    return s;
}

// A "MESGbmg1" file: INF1 (entry size `entrySize`) and DAT1, plus an extra section when `extra`, in
// the order DAT1 first when `datFirst`.
std::vector<uint8_t> makeBmg(const std::vector<Msg>& msgs, uint16_t entrySize = 0x18, bool datFirst = false,
                             bool extra = false) {
    std::vector<uint8_t> dat(1, 0); // offset 0 is the empty string, as in the game's BMGs
    std::vector<uint8_t> inf(8 + msgs.size() * entrySize, 0);
    put16(inf, 0, (uint16_t)msgs.size());
    put16(inf, 2, entrySize);
    for (size_t i = 0; i < msgs.size(); i++) {
        const size_t e = 8 + i * entrySize;
        uint32_t offs = 0;
        if (msgs[i].text != nullptr) {
            offs = (uint32_t)dat.size();
            size_t n = 0;
            pc_rando_msg_encode(msgs[i].text, nullptr, 0, &n);
            std::vector<uint8_t> bytes(n);
            RT_CHECK_EQ(pc_rando_msg_encode(msgs[i].text, bytes.data(), n, &n), PCR_MSG_OK);
            dat.insert(dat.end(), bytes.begin(), bytes.end());
        }
        put32(inf, e, offs);
        put16(inf, e + 4, msgs[i].no);
        inf[e + 0x0C] = msgs[i].box;
    }
    while (dat.size() % 4 != 0) {
        dat.push_back(0);
    }
    const std::vector<uint8_t> infSec = section("INF1", inf);
    const std::vector<uint8_t> datSec = section("DAT1", dat);
    const std::vector<uint8_t> extraSec = section("MID1", std::vector<uint8_t>(8, 0x55));
    std::vector<uint8_t> f(0x20, 0);
    std::memcpy(f.data(), "MESGbmg1", 8);
    const std::vector<const std::vector<uint8_t>*> order =
        datFirst ? std::vector<const std::vector<uint8_t>*>{&datSec, &infSec}
                 : std::vector<const std::vector<uint8_t>*>{&infSec, &datSec};
    uint32_t count = 0;
    if (extra) {
        f.insert(f.end(), extraSec.begin(), extraSec.end());
        count++;
    }
    for (const auto* s : order) {
        f.insert(f.end(), s->begin(), s->end());
        count++;
    }
    put32(f, 8, (uint32_t)f.size());
    put32(f, 0x0C, count);
    return f;
}

std::string decode(const pcr::BmgMessage& m) {
    char buf[512];
    const size_t n = pc_rando_msg_decode(m.text, m.text_max, buf, sizeof buf);
    return std::string(buf, n < sizeof buf ? n : sizeof buf - 1);
}

const char* kChart = "\\{1A 05 00 00 01}You got a \\{1A 06 FF 00 00 01}Treasure Chart\\{1A 06 FF 00 00 00}!";

} // namespace

int main() {
    const std::vector<Msg> msgs = {
        {101 + 0xC2, kChart, 9},
        {3957, "Line one\nLine two\nLine three", 0},
        {500, nullptr, 0},
        {500, "the second 500", 2},
    };

    // 1.
    {
        const std::vector<uint8_t> f = makeBmg(msgs);
        pcr::BmgMessage m;
        RT_CHECK(pcr::bmgFind(f.data(), f.size(), 101 + 0xC2, &m));
        RT_CHECK_EQ(decode(m), std::string(kChart));
        PcRandoMsgEntry e;
        pc_rando_msg_entry_unpack(m.entry, &e);
        RT_CHECK_EQ((int)e.text_box_type, 9);
        RT_CHECK(pcr::bmgFind(f.data(), f.size(), 3957, &m));
        RT_CHECK_EQ(decode(m), std::string("Line one\nLine two\nLine three"));
        RT_CHECK(m.text + m.text_max <= f.data() + f.size());
        // 2.
        RT_CHECK(pcr::bmgFind(f.data(), f.size(), 500, &m));
        RT_CHECK_EQ(decode(m), std::string("the second 500"));
        pc_rando_msg_entry_unpack(m.entry, &e);
        RT_CHECK_EQ((int)e.text_box_type, 2);
        // 4. a missing number
        RT_CHECK(!pcr::bmgFind(f.data(), f.size(), 501, &m));
        // a bad magic
        std::vector<uint8_t> bad = f;
        bad[3] = 'X';
        RT_CHECK(!pcr::bmgFind(bad.data(), bad.size(), 3957, &m));
        // truncated: the last section runs past the end
        RT_CHECK(!pcr::bmgFind(f.data(), f.size() - 4, 3957, &m));
        RT_CHECK(!pcr::bmgFind(f.data(), 0x10, 3957, &m));
        RT_CHECK(!pcr::bmgFind(nullptr, 0, 3957, &m));
        // an offset past DAT1
        bad = f;
        put32(bad, 0x20 + 0x10 + 0x18, 0x00100000); // the second entry's offset
        RT_CHECK(!pcr::bmgFind(bad.data(), bad.size(), 3957, &m));
        // an entry size under 0x18
        bad = f;
        put16(bad, 0x20 + 0x0A, 0x10);
        RT_CHECK(!pcr::bmgFind(bad.data(), bad.size(), 3957, &m));
    }

    // 3.
    {
        const std::vector<uint8_t> f = makeBmg(msgs, 0x1C, true, true);
        pcr::BmgMessage m;
        RT_CHECK(pcr::bmgFind(f.data(), f.size(), 3957, &m));
        RT_CHECK_EQ(decode(m), std::string("Line one\nLine two\nLine three"));
        RT_CHECK(pcr::bmgFind(f.data(), f.size(), 500, &m));
        RT_CHECK_EQ(decode(m), std::string("the second 500"));
    }
    return rt::finish();
}
