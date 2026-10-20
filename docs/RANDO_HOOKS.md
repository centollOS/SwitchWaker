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

## R3.11

Cutscene removal (wwrando `remove_cutscenes.asm`, plus the Killer Bees' Picto Box hint and the
item-get music of `flexible_item_locations.asm`). Hook bodies: `native/src/pc/game_hooks/pc_rando_hooks_cutscenes.cpp`.
Smoke: `COS_RANDO_SMOKE=cutscenes`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_warpf.cpp | `daWarpf_c::checkEndDemo` | add (`return TRUE`) | remove_cutscenes.asm d_a_warpf 0xC3C |
| game/src/d/actor/d_a_obj_doguu.cpp | `daObjDoguu_c::_execute` (case 9) | if/else | remove_cutscenes.asm d_a_obj_doguu 0x267C |
| game/src/d/actor/d_a_warpdm20.cpp | `daWarpdm20_c::CreateInit`, `normal_execute` | `?:` (`pc_rando_hyrule_warp_unlocked`) | remove_cutscenes.asm d_a_warpdm20 0x634, 0xB50 |
| game/src/d/actor/d_a_obj_toripost.cpp | `daObjTpost_c::createInit` (Orca's letter) | `?:` | remove_cutscenes.asm d_a_obj_toripost 0x1B0C/0x1B10 |
| game/src/d/d_kankyo_dayproc.inc | `dKankyo_DayProc` (Aryll's and Tingle's letters) | `?:` | remove_cutscenes.asm 0x80197ADC/0x80197AFC |
| game/src/d/d_item.cpp | `item_func_pearl1/2/3` | add (`pc_rando_place_pearl`) | remove_cutscenes.asm 0x800C43F4/4424/4454 |
| game/src/d/actor/d_a_npc_cb1.cpp | `daNpc_Cb1_c::init` | `\|\|` | remove_cutscenes.asm d_a_npc_cb1 0x80B8 |
| game/src/d/actor/d_a_bgn.cpp | `daBgn_Execute` | `?:` (layer -1) | remove_cutscenes.asm d_a_bgn 0xB1E0 |
| game/src/d/actor/d_a_npc_tt.cpp | `daNpc_Tt_c::demoInitPatten` | `?:` | remove_cutscenes.asm d_a_npc_tt 0xC68 |
| game/src/d/actor/d_a_npc_mk.cpp | `daNpc_Mk_c::visitTalkInit`, `visitSetEvent` (0x1208), `visit01` | `?:`, `!pc_rando() &&` | flexible_item_locations.asm d_a_npc_mk 0x2C2C/0x2D0C; remove_cutscenes.asm 0x2F80 |
| game/src/d/actor/d_a_npc_uk.cpp | `daNpc_Uk_c::visitTalkInit`, `visitSetEvent` (0x1208) | `?:` | flexible_item_locations.asm d_a_npc_uk 0x29B4/0x2AC8 |
| game/src/d/actor/d_a_npc_ho.cpp | `daNpc_Ho_c::getMsg`, `wait01` (0x1F80) | `?:` | remove_cutscenes.asm d_a_npc_ho 0xDB4/0x15FC |
| game/src/d/actor/d_a_player_dproc.inc | `daPy_lk_c::setGetItemSound` | `?:` (`pc_rando_item_get_bgm`) | remove_cutscenes.asm 0x8012E3A4; flexible_item_locations.asm 0x8012E3E8 |
| game/src/d/actor/d_a_player_tact.inc | `daPy_lk_c::procTactPlay` (QoL song replay) | `\|\|` | remove_cutscenes.asm 0x8014ECE0/0x8014EF28 |
| game/src/d/actor/d_a_ship.cpp | `daShip_c::procTactWarp_init`, `procTactWarp` (QoL Ballad) | `\|\|` | remove_cutscenes.asm d_a_ship 0x7680/0x7A10 |
| game/src/d/d_s_name.cpp | `dScnName_c::changeGameScene` (QoL intro) | `\|\|` | remove_cutscenes.asm 0x80232C78/0x80232C88 |
