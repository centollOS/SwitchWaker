#!/usr/bin/env python3
"""Writes native/check/rando/fixtures/R7_1a_jpc.txt for cos_rando_test_R7_1a (docs/RANDOMIZER_PLAN.md
step R7.1a, the particle-file library features/rando/pc_rando_jpc.{h,cpp}).

    python3 -I native/tools/rando/make_jpc_fixture.py [out.txt]

Builds two small synthetic JPAC1-00 particle files here (no disc bytes): A (a destination bank, like
Pscene050) and B (a source bank, like Pscene035). They hold every block kind (BEM1 BSP1 ESP1 SSP1
ETX1 KFA1 FLD1 TDB1) and the quirks gclib normalises on save: a wrong particle size field, garbage
in particle header bytes 0x1E..0x1F, blocks whose size is not a multiple of 0x20, BSP1 colour key
tables out of order with gaps and stale offsets on empty tables, TDB1 garbage after the ids, TEX1
filename garbage after the NUL, a BTI with the wrong palette flag / max LOD, one with mipmap count 0
and one whose image does not start right after its header, and a texture filename shared by A and
B with different bytes. gclib (MIT, the commit wwrando pins) then:

    file <A|B> <hex>                       the synthetic file
    particle <A|B> <index> <id> <true size> <size field> <blocks> <texture names|->
                                           what JPC100 read (blocks: MAGIC:size,...)
    texture <A|B> <index> <filename> <size>
    saved <A|B> <hex>                      JPC100(file).save()
    resaved <A|B> <hex>                    the saved file read and saved again (not always equal:
                                           a TEX1 image moved to 0x40 keeps its old header offset)
    copied <hex>                           A after wwrando's add_inter_dungeon_warp_pots copy loop
                                           (tweaks.py) of 0x8161, 0x8162, 0x0001 from B, saved
    edit <id> <bsp1_prm|bsp1_env|ssp1_prm|ssp1_env> <r> <g> <b>
    editkey <id> <prm|env> <index> <r> <g> <b>
    edited <hex>                           A after those colour edits (rgb only, alpha kept), saved

Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import struct
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import copy  # noqa: E402
from io import BytesIO  # noqa: E402
from gclib.jpc import JPC100  # noqa: E402

OUT = os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R7_1a_jpc.txt")


def chunk(magic, body):
  return magic.encode() + struct.pack(">I", 8 + len(body)) + body


def filler(n, seed):
  return bytes((seed * 37 + i * 11) & 0xFF for i in range(n))


def bem1():
  b = bytearray(0x84)
  struct.pack_into(">f", b, 0x10, 1.5)
  return chunk("BEM1", bytes(b))


def bsp1(prm, env, tex_keys=(), prm_keys=(), env_keys=(), stale=False):
  """A BSP1_JPC100. The key tables are laid out env first, then prm, with gaps (not gclib's order)."""
  b = bytearray(0x58)  # chunk 0x08..0x5F
  struct.pack_into(">ff", b, 0x14 - 8, 2.0, 3.0)  # base_size
  b[0x25 - 8] = len(tex_keys)
  b[0x28 - 8] = len(prm_keys)
  b[0x29 - 8] = len(env_keys)
  b[0x2C - 8:0x30 - 8] = bytes(prm)
  b[0x30 - 8:0x34 - 8] = bytes(env)
  struct.pack_into(">ff", b, 0x34 - 8, 1.0, 1.0)  # tiling
  tail = bytearray(bytes(tex_keys))
  tail += b"\xEE" * 5  # gap
  env_off = 0x60 + len(tail)
  for t, c in env_keys:
    tail += struct.pack(">H", t) + bytes(c)
  tail += b"\xDD" * 2
  prm_off = 0x60 + len(tail)
  for t, c in prm_keys:
    tail += struct.pack(">H", t) + bytes(c)
  tail += b"\xCC" * 7
  struct.pack_into(">hh", b, 0x10 - 8, prm_off if prm_keys or stale else 0, env_off if env_keys or stale else 0)
  if stale:
    struct.pack_into(">hh", b, 0x10 - 8, 0x1234, 0x0F00)
  return chunk("BSP1", bytes(b) + bytes(tail))


def ssp1(prm, env):
  b = bytearray(0x4C)  # 0x54 bytes in all: not a multiple of 0x20
  struct.pack_into(">f", b, 0x10 - 8, 0.25)
  struct.pack_into(">hh", b, 0x28 - 8, 30, 2)
  b[0x48 - 8:0x4C - 8] = bytes(prm)
  b[0x4C - 8:0x50 - 8] = bytes(env)
  return chunk("SSP1", bytes(b))


def tdb1(ids, junk=0):
  body = struct.pack(">I", 0) + b"".join(struct.pack(">H", i) for i in ids)
  return chunk("TDB1", body + b"\xAB" * junk)


def particle(pid, blocks, num_textures, num_kfa=0, num_fld=0, size_field=None, tail=0):
  body = b"".join(blocks)
  size = 0x20 + len(body) if size_field is None else size_field
  hdr = b"JEFFjpa1" + struct.pack(">III", 0x12345678, len(blocks), size)
  hdr += bytes([num_kfa, num_fld, num_textures, 0]) + struct.pack(">HHHH", pid, 0, 0, tail)
  assert len(hdr) == 0x20
  return hdr + body


def tex1(name, fmt, w, h, image, palette=b"", pal_fmt=0, mips=1, pal_flag=None, max_lod=None,
         image_gap=0, name_junk=b""):
  bti_size = 0x20
  image_off = bti_size + image_gap
  pal_off = image_off + len(image)
  bti = struct.pack(">BBHHBB", fmt, 0, w, h, 1, 0)
  bti += bytes([(1 if palette else 0) if pal_flag is None else pal_flag, pal_fmt])
  bti += struct.pack(">HI", len(palette) // 2, pal_off if palette else 0)
  bti += struct.pack(">I", 0x00000000)
  lod = max(0, (max(mips, 1) - 1) * 8) if max_lod is None else max_lod
  bti += bytes([1, 1, 0, lod, mips, 0]) + struct.pack(">hI", 0, image_off)
  assert len(bti) == 0x20
  fname = name.encode() + b"\0" + name_junk
  fname = fname.ljust(0x14, b"\0")
  head = b"TEX1" + b"\0\0\0\0" + struct.pack(">I", 0x00C0FFEE) + fname
  assert len(head) == 0x20
  data = head + bti + filler(image_gap, 9) + image + palette
  return data[:4] + struct.pack(">I", len(data)) + data[8:]


def jpc(particles, textures):
  hdr = b"JPAC1-00" + struct.pack(">HH", len(particles), len(textures)) + filler(0x14, 3)
  return hdr + b"".join(particles) + b"".join(textures)


def build_a():
  p1 = particle(0x0001, [
    bem1(),
    bsp1((200, 100, 50, 255), (10, 20, 30, 128), tex_keys=(1, 0, 1),
         prm_keys=((0, (255, 0, 0, 255)), (15, (0, 255, 0, 200))), env_keys=((5, (1, 2, 3, 4)),)),
    ssp1((90, 80, 70, 60), (5, 6, 7, 8)),
    chunk("KFA1", filler(0x24, 1)),
    tdb1([0, 1], junk=0x0E),
  ], num_textures=2, num_kfa=1, size_field=0x40, tail=0xABCD)
  p2 = particle(0x0002, [
    bsp1((1, 1, 1, 1), (2, 2, 2, 2), stale=True),
    chunk("ETX1", bytes(0x28)),
    chunk("ESP1", bytes(0x64)),
    chunk("FLD1", filler(0x3C, 2)),
    tdb1([2]),
  ], num_textures=1, num_fld=1)
  tex_a = tex1("texA", 0x1, 8, 4, filler(32, 4), pal_flag=1)
  shared = tex1("shared", 0x9, 8, 4, filler(32, 5), palette=filler(8, 6), pal_fmt=2)
  cmpr = filler(128 + 32, 7)
  tex_b = tex1("texB", 0xE, 16, 16, cmpr, mips=2, max_lod=0, name_junk=b"JUNK")
  return jpc([p1, p2], [tex_a, shared, tex_b])


def build_b():
  p1 = particle(0x8161, [
    bsp1((9, 9, 9, 9), (8, 8, 8, 8), prm_keys=((3, (50, 60, 70, 80)),)),
    tdb1([1, 0]),
  ], num_textures=2)
  p2 = particle(0x8162, [
    bem1(),
    bsp1((7, 7, 7, 7), (6, 6, 6, 6), env_keys=((1, (11, 12, 13, 14)), (2, (21, 22, 23, 24)))),
    ssp1((1, 2, 3, 4), (5, 6, 7, 8)),
    tdb1([0], junk=3),
  ], num_textures=1)
  p3 = particle(0x0001, [bsp1((0, 0, 0, 0), (0, 0, 0, 0)), tdb1([])], num_textures=0)
  tex_c = tex1("texC", 0x4, 8, 8, filler(128, 8), mips=0, image_gap=0x20)
  shared = tex1("shared", 0x0, 8, 8, filler(32, 10))
  return jpc([p1, p2, p3], [tex_c, shared])


def saved(j):
  j.save()
  j.data.seek(0)
  return j.data.read()


EDITS = [
  (0x0001, "bsp1_prm", (11, 22, 33)),
  (0x0001, "bsp1_env", (44, 55, 66)),
  (0x0001, "ssp1_prm", (77, 88, 99)),
  (0x0001, "ssp1_env", (100, 110, 120)),
  (0x0002, "bsp1_prm", (130, 140, 150)),
]
KEY_EDITS = [
  (0x0001, "prm", 0, (1, 2, 3)),
  (0x0001, "prm", 1, (4, 5, 6)),
  (0x0001, "env", 0, (7, 8, 9)),
]


def main():
  out = sys.argv[1] if len(sys.argv) > 1 else OUT
  files = {"A": build_a(), "B": build_b()}
  lines = []
  for name, data in files.items():
    lines.append(f"file {name} {data.hex()}")
  for name, data in files.items():
    j = JPC100(BytesIO(data))
    for i, p in enumerate(j.particles):
      blocks = ",".join(f"{c.magic}:{c.size}" for c in p.chunks)
      texs = ",".join(p.tdb1.texture_filenames) or "-"
      lines.append(f"particle {name} {i} {p.particle_id:04X} {len(p.data.getvalue())} {p.size} {blocks} {texs}")
    for i, t in enumerate(j.textures):
      lines.append(f"texture {name} {i} {t.filename} {t.size}")
  for name, data in files.items():
    once = saved(JPC100(BytesIO(data)))
    lines.append(f"saved {name} {once.hex()}")
    lines.append(f"resaved {name} {saved(JPC100(BytesIO(once))).hex()}")

  # tweaks.py add_inter_dungeon_warp_pots, with A as the only destination.
  src = JPC100(BytesIO(files["B"]))
  dst = JPC100(BytesIO(files["A"]))
  for particle_id in [0x8161, 0x8162, 0x0001]:
    particle = src.particles_by_id[particle_id]
    for dest_jpc in [dst]:
      if particle_id in dest_jpc.particles_by_id:
        continue
      copied_particle = copy.deepcopy(particle)
      dest_jpc.add_particle(copied_particle)
      for texture_filename in copied_particle.tdb1.texture_filenames:
        if texture_filename not in dest_jpc.textures_by_filename:
          texture = src.textures_by_filename[texture_filename]
          copied_texture = copy.deepcopy(texture)
          dest_jpc.add_texture(copied_texture)
  lines.append(f"copied {saved(dst).hex()}")

  # Colour edits the way palettes.py shift_all_colors_in_particle writes them (.rgb, alpha kept).
  j = JPC100(BytesIO(files["A"]))
  for pid, which, rgb in EDITS:
    p = j.particles_by_id[pid]
    block = p.bsp1 if which.startswith("bsp1") else p.ssp1
    color = block.prm_color if which.endswith("prm") else block.env_color
    color.r, color.g, color.b = rgb
    lines.append(f"edit {pid:04X} {which} {rgb[0]} {rgb[1]} {rgb[2]}")
  for pid, table, index, rgb in KEY_EDITS:
    p = j.particles_by_id[pid]
    keys = p.bsp1.prm_color_anim_keys if table == "prm" else p.bsp1.env_color_anim_keys
    keys[index].color.r, keys[index].color.g, keys[index].color.b = rgb
    lines.append(f"editkey {pid:04X} {table} {index} {rgb[0]} {rgb[1]} {rgb[2]}")
  lines.append(f"edited {saved(j).hex()}")

  with open(out, "w") as f:
    f.write("# Written by native/tools/rando/make_jpc_fixture.py (synthetic files, gclib d9322523).\n")
    f.write("\n".join(lines) + "\n")
  print(f"wrote {out} ({len(lines)} lines)")


if __name__ == "__main__":
  main()
