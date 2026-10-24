// Unit test of plan step R1.6: the generator's entry point (pc_rando_gen.h).
//
//   - TestSeed1 at wwrando's defaults gives the placement of fixtures/R0_6_TestSeed1.json
//     (ww_export.py TestSeed1): seed, permalink, options (dry_run aside), decisions, every location;
//     the pigs bitfield is PigsRandomizer's (ww_trace.py TestSeed1: "PigsRandomizer randbelow 3 0",
//     so 0x01) and reaches the slot config;
//   - fixtures/R1_6_CheckSeed11_dungeons_only.json (ww_export.py CheckSeed11 with every
//     progression_* option off except progression_dungeons) checks the usefulness-cache fix of this
//     step (Logic::checkItemIsUseful stores like Python's dict);
//   - the refusals of options whose randomizer is not ported, hints on by default (R5.1), wwrando's
//     TooFewProgressionLocationsError with its error log, the progress callback and determinism.
// The full oracle batch is native/tools/rando/check_seeds.py (regress_rando/R1.6.txt).
#include <cstring>
#include <string>
#include <vector>

#include "pc/pc_rando.h"
#include "pc/pc_rando_modes.h"
#include "pc_rando_gen.h"
#include "pc_rando_json.h"
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

pcr::GenRequest request(const char* seed) {
    pcr::GenRequest req;
    req.seed = seed;
    req.allowMissingHints = true;
    req.version = "test";
    return req;
}

// Our placement against wwrando's (ww_export.py) one.
void comparePlacement(const char* label, const PcRandoSeed& ours, const PcRandoSeed& want) {
    if (!RT_CHECK_EQ(ours.seed, want.seed) || !RT_CHECK_EQ(ours.permalink, want.permalink)) {
        std::fprintf(stderr, "  %s\n", label);
    }
    pcr::Json a = ours.options, b = want.options;
    a.erase("dry_run");
    b.erase("dry_run");
    if (!RT_CHECK(a == b)) {
        std::fprintf(stderr, "  %s: options differ\n", label);
    }
    RT_CHECK(ours.charts == want.charts);
    RT_CHECK(ours.entrances == want.entrances);
    RT_CHECK(ours.extra_starting_items == want.extra_starting_items);
    RT_CHECK(ours.required_boss_locations == want.required_boss_locations);
    RT_CHECK_EQ(ours.starting_island_room, want.starting_island_room);
    RT_CHECK(ours.location_names == want.location_names);
    int diff = 0;
    for (size_t i = 0; i < ours.location_item_names.size() && i < want.location_item_names.size(); i++) {
        diff += ours.location_item_names[i] != want.location_item_names[i];
    }
    if (!RT_CHECK_EQ(diff, 0) || !RT_CHECK_EQ(ours.location_item_names.size(), want.location_item_names.size())) {
        std::fprintf(stderr, "  %s: locations differ from wwrando\n", label);
    }
    RT_CHECK(memcmp(ours.locations, want.locations, sizeof ours.locations) == 0);
}

struct ProgressLog {
    std::vector<int> done;
    int total = -1;
    bool sameTotal = true;
};

void onProgress(void* user, const char* text, int done, int total) {
    ProgressLog* p = (ProgressLog*)user;
    RT_CHECK(text != nullptr && text[0] != '\0');
    if (p->total >= 0 && p->total != total) {
        p->sameTotal = false;
    }
    p->total = total;
    p->done.push_back(done);
}

void testTestSeed1() {
    PcRandoSeed want;
    if (!loadFixture("R0_6_TestSeed1.json", want)) {
        return;
    }
    pcr::GenRequest req = request("TestSeed1");
    ProgressLog prog;
    req.progress = onProgress;
    req.progressUser = &prog;
    pcr::GenResult res;
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  TestSeed1: %s\n", res.error.c_str());
        return;
    }
    comparePlacement("TestSeed1", res.seed, want);
    RT_CHECK_EQ(res.sanitizedSeed, std::string("TestSeed1"));
    RT_CHECK_EQ(res.pigsBitfield, 0x01);
    RT_CHECK(res.hintsMissing);
    RT_CHECK(!res.unsupported);
    RT_CHECK(res.errorLog.empty());
    RT_CHECK(!res.spoilerLog.empty());
    RT_CHECK(!res.nonSpoilerLog.empty());
    RT_CHECK(res.spoilerLog.find("Seed: TestSeed1\n") != std::string::npos);
    RT_CHECK(res.spoilerLog.find("Seed Hash: " + res.seedHash + "\n") != std::string::npos);

    // Our keys: version, the pigs; the JSON is the seed file's own serialization.
    RT_CHECK(res.seed.has_version);
    RT_CHECK_EQ(res.seed.version, std::string("SwitchWaker test"));
    const pcr::Json* pigs = res.seed.extra.find("captured_pigs_bitfield");
    RT_CHECK(pigs != nullptr && pigs->isInteger() && pigs->asInt() == 0x01);
    RT_CHECK_EQ(res.placementJson, pc_rando_seed_serialize(res.seed));
    PcRandoSeed again;
    std::string err;
    RT_CHECK(pc_rando_seed_parse(res.placementJson, &again, &err));
    RT_CHECK(memcmp(again.hash, res.seed.hash, 16) == 0);

    // The pigs reach the new-game config (pc_rando_slot); a placement without the key keeps 0x04.
    PcRandoConfig cfg;
    PcRandoModes modes;
    RT_CHECK(pc_rando_slot_config_from_seed(res.seed, &cfg, &modes, &err));
    RT_CHECK_EQ(cfg.pigs_bitfield, 0x01);
    RT_CHECK(pc_rando_slot_config_from_seed(want, &cfg, &modes, &err));
    RT_CHECK_EQ(cfg.pigs_bitfield, 0x04);

    // Progress: one total, done never goes back, ends at the total.
    RT_CHECK(prog.sameTotal);
    RT_CHECK(!prog.done.empty() && prog.done.back() == prog.total);
    for (size_t i = 1; i < prog.done.size(); i++) {
        RT_CHECK(prog.done[i] >= prog.done[i - 1]);
    }

    // Deterministic.
    pcr::GenResult res2;
    RT_CHECK(pcr::generate(request("TestSeed1"), res2));
    RT_CHECK_EQ(res2.placementJson, res.placementJson);
    RT_CHECK_EQ(res2.spoilerLog, res.spoilerLog);

    // The seed is sanitized as wwrando does.
    pcr::GenResult res3;
    RT_CHECK(pcr::generate(request("  Test Seed!1 "), res3));
    RT_CHECK_EQ(res3.sanitizedSeed, std::string("TestSeed1"));
    RT_CHECK_EQ(res3.placementJson, res.placementJson);
}

void testDungeonsOnly() {
    PcRandoSeed want;
    if (!loadFixture("R1_6_CheckSeed11_dungeons_only.json", want)) {
        return;
    }
    pcr::GenRequest req = request("CheckSeed11");
    for (int id = 0; id < pcr::OPT_COUNT; id++) {
        if (strncmp(pcr::kOptionInfo[id].name, "progression_", 12) == 0) {
            pcr::optionSetInt(req.options, id, id == pcr::OPT_progression_dungeons ? 1 : 0);
        }
    }
    pcr::GenResult res;
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  CheckSeed11: %s\n", res.error.c_str());
        return;
    }
    comparePlacement("CheckSeed11 dungeons only", res.seed, want);
}

void testRefusals() {
    // Hints are generated since R5.1 (no refusal; test_R5_1 checks them).
    {
        pcr::GenRequest req = request("TestSeed1");
        req.allowMissingHints = false;
        pcr::GenResult res;
        RT_CHECK(pcr::generate(req, res));
        RT_CHECK(!res.unsupported && !res.hintsMissing);
        RT_CHECK(res.seed.has_hints);
    }
    // No option is refused any more: entrances are generated since R6.11, randomize_enemy_palettes is
    // live since R7.2e, randomize_enemies since R7.4d (test_R7_3c checks the menu generates it).
    {
        pcr::Options o;
        o.randomize_enemies = true;
        o.randomize_enemy_palettes = true;
        RT_CHECK(pcr::unsupportedOptionsError(o, true).empty());
    }
    RT_CHECK(pcr::unsupportedOptionsError(pcr::Options(), true).empty());
    // A seed with no allowed character.
    pcr::GenResult res;
    RT_CHECK(!pcr::generate(request("!!!"), res));
    RT_CHECK(!res.unsupported);
}

void testTooFewLocations() {
    // Every category off: wwrando's TooFewProgressionLocationsError.
    pcr::GenRequest req = request("TestSeed1");
    for (int id = 0; id < pcr::OPT_COUNT; id++) {
        if (strncmp(pcr::kOptionInfo[id].name, "progression_", 12) == 0) {
            pcr::optionSetInt(req.options, id, 0);
        }
    }
    pcr::GenResult res;
    RT_CHECK(!pcr::generate(req, res));
    RT_CHECK(!res.unsupported);
    RT_CHECK(res.error.rfind("Not enough progress locations to place all progress items.\n", 0) == 0);
    RT_CHECK(res.errorLog.find("Permalink: " + res.permalink + "\n") != std::string::npos);
    RT_CHECK(res.errorLog.find("Error on seed TestSeed1:\nNot enough progress locations") != std::string::npos);
}

void testOptionsJson() {
    const pcr::Json j = pcr::optionsToJson(pcr::Options());
    RT_CHECK_EQ(j.size(), (size_t)pcr::OPT_COUNT);
    RT_CHECK_EQ(j.keyAt(0), std::string("progression_dungeons"));
    const pcr::Json* sm = j.find("sword_mode");
    RT_CHECK(sm != nullptr && sm->text() == "Start with Hero's Sword");
    const pcr::Json* cc = j.find("custom_colors");
    RT_CHECK(cc != nullptr && cc->isObject() && cc->size() == 0);
    const pcr::Json* sg = j.find("starting_gear");
    RT_CHECK(sg != nullptr && sg->isArray() && sg->size() == 4);
    pcr::Options back;
    std::string err;
    RT_CHECK(pcr::optionsFromJson(j, &back, &err));
    RT_CHECK(pcr::optionsEqual(back, pcr::Options()));
}

} // namespace

int main() {
    testOptionsJson();
    testTestSeed1();
    testDungeonsOnly();
    testRefusals();
    testTooFewLocations();
    return rt::finish();
}
