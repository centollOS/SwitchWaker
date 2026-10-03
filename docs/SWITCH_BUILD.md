# Building the Switch NRO

How to build the unofficial Switch homebrew build of the recompilation project from your own disc,
copy it and its data to the console, and read its logs. There are two builds:

- **headless** (the default): the game runs without graphics, sound or controls and reports its
  progress on screen and in a log. It proves the recompiled game code and runtime on the console.
- **Aurora** (`--aurora`): with the GX renderer, the controller, sound and rumble. Aurora itself
  runs on the console (picture through Dawn's OpenGL ES backend at 60 FPS, the controller through
  the GameCube PAD API, sound and HD rumble); the game with Aurora has not run yet.

Plan and status:
[SWITCH_PORT_PLAN.md](SWITCH_PORT_PLAN.md), [SWITCH_IMPLEMENTATION_CHECKLIST.md](SWITCH_IMPLEMENTATION_CHECKLIST.md).

> [!IMPORTANT]
> The NRO contains code translated from your disc. Like every build in this repository, it is for
> your own console only: never share or upload it, the disc image or any file extracted from it
> ([AGENTS.md](../AGENTS.md)). Everything below stays in the ignored `build/` and `ref/` folders.

## What you need

- A Mac (Apple silicon) or Linux machine with Git, Python 3, clang, CMake and Ninja.
  On macOS, Xcode's clang and `brew install cmake ninja libmtp libusb uv`.
- Docker Desktop or Podman. The scripts use the official devkitPro image, pinned by digest. It runs
  natively on Apple silicon (`linux/arm64`). Give it at least 8 GB of memory (Settings › Resources).
- Your disc image of the game, GameCube USA (`GZLE01`, revision 0), as an
  uncompressed `.iso`. The builder checks it and refuses other versions.
- A Switch you have already set up for homebrew (Atmosphère and the Homebrew Menu), a USB-C data
  cable, and the console's **USB file transfer** (Horizon's own, or haze/DBI).

## 1. Build

```sh
scripts/bootstrap.sh                                         # pinned RecompCore, DolRecomp, Aurora into ref/
scripts/builder/build.sh /path/to/GZLE01.iso --source-only   # check the disc, generate the game source
scripts/switch/build_host.sh                                 # build/switch-host/BlueWakeSwitch.nro
scripts/switch/build_host.sh --aurora                        # build/switch-host-aurora/BlueWakeSwitch.nro
```

- `--source-only` extracts `main.dol` and the 415 RELs into `build/device/game` and generates the
  translated source into `build/device/composite-src` (about 900 MB of C). It takes a few minutes
  and ends with `composite source digest …: the verified tree`.
- `build_host.sh`:
  1. compiles that source on the computer itself with clang for the Switch's CPU, against
     devkitA64's newlib headers (`switch/composite/clang-switch.cmake`). clang takes about 1.7x
     less time than devkitA64's GCC on these files and half the memory (1.5 GB for the largest);
  2. in the container, links those objects into one relocatable object,
     `build/switch-composite-clang/gGZLE01_recomp.o`, with every symbol renamed `bwc_<name>`;
  3. links it with the host, GXRuntime and the donor DSP (and, with `--aurora`, Aurora and Dawn)
     into the NRO (`switch/host`).

  The first run takes about an hour and a half on a 10-core Mac; later runs reuse what is already
  compiled. The first `--aurora` build also compiles Dawn. `SWITCH_BUILD_JOBS` sets the parallel
  jobs (default: every core for the composite, 4 in the container).

If your disc image is compressed (`.ciso`, `.rvz`, …), convert it to `.iso` first. A GameCube `.iso`
is exactly 1,459,978,240 bytes.

## 2. Copy to the console

Turn on USB file transfer on the console, connect it to the computer, then:

```sh
scripts/switch/push.sh --game /path/to/GZLE01.iso   # game data: about 1.5 GB, 82 s the first time
scripts/switch/push.sh host                         # the NRO, read back and checked by SHA-256
scripts/switch/push.sh host-aurora                  # or the Aurora build, under the same name
```

`push.sh` copies over MTP and needs no SD-card reader or reboot. `--game` skips files that are
already on the console with the same size, so it is quick to rerun. The resulting SD-card layout:

| Path on the SD card | Contents | Source |
|---|---|---|
| `switch/centollos/BlueWakeSwitch.nro` | the app | `build/switch-host/BlueWakeSwitch.nro` |
| `switch/centollos/GZLE01.iso` | your disc image; the game reads its data from it | your `.iso` |
| `switch/centollos/dsp_rom.bin`, `dsp_coef.bin` | free replacement DSP ROMs | `ref/recompcore/Data/Sys/GC/` |
| `switch/centollos/game/main.dol` | the game's executable | `build/device/game/main.dol` |
| `switch/centollos/game/rels/*.rel` | the game's 415 modules | `build/device/game/rels/` |
| `switch/centollos/GZLE01.card` | memory card; created by the game | — |
| `switch/centollos/host.log`, `states/` | log and save states; created at run time | — |

Without USB file transfer, copy the same files with an SD-card reader, or with Hekate's
**Tools › USB Tools › SD Card**, to the paths above.

## 3. Run

Start the Homebrew Menu in **title mode**: hold **R** while you open an installed game. Launching it
from the Album gives applets far less memory than the game needs. Open **BlueWakeSwitch**.

The headless build:

- shows no game picture: the screen shows its log as text, the same lines as `host.log` and the
  live USB log;
- runs about one minute of game time (`BLUEWAKE_MAX_RETRACES=3600`) and then writes
  `[switch] host returned …`;
- exits with **+**.

It checks for the DOL, disc and DSP ROMs first, and logs any that are missing.

## 4. Logs and crashes

- **Live over USB.** Leave the cable connected and run, on the computer:

  ```sh
  uv run scripts/switch/usb_log.py --out build/switch-logs/live.log
  ```

  It waits for the app and prints its log as the app writes it. It runs directly on the host, not
  in a container (Docker Desktop has no USB access).
- **From the SD card.** Every line also goes to `switch/centollos/host.log`. With USB file
  transfer on, `scripts/switch/push.sh --logs` copies it, and the probes' logs, to
  `build/switch-logs/`.
- **Crashes.** Atmosphère writes a report to `atmosphere/crash_reports/`. Copy it with
  `build/switch-tools/switch_mtp pull atmosphere/crash_reports <name>.log out.log`, then resolve its
  addresses (`+ 0x…` after the module name) with the ELF next to the NRO:

  ```sh
  aarch64-none-elf-addr2line -f -C -i -e build/switch-host/bluewake_switch_host.elf 0x…
  ```

  `aarch64-none-elf-addr2line` is in the devkitPro image. Run it through `docker run` with the
  repository mounted, as the build scripts do.

## Native port

The native port (`native/`: the game built from its decompilation on Aurora, see
[NATIVE_PORT_PLAN.md](NATIVE_PORT_PLAN.md)) has its own NRO, `centollos.nro`, built from the same
toolchain image, Aurora/Dawn-for-Switch build, SDL 3 shim, SD card folder and logs as the translated
port. It reads only the disc image from the SD card: the game's code is compiled in, and the asset
headers are compiled in at build time, from the same `COS_ASSETS_DIR` as the Mac build. It has not
run on a console yet (phase 7 of the plan).

### Build

Needs what the Mac build of `native/` needs (Aurora at the pin in `build/aurora-3227d76`, the asset
headers in `build/native-mac/assets/GZLE01`, `ref/recompcore`; [native/README.md](../native/README.md))
plus Docker Desktop or Podman:

```sh
scripts/switch/build_native.sh       # build/switch-native/centollos.nro and centollos.elf
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
  and file contents as nod for GZLE01, and a Mac `centollos` linked with it passes `disc-ls`, `arc-sweep`,
  `stage-sweep`, `j3d-sweep` and `opening`.
- The first build fetches Dawn's dependencies (unless the probe's source is there), SDL 3's headers,
  ImGui, Tracy, fmt, xxhash and sqlite, and compiles Dawn and the game: about 40 minutes with 4 jobs
  on a 10-core Mac when the probe's Dawn source is reused; later builds take minutes. `--jobs N` (or
  `SWITCH_BUILD_JOBS`) sets the parallel jobs, default 4. `--aurora`, `--assets`, `--recompcore` and
  `--dawn-src` point at other copies of the inputs; from a git worktree (`build/lanes/<lane>`) the
  main checkout's are used.
- Output: `build/switch-native/centollos.nro` (about 21 MB) and `build/switch-native/centollos.elf`, the
  same program with its symbols, for `addr2line`. Keep the ELF of the NRO you test.

### Copy to the console

```sh
scripts/switch/push.sh --disc /path/to/GZLE01.iso   # once: the disc image (skipped if already there)
scripts/switch/push.sh native                       # the NRO, read back and checked by SHA-256
scripts/switch/push.sh --native-env my-env.txt      # optional: run options (see below)
scripts/switch/push.sh --pipeline-cache             # optional: the bundled pipeline cache (see "Pipeline precompile")
```

`--game` (the translated port's data) puts the disc image in the same place, so `--disc` is not
needed after it. SD card layout:

| Path on the SD card | Contents |
|---|---|
| `switch/centollos/centollos.nro` | the app: "the game (native)" in the Homebrew Menu |
| `switch/centollos/GZLE01.iso` | your disc image, shared with the translated port |
| `switch/centollos/initial_pipeline_cache.db` | optional: pipelines to precompile at boot, made on the Mac from your disc (never committed) |
| `switch/centollos/native/env.txt` | optional run options |
| `switch/centollos/native/centollos.log`, `centollos.prev.log` | this run's log and the previous one's |
| `switch/centollos/native/user/` | memory card (`USA/Card A`), Aurora's caches |

### Run

Start the Homebrew Menu in title mode (hold **R** while opening an installed game; an applet has far
less memory than the game needs, and the log says so) and open *the game (native)**. The CPU
stays at its stock 1020 MHz. With USB connected, `uv run scripts/switch/usb_log.py --out
build/switch-logs/native-live.log` shows the log live.

Run options come from `native/env.txt`, one `NAME=value` per line, with `#` comments
([switch/native/env.example.txt](../switch/native/env.example.txt)); they are the Mac's `COS_*`
variables ([native/README.md](../native/README.md), "Running centollos"). Without the file:
`COS_DISC=/switch/centollos/GZLE01.iso`, `COS_RUN_DIR=/switch/centollos/native`,
`COS_PERF_EVERY=60`, `COS_HITCH_MS=50`, `COS_STALL_S=90` and `COS_ASPECT=16:9` (the widescreen
option on the 1280x720 screen; `COS_ASPECT=4:3` gives the GameCube picture, pillarboxed).

What the log shows, in order (the same `[cos]` lines as on the Mac; values vary):

```
[switch] the game, native port (phase 7); argv[0]=sdmc:/switch/centollos/centollos.nro
[switch] centollos native: application (title mode); memory 3xxx MiB, ... core mask 0x7; image at 0x...
[switch] logs: /switch/centollos/native/centollos.log open, USB live log started
[cos] harness: smoke=- milestone=- timeout=0s stall=90s ...
[cos] perf: game-thread frame times every 60 frames (COS_PERF_EVERY)
[cos] disc: /switch/centollos/GZLE01.iso GZLE01 revision 0, 1459978240 bytes
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
`COS_SWITCH_GL_NO_ERROR=1` in `env.txt` makes Dawn ask for a `KHR_no_error` GL context
(`switch/dawn/patches/dawn-switch-gl-no-error-context.patch`), in which Mesa skips the error
checks of every GL call, draw and uniform validation included; `[dawn] COS_SWITCH_GL_NO_ERROR:` in
the log says whether Mesa accepted it. It is an A/B option for the replay times: in such a context
a GL error has undefined results.

"begin" of the perf line is `events` (Aurora's event pump) plus `aurora_begin_frame`, which mostly
waits for a free frame slot (the render worker still has two frames in flight: GPU-bound or
worker-bound) or for a mapped staging buffer (the GPU has not finished the frame that used it).
The render worker's busy time is what it costs to turn a frame into GL calls and present it; a
worker near the frame time means the worker, not the game thread, sets the frame rate. Dawn's
GL queue has no EGL sync extension on the console's Mesa: it used to call `glFinish` after every
submission (the CPU waited for the GPU each frame); it now puts a GLES sync object in
(`switch/dawn/patches/dawn-switch-gl-fence-queue.patch`, the "gl ... fences" count) and polls it.
`COS_SWITCH_GL_FINISH=1` in `env.txt` brings the `glFinish` back for comparison ("glFinish" count
and time). Every game frame whose busy time is over `COS_HITCH_MS` (50 ms by default; 0 turns it
off) gets one `[cos] hitch frame N: busy ... ms (wall ...): events, begin_frame, cpd, aud, logic,
painter, end_frame, other; pipelines +n (q queued), tex upload KiB, res loads +n last <path>,
scene NAME (new); switch: slot wait, staging wait, queue-full wait, worker busy (encode, submit,
present, events), gl fence wait, glFinish, pipeline compile ms (count), dvd reads; dawn gl: draws,
tex binds, texparams, execute, other work, release ms` line.

Aurora's caches (`user/cache/dawn_cache.db`, `pipeline_cache.db`) failed their first transaction
on the console with "database disk image is malformed", so shaders and pipelines were compiled
again every run. On the Switch they now keep no journal file (`journal_mode=MEMORY`, Aurora patch
0006, with exclusive locking and in-memory temp files in the sqlite build); a cache that still
fails is deleted and created once more (`Removed ... and retrying` in the log), and sqlite's own
error log is in the run log as `[sqlite] (code) message` lines, which name the failing check.

### Pipeline precompile

Every new pipeline costs 0.1-0.4 s on the console, and the game stutters for that long: Dawn's GL
backend links one GL program per pipeline on the single GL context (with `gl_defer` the render
worker waits for it), and Mesa 20.1 compiles every program from GLSL on every run. There is no
binary to keep: Dawn already stores `glGetProgramBinary` results in its blob cache
(`dawn_cache.db`) where the driver offers them, but the devkitPro `switch-mesa` 20.1.0 build
reports `GL_NUM_PROGRAM_BINARY_FORMATS` 0. Its meson rule compiles the disk shader cache out on
Horizon (`-DENABLE_SHADER_CACHE` only when `host_machine.system() != 'horizon'`), so nouveau's
`get_disk_shader_cache` returns NULL, Mesa's state tracker sets `NumProgramBinaryFormats` only
when there is a disk cache, and `MESA_GLSL_CACHE_DIR`/`MESA_SHADER_CACHE_DIR` do nothing (no
`disk_cache_create` in `libEGL.a`). What remains is to compile fewer programs and to compile them
before they are needed:

- `switch/dawn/patches/dawn-switch-gl-program-share.patch`: pipelines whose stages translate to the
  same GLSL share one linked program. Aurora's GX pipelines that differ only in blend, depth, cull or
  polygon offset state do: of the 1015 GX pipelines the Mac recorded over the boot path and every
  stage, 823 have distinct shaders (about a fifth fewer compiles).
- Aurora queues every pipeline its cache knows (`user/cache/pipeline_cache.db`) on its compile thread
  at start, in order of first use. `native/tools/gen_pipeline_cache.sh` (on the Mac, with your disc)
  records the pipelines of the logos, title and file select, a new game through the prologue,
  Outset with Link controllable and a 600-frame boot of every stage, and merges them into
  `build/pipeline-cache/initial_pipeline_cache.db` (about 1000 rows, 4 MB; ordered so the boot path
  comes first). Its `pipeline_priority` table marks the rows recorded on the boot path (tiers 0-3,
  logos to Outset: 176 of 1016 rows) priority 0 and the stage sweep's 1;
  `gen_pipeline_cache.sh --mark-priority DB` rewrites only that table in an existing file.
  `scripts/switch/push.sh --pipeline-cache` copies it next to the NRO, where Aurora
  merges it into the player's cache at every start (`Seeded pipeline cache from ...`). It holds
  Aurora's pipeline keys (GX TEV stage and combiner selectors, vertex formats, blend, depth and cull
  state) recorded from the game's materials: no textures, models, text, audio or code, but it is
  derived from the disc, so it stays out of git like the disc itself.
- Aurora Switch patch 0008 queues the priority-0 pipelines first (the player's own cache records
  each run's frames, so by first use alone pipelines seen anywhere in the game would come first
  after a few sessions) and counts them for the harness.
- `COS_PRECOMPILE` (in `native/env.txt`) says how long to wait and when to stop:
  - `boot` (the default): before the game starts (before the boot logo) a loading screen,
    "Preparing shaders... N/M" and a bar drawn with Aurora's ImGui, until the priority pipelines are
    built (about 20-25 s at 126-176 ms each). The screen keeps presenting frames (slowly: a build
    holds the GL context) and pumping events, so HOME works. Then the game starts and the rest are
    built behind the logos, title and menus, with "Shaders N/M" in the bottom-right corner, until
    the game first enters its PLAY scene: there the warm-up ends (Aurora Switch patch 0007) and what
    is left is built when first drawn, as before. A bundled file without the priority table (an
    older `gen_pipeline_cache.sh`) gives no loading screen.
  - `full`: the loading screen until every known pipeline is built (1094 at about 2.5 min); nothing
    is left to stutter on a pipeline the cache knows.
  - `all`: no loading screen; every known pipeline is built whatever the game does, into gameplay
    (a stutter per pipeline), with the corner indicator until done.
  - `off`: nothing is built ahead.

The log shows the warm-up (`COS_PRECOMPILE_LOG=0` hides the progress lines; values vary):

```
[info] [aurora::gfx::pipeline_cache] Seeded pipeline cache from '/switch/centollos/initial_pipeline_cache.db' (R rows merged, 0 rows skipped)
[info] [aurora::gfx::pipeline_cache] Bundled pipeline cache marks 176 pipelines as priority 0
[cos] precompile: M pipelines queued from the pipeline cache, P of them priority (boot: loading screen for the priority set, then until the first PLAY scene)
[cos] precompile loading screen: waiting for P priority pipelines
[cos] precompile loading screen N/P, T s, F frames presented   <- once a second
[cos] precompile loading screen done: P/P priority pipelines in T s, F frames presented; N/M of the warm-up built, the game starts
[cos] precompile N/M pipelines, T s, compile C s (X ms each); GL programs L linked, S shared; frame F, scene LOGO_SCENE
[cos] precompile stopped (PLAY scene; D left to build when first drawn) at N/M pipelines, ...
[cos] precompile done: M/M pipelines, ...                       <- instead, if it finished first
```

Every start compiles again (nothing survives in Mesa), so the loading screen comes back at every
start, and the logos and menus run slowly while the rest is built: about one frame per pipeline
built. The game's frame counter does not move during the loading screen; the stall watchdog
(`COS_STALL_S`) counts its frames instead. On the Mac the same file is read only if it is copied
next to `build/native-mac/centollos`; there the whole warm-up of 995 pipelines took 83 s of the compile
thread with a warm Dawn cache, and frames captured with and without it are identical. The Mac
draws no loading screen or indicator unless `COS_PRECOMPILE` is set (native/README.md).

Threads: the game thread runs on core 0; JAudio's, the DVD thread, Aurora's and Dawn's
workers prefer cores 1 and 2 (`switch/native/source/thread_wrap.c`). Every 15 seconds, at exit
and in a crash report, `[switch] memory: used N MiB of M MiB` shows the process's memory.

### Crashes

- **The log.** A crash prints `[cos] CRASH <kind> esr=... far=...`, the registers, and a backtrace
  with every address also given as `centollos.elf+0x<offset>` (the offset from the start of the NRO's
  text mapping, which `[cos] image base=0x...` and the start banner's `image at 0x...` print);
  then the harness's state line (scene, frame, last resource). `abort()` (Aurora's fatal errors, asserts) prints `[cos] ABORT` with a
  backtrace the same way, and an `OSPanic` `[cos] PANIC` (exit 12). Get the log with
  `scripts/switch/push.sh --logs` (to `build/switch-logs/native/centollos.log`), or from the live USB log.
  Resolve the offsets with the ELF of the same build:

  ```sh
  docker run --rm -v "$PWD/build/switch-native:/b" localhost/centollos-switch-native-build:2026-10-03 \
      /opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line -f -C -i -e /b/centollos.elf 0x<offset> ...
  ```

- **Atmosphère's report.** After the log, the crash goes on to Atmosphère, which writes
  `atmosphere/crash_reports/<time>_<program id>.log`. Copy it with
  `build/switch-tools/switch_mtp pull atmosphere/crash_reports <name>.log out.log` and resolve the
  addresses after the module name (`+ 0x...`) the same way.
- `COS_SMOKE=crash-test` in `env.txt` crashes on purpose, to see both reports once.

### If something goes wrong

- Nothing in the log at all: check that `switch/centollos/native/` exists afterwards (the
  app creates it); start from title mode.
- `[cos] DISC: cannot open COS_DISC=...` (exit 14): the disc image is missing; `push.sh --disc`.
- The app closes right after `Attempting to initialize OpenGLES`: Dawn or Mesa failed; the Atmosphère
  report and the last `[info] [aurora::gpu]` lines say where.
- `[cos] STALL: frame counter frozen` (exit 11): no game frame for 90 seconds (`COS_STALL_S`).
- Every run aborts at the same point right after start-up, after one that aborted in a shader:
  Aurora recompiles its cached pipelines at start-up (as on the Mac, phase 6 render issues); delete
  `switch/centollos/native/user/cache/`.
- Docker Desktop on macOS needs access to the folder the repository is in: if `build_native.sh` hangs
  with its container in the "Created" state, allow Docker in System Settings › Privacy & Security ›
  Files and Folders (Documents), or restart Docker Desktop.

## Probes

The graphics-feasibility probes live in `switch/` and are built and copied the same way:

```sh
scripts/switch/push.sh --build dawn gles boot
```

See [switch/README.md](../switch/README.md) and [the graphics spike](status/SWITCH_GRAPHICS_SPIKE.md).
