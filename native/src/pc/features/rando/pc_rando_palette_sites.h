// pc_rando_palette_sites.h - the enemy palettes outside the archives (docs/RANDOMIZER_PLAN.md step
// R7.2e): wwrando randomizers/palettes.py shift_colors_for_file_group's particle part
// (shift_all_colors_in_particle) and shift_hardcoded_*_colors. The game API is
// native/include/pc/pc_rando_palette_code.h; this header (body pc_rando_palette_sites.cpp) adds what the palettes smoke reads.
//
// - Particles: a colour editor on every Pscene file (R7.1b's registry, target PCR_JPC_SCENE_ALL,
//   registered by an activation listener) runs palettes.py's loop on the bytes the
//   JPAResourceManager will read: per group in file order, every particle of the file whose id is in
//   the group's list gets BSP1 prm / env colour, its prm / env colour keys and SSP1 prm / env colour
//   hue-shifted (v_shift 0, as wwrando), alpha kept. A particle in several groups is shifted by each,
//   as wwrando's per-group loop does. common.jpc is not touched (wwrando loops Pscene000-254 only).
// - Code colours: a table of the decompiled values of each site (pc_rando_palette_code.h's
//   PcRandoPaletteSite; the game passes its own value, a different one is logged once and shifted as
//   it is) and d_a_mant's 5 cape palettes (pal_d, registered by the actor's static initialiser).
//
// Pure: no game headers (cos_rando_core links it).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "pc/pc_rando_palette_code.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registers the activation listener that adds the particle editor (pc_rando_patchers.def).
void pc_rando_palette_code_init(void);

// The particle editor (what the listener registers), for the smoke's copies of the disc files.
void pc_rando_palette_particles_edit(int scene, uint8_t* data, size_t size, void* user);

// One site of the table.
typedef struct PcRandoPaletteSiteInfo {
    const char* name;  // short name for the dumps ("tn", "mo2", ...); palette_check.py uses the same
    const char* group; // the palette group (palette_randomizable_files.txt Name)
    int count;         // colours
    const uint8_t (*rgb)[3]; // the decompiled values
} PcRandoPaletteSiteInfo;
const PcRandoPaletteSiteInfo* pc_rando_palette_site(int site);

// d_a_mant's pal_d as registered (NULL before d_a_mant's static initialiser, or a NULL slot).
uint8_t* pc_rando_palette_mant_orig(int slot);

typedef struct PcRandoPaletteCodeStats {
    uint32_t particleFiles;   // Pscene files the editor ran on
    uint32_t particleShifted; // particles shifted (each group pass)
    uint32_t particleColors;  // colours written (prm, env, keys, SSP1)
    uint32_t particleBad;     // files the editor could not read
    uint32_t particleConflicts; // a prm / env key table overlap that could not hold both shifts
    uint32_t codeShifted;     // code colours shifted (each call)
    uint32_t codeMismatch;    // calls whose value was not the table's
    uint32_t mantShifted;     // cape palettes built
    uint32_t editorRegistered; // 1 while the particle editor is registered for this activation
} PcRandoPaletteCodeStats;
void pc_rando_palette_code_stats(PcRandoPaletteCodeStats* out);

#ifdef __cplusplus
}
#endif
