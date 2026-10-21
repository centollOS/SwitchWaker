// pc_rando_gen_extra.cpp - ExtraStartingItemsRandomizer (docs/RANDOMIZER_PLAN.md step R6.4); see
// pc_rando_gen_extra.h. Follows randomizers/extra_starting_items.py line by line.
#include "pc_rando_gen_extra.h"

#include <string.h>

#include <algorithm>
#include <set>
#include <vector>

#include "pc_rando_data.h"
#include "pc_rando_gen_items.h"
#include "pc_rando_rng.h"

namespace pcr {

namespace {

using pc::rando::PyRandom;

bool startsWith(const std::string& s, const char* prefix) {
    return s.compare(0, strlen(prefix), prefix) == 0;
}

// DISALLOWED_RANDOM_STARTING_ITEMS: the Triforce Shards (num_starting_triforce_shards owns them).
bool isDisallowed(const std::string& name) {
    return startsWith(name, "Triforce Shard ") && name.size() == strlen("Triforce Shard ") + 1 &&
           name.back() >= '1' && name.back() <= '8';
}

// DELIVERY_BAG_ITEMS.
bool isDeliveryBagItem(const std::string& name) {
    return name == "Note to Mom" || name == "Maggie's Letter" || name == "Moblin's Letter" || name == "Cabana Deed";
}

bool isDungeonProgressItem(const std::string& name) {
    for (const char* n : dungeon_progress_items()) {
        if (name == n) {
            return true;
        }
    }
    return false;
}

bool inList(const std::vector<int>& v, int x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

size_t accessibleProgressCount(Logic& logic) {
    std::vector<int> locs;
    logic.getAccessibleRemainingLocations(true, locs);
    return locs.size();
}

// filter_possible_random_starting_items: sorted names (Python sorts the set of str by code point,
// which for these ASCII names is std::string's byte order).
std::vector<std::string> filterPossible(Logic& logic, int maxFraction) {
    const std::vector<int> toCheck = logic.unplacedProgressItems;
    bool needSunkenTreasure = false;
    for (int it : toCheck) {
        const std::string& n = logic.name(it);
        if (startsWith(n, "Treasure Chart ") || startsWith(n, "Triforce Chart ")) {
            needSunkenTreasure = true;
        }
    }
    std::vector<std::pair<int, int>> byFraction;
    logic.getItemsByUsefulnessFraction(toCheck, !needSunkenTreasure, byFraction);

    std::set<std::string> available;
    for (const auto& p : byFraction) {
        if (p.second <= maxFraction) {
            available.insert(logic.name(p.first));
        }
    }
    const int deliveryBag = logic.item("Delivery Bag");
    const bool haveDeliveryBag = inList(logic.currentlyOwnedItems, deliveryBag);
    const bool keylunacy = logic.options().keylunacy;
    for (auto it = available.begin(); it != available.end();) {
        const bool drop = isDisallowed(*it) || (!haveDeliveryBag && isDeliveryBagItem(*it)) ||
                          (!keylunacy && isDungeonProgressItem(*it));
        it = drop ? available.erase(it) : std::next(it);
    }
    // Group the charts so that they weigh as one item each.
    auto group = [&](const std::vector<int>& names, const char* groupName) {
        bool any = false;
        for (int c : names) {
            any |= available.erase(logic.name(c)) != 0;
        }
        if (any) {
            available.insert(groupName);
        }
    };
    group(logic.treasureChartNames, "Treasure Chart");
    group(logic.triforceChartNames, "Triforce Chart");
    // A random Progressive Sword would desync No Starting Sword's sword count.
    if (logic.options().sword_mode == SwordMode::NoStartingSword) {
        available.erase("Progressive Sword");
    }
    return std::vector<std::string>(available.begin(), available.end());
}

} // namespace

bool runExtraStartingItemsRandomizer(Logic& logic, const std::string& sanitizedSeed, const char* seedKey,
                                     std::string* err) {
    const Options& o = logic.options();
    // is_enabled: items.is_enabled() (always, outside --noitemrando) and the option above 0.
    if (o.num_extra_starting_items <= 0) {
        return true;
    }
    auto fail = [&](const std::string& msg) {
        if (err != nullptr) {
            *err = msg;
        }
        return false;
    };
    PyRandom rng;
    newRandomizerRng(rng, sanitizedSeed, o, seedKey);
    traceAttach(rng, "ExtraStartingItemsRandomizer");

    LogicState& st = logic.state();
    const size_t initialSphere0 = accessibleProgressCount(logic);
    for (int remaining = o.num_extra_starting_items; remaining > 0; remaining--) {
        int maxFraction = remaining;
        if (accessibleProgressCount(logic) > initialSphere0) {
            // A check is open already: any useful item will do for the remaining slots.
            maxFraction = 9998;
        }
        const std::vector<std::string> available = filterPossible(logic, maxFraction);
        if (logic.failed()) {
            return fail(logic.error());
        }
        if (available.empty()) {
            break;
        }
        std::string selected = rng.choice(available);
        if (selected == "Treasure Chart" || selected == "Triforce Chart") {
            std::vector<int> charts = logic.treasureChartNames;
            charts.insert(charts.end(), logic.triforceChartNames.begin(), logic.triforceChartNames.end());
            std::vector<std::pair<int, int>> chartUsefulness;
            logic.getItemsByUsefulnessFraction(charts, false, chartUsefulness);
            std::vector<std::string> choices;
            for (int it : logic.unplacedProgressItems) {
                const std::string& n = logic.name(it);
                if (!startsWith(n, selected.c_str())) {
                    continue;
                }
                for (const auto& p : chartUsefulness) {
                    if (p.first == it && p.second <= maxFraction) {
                        choices.push_back(n);
                        break;
                    }
                }
            }
            if (choices.empty()) {
                return fail("IndexError: Cannot choose from an empty sequence");
            }
            selected = rng.choice(choices);
        }
        logic.addOwnedItem(selected);
        st.randomStartingItems.push_back(selected);
        if (logic.failed()) {
            return fail(logic.error());
        }
    }
    if (!st.randomStartingItems.empty() && accessibleProgressCount(logic) <= initialSphere0) {
        return fail("Random starting items didn't unlock at least one check");
    }
    return true;
}

} // namespace pcr
