# Loading the disc-derived data at run time (design note)

Status: **implemented 2026-10-09** (steps 1-3 below, and the Switch build); written 2026-10-05 as a
proposal, agreed 2026-10-08 as the way to make installing easier (see
[Player distribution](#player-distribution-agreed-2026-10-08)). What was built and how it was checked:
[Implementation](#implementation-2026-10-09).

## Why the build needs the disc today

`native/tools/gen_assets.sh` runs the decompilation's own asset step against the player's disc and
writes two kinds of headers into `build/native-mac/assets/GZLE01` (`COS_ASSETS_DIR`):

| Kind | Files | What they hold | Derived from the disc? |
|---|---|---|---|
| `res/{Object,Msg,Fmap,CardIcon}/*.h` | 614 | `enum dRes_INDEX_*` resource indices (file names inside archives) | No game bytes: the decompilation commits them (`assets/GZLE01/res`) |
| `include/assets/*.h` | 163 | data arrays cut out of `main.dol` and the RELs (display lists, textures, vertex positions, colours) | **Yes**: ~282 KB of game data |

The game includes the `assets/*.h` headers from 23 source files (174 `#include`s), for example
`d_a_mant.cpp` (a 0x2000-byte texture), `d_drawlist.cpp`, `d_tree.cpp`, `d_grass.cpp` and
`m_Do_ext.cpp`. 104 of them are raw `unsigned char` arrays; 59 were converted by the decomp's
`tools/converters` into typed initialisers (`Vec`, `cXy`, `GXColor`, and material display lists
from `matDL_dis.py`). Every header carries its origin, e.g.
`// l_pg_mantle1TEX (size: 0x2000, address: 0x40, section: .data)`, and the decomp's
`config/GZLE01/config.yml` (`extract:` entries: symbol, binary, header, `custom_type`) lists them all.

Because these bytes are compiled into the executable, **every build needs the disc**, CI cannot
build the game, and no binary can be published.

## Proposal

Compile the game against **stub headers** with the same names, types, sizes and alignment but no
contents, and fill the arrays from the player's disc when the program starts.

1. **Stub generator** (`native/tools/gen_asset_stubs.py`, no disc needed). Reads the decomp's
   `config.yml` and `symbols.txt` (committed there, CC0, names and addresses only) and writes, per
   asset, a header such as

   ```c
   // l_pg_mantle1TEX: d_a_mant.rel .data+0x40, 0x2000 bytes (filled by cos_assets at start-up)
   static unsigned char l_pg_mantle1TEX[0x2000] ATTRIBUTE_ALIGN(32);
   COS_ASSET_REGISTER(l_pg_mantle1TEX, "d_a_mant", COS_SECTION_DATA, 0x40, 0x2000, COS_ASSET_U8);
   ```

   `COS_ASSET_REGISTER` adds `{address, module, section, offset, size, element type}` to a table
   through a static initialiser (the arrays are `static` per unit, so each unit registers its own
   copy). Typed assets (`Vec`, `cXy`, `GXColor`) declare their real element type and count; the
   element type tells the loader how to byte-swap. The `res/` enum headers are copied from the
   decomp checkout with no disc involved, as `gen_assets.sh --res-only` already does for CI.
   The stubs contain no game data, so they could even be committed, but generating them keeps the
   decomp as the single source.

2. **Loader** (`native/src/pc/pc_assets.cpp`, called from `pc_harness_init` right after the disc
   check, before the game's `main`). Through Aurora's disc reader (nod; `switch/native/nod` on the
   Switch) it reads `sys/main.dol` and the RELs (`files/rels/*.rel` and the Yaz0-compressed
   `files/RELS.arc`), finds each registered asset's section from the DOL header or the REL
   section table, copies its bytes, and swaps `Vec`/`cXy` floats and `GXColor` as the converters
   did (raw arrays stay big-endian, exactly as the generated headers have them). Needs a small
   RARC reader and Yaz0 decoder (about 200 lines; the game's JKR code cannot be used that early).
   Missing or mismatching data (another revision) exits with `PC_EXIT_DISC`, as the disc check does.

3. **Verification**. A `COS_SMOKE=assets` test compares every loaded array with the build-time
   headers when `COS_ASSETS_DIR` is present (byte for byte, after the same conversion), so both
   paths can coexist for a while: `-DCOS_RUNTIME_ASSETS=ON` selects the stubs. The regression runs
   with both.

4. **Consequences**. With `COS_RUNTIME_ASSETS=ON` the build needs no disc: CI builds and tests
   the real binaries (smoke tests that need no disc), and releases can ship `switchwaker` (Mac,
   Linux) and `switchwaker.nro` (Switch); players still supply the disc to run them, which they do
   already. The binaries stay GPL-2.0-or-later (Dolphin's DSP HLE), with source available.

## Implementation (2026-10-09)

Build without a disc:

```sh
native/tools/gen_assets.sh --stubs --decomp build/decomp --out build/assets-stubs/GZLE01   # PyYAML
cmake -S native -B build/native-mac-ra -G Ninja -DCOS_RUNTIME_ASSETS=ON \
    -DCOS_ASSETS_DIR=$PWD/build/assets-stubs/GZLE01
ninja -C build/native-mac-ra switchwaker
scripts/switch/build_native.sh --renderer deko3d --runtime-assets   # the NRO (also the GL one)
```

What differs from the proposal:

| Part | As built |
|---|---|
| Stubs | `native/tools/gen_asset_stubs.py` (called by `gen_assets.sh --stubs`). The declaration is derived from `symbols.txt` (`scope`, `align`, the section) and `config.yml` (`custom_type`, `custom_data.scope`, `rename`, the `name!.section:addr` form of non-unique names); a check against the disc's headers found all 163 declarations identical (type, name, linkage, alignment, size). The three `.rodata` arrays (`black_tex`, `font_data`, `msg_data`) lose `const` (they are written at start-up) and become `static`, the linkage `const` gave them. `matDL` headers stay macros of the texture name (the texture address in the display list is rewritten by `mDoLib_loadDLTexImage` before each draw on PC, so the disc's bytes serve as they are). |
| Registration | Not a static-initialiser table: several headers are included inside function bodies (`m_Do_graphic.cpp`, `d_drawlist.cpp`, `m_Do_ext.cpp`) or namespaces. `COS_ASSET_FILL` defines a `static const bool` initialised by `cos_asset_fill(array, size, id)`: at namespace scope it runs during static initialisation and records the array; inside a function it runs on the first call, after the loader, and copies at once. `cos_asset_fill` is declared in `native/include/pc/cos_pc_config.h` (included first in every unit), since a declaration inside a function or namespace would name another function. |
| Loader | `native/src/pc/runtime/pc_assets.cpp`, from `pc_aurora_init` right after `aurora_dvd_open` and the disc ID check, before `OSInit`. main.dol through Aurora's `DVDGetDOLLocation`, the RELs through `DVDOpen`/`DVDReadPrio` (Yaz0 and RARC readers of its own; dtk's archive paths start with the root node's name). Each main.dol and REL is checked against the SHA-1 the decomp's `config.yml` records for it (a SHA-1 of the file, not of game data); a mismatch exits `PC_EXIT_DISC` with a `[cos] DISC:` line. The table orders a module's arrays together, so each REL is read once. `Vec`/`cXy` floats are byte-swapped; raw arrays and `GXColor` are copied as they are. |
| Verification | `COS_SMOKE=assets` writes every array (after conversion) to `<run>/assets/` and exits; `native/tools/check_runtime_assets.py` compares them with dtk's own split of the same disc (`build/decomp/build/GZLE01/bin/assets/*.bin`, the converters' input), swapping the float arrays: **163 of 163 identical**. Then `COS_BUILD_DIR=build/native-mac-ra native/tools/regress.sh`: **all checks passed** (every smoke and run target of `regress_targets.txt`, sailing, shore foam, telescope, saves, the sweeps). Screenshots cannot be compared byte for byte: two runs of the same build already differ (11.7 % of the pixels at frame 1200, `--uncapped`). |
| CMake | `COS_RUNTIME_ASSETS` (`GameConfig.cmake`) only adds the define to `cos_pc` (the loader); the stubs need none, so switching a build directory between the two modes recompiles only the 23 units that include assets/ headers and the harness. Configuring with the wrong kind of headers for the option is an error. |
| CI | The `linux` job also builds `switchwaker` from stubs (`python3-yaml` added to the Linux image). |

## What stays out of scope

- The disc itself is still required at run time (all models, stages, audio and messages already
  come from it through DVD reads; nothing changes there).
- The pipeline cache (`initial_pipeline_cache.db`) is generated from runs of the game and holds
  only pipeline keys (GX state), no game data; it is committed in `native/data/` (2026-10-06) and
  ships next to the executable.

## Estimate

| Part | Effort |
|---|---|
| Stub generator from `config.yml`/`symbols.txt`, CMake option, both header sets building | 1–2 days |
| Loader: DOL sections, REL sections, RARC + Yaz0, typed swaps, error paths | 1–2 days |
| `assets` smoke test against the build-time headers, regression in both modes | 0.5–1 day |
| Switch (same loader through `switch/native/nod`), CI job building with stubs, release packaging | 1 day |
| **Total** | **about 4–6 working days** |

Risks: a few assets may be referenced from static initialisers that run before the loader (they
would read zeros; the `assets` smoke test plus a check that no registered array is read before
the loader ran would catch it), and converter details (`matDL_dis.py`) that turn out not to be a
pure byte copy.

## Player distribution (agreed 2026-10-08)

Players find the build of [INSTALL.md](../INSTALL.md) hard (Docker Desktop, WSL on Windows, a first
build of an hour or more). Options weighed:

| Option | Verdict |
|---|---|
| **Publish the NRO** once this design is in (the Dusklight model: Dusklight is a decompilation whose binary holds no game bytes and reads the disc at start-up; it compiles nothing on the device, only shaders on the first run, which SwitchWaker already does) | **Chosen** |
| Build on the Switch at the first start | Rejected: Horizon has no C/C++ compiler for homebrew, and the A57 cores with ~3 GB in title mode would take hours for the whole game |
| Self-contained builder per desktop system (exe / AppImage) | Not needed here once the NRO ships; it is the plan for SwitchWakerHD, whose NRO is the game's translated code and can never be published (its `docs/player-builder.md`) |
| Prebuilt toolchain image on ghcr.io with the graphics libraries | Optional stopgap only, if players complain before this lands |

What changes for players when it is done:

- GitHub Releases ship `switchwaker.nro` and `initial_pipeline_cache.db` (plus the Mac/Linux
  binaries), built by CI with `COS_RUNTIME_ASSETS=ON`.
- Install = copy those files and their own `GZLE01.iso` (revision 0, uncompressed) to
  `sdmc:/switch/switchwaker/`. No Docker, no build.
- The loader must check the disc (GZLE01 rev 0) and show a clear message on the console when it is
  missing, compressed or another revision, the checks `make_sd.sh` does today.
- `make_sd.sh` stays for developers and for people who want to build from source.
- INSTALL.md is rewritten around the download; the build moves to a "from source" section.
