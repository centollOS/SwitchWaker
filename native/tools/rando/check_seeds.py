#!/usr/bin/env python3
"""Oracle batch for our seed generator (docs/RANDOMIZER_PLAN.md steps R1.6 and R6.1).

    python3 -I native/tools/rando/check_seeds.py [--cli PATH] [--n N] [--preset P[,P...]] [--golden]
                                                 [--jobs J] [--prefix S] [--start K] [--keep DIR]
                                                 [--list-presets] [name=<json> ...]

Generates N seeds (<prefix><K>, <prefix><K+1>, ...) with our CLI (cos_rando_cli, default
build/native-mac/cos_rando_cli of this checkout) at the preset's options, then checks each
placement with wwrando's own Logic (ww_verify.verify: complete, valid pairs, pool, beatable).

--golden also runs wwrando 1.10.0_9775811 (dry) on the same seed and options and requires:
  - the placement identical to ww_export.py's: seed, permalink, options (except dry_run, which the
    oracle's --dry sets), extra starting items, charts, required bosses, entrances, starting island
    and every location's item, in order;
  - captured_pigs_bitfield equal to PigsRandomizer's;
  - the non-spoiler log byte-identical, and the spoiler log byte-identical except its Hints
    section (hints are step R5; decision D1 checks them for validity only), after dropping our
    trailer line (D5) and wwrando's "dry_run" option in the header;
  - a seed our generator refuses with a wwrando error (e.g. too few progression locations) must
    make wwrando raise the same message.

Presets (--preset, default "default"; a comma-separated list runs each in turn with the same seeds);
name=<json> pairs are applied after the preset:
  default          wwrando's Options()
  all_categories   every progression_* option on
  dungeons_only    progression_dungeons only
  no_dungeons      the defaults without progression_dungeons
  only_<category>  R6.1: that one progression_<category> on and the other 22 off (only_dungeons is
                   dungeons_only). Most single categories have too few progression locations, so
                   wwrando raises TooFewProgressionLocationsError and the check is error parity.
  dungeons_<category>  R6.1: progression_dungeons plus that one category, everything else off, so
                   each category's locations are placed (and checked) on a beatable base.
  obscurity_<lvl>  R6.1: the defaults with logic_obscurity at <lvl> (none, normal, hard, very_hard)
  precision_<lvl>  R6.1: the defaults with logic_precision at <lvl>
  all_tricks       R6.1: all_categories with both logic options at Very Hard
  r6_1             R6.1's matrix: all_categories, the 23 only_* presets, the 22 dungeons_*
                   presets, the 8 logic presets and all_tricks
  (--list-presets prints every name.)

The CLI runs with --allow-missing-hints unless --hints is given (R5.1): then our hints are generated
and the spoiler log must be byte-identical including its Hints section (ww_verify_hints.py checks
the hints themselves). The wwrando side runs in J
worker processes (default 4), each importing wwrando once (about 0.5 s per seed and check).

Prints one line per failing seed and a summary with our generation time. Exit 0 all passed, 1 a
check failed, 2 usage or setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import contextlib  # noqa: E402
import json  # noqa: E402
import multiprocessing  # noqa: E402
import re  # noqa: E402
import shutil  # noqa: E402
import subprocess  # noqa: E402
import tempfile  # noqa: E402

# The port's SEED_KEY for do_not_generate_spoiler_log seeds (decision D2): kSwitchWakerSeedKey in
# native/src/pc/features/rando/pc_rando_seedgen.cpp.
SWITCHWAKER_SEED_KEY = "SwitchWaker-rando-key-1"
CLI_VERSION = "check_seeds"

PLACEMENT_KEYS = ("format", "seed", "permalink", "extra_starting_items", "charts",
                  "required_boss_locations", "entrances", "starting_island_room")


def progression_options():
  from options.wwrando_options import Options
  return [f.name for f in __import__("dataclasses").fields(Options) if f.name.startswith("progression_")]


# R6.1: wwrando's TrickDifficulty values, by preset suffix.
TRICK_LEVELS = (("none", "None"), ("normal", "Normal"), ("hard", "Hard"), ("very_hard", "Very Hard"))


def r6_1_presets():
  prog = progression_options()
  names = ["all_categories"] + ["only_" + p[len("progression_"):] for p in prog]
  names += ["dungeons_" + p[len("progression_"):] for p in prog if p != "progression_dungeons"]
  for opt in ("obscurity", "precision"):
    names += [f"{opt}_{lvl}" for lvl, _ in TRICK_LEVELS]
  return names + ["all_tricks"]


def all_presets():
  return ["default", "dungeons_only", "no_dungeons"] + r6_1_presets()


def preset_values(name):
  if name == "default":
    return {}
  prog = progression_options()
  if name == "all_categories":
    return {p: True for p in prog}
  if name == "dungeons_only":
    return {p: p == "progression_dungeons" for p in prog}
  if name == "no_dungeons":
    return {"progression_dungeons": False}
  if name == "all_tricks":
    return dict({p: True for p in prog}, logic_obscurity="Very Hard", logic_precision="Very Hard")
  if name.startswith("dungeons_") and "progression_" + name[9:] in prog:
    return {p: p in ("progression_dungeons", "progression_" + name[9:]) for p in prog}
  if name.startswith("only_") and "progression_" + name[5:] in prog:
    return {p: p == "progression_" + name[5:] for p in prog}
  for opt in ("obscurity", "precision"):
    for lvl, value in TRICK_LEVELS:
      if name == f"{opt}_{lvl}":
        return {f"logic_{opt}": value}
  oc.die(f"unknown preset '{name}' (--list-presets lists them)")


def expand_presets(spec):
  out = []
  for name in spec.split(","):
    out += r6_1_presets() if name == "r6_1" else [name]
  return out


def log_names(seed):
  return (f"WW Random {seed} - Spoiler Log.txt", f"WW Random {seed} - Non-Spoiler Log.txt")


def read_text(path):
  try:
    with open(path, newline="") as f:
      return f.read()
  except OSError:
    return None


def strip_trailer(text):
  # D5: our logs end with one "Generated by SwitchWaker <version>" line.
  trailer = f"Generated by SwitchWaker {CLI_VERSION}\n"
  if text is not None and text.endswith(trailer):
    return text[:-len(trailer)]
  return text


def drop_dry_run(text):
  return re.sub(r", dry_run$", "", text, flags=re.M) if text is not None else None


def drop_hints(text):
  if text is None:
    return None
  i = text.find("Hints:\n")
  if i < 0:
    return text
  j = text.find("\n\n\n", i)
  return text[:i] + (text[j + 3:] if j >= 0 else "")


def first_diff(a, b):
  la, lb = a.split("\n"), b.split("\n")
  for i, (x, y) in enumerate(zip(la, lb)):
    if x != y:
      return f"line {i + 1}: ours {x!r} wwrando {y!r}"
  return f"length: ours {len(la)} lines, wwrando {len(lb)}"


# ---------------------------------------------------------------------------------------------
# Worker side (one wwrando import per process)

def worker_init():
  oc.bootstrap()
  oc.set_seed_key(SWITCHWAKER_SEED_KEY)


def check_one(job):
  """job: (seed, values, our_json_path or None, our_error or None, out_dir, golden). Returns
  (seed, [problems])."""
  seed, values, path, our_error, out_dir, golden, hints = job
  problems = []
  d = None
  if path is not None:
    d = oc.read_json(path)
    import ww_verify
    with contextlib.redirect_stdout(sys.stderr):
      res = ww_verify.verify(d)
    if res.get("errors"):
      problems.append("ww_verify: " + "; ".join(str(e) for e in res["errors"][:5]))
  if not golden:
    return seed, problems

  logs_dir = os.path.join(out_dir, "ww_" + seed)
  opts = oc.make_options(values)
  try:
    # WWRandomizer.__init__ already raises some errors (TooFewProgressionLocationsError), so
    # building the randomizer sits inside the try too.
    r = oc.make_randomizer(seed, opts, logs_dir=logs_dir)
    oc.randomize(r)
  except Exception as e:  # wwrando raised
    msg = str(e).split("\n")[0]
    if d is not None:
      problems.append(f"wwrando raised but we generated: {msg}")
    elif our_error != msg:
      problems.append(f"different errors: ours {our_error!r} wwrando {msg!r}")
    return seed, problems
  if d is None:
    problems.append(f"we failed ({our_error}) but wwrando generated the seed")
    return seed, problems

  want = oc.placement_from_randomizer(r, opts)
  for k in PLACEMENT_KEYS:
    if d.get(k) != want[k]:
      problems.append(f"placement key {k} differs")
  ours_opts = dict(d.get("options", {}))
  want_opts = dict(want["options"])
  ours_opts.pop("dry_run", None)
  want_opts.pop("dry_run", None)
  if ours_opts != want_opts:
    diff = sorted(k for k in set(ours_opts) | set(want_opts) if ours_opts.get(k) != want_opts.get(k))
    problems.append(f"options differ: {diff[:5]}")
  ol, wl = list(d.get("locations", {}).items()), list(want["locations"].items())
  if ol != wl:
    bad = [a[0] for a, b in zip(ol, wl) if a != b]
    problems.append(f"locations differ ({len(bad)}): first {bad[:1]}" if bad else "location order differs")
  if d.get("captured_pigs_bitfield") != r.pigs.captured_pigs_bitfield:
    problems.append(f"pigs: ours {d.get('captured_pigs_bitfield')} wwrando {r.pigs.captured_pigs_bitfield}")

  spoiler, non_spoiler = log_names(seed)
  ours_dir = os.path.dirname(path)
  for name, hints_free in ((non_spoiler, False), (spoiler, True)):
    ours = strip_trailer(read_text(os.path.join(ours_dir, name)))
    theirs = drop_dry_run(read_text(os.path.join(logs_dir, name)))
    if hints_free and not hints:
      ours, theirs = drop_hints(ours), drop_hints(theirs)
    if ours is None or theirs is None:
      if ours is not None or theirs is not None:
        problems.append(f"{name}: present on one side only")
    elif ours != theirs:
      problems.append(f"{name}: {first_diff(ours, theirs)}")
  return seed, problems


# ---------------------------------------------------------------------------------------------

def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("pairs", nargs="*", help="name=<json> option overrides")
  ap.add_argument("--cli", default=os.path.join(oc.REPO_ROOT, "build", "native-mac", "cos_rando_cli"))
  ap.add_argument("--n", type=int, default=20)
  ap.add_argument("--preset", default="default")
  ap.add_argument("--golden", action="store_true")
  ap.add_argument("--hints", action="store_true", help="generate hints too and compare them in the logs")
  ap.add_argument("--jobs", type=int, default=4)
  ap.add_argument("--prefix", default="CheckSeed")
  ap.add_argument("--start", type=int, default=0)
  ap.add_argument("--keep", metavar="DIR", help="keep the placements and logs in DIR")
  ap.add_argument("--list-presets", action="store_true")
  a = ap.parse_args()

  oc.bootstrap()
  if a.list_presets:
    print("\n".join(all_presets() + ["r6_1"]))
    return 0
  if not os.access(a.cli, os.X_OK):
    oc.die(f"no CLI at {a.cli} (ninja -C build/native-mac cos_rando_cli)")
  presets = expand_presets(a.preset)
  overrides = oc.parse_opt_pairs(a.pairs)
  preset_vals = {}
  for name in presets:
    values = preset_values(name)
    values.update(overrides)
    oc.make_options(values)  # validates the names and values
    preset_vals[name] = values
  seeds = [f"{a.prefix}{i}" for i in range(a.start, a.start + a.n)]
  if any(sanitize(s) != s for s in seeds):
    oc.die("--prefix must use only wwrando's seed characters and stay within 42 characters")

  base_dir = a.keep or tempfile.mkdtemp(prefix="check_seeds_")
  os.makedirs(base_dir, exist_ok=True)
  ctx = multiprocessing.get_context("spawn")
  failed_presets = []
  with ctx.Pool(processes=max(1, a.jobs), initializer=worker_init) as pool:
    for name in presets:
      # One folder per preset when several run, so wwrando's logs of the same seed do not mix.
      out_dir = os.path.join(base_dir, name) if len(presets) > 1 else base_dir
      if run_preset(pool, a, name, preset_vals[name], seeds, out_dir):
        failed_presets.append(name)
  if len(presets) > 1:
    print(f"check_seeds: {len(presets)} presets, {len(presets) - len(failed_presets)} passed, "
          f"{len(failed_presets)} failed" + (f": {', '.join(failed_presets)}" if failed_presets else ""))
  if not a.keep:
    shutil.rmtree(base_dir, ignore_errors=True)
  return 0 if not failed_presets else 1


def run_preset(pool, a, preset, values, seeds, out_dir):
  """Generates and checks the seeds at one preset's options; returns the number of failing seeds."""
  ours_dir = os.path.join(out_dir, "ours")
  os.makedirs(ours_dir, exist_ok=True)
  cmd = [a.cli, "--out", ours_dir] + ([] if a.hints else ["--allow-missing-hints"])
  cmd += ["--sw-version", CLI_VERSION, "--time"]
  cmd += [f"{k}={json.dumps(v)}" for k, v in values.items()]
  cmd += seeds
  p = subprocess.run(cmd, capture_output=True, text=True)
  if p.returncode not in (0, 1, 3):
    sys.stderr.write(p.stderr)
    oc.die(f"cos_rando_cli exited {p.returncode} (preset {preset})")
  results = {}
  for line in p.stdout.splitlines():
    if line.startswith("ok "):
      parts = line.split(" ", 3)
      results[parts[1]] = (parts[3], None)
    elif line.startswith("FAIL "):
      seed, msg = line[5:].split(": ", 1)
      results[seed] = (None, msg)
  times = [float(m.group(1)) for m in re.finditer(r"^time \S+ ([0-9.]+) s$", p.stderr, re.M)]

  failures = 0
  jobs = []
  for s in seeds:
    if s not in results:
      print(f"FAIL {preset} {s}: no result from cos_rando_cli")
      failures += 1
      continue
    path, err = results[s]
    if path is None and (not a.golden or err.startswith("Not supported yet")):
      print(f"FAIL {preset} {s}: cos_rando_cli: {err}")
      failures += 1
      continue
    jobs.append((s, values, path, err, out_dir, a.golden, a.hints))

  for seed, problems in pool.imap_unordered(check_one, jobs):
    for pr in problems:
      print(f"FAIL {preset} {seed}: {pr}")
    failures += bool(problems)

  generated = sum(1 for s in seeds if s in results and results[s][0] is not None)
  timing = f", generation mean {1000 * sum(times) / len(times):.1f} ms max {1000 * max(times):.1f} ms" if times else ""
  print(f"check_seeds: preset {preset}: {len(seeds)} seeds, {generated} generated, "
        f"{len(seeds) - failures} passed{' (golden)' if a.golden else ''}, {failures} failed{timing}",
        flush=True)
  return failures


def sanitize(seed):
  # WWRandomizer.sanitize_seed.
  allowed = set("-_'%." + "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
  return "".join(c for c in seed.strip() if c in allowed)[:42]


if __name__ == "__main__":
  sys.exit(main())
