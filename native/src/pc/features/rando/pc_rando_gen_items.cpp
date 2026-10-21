// pc_rando_gen_items.cpp - the item fill (docs/RANDOMIZER_PLAN.md step R1.4); see pc_rando_gen_items.h.
// Each function is the wwrando function of the same name (randomizers/items.py); comments quote the
// Python where the order of operations matters for exactness.
#include "pc_rando_gen_items.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "pc_rando_data.h"

namespace pcr {

namespace {

using pc::rando::PyRandom;

bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

bool endsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool startsWith(const std::string& s, const char* prefix) { return s.compare(0, strlen(prefix), prefix) == 0; }

class ItemFill {
public:
    ItemFill(Logic& logic, PyRandom& rng, bool dacOnlyStart, ItemFillResult& out)
        : L(logic), rng(rng), dungeonsAndCavesOnlyStart(dacOnlyStart), res(out) {}

    bool run() {
        // _randomize
        if (!L.options().keylunacy) {
            if (!randomizeDungeonItems()) {
                return false;
            }
        }
        if (!randomizeProgressionItemsForwardFill()) {
            return false;
        }
        if (!randomizeUniqueNonprogressItems()) {
            return false;
        }
        // (The "Inaccessible locations" print draws nothing and changes no state.)
        return randomizeConsumableItems();
    }

private:
    bool fail(const std::string& msg) {
        if (res.error.empty()) {
            res.error = msg;
        }
        return false;
    }
    // A Logic error (where Python raises inside Logic) ends the fill.
    bool logicOk() {
        if (L.failed()) {
            return fail(L.error());
        }
        return true;
    }

    // The unplaced progress + nonprogress items whose name passes `pred`, in that order.
    template <class Pred> std::vector<int> unplacedWhere(Pred pred) {
        std::vector<int> out;
        for (const std::vector<int>* list : {&L.unplacedProgressItems, &L.unplacedNonprogressItems}) {
            for (int it : *list) {
                if (pred(it)) {
                    out.push_back(it);
                }
            }
        }
        return out;
    }

    // --- (A) randomize_dungeon_items ---
    bool randomizeDungeonItems() {
        // "Temporarily add all items except for dungeon keys while we randomize them."
        const std::vector<int> itemsToTemporarilyAdd = unplacedWhere([&](int it) { return !L.isDungeonItem(it); });
        for (int it : itemsToTemporarilyAdd) {
            L.addOwnedItem(it);
        }
        L.temporarilyMakeDungeonEntranceMacrosAccessible();
        if (!logicOk()) {
            return false;
        }

        if (dungeonsAndCavesOnlyStart) {
            static const char* const kDrcFailsafe[] = {
                "Dragon Roost Cavern - First Room",
                "Dragon Roost Cavern - Alcove With Water Jugs",
                "Dragon Roost Cavern - Boarded Up Chest",
                "Dragon Roost Cavern - Rat Room",
                "Dragon Roost Cavern - Rat Room Boarded Up Chest",
                "Dragon Roost Cavern - Bird's Nest",
            };
            const size_t i = rng.choiceIndex(sizeof kDrcFailsafe / sizeof kDrcFailsafe[0]);
            res.drcFailsafeLocation = L.locationIndex(kDrcFailsafe[i]);
            if (res.drcFailsafeLocation < 0) {
                return fail(std::string("Unknown location: ") + kDrcFailsafe[i]);
            }
        }

        // Small keys.
        const std::vector<int> smallKeys = unplacedWhere([&](int it) { return endsWith(L.name(it), " Small Key"); });
        if (smallKeys.empty()) {
            return fail("No small keys to place (assert len(small_keys_to_place) > 0)");
        }
        for (int it : smallKeys) {
            if (!placeDungeonItem(it)) {
                return false;
            }
            L.addOwnedItem(it);
        }
        // Big keys.
        const std::vector<int> bigKeys = unplacedWhere([&](int it) { return endsWith(L.name(it), " Big Key"); });
        if (bigKeys.empty()) {
            return fail("No big keys to place (assert len(big_keys_to_place) > 0)");
        }
        for (int it : bigKeys) {
            if (!placeDungeonItem(it)) {
                return false;
            }
            L.addOwnedItem(it);
        }
        // Dungeon maps and compasses.
        const std::vector<int> others = unplacedWhere([&](int it) {
            const std::string& n = L.name(it);
            return endsWith(n, " Dungeon Map") || endsWith(n, " Compass");
        });
        for (int it : others) {
            if (!placeDungeonItem(it)) {
                return false;
            }
        }

        // "Remove the items we temporarily added." (remove_owned_item re-appends them to the unplaced
        // lists, which reorders those lists.)
        for (int it : itemsToTemporarilyAdd) {
            L.removeOwnedItem(it);
        }
        for (int it : smallKeys) {
            L.removeOwnedItem(it);
        }
        for (int it : bigKeys) {
            L.removeOwnedItem(it);
        }
        L.updateEntranceConnectionMacros();
        return logicOk();
    }

    bool placeDungeonItem(int item) {
        // Dungeons as progress: dungeon items only in progress locations.
        const bool forProgression = L.options().progression_dungeons;
        std::vector<int> accessible;
        L.getAccessibleRemainingLocations(forProgression, accessible);
        std::vector<int> notPrerand;
        for (int loc : accessible) {
            if (L.prerandomizedItem(loc) < 0) {
                notPrerand.push_back(loc);
            }
        }
        std::vector<int> possible;
        L.filterLocationsValidForItem(notPrerand, item, possible);
        if (!logicOk()) {
            return false;
        }

        const std::string& name = L.name(item);
        if (dungeonsAndCavesOnlyStart && name == "DRC Small Key") {
            const int a = L.locationIndex("Dragon Roost Cavern - Big Key Chest");
            const int b = L.locationIndex("Dragon Roost Cavern - Tingle Statue Chest");
            possible.erase(std::remove_if(possible.begin(), possible.end(), [&](int loc) { return loc == a || loc == b; }),
                           possible.end());
        }
        if (dungeonsAndCavesOnlyStart &&
            (name == "DRC Big Key" || name == "DRC Dungeon Map" || name == "DRC Compass")) {
            const int f = res.drcFailsafeLocation;
            possible.erase(std::remove(possible.begin(), possible.end(), f), possible.end());
        }
        if (possible.empty()) {
            return fail("No valid locations left to place dungeon items!");
        }
        const int loc = rng.choice(possible);
        L.setPrerandomizationItemLocation(loc, item);
        return true;
    }

    // --- (B) randomize_progression_items_forward_fill ---
    bool isBossBanned(int loc) const {
        const std::vector<int>& b = L.state().bannedLocations;
        return std::find(b.begin(), b.end(), loc) != b.end();
    }

    bool randomizeProgressionItemsForwardFill() {
        std::vector<int> accessible;
        L.getAccessibleRemainingLocations(true, accessible);
        if (!logicOk()) {
            return false;
        }
        if (!L.unplacedProgressItems.empty() && accessible.empty()) {
            return fail("No progress locations are accessible at the very start of the game!");
        }

        // location_weights: location -> weight; 0 = not in the dict yet.
        std::vector<int> weights(L.locationCount(), 0);
        int currentWeight = 1;
        const bool requiredBosses = L.options().required_bosses;
        std::vector<int> possibleItems, filtered, possibleLocations, noSunken, shuffled;
        std::vector<std::pair<int, int>> fractions;

        while (!L.unplacedProgressItems.empty()) {
            L.getAccessibleRemainingLocations(true, accessible);
            if (!logicOk()) {
                return false;
            }
            if (requiredBosses) {
                // Banned locations stay out, except prerandomized ones (e.g. small keys).
                accessible.erase(std::remove_if(accessible.begin(), accessible.end(),
                                                [&](int loc) { return isBossBanned(loc) && L.prerandomizedItem(loc) < 0; }),
                                 accessible.end());
            }
            if (accessible.empty()) {
                return fail("No locations left to place progress items!");
            }

            // Newly reached predetermined locations get their items, then the loop starts over.
            bool placedPredetermined = false;
            for (int loc : accessible) {
                const int it = L.prerandomizedItem(loc);
                if (it >= 0) {
                    L.setLocationToItem(loc, it);
                    placedPredetermined = true;
                }
            }
            if (placedPredetermined) {
                if (!logicOk()) {
                    return false;
                }
                continue;
            }

            for (int loc : accessible) {
                if (weights[loc] == 0) {
                    weights[loc] = currentWeight;
                } else if (weights[loc] > 1) {
                    weights[loc] -= 1;
                }
            }
            currentWeight += 1;

            possibleItems = L.unplacedProgressItems;
            // "Don't randomly place items that already had their location predetermined."
            for (const auto& p : L.prerandomizationItemLocations) {
                if (!contains(L.remainingItemLocations, p.first)) {
                    continue;
                }
                if (!contains(L.allProgressItems, p.second)) {
                    continue;
                }
                auto pos = std::find(possibleItems.begin(), possibleItems.end(), p.second);
                if (pos == possibleItems.end()) {
                    return fail("list.remove(x): x not in list (prerandomized item " + L.name(p.second) + ")");
                }
                possibleItems.erase(pos);
            }
            if (possibleItems.empty()) {
                return fail("Only items left to place are predetermined items at inaccessible locations!");
            }

            L.filterItemsByAnyValidLocation(possibleItems, accessible, filtered);
            if (filtered.empty()) {
                return fail("Not enough valid locations left for any of the unplaced progress items!");
            }
            // De-duplicate, except dungeon items (so small keys can be front-loaded in key-lunacy).
            possibleItems.clear();
            for (int it : filtered) {
                if (L.isDungeonItem(it) || !contains(possibleItems, it)) {
                    possibleItems.push_back(it);
                }
            }

            bool mustPlaceUseful = false;
            bool shouldPlaceUseful = true;
            if (accessible.size() == 1 && possibleItems.size() > 1) {
                mustPlaceUseful = true;
            } else if (accessible.size() >= 17) {
                shouldPlaceUseful = false;
            }

            int item = -1;
            if (mustPlaceUseful || shouldPlaceUseful) {
                shuffled = possibleItems;
                rng.shuffle(shuffled);
                // Each probe adds and removes the item, which moves it to the end of
                // unplaced_progress_items (Logic::removeOwnedItem).
                item = L.getFirstUsefulItem(shuffled);
                if (!logicOk()) {
                    return false;
                }
                if (item < 0) {
                    if (mustPlaceUseful) {
                        return fail("No useful progress items to place!");
                    }
                    // The items at the best usefulness fraction (the smallest denominator).
                    L.getItemsByUsefulnessFraction(possibleItems, false, fractions);
                    if (!logicOk()) {
                        return false;
                    }
                    int best = fractions.empty() ? 0 : fractions[0].second;
                    for (const auto& f : fractions) {
                        best = std::min(best, f.second);
                    }
                    std::vector<int> atBest;
                    for (const auto& f : fractions) {
                        if (f.second == best) {
                            atBest.push_back(f.first);
                        }
                    }
                    if (atBest.empty()) {
                        return fail("min() arg is an empty sequence");
                    }
                    item = rng.choice(atBest);
                }
            } else {
                item = rng.choice(possibleItems);
            }

            L.filterLocationsValidForItem(accessible, item, possibleLocations);
            // Try to prevent chains of charts that lead to sunken treasures with more charts.
            const std::string& name = L.name(item);
            if (startsWith(name, "Treasure Chart") || startsWith(name, "Triforce Chart")) {
                noSunken.clear();
                for (int loc : possibleLocations) {
                    if (!(L.locationTypes(loc) & LOC_SUNKEN_TREASURE)) {
                        noSunken.push_back(loc);
                    }
                }
                if (!noSunken.empty()) {
                    possibleLocations = noSunken;
                }
            }

            // rng.choice over the list with each location repeated `weight` times: randbelow(sum),
            // then the location whose run holds that index.
            uint64_t total = 0;
            for (int loc : possibleLocations) {
                total += (uint64_t)weights[loc];
            }
            if (total == 0) {
                return fail("Cannot choose from an empty sequence");
            }
            uint64_t pick = rng.randbelow(total);
            int location = -1;
            for (int loc : possibleLocations) {
                const uint64_t w = (uint64_t)weights[loc];
                if (pick < w) {
                    location = loc;
                    break;
                }
                pick -= w;
            }
            L.setLocationToItem(location, item);
            if (!logicOk()) {
                return false;
            }
        }

        // Predetermined locations the loop never reached still get their items.
        const std::vector<std::pair<int, int>> prerand = L.prerandomizationItemLocations;
        for (const auto& p : prerand) {
            if (contains(L.remainingItemLocations, p.first)) {
                L.setLocationToItem(p.first, p.second);
            }
        }
        if (!logicOk()) {
            return false;
        }
        const bool beatable = L.req().checkRequirementMet("Can Reach and Defeat Ganondorf");
        if (!logicOk()) {
            return false;
        }
        if (!beatable) {
            return fail("Game is not beatable on this seed! This error shouldn't happen.");
        }
        return true;
    }

    // --- (C) randomize_unique_nonprogress_items ---
    bool randomizeUniqueNonprogressItems() {
        std::vector<int> accessible, possible;
        while (!L.unplacedNonprogressItems.empty()) {
            L.getAccessibleRemainingLocations(false, accessible);
            const int item = rng.choice(L.unplacedNonprogressItems);
            L.filterLocationsValidForItem(accessible, item, possible);
            if (!logicOk()) {
                return false;
            }
            if (possible.empty()) {
                return fail("No valid locations left to place non-progress items!");
            }
            const int loc = rng.choice(possible);
            L.setLocationToItem(loc, item);
            if (!logicOk()) {
                return false;
            }
        }
        return true;
    }

    // --- (D) randomize_consumable_items ---
    bool randomizeConsumableItems() {
        const std::vector<int> locs = L.remainingItemLocations;
        std::vector<int> possible;
        for (int loc : locs) {
            L.filterItemsValidForLocation(L.unplacedFixedConsumableItems, loc, possible);
            if (possible.empty()) {
                // Duplicatable consumables are never used up.
                L.filterItemsValidForLocation(L.duplicatableConsumableItems, loc, possible);
                if (possible.empty()) {
                    return fail(std::string("No valid consumable items for location ") + L.locationName(loc));
                }
            }
            const int item = rng.choice(possible);
            L.setLocationToItem(loc, item);
            if (!logicOk()) {
                return false;
            }
        }
        return true;
    }

    Logic& L;
    PyRandom& rng;
    const bool dungeonsAndCavesOnlyStart;
    ItemFillResult& res;
};

// --- COS_RANDO_TRACE ---
FILE* gTrace = nullptr;
bool gTraceOwned = false;

void traceDraw(void* user, PyRandom::TraceKind kind, uint64_t n, uint64_t result) {
    if (gTrace == nullptr) {
        return;
    }
    const char* owner = (const char*)user;
    if (kind == PyRandom::kTraceRandbelow) {
        fprintf(gTrace, "%s randbelow %llu %llu\n", owner, (unsigned long long)n, (unsigned long long)result);
    } else {
        fprintf(gTrace, "%s random %llu\n", owner, (unsigned long long)result);
    }
}

} // namespace

bool randomizeItems(Logic& logic, PyRandom& rng, bool dungeonsAndCavesOnlyStart, ItemFillResult& out) {
    out = ItemFillResult();
    if (logic.failed()) {
        out.error = logic.error();
        return false;
    }
    ItemFill fill(logic, rng, dungeonsAndCavesOnlyStart, out);
    return fill.run();
}

void rngChangingOptionValues(const Options& o, int v[pc::rando::kRngChangingOptionCount]) {
    v[0] = o.fishmen_hints ? 1 : 0;
    v[1] = o.hoho_hints ? 1 : 0;
    v[2] = o.korl_hints ? 1 : 0;
    v[3] = o.num_path_hints;
    v[4] = o.num_barren_hints;
    v[5] = o.num_location_hints;
    v[6] = o.num_item_hints;
    v[7] = o.cryptic_hints ? 1 : 0;
    v[8] = o.prioritize_remote_hints ? 1 : 0;
    v[9] = o.hint_importance ? 1 : 0;
    v[10] = o.do_not_generate_spoiler_log ? 1 : 0;
}

void newRandomizerRng(PyRandom& rng, const std::string& sanitizedSeed, const Options& o, const char* seedKey) {
    uint8_t digest[16];
    integerSeedMd5(sanitizedSeed, o, digest, seedKey);
    int values[pc::rando::kRngChangingOptionCount];
    rngChangingOptionValues(o, values);
    pc::rando::getNewRng(rng, digest, values);
}

bool runItemRandomizer(Logic& logic, const RandomizerLogicInit& init, const std::string& sanitizedSeed,
                       ItemFillResult& out, const char* seedKey) {
    PyRandom rng;
    newRandomizerRng(rng, sanitizedSeed, logic.options(), seedKey);
    traceAttach(rng, "ItemRandomizer");
    return randomizeItems(logic, rng, init.dungeonsAndCavesOnlyStart, out);
}

void traceBegin(const std::string& sanitizedSeed) {
    traceEnd();
    const char* env = getenv("COS_RANDO_TRACE");
    if (env == nullptr || env[0] == '\0' || strcmp(env, "0") == 0) {
        return;
    }
    if (strcmp(env, "1") == 0) {
        gTrace = stderr;
        gTraceOwned = false;
    } else {
        gTrace = fopen(env, "a");
        gTraceOwned = gTrace != nullptr;
        if (gTrace == nullptr) {
            fprintf(stderr, "COS_RANDO_TRACE: cannot open %s\n", env);
            return;
        }
    }
    fprintf(gTrace, "# ww_trace 1 seed=%s version=%s\n", sanitizedSeed.c_str(), kWwrandoVersion);
}

void traceBeginFile(FILE* f, const std::string& sanitizedSeed) {
    traceEnd();
    gTrace = f;
    gTraceOwned = false;
    if (gTrace != nullptr) {
        fprintf(gTrace, "# ww_trace 1 seed=%s version=%s\n", sanitizedSeed.c_str(), kWwrandoVersion);
    }
}

void traceAttach(PyRandom& rng, const char* owner) {
    if (gTrace != nullptr) {
        rng.setTrace(traceDraw, (void*)owner);
    } else {
        rng.setTrace(nullptr, nullptr);
    }
}

void traceEnd() {
    if (gTrace != nullptr) {
        fflush(gTrace);
        if (gTraceOwned) {
            fclose(gTrace);
        }
    }
    gTrace = nullptr;
    gTraceOwned = false;
}

} // namespace pcr
