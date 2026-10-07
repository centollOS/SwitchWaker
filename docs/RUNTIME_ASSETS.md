# Loading the disc-derived data at run time (design note)

Status: proposal, not implemented. Written 2026-10-05.

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
