/*
 * pc_rando_tweak_cosmetic.h - the cosmetic tweaks wwrando applies on every seed (docs/RANDOMIZER_PLAN.md
 * step R7.5a; "Decisions taken at the split": they count toward default-preset parity):
 *
 *   make_tingle_statue_reward_rupee_rainbow_colored (tweaks.py; misc_rando_features.asm 0x800F93F4)
 *     item_resource[0xB8].mTevFrm (GameCube +0x14) becomes 7, an originally unused value that marks
 *     the Rainbow Rupee. The table edit is registered by game_hooks/pc_rando_hooks_cosmetic.cpp
 *     (R0.8's pc_rando_table_edit); daItemBase_c::animEntry (d_a_itembase.cpp) asks
 *     pc_rando_cosmetic_brk_frame (pc_rando_hooks.h) for the BRK frame under pc_rando(). For colour 7
 *     that is pc_rando_cosmetic_rainbow_step: a global key frame (f32) that gains 0.15 per draw, is
 *     reset to -6.0 once it is no longer below 6.0, and is passed as fabs(), so the rupee runs through
 *     the BRK's rupee colours 0 -> 6 -> 0 -> 6 ... without a jump. As in wwrando the key frame moves
 *     once per DRAW of any rainbow rupee and is never reset (it starts at 0.0 at boot).
 *
 *   give_fairy_fountains_distinct_colors (tweaks.py)
 *     In Fairy01..Fairy06 Stage.arc stage.dzs, every Pale record's bg0_c0 (+0x06) and bg0_k0 (+0x09)
 *     colour gets the hue k/6 for the k-th stage (k = 0..5, the hue summed as wwrando sums it), keeping
 *     its saturation and value: Python's colorsys.rgb_to_hsv / hsv_to_rgb in double precision, then
 *     int() truncation of each channel * 255. A grey colour (saturation 0) stays as it is. A dzx
 *     patcher (pc_rando_resources.h, step R0.10) applies it while pc_rando() is 1. D9: a file whose
 *     Pale chunk is missing or does not have 0x2C-byte entries is left vanilla and logged.
 *
 * Pure code (no game headers, no game calls): it links into cos_rando_core. The game side calls
 * pc_rando_tweak_cosmetic_init through pc_rando_patchers.def. Game thread only.
 */
#ifndef PC_RANDO_TWEAK_COSMETIC_H
#define PC_RANDO_TWEAK_COSMETIC_H

#include <stddef.h>
#include <stdint.h>

#include "pc_rando_resources.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Rainbow Rupee --------------------------------------------------------------------------- */

/* The Tingle Statue reward rupee (dItemNo_TINGLE_RUPEE_6_e) and its marker colour. */
#define PCR_COSMETIC_RAINBOW_ITEM 0xB8
#define PCR_COSMETIC_RAINBOW_TEV_FRM 7
/* rainbow_rupee_keyframe's constants (misc_rando_features.asm). */
#define PCR_COSMETIC_RAINBOW_STEP 0.15f
#define PCR_COSMETIC_RAINBOW_MAX 6.0f
#define PCR_COSMETIC_RAINBOW_MIN (-6.0f)

/* One draw of a rainbow rupee: advances the global key frame and returns fabs() of it. */
float pc_rando_cosmetic_rainbow_step(void);
/* The global key frame (signed), and a setter for tests and smokes. */
float pc_rando_cosmetic_rainbow_keyframe(void);
void pc_rando_cosmetic_rainbow_set_keyframe(float keyframe);
/* The step on an explicit key frame (what pc_rando_cosmetic_rainbow_step does to the global one):
   returns fabs() of the new value, stored back in *keyframe. */
float pc_rando_cosmetic_rainbow_advance(float* keyframe);
/* Number of rainbow draws since boot (for the smoke). */
uint32_t pc_rando_cosmetic_rainbow_draws(void);

/* --- Fairy fountain colours ------------------------------------------------------------------ */

#define PCR_COSMETIC_FAIRY_STAGES 6
/* 0..5 for "Fairy01".."Fairy06", else -1. */
int pc_rando_cosmetic_fairy_index(const char* stage);
/* The hue wwrando gives the k-th stage (hue += 1.0 / 6 per stage, from 0). */
double pc_rando_cosmetic_fairy_hue(int k);
/* colorsys: rgb (0..255 each) -> the same saturation and value at `hue`, int()-truncated. */
void pc_rando_cosmetic_set_hue(uint8_t rgb[3], double hue);
/* Applies the edit to a raw stage.dzs of stage index k (0..5). Returns the number of Pale records
   changed (0 when there is none), or -1 when the file is refused (D9; nothing is written). */
int pc_rando_cosmetic_fairy_patch_dzs(uint8_t* data, size_t size, int k);

/* The dzx patcher (registered by init; exposed for tests and smokes). */
void pc_rando_tweak_cosmetic_patch(PcRandoResFile* file);
/* Registers the patcher. Idempotent. */
void pc_rando_tweak_cosmetic_init(void);

/* What the patcher did per Fairy stage since boot (or the last reset). */
typedef struct PcRandoCosmeticStatus {
    uint32_t seen;    /* Fairy0N stage.dzs files dispatched */
    uint32_t patched; /* of those, files changed */
    int last;         /* the last result: Pale records changed, -1 refused, -2 before the first file */
} PcRandoCosmeticStatus;
void pc_rando_tweak_cosmetic_status(int k, PcRandoCosmeticStatus* out);
void pc_rando_tweak_cosmetic_reset_status(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_TWEAK_COSMETIC_H */
