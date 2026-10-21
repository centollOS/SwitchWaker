// pc_rando_logic.h - the randomizer's logic state and item pool (docs/RANDOMIZER_PLAN.md step R1.3): a
// port of wwrando 1.10.0_9775811's logic/logic.py Logic (everything except the requirement evaluator,
// which is R1.1's ReqLogic, and the enemy-rando helpers) plus the part of WWRandomizer.__init__ that
// sets the Logic up: the starting items, the TooFewProgressionLocationsError check,
// dungeons_only_start and dungeons_and_caves_only_start.
//
// Pure C++ (no game headers): cos_rando_core and the unit tests link it.
//
// Exactness (decision D1). The item fill reads these lists in order, so they keep Python's list
// semantics: remove() erases the first occurrence, append() adds at the end, and every mutation
// happens in wwrando's order (remove_owned_item appends to unplaced_progress_items only while
// all_progress_items holds more copies than unplaced_progress_items, so ownership probes move the
// probed item to the end). Items are ReqLogic name ids of the exact Python strings; locations are
// indices into pcr::locations() (item_locations.txt order).
//
// What other randomizers own (entrances, charts, required bosses, extra starting items) reaches the
// Logic through LogicState, which holds the WWRandomizer attributes the Logic reads.
// defaultLogicState() is what WWRandomizer.__init__ has before any randomizer runs (vanilla entrance
// connections and charts, nothing sampled yet); later steps fill in their parts and call the
// matching update_* function, as the Python randomizers do.
//
// Errors: where wwrando raises, the first error is recorded (failed()/error()), as in ReqLogic.
#pragma once

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "pc_rando_logic_expr.h"
#include "pc_rando_options.h"

namespace pcr {

// randomizers/entrances.py: the 44 ZoneEntrance -> ZoneExit pairs of EntranceRandomizer's default
// entrance_connections, in dict order (which is also ZoneEntrance.all's definition order and, per
// group, the *_ENTRANCES and *_EXITS list order).
enum class EntranceGroup : uint8_t { Dungeon, Miniboss, Boss, SecretCave, SecretCaveInner, FairyFountain };
struct VanillaEntrance {
    const char* entrance;  // ZoneEntrance.entrance_name
    const char* exit;      // ZoneExit.unique_name
    EntranceGroup group;
    const char* nestedIn;  // ZoneEntrance.nested_in.unique_name, or nullptr for an island entrance
};
const VanillaEntrance* vanillaEntrances(int* count);

// The WWRandomizer state the Logic reads.
struct LogicState {
    // WWRandomizer.starting_items (startingItemsFor).
    std::vector<std::string> startingItems;
    // entrances.entrance_connections: entrance name -> zone exit name, dict order.
    std::vector<std::pair<std::string, std::string>> entranceConnections;
    // ZoneEntrance.all's nested entrances: entrance name -> the unique_name of the exit it is in.
    std::vector<std::pair<std::string, std::string>> nestedEntrances;
    // DUNGEON_EXITS' unique names (temporarily_make_dungeon_entrance_macros_*).
    std::vector<std::string> dungeonExits;
    // entrances.is_enabled() and get_all_entrance_sets_to_be_randomized(): (entrance names, exit
    // names) per set. Empty when entrances are off.
    bool entrancesEnabled = false;
    std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> entranceSets;
    // charts.island_number_to_chart_name, islands 1..49 (index 0 unused).
    std::vector<std::string> islandCharts;
    // boss_reqs.is_enabled(), required_boss_item_locations and banned_locations (location indices).
    bool bossReqsEnabled = false;
    std::vector<std::string> requiredBossItemLocations;
    std::vector<int> bannedLocations;
    // extra_start_items.is_enabled() and random_starting_items.
    bool extraStartItemsEnabled = false;
    std::vector<std::string> randomStartingItems;
    // A non-empty text: building the state failed (an invalid entrance option combination, as
    // get_all_entrance_sets_to_be_randomized raises).
    std::string error;
};

// WWRandomizer.__init__'s starting_items: Wind Waker, Wind's Requiem, Boat's Sail, starting_gear in
// order, Progressive Sword when the sword mode starts with one, Triforce Shard 1..k, then the
// Pieces of Heart and Heart Containers.
std::vector<std::string> startingItemsFor(const Options& o);
// get_all_entrance_sets_to_be_randomized for the options (empty when no entrance option is on).
// False (and *err) for an invalid combination.
bool entranceSetsFor(const Options& o,
                     std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>>& sets,
                     std::string* err);
// The state WWRandomizer.__init__ hands the Logic before any randomizer runs.
LogicState defaultLogicState(const Options& o);

class Logic {
public:
    // save_simulated_playthrough_state.
    struct PlaythroughState {
        std::vector<int> owned;
        std::vector<int> unplacedProgress;
        std::vector<int> unplacedNonprogress;
        std::vector<int> unplacedFixedConsumables;
        ReqLogic::Snapshot req;
    };

    // Logic.__init__ with rando.fully_initialized False: the item lists for the options, the
    // cleaned item names, the macros and the locations. The unplaced lists stay empty until
    // initializeFromRandomizerState. False (and failed()) when the logic data does not compile.
    bool create(const Options& opts);
    // Logic.__init__ with fully_initialized True (a Logic made later by the item fill or hints):
    // create, then initializeFromRandomizerState.
    bool create(const Options& opts, const LogicState& state);
    // initialize_from_randomizer_state: nested-entrance macros, entrance and chart macros, the
    // unplaced lists, the starting items, make_useless_progress_items_nonprogress, the extra
    // starting items. The state is copied (state() for later updates).
    bool initializeFromRandomizerState(const LogicState& state);

    const Options& options() const { return mOpts; }
    LogicState& state() { return mState; }
    const LogicState& state() const { return mState; }
    ReqLogic& req() { return mReq; }
    const ReqLogic& req() const { return mReq; }

    // --- Names ---
    int item(const std::string& name) { return mReq.intern(name); }
    const std::string& name(int id) const { return mReq.name(id); }
    // clean_item_name: parentheses removed.
    int cleanItem(int id);
    static std::string cleanItemName(const std::string& name);
    int locationCount() const { return (int)mLocZone.size(); }
    int locationIndex(const std::string& name) const;
    const char* locationName(int loc) const;
    // split_location_name_by_zone's zone name.
    const std::string& locationZone(int loc) const { return mLocZone[loc]; }
    uint32_t locationTypes(int loc) const;

    // --- Lists (Python lists of name ids, in order) ---
    std::vector<int> allProgressItems;
    std::vector<int> allNonprogressItems;
    std::vector<int> allFixedConsumableItems;
    std::vector<int> duplicatableConsumableItems;
    std::vector<int> triforceChartNames;
    std::vector<int> treasureChartNames;
    std::vector<int> allCleanedItemNames;
    std::vector<int> unplacedProgressItems;
    std::vector<int> unplacedNonprogressItems;
    std::vector<int> unplacedFixedConsumableItems;
    std::vector<int> currentlyOwnedItems; // cleaned names
    // Locations.
    std::vector<int> remainingItemLocations;
    std::vector<int> doneItemLocations;   // per location: the item placed, or -1 (None)
    std::vector<std::pair<int, int>> prerandomizationItemLocations; // location -> item, dict order
    std::vector<int> rockSpireShopShipLocations;
    // locations_by_zone_name: zone name -> location indices, dict order.
    std::vector<std::pair<std::string, std::vector<int>>> locationsByZoneName;

    // --- Inventory ---
    void addOwnedItem(int item);
    void removeOwnedItem(int item);
    void addOwnedItem(const std::string& name) { addOwnedItem(item(name)); }
    void removeOwnedItem(const std::string& name) { removeOwnedItem(item(name)); }
    // add_temporary_items' two halves (the context manager's enter and exit).
    void addTemporaryItems(const std::vector<int>& items);
    void removeTemporaryItems(const std::vector<int>& items);
    void clearReqCaches() { mReq.clearCaches(); }
    PlaythroughState saveSimulatedPlaythroughState() const;
    void loadSimulatedPlaythroughState(const PlaythroughState& s);

    void setLocationToItem(int loc, int item);
    void setPrerandomizationItemLocation(int loc, int item);
    // The prerandomized item of a location, or -1.
    int prerandomizedItem(int loc) const;

    // --- Locations ---
    int getNumProgressionLocations() const { return getNumProgressionLocationsStatic(mOpts); }
    static int getNumProgressionLocationsStatic(const Options& o);
    int getMaxRequiredBossesBannedLocations() const;
    void getProgressAndNonProgressLocations(std::vector<int>& progress, std::vector<int>& nonprogress);
    void getAccessibleRemainingLocations(bool forProgression, std::vector<int>& out);
    void filterLocationsForProgression(const std::vector<int>& in, std::vector<int>& out,
                                       bool filterSunkenTreasure = false) const {
        filterLocationsForProgressionStatic(in, out, mOpts, filterSunkenTreasure);
    }
    static void filterLocationsForProgressionStatic(const std::vector<int>& in, std::vector<int>& out,
                                                    const Options& o, bool filterSunkenTreasure = false);
    // One location of filter_locations_for_progression_static.
    static bool locationIsProgression(int loc, const Options& o, bool filterSunkenTreasure);
    bool checkLocationAccessible(int loc) { return mReq.checkLocationAccessible(loc); }

    // --- Items and locations ---
    bool checkItemValidInLocation(int item, int loc) const;
    void filterItemsByAnyValidLocation(const std::vector<int>& items, const std::vector<int>& locs,
                                       std::vector<int>& out) const;
    void filterLocationsValidForItem(const std::vector<int>& locs, int item, std::vector<int>& out) const;
    void filterItemsValidForLocation(const std::vector<int>& items, int loc, std::vector<int>& out) const;
    bool isDungeonItem(int item) const;
    // is_dungeon_location; dungeonName nullptr for any dungeon.
    bool isDungeonLocation(int loc, const char* dungeonName = nullptr) const;
    // chart_name_for_location (a Sunken Treasure location), or -1 (and failed()).
    int chartNameForLocation(int loc);

    // --- Usefulness (used by the item fill and extra starting items) ---
    // get_first_useful_item: the first item that opens a new progress location, or -1.
    int getFirstUsefulItem(const std::vector<int>& items);
    // get_items_by_usefulness_fraction: item -> fraction, in the order of `items` (duplicates once).
    void getItemsByUsefulnessFraction(const std::vector<int>& items, bool filterSunkenTreasure,
                                      std::vector<std::pair<int, int>>& out);
    // get_all_useless_items.
    void getAllUselessItems(const std::vector<int>& items, std::vector<int>& out);
    // get_item_names_by_req_name / get_item_names_from_logical_expression_req (flattened).
    void getItemNamesByReqName(const std::string& req, std::vector<int>& out);
    void getItemNamesForLocation(int loc, std::vector<int>& out);

    // --- Macros ---
    bool setMacro(const std::string& name, const std::string& body) { return mReq.setMacro(name, body); }
    void updateEntranceConnectionMacros();
    void updateChartMacros();
    void updateRequiredBossesMacro();
    void temporarilyMakeDungeonEntranceMacrosImpossible();
    void temporarilyMakeDungeonEntranceMacrosAccessible();
    void temporarilyMakeEntranceMacrosImpossible();
    void temporarilyMakeEntranceMacrosWorstCaseScenario();
    void temporarilyMakeOneSetOfEntranceMacrosWorstCaseScenario(const std::vector<std::string>& entrances,
                                                                const std::vector<std::string>& exits);
    void temporarilyMakeRequiredBossesMacroWorstCaseScenario();
    void makeUselessProgressItemsNonprogress();

    // --- Errors (where wwrando raises) ---
    bool failed() const { return !mError.empty() || mReq.failed(); }
    std::string error() const { return !mError.empty() ? mError : mReq.error(); }
    void fail(const std::string& msg) {
        if (mError.empty()) {
            mError = msg;
        }
    }

private:
    bool checkItemIsUseful(int item, const std::vector<int>& inaccessible);
    void inaccessibleProgressLocations(std::vector<int>& out);
    bool listHas(const std::vector<int>& v, int x) const;
    bool listRemove(std::vector<int>& v, int x, const char* what);
    static int listCount(const std::vector<int>& v, int x);
    bool isBanned(int loc) const;

    Options mOpts;
    LogicState mState;
    ReqLogic mReq;
    std::vector<std::string> mLocZone;
    std::vector<uint8_t> mLocDungeon;       // index into DUNGEON_NAMES + 1, or 0
    std::vector<uint8_t> mLocAllChests;     // every path's last part starts with "Chest"
    std::vector<uint8_t> mIsDungeonItem;    // by name id (grown on demand)
    std::vector<uint8_t> mIsConsumable;     // fixed or duplicatable consumable, by name id
    std::vector<std::pair<int, uint8_t>> mUsefulCache; // cached_items_are_useful
    bool mUsefulCacheOn = false;
    int mBeedle20 = -1, mZunari = -1;
    int mAllPurposeBait = -1, mHyoiPear = -1, mTownFlower = -1, mBoatsSail = -1;
    std::string mError;
};

// WWRandomizer.__init__'s Logic setup and checks.
struct RandomizerLogicInit {
    std::vector<int> allRandomizedProgressItems; // logic.unplaced_progress_items copy
    int numProgressLocations = 0;
    int maxRequiredBossesBannedLocations = 0;
    bool dungeonsOnlyStart = false;
    bool dungeonsAndCavesOnlyStart = false;
    bool tooFewProgressionLocations = false;     // TooFewProgressionLocationsError; error holds its text
    std::string error;
};
// Runs Logic.__init__ + initialize_from_randomizer_state on `state`, the progress location count
// check, then the dungeons-only and dungeons-and-caves-only start probes. False on any error
// (out.error; logic keeps the state it had when it stopped, as wwrando's Logic does when it raises).
bool initRandomizerLogic(Logic& logic, const Options& opts, const LogicState& state, RandomizerLogicInit& out);

} // namespace pcr
