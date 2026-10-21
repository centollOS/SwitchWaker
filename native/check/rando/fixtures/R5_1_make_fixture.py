#!/usr/bin/env python3
"""Writes R5_1_hints.json for cos_rando_test_R5_1 (docs/RANDOMIZER_PLAN.md step R5.1).

    python3 -I native/check/rando/fixtures/R5_1_make_fixture.py

Runs wwrando 1.10.0_9775811 (dry, no disc) on a few seeds and option sets that cover every hint
type and placement path (defaults; many path and barren hints with remote hints and importance;
key-lunacy; every category; one hint over three placements, which duplicates it; every placement
off) and writes, per case, the options it changed and the HintsRandomizer's hints as our
generator's "hints" value (ww_verify_hints.hints_to_json). Uses the oracle venv
(native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import json
import os
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "rando"))
import oracle_common as oc  # noqa: E402

oc.bootstrap()
import ww_verify_hints  # noqa: E402

HEAVY = {"num_path_hints": 6, "num_barren_hints": 6, "num_item_hints": 5, "num_location_hints": 6,
         "korl_hints": True, "prioritize_remote_hints": True, "hint_importance": True,
         "cryptic_hints": True}

CASES = [
  ("TestSeed1", {}),
  ("HintSeed7", {}),
  ("Heavy5", HEAVY),
  ("Heavy12", HEAVY),
  ("KeyHints3", dict(HEAVY, keylunacy=True)),
  ("AllCats2", dict({"num_path_hints": 4, "num_barren_hints": 4, "hint_importance": True},
                    **{p: True for p in __import__("check_seeds").progression_options()})),
  ("OneHint1", {"num_item_hints": 1, "num_location_hints": 0, "num_path_hints": 0, "num_barren_hints": 0,
                "fishmen_hints": True, "hoho_hints": True, "korl_hints": True}),
  ("NoPlacements4", {"fishmen_hints": False, "hoho_hints": False, "korl_hints": False}),
]


def main():
  oc.set_seed_key(ww_verify_hints.SWITCHWAKER_SEED_KEY)
  out = []
  for seed, values in CASES:
    opts = oc.make_options(values)
    r = oc.make_randomizer(seed, opts)
    oc.randomize(r)
    out.append({"seed": seed, "options": values, "hints": ww_verify_hints.hints_to_json(r)})
  path = os.path.join(HERE, "R5_1_hints.json")
  with open(path, "w", newline="\n") as f:
    f.write(json.dumps(out, indent=1) + "\n")
  print(f"wrote {path} ({len(out)} cases)")


if __name__ == "__main__":
  main()
