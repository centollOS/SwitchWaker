// pc_rando_jpc_scene.h - what the particle bank hook (docs/RANDOMIZER_PLAN.md step R7.1b) does to a
// particle file when the game loads it: a registry of particle imports and colour editors. Pure: no
// game headers, fixed-size storage (registrations come from activation listeners, where the game's
// operator new would take blocks of whatever JKRHeap is current).
//
// - An import {destination scene, particle id, source scene} is wwrando's copy of one particle (and
//   every texture of it the destination lacks) into files/res/Particle/Pscene<dest>.jpc: tweaks.py
//   add_inter_dungeon_warp_pots (R7.1c, source 035) and enemies.py update_loaded_particles (R7.4c,
//   source "any": the first of Pscene000-254 that holds the id, in that order). As in wwrando, an
//   import whose id the destination already holds is skipped.
// - An editor is a callback that edits a loaded particle file in place (the size never changes):
//   R7.2e's particle colour shifts (target "every Pscene", as palettes.py loops over all of them) and
//   R7.5b's arrow trail colour (target common.jpc). Editors run after the imports, on the bytes the
//   JPAResourceManager will read, in registration order.
//
// The game side (game_hooks/pc_rando_hooks_particles.cpp) applies them in dPa_control_c::createRoomScene
// and dPa_control_c::createCommon (d_particle.cpp), and clears this registry when pc_rando() turns
// 0 (pc_rando_deactivate). Steps register from an activation listener (active == 1); registering an
// import or an editor twice keeps one.
#pragma once

#include <stddef.h>
#include <stdint.h>

#define PCR_JPC_SCENE_ANY (-1)      // import source: the first Pscene000-254 that holds the id
#define PCR_JPC_SCENE_COMMON 0x100  // editor target: common.jpc
#define PCR_JPC_SCENE_ALL 0x101     // editor target: every Pscene file
#define PCR_JPC_SCENE_MAX_IMPORTS 1024
#define PCR_JPC_SCENE_MAX_EDITORS 32

struct PcRandoJpcImport {
    uint16_t id;
    int16_t dest;   // 0-254
    int16_t source; // 0-254 or PCR_JPC_SCENE_ANY
};

// Edits a particle file in place. scene: 0-254, or PCR_JPC_SCENE_COMMON. data/size: the whole file
// as the JPAResourceManager reads it (JPAC1-00, big-endian; pc_rando_jpc_scan lists it). The game
// side runs editors inside a host allocation scope, so their std containers use host memory.
typedef void (*PcRandoJpcEditor)(int scene, uint8_t* data, size_t size, void* user);

// 1 when registered (or already registered), 0 when the table is full or an argument is out of range
// (dest 0-254; source 0-254 or ANY).
int pc_rando_jpc_scene_add_import(int dest, uint16_t id, int source);
// target: 0-254, PCR_JPC_SCENE_COMMON or PCR_JPC_SCENE_ALL. 1 when registered (or already, same fn,
// user and target), 0 when full or out of range. A common.jpc editor is applied at once when the
// game has already built common.jpc's resources (the notifier below).
int pc_rando_jpc_scene_add_editor(int target, PcRandoJpcEditor fn, void* user);

// The imports into `dest`, in registration order: copies up to cap of them into out (may be NULL)
// and returns how many there are.
size_t pc_rando_jpc_scene_imports(int dest, PcRandoJpcImport* out, size_t cap);
// Whether `scene` (0-254 or COMMON) has an editor (its own, or ALL for a Pscene).
bool pc_rando_jpc_scene_has_editors(int scene);
// Calls the editors of `scene` in registration order, starting at registration index `from` (the
// value a previous call returned; 0 for all). Returns the registration count, for the next `from`.
size_t pc_rando_jpc_scene_run_editors(int scene, uint8_t* data, size_t size, size_t from);

// Number of registered imports / editors (all targets).
size_t pc_rando_jpc_scene_import_count(void);
size_t pc_rando_jpc_scene_editor_count(void);

// Forgets every import and editor.
void pc_rando_jpc_scene_clear(void);

// Called (by pc_rando_jpc_scene_add_editor) after a common.jpc editor is registered. Set once by the
// game side; NULL by default.
typedef void (*PcRandoJpcCommonNotifier)(void);
void pc_rando_jpc_scene_set_common_notifier(PcRandoJpcCommonNotifier fn);
