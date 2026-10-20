#!/usr/bin/env python3
"""Golden placement from wwrando (the oracle's generator side).

    python3 -I native/tools/rando/ww_export.py <seed> <out.json> [name=<json> ...]
    python3 -I native/tools/rando/ww_export.py --permalink <permalink> <out.json>

Runs wwrando 1.10.0_9775811 in dry mode (no ISO) with the given seed and options and writes the
placement JSON, schema format 1 (docs/RANDOMIZER_PLAN.md "The oracle"):

    {format, seed, permalink, options, extra_starting_items, charts, required_boss_locations,
     entrances, starting_island_room, locations{location: item}}

Options start at wwrando's defaults. `name=<json>` sets one (a value that is not JSON is taken as a
string: sword_mode=Swordless); --options FILE takes a JSON dict of options, or the options of a
placement JSON; name=value pairs are applied after it. <out.json> may be '-' for stdout.

  --logs DIR        also write wwrando's spoiler and non-spoiler logs to DIR
  --seed-key KEY    wwrando's SEED_KEY for do_not_generate_spoiler_log seeds (default: empty, as in a
                    source checkout; SwitchWaker's own key is SWITCHWAKER_SEED_KEY, decision D2)

Exit 0 written, 2 usage or setup error; a wwrando failure (e.g. too few progression locations)
prints its traceback and exits 1.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import traceback  # noqa: E402


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("args", nargs="+", help="<seed> <out.json> [name=json ...] (or <out.json> with --permalink)")
  ap.add_argument("--permalink")
  ap.add_argument("--options", metavar="FILE")
  ap.add_argument("--logs", metavar="DIR")
  ap.add_argument("--seed-key", metavar="KEY")
  a = ap.parse_args()

  oc.bootstrap()
  if a.seed_key is not None:
    oc.set_seed_key(a.seed_key)

  if a.permalink:
    if a.options or len(a.args) != 1:
      ap.error("--permalink takes only <out.json> (the permalink holds the seed and options)")
    out_path = a.args[0]
    seed, opts = oc.decode_permalink(a.permalink)
  else:
    if len(a.args) < 2:
      ap.error("need <seed> <out.json>")
    seed, out_path, pairs = a.args[0], a.args[1], a.args[2:]
    values = oc.load_options_file(a.options) if a.options else {}
    values.update(oc.parse_opt_pairs(pairs))
    opts = oc.make_options(values)

  r = oc.make_randomizer(seed, opts, logs_dir=a.logs)
  if not r.seed:
    oc.die(f"seed '{seed}' is empty after wwrando's sanitizing")
  try:
    oc.randomize(r)
  except Exception:
    traceback.print_exc()
    return 1
  oc.write_json(oc.placement_from_randomizer(r, opts), out_path)
  return 0


if __name__ == "__main__":
  sys.exit(main())
