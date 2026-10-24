#!/usr/bin/env python3
"""Golden placement with the enemy randomizer's decisions (docs/RANDOMIZER_PLAN.md step R7.3c).

    python3 -I native/tools/rando/ww_export_enemies.py <seed> <out.json> [name=<json> ...]
    python3 -I native/tools/rando/ww_export_enemies.py --vectors [--check] [out.txt]

Runs wwrando 1.10.0_9775811 in dry mode (as ww_export.py; randomize_enemies is forced on) and writes
ww_export.py's placement plus the top-level key "enemies":

    "enemies": {
      "locations": {"<path>": {"type", "name", "params", "x_rot", "z_rot", "category", "flags",
                               ["pos"], ["pos_offset"], ["y_rot"], ["y_rot_offset"]}, ...},
      "homen": [kaze/Room2 ACTR 0x27, kaze/Room10 ACTR 0, kaze/Room10 ACTR 9]
    }

A dry run never calls EnemyRandomizer._save, whose RNG part decides the params: _save calls
reset_rng() and then randomize_enemy_params for every done location in order, then
randomize_enemies_spawned_by_objects draws the 3 Homen enemies. Those draws depend only on the new
type and the placement category, so this tool runs wwrando's own randomize_enemy_params on stub ACTR
entries (wwlib.dzx.ACTR with no file data: name, params and rotations set from the new type, the
bitfields through wwrando's actor_parameters.txt as in _save) and repeats the Homen draws. The values
are the ones _save writes before it copies the vanilla spawn switch, death switch and path index
(those need the disc; R7.4a does them, checked by enemy_check.py on the disc). "flags" (bits, as
pc_rando_gen_enemies.h EFLAG_*): 1 ChecksConditionSwitch, 2 SetsDeathSwitch, 4 a Ceiling location
for a type without Ceiling (adjust_enemy y -500), 8 magtail (y +50), 16 a ChuChu in a Pot location
(snapped to the nearest pot). The overrides are the location's Position / Position offset /
Y Rotation / Y Rotation offset.

--vectors writes the fixture of cos_rando_test_R7_3c (default
native/check/rando/fixtures/R7_3c_enemies.txt); deterministic, so regenerating gives no diff
(--check compares with the file instead of writing it: exit 1 when it differs):

    version   <VERSION>
    case      <name> <seed>
    opt       <name> <value>        options that differ from Options() (R1.2 value format)
    enemy     <path> <type> <params hex8> <x_rot hex4> <z_rot hex4> <flags>
    homen     <a> <b> <c>
    locations <crc32 of "loc=item|..." of the item placement, hex>
    error     <first line of the exception>   (instead of enemy/homen/locations)
    endcase
    end

Exit 0 written (or up to date), 1 --check found a difference, 2 usage or setup error; a wwrando failure prints its traceback and exits 1.
"""
import os
import sys
import zlib
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import contextlib  # noqa: E402
import traceback  # noqa: E402

CHUCHUS = ("c_green", "c_red", "c_kiiro", "c_blue", "c_black")

# randomize_enemies_spawned_by_objects's possible_homen_enemies (randomizers/enemies.py).
POSSIBLE_HOMEN_ENEMIES = [0, 1, 2, 3, 4, 5, 6, 7, 10]

FLAG_SPAWN_SWITCH = 1
FLAG_DEATH_SWITCH = 2
FLAG_CEILING_DROP = 4
FLAG_MAGTAIL_RAISE = 8
FLAG_POT_SNAP = 16


def placement_flags(category, enemy_data):
  conds = category.split(":", 1)[1].split(",") if ":" in category else []
  f = 0
  if "ChecksConditionSwitch" in conds:
    f |= FLAG_SPAWN_SWITCH
  if "SetsDeathSwitch" in conds:
    f |= FLAG_DEATH_SWITCH
  # adjust_enemy
  if category == "Ceiling" and "Ceiling" not in enemy_data["Placement categories"]:
    f |= FLAG_CEILING_DROP
  if enemy_data["Actor name"] == "magtail":
    f |= FLAG_MAGTAIL_RAISE
  elif enemy_data["Actor name"] in CHUCHUS and category == "Pot":
    f |= FLAG_POT_SNAP
  return f


def enemies_from_randomizer(r):
  """The "enemies" value of a randomizer that ran (dry) with randomize_enemies: the RNG part of
  EnemyRandomizer._save replayed on stub actors."""
  from wwlib.dzx import ACTR
  er = r.enemies
  er.reset_rng()  # _save's first call
  locations = {}
  for enemy_location, new_enemy_data in er.done_enemy_locations:
    path = enemy_location["Path"]
    category = enemy_location["Placement category"]
    enemy = ACTR(None)
    enemy.name = new_enemy_data["Actor name"]
    enemy.params = new_enemy_data["Params"]
    enemy.x_rot = new_enemy_data["X Rotation"]
    enemy.z_rot = new_enemy_data["Z Rotation"]
    er.randomize_enemy_params(new_enemy_data, enemy, category, None, None)
    e = {
      "type": new_enemy_data["Pretty name"],
      "name": enemy.name,
      "params": enemy.params & 0xFFFFFFFF,
      "x_rot": enemy.x_rot & 0xFFFF,
      "z_rot": enemy.z_rot & 0xFFFF,
      "category": category,
      "flags": placement_flags(category, new_enemy_data),
    }
    if "Position" in enemy_location:
      e["pos"] = list(enemy_location["Position"])
    if "Position offset" in enemy_location:
      e["pos_offset"] = list(enemy_location["Position offset"])
    if "Y Rotation" in enemy_location:
      e["y_rot"] = enemy_location["Y Rotation"]
    if "Y Rotation offset" in enemy_location:
      e["y_rot_offset"] = enemy_location["Y Rotation offset"]
    if path in locations:
      raise Exception(f"Enemy location path listed twice: {path}")
    locations[path] = e
  # add_modify_and_replace_actors_for_enemy_rando draws nothing; then the 3 Homen.
  homen = [er.rng.choice(POSSIBLE_HOMEN_ENEMIES) for _ in range(3)]
  er._rng = None
  return {"locations": locations, "homen": homen}


def placement_with_enemies(r, opts):
  d = oc.placement_from_randomizer(r, opts)
  d["enemies"] = enemies_from_randomizer(r)
  return d


# ---------------------------------------------------------------------------------------------
# --vectors

def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  if isinstance(v, list):
    return "|".join(str(x) for x in v)
  return str(v)


def vector_cases():
  import dataclasses
  from options.wwrando_options import Options
  all_cats = {f.name: True for f in dataclasses.fields(Options) if f.name.startswith("progression_")}
  return [
    ("default", "EnemyVec0", {}),
    ("default", "EnemyVec1", {}),
    ("all_categories", "EnemyVec2", all_cats),
    ("swordless", "EnemyVec3", {"sword_mode": "Swordless"}),
    ("extra_start", "EnemyVec4", {"num_extra_starting_items": 2, "starting_gear": ["Progressive Bow", "Bombs"]}),
  ]


def write_vectors(path, check=False):
  import version
  from options.wwrando_options import Options
  defaults = Options()
  out = [f"version\t{version.VERSION}"]
  for name, seed, values in vector_cases():
    values = dict(values, randomize_enemies=True)
    opts = oc.make_options(values)
    out.append(f"case\t{name}\t{seed}")
    for k in values:
      if getattr(opts, k) != getattr(defaults, k):
        out.append(f"opt\t{k}\t{opt_value(oc._plain(getattr(opts, k)))}")
    try:
      r = oc.make_randomizer(seed, opts)
      oc.randomize(r)
    except Exception as e:
      out.append(f"error\t{str(e).splitlines()[0]}")
      out.append("endcase")
      continue
    en = enemies_from_randomizer(r)
    for p, e in en["locations"].items():
      out.append(f"enemy\t{p}\t{e['type']}\t{e['params']:08x}\t{e['x_rot']:04x}\t{e['z_rot']:04x}\t{e['flags']}")
    out.append("homen\t" + "\t".join(str(h) for h in en["homen"]))
    locs = "|".join(f"{k}={v}" for k, v in r.logic.done_item_locations.items())
    out.append(f"locations\t{zlib.crc32(locs.encode()):08x}")
    out.append("endcase")
  out.append("end")
  text = "\n".join(out) + "\n"
  if check:
    try:
      with open(path, newline="") as f:
        same = f.read() == text
    except OSError:
      same = False
    print(f"{os.path.basename(path)}: " + ("up to date" if same else "differs from wwrando"), file=sys.stderr)
    return same
  with open(path, "w", newline="\n") as f:
    f.write(text)
  return True


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("args", nargs="*", help="<seed> <out.json> [name=json ...], or [out.txt] with --vectors")
  ap.add_argument("--vectors", action="store_true")
  ap.add_argument("--check", action="store_true", help="with --vectors: compare instead of writing")
  ap.add_argument("--options", metavar="FILE")
  ap.add_argument("--seed-key", metavar="KEY")
  a = ap.parse_args()

  oc.bootstrap()
  if a.seed_key is not None:
    oc.set_seed_key(a.seed_key)
  if a.vectors:
    if len(a.args) > 1:
      ap.error("--vectors takes at most [out.txt]")
    path = a.args[0] if a.args else os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures",
                                                 "R7_3c_enemies.txt")
    with contextlib.redirect_stdout(sys.stderr):
      ok = write_vectors(path, check=a.check)
    return 0 if ok else 1
  if len(a.args) < 2:
    ap.error("need <seed> <out.json>")
  seed, out_path, pairs = a.args[0], a.args[1], a.args[2:]
  values = oc.load_options_file(a.options) if a.options else {}
  values.update(oc.parse_opt_pairs(pairs))
  values["randomize_enemies"] = True
  opts = oc.make_options(values)
  r = oc.make_randomizer(seed, opts)
  if not r.seed:
    oc.die(f"seed '{seed}' is empty after wwrando's sanitizing")
  try:
    oc.randomize(r)
    d = placement_with_enemies(r, opts)
  except Exception:
    traceback.print_exc()
    return 1
  oc.write_json(d, out_path)
  return 0


if __name__ == "__main__":
  sys.exit(main())
