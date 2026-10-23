#!/usr/bin/env python3
"""Writes the fixtures of the entrance randomizer's stage data (docs/RANDOMIZER_PLAN.md R6.12).

    python3 -I native/check/rando/fixtures/R6_12_make_fixture.py --disc <GZLE01.iso>

The entrance maps (placements as ww_export.py writes them; the runtime reads only "options" and
"entrances"):

    R6_12_seed_A.json   wwrando seed, every randomize_*_entrances option, separate sets
    R6_12_seed_B.json   wwrando seed, every option, "Mix Dungeons & Caves & Fountains"
    R6_12_seed_C.json   wwrando seed, dungeons + miniboss + boss + inner caves, mixed (nesting)
    R6_12_seed_D.json   hand-made (R0_6_identity.json's items): the vanilla connections except
                        Fire Mountain's cave entrance -> Ice Ring Isle Secret Cave and Ice Ring Isle's
                        -> Fire Mountain Secret Cave (the VolTag type swap); secret cave entrances on
    R0_6_identity.json  (existing) no entrance option: nothing may change

R6_12_wwrando_entr.txt (smoke entr), derived values only, no disc bytes:

    file <stage> <room>                    every stage file the step can touch (room -1: stage.dzs,
                                           else Room<room>.arc's room.dzr)
    map <k> <placement file>               k in A B C D V
    scls <k> <stage> <room> <tag> <i> <dest> <room_index> <spawn_id>
    plyr <k> <stage> <room> <tag> <i> <params hex>
    dma <k> <stage> <room> <tag> <i> <sector byte hex>
    actr <k> <stage> <room> <tag> <i> <name or -> <params hex>

A record is a field of a vanilla entry (chunk <tag>, index <i> in that chunk) that wwrando's
EntranceRandomizer._save (update_all_entrance_destinations: update_entrance_to_lead_to_exit for every
exit) changes, with its value afterwards. Map V has no record (is_enabled is false, _save never runs).
wwrando runs it on files that apply_necessary_tweaks already edited; the two tweaks that touch these
files' chunks (add_failsafe_id_0_spawns, add_spawns_outside_boss_doors: PLYR entries appended at the
chunk end) are applied first, so the spawn lookups find what wwrando finds; changes to appended
entries are not records (the maker checks there are none).
Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib.gcm import GCM  # noqa: E402
from gclib.rarc import RARC  # noqa: E402
from wwlib.dzx import DZx, SCLS, PLYR, ACTR, _2DMA  # noqa: E402
import randomizers.entrances as en  # noqa: E402
import tweaks  # noqa: E402

ALL_ENTRANCES = {
  "randomize_dungeon_entrances": True,
  "randomize_secret_cave_entrances": True,
  "randomize_miniboss_entrances": True,
  "randomize_boss_entrances": True,
  "randomize_secret_cave_inner_entrances": True,
  "randomize_fairy_fountain_entrances": True,
}
SEEDS = [
  ("A", "R6.12 entr A", dict(ALL_ENTRANCES)),
  ("B", "R6.12 entr B", dict(ALL_ENTRANCES, mix_entrances="Mix Dungeons & Caves & Fountains")),
  ("C", "R6.12 entr C", {"randomize_dungeon_entrances": True, "randomize_miniboss_entrances": True,
                         "randomize_boss_entrances": True, "randomize_secret_cave_inner_entrances": True,
                         "randomize_secret_cave_entrances": True,
                         "mix_entrances": "Mix Dungeons & Caves & Fountains"}),
]
SAVAGE_ROOMS = [("Cave10", 0), ("Cave10", 20), ("Cave11", 0)]


class Disc:
  def __init__(self, iso):
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    self.arcs = {}

  def get_arc(self, path):
    if path not in self.arcs:
      self.arcs[path] = RARC(self.gcm.read_file_data(path))
    return self.arcs[path]

  def dzx(self, stage, room):
    if room < 0:
      return self.get_arc("files/res/Stage/%s/Stage.arc" % stage).get_file("stage.dzs", DZx)
    return self.get_arc("files/res/Stage/%s/Room%d.arc" % (stage, room)).get_file("room.dzr", DZx)


def involved_files():
  files = []
  def add(stage, room):
    if (stage, room) not in files:
      files.append((stage, room))
  for e in en.ZoneEntrance.all.values():
    add(e.stage_name, -1)
    add(e.stage_name, e.room_num)
  for x in en.ZoneExit.all.values():
    add(x.stage_name, -1)
    add(x.stage_name, x.room_num)
  for stage, room in SAVAGE_ROOMS:
    add(stage, -1)
    add(stage, room)
  return files


def snapshot(dzx):
  """{(tag, i): fields} of the chunks the step edits."""
  out = {}
  for chunk in dzx.chunks:
    t = chunk.chunk_type
    if t not in (SCLS, PLYR, _2DMA, ACTR):
      continue
    tag = chunk.fourcc
    for i, e in enumerate(chunk.entries):
      if t is SCLS:
        out[(tag, i)] = ("scls", e.dest_stage_name, e.room_index, e.spawn_id)
      elif t is PLYR:
        out[(tag, i)] = ("plyr", e.params)
      elif t is _2DMA:
        out[(tag, i)] = ("dma", (e.sector_x & 0xF) | ((e.sector_y & 0xF) << 4))
      else:
        out[(tag, i)] = ("actr", e.name, e.params)
  return out


def record(k, stage, room, key, v):
  tag, i = key
  if v[0] == "scls":
    return "scls %s %s %d %s %d %s %d %d" % (k, stage, room, tag, i, v[1], v[2], v[3])
  if v[0] == "plyr":
    return "plyr %s %s %d %s %d %08x" % (k, stage, room, tag, i, v[1])
  if v[0] == "dma":
    return "dma %s %s %d %s %d %02x" % (k, stage, room, tag, i, v[1])
  return "actr %s %s %d %s %d %s %08x" % (k, stage, room, tag, i, v[1] or "-", v[2])


def run_save(iso, k, r, files):
  """wwrando's _save on a fresh copy of the disc's files; the record lines."""
  disc = Disc(iso)
  vanilla = {f: snapshot(disc.dzx(*f)) for f in files}
  tweaks.add_failsafe_id_0_spawns(disc)
  tweaks.add_spawns_outside_boss_doors(disc)
  before = {f: snapshot(disc.dzx(*f)) for f in files}
  r.get_arc = disc.get_arc
  r.entrances.update_all_entrance_destinations()
  lines = []
  for f in files:
    after = snapshot(disc.dzx(*f))
    for key, v in after.items():
      if key not in before[f]:
        oc.die("%s/%d: %s %d appeared" % (f[0], f[1], key[0], key[1]))
      if before[f][key] == v:
        continue
      if key not in vanilla[f]:
        oc.die("%s/%d: wwrando changed an entry a tweak appended (%s %d)" % (f[0], f[1], key[0], key[1]))
      lines.append(record(k, f[0], f[1], key, v))
  return lines


def write_seed(k, seed, values):
  opts = oc.make_options(values)
  r = oc.make_randomizer(seed, opts)
  oc.randomize(r)
  name = "R6_12_seed_%s.json" % k
  oc.write_json(oc.placement_from_randomizer(r, opts), os.path.join(HERE, name))
  return r, name


def make_hand_map():
  """Map D: R0_6_identity.json with the Fire Mountain / Ice Ring Isle cave entrances swapped."""
  d = oc.read_json(os.path.join(HERE, "R0_6_identity.json"))
  d["seed"] = "R6.12 entr D"
  d["options"]["randomize_secret_cave_entrances"] = True
  conns = dict(d["entrances"])
  conns["Secret Cave Entrance on Fire Mountain"] = "Ice Ring Isle Secret Cave"
  conns["Secret Cave Entrance on Ice Ring Isle"] = "Fire Mountain Secret Cave"
  d["entrances"] = conns
  name = "R6_12_seed_D.json"
  oc.write_json(d, os.path.join(HERE, name))
  opts = oc.make_options({"randomize_secret_cave_entrances": True})
  r = oc.make_randomizer("R6.12 entr D", opts)
  er = r.entrances
  er.done_entrances_to_exits.clear()
  er.done_exits_to_entrances.clear()
  for en_name, ex_name in conns.items():
    zen = en.ZoneEntrance.all[en_name]
    zex = en.ZoneExit.all[ex_name]
    er.done_entrances_to_exits[zen] = zex
    er.done_exits_to_entrances[zex] = zen
  er.entrance_connections = conns
  return r, name


def main():
  if len(sys.argv) != 3 or sys.argv[1] != "--disc":
    oc.die("usage: R6_12_make_fixture.py --disc <GZLE01.iso>")
  iso = sys.argv[2]
  files = involved_files()
  lines = ["file %s %d" % f for f in files]
  maps = []
  for k, seed, values in SEEDS:
    r, name = write_seed(k, seed, values)
    maps.append((k, name, r))
  r, name = make_hand_map()
  maps.append(("D", name, r))
  ident = oc.read_json(os.path.join(HERE, "R0_6_identity.json"))
  if any(ident["options"].get(o) for o in ALL_ENTRANCES):
    oc.die("R0_6_identity.json has an entrance option on")
  for k, name, _ in maps:
    lines.append("map %s %s" % (k, name))
  lines.append("map V R0_6_identity.json")
  for k, name, r in maps:
    recs = run_save(iso, k, r, files)
    kinds = {}
    for rec in recs:
      kinds[rec.split()[0]] = kinds.get(rec.split()[0], 0) + 1
    print("map %s (%s): %d records %s" % (k, name, len(recs), kinds))
    lines += recs
  path = os.path.join(HERE, "R6_12_wwrando_entr.txt")
  with open(path, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  print("wrote %s (%d files)" % (path, len(files)))


if __name__ == "__main__":
  main()
