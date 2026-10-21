#!/usr/bin/env python3
"""Entrance tables dump for cos_rando_test_R6_10 (plan step R6.10).

    python3 -I native/tools/rando/gen_entrance_tables.py [out.txt]

Default output: native/check/rando/fixtures/R6_10_entrance_tables.txt. The output is deterministic,
so regenerating must give a byte-identical file:

    python3 -I native/tools/rando/gen_entrance_tables.py /tmp/x.txt && \\
        diff /tmp/x.txt native/check/rando/fixtures/R6_10_entrance_tables.txt

It imports wwrando 1.10.0_9775811's randomizers/entrances.py (ref/wwrando, see README.md) and dumps
its module-level tables and what EntranceRandomizer computes from the options before it randomizes
anything. Tab-separated output ("-" stands for None, "" for an empty list):

    version   <VERSION>
    entrance  <i> <group> <stage> <room> <scls_exit_index> <spawn_id> <entrance_name> <island_name>
              <warp_out_stage_name> <warp_out_room_num> <warp_out_spawn_id> <nested_in>
                                   ZoneEntrance.all, definition order
    exit      <i> <group> <stage> <room> <scls_exit_index> <spawn_id> <unique_name> <boss_stage_name>
              <zone_name>          ZoneExit.all, definition order
    group     <LIST_NAME> <a|b|...>  the *_ENTRANCES / *_EXITS lists (names, in order)
    conn      <entrance> <exit>    EntranceRandomizer.entrance_connections (default), dict order
    override  <location> <exit>    ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES, dict order
    list      <NAME> <a|b|...>     the *_NAMES_WITH_NO_REQUIREMENTS lists and
                                   ENTRANCE_RANDOMIZABLE_ITEM_LOCATION_TYPES
    sets      <mask> <mix_entrances>  get_all_entrance_sets_to_be_randomized for one combination:
                                   mask bits 0-5 = randomize_{dungeon, secret_cave, miniboss, boss,
                                   secret_cave_inner, fairy_fountain}_entrances
    set       <entrances a|b|...> <exits a|b|...>   one yielded set
    seterror  <text>               the generator raised
    endsets
    case      <name>               an EntranceRandomizer built by WWRandomizer.__init__
    opt       <name> <value>       options that differ from Options() (R1.2 value format)
    flag      is_enabled|nesting_enabled <0|1>
    list      entrance_names_with_no_requirements|exit_names_with_no_requirements <a|b|...>
    (first case only, they do not depend on the options:)
    locexit   <location index> <exit>        item_location_to_containing_zone_exit, dict order
    exitlocs  <exit> <i,j,...>     zone_exit_to_logically_dependent_item_locations (location
                                   indices, item_locations.txt order), dict order
    path      <entrance> <a|b|...> get_all_entrances_on_path_to_entrance with the default
                                   connections, for every entrance
    endcase
    end

Exit 0 written, 2 setup error.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import itertools  # noqa: E402
import types  # noqa: E402

ENTRANCE_GROUPS = [
  ("Dungeon", "DUNGEON_ENTRANCES", "DUNGEON_EXITS"),
  ("Miniboss", "MINIBOSS_ENTRANCES", "MINIBOSS_EXITS"),
  ("Boss", "BOSS_ENTRANCES", "BOSS_EXITS"),
  ("SecretCave", "SECRET_CAVE_ENTRANCES", "SECRET_CAVE_EXITS"),
  ("SecretCaveInner", "SECRET_CAVE_INNER_ENTRANCES", "SECRET_CAVE_INNER_EXITS"),
  ("FairyFountain", "FAIRY_FOUNTAIN_ENTRANCES", "FAIRY_FOUNTAIN_EXITS"),
]
SET_OPTIONS = [
  "randomize_dungeon_entrances",
  "randomize_secret_cave_entrances",
  "randomize_miniboss_entrances",
  "randomize_boss_entrances",
  "randomize_secret_cave_inner_entrances",
  "randomize_fairy_fountain_entrances",
]
NO_REQ_LISTS = [
  "DUNGEON_ENTRANCE_NAMES_WITH_NO_REQUIREMENTS",
  "SECRET_CAVE_ENTRANCE_NAMES_WITH_NO_REQUIREMENTS",
  "DUNGEON_EXIT_NAMES_WITH_NO_REQUIREMENTS",
  "PUZZLE_SECRET_CAVE_EXIT_NAMES_WITH_NO_REQUIREMENTS",
  "COMBAT_SECRET_CAVE_EXIT_NAMES_WITH_NO_REQUIREMENTS",
  "ENTRANCE_RANDOMIZABLE_ITEM_LOCATION_TYPES",
]


def s(v):
  if v is None:
    return "-"
  if isinstance(v, bool):
    return "1" if v else "0"
  return str(v)


def opt_value(v):
  if isinstance(v, bool):
    return "1" if v else "0"
  return str(v)


def cases():
  cats = ["progression_dungeons", "progression_puzzle_secret_caves", "progression_combat_secret_caves",
          "progression_savage_labyrinth", "progression_great_fairies"]
  out = [("Defaults", {})]
  # Every on/off combination of the categories entrances.py reads.
  # The other categories are all on, so that WWRandomizer.__init__ has enough progress locations.
  import dataclasses
  from options.wwrando_options import Options
  others = {f.name: True for f in dataclasses.fields(Options)
            if f.name.startswith("progression_") and f.name not in cats}
  for bits in itertools.product((False, True), repeat=len(cats)):
    out.append(("Cats" + "".join("1" if b else "0" for b in bits), others | dict(zip(cats, bits))))
  out.append(("Nesting", {"randomize_boss_entrances": True, "randomize_miniboss_entrances": True}))
  out.append(("InnerOnly", {"randomize_secret_cave_inner_entrances": True}))
  out.append(("AllMixed", {o: True for o in SET_OPTIONS} | {"mix_entrances": "Mix Dungeons & Caves & Fountains"}))
  return out


def main():
  out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R6_10_entrance_tables.txt")
  oc.bootstrap()
  from version import VERSION
  from options.wwrando_options import Options, EntranceMixMode
  import randomizers.entrances as en
  defaults = Options()

  lines = [f"version\t{VERSION}"]
  ent_group = {}
  exit_group = {}
  for g, el, xl in ENTRANCE_GROUPS:
    for e in getattr(en, el):
      ent_group[e.entrance_name] = g
    for x in getattr(en, xl):
      exit_group[x.unique_name] = g
  assert len(ent_group) == len(en.ZoneEntrance.all) and len(exit_group) == len(en.ZoneExit.all)

  for i, e in enumerate(en.ZoneEntrance.all.values()):
    lines.append("\t".join(["entrance", str(i), ent_group[e.entrance_name], e.stage_name, s(e.room_num),
                            s(e.scls_exit_index), s(e.spawn_id), e.entrance_name, s(e.island_name),
                            s(e.warp_out_stage_name), s(e.warp_out_room_num), s(e.warp_out_spawn_id),
                            s(e.nested_in.unique_name if e.nested_in else None)]))
  for i, x in enumerate(en.ZoneExit.all.values()):
    lines.append("\t".join(["exit", str(i), exit_group[x.unique_name], x.stage_name, s(x.room_num),
                            s(x.scls_exit_index), s(x.spawn_id), x.unique_name, s(x.boss_stage_name),
                            s(x.zone_name)]))
  for _, el, xl in ENTRANCE_GROUPS:
    lines.append(f"group\t{el}\t" + "|".join(e.entrance_name for e in getattr(en, el)))
    lines.append(f"group\t{xl}\t" + "|".join(x.unique_name for x in getattr(en, xl)))
  for loc, x in en.ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES.items():
    lines.append(f"override\t{loc}\t{x.unique_name}")
  for name in NO_REQ_LISTS:
    lines.append(f"list\t{name}\t" + "|".join(getattr(en, name)))

  # get_all_entrance_sets_to_be_randomized reads only self.options (and get_one_entrance_set).
  for mix in (EntranceMixMode.SEPARATE_DUNGEONS, EntranceMixMode.MIX_DUNGEONS):
    for mask in range(1 << len(SET_OPTIONS)):
      values = {name: bool(mask >> b & 1) for b, name in enumerate(SET_OPTIONS)}
      values["mix_entrances"] = mix
      stub = types.SimpleNamespace(options=types.SimpleNamespace(**values))
      stub.get_one_entrance_set = lambda **kw: en.EntranceRandomizer.get_one_entrance_set(stub, **kw)
      lines.append(f"sets\t{mask}\t{mix.value}")
      try:
        for ents, exits in en.EntranceRandomizer.get_all_entrance_sets_to_be_randomized(stub):
          lines.append("set\t" + "|".join(e.entrance_name for e in ents) + "\t" +
                       "|".join(x.unique_name for x in exits))
      except Exception as e:  # noqa: BLE001 - recorded for the test
        lines.append(f"seterror\t{e}")
      lines.append("endsets")

  # The default connections (the randomizer has not run yet; the same for every option set).
  r0 = oc.make_randomizer("EntranceTables", oc.make_options({}))
  for ent, ex in r0.entrances.entrance_connections.items():
    lines.append(f"conn\t{ent}\t{ex}")

  first = True
  for name, values in cases():
    opts = oc.make_options(values)
    lines.append(f"case\t{name}")
    for k in sorted(values):
      if getattr(opts, k) != getattr(defaults, k):
        lines.append(f"opt\t{k}\t{opt_value(getattr(opts, k))}")
    r = oc.make_randomizer("EntranceTables", opts)
    er = r.entrances
    lines.append(f"flag\tis_enabled\t{s(er.is_enabled())}")
    lines.append(f"flag\tnesting_enabled\t{s(er.nesting_enabled)}")
    lines.append("list\tentrance_names_with_no_requirements\t" + "|".join(er.entrance_names_with_no_requirements))
    lines.append("list\texit_names_with_no_requirements\t" + "|".join(er.exit_names_with_no_requirements))
    if not first:
      lines.append("endcase")
      continue
    # The rest does not depend on the options (Logic.item_locations holds every location): first
    # case only.
    first = False
    order = list(r.logic.item_locations.keys())
    for loc, x in er.item_location_to_containing_zone_exit.items():
      lines.append(f"locexit\t{order.index(loc)}\t{x.unique_name}")
    for x, locs in er.zone_exit_to_logically_dependent_item_locations.items():
      lines.append(f"exitlocs\t{x.unique_name}\t" + ",".join(str(order.index(l)) for l in locs))
    for e in en.ZoneEntrance.all.values():
      path = er.get_all_entrances_on_path_to_entrance(e)
      lines.append(f"path\t{e.entrance_name}\t" + "|".join(p.entrance_name for p in path))
    lines.append("endcase")
  lines.append("end")
  with open(out_path, "w") as f:
    f.write("\n".join(lines) + "\n")
  return 0


if __name__ == "__main__":
  sys.exit(main())
