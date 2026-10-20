/*
 * pc_rando_hooks.h - the randomizer's hook bodies that game/ calls (docs/RANDOMIZER_PLAN.md,
 * "Conventions every step follows").
 *
 * A hunk in game/ that needs more than a few lines calls a function declared here; its body lives in
 * native/src/pc/game_hooks/pc_rando_hooks_<area>.cpp. Every hook is only called under pc_rando()
 * (pc_rando.h), from a hunk marked "// pc_rando(<step>): ..." and listed in docs/RANDO_HOOKS.md.
 *
 * Append-only registry: each step adds its own section below, headed "R<phase>.<n>", in plan
 * order; on a merge keep both sides. Game thread only.
 */
#ifndef PC_RANDO_HOOKS_H
#define PC_RANDO_HOOKS_H

#include "pc/pc_rando.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- R0.1: no hooks (the switch itself is pc_rando.h) --- */

/* --- R3.9: nonlinear A (make_game_nonlinear.asm) --- */

/* reset_makar_position / reset_medli_position: when Makar (option 1, d_a_npc_cb1) or Medli (option 2,
   d_a_npc_md) is created in his/her temple, the saved partner position (dSv_player_priest_c, flag
   kept) is set to the spawn near the entry the player came from (dComIfGp_getStartStagePoint, 7
   entries per temple); for any other entry it is set to the player's position, rotation and room.
   The caller then passes it to dComIfGs_setRestartOption as the original code does. */
void pc_rando_reset_partner_position(int option);

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_HOOKS_H */
