# Randomizer hook log

Every hunk the built-in randomizer adds to `game/` (docs/RANDOMIZER_PLAN.md, "Conventions every step
follows"). A randomizer step appends its own section `## R<phase>.<n>` here, in plan order, in the same
commit as its hunks; on a merge keep both sides.

Rules for a hunk (checked by `python3 -I native/tools/rando_hook_census.py`, which `native/tools/regress.sh`
runs):

- It is on the host side of a `#if TARGET_PC` guard, with the original code in the `#else` where the
  hunk replaces something, and takes the original branch when `pc_rando()` is 0. Guard forms:
  `?:` (`pc_rando() ? new : original`), `||` (`pc_rando() || original`), `if/else`
  (`if (pc_rando()) {new} else {original}`), `add` (new code under `if (pc_rando())` with nothing
  replaced), `include` (`#include "pc/pc_rando.h"`; needs no row).
- The guarded branch holds a marker comment
  `// pc_rando(<step>): <wwrando ref, e.g. flexible_item_locations.asm d_a_bigelf 0x217C>; original: <value>`.
- Each file with a marker of a step has a row in that step's section. One row per function; the
  census matches the first cell (the file path from the repository root, an optional `:line` is
  ignored) and lists stale rows. A step without hunks in `game/` writes a single row whose first cell
  is `-`.
- The event bits wwrando reuses (0x6901-0x6980, 0x6A01-0x6A40; plan D17) appear only in these hunks
  and in the randomizer's own files.

Columns: file, function, guard form, wwrando reference.

## R0.1

| File | Function | Guard | wwrando |
|---|---|---|---|
| - | no hunks in `game/`: the switch (`native/include/pc/pc_rando.h`), the `rando` smoke and the test plumbing | - | - |

## R3.12

Vanilla bug fixes (`fix_vanilla_bugs.asm`) and the enemy death switches (`flexible_enemies.asm`).
The 11 fixes QoL item 15 already had are forced on under rando (`pc_rando() ||` before
`pc_qol(PC_QOL_FIX_BUGS)`; the hookshot sight and Orca fixes were always on and need no hunk). The 8
newer fixes are under `pc_rando() || pc_qol(PC_QOL_FIX_BUGS)` too, so vanilla players get them with
QoL item 15 (off by default). The `flexible_enemies.asm` parts are rando only. Guard `||+qol` below
means `pc_rando() || pc_qol(PC_QOL_FIX_BUGS)`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_item.cpp | `item_func_camera2` | `\|\|+qol` | fix_vanilla_bugs.asm deluxe_picto_box_item_func_fix_equipped_picto_box (main.dol 0x800C3420) |
| game/src/d/actor/d_a_pt.cpp | `daPt_Create` | `\|\|+qol` | fix_vanilla_bugs.asm miniblin_set_death_switch_when_light_arrowed (d_a_pt 0x4B44) |
| game/src/d/actor/d_a_pw.cpp | `Big_pow_down_check` | `\|\|+qol` | fix_vanilla_bugs.asm poe_fix_light_arrows_bug (d_a_pw 0x8CC/0x900) |
| game/src/d/actor/d_a_npc_bs1.cpp | `daNpc_Bs1_c::getMsg` | `\|\|+qol` | fix_vanilla_bugs.asm d_a_npc_bs1 0x214C |
| game/src/d/actor/d_a_st.cpp | `daSt_Execute` (lower body, 0x85CC: `\|\|+qol`; fall death, 0x845C) | `\|\|+qol`, `if/else` | fix_vanilla_bugs.asm stalfos_kill_lower_body_when_upper_body_light_arrowed; flexible_enemies.asm stalfos_set_death_switch (d_a_st 0x845C) |
| game/src/d/actor/d_a_npc_ba1.cpp | `daNpc_Ba1_c::next_msgStatus` | `\|\|+qol` | fix_vanilla_bugs.asm d_a_npc_ba1 0x16DC |
| game/src/d/actor/d_a_fganon.cpp | `start` | `\|\|+qol` | fix_vanilla_bugs.asm phantom_ganon_check_link_within_y_diff (d_a_fganon 0x4F50) |
| game/src/d/actor/d_a_mt.cpp | `damage_check` | `\|\|+qol` | fix_vanilla_bugs.asm magtail_respawn_when_head_light_arrowed (d_a_mt 0x6000) |
| game/src/d/actor/d_a_ks.cpp | `naraku_check` | `\|\|+qol` | fix_vanilla_bugs.asm d_a_ks 0x678 |
| game/src/d/d_s_play.cpp | `dScnPly_Delete` | `add` (`\|\|+qol`) | fix_vanilla_bugs.asm stop_sub_bgm_when_unloading_stage (main.dol 0x80235340) |
| game/src/d/actor/d_a_arrow.cpp | `daArrow_c::procMove` (callback), `daArrow_c::ShieldReflect` (phase 3) | `add`, `\|\|+qol` | fix_vanilla_bugs.asm zero_out_arrow_on_hit_callback (main.dol 0x800D6194), check_ganondorf_in_phase_3 (main.dol 0x800D4EBC) |
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::processTime` | `?:` (`\|\|+qol`) | fix_vanilla_bugs.asm main.dol 0x802ABEF8 |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::checkNextActionFromButton`, `daPy_lk_c::setShieldGuard` | `\|\|+qol` | fix_vanilla_bugs.asm check_can_defend (main.dol 0x8010E288, 0x8010E504) |
| game/src/d/actor/d_a_bdk.cpp | `landing` | `add` (`\|\|+qol`) | fix_vanilla_bugs.asm initialize_helmaroc_king_landing_timer, check_helmaroc_king_landing_timeout (d_a_bdk 0x24D4, 0x2AD0) |
| game/src/d/actor/d_a_boko.cpp | `daBoko_c::procCarry` | `if/else` (`\|\|+qol`) | fix_vanilla_bugs.asm d_a_boko 0x26A8 |
| game/src/d/actor/d_a_npc_people.cpp | `daNpcPeople_c::isUo1FdaiAll` | `if/else` (`\|\|+qol`) | fix_vanilla_bugs.asm sam_decorate_town_require_pedestals_to_exist (d_a_npc_people 0x9260) |
| game/src/d/actor/d_a_mo2.cpp | `daMo2_Execute` | `add` | flexible_enemies.asm moblin_set_death_switch (d_a_mo2 0xAB28) |
| game/src/d/actor/d_a_tn.cpp | `daTn_Execute` | `add` | flexible_enemies.asm darknut_set_death_switch (d_a_tn 0xB17C) |
| game/src/d/actor/d_a_rd.cpp | `daRd_c::_create`, `daRd_c::modeDeath` | `add` | flexible_enemies.asm redead_check_disable_spawn_switch, redead_set_death_switch (d_a_rd 0x4874, 0x1FBC) |
| game/src/d/actor/d_a_ph.cpp | `daPH_Create`, `dead_item`, `daPH_Execute`, `daPH_Draw` | `add` | flexible_enemies.asm peahat_check_disable_spawn_switch, peahat_set_death_switch, peahat_check_enable_spawn_switch(_for_draw) (d_a_ph 0x68B0, 0x4118, 0x5B2C, 0x358) |
| game/src/d/actor/d_a_nzg.cpp | `daNZG_Create` (group), `nzg_00_move` | `add`, `if/else` | flexible_enemies.asm g_profile_NZG 0xD08, rat_hole_all_rats_dead (d_a_nzg 0x1FC) |
