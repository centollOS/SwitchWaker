#!/usr/bin/env python3
"""palette_check.py - checks step R7.2c's palette textures and rat-hole tweak against wwrando on the disc.

    python3 -I native/tools/rando/palette_check.py [--disc GZLE01.iso] [--shift H,V | --vanilla]
                                                   [--dump FILE | --run]

Builds, from the user's disc, the lines the palettes smoke dumps
(native/src/pc/harness/smokes/pc_rando_smoke_palettes.cpp): for every palette group's
/res/Object/<RARC>.arc (wwrando's data/palette_randomizable_files.txt, group order, each archive once)
and Nzg.arc, one line per texture header of every .bdl .bmd .bmt .brk .bti file,

    tex <arc> <file> <index> <name|-> <format> <palette format> img <md5> pal <md5|->
    tex <arc> <file> <index> <name> toon

and `rathole Nzg kana_00.bdl cull <u32 at 0xC80> gen <u8 at 0xFCE>`. With a shift (default 120,-20,
the smoke's COS_RANDO_PALETTE_TEST: every group gets it) the files are first recoloured by wwrando
1.10.0's own PaletteRandomizer.shift_all_colors_in_tex1 / shift_all_colors_in_bti (randomizers/
palettes.py, in its group and file order, excluded files skipped, then J3D.save / BTI.save_changes
with gclib) and tweaks.make_rat_holes_visible_from_behind runs; then every file is read back with
gclib and hashed. --vanilla does neither (the disc as it is: what a game without pc_rando() holds).
One known difference is not a port error: gclib's TEX1 save makes a later texture with the same
format and name as an earlier one, but another size, read the earlier one's data (gclib_aliases;
one header on the disc, Btd.arc namakubi.bmd #2); the port keeps the disc's bytes there, so those
headers are compared with the disc's lines and listed as artifacts.

--dump FILE  compares those lines with a dump the smoke wrote (exit 1 on any difference).
--run        runs the smoke first (native/tools/run.sh rando --stage Fairy01:0:0 with
             COS_RANDO_SMOKE=palettes and, unless --vanilla, COS_RANDO_FORCE=1 and
             COS_RANDO_PALETTE_TEST=H,V; COS_PALETTE_DUMP in a temporary folder), requires its exit 0,
             then compares its dump.
Without either, prints the lines.

The disc defaults to $COS_DISC, else ~/Documents/windwaker/GZLE01.iso. Nothing read from the disc is
written to the repository (plan R7 intro: compared at check time, no disc bytes committed).
Exit codes: 0 equal, 1 different or the smoke failed, 2 usage or setup error.
Uses the oracle venv (setup_oracle.sh) through oracle_common.bootstrap().
"""
import argparse
import contextlib
import hashlib
import os
import subprocess
import sys
import tempfile
from io import BytesIO

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from ruamel.yaml import YAML  # noqa: E402
from gclib import fs_helpers as fs  # noqa: E402
from gclib.bti import BTI  # noqa: E402
from gclib.gcm import GCM  # noqa: E402
from gclib.j3d import J3D  # noqa: E402
from gclib.rarc import RARC  # noqa: E402
import tweaks  # noqa: E402
from randomizers.palettes import PaletteRandomizer  # noqa: E402

EXTS = [".bdl", ".bti", ".bmd", ".bmt", ".brk"]
RAT_ARC, RAT_FILE = "Nzg", "kana_00.bdl"


class Disc:
  """Just enough of WWRandomizer for the palette and rat-hole code (self.get_arc)."""

  def __init__(self, iso):
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    self.arcs = {}

  def get_arc(self, path):
    if path not in self.arcs:
      self.arcs[path] = RARC(self.gcm.read_file_data(path))
    return self.arcs[path]


def load_groups():
  with open(os.path.join(oc.WWRANDO_DIR, "data", "palette_randomizable_files.txt")) as f:
    return YAML(typ="safe").load(f)


def arc_list(groups):
  arcs = []
  for g in groups:
    for r in g["RARCs"]:
      if r["Name"].lower() not in [a.lower() for a in arcs]:
        arcs.append(r["Name"])
  arcs.append(RAT_ARC)
  return arcs


def recolor(disc, groups, h, v):
  """shift_colors_for_file_group's RARC loop, textures only (TEX1 and BTI; R7.2d adds the rest)."""
  pr = PaletteRandomizer.__new__(PaletteRandomizer)  # its shift_* methods use no state
  for g in groups:
    for rarc_data in g["RARCs"]:
      rarc = disc.get_arc("files/res/Object/%s.arc" % rarc_data["Name"])
      for file_entry in rarc.file_entries:
        file_name = file_entry.name
        _, ext = os.path.splitext(file_name)
        if ext not in EXTS or file_name in rarc_data["Excluded files"]:
          continue
        if ext == ".bti":
          bti = rarc.get_file(file_name, BTI)
          pr.shift_all_colors_in_bti(file_name, bti, h, v)
          bti.save_changes()
        else:
          j3d = rarc.get_file(file_name, J3D)
          if j3d.tex1 is not None:
            pr.shift_all_colors_in_tex1(file_name, j3d.tex1, h, v)
          j3d.save()


def md5(b):
  return hashlib.md5(b).hexdigest()


def texture_line(arc, file_name, index, name, tex):
  nm = name if name else "-"
  if "toon" in nm:
    return "tex %s %s %d %s toon" % (arc, file_name, index, nm)
  img = md5(tex.image_data.getvalue())
  pal = md5(tex.palette_data.getvalue()) if tex.needs_palettes() else "-"
  return "tex %s %s %d %s %d %d img %s pal %s" % (arc, file_name, index, nm, tex.image_format.value,
                                                   tex.palette_format.value, img, pal)


def file_lines(arc, file_entry):
  """[(key, line)] of one file; key (arc, file, index) for textures, (arc, file, "rathole")."""
  file_name = file_entry.name
  data = BytesIO(fs.read_all_bytes(file_entry.data))
  lines = []
  if file_name.endswith(".bti"):
    lines.append(((arc, file_name, 0), texture_line(arc, file_name, 0, "", BTI(data))))
  else:
    j3d = J3D(data)
    if j3d.tex1 is not None:
      for i, tex in enumerate(j3d.tex1.textures):
        lines.append(((arc, file_name, i), texture_line(arc, file_name, i, j3d.tex1.texture_names[i], tex)))
  if arc == RAT_ARC and file_name == RAT_FILE:
    lines.append(((arc, file_name, "rathole"), "rathole %s %s cull %d gen %d"
                  % (arc, file_name, fs.read_u32(data, 0xC80), fs.read_u8(data, 0xFCE))))
  return lines


def gclib_aliases(arc, file_entry):
  """Texture keys gclib's TEX1 save points at another texture's data although their sizes differ.

  TEX1.save_chunk_specific_data writes one image per (format, name) and points every later header
  with the same pair at it, so a second texture of that name with another width, height or mipmap
  count (Btd.arc namakubi.bmd: two IA4 "base02", 64x32 and 128x64) reads the first one's bytes and
  whatever follows them in gclib's layout. The port keeps the disc's layout, where that texture keeps
  its own bytes: those headers are compared with the disc's bytes instead (reported as artifacts)."""
  if file_entry.name.endswith(".bti"):
    return set()
  j3d = J3D(BytesIO(fs.read_all_bytes(file_entry.data)))
  if j3d.tex1 is None:
    return set()
  first = {}
  out = set()
  for i, tex in enumerate(j3d.tex1.textures):
    key = (tex.image_format.value, j3d.tex1.texture_names[i])
    shape = (tex.width, tex.height, tex.mipmap_count)
    if key in first and first[key] != shape:
      out.add((arc, file_entry.name, i))
    first.setdefault(key, shape)
  return out


def collect(disc, arcs, with_aliases=False):
  lines, aliases = [], set()
  for arc in arcs:
    rarc = disc.get_arc("files/res/Object/%s.arc" % arc)
    for file_entry in rarc.file_entries:
      if file_entry.is_dir or os.path.splitext(file_entry.name)[1] not in EXTS:
        continue
      lines += file_lines(arc, file_entry)
      if with_aliases:
        aliases |= gclib_aliases(arc, file_entry)
  return lines, aliases


def expected(iso, shift):
  """The lines, and the gclib relayout artifacts (lines taken from the disc instead)."""
  disc = Disc(iso)
  groups = load_groups()
  arcs = arc_list(groups)
  vanilla, aliases = collect(disc, arcs, with_aliases=True)
  if shift is None:
    return [l for _, l in vanilla], []
  with contextlib.redirect_stdout(sys.stderr):
    recolor(disc, groups, shift[0], shift[1])
    tweaks.make_rat_holes_visible_from_behind(disc)
  shifted, _ = collect(disc, arcs)
  by_key = dict(vanilla)
  lines, artifacts = [], []
  for key, line in shifted:
    if key in aliases:
      artifacts.append(line)
      line = by_key[key]
    lines.append(line)
  return lines, artifacts


def run_smoke(dump, iso, shift):
  env = dict(os.environ)
  env["COS_DISC"] = os.path.abspath(iso)
  cmd = [os.path.join(oc.REPO_ROOT, "native", "tools", "run.sh"), "rando", "--stage", "Fairy01:0:0",
         "--env", "COS_RANDO_SMOKE=palettes", "--env", "COS_PALETTE_DUMP=%s" % dump]
  if shift is not None:
    cmd += ["--env", "COS_RANDO_FORCE=1", "--env", "COS_RANDO_PALETTE_TEST=%d,%d" % shift]
  print("palette_check: " + " ".join(cmd), file=sys.stderr)
  return subprocess.run(cmd, cwd=oc.REPO_ROOT, env=env).returncode


def parse_shift(s):
  try:
    h, v = (int(x) for x in s.split(","))
  except ValueError:
    oc.die("--shift wants H,V (two integers)")
  return (h, v)


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--disc", default=os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso"))
  m = ap.add_mutually_exclusive_group()
  m.add_argument("--shift", default="120,-20", help="the test shift H,V (default 120,-20)")
  m.add_argument("--vanilla", action="store_true", help="no shift, no rat-hole tweak (no pc_rando())")
  g = ap.add_mutually_exclusive_group()
  g.add_argument("--dump", help="the smoke's dump to compare")
  g.add_argument("--run", action="store_true", help="run the palettes smoke and compare its dump")
  args = ap.parse_args()
  if not os.path.isfile(args.disc):
    oc.die("disc not found: %s (--disc or COS_DISC)" % args.disc)
  shift = None if args.vanilla else parse_shift(args.shift)

  want, artifacts = expected(args.disc, shift)
  for l in artifacts:
    print("gclib relayout artifact (compared with the disc's bytes): " + l, file=sys.stderr)
  if not args.dump and not args.run:
    print("\n".join(want))
    return 0

  tmp = None
  dump = args.dump
  if args.run:
    tmp = tempfile.TemporaryDirectory()
    dump = os.path.join(tmp.name, "palettes.txt")
    code = run_smoke(dump, args.disc, shift)
    if code != 0:
      print("palette_check: the palettes smoke exited %d" % code, file=sys.stderr)
      return 1
  try:
    with open(dump) as f:
      got = [l.strip() for l in f if l.strip()]
  except OSError as e:
    oc.die("cannot read the dump: %s" % e)

  missing = [l for l in want if l not in set(got)]
  unexpected = [l for l in got if l not in set(want)]
  for l in missing[:40]:
    print("missing    " + l)
  for l in unexpected[:40]:
    print("unexpected " + l)
  ok = sorted(got) == sorted(want)
  print("palette_check: %d lines (%s; %d gclib relayout artifacts), %d missing, %d unexpected: %s wwrando on %s"
        % (len(want), "vanilla" if shift is None else "shift %d,%d" % shift, len(artifacts), len(missing),
           len(unexpected), "equal to" if ok else "DIFFERENT from", os.path.basename(args.disc)))
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
