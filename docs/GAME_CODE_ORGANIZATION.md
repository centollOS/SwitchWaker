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

## G3 notes (2026-10-06)

The largest host-only blocks of `game/` moved to `native/src/pc/game_hooks/` (functions, declared
in `native/include/pc/game_hooks.h`) and `native/include/pc/game_hooks/` (headers `game/`
includes in place of a block: declarations, the PC profile list, per-vertex code that must stay
inline). `native/src/pc/README.md` lists what went where. Census (`docs/GAME_DIVERGENCE.md`) before
and after:

| | Before | After |
|---|---|---|
| `game/` lines since the import | +10898 / -1201 | +8327 / -1191 |
| Guarded blocks (outermost) | 1226 (1220) | 1202 (1198) |
| Lines inside the outermost guarded blocks (the `#else` originals included) | 13750 | 11089 |
| Unguarded code divergence | 208 added, 148 removed, 51 files | 190 added, 138 removed, 50 files |

Left in `game/` on purpose:
- blocks that are mostly the original code in an `#else` or an `#if !TARGET_PC` (c_dylink's REL
  name table, DynamicLink's REL loading, m_Do_ext's debug-draw packets enabled for PC with
  `#if DEBUG || TARGET_PC`): the PC side of each is a few lines already;
- line-for-line parallel versions where the host type or call differs on each line (JSupport.h's
  offset templates, c_lib.h's bit templates, JAIAnimation.h's BAS structs, binary.cpp's parser,
  JKRExpHeap's `uintptr_t` arithmetic, J3DShape.h/J3DGD.h's FIFO writes, J3DSys.h's matrix arrays):
  moving them would hide the data layout or the call from the code that uses it;
- `J3DHermiteInterpolationS` (J3DAnimation.cpp): per key frame and called from its own unit, where
  it is inlined;
- `__MTGQR7` (J3DTransform.cpp): J3DGQRSetup7 calls it in the same unit;
- code that would need friend declarations or other exposure of private members: the joint tree and
  shadow-control members stay assigned in `game/` (the loaders' and `imageDraw`'s own code), only
  the conversions and target set-up moved;
- GFGeometry.cpp's `GFSetArraySized`: SDK code compiled into `cos_sdk_gf`, not a game unit;
- dsptask.c's MEM1 copies (a C unit) and JASDSPInterface's (about 20 lines): small, audio start-up.

Unguarded changes kept as they are (no code change on the GameCube): identity macros
(`JKAR_DATA`, `JUT_CONTEXT`, `DEMO_PRM`, `COS_SCALE_FLAG`, `mDoMemCd_tryLockForSync`,
`PC_GPU_GROUP`, `fopAcM_ct_placement`), big-endian reads through `BE(T)` with an explicit cast
(JStudio, JASDSPInterface), `u32` for `unsigned long` in signatures (the same type on the
GameCube), `this->` and `public:` for two-phase lookup and the node offsets, scope braces around
blocks a `TARGET_PC` condition skips (m_Do_graphic, d_drawlist, d_event_manager), and the fixes
zeldaret/tww made too (fopScnRq_Execute's result, camera_delete's int, d_menu_cloth's GXEnd).

## G4b notes (2026-10-06)

The 199 candidates G4a left (units Matching upstream that we had edited, and the 14 G4a skipped for
their headers) were 3-way merged (base snrubrm b09eebc, ours, theirs zeldaret a1854d47c) in five
batches (JSystem and audio; framework, SSystem and m_Do; d/ core; actors A-M; actors N-Z with
d_npc and d_com_static): zeldaret's code everywhere, our guarded hunks re-applied on top with
zeldaret's names, hunks dropped where zeldaret has the same host fix (mostly `(u32)` ->
`(uintptr_t)` pointer casts and `u32` vs `unsigned long`). Coupled groups moved together:
ALIGN_DECL's two-argument form with its users, the controller pad buttons, the save's packed card
structs, MyScreen, the collision headers, d_npc with the NPCs. Census (`docs/GAME_DIVERGENCE.md`):

| | Before G4b | After G4b |
|---|---|---|
| Files equal to zeldaret/tww | 186 | 342 |
| Files that are zeldaret's code under our guarded hunks | (not counted) | 114 |
| Convergence candidates | 199 | 19 |
| Guarded blocks added (outermost) | 1200 (1196) | 1024 |
| Unguarded code divergence | 184 added, 119 removed, 46 files | 193 added, 120 removed, 49 files |

What is left: d_a_beam/d_a_obj_bemos (snrubrm's d_a_mozo, NonMatching upstream, uses the stepping
beamOn/beamOff zeldaret's d_a_beam.h no longer has), d_cam_param (zeldaret's d_cam_param.h moves
dCamSetup_c's names to other offsets and stubs accessors d_camera.cpp reads), JAIZelSound and
JAISystemInterface (they need zeldaret's JAISound.h, a work in progress there), and 15 merged
units whose only remaining difference is one of G3's unguarded identity macros (JKAR_DATA,
JUT_CONTEXT, TObject::NodeOffset, mDoMemCd_tryLockForSync, scope braces).

Lessons for the next convergence:
- Zeldaret's headers sometimes turn an inline snrubrm implements into an empty stub
  (`void setKind(u8, u8) {}`, `void chk_walk(cXyz*) { /* TODO */ }`), because no matched unit calls
  it. A snrubrm unit that still calls it compiles and silently does nothing. Every header taken
  is checked for such stubs; where snrubrm code calls one, snrubrm's body stays (d_a_tsubo.h,
  d_a_obj_movebox.h, d_a_obj_ikada.h, d_a_obj_swlight.h, d_a_player_main.h), or the whole header
  stays ours (JAISound.h, JAIBasic.h, JASDSPInterface.h, JASTrack.h, JAIStreamMgr.h,
  d_a_auction.h, d_a_obj_light.h, d_a_mant.h, d_cam_param.h).
- The census works on configure.py units: the `.inc` files a unit includes (d_a_player_*.inc,
  d_menu_capture.inc, d_a_bomb3.inc...) have to be taken with it by hand.
- Zeldaret's fakematches (`*(f32*)NULL = *(f32*)NULL;`) are undefined behaviour for clang and stay
  `#ifdef __MWERKS__`.
- The asset headers gen_assets.sh makes with snrubrm's converters write the one-argument
  ALIGN_DECL and no `*_NUM_JNTS_e`: global.h takes both ALIGN_DECL forms on TARGET_PC, and
  d_a_player_main.h defines CL_NUM_JNTS_e there.
