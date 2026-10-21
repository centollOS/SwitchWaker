// pc_rando_gen_items.h - the item fill (docs/RANDOMIZER_PLAN.md step R1.4): a port of wwrando
// 1.10.0_9775811's randomizers/items.py ItemRandomizer._randomize, which places every item of the
// pool into the Logic's locations:
//   (A) dungeon items, when not key-lunacy (randomize_dungeon_items / place_dungeon_item): pre-placed
//       assumed-fill style into prerandomization_item_locations;
//   (B) the weighted forward fill of the progress items (randomize_progression_items_forward_fill);
//   (C) the unique nonprogress items (randomize_unique_nonprogress_items);
//   (D) the consumables (randomize_consumable_items).
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and the Mac CLI link it.
//
// Exactness (decision D1): every RNG draw and every list mutation happens in wwrando's order, so the
// same seed and options give the same done_item_locations as Python. The RNG is the ItemRandomizer's
// own: a fresh get_new_rng() (BaseRandomizer.reset_rng), independent of the other randomizers.
//
// Trace (COS_RANDO_TRACE, plan "The oracle"): with COS_RANDO_TRACE=1 the draws go to stderr, with any
// other value except 0 / empty to that file path (appended), in ww_trace.py's format:
//   # ww_trace 1 seed=<sanitized seed> version=1.10.0_9775811
//   ItemRandomizer randbelow <n> <result>
// so `diff` against `ww_trace.py <seed> --owner ItemRandomizer` finds the first diverging draw.
#pragma once

#include <stdint.h>
#include <stdio.h>

#include <string>

#include "pc_rando_logic.h"
#include "pc_rando_options.h"
#include "pc_rando_rng.h"

namespace pcr {

// ItemRandomizer's state after _randomize (drc_failsafe_location; -1 when not chosen).
struct ItemFillResult {
    int drcFailsafeLocation = -1;
    std::string error; // the text of the exception wwrando raises, when the fill fails
};

// ItemRandomizer._randomize on a Logic set up by initRandomizerLogic (and by the randomizers that run
// before items: their decisions are in logic.state()). `dungeonsAndCavesOnlyStart` is
// WWRandomizer.dungeons_and_caves_only_start (RandomizerLogicInit). Required-bosses banned locations
// are read from logic.state().bannedLocations when options.required_bosses is on. On success every
// location of logic.doneItemLocations is filled. False (and out.error) where wwrando raises.
bool randomizeItems(Logic& logic, pc::rando::PyRandom& rng, bool dungeonsAndCavesOnlyStart, ItemFillResult& out);

// The 11 RNG_CHANGING_OPTIONS values of `o`, in randomizer.py's order (bools 0/1).
void rngChangingOptionValues(const Options& o, int values[pc::rando::kRngChangingOptionCount]);
// WWRandomizer.get_new_rng() for a sanitized seed and the options (seedKey is used only when
// do_not_generate_spoiler_log is on; decision D2).
void newRandomizerRng(pc::rando::PyRandom& rng, const std::string& sanitizedSeed, const Options& o,
                      const char* seedKey = kSwitchWakerSeedKey);

// BaseRandomizer.randomize() for the ItemRandomizer: a fresh RNG from the seed, COS_RANDO_TRACE
// attached (owner "ItemRandomizer"), then randomizeItems.
bool runItemRandomizer(Logic& logic, const RandomizerLogicInit& init, const std::string& sanitizedSeed,
                       ItemFillResult& out, const char* seedKey = kSwitchWakerSeedKey);

// COS_RANDO_TRACE support, shared by the randomizers. traceBegin opens the sink named by the
// environment (none: tracing is off) and writes the header line; traceAttach makes `rng` log its
// draws under `owner` (a string that must outlive the RNG's use); traceEnd closes the sink.
// traceBeginFile does the same for an explicit FILE* (tests); it is not closed by traceEnd.
void traceBegin(const std::string& sanitizedSeed);
void traceBeginFile(FILE* f, const std::string& sanitizedSeed);
void traceAttach(pc::rando::PyRandom& rng, const char* owner);
void traceEnd();

} // namespace pcr
