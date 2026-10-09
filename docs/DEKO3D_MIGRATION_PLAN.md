# Migrating the Switch renderer to deko3d

Plan written 2026-10-08 from `main` 8c6b16a (Aurora 3227d76 + `native/patches/aurora` 0001-0017 +
`switch/native/aurora/patches` 0001-0012, Dawn encounter/266c1cf + `switch/dawn/patches` (39),
Mesa 20.1.0-rc3 + `switch/mesa/patches` 0001-0004), the console logs in `build/switch-logs/`, and
SwitchWakerHD `main`/`dev` df8fbde, whose Switch renderer moved from OpenGL/Mesa to deko3d on
2026-10-07 (`~/Documents/SwitchWakerHD/docs/deko3d-plan.md`, `runtime/src/gfx/deko/`). Research only:
no code changed. "Measured" means a number read from a log, a document or an experiment run while
writing this; "estimated" means a projection.

## Resume here (2026-10-09)

v0.2.0 is out with the deko3d NRO (phase 6 done; picto box, disc error applet and the old
`switchwaker_dk/` folder checked/removed). **Phase 4 in progress** (section "Phase 4 progress"):
dynamic resolution fed by the deko3d timestamps, 1920x1080 window docked and GPU groups, all checked on
the console. Next: the rest of the phase 4 list (A/B per option, HD pack census, 2 x 30 minutes).

## Earlier state (end of 2026-10-08, evening)

Phases 1-3 are done and **the go/no-go checkpoint of phase 3 passes on performance** (section "Phase 3
console results"): deko3d runs the whole route at 30 fps, the render worker at 1.3-2.1 ms a frame
against GL's 10-20 ms, the GPU 30-45 % lower, no compile screen. Console testing goes over the debug
server (`scripts/switch/switchwaker_debug.py`: `deploy`, `warp N` then `press A` from the title,
`shot`, `lastlog`); the deko3d NRO is deployed as `sdmc:/switch/switchwaker/switchwaker.nro` (the
forwarder loads that path), its caches in `sdmc:/switch/switchwaker_dk/`.

**Phase 6 done (2026-10-09, local commits for review):** `switchwaker.nro` is the deko3d build in
`sdmc:/switch/switchwaker/` with its `initial_dksh_cache.bin` ("SwitchWaker" in the Homebrew Menu, what the
HOME forwarder starts); the GL NRO is `switchwaker_gl.nro` in `sdmc:/switch/switchwaker_gl/` ("SwitchWaker
(GL)", `build_native.sh --renderer gl`, which alone needs Mesa) for two releases. Build trees:
`build/switch-native` (deko3d) and `build/switch-native-gl`. `push.sh native|gl`, `make_sd.sh [--gl]`,
`switchwaker_debug.py deploy` (now also the caches next to the NRO), SWITCH_BUILD.md, README, INSTALL.md,
THIRD_PARTY.md updated. Runtime assets (docs/RUNTIME_ASSETS.md) make both NROs buildable without a disc.

Correctness rows of the checkpoint: shore foam checked by the user on the console (2026-10-09); picto
box: 39 photos in one session after the thread-reaping fix (955e16a: the crash after many photos was
leaked detached threads, renderer-independent). Still open before phase 6: 30 minutes of play, then phases 4 (parity: HD textures, dynamic resolution, docked, captures) and 5.
Upstream reports: devkitPro/deko3d#29 (alpha destination factor, worked around here), devkitPro/uam#7
(our uam patch 8) and #8 (patch 6) were closed unmerged by the maintainer on 2026-10-08 ("LLM-generated
content removed"): devkitPro does not take LLM-written contributions, so the workaround and the uam patches
stay here for good (a fix there would have to come from a person writing it themselves).

## 0. Summary

**What deko3d buys.** CPU on the render worker (no Dawn GL replay, no Mesa state validation per draw,
no GL context shared with the compile thread) and shader loading (a precompiled DKSH file loads in a
fraction of a second instead of the 8-10 minute first start and the 100-640 ms compile hitches of
today). It also drops the Mesa build (`scripts/switch/build_mesa.sh`, 2397 patch lines), the 39 Dawn
patches (5067 lines) and the Mesa program memory. SwitchWakerHD measured on the same hardware:
~20 % less total CPU per draw than GL + Mesa (render thread 7.7 -> 5.6-7.2 us/draw, with Mesa's GL
thread gone), median 28.1 -> 29.6-29.9 fps, frames at >= 28 fps 53 % -> 80-92 %, 7680 shaders
loaded in 0.26-0.29 s.

**What it does not buy.** GPU time. The forest, the open sea and the Outset dips are bound by
fragments per frame (`docs/SWITCH_PERF_STUDY.md` sections 7.1, 8, 9: ~2.4 ms of GPU per 720p screen of
fragments at 460 MHz). Those keep the levers they have now: the mist at 1/4 resolution, `COS_SKY_LOWRES`,
`COS_DYNRES`, `COS_FB_SCALE`. HD's GPU time did not change with deko3d.

**Recommendation.** Option (a) of section 3: a deko3d backend inside Aurora's `lib/gfx` layer,
keeping Aurora's WGSL shader generator and translating WGSL -> GLSL with the Tint that is already in
the NRO -> DKSH with uam, with the DKSH for every pipeline of the committed
`native/data/initial_pipeline_cache.db` built offline at build time and loaded at boot, and uam on one
background thread for the misses. GL stays buildable as a second NRO until deko3d wins on the
console. A Dawn backend for deko3d (option b) is a far larger project and keeps every problem of the
Dawn GL path that we have patched around; see 3.2.

**Decide with one measuring session first** (section 2). Go if the console shows render-worker-bound
windows in play, or if the shader-load behaviour (first start, compile hitches after a new NRO) is
judged unacceptable: the second is a product decision, the numbers to put next to it are in 2.4.
Defer if every window under 30 fps is GPU-bound and the first-start time is accepted.

| Phase | Deliverable | Effort (estimated) | Needs the console |
|---|---|---|---|
| 0 Measure | one GL measuring session on the route of 2.2, the classification of 2.3, go/no-go | 1 day + 1 console session | yes |
| 1 Shaders offline | `native/tools/dksh_cache`: pipeline DB -> WGSL -> GLSL -> DKSH on the Mac; uam vendored; every pipeline of the DB compiles | 2-3 days | no |
| 2 Device, present, overlay | deko3d device, swapchain, frame rings, present with the aspect fit, ImGui menu and loading screen, GX as no-op; orientation/depth test pattern | 3-5 days | yes (1 session) |
| 3 First playable | passes, draws, EFB copies and conversions, textures, pipeline/shader path with the offline cache; title, Outset, one dungeon drawn right. **Go/no-go checkpoint** (section 5) | 2-3 weeks | yes (2-4 sessions) |
| 4 Parity | picto box readback, HD textures, dynamic resolution, GPU timers, census/captures, shadows/DOF/mist options, sailing, 30 min stability | 1-2 weeks | yes (2-3 sessions) |
| 5 Performance | barriers, per-draw cost, uploads, descriptor churn, A/B switches | 1-2 weeks | yes |
| 6 Default | deko3d NRO is `switchwaker.nro`, GL as `switchwaker_gl.nro`; docs, `build_native.sh`, `push.sh` | 2-3 days | yes |

Total 6-9 weeks of effective work, the estimate HD's plan made for itself (`deko3d-plan.md` section 5);
HD's agents delivered P0-P5 in about a day of wall-clock time with parallel lanes and ~12 hardware
cycles, and the console cycles, not the code, set the pace. Expect the same shape here.

**Top risks** (section 7): Tint's GLSL against uam's dialect (two required rewrites are known and
verified on the host, the rest is to be checked in phase 1); uam on one thread at 70 ms mean /
430 ms p99 per shader for cache misses (the offline cache covers 3056 pipelines, HD's covered 96.2 %
of a console's shaders); deko3d 0.5.0's BC mip-level bug and uam's dual-issue scheduling (both fixed
in HD, lift the fixes); explicit barriers between the 9-13 EFB passes and the copies that feed
later passes; the ~250 MiB deko3d set-up cost against today's 315 MiB of staging buffers; debugging
without Mesa (libdeko3dd and HD's capture tool).

## 1. Where the Switch build stands

### 1.1 The stack and what it costs us

| Layer | Pin | Our code on top | Role on the Switch |
|---|---|---|---|
| Aurora | 3227d76 (`build/aurora-3227d76`) | 17 shared patches (3679 lines), 12 Switch patches (1594 lines: NWindow/EGL surface + `gl_defer` 0001, ImGui without SDL backends 0002, cache callbacks 0003, stats 0005, sqlite journal 0006, precompile 0007-0012) | GX -> WebGPU; WGSL from TEV state; render worker; pipeline cache |
| Dawn | encounter/dawn 266c1cf | `switch/dawn/dawn.cmake` (840 lines), 39 patches (5067 lines): platform and toolchain (11), context and queue (6), instrumentation (10), optimisations (12) | WebGPU -> OpenGL ES 3.2 calls; Tint WGSL -> ESSL |
| Mesa | 20.1.0-rc3, devkitPro switch-mesa 20.1.0-5 recipe, rebuilt by `build_mesa.sh` | 4 patches (2397 lines): compile stats, single-file shader cache, nvc0 codegen cache | GLES -> nouveau nvc0 -> push buffers |
| libdrm_nouveau | devkitPro package | `nv_wrap.c` counters | push-buffer submission over nvhost |

The pinned build image (`scripts/switch/Containerfile.native`, `devkitpro/devkita64@sha256:1fc388c3...`)
already ships `libdeko3d.a`, `libdeko3dd.a`, `deko3d.h/.hpp` (deko3d 0.5.0-1) and `uam` 1.1.0-1 in
`/opt/devkitpro/{libnx/lib,libnx/include,tools/bin}` (checked with `docker run --rm` on 2026-10-08).
Nothing in this repository references deko3d, uam or DKSH yet except `SWITCH_PERF_STUDY.md` option (e).

### 1.2 Measured state (handheld, CPU 1020 / GPU 460.8 MHz, 1280x720, 60-frame windows)

Medians per scene from the three console logs in `build/switch-logs/` (`native/centollos.log` and
`.prev.log` of 2026-10-06, `centollos-run1.log` of 2026-10-04 with the render-stall fix in), read
from the `[cos] perf`, `perf-switch gpu per frame` and `perf-switch cpu per frame` lines:

| Scene (`stage:room`) | windows | fps med / p10 | GPU ms med / p95 | worker CPU / busy wall ms | game thread ms |
|---|---|---|---|---|---|
| title sea (`sea_T:44`) | 22 | 30.0 / 29.9 | 8.8-13.4 / 11.3-16.8 | 7.8-10.5 / 10.1-12.8 | 8.4-10.5 |
| Outset (`sea:44`) | 94 | 30.0 / 30.0 | 17.8 / 21.5 | 16.7 / 19.5 | 15.4 |
| Windfall (`sea:11`) | 42 | 30.0 / 29.9 | 20.5 / 23.6 | 15.7 / 18.4 | 17.4 |
| Dragon Roost (`sea:13`) | 793 | 30.0 / 29.9-30.0 | 17.5-21.4 / 20.9-24.0 | 15.7-16.8 / 18.3-19.4 | 16.0-18.5 |
| Dragon Roost Cavern (`M_NewD2:0`) | 44 | 30.0 / 30.0 | 13.7 / 15.8 | 12.8 / 15.3 | 12.2 |
| Forest Haven (`Omori:0`) | 61 | 30.0 / 30.0 | 17.6 / 20.8 | 16.7 / 19.2 | 17.0 |
| Windfall shop (`Obshop:2`) | 34 | 30.0 / 30.0 | 10.8 / 13.0 | 9.9 / 12.3 | 10.8 |

Draws per frame in `centollos.log`: median 739, p90 1376, max 1689 (`dawn gl per frame` line).
The spots that drop below 30 are not in these logs; the study has them: Outset at 1300-1550 draws
20-23 fps with worker CPU 24.7-27.6 ms and GPU 42-49 ms real (section 7.1, before the mist and
dynres work), the forest 13-28 fps at 20-46 ms of GPU (section 8, since then the mist at 1/4
resolution), the open sea 27-31 ms of GPU estimated from a Mac census (section 9, untested on the
console). The whole render worker costs ~13 us of CPU per draw (section 7.1): Aurora's encode
(~6-7 us/draw) plus Dawn's GL replay and Mesa's validation (`glDraw*` 3.6-3.9 us/draw plus binds).

Shaders, same logs:

| | measured |
|---|---|
| cold start (2026-10-04, 1135 pipelines bundled then) | loading screen 180 s, 159 ms per pipeline; 45 of the session's 50 hitch lines carried a pipeline compile (max 638 ms) |
| warm start (2026-10-06, 3046-3049 pipelines) | no loading screen; warm-up 25-35 s behind the logos at 8-11 ms per pipeline; still 2-3 compile hitches per session (415-445 ms for 11-14 pipelines the bundle lacked) |
| Tint share of a build (`precompile ... tint X ms, GL context Y ms each`) | **3-6 ms**; the GL context part 6-9 ms warm, 187 ms cold |
| Mesa shader cache | 4336-4415 entries, 26 MiB, opened in 43-88 ms |
| first start for players (README) | "roughly 8 to 10 minutes" for ~3000 pipelines |

### 1.3 What SwitchWakerHD measured with deko3d (same console, 1020/460)

From `deko3d-plan.md` and the memory of its hardware runs (2026-10-07):

| | GL + Mesa (HD) | deko3d (HD) |
|---|---|---|
| render thread per draw | 6.2-8.4 us + Mesa GL thread ~2.5 us on another core | 7.7 us at first, 5.6-7.2 after P4 (lookup 0.6-1.3, indices 0.4, resources 2.7-3.5, state 1.6-2.0, submit 0.1) |
| same route, median fps / >= 28 fps | 28.1 / 53 % | 29.6-29.9 / 80-92 % |
| shader load at boot | Mesa cache 10 s (131 s before it) | 7680 DKSH in 0.26-0.29 s |
| compile of a miss | 100-300 ms on the GL context, stalls the frame | 70 ms mean, p50 49, p90 180, p99 426, max 1055 on a background thread; the draw is skipped until ready |
| heap after set-up | 1643 MiB free, 1320 after the Mesa cache | 250 MiB spent by deko3d set-up |
| GPU per frame | ~33 ms | ~33 ms (unchanged) |

## 2. Phase 0: measure first (go/no-go)

One GL session with the build as it is; no code change. Everything below exists today.

### 2.1 `env.txt`

```
COS_PERF_EVERY=60
COS_HITCH_MS=50
COS_FPS_OVERLAY=1
COS_PERF_LOG=1
```

Defaults otherwise (`COS_FB_SCALE=1.5`, `COS_SWITCH_GPU_PROFILE=460`, `COS_MIST_LOWRES=4`, `COS_DYNRES`
off). Two extra runs if time allows: `COS_SWITCH_CORES=isolate` (separates core contention from
driver time in the worker's busy wall) and a cold start after deleting `native/user/cache/` (the
first-start time and the Tint share with the full 3056-pipeline bundle). Note the mode and clocks
from the `[switch] SwitchWaker:` line; stay handheld for the whole session.

### 2.2 Route (about 15 minutes; the same route is the A/B for every later phase)

1. Title and file select (`sea_T:44`), load a file on Outset.
2. Outset (`sea:44`): the beach, the village, up to the bridge and the lookout over the village
   (the 1300-1550-draw spot of study section 7.1), into the forest `A_mori` and back.
3. Options menu > Viajar: Dragon Roost (`sea:13`), run the island; Dragon Roost Cavern
   (`M_NewD2:0`) two rooms.
4. Windfall (`sea:11`): the square by day and the night market if available; Forest Haven
   (`Omori:0`).
5. Sailing: Depuración > Navegar (barco, vela y batuta), 60 s north from Windfall.
6. Picto box: one photo (B36-B38 readback path; `[cos] capture:` and `[cos] snap:` lines).

### 2.3 What to read per 60-frame window, and the classification

| Line (`docs/SWITCH_BUILD.md`, "Run") | Fields |
|---|---|
| `[cos] perf frames a-b:` | fps, game thread ms, `begin` (= slot wait on the worker) |
| `[cos] perf-switch frames a-b:` | render worker busy ms (encode, submit, present), pipelines compiled, queued |
| `perf-switch dawn gl per frame:` | passes, draws, pipeline switches, bind groups, tex binds, execute / other work ms |
| `perf-switch dawn gl replay per frame:` | draw calls ms and us per draw (after a pipeline change / a texture bind / others) |
| `perf-switch gpu per frame:` | GPU ms, p95, max; EFB passes, tex copy conv, present, imgui, copies |
| `perf-switch cpu per frame:` | worker CPU vs busy wall, audio, other threads, mode and clocks |
| `[cos] hitch frame N:` | `pipeline compile X ms (n)`, `submit`, `execute` of the hitch frame |
| `[cos] precompile ...` | `tint X ms, GL context Y ms each`, loading screen time, slow builds |
| `[switch] shader compile:` | compiles, links, nvc0, binaries loaded / refused |

For each window with `frame = 1000 / fps`:

| Class | Rule |
|---|---|
| GPU-bound | GPU ms >= 0.9 x frame |
| worker-bound | worker busy >= 0.9 x frame and GPU < 0.8 x frame (or worker CPU >= 0.8 x frame) |
| game-bound | game thread >= 0.9 x frame and `begin` small |
| compile | a hitch line with `pipeline compile` > 0 |

A short Python script over the log does this (the one used for table 1.2 took the perf, gpu and
cpu lines per scene; phase 1 commits it as `scripts/switch/perf_scenes.py`).

### 2.4 Go / no-go

GO on performance if, on the route, **worker-bound windows are >= 20 % of the windows under
29 fps**, or **worker CPU median >= 18 ms** in any scene with the player free (that is 55 % of the
33 ms budget on one core, so a 1.5-2x draw peak, which the logs show every few minutes, is a dip).
Today's logs sit at 15.7-16.8 ms in Dragon Roost, Windfall and Forest Haven at a median of ~740
draws with peaks to 1690, so this is close; the heavy spots decide it.

GO on shader loading if the user considers unacceptable any of: the first start (8-10 min cold
today; every NRO whose shaders changed, e.g. the uniform window or clip-control change, made it
cold again), 2-3 compile hitches of 400 ms per session with the bundle, or the warm-up load behind
the logos. With deko3d the first start loads a file in well under a second, and a miss compiles
on a thread that never holds anything the render worker needs.

DEFER if every window under 29 fps is GPU-bound and the first start is accepted: then the
levers are `COS_DYNRES=1`, `COS_SKY_LOWRES=2`, a cheaper shadow volume near the camera (study
section 9) and docked clocks, and deko3d would change nothing visible.

Also record for later phases: Tint ms per build (expected 3-6 ms: it stays in the deko3d miss
path), the pipelines compiled per session beyond the bundle (the offline cache's miss rate), the
heap free after init (`[cos] heaps:` lines; the `[switch] memory:` line reports Horizon's whole
reservation, 3281 of 3285 MiB, and is useless for this), and the count of EFB passes per frame
(9-13: each becomes a barrier boundary in deko3d).

## 3. Architecture

### 3.1 What Aurora asks of WebGPU (inventory, `build/aurora-3227d76/lib`)

About 2,000 lines in ~50 files mention wgpu or WGSL (RmlUi excluded; the project builds it off).
The GX layer is already mostly backend-free: `gx/command_processor.cpp` (FIFO decode, index
generation, draw merging), `gx/regs.cpp`, `gx/fifo.cpp`, `gx/dl.cpp`, `gx/texture.cpp` (texture
and copy caches), `gx/shader_info.cpp` (uniform packing, std140-compatible), `gfx/texture_convert.cpp`
(CPU decode of GX formats to RGBA8) and the sqlite persistence of `gfx/pipeline_cache.cpp` make no
wgpu calls or only use the format enum. The backend-specific part:

| Aurora use | WebGPU today | deko3d equivalent | Where today |
|---|---|---|---|
| device, queue, surface | `g_device`, `g_queue`, surface on libnx NWindow through EGL (Switch patch 0001 + `dawn-switch-nwindow-surface.patch`); FifoRelaxed | `DkDevice` (`OriginUpperLeft | DepthZeroToOne`), one `DkQueue` Graphics, swapchain of 3 RGBA8 1280x720 images on `nwindowGetDefault()`, swap interval 1 | `webgpu/gpu.cpp` (1197 lines), `aurora.cpp:264-424` |
| frame data | 2 frame slots; 5 staging buffers of 63 MiB (`MapWrite`), split into verts 5 / uniforms 24 / indices 2 / storage 8 / texture upload 24 MiB; `CopyBufferToBuffer` into the shared device buffers before every pass (16-20 copies, 1-4 MB per frame); `MapAsync` completion is the GPU fence | per-slot CPU-visible rings (`CpuUncached | GpuCached`) that the FIFO thread writes directly; `DkFence` per slot; no copies. HD: `memory.cpp` (4 x 32 MiB stream ring, fences, command memory callback) | `gfx/frame.cpp` (746), `gfx/resources.hpp`, `encoding.cpp:353-401` |
| vertex data | vertex pulling: `vbuf` (group 0 binding 0) and `abuf` (binding 1) read-only storage, no vertex layouts; the shader decodes big-endian GX bytes | `dkCmdBufBindStorageBuffer(stage, 0/1, addr, size)` once per pass; same shaders | `gx/gx.cpp:535-609`, `gx/shader.cpp` |
| per-draw uniforms | group 1: one uniform buffer, dynamic offset, bound size 3840 (`MaxUniformSize`); offsets aligned to `minUniformBufferOffsetAlignment` (the Switch GL path binds a 64 KiB window instead, `dawn-switch-gl-ubo-window.patch`, Tint `UniformWindowOptions`) | `dkCmdBufBindUniformBuffer(stage, 0, ring + offset, 3840)`; `DK_UNIFORM_BUF_ALIGNMENT` is 0x100, WebGPU's 256: Aurora's alignment logic is unchanged; the window patch is not needed | `gx/pipeline.cpp:60-82`, `shader_info.cpp:223-396` |
| immediates | 64 B `DrawImmediateData` (`vtx_start`, `current_pnmtx`, `fog_range_base`, pad, `array_start[12]`) via `SetImmediates` (Dawn GL: `glUniform1uiv`) | a 64 B uniform at UBO slot 15 written with `dkCmdBufPushConstants` per draw (HD `WWHD_DK_UF_CACHE=2`) | `gx/gx.hpp:80`, `gx/pipeline.cpp:66` |
| textures and samplers | group 2: 8 x (`texture_2d<f32>`, filtering sampler); bind groups cached by xxh3 of the descriptor (`resource_cache.cpp:43-112`), expired after 32 unused frames; samplers cached by hash; images `RGBA8Unorm` (every GX format, CMPR included, is decoded on the CPU), BC1-7 and ASTC only for HD packs, R8/RG8 PC formats, R16Sint palette indices | `DkImage` + `DkImageView` + image/sampler descriptor sets (HD `descriptors.cpp`: 8192 images, 1024 samplers, slot free-lists tied to frame fences) and `dkCmdBufBindTextures(stage, 0, 8 handles)` with `dkMakeTextureHandle(image, sampler)`; the same cache key over the 8 handles | `gfx/texture.cpp` (343), `resource_cache.cpp` (117), `gx/gx.cpp:572-609` |
| pipelines | `RenderPipeline` = 2 shader modules + fixed state, synchronous `CreateRenderPipeline` on Aurora's own compile thread (`pipeline_cache.cpp:958`); a draw whose pipeline is not ready is skipped (priority Normal) or waits (Blocking, patch 0005) | a struct of two `DkShader` + `DkRasterizerState`, `DkDepthStencilState`, `DkColorState`, `DkColorWriteState`, `DkBlendState`, depth bias; `dkCmdBufBindShaders` + state calls behind a state cache (HD `draw.cpp`); compile thread -> uam worker | `gx/gx.cpp:379` (`build_pipeline`), `gx/pipeline.cpp:21` |
| render passes | one `CommandEncoder` per frame; `BeginRenderPass` per EFB segment (1 + one per `GXCopyTex`, offscreen target or encoder task), load ops Clear/Load, partial clears as a draw (`clear.cpp`), viewport/scissor/blend-constant commands, `SetPipeline` with redundant-bind skipping | `dkCmdBufBindRenderTargets`, `dkCmdBufClearColor/ClearDepthStencil` for Clear, viewport/scissor/blend constant; explicit `dkCmdBufBarrier(DkBarrier_Fragments, Image)` at the end of a pass that a later pass samples | `encoding.cpp` (446), `recording.cpp` (1253; targets at :179-217, :389-407, `resolve_pass_into` :826) |
| EFB and targets | colour = swapchain format at the scaled size (1280x720 at `COS_FB_SCALE=1.5`), depth `Depth32Float` reversed-Z, optional normal `RGB10A2`; copy destinations cached per (dest, size, format); pass snapshots | `RGBA8_Unorm` + `ZF32` images with `UsageRender` (HD: `HwCompression` on colour, depth compression off); content scale (patch 0009, dynres) unchanged: it is a viewport/scissor mapping | `gpu.cpp:263-420`, `gx.cpp:242-297` |
| EFB copies | never read back: `CopyTextureToTexture` or a conversion pass (`tex_copy_conv.cpp`: 16 format shaders incl. Z8/Z16 from depth, `blit` for scaling), `tex_palette_conv` for indexed copies, `GXCopyDisp` no-op | `dkCmdBufCopyImage` / `dkCmdBufBlitImage`; the conversion and palette passes as DKSH compiled offline from their fixed WGSL | `encoding.cpp:257-334`, `tex_copy_conv.cpp` (700), `tex_palette_conv.cpp` (262) |
| texture uploads | `CopyBufferToTexture` from the frame's 24 MiB region (256-byte rows), oversized uploads get their own buffer; per mip level; HD packs: BC1-7 with mips, `dawn-switch-gl-compressed-upload.patch` for the per-level GLES path | staging ring + `dkCmdBufCopyBufferToImage` per level (HD `surfaces.cpp:229-345` ring, `upload_surface`); `image_tile_size_fix` for BC levels 6-8 block rows tall (deko3d 0.5.0 bug) | `encoding.cpp:396`, `recording.cpp:668-705` |
| present | resample pass (bilinear/area, `gpu.cpp:673`), blit to the swapchain with the aspect fit (patch 0006), ImGui pass (`imgui_impl_wgpu`), `Submit`, `Present` | HD `backend.cpp present()`: acquire, draw the picture fitted 16:9 with a linear or nearest sampler, overlay, `dkQueuePresentImage`; ImGui from HD `overlay_dk.cpp` | `aurora.cpp:290-424`, `imgui.cpp` (298) |
| readbacks | none in Aurora; game side `pc_capture.cpp` (picto box B36/B37, `CopyTextureToBuffer` + `MapAsync` + `WaitAny` on the worker) and `pc_shot.cpp` (`COS_SHOT`); `depth_peek.cpp` compute pass for `GXPeekZ` (the game never calls it); census occlusion/atomic counter (patch 0008, Mac debugging) | one Aurora function `gfx::read_texture(handle, rgba)` over `dkCmdBufCopyImageToBuffer` + fence (HD `guest_writeback`, `capture.cpp`), so no unit of ours touches backend types (bug B38's lesson); depth peek and census not ported | `native/src/pc/game_hooks/pc_capture.cpp:47-111`, `harness/pc_shot.cpp:226-266` |
| GPU timers | `GL_TIME_ELAPSED_EXT` per pass (`dawn-switch-gl-gpu-timer*.patch`), PTIMER x1.6276 | `dkCmdBufReportCounter(DkCounter_Timestamp)` at pass boundaries (HD `GpuPasses`, `backend.cpp:791`), the factor measured at start (HD `GpuClock`, `backend.cpp:763-787`) | `cos_switch_stats.cpp` |
| caches | `pipeline_cache.db` (configs, 2772-byte `gx::PipelineConfig` v13), `dawn_cache.db` (GL program binaries, sqlite), Mesa `mesa_shader_cache.bin` | `pipeline_cache.db` unchanged; the two binary caches replaced by two DKSH files (section 6) | `pipeline_cache.cpp` (1211), `gpu_cache.cpp` (439), `cos_shader_cache.cpp` |

Threads today: the game thread (init, `begin_frame`/`end_frame`), the "Aurora FIFO processor" (records
the frame, **creates textures, samplers and bind groups**), the "Aurora render worker" (encodes,
maps/unmaps staging, submits, presents) and the "Pipeline compilation thread". In deko3d one thread
records and submits (the render worker); image creation stays CPU-side on the FIFO thread (layout +
memory from a thread-safe allocator) but descriptor writes are commands (`dkCmdBufPushData`) and go
through the worker at the start of the pass that needs them; the compile thread becomes the uam
worker (section 3.4).

Coordinates. Aurora is reversed-Z (`gx.hpp:51`): projection row 2 negated so GX's [-w, 0] becomes
[0, w] (`shader_info.cpp:391-396`), compare functions flipped (`gx.cpp:89-109`), viewport depth
[1 - zfar, 1 - znear] (`encoding.cpp:37-49`), depth bias negated. Its clip space is WebGPU's: y up,
framebuffer origin top-left, depth [0, 1], texture row 0 at the top, no y flip anywhere, row-vector
matrices. deko3d with `DkDeviceFlags_OriginUpperLeft | DepthZeroToOne` has the same framebuffer
origin and depth range, and HD found that its clip y points up (y = +1 is row 0 with the upper-left
origin: `dkCmdBufSetViewports` uses scaleY = -h/2; `deko3d-plan.md` section 2, `draw.cpp:893-934`):
**WebGPU's and deko3d's conventions coincide**, so Aurora's shaders need no y change. The one
mismatch is Tint, which negates `gl_Position.y` for GL's lower-left origin
(`src/tint/lang/glsl/writer/raise/shader_io.cc:274`); see 3.3. Front face: WebGPU and deko3d both
define it in framebuffer coordinates with the upper-left origin, so `FrontFace::CCW` maps to
`DkFrontFace_CCW` (Dawn GL maps it to `GL_CW` because of its flip). Phase 2's test pattern confirms
all four (orientation, depth, winding, texture origin) in one picture, as HD's P1 did.

### 3.2 Options

**(a) A deko3d backend inside Aurora's gfx layer (recommended).** Replace `lib/webgpu/`,
`encoding.cpp`, the buffer part of `frame.cpp`, the target/upload part of `recording.cpp`,
`gfx/texture.cpp`, `resource_cache.cpp`, `clear`/`tex_copy_conv`/`tex_palette_conv`, ~380 of the 628
lines of `gx/gx.cpp`, `gx/pipeline.cpp`, `imgui.cpp` and the present in `aurora.cpp` with deko3d
versions (roughly 10,000 lines touched or replaced; the deko3d side is estimated at 6-8k new lines,
of which ~1.5-2k lifted from HD). Keep untouched: the GX decode, recording structure
(`FramePacket`, `RenderPass`, `DrawCommand`), uniform packing, texture conversion and caches, the
WGSL generator, the pipeline cache persistence, the compile-thread queue and priorities, the Switch
precompile patches and every `native/src/pc` feature. The seam is a compile-time choice
(`COS_SWITCH_RENDERER=deko3d|gl`): new files live in this repository (`switch/deko/`), Aurora gets
one more Switch patch that lists them in `cmake/aurora_core.cmake` under the option and puts
`#if AURORA_GFX_DEKO3D` around the shared files' backend calls. The GL path keeps building as
`switchwaker_gl.nro` from the same tree, as HD did with `wwhd_gl.nro`.

Why this shape: Aurora's recorded frame already is a command stream with explicit passes, offsets
and bind sets, which is what deko3d consumes, and HD's renderer was a structural copy of its GL
backend with the calls swapped, in two days of lanes (`deko3d-plan.md` section 1).

**(b) A Dawn backend for deko3d.** Dawn's OpenGL backend is about 60 source files; a backend implements the
whole WebGPU object model (bind group layouts, lazy clears, resource state tracking, validation,
ticking, mapping) that Aurora only uses a corner of; Dawn's `CreateRenderPipeline` is synchronous,
so uam's 70-430 ms would run on whoever creates the pipeline unless the async path is implemented
too; immediates, dual-source blending, query sets and surface integration all need Dawn-internal
work; and it keeps the Dawn dependency, its 5067 patch lines and `dawn.cmake`. Months, with a Dawn
upstream that moves. Not recommended.

**(c) Others.** (c1) Aurora emitting GLSL directly instead of WGSL (a 2076-line rewrite of
`gx/shader.cpp` kept in sync with the Mac's WGSL): not needed while Tint translates in 3-6 ms on the
console and ~1 ms on the Mac; the fallback if Tint's output proves uam-hostile beyond the two
rewrites of 3.3. (c2) zalo's `gles-direct-submission` Aurora/Dawn interop fast path (memory,
2026-10-06): CPU only, keeps Mesa and its shader path. (c3) Vulkan: no driver on Horizon homebrew;
NVK needs the Linux nouveau uAPI (study appendix B). (c4) more Dawn GL patches: diminishing
returns after the 13 optimisation patches; the remaining per-draw cost is Mesa's validation and
nouveau's push-buffer path, which only a native API removes.

### 3.3 Shader path: WGSL -> Tint GLSL -> uam -> DKSH

```
gx::PipelineConfig (2772 B, v13)  --build_shader_source (gx/shader.cpp)-->  WGSL
  --Tint glsl writer (Dawn source, already in the NRO)-->  GLSL 4.60 (or ESSL 3.20)
  --post-pass (sampler bindings, immediates block)-->  uam GLSL
  --uam (uamlib: Mesa GLSL + nv50 codegen, patches 1-7 from HD)-->  DKSH (vertex, fragment)
  --code memory (dkShaderInitialize)-->  DkShader
```

Tint options, from the GL path's `ShaderModuleGL.cpp:437-458` with changes:

| Option | GL path today | deko3d |
|---|---|---|
| `version` | ES 3.2 (the context's) | `kDesktop` 4.6 preferred (uam initialises Mesa with `API_OPENGL_CORE` and GLSL 460: `R/runtime/third_party/uam/source/glsl_frontend.cpp:176-183, :335`); uam also accepted a `#version 310 es` shader on the host, so ES is the fallback |
| `depth_zero_to_one` | true (`dawn-switch-gl-clip-control.patch`) | true: no `2z - w` rewrite, the device flag does it |
| `gl_Position.y` negation | always (`shader_io.cc:274`, unconditional) | off: a 7-line Tint option next to `depth_zero_to_one` (the same function we already patch), fallback `dkCmdBufSetViewportSwizzles` with `DkSwizzle_NegativeY` (HD `draw.cpp:929`) as an A/B switch |
| `UniformWindowOptions` | on | off (dynamic offsets are native) |
| bindings | Tint `GenerateBindings` + Dawn's remap | fixed table: `vbuf`/`abuf` -> SSBO 0/1; `ubuf` -> UBO 0; textures `@binding(2i)` -> combined samplers i; immediates -> UBO 15; `disable_robustness` as today |

The post-pass (C++, ~150 lines, the HD `glsl_convert.cpp` pattern, host and Switch) rewrites two
things uam refuses (phase 1 found that the immediates come out as an array and that dual-source
blending needs a third rewrite: see "Phase 1 results" in section 5). Verified on 2026-10-08 with the
host uam 1.1.0 build in `~/Documents/uam-proto` (docker image `uam-host`) on GLSL shaped like Tint's
desktop output:

| Input | uam result |
|---|---|
| `#version 460` VS with `layout(binding = 0, std140) uniform`, `layout(binding = 1, std430) buffer`, `gl_VertexID`, `uintBitsToFloat`, flat varying | compiles (768-byte DKSH) |
| `uniform highp sampler2D tex_smp;` (Tint emits combined samplers without a layout; Dawn sets the unit with `glUniform1i`) | **"explicit binding required for sampler uniform"** -> rewrite to `layout(binding = i) uniform highp sampler2D` |
| `layout(location = 0) uniform Imm tint_immediates;` (Tint's immediates, `printer.cc:1136-1146`) | **"uniform ... in driver constbuf not supported"** -> rewrite to `layout(binding = 15, std140) uniform tint_immediates_block { Imm tint_immediates; };` |
| the rewritten pair, with `precision highp float;` | compiles (512-byte DKSH) |
| `#version 310 es` fragment shader | compiles |

Known uam semantics to check in phase 1 on every generated shader (from uam's README via HD):
integer division is lowered to float (TEV math is integer in places: compare the DKSH output on
the Mac against the WebGPU reference frames in phase 3), a dynamic component index into a UBO
vector reads `.x` (HD's FPS counter bug; grep the GLSL for `v[i]` on vec types; Aurora's swap tables
and k-colour selectors are baked at generation time so none is expected), no linking (varyings
match by `layout(location)`, which Tint emits), `gl_FragCoord` follows the device origin, dual-source
blending (`enable dual_source_blending` in Aurora's WGSL, Tint emits `GL_EXT_blend_func_extended`
with `index = 1`: if uam rejects it, Aurora's two-pipeline alpha prepass path `gx/pipeline.cpp:30-52`
is the fallback at the cost of a second draw for those pipelines), `textureSampleBias`,
`isampler2D` loads for palette indices, depth texture loads for Z copies, and the depth-peek
compute shader (not ported: the game never calls `GXPeekZ`).

Pipeline count: 3055 GX configs (one clear pipeline besides); a config yields one or two pipelines
(main plus alpha prepass) and the shader hash also covers `dstAlphaMode` and the normal
attachment: expect ~6-7k DKSH. HD's averaged 1.8 KB; Aurora's TEV shaders are longer, so plan for
~20-25 MB in code memory (a 32 MiB `DkMemBlockFlags_Code` block as HD's).

### 3.4 Threads with deko3d

| Thread | Today | deko3d |
|---|---|---|
| game | init, begin/end frame | same |
| FIFO processor | records; creates textures/bind groups | records into the frame ring; creates `DkImage` layouts and allocates image memory; queues descriptor writes |
| render worker | encode + Submit (all GL) + present | the only thread with the `DkCmdBuf`/`DkQueue`: binds targets, replays draws, copies, barriers, submits per pass or every N draws (HD: 256), presents; loads finished DKSH into code memory at frame start (budget) |
| pipeline compile thread | Tint + GL link on the shared context (stalls the worker) | WGSL + Tint + post-pass, then hands the GLSL to the **uam worker** (one thread, 8 MiB stack, priority 0x3B: 0x3C is refused by the NPDM, HD `shaders_dk.cpp:77-79`; never on the worker's core: `COS_SWITCH_COMPILE_CORE` logic kept) |
| cache writer | sqlite | appends DKSH records to `dksh_local.bin` once a second (HD `cache_writer`) |

## 4. What to lift from SwitchWakerHD

`R` = `~/Documents/SwitchWakerHD`, `deko/` = `R/runtime/src/gfx/deko/`. A = as is, B = adapt, C = pattern only.

| Piece | Source | Lines | Use here | Class |
|---|---|---|---|---|
| uam as a library with patches 1-7 (pthread mutexes, DKSH to memory, log callback, no exit/abort, resident frontend, deterministic scheduling, **no dual issue**) + `uam_api.h` (`init`, `compile(Stage, glsl) -> {ok, dksh, log}`) + CMake for host and Switch | `R/runtime/third_party/uam/` | ~Mesa-sized | `switch/uam/` (or `native/third_party/uam/`), built for the NRO and for the host tool | A |
| memory: command ring 4 x 4 MiB with the `cbAddMem` callback (`dkCmdBufClear` rewinds into the previous chunk: feed the first chunk each frame), stream ring, 64 MiB image chunks with a first-fit allocator, code block, descriptor and query memory, per-slot fences, `image_tile_size_fix` | `deko/memory.cpp` + `dk.h` constants | 330 + 225 | frame rings sized for Aurora (verts 5 / uniforms 24 / indices 2 / storage 8 MiB per slot, 2-3 slots; texture staging 32 MiB) | A |
| descriptors: image/sampler slot free-lists retired by frame, sampler cache by hash with LRU, `dkCmdBufPushData` writes + `DkInvalidateFlags_Descriptors` | `deko/descriptors.cpp` | 249 | replace `make_sampler` (Latte words) with `TextureBind::get_descriptor`'s GX wrap/filter/LOD/aniso (`gfx/texture.cpp:306`); LOD min/max swap kept | B |
| GPU sync: epoch-based hazard tracking (`syncRead`/`syncWrite` per surface), `Fragments + Image` on read-after-write, `Full` around copy-engine work, batched upload barriers, zcull epoch | `deko/gpu_sync.cpp`, `dk_sync.h` | 298 + 88 | Aurora's pass structure gives the hazard points for free (every copy ends a pass); start with a barrier per pass end and lift the lazy variant in phase 5 | A/B |
| formats: `format_can_render` / `format_can_2d` from deko3d's `format_traits.inc`; BC1-5 rows; depth entries | `deko/formats.cpp` | 235 | GX2 table dropped; map `wgpu::TextureFormat` (the enum Aurora keeps using) -> `DkImageFormat` (~20 entries: RGBA8/BGRA8 (+sRGB), R8, RG8, R16Sint, R32F, RGB10A2, Depth32Float -> ZF32, BC1-7 (+sRGB), ASTC) | B |
| shader worker, budget, caches: single uam worker with a priority queue (partner stage first, most skipped draws, foreground before background), 64 DKSH loads per frame (`WWHD_DK_SHADER_BUDGET`, 0 = the draw waits), draws with a pending shader skipped, offline + local cache files loaded whole at boot, writer thread, self-test | `deko/shaders_dk.cpp`, `dk_shaders.h` | 1408 + 161 | the queue and loading machinery; `translate`/hashes/`pack_uniforms` are Cemu-specific and drop out (Aurora's `pipeline_cache.cpp` keeps the queue of configs) | B |
| WDK1 cache file format (`'WDK1'`, version, uamId; records stage, hash, bindings, size, DKSH) and `dksh_uam_id()` | `deko/shader_files.cpp/.h` | 144 + 65 | key = Aurora's shader hash instead of the WGS1 hash; add the config version and the Tint/post-pass revision to the id | B |
| GLSL post-pass | `deko/glsl_convert.cpp` | 206 | Cemu-specific text; the pattern (version line, binding renumbering, appended binding map) for the two rewrites of 3.3 | C |
| offline cache builder: `build`/`coverage`/`dump` subcommands, per-shader timing percentiles, failures grouped by first error, read-back check; Debian container with g++/cmake/ninja/zlib | `R/tools/switch/dksh_cache/` (`dksh_cache.cpp` 215, `build.sh`, `Dockerfile`) | | `native/tools/dksh_cache`: input `initial_pipeline_cache.db` instead of `shadercache_gl.bin`; links Aurora's `gx/shader.cpp` + `shader_info.cpp` (host), Tint's GLSL writer (from the Dawn source) and uamlib | B |
| present and overlay: swapchain, resize on dock (1920x1080 docked, `appletGetOperationMode`), present with 16:9 fit and linear/nearest filter, progress bar with its own command buffer, FPS text | `deko/backend.cpp` | 1570 | the present/swapchain/resize parts; Aurora's resample pass (area/bilinear) replaces `draw_picture`'s filter; HD's grade/sRGB shader not needed | B |
| ImGui renderer | `deko/overlay_dk.cpp` + `shaders/imgui_*.glsl` | 192 | written for ImGui 1.92's `ImTextureData` protocol; the Switch build pins 1.91.9b-docking (`switch/native/CMakeLists.txt:86`): either adapt to `GetTexDataAsRGBA32` or bump ImGui. Replaces `imgui_impl_wgpu` for the options menu, loading screen and FPS panel | B |
| capture: `dkCmdBufCopyImageToBuffer` in batches into a 16 MiB CPU-cached block, PNG writer thread, BC/half/packed/depth decoders, both-sticks trigger, `WWHD_DUMP_*` | `deko/capture.cpp`, `dk_capture.h` | 945 + 70 | debugging without Mesa; also the pattern for `gfx::read_texture` (picto box, `COS_SHOT`) | B |
| GPU timestamps and PTIMER factor | `deko/backend.cpp:763-787, 791+` | ~150 | feeds the `perf-switch gpu per frame` line and `COS_DYNRES` | B |
| dynamic resolution | `deko/backend.cpp:647-755` | 110 | not needed: Aurora's content scale (patch 0009) + `pc_dynres.cpp` stay; only the GPU-time input changes | - |
| viewport orientation notes, state cache, depth bias units (deko3d doubles units like nvc0), submit every N draws | `deko/draw.cpp:893-934, 975-984, 1486` | | reference | C |
| bisect: runtime DKSH scheduling patcher, early-Z/SPH bits | `deko/bisect.cpp` | 169 | keep in the tree for the next speck-class bug | A |
| deko3d lessons | `deko3d-plan.md`, `dk.h:6-10, 56-62, 115-122` | | zcull off by default (`DkQueueFlags_DisableZcull`) until an A/B on the console; `libdeko3dd` with `cbDebug` -> fatal during development (release deko3d aborts with 2359-xxxx without calling it); `log_flush()` before every creation; `dkQueueIsInErrorState` before submit/acquire/present; `dkCmdBufCopyImage` ignores `srcRect->z`; LOD clamp swap | - |

Not reusable: `surfaces.cpp` (1853: Latte detiling, GuestRanges, sRGB twins, volume sources),
`draw.cpp` (1841: Latte registers to deko3d state, memo/combos, index conversion), the
`translate`/`pack_uniforms` parts of `shaders_dk.cpp`, `glsl_convert.cpp`'s Cemu rules.

**Licensing.** SwitchWakerHD is MPL-2.0 (`R/LICENSE`, `R/THIRD_PARTY.md`: everything not listed is
MPL-2.0; the deko files carry no per-file headers). SwitchWaker is MIT (`LICENSE`), with
`native/dsp_hle` GPL-2.0-or-later, so the binaries are GPL already (`THIRD_PARTY.md`, "Binaries"). MPL
is file-level copyleft: copied files, and our files that contain copied code, stay MPL-2.0 and must
carry the Exhibit A notice; their source must stay available with the binaries (section 3.2 of the
MPL), which a public repository satisfies; MIT files may sit beside them (a "Larger Work"), and
MPL-2.0 is compatible with the GPL binary. Do: keep lifted files in `switch/deko/hd/` with an MPL
header naming SwitchWakerHD, add the row to `THIRD_PARTY.md`, and keep our own new files MIT. Files
written from scratch by SwitchWakerHD's authors (not derived from the upstream recompilation's GL
backend) can be relicensed by those authors if they prefer one license; `memory.cpp`, `descriptors.cpp`,
`gpu_sync.cpp`, `overlay_dk.cpp`, `capture.cpp`, `shader_files.cpp` look new, `draw.cpp`,
`shaders_dk.cpp`, `surfaces.cpp`, `formats.cpp` are ports of the upstream GL/Vulkan backends. uam is
zlib (uam's files) + MIT (`mesa-imported/`): keep `R/runtime/third_party/uam/LICENSE` and the
"SwitchWakerHD patch N" markers (zlib requires altered versions to be marked); deko3d is zlib.

## 5. Phases

Conventions: every phase ends with a commit per verified step; each change that alters the
rendering has an `env.txt` switch (`COS_DK_*=0` restores the previous path) so the console A/B is
one line; the GL NRO stays buildable throughout (`build_native.sh --renderer gl`). Mac-testable
means without a console: the host tool, unit tests, the Mac build's WebGPU path for reference
frames. The console is the owner's; each "session" is one route of 2.2 with a log pulled afterwards.

### Phase 1: shaders offline (2-3 days, Mac only)

Deliverables:
- `switch/uam/` vendored from HD with patches 1-7 and the CMake that builds `uamlib` for the host
  (in the `uam-host`-style Debian container: `tools/switch/dksh_cache/Dockerfile`) and for the NRO.
- `native/tools/dksh_cache/` (host): reads `initial_pipeline_cache.db` (`pipeline_cache` rows,
  `config` blob -> `gx::PipelineConfig`), runs `build_shader_source` for the main and prepass
  variants and both stages, Tint's GLSL writer with the options of 3.3, the post-pass, `uam::compile`;
  writes `initial_dksh_cache.bin` (WDK1-derived, section 6) and a report (count, failures by first
  error, timing percentiles, bytes). Also compiles the fixed shaders (clear, 16 `tex_copy_conv`
  variants, `tex_palette_conv`, resample, present blit, ImGui) into an embedded table. Shards across
  processes (uam is not thread-safe; HD's host p50 is 9 ms per shader, so ~1-2 minutes for ~6.6k).
- The Tint option for the y negation (a patch in `switch/dawn/patches/`, like `dawn-switch-gl-clip-control.patch`).
- `scripts/switch/perf_scenes.py` (the phase 0 classifier).

Verification (Mac): 100 % of the 3055 configs compile in both stages; the report lists every uam
warning; the GLSL of a sample of 20 pipelines reviewed by hand for the semantics of 3.3; a
`dksh_cache dump` of a shader disassembled with `uam`'s own tooling is sane; determinism (two runs,
identical bytes: HD's uam patch 6). Nothing on the console.

### Phase 1 results (2026-10-08, `dev`)

Built: `switch/uam/` (HD's uam with patches 1-7; `uamlib` builds on the host in the tool's Debian
13 container and with devkitA64 in the Switch build image: 180/180 objects, not linked into an NRO
yet); `switch/deko/shader_translate.{h,cpp}` (Tint options of 3.3 + post-pass, for the host tool
now and the NRO's misses later) and `switch/deko/dksh_file.{h,cpp}` (the cache format below);
`native/tools/dksh_cache/` (`build.sh` builds Tint from a copy of the Switch build's Dawn source
plus `switch/dawn/patches/dawn-switch-tint-position-y-up.patch`, Aurora's `gx/shader.cpp` and
`shader_info.cpp` from the NRO's patched copy, and uamlib, then runs the tool);
`fixed_wgsl.py` (the fixed shaders' WGSL out of Aurora and ImGui); `scripts/switch/perf_scenes.py`.
Outputs stay in `build/` (`build/dksh/initial_dksh_cache.bin`, `.report.txt`, `.fixed.inc`, and
the GLSL of every shader in `.work/glsl/`).

**Cache format** `SWDK` v1 (WDK1's shape): header {magic, version, compiler id = FNV of uam's
name + patch set, `kTranslateRevision`, `GXPipelineConfigVersion` 13 and the format version};
appendable records {kind, size, CRC32}: shader (stage, XXH3 of the GLSL uam compiled, DKSH),
module (Aurora's shader hash `xxh3(dstAlphaMode, xxh3(normalAttachment, xxh3(ShaderConfig)))` ->
vertex and fragment GLSL hashes) and named (fixed shaders, with their slot table). Keying the
DKSH by the GLSL hash stores a vertex shader shared by many modules once.

**Numbers** (`native/tools/dksh_cache/build.sh build native/data/initial_pipeline_cache.db ...`,
M-series Mac, arm64 container, 10 processes):

| | measured |
|---|---|
| input | 3055 GX configs (v13, every row's hash re-checked against its blob) |
| modules `create_pipeline` needs | 3055: 3046 None, 9 Replace (destination alpha without source alpha), 0 DualSource / alpha prepass; 1847 distinct |
| GLSL after Tint + post-pass | 1844 distinct (765 vertex, 1079 fragment) from 3694 module stages; Tint + post-pass 1.7 ms mean per stage (p99 2.6) |
| fixed shaders | 28 modules, 55 entry points (clear colour/depth, 14 copy conversions + blit, Z8, Z16, depth snapshot and its MS variant, 3 palette conversions, present resample, XFB copy, ImGui), 36 distinct DKSH |
| uam | **1880 / 1880 compiled** (773 vertex, 1107 fragment, GX + fixed): **3055 / 3055 configs** have every module in both stages, 55 / 55 fixed entry points; no failure, **no uam warning** (log captured per shader; the uam CLI prints nothing either) |
| uam per shader (host) | vertex mean 14.3 ms (p50 14.2, p90 21.4, p99 27.2, max 33.6); fragment mean 2.7 ms (p50 2.3, p90 4.7, p99 7.7, max 12.4) |
| wall | 9.4 s for the whole run (uam 1.9 s over 10 processes); 86 s for the first tool build (Tint, uamlib) |
| bytes | DKSH 6,236,160 (5.95 MiB, mean 3.3 KB, max 12 KB); the file 6.04 MiB; 18 % of the 32 MiB code block |
| DKSH headers (`dksh_cache dump`) | 1880 sane (magic, sizes, one program of the right type); GPRs vertex 5-35, fragment 4-23; no scratch memory |
| determinism | identical bytes over runs with 10, 4 and 3 processes and after a from-scratch tool build; every GX DKSH identical to the uam CLI's (one process per shader) |

The plan expected ~6.6k DKSH and 20-25 MB: deduplication by GLSL leaves 1880 and 6 MiB, and the
boot load and the miss rate's code-memory headroom are correspondingly better.

**What uam refused, and the fixes** (all in the post-pass, `kTranslateRevision` 1):

1. Sampler bindings (as 3.3 says): `layout(binding = i)` added; GX texture map i is slot i.
2. Immediates: this Tint emits `layout(location = 0) uniform uint tint_immediates[16];`, not a
   struct, and indexes it with constants (`tint_immediates[5u]`). uam: "uniform 'tint_immediates'
   in driver constbuf (c[0x1][0x000]) not supported" on the first run (all 765 vertex and the 456
   fragment shaders that read immediates). A `uint[16]` in a std140 block would have a 16-byte
   stride, so the block at slot 15 is `uvec4 tint_immediates_v[4]` (64 bytes, Aurora's
   `DrawImmediateData` as is) and each `tint_immediates[N]` becomes `tint_immediates_v[N/4].xyzw[N%4]`;
   a non-constant index fails the translation on purpose (uam would read `.x`).
3. Dual-source blending: Tint writes `#extension GL_EXT_blend_func_extended: require` also for
   desktop GLSL; uam: "extension `GL_EXT_blend_func_extended' unsupported in fragment shader".
   The line is dropped (`layout(location = 0, index = 1)` is core GLSL); the
   `--dual-source-probe 100` run compiles 100 / 100 DualSource fragment shaders, TGSI shows the
   second output as `COLOR[1]`. No config of today's database needs dual source (or the alpha
   prepass), so this only matters for future rows; whether the blend unit takes `COLOR[1]` as
   source 1 is a phase 3 console check before `g_dualSourceBlendingSupported` is set on deko3d.

**Semantics review** (20 modules sampled every 92nd of `modules.tsv`, plus greps over all 1844 GX
shaders and the TGSI of each from the uam CLI):

- Integer division: 181 vertex shaders divide (`tint_div_u32`: matrix index / 3, byte offset / 4);
  after inlining every one of the 15,397 TGSI `UDIV` has an immediate divisor, which nv50 lowers
  exactly (multiply-high), so uam's float fallback for variable divisors never applies; no
  modulo, none in fragment shaders.
- Dynamic component index into a UBO vector: none. Dynamic indices are array indices only
  (`postex_mtx[in_pnmtxidx]`, `nrm_mtx[in_pnmtxidx]` in all 765 vertex shaders, `lights[i]` in 305),
  which uam addresses indirectly; `ind_mtx[k][j]` and all immediates indices are constants.
- `textureSampleBias`: every GX sample (1881) is `texture(s, uv, clamp(bias, -16.0, 15.99))`,
  TGSI `TXB`: same meaning as WebGPU.
- `isampler2D`: none in GX shaders; the palette conversion's `isampler2D` + `texelFetch` compiles.
- Depth loads: the Z8/Z16 conversions and the depth snapshot read `sampler2D` / `sampler2DMS`
  with `texelFetch(...).x`; deko3d returns (D, D, D, 1), so `.x` is right (3.3, risk 9).
- Position: every vertex shader ends `gl_Position = vec4(p.x, p.y, p.z, p.w)` (no y negation, no
  `2z - w`). `gl_VertexID` (all vertex shaders, vertex pulling) and `gl_InstanceID` (3, line and
  point expansion) match WebGPU's indices only with first vertex / first instance 0, which is how
  Aurora draws: keep it so in phase 3.
- `gl_FragCoord`: 456 fragment shaders with range fog read `abuf[imm.fog_range_base + u32(frag.x)]`
  and fog reads `frag.z`; with the upper-left origin and depth [0, 1] these are WebGPU's values.
  Phase 3 consequence: the fragment stage needs SSBO 1 (`abuf`) and the immediates UBO 15 bound,
  not only the vertex stage.
- The vertex shaders' bounds checks use `vbuf.length()` / `abuf.length()`: bind the real range
  sizes with `dkCmdBufBindStorageBuffer`, not the whole ring.
- Varyings: `layout(location = N)` on both sides, no `flat`, no name matching needed.

**Slot table for phase 3** (both stages unless noted): SSBO 0 `vbuf` (vertex), SSBO 1 `abuf`
(vertex; fragment with range fog), UBO 0 `ubuf`, UBO 15 immediates (64 bytes,
`dkCmdBufPushConstants`), combined samplers 0-7 = GX texture maps 0-7. Fixed shaders use the slots
in their named records (also printed in the report and in the `.fixed.inc` table).

**Phase 0 numbers from the existing logs** (`perf_scenes.py`, by-product): the 2026-10-04/06 logs
give table 1.2's medians (12 play windows under 29 fps: 10 compile hitches, 2 worker-bound); the
2026-10-07 logs (`build/console-logs/20261007-1043`) give Dragon Roost Cavern (`M_NewD2:0`, 16
windows) a worker CPU median of 19.6 ms at 1145 draws, above the 18 ms GO threshold of 2.4. The
measuring session of section 2 still decides.

Remaining for later phases: the NRO side of everything above (uamlib, Tint alone, the translate and
file code in the deko3d NRO; `build_native.sh` running `dksh_cache` and pushing the file; the
loader), the `.fixed.inc` table embedded where phases 2-3 draw the fixed passes, and the console
checks of dual source and of the conventions (phase 2's test pattern).

### Phase 2: device, present, overlay (3-5 days)

Deliverables: `switch/deko/` with the device (`OriginUpperLeft | DepthZeroToOne`, `cbDebug`),
queue (`DisableZcull` default), memory (HD `memory.cpp`), swapchain and present (HD `backend.cpp`),
descriptors, ImGui renderer (the options menu, loading screen and FPS panel draw as today), the
`COS_SWITCH_RENDERER` CMake option, `build_native.sh --renderer`, a `switchwaker_gl` NRO folder;
GX recording still runs but `encoding` is a no-op; a test pattern behind `COS_DK_TEST_PATTERN=1`:
four coloured corners, two depth-tested quads, a textured quad with row 0 marked, a CCW and a CW
triangle under culling.

Verification (console, one session): the pattern shows the expected orientation, depth order,
culling and texture origin (photo); the menu opens on Minus, the loading screen draws; the game
runs behind it to Outset by sound at 30 fps; 10 minutes without a deko3d error; `[cos] heaps:` and
the deko3d set-up cost logged. Mac: nothing new (the Mac build does not change).

### Phase 2 results (2026-10-08, `dev`; console session pending)

Built: `switch/deko/` (device, queue, memory, descriptors, swapchain and present, ImGui renderer, test
pattern, DKSH cache loader; adapted from HD `runtime/src/gfx/deko/` under MPL-2.0 where lifted),
Aurora Switch patch 0013 (`AURORA_GFX_DEKO3D`), `COS_SWITCH_RENDERER=gl|deko3d` and
`build_native.sh --renderer deko3d [--dk-debug-lib]`, `push.sh deko3d`, `make_sd.sh --deko3d`,
`dksh_cache check`. Mac-side checks: both NROs build; the GL NRO's `.text` and `.data` are
byte-identical to the build before the change (`.rodata` differs in one unit's `__DATE__`/`__TIME__`
only); `native/tools/regress.sh` all checks passed (the Mac build does not see any of it); the generated
cache passes `dksh_cache check` (the loader's own parse and summary) with 1882 DKSH, 1847 / 1847
modules and 57 / 57 named shaders.

How it is put together, and where it departs from the deliverables above:

- **"Encoding is a no-op" = Dawn's Null device.** Instead of `#if`s through `encoding.cpp`,
  `frame.cpp` and the resource code, the deko3d build gives Dawn its Null backend and Aurora asks
  for it (patch 0013): recording, uniform packing, pipeline creation (WGSL parsed by Tint, no GL),
  staging maps and submits all run as on the GL NRO and cost no GPU time. The Null device offers
  every feature; Aurora takes the GL adapter's set from the console logs (BC, ASTC, dual-source,
  swizzle; not `CoreFeaturesAndLimits`) so the pipeline configs, and the shared
  `pipeline_cache.db`, stay the GL NRO's. Its buffers live in the heap (the 5 x 63 MiB staging
  buffers, under the Null device's 512 MiB cap), as Mesa's did. Phase 3 replaces the Null device
  piece by piece behind the same patch.
- **Mesa is out of the deko3d NRO.** Dawn's GL backend is still compiled, but with patch 0013
  nothing references EGL, so the linker drops Mesa: 16.2 MB against the GL NRO's 21.6 MB.
- **Folders.** The plan's `switchwaker_gl` folder belongs to phase 6; until then the GL NRO keeps
  `sdmc:/switch/switchwaker/` and the deko3d NRO gets `sdmc:/switch/switchwaker_dk/` with what is
  build-specific (`switchwaker_dk.nro`, its copy of `initial_pipeline_cache.db`,
  `initial_dksh_cache.bin`: SDL's base path points there), while the disc, `native/env.txt`, saves,
  settings and Aurora's caches are the GL NRO's (`COS_SWITCH_ROOT` unchanged), and its logs are
  `native/switchwaker_dk.log` / `.prev.log` (`COS_SWITCH_NRO_NAME`). `docs/SWITCH_BUILD.md`, "The
  deko3d NRO (experimental)", has the table.
- **Memory.** Rings sized for Aurora (section 3.1), not HD's: command 3 x 4 MiB, stream 3 x 40 MiB,
  texture staging 32 MiB (allocated now so the cost is phase 3's), code 32 MiB, descriptors
  (8192 + 1024), queries, one 64 MiB image chunk (swapchain and pattern depth inside): about
  260 MiB expected; logged as `[dk] set-up cost: N MiB of heap` (heap never used before and after,
  HD's measure). The `[cos] heaps:` lines are the game's JKR heaps and come as before.
- **Shaders.** ImGui and the legend's 3x5 font are GLSL compiled by the image's uam at build time and
  embedded (HD's shaders), so the menu never depends on the cache file. The test pattern is WGSL
  (`switch/deko/shaders/dk_test_pattern.wgsl`) compiled by `dksh_cache` like Aurora's shaders
  (Tint with `disable_position_y_negation`, post-pass, uam) and drawn from the cache's named
  records with vertex pulling from SSBO 0: the photo checks deko3d's conventions *through the
  phase 3 shader path*. Without the cache file the pattern shows its legend only. The whole cache
  (1882 DKSH, ~6 MiB) is loaded into the code block at start, one `[dk] shader cache:` line.
- **Window.** 1280x720 in both modes (as the GL NRO); the docked 1080p swapchain is phase 4.

What the console session must show (the note for the owner is `build/deko3d-phase2-prueba.md`):
the pattern with red top left, green top right, blue bottom left, yellow bottom right; cyan in
front of magenta; the gray square empty (z = -0.5 clipped); the textured square's red row at the
top and green column at the left; the white counter-clockwise triangle drawn and the orange
clockwise one culled (FrontFace::CCW -> `DkFrontFace_CCW`); the legend upright. If the corners or
the texture come out flipped vertically, Tint's y handling or the device origin is wrong for phase
3; if the triangles swap, Aurora's front face maps to `DkFrontFace_CW`. Then without the pattern:
Minus opens the options menu, `COS_PRECOMPILE=full` + `COS_PRECOMPILE_SCREEN=always` shows the
loading screen, the game plays to Outset by sound at 30 fps (FPS panel), 10 minutes without a
`[dk]` error.

Open for phase 3: the GX passes, draws, copies and textures on deko3d (replacing the Null device);
uamlib and Tint alone in the NRO for misses (one uam worker at priority <= 0x3B,
`swdk::kMaxThreadPriority`), the miss path with pop-in, `dksh_local.bin`; the fixed shaders drawn
from the cache's named records (or the `.fixed.inc` table); the resample/present of the EFB with
the aspect fit; the conventions this session's photo settles; GPU timestamps for the perf lines.

### Phase 3: first playable (2-3 weeks)

Deliverables, in lanes that can run in parallel as HD's P2 did (interfaces first):
- *frame*: per-slot rings written by the FIFO thread, fences, uniform offsets, immediates via push
  constants, storage buffer binds.
- *passes and draws*: `encoding` replaced: render targets, clears, viewport/scissor/blend constant,
  pipeline state cache, `dkCmdBufBindShaders`, draws and indexed draws (u16), instanced line/point
  quads, barriers per pass end.
- *textures and copies*: images and views, uploads per level through the staging ring (RGBA8 and
  the HD pack formats), samplers, the 8-handle bind cache, EFB copies (`CopyImage`/`BlitImage`),
  the conversion and palette passes with their embedded DKSH, copy-destination and snapshot
  images, `image_tile_size_fix`.
- *shaders*: the compile thread producing GLSL, the uam worker, the two cache files, the load
  budget, the loading-screen gate (section 6), `[cos] shaders:` stats line.
- *readback*: `gfx::read_texture`; `pc_capture.cpp` and `pc_shot.cpp` switched to it on both
  renderers (the Mac keeps its WebGPU implementation behind the same function).

Verification (console, 2-4 sessions): title, file select, Outset with the player free, Dragon
Roost Cavern, Windfall; `COS_SMOKE=shore-foam` with `COS_BOOT_STAGE=sea:44:8` (bug B7's
regression, runs on the console from `env.txt`); `COS_SMOKE=picto-box`; `COS_MILESTONE=opening`;
`COS_SHOT` frames (`pc_shot.cpp`, built for the Switch too) of 6 fixed frames compared with the GL
NRO's of the same `COS_BOOT_STAGE` + `COS_INPUT` script (mean absolute difference per channel
<= 2/255 outside dynres and particles);
the route of 2.2 with the perf lines, same env, both NROs. Mac: `native/tools/regress.sh` unchanged
(the Mac path is untouched); the host tool regenerates the cache for the new NRO.

### Phase 3 results (2026-10-08, `dev`; Mac side, console sessions pending)

Built: Aurora Switch patch 0014 (`AURORA_GFX_DEKO3D`, a public define of the deko3d build now) and
`switch/deko/aurora/` (compiled into Aurora's gx library): the deko3d NRO records Aurora's frames
into deko3d. NRO `build/switch-native-dk/switchwaker_dk.nro`, 18.1 MB (16.2 in phase 2: + uamlib and
Tint's GLSL writer); the DKSH cache is unchanged (`initial_dksh_cache.bin` sha256 `5725e914`, 1882
DKSH; patch 0014 changes no shader).

How it is put together, and where it departs from the deliverables above:

- **Dawn's Null device stays Aurora's object model.** Instead of replacing `wgpu::Texture` & co. in
  Aurora's ~50 files, every texture, view, sampler and bind group Aurora creates gets a deko3d
  shadow through the linker's `--wrap` of the 12 WebGPU C functions that create them and count
  their references (`dk_objects.cpp`): a `DkImage` in the image heap (every WebGPU format, BC1-7 and
  ASTC for the HD packs, `image_tile_size_fix` on all), a `DkImageView` (levels, format, swizzle:
  the R8/RG8 PC formats) with an image descriptor once sampled, a `DkSampler` (descriptor slots by
  key with an LRU), the bind group's entries. The shadows mirror Dawn's lifetimes (a view holds its
  texture, a bind group its views and samplers); frees wait for the GPU. Aurora's recording, caches,
  uniform packing and pipeline cache are untouched; only `encode_op`, the staging buffers, the
  frame start/end items and pipeline creation take the deko3d path.
- **Frame data.** Aurora's staging slots are deko3d blocks (3 x 63 MiB, one more than the frame
  slots instead of 5) that the FIFO thread writes and the GPU reads in place, at `encoding.cpp`'s
  offsets: storage buffers 0 (`vbuf`) and 1 (`abuf`, also the fragment stage) bound once per frame
  with their regions' sizes (5 and 8 MiB, as WebGPU's whole-buffer bindings: the `length()` checks
  see the same), uniform buffer 0 at each draw's dynamic offset (3840 bytes, both stages), the
  immediates in uniform buffer 15 (both stages) by `dkCmdBufPushConstants`, u16 indices at the draw's
  offset. A slot is "mapped" again once the GPU has finished its frame (`frames_completed`, polled
  under the fence lock). The Null device's shared buffers shrink to 64 KiB, so the 5 x 63 + 39 MiB of
  heap the Null device held in phase 2 are gone; the renderer's own stream ring is 3 x 8 MiB.
- **Passes and draws** (`dk_encode.cpp`): render targets from the views' shadows, load-op clears
  (`dkCmdBufClearColorFloat`, `ClearDepthStencil`), viewport (reversed Z as `encoding.cpp`),
  scissor clamped to the target, blend constant, a state cache (shaders, the five state blocks,
  depth bias, viewport, scissor, uniform offset, texture handles, index buffer, immediates), GX
  draws (`dkCmdBufDraw` / `DrawIndexed` with first vertex/instance 0, instanced for lines and
  points, the alpha prepass) and the partial-clear draws; a `Fragments` barrier with a texture
  cache invalidate at every pass end; uploads per level (`dkCmdBufCopyBufferToImage` from the
  staging, rows of 256 bytes; oversized ones from blocks of their own) and copies on the copy engine
  between `Full` barriers (also before a frame's first transfer); EFB copies as `encoding.cpp`
  decides them (`dkCmdBufCopyImage`, or the conversion / blit draw with `tex_copy_conv_<fmt>` from
  the cache's named records), pass snapshots, depth snapshots and the palette conversions before
  their pass. The list goes to the GPU every `COS_DK_SUBMIT_DRAWS` draws (256). The depth peek and
  the draw census are not ported (plan 3.1); a runtime-registered draw type or encoder task is
  logged once and skipped (the game registers none).
- **Pipelines** (`dk_pipeline.cpp`) are built on Aurora's compile thread: the module's two DKSH from
  the caches by the module records, else WGSL (`gx::build_shader_source`) -> Tint -> post-pass ->
  the uam worker, the compile thread waiting (so a miss costs the compile thread 70-430 ms, never
  the render worker, and the warm-up's slow-build detector and the loading screen see real build
  times: section 6's gate without code of its own); the GX fixed state is `gx::pipeline_state_desc`,
  `build_pipeline`'s values. Dual-source blending only with `COS_DK_DUAL_SOURCE=1` (the alpha prepass
  otherwise) until the console shows uam's `COLOR[1]` reaches the blender; no config of today's
  database needs either.
- **Shaders at run time** (`switch/deko/shaders.cpp`): the DKSH registry by (stage, GLSL hash),
  filled from `initial_dksh_cache.bin` and `native/user/cache/dksh_local.bin` at start; one uam
  worker (8 MiB stack, priority 0x3B, kept off the render worker's core like the compile thread),
  every compile appended to `dksh_local.bin`, failures dumped as `shaderfail_<hash>_<vs|fs>.glsl`
  (16 at most); a draw whose shader is not loaded is skipped and counted (pop-in), at most
  `COS_DK_SHADER_BUDGET` (64) compiled shaders loaded per frame; `[cos] shaders:` every 15 s with the
  log writer's memory line (not per perf window: that line lives in the platform layer).
- **Present** (`dk_present.cpp`): the EFB resampled to the shown size (`present_resample`, area or
  bilinear as the menu says), then drawn with `xfb_copy` in `calculate_present_viewport`'s fitted
  viewport, under ImGui.
- **Readback.** `aurora_switch_dk_read_texture` (copy engine into CPU-visible memory, its own command
  buffer, a fence wait); `pc_capture.cpp` and `pc_shot.cpp` call it under `COS_SWITCH_DEKO3D` only.
  Deviation from the deliverable: the GL NRO and the Mac keep their inline WebGPU readback instead
  of one shared `gfx::read_texture`, because moving it would change the GL NRO's code, which this
  phase keeps byte-identical; the switch to one function belongs with phase 6.
- **GPU timestamps** every frame (`COS_DK_GPU_TIMERS=0` turns them off): at the frame start, every
  pass end, copy, present and after ImGui, read when the slot comes round, the timer factor measured
  against the CPU clock (SwitchWakerHD's GpuClock); they fill the `perf-switch gpu per frame` line,
  and the encoder's counters the `dawn gl per frame` line's passes, draws, pipelines, texture binds.
- **Conventions**, all switchable from `env.txt` and logged at start (`[dk] conventions:`), since the
  phase 2 photo has not been taken: `COS_DK_FLIP_Y=1` (game passes drawn with clip-space y negated:
  a viewport swizzle), `COS_DK_FLIP_FRONT=1` (Aurora's front face inverted), `COS_DK_FLIP_TEXTURE=1`
  (uploads written bottom row first), `COS_DK_FLIP_PRESENT=1` (the shown picture flipped). Defaults
  0: WebGPU's clip space and facing unchanged, `FrontFace::CW` -> `DkFrontFace_CW`.
- **Build.** `build_native.sh --renderer deko3d` gives the NRO a Dawn source of its own
  (`build/dawn-src-deko3d`, an APFS clone of the shared one plus `dawn-switch-tint-position-y-up.patch`)
  so that Tint in the NRO has the y option and the GL NRO's Dawn stays as it is; `dksh_cache check`
  also checks that every fixed shader the NRO draws with is in the cache.

Mac-side checks: the deko3d NRO builds and links (uamlib, Tint, the wrapped functions; no EGL
symbol left); `dksh_cache build` translated and compiled every module (1880 / 1880, as phase 1:
the NRO's miss path runs the same functions); `dksh_cache check` OK with the 57 named shaders; the
GL NRO rebuilt from this tree has `.text` and `.data` byte-identical to the phase 2 baseline
(`.rodata`: `__DATE__`/`__TIME__` only); `native/tools/regress.sh` all checks passed. Nothing has
run on the console: the session note is `build/deko3d-phase3-prueba.md` (not committed), and it also
covers phase 2's pattern photo.

Left for the console sessions and after: the conventions (the photo), every rendering check of the
verification list above, the dual-source check, the memory figures (`[dk] set-up cost`, `blocks`
in the 30 s line), the per-draw CPU of the go/no-go table, zcull and lazy barriers (phase 5).

### Go/no-go checkpoint (end of phase 3)

Continue if, on the route of 2.2 against the GL NRO of the same commit:

| Criterion | Threshold |
|---|---|
| render worker CPU per draw (worker CPU ms / draws, `perf-switch cpu` and `dawn gl per frame`-equivalent lines) | <= 70 % of GL's (GL ~13 us/draw whole worker; target <= 9 us, expected 5-7) |
| worker CPU at the 1300-1550-draw Outset spot | <= 14 ms (GL 24.7-27.6) |
| fps per scene, median and p10 | >= GL's - 0.5 everywhere; the dip spots better |
| GPU ms per frame | within +10 % of GL (same work; a larger gap means a barrier or state problem) |
| offline cache misses on the route | <= 5 % of the pipelines used (HD: 3.8 %); no worker stall on a compile |
| heap free after init | >= GL's - 100 MiB |
| correctness | the 6 captures within tolerance; shore-foam and picto-box smokes pass; 30 minutes of play without a deko3d error or a crash |

Stop and keep GL (the measuring, the host shader checks and the lifted HD pieces remain useful)
if the per-draw saving is under 30 % after the obvious fixes, if a convention
mismatch (depth, orientation, dual-source) is still open after two extra weeks, or if uam leaks
on the console (HD saw +24 KB per compile, ~31 MB over 1561 compiles, tolerable for the miss rate
expected here; a leak per draw or per frame is not).

### Phase 3 console results (2026-10-08, `dev`)

**Bugs found on the console and fixed:**
- Torch, lantern and firefly halos (light volumes, `dDlst_alphaModel`) as flickering 4x8 brown specks:
  uam gave the last instruction of a block no stall before the next block's waits (Mesa stalls 2
  there). uam patch 8 (`switch/uam/PATCHES.md`); found by bisecting with `COS_DK_SHADER_SCHED` (3 fixed
  it, 2 did not) and `COS_DK_SKIP_MODULE`.
- deko3d's `dkCmdBufBindBlendStates` programs the alpha destination factor from the colour one
  (devkitPro/deko3d#29). Pipelines whose two destination factors differ (the destination alpha
  constant: alpha `ConstAlpha / Zero` next to colour `One`, `InvSrcAlpha` or `InvDstAlpha`, 8+ modules
  on the route, ~2 draws a frame in Dragon Roost Cavern) now draw RGB, then alpha alone
  (`Pipe::alphaPass`, `dk_pipeline.cpp` `split_alpha`; logged per module). Vendoring a patched deko3d
  was dropped: the image has no `dekodef`/`dekomme`, and MME macros generated from v0.5.0 could not be
  checked against the packaged library.
- Shader code loaded during a frame: the shader caches are now invalidated before the next shader
  bind (`code_take_written`).
- The deko3d NRO pruned GL's Dawn blob cache (2.5-minute shader screen on the next GL start): its own
  `dawn_cache_dk.db` (Aurora patch 0015).

Conventions: all `COS_DK_FLIP_*` = 0 (orientation, front face, textures and present right as built).
Diagnostics kept as `[dev]` switches (`settings-dev.example.ini`): `COS_DK_TRACE_FRAME`,
`COS_DK_SKIP_MODULE`, `COS_DK_DUMP_COLOR`/`DEPTH`, `COS_DK_WGSL_PATCH`, `COS_DK_SHADER_SCHED`.

**A/B against GL** (handheld, CPU 1020 / GPU 460.8 MHz, the same warps of the Warp tab, ~45 s each:
Outset, Windfall, Dragon Roost, Dragon Roost Cavern, Forest Haven, Tower of the Gods; deko3d dev
b1437d9, GL `main` 71656e4; `perf_scenes.py`, 60-frame windows):

| scene | draws | fps med (dk / GL) | GPU ms med (dk / GL) | worker CPU ms (dk / GL) | us/draw (dk / GL) |
|---|---|---|---|---|---|
| Outset `sea:44` | 1122 | 30.0 / 30.0 | 11.0 / 17.5 | 2.1 / 19.7 | 1.8 / 17.4 |
| Windfall `sea:11` | 638 | 30.0 / 30.0 | 11.8 / 18.2 | 1.7 / 15.2 | 2.7 / 24.2 |
| Dragon Roost `sea:13` | 833 | 30.0 / 30.0 | 11.7 / 20.8 | 1.9 / 18.2 | 2.3 / 21.4 |
| Dragon Roost Cavern `M_NewD2:0` | 543 | 30.0 / 30.0 | 7.5 / 13.7 | 1.5 / 14.5 | 2.8 / 26.3 |
| Forest Haven `sea:41` | 1073 | 30.0 / 30.0 | 12.1 / 19.9 | 2.0 / 19.3 | 1.9 / 17.8 |
| Tower of the Gods `Siren:0` | 1046 | 30.0 / 30.0 | 12.7 / 21.0 | 2.1 / 19.7 | 2.0 / 18.6 |

Shader warm-up: deko3d 3095/3095 pipelines in 1.1 s behind the game; GL 154.6 s behind a loading
screen (its blob cache had been pruned, see above). No pipeline compiled during play on either. Heap
root after init 123.2 MiB free on both. Pictures equal apart from the animation (Dragon Roost, Outset,
Windfall, the Tower). Against the checkpoint table: worker CPU per draw ~10 % of GL's (threshold 70 %),
fps equal, GPU lower instead of within +10 % (expected: no Dawn/Mesa validation, fewer passes'
barriers), misses 0 %, heap equal.

### Phase 4: parity (1-2 weeks)

HD textures (`COS_HD_TEXTURES=1`, the 512 pack: BC7 with mips through the staging ring, the
publish-time estimate `COS_HD_COST_*` re-measured for the copy engine, the toggle without a freeze
as `docs/HD_TEXTURES.md` "Smooth toggling" requires); dynamic resolution fed by the deko3d
timestamps (`COS_DYNRES=1`, `fixed:1.125`); GPU timers and `COS_GPU_GROUPS` on the `perf-switch gpu`
lines (markers -> `dkCmdBufReportCounter` segments); `COS_SHADOW_OFFSCREEN`, `COS_DOF=0`,
`COS_MIST_LOWRES`, `COS_SKY_LOWRES` (offscreen targets and their copies); widescreen (`COS_ASPECT`,
only the present's fit is backend work; the game-side 16:9 code is untouched); docked: the swapchain
at 1920x1080 on dock (HD `p4-docked`) with the EFB at the per-mode `COS_FB_SCALE`; sailing
(`COS_BOOT_PRESET=sailing`); captures (`COS_DK_CAPTURE` on both sticks, HD `capture.cpp`); the
`[cos] hitch` and `perf-switch` lines filled with deko3d counters (submits, barriers, uploads,
descriptor writes, DKSH loads, pending shaders, skipped draws); `COS_SWITCH_CORES` placement of the
uam worker. Verification: each option A/B on the console in the same spot; HD pack census on
Outset and the forest (`docs/HD_TEXTURES.md` "Coverage"); 30 minutes of play per session, two sessions.

### Phase 4 progress (2026-10-09, `dev`)

Audit of the list against the code: HD textures (BC7 and the other BC formats are mapped in
`dk_objects.cpp`), the offscreen/low-res options, widescreen and captures go through Aurora and the
deko3d path already; the `perf-switch` and hitch lines read the deko3d counters
(`cos_switch_stats.cpp`). Gaps found and fixed (built, console check pending):
- dynamic resolution (`COS_DYNRES` auto) read Dawn GL's GPU timer, zero on deko3d, so it never
  moved: now `cos_switch_gfx_stats` (0409073);
- the window was 1280x720 docked too: 1920x1080 docked, swapchain recreated on a mode change,
  `COS_DK_DOCKED_1080=0` / `COS_DK_WINDOW=WxH` (c6a9f28);
- `COS_GPU_GROUPS` had no deko3d implementation: timestamp segments per marker/pass label,
  `aurora_switch_dk_gpu_groups` (c69502b).

Console results (2026-10-09, handheld title + Forbidden Woods, NRO fa5878f0): the `gpu groups` line
fills (title: sky 4-6 ms, opa_bg, conversions, dof); dynres auto moved 2.250 -> 1.250 on the deko3d
timer (forced with `COS_FB_SCALE@handheld=2.25`, `COS_DYNRES_HIGH=12`; at the normal 30 ms threshold
2.25 handheld only needs ~17 ms GPU); dock: window 1280x720 -> 1920x1080 in 45.5 ms, undock back in
38.0 ms, 30 fps on both sides, docked GPU 9.3 ms (present 1.86 ms for the 720p EFB scaled to 1080p);
the window of the dock itself had 26.4 presents/s (the system's own mode switch plus the GPU idle wait).

Options A/B (2026-10-09, handheld 1.5, NRO 0.2.0-5, same warp each, 300-frame GPU medians, all 30 fps):

| spot / option | GPU ms | note |
|---|---|---|
| Outset `sea:44` (night), defaults | 10.66 | |
| `COS_DOF=0` | 9.40 | conversions 0.72 -> 0.08 |
| `COS_SKY_LOWRES=2` | 9.91 | **bug**: Link's real-time shadow solid black (translucent with 0) |
| `COS_ASPECT=4:3` / `16:10` | 11.05 / 10.25 | pictures right |
| `COS_SHADOW_OFFSCREEN=1` / `gc` | 10.26 / 10.21 | shadow right |
| Forbidden Woods `kindan:0`, defaults (mist 1/4) | 11.84 | |
| `COS_MIST_LOWRES=0` / `2` | 11.15 / 11.92 | mist 1/4 not cheaper here than full |

Open: the black shadow with `COS_SKY_LOWRES=2` (not yet known whether GL or the Mac show it: a Mac
shot at `sea:44:206` frame 400 has no Link in view); the system Capture button on deko3d.

Remaining console checks: `COS_DYNRES=1` in a heavy spot (level changes in the log), dock/undock
(`[dk] frame N: window ... swapchain recreated`, picture and menu fill the TV), the `gpu groups`
line with groups 1 and 2, then the A/B per option and the HD pack census, 2 x 30 minutes of play.

### Phase 5: performance (1-2 weeks)

In HD's order and with HD's switches as the model (`deko3d-plan.md` P4): lazy barriers (hazard
epochs instead of a barrier per pass end: HD 47 -> 20 per frame), zcull A/B (`COS_DK_ZCULL=1`),
depth sampled in place for the DOF Z16 conversion instead of a snapshot copy
(`WWHD_DK_DEPTH_SAMPLE_BOUND` pattern), submit granularity (`COS_DK_SUBMIT_DRAWS`), descriptor and
8-handle cache hit rates, upload batching, native quads/fans if Aurora's index generation shows in
the profile (deko3d draws `DkPrimitive_Quads`/`TriangleFan` natively), per-draw cost split
(`[dk] draws per frame` style: lookup / binds / state / submit us per draw). Target: worker CPU
<= 7 us per draw at the Outset spot, and the GPU time no worse than GL's. Verification: same-spot
A/B per switch, the route, `perf_scenes.py` tables in the commit message.

### Phase 6: deko3d by default (2-3 days)

`switchwaker.nro` is the deko3d build, `switchwaker_gl.nro` the GL one (its own SD folder:
hbmenu shows one NRO per folder); `push.sh` pushes the DKSH file with the NRO; `SWITCH_BUILD.md`,
`README.md` (first start: no shader preparation), `THIRD_PARTY.md` (uam, deko3d, SwitchWakerHD),
`env.example.txt` (`COS_DK_*`); the Mesa build and the Dawn GL patches stay for the GL NRO for two
releases, then go (HD removed its GL renderer the same day; its recover point is `main` 207349b).

## 6. Shader loading design

Goal: no blocking loading screen, no compile on the render worker, every known pipeline ready
before its first draw, misses invisible beyond a brief pop-in of the draws that need them.

**Files.**

| File | Where | Written by | Contents |
|---|---|---|---|
| `initial_dksh_cache.bin` | next to the NRO (`sdmc:/switch/switchwaker/`), like `initial_pipeline_cache.db` | `native/tools/dksh_cache` at build time (`build_native.sh`, in the container; cached by the DB's and the tool's content hash) | header: magic, format version, uamId (uam version + patches + Tint/post-pass revision + `GXPipelineConfigVersion` 13); records: stage, Aurora shader hash (xxh3 of the config with the shader-relevant layout bits), size, DKSH; plus the fixed shaders |
| `native/user/cache/dksh_local.bin` | SD | the cache writer thread, one `write()` per record with a CRC (Mesa patch 0003's rules: a torn tail is cut at the next open, a bad CRC is a miss) | what this console compiled |

A file whose uamId differs from the NRO's is ignored with one log line (HD rejects the old id the
same way), so a new NRO never loads stale code. The pipeline DB keeps recording every config the
console uses, and `pull_pipeline_cache.sh` + `gen_pipeline_cache.sh --merge-only` keep growing the
bundle as today; the DKSH for the new rows is regenerated on the Mac at the next build, so the
console's `dksh_local.bin` never needs pulling.

**Boot.** `shaders_init` (render worker, before the first frame) reads both files whole into code
memory (HD: 7680 in 0.26-0.29 s from the SD card; ~6.6k expected here) and fills
`hash -> DkShader`. Aurora's `rebuild_pipeline_cache` then queues every known config as today
(Background priority, boot path first: Switch patch 0008); with the DKSH present a "build" is the
hash lookup plus the fixed-state struct, microseconds each, so the 3056-pipeline warm-up finishes
during the boot logo. `COS_PRECOMPILE`'s loading screen (`pc_precompile.cpp`, gate in
`pc_precompile_gate.h`) stays but its slow-build detector now only trips when the DKSH file is
missing or stale: with `COS_PRECOMPILE_SCREEN=auto` and the usual ~70 ms per miss, a cold console
with no file would show it for ~3-8 minutes once (3056 x 70 ms), the same order as today but
never again after a cache file is shipped; the default stays `auto` and the texts stay bilingual.

**Misses** (a config not in either file: a new pipeline, or a draw before its warm-up turn). The
compile thread (Aurora's, Normal priority first, then Background) generates WGSL, runs Tint
(3-6 ms measured on the console) and the post-pass, and queues the GLSL on the uam worker (one
thread; HD's queue order: the partner stage of a pair first, then the pipeline that skipped most
draws, foreground before background). uam takes 70 ms mean (p99 426) at 1020 MHz. The result goes
to a finished list; the render worker loads up to `COS_DK_SHADER_BUDGET` DKSH per frame at frame
start (HD default 64; each load is a copy into code memory plus `dkShaderInitialize`) and marks the
pipeline ready. Until then the draw is skipped (Aurora's existing behaviour for a pipeline that is
not ready) or, with `COS_SYNC_PIPELINES=1` / `blockingPipelines`, waits. The miss is appended to
`dksh_local.bin`. A compile failure is logged with the GLSL dumped (`shaderfail_<hash>.glsl`, HD
keeps 16) and the pipeline marked failed so the draw stays skipped rather than retried every frame.

**Code memory.** One 32 MiB `DkMemBlockFlags_Code` block, bump-allocated, never freed (HD); at
~3-4 KB per Aurora shader that holds ~8-10k shaders, so a session that compiles hundreds of misses
is fine; a `[cos] shaders:` line every perf window reports loaded, pending, failed, skipped draws
and code bytes, and the hitch line reports loads in the frame.

**Mac.** The Mac keeps WebGPU; the only Mac-side piece is the host tool, which doubles as the test
of every generated shader through uam before a console session.

## 7. Risks and open questions

Each with a recommendation; "ask" marks a decision for the user.

1. **Tint GLSL vs uam.** Verified: the two rewrites of 3.3 are needed and sufficient for the
   tested constructs. Open: dual-source blending, integer division, `textureSampleBias`, integer
   and depth texture loads on every generated shader. Phase 1 compiles all of them on the Mac and
   phase 3's captures compare the pixels. Fallbacks: ES 3.2 output (uam accepted it), the alpha
   prepass instead of dual-source, Aurora emitting GLSL (c1) as the last resort.
   Phase 1 (section 5, "Phase 1 results"): every shader of the database and every fixed shader
   compiles with three post-pass rewrites and no uam warning; the open items are console checks
   (dual-source output, the conventions).
2. **Misses on one uam thread** (70 ms mean, 430 ms p99). The committed bundle has every pipeline
   the Mac sweeps and the console play so far have seen (3056); HD's harvest covered 96.2 % of a
   console's shaders. Phase 3 measures the route's miss rate; the skip-until-ready behaviour means
   a miss costs a few frames of a missing object, never a stall. Ask: accept pop-in for unknown
   pipelines (recommended, with the bundle growing from `pull_pipeline_cache.sh`) or keep
   `blockingPipelines` as the default (a 70-430 ms stall per new pipeline, as today's 100-640 ms)?
3. **Shipping the DKSH cache.** It is derived from `initial_pipeline_cache.db` (TEV state recorded
   from the game's materials, no textures, models or code: the same provenance as the DB that is
   already committed) by Aurora's shader generator and the homebrew compiler; it contains no game
   data, so it can be shipped. Recommendation: do not commit it; `build_native.sh` generates it
   in the container in ~2 minutes and caches it in `build/`; CI could build it without a disc. Ask:
   generate at build time (recommended) or commit ~20 MB that changes with every shader change?
   Phase 1 measured 6.0 MiB (1880 DKSH after deduplication), generated in ~10 s once the tool is
   built (~90 s the first time, in its container).
4. **deko3d 0.5.0 defects** found by HD: BC images whose level 0 is 6-8 block rows tall overlap mip
   1 (hits only the HD pack path here: Aurora decodes CMPR to RGBA8), fixed by
   `image_tile_size_fix`; `dkCmdBufCopyImage` ignores `srcRect->z`; LOD clamp order; release
   library aborts without the callback. Lift all four. uam's dual-issue scheduler corrupted sunlit
   4x8-pixel warps: patch 7 (no dual issue) costs nothing measurable (HD: GPU 33.3 vs 33.5 ms).
5. **ZCULL.** Off by default in HD since the speck hunt (it was not the cause; the A/B was never
   re-run). Early-Z rejection matters for alpha-tested foliage and the forest's fragments, so a
   phase 5 A/B on the forest (`COS_DK_ZCULL=1`) is worth 1-2 ms of GPU.
6. **Barriers.** Aurora's EFB passes sample what earlier passes wrote (shadows: `GXCopyTex` I4
   then sampled by the casters' receivers; DOF: Z16 and colour copies read by the composite; mist
   and sky offscreen targets). Deko3d needs explicit `DkBarrier_Fragments` + image invalidation
   between them; a missing one shows as flicker or stale shadows (HD's 2026-10-07 speck hunt looked
   there first). Start with one barrier per pass end and per copy (9-13 per frame, ~10-30 us each),
   relax in phase 5 with HD's epochs.
7. **Memory.** Today 5 x 63 MiB staging + 39 MiB device buffers for the frame; deko3d's set-up
   cost 250 MiB in HD (queue command memory, rings, image heap chunk). Sizing the rings for Aurora's
   actual high-water (`[cos] gfx high-water:` line) rather than the 63 MiB maps should leave more
   free than today. Phase 2 logs the set-up cost; the checkpoint requires >= GL - 100 MiB.
8. **Picto box, EFB peek, `COS_SHOT`.** Three units read textures back through WebGPU today
   (`pc_capture.cpp`, `pc_shot.cpp`); with `gfx::read_texture` in Aurora they stop including
   `lib/webgpu/gpu.hpp`, which also removes the compiler-layout hazard of bug B38 for good. The
   deko3d readback is a copy-engine copy into CPU-cached memory plus a fence wait: ~1 ms for the
   shutter area, done once per photo. `GXPeekARGB` itself stays white (the game only uses the B37
   path). Depth peek (`GXPeekZ`) and the census are not ported: the game never peeks Z and the
   census is a Mac tool.
9. **Shadows, DOF, mist, sky options.** All are passes and copies through the same mechanism as
   the main EFB; the offscreen variants (`COS_SHADOW_OFFSCREEN`, `COS_MIST_LOWRES`, `COS_SKY_LOWRES`)
   add render targets and partial-clear draws. The Z16 conversion reads the depth image: a view of
   the `ZF32` image sampled as R32F (HD notes deko3d returns (D, D, D, 1) where GL returns (D, 0, 0, 1);
   Tint reads `.x`).
10. **HD texture packs.** Formats exist (BC1-7, sRGB, ASTC); uploads move from `glCompressedTexSubImage2D`
    per level to copy-engine copies per level, likely cheaper; the publish budget's cost model
    (`COS_HD_COST_TEXTURE_US` etc.) is re-measured from the copy times. The BC tile fix is mandatory
    here (64x32 and 256x32 masks are exactly the broken sizes).
11. **Dynamic resolution and widescreen.** Both live above the backend (content scale in Aurora's
    viewport mapping, 16:9 in the game and the present fit). Only the GPU time input of
    `pc_dynres.cpp` changes source.
12. **The options menu and loading screen** draw with Aurora's ImGui: one ImGui renderer (HD's,
    192 lines) adapted to ImGui 1.91.9b's atlas API, or bump the Switch pin to 1.92 and take HD's
    as is. Ask: bump ImGui on the Switch (the Mac uses Aurora's pin; the menu code
    `pc_menu.cpp` uses no 1.92-only API as far as grep shows)? Recommended: adapt the 192 lines,
    keep the pin.
13. **Tint on the console** stays for misses (already linked for Dawn GL; 3-6 ms per shader
    measured). Without Dawn, Tint is built alone from the Dawn source (`TINT_BUILD_GLSL_WRITER`;
    `dawn.cmake` already configures the source tree), a few MB of code.
14. **Debugging.** No Mesa, no GL errors: `libdeko3dd` during phases 2-4 (`COS_DK_DEBUG_LIB=ON`
    build option, as HD's), `dkQueueIsInErrorState` checks, the capture tool, the test pattern,
    captures compared with the GL NRO. The GL NRO stays the reference until phase 6.
15. **Docked 1080p.** Today the Switch window is fixed at 1280x720 (`switch/aurora/sdl3_shim`) and
    Horizon scales to the TV. With deko3d the swapchain can follow the mode (HD does); the EFB
    scale per mode is already a setting (`COS_FB_SCALE` [portátil]/[sobremesa]). Phase 4, optional.
16. **Who removes what.** Ask: after phase 6, drop the Mesa build, the Dawn patches and the
    GL-only `COS_SWITCH_GL_*` options after two releases (recommended; HD removed GL the same day
    and kept a recover-point commit), or keep both renderers indefinitely (two code paths to test
    on every change)?

## Appendix A: references

- Aurora: `build/aurora-3227d76/lib/gfx/{frame.cpp,recording.cpp,encoding.cpp,render_worker.cpp,pipeline_cache.cpp,resource_cache.cpp,texture.cpp,tex_copy_conv.cpp,tex_palette_conv.cpp,clear.cpp,depth_peek.cpp}`,
  `lib/gx/{gx.cpp,gx.hpp,pipeline.cpp,pipeline.hpp,shader.cpp,shader_info.cpp,command_processor.cpp}`,
  `lib/webgpu/gpu.cpp`, `lib/aurora.cpp:264-424`, `lib/imgui.cpp`, `include/aurora/gfx.hpp`.
- Our units touching WebGPU: `native/src/pc/game_hooks/pc_capture.cpp`, `native/src/pc/harness/pc_shot.cpp`,
  `switch/native/aurora/patches/{0001,0003,0005}`.
- Switch build: `switch/native/CMakeLists.txt` (Mesa link at :180-198, ImGui :86-104, `--wrap` list),
  `switch/dawn/dawn.cmake`, `scripts/switch/{build_native.sh,build_mesa.sh,push.sh,pull_pipeline_cache.sh,Containerfile.native}`,
  `switch/native/source/{cos_switch.cpp,cos_switch_stats.cpp,cos_shader_cache.cpp,thread_wrap.c,nv_wrap.c}`.
- Runtime: `native/src/pc/runtime/pc_frame.cpp` (perf lines :447-844, hitch :308), `features/pc_precompile.cpp`,
  `features/pc_precompile_gate.h`, `features/pc_dynres.cpp`, `features/pc_hd_textures.cpp`, `features/pc_menu.cpp`,
  `features/pc_gpu_opts.cpp`.
- Tint in the Dawn source (`build/switch-dawn-probe/_deps/dawn-src`): `src/tint/lang/glsl/writer/raise/shader_io.cc:271-287`
  (y negation, `depth_zero_to_one`), `writer/printer/printer.cc:1080-1117` (buffer bindings), `:1136-1146` (immediates),
  `writer/common/options.h`, `src/dawn/native/opengl/ShaderModuleGL.cpp:437-458` (the GL path's options),
  `src/tint/cmd/tint/main.cc:483-490, 1382-1389` (`--format glsl --glsl-desktop`).
- SwitchWakerHD: `docs/deko3d-plan.md` (sections 0-7, P4 lanes), `docs/switch-port.md`, `runtime/src/gfx/deko/*`,
  `runtime/third_party/uam/{PATCHES.md,README.md,uam_api.h,CMakeLists.txt}`, `tools/switch/dksh_cache/*`,
  `tools/switch/make_sd.py`.
- Experiments of 2026-10-08: uam host runs on Tint-shaped GLSL (section 3.3); `docker run --rm localhost/centollos-switch-native-build:2026-10-03`
  listing deko3d and uam; per-scene medians of section 1.2.
- deko3d 0.5.0 header facts (`/opt/devkitpro/libnx/include/deko3d.h` in the image): `DK_UNIFORM_BUF_ALIGNMENT 0x100`,
  `DK_UNIFORM_BUF_MAX_SIZE 0x10000`, `DK_MAX_RENDER_TARGETS 8`, `DK_MAX_VERTEX_BUFFERS 16`, `DkDeviceFlags_DepthZeroToOne | OriginUpperLeft`
  (the defaults), `DkImageFormat_{ZF32,RGBA8_Unorm_sRGB,BGRA8_Unorm,RGB10A2_Unorm,R16_Sint,R32_Float,BC*}`, `DkSwizzle_NegativeY`,
  `DkFrontFace_CCW` (default), `DkCounter_Timestamp`, `DkStage_Compute`.

## Appendix B: effort per phase, with HD's calibration

| Phase | Estimate here | HD's plan | HD's actual (wall clock, parallel lanes) |
|---|---|---|---|
| shaders offline | 2-3 d | P0 2-3 d | hours (+ one console run of 1561 shaders) |
| device/present/overlay | 3-5 d | P1 3-5 d | hours; first NRO died on priority 0x3C |
| first playable | 2-3 wk | P2 2-3 wk | one afternoon to Outset with texture bugs; evening for the BC fix |
| parity | 1-2 wk | P3 1-2 wk | one evening (captures, FPS counter, specks until the uam patch) |
| performance | 1-2 wk | P4 1-2 wk | one evening, four lanes, 7.7 -> 5.6-7.2 us/draw |
| default | 2-3 d | P5 2-3 d | same night |

HD's compression came from its GL backend being the template line by line. Here the template is
Aurora's own structure plus HD's building blocks; the shader path is new work (Tint + post-pass +
offline tool) but small. Budget the console sessions explicitly: phase 0 one, phase 2 one, phase 3
two to four, phase 4 two to three, phase 5 and 6 one each.
