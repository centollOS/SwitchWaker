#!/usr/bin/env python3
"""Enemy logic filter vectors for cos_rando_test_R7_3b (plan step R7.3b).

    python3 -I native/tools/rando/gen_enemy_logic_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R7_3b_enemy_logic.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_enemy_logic_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R7_3b_enemy_logic.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode. For each case (a set of
options) it builds a WWRandomizer (its Logic is initialized from the randomizer state, as when
EnemyRandomizer runs right after the extra starting items) and calls
Logic.filter_out_enemies_that_add_new_requirements on the groups of logic/enemy_locations.txt in file
order, with the list EnemyRandomizer passes (enemies_to_randomize_to_when_all_enemies_must_be_killed):

  pass 1  every group with "Must defeat enemies" (what EnemyRandomizer.randomize_enemy_group calls);
  pass 2  the same groups again (every call is answered by the cache: no list changes);
  pass 3  every group without "Must defeat enemies" (more requirement strings for the algorithm;
          they share the cache and keep reordering the lists).

After each call it dumps the allowed list, the combo counts of get_all_item_combo_subsets_meeting_req
(or "cached" when the call returned early) and CRC-32s of the unplaced lists; at the end of a case
the full unplaced progress / nonprogress lists and the owned items. Tab-separated output:

    version  <VERSION>
    case     <name>
    opt      <name> <value>         options that differ from Options() (R1.2 value format)
    call     <pass> <group> <throwable 0|1> <bomb flowers 0|1> <original requirements>
                                    group = index into the groups in file order
    allowed  <pretty|pretty|...>
    combos   <matched> <checked>    or "combos cached"
    order    <crc progress> <crc nonprogress> <crc owned>   zlib.crc32 of "|".join(list), hex
    error    <text>                 the call raised (newlines as \\n)
    list     <name> <a|b|...>       at the end of the case
    endcase
    repr     <requirement string> <repr(Logic.parse_logic_expression(s))>   every requirement string
    guard    <a|b|...> <requirement> <error or "ok"> <matched> <checked>
                                    get_all_item_combo_subsets_meeting_req on a large combo (case
                                    default's fresh Logic), the 10,000-combo guard
    end

Exit 0 written, 2 setup error.
"""
import os
import sys
import zlib
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import contextlib  # noqa: E402


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  if isinstance(v, list):
    return "|".join(str(x) for x in v)
  return str(v)


def all_categories():
  import dataclasses
  from options.wwrando_options import Options
  return {f.name: True for f in dataclasses.fields(Options) if f.name.startswith("progression_")}


def cases():
  return [
    ("default", {}),
    ("all_categories", all_categories()),
    ("swordless", {"sword_mode": "Swordless"}),
    # Starting gear that the original requirements mention (the "remove starting items" step).
    ("start_gear", {"sword_mode": "No Starting Sword",
                    "starting_gear": ["Progressive Bow", "Hookshot", "Deku Leaf", "Bombs"]}),
  ]


def crc(lst):
  return "%08x" % (zlib.crc32("|".join(lst).encode("utf-8")) & 0xFFFFFFFF)


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R7_3b_enemy_logic.txt")
  oc.bootstrap()
  from version import VERSION
  from options.wwrando_options import Options
  defaults = Options()
  from logic.logic import Logic

  last_counts = []
  orig_subsets = Logic.get_all_item_combo_subsets_meeting_req

  def subsets(self, item_combo, orig_req_expression):
    matched, checked = orig_subsets(self, item_combo, orig_req_expression)
    last_counts.append((len(matched), len(checked)))
    return matched, checked
  Logic.get_all_item_combo_subsets_meeting_req = subsets

  lines = [f"version\t{VERSION}"]
  for name, values in cases():
    opts = oc.make_options(values)
    lines.append(f"case\t{name}")
    for k in sorted(values):
      if getattr(opts, k) != getattr(defaults, k):
        lines.append(f"opt\t{k}\t{opt_value(getattr(opts, k))}")
    r = oc.make_randomizer("EnemyLogic", opts)
    logic = r.logic
    er = r.enemies
    pool = er.enemies_to_randomize_to_when_all_enemies_must_be_killed
    groups = [g for stage_groups in er.enemy_locations.values() for g in stage_groups]

    def call(pass_no, gi, g):
      req = g["Original requirements"]
      thr = bool(g.get("Has throwable objects", False))
      bomb = bool(g.get("Has bomb flowers", False))
      lines.append(f"call\t{pass_no}\t{gi}\t{int(thr)}\t{int(bomb)}\t{req}")
      last_counts.clear()
      try:
        with contextlib.redirect_stdout(sys.stderr):
          allowed = logic.filter_out_enemies_that_add_new_requirements(req, thr, bomb, pool)
      except Exception as e:  # noqa: BLE001 - recorded for the test
        lines.append("error\t" + str(e).replace("\n", "\\n"))
      else:
        lines.append("allowed\t" + "|".join(d["Pretty name"] for d in allowed))
        if last_counts:
          lines.append(f"combos\t{last_counts[0][0]}\t{last_counts[0][1]}")
        else:
          lines.append("combos\tcached")
      lines.append(f"order\t{crc(logic.unplaced_progress_items)}\t{crc(logic.unplaced_nonprogress_items)}"
                   f"\t{crc(logic.currently_owned_items)}")

    for pass_no, must in ((1, True), (2, True), (3, False)):
      for gi, g in enumerate(groups):
        if bool(g["Must defeat enemies"]) == must:
          call(pass_no, gi, g)
    lines.append("list\tunplaced_progress_items\t" + "|".join(logic.unplaced_progress_items))
    lines.append("list\tunplaced_nonprogress_items\t" + "|".join(logic.unplaced_nonprogress_items))
    lines.append("list\tcurrently_owned_items\t" + "|".join(logic.currently_owned_items))
    lines.append("endcase")
  r = oc.make_randomizer("EnemyLogic", oc.make_options({}))
  er = r.enemies
  reqs = []
  for stage_groups in er.enemy_locations.values():
    for g in stage_groups:
      reqs.append(g["Original requirements"])
  reqs += [d["Requirements to defeat"] for d in er.enemy_types]
  reqs += ["(A & (B | C)) | D", " x ( ( y ) ) ", "Hero's Bow & \"Q\"", "it's"]
  seen = set()
  for req in reqs:
    if req not in seen:
      seen.add(req)
      lines.append(f"repr\t{req}\t{Logic.parse_logic_expression(req)!r}")
  guard_items = ["Hookshot", "Deku Leaf", "Grappling Hook", "Boomerang", "Bombs", "Skull Hammer",
                 "Power Bracelets", "Iron Boots", "Spoils Bag", "Bait Bag", "Delivery Bag",
                 "Magic Armor", "Hero's Charm", "Telescope"]
  for n, req in ((10, "Nothing"), (13, "Nothing"), (14, "Nothing"), (14, "Hookshot & Deku Leaf"),
                 (14, "Hookshot | Bombs")):
    combo = tuple(guard_items[:n])
    expr = Logic.parse_logic_expression(req)
    try:
      matched, checked = orig_subsets(r.logic, combo, expr)
      res = f"ok\t{len(matched)}\t{len(checked)}"
    except Exception as e:  # noqa: BLE001 - recorded for the test
      res = str(e).replace("\n", "\\n")
    lines.append(f"guard\t{'|'.join(combo)}\t{req}\t{res}")
  lines.append("list\tguard_unplaced_progress_items\t" + "|".join(r.logic.unplaced_progress_items))
  lines.append("list\tguard_unplaced_nonprogress_items\t" + "|".join(r.logic.unplaced_nonprogress_items))
  lines.append("end")
  with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
