#!/usr/bin/env python3
"""Phase 4 inventory (docs/NATIVE_PORT_PHASE4_6.md, step 4.0a).

Counts the `TODO(native phase 4):` markers of the native port and sorts them into the groups of the
plan's finding 1, each tied to the step that clears it, and (with --log) counts the phase 4 warnings
of a build log. native/check/phase4_baseline.txt records the counts at the start of phase 4; each
later step lowers its group's count and regenerates the baseline.

Markers:
  open       `TODO(native phase 4): ...`             counted in its group
  justified  `NOTE(native phase 4, harmless): ...`   a marker a step reviewed and kept on purpose
                                                     (J2DPrint in 4.1, debug walks in 4.19)
A mention such as "see TODO(native phase 4) in ..." (no colon) is not a marker.

Warnings (--log): the build log of a full game build configured with -DCOS_PHASE4_WARNINGS=ON
(turning the option on recompiles every game unit). Each diagnostic is counted once per
file:line:column, so a header warning seen by many units counts once.

Usage:
  phase4_inventory.py                       group table
  phase4_inventory.py --list A              the markers of group A (file:line: text)
  phase4_inventory.py --by-file             marker count per file
  phase4_inventory.py --log build.log       also count the phase 4 warnings
  phase4_inventory.py --log build.log --write-baseline native/check/phase4_baseline.txt
  phase4_inventory.py [--log build.log] --check native/check/phase4_baseline.txt
        exit 1 if a marker is unclassified or any count is above the baseline
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from collections import Counter, defaultdict

NATIVE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REPO = os.path.dirname(NATIVE)
SCAN_DIRS = ("../game/include", "../game/src", "include", "src", "sdk")
EXTS = (".c", ".cpp", ".h", ".hpp", ".inc")

OPEN_RE = re.compile(r"TODO\(native phase 4\):")
JUSTIFIED_RE = re.compile(r"NOTE\(native phase 4, harmless\):")

# (letter, name, step that clears it, finding-1 category, rule)
# A rule is (path regex, text regex or None); the first group with a matching rule wins.
GROUPS = [
    ("A", "pointer kept in a u32 field", "4.1", "setUserArea/mUserArea, J3DPacket user area, TVector",
     [(r"/src/d/actor/", r"user area|UserArea"),
      (r"J3DGraphBase/J3DPacket\.h$", None),
      (r"JGadget/vector\.h$", None)]),
    ("B", "heap headers and ARAM callback", "4.2", "JKRExpHeap header, JKRAramPiece callback",
     [(r"JKernel/JKRExpHeap\.h$", None),
      (r"JKernel/JKRAramPiece\.cpp$", None)]),
    ("C", "logo-scene resources", "4.8", "d_s_actor_data_mng, d_com_inf_game, d_resorce",
     [(r"/d/d_s_actor_data_mng\.cpp$", None),
      (r"/d/d_com_inf_game\.cpp$", None),
      (r"/d/d_resorce\.cpp$", None)]),
    ("D", "stage chunk table", "4.9a", "d_stage, d_s_menu",
     [(r"/d/d_stage\.cpp$", None),
      (r"/d/d_s_menu\.cpp$", None)]),
    ("E", "collision (dzb)", "4.10", "c_bg_s",
     [(r"SComponent/c_bg_s\.cpp$", None)]),
    ("F", "J3D", "4.11/4.12", "J3D loaders, factories, DrawBuffer/Shape/Joint/Sys, J3DMaterial.h/J3DTexture.h",
     [(r"/JSystem/J3DGraph(Loader|Base|Animator)/", None)]),
    ("G", "JStudio", "4.17", "stb, fvb/functionvalue, object-actor",
     [(r"/JSystem/JStudio", None)]),
    ("H", "J2DPrint (harmless)", "4.1 marks", "J2DPrint",
     [(r"J2DGraph/J2DPrint\.cpp$", None)]),
    ("I", "debug, JOR and stack walks", "4.19 justifies", "m_Do_printf, m_Do_machine, DynamicLink, m_Do_hostIO",
     [(r"/m_Do/m_Do_printf\.cpp$", None),
      (r"/m_Do/m_Do_machine\.cpp$", None),
      (r"/src/DynamicLink\.cpp$", None),
      (r"/m_Do/m_Do_hostIO\.h$", None)]),
    ("J", "THP movie", "4.18", "THP header; plus the asm-only sites in d_a_movie_player",
     [(r"cos_thp_extras\.h$", None),
      (r"/d/actor/d_a_movie_player\.cpp$", None)]),
    ("K", "audio and physical addresses", "5.1/5.2", "OS.h; the JAudio markers came with phase 3 step 3.7",
     [(r"/JSystem/JAudio/", None),
      (r"/dolphin/os/OS\.h$", None)]),
    ("L", "other offsets and ResTIMG", "4.4/4.5", "JSupport; plus the ResTIMG site in d_a_player_main",
     [(r"JSupport/JSupport\.h$", None),
      (r"/d/actor/", r"ResTIMG")]),
]

WARNING_FLAGS = ("-Wint-to-pointer-cast", "-Wpointer-to-int-cast", "-Wint-to-void-pointer-cast",
                 "-Wreturn-type", "-Wfortify-source")
DIAG_RE = re.compile(r"^(?P<file>[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+): (?:warning|error): .*\[(?P<flags>-W[^\]]+)\]\s*$")
ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def scan():
    """Return (open, justified): lists of (relpath, line, text)."""
    found_open, found_just = [], []
    for d in SCAN_DIRS:
        root = os.path.join(NATIVE, d)
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames.sort()
            for fn in sorted(filenames):
                if not fn.endswith(EXTS):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, REPO)
                with open(path, encoding="utf-8", errors="replace") as f:
                    for i, line in enumerate(f, 1):
                        if OPEN_RE.search(line):
                            found_open.append((rel, i, line.strip()))
                        elif JUSTIFIED_RE.search(line):
                            found_just.append((rel, i, line.strip()))
    return found_open, found_just


def classify(rel, text):
    path = "/" + rel
    for letter, _name, _step, _cat, rules in GROUPS:
        for prx, trx in rules:
            if re.search(prx, path) and (trx is None or re.search(trx, text)):
                return letter
    return "?"


def count_warnings(log_path):
    seen = defaultdict(set)
    with open(log_path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = DIAG_RE.match(ANSI_RE.sub("", raw.rstrip("\n")))
            if not m:
                continue
            for flag in m.group("flags").split(","):
                flag = flag.strip()
                if flag in WARNING_FLAGS:
                    key = (os.path.normpath(m.group("file")), m.group("line"), m.group("col"))
                    seen[flag].add(key)
    return {flag: len(seen[flag]) for flag in WARNING_FLAGS}


def counts(found_open, found_just, warnings):
    c = Counter(classify(rel, text) for rel, _l, text in found_open)
    out = {f"group {g[0]}": c.get(g[0], 0) for g in GROUPS}
    out["unclassified"] = c.get("?", 0)
    out["markers open"] = len(found_open)
    out["markers justified"] = len(found_just)
    if warnings is not None:
        for flag, n in warnings.items():
            out[f"warning {flag}"] = n
    return out


def write_baseline(path, data):
    lines = [
        "# Phase 4 inventory baseline (docs/NATIVE_PORT_PHASE4_6.md, step 4.0a).",
        "# Generated by native/tools/phase4_inventory.py --write-baseline; a step that fixes markers or",
        "# warnings regenerates it, and --check fails if any count rises above it.",
        "# Groups:",
    ]
    for letter, name, step, cat, _r in GROUPS:
        lines.append(f"#   {letter}  {name} (step {step}; finding 1: {cat})")
    lines.append("# Warning counts are distinct file:line:column sites in a full game build with")
    lines.append("# -DCOS_PHASE4_WARNINGS=ON.")
    for k, v in data.items():
        lines.append(f"{k}\t{v}")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def read_baseline(path):
    data = {}
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            k, v = line.rsplit("\t", 1)
            data[k] = int(v)
    return data


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list", metavar="GROUP", help="list the markers of one group ('?' = unclassified)")
    ap.add_argument("--by-file", action="store_true", help="marker count per file")
    ap.add_argument("--log", help="build log of a -DCOS_PHASE4_WARNINGS=ON build")
    ap.add_argument("--write-baseline", metavar="FILE")
    ap.add_argument("--check", metavar="FILE", help="fail if a count is above this baseline")
    args = ap.parse_args()

    found_open, found_just = scan()
    warnings = count_warnings(args.log) if args.log else None

    if args.list:
        for rel, line, text in found_open:
            if classify(rel, text) == args.list:
                print(f"{rel}:{line}: {text}")
        return 0
    if args.by_file:
        c = Counter((rel, classify(rel, text)) for rel, _l, text in found_open)
        for (rel, g), n in sorted(c.items(), key=lambda x: (-x[1], x[0])):
            print(f"{n:5d}  {g}  {rel}")
        return 0

    data = counts(found_open, found_just, warnings)
    print(f"{'group':6} {'count':>5}  {'step':14} name")
    for letter, name, step, _cat, _r in GROUPS:
        print(f"{letter:6} {data['group ' + letter]:5d}  {step:14} {name}")
    print(f"{'?':6} {data['unclassified']:5d}  {'':14} unclassified")
    print(f"open markers: {data['markers open']}, justified: {data['markers justified']}")
    if warnings is not None:
        for flag in WARNING_FLAGS:
            print(f"warning {flag}: {warnings[flag]}")

    if args.write_baseline:
        write_baseline(args.write_baseline, data)
        print(f"wrote {args.write_baseline}")

    rc = 0
    if data["unclassified"]:
        print(f"error: {data['unclassified']} unclassified marker(s); see --list '?'", file=sys.stderr)
        rc = 1
    if args.check:
        base = read_baseline(args.check)
        for k, v in data.items():
            if k.startswith("warning ") and warnings is None:
                continue
            if k == "markers justified":
                continue
            if k in base and v > base[k]:
                print(f"error: {k} = {v}, above the baseline {base[k]}", file=sys.stderr)
                rc = 1
        if rc == 0:
            print(f"ok: no count above {args.check}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
