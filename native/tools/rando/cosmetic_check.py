#!/usr/bin/env python3
"""cosmetic_check.py - checks step R7.5a's fairy fountain colours against wwrando on the user's disc.

    python3 -I native/tools/rando/cosmetic_check.py [--disc GZLE01.iso] [--dump FILE | --run]

Runs wwrando 1.10.0's own tweaks.give_fairy_fountains_distinct_colors (ref/wwrando/tweaks.py) with
gclib on the six Fairy01..Fairy06 Stage.arc of the disc, re-reads the saved stage.dzs bytes and prints
every Pale record's bg0_c0 / bg0_k0 in the format of the cosmetic smoke's dump
(native/src/pc/harness/smokes/pc_rando_smoke_cosmetic.cpp):

    pale <stage> <index> bg0_c0 <r> <g> <b> bg0_k0 <r> <g> <b>

--dump FILE  compares those lines with a dump the smoke wrote (exit 1 on any difference).
--run        runs the smoke itself first (native/tools/run.sh rando --stage Fairy01:0:0 with
             COS_RANDO_SMOKE=cosmetic, COS_RANDO_FORCE=1 and COS_COSMETIC_DUMP in a temporary folder),
             requires its exit 0, then compares its dump.

The disc defaults to $COS_DISC, else ~/Documents/windwaker/GZLE01.iso. Nothing read from the disc is
written to the repository (plan R7 intro: compared at check time, no disc bytes committed).
Exit codes: 0 equal, 1 different or the smoke failed, 2 usage or setup error.
Uses the oracle venv (setup_oracle.sh) through oracle_common.bootstrap().
"""
import argparse
import contextlib
import os
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib import fs_helpers as fs  # noqa: E402
from gclib.gcm import GCM  # noqa: E402
from gclib.rarc import RARC  # noqa: E402
from wwlib.dzx import DZx  # noqa: E402
import tweaks  # noqa: E402

STAGES = ["Fairy01", "Fairy02", "Fairy03", "Fairy04", "Fairy05", "Fairy06"]
PALE_SIZE = 0x2C


class Disc:
  """Just enough of WWRandomizer for tweaks.give_fairy_fountains_distinct_colors (self.get_arc)."""

  def __init__(self, iso):
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    self.arcs = {}

  def get_arc(self, path):
    if path not in self.arcs:
      self.arcs[path] = RARC(self.gcm.read_file_data(path))
    return self.arcs[path]


def pale_lines(stage, data):
  """The Pale records of raw stage.dzs bytes (the chunk table read directly, as the game does)."""
  lines = []
  count = fs.read_u32(data, 0)
  for c in range(count):
    tag = fs.read_str(data, 4 + c * 12, 4)
    if tag != "Pale":
      continue
    n = fs.read_u32(data, 4 + c * 12 + 4)
    off = fs.read_u32(data, 4 + c * 12 + 8)
    for i in range(n):
      e = fs.read_bytes(data, off + i * PALE_SIZE, PALE_SIZE)
      lines.append("pale %s %d bg0_c0 %d %d %d bg0_k0 %d %d %d" % ((stage, i) + tuple(e[6:12])))
    break
  return lines


def expected(iso):
  disc = Disc(iso)
  with contextlib.redirect_stdout(sys.stderr):
    tweaks.give_fairy_fountains_distinct_colors(disc)
  lines = []
  for stage in STAGES:
    dzs = disc.get_arc("files/res/Stage/%s/Stage.arc" % stage).get_file("stage.dzs", DZx)
    # give_fairy_fountains_distinct_colors called save_changes(): dzs.data holds the bytes _save writes.
    lines += pale_lines(stage, dzs.data)
  return lines


def run_smoke(dump, iso):
  env = dict(os.environ)
  env["COS_DISC"] = os.path.abspath(iso)
  cmd = [os.path.join(oc.REPO_ROOT, "native", "tools", "run.sh"), "rando", "--stage", "Fairy01:0:0",
         "--env", "COS_RANDO_SMOKE=cosmetic", "--env", "COS_RANDO_FORCE=1",
         "--env", "COS_COSMETIC_DUMP=%s" % dump]
  print("cosmetic_check: " + " ".join(cmd), file=sys.stderr)
  r = subprocess.run(cmd, cwd=oc.REPO_ROOT, env=env)
  return r.returncode


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--disc", default=os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso"))
  g = ap.add_mutually_exclusive_group()
  g.add_argument("--dump", help="the smoke's dump to compare")
  g.add_argument("--run", action="store_true", help="run the cosmetic smoke and compare its dump")
  args = ap.parse_args()
  if not os.path.isfile(args.disc):
    oc.die("disc not found: %s (--disc or COS_DISC)" % args.disc)

  want = expected(args.disc)
  if not args.dump and not args.run:
    print("\n".join(want))
    return 0

  tmp = None
  dump = args.dump
  if args.run:
    tmp = tempfile.TemporaryDirectory()
    dump = os.path.join(tmp.name, "cosmetic_pale.txt")
    code = run_smoke(dump, args.disc)
    if code != 0:
      print("cosmetic_check: the cosmetic smoke exited %d" % code, file=sys.stderr)
      return 1
  try:
    with open(dump) as f:
      got = [l.strip() for l in f if l.strip()]
  except OSError as e:
    oc.die("cannot read the dump: %s" % e)

  ok = got == want
  if not ok:
    for line in want:
      if line not in got:
        print("missing  " + line)
    for line in got:
      if line not in want:
        print("unexpected " + line)
    if sorted(got) == sorted(want):
      print("same records, different order")
  print("cosmetic_check: %d Pale records, %s wwrando's give_fairy_fountains_distinct_colors on %s"
        % (len(want), "equal to" if ok else "DIFFERENT from", os.path.basename(args.disc)))
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
