#!/usr/bin/env python3
"""Requirement-evaluation vectors for cos_rando_test_R1_1 (plan step R1.1).

    python3 -I native/tools/rando/gen_logic_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R1_1_logic_vectors.txt. The output is deterministic (fixed
Python Randoms pick the inventories), so regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_logic_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R1_1_logic_vectors.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode. For each of 4 option sets it
builds a WWRandomizer (so its Logic is initialized from the randomizer state, as at seed time), records
every set_macro call made after macros.txt was loaded, applies the set's extra macro rewrite (the
worst-case macros of make_useless_progress_items_nonprogress), and then, for 30 random inventories,
evaluates in this order with the caches cleared once per inventory:

    for each of the 320 locations:  check_location_accessible, then
                                    get_items_needed_from_logical_expression_req(Need)
    get_items_needed_by_req_name("Can Reach and Defeat Ganondorf")
    for each macro (dict order):    check_requirement_met(name)

The C++ test replays the same calls in the same order, so path-dependent cache entries (the nested-
entrance recursion guard) must match too. Tab-separated output:

    version  <VERSION>
    name     <id> <text>                     a name used by the need/inv lines, defined on first use
    need     <id> <name id>*<count>,...      a distinct items_needed dict, insertion order ("" = {})
    set      <n> <label>
    opt      <name> <value>                  options that differ from Options() (R1.2 value format)
    item     <cleaned item name>             Logic.all_cleaned_item_names, in order
    macro    <name> <body>                   the macros set_macro changed after macros.txt was loaded:
                                             the last body of each, in the order of the first call
    nested   <entrance macro> <zone macro>   Logic.nested_entrance_macros, in dict order
    inv      <k> <name id>*<count>,...       an inventory (currently_owned_items as counts), then:
    locs     <bits>                          check_location_accessible per location, '0'/'1'
    needs    <need id>,...                   items needed per location
    gan      <need id>                       (absent when wwrando raised before it)
    mac      <bits>                          check_requirement_met of each macro, dict order
    error    <text>                          wwrando raised; the lines before it hold the results
                                             computed until then (a location counts once both of
                                             its calls returned)
    end

Exit 0 written, 2 setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import contextlib  # noqa: E402
import random  # noqa: E402
from collections import Counter  # noqa: E402

N_INVENTORIES = 30
VECTOR_SEED = 0x52312E31  # "R1.1"

ENTRANCES_ALL = {
  "randomize_dungeon_entrances": True,
  "randomize_secret_cave_entrances": True,
  "randomize_miniboss_entrances": True,
  "randomize_boss_entrances": True,
  "randomize_secret_cave_inner_entrances": True,
  "randomize_fairy_fountain_entrances": True,
  "mix_entrances": "Mix Dungeons & Caves & Fountains",
}

# (label, options, extra rewrite applied to the Logic after the randomizer is built)
OPTION_SETS = [
  ("default", {}, None),
  ("swordless_bosses_traps_hard", {
    "sword_mode": "Swordless",
    "required_bosses": True,
    "num_required_bosses": 3,
    "trap_chests": True,
    "logic_obscurity": "Very Hard",
    "logic_precision": "Hard",
    "skip_rematch_bosses": False,
  }, "bosses_worst_case"),
  ("all_progression_keylunacy_charts", {
    "keylunacy": True,
    "sword_mode": "No Starting Sword",
    "progression_tingle_chests": True,
    "progression_dungeon_secrets": True,
    "progression_combat_secret_caves": True,
    "progression_savage_labyrinth": True,
    "progression_short_sidequests": True,
    "progression_long_sidequests": True,
    "progression_spoils_trading": True,
    "progression_minigames": True,
    "progression_battlesquid": True,
    "progression_mail": True,
    "progression_platforms_rafts": True,
    "progression_submarines": True,
    "progression_eye_reef_chests": True,
    "progression_big_octos_gunboats": True,
    "progression_triforce_charts": True,
    "progression_treasure_charts": True,
    "progression_island_puzzles": True,
    "logic_obscurity": "Normal",
    "logic_precision": "Very Hard",
  }, None),
  ("entrances_mixed_worst_case", dict(ENTRANCES_ALL), "entrances_worst_case"),
]


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  if isinstance(v, (list, tuple)):
    return "|".join(str(x) for x in v)
  return str(v)


class Tables:
  """Defines names and needed-dicts once, on first use."""
  def __init__(self, out):
    self.names = {}
    self.needs = {}
    self.out = out

  def name(self, text):
    if text not in self.names:
      self.names[text] = len(self.names)
      self.out.append(f"name\t{self.names[text]}\t{text}")
    return self.names[text]

  def counts(self, pairs):
    return ",".join(f"{self.name(k)}*{n}" for k, n in pairs)

  def need(self, d):
    key = self.counts(d.items())
    if key not in self.needs:
      self.needs[key] = len(self.needs)
      self.out.append(f"need\t{self.needs[key]}\t{key}")
    return self.needs[key]


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R1_1_logic_vectors.txt")
  oc.bootstrap()
  import version
  from logic.logic import Logic
  from options.wwrando_options import Options

  calls = []
  recording = [False]
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

  lines = [f"version\t{version.VERSION}"]
  tab = Tables(lines)
  defaults = Options()
  for set_no, (label, values, rewrite) in enumerate(OPTION_SETS):
    calls.clear()
    opts = oc.make_options(values)
    r = oc.make_randomizer(f"R1.1-{label}", opts)
    L = r.logic
    with contextlib.redirect_stdout(sys.stderr):
      if rewrite == "bosses_worst_case":
        L.temporarily_make_required_bosses_macro_worst_case_scenario()
      elif rewrite == "entrances_worst_case":
        L.temporarily_make_entrance_macros_worst_case_scenario()
    recording[0] = False

    lines.append(f"set\t{set_no}\t{label}")
    for f in Options.all():
      v, d = opts[f.name], defaults[f.name]
      if v != d:
        lines.append(f"opt\t{f.name}\t{opt_value(v)}")
    for name in L.all_cleaned_item_names:
      lines.append(f"item\t{name}")
    last = {}
    for name, body in calls:
      last[name] = body  # dict order: first call; value: last call
    for name, body in last.items():
      lines.append(f"macro\t{name}\t{body}")
    for k, v in L.nested_entrance_macros.items():
      lines.append(f"nested\t{k}\t{v}")

    # The inventories: random sub-multisets of every progress and nonprogress item, from empty to
    # nearly everything.
    pool = sorted(L.clean_item_name(x) for x in L.all_progress_items + L.all_nonprogress_items)
    for k in range(N_INVENTORIES):
      rng = random.Random(VECTOR_SEED * 100 + set_no * N_INVENTORIES + k)
      size = round(len(pool) * k / (N_INVENTORIES - 1) * 0.97)
      inv = rng.sample(pool, size)
      counts = Counter(inv)
      inv_line = f"inv\t{k}\t" + tab.counts((n, counts[n]) for n in sorted(counts))
      L.currently_owned_items = inv
      L.clear_req_caches()
      bits, need_ids, mac, gan, err = [], [], [], None, None
      try:
        for loc in L.item_locations:
          met = L.check_location_accessible(loc)
          need = L.get_items_needed_from_logical_expression_req(L.item_locations[loc]["Need"])
          bits.append("1" if met else "0")
          need_ids.append(str(tab.need(need)))
        gan = tab.need(L.get_items_needed_by_req_name("Can Reach and Defeat Ganondorf"))
        for m in L.macros:
          mac.append("1" if L.check_requirement_met(m) else "0")
      except Exception as e:  # noqa: BLE001 - the vector records that wwrando raised
        err = f"{type(e).__name__}: {e}"
      lines.append(inv_line)
      lines.append("locs\t" + "".join(bits))
      lines.append("needs\t" + ",".join(need_ids))
      if gan is not None:
        lines.append(f"gan\t{gan}")
      lines.append("mac\t" + "".join(mac))
      if err is not None:
        lines.append(f"error\t{err}")
  lines.append("end")

  text = "\n".join(lines) + "\n"
  with open(out_path, "w", newline="\n") as f:
    f.write(text)
  print(f"wrote {out_path} ({len(text)} bytes)", file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
