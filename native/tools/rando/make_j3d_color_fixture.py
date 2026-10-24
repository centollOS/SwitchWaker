#!/usr/bin/env python3
"""Fixture of the randomizer's material and animation colours (docs/RANDOMIZER_PLAN.md step R7.2d).

    python3 -I native/tools/rando/make_j3d_color_fixture.py [--out FILE]

Writes native/check/rando/fixtures/R7_2d_j3d.txt (read by test_R7_2d.cpp) with gclib's own J3D code
(build/rando-deps/gclib at d9322523, MIT) and wwrando 1.10.0's own PaletteRandomizer methods
(randomizers/palettes.py, ref/wwrando, MIT). No disc data: every file is synthetic, built by the small
writers below from fixed parameters.

Cases:
  - "bdl": a J3D2 bdl4 file with MAT3 and MDL3 chunks. The raw MAT3 below goes through gclib once
    (J3D.save: MAT3 re-saved in gclib's layout, MDL3 generated from the materials) so the input is in
    gclib's own layout; the generator checks that a second gclib save leaves it byte-identical. The
    materials share tev / konst colour list entries and hold a negative tev colour (skipped by
    wwrando) and one above 255. Every material sets its 4 tev and 4 konst colours: wwrando's
    shift_all_colors_in_mat3 fails on an unset one (the port skips those).
  - "bmd": the same materials, MAT3 only (no MDL3).
  - "brk": J3D1 brk1 files with one TRK1 chunk written raw (not in gclib's layout): register and
    konst animations, a material name used twice (gclib groups the animations by name), one-key
    tracks, tangent types IN and IN_OUT, R/G/B tracks with different key times (the Gohma case,
    wwrando's closest-time rule), duplicated times, track data shared between animations and between
    a value and a time; one file is named cc.brk (wwrando skips its animation 1).

For each case and shift (h, v) the expected file is wwrando's shift_colors_for_file_group J3D branch:
shift_all_colors_in_mat3 / _mdl3 / _trk1 when the chunk exists, then J3D.save() (which regenerates
MDL3 from the materials and relays TRK1 out).

Lines:
  F <name> <file name> <input hex>
  S <name> <h> <v> <expected hex>
"""
import argparse
import os
import struct
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import contextlib  # noqa: E402
from io import BytesIO  # noqa: E402

from gclib import fs_helpers as fs  # noqa: E402
from gclib.j3d import J3D  # noqa: E402
from randomizers.palettes import PaletteRandomizer  # noqa: E402

SHIFTS = [(20, 0), (120, -20), (200, 40), (340, -40), (77, 13)]

PAD = b"This is padding data to alignme"


def be16(v):
  return struct.pack(">H", v & 0xFFFF)


def be32(v):
  return struct.pack(">I", v & 0xFFFFFFFF)


def align(b, n, pad=PAD):
  need = (n - len(b) % n) % n
  return b + (pad * (need // len(pad) + 1))[:need]


def string_table(names):
  out = be16(len(names)) + be16(0xFFFF)
  off = 4 + 4 * len(names)
  body = b""
  for s in names:
    h = 0
    for c in s:
      h = (h * 3 + ord(c)) & 0xFFFF
    out += be16(h) + be16(off)
    body += s.encode("ascii") + b"\0"
    off += len(s) + 1
  return out + body


# ---- MAT3 ----------------------------------------------------------------------------------------

def mat3_chunk(materials, tev_colors, konst_colors, remap):
  """materials: list of dicts {tev: [4 indexes or None], konst: [4 indexes or None]}."""
  n = len(remap)
  hdr_size = 0x84
  mat_data = b""
  for m in materials:
    rec = bytearray(b"\xFF" * 0x14C)
    rec[0] = 1  # pixel engine mode Opaque
    rec[1] = 0  # cull mode index 0
    rec[2] = 0  # num color chans index
    rec[3] = 0  # num tex gens index
    rec[4] = 0  # num tev stages index
    rec[5] = 0  # z compare index
    rec[6] = 0  # z mode index
    rec[7] = 0  # dither index
    rec[0x08:0x0A] = be16(0)  # material colour 0
    for i in range(4):
      rec[0x0C + i * 2:0x0E + i * 2] = be16(0)  # colour channels
    rec[0x14:0x16] = be16(0)  # ambient colour 0
    for i, idx in enumerate(m["konst"]):
      rec[0x94 + i * 2:0x96 + i * 2] = be16(0xFFFF if idx is None else idx)
    for i in range(16):
      rec[0x9C + i] = 0x0C  # konst colour sel K0
      rec[0xAC + i] = 0x1C  # konst alpha sel K0_A
    for i, idx in enumerate(m["tev"]):
      rec[0xDC + i * 2:0xDE + i * 2] = be16(0xFFFF if idx is None else idx)
    rec[0x144:0x146] = be16(0)  # fog
    rec[0x146:0x148] = be16(0)  # alpha compare
    rec[0x148:0x14A] = be16(0)  # blend mode
    rec[0x14A:0x14C] = be16(0)  # nbt scale
    mat_data += bytes(rec)
  body = bytearray()
  offs = {}

  def put(name, data, al=4):
    nonlocal body
    offs[name] = hdr_size + len(body)
    body += data
    body = bytearray(align(bytes(body), al))

  put("material_data", mat_data)
  put("remap", b"".join(be16(r) for r in remap))
  put("names", string_table(["mat%d" % i for i in range(n)]))
  ind = b""
  for _ in range(n):
    rec = b"\x00\x00\xFF\xFF"
    rec += b"\xFF\xFF\xFF\xFF" * 4  # tev orders
    rec += (struct.pack(">6f", 0.5, 0, 0, 0, 0.5, 0) + b"\x01\xFF\xFF\xFF") * 3
    rec += b"\x00\x00\xFF\xFF" * 4  # scales
    rec += (b"\x00" * 9 + b"\xFF\xFF\xFF") * 16
    assert len(rec) == 0x138
    ind += rec
  put("indirect", ind)
  put("cull", be32(2) + be32(1) + be32(0))
  put("mat_color", b"\xC0\xC0\xC0\xFF")
  put("num_color_chans", b"\x01")
  put("color_channel", b"\x00\x00\x00\x02\x02\x00\xFF\xFF")
  put("ambient", b"\x32\x32\x32\xFF")
  put("num_tex_gens", b"\x00")
  put("num_tev_stages", b"\x01")
  put("tev_color", b"".join(struct.pack(">4h", *c) for c in tev_colors))
  put("tev_konst", b"".join(bytes(c) for c in konst_colors))
  put("fog", b"\x02\x00" + be16(320) + struct.pack(">4f", 10.0, 2000.0, 10.0, 10000.0) + b"\x80\x80\x80\xFF" +
      b"".join(be16(0x100) for _ in range(10)))
  put("alpha_compare", b"\x07\x00\x00\x07\xFF\xFF\xFF\xFF")
  put("blend", b"\x00\x01\x00\x03")
  put("z_mode", b"\x01\x03\x01\xFF")
  put("z_compare", b"\x00\x01")
  put("dither", b"\x00\x01")
  put("nbt", b"\x00\xFF\xFF\xFF" + struct.pack(">3f", 1.0, 1.0, 1.0))
  hdr = bytearray(hdr_size)
  hdr[0:4] = b"MAT3"
  hdr[8:10] = be16(n)
  hdr[0xA:0xC] = be16(0xFFFF)
  order = ["material_data", "remap", "names", "indirect", "cull", "mat_color", "num_color_chans", "color_channel", "ambient", None,
           "num_tex_gens", None, None, None, None, None, None, "tev_color", "tev_konst", "num_tev_stages", None,
           None, None, "fog", "alpha_compare", "blend", "z_mode", "z_compare", "dither", "nbt"]
  assert len(order) == 30
  for i, name in enumerate(order):
    hdr[0x0C + i * 4:0x10 + i * 4] = be32(offs[name] if name else 0)
  chunk = bytes(hdr) + bytes(body)
  chunk = bytearray(align(chunk, 0x20))
  chunk[4:8] = be32(len(chunk))
  return bytes(chunk)


def mdl3_stub(n):
  hdr = bytearray(0x40)
  hdr[0:4] = b"MDL3"
  hdr[8:10] = be16(n)
  hdr[0xA:0xC] = be16(0xFFFF)
  body = bytearray()
  packets = 0x40
  entries = packets + 8 * n
  entries = (entries + 0x1F) & ~0x1F
  data = bytearray(entries - 0x40 + 0x20 * n)
  for i in range(n):
    po = packets + 8 * i
    eo = entries + 0x20 * i
    data[po - 0x40:po - 0x40 + 8] = be32(eo - po) + be32(0x20)
  body += data
  mtx = 0x40 + len(body)
  for i in range(n):
    body += struct.pack(">f", 0.0297617912) + struct.pack("<f", 0.0253844261)
  pem = 0x40 + len(body)
  body += bytes([1] * n)
  body = bytearray(align(bytes(body), 4))
  idx = 0x40 + len(body)
  for i in range(n):
    body += be16(i)
  body = bytearray(align(bytes(body), 4))
  names = 0x40 + len(body)
  body += string_table(["mat%d" % i for i in range(n)])
  hdr[0x0C:0x10] = be32(packets)
  hdr[0x10:0x14] = be32(0)
  hdr[0x14:0x18] = be32(mtx)
  hdr[0x18:0x1C] = be32(pem)
  hdr[0x1C:0x20] = be32(idx)
  hdr[0x20:0x24] = be32(names)
  chunk = bytearray(align(bytes(hdr) + bytes(body), 0x20))
  chunk[4:8] = be32(len(chunk))
  return bytes(chunk)


def j3d_file(magic, ftype, chunks):
  hdr = bytearray(b"\xFF" * 0x20)
  hdr[0:4] = magic
  hdr[4:8] = ftype
  body = b"".join(chunks)
  hdr[8:12] = be32(0x20 + len(body))
  hdr[0xC:0x10] = be32(len(chunks))
  return bytes(hdr) + body


# ---- TRK1 ----------------------------------------------------------------------------------------

def trk1_chunk(reg, konst, tables):
  """reg/konst: lists of (mat name, [(count, index, tangent) x4], color_id); tables: 8 lists of s16
  (reg r g b a, konst r g b a)."""
  out = bytearray(0x60)
  out[0:4] = b"TRK1"
  out[8] = 2
  out[9] = 0xFF
  out[0xA:0xC] = be16(60)
  out[0xC:0xE] = be16(len(reg))
  out[0xE:0x10] = be16(len(konst))
  for i in range(8):
    out[0x10 + i * 2:0x12 + i * 2] = be16(len(tables[i]))
  offs = {}

  def put(name, data, al=4):
    nonlocal out
    offs[name] = len(out)
    out += data
    out = bytearray(align(bytes(out), al))

  for kind, anims in (("reg", reg), ("konst", konst)):
    data = b""
    for _, tracks, cid in anims:
      for (c, ix, t) in tracks:
        data += be16(c) + be16(ix) + be16(t)
      data += bytes([cid, 0xFF, 0xFF, 0xFF])
    put(kind + "_anims", data)
  for i, t in enumerate(tables):
    put("tab%d" % i, b"".join(struct.pack(">h", v) for v in t))
  put("reg_remap", b"".join(be16(i) for i in range(len(reg))))
  put("konst_remap", b"".join(be16(i) for i in range(len(konst))))
  put("reg_names", string_table([a[0] for a in reg]))
  put("konst_names", string_table([a[0] for a in konst]))
  out[0x20:0x24] = be32(offs["reg_anims"] if reg else 0)
  out[0x24:0x28] = be32(offs["konst_anims"] if konst else 0)
  out[0x28:0x2C] = be32(offs["reg_remap"])
  out[0x2C:0x30] = be32(offs["konst_remap"])
  out[0x30:0x34] = be32(offs["reg_names"])
  out[0x34:0x38] = be32(offs["konst_names"])
  for i in range(8):
    out[0x38 + i * 4:0x3C + i * 4] = be32(offs["tab%d" % i] if tables[i] else 0)
  out = bytearray(align(bytes(out), 0x20))
  out[4:8] = be32(len(out))
  return bytes(out)


def brk_cases():
  # Tables: reg r, g, b, a, konst r, g, b, a. IN_OUT keys: time, value, tin, tout; IN: time, value, t.
  reg_r = [0, 200, 0, 0, 30, 40, 0, 0, 60, 200, 0, 0,     # 0: 3 IN_OUT keys at 0, 30, 60
           120,                                           # 12: one-key value
           0, 10, 5, 45, 90, 7]                           # 13: 2 IN keys at 0, 45 (value 10, 90)
  reg_g = [0, 50, 0, 0, 60, 180, 1, 1,                    # 0: 2 IN_OUT keys at 0, 60 (Gohma case)
           77,                                            # 8: one-key value shared below
           0, 33, 2, 2, 20, 99, 3, 3, 20, 140, 4, 4]      # 9: 3 IN_OUT keys, time 20 twice
  reg_b = [0, 255, 0, 0, 15, 100, 0, 0, 45, 10, 0, 0, 60, 128, 0, 0,  # 0: 4 IN_OUT keys
           0, 30]                                         # 16: value 30 at index 17; index 16 (0) is both
                                                          #     a time of track 0 and a one-key value
  reg_a = [255, 0, 255, 0, 0]
  k_r = [0, 300, 0, 0, 30, -20, 0, 0, 90]                 # values above 255 and negative (& 0xFF)
  k_g = [64, 0, 10, 0, 0, 30, 250, 0, 0]
  k_b = [0, 1, 0, 0, 30, 2, 0, 0, 200]
  k_a = [128]
  tables = [reg_r, reg_g, reg_b, reg_a, k_r, k_g, k_b, k_a]
  IN, IO = 0, 1
  reg = [
    ("body", [(3, 0, IO), (2, 0, IO), (4, 0, IO), (1, 0, IO)], 1),
    ("eye", [(1, 12, IO), (1, 8, IO), (1, 16, IO), (1, 2, IO)], 2),
    ("body", [(2, 13, IN), (3, 9, IO), (1, 17, IO), (1, 4, IO)], 3),   # "body" again: grouped first
    ("tail", [(1, 12, IO), (1, 8, IO), (1, 17, IO), (1, 0, IO)], 0),   # shares eye's r, g
  ]
  konst = [
    ("eye", [(2, 0, IO), (1, 0, IO), (2, 0, IO), (1, 0, IO)], 0),
    ("fin", [(1, 8, IO), (2, 1, IN), (1, 8, IO), (1, 0, IO)], 1),
  ]
  chunk = trk1_chunk(reg, konst, tables)
  return j3d_file(b"J3D1", b"brk1", [chunk])


def gclib_save(raw):
  j = J3D(BytesIO(raw))
  j.save()
  return fs.read_all_bytes(j.data)


def wwrando_shift(raw, file_name, h, v):
  pr = PaletteRandomizer.__new__(PaletteRandomizer)  # its shift_* methods use no state
  j = J3D(BytesIO(raw))
  if j.tex1 is not None:
    pr.shift_all_colors_in_tex1(file_name, j.tex1, h, v)
  if j.mat3 is not None:
    pr.shift_all_colors_in_mat3(file_name, j.mat3, h, v)
  if j.mdl3 is not None:
    pr.shift_all_colors_in_mdl3(file_name, j.mdl3, h, v)
  if j.trk1 is not None:
    pr.shift_all_colors_in_trk1(file_name, j.trk1, h, v)
  j.save()
  return fs.read_all_bytes(j.data)


def model_cases():
  tev_colors = [
    (255, 0, 0, 255), (10, 200, 30, 128), (-1, 50, 60, 255), (300, 120, 40, 255), (128, 128, 128, 0),
    (0, 0, 0, 255), (90, 60, 30, 200),
  ]
  konst_colors = [(255, 255, 255, 255), (200, 40, 120, 255), (0, 128, 255, 64), (50, 50, 50, 255),
                  (255, 200, 0, 255)]
  materials = [
    {"tev": [0, 1, 2, 6], "konst": [0, 1, 4, 2]},
    {"tev": [3, 1, 5, 4], "konst": [3, 3, 1, 0]},
    {"tev": [4, 5, 6, 0], "konst": [4, 2, 2, 3]},
    {"tev": [2, 3, 0, 1], "konst": [2, 4, 0, 1]},
  ]
  remap = [0, 1, 2, 3, 1]  # material 4 uses material 1's record
  mat3 = mat3_chunk(materials, tev_colors, konst_colors, remap)
  bdl = j3d_file(b"J3D2", b"bdl4", [mat3, mdl3_stub(len(remap))])
  bmd = j3d_file(b"J3D2", b"bmd3", [mat3])
  out = []
  for name, raw, fname in (("bdl", bdl, "synth.bdl"), ("bmd", bmd, "synth.bmd")):
    canon = gclib_save(raw)
    again = gclib_save(canon)
    if again != canon:
      oc.die("%s: gclib's layout is not stable over a second save" % name)
    out.append((name, fname, canon))
  return out


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--out", default=os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures",
                                                "R7_2d_j3d.txt"))
  args = ap.parse_args()
  cases = model_cases()
  brk = brk_cases()
  cases.append(("brk", "synth.brk", brk))
  cases.append(("ccbrk", "cc.brk", brk))
  lines = ["# R7.2d fixture: native/tools/rando/make_j3d_color_fixture.py (gclib d9322523 + wwrando 1.10.0 "
           "palettes.py, MIT; synthetic files only)"]
  with contextlib.redirect_stdout(sys.stderr):
    for name, fname, raw in cases:
      lines.append("F %s %s %s" % (name, fname, raw.hex()))
      for h, v in SHIFTS:
        exp = wwrando_shift(raw, fname, h, v)
        if name in ("bdl", "bmd") and len(exp) != len(raw):
          oc.die("%s %d,%d: gclib relaid the file out (%d -> %d bytes); pick other colours"
                 % (name, h, v, len(raw), len(exp)))
        lines.append("S %s %d %d %s" % (name, h, v, exp.hex()))
  with open(args.out, "w") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d cases x %d shifts)" % (args.out, len(cases), len(SHIFTS)))
  return 0


if __name__ == "__main__":
  sys.exit(main())
