#!/usr/bin/env python3
"""Writes R2_10_wwrando_added_chests.txt for cos_rando_test_R2_10 (docs/RANDOMIZER_PLAN.md step R2.10).

    python3 -I native/check/rando/fixtures/R2_10_make_fixture.py

Builds synthetic versions (no disc bytes) of the 7 files wwrando's added-chest tweaks edit, shaped
like the vanilla ones (the chunks and actor names the tweaks look for, no TRES where the chest
becomes Chest000): Pjavdou/Room0, sea/Room44, kenroom/Room0, sea/Room9 room.dzr, M_Dra09/Stage and
M_NewD2/Stage stage.dzs, and a sea/Stage event_list.dat with an "ajav_uzu" event. It then runs
wwrando's own tweaks.py functions on them (add_chest_in_place_medli_grappling_hook_gift,
add_chest_in_place_queen_fairy_cutscene, add_chest_in_place_of_jabun_cutscene,
add_chest_in_place_of_master_sword, in randomizer.py's order) and items.py's change_item for the 4
chests (Chest000 of their files, with the items of PLACED below), saves, and writes:

    file <stage> <arc> <name>   starts a file
    src <hex>                   the synthetic file
    before <line>               its logical dump
    after <line>                the logical dump after wwrando's edits

Dump of a dzx: per chunk tag in sorted order, "chunk <tag>" then " e <hex>" for each entry, leaving
out actor entries with an empty name (wwrando removes entities, the port clears their names, R0.9)
and chunks left with no entry. Dump of an event list: R0_11_make_fixture.py's. The test patches the
same src bytes with features/rando/pc_rando_tweak_added_chests.h and compares its own dumps. Uses
the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import struct
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
sys.path.insert(0, HERE)
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from io import BytesIO  # noqa: E402
from gclib.gclib_file import GCLibFileEntry  # noqa: E402
from wwlib.dzx import DZx  # noqa: E402
from wwlib.events import EventList  # noqa: E402
import tweaks  # noqa: E402  (wwrando's)
import R0_11_make_fixture as r011  # noqa: E402  (its event-list dump)

# The placed items the test sets (PCR_LOC_* of the four chests; any valid item ids).
PLACED = {
  ("Pjavdou", "Room0"): 0x31,   # Outset Island - Jabun's Cave
  ("M_Dra09", "Stage"): 0x26,   # Dragon Roost Cavern - Miniboss
  ("kenroom", "Room0"): 0x2A,   # Hyrule - Master Sword Chamber
  ("sea", "Room9"): 0x0B,       # Mother and Child Isles - Inside Mother Isle
}

ACTOR_TAGS = ("ACTR", "TRES", "SCOB", "PLYR", "TGOB", "TGSC", "DOOR", "TGDR")


def is_actor_tag(tag):
  return tag in ACTOR_TAGS or tag[:3] in ("ACT", "TRE", "SCO")


def actor(name, params, pos, rot, enemy=0xFFFF):
  b = name.encode().ljust(8, b"\0")
  b += struct.pack(">I", params) + struct.pack(">fff", *pos) + struct.pack(">HHHH", *rot, enemy)
  assert len(b) == 0x20
  return b


def scob(name, params, pos, rot, scale=(10, 10, 10)):
  return actor(name, params, pos, rot) + bytes(scale) + b"\xFF"


def fili(params):
  return struct.pack(">If", params, 0.0)


def dzx(chunks):
  data = bytearray(4 + 12 * len(chunks))
  struct.pack_into(">I", data, 0, len(chunks))
  for i, (tag, entries) in enumerate(chunks):
    while len(data) % 4:
      data += b"\xFF"
    struct.pack_into(">4sII", data, 4 + 12 * i, tag.encode(), len(entries), len(data))
    for e in entries:
      data += e
  while len(data) % 0x20:
    data += b"\xFF"
  return bytes(data)


def build_dzx_files():
  link = lambda spawn, pos, yrot: actor("Link", 0xFFFF0000, pos, (0, yrot, 0xFF00 | spawn))
  return {
    ("Pjavdou", "Room0", "room.dzr"): dzx([
      ("SCLS", [b"Pjavdou\0" + bytes([0, 0, 0, 0xFF])]),
      ("FILI", [fili(0x5FEC5002)]),
      ("PLYR", [link(0, (0.25, -51.5, -2499.5), 0)]),
      ("ACT0", [actor("Jab", 0x12345678, (10.0, -100.0, 50.0), (0, 0x8000, 0))]),
    ]),
    ("sea", "Room44", "room.dzr"): dzx([
      ("ACTR", [actor("kusax1", 0x980, (-1.5, 2.5, 3.5), (0, 0xC000, 0)),
                actor("Puti", 0xE3E43CF0, (-192215.2, 2934.3, 323720.7), (0, 0, 0))]),
      ("ACT5", [actor("Ajav", 0x1C, (-205542.06, 0.0, 324297.25), (0, 0, 0)),
                actor("Auzu", 0x971C, (-205662.34, 1.0, 329245.22), (0, 0, 0)),
                actor("c_green", 0xFFFF00FF, (-201581.08, 210.57, 315758.47), (0, 0, 0xFF))]),
      ("SCO5", [scob("TagEv", 0x01020304, (5.0, 6.0, 7.0), (0, 0, 0))]),
    ]),
    ("kenroom", "Room0", "room.dzr"): dzx([
      ("FILI", [fili(0x5FEC5F02)]),
      ("PLYR", [link(0, (-124.3, -2949.4, -3653.1), 0x8000), link(10, (-129.2, -3250.0, -7024.8), 0x8000),
                link(1, (-129.9, -3220.0, -7508.75), 0x8000)]),
      ("ACTR", [actor("zouK", 0, (-1036.8, -3400.0, -7296.5), (0, 0x5555, 0)),
                actor("VmsMS", 0xFFFFFFFF, (-124.34, -3223.0, -7823.73), (0, 0, 0)),
                actor("VmsDZ", 0xFFFFFFFF, (-124.34, -3223.0, -7823.73), (0, 0, 0)),
                actor("zouK1", 1, (-1036.5, -3400.0, -8349.1), (0, 0x2AAA, 0))]),
      ("ACT2", [actor("Tn", 0x11111111, (1.0, 2.0, 3.0), (0, 0, 0))]),
      ("ACT5", [actor("Yswdr00", 5, (-122.49, -3220.0, -7826.99), (0, 0, 0)),
                actor("p_zelda", 0xFFFF01FF, (-123.5, -3220.0, -7787.13), (0, 0x8000, 0)),
                actor("ALLdie", 0xFFFF05FF, (-150.0, -3250.0, -7000.0), (0, 0, 0)),
                actor("Tn", 0xFFFF194E, (130.0, -3220.0, -7800.0), (0xF4, 0xEAAB, 0xFF)),
                actor("Tn", 0xFFFF194E, (-420.0, -3240.0, -7800.0), (0xF5, 0x1555, 0xFF), enemy=7)]),
    ]),
    ("M_Dra09", "Stage", "stage.dzs"): dzx([
      ("TGDR", [scob("door12", 0x0000FFFF, (1.0, 2.0, 3.0), (0x0249, 0, 0))]),
      ("ACTR", [actor("Kanban", 0x40, (100.0, 13600.0, 0.0), (0, 0, 0))]),
    ]),
    ("M_NewD2", "Stage", "stage.dzs"): dzx([
      ("ACTR", [actor("Hmos1", 0, (0.0, 0.0, 0.0), (0, 0, 0))]),
      ("TRES", [actor("takara4", 0xFF300600, (6438.5, -115.8, -2036.0), (0xC, 0xA445, 0x4E80)),
                actor("takara2", 0xFF000580, (845.3, 5700.0, 1289.5), (0xA, 0xC000, 0x48FF))]),
    ]),
    ("sea", "Room9", "room.dzr"): dzx([
      ("FILI", [fili(0x1A564604)]),
      ("PLYR", [link(0, (-179900.0, 510.0, -193100.0), 0xE2D9)]),
      ("ACTR", [actor("Ocanon", 0x2A, (-180000.0, 700.0, -199000.0), (0, 0, 0))]),
      ("SCOB", [scob("Salvage", 0x00000F00, (-180100.0, 0.0, -199100.0), (0, 0, 0))]),
    ]),
  }


def build_event_list():
  el = r011.load(r011.empty_file())
  ev = el.add_event("ajav_uzu")
  ev.priority = 2
  d = ev.add_actor("DIRECTOR")
  d.staff_type = 1
  d.add_action("WAIT", [("Timer", [20])])
  d.add_action("NEXT")
  d.add_action("WAIT", [("Timer", [90])])
  c = ev.add_actor("CAMERA")
  c.staff_type = 2
  c.add_action("UNITRANS", [("Eye", (1.0, 2.0, 3.0)), ("Fovy", 60.0)])
  c.add_action("FIXEDFRM", [("Timer", [10])])
  c.add_action("UNITRANS", [("Eye", (4.0, 5.0, 6.0))])
  c.add_action("END")
  s = ev.add_actor("Ship")
  s.add_action("WAIT")
  s.add_action("FORWARD", [("Speed", 12.5)])
  s.add_action("ENTER")
  link = ev.add_actor("Link")
  link.add_action("WAIT")
  link.add_action("ENTER_CAVE")
  ev.ending_flags = [d.actions[-1].flag_id_to_set, c.actions[-1].flag_id_to_set, s.actions[-1].flag_id_to_set]
  ev2 = el.add_event("other")
  ev2.add_actor("DIRECTOR").add_action("WAIT")
  el.save_changes()
  return el.data.getvalue()


def dump_dzx(data):
  n = struct.unpack_from(">I", data, 0)[0]
  chunks = {}
  for i in range(n):
    tag, count, off = struct.unpack_from(">4sII", data, 4 + 12 * i)
    tag = tag.decode("latin1")
    size = {"FILI": 8, "SCLS": 0xC}.get(tag)
    if size is None:
      size = 0x24 if tag in ("SCOB", "TGSC", "DOOR", "TGDR") or tag[:3] == "SCO" else 0x20
    entries = []
    for j in range(count):
      e = data[off + size * j: off + size * (j + 1)]
      if is_actor_tag(tag) and e[0] == 0:
        continue
      entries.append(e.hex())
    if entries:
      chunks[tag] = entries
  out = []
  for tag in sorted(chunks):
    out.append("chunk " + tag)
    out += [" e " + e for e in chunks[tag]]
  return out


class FakeArc:
  def __init__(self, files):
    self.files = files  # name -> GCLibFile
  def get_file(self, name, cls):
    return self.files[name]


def main():
  srcs = build_dzx_files()
  srcs[("sea", "Stage", "event_list.dat")] = build_event_list()
  objs = {}
  arcs = {}
  for (stage, arc, name), src in srcs.items():
    if name.endswith(".dat"):
      obj = r011.load(src)
    else:
      obj = DZx(BytesIO(src))
    objs[(stage, arc, name)] = obj
    arcs.setdefault("files/res/Stage/%s/%s.arc" % (stage, arc), FakeArc({}))
    arcs["files/res/Stage/%s/%s.arc" % (stage, arc)].files[name] = obj

  class Fake:
    item_name_to_id = {"Grappling Hook": 0x25, "Progressive Bow": 0x27, "Nayru's Pearl": 0x69,
                       "Progressive Sword": 0x38}
    def get_arc(self, path):
      return arcs[path]

  self = Fake()
  tweaks.add_chest_in_place_medli_grappling_hook_gift(self)
  tweaks.add_chest_in_place_queen_fairy_cutscene(self)
  tweaks.add_chest_in_place_of_jabun_cutscene(self)
  tweaks.add_chest_in_place_of_master_sword(self)
  # items.py change_item for "<stage>/<arc>.arc/Chest000"
  from wwlib.dzx import TRES
  for (stage, arc), item in PLACED.items():
    obj = objs[(stage, arc, "stage.dzs" if arc == "Stage" else "room.dzr")]
    chest = obj.entries_by_type(TRES)[0]
    chest.item_id = item
    chest.save_changes()

  lines = []
  for key, src in srcs.items():
    stage, arc, name = key
    obj = objs[key]
    obj.save_changes()
    lines.append("file %s %s %s" % (stage, arc, name))
    lines.append("src " + src.hex())
    if name.endswith(".dat"):
      before = r011.dump(r011.load(src))
      after = r011.dump(r011.load(obj.data.getvalue()))
    else:
      before = dump_dzx(src)
      after = dump_dzx(obj.data.getvalue())
    lines += ["before " + x for x in before]
    lines += ["after " + x for x in after]
  path = os.path.join(HERE, "R2_10_wwrando_added_chests.txt")
  with open(path, "w") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d files)" % (path, len(srcs)))


if __name__ == "__main__":
  main()
