/*
 * pc_rando_player_colors.h - the hooks of the player's custom colours (docs/RANDOMIZER_PLAN.md step
 * R7.5b; pure part features/rando/pc_rando_player_colors.h, bodies
 * game_hooks/pc_rando_hooks_player_colors.cpp, hunks in docs/RANDO_HOOKS.md "R7.5b").
 *
 * The colours come from the active seed's custom_colors option (wwrando's cosmetic option; not in
 * the permalink), or from COS_RANDO_CUSTOM_COLORS=<JSON object> when that is set (dev and smokes),
 * and are read at each activation. With none chosen ({}) every hook leaves the vanilla value and
 * bytes alone. Each hook is called only under pc_rando(). Game thread only.
 */
#ifndef PC_RANDO_PLAYER_COLORS_HOOKS_H
#define PC_RANDO_PLAYER_COLORS_HOOKS_H

#include <dolphin/gx/GXStruct.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The trails (PcRandoTrail in features/rando/pc_rando_player_colors.h). */
#define PCR_HOOK_TRAIL_SWORD_SLASH 0 /* daPy_swBlur_c::draw n_color0 (main.dol 0x803F62AC) */
#define PCR_HOOK_TRAIL_ELIXIR_SOUP 1 /* y_color0 (0x803F62B0) */
#define PCR_HOOK_TRAIL_PARRYING 2    /* g_color0 (0x803F62B4) */
#define PCR_HOOK_TRAIL_BOOMERANG 3   /* daBoomerang_blur_c::draw color0 (0x803F6268) */

/* The colour a trail is drawn with: the chosen one, else `vanilla`. Counted per trail. */
GXColor pc_rando_player_trail_color(int trail, GXColor vanilla);

/* daPy_lk_c::setSwordModel, right after JKRReadIdxResource read LkAnm.arc cutfh.brk / cutfm.brk
   (the forward jab trail's colour animation) into `buffer` (cap bytes), before
   J3DAnmLoaderDataBase::load: the chosen sword trail colours go into its key frames 0-2. */
void pc_rando_player_colors_cutf_brk(void* buffer, uint32_t cap);

/* daPy_lk_c::createHeap, right after Link.arc cl.bdl's model data is set up (`modelData`, a
   J3DModelData*): the chosen pupil colour goes into its "hitomi" texture (once per activation; the
   disc's bytes are put back when pc_rando() turns 0). */
void pc_rando_player_colors_cl_model(void* modelData);

/* For the colors smoke. */
typedef struct PcRandoPlayerColorsStatus {
    int active;             /* a configuration was read at the last activation */
    int any;                /* it chooses something */
    uint32_t trailCalls[4]; /* pc_rando_player_trail_color calls per trail */
    GXColor trailLast[4];   /* the last colour returned per trail */
    uint32_t brkCalls;      /* pc_rando_player_colors_cutf_brk calls */
    int brkWritten;         /* key frames written by the last call (-1 refused) */
    int brkChangedBytes;    /* bytes the last call changed */
    uint8_t brkLast[0x400]; /* the last buffer after the edit (setSwordModel reads 0x400 bytes) */
    uint32_t brkLastSize;   /* the J3D file size of that buffer */
    uint32_t hitomiApplied; /* textures recoloured in this activation */
    int hitomiResult;       /* PcRandoTexResult of the last recolour (-1: none tried) */
    uint32_t hitomiRestored;/* textures put back at deactivations since boot */
    int jpcRegistered;      /* the common.jpc editor was registered in this activation */
    int jpcResult;          /* pc_rando_player_colors_jpc's last result (-2: never ran) */
} PcRandoPlayerColorsStatus;
const PcRandoPlayerColorsStatus* pc_rando_player_colors_status(void);

#ifdef __cplusplus
}
/* The configuration the hooks apply (read now when it is due). */
struct PcRandoPlayerColors;
const PcRandoPlayerColors* pc_rando_player_colors_config(void);
#endif

#endif /* PC_RANDO_PLAYER_COLORS_HOOKS_H */
