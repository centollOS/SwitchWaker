# Randomizer: feasibility study

Question (2026-10-17): can SwitchWaker offer a Wind Waker randomizer while the player still needs only the NRO
and their unmodified disc (the same `.iso` as today)?

Status: **analysis only, parked** (decision 2026-10-17: not started). Kept on this branch for reference.

Short answer: **yes, it is feasible**, as a randomizer built into the port. It is not a matter of loading a
randomized ISO: the randomizer's code changes have to be written into the decompiled code (as the BetterWW
options already were, [QOL_PLAN.md](QOL_PLAN.md)), its data changes applied in memory when the game loads the
files, and its seed generator ported to C++ so the console makes the seeds. It is a large project (several
times the QoL work), which can be split into phases that each leave something playable.

Reference: the Wind Waker Randomizer (wwrando, LagoLunatic, MIT licence), `github.com/LagoLunatic/wwrando`
at `9775811` (2026-07-20). Figures below are from that tree.

## Why a randomized ISO does not work as it is

wwrando writes a new ISO: changed stage, event and text files, plus PowerPC patches to `main.dol` and the RELs.

- SwitchWaker does not run the disc's code: the game is the decompilation, compiled natively. The PowerPC
  patches would be ignored.
- SwitchWaker reads data out of `main.dol` and the RELs at start and checks their SHA-1 against the supported
  disc (`native/src/pc/runtime/pc_assets.cpp`, `pc_disc.cpp`): a randomized ISO's `main.dol` fails the check
  (exit `PC_EXIT_DISC`).
- And it would break the "your disc as it is" condition anyway.

## What wwrando changes

| Part | Size | What it is |
|---|---|---|
| Item locations | 320 locations (`logic/item_locations.txt`) | ~240 are actor/chest/event entries in stage files (`LinkUG/Stage.arc/Chest000`, `.../Event085:.../Action001`); ~83 are item IDs in code (76 in RELs, 7 in `main.dol`); 3 custom |
| Code patches | 6,342 lines of PowerPC assembly, ~600 patch sites, 22 files (`asm/patches/`) | biggest: `custom_funcs` (1,609 lines: new-game state, starting items, story flags), `flexible_item_locations` (177 sites: places whose item cannot change without code changes, e.g. Great Fairies' upgrades, boss heart containers), `make_game_nonlinear` (99), `misc_rando_features` (91), `fix_vanilla_bugs` (60), `remove_cutscenes` (41), `flexible_enemies` (28), `make_items_progressive` (17), `swift_sail` (14) |
| Data tweaks | `tweaks.py`, 2,710 lines, 89 functions (81 called by `randomizer.py`) | stage/event/text edits: added chests and warp pots, removed cutscenes in event lists, item names and articles in messages, starting spawn, hint signs, key bag, softlock fixes |
| Custom actors | 2 C++ sources (`asm/actors/d_a_dungeon_flag_sw.cpp`, `d_a_tbox.cpp`) | compiled to RELs for the GameCube; here they would be compiled natively |
| Seed generator | ~7,000 lines of Python (`logic/logic.py` 1,311, `randomizers/items.py` 718, entrances 1,141, hints 1,215, enemies 1,320, ...) + logic data (`item_locations.txt` 3,214 lines, `macros.txt` 1,164, `enemy_locations.txt` 8,150) | the requirements of each location, the fill that keeps the seed beatable, hints, spoiler log |
| Options | ~90 (`options/wwrando_options.py`) | progression categories, starting gear, sword mode, key-lunacy, entrances, enemies, hints, cosmetics |

Only the North American disc (GZLE01) is supported by wwrando (`randomizer.py` refuses others).

## How it would fit "only the NRO and the disc"

1. **The seed is made on the console.** A new-game option ("New randomized file": seed text, a preset of
   options) runs the generator in C++ and writes the result next to the save (e.g.
   `sdmc:/switch/switchwaker/rando/<slot>.json`: options, seed, placement, spoiler). The logic data files are
   plain text under MIT: they can be compiled into the NRO.
2. **Item placement applied at load.** Stage actors and chests are created in `dStage_actorInit` and
   `dStage_stageTresureInit` / `dStage_roomTresureInit` (`game/src/d/d_stage.cpp`), which walk each chunk with its index: the item bits of the
   parameters can be replaced there from the placement (stage, room, layer, chunk, index), without touching
   the disc. Event-list item actions go through the event code, which `make_items_progressive` already hooks.
   The ~83 locations in code become a lookup in the decompiled function (`pc_rando_item(location, vanilla)`).
3. **Code patches as guarded C++ changes**, as the QoL items: `if (pc_rando()) <new> else <original>`, so the
   game without a randomized file is unchanged ([GAME_DIVERGENCE.md](GAME_DIVERGENCE.md) counts them). In the
   decompilation they are named functions, not addresses, which makes most of the ~600 sites a line or two.
4. **Data tweaks in memory**: messages through a hook on the message lookup (item names, hints); event lists
   and stage chunks patched after the archive is loaded (removed cutscene steps, changed spawns); added
   chests, pots and signs created at room init.

## Already done here

The BetterWW options ([QOL_PLAN.md](QOL_PLAN.md), BetterWW is a wwrando derivative) ported 15 of its features
this way (49 guarded sites in 31 files): instant text, swift/brisk sail, unrestricted boat (= the King of Red
Lions part of `make_game_nonlinear`), Tingle chests, full sea chart, faster roll/grapple/blocks/climb, fast
Ballad, skip intro, several vanilla bug fixes. The method, the guard (`pc_qol`) and the Mac verification
harness are proven.

## Phases

| Phase | Content | Playable result |
|---|---|---|
| 0 | Randomizer state per save slot; placement hook for stage chests/actors and event items; "any item anywhere" (`allow_all_items_to_be_field_items`, item get for any ID); a fixed test placement from a file | a hand-made shuffle works |
| 1 | C++ generator for items only (logic parser, requirements, assumed fill), progression categories as options; on Mac, wwrando's own Python logic checks every generated seed (beatable, every requirement met) as a test oracle | real seeds, vanilla story order |
| 2 | The patches the logic assumes: `make_game_nonlinear`, `make_items_progressive`, `flexible_item_locations`, `misc_rando_features`/`custom_funcs` (starting state and flags), `remove_cutscenes`, key bag, softlock fixes | a randomizer equivalent to wwrando's default preset |
| 3 | Code locations in RELs/`main.dol`, remaining data tweaks, hints, spoiler log on the SD card | complete item randomizer |
| 4 (optional) | Entrances, enemies, charts, sword modes, hero mode, cosmetics; European disc (message IDs and text differ; wwrando does not support it) | parity with wwrando's options |

## Risks and limits

- **Size.** ~600 code sites + ~80 data tweaks + a C++ generator: several times the QoL work. Each phase is
  usable, but phase 2 is needed before seeds are fair.
- **Beatability depends on matching wwrando's assumptions.** A location or patch missed means a softlock in a
  seed. Mitigations: the Python logic as an oracle on Mac; the existing sweeps (`item_sweep.py`,
  `event_sweep.py`) extended to "every location gives its placed item"; seeds tested in full.
- **Seeds are not compatible with the PC randomizer** (a different generator and random source): no shared
  seeds, races or permalinks with wwrando, unless its generator is reproduced exactly (Python's `random`).
- **Keeping up with wwrando**: its changes have to be followed by hand.
- **Saves**: a randomized file needs its placement file; losing it makes the save unplayable as a randomizer
  (keep both together, warn on delete/copy).
- **Licence**: wwrando is MIT (compatible with this repository's MIT); its logic data and actor sources keep
  their notice (THIRD_PARTY.md).

## Recommendation

Start with phases 0 and 1 on a branch, which answer the open technical questions (the
placement hook covers the stage locations; the generator matches wwrando's oracle), before committing to
phase 2. USA disc first.
