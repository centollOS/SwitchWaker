// pc_rando_gen_extra.h - the extra starting items randomizer (docs/RANDOMIZER_PLAN.md step R6.4): a
// port of wwrando 1.10.0_9775811's randomizers/extra_starting_items.py ExtraStartingItemsRandomizer.
//
// It runs on the main Logic after the pigs and before the items (randomizer.py's order). For each of
// the num_extra_starting_items slots (counting down), it lists the unplaced progress items whose
// usefulness fraction is at most the slots left (any useful item once a new check is open), minus
// the Triforce Shards, the delivery-bag letters without the Delivery Bag, the dungeon keys without
// key-lunacy, and Progressive Sword in No Starting Sword mode; treasure and triforce charts are
// grouped as one entry each. The list is sorted (Python's sorted on str), one is chosen, a grouped
// chart becomes a chart of that kind, and it is added to the inventory and to
// logic.state().randomStartingItems. The spoiler line ("Random starting item: ...") is R1.5's, from
// that list; the new game gets them through update_starting_gear (pc_rando_slot's config).
//
// Pure C++ (no game headers): cos_rando_core links it.
#pragma once

#include <string>

#include "pc_rando_logic.h"

namespace pcr {

// randomizers/*.py progress_randomize_duration_weight of ExtraStartingItemsRandomizer.
constexpr int kWeightExtraStartItems = 20;

// ExtraStartingItemsRandomizer._randomize with a fresh RNG from the seed (owner
// "ExtraStartingItemsRandomizer" in COS_RANDO_TRACE). Does nothing when the option is 0. False (and
// *err) where wwrando raises.
bool runExtraStartingItemsRandomizer(Logic& logic, const std::string& sanitizedSeed, const char* seedKey,
                                     std::string* err);

} // namespace pcr
