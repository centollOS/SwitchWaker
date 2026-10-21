// cos_rando_test_R1_5: progression spheres and the seed's logs (docs/RANDOMIZER_PLAN.md step R1.5).
//
// 1. Golden logs from wwrando (fixtures/R1_5_logs.txt, native/tools/rando/gen_spoiler_vectors.py):
//    - the plan's 5 seeds with hints off and a key-lunacy seed: our item fill (R1.4) runs end to end,
//      then the spoiler and non-spoiler logs must be byte-identical to the files wwrando wrote;
//    - "inject" cases (required bosses, randomized charts, entrances with nesting, a random starting
//      island, extra starting items): those randomizers are not in C++ yet, so the recorded decisions
//      and placement are restored into the Logic, and both logs must again be byte-identical;
//    - a do_not_generate_spoiler_log seed made with the port's key (non-spoiler log only).
//    The permalink, the seed hash and the Savage Labyrinth hint rows are checked against wwrando too;
//    the other hint rows come from the fixture (hint generation is R5's).
// 2. Synthetic checks: Python repr, entrance path names, file names, the error log, the D5 trailer,
//    and the "Failed to calculate progression spheres" error on a placement missing a progress item.
//
// A log mismatch prints the first differing line.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_gen_items.h"
#include "pc_rando_logic.h"
#include "pc_rando_options.h"
#include "pc_rando_spoiler.h"
#include "rando_test.h"

namespace {

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t b = 0;
    for (;;) {
        size_t e = s.find(sep, b);
        if (e == std::string::npos) {
            out.push_back(s.substr(b));
            return out;
        }
        out.push_back(s.substr(b, e - b));
        b = e + 1;
    }
}

struct Case {
    std::string label, seed, key, permalink, hash;
    bool hasKey = false, inject = false, nesting = false;
    int island = 44;
    std::vector<std::pair<std::string, std::string>> opts, charts, entrances, prerand, hints;
    std::vector<std::vector<std::string>> npaths;
    std::vector<std::string> reqDungeons, banDungeons, reqBosses, reqBossLocs, banLocs, xstart, items;
    std::vector<std::string> spoilerLines, nonSpoilerLines;
};

bool loadFixture(std::vector<Case>& out) {
    std::string text;
    if (!rt::readFixture("R1_5_logs.txt", text)) {
        return false;
    }
    bool ended = false;
    std::vector<std::string> lines = split(text, '\n');
    for (const std::string& line : lines) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = split(line, '\t');
        auto arg = [&](size_t i) { return i < f.size() ? f[i] : std::string(); };
        const std::string& tag = f[0];
        if (tag == "version") {
            RT_CHECK_EQ(arg(1), std::string(pcr::kWwrandoVersion));
            continue;
        }
        if (tag == "end") {
            ended = true;
            continue;
        }
        if (tag == "case") {
            out.push_back(Case());
            out.back().label = arg(1);
            out.back().seed = arg(2);
            continue;
        }
        if (out.empty()) {
            std::fprintf(stderr, "FAIL fixture line before a case: %s\n", line.c_str());
            return false;
        }
        Case& c = out.back();
        if (tag == "key") {
            c.key = arg(1);
            c.hasKey = true;
        } else if (tag == "inject") {
            c.inject = arg(1) == "1";
        } else if (tag == "opt") {
            c.opts.emplace_back(arg(1), arg(2));
        } else if (tag == "permalink") {
            c.permalink = arg(1);
        } else if (tag == "hash") {
            c.hash = arg(1);
        } else if (tag == "chart") {
            c.charts.emplace_back(arg(1), arg(2));
        } else if (tag == "entrance") {
            c.entrances.emplace_back(arg(1), arg(2));
        } else if (tag == "nesting") {
            c.nesting = arg(1) == "1";
        } else if (tag == "npath") {
            c.npaths.emplace_back(f.begin() + 1, f.end());
        } else if (tag == "reqdungeon") {
            c.reqDungeons.push_back(arg(1));
        } else if (tag == "bandungeon") {
            c.banDungeons.push_back(arg(1));
        } else if (tag == "reqboss") {
            c.reqBosses.push_back(arg(1));
        } else if (tag == "reqbossloc") {
            c.reqBossLocs.push_back(arg(1));
        } else if (tag == "banloc") {
            c.banLocs.push_back(arg(1));
        } else if (tag == "xstart") {
            c.xstart.push_back(arg(1));
        } else if (tag == "island") {
            c.island = std::atoi(arg(1).c_str());
        } else if (tag == "prerand") {
            c.prerand.emplace_back(arg(1), arg(2));
        } else if (tag == "loc") {
            if ((int)c.items.size() != std::atoi(arg(1).c_str())) {
                std::fprintf(stderr, "FAIL fixture location order at %s\n", line.c_str());
                return false;
            }
            c.items.push_back(arg(2));
        } else if (tag == "hint") {
            c.hints.emplace_back(arg(1), arg(2));
        } else if (tag == "S") {
            c.spoilerLines.push_back(line.substr(2));
        } else if (tag == "N") {
            c.nonSpoilerLines.push_back(line.substr(2));
        } else if (tag == "endcase") {
        } else {
            std::fprintf(stderr, "FAIL unknown fixture line: %s\n", line.c_str());
            return false;
        }
    }
    return RT_CHECK(ended);
}

// The S/N lines joined back (the generator split each log on \n; an empty log line is "S\t").
std::string joinLines(const std::vector<std::string>& lines) {
    std::string s;
    for (size_t i = 0; i < lines.size(); i++) {
        s += (i ? "\n" : "") + lines[i];
    }
    return s;
}

bool buildOptions(const Case& c, pcr::Options& o) {
    o = pcr::Options();
    for (const auto& kv : c.opts) {
        int id = pcr::optionFind(kv.first.c_str());
        if (!RT_CHECK(id >= 0) || !RT_CHECK(pcr::optionFromString(o, id, kv.second.c_str()))) {
            std::fprintf(stderr, "  option %s=%s\n", kv.first.c_str(), kv.second.c_str());
            return false;
        }
    }
    return true;
}

void compareText(const Case& c, const char* what, const std::string& ours, const std::string& want) {
    if (ours == want) {
        return;
    }
    rt::failures()++;
    std::vector<std::string> a = split(ours, '\n'), b = split(want, '\n');
    size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) {
        i++;
    }
    std::fprintf(stderr, "FAIL case %s: %s differs at line %zu (ours %zu lines, wwrando %zu)\n", c.label.c_str(),
                 what, i + 1, a.size(), b.size());
    std::fprintf(stderr, "  ours:    [%s]\n  wwrando: [%s]\n", i < a.size() ? a[i].c_str() : "(end)",
                 i < b.size() ? b[i].c_str() : "(end)");
}

// Restores the recorded decisions and placement into a Logic made by initRandomizerLogic, as the
// randomizers that run before the items (and the item fill) would leave it.
bool injectState(const Case& c, pcr::Logic& L) {
    const pcr::Options& o = L.options();
    pcr::LogicState& st = L.state();
    st.entranceConnections = c.entrances;
    st.islandCharts.assign(50, std::string());
    for (const auto& ch : c.charts) {
        const int island = std::atoi(ch.first.c_str());
        if (!RT_CHECK(island >= 1 && island <= 49)) {
            return false;
        }
        st.islandCharts[island] = ch.second;
    }
    st.bossReqsEnabled = o.required_bosses;
    st.requiredBossItemLocations = c.reqBossLocs;
    st.bannedLocations.clear();
    for (const std::string& n : c.banLocs) {
        const int loc = L.locationIndex(n);
        if (!RT_CHECK(loc >= 0)) {
            return false;
        }
        st.bannedLocations.push_back(loc);
    }
    st.extraStartItemsEnabled = o.num_extra_starting_items > 0;
    st.randomStartingItems = c.xstart;
    L.updateEntranceConnectionMacros();
    L.updateChartMacros();
    L.updateRequiredBossesMacro();
    for (const auto& p : c.prerand) {
        const int loc = L.locationIndex(p.first);
        if (!RT_CHECK(loc >= 0)) {
            return false;
        }
        L.setPrerandomizationItemLocation(loc, L.item(p.second));
    }
    if (!RT_CHECK_EQ((int)c.items.size(), L.locationCount())) {
        return false;
    }
    for (int i = 0; i < L.locationCount(); i++) {
        L.doneItemLocations[i] = L.item(c.items[i]);
    }
    return RT_CHECK(!L.failed());
}

pcr::SpoilerInput inputFor(const Case& c) {
    pcr::SpoilerInput in;
    in.seed = c.seed;
    in.permalink = c.permalink;
    in.seedHash = c.hash;
    in.requiredDungeons = c.reqDungeons;
    in.bannedDungeons = c.banDungeons;
    in.requiredBosses = c.reqBosses;
    in.startingIslandRoom = c.island;
    in.nestingEnabled = c.nesting;
    in.nestedEntrancePaths = c.npaths;
    in.hints = c.hints;
    return in;
}

int gCasesWithSpoiler = 0, gFilledCases = 0;

void runCase(const Case& c) {
    pcr::Options o;
    if (!buildOptions(c, o)) {
        return;
    }
    const char* key = c.hasKey ? c.key.c_str() : pcr::kSwitchWakerSeedKey;

    // The header's inputs.
    RT_CHECK_EQ(pcr::sanitizeSeed(c.seed), c.seed);
    const std::string permalink = pcr::encodePermalink(c.seed, o);
    if (!RT_CHECK_EQ(permalink, c.permalink)) {
        std::fprintf(stderr, "  case %s: permalink\n", c.label.c_str());
    }
    RT_CHECK_EQ(pcr::seedHash(permalink, o, key), c.hash);

    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    if (!RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init))) {
        std::fprintf(stderr, "  case %s: %s\n", c.label.c_str(), init.error.c_str());
        return;
    }
    if (c.inject) {
        if (!injectState(c, L)) {
            std::fprintf(stderr, "  case %s: inject failed: %s\n", c.label.c_str(), L.error().c_str());
            return;
        }
    } else {
        // Only the item fill changes the Logic in these presets (the fixture must agree).
        RT_CHECK(!o.required_bosses && !o.randomize_charts && !o.randomize_starting_island &&
                 o.num_extra_starting_items == 0 && c.island == 44 && !c.nesting);
        pcr::ItemFillResult res;
        if (!RT_CHECK(pcr::runItemRandomizer(L, init, c.seed, res, key))) {
            std::fprintf(stderr, "  case %s: %s\n", c.label.c_str(), res.error.c_str());
            return;
        }
        gFilledCases++;
        int mismatches = 0;
        for (int i = 0; i < L.locationCount() && i < (int)c.items.size(); i++) {
            const int it = L.doneItemLocations[i];
            mismatches += (it >= 0 ? L.name(it) : std::string("None")) != c.items[i];
        }
        if (!RT_CHECK_EQ(mismatches, 0)) {
            std::fprintf(stderr, "  case %s: the fill differs from wwrando\n", c.label.c_str());
        }
        // The entrance list printed is the vanilla one.
        RT_CHECK(L.state().entranceConnections == c.entrances);
    }

    // The Savage Labyrinth hint rows.
    std::vector<std::pair<std::string, std::string>> savage;
    pcr::savageLabyrinthHintRows(L, savage);
    if (RT_CHECK(c.hints.size() >= 2)) {
        RT_CHECK(savage[0] == c.hints[0]);
        RT_CHECK(savage[1] == c.hints[1]);
    }

    const pcr::SpoilerInput in = inputFor(c);
    compareText(c, "non-spoiler log", pcr::nonSpoilerLog(L, in), joinLines(c.nonSpoilerLines));
    if (!o.do_not_generate_spoiler_log) {
        std::string spoiler, err;
        if (!RT_CHECK(pcr::spoilerLog(L, in, spoiler, &err))) {
            std::fprintf(stderr, "  case %s: %s\n", c.label.c_str(), err.c_str());
            return;
        }
        compareText(c, "spoiler log", spoiler, joinLines(c.spoilerLines));
        gCasesWithSpoiler++;

        // The spheres alone: exactly one sphere defeats Ganondorf.
        std::vector<pcr::ProgressionSphere> spheres;
        RT_CHECK(pcr::calculateProgressionSpheres(L, in, spheres, &err));
        int ganon = 0;
        for (const pcr::ProgressionSphere& sp : spheres) {
            for (const pcr::SphereEntry& e : sp) {
                ganon += e.location == "Ganon's Tower - Rooftop" && e.item == "Defeat Ganondorf";
            }
        }
        RT_CHECK_EQ(ganon, 1);
    } else {
        RT_CHECK(c.spoilerLines.empty());
    }
}

void syntheticChecks() {
    // Python repr of a str.
    RT_CHECK_EQ(pcr::pyRepr("Bombs"), std::string("'Bombs'"));
    RT_CHECK_EQ(pcr::pyRepr("Hero's Charm"), std::string("\"Hero's Charm\""));
    RT_CHECK_EQ(pcr::pyRepr("a\"b"), std::string("'a\"b'"));
    RT_CHECK_EQ(pcr::pyRepr("it's \"x\""), std::string("'it\\'s \"x\"'"));
    RT_CHECK_EQ(pcr::pyRepr("a\\b\n"), std::string("'a\\\\b\\n'"));

    // shorten_path_name.
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Dungeon Entrance on Dragon Roost Island"),
                std::string("Dragon Roost Island (Main)"));
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Secret Cave Entrance on Dragon Roost Island"),
                std::string("Dragon Roost Island (Pit)"));
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Dungeon Entrance in Forest Haven Sector"),
                std::string("Forest Haven Sector"));
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Inner Entrance in Ice Ring Isle"), std::string("Ice Ring Isle"));
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Miniboss Entrance in Tower of the Gods"),
                std::string("Tower of the Gods (Miniboss)"));
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Boss Entrance in Forbidden Woods"),
                std::string("Forbidden Woods (Boss)"));
    RT_CHECK_EQ(pcr::shortenEntrancePathName("Gohma Boss Arena"), std::string("Gohma Boss Arena"));

    std::string zone, specific;
    pcr::splitLocationName("Windfall Island - Jail - Tingle - First Gift", zone, specific);
    RT_CHECK_EQ(zone, std::string("Windfall Island"));
    RT_CHECK_EQ(specific, std::string("Jail - Tingle - First Gift"));
    pcr::splitLocationName("Mailbox", zone, specific);
    RT_CHECK_EQ(zone, std::string("Mailbox"));
    RT_CHECK_EQ(specific, std::string("Mailbox"));

    RT_CHECK_EQ(pcr::logFileName("abc", "Spoiler Log"), std::string("WW Random abc - Spoiler Log.txt"));
    RT_CHECK_EQ(pcr::logFileName("abc", "Non-Spoiler Log"), std::string("WW Random abc - Non-Spoiler Log.txt"));
    RT_CHECK_EQ(pcr::logTrailer("v0.6.0"), std::string("Generated by SwitchWaker v0.6.0\n"));

    // The header and the error log at the defaults (no permalink or hash given: no lines for them).
    pcr::Options o;
    pcr::SpoilerInput in;
    in.seed = "abc";
    const std::string header = pcr::logHeader(o, in);
    const std::string want =
        std::string("Wind Waker Randomizer Version ") + pcr::kWwrandoVersion +
        "\nSeed: abc\nOptions selected:\n  progression_dungeons, progression_puzzle_secret_caves, "
        "progression_great_fairies, progression_free_gifts, progression_expensive_purchases, progression_misc, "
        "sword_mode: Start with Hero's Sword, num_required_bosses: 4, logic_obscurity: None, logic_precision: None, "
        "mix_entrances: Separate Dungeons From Caves & Fountains, hoho_hints, fishmen_hints, num_item_hints: 15, "
        "num_location_hints: 5, cryptic_hints, swift_sail, instant_text_boxes, reveal_full_sea_chart, "
        "skip_rematch_bosses, remove_title_and_ending_videos, starting_gear: ['Ballad of Gales', "
        "'Progressive Magic Meter', 'Progressive Shield', 'Song of Passing'], starting_hcs: 3, custom_player_model: "
        "Link, custom_color_preset: Default\n\n\n";
    RT_CHECK_EQ(header, want);
    RT_CHECK_EQ(pcr::errorLog(o, in, "Error on seed abc:\nboom\n\n"), want + "Error on seed abc:\nboom\n\n");

    // A placement missing a progress item: the spheres cannot be calculated (wwrando raises).
    pcr::Logic L;
    pcr::RandomizerLogicInit init;
    pcr::ItemFillResult res;
    if (RT_CHECK(pcr::initRandomizerLogic(L, o, pcr::defaultLogicState(o), init)) &&
        RT_CHECK(pcr::runItemRandomizer(L, init, "SpheresFail", res))) {
        const int hookshot = L.item("Hookshot");
        const int rupee = L.item("Green Rupee");
        for (int& it : L.doneItemLocations) {
            if (it == hookshot) {
                it = rupee;
            }
        }
        std::vector<pcr::ProgressionSphere> spheres;
        std::string err, log;
        RT_CHECK(!pcr::calculateProgressionSpheres(L, in, spheres, &err));
        RT_CHECK_EQ(err, std::string("Failed to calculate progression spheres"));
        RT_CHECK(!pcr::spoilerLog(L, in, log, &err));
    }
}

} // namespace

int main() {
    std::vector<Case> cases;
    if (loadFixture(cases)) {
        RT_CHECK(cases.size() >= 6);
        for (const Case& c : cases) {
            runCase(c);
        }
        // The plan's 5 seeds with hints off, plus key-lunacy, run the fill; every case but the
        // no-spoiler one has a spoiler log.
        RT_CHECK(gFilledCases >= 6);
        RT_CHECK_EQ(gCasesWithSpoiler, (int)cases.size() - 1);
    }
    syntheticChecks();
    return rt::finish();
}
