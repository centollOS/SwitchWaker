#!/usr/bin/env python3
"""gen_palette_data.py - wwrando's enemy palette groups as a checked-in C++ table (plan step R7.2a, D11).

    python3 -I native/tools/rando/gen_palette_data.py            # (re)write the table
    python3 -I native/tools/rando/gen_palette_data.py --check    # exit 1 if it would change

Reads data/palette_randomizable_files.txt of wwrando 1.10.0_9775811 (ref/wwrando, MIT) with ruamel's
safe loader, exactly as randomizers/palettes.py loads it, and writes
native/src/pc/features/rando/generated/rando_palettes.inc: the 37 groups in file order (that order
is PaletteRandomizer._randomize's draw order), each with its name, its particle ids (in the file's
order) and its RARCs (name and excluded files).

The build never runs this script; running it again must give no diff. Needs the oracle venv
(native/tools/rando/setup_oracle.sh) for ruamel.yaml; it re-executes itself there when needed.
"""
import hashlib
import os
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

OUT_PATH = os.path.join(oc.REPO_ROOT, "native", "src", "pc", "features", "rando", "generated",
                        "rando_palettes.inc")
SOURCE = "data/palette_randomizable_files.txt"
EXPECTED_GROUPS = 37


def file_sha1(path):
  with open(path, "rb") as f:
    return hashlib.sha1(f.read()).hexdigest()


def wwrando_commit():
  try:
    return subprocess.run(["git", "-C", oc.WWRANDO_DIR, "rev-parse", "HEAD"], check=True,
                          capture_output=True, text=True).stdout.strip()
  except (OSError, subprocess.CalledProcessError):
    return "unknown"


def c_str(s):
  assert all(32 <= ord(c) < 127 for c in s), s
  return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate():
  from ruamel.yaml import YAML
  yaml = YAML(typ="safe")
  src = os.path.join(oc.WWRANDO_DIR, SOURCE)
  with open(src) as f:
    groups = yaml.load(f)
  if len(groups) != EXPECTED_GROUPS:
    oc.die(f"{SOURCE} has {len(groups)} groups, expected {EXPECTED_GROUPS}")

  out = [
    "// GENERATED FILE - do not edit. Regenerate with: python3 -I native/tools/rando/gen_palette_data.py",
    "// Enemy palette groups (randomizers/palettes.py, plan step R7.2a)",
    f"// Source: wwrando 1.10.0_9775811 (MIT), commit {wwrando_commit()}",
    f"// Generator: native/tools/rando/gen_palette_data.py sha1 {file_sha1(os.path.abspath(__file__))}",
    f"//   {SOURCE} sha1 {file_sha1(src)}",
    "// Included once, by pc_rando_gen_palettes.cpp (inside namespace pcr, anonymous namespace).",
    "",
  ]
  rows = []
  for gi, g in enumerate(groups):
    name = g["Name"]
    pids = list(g["Particle IDs"] or [])
    rarcs = list(g["RARCs"] or [])
    for p in pids:
      assert isinstance(p, int) and 0 <= p <= 0xFFFF, (name, p)
    pid_ref = "nullptr"
    if pids:
      out.append(f"static const uint16_t kPalParticles{gi}[] = {{"
                 + ", ".join("0x%04X" % p for p in pids) + "};")
      pid_ref = f"kPalParticles{gi}"
    rarc_rows = []
    for ri, r in enumerate(rarcs):
      excl = list(r["Excluded files"] or [])
      excl_ref = "nullptr"
      if excl:
        out.append(f"static const char* const kPalExcluded{gi}_{ri}[] = {{"
                   + ", ".join(c_str(e) for e in excl) + "};")
        excl_ref = f"kPalExcluded{gi}_{ri}"
      rarc_rows.append(f"    {{{c_str(r['Name'])}, {excl_ref}, {len(excl)}}},")
    rarc_ref = "nullptr"
    if rarc_rows:
      out.append(f"static const PaletteRarc kPalRarcs{gi}[] = {{")
      out.extend(rarc_rows)
      out.append("};")
      rarc_ref = f"kPalRarcs{gi}"
    rows.append(f"    {{{c_str(name)}, {pid_ref}, {len(pids)}, {rarc_ref}, {len(rarcs)}}},")
  out.append("")
  out.append("// {name, particle ids, count, RARCs, count}, in file order")
  out.append("static const PaletteGroup kPaletteGroupTable[] = {")
  out.extend(rows)
  out.append("};")
  return "\n".join(out) + "\n"


def main(argv):
  check = "--check" in argv
  unknown = [a for a in argv if a != "--check"]
  if unknown:
    oc.die(f"unknown arguments: {' '.join(unknown)}")
  oc.bootstrap()
  text = generate()
  old = None
  if os.path.exists(OUT_PATH):
    with open(OUT_PATH, encoding="utf-8") as f:
      old = f.read()
  if check:
    if old != text:
      print("out of date: rando_palettes.inc")
      return 1
    print("up to date")
    return 0
  if old != text:
    with open(OUT_PATH, "w", newline="\n", encoding="utf-8") as f:
      f.write(text)
    print("wrote rando_palettes.inc")
  else:
    print("no changes")
  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
