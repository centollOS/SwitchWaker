# Randomizer tests

The test plumbing of the built-in randomizer (docs/RANDOMIZER_PLAN.md, step R0.1 and "Tests,
conflict-free for parallel work"). Every piece is one file per step, so steps that run in parallel
never edit the same file; the only shared files are append-only lists (marked below).

## Unit tests: `native/check/rando/test_<step>.cpp`

- One file per step, named after the step with `_` for `.`: `test_R0_1.cpp`, `test_R1_2.cpp`.
- CMake globs them (`native/cmake/executable.cmake`): each becomes the executable
  `build/native-mac/cos_rando_test_<step>`; the target `cos_rando_tests` builds them all. A new file
  needs a re-run of CMake's glob, which `ninja` does on its own (`CONFIGURE_DEPENDS`).
- They link `cos_rando_core` only: a separate compile of `native/src/pc/features/rando/pc_rando*.cpp`
  with the game's flags and headers (C++20, no exceptions, no RTTI), and nothing else - no game code,
  no SDK, not `cos_pc`. The link is strict, so a test can only use code that needs no game symbol: the
  generator (`pc_rando_gen_*`, `pc_rando_logic_*`, `pc_rando_rng*`, `pc_rando_options*`), the data
  tables, the JSON and seed-file code, the dzx and event-list libraries, and `pc_rando.cpp`. Keep game
  calls out of those files (game-side work goes in `game_hooks/pc_rando_hooks_<area>.cpp` or a
  listener).
- `rando_test.h` (header only): `RT_CHECK(cond)`, `RT_CHECK_EQ(a, b)` (prints both values),
  `rt::readFixture(name, out)`, `rt::fixturePath(name)` and `rt::finish()`, which prints `ok` and
  returns 0 when every check held. `main` returns `rt::finish()`.
- Run one: `ninja -C build/native-mac cos_rando_test_R0_1 && build/native-mac/cos_rando_test_R0_1`.
  `native/tools/regress.sh` runs every one; a test passes when it exits 0 and its last line is `ok`.

## Fixtures: `native/check/rando/fixtures/`

Produced by the oracle (`native/tools/rando/`, step R0.4) from wwrando's MIT data, never from disc
bytes. Name them after the step (`R0_2_rng_vectors.txt`, ...). Tests find them through
`rt::fixturePath` (the source tree's folder, or `COS_RANDO_FIXTURES` at run time).

## In-game smokes: `COS_SMOKE=rando`

One smoke target `rando`, which picks a function by `COS_RANDO_SMOKE=<name>`:

    native/tools/run.sh rando --stage sea:44 --env COS_RANDO_SMOKE=null

- `native/src/pc/harness/smokes/pc_rando_smokes.def` (**append-only**): one
  `PC_RANDO_SMOKE(<name>, <function>)` line per step.
- `native/src/pc/harness/smokes/pc_rando_smoke_<name>.cpp`: the step's `pc::<function>(unsigned int
  frames)`, called every game frame from the first; helpers in `pc_rando_smokes.h`
  (`randoSmokeInPlay(settle)`, `randoSmokeCheck(smoke, what, ok)`, `randoSmokeEnd(smoke)`: exit 0 when
  every check held, else 1). See `pc_rando_smoke_null.cpp` for a complete one.
- `COS_RANDO_FORCE=1` turns `pc_rando()` on with the default config before slots exist
  (`native/include/pc/pc_rando.h`).
- An unknown or missing `COS_RANDO_SMOKE` exits 2 and lists the names.

## Regression lines: `native/check/regress_rando/<step>.txt`

Same format as `native/check/regress_targets.txt` (`<target> <expected exit> [run.sh options]`, `#`
comments). `native/tools/regress.sh` runs them after `regress_targets.txt`;
`regress.sh --list` prints every line and test it would run, and `regress.sh --rando-only [--no-build]`
runs only the randomizer's part (unit tests, the hook census, the `regress_rando` lines).

## Hook census

`python3 -I native/tools/rando_hook_census.py` checks the hunks in `game/` against
`docs/RANDO_HOOKS.md` and the event bits of plan D17 (see its docstring; `--self-test` checks the
checker). `regress.sh` runs it.
