#!/usr/bin/env python3
"""Fixture of the randomizer's GX texture codec (docs/RANDOMIZER_PLAN.md step R7.2b).

    python3 -I native/tools/rando/make_texture_fixture.py [--out FILE]

Writes native/check/rando/fixtures/R7_2b_textures.txt (read by test_R7_2b.cpp) with gclib's own
texture_utils / BTI code (build/rando-deps/gclib at d9322523, MIT) and wwrando's own
PaletteRandomizer.shift_all_colors_in_bti (ref/wwrando, MIT). No disc data: every texture is
synthetic, made by the small generators below, which test_R7_2b.cpp repeats exactly. The fixture
therefore holds only parameters and FNV-1a 64 hashes; each line also carries the hash of its input,
so a generator that drifts shows up as an input mismatch rather than a codec one.

Generators (xorshift32: x ^= x << 13; x ^= x >> 17; x ^= x << 5, 32-bit; a byte is next() >> 24):
  bytes(seed, n)          n bytes
  c14(seed, n, ncolors)   n big-endian u16s: v = next(); ((v >> 16) & 0xC000) | ((v & 0xFFFF) % ncolors)
  rgba(kind, seed, w, h)  an RGBA image, row-major:
    0 random RGBA          1 random RGB, opaque
    2 opaque gradient      r = x*255//max(1,w-1), g = y*255//max(1,h-1), b = (x+y)*255//max(1,w+h-2)
    3 gradient + alpha     as 2, a = (x*16 + y*8) & 255
    4 few colours          k = 1 + seed % 7 colours (r, g, b bytes, a = [0,8,128,255,255,255][byte % 6]),
                           then each pixel = colours[byte % k]
    5 few opaque colours   k = 2 + seed % 15, as 4 with a = 255 (no alpha byte drawn)
    6 near-solid           base r, g, b bytes; each pixel channel = clamp(base + byte % 5 - 2), a = 255
  The palette of a raw texture is bytes(seed ^ 0x5A5A5A5A, ncolors * 2).

Lines (formats and palette formats as gclib's numbers; hashes are 16 hex digits):
  D fmt pal ncolors w h seed in out
      decode_image of one level: input image = bytes(seed, level size) (c14 for C14X2) || palette;
      out = the RGBA bytes.
  E fmt pal kind seed w h mips in status ncolors img pal
      encode_image of rgba(kind, ...) with mips levels; palette formats through
      generate_new_palettes_from_images + encode_mipmap_image + encode_palette (encode_image minus its
      imagequant step); status ok or toomany (TooManyColorsError); in = hash of the RGBA input.
  S fmt pal ncolors w h mips src seed hshift vshift status img pal
      wwrando's shift_all_colors_in_bti on a BTI whose image is bytes(seed, all mips) (c14 for C14X2;
      src r) or encode_image(rgba(src, seed, w, h)) (src = the kind digit); status ok or skipped;
      img/pal = the BTI's image and palette data after the shift. hsv_shift_color is gclib's.
  X kind seed w h base repl ib in out
      color_exchange(rgba(kind, ...), base, repl, ignore_bright=ib) without a mask; base/repl RRGGBB.
  T fmt kind seed w h mips base repl ib img
      customizer.py's texture recolour: a BTI from encode_image(rgba(kind, ...)), render(),
      color_exchange, replace_image; img = the BTI's image data.
"""
import argparse
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import contextlib  # noqa: E402
import struct  # noqa: E402
from io import BytesIO  # noqa: E402

from PIL import Image  # noqa: E402

from gclib import texture_utils as tu  # noqa: E402
from gclib.bti import BTI  # noqa: E402
from gclib.gx_enums import ImageFormat, PaletteFormat  # noqa: E402

with contextlib.redirect_stdout(sys.stderr):
  from randomizers.palettes import PaletteRandomizer  # noqa: E402

M32 = 0xFFFFFFFF


class XS:
  def __init__(self, seed):
    self.x = (seed & M32) or 1

  def next(self):
    x = self.x
    x ^= (x << 13) & M32
    x ^= x >> 17
    x ^= (x << 5) & M32
    self.x = x
    return x

  def byte(self):
    return self.next() >> 24


def gen_bytes(seed, n):
  r = XS(seed)
  return bytes(r.byte() for _ in range(n))


def gen_c14(seed, n, ncolors):
  r = XS(seed)
  out = bytearray()
  for _ in range(n):
    v = r.next()
    out += struct.pack(">H", ((v >> 16) & 0xC000) | ((v & 0xFFFF) % ncolors))
  return bytes(out)


def gen_rgba(kind, seed, w, h):
  r = XS(seed)
  px = []
  if kind in (4, 5):
    k = 1 + seed % 7 if kind == 4 else 2 + seed % 15
    cols = []
    for _ in range(k):
      c = (r.byte(), r.byte(), r.byte())
      a = [0, 8, 128, 255, 255, 255][r.byte() % 6] if kind == 4 else 255
      cols.append(c + (a,))
  if kind == 6:
    base = (r.byte(), r.byte(), r.byte())
  for y in range(h):
    for x in range(w):
      if kind == 0:
        px.append((r.byte(), r.byte(), r.byte(), r.byte()))
      elif kind == 1:
        px.append((r.byte(), r.byte(), r.byte(), 255))
      elif kind in (2, 3):
        c = (x * 255 // max(1, w - 1), y * 255 // max(1, h - 1), (x + y) * 255 // max(1, w + h - 2))
        px.append(c + ((255,) if kind == 2 else (((x * 16 + y * 8) & 255),)))
      elif kind in (4, 5):
        px.append(cols[r.byte() % len(cols)])
      elif kind == 6:
        px.append(tuple(max(0, min(255, b + r.byte() % 5 - 2)) for b in base) + (255,))
  im = Image.new("RGBA", (w, h))
  im.putdata(px)
  return im


def rgba_bytes(im):
  return im.convert("RGBA").tobytes()


def fnv(data):
  h = 0xCBF29CE484222325
  for b in data:
    h ^= b
    h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
  return "%016x" % h


def level_size(fmt, w, h):
  bw, bh, bs = tu.BLOCK_WIDTHS[fmt], tu.BLOCK_HEIGHTS[fmt], tu.BLOCK_DATA_SIZES[fmt]
  return ((w + bw - 1) // bw) * ((h + bh - 1) // bh) * bs


def image_size(fmt, w, h, mips):
  total = 0
  for _ in range(mips):
    total += level_size(fmt, w, h)
    w //= 2
    h //= 2
  return total


def max_mips(w, h, cap=4):
  n = 1
  while n < cap and (w >> n) > 0 and (h >> n) > 0:
    n += 1
  return n


def raw_image(fmt, seed, size, ncolors):
  if fmt == ImageFormat.C14X2:
    return gen_c14(seed, size // 2, ncolors)
  return gen_bytes(seed, size)


def make_bti(fmt, pal, ncolors, w, h, mips, image, palette):
  hdr = bytearray(0x20)
  hdr[0] = fmt.value
  struct.pack_into(">HH", hdr, 2, w, h)
  hdr[8] = 1 if fmt in tu.IMAGE_FORMATS_THAT_USE_PALETTES else 0
  hdr[9] = pal.value
  struct.pack_into(">H", hdr, 0xA, ncolors)
  struct.pack_into(">I", hdr, 0xC, 0x20 + len(image))
  hdr[0x18] = mips
  struct.pack_into(">I", hdr, 0x1C, 0x20)
  return BTI(BytesIO(bytes(hdr) + image + palette))


def encode_indexed(im, fmt, pal, mips):
  encoded_colors, idx = tu.generate_new_palettes_from_images([im], fmt, pal)
  palette = tu.encode_palette(encoded_colors, pal, fmt)  # raises TooManyColorsError first
  data = bytearray()
  mw, mh = im.size
  for i in range(mips):
    mip = im
    if i != 0:
      mw //= 2
      mh //= 2
      mip = im.resize((mw, mh), Image.Resampling.NEAREST)
    data += tu.encode_mipmap_image(mip, fmt, idx, mw, mh).getvalue()
  return bytes(data), palette.getvalue(), len(encoded_colors)


FORMATS = [ImageFormat.I4, ImageFormat.I8, ImageFormat.IA4, ImageFormat.IA8, ImageFormat.RGB565,
           ImageFormat.RGB5A3, ImageFormat.RGBA32, ImageFormat.C4, ImageFormat.C8, ImageFormat.C14X2,
           ImageFormat.CMPR]
PALETTES = [PaletteFormat.IA8, PaletteFormat.RGB565, PaletteFormat.RGB5A3]
SIZES = [(1, 1), (3, 5), (4, 4), (8, 8), (13, 7), (16, 16), (37, 29), (64, 32)]
NCOLORS = {ImageFormat.C4: 16, ImageFormat.C8: 256, ImageFormat.C14X2: 300}


def palettes_for(fmt):
  return PALETTES if fmt in tu.IMAGE_FORMATS_THAT_USE_PALETTES else [PaletteFormat.IA8]


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--out", default=os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures",
                                                "R7_2b_textures.txt"))
  args = ap.parse_args()
  lines = ["# R7.2b GX texture codec fixture, written by native/tools/rando/make_texture_fixture.py",
           "# (gclib d9322523 texture_utils/bti, wwrando 1.10.0 palettes.py; synthetic data only)."]
  rng = XS(0x7E2B)

  # D: decode
  for fmt in FORMATS:
    for pal in palettes_for(fmt):
      nc = NCOLORS.get(fmt, 0)
      for (w, h) in SIZES:
        for _ in range(2):
          seed = rng.next()
          img = raw_image(fmt, seed, level_size(fmt, w, h), max(nc, 1))
          palette = gen_bytes(seed ^ 0x5A5A5A5A, nc * 2)
          out = tu.decode_image(BytesIO(img), BytesIO(palette), fmt, pal, nc, w, h)
          lines.append(f"D {fmt.value} {pal.value} {nc} {w} {h} {seed} {fnv(img + palette)} "
                       f"{fnv(rgba_bytes(out))}")

  # E: encode
  for fmt in FORMATS:
    for pal in palettes_for(fmt):
      kinds = range(7)
      if fmt == ImageFormat.C4:
        kinds = [4, 5]
      elif fmt == ImageFormat.C8:
        kinds = [2, 4, 5, 6]
      for kind in kinds:
        for (w, h) in SIZES:
          for mips in sorted({1, max_mips(w, h)}):
            seed = rng.next()
            im = gen_rgba(kind, seed, w, h)
            src = fnv(rgba_bytes(im))
            if fmt in tu.IMAGE_FORMATS_THAT_USE_PALETTES:
              try:
                data, palette, nc = encode_indexed(im, fmt, pal, mips)
                status = "ok"
              except (tu.TooManyColorsError, AssertionError):  # C4/C8: gclib asserts, C14X2 raises
                data, palette, nc, status = b"", b"", 0, "toomany"
            else:
              data, palette, _ = tu.encode_image(im, fmt, pal, mipmap_count=mips)
              data, palette, nc, status = data.getvalue(), b"", 0, "ok"
              assert len(data) == image_size(fmt, w, h, mips)
            lines.append(f"E {fmt.value} {pal.value} {kind} {seed} {w} {h} {mips} {src} {status} {nc} "
                         f"{fnv(data)} {fnv(palette)}")

  # S: wwrando's texture shift
  shift_sizes = [(1, 1), (3, 5), (8, 8), (13, 7), (37, 29), (64, 32)]
  for fmt in FORMATS:
    for pal in palettes_for(fmt):
      nc = NCOLORS.get(fmt, 0)
      sources = ["r"]
      if fmt not in tu.IMAGE_FORMATS_THAT_USE_PALETTES:
        sources += ["2", "3", "6", "0"]
      for src in sources:
        for (w, h) in shift_sizes:
          for mips in sorted({1, max_mips(w, h)}):
            seed = rng.next()
            hs = 20 + rng.next() % 321
            vs = [-40, 0, 40][seed % 3] if seed % 4 == 0 else (rng.next() % 81) - 40
            if src == "r":
              img = raw_image(fmt, seed, image_size(fmt, w, h, mips), max(nc, 1))
            else:
              img = tu.encode_image(gen_rgba(int(src), seed, w, h), fmt, pal, mipmap_count=mips)[0].getvalue()
            palette = gen_bytes(seed ^ 0x5A5A5A5A, nc * 2)
            bti = make_bti(fmt, pal, nc, w, h, mips, img, palette)
            before = (bti.image_data.getvalue(), bti.palette_data.getvalue())
            PaletteRandomizer.shift_all_colors_in_bti(None, "synthetic", bti, hs, vs)
            after = (bti.image_data.getvalue(), bti.palette_data.getvalue())
            skipped = fmt in tu.GREYSCALE_IMAGE_FORMATS or (
              fmt in tu.IMAGE_FORMATS_THAT_USE_PALETTES and pal in tu.GREYSCALE_PALETTE_FORMATS)
            if skipped:
              assert after == before
            lines.append(f"S {fmt.value} {pal.value} {nc} {w} {h} {mips} {src} {seed} {hs} {vs} "
                         f"{'skipped' if skipped else 'ok'} {fnv(after[0])} {fnv(after[1])}")

  # X: colour exchange on pixels
  for kind in range(7):
    for (w, h) in [(1, 1), (5, 3), (16, 16), (37, 29)]:
      for ib in (0, 1):
        seed = rng.next()
        base = (rng.byte(), rng.byte(), rng.byte())
        repl = (rng.byte(), rng.byte(), rng.byte())
        if seed % 5 == 0:
          base = (base[0], base[0], base[0])  # a grey base (s = 0)
        im = gen_rgba(kind, seed, w, h)
        src = fnv(rgba_bytes(im))
        out = tu.color_exchange(im, base, repl, ignore_bright=bool(ib))
        lines.append(f"X {kind} {seed} {w} {h} {'%02x%02x%02x' % base} {'%02x%02x%02x' % repl} {ib} {src} "
                     f"{fnv(rgba_bytes(out))}")

  # T: colour exchange on a texture
  for fmt in FORMATS:
    if fmt in tu.IMAGE_FORMATS_THAT_USE_PALETTES:
      continue
    for kind in (1, 3, 6):
      for (w, h) in [(3, 5), (16, 16), (37, 29)]:
        mips = max_mips(w, h)
        seed = rng.next()
        base = (rng.byte(), rng.byte(), rng.byte())
        repl = (rng.byte(), rng.byte(), rng.byte())
        ib = seed & 1
        img = tu.encode_image(gen_rgba(kind, seed, w, h), fmt, PaletteFormat.IA8, mipmap_count=mips)[0].getvalue()
        bti = make_bti(fmt, PaletteFormat.IA8, 0, w, h, mips, img, b"")
        image = tu.color_exchange(bti.render(), base, repl, ignore_bright=bool(ib))
        bti.replace_image(image)
        lines.append(f"T {fmt.value} {kind} {seed} {w} {h} {mips} {'%02x%02x%02x' % base} "
                     f"{'%02x%02x%02x' % repl} {ib} {fnv(bti.image_data.getvalue())}")

  with open(args.out, "w") as f:
    f.write("\n".join(lines) + "\n")
  print(f"wrote {args.out}: {len(lines) - 2} cases")


if __name__ == "__main__":
  main()
