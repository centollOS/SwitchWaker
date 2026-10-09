# 60 fps by frame interpolation

Plan written 2026-10-09 from `dev` a21449f (game logic stays at 30 Hz; the screen gets a second,
interpolated paint per game frame). Line numbers are from that commit.

## Measured (Switch, handheld, CPU 1020 MHz)

Per game frame the game thread spends ~5-7 ms in logic (`fpcEx_Handler`), ~9 ms in the actor draw
pass (`fpcDw_Handler`: Draw methods, J3D `calc`/`viewCalc`, JPA calc, list building) and ~5-7 ms in
the painter (`cAPIGph_Painter` -> `mDoGph_Painter`). The GPU takes 10-13 ms a frame at 720p.
`COS_FPS60_TEST=1` (345b65e), which repeats the draw pass and the paint, costs ~16 ms per extra
frame: 54-60 presents/s at 1020 MHz, ~60 at 1224 MHz. The in-between frame must therefore repaint
the existing lists (~6-7 ms), never rerun the draw pass. Budget: 6 + 9 + 7 + 7 = ~28 of 33.3 ms.

## Facts the design rests on

- The lists outlive the paint: `fpcDw_Handler` (`f_pc_draw.cpp:36`) resets them through
  `cAPIGph_BeforeOfDraw` -> `mDoGph_BeforeOfDraw` -> `dScnPly_BeforeOfPaint` -> `dComIfGd_reset` ->
  `dDlst_list_c::reset` (`d_drawlist.cpp:2035`) at the start of the next draw pass. A second
  `cAPIGph_Painter()` after `callBack2` repaints them without the draw pass. JPA calc
  (`dComIfGp_particle_calc3D/2D`) is in the draw pass (`d_s_play.cpp:365-375`), not in the paint.
- J3D: `J3DModel::viewCalc` (`J3DModel.cpp:797`) swaps `mpDrawMtxBuf[2]`/`mpNrmMtxBuf[2]` and writes
  view-space draw matrices (`calcDrawMtx`, `MTXConcat(j3dSys.getViewMtx(), anm)`). Packets keep
  pointers (`mpDrawMtxBuf[1]`, `&mCurrentViewNo`, base matrix `&j3dSys.mViewMtx` or
  `&mViewBaseMtx`); `J3DShape::setArrayAndBindPipeline` (`J3DShape.cpp:230-234`) reads them at
  paint time. buf[0] is not a usable "previous": shadow models `viewCalc` twice (once in the
  painter, `d_drawlist.cpp:1174`), culled models leave it stale, and Aurora's FIFO worker reads game
  memory late (`fifo.cpp:27`, `command_processor.cpp:251-270`): interpolated matrices go to
  scratch memory kept until the FIFO is drained, never edited in place.
- Camera: `camera_draw` (`d_camera.cpp:8042`, draw pass) builds view and projection; the painter
  reads `camera->view` again (`m_Do_graphic.cpp:1647-1755`). J3D matrices already hold the view, but
  JPA, the sea, weather, ConcatView/NoUseDrawMtx/CPU-skinned models apply it at paint time and
  grass/tree/flower/wood pre-concatenate it: the camera needs its own interpolation.

## Paint side effects (the extra paint must be pure)

| Mutator | Where | In the extra paint |
|---|---|---|
| fade, monotone | `mDoGph_gInf_c::calcFade` (`:190`, called `:719`, `:1813`), `calcMonotone` (`:640`) | save, half step, restore |
| wipe | `dDlst_list_c::calcWipe` (`d_drawlist.cpp:2135`) advances and appends `mWipeDlst` | draw it, no advance, no append |
| picto box capture | `mCaptureStep` (`m_Do_graphic.cpp:1805-1964`) | normal paint only |
| JUTFader | `JFWDisplay::endGX` (`JFWDisplay.cpp:239-241`) | no fader |
| sea scroll | `daSea_packet_c::draw` `mAnimCounter += 1` (`d_a_sea.cpp:715`) | guarded |
| weather | sun `field_0x3c += 2` (`d_kankyo_rain.cpp:3354`), `rot += 1.3f` (`:4318`), stars `rot++` (`:5106`), `cM_rndF` (`:4213`) | guarded; RNG seeds saved and restored |
| mDoGph heap flip | `mDoGph_gInf_c::free()` (`:159`, `:1594`) | kept in both paints (the sea's 50 KB per paint) |

A purity checker (`COS_PAINT_PURITY`: hash `.data`/`.bss` and the game heaps around the extra paint,
GX FIFO and mDoGph heaps excluded) finds the actor packets not audited by hand.

## Discontinuities

Whole frame (extra paint at t = 1): camera reset (`dCamera_c::Reset`, `d_camera.cpp:7474`), demo
camera start/stop, eye or center jumping over ~400 units or a large fovy/bank change, room change
(`dComIfGp_roomControl_getStayNo`), scene create, wipe or overlap, menu or pause toggled.
Per model: a host table keyed by `J3DModel*` plus a generation written in `J3DModel::initialize`
(`:30`), a reused pointer caught; interpolate only a model snapshotted in the previous draw frame
with the same draw-matrix count, not moved past a threshold nor turned over ~45 degrees.

## Design

- Order per iteration: paint A (lists L_N, t = 1), **present**, logic N+1, draw pass N+1, paint B
  (lists L_N+1 blended with N, t = 0.5), present. No latency over 30 fps. The split present moves
  right after paint A (today A is presented after logic and the draw pass: presents bunch ~21/6 ms).
  Each paint waits one retrace (`pc_jfw_display.cpp`).
- Capture at the end of the draw pass (`mDoGph_AfterOfDraw`): camera eye/center/up/bank/fovy and
  V, P for N and N+1; every model `viewCalc`'d this draw frame (a hook in `viewCalc`, ignoring calls
  from the painter): its draw and normal matrices as "cur", the old "cur" as "prev" (~0.3 ms).
- Camera (step B): an Aurora patch premultiplies perspective-pass position matrices by
  C = V_t * V_cur^-1 and normal matrices by its rotation in `copy_xf_data` (`regs.cpp:983`),
  corrects lights (`:1056`) and swaps in P_t (`:903`) while a game flag marks paint B's 3D part;
  orthographic passes (shadow image, fullscreen quads, 2D) stay as they are. One hook covers J3D,
  grass, sea, JPA, weather and unknown packets. V_t from `mDoMtx_lookAt` on the lerped eye/center.
- Objects (step C): after `dComIfGd_imageDrawShadow` (`m_Do_graphic.cpp:1629`), per valid model
  D = lerp(V_cur V_prev^-1 D_prev, D_cur) into a scratch arena; `mpDrawMtxBuf[1][v]` (and the
  normal buffer) point at it for paint B and are restored after; the arena is recycled after the
  next FIFO drain.
- Pacing and budget: dynres per present aiming at 16.7 ms; paint B skipped when the iteration
  already took more than ~26 ms.

## Steps

| Step | Content | Estimate |
|---|---|---|
| A | paint B = `cAPIGph_Painter` only (no draw pass), the guards above, present split after paint A, purity checker; measure CPU (Mac, Switch) | 2-3 days |
| B | camera capture, Aurora view-delta patch, cut detection | 3-4 days |
| C | model snapshot table, delta lerp, pointer substitution, rotation/teleport fallbacks | 3-5 days |
| D | JPA previous positions, half-step fade/wipe/sea scroll, ConcatView / `mViewBaseMtx`, motion-blur alpha | 1-2 weeks |
| E | menu option (default off), auto-off when docked or dynres at its floor, 30 fps when B keeps being skipped | 1-2 days |

Risks: the Switch CPU margin (~5 ms); paint-time mutators in unaudited actor packets; FIFO
lifetimes; light-space and other perspective passes that are not the world view (wind view
`m_Do_graphic.cpp:1727`, shadow models); `GXPeekZ` after the present moves; culling/LOD of frame N+1
showing at the edges in paint B.

## Step A results (2026-10-09, Mac)

`COS_FPS60_TEST=1` ([dev]) now runs the split frame of the design without a second draw pass.
`fpcM_Management` (`f_pc_manager.cpp`): paint A (`cAPIGph_Painter`, the previous draw pass's
lists), then `pc_frame_split()`, which ends the Aurora frame (the FPS overlay counted as a present,
`captureFrameEnd` for a picto box copy of paint A) and begins the next; logic and the draw pass;
after `callBack2` paint B = `MtxInit` + `cAPIGph_Painter()` between `pc_paint_extra_begin/end`, the
`cM_rnd` seeds saved and restored around it (`cM_pcGetRnd`/`cM_pcSetRnd`, `c_math.cpp`); the
normal `pc_frame_end` presents paint B. The split is decided by paint A's wait
(`pc_frame_wait_retraces`, `pc_jfw_display.cpp`): only a game frame of two retraces, the mode on
and the options menu closed; paint B always waits one retrace. Frames of another length (the logo,
loading at 60), the menu open and the mode off run exactly as before. The crude test's second
`fpcDw_Handler` is gone. Once per game frame as before: Aurora's event pump, `pc_dynres_frame_begin`
(the dynamic resolution level holds for both presents), the options menu, `COS_SHOT` (it saves
paint B).

### Guards (all `#if TARGET_PC`, `pc_paint_is_extra()`)

| Mutator | Where | Paint B |
|---|---|---|
| fade | `mDoGph_gInf_c::calcFade` (`m_Do_graphic.cpp:190`, called `:1813`; the `:719` call is JPN/demo only) | `mFadeRate` not advanced; the quad drawn at paint A's rate (the brightness branch as usual) |
| monotone | `calcMonotone` (`:266`, called from `drawDepth` `:640`) | returns at once |
| picto box capture | `mCaptureStep == 1` (`:1817`), steps 3/4/5->6/6 (`:1835-1960`) | none of the steps; `mDoGph_screenCaptureDraw` (the photo on screen) still drawn |
| wipe | `dDlst_list_c::calcWipe` (`d_drawlist.cpp:2135`) | not advanced; the scroll set from the current rate; `mWipeDlst` appended only when the 2D list lacks it (`dDlst_list_c::has2DXlu`), so paint A after paint B (same lists) does not add it twice |
| JUTFader | `JFWDisplay::endGX` (`JFWDisplay.cpp:240`) | `draw()` without `control()` (drawn unless `WaitIn`) |
| sea scroll | `daSea_packet_c::draw` `mAnimCounter` (`d_a_sea.cpp:715`) | not advanced |
| sun | `field_0x3c += 2` (`d_kankyo_rain.cpp:3354`) | not advanced |
| poison / star rotation | `rot += 1.3f` (`:4318`), `rot++` (`:5106`) | not advanced |
| cloud shadow rotation | `drawCloudShadow` `rot -= 1.5f` (`:5495`) **found by the checker** | not advanced |
| cloud sway | `drawVrkumo` `howa_loop_cnt +=` (`:5748`) **found by the checker** | not advanced |
| lights | `dKy_setLight` (`d_kankyo.cpp:2479-2560`): player light eased (`cLib_addCalc`), flicker targets (statics + `cM_rndF`), eflight **found by the checker** | the two update blocks skipped; the lights loaded as paint A left them |
| rope/line arrays | `mDoExt_3DlineMat0_c::draw`, `mDoExt_3DlineMat1_c::draw` `mCurArr ^= 1` (`m_Do_ext.cpp:2023`, `:2310`) **found by the checker** | no flip. Without it paint A drew the array of the draw pass before the last one (ropes, bridges, chains one frame late on every other present) |
| RNG | `cM_rnd` seeds `r0/r1/r2` (`c_math.cpp:167`; rain `cM_rndF` `d_kankyo_rain.cpp:4213`) | saved before paint B, restored after |
| mDoGph heap flip | `mDoGph_gInf_c::free()` (`:159`, `:1594`) | kept in both paints (only the sea allocates there, per paint) |

### Purity checker (`COS_PAINT_PURITY`, `native/src/pc/harness/pc_paint_purity.cpp`)

macOS only. `=1`: the game's writable globals, i.e. the `__DATA,__data/__bss/__common` symbols whose
object file comes from `game/src/` (the executable's debug map: N_OSO/N_STSYM/N_GSYM stabs; 4715
symbols, 1.6 MiB; Aurora, Dawn, ImGui, the SDK and the harness left out by construction), copied
before paint B and compared after it in 64-byte blocks, every changed symbol named (demangled, with
its file). `=2`: also the JKR root heap (230 MiB, without the two mDoGph heaps and the audio heap):
a change there is named by its heap, its allocation (expanded heaps), the process (`g_profile_*`
through `base_process_class::mpProf`) or class (primary vptr) it is in, and who points to the block
or to its solid heap. `=1` copies and compares 1.6 MiB per paint B (lost in the Mac's noise); `=2` adds ~19 ms per game
frame on the Mac (counted as logic in the perf line). Helpers:
`COS_PAINT_PURITY_REPEAT=1` paints B twice and reports only what the second paint changes (plain
writes of the same value drop out, accumulating state stays); `COS_PAINT_PURITY_TRAP=1` sets an
arm64 hardware watchpoint on the game thread for the 8 bytes of each new change and logs the
backtrace of its first write (that found the line arrays, the cloud rotations and the JPA axes);
`COS_PAINT_PURITY_IGNORE=a,b`.

Runs (600 frames each, `--env COS_FPS60_TEST=1 --env COS_PAINT_PURITY=2`, with and without
`_REPEAT`): `sea:44:206` (Outset), `Siren:0:0` (Tower of the Gods), `kindan:0:0` (Forbidden Woods),
`sea:13:0` (Dragon Roost), `M_NewD2:0:0` (Dragon Roost Cavern). With the guards above, what paint
B still changes the second time (`_REPEAT`), all harmless:

- `mDoGph_gInf_c::mCurrentHeap` and the mDoGph solid heaps' heads, `l_cloth` (the sea packet's
  `m_draw_vtx` and texture objects): the heap flip, kept by design.
- `GXTexObj::texObjId` (Aurora's id per `GXInitTexObj`, `GXTexObj` +0x30) in
  `mDoGph_gInf_c::mFrameBufferTexObj/mZbufferTexObj`, `clear_z_tobj` (`JFWDisplay.cpp`), the fonts'
  `JUTResFont::mTexObj`: host ids, rewritten by every paint.
- `JPADraw::cb.mDrawMtxPtr`, `dComIfG_play_c::mCurrentGrafPort`: pointers to the painter's stack
  (different stack depth in the repeat), rewritten before use.
- `JFWDisplay` +0x30..0x3c (tick fields, `mCombinationRatio`): `beginRender`/`endRender` timing,
  read only by `JUTProcBar`.
- JPA particles' `JPADrawParams::mAxis` (`JPADrawExecDirectional`, `JPADrawExecRotDirectionalCross`,
  `JPADrawExecStripe`): re-orthogonalised against the direction at each draw, a projection, the
  second time equal but for the last bit (0.99999994 -> 1.0).
- Other threads, at any time: JAudio (`JASystem::*`, `JAInter::*`, `sSeqTickCount`, `cmd_once`),
  the disc (`JKRDvdRipper`, `JKRDvdAramRipper`, `JKRAram*`, `JKRFileLoader::sVolumeList`,
  `mDoDvdThd_*`, `m_Do_DVDError` `Alarm`), `JUTVideo::preRetraceProc` (the retraces of paint B's
  wait).

Without `_REPEAT` paint B also writes what the next paint A writes again before any logic runs
(scratch matrices `mDoMtx_stack_c::now`, `j3dSys`, `J3DShapeMtx*` statics, `dDlst_shadow*` colours,
`drawDepth`'s `l_tevColor0`, `g_env_light.mLightDir`, `dMeter`/`dMap_c` texture objects, J2D
texture objects, `dComIfG_play_c::mCurrentWindow/View/Viewport`, J3D shape/texture state in the
model heaps), and one-shot work that would happen in the next paint A anyway: `dSnap_packet::Judge`
(the photo judgement), `dPa_ripplePcallBack::draw`'s `setInvisibleParticleFlag`.

### Mac numbers (M-series, `COS_PERF_EVERY=300`, 1200 frames, frames 301-1200)

| | game thread ms/frame | logic | painter | painter2 (paint B) | split | presents/s | retraces/s | pacing ratio |
|---|---|---|---|---|---|---|---|---|
| Outset `sea:44:206`, off | 3.50-3.60 | 2.0-2.2 | 1.0-1.1 | - | - | 30 | 60.0 | 1.0002 |
| Outset, `COS_FPS60_TEST=1` | 4.9-5.1 | 1.5-1.6 | 0.54-0.63 | 0.53-0.62 | 1.0-1.2 | 59.9 | 59.9 | 1.0001 |
| Dragon Roost `sea:13:0`, off | 3.8-4.0 | 2.1-2.2 | 1.3-1.4 | - | - | 30 | 60.0 | 1.0001 |
| Dragon Roost, `COS_FPS60_TEST=1` | 5.4-5.8 | 1.4-1.5 | 0.57-0.59 | 0.56-0.58 | 1.3-1.6 | 59.9 | 59.9 | 1.0001 |

Logic stays at 30 game frames a second (30.0 fps, 59.9 retraces/s, pacing ratio ~1.0); paint B
costs what paint A does (~0.6 ms on the Mac); the split present (`aurora_end_frame` +
`aurora_begin_frame` in the middle of the frame) ~1-1.6 ms. On the Switch the painter was ~5-7 ms,
so paint B should cost about that instead of the ~16 ms of the crude test (to be measured on the
console). `COS_SHOT` images with the mode on (paint B: Outset frames 450/600, Tower of the Gods
frame 750 with the full HUD, map and ripples) match the mode off.

Verified: Mac build; full `native/tools/regress.sh` passes (the mode off); with
`COS_FPS60_TEST=1` the picto-box targets (I8, RGB565 and the Windfall subject check, result 97),
file-select at 16:10 and outset-control pass; Switch NRO `scripts/switch/build_native.sh
--runtime-assets` builds. Not yet measured on the console (the next step: paint B's cost there at
1020 MHz, Outset / Dragon Roost / Tower of the Gods).

## Async end of frame (2026-10-09, Aurora patch 0018, `COS_ASYNC_END_FRAME`)

Console measurement of step A: Aurora's GX FIFO worker is slower than the painter, and
`aurora::end_frame` began with `gx::fifo::drain()`, so the game thread waited ~9 ms in the
`aurora_end_frame` after paint B and ~5-6 ms at the split (Outset 55, Dragon Roost 52 presents/s),
while the frame without those waits needs ~22 of 33.3 ms.

Aurora's frame in Thread mode: the game thread writes GX commands into one growing buffer
(`lib/gx/fifo.cpp`) and publishes them per draw; the worker translates them
(`command_processor.cpp`) and records into the current gfx frame packet (`g_recorder`,
`lib/gfx/recording.cpp`), reading game memory late (vertex arrays, display lists, textures,
indexed matrices). `gfx::begin_frame` (frame and staging slots, `begin_recording`) and the frame's
end (`fifo::end_frame`'s draw cache reset, `gx::texture::end_frame`: texture caches, replacement
streaming, invalidations; `gfx::finish`: the last pass; `gfx::end_frame`: the frame to the render
worker with the present callback) all touch the worker's state, hence the drain.

Patch 0018 (`native/patches/aurora/0018-async-end-frame.patch`, off by default in Aurora):
`aurora_set_async_end_frame(true)` makes `aurora_end_frame` freeze ImGui's draw data (game thread),
publish the frame and queue a **frame job** at the stream's end position: the draw cache reset,
`texture::end_frame`, `gfx::finish` and the present (`present_frame`), run by the worker when it
has processed the stream up to there. `aurora_begin_frame` keeps its window/surface checks and
`imgui::new_frame` on the game thread and queues `gfx::begin_frame` as a job at the same position.
The worker runs a due job before any further command and processes no command past a pending job,
so frames stay strictly ordered (it reads `sPublished` before the job queue; a frame's begin job is
queued before any of its commands is published). `aurora_end_frame` returns at once unless two
ended frames are not done (one frame in flight, `kMaxFramesInFlight`). The command buffer is reset
whenever the worker has caught up (any drain, `GXDrawDone`, an end of frame finding it idle; never
waiting on the buffer lock, which the worker holds while translating a range), and drained past
64 MiB.

What stays synchronous, and why:

- `drain()` (`GXDrawDone`, `AuroraGXSync`, the custom draw/pass API): unchanged. The draw pass's
  `GXDrawDone` (`mDoGph_AfterOfDraw` -> `JFWDisplay::endFrame`, every game frame) is the game's own
  wait for the GPU and now the only per-frame wait: paint A is translated while the draw pass runs,
  as on the console (paint processed by the GP during logic and draw, synchronised there).
- `aurora_frame_sync()` / `fifo::sync()` waits for every queued frame job: readbacks after a frame
  call it first (`pc_capture.cpp` `readCopy`: the picto box and the EFB peek, whose copy texture is
  looked up in the worker's `g_gxState`; `pc_shot.cpp`: `COS_SHOT`, the debug-server shot, the
  menu screenshot, the telescope probe), so they still run on the render worker behind their frame.
  `gfx::gpu_synchronize` syncs too, so `release_surface`, `refresh_surface` and `resize_swapchain`
  (which replace the EFB, swapchain and surface) never race a frame the worker is ending; so do
  `gx::update` before a new viewport policy, `set_async_end_frame` and `fifo::shutdown`. It is a
  no-op on the FIFO and render workers (no self-wait; `release_surface` runs on the render worker).
- Not affected: `GXPeekZ` reads the latest finished depth snapshot (`depth_peek`, mutex), now up
  to one frame older; display list building is game-thread only; `AuroraSetContentScale` and the
  census request go through the FIFO or a mutex; the HD texture API locks its registry and posts
  cache clears through atomics.

Races (reasoned; nothing new on the game thread's side): everything moved to the worker was the
worker's state already, or is changed by the game thread only after a sync (webgpu frame buffer,
surface configuration, viewport policy); `g_frameBegun` (aurora.cpp) is written by whichever thread
begins/ends frames, the switch between them synced. The new overlap with game memory: paint B is
translated while the game thread runs `pc_frame_end`, the pace wait, `pc_frame_begin`, paint A
and the logic (until the split's bounded wait / the draw pass's `GXDrawDone`). Paint A paints the
same lists and its paint-time writes are those the purity checker lists (by value into the FIFO,
the double-buffered mDoGph heaps and J3D draw matrices, host texture ids), so a worker reading
paint B's memory late sees the same data; the 30 fps mode already overlaps the paint with the logic
this way. Risk: an unaudited packet that rewrites memory by pointer at paint time (a CPU-written
texture or vertex array that is not double-buffered) could show its paint A content in paint B.

Default: on with `COS_FPS60_TEST=1`, off otherwise (the 30 fps frame already waits in `GXDrawDone`,
so it gains little); `COS_ASYNC_END_FRAME=1/0` forces it. With `COS_FPS60_TEST` the perf line's
`fps60:` part now also gives `drawdone` (`cAPIGph_AfterOfDraw`, part of the logic) and
`end_frame calls` (both `aurora_end_frame` calls, and the split's) with the mode.

Mac numbers (M-series, `COS_FPS60_TEST=1 COS_PERF_EVERY=300`, 1200 frames, frames 301-1200, per
game frame; "end_frame calls" = both `aurora_end_frame` calls, the split's in brackets):

| | game thread ms | logic | painter | painter2 | split | drawdone | end_frame calls | presents/s |
|---|---|---|---|---|---|---|---|---|
| Outset `sea:44:206`, sync | 3.25-3.48 | 1.30-1.32 | 0.40-0.44 | 0.41-0.43 | 0.15-0.33 | 0.02 | 0.95-1.27 (0.14-0.31) | 59.9 |
| Outset, async | 2.22-2.37 | 1.30-1.35 | 0.41-0.46 | 0.40-0.45 | 0.01-0.02 | 0.02-0.03 | 0.00-0.01 (0.00) | 59.9 |
| Dragon Roost `sea:13:0`, sync | 4.07-4.37 | 1.32-1.40 | 0.51-0.52 | 0.50-0.52 | 0.45-0.63 | 0.02 | 1.56-1.93 (0.44-0.62) | 59.9 |
| Dragon Roost, async | 2.35-2.37 | 1.29-1.32 | 0.47-0.50 | 0.47-0.50 | 0.01 | 0.02-0.03 | 0.00 (0.00) | 59.9 |

The Mac's worker keeps up, so `drawdone` stays ~0 there; on the Switch the split's wait should
partly move to `drawdone` (paint A translated during the draw pass) and paint B's ~9 ms should
overlap the next frame's pace wait and paint A. Uncapped (`--uncapped`, Tower of the Gods):
85 instead of 60 game frames a second.

Verified: Mac build; full `native/tools/regress.sh` passes with the default (off without
`COS_FPS60_TEST`) and with `COS_ASYNC_END_FRAME=1` forced for every run; with `COS_FPS60_TEST=1`
(async on) the three picto-box targets pass (I8, RGB565, Windfall); `COS_SHOT` images with the mode
on and off are byte-identical (Outset frames 450/600, Tower of the Gods 750); the ASan variant
(`native/CMakeLists.txt`) runs Outset 1200 frames and the picto box with no report; Switch NRO
`scripts/switch/build_native.sh --runtime-assets` builds (the Switch patches apply on top of 0018
unchanged). A ThreadSanitizer build of the whole game links (`-Wl,-no_compact_unwind`) but aborts
at start-up in the TSan allocator (a string freed across the CLT clang / system libc++ boundary in
`PADInit`), so the races above are reasoned, not tool-checked. Not yet measured on the console.

## Step A on the console (2026-10-09, handheld, CPU 1020 MHz, `COS_FPS60_TEST=1`)

| Spot | presents/s | game thread ms per game frame | paint B ms | GPU ms |
|---|---|---|---|---|
| Outset `sea:44` | 59.9 | 23.1 | 5.4 | 11.4 |
| Forbidden Woods `kindan:0` | 59.9 | 20.3 | 5.0 | 12.4 |
| Dragon Roost `sea:13` | 59.9 | 25.9 | 6.4 | 13.2 |
| Tower of the Gods `Siren:0` | 59.9 | 22.2 | 4.4 | 14.4 |

Before the async end of frame (383300a) the game thread waited for Aurora's GX worker at each present
(~5-6 ms at the split, ~9 ms at the end: 47-56 presents/s outside the forest); now the split costs
0.2 ms and the wait is `GXDrawDone` in the draw pass (2.6-4.1 ms), the game's own GPU sync. Margin
7-13 ms per game frame at the stock clock. Next: step B (camera).

## Step B: the camera of paint B (2026-10-10, Aurora patch 0019)

With `COS_FPS60_TEST=1` paint B now shows the scene from the camera halfway (t = 0.5) between the
camera of the draw pass before (the one paint A just showed) and this frame's; no extra latency.

### Design as built

- Capture (`native/src/pc/game_hooks/pc_fps60_camera.cpp`): `camera_draw` (`d_camera.cpp`, draw pass)
  calls `pc_fps60_camera_drawn(&view)` right after it builds the view: eye, center, up, bank, fovy,
  aspect, near, far, `mViewMtx`, `mProjMtx`, and for the cut checks the room (`getStayNo`), the stage
  name, `dComIfGp_event_runCheck`, the demo mode, `dDlst_list_c::mWipe`, `fopOvlpM_IsDoingReq`,
  `dMenu_flag` and whether `dCamera_c::Reset` ran since the last capture (`pc_fps60_camera_reset`,
  in `Reset()`, which the other two overloads call). Two snapshots are kept: N and N+1, each with
  its game frame number.
- Paint B (`mDoGph_Painter`): right after `GXSetProjection(camera->view.mProjMtx, GX_PERSPECTIVE)`,
  `pc_fps60_view_begin(&camera->view)`; after the camera block (before `pc_dynres_3d_end`, i.e.
  before the 2D), `pc_fps60_view_end()`. Begin (paint B only) computes V_t = `mDoMtx_lookAt`(lerped
  eye, center, up; bank lerped as an s16 angle), P_t = `C_MTXPerspective`(lerped fovy, aspect, near,
  far) (P_cur itself when they did not change) and C = V_t * V_cur^-1, and calls
  `AuroraSetViewDelta(C, P_cur, P_t)`; end calls `AuroraSetViewDelta(NULL, ...)`.
- Aurora patch 0019 (`native/patches/aurora/0019-view-delta.patch`): `GX_AURORA_SET_VIEW_DELTA`
  (0x0051) is a command **in the GX stream**, decoded by the FIFO worker into
  `GXState::viewDelta`, so it applies exactly to the draws written between begin and end however
  late the worker translates them (patch 0018). It is applied where every path meets, when the
  worker builds a draw's uniform (`fill_uniform`, `shader_info.cpp`), not when matrices are loaded
  (`copy_xf_data`) as first planned: a matrix can be loaded before the projection that decides
  whether it is a world draw, and indexed loads, immediate loads and display lists all end in the
  same `pnMtx` array. For a draw whose projection is perspective **and equal to P_cur** (six
  floats, GXSetProjection's encoding): every position matrix is premultiplied by C, every normal
  matrix by C's rotation, light positions by C and light directions by its rotation (GX lights are
  given in view space: lighting stays the same on the moved geometry), and the projection is
  replaced by P_t. Orthographic draws (2D, full-screen quads of `drawAlphaBuffer`, `drawDepth`,
  `motionBlure`, the photo) and any other perspective projection are untouched. Toggling marks the
  uniform dirty; the end of every frame (`clear_draw_cache`) turns it off. The P_cur match makes
  the hook safe against perspective passes that are not the world camera even inside the window
  (none found: `d_menu_capture`, `d_ovlp_fade2`, `d_s_name` set their own projections in other
  passes), the 3D section bracket keeps it away from the 2D list's 3D (`drawOpaList2D`).
- What moves: everything drawn with the world projection in the 3D section. J3D (view-space draw
  matrices baked with V_cur in the draw pass), packets that concatenate `j3dSys`'s view or
  `camera->view` at paint time (JPA with `jpaDrawInfo`, the sea, weather, grass/tree/flower/wood,
  ConcatView/NoUseDrawMtx models, shadows, the sky vrbox drawn around the eye of N+1: off-centre by
  half a frame of camera travel, invisible at its radius).
- Not moved / not interpolated: objects themselves (step C): a model is drawn where frame N+1 put
  it, seen from the camera at t. Texture matrices (texgens take model-space input, so projected
  textures stay on their surfaces; view-dependent env maps and the specular half-angle stay
  V_cur's). Things the CPU projected to the screen and draws in 2D or orthographic (lens flare and
  sun glare, HUD markers anchored to 3D positions) stay at N+1's positions. Culling and LOD are
  N+1's (geometry culled for N+1 may be missing at the screen edge). `dPa_control_c::mWindViewMatrix`
  (the `particle_wind` group's view) is never written by the game (zero): not a world-camera draw.

### Cuts (paint B at t = 1, as in step A)

Checked at paint B's begin: no camera drawn in this frame's draw pass or the one before ("no camera
drawn this frame" e.g. in the pause menu, where the camera process is not drawn: the world is
still), another camera/view, `mViewMtx`/`mProjMtx` changed since the capture, stage or room change,
a `dCamera_c::Reset` starting (a reset in both frames, a camera driven by resets every frame, is
not a cut: the jump checks still apply), event or demo start/stop, a wipe or overlap in either
frame, `dMenu_flag` toggled, eye or center moving over 400 units, fovy over 10 degrees, bank over
0x1000 (~22 degrees), the forward axes over 45 degrees apart. Logged as `[cos] fps60 camera: frame
N: paint B not interpolated (<reason>; <n> such paint(s) since the last line)` when the reason
changes or every 2 s; counted as `camera blended <n> cuts <n>` in the perf line.

### Transitions: paint B dropped (console report of step A)

On the console, step A flickered right after Start on the title screen and when warping. Found on
the Mac with `COS_SHOT_PAINT_A=1` around the transitions (`new-game`, the options menu's travel):
during the fade out of a scene change paint B was black (mean 0.2) while the paints A around it
faded 118 -> 9 (24 frames in `new-game`, 46 in the travel run). Paint B is now **not done at all**
in those frames: `pc_frame_split` asks `pc_fps60_paint_b_allowed()` (every frame, after the logic)
and presents the frame once, as at 30 fps, while a node (scene/room scene) request is queued
(`pc_fpcNdRq_pending`, `f_pc_node_req.cpp`), an overlap runs (`fopOvlpM_IsDoingReq`/`IsPeek`),
a wipe, a screen fade (`mDoGph_gInf_c::isFade`), a JUTFader fade (FadeIn/FadeOut), the monotone
rate changes, or the stage name changes, and for 3 frames after. Pacing: paint A waits one retrace
only after a frame that ended with paint B; after a dropped paint B it waits two, so the game
frame keeps two retraces either way. Logged (`[cos] fps60: frame N: presented once (<reason>)`,
`paint B again (after <reason>)`), counted as `paint B dropped <n>`; `COS_FPS60_GATE=0` restores
step A's behaviour (the test above). With the gate: 0 suspicious paints B out of 144 split frames
around `new-game`'s transitions and 82 around the travel (paint B differing from both neighbouring
paints A by more than they differ from each other + 4 levels).

### Verification (Mac)

- Delta correctness, sailing smoke (`--preset sailing`, frame 1200, `COS_SHOT_PAINT_A=1`): on a world
  region without the boat (clouds, horizon, sea at the left third), mean abs difference / best
  shift: `COS_FPS60_CAMERA_T=0` paint B vs its paint A (camera N) 0.46 (vs 3.41 between the two
  paints A), `T=1` paint B vs the next paint A 0.03, `T=0.5` paint B vs the mean of the two paints A
  0.58 and shifted 2 px where the paints A are 4 px apart (frame 1500: 3 of 6 px). On the whole
  image paint B differs more than the two paints A do: the boat and Link, which the camera follows,
  are drawn at N+1 from the camera at t (step C).
- `COS_PAINT_PURITY=1` + `_REPEAT` (Outset, 600 frames): only the known harmless changes.
- Mac perf (frames 301-1200, per game frame, `COS_FPS60_CAMERA=0` -> on): Outset game thread
  2.21-2.25 -> 2.16-2.24 ms, painter2 0.37-0.42 -> 0.39-0.40, drawdone 0.02 -> 0.02; Dragon Roost
  2.45-2.51 -> 2.34-2.36, drawdone 0.02-0.05 -> 0.02-0.03: the hook's cost is in the noise (game
  thread: two small matrix computations per paint B; GX worker: 10 position, 10 normal matrices and
  8 lights transformed per uniform built in paint B's world section).
- Full `native/tools/regress.sh` passes (the mode off: no change). With `COS_FPS60_TEST=1`:
  file-select, outset-control (4:3 and 16:9), new-game, telescope-demo, sailing and the three
  picto-box targets pass. Switch NRO `scripts/switch/build_native.sh --runtime-assets` builds (patch
  0019 applies on top of the Switch's own patches). Not measured on the console.

### Known artifacts (to be judged on the console)

- **Objects attached to the camera jitter**: the camera is at t = 0.5 but every model is still at
  N+1 (step C). When the camera follows the player (running, sailing) the player, boat and
  anything moving with them move forward by half a frame's step in paint B and back in the next
  paint A: before step B the world stuttered at 30 Hz and the player stood still on screen; now
  the world is smooth and the followed objects shake at 60 Hz by half their per-frame motion. This
  is the main reason step C matters; `COS_FPS60_CAMERA=0` compares.
- Geometry or effects the CPU placed in screen space (lens flare, HUD markers on 3D targets), env
  maps and specular highlights follow N+1's camera; culling at the screen edges is N+1's.
- The boat's real shadow showed a stepped edge at the bottom of the screen with t = 0 (shadow
  receiver drawn for N+1's camera); not seen at t = 0.5 in the shots taken.

## The options menu row (2026-10-10)

Rendimiento > "60 fps (interpolación)" / "60 fps (interpolation)" (`COS_FPS60`, `0`/`1`, per
operation mode, default off: fresh installs stay at 30 presents a second). `pc_fps60_test()` is now a
runtime query: `COS_FPS60_TEST=1`/`0` (environment or `[dev]`) still forces the mode for a run
(tests, measurements), else the menu's value, which its apply callback (`pc_fps60_set`) changes
live. Switching is safe between frames: the menu is open when the row changes (no split while it is
open), the camera capture restarts (the first paint B is a cut), paint A keeps its full wait until a
frame has ended with paint B, and `pc_frame_begin` switches Aurora's async end of frame to follow the
mode (`aurora_set_async_end_frame` syncs the worker first; `COS_ASYNC_END_FRAME=0/1` still forces
it). Docked at 2.25 the GPU may need `COS_DYNRES` (per mode, like this row). Checked by the
`options-menu` target with `native/check/input/menu-fps60.txt` (on from the menu: 33 presents/s in
the window where it is turned on, 59.9 after).
