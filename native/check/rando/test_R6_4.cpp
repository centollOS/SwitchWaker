// Unit test of plan step R6.4: the extra starting items (pc_rando_gen_extra.h) and the starting
// hearts, Pieces of Heart and Triforce Shards that reach the new-game config.
//
//   - fixtures/R6_4_extra_start.json (ww_export.py R64Seed1 num_extra_starting_items=2
//     starting_pohs=7 num_starting_triforce_shards=8), R6_4_all_triforce_chart.json (chq178) and
//     R6_4_all_treasure_chart.json (chq107), both with every progression_* option on and 3 extra
//     starting items: our generator gives wwrando's placement (extra starting items, every location),
//     the grouped chart branch included (a Triforce Chart and a Treasure Chart are picked);
//   - the spoiler log's "Random starting item: ..." line, and none at 0 extra items;
//   - the config (pc_rando_slot_config_from_seed): the default gear followed by the extra items in
//     order, 3*4+7 = 19 quarter hearts, 8 shards;
//   - num_extra_starting_items = 0 leaves TestSeed1 as it was (fixtures/R0_6_TestSeed1.json).
// The full oracle batch: check_seeds.py --preset extra_start / extra_start_all / extra_start_keys
// --golden (regress_rando/R6.4.txt).
#include <cstring>
#include <string>
#include <vector>

#include "pc/pc_rando.h"
#include "pc/pc_rando_modes.h"
#include "pc_rando_gen.h"
#include "pc_rando_json.h"
#include "pc_rando_options.h"
#include "pc_rando_seedfile.h"
#include "pc_rando_slot.h"
#include "pc_rando_trap.h" // optionsFromJson
#include "rando_test.h"

namespace {

bool loadFixture(const char* name, PcRandoSeed& out) {
    std::string text, err;
    if (!RT_CHECK(rt::readFixture(name, text))) {
        return false;
    }
    if (!RT_CHECK(pc_rando_seed_parse(text, &out, &err))) {
        std::fprintf(stderr, "  %s: %s\n", name, err.c_str());
        return false;
    }
    return true;
}

// Generates the fixture's seed at the fixture's options and checks it equals wwrando's placement.
bool generateLike(const char* fixture, PcRandoSeed& want, pcr::GenResult& res) {
    if (!loadFixture(fixture, want)) {
        return false;
    }
    pcr::GenRequest req;
    req.seed = want.seed;
    req.allowMissingHints = true;
    req.version = "test";
    pcr::Json opts = want.options;
    opts.erase("dry_run");
    std::string err;
    if (!RT_CHECK(pcr::optionsFromJson(opts, &req.options, &err))) {
        std::fprintf(stderr, "  %s: %s\n", fixture, err.c_str());
        return false;
    }
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  %s: %s\n", fixture, res.error.c_str());
        return false;
    }
    if (!RT_CHECK(res.seed.extra_starting_items == want.extra_starting_items)) {
        std::fprintf(stderr, "  %s: extra starting items differ from wwrando\n", fixture);
    }
    RT_CHECK(res.seed.location_names == want.location_names);
    int diff = 0;
    for (size_t i = 0; i < res.seed.location_item_names.size() && i < want.location_item_names.size(); i++) {
        diff += res.seed.location_item_names[i] != want.location_item_names[i];
    }
    if (!RT_CHECK_EQ(diff, 0)) {
        std::fprintf(stderr, "  %s: locations differ from wwrando\n", fixture);
    }
    RT_CHECK(memcmp(res.seed.locations, want.locations, sizeof res.seed.locations) == 0);
    RT_CHECK_EQ(res.seed.permalink, want.permalink);
    return true;
}

std::string spoilerLine(const std::vector<std::string>& items) {
    std::string s = "Random starting item: ";
    for (size_t i = 0; i < items.size(); i++) {
        s += (i ? ", " : "") + items[i];
    }
    return s + "\n\n";
}

void testExtraStart() {
    PcRandoSeed want;
    pcr::GenResult res;
    if (!generateLike("R6_4_extra_start.json", want, res)) {
        return;
    }
    const std::vector<std::string> expect = {"Bait Bag", "Skull Hammer"};
    RT_CHECK(want.extra_starting_items == expect);
    RT_CHECK(res.spoilerLog.find(spoilerLine(expect)) != std::string::npos);
    // The non-spoiler log does not name them.
    RT_CHECK(res.nonSpoilerLog.find("Random starting item") == std::string::npos);

    // The config: default gear (Ballad of Gales, Progressive Shield, Song of Passing; the magic meter is
    // starting_magic), then the extra items in order; 19 quarter hearts; 8 shards.
    PcRandoConfig cfg;
    PcRandoModes modes;
    std::string err;
    if (!RT_CHECK(pc_rando_slot_config_from_seed(res.seed, &cfg, &modes, &err))) {
        std::fprintf(stderr, "  %s\n", err.c_str());
        return;
    }
    const uint8_t gear[] = {0x6E, 0x3B, 0x72, (uint8_t)pc_rando_item_id("Bait Bag"),
                            (uint8_t)pc_rando_item_id("Skull Hammer"), PC_RANDO_ITEM_END};
    RT_CHECK(memcmp(cfg.starting_gear, gear, sizeof gear) == 0);
    RT_CHECK_EQ(pc_rando_item_id("Bait Bag"), 0x2C);
    RT_CHECK_EQ(pc_rando_item_id("Skull Hammer"), 0x33);
    RT_CHECK_EQ(cfg.starting_magic, 16);
    RT_CHECK_EQ(cfg.starting_quarter_hearts, 3 * 4 + 7);
    RT_CHECK_EQ(cfg.num_triforce_shards, 8);

    // Deterministic.
    pcr::GenResult again;
    PcRandoSeed want2;
    RT_CHECK(generateLike("R6_4_extra_start.json", want2, again));
    RT_CHECK_EQ(again.placementJson, res.placementJson);
}

void testCharts(const char* fixture, const char* chartPrefix) {
    PcRandoSeed want;
    pcr::GenResult res;
    if (!generateLike(fixture, want, res)) {
        return;
    }
    RT_CHECK_EQ(want.extra_starting_items.size(), (size_t)3);
    bool chart = false;
    for (const std::string& n : want.extra_starting_items) {
        chart |= n.rfind(chartPrefix, 0) == 0;
    }
    if (!RT_CHECK(chart)) {
        std::fprintf(stderr, "  %s: no %s among the extra items\n", fixture, chartPrefix);
    }
    RT_CHECK(res.spoilerLog.find(spoilerLine(want.extra_starting_items)) != std::string::npos);
    PcRandoConfig cfg;
    PcRandoModes modes;
    std::string err;
    RT_CHECK(pc_rando_slot_config_from_seed(res.seed, &cfg, &modes, &err));
    for (size_t i = 0; i < 3; i++) {
        RT_CHECK_EQ((int)cfg.starting_gear[3 + i], pc_rando_item_id(want.extra_starting_items[i]));
    }
    RT_CHECK_EQ(cfg.starting_gear[6], PC_RANDO_ITEM_END);
}

void testNoneAtZero() {
    PcRandoSeed want;
    pcr::GenResult res;
    if (!generateLike("R0_6_TestSeed1.json", want, res)) {
        return;
    }
    RT_CHECK(res.seed.extra_starting_items.empty());
    RT_CHECK(res.spoilerLog.find("Random starting item") == std::string::npos);
}

void testNoLongerRefused() {
    pcr::GenRequest req;
    req.seed = "R64Seed2";
    req.allowMissingHints = true;
    req.version = "test";
    pcr::optionSetInt(req.options, pcr::OPT_num_extra_starting_items, 3);
    pcr::GenResult res;
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  R64Seed2: %s\n", res.error.c_str());
    }
    RT_CHECK(!res.unsupported);
    RT_CHECK_EQ(res.seed.extra_starting_items.size(), (size_t)3);
}

} // namespace

int main() {
    testExtraStart();
    testCharts("R6_4_all_triforce_chart.json", "Triforce Chart ");
    testCharts("R6_4_all_treasure_chart.json", "Treasure Chart ");
    testNoneAtZero();
    testNoLongerRefused();
    return rt::finish();
}
