#!/usr/bin/env python3
"""identity_check.py - step R9.2 (Milestone R2's second bullet): the identity placement is vanilla.

    python3 -I native/tools/rando/identity_check.py run   [--out DIR] [--only LIST] [--stage S:R:P[:L]]
                                                           [--placement P.json] [--disc ISO] [--exe EXE]
    python3 -I native/tools/rando/identity_check.py check --identity RUN --vanilla RUN [--placement P.json]
                                                           [--disc ISO] [--out DIR]
    python3 -I native/tools/rando/identity_check.py saves [--out DIR] [--placement P.json] [case ...]
    python3 -I native/tools/rando/identity_check.py saves-compare --vanilla DIR --identity DIR [--out DIR]

ARCHIVES (`run` = two smoke runs + `check`). The smoke COS_SMOKE=rando COS_RANDO_SMOKE=idhash
(native/src/pc/harness/smokes/pc_rando_smoke_idhash.cpp) loads every .arc under /res/Stage/<stage>/
and directly in /res/Object/ through dRes_control_c (1,282 on the USA disc, about 45 s) and reports
each file the archive-load hook dispatches: its MD5 before any patcher and after each init function
of features/rando/pc_rando_patchers.def ("step"), with the bytes of every changed file
(COS_RANDO_IDHASH_DUMP). It runs once with COS_RANDO_PLACEMENT=<placement> (default
native/check/rando/fixtures/R0_6_identity.json) and once without (vanilla). The verdict:

  1. vanilla: pc_rando() 0 and no file dispatched (the hook is off, the game reads the disc's bytes);
     the same archives loaded as the identity run, every one of them.
  2. the disc: every file the identity run dispatched hashes, before any patcher, as the same file in
     the disc's archive (gclib), so the hook sees the vanilla bytes.
  3. placement steps (R2.1 chests, R2.2 actors, R2.3 scalable objects, R2.4 events): wwrando's own
     ItemRandomizer.change_chest_item / change_actor_item / change_scob_item / change_event_item
     (randomizers/items.py) with the placement's item for every location path, after the tweaks
     wwrando's apply_necessary_tweaks runs on the same records before it saves the items
     (fix_deku_leaf_model and make_dungeon_joy_pendant_locations_flexible for R2.2,
     fix_needle_rock_island_salvage_flags for R2.3), run with gclib on the bytes the step got, must
     give exactly the bytes the step left (stage files byte for byte; event_list.dat action by
     action, since the port edits it in place and wwrando re-serializes it). Every placed item must
     be the location's vanilla ("Original item") one, and every record the step changed must still
     hold that item. So the placement changes nothing but wwrando's re-encodings of the same items
     (pots and stone heads: the item moved to the custom item_id field, dropped_item_id 0x3F; field
     items: behavior type 0 -> 3; events: 046pget / 011_get_item -> 011get_item).
  4. seed-dependent steps (charts, traps, required bosses, hints, entrances, warp pots, palettes,
     enemies, and anything registered later): no file changed under the identity placement.
  5. tweak steps (wwrando's always-applied tweaks, which a wwrando seed with vanilla items gets as
     well: R3.15-R3.19, R3.21, R2.10, R3.2, R7.5a, R7.2c's make_rat_holes_visible_from_behind):
     they change exactly the files listed in native/check/rando/fixtures/R9_2_tweak_files.json
     (a list of names, no disc bytes); --write-tweaks rewrites it from the run.

  Locations outside the archives (main.dol / REL / custom symbols: code locations) are not archive
  bytes; the location sweep (location_sweep.py, R9.1) checks them.

  --only LIST   COS_RANDO_IDHASH_ONLY for both runs (archive paths below /res/, prefixes, e.g.
                Stage/Omori/,Stage/sea/Room29.arc); checks 3 and 5 then cover the loaded files only
  --stage       the boot stage (default Asoko:0:0:2)

SAVES (`saves`): native/tools/save_sweep.py (every case, or the ones named) once without and once
with COS_RANDO_PLACEMENT=<placement>; both sweeps must pass (each save loads back equal). A
randomized file is not a vanilla file even with vanilla items: its new game runs wwrando's
init_save_with_tweaks / init_starting_gear / reveal_sea_chart (R3.1, R3.2, R6.4) and its slot holds
the randomizer record (D3, "WWR1" in dSv_reserve_c). So the saves must be equal both ways apart from
the record and from one new-file state that is the same in every case (it cannot depend on where or
what the game saved): flag regions (event, memory, map, ...) identity = vanilla | N with no bit
cleared; other regions identity = N at N's positions and vanilla elsewhere; the return place, the
names and the config equal. `saves-compare` compares two existing sweep directories (the cases both
have).

Reports: <out>/identity_check.txt (archives) and <out>/saves_check.txt. <out> defaults to
build/rando-identity/<timestamp>. Exit codes: 0 every check held, 1 a check failed, 2 usage or
setup error. Nothing read from the disc is written to the repository.
"""
import argparse
import collections
import contextlib
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

REPO = oc.REPO_ROOT
RUN_SH = os.path.join(REPO, "native", "tools", "run.sh")
SAVE_SWEEP = os.path.join(REPO, "native", "tools", "save_sweep.py")
DEFAULT_PLACEMENT = os.path.join(REPO, "native", "check", "rando", "fixtures", "R0_6_identity.json")
TWEAKS_FIXTURE = os.path.join(REPO, "native", "check", "rando", "fixtures", "R9_2_tweak_files.json")
DEFAULT_DISC = os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso")

# pc_rando_patchers.def init functions by role (check 3, 4, 5). An init not named here is a
# seed-dependent step (check 4): a new step must change nothing under identity unless listed.
PLACEMENT_STEPS = {
  "pc_rando_place_dzx_init": "Chest",            # R2.1
  "pc_rando_place_actr_init": "Actor",           # R2.2
  "pc_rando_place_scob_init": "ScalableObject",  # R2.3
  "pc_rando_place_event_init": "Event",          # R2.4
}
# wwrando's apply_necessary_tweaks that edit the records of a placement step's files; the port's step
# runs them in the same patcher (wwrando runs them before ItemRandomizer.save).
PLACEMENT_TWEAKS = {
  "pc_rando_place_actr_init": ["fix_deku_leaf_model", "make_dungeon_joy_pendant_locations_flexible"],
  "pc_rando_place_scob_init": ["fix_needle_rock_island_salvage_flags"],
}
TWEAK_STEPS = {
  "pc_rando_stages_a_init": "R3.15 stage tweaks A",
  "pc_rando_tweak_stages_b_init": "R3.16 stage tweaks B",
  "pc_rando_posttext_patchers_init": "R3.21 post-randomization texts",
  "pc_rando_tweak_added_chests_init": "R2.10 the 4 added chests",
  "pc_rando_start_init": "R3.2 start_ship_at_outset, prevent_fire_mountain_lava_softlock",
  "pc_rando_tweak_zephos_init": "R3.19 shorten_zephos_event, shorten_auction_intro_event",
  "pc_rando_pirate_ship_init": "R3.17 add_pirate_ship_to_windfall",
  "pc_rando_tweak_totg_init": "R3.18 allow_nonlinear_servants_of_the_towers",
  "pc_rando_tweak_cosmetic_init": "R7.5a give_fairy_fountains_distinct_colors",
  # R7.2c: the palettes themselves are seed-dependent (none under identity); the same init applies
  # make_rat_holes_visible_from_behind (Nzg kana_00.bdl), which wwrando always applies.
  "pc_rando_palettes_init": "R7.2c make_rat_holes_visible_from_behind",
}


def log(msg):
  print(msg, file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------------------------
# The smoke's report

class Report:
  def __init__(self, run_dir):
    self.run_dir = run_dir
    path = os.path.join(run_dir, "idhash.txt")
    if not os.path.isfile(path):
      raise FileNotFoundError(path)
    self.header = ""
    self.steps = {}      # n -> init name
    self.arcs = []       # (path, loaded)
    self.files = []      # dicts
    with open(path) as f:
      for line in f:
        line = line.rstrip("\n")
        if line.startswith("#"):
          self.header = line
          continue
        p = line.split(" ")
        if p[0] == "step":
          self.steps[int(p[1])] = p[2]
        elif p[0] == "arc":
          self.arcs.append((p[1], p[2] == "loaded"))
        elif p[0] == "file":
          self.files.append({
            "list": p[1], "dir": p[2], "arc": p[3], "name": p[4], "type": p[5],
            "size0": int(p[6]), "md50": p[7], "sizeF": int(p[8]), "md5F": p[9],
            "steps": [] if p[10] == "-" else [int(s) for s in p[10].split(",")],
          })
    m = re.search(r"pc_rando\(\) (\d)", self.header)
    self.pc_rando = int(m.group(1)) if m else -1
    rc_path = os.path.join(run_dir, "exit_code.txt")
    self.exit_code = int(open(rc_path).read().strip()) if os.path.isfile(rc_path) else -1


def file_key(f):
  return f"{f['dir']}~{f['arc']}~{f['name']}".replace("/", "_")


def disc_arc_path(f):
  return "files" + f["dir"] + f["arc"] + ".arc"


# ---------------------------------------------------------------------------------------------
# Runs

def run_smoke(run_dir, placement, only, stage, exe, disc, dump_dir):
  cmd = [RUN_SH, "rando", "--stage", stage, "--timeout", "900", "--stall", "120", "--quiet",
         "--disc", disc, "--run-dir", run_dir, "--env", "COS_RANDO_SMOKE=idhash"]
  if exe:
    cmd += ["--exe", exe]
  if placement:
    cmd += ["--env", "COS_RANDO_PLACEMENT=" + placement]
  if only:
    cmd += ["--env", "COS_RANDO_IDHASH_ONLY=" + only]
  if dump_dir:
    os.makedirs(dump_dir, exist_ok=True)
    cmd += ["--env", "COS_RANDO_IDHASH_DUMP=" + dump_dir]
  env = dict(os.environ, COS_CACHE_PER_RUN="1")
  env.pop("COS_RANDO_PLACEMENT", None)
  log("identity_check: " + " ".join(cmd[1:]))
  t = time.time()
  subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=REPO)
  log("identity_check:   %.0f s" % (time.time() - t))


# ---------------------------------------------------------------------------------------------
# The disc and wwrando

class Disc:
  def __init__(self, iso):
    from gclib.gcm import GCM
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    self.arcs = {}
    self.md5s = {}

  def rarc(self, path):
    from gclib.rarc import RARC
    if path not in self.arcs:
      try:
        r = RARC(self.gcm.read_file_data(path))
        r.read()
      except Exception:  # noqa: BLE001 - not on the disc / unreadable: reported by the caller
        r = None
      self.arcs[path] = r
    return self.arcs[path]

  def file_bytes(self, arc_path, name):
    """Every file of that name in the archive, decompressed (an archive may hold a name twice)."""
    r = self.rarc(arc_path)
    if r is None:
      return []
    out = []
    for fe in r.file_entries:
      # The smoke writes a name's blanks as '_' (pc_rando_smoke_idhash.cpp field()).
      if fe.is_dir or re.sub(r"[ \t\n]", "_", fe.name) != name:
        continue
      fe.decompress_data_if_necessary()
      out.append(fe.data.getvalue())
    return out

  def md5_set(self, arc_path, name):
    k = (arc_path, name)
    if k not in self.md5s:
      self.md5s[k] = {(len(b), hashlib.md5(b).hexdigest()) for b in self.file_bytes(arc_path, name)}
    return self.md5s[k]


def file_entry(data):
  from gclib.gclib_file import GCLibFileEntry
  fe = GCLibFileEntry()
  fe.data = io.BytesIO(data)
  return fe


class StubArc:
  def __init__(self, owner, arc_path):
    self.owner = owner
    self.arc_path = arc_path
    self.files = {}

  def get_file(self, name, cls):
    if name not in self.files:
      data = self.owner.input_bytes(self.arc_path, name)
      if data is None:
        raise KeyError(f"{self.arc_path}/{name}: no input bytes")
      self.files[name] = cls(file_entry(data))
    return self.files[name]


class StubRando:
  """Just enough of WWRandomizer for tweaks.* and ItemRandomizer.change_*: get_arc over the bytes a
  step got, item_name_to_id, options."""

  def __init__(self, input_bytes, item_name_to_id, options):
    self.input_bytes = input_bytes
    self.item_name_to_id = item_name_to_id
    self.options = options
    self.arcs = {}

  def get_arc(self, path):
    if path not in self.arcs:
      self.arcs[path] = StubArc(self, path)
    return self.arcs[path]


class StubItems:
  def __init__(self, rando, options):
    self.rando = rando
    self.options = options


def event_dump(ev_list):
  out = {}
  for ei, ev in enumerate(ev_list.events):
    for ai, a in enumerate(ev.actors):
      for ci, act in enumerate(a.actions):
        out[(ei, ev.name, ai, a.name, ci)] = (act.name, [(p.name, p.value if isinstance(p.value, list) else [p.value])
                                                         for p in act.properties])
  return out


# The location-holding records' raw layout, for the "record still holds its item" line
# (gen_location_keys.py's FIELDS give the same fields).
def record_item(entry):
  cls = entry.actor_class_name if entry.IS_ACTOR_CHUNK else None
  if cls in ("d_a_tsubo", "d_a_obj_homen"):
    return entry.item_id if entry.dropped_item_id == 0x3F else entry.dropped_item_id
  if cls == "d_a_deku_item":
    return 0x34  # Deku Leaf: the itemDek actor gives nothing else
  return entry.item_id


# ---------------------------------------------------------------------------------------------
# The archive check

class Checker:
  def __init__(self, out_lines):
    self.lines = out_lines
    self.fails = 0

  def check(self, what, ok, detail=""):
    self.lines.append(("ok   " if ok else "FAIL ") + what + (f": {detail}" if detail else ""))
    if not ok:
      self.fails += 1
    return ok

  def note(self, text):
    self.lines.append("     " + text)


def check_archives(args, identity_dir, vanilla_dir, dump_dir):
  oc.bootstrap()
  from wwlib.dzx import DZx, ACTR, SCOB, TRES, DZxLayer
  from wwlib.events import EventList
  import tweaks
  from randomizers.items import ItemRandomizer
  import gen_rando_data as gd

  lines = []
  c = Checker(lines)
  try:
    ident = Report(identity_dir)
  except FileNotFoundError as e:
    c.check("identity run wrote its report", False, str(e))
    return c
  try:
    van = Report(vanilla_dir)
  except FileNotFoundError as e:
    c.check("vanilla run wrote its report", False, str(e))
    return c
  placement = oc.load_json(args.placement) if hasattr(oc, "load_json") else json.load(open(args.placement))

  # 1. both runs
  c.check("identity smoke exit 0", ident.exit_code == 0, f"exit {ident.exit_code} ({identity_dir}/run.log)")
  c.check("identity run under pc_rando()", ident.pc_rando == 1, ident.header)
  c.check("vanilla smoke exit 0", van.exit_code == 0, f"exit {van.exit_code} ({vanilla_dir}/run.log)")
  c.check("vanilla run: pc_rando() 0", van.pc_rando == 0, van.header)
  c.check("vanilla run: no file dispatched (the hook is off)", not van.files, f"{len(van.files)} files")
  ia = sorted(ident.arcs)
  va = sorted(van.arcs)
  c.check("both runs loaded the same archives", ia == va, f"identity {len(ia)}, vanilla {len(va)}")
  c.check("every archive loaded (identity)", all(ok for _, ok in ia) and ia,
          f"{sum(ok for _, ok in ia)} of {len(ia)}")
  c.check("every archive loaded (vanilla)", all(ok for _, ok in va) and va,
          f"{sum(ok for _, ok in va)} of {len(va)}")
  c.note(f"{len(ia)} archives, {len(ident.files)} file dispatches "
         f"({sum(f['list'] == 'raw' for f in ident.files)} raw J3D)")

  # 2. the disc. DZB files are skipped: loadResource converts them (cBgS::ConvDzb, vanilla code) before
  # the hook sees them; no patcher edits them (check 4/5 would name any that did).
  disc = Disc(args.disc)
  bad = []
  n_dzb = 0
  for f in ident.files:
    if f["type"] == "DZB_":
      n_dzb += 1
      continue
    if (f["size0"], f["md50"]) not in disc.md5_set(disc_arc_path(f), f["name"]):
      bad.append(f"{f['list']} {f['dir']}{f['arc']}/{f['name']}")
  c.check("every dispatched file, before any patcher, is the disc's file", not bad,
          f"{len(bad)} differ: " + ", ".join(sorted(set(bad))[:20]) if bad else
          f"{len(ident.files) - n_dzb} dispatches ({n_dzb} DZB converted by cBgS::ConvDzb first, skipped)")

  # Per file: its steps (every dispatch of a file must have the same steps: the same bytes in, the
  # same patchers).
  name_of = dict(ident.steps)
  # An archive may hold one name twice (other folder or type): only a changed file must be unique,
  # since its dumps are keyed by dir, archive and name.
  by_file = collections.OrderedDict()
  for f in ident.files:
    by_file.setdefault((f["list"], f["dir"], f["arc"], f["name"]), []).append(f)
  inconsistent = [k for k, fs_ in by_file.items() if any(x["steps"] for x in fs_) and
                  len({(x["type"], x["md50"], x["md5F"], tuple(x["steps"])) for x in fs_}) > 1]
  c.check("each changed file's dispatches agree (type, bytes in, bytes out, steps)", not inconsistent,
          ", ".join("/".join(k[1:]) for k in inconsistent[:10]))
  changed = {k: v[0] for k, v in by_file.items() if v[0]["steps"]}
  per_step = collections.defaultdict(list)
  for k, f in changed.items():
    for s in f["steps"]:
      per_step[name_of.get(s, "late")].append(k)
  c.note(f"{len(changed)} files changed by a step: " +
         ", ".join(f"{n} {len(v)}" for n, v in sorted(per_step.items())))

  # 4. seed-dependent steps
  for n in sorted(name_of):
    init = name_of[n]
    if init in PLACEMENT_STEPS or init in TWEAK_STEPS:
      continue
    ks = per_step.get(init, [])
    c.check(f"seed-dependent step {n} {init}: no file changed", not ks,
            ", ".join("/".join(k[1:]) for k in ks[:20]))

  # 5. tweak steps
  try:
    expected_tweaks = json.load(open(TWEAKS_FIXTURE))
  except FileNotFoundError:
    expected_tweaks = {}
  def label(k):  # (list, dir, arc, name) -> "/res/Stage/Omori/Room0/room.dzr[ (raw)]"
    return f"{k[1]}{k[2]}/{k[3]}" + ("" if k[0] == "res" else " (raw)")
  loaded = {label((f["list"], f["dir"], f["arc"], f["name"])) for f in ident.files}
  got_tweaks = {}
  for init in TWEAK_STEPS:
    got = sorted(label(k) for k in per_step.get(init, []))
    got_tweaks[init] = got
    if args.write_tweaks:
      continue
    exp = expected_tweaks.get(init)
    if exp is None:
      c.check(f"tweak step {init}: listed in {os.path.basename(TWEAKS_FIXTURE)}", False)
      continue
    exp_l = sorted(s for s in exp if s in loaded)  # only the files this run loaded (--only)
    extra = sorted(set(got) - set(exp_l))
    missing = sorted(set(exp_l) - set(got))
    c.check(f"tweak step {init} ({TWEAK_STEPS[init]}): changed exactly its {len(exp_l)} listed files",
            not extra and not missing,
            ("unlisted: " + ", ".join(extra[:10]) if extra else "") +
            (" not changed: " + ", ".join(missing[:10]) if missing else ""))
  if args.write_tweaks:
    with open(TWEAKS_FIXTURE, "w") as f:
      f.write(json.dumps({
        "_comment": "R9.2 (identity_check.py): the files each always-applied tweak step changes under the "
                    "identity placement; '<dir><arc>/<name>', ' (raw)' = the raw J3D list. Regenerate with "
                    "identity_check.py run --write-tweaks (full run, no --only).",
        **got_tweaks}, indent=1) + "\n")
    c.note(f"wrote {TWEAKS_FIXTURE}")

  # 3. placement steps against wwrando
  locs = gd.yaml_load("logic/item_locations.txt")
  item_ids = {name: iid for iid, name in gd.load_item_names()}
  placed = placement.get("locations", {})
  not_vanilla = [n for n, v in locs.items() if n in placed and placed[n] != v["Original item"]]
  c.check("every placed item is the location's Original item (identity)", not not_vanilla,
          ", ".join(not_vanilla[:10]))
  missing_locs = [n for n in locs if n not in placed]
  c.check("the placement fills every location", not missing_locs, ", ".join(missing_locs[:10]))
  opts = placement.get("options", {})

  class Opts:
    chest_type_matches_contents = bool(opts.get("chest_type_matches_contents", False))

  step_no = {init: n for n, init in name_of.items()}
  dumps = collections.defaultdict(dict)  # key -> {step: path}
  if dump_dir and os.path.isdir(dump_dir):
    for fn in os.listdir(dump_dir):
      m = re.match(r"^(.*)\.s(\d+)$", fn)
      if m:
        dumps[m.group(1)][int(m.group(2))] = os.path.join(dump_dir, fn)
  res_files = {(f["dir"], f["arc"], f["name"]): f for f in ident.files if f["list"] == "res"}

  def file_of(arc_path, name):
    # "files/res/Stage/Omori/Room0.arc" -> ("/res/Stage/Omori/", "Room0", name)
    p = arc_path[len("files"):]
    d, _, a = p.rpartition("/")
    return res_files.get((d + "/", a[:-4], name))

  def bytes_before(f, step):
    """The bytes step `step` got: the dump of the last earlier changing step, else the disc's."""
    d = dumps.get(file_key(f), {})
    earlier = [s for s in d if s < step]
    if earlier:
      return open(d[max(earlier)], "rb").read()
    got = disc.file_bytes(disc_arc_path(f), f["name"])
    return got[0] if got else None

  def bytes_after(f, step):
    d = dumps.get(file_key(f), {})
    if step in f["steps"]:
      return open(d[step], "rb").read() if step in d else None
    return bytes_before(f, step)

  for init, kind in PLACEMENT_STEPS.items():
    n = step_no.get(init)
    if n is None:
      c.check(f"placement step {init} in pc_rando_patchers.def", False)
      continue
    skipped = collections.Counter()
    paths = []
    for name, v in locs.items():
      for path in v["Paths"]:
        k = gd.classify_path(path)
        if k[0] != kind:
          if k[0] in ("Rel", "MainDol", "CustomSymbol"):
            skipped["code"] += 1
          continue
        paths.append((name, path, k))

    def inputs(arc_path, fname, _n=n):
      f = file_of(arc_path, fname)
      if f is None:
        return None
      return bytes_before(f, _n)

    rando = StubRando(inputs, item_ids, Opts)
    items = StubItems(rando, Opts)
    errors = []
    unloaded = set()
    for tw in PLACEMENT_TWEAKS.get(init, []):
      try:
        with contextlib.redirect_stdout(sys.stderr):
          getattr(tweaks, tw)(rando)
      except KeyError as e:
        unloaded.add(str(e))
    held = []   # (arc_path, dzname, chunk class, layer, index, item id) to re-read afterwards
    for name, path, k in paths:
      _, lay, index, actor, action, arcfile, evname, _ = k
      arc_path = "files/res/Stage/" + arcfile
      item = placed.get(name, locs[name]["Original item"])
      layer = DZxLayer(None if lay < 0 else f"{lay:x}")
      dzname = "stage.dzs" if arcfile.endswith("Stage.arc") else "room.dzr"
      try:
        with contextlib.redirect_stdout(sys.stderr):
          if kind == "Chest":
            dzx = rando.get_arc(arc_path).get_file(dzname, DZx)
            if index >= len(dzx.entries_by_type_and_layer(TRES, layer=layer)):
              skipped["added chest (R2.10, a tweak step)"] += 1
              continue
            ItemRandomizer.change_chest_item(items, arc_path, index, layer, item)
            held.append((arc_path, dzname, TRES, layer, index, item_ids[item], path))
          elif kind == "Actor":
            dzx = rando.get_arc(arc_path).get_file(dzname, DZx)
            if dzx.entries_by_type_and_layer(ACTR, layer=layer)[index].actor_class_name == "d_a_boss_item":
              # wwrando writes the heart container into Bitem's params and patches d_a_boss_item to read
              # it (flexible_item_locations.asm); the port's daBossItem_Create / fopAcM_createItemForBoss
              # hunks take it from the placement by stage number (RANDO_HOOKS.md), so the file stays
              # vanilla: a code location here.
              skipped["boss item (code: pc_rando_boss_item)"] += 1
              continue
            ItemRandomizer.change_actor_item(items, arc_path, index, layer, item)
            held.append((arc_path, dzname, ACTR, layer, index, item_ids[item], path))
          elif kind == "ScalableObject":
            ItemRandomizer.change_scob_item(items, arc_path, index, layer, item)
            held.append((arc_path, dzname, SCOB, layer, index, item_ids[item], path))
          else:
            ItemRandomizer.change_event_item(items, arc_path, index, actor, action, item)
      except KeyError as e:
        unloaded.add(str(e))
      except Exception as e:  # noqa: BLE001
        errors.append(f"{path}: {type(e).__name__}: {e}")
    c.check(f"placement step {n} {init}: wwrando's change_item ran for every {kind} path", not errors,
            "; ".join(errors[:5]))
    # Compare every file the oracle touched with the step's output.
    touched = set()
    diffs = []
    for arc_path, sa in rando.arcs.items():
      for fname, obj in sa.files.items():
        f = file_of(arc_path, fname)
        touched.add((f["dir"], f["arc"], f["name"]))
        actual = bytes_after(f, n)
        label = f"{f['dir']}{f['arc']}/{fname}"
        if actual is None:
          diffs.append(f"{label}: changed by the step but not dumped (run without COS_RANDO_IDHASH_DUMP?)")
          continue
        if isinstance(obj, EventList):
          exp = event_dump(obj)
          act = event_dump(EventList(file_entry(actual)))
          if exp != act:
            ks = [kk for kk in sorted(set(exp) | set(act), key=str) if exp.get(kk) != act.get(kk)]
            diffs.append(f"{label}: " + "; ".join(f"{kk[1]}/{kk[3]}/{kk[4]} wwrando {exp.get(kk)} port "
                                                  f"{act.get(kk)}" for kk in ks[:5]))
        else:
          exp_b = obj.data.getvalue()
          if exp_b != actual:
            diffs.append(f"{label}: " + "; ".join(dzx_diff(exp_b, actual)[:5]))
    # Files the step changed but wwrando would not touch.
    stray = [k for k in per_step.get(init, []) if k[0] != "res" or (k[1], k[2], k[3]) not in touched]
    for k in stray:
      diffs.append(f"{k[1]}{k[2]}/{k[3]}: changed by the step, not by wwrando's change_item")
    c.check(f"placement step {n} {init}: the step's bytes = wwrando's on {len(touched)} files "
            f"({len(per_step.get(init, []))} changed)", not diffs, " | ".join(diffs[:8]))
    # Every record the location paths name still holds the location's vanilla item.
    wrong = []
    for arc_path, dzname, cls, layer, index, iid, path in held:
      f = file_of(arc_path, dzname)
      actual = bytes_after(f, n) if f else None
      if actual is None:
        continue
      ent = DZx(file_entry(actual)).entries_by_type_and_layer(cls, layer=layer)[index]
      if record_item(ent) != iid:
        wrong.append(f"{path} holds {record_item(ent):#x}, not {iid:#x}")
    if kind != "Event":
      c.check(f"placement step {n} {init}: every {kind} record holds its vanilla item after the step",
              not wrong, "; ".join(wrong[:5]))
    if unloaded:
      c.note(f"{init}: {len(unloaded)} files not loaded by this run (not checked)")
    if skipped:
      c.note(f"{init}: skipped " + ", ".join(f"{v} {k}" for k, v in skipped.items()))
    for k in per_step.get(init, []):
      c.note(f"{init} changed {k[1]}{k[2]}/{k[3]} (wwrando re-encodes the same item there)")
  return c


def dzx_diff(a, b):
  """Record-level differences of two DZx files, for messages."""
  import struct

  def chunks(x):
    out = {}
    try:
      n = struct.unpack_from(">I", x, 0)[0]
      for i in range(n):
        t, cnt, off = struct.unpack_from(">4sII", x, 4 + 12 * i)
        out[t.decode("latin1")] = (cnt, off)
    except struct.error:
      pass
    return out
  ca, cb = chunks(a), chunks(b)
  out = []
  if len(a) != len(b):
    out.append(f"size {len(a)} != {len(b)}")
  for t in sorted(set(ca) | set(cb)):
    if ca.get(t, (None,))[0] != cb.get(t, (None,))[0]:
      out.append(f"{t} count {ca.get(t, (0,))[0]} != {cb.get(t, (0,))[0]}")
  if not out:
    first = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), None)
    out.append(f"first differing byte at {first:#x}" if first is not None else "equal")
  return out


# ---------------------------------------------------------------------------------------------
# Saves

def summary(path):
  out = {}
  if not os.path.isfile(path):
    return None
  for line in open(path, errors="replace"):
    line = line.rstrip("\n")
    if line.startswith("#") or not line.strip():
      continue
    key, _, value = line.partition(" ")
    if key == "region":
      name, _, value = value.partition(" ")
      key = "region " + name
    out[key] = value
  return out


def compare_saves(van_dir, id_dir):
  lines = []
  c = Checker(lines)
  for label, d in (("vanilla", van_dir), ("identity", id_dir)):
    rep = os.path.join(d, "save_sweep.txt")
    txt = open(rep).read() if os.path.isfile(rep) else ""
    m = re.search(r"# (\d+) of (\d+) cases saved and loaded back equal", txt)
    c.check(f"{label} save sweep: every case saved and loaded back equal", bool(m) and m.group(1) == m.group(2),
            m.group(0)[2:] if m else f"no report {rep}")
  cases = sorted(x for x in os.listdir(van_dir) if os.path.isdir(os.path.join(van_dir, x)))
  id_cases = sorted(x for x in os.listdir(id_dir) if os.path.isdir(os.path.join(id_dir, x)))
  c.check("every identity case has its vanilla case", set(id_cases) <= set(cases),
          ", ".join(sorted(set(id_cases) - set(cases))))
  if set(cases) - set(id_cases):
    c.note("vanilla cases without an identity run (not compared): " + ", ".join(sorted(set(cases) - set(id_cases))))
  pairs = {}
  for case in sorted(set(cases) & set(id_cases)):
    a = summary(os.path.join(van_dir, case, "save", "save_expect.txt"))
    b = summary(os.path.join(id_dir, case, "save", "save_expect.txt"))
    if not c.check(f"{case}: both saves wrote save_expect.txt", a is not None and b is not None):
      continue
    pairs[case] = (a, b)
  compare_new_file_state(c, pairs)
  return c


# A randomized file is not a vanilla file even with vanilla items: its new game runs wwrando's
# init_save_with_tweaks / init_starting_gear / reveal_sea_chart (R3.1, R3.2, R6.4,
# pc_rando_new_game_init) and the slot gets its record (D3). Under the identity placement nothing
# else may differ, and that new-file state cannot depend on where or what the game saved: so every
# difference must be the same state N in every case. Flag regions: identity = vanilla | N (no bit
# cleared); other regions: at N's positions identity = N, elsewhere identity = vanilla. STRICT_KEYS
# must be equal.
FLAG_REGIONS = {"region event", "region memory", "region map", "region collect", "region get_item",
                "region get_bag_item", "region ocean", "region status_c", "region priest"}
STRICT_KEYS = {"return", "region return_place", "region info", "region config"}
DERIVED_KEYS = {"items": "region item"}  # summary lines repeating a region


def _hexbytes(v):
  v = re.sub(r"\([a-z]+\)", "", v or "")
  if len(v) % 2:
    v += "0"  # the summary cuts a long region at an odd number of digits
  try:
    return bytes.fromhex(v)
  except ValueError:
    return None


def compare_new_file_state(c, pairs):
  if not pairs:
    c.check("save cases compared", False, "none")
    return
  keys = sorted(set().union(*(set(a) | set(b) for a, b in pairs.values())))
  for case, (a, b) in pairs.items():
    rec = _hexbytes(b.get("region reserve"))
    c.check(f"{case}: the identity save holds the randomizer record (reserve starts with WWR1)",
            rec is not None and rec[:4] == b"WWR1", (b.get("region reserve") or "-")[:16])
    c.check(f"{case}: the vanilla save holds no randomizer record",
            (_hexbytes(a.get("region reserve")) or b"\0")[:4] != b"WWR1")
  state = []
  for key in keys:
    if key == "region reserve" or key in DERIVED_KEYS:
      continue
    diff_cases = [cs for cs, (a, b) in pairs.items() if a.get(key) != b.get(key)]
    if not diff_cases:
      continue
    if key in STRICT_KEYS:
      c.check(f"{key}: equal both ways", False, "; ".join(
        f"{cs}: {pairs[cs][0].get(key)} -> {pairs[cs][1].get(key)}" for cs in diff_cases[:4]))
      continue
    vals = {cs: (_hexbytes(a.get(key)), _hexbytes(b.get(key))) for cs, (a, b) in pairs.items()}
    if any(v is None or w is None or len(v) != len(w) for v, w in vals.values()):
      # A text line (e.g. "status life .. magic .."): token by token, as a value region.
      vals = {cs: ((a.get(key) or "").split(), (b.get(key) or "").split()) for cs, (a, b) in pairs.items()}
      if any(len(v) != len(w) for v, w in vals.values()):
        c.check(f"{key}: same shape both ways", False)
        continue
      flag = False
    else:
      flag = key in FLAG_REGIONS
    bad = []
    if flag:
      n = bytearray(len(next(iter(vals.values()))[0]))
      for v, w in vals.values():
        for i in range(len(v)):
          n[i] |= w[i] & ~v[i] & 0xFF
      for cs, (v, w) in vals.items():
        cleared = [i for i in range(len(v)) if v[i] & ~w[i] & 0xFF]
        other = [i for i in range(len(v)) if w[i] != (v[i] | n[i])]
        if cleared or other:
          bad.append(f"{cs}: bytes {sorted(set(cleared + other))[:8]}")
      desc = f"{sum(bin(x).count('1') for x in n)} bits set"
    else:
      n = {}
      for v, w in vals.values():
        for i in range(len(v)):
          if v[i] != w[i]:
            n.setdefault(i, set()).add(w[i])
      for i, ws in sorted(n.items()):
        if len(ws) > 1:
          bad.append(f"position {i}: identity values {sorted(ws)} differ between cases")
      for cs, (v, w) in vals.items():
        wrong = [i for i in range(len(v)) if (w[i] not in n[i] if i in n else w[i] != v[i])]
        if wrong:
          bad.append(f"{cs}: positions {wrong[:8]}")
      desc = f"{len(n)} positions"
    state.append(f"{key} ({desc}, {len(diff_cases)} of {len(pairs)} cases)")
    c.check(f"{key}: the differences are one new-file state in every case", not bad, "; ".join(bad[:4]))
  c.note("new-file state (the same in every case): " + (", ".join(state) or "none"))


# ---------------------------------------------------------------------------------------------

def load_save_sweep():
  import importlib.util
  spec = importlib.util.spec_from_file_location("save_sweep", SAVE_SWEEP)
  mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(mod)
  return mod


# save_sweep.py's cases, run the same way both ways, adapted to states a randomized file reaches.
# A randomized new file always has RODE_KORL (0x2A08, pc_rando_new_game_init: wwrando's
# init_save_with_tweaks) and starts on Outset's dock, spawn 0 (R3.2 skip_wakeup_intro_and_start_at_dock),
# never at spawn 206 (Aryll's lookout and the "awake" event, whose demo archive the randomized file's
# Outset layer does not load: booting a randomized file there stops at d_event_data.cpp:1070). So:
# outset boots the dock with RODE_KORL (the vanilla "new file returns to Outset" case has no
# randomized counterpart), outset-rode is dropped (the same state now), and items gets RODE_KORL so
# both ways take the same return-place branch (dComIfGs_setGameStartStage).
def rando_cases(ss):
  cases = {}
  for name, (desc, smoke, env) in ss.CASES.items():
    env = dict(env)
    if name == "outset-rode":
      continue
    if name == "outset":
      desc = "Outset's dock (spawn 0, the randomizer's new-game start) with RODE_KORL"
      env.update({"COS_BOOT_STAGE": "sea:44:0", "COS_BOOT_EVENTS": ss.RODE_KORL})
    if name == "items":
      env["COS_BOOT_EVENTS"] = ss.RODE_KORL
    cases[name] = (desc, smoke, env)
  return cases


# The identity placement's file starts with the Hero's Sword and a Progressive Shield (its sword_mode
# and starting_gear, R3.1). The randomizer makes the sword and shield progressive (wwrando's
# progressive item patches), so the items case giving Hero's Sword 0x38 and Hero's Shield 0x3B again
# upgrades them to the Master Sword and the Mirror Shield, with their collect bits and events: a
# state that depends on the case, not the new file. Both ways therefore sweep the items without the
# two (no identity seed has either in a chest, they are starting gear).
def rando_items(ss):
  out = []
  for part in ss.DEFAULT_ITEMS.split(","):
    a, _, b = part.partition("-")
    lo, hi = int(a, 0), int(b or a, 0)
    ids = [i for i in range(lo, hi + 1) if i not in (0x38, 0x3B)]
    while ids:  # re-pack consecutive runs
      j = 0
      while j + 1 < len(ids) and ids[j + 1] == ids[j] + 1:
        j += 1
      out.append(f"0x{ids[0]:02X}" if j == 0 else f"0x{ids[0]:02X}-0x{ids[j]:02X}")
      ids = ids[j + 1:]
  return ",".join(out)


def sweep_main(argv):
  ss = load_save_sweep()
  ss.CASES = rando_cases(ss)
  ss.DEFAULT_ITEMS = rando_items(ss)
  sys.argv = [SAVE_SWEEP] + argv
  return ss.main()


def main(argv=None):
  argv = sys.argv[1:] if argv is None else argv
  if argv[:1] == ["_sweep"]:  # internal: save_sweep.py with rando_cases()
    return sweep_main(argv[1:])
  ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
  ap.add_argument("mode", choices=["run", "check", "saves", "saves-compare"])
  ap.add_argument("cases", nargs="*", help="saves: save_sweep.py cases (default all)")
  ap.add_argument("--out")
  ap.add_argument("--only", default="")
  ap.add_argument("--stage", default="Asoko:0:0:2")
  ap.add_argument("--placement", default=DEFAULT_PLACEMENT)
  ap.add_argument("--disc", default=DEFAULT_DISC)
  ap.add_argument("--exe")
  ap.add_argument("--identity")
  ap.add_argument("--vanilla")
  ap.add_argument("--dump")
  ap.add_argument("--write-tweaks", action="store_true")
  args = ap.parse_args(argv)
  args.placement = os.path.abspath(args.placement)
  if not os.path.isfile(args.placement):
    oc.die(f"placement not found: {args.placement}")
  if not os.path.isfile(args.disc):
    oc.die(f"disc not found: {args.disc} (pass --disc or set COS_DISC)")
  out = os.path.abspath(args.out or os.path.join(REPO, "build", "rando-identity", time.strftime("%Y%m%d-%H%M%S")))

  if args.mode in ("run", "check"):
    if args.mode == "run":
      os.makedirs(out, exist_ok=True)
      identity, vanilla, dump = (os.path.join(out, x) for x in ("identity", "vanilla", "dump"))
      for d in (identity, vanilla, dump):
        shutil.rmtree(d, ignore_errors=True)
      run_smoke(identity, args.placement, args.only, args.stage, args.exe, args.disc, dump)
      run_smoke(vanilla, None, args.only, args.stage, args.exe, args.disc, None)
    else:
      if not args.identity or not args.vanilla:
        oc.die("check needs --identity RUN and --vanilla RUN")
      identity, vanilla = os.path.abspath(args.identity), os.path.abspath(args.vanilla)
      dump = os.path.abspath(args.dump) if args.dump else os.path.join(os.path.dirname(identity), "dump")
      os.makedirs(out, exist_ok=True)
    c = check_archives(args, identity, vanilla, dump)
    report = os.path.join(out, "identity_check.txt")
    head = [f"# identity_check.py (R9.2): placement {os.path.relpath(args.placement, REPO)}, "
            f"identity run {identity}, vanilla run {vanilla}", f"# {c.fails} check(s) failed"]
    with open(report, "w") as f:
      f.write("\n".join(head + c.lines) + "\n")
    print("\n".join(c.lines))
    print(f"identity_check: {'PASS' if c.fails == 0 else 'FAIL'} ({c.fails} failed); report {report}")
    return 0 if c.fails == 0 else 1

  if args.mode == "saves":
    os.makedirs(out, exist_ok=True)
    van, ide = os.path.join(out, "saves-vanilla"), os.path.join(out, "saves-identity")
    for d, extra in ((van, {}), (ide, {"COS_RANDO_PLACEMENT": args.placement})):
      shutil.rmtree(d, ignore_errors=True)
      env = dict(os.environ, COS_DISC=args.disc)
      env.pop("COS_RANDO_PLACEMENT", None)
      env.update(extra)
      cmd = (["python3", "-I", os.path.abspath(__file__), "_sweep", "--out", d]
             + (["--exe", args.exe] if args.exe else []) + args.cases)
      log("identity_check: " + " ".join(cmd[2:]) + (" (identity)" if extra else " (vanilla)"))
      subprocess.run(cmd, env=env, cwd=REPO, stdout=subprocess.DEVNULL)
  else:
    if not args.identity or not args.vanilla:
      oc.die("saves-compare needs --vanilla DIR and --identity DIR")
    van, ide = os.path.abspath(args.vanilla), os.path.abspath(args.identity)
    os.makedirs(out, exist_ok=True)
  c = compare_saves(van, ide)
  report = os.path.join(out, "saves_check.txt")
  with open(report, "w") as f:
    f.write(f"# identity_check.py saves (R9.2): vanilla {van}, identity {ide}\n# {c.fails} check(s) failed\n")
    f.write("\n".join(c.lines) + "\n")
  print("\n".join(c.lines))
  print(f"identity_check saves: {'PASS' if c.fails == 0 else 'FAIL'} ({c.fails} failed); report {report}")
  return 0 if c.fails == 0 else 1


if __name__ == "__main__":
  sys.exit(main())
