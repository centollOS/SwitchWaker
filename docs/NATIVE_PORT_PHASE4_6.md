# Phases 4–6 plan: 64-bit and endianness, audio, Mac milestones

**Basis:** `docs/NATIVE_PORT_PLAN.md` at 256aa59 (no Phase 3 log is committed yet), `docs/NATIVE_PORT_PHASE2_3.md`, `native/README.md`, `native/sdk/**`, Dusklight `40457c6` and Aurora `3227d76`.

**What this plan assumes Phase 3 delivers:**
- the `centollos` executable (3.8);
- `COS_SMOKE=static-init` (3.9);
- JAudio/JAZelAudio compiled and silent (3.7, decision D4a).

## Findings that shape the plan

1. **The 255 `TODO(native phase 4)` markers are only the pointer-width part.** Big-endian disc data is mostly unmarked, because clang compiles a big-endian read without complaint. Of the 255:
   - 173 are `setUserArea`/`mUserArea` sites (168 in actors, 3 `J3DPacket`, 2 calc user area);
   - 30 are in J3D (`J3DModelLoader` 15, `J3DAnmLoader` 4, factories, `J3DDrawBuffer`/`Shape`/`Joint`/`Sys`, `J3DMaterial.h`/`J3DTexture.h`/`J3DPacket.h`);
   - 12 are in `d/` (`d_stage` 5, `d_s_actor_data_mng` 2, `d_s_menu` 2, `d_com_inf_game` 2, `d_resorce` 1);
   - 8 are in JStudio, 4 in `J2DPrint` (harmless: the 32-bit difference is still right), 9 are debug or stack walks (`m_Do_printf`, `m_Do_machine`, `DynamicLink`);
   - the rest are one each: `JKRExpHeap` header, `JKRAramPiece` callback, `JSupport`, `JGadget::TVector` `/ 4`, `c_bg_s` (dzb), `OS.h`, `m_Do_hostIO`, the THP header, and the `OSContext` note in cos_sdk.
   - The old build logs also show 14 `-Wreturn-type` warnings (missing returns, which clang compiles as undefined behaviour) and 6 `-Wfortify-source` warnings. These need fixing first.
2. **Bugs already in the tree:**
   - `J3DPacket::setUserArea(u32 area)` on PC still truncates before `(void*)area`.
   - `TVector::size`'s `/ 4` is wrong with 8-byte pointers.
   - `dsptask.c` does `(u16*)(jdsp + 0x80000000)`.
   - Many file-mapped structs declare file offsets as `void*` (for example `J3DModelLoader.h` `mpHierarchy`, `mpVtxPosArray`). They are 8 bytes on the host, so every field after them is at the wrong offset.
3. **Dusklight's method is a hybrid, mostly `BE<T>` in place:**
   - `BE(T)` fields in the disc-mapped struct definitions (`JKRArchive.h` 25, `J3DModelLoader.h` 108, `J3DAnimation.h` 206, `d_stage.h` 98, `d_bg_w.h` 37, `JUTFont.h`, `JUTTexture.h`, the JPA blocks, `stb-data.h`…). On GameCube `BE(T)` is just `T`.
   - `OFFSET_PTR(T)`: a self-relative offset with a "relocated" bit, for offsets the game used to overwrite in place with pointers (dzb tables, stage chunks, paths).
   - Side tables where a pointer was stored inside file data: `SDIFileEntry::index` plus `JKRArchive::mFileData`.
   - Byte-swap at load only for CPU-hot arrays handed to generic code: J3D transform key data, JStudio fvb copied into `mSwappedData`.
   - Bulk GPU data stays big-endian. Aurora reads big-endian display lists and textures, and vertex arrays when `GXSETARRAY(..., le=false)`.
4. **Pointers that must fit in 32 bits can be kept inside MEM1.** Aurora's `OSCachedToPhysical` returns an offset into its MEM1 block. If every JKR heap stays inside MEM1 (`AuroraConfig.mem1Size`), each u32 "physical address" the game hands to the DSP, AI or AR remains valid. This matters for phase 5. Also, macOS arm64's 4 GiB `__PAGEZERO` means a pointer truncated to 32 bits always faults, so truncation bugs show up immediately.
5. **The game's title screen is a 3D stage.** `dComIfG_changeOpeningScene` sets `sea_T` room 44, which is Outset. So "boot to title" already needs stage files, dzb collision, J3D, actors and particles. The logo scene loads about 15 resource kinds first: messages, fonts, ruby font, `common.jpc`, item table, actor data, fmap, lod, error/menu resources.
6. **Audio:**
   - The game's JAudio1 `DSPBuffer` is 0x180 bytes with the same automixer layout as JAudio2's `TChannel`. Both drive the "the princess" DSP ucode family. The streamed AFC files are decoded on the CPU (`StreamLib::__DecodeADPCM`) and played as direct PCM from main RAM.
   - Dusklight does not emulate the DSP. It replaced the mail protocol: `JASAudioThread::run` is removed, an SDL3 stream callback drives `updateDSP`, and `DuskDsp` (953 lines, CC0, plus freeverb) mixes the `TChannel`s. `DspStub.cpp` crashes on any mail call.
   - This repository already has a second option. `runtime/host/src/dsp_hle_backend.cpp` wraps Dolphin's DSPHLE (the JAudio ucode) for the translated build, with an "audio envelope r = 0.999 against LLE" result and Switch runs. It is GPLv2+; the repository as a whole is GPLv3 (`RIGHTS_AND_LICENSES.md`).
7. **Aurora 3227d76 has no screenshot or readback API.** It has `aurora_get_stats()` (draw calls, texture uploads) and a `BACKEND_NULL` that is available only if the prebuilt Dawn package enables it.

---

## How the work is organised

There are two tracks plus an audio side-track:
- **Track B (boot):** a loop driven by milestones.
- **Track S (sweeps):** each disc format is checked by a headless `COS_SMOKE=<fmt>-sweep` mode, independent of how far boot has got. These steps touch separate files, so they can run in parallel with the boot loop.
- **Audio:** stays off (`COS_AUDIO=off`, like Dusklight's `DUSK_AUDIO_DISABLED` calling `onInitFlag`) until phase 5 lands.

**Order:** 6.0 → 4.0a–d (in parallel) → 6.1 → 4.1 → 4.2 → 4.3 → 4.4 → 6.2 → 4.5–4.8 → 4.9a–d / 4.10 / 4.11 / 4.12 / 4.13 (sweeps in parallel; the boot loop drives M7–M9) → 6.3 → 4.15 → 6.4 → 4.16 → 6.5 → 6.6 → 4.17 → 6.7–6.9. Phase 5 runs alongside from M6 on.

### Milestones (logged as `[cos] MILESTONE <name> frame= retrace= ms=`)

| ID | Name | Criterion |
|---|---|---|
| M0 | static-init | 3.9 passes |
| M1 | aurora-up | window and device created; `aurora_dvd_open` ok; disc ID is `GZLE01`, version 0 |
| M2 | heaps | `mDoMch_Create` returned; `JKRExpHeap::check()` true for every heap |
| M3 | gfx-create | `mDoGph_Create`, `mDoCPd_Create` and `LOAD_COPYDATE` ran |
| M4 | frame-loop | 120 game frames; retrace count goes up; `aurora_get_stats()->drawCallCount > 0` |
| M5 | logo-scene | LOGO scene exists; Logo archive mounted; texture upload > 0 |
| M6 | logo-res | every `l_*Command` synced; `changeOpeningScene` called |
| M7 | opening | `sea_T` stage arc mounted; `dStage_Create` done |
| M8 | title-stage | room 44 loaded; collision registered; actors created; 300 frames without a fault |
| M9 | title | `d_a_title` created and drawing (BLO + JPA) |
| M10 | file-select | scripted START reaches the name / file-select scene |
| M12 | outset-debug | `COS_BOOT_STAGE` puts Link in Outset; PLAY scene; player actor exists |
| M13 | outset-control | holding the stick for 120 frames moves Link more than 300 units; 3,600 frames without a fault |
| M11 / M14 | new-game / outset-real | the real new-game flow, through the intro cutscene (STB) |

### The crash-to-fix loop (phases 4 and 6 together)

```
target := next milestone
loop:
  native/tools/run.sh <target> --timeout 180        # writes build/native-mac/runs/<target>-<ts>/
  exit 0  -> run the same command 3 times (flake check); append to the plan log; commit; target++
  exit 13 (signal) / 12 (OSPanic, JUT_ASSERT) -> backtrace.txt comes from the built-in handler;
          then native/tools/lldb_crash.sh reruns:
          lldb --batch -o run -k "thread backtrace all" -k "frame variable" -k "register read" -k "quit 13" -- build/native-mac/centollos
          (the environment is inherited by the inferior)
  exit 11 (stall: frame counter frozen for COS_STALL_S) -> `sample <pid> 3` of all threads, then SIGABRT
  exit 10 (timeout while frames still advance) -> treat it as "waiting on something":
          read the last lines of the COS_TRACE=res,scene log
  classify the failure:
    truncation: fault address < 2^32, or upper 32 bits cleared
    endian: counts like 0x01000000, reversed magic, floats 1e38 or denormal,
            sizes larger than the file (check against disc_manifest.json)
    layout: cos_layout_check xfail / offset differs from the decomp's /* 0xNN */ comments
    render-only: wrong picture, no fault -> log it under "phase 6 render issues"; do not block
                 unless the milestone criterion depends on it
  fix: the smallest change, under #if TARGET_PC with the GameCube code kept in #else;
       one cause per commit; any struct changed to BE goes into layout_headers.txt
  regression before each commit: ninja centollos cos_sdk_smoke cos_layout_check cos_pc_tests;
       cos_sdk_smoke; every earlier milestone; every passing sweep; census diff
```

**Ownership in the boot loop:** a milestone loop may fix whatever root cause the boot hits, even in files a later format step owns; that later step then covers the rest of its format (sweep, layout xfail, remaining fields). "Files another active step owns" means a step running at the same time, not a later one.

**Stop conditions, which escalate to the human:**
- the same crash signature after 3 fix attempts;
- a fix needs Aurora changed, GPL code, or a decision listed below;
- a fix touches files another active step owns;
- a step is past 1 hour or 10 commits (split it);
- a struct is used both as disc data and as a host object with conflicting needs.

**Success:** the milestone passes 3 runs in a row.

---

## Phase 6 harness steps (first, because phase 4 verifies through them)

**6.0 Run harness**
- Files: new `native/src/pc/{pc_disc,pc_milestone,pc_crash,pc_smoke,pc_watchdog}.cpp` (globbed by `executable.cmake`), `native/tools/run.sh`, `native/tools/lldb_crash.sh`.
- Environment variables:
  - `COS_DISC`: required. The run script defaults it to `/path/to/GZLE01.iso`, which is never committed; add `*.iso`/`*.ciso` to `.gitignore`.
  - `COS_SMOKE`, `COS_MILESTONE`, `COS_TIMEOUT_S`, `COS_STALL_S`, `COS_TRACE`, `COS_UNCAPPED`, `COS_AUDIO`, `COS_FRAMES`.
- Exit codes: 0 reached, 10 timeout, 11 stall, 12 panic, 13 signal, 14 disc problem.
- The crash handler (`sigaction` plus `backtrace_symbols_fd`) prints the scene, frame and last resource. If 3.9 put `COS_SMOKE` handling in `m_Do_main.cpp`, move it here.
- From Dusklight: nothing directly (`borealis::crash` is not available).
- Verify:
  - `native/tools/run.sh static-init` gives exit 0.
  - `COS_DISC=/nonexistent build/native-mac/centollos; echo $?` prints 14.
  - A test `COS_SMOKE=crash-test` (deliberate null write) gives 13 with a symbolised backtrace.
- Risk: lldb may need `DevToolsSecurity` enabled (a human prerequisite). The built-in handler is the fallback.

**6.1 Aurora bring-up in `main`**
- Files: `game/src/m_Do/m_Do_main.cpp` (`TARGET_PC`), `native/src/pc/pc_main.cpp`.
- What changes:
  - `main` calls `aurora_initialize` (with `mem1Size` per decision H5, `mem2Size` 16 MiB, `userPath`/`cachePath` under `build/native-mac/user`, vsync off when uncapped).
  - Then `aurora_dvd_open(COS_DISC)` and a check of `DVDGetCurrentDiskID()`.
  - `main01` runs on the process main thread, as in Dusklight, instead of `OSCreateThread` plus suspend: SDL/Metal needs the main thread.
  - `COSSdkSetThreadStartHook` sets the current `JKRHeap` for each thread.
  - `COS_AUDIO=off` calls `mDoAud_zelAudio_c::onInitFlag()`.
- From Dusklight: `src/m_Do/m_Do_main.cpp` lines 190–363 and around 700–960 (init order, `aurora_dvd_open`, the event pump).
- Verify: `run.sh aurora-up` reaches M1.
- Risk: the game's `main` sets up reset data and `g_dComIfG_gameInfo.ct()` before the thread exists; keep that order.

**6.2 Frame loop and pacing**
- Files: `m_Do_main.cpp` (`aurora_update` / `aurora_begin_frame` / `aurora_end_frame` around each `main01` iteration), `JFramework/JFWDisplay.cpp` (`waitBlanking`/`waitForTick` replaced by a limiter, bypassed when `COS_UNCAPPED`), the game frame counter.
- From Dusklight: `JFWDisplay.cpp:371-420` (`Limiter`, `FRAME_PERIOD` = 1001/30000).
- Verify: M3 and M4 with no pending assertion or panic in the log; the capped run's `[cos] pacing` ratio after frame 1, over the first 120 frames, within 5 percent of 1.0 (the game picks its own rate: the logo scene asks for 60 Hz); `COS_UNCAPPED=1` reaches M4. Longer runs (600 frames capped and uncapped) belong to the boot milestones M5/M6, because they load logo-scene resources owned by later format steps (amended twice on 2026-10-03).
- Risk: the game calls `VIWaitForRetrace` inside `fapGm_Execute`, so GX must stay inside begin/end frame.

**6.3 Input injection**
- Files: `m_Do_controller_pad.cpp` / `JUTGamePad::read` under `TARGET_PC`, reading a `COS_INPUT` script (`frame buttons stickX stickY`).
- Verify: `COS_SMOKE=pad-echo` logs the scripted values; M10.

**6.4 Debug stage boot**
- `COS_BOOT_STAGE=sea:44:<point>:<layer>` hooks the logo scene's end (after M6). It calls new-file save initialisation and `dComIfGp_setNextStage`, then switches to PLAY, skipping title, name entry and the intro.
- Verify: M12.
- Risk: save-state expectations that the name scene normally sets up.

**6.5 Screenshots and render sanity** (decision H3)
- Either a patch to Aurora's FetchContent for frame readback, or `screencapture -l<windowid>`.
- The criterion is "PNG is not uniform" (pixel variance above a threshold), plus the `aurora_get_stats` counters.

**6.6 M13 controllable Outset** with the input script; recorded in the plan.

**6.7 Performance instrumentation**
- Per game frame:
  - game-thread CPU time (`CLOCK_THREAD_CPUTIME_ID`, which leaves out time spent blocked);
  - wall time;
  - the split between `mDoCPd_Read`, `mDoAud_Execute`, the `fapGm_Execute` logic, the `mDoGph_Painter` GX encode, and `aurora_end_frame`;
  - written as CSV to `COS_PERF`.
- A `COS_PERF_BUILD` variant: `-O2`, `-march=armv8-a` (generic ARMv8.0, like the A57), no Tracy.
- The game thread runs at QoS USER_INTERACTIVE so it lands on P-cores.

**6.8 Benchmarks and extrapolation**
- Scenarios: title (1,800 frames), Outset idle and Outset running (input script); uncapped; audio off and on; 5 runs each; report median, p95 and p99.
- Instructions per frame by differencing two runs of N and N+Δ frames with `/usr/bin/time -l`, as `scripts/bench_instructions.sh` already does.
- Two estimates:
  - `ms_A57 = instr_per_frame / (IPC × 1.02e9)` with IPC between 0.8 and 1.5;
  - `ms_M × 10–15`, the per-core ratio already used in `docs/SWITCH_IMPLEMENTATION_CHECKLIST.md`.
- Gate: p95 of the game thread ≤ 25 ms on the pessimistic estimate, leaving margin under 33.3 ms. Audio and Aurora's GX are budgeted separately because they run on other cores.

**6.9 Robustness**
- `COS_SMOKE=actor-sweep`: spawn every profile next to Link in Outset, run 30 frames, delete. Pass means no fault; a refused creation is fine.
- A 30-minute soak.
- An ASan build in `build/native-mac-asan` through M0–M13. ASan does not see inside JKR heaps, but catches stack, global and malloc errors.

---

## Phase 4 steps

Every edit goes under `#if TARGET_PC`. Dusklight's macros make `BE(T)` plain `T` off PC, so the GameCube code stays the same.

**4.0a Phase 4 inventory**
- Files: `native/tools/phase4_inventory.py`; a `COS_PHASE4_WARNINGS` CMake option adding `-Wint-to-pointer-cast -Wpointer-to-int-cast -Wint-to-void-pointer-cast -Wreturn-type -Wfortify-source`; `native/check/phase4_baseline.txt`.
- Verify: the script groups the 255 markers as listed in finding 1, and each later step lowers its group's count.

**4.0b Helpers**
- Port `endian.h`, `endian_gx.hpp`, `endian_ssystem.h` (the game's `cXyz`/`csXyz`) and `offset_ptr.h/.cpp` into `native/include/helpers/`, with provenance (CC0).
- Add a shim with the same name in `game/include/helpers/` (`BE(T)`=`T`, `OFFSET_PTR(T)`=`T*`) so the GameCube build still finds the include. Add it to the shadow check.
- New `cos_pc_tests` target: BE round trips for u16/s16/u32/f32/Vec, compound assignment, `OffsetPtr::setBase` (idempotent, negative offsets).
- Verify: `ninja -C build/native-mac cos_pc_tests && build/native-mac/cos_pc_tests` prints `ok`.

**4.0c Layout check**
- Files: `native/tools/layout_check.py`, `native/check/layout_headers.txt`, `native/check/layout_xfail.txt`.
- It generates `static_assert(offsetof(...) == 0xNN)` from the decomp's `/* 0xNN */` comments for the structs listed as disc-mapped.
- Verify: `ninja cos_layout_check` passes when the xfail list is honoured. Each format step removes its structs from xfail.
- Risk: comments missing or wrong; bitfields (endian-dependent bit order, so replace them with masks).

**4.0d Disc oracle**
- `native/tools/disc_manifest.py`, pure Python: GameCube FST, Yaz0, RARC, and the header fields of BMD/BDL/BCK/BTI/BFN/BMG/JPC/STB/dzs/dzb/AAF. It writes `build/native-mac/disc_manifest.json`, never committed.
- Verify: it runs on the ISO and its FST file count equals `DVDReadDir`'s count in `COS_SMOKE=disc-ls`.

**4.1 Clean-up and pointers kept in u32 fields**
- `J3DModel::mUserArea`/`setUserArea`, `J3DPacket`, `m_Do_ext.h` `mUserArea` and the calc user area become `uintptr_t` (Dusklight's `J3DModel.h:106`).
- `TVector::size` uses `sizeof(T*)`.
- Fix the 14 missing returns and the `f_op_msg_mng` strcpy overflows; mark the `J2DPrint` markers as harmless.
- Verify: inventory group A = 0 and return-type = 0; `ninja -k 0 centollos` has 0 errors; the census diff is empty; M0.

**4.2 Heaps**
- `CMemBlock` padded to 0x20 with `getBlock` based on `sizeof` and a `static_assert`.
- Go through the `0x10` literals in `JKRExpHeap.cpp` (30 of them) and separate header size from alignment.
- `JKRHeap::initArena` gets a PC path; `operator new` falls back to malloc during static initialisation (Dusklight's `fallback_alloc`).
- Heap sizes scale in `mDoMch_Create`; `fopAcM_entrySolidHeap` uses ×2; the `JKRAramPiece` callback takes `uintptr_t`.
- From Dusklight: `JKRExpHeap.h`, `JKRHeap.cpp:40-150,521-600`, `m_Do_machine.cpp:856-866`, `f_op_actor_mng.cpp:739`.
- Verify: `COS_SMOKE=heap` (10k random alloc/free in each exp heap, `check()`, `freeAll`) exits 0; M2.

**4.3 System font, ResTIMG and console**
- `JUTFont.h` ResFONT blocks become BE; `JUTResFont.cpp`; `JUTTexture.h` ResTIMG; `JUTPalette`; `JUTNameTab`; the compiled-in `JUTFontData_Ascfont_fix12` (big-endian bytes, alignment).
- Verify: `COS_SMOKE=font` (glyph widths, `getWidth('A')`) and a console line drawn; M3.

**4.4 Archives**
- `JKRArchive.h` gets BE fields and the `index` field with a `mFileData` side table (Dusklight's `JKAR_DATA`); `JKRMem/Aram/Dvd/CompArchive`, `ArchivePri/Pub`, the DVD/ARAM rippers.
- Dusklight's overlay feature is left out.
- Verify: `COS_SMOKE=arc-sweep` mounts every `.arc` under `/res`, fetches every entry, and the counts, names and sizes equal the manifest.

**4.5 Logo 2D.** `ResTIMG` image and palette offsets, `J2DPicture`. Verify: M5.

**4.6 BMG and disc fonts**
- `d_mesg.cpp` (`zel_00/01.bmg`), JMessage `data.h`/`resource.cpp` (Dusklight's JMessage), the BFN fonts.
- Verify: `COS_SMOKE=msg-sweep` decodes every message ID; the count equals the manifest.

**4.7 JParticle (JPAC1-00 / JEFFjpa1, not Dusklight's format)**
- `JPAEmitterLoader` blocks, the shape/field/key/dynamics structs, `JPATexture`.
- Verify: `COS_SMOKE=jpa-sweep` loads `common.jpc` and every `Pscene*.jpc` and runs each emitter's calculation for 30 frames.

**4.8 Remaining logo resources**
- Item table, `d_s_actor_data_mng` (2 markers), fmap, lod, error and menu resources, `d_resorce`, `d_com_inf_game` buffers.
- Verify: M6.

**4.9a Stage chunk table**
- `d_stage.h` chunk header as `OFFSET_PTR_RAW`; the relocation in `d_stage.cpp` (5 markers); `d_s_menu` (2).
- From Dusklight: `d_stage.h`, `offset_ptr.cpp`.
- Verify: `COS_SMOKE=stage-sweep` checks the chunk tags and counts of every dzs/dzr against the manifest.

**4.9b Actor records**
- ACTR, TRES, SCOB, TGOB, DOOR… get BE fields, and are swapped once where they are copied into `fopAcM_prm`.
- Verify: the sweep's actor names and parameters equal the manifest; M7.

**4.9c Rooms and files.** RTBL, FILI, MULT, SCLS, STAG, RPAT/RPPN, PATH/PPNT, CAMR/AROB/RARO, EVNT, 2DMA, SOND.

**4.9d Environment**
- LGHT/LGTV/Pale/Virt/EnvR/Colo (`d_kankyo` reads these every frame).
- Verify: the `sea_T` fog and colour values print within their ranges.

**4.10 Collision (dzb)**
- `cBgD_*` with BE fields and `OFFSET_PTR`; `cBgS::ConvDzb`; `dBgW` readers.
- From Dusklight: `d_bg_w.h`.
- Verify: `COS_SMOKE=dzb-sweep` fires a grid of downward `GroundCross` rays per dzb; every hit lies inside the bounding box; M8.

**4.11 J3D model data**
- `J3DModelLoader.h` (`OFFSET_PTR_V0`), `J3DModelLoader.cpp` (15 markers), `J3DMaterialFactory` (and `_v21`), shape, joint and cluster loaders.
- Vertex arrays and display lists stay big-endian.
- From Dusklight: `J3DGraphLoader/*` (nearly the same as the game).
- Verify: `COS_SMOKE=j3d-sweep` loads every BMD/BDL/BMT with the game's loader flags; joint, material and shape counts equal the manifest.

**4.12 J3D animation and runtime**
- `J3DAnimation.h` BE fields; `J3DAnmLoader` (the `mpData` index goes to a base pointer, Dusklight's `colorAddressBase`); key data swapped at load with a guard against swapping twice; skin and cluster; the `DrawBuffer`/`Shape`/`Joint`/`Sys` markers, with the real matrix count passed to `GXSETARRAY`.
- Verify: `COS_SMOKE=anm-sweep` evaluates every animation at the first, middle and last frame; all values finite, |scale| < 1e3, |translation| < 1e6.

**4.13 J2D BLO screens**
- `JSUInputStream` swaps in `readU16`/`readU32`; `J2DScreen`, `Pane`, `Picture`, `Window`, `TextBox` (the game's blo1).
- Verify: `COS_SMOKE=blo-sweep`; pane counts equal the manifest.

**4.14 Title actors.** Iterations of the loop through M7–M9: `d_a_title`, sea, ship, kankyo. One cause per commit.

**4.15 Save data and memory card** (decision H2)
- `d_save.h`, the `mDoMemCd` checksums (computed over big-endian bytes).
- Verify: new save, write, reload, compare equal; M10.

**4.16 Events, camera, paths, message flow and maps**
- `d_event_data.h`, `d_cam_param`, `d_msg_flow`, `d_path`, `d_map`.
- Verify: M12 and M13.

**4.17 JStudio and demos**
- `stb.h`/`stb-data(-parse)`, `fvb`/`ctb`/`functionvalue` (8 markers), `object-actor`, `d_demo`. For fvb, use Dusklight's copy-and-swap (`mSwappedData`).
- Verify: `COS_SMOKE=stb-sweep` runs every STB with null adaptors through all its frames; M14.

**4.18 THP.** `cos_thp_extras.h` as BE, with `d_a_movie_player` going through Aurora's THP. Deferred unless the loop hits it.

**4.19 Phase exit**
- Every remaining marker is either fixed or justified as harmless (debug back-chain walks, JOR).
- Layout xfail list empty; all sweeps pass; M0–M13 pass; the plan log is written.

---

## Phase 5 steps (audio; can start after M6; boot stays at `COS_AUDIO=off` until 5.5)

**5.1 Audio data formats**
- `JaiInit.aaf` (`JAIInitData`), BNK (`TOffset` as BE), WS, `JAISoundTable` (raw `*(u16*)` reads), sequence archive, stream table. (BAA/BST are JAudio2 formats; the game's equivalent is AAF.)
- From Dusklight: the approach in `JASBNKParser.h`/`JASWSParser.h`.
- Verify: `COS_SMOKE=audio-parse`; the bank, wave and sequence counts equal the manifest.

**5.2 JAudio and 64-bit**
- Callback IDs like `(u32)&assign_ch[i]` become `uintptr_t`.
- Addresses passed to the DSP or AI go through `OSCachedToPhysical`.
- `CH_BUF`, `FX_BUF`, `dsp_buf` and the copied `jdsp` ucode live in MEM1; fix `jdsp + 0x80000000`.
- Scale the 0x166800 audio heap.
- Verify: the build is clean; `audio-parse` still passes.

**5.3 Audio output over SDL3**
- `AIInitDMA` records the physical address and length. An SDL3 audio stream (32 kHz, s16 stereo) pulls each block, swaps it from big-endian, and calls the registered DMA callback with the OS lock held.
- `SDL_AUDIO_DRIVER=dummy` for headless runs; `COS_AUDIO_DUMP=x.wav`.
- From Dusklight: `DuskAudioSystem.cpp`.
- Verify: cos_sdk smoke test `ai-tone` (a sine wave in MEM1 goes through DMA; the dump equals the input).

**5.4 DSP backend: one option, decision H6**
- **(A) Dusklight-style HLE.**
  - Under `TARGET_PC`, `DspBoot` and `DsetupTable` only record. `DsyncFrame2` renders the subframes at once into `dsp_buf` with `DuskDsp` ported to the `DSPBuffer` fields: ADPCM4 (AFC, 9-byte frames), plus ADPCM2 and PCM8, which Dusklight crashes on; pitch, automixer and mixer volumes, IIR/FIR8, oscillator channels; no effects at first.
  - The 0xFF00 mail flow is replaced by direct `updateDSP` calls.
  - Verify: a synthetic-channel test (sine PCM16 in ARAM; the rendered RMS and frequency match).
- **(B) Dolphin's JAudio ucode HLE.**
  - Through the existing `dsp_hle_backend` adapter: guest memory is Aurora MEM1, ARAM is Aurora's buffer, mail and interrupt go to the `osdsp_task` handler. JAudio stays unchanged.
  - Verify: `COS_SMOKE=audio-boot` (ucode recognised, `DsetupTable` handshake returns, 600 DSP frames in 10 s).

**5.5 Sequences and sound effects**
- Verify: during the title, the WAV dump is above -40 dBFS RMS and the sequence tick counter advances. `COS_AUDIO` defaults to on from here.

**5.6 Streams**
- AFC through `StreamLib`/DirectPCM. Under (A), the PCM it decodes is host-endian, so mark those buffers or swap them.
- DTK hardware streaming only if `useHardStreaming` turns out to be true at runtime.
- Verify: a streamed BGM is audible in the dump.

**5.7 Movie audio (extMix).** Only if THP is reached.

**5.8 Fidelity and cost**
- Compare dump envelopes with the translated build's LLE DSP on the same scene (r ≥ 0.95).
- Audio-thread CPU per second; a TSan run.

---

## Decisions that need you

- **H1. Endianness approach.** I recommend Dusklight's hybrid: `BE<T>` in place by default, which is idempotent and safe when several actors share one resource. Swap at load (with a guard against double swapping) only for CPU-hot arrays given to generic code. Bulk GPU data stays big-endian for Aurora.
- **H2. Memory-card save format.** Big-endian `d_save` (GCI files compatible with Dolphin and real hardware) or host order.
- **H3. Screenshots.** Patch Aurora's FetchContent to add readback, or use macOS `screencapture`, which needs Screen Recording permission. Also: may Aurora be patched at all, and should the null backend be used if the Dawn package has it?
- **H4. Debug stage boot (`COS_BOOT_STAGE`) for M12/M13.** I recommend it first, with the real new-game flow (M14) after.
- **H5. MEM1 size and heap scaling.** For example `mem1Size` of 256 MiB and Dusklight's ×2 / ×20 multipliers, versus measured per-heap sizes.
- **H6. DSP backend.** (A) CC0, Dusklight-style, more work and more fidelity risk. (B) Dolphin's GPL HLE, already proven on this game and on the Switch, but adds the Dolphin dependency to the native build. Also whether Dolphin may be read as a reference for (A).
- **H7. Performance gate.** The IPC range and ratio used for the A57 estimate, whether the gate is p95 ≤ 25 ms for the game thread alone, and whether a Switch measurement in phase 7 is required before the gate counts.
- **H8. `-fno-strict-aliasing`.** The decompiled code type-puns heavily and Dusklight does not use the flag. Adding it costs a little performance but is safer.
- **H9. Prerequisites on your side:** `DevToolsSecurity` enabled for lldb, and whether the `.iso` or `.ciso` is the canonical disc (I'd suggest a SHA-1 check of the ISO on first use).

### Critical files for implementation
- ~/Documents/centollos/ref/dusklight/include/helpers/endian.h (and `offset_ptr.h`, `src/helpers/offset_ptr.cpp`)
- ~/Documents/centollos/game/include/JSystem/JKernel/JKRArchive.h (with `JKRExpHeap.h` and `ref/dusklight/libs/JSystem/include/JSystem/JKernel/JKRArchive.h`)
- ~/Documents/centollos/game/include/JSystem/J3DGraphLoader/J3DModelLoader.h (with `include/d/d_stage.h`, `include/d/d_bg_w.h`)
- ~/Documents/centollos/game/src/m_Do/m_Do_main.cpp (with `ref/dusklight/src/m_Do/m_Do_main.cpp`, `ref/dusklight/libs/JSystem/src/JFramework/JFWDisplay.cpp`)
- ~/Documents/centollos/game/src/JSystem/JAudio/JASDSPBuf.cpp (with `JASAudioThread.cpp`, `dspproc.c`, `ref/dusklight/src/dusk/audio/DuskDsp.cpp`, `runtime/host/src/dsp_hle_backend.cpp`)

## Decisions taken (2026-10-03)

Phase 3 is done (`centollos` links with nothing unresolved; `COS_SMOKE=static-init` passes, 502 of 502 profile slots).

- **H1:** Dusklight's hybrid. `BE<T>` in place by default, `OFFSET_PTR` for offsets relocated in place, swap at load (with a double-swap guard) only for CPU-hot arrays, GPU bulk data stays big-endian for Aurora.
- **H2:** saves stay big-endian (GCI compatible with Dolphin and real hardware).
- **H3:** screenshots are not a milestone criterion; Aurora's counters (`aurora_get_stats`) are. No Aurora patch, no screen-recording permission.
- **H4:** `COS_BOOT_STAGE` debug boot first (M12/M13), the real new-game flow (M14) after.
- **H5:** `mem1Size` 256 MiB and Dusklight's heap multipliers; measured sizes later if needed.
- **H6:** (B) reuse this repository's Dolphin JAudio ucode HLE adapter (`runtime/host/src/dsp_hle_backend.cpp`, GPLv2+, proven on this game and on the Switch); the repository is GPLv3.
- **H7:** gate = game-thread p95 ≤ 25 ms on the pessimistic A57 estimate; it only counts once confirmed by a Switch measurement in phase 7.
- **H8:** build with `-fno-strict-aliasing`.
- **H9:** disc = `/path/to/GZLE01.iso` (SHA-1 checked on first use, never committed). macOS developer mode is off, so lldb may prompt or fail: the built-in crash handler with symbolised backtraces is the primary tool; lldb is used only when it works non-interactively.
