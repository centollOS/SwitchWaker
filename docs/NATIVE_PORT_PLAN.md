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

Phase 2 and 3 step plan and decisions: `docs/NATIVE_PORT_PHASE2_3.md`.

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
- **framework:** 57/57 units (f_pc, f_op, f_ap, c, `DynamicLink.cpp`) compile, 0 deferred; on by
  default. Most units need the asset headers (`d/d_com_inf_game.h` includes `res/Object/Always.h`),
  copied from a built decomp checkout into the gitignored build dir (see `native/README.md`).
  Pointer-to-`u32` casts in `JORMContext::genCheckBox` (`m_Do_hostIO.h`) and in `DynamicLink.cpp`
  (`do_link`'s address assert and `fixSizePtr`, `ModuleUnresolved`'s back-chain walk) go through
  `uintptr_t` under `TARGET_PC` (`TODO(native phase 3/4)`). Original bugs left for phase 4:
  `-Wfortify-source` strcpy overflows in `f_op_msg_mng.cpp` (Rupee strings) and missing returns
  in its `dummyfloat*` stubs.
- **m_Do:** 17/17 units compile, 0 deferred; on by default. `print_f` in
  `m_Do_machine_exception.cpp` runs `va_start`/`va_end` on `args.list` under `TARGET_PC` (MSL
  allowed them on the `std::__tag_va_List` wrapper). The arena bounds in `mDoMch_Create` and the
  stack walks and address checks in `m_Do_printf.cpp` (`OSGetCallerPC`, `search_partial_address`,
  `convert_partial_address`, `OSPanic`) cast pointers through `uintptr_t` under `TARGET_PC`
  (`TODO(native phase 4)`).
- **d-core:** 136/136 units (top level of `src/d`) compile, 0 deferred; on by default.
  `cLib_onBit`/`offBit`/`checkBit`/`setBit` (`c_lib.h`) take a non-deduced bit argument under
  `TARGET_PC` (u32 fields with `0x..UL` literals deduce one `T` only where `u32` is
  `unsigned long`). The compat header predefines the generated display-list macro block
  (`LOAD_BP_REG` & co., same text) with `IMAGE_ADDR` through `uintptr_t`
  (`TODO(native phase 2)`: Aurora GX must resolve DL texture references). `d_meter.cpp`'s global
  `clock[3]` is renamed by macro to `dMeter_clock` under `TARGET_PC` (host `::clock()`). Offset
  relocations in `d_stage`, `d_s_menu`, `d_s_actor_data_mng`, `d_resorce` and the recollection
  buffers in `d_com_inf_game` go through `uintptr_t` under `TARGET_PC` (`TODO(native phase 4)`).
- **actors-1:** 74/74 units (`d_a_acorn_leaf` .. `d_a_fan`) compile, 0 deferred; on by default.
  `J3DModel::setUserArea((u32)this)` passes the pointer as `(uintptr_t)` under `TARGET_PC`
  (`TODO(native phase 4)`: `mUserArea` is still a u32). Locals initialised inside a `switch` and
  bypassed by a later `case` (`d_a_bo`, `d_a_bl`) are declared then assigned under `TARGET_PC`.
  `next_msgStatus(unsigned long*)` (`d_a_dai.h`, `d_a_bigelf`) takes `u32*` under `TARGET_PC`.
  `JGadget::binary` iterators' `operator!=` compares `mBegin` directly under `TARGET_PC`: the
  hidden-friend `operator==` is not found by ADL on the base-class arguments.
- **actors-2:** 74/74 units (`d_a_floor` .. `d_a_npc_aj1`, sorted indices 74-147) compile, 0
  deferred; on by default. Same idioms as actors-1: `setUserArea` casts through `uintptr_t` (24
  sites, `TODO(native phase 4)`), case-bypassed locals declared then assigned (`d_a_gy`, `d_a_gnd`,
  `d_a_kamome`; in `d_a_mt` the bypassed `dAttention_c&` is scoped to its one statement),
  `u32*`/`u32&` instead of `unsigned long` in `d_a_npc_ah.h`, `d_a_npc_aj1.h` and `d_a_lod_bg.h`.
  `d_a_mt` names the `cXyz` temporary whose address it passes. `JGadget/search.h` includes
  `dolphin/types.h` (it used `s32` without it). `d_a_movie_player`'s THP decoder casts its
  addresses through `uintptr_t`; its Huffman/IDCT paths are PowerPC asm under `#ifdef __MWERKS__`
  with no C fallback in the decomp, so on PC they are still empty. Dusklight replaces that decoder
  under `TARGET_PC`; that belongs with the SDK work (phase 2), not here.
- **actors-3:** 74/74 units (`d_a_npc_auction` .. `d_a_obj_barrel`, sorted indices 148-221)
  compile, 0 deferred; on by default. Same idioms again: `setUserArea` casts through `uintptr_t`
  (69 sites in 54 files, `TODO(native phase 4)`); `next_msgStatus(unsigned long*)` and
  `setMessage(unsigned long)` take `u32` under `TARGET_PC` in 16 `d_a_npc_*.h` headers (and the
  `d_a_npc_p2` definition), so the virtual ones override `fopNpc_npc_c`'s again; `d_a_npc_ym1`'s
  `area_check` callback takes `u32`; `d_a_npc_bj1`'s `dist_xz`, bypassed by a `goto`, is declared
  then assigned.
- **actors-4:** 74/74 units (`d_a_obj_barrel2` .. `d_a_obj_nest`, sorted indices 222-295)
  compile, 0 deferred; on by default. Only 13 errors in 11 units, all known idioms:
  `setUserArea` casts through `uintptr_t` (11 sites in 9 files, one of them a `J3DPacket` in
  `d_a_obj_buoyflag`, `TODO(native phase 4)`); `d_a_obj_doguu.h`'s `next_msgStatus` takes `u32*`
  under `TARGET_PC`; `d_a_obj_hcbh`'s case-bypassed `actor` local is declared then assigned.
- **actors-5:** 74/74 units (`d_a_obj_ohatch` .. `d_a_ship`, sorted indices 296-369) compile,
  0 deferred; on by default. 57 errors in 19 units: `setUserArea` casts through `uintptr_t`
  (30 sites in 21 files, one a `J3DPacket` in `d_a_obj_tapestry`, `TODO(native phase 4)`);
  `d_a_player_main` carves its animation buffers and the rock-mark image data by address
  arithmetic on `uintptr_t`; `d_a_obj_tapestry`'s alignment assert uses `uintptr_t`, and its
  `l_HIO` is a file-scope static instead of an anonymous-namespace one (ambiguous with the
  global class `l_HIO`); `d_a_ph`'s two case-bypassed `speed` locals are declared then assigned;
  `d_a_obj_search` passes a named `GXColor` to `dComIfGd_setAlphaModelColor(GXColor&)`;
  `d_a_player_rope.inc` calls `abs((int)u32)`, since only MSL's `abs(int)` made it unambiguous.
- **actors-6:** 71/71 units (`d_a_shop_item` .. `d_a_yougan`, sorted indices 370-440) compile,
  0 deferred; on by default. 15 errors in 13 units, all known idioms: `setUserArea` casts
  through `uintptr_t` (13 sites in 11 files, `TODO(native phase 4)`); `d_a_tag_hint` and
  `d_a_tag_island` declare and define `next_msgStatus` with `u32*` under `TARGET_PC`.

## Phase 2 log

- **2.1 Aurora:** `native/cmake/Aurora.cmake` fetches Aurora at `3227d76` behind `COS_WITH_AURORA`
  (default OFF), with Dusklight's options (GX, DVD, CARD, THP on; RmlUi off; `aurora_mtx` with
  `MTX_USE_PS=1`) and prebuilt Dawn/nod packages on darwin-arm64. All Aurora SDK libraries build;
  MIT notice in `RIGHTS_AND_LICENSES.md`. Deviation: off GEKKO, Aurora's `<dolphin/mtx.h>` maps
  `PSMTX*` to `C_MTX*` by macro, so `libaurora_mtx.a` exports `_C_MTXConcat`, not `_PSMTXConcat`;
  the 2.2 smoke test calls `PSMTXConcat` through the header instead of looking for the symbol.
- **2.2 cos_sdk skeleton:** `native/cmake/sdk.cmake` (only with `COS_WITH_AURORA=ON`) globs
  `native/sdk/src/**/*.{c,cpp}` with `CONFIGURE_DEPENDS` into the STATIC `cos_sdk` (Aurora headers
  only, public `MTX_USE_PS=1`, linked to `COS_AURORA_LIBS`) and `native/sdk/tests/*.cpp` into the
  headless `cos_sdk_smoke`, whose tests self-register by name (`tests/smoke.h`), so 2.6a-f add files
  without touching shared ones. `cos_sdk_smoke` prints `ok` (`basic`: OSInit, OSGetTime,
  PSMTXConcat); Aurora's `OSInit` needs no `aurora_initialize` with the zeroed config.
- **2.3 SDK header mode:** `COS_SDK_HEADERS=decomp|aurora` in `GameConfig.cmake` (default
  `decomp` until 2.8; `aurora` needs `COS_WITH_AURORA=ON` and includes `Aurora.cmake` early, which
  now has an include guard). Aurora order: `native/include` → `native/include/sdk` → Aurora
  `include` → `game/include`, plus `MTX_USE_PS=1` and the force-included
  `native/include/sdk/cos_sdk_extras.h` (`uint`, `READU32_BE`, `FLOAT_MIN/MAX`). Forwarders for the
  16 game-only OS, MTX, VI and PAD names (`base/PPCArch.h` collides, so base needs none); game-only
  declarations Aurora lacks are added once (`__OSReport_*`, `__OSModuleList`, `SVec`, `MtxP`,
  `Mtx33`, `PADClampRegion`, `VI_3D`...), hardware registers and boot code left out, and
  declarations Aurora has with another signature (`OSGetStackPointer`, `OSBootInfo`'s fields) left
  to Aurora for 2.7. `global.h`, `DynamicLink.h` and `weak_bss_3569.h` include their SDK header with
  `<>` under `TARGET_PC`: a quoted include there is looked up next to the header first, i.e. in
  `game/include/dolphin`, whatever the `-I` order. `cos_sdk_header_check` builds in both
  modes; `cos_sdk_shadow_check` (`check/check_sdk_shadow.sh`) passes for 55 names, 27 pending 2.4.
- **2.4 Remaining forwarders:** the 27 game-only GX, GF, DVD, AR, AI, SI, EXI, GBA, DB and amcstubs
  names (GD has none: all three the game GD names are Aurora's). Each includes Aurora's header and adds
  once what Aurora lacks: `GXColor3x8/4x8` (over `GXColor3u8/4u8`), `GXSetDrawSync`, `GXCopyMode`,
  the GX bit-field macros, `DVDDirectory`/`DVDDirectoryEntry` (= Aurora's `DVDDir`/`DVDDirEntry`;
  field names left to 2.7), `DVDGetLength`, the SI API beyond `SIProbe`, `EXI_STATE_*`, the GBA
  private state, AMC EXI2 and DB link functions, and SDK-internal `__DVD*`/`__GX*` prototypes. game-only
  GF declarations go in `native/include/sdk/cos_gf_extras.h` (C++ linkage, register macros taken from
  Aurora's GD headers). Left out on purpose: `GXFIFO` and the macros that write it (J3D moves to
  `GXCmd1u*` in 2.7, so it fails to compile rather than write to 0xCC008000), `GXData`/`gx` and
  the CP/PE/PI registers, `__DIRegs`/`__EXIRegs`/`__SIRegs`, and the `DVDState`/`DVDResult`/
  `ARamType` enums whose enumerators are Aurora macros (Aurora's `DVD_RESULT_CANCELED` is -6, the
  decomp's -3). `native/include/sdk/types.h` forwards the bare `<types.h>` Aurora's `gba.h` includes.
  `cos_sdk_shadow_check`: 82 names checked, 0 pending.
- **2.5 Link census:** `native/cmake/census.cmake` builds the MODULE `cos_link_census` (bundle,
  `-undefined dynamic_lookup`, C++ link, not in `all`) from every enabled module's objects minus
  the REL units (nested `$<FILTER:$<TARGET_OBJECTS:m>,EXCLUDE,regex>` of 40 names each), plus
  `cos_sdk` and Aurora when built; `native/tools/link_census.py` classifies `nm -um` into SDK (per
  library), REL, JAudio/JAZel, MSL/runtime, deferred and other (`link_census.txt`,
  `link_census_unresolved.txt`). `native/cmake/rel_units.txt` has **416** units, not 415: the
  decomp's `configure.py` has 415 `ActorRel`s plus `Rel("f_pc_profile_lst")`; 424 non-REL + 416
  REL = 840. `native/tools/symbol_census.py` reports duplicate strong definitions and weak
  definitions with differing sizes (Mach-O has no symbol sizes: distance to the next symbol or
  the section end). Deviation: in decomp header mode every unit defines the SDK hardware
  registers, so a strict link fails in every module (SSystem alone: 19 duplicates; all non-REL
  units: 30, including `JPACallBackBase*` methods and `hio_set`). By default the census therefore
  makes duplicates local in `ld -r` copies and counts them in the report;
  `COS_LINK_CENSUS_STRICT=ON` keeps the planned behaviour (listed first, then the link stops).
  First result (decomp headers, no Aurora): 452 unresolved: SDK 316 (GX 125, OS 78, MTX 32,
  CARD 16, DVD 15, GF 11, VI 10, GD 8, PAD 8, GBA 7, AR 5, SI 1), REL 4 (`OSLink`, `OSLinkFixed`,
  `OSUnlink`, `OSSetStringTable`), JAudio/JAZel 120, other 12: `dCamera_c::eyePos` (defined
  `inline` in `d_camera.cpp`, called from `d_ev_camera.cpp`) and 11 `mDoExt_*Packet`
  constructors (defined under `#if DEBUG` in `m_Do_ext.cpp`, called from `d_debug_viewer.cpp`),
  both for 2.9 to settle. With `COS_WITH_AURORA=ON` (still decomp headers, `cos_sdk` and Aurora
  linked): 230, of which SDK 94 (OS 56, GX 13, GF 11, GBA 7, VI 7); MTX, CARD, DVD, PAD, SI, AR
  and GD all resolve; JAudio/JAZel 120. The classifier harvests only single-line column-0
  declarations and tests plain C names against the SDK before the JAudio/JAZel and deferred
  buckets, so an SDK gap such as `OSLockMutex` called from audio code is never filed under
  JAudio (where 2.9's expected list could hide it); the bundle relinks when the census scripts
  change, so the POST_BUILD report always reflects the current classifier.
- **2.6a OS threads, interrupts, mutexes, messages, alarms, contexts:** `native/sdk/src/os/`
  (`OSThread`, `OSInterrupt`, `OSMutex`, `OSMessage`, `OSAlarm`, `OSContext`, internal
  `os_internal.h`), adapted from Dusklight's `OSThread.cpp`/`OSMutex.cpp`/`OSContext.cpp` and the
  message queues of `stubs.cpp` (provenance in each file). Model: "interrupts disabled" is one
  process-wide lock held per thread with the GameCube's boolean Disable/Restore semantics; every OS
  function runs under it and uses the SDK's own algorithms (priority-ordered thread queues,
  `OSSleepThread`/`OSWakeupThread`, active-thread list, joinable vs detached, mutexes on
  `thread->queueMutex` released on exit/cancel), so there are no per-object side tables. Blocked
  threads wait on their own condition variable, which releases the lock even if the caller had
  interrupts disabled (as a thread switch restores the next thread's MSR). `OSCreateThread`
  threads are detached pthreads launched by the first `OSResumeThread` (host stack ≥ 1 MiB);
  `OSExitThread` ends the thread (`pthread_exit`) and `OSJoinThread` returns the full 64-bit value.
  Alarms (D6) fire on a real host timer thread that calls handlers with the lock held; handlers
  that block abort. Deviations: priorities order nothing (phase 6); `OSSuspendThread`/
  `OSCancelThread` on another thread running game code act at its next OS call (logged); the
  switch-thread callback is never called (logged once). `OSInitContext`/`OSSaveContext`/`OSLoadContext`/
  `OSSwitchStack`/`OSSwitchFiber` abort; `OSGetStackPointer` returns 0. New
  `native/sdk/include/cos_sdk/hooks.h`: `COSSdkRequestShutdown` (blocking message calls return
  FALSE, alarms stop) and `COSSdkSetThreadStartHook` (per-thread setup such as the current
  `JKRHeap`, replacing Dusklight's game couplings). Tests `threads` and `alarms`
  (`native/sdk/tests/sdk_threads.cpp`, 1000-message ping-pong, mutex/cond counter, cancel,
  periodic/cancelled/re-arming alarms, JFWDisplay's alarm-resume-self-suspend). TSan: new target
  `cos_sdk_smoke_tsan` (not in `all`, without `aurora::dvd`, whose Rust personality breaks the
  macOS TSan link). Xcode 26's clang 17 TSan crashes at start-up on macOS 26.6 even for an empty
  program, so the run uses `build/native-mac-tsan` configured with the Command Line Tools clang
  21 (recipe in `native/sdk/README.md`): 10 runs + 40 runs of `threads alarms`, no report. Census
  with `COS_WITH_AURORA=ON`: SDK/OS 56 → 18 (the rest is 2.6b: reset, console, PPC, `__OSBusClock`).
- **2.6b OS misc:** `native/sdk/src/os/{OSReport,OSMisc,OSReset,OSSram,PPC,LC}.cpp`, adapted from
  Dusklight's `OSReport.cpp` and the OS/PPC parts of `stubs.cpp` (provenance in each file).
  `OSReport`/`OSVReport`/`OSPanic` are defined by the game (`m_Do_printf.cpp`), so cos_sdk only has
  weak defaults (stderr; `OSPanic` aborts) in a file that defines nothing else: the archive member
  is never loaded when the game is linked (the census bundle uses the game's strong ones).
  `OSFatal`, `PPCHalt`, `PPCMtdmaL` with the trigger bit and the IPL-font getters abort.
  `OSResetSystem` runs the SDK's priority-ordered reset functions (non-final passes until all are
  ready, then the final pass with interrupts disabled), records the reset code (hot reset: the code;
  restart: `0x80000000`) and the saved region, then calls the new reset hook
  (`COSSdkSetResetHook` in `hooks.h`, for phase 6 to restart the game); it never returns: without a
  hook, or if the hook returns, it logs, calls `COSSdkRequestShutdown` and ends the process with
  exit code 0 (the game spins after the call). SRAM is the SDK's lock/unlock/checksum model over an
  in-memory copy (stereo, NTSC, interlaced, English; not persisted), declared with the RTC
  functions in the new `cos_sdk/sram.h`; `OSGetSoundMode` returns 0/1 as on the console (Dusklight
  returns 2). PPC registers are emulated values: MSR[EE] is the interrupt state, MSR/FPSCR are per
  thread, the decrementer counts at the timer clock, HID2[LCE] reads set (Aurora's locked cache is
  always usable; `d_a_movie_player` checks it), and settings the host cannot honour (FP exception
  enables, non-IEEE mode, performance counters, memory protection) are logged once. Aurora's
  `OSCache.cpp` already has the cache ranges and `LC*` copies; `LC.cpp` adds the rest (`DCBlockZero`
  clears its block; the others do nothing). `__OSBusClock` is a real variable for the decomp
  headers (Aurora makes it a low-memory read). `OSSetErrorHandler` records handlers
  (`COSSdkGetErrorHandler` for later glue). Test `misc` (`native/sdk/tests/sdk_misc.cpp`, death
  checks in forked children with captured stderr); 20 runs of the whole smoke program and 10 TSan
  runs, no failure or report. Census with `COS_WITH_AURORA=ON`: SDK/OS 18 → 0 (total 192 → 174).
- **2.6c VI retrace and GX gaps:** `native/sdk/src/vi/VIRetrace.cpp` and
  `native/sdk/src/gx/GXExtras.cpp`, adapted from the VI and GX sections of Dusklight's `stubs.cpp`
  (provenance in each file); the GX list is the plan's plus the census's SDK/GX bucket
  (`GXInitTexCacheRegion`, `GXPeekARGB`, `GXPokeAlphaRead`, `GXReadXfRasMetric` besides the
  planned names). Each `VIWaitForRetrace` call is one retrace, run as the SDK's handler does
  (count, pre-retrace callback, next frame buffer latched, post-retrace callback) with interrupts
  disabled; it does not pace (phase 6). `VISetBlack` is recorded for the glue
  (`COSSdkVIIsBlack` in `hooks.h`; Aurora keeps presenting, logged once), `VIGetNextField`
  alternates with the retrace count, `VIGetDTVStatus` is 0 (no progressive-scan prompt).
  `GXGetNumXfbLines`/`GXGetYScaleFactor` and `GXInitTexCacheRegion` use the SDK's algorithms
  (Dusklight returns 0, which would size JUTXfb's buffers to 0 lines). The GX thread is kept as in
  the SDK, starting as the default (main) OS thread (new `DefaultThreadLocked` in `os_internal.h`),
  since `mDoRst_reset` cancels it when it is not the caller. `GXSetDrawSync` delivers the token at
  once, inside the call (Aurora's public API has no GPU-signalled token). Logged once where the host
  differs: draw sync, `GXSetMisc(GX_MT_DL_SAVE_CONTEXT, 0)` (Aurora's private `dlSaveContext` stays
  1), `GXPeekARGB` (no EFB colour read-back: white, alpha from `GXPokeAlphaRead`, which d_snap reads
  as "no object"; TODO phase 6), the counters (0); `GXAbortFrame` logs every call.
  `GXGetFifoBase`/`GXGetFifoSize` abort (Aurora's FIFO objects keep no base or size; nothing in the game calls them). Not added:
  `GXWaitDrawDone` is static in the game's SDK, and the metric functions beyond the game's three. Tests `vi`
  (the plan's three retraces, plus frame buffer latch, fields, 1000 retraces from two threads) and
  `gx` (`native/sdk/tests/sdk_vi.cpp`); 20 runs of the whole smoke program and 10 TSan runs, no
  failure or report. Census with `COS_WITH_AURORA=ON`: SDK/GX 13 → 0, SDK/VI 7 → 0 (total
  174 → 154). Reviewed in round 1: smoke `vi`/`gx`, 18 more whole-program runs and 8 TSan runs
  clean, census reproduced, default configuration (`cos_modules` and checks) unchanged.
