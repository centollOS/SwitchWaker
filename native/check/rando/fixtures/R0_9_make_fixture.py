#!/usr/bin/env python3
"""Writes R0_9_wwrando_grow.txt for cos_rando_test_R0_9 (docs/RANDOMIZER_PLAN.md step R0.9).

    python3 -I native/check/rando/fixtures/R0_9_make_fixture.py

Builds a small synthetic dzr (no disc bytes): ACTR x2, TRES, an RTBL whose 2 entries share one
sub-table, LBNK (1-byte entries), SCLS, ACT3. It then lets wwrando's own wwlib/dzx.py DZx add
entities the way tweaks.py does (add_entity + field sets + save_changes) - two to existing chunks
(ACTR, ACT3), two new chunks (SCOB, PLYR), one more ACTR - and writes:

    src <hex>                the synthetic file
    add <tag> <hex entry>    each added entry, in add order, as wwrando saved it
    wwrando <hex>            the file wwrando saved

The test feeds the same adds to pc_rando_dzx_grow and checks the two files hold the same chunks
and entries (the layouts differ: wwrando rewrites the file, the port keeps vanilla bytes in place).
Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import struct
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from io import BytesIO  # noqa: E402
from wwlib.dzx import DZx, ACTR, TRES, SCOB, PLYR, DZxLayer  # noqa: E402


def actor(name, params, pos, rot, enemy=0xFFFF):
  b = name.encode().ljust(8, b"\0")
  b += struct.pack(">I", params) + struct.pack(">fff", *pos) + struct.pack(">HHHH", *rot, enemy)
  assert len(b) == 0x20
  return b


def build_src():
  chunks = [
    ("ACTR", 2, actor("Kanban", 0x11223344, (100.0, 0.0, -250.5), (0, 0x4000, 0))
                + actor("Bitem", 0x55667788, (1.0, 2.0, 3.0), (0, 0, 0))),
    ("TRES", 1, actor("takara", 0x00FF0000, (10.0, 20.0, 30.0), (0, 0, 0x0500))),
    ("RTBL", 2, None),
    ("LBNK", 1, b"\x07"),
    ("SCLS", 1, b"sea\0\0\0\0\0" + bytes([0, 44, 0, 0xFF])),
    ("ACT3", 1, actor("Pirates", 0xCAFEBABE, (5.0, 6.0, 7.0), (0, 0x8000, 0))),
  ]
  data = bytearray(4 + 12 * len(chunks))
  struct.pack_into(">I", data, 0, len(chunks))
  for i, (tag, count, body) in enumerate(chunks):
    while len(data) % 4:
      data += b"\xFF"
    off = len(data)
    if tag == "RTBL":
      body = struct.pack(">II", off + 8, off + 8) + bytes([2, 0x10, 1, 0]) + struct.pack(">I", off + 16)
      body += bytes([0x81, 0x02])
    struct.pack_into(">4sII", data, 4 + 12 * i, tag.encode(), count, off)
    data += body
  while len(data) % 0x20:
    data += b"\xFF"
  return bytes(data)


def main():
  src = build_src()
  dzx = DZx(BytesIO(src))
  added = []

  def add(cls, layer, tag, **fields):
    e = dzx.add_entity(cls, layer=layer)
    for k, v in fields.items():
      setattr(e, k, v)
    added.append((tag, e, cls.DATA_SIZE))

  add(ACTR, None, "ACTR", name="Ls1", params=0x000000FF, x_pos=-100.0, y_pos=0.0, z_pos=500.0, y_rot=0x2000)
  add(SCOB, None, "SCOB", name="SW_C00", params=0x00C3FFFF, x_pos=1.0, y_pos=2.0, z_pos=3.0,
      scale_x=20, scale_y=20, scale_z=20)
  add(PLYR, None, "PLYR", params=0xFF45, x_pos=7.0, y_pos=8.0, z_pos=9.0, y_rot=0xC000)
  add(ACTR, DZxLayer.Layer3, "ACT3", name="SwOp", params=0x12345678, x_pos=0.0, y_pos=0.0, z_pos=0.0)
  add(ACTR, None, "ACTR", name="SwOp", params=0x9ABCDEF0, x_pos=4.0, y_pos=5.0, z_pos=6.0)
  dzx.save_changes()
  out = dzx.data.getvalue()

  lines = [f"src {src.hex()}"]
  for tag, e, size in added:
    lines.append(f"add {tag} {out[e.offset:e.offset + size].hex()}")
  lines.append(f"wwrando {out.hex()}")
  path = os.path.join(HERE, "R0_9_wwrando_grow.txt")
  with open(path, "w") as f:
    f.write("\n".join(lines) + "\n")
  print(f"wrote {path}")


if __name__ == "__main__":
  main()
