// pc_rando_logic.cpp - the randomizer's logic state and item pool (docs/RANDOMIZER_PLAN.md step R1.3):
// a port of wwrando 1.10.0_9775811's logic/logic.py Logic and the Logic setup of
// WWRandomizer.__init__. See pc_rando_logic.h. Pure C++ (cos_rando_core).
#include "pc_rando_logic.h"

#include <algorithm>
#include <cstring>

#include "pc_rando_data.h"

namespace pcr {

namespace {

using G = EntranceGroup;

// randomizers/entrances.py EntranceRandomizer.entrance_connections (default), with each entrance's
// group and nested_in (ZoneEntrance definitions).
const VanillaEntrance kVanillaEntrances[] = {
    {"Dungeon Entrance on Dragon Roost Island", "Dragon Roost Cavern", G::Dungeon, nullptr},
    {"Dungeon Entrance in Forest Haven Sector", "Forbidden Woods", G::Dungeon, nullptr},
    {"Dungeon Entrance in Tower of the Gods Sector", "Tower of the Gods", G::Dungeon, nullptr},
    {"Dungeon Entrance on Headstone Island", "Earth Temple", G::Dungeon, nullptr},
    {"Dungeon Entrance on Gale Isle", "Wind Temple", G::Dungeon, nullptr},

    {"Miniboss Entrance in Forbidden Woods", "Forbidden Woods Miniboss Arena", G::Miniboss, "Forbidden Woods"},
    {"Miniboss Entrance in Tower of the Gods", "Tower of the Gods Miniboss Arena", G::Miniboss, "Tower of the Gods"},
    {"Miniboss Entrance in Earth Temple", "Earth Temple Miniboss Arena", G::Miniboss, "Earth Temple"},
    {"Miniboss Entrance in Wind Temple", "Wind Temple Miniboss Arena", G::Miniboss, "Wind Temple"},
    {"Miniboss Entrance in Hyrule Castle", "Master Sword Chamber", G::Miniboss, nullptr},

    {"Boss Entrance in Dragon Roost Cavern", "Gohma Boss Arena", G::Boss, "Dragon Roost Cavern"},
    {"Boss Entrance in Forbidden Woods", "Kalle Demos Boss Arena", G::Boss, "Forbidden Woods"},
    {"Boss Entrance in Tower of the Gods", "Gohdan Boss Arena", G::Boss, "Tower of the Gods"},
    {"Boss Entrance in Forsaken Fortress", "Helmaroc King Boss Arena", G::Boss, nullptr},
    {"Boss Entrance in Earth Temple", "Jalhalla Boss Arena", G::Boss, "Earth Temple"},
    {"Boss Entrance in Wind Temple", "Molgera Boss Arena", G::Boss, "Wind Temple"},

    {"Secret Cave Entrance on Outset Island", "Savage Labyrinth", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Dragon Roost Island", "Dragon Roost Island Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Fire Mountain", "Fire Mountain Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Ice Ring Isle", "Ice Ring Isle Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Private Oasis", "Cabana Labyrinth", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Needle Rock Isle", "Needle Rock Isle Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Angular Isles", "Angular Isles Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Boating Course", "Boating Course Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Stone Watcher Island", "Stone Watcher Island Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Overlook Island", "Overlook Island Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Bird's Peak Rock", "Bird's Peak Rock Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Pawprint Isle", "Pawprint Isle Chuchu Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Pawprint Isle Side Isle", "Pawprint Isle Wizzrobe Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Diamond Steppe Island", "Diamond Steppe Island Warp Maze Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Bomb Island", "Bomb Island Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Rock Spire Isle", "Rock Spire Isle Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Shark Island", "Shark Island Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Cliff Plateau Isles", "Cliff Plateau Isles Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Horseshoe Island", "Horseshoe Island Secret Cave", G::SecretCave, nullptr},
    {"Secret Cave Entrance on Star Island", "Star Island Secret Cave", G::SecretCave, nullptr},

    {"Inner Entrance in Ice Ring Isle Secret Cave", "Ice Ring Isle Inner Cave", G::SecretCaveInner,
     "Ice Ring Isle Secret Cave"},
    {"Inner Entrance in Cliff Plateau Isles Secret Cave", "Cliff Plateau Isles Inner Cave", G::SecretCaveInner,
     "Cliff Plateau Isles Secret Cave"},

    {"Fairy Fountain Entrance on Outset Island", "Outset Fairy Fountain", G::FairyFountain, nullptr},
    {"Fairy Fountain Entrance on Thorned Fairy Island", "Thorned Fairy Fountain", G::FairyFountain, nullptr},
    {"Fairy Fountain Entrance on Eastern Fairy Island", "Eastern Fairy Fountain", G::FairyFountain, nullptr},
    {"Fairy Fountain Entrance on Western Fairy Island", "Western Fairy Fountain", G::FairyFountain, nullptr},
    {"Fairy Fountain Entrance on Southern Fairy Island", "Southern Fairy Fountain", G::FairyFountain, nullptr},
    {"Fairy Fountain Entrance on Northern Fairy Island", "Northern Fairy Fountain", G::FairyFountain, nullptr},
};
constexpr int kNumVanillaEntrances = (int)(sizeof(kVanillaEntrances) / sizeof(kVanillaEntrances[0]));
static_assert(kNumVanillaEntrances == 44, "entrances.py has 44 entrance connections");

bool startsWith(const std::string& s, const char* p) { return s.compare(0, std::strlen(p), p) == 0; }
bool contains(const std::string& s, const char* p) { return s.find(p) != std::string::npos; }

void strs(StrSpan span, std::vector<std::string>& out) {
    for (const char* s : span) {
        out.emplace_back(s);
    }
}

// The one-word dungeon prefix of an item name, as item_name.split(" ")[0].
std::string firstWord(const std::string& s) {
    size_t sp = s.find(' ');
    return sp == std::string::npos ? s : s.substr(0, sp);
}

// items_needed[name] = max(n, items_needed.setdefault(name, 0)).
void mergeMax(ItemsNeeded& into, int name, int count) {
    for (ItemCount& ic : into) {
        if (ic.name == name) {
            ic.count = std::max(ic.count, count);
            return;
        }
    }
    into.push_back({name, std::max(count, 0)});
}

} // namespace

const VanillaEntrance* vanillaEntrances(int* count) {
    if (count != nullptr) {
        *count = kNumVanillaEntrances;
    }
    return kVanillaEntrances;
}

// ---------------------------------------------------------------------------------------------
// The randomizer state

std::vector<std::string> startingItemsFor(const Options& o) {
    std::vector<std::string> items = {"Wind Waker", "Wind's Requiem", "Boat's Sail"};
    items.insert(items.end(), o.starting_gear.begin(), o.starting_gear.end());
    if (o.sword_mode == SwordMode::StartWithSword) {
        items.emplace_back("Progressive Sword");
    }
    for (int i = 0; i < o.num_starting_triforce_shards; i++) {
        items.push_back("Triforce Shard " + std::to_string(i + 1));
    }
    for (int i = 0; i < o.starting_pohs; i++) {
        items.emplace_back("Piece of Heart");
    }
    for (int i = 0; i < o.starting_hcs; i++) {
        items.emplace_back("Heart Container");
    }
    return items;
}

bool entranceSetsFor(const Options& o,
                     std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>>& sets,
                     std::string* err) {
    sets.clear();
    const bool dungeons = o.randomize_dungeon_entrances;
    const bool minibosses = o.randomize_miniboss_entrances;
    const bool bosses = o.randomize_boss_entrances;
    const bool caves = o.randomize_secret_cave_entrances;
    const bool innerCaves = o.randomize_secret_cave_inner_entrances;
    const bool fountains = o.randomize_fairy_fountain_entrances;
    const bool anyDungeons = dungeons || minibosses || bosses;
    const bool anyNonDungeons = caves || innerCaves || fountains;
    if (!anyDungeons && !anyNonDungeons) {
        return true; // entrances.is_enabled() is False: no set is ever asked for
    }
    // get_one_entrance_set: the groups in DUNGEON, MINIBOSS, BOSS, SECRET_CAVE, SECRET_CAVE_INNER,
    // FAIRY_FOUNTAIN order, which is the table's order.
    auto oneSet = [&](bool d, bool mb, bool b, bool c, bool ic, bool f) {
        std::pair<std::vector<std::string>, std::vector<std::string>> s;
        for (const VanillaEntrance& e : kVanillaEntrances) {
            bool on = (e.group == G::Dungeon && d) || (e.group == G::Miniboss && mb) || (e.group == G::Boss && b) ||
                      (e.group == G::SecretCave && c) || (e.group == G::SecretCaveInner && ic) ||
                      (e.group == G::FairyFountain && f);
            if (on) {
                s.first.emplace_back(e.entrance);
                s.second.emplace_back(e.exit);
            }
        }
        sets.push_back(std::move(s));
    };
    if (o.mix_entrances == EntranceMixMode::SeparateDungeons && anyDungeons && anyNonDungeons) {
        oneSet(dungeons, minibosses, bosses, false, false, false);
        oneSet(false, false, false, caves, innerCaves, fountains);
    } else if (o.mix_entrances == EntranceMixMode::SeparateDungeons || o.mix_entrances == EntranceMixMode::MixDungeons) {
        oneSet(dungeons, minibosses, bosses, caves, innerCaves, fountains);
    } else {
        if (err != nullptr) {
            *err = "An invalid combination of entrance randomizer options was selected.";
        }
        return false;
    }
    return true;
}

LogicState defaultLogicState(const Options& o) {
    LogicState s;
    s.startingItems = startingItemsFor(o);
    for (const VanillaEntrance& e : kVanillaEntrances) {
        s.entranceConnections.emplace_back(e.entrance, e.exit);
        if (e.nestedIn != nullptr) {
            s.nestedEntrances.emplace_back(e.entrance, e.nestedIn);
        }
        if (e.group == G::Dungeon) {
            s.dungeonExits.emplace_back(e.exit);
        }
    }
    s.entrancesEnabled = o.randomize_dungeon_entrances || o.randomize_secret_cave_entrances ||
                         o.randomize_miniboss_entrances || o.randomize_boss_entrances ||
                         o.randomize_secret_cave_inner_entrances || o.randomize_fairy_fountain_entrances;
    if (s.entrancesEnabled) {
        entranceSetsFor(o, s.entranceSets, &s.error);
    }
    s.islandCharts.assign(50, std::string());
    for (const IslandChart& c : default_charts()) {
        if (c.island < 50) {
            s.islandCharts[c.island] = c.chart;
        }
    }
    s.bossReqsEnabled = o.required_bosses;
    // ExtraStartingItemsRandomizer.is_enabled: items.is_enabled() (always, outside wwrando's
    // --noitemrando debug flag) and num_extra_starting_items > 0.
    s.extraStartItemsEnabled = o.num_extra_starting_items > 0;
    return s;
}

// ---------------------------------------------------------------------------------------------
// Logic: construction

std::string Logic::cleanItemName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (c != '(' && c != ')') {
            out.push_back(c);
        }
    }
    return out;
}

int Logic::cleanItem(int id) {
    const std::string& n = mReq.name(id);
    if (n.find('(') == std::string::npos && n.find(')') == std::string::npos) {
        return id;
    }
    return mReq.intern(cleanItemName(n));
}

bool Logic::create(const Options& opts) {
    mOpts = opts;
    mState = LogicState();
    mError.clear();
    mUsefulCache.clear();
    mUsefulCacheOn = false;

    // Logic.__init__'s item lists, as strings first: the cleaned names must be known before the
    // requirement logic is built.
    std::vector<std::string> prog, nonprog, cons, dup, triforce, treasure;
    strs(progress_items(), prog);
    strs(nonprogress_items(), nonprog);
    strs(consumable_items(), cons);
    strs(duplicatable_consumable_items(), dup);
    strs(triforce_chart_names(), triforce);
    strs(treasure_chart_names(), treasure);
    if (opts.sword_mode == SwordMode::Swordless) {
        prog.erase(std::remove(prog.begin(), prog.end(), "Progressive Sword"), prog.end());
        nonprog.erase(std::remove(nonprog.begin(), nonprog.end(), "Hurricane Spin"), nonprog.end());
    }
    std::vector<std::string>& tri = opts.progression_triforce_charts ? prog : nonprog;
    tri.insert(tri.end(), triforce.begin(), triforce.end());
    std::vector<std::string>& tre = opts.progression_treasure_charts ? prog : nonprog;
    tre.insert(tre.end(), treasure.begin(), treasure.end());
    std::vector<std::string> dungeonProg, dungeonNonprog;
    strs(dungeon_progress_items(), dungeonProg);
    strs(dungeon_nonprogress_items(), dungeonNonprog);
    std::vector<std::string>& dp = opts.progression_dungeons ? prog : nonprog;
    dp.insert(dp.end(), dungeonProg.begin(), dungeonProg.end());
    nonprog.insert(nonprog.end(), dungeonNonprog.begin(), dungeonNonprog.end());
    if (opts.trap_chests) {
        for (int i = 0; i < 5; i++) {
            prog.emplace_back("Ice Trap Chest");
        }
    }
    std::vector<std::string> cleaned;
    for (const std::vector<std::string>* list : {&prog, &nonprog, &cons, &dup}) {
        for (const std::string& n : *list) {
            std::string c = cleanItemName(n);
            if (std::find(cleaned.begin(), cleaned.end(), c) == cleaned.end()) {
                cleaned.push_back(std::move(c));
            }
        }
    }

    if (!mReq.init(opts, cleaned)) {
        return false;
    }

    auto ids = [&](const std::vector<std::string>& in, std::vector<int>& out) {
        out.clear();
        out.reserve(in.size());
        for (const std::string& n : in) {
            out.push_back(mReq.intern(n));
        }
    };
    ids(prog, allProgressItems);
    ids(nonprog, allNonprogressItems);
    ids(cons, allFixedConsumableItems);
    ids(dup, duplicatableConsumableItems);
    ids(triforce, triforceChartNames);
    ids(treasure, treasureChartNames);
    ids(cleaned, allCleanedItemNames);
    unplacedProgressItems.clear();
    unplacedNonprogressItems.clear();
    unplacedFixedConsumableItems.clear();
    currentlyOwnedItems.clear();

    // Per-name flags of the fixed lists.
    mIsDungeonItem.assign(mReq.nameCount() + 64, 0);
    mIsConsumable.assign(mReq.nameCount() + 64, 0);
    for (StrSpan span : {dungeon_progress_items(), dungeon_nonprogress_items()}) {
        for (const char* s : span) {
            int id = mReq.intern(s);
            if (id >= (int)mIsDungeonItem.size()) {
                mIsDungeonItem.resize(id + 1, 0);
            }
            mIsDungeonItem[id] = 1;
        }
    }
    for (const std::vector<int>* list : {&allFixedConsumableItems, &duplicatableConsumableItems}) {
        for (int id : *list) {
            if (id >= (int)mIsConsumable.size()) {
                mIsConsumable.resize(id + 1, 0);
            }
            mIsConsumable[id] = 1;
        }
    }

    // Locations.
    Span<Location> locs = locations();
    Span<DungeonName> dn = dungeon_names();
    const int n = (int)locs.size;
    mLocZone.assign(n, std::string());
    mLocDungeon.assign(n, 0);
    mLocAllChests.assign(n, 0);
    remainingItemLocations.clear();
    doneItemLocations.assign(n, -1);
    prerandomizationItemLocations.clear();
    rockSpireShopShipLocations.clear();
    locationsByZoneName.clear();
    for (int i = 0; i < n; i++) {
        std::string name = locs[i].name;
        size_t sep = name.find(" - ");
        mLocZone[i] = sep == std::string::npos ? name : name.substr(0, sep);
        for (size_t d = 0; d < dn.size; d++) {
            if (mLocZone[i] == dn[d].name) {
                mLocDungeon[i] = (uint8_t)(d + 1);
            }
        }
        bool allChests = true;
        for (const LocPath& p : location_paths(locs[i])) {
            const char* raw = p.raw;
            const char* slash = std::strrchr(raw, '/');
            const char* last = slash != nullptr ? slash + 1 : raw;
            if (std::strncmp(last, "Chest", 5) != 0) {
                allChests = false;
            }
        }
        mLocAllChests[i] = allChests ? 1 : 0;
        remainingItemLocations.push_back(i);
        if (startsWith(name, "Rock Spire Isle - Beedle's Special Shop Ship - ")) {
            rockSpireShopShipLocations.push_back(i);
        }
        bool found = false;
        for (auto& z : locationsByZoneName) {
            if (z.first == mLocZone[i]) {
                z.second.push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            locationsByZoneName.push_back({mLocZone[i], {i}});
        }
    }
    mBeedle20 = location_index("The Great Sea - Beedle's Shop Ship - 20 Rupee Item");
    mZunari = location_index("Windfall Island - Zunari - Stock Exotic Flower in Zunari's Shop");
    mAllPurposeBait = mReq.intern("All-Purpose Bait");
    mHyoiPear = mReq.intern("Hyoi Pear");
    mTownFlower = mReq.intern("Town Flower");
    mBoatsSail = mReq.intern("Boat's Sail");
    return !failed();
}

bool Logic::create(const Options& opts, const LogicState& state) {
    return create(opts) && initializeFromRandomizerState(state);
}

bool Logic::initializeFromRandomizerState(const LogicState& state) {
    if (&state != &mState) {
        mState = state;
    }
    if (!mState.error.empty()) {
        fail(mState.error);
        return false;
    }
    mReq.clearNestedEntranceMacros();
    for (const auto& ne : mState.nestedEntrances) {
        mReq.setNestedEntranceMacro("Can Access " + ne.first, "Can Access " + ne.second);
    }
    updateEntranceConnectionMacros();
    updateChartMacros();

    unplacedProgressItems = allProgressItems;
    unplacedNonprogressItems = allNonprogressItems;
    unplacedFixedConsumableItems = allFixedConsumableItems;
    currentlyOwnedItems.clear();
    mReq.clearOwned();

    for (const std::string& it : mState.startingItems) {
        addOwnedItem(it);
    }

    clearReqCaches();
    makeUselessProgressItemsNonprogress();

    if (mState.extraStartItemsEnabled) {
        for (const std::string& it : mState.randomStartingItems) {
            addOwnedItem(it);
        }
    }
    clearReqCaches();
    return !failed();
}

// ---------------------------------------------------------------------------------------------
// Names and lists

int Logic::locationIndex(const std::string& name) const { return location_index(name.c_str()); }

const char* Logic::locationName(int loc) const {
    Span<Location> locs = locations();
    return loc >= 0 && loc < (int)locs.size ? locs[loc].name : "?";
}

uint32_t Logic::locationTypes(int loc) const {
    Span<Location> locs = locations();
    return loc >= 0 && loc < (int)locs.size ? locs[loc].types : 0;
}

bool Logic::listHas(const std::vector<int>& v, int x) const { return std::find(v.begin(), v.end(), x) != v.end(); }

int Logic::listCount(const std::vector<int>& v, int x) { return (int)std::count(v.begin(), v.end(), x); }

bool Logic::listRemove(std::vector<int>& v, int x, const char* what) {
    auto it = std::find(v.begin(), v.end(), x);
    if (it == v.end()) {
        fail(std::string("list.remove(x): x not in list: ") + mReq.name(x) + " (" + what + ")");
        return false;
    }
    v.erase(it);
    return true;
}

bool Logic::isBanned(int loc) const {
    return std::find(mState.bannedLocations.begin(), mState.bannedLocations.end(), loc) != mState.bannedLocations.end();
}

// ---------------------------------------------------------------------------------------------
// Inventory

void Logic::addOwnedItem(int item) {
    int cleaned = cleanItem(item);
    if (!mReq.isItem(cleaned)) {
        fail("Unknown item name: " + mReq.name(item));
        return;
    }
    currentlyOwnedItems.push_back(cleaned);
    mReq.addOwned(cleaned);
    if (listHas(unplacedProgressItems, item)) {
        listRemove(unplacedProgressItems, item, "unplaced_progress_items");
    } else if (listHas(unplacedNonprogressItems, item)) {
        listRemove(unplacedNonprogressItems, item, "unplaced_nonprogress_items");
    } else if (listHas(unplacedFixedConsumableItems, item)) {
        listRemove(unplacedFixedConsumableItems, item, "unplaced_fixed_consumable_items");
    }
    clearReqCaches();
}

void Logic::removeOwnedItem(int item) {
    int cleaned = cleanItem(item);
    if (!mReq.isItem(cleaned)) {
        fail("Unknown item name: " + mReq.name(item));
        return;
    }
    if (!listRemove(currentlyOwnedItems, cleaned, "currently_owned_items")) {
        return;
    }
    mReq.removeOwned(cleaned);
    if (listCount(allProgressItems, item) > listCount(unplacedProgressItems, item)) {
        unplacedProgressItems.push_back(item);
    } else if (listHas(allNonprogressItems, item)) {
        unplacedNonprogressItems.push_back(item);
    } else {
        fail("Cannot remove item from simulated inventory: " + mReq.name(item));
        return;
    }
    clearReqCaches();
}

void Logic::addTemporaryItems(const std::vector<int>& items) {
    for (int it : items) {
        addOwnedItem(it);
    }
}

void Logic::removeTemporaryItems(const std::vector<int>& items) {
    for (int it : items) {
        removeOwnedItem(it);
    }
}

Logic::PlaythroughState Logic::saveSimulatedPlaythroughState() const {
    PlaythroughState s;
    s.owned = currentlyOwnedItems;
    s.unplacedProgress = unplacedProgressItems;
    s.unplacedNonprogress = unplacedNonprogressItems;
    s.unplacedFixedConsumables = unplacedFixedConsumableItems;
    s.req = mReq.snapshot();
    return s;
}

void Logic::loadSimulatedPlaythroughState(const PlaythroughState& s) {
    currentlyOwnedItems = s.owned;
    unplacedProgressItems = s.unplacedProgress;
    unplacedNonprogressItems = s.unplacedNonprogress;
    unplacedFixedConsumableItems = s.unplacedFixedConsumables;
    mReq.restore(s.req);
}

void Logic::setLocationToItem(int loc, int item) {
    if (doneItemLocations[loc] >= 0) {
        fail(std::string("Location was used twice: ") + locationName(loc));
        return;
    }
    doneItemLocations[loc] = item;
    listRemove(remainingItemLocations, loc, "remaining_item_locations");
    addOwnedItem(item);
}

void Logic::setPrerandomizationItemLocation(int loc, int item) {
    for (auto& p : prerandomizationItemLocations) {
        if (p.first == loc) {
            p.second = item;
            return;
        }
    }
    prerandomizationItemLocations.emplace_back(loc, item);
}

int Logic::prerandomizedItem(int loc) const {
    for (const auto& p : prerandomizationItemLocations) {
        if (p.first == loc) {
            return p.second;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------------------------
// Locations

bool Logic::locationIsProgression(int loc, const Options& o, bool filterSunkenTreasure) {
    Span<Location> locs = locations();
    const uint32_t t = locs[loc].types;
    if (t & (LOC_NO_PROGRESSION | LOC_CONSUMABLES_ONLY)) {
        return false;
    }
    struct Rule {
        uint32_t bits;
        bool on;
    };
    const Rule rules[] = {
        {LOC_DUNGEON, o.progression_dungeons},
        {LOC_TINGLE_CHEST, o.progression_tingle_chests},
        {LOC_GREAT_FAIRY, o.progression_great_fairies},
        {LOC_PUZZLE_SECRET_CAVE, o.progression_puzzle_secret_caves},
        {LOC_COMBAT_SECRET_CAVE, o.progression_combat_secret_caves},
        {LOC_SAVAGE_LABYRINTH, o.progression_savage_labyrinth},
        {LOC_SHORT_SIDEQUEST, o.progression_short_sidequests},
        {LOC_LONG_SIDEQUEST, o.progression_long_sidequests},
        {LOC_SPOILS_TRADING, o.progression_spoils_trading},
        {LOC_MINIGAME, o.progression_minigames},
        {LOC_BATTLESQUID, o.progression_battlesquid},
        {LOC_FREE_GIFT, o.progression_free_gifts},
        {LOC_MAIL, o.progression_mail},
        {LOC_PLATFORM | LOC_RAFT, o.progression_platforms_rafts},
        {LOC_SUBMARINE, o.progression_submarines},
        {LOC_EYE_REEF_CHEST, o.progression_eye_reef_chests},
        {LOC_BIG_OCTO | LOC_GUNBOAT, o.progression_big_octos_gunboats},
        {LOC_EXPENSIVE_PURCHASE, o.progression_expensive_purchases},
        {LOC_ISLAND_PUZZLE, o.progression_island_puzzles},
        {LOC_OTHER_CHEST | LOC_MISC, o.progression_misc},
        {LOC_DUNGEON_SECRET, o.progression_dungeon_secrets},
    };
    for (const Rule& r : rules) {
        if ((t & r.bits) && !r.on) {
            return false;
        }
    }
    if ((t & LOC_SUNKEN_TREASURE) && filterSunkenTreasure) {
        return false;
    }
    return true;
}

void Logic::filterLocationsForProgressionStatic(const std::vector<int>& in, std::vector<int>& out, const Options& o,
                                                bool filterSunkenTreasure) {
    std::vector<int> res;
    res.reserve(in.size());
    for (int loc : in) {
        if (locationIsProgression(loc, o, filterSunkenTreasure)) {
            res.push_back(loc);
        }
    }
    out = std::move(res);
}

int Logic::getNumProgressionLocationsStatic(const Options& o) {
    int n = 0;
    const int count = (int)locations().size;
    for (int i = 0; i < count; i++) {
        if (locationIsProgression(i, o, true)) {
            n++;
        }
    }
    if (o.progression_triforce_charts) {
        n += 8;
    }
    if (o.progression_treasure_charts) {
        n += 41;
    }
    return n;
}

int Logic::getMaxRequiredBossesBannedLocations() const {
    if (!mOpts.required_bosses) {
        return 0;
    }
    std::vector<std::pair<std::string, int>> counts; // dict order
    const int n = locationCount();
    for (int i = 0; i < n; i++) {
        if (!locationIsProgression(i, mOpts, false)) {
            continue;
        }
        const std::string name = locationName(i);
        std::string dungeon;
        if (isDungeonLocation(i)) {
            dungeon = mLocZone[i];
        } else if (name == "Mailbox - Letter from Orca") {
            dungeon = "Forbidden Woods";
        } else if (name == "Mailbox - Letter from Baito") {
            dungeon = "Earth Temple";
        } else if (name == "Mailbox - Letter from Aryll" || name == "Mailbox - Letter from Tingle") {
            dungeon = "Forsaken Fortress";
        } else {
            continue;
        }
        bool found = false;
        for (auto& c : counts) {
            if (c.first == dungeon) {
                c.second++;
                found = true;
            }
        }
        if (!found) {
            counts.emplace_back(dungeon, 1);
        }
    }
    std::vector<int> values;
    for (const auto& c : counts) {
        values.push_back(c.second);
    }
    std::sort(values.begin(), values.end(), [](int a, int b) { return a > b; });
    int banned = 6 - mOpts.num_required_bosses;
    int sum = 0;
    for (int i = 0; i < banned && i < (int)values.size(); i++) {
        sum += values[i];
    }
    return sum;
}

void Logic::getProgressAndNonProgressLocations(std::vector<int>& progress, std::vector<int>& nonprogress) {
    std::vector<int> all;
    for (int i = 0; i < locationCount(); i++) {
        all.push_back(i);
    }
    filterLocationsForProgression(all, progress, true);
    std::vector<uint8_t> isProgress(locationCount(), 0);
    for (int loc : progress) {
        isProgress[loc] = 1;
    }
    nonprogress.clear();
    for (int loc : all) {
        if (isProgress[loc]) {
            continue;
        }
        if (locationTypes(loc) & LOC_SUNKEN_TREASURE) {
            int chart = chartNameForLocation(loc);
            bool triforce = chart >= 0 && contains(mReq.name(chart), "Triforce Chart");
            bool prog = triforce ? mOpts.progression_triforce_charts : mOpts.progression_treasure_charts;
            if (prog) {
                progress.push_back(loc);
                isProgress[loc] = 1;
            } else {
                nonprogress.push_back(loc);
            }
        } else {
            nonprogress.push_back(loc);
        }
    }
}

void Logic::getAccessibleRemainingLocations(bool forProgression, std::vector<int>& out) {
    std::vector<int> toCheck;
    if (forProgression) {
        filterLocationsForProgression(remainingItemLocations, toCheck);
    } else {
        toCheck = remainingItemLocations;
    }
    out.clear();
    for (int loc : toCheck) {
        if (checkLocationAccessible(loc)) {
            out.push_back(loc);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Items and locations

bool Logic::isDungeonItem(int item) const { return item >= 0 && item < (int)mIsDungeonItem.size() && mIsDungeonItem[item]; }

bool Logic::isDungeonLocation(int loc, const char* dungeonName) const {
    if (mLocDungeon[loc] == 0) {
        return false;
    }
    if (dungeonName != nullptr && dungeonName[0] != '\0' && mLocZone[loc] != dungeonName) {
        return false;
    }
    if (locationTypes(loc) & LOC_SUNKEN_TREASURE) {
        return false;
    }
    return true;
}

bool Logic::checkItemValidInLocation(int item, int loc) const {
    const uint32_t types = locationTypes(loc);
    const std::string& itemName = mReq.name(item);

    if (isDungeonItem(item) && !mOpts.keylunacy) {
        const std::string shortName = firstWord(itemName);
        const char* dungeonName = nullptr;
        for (const DungeonName& d : dungeon_names()) {
            if (shortName == d.short_name) {
                dungeonName = d.name;
            }
        }
        if (dungeonName == nullptr || !isDungeonLocation(loc, dungeonName)) {
            return false;
        }
        if (types & LOC_BOSS) {
            return false;
        }
        if ((types & LOC_RANDOMIZABLE_MINIBOSS_ROOM) && mOpts.randomize_miniboss_entrances) {
            return false;
        }
    }

    if (loc == mBeedle20 && (item == mAllPurposeBait || item == mHyoiPear)) {
        return false;
    }

    if (std::find(rockSpireShopShipLocations.begin(), rockSpireShopShipLocations.end(), loc) !=
        rockSpireShopShipLocations.end()) {
        for (int other : rockSpireShopShipLocations) {
            if (other == loc) {
                continue;
            }
            if (doneItemLocations[other] == item) {
                return false;
            }
        }
    }

    if (loc == mZunari && (item == mTownFlower || item == mBoatsSail)) {
        return false;
    }

    if (types & LOC_CONSUMABLES_ONLY) {
        if (!(item >= 0 && item < (int)mIsConsumable.size() && mIsConsumable[item])) {
            return false;
        }
    }

    static const char kTrapSuffix[] = " Trap Chest";
    const size_t sl = sizeof(kTrapSuffix) - 1;
    if (itemName.size() >= sl && itemName.compare(itemName.size() - sl, sl, kTrapSuffix) == 0) {
        if (!mLocAllChests[loc]) {
            return false;
        }
    }
    return true;
}

void Logic::filterItemsByAnyValidLocation(const std::vector<int>& items, const std::vector<int>& locs,
                                          std::vector<int>& out) const {
    std::vector<int> res;
    for (int it : items) {
        for (int loc : locs) {
            if (checkItemValidInLocation(it, loc)) {
                res.push_back(it);
                break;
            }
        }
    }
    out = std::move(res);
}

void Logic::filterLocationsValidForItem(const std::vector<int>& locs, int item, std::vector<int>& out) const {
    std::vector<int> res;
    for (int loc : locs) {
        if (checkItemValidInLocation(item, loc)) {
            res.push_back(loc);
        }
    }
    out = std::move(res);
}

void Logic::filterItemsValidForLocation(const std::vector<int>& items, int loc, std::vector<int>& out) const {
    std::vector<int> res;
    for (int it : items) {
        if (checkItemValidInLocation(it, loc)) {
            res.push_back(it);
        }
    }
    out = std::move(res);
}

int Logic::chartNameForLocation(int loc) {
    std::vector<int> toks;
    mReq.topLevelTokens(mReq.locationExpr(loc), toks);
    int chartReq = -1;
    for (int t : toks) {
        if (startsWith(mReq.name(t), "Chart for Island ")) {
            chartReq = t;
            break;
        }
    }
    if (chartReq < 0 || !mReq.hasMacro(chartReq)) {
        fail(std::string("StopIteration: no chart requirement for ") + locationName(loc));
        return -1;
    }
    mReq.topLevelTokens(mReq.macroExpr(chartReq), toks);
    if (toks.empty() || !mReq.isItem(toks[0])) {
        fail("AssertionError: chart macro of " + std::string(locationName(loc)) + " does not start with a chart");
        return -1;
    }
    return toks[0];
}

// ---------------------------------------------------------------------------------------------
// Usefulness

void Logic::getItemNamesByReqName(const std::string& req, std::vector<int>& out) {
    ItemsNeeded needed;
    mReq.itemsNeededByReq(req, needed);
    ReqLogic::flatten(needed, out);
}

void Logic::getItemNamesForLocation(int loc, std::vector<int>& out) {
    ItemsNeeded needed;
    mReq.itemsNeededForLocation(loc, needed);
    ReqLogic::flatten(needed, out);
}

// The progress locations among the remaining ones that are not accessible now (in order).
void Logic::inaccessibleProgressLocations(std::vector<int>& out) {
    std::vector<int> accessible;
    getAccessibleRemainingLocations(true, accessible);
    std::vector<int> toCheck;
    filterLocationsForProgression(remainingItemLocations, toCheck);
    out.clear();
    for (int loc : toCheck) {
        if (std::find(accessible.begin(), accessible.end(), loc) == accessible.end()) {
            out.push_back(loc);
        }
    }
}

int Logic::getFirstUsefulItem(const std::vector<int>& items) {
    std::vector<int> inaccessible;
    inaccessibleProgressLocations(inaccessible);
    mUsefulCache.clear();
    mUsefulCacheOn = true;
    for (int it : items) {
        if (checkItemIsUseful(it, inaccessible)) {
            mUsefulCache.clear();
            mUsefulCacheOn = false;
            return it;
        }
        if (failed()) {
            break;
        }
    }
    mUsefulCache.clear();
    mUsefulCacheOn = false;
    return -1;
}

void Logic::getItemsByUsefulnessFraction(const std::vector<int>& items, bool filterSunkenTreasure,
                                         std::vector<std::pair<int, int>>& out) {
    std::vector<int> accessible;
    getAccessibleRemainingLocations(true, accessible);
    std::vector<int> toCheck;
    filterLocationsForProgression(remainingItemLocations, toCheck, filterSunkenTreasure);
    std::vector<int> inaccessible;
    for (int loc : toCheck) {
        if (std::find(accessible.begin(), accessible.end(), loc) != accessible.end()) {
            continue;
        }
        if (isBanned(loc) || prerandomizedItem(loc) >= 0) {
            continue;
        }
        inaccessible.push_back(loc);
    }
    std::vector<std::vector<int>> namesPerLoc;
    namesPerLoc.reserve(inaccessible.size() + 1);
    for (int loc : inaccessible) {
        namesPerLoc.emplace_back();
        getItemNamesForLocation(loc, namesPerLoc.back());
    }
    namesPerLoc.emplace_back();
    getItemNamesByReqName("Can Reach and Defeat Ganondorf", namesPerLoc.back());

    out.clear();
    for (int it : items) {
        bool have = false;
        for (const auto& p : out) {
            if (p.first == it) {
                have = true;
            }
        }
        if (!have) {
            out.emplace_back(it, 9999);
        }
    }
    std::vector<int> without;
    for (const std::vector<int>& names : namesPerLoc) {
        without = names;
        for (int owned : currentlyOwnedItems) {
            auto f = std::find(without.begin(), without.end(), owned);
            if (f != without.end()) {
                without.erase(f);
            }
        }
        const int frac = (int)without.size();
        for (int it : without) {
            for (auto& p : out) {
                if (p.first == it && frac < p.second) {
                    p.second = frac;
                }
            }
        }
    }
}

void Logic::getAllUselessItems(const std::vector<int>& items, std::vector<int>& out) {
    out.clear();
    if (items.empty()) {
        return;
    }
    std::vector<int> inaccessible;
    inaccessibleProgressLocations(inaccessible);
    mUsefulCache.clear();
    mUsefulCacheOn = true;
    for (int it : items) {
        if (!checkItemIsUseful(it, inaccessible)) {
            out.push_back(it);
        }
        if (failed()) {
            break;
        }
    }
    mUsefulCache.clear();
    mUsefulCacheOn = false;
}

bool Logic::checkItemIsUseful(int item, const std::vector<int>& inaccessible) {
    for (const auto& c : mUsefulCache) {
        if (c.first == item) {
            return c.second != 0;
        }
    }
    auto finish = [&](bool useful) {
        removeOwnedItem(item);
        // cached_items_are_useful[item_name] = ...: a dict store, so a result computed deeper in the
        // recursion for the same item (a prerandomized copy of it) is overwritten, not kept (R1.6 fix:
        // appending made the lookup above return the inner result).
        for (auto& c : mUsefulCache) {
            if (c.first == item) {
                c.second = useful ? 1 : 0;
                return useful;
            }
        }
        mUsefulCache.emplace_back(item, useful ? 1 : 0);
        return useful;
    };
    addOwnedItem(item);
    if (failed()) {
        return false;
    }
    for (int loc : inaccessible) {
        if (isBanned(loc)) {
            continue;
        }
        int prerand = prerandomizedItem(loc);
        if (prerand >= 0) {
            std::vector<int> temp;
            for (int l : inaccessible) {
                if (l != loc) {
                    temp.push_back(l);
                }
            }
            if (!checkItemIsUseful(prerand, temp)) {
                continue;
            }
            if (checkLocationAccessible(loc)) {
                return finish(true);
            }
        }
        if (checkLocationAccessible(loc)) {
            return finish(true);
        }
    }
    return finish(false);
}

// ---------------------------------------------------------------------------------------------
// Macros

void Logic::updateEntranceConnectionMacros() {
    for (const auto& c : mState.entranceConnections) {
        setMacro("Can Access " + c.second, "Can Access " + c.first);
    }
}

void Logic::updateChartMacros() {
    for (int island = 1; island <= 49; island++) {
        const std::string chart = island < (int)mState.islandCharts.size() ? mState.islandCharts[island] : std::string();
        std::string body = contains(chart, "Triforce Chart") ? chart + " & Any Wallet Upgrade" : chart;
        setMacro("Chart for Island " + std::to_string(island), body);
    }
}

void Logic::updateRequiredBossesMacro() {
    std::string body;
    for (const std::string& loc : mState.requiredBossItemLocations) {
        if (!body.empty()) {
            body += " & ";
        }
        body += "Can Access Item Location \"" + loc + "\"";
    }
    setMacro("Can Defeat All Required Bosses", body);
}

void Logic::temporarilyMakeDungeonEntranceMacrosImpossible() {
    for (const std::string& e : mState.dungeonExits) {
        setMacro("Can Access " + e, "Impossible");
    }
}

void Logic::temporarilyMakeDungeonEntranceMacrosAccessible() {
    for (const std::string& e : mState.dungeonExits) {
        setMacro("Can Access " + e, "Nothing");
    }
}

void Logic::temporarilyMakeEntranceMacrosImpossible() {
    for (const auto& c : mState.entranceConnections) {
        setMacro("Can Access " + c.second, "Impossible");
    }
}

void Logic::temporarilyMakeEntranceMacrosWorstCaseScenario() {
    for (const auto& s : mState.entranceSets) {
        temporarilyMakeOneSetOfEntranceMacrosWorstCaseScenario(s.first, s.second);
    }
}

void Logic::temporarilyMakeOneSetOfEntranceMacrosWorstCaseScenario(const std::vector<std::string>& entrances,
                                                                   const std::vector<std::string>& exits) {
    std::string all;
    for (const std::string& e : entrances) {
        const std::string macro = "Can Access " + e;
        const std::string* body = mReq.macroBody(mReq.find(macro));
        if (body != nullptr && *body == "Impossible") {
            fail("AssertionError: " + macro + " is Impossible");
        }
        if (!all.empty()) {
            all += " & ";
        }
        all += macro;
    }
    for (const std::string& x : exits) {
        setMacro("Can Access " + x, all);
    }
}

void Logic::temporarilyMakeRequiredBossesMacroWorstCaseScenario() {
    std::string body;
    for (int i = 0; i < locationCount(); i++) {
        if (locationTypes(i) & LOC_BOSS) {
            if (!body.empty()) {
                body += " & ";
            }
            body += std::string("Can Access Item Location \"") + locationName(i) + "\"";
        }
    }
    setMacro("Can Defeat All Required Bosses", body);
}

void Logic::makeUselessProgressItemsNonprogress() {
    if (mState.entrancesEnabled) {
        temporarilyMakeEntranceMacrosWorstCaseScenario();
    }
    if (mState.bossReqsEnabled) {
        temporarilyMakeRequiredBossesMacroWorstCaseScenario();
    }

    const bool filterSunken = !(mOpts.progression_triforce_charts || mOpts.progression_treasure_charts);
    ItemsNeeded needed;
    ItemsNeeded sub;
    for (int loc = 0; loc < locationCount(); loc++) {
        if (!locationIsProgression(loc, mOpts, filterSunken)) {
            continue;
        }
        mReq.itemsNeededForLocation(loc, sub);
        for (const ItemCount& ic : sub) {
            mergeMax(needed, ic.name, ic.count);
        }
    }
    mReq.itemsNeededByReq("Can Reach and Defeat Ganondorf", sub);
    for (const ItemCount& ic : sub) {
        mergeMax(needed, ic.name, ic.count);
    }
    if (failed()) {
        return;
    }
    std::vector<int> useful;
    ReqLogic::flatten(needed, useful);

    auto isChart = [&](int id) {
        const std::string& n = mReq.name(id);
        return startsWith(n, "Triforce Chart ") || startsWith(n, "Treasure Chart");
    };
    std::vector<int> filtered;
    for (int it : useful) {
        if (mReq.name(it) == "Progressive Sword" && mOpts.sword_mode == SwordMode::Swordless) {
            continue;
        }
        if (isDungeonItem(it) && !mOpts.progression_dungeons) {
            continue;
        }
        if (!listHas(allProgressItems, it) && !isChart(it)) {
            fail("Item " + mReq.name(it) + " opens up progress locations but is not in the list of all progress items.");
            return;
        }
        filtered.push_back(it);
    }

    std::vector<int> toNonprogress = allProgressItems;
    std::vector<int> startingToRemove;
    for (const std::string& s : mState.startingItems) {
        startingToRemove.push_back(mReq.intern(s));
    }
    for (int it : filtered) {
        if (!listHas(toNonprogress, it) && isChart(it)) {
            continue;
        }
        if (!listRemove(toNonprogress, it, "all_items_to_make_nonprogress")) {
            return;
        }
        auto f = std::find(startingToRemove.begin(), startingToRemove.end(), it);
        if (f != startingToRemove.end()) {
            startingToRemove.erase(f);
        }
    }
    std::vector<int> unplacedToNonprogress = toNonprogress;
    for (int it : startingToRemove) {
        if (!listHas(allProgressItems, it)) {
            continue;
        }
        if (!listRemove(unplacedToNonprogress, it, "unplaced_items_to_make_nonprogress")) {
            return;
        }
    }

    static const char kTrapSuffix[] = " Trap Chest";
    const size_t sl = sizeof(kTrapSuffix) - 1;
    for (int it : toNonprogress) {
        const std::string& n = mReq.name(it);
        if (n.size() >= sl && n.compare(n.size() - sl, sl, kTrapSuffix) == 0) {
            continue; // traps stay progress items although they are useless
        }
        if (!listRemove(allProgressItems, it, "all_progress_items")) {
            return;
        }
        allNonprogressItems.push_back(it);
    }
    for (int it : unplacedToNonprogress) {
        if (!listRemove(unplacedProgressItems, it, "unplaced_progress_items")) {
            return;
        }
        unplacedNonprogressItems.push_back(it);
    }

    if (mState.entrancesEnabled) {
        updateEntranceConnectionMacros();
    }
    if (mState.bossReqsEnabled) {
        updateRequiredBossesMacro();
    }
}

// ---------------------------------------------------------------------------------------------
// WWRandomizer.__init__

bool initRandomizerLogic(Logic& logic, const Options& opts, const LogicState& state, RandomizerLogicInit& out) {
    out = RandomizerLogicInit();
    if (!logic.create(opts) || !logic.initializeFromRandomizerState(state)) {
        out.error = logic.error();
        return false;
    }
    out.numProgressLocations = logic.getNumProgressionLocations();
    out.maxRequiredBossesBannedLocations = logic.getMaxRequiredBossesBannedLocations();
    out.allRandomizedProgressItems = logic.unplacedProgressItems;
    const int numItems = (int)out.allRandomizedProgressItems.size();
    if (out.numProgressLocations - out.maxRequiredBossesBannedLocations < numItems) {
        std::string msg = "Not enough progress locations to place all progress items.\n\n";
        msg += "Total progress items: " + std::to_string(numItems) + "\n";
        msg += "Progress locations with current options: " + std::to_string(out.numProgressLocations) + "\n";
        if (out.maxRequiredBossesBannedLocations > 0) {
            msg += "Maximum Required Bosses Mode banned locations: " +
                   std::to_string(out.maxRequiredBossesBannedLocations) + "\n";
        }
        msg += "\nYou need to check more of the progress location options in order to give the randomizer enough "
               "space to place all the items.";
        out.tooFewProgressionLocations = true;
        out.error = msg;
        return false;
    }

    std::vector<int> accessible;
    logic.temporarilyMakeDungeonEntranceMacrosImpossible();
    logic.getAccessibleRemainingLocations(true, accessible);
    out.dungeonsOnlyStart = accessible.empty();
    logic.updateEntranceConnectionMacros();

    logic.temporarilyMakeEntranceMacrosImpossible();
    logic.getAccessibleRemainingLocations(true, accessible);
    out.dungeonsAndCavesOnlyStart = accessible.empty();
    logic.updateEntranceConnectionMacros();

    if (logic.failed()) {
        out.error = logic.error();
        return false;
    }
    return true;
}

} // namespace pcr
