#!/usr/bin/env python3
"""Logic state and item pool vectors for cos_rando_test_R1_3 (plan step R1.3).

    python3 -I native/tools/rando/gen_pool_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R1_3_pools.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_pool_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R1_3_pools.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode. For each preset it builds a
WWRandomizer, which runs Logic.__init__, Logic.initialize_from_randomizer_state (starting items,
entrance and chart macros, make_useless_progress_items_nonprogress) and WWRandomizer.__init__'s
TooFewProgressionLocationsError check, dungeons_only_start and dungeons_and_caves_only_start. It
dumps that state, then probes the usefulness helpers the item fill uses (each probe owns and
un-owns items, which reorders unplaced_progress_items):

    1. get_all_useless_items(unplaced_progress_items)            -> 'useless', then 'list probe1'
    2. get_items_by_usefulness_fraction(unplaced_progress_items,
                                         filter_sunken_treasure=True) -> 'frac'
    3. get_first_useful_item(reversed(unplaced_progress_items))  -> 'first', then 'list probe3'

A preset whose randomizer raises (TooFewProgressionLocationsError) records the message and the
state the Logic had when it raised. Tab-separated output:

    version  <VERSION>
    preset   <label>
    opt      <name> <value>        options that differ from Options() (R1.2 value format)
    list     <name> <a|b|...>      a Python list of names, in order ("" = empty)
    locs     <name> <i,j,...>      a list of location indices (item_locations.txt order)
    num      <name> <int>
    flag     <name> 0|1
    macro    <name> <body>         every macro set_macro wrote after macros.txt loaded: its last
                                   body, in the order of the first call
    nested   <entrance macro> <zone macro>
    valid    <item> <hex>          check_item_valid_in_location for each of the 320 locations,
                                   bit i = location i, 80 hex digits, most significant first
    useless  <a|b|...>
    frac     <item>*<n>,...         dict order
    first    <item or "">
    error    <text>                wwrando raised (newlines as \\n)
    endpreset
    end

Exit 0 written, 2 setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import contextlib  # noqa: E402

ALL_PROGRESSION = {
  "progression_dungeons": True,
  "progression_tingle_chests": True,
  "progression_dungeon_secrets": True,
  "progression_puzzle_secret_caves": True,
  "progression_combat_secret_caves": True,
  "progression_savage_labyrinth": True,
  "progression_great_fairies": True,
  "progression_short_sidequests": True,
  "progression_long_sidequests": True,
  "progression_spoils_trading": True,
  "progression_minigames": True,
  "progression_battlesquid": True,
  "progression_free_gifts": True,
  "progression_mail": True,
  "progression_platforms_rafts": True,
  "progression_submarines": True,
  "progression_eye_reef_chests": True,
  "progression_big_octos_gunboats": True,
  "progression_triforce_charts": True,
  "progression_treasure_charts": True,
  "progression_expensive_purchases": True,
  "progression_island_puzzles": True,
  "progression_misc": True,
}
NO_PROGRESSION = {k: False for k in ALL_PROGRESSION}

PRESETS = [
  ("defaults", {}),
  ("all_categories", dict(ALL_PROGRESSION, keylunacy=True)),
  ("dungeons_only", dict(NO_PROGRESSION, progression_dungeons=True)),
  ("swordless_gear_traps", {
    "sword_mode": "Swordless",
    "trap_chests": True,
    "progression_combat_secret_caves": True,
    "progression_short_sidequests": True,
    "num_starting_triforce_shards": 3,
    "starting_pohs": 6,
    "starting_hcs": 5,
    "num_extra_starting_items": 2,
    "starting_gear": sorted(["Progressive Shield", "Progressive Magic Meter", "Ballad of Gales",
                             "Song of Passing", "Bombs", "Progressive Bow", "Progressive Bow",
                             "Grappling Hook", "Delivery Bag", "Empty Bottle"]),
  }),
  ("charts_bosses_entrances", {
    "sword_mode": "No Starting Sword",
    "progression_triforce_charts": True,
    "progression_treasure_charts": True,
    "progression_dungeon_secrets": True,
    "progression_savage_labyrinth": True,
    "required_bosses": True,
    "num_required_bosses": 2,
    "randomize_dungeon_entrances": True,
    "randomize_secret_cave_entrances": True,
    "randomize_miniboss_entrances": True,
    "randomize_boss_entrances": True,
    "randomize_secret_cave_inner_entrances": True,
    "randomize_fairy_fountain_entrances": True,
    "logic_obscurity": "Hard",
    "logic_precision": "Normal",
  }),
  ("entrances_mixed_caves", {
    "progression_combat_secret_caves": True,
    "randomize_secret_cave_entrances": True,
    "randomize_fairy_fountain_entrances": True,
    "randomize_dungeon_entrances": True,
    "mix_entrances": "Mix Dungeons & Caves & Fountains",
  }),
  ("too_few_locations", dict(NO_PROGRESSION, progression_free_gifts=True, progression_misc=True)),
]


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  if isinstance(v, (list, tuple)):
    return "|".join(str(x) for x in v)
  return str(v)


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R1_3_pools.txt")
  oc.bootstrap()
  import version
  from logic.logic import Logic
  from options.wwrando_options import Options

  calls = []
  recording = [False]
  last_logic = [None]
  orig_set_macro = Logic.set_macro

  def set_macro(self, macro_name, req_string):
    if recording[0]:
      calls.append((macro_name, req_string))
    return orig_set_macro(self, macro_name, req_string)
  Logic.set_macro = set_macro

  orig_load = Logic.load_and_parse_macros

  def load_macros(self):
    recording[0] = False
    r = orig_load(self)
    recording[0] = True
    return r
  Logic.load_and_parse_macros = load_macros

  orig_init_state = Logic.initialize_from_randomizer_state

  def init_state(self):
    last_logic[0] = self
    return orig_init_state(self)
  Logic.initialize_from_randomizer_state = init_state

  lines = [f"version\t{version.VERSION}"]
  defaults = Options()
  for label, values in PRESETS:
    calls.clear()
    last_logic[0] = None
    opts = oc.make_options(values)
    err = None
    r = None
    try:
      r = oc.make_randomizer(f"R1.3-{label}", opts)
    except Exception as e:  # noqa: BLE001 - the vector records that wwrando raised
      err = str(e)
    recording[0] = False
    L = r.logic if r is not None else last_logic[0]
    loc_names = list(L.item_locations.keys())
    loc_index = {n: i for i, n in enumerate(loc_names)}

    lines.append(f"preset\t{label}")
    for f in Options.all():
      v, d = opts[f.name], defaults[f.name]
      if v != d:
        lines.append(f"opt\t{f.name}\t{opt_value(v)}")

    def lst(name, items):
      lines.append(f"list\t{name}\t" + "|".join(items))

    def locs(name, names):
      lines.append(f"locs\t{name}\t" + ",".join(str(loc_index[n]) for n in names))

    lst("starting_items", L.rando.starting_items)
    lst("all_cleaned_item_names", L.all_cleaned_item_names)
    lst("all_progress_items", L.all_progress_items)
    lst("all_nonprogress_items", L.all_nonprogress_items)
    lst("all_fixed_consumable_items", L.all_fixed_consumable_items)
    lst("duplicatable_consumable_items", L.duplicatable_consumable_items)
    lst("unplaced_progress_items", L.unplaced_progress_items)
    lst("unplaced_nonprogress_items", L.unplaced_nonprogress_items)
    lst("unplaced_fixed_consumable_items", L.unplaced_fixed_consumable_items)
    lst("currently_owned_items", L.currently_owned_items)
    lines.append(f"num\tnum_progression_locations\t{L.get_num_progression_locations()}")
    lines.append(f"num\tmax_required_bosses_banned_locations\t{L.get_max_required_bosses_banned_locations()}")
    with contextlib.redirect_stdout(sys.stderr):
      prog, nonprog = L.get_progress_and_non_progress_locations()
    locs("progress_locations", prog)
    locs("nonprogress_locations", nonprog)
    locs("progression_filtered", L.filter_locations_for_progression(loc_names))
    locs("progression_filtered_sunken", L.filter_locations_for_progression(loc_names, filter_sunken_treasure=True))
    if r is not None:
      lst("all_randomized_progress_items", r.all_randomized_progress_items)
      lines.append(f"flag\tdungeons_only_start\t{int(r.dungeons_only_start)}")
      lines.append(f"flag\tdungeons_and_caves_only_start\t{int(r.dungeons_and_caves_only_start)}")
    last = {}
    for name, body in calls:
      last[name] = body
    for name, body in last.items():
      lines.append(f"macro\t{name}\t{body}")
    for k, v in L.nested_entrance_macros.items():
      lines.append(f"nested\t{k}\t{v}")

    items = []
    for name in (L.all_progress_items + L.all_nonprogress_items + L.all_fixed_consumable_items
                 + L.duplicatable_consumable_items):
      if name not in items:
        items.append(name)
    for item in items:
      bits = 0
      for i, loc in enumerate(loc_names):
        if L.check_item_valid_in_location(item, loc):
          bits |= 1 << i
      lines.append(f"valid\t{item}\t{bits:080x}")

    if r is not None:
      with contextlib.redirect_stdout(sys.stderr):
        useless = L.get_all_useless_items(L.unplaced_progress_items.copy())
        lines.append("useless\t" + "|".join(useless))
        lst("probe1", L.unplaced_progress_items)
        frac = L.get_items_by_usefulness_fraction(L.unplaced_progress_items.copy(), filter_sunken_treasure=True)
        lines.append("frac\t" + ",".join(f"{k}*{v}" for k, v in frac.items()))
        first = L.get_first_useful_item(list(reversed(L.unplaced_progress_items)))
        lines.append("first\t" + (first or ""))
        lst("probe3", L.unplaced_progress_items)
    if err is not None:
      lines.append("error\t" + err.replace("\n", "\\n"))
    lines.append("endpreset")
  lines.append("end")

  text = "\n".join(lines) + "\n"
  with open(out_path, "w", newline="\n") as f:
    f.write(text)
  print(f"wrote {out_path} ({len(text)} bytes)", file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
