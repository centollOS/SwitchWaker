# Phase 2 and Phase 3 implementation plan: the GameCube SDK over Aurora, then RELs linked statically


## Four findings that change the brief

**1. Dusklight does not build its `libs/dolphin`.** That directory is Dusklight's decomp's own SDK source, and only 5 files in it have `TARGET_PC` edits. Dusklight's real SDK has three parts:
- **Aurora's SDK libraries:** `aurora::core gx gd si vi pad mtx os dvd thp card` (`ref/dusklight/CMakeLists.txt:280`).
- **Aurora's headers as the only SDK headers:** `GameABIConfig.cmake` adds `extern/aurora/include` and `extern/aurora/include/dolphin`. The game has no `include/dolphin` of its own.
- **Dusklight's own glue in `src/dusk/`:**
  - `OSThread.cpp` (705 lines), `OSMutex.cpp` (215), `OSContext.cpp` (72) and `OSReport.cpp` (147).
  - `stubs.cpp` (1152): message queues, alarms, VI retrace emulation, GX metrics and sync, AI, EXI, PPC and HIO.
  - `extras.c`: `stricmp` and `strnicmp`.
  - The only `libs/dolphin` sources it compiles are `src/gf/GF*.cpp` (`files.cmake:1406`).

  So the model to copy is "Aurora plus `src/dusk` glue", not `libs/dolphin`.

**2. Where Aurora is.** Nothing tracked in this repository has the upstream Aurora; `ref/` is gitignored.
- `ref/dusklight/extern/aurora` is an empty submodule. Dusklight pins encounter/aurora at `3227d76` (2026-09-29).
- `ref/aurora` is an encounter/aurora clone checked out at `8b690b6`, 38 commits older. It does contain `3227d76` in its object store.
- `switch/aurora/aurora.cmake` and `runtime/host` build `ref/recompcore/GXRuntime` (the RecompCore fork's vendored Aurora) plus `patches/aurora/*` and `switch/aurora/patches/*`. That is the translated build's stack, not the upstream that Dusklight's SDK layer expects.

**3. What Aurora `3227d76` provides.**

| Library | What it covers |
|---|---|
| `aurora_os` | OSInit, cache, memory, arena, alloc, address, time, OSReport, **AR/ARQ** |
| `aurora_vi` | Configure and window functions only. No retrace, callbacks or next framebuffer; Dusklight stubs those. |
| `aurora_pad`, `aurora_si` | PAD, SI |
| `aurora_mtx` | MTX (C and PS variants) |
| `aurora_gx`, `aurora_gd` | GX, GD |
| `aurora_dvd` | DVD (backed by nod) |
| `aurora_card` | CARD (kabufuda) |
| `aurora_thp` | THP |
| `aurora_main` | `#define main aurora_main` |

Not provided anywhere:
- threads, mutexes, condition variables, messages, alarms, interrupts (Dusklight's glue);
- GF function bodies (the game also uses `GFLoadPosMtxImm`, `GFSetArray`, `GFBegin`/`GFEnd` and a `GFTransform.cpp` that Aurora's GF headers lack);
- DSP, AI, DTK, EXI, DB, LC;
- **GBA** (game-only: `JUTGba`, the Tingle Tuner);
- DVD streaming and `DVDLow*`.

Prebuilt Dawn and nod packages exist for darwin-arm64, so no Rust and no Dawn source build is needed.

**4. The game's own SDK headers cannot be used for linking.**
- They define hardware registers in headers: `vu16 __VIRegs[59] AT_ADDRESS(...)` in `vi/vi.h`, `volatile PPCWGPipe GXFIFO` in `gx/GX.h`, and `__DSPRegs`, `__PIRegs`, `__SIRegs`, `__EXIRegs`, `__DIRegs`. Because `AT_ADDRESS` is empty under clang, every C++ unit that includes them defines the symbol. That gives duplicate symbols at link time, and GX FIFO writes that go nowhere.
- Their layouts differ from Aurora's: the game's `GXTexObj` is 32 bytes with fields, Aurora's `TARGET_PC` one is `u32 dummy[16]` (64 bytes); `GXTlutObj` is `u32[10]`.
- 39 of the game's 82 `dolphin/` header paths collide (case-insensitively) with Aurora's, e.g. `dolphin/types.h`, `dolphin/gx/GXStruct.h`, `dolphin/os/OSThread.h`, `dolphin/card.h`, `dolphin/dsp.h`. The other 43 are game-only names such as `dolphin/os/OS.h`, `dolphin/gx/GX.h`, `dolphin/pad/Pad.h` and `dolphin/vi/vi.h`.
- Encouraging: game code uses the GX objects only opaquely. The exceptions are layout-dependent members such as `d_map.h` `GXTexObj field_0x370[10]`, which belong to phase 4.

## Facts for phase 3

- **The decomp's `configure.py`** (`ref/decomp-snrubrm`) has `Rel("f_pc_profile_lst")` plus 414 `ActorRel`s, one unit each. 26 `d/actor` units are in `main.dol`. The REL glue is `src/REL/executor.c`, plus `global_destructor_chain.c` taken from PowerPC_EABI_Support.
- **Duplicate symbols:**
  - `ModuleProlog`/`ModuleEpilog` are defined in both `src/DynamicLink.cpp:545-551` and `src/f_pc/f_pc_profile_lst.cpp`.
  - Same-named classes are defined in several actor `.cpp` files: `daNpc_Gp1_HIO_c` (2), `NpcDatStruct` (3), `PsoData`, `SaveDatStruct`, `attack_info_s` and `SafetyCallback`. These are possible ODR conflicts (weak inline methods and vtables merged silently).
- **Profiles:**
  - `g_fpcPf_ProfileList_p` is set only by the REL's `ModuleProlog`.
  - `f_pc_profile_lst.h` declares all 503 profiles as `process_profile_definition`, while they are defined as `actor_process_profile_definition` and similar. This links (variable names are not type-mangled), but it is undefined behaviour; Dusklight fixed it with `.base.base`.
- **`m_Do_main.cpp:533`** declares `int main(int argc, const char* argv[])`, while Aurora's `aurora_main` is `extern "C" int aurora_main(int, char**)`. Including `aurora/main.h` therefore needs a `TARGET_PC` signature change.
- **JAudio and JAZelAudio** (74 units) are outside phase 1, but the game calls them everywhere. Without them a strict link cannot succeed in phase 3.

---

## Phase 2: the GameCube SDK over Aurora

Steps that add new files outside `game` can run alongside phase 1. Steps that edit `game` sources must not run on a module phase 1 is still working on.

| ID | Step | Files | From Dusklight (CC0) or Aurora (MIT) | Verification | Depends on | Risks |
|---|---|---|---|---|---|---|
| **2.1** | Bring in Aurora at `3227d76` behind `COS_WITH_AURORA` (default OFF until 2.9). GX, DVD, CARD and THP on, RmlUi off, Dawn and nod from prebuilt packages. Offline builds use CMake's own `-DFETCHCONTENT_SOURCE_DIR_AURORA=ref/aurora` after `git -C ref/aurora worktree` at the pin. | new `native/cmake/Aurora.cmake`; one `include()` line in `native/CMakeLists.txt`; README section | Dusklight `CMakeLists.txt:73-78` (`AURORA_ENABLE_*`, `aurora_mtx PRIVATE MTX_USE_PS=1`) | `cmake -S native -B build/native-mac -G Ninja -DCOS_WITH_AURORA=ON && ninja -C build/native-mac aurora_core aurora_gx aurora_gd aurora_os aurora_vi aurora_pad aurora_si aurora_mtx aurora_dvd aurora_card aurora_main` builds; `ninja SSystem cos_scaffold_check` still builds; `nm -g` on `libaurora_mtx.a` shows `_PSMTXConcat` | — | Needs network (Dawn package, abseil, SDL3). Configure time. Must not leak `-fno-exceptions` or game flags into Aurora (they are on the interface target, so it is fine). Aurora is MIT, so its notice goes in `RIGHTS_AND_LICENSES.md`. |
| **2.2** | `cos_sdk` STATIC library skeleton. Globs `native/sdk/src/**/*.{c,cpp}` with `CONFIGURE_DEPENDS`, so parallel steps never touch CMake. Compiled with Aurora headers only (no game headers), linked to the Aurora libraries. Adds a headless `cos_sdk_smoke` program (no GPU). | new `native/cmake/sdk.cmake`, `native/sdk/README.md`, `native/sdk/tests/sdk_smoke.cpp` | — | `ninja cos_sdk cos_sdk_smoke && build/native-mac/cos_sdk_smoke` prints `ok` (OSInit, OSGetTime, PSMTXConcat) | 2.1 | Aurora's `OSInit` may expect `aurora_initialize` first; the smoke test may need a headless config. |
| **2.3** | SDK header mode `COS_SDK_HEADERS=decomp\|aurora`. In aurora mode the order is `native/include` → `native/include/sdk` → Aurora `include` → `game/include`. Aurora wins all 39 collisions, so its own internal `<dolphin/gx/GXStruct.h>` includes stay consistent. Adds forwarders for the base, OS, MTX, VI and PAD game-only names (`dolphin/os/OS.h` → `<dolphin/os.h>` + extras), plus `cos_sdk_extras.h` (`READU32_BE`, `uint`, `FLOAT_MIN/MAX`). Adds a **shadow check**: one unit includes all 82 game SDK header names with `-MD`, and a script fails if any dependency resolves under `game/include/dolphin`. | `native/cmake/GameConfig.cmake` (mode only); new `native/include/sdk/dolphin/{os,base,mtx,vi,pad}/...`; `native/check/sdk_headers.cpp`; `native/check/check_sdk_shadow.sh` | Dusklight's include order (`GameABIConfig.cmake`) | `ninja cos_scaffold_check cos_sdk_header_check` in both modes; shadow script exits 0 (for the names done so far) | 2.1 | The extras header must not redeclare things Aurora declares (`-Werror` on duplicate typedefs is clean in C++). `GameConfig.cmake` is shared with phase 1, so keep the diff small. |
| **2.4** | Remaining forwarders: GX (`GX.h`, `GXInit.h`, `GXAttr.h`, `GXMisc.h`, `GXLight.h`, `GXFrameBuf.h`, `GXDisplayList.h`), GF (game-only GF declarations go in extras), GD, DVD (`dvd.h`, `dvdlow.h`, `dvdfs.h`, `fstload.h`), AR, AI, SI, EXI, GBA, DB, amcstubs. **No changes to `game`.** | `native/include/sdk/dolphin/**` | — | Shadow check passes for all 82 names | 2.3 | game-only declarations Aurora lacks (GBA API, `GXGetTexObj*` variants, `OSStopwatch`, DVD internals) must be declared exactly once. `thp.h` collides: `d_a_movie_player` may rely on the game's THP structs. |
| **2.5** | **Link census tool.** `cos_link_census` is a `MODULE` (bundle) linked with `-Wl,-undefined,dynamic_lookup` from every enabled module's objects, with REL units removed through `$<FILTER:$<TARGET_OBJECTS:m>,EXCLUDE,regex>`, plus `cos_sdk` and Aurora. A post-build script runs `nm -um` and sorts symbols into: SDK, REL (`OSLink*`, `OSSetStringTable`, `g_profile_*`), JAudio/JAZel, MSL/runtime, deferred game units, other. Adds the REL unit list (415 names from `configure.py`; names only, no game data). Also `native/tools/symbol_census.py`: duplicate strong definitions, and weak definitions with differing sizes, across objects, without linking. | new `native/cmake/census.cmake`, `native/cmake/rel_units.txt`, `native/tools/link_census.py`, `native/tools/symbol_census.py` | — | `ninja cos_link_census` then `cat build/native-mac/link_census.txt` gives per-category counts; `symbol_census.py` runs on the SSystem objects | 2.2 (useful in both header modes, meaningful in aurora) | `-undefined dynamic_lookup` is deprecated with chained fixups (a warning only on a bundle). Duplicate symbols still stop the link, which is wanted: `symbol_census.py` lists them first. |
| **2.6a** | OS threading: `OSCreateThread`/`Resume`/`Suspend`/`Sleep`/`Wakeup`/`Join`/`Yield`/`Cancel`/`Exit`, thread-specific storage, `OSDisableInterrupts`/`OSRestoreInterrupts` (recursive mutex), mutex and condition variables, message queues, alarms (no-op as in Dusklight, or a host timer thread; see decision D6), `OSContext` stubs. Dusklight's `dusk::IsShuttingDown`, Tracy and `JKRHeap` couplings are replaced by an `sdk_hooks.h` (shutdown flag; the current-heap hook is set later by game glue). | `native/sdk/src/os/{OSThread,OSMutex,OSMessage,OSAlarm,OSContext}.cpp`, `native/sdk/include/cos_sdk/hooks.h` | Adapt `src/dusk/OSThread.cpp`, `OSMutex.cpp`, `OSContext.cpp`, and the message-queue part of `stubs.cpp:60-200`. Note the provenance at the top of each file. | `cos_sdk_smoke threads`: two threads exchange 1000 messages through `OSSendMessage`/`OSReceiveMessage` with `OS_MESSAGE_BLOCK`, plus a mutex/cond counter test; exits 0 under `-fsanitize=thread` | 2.2 | The game's `JKRThread` reads stack fields of `OSThread` (Dusklight sets them, `OSThread.cpp:170`). Thread priorities are cooperative on GameCube: game code can assume a higher-priority thread runs immediately (phase 6 behaviour). |
| **2.6b** | OS misc: `OSReport`/`OSPanic`/`OSFatal`/`OSVReport`, `OSGetConsoleType`, `OSGetSoundMode`, `OSGetLanguage`, `OSGetProgressiveMode`, reset and reboot (`OSResetSystem`, `OSGetResetCode`, `OSRegisterResetFunction`), SRAM (`__OSLockSram` with a static `OSSram`), RTC, stopwatch, font (`OSInitFont` returns false), `PPCMf*`/`PPCMt*`, `LC*` (locked cache: `LCStoreData` = memcpy), `DC*`/`IC*` gaps, `OSSetSaveRegion`, `OSProtectRange`, error handlers | `native/sdk/src/os/{OSReport,OSMisc,OSReset,OSSram,PPC,LC}.cpp` | `src/dusk/OSReport.cpp` (`borealis` log → `fprintf(stderr)`), `stubs.cpp:44-230, 1013-1035` | `cos_sdk_smoke misc`; census "SDK/OS" bucket shrinks | 2.2 (parallel with 2.6a, separate files) | `OSFatal` and `OSPanic` should abort loudly, not be no-ops. |
| **2.6c** | VI retrace emulation (`VIWaitForRetrace`, pre/post retrace callbacks, retrace count, `VISetNextFrameBuffer`, `VISetBlack`, `VIGetNextField`, `VIGetDTVStatus`) and GX gaps (metrics, `GXSetDrawSync`/`Callback`, `GXWaitDrawDone`, FIFO base/size getters, `GXSetMisc`, `GXSetCopyClamp`, `GXGetNumXfbLines`/`GXGetYScaleFactor`, `GXAbortFrame`, current GX thread, plus any game-only GX the census lists) | `native/sdk/src/vi/VIRetrace.cpp`, `native/sdk/src/gx/GXExtras.cpp` | `stubs.cpp:280-345, 900-1000` | `cos_sdk_smoke vi`: register a pre-retrace callback, call `VIWaitForRetrace()` 3 times, count == 3 | 2.2 | Retrace pacing belongs to the main loop (phase 6). Keep it as a plain API. |
| **2.6d** | GF: compile the game's `src/dolphin/gf/GF{Geometry,Light,Pixel,Tev,Transform}.cpp` into `cos_sdk` against Aurora's GF headers plus extras (`GFLoadPosMtxImm`, `GFSetArray`, `GFBegin`/`GFEnd`, `GFSetTevColorS10`, `GFSetBlendModeEtc`, `GFSetFog`, …), with `TARGET_PC` edits only if needed | `native/cmake/sdk.cmake` (adds the 5 `game/src/dolphin/gf` paths), `native/include/sdk/cos_gf_extras.h` | Dusklight compiles Dusklight's GF the same way (`files.cmake:1406`); Aurora `gf/GFGeometry.h` writes through `GXCmd1u*` | `ninja cos_sdk`; `nm` shows the 5 units' `GF*` symbols defined | 2.4 | GF writes raw BP/XF/CP commands; Aurora must parse them in immediate mode (it does for display lists; check `GXCmd1u8` outside a display list). |
| **2.6e** | Devices Aurora lacks or covers only partly: DVD (`DVDLow*`, streaming, `DVDChangeDir`, `DVDCancel*`, `DVDGetDriveStatus`, `DVDCheckDisk`, `DVDSetAutoFatalMessaging`), AR (`ARStartDMA`, `ARRegisterDMACallback`, `ARGetBaseAddress`, `ARGetDMAStatus`, routed to Aurora's `aramToHost`), CARD functions missing from Aurora's `card.cpp` (whatever the census lists, e.g. icon and banner, `CARDGetSerialNo`), GBA (all `GBA*` return "not ready"), EXI, SI extras, DB, amcstubs | `native/sdk/src/{dvd,ar,card,gba,exi,db}/*.cpp` | `stubs.cpp:240-280` (EXI) | Census "SDK" bucket for these prefixes reaches 0; `cos_sdk_smoke ar` round-trips 4 KB through `ARStartDMA` | 2.2, 2.5 | AR: Aurora's ARAM base comes from `g_config.mem2Size`; the `JKRAram` changes from phase 1 (`uintptr_t`) must agree with it. |
| **2.6f** | Audio-hardware stubs for phase 5 (AI, DSP, DTK, `AIInitDMA` with `uintptr_t`) and runtime/MSL extras (`stricmp`, `strnicmp`, and any MSL-only names the census shows) | `native/sdk/src/audio/AIStubs.cpp`, `native/sdk/src/audio/DSPStubs.cpp`, `native/sdk/src/runtime/extras.c` | `stubs.cpp:866-900` (AI); `src/dusk/extras.c` (keep only the string functions; `__dcbz`/`__cntlzw` already live in `cos_pc_config.h`) | Census buckets "SDK/audio" and "MSL" are 0 | 2.2 | DSP semantics matter only once JAudio is in (phase 5); now they just must not hang. |
| **2.7…** | **Migrate compiled modules to the Aurora headers**, one step per module: SSystem; JSystem-core; J3D; J2D+JParticle; JStudio/JStage/JMessage; framework+m_Do; d-core (split into 2 steps alphabetically if large); actors-1 to actors-6 (6 parallel steps, separate files). Fix errors with `#if TARGET_PC` in `game`. Expected kinds of fix: OS struct field names, FIFO macro use (`J3DGD`, `J3DTevs`, `J3DMatBlock`, `JRenderer`, `GXWGFifo`, `GDWrite*`), `OSRoundUp32B` duplicates, `STATIC_ASSERT` sizes (record them as phase 4 items rather than "fixing" layouts). | `game/src/<module>/**`, `game/include/<module>/**` | Dusklight's `TARGET_PC` edits to the equivalent JSystem files (J3D, JUtility, JKernel are near-identical in Dusklight) | `cmake -DCOS_SDK_HEADERS=aurora -DCOS_MODULE_<m>=ON … && ninja -k 0 <m>`: 0 errors; still clean in decomp mode | 2.3, 2.4, and **phase 1 has finished that module** | Biggest unknown in effort, and the step most likely to conflict with phase 1's edits. Hundreds of errors are possible in J3D. Split a step whenever it exceeds about 40 changed files. |
| **2.8** | **Default switch:** `COS_SDK_HEADERS=aurora` becomes the default and decomp mode is removed for `TARGET_PC`; `COS_WITH_AURORA` ON; README and plan updated | `GameConfig.cmake`, `Aurora.cmake`, `native/README.md`, `docs/NATIVE_PORT_PLAN.md` | — | Clean configure, `ninja cos_modules` with all ready modules, shadow check passes | all of 2.7 for the modules phase 1 has finished | Needs coordination with the phase 1 workflow (decision D3). |
| **2.9** | **Phase 2 exit:** run the census over every non-REL unit. Commit `native/check/expected_unresolved_phase2.txt`. It may contain only REL-related symbols (`OSLink`, `OSLinkFixed`, `OSUnlink`, `OSSetStringTable`, `g_profile_*` referenced by main.dol code), JAudio/JAZelAudio (phase 5), and units deferred under `cos_defer` with their reasons. Write the phase 2 log in the plan with the exact list. | `native/check/expected_unresolved_phase2.txt`, `docs/NATIVE_PORT_PLAN.md` | — | `ninja cos_link_census && diff -u native/check/expected_unresolved_phase2.txt build/native-mac/link_census_unresolved.txt` is empty; `symbol_census.py --dol` reports 0 duplicate strong symbols | 2.5–2.8 | If phase 1 still has deferred units, the exit is "every unit phase 1 has compiled", and the list says so. |

**Can run in parallel:**
- 2.6a–2.6f with each other (separate files under `native/sdk/src/*`, globbed);
- 2.6a–2.6f with 2.3/2.4 (headers);
- 2.5 with all of them;
- the 2.7 actor steps with each other.

2.1 → 2.2 must come first. 2.6d also touches `sdk.cmake`; give it its own small `native/cmake/sdk_gf.cmake` to avoid a conflict.

---

## Phase 3: RELs linked statically into one executable

| ID | Step | Files | From Dusklight | Verification | Depends on | Risks |
|---|---|---|---|---|---|---|
| **3.1** | Full symbol census including REL units: duplicate strong symbols, and weak symbols defined in more than one REL with different sizes (ODR suspects). Output to `build/native-mac/symbol_census.txt`. | `native/tools/symbol_census.py` (`--all` mode), `native/cmake/census.cmake` | — | Report lists at least `ModuleProlog`/`ModuleEpilog` and the same-named classes found above | 2.5, all actor modules compiled | Only finds what is compiled. Deferred units are invisible. |
| **3.2** | Duplicate strong symbols. `f_pc_profile_lst.cpp`: `ModuleProlog`/`ModuleEpilog` under `#if !TARGET_PC`. Every other duplicate the census lists: make it `static` or put it in an unnamed namespace under `TARGET_PC`. One step per actor chunk if there are many (parallel, separate files). | `game/src/f_pc/f_pc_profile_lst.cpp`, `src/d/actor/*.cpp` | — | `symbol_census.py --all --dups` reports 0 | 3.1 | Renaming could break symbols referenced from main.dol. Only make internal what nothing else references (check with `nm -u` over all objects). |
| **3.3** | ODR audit: classes and structs defined in more than one `.cpp`/`.inc` (`daNpc_Gp1_HIO_c` ×2, `NpcDatStruct` ×3, `PsoData`, `SaveDatStruct`, `attack_info_s`, `SafetyCallback`, plus `Attr_c` ×12 if any is outside a namespace). Wrap them in a per-REL namespace or unnamed namespace under `TARGET_PC`. | the actor `.cpp` files concerned | — | Census "weak definitions with different sizes" = 0; `grep`-based list of duplicate class names outside namespaces = 0 | 3.1 (parallel with 3.2 if the file sets differ) | The linker merges these silently; missing one gives wrong vtables at runtime, not a link error. |
| **3.4** | Static profile list. `f_pc_profile.cpp` under `TARGET_PC`: `g_fpcPf_ProfileList_p = g_fpcPfLst_ProfileList`, and `fpcPf_Get` checks bounds and NULL. Optionally fix the extern types (decision D5). | `game/src/f_pc/f_pc_profile.cpp`, `include/f_pc/f_pc_profile.h`, optionally `include/f_pc/f_pc_profile_lst.h` and `src/f_pc/f_pc_profile_lst.cpp` | `ref/dusklight/src/f_pc/f_pc_profile.cpp` (without the mod service) and `f_pc_profile_lst.cpp` `.base.base` | Census: `g_profile_*` no longer unresolved | 3.2 | — |
| **3.5** | Replace DynamicLink. `c_dylink.cpp` under `TARGET_PC`: `DynamicNameTable` empty; `cCc_Init` creates no DMCs; `cDyl_LinkASync`/`cDyl_Link` return `cPhs_COMPLEATE_e`; `cDyl_Unlink` returns FALSE; `cDyl_InitCallback` does not load `/dvd/framework.str` or link `f_pc_profile_lst`, only sets `cDyl_Initialized`; `cCc_Check` does not test 0x80000000 addresses. `DynamicLink.cpp`: loading and linking (`do_load`, `do_link`, `do_unlink`, checksum, `OSLink*`) under `!TARGET_PC`, keeping the class shell. | `game/src/c/c_dylink.cpp`, `game/src/DynamicLink.cpp`, `include/DynamicLink.h` | `ref/dusklight/src/c/c_dylink.cpp:813-1030`, `src/DynamicLink.cpp` (`#if !TARGET_PC` blocks at 17 and 174) | Census: no `OSLink`, `OSLinkFixed`, `OSUnlink`, `OSSetStringTable` | 2.9 | Keep `cDyl_InitAsync`'s DVD-thread callback, since its sequencing (it creates the logo scene) is relied on at boot. |
| **3.6** | Deferred actors, only if `cos_defer` still lists REL units: CMake generates a unit with a zeroed profile per deferred actor, and `fpcPf_Get` returns NULL for a zero profile. Plus a log line. | `native/cmake/rels.cmake`, generated `build/.../cos_deferred_profiles.cpp` | — | The player succeeds with an actor forced into `cos_defer` as a test | 3.4 | Spawning such an actor must fail cleanly (`fopAcM_create` returns an error), not crash. |
| **3.7** | JAudio for the link (decision D4). **(a)** Compile `JSystem/JAudio` and `JAZelAudio` as a module `audio` against the 2.6f stubs (silent). **(b)** `native/tools/gen_traps.py` turns a committed list `native/check/unresolved_traps.txt` (symbol + owning phase) into a `.S` file: one `brk` label per function, zeroed storage per variable. | (a) `native/cmake/modules.cmake` + `game/src/JSystem/JAudio/**`; (b) the tool + list | Dusklight builds JAudio2 for real; it has no trap generator | (a) `ninja -k 0 audio` 0 errors; (b) census ∖ traps = ∅ | 2.9 | (a) is 74 units of 64-bit audio code, roughly 3–6 steps. (b) can hide missing work, so each entry needs a phase. |
| **3.8** | Executable `centollos`: objects ordered main.dol units first, then the 415 REL units. ld64 runs static initialisers in input order, so RELs see main.dol globals already initialised, as they did when the REL `_prolog` ran after boot. Links `cos_sdk`, the Aurora libraries and `aurora::main`. `m_Do_main.cpp` under `TARGET_PC`: `#include <aurora/main.h>`, and the signature `int main(int, char**)`. | new `native/cmake/executable.cmake`, `game/src/m_Do/m_Do_main.cpp` | `ref/dusklight/src/m_Do/m_Do_main.cpp` (only the include and signature; the Aurora event loop is phase 6) | `ninja -C build/native-mac centollos` exits 0 with no `-undefined dynamic_lookup`; `nm -u build/native-mac/centollos` lists only system and framework symbols | 3.2–3.7 | The player time. Metal, QuartzCore and IOKit frameworks come from the Aurora targets. |
| **3.9** | Static-initialisation smoke check: under `TARGET_PC`, `COS_SMOKE=static-init` makes `main` check that each non-NULL `g_fpcPfLst_ProfileList[i]` has `mProcName == i` (list order = profile number), then exit 0 before any SDK initialisation | `m_Do_main.cpp` or new `native/src/pc/smoke.cpp` | — | `COS_SMOKE=static-init build/native-mac/centollos; echo $?` → 0 | 3.8 | REL global constructors now run before `main`, so before any `JKRHeap` exists. Any constructor that calls `operator new` crashes here; that is exactly what this check is for. If the index invariant does not hold in the game, check `fpcPf_Get` instead. |
| **3.10** | Phase 3 log and README (layout, targets, census) | `docs/NATIVE_PORT_PLAN.md`, `native/README.md`, `RIGHTS_AND_LICENSES.md` (Dusklight CC0 provenance, Aurora MIT notice) | — | Review | 3.9 | — |

**Can run in parallel:** 3.2 and 3.3 per actor chunk; 3.4 and 3.5 (different files); 3.7 alongside 3.2–3.5.

---

## Decisions that need you

- **D1. Aurora's version and how it is pulled in.** Options:
  - FetchContent pinned to `3227d76`, Dusklight's pin (my recommendation);
  - a git submodule `native/extern/aurora`;
  - `ref/aurora`: gitignored, not reproducible, and 38 commits older.

  Related: the Switch layer (`switch/aurora`, `patches/aurora`) targets GXRuntime's Aurora fork. Phase 7 will have to port the SDL3 shim and GLES patches to upstream, or the native port has to stay on the fork.

- **D2. SDK headers.**
  - **(A) Recommended:** Aurora's headers are the only SDK (the Dusklight model): include order plus forwarders in `native/include/sdk`, with `game/include/dolphin` never reached.
  - **(B)** Keep the game's headers and edit them under `TARGET_PC` to match Aurora's ABI. Less churn, but two declarations of the same API with no type checking across C linkage.

- **D3. When to switch phase 1 onto the Aurora headers (2.8).** Every module phase 1 finishes on the decomp headers has to be compiled again (2.7). Switching early (before J3D, d-core and the actors) avoids work done twice, but changes the ground under the phase 1 workflow.

- **D4. JAudio for the phase 3 link:** build it now, silent, as an early part of phase 5, or use trap stubs from a committed list.

- **D5. Profile extern types:** fix the declarations (Dusklight's `.base.base`) or leave the type punning that MWCC tolerated.

- **D6. OS alarms:** no-op as in Dusklight (`OSSetAlarm {}`), or a real timer thread. The game may depend on alarms (e.g. `mDoMemCd` and the DVD error timers), and that only shows up at runtime.

- **D7. Copied code.** Copy Dusklight's `src/dusk/OS*.cpp` and `stubs.cpp` and adapt them (CC0, provenance in the file header), or rewrite them. Also: where the game-specific SDK lives (`native/sdk/` is my proposal).

- **D8. Phase 2's exit when phase 1 is not finished:** accept "all units compiled so far" plus an explicit list.

### Critical Files for Implementation
- native/cmake/GameConfig.cmake
- ref/dusklight/src/dusk/stubs.cpp (plus `OSThread.cpp`, `OSMutex.cpp` alongside)
- ref/dusklight/src/c/c_dylink.cpp
- game/src/c/c_dylink.cpp (with `game/src/DynamicLink.cpp` and `game/src/f_pc/f_pc_profile.cpp`)
- ref/aurora (at commit `3227d76`: `cmake/aurora_*.cmake`, `include/dolphin/**`)
## Decisions taken (2026-10-03)

Phase 1 finished with all 840 in-scope units compiling and none deferred, so the overlap notes above no longer apply.

- **D1:** Aurora via FetchContent pinned to `3227d76` (Dusklight's pin); `FETCHCONTENT_SOURCE_DIR_AURORA` allowed for offline builds. The Switch layer's move from the GXRuntime fork to upstream Aurora is a phase 7 task.
- **D2:** (A) Aurora's headers are the only SDK headers; game-only names get forwarders in `native/include/sdk`; `game/include/dolphin` is never reached (shadow check).
- **D3:** switch right away (phase 1 is done); the 2.7 migration runs module by module.
- **D4:** (a) build JAudio and JAZelAudio for the phase 3 link, silent over the 2.6f stubs; trap stubs only for what remains, each with its owning phase.
- **D5:** fix the profile extern types (Dusklight's `.base.base`).
- **D6:** OS alarms are a real host timer thread, not no-ops.
- **D7:** adapt Dusklight's `src/dusk/OS*.cpp` and `stubs.cpp` (CC0) with provenance in each file header; game SDK layer lives in `native/sdk/`.
- **D8:** not needed: phase 1 deferred nothing.
