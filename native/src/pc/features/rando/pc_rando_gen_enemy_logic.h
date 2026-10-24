// pc_rando_gen_enemy_logic.h - the enemy randomizer's logic filter (docs/RANDOMIZER_PLAN.md step R7.3b):
// a port of wwrando 1.10.0_9775811's logic/logic.py
//   filter_out_enemies_that_add_new_requirements (with cached_enemies_tested_for_reqs_tuple),
//   get_all_item_combo_subsets_meeting_req(_recursive) (with its 10,000-combo guard),
// and the two enemy lists of EnemyRandomizer.__init__ (enemies_to_randomize_to and
// enemies_to_randomize_to_when_all_enemies_must_be_killed).
//
// Pure C++ (no game headers): cos_rando_core and the unit tests link it.
//
// Exactness (decision D1). The filter runs on the main Logic before the item fill, and every probe
// goes through add_temporary_items (Logic::addOwnedItem / removeOwnedItem), which moves each probed
// item to the end of the unplaced lists. The item fill reads those lists in order, so item parity
// with enemy rando on depends on doing exactly wwrando's probes in wwrando's order:
//   - items_needed of the original requirements (dict insertion order), minus one per owned copy of
//     each item (currently_owned_items order), minus Progressive Sword and Hurricane Spin when
//     Swordless; the biggest combo is each name repeated its count times;
//   - the recursive subset walk: one probe per combo not yet checked, depth first, removing each
//     item of the combo in turn (list.remove: the first occurrence);
//   - the matched combos in sorted() order (Python tuple-of-str order), one probe per combo, each
//     checking every enemy not yet removed.
// The cache is keyed by (original requirement string, throwable, bomb flowers) and holds per enemy
// type whether it was allowed; a call whose enemies are all cached returns before any probe.
// wwrando keeps it on the Logic and clears it in initialize_from_randomizer_state; here it lives in
// the filter object, so the enemy generator (R7.3c) makes one per Logic after that initialization.
//
// Errors: where wwrando raises, the call returns false with the same text ("No item combos satisfied
// the requirement!", the exponential-loop guard with Python's repr of the parsed expression) or the
// Logic's error. The cache entry created before the raise stays, as in Python.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace pcr {

class Logic;

// EnemyRandomizer.enemies_to_randomize_to: the enemy_types() indices with "Allow randomizing to",
// in file order.
std::vector<int> enemyTypesToRandomizeTo();
// enemies_to_randomize_to_when_all_enemies_must_be_killed: the same minus every type whose pretty
// name contains "Morth" and the Dexivine.
std::vector<int> enemyTypesToRandomizeToWhenAllMustBeKilled();

// Logic.parse_logic_expression's result as Python's repr() prints it (the guard's error text).
std::string pyReprLogicExpression(const std::string& reqString);

class EnemyLogicFilter {
public:
    explicit EnemyLogicFilter(Logic& logic) : mLogic(logic) {}

    // filter_out_enemies_that_add_new_requirements: `possible` (enemy_types() indices, in order)
    // minus the types that would need an item the original enemies of the group do not; the
    // result keeps the input order. False (and *err) where wwrando raises.
    bool filter(const std::string& originalReq, bool hasThrowableObjects, bool hasBombFlowers,
                const std::vector<int>& possible, std::vector<int>& allowed, std::string* err);

    // get_all_item_combo_subsets_meeting_req: every subset of `combo` (name ids) meeting the
    // expression, found by the depth-first walk (in the walk's order; filter sorts them), and the
    // number of combos checked. False (and *err) when the guard trips.
    bool combosMeetingReq(const std::vector<int>& combo, int expr, const std::string& reqString,
                          std::vector<std::vector<int>>& matched, size_t* checked, std::string* err);

    // Diagnostics of the last filter call that did not return from the cache (else both -1).
    int lastMatchedCombos() const { return mLastMatched; }
    int lastCheckedCombos() const { return mLastChecked; }

    // cached_enemies_tested_for_reqs_tuple.clear().
    void clearCache() { mCache.clear(); }

private:
    struct CacheEntry {
        std::string req;
        bool throwable;
        bool bombFlowers;
        std::vector<int8_t> result; // per enemy_types() index: -1 not tested, 0 banned, 1 allowed
    };
    CacheEntry& cacheEntry(const std::string& req, bool throwable, bool bombFlowers, bool* existed);
    int typeExpr(int type);

    Logic& mLogic;
    std::vector<CacheEntry> mCache;
    std::vector<int> mTypeExpr; // compiled "Requirements to defeat" per type, -2 = not yet
    int mLastMatched = -1;
    int mLastChecked = -1;
};

} // namespace pcr
