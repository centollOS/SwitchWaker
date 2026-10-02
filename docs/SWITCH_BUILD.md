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

## Probes

The graphics-feasibility probes live in `switch/` and are built and copied the same way:

```sh
scripts/switch/push.sh --build dawn gles boot
```

See [switch/README.md](../switch/README.md) and [the graphics spike](status/SWITCH_GRAPHICS_SPIKE.md).
