// pc_rando_entr_stage.h - the entrance randomizer's stage data at run time (docs/RANDOMIZER_PLAN.md
// step R6.12): wwrando 1.10.0_9775811's randomizers/entrances.py EntranceRandomizer._save, first half
// (update_all_entrance_destinations -> update_entrance_to_lead_to_exit for every exit), applied in
// memory to stage.dzs / room.dzr as dRes_info_c::loadResource hands them over (an archive patcher,
// pc_rando_patchers.def; R0.10). The boss warp-out half (update_all_boss_warp_out_destinations:
// WARP_WIND_AFTER and M2tower/Room0 SCLS[1]) is R6.13.
//
// The map: the active seed's "entrances" (entrance name -> exit name, pc_rando_seed()->entrances)
// over EntranceRandomizer.__init__'s default connections (pcr::EntranceConnections, R6.10), only when
// the seed's options turn entrance rando on (is_enabled: wwrando's _save runs only then). For every
// exit X and its entrance E (O: the outermost island entrance of X), as wwrando does it:
//   a. E's room.dzr SCLS[E.scls_exit_index] -> X's stage, room and spawn;
//   b. the first PLYR with E's spawn id (in E's stage.dzs when it has PLYR entries, else in E's
//      room.dzr): spawn type (params 0xF000) 5 -> 1 (the DRI spawn; KoRL would crash out of TotG);
//   c. X a miniboss or boss arena: X's stage.dzs SCLS[0] (save and reload) -> the dungeon start
//      (SCLS[0] of E's stage.dzs, a vanilla constant, kDungeonStarts) when E is a miniboss or boss
//      entrance, else E itself;
//   d. X not a boss arena: X's room.dzr SCLS[X.scls_exit_index] -> E;
//   e. X Savage Labyrinth: every SCLS to "sea" in Cave10/Room0, Cave10/Room20 and Cave11/Room0 -> E;
//   f. X a secret cave, a fairy fountain or Ice Ring Isle Inner Cave: X's stage.dzs 2DMA[0] sector
//      (byte 0x36: x low nibble, y high, signed) -> O's island ((n-1)%7-3, (n-1)/7-3), so
//      save-and-quit inside puts the player on that island (dComIfGs_setGameStartStage, STAG stage
//      id 11-13);
//   g. X Fire Mountain or Ice Ring Isle Secret Cave (MiniKaz / MiniHyo Room0): the first VolTag ACTR
//      (all ACTR layers in file order): unchanged for its own entrance, VolTag type (params 0xC0)
//      2 / 1 for the other one's (the timer of the cave it came from), else removed (the 8-byte name
//      cleared, wwlib's remove_entity), since an entrance without a timer would throw the player out.
// The records a..g never overlap, so the order of the exits does not matter. Each edit checks the
// file first (decision D9): the chunk and entry exist, a SCLS of a/d still leads to its vanilla stage
// (or already to the target), STAG says 11-13 for f, VolTag is there for g; else that edit is refused
// and logged and the record stays vanilla.
//
// R3.16 (add_spawns_outside_boss_doors) appends the spawns 27 that b looks for in the boss door
// rooms; its patcher is registered first. Those spawns are type 0, so b never changes them (the
// smoke's fixture checks wwrando changes no appended entry).
//
// Pure: no game headers or calls (cos_rando_core). Game thread only.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "pc_rando_entrances.h"
#include "pc_rando_resources.h"

struct PcRandoSeed;

namespace pcr {

// An entrance map as the runtime uses it.
struct EntrStageMap {
    EntranceConnections conn;           // done_exits_to_entrances (all 44 decided)
    int outermost[kNumZoneExits];       // get_outermost_entrance_for_exit (an island entrance)
};

// The map of a seed: false (and *err) when entrance rando is off in its options (*err empty), its
// options or entrance/exit names do not parse, an exit has no entrance or the nesting loops.
bool entrStageMapFromSeed(const PcRandoSeed& seed, EntrStageMap& out, std::string* err);

// The active seed's map: nullptr while pc_rando() is 0, without a seed, with entrance rando off or a
// bad map (logged once per seed). Built once per seed (by its hash).
const EntrStageMap* entrStageActiveMap();

// What entrStagePatch did to one file.
struct EntrStagePatchStats {
    int written = 0; // records written (a..g; e counts each SCLS)
    int refused = 0; // records the D9 checks refused (logged)
    int noSpawn = 0; // b found no PLYR with the entrance's spawn id (logged): only the spawns 27 R3.16
                     // adds to the boss door rooms can be missing, on a file R3.16 did not patch
};

// Patches one raw stage file for the map. room -1: <stage>'s stage.dzs, else Room<room>'s room.dzr.
// stagePlyr: the PLYR entry count of <stage>'s stage.dzs (for b in a room.dzr); -1 when unknown
// (taken as 0 and logged when b needs it). Returns false when the file does not parse (nothing
// written).
bool entrStagePatch(const EntrStageMap& m, const char* stage, int room, uint8_t* data, size_t size, int stagePlyr,
                    EntrStagePatchStats* stats);

// The PLYR entry count of a raw stage file (0 when it does not parse).
int entrStagePlyrCount(const uint8_t* data, size_t size);

// c's dungeon start: SCLS[0] of the stage.dzs of a miniboss / boss entrance's stage, as on the USA
// disc (wwrando reads it from the file; the port keeps the constant). The patcher checks it against
// that stage.dzs when it loads (entrStageDungeonStartMismatches).
struct EntrSclsDest {
    const char* stage;
    uint8_t room;
    uint8_t spawn;
};
const EntrSclsDest* entrStageDungeonStart(const char* entranceStage);
// A raw SCLS entry leads to (stage, room, spawn).
bool entrSclsEquals(const uint8_t* entry, const char* stage, uint8_t room, uint8_t spawn);

// --- the archive patcher (pc_rando_patchers.def) and its counters for the smoke ---

struct EntrStageRuntimeStats {
    int files = 0;      // stage files the patcher saw with a map active
    int patched = 0;    // files it wrote at least one record in
    int written = 0;    // records written
    int refused = 0;    // records refused (D9)
    int noSpawn = 0;    // b's spawn missing (EntrStagePatchStats::noSpawn)
    int badFiles = 0;   // files that did not parse
    int startMismatches = 0; // dungeon-start stage.dzs files whose SCLS[0] is not kDungeonStarts'
    int noMap = 0;      // files seen with pc_rando() on but no map (entrance rando off / no seed)
};
const EntrStageRuntimeStats& entrStageRuntimeStats();
void entrStageRuntimeReset();

} // namespace pcr

extern "C" {
// The patcher (stage.dzs and room.dzr) and its init (pc_rando_patchers.def).
void pc_rando_entr_stage_patch(PcRandoResFile* file);
void pc_rando_entr_stage_init(void);
}
