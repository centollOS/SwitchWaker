// pc_rando_gen_entrances.cpp - entrance randomizer, placement and logic (docs/RANDOMIZER_PLAN.md step
// R6.11); see pc_rando_gen_entrances.h. Every list keeps Python's order and mutations (remove() takes
// the first occurrence, pop(0) the head), since the RNG draws depend on them (decision D1).
#include "pc_rando_gen_entrances.h"

#include <algorithm>
#include <cstring>

#include "pc_rando_entrances.h"

namespace pcr {

namespace {

using G = EntranceGroup;
using pc::rando::PyRandom;

bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
bool hasStr(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

const ZoneEntranceDef& EN(int i) { return zoneEntrances()[i]; }
const ZoneExitDef& EX(int i) { return zoneExits()[i]; }

struct EntranceRando {
    Logic& L;
    const Options& o;
    PyRandom& rng;
    const RequiredBosses& rb;
    const bool dacOnlyStart;
    EntranceRandoResult& out;

    EntranceConnections done = EntranceConnections::defaults();
    ExitLocationMap exitLocs;
    std::vector<std::string> entranceNamesNoReq, exitNamesNoReq;
    int safetyEntrance = -1;
    std::vector<int> bannedExits;
    std::vector<std::string> islandsWithBannedDungeon; // a set (membership only)
    std::string error;

    EntranceRando(Logic& l, const Options& opts, PyRandom& r, const RequiredBosses& b, bool dac,
                  EntranceRandoResult& res)
        : L(l), o(opts), rng(r), rb(b), dacOnlyStart(dac), out(res) {}

    bool fail(const std::string& msg) {
        if (error.empty()) {
            error = msg;
        }
        return false;
    }

    // list.remove(x): ValueError when absent.
    bool removeFrom(std::vector<int>& v, int x) {
        auto it = std::find(v.begin(), v.end(), x);
        if (it == v.end()) {
            return fail("ValueError: list.remove(x): x not in list");
        }
        v.erase(it);
        return true;
    }

    // entrance_connections[entrance_name] (dict order kept: the value is replaced in place).
    std::string* connectionFor(const char* entranceName) {
        for (auto& c : L.state().entranceConnections) {
            if (c.first == entranceName) {
                return &c.second;
            }
        }
        return nullptr;
    }

    // get_outermost_entrance_for_entrance: -1 for None; a loop raises (error set, returns -1).
    int outermost(int entrance) {
        std::string err;
        const int r = outermostEntranceForEntrance(done, entrance, &err);
        if (!err.empty()) {
            fail(err);
        }
        return r;
    }

    // ---- init_banned_exits ----
    bool initBannedExits() {
        if (o.required_bosses) {
            const IndexRange boss = exitGroup(G::Boss);
            for (int ex = boss.first; ex < boss.first + boss.count; ex++) {
                std::string name = EX(ex).name;
                if (!endsWith(name, " Boss Arena")) {
                    return fail("AssertionError: boss exit name");
                }
                name.resize(name.size() - std::strlen(" Boss Arena"));
                if (hasStr(rb.bannedBosses, name)) {
                    bannedExits.push_back(ex);
                }
            }
            const IndexRange dun = exitGroup(G::Dungeon);
            for (int ex = dun.first; ex < dun.first + dun.count; ex++) {
                if (hasStr(rb.bannedDungeons, EX(ex).name)) {
                    bannedExits.push_back(ex);
                }
            }
            const IndexRange mini = exitGroup(G::Miniboss);
            for (int ex = mini.first; ex < mini.first + mini.count; ex++) {
                std::string name = EX(ex).name;
                if (name == "Master Sword Chamber") {
                    continue; // Hyrule cannot be chosen as a banned dungeon.
                }
                if (!endsWith(name, " Miniboss Arena")) {
                    return fail("AssertionError: miniboss exit name");
                }
                name.resize(name.size() - std::strlen(" Miniboss Arena"));
                if (hasStr(rb.bannedDungeons, name)) {
                    bannedExits.push_back(ex);
                }
            }
        }
        if (!o.randomize_dungeon_entrances) {
            const IndexRange dun = entranceGroup(G::Dungeon);
            for (int en = dun.first; en < dun.first + dun.count; en++) {
                const std::string* c = connectionFor(EN(en).name);
                if (c == nullptr) {
                    return fail(std::string("KeyError: ") + EN(en).name);
                }
                if (hasStr(rb.bannedDungeons, *c) && !hasStr(islandsWithBannedDungeon, EN(en).islandName)) {
                    islandsWithBannedDungeon.emplace_back(EN(en).islandName);
                }
            }
        }
        return true;
    }

    // ---- check_if_one_exit_is_progress ----
    bool exitIsProgress(int ex, bool& progress) {
        const std::vector<int>& locs = exitLocs.locationsFor(ex);
        if (locs.empty()) {
            return fail(std::string("Could not find any item locations corresponding to zone exit: ") + EX(ex).name);
        }
        const std::vector<int>& banned = L.state().bannedLocations;
        std::vector<int> nonbanned;
        for (int loc : locs) {
            if (!has(banned, loc)) {
                nonbanned.push_back(loc);
            }
        }
        std::vector<int> prog;
        L.filterLocationsForProgression(nonbanned, prog);
        progress = !prog.empty();
        return true;
    }

    // ---- split_nonprogress_entrances_and_exits ----
    bool splitNonprogress(const std::vector<int>& entrances, const std::vector<int>& exits, std::vector<int>& npEn,
                          std::vector<int>& npEx) {
        npEn.clear();
        npEx.clear();
        for (int ex : exits) {
            bool p = false;
            if (!exitIsProgress(ex, p)) {
                return false;
            }
            if (!p) {
                npEx.push_back(ex);
            }
        }
        for (int en : entrances) {
            const int in = EN(en).nestedIn;
            if (in < 0) {
                continue;
            }
            bool take = has(npEx, in);
            if (!take && !has(exits, in)) {
                bool p = false;
                if (!exitIsProgress(in, p)) {
                    return false;
                }
                take = !p;
            }
            if (take) {
                npEn.push_back(en);
            }
        }
        std::vector<int> possible;
        for (int en : entrances) {
            if (EN(en).islandName != nullptr) {
                possible.push_back(en);
            }
        }
        const int ff = zoneEntranceIndex("Boss Entrance in Forsaken Fortress");
        if (has(possible, ff)) {
            const bool ffProgress = o.progression_dungeons && !hasStr(rb.bannedDungeons, "Forsaken Fortress");
            if (!ffProgress) {
                npEn.push_back(ff);
            }
            if (!removeFrom(possible, ff)) {
                return false;
            }
        }
        if (safetyEntrance >= 0) {
            if (!removeFrom(possible, safetyEntrance)) {
                return false;
            }
            if (o.required_bosses) {
                std::vector<int> kept;
                for (int en : possible) {
                    if (std::strcmp(EN(en).islandName, EN(safetyEntrance).islandName) != 0) {
                        kept.push_back(en);
                    }
                }
                possible.swap(kept);
            }
        }
        const int needed = (int)npEx.size() - (int)npEn.size();
        if (needed > (int)possible.size()) {
            return fail("Not enough island entrances left to split entrances.");
        }
        for (int i = 0; i < needed; i++) {
            npEn.push_back(possible.front());
            possible.erase(possible.begin());
        }
        if (npEn.size() != npEx.size()) {
            return fail("AssertionError: nonprogress entrances and exits differ in number");
        }
        return true;
    }

    // ---- randomize_one_set_of_exits ----
    bool randomizeOneSetOfExits(const std::vector<int>& entrances, const std::vector<int>& exits,
                                const std::vector<int>& terminal) {
        std::vector<int> remEn = entrances;
        std::vector<int> remEx = exits;

        bool doingBanned = false;
        for (int ex : exits) {
            if (has(bannedExits, ex)) {
                doingBanned = true;
            }
        }
        if (o.required_bosses && !doingBanned) {
            // Entrances on an island that already holds a banned dungeon go first.
            std::vector<int> notUnique;
            for (int en : entrances) {
                if (EN(en).isNested()) {
                    continue;
                }
                if (hasStr(islandsWithBannedDungeon, EN(en).islandName)) {
                    notUnique.push_back(en);
                }
            }
            for (int en : notUnique) {
                if (!removeFrom(remEn, en)) {
                    return false;
                }
            }
            remEn.insert(remEn.begin(), notUnique.begin(), notUnique.end());
        }
        if (safetyEntrance >= 0 && has(remEn, safetyEntrance)) {
            removeFrom(remEn, safetyEntrance);
            remEn.insert(remEn.begin(), safetyEntrance);
        }

        while (!remEn.empty()) {
            // Only entrances already reachable from the sea (no loops).
            std::vector<int> possibleEn;
            for (int en : remEn) {
                const int outer = outermost(en);
                if (!error.empty()) {
                    return false;
                }
                if (outer >= 0) {
                    possibleEn.push_back(en);
                }
            }
            if (possibleEn.empty()) {
                return fail("IndexError: pop from empty list");
            }
            const int en = possibleEn.front();
            possibleEn.erase(possibleEn.begin());
            removeFrom(remEn, en);

            std::vector<int> possibleEx;
            if (en == safetyEntrance) {
                for (int ex : remEx) {
                    if (hasStr(exitNamesNoReq, EX(ex).name)) {
                        possibleEx.push_back(ex);
                    }
                }
            } else {
                possibleEx = remEx;
            }
            if (possibleEn.empty() && !remEn.empty()) {
                // The last reachable entrance cannot take a terminal exit.
                std::vector<int> kept;
                for (int ex : possibleEx) {
                    if (!has(terminal, ex)) {
                        kept.push_back(ex);
                    }
                }
                possibleEx.swap(kept);
            }
            if (o.required_bosses && EN(en).islandName != nullptr && !doingBanned &&
                hasStr(islandsWithBannedDungeon, EN(en).islandName)) {
                std::vector<int> kept;
                for (int ex : possibleEx) {
                    if (has(terminal, ex) && EX(ex).group != G::Dungeon && EX(ex).group != G::Boss) {
                        kept.push_back(ex);
                    }
                }
                possibleEx.swap(kept);
            }
            if (possibleEx.empty()) {
                return fail(std::string("No valid exits to place for entrance: ") + EN(en).name);
            }

            // WWRandomizer.weighted_choice: each exit's weight is the product of the matching factors.
            std::vector<uint64_t> weights;
            weights.reserve(possibleEx.size());
            for (int ex : possibleEx) {
                const G g = EX(ex).group;
                uint64_t w = 1;
                if (EN(en).group == G::Boss) {
                    w *= g == G::Dungeon ? 10 : 1;
                    w *= g == G::Boss ? 5 : 1;
                } else if (EN(en).group == G::Miniboss) {
                    w *= g == G::Dungeon ? 12 : 1;
                    w *= g == G::Miniboss ? 6 : 1;
                    w *= g == G::FairyFountain ? 2 : 1;
                } else {
                    w *= g == G::SecretCave ? 7 : 1;
                    w *= g == G::Dungeon ? 3 : 1;
                }
                weights.push_back(w);
            }
            const int ex = possibleEx[rng.weightedIndex(weights.data(), weights.size())];
            removeFrom(remEx, ex);

            std::string* c = connectionFor(EN(en).name);
            if (c == nullptr) {
                return fail(std::string("KeyError: ") + EN(en).name);
            }
            *c = EX(ex).name;
            done.connect(en, ex);

            if (has(bannedExits, ex) && (EX(ex).group == G::Dungeon || EX(ex).group == G::Boss)) {
                // Islands with a banned dungeon or boss (miniboss exits may share an island).
                const int outer = outermost(en);
                if (outer < 0) {
                    return fail(error.empty() ? "AssertionError: banned exit not reachable from the sea" : error);
                }
                if (!hasStr(islandsWithBannedDungeon, EN(outer).islandName)) {
                    islandsWithBannedDungeon.emplace_back(EN(outer).islandName);
                }
            }
        }
        return true;
    }

    // ---- randomize_one_set_of_entrances ----
    bool randomizeOneSet(std::vector<int> entrances, const std::vector<int>& exits) {
        for (int en : entrances) {
            if (done.entranceToExit[en] < 0) {
                return fail(std::string("KeyError: ") + EN(en).name);
            }
            done.entranceToExit[en] = -1;
        }
        for (int ex : exits) {
            if (done.exitToEntrance[ex] < 0) {
                return fail(std::string("KeyError: ") + EX(ex).name);
            }
            done.exitToEntrance[ex] = -1;
        }
        bool doingDungeons = false, doingCaves = false;
        for (int ex : exits) {
            doingDungeons |= EX(ex).group == G::Dungeon;
            doingCaves |= EX(ex).group == G::SecretCave;
        }

        rng.shuffle(entrances);

        bool progressDacStart = false;
        if (dacOnlyStart) {
            if (doingDungeons && o.progression_dungeons) {
                progressDacStart = true;
            }
            if (doingCaves && (o.progression_puzzle_secret_caves || o.progression_combat_secret_caves ||
                               o.progression_savage_labyrinth)) {
                progressDacStart = true;
            }
        }
        safetyEntrance = -1;
        if (progressDacStart) {
            std::vector<int> possible;
            for (int en : entrances) {
                if (hasStr(entranceNamesNoReq, EN(en).name)) {
                    possible.push_back(en);
                }
            }
            if (possible.empty()) {
                return fail("IndexError: Cannot choose from an empty sequence");
            }
            safetyEntrance = rng.choice(possible);
        }

        // Terminal exits per set (an exit with a randomized entrance nested in it is not terminal).
        std::vector<int> nonTerminal;
        for (int en : entrances) {
            const int in = EN(en).nestedIn;
            if (in >= 0 && !has(nonTerminal, in)) {
                nonTerminal.push_back(in);
            }
        }
        std::vector<int> terminal;
        for (int ex : exits) {
            if (!has(nonTerminal, ex)) {
                terminal.push_back(ex);
            }
        }

        std::vector<int> remEn = entrances;
        std::vector<int> remEx = exits;
        std::vector<int> npEn, npEx;
        if (!splitNonprogress(remEn, remEx, npEn, npEx)) {
            return false;
        }
        if (!npEn.empty()) {
            for (int en : npEn) {
                if (!removeFrom(remEn, en)) {
                    return false;
                }
            }
            for (int ex : npEx) {
                if (!removeFrom(remEx, ex)) {
                    return false;
                }
            }
            if (!randomizeOneSetOfExits(npEn, npEx, terminal)) {
                return false;
            }
        }
        return randomizeOneSetOfExits(remEn, remEx, terminal);
    }

    // get_entrance_zone_for_boss.
    bool entranceZoneForBoss(const std::string& boss, std::string& island) {
        const int ex = zoneExitIndex(boss + " Boss Arena");
        if (ex < 0) {
            return fail("KeyError: " + boss + " Boss Arena");
        }
        std::string err;
        const int outer = outermostEntranceForExit(done, ex, &err);
        if (outer < 0) {
            return fail(err.empty() ? "AttributeError: 'NoneType' object has no attribute 'island_name'" : err);
        }
        island = EN(outer).islandName;
        return true;
    }

    // ---- finalize_all_randomized_sets_of_entrances ----
    bool finalize() {
        std::vector<int> nonTerminal;
        for (int en = 0; en < kNumZoneEntrances; en++) {
            const int in = EN(en).nestedIn;
            if (in >= 0 && !has(nonTerminal, in)) {
                nonTerminal.push_back(in);
            }
        }
        out.nestedEntrancePaths.clear();
        for (int ex = 0; ex < kNumZoneExits; ex++) {
            if (has(nonTerminal, ex)) {
                continue;
            }
            const int en = done.exitToEntrance[ex];
            if (en < 0) {
                return fail(std::string("KeyError: ") + EX(ex).name);
            }
            std::vector<int> seen;
            std::string err;
            const EntrancePath r = entrancesOnPathToEntrance(done, en, seen, &err);
            if (r == EntrancePath::Loop) {
                return fail(err);
            }
            if (r == EntrancePath::Undecided) {
                return fail("TypeError: 'NoneType' object is not iterable");
            }
            std::vector<std::string> path;
            path.emplace_back(EX(ex).name);
            for (int e : seen) {
                path.emplace_back(EN(e).name);
            }
            std::reverse(path.begin(), path.end());
            out.nestedEntrancePaths.push_back(std::move(path));
        }

        L.updateEntranceConnectionMacros();

        if (o.required_bosses) {
            std::vector<std::string> bannedIslands, requiredIslands;
            for (const std::string& b : rb.bannedBosses) {
                std::string island;
                if (!entranceZoneForBoss(b, island)) {
                    return false;
                }
                bannedIslands.push_back(island);
            }
            for (const std::string& b : rb.requiredBosses) {
                std::string island;
                if (!entranceZoneForBoss(b, island)) {
                    return false;
                }
                requiredIslands.push_back(island);
            }
            for (const std::string& i : bannedIslands) {
                if (hasStr(requiredIslands, i)) {
                    return fail("AssertionError: a banned and a required boss share the island " + i);
                }
            }
        }
        return true;
    }

    bool run() {
        out.nestingEnabled = entranceNestingEnabled(o);
        entranceNamesNoReq = entranceNamesWithNoRequirements(o);
        exitNamesNoReq = exitNamesWithNoRequirements(o);
        if (!buildExitLocationMap(exitLocs, &error)) {
            return false;
        }
        // EntranceRandomizer.__init__ resets entrance_connections to the defaults.
        for (int en = 0; en < kNumZoneEntrances; en++) {
            std::string* c = connectionFor(EN(en).name);
            if (c == nullptr) {
                return fail(std::string("KeyError: ") + EN(en).name);
            }
            *c = EX(defaultConnectionExit(en)).name;
        }

        if (!initBannedExits()) {
            return false;
        }
        std::vector<EntranceSet> sets;
        if (!entranceSetsToBeRandomized(o, sets, &error)) {
            return false;
        }
        for (const EntranceSet& s : sets) {
            if (!randomizeOneSet(s.entrances, s.exits)) {
                return false;
            }
        }
        return finalize();
    }
};

} // namespace

bool randomizeEntrances(Logic& logic, const Options& o, PyRandom& rng, const RequiredBosses& bosses,
                        bool dungeonsAndCavesOnlyStart, EntranceRandoResult& out, std::string* err) {
    out = EntranceRandoResult();
    EntranceRando r(logic, o, rng, bosses, dungeonsAndCavesOnlyStart, out);
    if (!r.run() || logic.failed()) {
        if (err != nullptr) {
            *err = !r.error.empty() ? r.error : logic.error();
        }
        return false;
    }
    return true;
}

} // namespace pcr
