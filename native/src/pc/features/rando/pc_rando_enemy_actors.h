/*
 * pc_rando_enemy_actors.h - the enemy randomizer's added, removed and converted actors
 * (docs/RANDOMIZER_PLAN.md step R7.4b): wwrando 1.10.0_9775811 randomizers/enemies.py
 * EnemyRandomizer.add_modify_and_replace_actors_for_enemy_rando and randomize_enemies_spawned_by_objects,
 * applied to the raw stage files while pc_rando() is 1 and the seed holds a usable "enemies" key
 * (pc_rando_enemies_active(), R7.4a). Registered after R7.4a's patcher, as _save runs them after
 * save_changed_enemies_and_randomize_their_params.
 *
 * wwrando runs these on every group of logic/enemy_locations.txt whenever enemy rando is on (not only
 * on the groups it randomized). In its order, on the tables of R7.3a (pc_rando_enemy_data.h):
 *   1. "Actors to replace with ALLdies" (13): the AND_SW0 / AND_SW2 / Kbota_A / Kbota_B / KbotaC at the
 *      path becomes ALLdie: params 0xFFFFFFFF, x_rot 0, z_rot 0, then switch_to_set = the original's
 *      switch_to_set (read with the original class's bitfield, written with d_a_alldie's); position,
 *      y_rot and enemy_number kept. Any other name: wwrando raises; here a D9 mismatch.
 *   2. "Existing actors to modify" (3): the fields the entry gives (Name, Params, rotations, Position).
 *      One of them is TF_01's stage.dzs ScalableDoor (TGDR) 005.
 *   3. "New actors to add" (5 in 3 groups): appended to the ACTR chunk of the layer of the group's
 *      first enemy, in its file (the file grows: pc_rando_res_grow_dzx; dzx.py's ACTR defaults:
 *      params / position / rotations 0, enemy_number 0xFFFF, unless the entry sets them).
 *   4. "Actors to remove" (2): dzx.remove_entity, which wwrando itself does by clearing the name
 *      (pc_rando_dzx_suppress), so later indexes keep their meaning. pc_rando_enemy_actors_selfcheck
 *      asserts that no other path of the table points at a removed entry.
 *   5. The Homen enemy_to_spawn (seed "enemies".homen, drawn by R7.3c): kaze/Room2 ACTR 0x27, kaze/Room10
 *      ACTR 0 and 9 (entries_by_type(ACTR): every ACTR-type chunk of the file in chunk order).
 * D9 fingerprints: the entry's vanilla name (for the modified and removed entries the name the disc
 * holds there after the earlier patchers: ootubo1, Zenshut, Yswdr00 copied by R2.10, TagEv; the stone
 * heads Homen1/Homen2). A mismatch leaves that entry as it is and is logged.
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. Game thread only.
 */
#ifndef PC_RANDO_ENEMY_ACTORS_H
#define PC_RANDO_ENEMY_ACTORS_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What an edit does. */
enum PcRandoEnemyActorKind {
    PCR_EACTOR_ALLDIE = 0, /* a switch-setting actor converted to ALLdie */
    PCR_EACTOR_MODIFY = 1, /* an existing actor's fields */
    PCR_EACTOR_ADD = 2,    /* a new ACTR entry */
    PCR_EACTOR_REMOVE = 3, /* an entry suppressed (name cleared) */
    PCR_EACTOR_HOMEN = 4   /* a stone head's enemy_to_spawn */
};

/* One edit of the table, in wwrando's order (see above). */
typedef struct PcRandoEnemyActorTarget {
    int kind;          /* PcRandoEnemyActorKind */
    const char* label; /* the table's path ("Abship/Room0.arc/Actor005"); adds: "<file>/Add<k>"; homen:
                          "kaze/Room2.arc/Homen027" */
    const char* stage; /* "Abship" */
    int room;          /* N, or -1 for Stage.arc (stage.dzs) */
    char tag[5];       /* the chunk tag ("ACTR", "ACT5", "TGDR", "SCOB") */
    int index;         /* the entry index in that chunk; adds: -1 (appended); homen: the
                          entries_by_type(ACTR) index (over every ACTR-type chunk) */
    const char* vanilla_name; /* the D9 name(s) expected, '|' separated; NULL for adds */
} PcRandoEnemyActorTarget;

/* The table's edits (13 + 3 + 5 + 2 + 3 = 26), built once. */
const PcRandoEnemyActorTarget* pc_rando_enemy_actors_targets(int* count);

/* Counters since the last activation (a new seed). */
typedef struct PcRandoEnemyActorStats {
    uint32_t files;      /* files that hold at least one edit (with a usable seed) */
    uint32_t done;       /* edits made */
    uint32_t mismatched; /* D9 mismatches (and missing entries) */
    uint32_t failed;     /* other failures (unknown bitfield, the file could not grow) */
    uint32_t skipped;    /* edit files seen while pc_rando() was 0 or the seed had no usable enemies */
} PcRandoEnemyActorStats;
void pc_rando_enemy_actors_stats(PcRandoEnemyActorStats* out);

/* The last result of target `i` since the activation: 1 done, 0 D9 mismatch (or a missing entry / a file
   that does not parse), 2 other failure, -1 not seen. */
int pc_rando_enemy_actors_last_result(int i);

/* The table check of "Actors to remove": the number of other table paths (enemy locations, ALLdies,
   modified actors, stone heads) that name a removed entry (0 when sound). why (optional) gets the
   first conflict. */
int pc_rando_enemy_actors_selfcheck(char* why, size_t whySize);

/* The patcher (registered by pc_rando_enemy_actors_init for stage.dzs / room.dzr files). */
void pc_rando_enemy_actors_patch(PcRandoResFile* file);
void pc_rando_enemy_actors_init(void);

/* Smokes: while 1, pc_rando_enemy_actors_patch does nothing. */
void pc_rando_enemy_actors_set_bypass(int on);

/* The pure edit (patcher, smoke and tests): every target in the file (stage, room, file->name),
   homen[3] the stone heads' enemy_to_spawn values. Grows the file through file->alloc when it gets new
   actors. results[] (optional, indexed like pc_rando_enemy_actors_targets) gets 1 / 0 / 2 per handled
   target (as pc_rando_enemy_actors_last_result). Returns the number of edits made; *handled (optional) the number of the file's targets. */
int pc_rando_enemy_actors_apply(const int homen[3], PcRandoResFile* file, int8_t* results, int* handled);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_ENEMY_ACTORS_H */
