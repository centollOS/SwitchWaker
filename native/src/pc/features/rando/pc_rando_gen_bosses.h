// pc_rando_gen_bosses.h - required bosses mode, generator half (docs/RANDOMIZER_PLAN.md step R6.8): a
// port of wwrando 1.10.0_9775811's RequiredBossesRandomizer._randomize (randomizers/boss_reqs.py):
//   - randomize_required_bosses: rng.sample of the 6 boss item locations (item_locations order),
//     the required and banned dungeons and bosses, and the banned locations (every dungeon location
//     of a banned dungeon, plus the 4 letters: Orca for Forbidden Woods, Baito for Earth Temple,
//     Aryll and Tingle for Forsaken Fortress);
//   - logic.update_required_bosses_macro.
// The item fill (R1.4) and the spheres (R1.5) already skip logic.state().bannedLocations; this step
// fills that state. The runtime half (Puppet Ganon's door, the sea chart markers) is R6.9.
//
// Pure C++ (no game headers): cos_rando_core, the unit tests and the Mac CLI link it.
#pragma once

#include <string>
#include <vector>

#include "pc_rando_logic.h"
#include "pc_rando_rng.h"

namespace pcr {

// RequiredBossesRandomizer's instance variables after _randomize (all empty when the mode is off).
struct RequiredBosses {
    std::vector<std::string> requiredBossItemLocations; // sample order
    std::vector<std::string> requiredDungeons;          // item_locations order of the boss locations
    std::vector<std::string> requiredBosses;            // "Gohma", "Kalle Demos", ...
    std::vector<std::string> bannedLocations;           // item_locations order
    std::vector<std::string> bannedDungeons;
    std::vector<std::string> bannedBosses;
};

// RequiredBossesRandomizer._randomize on the WWRandomizer's Logic: draws from `rng`, then writes
// required_boss_item_locations and banned_locations into logic.state() and runs
// updateRequiredBossesMacro. False (and *err: the exception wwrando raises) on an error.
bool randomizeRequiredBosses(Logic& logic, pc::rando::PyRandom& rng, RequiredBosses& out, std::string* err);

} // namespace pcr
