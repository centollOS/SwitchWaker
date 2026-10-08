#!/usr/bin/env python3
"""Release guard: fails if a release artifact contains anything taken from the disc, any build of the game
or any key.

usage: guard.py ARTIFACT.zip|DIR [...]

Release packages carry this repository's sources only (scripts/release/package_release.sh). The game is
built by each player from their own disc (scripts/switch/make_sd.sh). This check rejects, by name and
by content:
  - disc images and disc files: *.iso, *.gcm, *.rvz, *.gcz, *.ciso, *.nkit.*, *.wbfs, *.dol, *.rel,
    and the disc's archive formats when they appear as files (*.arc, *.szs, *.bmd, *.bdl, *.bti, *.aw);
  - builds: *.nro, *.nsp, *.nca, *.nacp, *.elf, *.dksh, the generated asset headers (include/assets/);
  - keys: *.key, *.keys (prod.keys, title.keys);
  - saves and caches made on a console: *.gci, *.sav, "Card A", dawn_cache.db, mesa_shader_cache*;
    the only database allowed is native/data/initial_pipeline_cache.db (a list of pipeline states,
    committed on purpose);
  - in every text file: a 32-hex-digit string (the shape of a console key; SHA-1 is 40 digits).
Exit status 1 lists every problem.
"""
import os
import re
import sys
import zipfile

BAD_NAME = [
    (re.compile(r"\.(iso|gcm|rvz|gcz|ciso|wbfs|wux|wud)$", re.I), "disc image"),
    (re.compile(r"\.nkit\.", re.I), "disc image"),
    (re.compile(r"\.(dol|rel)$", re.I), "game executable"),
    (re.compile(r"\.(arc|szs|bmd|bdl|bti|aw|afc|bms)$", re.I), "game data file"),
    (re.compile(r"\.(nro|nsp|nca|nacp|elf|dksh)$", re.I), "built binary"),
    (re.compile(r"(^|/)include/assets/"), "asset headers generated from the disc"),
    (re.compile(r"\.keys?$", re.I), "key file"),
    (re.compile(r"\.(gci|sav)$", re.I), "save file"),
    (re.compile(r"(^|/)Card A/"), "memory card"),
    (re.compile(r"(^|/)(dawn_cache\.db|mesa_shader_cache[^/]*)$"), "console shader cache"),
]
ALLOWED_DB = re.compile(r"(^|/)native/data/initial_pipeline_cache\.db$")
TEXT_EXT = {".py", ".txt", ".md", ".json", ".sh", ".bat", ".ps1", ".h", ".hpp", ".c", ".cpp", ".inl", ".cmake",
            ".cfg", ".ini", ".xml", ".toml", ".yml", ".yaml", ".patch", ""}
KEYLIKE = re.compile(rb"(?<![0-9A-Fa-f])[0-9A-Fa-f]{32}(?![0-9A-Fa-f])")


def check_entry(name, data, problems):
    n = name.replace("\\", "/")
    for rx, why in BAD_NAME:
        if rx.search(n):
            problems.append("%s: %s" % (name, why))
    if n.endswith(".db") and not ALLOWED_DB.search(n):
        problems.append("%s: database (only native/data/initial_pipeline_cache.db may ship)" % name)
    ext = os.path.splitext(n)[1].lower()
    if ext in TEXT_EXT and b"\0" not in data[:8192]:
        for m in KEYLIKE.finditer(data):
            line = data.count(b"\n", 0, m.start()) + 1
            problems.append("%s:%d: 32-hex-digit string (key-like)" % (name, line))


def scan(path):
    problems, count = [], 0
    if os.path.isdir(path):
        for dp, _, fns in os.walk(path):
            for fn in fns:
                full = os.path.join(dp, fn)
                with open(full, "rb") as f:
                    check_entry(os.path.relpath(full, path), f.read(), problems)
                count += 1
    else:
        with zipfile.ZipFile(path) as z:
            for info in z.infolist():
                if not info.is_dir():
                    check_entry(info.filename, z.read(info), problems)
                    count += 1
    return problems, count


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    bad = False
    for p in sys.argv[1:]:
        problems, count = scan(p)
        if problems:
            bad = True
            print("REJECTED %s (%d files):" % (p, count))
            for pr in problems:
                print("  " + pr)
        else:
            print("ok %s (%d files checked)" % (p, count))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
