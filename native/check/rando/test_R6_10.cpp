// cos_rando_test_R6_10: entrance randomizer tables and sets (docs/RANDOMIZER_PLAN.md step R6.10).
//
// 1. Table equality with a Python dump of wwrando's randomizers/entrances.py
//    (fixtures/R6_10_entrance_tables.txt, native/tools/rando/gen_entrance_tables.py):
//    - every field of the 44 ZoneEntrances and 44 ZoneExits, in definition order;
//    - the 12 *_ENTRANCES / *_EXITS lists, the default entrance_connections, the 15
//      ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES and the module-level name lists;
//    - get_all_entrance_sets_to_be_randomized for all 64 combinations of the 6 randomize_*_entrances
//      options x both mix_entrances modes (including the raise for none on);
//    - EntranceRandomizer.__init__ for 36 option sets (defaults, the 32 on/off combinations of the 5
//      categories it reads, nesting, inner caves only, all mixed): is_enabled, nesting_enabled,
//      entrance_names_with_no_requirements, exit_names_with_no_requirements;
//    - register_mappings_between_item_locations_and_zone_exits (both maps, dict order) and
//      get_all_entrances_on_path_to_entrance for every entrance with the default connections.
// 2. The tables agree with what R1.3 already holds (vanillaEntrances(), entranceSetsFor()).
// 3. The nesting walks: undecided, a loop, and an outer zone moved by a connection.
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_rando_entrances.h"
#include "pc_rando_logic.h"
#include "pc_rando_options.h"
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

std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        s += (i ? "|" : "") + v[i];
    }
    return s;
}

std::string joinSpan(pcr::StrSpan v) {
    std::string s;
    for (size_t i = 0; i < v.size; i++) {
        s += (i ? "|" : "") + std::string(v[i]);
    }
    return s;
}

std::string entranceNames(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        s += (i ? "|" : "") + std::string(pcr::zoneEntrances()[v[i]].name);
    }
    return s;
}

std::string exitNames(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        s += (i ? "|" : "") + std::string(pcr::zoneExits()[v[i]].name);
    }
    return s;
}

std::string ints(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) {
        s += (i ? "," : "") + std::to_string(v[i]);
    }
    return s;
}

std::string str(const char* s) { return s == nullptr ? std::string("-") : std::string(s); }
std::string num(int v) { return v < 0 ? std::string("-") : std::to_string(v); }

const char* groupName(pcr::EntranceGroup g) {
    static const char* const kNames[] = {"Dungeon", "Miniboss", "Boss", "SecretCave", "SecretCaveInner",
                                         "FairyFountain"};
    return kNames[(int)g];
}

std::string mixName(pcr::EntranceMixMode m) {
    return m == pcr::EntranceMixMode::SeparateDungeons ? "Separate Dungeons From Caves & Fountains"
                                                       : "Mix Dungeons & Caves & Fountains";
}

const pcr::EntranceGroup kAllGroups[] = {pcr::EntranceGroup::Dungeon,    pcr::EntranceGroup::Miniboss,
                                         pcr::EntranceGroup::Boss,       pcr::EntranceGroup::SecretCave,
                                         pcr::EntranceGroup::SecretCaveInner, pcr::EntranceGroup::FairyFountain};

// --- 1. The dump ----------------------------------------------------------------------------------

struct Counts {
    int entrances = 0, exits = 0, groups = 0, conns = 0, overrides = 0, lists = 0;
    int setBlocks = 0, setErrors = 0, cases = 0, locExits = 0, exitLocs = 0, paths = 0;
};

void checkEntranceLine(const std::vector<std::string>& f, Counts& n) {
    const int i = std::atoi(f[1].c_str());
    if (!RT_CHECK(f.size() == 13) || !RT_CHECK(i >= 0 && i < pcr::kNumZoneEntrances)) {
        return;
    }
    const pcr::ZoneEntranceDef& e = pcr::zoneEntrances()[i];
    const std::string got = std::string(groupName(e.group)) + "\t" + e.stage + "\t" + std::to_string(e.room) + "\t" +
                            std::to_string(e.sclsExitIndex) + "\t" + std::to_string(e.spawnId) + "\t" + e.name +
                            "\t" + str(e.islandName) + "\t" + str(e.warpOutStage) + "\t" + num(e.warpOutRoom) +
                            "\t" + num(e.warpOutSpawn) + "\t" +
                            (e.nestedIn >= 0 ? std::string(pcr::zoneExits()[e.nestedIn].name) : std::string("-"));
    std::string want;
    for (size_t k = 2; k < f.size(); k++) {
        want += (k > 2 ? "\t" : "") + f[k];
    }
    if (!RT_CHECK_EQ(got, want)) {
        std::fprintf(stderr, "  entrance %d\n", i);
    }
    RT_CHECK_EQ(pcr::zoneEntranceIndex(e.name), i);
    // Exactly one of island_name and nested_in (ZoneEntrance.__post_init__'s asserts).
    RT_CHECK((e.islandName == nullptr) != (e.nestedIn < 0));
    RT_CHECK((e.warpOutStage == nullptr) != (e.nestedIn < 0));
    n.entrances++;
}

void checkExitLine(const std::vector<std::string>& f, Counts& n) {
    const int i = std::atoi(f[1].c_str());
    if (!RT_CHECK(f.size() == 10) || !RT_CHECK(i >= 0 && i < pcr::kNumZoneExits)) {
        return;
    }
    const pcr::ZoneExitDef& x = pcr::zoneExits()[i];
    const std::string got = std::string(groupName(x.group)) + "\t" + x.stage + "\t" + std::to_string(x.room) + "\t" +
                            num(x.sclsExitIndex) + "\t" + std::to_string(x.spawnId) + "\t" + x.name + "\t" +
                            str(x.bossStageName) + "\t" + str(x.zoneName);
    std::string want;
    for (size_t k = 2; k < f.size(); k++) {
        want += (k > 2 ? "\t" : "") + f[k];
    }
    if (!RT_CHECK_EQ(got, want)) {
        std::fprintf(stderr, "  exit %d\n", i);
    }
    RT_CHECK_EQ(pcr::zoneExitIndex(x.name), i);
    n.exits++;
}

void checkGroupLine(const std::string& list, const std::string& names, Counts& n) {
    for (pcr::EntranceGroup g : kAllGroups) {
        const std::string base = pcr::entranceGroupListName(g);
        const bool ents = list == base + "_ENTRANCES";
        const bool exits = list == base + "_EXITS";
        if (!ents && !exits) {
            continue;
        }
        const pcr::IndexRange r = ents ? pcr::entranceGroup(g) : pcr::exitGroup(g);
        std::vector<int> idx;
        for (int i = r.first; i < r.first + r.count; i++) {
            idx.push_back(i);
            RT_CHECK(ents ? pcr::zoneEntrances()[i].group == g : pcr::zoneExits()[i].group == g);
        }
        if (!RT_CHECK_EQ(ents ? entranceNames(idx) : exitNames(idx), names)) {
            std::fprintf(stderr, "  group %s\n", list.c_str());
        }
        n.groups++;
        return;
    }
    RT_CHECK(!"unknown group list");
    std::fprintf(stderr, "  group %s\n", list.c_str());
}

void checkGlobalList(const std::string& name, const std::string& value, Counts& n) {
    pcr::StrSpan got{nullptr, 0};
    if (name == "DUNGEON_ENTRANCE_NAMES_WITH_NO_REQUIREMENTS") {
        got = pcr::dungeonEntranceNamesWithNoRequirements();
    } else if (name == "SECRET_CAVE_ENTRANCE_NAMES_WITH_NO_REQUIREMENTS") {
        got = pcr::secretCaveEntranceNamesWithNoRequirements();
    } else if (name == "DUNGEON_EXIT_NAMES_WITH_NO_REQUIREMENTS") {
        got = pcr::dungeonExitNamesWithNoRequirements();
    } else if (name == "PUZZLE_SECRET_CAVE_EXIT_NAMES_WITH_NO_REQUIREMENTS") {
        got = pcr::puzzleSecretCaveExitNamesWithNoRequirements();
    } else if (name == "COMBAT_SECRET_CAVE_EXIT_NAMES_WITH_NO_REQUIREMENTS") {
        got = pcr::combatSecretCaveExitNamesWithNoRequirements();
    } else if (name == "ENTRANCE_RANDOMIZABLE_ITEM_LOCATION_TYPES") {
        got = pcr::entranceRandomizableItemLocationTypes();
        for (const char* t : got) {
            RT_CHECK(pcr::loc_type_bit(t) != 0);
        }
    } else {
        RT_CHECK(!"unknown list");
    }
    if (!RT_CHECK_EQ(joinSpan(got), value)) {
        std::fprintf(stderr, "  list %s\n", name.c_str());
    }
    n.lists++;
}

pcr::Options setOptions(int mask, const std::string& mix) {
    pcr::Options o;
    o.randomize_dungeon_entrances = (mask >> 0) & 1;
    o.randomize_secret_cave_entrances = (mask >> 1) & 1;
    o.randomize_miniboss_entrances = (mask >> 2) & 1;
    o.randomize_boss_entrances = (mask >> 3) & 1;
    o.randomize_secret_cave_inner_entrances = (mask >> 4) & 1;
    o.randomize_fairy_fountain_entrances = (mask >> 5) & 1;
    o.mix_entrances = mix == mixName(pcr::EntranceMixMode::MixDungeons) ? pcr::EntranceMixMode::MixDungeons
                                                                        : pcr::EntranceMixMode::SeparateDungeons;
    RT_CHECK_EQ(mixName(o.mix_entrances), mix);
    return o;
}

// One "sets" block: the yielded sets (entrances \t exits), or the error.
void checkSets(int mask, const std::string& mix, const std::vector<std::string>& want, const std::string& wantErr,
               Counts& n) {
    const pcr::Options o = setOptions(mask, mix);
    std::vector<pcr::EntranceSet> sets;
    std::string err;
    const bool ok = pcr::entranceSetsToBeRandomized(o, sets, &err);
    const int before = rt::failures();
    if (!wantErr.empty()) {
        RT_CHECK(!ok);
        RT_CHECK_EQ(err, wantErr);
        RT_CHECK(sets.empty());
        n.setErrors++;
    } else if (RT_CHECK(ok) && RT_CHECK_EQ(sets.size(), want.size())) {
        for (size_t i = 0; i < sets.size(); i++) {
            RT_CHECK_EQ(entranceNames(sets[i].entrances) + "\t" + exitNames(sets[i].exits), want[i]);
        }
    }
    RT_CHECK_EQ(pcr::entranceRandoEnabled(o), mask != 0);
    // R1.3's copy of the same function (names; empty and no error when entrances are off).
    std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> old;
    std::string oldErr;
    const bool oldOk = pcr::entranceSetsFor(o, old, &oldErr);
    if (mask != 0 && RT_CHECK(oldOk) && RT_CHECK_EQ(old.size(), sets.size())) {
        for (size_t i = 0; i < old.size(); i++) {
            RT_CHECK_EQ(join(old[i].first), entranceNames(sets[i].entrances));
            RT_CHECK_EQ(join(old[i].second), exitNames(sets[i].exits));
        }
    }
    if (rt::failures() != before) {
        std::fprintf(stderr, "  sets mask=%d mix=%s\n", mask, mix.c_str());
    }
    n.setBlocks++;
}

struct Case {
    std::string name;
    std::vector<std::pair<std::string, std::string>> opts;
    std::vector<std::pair<std::string, std::string>> flags, lists;
    std::vector<std::pair<int, std::string>> locExits;
    std::vector<std::pair<std::string, std::string>> exitLocs, paths;
};

bool buildOptions(const Case& c, pcr::Options& o) {
    o = pcr::Options();
    for (const auto& kv : c.opts) {
        const int id = pcr::optionFind(kv.first.c_str());
        if (!RT_CHECK(id >= 0) || !RT_CHECK(pcr::optionFromString(o, id, kv.second.c_str()))) {
            std::fprintf(stderr, "  option %s=%s\n", kv.first.c_str(), kv.second.c_str());
            return false;
        }
    }
    pcr::validateOptions(o);
    return true;
}

void checkCase(const Case& c, Counts& n) {
    pcr::Options o;
    if (!buildOptions(c, o)) {
        return;
    }
    const int before = rt::failures();
    for (const auto& kv : c.flags) {
        if (kv.first == "is_enabled") {
            RT_CHECK_EQ(pcr::entranceRandoEnabled(o) ? "1" : "0", kv.second);
        } else if (kv.first == "nesting_enabled") {
            RT_CHECK_EQ(pcr::entranceNestingEnabled(o) ? "1" : "0", kv.second);
        } else {
            RT_CHECK(!"unknown flag");
        }
    }
    for (const auto& kv : c.lists) {
        if (kv.first == "entrance_names_with_no_requirements") {
            RT_CHECK_EQ(join(pcr::entranceNamesWithNoRequirements(o)), kv.second);
        } else if (kv.first == "exit_names_with_no_requirements") {
            RT_CHECK_EQ(join(pcr::exitNamesWithNoRequirements(o)), kv.second);
        } else {
            RT_CHECK(!"unknown case list");
        }
    }
    if (!c.locExits.empty() || !c.exitLocs.empty()) {
        pcr::ExitLocationMap m;
        std::string err;
        if (RT_CHECK(pcr::buildExitLocationMap(m, &err))) {
            // item_location_to_containing_zone_exit: the locations with an exit, in order.
            std::vector<std::pair<int, std::string>> got;
            for (int loc = 0; loc < (int)m.locationExit.size(); loc++) {
                if (m.locationExit[loc] >= 0) {
                    got.emplace_back(loc, pcr::zoneExits()[m.locationExit[loc]].name);
                    bool behind = pcr::itemLocationBehindRandomizableEntrance(loc);
                    RT_CHECK(behind);
                }
            }
            if (RT_CHECK_EQ(got.size(), c.locExits.size())) {
                for (size_t i = 0; i < got.size(); i++) {
                    RT_CHECK_EQ(got[i].first, c.locExits[i].first);
                    RT_CHECK_EQ(got[i].second, c.locExits[i].second);
                }
            }
            n.locExits += (int)c.locExits.size();
            if (RT_CHECK_EQ(m.exitLocations.size(), c.exitLocs.size())) {
                for (size_t i = 0; i < m.exitLocations.size(); i++) {
                    RT_CHECK_EQ(std::string(pcr::zoneExits()[m.exitLocations[i].first].name), c.exitLocs[i].first);
                    RT_CHECK_EQ(ints(m.exitLocations[i].second), c.exitLocs[i].second);
                    RT_CHECK(&m.locationsFor(m.exitLocations[i].first) == &m.exitLocations[i].second);
                }
            }
            n.exitLocs += (int)c.exitLocs.size();
        } else {
            std::fprintf(stderr, "  buildExitLocationMap: %s\n", err.c_str());
        }
    }
    if (!c.paths.empty()) {
        const pcr::EntranceConnections conns = pcr::EntranceConnections::defaults();
        RT_CHECK_EQ(c.paths.size(), (size_t)pcr::kNumZoneEntrances);
        for (const auto& kv : c.paths) {
            const int e = pcr::zoneEntranceIndex(kv.first);
            if (!RT_CHECK(e >= 0)) {
                continue;
            }
            std::vector<int> path;
            std::string err;
            RT_CHECK(pcr::entrancesOnPathToEntrance(conns, e, path, &err) == pcr::EntrancePath::Ok);
            RT_CHECK_EQ(entranceNames(path), kv.second);
            RT_CHECK_EQ(pcr::outermostEntranceForEntrance(conns, e, &err), path.back());
            n.paths++;
        }
    }
    if (rt::failures() != before) {
        std::fprintf(stderr, "  case %s\n", c.name.c_str());
    }
    n.cases++;
}

void testDump() {
    std::string text;
    if (!RT_CHECK(rt::readFixture("R6_10_entrance_tables.txt", text))) {
        return;
    }
    Counts n;
    bool ended = false;
    std::vector<Case> cases;
    // The "sets" block being read.
    int setMask = -1;
    std::string setMix, setErr;
    std::vector<std::string> setLines;
    for (const std::string& line : split(text, '\n')) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> f = split(line, '\t');
        auto arg = [&](size_t i) { return i < f.size() ? f[i] : std::string(); };
        const std::string& tag = f[0];
        if (tag == "version") {
            RT_CHECK_EQ(arg(1), std::string(pcr::kWwrandoVersion));
        } else if (tag == "entrance") {
            checkEntranceLine(f, n);
        } else if (tag == "exit") {
            checkExitLine(f, n);
        } else if (tag == "group") {
            checkGroupLine(arg(1), arg(2), n);
        } else if (tag == "override") {
            const auto ov = pcr::itemLocationExitOverrides();
            if (RT_CHECK((size_t)n.overrides < ov.size)) {
                RT_CHECK_EQ(std::string(ov[n.overrides].location), arg(1));
                RT_CHECK_EQ(std::string(pcr::zoneExits()[ov[n.overrides].exit].name), arg(2));
                RT_CHECK(pcr::location_index(ov[n.overrides].location) >= 0);
            }
            n.overrides++;
        } else if (tag == "list" && cases.empty()) {
            checkGlobalList(arg(1), arg(2), n);
        } else if (tag == "list") {
            cases.back().lists.emplace_back(arg(1), arg(2));
        } else if (tag == "sets") {
            setMask = std::atoi(arg(1).c_str());
            setMix = arg(2);
            setErr.clear();
            setLines.clear();
        } else if (tag == "set") {
            setLines.push_back(arg(1) + "\t" + arg(2));
        } else if (tag == "seterror") {
            setErr = arg(1);
        } else if (tag == "endsets") {
            checkSets(setMask, setMix, setLines, setErr, n);
        } else if (tag == "conn") {
            const int e = pcr::zoneEntranceIndex(arg(1));
            if (RT_CHECK_EQ(e, n.conns)) {
                RT_CHECK_EQ(std::string(pcr::zoneExits()[pcr::defaultConnectionExit(e)].name), arg(2));
            }
            n.conns++;
        } else if (tag == "case") {
            cases.push_back(Case());
            cases.back().name = arg(1);
        } else if (tag == "opt") {
            cases.back().opts.emplace_back(arg(1), arg(2));
        } else if (tag == "flag") {
            cases.back().flags.emplace_back(arg(1), arg(2));
        } else if (tag == "locexit") {
            cases.back().locExits.emplace_back(std::atoi(arg(1).c_str()), arg(2));
        } else if (tag == "exitlocs") {
            cases.back().exitLocs.emplace_back(arg(1), arg(2));
        } else if (tag == "path") {
            cases.back().paths.emplace_back(arg(1), arg(2));
        } else if (tag == "endcase") {
            checkCase(cases.back(), n);
        } else if (tag == "end") {
            ended = true;
        } else {
            std::fprintf(stderr, "FAIL unknown fixture line: %s\n", line.c_str());
            rt::failures()++;
        }
    }
    RT_CHECK(ended);
    RT_CHECK_EQ(n.entrances, 44);
    RT_CHECK_EQ(n.exits, 44);
    RT_CHECK_EQ(n.groups, 12);
    RT_CHECK_EQ(n.conns, 44);
    RT_CHECK_EQ(n.overrides, 15);
    RT_CHECK_EQ((size_t)n.overrides, pcr::itemLocationExitOverrides().size);
    RT_CHECK_EQ(n.lists, 6);
    RT_CHECK_EQ(n.setBlocks, 128);
    RT_CHECK_EQ(n.setErrors, 2);
    RT_CHECK_EQ(n.cases, 36);
    RT_CHECK(n.locExits > 100);
    RT_CHECK(n.exitLocs >= 40);
    RT_CHECK_EQ(n.paths, 44);
}

// --- 2. Agreement with R1.3's tables ----------------------------------------------------------------

void testAgreesWithLogic() {
    int count = 0;
    const pcr::VanillaEntrance* v = pcr::vanillaEntrances(&count);
    if (!RT_CHECK_EQ(count, pcr::kNumZoneEntrances)) {
        return;
    }
    for (int i = 0; i < count; i++) {
        const pcr::ZoneEntranceDef& e = pcr::zoneEntrances()[i];
        RT_CHECK_EQ(std::string(v[i].entrance), std::string(e.name));
        RT_CHECK_EQ(std::string(v[i].exit), std::string(pcr::zoneExits()[pcr::defaultConnectionExit(i)].name));
        RT_CHECK(v[i].group == e.group);
        RT_CHECK_EQ(str(v[i].nestedIn), e.nestedIn >= 0 ? std::string(pcr::zoneExits()[e.nestedIn].name) : "-");
    }
    // pcr::exit_overrides() (R0.3's generated copy used by the hints).
    const auto gen = pcr::exit_overrides();
    const auto ov = pcr::itemLocationExitOverrides();
    if (RT_CHECK_EQ(gen.size, ov.size)) {
        for (size_t i = 0; i < ov.size; i++) {
            RT_CHECK_EQ(std::string(gen[i].name), std::string(ov[i].location));
            RT_CHECK_EQ(std::string(gen[i].text), std::string(pcr::zoneExits()[ov[i].exit].name));
        }
    }
}

// --- 3. Nesting walks -------------------------------------------------------------------------------

void testNesting() {
    const int gohmaDoor = pcr::zoneEntranceIndex("Boss Entrance in Dragon Roost Cavern");
    const int drc = pcr::zoneExitIndex("Dragon Roost Cavern");
    const int driMain = pcr::zoneEntranceIndex("Dungeon Entrance on Dragon Roost Island");
    const int galeDoor = pcr::zoneEntranceIndex("Dungeon Entrance on Gale Isle");
    const int wt = pcr::zoneExitIndex("Wind Temple");
    const int wtBossDoor = pcr::zoneEntranceIndex("Boss Entrance in Wind Temple");
    const int molgera = pcr::zoneExitIndex("Molgera Boss Arena");
    if (!RT_CHECK(gohmaDoor >= 0 && drc >= 0 && driMain >= 0 && galeDoor >= 0 && wt >= 0 && wtBossDoor >= 0 &&
                  molgera >= 0)) {
        return;
    }
    std::string err;
    std::vector<int> path;

    // Undecided: DRC has no entrance (deleted from done_exits_to_entrances).
    pcr::EntranceConnections c = pcr::EntranceConnections::defaults();
    c.exitToEntrance[drc] = -1;
    c.entranceToExit[driMain] = -1;
    RT_CHECK(pcr::entrancesOnPathToEntrance(c, gohmaDoor, path, &err) == pcr::EntrancePath::Undecided);
    RT_CHECK_EQ(pcr::outermostEntranceForEntrance(c, gohmaDoor, &err), -1);
    RT_CHECK(err.empty());
    RT_CHECK_EQ(pcr::outermostEntranceForExit(c, drc, &err), -1);
    RT_CHECK_EQ(err, std::string("KeyError: ZoneExit('Dragon Roost Cavern')"));

    // Moved: DRC behind Wind Temple's boss door -> Gohma's door is reached from Gale Isle.
    c = pcr::EntranceConnections::defaults();
    c.connect(wtBossDoor, drc);
    c.connect(driMain, molgera);
    err.clear();
    RT_CHECK(pcr::entrancesOnPathToEntrance(c, gohmaDoor, path, &err) == pcr::EntrancePath::Ok);
    RT_CHECK_EQ(entranceNames(path), std::string("Boss Entrance in Dragon Roost Cavern|Boss Entrance in Wind "
                                                 "Temple|Dungeon Entrance on Gale Isle"));
    RT_CHECK_EQ(pcr::outermostEntranceForExit(c, drc, &err), galeDoor);
    RT_CHECK_EQ(pcr::outermostEntranceForExit(c, molgera, &err), driMain);

    // A loop: Wind Temple behind its own boss door.
    c = pcr::EntranceConnections::defaults();
    c.connect(wtBossDoor, wt);
    RT_CHECK(pcr::entrancesOnPathToEntrance(c, wtBossDoor, path, &err) == pcr::EntrancePath::Loop);
    RT_CHECK_EQ(err, std::string("Entrances are in an infinite loop: Boss Entrance in Wind Temple"));
    RT_CHECK_EQ(pcr::outermostEntranceForExit(c, wt, &err), -1);

    // Every non-terminal exit (an exit some entrance is nested in) is a dungeon or a secret cave.
    for (const pcr::ZoneEntranceDef& e : pcr::zoneEntrances()) {
        if (e.isNested()) {
            const pcr::EntranceGroup g = pcr::zoneExits()[e.nestedIn].group;
            RT_CHECK(g == pcr::EntranceGroup::Dungeon || g == pcr::EntranceGroup::SecretCave);
        }
    }
    RT_CHECK_EQ(pcr::defaultConnectionExit(-1), -1);
    RT_CHECK_EQ(pcr::defaultConnectionExit(pcr::kNumZoneEntrances), -1);
    RT_CHECK_EQ(pcr::zoneEntranceIndex("Dungeon Entrance in Forsaken Fortress Sector"), -1);
    RT_CHECK_EQ(pcr::zoneExitIndex("Forsaken Fortress"), -1);
}

} // namespace

int main() {
    testDump();
    testAgreesWithLogic();
    testNesting();
    return rt::finish();
}
