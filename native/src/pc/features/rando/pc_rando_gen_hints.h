// pc_rando_gen_hints.h - the hint generator (docs/RANDOMIZER_PLAN.md step R5.1): a port of wwrando
// 1.10.0_9775811's randomizers/hints.py HintsRandomizer._randomize, plus the entrance-zone helpers of
// randomizers/entrances.py it reads (get_entrance_zone_for_item_location,
// get_all_zones_for_item_location and the zone-exit lookup behind them).
//
//   - importance (required / possibly required / not required) from the path and barren sets;
//   - path logic: a second Logic (Logic(self.rando)) that replays a playthrough without one
//     location's item to see which path goals it is required for;
//   - barren zones, weighted by sqrt(number of progress locations in the zone);
//   - item, location and remote hints;
//   - the order remote -> path -> barren -> item -> location, then the Savage Labyrinth and Big
//     Octo Great Fairy hints, and the round robin over the enabled placements (fishmen, Ho Ho,
//     KoRL), with the fishmen and Ho Ho distributions.
//
// Exactness: every RNG draw and list mutation follows hints.py. Its Python sets of strings are only
// used for membership, unions and a sorted() result, so their hash order never reaches the RNG; the
// hints are byte-identical to wwrando's for the same placement (check_seeds.py --golden compares the
// spoiler log's Hints section and the hints JSON). Decision D1 only requires validity
// (ww_verify_hints.py).
//
// The RNG is the HintsRandomizer's own (a fresh get_new_rng(), BaseRandomizer.reset_rng), so item
// placement does not depend on the hints.
//
// What other randomizers decided is read from logic.state() (entrance connections, required-bosses
// banned locations) and from HintsInput (required dungeons, which LogicState does not hold).
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and the Mac CLI link it.
#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "pc_rando_json.h"
#include "pc_rando_logic.h"
#include "pc_rando_rng.h"

namespace pcr {

// hints.py HintType and ItemImportance.
enum class HintType : uint8_t { Path, Barren, Item, Location, FixedLocation };
enum class HintImportance : int8_t { None = -1, Required = 0, PossiblyRequired = 1, NotRequired = 2 };

// hints.py Hint: place is a zone (path, barren, item) or a location name (location, fixed
// location); reward is the boss name (path), the item name (item, location, fixed location) or
// empty (barren: None).
struct Hint {
    HintType type = HintType::Item;
    std::string place;
    std::string reward;
    HintImportance importance = HintImportance::None;

    bool hasReward() const { return type != HintType::Barren; }
    bool operator==(const Hint& o) const {
        return type == o.type && place == o.place && reward == o.reward && importance == o.importance;
    }
};

// The hint placement options, in hints.py's variable_hint_placement_options order.
enum class HintPlacement : uint8_t { Fishmen, Hoho, Korl };
const char* hintPlacementName(HintPlacement p); // "fishmen_hints", "hoho_hints", "korl_hints"

constexpr int kHohoCount = 10;
// HOHO_INDEX_TO_ISLAND_NUM: the sea room of each Ho Ho (index 2's room has him on several layers).
extern const int kHohoIndexToIsland[kHohoCount];

struct HintsInput {
    // boss_reqs.required_dungeons, in its order (empty without required bosses).
    std::vector<std::string> requiredDungeons;
    // WWRandomizer.all_randomized_progress_items (RandomizerLogicInit, ids of the main Logic).
    std::vector<int> allRandomizedProgressItems;
};

struct HintsResult {
    // floor_30_hint / floor_50_hint (always made).
    Hint floor30;
    Hint floor50;
    // octo_fairy_hint: absent when no randomized progress item can be hinted at (wwrando then
    // leaves every hint text vanilla except the Savage Labyrinth tablet).
    bool hasOctoFairy = false;
    Hint octoFairy;
    // hints_per_placement, dict order (the enabled placements; empty when no hint is placed).
    std::vector<std::pair<HintPlacement, std::vector<Hint>>> placements;
    // island_to_fishman_hint: index 1..49 (0 unused); filled only when fishmen hints are placed.
    std::vector<Hint> fishmen;
    // hoho_index_to_hints, and the island each Ho Ho turns to (rotate_hoho_to_face_hint: the first
    // of his hints that names an island; -1 keeps his vanilla rotation). Only with Ho Ho hints.
    std::vector<std::vector<Hint>> hoho;
    std::vector<int> hohoLookIsland;
    // KoRL's hints (hints_per_placement["korl_hints"]), when placed.
    std::vector<Hint> korl;

    // The exception wwrando raises, when generation fails.
    std::string error;

    const std::vector<Hint>* placement(HintPlacement p) const;
};

// HintsRandomizer._randomize on the Logic the item fill filled, with `rng` fresh from
// get_new_rng(). False (and out.error) where wwrando raises.
bool generateHints(Logic& logic, pc::rando::PyRandom& rng, const HintsInput& in, HintsResult& out);

// BaseRandomizer.randomize() for the HintsRandomizer: a fresh RNG, COS_RANDO_TRACE attached (owner
// "HintsRandomizer"), then generateHints.
bool runHintsRandomizer(Logic& logic, const HintsInput& in, const std::string& sanitizedSeed, HintsResult& out,
                        const char* seedKey = kSwitchWakerSeedKey);

// HintsRandomizer.write_to_spoiler_log's rows (place, reward or "Nothing"). False (and *err, the
// AttributeError wwrando raises) when there is no Big Octo Great Fairy hint.
bool hintSpoilerRows(Logic& logic, const HintsResult& h, std::vector<std::pair<std::string, std::string>>& rows,
                     std::string* err);

// hints.py check_item_can_be_hinted_at.
bool checkItemCanBeHintedAt(const Logic& logic, int item);

// The placement's "hints" value (PcRandoSeed::hints; R5.2 and R5.3 read it):
//   {"format": 1,
//    "savage_labyrinth": [<floor 30>, <floor 50>],
//    "octo_fairy": <hint> | null,
//    "placements": {"fishmen_hints": [<hint>...], ...}       (hints_per_placement, dict order)
//    "fishmen": [<island 1>, ..., <island 49>]              (only with fishmen hints)
//    "hoho": [{"hints": [<hint>...], "look_island": n|null}, x10]   (only with Ho Ho hints)
//    "korl": [<hint>...]}                                   (only with KoRL hints)
// <hint> = {"type": "path"|"barren"|"item"|"location"|"fixed_location", "place": "...",
//           "reward": "..."|null, "importance": "required"|"possibly required"|"not required"|null}
constexpr int kHintsJsonFormat = 1;
Json hintsToJson(const HintsResult& h);
// The reverse. False (and *err) on a malformed value.
bool hintsFromJson(const Json& j, HintsResult& out, std::string* err);
const char* hintTypeName(HintType t);
// Hint.formatted_importance ("" for None).
const char* hintImportanceName(HintImportance i);

// --- entrances.py helpers (the connections come from logic.state().entranceConnections) ---------

// is_item_location_behind_randomizable_entrance.
bool isItemLocationBehindRandomizableEntrance(const Logic& logic, int loc);
// get_zone_exit_for_item_location: the ZoneExit unique_name, or "" (None).
std::string zoneExitForItemLocation(const Logic& logic, int loc);
// get_entrance_zone_for_item_location. False (and *err) where wwrando raises.
bool entranceZoneForItemLocation(const Logic& logic, int loc, std::string& out, std::string* err);
// get_all_zones_for_item_location (a set: no duplicates, order unspecified).
bool allZonesForItemLocation(const Logic& logic, int loc, std::vector<std::string>& out, std::string* err);

} // namespace pcr
