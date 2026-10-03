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

## Building on the Mac (phase 1)

Phase 1 of `docs/NATIVE_PORT_PLAN.md`: every game unit compiles to an object with Apple clang
(arm64, C++20 / C11) under `TARGET_PC`, against the host C and C++ libraries instead of
Metrowerks' MSL. Nothing is linked yet. Since step 2.8 of `docs/NATIVE_PORT_PHASE2_3.md` the game
compiles against Aurora's SDK headers (see "Aurora (phase 2)" below), so every configure brings in
Aurora.

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
in `COS_MODULES_READY` (`cmake/modules.cmake`) compile and default to on (currently `SSystem`, `JSystem-core`, `JSystem-J3D`, `JSystem-2D-particle`, `JSystem-studio`, `framework`, `m_Do`, `d-core`, `actors-1`, `actors-2`, `actors-3`, `actors-4`, `actors-5`, `actors-6`):

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

```sh
cmake -S native -B build/native-mac -DCOS_MODULE_framework=ON  # or -DCOS_ALL_MODULES=ON
ninja -C build/native-mac -k 0 SSystem                         # one module at a time
ninja -C build/native-mac cos_deferred                         # deferred units and reasons
```

Never part of the build in phase 1 (their headers may still be included): `src/dolphin` (the SDK
over Aurora is phase 2), `src/REL` (phase 3), `src/JSystem/JAudio` and `src/JAZelAudio` (phase 5),
`src/PowerPC_EABI_Support`, `src/TRK_MINNOW_DOLPHIN`, `src/OdemuExi2`, `src/odenotstub`,
`src/amcstubs`.

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
With a built checkout of the decomp (`python configure.py && ninja` for GZLE01), copy them in:

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
- Two names both trees have but whose game-only content Aurora lacks come from headers a unit
  includes under `#if TARGET_PC`: `cos_card_extras.h` (`CARD_ERROR_*`) and `cos_thp_extras.h`
  (the THP player types, instead of `dolphin/thp.h`).
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
- Phase 2 exit (step 2.9): the list must equal `native/check/expected_unresolved_phase2.txt`, which
  holds only the REL symbols (`OSLink`, `OSLinkFixed`, `OSUnlink`, `OSSetStringTable`; step 3.5)
  and the JAudio/JAZel ones (step 3.7, phase 5):

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
source file, since a plain struct leaves no symbol for `nm` to compare. It only reports; `--dups`
prints only the duplicate strong definitions and exits 1 if there is any.

```sh
ninja -C build/native-mac cos_symbol_census     # writes build/native-mac/symbol_census.txt
native/tools/symbol_census.py --all             # the same, by hand
native/tools/symbol_census.py --all --dups      # the step 3.2 check
```

The REL list is regenerated with
`native/tools/link_census.py rel-units --configure <decomp>/configure.py --out native/cmake/rel_units.txt --game-src game/src`.
