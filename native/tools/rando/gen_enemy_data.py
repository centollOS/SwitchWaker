#!/usr/bin/env python3
"""gen_enemy_data.py - convert wwrando's enemy randomizer data into a checked-in C++ table (plan step R7.3a).

    python3 -I native/tools/rando/gen_enemy_data.py            # (re)write the table
    python3 -I native/tools/rando/gen_enemy_data.py --check    # exit 1 if the table would change

Reads wwrando 1.10.0_9775811 (ref/wwrando, MIT) and writes
native/src/pc/features/rando/generated/rando_enemies.inc:

  data/enemy_types.txt        every enemy type in file order with every field, plus the compiled
                              EnemyCategory facts (can check a switch, can set a switch, allowed near pits)
  logic/enemy_locations.txt   every stage key in file order, its groups and their enemy locations; every
                              path parsed into stage folder / arc / layer / chunk / index; the optional
                              group fields (ALLdie lists, existing actors to modify, new actors to add,
                              actors to remove, Free memory, throwables, bomb flowers) and the per-location
                              Position / Position offset / Y Rotation / Y Rotation offset
  data/actor_info.txt         every actor name: class name (lower case, as data_tables.py), id, subtype,
                              GBA name
  data/actor_parameters.txt   the bitfields of the classes of every enemy actor name, plus the classes of
                              Homen*, ALLdie, AND_SW*, Kbota* (class names lower case, as data_tables.py)
  randomizers/enemies.py      the module constants (MAX_ENEMY_SPECIES_PER_STAGE, ..., the two-room stage list)

The YAML files are loaded with ruamel's safe loader, as wwrando loads them, so folding of multi-line
requirement strings matches. The script checks what the C++ side relies on: every Original enemy names a
type, every placement category condition is one EnemyCategory.__eq__ implements, every path matches
the regex enemies.py uses for it, every switch/path param name exists in its class, and every attribute
enemies.py reads or writes on an enemy-rando actor is a bitfield of one of the emitted classes or a base
ACTR field. The build never runs this script; running it again must give no diff (R7.3a's verification).
"""
import ast
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402
import gen_rando_data as grd  # noqa: E402  (c_str, yaml_load, file_sha1, wwrando_commit)

OUT_DIR = grd.OUT_DIR
OUT_NAME = "rando_enemies.inc"

SOURCES = [
  "data/enemy_types.txt",
  "logic/enemy_locations.txt",
  "data/actor_info.txt",
  "data/actor_parameters.txt",
  "randomizers/enemies.py",
]

# EnemyCat in pc_rando_enemy_data.h, in this order.
CATEGORIES = ["Ground", "Pot", "Air", "StationaryAir", "Ceiling", "Sea", "Water"]
# EnemyCategory.__eq__'s conditions -> ECOND_* bits.
CONDITIONS = {"ChecksConditionSwitch": 1, "SetsDeathSwitch": 2, "HasPit": 4}
# DZx chunk names get_fourcc_by_english_chunk_name accepts -> EnemyChunk.
CHUNKS = ["Actor", "ScalableObject", "Door", "ScalableDoor"]
# ACTR fields that are not bitfields (dzx.py ChunkEntry.__setattr__'s allow list).
BASE_FIELDS = {"name", "params", "x_pos", "y_pos", "z_pos", "x_rot", "y_rot", "z_rot", "enemy_number",
               "scale_x", "scale_y", "scale_z", "save_changes", "actor_class_name"}
BITFIELDS = ["params", "x_rot", "z_rot"]
EXTRA_PARAM_ACTOR_PREFIXES = ("Homen", "ALLdie", "AND_SW", "Kbota")
CONSTANTS = ["MAX_ENEMY_SPECIES_PER_STAGE", "MAX_ENEMY_SPECIES_PER_GROUP", "MAX_RANDOMIZATION_REDOS_PER_STAGE",
             "MAX_RANDOMIZATION_REDOS_PER_ROOM", "MIN_FREE_SPACE_TO_LEAVE_PER_ROOM"]

ENEMY_PATH_RE = r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/Actor([0-9A-F]{3})$"  # get_enemy_instance_by_path
ACTOR_PATH_RE = r"^([^/]+/[^/]+\.arc)(?:/Layer([0-9a-b]))?/(Actor|ScalableObject|Door|ScalableDoor)([0-9A-F]{3})$"


def c_str(s):
  return grd.c_str(s)


def c_bool(b):
  return "true" if b else "false"


def c_double(v):
  if isinstance(v, bool) or not isinstance(v, (int, float)):
    oc.die(f"not a number: {v!r}")
  r = repr(float(v))
  assert float(r) == float(v)
  return r


def u32(v, what):
  if not isinstance(v, int) or isinstance(v, bool) or not 0 <= v <= 0xFFFFFFFF:
    oc.die(f"{what}: not a u32: {v!r}")
  return v


def u16(v, what):
  if not isinstance(v, int) or isinstance(v, bool) or not 0 <= v <= 0xFFFF:
    oc.die(f"{what}: not a u16: {v!r}")
  return v


def parse_category(s, what):
  if ":" in s:
    base, conds = s.split(":", 1)
    conds = conds.split(",")
  else:
    base, conds = s, []
  if base not in CATEGORIES:
    oc.die(f"{what}: unknown placement category {s!r}")
  bits = 0
  for c in conds:
    if c not in CONDITIONS:
      oc.die(f"{what}: condition not implemented by EnemyCategory.__eq__: {c!r}")
    bits |= CONDITIONS[c]
  return CATEGORIES.index(base), bits


def parse_path(path, regex, what):
  m = re.search(regex, path)
  if not m:
    oc.die(f"{what}: invalid actor path {path!r}")
  file = m.group(1)
  layer = -1 if m.group(2) is None else int(m.group(2), 16)
  if regex == ENEMY_PATH_RE:
    chunk, index = "Actor", int(m.group(3), 16)
  else:
    chunk, index = m.group(3), int(m.group(4), 16)
  stage, arc = file.split("/")
  rm = re.fullmatch(r"Room(\d+)\.arc", arc)
  if rm:
    room = int(rm.group(1))
  elif arc == "Stage.arc":
    room = -1
  else:
    oc.die(f"{what}: arc is neither RoomN.arc nor Stage.arc: {path!r}")
  return (stage, arc, file, room, layer, chunk, index, path)


def path_init(p):
  stage, arc, file, room, layer, chunk, index, raw = p
  return (f"{{{c_str(stage)}, {c_str(arc)}, {c_str(file)}, {room}, {layer}, EnemyChunk::{chunk}, "
          f"0x{index:03X}, {c_str(raw)}}}")


def load_constants():
  tree = grd.ast_module("randomizers/enemies.py")
  vals = {}
  for node in tree.body:
    if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
      name = node.targets[0].id
      if name in CONSTANTS or name == "STAGE_NAMES_WHERE_MULTIPLE_ROOMS_CAN_BE_LOADED_AT_ONCE":
        vals[name] = ast.literal_eval(node.value)
  for name in CONSTANTS + ["STAGE_NAMES_WHERE_MULTIPLE_ROOMS_CAN_BE_LOADED_AT_ONCE"]:
    if name not in vals:
      oc.die(f"enemies.py: {name} not found")
  return vals


def enemies_py_attributes():
  """Every attribute enemies.py reads or writes on an actor variable (enemy, actor, homen)."""
  with open(grd.ww("randomizers/enemies.py")) as f:
    src = f.read()
  names = set(re.findall(r"\b(?:enemy|actor|homen)\.([a-z_][a-z_0-9]*)\b", src))
  # setattr(enemy, <var>, ...) / getattr(enemy, <var>) use the param name fields, checked separately.
  return sorted(names)


def gen():
  types = grd.yaml_load("data/enemy_types.txt")
  locs = grd.yaml_load("logic/enemy_locations.txt")
  actor_info = grd.yaml_load("data/actor_info.txt")
  actor_params_cs = grd.yaml_load("data/actor_parameters.txt")
  consts = load_constants()

  # data_tables.py: class names are case insensitive (lower-cased).
  actor_class = {}
  for name, info in actor_info.items():
    cls = info["Class Name"]
    actor_class[name] = None if cls is None else cls.lower()
  actor_params = {}
  for cls, params in actor_params_cs.items():
    actor_params[cls.lower()] = [(p, d["Bitfield name"], d["Mask"]) for p, d in params.items()]

  # --- Enemy types ---
  type_names = [t["Pretty name"] for t in types]
  if len(set(type_names)) != len(type_names):
    oc.die("enemy_types.txt: duplicate Pretty name")
  particles = []
  type_rows = []
  for t in types:
    what = f"enemy type {t['Pretty name']}"
    for key in ("Allow randomizing from", "Allow randomizing to", "Allow near pits",
                "Can be killed with thrown objects", "Can be killed with bomb flowers"):
      if not isinstance(t[key], bool):
        oc.die(f"{what}: {key} is not a bool")
    if not isinstance(t["Requirements to defeat"], str):
      oc.die(f"{what}: Requirements to defeat is not a string")
    cats = [parse_category(c, what) for c in t["Placement categories"]]
    if any(bits for _, bits in cats):
      oc.die(f"{what}: a type's placement category has conditions")
    if not 1 <= len(cats) <= 3:
      oc.die(f"{what}: {len(cats)} placement categories")
    name = t["Actor name"]
    cls = actor_class.get(name)
    if cls is None:
      oc.die(f"{what}: actor {name} has no class in actor_info.txt")
    for key in ("Enable spawn switch param name", "Death switch param name", "Path param name"):
      p = t[key]
      if p is not None and p not in [f for f, _, _ in actor_params.get(cls, [])]:
        oc.die(f"{what}: {key} {p} is not a bitfield of {cls}")
    first = len(particles)
    for pid in t["Required particle IDs"]:
      particles.append(u16(pid, what))
    type_rows.append((t, cats, first, len(particles) - first))

  # --- Enemy locations ---
  stages = []
  groups = []
  enemies = []
  alldies = []
  modifies = []
  adds = []
  removes = []
  for key, glist in locs.items():
    stage_first = len(groups)
    for gi, g in enumerate(glist):
      what = f"{key} group {gi}"
      known = {"Must defeat enemies", "Original requirements", "Enemies", "Has throwable objects",
               "Has bomb flowers", "Free memory", "Actors to replace with ALLdies", "Existing actors to modify",
               "New actors to add", "Actors to remove"}
      if set(g) - known:
        oc.die(f"{what}: unknown keys {sorted(set(g) - known)}")
      if not isinstance(g["Must defeat enemies"], bool) or not isinstance(g["Original requirements"], str):
        oc.die(f"{what}: bad Must defeat enemies / Original requirements")
      gidx = len(groups)
      e_first = len(enemies)
      for e in g["Enemies"]:
        ew = f"{what} {e.get('Path')}"
        ekeys = {"Original enemy", "Placement category", "Path", "Position", "Position offset", "Y Rotation",
                 "Y Rotation offset"}
        if set(e) - ekeys:
          oc.die(f"{ew}: unknown keys {sorted(set(e) - ekeys)}")
        if e["Original enemy"] not in type_names:
          oc.die(f"{ew}: Original enemy {e['Original enemy']!r} is not a type")
        cat, bits = parse_category(e["Placement category"], ew)
        flags = []
        pos = [0, 0, 0]
        off = [0, 0, 0]
        yrot = 0
        yoff = 0
        if "Position" in e:
          flags.append("ELOC_POSITION")
          pos = e["Position"]
          assert len(pos) == 3, ew
        if "Position offset" in e:
          flags.append("ELOC_POSITION_OFFSET")
          off = e["Position offset"]
          assert len(off) == 3, ew
        if "Y Rotation" in e:
          flags.append("ELOC_Y_ROTATION")
          yrot = u16(e["Y Rotation"], ew)
        if "Y Rotation offset" in e:
          flags.append("ELOC_Y_ROTATION_OFFSET")
          yoff = e["Y Rotation offset"]
          assert isinstance(yoff, int) and -0x10000 < yoff < 0x10000, ew
        enemies.append((gidx, type_names.index(e["Original enemy"]), e["Original enemy"], e["Placement category"],
                        cat, bits, parse_path(e["Path"], ENEMY_PATH_RE, ew), flags, pos, off, yrot, yoff))
      if not g["Enemies"]:
        oc.die(f"{what}: no enemies")
      a_first = len(alldies)
      for p in g.get("Actors to replace with ALLdies", []):
        alldies.append(parse_path(p, ACTOR_PATH_RE, what))
      m_first = len(modifies)
      for m in g.get("Existing actors to modify", []):
        modifies.append(actor_data(m, what, need_path=True))
      n_first = len(adds)
      for n in g.get("New actors to add", []):
        adds.append(actor_data(n, what, need_path=False))
      r_first = len(removes)
      for p in g.get("Actors to remove", []):
        removes.append(parse_path(p, ACTOR_PATH_RE, what))
      fm = g.get("Free memory", None)
      if fm is not None:
        u32(fm, what)
      groups.append((len(stages), g["Must defeat enemies"], g["Original requirements"],
                     g.get("Has throwable objects", False), g.get("Has bomb flowers", False),
                     -1 if fm is None else fm, e_first, len(enemies) - e_first, a_first, len(alldies) - a_first,
                     m_first, len(modifies) - m_first, n_first, len(adds) - n_first, r_first, len(removes) - r_first))
      for b in (g.get("Has throwable objects", False), g.get("Has bomb flowers", False)):
        if not isinstance(b, bool):
          oc.die(f"{what}: throwable / bomb flower flag is not a bool")
    stages.append((key, stage_first, len(groups) - stage_first))

  # --- Actor parameter classes ---
  wanted = []
  for t in types:
    cls = actor_class[t["Actor name"]]
    if cls not in wanted:
      wanted.append(cls)
  for name, cls in actor_class.items():
    if name.startswith(EXTRA_PARAM_ACTOR_PREFIXES) and cls is not None and cls not in wanted:
      wanted.append(cls)
  for cls in wanted:
    if cls not in actor_params:
      oc.die(f"actor_parameters.txt: no class {cls}")
    for p, bf, mask in actor_params[cls]:
      if bf not in BITFIELDS:
        oc.die(f"{cls}.{p}: unknown bitfield {bf}")
      u32(mask, f"{cls}.{p}")
      if mask == 0:
        oc.die(f"{cls}.{p}: zero mask")
  emitted_params = {p for cls in wanted for p, _, _ in actor_params[cls]}
  for attr in enemies_py_attributes():
    if attr not in BASE_FIELDS and attr not in emitted_params:
      oc.die(f"enemies.py uses actor attribute {attr}, which no emitted class has")

  # --- Output ---
  gen_sha = grd.file_sha1(os.path.abspath(__file__))
  out = [
    "// GENERATED FILE - do not edit. Regenerate with: python3 -I native/tools/rando/gen_enemy_data.py",
    "// Enemy randomizer data: enemy_types.txt, enemy_locations.txt, actor_info.txt, actor_parameters.txt",
    f"// Source: wwrando 1.10.0_9775811 (MIT), commit {grd.wwrando_commit()}",
    f"// Generator: native/tools/rando/gen_enemy_data.py sha1 {gen_sha}",
  ]
  for s in SOURCES:
    out.append(f"//   {s} sha1 {grd.file_sha1(grd.ww(s))}")
  out.append("// Included once, by pc_rando_enemy_data.cpp (inside namespace pcr::gen_enemy).")
  out.append("")

  out.append("// randomizers/enemies.py module constants")
  for name in CONSTANTS:
    out.append(f"static const int k{''.join(w.capitalize() for w in name.lower().split('_'))} = {consts[name]};")
  out.append("")
  out.append(grd.str_array("kTwoRoomStages", consts["STAGE_NAMES_WHERE_MULTIPLE_ROOMS_CAN_BE_LOADED_AT_ONCE"]))
  out.append("")

  out.append("// data/enemy_types.txt Required particle IDs, per type")
  out.append("static const uint16_t kEnemyParticleIds[] = {")
  for i in range(0, len(particles), 8):
    out.append("    " + " ".join(f"0x{p:04X}," for p in particles[i:i + 8]))
  out.append("};\n")

  out.append("// data/enemy_types.txt, file order")
  out.append("static const EnemyType kEnemyTypes[] = {")
  for t, cats, pfirst, pnum in type_rows:
    cat_list = ", ".join(f"EnemyCat::{CATEGORIES[c]}" for c, _ in cats)
    out.append(
      f"    {{{c_str(t['Pretty name'])}, {c_str(t['Actor name'])}, 0x{u32(t['Params'], 'Params'):08X}u, "
      f"0x{u16(t['X Rotation'], 'X Rotation'):04X}, 0x{u16(t['Z Rotation'], 'Z Rotation'):04X},\n"
      f"     {pfirst}, {pnum}, {c_str(t['Requirements to defeat'])},\n"
      f"     {c_bool(t['Allow randomizing from'])}, {c_bool(t['Allow randomizing to'])}, {len(cats)}, {{{cat_list}}},\n"
      f"     {u32(t['Memory used by first one'], 'mem')}u, {u32(t['Memory used by subsequent ones'], 'mem')}u,\n"
      f"     {c_str(t['Enable spawn switch param name'])}, {c_str(t['Death switch param name'])}, "
      f"{c_str(t['Path param name'])},\n"
      f"     {c_bool(t['Allow near pits'])}, {c_bool(t['Can be killed with thrown objects'])}, "
      f"{c_bool(t['Can be killed with bomb flowers'])}}},")
  out.append("};\n")

  out.append("// logic/enemy_locations.txt: every stage key in file order -> its groups")
  out.append("static const EnemyStage kEnemyStages[] = {")
  for key, first, n in stages:
    out.append(f"    {{{c_str(key)}, {first}, {n}}},")
  out.append("};\n")

  out.append("// logic/enemy_locations.txt groups, in file order")
  out.append("static const EnemyGroup kEnemyGroups[] = {")
  for (stage, must, req, thr, bomb, fm, ef, en, af, an, mf, mn, nf, nn, rf, rn) in groups:
    out.append(f"    {{{stage}, {c_bool(must)}, {c_bool(thr)}, {c_bool(bomb)}, {fm},\n"
               f"     {c_str(req)},\n"
               f"     {ef}, {en}, {af}, {an}, {mf}, {mn}, {nf}, {nn}, {rf}, {rn}}},")
  out.append("};\n")

  out.append("// The Enemies of every group, in file order")
  out.append("static const EnemyLocation kEnemyLocations[] = {")
  for (g, ti, orig, raw_cat, cat, bits, path, flags, pos, off, yrot, yoff) in enemies:
    fl = " | ".join(flags) if flags else "0"
    out.append(f"    {{{g}, {ti}, {c_str(orig)}, {c_str(raw_cat)}, EnemyCat::{CATEGORIES[cat]}, {bits},\n"
               f"     {path_init(path)},\n"
               f"     {fl}, {{{', '.join(c_double(v) for v in pos)}}}, {{{', '.join(c_double(v) for v in off)}}}, "
               f"0x{yrot:04X}, {yoff}}},")
  out.append("};\n")

  for cname, rows, comment in (("kEnemyAlldiePaths", alldies, "Actors to replace with ALLdies"),
                               ("kEnemyRemovePaths", removes, "Actors to remove")):
    out.append(f"// Every group's \"{comment}\", in file order")
    out.append(f"static const EnemyActorPath {cname}[] = {{")
    for p in rows:
      out.append(f"    {path_init(p)},")
    out.append("};\n")

  for cname, rows, comment in (("kEnemyModifyActors", modifies, "Existing actors to modify"),
                               ("kEnemyAddActors", adds, "New actors to add")):
    out.append(f"// Every group's \"{comment}\", in file order")
    out.append(f"static const EnemyActorData {cname}[] = {{")
    for (path, flags, name, params, xr, yr, zr, pos) in rows:
      fl = " | ".join(flags) if flags else "0"
      pinit = "{nullptr, nullptr, nullptr, -1, -1, EnemyChunk::Actor, 0, nullptr}" if path is None else path_init(path)
      out.append(f"    {{{pinit},\n     {fl}, {c_str(name)}, 0x{params:08X}u, 0x{xr:04X}, 0x{yr:04X}, 0x{zr:04X}, "
                 f"{{{', '.join(c_double(v) for v in pos)}}}}},")
    out.append("};\n")

  out.append("// data/actor_info.txt, file order: {actor name, class name (lower case), actor id, subtype, GBA name}")
  out.append("static const ActorInfo kActorInfo[] = {")
  for name, info in actor_info.items():
    out.append(f"    {{{c_str(name)}, {c_str(actor_class[name])}, 0x{u16(info['Actor ID'], name):04X}, "
               f"0x{info['Subtype']:02X}, 0x{info['GBA Name']:02X}}},")
  out.append("};\n")

  out.append("// data/actor_parameters.txt: the classes of the enemy actors, then Homen*, ALLdie, AND_SW*, Kbota*")
  fields = []
  classes = []
  for cls in wanted:
    first = len(fields)
    for p, bf, mask in actor_params[cls]:
      fields.append((p, bf, mask))
    classes.append((cls, first, len(fields) - first))
  out.append("static const ActorParamField kActorParamFields[] = {")
  for p, bf, mask in fields:
    out.append(f"    {{{c_str(p)}, ActorBitfield::{bf}, 0x{mask:08X}u}},")
  out.append("};\n")
  out.append("static const ActorParamClass kActorParamClasses[] = {")
  for cls, first, n in classes:
    out.append(f"    {{{c_str(cls)}, {first}, {n}}},")
  out.append("};")
  return "\n".join(out) + "\n"


def actor_data(d, what, need_path):
  keys = {"Path", "Name", "Params", "X Rotation", "Y Rotation", "Z Rotation", "Position"}
  if set(d) - keys:
    oc.die(f"{what}: unknown actor data keys {sorted(set(d) - keys)}")
  if need_path != ("Path" in d):
    oc.die(f"{what}: Path {'missing' if need_path else 'unexpected'}")
  if not need_path and ("Name" not in d or "Params" not in d):
    oc.die(f"{what}: a new actor needs Name and Params")
  path = parse_path(d["Path"], ACTOR_PATH_RE, what) if need_path else None
  flags = []
  if "Name" in d:
    flags.append("EACT_NAME")
  params = 0
  if "Params" in d:
    flags.append("EACT_PARAMS")
    params = u32(d["Params"], what)
  rots = []
  for k, f in (("X Rotation", "EACT_X_ROTATION"), ("Y Rotation", "EACT_Y_ROTATION"), ("Z Rotation", "EACT_Z_ROTATION")):
    if k in d:
      flags.append(f)
      rots.append(u16(d[k], what))
    else:
      rots.append(0)
  pos = [0, 0, 0]
  if "Position" in d:
    flags.append("EACT_POSITION")
    pos = d["Position"]
    assert len(pos) == 3, what
  return (path, flags, d.get("Name"), params, rots[0], rots[1], rots[2], pos)


def main(argv):
  check = "--check" in argv
  unknown = [a for a in argv if a != "--check"]
  if unknown:
    oc.die(f"unknown arguments: {' '.join(unknown)}")
  oc.bootstrap()
  text = gen()
  path = os.path.join(OUT_DIR, OUT_NAME)
  old = None
  if os.path.exists(path):
    with open(path, encoding="utf-8") as f:
      old = f.read()
  if check:
    if old != text:
      print("out of date: " + OUT_NAME)
      return 1
    print("up to date")
    return 0
  if old != text:
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(path, "w", newline="\n", encoding="utf-8") as f:
      f.write(text)
    print("wrote " + OUT_NAME)
  else:
    print("no changes")
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
