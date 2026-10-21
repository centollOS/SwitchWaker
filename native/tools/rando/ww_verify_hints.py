#!/usr/bin/env python3
"""Check the hints of our generator's placements (docs/RANDOMIZER_PLAN.md step R5.1).

    python3 -I native/tools/rando/ww_verify_hints.py <placement.json> ... [--golden] [--jobs J]
    python3 -I native/tools/rando/ww_verify_hints.py --cli build/native-mac/cos_rando_cli --n N
                                                     [--prefix S] [--start K] [--golden] [--jobs J]
                                                     [--keep DIR] [name=<json> ...]

The placements are our generator's (pc_rando_gen_hints.h: the "hints" value, format 1). With --cli
the tool generates N seeds (<prefix><K>, ...) itself at wwrando's default options plus the
name=<json> pairs, as check_seeds.py does.

Validity (decision D1), with wwrando's own Logic and HintsRandomizer helpers on a dry WWRandomizer
rebuilt from the placement (ww_verify.restore_state, every location filled, the dungeon small keys
put back as prerandomized items as the item fill leaves them):
  - every item hint's item sits at a progress location whose entrance zone is the hinted zone;
  - every location and Savage Labyrinth hint names the item placed at that location;
  - a hint marked "required" points at a location required for a path goal (removing its item breaks
    it: check_location_required_for_paths), one marked "not required" at a location that is not
    required for any path goal;
  - barren zones hold no required item: no non-banned progress location of the zone
    (get_all_zones_for_item_location) is required for any path goal;
  - every path hint names a zone holding a location required for the hinted goal (dungeon locations
    by dungeon, the others by entrance zone), and not a dungeon key without key-lunacy;
  - the placements are the enabled ones in wwrando's order; the fishmen (49), Ho Ho (10, the same
    number each) and KoRL lists are drawn from their placement's hints; the Big Octo Great Fairy
    hint is an item hint that is not about her own location.

--golden also runs wwrando 1.10.0_9775811 (dry) on the placement's seed and options and requires
the hints value to equal what its HintsRandomizer made (same hints, order, distribution and Ho Ho
look islands): our hints are byte-identical, not only valid.

Prints one line per failing placement and a summary. Exit 0 all passed, 1 a check failed, 2 usage or
setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import contextlib  # noqa: E402
import json  # noqa: E402
import multiprocessing  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402
import tempfile  # noqa: E402

SWITCHWAKER_SEED_KEY = "SwitchWaker-rando-key-1"  # kSwitchWakerSeedKey (decision D2)
HINTS_FORMAT = 1
PLACEMENTS = ("fishmen_hints", "hoho_hints", "korl_hints")
GOAL_FOR_REWARD = {
  "Gohma": "Dragon Roost Cavern", "Kalle Demos": "Forbidden Woods", "Gohdan": "Tower of the Gods",
  "Helmaroc King": "Forsaken Fortress", "Jalhalla": "Earth Temple", "Molgera": "Wind Temple",
  "Hyrule": "Hyrule", "Ganondorf": "Ganon's Tower",
}


# ---------------------------------------------------------------------------------------------
# wwrando's HintsRandomizer state as our JSON (pc_rando_gen_hints.h hintsToJson)

def hint_to_json(h):
  from randomizers.hints import HintType
  imp = h.formatted_importance() if h.importance is not None else None
  return {"type": "fixed_location" if h.type == HintType.FIXED_LOCATION else h.type.name.lower(),
          "place": h.place, "reward": h.reward, "importance": imp}


def hoho_look_island(r, hints):
  # rotate_hoho_to_face_hint's choice of island (the rotation itself is R5.3's).
  from randomizers.hints import HintType
  zone_name = None
  for hint in hints:
    if hint.type in (HintType.PATH, HintType.BARREN, HintType.ITEM):
      zone_name = hint.place
    elif hint.type == HintType.LOCATION:
      zone_name = r.entrances.get_entrance_zone_for_item_location(hint.place)
    if zone_name == "Ganon's Tower":
      zone_name = "Tower of the Gods Sector"
    if zone_name in r.island_name_to_number:
      return r.island_name_to_number[zone_name]
  return None


def hints_to_json(r):
  hr = r.hints
  out = {
    "format": HINTS_FORMAT,
    "savage_labyrinth": [hint_to_json(hr.floor_30_hint), hint_to_json(hr.floor_50_hint)],
    "octo_fairy": hint_to_json(hr.octo_fairy_hint) if hr.octo_fairy_hint is not None else None,
    "placements": {k: [hint_to_json(h) for h in v] for k, v in hr.hints_per_placement.items()},
  }
  if "fishmen_hints" in hr.hints_per_placement:
    out["fishmen"] = [hint_to_json(hr.island_to_fishman_hint[i]) for i in range(1, 50)]
  if "hoho_hints" in hr.hints_per_placement:
    out["hoho"] = [{"hints": [hint_to_json(h) for h in hr.hoho_index_to_hints[i]],
                    "look_island": hoho_look_island(r, hr.hoho_index_to_hints[i])} for i in range(10)]
  if "korl_hints" in hr.hints_per_placement:
    out["korl"] = [hint_to_json(h) for h in hr.hints_per_placement["korl_hints"]]
  return out


# ---------------------------------------------------------------------------------------------
# Validity

def verify_hints(d):
  """Problems (a list of strings) of the placement's hints."""
  import ww_verify
  from logic.logic import Logic
  from randomizers.hints import HintsRandomizer

  h = d.get("hints")
  if not isinstance(h, dict):
    return ["the placement has no hints"]
  if h.get("format") != HINTS_FORMAT:
    return [f"hints format {h.get('format')!r} is not {HINTS_FORMAT}"]

  opts = oc.make_options(d["options"], strict=False)
  r = oc.make_randomizer(d.get("seed") or "x", opts)
  with contextlib.redirect_stdout(sys.stderr):
    ww_verify.restore_state(r, d, opts)
  L = r.logic
  locs = d["locations"]
  for loc, item in locs.items():
    L.done_item_locations[loc] = item
  if not opts.keylunacy:
    for loc, item in locs.items():
      if item.endswith(" Small Key"):
        L.prerandomization_item_locations[loc] = item
  with contextlib.redirect_stdout(sys.stderr):
    hr = HintsRandomizer(r)
    hr._path_logic = Logic(r)
  hr.path_logic_initial_state = hr.path_logic.save_simulated_playthrough_state()

  banned = set(r.boss_reqs.banned_locations)
  progress, _ = L.get_progress_and_non_progress_locations()
  progress = [loc for loc in progress if loc not in banned]
  goals = list(r.boss_reqs.required_dungeons) + ["Hyrule", "Ganon's Tower"]
  ent = r.entrances
  cache = {}

  def required_for(loc):
    if loc not in cache:
      cache[loc] = {g for g, met in hr.check_location_required_for_paths(loc, goals).items() if met}
    return cache[loc]

  problems = []

  def check_importance(what, cands, imp):
    if imp == "required" and not any(required_for(c) for c in cands):
      problems.append(f"{what}: marked required but no such location is required")
    if imp == "not required" and all(required_for(c) for c in cands):
      problems.append(f"{what}: marked not required but its location is required")

  def check_hint(x, where):
    if not isinstance(x, dict) or not isinstance(x.get("place"), str):
      problems.append(f"{where}: malformed hint {x!r}")
      return
    t, place, reward, imp = x.get("type"), x["place"], x.get("reward"), x.get("importance")
    what = f"{where} {t} ({place}, {reward})"
    if t == "item":
      cands = [loc for loc in progress if locs[loc] == reward and ent.get_entrance_zone_for_item_location(loc) == place]
      if not cands:
        problems.append(f"{what}: no progress location in that zone holds the item")
      else:
        check_importance(what, cands, imp)
    elif t in ("location", "fixed_location"):
      if locs.get(place) != reward:
        problems.append(f"{what}: the location holds {locs.get(place)!r}")
      elif imp is not None:
        check_importance(what, [place], imp)
    elif t == "barren":
      if reward is not None:
        problems.append(f"{what}: a barren hint has a reward")
      for loc in progress:
        if place in ent.get_all_zones_for_item_location(loc) and required_for(loc):
          problems.append(f"{what}: {loc} ({locs[loc]}) is required for {sorted(required_for(loc))}")
    elif t == "path":
      goal = GOAL_FOR_REWARD.get(reward)
      if goal is None or goal not in goals:
        problems.append(f"{what}: unknown path goal")
        return
      cands = []
      for loc in progress:
        zone = L.split_location_name_by_zone(loc)[0] if L.is_dungeon_location(loc) \
          else ent.get_entrance_zone_for_item_location(loc)
        if zone != place or goal not in required_for(loc):
          continue
        if not opts.keylunacy and locs[loc].endswith(" Key"):
          continue
        cands.append(loc)
      if not cands:
        problems.append(f"{what}: no location of the zone is required for {goal}")
    else:
      problems.append(f"{where}: unknown hint type {t!r}")

  savage = h.get("savage_labyrinth")
  if not isinstance(savage, list) or len(savage) != 2:
    problems.append("savage_labyrinth must hold 2 hints")
  else:
    for x, loc in zip(savage, ("Outset Island - Savage Labyrinth - Floor 30",
                               "Outset Island - Savage Labyrinth - Floor 50")):
      if x.get("type") != "fixed_location" or x.get("place") != loc:
        problems.append(f"savage_labyrinth: wrong hint {x!r}")
      else:
        check_hint(x, "savage")

  octo = h.get("octo_fairy")
  if octo is not None:
    if octo.get("type") != "item":
      problems.append("octo_fairy is not an item hint")
    check_hint(octo, "octo_fairy")
    own = "Two-Eye Reef - Big Octo Great Fairy"
    if octo.get("reward") == locs.get(own):
      others = [loc for loc in progress if loc != own and locs[loc] == octo.get("reward")
                and ent.get_entrance_zone_for_item_location(loc) == octo.get("place")]
      if not others:
        problems.append("octo_fairy hints at her own location")

  pl = h.get("placements", {})
  enabled = [p for p in PLACEMENTS if opts[p]]
  if octo is not None and list(pl) and list(pl) != enabled:
    problems.append(f"placements {list(pl)} are not the enabled {enabled}")
  for p, lst in pl.items():
    for i, x in enumerate(lst):
      check_hint(x, f"{p}[{i}]")
  if "fishmen_hints" in pl:
    fish = h.get("fishmen")
    if not isinstance(fish, list) or len(fish) != 49 or any(x not in pl["fishmen_hints"] for x in fish):
      problems.append("fishmen: not 49 hints from fishmen_hints")
  if "hoho_hints" in pl:
    hoho = h.get("hoho")
    if not isinstance(hoho, list) or len(hoho) != 10:
      problems.append("hoho: not 10 entries")
    else:
      sizes = {len(e.get("hints", [])) for e in hoho}
      if len(sizes) != 1 or any(x not in pl["hoho_hints"] for e in hoho for x in e.get("hints", [])):
        problems.append("hoho: uneven lists or hints not from hoho_hints")
  if "korl_hints" in pl and h.get("korl") != pl["korl_hints"]:
    problems.append("korl: not the korl_hints list")
  return problems


# ---------------------------------------------------------------------------------------------
# Worker side

def worker_init():
  oc.bootstrap()
  oc.set_seed_key(SWITCHWAKER_SEED_KEY)


def check_one(job):
  name, path, golden = job
  d = oc.read_json(path)
  try:
    problems = verify_hints(d)
  except Exception as e:  # a wwrando helper raised on our data
    problems = [f"verify raised {type(e).__name__}: {e}"]
  if golden and isinstance(d.get("hints"), dict):
    opts = oc.make_options(d["options"], strict=False)
    try:
      r = oc.make_randomizer(d["seed"], opts)
      oc.randomize(r)
      want = hints_to_json(r)
    except Exception as e:
      problems.append(f"wwrando raised {type(e).__name__}: {e}")
    else:
      if d["hints"] != want:
        for k in sorted(set(want) | set(d["hints"])):
          if d["hints"].get(k) != want.get(k):
            problems.append(f"golden: hints key {k!r} differs from wwrando's")
  return name, problems


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("args", nargs="*", help="placement JSON files, or name=<json> pairs with --cli")
  ap.add_argument("--cli")
  ap.add_argument("--n", type=int, default=20)
  ap.add_argument("--prefix", default="HintSeed")
  ap.add_argument("--start", type=int, default=0)
  ap.add_argument("--golden", action="store_true")
  ap.add_argument("--jobs", type=int, default=4)
  ap.add_argument("--keep", metavar="DIR")
  a = ap.parse_args()
  oc.bootstrap()

  jobs = []
  out_dir = None
  if a.cli:
    if not os.access(a.cli, os.X_OK):
      oc.die(f"no CLI at {a.cli} (ninja -C build/native-mac cos_rando_cli)")
    values = oc.parse_opt_pairs(a.args)
    oc.make_options(values)
    out_dir = a.keep or tempfile.mkdtemp(prefix="ww_verify_hints_")
    os.makedirs(out_dir, exist_ok=True)
    seeds = [f"{a.prefix}{i}" for i in range(a.start, a.start + a.n)]
    cmd = [a.cli, "--out", out_dir, "--no-logs", "--quiet"]
    cmd += [f"{k}={json.dumps(v)}" for k, v in values.items()] + seeds
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode not in (0, 1, 3):
      sys.stderr.write(p.stderr)
      oc.die(f"cos_rando_cli exited {p.returncode}")
    for s in seeds:
      path = os.path.join(out_dir, s + ".json")
      jobs.append((s, path, a.golden))
  else:
    if not a.args:
      ap.error("give placement files or --cli")
    jobs = [(os.path.basename(f), f, a.golden) for f in a.args]

  failures = 0
  missing = [j for j in jobs if not os.path.isfile(j[1])]
  for j in missing:
    print(f"FAIL {j[0]}: no placement ({j[1]})")
  failures += len(missing)
  jobs = [j for j in jobs if os.path.isfile(j[1])]
  ctx = multiprocessing.get_context("spawn")
  with ctx.Pool(processes=max(1, a.jobs), initializer=worker_init) as pool:
    for name, problems in pool.imap_unordered(check_one, jobs):
      for pr in problems[:8]:
        print(f"FAIL {name}: {pr}")
      failures += bool(problems)
  total = len(jobs) + len(missing)
  print(f"ww_verify_hints: {total} placements, {total - failures} passed"
        f"{' (golden)' if a.golden else ''}, {failures} failed")
  if out_dir and not a.keep:
    shutil.rmtree(out_dir, ignore_errors=True)
  return 0 if failures == 0 else 1


if __name__ == "__main__":
  sys.exit(main())
