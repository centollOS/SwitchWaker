/*
 * pc_rando_tweak_stages_a.h - stage tweaks A: the logic and softlock fixes wwrando always applies to
 * stage.dzs / room.dzr (docs/RANDOMIZER_PLAN.md step R3.15; ref/wwrando tweaks.py, called from
 * randomizer.py apply_necessary_tweaks).
 *
 * pc_rando_stages_a_patch is a dzx patcher for R0.10's archive-load dispatcher
 * (pc_rando_resources.h). It edits, in the disc's big-endian bytes, while the archive is mounted:
 *   #4  M2tower/Room0  PLYR spawn 16 evnt_index 0xFF; SCLS to "M2ganon" -> "sea" room 1 spawn 0
 *                      (remove_forsaken_fortress_2_cutscenes)
 *   #5  sea/Room1      ACT2 "Warpmj" copied to ACT1 (add_ganons_tower_warp_to_ff2; a grown copy)
 *   #10 kaze/Room3     default ACTR "AND_SW2" removed; "wiz_r" enable_spawn_switch 0xFF
 *                      (remove_makar_kidnapping_event)
 *   #11 M_NewD2/Room2  SCOB "Akabe" disable_spawn_switch 0xFF (disable_invisible_walls)
 *   #13 M_NewD2/Room13, Room14  + SCOB "SW_C00" (switch 5 / 6, behavior 3)
 *                      (prevent_door_boulder_softlocks; grown copies)
 *   #14 PShip/Stage    every TRES with room_num != 2: x += 2000 (fix_ghost_ship_chest_crash)
 *   #17 sea/Room26     PLYR spawn 1 z += 1000 (fix_totg_warp_out_spawn_pos)
 *   #18 sea/Room24, 46, 22, 8, 37, 25  "Ocanon" enable_spawn_switch 0x2A -> 0xFF, "Oship" x_rot low
 *                      byte 0x2A -> 0xFF (remove_phantom_ganon_requirement_from_eye_reefs)
 *   #24 M2tower/Room0  default ACTR "Otble" removed (fix_helmaroc_king_table_softlock)
 *   #26 sea/Room15     PLYR spawn 1 x = -320170 (prevent_fairy_island_softlocks)
 * "Removed" is wwrando's remove_entity: the 8-byte name is cleared (pc_rando_dzx_suppress), so the
 * entity indexes of the logic files do not move. Records are found as wwrando finds them
 * (entries_by_type: every layer chunk of the type in file order; entries_by_type_and_layer: one
 * chunk), and identified by name or spawn id (plan D9: a record that is not there is logged and the
 * file is left as it was for that tweak).
 *
 * The tweak tweaks.py fix_forsaken_fortress_door_softlock (sea/Room1 room.dzb, face 0x1493 property
 * 0x11) needs the game's cBgD_t layout (loadResource converts dzb files before the dispatcher sees
 * them), so it lives in game_hooks/pc_rando_hooks_stages_a.cpp, which also registers both patchers
 * (pc_rando_patchers.def). Its result is reported here under PCR_SA_FF_DOOR.
 *
 * Pure: no game header or symbol (cos_rando_core links this file into the unit tests).
 */
#ifndef PC_RANDO_TWEAK_STAGES_A_H
#define PC_RANDO_TWEAK_STAGES_A_H

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

enum PcRandoStagesATweak {
    PCR_SA_FF2_CUTSCENES = 0, /* #4 */
    PCR_SA_GT_WARP,           /* #5 */
    PCR_SA_MAKAR,             /* #10 */
    PCR_SA_WALLS,             /* #11 */
    PCR_SA_BOULDERS,          /* #13 */
    PCR_SA_GHOST_SHIP,        /* #14 */
    PCR_SA_TOTG,              /* #17 */
    PCR_SA_EYE_REEFS,         /* #18 */
    PCR_SA_HELMAROC,          /* #24 */
    PCR_SA_FAIRY,             /* #26 */
    PCR_SA_FF_DOOR,           /* fix_forsaken_fortress_door_softlock (game side) */
    PCR_SA_COUNT
};

/* The dzx patcher (stage.dzs / room.dzr). Files of other stages pass through untouched. */
void pc_rando_stages_a_patch(PcRandoResFile* file);

/* Counters since boot (or the last reset): records edited, and edits refused (record missing,
   fingerprint mismatch, no room to grow). */
typedef struct PcRandoStagesAStats {
    uint32_t applied[PCR_SA_COUNT];
    uint32_t failed[PCR_SA_COUNT];
} PcRandoStagesAStats;
void pc_rando_stages_a_stats(PcRandoStagesAStats* out);
void pc_rando_stages_a_reset_stats(void);
/* For the game-side dzb patcher: counts one result of tweak t. */
void pc_rando_stages_a_count(int t, int ok);
/* A short name for logs ("ff2-cutscenes", ...). */
const char* pc_rando_stages_a_name(int t);

/* The values the patcher writes, shared with the smoke (wwrando tweaks.py). */
#define PCR_SA_GHOST_SHIP_DX 2000.0
#define PCR_SA_TOTG_DZ 1000.0
#define PCR_SA_FAIRY_X (-320170.0f)
#define PCR_SA_FF_DOOR_FACE 0x1493
#define PCR_SA_FF_DOOR_PROPERTY 0x11

/* The SW_C00 entries #13 appends (0x24 bytes, disc format) for room 13 or 14; NULL for another room. */
const uint8_t* pc_rando_stages_a_swc00(int room);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_STAGES_A_H */
