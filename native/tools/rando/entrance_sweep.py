#!/usr/bin/env python3
"""Entrance sweep: check entrance maps for structure and in the running game (step R6.14).

    python3 -I native/tools/rando/entrance_sweep.py check <placement.json>... [--no-logic]
    python3 -I native/tools/rando/entrance_sweep.py selftest
    python3 -I native/tools/rando/entrance_sweep.py plan <placement.json> [--out FILE] [--disc ISO]
    python3 -I native/tools/rando/entrance_sweep.py run <placement.json>... [options]

docs/RANDOMIZER_PLAN.md, step R6.14. A placement JSON (schema format 1, ww_export.py or our
generator) holds the entrance map in "entrances" (entrance name -> exit name, wwrando's
EntranceRandomizer.entrance_connections). The tool rebuilds a dry wwrando randomizer with the JSON's
options and decisions (ww_verify.restore_state) and uses wwrando's own tables for everything.

check (the structure checker; exit 0 valid, 1 invalid):
  map       every one of the 44 entrances names a known exit and every exit has exactly one entrance;
  sets      each entrance leads to an exit of its own entrance set (get_all_entrance_sets_to_be_
            randomized per mix_entrances); entrances in no set keep their vanilla exit;
  nesting   every exit is reached from an island entrance: the chain of nested entrances
            (get_all_entrances_on_path_to_entrance) ends on the sea, with no loop and no undecided
            zone; an exit with an inner entrance is not the end of its own chain;
  safety    when the seed starts with only dungeons and caves reachable (dungeons_and_caves_only_
            start), each entrance set wwrando chose a safety entrance for has one: an entrance with
            no requirements that leads to an exit with no requirements;
  progress  a progress exit (one with progress locations, check_if_one_exit_is_progress) is never
            nested in a nonprogress one (split_nonprogress_entrances_and_exits keeps them apart);
            Forsaken Fortress's boss entrance leads to a progress exit exactly when FF is progress;
  bosses    with required bosses, no island holds both a required and a banned boss
            (finalize_all_randomized_sets_of_entrances's assertion);
  logic     ww_verify.py's checks with the map in place (all locations, pool, reachability of
            Ganondorf and every progress location); --no-logic skips it.
selftest breaks wwrando seeds in each of those ways (and generates a dungeons-and-caves-only start
seed for the safety rule) and requires check to catch every break and pass the unbroken maps.

plan writes the in-game plan (one line per entrance, format in
native/src/pc/harness/smokes/pc_rando_smoke_entrsweep.cpp): for each entrance E -> exit X, X's
stage/room/spawn and way out (SCLS index; boss arenas: the WARP_WIND_AFTER warp, Helmaroc King:
M2tower room 0 SCLS[1]), where leaving X goes (E's spawn; a boss arena: the outermost island
entrance's warp-out spawn) and where a save inside X reloads (dComIfGs_setGameStartStage:
miniboss/boss arenas -> E, or the dungeon's start (SCLS[0] of E's stage.dzs, read from the disc)
when E is a miniboss or boss entrance; dungeons -> their own stage.dzs SCLS[0]; caves, fountains and
Ice Ring Isle's inner cave -> the outermost entrance's island, sea room n spawn 0; Cliff Plateau
Isles' inner cave is on the sea and has no save check).

run checks the maps, writes their plans and runs native/tools/run.sh rando with
COS_RANDO_SMOKE=entrsweep over them, a few entrances per run and a few runs at a time; each entrance
is used forward, saved and quit inside its exit and reloaded, and used back. The report is a TSV
(default build/rando-sweep/entrance_sweep.tsv), one row per entrance and check:
    map  entrance  exit  check  run  result  detail
result is ok, FAIL or not-reached (the run ended before that entrance). Exit 0 when the structure
checks and every row are ok, 1 otherwise, 2 usage. Options:
  --per-run N    entrances per run (default 6)
  --jobs N       runs at a time (default 3)
  --only I,...   only these plan lines (0-based, the same for every map)
  --sweep-dir D  where the runs go (default build/rando-sweep/entr-<time>)
  --out FILE     the TSV
  --disc PATH    COS_DISC (default $COS_DISC or ~/Documents/windwaker/GZLE01.iso)
  --dry          write the plans and print the runs, do not start them

Mac only, dev time; uses the oracle venv (setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import argparse  # noqa: E402
import concurrent.futures  # noqa: E402
import contextlib  # noqa: E402
import copy  # noqa: E402
import re  # noqa: E402
import subprocess  # noqa: E402
import time  # noqa: E402

import randomizers.entrances as en  # noqa: E402
import ww_verify  # noqa: E402

TOOLS = os.path.dirname(oc.TOOLS_DIR)
REPO = oc.REPO_ROOT
RUN_SH = os.path.join(TOOLS, "run.sh")
DEFAULT_DISC = os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso")
TSV_COLUMNS = ["map", "entrance", "exit", "check", "run", "result", "detail"]


# ---------------------------------------------------------------------------------------------
# The map in a dry randomizer

class Map:
  def __init__(self, path, d=None):
    self.path = path
    self.d = d if d is not None else oc.read_json(path)
    self.name = os.path.splitext(os.path.basename(path))[0]
    self.opts = oc.make_options(self.d["options"], strict=False)
    self.r = oc.make_randomizer(self.d.get("seed") or "x", self.opts)
    with contextlib.redirect_stdout(sys.stderr):
      ww_verify.restore_state(self.r, self.d, self.opts)
    self.er = self.r.entrances
    self.errors = []
    self.conns = dict(self.er.entrance_connections)
    self.er.done_entrances_to_exits.clear()
    self.er.done_exits_to_entrances.clear()
    for e_name, x_name in self.conns.items():
      ze = en.ZoneEntrance.all.get(e_name)
      zx = en.ZoneExit.all.get(x_name)
      if ze is None or zx is None:
        self.errors.append(f"map: unknown entrance or exit {e_name!r} -> {x_name!r}")
        continue
      self.er.done_entrances_to_exits[ze] = zx
      self.er.done_exits_to_entrances[zx] = ze

  def exit_of(self, ze):
    return self.er.done_entrances_to_exits.get(ze)

  def entrance_of(self, zx):
    return self.er.done_exits_to_entrances.get(zx)

  def path_to(self, ze):
    """get_all_entrances_on_path_to_entrance, or (None, error)."""
    try:
      p = self.er.get_all_entrances_on_path_to_entrance(ze)
    except Exception as e:  # noqa: BLE001 - wwrando raises a bare Exception for loops
      return None, str(e)
    if p is None:
      return None, "undecided"
    return p, None

  def outermost_for_exit(self, zx):
    p, _ = self.path_to(self.entrance_of(zx))
    return p[-1] if p else None


# ---------------------------------------------------------------------------------------------
# The structure checker

def entrance_sets(m):
  if not m.er.is_enabled():
    return []
  try:
    return [(list(a), list(b)) for a, b in m.er.get_all_entrance_sets_to_be_randomized()]
  except Exception as e:  # noqa: BLE001
    m.errors.append(f"sets: {e}")
    return []


def doing_progress_for_dacos(m, exits):
  """randomize_one_set_of_entrances's doing_progress_entrances_for_dungeons_and_caves_only_start."""
  o = m.opts
  if not m.r.dungeons_and_caves_only_start:
    return False
  doing_dungeons = any(x in en.DUNGEON_EXITS for x in exits)
  doing_caves = any(x in en.SECRET_CAVE_EXITS for x in exits)
  if doing_dungeons and o.progression_dungeons:
    return True
  if doing_caves and (o.progression_puzzle_secret_caves or o.progression_combat_secret_caves
                      or o.progression_savage_labyrinth):
    return True
  return False


def check_structure(m, logic=True):
  """The rule names that failed, with details, in m.errors (and returned)."""
  errs = m.errors
  all_e = list(en.ZoneEntrance.all.values())
  all_x = list(en.ZoneExit.all.values())

  # map
  if len(m.conns) != len(all_e):
    errs.append(f"map: {len(m.conns)} entrances, not {len(all_e)}")
  for ze in all_e:
    if m.exit_of(ze) is None:
      errs.append(f"map: {ze.entrance_name} has no exit")
  for zx in all_x:
    users = [e for e, x in m.conns.items() if x == zx.unique_name]
    if len(users) != 1:
      errs.append(f"map: exit {zx.unique_name} has {len(users)} entrances: {users}")
  if errs:
    return errs

  # sets
  sets = entrance_sets(m)
  in_set = {}
  for k, (es, xs) in enumerate(sets):
    for ze in es:
      in_set[ze] = k
    for ze in es:
      if m.exit_of(ze) not in xs:
        errs.append(f"sets: {ze.entrance_name} -> {m.exit_of(ze).unique_name}, not an exit of its set {k}")
  for ze in all_e:
    if ze not in in_set:
      want = en.ZoneExit.all[DEFAULT_CONNECTIONS[ze.entrance_name]]
      if m.exit_of(ze) != want:
        errs.append(f"sets: {ze.entrance_name} is not randomized but leads to {m.exit_of(ze).unique_name}, "
                    f"not {want.unique_name}")

  # nesting
  for zx in all_x:
    ze = m.entrance_of(zx)
    p, err = m.path_to(ze)
    if p is None:
      errs.append(f"nesting: {zx.unique_name}: {err}")
      continue
    if p[-1].island_name is None:
      errs.append(f"nesting: {zx.unique_name}'s chain does not end on an island")
    chain_exits = [x.nested_in for x in p if x.nested_in is not None]
    if zx in chain_exits:
      errs.append(f"nesting: {zx.unique_name} is nested in itself")

  # safety
  for k, (es, xs) in enumerate(sets):
    if not doing_progress_for_dacos(m, xs):
      continue
    ok_e = set(m.er.entrance_names_with_no_requirements)
    ok_x = set(m.er.exit_names_with_no_requirements)
    safe = [ze.entrance_name for ze in es if ze.entrance_name in ok_e and m.exit_of(ze).unique_name in ok_x]
    if not safe:
      errs.append(f"safety: set {k}: no entrance with no requirements leads to an exit with no requirements "
                  f"(dungeons_and_caves_only_start)")

  # progress
  progress = {}
  for zx in all_x:
    if zx in m.er.zone_exit_to_logically_dependent_item_locations:
      progress[zx] = m.er.check_if_one_exit_is_progress(zx)
  for zx, prog in progress.items():
    if not prog:
      continue
    ze = m.entrance_of(zx)
    seen = set()
    while ze is not None and ze.nested_in is not None and ze not in seen:
      seen.add(ze)
      outer = ze.nested_in
      if outer in progress and not progress[outer]:
        errs.append(f"progress: progress exit {zx.unique_name} is nested in nonprogress {outer.unique_name}")
        break
      ze = m.entrance_of(outer)
  ff = en.ZoneEntrance.all["Boss Entrance in Forsaken Fortress"]
  if ff in in_set:
    ff_progress = bool(m.opts.progression_dungeons) and "Forsaken Fortress" not in m.r.boss_reqs.banned_dungeons
    zx = m.exit_of(ff)
    if zx in progress and progress[zx] != ff_progress:
      errs.append(f"progress: Forsaken Fortress's boss entrance leads to {zx.unique_name} "
                  f"({'progress' if progress[zx] else 'nonprogress'}), FF is {'progress' if ff_progress else 'nonprogress'}")

  # bosses
  if m.opts.required_bosses:
    br = m.r.boss_reqs
    try:
      banned = {m.er.get_entrance_zone_for_boss(b) for b in br.banned_bosses}
      required = {m.er.get_entrance_zone_for_boss(b) for b in br.required_bosses}
      if banned & required:
        errs.append(f"bosses: islands with a required and a banned boss: {sorted(banned & required)}")
    except Exception as e:  # noqa: BLE001
      errs.append(f"bosses: {e}")

  # logic
  if logic:
    with contextlib.redirect_stdout(sys.stderr):
      res = ww_verify.verify(m.d)
    for e in res.get("errors", []):
      errs.append(f"logic: {e}")
  return errs


DEFAULT_CONNECTIONS = None


def default_connections():
  global DEFAULT_CONNECTIONS
  if DEFAULT_CONNECTIONS is None:
    r = oc.make_randomizer("x", oc.make_options({}))
    DEFAULT_CONNECTIONS = dict(r.entrances.entrance_connections)
  return DEFAULT_CONNECTIONS


def cmd_check(args):
  default_connections()
  bad = 0
  for p in args.placements:
    m = Map(p)
    errs = check_structure(m, logic=not args.no_logic)
    if errs:
      bad += 1
      print(f"entrance-sweep: {p}: INVALID")
      for e in errs:
        print("  " + e)
    else:
      print(f"entrance-sweep: {p}: ok ({'entrance rando on' if m.er.is_enabled() else 'entrance rando off'}"
            f", {len(entrance_sets(m))} set(s), dungeons_and_caves_only_start {m.r.dungeons_and_caves_only_start})")
  return 1 if bad else 0


# ---------------------------------------------------------------------------------------------
# selftest

def broken(path, mutate):
  d = copy.deepcopy(oc.read_json(path))
  mutate(d)
  return d


def swap(d, a, b):
  c = d["entrances"]
  c[a], c[b] = c[b], c[a]


def cmd_selftest(args):
  default_connections()
  fx = os.path.join(REPO, "native", "check", "rando", "fixtures")
  A = os.path.join(fx, "R6_12_seed_A.json")   # separate sets
  B = os.path.join(fx, "R6_12_seed_B.json")   # mixed
  failures = []

  def expect(label, d, rule, logic=False):
    m = Map(label, d)
    errs = check_structure(m, logic=logic)
    if rule is None:
      ok = not errs
    else:
      ok = any(e.startswith(rule + ":") for e in errs)
    print(f"entrance-sweep: selftest {label}: {'ok' if ok else 'FAIL'}"
          + ("" if rule is None else f" (wants {rule})") + (f": {errs[:3]}" if errs and (rule is None or not ok) else ""))
    if not ok:
      failures.append(label)
    return m

  expect("A unbroken", oc.read_json(A), None, logic=True)
  expect("B unbroken", oc.read_json(B), None, logic=True)

  # map: two entrances to one exit
  def dup(d):
    c = d["entrances"]
    first, second = list(c)[0], list(c)[1]
    c[second] = c[first]
  expect("A two entrances, one exit", broken(A, dup), "map")

  # sets: a dungeon entrance swapped with a cave entrance in separate mode
  def cross(d):
    swap(d, "Dungeon Entrance on Gale Isle", "Secret Cave Entrance on Star Island")
  expect("A cross-set swap", broken(A, cross), "sets")

  # sets: an entrance not randomized but moved (map D: only secret caves randomized)
  D = os.path.join(fx, "R6_12_seed_D.json")
  expect("D unbroken", oc.read_json(D), None)
  expect("D dungeon moved", broken(D, lambda d: swap(d, "Dungeon Entrance on Gale Isle",
                                                       "Dungeon Entrance on Headstone Island")), "sets")

  # nesting: an inner entrance of an exit leads into that exit's own chain
  def loop(d):
    c = d["entrances"]
    # the exit holding Wind Temple's boss entrance, and whatever that boss entrance leads to
    inner = "Boss Entrance in Wind Temple"
    outer_exit = "Wind Temple"
    holder = next(e for e, x in c.items() if x == outer_exit)
    c[inner], c[holder] = c[holder], c[inner]
  expect("A nesting loop", broken(A, loop), "nesting")

  # progress: a progress exit moved behind a nonprogress one. Map A has every exit progress, so a
  # seed with nonprogress dungeons (and progress secret caves, mixed with them) is generated.
  dp = generated("R6.14 selftest progress", {
    "randomize_dungeon_entrances": True, "randomize_miniboss_entrances": True, "randomize_boss_entrances": True,
    "randomize_secret_cave_entrances": True, "randomize_secret_cave_inner_entrances": True,
    "progression_dungeons": False, "mix_entrances": "Mix Dungeons & Caves & Fountains"})
  mp = expect("nonprogress dungeons unbroken", dp, None, logic=True)
  prog = {zx.unique_name: mp.er.check_if_one_exit_is_progress(zx) for zx in en.ZoneExit.all.values()
          if zx in mp.er.zone_exit_to_logically_dependent_item_locations}
  hosts = [x for x in prog if not prog[x] and any(
    e.nested_in is not None and e.nested_in.unique_name == x for e in en.ZoneEntrance.all.values())]
  targets = [x for x in prog if prog[x] and not any(
    e.nested_in is not None and e.nested_in.unique_name == x for e in en.ZoneEntrance.all.values())]
  if hosts and targets:
    host, target = hosts[0], targets[0]
    def bury(d):
      c = d["entrances"]
      inner = next(e.entrance_name for e in en.ZoneEntrance.all.values()
                   if e.nested_in is not None and e.nested_in.unique_name == host)
      holder = next(e for e, x in c.items() if x == target)
      c[inner], c[holder] = c[holder], c[inner]
    expect(f"progress {target} behind nonprogress {host}", broken_d(dp, bury), "progress")
  else:
    print(f"entrance-sweep: selftest progress: no nonprogress host / progress target ({hosts}, {targets}): FAIL")
    failures.append("progress seed")

  # safety: a dungeons-and-caves-only start
  dacos_opts = {
    "progression_dungeons": True, "progression_puzzle_secret_caves": True, "progression_combat_secret_caves": True,
    "progression_savage_labyrinth": True, "progression_great_fairies": False, "progression_free_gifts": False,
    "progression_tingle_chests": False, "progression_misc": False, "progression_short_sidequests": False,
    "progression_long_sidequests": False, "progression_spoils_trading": False, "progression_minigames": False,
    "progression_battlesquid": False, "progression_expensive_purchases": False, "progression_island_puzzles": False,
    "progression_platforms_rafts": False, "progression_submarines": False, "progression_eye_reef_chests": False,
    "progression_big_octos_gunboats": False, "progression_treasure_charts": False,
    "progression_triforce_charts": False, "progression_mail": False,
    "randomize_dungeon_entrances": True, "randomize_secret_cave_entrances": True,
  }
  d = generated("R6.14 selftest safety", dacos_opts)
  m = expect("dungeons-and-caves-only start unbroken", d, None, logic=True)
  if not m.r.dungeons_and_caves_only_start:
    print("entrance-sweep: selftest safety: the generated seed is not a dungeons_and_caves_only_start seed: FAIL")
    failures.append("safety seed")
  else:
    ok_e = list(m.er.entrance_names_with_no_requirements)
    ok_x = list(m.er.exit_names_with_no_requirements)
    def unsafe(dd):
      c = dd["entrances"]
      # every no-requirement entrance of each set gets a non-no-requirement exit of the same group
      for e_name in ok_e:
        if c[e_name] in ok_x:
          ze = en.ZoneEntrance.all[e_name]
          grp = en.DUNGEON_EXITS if ze in en.DUNGEON_ENTRANCES else en.SECRET_CAVE_EXITS
          other = next(o for o, x in c.items() if x not in ok_x and en.ZoneExit.all[x] in grp
                       and en.ZoneEntrance.all[o].island_name is not None and o not in ok_e)
          c[e_name], c[other] = c[other], c[e_name]
    expect("dungeons-and-caves-only start, no safe entrance", broken_d(d, unsafe), "safety")

  # bosses: a required and a banned boss on one island. Tower of the Gods Sector has two entrances of
  # the dungeon set (Tower of the Gods' and Hyrule's miniboss entrance): the banned boss's chain is
  # moved to the first and the required boss's chain to the second.
  dr = generated("R6.14 selftest bosses", {
    "required_bosses": True, "num_required_bosses": 3, "randomize_dungeon_entrances": True,
    "randomize_miniboss_entrances": True, "randomize_boss_entrances": True,
    "mix_entrances": "Separate Dungeons From Caves & Fountains"})
  mr = expect("required bosses unbroken", dr, None, logic=True)
  br = mr.r.boss_reqs
  totg = "Dungeon Entrance in Tower of the Gods Sector"
  hyrule = "Miniboss Entrance in Hyrule Castle"
  def outer_name(dd, arena):
    return Map("tmp", dd).outermost_for_exit(en.ZoneExit.all[arena]).entrance_name
  def clash(dd):
    ban = br.banned_bosses[0] + " Boss Arena"
    req = br.required_bosses[0] + " Boss Arena"
    ob = outer_name(dd, ban)
    if ob != totg:
      swap(dd, ob, totg)
    orq = outer_name(dd, req)
    if orq != hyrule:
      swap(dd, orq, hyrule)
  dd = broken_d(dr, clash)
  if outer_name(dd, br.banned_bosses[0] + " Boss Arena") == totg and outer_name(dd, br.required_bosses[0] + " Boss Arena") == hyrule:
    expect("required bosses, a banned and a required boss on Tower of the Gods Sector", dd, "bosses")
  else:
    print("entrance-sweep: selftest bosses: the swap did not put both chains on one island: FAIL")
    failures.append("bosses swap")

  print(f"entrance-sweep: selftest: {'PASS' if not failures else 'FAIL ' + ', '.join(failures)}")
  return 1 if failures else 0


def broken_d(d, mutate):
  dd = copy.deepcopy(d)
  mutate(dd)
  return dd


def generated(seed, values):
  opts = oc.make_options(values)
  r = oc.make_randomizer(seed, opts)
  oc.randomize(r)
  return oc.placement_from_randomizer(r, opts)


# ---------------------------------------------------------------------------------------------
# The plan

class Disc:
  def __init__(self, iso):
    from gclib.gcm import GCM
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    self.cache = {}

  def stage_scls0(self, stage):
    if stage not in self.cache:
      from gclib.rarc import RARC
      from wwlib.dzx import DZx, SCLS
      arc = RARC(self.gcm.read_file_data("files/res/Stage/%s/Stage.arc" % stage))
      s = arc.get_file("stage.dzs", DZx).entries_by_type(SCLS)[0]
      # room_index is a u8; 255 is the s8 -1 the game's return place holds (the spawn's own room)
      room = s.room_index - 256 if s.room_index >= 128 else s.room_index
      self.cache[stage] = (s.dest_stage_name, room, s.spawn_id)
    return self.cache[stage]


def plan_lines(m, disc):
  lines = [f"# {m.path}: seed {m.d.get('seed')!r}; entrance_sweep.py plan (wwrando tables, the disc's SCLS[0])"]
  arenas = en.MINIBOSS_EXITS + en.BOSS_EXITS
  inner_ents = en.MINIBOSS_ENTRANCES + en.BOSS_ENTRANCES
  cave_like = en.SECRET_CAVE_EXITS + en.FAIRY_FOUNTAIN_EXITS + [en.ZoneExit.all["Ice Ring Isle Inner Cave"]]
  for ze in en.ZoneEntrance.all.values():
    zx = m.exit_of(ze)
    o = m.outermost_for_exit(zx)
    E = f"E {ze.stage_name} {ze.room_num} {ze.scls_exit_index} {ze.spawn_id}"
    xs = -1 if zx.scls_exit_index is None else zx.scls_exit_index
    X = f"X {zx.stage_name} {zx.room_num} {xs} {zx.spawn_id}"
    if zx in en.BOSS_EXITS:
      kind = "helmaroc" if zx.unique_name == "Helmaroc King Boss Arena" else "warp"
      B = f"B {kind} {o.warp_out_stage_name} {o.warp_out_room_num} {o.warp_out_spawn_id}"
    else:
      B = f"B scls {ze.stage_name} {ze.room_num} {ze.spawn_id}"
    if zx in arenas:
      if ze in inner_ents:
        s = disc.stage_scls0(ze.stage_name)
      else:
        s = (ze.stage_name, ze.room_num, ze.spawn_id)
      S = f"S {s[0]} {s[1]} {s[2]}"
    elif zx in en.DUNGEON_EXITS:
      s = disc.stage_scls0(zx.stage_name)
      S = f"S {s[0]} {s[1]} {s[2]}"
    elif zx in cave_like:
      S = f"S sea {m.r.island_name_to_number[o.island_name]} 0"
    else:
      S = "S -"
    lines.append("|".join([ze.entrance_name, zx.unique_name, E, X, B, S]))
  return lines


def write_plan(m, disc, out):
  lines = plan_lines(m, disc)
  os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
  with open(out, "w", newline="\n") as f:
    f.write("\n".join(lines) + "\n")
  return [l for l in lines if not l.startswith("#")]


def cmd_plan(args):
  default_connections()
  m = Map(args.placement)
  if m.errors:
    oc.die("; ".join(m.errors), 1)
  disc = Disc(args.disc)
  out = args.out or "-"
  lines = plan_lines(m, disc)
  if out == "-":
    sys.stdout.write("\n".join(lines) + "\n")
  else:
    write_plan(m, disc, out)
  return 0


# ---------------------------------------------------------------------------------------------
# run

_P = r"^\[cos\] rando-smoke entrsweep: "
RE_CHECK = re.compile(_P + r"(\d+) (.+?): (forward|back[^:]*|save|quit[^:]*|to the entrance|to the exit): (.*): (ok|FAIL)$")
RE_DONE = re.compile(_P + r"entrance (\d+) (.+): (ok|FAIL)$")


def read_text(p):
  try:
    with open(p, errors="replace") as f:
      return f.read()
  except OSError:
    return ""


def run_one(job, args):
  run_dir = job["dir"]
  first = job["first"]
  cmd = [RUN_SH, "rando", "--stage", first, "--timeout", str(job["timeout"]), "--stall", "120",
         "--env", "COS_RANDO_SMOKE=entrsweep", "--env", "COS_RANDO_PLACEMENT=" + job["placement"],
         "--env", "COS_ENTRSWEEP_PLAN=" + job["plan"], "--env", "COS_ENTRSWEEP_ONLY=" + ",".join(map(str, job["only"])),
         "--disc", args.disc, "--quiet", "--run-dir", run_dir]
  t0 = time.time()
  subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=REPO)
  job["seconds"] = time.time() - t0
  try:
    job["rc"] = int(read_text(os.path.join(run_dir, "exit_code.txt")).strip())
  except ValueError:
    job["rc"] = -1
  job["log"] = read_text(os.path.join(run_dir, "run.log"))
  return job


def parse_job(job, plan):
  rows = []
  done = set()
  seen = set()
  for line in job["log"].splitlines():
    mc = RE_CHECK.match(line)
    if mc:
      k = int(mc.group(1))
      if k >= len(job["only"]):
        continue
      i = job["only"][k]
      seen.add(i)
      rows.append((i, mc.group(3), mc.group(4), mc.group(5)))
      continue
    md = RE_DONE.match(line)
    if md:
      k = int(md.group(1))
      if k < len(job["only"]):
        done.add(job["only"][k])
  for i in job["only"]:
    if i not in done:
      tail = ""
      bt = read_text(os.path.join(job["dir"], "backtrace.txt")).strip().splitlines()
      if bt:
        tail = bt[0][:160]
      rows.append((i, "entrance", f"not finished (run exit {job['rc']}){': ' + tail if tail else ''}", "not-reached"))
  return rows


def cmd_run(args):
  default_connections()
  stamp = time.strftime("%Y%m%d-%H%M%S")
  sweep_dir = os.path.abspath(args.sweep_dir or os.path.join(REPO, "build", "rando-sweep", "entr-" + stamp))
  os.makedirs(sweep_dir, exist_ok=True)
  out = args.out or os.path.join(REPO, "build", "rando-sweep", "entrance_sweep.tsv")
  disc = Disc(args.disc)
  report = []
  structure_bad = 0
  jobs = []
  plans = {}
  for p in args.placements:
    m = Map(p)
    errs = check_structure(m, logic=not args.no_logic)
    for e in errs:
      report.append([m.name, "-", "-", "structure", "-", "FAIL", e])
    if errs:
      structure_bad += 1
      print(f"entrance-sweep: {m.name}: structure INVALID: {errs[:3]}", flush=True)
      continue
    report.append([m.name, "-", "-", "structure", "-", "ok", ""])
    print(f"entrance-sweep: {m.name}: structure ok", flush=True)
    if not m.er.is_enabled():
      # The plan states what wwrando's _save writes; with entrance rando off nothing is written and the
      # disc differs from wwrando's tables (a few SCLS rooms 255, the caves' save sectors).
      report.append([m.name, "-", "-", "plan", "-", "not-swept", "entrance rando is off in this seed"])
      print(f"entrance-sweep: {m.name}: entrance rando is off: not swept", flush=True)
      continue
    plan_path = os.path.join(sweep_dir, m.name + ".plan")
    lines = write_plan(m, disc, plan_path)
    plans[m.name] = lines
    idx = list(range(len(lines)))
    if args.only:
      idx = [int(x) for x in args.only.split(",")]
    for b in range(0, len(idx), args.per_run):
      only = idx[b:b + args.per_run]
      first = lines[only[0]].split("|")[2].split()
      jobs.append({
        "map": m.name, "placement": os.path.abspath(p), "plan": plan_path, "only": only,
        "first": f"{first[1]}:{first[2]}:{first[4]}", "dir": os.path.join(sweep_dir, f"{m.name}-{b // args.per_run:02d}"),
        "name": f"{m.name}-{b // args.per_run:02d}", "timeout": 120 + 150 * len(only),
      })
  if args.dry:
    for j in jobs:
      print(f"entrance-sweep: {j['name']}: --stage {j['first']} ONLY={','.join(map(str, j['only']))}")
    return 0
  with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
    futs = [ex.submit(run_one, j, args) for j in jobs]
    for f in concurrent.futures.as_completed(futs):
      j = f.result()
      rows = parse_job(j, plans[j["map"]])
      bad = sum(1 for r in rows if r[3] != "ok")
      print(f"entrance-sweep: {j['name']}: {len(j['only'])} entrance(s), exit {j['rc']}, {j['seconds']:.0f} s, "
            f"{len(rows) - bad}/{len(rows)} check(s) ok", flush=True)
      for i, check, detail, result in rows:
        f6 = plans[j["map"]][i].split("|")
        report.append([j["map"], f6[0], f6[1], check, j["name"], result, detail])
  os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
  with open(out, "w", newline="\n") as f:
    f.write("\t".join(TSV_COLUMNS) + "\n")
    for r in report:
      f.write("\t".join(str(x) for x in r) + "\n")
  fails = [r for r in report if r[5] != "ok"]
  for r in fails:
    print("entrance-sweep: %s %s -> %s: %s: %s (%s, %s)" % (r[0], r[1], r[2], r[3], r[6], r[5], r[4]))
  per_map = {}
  for r in report:
    if r[3] == "structure":
      continue
    per_map.setdefault(r[0], set()).add(r[1])
  for k, v in per_map.items():
    print(f"entrance-sweep: {k}: {len(v)} entrance(s) swept")
  print(f"entrance-sweep: {len(report)} row(s), {len(fails)} failure(s); report {out}; runs {sweep_dir}")
  return 1 if fails or structure_bad else 0


def main(argv):
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  sub = ap.add_subparsers(dest="cmd", required=True)
  c = sub.add_parser("check")
  c.add_argument("placements", nargs="+")
  c.add_argument("--no-logic", action="store_true")
  sub.add_parser("selftest")
  p = sub.add_parser("plan")
  p.add_argument("placement")
  p.add_argument("--out")
  p.add_argument("--disc", default=DEFAULT_DISC)
  r = sub.add_parser("run")
  r.add_argument("placements", nargs="+")
  r.add_argument("--per-run", type=int, default=6)
  r.add_argument("--jobs", type=int, default=3)
  r.add_argument("--only")
  r.add_argument("--sweep-dir")
  r.add_argument("--out")
  r.add_argument("--disc", default=DEFAULT_DISC)
  r.add_argument("--dry", action="store_true")
  r.add_argument("--no-logic", action="store_true")
  args = ap.parse_args(argv)
  return {"check": cmd_check, "selftest": cmd_selftest, "plan": cmd_plan, "run": cmd_run}[args.cmd](args)


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
