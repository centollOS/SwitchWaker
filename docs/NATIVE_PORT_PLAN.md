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

Phase 2 and 3 step plan and decisions: `docs/NATIVE_PORT_PHASE2_3.md`. Phases 4-6: `docs/NATIVE_PORT_PHASE4_6.md`.

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
- **3.9 Static-init smoke:** under `TARGET_PC`, `COS_SMOKE=static-init` makes `main` run
  `pc_smoke_static_init()` before any SDK call and leave with `_Exit` (returning would run the game's
  static destructors, which the GameCube never ran; `~dComIfG_inf_c` reaches a missing
  `JUTGamePad`, TODO(native phase 6)). It checks `g_fpcPf_ProfileList_p == g_fpcPfLst_ProfileList`,
  the NULL at `fpcNm_MAX_NUM_e`, `mProcName == i` for every non-NULL entry and `fpcPf_Get(i)`.
  Two crashes before `main` fixed under `TARGET_PC`: the global `operator new`/`new[]` forms in
  `JKRHeap.cpp` fall back to `aligned_alloc` when no heap is given and none is current, and
  `operator delete` hands pointers no JKRHeap owns to `free` (adapted from Dusklight's
  `fallback_alloc`, CC0; Aurora's static `std::vector<bool>` hit it); `dComIfGs_isStageTbox`
  reads the saved table when no stage is loaded (`daNpc_Tc_HIO_c`'s REL constructor calls it).
  Reviewed in round 1: `unifdef -UTARGET_PC` of the 3 sources reproduces HEAD, every default
  target rc=0, smoke ok, link census unchanged (REL 1), and `COS_SMOKE=static-init centollos` prints
  "502 of 502 profile slots filled, 0 error(s)" and exits 0 on two runs.
- **3.10 Phase 3 log and README:** `native/README.md` gains "The executable `centollos` (phase 3)" (how
  to build `centollos` and run the static-init smoke, the link order of `executable.cmake`, the REL
  loader and profile list changes, the link checks) and a table of every target and whether it is
  in `all`; the stale phase 1 notes (nothing linked, JAudio left out, `JAZelAudio` to follow, the
  expected unresolved list still holding JAudio/JAZel) are brought up to date. `native/sdk/README.md`
  says where the `OSLink*` callers went. `RIGHTS_AND_LICENSES.md` gains "Code adapted from
  Dusklight in the native port": Dusklight is CC0 (`ref/dusklight` at `40457c6`, not in the
  repository), and lists every file with adapted or copied Dusklight code, each of which carries a
  provenance comment (16 in `native/sdk/src`, 7 in `game`); the Aurora MIT notice was already
  there. Final clean build of `build/native-mac` (`ninja -t clean`, then `cmake --fresh` with the
  default options plus `FETCHCONTENT_SOURCE_DIR_AURORA`): configure 0 errors, every module
  "0 deferred"; `ninja -k 0 all` rc=0, 0 errors, in 73 s on 10 cores (Aurora, `cos_sdk`,
  `cos_sdk_gf`, the 914 game objects: 424 main.dol, 416 REL, 74 audio); then `centollos`,
  `cos_link_census`, `cos_symbol_census`, `cos_deferred` and `cos_sdk_shadow_check` rc=0.
  `cos_sdk_smoke` prints ok; shadow check ok (82 names); link census equal to
  `expected_unresolved_phase2.txt` (REL 1: `g_fpcPfLst_ProfileList`); symbol census over 940
  objects (498 main.dol, 416 REL, 26 cos_sdk): 0 duplicate strong, 0 weak data size mismatches,
  0 weak overridden, 0 duplicate types, 11 weak code size mismatches (per-unit inlining of
  `setBlendRatio`, `~J3DMtxCalcAnm` and `~dBgS_ObjAcch` with its thunks, plus the deliberate weak
  `OSPanic`/`OSVReport` of `cos_sdk` and the `DSPAddTask` pair of step 3.7b); `--all --dups` and
  `--dol` exit 0; `centollos` links without `-undefined dynamic_lookup` against system frameworks,
  libc++ and Homebrew's libpng/freetype/zstd only; `COS_SMOKE=static-init build/native-mac/centollos`
  prints "502 of 502 profile slots filled, 0 error(s)" and exits 0 on two runs.
  Reviewed in round 1: docs only; every listed Dusklight file carries its provenance comment;
  a second clean configure and build gives the same results (`all` and the extra targets rc=0 in
  75 s, smoke ok, link census unchanged, 0 duplicate strong, 11 weak code size mismatches,
  static-init 502 of 502).
- **Phase 3 done:** the main.dol units, the 416 REL units (`f_pc_profile_lst` and 415 actors)
  and JAudio/JAZelAudio link statically into one executable, `build/native-mac/centollos`, with
  `cos_sdk` and Aurora and nothing left unresolved; every static constructor runs before `main`
  and the profile list is complete and in order. On the way: 3 duplicate strong symbols removed
  (3.2) and 8 same-named types kept apart, one of them (`MyScreen`) a real silent vtable merge
  (3.3); the profile list is static and typed (3.4, D5); the REL loader is gone while
  `cDyl_InitAsync` keeps the boot order (3.5); JAudio and JAZelAudio compile for real and silent
  (3.7a-c, D4) with the real 64-bit pointer bugs they had fixed, so no trap list was needed; two
  constructors that crashed before `main` fixed (3.9). Steps 3.6 (deferred actors) and 3.7's trap
  generator were not needed. Not done here, by design: the game's `main` without `COS_SMOKE`
  (phase 6), the `TODO(native phase 4)` pointer-in-32-bit-field and big-endian data notes, and
  the `TODO(native phase 5)` DSP and streaming semantics.

## Phase 6 log

Plan, milestones and decisions: `docs/NATIVE_PORT_PHASE4_6.md`.

- **6.0 Run harness:** new static library `cos_pc` from `native/src/pc/pc_*.cpp` (globbed by
  `native/cmake/executable.cmake`, compiled with the game's flags; also linked into the link
  census bundle, so its symbols never show up as unresolved), API `native/include/pc/pc_harness.h`.
  `pc_harness.cpp` reads `COS_DISC`, `COS_SMOKE`, `COS_MILESTONE`, `COS_TIMEOUT_S`, `COS_STALL_S`,
  `COS_TRACE`, `COS_UNCAPPED`, `COS_AUDIO`, `COS_FRAMES` and `COS_RUN_DIR`, rejects unknown
  smoke/milestone names (exit 2) and leaves through `pc_exit` (`_Exit` after flushing stdio
  without blocking on another thread's lock). `pc_milestone.cpp` logs
  `[cos] MILESTONE <name> frame= retrace= ms=` and exits 0 on `COS_MILESTONE`; `pc_frame_tick`
  (called from step 6.2 on) feeds the stall watchdog and `COS_FRAMES`. `pc_crash.cpp`: sigaction
  handler (SEGV/BUS/ILL/FPE/TRAP/ABRT, alternate stack on the main thread) printing the fault
  address with a below-4-GiB truncation hint, scene, frame, retrace count, last resource,
  registers, image load address and a `backtrace_symbols_fd` backtrace of the faulting context
  (pc, lr when it is a caller, then the frame-record chain read with `vm_read_overwrite`, return
  addresses shown as call sites), to stderr and `<COS_RUN_DIR>/backtrace.txt`, exit 13; `pc_panic`
  the same with exit 12. `pc_watchdog.cpp`: exit 10 on timeout, exit 11 when the frame counter is
  frozen for `COS_STALL_S` (from start-up), with every other thread suspended and its frame chain
  written to `stall.txt`. `pc_disc.cpp`: `COS_DISC` must be a readable GameCube image
  (magic 0xC2339F3D) of GZLE01 revision 0, else exit 14 (opening it for the game is step 6.1).
  `pc_smoke.cpp`: `static-init` moved out of `m_Do_main.cpp` unchanged (it now also logs
  milestone M0), plus the harness self-tests `crash-test` (null write through a volatile
  pointer), `panic-test` (`OSPanic`), `stall-test` and `timeout-test`. Game changes, all under
  `TARGET_PC` (`unifdef -UTARGET_PC` of the four files reproduces HEAD): `main` calls
  `pc_harness_init` first; `OSPanic` calls `pc_panic` instead of walking the PowerPC back chain
  (which faults on the host; one `TODO(native phase 4)` marker of the stack-walk group goes) and
  writing to 0x1234567; `fopScn_Create` reports the scene (`pc_trace_scene`) and
  `my_DVDConvertPathToEntrynum` the resource path (`pc_trace_resource`) for the crash report and
  `COS_TRACE=scene,res`. `native/tools/run.sh <target>`: milestone or smoke, default timeout
  180 s and stall 30 s, `COS_AUDIO=off`, disc default `/path/to/GZLE01.iso`
  with its `main.dol` SHA-1 checked against `8d28bab6…` on first use (cached by size and mtime in
  `build/native-mac/runs/disc_check.txt`), run directory `build/native-mac/runs/<target>-<ts>/`
  (`command.txt`, `env.txt`, `run.log`, `exit_code.txt`, `backtrace.txt`/`stall.txt` with `atos`
  file:line names appended), a hard kill 30 s after the timeout (counted as a stall). Found here:
  with developer mode off both `lldb` and `sample` wait for an authorisation prompt, so
  `native/tools/lldb_crash.sh` checks `DevToolsSecurity -status` and exits 3 without starting
  lldb (`--force` tries anyway under its timeout and kills lldb and debugserver), and stalls use
  the in-process thread walk instead of `sample`. `*.iso`/`*.ciso` were already in `.gitignore`.
  Verified: `run.sh static-init` exit 0 (3 runs); `COS_DISC=/nonexistent build/native-mac/centollos`
  exit 14 (and 14 with `COS_DISC` unset); `run.sh crash-test` exit 13 with
  `runSmoke(char const*) (pc_smoke.cpp:85)` and its callers named; `panic-test` 12 (`OSPanic` in
  the backtrace), `timeout-test` 10, `stall-test` 11; `run.sh aurora-up --stall 5` stalls (11)
  with the main thread in `aurora_main` (`m_Do_main.cpp`, the reset-data loop: the arena is not
  set up before step 6.1). Regression: `ninja all centollos cos_sdk_smoke` 0 errors, smoke ok, shadow
  check ok, link census equal to `expected_unresolved_phase2.txt`, `symbol_census.py --all --dups`
  0.
- **6.0 review (round 1):** approved; verification and regression rerun independently (static-init
  0 x3, no disc 14, crash-test 13, panic-test 12, timeout-test 10, stall-test 11, lldb_crash.sh 3
  with developer mode off, census diff empty, `unifdef -UTARGET_PC` of the game files equals HEAD).
- **4.0a Phase 4 inventory:** `native/tools/phase4_inventory.py` scans `{game/include,game/src,
  include,src,sdk}` for `TODO(native phase 4):` markers (a mention without the colon, as in
  `OSContext.cpp`, is not one; `NOTE(native phase 4, harmless):` counts as justified) and sorts
  them into groups tied to the step that clears each: A pointer kept in a u32 field (4.1) 175,
  B heap headers and ARAM callback (4.2) 2, C logo-scene resources (4.8) 5, D stage chunk table
  (4.9a) 7, E dzb (4.10) 1, F J3D (4.11/4.12) 30, G JStudio (4.17) 9, H J2DPrint harmless (4.1)
  4, I debug/JOR/stack walks (4.19) 8, J THP (4.18) 7, K audio and `OSCachedToPhysical` (5.1/5.2)
  13, L JSupport and a ResTIMG site (4.4/4.5) 2; 263 open, 0 unclassified. Finding 1's 255 is
  now 263: JAudio (3.7) brought 12 markers, `d_a_movie_player` has 6 asm-only sites and
  `d_a_player_main` one ResTIMG site the finding did not count, and 6.0 removed one stack walk.
  CMake option `COS_PHASE4_WARNINGS` (default OFF) adds `-Wint-to-pointer-cast
  -Wpointer-to-int-cast -Wint-to-void-pointer-cast -Wreturn-type -Wfortify-source` to every game
  unit; toggling it recompiles them all, and `--log` counts each warning once per
  file:line:column. Baseline `native/check/phase4_baseline.txt` (full `ninja -k 0 centollos` with the
  option on, 0 errors): int-to-pointer-cast 214, pointer-to-int-cast 0 (an error in C++, so none
  survive), int-to-void-pointer-cast 102, return-type 8 (`d_a_bigelf` 2, `d_a_npc_ko1`,
  `d_menu_save`, `d_operate_wind`, `f_op_msg_mng` 3), fortify-source 2 (the `f_op_msg_mng`
  strcpy overflows); the plan's 14 and 6 came from older logs. `--check <baseline>` exits 1 on an
  unclassified marker or any count above the baseline. Decision H8: `-fno-strict-aliasing` is
  in `COS_GAME_COMPILE_OPTIONS` (every game unit and `cos_pc`; not cos_sdk or Aurora).
  Verified: `ninja -k 0 centollos cos_sdk_smoke` with the option on, then `ninja -k 0 all centollos
  cos_sdk_smoke` with it off, 0 errors both times;
  `run.sh static-init` exit 0 (3 runs). Regression: smoke ok, shadow check ok, link census
  equal to `expected_unresolved_phase2.txt`, `symbol_census.py --all --dups` 0.
- **4.0a review (round 1):** approved; rerun independently: warnings-on build 0 errors and
  `--log --check` equal to the baseline (214/0/102/8/2, 263 markers, 0 unclassified); off build
  0 errors, `-fno-strict-aliasing` in all 1170 game compile commands and none of cos_sdk/Aurora;
  smoke ok, shadow check ok, census diff empty, 0 duplicate strong, static-init 0 x3 (502/502).
  Grouping J/K/L beyond finding 1's 255 accepted.
- **4.0b Helpers:** Dusklight's `endian.h`, `endian_gx.hpp`, `endian_ssystem.h` and `offset_ptr.h`
  (CC0, 40457c6, provenance in each file) are in `native/include/helpers/`, and
  `src/helpers/offset_ptr.cpp` is in `native/src/helpers/`, compiled into `cos_pc` (glob
  `src/helpers/*.cpp`). Changes from Dusklight: `endian.h` stops with an `#error` in C units;
  `endian_ssystem.h` also includes `c_xyz.h` (for `cXyz`/`cXy`); `offset_ptr.h` spells out
  `POINTER_ADD`, which the game's `global.h` lacks; `OffsetPtr::setBase` panics through `OSPanic`
  instead of `JUT_ASSERT` (keeps JSystem out of the helper). One bug fix: the range check now
  accepts a positive relative offset only up to `0x3FFF'FFFF`. Bit 31 is the relocated flag and
  the reader takes bit 30 as the sign, so Dusklight's limit of `0x7FFF'FFFF` decoded
  `[0x4000'0000, 0x7FFF'FFFF]` as negative. The GameCube shims in `game/include/helpers/`
  (same four names) define `BE(T)`=`T`, `LE(T)`=`T`, `BE_HOST`, `RES_U16/S16/U32/S32` as no-ops,
  `OFFSET_PTR(T)`=`T*` and `OFFSET_PTR_RAW`=`u32`. They `#error` under `TARGET_PC` and compile
  as C and C++. `check_sdk_shadow.sh` now also fails if a dependency of `sdk_headers.cpp`
  resolves under `game/include/helpers`, or if a name there is not included by it
  (86 names, 4 of them helpers). New `cos_pc_tests` (`native/check/pc_tests.cpp`, built like a
  game unit plus `c_sxyz.cpp`, linked with `cos_pc` and `cos_sdk`) covers:
  - byte order and round trips for u16/s16/u32/s32/u64/f32 (including NaN payload and -0),
    Vec, S16Vec, cXyz, csXyz, cXy, Mtx, the GX enums and vertex lists, `RES_*` and `be_swap`;
  - constant evaluation, and the disc sizes of the fields;
  - `|= &= ^= += -= /=` and post-increment/decrement;
  - `OffsetPtr`: relocation, idempotence (a second `setBase` with the same or another base
    changes nothing), negative and zero relative offsets, both range ends, and the null and
    out-of-range panics (in forked children).
  With the old range check restored, the test fails.
  Verified: `ninja -C build/native-mac cos_pc_tests && build/native-mac/cos_pc_tests` prints
  `ok`. Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests` 0 errors, smoke ok, shadow check
  ok, link census equal to `expected_unresolved_phase2.txt`, `symbol_census.py --all --dups` 0,
  `run.sh static-init` exit 0 (3 runs), inventory unchanged (263 open).
  Review (round 1): accepted, including `OSPanic` in place of the game's `JUT_ASSERT` (which always
  ends in `OSPanic` too); rerun tests ok, range-check mutation fails the test, census equal.
- **4.0c Layout check:** `native/tools/layout_check.py` reads `native/check/layout_headers.txt`
  (131 disc-mapped structs under 28 headers, grouped by the step that owns them: data block
  headers, ResTIMG/ResTLUT/ResNTAB/ResFONT, RARC, BMG, the JPA blocks, the dzs/dzr chunks and
  paths, dzb, the J3D model and animation blocks, event data, STB/FVB) and turns each
  `/* 0xNN */` field comment and `// Size: 0xNN` comment into a `static_assert` on
  `__builtin_offsetof`/`sizeof`, 1050 checks in all. A struct line can add `size=` and
  `<field>=` where the decomp has no comment (`SDIDirEntry`, `SDIFileEntry`, `ResTLUT`,
  `ResNTAB`, the `{num, pointer}` chunk pairs). Headers are read with a small preprocessor
  (TARGET_PC=1, VERSION 2), so the fields are the ones clang compiles; anonymous unions belong to
  the enclosing struct; a comment without a data member is skipped, never guessed (`--list`
  shows them; none in the list). A bitfield gets a check that always fails (bit order differs on
  a little-endian host), so it must be xfailed until it becomes a mask; the list has none.
  `native/check/layout_xfail.txt` is strict: `<struct>` asserts that at least one of its checks
  still fails, `<struct>.<field>` that the check fails, and a name that is not a check stops the
  generator, so a format step must remove what it fixes. It holds 34 structs, 165 failing
  checks, every one caused by a file offset declared as a pointer (8 bytes on the host): RARC
  `SDIFileEntry::data` (4.4), the 19 chunk pairs and `roomRead_data_class` (4.9a-c), `dPath`
  (4.9c/4.16), `cBgD_Grp_t`/`cBgD_t` (4.10), the J3D model blocks (4.11). Offsets cannot see
  endianness: that stays with the sweeps.
  The comments are checked too: `--gc-verify` compiles the same checks, without the xfail list,
  against the decomp's GameCube headers and MSL (no TARGET_PC, so every `#if TARGET_PC` takes its
  `#else`) with clang `--target=powerpc-unknown-eabi`, whose layout rules match MWCC. It found
  four wrong comments, corrected in the headers (comments only, no code or layout change):
  `dStage_Event_dt_c::mName` 0x04 -> 0x01, `J3DJointInitData::mMax` 0x2C -> 0x34 and its size
  0x30 -> 0x40, `J3DMaterialBlock_v21` `mpFogInfo`..`mpNBTScaleInfo` 0x68..0x80 -> 0x5C..0x74.
  CMake (`native/cmake/layout_check.cmake`): the generated unit is
  `build/native-mac/layout_check/cos_layout_check.cpp` (depfile over the tool, both lists and the
  listed headers); `cos_layout_check_host` compiles it with the game's flags plus
  `-fno-access-control` (offsetof of private members) and `-ferror-limit=0`; `cos_layout_check`
  runs it and the GameCube verification (stamp). Neither is in `all`. `--discover` prints the
  host offsets of every failing check, `--write-xfail` rewrites the list, `--scan <header>`
  lists a header's structs to choose new entries.
  Verified: `ninja cos_layout_check` passes; removing `dPath` from the xfail list fails it
  (`dPath.sizeof`), adding `ResTIMG` or `ResTIMG.width` fails it (XPASS), an unknown name stops
  the generator, and a wrong `size=` fails `--gc-verify`. Regression: `ninja all centollos
  cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check cos_link_census` 0 errors,
  smoke ok, `cos_pc_tests` ok, shadow check ok, link census equal to
  `expected_unresolved_phase2.txt`, `symbol_census.py --all --dups` 0, inventory unchanged (263
  open), `run.sh static-init` exit 0 (3 runs).
  Review (round 1): accepted; the four comment fixes went into their own commit first.
  Rerun from a clean `layout_check/`: 826 checks hold, 224 xfailed, 1050 hold on the GameCube;
  dropping an xfail entry, an XPASS entry and a wrong header comment each fail the target, and
  the original comments fail `--gc-verify` on exactly the 10 corrected checks; regression equal.
- **4.0d Disc oracle:** `native/tools/disc_manifest.py` (pure Python, standard library) reads
  the disc on its own: the FST, Yaz0, RARC (nodes, the raw file-table count with `.`/`..`, every
  file with id, flags, size, nested Yaz0 and archives) and the header fields of BMD/BDL/BMT
  (block tags; INF1 packets and vertices; JNT1/MAT3/MAT2/SHP1/TEX1/EVP1/DRW1 counts and name
  tables), the J3D animations (attribute, frame count), BTI (ResTIMG), BFN (INF1/WID1/MAP1/GLY1),
  BMG (INF1 entries, MID1), BLO (block tags, pane counts), JPC (JPAC1-00: emitters with resID,
  block tags and key/field/texture counts; TEX1 names), STB (version, block types and ids),
  dzs/dzr (chunk tags and counts; ACTR/SCOB/TRES/TGOB/TGSC/DOOR/TGDR/PLYR and the layer variants
  with name, parameters, position, angle, set id), dzb (counts, table offsets, vertex bounding
  box) and AAF (sections as `JAInter::InitData::checkInitDataOnMemory` walks them). It writes
  `build/native-mac/disc_manifest.json` (never committed; counts, names, sizes and header
  fields, no file contents) in about 4 s: 2213 files and 177 directories on the disc, 1321
  archives holding 13808 files (9330 J3D, 1849 BTI, 866 dzb, 496 dzr, 155 dzs, 63 BLO, 54 STB),
  0 parse errors. What the disc does that the parser had to allow, recorded per file rather than
  rejected: BMG counts its size in 32-byte units; 86 BTK end their last block up to 0x1F bytes
  past the file (`overrun`); 7 BMT declare more blocks than they hold (`blocks_missing`); one
  text file in the test stage `A_R00` is named `model.bmd`; SHP1 has no name table.
  Disc check (decision H9): the expected SHA-1 of the image and of main.dol live only in
  `disc_manifest.py`; `--verify` checks both on first use and caches the result by path, size
  and mtime in `build/native-mac/runs/disc_check.txt`. `run.sh` calls it in place of its
  own main.dol check (the `.ciso` is refused, exit 14).
  New `COS_SMOKE=disc-ls` (`pc_smoke.cpp`, run by `pc_harness_init` right after the disc check):
  `aurora_dvd_open`, then a recursive `DVDOpenDir`/`DVDReadDir` walk with `DVDFastOpen` sizes into
  `<run dir>/disc_ls.txt` (`D <entry> <path>`, `F <entry> <size> <path>`). For `disc-ls`,
  `run.sh` writes the manifest if it is missing and runs `disc_manifest.py --check-ls`, which
  compares the file and directory counts and each entry number, path and size; a difference
  turns exit 0 into 1.
  Verified: `run.sh disc-ls` exit 0 (3 runs): FST 2213 files, 177 dirs; DVDReadDir 2213
  files, 177 dirs; every entry equal. A listing with one file removed fails `--check-ls`;
  `COS_DISC=/nonexistent` gives 14. Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests
  cos_layout_check cos_sdk_shadow_check cos_link_census` 0 errors, smoke ok, `cos_pc_tests` ok,
  shadow check ok, link census equal to `expected_unresolved_phase2.txt`,
  `symbol_census.py --all --dups` 0, `run.sh static-init` exit 0 (3 runs), inventory
  unchanged (263 open).
- **4.0d review (round 1):** approved; rerun independently: `shasum` of the ISO equals
  `EXPECTED_ISO_SHA1`; `--verify` from an empty cache ok, the `.ciso` refused (14); manifest 0
  parse errors in 4 s; `disc-ls` 0 x2; a dropped line and a size off by one both reported as
  DIFF; `COS_DISC=/nonexistent` 14; build 0 errors, smoke and `cos_pc_tests` ok, census diff
  empty, 0 duplicate strong, static-init 0 x2, crash/panic/stall/timeout-test 13/12/11/10.
  `--check-ls` compares directories by count only (file entries by number, path and size).
- **6.1 Aurora bring-up in `main`:** new `native/src/pc/pc_main.cpp` (`pc_aurora_init`, called
  by `main` right after `pc_harness_init`): `aurora_initialize` with `mem1Size` 256 MiB (H5),
  `mem2Size` 16 MiB, user/cache paths `<exe dir>/user` and `<exe dir>/user/cache`
  (`build/native-mac/user`, ignored), vsync off when `COS_UNCAPPED`, log level info; then
  `aurora_dvd_open(COS_DISC)` and `DVDGetCurrentDiskID` must be GZLE01 version 0 (else 14); then
  `OSInit` (the GameCube's `__start` ran it before `main`; Aurora's needs `mem1Size` first, and
  `main`'s `OSAllocFromArenaLo` needs the arena); `COS_AUDIO=off` calls
  `mDoAud_zelAudio_c::onInitFlag()` (Dusklight's `DUSK_AUDIO_DISABLED`). `main` keeps its order
  (reset data, `g_dComIfG_gameInfo.ct()`, development mode from the disc ID) and, under
  `TARGET_PC`, logs `aurora-up` and calls `main01` on the process main thread instead of
  `OSCreateThread` + suspend (GameCube code in `#else`). `mainThread` must stay the record
  `main01` runs as (`m_Do_ext.cpp` asserts it, `DynamicLink.cpp` checks it), so on PC it is an
  `OSThread&` bound to cos_sdk's default thread (`COSSdkGetDefaultThread`, new, via
  `pc_main_thread`); `main` panics if it does not run as that record. Per-thread current heap:
  `JKRHeap::sCurrentHeap` is `thread_local` on PC (as in Dusklight); the new cos_sdk launch hook
  (`COSSdkSetThreadLaunchHook`, called on the thread whose `OSResumeThread` starts an
  `OSCreateThread` thread; its result goes to the start hook, whose signature gains
  `launchValue`) hands the resuming thread's current heap to the new thread, which is the heap
  the GameCube's single `sCurrentHeap` holds when that thread is first switched to. Host threads
  not made by `OSCreateThread` (Aurora, SDL, Dawn) start with no current heap, so the global
  `operator new` gives them host memory. Not reproduced: `~JKRHeap` moves only the destroying
  thread's current heap off the dead heap (on the GameCube the single value moved for all).
  New cos_sdk smoke test `thread_hooks`.
  Verified: `run.sh aurora-up` exit 0 (4 runs, ~2 s; Metal on Apple M4, window 960x720,
  framebuffer 1920x1440; `[cos] dvd: GZLE01 version 0`), `--uncapped` exit 0 with vsync 0;
  `COS_DISC=/nonexistent` 14. Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests
  cos_layout_check cos_sdk_shadow_check cos_link_census` 0 errors, smoke ok, `cos_pc_tests` ok,
  shadow check ok, census equal to `expected_unresolved_phase2.txt`, `--all --dups` 0,
  static-init 0 x3, disc-ls 0, crash/panic/timeout/stall-test 13/12/10/11, inventory unchanged
  (263). Next (M2, step 4.2): `heaps` faults in `JKRExpHeap::createRoot` (`JKRHeap` constructor
  on the `initArena` result, address 0), the expected PC `initArena` path.
  Review (round 1): rebuilt and reran: aurora-up 0 x3, static-init 0 x2, disc-ls 0, no disc 14,
  crash/panic-test 13/12, smoke (incl. `thread_hooks`) and `cos_pc_tests` ok, census diff empty,
  0 duplicate strong; committed as three commits (cos_sdk hooks, per-thread heap, bring-up).
- **4.1 Clean-up and pointers kept in u32 fields:** everything under `TARGET_PC`, GameCube code in
  `#else` (`unifdef -UTARGET_PC` of every changed file equals HEAD).
  - User areas that hold an actor pointer are `uintptr_t` on the host, as in Dusklight's
    `J3DModel.h:106`: `J3DModel::mUserArea` and its accessors, `J3DPacket::setUserArea`/
    `getUserArea` (the setter used to truncate before `(void*)area`, the getter on the way back),
    `mDoExt_MtxCalcAnmBlendTblOld::mUserArea`/`setUserArea` and its `CalcCallback` first
    parameter, with `daPy_jointBeforeCallback`/`daPy_jointAfterCallback` to match. The 173 call
    sites already passed `uintptr_t` under `TARGET_PC`; only their markers go. The readers cast
    the result to a pointer and need no change.
  - `JGadget::TVector::size` divides by `sizeof(T)` (the `/ 4` was `sizeof(void*)` on the
    GameCube).
  - The 8 `-Wreturn-type` sites return explicitly. The GameCube code was read from the disc
    (main.dol and the RELs) to choose each value: `daBigelf_c::demoProcCom` (FALSE) and
    `demoProc` (`demoProcCom()`, which is what r3 held) are never read by a caller;
    `dMenu_save_c::closeForGameover` with another `endStatus` left r3 = `this`, which
    `closeNormal`'s `rt == TRUE` treats as not done, so FALSE; `daNpc_Ko1_c::btpNum_toResID`
    (`btp`) and `dOperate_wind_c::dOw_angleRegular` (225) also left r3 = `this` on paths no
    caller reaches (`mType` is 0 or 1 on a created actor; the only angle passed is in
    [-270, 90)); the three `dummyfloat*` in `f_op_msg_mng.cpp` are never called (0).
  - The two `-Wfortify-source` overflows in `f_op_msg_mng.cpp`: `tag_len_num_input`'s
    `char buf[12]` takes "000 Rupee(s)" (13 bytes) and `tag_num_input`'s `char buf[8]` takes
    " Rupee(s)" (10 bytes); the GameCube overran its stack frame, the host's fortified `strcpy`
    would abort. Both buffers are 16 bytes on the host.
  - The 4 `J2DPrint` markers become `NOTE(native phase 4, harmless)`: only the distance between
    two pointers into one string is used, and its low 32 bits are the whole distance.
  Inventory: group A 175 -> 0, H 4 -> 0 (4 justified), 263 -> 84 open; warnings (option on):
  int-to-pointer-cast 214 -> 37, int-to-void-pointer-cast 102 -> 100, return-type 8 -> 0,
  fortify-source 2 -> 0; `native/check/phase4_baseline.txt` regenerated. Not in this step (the
  remaining int-to-pointer casts belong to other groups, except one): `kankyo_class::mParam`
  (u32, from `fopKyM_create`'s `int` parameter) carries the ship's `this` to `dWindArrow_c::draw`,
  which dereferences it; it truncates on the host and needs its own fix before sailing.
  Verified: `ninja -k 0 centollos cos_sdk_smoke` with `COS_PHASE4_WARNINGS=ON` 0 errors and
  `phase4_inventory.py --log --check` ok; then with the option off `ninja -k 0 all centollos
  cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check cos_link_census` 0 errors.
  Regression: smoke ok, `cos_pc_tests` ok, layout check ok, census equal to
  `expected_unresolved_phase2.txt`, `--all --dups` 0, static-init 0 x3 (M0), aurora-up 0 x3
  (M1), disc-ls 0, crash/panic-test 13/12, no disc 14.
- **4.1 review (round 1):** approved; rerun independently: `unifdef -UTARGET_PC` of every changed
  file equals HEAD's, actor diffs are marker removals only; warnings-on build 0 errors and
  `--check` ok (A 0, H 0, 84 open); warnings-off build 0 errors, smoke and `cos_pc_tests` ok,
  census diff empty, `--all --dups` 0; static-init 0 x3, aurora-up 0 x2, disc-ls 0, crash-test 13.
  Committed as five commits (user areas, `TVector::size`, missing returns, `f_op_msg_mng`
  buffers, `J2DPrint` notes) plus this log and the baseline.
- **4.2 Heaps:** everything under `TARGET_PC`, GameCube code in `#else` (`unifdef -UTARGET_PC` of
  every changed game file equals HEAD's).
  - `JKRExpHeap::CMemBlock` is padded to 0x20 on the host (Dusklight's `_pad`, with a
    `static_assert`): with 8-byte pointers the fields end at 0x18, and a header that is not a
    multiple of 16 breaks `JKRHeap::getMaxAllocatableSize`, which takes a block's content to have
    the block's address modulo 16 (masks it with 0xf), so `JKRExpHeap::create(-1, ...)` could be
    promised a size it is then refused. `getBlock` subtracts `sizeof(CMemBlock)`. The two header
    literals of `JKRExpHeap.cpp` become `sizeof(CMemBlock)` (`do_freeAll`) and
    `expHeapSize + sizeof(CMemBlock)` (`create`'s minimum, 0xa0 on the GameCube); every other 0x10
    there is an alignment and stays.
  - Host addresses held in 32 bits in the heap code: `JKRExpHeap::allocFromTail(size, align)`
    built the block pointer from a `u32 start`; `JKRSolidHeap::allocFromHead`/`allocFromTail`
    returned a `u32 alignedStart` as the pointer; `JKRExpHeap::joinTwoBlocks` compared block
    addresses as `u32` (wrong across a 4 GiB boundary); `JKRHeap::dispose_subroutine` took the
    range as `u32`, so `freeTail`/`dispose(ptr, size)` never found the disposers in it. All
    `uintptr_t` now (and `allocFromHead(size, align)`'s aligned content address).
  - `JKRAMCommand::AsyncCallback` and `JKRDecompCommand::AsyncCallback` take `uintptr_t`: both
    are called with the command's address (`JKRDecomp` passed `(uintptr_t)command` into the `u32`
    parameter). No caller passes a callback yet; group B 2 -> 0.
  - The global `operator new`/`new[]` without an alignment pass `__STDCPP_DEFAULT_NEW_ALIGNMENT__`
    (16) instead of 4 (Dusklight passes `alignof(max_align_t)`): the host's operator new must
    return memory aligned for any type and the compiler assumes it. The forms with an alignment
    keep the game's. The fallback to the host allocator before any heap exists (static
    constructors, Aurora's threads) is the one of step 3.9.
  - `JKRHeap::initArena` needs no PC path: Aurora implements `OSInitAlloc`, `OSPhysicalToCached`
    and the boot info over its MEM1 block, so the GameCube code takes the whole arena; the root
    heap lies inside MEM1 (finding 4). The fault step 6.1 logged in `createRoot` no longer occurs
    at HEAD (the run now reaches `JFWSystem::init`).
  - Heap sizes (decision H5, Dusklight's multipliers): in `mDoMch_Create` the arena bounds are
    `uintptr_t` (two group I markers go; Aurora's MEM1 is above 4 GiB, so the GameCube's
    development-console test always lowers the arena by 24 MiB, leaving 232 MiB), the system heap
    is a fixed 32 MiB, command and archive heaps x2, game heap x20: about 109 MiB.
    `fopAcM_entrySolidHeap` doubles each actor's estimate (an estimate with room to spare is
    shrunk by `mDoExt_adjustSolidHeap`, as on the GameCube).
  - Harness: `COS_SMOKE=heap` (`native/src/pc/pc_heap.cpp`, run by `pc_aurora_init` once Aurora
    and `OSInit` are up): `operator new` before any heap gives host memory; `createRoot`
    (`initArena`) inside MEM1; below it the heaps of `mDoMch_Create` with their PC sizes (system,
    main in system, command, archive, game); in each 10,000 random allocations (head and tail,
    alignments 0-0x80), frees and resizes, every block inside its heap, aligned, its contents
    intact, `check()` every 500 operations, `getMaxAllocatableSize(0x10)` allocatable every 50,
    and the free size back to its start once all is freed; `freeTail` disposes the objects of
    tail blocks only; a solid heap (head and tail, every alignment); `operator new` in the current
    heap 16-byte aligned; `freeAll` leaves one free block over the whole heap. Milestone M2:
    `main01` calls `pc_heaps_created` right after `mDoMch_Create` (`check()` on the root, system,
    main, game, archive and command heaps, then `heaps`).
  Verified: `run.sh heap` exit 0 (3 runs, about 1 s). Each fix reverted on its own makes it
  fail: `u32` solid heap pointer exit 1 (blocks outside the heap), `u32` tail start exit 13,
  `u32` dispose range exit 1 (freeTail disposed nothing), operator new alignment 4 exit 1, no
  `CMemBlock` padding exit 1 (`getMaxAllocatableSize(0x10)` refused). **M2 is not reached:**
  `heaps` still ends with exit 13 in `JFWSystem::init` (called by `mDoMch_Create` before the
  command, archive, game and main heaps exist): `JUTResFont::getWidth` reads a NULL
  `mInfoBlock` because the system font's `ResFONT` block tags are read little-endian, which is
  step 4.3's work (its own verification is M3). M2 is to be checked together with step 4.3.
  Inventory: B 2 -> 0, I 8 -> 6, 84 -> 80 open; warnings (option on): int-to-pointer-cast
  37 -> 36, int-to-void-pointer-cast 100 -> 95; `native/check/phase4_baseline.txt` regenerated.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors, smoke ok, `cos_pc_tests` ok, layout check ok, shadow check ok,
  census equal to `expected_unresolved_phase2.txt`, `--all --dups` 0, static-init 0 x3 (M0),
  aurora-up 0 x3 (M1), disc-ls 0, crash/panic-test 13/12, no disc 14.
  Review (round 1): accepted with M2 deferred to step 4.3 (rerun: heap 0 x3, `heaps` 13 at
  `JUTResFont::getWidth` addr 0xe from `JFWSystem::init`, static-init 0 x3, aurora-up 0 x3,
  disc-ls 0, crash/panic 13/12, census equal, inventory check ok, `unifdef -UTARGET_PC` equal
  to HEAD). Committed as six commits (block header, host addresses, async callbacks,
  `operator new` alignment, H5 sizes, harness) plus this log and the baseline.
- **4.3 System font, ResTIMG and console:** struct fields as `BE(T)` (the shim makes them `T` on
  the GameCube: with `BE(T)` expanded, `unifdef -UTARGET_PC` of every changed game file equals
  HEAD's), code changes under `TARGET_PC`.
  - Data block headers: `JUTDataBlockHeader` and `JUTDataFileHeader` are big-endian (as in
    Dusklight's `J3DAnimation.h`). `JUTResFont::countBlock`/`setBlock` read the block tags and
    sizes little-endian, so no INF1 was ever found and `JFWSystem::init` faulted in
    `JUTResFont::getWidth` (the M2 failure of step 4.2). The header is shared by every JSystem
    format (J3D, BMG, BCK...), all disc data. `d_resorce.cpp`'s BCK `mSeAnmOffset` is now read as
    a host value: `(char*)pRes + (u32)mSeAnmOffset` (its group C marker goes).
  - `ResFONT` (INF1/WID1/MAP1/GLY1 and the file header) as in Dusklight's `JUTFont.h`; the MAP1
    table pointers in `getFontCode` and `convertSjis`'s parameter are `BE(u16)*`; `loadImage`
    casts the texture format through `u16`.
  - `ResTIMG` (width, height, numColors, paletteOffset, LODBias, imageOffset; Dusklight's
    `JUTTexture.h`, the offsets kept `u32`), `ResTLUT::numColors`, `ResNTAB` and its entries.
    `JUTTexture::initTexObj`'s `imageOffset ? imageOffset : 0x20` reads the field as `u32` (the
    conditional is ambiguous on a `BE`). The ResTIMG marker of `d_a_player_main.cpp` (group L)
    goes: its `imageOffset` read is now right.
  - `JUTResFONT_Ascfont_fix12` is 32-byte aligned on the host (`ATTRIBUTE_ALIGN(32)` on the
    definition, as in Dusklight): the header's `ALIGN_DECL(32)` is empty off MWCC, and GX reads
    the glyph page in place.
  - Not changed: `JUTCacheFont::setBlock`/`getFontFromAram` read the block tags and sizes through
    raw `int*` (`*pData`, `pData[1]`); it serves the fonts `mDoExt` loads from archives
    (step 4.6).
  - Layout check: `layout_check.py` read `BE(u16) width;` as a function declaration and skipped
    it, dropping 40 checks once the fields became `BE`; it now reads `BE`/`LE`/`OFFSET_PTR(_V0)`
    as a type (1050 checks again, the same list as before the change).
  - Milestone M3: `LOAD_COPYDATE` (run by the DVD thread, queued by `main01` after `mDoGph_Create`
    and `mDoCPd_Create`) calls `pc_copydate_loaded`, which logs the date and `gfx-create` when
    `/COPYDATE` was read.
  - Harness: `COS_SMOKE=font` (`native/src/pc/pc_font.cpp`, run by `pc_heaps_created` once M2's
    checks held, so after `JFWSystem::init`): the system font and `/res/Menu/kanfont_fix16.bfn`
    (Shift-JIS, 27 MAP1 blocks of methods 1 and 2, read into the game heap) against an
    independent reading of the same bytes (big-endian loads at the BFN offsets, no struct):
    block counts, INF1 through the accessors, `getFontCode` and `getWidthEntry` for every mapped
    code (256 and 4902), `loadImage`'s cell and texture object (data, size, format),
    `getWidth('A')`; then a `JUTConsole` with the system font prints a line (found in its
    buffer) and draws it in one Aurora frame, whose counters must show draw calls and a texture
    upload (2 draws, 131072 bytes: the fill box has no texture, the upload is the glyph page).
    It writes `font.txt` (what `JUTResFont` read from the disc font), which `run.sh` compares
    with the manifest (`disc_manifest.py --check-font`: block counts, INF1, every WID1/MAP1/GLY1
    field).
  Verified: `run.sh font` exit 0 (3 runs; manifest equal, 30 block headers), `heaps` exit 0
  (3 runs, M2 now reached), `gfx-create` exit 0 (3 runs, COPYDATE "03/02/19 11:43:53"). The test
  fails with GLY1 `numRows` and WID1 `endCode` left little-endian (cell positions), and without
  the font's alignment; `--check-font` reports a changed MAP1 entry count. Next: `frame-loop`
  stalls (frame counter at 0; step 6.2).
  Inventory: C 5 -> 4, L 2 -> 1, 80 -> 78 open; warnings (option on): int-to-pointer-cast
  36 -> 35; `native/check/phase4_baseline.txt` regenerated.
  Regression: `ninja -k 0 centollos cos_sdk_smoke` with `COS_PHASE4_WARNINGS=ON` 0 errors and
  `--log --check` ok; with it off `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check
  cos_sdk_shadow_check cos_link_census` 0 errors, smoke ok, `cos_pc_tests` ok, layout check ok
  (1050 hold on the GameCube), shadow check ok, census equal to
  `expected_unresolved_phase2.txt`, `--all --dups` 0, static-init 0 x3 (M0), aurora-up 0 x3 (M1),
  disc-ls 0, heap 0, crash/panic/timeout/stall-test 13/12/10/11, no disc 14.
- **4.3 review (round 1):** approved; rerun independently: targets up to date with 0 errors,
  smoke and `cos_pc_tests` ok, census diff empty, layout `--gc-verify` 1050 hold, inventory
  `--check` ok (78 open); font 0 x3 (manifest equal, 4902 codes, 2 draws), heaps 0 x3,
  gfx-create 0 x3, static-init 0 x3, aurora-up 0 x3, disc-ls 0, heap 0, crash/panic 13/12.
  Committed as six commits (layout check, block headers, ResFONT, ResTIMG/TLUT/NTAB, font
  alignment, M3 and font harness) plus this log and the baseline.
- **4.4 Archives:** struct fields as `BE(T)`, code changes under `TARGET_PC` (with `BE(T)` and
  `JKAR_DATA(e)` expanded to `T` and `e->data`, `unifdef -UTARGET_PC` of every changed game file
  equals HEAD's apart from the added include, comments and the GameCube `JKAR_DATA` definition).
  - RARC tables: `SArcHeader`, `SArcDataInfo`, `SDIDirEntry` and `SDIFileEntry` are big-endian (as
    in Dusklight's `JKRArchive.h`). The entry's `void* data` (written in place into the loaded file
    table) is `u32 index` on PC, and the resource pointer lives in `JKRArchive::mFileData`, read
    through `JKAR_DATA(entry)` (Dusklight's macro; `entry->data` on the GameCube) at every site of
    the archive classes. `initFileDataPointers` numbers the entries and clears the table in each
    `open()` (Mem x2, Dvd, Aram, Comp). Unlike Dusklight (system heap) the table comes from the
    archive's heap, where the GameCube kept these pointers: the system heap keeps only 64 KiB
    beside the main heap. A failed allocation fails the mount like `open()`'s other allocations
    (mDoDvdThd then retries in the main heap). Freed by `~JKRArchive` and by
    `JKRMemArchive::unmountFixed` (d_s_name keeps the object after it); `~JKRCompArchive` skips
    the resource loop when the table was never made.
  - Yaz0 headers: `SYaz0Header` is big-endian (the rippers' `decompSZS_subroutine` read `length`
    little-endian); `JKRDecomp::decodeSZS` reads the expanded size and the magic big-endian; the
    `((u32*)data)[1]` sizes of `JKRDecompressFromDVD`/`FromAramToMainRam` use
    `JKRDecompExpandSize` (and none when the first read failed). These were latent on this disc:
    `maxDest` (the caller's correct size) clamps the end pointer and `decodeSZS` stops on its
    length count, so the sweep also passes with them reverted; they are fixed for correctness.
  - ARAM transfers: `JKRAMCommand::mSrc`/`mDst`, `JKRAramPiece::prepareCommand`/`orderAsync`/
    `orderSync` and `JKRAramPcs` take `uintptr_t` (as in Dusklight): one side is a main-memory
    host pointer, which every caller already cast to `uintptr_t` and the `u32` parameter
    truncated; `firstSrcData`'s destination is `uintptr_t`; `orderAsync`'s report prints them with
    `%lx`. `JKRADCommand::mCallback`/`loadToAram_Async` take `uintptr_t` (the command's address,
    as step 4.2 did for `JKRAMCommand`; no caller passes one yet).
  - `JKRCompArchive::field_0x64` (the host address of the main-memory part) is `intptr_t` (as in
    Dusklight): an `int` truncated it.
  - `JKRDvdFile::doneProcess` finds `mDvdFile` at `offsetof(mDvdFile) - offsetof(mFileInfo)` past
    the `DVDFileInfo` (as in Dusklight): 0x3C is the GameCube distance, Aurora's `DVDFileInfo` is
    larger. Every asynchronous `JKRDvdFile::readData` (the DVD-to-ARAM stream) went through it.
  - Host bookkeeping off the game heaps (its own commit, before the archive fixes): one sweep
    run in 11 ended with exit 13, `FATAL: a blocking OS call was made from an alarm handler`.
    The alarm thread's `state.pending.erase` freed a map node through the global operator
    delete, which on PC is the game's (`JKRHeap.cpp`: `JKRExpHeap::do_free` takes the heap's
    `OSMutex`) while the main thread held that mutex. New `cos_sdk/host_alloc.h`
    (`HostAllocator`, `HostNew`/`HostDelete`, `HostMakeShared`/`HostMakeUnique`, `HostString`/
    `HostVector`/`HostMap`/`HostUnorderedMap`, over malloc/free). cos_sdk's alarm queue and
    pending table, the thread side table, `HostThread`/`ForeignThread`/launch records and the OS
    lock use it. The alarm timer thread is a pthread, because libc++'s `std::thread` allocates its
    start state with global new. The `cos_sdk_smoke` test `host_alloc` replaces the global
    operator new/delete with counting forms. Alarms (one-shot, periodic, cancelled, cancelled by
    tag), an OS thread that is created, run and joined, and an adopted host thread must call them
    0 times. Before the fix the test fails. The sweep's own strings, vectors and maps use the same
    allocator, so they stay out of the heaps it measures. `JKRHeap.cpp`'s TODO(native phase 6)
    now covers only Aurora/SDL/libc++ allocations.
  - Harness: `COS_SMOKE=arc-sweep` (`native/src/pc/pc_arc.cpp`, run by `pc_heaps_created`): every
    `.arc` of the FST (1319 under `/res`, `/RELS.arc`, one under `/Audiores`) read independently
    (DVDReadPrio, a Yaz0 decoder of its own, big-endian loads at the RARC offsets, every
    compressed entry expanded), then mounted with `JKRArchive::mount` in MEM, ARAM, DVD and COMP
    modes into a 48 MiB test heap and compared: node and entry counts, the tree walked through
    `mNodes`/`mFiles`/`mStringTable` (paths, IDs, flags, sizes, offsets), `index`, `getDirEntry`
    of every entry; per file `readResource` before the fetch (the DVD, ARAM and decompression
    paths), `getResource` by path and its bytes (stored where the mode keeps them: MEM, COMP's
    main-memory part; expanded otherwise), `getResSize`, `getExpandedResSize` (COMP answers the
    stored size when it has no expanded-size table, as on the GameCube), `findIdResource`,
    `getResource(0, name)` for unique names, `readResource` after the fetch; d_resorce's lookup
    (`getFirstResource(type)`, the finder, `getResource(type, name)`) for every directory with a
    unique type; after `unmount` the test, system and ARAM heaps back to their free size. The
    archive inside `/res/Stage/Name/Stage.arc` is mounted with `mountFixed` as d_s_name does (its
    heap must be back after `unmountFixed`, before the object is deleted). It writes
    `arc_sweep.txt` (the MEM mount's counts, nodes and files), which `run.sh` compares with the
    manifest (`disc_manifest.py --check-arc`: every archive, nested ones included, counts, nodes,
    every file's path, ID, flags, size, offset and expanded size). Names are percent-encoded there
    (some archive names hold spaces).
  - Manifest: a format's own size field (BLO/BMG/J3D/BFN headers, STB) overwrote the record's
    `size`, the stored size of the archive entry or FST file; it is now `header_size`, and
    `MANIFEST_VERSION` 2 makes `run.sh` rewrite an older manifest. disc-ls and font still
    equal it.
  - Layout check: `JKRArchive::SDIFileEntry` leaves the xfail list; `SYaz0Header` is added
    (132 structs, 1052 checks; `--gc-verify` holds).
  - Not changed: `JKRMemArchive::mountFixed` uses the buffer's address cut to `s32` as the
    archive's entry number (only compared for "already mounted"); `JSupport.h`'s group L marker
    concerns the J3D offset fields declared as pointers (step 4.11).
  Verified: `run.sh arc-sweep` exit 0 on 36 runs in a row (about 5 s each): 1321 archives,
  5284 mounts, 1 nested `mountFixed`, 55106 file checks, 54902 finder lookups, 1054 MiB compared;
  manifest equal (1322 archives, 13808 files). Each fix reverted on its own: `u32` ARAM addresses exit 13 in
  `ARQPostRequest` (JKRAram thread), `int` `field_0x64` exit 13 in
  `JKRMemArchive::fetchResource_subroutine` from `JKRCompArchive::fetchResource`, the 0x3C offset
  exit 13 in `OSSendMessage` on the DVD worker, the side table left in `unmountFixed` exit 1 (456
  bytes lost). Next: 6.2 (`frame-loop` still stalls with the frame counter at 0).
  Inventory: markers unchanged (78 open); warnings (option on): int-to-pointer-cast 35 -> 34,
  int-to-void-pointer-cast 95 -> 90; `native/check/phase4_baseline.txt` regenerated.
  Regression: `ninja -k 0 centollos cos_sdk_smoke` with `COS_PHASE4_WARNINGS=ON` 0 errors and
  `--log --check` ok; with it off `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check
  cos_sdk_shadow_check cos_link_census` 0 errors, smoke ok (`host_alloc` included),
  `cos_pc_tests` ok, layout check ok, census equal to `expected_unresolved_phase2.txt`, `--all --dups` 0, static-init 0 x3 (M0),
  aurora-up 0 x3 (M1), heaps 0 x3 (M2), gfx-create 0 x3 (M3), heap 0, disc-ls 0, font 0,
  crash/panic/timeout/stall-test 13/12/10/11, no disc 14.
  Review (round 2): approved; rerun by the reviewer: `cos_sdk_smoke` (`host_alloc` included) and
  `cos_pc_tests` ok, census equal, `--dups` 0, inventory ok, arc-sweep 0 x22, M0-M3 0 x2,
  heap/disc-ls/font 0. Committed as eight commits (host allocator, RARC BE and side table, Yaz0
  header, ARAM `uintptr_t`, `field_0x64`, `doneProcess` offset, manifest `header_size`, arc-sweep
  harness) plus this log and the baseline.
- **6.2 COS_AUDIO=off wave status** (own commit, a separate root cause from pacing):
  step 6.1's `onInitFlag` leaves audio uninitialised. The logo scene's `phase_0` calls
  `JAIZelBasic::checkFirstWaves`, and with no audio initialised the `JAInter::BankWave`
  status table does not exist, so frame 1 faulted at 0x8. `getWaveLoadStatus` now reports
  a missing table as loaded (2) under `TARGET_PC` (`unifdef -UTARGET_PC` of `JAIBankWave.cpp`
  equals HEAD's). This is Dusklight's `DUSK_AUDIO_DISABLED` guard pattern, needed because
  The game's JAudio1 has no such guards. With `COS_AUDIO=on` the game runs `zelAudio.init` over
  audio data whose formats are still little-endian (step 5.1); its wave wait does pass, but
  that is not a valid boot path before phase 5. Prerequisite for M4 under COS_AUDIO=off.
  Reviewed in round 1 (2026-10-03): guard only reached when the table is missing; `unifdef`
  equal to HEAD; committed on its own.
- **6.2 Frame loop and pacing:** `TARGET_PC` code in the game files, GameCube code in `#else`
  (`unifdef -UTARGET_PC` of `m_Do_main.cpp` and `JFWDisplay.cpp` equals HEAD's).
  - New `native/src/pc/pc_frame.cpp`. `main01`'s loop calls `pc_frame_begin` first: Aurora's
    events (`aurora_update`; a quit request exits 0, or 1 when a milestone or `COS_FRAMES` was
    still expected), then `aurora_begin_frame` (retried every 10 ms while the window cannot
    present, instead of Dusklight's skipped game frame). It calls `pc_frame_end` last:
    `aurora_end_frame`, then `pc_frame_tick` (stall watchdog, `COS_FRAMES`), then milestone M4.
    The pad read, audio, `fapGm_Execute` (GX, and the retraces below) and `debug()` all fall
    inside Aurora's frame.
  - `JFWDisplay`'s `waitForTick`: the console sleeps until `p1` ticks have passed (tick-rate
    mode, an `OSAlarm`), or until JUTVideo's post-retrace message shows `p2` more retraces
    (frame-rate mode). There is no VI interrupt on the host, so the PC code does two things.
    First, `pc_frame_pace` waits out the same period with Dusklight's `Limiter` (CC0,
    `src/dusk/time.h`: `mach_wait_until` up to 2 ms before the target, then a spin) on the
    harness clock; `COS_UNCAPPED` skips it. Second, the retraces the console would count during
    that wait happen right there through `VIWaitForRetrace`: `p2`, or the tick period over
    `PC_RETRACE_PERIOD_NS` (1001/60000 s), rounded and at least 1. Retraces made elsewhere since
    the last wait count toward them. JUTVideo's callbacks therefore show the drawn XFB before
    the frame's exchange, which the double-buffer exchange requires, and the retrace count
    rises like the console's (1 per frame at 60 fps, 2 at 30). JUTVideo's message queue is no
    longer read, since this wait was its only reader.
  - Milestone M4 `frame-loop`: at frame 120 or later, the retrace count has gone up since the
    loop started, and Aurora reported draw calls (the largest `drawCallCount` seen after any
    frame, because its counters arrive asynchronously). `[cos] pacing:` is logged with M4 and at
    the end of `COS_FRAMES`. It gives the wall and requested time, overall and from the end of
    the first wait (frame 1 carries about 165 ms of scene creation), and their ratio.
    `COS_TRACE=frame` logs each frame's `aurora_begin_frame`, work, wait and `aurora_end_frame`
    times.
  - `run.sh run`: boots with neither a milestone nor a smoke test, for `--frames`.
  - `cos_pc` gets SDL3's include directories (headers only) for `<aurora/event.h>`.
  Verified (first attempt, before 4.4; restored on top of the 4.4 commits, no conflict in code):
  - `gfx-create` (M3): exit 0, 3 runs (and 3 more after 4.4).
  - `frame-loop` (M4) capped after 4.4: exit 0, 3 runs, about 2.3 s each. Each logs 120 frames,
    120 retraces and up to 3 draw calls in a frame; no assertion or panic in the log.
  - Pacing after frame 1: wall 1983.7 ms against 1983.4 ms requested (ratio 1.0002). The waits
    measure 16.62 to 16.65 ms each.
  - `--uncapped` M4: exit 0, Aurora present mode Immediate.
  - Uncapped speed is about 105 fps. `aurora_begin_frame` blocks for 7 to 9 ms waiting for the
    previous frame's presentation on the 120 Hz display (`COS_TRACE=frame`), and the game's
    own work is under 0.1 ms. This is for step 6.7.
  **Beyond 6.2 (handed to M5/M6 by the amended acceptance, commit 830d93d):**
  - `Logo.arc` now mounts (the 4.4 RARC fix); the earlier `JUT_ASSERT(418, signature == 'RARC')`
    is gone. `run --frames 600` passes M4 and then ends with exit 13 at frame 243 capped
    (SIGBUS, about 4.3 s) and frame 244 uncapped (SIGSEGV, about 2.3 s), in the logo scene:
    `main01` -> `fapGm_Execute` -> `dvdWaitDraw` -> `dRes_control_c::syncAllRes` ->
    `dRes_info_c::loadResource` -> `J3DAnmLoaderDataBase::load` -> `J3DAnmKeyLoader_v15::load`
    -> `JUTNameTab::setResource`. The name table pointer is the block base plus an offset read
    little-endian (x9 = 0xffffffff94000000: 0x94 swapped and sign-extended). `J3DAnimation.h`
    has no `BE(T)` field yet (J3D animation, step 4.12). Under the boot-loop ownership rule this
    is the next fix of the boot loop toward M5/M6, not part of 6.2: the 600-frame runs left
    6.2's acceptance (amended on 2026-10-03), which is M3, M4, the capped ratio over the first
    120 frames and uncapped M4.
  Regression (after 4.4):
  - `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
    cos_link_census`: 0 errors.
  - smoke ok, `cos_pc_tests` ok.
  - Census equal to `expected_unresolved_phase2.txt`; `--all --dups` 0.
  - Inventory `--check` ok (78 open).
  - static-init, aurora-up and heaps 0 x3; gfx-create 0 x3; disc-ls, heap, font and arc-sweep 0.
  - crash/panic/timeout/stall-test 13/12/10/11; no disc 14.
  Verified against the amended acceptance (fixer, 2026-10-03, same working tree):
  - `gfx-create` (M3) 0 x3 and `frame-loop` (M4) capped 0 x3, no assertion or panic in any log;
    each M4 run logs 120 frames, 120 retraces, up to 3 draw calls.
  - Capped pacing after frame 1 over the first 120 frames: ratio 1.0001, 1.0002, 1.0002
    (wall 1983.5-1983.7 ms against 1983.4 ms requested), well inside 5 percent.
  - `frame-loop --uncapped` 0 x3 (present mode Immediate, vsync=0; ratio about 0.49).
  - Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
    cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; shadow check ok; census equal to
    `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (78 open);
    static-init, aurora-up, heaps 0 x3; heap, disc-ls, font, arc-sweep 0;
    crash/panic/timeout/stall-test 13/12/10/11; no disc 14. `unifdef -UTARGET_PC` of the three
    game files equals HEAD's.
  Reviewed in round 1 (2026-10-03): build 0 errors; M3 0 x3; M4 capped 0 x3, ratio 1.0002,
  1.0002, 1.0008, no assertion or panic; M4 uncapped 0 x3; static-init/aurora-up/heaps 0 x3;
  heap/disc-ls/font/arc-sweep 0; crash/panic/timeout/stall 13/12/10/11, no disc 14; census diff
  empty, `--dups` 0, inventory ok (78 open), smoke and `cos_pc_tests` ok. Accepted.
- M4 reached (boot loop, 2026-10-03, HEAD d12e91c, no code change): `frame-loop` capped 0 x3
  (120 frames, 120 retraces, up to 3 draw calls; ratio 1.0004, 1.0002, 1.0002), uncapped 0;
  static-init/aurora-up/heaps/gfx-create 0 x3; heap/disc-ls/font/arc-sweep 0;
  crash/panic/timeout/stall 13/12/10/11; census diff empty, `--dups` 0, inventory ok (78 open),
  smoke and `cos_pc_tests` ok.
- **4.5 Logo 2D** (M5 logo-scene). The `ResTIMG` image and palette offsets were already read
  big-endian since 4.3 (`imageOffset`/`paletteOffset` are `BE(u32)`; `JUTTexture::storeTIMG` and
  `initTexObj` use them with `intptr_t`), so the logo textures reached Aurora unchanged. Two
  changes, meant as two commits:
  - **`JUtility::TColor`'s u32 form** (root cause, J2DPicture): `set(u32)`/`toUInt32` wrote and
    read `r,g,b,a` as a host-order u32, so on the little-endian host `0xRRGGBBAA` landed as
    `a,b,g,r`. `J2DPicture::draw`/`drawFullSet`/`drawTexCoord` pass the corner colours to
    `GXColor1u32` through that u32 (alpha-faded pictures such as the logo scene's progressive
    choice became opaque, wrongly coloured). Under `TARGET_PC` both go through `BE(u32)`, plus the `TColor(BE(u32))`
    constructor, as in Dusklight 40457c6 (CC0, provenance in the header); `unifdef -UTARGET_PC`
    of `TColor.h` equals HEAD's. `cos_pc_tests` gained `testTColor` (r is the high byte of the
    u32 form, round trips, bytes in memory order).
  - **Milestone M5 `logo-scene`:** `d_s_logo.cpp`'s `phase_2`, at its end under `TARGET_PC`,
    calls the new `pc_logo_scene_created` (pc_harness.h, pc_frame.cpp) with the Logo archive's
    entry count, the boot logo's `ResTIMG` and its resource size. It checks the archive is
    mounted and the header (376x104, image offset at least 0x20, `GXGetTexBufferSize` inside the
    resource), else exit 1; `pc_frame_end` then logs `logo-scene` after the first frame for which
    Aurora reported texture bytes uploaded since the scene was created. `unifdef -UTARGET_PC` of
    `d_s_logo.cpp` equals HEAD's.
  Verified: `run.sh logo-scene` 0 x6 (and `--uncapped` 0): "Logo archive 12 entries;
  the logo BTI 376x104 format 3, 0 colours, image at 0x20 (78208 bytes) in 78240 bytes"
  (the manifest has the same: 12 entries, IA8 376x104, image offset 32, size 78240), created at
  frame 2, 159744 texture bytes uploaded since (0 before), MILESTONE at frame 6; no assertion or
  panic in the log.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; census equal to
  `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (78 open);
  static-init, aurora-up, heaps, gfx-create, frame-loop 0 x3, frame-loop `--uncapped` 0;
  heap, disc-ls, font, arc-sweep 0; crash/panic/timeout/stall-test 13/12/10/11; no disc 14.
  Not changed: the frame-243 fault in `JUTNameTab::setResource` under `dvdWaitDraw` (J3D
  animation, step 4.12) is on the way to M6, not M5.
  Review (round 1): accepted, all of the above rerun (logo-scene 0 x3 and `--uncapped` 0, M0-M4
  0 x3, sweeps 0, harness tests 13/12/10/11, no disc 14, inventory ok). Correction: BLO colours
  were not broken the same way. `JSUInputStream::readU32` does not swap yet, so its host-order
  value was right for the old `set(u32)` and is now byte-reversed. That is latent (no BLO screen
  is parsed correctly before 4.13, `readS16` is unswapped too); 4.13 must make `readU32` big-endian.
- M5 reached (boot loop, 2026-10-03, HEAD 3c9142d, no code change): `logo-scene` 0 x3 (Logo
  archive 12 entries, the logo BTI 376x104 format 3, created at frame 2 or 3, 159744
  texture bytes uploaded since, MILESTONE at frame 6-7), `--uncapped` 0; static-init/aurora-up/
  heaps/gfx-create/frame-loop 0 x3, frame-loop uncapped 0; heap/disc-ls/font/arc-sweep 0;
  crash/panic/timeout/stall 13/12/10/11, no disc 14; census diff empty, `--dups` 0, inventory ok
  (78 open), smoke and `cos_pc_tests` ok.
- **4.6 BMG and disc fonts** (`COS_SMOKE=msg-sweep`). Two root causes, meant as two commits, then
  the sweep:
  - **BMG/BMC container data in host order.** The BMG header and block headers that JMessage
    reads through `TParse_THeader`/`TParse_TBlock` (`*(u32*)`), the `JUTMesgInfo`/`JUTMesgIDData`
    fields, `ga4cSignature` (the `'MESG'` int compared with `memcmp`, so on the host every BMG
    failed the signature and `dMesg_parse` built no resource), `TControl`'s `*(u32*)` entry offset,
    `JMSMesgEntry_c` (offset, number, price, next) and f_op_msg_mng.cpp's `mesg_info` and BMC
    `clt1_header`. Fields are `BE(T)` (struct shim, as Dusklight's JMessage `data.h`); the getters,
    the signature and the two `TControl` reads are under `TARGET_PC`. Without it `getMesgEntry(0)`
    read offset 0x1000000 and number 256 for 0x1 and 1.
  - **Message text tag values in host order.** A control tag is 0x1A, length, group, code u16,
    then parameters, all big-endian. `fopMsgM_messageGet`/`fopMsgM_passwordGet` and
    `d_2dnumber` read length+group+code as `*(u32*)`; `d_mesg`'s four tag parameters (wait time,
    font size) as `*(u16*)`; JMessage's select and branch tables as `u16`/`u32` arrays; and
    `JGadget::binary::TParseValue_endian_big_` (system tag 5's message code, branch counts) was
    raw. All read through `BE(T)` under `TARGET_PC`; `TParseValue_raw_` itself is left to JStudio
    (4.17). Without it `fopMsgM_messageGet` kept the tags that hold the player name (message 13:
    118 bytes instead of 119).
  - **`COS_SMOKE=msg-sweep`** (new `native/src/pc/pc_msg.cpp`, after M2): mounts bmgres, bmgresh,
    fontres and rubyres (MEM, archive heap) and sets them as d_s_logo's phase_2 does, sets the
    player name "Link" (PAL language 0, clear count 0 required). Each BMG is read independently
    (plain big-endian loads), then through the game: `JMessage::TParse` into one container as
    `dMesg_parse`; per resource the header/INF1/DAT1 pointers, counts, group, encoding; for every
    index `TControl::getMessageEntry`/`getMessageData` and a `TRenderingProcessor` run whose
    characters and tags (count and digest) equal the independent walk; every INF1 entry through
    `fopMsgM_msgGet_c::getMesgEntry` (all 24 bytes); per message number routed to that file by
    `fopMsgM's language check`, `fopMsgM_msgGet_c`/`fopMsgM_itemMsgGet_c::getMessage` find
    the first entry with it and `fopMsgM_messageGet` decodes it to the same text as an
    independent decoder; `fopMsgM_getColorTable` for all 256 CLT1 colours; the message fonts
    from `mDoExt_getMesgFont` (rock_24_20_4i_usa.bfn) and `mDoExt_getRubyFont` (the ruby font)
    through the font smoke's checker, now `checkResFont` in pc_font.cpp (every MAP1 code). It
    writes `msg_sweep.txt` (BMG/INF1/DAT1, BMC/CLT1, FONT lines); `run.sh` compares it with
    the manifest (`disc_manifest.py --check-msg`).
  - Manifest version 3: BMG INF1 gets `messages` (entries with text), `distinct_ids` and
    `ids_fnv` (FNV-1a 64 of every entry's offset and number); BMC is a format (size in 32-byte
    units like BMG) with CLT1 `entries`/`colors_fnv`. The font check now shares the BMG/BMC
    check's line reader and finds files inside archives (`<arc>:<file>`); font.txt still equals.
  - `cos_pc_tests` gains `testBmg` (header/block/INF1 getters, `JMSMesgEntry_c` in place and
    copied, `TParseValue_endian_big_`); the layout check adds `JMSMesgEntry_c` (133 structs,
    1070 checks on the GameCube configuration).
  Verified: `run.sh msg-sweep` 0 x7, manifest equal: zel_00 group 0, 4411 entries, 4326
  messages (4326 numbers), 4311 decoded by fopMsgM (15 route to zel_01); zel_01 group 1, 15
  messages, 15 decoded; 4341 through JMessage's processor; 256 colours; rock_24_20_4i_usa.bfn
  226 codes and the ruby font 34755 codes, 0 wrong; about 20 ms. Without the game changes: TParse
  fails on both files and every entry is wrong (exit 1); with only the text-tag read reverted:
  `fopMsgM_messageGet` differs (exit 1). `unifdef -UTARGET_PC` of every changed game file equals
  HEAD's except the `BE(T)` struct fields and their `helpers/endian.h` includes.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; census equal to
  `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (78 open, warnings
  unchanged); static-init, aurora-up, heaps, gfx-create, frame-loop, logo-scene 0 x3,
  frame-loop and logo-scene `--uncapped` 0; heap, disc-ls, font, arc-sweep 0;
  crash/panic/timeout/stall-test 13/12/10/11; no disc 14.
  Review (round 1): accepted and rerun (msg-sweep 0 x3, manifest equal, 4341 messages through
  JMessage, 0 errors; M0-M5 0 x3, uncapped 0; sweeps 0; harness tests 13/12/10/11; census equal,
  dups 0, inventory ok, smoke and `cos_pc_tests` ok); committed as three commits plus this log.
- **4.7 JParticle** (`COS_SMOKE=jpa-sweep`). One root cause, meant as one commit, then the sweep:
  - **JPAC1-00 data read in host order.** `JPAResourceManager` got nothing on the host: the
    archive header (`'JPAC'`/`'1-00'` compared as host u32s) was rejected, so no emitter or
    texture resource was built. Every file-mapped struct now uses `BE(T)` (struct shim, as
    Dusklight's JParticle headers for Dusklight's JPAC2-10, a different layout): the loader's archive,
    `JEFFjpa1` emitter and block headers (`JPAEmitterLoader.cpp`); `JPADynamicsBlockData`,
    `JPAFieldBlockData`, `JPABaseShapeData`, `JPAColorRegAnmKey` (the frame of a colour key),
    `JPAExtraShapeData`, `JPASweepShapeData`, `JPAExTexShapeData` (the indirect matrix as
    `BE(Mtx23)`), with `JGeometry::TVec3<BE(T)>` for the vectors. The KFA1 key data
    (`JPAKeyBlock::getKeyDataPtr`, `JPAGetKeyFrameValue`) and the TDB1 texture indices
    (`JPADataBlockLinkInfo::texDataBase`, `JPADrawContext::pTexIdx`) are `BE(f32)*`/`BE(u16)*`, read
    in place. Under `TARGET_PC`: `JPAFieldBlockArc::getPos`/`getDir` convert member by member
    (`TVec3<f32>::set`), and `JPADrawSetupTev::setupTev` converts the indirect matrix to host floats
    before `GXSetIndTexMtx` (Aurora reads host floats). `unifdef -UTARGET_PC` of every changed game
    file equals HEAD's once `BE(T)` is read as `T` and the `helpers/endian.h` includes dropped
    (`JPAColorRegAnmKey` also gained its offset comments). The layout check adds
    `JPAColorRegAnmKey` (134 structs, 1073 checks on the GameCube configuration).
  - **`COS_SMOKE=jpa-sweep`** (new `native/src/pc/pc_jpa.cpp`, after M2): every JPC under
    `/res/Particle` (DVDReadDir; common.jpc first) loaded as `mDoDvdThd_toMainRam_c` does, read
    independently (plain big-endian loads) and through the game as `dPa_control_c` does:
    `JPAResourceManager` (common.jpc in the sweep heap, each scene in a solid heap adjusted after
    the load), a `JPAEmitterManager` with the game's pool sizes (3000/150/200) whose resource
    manager 1 is the scene's. For every emitter resource: user index, key/field/texture counts,
    block order; every getter of BEM1, FLD1, KFA1 (all key data), BSP1 (texture-animation indices,
    both colour tables at each key frame), ESP1 (with the derived rates), SSP1, ETX1 and TDB1
    against the independent reading, floats by their bits; every texture's name and `ResTIMG`
    through `JPATexture`/`JUTTexture`. Then each emitter alone: `createSimpleEmitterID`, its data
    flags and rate after `JPABaseEmitter::create`, 30 frames of `JPAEmitterManager::calc` with every
    live particle and child finite (position, velocity, age, life), `forceDeleteAllEmitter` (pools
    back to empty); a scene ends with `clearResourceManager(1)` as `removeRoomScene`. It writes
    `jpa_sweep.txt` (JPC/EMTR/TEX lines); `run.sh` compares it with the manifest
    (`disc_manifest.py --check-jpa`: every JPC, every emitter's `res_id`, block/key/field/texture
    counts and tag order, every texture's name, format, size, mipmap count and image offset).
  Verified: `run.sh jpa-sweep` 0 x6, manifest equal: 58 JPC (common.jpc 193 emitters and 96
  textures, 57 scenes), 3370 emitter resources, 1874 textures, 12896 key values; 3370 emitters
  calculated (3049 emit within 30 frames, 341 end early), 96958 emitter frames, up to 2000 live
  particles, about 100 ms; no assertion or panic. Without the game changes: `JPAResourceManager`
  builds nothing for any file (exit 1); with only the key and base-shape changes reverted: BSP1
  getters differ (`getType` 0 for 2, `getBaseSizeX` 4.6e-41 for 1.0) and calc then panics.
  Not exercised: drawing (`JPAEmitterManager::draw`, the indirect matrix's `GXSetIndTexMtx`); that
  runs once the boot loop draws particles.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; census equal to
  `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (78 open); static-init,
  aurora-up, heaps, gfx-create, frame-loop, logo-scene 0 x3, frame-loop and logo-scene
  `--uncapped` 0; heap, disc-ls, font, arc-sweep, msg-sweep 0; crash/panic/timeout/stall-test
  13/12/10/11; no disc 14.
  Review (round 1): accepted and rerun (jpa-sweep 0 x3, manifest equal, 58 JPC, 3370 emitters,
  1874 textures, 0 errors; M0-M5 0 x3, uncapped 0; sweeps 0; harness tests 13/12/10/11, no disc
  14; census equal, dups 0, inventory ok (78 open), smoke and `cos_pc_tests` ok); committed as two
  commits plus this log.
- **4.8 Remaining logo resources** (M6 `logo-res`). The boot loop hit, in order, three root
  causes in files 4.11/4.12 own (fixed only as far as M6 needs, under the ownership rule), then
  step 4.8's own markers. Each is meant as its own commit:
  - **J3D animation blocks read in host order.** `J3DAnmKeyLoader_v15::load` built name tables
    from offsets read little-endian (`JUTNameTab::setResource` faulted on BTK/BPK/BRK). The data
    block structs of `J3DAnimation.h` (`J3DAnm*Data`) use `BE(T)`; their file offsets are
    `J3D_ANM_OFFSET(T)`: `T` on the GameCube, on `TARGET_PC` a big-endian s32 with an explicit
    `void*` cast, so the loader's `(void*)data->m...Offset` calls stay as they are. The key
    tables and value arrays the animations read at run time are 4.12's (not touched).
  - **J3D model blocks: file offsets as host pointers and host-order fields.** In
    `J3DModelLoader.h` the block offsets are `OFFSET_PTR_V0` (`void*` on the GameCube, a BE u32 on
    PC, as Dusklight) and the counts `BE(T)`; `J3DJointBlock`/`J3DShapeBlock` hold BE u32 offsets
    under `TARGET_PC`. `BE(T)` in `J3DModelHierarchy`, `J3DJointInitData` (the transform as
    `J3DTransformInfoData` on PC, converted by `getTransformInfo`), `J3DShapeInitData`,
    `J3DShapeMtxInitData`, `J3DShapeDrawInitData`, `J3DMaterialInitData`, `J3DPatchingInfo`,
    `J3DDisplayListInit`; the factories' index/texture/material-ID tables are `BE(u16)*`, the cull
    modes `BE(GXCullMode)*`, the TEV colours `BE(GXColorS10)*`, `J3DShapeMtx*`'s use-matrix tables
    `BE(u16)*`. The J3DStruct.h infos are host objects too, so on PC the material factory copies
    the tex-matrix, fog, NBT-scale and indirect-matrix infos with their multi-byte members swapped,
    and builds `J3DCurrentMtxInfo` from the two BE words. The vertex attribute format list and the
    vertex descriptor lists become host-order copies on the model's heap (no swap in place, so a
    resource can be loaded twice). Vertex arrays and display lists stay big-endian. Ten structs
    left `layout_xfail.txt`; `J3DModelHierarchy` joined `layout_headers.txt`. Not done (4.11/4.12):
    the MAT2 v21 factory, `J3DClusterBlock`, the envelope/draw-matrix tables
    (`mWEvlpMix*`, `mInvJointMtx`, `mDrawMtxIndex`) the runtime reads, the vertex data the CPU reads.
  - **GD vertex array commands.** `J3DShape::makeVtxArrayCmd` used `GDSetArray`, which is fatal
    in Aurora 3227d76 ("GDSetArray is not supported on Aurora"). On PC it uses `GDSetArraySized`
    (64-bit pointer, size, `le = false`), with `J3DVertexData::getVtxArraySize` (an array ends at
    the next array of the VTX1 block or at its end, recorded by `readVertex`), and
    `kVcdVatDLSize` is 0x180 (Dusklight's value) since the Aurora commands are longer.
  - **Step 4.8's markers (group C 4 -> 2).** `dADM::SetData` relocated ActorDat.bin's block and
    name-table offsets in place into u32 pointers, and read them host-order. On PC `FindTag` reads
    the BE headers and returns file offsets, and a `dADM_CharTbl::SetData(void* base, ...)`
    builds host `char*` tables on the heap holding the file. `ItemTableList`'s count and padding
    are `BE(T)` (`layout_headers.txt`). The two `d_com_inf_game.cpp` markers left are already
    `uintptr_t` (harmless). The fmap check-point data (`dMap_FmapChkPnt`) is read by `d_map` (4.16).
  - **Harness:** `dvdWaitDraw` reports, once, every object archive the logo scene keeps (System,
    Logo, Always, Link, Agb: files vs. converted resources, equal to the manifest's 2/7/113/91/2)
    and the commands (26 archives mounted, 4 files in main RAM); `dComIfG_changeOpeningScene`
    logs M6 on entry (`pc_logo_res_object`, `pc_logo_res_synced`, `pc_opening_scene_called`).
  M6 `logo-res` 0 x3 capped and 0 uncapped (milestone at frame 244, about 0.9-4.4 s).
  **Beyond M6 (blockers for M7, not fixed):**
  - With `COS_AUDIO=off`, `dComIfG_changeOpeningScene` -> `mDoAud_setSceneName` ->
    `JAIZelBasic::setSceneName` -> `talkOut` -> `checkStreamPlaying` faults (address 0x18):
    `JAInter::StreamMgr::streamUpdate` is only allocated by `mDoAud_Create`, which `onInitFlag`
    skips. The game's JAudio1 interface is not usable uninitialised (unlike Dusklight's Z2 in Dusklight), so
    audio-off needs a decision: initialise JAudio without output (needs 5.1's AAF parsing) or
    gate the interface on PC.
  - Probed only with a temporary local guard (reverted): `dScnLogo_Delete` then stops at
    `JUT_ASSERT(288)` in `dPa_modelControl_c`: 128 `mDoExt_J3DModel__create` in the particle
    solid heap (0x16e800, not scaled for 64-bit objects, decision H5).
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; census equal to
  `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (67 open, C 2,
  F 21); static-init, aurora-up, heaps, gfx-create, frame-loop, logo-scene, logo-res 0 x3,
  frame-loop, logo-scene and logo-res `--uncapped` 0; disc-ls, heap, font, arc-sweep, msg-sweep,
  jpa-sweep 0; crash/panic/timeout/stall-test 13/12/10/11; no disc 14.
  Reviewed in round 1 (2026-10-03): `unifdef -UTARGET_PC` leaves the .cpp files as before up to
  the `BE(T)` shims; rerun by the reviewer: build 0 errors, smoke and `cos_pc_tests` ok, census
  equal, `--dups` 0, inventory ok (67 open); M0-M5 0 x3, logo-res 0 x3, frame-loop/logo-scene/
  logo-res `--uncapped` 0, sweeps 0, harness tests 13/12/10/11, no disc 14. Accepted and committed
  as six commits (one cause each) plus this log; the audio-off crash before M7 needs a decision.

- **M6 reached** (`logo-res`, boot loop iteration 1, 2026-10-03): no code change at 69b1894;
  reviewer rerun: logo-res 0 x3 (milestone at frame 243, retrace 244, about 4.34 s; 26 archives
  mounted, 4 files in main RAM, 0 empty), M0-M5 0 x3, frame-loop/logo-scene/logo-res `--uncapped`
  0; sweeps 0; harness tests 13/12/10/11; build 0 errors, smoke and `cos_pc_tests` ok, census
  equal, `--dups` 0, inventory ok (67 open). Next: M7 (`opening`), blocked by the audio-off fault above.

- **4.9a Stage chunk table** (2026-10-03): `COS_SMOKE=stage-sweep` 0 x4; the report equals the
  manifest (651 dzs/dzr files, 6063 chunks). Group D of the inventory 7 -> 0. Causes, each under
  `TARGET_PC` (struct fields through the `BE(T)`/`OFFSET_PTR` shims, so the GameCube types are
  unchanged; `unifdef -UTARGET_PC` of the four .cpp files equals HEAD):
  - **The chunk table was relocated in place into 32-bit pointers, and read host-order.**
    `dStage_nodeHeader` is `{u32 m_tag; BE(int) m_entryNum; OFFSET_PTR_RAW m_offset}` and
    `m_chunkCount` is `BE(int)` (Dusklight's `d_stage.h`). `m_tag` stays raw: `dStage_dt_c_decode`
    compares it with the FuncTable identifier read the same way. The 19 `{num, pointer}` chunk
    structs that overlay `{m_entryNum, m_offset}` get `BE(int)` and `OFFSET_PTR(T)` (RTBL:
    `OFFSET_PTR(OFFSET_PTR(roomRead_data_class))`, `roomRead_data_class::m_rooms`
    `OFFSET_PTR(u8)`, `dStage_dPnt_c::m_pnt_offset` `OFFSET_PTR_RAW`), and `dPath::m_points` is
    `OFFSET_PTR(dPnt)`. `dStage_dt_c_offsetToPtr` calls `setBase(i_data)` (0 stays "no data", as
    the original check), `dStage_roomReadInit` relocates each entry and room list from the file,
    `dStage_pathInfoInit`/`dStage_rpatInfoInit` each path from its PPNT/RPPN entries. A path's
    point offset 0 is valid data (225 paths on the disc), so `OffsetPtr` gains
    `setBaseAllowZero` (provenance note in `offset_ptr.{h,cpp}`, tested in `cos_pc_tests`). The
    20 entries of `layout_xfail.txt` for these structs and `dPath` are gone. The entries' own
    fields (actors, rooms, paths, environment) stay for 4.9b-d.
  - **`createRoomScene` passed its parameter pointer to `fopScnM_CreateReq` as a u32.** On PC
    `fopScnM_CreateReq`/`fopScnM_ReRequest` take `uintptr_t` (Dusklight's signature).
  - **`/res/Menu/Menu1.dat`'s two offsets were relocated in place into 32-bit pointers.**
    `menu_of_scene_class::menu_inf::stage` and `stage_inf::roomPtr` are `OFFSET_PTR`, relocated
    with `setBase(info)` in `phase_2`; the three structs joined `layout_headers.txt`.
  - **Harness:** `pc_stage.cpp`. Every .arc under /res/Stage (705) is mounted in main RAM; its
    stage.dzs or room.dzr is read raw first, then relocated by `dStage_dt_c_offsetToPtr` and read
    through `dStage_fileHeader` (count, tags, entry counts, each offset resolved); every
    `{num, pointer}` struct laid over every chunk; `dStage_dt_c_decode` with a recorder per tag
    (first chunk of the tag, its count, the file); then the game's RTBL/PPNT/PATH/RPPN/RPAT loaders
    through `dStage_dt_c_decode` into a `dStage_stageDt_c` or `dStage_roomDt_c` (690 RTBL entries
    and 1881 paths resolved where the file says). Menu1.dat is relocated as `phase_2` does and its
    40 stages and 468 rooms compared with the file. `disc_manifest.py --check-stage` compares
    `stage_sweep.txt` (STG/CHUNK lines) with the manifest; `run.sh stage-sweep` runs it.
    Negative check: with the RPAT relocation removed the sweep reports 1821 errors and exits 1.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; census equal to
  `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (60 open, D 0);
  static-init, aurora-up, heaps, gfx-create, frame-loop, logo-scene, logo-res 0 x3,
  frame-loop, logo-scene and logo-res `--uncapped` 0; disc-ls, heap, font, arc-sweep, msg-sweep,
  jpa-sweep 0; crash/panic/timeout/stall-test 13/12/10/11; no disc 14.
  Reviewed in round 1: `unifdef -UTARGET_PC` equal to HEAD for the four files, layout check
  (host static asserts and `--gc-verify`, 1021 checks) ok, smoke and `cos_pc_tests` ok, census
  report equal, `--all --dups` 0, inventory ok (60 open, D 0); stage-sweep 0 x2 (651 files, 6063
  chunks, 690 RTBL entries, 1881 paths, Menu1.dat 40/468); M0-M6 0, frame-loop/logo-res
  `--uncapped` 0, disc-ls/heap/font/arc/msg/jpa sweeps 0, crash/panic-test 13/12.

- **4.9b Actor records** (2026-10-03): `COS_SMOKE=stage-sweep` 0 x4; the report equals the
  manifest (651 dzs/dzr files, 6063 chunks, 20207 actor records). M7 not reached: the audio-off
  fault logged under 4.8 (`JAIZelBasic::talkOut` from `mDoAud_setSceneName`, still waiting for
  its decision) stops `opening` before `sea_T` is mounted.
  - **The actor records were read host-order.** ACTR/TGOB/PLYR/ACT0-b/TRE0-b
    (`stage_actor_data_class`), TRES (`stage_tresure_data_class`) and SCOB/TGSC/DOOR/TGDR/SCO0-b
    (`stage_tgsc_data_class`) embed an `fopAcM_prmBase_class`, which is also the host
    `fopAcM_prm_class::base` every actor reads. Under `TARGET_PC` the records hold a new
    `dStage_prmBase_class` (d_stage.h: `BE(u32)`, `BE(cXyz)`, `BE(csXyz)`, `BE(u16)`, same
    offsets, size 0x18) with a conversion to `fopAcM_prmBase_class`, so `fopAcM_prm_class` stays
    host order and the record is swapped once where a chunk loader copies it
    (`dStage_actorInit`'s field copies go through `BE<T>`'s conversions, `dStage_tgscInfoInit`'s
    `appen->base = actor_data->base` through the new operator). The other readers
    (`dStage_playerInit`, `dStage_playerInitIkada`, `dStage_chkPlayerId`,
    `dStage_decodeSearchIkada`, `d_menu_dmap`, `d_a_player_npc`) read the fields through the
    same conversions; no source change was needed there. `unifdef -UTARGET_PC` of d_stage.h equals
    HEAD's except the `helpers/endian_ssystem.h` include (a no-op shim on the GameCube). The three
    record structs were already in `layout_headers.txt` and hold.
  - **Harness:** `pc_stage.cpp` reads the first chunk of each actor tag (the one
    `dStage_dt_c_decode` hands out) through the struct its loader uses, checks each entry's
    address, copies it into an `fopAcM_prm_class` both ways the loaders do, and compares both
    copies bit for bit with the file's big-endian fields (and SCOB-type scale); ACTOR lines
    (name, params, pos, angle, set id) go to `stage_sweep.txt` and `disc_manifest.py
    --check-stage` compares them with the manifest's actors (positions at its 3 decimals; every
    record of the manifest must be reported). Negative check: with HEAD's d_stage.h the sweep
    reports byte-swapped parameters and positions and exits 1.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; layout check ok (1021 GameCube
  checks); census equal to `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory
  `--check` ok (60 open); static-init, aurora-up, heaps, gfx-create, frame-loop, logo-scene,
  logo-res 0 x3; frame-loop, logo-scene, logo-res `--uncapped` 0; disc-ls, heap, font, arc-sweep,
  msg-sweep, jpa-sweep 0.
  Reviewed in round 1: every reader of the three record structs (d_stage.cpp, d_menu_dmap,
  d_a_player_npc) goes through `BE<T>`'s conversions; `unifdef -UTARGET_PC` of d_stage.h equals
  HEAD's but the shim include; rerun: build 0 errors, smoke and `cos_pc_tests` ok, layout
  `--gc-verify` 1021 checks, census equal, `--dups` 0, inventory ok (60 open); stage-sweep 0 x2
  (20207 actor records equal to the manifest); M0-M6 0 x3, frame-loop/logo-scene/logo-res
  `--uncapped` 0, disc-ls/heap/font/arc/msg/jpa sweeps 0, crash/panic-test 13/12. Accepted with
  the sweep half; M7 (`opening`, 13 in `JAIZelBasic::talkOut`) waits for the audio-off decision.

- **4.9c Rooms and files** (2026-10-03): `COS_SMOKE=stage-sweep` 0 x5 (3 on the final build); the
  report equals the manifest (651 dzs/dzr files, 6063 chunks, 20207 actor records, 21443
  room/file/path records). M7 still stops at the audio-off fault of 4.8 (`opening` 13 in
  `JAIZelBasic::talkOut`, unchanged).
  - **The room, file and path records were read host-order.** In `d_stage.h` the STAG
    (`stage_stag_info_class`: planes, particle scene, stage type and schbit words), FILI
    (`dStage_FileList_dt_c`), MULT (`dStage_Mult_info`), AROB/RARO (`stage_arrow_data_class`,
    `BE(cXyz)`/`BE(csXyz)`), 2DMA (`stage_map_info_class`'s 13 floats) and SOND
    (`stage_sound_data::field_0x8`, `BE(Vec)`) fields are `BE(T)`; in `d_path.h` `dPath::m_num`/
    `m_nextID` are `BE(u16)` and, under `TARGET_PC`, `dPnt::m_position` is `BE(cXyz)` (the
    readers copy it into a cXyz; GameCube keeps `Vec`). `daShip_c::checkOutRange` keeps pointers
    to point positions: under `TARGET_PC` they are `BE(cXyz)*` (`unifdef -UTARGET_PC` equals
    HEAD). RTBL entries, SCLS, CAMR/RCAM and EVNT hold only bytes and strings: no change, checked
    by the sweep. Every reader goes through `BE<T>`'s conversions (it compiles without a cast);
    a `-fsyntax-only -Wclass-varargs` pass over every game and harness unit finds no `BE<T>`
    passed to a variadic function. `d_stage.h` and `d_path.h` are in `layout_headers.txt` and hold.
  - **Harness:** `pc_stage.cpp` reads the records of the first chunk of each of these tags
    through the struct its chunk loader uses (STAG/FILI through the node's offset, the others
    through their `{num, pointer}` struct, RTBL after `dStage_roomReadInit`), checks each entry's
    address and the struct size, and writes a REC line per entry; `disc_manifest.py` (manifest
    version 4) decodes the same records at the format's offsets (`STAGE_RECORDS`) and
    `--check-stage` compares every field (an f32 as the f32 its decimal rounds to) and requires
    every record. A chunk with offset 0 is "no data", as the relocation leaves it (four stage.dzs
    have a 2DMA chunk of 1 entry at offset 0). Negative check: with HEAD's `d_stage.h`/`d_path.h`
    the sweep reports 20015 differences and exits 1.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors (layout check 1021 GameCube checks); smoke ok; `cos_pc_tests` ok;
  census equal to `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (60
  open); static-init, aurora-up, heaps, gfx-create, frame-loop, logo-scene, logo-res 0 x3;
  frame-loop, logo-scene, logo-res `--uncapped` 0; disc-ls, heap, font, arc-sweep, msg-sweep,
  jpa-sweep 0.
- **4.9c review (round 1):** approved; rerun independently: build 0 errors, smoke ok,
  `cos_pc_tests` ok, census equal, `--all --dups` 0, `unifdef -UTARGET_PC` of `d_a_ship.cpp` equals
  HEAD, stage-sweep 0 (21443 records equal), static-init..logo-res 0, `--uncapped` 0, sweeps 0;
  M7 `opening` still 13 in `JAIZelBasic::talkOut` (audio-off decision pending).
- **4.9d Environment** (2026-10-03): `COS_SMOKE=stage-sweep` 0 x4 (3 on the final build); the report
  equals the manifest (651 dzs/dzr files, 6063 chunks, 20207 actor records, 24790 room/file/path/
  environment records, 3347 of them new); sea_T's fog and colour values print within their ranges.
  M7 still stops at the audio-off fault of 4.8 (`opening` 13 in `JAIZelBasic::talkOut`, unchanged).
  - **The environment records were read host-order.** `d_kankyo` reads the LGHT/LGTV/Colo/Pale/Virt/
    EnvR chunks in place every frame. In `d_stage.h`, `stage_plight_info_class` and
    `stage_lightvec_info_class` have `BE(Vec) position` and `BE(f32) radius`,
    `stage_pselect_info_class::change_rate` and `stage_palet_info_class::mFogStartZ`/`mFogEndZ` are
    `BE(f32)`, and `stage_vrbox_info_class`'s four leading words are `BE(u32)` (unread, kept
    correct). Colours, indices and EnvR are bytes. Every reader (`plight_set`, `SetBaseLight`,
    `dKy_setLight*`, `envcolor_init`'s change-rate clamp) goes through `BE<T>`'s conversions with
    no source change; `d_kankyo_data.cpp`'s default tables are built through `BE<T>`'s constexpr
    constructor. A `-fsyntax-only -Wclass-varargs` pass over `d_kankyo*`, `d_kyeff` and `d_stage`
    finds no `BE<T>` passed to a variadic function. On the GameCube `BE(T)` is `T`; the layout
    check holds (1021 GameCube checks).
  - **Harness:** `pc_stage.cpp` writes a REC line per entry of the first LGHT, LGTV, Colo, Pale,
    Virt and EnvR chunk, read through the node's offset as their chunk loaders do;
    `disc_manifest.py` (manifest version 5) decodes the same records. For sea_T's `stage.dzs`,
    `checkSeaEnv` runs the game's Pale/Colo/Virt/EnvR/LGHT loaders into a `dStage_stageDt_c`, prints
    every palette (fog start..end, fog/actor/BG0 colours, sky index), colour set and sky to the run
    log, and checks: fog finite with 0 <= start <= end <= 1e6 (palette 19 ends at 200000, past the
    160000 far plane), each 0 or >= 1; change rate in [1/30, 1000] or 0; palette, sky and
    colour-set indices below their chunk counts. sea_T: 33 palettes, fog 0..200000, 5 colour sets
    (change rate 10), 31 skies, 50 environments. Negative check: with HEAD's `d_stage.h` the
    manifest comparison reports 2552 differences and the range check 38 errors (every fog distance
    and change rate is a denormal); exit 1.
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; smoke ok; `cos_pc_tests` ok; census equal to
  `expected_unresolved_phase2.txt`; `--all --dups` 0; inventory `--check` ok (60 open);
  static-init, aurora-up, heaps, gfx-create, frame-loop, logo-scene, logo-res 0 x3; frame-loop,
  logo-scene, logo-res `--uncapped` 0; disc-ls, heap, font, arc-sweep, msg-sweep, jpa-sweep 0.
  Review (round 1): rebuilt, stage-sweep 0 (sea_T fog 0..200000, every value in its range; 128 LGHT,
  492 LGTV, 605 Colo, 1317 Pale, 845 Virt, 841 EnvR REC lines equal the manifest), smoke and
  `cos_pc_tests` ok, `--gc-verify` 1021, census equal, `--dups` 0, inventory ok (60 open), static-init..logo-res and
  sweeps 0, `--uncapped` 0; `opening` 13 in `JAIZelBasic::talkOut` as before.
- **4.13 J2D BLO screens** (2026-10-03): `COS_SMOKE=blo-sweep` 0 x4; the report equals the manifest
  (63 BLO files, nested `dat/file_select.arc` included; 4196 panes).
  - **The screen stream was read host-order.** `J2DScreen::set` builds every pane from a
    `JSUMemoryInputStream` over the big-endian SCRNblo1 file. `JSUInputStream`'s 16- and 32-bit
    reads (`readU16/S16/U32/S32`, `read16b/32b`) now read through `BE(T)` locals, as Dusklight's
    `JSUInputStream.h` does (only J2D uses them); the block headers read whole into structs
    (`J2DPaneHeader`, `J2DPane::J2DScrnBlockHeader`, the file header in `checkSignature`, INF1 in
    `getScreenInformation`, `J2DWindow`'s header words) have `BE(u32)`/`BE(u16)` fields. Tags,
    bounds, colours (through `TColor`'s 0xRRGGBBAA form), spacing and font sizes then come out as on
    the GameCube; `BE(T)` is `T` there. `J2DPane.h`'s two headers join `layout_headers.txt`
    (1027 GameCube checks).
  - **Harness:** new `native/src/pc/pc_blo.cpp` (after M2). Every `.arc` under `/res` is mounted
    in main RAM, or in ARAM when it stores compressed files (the logo scene's `aramMount` archives
    under `/res/Msg`; a MEM mount hands those out still compressed); the nested archive is mounted
    with `mountFixed` as `d_s_name` does; `fontres.arc` stays mounted. Each BLO is read
    independently (big-endian loads: INF1, the BGN1/END1/EXT1 tree, every pane field and its
    kind's fields) and built with `J2DScreen::set(<name>, archive)`; the pane tree in pre-order
    must match in kind, depth and every field; texture references to the same archive must give a
    `JUTTexture` on that ResTIMG (2487 checked; 1769 references to other archives are counted, as
    are the USA disc's missing `rock_24_20_4i*`/`rodb_16_11_4i` fonts); deleting the screen must
    give the test heap back. `blo_sweep.txt` goes to `disc_manifest.py --check-blo` (pane counts
    per file, every BLO of the disc). Negative check: with HEAD's `JSUInputStream.h` every one of the
    4196 panes differs (tags reversed, bounds and rotations byte-swapped); exit 1.
  Verified: build 0 errors; `cos_pc_tests` ok; inventory ok (60 open); logo-res, font, msg-sweep 0.
  Reviewed (round 1): `blo-sweep` 0 (63 BLO, 4196 panes, 0 errors); `regress.sh -j 3` all checks passed.

- **6.3 Input injection** (2026-10-03): `COS_SMOKE=pad-echo` 0 x5 (4 capped, 1 `--uncapped`);
  M10 `file-select` not reachable yet: the boot still stops at M7 (`file-select` 13 in
  `JAIZelBasic::talkOut` from `dComIfG_changeOpeningScene`, the audio-off fault of 4.8), so no
  code logs M10 yet; the step that gets past M7-M9 adds that hook and runs M10 with a START script.
  - **Script:** `COS_INPUT=<file>`, one line per change of state `<frame> <buttons> <stickX>
    <stickY>` (frame = `pc_frame_count()` at `mDoCPd_Read`, strictly increasing, each line holds
    until the next; buttons `-`, a number or `A+B+...` names; raw stick -128..127). A malformed
    script exits 2 with file:line. `run.sh --input PATH`; pad-echo defaults to
    `native/check/input/pad-echo.txt`.
  - **Injection:** `mDoCPd_Read` (`TARGET_PC`) calls `pc_pad_feed` before `JUTGamePad::read`; it
    hands the frame's state to Aurora's virtual pad (`PADSetVirtualStatus(0, ...)`, port 0 from
    frame 0 on, neutral before the first line), so the game reads it through `PADRead`, `PADClamp`,
    `JUTGamePad` and `mDoCPd_Convert` like a controller. L/R also set the analog trigger to 180, as
    Aurora does for a digital trigger. Without `COS_INPUT` nothing changes.
  - **pad-echo** (`pc_input.cpp`, after each `mDoCPd_Read`): checks `g_mDoCPd_cpadInfo[0]` every
    frame against the script (12 mapped buttons held exactly, triggered only on the press frame;
    main stick 0 in the dead zone, sign per axis, value 1 past the clamp; C stick at rest; analog
    L/R 1 with L/R; no pad error), logs each change as `[cos] pad-echo: frame= line= script=
    clamped= hold= trig= stick= value= angle= L= R=` and exits 8 frames after the last line (frame
    80, before the logo scene's hand-over at 243). The default script covers single, combined and
    numeric buttons, a held button, the four full directions, a diagonal, the dead zone, partial
    deflection and button plus stick. Negative checks: without `PADSetVirtualStatus` the test
    reports 172 errors and exits 1; frames out of order or an unknown button name exit 2;
    pad-echo without `COS_INPUT` exits 2.
  - Review (round 1): pad-echo 0 capped and `--uncapped`, `regress.sh -j 3` all passed,
    `unifdef -UTARGET_PC m_Do_controller_pad.cpp` equals HEAD. Step 6.3 stays open for M10.

- **4.15 Save data and memory card** (2026-10-03, decision H2): `COS_SMOKE=save` 0 x4; M10 not
  reachable yet (`opening` still 13 in `JAIZelBasic::talkOut`, the audio fault of 4.8).
  - **The save went to the card host-order.** `memory_to_card`/`card_to_memory` memcpy the
    `dSv_*` parts of `dSv_save_c` into the card buffer and the file select reads that buffer, so
    the parts are now big-endian in memory too (Dusklight's `d_save.h` does the same): in
    `d_save.h`, `BE(T)` for every multi-byte field that goes to the card (status A's life/rupee
    halfwords, status B's save date, floats and wind angles, the item record timer, the reserve
    flags, the map words, info's two halfwords, the priest position (`BE(Vec)` under `TARGET_PC`,
    `getPos` returning a copy; `dComIfGs_getPlayerPriestPos` too, and `d_a_npc_cb1`/`d_a_npc_md`
    pass a local copy to `dComIfGs_setRestartOption`) and angle, `dSv_memBit_c`'s words,
    `dSv_ocean_c`). Runtime-only state (zones, `dSv_danBit_c`, restart) stays host-order. The
    `mDeathCount` offset comment said 0x10; it is 0x12. In `m_Do_MemCardRWmng.h`, `card_savedata`'s
    save count, data version and checksum, `card_gamedata`'s u64 checksum and `card_pictdata`'s
    `snap_result`/checksum are `BE(T)`; `mDoMemCdRWm_CalcCheckSum` sums `BE(u16)` halfwords as the
    GameCube does. `d_file_select.cpp` reads the life halfwords and the save date of the card
    buffer through `BE(T)` casts. No source change elsewhere: every reader goes through `BE<T>`'s
    conversions. A `-fsyntax-only -Wformat -Wclass-varargs` pass over the 300-odd units that use the
    save finds no `BE<T>` passed to a variadic function. `d_save.h` (the 25 card structs) and
    `m_Do_MemCardRWmng.h` are in `layout_headers.txt` and hold (1181 GameCube checks).
  - **Harness:** `pc_save.cpp`. `prepareSaveSmoke` (from `pc_aurora_init`, before `CARDInit`)
    points slot A at `<run dir>/card/`, a fresh GCI folder. From `pc_heaps_created`: attach through
    `mDoMemCd_UpDate`; `dSv_info_c::init` and distinctive values in every card field above through
    the game's setters; `initdata_to_card` x2 + `memory_to_card` + `mDoMemCdRWm_SetCheckSumGameData`
    and `mDoMemCd_Save` as the name scene does; the `.gci` is then read with plain big-endian loads
    (directory entry GZLE/01/<save file>, 12 blocks, comment at 0x1C00; header title; both
    `card_savedata` copies: save count, version, halfword and byte checksums recomputed; every value
    at its GameCube offset in the packed file); reload through `mDoMemCd_Load`/`LoadSync` must give
    the written bytes back, pass `TestCheckSumGameData`, and `card_to_memory` must give every value
    back through the getters; `memory_to_card` of the reloaded state equals file 1 (but the save
    date it stamps); a second write gives save count 2. Negative checks: with HEAD's `d_save.h` the
    test reports 47 failures (e.g. max life `0x2800`, time 8.9e-41); with HEAD's
    `m_Do_MemCardRWmng.{h,cpp}` 22 (save count `0x01000000`, every checksum). `save 0` is in
    `regress_targets.txt`.
  - Left for later steps: `d_name.cpp` reads two name characters as one `*(u16*)` (name entry,
    M10/M11's boot loop); `card_pictdata::tex_buffer` holds what `GXCopyTex` captured (pictograph
    pictures, phase 6).
  Regression: `ninja all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check
  cos_link_census` 0 errors; `cos_pc_tests` ok; census equal; `--all --dups` 0; inventory `--check`
  ok (60 open); `unifdef -UTARGET_PC` of `d_a_npc_cb1.cpp`, `d_a_npc_md.cpp`, `d_com_inf_game.h`
  equals HEAD.
  Review (round 1): `save` 0 x3, `regress.sh -j 3` all checks passed; M10 stays open until
  the audio fault of 4.8 (another lane) lets `opening` through.

- **4.10 Collision (dzb)** (2026-10-03, lane dzb): `COS_SMOKE=dzb-sweep` 0 x4; the report equals
  the manifest (865 dzb files of the 'DZB ' directories, 379403 vertices, 531864 triangles, 11080
  groups); 69984 grid rays (43242 hits) and 27306 ground-triangle rays hold. M8 is not reachable
  yet: M7 (`opening`) still stops at 13 in `JAIZelBasic::talkOut` (H10, step 5.A).
  - **The dzb was read through 8-byte pointers and host order.** In `c_bg_w.h` the `cBgD_t`
    table offsets are `OFFSET_PTR(T)` and its counts and flag `BE(T)`; `cBgD_Tri_t`, `cBgD_Blk_t`,
    `cBgD_Tree_t`, `cBgD_Ti_t` and `cBgD_Grp_t` fields are `BE(T)` (`m_name` `OFFSET_PTR(char)`,
    scale/translation `BE(cXyz)`, rotation `BE(csXyz)`), as Dusklight's `d_bg_w.h`.
    `cBgS::ConvDzb`'s `TARGET_PC` branch relocates with `setBase` (a vertex offset of 0 stays
    null, as on GameCube: Bwdg's `hsand1.dzb`, whose owner supplies the vertices) and swaps the
    vertex table to host order once (guarded by the 0x80000000 flag), because `cBgW` and the
    movable-BG code read it as host `Vec`; the other tables stay big-endian. The one writer into
    the file, `dBgS_ChangeAttributeCode` (`dBgW::ChangeAttributeCodeByPathPntNo`), takes
    `BE(u32)*` (`u32*` on GameCube). `cBgD_t` and `cBgD_Grp_t` leave `layout_xfail.txt`; a
    `-fsyntax-only` pass over every game and harness unit finds no `BE<T>`/`OffsetPtr` passed to a
    variadic function.
  - **Harness:** `pc_dzb.cpp` mounts every .arc under /res/Stage and /res/Object, fetches the
    'DZB ' files as `dRes_info_c::loadResource` does, checks every field of every table after
    `ConvDzb` against an independent big-endian reading (vertices bit for bit, a second `ConvDzb`
    a no-op), then sets each file into a `dBgW` (GLOBAL_e, as `d_a_bg`) registered in a test
    `dBgS`: a 9 x 9 grid of downward `dBgS_GndChk` rays must hit inside the bounding box (widened
    only by the plane rise over `cM3d_CrossY_Tri_Front`'s 20-unit edge margin) on a triangle
    that contains the ray and gives the hit height; a ray at the centroid of up to 48 reachable
    ground triangles per file must stop on or above it; a MOVE_BG_e copy with an identity matrix
    must give the same hits. `disc_manifest.py --check-dzb` compares counts, offsets, flag and
    bounding box (the disc's extra `dzs/door10.dzb` in ITest61's Stage.arc is not collision).
    Negative checks: with HEAD's `c_bg_w.h`/`c_bg_s.cpp` the sweep dies in `cBgS::ConvDzb`
    (exit 13); without the vertex swap it reports 4236 errors.
  - Review (round 1): dzb-sweep 0 (865 files, report equals the manifest), `regress.sh -j 3`
    all checks passed; accepted with M8 deferred until M7 passes (as 4.9a-d with M7).

- **5.1 Audio data formats** (2026-10-03, pulled ahead of M7 by H10): `COS_SMOKE=audio-parse` 0 x4
  (3 uncapped, 1 capped); `audio_parse.txt` equals the manifest: 2374 sounds in 18 categories, 65
  banks (778 instruments, 787 oscillators, 2463 velocity regions, 7 drum sets with 60 keys), 65
  wave systems (65 groups, 2042 waves, each inside its .aw), 97 sequences (JaiSeqs.arc sizes), 75
  streams (table header equal to the .afc header), 2 scenes, 2 fx scenes (8 lines). Added to
  `regress_targets.txt`.
  - **The audio data was read host-order.** `JASBNKParser.h` (`TOffset`, `TOsc`, `TRand`,
    `TSense`, `TVmap`, `TKeymap`, `TInst`, `TPmap`, `TPerc`), `JASWSParser.h` (every struct and
    `TOffset`), `SoundInfo` (`JAISoundTable.h`), `initOnCodeFxScene_s` (`JAIFx.h`),
    `FxlineConfig_` (`JASDSPInterface.h`) and `StreamLib::StreamHeader` (`JAIStreamMgr.h`, the
    stream table's and the .afc's header) are `BE(T)`. Under `TARGET_PC`: `aafPointer` is
    `BE(u32)*`; the bank and wave-system lists (sections 2 and 3), which the GameCube relocates in
    place into `initOnCode_s` pointers, are built as a host table with the same zero terminator;
    the scene table's offsets (section 6) go to a pointer table of their own; `SoundTable::init`
    reads its u16 header through `BE(u16)`; the BNK bank ID (`registBankBNK`) and envelope tables
    are read big-endian and each oscillator's copied table is swapped to host order, which is how
    TOscillator reads its compiled-in tables; `JAISequenceMgr`/`JAISeMgr` read the `SoundInfo`
    flag through the struct instead of `*(u32*)`; `setFXLine` copies the filter taps to host
    order. The removed `TODO(native phase 4)` markers in `JAIInitData.cpp` lower group K to 10.
    The sequence data (BMS) is read byte by byte (`TSeqCtrl::get16/read24`) and needed nothing.
  - **Pointer tables sized for 4-byte pointers** (step 5.2's, fixed here because the parse reads
    them): `BankMgr::init`, `WaveBankMgr::init` allocate and `TBasicBank::setInstCount`,
    `TBasicInst::setEffectCount/setOscCount`, `TDrumSet::TPerc::setEffectCount`,
    `TBasicWaveBank::setWaveTableSize` clear `n * 4` bytes for `n` pointers; under `TARGET_PC`
    they use `sizeof` the pointer, so unset oscillator and effect slots are null.
  - **Harness:** `pc_audio.cpp` sets up what `mDoAud_Create`/`JAIZelBasic::init` do for the data
    (an audio solid heap, `sysDramSetup`/`sysAramSetup(0xa00000)`, the path parameters,
    JaiInit.aaf through `setParamInitDataPointer`), without the audio thread, DVD thread or DSP
    (step 5.A), then runs `JAIBasic::initHeap/initResourcePath/initArchive/initReadFile`, and
    writes every record back from the game's structures. `disc_manifest.py` (manifest version 6)
    decodes JaiInit.aaf's sections, the banks and wave systems inside it and the .afc headers
    independently; `--check-audio` builds the same lines and compares them in order. Negative
    checks: with HEAD's JAudio the smoke faults (13); a changed field is reported as a DIFF.
  - The new structs are in `layout_headers.txt` (22 structs, the GameCube check holds 1149).
    `unifdef -UTARGET_PC` of every changed game file equals HEAD's but the `BE(T)` fields and the
    `helpers/endian.h` includes. Audio heap used by the parse: 600160 bytes (the GameCube heap is
    0x166800; its 64-bit scaling stays with 5.2).
  - Review (round 1): accepted; `audio-parse` rerun equal to the manifest, `regress.sh -j 3`
    all checks passed. Committed as two root causes (pointer-table sizes, then the formats).

- **4.11 J3D model data** (2026-10-03): `COS_SMOKE=j3d-sweep` 0 x3; the report equals the manifest
  (1319 archives, 3103 J3D2 files: 202 BMD (9 MAT2), 2556 BDL, 345 BMT; 17004 joints, 11692
  materials, 11553 shapes, 16647 textures, 36606 draw matrices, 12917 weighted matrices). Builds on
  4.8's model blocks; each item below is meant as its own commit:
  - **MAT2 (v21) factory read host-order.** `J3DMaterialInitData_v21` gets `BE(u16)` index fields
    and offsets (size 0x138); the factory's material-ID/texture/cull/TEV-colour tables are `BE(T)*`
    and, as the MAT3 factory, it copies the tex-matrix, fog and NBT-scale infos host-order (the
    converters `J3DHostTexMtxInfo`/`J3DHostFogInfo`/`J3DHostNBTScaleInfo` are now shared from
    `J3DMaterialFactory.cpp`).
  - **J3DStruct.h table entries lost their alignment.** `ALIGN_DECL` is empty off Metrowerks, so
    `J3DTevSwapModeInfo`, `J3DIndTevStageInfo`, `J3DTexCoordInfo` and `J3DTevOrderInfo` were
    smaller than the MAT3/MAT2 table entries they index (and `J3DIndInitData` 0x138); under
    `TARGET_PC` they keep the Metrowerks alignment. The J3DStruct.h infos and the material init
    data joined `layout_headers.txt` with their sizes.
  - **EVP1/DRW1 tables the runtime reads were big-endian.** `readEnvelop` keeps host-order copies
    of the mix indices, weights and inverse joint matrices (the matrix count is the table's length,
    up to the next table or the block's end), `readDraw` of the draw matrix indices, on the model's
    heap (no swap in place, so a resource can be loaded twice).
  - **Material IDs from 64-bit addresses.** `mDiffFlag` is built from `(u32)ptr` or `(u32)ptr >> 4`
    and `J3DMatPacket`/`J3DMaterial::change` read bits 31/30; on PC `J3DGCAddressBits` gives the
    low 30 bits of the host address (unique inside the 256 MiB MEM1 block, H5) the GameCube's
    upper bits. The two `readVertex` count markers compute a distance inside VTX1. Group F 21 -> 11.
  - **Joint scale compensation read as a bool.** `J3DJointInitData::mScaleCompensate` is 0, 1 or
    0xFF in the file; as a `bool` clang treats 0xFF as undefined and dropped
    `J3DJointFactory::create`'s 0xFF test (4 models kept 255). On PC it is a `u8`.
  - **CLS1 (`J3DClusterBlock`) as BE with `OFFSET_PTR_V0`;** `readCluster` converts the records into
    the host structs (no BLS file is on the disc; compiled, not exercised). `J3DClusterBlock` left
    `layout_xfail.txt`.
  - **Harness:** `pc_j3d.cpp` mounts every archive under /res and loads every BMD/BDL/BMT through
    `J3DModelLoaderDataBase` in a solid heap with `dRes_info_c::loadResource`'s flags for its
    directory (the LODALL BDLs with `d_a_lod_bg`'s), then compares the model data with plain
    big-endian reads of the file: INF1, VTX1 format list and arrays, EVP1/DRW1 values, every
    joint, shape (descriptors, display lists, matrices), material (cull, counts, colours, textures,
    tex matrices, fog, NBT scale), MDL3 display lists, current matrix (recomputed from MAT3 with
    BDL flag 0x2000, as `modifyMaterial`) and patching offsets, TEX1. `j3d_sweep.txt` goes to
    `disc_manifest.py --check-j3d` (counts, INF1 flags and vertices, joint/material/texture names;
    every J3D2 file once). Negative checks: HEAD's `J3DJointFactory.h` gives 4 joint errors;
    HEAD's v21 factory and `J3DModelLoader.cpp` crash the sweep (13).
  Not done (4.12): the remaining group F markers are run-time (`J3DMaterial::getMaterialAnm`,
  `J3DTexture::setResTIMG`, DrawBuffer, Shape, Joint, Sys, `J3DAnmLoader`), and the vertex data
  the CPU reads (skinning).
  Step verification: j3d-sweep 0 x3, logo-res 0, `cos_pc_tests` ok, layout check 1154 GameCube
  checks, inventory `--check` ok (50 open); `unifdef -UTARGET_PC` of the changed files equals HEAD
  but for `BE(T)` shims.
  Review (2026-10-03): approved; j3d-sweep 0 equal to the manifest, `regress.sh -j 3` all checks
  passed; committed as seven commits, one root cause each, plus this log.
- **5.2 JAudio and 64-bit** (2026-10-03, pulled ahead of M7 by H10): build clean (`ninja` on every
  default target), `COS_SMOKE=audio-parse` 0 x3 (one uncapped) and equal to the manifest, logo-res
  0. Each change is under `TARGET_PC` with the GameCube code in `#else`; `unifdef -UTARGET_PC` of
  every changed game file equals HEAD's but one removed marker comment.
  - **Owner tags (callback IDs).** `TDSPChannel`'s owner tag (`field_0x8`, `allocate`, `alloc`,
    `free`, `checkSign`) is `TDSPChannelSign`, a `uintptr_t`: `getLogicalChannel` turns it back
    into the owning `TChannel*`, which the u32 truncated. Callers (`JASChAllocQueue`,
    `TChannel`, `StreamLib`'s `&assign_ch[i]`) pass the whole address. 7 markers of group K go.
  - **DSP and AI addresses** are MEM1 physical addresses through the new
    `JASystem::Kernel::toPhysical` (`JASSystemHeap`), which panics outside MEM1 (Aurora's
    `OSCachedToPhysical` only asserts, and not in RelWithDebInfo): `DsetupTable` (CH_BUF, FX_BUF,
    both filter tables), `DsetDolbyDelay`, `DsyncFrame2` (`dsp_buf`), `Play_DirectPCM`'s PCM
    address, `AIInitDMA` (the DAC buffers; the cos_sdk AI stub comment says so) and
    `FXBuffer::field_0x4`, which is a u32 physical address so `FXBuffer` keeps the 0x20 bytes the
    DSP reads (`static_assert` on `FXBuffer` 0x20 and `DSPBuffer` 0x180).
  - **MEM1 placement.** CH_BUF, FX_BUF, `dsp_buf` and the DAC buffers already come from JASDram (the
    audio heap, inside MEM1). The static `DSPRES_FILTER`, `DSPADPCM_FILTER` (now sized in
    `JASDriverTables.h`) and `DOLBY2_DELAY_BUF` are outside MEM1, so `setupBuffer` hands the DSP
    JASDram copies. `DspBoot` copies the `jdsp` ucode to JASDram and allocates the 8 KiB yield
    buffer there, and the task gets their physical addresses instead of `jdsp + 0x80000000`.
  - **Audio heap** 0x166800 x2 (H5, Dusklight's `audioHeapSize` x2) in `m_Do_main.cpp`.
  - **Pointer tables:** `StreamLib::allocBuffer`'s `loop_buffer`/`store_buffer` tables and
    `getNeedBufferSize` use `sizeof` the pointer instead of 8 and `LOOP_BLOCKS << 2`.
  - **DVD command pointer:** `mDoDvdThd_param_c::mainLoop` hands `JASystem::Dvd::sendCmdMsg` the
    command pointer with a size of 4, so the JAudio DVD thread called `cb` with half a pointer
    (SIGSEGV in `cb` at logo-res with `COS_AUDIO=on`); it passes `sizeof(cmd)`. Outside JAudio
    (`m_Do_dvd_thread.cpp`), fixed here because it is the 64-bit argument of a JAudio callback.
  - Markers: group K 10 -> 1. `JASSeqParser`'s debug-print one and the never-compiled decomp
    `OS.h` one are `NOTE(native phase 4, harmless)`; open is `cmdJmp` through the address
    registers 0x28..0x2b (32-bit values read from the sequence).
  - Left for 5.A/5.4: with `COS_AUDIO=on` logo-res now gets to `TGlobalChannel::alloc` on a null
    `sChannelMgr` (`mDoAud_Execute` -> `checkReadSeq`): the audio thread waits in `DspBoot`'s task
    for a DSP (cos_sdk has none), so `Driver::init` never runs. Everything the DSP reads
    (CH_BUF, FX_BUF, the filter tables) is written in host byte order; a big-endian DSP backend
    (5.4 option B) needs it swapped.
  - Review (round 1): accepted; `audio-parse` equal to the manifest, inventory check ok,
    `regress.sh -j 3` all checks passed. Committed as one root cause per commit (owner tags,
    stream pointer tables, DVD command pointer, audio heap, physical addresses, markers and log).
- **5.A `mDoAud_Create` with `COS_AUDIO=on`** (2026-10-03, decision H10; pulls in 5.4 option B
  of H6): the handshake needs a DSP that answers (`DspHandShake`, `DSPSendCommands2`,
  `DsetupTable`, `DsetDolbyDelay` wait for its mails and interrupts), so cos_sdk's silent DSP
  stubs are replaced by an emulated DSP. JAudio's DSP code runs unchanged. Committed as:
  - **DSP backend (H6 B).** `native/dsp_hle`: this repository's
    `runtime/host/src/dsp_hle_backend.cpp` (and the Core::System stubs of
    `dsp_adapter_donor_stubs.cpp`, the Common shim of `apple/ios/src/dsp_common_shim.cpp`)
    adapted behind `cos_dsp_hle.h` (control register, both mailboxes, Update; the interrupt is
    delivered at once instead of latched). `native/cmake/dsp_hle.cmake` compiles Dolphin's DSPHLE,
    its ucodes and `DSPAccelerator.cpp` from `COS_RECOMPCORE_DIR` (default `ref/recompcore`, or
    the main checkout's from a worktree) with Aurora's fmt. GPLv2+ code in a GPLv3 repository.
  - **cos_sdk DSP over it.** `src/audio/DSPStubs.cpp` becomes `DSP.cpp`: mailboxes and DSPCR go to
    the DSP (MEM1 by physical address, Aurora's ARAM, created at `DSPInit`); the SDK's
    `__DSP_boot_task`/`__DSP_exec_task` mail sequences; a "DSP interrupt" host thread calls the
    `__OS_INTERRUPT_DSP_DSP` handler with the OS lock held while DSPCR's interrupt bit is set
    (and runs the ucode's Update every 1 ms). A weak `__DSPHandler` only acknowledges (JAudio's
    wins). `cos_sdk_smoke audio` now checks the boot ROM's 0x8071FEED, its 0xFEEE reply,
    `DSPReset` and task boots against the real DSP.
  - **`__DSPHandler` acknowledges through cos_sdk** (`osdsp_task.c`, `TARGET_PC`): it wrote the
    console register `__DSPRegs[5]` (0xCC00500A); it now uses `COSDSPRead/WriteControlRegister`
    (`cos_dsp_extras.h`).
  - **Audio thread start-up order** (`JASAudioThread.cpp`, `TARGET_PC`; review round 1): on the
    console the audio thread has priority main-3, so `OSResumeThread` in `TAudioThread::start`
    preempts main and `audioproc`'s start-up (`Kernel::init`, `TDSP_DACBuffer::init`, `DspBoot`,
    `Driver::init`, the AI set-up) ends before `start` returns. Host threads ran at once, so
    `JAInter::Fx::init` could reach `FXBuffer::setFXLine` with FX_BUF still null (SIGSEGV in
    `mDoAud_Create`, about 1 run in 30 under load). `start` now waits on a message `audioproc`
    sends right before its loop. Checked with `regress.sh -j 3` 10x and 26 rounds of 5-6
    concurrent milestones (frame-loop, logo-scene, static-init, gfx-create, heaps, logo-res):
    148 runs, no failure.
  - **DSP state outlives exit** (`DSP.cpp`, `dsp_hle_backend.cpp`; review round 1): the DSP
    interrupt thread is detached and keeps calling the DSP every 1 ms while `exit` runs the static
    destructors, which destroyed the DSPHLE object (`std::unique_ptr`) and the DSP locks, so a
    process could crash after it finished (`cos_sdk_smoke` exited 139 with no `ok`, 2 runs in
    240 under load). The DSPHLE object and the locks now live in never-destroyed storage, as
    `Lock()` and the alarm state do. 0 failures in 720 runs since.
  - **Harness:** `run.sh` defaults to `COS_AUDIO=on`; `pc_frame.cpp` logs
    `[cos] audio: mDoAud_Create done at frame N; DSP handshake done` and logo-res fails (check
    failed) if the scene changes with audio on but JAudio not up.
  Result: Dolphin recognises the uploaded `jdsp` (JAudio ucode, it handles `DsetDolbyDelay`'s
  CMD 0D), `mDoAud_Create` finishes at frame 2 with the handshake done, logo-res 0 x3 capped and
  once uncapped with audio on; `regress.sh -j 3` all checks passed (every milestone
  static-init..logo-res now runs with `COS_AUDIO=on`). `run.sh opening` gets past
  `dComIfG_changeOpeningScene` -> `mDoAud_setSceneName` (no fault) and the logo scene's delete,
  then panics in `dPa_modelControl_c` (`d_particle.cpp:288`, the particle solid heap of H10/H5,
  the M7 boot loop's). Still silent: the AI raises no DMA interrupt, so the audio thread never
  asks the DSP for a frame (5.3); CH_BUF, FX_BUF and the filter tables are written host-order
  and the JAudio ucode reads them big-endian (5.4/5.5).
  - Review (round 2): accepted; `regress.sh -j 3` all checks passed 5x with `COS_AUDIO=on`
    the default, `unifdef -UTARGET_PC` of `JASAudioThread.cpp`/`osdsp_task.c` equals HEAD,
    `run.sh opening` logs the handshake done and panics only later in `dPa_modelControl_c`.
- **5.3 Audio output over SDL3** (2026-10-03): `cos_sdk_smoke ai-tone` passes (3840 frames, 24
  blocks of 0x280 bytes through the DMA in 0.12 s; the dump equals the input), 5 full
  `cos_sdk_smoke` runs ok; negative check: without the big-endian swap `ai-tone` fails.
  - **AI DMA over SDL3** (`native/sdk/src/audio/AI.cpp`, was `AIStubs.cpp`): a host clock thread
    plays the console's DMA engine as Dolphin models it: `AIStartDMA` latches the registers
    `AIInitDMA` set (MEM1 physical address, length); every millisecond the frames real time says
    are due are read in 32-byte steps as big-endian s16 stereo, right channel first, and at each
    block end the registers are latched again and the DMA callback is called with the OS lock
    held (interrupt context), so JAudio's `syncAudio` posts its message and `updateDac` sets the
    next block. The lock is released between steps. The frames go to an SDL3 stream (s16 stereo
    at the DSP rate, `SDL_PutAudioStreamData`, capped at 200 ms queued) and to
    `COS_AUDIO_DUMP=x.wav` (header rewritten after each write; reopened at each `AIInit`).
    Deviation from the step text ("an SDL3 stream pulls each block"): a pull callback is asked
    for several blocks at once and would raise several interrupts back to back before the audio
    thread could set the next block (stale blocks replayed), so the engine is paced by the host
    clock and SDL only plays what it is given. `AIGetDMABytesLeft` reads the engine's count
    (Dolphin's register read); the `audio` test's check changes from `== 0` to `< 0x280`.
  - **Harness:** `cos_sdk_smoke` sets `SDL_AUDIO_DRIVER=dummy` unless set; `run.sh` does the
    same (headless runs, several lanes at once) unless `--sound`, and `--audio-dump F` sets
    `COS_AUDIO_DUMP` (a relative F goes into the run directory).
  - **`TTrack::writeRegParam` stored an indeterminate flag** (`JASTrack.cpp`, `TARGET_PC`; step
    5.5's file, fixed here because the DMA ticks now run the sequencer and `outset-debug` (M12)
    died with SIGTRAP in it): for register writes 0x20/0x21/0x2E/0x2F `reg_flags` is never set
    (the decomp's "Bug" comment); clang compiles the store of the indeterminate value to a trap
    right after `getProgramNumber`. On the console the value is the caller's r31, which
    `TSeqParser::parseSeq` holds the track (`this`) in (main.dol 0x80280264, stored at
    0x80283674), so the low half of `this` is stored.
  Result: the audio thread now gets a DMA tick every 5 ms and requests DSP frames; logo-res,
  title and outset-debug reach their milestones with `--audio-dump` and the dumps are silent
  (0 non-zero samples): the JAudio ucode reads CH_BUF/FX_BUF host-order (5.4/5.5), which shows
  as `DSP: main-memory range 0xc0063900+0xa0 is outside MEM1` and about 25000 `[dsp-alert]
  ... unknown/unimplemented sample source: 0900` lines per outset-debug run (run.log 14 MB).
  The SDL dummy driver plays a little slower than the clock (its `SDL_Delay` per buffer), so on
  long runs the 200 ms cap drops frames (logged once); the dump is unaffected.
  - Review (round 1): accepted; `cos_sdk_smoke ai-tone audio` ok 3x (0.122 s), full
    `cos_sdk_smoke` ok, `regress.sh -j 3` all checks passed, `run.sh outset-debug --stage
    sea:44:206 --audio-dump a.wav` reached with a 1.9 MB WAV (silent, as expected before 5.4/5.5).
    Committed as two commits (the `writeRegParam` fix, then the AI DMA).
- **5.5 Sequences and sound effects** (2026-10-03; 5.4 is option (B) of H6, in place since 5.A):
  `run.sh title-audio` 0 x3 (RMS -27.2 dBFS over the 300 frames after the title, about 960
  sequence ticks); `run --frames 1500 --audio-dump a.wav` reaches the title and the dump is
  -27.6 dBFS RMS from 15 s to 45 s (was 0 non-zero samples). Negative check: with HEAD's
  `JASDSPInterface.h/.cpp` title-audio fails (-999 dBFS, the ticks still advance). Each item below
  is meant as its own commit:
  - **The DSP's channel and effect buffers are big-endian** (`JASDSPInterface.h/.cpp`): every
    multi-byte field of `DSPBuffer` (CH_BUF, 64 x 0x180 bytes) and `FXBuffer` (FX_BUF) is `BE(T)`
    (a u32 is its high word first, as the JAudio ucode's `_h`/`_l` word pairs), since Dolphin's
    JAudio ucode HLE reads and writes them as big-endian u16 words, as the console's DSP does
    (`FetchVPB`/`StoreVPB`, the reverb PBs). `setMixerInitVolume`/`setMixerVolume`/`setBusConnect`
    take a `BE(u16)*` row; a `TARGET_PC` overload of `setFilterTable` copies the host-order taps
    of `TChannelMgr` into the `BE(s16)` FIR8/IIR fields; three assignments between `BE` fields of
    different types spell out the conversion with a cast (same value on the GameCube). The MEM1
    copies of `DSPRES_FILTER` and `DSPADPCM_FILTER` that `DsetupTable` hands the DSP are written
    big-endian (`RES_U16`). This was the `unknown/unimplemented sample source: 0900` alert (AFC
    type 9 read as 0x0900) and `main-memory range ... outside MEM1` of 5.3.
  - **Mixer configuration bytes on a little-endian host** (`JASChannel.h/.cpp`, `TARGET_PC`):
    `TChannel::MixConfig`'s `mParts` (high byte `u`, then the two nibbles of the low byte) and the
    `{hi, lo}` union of `TChannel::playLogicalChannel` read the parts of a host-order `u16` with
    the console's byte and bit-field order, so on the host `setBusConnect` got the low byte and
    indexed past its 12-entry `connect_table` (bus IDs 0xfff9/0x5678: `BufferForID` drops the
    voice, so every voice mixed through the six mixer channels, i.e. all instrument notes, was
    silent; only auto-mixer (Dolby) voices like the title's sea played) and `updateMixer` picked
    the wrong pan/fx/dolby curves. On the host the parts are declared low byte first (bit-fields
    from the least significant bit; a static_assert requires a little-endian host). After it the
    name scene's `JA_BGM_SELECT` (sound 0x8000001e) shows as notes in a spectrogram of the dump.
  - **Harness:** `COS_SMOKE=title-audio` (`pc_title_audio.cpp`, in-game smoke test, added to
    `regress_targets.txt`): from milestone M9 title on it measures 300 game frames: the RMS of
    what the AI DMA played (`COSAIGetOutputStats`, `cos_sdk/audio.h`, both channels, silence
    included) must be above -40 dBFS and `JASystem::getSeqTickCount` (a `TARGET_PC` counter of
    root-track `mainProc` calls in `TTrack::rootCallback`) must advance; it logs both and the
    main/sub/stream BGM sound IDs.
  Open: the title has no main BGM (0xffffffff): its music is the stream `title.afc` (5.6). The
  balance between instrument voices (mixer volumes around 0x0010-0x0480) and the sea (Dolby
  volume 0x050d) is for 5.8's comparison with the LLE DSP. `COS_AUDIO` stays on by default
  (since 5.A).
  Review (round 1): title-audio 0 x2 (-27.3 dBFS, 960 ticks), `run --frames 1500` dump -27.6 dBFS
  from 15 s on, `regress.sh -j 3` all checks passed; committed as three commits.

- **5.6 Streams** (2026-10-03): `run --frames 4000 --audio-dump a.wav` (about 130 s) lets the title
  run until d_a_title switches to the attract prologue (`fpcNm_OPEN2_SCENE_e`, `dScnOpen_proc_c`),
  which prepares and plays the stream `JA_STRM_DEMO_01_01` (`AudioRes/Stream/1tale.afc`, AFC,
  32000 Hz). The dump from 76.76 s on equals an independent decode of the AFC read from the disc
  (both channels: correlation 0.999 for the first 10 s, 1.0000 from 10 s to 45 s, gain 0.59,
  residual 46-54 dB below the signal); before the fix the same window was full-scale noise
  (-11 dBFS, correlation 0.00). The title itself starts no BGM: `setSceneName` sets `field_0x0066`
  for sea_T (scene 0x75), so `bgmStart` refuses everything but `JA_BGM_SELECT`, and the opening
  scene is not `fpcNm_PLAY_SCENE_e` (no `sceneBgmStart`); 5.5's "title.afc" guess is withdrawn.
  `HardStream::useHardStreaming` is never set (DTK streaming unused).
  - **AFC samples stored big-endian** (`JAIStreamMgr.cpp`, `TARGET_PC`): `__DecodeADPCM` decodes
    in host order and copies the samples into the DirectPCM loop buffers, which the DSP reads as
    big-endian s16 (Dolphin's JAudio ucode HLE, as from the console's RAM); each sample is now
    stored with `RES_S16`. `__DecodePCM` copies the disc's big-endian PCM as it is and needs no
    change.
  No regression target: the stream starts about 77 s into a run, too slow for `regress.sh`.
  Review (round 1): rerun `run --frames 4000 --audio-dump` (129 s): stream at 76.755 s, both
  channels correlation 0.999 (0-10 s) and 1.0000 (10-45 s), gain 0.59; `regress.sh -j 3` all
  checks passed.

- **4.12 J3D animation and runtime** (2026-10-03): `COS_SMOKE=anm-sweep` 0 x3; the report equals
  the manifest (1319 archives, 6227 J3D1 files: 3444 BCK, 1070 BTK, 444 BRK, 13 BPK, 1255 BTP,
  1 BVA; 2799678 values at the first, middle and last frame). Builds on 4.8's animation block
  headers; each item below is meant as its own commit:
  - **Animation tables and value arrays read host-order.** The tables the getters index
    (`J3DAnmKeyTableBase`, the colour/transform/visibility/tex-pattern/cluster full tables) are
    `BE(u16)`; the value arrays the `J3DAnm*` objects point into are `BE(f32)*`/`BE(s16)*`/
    `BE(u16)*`/`BE(Vec)*` (u8 arrays stay as they are), and `J3DGetKeyFrameInterpolation` is
    instantiated on the BE types. No swap at load (decision H1 allows one for CPU-hot arrays with
    a double-swap guard): `searchUpdateMaterialID` writes material IDs back into the file data,
    `dRes_info_c` binds BCKs with `setResource` and the player loads the same buffers again, so
    reading in place needs no "swapped once" mark. The `bswap` per key read is one instruction.
  - **`J3DHermiteInterpolationS` was empty on PC.** Its body is paired-single assembly only, so
    every interpolated BCK/BTK rotation and BXK colour returned an uninitialised register. On PC
    it is the same operations in C, in the assembly's order.
  - **Vertex-colour index records relocated in place.** `J3DAnmVtxColorIndexData::mpData` holds a
    file index the GameCube loader overwrites with a 32-bit pointer; on PC the 8-byte record
    stays as in the file (`BE(u16)`, `BE(u32)`) and `J3DAnmVtxColor::colorAddressBase` keeps the
    two index tables (Dusklight's base), with `getVtxColorIndexPointer` resolving a record. No
    BXK/BXA is on the disc (compiled, not exercised). The record joined `layout_headers.txt`.
  - **`J3DMaterial::getMaterialAnm` dropped animations by a truncated address.** The GameCube
    test `< 0xC0000000` read the low 32 bits of the host pointer; on PC it returns the pointer
    (Dusklight's `J3DMaterial.h`), and `J3DMatPacket::setMaterialAnmID` keeps the whole pointer
    (`uintptr_t`), which the draw buffer compares. The two `J3DDrawBuffer` truncations are only
    hashes (marked harmless).
  - **`J3DTexture::setResTIMG` offsets.** The entry's image/palette offsets become relative to
    the entry and can be negative (a 32-bit wrap on the GameCube); on PC the distance is checked
    to fit in 32 bits (every ResTIMG passed is in a MEM1 heap, H5; OSPanic otherwise) and
    `loadTexNo` adds the offsets sign-extended. Group F 11 -> 2.
  - **Harness:** `pc_anm.cpp` mounts every archive under /res, expands Yaz0 entries (LkAnm,
    LkD00, LkD01, as `readResource` does) and loads every J3D1 file as the game does (BCK:
    `mDoExt_transAnmBas` + `setResource`, as `dRes_info_c`; the rest `J3DAnmLoaderDataBase::load`)
    in a solid heap, evaluates it through the game's getters at frames 0, max/2 and max, checks
    every value finite, |scale| < 1e3, |translation| < 1e6, loads the file a second time
    (`load`) and requires the same values bit for bit. `anm_sweep.txt` goes to
    `disc_manifest.py --check-anm` (manifest version 7), which evaluates the same animations from
    the file bytes in emulated f32 and compares per file the block tag, attribute, frame count,
    track and name counts and, per family, count, sum, sum of |v| and max |v| (tolerance: integer
    results within 1e-3 of a truncation boundary, 1e-5 of the magnitude for floats). Negative
    checks: HEAD's `J3DAnimation.h/.cpp` and `J3DAnmLoader.cpp` give 8526 errors (scale and
    translation out of range); a broken `J3DHermiteInterpolationS` gives 7017 manifest DIFFs.
  Not done (split off; the runtime of drawn and skinned models, verified once models draw): the
  CPU skinning `J3DSkinDeform` (display-list reads of `vtxCount`/indices and the big-endian
  source positions and normals, used by doors, the ship and `dBgWDeform`), `J3DShape`'s
  `J3DLoadArrayBasePtr` (CP 0xA0, which Aurora rejects: needs `GX_AURORA_LOAD_ARRAYBASE` with the
  array size and byte order of the transformed arrays; done by R4-arraybase) and `J3DSys`'s matrix count for
  `GXSETARRAY` (the two remaining group F markers). Clusters (`J3DDeformer`) have no BLS on the
  disc.
  Step verification: anm-sweep 0 x3 equal to the manifest, logo-res 0, j3d-sweep 0 equal,
  `cos_pc_tests` ok, layout check (GameCube 1451 checks), inventory `--check` ok (37 open);
  `unifdef -UTARGET_PC` of the changed game files equals HEAD but for `BE(T)` shims and comments.
  Review (2026-10-03): approved; anm-sweep 0 x3 equal to the manifest, logo-res 0, j3d-sweep 0,
  `regress.sh -j 3` all checks passed; committed as six commits, one root cause each, plus
  this log. The split-off items (J3DSkinDeform, J3DShape array base, J3DSys matrix count) stay open.

- **M7 boot loop, iteration 1** (2026-10-03, layout, H5/H10): `opening` panicked at frame 272 in
  `dPa_modelControl_c` (d_particle.cpp:288, `model->mModel != NULL`) from `dPa_control_c::createCommon`
  in `dScnLogo_Delete`: the particle solid heap (0x16e800) is too small for 64-bit objects, since
  createCommon needs 0x18e020 bytes on the host. Under `TARGET_PC` the heap is created at
  0x16e800 * 2, and `mDoExt_adjustSolidHeap` trims it right after. Review: `regress.sh -j 3` all
  checks passed; `opening` now reaches OPENING_SCENE(8) at frame 282 and stops on the next blocker,
  SIGSEGV addr=0x18 in `fpcMtd_IsDelete` <- `fpcCtRq_Cancel` (null method table on a cancelled
  create request). Note: after a rebase, build the `centollos` target explicitly (`ninja centollos`).

- **M7 boot loop, iteration 2** (2026-10-03, host-semantics): the cancelled `daShip_c` create
  (cPhs_ERROR_e, MET_KORL not set) crashed with SIGSEGV addr=0x18 in `fpcMtd_IsDelete` <-
  `fpcCtRq_Cancel`: `fopAcM_ct`'s `new (ptr) ClassName()` value-initialises, and clang zero-fills
  classes without a user-provided constructor, wiping the base_process_class header (`mpPcMtd`)
  that `fpcBs_Create` had set; MWCC default-initialises. Under `TARGET_PC` `fopAcM_ct` uses
  `fopAcM_ct_placement` (`new (ptr) ClassName`, after Dusklight). Review: `regress.sh -j 3` all
  checks passed; `opening` reaches ROOM_SCENE at frame 284 and stops on the next blocker, SIGSEGV
  addr=0xc in `fpcPi_Change` <- `fopAcM_setStageLayer` <- `phase_1(daPy_lk_c*)`.

- **M7 boot loop, iteration 3** (2026-10-03, layout): SIGSEGV addr=0xc in `fpcPi_Change` <-
  `fopAcM_setStageLayer` <- `phase_1(daPy_lk_c*)`. `daPy_lk_c` has virtual functions and
  `fopAc_ac_c` had none, so the Itanium ABI put the vtable pointer at offset 0 and `fopAc_ac_c` at
  offset 8; MWCC appends it after the base. Every `void*`/`base_process_class*` use of such an actor
  (here `fopAcM_setStageLayer(i_this)`) then read the process header 8 bytes early. Under
  `TARGET_PC` (after Dusklight) `base_process_class` gets a virtual destructor and `leafdraw_class`
  and `fopAc_ac_c` inherit from it instead of embedding it, so the vtable pointer is shared at
  offset 0; the 19 `base.base.X` / `i_leaf->base.X` sites have a `TARGET_PC` form. A static probe
  found no polymorphic scene, msg, kankyo, view or overlap process class in GZLE01. Review:
  `regress.sh -j 3` all checks passed; `opening` now gets past the player's phase_1 and stops
  on the next blocker, SIGABRT (stack buffer overflow, `__stack_chk_fail`) in
  `J3DSkinDeform::initMtxIndexArray` (the J3DSkinDeform item split off from 4.11).

- **M7 boot loop, iteration 4** (2026-10-03, endian): SIGABRT (`__stack_chk_fail`) in
  `J3DSkinDeform::initMtxIndexArray` during the player's create. J3D shape display lists stay
  big-endian on the host (Aurora consumes them as GX streams), but the strip/fan vertex count and
  the GX_INDEX16 position/normal indices were read host-order, so the loop ran with a bogus count
  and overran the 10-entry `useMtxIdxBuf`. Under `TARGET_PC` those reads go through `BE(u16)`, and
  `changeFastSkinDL` reads and writes the vertex count big-endian. Review: `regress.sh -j 3`
  all checks passed; `opening` now stops on the next blocker, SIGSEGV addr=0xc in
  `daPy_lk_c::playerInit` (d_a_player_main.cpp:12293, `createAnimeHeap` area) <- `phase_2`.

- **M7 boot loop, iteration 5** (2026-10-03, truncation/H5): SIGSEGV addr=0xc in
  `daPy_lk_c::playerInit` <- `phase_2`. `createAnimeHeap` sizes its small solid heaps with
  GameCube magic numbers (0x40/0x50/0xA0); on the host `J3DAnmTransformKey` is 0x48,
  `J3DAnmTexPattern` 0x58, `mDoExt_transAnmBas` 0x50 and `J3DAnmTextureSRTKey` 0xF0 bytes, so the
  `new` failed and the constructor wrote through NULL. Under `TARGET_PC` the size is doubled
  (decision H5); `mDoExt_adjustSolidHeap` trims the rest. Fixer: `regress.sh -j 3` all checks
  passed; `opening` now stops on the next blocker, SIGSEGV addr=0xd7a46cc60 in `strcmp` <-
  `dEvent_manager_c::getEventIdx` <- `dEvt_info_c::setEventName` <- `daAgb_Create`.
  Reviewer: regress all checks passed; `opening` confirmed past `playerInit`, stops in `daAgb_Create`.
- **M7 boot loop, iteration 6** (2026-10-03, endian, H1; file owned by 4.16): SIGSEGV in `strcmp`
  <- `dEvent_manager_c::getEventIdx` <- `daAgb_Create`. The event list binary is big-endian disc
  data used in place, but `event_binary_data_header` and the Event/Staff/Cut/Data records were
  read host-order, so `eventTop` came out byte-swapped and `mEventP` pointed far past the
  buffer. `d_event_data.h` now declares every disc field of the header and the four record types
  as `BE(T)` (Dusklight's layout, CC0); on the GameCube `BE(T)` is `T`. Left for 4.16: the
  substance arrays reached through `getMySubstanceP` (f32/int data, still raw `f32*`/`int*`).
  Fixer: `regress.sh -j 3` all checks passed; `opening` now gets past `daAgb_Create` and stops
  on the next blocker, SIGSEGV addr=0xc in `J3DAnmLoaderDataBase::load` <-
  `daPy_lk_c::getUnderUpperAnime` <- `setMoveAnime` <- `procWait_init` <- `makeBgWait` (frame 285).
  Reviewer: regress all checks passed; `opening` confirmed past `daAgb_Create`, stops in
  `J3DAnmLoaderDataBase::load` <- `daPy_lk_c::getUnderUpperAnime`.
- **M7 boot loop, iteration 7** (2026-10-03, host-semantics): SIGSEGV addr=0xc in
  `J3DAnmBase::J3DAnmBase` <- `J3DAnmLoaderDataBase::load` <- `daPy_lk_c::getAnimeResource` <-
  `procWait_init` <- `makeBgWait`. `createAnimeHeap` and `playerInit` size solid heaps with
  throw-away `new` allocations whose only use is `JUT_ASSERT(p != NULL)`; clang folds that check
  and elides the unused allocation ([expr.new]p10), so `mDoExt_adjustSolidHeap` trimmed the heaps
  to 0 bytes and the first real anime load got NULL. Under `TARGET_PC` an empty `asm volatile`
  takes each pointer (`daPy_keepHeapSizingAlloc`) so the allocation is kept, as on the GameCube.
  Open note: clang also folds every `JUT_ASSERT(p != NULL)` after a plain `new`, so heap
  exhaustion shows up as a NULL write instead of an assert. Fixer and reviewer:
  `regress.sh -j 3` all checks passed; `opening` gets past `makeBgWait` to ROOM_SCENE frame 302
  and stops on the next blocker, PANIC GFGeometry.cpp:263 (`GFSetArray(attr 9, stride 3)` needs
  `GFSetArraySized`) in `dWood::Packet_c::draw` <- `J3DDrawBuffer::drawHead`.

- **M7 boot loop, iteration 8** (2026-10-03, host-semantics): PANIC GFGeometry.cpp:263
  (`GFSetArray` has no array size on Aurora) in `dWood::Packet_c::draw` <- `J3DDrawBuffer::drawHead`
  (ROOM_SCENE frame 302). Aurora ignores CP_REG_ARRAYBASE; `GFSetArraySized` (pointer, byte size,
  byte order) existed in GFGeometry.cpp but was never declared or called. GFGeometry.h now declares
  it under `TARGET_PC`, and `dWood::Packet_c::draw`, `dTree_packet_c::draw` and
  `dGrass_packet_c::draw` pass the static, host-endian asset arrays with their byte sizes (grass
  picks the `l_Vmori_*` or `l_*` set its pointers name). Still unconverted: the `GFSetArray`
  callers in m_Do_graphic.cpp, d_drawlist.cpp, d_a_bwdg.cpp and d_a_mant.cpp (dynamic arrays,
  sized case by case when reached). Fixer and reviewer: `regress.sh -j 3` all checks passed;
  `opening` gets past the wood/grass draw to ROOM_SCENE frame 656 (sea_T Room0) and stops on the
  next blocker, PANIC JAISoundTable.cpp:61 in `JAInter::SoundTable::getInfoPointer` <-
  `JAIBasic::startSoundActor` <- `JAIZelAnime::startAnimSound` <- `daPy_lk_c::execute`.

- **M7 boot loop, iteration 9** (2026-10-03, endian + layout): PANIC JAISoundTable.cpp:61
  (`_category < getParamSeCategoryMax()`) in `JAInter::SoundTable::getInfoPointer` <-
  `JAIBasic::startSoundActor` <- `JAIZelAnime::startAnimSound` <- `JAIAnimeSound::setAnimSoundVec`
  <- `daPy_lk_c::execute` (ROOM_SCENE frame 656). The BAS animation sound data (the .bas used next
  to a BCK) is big-endian disc data used in place, but `JAIAnimeSoundData` and
  `JAIAnimeFrameSoundData` were read in host order, and the header's 4-byte `field_0x04` was a
  `void*`, which on the 64-bit host moved `mAfsData` from 0x08 to 0x10, so the sound IDs were garbage.
  Under `TARGET_PC`, JAIAnimation.h declares the multi-byte fields as BE(T) and `field_0x04` as
  `BE(u32)` (it is never read); the GameCube structs stay in `#else`. Both structs went into
  layout_headers.txt. `regress.sh -j 3`: all checks passed. `opening` no longer crashes: it runs
  the sea_T opening to the 180 s timeout (exit 10, ROOM_SCENE frame 5534, frames advancing). The
  run still cannot pass: nothing calls `pc_milestone("opening")` yet (the M7 probe for "`sea_T`
  stage arc mounted; `dStage_Create` done" is not wired). The log also prints the game's
  "デモデータ読み込みエラー" (demo data load error) once; it was already in the iteration 8 run.
  Reviewed: regress passed again; an independent `opening --timeout 180` run reached exit 10 at
  ROOM_SCENE frame 5534 with no PANIC.

- **M7 boot loop, iteration 10 / M7 reached** (2026-10-03, harness): the M7 probe was not wired.
  `dStage_Create` now ends (under `TARGET_PC`) with `pc_stage_created` (pc_frame.cpp), which logs
  `[cos] stage: <name> room <n> created at frame <f>; Stage archive <files> files, stage.dzs
  found|missing` for every stage and, for `sea_T`, logs milestone `opening` once the "Stage"
  archive is mounted and stage.dzs was read (exit 1 otherwise). `opening --timeout 180` exit 0 x3
  (sea_T room 44 at frame 281, 23 files in the Stage archive); `opening 0` added to
  regress_targets.txt. Open for M8: the game still prints "デモデータ読み込みエラー！！" (demo
  data load error) from `dDemo_manager_c::create` (JStudio `TParse::parse_next` rejects the STB),
  after the milestone frame.
  Reviewed: regress passed; three independent capped `opening --timeout 180` runs exit 0 (sea_T
  room 44, frame 281, ~5 s each).

- **Step 6.4: debug stage boot** (2026-10-03, lane outset, decision H4). `COS_BOOT_STAGE=<stage>:
  <room>[:<point>[:<layer>]]` (point 0 and layer -1 by default; `run.sh --stage`), parsed by
  the new `native/src/pc/pc_boot.cpp` at start-up (a malformed spec exits 2). When the logo
  scene's `dvdWaitDraw` has every `l_*Command` synced, `pcBootStage` (d_s_logo.cpp, `TARGET_PC`)
  replaces `dComIfG_changeOpeningScene`: a new file as `dScnName_c::NameInMain` makes it
  (`dComIfGs_init`, which sets the name "Link" and the return place sea 44 point 206, then
  `dComIfGp_itemDataInit`), the requested next stage, and `dScnName_c::changeGameScene`'s request
  for the PLAY scene (`fopScnM_ChangeReq`, `dComIfGs_resetDan`, `dComIfGs_setRestartRoomParam(0)`,
  `mDoAud_setSceneName`). The harness logs the next stage the game now holds and checks it against
  the request, then logs M6 logo-res (the request stands in for `changeOpeningScene`); d_s_play.cpp
  `phase_1` reports the first PLAY scene's start stage, which must be the requested one ("request
  honoured"; exit 1 otherwise). Verified: `logo-res --stage sea:44:206` exit 0 three times (the
  plain `logo-res` still exits 0), eight malformed specs exit 2, and `run --stage sea:44:206
  --frames 600` and `sea:44:206:0 --frames 400` log "PLAY scene starts stage sea room 44 point 206"
  at frame 282-283 and "request honoured". New regress lines: `logo-res 0 --stage sea:44:206` and
  `static-init 2 --stage sea`. Reaching a working Outset is M12: the first run goes on to
  ROOM_SCENE (sea Room0) frame 1230 and aborts in Aurora's shader generator (render issue below).
  Reviewed: regress passed; an independent `run --stage sea:44:206 --frames 400 --uncapped` logged
  the request at frame 245 and "request honoured" at frame 282, then hit the same Aurora abort at
  frame 371 with the pipeline cache from earlier runs present (deleted afterwards).

- **Screenshots: COS_SHOT** (2026-10-03, lane shot): `COS_SHOT=<frame>,...` and/or
  `COS_SHOT_EVERY=<n>` (`run.sh --shot`) save the presented frame of those game frames
  (pc_frame_count numbering) as `shot-<frame>.png` in `COS_SHOT_DIR`, else the run directory,
  without macOS Screen Recording permission. The new `native/src/pc/pc_shot.cpp` (parsed at
  start-up, a malformed list exits 2; inert without the variables) is called by `pc_frame_end`
  right after `aurora_end_frame`: it queues a job on Aurora's render worker behind the frame
  (FIFO, so the frame is submitted and the next not begun) that copies `present_source()` (the
  EFB render texture, its resolved copy under MSAA; CopySrc) into a MapRead buffer (rows padded to
  256 bytes), waits with `Instance::WaitAny`, converts BGRA8/RGBA8/RGB10A2 to RGB and writes an
  uncompressed PNG (stored deflate blocks, no library); the game thread then waits for the worker,
  so a shot right before an exit is on disk. Aurora internal headers (`lib/webgpu/gpu.hpp`,
  `lib/gfx/render_worker.hpp`) and Dawn's include directory are added for that file only. The
  image is the EFB at its own size (1920x1440 on a Retina 960x720 window, about 8 MB per PNG),
  without letterboxing or the ImGui overlay. Verified: `logo-res --shot 30,200` (boot logo,
  Dolby logo) and `run --frames 3000 --shot 300,1500,3000` (`opening` itself ends at frame 281):
  frame 300 black (stage fade-in), 1500 and 3000 the title over sea room 44 with Link and the HUD.

- **M8 boot loop, iteration 1** (2026-10-03, endian): the title demo printed
  "デモデータ読み込みエラー！！" because `JStudio::stb::TParse` read the STB container in host
  order: `memcmp` against the host-order `ga4cSignature`, then the header, block and object
  fields, the sequence heads, the variable-length paragraph headers
  (`JGadget::binary::parseVariableUInt_16_32_following`) and the reserved paragraphs (flag, wait,
  jump). Under `TARGET_PC` (Dusklight's stb-data.h/stb.cpp/binary.cpp, CC0): `stb-data.h` fields
  are `BE(T)` and `ga4cSignature` is `BE(u32)`; `get_head`, the reserved paragraphs and
  `parseVariableUInt_16_32_following` read `BE(T)`. The STB header now parses (`STB` 0xFEFF
  version 3, target `jstudio` 3, 5 blocks); the parse stops at the first block, `JFVB`, whose
  FVB header and blocks are still host order (`fvb-data.h`): that is the next root cause, and
  the paragraph payloads (`TParseValue_raw_`, `jstudio-object` values) follow it (step 4.17's
  format). `title-stage` itself still times out (exit 10, frames advancing): no M8 probe yet.
  Reviewed: regress passed; a temporary (removed) trace confirmed the STB header passes (5
  blocks) and the first block, `JFVB` (3928 bytes), is the one rejected now.

- **M8 boot loop, iteration 2** (2026-10-03, endian): the STB's `JFVB` block was rejected because
  `JStudio::fvb` read the FVB container and its paragraphs in host order. Under `TARGET_PC`
  (Dusklight's fvb-data.h/fvb.cpp, CC0): `fvb-data.h` `THeader`/`TBlock` fields and the
  `TObject_list*`/`TObject_hermite` count words are `BE(T)`; `TObject::prepare` reads the refer,
  range, progress, adjust, outside and interpolate paragraphs as `BE(T)`; the composite operand is
  two big-endian words (the GameCube struct's `const void*` member would sit at offset 8 on the
  host); constant/transition values are `BE(f32)`; the list, list-parameter and hermite tables are
  copied to a host-order `std::vector<f32>` (`mSwappedData`) that the function value reads. The
  `JFVB` block now parses and the parse moves on to the first object block, which crashes
  (SIGSEGV) in `JStudio::TFactory::create` walking `mList`, a `TLinkList<TCreateObject, -4>`:
  the node offset assumes a 4-byte vtable pointer (8 on the host). That is the next root cause.
  Reviewed: regress passed; title-stage no longer logs the demo-data error and now stops at frame
  301 (exit 13) in `JStudio::TFactory::create`, after `opening` at frame 281.
- M7 boot loop (lane boot, M8 iter 3): JStudio's intrusive lists hard-coded the GameCube node
  offsets (`TLinkList<TCreateObject, -4>`, `TLinkList<stb::TObject, -12>`,
  `TLinkList<fvb::TObject, -12>`); with an 8-byte vptr and host pointers the node moves, so
  iterating `TFactory::mList` turned nodes into garbage object pointers. Each class now has a
  `NodeOffset` (TARGET_PC: `-(int)offsetof(...)`, Dusklight pattern, CC0; #else the original
  literal) used by every list typedef/iterator. The STB parse now completes and every object is
  created; title-stage stops at frame 301 in `TVariableValue::update_functionValue_` (null, via
  `TObject::do_wait` during `stb::TControl::forward`), the next root cause.
  Reviewed: regress passed; title-stage gets past `JStudio::TFactory::create`.
- M7 boot loop (lane boot, M8 iter 4): `JStudio::TAdaptor::adaptor_setVariableValue_IMMEDIATE_`,
  `_TIME_` (f32) and `_FVR_INDEX_` (u32) read the STB operand payloads in host order, so the
  swapped function-value index made `getFunctionValue_index` return NULL and
  `TVariableValue::update_functionValue_` faulted. The three reads now go through `BE(T)` (no-op
  on GameCube; Dusklight pattern, CC0). The title demo now runs forward; title-stage stops at
  frame 301 in an OSPanic from `daPy_lk_c::changeDemoProc` (d_a_player_main.cpp:9342), the next
  root cause. Reviewed: regress passed; title-stage gets past `update_functionValue_`.
- M7 boot loop (lane boot, M8 iter 5): the `JStudio_JStage` adaptors (`TAdaptor_object_`,
  `TAdaptor_actor`, `TAdaptor_camera`, `TAdaptor_light`) read their STB operation operands (data
  ID, ENABLE flag, SHAPE/ANIMATION/TEXTURE_ANIMATION ids and modes, PARENT/RELATION node ids and
  enables, light FACULTY type) in host order. The player's demo actor got a byte-swapped animation
  id, so `daPy_lk_c::changeDemoProc` hit its `demo_mode < DEMO_LAST_e` assert (OSPanic at
  d_a_player_main.cpp:9342, frame 301). The reads now go through `BE(T)` (no-op on GameCube;
  Dusklight pattern, CC0). The title demo now plays through without a fault: title-stage times out
  at frame 5534 in OPEN2_SCENE with frames advancing; nothing calls `pc_milestone("title-stage")`
  yet, the next root cause. Reviewed: regress passed; title-stage gets past `changeDemoProc` (no
  fault through frame 3732, OPEN2_SCENE).
- M8 boot loop (lane boot, iter 6, harness): no code reported milestone M8, so title-stage timed
  out (exit 10) in OPEN2_SCENE with the title demo playing and frames advancing. The new
  `native/src/pc/pc_title_stage.cpp` adds the probe: `pc_stage_created` arms it with sea_T's start
  room (M7), and `pc_frame_end` polls each frame, reading game state only, until the room is
  loaded (its ROOM_SCENE process is executing, `Room<n>` holds room.dzr and the room status has its
  `dStage_roomDt_c`), its collision is registered (the room's BG actor is created, status flag 0x10
  is set and its `dBgW` is in a used `dBgS` element) and its actors are created (at least one, none
  still creating). It then logs the room and reports `title-stage` 300 frames later. Runs reach
  "room 44 ready at frame 287 ... 51 actor(s) created" and MILESTONE title-stage at frame 587,
  3 of 3. Adds "title-stage 0" to the regression targets.
  Reviewed: regress passed; title-stage reached 3 of 3 (one capped, frame 587; two uncapped).
- M9 boot loop (step 4.14, lane boot, iter 1, harness): the game already reached M9, but no code
  reported it, so `title` timed out (exit 10) in OPEN2_SCENE at frame 5535 with frames advancing
  (the title actor had run and requested OPEN2_SCENE). `daTitle_proc_c::proc_draw` now calls
  `pc_title_drawn` (under `TARGET_PC`) after it draws the title_logo BLO screen, and the new
  `native/src/pc/pc_title.cpp` polls from `pc_frame_end`, reading game state only. A frame counts
  when the d_a_title actor has finished creating, its screen was drawn that frame, its logo pane is
  fully faded in (J2DPane alpha equals the BLO's initial alpha, which is nonzero), its
  title-smoke JPA emitter has live particles and its sparkle emitter was set once. The probe
  reports `title` after 60 such frames in a row. Runs reach "d_a_title created at frame 286" and
  "60 frames drawn with the logo at alpha 255, 25 title smoke particle(s), sparkle emitter set"
  at frame 560, 3 of 3. Adds "title 0" to the regression targets.
  Reviewed: regress passed; title reached 3 of 3 (one capped, frame 560; two uncapped, frame 562).
- M10 boot loop (lane boot, iter 1, host-semantics): after START, d_a_title requests NAME_SCENE
  and its second create phase crashed (SIGSEGV addr=0 in `fpcMtd_Create`, frame 752).
  `dScnName_Create` runs `new (i_scn) dScnName_c()` on every phase; the class has no
  user-provided constructor, so clang value-initialises and zero-fills the process header that
  `fpcBs_Create` set (mpPcMtd, mProcName, ...). MWCC default-initialises. Same bug as actors in
  352bd0c. The scene and the five kankyo processes with the same pattern (dWpotWater_c,
  dWpillar_c, dWaterMark_c, dWindArrow_c, dThunder_c) use `new (p) T;` under `TARGET_PC`. Adds
  `native/check/input/file-select.txt` (START at frames 600/720/840). Next blocker: SIGSEGV
  addr=0x20 in `aurora::gfx::enqueue_pass` from `aurora_end_frame`, NAME_SCENE frame 752.
  Reviewed: regress passed; file-select now enters NAME_SCENE and gets past the create crash.
- M10 boot loop (lane boot, iter 2, host-semantics): the frame-752 SIGSEGV (addr=0x20 in
  `aurora::gfx::enqueue_pass`, `aurora_end_frame`) was Aurora growing its frame's pass list
  (`frame.ops.emplace_back`) through the game's global `operator new`, which on PC takes a block
  of the current JKRHeap: the heap NAME_SCENE left current had 0x750 bytes free for a 4048-byte
  request, so `pc_new` returned NULL. New PC-only `JKRPcHostAllocScope` (JKRHeap.h/.cpp, a
  thread-local depth): while open, the heapless global forms give host memory. `pc_frame.cpp`
  opens it around `aurora_update`, `aurora_begin_frame`, `aurora_end_frame` (+ COS_SHOT readback)
  and `aurora_get_stats`. Aurora allocations made inside the game's own GX/VI/PAD calls still
  use the current heap (JKRHeap.cpp TODO, phase 6). Next blocker: SIGSEGV addr=0x10 in
  `wether_move_vrkumo` (dKyeff2_Execute), NAME_SCENE frame 756.
  Reviewed: regress passed; file-select gets past frame 752 and stops at the frame-756 blocker.
- M10 boot loop (lane boot, iter 3, layout/H5): SIGSEGV addr=0x10 in `wether_move_vrkumo`
  (d_kankyo_wether.cpp:813, `new (0x20) dKankyo_vrkumo_Packet()`), NAME_SCENE frame 756. The
  current heap is the 0x68000 ExpHeap `dScnName_c::create` makes and leaves current; a temporary
  allocation log showed about 1,150 blocks in it (J2D screens' ~1,000 panes, dFile_select_c,
  dName_c, dFile_error_c, card_pictdata[9]), larger on the host because of 64-bit pointers, with
  1,872 bytes left for the 4,464-byte packet. `new` returned NULL and the constructor wrote
  through it (clang drops the game's NULL check after a non-noexcept `operator new`; a separate
  host-semantics issue, not fixed here). Under `TARGET_PC` the scene heap is 0x68000 * 2 (H5).
  Now NAME_SCENE runs and shows the memory-card "create a save file" prompt (shot at frame 1200);
  the run ends in exit 10 at frame 5530 with frames advancing because nothing reports
  `file-select` yet. Next: the M10 milestone probe.
  Reviewed: regress passed; file-select gets past frame 756, no fault, timeout in NAME_SCENE at frame 5531.
- M10 boot loop (lane boot, iter 4, harness): the game already reached M10 (NAME_SCENE drawn from
  frame 757, the memory-card "create a save file" prompt on screen), but no code reported it, so
  `file-select` timed out (exit 10) in NAME_SCENE at frame 5534 with frames advancing.
  `dScnName_c::draw` now calls `pc_name_scene_drawn(mMainProc, mMemCardCheckProc, mDrawProc)`
  (under `TARGET_PC`) at its end, and the new `native/src/pc/pc_file_select.cpp` polls from
  `pc_frame_end`, reading only what the hook recorded. A frame counts when the name scene drew
  since the previous frame with a screen up (draw procedure other than `NoneDraw`); after 60 such
  frames in a row it reports `file-select`. Runs log "name scene first drawn at frame 757" and
  "60 frames in a row with FileErrorDraw; main proc 0, memory card check proc 9" (MemCardCheckMain,
  MemCardMakeGameFileSel: the user card image has no the save file file yet) and MILESTONE file-select at
  frame 816, 3 of 3. Adds `file-select 0 --input native/check/input/file-select.txt` to the
  regression targets.
  Reviewed: regress passed (file-select included); 3 of 3 capped runs reach MILESTONE file-select at frame 816.
- M12 boot loop (lane outset, iter 1, host-semantics): SIGABRT in `aurora::gx::build_shader`
  (invalid WGSL `sampled0.a.r`) for an alpha stage using `GX_TEV_COMP_R8_GT`, sea room 44. First
  H11 patch, `native/patches/aurora/0001-alpha-stage-channel-compares.patch`, plus the patch
  mechanism in `native/cmake/Aurora.cmake` / `aurora_apply_patches.cmake` (see render issues).
  Reviewed: regress passed; `outset-debug --stage sea:44:206` runs ROOM_SCENE to frame 3732 in
  120 s with no fault (timeout: nothing reports M12 yet); shared Aurora checkout left clean.

- M12 boot loop (lane outset, iter 2, harness): `outset-debug --stage sea:44:206` timed out
  (exit 10) in ROOM_SCENE at frame 5531 with frames advancing: the game had reached M12, but no
  code called `pc_milestone("outset-debug")`. New `native/src/pc/pc_outset.cpp`: `pc_stage_created`
  arms it for the COS_BOOT_STAGE stage's start room and `pc_frame_end` polls it, reading game state
  only, until a PLAY_SCENE process executes with the requested start stage, the room is up (the M8
  room checks, now shared as `stageRoomReady` in `pc_title_stage.cpp`) and `dComIfGp_getPlayer(0)`
  is a PLAYER actor that finished creating; it reports outset-debug 300 frames later (until then it
  logs the first unmet condition). Link is in sea room 44 at frame 289 at (-195138, 1650, 313772);
  M12 at frame 589, 3/3 runs. Adds `outset-debug 0 --stage sea:44:206` to the regression targets.
  Reviewed: regress passed; 3/3 capped `outset-debug --stage sea:44:206` runs reached M12 in 15 s
  (Link in room 44 at frame 289); the probe only reads game state.
- **Step 4.16: events, camera, paths, message flow and maps** (2026-10-03, lane outset). Event
  list, stage camera/arrow records and paths were already big-endian (4.9a/c, M7 iter 6); the
  debug boot into Outset (`sea:44:206`, Aryll's lookout event) then hit four root causes in turn,
  each a separate change:
  1. *Event substance data* (`d_event_manager.cpp`): the FData (f32) and IData (int) arrays of
     event_list.dat, which actors read through `getMySubstanceP` as plain `f32*`/`Vec*`/`int*`,
     were host-order garbage, so the lookout event never advanced (letterbox and a camera out at
     sea in the M12 shots). `setData` swaps them in place once, as Dusklight's
     `dEvDtBase_c::init`; header byte `unk[0]` (0 on the disc in both lists logged, title and
     Outset) marks a swapped buffer.
  2. *STB message code* (`object-message.cpp`, file of 4.17): `adaptor_do_MESSAGE` read the u32
     message code host-order (0x00000357 became group 0x5703), `getMessageEntry` returned NULL
     and `dMesg_waitProc` faulted (SIGSEGV addr=0x18, ROOM_SCENE frame 831). Read as `BE(u32)`.
  3. *Message heaps* (`d_mesg.cpp`, H5): the 0xa32d ExpHeap `dMesg_waitProc` makes held 17 of
     the 18 `dMesg_outFont_c` on the host (1,872 bytes each, logged); `new JUTTexture` returned
     NULL (SIGSEGV addr=0x50 in `J2DPicture::insert`, frame 829). Under `TARGET_PC` it and its
     parent `dMsg_Create` heap (0xb6b5) are doubled.
  4. *Demo-actor parameters* (`d_demo.cpp`, `JGadget/binary.h`, files of 4.17): the block
     `JSGSetData` stores is big-endian STB data at odd offsets, read raw by `getP_Btp/Brk/BtkData`
     and through `TValueIterator_misaligned` by the demo actors; a byte-swapped BTP id faulted in
     `J3DAnmTexPattern::searchUpdateMaterialID` <- `daNpc_Ls1_c::demo` (frame 2128).
     `TParseValue_misaligned_` now assembles big-endian values on the host, and d_demo's ids go
     through it (`DEMO_PRM`). The rest of the STB raw values stay with 4.17.
  d_cam_param/d_cam_style/d_cam_type are compiled-in tables (no disc data); the game has no
  d_msg_flow (its flow is in code and the STB message objects above). Not done here:
  `mDoLib_cnvind16/32` byte-swap the little-endian AGB `.amp` dungeon maps (`map_dt_c`) and GBA
  buffers, which on a little-endian host is wrong; no run reaches a floor map yet (fixed by
  F2-agb-map below).
  Result: `outset-debug --stage sea:44:206` 3/3; an uncapped 4,000-frame run plays Aryll's
  lookout event with its messages ("I knew you'd be here!") with no fault; a capped 6,400-frame
  run tapping A through the event ends with Link free on the lookout, no fault. Uncapped, the
  same input stalled at frame 5856 in `JFWDisplay::calcCombinationRatio` (not fixed here, see
  render issues: likely `JUTVideo::sVideoInterval` 0 from back-to-back host retraces).
  Reviewed (round 1): regress passed; `outset-debug --stage sea:44:206` 3/3 in 15-16 s; the
  uncapped 4,000-frame Outset run finished with no fault. Step stays open: M13 waits for the
  outset-control probe (6.6), the uncapped VI stall and the AGB map swap.

- M13 boot loop (lane outset, iter 1, harness; step 6.6): `outset-control --stage sea:44:206`
  had no probe and no input script. `native/src/pc/pc_outset.cpp` now also reports outset-control:
  after Link is in the room it logs every change of `dComIfGp_event_runCheck`, measures each run of
  frames in which `g_mDoCPd_cpadInfo[0].mMainStickValue` > 0.5 and, when one reaches 120 frames,
  logs Link's horizontal displacement over it; one above 300 units makes Link controllable, and the
  milestone is reported once he is and 3,600 frames passed since he was in the room (read-only).
  New script `native/check/input/outset-control.txt`: B taps every 10 frames advance Aryll's
  lookout event (over at frame 3269; A is not used because, once Link is free, A next to Aryll
  starts a new talk), then six 140-frame stick holds; the lookout's railing blocks some directions
  (293, 120, 200 units), the fourth moves Link 353 units. M13 at frame 4100, 3/3 capped runs in
  132-133 s, identical positions. Adds the target to the regression list.
  Reviewed: regression passes; 3/3 capped runs reach M13 at frame 4100 (132 s) with identical
  probe lines. M13 reached.
- **F1-vi-stall: retraces spaced in OS time** (2026-10-03, lane outset, host-semantics). The
  uncapped Outset stall in `JFWDisplay::calcCombinationRatio` (render issues below): its loop steps
  by `JUTVideo::sVideoInterval`, the `OSGetTick` delta between two pre-retrace callbacks, and never
  ends on 0. On the console retraces are a field apart; the host VI made them on demand, so a burst
  (waitForTick catching up, JKRDvdRipper polling) gave callbacks a few ticks apart (instrumented
  uncapped run: about half the retraces 5-9 ticks after the previous one), and two in the same
  tick, or on Aurora's game clock while it is paused (window hidden or minimised: `OSGetTime` stops),
  give 0. `VIWaitForRetrace` (`native/sdk/src/vi/VIRetrace.cpp`) now holds a retrace until
  `OSGetTick` has advanced `COS_SDK_VI_MIN_RETRACE_US` (1 us, `cos_sdk/hooks.h`) past the end of the
  previous pre-retrace callback, waiting with the OS lock released; GameCube code unchanged. The
  `vi` SDK smoke test checks the measured interval over 1000 back-to-back retraces from two
  threads (fails without the change). The 4.16 stall did not reproduce before the change either
  (it is timing-dependent); after it: an uncapped 7000-frame Outset run tapping A from frame 400
  to 6400, 3/3 without a stall (8.35 ms a frame, as before); `outset-control` uncapped 2/2 (22-26
  s) and capped 1/1 (133 s); `run --frames 600` capped pacing ratio 1.0001. Adds
  `outset-control --uncapped` to the regression list.
  Reviewed: regression passes; `vi` smoke fails with the old VIRetrace.cpp and passes with the
  fix; uncapped `outset-control` and an uncapped 6200-frame Outset run with input pass; capped
  `run --frames 600` pacing ratio 1.0001.
- **F2-agb-map: AGB data in host order** (2026-10-03, lane outset, layout). The dungeon floor maps
  `m<N>.amp` (`map_dt_c`, read by `dMap_2DAGBScrDsp_c` and `dMap_RoomInfo_c`) are AGB data, stored
  little-endian, and the GBA link buffers (`dMap_c::mAgbSendBuf`) go to the GBA little-endian; the
  game converts both with `mDoLib_cnvind16/32`, a byte swap on the big-endian GameCube, which on a
  little-endian host turned every field into garbage (a 6054-byte map gave `getMapDtSize`
  1779826748 and its tile map at 0xfc010000 instead of 0x1fc). Under `TARGET_PC &&
  TARGET_LITTLE_ENDIAN` both functions return the value unchanged (`m_Do_lib.cpp`); GameCube code
  unchanged. No run reaches a floor map yet: a debug boot into a dungeon (`--stage kindan:0:0`)
  panics first in `dStage_memaInfoInit` (`d_stage.cpp:2932`: the MEMA chunk's
  `OFFSET_PTR(u32) m_entries` are read raw, so the room heap size is byte-swapped; not fixed here),
  and with `--audio off` faults earlier in `JAIZelBasic::sceneChange`. So the new smoke
  `COS_SMOKE=amp-sweep` (`native/src/pc/pc_amp.cpp`) mounts every `/res/Stage` archive (705) and
  puts each of the 180 `.amp` maps with its `m<N>.bti` through `dMap_2DAGBScrDsp_c::init`, then
  reads `getMapDtSize`, the pixel size, the tile map offset and every tile's info word (497,289) as
  the game does, against an independent little-endian reading: the file must be 0x3C header +
  graphics + tile map exactly, the pixel size must fit the tile count and every tile's texture
  cell must lie inside the `.bti`; a value stored as `dMap_c` fills the GBA buffer must give its
  little-endian bytes. Fails without the change (every map), passes with it 4/4. Adds `amp-sweep`
  to the regression list.
  Reviewed: regression passes (with `amp-sweep`); `amp-sweep` fails with the old m_Do_lib.cpp
  (705 archives, 0 maps, 541 errors) and passes with the fix (180 maps, 497,289 tiles, 0 errors).
- **F3-mema: stage chunk fields big-endian** (2026-10-03, lane outset, layout). The MEMA chunk's
  `dStage_MemoryMap_c::m_entries` was `OFFSET_PTR(u32)` read raw, so `dStage_memaInfoInit` asked
  for a byte-swapped room heap (0xb0870f00) and a dungeon boot (`--stage kindan:0:0`) panicked at
  `d_stage.cpp:2932`. Now `OFFSET_PTR(BE(u32))` (H1, as Dusklight). Audit of the other chunk structs
  found the same raw multi-byte reads in FLOR (`field_0x00` f32), DMAP (origin/scale/offsetY f32)
  and SHIP (`m_pos` cXyz, `m_angle` s16), now `BE(T)`; `dStage_setShipPos` copies the position to
  a host `cXyz` for `daShip_c::initStartPos` under `TARGET_PC`. GameCube layout unchanged.
  The kindan boot now loads stage and rooms (74 resources) and reaches ROOM_SCENE frame 293, then
  panics on a separate cause: `GFSetArray(attr 9, stride 12)` without an array size on Aurora from
  `dDlst_alphaModelData_c::draw` (not fixed here).
  Reviewed: regression passes; `sea:44:206` still reaches its milestone.
- **F4-boot-sweep: every stage of the disc booted** (2026-10-03, lane outset, harness). New
  diagnostic target `native/tools/run.sh boot-sweep` (driver `native/tools/boot_sweep.py`;
  `run.sh` gained `--run-dir DIR`). It lists the 156 stages on the disc (`/res/Stage/<name>/
  Stage.arc`, from the disc manifest, written first if missing), picks each start from the stage
  data and boots it with `run.sh run --stage <stage>:<room>:<point> --frames 600 --uncapped
  --audio on`, 4 at a time (`--jobs`), each run in `<sweep dir>/<stage>/`; the report
  `boot_sweep.txt` (stage, spec, source, exit code, PLAY start frame, last frame, signature) is in
  `build/native-mac/runs/boot-sweep-<ts>/`. Exit 0 only if every stage ran to its last frame.
  The start: the PLYR records `dStage_playerInit` would use (stage.dzs's when it has any, room =
  parameters & 0x3F; else each Room<N>.dzr's whose room bits name that room; point = angle.z &
  0xFF); a spawn point that some SCLS exit of the disc leads to (same room and start point) is
  preferred (108 stages), else the lowest room and point (47); the layer is left to the game.
  `Name` (the name-entry stage) has no PLYR record and is reported as skipped. `--list` prints
  the choices; `--only a,b` runs a subset. COS_FRAMES counts from boot: the PLAY scene starts the
  stage at frame 281-300, so a pass is about 300 frames in the stage. Runs of identical log lines
  (Aurora's per-draw `CP_REG_ARRAYBASE_ID` warnings) are collapsed to one line and a count (the
  whole sweep is 3.5 MB). Not in `regress_targets.txt` yet (the boot loop adds it once it passes).
  Current result, 2 sweeps identical (about 2 min each): 83 of 155 stages pass, 72 fail, 1
  skipped. A stage stops at its first fault, so a later fault of the same stage is hidden until
  the first is fixed. Failures (start `0:0` unless given):

  | Stages | n | Signature |
  |---|---|---|
  | Abesso, Abship, Asoko, Cave03, Cave04, Cave05, Cave06, Cave09, Comori, Edaichi, Ekaze, GanonB, GanonD, GanonE, GanonL, GanonN, Hyroom, I_SubAN (9:0), I_TestM, I_TestR, K_Testa, K_Testc, LinkRM (0:1), M2tower (0:16), M_DaiB, M_DaiMB (12:0), M_NewD2, MajyuE, Mjtower (0:16), Obshop (1:0), Omori, Otkura, Pnezumi, ShipD, SubD42, TF_03, TF_05, TF_07 (1:0), TyuTyu, WarpD, Xboss2, kindan, ma2room, ma3room, majroom, sea (1:0) | 46 | `PANIC GFGeometry.cpp:263, ROOM_SCENE, GFSetArray <- dDlst_alphaModelData_c::draw (d_drawlist.cpp:810)` (attr 9, stride 12, no array size on Aurora) |
  | PShip, SubD45, kenroom | 3 | `PANIC GFGeometry.cpp:263, ROOM_SCENE, GFSetArray <- dDlst_alphaModelData_c::draw (d_drawlist.cpp:902)` |
  | K_Test5 | 1 | `PANIC GFGeometry.cpp:263, ROOM_SCENE, GFSetArray <- dDlst_alphaModelData_c::draw (d_drawlist.cpp:819)` |
  | Xboss3, kazeB | 2 | `PANIC GFGeometry.cpp:263, ROOM_SCENE, GFSetArray <- daBwdg_packet_c::draw (d_a_bwdg.cpp:127)` |
  | GanonJ (1:0) | 1 | `PANIC GFGeometry.cpp:263, ROOM_SCENE, GFSetArray <- daMant_packet_c::draw (d_a_mant.cpp:236)` |
  | M_DragB, MiniHyo, VrTest, Xboss0 | 4 | `CRASH SIGABRT, ROOM_SCENE, aurora::gfx::TextureBind::get_descriptor (texture.cpp:332) <- aurora::gx::build_bind_groups (gx.cpp:518)` |
  | GTower, M2ganon | 2 | `CRASH SIGSEGV addr=0x0, ROOM_SCENE, JUTNameTab::getIndex (JUTNameTab.cpp:34) <- J3DAnmTextureSRTKey::searchUpdateMaterialID (J3DAnimation.cpp:908)` |
  | GanonA, Siren | 2 | `CRASH SIGSEGV addr=0x0, ROOM_SCENE, cBgW::Set (c_bg_w.cpp:337) <- daBg_c::createHeap (d_a_bg.cpp:211)` |
  | GanonM, M_Dai | 2 | `CRASH SIGSEGV addr=0x0, ROOM_SCENE, cBgW::Set (c_bg_w.cpp:332) <- daBg_c::createHeap (d_a_bg.cpp:211)` |
  | ADMumi (0:100) | 1 | `CRASH SIGSEGV addr=0x0, ROOM_SCENE, cBgW::Set (c_bg_w.cpp:332) <- daObjDoguuD_c::CreateHeap (d_a_obj_doguu_demo.cpp:41)` |
  | K_Test9, Opub | 2 | `CRASH SIGSEGV addr=0x0, ROOM_SCENE, J3DModel::J3DModel (J3DModel.cpp:21) <- mDoExt_J3DModel__create (m_Do_ext.cpp:3294)` |
  | K_Testd | 1 | `CRASH SIGSEGV addr=0x0, ROOM_SCENE, daWarphr_c::_draw (d_a_warphr.cpp:428) <- daWarphr_Draw (d_a_warphr.cpp:483)` |
  | E3ROOP | 1 | `CRASH SIGSEGV addr=0x8, ROOM_SCENE, C_MTXMultVec (mtxvec.c:11) <- JAInter::SeMgr::checkNextFrameSe (JAISeMgr.cpp:187)` |
  | the castle stage | 1 | `CRASH SIGSEGV addr=0x0, PLAY_SCENE (before the stage started), JAIZelBasic's per-frame BGM work (JAIZelBasic.cpp:470) <- JAIZelBasic::gframeProcess (JAIZelBasic.cpp:629)` |
  | GanonK | 1 | `PANIC JAISoundTable.cpp:61, ROOM_SCENE, JAInter::SoundTable::getInfoPointer (JAISoundTable.cpp:51) <- JAIBasic::startSoundVec (JAIBasic.cpp:220)` |
  | ENDumi | 1 | `PANIC d_event_data.cpp:1070, ROOM_SCENE, dEvDtStaff_c::specialProcPackage (d_event_data.cpp:802) <- dEvDtEvent_c::specialStaffProc (d_event_data.cpp:119)` |
  | Msmoke | 1 | `PANIC d_a_door10.cpp:356, PLAY_SCENE, daDoor10_c::CreateHeap (d_a_door10.cpp:268) <- fopAcM_entrySolidHeap (f_op_actor_mng.cpp:324)` |

  Passing (83): A_R00, A_mori, A_nami, A_umikz, Adanmae, Amos_T, Atorizk, Cave01, Cave02,
  Cave07, Cave08, Cave10 (1:0), Cave11 (1:0), DmSpot0, Ebesso, Fairy01-06, GanonC, H_test,
  ITest61-63, KATA_HB, KATA_RM (18:1), K_Test2/3/4/6/8/b/e, Kaisen, LinkUG (0:1), M_Dra09 (9:0),
  MiniKaz, Mukao, Nitiyou, Obombh, Ocean, Ocmera, Ocrogh, Ojhous, Ojhous2 (1:0), Omasao, Onobuta,
  Orichh, PShip2, PShip3, Pdrgsh, Pfigure, Pjavdou, SirenB, SirenMB (23:0), SubD43, SubD44,
  SubD51, SubD71, TEST, TF_01, TF_02, TF_04, TF_06, Xboss1, figureA-G, kazan, kaze (15:15),
  kazeMB (6:0), kinBOSS, kinMB (10:0), morocam, sea_E, sea_T (44:0), tincle.
  No crash fixed in this step.
  Reviewed: third sweep gives the same 83/72/1 result and the same signatures; regression passes.

- **Boot-sweep fix 1: the last `GFSetArray` callers pass their array size** (2026-10-03, lane
  outset). Most common sweep signature (54 of 72 failures): `PANIC GFGeometry.cpp:263` from
  `dDlst_alphaModelData_c::draw` (d_drawlist.cpp), `daBwdg_packet_c::draw` and
  `daMant_packet_c::draw`. Root cause: Aurora ignores CP_REG_ARRAYBASE, so `GFSetArray` stops
  loudly and every caller must use `GFSetArraySized` (step 2.7); these three draw paths were never
  converted. Under `#if TARGET_PC` they now pass the byte size and host byte order (`le = true`):
  the static bonbori/beam-check/cube/bonbori2 position assets, the current half of the bridge's
  double-buffered `mPos`/`mNrm` (0x1081 entries each) and its texcoord asset, and the cape's
  current `mPosition`/`mNormal` buffer and texcoord table. GameCube code unchanged in `#else`.
  Sweep: 83 -> 131 of 155 pass (24 fail, 1 skipped). Next blockers revealed by stages that got
  further: `PANIC d_bg_w_hf.cpp:365` in `dBgWHf::MakeNodeTreeRpHf` (Xboss3, kazeB),
  `JAISoundTable.cpp:61/70` in `SoundTable::getInfoPointer` (GanonK, M2tower), SIGSEGV in
  `cNd_LengthOf <- cLs_Addition` (sea 1:0); figureA crashed once in
  `JASystem::Kernel::portCmdMain` (passed in the previous sweep). Remaining older signatures are
  unchanged (TextureBind::get_descriptor x4, cBgW::Set x5, JUTNameTab::getIndex x2,
  J3DModel::J3DModel x2, others x1). Regression passes.
  Reviewed: a second sweep gives 132 of 155 (23 fail, 1 skipped) with no `GFSetArray` panic left;
  regression passes.

- **Boot-sweep fix 2: room memory blocks sized for host objects** (2026-10-03, lane outset,
  host-semantics, H5). Most common sweep signature left (5 of 23): SIGSEGV addr=0x0 in
  `cBgW::Set` (c_bg_w.cpp:332/337) from `daBg_c::createHeap` (GanonA, GanonM, M_Dai, Siren) and
  `daObjDoguuD_c::CreateHeap` (ADMumi). Root cause for the daBg cases: the MEMA chunk gives the
  room heap sizes in GameCube bytes; `daBg_c` builds the room's models, tev blocks and collision
  in a solid heap that takes the whole room block, and with 8-byte pointers and 16-byte aligned
  `operator new` it runs out, so `new cBgW_RwgElm[]` returns NULL and the constructor loop writes
  through it. `dStage_roomControl_c::createMemoryBlock` now doubles the size under `#if
  TARGET_PC`, as Dusklight does (CC0). Sweep: 132 -> 135 of 155 pass (20 fail, 1 skipped);
  GanonA, GanonM and M_Dai pass, Siren gets further and now fails in `J3DModel::J3DModel` like
  K_Test9/Opub. ADMumi is a different case (an actor solid heap from an estimate, not a room
  block) and still fails in `cBgW::Set`. Both remaining kinds share a second cause: on the host,
  `new T[n]` / `new T` with a constructor do not check a NULL result from the replaceable
  `operator new` (the game relies on NULL to fall back to a bigger heap); to be fixed separately.
  Regression passes.
  Reviewed: a second sweep gives 135 of 155 with no `cBgW::Set` crash from `daBg_c::createHeap`
  left (only ADMumi's actor heap); regression passes.

- **Boot-sweep fix 3: TEV stage BP commands read big-endian** (2026-10-03, lane outset, endian).
  Signature (4 of 20): `CRASH SIGABRT` `[fatal] invalid wrap mode 3` in
  `aurora::gfx::TextureBind::get_descriptor` (VrTest, Xboss0, M_DragB, MiniHyo). Root cause:
  `J3DTevStage::load` sends {reg, op, AB, CD} as one BP command word through a native `u32` read;
  on the host the register byte landed in the low bits, so every TEV stage wrote its combiner
  bytes to other BP registers (alpha stage 1, reg 0xC3, hit texture mode 0 of map 0 with wrap 3)
  and every BMD material's TEV setup was wrong. Under `#if TARGET_PC` both words are read as
  `BE(u32)`, as Dusklight does (CC0). Sweep: 135 -> 138 of 155 (17 fail, 1 skipped); VrTest,
  M_DragB, MiniHyo pass, Xboss0 now fails later in `dPa_J3DmodelEmitter_c` (d_particle.cpp:50)
  <- `hahen_set`. Same bug left in `getTexNoReg` (J3DTevs.cpp:134), next candidate.
  Reviewed: VrTest/M_DragB/MiniHyo reach their frames; a second sweep gives 139 of 155 (16 fail)
  with no wrap-mode abort; regression passes.

- **Boot-sweep fix 4: actor heap size -1 kept as "all free memory"** (2026-10-03, lane outset,
  host-semantics, H5). Most common signature left (3 of 16): SIGSEGV addr=0x0 in
  `J3DModel::J3DModel` (J3DModel.cpp:21) <- `mDoExt_J3DModel__create` (K_Test9, Opub, Siren; from
  `daObjTable::Act_c::CreateHeap` and other `MoveBGCreate(..., -1)` actors). Root cause: the H5
  doubling in `fopAcM_entrySolidHeap` also doubled the sentinel -1 ("take all free memory") to
  0xFFFFFFFE, which `mDoExt_createSolidHeap` does not recognise; `ALIGN_NEXT(0xFFFFFFFE, 0x10)`
  wraps to 0, so the actor got a header-only heap and its first `new J3DModel` returned NULL.
  Under `#if TARGET_PC` 0 and -1 are now passed through unchanged (and a size above 0x7FFFFFFF
  becomes -1 instead of wrapping). Sweep: 139 -> 142 of 155 (13 fail, 1 skipped); K_Test9, Opub
  and Siren pass. ADMumi (`cBgW::Set` <- `daObjDoguuD_c::CreateHeap`, estimate 0x1460) is the
  other cause noted in fix 2 (NULL from `operator new` not checked on the host) and still fails.
  Regression passes.
  Reviewed: K_Test9/Opub/Siren reach frame 600; a second sweep gives 142 of 155 (13 fail) with no
  J3DModel NULL crash; regression passes.

- **Boot-sweep fix 5: boot-sweep expected fails (GTower, M2ganon)** (2026-10-03, lane outset,
  harness). Signature (2 of 14 in a 141/155 sweep, tied with `d_bg_w_hf.cpp:365` and
  `JAISoundTable.cpp`): SIGSEGV addr=0x0 in `JUTNameTab::getIndex` (JUTNameTab.cpp:34) <-
  `J3DAnmTextureSRTKey::searchUpdateMaterialID` from `daPy_lk_c::dProcTool` ->
  `setDemoTextureAnime` (GTower, M2ganon). Cause: the debug boot, not game code. Link's stage
  cutscene asks for demo texture animations by file id (btp 368, btk 355); `phase_0` of
  d_s_play.cpp mounts `LkD01.arc` only when event flag 0x2D01 is set (by M2tower's `rescue.stb`,
  which the game always plays before these stages) and `LkD00.arc` otherwise. The debug boot's
  new file has no such flag, so it mounts LkD00, where id 355 is a `.btp`
  (`23_cl_cut07_gwaitturn_o.btp`; in LkD01 it is `42_cl_cut1_l.btk`); the loader builds a
  texture-pattern animation that is then used as an SRT animation. Checked: with LkD01 forced
  (temporary edit, reverted) both stages reach frame 600. `boot_sweep.py` gained
  `EXPECTED_FAIL` (stage -> signature regex and reason): a listed stage that fails with exactly
  that signature is reported `xfail` with the reason and does not fail the sweep; any other
  failure still fails it, and a pass is reported `xpass` (remove the entry). **Boot-sweep
  expected fails:** GTower and M2ganon (GanonK added by fix 8), reason above. Found on the way, not fixed here (next
  candidate, own commit): `dProcTool` reads the btp id with a native `*(u16*)` of the STB
  parameter data (`setDemoTextureAnime(*(u16*)(sp9C.begin() + 1).get(), ...)`,
  d_a_player_dproc.inc), so on the host it is byte-swapped (368 -> 28673) and
  `findIdResource` indexes `mFiles + 28673` out of bounds before its search; the btk id goes
  through the big-endian iterator and is right.
  Reviewed: a full sweep reports GTower and M2ganon as xfail with the listed signature (141 of
  155 pass, 12 fail, 2 expected fails; Xboss0's d_particle.cpp:50 crash is intermittent and
  unrelated); regression passes.

- **Boot-sweep fix 6: the height-field grid index table read big-endian** (2026-10-03, lane
  outset, endian). Most common signature left (2 of 11 in a 142/155 sweep, tied with
  `JAISoundTable.cpp`): `PANIC d_bg_w_hf.cpp:365` (`CHECK_MINMAX_2`) in
  `dBgWHf::MakeNodeTreeRpHf` <- `daBwdg_Execute` (Xboss3, kazeB: the sand of the Molgera fight),
  about 150 frames into the stage. A temporary trace showed leaf node 63 with min.z 0 and max.z
  -897.6, values no grid vertex has. Root cause: `dBgWHf::CalcPlane` reads the grid's triangle
  indices from `mC8`, the `u16` table of Bwdg's GridIdx.dat read in place, through a native
  `u16` read. On the host each index came out byte-swapped, e.g. 1 became 256 and some went up
  to 0xFF1F against 8192 triangles, so `pm_tri[triIdx].m_plane.SetupNP0` wrote planes outside the
  table and corrupted the actor's solid heap, here the collision node tree. `mC8` and the `Set`
  parameter are now `BE(u16)*`, which is plain `u16*` on GameCube. Sweep: 142 -> 144 of 155 (9
  fail, 2 expected fails, 1 skipped); Xboss3 and kazeB reach frame 600. Regression passes. Reviewed:
  Xboss3 and kazeB re-run to frame 600, regression re-run passes.

- **Boot-sweep fix 7: the JStudio sound adaptor reads its STB operands big-endian** (2026-10-03,
  lane outset, endian). Most common signature (2 of 12 in a 143/155 sweep): `PANIC
  JAISoundTable.cpp:61/70` (invalid category) in `JAInter::SoundTable::getInfoPointer` <-
  `JAIBasic::startSoundVec` <- `JStudio_JAudio::TAdaptor_sound` while a demo STB plays (GanonK,
  M2tower). Root cause: `TAdaptor_sound::adaptor_do_SOUND` read the 4-byte STB sound-ID operand
  with a native `*(s32*)`, so on the host the ID was byte-swapped and its category bits garbage;
  `adaptor_do_LOCATED` read its 4-byte operand as an 8-byte host `Vec*`. Both now read
  `BE(u32)` under `TARGET_PC` (after Dusklight JStudio_JAudio2/object-sound.cpp, CC0). Sweep:
  143 -> 145 of 155; M2tower reaches frame 600; GanonK now stops at the LkD00/LkD01 demo-archive
  signature of fix 5 (`JUTNameTab::getIndex` <- `searchUpdateMaterialID`). Reviewed: GanonK and
  M2tower re-run (no JAISoundTable panic), regression passes.

- **Boot-sweep fix 8: Link's demo texture-pattern id read big-endian** (2026-10-03, lane outset,
  endian). Most common signature (3 of 12 in a 143/155 sweep, 2 of them the fix-5 expected
  fails): SIGSEGV in `JUTNameTab::getIndex` <- `J3DAnmTextureSRTKey::searchUpdateMaterialID` <-
  `daPy_lk_c::setDemoTextureAnime` <- `dProcTool` (GanonK, GTower, M2ganon). Root cause (noted in
  fix 5): `dProcTool` read the btp file id of the STB parameter data with a native `*(u16*)`
  (d_a_player_dproc.inc), so on the host 368 became 28673 and `findIdResource` indexed past the
  archive's file table. Under `TARGET_PC` it now reads through the big-endian misaligned
  iterator, as `front()`/`back()` already do (GameCube line kept in `#else`). With the right id
  the three stages stop at the LkD00 btp/btk mismatch of fix 5 in
  `J3DAnmTexPattern::searchUpdateMaterialID`; checked with LkD01 forced (temporary edit,
  reverted): GTower and M2ganon reach frame 600, GanonK gets past Link's cutscene and stops at
  the `fpcLyTg_ToQueue` signature also seen in Atorizk. GanonK (the final battle, after M2tower's
  rescue) joins the expected fails for the same 0x2D01 reason; the expected-fail regex now
  accepts both the btk and btp form of that crash. Sweep: 143 -> 145 of 155 (7 fail, 3 expected
  fails, 1 skipped; Atorizk and I_SubAN passed this run, both intermittent).
  Reviewed: regression passes; sweep 144/155 (8 fail incl. intermittent Atorizk, 3 xfail):
  GanonK and M2ganon now stop in `J3DAnmTexPattern::searchUpdateMaterialID`, GTower in the btk
  form (`JUTNameTab::getIndex` <- `J3DAnmTextureSRTKey`) of the same LkD00/LkD01 mismatch.

- **Boot-sweep fix 9: `new` checked for NULL as MWCC did (`-fcheck-new`)** (2026-10-03, lane
  outset, host-semantics). Every failure of a 145/155 sweep had its own signature; this one dates
  from fix 2: SIGSEGV addr=0x0 in `cBgW::Set` (c_bg_w.cpp:332, inlined `SetTri`) <-
  `daObjDoguuD_c::CreateHeap` (ADMumi). Root cause: the doubled estimate (0x28c0) is too small on
  the host, so `new cBgW_TriElm[n]` gets NULL from the full JKRSolidHeap. The game handles that
  (`if (pm_tri == NULL) return true;`, and `fopAcM_entrySolidHeap` then retries with a measured
  heap), but clang assumes a non-`noexcept` `operator new` never returns NULL: it wrote the
  `new[]` array cookie to address 0 before the check, and dropped `SetVtx`'s NULL check
  altogether. MWCC checked first. `-fcheck-new` (C++ game units, native/cmake/GameConfig.cmake;
  GCC accepts it too) restores the check everywhere; no game source changes. ADMumi now logs the
  game's own "見積もりヒープサイズ(000028c0)で登録失敗" and reaches frame 600 on the retry. Sweep:
  145 -> 146 of 155 (6 fail, 3 expected fails, 1 skipped). Regression passes.

- **Boot-sweep fix 10: the fast create request allocated at its host size** (2026-10-03, lane
  outset, layout). Every failure of a 146/155 sweep again had its own signature; this one is on
  the main sea stage: SIGSEGV addr=0x128020000000138 in `cNd_LengthOf` (c_node.cpp:20) <-
  `cLs_Addition` <- `cTg_Addition` <- `fpcCtRq_Create` <- `fpcFCtRq_Request` <-
  `daObj_Canon_c::attackCannon` (sea 1:0, frame 480, a cannon's `fopAcM_fastCreate` of a bomb).
  Root cause: `fpcFCtRq_Request` (f_pc_fstcreate_req.cpp) allocated its request with the
  GameCube literal 0x50, while `fast_create_request` is larger on the host (64-bit pointers in
  `create_request`), so writing `mpFastCreateFunc`/`mpFastCreateData` overran the block into the
  next heap block and corrupted a create-request list node; the next create walked into it.
  Under `TARGET_PC` it now allocates `sizeof(fast_create_request)` (GameCube literal kept in
  `#else`); the standard create request already used `sizeof`, and no other `memalignB` takes a
  literal size. sea 1:0 now reaches frame 900. Sweep: sea passes, but I_SubAN (9:0) now fails
  3 runs out of 3 (the build without this fix passes it 3 out of 3): at frame 452 an event
  starts that did not start before, its Ywarp00 (params 0xFF: no switch, active from the start)
  sends Link out by room 9's SCLS 0 (sea, start 1, room 47), and `dStage_playerInit` stops at
  `PANIC d_stage.cpp:1787` (`i != num`) because sea room 47's PLYR list has points 0, 5 and
  100-103 but no 1. I_SubAN is a test stage the game never reaches; whether that SCLS is broken
  data or the event is reached for another reason is left for the next iteration. 146 of 155
  (6 fail, 3 expected fails, 1 skipped). Regression passes. Review: regression passes; sea 1:0
  reaches frame 900 capped and uncapped; the reviewer's sweep gave 147 of 155 with I_SubAN passing
  (5 fail: E3ROOP, ENDumi, the castle stage, K_Testd, Msmoke), so I_SubAN's warp-out is timing-dependent.
- **Boot-sweep fix 11: the debug stage boot registers the audio camera** (2026-10-03, lane
  outset, harness). the castle stage: SIGSEGV addr=0x0 in `JAIZelBasic's per-frame BGM work` (JAIZelBasic.cpp:470,
  stage BGM 0x35 reads `mAudioCamera->field_0x0->x`); E3ROOP: SIGSEGV addr=0x8 in `C_MTXMultVec` <-
  `JAInter::SeMgr::checkNextFrameSe` (JAISeMgr.cpp:187, `mAudioCamera[0].field_0x8` NULL). Root
  cause: `JAIZelBasic::initSe` (from `mDoAud_Create`) leaves the audio camera NULL and only a camera
  actor sets it (d_camera.cpp `init_phase1`/`camera_draw` via `mDoAud_getCameraInfo`); the real
  game always passes the sea_T PLAY scene (title) first, so a camera is registered before any stage,
  but `COS_BOOT_STAGE` skips it. `pcBootStage` (d_s_logo.cpp, `TARGET_PC` harness) now registers it
  as `init_phase1` does: eye (1e7, 1e7, 1e7) in a static (the game keeps the pointer), j3dSys's view
  matrix, camera 0. E3ROOP passes; the castle stage reaches ROOM_SCENE and stops at the K_Testd signature
  (`daWarphr_c::_draw`, d_a_warphr.cpp:428). Review: regression passes; the reviewer's sweep gave
  147 of 155 (E3ROOP ok, the castle stage past the blocker; fail: ENDumi, the castle stage, I_SubAN, K_Testd, Msmoke),
  I_SubAN being the timing-dependent warp-out of fix 10 (also seen before this change).

- **Boot-sweep fix 12: automatic variables zero-initialized in game units
  (`-ftrivial-auto-var-init=zero`)** (2026-10-03, lane outset, host-semantics). the castle stage and K_Testd:
  SIGSEGV addr=0x0 in `daWarphr_c::_draw` (d_a_warphr.cpp:428) <- `daWarphr_Draw`. The source checks
  `if (demoCamera != NULL)`, but the disassembly had no check: it called the camera's vtable
  directly. Root cause: on the NULL path `sp1C`/`sp08` stay uninitialized and are then passed to
  `transS`/`YrotM`. Clang marks arguments `noundef`, so passing an uninitialized value is
  undefined behaviour, and it deleted the whole NULL path as unreachable. MWCC just used stack
  garbage. `-ftrivial-auto-var-init=zero` (native/cmake/GameConfig.cmake, all game units; GCC 12+
  accepts it) gives every local defined contents, so clang keeps branches like this one. No game
  source changes. The `cbz x0` is back in `_draw`. Sweep: 148 -> 150 of 155 (fail: ENDumi,
  Msmoke; 3 expected fails, 1 skipped). Regression passes.
  Reviewed: regression passes; reviewer sweep 150 of 155 (the castle stage, K_Testd ok).

- **Boot-sweep fix 13: boot-sweep expected fail ENDumi (the ending needs layer 8)** (2026-10-03,
  lane outset, harness). Signature (1 of 2 failures in a 150/155 sweep, tied with Msmoke's
  `d_a_door10.cpp:356`): `PANIC d_event_data.cpp:1070` in `dEvDtStaff_c::specialProcPackage` <-
  `dEvDtEvent_c::specialStaffProc` (ENDumi). The `JUT_ASSERT(demo_data)` of the PLAY action:
  the stage event asks for `ending.stb`, which is in neither the Stage archive nor a demo archive.
  Cause: the debug boot, not game code. Room 0's LBNK is `ff x8, 25, ff x3`: only layer 8 has a
  demo bank (0x25), so `d_s_room.cpp` mounts `Demo37.arc` (with `ending.stb`) only on layer 8. The
  debug boot's layer -1 resolves to layer 0 (no bank, no demo archive). The disc's debug stage
  list also enters ENDumi with layer 8. Checked (temporary logging, reverted): booted as
  `ENDumi:0:0:8`, Demo37 is mounted, `ending.stb` is found and the ending starts; it then stops at
  the LkD01 signature of fix 5 (`JUTNameTab::getIndex` <- `setDemoTextureAnime`), since the ending
  also needs event flag 0x2D01. `boot_sweep.py` lists ENDumi in `EXPECTED_FAIL` with the
  `d_event_data.cpp:1070` signature and this reason. **Boot-sweep expected fails:** GTower,
  M2ganon, GanonK (LkD01), ENDumi (layer 8, then LkD01). Left: Msmoke (`door10.bdl` asked by
  `daDoor10_c::CreateHeap`, while its Stage archive only has `bmdc/door10.bmd`; to be checked).
  Reviewed: regression passes; `ENDumi:0:0:8` gets past `d_event_data.cpp:1070` and stops at the
  LkD01 `JUTNameTab::getIndex` signature; reviewer sweep 150 of 155 (1 failure Msmoke, 4 xfail).

- **Boot-sweep fix 14: boot-sweep expected fail Msmoke (the disc's Stage archive lacks door10.bdl)**
  (2026-10-03, lane outset, harness). Signature (the only failure in a 150/155 sweep):
  `PANIC d_a_door10.cpp:356` in `daDoor10_c::CreateHeap` (d_a_door10.cpp:268, `JUT_ASSERT(modelData)`)
  <- `fopAcM_entrySolidHeap` (Msmoke). Cause: the disc data, not the port. Msmoke's stage.dzs
  places one `door10` (TGDR, params 0x0FFFF0FF: type 0, `home.angle.z` 63 so arg1 0 and m364 0),
  so `CreateHeap` asks `dComIfG_getStageRes("Stage", "door10.bdl")`. Read straight from the
  disc's RARC (disc_manifest.py helpers), Msmoke/Stage.arc has a root file named `bdl` (3072
  bytes), `bmdc/door10.bmd`, `dzb/door10.dzb`, `dzb/door11.dzb`, but no `door10.bdl`;
  `dRes_control_c::getRes` matches names with `strcmp`, exactly as the original, so the original
  game gets NULL too and stops at the same assert (a retail build would dereference NULL in
  `mDoExt_J3DModel__create`). No other stage with a bmd-only door10 places a door10 actor.
  `boot_sweep.py` lists Msmoke in `EXPECTED_FAIL` with the `d_a_door10.cpp:356` signature and
  this reason; the docstring now also admits a stage whose disc data the original cannot run.
  No game code changes. Sweep: 150 of 155, 0 failed, 5 expected fails, 1 skipped (x2); a third
  sweep had 2 intermittent failures, each its own signature, both new to this list:
  Cave10 (1:0) `CRASH SIGSEGV addr=0x0` in `JASystem::Kernel::portCmdMain` (JASCmdStack.cpp:121)
  <- `subframeCallback` on the audio thread, and I_SubAN (9:0) `PANIC d_stage.cpp:1787` in
  `dStage_playerInit` after the warp-out to sea room 47 (frame 558; the timing-dependent one noted
  in fix 11). **Boot-sweep expected fails:** GTower, M2ganon, GanonK (LkD01), ENDumi (layer 8,
  then LkD01), Msmoke (disc data).
  Reviewed: Msmoke/Stage.arc re-read from the disc (no `door10.bdl`), getRes `strcmp` confirmed;
  regression passes; reviewer sweep 149 of 155 (Msmoke xfail, 1 failure I_SubAN `d_stage.cpp:1787`).

- **4.17 JStudio and demos** (2026-10-03, lane j3d): `COS_SMOKE=stb-sweep` 0 x3; the report equals
  the manifest (1319 archives, 54 STB files, 1025 objects; 118406 frames played, 41.5 million
  variable values checked, max |v| 500000). Builds on the boot loops' STB/FVB big-endian
  commits (5395c07, 444a2fe, cfcd7bd, e0b30df, 8eb9db8), which already let every STB of the disc
  parse and play; each item below is meant as its own commit:
  - **Harness:** `pc_stb.cpp` mounts every archive under /res (Yaz0 entries expanded) and plays
    every STB as `dDemo_manager_c` does (`JStudio::TControl` at 1/30 s per frame, `TParse::
    parse_next` with flags 0, `forward(0)`, then `forward(1)` until it returns false) with a
    `TCreateObject` that makes the JStudio object of each kind (JACT, JCMR, JABL, JLIT, JFOG,
    JPTC, JSND, JMSG) over a null adaptor that only records its operations; a suspend of the
    control object (the message window holding the demo) is released when seen, as d_mesg does.
    Every frame every variable value must be finite and below 1e7, every object must end within
    100000 frames, and the objects must give the sweep heap back all it lent. `stb_sweep.txt`
    goes to `disc_manifest.py --check-stb` (manifest version 8), which walks each object's
    sequence from the file bytes and compares per file the version, block, JFVB object and object
    counts and the suspend total, and per object its type, ID, flag and its `do_paragraph`,
    `do_data` and waited-frame counts (frames run must be at least the longest wait). No STB on
    the disc has a flag or jump entry (the manifest rejects one). Negative checks: reading the
    IMMEDIATE operand host-order gives 24983 errors (values of 2.7e+23); a report with one wait
    and one paragraph count off gives 2 manifest DIFFs. `stb-sweep 0` joins the regression.
  - **`TAdaptor_actor::TVVOutput_ANIMATION_FRAME_` read the play mode at a GameCube offset.**
    `_08` (317, 321) is the GameCube offset plus 1 of `m13C`/`m140` (ANIMATION_MODE,
    TEXTURE_ANIMATION_MODE); on the host it landed elsewhere in the adaptor: a temporary trace
    in title-stage read 0x4943d20 instead of 0 for the texture-animation frame (outside-function
    index 0x20, reverse flag 0x3d). On PC the field is named (Dusklight's object-actor.cpp).
  - **Group G markers.** `TParse_TSequence`/`TParse_TParagraph::getData`, `getSequence_offset`
    and `adaptor_setVariableValue_n` already add 32-bit sizes and offsets at host width: the TODOs
    become plain comments. The 0x81 data paragraph's size and the B-spline list's key counts are
    now pointer differences instead of differences of truncated addresses, and
    `TFunctionValue_composite::TData(void*)` keeps the whole pointer in `rawData` (its low half
    is `uintdata` on the host; only `initialize` passes one, NULL). Group G 9 -> 0 (baseline).
  Step verification: stb-sweep 0 x3 equal to the manifest, title-stage 0, title 0, logo-res 0;
  inventory `--check` ok; `unifdef -UTARGET_PC` of the changed game files equals HEAD.
  Not done (outside the null-adaptor sweep, for M14): `JStudio_JAudio` (`object-sound.cpp`, the
  SOUND id twice), `JStudio_JMessage` (`object-message.cpp`, the message code) and
  `JStudio_JParticle` (`object-particle.cpp`, the PARENT_NODE id) still read their STB operands
  host-order (the particle's PARENT_ENABLE word also comes from the host-order BOOL output); there is no `ctb` unit in the game decomp.
  (The message code was fixed meanwhile on feature/switch-native by 0915023.)
  Reviewed: regress passed (stb-sweep in it); stb-sweep 0 equal to the manifest; `unifdef
  -UTARGET_PC` of every changed game file equals HEAD; committed as three commits (actor play
  mode, group G markers, harness).

- R1-lighting (lane boot, render): characters and J3D models drew black or posterised grey.
  Cause 1: J3D textures were never bound (Aurora ignores the BP image-pointer writes of
  `loadTexNo`); `J3DTexture` keeps GX texture/TLUT objects and `J3DTevBlock::loadTexture` loads
  them before each material display list (TARGET_PC, Dusklight pattern). Cause 2: Aurora fed
  `GX_TG_SRTG` texgens the raw vertex colour instead of the lit channel; H11 patch
  `0002-srtg-texgen-lit-colour.patch`. See render issues.
  Reviewed: regress passed; title shots 900/1300 show sky, clouds, subtitle and Link in his
  colours with toon shading; outset-debug shot 400 shows the horizon cloud band. Committed as two
  commits (one per cause).

- R2-textures (lane boot, render): the name-scene backdrop's multicoloured blocks and the title's
  garbled "the game" subtitle were already gone with R1-lighting's J3D texture binding
  (title shot 600 and file-select shots 900/1200 show the subtitle and the cloudy sky before this
  step). What remained on the name scene were long white lines across the sky: Aurora drew the
  3-vertex `GX_QUADS` of `dKyr_drawStar` with a fourth index past the primitive. H11 patch
  `0003-quads-three-vertex-triangle.patch`. See render issues.
  Reviewed: patch matches Dolphin `IndexGenerator::AddQuads`, applies to clean 3227d76;
  file-select shots 900/1200 show stars without lines, title shot 600 intact; regress passes.

- R3-title-hud (lane boot, render triage, no code change): the title showed the gameplay HUD
  (hearts, rupees, A/B/X/Y/Z, "Crouch", camera arrows), Link in gameplay, a black jagged King of
  Red Lions left of the logo and a green striped pole. Already fixed by integrated work: the run
  that showed it (lane shot, `run --frames 3000 --shot 300,1500,3000`, shot 1500) logs
  "デモデータ読み込みエラー！！", i.e. it predates the M8 boot loop's STB fixes (iterations 1-5,
  8eb9db8 and the JStudio commits). With no title demo, no event ran, so `dMeter_statusCheck`
  never saw `dComIfGp_event_runCheck()` and the meter drew as in play; with the demo running the
  meter is hidden as on the GameCube. The black boat and the striped pole were J3D materials
  sampling stale textures, fixed by R1-lighting (3c3f2ae). See render issues.
  Checked with COS_SHOT on the current tree: `run --frames 1310 --shot 600,900,1300` and
  `run --frames 3010 --uncapped --shot 1500,2000,2500,3000` show no HUD in any frame (title over
  Outset, Link on the cliff, open sea, then the prologue scroll and text).
  Reviewed: the before-run (lane shot, 14:47) predates the STB commits 5395c07..8eb9db8
  (14:56-15:21); reran both runs on 28c72a8, no HUD and no demo-data error; regress passes.

- R4-arraybase (lane boot, render): `J3DShape::loadVtxArray` wrote CP 0xA0+n array bases (341205
  "not supported" lines in 1500 frames of sea room 44); under `TARGET_PC` it now sends
  `GX_AURORA_LOAD_ARRAYBASE` with pointer, size and byte order. Reviewed: rerun logs 0 lines, shots
  match the ones before, GameCube path unchanged (unifdef), regress passes. See render issues.

- **Step NG-probe: probes for the real new-game flow; M11 new-game reached intermittently, M14
  outset-real defined** (2026-10-03, lane audio, harness only). Milestone table rows M11 and M14
  now have their own criteria (docs/NATIVE_PORT_PHASE4_6.md). `pc_new_game.cpp` reads game state only:
  every change of the name scene's main / memory card / draw procedures (`[cos] name-scene:`), the
  prologue's `dScnOpen_proc_c::mState`, the PLAY scene's arrival, event and STB demo changes, and
  until each milestone the first unmet condition. Both milestones start from a clean card:
  `prepareRunCard` (pc_save.cpp, shared with `COS_SMOKE=save`) points slot A at
  `<run dir>/card/` before CARDInit. Script `native/check/input/new-game.txt`.
  - **M11 new-game reached, but only intermittently:** title START x2 -> name scene at frame ~758 ->
    `MemCardMakeGameFileSel` (No preselected; stick left, A) -> `MemCardMakeGameFile` ->
    `MemCardMakeGameFileCheck` (the GCI file is on the card) -> A -> `FileSelectMain` (~1050) ->
    A, A -> `NameInMain` (~1228) -> A x3, START, A -> `changeGameScene` -> OPEN scene executing
    at ~1460 with player name "AAA"; MILESTONE new-game at ~1522. On an idle machine 3/3
    uncapped (7-12 s) and 1/1 capped (46 s); under load (4 runs in parallel, or inside
    `regress.sh -j 3`) about half the runs fail: `MemCardMakeGameFile` lasts 1 frame (912 ->
    913) instead of about 12, `mDoMemCd_SaveSync()` returns 2, the scene goes
    `MemCardMakeGameFileCheck` -> `MemCardErrMsgWaitKey` (message 0x18) -> `MemCardStatCheck` and
    stays there until the timeout (exit 10). Two causes, fixed in the M11 boot loop: fix
    NG-run-dir (parallel runs shared one card) and fix NG-memcard-sync (the save race the NG-probe
    suspected), both below. `save` and `file-select` still pass.
  - **M14 outset-real: where the run stops.** The prologue plays all its states (0 -> 44, frame
    ~1460 -> ~8000, no input) and the OPEN scene requests the PLAY scene for the save's return
    place (sea room 44 point 206). The PLAY scene is then never created: no resource is requested
    after it (`--trace res,scene`: last `/res/Object/Opening.arc`), and a temporary trace in
    `dScnPly_Create` (not committed) showed the phase handler stuck in `phase_00` for 20,000+
    frames, which returns `cPhs_INIT_e` while `mDoAud_isUsedHeapForStreamBuffer()` is true: the
    prologue's streamed BGM still holds the audio heap's stream buffer after the OPEN scene is
    deleted (the stream is never released on the host; audio streams, step 5.6 area). The probe
    logs `outset-real: frame N: waiting: PLAY scene not executing` every 600 frames (exit 10 at
    the timeout). The script's lines after frame 8200 (B taps through the intro event, then six
    stick holds) follow outset-control.txt's timing and are provisional until the PLAY scene runs;
    the M14 boot loop retimes them.
  Render: the prologue draws correctly (shot 2000: the tapestry with its text). Not blocking: the
  memory card dialog shows its frame without message text (shot 900), and the name scene logs
  `CP_REG_ARRAYBASE_ID is not supported` on every frame (already listed below).
  Review: `new-game` reached at frame 1522 (uncapped, 11 s); `outset-real` still waits for the
  PLAY scene at frame 27,822 (timeout); `regress.sh -j 3` all checks passed.
- R5-cpu-skinning (lane boot, render): two root causes. (1) Every paired-single routine of
  J3DTransform had no host body (`__MWERKS__` asm only): the four `J3DPSMulMtxVec` overloads and
  `J3DPSCalcInverseTranspose`, `J3DScaleNrmMtx`/`J3DScaleNrmMtx33`, `J3DMtxProjConcat`,
  `J3DPSMtx33Copy`/`CopyFrom34`, `J3DPSMtxArrayConcat`, `__MTGQR7`; so besides CPU skinning, J3D
  normal matrices, weighted-envelope draw matrices and projected texture matrices were never
  written on the host. Under `TARGET_PC` they are plain C (after Dusklight, CC0), the S16Vec forms
  keeping GQR7's load/store scales (`j3dHostGQR7`, truncate and clamp as Dolphin's
  `ScaleAndClamp`). (2) `J3DSkinDeform::deformVtx*` read the model's big-endian VTX1 arrays host
  order: under `TARGET_PC` they read each source vector big-endian and store the result big-endian
  (the order `loadVtxArray` declares to Aurora), and `dBgWDeform`, whose collision used the model's
  current positions as its host-order vertex table, gets a host-order copy (`mHostVtx`).
  Check: `run --stage Omori:0:3 --frames 900 --shot 490,890` (Forest Haven): before, the Great Deku
  Tree (`daNpc_De1_c`, CPU-skinned through `dBgWDeform`, 938 F32 positions) is missing, after it
  draws with its face and branches; `run --frames 1310 --shot 900,1300` (title) and
  `run --stage sea:44:206 --frames 1500 --shot 400,800,1200,1490` (Outset) unchanged but for the
  wind streaks now drawn on the title; `unifdef -UTARGET_PC` of the five files equals HEAD (one
  blank line). See render issues.
  Reviewed: before/after `Omori:0:3` shot 890 rerun (tree missing, then drawn), Outset shot
  unchanged, GameCube path unchanged (unifdef), regress passes. Committed as two commits (the
  J3DTransform host bodies, then the skin deform byte order).
- R6-grass (lane boot, render): the GX-direct packet material lists (grass, flowers, trees, bushes,
  chains, hookshot, bwdg sand, tree shadows) name their texture by physical address in BP
  SETIMAGE3, which Aurora never reads; under `TARGET_PC` `mDoLib_loadDLTexImage` loads a cached
  `GXTexObj` built from the list's own SETMODE0/1 and SETIMAGE0 before each list. See render
  issues. Reviewed: `run --stage sea:44:206 --frames 820 --shot 800` (white flowers cut out on the
  sand) and `run --stage Omori:0:3 --frames 500 --shot 490` rerun and inspected, all changes under
  `TARGET_PC`, `regress.sh -j 3` all checks passed.
- R7-korl-dark (lane boot, render): the title's King of Red Lions drew dark brown because Aurora's
  `GXInitLightDistAttn` kept `GX_DA_GENTLE` for a reference brightness of 0 (k1 = inf), so the
  2D-list light of `setLight()` contributed nothing; H11 patch
  `0004-light-dist-attn-zero-brightness.patch` matches the SDK's `<= 0` test. See render issues. Reviewed:
  `run --frames 1310 --shot 900,1300` rerun and inspected (red ship with white and gold trim), patch
  applies to untouched 3227d76, `regress.sh -j 3` all checks passed.
- RA-A1 (lane boot, render audit A1): the arena floors were not a render bug. The room-entry
  event camera (`fixedFrameEvCamera`, then `uniformTransEvCamera`, `WaitAnyKey` on) takes its
  eye and centre relative to `RelActor` "@PLAYER"; `dCamera_c::getEvActor` matched that name by
  reading its first four bytes as a u32 and comparing with `'@PLA'`, which on a little-endian
  host never matches, so no actor was found and the offsets became world positions (eye
  (0,-40,-170), centre (0,-20,0) in GanonE): the camera sat under the floor, which is culled from
  below (`GX_CULL_BACK`), so the clear colour showed. The same host-order tag read broke the
  event "Trim" modes (`'STAN'`...) and `styleEvCamera`'s style search (`'FN01'`, index -1).
  Under `TARGET_PC` the new `dCamera_strTag` (d_camera.h) reads the four characters big-endian
  as the console does. See render issues (A1). Reviewed: GanonE:0:0 / M_DaiMB:12:0 shot 1190 and
  kinBOSS:0:0 shot 900 rerun and inspected (Link from behind on the floor), `unifdef -UTARGET_PC`
  unchanged, `regress.sh -j 3` all checks passed.
- RA-A2 (lane boot, render audit A2): Kalle Demos renders correctly; the A2 picture came from
  A1's camera. Under the floor (A1 reverted for diagnosis), the camera saw the boss's open flower
  (`bkm.bmd`, the `hana_wait` pose at the arena centre) from below. The floor is culled from below,
  so the undersides of the petals (material `hana_sitahana`) showed as large flat blue, magenta
  and cyan polygons against the clear colour. Their jagged edges are the petal outlines of the
  `bkm_body` texture. Nothing was changed in the code. Checks against the disc, with an
  independent decoder: `bkm_body` (CMPR 512x256, decoded with a scratch CMPR decoder) is itself
  cyan/blue/violet with a green-cyan-blue-magenta band. The `bkm.bmd` TEV stages sample it with
  the identity swap table (only the toon stage reads `toonEX` through the RRRA table). `bkm.brk`
  animates only `hana_sitahana` C2 and the `hikari_ex_hana_ura` konst, for the 250-frame death
  fade. So the blue bulb and the cyan/green/magenta petals are the model's own colours, not a
  host-order BRK or texture decode. Seen from above the floor (a temporary diagnostic camera,
  not committed), the flower is a pale blue lily with white-spotted petals and Makar at its centre.
  The new `native/check/input/kinboss-fight.txt` walks Link to the flower: the flower closes and
  rises, and from frame ~1500 the fight camera frames the bulb. The bulb is blue-violet with a
  lighter zigzag and yellow spots, the flower is green/cyan, and the barbed tentacles and the
  ceiling vines with their flower-tipped anchors are drawn. The geometry is clean and the
  envelope-skinned vines bend smoothly, so the
  suspected corrupt skinned vertices are not there. Shots inspected: `run --stage kinBOSS:0:0
  --frames 1200 --uncapped --shot 590,900` (unchanged, boss out of view behind Link) and `run
  --stage kinBOSS:0:0 --frames 2500 --uncapped --input native/check/input/kinboss-fight.txt
  --shot 590,1100,1800,2400`. See render issues (A2).
  Reviewed: the fight script rerun, shots 1100 (closing blue spotted flower, cutscene), 1800 and
  2400 (bulb, petals, thorned tentacles, clean skinned geometry) inspected; no code change,
  `regress.sh -j 3` all checks passed.
- RA-A3 (lane boot, render audit A3): Orca's text was missing only under load, not in the
  game. A single `run --stage Ojhous:0:0 --frames 1200 --uncapped --shot 590,900,1190` shows the
  text at the head, at the audit commit 772362e and at the audit-time head 49cfd9d. Running the same
  command 10 at a time gave 2 to 7 runs with the box, Next button and arrow but no text, and only
  one dim heart and no rupee counter (the audit's picture). CPU load alone (10 `yes` processes,
  runs one at a time) gave 0/4. A temporary `dMsg_Execute` trace (removed) showed identical
  message logic in good and bad runs: message 0x961, statuses 1, 2, 6, 7, and `output_text`
  `CC[ffffffff]GM[0]CR[0]Link! Is that you? ...` (137 bytes) complete at frame ~620. Save data
  (life 12/12, name "Link") was the same too. `aurora_get_stats` (temporary, removed) showed the
  cause: good runs had 0 pipelines queued and 350 created by frame ~350; bad runs still had
  220-300 queued at frame 1100, compiling about one per 25 frames. Aurora's GX `find_pipeline`
  uses `PipelinePriority::Normal`, which skips a draw until its pipeline is built. Parallel
  processes compete for the Metal compiler and the shared Dawn cache (`[aurora::gpu::cache]
  Failed to start transaction: database is locked`, a failed prune), so whether a draw shows
  depends on wall-clock compile time. Fix: Aurora patch 0005
  (`native/patches/aurora/0005-blocking-pipelines-option.patch`) adds
  `AuroraConfig::blockingPipelines` (default false, Aurora's behaviour unchanged), which resolves
  GX and clear pipelines through the existing `PipelinePriority::Blocking` path. The harness sets
  it from `COS_SYNC_PIPELINES`, which defaults to on when `COS_SHOT`/`COS_SHOT_EVERY` capture
  frames (`pc_harness.cpp`, `pc_main.cpp`; the aurora log line now prints `pipelines=sync|async`).
  So a shot shows every draw, as on the console (Dolphin's default `ShaderCompilationMode::
  Synchronous` does the same; skip-drawing is its opt-in mode). Plain runs keep async compiles.
  Checked (shots inspected): 6 parallel cold-cache runs (fresh user dirs) with
  `COS_SYNC_PIPELINES=0` drew neither Orca, Link, the HUD nor the box at 900/1190 (0/6 with text),
  while 6 with sync drew the full frame, text included (6/6; about 50 s wall each when cold, 9 s
  warm). 10 parallel runs with the shared cache and the default had text at 900 and 1190 (10/10),
  and so did the step's repro. Not fixed here (already known, see the amp-sweep entry):
  `--audio off` faults in `JAIZelBasic::sceneChange` from `pcBootStage` at frame 244. See render
  issues (A3).
  Reviewed: the step's repro (log shows `pipelines=sync`) and 6 parallel runs of it with separate
  run dirs, shots 900 and 1190 inspected (Orca's text, full HUD in all); `regress.sh -j 3`
  all checks passed.
- RA-A4 (lane boot, render audit A4): not a defect, and not Link's shadow. The white lobed shapes
  behind Link in K_Testc are three `Obj_Ygush00` spring actors (`Ygush00.arc`, `ygush00.bdl`,
  placed by the test map's Room0 at (-800,0,-700), (-800,0,-600) and (-800,0,-400), with Link
  starting at (-800,0,-900)). Their only material, `suimen`, is an opaque toon water surface (no
  blend, alpha compare always). Stages 0-2 add the two scrolling I8 textures and multiply by the
  third. Stage 3 is a `GX_TEV_COMP_R8_GT` threshold (prev.r > A2 = 128 selects C2 = 4.0). Stage 4
  computes `mix(K0, C0, prev)`, so the water is K0 and the foam rings are C0. `_draw` fills C0 and
  K0 from `settingTevStruct(TEV_TYPE_BG1)` and `setLightTevColorType`, that is, from the stage
  palette's BG1 colours. In K_Testc's `stage.dzs` `Pale` chunk (read from the disc with a scratch
  parser outside the repo), BG1 C0 and K0 are (255,255,255) in all four palettes. So both lerp ends
  are white, and the console and Dolphin draw a flat white silhouette too. Compared with VrTest,
  which has BG1 K0 (57,54,70)/(117,130,147)/(9,99,224), and Omori, the only other room with
  Ygush00, where BG1 is C0 (140,164,192) and K0 (15,56,53). How it was found (all temporary code
  removed): first a trace of the `dDlst_shadowReal_c` receiver triangles, which were only the floor
  at y=2. Turning fog off in the shadow pass did not change the blob. Skipping
  `dDlst_shadowControl_c::draw` removed the dark shadow at Link's feet, which is Link's real-time
  shadow and is correct, and kept the blob. Skipping the draw lists one at a time removed the blob
  only with `drawOpaList`. An actor census near the player found the three `Obj_Ygush00`. A dump
  of Aurora's WGSL for `suimen` matched the decoded MDL3 TEV stages exactly, and the runtime C0/K0
  were 255,255,255,255. With Omori's BG1 colours forced onto the material, the K_Testc springs
  drew dark teal water with light-blue foam rings that scroll between frames 590 and 900. So the
  geometry, the I8 textures, the BTK texture matrices and the colour compare all work. Nothing was
  changed in the code. Shots inspected: the repro `run --stage K_Testc:0:0 --frames 1200
  --uncapped --shot 590,900` before and after (identical) and `run --stage Omori:0:0 --frames 700
  --uncapped --shot 600`. See render issues (A4).
  Reviewed: the repro shots 590 and 900 inspected (white lobes behind Link, dark shadow at his
  feet); an independent parse of the disc's `Pale` chunks confirmed BG1 C0/K0 = (255,255,255) in
  all four K_Testc palettes and (140,164,192)/(15,56,53) in Omori; `regress.sh -j 3` all
  checks passed.
- RA-A5 (lane boot, render audit A5): not a defect, and not a billboard. The X consists of two
  authored light beams. They are the two `LTag1` actors (`daTagLight`, type 1, params `00fffffd`:
  no switch, no schedule, cone ratio 15/15 so `mspot.bdl`, not `mspocn`), placed at
  (3075,975,975) and (3062.9,975,-1097.3) with angles (-16384,8192,0) and (16384,-8192,0) and
  scale y 15 (30 after `_create`'s doubling). `mspot.bdl` is a 16-sided open cylinder of radius
  51 along local +Y (y 0..100, 96 positions). The beams leave the side walls near the entrance
  (local y=100, which `get_base_model_light_start_offset` uses as the light's start), run
  horizontally at 45 degrees, cross above the Ecube pillar and end near the far wall. The base
  matrix the actor builds, (0.707,-0.707,0,3075 / 0,0,1,975 / -0.707,-0.707,0,975), maps +Y to
  (-0.707,0,-0.707), and that is the crossing seen in the shot. No joint is a billboard: all three
  joints have mtx type 0, `checkBBoardFlag()` is 0, and every SHP1 matrix type is 0. So
  `J3DCalcBBoardMtx`/`J3DCalcYBBoardMtx` are never involved. Materials (J3D data and Aurora
  pipeline state match): `Kogen1_v_x` uses a 16x16 I4 texture that is solid 0xFF, an identity
  texgen, TEXC*RASC and TEXA*RASA*K3A, blend SRCALPHA/INVSRCALPHA, z compare on, z update off,
  no culling, and PERSP_LIN fog. Its RGBA8 vertex colours (big-endian, decoded right) are white
  with alpha 0x73/0x40/0x33 or 0x59/0x40/0x00 along the beam. So it is a translucent white
  cylinder (at most 45%) that fades along its length, and its edges are naturally hard. The
  console draws the same: Dolphin's blend and TEV give the same result, and the measured pixels
  (about 125 over a background of 25,33,45) match one 45% white layer.
  `Hokori_v_x` adds the dust: two scrolling 128x128 I4 layers, additive (SRCALPHA/ONE), 3 BTK
  texture matrices. Hiding `daTagLight::_draw` removes the X, and nothing else changes. The
  "opaque white quads" in the audit were these translucent beams. Not checked against a console
  capture (no Dolphin or console image of this grotto was available), but every input was
  checked: actor params and angles, geometry, joints, shape matrix types, vertex colours and
  texcoords, texture, TEV, blend, z and fog. How it was found (all temporary code removed: an
  actor census in `dStage_actorCreate`, a joint, material and display-list dump in
  `daTagLight::_draw`, and a GX state dump in the build dir's Aurora copy keyed on the model's
  position array): the census found `LTag1`, hiding its draw confirmed it, and the dumps covered
  the rest. Nothing was changed in the code. Shots inspected: the repro `run --stage SubD43:0:0
  --frames 1200 --uncapped --shot 900` before and after (the same scene, with only the dust and
  sparkle animation differing), and the same run with the tag-light draw hidden (no X). See render
  issues (A5).
  Review: `mDoMtx_ZXYrotM` (M*Ry*Rx*Rz) with x=-16384, y=8192 maps local +Y to (-0.707,0,-0.707),
  so the horizontal crossing comes from the authored angles; the repro shot shows translucent
  beams; `regress.sh -j 3` all checks passed.

- **Fix NG-run-dir (M11 boot loop, lane audio, harness): parallel runs of one target shared a run
  directory.** `run.sh` tested a directory name for existence and then created it with
  `mkdir -p`, so runs of one target started in the same second (`regress.sh -j`, 4 parallel
  `new-game` runs) took the same name and so one memory card folder (`<run dir>/card/`); their
  logs show `[aurora::card] Failed to create file: the save file`. It now claims the name with a plain
  `mkdir` (atomic) and tries the next suffix on failure. This removes one failure mode under
  parallel load only: the `MemCardMakeGameFile` 1-frame failure still happened with a private card
  (1 of 3 sequential runs, uncapped, reviewer run `new-game-20261003-191900`: no card error in the
  log, `01-GZLE-<save file>.gci` written, but `SaveSync()` returned 2); see fix NG-memcard-sync.

- **Fix NG-memcard-sync (M11 boot loop, lane audio, host-semantics): the game read a posted but not
  yet taken memory card command as a failed save.** `mDoMemCd_Ctrl_c::save()` sets
  `mCommand = CARD_STORE` under `mMutex` and signals `mCond`; the memory card thread takes the
  command by locking `mMutex` and holds it until the command is done. The readers (`SaveSync`,
  `LoadSync`, `FormatSync`, `LoadSync2`, `getStatus`) report busy (0 / 14) when
  `OSTryLockMutex` fails. On the GameCube the card thread (priority + 1) runs as soon as the game
  thread waits for the retrace, so the next frame's reader sees the lock held or the command done.
  Host threads are preemptive and, under load or uncapped, the card thread may not have taken the
  command yet: the next frame's `SaveSync()` won the lock, read `field_0x1660 == 2` (no file yet)
  and returned 2, the name scene showed message 0x18 and waited in `MemCardStatCheck` forever while
  the card thread then wrote the file. Under `TARGET_PC` (m_Do_MemCard.cpp) the readers take the
  lock only when no command is pending (`mDoMemCd_tryLockIdle`), so a posted command counts as
  the card thread's, as on the console; the command posters and the card thread are unchanged.
  Verified: 9 sequential uncapped runs, 4-way parallel capped and uncapped, and two rounds of 8
  parallel uncapped runs all pass; `MemCardMakeGameFile` lasts 3-22 frames in all 33 runs (no
  1-frame case). `new-game 0 --input native/check/input/new-game.txt` is now in
  `regress_targets.txt`. Open, not this cause: `regress.sh -j 3` failed 2 of 6 runs on an
  audio thread crash, SIGSEGV pc=0 in `JASystem::Kernel::portCmdMain` (from `TAudioThread` ->
  `updateDac` -> `mixDSP` -> `finishDSPFrame` -> `subframeCallback`), once in `new-game` (frame
  431, title demo room) and once in `outset-control` (frame 305): a port command with a null
  function, likely the game thread's port command list racing the audio thread.
  Review: 3 `regress.sh -j 3` runs passed (new-game in the list); `new-game` reached 2 times
  uncapped (7 s), once capped (47 s) and 4 of 4 in parallel uncapped, each run in its own run dir.
- **M11 new-game reached** (2026-10-03, lane audio, boot loop iteration 3, no code change): with
  fixes NG-run-dir (22badc6) and NG-memcard-sync (e6514c7) integrated, `new-game` with
  `--input native/check/input/new-game.txt` passes 3/3 (fixer, capped, ~46 s each). Without
  `--input` no START reaches the title, the game stays in OPEN2_SCENE and times out: a run-command
  mistake, not a game bug (the regress line has `--input`).
  Review: `regress.sh -j 3` passed; `new-game` reached 3/3 (2 uncapped, 11-12 s; 1 capped, 47 s).
- **M14 outset-real reached** (2026-10-03, lane audio, boot loop iteration 1, no code change):
  `outset-real --input native/check/input/new-game.txt --timeout 480`, capped, passes 3/3 (~410 s
  each, two of them in parallel): OPEN scene gone at frame ~8020, PLAY scene in sea room 44 at
  ~8060, the intro event and its STB demo run 2973 demo frames (to ~11040), Link is free, the
  script's fourth stick hold moves him 356 units, milestone at frame 12420. The NG-probe's "PLAY
  scene never created" was a pacing artifact, not a stream leak: `dScnPly` `phase_00` waits until
  the prologue's streamed BGM (JA_STRM_DEMO_01_01, ~218 s real time) has ended and freed the stream
  buffer, which is GameCube behaviour; uncapped, the prologue's 6560 frames take ~25 s while the
  stream still plays in real time, so the PLAY scene comes up ~190 s later (seen at frame 42955,
  then the 240 s timeout ran out during the intro event). The script's provisional timing after
  frame 8200 works as is, capped. Not in `regress_targets.txt`: `regress.sh` runs every
  target uncapped, and uncapped the PLAY scene comes up near frame 40,000, long after the script's
  B taps, so the intro event waits on its first message until the timeout (seen: 480 s, demo
  mode 1 throughout). A capped line would also add ~410 s to the ~15 s suite. Run it by hand:
  `run.sh outset-real --input native/check/input/new-game.txt --timeout 480` (capped).
  Review: `regress.sh -j 3` passed; `outset-real` reached 3/3 capped (3 parallel runs, 410 s
  each, milestone frame 12420).
- **Step 6.7 performance instrumentation** (2026-10-03, lane audio). `COS_PERF=<file>`
  (`run.sh --perf perf.csv`): one CSV row per game frame from `pc_frame.cpp`, with the wall
  time, the busy time (wall minus the pace wait), the game thread's CPU time
  (`CLOCK_THREAD_CPUTIME_ID`, minus the CPU the limiter's final spin uses), the pace wait and the
  split `begin` (events + `aurora_begin_frame`), `mDoCPd_Read`, `mDoAud_Execute`, the
  `fapGm_Execute` logic, the `mDoGph_Painter` GX encode, `aurora_end_frame` and the rest. The
  split comes from `pc_perf_begin`/`pc_perf_end` brackets under `TARGET_PC` around the three
  calls of main01's loop (m_Do_main.cpp) and around `cAPIGph_Painter` (c_API_graphic.cpp; the
  painter runs first inside `fapGm_Execute`, so logic = `fapGm_Execute` - painter); pace waits
  inside a bracket are left out of it. Rows are buffered (256 KiB) and written at exit by
  `pc_exit`. The Switch's `COS_PERF_EVERY` line (a88916e) now averages the same per-frame numbers
  and appends the split and the CPU time, keeping its existing prefix, so Mac CSVs and Switch logs
  compare column for column. The game thread is set to QoS USER_INTERACTIVE
  (`pthread_set_qos_class_self_np`, logged; threads it creates inherit it). `-DCOS_PERF_BUILD=ON`
  (own build dir, e.g. `build/native-mac-perf`): Release at `-O2`, `-march=armv8-a` on every
  target (Aurora and its dependencies included), Tracy forced off, and the executable logs the
  variant. Verified: `run --frames 900 --perf perf.csv` (capped, `COS_PERF_EVERY=120`) exits 0
  with 901 lines (header + frames 1-900, contiguous); in every row wall = busy + wait, the split
  sums to busy and cpu <= busy; at the title (frames 600-900) busy is ~1.4 ms median (logic ~0.9,
  painter ~0.4), wait ~32 ms (30 fps title); frame 1 carries the boot (~280 ms in
  `aurora_begin_frame`). Uncapped 400 frames: wait 0, rows complete. The perf variant configures
  (all 1580 compile commands carry `-O2 -march=armv8-a`, no `-O3`) and builds; benchmarks are step
  6.8.
  Review: `regress.sh -j 3` passed; `run --frames 900 --perf perf.csv` (COS_PERF_EVERY=120)
  exit 0, 901 CSV lines, wall = busy + wait and the split sums to busy in every row, cpu <= busy.

- **Step 6.9a actor sweep** (2026-10-03, lane audio). `COS_SMOKE=actor-sweep`
  (`native/src/pc/pc_actor_sweep.cpp`, `run.sh actor-sweep --stage sea:44:206`): once the M12
  probe has Link in Outset's start room (plus 60 frames), every actor profile of
  `g_fpcPfLst_ProfileList` (leaf sub-method `g_fopAc_Method`: 453 of the 502 names; scenes, overlaps,
  kankyo, message and camera processes are skipped) is created in turn with
  `fopAcM_create(name, parameter 0, 150 units in front of Link, Link's room)` from Link's layer,
  run 30 game frames (found each frame among the creating and the executing processes) and deleted
  with `fpcM_Delete` (a process still creating is aborted that way), then 3 frames pass before the
  next one so a fault Aurora's worker reports a frame late stays attributed. After each profile the
  PLAY scene, Link and the stage must still be there (else exit 1). `<run dir>/actor_sweep.txt` gets
  a `begin` line before each creation (written straight to the file) and a result line after
  (`ran`, `refused`, `deleted-itself`, `creating`, `not-deleted`). `COS_ACTOR_SWEEP=<first>[-<last>]`
  limits the process names. `native/tools/actor_sweep.py` runs the smoke again after each fault
  from the next profile and writes `<sweep dir>/actor_sweep.txt` (profile -> signature, with
  `boot_sweep.py`'s signature format); a fault between two profiles is put on the last one
  deleted, marked `(after-delete)`. A whole sweep takes ~3 min (23 runs, uncapped).
  Result (two sweeps, same build, 453 profiles each): 368 ran 30 frames and were deleted, 61 were
  refused, 2 deleted themselves, 2 were deleted while still creating, 20 faulted while in flight and
  2 more faults came right after a deletion (22 faulting runs per sweep, table).
  Classes: **ctx** = the actor needs a parent, the ship or other actors the sweep does not make (it
  reads them through a NULL `fopAcM_SearchByID` or `l_p_ship`, or asserts on them): game behaviour
  when spawned alone, not a port bug; **prm** = parameter 0 selects a type the game never places
  (NULL resource name, assert on the parameters): game behaviour; **del** = a child actor or a
  neighbour keeps a reference to the swept actor after its deletion (a child running without its
  parent, a seagull's floor polygon on a deleted moving BG: `GetTriPla` returns NULL): game
  behaviour under a deletion the game never makes; **dup** = a second instance of a singleton;
  **?** = not understood yet. No signature is shared by many profiles through one port root cause,
  so nothing is fixed in this step and `actor-sweep` is not in `regress_targets.txt` (it does not
  pass). The largest group (ctx) is the sweep spawning child-only actors alone; changing it would
  mean per-profile parameters or parents, i.e. a different test.

  | profile (fpcNm) | dStage name | signature | class |
  |---|---|---|---|
  | SEA (40) | sea | STALL in `daSea_packet_c::draw` (d_a_sea.cpp:666/1212) <- `J3DDrawBuffer::drawHead` | dup? |
  | Obj_Pirateship (59) | Pirates | (after-delete) SIGSEGV 0x15c `fopAc_IsActor` <- `daKnob00_c::draw` (its door knob child) | del |
  | Obj_Tousekiki (60) | Touseki | SIGSEGV 0x3c0 `daObj_Tousekiki_c::_execute` (d_a_obj_tousekiki.cpp:113) | ctx |
  | Kaji (62) | Kaji | SIGSEGV 0x3c0 `daKajiExecute` (d_a_kaji.cpp:114) | ctx |
  | MFLFT (93) | Mflft | SIGSEGV 0x20 `cCcD_Stts::Init` <- `daMflft_Create` (d_a_mflft.cpp:641) | ? |
  | KITA (99) / KOKIIE (100), one per sweep | Kita / Kokiie | SIGSEGV 0xc `dDlst_shadowSimple_c::set` <- `daKamome_Draw`: `dComIfGd_setSimpleShadow2` gets a NULL plane for a floor polygon of the deleted platform | del |
  | SHUTTER (114), first sweep only | Htobi1 | SIGSEGV 0x20010 `cNdIt_Judge` <- `fopAcIt_Judge` <- `dEvt_control_c::moveApproval` | ? (flaky) |
  | LEAF_LIFT (122), second sweep only | Olift | SIGSEGV 0x0 `cPhs_Handler` <- `daLlift_c::_create` (d_a_leaflift.cpp:162) | ? (flaky) |
  | Obj_Vds (134) | Vds | SIGSEGV 0x14f4 `daObjVds::Act_c::process_off_main` (its Swlight actors) | ctx |
  | PLAYER (169) | Link | SIGABRT in Aurora `gfx::push` <- `gx::fifo::draw_prim` (FIFO worker), second Link | dup / Aurora |
  | GRID (172) | Grid | SIGSEGV 0x4b4 `ho_move` <- `daGrid_c::_create` (the ship) | ctx |
  | SAIL (173) | Psail | SIGSEGV 0x3be `daSail_Create` (d_a_sail.cpp:650, parent ship) | ctx |
  | PIRATE_FLAG (174) | - | SIGSEGV 0x3c0 `daPirate_Flag_Execute` (d_a_pirate_flag.cpp:470, `l_p_ship`) | ctx |
  | WBIRD (197) | WBird | PANIC d_operate_wind.cpp:1001 in `dOperate_wind_c::_create` | ctx |
  | BPW (212) | big_pow | (after-delete) SIGSEGV 0x4fe `action_big_demo` <- `daPW_Execute` (its PW children) | del |
  | BST (242) | Bst | SIGSEGV 0x1728 `daBst_Execute` (d_a_bst.cpp:2644, boss parts) | ctx |
  | SBOX (295) | tkrSlv | PANIC d_a_sbox.cpp:220 in `daSbox_c::shipMtx` | ctx |
  | BOMB (296) | Bomb | PANIC d_a_bomb_static.cpp:206 in `daBomb_c::prm_get_state` | prm |
  | DOOR10 (302) | door10 | PANIC d_a_door10.cpp:356 in `daDoor10_c::CreateHeap` | prm |
  | KDDOOR (306) | doorKD | PANIC d_a_kddoor.cpp:868 in `daKddoor_c::CreateHeap` | prm |
  | NPC_OS (316) | Os | PANIC d_a_player_npc.cpp:157 in `daPy_npc_c::setPointRestart` | ctx |
  | LODBG (451) | LOD01 | PANIC d_a_lod_bg.cpp:436 in `daLodbg_c::createHeap` | prm |
  | Stone2 (461) | Ebrock | SIGSEGV 0x0 `strcmp` in `dRes_control_c::setRes` <- `phase_1`: type 0's `resName` is NULL in `M_attr` | prm |

  Follow-ups (not fixed here): (1) SHUTTER / LEAF_LIFT: faults that move between sweeps (a
  corrupted actor list, a phase handler reached through 0) point at memory damaged by an earlier
  profile of the same run; bisect with `COS_ACTOR_SWEEP=100-122` and run under the ASan build
  (step 6.9 part 3). (2) PLAYER: Aurora aborts in `gfx::push` on a second Link's draw; check the
  abort's size against Aurora's buffer limits (an Aurora robustness item even if two Links never
  happen in the game). (3) SEA: a second sea stalls `daSea_packet_c::draw`; confirm it is the
  singleton's static state. (4) MFLFT: `mStts` at 0x20 from create; read d_a_mflft.cpp's create
  path with parameter 0. (5) A sweep variant that gives the ctx/prm profiles the parameters and
  parents the stage data uses (from the disc's ACTR records) would cover them for real.
  Review: `regress.sh -j 3` passed; `COS_ACTOR_SWEEP=175-196 run.sh actor-sweep --stage
  sea:44:206 --uncapped` exit 0 (19 ran, 2 refused, 1 deleted itself); a third full sweep gave 21
  faulting runs (the table minus KITA/KOKIIE and LEAF_LIFT, plus SHUTTER), so the del/? rows are flaky.

- **Widescreen option `COS_ASPECT`** (2026-10-03, lane wide). The community 16:9 Gecko code that the
  translated build applies (mods/widescreen/GZLE01.gecko) is done in C under `TARGET_PC`:
  `COS_ASPECT=4:3|16:9|16:10` (`native/include/pc/pc_aspect.h`, `native/src/pc/pc_aspect.cpp`; 4:3 by
  default on the Mac, 16:9 by default on the Switch through `cos_switch.cpp`'s defaults; a malformed
  value exits 2). Every line of the code was decoded against the decompilation's symbols and
  `main.dol` (table in [MODS.md](MODS.md), "Widescreen in the native port"): the camera aspect (which
  also widens `mDoLib_clipper`'s culling frustum), the 2D screen -9..650 widened to -123..767, the
  wipe and fade bounds, the HUD and minimap moved 114 toward the edges (constructor values of
  `g_meterHIO`/`g_meter_mapHIO`, set in `dMeter_Create`, plus the redirected `fopMsgM_paneTrans`/
  `setAlpha`/`setInitAlpha` calls of the meter), the menus' 640-wide mask panes left unfaded, full-
  screen J2D screens drawn with their own 640x480 port, the Picto Box and telescope given black bars
  and a 4:3 photo viewport, the boomerang sights mapped onto the wider 2D screen. 16:10 interpolates
  each value as `scripts/mods/widescreen_aspect.py` does. Aurora patch 0006 (`AuroraSetFitAspect`)
  makes `AURORA_VIEWPORT_FIT` keep the chosen aspect instead of the render mode's, so the 640x480
  EFB is presented stretched to 16:9 (an anamorphic picture, as a widescreen TV shows the console's
  output) and letterboxed in a window of another shape; the Mac window opens at 1280x720 (16:9) or
  1152x720 (16:10). Checked with COS_SHOT (inspected, 4:3 and 16:9): title, name scene (dialog
  centred, not stretched, the sky across the whole width), Outset after M13 (the view shows more on
  both sides, hearts and magic at the left edge, buttons at the right, minimap and its buttons at
  the bottom left with the map clipped to its frame, rupees at the bottom right, no horizontal
  stretch: Link's height is the same share of the frame), the item menu (centred, the captured
  background across the width), and 16:10 Outset. `regress.sh -j 3`: all checks passed, with three
  new targets (`outset-control --aspect 16:9`, `file-select --aspect 16:10`, `static-init 2 --aspect
  21:9`; `run.sh --aspect` sets `COS_ASPECT`). Not checked on screen: culling at the edges in
  motion, the Picto Box, telescope, sea chart, game over and conducting screens, the place name.
  Seen in 4:3 too, so not from this change: a dark translucent box over part of Outset in Aryll's
  lookout event, a one-pixel blue line at the bottom of fully black frames, and a panic in
  `dMenu_save_c::_create` (d_menu_save.cpp:184) when R opens the collection screen from the item menu.

### Phase 6 render issues

- **Aurora WGSL for an alpha compare on a texture's alpha** (found by step 6.4, sea room 44,
  ROOM_SCENE frame 1230): `build_shader` emits `round(sampled0.a.r * 255.0) >
  round(tev_overflow_f32(tevreg1.a).r * 255.0)`, Dawn rejects it ("cannot index into expression of
  type 'f32'"): an alpha TEV stage with a compare op (`GX_TEV_COMP_R8_GT`; Aurora's
  lib/gx/shader.cpp:376 formats `{0}.r` on the alpha stage's scalar operands) and Aurora aborts (SIGABRT on its pipeline worker thread). The bad pipeline is then
  in `build/native-mac/user/cache/pipeline_cache.db`, which Aurora recompiles at start-up, so every
  later run, even `logo-res`, aborts right after gfx-create until that file is deleted. The fix is
  in Aurora (stop condition: needs a decision); it blocks M12. **Fixed** (decision H11) by
  `native/patches/aurora/0001-alpha-stage-channel-compares.patch`: an alpha stage's R8/GR16/BGR24
  compares now read the stage's colour inputs A and B, latched before the colour half writes its
  register, as Dolphin's `tevin_a`/`tevin_b` do. The Aurora patch mechanism (H11):
  `native/cmake/Aurora.cmake` applies `native/patches/aurora/*.patch` in name order through
  `native/cmake/aurora_apply_patches.cmake` (idempotent), as the FetchContent `PATCH_COMMAND`, and,
  with `FETCHCONTENT_SOURCE_DIR_AURORA`, to a per-build-dir copy in `_deps/aurora-patched-src`
  (redone when the checkout's commit, its `git status` or the patch set change), so the shared
  checkout is never modified. `outset-debug --stage sea:44:206` now runs ROOM_SCENE past frame 5500
  without a fault (timeout: nothing reports M12 yet). Still open, not fatal: about 470k
  `CP_REG_ARRAYBASE_ID is not supported` log lines per 180 s run in sea room 44 (**fixed** by
  R4-arraybase, entry below).
- **Uncapped stall in `JFWDisplay::calcCombinationRatio`** (found by step 4.16, Outset with A
  taps, uncapped only, frame 5856): the main thread spins at `JFWDisplay.cpp:320`. The loop
  `for (i = vidInterval; i < field_0x34 * 2; i += vidInterval)` never ends when
  `JUTVideo::sVideoInterval` (OSGetTick delta between two pre-retrace callbacks) is 0, which the
  host VI (`VIRetrace.cpp`: every `VIWaitForRetrace` call is a retrace, a polling thread makes them
  back to back) can produce. Not a render issue strictly, but host timing; blocks uncapped M13.
  **Fixed** by F1-vi-stall: `VIWaitForRetrace` spaces retraces by at least 1 us of `OSGetTick`.
- **Outset (sea room 44) draws badly** (found by M12, `--shot`; after step 4.16 the event camera
  shows the lookout and island geometry, so the missing island was event/camera state; character
  models (Link, Aryll) draw as black silhouettes with noisy faces): at frame 400 the backdrop band
  behind the clouds is a grid of garbage-coloured blocks (a texture decoded or sampled wrongly) and
  the island geometry is missing (only a distant grey silhouette and the sea); by frame 1450 the
  view shows only the sea under the letterbox. Not yet triaged (render vs. camera/event state); M12
  does not depend on it, M13 may. The backdrop band is **fixed** by the J3D texture fix below (step
  R1-lighting): it was the horizon cloud model drawn with stale textures, and now shows the cloud
  band. The missing island and the sea-only view after frame 1000 remain open.
- **Characters and J3D models draw black or posterised grey** (step R1-lighting, lane boot): Link and
  Aryll solid black in Outset, Link a streaky grey on the title, "the game" subtitle of the
  title logo and the title's sky backdrop a grid of garbage blocks. Two independent causes, both
  **fixed**:
  1. (main) J3D textures were never bound. `loadTexNo` puts a material's textures in its display
     list as BP writes (image pointer `OSCachedToPhysical(ptr) >> 5`, attributes, TLUT load), and
     Aurora takes a texture's image and TLUT only from `GXLoadTexObj`/`GXLoadTlut`
     (`GX_AURORA_LOAD_TEXOBJ`): its BP handler keeps the image-pointer register but never resolves
     it, so every J3D material sampled whatever texture object was last loaded in that texture map,
     read with the material's size and format. Fix in `game` under `TARGET_PC`, the Dusklight
     pattern (CC0): `J3DTexture` keeps a `GXTexObj`/`GXTlutObj` per entry, built from its ResTIMG in
     the constructor and again in `setResTIMG` (toon images, frame-buffer copies, swapped textures);
     `J3DTevBlock::loadTexture` (blocks 1/2/4/16/Patched) loads `mTexNo[i]` into texture map i, and
     `J3DMaterial`/`J3DPatchedMaterial`/`J3DLockedMaterial::load` and `loadSharedDL` call it just
     before the material display list. Known limit (as in Dusklight): a texture number that a
     model instance's diff display list changes (`diffTexNo`, texture-pattern animation of models
     sharing one J3DModelData) shows the material's current `mTexNo` at draw time, not the value
     the instance had when its diff list was built. Likewise `J3DMatPacket::draw` binds the
     J3DTexture the packet got at model creation, so a material table swapped in only while the
     display list is rebuilt (`mDoExt_McaMorf::updateDL(J3DMaterialTable*)`, `setMaterialTable`
     with `J3DMatCopyFlag_Texture`) has its texture numbers resolved in the model's own table, not
     the swapped one as on the GameCube. Not seen yet; open if such a model draws wrong textures.
  2. Aurora fed `GX_TG_SRTG` texgens the raw vertex colour (absent in these models, so (0, 0))
     instead of the lit colour channel, as the hardware does (Dolphin VertexShaderGen:
     `vertex_lighting_0.xy`), so the toon ramp was always sampled at its shadow end. Fixed by
     `native/patches/aurora/0002-srtg-texgen-lit-colour.patch` (decision H11).
  Checked with COS_SHOT: `run --frames 1310 --shot 900,1300` (title) shows the logo with its
  subtitle, the sky and clouds, the King of Red Lions and Link in his blue shirt and orange
  trousers with two-tone toon shading; without patch 0002 Link is uniformly in shadow colours.
  `outset-debug --stage sea:44:206 --shot 400` shows the horizon cloud band instead of the garbage
  blocks; Outset's island and characters are not in view in that run (open entry above).
- **Name-scene backdrop garbled, title subtitle garbled** (step R2-textures, lane boot;
  `file-select --shot 1200`): the file-select sky showed multicoloured blocks and the subtitle
  under the title logo was garbled. Two causes:
  1. The blocks and the subtitle were J3D materials sampling stale textures; **fixed** by
     R1-lighting's J3D texture binding (3c3f2ae): the sky (vrbox) and its clouds and the
     subtitle draw correctly before R2.
  2. (R2's fix) Long straight white lines crossed the name scene's night sky. `dKyr_drawStar`
     draws every star as two `GXBegin(GX_QUADS, GX_VTXFMT0, 3)` triangles (`dKyr_drawLenzflare`
     draws its rays the same way); the hardware rasterizes three vertices left over in a quad
     primitive as a triangle (Dolphin `IndexGenerator::AddQuads`: "... do this for sun rays", about this game),
     but Aurora's `prepare_idx_buffer` always emitted a fourth index, one vertex past the
     primitive (the next star merged into the same draw, or stale vertex data), so each star
     became a sliver across the screen. **Fixed** by
     `native/patches/aurora/0003-quads-three-vertex-triangle.patch` (decision H11): only whole
     quads, plus one triangle for a remainder of three.
  Checked with COS_SHOT: `run --frames 1210 --input native/check/input/file-select.txt --shot
  900,1200` shows the starry night sky with clouds behind the memory-card prompt, as on the
  GameCube; before, the same frames had the white lines. `run --frames 610 --shot 600` (title,
  logo with subtitle) unchanged.
- **Gameplay HUD drawn on the title, black jagged geometry left of the logo, green striped pole**
  (step R3-title-hud, lane boot; title `run --frames 3000 --shot 1500` of a build that logged
  "デモデータ読み込みエラー！！"): not a render bug. Two causes, both **fixed** by earlier work:
  1. (main) The title demo's STB did not parse (big-endian STB/FVB containers, JStudio list node
     offsets, adaptor operands), so the title stage ran without its demo event: Link stood in
     play and `dMeter_statusCheck` (d_meter.cpp) set none of the event flags that hide the meter
     (`dComIfGp_event_runCheck()` false). Fixed by the M8 boot loop iterations 1-5 (STB parse
     through 8eb9db8); the demo now plays and the HUD is hidden, as on the GameCube.
  2. The black jagged shape (the King of Red Lions' head) and the green striped pole were J3D
     materials sampling stale textures; fixed by R1-lighting's J3D texture binding (3c3f2ae).
  Checked with COS_SHOT: `run --frames 1310 --shot 600,900,1300` and `run --frames 3010
  --uncapped --shot 1500,2000,2500,3000` show the title without HUD, the boat textured (its dark
  colour is the open entry below) and no striped pole.
- **King of Red Lions dark on the title** (found while reviewing R1-lighting, title frames
  900/1300): after the J3D texture fix the boat's head and hull draw dark olive/brown with little
  of the red of the GameCube title. Not triaged (lighting/colour registers of its materials vs.
  texture). **Fixed** by R7-korl-dark: an Aurora divergence from the SDK, not the port's data.
  `title_ship.bdl`'s three materials (one TEV stage, texture x lit colour 0) light colour channel 0
  with GX_LIGHT0 (`GX_AF_SPOT`, `GX_DF_CLAMP`, ambient 0x32, material colour white from the BPK);
  for the 2D list `mDoGph_Painter` loads that light with `setLight()` (m_Do_graphic.cpp), which
  calls `GXInitLightDistAttn(&light, 0.0f, 0.0f, GX_DA_GENTLE)`. The SDK (decomp GXLight.c) turns
  the attenuation off for a reference brightness <= 0; Aurora 3227d76 tested < 0, kept
  GX_DA_GENTLE and set k1 = 1 / (0 * 0) = inf, so the light added nothing and the ship showed
  only 0x32/255 of its red C8 textures. H11 patch
  `native/patches/aurora/0004-light-dist-attn-zero-brightness.patch` uses the SDK's `<= 0`.
  Any other light the game builds with a reference brightness of 0 gets the SDK behaviour too.
  Checked with COS_SHOT `run --frames 1310 --shot 900,1300`: before, a
  dark brown silhouette; after, the King of Red Lions in red with its white and gold head and
  hull trim, lit from the left, as on the GameCube title; the rest of both frames unchanged.
- **Thousands of `CP_REG_ARRAYBASE_ID is not supported` lines once a stage loads** (step
  R4-arraybase, lane boot): **fixed**. The writer was `J3DShape::loadVtxArray`, called on every J3D
  shape draw (`drawFast`, `simpleDraw`, `simpleDrawCache`): its `J3DLoadArrayBasePtr` wrote CP
  0xA0+n (POS, NRM, CLR0) with a pointer truncated to 32 bits, three per shape, which Aurora
  rejects and ignores (341205 lines in 1500 frames of `run --stage sea:44:206`). For static models
  nothing showed, because the shape's VCD/VAT list (`makeVtxArrayCmd`, `GDSetArraySized`) had
  already bound the same arrays; but every array the vertex buffer swaps in at draw time (the CPU
  deformers' `mTransformedVtx*`/`mVtxPosArray[1]` output) never reached Aurora, and a shape drawn
  again without its VCD/VAT list (`sOldVcdVatCmd` unchanged) kept the previous draw's array. Under
  `TARGET_PC` `J3DLoadArrayBasePtr` writes `GX_AURORA_LOAD_ARRAYBASE` (64-bit pointer, byte size,
  byte order, after Dusklight's J3DShape.cpp) on every call, as the GameCube does: the model's own
  VTX1 array with `J3DVertexData::getVtxArraySize` (the size the VCD/VAT list gives, so Aurora
  keeps its cached upload), any other array with the vertex/normal/colour count times the stride
  of its type; all big-endian (`J3DVertexData::getColNum` added for the colour count). Check:
  `run --stage sea:44:206 --frames 1500 --shot 400,800,1200,1490` logs 0 such lines (before
  341205) and the shots equal the ones before (Outset lookout, Aryll, Link close-up), title
  `run --frames 610 --shot 600` unchanged and 0 lines. Two independent issues seen while checking
  are the next two entries.
- **CPU skinning computes nothing on the host** (found by R4-arraybase): the C++ bodies of the
  four `J3DPSMulMtxVec` overloads (J3DTransform.h) are `__MWERKS__` paired-single asm only, so on
  the host `J3DSkinDeform::deformVtxPos_*`/`deformVtxNrm_*` leave the transformed arrays
  unwritten, and they read the model's arrays host-order although those are big-endian. Before
  R4-arraybase the transformed arrays never reached Aurora (models drew in bind pose); now they
  do, so a CPU-skinned model (`dDoor_key2_c` boss-key lock, `dBgWDeform` users d_a_sk2 and
  d_a_npc_de1) draws from those unwritten arrays until this is fixed. The port should read the
  source big-endian and write the output big-endian (the byte order `loadVtxArray` declares).
  Part of the J3DSkinDeform item split off from 4.11. **Fixed** by R5-cpu-skinning: host C bodies
  for every J3DTransform paired-single routine (also the normal, envelope and projection matrix
  helpers, which were never written either), GQR7 scales kept for the S16 forms; the skin deform
  reads and writes the vertex arrays big-endian, and `dBgWDeform` hands its collision a host-order
  copy. The Great Deku Tree in Forest Haven (`Omori:0:3`), missing before, now draws. Not
  exercised yet: the S16 position/normal paths (the Deku Tree is F32, positions only) and the
  other CPU-skinned models (`dDoor_key2_c`, the ship's body, d_a_sk2); the title's King of Red
  Lions ran no skin deform in the title runs, so its dark colour (entry above) is not this issue.
- **Outset grass draws as spiky green squares with purple and blue** (found by R4-arraybase,
  `run --stage sea:44:206 --shot 800`, lower right): the grass tufts below the lookout show their
  whole quads (no alpha cut-out) with purple/blue texels, before and after R4-arraybase. On the
  GameCube they are alpha-tested blade tufts. Not triaged (`dGrass_packet_c::draw`'s texture or
  alpha-compare state vs. its vertex arrays). **Fixed** by R6-grass. The tufts at that spot are
  Outset's flowers (`dFlower_packet_c`: skipping its draw removes them, skipping grass, wood or
  tree does not), but grass, bushes and trees share the root cause: every static packet material
  display list from the DOL (`l_matDL*` of d_grass, d_flower x3, d_tree, d_wood, d_chain,
  d_a_hookshot, d_a_bwdg and `g_dTree_shadowMatDL`) names its texture with a BP SETIMAGE3 write of
  the image's physical address >> 5 (`IMAGE_ADDR`), which cannot hold a host pointer. Aurora binds
  a texture's image only through `GXLoadTexObj` (its BP handler stores SETIMAGE3 but never reads
  it), and the list's SETIMAGE0 then overwrote the slot's size and format, so these lists sampled
  whatever image was last loaded in GX_TEXMAP0, decoded as their own CMPR/I4 size: garbage texels,
  and garbage alpha for the GREATER-0 alpha test (whole quads). Same cause as the J3D materials
  fixed by 3c3f2ae, in the GX-direct packets. Under `TARGET_PC` the new `mDoLib_loadDLTexImage(dl,
  size, image)` (m_Do_lib.cpp) reads the list's own SETMODE0/1 and SETIMAGE0 registers for each
  map it names through SETIMAGE3, builds one cached `GXTexObj` per image and register set, and
  loads it right before every such `GXCallDisplayList` (as Dusklight's grass loads a
  `GXTexObj` before its material lists); the list then writes the same size, format and modes
  again. Check (COS_SHOT, inspected): `run --stage sea:44:206 --frames 820 --shot 800` draws white
  flowers with green stems and leaves cut out against the sand (before: green/purple/blue quads
  with blue stalks); `run --stage Omori:0:3 --frames 500 --shot 490` draws Forest Haven's grass
  blades (`l_Vmori_*` set) and bushes (dWood) green and leafy (before: dark blue grass and
  blue/beige bush balls). Not seen on screen yet: dTree trees and shadows, the Outset grass set
  (`l_Txa_ob_kusa_a`), chains, the hookshot chain and d_a_bwdg sand (same helper, same lists).

#### Render audit, 2026-10-03 (lane boot, after R1-R7)

Method: `COS_SHOT=300,590 native/tools/run.sh boot-sweep --frames 600` and
`COS_SHOT=900,1190 native/tools/run.sh boot-sweep --frames 1200` (every stage the sweep boots,
uncapped, 141 of 155 reach their last frame), plus title (`run --frames 1310 --shot 900,1300`), name
scene (`run --frames 1210 --input native/check/input/file-select.txt --shot 1200`) and Outset
(`outset-control --stage sea:44:206 --input native/check/input/outset-control.txt --uncapped --shot
600,1500,2500,4000`). Every shot was inspected (frame 300 is still the stage fade-in in most
stages). Title, name scene and Outset look right, and so do most stages: the sea, the sky, clouds,
characters, toon shading, grass, HUD and message boxes. These are not defects: black skies in
stages whose Stage.arc has no `vr_sky.bdl` (DmSpot0, Mukao, kazan, KATA_HB, the K_Test* maps,
I_TestR, Ebesso); greyscale Hyroom and Xboss0-3 (the game's monochrome effect); Link drawn red or
orange while burning (A_nami, K_Test8, Xboss0). Stages that fail to boot are left to the boot-sweep
loop. Open defects, most visible first (one probable root cause per row):

| # | Defect | Stages | Suspected cause | Repro |
|---|--------|--------|-----------------|-------|
| A1 | Arena floors are missing: below the walls the frame shows one flat colour, the same everywhere in a stage (RGB 48,48,45 in GanonE/GanonM/M_DaiMB, 59,59,59 in Xboss2, black in SirenB/kazeMB/kinBOSS). Link stands over nothing. | kinBOSS, GanonE, GanonM, M_DaiMB, Xboss2, SirenB, kazeMB; GanonB likely (dark brown gradient where its lava should be) | **Fixed** by RA-A1: not a render issue. The room-entry event camera found no `RelActor` ("@PLAYER" matched by a host-order u32 read of its name against `'@PLA'`), so its relative eye/centre offsets became world positions and the camera sat under the floor (eye (0,-40,-170) in GanonE), whose triangles are back-face culled from below. `dCamera_strTag` reads such tags big-endian. Checked with COS_SHOT (inspected, before/after): GanonE:0:0 and M_DaiMB:12:0 shot 1190, kinBOSS:0:0 shot 900 went from walls over the clear colour (kinBOSS: Kalle Demos seen from below) to Link seen from behind on the tiled/earth floor; GanonM:0:0, Xboss2:0:0, SirenB:0:0, GanonB:0:0 and kazeMB:6:0 (the sweep start; kazeMB:0:0 faults in `dStage_escapeRestart` with no player, before and after) shot 1190 all show the floor; title shot 1300 unchanged. | `native/tools/run.sh run --stage GanonE:0:0 --frames 1200 --uncapped --shot 1190` (also `M_DaiMB:12:0`, `kinBOSS:0:0 --shot 900`) |
| A2 | Kalle Demos (the `Bmd` actor) draws as large flat blue, magenta and cyan polygons with jagged leaves. (Seen with the camera under the arena floor, A1; after RA-A1 the frame-900 view is behind Link and the boss is out of view: recheck A2 from a shot that frames it.) On the GameCube it has a textured red bulb and green tentacles. | kinBOSS | **Not a defect; resolved by RA-A1** (checked in RA-A2): the flat blue/magenta/cyan polygons were the undersides of the boss's open flower (material `hana_sitahana`), seen from A1's under-floor camera. The flower's colours come from its own `bkm_body` CMPR texture (cyan/blue/violet, a green-to-magenta band), decoded independently from the disc; the TEV stages sample it unswizzled and `bkm.brk` animates only the death fade. With the camera fixed, the fight (Link walks to the flower) shows the blue-violet yellow-spotted bulb, the green/cyan petals, the barbed tentacles and the ceiling vines. Their geometry is intact and the skinned vines bend smoothly. The "red bulb" in the audit was a wrong expectation. | `native/tools/run.sh run --stage kinBOSS:0:0 --frames 2500 --uncapped --input native/check/input/kinboss-fight.txt --shot 1100,1800,2400` |
| A3 | Orca's message box shows its dark panel, the Next button and the arrow but no text, at frames 590, 900 and 1190. The Ojhous2 and Outset boxes show their text. | Ojhous | **Fixed** by RA-A3: not a game or decode issue, a harness timing artefact. Aurora skips a draw until its pipeline is compiled (`PipelinePriority::Normal`, "async skip draw"). In the parallel boot sweep, several game processes were compiling Metal pipelines at once and contending for the shared Dawn cache ("database is locked"), so about 300 pipelines were still queued at frame 1100 and the text's and most of the HUD's draws never showed. The game state matched good runs exactly: message 0x961, the same status sequence, and `output_text` "Link! Is that you? ..." complete by frame ~620. The BMG text, font and colours are fine. Aurora patch 0005 adds `AuroraConfig::blockingPipelines`, and `COS_SYNC_PIPELINES` (default on with `COS_SHOT`/`COS_SHOT_EVERY`) turns it on, so a captured frame shows every draw, as the console and Dolphin's default synchronous shader mode do. Checked (COS_SHOT, inspected): with cold caches and 6 parallel runs, async gave 0/6 with text at 900/1190 (no characters, HUD or box), sync 6/6; with the shared cache, 10/10 parallel runs show the text. | `native/tools/run.sh run --stage Ojhous:0:0 --frames 1200 --uncapped --shot 900,1190` (to reproduce the old behaviour: run 8 at once with `COS_SYNC_PIPELINES=0` and fresh user dirs) |
| A4 | Link's real-time shadow is drawn as a bright white lobed blob on the floor. Elsewhere (VrTest, A_umikz, Kaisen) it is dark. | K_Testc | **Not a defect** (checked in RA-A4). The blob is not a shadow: Link's real-time shadow is the dark shape at his feet and is correct. The white shapes are three `Obj_Ygush00` springs that the test map places in front of Link. Their toon water material `suimen` lerps the palette's BG1 K0 (water) to BG1 C0 (foam). K_Testc's `Pale` chunk on the disc sets both to (255,255,255) in every palette, so the console draws them white too. Hiding the shadow pass keeps the blob, and hiding `drawOpaList` removes it. With Omori's BG1 colours forced on (diagnostic only), the springs show dark teal water and scrolling light-blue foam rings, so the textures, BTK and TEV compare are right. | `native/tools/run.sh run --stage K_Testc:0:0 --frames 1200 --uncapped --shot 590,900` |
| A5 | Two crossed opaque white quads (an X) hang in the cave in every shot. They look like a light-shaft or billboard model that keeps its bind orientation. | SubD43 | **Not a defect** (checked in RA-A5). The X is the cave's two authored light beams: two `LTag1` (`daTagLight` type 1) actors, each drawing `mspot.bdl`, a 16-sided cylinder (radius 51, 3000 units long after scale). Their authored angles (x ±90°, y ±45°) make the beams run horizontally from the side walls and cross above the Ecube pillar. No joint or shape is a billboard (all matrix types 0, BBoard flag 0). The `Kogen1_v_x` material is a solid-white I4 texture times vertex alpha 0x00-0x73 with SRCALPHA/INVSRCALPHA blend, so it is a translucent white shaft (at most 45%) that fades along its length, and the `Hokori_v_x` dust adds on top. The J3D material data, the Aurora pipeline state and the disc's vertex data agree. The beams are translucent, not opaque. | `native/tools/run.sh run --stage SubD43:0:0 --frames 1200 --uncapped --shot 900` |
| A6 | Tingle Tower's walls and floor show a huge blocky, smeared Tingle mural (texels several screen pixels wide). The easel painting is sharp. | tincle | Either the authentic low-resolution mural texture or a wrong mip/LOD or texture size for that material. Needs a GameCube reference before work starts. | `native/tools/run.sh run --stage tincle:0:0 --frames 1200 --uncapped --shot 900` |
| A7 | The island is untextured flat sand and blue, and the house and pier are pure white silhouettes. | Ebesso | Possibly authentic unfinished test geometry (a pre-release Outset). If the GameCube shows textures, the house's materials sample nothing. Low priority, verify first. | `native/tools/run.sh run --stage Ebesso:0:0 --frames 1200 --uncapped --shot 590` |
