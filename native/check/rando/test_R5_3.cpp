// cos_rando_test_R5_3 (docs/RANDOMIZER_PLAN.md step R5.3): the pure part of the hint placement
// (features/rando/pc_rando_hint_place.h).
//   1. seaIslandMultPos: the 7x7 grid, bad numbers refused;
//   2. hohoYRot against Python's int(math.atan2(dx, dz) * (0x8000 / math.pi)) % 0x10000 on made-up
//      positions (values from python3; negative angles, the +-pi edge, truncation towards zero);
//   3. patchHohoRoom on a made-up room.dzr: every Ah of ACTR and the layer chunks with the room's
//      which_hoho is turned, an Ah with another which_hoho and other actors are left alone; a Ho Ho
//      with no look island, a room that is not a Ho Ho room and hints without Ho Ho hints write
//      nothing;
//   4. hintSeedOptions: the seed options and wwrando's defaults;
//   5. the fixture placements' hints parse (octo fairy hint, 10 Ho Ho look islands) and
//      R5_3_hints_game.txt has a section for each, with a row for each of the 13 Ah actors.
// No disc data (the in-game half is the hoho smoke).
#include "rando_test.h"

#include "pc_rando_dzx.h"
#include "pc_rando_gen_hints.h"
#include "pc_rando_hint_place.h"
#include "pc_rando_json.h"
#include "pc_rando_seedfile.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void putf(uint8_t* p, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    put32(p, v);
}

uint16_t be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

struct Actor {
    const char* name;
    uint32_t params;
    float x, z;
    uint16_t yRot;
};

struct Chunk {
    const char* tag;
    std::vector<Actor> actors;
};

Bytes makeDzx(const std::vector<Chunk>& chunks) {
    size_t total = 4 + 12 * chunks.size();
    for (const Chunk& c : chunks) {
        total += c.actors.size() * 0x20;
    }
    Bytes b(total, 0);
    put32(&b[0], (uint32_t)chunks.size());
    size_t off = 4 + 12 * chunks.size();
    for (size_t i = 0; i < chunks.size(); i++) {
        const Chunk& c = chunks[i];
        uint8_t* row = &b[4 + 12 * i];
        std::memcpy(row, c.tag, 4);
        put32(row + 4, (uint32_t)c.actors.size());
        put32(row + 8, (uint32_t)off);
        for (size_t k = 0; k < c.actors.size(); k++) {
            uint8_t* e = &b[off + k * 0x20];
            const Actor& a = c.actors[k];
            std::memcpy(e, a.name, std::strlen(a.name));
            put32(e + 0x08, a.params);
            putf(e + 0x0C, a.x);
            putf(e + 0x10, 10.0f);
            putf(e + 0x14, a.z);
            e[0x1A] = (uint8_t)(a.yRot >> 8);
            e[0x1B] = (uint8_t)a.yRot;
        }
        off += c.actors.size() * 0x20;
    }
    return b;
}

uint16_t yRotOf(Bytes& b, const char* tag, uint32_t index) {
    PcRandoDzx dzx;
    if (pc_rando_dzx_open(&dzx, b.data(), b.size()) != PCR_DZX_OK) {
        return 0;
    }
    const uint8_t* e = pc_rando_dzx_entry(&dzx, tag, index);
    return e != nullptr ? be16(e + 0x1A) : 0;
}

pcr::HintsResult hohoHints(int index, int look) {
    pcr::HintsResult h;
    h.hoho.assign(pcr::kHohoCount, std::vector<pcr::Hint>(1));
    h.hohoLookIsland.assign(pcr::kHohoCount, -1);
    h.hohoLookIsland[index] = look;
    return h;
}

} // namespace

int main() {
    // 1. MULT grid
    float x = 0, z = 0;
    RT_CHECK(pcr::seaIslandMultPos(1, &x, &z) && x == -300000.0f && z == -300000.0f);
    RT_CHECK(pcr::seaIslandMultPos(44, &x, &z) && x == -200000.0f && z == 300000.0f);
    RT_CHECK(pcr::seaIslandMultPos(25, &x, &z) && x == 0.0f && z == 0.0f);
    RT_CHECK(pcr::seaIslandMultPos(49, &x, &z) && x == 300000.0f && z == 300000.0f);
    for (int n = 1; n <= 49; n++) {
        RT_CHECK(pcr::seaIslandMultPos(n, &x, &z) && x == ((n - 1) % 7 - 3) * 100000.0f &&
                 z == ((n - 1) / 7 - 3) * 100000.0f);
    }
    RT_CHECK(!pcr::seaIslandMultPos(0, &x, &z));
    RT_CHECK(!pcr::seaIslandMultPos(50, &x, &z));

    // 2. the angle (python3: int(math.atan2(mx - x, mz - z) * (0x8000 / math.pi)) % 0x10000)
    struct Case {
        float x, z;
        int island;
        int want;
    };
    const Case cases[] = {
        {0.0f, 0.0f, 1, 40960},          {0.0f, 0.0f, 49, 8192},          {123.5f, -4567.25f, 44, 59473},
        {-250000.0f, 280000.0f, 44, 12415}, {310000.0f, -310000.0f, 7, 57344}, {5.0f, 5.0f, 25, 40960},
        {-99999.5f, 1.0f, 24, 37605},    {0.0f, -299990.0f, 4, 32768},    {1000.0f, 2000.0f, 46, 65501},
    };
    for (const Case& c : cases) {
        uint16_t y = 0;
        RT_CHECK(pcr::hohoYRot(c.x, c.z, c.island, &y));
        RT_CHECK_EQ((int)y, c.want);
    }
    uint16_t y = 0;
    RT_CHECK(!pcr::hohoYRot(0, 0, 0, &y));

    // 3. the room patcher (room 44 is Ho Ho index 2; room 14 index 1)
    RT_CHECK_EQ(pcr::hohoIndexOfRoom(44), 2);
    RT_CHECK_EQ(pcr::hohoIndexOfRoom(46), 9);
    RT_CHECK_EQ(pcr::hohoIndexOfRoom(45), -1);
    {
        Bytes b = makeDzx({
            {"ACTR", {{"Ah", 0xFF02u, 123.5f, -4567.25f, 0x1111}, {"Bk", 0xFF02u, 0, 0, 0x2222}}},
            {"ACT4", {{"kanban", 0, 0, 0, 0x3333}, {"Ah", 0xFF02u, -250000.0f, 280000.0f, 0xB1C8}}},
            {"ACT7", {{"Ah", 0xFF05u, 0, 0, 0x4444}}},
        });
        PcRandoDzx dzx;
        RT_CHECK_EQ(pc_rando_dzx_open(&dzx, b.data(), b.size()), (int)PCR_DZX_OK);
        pcr::HohoPatchResult r;
        RT_CHECK(pcr::patchHohoRoom(&dzx, 44, hohoHints(2, 44), &r));
        RT_CHECK_EQ(r.found, 3);
        RT_CHECK_EQ(r.turned, 2);
        RT_CHECK_EQ(r.mismatch, 1);
        RT_CHECK_EQ(yRotOf(b, "ACTR", 0), (uint16_t)59473);
        RT_CHECK_EQ(yRotOf(b, "ACTR", 1), (uint16_t)0x2222);
        RT_CHECK_EQ(yRotOf(b, "ACT4", 0), (uint16_t)0x3333);
        RT_CHECK_EQ(yRotOf(b, "ACT4", 1), (uint16_t)12415);
        RT_CHECK_EQ(yRotOf(b, "ACT7", 0), (uint16_t)0x4444);

        const Bytes before = b;
        RT_CHECK(!pcr::patchHohoRoom(&dzx, 44, hohoHints(2, -1), &r)); // no look island: vanilla
        RT_CHECK_EQ(r.turned, 0);
        RT_CHECK(!pcr::patchHohoRoom(&dzx, 45, hohoHints(2, 44), &r)); // not a Ho Ho room
        RT_CHECK(!pcr::patchHohoRoom(&dzx, 14, hohoHints(2, 44), &r)); // Ho Ho 1 keeps his rotation
        pcr::HintsResult none;
        RT_CHECK(!pcr::patchHohoRoom(&dzx, 44, none, &r)); // no Ho Ho hints
        RT_CHECK(b == before);
    }

    // 4. options
    {
        pcr::Json o = pcr::Json::object();
        pcr::HintSeedOptions d = pcr::hintSeedOptions(o);
        RT_CHECK(d.cryptic && !d.importance && !d.keylunacy);
        o.set("cryptic_hints", pcr::Json::boolean(false));
        o.set("hint_importance", pcr::Json::boolean(true));
        o.set("keylunacy", pcr::Json::boolean(true));
        d = pcr::hintSeedOptions(o);
        RT_CHECK(!d.cryptic && d.importance && d.keylunacy);
    }

    // 5. the fixture placements against the fixture's R lines
    std::string fixture;
    RT_CHECK(rt::readFixture("R5_3_hints_game.txt", fixture));
    int sections = 0;
    for (const char* name : {"R5_3_TestSeed1.json", "R5_3_Heavy5.json"}) {
        std::string text, err;
        RT_CHECK(rt::readFixture(name, text));
        PcRandoSeed seed;
        if (!RT_CHECK(pc_rando_seed_parse(text, &seed, &err)) || !RT_CHECK(seed.has_hints)) {
            continue;
        }
        pcr::HintsResult h;
        if (!RT_CHECK(pcr::hintsFromJson(seed.hints, h, &err))) {
            continue;
        }
        RT_CHECK(h.hasOctoFairy);
        RT_CHECK_EQ((int)h.hohoLookIsland.size(), pcr::kHohoCount);
        // The section lists every Ah of the 10 Ho Ho rooms (their positions are disc data: the smoke
        // compares the y_rot values).
        const std::string head = std::string("P ") + name + "\n";
        const size_t at = fixture.find(head);
        if (!RT_CHECK(at != std::string::npos)) {
            continue;
        }
        sections++;
        size_t end = fixture.find("\nP ", at + head.size());
        const std::string sec = fixture.substr(at, end == std::string::npos ? std::string::npos : end - at);
        int rows = 0;
        for (int i = 0; i < pcr::kHohoCount; i++) {
            const std::string room = "\nR " + std::to_string(pcr::kHohoIndexToIsland[i]) + " ";
            for (size_t p = sec.find(room); p != std::string::npos; p = sec.find(room, p + 1)) {
                rows++;
            }
        }
        RT_CHECK_EQ(rows, 13); // 9 rooms with one Ah, room 44 with four
    }
    RT_CHECK_EQ(sections, 2);
    return rt::finish();
}
