// pc_rando_gen_entrances.h - entrance randomizer, placement and logic (docs/RANDOMIZER_PLAN.md step
// R6.11): a port of wwrando 1.10.0_9775811's EntranceRandomizer._randomize (randomizers/entrances.py):
//   - init_banned_exits (required bosses mode: the banned boss, dungeon and miniboss exits, and the
//     islands that already hold a banned dungeon when dungeon entrances stay vanilla);
//   - randomize_one_set_of_entrances per get_all_entrance_sets_to_be_randomized set: the shuffle, the
//     safety entrance of a dungeons-and-caves-only start, terminal / non-terminal exits, the
//     nonprogress split (split_nonprogress_entrances_and_exits, with the Forsaken Fortress boss
//     entrance special case) and randomize_one_set_of_exits (sea-inwards nesting, the last-entrance
//     terminal filter, the required-bosses island filter, weighted_choice of the exit);
//   - finalize_all_randomized_sets_of_entrances: nested_entrance_paths for the spoiler log,
//     logic.update_entrance_connection_macros, and the banned/required boss island check.
// The tables, sets and nesting walks are R6.10 (pc_rando_entrances.h). The decisions go into
// logic.state().entranceConnections (EntranceRandomizer.entrance_connections, which the Logic, the
// hints and the placement read). Runtime (stage data, warp-outs) is R6.12-R6.13.
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and the Mac CLI link it.
#pragma once

#include <string>
#include <vector>

#include "pc_rando_gen_bosses.h"
#include "pc_rando_logic.h"
#include "pc_rando_options.h"
#include "pc_rando_rng.h"

namespace pcr {

// randomizers/entrances.py progress_randomize_duration_weight.
constexpr int kWeightEntrances = 5;

struct EntranceRandoResult {
    // nesting_enabled and nested_entrance_paths (each path: outermost entrance first, exit last).
    bool nestingEnabled = false;
    std::vector<std::vector<std::string>> nestedEntrancePaths;
};

// EntranceRandomizer._randomize on the WWRandomizer's Logic (call only when entranceRandoEnabled):
// draws from `rng`, writes the connections into logic.state().entranceConnections and runs
// updateEntranceConnectionMacros. `bosses` is RequiredBossesRandomizer's state (empty when the mode
// is off); dungeonsAndCavesOnlyStart is WWRandomizer.dungeons_and_caves_only_start. False (and
// *err: the exception wwrando raises) on an error.
bool randomizeEntrances(Logic& logic, const Options& o, pc::rando::PyRandom& rng, const RequiredBosses& bosses,
                        bool dungeonsAndCavesOnlyStart, EntranceRandoResult& out, std::string* err);

} // namespace pcr

// The plan's name for this step's entry point.
inline bool pc_rando_gen_entrances(pcr::Logic& logic, const pcr::Options& o, pc::rando::PyRandom& rng,
                                   const pcr::RequiredBosses& bosses, bool dungeonsAndCavesOnlyStart,
                                   pcr::EntranceRandoResult& out, std::string* err) {
    return pcr::randomizeEntrances(logic, o, rng, bosses, dungeonsAndCavesOnlyStart, out, err);
}
