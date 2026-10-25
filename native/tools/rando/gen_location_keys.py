#!/usr/bin/env python3
"""gen_location_keys.py - the runtime key table of the 320 item locations (plan step R0.5).

    python3 -I native/tools/rando/gen_location_keys.py [--iso GZLE01.iso]           # (re)write
    python3 -I native/tools/rando/gen_location_keys.py [--iso GZLE01.iso] --check   # exit 1 if stale
    python3 -I native/tools/rando/gen_location_keys.py --disc GZLP01 [--iso GZLP01.iso]   # EUR compare (R7.6b)

wwrando addresses a location's item by a path (logic/item_locations.txt "Paths", parsed by
randomizers/items.py change_item). The port patches the same data at run time, so it needs each path
as a key the game side can match while an archive loads (D8) or a decompiled function runs. This
script turns every path into one key, in loc_paths() order (key i is path i of R0.3's table):

  DZX    <stage>/<Stage|RoomN>, chunk tag (TRES/TRE0-b, ACTR/ACT0-b, SCOB/SCO0-b) and index in that
         chunk, the item field of the entry's actor class (wwrando data/actor_parameters.txt
         item_id: big-endian word offset, width and mask), and the D9 fingerprint: the 8-byte actor
         name plus the field's byte on the disc.
  EVENT  <stage>/<arc> event_list.dat: event name and index, actor slot, action ordinal, plus the
         resolved global action index, the integer-list slot of the action's prm0 and the vanilla
         action name and prm0 (fingerprint).
  CODE   rels/<x>.rel@offset, main.dol@address or CustomSymbol:<name>; the byte on the disc for
         REL/DOL (CustomSymbols are wwrando's own asm, not on the disc).
  ADDED  a chest wwrando adds (tweaks.py add_chest_in_place_*): stage, arc, TRES index it gets
         (the disc has no TRES entry there) and the added-entity id.

It reads the user's disc (default ~/Documents/windwaker/GZLE01.iso, or $COS_DISC) with gclib and
wwrando's own DZx/EventList readers, so indices mean exactly what change_item means, and it fails
unless every check holds:
  - every location has at least 1 key, and the per-location classes are the locations report's
    (CHEST 166, SCOB 57, ACTR 14, ACTR+CODE 7, EVENT 10, EVENT+CODE 1, CODE 58, CODE+CUSTOM 1,
    CUSTOM 2, ADDED+CODE 4);
  - the 391 on-disc item fingerprints (chests, d_a_item, salvage, dig spot) hold the location's
    original item, the 11 event actions hold it too (prm0, or the song index for 059get_dance), and
    the REL/main.dol bytes hold it except at the 6 boss sites listed in CODE_BYTE_NOT_ITEM;
  - wwrando's DZx item_id attribute agrees with the raw field (checks the masks).

Output (checked in; the build never runs this script, D11):
  native/include/pc/pc_rando_locs.h                         enum PCR_LOC_* (one per location, file
                                                             order) and PCR_ADDED_*; C, game-includable
  native/src/pc/features/rando/generated/rando_location_keys.inc   the key table (pc_rando_lockeys.cpp)

European disc (plan step R7.6b): --disc GZLP01 reads the PAL disc (default
~/Documents/windwaker/GZLP01/GZLP01.iso) with the same checks and compares its keys with the checked-in
table instead of writing it. Every DZX, ADDED and EVENT key (the data the runtime matches while an
archive loads: chunk, index, field, fingerprint name and byte, event and action indices, prm0) must be
the USA key, field for field; then the EUR build uses the same table and the script exits 0. A
differing key is printed and the script exits 1: the fix is a generated rando_location_keys_pal.inc
selected by VERSION (none is needed for GZLP01, which matches). CODE keys only record where wwrando
patches its REL/DOL (USA offsets): the port changes those values in decompiled code, under VERSION
where the PAL code differs, so their PAL bytes are not checked; the script reports how many of the
USA offsets still hold the item on the PAL disc, for information.

Only item ids, actor names and offsets come from the disc (the same facts as the locations report);
no disc bytes are copied. Needs the oracle venv and gclib (native/tools/rando/setup_oracle.sh).
"""
import argparse
import hashlib
import os
import re
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

REPO_ROOT = oc.REPO_ROOT
OUT_INC = os.path.join(REPO_ROOT, "native", "src", "pc", "features", "rando", "generated",
                       "rando_location_keys.inc")
OUT_H = os.path.join(REPO_ROOT, "native", "include", "pc", "pc_rando_locs.h")
DEFAULT_ISO = os.path.expanduser("~/Documents/windwaker/GZLE01.iso")
DEFAULT_ISO_PAL = os.path.expanduser("~/Documents/windwaker/GZLP01/GZLP01.iso")  # R7.6b
DISCS = ("GZLE01", "GZLP01")

# Item field of each actor class that holds a location's item (wwrando data/actor_parameters.txt,
# item_id; d_a_deku_item is first turned into an "item" actor by tweaks.py
# replace_deku_leaf_actor_with_field_item, so its field is d_a_item's). Values:
# (field enum name, big-endian word offset in the entry, word width, mask, disc byte is the item).
FIELDS = {
  "d_a_tbox":        ("TboxItem",    0x1C, 2, 0xFF00,     True),   # z_rot & 0xFF00
  "d_a_item":        ("ItemItem",    0x08, 4, 0x000000FF, True),   # params & 0xFF
  "d_a_boss_item":   ("BossItem",    0x08, 4, 0x0000FF00, False),  # params & 0xFF00 (wwrando's)
  "d_a_tsubo":       ("TsuboItem",   0x1C, 2, 0xFF00,     False),  # z_rot & 0xFF00 (wwrando's)
  "d_a_obj_homen":   ("HomenItem",   0x18, 2, 0x00FF,     False),  # x_rot & 0xFF (wwrando's)
  "d_a_salvage":     ("SalvageItem", 0x08, 4, 0x00000FF0, True),   # params & 0xFF0
  "d_a_tag_kb_item": ("TagKbItem",   0x08, 4, 0x000000FF, True),   # params & 0xFF
  "d_a_deku_item":   ("DekuItem",    0x08, 4, 0x000000FF, False),  # after the swap to "item"
}
CHUNK_BASE = {"Chest": "TRE", "ScalableObject": "SCO", "Actor": "ACT"}
DEFAULT_TAG = {"TRE": "TRES", "SCO": "SCOB", "ACT": "ACTR"}
ENTRY_SIZE = {"TRE": 0x20, "SCO": 0x24, "ACT": 0x20}
CLASS_OF_CHUNK = {"TRE": "CHEST", "SCO": "SCOB", "ACT": "ACTR"}

# Chests wwrando adds (tweaks.py); they are not on the disc.
ADDED = {
  "Pjavdou/Room0.arc/Chest000": ("JABUN_CHEST", "add_chest_in_place_of_jabun_cutscene"),
  "M_Dra09/Stage.arc/Chest000": ("MEDLI_GRAPPLING_HOOK_CHEST", "add_chest_in_place_medli_grappling_hook_gift"),
  "kenroom/Room0.arc/Chest000": ("MASTER_SWORD_CHEST", "add_chest_in_place_of_master_sword"),
  "sea/Room9.arc/Chest000": ("QUEEN_FAIRY_CHEST", "add_chest_in_place_queen_fairy_cutscene"),
}

# Code paths whose disc byte is not the vanilla item: the boss Heart Container sites, where wwrando
# rewrites the fopAcM_createDisappear / fopAcM_createItemForBoss argument (flexible_item_locations.asm).
CODE_BYTE_NOT_ITEM = {
  "rels/d_a_btd.rel@36FE", "rels/d_a_bmd.rel@2B2E", "rels/d_a_bdk.rel@5CCE",
  "rels/d_a_bpw.rel@877E", "rels/d_a_bst.rel@9D43", "rels/d_a_bwd.rel@6D83",
}

# The locations report's per-location classes (plan R0.5 Verify).
EXPECTED_CLASSES = {
  "CHEST": 166, "SCOB": 57, "ACTR": 14, "ACTR+CODE": 7, "EVENT": 10, "EVENT+CODE": 1,
  "CODE": 58, "CODE+CUSTOM": 1, "CUSTOM": 2, "ADDED+CODE": 4,
}
CLASS_ORDER = ["CHEST", "ADDED", "ACTR", "SCOB", "EVENT", "CODE", "CUSTOM"]
EXPECTED_ITEM_FINGERPRINTS = 391


def fail(msg):
  oc.die(msg, code=1)


def enum_name(loc_name):
  s = loc_name.upper().replace("'", "")
  s = re.sub(r"[^A-Z0-9]+", "_", s).strip("_")
  return "PCR_LOC_" + s


class Disc:
  def __init__(self, iso, want="GZLE01"):
    from gclib.gcm import GCM
    from gclib import fs_helpers as fs
    self.fs = fs
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    boot = self.gcm.read_file_data("sys/boot.bin").getvalue()
    self.game_id = boot[:6].decode("latin1")
    if self.game_id != want:
      fail(f"{iso}: game id {self.game_id}, expected {want} ({'USA' if want == 'GZLE01' else 'EUR'})")
    self.arcs = {}
    self.rels = {}
    self.dol = None
    self.digest = hashlib.sha1()  # over every file this run reads, in read order

  def _note(self, path, data):
    self.digest.update(path.encode() + b"\0" + hashlib.sha1(data).digest())

  def arc(self, path):
    if path not in self.arcs:
      from gclib.rarc import RARC
      data = self.gcm.read_file_data(path)
      self._note(path, data.getvalue())
      self.arcs[path] = RARC(data)
      self.arcs[path].read()
    return self.arcs[path]

  def arc_file(self, arc_path, name):
    entry = self.arc(arc_path).get_file_entry(name)
    if entry is None:
      fail(f"{arc_path}: no {name}")
    entry.decompress_data_if_necessary()
    return entry

  def rel(self, rel_path):
    if rel_path not in self.rels:
      from gclib.rel import REL
      name = os.path.basename(rel_path)
      entry = self.arc("files/RELS.arc").get_file_entry(name)
      if entry is not None:
        entry.decompress_data_if_necessary()
        data = entry.data
      else:
        data = self.gcm.read_file_data("files/" + rel_path)
        self._note("files/" + rel_path, data.getvalue())
      rel = REL()
      rel.read(data)
      self.rels[rel_path] = rel
    return self.rels[rel_path]

  def dol_byte(self, address):
    if self.dol is None:
      from gclib.dol import DOL
      data = self.gcm.read_file_data("sys/main.dol")
      self._note("sys/main.dol", data.getvalue())
      self.dol = DOL()
      self.dol.read(data)
    return self.dol.read_data(self.fs.read_u8, address)


def dzx_chunks(raw):
  """{tag: (count, offset)} from the raw dzx header; a tag may appear only once."""
  n = struct.unpack_from(">I", raw, 0)[0]
  out = {}
  for i in range(n):
    tag, count, off = struct.unpack_from(">4sII", raw, 4 + i * 12)
    tag = tag.decode("latin1")
    if tag in out:
      fail(f"chunk {tag} appears twice")
    out[tag] = (count, off)
  return out


def field_value(entry, word_off, width, mask):
  word = struct.unpack_from(">I" if width == 4 else ">H", entry, word_off)[0]
  shift = (mask & -mask).bit_length() - 1
  return (word & mask) >> shift


def main(argv):
  ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
  ap.add_argument("--iso")
  ap.add_argument("--disc", default="GZLE01", choices=DISCS,
                  help="GZLP01: compare the European disc's keys with the table (R7.6b); writes nothing")
  ap.add_argument("--check", action="store_true")
  args = ap.parse_args(argv)
  pal = args.disc == "GZLP01"
  if args.iso is None:
    args.iso = DEFAULT_ISO_PAL if pal else (os.environ.get("COS_DISC") or DEFAULT_ISO)
  if not os.path.isfile(args.iso):
    oc.die(f"disc not found: {args.iso} (pass --iso or set COS_DISC)")
  oc.bootstrap()
  import gen_rando_data as gd  # R0.3's loaders and C helpers (read-only use)
  from wwlib.dzx import DZx, TRES, SCOB, ACTR, DZxLayer
  from wwlib.events import EventList

  locs = gd.yaml_load("logic/item_locations.txt")
  item_ids = {name: iid for iid, name in gd.load_item_names()}
  disc = Disc(args.iso, args.disc)
  chunk_types = {"TRE": TRES, "SCO": SCOB, "ACT": ACTR}

  keys = []          # one per path, loc_paths() order
  loc_rows = []      # (enum, name, classes)
  enum_seen = set()
  n_item_fp = 0
  n_event_fp = 0
  n_code_fp = 0
  n_code_pal_item = 0  # PAL: USA code offsets that still hold the item (information only)
  for loc_index, (name, v) in enumerate(locs.items()):
    orig = item_ids.get(v["Original item"])
    if orig is None:
      fail(f"{name}: unknown original item {v['Original item']}")
    en = enum_name(name)
    if en in enum_seen:
      fail(f"duplicate enum name {en}")
    enum_seen.add(en)
    classes = set()
    for path in v["Paths"]:
      kind, lay, index, actor, action, file, nm, offset = gd.classify_path(path)
      k = {"loc": loc_index, "path": path, "vanilla": orig}
      if kind in CHUNK_BASE:
        base = CHUNK_BASE[kind]
        tag = DEFAULT_TAG[base] if lay < 0 else f"{base}{lay:x}"
        stage, arcfile = file.split("/")
        arcname = arcfile[:-4]
        arc_path = "files/res/Stage/" + file
        dzname = "stage.dzs" if arcname == "Stage" else "room.dzr"
        entry_file = disc.arc_file(arc_path, dzname)
        raw = disc.fs.read_all_bytes(entry_file.data)
        chunks = dzx_chunks(raw)
        k.update(stage=stage, arc=arcname, tag=tag, index=index)
        if path in ADDED:
          count = chunks.get(tag, (0, 0))[0]
          if base != "TRE" or lay >= 0 or count != index:
            fail(f"{path}: added chest expected at TRES[{count}]")
          k.update(kind="Added", added=ADDED[path][0])
          classes.add("ADDED")
        else:
          if tag not in chunks or index >= chunks[tag][0]:
            fail(f"{path}: {tag}[{index}] not on disc")
          off = chunks[tag][1] + index * ENTRY_SIZE[base]
          ent = raw[off:off + ENTRY_SIZE[base]]
          aname = ent[:8].split(b"\0")[0].decode("latin1")
          dzx = disc.arc(arc_path).get_file(dzname, DZx)
          ww_ent = dzx.entries_by_type_and_layer(chunk_types[base], layer=DZxLayer(lay))[index]
          cls = ww_ent.actor_class_name
          if ww_ent.name != aname:
            fail(f"{path}: wwrando entry {ww_ent.name} != raw {aname}")
          if cls not in FIELDS:
            fail(f"{path}: actor {aname} class {cls} holds no item")
          fld, woff, width, mask, is_item = FIELDS[cls]
          byte = field_value(ent, woff, width, mask)
          if cls != "d_a_deku_item" and getattr(ww_ent, "item_id") != byte:
            fail(f"{path}: wwrando item_id {ww_ent.item_id:#x} != raw field {byte:#x}")
          if is_item:
            if byte != orig:
              fail(f"{path}: fingerprint item {byte:#04x} != original {orig:#04x} ({v['Original item']})")
            n_item_fp += 1
          k.update(kind="Dzx", field=fld, word_off=woff, width=width, mask=mask, fp_name=ent[:8],
                   disc_byte=byte, is_item=is_item, cls=cls)
          classes.add(CLASS_OF_CHUNK[base])
      elif kind == "Event":
        stage, arcfile = file.split("/")
        arc_path = "files/res/Stage/" + file
        ef = disc.arc_file(arc_path, "event_list.dat")
        d = disc.fs.read_all_bytes(ef.data)
        el = disc.arc(arc_path).get_file("event_list.dat", EventList)
        ev = el.events[index]
        if ev.name != nm:
          fail(f"{path}: event {index} is {ev.name}")
        ww_action = ev.actors[actor].actions[action]
        u32 = lambda o: struct.unpack_from(">I", d, o)[0]
        s32 = lambda o: struct.unpack_from(">i", d, o)[0]
        eo, ao, aco, po, io = u32(0x00), u32(0x08), u32(0x10), u32(0x18), u32(0x28)
        gactor = s32(eo + index * 0xB0 + 0x2C + actor * 4)
        gact = s32(ao + gactor * 0x50 + 0x30)
        for _ in range(action):
          gact = s32(aco + gact * 0x50 + 0x3C)
        aoff = aco + gact * 0x50
        aname = d[aoff:aoff + 32].split(b"\0")[0].decode("latin1")
        pidx = s32(aoff + 0x38)
        poff = po + pidx * 0x40
        pname = d[poff:poff + 32].split(b"\0")[0].decode("latin1")
        ptype, pdata = u32(poff + 0x24), u32(poff + 0x28)
        if aname != ww_action.name or pname != "prm0" or ptype != 3:
          fail(f"{path}: action {aname} prop {pname} type {ptype}")
        val = s32(io + pdata * 4)
        want = orig - 0x6D if aname.startswith("059") else orig
        if val != want:
          fail(f"{path}: event prm0 {val} != {want}")
        n_event_fp += 1
        k.update(kind="Event", stage=stage, arc=arcfile[:-4], event=nm, event_index=index,
                 actor=actor, action=action, gaction=gact, int_index=pdata, action_name=aname,
                 prm0=val)
        classes.add("EVENT")
      else:
        if kind == "Rel":
          byte = disc.rel(file).read_data(disc.fs.read_u8, offset)
          k.update(kind="Code", src="Rel", file=file, offset=offset, disc_byte=byte)
          classes.add("CODE")
        elif kind == "MainDol":
          byte = disc.dol_byte(offset)
          k.update(kind="Code", src="MainDol", file="main.dol", offset=offset, disc_byte=byte)
          classes.add("CODE")
        else:
          byte = None
          k.update(kind="Code", src="CustomSymbol", file=nm, offset=0, disc_byte=0)
          classes.add("CUSTOM")
        if byte is not None:
          is_item = path not in CODE_BYTE_NOT_ITEM
          if pal:
            # USA offsets: not checked on the PAL disc (see the module doc).
            n_code_pal_item += is_item and byte == orig
          elif is_item and byte != orig:
            fail(f"{path}: code byte {byte:#04x} != original {orig:#04x}")
          if not pal and not is_item and byte == orig:
            fail(f"{path}: listed in CODE_BYTE_NOT_ITEM but holds the item")
          n_code_fp += is_item
          k["is_item"] = is_item
      keys.append(k)
    cl = "+".join(c for c in CLASS_ORDER if c in classes)
    loc_rows.append((en, name, cl, classes))

  # Checks (plan R0.5 Verify).
  from collections import Counter
  got = Counter(r[2] for r in loc_rows)
  if dict(got) != EXPECTED_CLASSES:
    fail(f"location classes {dict(got)} != {EXPECTED_CLASSES}")
  if n_item_fp != EXPECTED_ITEM_FINGERPRINTS:
    fail(f"{n_item_fp} on-disc item fingerprints, expected {EXPECTED_ITEM_FINGERPRINTS}")
  if len(loc_rows) != 320 or len(keys) != 511:
    fail(f"{len(loc_rows)} locations, {len(keys)} keys")

  inc, hdr = render(gd, keys, loc_rows, disc, n_item_fp, n_event_fp, n_code_fp, got)
  if pal:
    return compare_pal(inc, hdr, keys, loc_rows, n_item_fp, n_event_fp, n_code_pal_item, n_code_fp)
  changed = []
  for path, text in ((OUT_INC, inc), (OUT_H, hdr)):
    old = None
    if os.path.exists(path):
      with open(path, encoding="utf-8") as f:
        old = f.read()
    if old != text:
      changed.append(os.path.relpath(path, REPO_ROOT))
      if not args.check:
        with open(path, "w", newline="\n", encoding="utf-8") as f:
          f.write(text)
  print(f"{len(loc_rows)} locations, {len(keys)} keys; classes "
        + ", ".join(f"{c} {got[c]}" for c in EXPECTED_CLASSES)
        + f"; fingerprints: {n_item_fp} dzx items, {n_event_fp} events, {n_code_fp} code bytes"
        + f" (+{len(CODE_BYTE_NOT_ITEM)} boss sites), 0 mismatches")
  if args.check:
    if changed:
      print("out of date: " + ", ".join(changed))
      return 1
    print("up to date")
    return 0
  print(("wrote " + ", ".join(changed)) if changed else "no changes")
  return 0


def _table_lines(text, start):
  """The entry lines of the C array that starts at the line `start` (comments dropped)."""
  lines = text.split("\n")
  i = lines.index(start) + 1
  out = []
  while lines[i] != "};":
    if not lines[i].lstrip().startswith("//"):
      out.append(lines[i])
    i += 1
  return out


def compare_pal(inc, hdr, keys, loc_rows, n_item_fp, n_event_fp, n_code_pal_item, n_code_fp):
  """R7.6b: the PAL disc's keys against the checked-in (USA) table. Exit 0 when every data key and
  the location list are the same."""
  with open(OUT_INC, encoding="utf-8") as f:
    usa_inc = f.read()
  with open(OUT_H, encoding="utf-8") as f:
    usa_h = f.read()
  usa_keys = _table_lines(usa_inc, "static const LocKey kLocKeys[] = {")
  pal_keys = _table_lines(inc, "static const LocKey kLocKeys[] = {")
  bad = []
  if len(usa_keys) != len(pal_keys):
    bad.append(f"{len(pal_keys)} keys, the table has {len(usa_keys)}")
  n_data = n_code = 0
  for k, u, p in zip(keys, usa_keys, pal_keys):
    if k["kind"] == "Code":
      n_code += 1
      # Same location, class and source; the disc byte (the field after "{}, ") is not compared.
      strip = lambda line: re.sub(r"\{\}, 0x[0-9A-F]{2}, ", "{}, _, ", line)
      if strip(u) != strip(p):
        bad.append(f"{k['path']}: code key differs:\n  USA {u.strip()}\n  EUR {p.strip()}")
      continue
    n_data += 1
    if u != p:
      bad.append(f"{k['path']}: key differs:\n  USA {u.strip()}\n  EUR {p.strip()}")
  if _table_lines(usa_inc, "static const LocInfo kLocInfo[] = {") != \
     _table_lines(inc, "static const LocInfo kLocInfo[] = {"):
    bad.append("the per-location classes differ")
  body = lambda t: [l for l in t.split("\n") if not l.startswith("// ")]
  if body(usa_h) != body(hdr):
    bad.append("pc_rando_locs.h differs (location list)")
  print(f"GZLP01: {len(loc_rows)} locations, {len(keys)} keys; fingerprints on the EUR disc: {n_item_fp} dzx "
        f"items, {n_event_fp} events, 0 mismatches")
  print(f"GZLP01: {n_data} DZX/ADDED/EVENT keys compared with the table: {n_data - sum(1 for b in bad if 'code key' not in b)} "
        f"identical; {n_code} CODE keys not compared ({n_code_pal_item} of the {n_code_fp} USA item offsets "
        f"hold the item on the EUR disc; the port patches decompiled code, not these bytes)")
  for b in bad:
    print("GZLP01: " + b)
  if bad:
    print(f"GZLP01: {len(bad)} difference(s): the EUR build needs generated/rando_location_keys_pal.inc")
    return 1
  print("GZLP01: same keys as the USA table (no rando_location_keys_pal.inc needed)")
  return 0


def c_bytes8(b):
  return "{" + ", ".join(f"0x{x:02X}" for x in b) + "}"


def render(gd, keys, loc_rows, disc, n_item_fp, n_event_fp, n_code_fp, classes):
  gen_sha = gd.file_sha1(os.path.abspath(__file__))
  commit = gd.wwrando_commit()
  head = [
    "// GENERATED FILE - do not edit. Regenerate with: python3 -I native/tools/rando/gen_location_keys.py",
    f"// Source: wwrando 1.10.0_9775811 (MIT), commit {commit}: logic/item_locations.txt",
    f"//   sha1 {gd.file_sha1(gd.ww('logic/item_locations.txt'))}, data/actor_parameters.txt classes;",
    f"//   disc {disc.game_id} (offsets, actor names and item ids only), files read sha1 {disc.digest.hexdigest()}",
    f"// Generator: native/tools/rando/gen_location_keys.py sha1 {gen_sha}",
  ]
  inc = list(head)
  inc.append("// Included once, by pc_rando_lockeys.cpp (inside namespace pcr::gen).")
  inc.append(f"// {len(loc_rows)} locations, {len(keys)} keys (key i is loc_paths()[i]); classes: "
             + ", ".join(f"{c} {classes[c]}" for c in EXPECTED_CLASSES) + ".")
  inc.append(f"// Fingerprints checked on the disc: {n_item_fp} dzx items, {n_event_fp} event actions, "
             f"{n_code_fp} code bytes; 0 mismatches.")
  inc.append("")
  inc.append("static const LocKey kLocKeys[] = {")
  for k in keys:
    c = gd.c_str
    loc = loc_rows[k["loc"]][0]
    if k["kind"] == "Dzx":
      flags = "KEY_FP_ITEM" if k["is_item"] else "0"
      body = (f"KeyKind::Dzx, {loc}, 0x{k['vanilla']:02X}, {flags}, {c(k['stage'])}, {c(k['arc'])}, "
              f"{c(k['tag'])}, {k['index']}, DzxField::{k['field']}, 0x{k['word_off']:02X}, {k['width']}, "
              f"0x{k['mask']:08X}u, {c_bytes8(k['fp_name'])}, 0x{k['disc_byte']:02X}, "
              f"nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_NONE")
    elif k["kind"] == "Added":
      body = (f"KeyKind::Added, {loc}, 0x{k['vanilla']:02X}, 0, {c(k['stage'])}, {c(k['arc'])}, "
              f"{c(k['tag'])}, {k['index']}, DzxField::TboxItem, 0x1C, 2, 0x0000FF00u, {{}}, 0, "
              f"nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::None, nullptr, 0, PCR_ADDED_{k['added']}")
    elif k["kind"] == "Event":
      body = (f"KeyKind::Event, {loc}, 0x{k['vanilla']:02X}, KEY_FP_ITEM, {c(k['stage'])}, {c(k['arc'])}, "
              f"nullptr, 0, DzxField::None, 0, 0, 0u, {{}}, 0, "
              f"{c(k['event'])}, {k['event_index']}, {k['actor']}, {k['action']}, {k['gaction']}, "
              f"{k['int_index']}, {c(k['action_name'])}, {k['prm0']}, CodeSource::None, nullptr, 0, PCR_ADDED_NONE")
    else:
      flags = "KEY_FP_ITEM" if k.get("is_item") else "0"
      body = (f"KeyKind::Code, {loc}, 0x{k['vanilla']:02X}, {flags}, nullptr, nullptr, "
              f"nullptr, 0, DzxField::None, 0, 0, 0u, {{}}, 0x{k['disc_byte']:02X}, "
              f"nullptr, 0, 0, 0, 0, 0, nullptr, 0, CodeSource::{k['src']}, {c(k['file'])}, 0x{k['offset']:08X}u, "
              f"PCR_ADDED_NONE")
    inc.append(f"    // {k['path']}")
    inc.append(f"    {{{body}}},")
  inc.append("};")
  inc.append("")
  inc.append("// Per location: PCR_LOC_* name (for tests and logs) and class bits.")
  inc.append("static const LocInfo kLocInfo[] = {")
  bits = {"CHEST": "PCR_CLASS_CHEST", "ADDED": "PCR_CLASS_ADDED", "ACTR": "PCR_CLASS_ACTR",
          "SCOB": "PCR_CLASS_SCOB", "EVENT": "PCR_CLASS_EVENT", "CODE": "PCR_CLASS_CODE",
          "CUSTOM": "PCR_CLASS_CUSTOM"}
  for en, name, cl, cset in loc_rows:
    b = " | ".join(bits[x] for x in CLASS_ORDER if x in cset)
    inc.append(f"    {{{gd.c_str(en)}, {b}}},")
  inc.append("};")
  inc_text = "\n".join(inc) + "\n"

  h = list(head)
  h += [
    "/*",
    " * pc_rando_locs.h - one id per randomizer item location (docs/RANDOMIZER_PLAN.md, step R0.5).",
    " *",
    " * PCR_LOC_<name> is the location's index in wwrando's logic/item_locations.txt (file order), the",
    " * same index as pcr::locations() (R0.3) and PcRandoSeed::locations (R0.6). Item id sites in game/",
    " * use pc_rando_loc_item(PCR_LOC_<name>, vanilla) (pc_rando.h). The runtime keys of each location",
    " * are in features/rando/pc_rando_lockeys.h. Plain C, so game code can include it.",
    " */",
    "#ifndef PC_RANDO_LOCS_H",
    "#define PC_RANDO_LOCS_H",
    "",
    "enum PcRandoLoc {",
  ]
  for i, (en, name, cl, _) in enumerate(loc_rows):
    h.append(f"    {en} = {i}, /* {name} [{cl}] */")
  h += [
    f"    PCR_LOC_COUNT = {len(loc_rows)}",
    "};",
    "",
    "/* Chests wwrando adds that are not on the disc (tweaks.py add_chest_in_place_*); KeyKind::Added. */",
    "enum PcRandoAdded {",
    "    PCR_ADDED_NONE = 0,",
  ]
  for i, (path, (name, fn)) in enumerate(ADDED.items()):
    h.append(f"    PCR_ADDED_{name} = {i + 1}, /* {path} ({fn}) */")
  h += [f"    PCR_ADDED_COUNT = {len(ADDED) + 1}", "};", "", "#endif /* PC_RANDO_LOCS_H */"]
  return inc_text, "\n".join(h) + "\n"


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
