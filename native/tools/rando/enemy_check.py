#!/usr/bin/env python3
"""enemy_check.py - checks step R7.4a (enemy actor rewrites) against wwrando's own _save on the disc.

    python3 -I native/tools/rando/enemy_check.py (--placement P.json | --seed NAME [--cli CLI] [opt=json ...]
                                                  | --vanilla) [--disc GZLE01.iso]
                                                 [--dump FILE | --run [--stage S:R:P]]

--seed generates the placement first with our generator (CLI, default build/native-mac/cos_rando_cli,
randomize_enemies=true plus the opt=json pairs) into a temporary folder under build/.

For the seed and options of a placement JSON written by our generator (cos_rando_cli, with
randomize_enemies), this tool:
  1. runs wwrando 1.10.0_9775811 in dry mode for the same seed and options (as ww_export_enemies.py)
     and checks the placement's "enemies" key equals wwrando's (the R7.3c decisions);
  2. loads the room archives of logic/enemy_locations.txt from the user's disc with gclib, applies the
     wwrando tweaks that change those records before _save (TWEAKS_BEFORE_SAVE:
     add_chest_in_place_of_master_sword copies kenroom's Layer 5 Darknuts to the default layer, the
     port's R2.10 patcher; remove_makar_kidnapping_event clears kaze/Room3's Wizzrobe spawn switch,
     R3.15), and records each enemy location's ACTR entry ("base" lines, the port's chain before this
     step's patcher);
  3. runs wwrando's own EnemyRandomizer.save_changed_enemies_and_randomize_their_params on those files
     (reset_rng() first, as _save does), and records every entry again ("enemy" lines):
         base  <path> <the 0x20-byte ACTR record, hex>
         enemy <path> <the 0x20-byte ACTR record, hex>
     (ACTR.save_changes layout; the positions written as float32, as wwrando saves the file).
  4. (R7.4b) runs add_modify_and_replace_actors_for_enemy_rando and randomize_enemies_spawned_by_objects
     on the same rng (as _save), then records the final enemy records and the actors R7.4b edits and
     adds:
         actor <label> <the 0x20/0x24-byte record>   (ALLdie conversions, modified actors, removed =
                                                     suppressed by remove_entity, the 3 stone heads as
                                                     kaze/Room2.arc/Homen027 ... over entries_by_type(ACTR))
         added <file> <tag> <index> <the appended ACTR record>
With --vanilla only the base lines are built from the plain disc (no tweak, no _save: a game whose
seed has no enemies), and the "enemy" lines must equal them.

--dump FILE  compares those lines with the enemies smoke's dump (COS_ENEMY_DUMP; exit 1 on any
             difference).
--run        runs the smoke first: native/tools/run.sh rando --stage <S:R:P> (default M_NewD2:2:1) with
             COS_RANDO_SMOKE=enemies, COS_RANDO_PLACEMENT=<placement> and COS_ENEMY_DUMP in a temporary
             folder; requires its exit 0, then compares its dump.
Without either, prints the lines.

The disc defaults to $COS_DISC, else ~/Documents/windwaker/GZLE01.iso. Nothing read from the disc is
written to the repository (plan R7 intro: compared at check time, no disc bytes committed).
Exit codes: 0 equal, 1 different or the smoke failed, 2 usage or setup error.
Uses the oracle venv (setup_oracle.sh) through oracle_common.bootstrap().
"""
import argparse
import collections
import contextlib
import json
import os
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib import fs_helpers as fs  # noqa: E402
from gclib.gcm import GCM  # noqa: E402
from gclib.rarc import RARC  # noqa: E402
from wwlib.dzx import ACTR, DZx, DZxLayer, SCOB, TGDR, DOOR  # noqa: E402
import tweaks  # noqa: E402
import ww_export_enemies  # noqa: E402

# The tweaks of randomizer.py apply_necessary_tweaks that edit enemy location records (the port's
# R2.10 and R3.15 patchers run before this step's); found by the base-line comparison.
TWEAKS_BEFORE_SAVE = ["add_chest_in_place_of_master_sword", "remove_makar_kidnapping_event"]

PATH_RE = re.compile(r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/Actor([0-9A-F]{3})$")
ANY_PATH_RE = re.compile(r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/(Actor|ScalableObject|Door|ScalableDoor)([0-9A-F]{3})$")
CHUNKS = {"Actor": ACTR, "ScalableObject": SCOB, "Door": DOOR, "ScalableDoor": TGDR}
# randomize_enemies_spawned_by_objects: (room archive, entries_by_type(ACTR) index), the port's labels.
HOMEN = [("kaze/Room2.arc", 0x27), ("kaze/Room10.arc", 0x00), ("kaze/Room10.arc", 0x09)]


class DiscRando:
  """WWRandomizer with its get_arc reading the user's disc (gclib); every other attribute is the dry
  randomizer's (item_name_to_id, options, ...)."""

  def __init__(self, iso, rando):
    self.__dict__["_rando"] = rando
    self.__dict__["gcm"] = GCM(iso)
    self.gcm.read_entire_disc()
    self.__dict__["arcs"] = {}
    # The chest the Master Sword tweak adds holds an item id (irrelevant to the enemy records); a dry
    # randomizer may not know every item name.
    self.__dict__["item_name_to_id"] = collections.defaultdict(int, getattr(rando, "item_name_to_id", {}) or {})

  def get_arc(self, path):
    if path not in self.arcs:
      self.arcs[path] = RARC(self.gcm.read_file_data(path))
    return self.arcs[path]

  def __getattr__(self, name):
    return getattr(self._rando, name)


def location_paths():
  import logic.logic as wwlogic
  out = []
  for _stage, groups in wwlogic.Logic.load_and_parse_enemy_locations().items():
    for group in groups:
      for loc in group["Enemies"]:
        out.append(loc["Path"])
  return out


def entry_of(disc, path):
  m = PATH_RE.match(path)
  if not m:
    raise Exception("Invalid actor path: %s" % path)
  arc_path = "files/res/Stage/" + m.group(1)
  layer = DZxLayer(m.group(2))
  dzx = disc.get_arc(arc_path).get_file("stage.dzs" if arc_path.endswith("Stage.arc") else "room.dzr", DZx)
  entries = dzx.entries_by_type_and_layer(ACTR, layer=layer)
  index = int(m.group(3), 16)
  return entries[index] if index < len(entries) else None


def record_hex(entry):
  if entry is None:
    return "00" * ACTR.DATA_SIZE  # no such entry (the smoke writes zeros too): kenroom without the tweak
  entry.save_changes()
  return fs.read_bytes(entry.data, entry.offset, ACTR.DATA_SIZE).hex()


def dzx_of(disc, arc):
  arc_path = "files/res/Stage/" + arc
  return disc.get_arc(arc_path).get_file("stage.dzs" if arc_path.endswith("Stage.arc") else "room.dzr", DZx)


def raw_hex(entry, cls):
  if entry is None:
    return "00" * cls.DATA_SIZE
  entry.save_changes()
  return fs.read_bytes(entry.data, entry.offset, cls.DATA_SIZE).hex()


def actor_targets():
  """R7.4b's edited records in the port's order: (label, arc, chunk class, layer, index or None, homen
  by-type index or None)."""
  import logic.logic as wwlogic
  groups = [g for _s, gs in wwlogic.Logic.load_and_parse_enemy_locations().items() for g in gs]
  out = []
  def from_path(path):
    m = ANY_PATH_RE.match(path)
    return (path, m.group(1), CHUNKS[m.group(3)], DZxLayer(m.group(2)), int(m.group(4), 16), None)
  for key in ("Actors to replace with ALLdies",):
    for g in groups:
      out += [from_path(p) for p in g.get(key, [])]
  for g in groups:
    out += [from_path(d["Path"]) for d in g.get("Existing actors to modify", [])]
  for g in groups:
    out += [from_path(p) for p in g.get("Actors to remove", [])]
  for arc, i in HOMEN:
    out.append(("%s/Homen%03X" % (arc, i), arc, ACTR, None, None, i))
  return groups, out


def actor_lines(disc, targets):
  lines = []
  for label, arc, cls, layer, index, homen in targets:
    dzx = dzx_of(disc, arc)
    if homen is not None:
      entries = dzx.entries_by_type(ACTR)
      e = entries[homen] if homen < len(entries) else None
    else:
      entries = dzx.entries_by_type_and_layer(cls, layer=layer)
      e = entries[index] if index < len(entries) else None
    lines.append("actor %s %s" % (label, raw_hex(e, cls)))
  return lines


def add_sites(groups):
  """(arc, layer, tag) of each group with "New actors to add": the layer of its first enemy."""
  out = []
  for g in groups:
    if g.get("New actors to add"):
      m = PATH_RE.match(g["Enemies"][0]["Path"])
      layer = DZxLayer(m.group(2))
      tag = "ACTR" if layer == DZxLayer.Default else "ACT%x" % layer.value
      out.append((m.group(1), layer, tag))
  return out


def lines_for(disc, paths, kind):
  return ["%s %s %s" % (kind, p, record_hex(entry_of(disc, p))) for p in paths]


def expected(iso, placement, vanilla):
  paths = location_paths()
  groups, targets = actor_targets()
  if vanilla:
    disc = DiscRando(iso, None)
    base = lines_for(disc, paths, "base")
    return base + ["enemy" + l[len("base"):] for l in base] + actor_lines(disc, targets)

  values = dict(placement.get("options", {}))
  values["randomize_enemies"] = True
  opts = oc.make_options(values)
  r = oc.make_randomizer(placement["seed"], opts)
  oc.randomize(r)
  want = ww_export_enemies.enemies_from_randomizer(r)
  if json.dumps(want, sort_keys=True) != json.dumps(placement.get("enemies"), sort_keys=True):
    print("enemy_check: the placement's \"enemies\" differ from wwrando's for this seed (R7.3c)", file=sys.stderr)
    return None

  disc = DiscRando(iso, r)
  er = r.enemies
  with contextlib.redirect_stdout(sys.stderr):
    # apply_necessary_tweaks: the two that change enemy records before _save (kenroom's Darknuts
    # copied to the default layer; kaze/Room3's Wizzrobe spawn switch cleared).
    for tweak in TWEAKS_BEFORE_SAVE:
      getattr(tweaks, tweak)(disc)
    base = lines_for(disc, paths, "base")
    er.rando = disc
    er.reset_rng()
    er.save_changed_enemies_and_randomize_their_params()
    # R7.4b: the rest of _save (but update_loaded_particles, R7.4c), on the same rng
    sites = add_sites(groups)
    before = {(arc, tag): len(dzx_of(disc, arc).entries_by_type_and_layer(ACTR, layer=layer))
              for arc, layer, tag in sites}
    er.add_modify_and_replace_actors_for_enemy_rando()
    er.randomize_enemies_spawned_by_objects()
    er._rng = None
  added = []
  for arc, layer, tag in sites:
    entries = dzx_of(disc, arc).entries_by_type_and_layer(ACTR, layer=layer)
    for i in range(before[(arc, tag)], len(entries)):
      added.append("added %s %s %d %s" % (arc, tag, i, raw_hex(entries[i], ACTR)))
  return base + lines_for(disc, paths, "enemy") + actor_lines(disc, targets) + added


def run_smoke(dump, iso, placement_path, stage, extra_env=()):
  env = dict(os.environ)
  env["COS_DISC"] = os.path.abspath(iso)
  cmd = [os.path.join(oc.REPO_ROOT, "native", "tools", "run.sh"), "rando", "--stage", stage,
         "--env", "COS_RANDO_SMOKE=enemies", "--env", "COS_ENEMY_DUMP=%s" % dump]
  if placement_path:
    cmd += ["--env", "COS_RANDO_PLACEMENT=%s" % os.path.abspath(placement_path)]
  for kv in extra_env:
    cmd += ["--env", kv]
  print("enemy_check: " + " ".join(cmd), file=sys.stderr)
  return subprocess.run(cmd, cwd=oc.REPO_ROOT, env=env).returncode


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--disc", default=os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso"))
  ap.add_argument("--placement", help="a placement JSON with \"enemies\" (our generator's)")
  ap.add_argument("--seed", help="generate the placement with our CLI for this seed")
  ap.add_argument("--cli", default=os.path.join(oc.REPO_ROOT, "build", "native-mac", "cos_rando_cli"))
  ap.add_argument("opts", nargs="*", help="--seed: extra options name=json for the CLI")
  ap.add_argument("--vanilla", action="store_true", help="no enemies: the disc's records, unchanged")
  ap.add_argument("--stage", default="M_NewD2:2:1", help="--run: the boot stage (default M_NewD2:2:1)")
  ap.add_argument("--smoke-env", action="append", default=[], metavar="K=V",
                  help="--run: an extra environment variable for the smoke (e.g. COS_RANDO_ENEMY_ALLDIE=1)")
  g = ap.add_mutually_exclusive_group()
  g.add_argument("--dump", help="the smoke's dump to compare")
  g.add_argument("--run", action="store_true", help="run the enemies smoke and compare its dump")
  args = ap.parse_args()
  if not os.path.isfile(args.disc):
    oc.die("disc not found: %s (--disc or COS_DISC)" % args.disc)
  placement = None
  gen_tmp = None
  if args.seed and not args.vanilla:
    if args.placement:
      oc.die("--seed and --placement exclude each other")
    gen_tmp = tempfile.TemporaryDirectory(dir=os.path.join(oc.REPO_ROOT, "build"))
    args.placement = os.path.join(gen_tmp.name, "placement.json")
    cmd = [args.cli, "randomize_enemies=true"] + args.opts + ["--json", args.placement, "--no-logs", "--quiet",
                                                              args.seed]
    print("enemy_check: " + " ".join(cmd), file=sys.stderr)
    if subprocess.run(cmd, cwd=oc.REPO_ROOT).returncode != 0:
      print("enemy_check: the generator failed", file=sys.stderr)
      return 1
  elif args.opts:
    oc.die("opt=json pairs need --seed")
  if not args.vanilla:
    if not args.placement:
      oc.die("--placement is required (or --vanilla)")
    placement = oc.read_json(args.placement)
    if "enemies" not in placement:
      oc.die("%s has no \"enemies\" key" % args.placement)

  want = expected(args.disc, placement, args.vanilla)
  if want is None:
    return 1
  if not args.dump and not args.run:
    print("\n".join(want))
    return 0

  tmp = None
  dump = args.dump
  if args.run:
    tmp = tempfile.TemporaryDirectory(dir=os.path.join(oc.REPO_ROOT, "build"))
    dump = os.path.join(tmp.name, "enemies.txt")
    code = run_smoke(dump, args.disc, args.placement, args.stage, args.smoke_env)
    if code != 0:
      print("enemy_check: the enemies smoke exited %d" % code, file=sys.stderr)
      return 1
  try:
    with open(dump) as f:
      got = [l.strip() for l in f if l.strip()]
  except OSError as e:
    oc.die("cannot read the dump: %s" % e)

  want_set, got_set = set(want), set(got)
  missing = [l for l in want if l not in got_set]
  unexpected = [l for l in got if l not in want_set]
  for l in missing[:40]:
    print("missing    " + l)
  for l in unexpected[:40]:
    print("unexpected " + l)
  ok = got == want
  n = sum(1 for l in want if l.startswith("enemy "))
  na = sum(1 for l in want if l.startswith("actor "))
  nd = sum(1 for l in want if l.startswith("added "))
  print("enemy_check: %d enemy records, %d edited and %d added actors (%s), %d missing, %d unexpected lines: "
        "%s wwrando's _save on %s"
        % (n, na, nd, "vanilla" if args.vanilla else placement.get("seed"), len(missing), len(unexpected),
           "equal to" if ok else "DIFFERENT from", os.path.basename(args.disc)))
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
