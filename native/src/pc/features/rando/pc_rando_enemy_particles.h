/*
 * pc_rando_enemy_particles.h - the enemy randomizer's particles (docs/RANDOMIZER_PLAN.md step R7.4c):
 * wwrando 1.10.0_9775811 randomizers/enemies.py, the particles_to_load_for_each_jpc_index part of
 * save_changed_enemies_and_randomize_their_params and update_loaded_particles.
 *
 * wwrando, per done enemy location (in done_enemy_locations order): the destination particle bank is
 * the room's room.dzr FILI[0] loaded_particle_bank on the sea, else the stage's stage.dzs STAG[0]
 * loaded_particle_bank; every "Required particle IDs" entry of the NEW enemy type is appended (once)
 * to that bank's list. update_loaded_particles then copies each listed particle that
 * files/res/Particle/Pscene<bank>.jpc lacks from the first of Pscene000-254 holding it, with every
 * texture of it the bank lacks.
 *
 * The port does the same on the files the game loads: a dzx patcher (pc_rando_patchers.def, after
 * R7.4a's) reads the bank from the raw stage.dzs (non-sea stage folders) or the raw sea room.dzr as
 * the game loads them, and registers each required id as an import {bank, id, "any"} in R7.1b's
 * registry (pc_rando_jpc_scene.h). The game parses those files (dStage_infoCreate, the room's
 * FileList) before it reads the bank (dPa_control_c::readScene), and the bank hook builds the merged
 * Pscene file at createRoomScene, so the imports are in place for the bank they belong to. Imports
 * accumulate for the activation (R7.1b clears them when pc_rando() turns 0); an id the bank already
 * holds is skipped by the hook, as wwrando's `if particle_id in dest_jpc.particles_by_id: continue`.
 *
 * Difference from wwrando (documented in docs/RANDO_HOOKS.md R7.4c): a bank shared by several stage
 * folders or sea rooms (Pscene043: ma2room, ma3room, majroom, MajyuE and sea/Room1; Pscene080: four
 * Ganon stages; sea bank 200: 16 rooms) holds, in wwrando's file, every one of those places' ids; here
 * it holds the ids of the places loaded so far in the activation, which always includes the place
 * being loaded. The ids a place needs are therefore always there, except on the sea when a room is
 * loaded while another room's copy of the same bank is still alive (dPa_control_c::readScene then
 * keeps the built bank, as in wwrando's game, where a room may even get another room's bank).
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. Game thread only. The state
 * it keeps (the parsed seed enemies, the banks read) is allocated with the global operator new, so the
 * game side calls it in a host allocation scope (JKRPcHostAllocScope): the patcher through
 * game_hooks/pc_rando_hooks_enemy_particles.cpp (listed in pc_rando_patchers.def; patchers run while an
 * archive's data heap is current), the smoke around its calls.
 */
#ifndef PC_RANDO_ENEMY_PARTICLES_H
#define PC_RANDO_ENEMY_PARTICLES_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
#include <vector>
namespace pcr {
struct EnemyRandoResult;
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Counters since the seed was parsed (pc_rando() turning 1 with a new seed). */
typedef struct PcRandoEnemyParticleStats {
    uint32_t files;      /* bank files seen (stage.dzs of a non-sea stage, sea room.dzr) with placed enemies */
    uint32_t requested;  /* required ids requested (sum over files) */
    uint32_t registered; /* register calls accepted by the registry (a repeat counts too) */
    uint32_t failed;     /* registry full, a bad bank (0xFF) or a file that does not parse */
    int last_bank;       /* the bank of the last file seen, -1 none */
} PcRandoEnemyParticleStats;

void pc_rando_enemy_particles_stats(PcRandoEnemyParticleStats* out);

/* The patcher, for stage.dzs / room.dzr files. pc_rando_enemy_particles_init registers it as is (unit
   tests); the game registers it inside a host allocation scope (pc_rando_enemy_particles_hooks_init). */
void pc_rando_enemy_particles_patch(PcRandoResFile* file);
void pc_rando_enemy_particles_init(void);

/* The required ids of the active seed's placements whose bank is chosen by this place: a non-sea
   stage folder (room ignored), or sea room `room` (stage "sea"). wwrando's order; copies up to cap
   into out (may be NULL), returns how many there are (0 without a usable seed). */
size_t pc_rando_enemy_particles_required(const char* stage, int room, uint16_t* out, size_t cap);

/* The bank the patcher last read for a place (same keys as above), or -1 when no such file was seen
   in this activation. */
int pc_rando_enemy_particles_bank_of(const char* stage, int room);

#ifdef __cplusplus
}

/* The pure part (patcher, smoke, tests): for one raw file, whether it is the bank file of placed
   enemies (stage.dzs of a non-sea stage folder; room.dzr of a sea room) and, if so, its bank and the
   required ids in wwrando's order (done_enemy_locations order, each id once).
   Returns 1 with *bank and *ids set, 0 when the file chooses no bank for placed enemies (another file,
   no placement there, or only types without particles: *ids empty), -1 when the file does not parse
   or has no STAG / FILI entry. */
int pc_rando_enemy_particles_for_file(const pcr::EnemyRandoResult& seed, const char* stage, int room,
                                      const char* name, uint8_t* data, size_t size, int* bank,
                                      std::vector<uint16_t>* ids);

/* The required ids of `seed`'s placements for a place (as pc_rando_enemy_particles_required). */
void pc_rando_enemy_particles_ids(const pcr::EnemyRandoResult& seed, const char* stage, int room,
                                  std::vector<uint16_t>* ids);
#endif

#endif /* PC_RANDO_ENEMY_PARTICLES_H */
