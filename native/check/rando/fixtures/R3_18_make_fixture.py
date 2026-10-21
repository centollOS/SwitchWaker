#!/usr/bin/env python3
"""Writes the fixture of the Tower of the Gods servants tweak (docs/RANDOMIZER_PLAN.md step R3.18).

    python3 -I native/check/rando/fixtures/R3_18_make_fixture.py --disc <GZLE01.iso> [--dump <dir>]

Runs wwrando's own tweaks.allow_nonlinear_servants_of_the_towers on the USA disc's
files/res/Stage/Siren/Stage.arc (stage.dzs, event_list.dat) and Room7.arc (room.dzr), saves them as
wwrando does, and writes R3_18_wwrando_totg.txt with hashes only (no disc bytes):

    evl <vanilla size> <vanilla fnv64> <patched size> <patched fnv64>
        the whole event_list.dat (the port's serializer must give wwrando's bytes exactly);
    dzs <vanilla fnv64> <patched fnv64>
    dzr <vanilla fnv64> <patched fnv64>
        content hashes (content_hash below) of stage.dzs's TGDR and EVNT chunks and of room.dzr's
        ACTR and SCOB chunks: the port grows a dzx by appending at the end of the file
        (pc_rando_dzx_grow) where wwrando rewrites it, so the files differ in layout, and other
        patchers (other steps) may edit other chunks of the same files.

content_hash: 64-bit FNV-1a over, for each tag in the given order, the 4 tag bytes and then every
entry of that chunk in order (actor entries whose name is empty are left out: wwrando's remove_entity
is a cleared name in the port). The smoke (smokes/pc_rando_smoke_totg.cpp) and the tweak's status
(features/rando/pc_rando_tweak_totg.h) compute the same hashes.

--dump <dir> (dev only, never committed: disc bytes) also writes <dir>/vanilla_<file> and
<dir>/wwrando_<file> for the three files, for test_R3_18's COS_R3_18_DISC_DIR mode.
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

from gclib.gcm import GCM  # noqa: E402
from gclib.rarc import RARC  # noqa: E402
from wwlib.dzx import DZx  # noqa: E402
from wwlib.events import EventList  # noqa: E402
import tweaks  # noqa: E402  (wwrando's)

SIZES = {"TGDR": 0x24, "EVNT": 0x18, "ACTR": 0x20, "SCOB": 0x24}
ACTOR_TAGS = ("TGDR", "ACTR", "SCOB")


def fnv64(data, h=0xcbf29ce484222325):
  for b in data:
    h ^= b
    h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
  return h


def content_hash(data, tags):
  n = struct.unpack_from(">I", data, 0)[0]
  rows = {}
  for i in range(n):
    tag, count, off = struct.unpack_from(">4sII", data, 4 + 12 * i)
    rows.setdefault(tag.decode("latin1"), (count, off))
  h = 0xcbf29ce484222325
  for tag in tags:
    h = fnv64(tag.encode(), h)
    count, off = rows.get(tag, (0, 0))
    size = SIZES[tag]
    for j in range(count):
      e = data[off + size * j: off + size * (j + 1)]
      if tag in ACTOR_TAGS and e[0] == 0:
        continue
      h = fnv64(e, h)
  return h


def main():
  args = sys.argv[1:]
  if len(args) not in (2, 4) or args[0] != "--disc" or (len(args) == 4 and args[2] != "--dump"):
    oc.die("usage: R3_18_make_fixture.py --disc <GZLE01.iso> [--dump <dir>]")
  gcm = GCM(args[1])
  gcm.read_entire_disc()
  arcs = {}
  for name in ("Stage", "Room7"):
    path = "files/res/Stage/Siren/%s.arc" % name
    arc = RARC(gcm.read_file_data(path))
    arc.read()
    arcs[path] = arc
  stage = arcs["files/res/Stage/Siren/Stage.arc"]
  room = arcs["files/res/Stage/Siren/Room7.arc"]
  raw_evl = stage.get_file_entry("event_list.dat").data.getvalue()
  raw_dzs = stage.get_file_entry("stage.dzs").data.getvalue()
  raw_dzr = room.get_file_entry("room.dzr").data.getvalue()

  class Fake:
    def get_arc(self, path):
      return arcs[path]

  tweaks.allow_nonlinear_servants_of_the_towers(Fake())
  el = stage.get_file("event_list.dat", EventList)
  el.save_changes()
  out_evl = el.data.getvalue()
  out_dzs = stage.get_file("stage.dzs", DZx).data.getvalue()
  out_dzr = room.get_file("room.dzr", DZx).data.getvalue()

  lines = [
    "evl %d %016x %d %016x" % (len(raw_evl), fnv64(raw_evl), len(out_evl), fnv64(out_evl)),
    "dzs %016x %016x" % (content_hash(raw_dzs, ("TGDR", "EVNT")), content_hash(out_dzs, ("TGDR", "EVNT"))),
    "dzr %016x %016x" % (content_hash(raw_dzr, ("ACTR", "SCOB")), content_hash(out_dzr, ("ACTR", "SCOB"))),
  ]
  path = os.path.join(HERE, "R3_18_wwrando_totg.txt")
  with open(path, "w") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s" % path)
  print("\n".join(lines))
  if len(args) == 4:
    os.makedirs(args[3], exist_ok=True)
    for name, raw, out in (("event_list.dat", raw_evl, out_evl), ("stage.dzs", raw_dzs, out_dzs),
                           ("room.dzr", raw_dzr, out_dzr)):
      with open(os.path.join(args[3], "vanilla_" + name), "wb") as f:
        f.write(raw)
      with open(os.path.join(args[3], "wwrando_" + name), "wb") as f:
        f.write(out)
    print("dumped the files to %s" % args[3])


if __name__ == "__main__":
  main()
