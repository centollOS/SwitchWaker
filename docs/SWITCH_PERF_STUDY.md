# Switch native port: where the Outset frame goes, and how to reach a stable 30 fps

Architecture study (no code changes). Basis: `feature/switch-native` at a6577ef, Aurora 3227d76
(`build/aurora-3227d76`), the Switch Dawn source staged under `scratchpad/stage-perf/dawn-src`
(encounter/dawn 266c1cf + `switch/dawn/patches/*`), Mesa 20.1.0-rc3 (devkitPro switch-mesa, nouveau
nvc0 on GM20B), and the hardware log `tasks/b6te8yi1n.output` (8 runs; the newest starts at line
128049 and carries the replay timers of 450eda7/b92b2f4).

## 0. Summary and recommendation

1. The ~20 ms of Dawn GL `execute` that no replay timer accounts for is almost certainly **not CPU
   work: it is the render worker waiting for the GPU inside Mesa**, surfacing in the first GL calls
   that emit commands in a frame (the FBO set-up and clears of the first render pass, which are
   untimed). Evidence (section 3): with the *same* draw and pass counts, `execute` is 7-9 ms in every
   window where the game thread paced itself at 30 fps (GPU idle part of the frame) and 26-31 ms in
   every window where the worker ran back to back. Nothing in Aurora or Dawn blocks on the GPU
   (frame slots are CPU-side, staging maps are `GL_MAP_UNSYNCHRONIZED_BIT`, fences are polled with
   timeout 0, present is a 0.5 ms blit+swap), so the only back-pressure left is nouveau's push-buffer
   ring reuse (`pushbuf_space` -> `nouveau_bo_map` -> fence wait) after each frame's kick.
   Corollary: **the Outset frame is GPU-bound at roughly 33-38 ms** on this console, and the
   worker's real CPU cost is ~15 ms (encode 3 + execute 8 + other 3.5 + present 0.5).
2. The GPU clock is not in the log. Stock handheld is 307 MHz (docked 768 MHz); this single fact
   changes the whole budget and must be logged in the next run.
3. One hardware run decides it with zero behaviour change (section 3.4): the existing
   `COS_SWITCH_GL_FINISH=1` A/B plus five cheap timers (per-region ticks in the untimed parts of
   `CommandBufferGL::Execute`, GPU `GL_TIME_ELAPSED_EXT` per pass, worker thread CPU ticks, GPU/EMC
   clock and operation mode at start-up).
4. If confirmed GPU-bound (expected), the lever is pixel work, not draw-call overhead: Aurora already
   has an internal-resolution API (`VISetFrameBufferScale`, used by Dusklight for its
   "internal resolution" setting). 960x540 (scale 1.125) should cut ~10-14 ms of GPU time and reach
   30 fps in Outset at half a day of work; 854x480 (scale 1.0, the GameCube's vertical resolution)
   is the fallback. Then take the shadow passes off the EFB, then dynamic resolution driven by the
   GPU timer. Per-draw restructuring (option b) only matters for CPU-bound scenes (1000+ draws) and
   should come after.

## 1. Measured state (newest run, Outset "sea room 44", `ROOM_SCENE`)

Steady state, frames 5881-6180 (`[cos] perf frames`, `perf-switch` lines):

| quantity | value |
|---|---|
| fps / retraces | 25.0-27.2 fps, 50-54 retraces/s |
| game thread | 36.7-40.0 ms avg, of which **begin (slot wait) 24.7-26.9**, logic 5.8-6.4, painter 2.8-3.2, end_frame 0.9-1.1, pace wait 0.0 |
| render worker busy | 36.7-39.9 ms: encode 3.1-3.6, end_frame 33.5-36.3 (unmap 0.01, acquire 0.1, **submit 32.1-34.8**, present 0.5), events 0.2 |
| Dawn GL per frame | 13 passes, 430-527 draws, 106-107 pipeline applies, 555-744 bind-group applies, 153-245 texture binds, 524-622 `glUniform` uploads, 20 buffer copies 1.37-1.55 MB, 0 texture uploads, 0 glFinish, 0 fence waits |
| flush | 31.8-34.5 ms = **execute 28.3-30.8** + other work 3.4-3.5 + release 0.12 |
| replay timers (inside execute) | pipelines 0.73, bind groups 1.5-2.2, immediates 0.6-0.7, vertex state 0.08, draw calls 3.4-4.1 (18 us after a pipeline change, 8-10 us after a texture bind, 3.4 us others) = **6.5-7.8 ms timed** |
| unaccounted inside execute | **21-23 ms** |

Earlier Outset windows of the same run (frames 2701-4560): 11 passes, 325-390 draws, 16 copies
1.1-1.25 MB, execute 26.5-29.4 ms, timed 6.3-7.5 ms. The heavier scene at frames 601-960 (1120-1320
draws, 10-11 passes, 3.2-3.7 MB copies) had execute 47-51 ms with ~10-11 ms timed.

Hitches: every 220-390 ms hitch is a pipeline compile ("pipeline compile 328.4 ms (1)" with
"other work 327.6" or "submit 334.7"): the lazy `glLinkProgram` inside Dawn's deferred GL work.
That is the other agent's program-binary-cache topic; it does not touch the steady state, but note
that in a GPU-bound regime a 300 ms CPU stall also drains the GPU queue, so the cache helps twice.

## 2. Frame anatomy: game -> Aurora -> Dawn GL -> Mesa

### 2.1 What the game asks for (per play frame in Outset)

- **Real-time shadows** (`game/src/d/d_drawlist.cpp:1203`, `dDlst_shadowReal_c::imageDraw`,
  driven by `dDlst_shadowControl_c::imageDraw` at :1588-1604): for each active shadow the casters are
  drawn into a 256x256 viewport *of the EFB*, then `GXCopyTex(I4, 128x128, clear=TRUE)`.
- **Main scene**: sky, sea, terrain, actors, particles.
- **Depth-of-field / distance haze** (`game/src/m_Do/m_Do_graphic.cpp:616-700`, `drawDepth`,
  called every frame while `!dMenu_flag()` at :1720): `GXCopyTex` of the Z buffer as Z16 and of the
  colour buffer at half size, then a full-screen composite quad.
- 2D/HUD/messages, then `GXCopyDisp` (`JFWDisplay.cpp:153/203`).
- Rare: `d_msg`/`d_message` text-box copies, capture, game over, fades.

### 2.2 What Aurora turns it into (`lib/gfx/recording.cpp`, `encoding.cpp`, `lib/dolphin/gx/GXFrameBuffer.cpp:36`)

- `copy_tex` maps the copy rectangle to EFB pixels (`map_logical_scissor`) and scales the destination
  by target/logical (1280/640 = 2, 720/480 = 1.5: a 128x128 I4 shadow becomes 256x192, the 320x240
  DOF copies become 640x360), keeps one destination texture per `(dest, size, format)` in
  `copyTextureCache`, then `resolve_pass_into` (recording.cpp:1020): seals the current EFB pass and
  opens a continuation pass with `LoadOp::Load`; a `clear` copy that is not the whole target becomes
  a `clear::render` full-rect draw (recording.cpp:1075, `clear.cpp`) in the continuation pass.
- `encoding.cpp:render`: each sealed pass becomes `BeginRenderPass` + commands + `End`; a pass with a
  `resolveTarget` is followed by a `tex_copy_conv::run` (Z16/I4/RGB565... conversion fragment pass,
  `tex_copy_conv.cpp:572-645`) or `blit`; segments with neither draws nor clears are `discardable`
  and skipped (the empty segment between the two DOF copies).
- Before every op the staging ranges written so far are copied into the shared vertex/uniform/index/
  storage buffers (`copy_staging_to_high_water`): 16-20 `CopyBufferToBuffer` per frame.
- `aurora.cpp:end_frame` adds two passes on the swapchain texture: the present blit ("EFB copy
  render pass") and the ImGui pass (the FPS overlay, on by default: `COS_FPS_OVERLAY=1`,
  `switch/native/source/cos_switch.cpp:409`).
- MSAA is already off: `AuroraConfig.msaa` is 0 -> 1 (`aurora.cpp:123`), so no resolve ever runs and
  "no MSAA on Switch" buys nothing.

Pass count = EFB segments (N shadows + 1 main + 1 DOF continuation) + conversion passes (N + 2) +
present + ImGui = 2N + 5: **9/11/13 passes = 2/3/4 shadows** on screen. This matches the log exactly.

### 2.3 What Dawn GL does with it (`CommandBufferGL.cpp`, with `gl_defer`)

All of it runs inside `Queue::Submit` on the render worker (`DeviceGL.cpp:FlushPendingGLCommands`,
one flush = 14 items: 13 deferred items (staging `glUnmapBuffer`/`glMapBufferRange`, texture or bind
group object creation, `WriteBuffer`) = "other work", plus the one `CommandBuffer::Execute`).

Per render pass (`ExecuteRenderPass`, :1342-1510, 1690-1697): `GenFramebuffers`,
`BindFramebuffer(READ, 0)`, `BindFramebuffer(DRAW, fbo)`, `FramebufferTexture2D` per attachment,
`DrawBuffers`, `Viewport`, `Scissor`, `ClearBufferfv/fi` for `LoadOp::Clear`, the command loop, then
`DeleteFramebuffers`. Lazy clears (`LazyClearSyncScope`, `EnsureSubresourceContentInitialized`) are
no-ops once a texture is initialised; the copy destinations and pass-snapshot textures are reused,
so none fire in steady state. Per draw (`gx/pipeline.cpp:60`): `SetImmediates` (64 B ->
`glUniform1uiv`), `SetBindGroup(1, dynamic offset)` -> one `glBindBufferRange(UBO)`,
`SetBindGroup(2, textures)` -> `ActiveTexture/BindTexture/BindSampler` per changed unit,
`SetIndexBuffer` (deduplicated by the shared-VAO patch), `glDrawElementsInstanced`.

Timed by `dawn-switch-gl-replay-timers.patch`: pipeline applies, bind-group applies, immediates,
vertex state, the `glDraw*` calls. **Untimed**: everything in pass begin/end above, `SetViewport`/
`SetScissorRect`/`SetBlendConstant`, `CopyBufferToBuffer` (`glCopyBufferSubData`, :908-937),
`CopyTextureToTexture` (`glCopyImageSubData`, :1117-1156), and whatever Mesa does inside those.

### 2.4 What Mesa/nouveau does

Draw-time validation (`nvc0_state_validate`) is what the 3.4-18 us per draw measures. The FBO
change per pass costs completeness validation and `nvc0_validate_fb` (tens of us). Buffer copies go
to the copy engine (`nouveau_copy_buffer` -> `copy_data`, both buffers in GART on Tegra), clears to
`nvc0_clear`; none of these wait on the CPU. The GPU-wait points nouveau has are: `nouveau_buffer_sync`
on a synchronised map of a busy buffer (Dawn maps staging with `GL_MAP_UNSYNCHRONIZED_BIT`,
`BufferGL.cpp:262`, so not here), fence waits (Dawn polls `glClientWaitSync(sync, 0, 0)`,
`dawn-switch-gl-fence-queue.patch`), and **push-buffer chunk reuse**: `nouveau_pushbuf_space` takes
the next chunk of the ring (upstream nvc0 creates 4 x 512 KiB) and maps it, which waits for the fence
of that chunk's previous submission. The Switch `libdrm_nouveau` port submits each kick with
`nvGpuChannelAppendEntry` + `nvGpuChannelKickoff` and keeps per-bo fences (devkitPro/libdrm_nouveau
`source/pushbuf.c`); a kick happens at least once per frame because Dawn's `glFenceSync` at the end
of each Submit flushes (`st_fence_sync` -> `pipe->flush` -> `PUSH_KICK`). A ~300 KiB Outset frame is
about one chunk, so the ring wraps every few frames and the *first* command emission of a frame (the
first pass's FBO validation/clear) blocks until the GPU retires a frame. That is the only mechanism
in the stack that fits all of the observations below.

## 3. Where the unaccounted ~20 ms goes

### 3.1 The decisive correlation in the log

Same content, same counts, two very different `execute` times, depending only on whether the game
thread was pacing itself (`pace wait`) or waiting for the worker (`begin` = slot wait):

| frames | draws | passes | copies | execute | game thread: begin / pace wait | fps |
|---|---|---|---|---|---|---|
| 2041-2100 | 357 | 11 | 16 / 1106 KiB | **7.5** | 0.17 / 22.8 | 30.0 |
| 2101-2160 | 409 | 10.8 | 15.7 / 1194 KiB | 22.5 | 12.5 / 10.9 | 29.8 |
| 2401-2460 | 307 | 11.1 | 16 / 1035 KiB | **9.3** | 0.36 / 22.3 | 30.0 |
| 2461-2520 | 386 | 10.8 | 15.6 / 1140 KiB | **8.3** | 0.17 / 21.9 | 30.0 |
| 2521-2580 | 397 | 10.8 | 15.7 / 1163 KiB | 22.7 | 3.5 / 18.8 | 30.0 |
| 2701-2880 | 329-331 | 11 | 16 / 1152-1157 KiB | 26.4-26.6 | 11.4 / 11.7 | 30.0 |
| 4981-5040 | 291 | 9.7 | 13.4 / 990 KiB | **7.3** | 0.15 / 22.9 | 30.0 |
| 5401-5460 | 399 | 9 | 12 / 1276 KiB | **8.1** | 0.17 / 20.6 | 30.0 |
| 5521-5580 | 361 | 9 | 12 / 1132 KiB | **7.2** | 0.15 / 23.1 | 30.0 (worker 14.2 busy) |
| 5881-6180 | 430-527 | 13 | 20 / 1371-1553 KiB | 28.3-30.8 | 24.7-26.9 / 0.0 | 25-27 |

The timed buckets scale with draws (3.4 us/draw, 18 us after a pipeline change) and never show
multi-ms stalls, so the extra 20 ms is not in any `glDraw*`, bind, uniform or pipeline call. The CPU
cost of a frame's GL replay is therefore ~7-9 ms for 300-400 draws; the rest is a wait that exists
only when the GPU has not finished the previous frame when the next `Execute` starts.

### 3.2 Candidates eliminated from the code

- GPU profiler timestamp queries (`ResolveQuerySet` -> blocking `glGetQueryObjectuiv`,
  CommandBufferGL.cpp:1199): off. `gpu_prof::initialize` needs `TimestampQuery`, which Aurora only
  requests under `TRACY_ENABLE` (`gpu.cpp:948`), the run's "Enabling features" list has none, and
  Dawn GL's `WriteTimestamp` is unimplemented (:1211).
- Depth-peek snapshot/readback: only when `GXPeekZ` was requested (`depth_peek.cpp:353`); the game does
  not call it (no `GXPeekZ` in `game/src`).
- Vsync / window buffer dequeue: Dawn's device context is on a pbuffer (`ContextEGL.cpp:271`); the
  window surface is touched only in `SwapChainEGL::PresentImpl` (MakeCurrent + `glBlitFramebuffer`
  + `eglSwapBuffers`) = "present 0.5 ms". Frame times (37-40 ms) are not vblank multiples.
- Staging maps/unmaps and Dawn fences: `GL_MAP_UNSYNCHRONIZED_BIT`; `glClientWaitSync` timeout 0;
  "staging wait 0.00", "0 waits", "0 glFinish" throughout.
- Lazy clears: no new textures per frame in steady state ("0 tex uploads"; copy destinations and
  snapshot pools are cached).
- MSAA resolves: `msaaSamples = 1`.
- Pipeline compiles: separately visible as hitches, not in the steady-state windows.

What remains is a GPU back-pressure wait in Mesa (section 2.4) and, less likely, preemption of the
worker by the JAudio/DVD threads sharing cores 1-2 (`switch/native/source/thread_wrap.c`: all
non-game threads alternate cores 1 and 2 at priority 0x2C; Horizon does not time-slice equal
priorities). Preemption does not explain why the paced windows are fast, but it is cheap to rule out.

### 3.3 Consequence

Frame time ~37-40 ms = GPU time. GM20B at 1280x720 with Aurora's TEV fragment shaders, alpha-tested
foliage (no early-Z), the sea, 4 shadow casters rendered into the EFB and cleared again, the DOF
copies (Z16 conversion reading the depth texture, colour blit), a full-screen composite, the present
blit and the overlay: ~5-10 Mpix of shading per frame. At 307 MHz (handheld) 20-35 ms is plausible;
at 768 MHz (docked) it would be 8-14 ms. The log does not say which mode the console was in.

### 3.4 Timers that confirm it in one more hardware run

All Switch-only, under `switch/dawn/patches/dawn-switch-gl-replay-timers.patch` and
`switch/native/source/cos_switch_stats.cpp`, printed on the existing `perf-switch` lines:

1. **Zero-code A/B first**: `COS_SWITCH_GL_FINISH=1` in `env.txt` (already implemented). Prediction if
   GPU-bound: `execute` drops to ~8 ms, the `glFinish` column shows ~20-28 ms per frame, fps unchanged.
   If `execute` stays ~28 ms with `glFinish` ~0-2 ms, the time is real CPU work in the untimed regions.
2. **Untimed-region ticks in `Execute`** (`cntpct_el0`, like the existing buckets): pass begin split
   into (a) FBO gen/bind/attach/DrawBuffers, (b) `ClearBuffer*`, (c) lazy-clear scopes; pass end
   (`DeleteFramebuffers`, resolve); `CopyBufferToBuffer` total; `CopyTextureToTexture` total+count;
   `SetViewport`+`SetScissorRect`+`SetBlendConstant` total; and the residual (pass total minus
   buckets). Report the **first pass of the frame separately** from the others: the hypothesis says
   pass 0's (a)+(b) carries the missing ~20 ms and every other pass's begin is ~50-100 us.
3. **GPU time per pass**: `glBeginQuery(GL_TIME_ELAPSED_EXT)`/`glEndQuery` around each
   `ExecuteRenderPass` and around each copy group, results read 3 frames later only when
   `GL_QUERY_RESULT_AVAILABLE` (never block), summed per pass label (EFB segments, TexCopyConv,
   present, ImGui) and per frame. Mesa nvc0 exposes `EXT_disjoint_timer_query` on GLES 3.2
   (`PIPE_CAP_QUERY_TIMESTAMP`). This is the number every later decision needs (which passes to
   cut, how far to drop resolution, dynamic-resolution control input).
4. **Worker CPU vs wall**: `svcGetInfo(InfoType_ThreadTickCount, <thread handle>, -1)` for the render
   worker, the JAudio thread and the DVD thread per perf window; worker ticks ~15 ms/frame against
   37 ms wall confirms a wait, not contention.
5. **Start-up facts**: `appletGetOperationMode()` (handheld/docked), GPU and EMC clock via
   `clkrstOpenSession(PcvModuleId_GPU/EMC)` + `clkrstGetClockRate` (or `pcvGetClockRate` on old FW),
   printed once in the `[switch] centollos native:` line.
6. Optional: the run with `COS_FPS_OVERLAY=0` for a clean pass count (removes the ImGui pass).

## 4. Options, ranked for "stable 30 fps in Outset" (worker < ~25 ms)

Assumes section 3 confirms GPU-bound (expected). Where the result would flip under the CPU-bound
branch it is stated. "Mac" = effect on the Mac build.

| rank | option | expected gain | effort | risk | Mac | fidelity |
|---|---|---|---|---|---|---|
| 1 | **(f) Internal resolution via `VISetFrameBufferScale`** (`include/aurora/vi.h`; `window.cpp:456` scales the 640x480 logical EFB by the factor and fits the window aspect; Dusklight: `ref/dusklight/src/dusk/settings.cpp:234`). 1.5 = 1280x720 today; 1.125 = 960x540 (-44 % pixels); 1.0 = 854x480 (-56 %). Present pass resamples (`resample_present_source`, `aurora_set_resampler` AREA/BILINEAR). EFB copies scale with it automatically (`scale_copy_dst`). | GPU 35 -> ~22 ms at 960x540 if ~80 % of GPU time is pixel work (to be read from timer 3); ~17 ms at 854x480. Worker CPU unchanged (~15 ms). Reaches 30 fps on its own in the likely case. | 0.5 day: `COS_FB_SCALE` env option read in `pc_aurora_init`/`cos_switch.cpp` defaults, call before the first frame; measure two values. | low | none (opt-in env) | softer 3D; HUD/text also at internal res (480p is what the GameCube drew). Area resampler keeps pixel art crisp. |
| 2 | **(a1) Shadow casters off the EFB**: route `dDlst_shadowReal_c::imageDraw` through Aurora's offscreen pass API (`gfx::create_pass`/`resolve_pass`, recording.cpp:854-903) under `TARGET_PC`, so each shadow is a 256x192 (or smaller) offscreen target instead of a pass break on the 1280x720 EFB plus a 512x384 clear draw. | GPU 1-3 ms (4 shadows: 4 EFB segment breaks, 4 clear draws, 4 conversions on the big RT); CPU ~0.5 ms (fewer FBO churn/passes). | 1-2 days | medium (shadow texture coordinates, I4 conversion path, actors drawn with `drawFast`) | benefits too (same pass reduction) | none if correct |
| 3 | **(a3/a5) Pass trimming**: overlay off in release (`COS_FPS_OVERLAY=0`, 1 pass), keep Aurora's discardable segments; nothing else is mergeable because `GXCopyTex` dictates breaks and the DOF copies run every play frame (`drawDepth` at m_Do_graphic.cpp:1720). | GPU ~0.3-0.5 ms, CPU ~0.2 ms | 0 | none | none | none |
| 4 | **(g') Core hygiene**: pin the render worker to core 2 and JAudio/DVD to core 1 (`cos_switch_next_thread_core`), so the worker is never queued behind the mixer at equal priority. | 0-3 ms worker wall time (timer 4 tells) | 0.5 day | low | none | none |
| 5 | **(c) Dawn GL tuning**: FBO cache keyed by attachment views (drop Gen/Attach/Delete x13 per frame and the completeness re-validation), skip `BindFramebuffer(READ, 0)`, dedupe redundant viewport/scissor. Dropping `gl_defer` gives nothing (same thread does all GL; the shared-context path draws nothing on this Mesa, patch 0001) and would spread "other work" into encode. | CPU 0.5-1 ms; 0 fps while GPU-bound | 1 day | low | none (Switch patches) | none |
| 6 | **(b) Per-draw cost**: per-draw uniforms as a storage array indexed from the 64-byte immediates (one `glBindBufferRange` per pass instead of 470-620), merged draws across equal pipeline+textures (Aurora already merges consecutive display lists: `mergedDrawCallCount`). Texture binds cannot be reordered (GX order) and GLES 3.2 Mesa has no bindless. Mesa's `nvc0_state_validate` per draw (~3.4 us) stays. | CPU 2-4 ms of the worker's ~15 ms; needed only for CPU-bound scenes (1120-1320 draws -> ~11 ms timed + untimed CPU; windows 601-960) | 4-6 days (WGSL generator `gx/shader.cpp`, Dawn immediates, Mac validation) | medium-high: shared shader path, Mac regression risk, nvc0 dynamic-index storage performance | shared code path; must be validated on Mac | none |
| 7 | **(f') Dynamic resolution**: `set_frame_buffer_scale` at run time already recreates the EFB/depth/resolved textures (`resize_swapchain_internal`); add hysteresis and a controller fed by the per-frame GPU elapsed time (timer 3), e.g. 0.9-1.5 in 1/16 steps, change at most every 60 frames. | holds 30 fps in heavier rooms without paying the resolution everywhere | 1-2 days after (f) | medium (texture recreation hitch per change; pass snapshot pools keyed by size) | opt-in | varies by scene |
| 8 | **(d) Newer Mesa / Dawn**: devkitPro's switch-mesa is the 20.1.0-rc3 port; no newer port exists, so it means forward-porting nvc0 and the nvdrv winsys/libdrm shim to Mesa 25 (weeks). Dawn is the pinned encounter fork; upstream GL backend changes do not alter GPU time. | CPU validation maybe 10-20 % better; GPU 0 | weeks | high | none | none |
| 9 | **(e) Vulkan or deko3d**: no Vulkan driver exists for Horizon homebrew. NVK needs the Linux nouveau DRM uAPI (VM_BIND) through `nvkmd`; the Switch talks to the GPU via nvhost/nvmap IPC and nobody has written that backend; Maxwell is also NVK's least-served generation. deko3d is the proven native Maxwell API (fincs), but its shaders are compiled offline by `uam`, which conflicts with Aurora's runtime TEV shader generation (porting uam on-device is possible in principle, switch-mesa already ships the GLSL compiler, but is a project of its own), and a Dawn or Aurora backend is a multi-month effort. | large CPU savings (driver overhead), GPU shading cost unchanged | months | very high | none | none |
| 10 | (g) Splitting encode/submit further across cores: encode (3 ms) is already off the game thread; GL cannot run on two threads with one context; Mesa 20.1 `glthread` is not a GLES path and buys nothing while GPU-bound. | 0 | - | - | - | - |

If the run shows the CPU-bound branch instead (glFinish ~0, pass-0 begin not inflated, worker ticks
~ wall): the order becomes (c) FBO cache and per-pass fixes -> (g') core pinning -> (b) per-draw
data, with (f) still useful for the heavier rooms.

Interaction with the program-binary cache work: compile hitches land in the deferred GL work
("other work 327.6 ms" at hitch frame 5852); they are independent of all options above, but in a
GPU-bound regime any CPU stall also empties the GPU queue, so the precompile pays back on both sides.

## 5. Plan

**Step 1 - one instrumentation run (0.5 day of patches, no behaviour change).** Add timers 2-5 of
section 3.4 to the Switch Dawn patch and `cos_switch_stats.cpp`; run Outset twice: default, and with
`COS_SWITCH_GL_FINISH=1` (`env.txt`). Record docked/handheld and the GPU clock.
Decision:
- pass-0 begin (or glFinish) ~20 ms and GPU elapsed ~30-38 ms, worker ticks ~15 ms -> GPU-bound ->
  Step 2a;
- GPU elapsed <= 15 ms, untimed CPU regions carry the time or worker ticks ~ wall -> Step 2b.

**Step 2a (GPU-bound, expected) - internal resolution (option f), 0.5-1 day.** `COS_FB_SCALE`
through `VISetFrameBufferScale`; measure 1.125 (960x540) and 1.0 (854x480) with `COS_FPS_OVERLAY=0`,
reading GPU elapsed per pass and fps. Accept the largest scale that keeps GPU elapsed < 28 ms
(p95) in Outset exterior with Link running. Then option (a1) if the per-pass GPU timer shows the
shadow segments and their clears above ~2 ms. If the console was handheld, state the docked numbers
in the docs as well (768 MHz GPU: likely 30 fps at 1280x720 without further work).

**Step 2b (CPU-bound branch) - Dawn FBO cache + core pinning (options c, g'), 1.5 days.** Re-measure;
if the worker is still > 25 ms, start option (b) with the per-draw uniform array.

**Step 3 - make it robust, 1-2 days.** Dynamic resolution (f') driven by GPU elapsed, with the
program-binary cache in place to remove the compile hitches. Revisit option (b) only when a scene
shows timed draw CPU above ~12 ms (1000+ draws), which the 601-960 windows did.

Measurements that decide between branches at every step: GPU elapsed per frame (timer 3) against
worker CPU ticks (timer 4); `execute` with glFinish on; pass-0 begin ticks; game thread `begin`
(slot wait) falling to ~0 at 30 fps.

## Appendix A - references

- Frame structure: `build/aurora-3227d76/lib/gfx/recording.cpp` (`resolve_pass_into` :1020,
  `begin_recording` :482, `finish` :1180), `encoding.cpp` (`render` :190, `copy_staging_to_high_water`
  :395), `clear.cpp`, `tex_copy_conv.cpp:572`, `lib/dolphin/gx/GXFrameBuffer.cpp:16-70`
  (`scale_copy_dst`, `copy_tex`), `lib/aurora.cpp:262-420` (`end_frame`: present + ImGui passes),
  `lib/gfx/frame.cpp` (2 frame slots, 5 staging buffers, `acquire_frame_slot`, `begin_frame`,
  `end_frame`), `lib/gfx/render_worker.cpp`, `lib/gx/pipeline.cpp:60` (`render`: per-draw binds),
  `lib/gx/gx.hpp:80` (`DrawImmediateData`, 64 B), `lib/gfx/resources.hpp` (24/5/2/8 MiB shared buffers),
  `lib/window.cpp:446-475` (`get_window_size`, `g_frameBufferScale`), `include/aurora/vi.h`
  (`VISetFrameBufferScale`), `lib/webgpu/gpu.cpp:1120-1150` (`resize_swapchain_internal`).
- Dawn GL: `scratchpad/stage-perf/dawn-src/src/dawn/native/opengl/CommandBufferGL.cpp`
  (`Execute` :846, `ExecuteRenderPass` :1342, resolve :714, copies :908/:1117, `ResolveQuerySet` :1180),
  `DeviceGL.cpp:503` (`FlushPendingGLCommands`), `DeviceGL.h:72-180` (`EnqueueGL`, `gl_defer`),
  `QueueGL.cpp:123` (`SubmitImpl`), `BufferGL.cpp:216-299` (map flags), `ContextEGL.cpp:271`
  (pbuffer), `SwapChainEGL.cpp:100-145` (present), `TextureGL.cpp:343` (`ClearTexture`),
  `UtilsGL.h:68-80` (`DAWN_GL_TRY` checks errors only with asserts).
- Switch patches: `switch/dawn/patches/dawn-switch-gl-command-stats.patch`,
  `dawn-switch-gl-replay-timers.patch`, `dawn-switch-gl-fence-queue.patch`,
  `dawn-switch-gl-shared-vao.patch`, `dawn-switch-gl-texture-params.patch`,
  `dawn-switch-nwindow-surface.patch`; `switch/native/aurora/patches/0001-...gl-defer.patch`,
  `0005-switch-frame-stats.patch`; `switch/native/source/thread_wrap.c`, `cos_switch.cpp:405-411`.
- Game: `game/src/m_Do/m_Do_graphic.cpp:616-700, 1720` (DOF copies every play frame),
  `game/src/d/d_drawlist.cpp:1170-1210, 1585-1605` (shadows into the EFB),
  `game/src/JSystem/JFramework/JFWDisplay.cpp:140-210` (`GXCopyDisp`).
- Mesa: `scratchpad/stage-perf/mesa/nouveau_buffer.c:374-490, 563-620, 640-690` (map sync rules,
  copy path, domains), `nouveau_screen.c:219-270`; the push-buffer ring behaviour is upstream
  libdrm_nouveau `pushbuf.c` as adapted in devkitPro/libdrm_nouveau (`nvGpuChannelAppendEntry`,
  `nvGpuChannelKickoff`, per-bo fences).
- Log: `tasks/b6te8yi1n.output` lines 128049-129673 (newest run); windows cited in section 3.1.

## Appendix B - external references used for options (d) and (e)

- NVK cannot run on Horizon homebrew (needs the Linux nouveau DRM driver; Switch homebrew uses
  nvhost/nvmap IPC); deko3d is the working native path: https://gbatemp.net/posts/10885909/
- NVK's current state (Vulkan 1.3 conformant, Vulkan Video merged for Mesa 26.3), Turing+ focus:
  https://phoronix.com/news/NVK-Vulkan-Video-Mesa-26.3 , https://www.phoronix.com/news/Nouveau-NVK-XDC2024
- devkitPro switch-mesa history (the port dates from 2018; the console runs 20.1.0-rc3):
  https://devkitpro.org/viewtopic.php?p=16134 , https://devkitpro.org/viewtopic.php?p=16211
- Switch libdrm_nouveau port (push-buffer submission via libnx): https://github.com/devkitPro/libdrm_nouveau
