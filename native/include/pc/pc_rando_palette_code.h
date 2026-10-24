/* pc_rando_palette_code.h - the enemy palette colours that live in code (docs/RANDOMIZER_PLAN.md step
   R7.2e): wwrando randomizers/palettes.py shift_hardcoded_*_colors, which rewrite RGB bytes, an
   RGB5A3 palette, three floats and three `li` immediates inside the actors' RELs. The port keeps the
   decompiled values and shifts them where the game reads them, with the group's (h_shift, v_shift)
   (features/rando/pc_rando_palettes.h pc_rando_palette_shift; gclib hsv_shift_color). The game hunks
   (listed in docs/RANDO_HOOKS.md "R7.2e") have the form
       pc_rando() ? pc_rando_palette_rgb(PCR_PALSITE_<x>, index, orig) : orig
   Every function returns the original unchanged when pc_rando() is 0 or the seed does not randomize
   palettes. Body: features/rando/pc_rando_palette_code.cpp. */
#ifndef PC_RANDO_PALETTE_CODE_H
#define PC_RANDO_PALETTE_CODE_H

#include <stdint.h>

/* The code sites, each a list of RGB colours (REL offsets of wwrando 1.10.0, USA disc). */
enum PcRandoPaletteSite {
    PCR_PALSITE_TN = 0,   /* Darknut: d_a_tn yoroi_break prim[6] (0-5) then env[6] (6-11), 0xE2AC */
    PCR_PALSITE_MO2,      /* Moblin: d_a_mo2 spear thread (ke_disp 3D line), 0xD648 */
    PCR_PALSITE_BOKO,     /* Moblin: d_a_boko spear thread (daBoko_c::keDraw), 0x4488 */
    PCR_PALSITE_ST,       /* Stalfos: d_a_st hat thread, 0x9F30 */
    PCR_PALSITE_NZ,       /* Rat: d_a_nz tail, 0x8FB8 */
    PCR_PALSITE_NPC_NZ,   /* Rat: d_a_npc_nz tail, 0x48C0 */
    PCR_PALSITE_CC,       /* ChuChu: d_a_cc eff_color_dt[5], 0x7F88 */
    PCR_PALSITE_CC_SPLIT, /* ChuChu: d_a_cc action_damage_move CCSPLIT00 multipliers 40/16/39 (floats
                             0x7E9C, 0x7EBC, 0x7EC0 after misc_rando_features.asm) */
    PCR_PALSITE_BGN,      /* Puppet Ganon: d_a_bgn red ropes (0: 0xF0A8 daBgn2_Draw, 1: 0xF0B0
                             daBgn3_Draw) */
    PCR_PALSITE_BGN3,     /* Puppet Ganon: d_a_bgn3 HIO ke colour m50/m52/m54 (li immediates at 0x2CF,
                             0x2D7, 0x2DF; ke_color in daBgn3_Create) */
    PCR_PALSITE_GND,      /* Ganondorf: d_a_gnd threads, 0x8F1C */
    PCR_PALSITE_COUNT
};

/* d_a_mant's 7 texture slots (pal_d), 5 of which hold an RGB5A3 palette of 16 colours (C4). */
#define PCR_PALSITE_MANT_SLOTS 7

#ifdef __cplusplus
extern "C" {
#endif

/* Shifts *r, *g, *b (the decompiled value of entry `index` of `site`) by the site's group under
   pc_rando(). Returns 1 when the colour was shifted. */
int pc_rando_palette_code_rgb(int site, int index, uint8_t* r, uint8_t* g, uint8_t* b);

/* d_a_mant: records the address of pal_d (its 7 palette pointers, NULL slots allowed) for the smoke.
   Returns 1. */
int pc_rando_palette_mant_register(uint8_t* const* palettes, int count);
/* d_a_mant draw: the palette of texture slot `slot` (orig, 16 RGB5A3 colours, 0x20 bytes) shifted by
   the Darknut group (shift_hardcoded_darknut_colors: decode, hsv_shift_palette, encode), as a cached
   32-byte aligned copy; orig when pc_rando() is 0 or palettes are not randomized. */
uint8_t* pc_rando_palette_mant_tlut(int slot, uint8_t* orig);

#ifdef __cplusplus
}

/* The hunks' form: any colour struct with r, g, b members (GXColor), alpha untouched. */
template <class C> inline C pc_rando_palette_rgb(int site, int index, C c) {
    pc_rando_palette_code_rgb(site, index, &c.r, &c.g, &c.b);
    return c;
}
#endif

#endif /* PC_RANDO_PALETTE_CODE_H */
