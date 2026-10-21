// pc_rando_entrances.h - entrance randomizer tables and sets (docs/RANDOMIZER_PLAN.md step R6.10): the
// data half of wwrando 1.10.0_9775811's randomizers/entrances.py, and what EntranceRandomizer computes
// from the options before it randomizes anything:
//   - the ZoneEntrance / ZoneExit tables (44 + 44, in ZoneEntrance.all / ZoneExit.all definition
//     order: DUNGEON 5, MINIBOSS 5, BOSS 6, SECRET_CAVE 20, SECRET_CAVE_INNER 2, FAIRY_FOUNTAIN 6),
//     with every field the runtime (R6.12-R6.13) needs: stage, room, SCLS exit index, spawn id,
//     island, warp-out stage/room/spawn, nested_in, boss stage and zone name;
//   - the per-group lists (DUNGEON_EXITS, BOSS_EXITS, ...) as index ranges;
//   - the default entrance_connections;
//   - ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES (15 location -> exit overrides) and
//     register_mappings_between_item_locations_and_zone_exits (item_location_to_containing_zone_exit,
//     zone_exit_to_logically_dependent_item_locations);
//   - the *_NAMES_WITH_NO_REQUIREMENTS lists, entrance_names_with_no_requirements,
//     exit_names_with_no_requirements, nesting_enabled and is_enabled;
//   - get_all_entrance_sets_to_be_randomized per mix_entrances;
//   - the nesting walks over a connection state (get_all_entrances_on_path_to_entrance,
//     get_outermost_entrance_for_entrance / _for_exit), which the placement (R6.11) uses.
// The placement itself (randomize_one_set_of_entrances and the rest) is R6.11.
//
// Entrances and exits are indices into zoneEntrances() / zoneExits(); -1 stands for Python's None.
// Locations are indices into pcr::locations() (item_locations.txt order, = Logic.item_locations).
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and the Mac CLI link it.
#pragma once

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "pc_rando_data.h"
#include "pc_rando_logic.h" // EntranceGroup
#include "pc_rando_options.h"

namespace pcr {

constexpr int kNumZoneEntrances = 44;
constexpr int kNumZoneExits = 44;
constexpr int kNumEntranceGroups = 6;

// One ZoneEntrance. Exactly one of islandName (an island entrance) and nestedIn (an entrance inside
// another zone exit) is set; the warp-out fields are set with islandName.
struct ZoneEntranceDef {
    const char* stage;          // stage_name
    int8_t room;                // room_num
    int8_t sclsExitIndex;       // scls_exit_index
    uint8_t spawnId;            // spawn_id
    const char* name;           // entrance_name
    const char* islandName;     // island_name, or nullptr
    const char* warpOutStage;   // warp_out_stage_name, or nullptr
    int8_t warpOutRoom;         // warp_out_room_num, or -1
    int16_t warpOutSpawn;       // warp_out_spawn_id, or -1
    int8_t nestedIn;            // nested_in (a zoneExits() index), or -1
    EntranceGroup group;        // the *_ENTRANCES list it is in
    bool isNested() const { return nestedIn >= 0; }
};

// One ZoneExit.
struct ZoneExitDef {
    const char* stage;          // stage_name
    int8_t room;                // room_num
    int8_t sclsExitIndex;       // scls_exit_index, or -1 (boss arenas)
    uint8_t spawnId;            // spawn_id
    const char* name;           // unique_name
    const char* bossStageName;  // boss_stage_name, or nullptr
    const char* zoneName;       // zone_name, or nullptr
    EntranceGroup group;        // the *_EXITS list it is in
};

Span<ZoneEntranceDef> zoneEntrances();
Span<ZoneExitDef> zoneExits();
// ZoneEntrance.all[name] / ZoneExit.all[name], or -1.
int zoneEntranceIndex(std::string_view name);
int zoneExitIndex(std::string_view name);

// A *_ENTRANCES or *_EXITS list: the indices [first, first + count) (each group is contiguous in
// definition order).
struct IndexRange {
    int first;
    int count;
    bool contains(int i) const { return i >= first && i < first + count; }
};
IndexRange entranceGroup(EntranceGroup g);
IndexRange exitGroup(EntranceGroup g);
// The Python list name, "DUNGEON" .. "FAIRY_FOUNTAIN" (DUNGEON_ENTRANCES = "<name>_ENTRANCES").
const char* entranceGroupListName(EntranceGroup g);

// EntranceRandomizer.entrance_connections before randomization: the zoneExits() index of each
// entrance's vanilla exit (dict order = zoneEntrances() order).
int defaultConnectionExit(int entrance);

// ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES, dict order.
struct LocationExitOverride {
    const char* location;
    int8_t exit;
};
Span<LocationExitOverride> itemLocationExitOverrides();

// Module-level name lists.
StrSpan dungeonEntranceNamesWithNoRequirements();
StrSpan secretCaveEntranceNamesWithNoRequirements();
StrSpan dungeonExitNamesWithNoRequirements();
StrSpan puzzleSecretCaveExitNamesWithNoRequirements();
StrSpan combatSecretCaveExitNamesWithNoRequirements();
StrSpan entranceRandomizableItemLocationTypes();

// EntranceRandomizer.__init__'s option-derived lists, in Python's order.
std::vector<std::string> entranceNamesWithNoRequirements(const Options& o);
std::vector<std::string> exitNamesWithNoRequirements(const Options& o);
// nesting_enabled: miniboss, boss or secret cave inner entrances randomized.
bool entranceNestingEnabled(const Options& o);
// is_enabled: any randomize_*_entrances option.
bool entranceRandoEnabled(const Options& o);

// get_all_entrance_sets_to_be_randomized: the (relevant_entrances, relevant_exits) pairs, in yield
// order. False (and *err) where the generator raises ("An invalid combination of entrance randomizer
// options was selected.", which includes no entrance option on: callers only ask when is_enabled).
struct EntranceSet {
    std::vector<int> entrances;
    std::vector<int> exits;
};
bool entranceSetsToBeRandomized(const Options& o, std::vector<EntranceSet>& out, std::string* err);

// --- Item locations and zone exits ----------------------------------------------------------------

// is_item_location_behind_randomizable_entrance (location zones and types come from the data tables).
bool itemLocationBehindRandomizableEntrance(int loc);
// get_zone_exit_for_item_location: a zoneExits() index, or -1 (None). False (and *err) where
// wwrando raises (several exits share the location's zone; cannot happen with its data).
bool zoneExitForItemLocationIndex(int loc, int& exit, std::string* err);

// register_mappings_between_item_locations_and_zone_exits.
struct ExitLocationMap {
    // item_location_to_containing_zone_exit: per location, a zoneExits() index or -1.
    std::vector<int> locationExit;
    // zone_exit_to_logically_dependent_item_locations: (exit, locations), dict insertion order;
    // "The Great Sea - Withered Trees" is appended to the exit of "Cliff Plateau Isles - Highest Isle".
    std::vector<std::pair<int, std::vector<int>>> exitLocations;
    // The dependent locations of an exit (empty when none).
    const std::vector<int>& locationsFor(int exit) const;
};
bool buildExitLocationMap(ExitLocationMap& out, std::string* err);

// --- Connections and nesting ------------------------------------------------------------------------

// done_entrances_to_exits / done_exits_to_entrances: -1 where undecided (deleted from the dict).
struct EntranceConnections {
    int entranceToExit[kNumZoneEntrances];
    int exitToEntrance[kNumZoneExits];
    // The default connections (EntranceRandomizer.__init__).
    static EntranceConnections defaults();
    void connect(int entrance, int exit) {
        entranceToExit[entrance] = exit;
        exitToEntrance[exit] = entrance;
    }
};

// get_all_entrances_on_path_to_entrance: the entrance, then the entrance of each zone it is nested
// in, ending with an island entrance.
enum class EntrancePath : uint8_t { Ok, Undecided, Loop };
// Undecided (None) when a zone on the way has no entrance yet; Loop (and *err: "Entrances are in an
// infinite loop: a, b") where wwrando raises.
EntrancePath entrancesOnPathToEntrance(const EntranceConnections& c, int entrance, std::vector<int>& out,
                                       std::string* err);
// get_outermost_entrance_for_entrance / _for_exit: the island entrance, or -1 (undecided, or a loop:
// *err set).
int outermostEntranceForEntrance(const EntranceConnections& c, int entrance, std::string* err);
int outermostEntranceForExit(const EntranceConnections& c, int exit, std::string* err);

} // namespace pcr
