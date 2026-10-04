# Native port: the game built from its decompilation

`game/` holds the source of the game decompilation: `src/`, `include/` and its license,
imported unchanged from the decompilation fork at `b09eebc` and changed only
by later commits in this repository. That fork builds on the work of the
the upstream decompilation contributors and completes the remaining functions
with AI assistance; it is not part of upstream. Both are CC0-1.0 (`game/LICENSE`).

At the import, the fork's own build (`configure.py`, Metrowerks compilers) reproduced the
player's GZLE01 revision 0 `main.dol` and all 415 RELs byte for byte (`416 files OK`, SHA-1
checked): every function has source, two units are "Equivalent" rather than byte-matching.

The source contains no game data. Some files include `assets/...` headers that the fork's build
generates from the player's disc; this port generates them the same way at build time, outside
the repository, under `build/`.

The goal is a native build of the game on Aurora: on the Mac first, then the Switch, following the
approach of [Dusklight](https://github.com/TwilitRealm/dusklight) (CC0), whose
SDK-over-Aurora layer and static REL linking are the reference. Why: the translated build runs at
about 6-8 percent speed on the Switch (`docs/SWITCH_IMPLEMENTATION_CHECKLIST.md`), and native code
costs about 0.9 host instructions per guest instruction against 27 for the translation.

## Quick start (Mac)

This builds the native port and boots it to the title screen on an Apple Silicon Mac. The steps
below are the whole procedure from a fresh clone; the sections after this one explain each part.

**Requirements**

- An Apple Silicon Mac (arm64). Xcode command line tools (`xcode-select --install`, Apple clang).
- CMake 3.28 or newer and Ninja (`brew install cmake ninja`).
- Python 3.10 or newer as `python3` (the decomp's `configure.py` that generates the asset headers
  needs it; the `python3` of the command line tools is 3.9). `brew install python` is enough; no
  packages are needed.
- The network for the one-time setup and the first configure: RecompCore, the decomp and its
  `dtk` binary, Aurora and the dependencies it fetches (prebuilt Dawn and nod packages, SDL3,
  abseil, fmt...).
- Your own disc image of the game, USA, revision 0 (`GZLE01`), as a plain `.iso`. Nothing
  derived from it goes into git: the generated files stay under `build/`, which is ignored.

**One-time setup** (from the repository root)

```sh
export COS_DISC=/path/to/GZLE01.iso            # every script and the game read the disc from here
native/tools/fetch_recompcore.sh               # ref/recompcore: Dolphin's DSP HLE sources (RecompCore 8ab24da)
native/tools/gen_assets.sh                     # build/native-mac/assets/GZLE01: asset headers from the disc
```

**Configure and build** (about 1500 build steps; drop `-j4` to use every core)

```sh
cmake -S native -B build/native-mac -G Ninja
ninja -C build/native-mac -j4 centollos              # build/native-mac/centollos
```

**Run**

```sh
native/tools/run.sh logo-res               # milestone check: "logo-res: exit 0 (reached)" after ~10 s
native/tools/run.sh run --frames 1500 --shot 1500
#   boots for 1500 game frames (~45 s) and saves the title screen as
#   build/native-mac/runs/run-<timestamp>/shot-001500.png
build/native-mac/centollos                           # interactive: a 960x720 window; Ctrl-C in the terminal quits
```

`run.sh` opens a window too; each run keeps its log in `build/native-mac/runs/` (see "Running
`centollos` (phase 6)" for its options and exit codes). Without `COS_DISC` (or `--disc PATH`) a run that
boots the game stops with exit 14.

**Controls.** Port 0 is Aurora's SDL gamepad: connect a controller (Xbox, PlayStation, Switch
Pro...) before launching. Left stick: control stick; right stick: C-stick; the bottom face button
is A, and B, X and Y follow Aurora's default for the controller type (on an Xbox controller: left
B, right X, top Y, the GameCube's arrangement); right shoulder: Z; analog triggers: L and R;
Start: START; D-pad: D-pad. The keyboard is not mapped: Aurora has no default key bindings and
the port sets none yet, so without a controller you can only watch.

**What you should see now.** The publisher and Dolby logos, then the title screen ("PRESS START")
over Outset Island. Known problems: the gameplay HUD
(hearts, buttons, rupees) is drawn over the title, the "the game" subtitle under the logo is
garbled, some geometry renders black, and there is no sound yet.

**Platforms.** Only macOS on Apple Silicon is built and tested so far. Linux and Windows are
untested, although Aurora supports them (the scripts, and `native/cmake/dsp_hle.cmake`'s arm64
definitions, assume an arm64 Mac in places). The Switch is phase 7 of `docs/NATIVE_PORT_PLAN.md`.

## Building on the Mac (phase 1)

Phase 1 of `docs/NATIVE_PORT_PLAN.md`: every game unit compiles to an object with Apple clang
(arm64, C++20 / C11) under `TARGET_PC`, against the host C and C++ libraries instead of
Metrowerks' MSL. Phase 3 links them into one executable (see "The executable `centollos` (phase 3)"
below). Since step 2.8 of `docs/NATIVE_PORT_PHASE2_3.md` the game compiles against Aurora's SDK
headers (see "Aurora (phase 2)" below), so every configure brings in Aurora.

Requirements: Xcode command line tools (Apple clang), CMake 3.28 or newer (Aurora's FetchContent
needs it), Ninja, and the network on the first configure unless a local Aurora checkout is given.

```sh
cmake -S native -B build/native-mac -G Ninja
#   or, offline, with a local checkout of Aurora at the pin (see "Aurora (phase 2)"):
#   cmake -S native -B build/native-mac -G Ninja -DFETCHCONTENT_SOURCE_DIR_AURORA="$PWD/build/aurora-3227d76"
ninja -C build/native-mac cos_scaffold_check       # toolchain + base headers sanity check
ninja -C build/native-mac -k 0 cos_modules         # every enabled module
```

A build directory configured before step 2.8 (`COS_SDK_HEADERS=decomp`, `COS_WITH_AURORA=OFF` in
its cache) stops the configure with an error; reconfigure it with `cmake --fresh`.

Each module is an `OBJECT` library behind an option, off until it compiles; the modules listed
in `COS_MODULES_READY` (`cmake/modules.cmake`) compile and default to on (currently `SSystem`, `JSystem-core`, `JSystem-J3D`, `JSystem-2D-particle`, `JSystem-studio`, `framework`, `m_Do`, `d-core`, `actors-1`, `actors-2`, `actors-3`, `actors-4`, `actors-5`, `actors-6`, `audio`):

| Target | Option | Sources (`game/src/...`) |
| --- | --- | --- |
| `SSystem` | `COS_MODULE_SSystem` | `SSystem/**` |
| `JSystem-core` | `COS_MODULE_JSystem_core` | `JSystem/{JKernel,JSupport,JUtility,JMath,JGadget,JFramework,JRenderer}` |
| `JSystem-J3D` | `COS_MODULE_JSystem_J3D` | `JSystem/{J3DGraphBase,J3DGraphAnimator,J3DGraphLoader,J3DU}` |
| `JSystem-2D-particle` | `COS_MODULE_JSystem_2D_particle` | `JSystem/{J2DGraph,JParticle}` |
| `JSystem-studio` | `COS_MODULE_JSystem_studio` | `JSystem/{JStage,JMessage}`, `JSystem/JStudio/**` |
| `framework` | `COS_MODULE_framework` | `f_pc`, `f_op`, `f_ap`, `c`, `DynamicLink.cpp` |
| `m_Do` | `COS_MODULE_m_Do` | `m_Do` |
| `d-core` | `COS_MODULE_d_core` | `d/*.cpp` |
| `actors-1` … `actors-6` | `COS_MODULE_actors_N` | `d/actor`, sorted, in six equal chunks |
| `audio` | `COS_MODULE_audio` | `JSystem/JAudio` and `JAZelAudio`, 74 units (steps 3.7a to 3.7c; the four DSP `.c` units compile as C++, as the decomp's `-lang c++`); silent until phase 5 |

```sh
cmake -S native -B build/native-mac -DCOS_MODULE_framework=ON  # or -DCOS_ALL_MODULES=ON
ninja -C build/native-mac -k 0 SSystem                         # one module at a time
ninja -C build/native-mac cos_deferred                         # deferred units and reasons
```

Never part of the build (their headers may still be included): `src/dolphin` (the SDK is Aurora
plus `native/sdk`, phase 2), `src/REL` (the REL runtime: on PC the REL units are linked statically,
phase 3), `src/PowerPC_EABI_Support`, `src/TRK_MINNOW_DOLPHIN`, `src/OdemuExi2`, `src/odenotstub`,
`src/amcstubs`. `src/JSystem/JAudio` and `src/JAZelAudio` were left out in phase 1 and are built by
the `audio` module since step 3.7.

### Layout

- `native/CMakeLists.txt`: the project; `native/cmake/GameConfig.cmake`: definitions
  (`TARGET_PC=1`, `VERSION=2` for GZLE01, `NDEBUG=1`), include paths and flags, on the interface
  target `cos_game_headers` (Dusklight's `dusklight_game_headers`); `native/cmake/modules.cmake`:
  the modules; `native/cmake/deferred.cmake`: the units left for a later phase, each with its
  reason (`cos_defer(path "reason")`).
- `native/include/pc/cos_pc_config.h`, force-included in every unit: portable versions of the
  Metrowerks PowerPC intrinsics (`__cntlzw`, `__rlwimi`, `__dcbz`, `__sync`, `__fres`,
  `__frsqrte`) and MSL's float math in `std::` (`std::sqrtf`...).
- `native/include/pc/msl/`: shims for MSL-only header names (`algorithm.h`, `iterator.h`,
  `functional.h`, `new.h`) that include the host's standard headers.
- Changes to `game` that differ for the original target are under `#if TARGET_PC` with the
  original code kept in the other branch, as in Dusklight.

### Asset headers

Units including `assets/...` or `res/Object/...` need the headers the decomp's build generates from
the player's disc. They go under `build/native-mac/assets/GZLE01/` (`include/assets/...` and
`res/Object/...`, the layout of the decomp's `build/GZLE01/include` and `assets/GZLE01`), never in
git; `-DCOS_ASSETS_DIR=` points elsewhere.

From `framework` on, most units need them (`d/d_com_inf_game.h` includes `res/Object/Always.h`).
`native/tools/gen_assets.sh` generates them: it checks out the decomp
(the decompilation fork at `b09eebc`, the commit `game` was imported
from) under `build/decomp`, links the disc into its `orig/GZLE01/`, runs its `configure.py`
and builds only the targets that write headers (the `dtk dol split`, which also checks main.dol's
SHA-1, and the model data converters; no Metrowerks compiler is downloaded), then copies the
decomp's `assets/GZLE01/res` and the generated `build/GZLE01/include/assets` into place. It needs
Python 3.10 or newer and, the first time, the network:

```sh
native/tools/gen_assets.sh --disc /path/to/GZLE01.iso    # or COS_DISC=...; --out DIR for another COS_ASSETS_DIR
```

By hand, with a built checkout of the decomp (`python configure.py && ninja` for GZLE01):

```sh
mkdir -p build/native-mac/assets/GZLE01/include
cp -R <decomp>/assets/GZLE01/res build/native-mac/assets/GZLE01/
cp -R <decomp>/build/GZLE01/include/assets build/native-mac/assets/GZLE01/include/
```

## Aurora (phase 2)

Phase 2 builds the GameCube SDK over [Aurora](https://github.com/encounter/aurora) (MIT), the
library Dusklight uses: `native/cmake/Aurora.cmake` pulls it in with FetchContent at Dusklight's
pin, `3227d76`. `COS_WITH_AURORA` is on by default since step 2.8 of
`docs/NATIVE_PORT_PHASE2_3.md` and required: the game compiles against Aurora's headers, so
turning it off stops the configure. As in Dusklight, GX, DVD, CARD and THP are on and `aurora_mtx` is
built with `MTX_USE_PS=1`; RmlUi, Aurora's examples and its tests are off. On darwin-arm64 Dawn and
nod come from Aurora's prebuilt packages (`AURORA_DAWN_PROVIDER` / `AURORA_NOD_PROVIDER` =
`package`), so neither a Dawn source build nor Rust is needed.

The first configure needs the network: Aurora (unless a local checkout is given), the Dawn and nod
packages, SDL3, abseil, fmt, xxhash, imgui and Tracy are fetched into the build directory. Aurora
takes libpng, Freetype, zlib, SQLite and zstd from the system (Homebrew) when found.

```sh
cmake -S native -B build/native-mac -G Ninja
ninja -C build/native-mac aurora_core aurora_gx aurora_gd aurora_os aurora_vi aurora_pad \
    aurora_si aurora_mtx aurora_dvd aurora_card aurora_thp aurora_main
```

With a local clone of Aurora that contains the pin (such as `ref/aurora`), use CMake's own
override instead of cloning; the configure warns if that checkout is not at the pin:

```sh
git -C ref/aurora worktree add --detach "$PWD/build/aurora-3227d76" 3227d76
cmake -S native -B build/native-mac -G Ninja \
    -DFETCHCONTENT_SOURCE_DIR_AURORA="$PWD/build/aurora-3227d76"
```

The SDK libraries the game will link are listed in `COS_AURORA_LIBS`. The game flags
(`TARGET_PC`, `-fno-exceptions`, the force-included PC config header) live on the interface target
`cos_game_headers` and never reach Aurora.

`native/sdk` holds the game-specific SDK over Aurora: the static library `cos_sdk` and its headless
test `cos_sdk_smoke` (`native/cmake/sdk.cmake`); see
`native/sdk/README.md`.

The DSP behind `cos_sdk` is Dolphin's high-level DSP emulation (step 5.A, decision H6:
`native/dsp_hle`, `native/cmake/dsp_hle.cmake`), compiled from the RecompCore checkout in
`COS_RECOMPCORE_DIR`. It defaults to `ref/recompcore`, or to the main checkout's when the source
tree is a git worktree (`build/lanes/<lane>`); pass `-DCOS_RECOMPCORE_DIR=...` otherwise.
`native/tools/fetch_recompcore.sh` puts it in `ref/recompcore`: a shallow checkout, without
submodules, of [elliotttate/RecompCore](https://github.com/elliotttate/RecompCore) at `8ab24da`
(branch `bluewake`, the commit the recompilation project builds from; `patches/recompcore/README.md`).

### SDK headers

Aurora's headers are the only SDK headers the game compiles against (`cmake/GameConfig.cmake`,
decision D2 of `docs/NATIVE_PORT_PHASE2_3.md`). Phase 1 compiled against the decomp's own
`game/include/dolphin` (the former `COS_SDK_HEADERS=decomp` mode); step 2.7 migrated every
module and step 2.8 removed that mode for `TARGET_PC` (the original GameCube build still uses the
decomp's headers, which stay in the tree).

- The include order is `native/include` → `native/include/sdk` → Aurora's `include` →
  `game/include`. Aurora wins every header name both have (39 of the decomp's 82), so
  Aurora's own includes stay consistent.
- `native/include/sdk/dolphin/**` holds forwarders for the names only the game has (`dolphin/os/OS.h` →
  `<dolphin/os.h>` plus the game-only declarations Aurora lacks).
- `native/include/sdk/cos_sdk_extras.h`, force-included in every game unit, restores what the
  decomp's `dolphin/types.h` had beyond Aurora's (`uint`, `READU32_BE`, `FLOAT_MIN`/`FLOAT_MAX`).
- Three names both trees have but whose game-only content Aurora lacks come from headers a unit
  includes under `#if TARGET_PC`: `cos_card_extras.h` (`CARD_ERROR_*`), `cos_thp_extras.h`
  (the THP player types, instead of `dolphin/thp.h`) and `cos_dsp_extras.h` (the SDK's DSP task
  list, `__DSP_*_task`, for JAudio's `osdsp.c`/`osdsp_task.c`).
- Hardware registers the decomp defines in its headers (`__VIRegs`, `OS_PI_INTR_*`...) are left
  out on purpose.

`cos_sdk_header_check` compiles `check/sdk_headers.cpp`, which includes every SDK header name the
decomp has. `cos_sdk_shadow_check` runs `check/check_sdk_shadow.sh`: it fails if any dependency
of that unit resolves under `game/include/dolphin`, or if a name there is missing from the
unit. Since step 2.4 every one of the 82 names has a forwarder or an Aurora header, and none is
pending. game-only GF declarations (`GFLoadPosMtxImm`, `GFSetArray`, `GFBegin`...) live in
`native/include/sdk/cos_gf_extras.h`, which the `GF.h` and `GFTransform.h` forwarders include.

```sh
ninja -C build/native-mac cos_scaffold_check cos_sdk_header_check cos_sdk_shadow_check
```

### Link census (`cos_link_census`)

`ninja -C build/native-mac cos_link_census` (macOS only, not part of `all`;
`native/cmake/census.cmake`) shows what the non-REL game units still need from outside:

- It links the objects of every enabled module into the bundle
  `build/native-mac/link_census/libcos_link_census.bundle` with `-undefined dynamic_lookup`. The
  REL units are left out: `native/cmake/rel_units.txt` lists the 416 units the decomp builds into
  `.rel` files, by name only, taken from its `configure.py`. `cos_sdk` and Aurora are linked too. The module objects are reused, not recompiled.
- `native/tools/link_census.py` sorts what the bundle still looks up (`nm -um`) into SDK (by
  library: OS, GX, DVD...), REL (`OSLink*`, `OSSetStringTable`, `g_profile_*`, anything a REL
  unit defines), JAudio/JAZel, MSL/runtime, deferred units and other. It writes the counts and the
  lists, with the units that reference each symbol, to `build/native-mac/link_census.txt`, and the
  sorted `category<TAB>symbol` list to `build/native-mac/link_census_unresolved.txt`.
- Before the link, `native/tools/symbol_census.py` lists the duplicate strong definitions among
  the inputs and the weak definitions whose sizes differ (possible ODR violations), in
  `build/native-mac/link_census/symbol_census.txt`. By default (`COS_LINK_CENSUS_STRICT=ON`, since
  step 2.9 left none) a duplicate stops the link. With `-DCOS_LINK_CENSUS_STRICT=OFF` the
  duplicates are made local in copies of the objects (`ld -r`) and the census still links; the
  report counts them. (Phase 1's decomp SDK headers defined the hardware registers, `__VIRegs`,
  `OS_*`..., in every unit; Aurora's headers do not.)
- Phase 2 exit (step 2.9), kept up to date through phase 3: the list must equal
  `native/check/expected_unresolved_phase2.txt`. Since step 3.7c it holds a single symbol, the
  REL symbol `g_fpcPfLst_ProfileList`: `f_pc_profile.cpp` points at it on PC (step 3.4) and the
  REL unit `f_pc_profile_lst.cpp`, which the census leaves out, defines it. The REL loader's
  `OSLink`, `OSLinkFixed`, `OSUnlink` and `OSSetStringTable` are gone since step 3.5, and the
  JAudio/JAZel symbols since step 3.7c (the `audio` module is a main.dol module):

  ```sh
  ninja -C build/native-mac cos_link_census
  diff -u native/check/expected_unresolved_phase2.txt build/native-mac/link_census_unresolved.txt
  native/tools/symbol_census.py --dol   # 0 duplicate strong definitions among the main.dol units
  ```

`symbol_census.py` also works on its own, without linking, on object files, directories or
`@list` files:

```sh
native/tools/symbol_census.py build/native-mac/CMakeFiles/SSystem.dir --root build/native-mac
```

Full symbol census (step 3.1): `--all` takes every object the phase 3 executable links (main.dol
units, the REL units, marked `(REL)`, and `cos_sdk`) and adds a source-level section: types
(class/struct/union) defined at namespace scope, outside unnamed namespaces, in more than one
source file, since a plain struct leaves no symbol for `nm` to compare. It also lists weak
definitions that a strong definition in another object overrides (step 3.3): the linker keeps the
strong one silently, so two same-named classes can share a vtable even when the sizes match. It
only reports; `--dups` prints only the duplicate strong definitions and exits 1 if there is any.

```sh
ninja -C build/native-mac cos_symbol_census     # writes build/native-mac/symbol_census.txt
native/tools/symbol_census.py --all             # the same, by hand
native/tools/symbol_census.py --all --dups      # the step 3.2 check
```

The REL list is regenerated with
`native/tools/link_census.py rel-units --configure <decomp>/configure.py --out native/cmake/rel_units.txt --game-src game/src`.

## The executable `centollos` (phase 3)

Phase 3 of `docs/NATIVE_PORT_PHASE2_3.md` links the main.dol units, the 415 REL actors and
`f_pc_profile_lst` statically into one executable, following Dusklight's `c_dylink`. It is a
link and static-initialisation milestone only: the game does not boot yet (64-bit and endianness
are phase 4, audio is silent until phase 5, Aurora's event loop and the boot to the title are
phase 6).

```sh
cmake -S native -B build/native-mac -G Ninja      # all modules are on by default
ninja -C build/native-mac -k 0 cos_modules         # 914 objects: 424 main.dol, 416 REL, 74 audio
ninja -C build/native-mac centollos                      # build/native-mac/centollos (not part of `all`)
COS_SMOKE=static-init build/native-mac/centollos; echo $?
#   static-init: 502 of 502 profile slots filled, 0 error(s)
#   0
```

- `native/cmake/executable.cmake` defines `centollos`. Its only source is a generated empty unit; the
  objects of every module (nothing is recompiled) go in through
  `build/native-mac/cos_exe/objects.rsp` in this order: the main.dol units, then the REL units of
  `native/cmake/rel_units.txt` (`f_pc_profile_lst` first), then `audio`. ld64 runs static
  initialisers in input order, so the REL units see main.dol's globals already initialised, as a
  REL's `_prolog` did after boot. It links `cos_sdk` (which carries `COS_AURORA_LIBS`) and
  `aurora::main`, which owns the process entry point and calls the game's `main` (renamed
  `aurora_main` by `<aurora/main.h>` in `m_Do_main.cpp`). There is no
  `-undefined dynamic_lookup`: `nm -u build/native-mac/centollos` lists only system, framework and
  Homebrew library symbols. The target is skipped when `cos_sdk` or any module is not enabled.
- The REL loader is replaced under `TARGET_PC`: `c_dylink.cpp`'s name table is empty and
  `cDyl_Link`/`cDyl_LinkASync` report the module as linked; `DynamicLink.cpp` keeps the class
  shell without the loading and linking (`OSLink*`). `cDyl_InitAsync` still runs its callback on
  the DVD thread, so the boot order (the logo scene waits for it) is unchanged.
- The profile list is static: `g_fpcPf_ProfileList_p` points at `g_fpcPfLst_ProfileList` from the
  start (constant initialisation), `fpcPf_Get` checks bounds and NULL, and every `g_profile_*` is
  declared with the type its unit defines it with (decision D5).
- `COS_SMOKE=static-init` (step 3.9) makes `main` stop before any SDK initialisation: reaching it
  proves every static constructor (main.dol, REL, audio and Aurora) ran, and it checks the profile
  list (`g_fpcPf_ProfileList_p`, the NULL terminator, `mProcName == i`, `fpcPf_Get(i)`). It leaves
  with `_Exit`, without the game's static destructors. Since step 6.0 it lives in the run harness
  (`native/src/pc/pc_smoke.cpp`, see "Running `centollos` (phase 6)" below). Running `centollos` without
  `COS_SMOKE` enters the game's `main`, which is not expected to work before phases 4 to 6.
- `native/check/expected_unresolved_phase2.txt` and the symbol census stay the link checks: the
  census over every object reports 0 duplicate strong definitions, 0 weak data size mismatches, 0
  weak definitions overridden by a strong one and 0 types defined in more than one source file
  (steps 3.1 to 3.3). Same-named unit-local types are kept apart under `TARGET_PC` with unnamed
  namespaces, since the linker would otherwise merge their vtables silently.
- Phase 1 deferred no unit, so no actor needs a zeroed placeholder profile (step 3.6), and every
  JAudio/JAZel symbol is defined by the `audio` module, so no trap list (part (b) of step 3.7)
  exists.

### Targets

| Target | In `all` | What it is |
| --- | --- | --- |
| `cos_modules` (and each module) | yes | Every game unit as an object (`OBJECT` libraries) |
| `cos_deferred` | no | Prints the deferred units and their reasons (none) |
| `cos_scaffold_check` | yes | Toolchain and base header sanity check |
| `cos_sdk_header_check` | yes | Every SDK header name the decomp has compiles |
| `cos_sdk_shadow_check` | no | No SDK header of that unit resolves under `game/include/dolphin` |
| `cos_sdk`, `cos_sdk_gf` | yes | The game SDK over Aurora (`native/sdk`) |
| `cos_sdk_smoke` | yes | Headless test of `cos_sdk` (prints `ok`) |
| `cos_sdk_smoke_tsan` | no | The same under ThreadSanitizer |
| `cos_link_census` | no | Links the main.dol units with `-undefined dynamic_lookup` and lists what they still need |
| `cos_symbol_census` | no | `symbol_census.py --all` over every object, to `build/native-mac/symbol_census.txt` |
| `cos_pc` | yes | The run harness (`native/src/pc/pc_*.cpp`), linked into `centollos` and the link census |
| `centollos` | no | The game executable |
| `cos_layout_check` | no | The GameCube offsets of the disc-mapped structs (`native/check/layout_headers.txt`) hold on the host, minus `layout_xfail.txt`, and the decomp's offset comments hold for the GameCube (`native/tools/layout_check.py`, step 4.0c) |

Full check after a change, from a clean build directory:

```sh
ninja -C build/native-mac -k 0 all cos_sdk_shadow_check cos_link_census cos_symbol_census centollos cos_layout_check
build/native-mac/cos_sdk_smoke
diff -u native/check/expected_unresolved_phase2.txt build/native-mac/link_census_unresolved.txt
native/tools/symbol_census.py --all --dups
native/tools/run.sh static-init
```

## Running `centollos` (phase 6)

Step 6.0 of `docs/NATIVE_PORT_PHASE4_6.md`: a run harness in `native/src/pc` (the static library
`cos_pc`, globbed by `native/cmake/executable.cmake`; API in `native/include/pc/pc_harness.h`).
The game's `main` calls `pc_harness_init` first under `TARGET_PC`: it reads the environment,
installs the crash handler and the watchdog, runs a smoke test that needs no SDK (and exits),
then checks the disc.

```sh
native/tools/run.sh static-init               # milestone M0 -> exit 0
native/tools/run.sh <milestone|smoke> [--timeout 180] [--stall 30] [--trace res,scene] ...
COS_DISC=/nonexistent build/native-mac/centollos; echo $?   # 14
```

| Variable | Meaning |
| --- | --- |
| `COS_DISC` | the GZLE01 revision 0 `.iso` (required to boot; `run.sh --disc PATH` sets it, and `run.sh`, `regress.sh` and `disc_manifest.py` stop with exit 14 when neither is given) |
| `COS_SMOKE` | a smoke test: `static-init`, and the harness self-tests `crash-test`, `panic-test`, `stall-test`, `timeout-test` |
| `COS_MILESTONE` | exit 0 when this milestone (M0-M14 names: `static-init`, `aurora-up`, `heaps`, ...) is logged |
| `COS_TIMEOUT_S`, `COS_STALL_S` | watchdog: exit 10 after this long; exit 11 when the game frame counter is frozen this long (0/unset: off) |
| `COS_TRACE` | `res` (every path through `my_DVDConvertPathToEntrynum`), `scene` (every scene process created), `all` |
| `COS_FRAMES` | exit 0 after this many game frames |
| `COS_SHOT`, `COS_SHOT_EVERY`, `COS_SHOT_DIR` | save the presented image of these game frames (`1500` or `300,1500`), or of every n-th frame, as `shot-<frame>.png` in the run directory or `COS_SHOT_DIR` (`native/src/pc/pc_shot.cpp`; `run.sh --shot`) |
| `COS_UNCAPPED`, `COS_AUDIO` | frame pacing off (step 6.2); `off` keeps audio silent (step 6.1, phase 5) |
| `COS_PERF_EVERY` | every this many game frames, a `[cos] perf` line: the game thread's time per frame (average, maximum), the pace wait, frames and VI retraces a second, the phase split and the CPU time (0/unset: off) |
| `COS_HITCH_MS` | a `[cos] hitch` line for every game frame whose busy time is over this many ms: its split (events, begin_frame, cpd, aud, logic, painter, end_frame, other) and what else happened in it (pipelines built, texture bytes uploaded, resources loaded, the scene; on the Switch also the render worker, GL fence and disc counters) (0/unset: off; the Switch sets 50) |
| `COS_FPS_OVERLAY` | `1`: a panel in the top-left corner (Aurora's ImGui, not in `COS_SHOT` images) with the frames per second over the last half second and the game thread's ms per frame; on the Switch also the render worker's ms per frame, its Queue::Submit part and Dawn's GL draws and texture binds per frame (0/unset: off; the Switch sets 1) |
| `COS_CACHE_DIR` | Aurora's caches (`pipeline_cache.db`, `dawn_cache.db`) in this directory instead of `<directory of the executable>/user/cache` (its parent must exist; not on the Switch). `run.sh` sets it to `<run dir>/cache` when `COS_CACHE_PER_RUN=1` (`native/tools/gen_pipeline_cache.sh` records each run's pipelines that way) |
| `COS_PRECOMPILE_LOG` | `1`: `[cos] precompile N/M pipelines` once a second while Aurora's start-up pipeline warm-up (every pipeline its cache knows, plus the bundled `initial_pipeline_cache.db` next to the executable if there is one) runs, `[cos] precompile loading screen N/M ...` while the loading screen is up, and `[cos] precompile done: ...` at its end (0/unset: off; the Switch sets 1) |
| `COS_PRECOMPILE_SCREEN` | when `COS_PRECOMPILE=boot` or `full` shows its loading screen (docs/SWITCH_BUILD.md, "Pipeline precompile"): `auto` (default) only when the warm-up's first builds (up to 8, at most 0.3 s) average more than `COS_PRECOMPILE_SLOW_MS` (default 25) ms, i.e. a cold shader cache; else the game starts at once and the priority pipelines build first behind the logos (Switch: if the builds turn slow during the logo scene and the priority pipelines left would take over 1 s, the loading screen comes up there); `always` the loading screen as before; `never` none. One `[cos] precompile screen` line logs the choice |
| `COS_PRECOMPILE` | the Switch's boot warm-up (docs/SWITCH_BUILD.md, "Pipeline precompile"): `boot` (Switch default) a "Preparing shaders... N/M" loading screen (Aurora's ImGui) before the game starts, until the pipelines the bundled cache marks priority 0 (the boot path) are built, then the rest are built behind the logos and menus with "Shaders N/M" in the bottom-right corner until the game first enters its PLAY scene, where the warm-up ends; `full` the loading screen until every known pipeline is built; `all` no loading screen, every known pipeline built (indicator until done); `off` none. On the Mac unset changes nothing (Aurora's warm-up runs to the end, nothing drawn); set, the loading screen and indicator are drawn to try them (`boot` waits for as many pipelines as the bundled file next to `centollos` marks priority 0; `off` only hides them) |
| `COS_PERF` | a file that gets one CSV row per game frame (step 6.7; `run.sh --perf perf.csv` puts it in the run directory); see "Performance instrumentation" |
| `COS_RUN_DIR` | where `backtrace.txt` and `stall.txt` go (set by `run.sh`) |
| `COS_ASPECT` | `4:3` (default on the Mac), `16:9` (default on the Switch) or `16:10`: the widescreen option, the community 16:9 Gecko code done in C (wider view and culling, HUD at the screen edges) with Aurora presenting the picture at that aspect ([docs/MODS.md](../docs/MODS.md), "Widescreen in the native port") |
| `COS_FB_SCALE` | the internal resolution: Aurora's frame-buffer scale (`VISetFrameBufferScale`) of the 640x480 EFB, widened to the window's aspect; at 16:9 `1.5` = 1280x720, `1.125` = 960x540, `1.0` = 854x480 (EFB copies scale with it; the present pass resamples to the window; `COS_SHOT` images are at this size). Unset or `0`: the window's own size (the Mac's default); the Switch sets `1.5` ([docs/SWITCH_BUILD.md](../docs/SWITCH_BUILD.md)) |
| `COS_SHADOW_OFFSCREEN` | GPU A/B option (docs/SWITCH_PERF_STUDY.md, option a1; `native/include/pc/pc_gpu_opts.h`), default off: `1` draws the real-time shadow casters into an offscreen target (`GXCreateFrameBuffer`) of the size their 256x256 EFB corner has at the internal resolution, so the main EFB pass is not split per shadow; the I4 shadow textures are pixel-identical. `gc`: the GameCube's 256x256 target and 128x128 textures at any resolution (softer, cheaper shadows; for measurement) |
| `COS_DOF` | GPU A/B option (`pc_gpu_opts.h`), default on: `0` skips `drawDepth`'s depth of field (m_Do_graphic.cpp, every play frame: a Z16 and a half-size colour EFB copy, their conversion passes and a full-screen composite) unless the monotone effect or the motion blur needs it; the letterbox bars stay. Far scenery is no longer softened. For measurement |

Performance instrumentation (step 6.7, `pc_frame.cpp`). `COS_PERF=<file>` writes, per game frame,
`frame,t_ms,wall_ms,busy_ms,cpu_ms,wait_ms,begin_ms,cpd_read_ms,aud_execute_ms,logic_ms,painter_ms,aurora_end_frame_ms,other_ms,retrace`:
`t_ms` is the frame's start since the loop's first frame, `wall_ms` runs from `pc_frame_begin` to
the end of `aurora_end_frame`, `wait_ms` is the pace wait (JFWDisplay's `waitForTick`),
`busy_ms` = wall - wait (the "game thread" of the `[cos] perf` line), `cpu_ms` the game thread's
CPU time (`CLOCK_THREAD_CPUTIME_ID`: time blocked is left out, and so is the CPU of the limiter's
final spin; empty where the clock is missing). The split adds up to `busy_ms`: `begin_ms` (Aurora's
events and `aurora_begin_frame`), `mDoCPd_Read`, `mDoAud_Execute`, `logic_ms` (`fapGm_Execute`
without the painter), `painter_ms` (`mDoGph_Painter`, the GX encode, without the pace wait inside
it), `aurora_end_frame` and `other_ms` (the rest of main01's loop). The `[cos] perf` lines of
`COS_PERF_EVERY` average the same numbers, so the Mac's CSV and the Switch's log compare
directly. The game thread (the process main thread) runs at QoS USER_INTERACTIVE on macOS, logged
as `[cos] game thread: QoS`. The benchmark variant is a build directory of its own:
`cmake -S native -B build/native-mac-perf -G Ninja -DCOS_PERF_BUILD=ON` (Release at `-O2`, every
target at `-march=armv8-a`, generic ARMv8.0 like the A57, Tracy off; it logs
`[cos] perf: COS_PERF_BUILD variant`), run with `run.sh --exe build/native-mac-perf/centollos`.

Exit codes: 0 reached, 1 smoke check failed, 2 usage error, 10 timeout, 11 stall, 12 panic
(`OSPanic`, which `JUT_ASSERT` ends in), 13 signal, 14 disc problem. Milestones are logged as
`[cos] MILESTONE <name> frame= retrace= ms=`.

- Crash handler (`pc_crash.cpp`): `sigaction` for SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP and
  SIGABRT. It prints the fault address (with a hint when it is below 4 GiB, i.e. in macOS arm64's
  `__PAGEZERO`: a pointer truncated to 32 bits), the scene, frame, retrace count and last resource,
  the registers, the image load address and a `backtrace_symbols_fd` backtrace of the faulting
  context; `run.sh` appends `atos` file:line names to `backtrace.txt`. On PC `OSPanic` prints
  the same through `pc_panic` instead of walking the PowerPC back chain.
- Watchdog (`pc_watchdog.cpp`): on a stall it suspends the other threads and writes their
  frame-pointer backtraces to `stall.txt` (`sample` needs developer mode).
- `run.sh` writes `build/native-mac/runs/<target>-<timestamp>/` (`command.txt`, `env.txt`,
  `run.log`, `exit_code.txt`, `backtrace.txt`, `stall.txt`), checks the disc's `main.dol` SHA-1
  on first use (cached in `runs/disc_check.txt`), and kills the process if it outlives the
  timeout by 30 s.
- `native/tools/lldb_crash.sh` reruns under `lldb --batch` with the inherited environment. With
  developer mode off it exits 3 without starting lldb, which would wait for an authorisation
  prompt.

## The Switch (phase 7)

`scripts/switch/build_native.sh` builds this tree as a Switch NRO, `build/switch-native/centollos.nro`,
in the translated port's devkitPro container, with the same asset headers and Aurora checkout as the
Mac build: docs/SWITCH_BUILD.md, "Native port", has the build, the copy to the console, the run
options (`COS_*` from an `env.txt` on the SD card) and the crash reports. The Switch build lives in
`switch/native` (Aurora patches, a GameCube disc reader standing in for nod, the harness's platform
layer); in this tree it only adds code under `#if defined(__SWITCH__)` in `src/pc` (no signals,
`backtrace()` or `_Exit` there: libnx's exception handler, `svcQueryMemory` frame walks and an exit
that writes the logs out) and the `COS_EXE_ENTRY` hook of `cmake/executable.cmake`. `game` is
unchanged. `COS_PERF_EVERY=<n>` (any host, off by default, on by default on the Switch) logs the
game thread's time per frame every n frames.
