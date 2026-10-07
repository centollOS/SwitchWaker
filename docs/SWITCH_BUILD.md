# Building the Switch NRO

How to build the native port for the Switch (homebrew) from your own disc, copy it to the
console and read its logs. The Mac build comes first: the Switch build reuses its inputs
([native/README.md](../native/README.md)).

> [!IMPORTANT]
> The NRO contains code built from headers generated from your disc. It is for your own console
> only: never share or upload it, the disc image or any file extracted from it. Everything below
> stays in the ignored `build/` and `ref/` folders.

## What you need

- A Mac (Apple silicon) or Linux machine with Git, Python 3, clang, CMake and Ninja.
  On macOS, Xcode's clang and `brew install cmake ninja libmtp`.
- Docker Desktop or Podman. The scripts use the official devkitPro image, pinned by digest. It runs
  natively on Apple silicon (`linux/arm64`). Give it at least 8 GB of memory (Settings › Resources).
- Your disc image of the game, GameCube USA (`GZLE01`, revision 0), as an uncompressed `.iso`.
- A Switch you have already set up for homebrew (Atmosphère and the Homebrew Menu), a USB-C data
  cable, and the console's **USB file transfer** (Horizon's own, or haze/DBI).


`switchwaker.nro` is the game built from its decompilation (`native/`) on Aurora, with Dawn's
OpenGL ES backend over the Switch's Mesa and an SDL 3 shim on libnx (plan: phase 7 of
[NATIVE_PORT_PLAN.md](NATIVE_PORT_PLAN.md)). It reads only the disc image from the SD card: the
game's code is compiled in, and the asset headers are compiled in at build time, from the same
`COS_ASSETS_DIR` as the Mac build.

## Build

Needs what the Mac build of `native/` needs (Aurora at the pin in `build/aurora-3227d76`, the asset
headers in `build/native-mac/assets/GZLE01`, `ref/recompcore`; [native/README.md](../native/README.md))
plus Docker Desktop or Podman:

```sh
scripts/switch/build_native.sh       # build/switch-native/switchwaker.nro and switchwaker.elf
```

- It builds `localhost/centollos-switch-native-build:2026-10-03` the first time
  (`scripts/switch/Containerfile.native`: the pinned devkitPro image of the translated port plus
  Debian's clang 19), then configures `switch/native` with devkitPro's Switch toolchain and builds the
  `cos_nro` target in `build/switch-native`.
- Aurora, Dawn, the SDK (`native/sdk`), Dolphin's DSP HLE and libnx compile with devkitA64's GCC; the
  game units and the run harness compile with clang 19 for the Cortex-A57
  (`switch/native/clang-launcher.sh`), as they do with Apple clang on the Mac.
- Aurora is `build/aurora-3227d76` copied into the build directory with the shared Aurora patches
  every Mac build gets (`native/patches/aurora`, decision H11) applied first, then
  `switch/native/aurora/patches` (the window surface on libnx's NWindow through Dawn's OpenGL ES backend, `gl_defer`, ImGui
  without SDL's backends, Dawn's cache callbacks, and `OSTicksToCalendarTime` on the console's
  time zone rule instead of libstdc++'s time zone database, which has no data on Horizon and
  faulted in `std::chrono::reload_tzdb` from the name scene's `dKyeff_Create`), the mechanism
  `switch/aurora` uses for the translated port. Dawn is the
  translated port's (`switch/dawn`, encounter/dawn `266c1cf` with its Horizon patches), the one that
  has presented Aurora's frames on the console; the Dawn source fetched by the translated port's Dawn
  probe (`build/switch-dawn-probe/_deps/dawn-src`) is reused when it is there.
- nod (Aurora's disc reader, written in Rust) has no libnx target: `switch/native/nod` reads plain
  GameCube `.iso` images behind nod's C API. On the Mac it gives the same file system table, metadata
  and file contents as nod for GZLE01, and a Mac `switchwaker` linked with it passes `disc-ls`, `arc-sweep`,
  `stage-sweep`, `j3d-sweep` and `opening`.
- The first build fetches Dawn's dependencies (unless the probe's source is there), SDL 3's headers,
  ImGui, Tracy, fmt, xxhash and sqlite, and compiles Dawn and the game: about 40 minutes with 4 jobs
  on a 10-core Mac when the probe's Dawn source is reused; later builds take minutes. `--jobs N` (or
  `SWITCH_BUILD_JOBS`) sets the parallel jobs, default 4. `--aurora`, `--assets`, `--recompcore` and
  `--dawn-src` point at other copies of the inputs; from a git worktree (`build/lanes/<lane>`) the
  main checkout's are used.
- Mesa (EGL, GLES and the nouveau driver) is built from source by `scripts/switch/build_mesa.sh`
  the first time (about 2 minutes; output `build/switch-mesa/prefix`, also reused from the main
  checkout by a worktree): devkitPro's `switch-mesa` 20.1.0-5 recipe (Mesa 20.1.0-rc3 and
  devkitPro's patches from `pacman-packages` `f103fe88`, sha256-checked, configured as the PKGBUILD
  does in `localhost/centollos-switch-mesa-build:2026-10-04`, `scripts/switch/Containerfile.mesa`)
  plus `switch/mesa/patches`: 0001 a newlib `timespec_get` clash of the newer devkitA64, 0002
  compile counters and timers for the log, 0003 the disk shader cache on Horizon, 0004 nvc0's code
  generation through that cache (see "Shader cache" below). `build_mesa.sh --stock` builds the
  recipe with 0001 alone (the package's global symbols, one for one); `--test` also builds Mesa's
  nouveau and OSMesa for Linux with 0002-0004 and runs `switch/mesa/test` (the cache file, nvc0's
  cached code against a fresh translation, and the GLSL cache and program binaries through the GL
  API). `build_native.sh --mesa DIR` links another prefix, `--stock-mesa` devkitPro's package from
  the image (no shader cache). libdrm_nouveau is the package's either way.
- Output: `build/switch-native/switchwaker.nro` (about 21 MB) and `build/switch-native/switchwaker.elf`, the
  same program with its symbols, for `addr2line`. Keep the ELF of the NRO you test.

## Copy to the console

```sh
scripts/switch/push.sh --disc /path/to/GZLE01.iso   # once: the disc image (skipped if already there)
scripts/switch/push.sh native                       # the NRO and the bundled pipeline cache, read back and checked by SHA-256
scripts/switch/push.sh --native-env my-env.txt      # optional: run options (see below)
scripts/switch/push.sh --pipeline-cache             # only the bundled pipeline cache (see "Pipeline precompile")
```

SD card layout:

| Path on the SD card | Contents |
|---|---|
| `switch/switchwaker/switchwaker.nro` | the app: "SwitchWaker" in the Homebrew Menu |
| `switch/switchwaker/GZLE01.iso` | your disc image |
| `switch/switchwaker/initial_pipeline_cache.db` | the pipelines to precompile at boot (`native/data/`, committed; `build_native.sh` puts a copy next to the NRO). Without it there is no warm-up and no "Preparing shaders" screen: the opening cutscene starts at once and every pipeline is built when first drawn |
| `switch/switchwaker/native/env.txt` | optional run options |
| `switch/switchwaker/native/switchwaker.log`, `switchwaker.prev.log` | this run's log and the previous one's |
| `switch/switchwaker/native/user/` | memory card (`USA/Card A`), Aurora's caches |

## Run

Start the Homebrew Menu in title mode (hold **R** while opening an installed game; an applet has far
less memory than the game needs, and the log says so) and open **SwitchWaker**. The CPU
stays at its stock 1020 MHz. With `COS_USB_LOG=1` in `env.txt` (or Depuración > "Registro en
directo por USB", at the next start; off by default, since it holds the USB port for the whole run)
and USB connected, `uv run scripts/switch/usb_log.py --out build/switch-logs/native-live.log`
shows the log live.

Run options come from `native/env.txt`, one `NAME=value` per line, with `#` comments
([switch/native/env.example.txt](../switch/native/env.example.txt)); they are the Mac's `COS_*`
variables ([native/README.md](../native/README.md), "Running switchwaker"). Without the file:
`COS_DISC=/switch/switchwaker/GZLE01.iso`, `COS_RUN_DIR=/switch/switchwaker/native`,
`COS_STALL_S=90`, `COS_ASPECT=16:9` (the widescreen
option on the 1280x720 screen; `COS_ASPECT=4:3` gives the GameCube picture, pillarboxed) and
`COS_FB_SCALE=1.5` (the internal resolution, see below). The defaults are for players: no
frame-rate panel and no perf or hitch lines in the log. For a measuring run put
`COS_PERF_EVERY=60`, `COS_HITCH_MS=50` and `COS_FPS_OVERLAY=1` in `env.txt` (the lines below
assume them).

Options menu: **Minus (−)** opens it in game (B closes it; the game pauses meanwhile). It changes
most of these options at run time and saves them to `native/user/settings.ini` on the SD card, with
separate values for handheld and docked where it says *[portátil]* / *[sobremesa]* (applied when the
console is docked or undocked). A line in `env.txt` wins over the menu's file: that row shows as
fixed (`[fijo]`); remove the line from `env.txt` to set it from the menu ([native/README.md](../native/README.md),
"Options menu"). To try sailing on an early file: Depuración > **Navegar (barco, vela y batuta)** gives
the file being played the boat, the sail (X), the wind baton (Y), the wind song and the open sea, and
puts the player on the boat next to Windfall; it stays in memory unless the game is saved
(`COS_BOOT_PRESET=sailing` in `env.txt` does the same on a fresh file at boot).

Internal resolution: `COS_FB_SCALE` is Aurora's frame-buffer scale (`VISetFrameBufferScale`, the
"internal resolution" setting of Dusklight): the game's 640x480 EFB times the scale, widened to the
screen's 16:9. `1.5` is 1280x720 (the default, the screen's own size), `1.125` is 960x540 (44 %
fewer pixels), `1.0` is 854x480 (the GameCube's vertical resolution, 56 % fewer); EFB copies
(shadows, depth of field, haze) scale with it and the present pass resamples the picture to the
screen, so the HUD and text are drawn at that resolution too. The docs/SWITCH_PERF_STUDY.md study
expects the Outset frame to be GPU-bound: if the `perf-switch gpu` line says so, a smaller scale is
the lever (`[cos] fb scale:` in the log confirms the value). The default stays 1.5 until a hardware
run decides.

What the log shows, in order (the same `[cos]` lines as on the Mac; values vary):

```
[switch] SwitchWaker, native port (phase 7); argv[0]=sdmc:/switch/switchwaker/switchwaker.nro
[switch] SwitchWaker: application (title mode); memory 3xxx MiB, ... core mask 0x7; image at 0x...
[switch] logs: /switch/switchwaker/native/switchwaker.log open
[switch] USB live log off (COS_USB_LOG=1 turns it on): the USB port is free
[cos] harness: smoke=- milestone=- timeout=0s stall=90s ...
[cos] perf: game-thread frame times every 60 frames (COS_PERF_EVERY)  <- COS_PERF_EVERY=60 only
[cos] disc: /switch/switchwaker/GZLE01.iso GZLE01 revision 0, 1459978240 bytes
[info] [aurora::gpu] Attempting to initialize OpenGLES          <- Dawn on Mesa (NV120)
[cos] aurora: backend=opengles window=1280x720 ...
[cos] dvd: GZLE01 version 0 disc 0                               <- the disc is read through nod_gcn
[cos] MILESTONE aurora-up ...
[cos] heaps: root ... check ok   (six heaps)
[cos] MILESTONE heaps ...
[cos] gfx-create: LOAD_COPYDATE status 1, COPYDATE "03/02/19 11:43:53"
[cos] MILESTONE gfx-create ...
[cos] frame loop: start, paced by JFWDisplay
[cos] audio: mDoAud_Create done at frame N; DSP handshake done
[cos] MILESTONE logo-scene ...                                   <- the boot logo is on screen
[cos] perf frames 1-60: game thread X ms avg, Y ms max (begin B, aurora_end_frame E); pace wait W ms avg; F fps, R retraces/s (60 = full speed); cpd_read C, aud_execute A, logic L, painter P; cpu U ms avg
[cos] MILESTONE frame-loop ...
[cos] logo-res: all commands synced at frame ...: 26 archives mounted, 4 files in main RAM, 0 empty
[cos] MILESTONE logo-res ...
[cos] stage: sea_T room 44 created at frame ...; Stage archive 23 files, stage.dzs found
[cos] MILESTONE opening ...                                      <- the title's sea
```

Then the game goes as far as the Mac build of the same commit: at `e0b30df` both stop in the title
demo at about frame 301 with `[cos] PANIC in ".../d_a_player_main.cpp" on line 9342` (exit 12, the
next root cause of milestone M8 on the Mac). The app ends with `[switch] exit <code> ...; ending
the process` and returns to the HOME menu (the game's threads cannot be stopped, so the process ends
instead of returning to the Homebrew Menu). Exit codes are the Mac's (native/README.md).

The `[cos] perf` lines are the speed at 1020 MHz: "game thread" is the game's own work per frame
(the frame minus the wait for the next tick), "begin" includes waiting for Aurora's render worker,
and "retraces/s" is the game's speed (60 is full speed; the game asks for a frame every one or two
retraces). The split (`mDoCPd_Read`, `mDoAud_Execute`, the `fapGm_Execute` logic, the
`mDoGph_Painter` GX encode) and "cpu" (the thread's CPU time, "n/a" if the clock is missing) are
the averages of the Mac's per-frame `COS_PERF` CSV columns (native/README.md, step 6.7), so the
two machines compare column for column.

Right after each perf line the Switch prints a `[cos] perf-switch` line (averages per frame over
the same window; `switch/native/source/cos_switch_stats.cpp` gathers the counters of Aurora's
Switch patch 0005, the Dawn GL queue patch and the disc reader):

```
[cos] perf-switch frames 61-120: begin: events E, slot wait S, staging wait T; queue-full wait Q; render worker B ms/frame busy (encode C, end_frame D: unmap U, acquire A, submit M, present P; events V), N presents/s; gl F fences (I in flight), W waits X ms, G glFinish H ms; pipelines K created, L compiled in Y ms (longest so far Z ms), J queued; tex upload KiB; dvd R reads KiB ms; res loads n; scene NAME
[cos] perf-switch dawn gl per frame: P passes, D draws, L pipelines, B bind groups, T tex binds, X texparams (Y skipped), U uniform uploads, C buffer copies K KiB, V tex uploads; flush F ms (I items): execute E, other work O, release R
[cos] perf-switch dawn gl replay per frame: pipelines P ms, bind groups B, immediates I, vertex state V, draw calls D (a after a pipeline change A ms = x us each, t after a texture bind T ms = y us each, o others O ms = z us each); u UBO binds, v VAO binds, i index binds
[cos] perf-switch dawn gl execute split per frame: passes P ms (lazy clears L, fbo setup F, default state S, clears C, pass end E, viewport/scissor/blend V, replay R, residual X); first pass xN T ms (lazy clears, fbo setup, default state, clears, pass end, replay, residual); buffer copies B ms (n before the first pass Bp ms, first copy B1 ms); m texture copies M ms; execute residual Y ms
[cos] perf-switch gpu per frame (n read back): G ms (p95 P, max M): efb passes E, tex copy conv C, present R, imgui I, copies K, other O; first pass F; d dropped, j disjoint
[cos] perf-switch cpu per frame: game thread G ms, render worker R ms (busy B ms wall), audio A, dvd D, other threads O; MODE, gpu G MHz, emc E MHz
```

The second line is Dawn's GL replay of the frame's submission
(`switch/dawn/patches/dawn-switch-gl-command-stats.patch`): with `gl_defer` every GL call of the
frame runs inside `Queue::Submit`, so "submit" above is this flush. "execute" is
`CommandBuffer::Execute` (the frame's passes, draws and copies turned into GL calls, Mesa's driver
work included); "other work" is the rest of the deferred GL work (buffer map/unmap, object
creation, buffer and texture writes); "release" is the context release at its end. The counts say
what the replay issued: draws, pipeline switches (`glUseProgram` plus the pipeline's fixed state),
bind group applications, sampled-texture binds and the `glTexParameteri` calls made while binding
them ("skipped" ones were left out because the texture object already had the value),
`glUniform` uploads of immediates, staging-to-buffer copies.
Dawn used to set a texture's base and max level and its four swizzles on every bind; Mesa 20.1
handles each swizzle `glTexParameteri` as a change (a flush, and every sampler view of the texture
dropped and rebuilt by the next draw), so `switch/dawn/patches/dawn-switch-gl-texture-params.patch`
remembers what each GL texture object has and sets only what differs.

The third line (`switch/dawn/patches/dawn-switch-gl-replay-timers.patch`, read with the CPU's
system counter) splits the render passes' part of "execute": applying pipelines, applying bind
groups (uniform/storage buffer ranges, texture and sampler binds), the `glUniform` of immediates,
vertex/index buffer and primitive-restart state, and the `glDraw*` calls. Mesa defers most of its
state validation to the draw call, so the cost of what was set before a draw shows up in the draw
call; the draws that are the first after a pipeline change and the other draws right after a
sampled-texture bind are therefore timed apart from the remaining ones, with the time per draw of
each group. The counts are the `glBindBufferRange` of uniform buffers, `glBindVertexArray` and
index buffer binds issued. The frame-rate panel (`COS_FPS_OVERLAY`) shows pipeline changes per
frame and the time of the draw calls and of the state set before them.
Dawn gave every render pipeline its own VAO, so each of the ~200 pipeline changes of an Outset
frame switched VAOs, which on Mesa 20.1 makes the next draw revalidate the vertex arrays, and
rebound the index buffer; Aurora's `SetIndexBuffer` before every draw also rebound it each time.
With `switch/dawn/patches/dawn-switch-gl-shared-vao.patch` the pipelines without vertex attributes
(all of Aurora's GX pipelines, which pull vertices from storage buffers) share one VAO, the index
buffer is rebound only when it or the VAO changes, and primitive restart is set only when it
changes ("VAO binds" and "index binds" in the third line).
The fourth line (`switch/dawn/patches/dawn-switch-gl-pass-timers.patch`, same counter) times
what the third leaves out of "execute" (docs/SWITCH_PERF_STUDY.md, section 3.4, timer 2): per
render pass, the texture synchronisation and lazy clears before it, the framebuffer set-up
(`glGenFramebuffers`, binds, attachments, `glDrawBuffers`), the default dynamic state (viewport,
scissor, depth range, blend colour), the `LoadOp::Clear` clears (`glClearBuffer*`), the pass end
(resolve, `glDeleteFramebuffers`) and the `SetViewport`/`SetScissorRect`/`SetBlendConstant`
commands; "replay" is the third line's total and "residual" what no timer of the pass covers.
"first pass" is the same split for the first render pass of each `Execute` alone (one per frame,
"xN" says how many per frame): if the frame waits for the GPU inside Mesa (nouveau reusing a
push-buffer chunk the GPU has not finished), the wait lands in the first GL calls that emit
commands, i.e. in the first pass's set-up or clears or in the buffer copies before it ("before the
first pass", "first copy"). "execute residual" is `Execute` minus its passes and copies.
The hitch line carries the same split for the hitch frame.
The fifth line (`switch/dawn/patches/dawn-switch-gl-gpu-timer.patch`, timer 3 of the study) is the
GPU's own time: a `GL_TIME_ELAPSED_EXT` query (`EXT_disjoint_timer_query`) around every render pass
and every run of copies between passes, read back a few frames later only once the results are
available (the CPU never waits for them), summed per frame and per kind of segment from Aurora's
pass labels: the EFB passes (the game's draws, the shadow segments and the DOF continuation), the
EFB copy conversions and scaled blits ("TexCopyConv"), the present pass, the ImGui pass and the
copies; "first pass" is the first render pass of each frame alone. p95 and max are over the frames
read back in the window. The present blit to the window surface (`eglSwapBuffers` side) is not
included. If Mesa does not expose the extension the line says so (and the overlay shows "gpu no
timer"); `COS_SWITCH_GPU_TIMER=0` in `env.txt` turns the queries off for an A/B run. The frame-rate
panel shows the same GPU ms per frame. If GPU ms per frame is about the frame time, the frame is
GPU-bound and the internal resolution (`COS_FB_SCALE`) is the lever.
The sixth line is each thread's CPU time per game frame from the kernel's per-thread tick count
(`svcGetInfo(InfoType_ThreadTickCount)`; `switch/native/source/thread_wrap.c` keeps the handle of
every pthread): the game thread, Aurora's render worker (all of Dawn's GL work runs on it), JAudio's
audio thread, the game's DVD thread (`mDoDvdThd`) and all other threads together (Aurora's DVD
worker, Dawn's and the log threads). A render worker CPU time well below its busy wall time (first
line) means the worker waits rather than works: for the GPU inside Mesa, or for a core another
thread holds (study, timer 4). The hitch line has the same five numbers for the hitch frame.
The line ends with the operation mode (handheld or docked), the apm performance configuration in
force and the CPU, GPU and memory controller (EMC) clocks at that moment (clkrst, or pcv before firmware 8.0.0; "clocks unavailable" if the service
refuses the app); the `[switch] SwitchWaker:` start-up line has the same. At stock the GPU runs at
307.2 or 384 MHz handheld and 768 MHz docked, so the same frame can be GPU-bound in one mode and
not in the other: always note the mode next to a measurement.
Handheld GPU profile: at start the app asks apm (`apmSetPerformanceConfiguration`, handheld =
`ApmPerformanceMode_Normal`) for one of the console's official performance configurations, CPU always at the stock
1020 MHz (switchbrew, PTM services, PerformanceConfiguration): `COS_SWITCH_GPU_PROFILE=460` (the
default; `0x92220008`, GPU 460.8 MHz, EMC 1331.2), `384` (`0x00020004`, GPU 384, EMC 1331.2),
`default` (the system's, `0x00020003`, GPU 307.2) or a raw `0x...` id. 460 falls back to 384, then
to the system's; every Result is logged (`[switch] gpu profile:`), then `[switch] clocks after the
gpu profile:` reads the clocks back. apm keeps a configuration per mode and swaps them on docking,
so docked stays at the system's 768 MHz (no official docked configuration with CPU 1020 is faster)
and nothing is re-applied. The previous handheld configuration is restored at exit and on a crash.
Title mode only (apm is the application's service). Env files for the 720p target:
`build/switch-envs/default720-460.txt` and `default720-384.txt`.
`COS_SWITCH_GL_NO_ERROR=1` in `env.txt` makes Dawn ask for a `KHR_no_error` GL context
(`switch/dawn/patches/dawn-switch-gl-no-error-context.patch`), in which Mesa skips the error
checks of every GL call, draw and uniform validation included; `[dawn] COS_SWITCH_GL_NO_ERROR:` in
the log says whether Mesa accepted it. It is an A/B option for the replay times: in such a context
a GL error has undefined results.
Depth uses WebGPU's [0, w] clip range in GL as well (on by default; bug B7,
`switch/dawn/patches/dawn-switch-gl-clip-control.patch`, `SwitchClipControlGL.h`): Dawn sets
`glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)` (`GL_EXT_clip_control`, which the console's Mesa
exposes) and Tint no longer rewrites each vertex's z as `2z - w` for GL's [-w, w] range. That
rewrite rounded the depth to about 2^-24 of the distance (Aurora uses reversed Z with the game's
near plane of 1), so decals a unit above another surface, such as Outset's shore foam, lost the
depth test in patches that flickered as the camera moved. `[dawn] COS_SWITCH_GL_CLIP_CONTROL:` in
the log says which path runs; `COS_SWITCH_GL_CLIP_CONTROL=0` in `env.txt` brings the rewrite back
(A/B). The shaders change with it, so the first run after the update rebuilds them (cold shader
cache). `COS_SMOKE=shore-foam` with `COS_BOOT_STAGE=sea:44:8` in `env.txt` runs the Mac's
regression check of the foam on the console (`[cos] shore-foam:` lines; native/README.md).
`COS_SWITCH_GL_FBO_CACHE=1` in `env.txt` (off by default; `switch/dawn/patches/dawn-switch-gl-fbo-cache.patch`,
`SwitchFboCacheGL.h`) keeps each render pass's framebuffer object, keyed by its attachments (GL
texture name, level, layer, attachment point), instead of `glGenFramebuffers`, one
`glFramebufferTexture2D` per attachment, `glDrawBuffers` and `glDeleteFramebuffers` per pass; skips
the pass's `glBindFramebuffer(GL_READ_FRAMEBUFFER, 0)`; and leaves out `glViewport`, `glScissor` and
`glDepthRangef` calls that repeat what the pass already set. A texture drops its cached framebuffers
before `glDeleteTextures` (GL names are reused). `[dawn] COS_SWITCH_GL_FBO_CACHE:` in the log
confirms it; the "fbo" share of the `execute split` line is what it saves. The game-side GPU options
`COS_SHADOW_OFFSCREEN` and `COS_DOF` are in `native/README.md` (`native/include/pc/pc_gpu_opts.h`).
GL texture and buffer names are deleted only once the GPU has finished the work submitted before
their Dawn object was destroyed (on by default; `switch/dawn/patches/dawn-switch-gl-deferred-delete.patch`,
`SwitchDeferredDeleteGL.h`): libnx's `libdrm_nouveau` waits for the GPU when Mesa frees a busy
buffer object, and Dawn frees its swapchain texture after every present, which made the next
frame's first clear wait for the whole previous frame on the GPU (`docs/SWITCH_PERF_STUDY.md`,
section 6). `COS_SWITCH_GL_DEFER_DELETE=0` deletes at once again (A/B). The
`[cos] perf-switch gl stall:` line shows the first render pass's clear time per frame (tens of us
when nothing waits) and the deferred, deleted and pending names.

"begin" of the perf line is `events` (Aurora's event pump) plus `aurora_begin_frame`, which mostly
waits for a free frame slot (the render worker still has two frames in flight: GPU-bound or
worker-bound) or for a mapped staging buffer (the GPU has not finished the frame that used it).
The render worker's busy time is what it costs to turn a frame into GL calls and present it; a
worker near the frame time means the worker, not the game thread, sets the frame rate. Dawn's
GL queue has no EGL sync extension on the console's Mesa: it used to call `glFinish` after every
submission (the CPU waited for the GPU each frame); it now puts a GLES sync object in
(`switch/dawn/patches/dawn-switch-gl-fence-queue.patch`, the "gl ... fences" count) and polls it.
`COS_SWITCH_GL_FINISH=1` in `env.txt` brings the `glFinish` back for comparison ("glFinish" count
and time). `COS_SWITCH_CORES=pinned` in `env.txt` (off by default) pins Aurora's render worker to
core 2 alone and JAudio's audio thread and the game's DVD thread to core 1 when they start
(`switch/native/source/thread_wrap.c`; `[switch] COS_SWITCH_CORES=pinned:` lines in the log): by
default every helper thread prefers core 1 or 2 in turn and Horizon does not time-slice threads of
equal priority, so the worker can wait behind the audio mixer (compare the worker's CPU time in
the `perf-switch cpu` line with and without it). Every game frame whose busy time is over `COS_HITCH_MS` (off by default; `COS_HITCH_MS=50` in
`env.txt` sets the threshold) gets one `[cos] hitch frame N: busy ... ms (wall ...): events, begin_frame, cpd, aud, logic,
painter, end_frame, other; pipelines +n (q queued), tex upload KiB, res loads +n last <path>,
scene NAME (new); switch: slot wait, staging wait, queue-full wait, worker busy (encode, submit,
present, events), gl fence wait, glFinish, pipeline compile ms (count), dvd reads; dawn gl: draws,
tex binds, texparams, execute, other work, release ms` line.

Aurora's caches (`user/cache/dawn_cache.db`, `pipeline_cache.db`) are sqlite databases. History:
they first failed their first transaction on the console with "database disk image is malformed"
on every run (cause never pinned down); `journal_mode=MEMORY` (no journal file), exclusive locking
and in-memory temp files cured that, but then a run closed with HOME (the process is killed, no
clean exit) in the middle of a commit, or of the VACUUM after the Dawn cache prune, left the file
damaged: the next run failed every lookup and insert (2026-10-04, ~40 000 `[sqlite]` lines, every
pipeline missed the cache). Now:

- **Journal** (Aurora Switch patch 0006): `journal_mode=PERSIST`, `journal_size_limit` 4 MiB,
  `synchronous=OFF`. The `-journal` file next to each cache is created once and kept open (the
  sqlite build's exclusive locking), its header zeroed at each commit; a commit cut short by a kill
  is rolled back from it at the next open. A kill is not a power loss: what `write()` handed the
  file system reaches the card, so no syncs are needed. Each cache logs
  `<path>: journal_mode=PERSIST, synchronous=OFF` when it opens. `COS_SWITCH_SQLITE_JOURNAL` in
  `env.txt` picks `persist` (default), `truncate`, `delete` or `memory` (the old setting) without a
  rebuild. A journal that does not work falls back to `memory` for the run with one line
  (`...: writing through the PERSIST journal failed (...); journal_mode=MEMORY for this run` at
  open, where a small write makes sqlite create the journal, or `a write through the persist
  journal failed` later, after which the write is tried again), so the caches and the bundled
  pipeline cache's merge keep working either way. The first PERSIST build failed every write on
  the console with `[sqlite] (1802) ... disk I/O error` (`SQLITE_IOERR_FSTAT`): sqlite stats the
  database when it creates the journal, and libnx's `stat` opens the file to read its size, which
  Horizon refuses for a file already open for writing. `switch/aurora/sqlite_horizon.c` wraps the
  VFS's open/close/stat and answers a failed stat of a file sqlite has open with `fstat` of its
  descriptor; `sqlite_kill_test.c ... persist N 1 raw` reproduces the error on the Mac with an
  emulated Horizon `stat`, `fixed` runs with the wrapper.
  `switch/aurora/sqlite_kill_test.c` measures it on the Mac with the Switch's sqlite build options
  (a writer doing what the Dawn cache does, killed with SIGKILL at random): `memory` left a damaged
  file after 113 of 150 kills, `persist`, `truncate` and `delete` after none.
- **No VACUUM** after the Dawn cache prune on the Switch: it rewrote the whole file for seconds with
  the cache's lock held (render-worker lookups waited); freed pages are reused instead, so the file
  keeps its largest size.
- **A damaged file is replaced** (shared Aurora patch `native/patches/aurora/0007`, Mac too): a
  statement that finds the file damaged (`SQLITE_CORRUPT`, `SQLITE_NOTADB`) closes the cache, deletes
  the file and its `-journal`, starts an empty cache and goes on, with one line:
  `GPU cache .../dawn_cache.db is damaged (...); deleted it (N bytes) and started an empty one`
  (or `Pipeline cache ... is damaged`); a cache that cannot be opened is deleted and created once
  more (`... could not be opened (...); deleted it (N bytes) and started an empty one`; on the
  Switch for any failure, elsewhere for a damaged file). At open, `SELECT count(*)` walks the Dawn
  cache's key index (a few pages); damage elsewhere is caught by the first lookup that meets it.
  After two resets in one run the Dawn cache is off for the rest of it. Statements are reset after
  every error (the old code left them busy: thousands of `bind on a busy prepared statement`), and
  other errors are logged rate-limited (the first three, then every 1000th).
- sqlite's own error log is in the run log as `[sqlite] (code) message` lines, which name the
  failing check (`database corruption at line N`) or system call; the first 64 are shown, then
  every 1000th with the count (`switch/aurora/sqlite_horizon.c`).

Mesa's `mesa_shader_cache.bin` ("Shader cache" below) survives a HOME kill by design: append-only,
one `write()` per record, a CRC per entry checked on every read (a damaged entry is a miss, counted
as "damaged"), and a torn last record cut at the next open ("cut N bytes of a damaged tail").

## Pipeline precompile

Every new pipeline costs 0.1-0.4 s on the console, and the game stutters for that long: Dawn's GL
backend links one GL program per pipeline on the single GL context (with `gl_defer` the render
worker waits for it). With devkitPro's `switch-mesa` package Mesa 20.1 compiled every program from
GLSL on every run (no disk cache, no program binaries); the Mesa the NRO now links keeps them
across runs ("Shader cache" below), so this is the cost of a pipeline's first build on a console.
Besides that, the port compiles fewer programs and compiles them before they are needed:

- `switch/dawn/patches/dawn-switch-gl-program-share.patch`: pipelines whose stages translate to the
  same GLSL share one linked program. Aurora's GX pipelines that differ only in blend, depth, cull or
  polygon offset state do: of the 1015 GX pipelines the Mac recorded over the boot path and every
  stage, 823 have distinct shaders (about a fifth fewer compiles).
- Aurora queues every pipeline its cache knows (`user/cache/pipeline_cache.db`) on its compile thread
  at start, in order of first use. `native/tools/gen_pipeline_cache.sh` (on the Mac, with your disc)
  records the pipelines of the logos, title and file select, a new game through the prologue,
  Outset with the player controllable and a 600-frame boot of every stage, then scripted gameplay
  on debug boots with given items (bombs on the Outset pier and in the first dungeon, sword fights
  with its chuchus and bokoblins, the boomerang, grappling hook, deku leaf, skull hammer, hookshot
  and bow), a 600-frame boot of every room that has a spawn point (`boot_sweep.py --rooms`; M_NewD2
  room 2 left out until bug B10's fix is in, `--room-skip`), an effects sweep (`COS_SMOKE=fx-sweep`:
  every particle emitter of every JPC on the disc, then each stage's own in its stage, drawn in
  front of the player) and a sweep of the disc's resources (`COS_SMOKE=res-sweep`: every BMD/BDL
  model of every archive drawn lit as an actor and as a room, every BLO screen drawn once), each a
  tier with its `--no-<name>` switch, and merges them into
  `build/pipeline-cache/initial_pipeline_cache.db` (ordered so the boot path comes first; the row
  counts per tier are in `report.txt`). Its `pipeline_priority` table marks the rows recorded on the boot path (tiers 0-3,
  logos to Outset: 176 of 2564 rows in the 2026-10-05 file, 10.7 MB: 984 rows before the gameplay,
  room, effects, model and screen tiers, which added 1580, 1041 of them from the model and screen
  sweeps) priority 0 and the rest 1. At 150-190 ms per cold build that is about 6.5-8 minutes of
  warm-up on a console whose shader cache is cold (the 176 priority ones about 30 s), about 26 s
  at the ~10 ms of a warm one;
  `gen_pipeline_cache.sh --mark-priority DB` rewrites only that table in an existing file.
  `--merge-from DB [--tier N]` (repeatable) adds another Aurora pipeline cache as one more tier
  (default: after the existing ones, priority 1), e.g. the console's own cache, which records every
  pipeline used in play (see "Growing the list from the console" below), or any Mac run's; rows
  already present keep their tier, rows of another config version than this build's Aurora writes
  are skipped and counted. `--merge-only` merges into the existing bundled file without the Mac runs.
  With the default output directory the result is copied to `native/data/initial_pipeline_cache.db`,
  the committed bundled file (commit it after a merge). `scripts/switch/build_native.sh` puts it
  next to the NRO and `scripts/switch/push.sh` pushes it with the NRO (`--pipeline-cache` alone
  pushes only it), where Aurora merges it into the player's cache at every start (`Seeded pipeline
  cache from ...`). It holds Aurora's pipeline keys (GX TEV stage and combiner selectors, vertex
  formats, blend, depth and cull state) recorded from the game's materials: no textures, models,
  text, audio or code. It is committed since 2026-10-06: a build made from a clean clone, without
  it, had no warm-up and no loading screen.
- Aurora Switch patch 0008 queues the priority-0 pipelines first (the player's own cache records
  each run's frames, so by first use alone pipelines seen anywhere in the game would come first
  after a few sessions) and counts them for the harness.
- `COS_PRECOMPILE` (in `native/env.txt`) says how long to wait and when to stop:
  - `boot` (the default): before the game starts (before the boot logo) a loading screen,
    "Preparing shaders" and a bar drawn with Aurora's ImGui, when `COS_PRECOMPILE_SCREEN` (below)
    calls for one. The screen keeps presenting frames (slowly: a build holds the GL context) and
    pumping events, so HOME works. Then the game starts and what is left is built behind the logos,
    title and menus, with "Shaders N/M" in the bottom-right corner, until the game first enters its
    PLAY scene: there the warm-up ends (Aurora Switch patch 0007) and what is left is built when
    first drawn, as before.
  - `full`: the loading screen until every known pipeline is built (1094 at about 2.5 min); nothing
    is left to stutter on a pipeline the cache knows.
  - `COS_PRECOMPILE_SCREEN` says when `boot` and `full` show that loading screen:
    - `auto` (the default): when there is slow work to do, for the **whole** warm-up. The warm-up
      starts with nothing drawn while the harness watches its first builds (up to 8, at most 0.3 s)
      and reads `user/cache/precompile_state.txt`, the outcome of the last warm-up. The slow work
      left is: every pipeline left if the first builds average more than `COS_PRECOMPILE_SLOW_MS`
      (25 ms; with the shader cache warm a build costs ~12 ms, a miss 100-300 ms) — a cold cache:
      the first start, a new NRO with new shaders or a new Mesa; none if they are fast and the last
      warm-up was complete; and the pipelines past the point where the last one stopped if it did
      not finish (the app closed during the loading screen) or if there is no record (the first
      start with this version), at the cost recorded then (100 ms each if none). If that would take
      more than `COS_PRECOMPILE_SCREEN_MIN_S` (3 s), the loading screen stays up until every queued
      pipeline is built, back to back (no throttle), presenting at `COS_PRECOMPILE_SCREEN_FPS`:
      "Preparando shaders (solo la primera vez): N/M, ~X s" (English on a console in another
      language: "Preparing shaders (first start only)"; `COS_LANG=es|en` overrides), the time left
      from the pace of the last 5 s, so builds that turn out to be cache hits shorten it at once.
      On the console a cold start takes about 2 minutes there (1120 pipelines, ~104 ms each) and
      the logos, title and menus then run without a single warm-up compile. The record is written
      when the screen starts, every 2 s while it is up and when it ends (`complete`), and when a
      warm-up finishes behind the logos. Otherwise (a warm cache, or only a few slow pipelines
      left) the game starts at once (the boot logo comes up directly) and the warm-up goes on
      behind the logos as before: the priority pipelines first, the throttle letting fast builds
      follow back to back, a pipeline a draw needs before the warm-up reached it built when asked
      for. If the builds turn slow behind the logos (Switch, logo scene: a sample of at least 8
      slow builds among the last 16, or 1.5 s of slow work measured, and the pipelines left times
      the recent slow share times the slow builds' cost over `COS_PRECOMPILE_SCREEN_MIN_S`;
      `native/src/pc/features/pc_precompile_gate.h`), the loading screen comes up there until the whole
      warm-up is built (the game's frame counter waits meanwhile). A build's time leaves out its
      wait for the GL context (Dawn `dawn-switch-gl-pipeline-wait.patch`, Aurora patch 0012): on
      every warm boot the 3-4 builds of the game's first frame waited ~300 ms in all for the render
      worker - shader cache hits, Mesa reporting 1576 of 1576 cache hits - and the old rule (3 slow
      of the last 32, every pipeline left at their cost: "slow work about 110 s") put a 10 s
      loading screen on each warm boot. Each build over `COS_PRECOMPILE_SLOW_MS` is logged (up to
      48): `[cos] precompile slow build N: pipeline <key> (warm-up, done/total), X ms: tint T,
      waited for the GL context W, held it H ms (its own work ... | slow only for the wait ...)`.
    - `priority`: what `auto` did before: when the first builds are slow, the loading screen for
      the 176 priority pipelines only ("Preparing shaders... N/M", about 10-25 s), then the rest
      behind the logos and menus, throttled (a 100-300 ms hitch per cold build on the title: 101
      on the first start measured); fast, no screen. If the builds turn slow behind the logos
      (3 slow builds, priority set left over 1 s at their pace) the screen comes up there for the
      priority set.
    - `always`: the loading screen for the priority set (`full`: every pipeline) whenever it is not
      built yet (the behaviour before `auto`); `never`: no loading screen.

    One line logs the choice, with the counts and the estimate (values vary):

    ```
    [cos] precompile screen auto: 4 builds in 0.30 s, 64.4 ms each (COS_PRECOMPILE_SLOW_MS=25): shader cache cold; slow work about 71.9 s (> COS_PRECOMPILE_SCREEN_MIN_S=3.0): loading screen until the whole warm-up is built, 1116 of 1120 pipelines left, about 72 s
    [cos] precompile screen auto: 8 builds in 0.10 s, 12.1 ms each (COS_PRECOMPILE_SLOW_MS=25): shaders cached and the last warm-up was complete (1120 pipelines); slow work about 0.0 s (<= COS_PRECOMPILE_SCREEN_MIN_S=3.0): no loading screen; the game starts and the 1112 of 1120 pipelines left (168 priority) build behind the logos
    [cos] precompile screen auto: 8 builds in 0.10 s, 12.3 ms each (COS_PRECOMPILE_SLOW_MS=25): shaders cached so far, but the last warm-up stopped at 527/1120, so the 593 pipelines past it are taken as uncached at 104 ms each (recorded); slow work about 61.7 s (> COS_PRECOMPILE_SCREEN_MIN_S=3.0): loading screen until the whole warm-up is built, 1112 of 1120 pipelines left, about 68 s
    [cos] precompile screen auto: 8 builds in 0.10 s, 12.0 ms each (COS_PRECOMPILE_SLOW_MS=25): shaders cached so far, but no record of a complete warm-up (precompile_state.txt), so all 1112 left are taken as uncached at 100 ms each; slow work about 111.2 s (> ...): loading screen until the whole warm-up is built, ...
    [cos] precompile screen auto: builds turned slow behind the logos (9 of the last 16 builds slow, 140 ms each; 1.3 s of slow work measured; 800 queued pipelines left, 56% of them slow at that share: about 63.0 s (> COS_PRECOMPILE_SCREEN_MIN_S=3.0)): loading screen until the whole warm-up is built
    [cos] precompile screen priority: 2 builds in 0.30 s, 151.0 ms each (COS_PRECOMPILE_SLOW_MS=25): shader cache cold, loading screen for the 174 priority pipelines left
    ```

    The first start of this version on a console whose shader cache is already warm has no record
    yet, so it shows the screen once; the builds are cache hits (~12 ms), so it lasts ~15 s and
    writes `complete`. Deleting `precompile_state.txt` (or `user/cache/`) brings that back.
  - `all`: no loading screen; every known pipeline is built whatever the game does, into gameplay
    (a stutter per pipeline), with the corner indicator until done.
  - `off`: nothing is built ahead.
- Behind the logos and menus (`boot` without a loading screen, or after the priority set's) the
  warm-up is throttled (after `auto`'s loading screen nothing is left to build): each
  build holds the GL context for 0.1-0.3 s and the render worker needs it for every frame, so back
  to back builds left the logos and title at 4-7 frames/s for about three minutes. Aurora Switch
  patch 0009 (`aurora_switch_set_warmup_throttle`) keeps the compile thread idle for
  (1 - d) / d times the previous build's duration and then starts the next build right after a
  present, so a build overlaps the game thread's next frame; a pipeline a draw waits for is never
  held back. `COS_PRECOMPILE_DUTY=d` (default 0.5; `0` or `1`: back to back as before) sets d.
  The warm-up then takes about 1/d as long, and what is not built by the first PLAY scene is built
  when first drawn. Not real parallelism: Mesa 20.1's nouveau has no threaded compile (no
  `set_max_shader_compiler_threads`, so `GL_KHR_parallel_shader_compile`'s
  `GL_COMPLETION_STATUS_KHR` is true as soon as `glLinkProgram` returns), and a second, shared GL
  context on another thread is not an option on this nouveau (patch 0001: it drew nothing; nouveau
  contexts share one push buffer without locking).
- `switch/dawn/patches/dawn-switch-gl-pipeline-compile.patch`: Dawn translates a render pipeline's
  WGSL to GLSL (Tint, CPU only) before it takes the GL context, so the render worker waits only for
  Mesa's compile and link. The progress line shows both parts per build ("tint X ms, GL context Y
  ms each"; the context part includes waiting for the worker to release the context).

The log shows the warm-up (`COS_PRECOMPILE_LOG=0` hides the progress lines; values vary):

```
[info] [aurora::gfx::pipeline_cache] Seeded pipeline cache from '/switch/switchwaker/initial_pipeline_cache.db' (R rows merged, 0 rows skipped)
[info] [aurora::gfx::pipeline_cache] Bundled pipeline cache marks 176 pipelines as priority 0
[cos] precompile: M pipelines queued from the pipeline cache, P of them priority (boot: the warm-up runs until the first PLAY scene; COS_PRECOMPILE_SCREEN=auto (...))
[cos] precompile screen auto: K builds in T s, X ms each (COS_PRECOMPILE_SLOW_MS=25): shader cache cold; slow work about S s (> COS_PRECOMPILE_SCREEN_MIN_S=3.0): loading screen until the whole warm-up is built, L of M pipelines left, about E s
[cos] precompile loading screen: waiting for L queued pipelines, 10 frames/s at most (COS_PRECOMPILE_SCREEN_FPS), builds back to back
[cos] precompile loading screen N/M, T s, F frames presented, about E s left at X ms each   <- once a second
[cos] precompile loading screen done: M/M queued pipelines in T s, L built (B slow builds, X ms each), F frames presented; M/M of the warm-up built, the game starts
[cos] precompile done: M/M pipelines, ...                       <- the first game frame: nothing left
With `priority` or `always` (the priority set on the screen, the rest behind the logos):

```
[cos] precompile loading screen done: P/P priority pipelines in T s, P built, F frames presented; N/M of the warm-up built, the game starts
[cos] precompile throttle: the rest of the warm-up builds about 50% of the time, each build after a present (COS_PRECOMPILE_DUTY=0.50)
[cos] precompile N/M pipelines, T s, compile C s (X ms each); GL programs L linked, S shared; B pipeline builds: tint X ms, GL context Y ms each; throttle duty 0.50, H held back, W s waited; frame F, scene LOGO_SCENE
[cos] precompile stopped (PLAY scene; D left to build when first drawn) at N/M pipelines, ...
[cos] precompile done: M/M pipelines, ...                       <- instead, if it finished first
```

Without the shader cache every start compiled again, so the loading screen came back at every
start and the logos and menus ran slower while the rest was built (throttled: see above).
With it the first start still does that (with `auto`, all of it on the loading screen), and later
starts build each known pipeline from the cache in about 12 ms (measured: the priority set's
loading screen then lasted ~2.3 s), so with `COS_PRECOMPILE_SCREEN=auto` those starts skip the
loading screen and the warm-up builds behind the logos; the warm-up is kept
(it creates Dawn's pipeline objects, which a first draw would otherwise create) and, once the
cache's hit rate is confirmed on the console, `full` costs little more than `boot`. The game's frame counter does not move during the loading screen; the stall watchdog
(`COS_STALL_S`) counts its frames instead. On the Mac the same file is read only if it is copied
next to `build/native-mac/switchwaker`; there the whole warm-up of 995 pipelines took 83 s of the compile
thread with a warm Dawn cache, and frames captured with and without it are identical. The Mac
draws no loading screen or indicator unless `COS_PRECOMPILE` is set (native/README.md).

### Growing the list from the console

A pipeline the bundled list does not have still compiles when first drawn (a 150-640 ms hitch:
bomb explosions on Dragon Roost, for one). The console's own cache,
`switch/switchwaker/native/user/cache/pipeline_cache.db`, records every pipeline used in play, so
after playing, with USB file transfer on and the app closed:

```sh
scripts/switch/pull_pipeline_cache.sh            # -> build/pipeline-cache/console/<timestamp>.db
native/tools/gen_pipeline_cache.sh --merge-only --merge-from build/pipeline-cache/console/<timestamp>.db
git add native/data/initial_pipeline_cache.db   # the merged file is copied there; commit it
scripts/switch/push.sh --pipeline-cache
```

`pull_pipeline_cache.sh [OUT]` copies the database and its `-journal` (the Switch build's PERSIST
rollback journal, Aurora Switch patch 0006) and `-wal` if present over MTP, twice for the database
(both copies must match, or it pulls again: the app writes its cache while it runs), opens the copy
with sqlite (a zeroed journal header is ignored; a hot journal, a commit cut short by HOME, is
rolled back on the copy as Aurora would at its next start) and writes one clean file with
`VACUUM INTO`; the raw files stay in `<timestamp>.raw/`. The merge adds the console's new rows as a
tier after the existing ones (priority 1) and reports how many were new, already known, or of
another config version (skipped: a cache written by an older NRO).

## Shader cache

The NRO's Mesa (`scripts/switch/build_mesa.sh`, `switch/mesa/patches`) keeps compiled shaders on the
SD card, so a pipeline built once is not compiled again on later runs:

- **Why devkitPro's package had none.** Its meson change compiles the disk shader cache out on
  Horizon (`-DENABLE_SHADER_CACHE` only when `host_machine.system() != 'horizon'`), so nouveau's
  `get_disk_shader_cache` returned NULL, the state tracker left `GL_NUM_PROGRAM_BINARY_FORMATS` at 0,
  and `MESA_GLSL_CACHE_DIR`/`MESA_SHADER_CACHE_DIR` did nothing. Mesa's file cache needs mmap,
  flock, getpwuid, zlib and one file per entry under 256 directories, and nouveau names its build
  with dladdr's build-id: none of that exists on Horizon (and thousands of small files open slowly
  on the SD card's FAT32/exFAT).
- **Patch 0003** keeps Mesa's `disk_cache.h` API in one append-only file,
  `switch/switchwaker/native/user/cache/mesa_shader_cache.bin` (`MESA_SHADER_CACHE_DIR`, set by
  the port before EGL starts): a header with the sha1 of the driver keys, then records (key, size,
  CRC-32, payload) written with one `write()` each; payloads are read when asked for and their CRC
  checked. Beside it, `mesa_shader_cache.idx` lists the records (key, size, offset; 40 bytes and a
  CRC each, appended after each record): at start it is read in one `read()` instead of one 32-byte
  read per record of the `.bin` (2747 records took 860 ms on the SD card that way). Its entries are
  used while they are whole, contiguous and inside the `.bin`, the last one is checked against the
  `.bin`, and a random generation in both headers ties the two files together; records the index
  misses (the app stopped between the two writes) are read one by one and added, and a missing,
  damaged or foreign index is rebuilt from the `.bin` once ("index rebuilt (...)" in the log). The
  `.bin` keeps its format (version 1). **Pruning:** nothing was ever removed, so a shader change
  left every old entry in the file (after the uniform window change: 5135 entries, 26.4 MiB,
  opened in 2241 ms). `mesa_shader_cache.use` records the run in which each entry was last used
  (stored, or read by a lookup; read with one `read()` at start, written by a maintenance thread
  every 15 s while it changed and at exit). Once per run, 60 s after start
  (`MESA_SHADER_CACHE_PRUNE_DELAY`), that thread compacts the file if the entries unused for 5 runs
  (`MESA_SHADER_CACHE_PRUNE_BOOTS`; 0: never) plus dead records are a quarter of it and at least
  1 MiB (or any, past three quarters of the size limit): it copies the kept records into
  `.bin.tmp` through its own descriptor in 512 KiB steps (the game and the compile thread keep
  using the cache meanwhile), then, holding the cache's lock for the swap only, adds the records
  written meanwhile, writes the new index and replaces the files. A start that finds `.bin.tmp`
  without `.bin` (stopped mid-swap) takes it; other leftover `.tmp` files are deleted. An entry
  missing from, or a damaged, `.use` counts as used in the current run, so nothing is pruned on a
  guess. Mesa's GLSL-cache entries of programs Dawn loads as binaries are not read again and so
  get pruned after 5 runs: they only matter if Dawn's binary is lost, and are rebuilt then. Safety: a file
  from another driver build (the build is named by `MESA_SWITCH_CACHE_ID`, a hash of Mesa's source
  and every patch, written by `build_mesa.sh`), of another format version or with a bad header is
  emptied; a torn last record (the app stopped mid-write) is cut off; a damaged entry is a miss and
  is written again; nothing is evicted, and past 256 MiB (`MESA_GLSL_CACHE_MAX_SIZE`) new entries
  are dropped. With it Mesa's GLSL cache works (a shader seen before is not compiled: the compile is
  deferred, and the link loads the program's GLSL metadata and TGSI from the cache) and Mesa offers
  one program binary format, which Dawn's GL backend already uses: it stores each program's
  `glGetProgramBinary` in its blob cache (`user/cache/dawn_cache.db`) and loads it with
  `glProgramBinary` instead of compiling (a stale or damaged binary is refused by Mesa's checksum
  and driver sha1, and Dawn then compiles from source).
- **Patch 0004**: neither of those skips nvc0's code generation (TGSI to Maxwell code, run when a
  program is linked or loaded). Mesa added a disk cache for it upstream in 20.3, after splitting
  `nv50_ir_prog_info`; 0004 is a smaller equivalent for 20.1: `nvc0_program_translate` looks the
  translated program (code, header, relocations, interpolation fixups, header state) up by a key of
  its inputs (chipset, stage, TGSI tokens, user clip planes) before running the compiler. Fixup
  function pointers are stored as indices into the GM107 emitter's table, the emitter of the
  console's GM20B.
- **Off switch:** `COS_SWITCH_SHADER_CACHE=0` in `env.txt` (Mesa then behaves as the package did:
  no cache, no program binaries); `COS_SWITCH_SHADER_CACHE=reset` deletes the file at start.
  Deleting `user/cache/` clears it with Aurora's caches.
- **Log.** Once EGL is up, and then every 15 s while the counters change and at exit (values
  vary):

  ```
  [switch] shader cache: MESA_SHADER_CACHE_DIR=/switch/switchwaker/native/user/cache
  [switch] shader cache: /switch/.../user/cache/mesa_shader_cache.bin: N entries, M MiB, opened in T ms; index: R records in one read; U unused for 5+ runs; run B (max 256 MiB)
  [switch] shader cache: compacted /switch/.../mesa_shader_cache.bin in T ms: K of N entries kept, U unused for 5+ runs dropped, M MiB -> M' MiB (run B)   <- at most once a run
  [switch] shader compile: compiles C (D deferred) X ms; links L (F from cache) Y ms = glsl G + st S; nvc0 T (H from cache) Z ms; binaries loaded B (R refused) W ms, saved V W ms; cache gets ... puts ...; dawn binaries: formats 1, hits h, misses m, refused r, stored s (M MiB)
  ```

  On the first start after installing an NRO with a new Mesa: "new file" or "discarded: written by
  another driver build" (every change to `switch/mesa/patches` renames the driver build, so the
  NRO with the index starts once from an empty cache too), dawn binaries mostly misses and stored, nvc0 none from cache. On the next
  start: dawn binary hits for the pipelines built before, nvc0 "from cache" close to its total, and
  the precompile lines' "compile ... (X ms each)" should drop from 126-176 ms to a few ms. The
  "glsl", "st" and "nvc0" times of the first start are where a compile's time goes (the GLSL front
  end and linker, GLSL IR to TGSI, and Maxwell code generation); what the precompile line counts
  beyond them is Dawn's own work (Tint, pipeline objects).
- **Checked off the console** (`build_mesa.sh --test`): the file's persistence, removal, torn tail,
  damaged entry, another driver build and size limit; the index with 3000 entries (read at once,
  rebuilt when missing, torn, damaged in the middle, of another generation or for a `.bin` from
  before it, records it misses read from the `.bin`, a torn `.bin` under a whole index); pruning
  (entries unused for N runs dropped by a compaction, the rest still readable and indexed, writes
  after it, a swap interrupted at either point, a damaged use file, the maintenance thread doing
  it by itself); nvc0 code from the cache identical to a fresh
  translation (code, header, state, relocations and fixups, GM20B); and through the GL API (OSMesa
  on softpipe with a test-only disk cache hook), a second run links every program from the cache
  with every compile deferred, draws the same pixels as the first, loads the saved program binaries
  with the same pixels, and refuses a damaged binary.

Threads: the game thread runs on core 0; JAudio's, the DVD thread, Aurora's and Dawn's
workers prefer cores 1 and 2 (`switch/native/source/thread_wrap.c`). Every 15 seconds, at exit
and in a crash report, `[switch] memory: used N MiB of M MiB` shows the process's memory.

## Crashes

- **The log.** A crash prints `[cos] CRASH <kind> esr=... far=...`, the registers, and a backtrace
  with every address also given as `switchwaker.elf+0x<offset>` (the offset from the start of the NRO's
  text mapping, which `[cos] image base=0x...` and the start banner's `image at 0x...` print);
  then the harness's state line (scene, frame, last resource). `abort()` (Aurora's fatal errors, asserts) prints `[cos] ABORT` with a
  backtrace the same way, and an `OSPanic` `[cos] PANIC` (exit 12). Get the log with
  `scripts/switch/push.sh --logs` (to `build/switch-logs/native/switchwaker.log`), or from the live USB log.
  Resolve the offsets with the ELF of the same build:

  ```sh
  docker run --rm -v "$PWD/build/switch-native:/b" localhost/centollos-switch-native-build:2026-10-03 \
      /opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line -f -C -i -e /b/switchwaker.elf 0x<offset> ...
  ```

- **Atmosphère's report.** After the log, the crash goes on to Atmosphère, which writes
  `atmosphere/crash_reports/<time>_<program id>.log`. Copy it with
  `build/switch-tools/switch_mtp pull atmosphere/crash_reports <name>.log out.log` and resolve the
  addresses after the module name (`+ 0x...`) the same way.
- `COS_SMOKE=crash-test` in `env.txt` crashes on purpose, to see both reports once.

## If something goes wrong

- Nothing in the log at all: check that `switch/switchwaker/native/` exists afterwards (the
  app creates it); start from title mode.
- `[cos] DISC: cannot open COS_DISC=...` (exit 14): the disc image is missing; `push.sh --disc`.
- The app closes right after `Attempting to initialize OpenGLES`: Dawn or Mesa failed; the Atmosphère
  report and the last `[info] [aurora::gpu]` lines say where.
- `[cos] STALL: frame counter frozen` (exit 11): no game frame for 90 seconds (`COS_STALL_S`).
- Every run aborts at the same point right after start-up, after one that aborted in a shader:
  Aurora recompiles its cached pipelines at start-up (as on the Mac, phase 6 render issues); delete
  `switch/switchwaker/native/user/cache/`.
- `GPU cache ... is damaged` or `... could not be opened ...; deleted it`: once, after a run that was
  killed while an older build (journal in memory) was writing, is expected; the cache starts empty
  and fills again. If it comes back after runs closed with HOME on this build, note the
  `journal_mode=` line and the `[sqlite]` lines before it, and try `COS_SWITCH_SQLITE_JOURNAL=truncate`
  (or `memory`) in `env.txt`.
- Docker Desktop on macOS needs access to the folder the repository is in: if `build_native.sh` hangs
  with its container in the "Created" state, allow Docker in System Settings › Privacy & Security ›
  Files and Folders (Documents), or restart Docker Desktop.

