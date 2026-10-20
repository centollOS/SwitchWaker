#!/usr/bin/env python3
"""Log every RNG draw wwrando makes while randomizing a seed, for call-by-call diffing against our
C++ generator (which writes the same format with COS_RANDO_TRACE=1).

    python3 -I native/tools/rando/ww_trace.py <seed> [name=<json> ...] [--options FILE]
                                              [--out FILE] [--owner NAME] [--summary]

Options are given as in ww_export.py. Each randomizer (ItemRandomizer, ChartRandomizer, ...) gets a
fresh RNG from get_new_rng() when it runs; the tool wraps that RNG and logs, in call order:

    # ww_trace 1 seed=<sanitized seed> version=1.10.0_9775811
    <Randomizer> randbelow <n> <result>
    <Randomizer> random <result * 2**53, as an integer>

`randbelow` is Random._randbelow, through which choice, shuffle, sample (also with counts=, i.e.
weighted_choice), randrange and randint all draw. `random` is Random.random, which only choices()
uses (hints). The get_new_rng warm-up (getrandbits per RNG-changing option) is not logged: it is
the same for every randomizer and checked by R0.2's vectors.

--owner keeps only one randomizer's lines; --summary prints draw counts per randomizer to stderr;
--placement FILE also writes the placement JSON (identical to ww_export.py's: tracing changes no draw).
Output goes to stdout unless --out is given. Exit 0, 1 if wwrando fails, 2 usage or setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import collections  # noqa: E402
import random  # noqa: E402
import traceback  # noqa: E402

TRACE_FORMAT = 1


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("seed")
  ap.add_argument("opts", nargs="*", help="name=<json> option overrides")
  ap.add_argument("--options", metavar="FILE")
  ap.add_argument("--out", metavar="FILE")
  ap.add_argument("--owner", metavar="NAME")
  ap.add_argument("--summary", action="store_true")
  ap.add_argument("--seed-key", metavar="KEY")
  ap.add_argument("--placement", metavar="FILE", help="also write the placement JSON (as ww_export.py)")
  a = ap.parse_args()

  oc.bootstrap()
  if a.seed_key is not None:
    oc.set_seed_key(a.seed_key)
  from randomizers.base_randomizer import BaseRandomizer

  values = oc.load_options_file(a.options) if a.options else {}
  values.update(oc.parse_opt_pairs(a.opts))
  opts = oc.make_options(values)

  lines = []
  counts = collections.defaultdict(collections.Counter)

  class TracingRandom(random.Random):
    owner = "?"

    def _randbelow(self, n):
      r = super()._randbelow(n)
      counts[self.owner]["randbelow"] += 1
      lines.append(f"{self.owner} randbelow {n} {r}")
      return r

    def random(self):
      x = super().random()
      counts[self.owner]["random"] += 1
      lines.append(f"{self.owner} random {int(x * (1 << 53))}")
      return x

  orig_reset = BaseRandomizer.reset_rng

  def reset_rng(self):
    orig_reset(self)
    t = TracingRandom()
    t.setstate(self._rng.getstate())
    t.owner = type(self).__name__
    self._rng = t

  BaseRandomizer.reset_rng = reset_rng

  r = oc.make_randomizer(a.seed, opts)
  try:
    oc.randomize(r)
  except Exception:
    traceback.print_exc()
    return 1

  header = f"# ww_trace {TRACE_FORMAT} seed={r.seed} version={oc.EXPECTED_VERSION}"
  body = [l for l in lines if a.owner is None or l.split(" ", 1)[0] == a.owner]
  text = "\n".join([header] + body) + "\n"
  if a.out:
    with open(a.out, "w", newline="\n") as f:
      f.write(text)
  else:
    sys.stdout.write(text)
  if a.placement:
    oc.write_json(oc.placement_from_randomizer(r, opts), a.placement)
  if a.summary:
    for owner, c in counts.items():
      print(owner, dict(c), file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
