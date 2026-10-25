# Randomizer oracle tools (Mac, dev time only)

These tools run **wwrando 1.10.0_9775811** (MIT, read-only checkout in `ref/wwrando`) in dry mode
(no ISO) as the oracle for our C++ seed generator: decision D1 in `docs/RANDOMIZER_PLAN.md` asks for
the same item in the same location as wwrando for the same seed and options. Nothing here is built
into the game or needed by players.

| Tool | Does |
|---|---|
| `setup_oracle.sh` | creates `build/rando-venv` and `build/rando-deps/gclib` |
| `ww_export.py` | golden placement JSON for a seed (or permalink) and options |
| `ww_export_enemies.py` | R7.3c: the same with enemy rando plus the `enemies` key (`_save`'s RNG replayed on stub actors); `--vectors` writes the R7.3c fixture |
| `ww_verify.py` | checks any placement JSON with wwrando's Logic (valid, complete, beatable) |
| `ww_verify_hints.py` | checks our placements' hints (valid; with `--golden` identical to wwrando's) |
| `ww_trace.py` | logs every RNG draw per randomizer, for call-by-call diffs against our generator |
| `make_test_placement.py` | standard placements `identity`, `rotation`, `seed` for runtime tests |
| `cosmetic_check.py` | R7.5a: fairy fountain Pale colours of the `cosmetic` smoke = wwrando on the disc (`--run`) |
| `enemy_check.py` | R7.4a: the 1,275 enemy records of the `enemies` smoke = wwrando's own `_save` on the disc (`--seed S --run`, `--vanilla`) |
| `fp_audit.py` | R7.6b: fingerprint audit: every stage.dzs, room.dzr and event_list.dat of the disc through the whole patcher chain (`fpaudit` smoke), rotation + every option, USA and EUR (`--region`) |
| `identity_check.py` | R9.2: the identity placement is vanilla (`run`: every archive's files vs. the disc, the placement steps = wwrando's `change_*_item` with vanilla items, tweak steps = `fixtures/R9_2_tweak_files.json`; `saves`: `save_sweep.py` both ways) |
| `oracle_common.py` | shared helpers (paths, venv re-exec, options, JSON) |

## Setup

```sh
native/tools/rando/setup_oracle.sh          # once per checkout or worktree (~10 s, needs network)
native/tools/rando/setup_oracle.sh --check  # exit 0 when ready
```

It needs `ref/wwrando` (wwrando at `9775811`; the plan's checkout) and a Python >= 3.11 (it prefers
`python3.12`; set `PYTHON=` to choose). It installs the pinned set ruamel.yaml 0.19.1, numpy,
pillow, imagequant, cffi, pycparser, appdirs and tqdm (no Qt: the UI is not used), and clones
gclib at `d9322523` because the `ref/wwrando/gclib` submodule is empty. Both go under `build/`,
which git ignores. It is idempotent; `--force` rebuilds both. Offline: `GCLIB_SRC=<local gclib
clone>` and `PIP_OPTS='--no-index --find-links <wheels>'`.

The tools are run with `python3 -I`. They put `ref/wwrando` and the gclib checkout on `sys.path`
themselves (`-I` ignores `PYTHONPATH`) and re-execute with `build/rando-venv/bin/python` when the
current interpreter lacks the packages, so any `python3` works once the venv exists. wwrando's own
prints go to stderr, so stdout carries only the tool's output. They refuse a wwrando whose VERSION
is not `1.10.0_9775811` (the permalink embeds it) unless `COS_WWRANDO_ANY_VERSION=1`.

Environment overrides: `COS_WWRANDO_DIR`, `COS_GCLIB_DIR`, `COS_RANDO_VENV`.

Exit codes for all tools: 0 success, 1 failed check or wwrando failure, 2 usage or setup error.

## Options

Every tool that makes a randomizer starts from wwrando's default `Options()` (the "default
preset") and accepts:

- `name=<json>` pairs, e.g. `keylunacy=true num_required_bosses=2 sword_mode=Swordless`
  (a value that is not JSON is taken as a string; StrEnum options take their string value, e.g.
  `mix_entrances="Mix Dungeons & Caves & Fountains"`);
- `--options FILE`: a JSON dict of options, or a placement JSON (its `options` are used). Pairs
  are applied after the file.

Values are converted with wwrando's `Options.validate()`; an unknown option name is an error.

## Placement JSON (schema `format 1`)

```
{format, seed, permalink, options, extra_starting_items, charts, required_boss_locations,
 entrances, starting_island_room, locations{location: item}}
```

- `seed`: wwrando's sanitized seed; `permalink`: wwrando's base64 permalink (version string
  `1.10.0_9775811`).
- `options`: every wwrando option (also non-permalink ones such as `dry_run`), enums as strings.
- `charts`: island number (string) -> chart item name; `entrances`: entrance name -> zone name;
  `required_boss_locations`: the boss item locations sampled by required bosses.
- `locations`: all 320 locations in wwrando's `item_locations.txt` order -> item name. Generated
  seeds use wwrando's logic item names (`Progressive Sword`, `DRC Small Key`, `Rainbow Rupee`...).
  The `identity` test placement uses the vanilla names of `data/item_names.txt` instead (`Hero's
  Sword`, `Small Key`...), which map to the original item IDs exactly. A consumer (R0.6) maps both
  name sets to item IDs.
- Written with `json.dumps(indent=1)` plus a newline, ASCII only.

Our generator writes the same schema, plus `hints` and `version` (R0.6), which wwrando ignores.

## ww_export.py

```sh
python3 -I native/tools/rando/ww_export.py TestSeed1 build/rando/TestSeed1.json
python3 -I native/tools/rando/ww_export.py abc out.json randomize_charts=true required_bosses=true
python3 -I native/tools/rando/ww_export.py --permalink MS4xMC4wXzk3NzU4MTEAVGVzdFNlZWQxAEkQUBgAAPsCyCUAAAAAAAEAQQCAAA== out.json
```

`--logs DIR` also writes wwrando's spoiler and non-spoiler logs (the reference for D5's log format).
`--seed-key KEY` sets wwrando's `SEED_KEY`, which only matters with `do_not_generate_spoiler_log`
(empty in a source checkout; our generator's key is `SWITCHWAKER_SEED_KEY`, D2). `out.json` may be
`-` for stdout. Takes about 2 s per default seed.

## ww_verify.py

```sh
python3 -I native/tools/rando/ww_verify.py placement.json               # full check
python3 -I native/tools/rando/ww_verify.py --validity-only rotation.json # checks 1 and 3 only
```

It rebuilds a dry randomizer with the JSON's options, restores charts, required bosses (through a
stub `sample` so the banned lists are re-derived), entrances and extra starting items, then checks:

1. exactly wwrando's 320 locations, known item names;
2. the pool multiset (progress and nonprogress items exactly; fixed consumables at most their count;
   anything else only if duplicatable);
3. `check_item_valid_in_location` for every pair, with the whole placement in place (so the rock
   spire shop's no-duplicates rule is checked), and no progress item in a location banned by
   required bosses;
4. at least N copies of each progress item in progress-eligible, non-banned locations;
5. a reachability fixpoint reaches `Can Reach and Defeat Ganondorf` and every non-banned progress
   location.

The result is JSON on stdout (`ok`, `beatable`, `sweeps`, `unreachable`, `unreachable_progress`,
`errors`); `--quiet` prints it only when invalid. Entrance structure rules (nesting, safety
entrance) are R6.14's checker.

## ww_trace.py

```sh
python3 -I native/tools/rando/ww_trace.py TestSeed1 --out ts1.trace --summary
```

Trace format 1, one draw per line in call order, after a header:

```
# ww_trace 1 seed=TestSeed1 version=1.10.0_9775811
PigsRandomizer randbelow 3 0
ItemRandomizer randbelow 1 0
HintsRandomizer random 4503599627370496
```

- `<Randomizer> randbelow <n> <result>`: `Random._randbelow(n)`, the draw behind `choice`,
  `shuffle`, `sample` (also `sample(counts=)`, i.e. `weighted_choice`), `randrange` and `randint`.
  A one-element `choice` still logs `randbelow 1 0`.
- `<Randomizer> random <k>`: `Random.random()` as the exact integer `k = x * 2**53` (only
  `choices()` in hints draws floats).
- The owner is the class name of the randomizer whose `reset_rng()` made that RNG (each
  randomizer gets a fresh `get_new_rng()`); the warm-up `getrandbits` calls are not logged.

Our generator writes the same lines with `COS_RANDO_TRACE=1`, so `diff` shows the first diverging
draw. `--owner ItemRandomizer` keeps one randomizer; `--placement FILE` also writes the placement
(identical to `ww_export.py`'s).

## make_test_placement.py

```sh
python3 -I native/tools/rando/make_test_placement.py --kind identity build/rando/identity.json
python3 -I native/tools/rando/make_test_placement.py --kind rotation build/rando/rotation.json
python3 -I native/tools/rando/make_test_placement.py --kind seed --seed TestSeed1 build/rando/ts1.json
```

- `identity`: every location holds its original item, vanilla charts, entrances, starting island
  and no required bosses. A game with it must behave exactly as the vanilla game.
- `rotation`: deterministic. Every location gets an item that `check_item_valid_in_location`
  accepts, never its original item, of another class (progress, nonprogress, consumable, dungeon
  item, chart, trap) wherever the location allows one ("Consumables only" locations do not), and
  unplaced items first, so all 153 logic items appear at default options. Not beatable; check it
  with `ww_verify.py --validity-only`. Options such as `keylunacy=true trap_chests=true` change
  what is valid and are honoured.
- `seed`: the same as `ww_export.py <seed>`.

## Oracle checks of later steps

- R1.6 `check_seeds.py --cli <cos_rando_cli> --n N --preset P [--golden]` runs our CLI, then
  `ww_verify.py` on each seed, and with `--golden` compares `locations` with `ww_export.py`.
- R5.1 `ww_verify_hints.py --cli <cos_rando_cli> --n N [--golden] [name=<json> ...]` (or placement
  files) checks every hint against the placement with wwrando's Logic and, with `--golden`, requires
  the `hints` value to equal wwrando's HintsRandomizer state. `check_seeds.py --hints` generates the
  hints too and compares the spoiler log's Hints section byte for byte.
- On a mismatch, diff `ww_trace.py` with our `COS_RANDO_TRACE=1` output.
- R4.3 `check_gen_run.py [--seed S] [--run-dir DIR]` runs the in-game smoke `gen` (the options menu's
  Generate row with `COS_RANDO_SEED=S`), then checks the sidecar it wrote in the run's card folder and
  its logs in `<run dir>/user/rando` as `check_seeds.py --golden` does: `ww_verify.py`, the
  placement against `ww_export.py S`, and the logs (hints aside). Needs `COS_DISC`.
- R6.14 `entrance_sweep.py` (needs `COS_DISC` for `plan` and `run`):
  - `check <placement.json>... [--no-logic]` is the entrance structure checker: the map is a bijection
    of the 44 entrances and exits, each entrance leads to an exit of its own entrance set
    (`mix_entrances`), every exit's nesting chain ends on an island (no loop), a
    dungeons-and-caves-only start has its safety entrance, a progress exit is never nested in a
    nonprogress one (and Forsaken Fortress's boss entrance follows FF's progress), no island holds a
    required and a banned boss, and `ww_verify.py` passes with the map.
  - `selftest` breaks wwrando seeds in each of those ways and requires `check` to catch every break.
  - `plan <placement.json>` writes the in-game plan from wwrando's tables and the disc's SCLS[0]
    (where each entrance leads, the way back, where a save inside the exit reloads).
  - `run <placement.json>... [--jobs 3] [--per-run 6]` runs the smoke `entrsweep` over the plans:
    every entrance forward, a save-and-quit inside its exit reloaded, and back (SCLS exit, boss
    warp-out or Helmaroc King's door). Report: `build/rando-sweep/entrance_sweep.tsv`. Only for maps
    with entrance rando on (the plan states what wwrando's `_save` writes; an unrandomized disc
    differs from wwrando's tables in a few SCLS rooms and the caves' save sectors).
