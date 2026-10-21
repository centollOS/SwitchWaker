#!/usr/bin/env python3
"""Golden item placements for cos_rando_test_R1_4 (plan step R1.4, the item fill).

    python3 -I native/tools/rando/gen_fill_vectors.py [out.txt] [--traces DIR]

Default output: native/check/rando/fixtures/R1_4_fill.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_fill_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R1_4_fill.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode for each case and dumps
logic.done_item_locations after the ItemRandomizer ran, plus the number of RNG draws the
ItemRandomizer made. The cases are the plan's 10 default seeds, then presets whose only randomizer
that changes the Logic is the ItemRandomizer (charts, required bosses, entrances and extra starting
items are R1.6's), covering key-lunacy, dungeons-and-caves-only start (the DRC failsafe), swordless,
every progression category, and a do_not_generate_spoiler_log seed made with the port's key
(SwitchWaker's SEED_KEY, decision D2).

--traces DIR also writes DIR/<label>.trace, the ItemRandomizer's draws in ww_trace.py's format (not
checked in; diff them against COS_RANDO_TRACE output of the test).

Tab-separated output:

    version  <VERSION>
    case     <label> <seed>
    key      <seed key>             the case uses this SEED_KEY (do_not_generate_spoiler_log)
    opt      <name> <value>         options that differ from Options() (R1.2 value format),
                                    except dry_run
    draws    <n>                    ItemRandomizer randbelow/random calls
    loc      <index> <item>         done_item_locations, item_locations.txt order
    endcase
    end

Exit 0 written, 1 wwrando failed, 2 setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import random  # noqa: E402
import traceback  # noqa: E402

# kSwitchWakerSeedKey in pc_rando_seedgen.cpp.
SWITCHWAKER_SEED_KEY = "SwitchWaker-rando-key-1"

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

DEFAULT_SEEDS = [
  "TestSeed1", "GummyAliveTrust", "R1.4-seed-3", "ZeldaSwitch", "Crestfallen'Pumpkin",
  "a", "2023", "WW_HD-%.", "LongSeedNameThatIsExactlyFortyTwoCharsLong", "KeyLunacyNot",
]

CASES = [(f"default{i + 1}", s, {}, None) for i, s in enumerate(DEFAULT_SEEDS)] + [
  ("keylunacy", "KeyLunacy1", {"keylunacy": True}, None),
  ("all_categories", "AllCats", dict(ALL_PROGRESSION, keylunacy=True), None),
  ("all_categories_keys", "AllCatsKeys", dict(ALL_PROGRESSION), None),
  ("dungeons_only", "DungeonsOnly", dict(NO_PROGRESSION, progression_dungeons=True), None),
  ("dungeons_only2", "DungeonsOnly2", dict(NO_PROGRESSION, progression_dungeons=True,
                                           progression_expensive_purchases=True), None),
  ("no_dungeons", "NoDungeons", {"progression_dungeons": False, "progression_combat_secret_caves": True,
                                 "progression_short_sidequests": True}, None),
  ("swordless", "Swordless1", {"sword_mode": "Swordless", "trap_chests": True,
                               "progression_tingle_chests": True}, None),
  ("no_starting_sword", "NoSword", {"sword_mode": "No Starting Sword", "logic_obscurity": "Hard",
                                     "logic_precision": "Very Hard",
                                     "progression_dungeon_secrets": True}, None),
  ("no_spoiler", "NoSpoiler", {"do_not_generate_spoiler_log": True}, SWITCHWAKER_SEED_KEY),
]


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  if isinstance(v, (list, tuple)):
    return "|".join(str(x) for x in v)
  return str(v)


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("out", nargs="?", default=os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R1_4_fill.txt"))
  ap.add_argument("--traces", metavar="DIR")
  a = ap.parse_args()

  oc.bootstrap()
  import version
  from options.wwrando_options import Options
  from randomizers.base_randomizer import BaseRandomizer
  from randomizers.items import ItemRandomizer

  trace = []

  class TracingRandom(random.Random):
    def _randbelow(self, n):
      r = super()._randbelow(n)
      trace.append(f"ItemRandomizer randbelow {n} {r}")
      return r

    def random(self):
      x = super().random()
      trace.append(f"ItemRandomizer random {int(x * (1 << 53))}")
      return x

  orig_reset = BaseRandomizer.reset_rng

  def reset_rng(self):
    orig_reset(self)
    if isinstance(self, ItemRandomizer):
      t = TracingRandom()
      t.setstate(self._rng.getstate())
      self._rng = t
  BaseRandomizer.reset_rng = reset_rng

  if a.traces:
    os.makedirs(a.traces, exist_ok=True)

  lines = [f"version\t{version.VERSION}"]
  defaults = Options()
  for label, seed, values, key in CASES:
    oc.set_seed_key(key if key is not None else "")
    opts = oc.make_options(values)
    trace.clear()
    try:
      r = oc.make_randomizer(seed, opts)
      oc.randomize(r)
    except Exception:  # noqa: BLE001
      traceback.print_exc()
      print(f"gen_fill_vectors: case {label} failed", file=sys.stderr)
      return 1
    loc_names = list(r.logic.item_locations.keys())
    lines.append(f"case\t{label}\t{r.seed}")
    if key is not None:
      lines.append(f"key\t{key}")
    for f in Options.all():
      v, d = opts[f.name], defaults[f.name]
      if v != d and f.name != "dry_run":  # every oracle run is dry; no effect on the fill
        lines.append(f"opt\t{f.name}\t{opt_value(v)}")
    lines.append(f"draws\t{len(trace)}")
    for i, n in enumerate(loc_names):
      lines.append(f"loc\t{i}\t{r.logic.done_item_locations[n]}")
    lines.append("endcase")
    if a.traces:
      with open(os.path.join(a.traces, f"{label}.trace"), "w", newline="\n") as f:
        f.write("\n".join([f"# ww_trace 1 seed={r.seed} version={version.VERSION}"] + trace) + "\n")
  lines.append("end")
  with open(a.out, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
