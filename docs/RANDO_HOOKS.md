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

## R0.8

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/d_item.cpp | `execItemGet`: `pc_rando_exec_item_get` (override registry `pc_rando_item_override`, else `item_func_ptr`) | add | tweaks.py `make_items_progressive`, `allow_dungeon_items_to_appear_anywhere`, `update_tingle_statue_item_get_funcs` (`item_func_ptr` writes) |
| game/src/d/d_item.cpp | `checkItemGet`: `pc_rando_check_item_get` (override registry `pc_rando_item_getcheck_override`, else `item_getcheck_func_ptr`) | add | `item_getcheck_func_ptr` counterpart of the same redirects |

## R0.12

The message overlay (`native/src/pc/features/rando/pc_rando_msg.h`, plan D10). Hook bodies:
`pc_rando_msg_hook_text` and `pc_rando_msg_hook_entry` (`native/include/pc/pc_rando_hooks.h`), in the
pure `pc_rando_msg.cpp`.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/f_op/f_op_msg_mng.cpp:1509 | `fopMsgM_msgGet_c::getMesgEntry` (1509), `fopMsgM_itemMsgGet_c::getMesgEntry` (1601): the overlay's INF1 entry fields (or the new-message defaults) replace the BMG entry before the instant-text QoL | add | gclib `bmg.py` `add_new_message` / `Message` fields, set in `tweaks.py` and `randomizers/hints.py` |
| game/src/f_op/f_op_msg_mng.cpp:1530 | `fopMsgM_msgGet_c::getMessage` (1530), `fopMsgM_itemMsgGet_c::getMessage` (1621): the overlay's encoded text replaces the BMG text (and serves numbers the BMG lacks; `mResMsgNo` = the asked number) | add | `msg.string = ...` / `add_new_message` (`tweaks.py`, `randomizers/hints.py`, `randomizers/boss_reqs.py`) |

## R3.7

The custom actors SwOp (0x1F6) and DngSw (0x1F7). New port-only units and registrations, each inside
`#if TARGET_PC` and unconditional: vanilla stage data never names `SwOp` or `DngSw`, so they are only
created from randomizer data, and a game that is not randomized never creates one. SwOp is written
from the disassembly of wwrando's binary REL (`ref/wwrando/asm/d_a_switch_op.rel`, read with capstone
and the GZLE01 symbol map) and keeps its quirks. The profile list entries are in
`native/include/pc/game_hooks/f_pc_profile_lst.{h,inc}` (port code, no marker).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_switch_op.cpp | new unit: `daSwitchOp_c`, `g_profile_SWITCH_OP` | add (whole file under `#if TARGET_PC`) | `asm/d_a_switch_op.rel` (binary; tweaks.py `add_custom_actor_rels`) |
| game/src/d/actor/d_a_dungeon_flag_sw.cpp | new unit: `daDungeonFlagSw_c`, `g_profile_DUNGEON_FLAG_SW` | add (whole file under `#if TARGET_PC`) | `asm/actors/d_a_dungeon_flag_sw.cpp` |
| game/include/f_pc/f_pc_name.h | `fpcNm_SWITCH_OP_e`, `fpcNm_DUNGEON_FLAG_SW_e` after `fpcNm_GAMEOVER_e` | add | `asm/patches/custom_data.asm` `custom_DynamicNameTable` |
| game/src/d/d_stage.cpp | `l_objectName`: `SwOp`, `DngSw` before `EndCode` | add | `asm/patches/custom_data.asm` `custom_l_objectName` |

## R3.8

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/include/d/actor/d_a_tbox.h | `daTbox_c::getIsTrap` (new inline: param 0x40 while pc_rando(); unused until R6.5) | add | asm/actors/d_a_tbox.cpp `getIsTrap` |
| game/src/d/actor/d_a_tbox.cpp | `commonShapeSet` (open bck; stands for `l_modelInfo[1..3].openBckId` 0x9) | `?:` | asm/actors/d_a_tbox.cpp `l_modelInfo` |
| game/src/d/actor/d_a_tbox.cpp | `getFuncType` (mask 0x3F) | `?:` | asm/actors/d_a_tbox.cpp `getFuncType`/`getIsTrap` |
| game/src/d/actor/d_a_tbox.cpp | `actionOpenWait` (no big chest BGM/lights; event `DEFAULT_TREASURE_A` for every chest) | `if/else`, `\|\|` | asm/actors/d_a_tbox.cpp `actionOpenWait` |
