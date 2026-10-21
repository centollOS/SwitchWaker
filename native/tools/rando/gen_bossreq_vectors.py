#!/usr/bin/env python3
"""Required bosses vectors for cos_rando_test_R6_8 (plan step R6.8).

    python3 -I native/tools/rando/gen_bossreq_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R6_8_bossreqs.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_bossreq_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R6_8_bossreqs.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode. For each case (a seed, the
options) it builds a WWRandomizer and runs only RequiredBossesRandomizer.randomize() (a fresh RNG
from get_new_rng, randomize_required_bosses, update_required_bosses_macro), then dumps its state.
Tab-separated output:

    version  <VERSION>
    case     <seed>
    opt      <name> <value>        options that differ from Options() (R1.2 value format)
    list     <name> <a|b|...>      a Python list, in order ("" = empty)
    locs     banned_locations <i,j,...>  location indices (item_locations.txt order)
    macro    <body>                "Can Defeat All Required Bosses" afterwards
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



def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  return str(v)


def all_categories():
  import dataclasses
  from options.wwrando_options import Options
  return {f.name: True for f in dataclasses.fields(Options) if f.name.startswith("progression_")}


def cases():
  out = []
  for seed in ("BossReqA", "BossReqB", "BossReqC"):
    for n in range(1, 7):
      out.append((f"{seed}{n}", {"required_bosses": True, "num_required_bosses": n}))
  # The 4 letters are only banned when their category is on (mail); all categories, and
  # key-lunacy / do_not_generate_spoiler_log (RNG-changing options).
  for n in (1, 3, 5):
    v = all_categories()
    v.update({"required_bosses": True, "num_required_bosses": n})
    out.append((f"BossReqAll{n}", v))
  out.append(("BossReqKL", {"required_bosses": True, "num_required_bosses": 2, "keylunacy": True}))
  return out


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R6_8_bossreqs.txt")
  oc.bootstrap()
  from version import VERSION
  from options.wwrando_options import Options
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
    br = r.boss_reqs
    try:
      with contextlib.redirect_stdout(sys.stderr):
        br.randomize()
    except Exception as e:  # noqa: BLE001 - recorded for the test
      lines.append("error\t" + str(e).replace("\n", "\\n"))
      lines.append("endcase")
      continue
    for name in ("required_boss_item_locations", "required_dungeons", "required_bosses",
                 "banned_dungeons", "banned_bosses"):
      lines.append(f"list\t{name}\t" + "|".join(getattr(br, name)))
    order = list(r.logic.item_locations.keys())
    lines.append("locs\tbanned_locations\t" + ",".join(str(order.index(x)) for x in br.banned_locations))
    lines.append("macro\t" + macro_strings.get("Can Defeat All Required Bosses", "<unset>"))
    lines.append("endcase")
  lines.append("end")
  with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
