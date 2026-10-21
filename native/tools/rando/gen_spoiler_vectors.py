#!/usr/bin/env python3
"""Golden spoiler, non-spoiler and progression-sphere vectors for cos_rando_test_R1_5 (plan step R1.5).

    python3 -I native/tools/rando/gen_spoiler_vectors.py [out.txt]

Default output: native/check/rando/fixtures/R1_5_logs.txt. The output is deterministic, so
regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_spoiler_vectors.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R1_5_logs.txt

It runs wwrando 1.10.0_9775811 (ref/wwrando, see README.md) in dry mode with logs on for each case
and records the logs wwrando writes ("WW Random <seed> - Spoiler Log.txt" and "- Non-Spoiler
Log.txt"), plus every decision the logs are built from, so the C++ log writer can be fed the same
state:

  - the plan's 5 seeds with hints off (num_*_hints=0, hoho_hints/fishmen_hints off; the hint options
    still enter the RNG warm-up). The test runs our own item fill for these (end to end);
  - key-lunacy with hints off (our fill too);
  - "inject" cases, where the randomizers that do not exist in C++ yet (required bosses, charts,
    entrances with nesting, starting island, extra starting items) are on: the test restores the
    recorded decisions and placement into a Logic instead of running a fill;
  - a do_not_generate_spoiler_log seed made with the port's key (only the non-spoiler log exists).

Hints with hints off still have rows (Savage Labyrinth floors 30 and 50, the Big Octo Great Fairy):
they are recorded as `hint` lines (hint generation is R5's).

Tab-separated output (a log line may hold any text but tabs or newlines; each log is split on \\n,
so its last `S`/`N` line is the empty text after the final newline):

    version  <VERSION>
    case     <label> <seed>
    key      <seed key>              do_not_generate_spoiler_log seed key
    inject   1                       restore the placement instead of running the fill
    opt      <name> <value>          options that differ from Options() (R1.2 value format),
                                     dry_run included (the oracle's --dry sets it; the header shows it)
    permalink <text>
    hash     <seed hash>
    chart    <island> <chart>        charts.island_number_to_chart_name
    entrance <entrance> <exit>       entrances.entrance_connections, dict order
    nesting  <0|1>
    npath    <name> <name> ...       entrances.nested_entrance_paths
    reqdungeon / bandungeon <name>   boss_reqs.required_dungeons / banned_dungeons
    reqboss  <name>                  boss_reqs.required_bosses
    reqbossloc <location>            boss_reqs.required_boss_item_locations
    banloc   <location>              boss_reqs.banned_locations
    xstart   <item>                  extra_start_items.random_starting_items
    island   <room>                  starting_island.room_number
    prerand  <location> <item>       logic.prerandomization_item_locations, dict order
    loc      <index> <item>          logic.done_item_locations, item_locations.txt order
    hint     <place> <reward>        the rows of HintsRandomizer.write_to_spoiler_log
    S        <line>                  the spoiler log
    N        <line>                  the non-spoiler log
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
import shutil  # noqa: E402
import tempfile  # noqa: E402
import traceback  # noqa: E402

# kSwitchWakerSeedKey in pc_rando_seedgen.cpp.
SWITCHWAKER_SEED_KEY = "SwitchWaker-rando-key-1"

HINTS_OFF = {
  "num_item_hints": 0, "num_location_hints": 0, "num_barren_hints": 0, "num_path_hints": 0,
  "hoho_hints": False, "fishmen_hints": False, "korl_hints": False,
}

CASES = [
  ("seed1", "TestSeed1", dict(HINTS_OFF), None, False),
  ("seed2", "GummyAliveTrust", dict(HINTS_OFF), None, False),
  ("seed3", "R1.5-spoiler-3", dict(HINTS_OFF), None, False),
  ("seed4", "ZeldaSwitch", dict(HINTS_OFF), None, False),
  ("seed5", "Crestfallen'Pumpkin", dict(HINTS_OFF), None, False),
  ("keylunacy", "KeyLunacy5", dict(HINTS_OFF, keylunacy=True, progression_dungeon_secrets=True), None,
   False),
  ("inject_all", "InjectAll", dict(HINTS_OFF, required_bosses=True, num_required_bosses=3,
                                   randomize_charts=True, progression_treasure_charts=True,
                                   randomize_dungeon_entrances=True, randomize_boss_entrances=True,
                                   randomize_miniboss_entrances=True,
                                   randomize_secret_cave_entrances=True,
                                   randomize_secret_cave_inner_entrances=True,
                                   randomize_starting_island=True, num_extra_starting_items=2,
                                   starting_pohs=3, num_starting_triforce_shards=2), None, True),
  ("inject_bosses", "InjectBosses", dict(HINTS_OFF, required_bosses=True, num_required_bosses=1,
                                         progression_triforce_charts=True, randomize_charts=True,
                                         progression_short_sidequests=True,
                                         progression_long_sidequests=True,
                                         progression_combat_secret_caves=True,
                                         progression_minigames=True,
                                         sword_mode="Swordless"), None, True),
  ("no_spoiler", "NoSpoiler", dict(HINTS_OFF, do_not_generate_spoiler_log=True), SWITCHWAKER_SEED_KEY,
   False),
]


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  if isinstance(v, (list, tuple)):
    return "|".join(str(x) for x in v)
  return str(v)


def hint_rows(h):
  # HintsRandomizer.write_to_spoiler_log's rows.
  rows = []
  for savage_hint in [h.floor_30_hint, h.floor_50_hint]:
    valid = h.check_item_can_be_hinted_at(savage_hint.reward)
    rows.append((savage_hint.place, savage_hint.reward if valid else "Nothing"))
  all_hints = [h.octo_fairy_hint]
  for hints_for_placement in h.hints_per_placement.values():
    all_hints += hints_for_placement
  for hint in all_hints:
    rows.append((hint.place, hint.reward or "Nothing"))
  return rows


def log_lines(tag, text):
  for part in text.split("\n"):
    assert "\t" not in part, part
    yield f"{tag}\t{part}"


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("out", nargs="?", default=os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R1_5_logs.txt"))
  a = ap.parse_args()

  oc.bootstrap()
  import version
  from options.wwrando_options import Options

  lines = [f"version\t{version.VERSION}"]
  defaults = Options()
  for label, seed, values, key, inject in CASES:
    oc.set_seed_key(key if key is not None else "")
    opts = oc.make_options(values)
    logs_dir = tempfile.mkdtemp(prefix="ww_spoiler_")
    try:
      r = oc.make_randomizer(seed, opts, logs_dir=logs_dir)
      oc.randomize(r)
      spoiler_path = os.path.join(logs_dir, f"WW Random {r.seed} - Spoiler Log.txt")
      nonspoiler_path = os.path.join(logs_dir, f"WW Random {r.seed} - Non-Spoiler Log.txt")
      spoiler = None
      if os.path.exists(spoiler_path):
        with open(spoiler_path, newline="") as f:
          spoiler = f.read()
      with open(nonspoiler_path, newline="") as f:
        nonspoiler = f.read()
    except Exception:  # noqa: BLE001
      traceback.print_exc()
      print(f"gen_spoiler_vectors: case {label} failed", file=sys.stderr)
      return 1
    finally:
      shutil.rmtree(logs_dir, ignore_errors=True)
    assert (spoiler is None) == bool(opts.do_not_generate_spoiler_log), label

    lines.append(f"case\t{label}\t{r.seed}")
    if key is not None:
      lines.append(f"key\t{key}")
    if inject:
      lines.append("inject\t1")
    for f in Options.all():
      v, d = r.options[f.name], defaults[f.name]
      if v != d:
        lines.append(f"opt\t{f.name}\t{opt_value(v)}")
    lines.append(f"permalink\t{r.permalink}")
    lines.append(f"hash\t{r.seed_hash}")
    for island, chart in r.charts.island_number_to_chart_name.items():
      lines.append(f"chart\t{island}\t{chart}")
    for entrance, exit_name in r.entrances.entrance_connections.items():
      lines.append(f"entrance\t{entrance}\t{exit_name}")
    lines.append(f"nesting\t{1 if r.entrances.nesting_enabled else 0}")
    for path in r.entrances.nested_entrance_paths:
      lines.append("npath\t" + "\t".join(path))
    for n in r.boss_reqs.required_dungeons:
      lines.append(f"reqdungeon\t{n}")
    for n in r.boss_reqs.banned_dungeons:
      lines.append(f"bandungeon\t{n}")
    for n in r.boss_reqs.required_bosses:
      lines.append(f"reqboss\t{n}")
    for n in r.boss_reqs.required_boss_item_locations:
      lines.append(f"reqbossloc\t{n}")
    for n in r.boss_reqs.banned_locations:
      lines.append(f"banloc\t{n}")
    for n in r.extra_start_items.random_starting_items:
      lines.append(f"xstart\t{n}")
    lines.append(f"island\t{r.starting_island.room_number}")
    for loc, item in r.logic.prerandomization_item_locations.items():
      lines.append(f"prerand\t{loc}\t{item}")
    for i, n in enumerate(r.logic.item_locations.keys()):
      lines.append(f"loc\t{i}\t{r.logic.done_item_locations[n]}")
    for place, reward in hint_rows(r.hints):
      lines.append(f"hint\t{place}\t{reward}")
    if spoiler is not None:
      lines.extend(log_lines("S", spoiler))
    lines.extend(log_lines("N", nonspoiler))
    lines.append("endcase")
  lines.append("end")
  with open(a.out, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
