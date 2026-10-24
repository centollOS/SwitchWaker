#!/usr/bin/env python3
"""Writes the fixtures of the enemy palette generator and colour math (docs/RANDOMIZER_PLAN.md step R7.2a).

    python3 -I native/check/rando/fixtures/R7_2a_make_fixture.py

R7_2a_colors.txt (cos_rando_test_R7_2a; synthetic colours, no disc data): gclib's
texture_utils.hsv_shift_color / hsv_shift_palette (gclib d9322523, MIT) on

  - 20,000 pseudo-random colours: xorshift32 from 0x2545F491 (x ^= x << 13; x ^= x >> 17;
    x ^= x << 5), one draw per colour, r g b = bits 0-7, 8-15, 16-23;
  - the 256 greys;
  - a 32-level grid (each channel (i * 255) // 31, i = 0..31; r outermost, b innermost);

in that order, under 30 shifts: the 4 corners of PaletteRandomizer's range (20/340 x -40/40), 0 value
shift at both hue ends, and 24 drawn with Python's random.Random(7202) as randint(20, 340),
randint(-40, 40). Lines:

    count <n>                                the number of colours per shift (20000 + 256 + 32768)
    shift <h> <v> <fnv1a64>                  FNV-1a 64 over the shifted r g b bytes of all colours
    sample <h> <v> <r> <g> <b> <r'> <g'> <b'>  the first 16 shifted colours of the shift, in full
    pal <h> <v> <hex RGBA...> -> <hex RGBA...>   hsv_shift_palette on 8 RGBA colours (alpha kept)

R7_2a_TestSeed1_palettes.json: ww_export.py TestSeed1 randomize_enemy_palettes=true (wwrando
1.10.0_9775811 dry run), the golden placement with its "palettes" key.

Uses the oracle venv (native/tools/rando/setup_oracle.sh) through oracle_common.bootstrap().
"""
import os
import random
import subprocess
import sys
sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..", "..", "..", "tools", "rando")
sys.path.insert(0, TOOLS)
import oracle_common as oc  # noqa: E402

oc.bootstrap()

from gclib import texture_utils  # noqa: E402

N_RANDOM = 20000
XS_SEED = 0x2545F491
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def colours():
  out = []
  x = XS_SEED
  for _ in range(N_RANDOM):
    x ^= (x << 13) & 0xFFFFFFFF
    x ^= x >> 17
    x ^= (x << 5) & 0xFFFFFFFF
    out.append((x & 0xFF, (x >> 8) & 0xFF, (x >> 16) & 0xFF))
  out += [(g, g, g) for g in range(256)]
  levels = [(i * 255) // 31 for i in range(32)]
  out += [(r, g, b) for r in levels for g in levels for b in levels]
  return out


def shifts():
  out = [(20, -40), (20, 40), (340, -40), (340, 40), (20, 0), (340, 0)]
  rng = random.Random(7202)
  while len(out) < 30:
    out.append((rng.randint(20, 340), rng.randint(-40, 40)))
  return out


def main():
  cols = colours()
  lines = [f"count {len(cols)}"]
  prng = random.Random(72021)
  for h, v in shifts():
    fnv = FNV_OFFSET
    shifted = []
    for c in cols:
      r, g, b = texture_utils.hsv_shift_color(c, h, v)
      for byte in (r, g, b):
        assert 0 <= byte <= 255
        fnv = ((fnv ^ byte) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
      if len(shifted) < 16:
        shifted.append((c, (r, g, b)))
    lines.append(f"shift {h} {v} {fnv:016x}")
    for c, s in shifted:
      lines.append(f"sample {h} {v} {c[0]} {c[1]} {c[2]} {s[0]} {s[1]} {s[2]}")
    pal = [tuple(prng.randrange(256) for _ in range(4)) for _ in range(8)]
    out = texture_utils.hsv_shift_palette(list(pal), h, v)
    hexs = lambda cs: " ".join("%02x%02x%02x%02x" % c for c in cs)  # noqa: E731
    lines.append(f"pal {h} {v} {hexs(pal)} -> {hexs(out)}")
  with open(os.path.join(HERE, "R7_2a_colors.txt"), "w", newline="\n") as f:
    f.write("# R7_2a_make_fixture.py: gclib texture_utils.hsv_shift_color (see the script)\n")
    f.write("\n".join(lines) + "\n")

  subprocess.run([sys.executable, "-I", os.path.join(TOOLS, "ww_export.py"), "TestSeed1",
                  os.path.join(HERE, "R7_2a_TestSeed1_palettes.json"), "randomize_enemy_palettes=true"],
                 check=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
