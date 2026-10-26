# Third-party code

Pins are the ones the build uses today; licenses marked "check" were not read from a local copy
when this was written.

SwitchWaker contains, builds against, or fetches at build time the components below. Nothing in this
list is game data: the repository contains no asset, executable or data file of the game, and every
build reads them from the player's own disc.

## In this repository

| Path | Origin | License |
|---|---|---|
| `game/` | The game's decompilation, imported from a fork ([github.com/snrubrm/tww](https://github.com/snrubrm/tww)) at `b09eebc` of the community decompilation project ([github.com/zeldaret/tww](https://github.com/zeldaret/tww)), changed by later commits of this repository under `TARGET_PC` (see `game/README.md`) | CC0-1.0 (`game/LICENSE`) |
| `native/include/helpers/{endian.h,endian_gx.hpp,endian_ssystem.h,offset_ptr.h}`, `native/src/helpers/offset_ptr.cpp` | [Dusklight](https://github.com/TwilitRealm/dusklight) (TwilitRealm) at `40457c6`; the SDK-over-Aurora layer follows Dusklight's model | CC0-1.0 |
| `native/dsp_hle/` | Adapted from elliotttate's recompilation project ([github.com/elliotttate/Wind-Waker-Recomp](https://github.com/elliotttate/Wind-Waker-Recomp)) (`runtime/host/src/dsp_hle_backend.cpp`, `apple/ios/src/dsp_common_shim.cpp`); compiles Dolphin's DSPHLE (below) | GPL-2.0-or-later (`native/dsp_hle/LICENSE`, SPDX headers in each file) |
| `native/patches/aurora/`, `switch/native/aurora/patches/` | Patches to Aurora | same as Aurora (MIT) |
| `switch/dawn/patches/` | Patches to Dawn and Tint (Horizon) | same as Dawn (BSD-3-Clause) |
| `switch/uam/` | [uam](https://github.com/devkitPro/uam) 1.1.0 at `5a5afc2` (fincs, devkitPro), the deko3d shader compiler, as vendored by [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) (`runtime/third_party/uam/` at `df8fbde`) with its patches 1-7, CMake build and `uam_api`, plus SwitchWaker's patch 8 and SwitchWakerHD's patch 9 (`switch/uam/PATCHES.md`) | zlib for uam's own files, MIT for `mesa-imported/` (from Mesa) (`switch/uam/LICENSE`); SwitchWakerHD lists its additions there under the same terms (SwitchWakerHD itself is MPL-2.0) |
| `switch/deko3d/` | [deko3d](https://github.com/devkitPro/deko3d) 0.5.0 at `5cc144d` (fincs, devkitPro), as vendored by [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) (`runtime/third_party/deko3d/` at `bed2af7`) with its patches 1-2 and CMake build (`switch/deko3d/PATCHES.md`) | zlib (`switch/deko3d/LICENSE`) |
| `native/tools/dksh_cache/dksh_cache.cpp`, `switch/deko/dksh_file.{h,cpp}`, `switch/deko/{dk.h,device.cpp,memory.cpp,descriptors.cpp,overlay.cpp,pattern.cpp}`, `switch/deko/shaders/{imgui,text}_{vsh,fsh}.glsl` | Adapted from [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) at `df8fbde` (`tools/switch/dksh_cache/dksh_cache.cpp`, `runtime/src/gfx/deko/{shader_files.{h,cpp},dk.h,backend.cpp,memory.cpp,descriptors.cpp,overlay_dk.cpp}`, `runtime/src/gfx/deko/shaders/`) | MPL-2.0 (header in each file; [mozilla.org/MPL/2.0](https://mozilla.org/MPL/2.0/)) |
| `switch/deko/shaders/dk_fsr_easu.wgsl`, `switch/deko/shaders/dk_fsr_rcas.wgsl` | Ports to WGSL of `FsrEasuF` and `FsrRcasF` from [AMD FidelityFX FSR 1.0](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (`ffx-fsr/ffx_fsr1.h`, Advanced Micro Devices, Inc.); compiled into `initial_dksh_cache.bin` | MIT (notice below) |
| `switch/deko/shaders/dk_fxaa.wgsl` | Port to WGSL of NVIDIA FXAA 3.11 (quality) by Timothy Lottes (`Fxaa3_11.h`, NVIDIA Corporation); compiled into `initial_dksh_cache.bin` | NVIDIA's notice below |
| `switch/forwarder/nx-hbloader-forwarder.patch` | Patch to nx-hbloader | same as nx-hbloader (ISC) |

The quality-of-life options (`docs/QOL_PLAN.md`, `native/src/pc/features/pc_qol.cpp`, the options
menu's "Calidad de vida" tab in `native/src/pc/features/pc_menu.cpp` and the hunks in `game/` the plan
lists) are derived from the patches and tweaks of [BetterWW](https://github.com/WideBoner/betterww)
(WideBoner, at `4501481`) and [wwrando](https://github.com/LagoLunatic/wwrando) (LagoLunatic, at
`9775811`). Their assembly patches cannot run here (the game is compiled from its decompilation), so
each patch was rewritten in the decompiled function it changes, reproducing its behaviour and its
constants (speeds, frame counts, message numbers, event bits) closely. No asset of theirs (textures,
models, icons) is in this repository. BetterWW credits LagoLunatic (BetterWW modifies his code),
brainfubar (his wwrando fork was used to make BetterWW), MelonSpeedruns (the original design of the
Brisk Sail) and JarheadHME (releases); wwrando credits MelonSpeedruns (game and graphic design),
Hypatia (textures), SageOfMirrors and LordNed (file format documentation) and CryZe (event flag
documentation). Both projects are under this license:

```
The MIT License (MIT)

Copyright (c) 2018 LagoLunatic

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The built-in randomizer (`docs/RANDOMIZER_PLAN.md`, `native/src/pc/features/rando/`,
`native/include/pc/pc_rando*.h`, `native/tools/rando/` and the hunks `docs/RANDO_HOOKS.md` lists) is a
port of [wwrando](https://github.com/LagoLunatic/wwrando) (LagoLunatic, at `9775811`, version
`1.10.0_9775811`) under the MIT license above. Its logic data (`logic/item_locations.txt`,
`logic/macros.txt`, `logic/item_types.py`, `data/item_names.txt`, `data/island_names.txt`, the seed
name word lists in `seedgen/`, and the option, chart and boss tables) is converted into checked-in
tables under `native/src/pc/features/rando/generated/`, each with a header naming its source commit;
its seed generator, assembly patches and tweaks are rewritten in C++ and in the decompiled
functions. Test fixtures in `native/check/rando/fixtures/` are produced by running wwrando's own
code (the oracle in `native/tools/rando/`) and contain no game data. No wwrando asset (textures,
models, archives) is shipped.

The FSR 1 shaders of the present pass (`dk_fsr_easu.wgsl`, `dk_fsr_rcas.wgsl`) are ports of AMD
FidelityFX FSR 1.0, under this license:

```
Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

The FXAA shader of the present pass (`dk_fxaa.wgsl`) is a port of NVIDIA FXAA 3.11, which carries
this notice:

```
NVIDIA FXAA 3.11 by TIMOTHY LOTTES

COPYRIGHT (C) 2010, 2011 NVIDIA CORPORATION. ALL RIGHTS RESERVED.
TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED
*AS IS* AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS
OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL NVIDIA
OR ITS SUPPLIERS BE LIABLE FOR ANY SPECIAL, INCIDENTAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR
LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION,
OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR INABILITY TO USE
THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.
```

The release zips carry this file and `LICENSE` (as `LICENSE.txt`) next to the NRO, since the shader
cache inside them holds these shaders compiled.

Everything else is the project's own code under the MIT license (`LICENSE`), copyright Pulpparty
and depende3000, and the SwitchWaker contributors.

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
| [libnx](https://github.com/switchbrew/libnx), [devkitA64](https://devkitpro.org) | pinned devkitPro image digest | Switch runtime and toolchain | ISC (libnx); toolchain runtime libraries under their own licenses |
| [curl](https://curl.se) (libcurl) | devkitPro's `switch-curl` in the pinned image (7.69.1, TLS through libnx's ssl service); the system's on the desktop, when found | the update check and download (`native/src/pc/features/pc_update.cpp`) | curl (MIT-style) |
| [zlib](https://zlib.net) | devkitPro's `switch-zlib`; Aurora's (system or bundled) on the desktop | reading the release zip (`pc_update_core.cpp`), uam | Zlib |
| [nx-hbloader](https://github.com/switchbrew/nx-hbloader) | v2.4.5 (`82b9512`) | HOME-menu forwarder (optional) | ISC |
| [hacBrewPack](https://github.com/TooTallNate/hacBrewPack) | v3.05 (`745b16e`) | packs the forwarder NSP (build tool only, not shipped) | GPL-2.0 |
| [hactool](https://github.com/SciresM/hactool) | in the devkitPro image (check) | verifies the forwarder NSP (build tool only) | ISC |
| [bc7enc_rdo](https://github.com/richgel999/bc7enc_rdo) | `b943862` | HD texture pack converter (`native/tools/hd_pack`) | MIT or Unlicense (bc7e.ispc: Apache-2.0, not built) |
| [decomp-toolkit](https://github.com/encounter/decomp-toolkit) (`dtk`) | the decomp's pin | asset header generation from the player's disc (`native/tools/gen_assets.sh`) | MIT OR Apache-2.0 (check) |

## Binaries

A build that includes `native/dsp_hle` (the default) links Dolphin's GPL-2.0-or-later code: the
resulting `switchwaker` and `switchwaker.nro` are distributed under GPL-2.0-or-later terms, with their
corresponding source. Builds contain code generated from the player's disc and are for the
player's own use only.
