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

### Phase 6 render issues

None yet.
