# Third-party code

Pins are the ones the build uses today; licenses marked "check" were not read from a local copy
when this was written.

centollOS contains, builds against, or fetches at build time the components below. Nothing in this
list is game data: the repository contains no asset, executable or data file of the game, and every
build reads them from the player's own disc.

## In this repository

| Path | Origin | License |
|---|---|---|
| `game/` | The game's decompilation, imported from a fork ([github.com/snrubrm/tww](https://github.com/snrubrm/tww)) at `b09eebc` of the community decompilation project ([github.com/zeldaret/tww](https://github.com/zeldaret/tww)), changed by later commits of this repository under `TARGET_PC` (see `game/README.md`) | CC0-1.0 (`game/LICENSE`) |
| `native/include/helpers/{endian.h,endian_gx.hpp,endian_ssystem.h,offset_ptr.h}`, `native/src/helpers/offset_ptr.cpp` | [Dusklight](https://github.com/TwilitRealm/dusklight) (TwilitRealm) at `40457c6`; the SDK-over-Aurora layer follows Dusklight's model | CC0-1.0 |
| `native/dsp_hle/` | Adapted from elliotttate's recompilation project ([github.com/elliotttate/Wind-Waker-Recomp](https://github.com/elliotttate/Wind-Waker-Recomp)) (`runtime/host/src/dsp_hle_backend.cpp`, `apple/ios/src/dsp_common_shim.cpp`); compiles Dolphin's DSPHLE (below) | GPL-2.0-or-later (`native/dsp_hle/LICENSE`, SPDX headers in each file) |
| `native/patches/aurora/`, `switch/native/aurora/patches/` | Patches to Aurora | same as Aurora (MIT) |
| `switch/dawn/patches/` | Patches to Dawn (Horizon/OpenGL ES backend) | same as Dawn (BSD-3-Clause) |
| `switch/mesa/patches/`, `switch/mesa/test/` | Patches to Mesa (devkitPro's switch-mesa recipe) and their tests | same as Mesa (MIT) |
| `switch/forwarder/nx-hbloader-forwarder.patch` | Patch to nx-hbloader | same as nx-hbloader (ISC) |

Everything else is the project's own code under the MIT license (`LICENSE`), copyright Pulpparty
and depende3000, and the centollOS contributors.

## Fetched at build time (not in the repository)

| Component | Pin | Used for | License |
|---|---|---|---|
| [Aurora](https://github.com/encounter/aurora) | `3227d76` | GameCube SDK over WebGPU (GX, VI, PAD, DVD, CARD, THP) | MIT |
| [Dawn](https://dawn.googlesource.com/dawn) | Mac: Aurora's prebuilt package; Switch: [encounter/dawn](https://github.com/encounter/dawn) `266c1cf` | WebGPU implementation | BSD-3-Clause |
| [abseil-cpp](https://github.com/abseil/abseil-cpp) | Aurora's / Dawn's pin | Dawn dependency | Apache-2.0 |
| [SDL 3](https://github.com/libsdl-org/SDL) | Aurora's pin (Mac); 3.4.10 headers (Switch, with this repository's libnx shim) | window, input, audio | Zlib |
| [Dear ImGui](https://github.com/ocornut/imgui) | Aurora's pin; v1.91.9b-docking (Switch) | menus and overlays | MIT |
| [fmt](https://github.com/fmtlib/fmt) | Aurora's pin | formatting | MIT |
| [xxHash](https://github.com/Cyan4973/xxHash) | Aurora's pin | hashing | BSD-2-Clause |
| [Tracy](https://github.com/wolfpld/tracy) | Aurora's pin; v0.14.1 (headers only on the Switch) | profiler hooks (off) | BSD-3-Clause |
| [SQLite](https://sqlite.org) | Aurora's pin | pipeline and texture caches | Public domain |
| [nod](https://github.com/encounter/nod) | Aurora's prebuilt package (Mac only; the Switch uses `switch/native/nod`) | disc reader | MIT OR Apache-2.0 (check) |
| [RecompCore](https://github.com/elliotttate/RecompCore) (Dolphin) | `8ab24da` | Dolphin's DSP HLE sources (`native/tools/fetch_recompcore.sh`) | GPL-2.0-or-later |
| [Mesa](https://mesa3d.org) | 20.1.0-rc3 via devkitPro's `switch-mesa` 20.1.0-5 recipe (`pacman-packages` `f103fe88`) | Switch EGL/GLES and nouveau driver | MIT |
| [libnx](https://github.com/switchbrew/libnx), [devkitA64](https://devkitpro.org) | pinned devkitPro image digest | Switch runtime and toolchain | ISC (libnx); toolchain runtime libraries under their own licenses |
| [nx-hbloader](https://github.com/switchbrew/nx-hbloader) | v2.4.5 (`82b9512`) | HOME-menu forwarder (optional) | ISC |
| [hacBrewPack](https://github.com/TooTallNate/hacBrewPack) | v3.05 (`745b16e`) | packs the forwarder NSP (build tool only, not shipped) | GPL-2.0 |
| [hactool](https://github.com/SciresM/hactool) | in the devkitPro image (check) | verifies the forwarder NSP (build tool only) | ISC |
| [bc7enc_rdo](https://github.com/richgel999/bc7enc_rdo) | `b943862` | HD texture pack converter (`native/tools/hd_pack`) | MIT or Unlicense (bc7e.ispc: Apache-2.0, not built) |
| [decomp-toolkit](https://github.com/encounter/decomp-toolkit) (`dtk`) | the decomp's pin | asset header generation from the player's disc (`native/tools/gen_assets.sh`) | MIT OR Apache-2.0 (check) |

## Binaries

A build that includes `native/dsp_hle` (the default) links Dolphin's GPL-2.0-or-later code: the
resulting `centollos` and `switchwaker.nro` are distributed under GPL-2.0-or-later terms, with their
corresponding source. Builds contain code generated from the player's disc and are for the
player's own use only.
