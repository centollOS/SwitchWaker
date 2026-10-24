#!/usr/bin/env python3
"""enemy_particle_check.py - checks step R7.4c (enemy rando particles) against wwrando's own code on the disc.

    python3 -I native/tools/rando/enemy_particle_check.py (--placement P.json | --seed NAME [--cli CLI] [opt=json ...]
                                                           | --vanilla) [--disc GZLE01.iso]
                                                          [--dump FILE | --run [--stage S:R:P] [--need]]
                                                          [--places]

--seed generates the placement first with our generator (CLI, default build/native-mac/cos_rando_cli,
randomize_enemies=true plus the opt=json pairs) into a temporary folder under build/.

For the seed and options of a placement JSON written by our generator (with randomize_enemies), this tool:
  1. runs wwrando 1.10.0_9775811 in dry mode for the same seed and options and checks the placement's
     "enemies" key equals wwrando's (as enemy_check.py; R7.3c);
  2. runs wwrando's own EnemyRandomizer.save_changed_enemies_and_randomize_their_params on the room files
     of the user's disc (gclib; after the two tweaks enemy_check.py applies first), which fills
     particles_to_load_for_each_jpc_index, then wwrando's own update_loaded_particles on the disc's
     Pscene files. Expected lines (the enemyptcl smoke's dump format):
         bank <NNN> <id> <id> ...      every bank with ids, in the dict's order
         need <NNN> <id> <source NNN>  each id the disc's bank lacks, with the first Pscene000-254 holding it
     and the ids update_loaded_particles really added to each bank must be exactly the "need" ids.
     The comparison is per bank (lines of one bank in order; the banks' order is not meaningful).
With --vanilla (no enemies) there are no lines.

--dump FILE  compares those lines with the enemyptcl smoke's dump (COS_ENEMY_PTCL_DUMP; exit 1 on any
             difference).
--run        runs the smoke first: native/tools/run.sh rando --stage <S:R:P> (default M_NewD2:2:1) with
             COS_RANDO_SMOKE=enemyptcl, COS_RANDO_PLACEMENT=<placement>, COS_ENEMY_PTCL_DUMP in a temporary
             folder and COS_CACHE_PER_RUN=1; --need adds COS_ENEMY_PTCL_NEED=1 (the boot place must need an
             import). Requires its exit 0, then compares its dump.
--places     prints, per place (stage folder or sea room), its bank and how many of its required ids the
             disc's bank lacks (to choose boot stages for --run --need).
Without --dump/--run, prints the lines.

The disc defaults to $COS_DISC, else ~/Documents/windwaker/GZLE01.iso. Nothing read from the disc is
written to the repository (plan R7 intro). Exit codes: 0 equal, 1 different or the smoke failed, 2 usage
or setup error. Uses the oracle venv (setup_oracle.sh) through oracle_common.bootstrap().
"""
import argparse
import contextlib
import json
import os
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib.jpc import JPC100  # noqa: E402
from wwlib.dzx import DZx, FILI, STAG  # noqa: E402
import tweaks  # noqa: E402
import enemy_check  # noqa: E402  (its DiscRando and TWEAKS_BEFORE_SAVE; not modified here)
import ww_export_enemies  # noqa: E402


class DiscRandoJpc(enemy_check.DiscRando):
  """enemy_check.DiscRando plus wwrando's get_jpc on the user's disc."""

  def __init__(self, iso, rando):
    super().__init__(iso, rando)
    self.__dict__["jpcs_by_path"] = {}

  def get_jpc(self, jpc_path):
    jpc_path = jpc_path.replace("\\", "/")
    if jpc_path not in self.jpcs_by_path:
      self.jpcs_by_path[jpc_path] = JPC100(self.gcm.read_file_data(jpc_path))
    return self.jpcs_by_path[jpc_path]


def vanilla_jpc(disc, index, cache):
  if index not in cache:
    path = "files/res/Particle/Pscene%03d.jpc" % index
    cache[index] = JPC100(disc.gcm.read_file_data(path)) if path.lower() in disc.gcm.files_by_path_lowercase else None
  return cache[index]


def expected(iso, placement):
  """(lines, places, problems): the smoke's dump lines, per-place facts, and wwrando-side problems."""
  values = dict(placement.get("options", {}))
  values["randomize_enemies"] = True
  opts = oc.make_options(values)
  r = oc.make_randomizer(placement["seed"], opts)
  oc.randomize(r)
  want = ww_export_enemies.enemies_from_randomizer(r)
  if json.dumps(want, sort_keys=True) != json.dumps(placement.get("enemies"), sort_keys=True):
    return None, None, ["the placement's \"enemies\" differ from wwrando's for this seed (R7.3c)"]

  disc = DiscRandoJpc(iso, r)
  er = r.enemies
  with contextlib.redirect_stdout(sys.stderr):
    for tweak in enemy_check.TWEAKS_BEFORE_SAVE:
      getattr(tweaks, tweak)(disc)
    er.rando = disc
    er.particles_to_load_for_each_jpc_index = {}
    er.reset_rng()
    er.save_changed_enemies_and_randomize_their_params()
    er._rng = None
  table = {k: list(v) for k, v in er.particles_to_load_for_each_jpc_index.items() if v}

  # What update_loaded_particles copies, on the vanilla files (the first holder of an id is the same
  # file whether or not an earlier bank got it: wwrando caches the particle by id).
  cache = {}
  lines, problems = [], []
  need = {}
  for bank, ids in table.items():
    lines.append("bank %03d %s" % (bank, " ".join("%04X" % i for i in ids)))
    dest = vanilla_jpc(disc, bank, cache)
    if dest is None:
      problems.append("Pscene%03d.jpc is not on the disc" % bank)
      continue
    for pid in ids:
      if pid in dest.particles_by_id:
        continue
      src = next((i for i in range(255) if vanilla_jpc(disc, i, cache) is not None
                  and pid in vanilla_jpc(disc, i, cache).particles_by_id), -1)
      lines.append("need %03d %04X %03d" % (bank, pid, src))
      need.setdefault(bank, []).append(pid)

  # wwrando's own update_loaded_particles on fresh copies: the ids it adds must be the "need" ids.
  before = {b: set(disc.get_jpc("files/res/Particle/Pscene%03d.jpc" % b).particles_by_id) for b in table}
  with contextlib.redirect_stdout(sys.stderr):
    er.update_loaded_particles(er.particles_to_load_for_each_jpc_index)
  for b in table:
    added = set(disc.get_jpc("files/res/Particle/Pscene%03d.jpc" % b).particles_by_id) - before[b]
    if added != set(need.get(b, [])):
      problems.append("Pscene%03d: update_loaded_particles added %s, expected %s"
                      % (b, sorted("%04X" % i for i in added), sorted("%04X" % i for i in need.get(b, []))))

  # Per place: its bank and lacking ids (done_enemy_locations holds (location, new enemy type) pairs).
  places = []
  by_place = {}
  for loc, new in er.done_enemy_locations:
    parts = loc["Path"].split("/")
    key = parts[0] + ("/" + parts[1] if parts[0] == "sea" else "")
    ids = by_place.setdefault(key, [])
    for pid in new["Required particle IDs"]:
      if pid not in ids:
        ids.append(pid)
  for key, ids in by_place.items():
    if not ids:
      continue
    bank = stage_bank(disc, key)
    dest = vanilla_jpc(disc, bank, cache)
    lacking = [i for i in ids if dest is not None and i not in dest.particles_by_id]
    places.append((key, bank, len(ids), len(lacking)))
  return lines, places, problems


def stage_bank(disc, key):
  if key.startswith("sea/"):
    dzr = disc.get_arc("files/res/Stage/" + key).get_file("room.dzr", DZx)
    return dzr.entries_by_type(FILI)[0].loaded_particle_bank
  dzs = disc.get_arc("files/res/Stage/%s/Stage.arc" % key).get_file("stage.dzs", DZx)
  return dzs.entries_by_type(STAG)[0].loaded_particle_bank


def blocks(lines):
  """{bank: [its lines in order]}."""
  out = {}
  for l in lines:
    out.setdefault(l.split()[1], []).append(l)
  return out


def run_smoke(dump, iso, placement_path, stage, need):
  env = dict(os.environ)
  env["COS_DISC"] = os.path.abspath(iso)
  cmd = [os.path.join(oc.REPO_ROOT, "native", "tools", "run.sh"), "rando", "--stage", stage,
         "--env", "COS_RANDO_SMOKE=enemyptcl", "--env", "COS_ENEMY_PTCL_DUMP=%s" % dump,
         "--env", "COS_CACHE_PER_RUN=1"]
  if need:
    cmd += ["--env", "COS_ENEMY_PTCL_NEED=1"]
  if placement_path:
    cmd += ["--env", "COS_RANDO_PLACEMENT=%s" % os.path.abspath(placement_path)]
  print("enemy_particle_check: " + " ".join(cmd), file=sys.stderr)
  return subprocess.run(cmd, cwd=oc.REPO_ROOT, env=env).returncode


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--disc", default=os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso"))
  ap.add_argument("--placement", help="a placement JSON with \"enemies\" (our generator's)")
  ap.add_argument("--seed", help="generate the placement with our CLI for this seed")
  ap.add_argument("--cli", default=os.path.join(oc.REPO_ROOT, "build", "native-mac", "cos_rando_cli"))
  ap.add_argument("opts", nargs="*", help="--seed: extra options name=json for the CLI")
  ap.add_argument("--vanilla", action="store_true", help="no enemies: no lines")
  ap.add_argument("--stage", default="M_NewD2:2:1", help="--run: the boot stage (default M_NewD2:2:1)")
  ap.add_argument("--need", action="store_true", help="--run: the boot place must need an import")
  ap.add_argument("--places", action="store_true", help="print each place's bank and lacking ids")
  g = ap.add_mutually_exclusive_group()
  g.add_argument("--dump", help="the smoke's dump to compare")
  g.add_argument("--run", action="store_true", help="run the enemyptcl smoke and compare its dump")
  args = ap.parse_args()
  if not os.path.isfile(args.disc):
    oc.die("disc not found: %s (--disc or COS_DISC)" % args.disc)
  gen_tmp = None
  if args.seed and not args.vanilla:
    if args.placement:
      oc.die("--seed and --placement exclude each other")
    gen_tmp = tempfile.TemporaryDirectory(dir=os.path.join(oc.REPO_ROOT, "build"))
    args.placement = os.path.join(gen_tmp.name, "placement.json")
    cmd = [args.cli, "randomize_enemies=true"] + args.opts + ["--json", args.placement, "--no-logs", "--quiet",
                                                              args.seed]
    print("enemy_particle_check: " + " ".join(cmd), file=sys.stderr)
    if subprocess.run(cmd, cwd=oc.REPO_ROOT).returncode != 0:
      print("enemy_particle_check: the generator failed", file=sys.stderr)
      return 1
  elif args.opts:
    oc.die("opt=json pairs need --seed")

  if args.vanilla:
    want, places, problems = [], [], []
  else:
    if not args.placement:
      oc.die("--placement is required (or --vanilla)")
    placement = oc.read_json(args.placement)
    if "enemies" not in placement:
      oc.die("%s has no \"enemies\" key" % args.placement)
    want, places, problems = expected(args.disc, placement)
  for p in problems:
    print("enemy_particle_check: wwrando: " + p)
  if want is None:
    return 1
  if args.places:
    for key, bank, n, lacking in places:
      print("place %-16s Pscene%03d %2d required, %2d lacking" % (key, bank, n, lacking))
  if not args.dump and not args.run:
    if not args.places:
      print("\n".join(want))
    return 0 if not problems else 1

  tmp = None
  dump = args.dump
  if args.run:
    tmp = tempfile.TemporaryDirectory(dir=os.path.join(oc.REPO_ROOT, "build"))
    dump = os.path.join(tmp.name, "enemyptcl.txt")
    code = run_smoke(dump, args.disc, None if args.vanilla else args.placement, args.stage, args.need)
    if code != 0:
      print("enemy_particle_check: the enemyptcl smoke exited %d" % code, file=sys.stderr)
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
  # The banks' order is not meaningful (the registry keeps imports per bank, and wwrando's dict gets a
  # bank's key at its first location even when that location's type needs no particle); within a bank,
  # the ids' order is (it is the order the particles are appended to the file).
  ok = blocks(got) == blocks(want) and not problems
  banks = sum(1 for l in want if l.startswith("bank "))
  needs = sum(1 for l in want if l.startswith("need "))
  print("enemy_particle_check: %d banks, %d imports (%s), %d missing, %d unexpected lines: %s wwrando's "
        "update_loaded_particles on %s"
        % (banks, needs, "vanilla" if args.vanilla else os.path.basename(args.placement), len(missing),
           len(unexpected), "equal to" if ok else "DIFFERENT from", os.path.basename(args.disc)))
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
