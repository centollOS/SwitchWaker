/*
 * pc_rando_enemies.h - the enemy randomizer's actor rewrites on the raw room files
 * (docs/RANDOMIZER_PLAN.md step R7.4a): wwrando 1.10.0_9775811 randomizers/enemies.py
 * EnemyRandomizer.save_changed_enemies_and_randomize_their_params and adjust_enemy, applied while
 * pc_rando() is 1 and the seed holds the "enemies" key (R7.3c, pc_rando_gen_enemies.h).
 *
 * The generator already made every random draw of _save (params, x_rot, z_rot after
 * randomize_enemy_params, the EFLAG_* actions); this patcher does the disc-dependent rest, per
 * enemy location of the table (enemy_locations(), path "<stage>/RoomN.arc[/Layer<l>]/Actor<idx>":
 * the idx-th entry of ACTR, or of ACT<l> for a layer), in done_enemy_locations order:
 *   1. D9 fingerprint: the vanilla entry's name is the original type's actor name (wwrando raises
 *      "points to a ... actor, not an enemy"); a mismatch leaves the record vanilla and is logged;
 *   2. the vanilla values _save copies, read with the ORIGINAL type's param names
 *      (actor_parameters.txt bitfields in params / x_rot / z_rot): the enable spawn switch when the
 *      placement category has ChecksConditionSwitch (refused: 0xFE, 0xFF, >= 0xF0), the death switch
 *      for SetsDeathSwitch (refused: 0x00, 0x80, 0xFF, >= 0xF0), the path index when the original
 *      type has a path param (0xFF: none; refused: >= the file's RPAT count). A refusal (wwrando
 *      raises) leaves the record vanilla and is logged;
 *   3. name (8 bytes, zero padded), params, x_rot, z_rot from the seed; Position (set), Position
 *      offset (add), Y Rotation (set), Y Rotation offset (add, & 0xFFFF) from the table;
 *   4. adjust_enemy: EFLAG_CEILING_DROP y -= 500, EFLAG_MAGTAIL_RAISE y += 50, EFLAG_POT_SNAP the
 *      ChuChu takes the position of the nearest d_a_tsubo actor of its layer chunk (Python's
 *      min(): the first of equal distances; none: refused);
 *   5. the copied values written with the NEW type's param names: the spawn switch (d_a_rd also
 *      dont_check_enable_spawn_switch = 0, d_a_st stalfos_type = 1), the death switch, the path index
 *      when the new type has a path param (d_a_fm also type = 1).
 * Positions are computed as Python does (doubles from the float32 record, one float32 rounding at
 * the write). The record is built in a scratch copy and written only when every step succeeded.
 *
 * The other enemy edits (ALLdie conversions, modified / added / removed actors, Homen
 * enemy_to_spawn) are R7.4b; the particles are R7.4c.
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_enemies_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_ENEMIES_H
#define PC_RANDO_ENEMIES_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
namespace pcr {
struct EnemyRandoResult;
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The outcome of one enemy location in a file. */
enum PcRandoEnemyResult {
    PCR_ENEMY_WRITTEN = 0,   /* the record now holds the placed enemy */
    PCR_ENEMY_MISMATCH = 1,  /* D9: the vanilla entry is missing or not the original enemy */
    PCR_ENEMY_REFUSED = 2,   /* a value _save copies is invalid, a param is unknown, no pot to snap to */
    PCR_ENEMY_NOT_PLACED = 3 /* the seed has no placement for this location */
};

/* Counters since the last activation (pc_rando() turning 1). */
typedef struct PcRandoEnemyStats {
    uint32_t files;      /* room files that hold at least one enemy location (with a usable seed) */
    uint32_t written;    /* records written */
    uint32_t mismatched; /* D9 mismatches */
    uint32_t refused;    /* refusals */
    uint32_t skipped;    /* enemy files seen while pc_rando() was 0 or the seed had no enemies */
} PcRandoEnemyStats;

void pc_rando_enemies_stats(PcRandoEnemyStats* out);

/* 1 when the active seed holds a usable "enemies" key (parsed once per seed). */
int pc_rando_enemies_active(void);

/* The patcher (registered by pc_rando_enemies_init for stage.dzs / room.dzr files). */
void pc_rando_enemies_patch(PcRandoResFile* file);
void pc_rando_enemies_init(void);

/* The last result of enemy location `loc` (enemy_locations() index) since the activation, or -1
   when no file holding it was patched. */
int pc_rando_enemies_last_result(int loc);

#ifdef __cplusplus
}

/* The pure edit (used by the patcher, the smoke and tests): every enemy location of the room file
   (stage, room) placed by `seed`, on the raw bytes in place. results[] (optional, indexed by
   enemy_locations() index) gets each handled location's PcRandoEnemyResult. Returns the number of
   records written; *handled (optional) the number of the file's table locations. */
int pc_rando_enemies_apply(const pcr::EnemyRandoResult& seed, const char* stage, int room, uint8_t* data,
                           size_t size, int8_t* results, int* handled);
#endif

#endif /* PC_RANDO_ENEMIES_H */
