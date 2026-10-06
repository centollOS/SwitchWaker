# native/src/pc: the host side of centollos

Everything here goes into the static library `cos_pc` (`native/cmake/executable.cmake` globs
`pc_*.cpp` recursively), linked into `centollos` and the link census. Public headers are in
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

`game_hooks/` (step G3 of `docs/GAME_CODE_ORGANIZATION.md`) will hold the larger host changes of
`game/`, declared in `native/include/pc/game_hooks.h`.

The harness is compiled into every build, the release and the Switch build included: the game calls
it under `TARGET_PC` and everything is chosen at run time (`COS_SMOKE`, `COS_MILESTONE`...). Making
it optional would need a stub for each hook the game calls and moving out the runtime pieces that
live in harness files today (the game frame counter in `pc_milestone.cpp`, the allocation-failure
reporter that `pc_heaps_created` installs, the debug stage boot that the logo scene asks for).
