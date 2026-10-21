// pc_rando_gen_hints.cpp - the hint generator (docs/RANDOMIZER_PLAN.md step R5.1); see
// pc_rando_gen_hints.h. Function names follow wwrando 1.10.0_9775811's randomizers/hints.py and
// randomizers/entrances.py; comments name the Python they port.
#include "pc_rando_gen_hints.h"

#include <math.h>
#include <string.h>

#include <algorithm>

#include "pc_rando_data.h"
#include "pc_rando_gen_items.h"

namespace pcr {

using pc::rando::PyRandom;

const int kHohoIndexToIsland[kHohoCount] = {34, 14, 44, 1, 5, 33, 3, 43, 31, 46};

const char* hintPlacementName(HintPlacement p) {
    switch (p) {
    case HintPlacement::Fishmen:
        return "fishmen_hints";
    case HintPlacement::Hoho:
        return "hoho_hints";
    case HintPlacement::Korl:
        return "korl_hints";
    }
    return "";
}

const char* hintTypeName(HintType t) {
    switch (t) {
    case HintType::Path:
        return "path";
    case HintType::Barren:
        return "barren";
    case HintType::Item:
        return "item";
    case HintType::Location:
        return "location";
    case HintType::FixedLocation:
        return "fixed_location";
    }
    return "";
}

const char* hintImportanceName(HintImportance i) {
    switch (i) {
    case HintImportance::Required:
        return "required";
    case HintImportance::PossiblyRequired:
        return "possibly required";
    case HintImportance::NotRequired:
        return "not required";
    case HintImportance::None:
        break;
    }
    return "";
}

const std::vector<Hint>* HintsResult::placement(HintPlacement p) const {
    for (const auto& e : placements) {
        if (e.first == p) {
            return &e.second;
        }
    }
    return nullptr;
}

namespace {

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool hasInt(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

bool hasStr(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

// list.remove(x): the first occurrence.
template <class T> void listRemove(std::vector<T>& v, const T& x) {
    auto it = std::find(v.begin(), v.end(), x);
    if (it != v.end()) {
        v.erase(it);
    }
}

// --- entrances.py data that LogicState does not carry -------------------------------------------

// ZoneEntrance.island_name of the island (not nested) entrances.
struct EntranceIsland {
    const char* entrance;
    const char* island;
};
const EntranceIsland kEntranceIslands[] = {
    {"Dungeon Entrance on Dragon Roost Island", "Dragon Roost Island"},
    {"Dungeon Entrance in Forest Haven Sector", "Forest Haven"},
    {"Dungeon Entrance in Tower of the Gods Sector", "Tower of the Gods Sector"},
    {"Dungeon Entrance on Headstone Island", "Headstone Island"},
    {"Dungeon Entrance on Gale Isle", "Gale Isle"},
    {"Miniboss Entrance in Hyrule Castle", "Tower of the Gods Sector"},
    {"Boss Entrance in Forsaken Fortress", "Forsaken Fortress Sector"},
    {"Secret Cave Entrance on Outset Island", "Outset Island"},
    {"Secret Cave Entrance on Dragon Roost Island", "Dragon Roost Island"},
    {"Secret Cave Entrance on Fire Mountain", "Fire Mountain"},
    {"Secret Cave Entrance on Ice Ring Isle", "Ice Ring Isle"},
    {"Secret Cave Entrance on Private Oasis", "Private Oasis"},
    {"Secret Cave Entrance on Needle Rock Isle", "Needle Rock Isle"},
    {"Secret Cave Entrance on Angular Isles", "Angular Isles"},
    {"Secret Cave Entrance on Boating Course", "Boating Course"},
    {"Secret Cave Entrance on Stone Watcher Island", "Stone Watcher Island"},
    {"Secret Cave Entrance on Overlook Island", "Overlook Island"},
    {"Secret Cave Entrance on Bird's Peak Rock", "Bird's Peak Rock"},
    {"Secret Cave Entrance on Pawprint Isle", "Pawprint Isle"},
    {"Secret Cave Entrance on Pawprint Isle Side Isle", "Pawprint Isle"},
    {"Secret Cave Entrance on Diamond Steppe Island", "Diamond Steppe Island"},
    {"Secret Cave Entrance on Bomb Island", "Bomb Island"},
    {"Secret Cave Entrance on Rock Spire Isle", "Rock Spire Isle"},
    {"Secret Cave Entrance on Shark Island", "Shark Island"},
    {"Secret Cave Entrance on Cliff Plateau Isles", "Cliff Plateau Isles"},
    {"Secret Cave Entrance on Horseshoe Island", "Horseshoe Island"},
    {"Secret Cave Entrance on Star Island", "Star Island"},
    {"Fairy Fountain Entrance on Outset Island", "Outset Island"},
    {"Fairy Fountain Entrance on Thorned Fairy Island", "Thorned Fairy Island"},
    {"Fairy Fountain Entrance on Eastern Fairy Island", "Eastern Fairy Island"},
    {"Fairy Fountain Entrance on Western Fairy Island", "Western Fairy Island"},
    {"Fairy Fountain Entrance on Southern Fairy Island", "Southern Fairy Island"},
    {"Fairy Fountain Entrance on Northern Fairy Island", "Northern Fairy Island"},
};

// ZoneExit.zone_name of the exits that have one, in ZoneExit.all order.
struct ExitZone {
    const char* exit;
    const char* zone;
};
const ExitZone kExitZones[] = {
    {"Dragon Roost Cavern", "Dragon Roost Cavern"},
    {"Forbidden Woods", "Forbidden Woods"},
    {"Tower of the Gods", "Tower of the Gods"},
    {"Earth Temple", "Earth Temple"},
    {"Wind Temple", "Wind Temple"},
    {"Savage Labyrinth", "Outset Island"},
    {"Dragon Roost Island Secret Cave", "Dragon Roost Island"},
    {"Fire Mountain Secret Cave", "Fire Mountain"},
    {"Ice Ring Isle Secret Cave", "Ice Ring Isle"},
    {"Cabana Labyrinth", "Private Oasis"},
    {"Needle Rock Isle Secret Cave", "Needle Rock Isle"},
    {"Angular Isles Secret Cave", "Angular Isles"},
    {"Boating Course Secret Cave", "Boating Course"},
    {"Stone Watcher Island Secret Cave", "Stone Watcher Island"},
    {"Overlook Island Secret Cave", "Overlook Island"},
    {"Bird's Peak Rock Secret Cave", "Bird's Peak Rock"},
    {"Pawprint Isle Chuchu Cave", "Pawprint Isle"},
    {"Diamond Steppe Island Warp Maze Cave", "Diamond Steppe Island"},
    {"Bomb Island Secret Cave", "Bomb Island"},
    {"Rock Spire Isle Secret Cave", "Rock Spire Isle"},
    {"Shark Island Secret Cave", "Shark Island"},
    {"Cliff Plateau Isles Secret Cave", "Cliff Plateau Isles"},
    {"Horseshoe Island Secret Cave", "Horseshoe Island"},
    {"Star Island Secret Cave", "Star Island"},
    {"Thorned Fairy Fountain", "Thorned Fairy Island"},
    {"Eastern Fairy Fountain", "Eastern Fairy Island"},
    {"Western Fairy Fountain", "Western Fairy Island"},
    {"Southern Fairy Fountain", "Southern Fairy Island"},
    {"Northern Fairy Fountain", "Northern Fairy Island"},
};

const char* entranceIsland(const std::string& entrance) {
    for (const EntranceIsland& e : kEntranceIslands) {
        if (entrance == e.entrance) {
            return e.island;
        }
    }
    return nullptr;
}

const char* exitZone(const std::string& exit) {
    for (const ExitZone& e : kExitZones) {
        if (exit == e.exit) {
            return e.zone;
        }
    }
    return nullptr;
}

// ZoneEntrance.nested_in's unique_name, or nullptr for an island entrance.
const std::string* nestedIn(const LogicState& st, const std::string& entrance) {
    for (const auto& p : st.nestedEntrances) {
        if (p.first == entrance) {
            return &p.second;
        }
    }
    return nullptr;
}

// done_exits_to_entrances[exit] (built from entrance_connections; the last assignment wins).
const std::string* entranceForExit(const LogicState& st, const std::string& exit) {
    const std::string* found = nullptr;
    for (const auto& c : st.entranceConnections) {
        if (c.second == exit) {
            found = &c.first;
        }
    }
    return found;
}

// get_all_entrances_on_path_to_entrance. False: undecided (None). *err on an infinite loop.
bool entrancesOnPath(const LogicState& st, std::string entrance, std::vector<std::string>& seen, std::string* err) {
    seen.clear();
    for (;;) {
        const std::string* in = nestedIn(st, entrance);
        if (in == nullptr) {
            break;
        }
        if (hasStr(seen, entrance)) {
            std::string path;
            for (size_t i = 0; i < seen.size(); i++) {
                path += (i ? ", " : "") + seen[i];
            }
            if (err != nullptr) {
                *err = "Entrances are in an infinite loop: " + path;
            }
            return false;
        }
        seen.push_back(entrance);
        const std::string* outer = entranceForExit(st, *in);
        if (outer == nullptr) {
            return false; // undecided
        }
        entrance = *outer;
    }
    seen.push_back(entrance);
    return true;
}

} // namespace

// --- entrances.py helpers --------------------------------------------------------------------------

bool isItemLocationBehindRandomizableEntrance(const Logic& logic, int loc) {
    const std::string& zone = logic.locationZone(loc);
    if (zone == "Ganon's Tower" || zone == "Mailbox") {
        return false;
    }
    const uint32_t types = logic.locationTypes(loc);
    if (zone == "Forsaken Fortress" && !(types & LOC_BOSS)) {
        return false;
    }
    if (types & LOC_BIG_OCTO) {
        return false;
    }
    // ENTRANCE_RANDOMIZABLE_ITEM_LOCATION_TYPES
    return (types & (LOC_DUNGEON | LOC_PUZZLE_SECRET_CAVE | LOC_COMBAT_SECRET_CAVE | LOC_SAVAGE_LABYRINTH |
                     LOC_GREAT_FAIRY)) != 0;
}

std::string zoneExitForItemLocation(const Logic& logic, int loc) {
    if (!isItemLocationBehindRandomizableEntrance(logic, loc)) {
        return std::string();
    }
    const char* name = logic.locationName(loc);
    for (const NameText& o : exit_overrides()) {
        if (strcmp(o.name, name) == 0) {
            return o.text;
        }
    }
    // possible_exits: the exits whose zone_name is the location's zone (zone names are unique among
    // the exits, so wwrando's "Multiple zone exits" error cannot happen with its data).
    const std::string& zone = logic.locationZone(loc);
    for (const ExitZone& e : kExitZones) {
        if (zone == e.zone) {
            return e.exit;
        }
    }
    return std::string();
}

bool entranceZoneForItemLocation(const Logic& logic, int loc, std::string& out, std::string* err) {
    if (!isItemLocationBehindRandomizableEntrance(logic, loc)) {
        out = logic.locationZone(loc);
        return true;
    }
    const std::string exit = zoneExitForItemLocation(logic, loc);
    if (exit.empty()) {
        if (err != nullptr) {
            *err = std::string("Could not determine entrance zone for item location: ") + logic.locationName(loc);
        }
        return false;
    }
    const std::string* entrance = entranceForExit(logic.state(), exit);
    std::vector<std::string> path;
    std::string e;
    if (entrance == nullptr || !entrancesOnPath(logic.state(), *entrance, path, &e)) {
        if (err != nullptr) {
            *err = !e.empty() ? e : "'NoneType' object has no attribute 'island_name'";
        }
        return false;
    }
    const char* island = entranceIsland(path.back());
    out = island != nullptr ? island : "";
    return true;
}

bool allZonesForItemLocation(const Logic& logic, int loc, std::vector<std::string>& out, std::string* err) {
    out.clear();
    auto add = [&](const std::string& z) {
        if (!hasStr(out, z)) {
            out.push_back(z);
        }
    };
    const std::string zone = logic.locationZone(loc);
    const std::string name = logic.locationName(loc);
    const char* via = nullptr;
    if (name == "Mailbox - Letter from Baito") {
        via = "Earth Temple - Jalhalla Heart Container";
    } else if (name == "Mailbox - Letter from Orca") {
        via = "Forbidden Woods - Kalle Demos Heart Container";
    } else if (name == "Mailbox - Letter from Aryll" || name == "Mailbox - Letter from Tingle") {
        via = "Forsaken Fortress - Helmaroc King Heart Container";
    } else if (name == "The Great Sea - Withered Trees") {
        via = "Cliff Plateau Isles - Highest Isle";
    }
    if (via != nullptr) {
        std::vector<std::string> sub;
        if (!allZonesForItemLocation(logic, logic.locationIndex(via), sub, err)) {
            return false;
        }
        add(zone);
        for (const std::string& z : sub) {
            add(z);
        }
        return true;
    }
    if (!isItemLocationBehindRandomizableEntrance(logic, loc)) {
        add(zone);
        return true;
    }
    const std::string exit = zoneExitForItemLocation(logic, loc);
    if (exit.empty()) {
        if (err != nullptr) {
            *err = "Could not determine entrance zone for item location: " + name;
        }
        return false;
    }
    const std::string* entrance = entranceForExit(logic.state(), exit);
    std::vector<std::string> path;
    std::string e;
    if (entrance == nullptr || !entrancesOnPath(logic.state(), *entrance, path, &e)) {
        if (err != nullptr) {
            *err = !e.empty() ? e : "'NoneType' object is not iterable";
        }
        return false;
    }
    for (const std::string& ent : path) {
        if (const char* island = entranceIsland(ent)) {
            // The Forsaken Fortress boss door is part of the dungeon.
            add(strcmp(island, "Forsaken Fortress Sector") == 0 ? std::string("Forsaken Fortress")
                                                                : std::string(island));
        } else if (const std::string* in = nestedIn(logic.state(), ent)) {
            if (hasStr(logic.state().dungeonExits, *in)) {
                const char* z = exitZone(*in);
                add(z != nullptr ? z : *in);
            }
        }
    }
    if (logic.isDungeonLocation(loc)) {
        add(zone);
    }
    return true;
}

// --- hints.py --------------------------------------------------------------------------------------

bool checkItemCanBeHintedAt(const Logic& logic, int item) {
    if (item < 0 || !hasInt(logic.allProgressItems, item)) {
        return false;
    }
    if (logic.isDungeonItem(item) && !logic.options().keylunacy) {
        return false;
    }
    if (endsWith(logic.name(item), " Trap Chest")) {
        return false;
    }
    return true;
}

namespace {

constexpr int kNumPaths = 8;
// DUNGEON_NAME_TO_BOSS_NAME and DUNGEON_NAME_TO_REQUIREMENT_NAME.
struct PathGoal {
    const char* dungeon;
    const char* boss;
    const char* requirement;
};
const PathGoal kPathGoals[kNumPaths] = {
    {"Dragon Roost Cavern", "Gohma", "Can Access Item Location \"Dragon Roost Cavern - Gohma Heart Container\""},
    {"Forbidden Woods", "Kalle Demos", "Can Access Item Location \"Forbidden Woods - Kalle Demos Heart Container\""},
    {"Tower of the Gods", "Gohdan", "Can Access Item Location \"Tower of the Gods - Gohdan Heart Container\""},
    {"Forsaken Fortress", "Helmaroc King",
     "Can Access Item Location \"Forsaken Fortress - Helmaroc King Heart Container\""},
    {"Earth Temple", "Jalhalla", "Can Access Item Location \"Earth Temple - Jalhalla Heart Container\""},
    {"Wind Temple", "Molgera", "Can Access Item Location \"Wind Temple - Molgera Heart Container\""},
    {"Hyrule", "Hyrule", "Can Access Hyrule"},
    {"Ganon's Tower", "Ganondorf", "Can Reach and Defeat Ganondorf"},
};

const PathGoal* pathGoal(const std::string& name) {
    for (const PathGoal& g : kPathGoals) {
        if (name == g.dungeon) {
            return &g;
        }
    }
    return nullptr;
}

// hints.py PlacedItem (a frozen dataclass; its fields identify the location, so equality is the
// location's).
struct PlacedItem {
    int loc;
    std::string zone;
    std::string entranceZone;
    std::string item;
    bool operator==(const PlacedItem& o) const { return loc == o.loc; }
};

using PathList = std::vector<std::pair<std::string, std::vector<PlacedItem>>>;

std::vector<PlacedItem>* pathFind(PathList& d, const std::string& k) {
    for (auto& e : d) {
        if (e.first == k) {
            return &e.second;
        }
    }
    return nullptr;
}

class HintsGen {
public:
    HintsGen(Logic& logic, PyRandom& rng, const HintsInput& in) : L(logic), rng(rng), in(in), opts(logic.options()) {}

    bool run(HintsResult& out);
    const std::string& errorText() const { return error; }

private:
    bool fail(const std::string& msg) {
        if (error.empty()) {
            error = msg;
        }
        return false;
    }
    bool ok() const { return error.empty() && !L.failed(); }

    int itemAt(int loc) const { return L.doneItemLocations[loc]; }
    bool isBanned(int loc) const { return banned[loc] != 0; }

    std::string entranceZone(int loc) {
        std::string z, e;
        if (!entranceZoneForItemLocation(L, loc, z, &e)) {
            fail(e);
        }
        return z;
    }
    const std::vector<std::string>& allZones(int loc) {
        if (!zonesDone[loc]) {
            std::string e;
            if (!allZonesForItemLocation(L, loc, zonesCache[loc], &e)) {
                fail(e);
            }
            zonesDone[loc] = 1;
        }
        return zonesCache[loc];
    }

    bool checkLocationRequiredForPaths(int locToCheck, const std::vector<std::string>& paths, std::vector<uint8_t>& met);
    bool getRequiredLocationsForPaths(const std::vector<int>& progress, PathList& out);
    bool getPathHint(std::vector<PlacedItem>& unhinted, const std::vector<int>& hinted, const std::string& pathName,
                     Hint& hint, int& loc);
    void getBarrenZones(const std::vector<int>& progress, const std::vector<int>& hintedRemote,
                        std::vector<std::string>& out);
    void filterOutHintedBarrenLocations(const std::vector<int>& in, const std::vector<Hint>& barrens,
                                        std::vector<int>& out);
    HintImportance importanceFor(int loc) const;
    bool legalItemHint(int loc, const std::vector<uint8_t>& isProgress, const std::vector<int>& previously) const;
    void getLegalItemHints(const std::vector<int>& progress, const std::vector<Hint>& barrens,
                           const std::vector<int>& previously, std::vector<int>& out);
    Hint getItemHint(std::vector<int>& hintable, int& loc);
    void getLegalLocationHints(const std::vector<int>& progress, const std::vector<Hint>& barrens,
                               const std::vector<int>& previously, std::vector<int>& remote,
                               std::vector<int>& standard);
    Hint getLocationHint(std::vector<int>& hintable, int& loc, bool ignoreImportance);
    bool generateOctoFairyHint(Hint& out);
    Hint generateSavageLabyrinthHint(const char* loc);
    bool generateHints(std::vector<Hint>& out);
    int hohoLookIsland(const std::vector<Hint>& hints);

    Logic& L;
    PyRandom& rng;
    const HintsInput& in;
    const Options& opts;
    std::string error;

    Logic path;
    Logic::PlaythroughState pathInitial;
    std::vector<uint8_t> banned;
    std::vector<uint8_t> pathLocations;   // self.path_locations (a set)
    std::vector<uint8_t> barrenLocations; // self.barren_locations (a set)
    bool setsReady = false;
    std::vector<std::vector<std::string>> zonesCache;
    std::vector<uint8_t> zonesDone;
};

// check_location_required_for_paths. met[i] is requirements_met[paths[i]]; false (met empty) for
// the {} wwrando returns when the location's item is not a progress item.
bool HintsGen::checkLocationRequiredForPaths(int locToCheck, const std::vector<std::string>& paths,
                                             std::vector<uint8_t>& met) {
    met.clear();
    const int item = itemAt(locToCheck);
    if (!hasInt(L.allProgressItems, item)) {
        return false;
    }
    path.loadSimulatedPlaythroughState(pathInitial);
    const int n = L.locationCount();
    std::vector<uint8_t> previously(n, 0);
    std::vector<int> accessible, sphere, keyLocs;
    std::vector<std::pair<int, int>> progressInSphere; // location -> item (path logic ids)
    while (!path.unplacedProgressItems.empty()) {
        path.getAccessibleRemainingLocations(true, accessible);
        accessible.erase(std::remove_if(accessible.begin(), accessible.end(), [&](int l) { return isBanned(l); }),
                         accessible.end());
        sphere.clear();
        for (int l : accessible) {
            if (!previously[l]) {
                sphere.push_back(l);
            }
        }
        if (sphere.empty()) {
            break;
        }
        if (!opts.keylunacy) {
            keyLocs.clear();
            for (int l : sphere) {
                const int pre = L.prerandomizedItem(l);
                if (pre >= 0 && endsWith(L.name(pre), " Small Key")) {
                    keyLocs.push_back(l);
                }
            }
            if (!keyLocs.empty()) {
                for (int l : keyLocs) {
                    const int key = path.item(L.name(L.prerandomizedItem(l)));
                    path.addOwnedItem(key);
                    if (l == locToCheck) {
                        path.removeOwnedItem(key);
                    }
                    previously[l] = 1;
                }
                continue;
            }
        }
        progressInSphere.clear();
        for (int l : sphere) {
            const int it = path.item(L.name(itemAt(l)));
            if (hasInt(path.allProgressItems, it)) {
                progressInSphere.emplace_back(l, it);
            }
        }
        for (const auto& p : progressInSphere) {
            path.addOwnedItem(p.second);
            if (p.first == locToCheck) {
                path.removeOwnedItem(p.second);
            }
        }
        std::fill(previously.begin(), previously.end(), 0);
        for (int l : accessible) {
            previously[l] = 1;
        }
    }
    for (const std::string& p : paths) {
        const PathGoal* g = pathGoal(p);
        met.push_back(g != nullptr && !path.req().checkRequirementMet(g->requirement) ? 1 : 0);
    }
    return true;
}

// get_required_locations_for_paths.
bool HintsGen::getRequiredLocationsForPaths(const std::vector<int>& progress, PathList& out) {
    std::vector<std::string> goals = in.requiredDungeons;
    goals.push_back("Hyrule");
    goals.push_back("Ganon's Tower");
    out.clear();
    for (const std::string& g : goals) {
        out.emplace_back(g, std::vector<PlacedItem>());
    }
    std::vector<uint8_t> met;
    for (int loc : progress) {
        const int item = itemAt(loc);
        if (isBanned(loc)) {
            continue;
        }
        if (!hasInt(L.allProgressItems, item)) {
            continue;
        }
        PlacedItem pi{loc, L.locationZone(loc), entranceZone(loc), L.name(item)};
        if (!ok()) {
            return false;
        }
        if (checkLocationRequiredForPaths(loc, goals, met)) {
            for (size_t i = 0; i < goals.size(); i++) {
                if (met[i]) {
                    pathFind(out, goals[i])->push_back(pi);
                }
            }
        }
        if (path.failed()) {
            return fail(path.error());
        }
        for (const std::string& d : in.requiredDungeons) {
            for (const PlacedItem& other : *pathFind(out, d)) {
                for (const char* nd : {"Hyrule", "Ganon's Tower"}) {
                    std::vector<PlacedItem>& lst = *pathFind(out, nd);
                    if (std::find(lst.begin(), lst.end(), other) == lst.end()) {
                        lst.push_back(other);
                    }
                }
            }
        }
    }
    return true;
}

// get_path_hint. False: (None, None).
bool HintsGen::getPathHint(std::vector<PlacedItem>& unhinted, const std::vector<int>& hinted,
                           const std::string& pathName, Hint& hint, int& loc) {
    PlacedItem pi;
    for (;;) {
        if (unhinted.empty()) {
            return false;
        }
        pi = rng.choice(unhinted);
        listRemove(unhinted, pi);
        if (!hasInt(hinted, pi.loc)) {
            break;
        }
    }
    loc = pi.loc;
    hint = Hint();
    hint.type = HintType::Path;
    hint.place = L.isDungeonLocation(pi.loc) ? pi.zone : pi.entranceZone;
    const PathGoal* g = pathGoal(pathName);
    hint.reward = g != nullptr ? g->boss : pathName;
    return true;
}

// get_barren_zones (also sets self.barren_locations).
void HintsGen::getBarrenZones(const std::vector<int>& progressIn, const std::vector<int>& hintedRemote,
                              std::vector<std::string>& out) {
    const int n = L.locationCount();
    std::vector<uint8_t> progress(n, 0);
    for (int l : progressIn) {
        if (!isBanned(l)) {
            progress[l] = 1;
        }
    }
    // progress_items: item -> its progress locations (a dict; only membership and the per-item
    // location sets matter below).
    std::vector<std::vector<int>> locsOfItem(L.req().nameCount());
    for (int l = 0; l < n; l++) {
        if (!progress[l]) {
            continue;
        }
        const int item = itemAt(l);
        if (hasInt(L.allProgressItems, item)) {
            if (item >= (int)locsOfItem.size()) {
                locsOfItem.resize(item + 1);
            }
            locsOfItem[item].push_back(l);
        }
    }
    auto inProgressItems = [&](int item) {
        return item >= 0 && item < (int)locsOfItem.size() && !locsOfItem[item].empty();
    };

    path.loadSimulatedPlaythroughState(pathInitial);
    std::vector<int> needed, other;
    path.getItemNamesByReqName("Can Reach and Defeat Ganondorf", needed);
    std::vector<int> queue; // the deque, as main-logic ids
    for (int id : needed) {
        queue.push_back(L.item(path.name(id)));
    }
    std::vector<uint8_t> checked;
    std::vector<uint8_t> useful(n, 0);
    for (size_t qi = 0; qi < queue.size(); qi++) {
        const int item = queue[qi];
        if (item < (int)checked.size() && checked[item]) {
            continue;
        }
        if (!inProgressItems(item)) {
            continue;
        }
        if (L.isDungeonItem(item) && !opts.keylunacy) {
            continue;
        }
        if (item >= (int)checked.size()) {
            checked.resize(item + 1, 0);
        }
        checked[item] = 1;
        for (int l : locsOfItem[item]) {
            path.getItemNamesByReqName(std::string("Can Access Item Location \"") + L.locationName(l) + "\"", other);
            for (int id : other) {
                queue.push_back(L.item(path.name(id)));
            }
            useful[l] = 1;
        }
    }
    barrenLocations.assign(n, 0);
    for (int l = 0; l < n; l++) {
        if (progress[l] && !useful[l]) {
            barrenLocations[l] = 1;
        }
    }
    // sorted() over location names, as wwrando iterates; the zone sets do not depend on the order.
    std::vector<std::string> usefulZones, barrenZones;
    for (int l = 0; l < n && ok(); l++) {
        if (useful[l]) {
            for (const std::string& z : allZones(l)) {
                if (!hasStr(usefulZones, z)) {
                    usefulZones.push_back(z);
                }
            }
        }
    }
    for (int l = 0; l < n && ok(); l++) {
        if (!barrenLocations[l] || hasInt(hintedRemote, l)) {
            continue;
        }
        for (const std::string& z : allZones(l)) {
            if (!hasStr(barrenZones, z)) {
                barrenZones.push_back(z);
            }
        }
    }
    out.clear();
    for (const std::string& z : barrenZones) {
        if (!hasStr(usefulZones, z)) {
            out.push_back(z);
        }
    }
    std::sort(out.begin(), out.end()); // sorted(): code point order = UTF-8 byte order
}

void HintsGen::filterOutHintedBarrenLocations(const std::vector<int>& locs, const std::vector<Hint>& barrens,
                                              std::vector<int>& out) {
    out.clear();
    for (int l : locs) {
        bool inBarren = false;
        for (const std::string& z : allZones(l)) {
            for (const Hint& h : barrens) {
                inBarren = inBarren || h.place == z;
            }
        }
        if (!inBarren) {
            out.push_back(l);
        }
    }
}

// get_importance_for_location.
HintImportance HintsGen::importanceFor(int loc) const {
    if (!hasInt(L.allProgressItems, itemAt(loc))) {
        return HintImportance::None;
    }
    if (pathLocations[loc]) {
        return HintImportance::Required;
    }
    if (barrenLocations[loc]) {
        return HintImportance::NotRequired;
    }
    return HintImportance::PossiblyRequired;
}

// check_is_legal_item_hint.
bool HintsGen::legalItemHint(int loc, const std::vector<uint8_t>& isProgress, const std::vector<int>& previously) const {
    if (!checkItemCanBeHintedAt(L, itemAt(loc))) {
        return false;
    }
    if (!isProgress[loc] || isBanned(loc) || hasInt(previously, loc)) {
        return false;
    }
    return true;
}

// get_legal_item_hints: done_item_locations order (the location order).
void HintsGen::getLegalItemHints(const std::vector<int>& progress, const std::vector<Hint>& barrens,
                                 const std::vector<int>& previously, std::vector<int>& out) {
    std::vector<uint8_t> isProgress(L.locationCount(), 0);
    for (int l : progress) {
        isProgress[l] = 1;
    }
    std::vector<int> hintable;
    for (int l = 0; l < L.locationCount(); l++) {
        if (legalItemHint(l, isProgress, previously)) {
            hintable.push_back(l);
        }
    }
    filterOutHintedBarrenLocations(hintable, barrens, out);
}

// get_item_hint (the caller checks that the list is not empty).
Hint HintsGen::getItemHint(std::vector<int>& hintable, int& loc) {
    loc = rng.choice(hintable);
    listRemove(hintable, loc);
    Hint h;
    h.type = HintType::Item;
    h.place = entranceZone(loc);
    h.reward = L.name(itemAt(loc));
    h.importance = importanceFor(loc);
    return h;
}

// get_legal_location_hints.
void HintsGen::getLegalLocationHints(const std::vector<int>& progress, const std::vector<Hint>& barrens,
                                     const std::vector<int>& previously, std::vector<int>& remote,
                                     std::vector<int>& standard) {
    std::vector<int> hintable;
    remote.clear();
    Span<LocationHint> lh = location_hints();
    for (int l : progress) {
        const char* name = L.locationName(l);
        for (const LocationHint& h : lh) {
            if (strcmp(h.location, name) == 0) {
                if (strcmp(h.type, "Remote") == 0) {
                    remote.push_back(l);
                } else if (strcmp(h.type, "Standard") == 0) {
                    hintable.push_back(l);
                }
                break;
            }
        }
    }
    if (!opts.prioritize_remote_hints) {
        hintable.insert(hintable.end(), remote.begin(), remote.end());
        remote.clear();
    }
    std::vector<int> filtered;
    for (int l : hintable) {
        if (isBanned(l) || hasInt(previously, l) || endsWith(L.name(itemAt(l)), " Trap Chest")) {
            continue;
        }
        filtered.push_back(l);
    }
    filterOutHintedBarrenLocations(filtered, barrens, standard);
}

// get_location_hint (the caller checks that the list is not empty).
Hint HintsGen::getLocationHint(std::vector<int>& hintable, int& loc, bool ignoreImportance) {
    loc = rng.choice(hintable);
    listRemove(hintable, loc);
    Hint h;
    h.type = HintType::Location;
    h.place = L.locationName(loc);
    h.reward = L.name(itemAt(loc));
    h.importance = ignoreImportance ? HintImportance::None : importanceFor(loc);
    return h;
}

// generate_octo_fairy_hint.
bool HintsGen::generateOctoFairyHint(Hint& out) {
    std::vector<int> progress, nonprogress, hintable;
    L.getProgressAndNonProgressLocations(progress, nonprogress);
    getLegalItemHints(progress, {}, {}, hintable);
    listRemove(hintable, L.locationIndex("Two-Eye Reef - Big Octo Great Fairy"));
    if (hintable.empty()) {
        return fail("No valid items to give hints for");
    }
    int loc = -1;
    out = getItemHint(hintable, loc);
    return ok();
}

// generate_savage_labyrinth_hint.
Hint HintsGen::generateSavageLabyrinthHint(const char* locName) {
    const int loc = L.locationIndex(locName);
    Hint h;
    h.type = HintType::FixedLocation;
    h.place = locName;
    if (loc >= 0) {
        h.reward = L.name(itemAt(loc));
        h.importance = importanceFor(loc);
    }
    return h;
}

// generate_hints.
bool HintsGen::generateHints(std::vector<Hint>& out) {
    std::vector<int> previously;
    std::vector<int> progress, nonprogress;
    L.getProgressAndNonProgressLocations(progress, nonprogress);
    if (L.failed()) {
        return fail(L.error());
    }

    // location_counter over all_world_areas.
    std::vector<std::pair<std::string, int>> counter;
    for (int l : progress) {
        const std::string area = L.isDungeonLocation(l) ? L.locationZone(l) : entranceZone(l);
        bool found = false;
        for (auto& c : counter) {
            if (c.first == area) {
                c.second++;
                found = true;
                break;
            }
        }
        if (!found) {
            counter.emplace_back(area, 1);
        }
    }
    if (!ok()) {
        return false;
    }
    auto count = [&](const std::string& z) {
        for (const auto& c : counter) {
            if (c.first == z) {
                return c.second;
            }
        }
        return 0;
    };

    // Remote location hints.
    std::vector<Hint> remoteHints;
    std::vector<int> remoteLocs;
    if (opts.prioritize_remote_hints) {
        std::vector<int> remote, standard;
        getLegalLocationHints(progress, {}, {}, remote, standard);
        while (!remote.empty() && (int)remoteHints.size() < opts.num_location_hints) {
            int loc = -1;
            remoteHints.push_back(getLocationHint(remote, loc, true));
            remoteLocs.push_back(loc);
            previously.push_back(loc);
        }
    }

    PathList required;
    if (!getRequiredLocationsForPaths(progress, required)) {
        return false;
    }
    pathLocations.assign(L.locationCount(), 0);
    for (const auto& g : required) {
        for (const PlacedItem& pi : g.second) {
            pathLocations[pi.loc] = 1;
        }
    }
    if (!opts.keylunacy) {
        for (auto& g : required) {
            g.second.erase(std::remove_if(g.second.begin(), g.second.end(),
                                          [](const PlacedItem& pi) { return endsWith(pi.item, " Key"); }),
                           g.second.end());
        }
    }

    // Path hints.
    std::vector<std::string> dungeonPaths = in.requiredDungeons;
    rng.shuffle(dungeonPaths);
    const int maxPath = opts.num_path_hints;
    if (maxPath > 0) {
        for (const std::string& d : dungeonPaths) {
            for (const PlacedItem& pi : *pathFind(required, d)) {
                listRemove(*pathFind(required, "Hyrule"), pi);
                listRemove(*pathFind(required, "Ganon's Tower"), pi);
            }
        }
        for (const PlacedItem& pi : *pathFind(required, "Hyrule")) {
            listRemove(*pathFind(required, "Ganon's Tower"), pi);
        }
    }
    std::vector<Hint> pathHints;
    for (const std::string& d : dungeonPaths) {
        if (required.empty()) {
            break;
        }
        if ((int)pathHints.size() < maxPath) {
            Hint h;
            int loc = -1;
            std::vector<PlacedItem>* lst = pathFind(required, d);
            if (lst == nullptr || !getPathHint(*lst, previously, d, h, loc)) {
                for (size_t i = 0; i < required.size(); i++) {
                    if (required[i].first == d) {
                        required.erase(required.begin() + i);
                        break;
                    }
                }
                continue;
            }
            if (!hasInt(previously, loc)) {
                pathHints.push_back(h);
                previously.push_back(loc);
            }
        }
    }
    while (!required.empty() && (int)pathHints.size() < maxPath) {
        const size_t k = rng.choiceIndex(required.size());
        const std::string name = required[k].first;
        Hint h;
        int loc = -1;
        if (!getPathHint(required[k].second, previously, name, h, loc)) {
            required.erase(required.begin() + k);
        } else if (!hasInt(previously, loc)) {
            pathHints.push_back(h);
            previously.push_back(loc);
        }
    }

    // Barren hints, weighted by sqrt(number of progress locations in the zone).
    std::vector<std::string> unhintedBarren;
    getBarrenZones(progress, remoteLocs, unhintedBarren);
    setsReady = true;
    if (!ok()) {
        return false;
    }
    std::vector<Hint> barrenHints;
    std::vector<double> weights;
    while (!unhintedBarren.empty() && (int)barrenHints.size() < opts.num_barren_hints) {
        weights.clear();
        double sum = 0.0;
        for (const std::string& z : unhintedBarren) {
            weights.push_back(sqrt((double)count(z)));
            sum += weights.back();
        }
        if (sum == 0.0) {
            break;
        }
        const size_t k = rng.choicesIndex(weights.data(), weights.size());
        Hint h;
        h.type = HintType::Barren;
        h.place = unhintedBarren[k];
        unhintedBarren.erase(unhintedBarren.begin() + k);
        barrenHints.push_back(h);
    }

    for (size_t i = 0; i < remoteHints.size(); i++) {
        remoteHints[i].importance = importanceFor(remoteLocs[i]);
    }

    // Item hints.
    std::vector<int> hintable;
    getLegalItemHints(progress, barrenHints, previously, hintable);
    std::vector<Hint> itemHints;
    while (!hintable.empty() && (int)itemHints.size() < opts.num_item_hints) {
        int loc = -1;
        itemHints.push_back(getItemHint(hintable, loc));
        previously.push_back(loc);
    }

    // Standard location hints, up to the total number of hints.
    std::vector<int> remote, standard;
    getLegalLocationHints(progress, barrenHints, previously, remote, standard);
    std::vector<Hint> standardHints;
    const int total = opts.num_path_hints + opts.num_barren_hints + opts.num_location_hints + opts.num_item_hints;
    int remaining = total - (int)pathHints.size() - (int)barrenHints.size() - (int)itemHints.size() -
                    (int)remoteHints.size();
    while (!standard.empty() && remaining > 0) {
        remaining--;
        int loc = -1;
        standardHints.push_back(getLocationHint(standard, loc, false));
        previously.push_back(loc);
    }
    if (!ok()) {
        return false;
    }

    out.clear();
    for (const auto* v : {&pathHints, &barrenHints, &itemHints, &remoteHints, &standardHints}) {
        out.insert(out.end(), v->begin(), v->end());
    }
    return true;
}

// rotate_hoho_to_face_hint's choice of island: the first hint that names an island (-1: none).
int HintsGen::hohoLookIsland(const std::vector<Hint>& hints) {
    std::string zone;
    for (const Hint& h : hints) {
        if (h.type == HintType::Path || h.type == HintType::Barren || h.type == HintType::Item) {
            zone = h.place;
        } else if (h.type == HintType::Location) {
            const int loc = L.locationIndex(h.place);
            zone = loc >= 0 ? entranceZone(loc) : std::string();
        }
        if (zone == "Ganon's Tower") {
            zone = "Tower of the Gods Sector";
        }
        // island_name_to_number: data/island_names.txt (islands 1..49).
        for (const Island& is : islands()) {
            if (is.number >= 1 && zone == is.name) {
                return is.number;
            }
        }
    }
    return -1;
}

// HintsRandomizer._randomize.
bool HintsGen::run(HintsResult& out) {
    out = HintsResult();
    const int n = L.locationCount();
    banned.assign(n, 0);
    for (int l : L.state().bannedLocations) {
        if (l >= 0 && l < n) {
            banned[l] = 1;
        }
    }
    zonesCache.assign(n, {});
    zonesDone.assign(n, 0);

    // self._path_logic = Logic(self.rando)
    if (!path.create(opts, L.state())) {
        return fail(path.error());
    }
    pathInitial = path.saveSimulatedPlaythroughState();

    std::vector<Hint> hints;
    if (!generateHints(hints)) {
        return false;
    }
    out.floor30 = generateSavageLabyrinthHint("Outset Island - Savage Labyrinth - Floor 30");
    out.floor50 = generateSavageLabyrinthHint("Outset Island - Savage Labyrinth - Floor 50");

    bool anyHintable = false;
    for (int item : in.allRandomizedProgressItems) {
        anyHintable = anyHintable || checkItemCanBeHintedAt(L, item);
    }
    if (!anyHintable) {
        return ok();
    }
    if (!generateOctoFairyHint(out.octoFairy)) {
        return false;
    }
    out.hasOctoFairy = true;

    if (opts.fishmen_hints) {
        out.placements.emplace_back(HintPlacement::Fishmen, std::vector<Hint>());
    }
    if (opts.hoho_hints) {
        out.placements.emplace_back(HintPlacement::Hoho, std::vector<Hint>());
    }
    if (opts.korl_hints) {
        out.placements.emplace_back(HintPlacement::Korl, std::vector<Hint>());
    }
    std::vector<HintPlacement> order;
    for (const auto& p : out.placements) {
        order.push_back(p.first);
    }
    const int total = opts.num_path_hints + opts.num_barren_hints + opts.num_location_hints + opts.num_item_hints;
    if (total == 0 || order.empty()) {
        return ok();
    }

    // Fewer hints than placements: duplicate them so every placement has one.
    std::vector<Hint> duplicated;
    while (hints.size() + duplicated.size() < order.size()) {
        if (hints.empty()) {
            // wwrando loops forever here (sample of an empty list); refuse instead.
            return fail("No hints could be generated for the enabled hint placements");
        }
        std::vector<Hint> s = rng.sample(hints, hints.size());
        duplicated.insert(duplicated.end(), s.begin(), s.end());
    }
    const size_t want = order.size() > hints.size() ? order.size() - hints.size() : 0;
    hints.insert(hints.end(), duplicated.begin(), duplicated.begin() + std::min(want, duplicated.size()));

    rng.shuffle(order);
    for (size_t i = 0; i < hints.size(); i++) {
        const HintPlacement p = order[i % order.size()];
        for (auto& e : out.placements) {
            if (e.first == p) {
                e.second.push_back(hints[i]);
            }
        }
    }

    if (const std::vector<Hint>* fish = out.placement(HintPlacement::Fishmen)) {
        // distribute_fishmen_hints
        std::vector<int> islandsOrder;
        for (int i = 1; i <= 49; i++) {
            islandsOrder.push_back(i);
        }
        rng.shuffle(islandsOrder);
        out.fishmen.assign(50, Hint());
        for (size_t k = 0; k < islandsOrder.size(); k++) {
            out.fishmen[islandsOrder[k]] = (*fish)[k % fish->size()];
        }
    }
    if (const std::vector<Hint>* hoho = out.placement(HintPlacement::Hoho)) {
        // distribute_hoho_hints
        std::vector<int> hohos;
        for (int i = 0; i < kHohoCount; i++) {
            hohos.push_back(i);
        }
        rng.shuffle(hohos);
        out.hoho.assign(kHohoCount, {});
        size_t idx = 0;
        while (idx < hoho->size()) {
            for (int h : hohos) {
                out.hoho[h].push_back((*hoho)[idx % hoho->size()]);
                idx++;
            }
        }
        out.hohoLookIsland.assign(kHohoCount, -1);
        for (int h = 0; h < kHohoCount; h++) {
            out.hohoLookIsland[h] = hohoLookIsland(out.hoho[h]);
        }
    }
    if (const std::vector<Hint>* korl = out.placement(HintPlacement::Korl)) {
        out.korl = *korl;
    }
    return ok();
}

} // namespace

bool generateHints(Logic& logic, PyRandom& rng, const HintsInput& in, HintsResult& out) {
    HintsGen g(logic, rng, in);
    const bool ok = g.run(out);
    if (!ok) {
        out.error = g.errorText();
        if (out.error.empty()) {
            out.error = logic.failed() ? logic.error() : "Hint generation failed";
        }
    }
    return ok;
}

bool runHintsRandomizer(Logic& logic, const HintsInput& in, const std::string& sanitizedSeed, HintsResult& out,
                        const char* seedKey) {
    PyRandom rng;
    newRandomizerRng(rng, sanitizedSeed, logic.options(), seedKey);
    traceAttach(rng, "HintsRandomizer");
    return generateHints(logic, rng, in, out);
}

bool hintSpoilerRows(Logic& logic, const HintsResult& h, std::vector<std::pair<std::string, std::string>>& rows,
                     std::string* err) {
    rows.clear();
    for (const Hint* s : {&h.floor30, &h.floor50}) {
        const int item = s->reward.empty() ? -1 : logic.item(s->reward);
        rows.emplace_back(s->place, checkItemCanBeHintedAt(logic, item) ? s->reward : std::string("Nothing"));
    }
    if (!h.hasOctoFairy) {
        if (err != nullptr) {
            *err = "'NoneType' object has no attribute 'place'";
        }
        return false;
    }
    std::vector<const Hint*> all = {&h.octoFairy};
    for (const auto& p : h.placements) {
        for (const Hint& x : p.second) {
            all.push_back(&x);
        }
    }
    for (const Hint* x : all) {
        rows.emplace_back(x->place, x->hasReward() && !x->reward.empty() ? x->reward : std::string("Nothing"));
    }
    return true;
}

// --- JSON ------------------------------------------------------------------------------------------

namespace {

Json hintToJson(const Hint& h) {
    Json j = Json::object();
    j.set("type", Json::string(hintTypeName(h.type)));
    j.set("place", Json::string(h.place));
    j.set("reward", h.hasReward() ? Json::string(h.reward) : Json::null());
    j.set("importance", h.importance == HintImportance::None ? Json::null()
                                                             : Json::string(hintImportanceName(h.importance)));
    return j;
}

Json hintListToJson(const std::vector<Hint>& v) {
    Json a = Json::array();
    for (const Hint& h : v) {
        a.push(hintToJson(h));
    }
    return a;
}

bool hintFromJson(const Json& j, Hint& h, std::string* err) {
    auto bad = [&](const char* what) {
        if (err != nullptr) {
            *err = std::string("hints: ") + what;
        }
        return false;
    };
    if (!j.isObject()) {
        return bad("a hint is not an object");
    }
    const Json* type = j.find("type");
    const Json* place = j.find("place");
    const Json* reward = j.find("reward");
    const Json* imp = j.find("importance");
    if (type == nullptr || !type->isString() || place == nullptr || !place->isString()) {
        return bad("a hint lacks its type or place");
    }
    h = Hint();
    bool typeOk = false;
    for (HintType t : {HintType::Path, HintType::Barren, HintType::Item, HintType::Location, HintType::FixedLocation}) {
        if (type->text() == hintTypeName(t)) {
            h.type = t;
            typeOk = true;
        }
    }
    if (!typeOk) {
        return bad("unknown hint type");
    }
    h.place = place->text();
    if (h.hasReward()) {
        if (reward == nullptr || !reward->isString()) {
            return bad("a hint lacks its reward");
        }
        h.reward = reward->text();
    }
    if (imp != nullptr && imp->isString()) {
        bool impOk = false;
        for (HintImportance i : {HintImportance::Required, HintImportance::PossiblyRequired,
                                 HintImportance::NotRequired}) {
            if (imp->text() == hintImportanceName(i)) {
                h.importance = i;
                impOk = true;
            }
        }
        if (!impOk) {
            return bad("unknown hint importance");
        }
    }
    return true;
}

bool hintListFromJson(const Json* j, std::vector<Hint>& out, std::string* err) {
    out.clear();
    if (j == nullptr || !j->isArray()) {
        if (err != nullptr) {
            *err = "hints: a hint list is not an array";
        }
        return false;
    }
    for (size_t i = 0; i < j->size(); i++) {
        Hint h;
        if (!hintFromJson(j->at(i), h, err)) {
            return false;
        }
        out.push_back(h);
    }
    return true;
}

} // namespace

Json hintsToJson(const HintsResult& h) {
    Json j = Json::object();
    j.set("format", Json::integer(kHintsJsonFormat));
    j.set("savage_labyrinth", hintListToJson({h.floor30, h.floor50}));
    j.set("octo_fairy", h.hasOctoFairy ? hintToJson(h.octoFairy) : Json::null());
    Json pl = Json::object();
    for (const auto& p : h.placements) {
        pl.set(hintPlacementName(p.first), hintListToJson(p.second));
    }
    j.set("placements", pl);
    if (!h.fishmen.empty()) {
        j.set("fishmen", hintListToJson(std::vector<Hint>(h.fishmen.begin() + 1, h.fishmen.end())));
    }
    if (!h.hoho.empty()) {
        Json a = Json::array();
        for (size_t i = 0; i < h.hoho.size(); i++) {
            Json o = Json::object();
            o.set("hints", hintListToJson(h.hoho[i]));
            const int look = i < h.hohoLookIsland.size() ? h.hohoLookIsland[i] : -1;
            o.set("look_island", look >= 0 ? Json::integer(look) : Json::null());
            a.push(o);
        }
        j.set("hoho", a);
    }
    if (h.placement(HintPlacement::Korl) != nullptr) {
        j.set("korl", hintListToJson(h.korl));
    }
    return j;
}

bool hintsFromJson(const Json& j, HintsResult& out, std::string* err) {
    out = HintsResult();
    auto bad = [&](const char* what) {
        if (err != nullptr) {
            *err = std::string("hints: ") + what;
        }
        return false;
    };
    if (!j.isObject()) {
        return bad("not an object");
    }
    const Json* fmt = j.find("format");
    if (fmt == nullptr || !fmt->isInteger() || fmt->asInt() != kHintsJsonFormat) {
        return bad("unknown format");
    }
    std::vector<Hint> savage;
    if (!hintListFromJson(j.find("savage_labyrinth"), savage, err)) {
        return false;
    }
    if (savage.size() != 2) {
        return bad("savage_labyrinth must hold 2 hints");
    }
    out.floor30 = savage[0];
    out.floor50 = savage[1];
    const Json* octo = j.find("octo_fairy");
    if (octo != nullptr && !octo->isNull()) {
        if (!hintFromJson(*octo, out.octoFairy, err)) {
            return false;
        }
        out.hasOctoFairy = true;
    }
    const Json* pl = j.find("placements");
    if (pl == nullptr || !pl->isObject()) {
        return bad("placements is not an object");
    }
    for (size_t i = 0; i < pl->size(); i++) {
        HintPlacement p;
        const std::string& k = pl->keyAt(i);
        if (k == "fishmen_hints") {
            p = HintPlacement::Fishmen;
        } else if (k == "hoho_hints") {
            p = HintPlacement::Hoho;
        } else if (k == "korl_hints") {
            p = HintPlacement::Korl;
        } else {
            return bad("unknown hint placement");
        }
        std::vector<Hint> v;
        if (!hintListFromJson(&pl->at(i), v, err)) {
            return false;
        }
        out.placements.emplace_back(p, v);
    }
    if (const Json* fish = j.find("fishmen")) {
        std::vector<Hint> v;
        if (!hintListFromJson(fish, v, err)) {
            return false;
        }
        if (v.size() != 49) {
            return bad("fishmen must hold 49 hints");
        }
        out.fishmen.assign(1, Hint());
        out.fishmen.insert(out.fishmen.end(), v.begin(), v.end());
    }
    if (const Json* hoho = j.find("hoho")) {
        if (!hoho->isArray() || hoho->size() != kHohoCount) {
            return bad("hoho must hold 10 entries");
        }
        for (size_t i = 0; i < hoho->size(); i++) {
            const Json& o = hoho->at(i);
            std::vector<Hint> v;
            if (!o.isObject() || !hintListFromJson(o.find("hints"), v, err)) {
                return err == nullptr || err->empty() ? bad("a hoho entry is not an object") : false;
            }
            out.hoho.push_back(v);
            const Json* look = o.find("look_island");
            out.hohoLookIsland.push_back(look != nullptr && look->isInteger() ? (int)look->asInt() : -1);
        }
    }
    if (const Json* korl = j.find("korl")) {
        if (!hintListFromJson(korl, out.korl, err)) {
            return false;
        }
    }
    return true;
}

} // namespace pcr
