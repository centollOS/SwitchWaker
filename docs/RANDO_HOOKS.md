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

## R6.15

Small modes (`native/include/pc/pc_rando_modes.h`). Hero mode is on while `pc_rando_cfg()->damage_multiplier`
is not 1.0; `remove_music` and `invert_sea_compass_x_axis` are `PcRandoModes` (`pc_rando_modes_set`).

| File | Function | Guard | wwrando |
|---|---|---|---|
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::setDamagePoint` | `?:` | `hero_mode.asm` 0x801102D0 `multiply_damage_amount` |
| game/src/d/actor/d_a_player_main.cpp | `daPy_lk_c::checkRestHPAnime` | `add` | `remove_low_health_beep_anim.asm` 0x80111EE0 `remove_low_health_anim_at_full_health` |
| game/src/d/d_item.cpp | `getItemNoByLife` | `?:` | `hero_mode.asm` 0x800C7D4C nop |
| game/src/d/actor/d_a_disappear.cpp | `daDisappear_Execute` | `add` (+ `#else` original) | `hero_mode.asm` 0x8038B0C0 `ki_item_d` |
| game/src/d/actor/d_a_ki.cpp | `ki_fail_move` | `?:` | `hero_mode.asm` d_a_ki.rel 0x326C |
| game/src/d/actor/d_a_bl.cpp | `action_normal_skull` | `?:` | `hero_mode.asm` d_a_bl.rel 0x50C4 |
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::bgmStreamPlay`, `bgmStart`, `subBgmStart`, `bgmNowBattle` | `add` (return at entry) | `remove_music.asm` 0x802A33D0, 0x802A34A4, 0x802A47B8, 0x802A4EB8 |
| game/src/JAZelAudio/JAIZelBasic.cpp | `JAIZelBasic::processHeartGaugeSound` | `add` (skips the 1-heart `seStart`) | `remove_low_health_beep_anim.asm` 0x802A2FA0 |
| game/src/d/d_meter.cpp | `dMeter_compassGetOnProc` | `?:` | `invert_sea_compass_x_axis.asm` 0x801FD37C |
| game/src/d/d_save.cpp | `dSv_player_config_c::init` | `?:` | `custom_funcs.asm` `option_targeting_mode` (`set_default_targeting_mode_to_switch`) |
