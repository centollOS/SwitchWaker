// R3.2 (docs/RANDOMIZER_PLAN.md): the pure part of health, magic, clothes and the start spawn
// (features/rando/pc_rando_start.h), on synthetic data (no disc bytes):
//  1. the HP rules of set_starting_health, the file-select patches, the load cap and the game-over
//     reset, as the asm computes them;
//  2. SHIP lookup by ship_id in a room.dzr, and the sea stage.dzs patch of the "Ship" ACTR
//     (change_ship_starting_island then prevent_fire_mountain_lava_softlock), other ACTRs untouched;
//  3. the Yaz0 expander (literals and back-references, both length encodings) and the RARC lookup.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_dzx.h"
#include "pc_rando_start.h"

#include <cstring>
#include <vector>

namespace {

void put32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    b[at] = (uint8_t)(v >> 24);
    b[at + 1] = (uint8_t)(v >> 16);
    b[at + 2] = (uint8_t)(v >> 8);
    b[at + 3] = (uint8_t)v;
}
void put16(std::vector<uint8_t>& b, size_t at, uint16_t v) {
    b[at] = (uint8_t)(v >> 8);
    b[at + 1] = (uint8_t)v;
}
void putf(std::vector<uint8_t>& b, size_t at, float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    put32(b, at, u);
}

// A dzx with one chunk per (tag, count, entry size), entries zeroed.
struct Chunk {
    const char* tag;
    uint32_t count;
    uint32_t size;
};
std::vector<uint8_t> makeDzx(const Chunk* c, int n, std::vector<size_t>* starts) {
    size_t off = 4 + 12 * (size_t)n;
    std::vector<uint8_t> b(off, 0);
    put32(b, 0, (uint32_t)n);
    for (int i = 0; i < n; i++) {
        memcpy(&b[4 + 12 * i], c[i].tag, 4);
        put32(b, 4 + 12 * i + 4, c[i].count);
        put32(b, 4 + 12 * i + 8, (uint32_t)off);
        starts->push_back(off);
        off += (size_t)c[i].count * c[i].size;
        b.resize(off, 0);
    }
    return b;
}

void testHealth() {
    RT_CHECK_EQ(pc_rando_start_life(12), 12);
    RT_CHECK_EQ(pc_rando_start_life(13), 12); // 3 hearts and a piece: start with 3 full hearts
    RT_CHECK_EQ(pc_rando_start_life(7), 4);
    RT_CHECK_EQ(pc_rando_start_life(80), 80);
    RT_CHECK_EQ(pc_rando_start_blank_slot_life(15), 12);
    RT_CHECK_EQ(pc_rando_start_loaded_life(13, 13), 12); // capped at whole hearts
    RT_CHECK_EQ(pc_rando_start_loaded_life(24, 9), 9);   // never raised
    RT_CHECK_EQ(pc_rando_start_loaded_life(4, 12), 4);
    RT_CHECK_EQ(pc_rando_start_gameover_life(80), 12);
    RT_CHECK_EQ(pc_rando_start_gameover_life(12), 12);
    RT_CHECK_EQ(pc_rando_start_gameover_life(11), 8);
    RT_CHECK_EQ(pc_rando_start_gameover_life(4), 4);
    PcRandoConfig cfg;
    pc_rando_config_defaults(&cfg);
    RT_CHECK_EQ(cfg.starting_quarter_hearts, 12); // the default preset starts with 3 hearts
    RT_CHECK_EQ(cfg.heros_clothes, 1);
}

void testShip() {
    // room.dzr: SHIP 3 entries, ids 2, 0, 0 (the first id-0 one wins)
    const Chunk room[] = {{"SHIP", 3, 0x10}};
    std::vector<size_t> rs;
    std::vector<uint8_t> dzr = makeDzx(room, 1, &rs);
    for (int i = 0; i < 3; i++) {
        const size_t e = rs[0] + 0x10 * i;
        putf(dzr, e, 100.0f * (i + 1));
        putf(dzr, e + 4, 5.0f);
        putf(dzr, e + 8, -200.0f * (i + 1));
        put16(dzr, e + 0x0C, (uint16_t)(0x1000 * (i + 1)));
        dzr[e + 0x0E] = i == 0 ? 2 : 0;
    }
    PcRandoShipSpawn sp;
    RT_CHECK(pc_rando_start_find_ship_spawn(dzr.data(), dzr.size(), 0, &sp) == 1);
    RT_CHECK_EQ(sp.x, 200.0f);
    RT_CHECK_EQ(sp.y, 5.0f);
    RT_CHECK_EQ(sp.z, -400.0f);
    RT_CHECK_EQ(sp.yRot, 0x2000);
    RT_CHECK_EQ(sp.shipId, 0);
    RT_CHECK(pc_rando_start_find_ship_spawn(dzr.data(), dzr.size(), 7, &sp) == 0);
    RT_CHECK(pc_rando_start_find_ship_spawn(dzr.data(), 3, 0, &sp) == 0);

    // stage.dzs: ACTR "Ikada", "Ship", "Ship2"
    const Chunk stage[] = {{"ACTR", 3, 0x20}};
    std::vector<size_t> ss;
    std::vector<uint8_t> dzs = makeDzx(stage, 1, &ss);
    const char* names[] = {"Ikada", "Ship", "Ship2"};
    for (int i = 0; i < 3; i++) {
        const size_t e = ss[0] + 0x20 * i;
        memcpy(&dzs[e], names[i], strlen(names[i]));
        put32(dzs, e + 8, 0xABCD0000u + i);
        putf(dzs, e + 0x0C, 1.0f);
        putf(dzs, e + 0x10, 2.0f);
        putf(dzs, e + 0x14, 3.0f);
        put16(dzs, e + 0x18, 0x1111);
        put16(dzs, e + 0x1A, 0x2222);
        put16(dzs, e + 0x1C, 0x3333);
    }
    const std::vector<uint8_t> before = dzs;
    PcRandoDzx dzx;
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, dzs.data(), dzs.size()), PCR_DZX_OK);
    PcRandoShipSpawn island = {200.0f, 5.0f, -400.0f, 0x2000, 0};
    uint32_t idx = 99;
    RT_CHECK_EQ(pc_rando_start_patch_sea_stage(&dzx, &island, &idx), PCR_DZX_OK);
    RT_CHECK_EQ(idx, 1u);
    const uint8_t* ship = &dzs[ss[0] + 0x20];
    RT_CHECK_EQ(pc_rando_dzx_bef32(ship + 0x0C), 200.0f);
    RT_CHECK_EQ(pc_rando_dzx_bef32(ship + 0x10), PCR_START_SHIP_Y);
    RT_CHECK_EQ(pc_rando_dzx_bef32(ship + 0x14), -400.0f);
    RT_CHECK_EQ(pc_rando_dzx_be16(ship + 0x18), 0x1111); // x_rot kept
    RT_CHECK_EQ(pc_rando_dzx_be16(ship + 0x1A), 0x2000);
    RT_CHECK_EQ(pc_rando_dzx_be16(ship + 0x1C), 0x3333);
    RT_CHECK_EQ(pc_rando_dzx_be32(ship + 8), 0xABCD0001u); // params kept
    // only the Ship's x, y, z and y_rot bytes changed
    size_t diffs = 0;
    for (size_t i = 0; i < dzs.size(); i++) {
        if (dzs[i] != before[i]) {
            const size_t rel = i - (ss[0] + 0x20);
            RT_CHECK(i >= ss[0] + 0x20 && rel >= 0x0C && rel < 0x1C && !(rel >= 0x18 && rel < 0x1A));
            diffs++;
        }
    }
    RT_CHECK(diffs > 0);

    // no spawn: x/z/y_rot stay, y still moves
    std::vector<uint8_t> dzs2 = before;
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, dzs2.data(), dzs2.size()), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_start_patch_sea_stage(&dzx, nullptr, nullptr), PCR_DZX_OK);
    ship = &dzs2[ss[0] + 0x20];
    RT_CHECK_EQ(pc_rando_dzx_bef32(ship + 0x0C), 1.0f);
    RT_CHECK_EQ(pc_rando_dzx_bef32(ship + 0x10), PCR_START_SHIP_Y);
    RT_CHECK_EQ(pc_rando_dzx_be16(ship + 0x1A), 0x2222);

    // no Ship ACTR
    std::vector<uint8_t> dzs3 = before;
    memset(&dzs3[ss[0] + 0x20], 0, 8);
    RT_CHECK_EQ(pc_rando_dzx_open(&dzx, dzs3.data(), dzs3.size()), PCR_DZX_OK);
    RT_CHECK_EQ(pc_rando_start_patch_sea_stage(&dzx, &island, nullptr), PCR_DZX_NO_CHUNK);
}

void testYaz0Rarc() {
    // "abcabcabcX" + 0x20 'z': literals a b c, back-ref (dist 3, len 6: short form), literal X,
    // literal z, back-ref (dist 1, len 31: long form, 31 - 0x12 = 13)
    const char* want = "abcabcabcXzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    const uint32_t n = (uint32_t)strlen(want);
    std::vector<uint8_t> y = {'Y', 'a', 'z', '0', 0, 0, 0, (uint8_t)n, 0, 0, 0, 0, 0, 0, 0, 0};
    // group 1: L L L B L L B (7 ops) -> bits 1110 1100 = 0xEC (last bit unused)
    y.push_back(0xEC);
    y.push_back('a');
    y.push_back('b');
    y.push_back('c');
    y.push_back((uint8_t)((6 - 2) << 4)); // len 6, dist-1 = 2
    y.push_back(2);
    y.push_back('X');
    y.push_back('z');
    y.push_back(0x00); // long form, dist-1 = 0
    y.push_back(0x00);
    y.push_back((uint8_t)(31 - 0x12));
    RT_CHECK_EQ(pc_rando_start_yaz0_size(y.data(), y.size()), n);
    std::vector<uint8_t> out(n);
    RT_CHECK(pc_rando_start_yaz0_expand(y.data(), y.size(), out.data(), out.size()) == 1);
    RT_CHECK(memcmp(out.data(), want, n) == 0);
    RT_CHECK(pc_rando_start_yaz0_expand(y.data(), y.size() - 1, out.data(), out.size()) == 0); // truncated
    RT_CHECK(pc_rando_start_yaz0_expand(y.data(), y.size(), out.data(), n - 1) == 0);          // small dst
    RT_CHECK_EQ(pc_rando_start_yaz0_size("RARC", 4), 0u);

    // RARC: header 0x20, info 0x20, 2 nodes (unused here), entries: dir "dzr", file "x.bin", file
    // "room.dzr", then strings and data.
    std::vector<uint8_t> a(0x200, 0);
    memcpy(&a[0], "RARC", 4);
    put32(a, 0x04, (uint32_t)a.size());
    put32(a, 0x08, 0x20);   // header length
    put32(a, 0x0C, 0x140);  // file data offset (from 0x20)
    const size_t info = 0x20;
    put32(a, info + 0x00, 1);     // nodes
    put32(a, info + 0x04, 0x20);  // node offset
    put32(a, info + 0x08, 3);     // entries
    put32(a, info + 0x0C, 0x40);  // entry offset
    put32(a, info + 0x10, 0x40);  // string table size
    put32(a, info + 0x14, 0x80);  // string offset
    const char strings[] = "dzr\0x.bin\0room.dzr\0";
    memcpy(&a[info + 0x80], strings, sizeof strings);
    auto entry = [&](int k, uint8_t flags, uint32_t nameOff, uint32_t off, uint32_t size) {
        const size_t e = info + 0x40 + 0x14 * k;
        put32(a, e + 4, ((uint32_t)flags << 24) | nameOff);
        put32(a, e + 8, off);
        put32(a, e + 0x0C, size);
    };
    entry(0, 0x02, 0, 1, 0x10);   // directory named like nothing we want
    entry(1, 0x11, 4, 0x00, 4);   // x.bin
    entry(2, 0x11, 10, 0x20, 6);  // room.dzr
    memcpy(&a[0x20 + 0x140 + 0x00], "XBIN", 4);
    memcpy(&a[0x20 + 0x140 + 0x20], "ROOMDZ", 6);
    const uint8_t* d = nullptr;
    uint32_t sz = 0;
    RT_CHECK(pc_rando_start_rarc_find(a.data(), a.size(), "room.dzr", &d, &sz) == 1);
    RT_CHECK_EQ(sz, 6u);
    RT_CHECK(d != nullptr && memcmp(d, "ROOMDZ", 6) == 0);
    RT_CHECK(pc_rando_start_rarc_find(a.data(), a.size(), "x.bin", &d, &sz) == 1 && memcmp(d, "XBIN", 4) == 0);
    RT_CHECK(pc_rando_start_rarc_find(a.data(), a.size(), "dzr", &d, &sz) == 0); // directories are skipped
    RT_CHECK(pc_rando_start_rarc_find(a.data(), a.size(), "room.dz", &d, &sz) == 0);
    entry(2, 0x11, 10, 0x1F0, 6); // data out of the file
    RT_CHECK(pc_rando_start_rarc_find(a.data(), a.size(), "room.dzr", &d, &sz) == 0);
}

} // namespace

int main() {
    testHealth();
    testShip();
    testYaz0Rarc();
    return rt::finish();
}
