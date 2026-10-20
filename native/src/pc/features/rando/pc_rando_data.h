/*
 * pc_rando_data.h - wwrando's logic data as read-only C++ tables (docs/RANDOMIZER_PLAN.md, step R0.3).
 *
 * The tables are generated offline from wwrando 1.10.0_9775811 (MIT, ref/wwrando) by
 * native/tools/rando/gen_rando_data.py into the generated/ .inc files (decision D11); the build never runs
 * Python. Every list keeps wwrando's order (file order for YAML files and item_types.py, dict order
 * for Python dicts): the generator ports Python list semantics (D1), so the order is part of the data.
 *
 * Strings are the exact Python values. Location `need` and macro `body` are the requirement strings
 * as wwrando's YAML loader returns them (multi-line values folded with spaces, comments stripped);
 * R1.1 compiles them, and macros stay strings because wwrando rewrites them at runtime (set_macro).
 *
 * Pure: no game headers, no allocation, no static constructors (cos_rando_core and the unit tests
 * link it; native/check/rando/README.md). Lookups by name are linear scans over a few hundred
 * entries; callers that look up often intern the names once.
 */
#ifndef PC_RANDO_DATA_H
#define PC_RANDO_DATA_H

#include <stddef.h>
#include <stdint.h>

namespace pcr {

// A read-only array view.
template <class T> struct Span {
    const T* data;
    size_t size;
    const T* begin() const { return data; }
    const T* end() const { return data + size; }
    const T& operator[](size_t i) const { return data[i]; }
};
using StrSpan = Span<const char*>;

// --- Item locations (logic/item_locations.txt, 320 active entries in file order) ---------------

// How a location's item is stored in the game (randomizers/items.py change_item's path formats).
enum class PathKind : uint8_t {
    Chest,          // <stage>/<arc>[/Layer<l>]/Chest<idx>           TRES entry
    ScalableObject, // <stage>/<arc>[/Layer<l>]/ScalableObject<idx>  SCOB entry
    Actor,          // <stage>/<arc>[/Layer<l>]/Actor<idx>           ACTR entry
    Event,          // <stage>/<arc>/Event<idx>:<name>/Actor<a>/Action<b>   event_list.dat
    Rel,            // rels/<name>.rel@<offset>                       code in a REL
    MainDol,        // main.dol@<address>                             code in main.dol
    CustomSymbol,   // CustomSymbol:<symbol>                          wwrando asm data
};

struct LocPath {
    PathKind kind;
    int8_t layer;        // Chest/ScalableObject/Actor: layer 0-11, or -1 for the base layer
    uint16_t index;      // Chest/ScalableObject/Actor: entry index; Event: event index
    uint16_t actor;      // Event: actor index
    uint16_t action;     // Event: action index
    uint32_t offset;     // Rel: offset in the REL; MainDol: address
    const char* file;    // "<stage>/<arc>.arc", "rels/<name>.rel", "main.dol"; nullptr for CustomSymbol
    const char* name;    // Event: event name; CustomSymbol: symbol; else nullptr
    const char* raw;     // the path string as written in item_locations.txt
};

// Bit `i` of Location::types is loc_type_names()[i] (the Types tags, sorted by name).
enum LocType : uint32_t {
    LOC_BATTLESQUID = 1u << 0,
    LOC_BIG_OCTO = 1u << 1,
    LOC_BOSS = 1u << 2,
    LOC_COMBAT_SECRET_CAVE = 1u << 3,
    LOC_CONSUMABLES_ONLY = 1u << 4,
    LOC_DUNGEON = 1u << 5,
    LOC_DUNGEON_SECRET = 1u << 6,
    LOC_EXPENSIVE_PURCHASE = 1u << 7,
    LOC_EYE_REEF_CHEST = 1u << 8,
    LOC_FREE_GIFT = 1u << 9,
    LOC_GREAT_FAIRY = 1u << 10,
    LOC_GUNBOAT = 1u << 11,
    LOC_ISLAND_PUZZLE = 1u << 12,
    LOC_LONG_SIDEQUEST = 1u << 13,
    LOC_MAIL = 1u << 14,
    LOC_MINIGAME = 1u << 15,
    LOC_MISC = 1u << 16,
    LOC_NO_PROGRESSION = 1u << 17,
    LOC_OTHER_CHEST = 1u << 18,
    LOC_PLATFORM = 1u << 19,
    LOC_PUZZLE_SECRET_CAVE = 1u << 20,
    LOC_RAFT = 1u << 21,
    LOC_RANDOMIZABLE_MINIBOSS_ROOM = 1u << 22,
    LOC_SAVAGE_LABYRINTH = 1u << 23,
    LOC_SHORT_SIDEQUEST = 1u << 24,
    LOC_SPOILS_TRADING = 1u << 25,
    LOC_SUBMARINE = 1u << 26,
    LOC_SUNKEN_TREASURE = 1u << 27,
    LOC_TINGLE_CHEST = 1u << 28,
};
constexpr int kNumLocTypes = 29;

struct Location {
    const char* name;          // "Outset Island - Underneath Link's House"
    const char* need;          // requirement string ("Nothing", "A & (B | C)", ...)
    const char* original_item; // vanilla item name
    uint32_t types;            // LocType bits
    uint16_t first_path;       // into loc_paths()
    uint16_t num_paths;
};

Span<Location> locations();
Span<LocPath> loc_paths();
// The paths of one location.
Span<LocPath> location_paths(const Location& loc);
StrSpan loc_type_names();
// Index into locations(), or -1.
int location_index(const char* name);
// The LocType bit of a Types tag, or 0 when unknown.
uint32_t loc_type_bit(const char* tag);

// --- Macros (logic/macros.txt, 307 entries in file order) ---------------------------------------

struct Macro {
    const char* name;
    const char* body; // requirement string
};
Span<Macro> macros();
int macro_index(const char* name); // or -1

// --- Items -------------------------------------------------------------------------------------

// data/item_names.txt: every named item id (randomizer.py item_names / item_name_to_id).
struct ItemName {
    uint8_t id;
    const char* name;
};
Span<ItemName> item_names();
int item_id(const char* name);       // 0x00-0xFF, or -1
const char* item_name(uint8_t id);   // nullptr for an unnamed id

// logic/item_types.py, in file order with duplicates.
StrSpan progress_items();
StrSpan nonprogress_items();
StrSpan consumable_items();
StrSpan duplicatable_consumable_items();
StrSpan dungeon_progress_items();
StrSpan dungeon_nonprogress_items();
// "Triforce Chart 1".."8" and "Treasure Chart 1".."41" (Logic.__init__).
StrSpan triforce_chart_names();
StrSpan treasure_chart_names();
// Logic.all_cleaned_item_names: the first default_logic_item_count() entries are the list for
// wwrando's default options (153 names, Logic.__init__ order); "Ice Trap Chest" (trap_chests)
// follows. Every option combination's names are a subset of this list.
StrSpan logic_item_names();
size_t default_logic_item_count();
int logic_item_index(const char* name); // or -1

// wwr_ui/inventory.py (permalink starting_gear layout): REGULAR_ITEMS and PROGRESSIVE_ITEMS sorted as
// there; DEFAULT_STARTING_ITEMS.
StrSpan inventory_regular_items();
StrSpan inventory_progressive_items();
StrSpan default_starting_items();

// --- Misc tables -------------------------------------------------------------------------------

// data/island_names.txt: sea rooms 0-49 (0 is "Sea Floor"; islands are 1-49).
struct Island {
    uint8_t number;
    const char* room_arc; // "Room44.arc"
    const char* name;     // "Outset Island"
};
Span<Island> islands();
const char* island_name(int number); // nullptr when unknown

// randomizers/charts.py: the vanilla chart of each island, in dict order (island 1..49).
struct IslandChart {
    uint8_t island;
    const char* chart; // "Treasure Chart 25", "Triforce Chart 2", ...
};
Span<IslandChart> default_charts();
const char* default_chart_for_island(int island); // nullptr when not 1..49

// randomizer.py RNG_CHANGING_OPTIONS (11 option names; the index is the warm-up's i).
StrSpan rng_changing_options();

// randomizers/boss_reqs.py.
struct Boss {
    const char* name;            // "Gohma"
    uint8_t quest_marker_index;  // BOSS_NAME_TO_SEA_CHART_QUEST_MARKER_INDEX
    uint8_t stage_id;            // BOSS_NAME_TO_STAGE_ID
};
Span<Boss> bosses();

// logic/logic.py Logic.DUNGEON_NAMES.
struct DungeonName {
    const char* short_name; // "DRC"
    const char* name;       // "Dragon Roost Cavern"
};
Span<DungeonName> dungeon_names();

// seedgen/*.txt (random seed names; the seed hash uses names.txt).
StrSpan seed_adjectives();
StrSpan seed_nouns();
StrSpan seed_names();

// Hint data (R5): data/progress_item_hints.txt, data/zone_name_hints.txt, data/location_hints.txt,
// and randomizers/entrances.py ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES (location -> ZoneExit unique_name).
struct NameText {
    const char* name;
    const char* text;
};
struct LocationHint {
    const char* location;
    const char* type; // "Standard" or "Remote"
    const char* text;
};
Span<NameText> progress_item_hints();
Span<NameText> zone_name_hints();
Span<LocationHint> location_hints();
Span<NameText> exit_overrides();

// The wwrando commit the tables were generated from (full hash).
const char* data_source_commit();

} // namespace pcr

#endif
