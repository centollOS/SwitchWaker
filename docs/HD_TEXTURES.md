# HD textures (optional, off by default)

The native port can replace the game's textures with a Dolphin-format HD texture pack for GZLE01,
such as Hypatia's *HD Mod v2.0*. Nothing changes unless `COS_HD_TEXTURES=1`. The pack is the
player's own download; it and everything converted from it stay out of git (under `build/` or on
the player's SD card).

## How it works

- **Key** (Aurora, `lib/gfx/texture_replacement.cpp` and `lib/gx/texture.cpp`, our pin 3227d76):
  Dolphin's name `tex1_<w>x<h>[_m]_<texhash>[_<tluthash>]_<fmt>[_arb].dds`, where `texhash` is the
  XXH64 (seed 0) of the base level as stored in GX format, at the block-expanded size
  (`TexDecoder_GetTextureSizeInBytes` of the width/height rounded up to the format's block); for
  C4/C8/C14X2 `tluthash` is the XXH64 of the TLUT entries between the smallest and the largest index
  the base level uses. `$` stands for any TLUT hash (tried second) or any texture hash (third).
  `_m` (the sampler had mipmaps on) and `_arb` (arbitrary mipmaps) are not part of the lookup.
  Checked against Dolphin's `TextureInfo.cpp`/`HiresTextures.cpp`: the same rules (current Dolphin
  reads one byte per C14X2 index when computing the range; the pack only uses `$` for C14X2).
- **Hook**: Aurora resolves every texture a draw samples (`resolve_sampled_textures`). EFB copies
  (`copyTextures`) are taken first and never replaced; static textures (J3D, J2D, everything through
  `GXLoadTexObj` or display lists) are hashed once per texture object and looked up. A replacement
  keeps the game's sampler (wrap, filter); its LOD range opens to all of its mips, and UVs are
  normalized, so any size works. `_arb` replacements with mips use Dolphin's arbitrary-mipmap LOD
  rule (Aurora patch 0010).
- **Streaming**: the lookup never waits. The first time a texture is seen, a loader thread reads
  and decodes its DDS; the original texture is drawn until then. At the end of a frame up to
  `COS_HD_PUBLISH_MB` of finished textures become GPU textures and the texture objects using them
  are re-resolved. The replacement cache holds at most `COS_HD_BUDGET_MB`; least recently looked-up
  textures are evicted, never one bound in the last 60 frames; a load that cannot fit is dropped and
  retried 300 frames later (Aurora patch 0010).
- **Runtime toggle**: the options menu's *Gráficos > Texturas HD* (`COS_HD_TEXTURES` in
  `pc_settings.h`, saved in `settings.ini`) reaches `cos_hd_textures_set_enabled(bool)`
  (`native/include/pc/pc_hd_textures.h`) through a settings subscription; applied at the end of the
  frame. Without a pack, turning it on logs "no pack" and stays off (regression target
  `options-menu ... hd-textures-menu.txt`). Off unregisters everything: the picture is
  pixel-identical to a run that never turned it on (checked: LinkRM, on at frame 200, off at 400,
  frame 590 identical).

## Converting a pack

```
scripts/hd/build_hd_pack.sh ~/Downloads/"<texture pack>.7z" --max-size 1024   # Mac
scripts/hd/build_hd_pack.sh ~/Downloads/"<texture pack>.7z" --max-size 512    # Switch
```

The archive is extracted once into `build/hd-src/`. The default set is the pack's `GZL` folder (the
readme's "Main Pack Installation"); optional folders are added with `--overlay DIR` and override
files of the same name. `cos_hd_pack` (`native/tools/hd_pack`, bc7enc_rdo for BC1/BC3/BC7):

- keeps BC7/BC3/BC1, turns uncompressed RGBA/BGRA into BC7;
- caps the larger side (`--max-size`, default 1024): the first source mip that fits, else the top
  box-reduced by a power of two in linear light (alpha-weighted colour) while streaming its blocks;
- keeps the source's own mips (authored and `_arb` mipmap effects survive) and generates the rest of
  the chain down to 1x1 (2x2 box in linear light, alpha scaled to keep the 50% alpha-test coverage,
  so foliage does not thin out in the distance). Most of the pack has one level only: without mips
  the GPU would read 1024-16384 px textures for distant surfaces (aliasing and bandwidth);
- writes `index.bin` and `data00.bin`, `data01.bin`, ... (1 GiB each at most, for FAT32 and MTP),
  each texture a complete DDS (DX10 header) at a 512-byte boundary. Index layout: see
  `native/src/pc/features/pc_hd_textures.cpp`.

ASTC is not used: Maxwell (Tegra X1) samples BC7 natively at the same 8 bits per texel as ASTC 4x4;
larger ASTC blocks would save memory at a visible quality and encoding-time cost.

| Hypatia HD v2.0, GZL (5744 DDS) | size | time (10 threads) |
|---|---|---|
| source (5184 BC7, 560 BGRA8, 5637 single-level, up to 16384 px) | 9353 MiB | |
| `--max-size 1024` | 2477 MiB | 110 s |
| `--max-size 512` | 930 MiB | 55 s |

## Running

Mac: `COS_HD_TEXTURES=1 COS_HD_PACK=build/hd-pack-1024 build/native-mac/switchwaker ...` (a loose Dolphin
folder works too: `COS_HD_PACK=".../GZL"`, but it uses up to 10x the memory since the files have no
size cap and few mips).

Switch: `scripts/switch/push_hd_pack.sh build/hd-pack-512` copies the pack by file name into
`sdmc:/switch/switchwaker/native/user/hd_textures/` (data first, `index.bin` last and read
back), then `COS_HD_TEXTURES=1` in `native/env.txt`.

| variable | default | |
|---|---|---|
| `COS_HD_TEXTURES` | `0` | `1` turns the replacement on |
| `COS_HD_PACK` | `<user>/hd_textures` | converted pack dir or loose Dolphin folder |
| `COS_HD_BUDGET_MB` | 512 Switch, 1024 else | GPU memory for replacements |
| `COS_HD_PUBLISH_MB` | 4 Switch, 12 else | uploads per frame |
| `COS_HD_WORKERS` | 1 Switch, auto else | loader threads |
| `COS_HD_STATS_EVERY` | 300 | `[cos] hd-textures` line: lookups hit/miss, loads, MiB, load ms avg/max, SD reads, published, cache MiB, evictions, over-budget, pending |
| `COS_HD_CENSUS` | | file: the Dolphin name of every distinct static texture resolved (works with HD off) |
| `COS_HD_TOGGLE_FRAMES` | | `a,b,...`: flip the setting at those frames (toggle checks) |
| `COS_HD_MAX_SIZE` | `auto` | per mode (menu *Tamaño máx. texturas HD*): `auto` (512 handheld, 1024 docked), `256`, `512`, `1024`, `full` |
| `COS_HD_MAX_SIZE_FRAMES` | | `frame:size,...`: set the cap at those frames (reload checks) |
| `COS_HD_PUBLISH_US` | 2500 Switch, 3000 else | publish budget per frame, estimated GPU-side time (0: bytes only) |
| `COS_HD_COST_TEXTURE_US` / `_LEVEL_US` / `_MIB_US` | 300/100/2000 Switch, 50/5/300 else | the estimate: per texture, per mip level, per MiB |
| `COS_HD_REGISTER_US` | 2000 | game-thread time per frame for (un)registering after a toggle |
| `COS_HD_RESCAN_KB` | 2048 Switch, 8192 else | static textures looked up again per frame after a toggle (0: no limit) |
| `COS_SWITCH_GL_LEVEL_UPLOAD` | `1` | Switch: `0` restores Dawn's row-by-row compressed uploads (A/B) |

## Smooth toggling and loading (lane hd-smooth)

First hardware run (512 pack, Outset, handheld): turning HD on from the menu froze the game for
about 3 s: frames 1324-1332 took 97, 95, 266, 209, 1071, 1065 and 228 ms with only 0.4-3.5 MiB of
uploads each. The hitch lines show where: frame 1324's `end_frame` 86 ms was the registration on
the game thread (index read from the SD card plus 5744 registrations, 49.6 ms); from frame 1328
the render worker's Dawn `execute` took 200-1120 ms with 439 draws, and the GPU timer reported
up to 1130 ms for the frame.

**Cause**: GLES has no `GL_UNPACK_COMPRESSED_BLOCK_*`, so Dawn's GL backend uploads a compressed
texture one row of 4x4 blocks per `glCompressedTexSubImage2D` (`DoTexSubImage`,
`CommandBufferGL.cpp`). A 512x512 BC7 texture with its mips is 257 calls, a 1024 one 513. Mesa
20.1's nvc0 has no PBO path for compressed formats (`PIPE_CAP_SURFACE_REINTERPRET_BLOCKS` is 0), so
each call maps the unpack buffer, allocates a staging bo (nvmap IPC on Horizon), copies and queues
a GPU copy: about 0.1-0.15 ms each. 30 textures in a frame are about 7700 calls, a second.
Smaller hitches on entering an area with HD on are the same uploads for the new area's textures.
On the Mac (Metal) none of this exists: there the whole toggle costs 3 ms.

**Fixes**:

- `switch/dawn/patches/dawn-switch-gl-compressed-upload.patch`: one `glCompressedTexSubImage2D`
  per mip level; a copy whose rows are padded (WebGPU's 256-byte row alignment, mips smaller than
  64 px in BC7) is packed on the CPU first from the mapped unpack buffer. 512x512 BC7 with mips:
  257 calls -> 10. Counters for the hitch and perf-switch lines: upload time, compressed calls,
  packed copies, textures created and their creation time (`glTexStorage`).
- Aurora patch 0011: the publish budget is also a time budget per frame
  (`COS_HD_PUBLISH_US`) over an estimate of the GPU-side cost (per texture, per level, per MiB),
  since a deferred GL backend runs the work later on the render worker where the game thread
  cannot time it. On the Switch the estimate is scaled every 30 frames by what Dawn measured
  (upload + creation ns over the estimate of what was published; `scale` in the stats line).
- Turning on no longer blocks: the index is read on a helper thread, the names are registered in
  slices of `COS_HD_REGISTER_US` per frame with one shared read state per slice (no per-entry
  label, thumbnail queueing or cache clear), and instead of clearing the texture-object cache
  (every scene texture re-hashed and looked up in one frame) its entries are marked stale and
  looked up again `COS_HD_RESCAN_KB` of source bytes per frame (Aurora patch 0011, gradual
  rescan). Turning off unregisters in slices too and only marks the objects that used a
  replacement stale: they keep the HD texture until looked up again, so the originals come back
  over a few frames instead of being re-uploaded in one.
- Stats line: phase, cap, publish time per frame on the game thread, largest estimate published in
  a frame, scale, textures per frame, frames that left work for later, reloads, hashing time per
  frame, rescanned objects, and hitches (frame intervals over 40 / 100 ms since the last line, how
  many in a frame with HD work, the longest).

Mac (Outset `sea:44:206`, on at frame 300, off at 600): index read 3-4 ms (helper thread),
registered in 2 frames (2.0 ms max per frame), first textures 70 ms after the request; publishing
1.1-2.0 ms per frame at most on the game thread, 25-28 textures per frame at most; hashing
0.13-0.16 ms per frame; unregistered in 2 frames (2.0 ms max). No frame over 40 ms with HD work
that a frame without HD work does not also show (the Mac's 40-45 ms frames are unrelated noise).
HD off after a toggle: LinkRM frame 590 identical to a run that never turned it on.

Switch estimate (to be measured with the stats and hitch lines): registration ~25 ms spread over
~13 frames (2 ms each); publishing at most about 2.5 ms of GPU-side work per frame (one 512 BC7
texture is 10 level uploads, about 1-1.5 ms plus its `glTexStorage`), so the 30 textures of Outset
take about 15-25 frames; the game thread's part stays under 1-2 ms. Expected worst frame: the
normal frame plus 2-5 ms, well under 40 ms.

## Size cap per mode

One 1024 pack serves both modes: `COS_HD_MAX_SIZE` (menu row *Tamaño máx. texturas HD*, one value
per mode, `auto` = 512 handheld / 1024 docked) caps the larger side. The pack stores every level
from the top down to 1x1 right after a 148-byte DX10 header, so the loader reads the header and
then only the levels that fit (one seek), and rewrites width, height, pitch and mip count: no
other pack format is needed. A change of the setting or a dock/undock queues a reload of every
cached texture whose size differs; the current texture stays until its replacement is published
(same time budget), and loads still in flight at the old size are caught by re-checking every 30
frames for 8 s. Outset docked, 1024 -> 256 -> 1024: cache 51.8 -> 6.6 -> 51.8 MiB, 64 and 74
textures reloaded.

## NPC clothes glitch (lane hd-glitch): not the HD path

First hardware report with HD on: Outset's Joel (the kid with the stick) and Windfall people's
clothes glitched, fine with HD off. Further runs showed the same with HD never turned on, and that
which NPC is wrong changes after a scene reload: bug B9 (docs/NATIVE_PORT_PLAN.md, "Known bugs").
NPCs sharing one model with different material tables were drawn with the base model's texture
objects, and Aurora's object cache reused the first decode of a texture object whatever size and
format the display list gave it. Fixed in J3D and Aurora patch 0012. The replacement key is
computed from the texture object being drawn, so with the fix each variant looks up its own image
(checked: `npc-variants` passes with the pack at 512 and uncapped, and after six toggles and three
cap changes ending with HD off the four regions read the same means as a run that never turned
it on).
With the pack, the four regions are 9-21 (of 441) from the HD-off references: same clothes, HD
shading. No pack entry had to be excluded.

## Coverage (census)

`native/tools/hd_census.py CENSUS.txt --pack PACK` matches a census against a pack. Title, Outset
(sea 44, 900 frames), LinkRM, LinkUG, Ojhous, Ojhous2, Omasao, Onobuta and A_mori: 289 distinct
static textures, **280 replaced (96.9%)** by the GZL set (97.2% with the optional folders), no TLUT
near misses. The misses are 4x4..128x8 ramps and two small IA8/I8 textures the pack does not cover.

## Cost (Mac, Metal, M-series, 960x720 window at 2x)

Uncapped, frames 300-900, peak memory footprint of the process (`/usr/bin/time -l`):

| scene | mode | HD cache | replaced | footprint | frame avg / p95 |
|---|---|---|---|---|---|
| Outset (sea 44) | off | - | - | 672 MiB | 2.12 / 3.21 ms |
| | loose GZL folder | 390 MiB | 152 | 1470 MiB | 2.39 / 3.71 ms |
| | pack 1024 | 57.5 MiB | 157 | 750 MiB | 2.04 / 3.21 ms |
| | pack 512 | 23.7 MiB | 157 | 706 MiB | 2.08 / 3.17 ms |
| forest (A_mori) | off | - | - | 672 MiB | 1.57-3.09 / 2.1-8.3 ms |
| | loose GZL folder | 107 MiB | 90 | 946 MiB | 2.76 / 4.01 ms |
| | pack 1024 | 33.5 MiB | 90 | 727 MiB | 2.42-3.03 / 8.3-9.4 ms |
| | pack 512 | 12.3 MiB | 90 | 691 MiB | 1.85 / 2.88 ms |

Frame times are within the run-to-run noise (other work on the Mac; the forest's off runs range
1.6-3.1 ms): the Mac's GPU does not notice. Loads take 0.3-1.3 ms each on average from the pack
(up to 62 ms from the loose 4096-16384 px files), publishing at the frame end 0.2-0.4 ms at most
(14 ms with loose files).

## Switch estimate

- **Memory**: a scene holds 25-60 MiB of replacements with the 1024 pack, 12-25 MiB with the 512
  pack (plus the originals, which stay cached): small next to the ~3.2 GB available. The 512 MiB
  budget leaves room for several scenes before evictions start.
- **Loading**: SD reads at roughly 20-40 MiB/s give Outset's 24 MiB (512 pack) in about a second
  after arriving, on one low-priority loader thread; the game thread never waits, so textures turn
  sharp a moment after a scene appears. Publishing is capped at 4 MiB per frame.
- **GPU**: with mips, texture fetches per pixel stay about the same; BC7 is 8 bits per texel against
  4 for the originals' CMPR/I4/C4, and minified HD textures touch more cache lines than the
  magnified originals. The fill and blend cost that bounds the forest (docs/SWITCH_PERF_STUDY.md,
  section 8) does not change. Expect a few percent of GPU time; the 512 pack is the handheld
  choice (its texel density already exceeds 720p for most surfaces), 1024 for docked.
- **CPU**: the lookup is one hash-map probe per new texture object (the hash itself is Aurora's
  existing content hash plus one XXH64, cached); the loader thread runs at low priority.
