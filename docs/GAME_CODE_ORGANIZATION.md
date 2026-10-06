# Organising the decompiled game code

Decided 2026-10-06. `game/` (the decompilation) stays structurally identical to the upstream
decompilation, so that upstream fixes and newly matched units keep landing as plain diffs. Readability
comes from upstream's matching work, not from reorganising `game/` ourselves. Our own code lives in
`native/`, where we organise freely.

State at the decision (since the import ce41ca9): 542 of ~2029 game files touched,
+10898/-1201 lines, 1213 `#if TARGET_PC` blocks.

## Rules for `game/`

- No renames, moves, reformatting or restructuring of decompiled code.
- A host change is the smallest guarded hunk that works: `#if TARGET_PC` (or `#ifdef __MWERKS__`
  around a fakematch that only makes sense for the original compiler).
- A host change of more than a few lines goes into a function in `native/src/pc/game_hooks/`
  (declared in `native/include/pc/game_hooks.h`) and `game/` keeps a one-line call.
- Every hunk can be traced: the divergence census (below) lists it with the commit that made it.

## Steps

| # | Step | Output |
|---|------|--------|
| G1 | Divergence census | `native/tools/divergence_census.py` -> `docs/GAME_DIVERGENCE.md`: every game file that differs from the import, its PC blocks and lines, the commits/bugs behind them, and whether zeldaret/tww upstream has the unit fully matched |
| G2 | Reorganise `native/src/pc` | subdirectories by role (runtime, platform glue, features, test harness: smokes and sweeps); no behaviour change |
| G3 | Hoist large PC blocks | the biggest `TARGET_PC` blocks in `game/` move into `native/src/pc/game_hooks/`; `game/` keeps one-line calls |
| G4 | Converge on zeldaret/tww | units zeldaret has matched and whose headers are compatible are taken from zeldaret (better names, verified logic), our PC hunks re-applied; in batches, full regression per batch |

Each step: its own lane, one commit per verified sub-step, full Mac regression
(`native/tools/regress.sh`, never `--no-build`) before integrating, Switch build checked for G2/G3.

## G2 notes (2026-10-06)

`native/src/pc` is split into `runtime/`, `features/` and `harness/` (with `milestones/`,
`boot_tests/`, `smokes/` and `sweeps/`); `native/src/pc/README.md` lists every file. File names
are kept, so the many comments and logs that name a `pc_*.cpp` still find it.

The harness stays compiled into every build, the release and the Switch build included: the tests
are chosen at run time (`COS_SMOKE`, `COS_MILESTONE`, `COS_INPUT`...) and the game calls harness
hooks under `TARGET_PC`. Compiling it out of a release would need a stub for each hook and moving
the runtime pieces that live in harness files today (the game frame counter in `pc_milestone.cpp`,
the allocation-failure reporter installed by `pc_heaps_created` in `pc_heap.cpp`, the debug stage
boot the logo scene asks for). Not planned; noted here in case binary size or start-up ever needs it.
