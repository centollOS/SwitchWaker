#!/usr/bin/env python3
"""Check a placement JSON with wwrando's own Logic (the oracle's checker side).

    python3 -I native/tools/rando/ww_verify.py <placement.json> [--validity-only] [--quiet]

The JSON is schema format 1 (ww_export.py, or our generator). The tool builds a dry WWRandomizer
with the JSON's options, restores the decisions the item fill depends on (charts, required bosses,
entrances, extra starting items) and checks:

  1. the placement fills exactly wwrando's 320 locations, with known item names;
  2. the pool multiset is placed: every progress and nonprogress item exactly as often as the pool
     holds it; fixed consumables at most as often; other items only if duplicatable consumables;
  3. check_item_valid_in_location holds for every pair (with the whole placement in place, so the
     rock spire shop's no-duplicates rule is checked too), and no progress copy sits in a location
     banned by required bosses (dungeon items excepted; count-based per item, so a nonprogress
     copy of a split item, e.g. the wallet left after one is an extra starting item, may);
  4. at least N copies of each progress item sit in progress-eligible, non-banned locations
     (count-based: duplicates such as wallets can be split between progress and nonprogress);
  5. reachability: owning everything reachable until a fixpoint reaches
     'Can Reach and Defeat Ganondorf' and every non-banned progress location (sunken treasures
     only when their chart is a progress item, as wwrando guarantees nothing more).

--validity-only runs checks 1 and 3 only (for test placements such as make_test_placement.py's
rotation, which need not be beatable). Entrance structure rules (nesting, safety entrance) are not
checked here (step R6.14).

Diagnostics are printed as JSON on stdout (--quiet: only when invalid). Exit 0 valid, 1 invalid,
2 usage or setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import collections  # noqa: E402
import contextlib  # noqa: E402
import json  # noqa: E402
import re  # noqa: E402

REQUIRED_KEYS = ("format", "options", "locations")


def restore_state(r, d, opts):
  """Put the non-item random decisions of the placement back into the dry randomizer."""
  if d.get("charts"):
    r.charts.island_number_to_chart_name = {int(k): v for k, v in d["charts"].items()}
  if opts.required_bosses:
    br = r.boss_reqs
    stored = list(d.get("required_boss_locations", []))

    # Re-derive the banned lists exactly like randomize_required_bosses, with a stub RNG whose
    # sample() returns the stored choice.
    class FixedRng:
      def sample(self, seq, k):
        return list(stored)

    br._rng = FixedRng()
    br.required_dungeons.clear()
    br.banned_dungeons.clear()
    br.required_bosses.clear()
    br.banned_bosses.clear()
    br.banned_locations.clear()
    br.randomize_required_bosses()
    br._rng = None
    br.required_boss_item_locations = stored
  if d.get("entrances"):
    r.entrances.entrance_connections.update(d["entrances"])
  r.extra_start_items.random_starting_items = list(d.get("extra_starting_items", []))
  r.logic.initialize_from_randomizer_state()
  if opts.required_bosses:
    r.logic.update_required_bosses_macro()


def verify(d, validity_only=False):
  errors = []
  for k in REQUIRED_KEYS:
    if k not in d:
      errors.append(f"missing key '{k}'")
  if errors:
    return {"errors": errors}
  if d["format"] != oc.PLACEMENT_FORMAT:
    return {"errors": [f"format {d['format']} is not {oc.PLACEMENT_FORMAT}"]}

  opts = oc.make_options(d["options"], strict=False)
  r = oc.make_randomizer(d.get("seed") or "x", opts)
  with contextlib.redirect_stdout(sys.stderr):
    restore_state(r, d, opts)
  L = r.logic
  locs = d["locations"]
  banned = set(r.boss_reqs.banned_locations)

  # 1. Location set and item names.
  missing = [l for l in L.item_locations if l not in locs]
  extra = [l for l in locs if l not in L.item_locations]
  if missing:
    errors.append(f"{len(missing)} locations missing: {missing[:10]}")
  if extra:
    errors.append(f"{len(extra)} unknown locations: {extra[:10]}")
  for loc, item in locs.items():
    if not isinstance(item, str) or L.clean_item_name(item) not in L.all_cleaned_item_names:
      errors.append(f"unknown item {item!r} @ {loc}")
  if errors:
    return {"errors": errors}

  # 3. Per-pair validity, with the whole placement in done_item_locations.
  for loc, item in locs.items():
    L.done_item_locations[loc] = item
  for loc, item in locs.items():
    if not L.check_item_valid_in_location(item, loc):
      errors.append(f"invalid: {item} @ {loc}")
  # Banned (required-bosses) locations may hold nonprogress copies of an item
  # whose other copies are progress (e.g. one Progressive Wallet given as an
  # extra starting item leaves the remaining wallet nonprogress). Count per item
  # against the progress copies in unplaced_progress_items, like check 4.
  progress_need = collections.Counter(L.unplaced_progress_items)
  placed_all = collections.Counter(locs.values())
  in_banned = collections.defaultdict(list)
  for loc, item in locs.items():
    if loc in banned and not L.is_dungeon_item(item):
      in_banned[item].append(loc)
  for item, blocs in in_banned.items():
    allowed = max(0, placed_all[item] - progress_need[item])
    if len(blocs) > allowed:
      errors.append(f"progress item in banned location: {item} @ {', '.join(blocs)}"
                    f" ({len(blocs)} in banned locations, {progress_need[item]} progress"
                    f" of {placed_all[item]} placed)")
  if validity_only:
    return {"locations": len(locs), "errors": errors}

  # 2. Pool multiset.
  placed = collections.Counter(locs.values())
  unique = collections.Counter(L.unplaced_progress_items + L.unplaced_nonprogress_items)
  fixed = collections.Counter(L.unplaced_fixed_consumable_items)
  dup = set(L.duplicatable_consumable_items)
  for item, n in unique.items():
    if item in dup or item in fixed:
      if placed[item] < n:
        errors.append(f"pool: {item} placed {placed[item]}, expected at least {n}")
    elif placed[item] != n:
      errors.append(f"pool: {item} placed {placed[item]}, expected {n}")
  for item, m in placed.items():
    if item in unique:
      continue
    if item in fixed:
      if m > fixed[item] and item not in dup:
        errors.append(f"pool: consumable {item} placed {m}, pool has {fixed[item]}")
    elif item not in dup:
      errors.append(f"pool: {item} placed {m} but is not in the item pool")

  # 4. Progress copies in progress-eligible locations.
  progress_locs = set(L.filter_locations_for_progression(list(L.item_locations)))

  def eligible(loc):
    return ((loc in progress_locs or "Sunken Treasure" in L.item_locations[loc]["Types"])
            and loc not in banned)

  need = collections.Counter(i for i in L.unplaced_progress_items if not i.endswith(" Trap Chest"))
  have = collections.Counter(i for l, i in locs.items()
                             if eligible(l) or (L.is_dungeon_item(i) and not opts.keylunacy))
  for item, n in need.items():
    if have[item] < n:
      errors.append(f"only {have[item]}/{n} copies of progress item {item} in progress locations")

  # 5. Reachability fixpoint (assumed collection; no sphere labelling).
  remaining = set(L.item_locations)
  sweeps = 0
  with contextlib.redirect_stdout(sys.stderr):
    while True:
      got = [loc for loc in sorted(remaining) if L.check_location_accessible(loc)]
      if not got:
        break
      sweeps += 1
      for loc in got:
        remaining.discard(loc)
        L.add_owned_item(locs[loc])
    beatable = L.check_requirement_met("Can Reach and Defeat Ganondorf")
  if not beatable:
    errors.append("NOT BEATABLE")
  # A sunken treasure whose chart is not a progress item (its chart category is off) holds no
  # guarantee: wwrando's fill treats it as inaccessible and the chart, placed as a nonprogress
  # item, may sit anywhere (items.py only prints "Inaccessible locations"). Only sunken treasures
  # whose chart is progress must be reachable.
  progress_items = set(L.unplaced_progress_items)

  def must_reach(loc):
    if loc not in progress_locs or loc in banned:
      return False
    if "Sunken Treasure" not in L.item_locations[loc]["Types"]:
      return True
    m = re.search(r"Chart for Island (\d+)", str(L.item_locations[loc]["Need"]))  # parsed tokens
    return m is None or L.macros["Chart for Island " + m.group(1)][0] in progress_items

  unreach_prog = sorted(l for l in remaining if must_reach(l))
  if unreach_prog:
    errors.append(f"{len(unreach_prog)} progress locations unreachable")
  return {
    "beatable": beatable,
    "sweeps": sweeps,
    "unreachable": sorted(remaining),
    "unreachable_progress": unreach_prog,
    "errors": errors,
  }


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("placement")
  ap.add_argument("--validity-only", action="store_true")
  ap.add_argument("--quiet", action="store_true")
  a = ap.parse_args()
  oc.bootstrap()
  result = verify(oc.read_json(a.placement), a.validity_only)
  ok = not result["errors"]
  result = {"ok": ok, **result}
  if not ok or not a.quiet:
    print(json.dumps(result, indent=1))
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
