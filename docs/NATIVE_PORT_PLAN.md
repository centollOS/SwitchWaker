# Native port plan: the game from its decompilation, on Aurora

**Branch:** `feature/switch-native` (from `feature/switch-port`) · **Started:** 2026-10-03

## Why

The translated build (DolRecomp → C) runs the whole game on the Switch, with Aurora, controller,
sound and rumble, but at 6-8 percent of real time at the stock 1020 MHz (3.7-4.7 retraces a second
against 60; `docs/SWITCH_IMPLEMENTATION_CHECKLIST.md`). On Apple M-class cores that translation
already keeps a core 80-100 percent busy; no translator change is likely to give the ~13x the Switch
needs (DolRecomp's LLVM backend measured slower than its C backend in August; the C emitter's cheap
levers are measured and spent). Native code from the decompilation costs about 0.9 host
instructions per guest instruction against about 27 for the translation
(`docs/status/ROUTE_DECISION_2026-09-22.md`). The CPU stays at its stock clock.

## Inputs

- **Game source:** `game/`, the decompilation fork of the upstream decompilation imported
  unchanged at `b09eebc` (`native/README.md`). Hito 0: its Metrowerks build reproduces this disc's
  `main.dol` and 415 RELs byte for byte (`416 files OK`).
- **Reference port:** Dusklight (another GameCube game on Aurora, CC0), in `ref/dusklight` for study:
  `libs/dolphin` (the GameCube SDK over Aurora), `src/c/c_dylink.cpp` (RELs linked statically),
  `include/helpers/endian.h` and `offset_ptr.h` (big-endian disc data, relative pointers), its
  `TARGET_PC` changes to the decompiled game (345 files), C++20 with a precompiled header.
- **Switch layer already proven on the console** (`switch/`): Aurora with Dawn on Mesa GLES and a
  window surface, the SDL 3 shim (HID gamepad with HD rumble, audout, events, SD I/O), the NRO
  packaging, push and live USB log tools.
- **Lessons from this repository's earlier decomp route** ("Route B", August-September 2026, which
  reached a room scene with a patched upstream centollos): about 170 `docs/status/ROUTE_B_*.md` notes.

## Phases

1. **Native build on the Mac.** A CMake project under `native/` that compiles `game` with
   clang for arm64 (C++20, `TARGET_PC`, the host C library instead of Metrowerks' MSL), unit by
   unit: SSystem, JSystem (JKernel, JSupport, JUtility, JMath, J3D, J2D, JParticle…), then the game
   framework (`f_*`, `m_Do`, `d/`) and the actors. Assets headers generated from the player's disc
   at build time, outside the repository.
2. **The GameCube SDK over Aurora** (`dolphin/` OS, GX, VI, PAD, DVD, CARD, AR, DSP), with
   Dusklight's `libs/dolphin` as the model; Aurora already provides GX, VI, PAD, OS, SI, MTX.
3. **RELs linked statically** into one executable, following Dusklight's `c_dylink`.
4. **64-bit and endianness:** pointers stored in 32-bit fields, structures read straight from disc
   data (big-endian), alignment and padding assumptions; Dusklight's `BE<T>` and `offset_ptr`.
5. **Audio (JAudio1, `JAZelAudio`):** Dusklight has JAudio2 only; port the decomp's JAudio1 to the
   host DSP/audio path.
6. **Milestones on the Mac:** boot to the title, then controllable Outset; measure the game thread's
   milliseconds per frame on an M-class core and extrapolate to the Cortex-A57 (must leave margin
   under 33 ms at 1020 MHz).
7. **The Switch:** cross-compile with devkitA64, reuse `switch/` (Aurora, shim, NRO, SD, logs).

Each phase lands as its own commits; this file records decisions and measured results as they come.

## Phase 1 log

- **scaffold:** CMake project under `native/` (Ninja, arm64, C11/C++20), `cos_game_headers` with
  `TARGET_PC=1`, `VERSION=2`, `NDEBUG=1` and the force-included `native/include/pc/cos_pc_config.h`
  (portable `__cntlzw`, `__rlwimi`, `__dcbz`, `__sync`, `__fres`, `__frsqrte`; MSL's float math in
  `std::`), MSL header-name shims, 14 module targets covering all 840 in-scope units (off by
  default), `cos_defer()` list (empty). `s32`/`u32` are `int` under `TARGET_PC` and the intrinsic
  declarations in `global.h` are original-target only. Check target: 2/2 units, 0 deferred.
- **SSystem:** 36/36 units compile, 0 deferred; module on by default (`COS_MODULES_READY`).
  MSL's `std::__tag_va_List` added to the compat header; pointer-to-integer casts in
  `JKRExpHeap::CMemBlock::getBlock` and `cBgS::ConvDzb` go through `uintptr_t` under `TARGET_PC`
  (`TODO(native phase 4)`: 0x10 block header and 32-bit dzb offsets).
- **JSystem-core:** 59/59 units (JKernel, JSupport, JUtility, JMath, JGadget, JFramework,
  JRenderer) compile, 0 deferred; on by default. Compat header: `<cctype>`, `<cstdio>`,
  `<cstdlib>`, `<cstring>` so MSL's `std::strrchr`/`std::tolower` resolve; MSL shim
  `msl_memory.h` (host `<memory>`). `JGadget_outMessage`'s `int`/`uint` overloads are
  original-target only (they redeclare the `s32`/`u32` ones when those are `int`).
  Pointer-to-integer casts (about 100, mostly JKernel heaps and archives, `JSUConvertOffsetToPtr`,
  `OSRoundUp32B`/`OSRoundUpPtr`/`OSRoundDownPtr`) go through `uintptr_t`/`intptr_t` under
  `TARGET_PC`; values still stored in `u32` fields/locals are phase 4 (`TODO(native phase 4)`
  on `TVector::size`'s `/ 4`).
- **JSystem-J3D:** 31/31 units (J3DGraphBase, J3DGraphAnimator, J3DGraphLoader, J3DU) compile,
  0 deferred; on by default. About 50 pointer-to-`u32` cast sites (hashes, `mDiffFlag`, file
  offsets kept in pointer-typed fields, GX array base, vertex counts from pointer differences) go
  through `uintptr_t`/`intptr_t` under `TARGET_PC` with `TODO(native phase 4)`; where the result
  becomes a pointer again the base is not truncated. `OSCachedToPhysical`/`OSUncachedToPhysical`
  in `OS.h` fixed at the macro. `J3DMaterialFactory_v21::create`'s declaration spells `u32` like
  its definition (was `unsigned long`, MWCC's `u32`).
- **JSystem-2D-particle:** 26/26 units (J2DGraph 8, JParticle 18; JRenderer already in
  JSystem-core) compile, 0 deferred; on by default. Only `J2DPrint.cpp` needed changes: the
  pointer differences compared as `u32` in `parse` and the hex-length checks of
  `getNumberS32`/`getNumberF32` cast each pointer through `uintptr_t` under `TARGET_PC`
  (`TODO(native phase 4)`); modulo-2^32 subtraction gives the original distance.
- **JSystem-studio:** 37/37 units (JStudio with all its adaptor subdirectories, JStage,
  JMessage) compile, 0 deferred; on by default. Compat header gains MSL's `DEG_TO_RAD`/`RAD_TO_DEG`
  (same text, MSL's float pi as `COS_MSL_M_PI`). `JGadget::search::TExpandStride_` gets
  `long`/`long long` specializations under `TARGET_PC` (host `ptrdiff_t`); `TLinkList_factory`
  calls `this->Erase` (dependent base). Pointer casts in `functionvalue`, `stb`,
  `stb-data-parse`, `jstudio-object` and `object-actor` go through `uintptr_t`/`intptr_t` under
  `TARGET_PC` (`TODO(native phase 4)`; `TFunctionValue_composite::TData` still stores a pointer in
  a `u32`).
