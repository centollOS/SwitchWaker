#!/usr/bin/env python3
"""Writes the fixtures of required bosses at runtime (docs/RANDOMIZER_PLAN.md step R6.9).

    python3 -I native/check/rando/fixtures/R6_9_make_fixture.py [--disc GZLE01.iso]

R6_9_wwrando_bossreq.txt (cos_rando_test_R6_9; no disc data): wwrando's own
RequiredBossesRandomizer._save (randomizers/boss_reqs.py show_quest_markers_on_sea_chart_for_dungeons
and update_puppet_ganon_door_unlock_requirements) run per case on synthetic files: a GanonL/Room0
room.dzr shaped like the vanilla one (its ALLdie on switch 0x19, the KGBdor door on TGOB) and an
f_map.blo of zeros long enough for the 8 quest marker PIC1 panes. The boss islands come from
wwrando's EntranceRandomizer.get_entrance_zone_for_boss on the case's entrance map.

    case <name>                     starts a case
    seed <file>                     the placement JSON it comes from ("-" for a synthetic case)
    boss <name>                     the required bosses, in wwrando's order (self.required_bosses)
    entrance <hex entrance> <hex exit>   a synthetic case's entrance map, one per connection
    marker <index> <visible> <x> <y> <rotation>   the f_map.blo PIC1 marker after _save (all 8)
    after <line>                    the room.dzr after _save (R3_17_make_fixture.py's dump_dzx)
    src <hex>                       the synthetic room.dzr (once, before the first case)
    msg <id> <box> <draw> <align> <hex string>   message 851's fields and its unwrapped string

R6_9_bossreq_game.txt (smoke `bossreq`; needs --disc): message 851 as wwrando leaves it, wrapped by
gclib with the disc's message font (wwrando's own text, no disc text):

    text851 <box type> <hex string>

Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import json
import os
import re
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
from randomizers.boss_reqs import RequiredBossesRandomizer  # noqa: E402  (wwrando's)
from randomizers import entrances as ww_entrances  # noqa: E402
import R2_10_make_fixture as r210  # noqa: E402  (its dzx builders)
import R3_17_make_fixture as r317  # noqa: E402  (its dump_dzx)

BOSS_ORDER = ["Gohma", "Kalle Demos", "Gohdan", "Helmaroc King", "Jalhalla", "Molgera"]
MARKER_BASE = 0x43B0


def h(s):
  return s.encode("utf-8").hex()


def island_name_to_number():
  out = {}
  with open(os.path.join(oc.WWRANDO_DIR, "data", "island_names.txt"), "r") as f:
    while True:
      room = f.readline()
      if not room:
        break
      name = f.readline().strip()
      out[name] = int(re.search(r"Room(\d+)", room).group(1))
  return out


def build_room():
  """A GanonL/Room0 room.dzr shaped like the disc's: enemies and pots on ACTR with the ALLdie
  (switch_to_set 0x19) in the middle, lamps and SW_C00 on SCOB, the door KGBdor on TGOB."""
  a, s = r210.actor, r210.scob
  return r210.dzx([
    ("PLYR", [a("Link", 0xFFFF0000, (0.0, 0.0, 1000.0), (0, 0x8000, 0xFF00))]),
    ("ACTR", [a("mo2", 0x1A005001, (100.0, 0.0, -1000.0), (0x001F, 0, 0x00FF)),
              a("Tn", 0x1AFF9654, (200.0, 0.0, -2000.0), (0x009F, 0, 0x00FF)),
              a("ootubo1", 0x717FFF0A, (300.0, 0.0, -3000.0), (0, 0, 0x00FF)),
              a("ALLdie", 0xFFFF19FF, (0.0, 0.0, 0.0), (0, 0, 0)),
              a("ootubo1", 0x717FFF11, (400.0, 0.0, -4000.0), (0, 0, 0x00FF))]),
    ("SCOB", [s("bonbori", 0xFFFFFF80, (-500.0, 0.0, -100.0), (0, 0, 0)),
              s("SW_C00", 0x0000191A, (0.0, 0.0, -500.0), (0, 0, 0), (50, 50, 50))]),
    ("TGOB", [a("KGBdor", 0x0FFFF119, (-0.79443359, 1250.0, -37400.0), (0x0FFF, 0, 0xFF00))]),
  ])


class FakeArc:
  def __init__(self, files):
    self.files = files

  def get_file(self, name, cls):
    return self.files[name]

  def get_file_entry(self, name):
    return self.files[name]


class FakeEntry:
  def __init__(self, data):
    self.data = BytesIO(data)

  def decompress_data_if_necessary(self):
    pass


class FakeMsg:
  def __init__(self, message_id):
    self.message_id = message_id
    self.wrapped_with = None

  def word_wrap_string(self, font, extra_line_length=0):
    self.wrapped_with = font
    assert extra_line_length == 0


class FakeBmg:
  def __init__(self):
    self.added = []

  def add_new_message(self, message_id):
    m = FakeMsg(message_id)
    self.added.append(m)
    return m


def entrance_rando(entrance_map):
  """wwrando's EntranceRandomizer with only done_exits_to_entrances set (what
  get_entrance_zone_for_boss reads), from a placement's entrance -> exit map."""
  er = object.__new__(ww_entrances.EntranceRandomizer)
  er.done_exits_to_entrances = {
    ww_entrances.ZoneExit.all[ex]: ww_entrances.ZoneEntrance.all[en] for en, ex in entrance_map.items()
  }
  return er


def run_save(required_bosses, entrance_map, room_bytes, bmg):
  room = DZx(BytesIO(room_bytes))
  blo = FakeEntry(bytes(MARKER_BASE + 8 * 0x40))
  arcs = {
    "files/res/Stage/GanonL/Room0.arc": FakeArc({"room.dzr": room}),
    "files/res/Msg/fmapres.arc": FakeArc({"f_map.blo": blo}),
  }
  fake = types.SimpleNamespace(
    get_arc=lambda path: arcs[path],
    entrances=entrance_rando(entrance_map),
    island_name_to_number=island_name_to_number(),
    bmg=bmg,
    bfn="font",
  )
  br = object.__new__(RequiredBossesRandomizer)
  br.rando = fake
  br.required_bosses = list(required_bosses)
  with contextlib.redirect_stdout(sys.stderr):
    br._save()
  room.save_changes()
  data = blo.data.getvalue()
  markers = []
  for i in range(8):
    o = MARKER_BASE + i * 0x40
    vis = data[o + 9]
    x, y = struct.unpack_from(">hh", data, o + 0x10)
    markers.append("marker %d %d %d %d %d" % (i, vis, x, y, data[o + 0x19]))
  return room.data.getvalue(), markers


def bosses_of(locations):
  names = [loc.split(" - ", 1)[1].removesuffix(" Heart Container") for loc in locations]
  return [b for b in BOSS_ORDER if b in names]


def vanilla_entrances():
  with open(os.path.join(HERE, "R0_6_TestSeed1.json")) as f:
    return json.load(f)["entrances"]


def cases():
  out = []
  for name, path in (("abc_full", "R0_6_abc_full.json"), ("seed_B", "R6_5_seed_B.json")):
    with open(os.path.join(HERE, path)) as f:
      d = json.load(f)
    out.append((name, path, bosses_of(d["required_boss_locations"]), d["entrances"], False))
  # All six, vanilla entrances: every marker on its own island.
  out.append(("all_vanilla", "-", list(BOSS_ORDER), vanilla_entrances(), True))
  # Two bosses on Dragon Roost Island (the overlap rotation): Kalle Demos's arena behind DRI's secret
  # cave entrance and Jalhalla's behind Earth Temple's boss entrance, Earth Temple being behind DRI's
  # dungeon entrance; Molgera's arena behind the boss entrance in Forbidden Woods (on Gale Isle) and
  # Gohma's behind Dragon Roost Cavern (on Headstone Island): nested exits resolved to their island.
  e = dict(vanilla_entrances())
  e["Secret Cave Entrance on Dragon Roost Island"] = "Kalle Demos Boss Arena"
  e["Boss Entrance in Forbidden Woods"] = "Molgera Boss Arena"
  e["Boss Entrance in Wind Temple"] = "Dragon Roost Island Secret Cave"
  e["Dungeon Entrance in Forest Haven Sector"] = "Wind Temple"
  e["Dungeon Entrance on Gale Isle"] = "Forbidden Woods"
  e["Dungeon Entrance on Headstone Island"] = "Dragon Roost Cavern"
  e["Dungeon Entrance on Dragon Roost Island"] = "Earth Temple"
  out.append(("overlap", "-", ["Gohma", "Kalle Demos", "Jalhalla", "Molgera"], e, True))
  return out


def write_synthetic(path):
  src = build_room()
  lines = ["src " + src.hex()]
  msg_line = None
  for name, seed, bosses, emap, synthetic in cases():
    bmg = FakeBmg()
    out, markers = run_save(bosses, emap, src, bmg)
    lines.append("case " + name)
    lines.append("seed " + seed)
    lines += ["boss " + b for b in bosses]
    if synthetic:
      lines += ["entrance %s %s" % (h(en), h(ex)) for en, ex in emap.items()]
    lines += markers
    lines += ["after " + x for x in r317.dump_dzx(out)]
    m = bmg.added[0]
    assert m.wrapped_with == "font" and len(bmg.added) == 1
    msg_line = "msg %d %d %d %d %s" % (m.message_id, m.text_box_type.value, m.initial_draw_type,
                                       m.text_alignment, h(m.string))
  lines.append(msg_line)
  with open(path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s" % path, file=sys.stderr)


def write_disc(path, disc):
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
  room = arc("files/res/Stage/GanonL/Room0.arc").get_file_entry("room.dzr").data.getvalue()
  blo = arc("files/res/Msg/fmapres.arc").get_file_entry("f_map.blo")
  blo.decompress_data_if_necessary()
  room_obj = DZx(BytesIO(room))
  arcs = {
    "files/res/Stage/GanonL/Room0.arc": FakeArc({"room.dzr": room_obj}),
    "files/res/Msg/fmapres.arc": FakeArc({"f_map.blo": blo}),
  }
  fake = types.SimpleNamespace(
    get_arc=lambda p: arcs[p],
    entrances=entrance_rando(vanilla_entrances()),
    island_name_to_number=island_name_to_number(),
    bmg=bmg,
    bfn=font,
  )
  br = object.__new__(RequiredBossesRandomizer)
  br.rando = fake
  br.required_bosses = ["Gohma"]
  with contextlib.redirect_stdout(sys.stderr):
    br._save()
  m = bmg.messages_by_id[851]
  with open(path, "w", encoding="ascii") as f:
    f.write("# R6.9: message 851 after RequiredBossesRandomizer._save on the disc's BMG and font "
            "(R6_9_make_fixture.py --disc; wwrando's text, no disc text)\n")
    f.write("text851 %d %s\n" % (m.text_box_type.value, h(m.string)))
  print("wrote %s" % path, file=sys.stderr)


def main():
  args = sys.argv[1:]
  disc = None
  while args:
    a = args.pop(0)
    if a == "--disc" and args:
      disc = args.pop(0)
    else:
      oc.die("usage: R6_9_make_fixture.py [--disc GZLE01.iso]")
  write_synthetic(os.path.join(HERE, "R6_9_wwrando_bossreq.txt"))
  if disc:
    write_disc(os.path.join(HERE, "R6_9_bossreq_game.txt"), disc)


if __name__ == "__main__":
  main()
