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

#ifdef __cplusplus
}
#endif

#endif /* PC_RANDO_HOOKS_H */
