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
