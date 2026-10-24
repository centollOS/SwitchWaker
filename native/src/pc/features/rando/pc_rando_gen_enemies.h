// pc_rando_gen_enemies.h - the enemy randomizer's generator side (docs/RANDOMIZER_PLAN.md step R7.3c):
// a port of wwrando 1.10.0_9775811's randomizers/enemies.py
//   EnemyRandomizer._randomize (decide_on_enemy_pool_for_stage, randomize_enemy_groups_for_stage,
//   randomize_enemy_group, get_amount_of_memory_for_enemy, get_free_memory_for_group), and
//   the RNG part of EnemyRandomizer._save: reset_rng(), randomize_enemy_params for every done location
//   in order (the bitfields packed into params / x_rot / z_rot with actor_parameters.txt's masks), and
//   the 3 Homen enemy_to_spawn choices of randomize_enemies_spawned_by_objects.
//
// wwrando's _save draws on a fresh get_new_rng() after every randomizer ran (its own TODO calls that
// a hack); the draws depend only on the decisions of _randomize (new type, placement category), never
// on disc data, so the generator makes them too and the game (R7.4) needs no RNG.
//
// Exactness (decision D1): Python list semantics are kept (the stage pool, the category/logic
// combos compared as (category string, list), choice over the filtered lists in order), the memory
// model is wwrando's (Free memory or 1,000,000, halved as a float in the 15 stages of
// STAGE_NAMES_WHERE_MULTIPLE_ROOMS_CAN_BE_LOADED_AT_ONCE, MIN_FREE_SPACE_TO_LEAVE_PER_ROOM 60000), and
// the logic filter is R7.3b's on the main Logic (its probes reorder the unplaced item lists before
// the item fill). Upstream bugs are reproduced, not fixed: the sea keys of enemy_locations.txt are
// "sea/RoomN.arc", so `stage_folder == "sea"` never holds and sea rooms get a stage pool of 10 and
// the normal retry counts.
//
// The placement key "enemies" (kept and hashed by PcRandoSeed::extra; absent when randomize_enemies
// is off), written identically by native/tools/rando/ww_export_enemies.py:
//
//   "enemies": {
//     "locations": {
//       "<enemy location path>": {"type": <new pretty name>, "name": <actor name>, "params": <u32>,
//                                 "x_rot": <u16>, "z_rot": <u16>, "category": <placement category>,
//                                 "flags": <EFLAG_* bits>
//                                 [, "pos": [x, y, z]] [, "pos_offset": [x, y, z]]
//                                 [, "y_rot": <u16>] [, "y_rot_offset": <int>]},
//       ...                                       (done_enemy_locations order = enemy_locations.txt order)
//     },
//     "homen": [<kaze/Room2 ACTR 0x27>, <kaze/Room10 ACTR 0>, <kaze/Room10 ACTR 9>]
//   }
//
// params/x_rot/z_rot are the new type's values after randomize_enemy_params, before the copies from
// the vanilla actor that _save makes afterwards (spawn switch, death switch, path index: they need
// the disc, R7.4a does them). The overrides are the location's Position / Position offset /
// Y Rotation / Y Rotation offset (as the YAML writes them: integers, two float y positions). flags tell R7.4a what _save and
// adjust_enemy do next.
//
// Errors: where wwrando raises, the generator fails with the same text (e.g. "Enemy rando failed 50
// times for one stage (M_NewD2)", or Python's IndexError text for a choice from an empty list).
//
// Pure C++ (no game headers): cos_rando_core, the unit tests, the Mac CLI and the game link it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "pc_rando_json.h"
#include "pc_rando_options.h"
#include "pc_rando_rng.h"

namespace pcr {

class Logic;

// EnemyRandomizer.progress_randomize_text and progress_randomize_duration_weight.
constexpr const char* kEnemyProgressText = "Randomizing enemy locations...";
constexpr int kWeightEnemies = 100;

// What _save / adjust_enemy do with a location after the params (R7.4a applies them).
enum : uint8_t {
    EFLAG_SPAWN_SWITCH = 1u << 0,  // ChecksConditionSwitch: copy the vanilla enable spawn switch
    EFLAG_DEATH_SWITCH = 1u << 1,  // SetsDeathSwitch: copy the vanilla death switch
    EFLAG_CEILING_DROP = 1u << 2,  // category "Ceiling" and the type has no Ceiling: y_pos -= 500
    EFLAG_MAGTAIL_RAISE = 1u << 3, // magtail: y_pos += 50
    EFLAG_POT_SNAP = 1u << 4,      // ChuChu in category "Pot": move to the nearest d_a_tsubo of its layer
};

// The candidates for the Homen's enemy_to_spawn (randomize_enemies_spawned_by_objects), in order.
constexpr int kHomenEnemyChoices[] = {0, 1, 2, 3, 4, 5, 6, 7, 10};
constexpr int kNumHomen = 3;

struct EnemyPlacement {
    int location = -1;   // enemy_locations() index
    int type = -1;       // enemy_types() index of the new enemy
    uint32_t params = 0; // after randomize_enemy_params
    uint16_t x_rot = 0;
    uint16_t z_rot = 0;
    uint8_t flags = 0;   // EFLAG_*
    bool operator==(const EnemyPlacement& o) const {
        return location == o.location && type == o.type && params == o.params && x_rot == o.x_rot &&
               z_rot == o.z_rot && flags == o.flags;
    }
};

struct EnemyRandoResult {
    std::vector<EnemyPlacement> placements; // done_enemy_locations order
    int homen[kNumHomen] = {0, 0, 0};
};

// EnemyRandomizer._randomize on `rng` (a fresh get_new_rng()) and the main Logic: the (location,
// type) pairs of done_enemy_locations, in order. False (and *err) where wwrando raises.
bool randomizeEnemyLocations(Logic& logic, pc::rando::PyRandom& rng,
                             std::vector<EnemyPlacement>& done, std::string* err);

// The RNG part of EnemyRandomizer._save on `rng` (a fresh get_new_rng()): fills params, x_rot,
// z_rot and flags of every placement in order, then out.homen. False (and *err) when a bitfield is
// missing from the actor tables (wwrando's "Tried to set unknown actor parameter").
bool randomizeEnemyParams(pc::rando::PyRandom& rng, EnemyRandoResult& out, std::string* err);

// BaseRandomizer.randomize() then the RNG part of save(): each on a fresh RNG from the seed
// (COS_RANDO_TRACE owners "EnemyRandomizer" and "EnemyRandomizer._save").
bool runEnemyRandomizer(Logic& logic, const std::string& sanitizedSeed, const Options& o, const char* seedKey,
                        EnemyRandoResult& out, std::string* err);

// The placement's "enemies" value (see above) and back. enemiesFromJson checks every path names a
// location of the table, every type and actor name match, and the homen values are candidates.
Json enemiesToJson(const EnemyRandoResult& r);
bool enemiesFromJson(const Json& j, EnemyRandoResult& out, std::string* err);

} // namespace pcr
