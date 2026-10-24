// pc_rando_palettes.h - the enemy palettes on the game's files (docs/RANDOMIZER_PLAN.md step R7.2c):
// wwrando palettes.py shift_colors_for_file_group's RARC part, and the rat-hole tweak.
//
// Archive patchers (pc_rando_patchers.def -> pc_rando_palettes_init):
//   - a raw patcher (before the J3D loaders, pc_rando_resources.h) for every .bdl .bmd .bmt .brk of a
//     palette group's /res/Object/<RARC>.arc minus the group's excluded files: TEX1 textures shifted
//     by the group's (h_shift, v_shift) (pc_rando_j3d_shift_tex1). Materials, MDL3 and TRK1 colours
//     come with R7.2d; particles and code colours with R7.2e;
//   - a patcher on the post-load list for the groups' .bti files (still raw there):
//     pc_rando_bti_shift;
//   - make_rat_holes_visible_from_behind (tweaks.py, randomizer.py:445): Nzg.arc kana_00.bdl, cull
//     mode none, whenever pc_rando() (wwrando applies it on every seed).
// The shifts are the active seed's "palettes" key (R7.2a; absent when randomize_enemy_palettes is
// off: no texture changes). An archive in several groups is shifted by each, in group order, as
// wwrando's per-group loop does.
//
// Dev override, read once from the environment:
//   COS_RANDO_PALETTE_TEST=<h>,<v>   under pc_rando(), every group uses (h, v), seed or not (the
//                                    palettes smoke and palette_check.py).
//
// Pure: no game headers (cos_rando_core links it).
#pragma once

#include <stdint.h>

#include "pc_rando_j3d.h"

#ifdef __cplusplus
extern "C" {
#endif

// Registers the patchers (pc_rando_patchers.def).
void pc_rando_palettes_init(void);

// The shift of group `group` (pcr::paletteGroup order) for the active seed or the test override:
// 1 with *h / *v, or 0 when palettes are not randomized.
int pc_rando_palette_shift(int group, int* h, int* v);
// 1 while COS_RANDO_PALETTE_TEST is set (with its values).
int pc_rando_palettes_test_override(int* h, int* v);

typedef struct PcRandoPaletteStats {
    uint32_t files;       // group files recoloured (J3D + BTI), counting each group pass
    uint32_t j3dFiles;
    uint32_t btiFiles;
    uint32_t excluded;    // group files skipped as excluded
    PcRandoRecolorStats tex; // summed over the files
    uint32_t ratHoles;    // kana_00.bdl patched
    uint32_t ratHoleFailed; // kana_00.bdl not the disc's (left alone)
} PcRandoPaletteStats;
void pc_rando_palettes_stats(PcRandoPaletteStats* out);
void pc_rando_palettes_reset_stats(void);

#ifdef __cplusplus
}
#endif
