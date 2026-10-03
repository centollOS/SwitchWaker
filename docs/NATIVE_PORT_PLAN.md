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
- **2.6d GF:** the game's `game/src/dolphin/gf/GF{Geometry,Light,Pixel,Tev,Transform}.cpp`
  compiled into `cos_sdk` through an OBJECT library `cos_sdk_gf` in its own
  `native/cmake/sdk_gf.cmake` (included after `sdk.cmake`), against Aurora's headers plus the
  `native/include/sdk` forwarders and `cos_gf_extras.h`, whatever `COS_SDK_HEADERS` is, as Dusklight
  compiles Dusklight's GF. Aurora's `GXCmd1u*` write into the same FIFO outside a display list as inside
  one, and its command processor parses `LOAD_BP/CP/XF_REG` in both, so GF needs no shim. One
  `TARGET_PC` edit: Aurora ignores `CP_REG_ARRAYBASE` (logs "not supported"), so the new
  `GFSetArraySized(attr, ptr, size, stride, le)` writes `GX_AURORA_LOAD_ARRAYBASE` with the 64-bit
  pointer as Aurora's `GDSetArraySized` does, and `GFSetArray` (no size) stops with `OSPanic`;
  its callers (`m_Do_graphic`, `d_tree`, `d_grass`) switch in step 2.7. Test `gf`
  (`native/sdk/tests/sdk_gf.cpp`): each GF call recorded in a GX display list equals Aurora's GD
  function of the same name byte for byte; `GFSetCullMode`, `GFBegin`/vertex writers and
  `GFSetArraySized` against hand-encoded commands; `GFSetArray`'s panic in a forked child. 20 runs
  of the whole smoke program and 10 TSan runs, no failure or report. Census with
  `COS_WITH_AURORA=ON` (decomp headers): SDK/GF 11 → 9 (total 154 → 152); the 9 left differ only
  in the decomp's enum tags (`_GXAttr` vs Aurora's `GXAttr`) in the mangled names, and resolve
  once the callers compile against Aurora's headers (2.7); the smoke test, which includes GF.h
  through the forwarder, links against them.
  THP: no replacement decoder is needed in `native/sdk`; Aurora's `aurora_thp` (already linked)
  provides `THPInit`/`THPVideoDecode`/`THPAudioDecode`, and the actors step that compiles
  `d_a_movie_player.cpp` puts its PPC-asm decoder under `#if !TARGET_PC` and calls Aurora's, as
  Dusklight does. Reviewed in round 1: GF units rebuilt, `nm` shows the 15 GF functions, smoke
  `gf` and the whole program ok, census reproduced (SDK/GF 9, total 152), default configuration
  (`cos_modules` and checks) unchanged.
- **2.6e Devices:** new `native/sdk/src/{ar,gba,exi,si,db}/*.cpp` (globbed; no CMake change).
  A scan of every compiled unit (REL ones included) against `cos_sdk` and the Aurora libraries
  found only the 7 `GBA*` functions of `JUTGba` missing among these prefixes, plus
  `ARGetBaseAddress` for JAudio (`JASSystemHeap`, phase 5). Every DVD name the plan lists is
  already in `aurora_dvd`, and the CARD icon/banner accessors are macros in Aurora's `card.h`, so
  no `dvd/` or `card/` file was needed; the SDK-internal `__DVD*` and unused `CARD*`
  (`CARDErase`, `CARDProgram`, vendor/disk-ID) stay undefined. Added: AR (`ARStartDMA` as an
  immediate copy between a MEM1 physical address and Aurora's ARAM buffer, `ARGetStorageAddress`
  standing in for its private `aramToHost`, callback with interrupts disabled, `ARGetDMAStatus` 0,
  fatal on any range outside MEM1/ARAM; `ARGetBaseAddress` 0x4000, the value Aurora's `ARInit`
  returns, so JKRAram and JAudio agree; `ARClear`, `ARGetInternalSize`, and the always-empty ARQ
  queue functions), GBA (every request `GBA_NOT_READY`, async ones refused without callback,
  `GBAGetProcessStatus` `GBA_READY` = idle), EXI (real lock/unlock queue from the decomp's
  EXIBios.c, no device answers a selection), SI extras (`SIGetType`/`SIGetStatus` over Aurora's
  `SIProbe`, `SITransfer` completing with `SI_ERROR_NO_RESPONSE` on the alarm thread, the SDK's
  poll word and polling-handler table), DB (host `__DBInterface` with no debugger, the OdemuExi2
  link refusing reads and writes, `__DBExceptionDestination` aborting) and amcstubs (the SDK's own
  no-op stubs). Tests `ar` (4 KB round trip through `ARStartDMA` both ways, also seen by Aurora's
  ARQ; aborts for a console address and an ARAM overrun), `gba`, `exi`, `si`, `db`
  (`native/sdk/tests/sdk_devices.cpp`, which sets `AuroraConfig.mem1Size`/`mem2Size` for the smoke
  program). 20 runs of the whole smoke program and 10 TSan runs, no failure or report. Census
  with `COS_WITH_AURORA=ON` (decomp headers): SDK/GBA 7 → 0, total 152 → 145, duplicates still 30;
  the only SDK names left are 2.6d's 9 GF enum-tag manglings.
  Reviewed in round 1: smoke `ar`/`gba`/`exi`/`si`/`db` and the whole program ok (5 runs), 5 TSan
  runs clean, census reproduced (SDK/GF 9, total 145), no cos_sdk/Aurora symbol overlap, default
  configuration (`cos_modules` and checks) unchanged; `GBAGetProcessStatus` = `GBA_READY` accepted
  (the SDK's idle value; JUTGba ignores the result).
- **2.6f Audio hardware (silent) and MSL extras:** new `native/sdk/src/audio/{AIStubs,DSPStubs,DTK}.cpp`
  and `native/sdk/src/runtime/extras.c` (globbed), tests `audio` and `msl`
  (`native/sdk/tests/sdk_audio.cpp`). AI: a register model that plays nothing (settings read back,
  `AIGetDSPSampleRate` in the SDK encoding, 1 before `AIInit` and 0 after, as `d_a_movie_player`
  expects; no interrupt or callback ever; `AIInitDMA` keeps the `uintptr_t`, `AIGetDMAStartAddr`
  aborts when it does not fit a u32). DSP: mail to the DSP taken at once, none back, the SDK's task
  list, `__DSP_boot_task`/`__DSP_exec_task` log once and never wait; `DSPAddTask` weak (JAudio's
  `osdsp.c` overrides it). DTK: Dusklight's `libs/dolphin` dtk.c copied as C++ over the silent AI
  and Aurora's synchronous DVD stream commands (the game never calls it; left out of the TSan target,
  which links no `aurora::dvd`). `stricmp`/`strnicmp` from Dusklight's `extras.c`, compared as
  unsigned char. Census (Aurora on, decomp headers): MSL/runtime 0, no SDK/audio bucket, total 145
  (both were already 0; the real consumers, JAudio/JAZelAudio, come in 3.7). Open for 3.7/phase 5:
  JAudio's own DSP handshake loops spin with no DSP; Aurora's `<dolphin/dsp.h>` lacks the
  `__DSP_*` task declarations `osdsp_task.c` needs in aurora-header mode.
  Reviewed in round 1: new files rebuilt, smoke `audio`/`msl` and the whole program ok (6 runs),
  3 TSan runs clean, census reproduced, no audio/MSL name of any compiled unit unresolved against
  cos_sdk and Aurora, DTK.cpp matches Dusklight's dtk.c apart from the listed changes, default
  configuration (`cos_modules` and checks) unchanged.
- **2.7 SSystem:** compiles in aurora header mode (36/36 units, 0 errors) and still in decomp mode.
  Fixes are headers only, under `TARGET_PC`: Aurora's `GXColor`/`GXRenderModeObj` are typedefs of
  unnamed structs, so the forward `typedef struct _GX* ...` lines go (`c_cc_d.h` includes
  `dolphin/gx/GXStruct.h`; `JUTXfb.h`/`JFWDisplay.h` already get it through `JUTVideo.h`);
  `JUTVideo::isAntiAliasing` reads Aurora's `aa` (same u8 at 0x19) under `defined(COS_SDK_AURORA)`,
  which the aurora-only `cos_sdk_extras.h` defines (TODO 2.8: remove with decomp mode); `global.h`
  undefines Aurora's `ASSERT` before its own (both empty without `DEBUG`). No `STATIC_ASSERT`
  fired. Pattern for later modules: the other `typedef struct _GX*` forwards (`m_Do_lib.h`,
  `J3DModelData.h`, `JUTException.h`, ...) and `JFWDisplay.cpp`'s `antialiasing` need the same fix.
  Aurora-mode builds use the gitignored `build/native-mac-aurora` (offline, sources from
  `build/native-mac/_deps` and `build/aurora-3227d76`). Reviewed in round 1: SSystem rebuilt
  from clean in aurora mode with the shadow, header and scaffold checks and smoke ok; default
  configuration (`cos_modules` and checks) rebuilt, 0 errors.
- **2.7 JSystem-core:** compiles in aurora header mode (all units of the module, 0 errors) and still
  in decomp mode. First aurora build: 552 errors in 17 files, 463 of them `GXFIFO` from the
  `JSystem.pch` headers (`J3DShape.h`, `J3DGD.h`). Fixes in the forwarders: `dolphin/os/OS.h` gets
  the decomp's `OSRoundUp`/`OSRoundDown` (+`Ptr`, through `uintptr_t`), `OS_ERROR_MEMORY_PROTECTION`
  (Aurora's `OS_ERROR_PROTECTION`), `OS_ERROR_FLOATING_POINT_EXCEPTION`, the `OSException` enum and
  `OSContextPPC`/`OSContextPPCOf`, a view of the PowerPC register image over Aurora's opaque
  `OSContext` storage (same 0x2C8 size, `static_assert`ed); `dolphin/gx/GX.h` gets the decomp's
  `GX_BL_*` blend-factor spellings. Fixes in `game`, all under `TARGET_PC` with the original
  kept: `JRNLoadCPCmd`/`JRNLoadXFCmdHdr`/`J3DCurrentMtx::load`/`J3DGXCmd1f32ptr`/`J3DFifo*` write
  through `GXCmd1u8/u16/u32` (the decomp's inline `GXFIFO` writes in decomp mode); `JSystem.pch`
  includes `global.h`, which the decomp reached through its GX headers; `JFWSystem.h` and
  `JUTException.h` include `GXStruct.h` instead of the `_GXRenderModeObj` forward; and, under
  `defined(COS_SDK_AURORA)` (TODO 2.8), Aurora's field names (`aa`, `DVDFileInfo::cb/startAddr`,
  `DVDDirEntry::isDir/entryNum`, `OSBootInfo::memorySize`, `OSThread::stackBase/stackEnd`), the
  typed `OSCreateThread` entry point (`JKRThread`, `JUTGba`), `JKRAramPiece::doneDMA(uintptr_t)`
  for Aurora's `ARQCallback` (TODO phase 4: `JKRAMCommand::AsyncCallback` still takes `u32`), and
  `JUTException.cpp` reading register fields through `JUT_CONTEXT(context)` (identity outside
  aurora mode). No `STATIC_ASSERT` fired (they are empty outside Metrowerks). `J3DShape.cpp`'s own
  `GXFIFO` writes belong to the J3D step. Reviewed in round 1: SSystem and JSystem-core rebuilt
  from clean in aurora mode (89 units, 0 errors) with the shadow, header and scaffold checks and smoke
  ok; JSystem-core rebuilt from clean in the default configuration and `cos_modules` and checks clean.
- **2.7 JSystem-J3D:** compiles in aurora header mode (all 31 units, 0 errors) and still in decomp
  mode. First aurora build: 26 of 31 units failed, 106 distinct errors (clang's 50-per-unit limit
  hid none beyond them). Fixes in the forwarders: `dolphin/gx/GX.h` includes Aurora's
  `<dolphin/gd/GDGeometry.h>` for the BP/CP/XF register numbers (`GX_BP_REG_SETMODE0_TEX*`,
  `GX_BP_REG_TEVCOLORCOMBINER0`, ...), which the decomp's `GX.h` exports through its `GXEnum.h`
  (all 318 shared names have the same values), and maps `GX_MAXCOORD`/`GX_TEXMAP_DISABLE` to Aurora's
  `GX_MAX_TEXCOORD`/`GX_TEX_DISABLE`. Fixes in `game`, all under `TARGET_PC` with the original
  kept: `J3DLoadCPCmd` (`J3DShape.cpp`) writes through `GXCmd1u8/u32`; `J3DVertex.h` and
  `J3DModelData.h` include `GXStruct.h` instead of the `_GXColor` forward; and, under
  `defined(COS_SDK_AURORA)` (TODO 2.8), `GXFogAdjTable::r` (the decomp's `fogVals`) in `J3DGD.cpp`,
  `GDGetCurrPointer` as Aurora's `u8*` `GDGetCurrPointer2` in `J3DMatBlock.cpp`, and
  `J3DSys::setModelDrawMtx`/`setModelNrmMtx` calling Aurora's 5-argument `GXSETARRAY` with host byte
  order and the whole 16-bit index range as the size (GX has no bound; TODO phase 4: pass the
  model's real matrix count so Aurora's indexed-load check bites). No `STATIC_ASSERT` fired.
  Aurora mode: SSystem, JSystem-core, JSystem-J3D, `cos_sdk`, smoke, scaffold, header and shadow
  checks (82 names, ok) build; default configuration: `cos_modules` and checks rebuilt (622 steps),
  0 errors.
  Reviewed in round 1: JSystem-J3D rebuilt from clean in aurora mode (31 units, 0 errors) with the
  shadow, header and scaffold checks and smoke ok, and from clean in the default configuration with
  `cos_modules` and checks clean; Aurora only pushes `size` bytes for vertex arrays, so the 16-bit
  index range for the matrix arrays affects nothing but the indexed-load bounds check.
- **2.7 JSystem-2D-particle:** compiles in aurora header mode (all 26 units of J2DGraph and JParticle,
  0 errors) and still in decomp mode. First aurora build: one unit failed (`JPABaseShape.cpp`) with 5
  errors, the decomp's `GXLogicOp` spellings `GX_LO_REV_AND`, `GX_LO_INV_AND`, `GX_LO_REV_OR`,
  `GX_LO_INV_COPY` and `GX_LO_INV_OR`. Fixed in the forwarder only: `dolphin/gx/GX.h` maps them to
  Aurora's `GX_LO_REVAND`, ... (same values 0x2, 0x4, 0xB, 0xC, 0xD); no change in `game`, no
  `STATIC_ASSERT` fired. Aurora mode: SSystem, JSystem-core, JSystem-J3D, JSystem-2D-particle,
  `cos_sdk`, smoke, scaffold, header and shadow checks (82 names, ok) build; default configuration:
  `cos_modules` and checks clean (the forwarder is not on the decomp-mode include path).
- **2.7 JSystem-studio:** compiles in aurora header mode (all 37 units of JStudio, JStage and
  JMessage, 0 errors) and still in decomp mode. First aurora build: 15 units failed with one error
  each, all from the `typedef struct _GXColor GXColor;` forward in `jstudio-object.h` (clang's
  50-per-unit limit hid nothing). Fixed as in `J3DVertex.h`: under `TARGET_PC` the header includes
  `dolphin/gx/GXStruct.h`, the original forward kept in `#else`. No forwarder change, no
  `STATIC_ASSERT` fired. Aurora mode: SSystem, JSystem-core, JSystem-J3D, JSystem-2D-particle,
  JSystem-studio (rebuilt from clean), `cos_sdk`, smoke, scaffold, header and shadow checks (82
  names, ok) build; default configuration: `cos_modules` and checks rebuilt (every includer of the
  header), 0 errors.
- **2.7 framework:** compiles in aurora header mode (all 57 units of f_pc, f_op, f_ap, c and
  `DynamicLink.cpp`, 0 errors) and still in decomp mode. `build/native-mac-aurora` now takes the
  asset headers from `build/native-mac` (`-DCOS_ASSETS_DIR=.../build/native-mac/assets/GZLE01`,
  cache only). First aurora build: 12 units failed on 7 distinct errors, all from headers (none hit
  clang's 50-per-unit limit): the `_GXColor` forwards in `d_bg_w.h` and the `_GXColor` overloads
  of `mDoExt_3DlineMat0_c::update` in `m_Do_ext.h`, the `_GXTexObj`/`_GXTlutObj` forwards in
  `m_Do_lib.h`, and `UNUSED` in `f_ap_game.h`. Fixes in `game`, all under `TARGET_PC` with
  the original kept: `d_bg_w.h` and `m_Do_lib.h` include `dolphin/gx/GXStruct.h`; the
  `m_Do_ext.h` overloads name `GXColor`; `m_Do_hostIO.h` includes `global.h`, which the decomp's
  `GXStruct.h` brings in, so every HIO class's `genMessage` gets `UNUSED`; `ModuleUnresolved`
  (`DynamicLink.cpp`) casts Aurora's `u32 OSGetStackPointer()` through `uintptr_t` (the decomp's
  returns `u8*`; cos_sdk returns 0, TODO phase 4 as before), so both modes give the same warnings.
  No forwarder change, no `STATIC_ASSERT` fired. Aurora mode: SSystem, the four JSystem modules,
  framework (rebuilt from clean), `cos_sdk`, smoke, scaffold, header and shadow checks (82 names,
  ok) build; default configuration: `cos_modules` and checks rebuilt (617 steps), 0 errors.
- **2.7 m_Do:** compiles in aurora header mode (all 17 units, 0 errors, rebuilt from clean) and
  still in decomp mode. First aurora build: 9 units failed on 78 errors (none hit clang's
  50-per-unit limit), 12 distinct causes: `OSCreateThread`'s typed entry point (5 units),
  `CARD_ERROR_*` (Aurora spells them `CARD_RESULT_*`, same values) and the TARGET_PC
  `CARDInit(game, maker)`, `OSCalendarTime` field names (`mday`, `mon`, `hour`... in Aurora),
  `DVDDiskID::gameVersion`, the 5-argument `GXSETARRAY`, and `OS_THREAD_QUEUE`/
  `active_threads_link` in `OSGetActiveThreadID`. Central fixes: new
  `native/include/sdk/cos_card_extras.h` (the decomp's `CARD_ERROR_*` enum, valued from Aurora's
  `CARD_RESULT_*`; `dolphin/card.h` is a name both have, so no forwarder can sit in front of it);
  the `dolphin/os/OS.h` forwarder declares `__OSActiveThreadQueue`, which `cos_sdk`
  (`os/OSThread.cpp`) now exports with C linkage instead of keeping a file-local list (same
  object, still under the OS lock). Fixes in `game/src/m_Do`, all under
  `TARGET_PC && defined(COS_SDK_AURORA)` with the original kept: `OSCreateThread` entry points cast
  to `void* (*)(void*)` (as Dusklight; `m_Do_main`, `m_Do_MemCard`, `m_Do_dvd_thread`,
  `m_Do_DVDError`, `m_Do_graphic`); `ThdInit` passes the disc header's game and maker codes to
  Aurora's `CARDInit`, which is what the SDK's `CARDInit` reads; Aurora's field names in
  `m_Do_main`, `m_Do_machine_exception` and `m_Do_MemCardRWmng`; `GXSETARRAY` in `m_Do_ext` with
  the arrays' byte sizes (`mMaxSegments * 2` entries, host-endian) and the `__OSActiveThreadQueue`/
  `linkActive` walk in `m_Do_printf`. No `STATIC_ASSERT` fired. Aurora mode: SSystem, the four
  JSystem modules, framework, m_Do, `cos_sdk`, smoke (20 more runs ok; TSan `threads alarms` 10
  runs, 0 reports), scaffold, header and shadow checks (82 names, ok) build; default
  configuration: `cos_modules` and checks build, 0 errors.
- **2.7 d-core:** compiles in aurora header mode (all 136 units of `src/d`, 0 errors, rebuilt from
  clean) and still in decomp mode. First aurora build: 11 units failed on 67 errors (none hit
  clang's 50-per-unit limit), 5 distinct causes: the THP player types in `d_a_movie_player.h`
  (included by `d_message_paper` and `d_s_title`), the 3-argument `GXSetArray` (29 calls in
  `d_chain`, `d_cloth_packet`, `d_drawlist`, `d_flower`, `d_magma`, `d_menu_cloth`),
  `OSCalendarTime` field names (`d_kyeff`, `d_file_select`) and `GXFogAdjTable::fogVals`
  (`d_kankyo_data`). Central fix: new `native/include/sdk/cos_thp_extras.h` with the decomp's THP
  types (a copy of the game's `dolphin/thp.h`; the name is both trees', and Aurora's declares only
  `THPInit`/`THPVideoDecode` and an extern "C" `THPAudioDecode` that clashes with the game's
  static one), which `d_a_movie_player.h` includes instead of `dolphin/thp.h`; this also clears
  every THP-type error of the `d_a_movie_player` actor (its remaining `OSCreateThread`/
  `DVDFileInfo` errors belong to the actor step). Fixes in `game/src/d`, all under
  `TARGET_PC && defined(COS_SDK_AURORA)` with the original kept: `GXSETARRAY` with each array's
  real byte size (`sizeof` of the static asset arrays, `mFlyGridSize * mHoistGridSize` cXyz for
  `dCloth_packet_c`, `ARR_SIZE` cXyz for `dMenu_Cloth_c`, the `*2`/`*3` set `d_flower`'s
  `field_0x4608..` points at) and host byte order (every array is host-built); Aurora's
  `hour`/`mon`/`mday`/`min`/`sec` and `GXFogAdjTable::r`. No `STATIC_ASSERT` fired. TODO phase 4:
  the THP header/info structs are read straight from big-endian `.thp` data.
  Aurora mode: SSystem, the four JSystem modules, framework, m_Do, d-core, `cos_sdk`, smoke (ok),
  scaffold, header and shadow checks (82 names, ok) build; default configuration: `cos_modules`
  and checks rebuilt (13 steps), 0 errors.
  Review: rerun independently (d-core rebuilt from clean in aurora mode, 136/136 units, 0 errors;
  smoke ok; edited files force-rebuilt in the default decomp configuration, 0 errors).
- **2.7 actors-1:** compiles in aurora header mode (all 74 units, `d_a_acorn_leaf` .. `d_a_fan`,
  0 errors; the first aurora build compiled all 74) and still in decomp mode. First aurora build:
  1 unit failed on 1 error (no unit near clang's 50-per-unit limit): `d_a_bgn` calls the game-only
  `GFSetCullMode` but includes only `dolphin/gf/GFGeometry.h`, a name both trees have, so it
  resolves to Aurora's, which lacks it. Fix, as `cos_gf_extras.h` prescribes: under
  `TARGET_PC && defined(COS_SDK_AURORA)` `d_a_bgn` includes the `dolphin/gf/GF.h` forwarder instead
  (original include kept). No forwarder change, no `STATIC_ASSERT` fired. Aurora mode: SSystem, the
  four JSystem modules, framework, m_Do, d-core, actors-1, `cos_sdk`, smoke (ok), scaffold, header
  and shadow checks (82 names, ok) build; default configuration: `cos_modules` and checks rebuilt
  (1 step), 0 errors.
  Review: rerun independently (actors-1 rebuilt from clean in aurora mode, 74/74 units, 0 errors;
  smoke ok; `d_a_bgn` force-rebuilt in the default decomp configuration, 0 errors).
- **2.7 actors-2:** compiles in aurora header mode (all 74 units, `d_a_floor` .. `d_a_npc_aj1`,
  0 errors, rebuilt from clean) and still in decomp mode. First aurora build: 5 units failed on 20
  errors, 3 causes (no unit near clang's 50-per-unit limit): the 3-argument `GXSetArray` (14 calls
  in `d_a_goal_flag`, `d_a_grid`, `d_a_hookshot`, `d_a_majuu_flag`), `OSCreateThread`'s typed entry
  point (5 calls in `d_a_movie_player`) and `DVDFileInfo::block` (Aurora: `cb`, in
  `d_a_movie_player`'s `DVDCancel`). Fixes, all under `TARGET_PC && defined(COS_SDK_AURORA)` with
  the original kept and a TODO(native phase 2.8): `GXSETARRAY` with each array's real byte size
  (one set of the double-buffered member arrays, `sizeof(arr[0])`, or the whole static asset array;
  host-endian), entry points cast to `void* (*)(void*)` as in m_Do, and `fileInfo.cb`. No forwarder
  change, no `STATIC_ASSERT` fired. Aurora mode: SSystem, the four JSystem modules, framework, m_Do,
  d-core, actors-1, actors-2, `cos_sdk`, smoke (ok), scaffold, header and shadow checks (82 names,
  ok) build; default configuration: `cos_modules` and checks rebuilt (5 steps), 0 errors.
  Review: rerun independently (actors-2 rebuilt from clean in aurora mode, 74/74 units, 0 errors;
  all aurora-mode modules and checks build, smoke ok, shadow check ok; default decomp configuration
  `cos_modules` and checks up to date with `d_a_hookshot` force-rebuilt, 0 errors).
- **2.7 actors-3:** compiles in aurora header mode (all 74 units, `d_a_npc_auction` ..
  `d_a_obj_YLzou`, 0 errors) and still in decomp mode, with no source change: the first aurora
  build compiled all 74 units with 0 errors (no unit near clang's 50-per-unit limit), because the
  forwarders and per-header fixes from the earlier 2.7 steps already cover every SDK use in this
  chunk. No forwarder change, no `STATIC_ASSERT` fired. Aurora mode: SSystem, the four JSystem
  modules, framework, m_Do, d-core, actors-1, actors-2, actors-3, `cos_sdk`, smoke (ok), scaffold,
  header and shadow checks (82 names, ok) build; default configuration: `cos_modules` and checks
  up to date, 0 errors.
  Review: rerun independently (actors-3 rebuilt from clean in aurora mode, 74/74 units, 0 errors,
  units built with `-DTARGET_PC=1`, the Aurora include dir and the forced `cos_sdk_extras.h`; all
  aurora-mode modules and checks build, smoke ok, shadow check ok; default decomp configuration
  `cos_modules` and checks build with `d_a_npc_auction` force-rebuilt, 0 errors).
- **2.7 actors-4:** compiles in aurora header mode (all 74 units, `d_a_obj_barrel2` ..
  `d_a_obj_nest`, 0 errors) and still in decomp mode. The first aurora build failed 2 units with
  8 errors (none near clang's 50-per-unit limit, so nothing hidden), both known idioms:
  `d_a_obj_buoyflag` makes 7 three-argument `GXSetArray` calls, now `GXSETARRAY` with each
  array's real byte size and `le=true` under `TARGET_PC && defined(COS_SDK_AURORA)` (the current
  `DrawVtx_c` buffer's `pos`/`normal`/`backNormal`, the static `Khata`/`Khasi` asset arrays and
  `M_hasi_nrm`; original calls in `#else`, `TODO(native phase 2.8)`); `d_a_obj_mkie` declares a
  local `_GXColor`, which is `GXColor` under `TARGET_PC` (Aurora's `GXColor` has no struct tag).
  No forwarder change, no `STATIC_ASSERT` fired. Aurora mode: SSystem, the four JSystem modules,
  framework, m_Do, d-core, actors-1 to actors-4, `cos_sdk`, smoke (ok), scaffold, header and
  shadow checks (82 names, ok) build; default configuration: `cos_modules` and checks rebuilt
  (2 units), 0 errors.
- **2.7 actors-5:** compiles in aurora header mode (all 74 units, `d_a_obj_ohatch` .. `d_a_ship`,
  0 errors) and still in decomp mode. The first aurora build failed 4 units with 14 errors (none
  near clang's 50-per-unit limit, so nothing hidden), all the three-argument `GXSetArray`: now
  `GXSETARRAY` with each array's real byte size and `le=true` under
  `TARGET_PC && defined(COS_SDK_AURORA)` (original calls in `#else`, `TODO(native phase 2.8)`).
  `d_a_obj_tapestry`: the given `DrawVtx_c` buffer's `pos`/`nrm`/`backNrm`, the static `l_color`
  and `m_draw_data.mTex`; `d_a_pirate_flag` and `d_a_sail`: one set of the double-buffered
  `mPos`/`mNrm`/`mBackNrm` and the static `l_texCoord` asset array; `d_a_sea`: the
  `GRID_CELLS * GRID_CELLS` `m_draw_vtx` buffer allocated in `draw`. No forwarder change, no
  `STATIC_ASSERT` fired. Aurora mode: SSystem, the four JSystem modules, framework, m_Do, d-core,
  actors-1 to actors-5, `cos_sdk`, smoke (ok), scaffold, header and shadow checks (82 names, ok)
  build; default configuration: `cos_modules` and checks rebuilt (4 units), 0 errors.
- **2.7 actors-6:** compiles in aurora header mode with no source change: the first build compiled
  all 71 units (`d_a_shop_item` .. `d_a_yougan`) with 0 errors (only the existing phase 4 warnings:
  null-conversion and int-to-pointer casts). The forwarders and per-header fixes of the earlier 2.7
  steps already cover every SDK use in this chunk. No forwarder change, no `game` change, no
  `STATIC_ASSERT` fired. Aurora mode: SSystem, the four JSystem modules, framework, m_Do, d-core,
  actors-1 to actors-6, `cos_sdk`, smoke (ok), scaffold, header and shadow checks build; default
  configuration: `cos_modules` and checks build with 0 errors.
- **2.8 default switch:** Aurora's headers are now the game's only SDK headers and Aurora is always
  built. `COS_WITH_AURORA` defaults to ON and is required (`GameConfig.cmake` includes
  `Aurora.cmake` first and stops the configure if it is OFF); the `COS_SDK_HEADERS` cache variable
  is gone (a stale `decomp` value stops the configure with a hint to use `cmake --fresh`). The Aurora
  include order, `MTX_USE_PS=1` and the force-included `cos_sdk_extras.h` are unconditional;
  `cos_sdk_shadow_check` always exists and now checks for `native/include/sdk` on the include path
  instead of the removed `COS_SDK_HEADERS_AURORA` marker (`check/sdk_headers.cpp` compiles
  `__start.h`, `Padclamp.h` and the extras asserts unconditionally). Decomp mode removed for
  `TARGET_PC` in `game` too: the `COS_SDK_AURORA` macro is gone and the 87
  `#if TARGET_PC && defined(COS_SDK_AURORA)` guards of step 2.7 (44 files) are plain `#if TARGET_PC`,
  with the original GameCube code still in `#else`; the one `#elif TARGET_PC` decomp-header branch
  (`JKRThread`'s `stack_base`/`stack_end`) is dropped, and the 54 `TODO(native phase 2.8)` notes
  with it. The decomp's own `game/include/dolphin` stays for the GameCube build and is never
  reached under `TARGET_PC`. README (`native/README.md`, `native/sdk/README.md`) updated. Clean
  configure (`cmake --fresh`, default options plus `FETCHCONTENT_SOURCE_DIR_AURORA`) of
  `build/native-mac`: `ninja -k 0 cos_modules cos_sdk cos_sdk_gf cos_sdk_smoke cos_scaffold_check
  cos_sdk_header_check` rc=0, 0 errors, all 840 module objects; `cos_sdk_shadow_check` ok (82 names,
  none under `game/include/dolphin`); `cos_sdk_smoke` prints ok. `build/native-mac-aurora` is
  now redundant.
  Review: rerun independently (`cmake --fresh` of `build/native-mac` with default options plus
  `FETCHCONTENT_SOURCE_DIR_AURORA`; `ninja -k 0 cos_modules cos_sdk cos_sdk_gf cos_sdk_smoke
  cos_scaffold_check cos_sdk_header_check` rc=0, 0 errors, 840/840 module objects; shadow check ok;
  smoke ok; a stale `-DCOS_SDK_HEADERS=decomp` stops the configure as intended).
- **2.9 phase 2 exit:** the census over every non-REL unit (all 424; phase 1 deferred nothing, so
  `cos_deferred.txt` is empty and the list has no deferred entries) now leaves only REL and
  JAudio/JAZel symbols, and the main.dol units have no duplicate strong definition. Before this
  step the census (2.8 defaults) found 137 unresolved (SDK/OS 1, REL 4, JAudio/JAZel 120, other
  12) and 7 duplicate strong symbols. Fixes, each under `#if TARGET_PC` with the GameCube code in
  `#else` where `game` is touched:
  - Duplicates: the explicit specializations of `JPACallBackBase<JPABaseEmitter*>::init/execute/
    executeAfter/draw` and `JPACallBackBase2<JPABaseEmitter*, JPABaseParticle*>::init/execute/draw`
    in `JPAEmitter.h` (defined in each of 181 units). An explicit specialization is inline only if
    it says so itself; MWCC emitted them weak, clang strong. They are declared `inline` on PC.
  - `__OSModuleList` (JUTException, m_Do_printf): new `native/sdk/src/os/OSModule.cpp` defines it
    and `__OSStringTable` once, as the `OSLink.h` forwarder asked; the list stays empty.
  - `dCamera_c::eyePos`, defined `inline` in `d_camera.cpp` and called from `d_ev_camera.cpp`:
    an ordinary definition on PC (MWCC emits an out-of-line copy, clang none for another unit).
  - The 11 `mDoExt_*Packet` constructors `d_debug_viewer.cpp` calls: the decomp builds them only
    `#if DEBUG` (the viewer is `DEBUG_ONLY` in `configure.py`), so `m_Do_ext.cpp` now builds that
    block under `DEBUG || TARGET_PC`, as Dusklight does (`DEBUG || !__MWERKS__`). Its 3 calls to
    the 3-argument `GXSetArray` became `GXSETARRAY` with the real byte size (the 8 cube corners,
    `sizeof(mPoints)` of the quad and triangle packets) and `true`; `GXDrawCylinder`/`GXDrawSphere`
    come from Aurora. The arrow packet then needs `cXyz::atan2sX_Z`, declared in `c_xyz.h` but not
    defined in the retail game: `c_xyz.cpp` defines it on PC (Dusklight's definition, provenance
    in the file).
  - Tooling: `link_census.py prepare` always rewrites `link.rsp` (an unchanged response file left
    the bundle and the report stale after an object changed, since CMake restats custom-command
    outputs); `symbol_census.py --dol [BUILD_DIR]` checks the main.dol units of the census
    (`link_census/objects.txt`, default `build/native-mac`) and exits 1 on a duplicate;
    `COS_LINK_CENSUS_STRICT` now defaults to ON (a duplicate stops the census link), as 2.5 planned.
  Result: `native/check/expected_unresolved_phase2.txt`, 124 symbols, identical to
  `link_census_unresolved.txt`:
  - REL (4, step 3.5): `OSLink`, `OSLinkFixed`, `OSUnlink` (DynamicLink.cpp), `OSSetStringTable`
    (c_dylink.cpp). No `g_profile_*` is referenced by main.dol code.
  - JAudio/JAZel (120, step 3.7 and phase 5): `JAIZelBasic` 72, `JAIZelInst` 15, `JAISound` 8,
    `JAInter` 7 (`BankWave::checkAllWaveLoadStatus`, `SequenceMgr::getArchiveName`/
    `setArchivePointer`, `StreamLib::getNeedBufferSize`/`setAllocBufferCallback`/
    `setDeallocBufferCallback`/`stop`), `JAIBasic` 6, `JAIZelAnime` 4 plus its vtable,
    `JAIAnimeSound` 3, `JAIGlobalParameter` 3, `JASystem::Dvd::sendCmdMsg` 1.
  - SDK, MSL/runtime, deferred and other: 0.
  `symbol_census.py --dol`: 424 objects, 0 duplicate strong definitions, 0 weak data definitions
  with differing sizes, 8 weak code ones (`J2DPicture::setBlendRatio`, `J3DMtxCalcAnm::~J3DMtxCalcAnm`,
  `dBgS_ObjAcch`'s destructors and thunks: per-unit inlining, not layouts). Verification
  (`build/native-mac`, default configuration plus `COS_LINK_CENSUS_STRICT=ON`): `ninja -k 0
  cos_modules cos_sdk cos_sdk_gf cos_sdk_smoke cos_scaffold_check cos_sdk_header_check
  cos_sdk_shadow_check` rc=0, 0 errors, 840 module objects; shadow check ok (82 names); smoke ok;
  `ninja cos_link_census` then `diff -u native/check/expected_unresolved_phase2.txt
  build/native-mac/link_census_unresolved.txt` empty.
  Reviewed in round 1: the module and check targets are up to date with rc=0, smoke ok, the census
  bundle relinked from scratch in strict mode (124 = REL 4 + JAudio/JAZel 120) with an empty diff,
  and `symbol_census.py --dol` reports 424 objects and 0 duplicates (rc=0).
- **Phase 2 done:** the 840 units compile against Aurora's headers only, `cos_sdk` (Dusklight's OS
  glue adapted, alarms on a host timer thread, VI retrace, GX gaps, GF, devices, silent audio
  hardware) plus Aurora resolve every SDK name the main.dol units use, and what is left for the
  link is the REL loader (phase 3, step 3.5) and JAudio/JAZel (step 3.7, phase 5).

## Phase 3 log

- **3.1 Full symbol census:** `symbol_census.py --all` (and `ninja cos_symbol_census`, not in
  `all`) scans 866 objects (424 main.dol, 416 REL = `f_pc_profile_lst` + 415 actors, 26 cos_sdk)
  into `build/native-mac/symbol_census.txt`, REL objects marked `(REL)`, plus a source-level scan of
  types defined at namespace scope in more than one source file; `--dups` exits 1 on duplicates.
  Result: 3 duplicate strong definitions (`ModuleProlog`/`ModuleEpilog` in `DynamicLink.cpp` vs
  `f_pc_profile_lst.cpp`, `hio_set` in `d_a_fganon.cpp` vs `d_a_shand.cpp`) for step 3.2; 8 types
  for step 3.3 (`Attr_c`, `MyScreen`, `NpcDatStruct` x6, `PsoData`, `SafetyCallback`,
  `SaveDatStruct`, `attack_info_s`, `fopMsg_prm_MGameTerm`; `daNpc_Gp1_HIO_c` is only `#if`
  alternatives in one file); weak size mismatches 0 data / 10 code; `d_mesg.cpp` braces do not
  balance for the scan and is listed. Reviewed in round 1: census rc=0, `--dups` rc=1 (3), all
  default targets rc=0, smoke ok, phase 2 unresolved diff empty, `--dol` 0 duplicates.
- **3.2 Duplicate strong symbols:** `ModuleProlog`/`ModuleEpilog` in `f_pc_profile_lst.cpp` under
  `#if !TARGET_PC` (on PC `DynamicLink.cpp`'s empty defaults remain, `REL/executor.c` is not built,
  and step 3.4 sets `g_fpcPf_ProfileList_p`); `hio_set` made `static` under `TARGET_PC` in
  `d_a_fganon.cpp` and `d_a_shand.cpp` after `nm -m` over all objects showed no other reference.
  Reviewed in round 1: `symbol_census.py --all --dups` reports 0 (866 objects), all default
  targets and checks rc=0, phase 2 unresolved diff empty.
- **3.3 ODR audit:** the 8 types the census listed are wrapped under `TARGET_PC` in an unnamed
  namespace per unit: `Attr_c` (`d_a_obj_shmrgrd.cpp` with its `attr()`, `d_wood.cpp` with
  `AttrSway_c`), `NpcDatStruct` (`d_a_auction`, `d_a_npc_photo`, `d_a_npc_roten` and the headers
  `d_a_npc_ah.h`/`mn.h`/`mt.h`, used only by their units' static data), `PsoData`, `SaveDatStruct`,
  `SafetyCallback`, `attack_info_s` (both definitions each) and `fopMsg_prm_MGameTerm` in
  `d_minigame_terminater.cpp`. `MyScreen` was a real merge: `d_file_error.cpp` defines its
  `MyScreen` destructor out of line, so its strong vtable (with the `createPane` override) replaced
  `d_menu_collect.cpp`'s weak one of the same size, which the size check cannot see. That one
  moves to `namespace dMenu_Collect` (`d_menu_collect.h` forward-declares it there and types
  `dMenu_Collect_c::scrn` with it), since the header refers to it. The census gains a section
  "weak definitions overridden by a strong one" for that case. Result: 0 duplicate strong, 0 weak
  data size mismatches, 0 weak overridden, 0 duplicate types (over d/actor alone too). The 10
  weak code size mismatches remain and are not ODR: one inline source optimised differently per
  unit (`setBlendRatio`, `~J3DMtxCalcAnm`, `~dBgS_ObjAcch` and thunks inline the out-of-line
  callees of their own unit) plus the deliberate weak `OSPanic`/`OSVReport` defaults of
  `cos_sdk` (step 2.6b); `d_mesg.cpp`, still unbalanced for the scan, defines no types.
  Reviewed in round 1: census over 866 objects reports 0 duplicate strong, 0 weak data size
  mismatches, 0 weak overridden, 0 duplicate types; an independent scan of d/actor, d, f_op and
  m_Do with `TARGET_PC` on finds 0 duplicate class names outside namespaces (6 with it off), and
  `nm -m` over all objects finds no weak/strong pair. All default targets and checks rc=0, phase 2
  unresolved diff empty.
- **3.4 Static profile list:** under `TARGET_PC`, `f_pc_profile.cpp` initialises
  `g_fpcPf_ProfileList_p = g_fpcPfLst_ProfileList` (constant initialisation, no static
  constructor; `f_pc_profile.h` declares the list) in place of the REL's `ModuleProlog`, and
  `fpcPf_Get` returns NULL for a NULL list or a name outside `[0, fpcNm_MAX_NUM_e)`;
  `fpcBs_Create` returns NULL for a NULL profile (as Dusklight's does) instead of reading through
  it. D5: `f_pc_profile_lst.h` declares each `g_profile_*` with the type its unit defines it with
  (503: 452 actor, 2 `actor_process_profile_definition2` (PLAYER, BG), 16 msg, 12 scene, 10 kankyo,
  9 overlap, 2 camera) and `f_pc_profile_lst.cpp` takes the embedded `process_profile_definition`
  (`.base.base`, `.def.base.base`, camera `.base.base.base`), same order and `VERSION` branches,
  with a `static_assert` that the list has `fpcNm_MAX_NUM_e` entries plus the NULL; the original
  declarations and list stay in the `#else` branches. `g_profile_*` and `g_fpcPfLst_ProfileList`
  are unresolved nowhere over the 866 objects. The main.dol-only link census now also lists `REL
  g_fpcPfLst_ProfileList` (referenced by `f_pc_profile.cpp`, defined in the REL unit), added to
  `expected_unresolved_phase2.txt` (125 symbols).
- **3.5 DynamicLink replaced:** under `TARGET_PC` (adapted from Dusklight's `c_dylink.cpp` and
  `DynamicLink.cpp`, provenance comments in both), `c_dylink.cpp`'s `DynamicNameTable` holds only
  the terminator, `cCc_Init` clears `DMC` and sets `DMC_initialized` without creating the DMC heap
  or any `DynamicModuleControl`, `cDyl_Link`/`cDyl_LinkASync` return `cPhs_COMPLEATE_e` after their
  range check, `cDyl_Unlink` returns FALSE, and `cDyl_InitCallback` neither mounts the DVD drive,
  reads `/dvd/framework.str` (`OSSetStringTable`) nor links `f_pc_profile_lst`: it only sets
  `cDyl_Initialized`. Unlike Dusklight, `cDyl_LinkASync` keeps its `cPhs_INIT_e` return while
  `cDyl_Initialized` is false, and `cDyl_InitAsync` still runs the callback on the DVD thread, so
  the logo scene that `fapGm_Create` requests still waits for it and `d_s_logo` still waits for
  `cDyl_InitAsyncIsDone`. The game has no `cCc_Check` (Dusklight's 0x80000000 pointer test), so there is
  nothing to change there. In `DynamicLink.cpp` the loading and linking (`calcSum2`, `do_load`,
  `do_link`, `do_unlink`: checksum, `OSLink`/`OSLinkFixed`/`OSUnlink`, prolog/epilog) is under
  `!TARGET_PC`; the class shell stays (base link counting, constructor, `do_load_async`,
  `do_unload`, `dump2`, size/type queries, the RELS.arc mount, `Module*` helpers), and on PC
  `do_load`/`do_link`/`do_unlink` report and fail, since the vtable is emitted in this unit and
  nothing creates a DMC. The two dead `TARGET_PC` casts inside the original `do_link` are gone,
  so `unifdef -UTARGET_PC` gives the HEAD code for both files (blank lines and one comment
  aside). `nm -m` over the 866 objects finds no `OSLink`, `OSLinkFixed`, `OSUnlink` or
  `OSSetStringTable`; the link census lists 121 symbols (REL 1, JAudio/JAZel 120), equal to
  `expected_unresolved_phase2.txt` with the four removed; `symbol_census.py --all --dups` 0.
- **3.7a JAudio, first half:** new module `audio` (`modules.cmake`, in `COS_MODULES_READY`) with
  the first sorted half of `src/JSystem/JAudio`, 33 of 66 units (`JAIAnimation.cpp` …
  `JASDSPInterface.cpp`); the second half and `JAZelAudio` follow in later 3.7 steps. All 33
  compile against Aurora's headers, 0 deferred. Changes, each under `TARGET_PC` with the original
  in `#else`: pointer arithmetic through `uintptr_t` (`JAISequenceHeap` stay heap,
  `JAIStreamMgr` header copy), alignment asserts and tests in `JASCalc`/`JASDSPInterface` through
  `uintptr_t`, `AIInitDMA` gets the whole address (cos_sdk's `uintptr_t` stub), the audio thread
  passes `audioproc` with its real type to `OSCreateThread` and reads its `OSMessage` through
  `intptr_t`, and `JAISequenceMgr` compares the `(void*)-1` "still loading" pointer of
  `checkOnMemory` whole instead of against `0xFFFFFFFF` (which a 64-bit -1 never equals).
  `TODO(native phase 4)`: the `aaf` init-data and scene tables relocated through pointer-sized
  fields (`JAIInitData`), and `TDSPChannel`'s u32 owner tag (`alloc`/`free` truncate alike;
  `getLogicalChannel` turns it back into a pointer). `TODO(native phase 5)`: the 32-bit DSP
  addresses given to `DsetupTable`, `DsetDolbyDelay`, `DsyncFrame2` and the stream's DSP buffer.
  `ninja -k 0 audio` 0 errors. The census now covers 899 objects (457 main.dol, 416 REL, 26
  cos_sdk): 0 duplicate strong definitions, 0 weak data size mismatches, 0 duplicate types; the
  link census lists 180 symbols (REL 1, JAudio/JAZel 179: the DSP task functions and the JAS units
  of the second half are now referenced (86 new), 27 JAI symbols are resolved), SDK 0, and
  `expected_unresolved_phase2.txt` is updated to that list.
- **3.7b JAudio, second half:** the `audio` module now globs all of `src/JSystem/JAudio`, 66 units
  (the second sorted half is `JASDriverIF.cpp` … `osdsp_task.c`, 33 units); `JAZelAudio` follows
  in a later 3.7 step. The four DSP units (`dspproc.c`, `dsptask.c`, `osdsp.c`, `osdsp_task.c`)
  are compiled as C++ (`LANGUAGE CXX` in `modules.cmake`), as the decomp's `configure.py` builds
  them with `-lang c++`: their headers are C++ and their non-`extern "C"` functions have C++
  linkage (`DSPReleaseHalt2__FUl`). New `native/include/sdk/cos_dsp_extras.h` declares the SDK's
  DSP task list (`__DSP_{first,last,curr,tmp}_task`, `__DSP_{exec,boot,insert,add,remove}_task`,
  C linkage, all defined by cos_sdk's `DSPStubs.cpp`), which Aurora's `dolphin/dsp.h` lacks;
  `osdsp.c` and `osdsp_task.c` include it under `TARGET_PC`. Changes, each under `TARGET_PC` with
  the original in `#else` (`unifdef -UTARGET_PC` reproduces HEAD): `TSolidHeap`'s base, current
  and last-allocation fields are `uintptr_t` (`streamHeap` hands out main-memory pointers, which
  the u32/int fields truncated) and `init`/`alloc` keep whole addresses; `THeap::initRootHeap` and
  `THeap::alloc` subtract pointers through `uintptr_t`; `ResArcLoader::loadResource` tests its
  `OSMessage` whole; `HardStream::setLastAddr` reads Aurora's `DVDFileInfo::startAddr` (the game:
  `start_address`); `TTrack::loadTbl` returns 0 for an unknown table type instead of falling off
  the end (undefined in C++). `TODO(native phase 4)`: `TSeqParser`'s address registers
  (0x28..0x2b) are 32-bit, both where `cmdJmp` starts a sequence from one and in the `cmdPrintf`
  debug print. `ninja -k 0 audio` 0 errors, 0 deferred. The census covers 932 objects (490
  main.dol, 416 REL, 26 cos_sdk): 0 duplicate strong definitions, 0 weak data size mismatches, 0
  duplicate types. Note for 3.8: Aurora declares `DSPAddTask` `DECL_WEAK`, so `osdsp.c`'s
  definition is weak too and the census lists it with cos_sdk's (weak code, sizes 104 and 272);
  ld64 keeps the first weak definition it sees, so linking the game objects before `cos_sdk`'s
  archive (as 3.8 does) keeps JAudio's. The link census lists 93 symbols (REL 1, JAudio/JAZel 92,
  all `JAIZelBasic`/`JAIZelInst`/`JAIZelAnime`, i.e. `JAZelAudio`), SDK 0, and
  `expected_unresolved_phase2.txt` is updated to that list.
- **3.7b review:** independent rebuild of the touched units, `ninja -k 0` on every default target
  0 errors, link census equal to the expected list (93), census `--dups` clean, `cos_sdk_smoke` ok.
  `cos_sdk_smoke_tsan` exits 139, but its binary predates 3.7a and links no `game` code, so
  it is tracked separately.
- **3.7c JAZelAudio:** the `audio` module now also globs `src/JAZelAudio`, the 8 units of
  `configure.py`'s `JAZelAudio` library, 74 units in all, 0 deferred. Changes, each under
  `TARGET_PC` with the original in `#else` (`unifdef -UTARGET_PC` reproduces HEAD for both files):
  `JAIZelBasic.h` includes `global.h` (`VERSION_SELECT`, `DEMO_SELECT`, `DEAD_STRING`, which the
  decomp's SDK headers bring in and Aurora's do not); one case of `seStart`
  (`JA_SE_CM_INOCHIDAMA_BLINK`/`JA_SE_CM_MAGTAIL_MOVE`) gets its own scope, since C++ forbids the
  next case label from jumping past `dist`'s initialisation. Four real 64-bit bugs: `JAIZelBasic`
  kept pointers in 32-bit fields. `field_0x00d0` (the `Vec*` of `cbPracticePlay`, read back as a
  pointer by `cbPracticeProcess`) is `intptr_t` and tested whole; `field_0x0194[]` (the `Vec*`
  each SE slot was started with, compared by `seStart`) is `uintptr_t`; `field_0x0214`,
  `field_0x1f44`, `field_0x1f48` and `field_0x2064` are `intptr_t`, because `startSoundVec` writes
  a `JAISound*` through `(JAISound**)&field`, which overran the 4-byte `int` on a 64-bit host.
  `ninja -k 0 audio` 0 errors. The link census lists 1 symbol (REL 1: `g_fpcPfLst_ProfileList`,
  defined by the REL unit `f_pc_profile_lst.cpp`), JAudio/JAZel 0, SDK 0, and
  `expected_unresolved_phase2.txt` is updated to it. Over all 940 objects (498 main.dol, 416 REL,
  26 cos_sdk) `nm` finds no undefined JAudio/JAZel/DSP symbol that no object defines, so no trap
  list (`unresolved_traps.txt`, `gen_traps.py`) is needed. `symbol_census.py --all --dups`: 0
  duplicate strong definitions; 0 weak data size mismatches; 0 duplicate types.
- **3.7c review:** independent rebuild of the 8 JAZelAudio units, `ninja -k 0` on every default
  target 0 errors, link census equal to the expected list (REL 1), `unifdef -UTARGET_PC` of both
  touched sources reproduces HEAD, `nm` over all built objects finds no undefined audio symbol, so
  no trap list; census `--dups` clean.
- **3.8 Executable `centollos`:** new `native/cmake/executable.cmake` (not in `all`) links one generated
  stub plus every module's objects through `build/native-mac/cos_exe/objects.rsp`, in order: 424
  main.dol units, the 416 REL units of `rel_units.txt` (`f_pc_profile_lst` + 415 actors), then the
  74 audio units; with `cos_sdk`, the Aurora libraries and `aurora::main`, `LINKER_LANGUAGE CXX`,
  no `-undefined dynamic_lookup`. `m_Do_main.cpp` under `TARGET_PC` includes `<aurora/main.h>`
  (Dusklight CC0 provenance) and takes `char* argv[]` (`unifdef -UTARGET_PC` reproduces HEAD).
  Step 3.6 not needed (0 deferred units). Reviewed in round 1: fresh relink exits 0, `nm -m -u centollos`
  binds all undefined symbols to libSystem/libc++/libobjc/libz/libsqlite3, Apple frameworks, and
  Aurora's Homebrew libpng16/libfreetype (libzstd linked); every default target rc=0, smoke ok,
  link census equal to the expected list, census `--dups` 0. Not run yet (step 3.9).
