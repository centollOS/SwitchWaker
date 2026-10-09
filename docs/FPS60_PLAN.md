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
