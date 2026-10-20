#!/usr/bin/env python3
"""Standard test placements for the runtime steps and the location sweep (R2.12).

    python3 -I native/tools/rando/make_test_placement.py --kind identity <out.json> [name=<json> ...]
    python3 -I native/tools/rando/make_test_placement.py --kind rotation <out.json> [name=<json> ...]
    python3 -I native/tools/rando/make_test_placement.py --kind seed --seed S <out.json> [name=<json> ...]

All three write schema format 1 (as ww_export.py) for all 320 locations, with wwrando's default
options unless overridden (name=<json> pairs or --options FILE, as in ww_export.py).

identity  Every location holds its original item (logic/item_locations.txt "Original item"); charts,
          entrances, starting island and required bosses are vanilla. A game with this placement
          must behave exactly like the vanilla game.
rotation  Deterministic (no RNG). Every location gets an item that wwrando's
          check_item_valid_in_location accepts, different from its original item and, where the
          location allows one, of another class (progress, nonprogress, consumable, dungeon item,
          chart, trap). Items are taken round-robin from the sorted list of every item the pool or
          the vanilla game uses (wwrando logic names), preferring items not placed yet, so every
          item is placed at least once where some location accepts it; the coverage is printed to
          stderr. Not meant to be beatable: check it with
          `ww_verify.py --validity-only`.
seed      A real seed, exactly as ww_export.py <seed> writes it.

Exit 0 written, 1 if a rotation location has no valid item or wwrando fails, 2 usage or setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import contextlib  # noqa: E402
import traceback  # noqa: E402


def vanilla_placement(r, opts, seed_name, locations):
  return {
    "format": oc.PLACEMENT_FORMAT,
    "seed": seed_name,
    "permalink": r.permalink,
    "options": oc.options_to_json(opts),
    "extra_starting_items": [],
    "charts": {str(k): v for k, v in r.charts.island_number_to_chart_name.items()},
    "required_boss_locations": [],
    "entrances": dict(r.entrances.entrance_connections),
    "starting_island_room": r.starting_island.room_number,
    "locations": locations,
  }


# Vanilla item names (data/item_names.txt, used by "Original item") that wwrando's logic knows by
# another name. Dungeon items become "<DRC|FW|TotG|FF|ET|WT> <name>".
DUNGEON_ITEM_NAMES = ("Small Key", "Big Key", "Dungeon Map", "Compass")
VANILLA_TO_LOGIC = {
  "Hero's Sword": "Progressive Sword", "Master Sword (Powerless)": "Progressive Sword",
  "Master Sword (Half Power)": "Progressive Sword", "Master Sword (Full Power)": "Progressive Sword",
  "Hero's Shield": "Progressive Shield", "Mirror Shield": "Progressive Shield",
  "Hero's Bow": "Progressive Bow", "Fire and Ice Arrows": "Progressive Bow", "Light Arrow": "Progressive Bow",
  "1000 Rupee Wallet": "Progressive Wallet", "5000 Rupee Wallet": "Progressive Wallet",
  "60 Bomb Bomb Bag": "Progressive Bomb Bag", "99 Bomb Bomb Bag": "Progressive Bomb Bag",
  "60 Arrow Quiver": "Progressive Quiver", "99 Arrow Quiver": "Progressive Quiver",
  "Picto Box": "Progressive Picto Box", "Deluxe Picto Box": "Progressive Picto Box",
  "Magic Meter": "Progressive Magic Meter", "Magic Meter Upgrade": "Progressive Magic Meter",
  "Piece of Heart (Alternate Message)": "Piece of Heart",
  "500 Rupees, reward for finding all Tingle Statues": "Rainbow Rupee",
}


def same_item(orig, item):
  if item == orig or VANILLA_TO_LOGIC.get(orig) == item:
    return True
  return orig in DUNGEON_ITEM_NAMES and item.endswith(" " + orig)


def original_class(L, orig):
  if orig in DUNGEON_ITEM_NAMES:
    return "dungeon"
  name = VANILLA_TO_LOGIC.get(orig, orig)
  if L.clean_item_name(name) in L.all_cleaned_item_names:
    return item_class(L, name)
  return None


def item_class(L, item):
  if L.is_dungeon_item(item):
    return "dungeon"
  if item.endswith(" Trap Chest"):
    return "trap"
  if item.startswith("Treasure Chart") or item.startswith("Triforce Chart") or item == "Ghost Ship Chart":
    return "chart"
  if item in L.all_fixed_consumable_items or item in L.duplicatable_consumable_items:
    return "consumable"
  if item in L.all_progress_items:
    return "progress"
  return "nonprogress"


def rotation(r):
  L = r.logic
  with contextlib.redirect_stdout(sys.stderr):
    L.initialize_from_randomizer_state()
  originals = {loc: L.item_locations[loc]["Original item"] for loc in L.item_locations}
  known = lambda i: L.clean_item_name(i) in L.all_cleaned_item_names  # noqa: E731
  pool = set(L.all_progress_items) | set(L.all_nonprogress_items)
  pool |= set(L.all_fixed_consumable_items) | set(L.duplicatable_consumable_items)
  pool |= set(originals.values())
  candidates = sorted(i for i in pool if known(i))

  placed = {}
  used = set()
  cursor = 0
  same_class = []
  for loc in L.item_locations:
    orig = originals[loc]
    orig_class = original_class(L, orig)
    # Best candidate in round-robin order from the cursor: another class first, then an item not
    # placed yet (so every item ID shows up where some location accepts it).
    choice, best_rank = None, None
    for k in range(len(candidates)):
      idx = (cursor + k) % len(candidates)
      item = candidates[idx]
      if same_item(orig, item) or not L.check_item_valid_in_location(item, loc):
        continue
      # Unplaced dungeon items go first: only their own dungeon accepts them.
      rank = (item_class(L, item) == orig_class, item in used, not L.is_dungeon_item(item))
      if best_rank is None or rank < best_rank:
        choice, best_rank = idx, rank
        if rank == (False, False, False):
          break
    if choice is None:
      oc.die(f"rotation: no valid item for {loc}", code=1)
    if best_rank[0]:
      same_class.append(loc)
    item = candidates[choice]
    placed[loc] = item
    used.add(item)
    L.done_item_locations[loc] = item  # the rock spire shop rule looks at the other slots
    cursor = choice + 1

  unused = [i for i in candidates if i not in used]
  print(f"rotation: {len(placed)} locations, {len(used)}/{len(candidates)} distinct items placed, "
        f"{len(same_class)} locations only allow the original's class", file=sys.stderr)
  if unused:
    print(f"rotation: never placed (no location accepts them here): {unused}", file=sys.stderr)
  return placed


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--kind", required=True, choices=("identity", "rotation", "seed"))
  ap.add_argument("--seed")
  ap.add_argument("--options", metavar="FILE")
  ap.add_argument("out")
  ap.add_argument("opts", nargs="*", help="name=<json> option overrides")
  a = ap.parse_args()
  if (a.kind == "seed") != bool(a.seed):
    ap.error("--seed is required with --kind seed and only allowed there")

  oc.bootstrap()
  values = oc.load_options_file(a.options) if a.options else {}
  values.update(oc.parse_opt_pairs(a.opts))
  opts = oc.make_options(values)

  if a.kind == "seed":
    r = oc.make_randomizer(a.seed, opts)
    try:
      oc.randomize(r)
    except Exception:
      traceback.print_exc()
      return 1
    out = oc.placement_from_randomizer(r, opts)
  else:
    r = oc.make_randomizer(a.kind, opts)
    if a.kind == "identity":
      locations = {loc: e["Original item"] for loc, e in r.logic.item_locations.items()}
    else:
      locations = rotation(r)
    out = vanilla_placement(r, opts, a.kind, locations)
  oc.write_json(out, a.out)
  return 0


if __name__ == "__main__":
  sys.exit(main())
