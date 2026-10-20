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
