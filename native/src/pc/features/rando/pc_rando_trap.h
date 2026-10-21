// pc_rando_trap.h - trap chests and chest type matches contents (docs/RANDOMIZER_PLAN.md step R6.5).
//
// wwrando 1.10.0_9775811, randomizers/items.py change_chest_item, for every "Chest..." path of a
// location:
//   - an "Ice Trap Chest" item sets the chest's custom trap flag (behavior_type |= 0x40, params bit
//     0x40) and leaves its item_id alone, then tweaks.py add_trap_chest_event_to_stage adds the event
//     DEFAULT_TREASURE_TRAP to the stage's Stage.arc event_list.dat (once per stage);
//   - with chest_type_matches_contents, chest_type (params 0x00F00000) becomes
//     get_ctmc_chest_type_for_item(item): 0 light wood (not in logic.all_progress_items), 2 metal
//     (progress, not a key), 1 dark wood (a key; with required_bosses only the keys of required
//     dungeons, the others 0). The dark wood texture wwrando ships is not used (decision D12): type 1
//     is the game's own dark wood chest.
// The game side (d_a_tbox.cpp hunks, game_hooks/pc_rando_hooks_trap.cpp) gives a trap chest no item
// and plays SPRING_TRAP (asm/actors/d_a_tbox.cpp demoProcSpringTrap).
//
// The port does the file edits as archive patchers (pc_rando_patchers.def), registered after R2.1's
// chest placement and R2.10's added chests so they see their result:
//   - dzx: every chest key (pcr::location_keys(), DzxField::TboxItem, Dzx and Added) of the file
//     whose placement held (R2.1 status written/unchanged, R2.10 site applied; D9) gets the trap bit
//     and the chest type;
//   - event: a stage with at least one trap chest key gets DEFAULT_TREASURE_TRAP (R3.14 serializer).
// A placement names the trap ("Ice Trap Chest", pc_rando_seed()->location_item_names); its item id
// for the location is the original item (pc_rando_seedfile.cpp), which is what the chest keeps.
//
// What is needed per seed (the trap locations, each chest location's type, the trap stages) is the
// chest plan, built once per active seed on the first patch: the type needs
// logic.all_progress_items, which is a Logic of the seed's options in WWRandomizer.__init__'s state
// (make_useless_progress_items_nonprogress already ran; nothing later changes the list).
//
// Pure: no game headers or calls (cos_rando_core; native/check/rando/test_R6_5.cpp).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "pc_rando_event_ser.h"
#include "pc_rando_json.h"
#include "pc_rando_options.h"
#include "pc_rando_resources.h"

struct PcRandoSeed;

// params bits (asm/actors/d_a_tbox.cpp, data/actor_parameters.txt)
#define PC_RANDO_TBOX_TRAP_BIT 0x00000040u
#define PC_RANDO_TBOX_TYPE_MASK 0x00F00000u
#define PC_RANDO_TBOX_TYPE_SHIFT 20
// No chest type change (chest_type_matches_contents off, or not a chest location).
#define PC_RANDO_CHEST_TYPE_KEEP 0xFF

namespace pcr {

// item_name.endswith(" Trap Chest").
bool isTrapChestItem(const std::string& item);

// items.py get_ctmc_chest_type_for_item. `progress`: logic.all_progress_items (names, any order).
// `requiredDungeons`: boss_reqs.required_dungeons (full dungeon names), read only with required_bosses.
int ctmcChestType(const std::string& item, const std::vector<std::string>& progress, bool requiredBosses,
                  const std::vector<std::string>& requiredDungeons);

// boss_reqs.required_dungeons from required_boss_item_locations (the zone of each location name).
std::vector<std::string> requiredDungeonsOf(const std::vector<std::string>& requiredBossLocations);

// A placement's "options" object as Options (missing options keep their defaults). False with *err
// for a value of the wrong kind or out of range.
bool optionsFromJson(const Json& options, Options* out, std::string* err);

// logic.all_progress_items for the options (Logic.__init__ + initialize_from_randomizer_state with
// WWRandomizer.__init__'s state), distinct names. False with *err when the logic fails.
bool allProgressItemNames(const Options& opts, std::vector<std::string>* out, std::string* err);

// The per-seed chest plan.
struct ChestPlan {
    bool ok = false;
    bool ctmc = false;          // chest_type_matches_contents
    std::vector<uint8_t> trap;  // per PCR_LOC_*: 1 for a trap chest
    std::vector<uint8_t> type;  // per PCR_LOC_*: the chest type, or PC_RANDO_CHEST_TYPE_KEEP
    std::vector<std::string> trapStages; // stage dirs that get DEFAULT_TREASURE_TRAP, key order
    int numTraps = 0;
    std::string error;
};
// Builds the plan of a parsed seed. With ctmc off the Logic is not built.
bool buildChestPlan(const PcRandoSeed& seed, ChestPlan* out);

// tweaks.py add_trap_chest_event_to_stage on an event list: 1 added, 0 already there, or a negative
// PCR_EVL_* error (the list may then be half edited; do not write it).
int addTrapChestEvent(EvsList& list);

} // namespace pcr

extern "C" {

// Registers the dzx and event_list.dat patchers (pc_rando_patchers.def).
void pc_rando_trap_init(void);
void pc_rando_trap_patch_dzx(PcRandoResFile* file);
void pc_rando_trap_patch_event(PcRandoResFile* file);

// The active seed's plan (built on first use; NULL when pc_rando() is 0, there is no seed, or the
// plan failed: pc_rando_trap_plan_error then says why).
const pcr::ChestPlan* pc_rando_trap_plan(void);
const char* pc_rando_trap_plan_error(void);
// 1 when the active seed puts a trap chest at location `loc`.
int pc_rando_trap_loc(int loc);

// What the dzx patcher did to a chest key (pcr::location_keys() index) the last time its file loaded.
enum PcRandoTrapKeyResult {
    PCR_TRAP_KEY_NONE = 0, // not patched (no plan entry, or its file not loaded)
    PCR_TRAP_KEY_DONE,     // trap bit and/or chest type written
    PCR_TRAP_KEY_SKIPPED,  // the placement did not hold for that entry (D9): left as it was
};
typedef struct PcRandoTrapKeyStatus {
    uint8_t result;   // PCR_TRAP_KEY_*
    uint32_t params;  // the entry's params after the patch (DONE)
    uint32_t patches; // how often since boot
} PcRandoTrapKeyStatus;
const PcRandoTrapKeyStatus* pc_rando_trap_key_status(int key_index);
// Stages whose event_list.dat got DEFAULT_TREASURE_TRAP since boot (or the last reset); `last`
// is the last result (1 added, 0 there already, < 0 a PCR_EVL_* error).
typedef struct PcRandoTrapEventStats {
    uint32_t added;
    uint32_t present;
    uint32_t failed;
    int last;
    char lastStage[16];
} PcRandoTrapEventStats;
void pc_rando_trap_event_stats(PcRandoTrapEventStats* out);
// Forgets the statuses and the cached plan (tests).
void pc_rando_trap_reset(void);

} // extern "C"
