#!/usr/bin/env python3
"""Save in several situations through the game's save code, load each back through the file select.

  native/tools/save_sweep.py [options] [case ...]

For each case: a first run saves (COS_SMOKE=save-sweep, native/src/pc/harness/sweeps/pc_save_sweep.cpp: the debug
boot of the case's stage with its story flags / preset / items, then the save screen's steps into
an empty card in the run directory; for the `items` case COS_SMOKE=item-sweep with
COS_ITEM_SWEEP_SAVE=1, so the items come from real item-get demos first); a copy of that card is
then loaded by a second run (COS_SMOKE=save-load: the title, START, the file select, A on file 1),
which compares the state loaded into memory with file 1 of the card region by region and checks
that the game started at the save's return place with the player in its room. The two summaries
(save_expect.txt, save_loaded.txt) are compared here too. Every run uses COS_CACHE_PER_RUN=1.
Runs land in <sweep dir>/<case>/save and <sweep dir>/<case>/load; the report is
<sweep dir>/save_sweep.txt. Exit 0 only if every case saved and loaded back equal.

Cases (default: all):
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import shutil
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
BUILD = os.path.join(REPO, "build", "native-mac")
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")

_spec = importlib.util.spec_from_file_location("boot_sweep", os.path.join(SCRIPT_DIR, "boot_sweep.py"))
boot_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boot_sweep)

# RODE_KORL (0x2A08): without it dComIfGs_setGameStartStage always returns a new file to Outset
# (l_checkData's last entry); with it the return place comes from the stage the save is made on.
RODE_KORL = "2A08"

# name: (description, smoke, env)
CASES = {
    "outset": ("Outset, a new file (return place: Outset, the default without story flags)",
               "save-sweep", {"COS_BOOT_STAGE": "sea:44:206"}),
    "outset-rode": ("Outset after the first boat ride (return place from the island)",
                    "save-sweep", {"COS_BOOT_STAGE": "sea:44:206", "COS_BOOT_EVENTS": RODE_KORL}),
    "sailing": ("on the boat at sea next to Windfall, COS_BOOT_PRESET=sailing (sail, baton, song, pearl)",
                "save-sweep", {"COS_BOOT_PRESET": "sailing"}),
    "drc": ("Dragon Roost Cavern (M_NewD2), a dungeon: the return place is its entrance",
            "save-sweep", {"COS_BOOT_STAGE": "M_NewD2:0:0", "COS_BOOT_EVENTS": RODE_KORL,
                           "COS_BOOT_ITEMS": "25,27,31"}),
    "forest": ("Forbidden Woods (kindan), a dungeon, with the sailing preset's flags",
               "save-sweep", {"COS_BOOT_STAGE": "kindan:0:0", "COS_BOOT_PRESET": "sailing"}),
    "windfall": ("Windfall Island on foot (sea room 11), with the sailing preset",
                 "save-sweep", {"COS_BOOT_STAGE": "sea:11:0", "COS_BOOT_PRESET": "sailing"}),
    "items": ("items collected through chests (COS_SMOKE=item-sweep over --items), then saved",
              "item-sweep", {"COS_BOOT_STAGE": "Asoko:0:0:2", "COS_ITEM_SWEEP_SAVE": "1"}),
}
DEFAULT_ITEMS = "0x01-0x08,0x0B,0x10,0x20-0x2D,0x2F-0x31,0x33-0x38,0x3B,0x42,0x50-0x53,0x61,0x69-0x6B,0x6D-0x72,0x78"


def item_count(text):
    n = 0
    for part in text.split(","):
        a, _, b = part.partition("-")
        n += int(b or a, 0) - int(a, 0) + 1
    return n


def summary(path):
    out = {}
    for line in boot_sweep.read(path).splitlines():
        if line.startswith("#") or not line.strip():
            continue
        key, _, value = line.partition(" ")
        if key == "region":
            name, _, value = value.partition(" ")
            key = "region " + name
        out[key] = value
    return out


def run(args, smoke, env, run_dir):
    cmd = [COS_RUN, smoke, "--timeout", str(args.timeout), "--disc", args.disc, "--quiet", "--run-dir", run_dir]
    if args.exe:
        cmd += ["--exe", args.exe]
    full = dict(os.environ, COS_CACHE_PER_RUN="1")
    for k in ("COS_BOOT_STAGE", "COS_BOOT_EVENTS", "COS_BOOT_ITEMS", "COS_BOOT_PRESET", "COS_ITEM_SWEEP",
              "COS_ITEM_SWEEP_SAVE", "COS_CARD_DIR"):
        full.pop(k, None)
    full.update(env)
    subprocess.run(cmd, env=full, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
    except ValueError:
        rc = -1
    boot_sweep.collapse_log(os.path.join(run_dir, "run.log"))
    return rc, boot_sweep.signature(run_dir, rc, boot_sweep.read(os.path.join(run_dir, "run.log")))


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("-h", "--help", action="store_true")
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--items", default=DEFAULT_ITEMS)
    ap.add_argument("--disc", default=os.environ.get("COS_DISC", ""))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("cases", nargs="*")
    args = ap.parse_args()
    if args.help:
        print(__doc__ + "".join("  %-12s %s\n" % (k, v[0]) for k, v in CASES.items()) + """
Options:
  --timeout S   run.sh --timeout per run (default 600; the items case gets 30 s an item more)
  --items LIST  the items case's COS_ITEM_SWEEP (default %s)
  --disc PATH   the GZLE01 .iso (default COS_DISC)
  --exe PATH    the executable (default build/native-mac/centollos)
  --out DIR     the sweep directory (default build/native-mac/runs/save-sweeps-<timestamp>)
Nothing here is meant for git.""" % DEFAULT_ITEMS)
        return 0
    cases = args.cases or list(CASES)
    for c in cases:
        if c not in CASES:
            print("save_sweep: unknown case %s (known: %s)" % (c, " ".join(CASES)), file=sys.stderr)
            return 2
    out = args.out or os.path.join(BUILD, "runs", "save-sweeps-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=False)

    rows = []
    for name in cases:
        desc, smoke, env = CASES[name]
        env = dict(env)
        case_dir = os.path.join(out, name)
        os.makedirs(case_dir)
        if smoke == "item-sweep":
            env["COS_ITEM_SWEEP"] = args.items
        save_dir = os.path.join(case_dir, "save")
        saved_args = args
        if smoke == "item-sweep":
            saved_args = argparse.Namespace(**vars(args))
            saved_args.timeout = args.timeout + 30 * item_count(args.items)
        rc, sig = run(saved_args, smoke, env, save_dir)
        row = {"case": name, "save": "%d %s" % (rc, sig), "load": "-", "diff": "-", "return": "-"}
        expect = summary(os.path.join(save_dir, "save_expect.txt"))
        row["return"] = expect.get("return", "-")
        if rc != 0:
            rows.append(row)
            print("%s: save exit %d: %s" % (name, rc, sig), file=sys.stderr)
            continue
        card = os.path.join(case_dir, "card")
        shutil.copytree(os.path.join(save_dir, "card"), card)
        load_dir = os.path.join(case_dir, "load")
        rc2, sig2 = run(args, "save-load", {"COS_CARD_DIR": card}, load_dir)
        row["load"] = "%d %s" % (rc2, sig2)
        loaded = summary(os.path.join(load_dir, "save_loaded.txt"))
        if loaded:
            diffs = [k for k in sorted(set(expect) | set(loaded)) if expect.get(k) != loaded.get(k)]
            row["diff"] = ",".join(diffs) if diffs else "equal"
        rows.append(row)
        print("%s: save %d, load %d (%s), summaries %s" % (name, rc, rc2, sig2, row["diff"]), file=sys.stderr)

    report = os.path.join(out, "save_sweep.txt")
    bad = 0
    with open(report, "w") as f:
        f.write("# save_sweep.py: %d cases\n" % len(rows))
        f.write("case\tsave\tload\tsummaries\treturn place\n")
        for r in rows:
            ok = r["save"].startswith("0 ") and r["load"].startswith("0 ") and r["diff"] == "equal"
            bad += 0 if ok else 1
            f.write("%s\t%s\t%s\t%s\t%s\n" % (r["case"], r["save"], r["load"], r["diff"], r["return"]))
        f.write("# %d of %d cases saved and loaded back equal\n" % (len(rows) - bad, len(rows)))
    print(open(report).read(), end="")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
