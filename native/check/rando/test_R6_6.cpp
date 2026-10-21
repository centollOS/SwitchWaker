// cos_rando_test_R6_6 (docs/RANDOMIZER_PLAN.md step R6.6): randomized charts, without the game:
//   - the generator against wwrando: seeds ChartsA/B/C with randomize_charts=true give the charts of
//     fixtures/R6_6_seed_{A,B,C}.json (ww_export.py ChartsA <out> randomize_charts=true); the logic's
//     "Chart for Island N" macros follow them (the spoiler's Charts section is golden in
//     check_seeds.py, regress line R6.6);
//   - chart names: number and owned index (0xFF - item id: item_func_collectmap<k> is item 0xFF - k);
//   - ChartRandomizer._save on a synthetic CmapDat.bin (the 49 charts at their vanilla islands plus
//     non-randomizable records): each chart record gets the texture id, sector and 4 positions of its
//     island's vanilla chart, keeps its owned index, number and type; other records stay; a table
//     that is not the vanilla one (D9) or a seed whose charts repeat is left alone;
//   - the game hook: patches once, a second call on the same buffer changes nothing, nothing at all
//     with pc_rando() 0;
//   - the SCOB patcher on a synthetic sea/Room44 room.dzr: every d_a_salvage salvage_type 0 entry in
//     every SCOB chunk gets the new chart index, nothing else changes; an entry with another index is
//     left (D9); other stages are not touched.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc/pc_rando.h"
#include "pc_rando_charts.h"
#include "pc_rando_data.h"
#include "pc_rando_dzx.h"
#include "pc_rando_gen.h"
#include "pc_rando_options.h"
#include "pc_rando_resources.h"
#include "pc_rando_seedfile.h"

#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

bool loadSeed(const char* file, PcRandoSeed* seed) {
    std::string text;
    std::string err;
    if (!RT_CHECK(rt::readFixture(file, text))) {
        return false;
    }
    if (!RT_CHECK(pc_rando_seed_parse(text, seed, &err))) {
        std::fprintf(stderr, "  %s: %s\n", file, err.c_str());
        return false;
    }
    return true;
}

std::vector<std::string> islandCharts(const PcRandoSeed& seed) {
    std::vector<std::string> out(50);
    for (const auto& c : seed.charts) {
        if (c.first >= 1 && c.first <= 49) {
            out[c.first] = c.second;
        }
    }
    return out;
}

void checkNames() {
    RT_CHECK_EQ(pc_rando_chart_number("Triforce Chart 1"), 1);
    RT_CHECK_EQ(pc_rando_chart_number("Triforce Chart 8"), 8);
    RT_CHECK_EQ(pc_rando_chart_number("Treasure Chart 1"), 9);
    RT_CHECK_EQ(pc_rando_chart_number("Treasure Chart 41"), 49);
    RT_CHECK_EQ(pc_rando_chart_number("Treasure Chart 42"), -1);
    RT_CHECK_EQ(pc_rando_chart_number("Triforce Chart 9"), -1);
    RT_CHECK_EQ(pc_rando_chart_number("Triforce Chart 1 got deciphered"), -1);
    RT_CHECK_EQ(pc_rando_chart_number("Ghost Ship Chart"), -1);
    RT_CHECK_EQ(pc_rando_chart_number(nullptr), -1);
    // item_names.txt: Triforce Chart 1 = 0xFE, Treasure Chart 25 = 0xD6, Treasure Chart 33 = 0xEF.
    RT_CHECK_EQ(pc_rando_chart_owned_index("Triforce Chart 1"), 1);
    RT_CHECK_EQ(pc_rando_chart_owned_index("Treasure Chart 25"), 41);
    RT_CHECK_EQ(pc_rando_chart_owned_index("Treasure Chart 33"), 16);
    RT_CHECK_EQ(pc_rando_chart_owned_index("Ghost Ship Chart"), -1);
    bool seen[256] = {};
    for (int n = 1; n <= 49; n++) {
        const std::string name = n <= 8 ? "Triforce Chart " + std::to_string(n) : "Treasure Chart " + std::to_string(n - 8);
        const int own = pc_rando_chart_owned_index(name.c_str());
        RT_CHECK(own >= 1 && own <= 64 && !seen[own]);
        if (own >= 0 && own < 256) {
            seen[own] = true;
        }
    }
    RT_CHECK_EQ(pc_rando_chart_island_of_sector(-3, -3), 1);
    RT_CHECK_EQ(pc_rando_chart_island_of_sector(-2, 3), 44);
    RT_CHECK_EQ(pc_rando_chart_island_of_sector(3, 3), 49);
}

void checkGenerator(const char* fixture) {
    PcRandoSeed want;
    if (!loadSeed(fixture, &want)) {
        return;
    }
    pcr::GenRequest req;
    req.seed = want.seed;
    req.allowMissingHints = true;
    req.version = "test";
    pcr::optionSetInt(req.options, pcr::OPT_randomize_charts, 1);
    pcr::GenResult res;
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  %s: %s\n", fixture, res.error.c_str());
        return;
    }
    RT_CHECK_EQ(res.seed.charts.size(), (size_t)49);
    RT_CHECK(res.seed.charts == want.charts);
    int moved = 0;
    for (const auto& c : res.seed.charts) {
        moved += c.second != pcr::default_chart_for_island(c.first) ? 1 : 0;
    }
    RT_CHECK(moved > 30);
    // The placement is wwrando's too (randomize_charts moves the sunken treasure requirements).
    RT_CHECK(memcmp(res.seed.locations, want.locations, sizeof want.locations) == 0);
}

// A synthetic CmapDat.bin: num, then the 49 charts at their vanilla islands (texture id = island,
// sector of the island, positions derived from the island), and 4 records of other types.
struct Cmap {
    Bytes data;
    int numRecords = 0;
};

Cmap makeCmap() {
    Cmap c;
    c.numRecords = 49 + 4;
    c.data.assign(4 + c.numRecords * PCR_CMAP_RECORD_SIZE, 0);
    put32(c.data.data(), (uint32_t)c.numRecords);
    int r = 0;
    auto rec = [&](int i) { return c.data.data() + 4 + i * PCR_CMAP_RECORD_SIZE; };
    // Two other records first, so the charts are not at their island's index.
    for (int k = 0; k < 2; k++, r++) {
        uint8_t* p = rec(r);
        p[0] = (uint8_t)(50 + k);
        p[1] = (uint8_t)(50 + k);
        p[2] = (uint8_t)(50 + k);
        p[3] = (uint8_t)(k == 0 ? 4 : 3);
        for (int b = 6; b < PCR_CMAP_RECORD_SIZE; b++) {
            p[b] = (uint8_t)(0xA0 + k);
        }
    }
    for (int island = 49; island >= 1; island--, r++) {
        const char* name = pcr::default_chart_for_island(island);
        uint8_t* p = rec(r);
        p[0] = (uint8_t)island;
        p[1] = (uint8_t)pc_rando_chart_owned_index(name);
        p[2] = (uint8_t)pc_rando_chart_number(name);
        p[3] = (uint8_t)(p[2] <= 8 ? 0 : island % 3 == 0 ? 2 : island % 7 == 0 ? 6 : 1);
        p[4] = (uint8_t)(int8_t)((island - 1) % 7 - 3);
        p[5] = (uint8_t)(int8_t)((island - 1) / 7 - 3);
        for (int b = 6; b < PCR_CMAP_RECORD_SIZE; b++) {
            p[b] = (uint8_t)(island * 5 + b);
        }
    }
    for (int k = 2; k < 4; k++, r++) {
        uint8_t* p = rec(r);
        p[0] = (uint8_t)(50 + k);
        p[1] = (uint8_t)(50 + k);
        p[2] = (uint8_t)(50 + k);
        p[3] = (uint8_t)(7 + k);
    }
    return c;
}

const uint8_t* findNumber(const Bytes& d, int number) {
    const uint32_t num = rd32(d.data());
    for (uint32_t r = 0; r < num; r++) {
        const uint8_t* p = d.data() + 4 + r * PCR_CMAP_RECORD_SIZE;
        if (p[2] == number) {
            return p;
        }
    }
    return nullptr;
}

void checkCmap(const PcRandoSeed& seed) {
    const std::vector<std::string> charts = islandCharts(seed);
    const char* names[50] = {};
    for (int i = 1; i <= 49; i++) {
        names[i] = charts[i].c_str();
    }
    const Cmap vanilla = makeCmap();
    Cmap c = makeCmap();
    RT_CHECK_EQ(pc_rando_charts_apply_cmapdat(c.data.data(), c.data.size(), names), PCR_CHARTS_PATCHED);
    for (int island = 1; island <= 49; island++) {
        const int number = pc_rando_chart_number(names[island]);
        const uint8_t* now = findNumber(c.data, number);
        const uint8_t* was = findNumber(vanilla.data, number);
        // The vanilla record of the island's own chart.
        const uint8_t* from = findNumber(vanilla.data, pc_rando_chart_number(pcr::default_chart_for_island(island)));
        if (!RT_CHECK(now != nullptr && was != nullptr && from != nullptr)) {
            continue;
        }
        RT_CHECK_EQ(now[0], (uint8_t)island);
        RT_CHECK_EQ(now[1], was[1]);
        RT_CHECK_EQ(now[2], was[2]);
        RT_CHECK_EQ(now[3], was[3]);
        RT_CHECK_EQ(pc_rando_chart_island_of_sector((int8_t)now[4], (int8_t)now[5]), island);
        RT_CHECK(memcmp(now + 6, from + 6, PCR_CMAP_RECORD_SIZE - 6) == 0);
    }
    for (int r : {0, 1, 51, 52}) {
        RT_CHECK(memcmp(c.data.data() + 4 + r * PCR_CMAP_RECORD_SIZE, vanilla.data.data() + 4 + r * PCR_CMAP_RECORD_SIZE,
                        PCR_CMAP_RECORD_SIZE) == 0);
    }

    // The identity (charts not randomized) changes nothing.
    {
        const char* id[50] = {};
        for (int i = 1; i <= 49; i++) {
            id[i] = pcr::default_chart_for_island(i);
        }
        Cmap d = makeCmap();
        RT_CHECK_EQ(pc_rando_charts_apply_cmapdat(d.data.data(), d.data.size(), id), PCR_CHARTS_PATCHED);
        RT_CHECK(d.data == vanilla.data);
    }
    // D9: an owned index that is not the item's, a chart missing, a short buffer.
    {
        Cmap d = makeCmap();
        d.data[4 + 10 * PCR_CMAP_RECORD_SIZE + 1] ^= 0x40;
        const Bytes before = d.data;
        RT_CHECK_EQ(pc_rando_charts_apply_cmapdat(d.data.data(), d.data.size(), names), PCR_CHARTS_MISMATCH);
        RT_CHECK(d.data == before);
        Cmap e = makeCmap();
        e.data[4 + 20 * PCR_CMAP_RECORD_SIZE + 3] = 4; // no longer a randomizable type
        RT_CHECK_EQ(pc_rando_charts_apply_cmapdat(e.data.data(), e.data.size(), names), PCR_CHARTS_MISMATCH);
        Cmap f = makeCmap();
        RT_CHECK_EQ(pc_rando_charts_apply_cmapdat(f.data.data(), f.data.size() - 1, names), PCR_CHARTS_MISMATCH);
    }
    // A seed whose charts repeat.
    {
        const char* bad[50];
        memcpy(bad, names, sizeof bad);
        bad[2] = bad[1];
        Cmap d = makeCmap();
        RT_CHECK_EQ(pc_rando_charts_apply_cmapdat(d.data.data(), d.data.size(), bad), PCR_CHARTS_BAD_SEED);
        RT_CHECK(d.data == vanilla.data);
    }

    // The game hook.
    pc_rando_charts_reset();
    {
        Cmap d = makeCmap();
        pc_rando_charts_cmapdat_loaded(d.data.data(), d.data.size());
        RT_CHECK_EQ(pc_rando_charts_cmapdat_status(), PCR_CHARTS_NOT_RUN); // pc_rando() is 0
        RT_CHECK(d.data == vanilla.data);
    }
    pc_rando_set_active(&seed, nullptr);
    {
        Cmap d = makeCmap();
        pc_rando_charts_cmapdat_loaded(d.data.data(), d.data.size());
        RT_CHECK_EQ(pc_rando_charts_cmapdat_status(), PCR_CHARTS_PATCHED);
        RT_CHECK(d.data == c.data);
        RT_CHECK(pc_rando_charts_cmapdat_buffer() == d.data.data());
        const uint8_t* v44 = pc_rando_charts_vanilla_record(44);
        RT_CHECK(v44 != nullptr && memcmp(v44, findNumber(vanilla.data, 4), PCR_CMAP_RECORD_SIZE) == 0);
        pc_rando_charts_cmapdat_loaded(d.data.data(), d.data.size());
        RT_CHECK_EQ(pc_rando_charts_cmapdat_status(), PCR_CHARTS_ALREADY);
        RT_CHECK(d.data == c.data);
        // A fresh read into the same buffer (the menu freed and reloaded it) is patched again.
        d.data = vanilla.data;
        pc_rando_charts_cmapdat_loaded(d.data.data(), d.data.size());
        RT_CHECK_EQ(pc_rando_charts_cmapdat_status(), PCR_CHARTS_PATCHED);
        RT_CHECK(d.data == c.data);
    }
    pc_rando_deactivate();
}

// A room.dzr with a SCOB chunk and an SCO0 chunk (0x24-byte entries).
struct Ent {
    const char* name;
    uint32_t params;
};

Bytes makeDzr(const std::vector<Ent>& scob, const std::vector<Ent>& sco0) {
    const uint32_t es = pc_rando_dzx_entry_size("SCOB");
    Bytes d(4 + 2 * 12 + (scob.size() + sco0.size()) * es, 0);
    put32(d.data(), 2);
    uint32_t off = 4 + 2 * 12;
    auto chunk = [&](int i, const char* tag, const std::vector<Ent>& ents) {
        uint8_t* row = d.data() + 4 + i * 12;
        memcpy(row, tag, 4);
        put32(row + 4, (uint32_t)ents.size());
        put32(row + 8, off);
        for (const Ent& e : ents) {
            uint8_t* p = d.data() + off;
            strncpy((char*)p, e.name, 8);
            put32(p + 8, e.params);
            for (uint32_t b = 12; b < es; b++) {
                p[b] = (uint8_t)(b * 3);
            }
            off += es;
        }
    };
    chunk(0, "SCOB", scob);
    chunk(1, "SCO0", sco0);
    return d;
}

uint32_t salvage(uint32_t type, uint32_t index, uint32_t item) {
    return (type << 28) | (index << 20) | (item << 4) | 0x3;
}

void checkScob(const PcRandoSeed& seed) {
    const std::vector<std::string> charts = islandCharts(seed);
    const int want = pc_rando_chart_owned_index(charts[44].c_str());
    const int vanilla = pc_rando_chart_owned_index("Triforce Chart 4");
    RT_CHECK_EQ(vanilla, 4);
    RT_CHECK(want > 0 && want != vanilla);
    const std::vector<Ent> scob = {
        {"Salvage", salvage(0, vanilla, 0x62)}, {"TagKb", salvage(0, vanilla, 0x01)},
        {"Salvage", salvage(0, vanilla, 0x62)}, {"Salvag2", salvage(2, vanilla, 0x05)},
        {"Salvage", salvage(0, 9, 0x62)},       {"Salvage", salvage(0, vanilla, 0x62)},
    };
    const std::vector<Ent> sco0 = {{"Salvage", salvage(0, vanilla, 0x62)}};
    const Bytes orig = makeDzr(scob, sco0);
    Bytes want_ = orig;
    const uint32_t es = pc_rando_dzx_entry_size("SCOB");
    for (int i : {0, 2, 5, 6}) {
        uint8_t* p = want_.data() + 4 + 2 * 12 + i * es;
        put32(p + 8, (rd32(p + 8) & ~0x0FF00000u) | ((uint32_t)want << 20));
    }

    auto run = [&](Bytes& d, const char* stage, const char* arc, int room) {
        PcRandoResFile f = {};
        f.dir = "";
        f.stage = stage;
        f.arc = arc;
        f.room = room;
        f.name = "room.dzr";
        f.type = PCR_RES_DZR;
        f.data = d.data();
        f.size = d.size();
        pc_rando_charts_scob_patch(&f);
    };

    pc_rando_charts_reset();
    Bytes d = orig;
    run(d, "sea", "Room44", 44); // pc_rando() 0
    RT_CHECK(d == orig);
    RT_CHECK_EQ(pc_rando_charts_room_status(44), PCR_CHARTS_NOT_RUN);

    pc_rando_set_active(&seed, nullptr);
    run(d, "sea", "Room44", 44);
    RT_CHECK(d == want_);
    RT_CHECK_EQ(pc_rando_charts_room_written(44), 4);
    RT_CHECK_EQ(pc_rando_charts_room_index(44), want);
    RT_CHECK_EQ(pc_rando_charts_room_status(44), PCR_CHARTS_MISMATCH); // the entry with index 9

    Bytes e = orig;
    run(e, "Asoko", "Room44", 44);
    run(e, "sea", "Stage", -1);
    RT_CHECK(e == orig);

    // Through the dispatcher's registration (pc_rando_patchers.def).
    pc_rando_charts_init();
    pc_rando_deactivate();
}

} // namespace

int main() {
    checkNames();
    checkGenerator("R6_6_seed_A.json");
    checkGenerator("R6_6_seed_B.json");
    checkGenerator("R6_6_seed_C.json");
    PcRandoSeed seed;
    if (loadSeed("R6_6_seed_A.json", &seed)) {
        checkCmap(seed);
        checkScob(seed);
    }
    return rt::finish();
}
