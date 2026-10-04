# Switch native port: where the Outset frame goes, and how to reach a stable 30 fps

Architecture study (no code changes). Basis: `feature/switch-native` at a6577ef, Aurora 3227d76
(`build/aurora-3227d76`), the Switch Dawn source staged under `scratchpad/stage-perf/dawn-src`
(encounter/dawn 266c1cf + `switch/dawn/patches/*`), Mesa 20.1.0-rc3 (devkitPro switch-mesa, nouveau
nvc0 on GM20B), and the hardware log `tasks/b6te8yi1n.output` (8 runs; the newest starts at line
128049 and carries the replay timers of 450eda7/b92b2f4).

## 0. Summary and recommendation

> **Update 2026-10-04 (section 8):** the GPU timer under-reads by 1.63x on the Switch (Tegra PTIMER
> at 19.2 MHz counted as 31.25 MHz); with real times, Outset's dips and the forest are GPU-bound.
> The forest's mist alone was 12.45 of 29 screens of fragments per frame at 720p: now drawn at a
> quarter of the resolution with the same blend (default). Dynamic resolution (`COS_DYNRES=1`) and
> a lower-resolution sky (`COS_SKY_LOWRES=2`) are opt-in.

> **Update 2026-10-04 (section 6):** the hardware run with the timers of section 3.4 confirmed the
> wait at the first pass's clears, but its cause is not the push-buffer ring of section 2.4: it is
> libdrm_nouveau waiting for the GPU when Mesa frees the swapchain texture Dawn destroys after every
> present. Fixed by deferring GL deletes (`dawn-switch-gl-deferred-delete.patch`).

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
   printed once in the `[switch] centollOS:` line.
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
(p95) in Outset exterior with the player running. Then option (a1) if the per-pass GPU timer shows the
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

## Appendix B - opt-in GPU switches for the A/B session (lane/gpu-opts)

All off by default; `env.txt` variants in `build/switch-envs/` (`shadowoff`, `shadowgc`, `fbocache`,
`nodof`, `best` = 960x540 + shadowoff + fbocache, `bestnodof`).

- **`COS_SHADOW_OFFSCREEN=1`** (option a1; `d_drawlist.cpp`, `pc_gpu_opts.cpp`): the casters go into
  a `GXCreateFrameBuffer` target the pixel size of their 256x256 EFB corner (512x384 at 1280x720),
  copied at the size Aurora gave the 128x128 copy (256x192). Mac, same frame, both paths rendered
  into separate copy textures and drawn side by side: pixel-identical at 2560x1440, 1280x720 and
  960x540. Pass count does not change (one offscreen segment per shadow instead of one EFB segment,
  plus a discarded trailing offscreen segment): with two shadows 5 GX segments, 4 resolves and 2
  partial-clear draws either way, but the render targets bound per frame go from 4.61 to 3.16 Mpx,
  the main EFB is no longer split and reloaded per shadow, and the post-copy depth clears become
  full-target clears. Expect a small GPU gain (well under the 1-3 ms of the table's upper bound;
  the per-pass GPU timer will tell). `=gc` renders at the GameCube's 256x256/128x128 at any
  resolution: a third of the caster pixels, softer shadows.
- **`COS_SWITCH_GL_FBO_CACHE=1`** (option c; `dawn-switch-gl-fbo-cache.patch`): CPU-side only (Gen/
  attach/DrawBuffers/Delete per pass, the READ unbind, repeated viewport/scissor/depth range);
  not testable on the Mac (Metal). Read it in the `execute split` fbo column.
- **`COS_DOF=0`** (`m_Do_graphic.cpp` drawDepth): no Z16 copy, no half-size colour copy, no
  full-screen composite unless monotone or motion blur is on; letterbox bars kept. Mac, two
  shadows: 5 GX segments / 4 resolves become 3 / 2 (Dawn passes 11 -> 7 with the overlay). Far
  scenery is no longer softened.

Note for Mac pixel compares: outset-control runs are not frame-reproducible across processes at
16:9 (camera timing differs from run to run), so A/B image checks must render both variants in the
same frame, as done for the shadows.

## 6. Hardware findings, 2026-10-04 (handheld, GPU 307.2 MHz, EMC 1331.2 MHz)

Log: `build/lanes/switch-gpu/build/switch-logs/gpu-default-handheld.log` (`env.txt`:
`COS_FB_SCALE=1.5`, overlay on, every timer of section 3.4 present). Fixes on `lane/render-stall`.

### 6.1 No CPU/GPU overlap: the first clear waits for the previous frame

Outset exterior at 1280x720 (frames 10921-11280, 1430-1545 draws, 10-13 passes, 4.1-4.3 MB of
buffer copies per frame):

| quantity | value |
|---|---|
| fps | 13.4-14.5 (game thread `begin` = slot wait 48-55 ms) |
| GPU per frame (`GL_TIME_ELAPSED`) | 37.6-41.2 ms (p95 41.2-43.6) |
| render worker | 69-75 ms busy wall, **25.5-26.9 ms CPU** |
| Dawn `execute` | 57.7-62.9 ms = **first-pass clears 41.9-46.3** + replay 13.8-14.4 + rest ~1.5 |
| first pass, everything but its clears | 0.3 ms (fbo 0.03, replay 0.26) |

So the frame costs clear-wait + replay + present instead of max(GPU 41, worker CPU ~27): CPU and GPU
serialise. In light scenes the same clear takes 1-3 ms, about the previous frame's GPU time (logo:
"clears 2.0" with "gpu 3.3 ms over 2 frames"), and in the Outset windows where the game thread
paced itself at 30 fps (frames 10801-10860: worker 14.7 ms busy, GPU 13.3 ms) it is 1.1 ms, because
the GPU had finished by the time the worker started. The wait does not burn CPU (worker CPU far below
wall), so it is a blocking kernel wait, not Mesa's spinning `nouveau_fence_wait`.

Cause (sources: devkitPro `mesa` branch `switch-20.1.0-rc3`, `libdrm_nouveau`, the Dawn tree):

1. Dawn's `SwapChainEGL::GetCurrentTextureImpl` creates a new 1280x720 texture every frame and
   `PresentImpl` destroys it right after `eglSwapBuffers` (`mTexture->APIDestroy()`).
2. Mesa's st/nvc0 still holds a reference to that texture through the present's framebuffer state;
   it is released at the next framebuffer validation, which is the next frame's first
   `glClearBuffer*` (Dawn's `LoadOp::Clear` of the first EFB pass; the FBO set-up before it does not
   validate). That is the last reference: `nv50_miptree_destroy`.
3. `nv50_miptree_destroy` (and `nouveau_buffer_release_gpu_storage` for buffers) defers the bo free
   to a fence callback only while the fence is not yet flushed; the texture's fence was flushed by
   the swap, so it calls `nouveau_bo_ref(NULL)` at once.
4. The Switch `libdrm_nouveau` frees with `nouveau_bo_del`, which starts with
   `nouveau_bo_fence_wait(bo, 0)` = `nvFenceWait(fence, -1)`: the CPU blocks until the GPU has
   retired the last submission that used the texture, i.e. the whole previous frame including the
   present blit. (On Linux the kernel keeps a busy bo alive; here unmapping it from the GPU address
   space early would fault, so the port waits.)

Candidates ruled out: the GPU timer queries (`dawn-switch-gl-gpu-timer.patch` checks
`GL_QUERY_RESULT_AVAILABLE` before reading and keeps queries 8 frames deep; nvc0's non-waiting
`get_query_result` only polls the fence and kicks; and the 20+ ms "unaccounted" execute time of
section 3 was there before the timers existed, so `COS_SWITCH_GPU_TIMER=0` cannot change it), Dawn's
fences (`glClientWaitSync` timeout 0), staging maps (unsynchronised), and clearing a texture the
previous frame still reads (nouveau orders that on the GPU, the CPU never waits for it). The
push-buffer ring of section 2.4 is a real back-pressure point but only once a frame's commands
fill 4 x 512 KiB; it would not land on the first clear every frame.

Fix (`switch/dawn/patches/dawn-switch-gl-deferred-delete.patch`, default on): Dawn GL textures,
texture views and buffers queue their GL name with the serial of the next queue fence, and
`Device::FlushPendingGLCommands` deletes the names whose serial has completed. Mesa then frees bos
whose fences have passed and `nouveau_bo_del` returns at once. `COS_SWITCH_GL_DEFER_DELETE=0` is the
A/B switch. Throttling now comes from where it should: the NWindow dequeue at present (FIFO, 3
buffers), the staging buffers and the push-buffer ring. Check on hardware: the new
`[cos] perf-switch gl stall: first-pass clears X ms per frame; deferred deletes on: +N deferred, +N
deleted, 0 forced, P pending` line should show tens of microseconds in every scene with about one
deferred texture per frame, `execute` in Outset should drop by ~45 ms, and the frame should become
GPU-bound (~41 ms, about 24 fps at 1280x720 handheld) instead of 73 ms; at `COS_FB_SCALE=1.125` or
docked it should then reach 30.

The GPU timer's classification also filed every GX pass under "other" ("efb passes 0.00"): Aurora
labels them "EFB N"/"Offscreen N", not "Render pass N". Fixed by
`dawn-switch-gl-gpu-timer-labels.patch`; in this log "other" (35.8-39.2 ms of 37.6-41.2) is in fact
the EFB passes, tex copy conv 0.1, present 0.6, ImGui 0.3, copies 0.9-1.1 ms. The GPU frame is
almost all game draws: internal resolution (option f) and the shadow/DOF options remain the levers.

### 6.2 The background pipeline warm-up blocks frames

After the loading screen (176 priority pipelines in 17.4 s), the other ~920 build back to back until
the first PLAY scene. Every hitch line of the logo scene (frames 2-26 and on, lines 70-450 of the
log) has "pipeline compile 126-140 ms (1)" and the render worker's `submit` as long, with worker CPU
3-5 ms and "other threads" 92-131 ms: Aurora's compile thread does the work inside
`ExecutePipelineGL`, which holds Dawn's single GL context (`ContextEGL::mExclusiveMakeCurrentMutex`)
for the whole build - Tint's WGSL -> GLSL translation, then Mesa's GLSL compile and nouveau codegen
at `glLinkProgram` - and the worker's next `FlushPendingGLCommands` waits for that mutex. Logos and
title ran at 4-7 fps for about three minutes (527/1094 after 57 s, ~105 ms per pipeline).

No real parallel compile exists on this stack: Mesa 20.1 nouveau has no
`set_max_shader_compiler_threads`, so `GL_KHR_parallel_shader_compile` (which Dawn already uses
when present) finishes inside `glLinkProgram`; a shared context on another thread draws nothing here
(Aurora Switch patch 0001) and nvc0 contexts share the screen's push buffer without locking. Fixes:

- `dawn-switch-gl-pipeline-compile.patch`: the Tint translation runs before the context is taken
  (`PipelineGL::PretranslateStages`; it needs only the GL version), so the worker waits only for
  Mesa's compile and link. The `[cos] precompile` line now prints "tint X ms, GL context Y ms each"
  per build: the first hardware run tells how much of the ~110 ms was Tint.
- Aurora Switch patch 0009 + `pc_precompile.cpp`: with `COS_PRECOMPILE=boot`, after the loading
  screen the warm-up is throttled to a duty cycle (`COS_PRECOMPILE_DUTY`, default 0.5): after a
  build of duration t the compile thread idles t(1-d)/d, then starts the next build right after a
  present. A pipeline a draw waits for is never held back. Expected at d = 0.5 with ~110 ms builds:
  a stall of ~110 ms (less the Tint share) every ~220 ms with full-rate frames in between, instead
  of every frame blocked; the warm-up takes about twice as long, and what the first PLAY scene
  leaves is built when first drawn as before. Lower d for smoother menus, 1 for the old behaviour.


## 7. Per-draw cost at 1400-1550 draws (720p handheld, GPU 460.8 MHz; lane/draw-cost)

Log: `build/lanes/render-stall/build/switch-logs/stall-720-460-handheld.log` (the render-stall
fixes in, clears 0.05 ms; Outset from "sea room 44 created at frame 7101"). Outset holds 30 fps up
to ~950 draws and drops to 20-23 fps in the 1300-1550-draw windows.

### 7.1 Where the worker's wall time goes when it is the bottleneck

All 60-frame windows of the Outset part, condensed (`dc` = the `glDraw*` bucket of the replay
timers, `others` = its per-draw cost for draws that follow neither a pipeline change nor a texture
bind; gap = worker busy wall minus worker CPU; o+a = CPU of the other threads and the audio thread):

| windows | fps | draws | dc ms | others us | GPU ms | worker CPU | worker wall | gap | o+a |
|---|---|---|---|---|---|---|---|---|---|
| 12301-15541 (typical) | 30.0 | 460-570 | 3.6-4.3 | 3.6-3.7 | 11-13 | 13.6-15.3 | 16.5-17.9 | 2.6-2.9 | 11-12.5 |
| 8401-8881, 9241-9421 | 30.0 | 700-950 | 5.0-6.7 | 3.7-3.8 | 16-20 | 17.4-21.4 | 20.2-24.1 | 2.7-3.3 | 13.6-16 |
| 7441-7921 | 27.5-29.8 | 860-995 | 6.1-7.0 | 3.8-3.9 | 20-22 | 20.5-22.5 | 30.8-36.3 | 10-15 | 15.4-16.7 |
| 10321-12061 (steady dip) | 23.0-23.3 | 1316-1373 | 19.4-21.4 | 10.3-13.1 | 26.0-26.5 | 24.6-25.7 | 42.6-43.6 | 17.7-18.6 | 18.1-18.6 |
| 10141-10200 (worst) | 20.4 | 1553 | 27.2 | 15.3 | 30.1 | 27.6 | 49.1 | 21.4 | 19.4 |

1. **The worker's CPU cost is linear in draws and not the problem by itself**: 13 us of CPU per
   draw for the whole worker (Aurora encode + Dawn replay + Mesa), 24.7-27.6 ms at 1300-1550 draws,
   under the 33 ms of a 30 fps frame.
2. **The extra wall time appears only when the worker runs back to back** (the game thread waits
   for its slot, fps < 30), and it is spent **inside the `glDraw*` calls**: the same kind of draw
   costs 3.7 us at 30 fps and 10-15 us in the dip, while the CPU per draw does not change. The gap
   is ~2.7 ms in every paced window whatever the draw count, 10-15 ms at 28-29 fps and 18-21 ms in
   the dip. It is not CPU work (thread ticks), so the worker is either **blocked** or **runnable but
   not running**.
3. Inside a `glDraw*`, Mesa 20.1 can only block in one place: `PUSH_SPACE` ->
   `nouveau_pushbuf_space`, when the 512 KiB push-buffer chunk is full. libdrm_nouveau
   (devkitPro's port, `pushbuf.c`) then kicks the chunk (`nvGpuChannelKickoff`, an ioctl = an IPC
   round trip to nvservices) and maps the next chunk of its 4-chunk ring, and `nouveau_bo_map(WR)`
   -> `nouveau_bo_wait` -> `nvFenceWait(fence, -1)` blocks until the GPU retired that chunk's last
   submission (Mesa's own `nouveau_fence_wait` spins, which would count as CPU). Constant data
   (`glUniform`) is pushed inline (`nvc0_cb_bo_push`), vertex/index/uniform buffers are never mapped
   in the draw path, so no other wait exists there.
4. Two mechanisms fit the numbers, and the log cannot tell them apart:
   - **(a) push-buffer back-pressure**: the ring (2 MiB) holds ~2-3 frames of commands at
     1500 draws (~0.4 KiB per draw: inline constants for two stages, UBO rebind, draw; ~1.5 KiB per
     program switch: nvc0 re-emits the shader buffer table of all 5 stages); when the GPU is ~30 ms
     per frame the worker catches up with it and waits at chunk switches.
   - **(b) core sharing**: every helper thread runs at priority 0x2C with the render worker on
     cores 1-2 (`thread_wrap.c`), and Horizon does not time-slice equal priorities. Each time the
     worker blocks (a kick's IPC, a fence wait), another ready thread on its core runs until *it*
     blocks. In the dip windows the gap (17.7-21.4 ms) equals the CPU of the other threads plus the
     audio thread (18.1-19.4 ms) window after window; in paced windows they run in the worker's idle
     time instead (gap 2.7 ms against 11-16 ms of their CPU). The GPU being busy only 30 of 49 ms
     also argues against (a) alone: pure back-pressure would keep it saturated.
5. Therefore the next run carries the counters that separate them (commit 3bb7fb9, no behaviour
   change), and the toggle that removes (b):
   - `[cos] perf-switch nv per frame`: for the render worker and for the other threads,
     `nouveau_pushbuf_space` calls and time (chunk switches, waits included), blocking
     `nvFenceWait` calls and time, polls, kicks and their IPC time, KiB pushed per frame (from
     `nvGpuChannelAppendEntry`); the NRO links `--wrap` for the four functions (`nv_wrap.c`).
     Reading: worker fence-wait time ~ gap -> (a); fence waits ~0 and gap ~ others' CPU -> (b).
   - `[cos] perf-switch threads`: every thread over 0.3 ms of CPU per frame with role, preferred
     core, affinity mask, CPU per frame and entry point (offset for addr2line with `centollos.elf`).
   - `COS_SWITCH_CORES=isolate`: the render worker alone on core 2, every other thread (existing
     and later, the game's included) on cores 0-1. If (b), the dip's wall time should fall to
     about worker CPU (~27 ms at 1550 draws, i.e. 30 fps) with the GPU at 30 ms.
   - If (a): a larger ring (`nouveau_pushbuf_new(..., 4, 512 * 1024, ...)` in Mesa's
     `nouveau_screen.c`, lane mesa-cache's tree) and fewer bytes per draw (below).

### 7.2 What each draw makes Mesa do, and what changed

Per GX draw Dawn GL issued, and Mesa 20.1 (st/mesa + nvc0) did at the next draw:

| Dawn GL call | Mesa state dirtied | nvc0 work at draw | per frame (dip) | now |
|---|---|---|---|---|
| `glUniform1uiv` (64 B of immediates) | VS and FS constants | `nvc0_cb_bo_push`: CB_SIZE + inline data per stage | 1737 | same (now also carries the window position) |
| `glBindBufferRange(UBO)` (dynamic offset) | VS and FS UBOs | rebind all UBOs of both stages (CB_SIZE/CB_BIND) and `cb_dirty` -> `MEM_BARRIER 0x1011` (instruction, data and constant cache invalidation on the GPU) | 1549 | ~1 per 20-30 draws (`COS_SWITCH_GL_UBO_WINDOW`) |
| texture/sampler binds (changed group 2) | sampler views/samplers | TIC/TSC validate | 294 | same |
| `SetPipeline` -> `glUseProgram` + ~25 state calls | program + every program-dependent atom (constants, UBOs, SSBOs, samplers, views) | shader switch, all stages' constbufs and textures rebound, `nvc0_validate_buffers` (~1.3 KiB) | 194 | redundant state calls and same-program `glUseProgram` skipped (`COS_SWITCH_GL_STATE_CACHE`); program switches themselves remain |

Changes (each default on, each with an env switch for the A/B, each checked on Mesa llvmpipe with
`switch/dawn/gltest`, see its README: the Mac's Dawn is Metal and cannot run these paths):

- **Uniform window** (`dawn-switch-gl-ubo-window.patch`, db6a0c5): the uniform buffer is bound for
  a 64 KiB window and Tint reads it as `array<vec4u, 4096>` plus the record's position in the
  window, an internal immediate that rides in the `glUniform1uiv` each draw makes anyway. Removes
  ~1500 `glBindBufferRange` per frame (bind groups 3.3 ms in the dip), Mesa's per-draw UBO
  re-validation (part of the `glDraw*` time) and ~1500 GPU cache invalidations. Costs: uniform
  loads become indexed loads (`LDC`) in the shaders (a few instructions per fragment), and every
  GL program is recompiled once (new GLSL; Mesa's shader cache refills). Read the "UBO binds" of
  the replay line (1549 -> ~60), the replay buckets and the GPU time.
- **Pipeline state cache** (`dawn-switch-gl-state-cache.patch`, 4a70a7d): per render pass, only the
  state calls whose value changed. "pipelines" of the replay line (1.28 ms in the dip).
- Not done: folding `vtxStart` into a base vertex. The immediates still change every draw (the
  window position, `currentPnMtx`, array starts), so one `glUniform1uiv` per draw remains either way.
  Reordering draws to cut the 194 program switches is not possible without breaking GX ordering
  (blending, alpha prepass/main pairs).

### 7.3 Loading screen and warm-up

- The loading screen presents at most `COS_PRECOMPILE_SCREEN_FPS` frames a second (default 10;
  d5dbdbc): at 60 it presented 1458 frames while its 176 pipelines took 37.1 s.
- Aurora's compile thread names itself (Switch patch 0010) and never shares the render worker's
  core (`COS_SWITCH_COMPILE_CORE=auto`, default; `off` for the old placement).
- With Mesa's shader cache (a84c009: 11-13 ms per cached pipeline) the warm-up throttle holds the
  next build back only after a build slower than `COS_PRECOMPILE_SLOW_MS` (default 25; Switch patch
  0011, 3e79acb), so a warm cache warms up back to back (~6 s for ~1100) and cache misses keep the
  duty cycle.
- With the cache warm the builds cost ~12 ms, yet the loading screen still showed for ~2.3 s on
  every start, and Mesa took 860 ms to open its cache file (2747 entries, one 32-byte read each)
  (lane/quick-boot):
  - `COS_PRECOMPILE_SCREEN=auto` (default): the warm-up starts with nothing drawn; if its first
    builds (up to 8, at most 0.3 s) average no more than `COS_PRECOMPILE_SLOW_MS`, the game starts
    at once and the priority set builds first behind the logos (fast builds back to back, a draw's
    pipeline still built on demand); if not, the loading screen as before. If the builds turn slow
    during the logo scene (at least 3, and the priority set left over 1 s at their pace) the
    loading screen comes up there. `always` restores the old behaviour, `never` skips the screen.
    Expected on a warm start: ~0.1 s of probe instead of ~2.3 s of loading screen before the logo.
  - Mesa patch 0003 keeps an index beside the cache file (`mesa_shader_cache.idx`, 40 bytes per
    record) read with one `read()` at start; expected well under 50 ms for ~3000 entries instead
    of 860 ms. The first start with it rebuilds the cache once (new driver build id). Entries
    unused for 5 runs (a shader change orphans every old entry: 5135 entries, 26.4 MiB, 2241 ms
    after the uniform window change) are pruned by a background compaction 60 s into a run
    (`mesa_shader_cache.use`, "Shader cache" in SWITCH_BUILD.md).

- The first start on hardware with a cold cache (lane/forest-gpu log, `priority` behaviour then
  called `auto`): the loading screen built the 176 priority pipelines in 10.5 s, then all 101
  hitches on the title were warm-up builds behind it ("pipeline compile ... (1)", 100-300 ms
  each; 1120 queued, ~104 ms each cold). `COS_PRECOMPILE_SCREEN=auto` now keeps the loading
  screen until the whole warm-up is built when the cache is cold or the last warm-up did not
  finish (`precompile_state.txt`), with "N/M, ~X s" from the measured pace and no throttle:
  expected ~2 min once, then no warm-up compile on the logos, title or menus. The old behaviour is
  `COS_PRECOMPILE_SCREEN=priority` (lane/cold-warmup; SWITCH_BUILD.md "Pipeline precompile").

### 7.4 A/B for the next hardware run

`env.txt` variants in `build/lanes/draw-cost/build/switch-envs/` (all at 720p, GPU profile 460):
`1-default` (everything on), `2-legacy` (window and state cache off: the old replay),
`3-nowindow`, `4-nostatecache`, `5-isolate` (`COS_SWITCH_CORES=isolate`), `6-oldwarmup` (60 fps
loading screen, every warm-up build throttled, compile thread placed as before). The first run of
`1-default` recompiles every GL program once (new GLSL): compare the second run.

## 8. The Outset forest (A_mori): GPU cost per pixel (720p handheld, GPU 460.8 MHz; lane/forest-gpu)

Log: `build/bug-reports/P1-amori/decomp-tetra.log` (NRO of feature/switch-native b5cd1f5..c1c43fa
era, `COS_FB_SCALE=1.5`, official handheld profile 0x92220008). A_mori room 0 (the forest north
of Outset where Tetra is rescued) from "stage: A_mori room 0 created at frame 34278" to frame
38880; Outset ("sea room 44") before and after.

### 8.1 What the hardware log shows

| windows | fps | draws | GPU timer ms (raw) | worker CPU | worker wall | wall / raw GPU |
|---|---|---|---|---|---|---|
| Outset 38941-40740 | 29.7-30.1 | 580-1920 | 6.8-15.5 | 10-28 | 14.3-30.6 | paced |
| A_mori 35041-35400 (good) | 29.9-30.0 | 450-610 | 18.3-20.2 | 12-14 | 14.9-33.6 | paced |
| A_mori 34321-38820 (62 windows < 29 fps) | 13.3-28.4 | 420-1780 | 20.5-45.7 | 11.7-24.5 | 25.9-74.9 | **1.61 mean, 1.64 GPU-bound** |
| A_mori 37081-37140 (worst) | 13.3 | 1035 | 45.68 (p95 48.4) | 19.4 | 74.9 | 1.64 |

The forest is slow with *fewer* draws than Outset: the cost is per pixel, not per draw. In every
slow window the render worker's wall time is 1.61-1.67 times the GPU timer's frame time,
whatever its CPU time (12-25 ms); lane draw-cost's Outset dip windows (section 7.1) show the same
ratio (42.6-43.6 ms wall for 26.0-26.5 ms of GPU, 49.1 for 30.1). A constant ratio rather than a
constant gap means the frames are GPU-bound and **the GPU timer under-reads by about 1.63**:
the Tegra X1's PTIMER is clocked at 19.2 MHz but counts as if it ran at the 31.25 MHz reference
(Linux nvgpu scales its PTIMER readings by 31.25/19.2 = 1.6276 for that reason), so nouveau's
GL_TIME_ELAPSED "ns" are 1/1.6276 of real time. `dawn-switch-gl-gpu-timer-scale.patch` (ec8160a)
now scales every result (`COS_SWITCH_GPU_TIMER_SCALE=1` gives the raw values of older logs). In
real time the forest's worst window is ~74 ms of GPU per frame, Outset's 30 fps windows 11-25 ms,
and section 7.1's 1300-1550-draw dips were GPU-bound at ~42-49 ms (the push-buffer waits and
idle worker time there were the worker waiting for the GPU), not CPU-bound.

The large "present" of the slow windows (52 ms of 75 in the worst) is the same thing: the NWindow
dequeue waits for the compositor, which waits for the GPU. `dawn-switch-gl-present-split.patch`
(d13b07a) splits it ("present per present: blit+dequeue X, swap Y") and adds
`COS_SWITCH_SWAP_INTERVAL`; `COS_SWITCH_GL_FLUSH_PASSES=1` (fe1ced7) kicks every render pass to
the GPU as it is replayed. Both were added while the frame time looked like GPU + CPU; with the
timer corrected they are checks, not expected wins.

The GPU timer's "copies" (0.5-6.9 ms in A_mori, 0.4-0.8 in Outset) are not EFB copies: the forest
copies the same four as Outset (two shadow I4 512x384 -> 256x192, the DOF Z16 and RGBA8
1280x720 -> 640x360, the latter through TexCopyConv passes). The segment holds the staging
buffer copies at the start of a frame, which on a GPU-bound frame wait for the previous frame's
draws to stop reading the vertex/uniform buffers.

### 8.2 Tools (ce7da8e)

- **Switch, GPU time per group** (`dawn-switch-gl-gpu-groups.patch`): each timer segment carries
  a group, the frame's last GX debug marker or, before one, the pass label ("EFB 3",
  "Offscreen 1", "TexCopyConv Pass"; copies "copies"). New perf-switch line
  `gpu groups per frame (ms, draws; total in n groups): name ms (draws), ...` (largest 24).
  Without markers it times each EFB pass. `COS_GPU_GROUPS=1`: a marker per draw-list bucket
  (sky, opa_bg, shadow, alpha_model, opa_p, opa, xlu_bg, xlu_p1, xlu, particle*, filter, spot,
  maskoff, motion_blur, dof, particle_proj, invisible, 2d, shadow_image). `COS_GPU_GROUPS=2`:
  also per J3D packet whose label changes ("opa|mat:<material>", or the packet class; on the
  Switch "vt+offset" from pc_gpu_group's address, resolve with `nm centollos.elf`).
- **Mac, draw census** (`native/patches/aurora/0008`, `COS_DRAW_CENSUS=<frame>[,...]`,
  `native/tools/census.py`): every GX draw of the frame with bucket/material, vertices,
  pipeline summary, textures and the fragments its shader ran for (Metal's occlusion queries are
  boolean, so with `AURORA_DRAW_CENSUS=1`, set by COS_DRAW_CENSUS, the GX fragment shaders count
  invocations and alpha-compare survivors per draw in a storage buffer); passes and EFB copies
  in a second CSV. For discarding or blended draws "shaded" is every rasterised fragment (as on
  Maxwell, where discard means late Z); for opaque ones the Mac's hidden-surface removal may skip
  hidden fragments.

Hardware model from the two logs: at 1280x720 the GPU spends ~1.5 ms (raw timer), ~2.4 ms real,
per screen of fragments (Outset 7.1 screens ~11 ms raw; A_mori 29.3 screens ~45 ms raw). A
30 fps frame (33 ms, minus the present) holds ~13 screens of 720p fragments.

### 8.3 Where the forest's fragments go (census, 1280x720, A_mori frame 399; Outset sea 44 frame 399)

| bucket (material / packet) | A_mori screens | Outset screens | what it is |
|---|---|---|---|
| filter (dKankyo_cloud_Packet) | **12.45** (81 draws) | - | the mist ("moya"): blended camera-facing sprites, no depth test, alpha compare > 0, fog |
| opa (dGrass_packet_c) | 4.68 (1082 draws) | 0.04 | grass tufts, alpha-tested, depth write |
| sky | 3.41 | 3.97 | dome + blended layers + vrkumo clouds, drawn first, mostly hidden later |
| shadow | 3.20 | 0.00 | real-time shadow volumes (alpha-only writes; the box covers the screen near the player) |
| opa_bg | 2.28 | 0.67 | the stage (leaves alpha-tested) |
| dof | 1.27 | 1.27 | depth-of-field composite (COS_DOF=0 removes it, changes the look) |
| alpha_model | 1.00 | 1.00 | drawAlphaBuffer's full-screen quad, every frame |
| total | **29.3** | **7.1** | |

Textures are not the issue: nearly all are 64x64-256x256 CMPR/I4 without mipmaps (the game's
own; adding mipmaps would change the look), anisotropy only where the game asks for it.

### 8.4 Changes

- **Mist at a quarter of the resolution, same blend** (fe8b588, `COS_MIST_LOWRES`, default 4,
  0 = old): the sprites only blend among themselves and over the scene, so S' = S*T + M with
  T = prod(1-a_i) and M the sprites over black. Offscreen target 1/n of the EFB; pass 1 the
  original blend into colour (M), pass 2 ONE/INV_SRC_ALPHA into alpha only, no fog (D = 1-T);
  copy, then ONE/INV_SRC_ALPHA over the EFB, colour only: S' = M + S(1-D). Skipped while spot
  lights or motion blur read the EFB alpha the mist no longer writes, or with a partial viewport.
  Same-frame Mac A/B (`COS_MIST_AB=<frame>`): max 6-7/255, mean ~1.5/255 at /2, /3, /4, 16:9 and
  4:3, 960x720 and 854x480. Census: mist 12.45 -> 2.56 screens, frame 29.3 -> 19.4.
- **Sky at a lower resolution** (811918e, `COS_SKY_LOWRES=2`, opt-in): the sky lists drawn into a
  1/n target over a copy of the cleared EFB and stretched back (colour only). Outset A/B at /2:
  max 24/255, mean 0.26 (horizon clouds a little softer), A_mori max 7. Sky 3.41 -> 1.85 screens.
- **Dynamic resolution** (d2b5171, `COS_DYNRES=1`, off by default): Aurora's EFB content scale
  (`native/patches/aurora/0009`, GX_AURORA_SET_CONTENT_SCALE) maps logical EFB coordinates to
  the top-left part of the EFB, so the 3D renders at 1.25 (1067x600) or 1.125 (960x540) and is
  stretched over the EFB before the 2D, which stays at 1280x720. Down one level after two
  evaluations (every 30 frames) with the 60-frame GPU p95 over 30 ms, up after four with the
  pixel-ratio-scaled p95 under 27 ms; `fixed:<scale>` and `COS_DYNRES_CYCLE` for checks.
- Considered and not done: drawing the sky after the opaque scenery with depth test (would cut the
  hidden 2-3 screens without a resolution change, but the vrkumo clouds and the sun have no depth
  compare and the shadow/alpha-buffer passes in between read the EFB; needs a forced-depth
  pipeline variant per sky draw); shadow volumes and the alpha-buffer quad (the shadow algorithm
  itself); grass (discard is needed, a depth prepass does not save shading); mipmap generation
  (changes the look).

Expected on the console from the census and the 2.4 ms-per-screen model: the forest's worst view
from ~74 ms to ~47 ms (mist) and ~44 ms (with the sky option); 30 fps there needs the 3D at
1.125 (`COS_DYNRES=1` does it on its own) or about 13 screens of fragments at 720p.

### 8.5 A/B for the next hardware run

NRO: `build/lanes/forest-gpu/build/switch-native/centollos.nro`; `env.txt` variants in
`build/lanes/forest-gpu/build/switch-envs/` (all 720p, GPU profile 460, overlay, perf lines):
`1-default` (mist /4), `2-mistoff`, `3-groups` and `4-groups-mistoff` (bucket timings with and
without the mist change), `5-groups-material`, `6-flushpasses`, `7-swap0`, `8-dynres`,
`9-dynres-fixed1125`, `10-mist2`, `11-nodof`, `12-dynres-flush`, `13-sky2`, `14-sky2-dynres`,
`15-rawtimer` (GPU timer without the PTIMER correction, to confirm the 1.63 ratio on the new
build). Read: "gpu per frame" (now real time), "gpu groups per frame", "present per present",
"[cos] dynres:" lines, fps.
