#!/usr/bin/env python3
"""Writes the fixtures of the pirate ship tweak (docs/RANDOMIZER_PLAN.md step R3.17).

    python3 -I native/check/rando/fixtures/R3_17_make_fixture.py [--disc GZLE01.iso [--disc-dir DIR]]

R3_17_wwrando_pirate_ship.txt (cos_rando_test_R3_17; no disc data): synthetic versions of the 4 files
wwrando's tweaks.py add_pirate_ship_to_windfall edits, shaped like the vanilla ones (sea/Room11
room.dzr with a layer-2 Pirates; Asoko/Room0 room.dzr with P2b and Ashut on layers 2 and 3;
Asoko/Stage stage.dzs with two EVNT entries; Asoko/Stage event_list.dat with one event). The tweak
itself runs on them (a stand-in randomizer object records its message, main.dol and REL writes):

    file <stage> <arc> <name>    starts a file
    src <hex>                    the synthetic file
    before <line> / after <line> the logical dump (R2_10_make_fixture.py's dump_dzx, EVNT 0x18 bytes)
    out <hex>                    event_list.dat only: the whole file after save_changes
    parts <hex> ...              message 3008's construct_string_from_parts parts (wwrando syntax)
    sound <n>                    message 3008's initial_sound
    msg <id> <box> <draw> <align> <hex string>   a new message (bmg.add_new_message)
    dol <address> <value>        a main.dol byte write
    rel <file> <offset> <value>  a REL byte write

R3_17_pirate_ship_game.txt (smoke `pirateship`; needs --disc): message 3008 as the tweak leaves it on
the disc's BMG and message font (wwrando's own text wrapped by gclib; no disc text):

    text3008 <box type> <hex string>

With --disc-dir (local only, never checked in: it holds disc bytes) the tool also writes the disc's 4
files before (<dir>/vanilla/<stage>_<arc>_<name>) and after the tweak (<dir>/ww/...), for
COS_R3_17_DISC_DIR=<dir> cos_rando_test_R3_17. Uses the oracle venv (native/tools/rando/setup_oracle.sh)
through oracle_common.bootstrap().
"""
import os
import struct
import sys
import types
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
sys.path.insert(0, HERE)
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import contextlib  # noqa: E402
from io import BytesIO  # noqa: E402
from wwlib.dzx import DZx  # noqa: E402
import tweaks  # noqa: E402  (wwrando's)
import R0_11_make_fixture as r011  # noqa: E402  (its event-list load and dump)
import R2_10_make_fixture as r210  # noqa: E402  (its dzx builders and dump)

FILES = [
  ("sea", "Room11", "room.dzr"),
  ("Asoko", "Room0", "room.dzr"),
  ("Asoko", "Stage", "stage.dzs"),
  ("Asoko", "Stage", "event_list.dat"),
]


def evnt(name):
  return b"\xFF" + name.encode().ljust(0xF, b"\0") + b"\xFF\xFF\x00\xFF\xFF" + b"\xFF" * 3


def dump_dzx(data):
  """R2_10_make_fixture.dump_dzx, with EVNT's 0x18-byte entries."""
  n = struct.unpack_from(">I", data, 0)[0]
  chunks = {}
  for i in range(n):
    tag, count, off = struct.unpack_from(">4sII", data, 4 + 12 * i)
    tag = tag.decode("latin1")
    size = {"FILI": 8, "SCLS": 0xC, "EVNT": 0x18}.get(tag)
    if size is None:
      size = 0x24 if tag in ("SCOB", "TGSC", "DOOR", "TGDR") or tag[:3] == "SCO" else 0x20
    entries = []
    for j in range(count):
      e = data[off + size * j: off + size * (j + 1)]
      if r210.is_actor_tag(tag) and e[0] == 0:
        continue
      entries.append(e.hex())
    if entries:
      chunks[tag] = entries
  out = []
  for tag in sorted(chunks):
    out.append("chunk " + tag)
    out += [" e " + e for e in chunks[tag]]
  return out


def build_files():
  a, s = r210.actor, r210.scob
  dzx = r210.dzx
  link = lambda spawn, pos, yrot: a("Link", 0xFFFF0000, pos, (0, yrot, 0xFF00 | spawn))
  rope = a("RopeR", 0xFFFF1500, (400.0, 0.0, -800.0), (0, 0, 0))
  ashut = a("Ashut", 0x0000FFC0, (0.0, -550.0, -3200.0), (0, 0, 0))
  niko = a("P2b", 0x00030005, (113.03, -550.0, -200.0), (0, 0xC000, 0))
  box = a("Ospbox", 0x0000073F, (400.0, -700.0, -475.0), (0, 0, 0))
  el = r011.load(r011.empty_file())
  ev = el.add_event("Hlift_up")
  cam = ev.add_actor("CAMERA")
  cam.staff_type = 2
  cam.add_action("FIXEDFRM", [("Eye", (1.0, 2.0, 3.0)), ("Timer", [40])])
  lift = ev.add_actor("Hlift")
  lift.add_action("UP", [("Speed", 4.5)])
  ev.ending_flags = [cam.actions[-1].flag_id_to_set, -1, -1]
  el.save_changes()
  return {
    FILES[0]: dzx([
      ("PLYR", [link(0, (669.06, 95.26, -197306.28), 0xD8E4)]),
      ("ACTR", [a("kusax1", 0x980, (-1.5, 2.5, 3.5), (0, 0xC000, 0))]),
      ("ACT2", [a("Kamome", 0xFFFFFF00, (1.0, 2.0, 3.0), (0, 0, 0)),
                a("Pirates", 0xFFFF0100, (-2795.89, 0.0, -204592.64), (0, 0x271C, 0)),
                a("Pirates", 0xFFFF0200, (-1000.0, 0.0, -1000.0), (0, 0, 0))]),
      ("ACT3", [a("Pirates", 0xFFFF0100, (-2795.6, 0.0, -204593.19), (0, 0x271C, 0), enemy=0x0012)]),
    ]),
    FILES[1]: dzx([
      ("PLYR", [a("Link", 0xFFFFB03F, (0.0, -100.0, 275.0), (0, 0, 0xFFFF))]),
      ("ACTR", [a("KNOB00", 0x001FF0FF, (0.0, -100.0, 275.0), (0x0FFF, 0x8000, 0))]),
      ("ACT2", [rope, ashut, niko, box]),
      ("ACT3", [rope, box, ashut, niko]),
      ("SCOB", [s("bonbori", 0xFFFFFFFF, (-744.86, -550.0, -181.79), (0, 0, 0), (15, 15, 15))]),
    ]),
    FILES[2]: dzx([
      ("EVNT", [evnt("Hlift_up"), evnt("RopeGame2Shut")]),
      ("PLYR", [link(0, (3.75, -100.0, 357.7), 0)]),
    ]),
    FILES[3]: el.data.getvalue(),
  }


class FakeArc:
  def __init__(self):
    self.files = {}

  def get_file(self, name, cls):
    return self.files[name]


class Recorder:
  """Stands in for main.dol and the RELs: records write_data(write_fn, address, value)."""
  def __init__(self, kind, name, log):
    self.kind, self.name, self.log = kind, name, log

  def write_data(self, fn, address, value):
    if self.kind == "dol":
      self.log.append("dol %08X %d" % (address, value))
    else:
      self.log.append("rel %s %X %d" % (self.name, address, value))


def run_tweak(objs, bmg, bfn):
  arcs = {}
  for (stage, arc, name), obj in objs.items():
    arcs.setdefault("files/res/Stage/%s/%s.arc" % (stage, arc), FakeArc()).files[name] = obj
  log = []
  fake = types.SimpleNamespace(
    get_arc=lambda path: arcs[path],
    get_rel=lambda path: Recorder("rel", os.path.basename(path), log),
    dol=Recorder("dol", "main.dol", log),
    bmg=bmg,
    bfn=bfn,
  )
  with contextlib.redirect_stdout(sys.stderr):
    tweaks.add_pirate_ship_to_windfall(fake)
  for obj in objs.values():
    obj.save_changes()
  return log


def load(key, data):
  return r011.load(data) if key[2].endswith(".dat") else DZx(BytesIO(data))


def file_bytes(obj):
  return obj.data.getvalue()


def h(s):
  return s.encode("utf-8").hex()


class FakeMsg:
  """Message 3008 in the synthetic run: records the parts instead of wrapping them."""
  def __init__(self):
    self.initial_sound = 0
    self.parts = None

  def construct_string_from_parts(self, font, parts, extra_line_length=0):
    self.parts = list(parts)
    assert extra_line_length == 0


class FakeBmg:
  def __init__(self):
    from gclib.bmg import Message
    self.Message = Message
    self.messages_by_id = {3008: FakeMsg()}
    self.added = []

  def add_new_message(self, message_id):
    m = self.Message(BytesIO(), self)
    m.message_id = message_id
    self.added.append(m)
    return m


def write_synthetic(path):
  srcs = build_files()
  objs = {k: load(k, v) for k, v in srcs.items()}
  bmg = FakeBmg()
  log = run_tweak(objs, bmg, None)
  lines = []
  for key in FILES:
    src, out = srcs[key], file_bytes(objs[key])
    lines.append("file %s %s %s" % key)
    lines.append("src " + src.hex())
    if key[2].endswith(".dat"):
      before, after = r011.dump(r011.load(src)), r011.dump(r011.load(out))
    else:
      before, after = dump_dzx(src), dump_dzx(out)
    lines += ["before " + x for x in before]
    lines += ["after " + x for x in after]
    if key[2].endswith(".dat"):
      lines.append("out " + out.hex())
  m3008 = bmg.messages_by_id[3008]
  lines.append("parts " + " ".join(h(p) for p in m3008.parts))
  lines.append("sound %d" % m3008.initial_sound)
  for m in bmg.added:
    lines.append("msg %d %d %d %d %s" % (m.message_id, m.text_box_type.value, m.initial_draw_type,
                                         m.text_alignment, h(m.string)))
  lines += log
  with open(path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s" % path, file=sys.stderr)


def write_disc(path, disc, disc_dir):
  from gclib.bfn import BFN
  from gclib.bmg import BMG
  from gclib.gcm import GCM
  from gclib.rarc import RARC
  gcm = GCM(disc)
  gcm.read_entire_disc()

  def arc(p):
    a = RARC(gcm.read_file_data(p))
    a.read()
    return a

  bmg = arc("files/res/Msg/bmgres.arc").get_file("zel_00.bmg", BMG)
  font = arc("files/res/Msg/fontres.arc").get_file("rock_24_20_4i_usa.bfn", BFN)
  srcs, objs = {}, {}
  for key in FILES:
    stage, a, name = key
    f = arc("files/res/Stage/%s/%s.arc" % (stage, a)).get_file_entry(name)
    srcs[key] = f.data.getvalue()
    objs[key] = load(key, srcs[key])
  log = run_tweak(objs, bmg, font)
  m = bmg.messages_by_id[3008]
  with open(path, "w", encoding="ascii") as f:
    f.write("# R3.17: message 3008 after add_pirate_ship_to_windfall on the disc's BMG and font "
            "(R3_17_make_fixture.py --disc; wwrando's text, no disc text)\n")
    f.write("text3008 %d %s\n" % (m.text_box_type.value, h(m.string)))
  print("wrote %s (%s)" % (path, "; ".join(log)), file=sys.stderr)
  if disc_dir:
    for sub in ("vanilla", "ww"):
      os.makedirs(os.path.join(disc_dir, sub), exist_ok=True)
    for key in FILES:
      base = "%s_%s_%s" % key
      with open(os.path.join(disc_dir, "vanilla", base), "wb") as f:
        f.write(srcs[key])
      with open(os.path.join(disc_dir, "ww", base), "wb") as f:
        f.write(file_bytes(objs[key]))
    print("wrote %s/{vanilla,ww}/ (%d files)" % (disc_dir, len(FILES)), file=sys.stderr)


def main():
  args = sys.argv[1:]
  disc = disc_dir = None
  while args:
    a = args.pop(0)
    if a == "--disc" and args:
      disc = args.pop(0)
    elif a == "--disc-dir" and args:
      disc_dir = args.pop(0)
    else:
      oc.die("usage: R3_17_make_fixture.py [--disc GZLE01.iso [--disc-dir DIR]]")
  write_synthetic(os.path.join(HERE, "R3_17_wwrando_pirate_ship.txt"))
  if disc:
    write_disc(os.path.join(HERE, "R3_17_pirate_ship_game.txt"), disc, disc_dir)
  elif disc_dir:
    oc.die("--disc-dir needs --disc")


if __name__ == "__main__":
  main()
