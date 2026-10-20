#!/usr/bin/env python3
"""gen_rando_data.py - convert wwrando's logic data into checked-in C++ tables (plan step R0.3, D11).

    python3 -I native/tools/rando/gen_rando_data.py            # (re)write the tables
    python3 -I native/tools/rando/gen_rando_data.py --check    # exit 1 if a table would change

Reads wwrando 1.10.0_9775811 (ref/wwrando, MIT) and writes, under
native/src/pc/features/rando/generated/:

  rando_items.inc      data/item_names.txt (id <-> name), the 6 lists of logic/item_types.py in file
                       order, the chart item names, the cleaned logic item names, and wwr_ui/inventory.py
                       (REGULAR_ITEMS, PROGRESSIVE_ITEMS, DEFAULT_STARTING_ITEMS)
  rando_locations.inc  logic/item_locations.txt: the 320 active locations in file order (name, Need
                       string, original item, Types bitmask) and their Paths, classified with the
                       regexes of randomizers/items.py:change_item
  rando_macros.inc     logic/macros.txt: the 307 macros in file order, bodies as strings
  rando_tables.inc     island names, the default chart map (randomizers/charts.py),
                       RNG_CHANGING_OPTIONS (randomizer.py), the boss constants (boss_reqs.py),
                       Logic.DUNGEON_NAMES, seedgen word lists, the hint data files and
                       ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES (randomizers/entrances.py)

YAML files are loaded with ruamel's safe loader, exactly as wwrando loads them, so folding of
multi-line values and comment stripping match. Python sources are read with `ast` (literals only) or
run in isolation (item_types.py and inventory.py are plain list arithmetic), never imported as
wwrando modules. The build never runs this script: the output is checked in, and running it again
must give no diff (R0.3's verification).

Needs the oracle venv (native/tools/rando/setup_oracle.sh) for ruamel.yaml; it re-executes itself in
build/rando-venv when the running Python lacks it.
"""
import ast
import hashlib
import os
import re
import runpy
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

TOOLS_DIR = oc.TOOLS_DIR
REPO_ROOT = oc.REPO_ROOT
OUT_DIR = os.path.join(REPO_ROOT, "native", "src", "pc", "features", "rando", "generated")


def ww(*parts):
  return os.path.join(oc.WWRANDO_DIR, *parts)


SOURCES = [
  "logic/item_locations.txt",
  "logic/macros.txt",
  "logic/item_types.py",
  "logic/logic.py",
  "data/item_names.txt",
  "data/island_names.txt",
  "data/progress_item_hints.txt",
  "data/zone_name_hints.txt",
  "data/location_hints.txt",
  "randomizers/charts.py",
  "randomizers/boss_reqs.py",
  "randomizers/entrances.py",
  "randomizer.py",
  "wwr_ui/inventory.py",
  "seedgen/adjectives.txt",
  "seedgen/nouns.txt",
  "seedgen/names.txt",
]


# ---------------------------------------------------------------------------------------------
# Loading

def yaml_load(rel):
  from ruamel.yaml import YAML
  yaml = YAML(typ="safe")
  with open(ww(rel)) as f:
    return yaml.load(f)


def ast_module(rel):
  with open(ww(rel)) as f:
    return ast.parse(f.read(), filename=rel)


def ast_assignment(tree, name, attr=False):
  """The value node of the first `name = ...` (or `self.name = ...` when attr) anywhere in tree."""
  for node in ast.walk(tree):
    targets = []
    if isinstance(node, ast.Assign):
      targets = node.targets
    elif isinstance(node, ast.AnnAssign) and node.value is not None:
      targets = [node.target]
    for t in targets:
      if attr and isinstance(t, ast.Attribute) and t.attr == name:
        return node.value
      if not attr and isinstance(t, ast.Name) and t.id == name:
        return node.value
  oc.die(f"assignment to {name} not found")


def ast_literal(rel, name, attr=False):
  return ast.literal_eval(ast_assignment(ast_module(rel), name, attr))


def load_exit_overrides():
  """ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES: {location: ZoneExit.all["<unique_name>"]} -> names."""
  node = ast_assignment(ast_module("randomizers/entrances.py"), "ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES")
  assert isinstance(node, ast.Dict)
  out = []
  for k, v in zip(node.keys, node.values):
    loc = ast.literal_eval(k)
    assert isinstance(v, ast.Subscript) and ast.unparse(v.value) == "ZoneExit.all", ast.unparse(v)
    out.append((loc, ast.literal_eval(v.slice)))
  return out


def load_class_dict(rel, class_name, name):
  for node in ast.walk(ast_module(rel)):
    if isinstance(node, ast.ClassDef) and node.name == class_name:
      for stmt in node.body:
        if isinstance(stmt, ast.Assign) and any(isinstance(t, ast.Name) and t.id == name for t in stmt.targets):
          return ast.literal_eval(stmt.value)
  oc.die(f"{class_name}.{name} not found in {rel}")


def load_item_names():
  # randomizer.py read_text_file_lists
  with open(ww("data/item_names.txt")) as f:
    matches = re.findall(r"^([0-9a-f]{2}) - (.+)$", f.read(), re.IGNORECASE | re.MULTILINE)
  out = []
  seen = set()
  for item_id, item_name in matches:
    if item_name:
      assert item_name not in seen, item_name
      seen.add(item_name)
      out.append((int(item_id, 16), item_name))
  return out


def load_island_names():
  # randomizer.py read_text_file_lists
  out = []
  with open(ww("data/island_names.txt")) as f:
    while True:
      room_arc_name = f.readline()
      if not room_arc_name:
        break
      island_name = f.readline().strip()
      number = int(re.search(r"Room(\d+)", room_arc_name).group(1))
      out.append((number, room_arc_name.strip(), island_name))
  return out


def load_word_list(rel):
  # seedgen.py: f.read().splitlines()
  with open(ww(rel)) as f:
    return f.read().splitlines()


PATH_KINDS = ["Chest", "ScalableObject", "Actor", "Event", "Rel", "MainDol", "CustomSymbol"]


def classify_path(path):
  """randomizers/items.py change_item's regexes -> (kind, layer, index, actor, action, file, name, offset)."""
  m = re.search(r"^(rels/[^.]+\.rel)@([0-9A-F]{4})$", path)
  if m:
    return ("Rel", -1, 0, 0, 0, m.group(1), None, int(m.group(2), 16))
  m = re.search(r"^main.dol@(8[0-9A-F]{7})$", path)
  if m:
    return ("MainDol", -1, 0, 0, 0, "main.dol", None, int(m.group(1), 16))
  m = re.search(r"^CustomSymbol:(.+)$", path)
  if m:
    return ("CustomSymbol", -1, 0, 0, 0, None, m.group(1), 0)
  m = re.search(r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/Chest([0-9A-F]{3})$", path)
  if m:
    return ("Chest", layer(m.group(2)), int(m.group(3), 16), 0, 0, m.group(1), None, 0)
  m = re.search(r"^([^/]+/[^/]+\.arc)/Event([0-9A-F]{3}):([^/]+)/Actor([0-9A-F]{3})/Action([0-9A-F]{3})$", path)
  if m:
    return ("Event", -1, int(m.group(2), 16), int(m.group(4), 16), int(m.group(5), 16), m.group(1), m.group(3), 0)
  m = re.search(r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/ScalableObject([0-9A-F]{3})$", path)
  if m:
    return ("ScalableObject", layer(m.group(2)), int(m.group(3), 16), 0, 0, m.group(1), None, 0)
  m = re.search(r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/Actor([0-9A-F]{3})$", path)
  if m:
    return ("Actor", layer(m.group(2)), int(m.group(3), 16), 0, 0, m.group(1), None, 0)
  oc.die(f"unknown path format: {path}")


def layer(s):
  return -1 if s is None else int(s, 16)


def clean_item_name(name):
  # Logic.clean_item_name
  return name.replace("(", "").replace(")", "")


# ---------------------------------------------------------------------------------------------
# C output

def c_str(s):
  if s is None:
    return "nullptr"
  out = ['"']
  for b in s.encode("utf-8"):
    c = chr(b)
    if c == "\\":
      out.append("\\\\")
    elif c == '"':
      out.append('\\"')
    elif 0x20 <= b < 0x7F and c != "?":  # '?' escaped to dodge trigraphs
      out.append(c)
    else:
      out.append("\\%03o" % b)
  out.append('"')
  return "".join(out)


def str_array(name, values):
  lines = [f"static const char* const {name}[] = {{"]
  for v in values:
    lines.append(f"    {c_str(v)},")
  lines.append("};")
  return "\n".join(lines)


def file_sha1(path):
  with open(path, "rb") as f:
    return hashlib.sha1(f.read()).hexdigest()


def wwrando_commit():
  try:
    return subprocess.run(["git", "-C", oc.WWRANDO_DIR, "rev-parse", "HEAD"], check=True,
                          capture_output=True, text=True).stdout.strip()
  except (OSError, subprocess.CalledProcessError):
    return "unknown"


def header(what, sources):
  gen = file_sha1(os.path.abspath(__file__))
  lines = [
    "// GENERATED FILE - do not edit. Regenerate with: python3 -I native/tools/rando/gen_rando_data.py",
    f"// {what}",
    f"// Source: wwrando 1.10.0_9775811 (MIT), commit {COMMIT}",
    f"// Generator: native/tools/rando/gen_rando_data.py sha1 {gen}",
  ]
  for s in sources:
    lines.append(f"//   {s} sha1 {file_sha1(ww(s))}")
  lines.append("// Included once, by pc_rando_data.cpp (inside namespace pcr::gen).")
  return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------------------------
# Tables

def gen_items():
  types = runpy.run_path(ww("logic/item_types.py"))
  inv = runpy.run_path(ww("wwr_ui/inventory.py"))
  lists = ["PROGRESS_ITEMS", "NONPROGRESS_ITEMS", "CONSUMABLE_ITEMS", "DUPLICATABLE_CONSUMABLE_ITEMS",
           "DUNGEON_PROGRESS_ITEMS", "DUNGEON_NONPROGRESS_ITEMS"]
  triforce = ["Triforce Chart %d" % i for i in range(1, 8 + 1)]
  treasure = ["Treasure Chart %d" % i for i in range(1, 41 + 1)]

  # Logic.__init__ with wwrando's default options (progression_dungeons on, both chart options off,
  # not swordless, no trap chests): all_progress + all_nonprogress + fixed + duplicatable consumables,
  # cleaned and de-duplicated in first-seen order. "Ice Trap Chest" (trap_chests) follows as the only
  # name outside the default set.
  progress = types["PROGRESS_ITEMS"] + types["DUNGEON_PROGRESS_ITEMS"]
  nonprogress = types["NONPROGRESS_ITEMS"] + triforce + treasure + types["DUNGEON_NONPROGRESS_ITEMS"]
  cleaned = []
  for n in progress + nonprogress + types["CONSUMABLE_ITEMS"] + types["DUPLICATABLE_CONSUMABLE_ITEMS"]:
    n = clean_item_name(n)
    if n not in cleaned:
      cleaned.append(n)
  num_default = len(cleaned)
  cleaned.append("Ice Trap Chest")

  out = [header("Items: item_names.txt, item_types.py, wwr_ui/inventory.py",
                ["data/item_names.txt", "logic/item_types.py", "logic/logic.py", "wwr_ui/inventory.py"])]
  out.append(f"static const char kSourceCommit[] = {c_str(COMMIT)};\n")
  out.append("static const ItemName kItemNames[] = {")
  for item_id, name in load_item_names():
    out.append(f"    {{0x{item_id:02X}, {c_str(name)}}},")
  out.append("};\n")
  for name in lists:
    cname = "k" + "".join(w.capitalize() for w in name.lower().split("_"))
    out.append(f"// item_types.py {name} (file order, duplicates kept)")
    out.append(str_array(cname, types[name]) + "\n")
  out.append(str_array("kTriforceChartNames", triforce) + "\n")
  out.append(str_array("kTreasureChartNames", treasure) + "\n")
  out.append("// Logic.all_cleaned_item_names for the default options, then \"Ice Trap Chest\".")
  out.append(f"static const size_t kNumDefaultLogicItemNames = {num_default};")
  out.append(str_array("kLogicItemNames", cleaned) + "\n")
  out.append("// wwr_ui/inventory.py (sorted as there)")
  out.append(str_array("kInventoryRegularItems", inv["REGULAR_ITEMS"]) + "\n")
  out.append(str_array("kInventoryProgressiveItems", inv["PROGRESSIVE_ITEMS"]) + "\n")
  out.append(str_array("kDefaultStartingItems", inv["DEFAULT_STARTING_ITEMS"]))
  return "\n".join(out) + "\n"


def gen_locations():
  locs = yaml_load("logic/item_locations.txt")
  tags = sorted({t.strip() for v in locs.values() for t in v["Types"].split(",")})
  assert len(tags) <= 32, tags
  out = [header("Item locations: logic/item_locations.txt", ["logic/item_locations.txt"])]
  out.append("// Every Types tag, sorted; tag i is bit i of Location::types.")
  out.append(str_array("kLocTypeNames", tags) + "\n")
  paths = []
  rows = []
  for name, v in locs.items():
    need = v["Need"]
    if need is None or not isinstance(need, str):
      oc.die(f"location {name}: Need is not a string")
    bits = 0
    for t in v["Types"].split(","):
      bits |= 1 << tags.index(t.strip())
    first = len(paths)
    for p in v["Paths"]:
      paths.append(p)
    rows.append((name, need, v["Original item"], bits, first, len(paths) - first))
  out.append("static const LocPath kLocPaths[] = {")
  for p in paths:
    kind, lay, index, actor, action, file, nm, offset = classify_path(p)
    out.append(f"    {{PathKind::{kind}, {lay}, 0x{index:03X}, 0x{actor:03X}, 0x{action:03X}, "
               f"0x{offset:08X}, {c_str(file)}, {c_str(nm)}, {c_str(p)}}},")
  out.append("};\n")
  out.append("static const Location kLocations[] = {")
  for name, need, orig, bits, first, n in rows:
    out.append(f"    {{{c_str(name)},\n     {c_str(need)},\n     {c_str(orig)}, 0x{bits:08X}u, {first}, {n}}},")
  out.append("};")
  return "\n".join(out) + "\n"


def gen_macros():
  macros = yaml_load("logic/macros.txt")
  out = [header("Macros: logic/macros.txt", ["logic/macros.txt"])]
  out.append("static const Macro kMacros[] = {")
  for name, body in macros.items():
    if not isinstance(body, str):
      oc.die(f"macro {name}: body is not a string")
    out.append(f"    {{{c_str(name)},\n     {c_str(body)}}},")
  out.append("};")
  return "\n".join(out) + "\n"


def gen_tables():
  srcs = ["data/island_names.txt", "randomizers/charts.py", "randomizer.py", "randomizers/boss_reqs.py",
          "logic/logic.py", "seedgen/adjectives.txt", "seedgen/nouns.txt", "seedgen/names.txt",
          "data/progress_item_hints.txt", "data/zone_name_hints.txt", "data/location_hints.txt",
          "randomizers/entrances.py"]
  out = [header("Misc tables", srcs)]

  out.append("// data/island_names.txt: {island number, room arc, name}")
  out.append("static const Island kIslands[] = {")
  for number, arc, name in load_island_names():
    out.append(f"    {{{number}, {c_str(arc)}, {c_str(name)}}},")
  out.append("};\n")

  charts = ast_literal("randomizers/charts.py", "island_number_to_chart_name", attr=True)
  assert sorted(charts) == list(range(1, 50)), charts
  out.append("// randomizers/charts.py ChartRandomizer.island_number_to_chart_name (dict order)")
  out.append("static const IslandChart kDefaultCharts[] = {")
  for island, chart in charts.items():
    out.append(f"    {{{island}, {c_str(chart)}}},")
  out.append("};\n")

  out.append("// randomizer.py RNG_CHANGING_OPTIONS (order matters: the RNG warm-up index)")
  out.append(str_array("kRngChangingOptions", ast_literal("randomizer.py", "RNG_CHANGING_OPTIONS")) + "\n")

  markers = ast_literal("randomizers/boss_reqs.py", "BOSS_NAME_TO_SEA_CHART_QUEST_MARKER_INDEX")
  stages = ast_literal("randomizers/boss_reqs.py", "BOSS_NAME_TO_STAGE_ID")
  assert list(markers) == list(stages)
  out.append("// randomizers/boss_reqs.py BOSS_NAME_TO_SEA_CHART_QUEST_MARKER_INDEX / BOSS_NAME_TO_STAGE_ID")
  out.append("static const Boss kBosses[] = {")
  for b in markers:
    out.append(f"    {{{c_str(b)}, {markers[b]}, {stages[b]}}},")
  out.append("};\n")

  dn = load_class_dict("logic/logic.py", "Logic", "DUNGEON_NAMES")
  out.append("// logic/logic.py Logic.DUNGEON_NAMES")
  out.append("static const DungeonName kDungeonNames[] = {")
  for short, full in dn.items():
    out.append(f"    {{{c_str(short)}, {c_str(full)}}},")
  out.append("};\n")

  out.append("// seedgen/*.txt (splitlines, as seedgen.py reads them)")
  out.append(str_array("kSeedAdjectives", load_word_list("seedgen/adjectives.txt")) + "\n")
  out.append(str_array("kSeedNouns", load_word_list("seedgen/nouns.txt")) + "\n")
  out.append(str_array("kSeedNames", load_word_list("seedgen/names.txt")) + "\n")

  for rel, cname in (("data/progress_item_hints.txt", "kProgressItemHints"),
                     ("data/zone_name_hints.txt", "kZoneNameHints")):
    d = yaml_load(rel)
    out.append(f"// {rel} (file order)")
    out.append(f"static const NameText {cname}[] = {{")
    for k, v in d.items():
      assert isinstance(k, str) and isinstance(v, str), (rel, k)
      out.append(f"    {{{c_str(k)}, {c_str(v)}}},")
    out.append("};\n")

  d = yaml_load("data/location_hints.txt")
  out.append("// data/location_hints.txt (file order): {location, Type, Text}")
  out.append("static const LocationHint kLocationHints[] = {")
  for k, v in d.items():
    assert set(v) == {"Type", "Text"}, (k, v)
    out.append(f"    {{{c_str(k)}, {c_str(v['Type'])}, {c_str(v['Text'])}}},")
  out.append("};\n")

  out.append("// randomizers/entrances.py ITEM_LOCATION_NAME_TO_EXIT_OVERRIDES: {location, ZoneExit unique_name}")
  out.append("static const NameText kExitOverrides[] = {")
  for loc, exit_name in load_exit_overrides():
    out.append(f"    {{{c_str(loc)}, {c_str(exit_name)}}},")
  out.append("};")
  return "\n".join(out) + "\n"


def main(argv):
  check = "--check" in argv
  unknown = [a for a in argv if a != "--check"]
  if unknown:
    oc.die(f"unknown arguments: {' '.join(unknown)}")
  oc.bootstrap()
  global COMMIT
  COMMIT = wwrando_commit()
  files = {
    "rando_items.inc": gen_items(),
    "rando_locations.inc": gen_locations(),
    "rando_macros.inc": gen_macros(),
    "rando_tables.inc": gen_tables(),
  }
  changed = []
  os.makedirs(OUT_DIR, exist_ok=True)
  for name, text in files.items():
    path = os.path.join(OUT_DIR, name)
    old = None
    if os.path.exists(path):
      with open(path, encoding="utf-8") as f:
        old = f.read()
    if old != text:
      changed.append(name)
      if not check:
        with open(path, "w", newline="\n", encoding="utf-8") as f:
          f.write(text)
  if check:
    if changed:
      print("out of date: " + ", ".join(changed))
      return 1
    print("up to date")
    return 0
  print(("wrote " + ", ".join(changed)) if changed else "no changes")
  return 0


COMMIT = "unknown"

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
