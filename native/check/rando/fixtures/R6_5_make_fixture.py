#!/usr/bin/env python3
"""Writes the fixtures of trap chests and chest type matches contents (docs/RANDOMIZER_PLAN.md R6.5).

    python3 -I native/check/rando/fixtures/R6_5_make_fixture.py [--disc <GZLE01.iso>]

R6_5_wwrando_trap.txt (cos_rando_test_R6_5):

    trap_src <hex>   a synthetic event_list.dat (wwrando's wwlib/events.py, no disc bytes)
    trap_out <hex>   the same file after tweaks.py add_trap_chest_event_to_stage, run twice (the
                     second call must change nothing), and save_changes
    seed <k> <file>  a placement written by wwrando (R6_5_seed_<k>.json, as ww_export.py writes it)
    ctmc <k> <type> <trap> <location>
                     for every chest location of seed k (a path ends in "Chest..."): the chest
                     type items.py get_ctmc_chest_type_for_item gives its item after randomize_all
                     (0 light wood, 1 dark wood, 2 metal), and 1 when the item is a trap chest

R6_5_wwrando_disc.txt (smoke trap), only with --disc: for every files/res/Stage/<stage>/Stage.arc
that holds an event_list.dat, hashes only (no disc bytes):

    disc <stage> <size> <fnv64> <trap size> <trap fnv64>

the vanilla file and the file after add_trap_chest_event_to_stage and save_changes.
Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import re
import struct
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from io import BytesIO  # noqa: E402
from gclib.gclib_file import GCLibFileEntry  # noqa: E402
from wwlib.events import EventList  # noqa: E402
import tweaks  # noqa: E402

# (k, seed name, options): chest_type_matches_contents and trap_chests on; B adds required bosses (keys
# of unrequired dungeons stay light wood), C key-lunacy, Swordless and fewer progress categories (more
# items demoted to non-progress: light wood).
SEEDS = [
  ("A", "R6.5 trap A", {"trap_chests": True, "chest_type_matches_contents": True}),
  ("B", "R6.5 trap B", {"trap_chests": True, "chest_type_matches_contents": True, "required_bosses": True,
                        "num_required_bosses": 3}),
  ("C", "R6.5 trap C", {"trap_chests": True, "chest_type_matches_contents": True, "keylunacy": True,
                        "sword_mode": "Swordless", "progression_combat_secret_caves": False,
                        "progression_short_sidequests": True, "progression_minigames": True}),
]


def load(data):
  entry = GCLibFileEntry()
  entry.data = BytesIO(data)
  return EventList(entry)


def save(el):
  el.save_changes()
  return el.data.getvalue()


def fnv64(data):
  h = 0xcbf29ce484222325
  for b in data:
    h ^= b
    h = (h * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
  return h


class FakeArc:
  def __init__(self, el):
    self.el = el

  def get_file(self, name, cls):
    assert name == "event_list.dat"
    return self.el


class FakeRando:
  """What add_trap_chest_event_to_stage uses of WWRandomizer: get_arc(path).get_file(...)."""
  def __init__(self, el):
    self.arc = FakeArc(el)

  def get_arc(self, path):
    return self.arc


def add_trap(el):
  rando = FakeRando(el)
  tweaks.add_trap_chest_event_to_stage(rando, "Test")
  tweaks.add_trap_chest_event_to_stage(rando, "Test")  # already there: no change


def build_src():
  header = bytearray(0x40)
  for i in range(7):
    struct.pack_into(">I", header, i * 8, 0x40)
  el = load(bytes(header))
  ev = el.add_event("DEFAULT_TREASURE_A")
  link = ev.add_actor("Link")
  link.add_action("010open_treasure", [("prm0", [1])])
  link.add_action("011get_item", [("prm0", [255])])
  box = ev.add_actor("TREASURE")
  box.add_action("OPEN_SHORT")
  cam = ev.add_actor("CAMERA")
  cam.staff_type = 2
  cam.add_action("PAUSE")
  ev.ending_flags = [link.actions[-1].flag_id_to_set, -1, -1]
  return save(el)


def is_chest_location(logic, name):
  # change_chest_item runs for every "Chest..." path (some locations also have a main.dol path).
  paths = logic.item_locations[name]["Paths"]
  return any(p.split("/")[-1].startswith("Chest") for p in paths)


def write_main():
  src = build_src()
  if save(load(src)) != src:
    oc.die("the synthetic file does not round-trip")
  el = load(src)
  add_trap(el)
  out = save(el)
  if save(load(out)) != out:
    oc.die("the trap file does not round-trip")
  lines = ["trap_src %s" % src.hex(), "trap_out %s" % out.hex()]
  for k, seed, values in SEEDS:
    opts = oc.make_options(values)
    r = oc.make_randomizer(seed, opts)
    oc.randomize(r)
    name = "R6_5_seed_%s.json" % k
    oc.write_json(oc.placement_from_randomizer(r, opts), os.path.join(HERE, name))
    lines.append("seed %s %s" % (k, name))
    n = traps = 0
    for loc, item in r.logic.done_item_locations.items():
      if not is_chest_location(r.logic, loc):
        continue
      trap = 1 if item.endswith(" Trap Chest") else 0
      lines.append("ctmc %s %d %d %s" % (k, r.items.get_ctmc_chest_type_for_item(item), trap, loc))
      n += 1
      traps += trap
    print("seed %s: %d chest locations, %d traps" % (k, n, traps))
  path = os.path.join(HERE, "R6_5_wwrando_trap.txt")
  with open(path, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s" % path)
  write_smoke_placement()


def write_smoke_placement():
  # The smoke's placement: R0.6's identity (every location its original item) with the chest under
  # Link's house (LinkUG) a trap, trap_chests and chest_type_matches_contents on.
  d = oc.read_json(os.path.join(HERE, "R0_6_identity.json"))
  d["seed"] = "R6.5 trap smoke"
  d["options"]["trap_chests"] = True
  d["options"]["chest_type_matches_contents"] = True
  d["locations"]["Outset Island - Underneath Link's House"] = "Ice Trap Chest"
  path = os.path.join(HERE, "R6_5_trap_placement.json")
  oc.write_json(d, path)
  print("wrote %s" % path)


def write_disc(iso):
  from gclib.gcm import GCM
  from gclib.rarc import RARC
  gcm = GCM(iso)
  gcm.read_entire_disc()
  lines = []
  for path in sorted(gcm.files_by_path):
    m = re.fullmatch(r"files/res/Stage/([^/]+)/Stage\.arc", path)
    if not m:
      continue
    arc = RARC(gcm.read_file_data(path))
    entry = arc.get_file_entry("event_list.dat")
    if entry is None:
      continue
    raw = entry.data.getvalue()
    el = load(raw)
    add_trap(el)
    out = save(el)
    lines.append("disc %s %d %016x %d %016x" % (m.group(1), len(raw), fnv64(raw), len(out), fnv64(out)))
  path = os.path.join(HERE, "R6_5_wwrando_disc.txt")
  with open(path, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d stages)" % (path, len(lines)))


def main():
  write_main()
  if len(sys.argv) == 3 and sys.argv[1] == "--disc":
    write_disc(sys.argv[2])
  elif len(sys.argv) != 1:
    oc.die("usage: R6_5_make_fixture.py [--disc <GZLE01.iso>]")


if __name__ == "__main__":
  main()
