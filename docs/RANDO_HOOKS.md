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

## R3.9

Nonlinear A: `make_game_nonlinear.asm` items 1-11 (asm report G6). REL offsets are file offsets
(`.text` starts at 0xDC-0xF4 in these RELs); each was checked against the disc's code.

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_ship.cpp | `daShip_c::checkOutRange` | `\|\|` (with `pc_qol(PC_QOL_FREE_BOAT)`) | d_a_ship 0x29EC/0x2A08/0x2A24 |
| game/src/d/actor/d_a_ship.cpp | `daShip_c::execute` | `?:` (boarding: `isGetItem(1, 0)` for event 0x0908), `\|\|` (0xB2D8 with the QoL) | d_a_ship 0xB29C, 0xB2D8 |
| game/src/d/actor/d_a_npc_so.cpp | `daNpc_So_c::_create` | `\|\|` (`!pc_rando() &&`) | d_a_npc_so 0x3FD8 |
| game/src/d/actor/d_a_npc_md.cpp | `daNpc_Md_c::create` | `\|\|` (sword check), `add` (`pc_rando_reset_partner_position(2)`, then setRestartOption always) | d_a_npc_md 0xA24, 0xD94/0xDB4 `reset_medli_position` |
| game/src/d/actor/d_a_npc_cb1.cpp | `daNpc_Cb1_c::create` | `\|\|` (sword check), `add` (`pc_rando_reset_partner_position(1)`, then setRestartOption always) | d_a_npc_cb1 0x640, 0x7B4/0x7D4 `reset_makar_position` |
| game/src/d/d_menu_dmap.cpp | `dMenu_Dmap_c::treasureSet` | `\|\|` | 0x801A9A6C/0x801A9AA8 |
| game/src/d/actor/d_a_obj_ftree.cpp | `daObjFtree::Act_c::set_first_stat` | `\|\|` | d_a_obj_ftree 0xA4C |
| game/src/d/actor/d_a_npc_bj1.cpp | `init_BJ4_0`, `init_BJ6_0`, `init_BJX_0`, `init_BJX_1`, `getMsg_BJ7_0` | `\|\|` (Farore's Pearl counts as owned) | d_a_npc_bj1 0x784/0x830/0x984/0xA30/0x2200 |
| game/src/d/actor/d_a_warpdm20.cpp | `daWarpdm20_c::CreateInit` | `\|\|` (`!pc_rando() &&`: no FF3 bit 0x1820) | d_a_warpdm20 0x68C |
| game/src/d/d_item.cpp | `check_itemno` | `\|\|` (bait, spoils) | 0x800C7E58/0x800C7E84 |
| game/src/d/actor/d_a_obj_mknjd.cpp | `daObjMknjD::Act_c::Create`, `Execute` | `\|\|` (song counts as owned) | d_a_obj_mknjd 0x96C/0x205C/0x20D4 |
| game/src/d/d_com_inf_game.cpp | `dComIfGs_copy/set/rev/exchangePlayerRecollectionData` | `add` (return at once) | 0x80054CC0/0x80054E9C/0x80055318/0x80055580 |
| game/src/d/actor/d_a_tag_ret.cpp | `daTagRet::Act_c::_create` | `\|\|` (`!pc_rando() &&`) | d_a_tag_ret 0x22C |
| game/src/d/actor/d_a_obj_toripost.cpp | `daObjTpost_c::_execute` | `\|\|` | d_a_obj_toripost 0x187C |
