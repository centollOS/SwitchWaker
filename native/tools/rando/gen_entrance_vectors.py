#!/usr/bin/env python3
"""Entrance randomizer vectors for cos_rando_test_R6_11 (plan step R6.11).

    python3 -I native/tools/rando/gen_entrance_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R6_11_entrances.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_entrance_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R6_11_entrances.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode. For each case (a seed, the
options) it builds a WWRandomizer and runs, as WWRandomizer.randomize does, only the randomizers
before it that the entrances depend on (RequiredBossesRandomizer when required_bosses is on) and then
EntranceRandomizer.randomize() (a fresh RNG from get_new_rng, _randomize), and dumps its state.
Tab-separated output:

    version  <VERSION>
    case     <seed>
    opt      <name> <value>        options that differ from Options() (R1.2 value format)
    conn     <exit|exit|...>       entrance_connections' values, in dict order (44)
    paths    <a>b>c|...>           nested_entrance_paths (elements joined by '>', paths by '|')
    nesting  <0|1>                 nesting_enabled
    macro    <exit> <body>         "Can Access <exit>" afterwards, for each exit, entrance dict order
    initerror <text>               WWRandomizer.__init__ raised (TooFewProgressionLocationsError)
    error    <text>                randomize() raised (newlines as \\n)
    endcase
    end

Exit 0 written, 2 setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import contextlib  # noqa: E402

MIX = "Mix Dungeons & Caves & Fountains"
ENTRANCE_OPTS = ("randomize_dungeon_entrances", "randomize_secret_cave_entrances",
                 "randomize_miniboss_entrances", "randomize_boss_entrances",
                 "randomize_secret_cave_inner_entrances", "randomize_fairy_fountain_entrances")


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  return str(v)


def all_categories():
  import dataclasses
  from options.wwrando_options import Options
  return {f.name: True for f in dataclasses.fields(Options) if f.name.startswith("progression_")}


def dungeons_only():
  v = {k: False for k in all_categories()}
  v["progression_dungeons"] = True
  return v


def all_entrances(mix=False):
  v = {k: True for k in ENTRANCE_OPTS}
  if mix:
    v["mix_entrances"] = MIX
  return v


def cases():
  out = []
  # Each option on its own (inner caves alone raise "Not enough island entrances").
  for opt in ENTRANCE_OPTS:
    short = opt[len("randomize_"):-len("_entrances")]
    for seed in ("EntA", "EntB"):
      out.append((f"{seed}_{short}", {opt: True}))
  for seed in ("EntA", "EntB", "EntC"):
    out.append((f"{seed}_sep", all_entrances()))
    out.append((f"{seed}_mix", all_entrances(True)))
  v = all_categories()
  v.update(all_entrances(True))
  out.append(("EntAll_mix", v))
  # Required bosses: banned exits, islands with a banned dungeon, the boss island check.
  for n, seed in ((2, "EntRB2"), (4, "EntRB4")):
    v = all_categories()
    v.update(all_entrances(True))
    v.update({"required_bosses": True, "num_required_bosses": n})
    out.append((seed, v))
  v = all_categories()
  v.update({"required_bosses": True, "num_required_bosses": 3, "randomize_boss_entrances": True,
            "randomize_miniboss_entrances": True})
  out.append(("EntRB3_fixed_dungeons", v))
  # Dungeons(-and-caves)-only starts: the safety entrance.
  for seed in ("EntDO1", "EntDO2"):
    v = dungeons_only()
    v.update({"randomize_dungeon_entrances": True, "randomize_boss_entrances": True})
    out.append((seed, v))
  v = dungeons_only()
  v.update({"progression_puzzle_secret_caves": True, "progression_savage_labyrinth": True})
  v.update(all_entrances(True))
  out.append(("EntDC_mix", v))
  v = dungeons_only()
  v.update({"progression_puzzle_secret_caves": True, "progression_combat_secret_caves": True,
            "progression_savage_labyrinth": True, "required_bosses": True, "num_required_bosses": 5})
  v.update(all_entrances(False))
  out.append(("EntDCRB_sep", v))
  return out


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R6_11_entrances.txt")
  oc.bootstrap()
  from version import VERSION
  from options.wwrando_options import Options
  from randomizers.entrances import ZoneExit
  defaults = Options()
  from logic.logic import Logic
  macro_strings = {}
  orig_set_macro = Logic.set_macro

  def set_macro(self, macro_name, req_string):
    macro_strings[macro_name] = req_string
    return orig_set_macro(self, macro_name, req_string)
  Logic.set_macro = set_macro

  lines = [f"version\t{VERSION}"]
  for seed, values in cases():
    opts = oc.make_options(values)
    lines.append(f"case\t{seed}")
    for k in sorted(values):
      if getattr(opts, k) != getattr(defaults, k):
        lines.append(f"opt\t{k}\t{opt_value(getattr(opts, k))}")
    try:
      r = oc.make_randomizer(seed, opts)
    except Exception as e:  # noqa: BLE001 - e.g. TooFewProgressionLocationsError in __init__
      lines.append("initerror\t" + str(e).replace("\n", "\\n"))
      lines.append("endcase")
      continue
    macro_strings.clear()
    er = r.entrances
    try:
      with contextlib.redirect_stdout(sys.stderr):
        if r.boss_reqs.is_enabled():
          r.boss_reqs.randomize()
        er.randomize()
    except Exception as e:  # noqa: BLE001 - recorded for the test
      lines.append("error\t" + str(e).replace("\n", "\\n"))
      lines.append("endcase")
      continue
    lines.append("conn\t" + "|".join(er.entrance_connections.values()))
    lines.append("paths\t" + "|".join(">".join(p) for p in er.nested_entrance_paths))
    lines.append("nesting\t" + ("1" if er.nesting_enabled else "0"))
    for exit_name in er.entrance_connections.values():
      assert exit_name in ZoneExit.all
      lines.append(f"macro\t{exit_name}\t" + macro_strings.get("Can Access " + exit_name, "<unset>"))
    lines.append("endcase")
  lines.append("end")
  with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
