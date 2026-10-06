#!/usr/bin/env python3
"""Give every item through a chest's item-get demo, going on after stuck items and faults.

  native/tools/item_sweep.py [options]

Runs `run.sh item-sweep --stage Asoko:0:0:2` (COS_SMOKE=item-sweep, native/src/pc/harness/sweeps/pc_item_sweep.cpp:
for each item number the item tables define, a big chest with that item is created in front of the
player, opened with A, and its DEFAULT_TREASURE event, the item-get demo with its fanfare and
message, must end within a wall-time limit). A stuck item (its event does not end: the smoke logs
the event-watch report with the sub BGM and message state, then exits 1) or a fault ends that run;
the item it was on is the last "begin" line of the run's item_sweep.txt. The next run starts after
it (COS_ITEM_SWEEP=<n>-<last>), until the last item is done. Each run lands in <sweep dir>/from-<n>/
(the usual run.sh run directory, with COS_CACHE_PER_RUN=1: its pipeline cache is
<run>/cache/pipeline_cache.db); the report is <sweep dir>/item_sweep.txt. The items accumulate
on the file within a run (a restart begins a new file). Exit 0 only if every item ended its event.

Report: one line per item, tab-separated: item number (hex), name (dItemNo_*_e of
game/include/d/d_item_data.h), archive, result (ok, no-open, refused, stuck, fault, scene-lost;
xfail / xpass for the EXPECTED items: a failure of the original game's data),
event frames, seconds, got (dComIfGs_checkGetItem after the event), shot (the run's shot-<frame>.png,
the item held up), detail (for stuck: event name, sub BGM flag, message-waits-for-music, message
status; for a fault: the crash/panic/stall signature in boot_sweep.py's format), run directory.
Then the counts.

Options:
  --stage SPEC    COS_BOOT_STAGE (default Asoko:0:0:2, the chest of COS_SMOKE=chest)
  --range LIST    item numbers, e.g. 0x20-0x30,0x50 (default every one, 0x00-0xFE)
  --timeout S     run.sh --timeout per run (default 7200: about 25 s an item)
  --uncapped      run.sh --uncapped (the fanfare still plays in real time)
  --max-runs N    stop after N runs (default 100)
  --disc PATH     the GZLE01 .iso (default COS_DISC)
  --exe PATH      the executable (default build/native-mac/centollos)
  --out DIR       the sweep directory (default build/native-mac/runs/item-sweeps-<timestamp>)
  --rebuild DIR   write DIR/item_sweep.txt again from DIR's runs (no new run; the same --range)
Nothing here is meant for git.
"""

from __future__ import annotations

import argparse
import collections
import importlib.util
import os
import re
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
BUILD = os.path.join(REPO, "build", "native-mac")
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")
ITEM_DATA_H = os.path.join(REPO, "game", "include", "d", "d_item_data.h")

_spec = importlib.util.spec_from_file_location("boot_sweep", os.path.join(SCRIPT_DIR, "boot_sweep.py"))
boot_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boot_sweep)


# Items that fail through a chest for reasons of the original game's data, not of the port: item ->
# (a substring of the result's detail, the reason). A matching failure is reported "xfail"
# and does not fail the sweep; a pass is reported "xpass" (remove the entry).
EXPECTED = {
    0x16: ("d_a_itembase.cpp:85",
           "RECOVER_FAIRY: its item_resource entry names the Fa archive with the model and animation "
           "indices of the Always archive (d_item_data.cpp: 'This file index is for the wrong RARC'), "
           "so the demo item finds no model (JUT_ASSERT modelData != NULL; a retail GameCube, without "
           "asserts, would create a model from NULL). The game never gives a fairy through the "
           "item-get demo: as a field item fopAcM_createItem creates the fairy actor NPC_FA1 for it."),
}


def item_names():
    """dItemNo_*_e by number, from the ItemTable enum."""
    names = {}
    for m in re.finditer(r"/\* 0x([0-9A-F]{2}) \*/ dItemNo_(\w+)_e", boot_sweep.read(ITEM_DATA_H)):
        names[int(m.group(1), 16)] = m.group(2)
    return names


def parse_list(text):
    """'0x20-0x30,80' -> sorted item numbers."""
    out = set()
    for part in text.split(","):
        m = re.fullmatch(r"\s*(\w+)(?:-(\w+))?\s*", part)
        if not m:
            raise ValueError(part)
        a = int(m.group(1), 0)
        b = int(m.group(2), 0) if m.group(2) else a
        if not (0 <= a <= b <= 0xFF):
            raise ValueError(part)
        out.update(range(a, b + 1))
    return sorted(out)


def compress(items):
    """[1,2,3,7] -> '1-3,7' (decimal, as COS_ITEM_SWEEP takes it)."""
    parts, i = [], 0
    while i < len(items):
        j = i
        while j + 1 < len(items) and items[j + 1] == items[j] + 1:
            j += 1
        parts.append("%d" % items[i] if i == j else "%d-%d" % (items[i], items[j]))
        i = j + 1
    return ",".join(parts)


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("-h", "--help", action="store_true")
    ap.add_argument("--stage", default="Asoko:0:0:2")
    ap.add_argument("--range", default="0x00-0xFE")
    ap.add_argument("--timeout", type=int, default=7200)
    ap.add_argument("--uncapped", action="store_true")
    ap.add_argument("--max-runs", type=int, default=100)
    ap.add_argument("--disc", default=os.environ.get("COS_DISC", ""))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--rebuild", default=None)
    args = ap.parse_args()
    if args.help:
        print(__doc__)
        return 0
    try:
        wanted = parse_list(args.range)
    except ValueError as e:
        print("item_sweep: bad --range part %s" % e, file=sys.stderr)
        return 2
    names = item_names()
    if args.rebuild:
        # The report again from an existing sweep directory's runs (no new run).
        out = args.rebuild
        rebuild_dirs = sorted((d for d in os.listdir(out) if d.startswith("from-")), key=lambda d: int(d[5:], 16))
    else:
        out = args.out or os.path.join(BUILD, "runs", "item-sweeps-" + time.strftime("%Y%m%d-%H%M%S"))
        os.makedirs(out, exist_ok=False)
        rebuild_dirs = None

    results = collections.OrderedDict()  # item -> dict
    remaining = list(wanted)
    runs = 0
    while remaining and runs < args.max_runs:
        runs += 1
        if rebuild_dirs is not None:
            if not rebuild_dirs:
                break
            run_dir = os.path.join(out, rebuild_dirs.pop(0))
        else:
            run_dir = os.path.join(out, "from-%02X" % remaining[0])
        cmd = [COS_RUN, "item-sweep", "--stage", args.stage, "--timeout", str(args.timeout), "--disc", args.disc,
               "--quiet", "--run-dir", run_dir]
        if args.uncapped:
            cmd.append("--uncapped")
        if args.exe:
            cmd += ["--exe", args.exe]
        env = dict(os.environ, COS_ITEM_SWEEP=compress(remaining), COS_CACHE_PER_RUN="1")
        env.pop("COS_ITEM_SWEEP_SAVE", None)
        if rebuild_dirs is None:
            subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
        except ValueError:
            rc = -1
        boot_sweep.collapse_log(os.path.join(run_dir, "run.log"))
        log = boot_sweep.read(os.path.join(run_dir, "run.log"))
        begun = None
        done = set()
        for line in boot_sweep.read(os.path.join(run_dir, "item_sweep.txt")).splitlines():
            f = line.split()
            if len(f) < 3 or line.startswith("#"):
                continue
            item = int(f[0], 16)
            if f[2] == "begin":
                begun = (item, f[1])
                continue
            r = {"arc": f[1], "result": f[2].split(":")[0], "frames": "", "secs": "", "got": "", "shot": "",
                 "detail": "", "run": os.path.relpath(run_dir, out)}
            if f[2] in ("ok", "stuck") and len(f) >= 5:
                r["frames"], r["secs"] = f[3], f[4]
            kv = dict(x.split("=", 1) for x in f[5:] if "=" in x)
            r["got"] = kv.get("got", "")
            if kv.get("shot", "0") != "0":
                r["shot"] = "shot-%s.png" % kv["shot"]
            if f[2] == "stuck":
                r["detail"] = " ".join(x for x in f[5:] if not x.startswith("shot="))
            elif f[2] in ("no-open",):
                r["frames"] = f[3] if len(f) > 3 else ""
                r["detail"] = " ".join(f[4:])
            elif f[2].startswith("scene-lost"):
                r["detail"] = f[2]
            elif f[2] in ("chest-not-deleted", "stuck-after"):
                # A follow-up of the item already reported: note it on that item.
                if item in results:
                    results[item]["detail"] = (results[item]["detail"] + " " + " ".join(f[2:])).strip()
                done.add(item)
                begun = None
                continue
            results[item] = r
            done.add(item)
            begun = None
        last_done = max(done) if done else None
        if begun is not None and begun[0] not in done:
            item = begun[0]
            if rc != 0:
                results[item] = {"arc": begun[1], "result": "fault", "frames": "", "secs": "", "got": "",
                                 "shot": "", "detail": "exit %d %s: %s" % (rc, boot_sweep.MEANING.get(rc, "?"),
                                                                          boot_sweep.signature(run_dir, rc, log)),
                                 "run": os.path.relpath(run_dir, out)}
                done.add(item)
                print("%02X %s: exit %d: %s" % (item, names.get(item, "?"), rc, results[item]["detail"]),
                      file=sys.stderr)
        elif rc not in (0, 1) or (rc == 1 and not done):
            # A fault before the first item or between two items.
            where = last_done if last_done is not None else -1
            sig = boot_sweep.signature(run_dir, rc, log)
            print("run from %02X: exit %d with no item in flight: %s" % (remaining[0], rc, sig), file=sys.stderr)
            if where in results:
                results[where]["detail"] = (results[where]["detail"] + " after: exit %d %s" % (rc, sig)).strip()
            if not done:
                break
        if done:
            top = max(done)
            remaining = [i for i in remaining if i > top]
        elif rc == 0:
            remaining = []
        if rc == 0:
            # Every remaining item was skipped (no archive) or done.
            break

    for item, (needle, reason) in EXPECTED.items():
        r = results.get(item)
        if r is None:
            continue
        if r["result"] == "ok":
            r["result"] = "xpass"
        elif needle in r["detail"]:
            r["result"] = "xfail"
            r["detail"] += " | expected: " + reason
    counts = collections.Counter(r["result"] for r in results.values())
    report = os.path.join(out, "item_sweep.txt")
    with open(report, "w") as f:
        f.write("# item_sweep.py: stage %s, items %s, %d runs\n" % (args.stage, args.range, runs))
        f.write("item\tname\tarc\tresult\tframes\tsecs\tgot\tshot\tdetail\trun\n")
        for item in sorted(results):
            r = results[item]
            f.write("%02X\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (
                item, names.get(item, "?"), r["arc"], r["result"], r["frames"], r["secs"], r["got"], r["shot"],
                r["detail"], r["run"]))
        skipped = [i for i in wanted if i not in results]
        f.write("# results: %s; not given (no archive, or not reached): %d\n"
                % (", ".join("%s %d" % kv for kv in sorted(counts.items())), len(skipped)))
        if remaining:
            f.write("# not reached (max runs or a fault before any item): %s\n" % compress(remaining))
    print(open(report).read(), end="")
    bad = sum(n for k, n in counts.items() if k not in ("ok", "xfail"))
    return 0 if bad == 0 and not remaining else 1


if __name__ == "__main__":
    sys.exit(main())
