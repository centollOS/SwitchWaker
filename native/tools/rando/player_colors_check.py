#!/usr/bin/env python3
"""player_colors_check.py - checks step R7.5b's player colours against wwrando on the user's disc.

    python3 -I native/tools/rando/player_colors_check.py [--disc GZLE01.iso]
        [--dump FILE | --run [--colors JSON] [--vanilla]]

Runs wwrando 1.10.0's own customizer.change_player_custom_colors (ref/wwrando/customizer.py) with gclib
on the disc, for the custom_colors the colors smoke used, and prints what the game must hold, in the
format of the smoke's dump (native/src/pc/harness/smokes/pc_rando_smoke_colors.cpp):

    trail <metadata key> <r> <g> <b> <a>     main.dol 0x803F62AC/B0/B4 and 0x803F6268
    hitomi <i> <format> <palette format> colors <n> img <md5> pal <md5>
                                             Link.arc cl.bdl "hitomi" (image, palette[:n*2])
    arrow prm <r> <g> <b> <a>                common.jpc particle 0x48 BSP1 prm colour
    brk <cutfh.brk|cutfm.brk> <channel> <key values...>
                                             LkAnm.arc, register colour animation 0 as gclib reads
                                             the file wwrando saved

How wwrando is driven (the port's subset, docs/RANDOMIZER_PLAN.md R7.5b and
features/rando/pc_rando_player_colors.h):
  - the trail colours are given to customizer.py as the player model's metadata (where it reads them;
    the port takes them from custom_colors), a colour without alpha getting the vanilla alpha;
  - the pupils: customizer.py's branch for a model without a hitomi mask (color_exchange with
    ignore_bright, then replace_image), for the "Eyes" colour, since the port ships no masks; the
    body colours (all of which need masks) are left out of wwrando's run;
  - the arrow trail: wwrando 1.10.0 writes `bsp1.color_prm`, a field gclib's BSP1 does not have, so
    its saved common.jpc keeps the vanilla colour (this tool checks that and says so); the expected
    value is gclib's `prm_color` set as customizer.py meant.
--dump FILE  compares those lines with a dump the smoke wrote (its "config" line gives the colours
             and the casual flag; "rando 0" means a vanilla run: everything must be the disc's).
--run        runs the smoke (native/tools/run.sh rando --stage M_NewD2:0:0 with COS_RANDO_SMOKE=colors,
             COS_BOOT_ITEMS=2D,38, COS_RANDO_FORCE=1 unless --vanilla, COS_RANDO_CUSTOM_COLORS=--colors,
             default a colour for every key) into a folder under build/, requires exit 0, compares.
Without either, prints the expected lines for --colors.
The disc defaults to $COS_DISC, else ~/Documents/windwaker/GZLE01.iso. Nothing read from the disc is
written to the repository. Exit codes: 0 equal, 1 different or the smoke failed, 2 usage or setup error.
Uses the oracle venv (setup_oracle.sh) through oracle_common.bootstrap().
"""
import argparse
import contextlib
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from io import BytesIO
from types import SimpleNamespace

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib import texture_utils  # noqa: E402
from gclib.gcm import GCM  # noqa: E402
from gclib.j3d import BDL, BRK  # noqa: E402
from gclib.jpc import JPC  # noqa: E402
from gclib.rarc import RARC  # noqa: E402

with contextlib.redirect_stdout(sys.stderr):
  import customizer  # noqa: E402

TRAILS = ["sword_slash_trail_color", "elixir_soup_sword_trail_color", "parrying_sword_trail_color",
          "boomerang_trail_color", "arrow_trail_color"]
TRAIL_ADDR = {0x803F62AC: 0, 0x803F62B0: 1, 0x803F62B4: 2, 0x803F6268: 3}
VANILLA = [(0xFF, 0xFF, 0xFF, 0x96), (0xFF, 0xFF, 0x7B, 0x96), (0x5A, 0xFF, 0x5A, 0x96), (0xFF, 0xFF, 0x7B, 0x96)]
EYES_BASE = (0x10, 0x51, 0x4A)  # link_metadata.txt hero_custom_colors / casual_custom_colors "Eyes"
ARROW_PARTICLE = 0x48
DEFAULT_COLORS = {
  "Eyes": [255, 0, 0],
  "sword_slash_trail_color": "FF0000C0",
  "elixir_soup_sword_trail_color": [0, 255, 0],
  "parrying_sword_trail_color": [0, 0, 255, 128],
  "boomerang_trail_color": "00FFFF",
  "arrow_trail_color": [255, 0, 255, 200],
}


def parse_color(v, alpha):
  """The port's custom_colors value forms (pc_rando_player_colors_parse). Returns (rgba, has_alpha)."""
  if isinstance(v, list) and all(isinstance(x, int) and 0 <= x <= 255 for x in v) and (
      len(v) == 3 or (alpha and len(v) == 4)):
    return (list(v) + [None])[:4], len(v) == 4
  if isinstance(v, str):
    m = re.fullmatch(r"(?:#|0[xX])?([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})", v)
    if m and (alpha or len(m.group(1)) == 6):
      x = int(m.group(1), 16)
      if len(m.group(1)) == 8:
        return [(x >> 24) & 255, (x >> 16) & 255, (x >> 8) & 255, x & 255], True
      return [(x >> 16) & 255, (x >> 8) & 255, x & 255, None], False
  if isinstance(v, int) and not isinstance(v, bool) and 0 <= v <= (0xFFFFFFFF if alpha else 0xFFFFFF):
    if alpha:
      return [(v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255], True
    return [(v >> 16) & 255, (v >> 8) & 255, v & 255, None], False
  return None, False


class Dol:
  def __init__(self):
    self.writes = {}

  def write_data(self, fn, addr, value, fmt):
    self.writes[addr] = tuple(value)


class Disc:
  """Just enough of WWRandomizer for customizer.change_player_custom_colors."""

  def __init__(self, iso, casual):
    self.gcm = GCM(iso)
    self.gcm.read_entire_disc()
    self.arcs = {}
    self.jpcs = {}
    self.dol = Dol()
    self.custom_model_name = "Link"
    self.options = SimpleNamespace(custom_colors={}, player_in_casual_clothes=bool(casual))

  def get_arc(self, path):
    if path not in self.arcs:
      self.arcs[path] = RARC(self.gcm.read_file_data(path))
    return self.arcs[path]

  def get_jpc(self, path):
    if path not in self.jpcs:
      self.jpcs[path] = JPC(self.gcm.read_file_data(path))
    return self.jpcs[path]


def md5(b):
  return hashlib.md5(bytes(b)).hexdigest()


def expected(iso, colors, casual, rando):
  if not rando:
    colors = {}
  meta = {}
  trail = {}
  for i, key in enumerate(TRAILS):
    if key not in colors:
      continue
    rgba, has_alpha = parse_color(colors[key], True)
    if rgba is None:
      continue
    if not has_alpha and i < 4:
      rgba[3] = VANILLA[i][3]
    trail[i] = (rgba, has_alpha)
    if i < 4 or has_alpha:
      meta[key] = list(rgba)
    else:
      meta[key] = rgba[:3] + [0]  # alpha filled from the file below
  eyes = None
  if "Eyes" in colors:
    rgba, _ = parse_color(colors["Eyes"], False)
    if rgba is not None and tuple(rgba[:3]) != EYES_BASE:
      eyes = tuple(rgba[:3])

  disc = Disc(iso, casual)
  # Arrow: what wwrando's own code leaves, before its run touches the cached JPC.
  jpc = disc.get_jpc("files/res/Particle/common.jpc")
  c = jpc.particles_by_id[ARROW_PARTICLE].bsp1.prm_color
  arrow_orig = (c.r, c.g, c.b, c.a)
  orig_get = customizer.get_model_metadata
  customizer.get_model_metadata = lambda name: meta
  try:
    with contextlib.redirect_stdout(sys.stderr):
      customizer.change_player_custom_colors(disc)
  finally:
    customizer.get_model_metadata = orig_get
  lines = []
  for addr, i in sorted(TRAIL_ADDR.items(), key=lambda kv: kv[1]):
    c = disc.dol.writes.get(addr, VANILLA[i])
    lines.append("trail %s %d %d %d %d" % ((TRAILS[i],) + tuple(c)))

  # The pupils: customizer.py's mask-free branch.
  link = disc.get_arc("files/res/Object/Link.arc")
  cl = link.get_file("cl.bdl", BDL)
  hitomi = cl.tex1.textures_by_name["hitomi"]
  if eyes is not None:
    image = hitomi[0].render()
    image = texture_utils.color_exchange(image, EYES_BASE, eyes, ignore_bright=True)
    for t in hitomi:
      t.replace_image(image)
  for i, t in enumerate(hitomi):
    pal = t.palette_data.getvalue()[:t.num_colors * 2]
    lines.append("hitomi %d %d %d colors %d img %s pal %s" % (
      i, t.image_format.value, t.palette_format.value, t.num_colors, md5(t.image_data.getvalue()), md5(pal)))

  # The arrow trail.
  jpc.save()
  c = JPC(jpc.data).particles_by_id[ARROW_PARTICLE].bsp1.prm_color
  wwr = (c.r, c.g, c.b, c.a)
  if 4 in trail and wwr == arrow_orig:
    print("player_colors_check: note: wwrando 1.10.0's own run left common.jpc 0x48 at %s (it sets "
          "bsp1.color_prm, which gclib does not save); expecting prm_color as customizer.py meant" % (wwr,),
          file=sys.stderr)
  a = list(arrow_orig)
  if 4 in trail:
    rgba, has_alpha = trail[4]
    a[0:3] = rgba[:3]
    if has_alpha:
      a[3] = rgba[3]
  lines.append("arrow prm %d %d %d %d" % tuple(a))

  # The sword tip BRKs, as gclib reads the files wwrando saved.
  lkanm = disc.get_arc("files/res/Object/LkAnm.arc")
  for name in ["cutfh.brk", "cutfm.brk"]:
    brk = lkanm.get_file(name, BRK)
    back = BRK(BytesIO(brk.data.getvalue()))
    anim = list(back.trk1.mat_name_to_reg_anims.values())[0][0]
    for ch, tr in enumerate([anim.r, anim.g, anim.b, anim.a]):
      lines.append("brk %s %d %s" % (name, ch, " ".join(str(k.value) for k in tr.keyframes)))
  return lines


def run_smoke(dump, iso, colors, vanilla):
  env = dict(os.environ)
  env["COS_DISC"] = os.path.abspath(iso)
  cmd = [os.path.join(oc.REPO_ROOT, "native", "tools", "run.sh"), "rando", "--stage", "M_NewD2:0:0",
         "--env", "COS_RANDO_SMOKE=colors", "--env", "COS_BOOT_ITEMS=2D,38",
         "--env", "COS_COLORS_DUMP=%s" % dump]
  if not vanilla:
    cmd += ["--env", "COS_RANDO_FORCE=1", "--env", "COS_RANDO_CUSTOM_COLORS=%s" % json.dumps(colors)]
  print("player_colors_check: " + " ".join(cmd), file=sys.stderr)
  return subprocess.run(cmd, cwd=oc.REPO_ROOT, env=env).returncode


def main():
  ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  ap.add_argument("--disc", default=os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso"))
  ap.add_argument("--colors", help="custom_colors JSON for --run and the printed lines (default: one per key)")
  ap.add_argument("--vanilla", action="store_true", help="--run without COS_RANDO_FORCE (no colours apply)")
  g = ap.add_mutually_exclusive_group()
  g.add_argument("--dump", help="the smoke's dump to compare")
  g.add_argument("--run", action="store_true", help="run the colors smoke and compare its dump")
  args = ap.parse_args()
  if not os.path.isfile(args.disc):
    oc.die("disc not found: %s (--disc or COS_DISC)" % args.disc)
  colors = DEFAULT_COLORS
  if args.colors is not None:
    try:
      colors = json.loads(args.colors)
    except ValueError as e:
      oc.die("--colors is not JSON: %s" % e)

  if not args.dump and not args.run:
    print("\n".join(expected(args.disc, colors, 0, True)))
    return 0

  dump = args.dump
  if args.run:
    base = os.path.join(oc.REPO_ROOT, "build")
    os.makedirs(base, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="player_colors_", dir=base)
    dump = os.path.join(tmp, "player_colors.txt")
    code = run_smoke(dump, args.disc, colors, args.vanilla)
    if code != 0:
      print("player_colors_check: the colors smoke exited %d" % code, file=sys.stderr)
      return 1
  try:
    with open(dump) as f:
      got = [l.strip() for l in f if l.strip()]
  except OSError as e:
    oc.die("cannot read the dump: %s" % e)
  casual, rando, used = 0, 1, {}
  for line in got:
    if line.startswith("config "):
      _, c, text = line.split(" ", 2)
      casual = int(c)
      used = json.loads(text)
    elif line.startswith("rando "):
      rando = int(line.split()[1])
  got = [l for l in got if not l.startswith(("config ", "rando "))]
  want = expected(args.disc, used, casual, rando)

  ok = got == want
  if not ok:
    for line in want:
      if line not in got:
        print("missing    " + line)
    for line in got:
      if line not in want:
        print("unexpected " + line)
  print("player_colors_check: %d values (%s), %s wwrando's change_player_custom_colors on %s"
        % (len(want), "vanilla" if not rando or not used else "custom_colors %s" % json.dumps(used),
           "equal to" if ok else "DIFFERENT from", os.path.basename(args.disc)))
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
