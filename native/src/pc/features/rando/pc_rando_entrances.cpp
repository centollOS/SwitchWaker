// pc_rando_entrances.cpp - entrance randomizer tables and sets (docs/RANDOMIZER_PLAN.md step R6.10): the
// data half of wwrando 1.10.0_9775811's randomizers/entrances.py. See pc_rando_entrances.h. Pure C++
// (cos_rando_core).
#include "pc_rando_entrances.h"

#include <cstring>

namespace pcr {

namespace {

using G = EntranceGroup;

// ZoneEntrance.all, definition order (DUNGEON_ENTRANCES, MINIBOSS_ENTRANCES, BOSS_ENTRANCES,
// SECRET_CAVE_ENTRANCES, SECRET_CAVE_INNER_ENTRANCES, FAIRY_FOUNTAIN_ENTRANCES).
// {stage, room, scls_exit_index, spawn_id, entrance_name, island_name, warp_out_stage_name,
//  warp_out_room_num, warp_out_spawn_id, nested_in (kZoneExits index), group}
// The Forsaken Fortress dungeon entrance and exit are commented out upstream, as here.
const ZoneEntranceDef kZoneEntrances[] = {
    {"Adanmae", 0, 2, 2, "Dungeon Entrance on Dragon Roost Island", "Dragon Roost Island", "sea", 13, 211, -1, G::Dungeon},
    {"sea", 41, 6, 6, "Dungeon Entrance in Forest Haven Sector", "Forest Haven", "Omori", 0, 215, -1, G::Dungeon},
    {"sea", 26, 0, 2, "Dungeon Entrance in Tower of the Gods Sector", "Tower of the Gods Sector", "sea", 26, 1, -1, G::Dungeon},
    {"Edaichi", 0, 0, 1, "Dungeon Entrance on Headstone Island", "Headstone Island", "sea", 45, 229, -1, G::Dungeon},
    {"Ekaze", 0, 0, 1, "Dungeon Entrance on Gale Isle", "Gale Isle", "sea", 4, 232, -1, G::Dungeon},
    {"kindan", 9, 0, 1, "Miniboss Entrance in Forbidden Woods", nullptr, nullptr, -1, -1, 1, G::Miniboss}, // Forbidden Woods
    {"Siren", 14, 0, 1, "Miniboss Entrance in Tower of the Gods", nullptr, nullptr, -1, -1, 2, G::Miniboss}, // Tower of the Gods
    {"M_Dai", 7, 0, 9, "Miniboss Entrance in Earth Temple", nullptr, nullptr, -1, -1, 3, G::Miniboss}, // Earth Temple
    {"kaze", 2, 3, 20, "Miniboss Entrance in Wind Temple", nullptr, nullptr, -1, -1, 4, G::Miniboss}, // Wind Temple
    {"Hyroom", 0, 2, 2, "Miniboss Entrance in Hyrule Castle", "Tower of the Gods Sector", "Hyroom", 0, 10, -1, G::Miniboss},
    {"M_NewD2", 10, 1, 27, "Boss Entrance in Dragon Roost Cavern", nullptr, nullptr, -1, -1, 0, G::Boss}, // Dragon Roost Cavern
    {"kindan", 16, 0, 1, "Boss Entrance in Forbidden Woods", nullptr, nullptr, -1, -1, 1, G::Boss}, // Forbidden Woods
    {"Siren", 18, 0, 27, "Boss Entrance in Tower of the Gods", nullptr, nullptr, -1, -1, 2, G::Boss}, // Tower of the Gods
    {"sea", 1, 16, 27, "Boss Entrance in Forsaken Fortress", "Forsaken Fortress Sector", "sea", 1, 0, -1, G::Boss},
    {"M_Dai", 15, 0, 27, "Boss Entrance in Earth Temple", nullptr, nullptr, -1, -1, 3, G::Boss}, // Earth Temple
    {"kaze", 12, 0, 27, "Boss Entrance in Wind Temple", nullptr, nullptr, -1, -1, 4, G::Boss}, // Wind Temple
    {"sea", 44, 8, 10, "Secret Cave Entrance on Outset Island", "Outset Island", "sea", 44, 10, -1, G::SecretCave},
    {"sea", 13, 2, 5, "Secret Cave Entrance on Dragon Roost Island", "Dragon Roost Island", "sea", 13, 5, -1, G::SecretCave},
    {"sea", 20, 0, 0, "Secret Cave Entrance on Fire Mountain", "Fire Mountain", "sea", 20, 0, -1, G::SecretCave},
    {"sea", 40, 0, 0, "Secret Cave Entrance on Ice Ring Isle", "Ice Ring Isle", "sea", 40, 0, -1, G::SecretCave},
    {"Abesso", 0, 1, 1, "Secret Cave Entrance on Private Oasis", "Private Oasis", "Abesso", 0, 1, -1, G::SecretCave},
    {"sea", 29, 0, 5, "Secret Cave Entrance on Needle Rock Isle", "Needle Rock Isle", "sea", 29, 5, -1, G::SecretCave},
    {"sea", 47, 1, 5, "Secret Cave Entrance on Angular Isles", "Angular Isles", "sea", 47, 5, -1, G::SecretCave},
    {"sea", 48, 0, 5, "Secret Cave Entrance on Boating Course", "Boating Course", "sea", 48, 5, -1, G::SecretCave},
    {"sea", 31, 0, 1, "Secret Cave Entrance on Stone Watcher Island", "Stone Watcher Island", "sea", 31, 1, -1, G::SecretCave},
    {"sea", 7, 0, 1, "Secret Cave Entrance on Overlook Island", "Overlook Island", "sea", 7, 1, -1, G::SecretCave},
    {"sea", 35, 0, 1, "Secret Cave Entrance on Bird's Peak Rock", "Bird's Peak Rock", "sea", 35, 1, -1, G::SecretCave},
    {"sea", 12, 0, 1, "Secret Cave Entrance on Pawprint Isle", "Pawprint Isle", "sea", 12, 1, -1, G::SecretCave},
    {"sea", 12, 1, 5, "Secret Cave Entrance on Pawprint Isle Side Isle", "Pawprint Isle", "sea", 12, 5, -1, G::SecretCave},
    {"sea", 36, 0, 1, "Secret Cave Entrance on Diamond Steppe Island", "Diamond Steppe Island", "sea", 36, 1, -1, G::SecretCave},
    {"sea", 34, 0, 1, "Secret Cave Entrance on Bomb Island", "Bomb Island", "sea", 34, 1, -1, G::SecretCave},
    {"sea", 16, 0, 1, "Secret Cave Entrance on Rock Spire Isle", "Rock Spire Isle", "sea", 16, 1, -1, G::SecretCave},
    {"sea", 38, 0, 5, "Secret Cave Entrance on Shark Island", "Shark Island", "sea", 38, 5, -1, G::SecretCave},
    {"sea", 42, 0, 2, "Secret Cave Entrance on Cliff Plateau Isles", "Cliff Plateau Isles", "sea", 42, 2, -1, G::SecretCave},
    {"sea", 43, 0, 5, "Secret Cave Entrance on Horseshoe Island", "Horseshoe Island", "sea", 43, 5, -1, G::SecretCave},
    {"sea", 2, 0, 1, "Secret Cave Entrance on Star Island", "Star Island", "sea", 2, 1, -1, G::SecretCave},
    {"MiniHyo", 0, 1, 0, "Inner Entrance in Ice Ring Isle Secret Cave", nullptr, nullptr, -1, -1, 19, G::SecretCaveInner}, // Ice Ring Isle Secret Cave
    {"Cave03", 0, 1, 1, "Inner Entrance in Cliff Plateau Isles Secret Cave", nullptr, nullptr, -1, -1, 33, G::SecretCaveInner}, // Cliff Plateau Isles Secret Cave
    {"A_mori", 0, 1, 2, "Fairy Fountain Entrance on Outset Island", "Outset Island", "A_mori", 0, 2, -1, G::FairyFountain},
    {"sea", 28, 0, 1, "Fairy Fountain Entrance on Thorned Fairy Island", "Thorned Fairy Island", "sea", 28, 1, -1, G::FairyFountain},
    {"sea", 19, 0, 1, "Fairy Fountain Entrance on Eastern Fairy Island", "Eastern Fairy Island", "sea", 19, 1, -1, G::FairyFountain},
    {"sea", 15, 0, 1, "Fairy Fountain Entrance on Western Fairy Island", "Western Fairy Island", "sea", 15, 1, -1, G::FairyFountain},
    {"sea", 39, 0, 1, "Fairy Fountain Entrance on Southern Fairy Island", "Southern Fairy Island", "sea", 39, 1, -1, G::FairyFountain},
    {"sea", 3, 0, 1, "Fairy Fountain Entrance on Northern Fairy Island", "Northern Fairy Island", "sea", 3, 1, -1, G::FairyFountain},
};
static_assert(sizeof(kZoneEntrances) / sizeof(kZoneEntrances[0]) == kNumZoneEntrances, "44 ZoneEntrances");

// ZoneExit.all, definition order (the *_EXITS lists in the same group order).
// {stage, room, scls_exit_index, spawn_id, unique_name, boss_stage_name, zone_name, group}
const ZoneExitDef kZoneExits[] = {
    {"M_NewD2", 0, 0, 0, "Dragon Roost Cavern", "M_DragB", "Dragon Roost Cavern", G::Dungeon},
    {"kindan", 0, 0, 0, "Forbidden Woods", "kinBOSS", "Forbidden Woods", G::Dungeon},
    {"Siren", 0, 1, 0, "Tower of the Gods", "SirenB", "Tower of the Gods", G::Dungeon},
    {"M_Dai", 0, 0, 0, "Earth Temple", "M_DaiB", "Earth Temple", G::Dungeon},
    {"kaze", 15, 0, 15, "Wind Temple", "kazeB", "Wind Temple", G::Dungeon},
    {"kinMB", 10, 0, 0, "Forbidden Woods Miniboss Arena", nullptr, nullptr, G::Miniboss},
    {"SirenMB", 23, 0, 0, "Tower of the Gods Miniboss Arena", nullptr, nullptr, G::Miniboss},
    {"M_DaiMB", 12, 0, 0, "Earth Temple Miniboss Arena", nullptr, nullptr, G::Miniboss},
    {"kazeMB", 6, 0, 0, "Wind Temple Miniboss Arena", nullptr, nullptr, G::Miniboss},
    {"kenroom", 0, 0, 0, "Master Sword Chamber", nullptr, nullptr, G::Miniboss},
    {"M_DragB", 0, -1, 0, "Gohma Boss Arena", nullptr, nullptr, G::Boss},
    {"kinBOSS", 0, -1, 0, "Kalle Demos Boss Arena", nullptr, nullptr, G::Boss},
    {"SirenB", 0, -1, 0, "Gohdan Boss Arena", nullptr, nullptr, G::Boss},
    {"M2tower", 0, -1, 16, "Helmaroc King Boss Arena", nullptr, nullptr, G::Boss},
    {"M_DaiB", 0, -1, 0, "Jalhalla Boss Arena", nullptr, nullptr, G::Boss},
    {"kazeB", 0, -1, 0, "Molgera Boss Arena", nullptr, nullptr, G::Boss},
    {"Cave09", 0, 1, 0, "Savage Labyrinth", nullptr, "Outset Island", G::SecretCave},
    {"TF_06", 0, 0, 0, "Dragon Roost Island Secret Cave", nullptr, "Dragon Roost Island", G::SecretCave},
    {"MiniKaz", 0, 0, 0, "Fire Mountain Secret Cave", nullptr, "Fire Mountain", G::SecretCave},
    {"MiniHyo", 0, 0, 0, "Ice Ring Isle Secret Cave", nullptr, "Ice Ring Isle", G::SecretCave},
    {"TF_04", 0, 0, 0, "Cabana Labyrinth", nullptr, "Private Oasis", G::SecretCave},
    {"SubD42", 0, 0, 0, "Needle Rock Isle Secret Cave", nullptr, "Needle Rock Isle", G::SecretCave},
    {"SubD43", 0, 0, 0, "Angular Isles Secret Cave", nullptr, "Angular Isles", G::SecretCave},
    {"SubD71", 0, 0, 0, "Boating Course Secret Cave", nullptr, "Boating Course", G::SecretCave},
    {"TF_01", 0, 0, 0, "Stone Watcher Island Secret Cave", nullptr, "Stone Watcher Island", G::SecretCave},
    {"TF_02", 0, 0, 0, "Overlook Island Secret Cave", nullptr, "Overlook Island", G::SecretCave},
    {"TF_03", 0, 0, 0, "Bird's Peak Rock Secret Cave", nullptr, "Bird's Peak Rock", G::SecretCave},
    {"TyuTyu", 0, 0, 0, "Pawprint Isle Chuchu Cave", nullptr, "Pawprint Isle", G::SecretCave},
    {"Cave07", 0, 0, 0, "Pawprint Isle Wizzrobe Cave", nullptr, nullptr, G::SecretCave},
    {"WarpD", 0, 0, 0, "Diamond Steppe Island Warp Maze Cave", nullptr, "Diamond Steppe Island", G::SecretCave},
    {"Cave01", 0, 0, 0, "Bomb Island Secret Cave", nullptr, "Bomb Island", G::SecretCave},
    {"Cave04", 0, 0, 0, "Rock Spire Isle Secret Cave", nullptr, "Rock Spire Isle", G::SecretCave},
    {"ITest63", 0, 0, 0, "Shark Island Secret Cave", nullptr, "Shark Island", G::SecretCave},
    {"Cave03", 0, 0, 0, "Cliff Plateau Isles Secret Cave", nullptr, "Cliff Plateau Isles", G::SecretCave},
    {"Cave05", 0, 0, 0, "Horseshoe Island Secret Cave", nullptr, "Horseshoe Island", G::SecretCave},
    {"Cave02", 0, 0, 0, "Star Island Secret Cave", nullptr, "Star Island", G::SecretCave},
    {"ITest62", 0, 0, 0, "Ice Ring Isle Inner Cave", nullptr, nullptr, G::SecretCaveInner},
    {"sea", 42, 1, 1, "Cliff Plateau Isles Inner Cave", nullptr, nullptr, G::SecretCaveInner},
    {"Fairy04", 0, 0, 0, "Outset Fairy Fountain", nullptr, nullptr, G::FairyFountain},
    {"Fairy05", 0, 0, 0, "Thorned Fairy Fountain", nullptr, "Thorned Fairy Island", G::FairyFountain},
    {"Fairy02", 0, 0, 0, "Eastern Fairy Fountain", nullptr, "Eastern Fairy Island", G::FairyFountain},
    {"Fairy03", 0, 0, 0, "Western Fairy Fountain", nullptr, "Western Fairy Island", G::FairyFountain},
    {"Fairy06", 0, 0, 0, "Southern Fairy Fountain", nullptr, "Southern Fairy Island", G::FairyFountain},
    {"Fairy01", 0, 0, 0, "Northern Fairy Fountain", nullptr, "Northern Fairy Island", G::FairyFountain},
};
static_assert(sizeof(kZoneExits) / sizeof(kZoneExits[0]) == kNumZoneExits, "44 ZoneExits");

// Each group's first index and size, in both tables (the *_ENTRANCES and *_EXITS lists of a group
// have the same length).
const IndexRange kGroups[kNumEntranceGroups] = {{0, 5}, {5, 5}, {10, 6}, {16, 20}, {36, 2}, {38, 6}};
const char* const kGroupListNames[kNumEntranceGroups] = {
    "DUNGEON", "MINIBOSS", "BOSS", "SECRET_CAVE", "SECRET_CAVE_INNER", "FAIRY_FOUNTAIN",
};

// ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES (exit indices into kZoneExits).
const LocationExitOverride kOverrides[] = {
    {"Forbidden Woods - Mothula Miniboss Room", 5},           // Forbidden Woods Miniboss Arena
    {"Tower of the Gods - Darknut Miniboss Room", 6},         // Tower of the Gods Miniboss Arena
    {"Earth Temple - Stalfos Miniboss Room", 7},              // Earth Temple Miniboss Arena
    {"Wind Temple - Wizzrobe Miniboss Room", 8},              // Wind Temple Miniboss Arena
    {"Hyrule - Master Sword Chamber", 9},                     // Master Sword Chamber

    {"Dragon Roost Cavern - Gohma Heart Container", 10},      // Gohma Boss Arena
    {"Forbidden Woods - Kalle Demos Heart Container", 11},    // Kalle Demos Boss Arena
    {"Tower of the Gods - Gohdan Heart Container", 12},       // Gohdan Boss Arena
    {"Forsaken Fortress - Helmaroc King Heart Container", 13},// Helmaroc King Boss Arena
    {"Earth Temple - Jalhalla Heart Container", 14},          // Jalhalla Boss Arena
    {"Wind Temple - Molgera Heart Container", 15},            // Molgera Boss Arena

    {"Pawprint Isle - Wizzrobe Cave", 28},                    // Pawprint Isle Wizzrobe Cave

    {"Ice Ring Isle - Inner Cave - Chest", 36},               // Ice Ring Isle Inner Cave
    {"Cliff Plateau Isles - Highest Isle", 37},               // Cliff Plateau Isles Inner Cave

    {"Outset Island - Great Fairy", 38},                      // Outset Fairy Fountain
};
static_assert(sizeof(kOverrides) / sizeof(kOverrides[0]) == 15, "15 location -> exit overrides");

const char* const kDungeonEntranceNoReq[] = {
    "Dungeon Entrance on Dragon Roost Island",
};
const char* const kSecretCaveEntranceNoReq[] = {
    "Secret Cave Entrance on Pawprint Isle",
    "Secret Cave Entrance on Cliff Plateau Isles",
};
const char* const kDungeonExitNoReq[] = {
    "Dragon Roost Cavern",
};
const char* const kPuzzleCaveExitNoReq[] = {
    "Pawprint Isle Chuchu Cave",
    "Ice Ring Isle Secret Cave",
    "Bird's Peak Rock Secret Cave", // only Wind Waker + Wind's Requiem upstream
    "Diamond Steppe Island Warp Maze Cave",
};
const char* const kCombatCaveExitNoReq[] = {
    "Rock Spire Isle Secret Cave",
};
// ENTRANCE_RANDOMIZABLE_ITEM_LOCATION_TYPES (the Types tags).
const char* const kRandomizableTypes[] = {
    "Dungeon", "Puzzle Secret Cave", "Combat Secret Cave", "Savage Labyrinth", "Great Fairy",
};
constexpr uint32_t kRandomizableTypeBits =
    LOC_DUNGEON | LOC_PUZZLE_SECRET_CAVE | LOC_COMBAT_SECRET_CAVE | LOC_SAVAGE_LABYRINTH | LOC_GREAT_FAIRY;

template <class T, size_t N> Span<T> span(const T (&a)[N]) { return Span<T>{a, N}; }
template <size_t N> StrSpan strSpan(const char* const (&a)[N]) { return StrSpan{a, N}; }

void appendAll(std::vector<std::string>& out, StrSpan s) {
    for (const char* n : s) {
        out.emplace_back(n);
    }
}

void appendGroup(EntranceSet& set, EntranceGroup g) {
    const IndexRange r = kGroups[(int)g];
    for (int i = r.first; i < r.first + r.count; i++) {
        set.entrances.push_back(i);
        set.exits.push_back(i);
    }
}

// get_one_entrance_set: the groups in the order dungeons, minibosses, bosses, caves, inner caves,
// fountains (the keyword arguments' order does not matter; the function's body order does).
EntranceSet oneEntranceSet(bool dungeons, bool minibosses, bool bosses, bool caves, bool innerCaves,
                           bool fountains) {
    EntranceSet s;
    if (dungeons) {
        appendGroup(s, G::Dungeon);
    }
    if (minibosses) {
        appendGroup(s, G::Miniboss);
    }
    if (bosses) {
        appendGroup(s, G::Boss);
    }
    if (caves) {
        appendGroup(s, G::SecretCave);
    }
    if (innerCaves) {
        appendGroup(s, G::SecretCaveInner);
    }
    if (fountains) {
        appendGroup(s, G::FairyFountain);
    }
    return s;
}

// split_location_name_by_zone's zone name.
std::string_view locationZone(const char* name) {
    const char* sep = std::strstr(name, " - ");
    return sep == nullptr ? std::string_view(name) : std::string_view(name, (size_t)(sep - name));
}

const std::vector<int> kNoLocations;

} // namespace

Span<ZoneEntranceDef> zoneEntrances() { return span(kZoneEntrances); }
Span<ZoneExitDef> zoneExits() { return span(kZoneExits); }

int zoneEntranceIndex(std::string_view name) {
    for (int i = 0; i < kNumZoneEntrances; i++) {
        if (name == kZoneEntrances[i].name) {
            return i;
        }
    }
    return -1;
}

int zoneExitIndex(std::string_view name) {
    for (int i = 0; i < kNumZoneExits; i++) {
        if (name == kZoneExits[i].name) {
            return i;
        }
    }
    return -1;
}

IndexRange entranceGroup(EntranceGroup g) { return kGroups[(int)g]; }
IndexRange exitGroup(EntranceGroup g) { return kGroups[(int)g]; }
const char* entranceGroupListName(EntranceGroup g) { return kGroupListNames[(int)g]; }

// The default entrance_connections pair each group's entrances with its exits in list order.
int defaultConnectionExit(int entrance) {
    return entrance >= 0 && entrance < kNumZoneEntrances ? entrance : -1;
}

Span<LocationExitOverride> itemLocationExitOverrides() { return span(kOverrides); }

StrSpan dungeonEntranceNamesWithNoRequirements() { return strSpan(kDungeonEntranceNoReq); }
StrSpan secretCaveEntranceNamesWithNoRequirements() { return strSpan(kSecretCaveEntranceNoReq); }
StrSpan dungeonExitNamesWithNoRequirements() { return strSpan(kDungeonExitNoReq); }
StrSpan puzzleSecretCaveExitNamesWithNoRequirements() { return strSpan(kPuzzleCaveExitNoReq); }
StrSpan combatSecretCaveExitNamesWithNoRequirements() { return strSpan(kCombatCaveExitNoReq); }
StrSpan entranceRandomizableItemLocationTypes() { return strSpan(kRandomizableTypes); }

std::vector<std::string> entranceNamesWithNoRequirements(const Options& o) {
    std::vector<std::string> out;
    if (o.progression_dungeons) {
        appendAll(out, dungeonEntranceNamesWithNoRequirements());
    }
    if (o.progression_puzzle_secret_caves || o.progression_combat_secret_caves || o.progression_savage_labyrinth) {
        appendAll(out, secretCaveEntranceNamesWithNoRequirements());
    }
    return out;
}

std::vector<std::string> exitNamesWithNoRequirements(const Options& o) {
    std::vector<std::string> out;
    if (o.progression_dungeons) {
        appendAll(out, dungeonExitNamesWithNoRequirements());
    }
    if (o.progression_puzzle_secret_caves) {
        appendAll(out, puzzleSecretCaveExitNamesWithNoRequirements());
    }
    if (o.progression_combat_secret_caves) {
        appendAll(out, combatSecretCaveExitNamesWithNoRequirements());
    }
    // progression_savage_labyrinth adds nothing (neither Savage item is free) upstream either.
    return out;
}

bool entranceNestingEnabled(const Options& o) {
    return o.randomize_miniboss_entrances || o.randomize_boss_entrances || o.randomize_secret_cave_inner_entrances;
}

bool entranceRandoEnabled(const Options& o) {
    return o.randomize_dungeon_entrances || o.randomize_secret_cave_entrances || o.randomize_miniboss_entrances ||
           o.randomize_boss_entrances || o.randomize_secret_cave_inner_entrances ||
           o.randomize_fairy_fountain_entrances;
}

bool entranceSetsToBeRandomized(const Options& o, std::vector<EntranceSet>& out, std::string* err) {
    out.clear();
    const bool dungeons = o.randomize_dungeon_entrances;
    const bool minibosses = o.randomize_miniboss_entrances;
    const bool bosses = o.randomize_boss_entrances;
    const bool caves = o.randomize_secret_cave_entrances;
    const bool innerCaves = o.randomize_secret_cave_inner_entrances;
    const bool fountains = o.randomize_fairy_fountain_entrances;
    const bool anyDungeons = dungeons || minibosses || bosses;
    const bool anyNonDungeons = caves || innerCaves || fountains;
    const EntranceMixMode mix = o.mix_entrances;
    if (mix == EntranceMixMode::SeparateDungeons && anyDungeons && anyNonDungeons) {
        out.push_back(oneEntranceSet(dungeons, minibosses, bosses, false, false, false));
        out.push_back(oneEntranceSet(false, false, false, caves, innerCaves, fountains));
        return true;
    }
    if ((anyDungeons || anyNonDungeons) &&
        (mix == EntranceMixMode::SeparateDungeons || mix == EntranceMixMode::MixDungeons)) {
        out.push_back(oneEntranceSet(dungeons, minibosses, bosses, caves, innerCaves, fountains));
        return true;
    }
    if (err != nullptr) {
        *err = "An invalid combination of entrance randomizer options was selected.";
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Item locations and zone exits

bool itemLocationBehindRandomizableEntrance(int loc) {
    const Span<Location> locs = locations();
    if (loc < 0 || (size_t)loc >= locs.size) {
        return false;
    }
    const std::string_view zone = locationZone(locs[loc].name);
    if (zone == "Ganon's Tower" || zone == "Mailbox") {
        // Dungeon-typed upstream, but not behind an entrance the randomizer moves.
        return false;
    }
    const uint32_t types = locs[loc].types;
    if (zone == "Forsaken Fortress" && !(types & LOC_BOSS)) {
        // FF is not randomized, except for its boss arena.
        return false;
    }
    if (types & LOC_BIG_OCTO) {
        // The only Great Fairy that is not in a fairy fountain.
        return false;
    }
    return (types & kRandomizableTypeBits) != 0;
}

bool zoneExitForItemLocationIndex(int loc, int& exit, std::string* err) {
    exit = -1;
    if (!itemLocationBehindRandomizableEntrance(loc)) {
        return true;
    }
    const char* name = locations()[loc].name;
    for (const LocationExitOverride& o : kOverrides) {
        if (std::strcmp(o.location, name) == 0) {
            exit = o.exit;
            return true;
        }
    }
    const std::string_view zone = locationZone(name);
    int found = -1, n = 0;
    for (int i = 0; i < kNumZoneExits; i++) {
        if (kZoneExits[i].zoneName != nullptr && zone == kZoneExits[i].zoneName) {
            if (found < 0) {
                found = i;
            }
            n++;
        }
    }
    if (n > 1) {
        if (err != nullptr) {
            *err = "Multiple zone exits share the same zone name: '" + std::string(zone) +
                   "'. Use a location exit override instead.";
        }
        return false;
    }
    exit = found;
    return true;
}

const std::vector<int>& ExitLocationMap::locationsFor(int exit) const {
    for (const auto& e : exitLocations) {
        if (e.first == exit) {
            return e.second;
        }
    }
    return kNoLocations;
}

bool buildExitLocationMap(ExitLocationMap& out, std::string* err) {
    const Span<Location> locs = locations();
    out.locationExit.assign(locs.size, -1);
    out.exitLocations.clear();
    auto dependent = [&](int exit) -> std::vector<int>& {
        for (auto& e : out.exitLocations) {
            if (e.first == exit) {
                return e.second;
            }
        }
        out.exitLocations.emplace_back(exit, std::vector<int>());
        return out.exitLocations.back().second;
    };
    const int highestIsle = location_index("Cliff Plateau Isles - Highest Isle");
    for (int loc = 0; loc < (int)locs.size; loc++) {
        int exit = -1;
        if (!zoneExitForItemLocationIndex(loc, exit, err)) {
            return false;
        }
        if (exit >= 0) {
            out.locationExit[loc] = exit;
            dependent(exit).push_back(loc);
        }
        if (std::strcmp(locs[loc].name, "The Great Sea - Withered Trees") == 0 && highestIsle >= 0) {
            // Not inside a zone exit, but it needs an item location that is.
            int sub = -1;
            if (!zoneExitForItemLocationIndex(highestIsle, sub, err)) {
                return false;
            }
            if (sub >= 0) {
                dependent(sub).push_back(loc);
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Connections and nesting

EntranceConnections EntranceConnections::defaults() {
    EntranceConnections c;
    for (int i = 0; i < kNumZoneExits; i++) {
        c.exitToEntrance[i] = -1;
    }
    for (int i = 0; i < kNumZoneEntrances; i++) {
        c.entranceToExit[i] = -1;
    }
    for (int i = 0; i < kNumZoneEntrances; i++) {
        c.connect(i, defaultConnectionExit(i));
    }
    return c;
}

EntrancePath entrancesOnPathToEntrance(const EntranceConnections& c, int entrance, std::vector<int>& out,
                                       std::string* err) {
    out.clear();
    while (kZoneEntrances[entrance].isNested()) {
        for (int seen : out) {
            if (seen == entrance) {
                if (err != nullptr) {
                    std::string path;
                    for (size_t i = 0; i < out.size(); i++) {
                        path += (i ? ", " : "") + std::string(kZoneEntrances[out[i]].name);
                    }
                    *err = "Entrances are in an infinite loop: " + path;
                }
                return EntrancePath::Loop;
            }
        }
        out.push_back(entrance);
        const int outer = c.exitToEntrance[kZoneEntrances[entrance].nestedIn];
        if (outer < 0) {
            return EntrancePath::Undecided;
        }
        entrance = outer;
    }
    out.push_back(entrance);
    return EntrancePath::Ok;
}

int outermostEntranceForEntrance(const EntranceConnections& c, int entrance, std::string* err) {
    std::vector<int> path;
    if (entrancesOnPathToEntrance(c, entrance, path, err) != EntrancePath::Ok) {
        return -1;
    }
    return path.back();
}

int outermostEntranceForExit(const EntranceConnections& c, int exit, std::string* err) {
    const int entrance = c.exitToEntrance[exit];
    if (entrance < 0) {
        // done_exits_to_entrances[zone_exit] raises KeyError upstream.
        if (err != nullptr) {
            *err = "KeyError: ZoneExit('" + std::string(kZoneExits[exit].name) + "')";
        }
        return -1;
    }
    return outermostEntranceForEntrance(c, entrance, err);
}

} // namespace pcr
