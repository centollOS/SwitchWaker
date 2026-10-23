/*
 * pc_rando_warpout.h - boss warp-out destinations under entrance rando (docs/RANDOMIZER_PLAN.md step
 * R6.13): wwrando 1.10.0_9775811 randomizers/entrances.py update_all_boss_warp_out_destinations.
 *
 * wwrando sends the warp out of each boss arena to the island the arena is reached from: the
 * outermost entrance of the arena's zone exit (get_outermost_entrance_for_exit, which follows the
 * nesting) and that entrance's warp_out_stage/room/spawn.
 *   - Gohma, Kalle Demos, Gohdan, Jalhalla and Molgera (M_DragB, kinBOSS, SirenB, M_DaiB, kazeB):
 *     wwrando rewrites the Stage, RoomNo and StartCode properties of the DIRECTOR's NEXT action in
 *     the stage's WARP_WIND_AFTER event. The port does it in code instead (the stage names differ in
 *     length): dEvDt_Next_Stage asks pc_rando_warpout_dest through the hook
 *     pc_rando_warpout_next_stage (game_hooks/pc_rando_hooks_warpout.cpp).
 *   - Helmaroc King (M2tower) has no warp-out event: wwrando rewrites M2tower/Room0 room.dzr SCLS[1]
 *     (the door to Ganondorf's room, which tweak remove_forsaken_fortress_2_cutscenes, R3.15, already
 *     turned into sea room 1 spawn 0). An archive patcher here does the same.
 * wwrando only saves the entrances when the entrance randomizer ran (BaseRandomizer.save,
 * made_any_changes): with every randomize_*_entrances option off, nothing changes.
 *
 * Pure: no game headers, no game calls (cos_rando_core links it into cos_rando_test_R6_13).
 */
#ifndef PC_RANDO_WARPOUT_H
#define PC_RANDO_WARPOUT_H

#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

struct PcRandoSeed;

/* A warp-out destination: a static stage name, room and spawn (start code). */
typedef struct PcRandoWarpOut {
    const char* stage;
    int room;
    int spawn;
} PcRandoWarpOut;

/* 1 when the seed randomizes entrances (EntranceRandomizer.is_enabled: any randomize_*_entrances
   option of the seed is true), else 0 (also for NULL). */
int pc_rando_warpout_enabled(const struct PcRandoSeed* seed);

/* The warp-out destination of the boss arena on stage `bossStage` ("M_DragB", "kinBOSS", "SirenB",
   "M2tower", "M_DaiB", "kazeB") for the seed's entrance map. Returns 1 and fills *out, or 0 when
   nothing changes: no seed, entrances not randomized, not a boss arena stage, or an entrance map
   that does not resolve (an unknown name, an undecided or looping path: logged). */
int pc_rando_warpout_dest(const struct PcRandoSeed* seed, const char* bossStage, PcRandoWarpOut* out);

/* update_helmaroc_king_arena_ganon_door on one file: when `file` is M2tower/Room0 room.dzr and the
   seed randomizes entrances, SCLS[1] (fingerprint: its destination is "sea", as R3.15 leaves it, or
   the disc's "M2ganon") gets the Helmaroc King arena's warp-out stage, room and spawn. Returns 1 when
   written, 0 when not this file or nothing changes, -1 on a fingerprint or format mismatch (D9: left
   as it is, logged). */
int pc_rando_warpout_patch_file(PcRandoResFile* file, const struct PcRandoSeed* seed);

/* What the archive patcher did since the last activation (for the smoke). */
typedef struct PcRandoWarpoutStatus {
    int m2towerPatched;  /* SCLS[1] writes */
    int m2towerSkipped;  /* M2tower/Room0 loads that left it (entrances off, mismatch) */
} PcRandoWarpoutStatus;
void pc_rando_warpout_status(PcRandoWarpoutStatus* out);

/* Registers the archive patcher (pc_rando_patchers.def) and an activation listener that clears the
   status. Idempotent. */
void pc_rando_warpout_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_WARPOUT_H */
