# cos_sdk: the GameCube SDK for the native port, over Aurora

Phase 2 of the native port (`docs/NATIVE_PORT_PHASE2_3.md`). The model is Dusklight's: Aurora's
SDK libraries (`aurora::core gx gd si vi pad mtx os dvd thp card`, listed in `COS_AURORA_LIBS`)
plus a layer of glue for what Aurora does not provide (threads, messages, alarms, VI retrace, GF,
AI/DSP, EXI, GBA, DVD extras, ...). That glue lives here.

Built only with `-DCOS_WITH_AURORA=ON` (see `native/README.md`, "Aurora (phase 2)").

## Layout

- `src/<library>/*.{c,cpp}`: the SDK functions, one directory per SDK library (`os`, `vi`, `gx`,
  `dvd`, ...). `native/cmake/sdk.cmake` globs `src/**/*.{c,cpp}` with `CONFIGURE_DEPENDS`, so adding
  a file needs no CMake change (re-running `ninja` picks it up).
- `include/cos_sdk/`: the library's own headers (hooks the game glue sets, information functions).
  This directory is on `cos_sdk`'s public include path.
- `tests/`: the headless test program `cos_sdk_smoke` (no window, no GPU). Every `tests/*.cpp` is
  globbed; each file registers its tests by name with `COS_SMOKE_TEST(name)` from `tests/smoke.h`.

## Rules

- Compiled against **Aurora's headers only**: never `game/include` and never the game's flags
  (`cos_game_headers`). The SDK's own declarations come from `<dolphin/...>` as Aurora ships them;
  game-only declarations go into the forwarders of `native/include/sdk` (steps 2.3 and 2.4).
- `MTX_USE_PS=1` is public on `cos_sdk`, as in Dusklight's game flags.
- Code adapted from Dusklight (`ref/dusklight/src/dusk`, CC0) starts with a provenance comment
  naming the original file and what changed (decision D7). Code adapted from Aurora (MIT) keeps
  its notice.
- Hardware-only functions may be stubs only where the plan says so. A stub whose silent result
  would hide a bug logs it, and `OSPanic`/`OSFatal` abort.
- The SDK layer itself is 64-bit correct (pointers are `uintptr_t`, never `u32`).

## Building and running the test

```sh
cmake -S native -B build/native-mac -G Ninja -DCOS_WITH_AURORA=ON
ninja -C build/native-mac cos_sdk cos_sdk_smoke
build/native-mac/cos_sdk_smoke            # every test; prints "ok"
build/native-mac/cos_sdk_smoke basic      # only the named tests
build/native-mac/cos_sdk_smoke --list
```

`basic` (step 2.2) checks `OSInit`, `OSGetTime` and `PSMTXConcat`. Aurora's `OSInit` does not need
`aurora_initialize`: with the default (zeroed) config it allocates no MEM1 and leaves the arena
unset, which is enough for the time and matrix functions.
