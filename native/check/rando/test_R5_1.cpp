// test_R5_1.cpp - the hint generator (docs/RANDOMIZER_PLAN.md step R5.1, pc_rando_gen_hints.h).
//
//   - fixture R5_1_hints.json (R5_1_make_fixture.py, wwrando 1.10.0_9775811 dry): for 8 seeds and
//     option sets covering every hint type and placement path (defaults, path/barren/remote hints with
//     importance, key-lunacy, every category, one hint duplicated over three placements, every
//     placement off), pcr::generate's "hints" value equals wwrando's HintsRandomizer state;
//   - the spoiler log's Hints rows follow the hints (octo fairy, then each placement);
//   - hintsToJson / hintsFromJson round trip;
//   - the entrances.py zone helpers on known locations;
//   - the hints do not change the item placement (allowMissingHints gives the same locations).
// The validity batch over 200 seeds is native/tools/rando/ww_verify_hints.py (regress_rando/R5.1.txt).
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_gen.h"
#include "pc_rando_gen_hints.h"
#include "pc_rando_json.h"
#include "pc_rando_logic.h"
#include "pc_rando_seedfile.h"
#include "pc_rando_trap.h" // optionsFromJson
#include "rando_test.h"

namespace {

bool generateCase(const std::string& seed, const pcr::Json& opts, bool withHints, pcr::GenResult& res) {
    pcr::GenRequest req;
    req.seed = seed;
    req.version = "test";
    req.allowMissingHints = !withHints;
    std::string err;
    if (!RT_CHECK(pcr::optionsFromJson(opts, &req.options, &err))) {
        std::fprintf(stderr, "  %s: %s\n", seed.c_str(), err.c_str());
        return false;
    }
    if (!RT_CHECK(pcr::generate(req, res))) {
        std::fprintf(stderr, "  %s: %s\n", seed.c_str(), res.error.c_str());
        return false;
    }
    return true;
}

void testFixture() {
    std::string text, err;
    if (!RT_CHECK(rt::readFixture("R5_1_hints.json", text))) {
        return;
    }
    pcr::Json cases;
    if (!RT_CHECK(pcr::jsonParse(text, &cases, &err)) || !RT_CHECK(cases.isArray())) {
        return;
    }
    RT_CHECK_EQ(cases.size(), (size_t)8);
    for (size_t i = 0; i < cases.size(); i++) {
        const pcr::Json& c = cases.at(i);
        const std::string seed = c.find("seed")->text();
        const pcr::Json& want = *c.find("hints");
        pcr::GenResult res;
        if (!generateCase(seed, *c.find("options"), true, res)) {
            continue;
        }
        if (!RT_CHECK(res.seed.has_hints) || !RT_CHECK(!res.hintsMissing)) {
            continue;
        }
        const pcr::Json& got = res.seed.hints;
        if (!RT_CHECK(got == want)) {
            for (size_t k = 0; k < want.size(); k++) {
                const pcr::Json* g = got.find(want.keyAt(k));
                if (g == nullptr || *g != want.at(k)) {
                    std::fprintf(stderr, "  %s: hints key %s differs from wwrando's\n", seed.c_str(),
                                 want.keyAt(k).c_str());
                }
            }
        }

        // The JSON round trip.
        pcr::HintsResult parsed;
        if (RT_CHECK(pcr::hintsFromJson(got, parsed, &err))) {
            RT_CHECK(pcr::hintsToJson(parsed) == got);
        } else {
            std::fprintf(stderr, "  %s: %s\n", seed.c_str(), err.c_str());
        }

        // The spoiler log's Hints rows: Savage Labyrinth, the octo fairy, then each placement.
        const size_t h0 = res.spoilerLog.find("Hints:\n");
        if (RT_CHECK(h0 != std::string::npos)) {
            const std::string section = res.spoilerLog.substr(h0, res.spoilerLog.find("\n\n\n", h0) + 1 - h0);
            const pcr::Json& octo = *want.find("octo_fairy");
            RT_CHECK(section.find(": " + octo.find("reward")->text() + "\n") != std::string::npos);
            size_t rows = 0;
            for (char ch : section) {
                rows += ch == '\n';
            }
            size_t expect = 4; // "Hints:", the two Savage Labyrinth floors, the octo fairy
            const pcr::Json& pl = *want.find("placements");
            for (size_t k = 0; k < pl.size(); k++) {
                expect += pl.at(k).size();
            }
            RT_CHECK_EQ(rows, expect);
        }

        // Hints use their own RNG: the placement without them is the same.
        if (i < 3) {
            pcr::GenResult bare;
            if (generateCase(seed, *c.find("options"), false, bare)) {
                RT_CHECK(bare.hintsMissing && !bare.seed.has_hints);
                RT_CHECK(bare.seed.location_item_names == res.seed.location_item_names);
            }
        }
    }
}

void testZones() {
    pcr::Options o;
    pcr::Logic L;
    if (!RT_CHECK(L.create(o, pcr::defaultLogicState(o)))) {
        return;
    }
    auto zone = [&](const char* loc) {
        std::string z, err;
        const int i = L.locationIndex(loc);
        RT_CHECK(i >= 0);
        RT_CHECK(pcr::entranceZoneForItemLocation(L, i, z, &err));
        return z;
    };
    auto zones = [&](const char* loc) {
        std::vector<std::string> z;
        std::string err;
        const int i = L.locationIndex(loc);
        RT_CHECK(i >= 0);
        RT_CHECK(pcr::allZonesForItemLocation(L, i, z, &err));
        std::sort(z.begin(), z.end());
        return z;
    };
    using V = std::vector<std::string>;
    RT_CHECK_EQ(zone("Dragon Roost Cavern - First Room"), std::string("Dragon Roost Island"));
    RT_CHECK_EQ(zone("Forbidden Woods - Mothula Miniboss Room"), std::string("Forest Haven"));
    RT_CHECK_EQ(zone("Hyrule - Master Sword Chamber"), std::string("Tower of the Gods Sector"));
    RT_CHECK_EQ(zone("Forsaken Fortress - Helmaroc King Heart Container"), std::string("Forsaken Fortress Sector"));
    RT_CHECK_EQ(zone("Forsaken Fortress - Phantom Ganon"), std::string("Forsaken Fortress"));
    RT_CHECK_EQ(zone("Outset Island - Savage Labyrinth - Floor 30"), std::string("Outset Island"));
    RT_CHECK_EQ(zone("Ice Ring Isle - Inner Cave - Chest"), std::string("Ice Ring Isle"));
    RT_CHECK_EQ(zone("Two-Eye Reef - Big Octo Great Fairy"), std::string("Two-Eye Reef"));
    RT_CHECK_EQ(zone("Mailbox - Letter from Baito"), std::string("Mailbox"));
    RT_CHECK(zones("Mailbox - Letter from Baito") == V({"Earth Temple", "Headstone Island", "Mailbox"}));
    RT_CHECK(zones("Forsaken Fortress - Helmaroc King Heart Container") == V({"Forsaken Fortress"}));
    RT_CHECK(zones("Wind Temple - Molgera Heart Container") == V({"Gale Isle", "Wind Temple"}));
    RT_CHECK(zones("The Great Sea - Withered Trees") == V({"Cliff Plateau Isles", "The Great Sea"}));
    RT_CHECK(zones("Windfall Island - Chu Jelly Juice Shop - Give 15 Green Chu Jelly") == V({"Windfall Island"}));
    RT_CHECK(!pcr::isItemLocationBehindRandomizableEntrance(L, L.locationIndex("Ganon's Tower - Maze Chest")));
    RT_CHECK_EQ(pcr::zoneExitForItemLocation(L, L.locationIndex("Pawprint Isle - Wizzrobe Cave")),
                std::string("Pawprint Isle Wizzrobe Cave"));
    RT_CHECK_EQ(pcr::zoneExitForItemLocation(L, L.locationIndex("Pawprint Isle - Chuchu Cave - Chest")),
                std::string("Pawprint Isle Chuchu Cave"));
}

} // namespace

int main() {
    testZones();
    testFixture();
    return rt::finish();
}
