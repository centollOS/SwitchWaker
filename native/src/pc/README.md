# native/src/pc: the host side of switchwaker

Everything here goes into the static library `cos_pc` (`native/cmake/executable.cmake` globs
`pc_*.cpp` recursively), linked into `switchwaker` and the link census. Public headers are in
`native/include/pc/`; `pc_internal.h` (here) is the state the files share, found by every file
through `cos_pc`'s include path. Files keep their `pc_` names wherever they are, so a file name in
a comment or a log still finds the file.

| Directory | What | Files |
|-----------|------|-------|
| `runtime/` | Start-up, the frame loop and the process: environment and exit (`pc_harness.cpp`, the API of `pc/pc_harness.h`), Aurora bring-up, frame pacing, disc check, crash handler, watchdog, stuck-event watch | `pc_main`, `pc_frame`, `pc_harness`, `pc_disc`, `pc_crash`, `pc_watchdog`, `pc_event_watch` |
| `features/` | What a player sees: the options menu and its settings file, controls, widescreen, dynamic resolution, GPU options, HD textures, the FPS overlay, the pipeline precompile (loading screen) | `pc_settings`, `pc_menu`, `pc_controls`, `pc_aspect`, `pc_dynres`, `pc_gpu_opts`, `pc_hd_textures`, `pc_overlay`, `pc_precompile` (+ `pc_precompile_gate.h`) |
| `harness/` | The test harness's core: `COS_SMOKE` tests before the SDK (`pc_smoke`), milestones and the game frame counter, `COS_INPUT` scripts, the debug stage boot and story presets, `COS_SHOT` screenshots, and `pc_heaps_created` (the M2 heap check, the `COS_SMOKE=heap` test, then the boot tests below) | `pc_smoke`, `pc_milestone`, `pc_input`, `pc_boot`, `pc_preset`, `pc_shot`, `pc_heap` |
| `harness/milestones/` | The probes behind the boot-loop milestones M8-M14 | `pc_title_stage`, `pc_title`, `pc_file_select`, `pc_new_game`, `pc_outset` |
| `harness/boot_tests/` | Host checks of the disc's data formats and of the save code, run from `pc_heaps_created` before the game starts, then exit (`COS_SMOKE=arc-sweep`, `j3d-sweep`, `save`...) | `pc_arc`, `pc_anm`, `pc_amp`, `pc_audio`, `pc_blo`, `pc_blur`, `pc_dzb`, `pc_font`, `pc_j3d`, `pc_jpa`, `pc_msg`, `pc_stage`, `pc_stb`, `pc_save` |
| `harness/smokes/` | One-scene smoke tests in a running game, mostly one per bug (`COS_SMOKE=chest`, `rope`, `sailing`...) | `pc_bgm_hop`, `pc_camera_invert`, `pc_chest`, `pc_evcam`, `pc_ky_procs`, `pc_npc_variants`, `pc_rope`, `pc_sailing`, `pc_shore` (also `COS_CAMERA`, `COS_ACTOR_LIST`), `pc_stage_hop`, `pc_telescope_demo`, `pc_title_audio`, `pc_wind_screen` |
| `harness/sweeps/` | Sweeps over everything of a kind in a running game, driven by `native/tools/*_sweep.py` and `gen_pipeline_cache.sh` | `pc_actor_sweep`, `pc_combat_sweep`, `pc_event_sweep`, `pc_fx_sweep`, `pc_item_sweep`, `pc_res_sweep`, `pc_save_sweep` |

`game_hooks/` (step G3 of `docs/GAME_CODE_ORGANIZATION.md`) holds the larger host changes of `game/`,
moved out of the decompiled files: `game/` keeps a one-line call under `TARGET_PC` and the original
code in the `#else`. The functions are declared in `native/include/pc/game_hooks.h`, except where a
game header already declares them (a member function, `mDoLib_loadDLTexImage`, the J3D host infos).
Header-level replacements and per-vertex code that must stay inline are headers in
`native/include/pc/game_hooks/`, included by the game file they replace a block of; the PC profile
list is an `.inc` there too, so that it stays defined in its REL unit (the link census leaves REL
units out).

| File | From `game/` |
|------|--------------|
| `pc_gpu_hooks` | the mist at a lower resolution (`COS_MIST_LOWRES`, default 1/4): `drawCloudShadow` (`d_kankyo_rain.cpp`) |
| `pc_logo_hooks` | the logo scene's milestones M5/M6 and the debug stage boot (`d_s_logo.cpp`) |
| `pc_jkr_heap`, `pc_jkr_hooks` | the host operator new/delete and allocation scopes (`JKRHeap.cpp`), the thread list lock (`JKRThread.cpp`), the archives' resource pointer table (`JKRArchivePri.cpp`) |
| `pc_j3d_transform` | the host bodies of the paired-single matrix functions (`J3DTransform.cpp`) |
| `pc_j3d_hooks` | the J3D loaders' host-order copies (`J3DModelLoader`, `J3DShapeFactory`, `J3DMaterialFactory`, `J3DClusterLoader`), `J3DTexture`'s texture objects (`J3DTevs.cpp`), `J3DTevBlock*::loadTexture` (`J3DMatBlock.cpp`) |
| `pc_m_do_lib` | `mDoLib_loadDLTexImage`, the textures of the static material display lists (`m_Do_lib.cpp`) |
| `pc_jfw_display` | `waitForTick`'s frame pacing (`JFWDisplay.cpp`) |
| `pc_c_bg_s` | the DZB tables' relocation in `cBgS::ConvDzb` (`c_bg_s.cpp`) |
| `pc_audio_hooks` | JaiInit.aaf's bank and wave-system tables (`JAIInitData.cpp`) |
| `pc_meter_hooks` | the widescreen HUD helpers (`d_meter.cpp`) |

Headers in `native/include/pc/game_hooks/`: `f_pc_profile_lst.h`/`.inc` (the typed profile list),
`j3d_transform.h` (inline `J3DPSMulMtxVec`), `j3d_shape.h` (the vertex array bases), `j3d.h` (the
loaders' copies, `J3DGCAddressBits`, the skinning's big-endian vertex access), `jkr_heap.h`,
`jkr_thread.h`, `jkr_exp_heap_poison.h` (COS_ASAN). The rope smoke's player hook moved next to its
test (`harness/smokes/pc_rope.cpp`); the decomp's `OSCalendarTime` field names over Aurora's struct
are `PcCalendarTime` in the SDK forwarder `native/include/sdk/dolphin/os/OS.h`.

The harness is compiled into every build, the release and the Switch build included: the game calls
it under `TARGET_PC` and everything is chosen at run time (`COS_SMOKE`, `COS_MILESTONE`...). Making
it optional would need a stub for each hook the game calls and moving out the runtime pieces that
live in harness files today (the game frame counter in `pc_milestone.cpp`, the allocation-failure
reporter that `pc_heaps_created` installs, the debug stage boot that the logo scene asks for).
