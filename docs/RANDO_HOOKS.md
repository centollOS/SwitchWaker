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

## R3.10

Nonlinear B: `make_game_nonlinear.asm` items 12-22 (asm report G6). Smoke `nonlinB`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_mo2.cpp | `daMo2_Create`, `daMo2_Execute` (sword checks, `rouya_mode`) | `\|\|` | make_game_nonlinear.asm d_a_mo2 0xBF2C, 0xAD70 |
| game/src/d/actor/d_a_obj_akabe.cpp | `daObjAkabe::Act_c::chk_appear` | `?:` | make_game_nonlinear.asm d_a_obj_akabe 0x650 |
| game/src/d/actor/d_a_kb.cpp | `pl_attack_hit_check`, `carry_move` | `if/else` (`!pc_rando() &&`), `\|\|` | make_game_nonlinear.asm d_a_kb 0x1460, 0x3C1C |
| game/src/d/d_menu_fmap.cpp | `dMenu_Fmap_c::checkMarkCheck1`, `checkMarkCheck2` | `add` (return) | make_game_nonlinear.asm 0x801B14C4, 0x801B1684 |
| game/src/d/actor/d_a_obj_ajav.cpp | `daObjAjav::Act_c::_create` | `\|\|` | make_game_nonlinear.asm d_a_obj_ajav 0x1568 |
| game/src/d/actor/d_a_obj_auzu.cpp | `daObjAuzu::Act_c::is_exist` | `\|\|` | make_game_nonlinear.asm d_a_obj_auzu 0x580 |
| game/src/d/actor/d_a_npc_co1.cpp | `daNpc_Co1_c::init_CO1_0` | `?:` | make_game_nonlinear.asm d_a_npc_co1 0x498 |
| game/src/d/actor/d_a_obj_eayogn.cpp | `daObjEayogn_c::check_ev_bit` | `?:` | make_game_nonlinear.asm d_a_obj_eayogn 0x40C |
| game/src/d/d_com_inf_game.cpp | `dComIfG_play_c::getLayerNo` (Hyrule) | `add` (return) | make_game_nonlinear.asm 0x80052668 |
| game/src/d/actor/d_a_npc_bms1.cpp | `shop_talk`, `CreateInit` (2), `_create` (2), `CreateHeap`: the six `checkItemGet(0x69, 1)` through a unit-local macro | `?:` | make_game_nonlinear.asm d_a_npc_bms1 0x1B9C, 0x2064, 0x20B4, 0x36A0, 0x36EC, 0x3E3C |
| game/src/d/actor/d_a_npc_de1.cpp | `daNpc_De1_c::getMsg` | `add` (return) | make_game_nonlinear.asm d_a_npc_de1 0xDB0 |
| game/src/d/actor/d_a_npc_bm1.cpp | `daNpc_Bm1_c::init_BMD_1` | `if/else` (`!pc_rando() &&`) | make_game_nonlinear.asm d_a_npc_bm1 0x1150 |
| game/src/d/actor/d_a_npc_os.cpp | `eventOrderCheck`, `finish02NpcAction`, `execute` | `if/else` (`!pc_rando() &&`), `\|\|`, `add` | make_game_nonlinear.asm d_a_npc_os 0x12E8, 0x1F74, 0x6000 |
| game/src/d/actor/d_a_obj_hsehi1.cpp | `daObj_hsh_c::actionDeleteEvent` | `add` | make_game_nonlinear.asm d_a_obj_hsehi1 0x1E60 |
| game/src/d/d_door.cpp | `dDoor_hkyo_c::proc` (west and north doors) | `if/else` (`!pc_rando() &&`) | make_game_nonlinear.asm 0x8006D618, 0x8006D674 |
