#!/usr/bin/env python3
"""Spawn every actor profile next to Link in Outset, going on after faults (docs/NATIVE_PORT_PHASE4_6.md,
step 6.9 robustness).

  native/tools/actor_sweep.py [options]

Runs `run.sh actor-sweep --stage sea:44:206 --uncapped` (COS_SMOKE=actor-sweep,
native/src/pc/pc_actor_sweep.cpp: each actor profile is created next to Link, run 30 game frames and
deleted; a refused creation is fine). A fault ends that run; the profile it was on is the last
"begin" line of the run's actor_sweep.txt. The next run starts after it (COS_ACTOR_SWEEP=<n>-<last>),
until the last profile is done. Each run lands in <sweep dir>/from-<n>/ (the usual run.sh run
directory); the report is <sweep dir>/actor_sweep.txt. Exit 0 only if no profile faulted; 1 otherwise.

Report: one line per profile that ended a run, tab-separated: process name (fpcNm number), profile
(g_profile_<name>, from game/src/f_pc/f_pc_profile_lst.cpp), dStage name ("(after-delete)"
when the fault came after that profile's actor was deleted, before the next one), exit code and
meaning, signature (the crash/panic/stall site as boot_sweep.py writes it), run directory.
Then the counts of the per-profile results of every run (ran, refused, deleted-itself, creating).

Options:
  --stage SPEC    COS_BOOT_STAGE (default sea:44:206, Outset)
  --range A-B     process names to sweep (default every one)
  --timeout S     run.sh --timeout per run (default 300)
  --max-runs N    stop after N runs (default 200)
  --disc PATH     the GZLE01 .iso (default COS_DISC or the maintainer's image)
  --exe PATH      the executable (default build/native-mac/centollos)
  --out DIR       the sweep directory (default build/native-mac/runs/actor-sweeps-<timestamp>)
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
PROFILE_LST = os.path.join(REPO, "game", "src", "f_pc", "f_pc_profile_lst.cpp")

_spec = importlib.util.spec_from_file_location("boot_sweep",
                                               os.path.join(SCRIPT_DIR, "boot_sweep.py"))
boot_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boot_sweep)


def profile_names():
    """g_fpcPfLst_ProfileList in order (index = fpcNm process name), from the TARGET_PC list, with
    its `#if VERSION ==/!= VERSION_<X>` lines taken for the USA build (VERSION_USA, GZLE01)."""
    text = boot_sweep.read(PROFILE_LST)
    m = re.search(r"#if TARGET_PC.*?g_fpcPfLst_ProfileList\[\] = \{(.*?)\};", text, re.S)
    if not m:
        return []
    names, keep = [], [True]
    for line in m.group(1).splitlines():
        c = re.match(r"\s*#if VERSION (==|!=) VERSION_(\w+)", line)
        if c:
            keep.append(keep[-1] and ((c.group(2) == "USA") == (c.group(1) == "==")))
            continue
        if re.match(r"\s*#endif", line):
            keep.pop()
            continue
        e = re.match(r"\s*(?:&g_profile_(\w+)[.\w]*|(NULL)),", line)
        if e and keep[-1]:
            names.append(e.group(1) or e.group(2))
    return names


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("-h", "--help", action="store_true")
    ap.add_argument("--stage", default="sea:44:206")
    ap.add_argument("--range", default=None)
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--max-runs", type=int, default=200)
    ap.add_argument("--disc", default=os.environ.get("COS_DISC") or boot_sweep.LEGACY_DISC)
    ap.add_argument("--exe", default=None)
    ap.add_argument("--out", default=None)
    args = ap.parse_args()
    if args.help:
        print(__doc__)
        return 0

    names = profile_names()
    first, last = 0, (len(names) - 2 if names else 0)
    if args.range:
        m = re.fullmatch(r"(\d+)-(\d+)", args.range)
        if not m:
            print("actor_sweep: --range needs A-B", file=sys.stderr)
            return 2
        first, last = int(m.group(1)), int(m.group(2))
    out = args.out or os.path.join(BUILD, "runs", "actor-sweeps-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=False)

    faults = []
    results = collections.Counter()
    start = first
    runs = 0
    while start <= last and runs < args.max_runs:
        runs += 1
        run_dir = os.path.join(out, "from-%d" % start)
        cmd = [COS_RUN, "actor-sweep", "--stage", args.stage, "--uncapped", "--timeout",
               str(args.timeout), "--disc", args.disc, "--quiet", "--run-dir", run_dir]
        if args.exe:
            cmd += ["--exe", args.exe]
        env = dict(os.environ, COS_ACTOR_SWEEP="%d-%d" % (start, last))
        subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
        except ValueError:
            rc = -1
        boot_sweep.collapse_log(os.path.join(run_dir, "run.log"))
        log = boot_sweep.read(os.path.join(run_dir, "run.log"))
        lines = boot_sweep.read(os.path.join(run_dir, "actor_sweep.txt")).splitlines()
        begun = None
        for line in lines:
            f = line.split()
            if len(f) >= 3 and f[0].isdigit():
                if f[2] == "begin":
                    begun = (int(f[0]), f[1])
                else:
                    results[f[2]] += 1
                    begun = None
        if rc == 0:
            break
        if begun is None:
            # No profile in flight: the run failed before the sweep or between two profiles.
            last_done = max([int(l.split()[0]) for l in lines if l[:1].isdigit()] or [start - 1])
            # The fault came between two profiles: after the deletion of the last one done (a child
            # actor it made, still running without it, is the usual case).
            faults.append((last_done, "(after-delete)", rc, boot_sweep.signature(run_dir, rc, log),
                           run_dir))
            print("run from %d: exit %d with no profile in flight" % (start, rc), file=sys.stderr)
            if last_done < start:
                break
            start = last_done + 1
            continue
        proc, dname = begun
        sig = boot_sweep.signature(run_dir, rc, log)
        faults.append((proc, dname, rc, sig, run_dir))
        print("%d %s: exit %d: %s" % (proc, names[proc] if proc < len(names) else "?", rc, sig),
              file=sys.stderr)
        start = proc + 1

    report = os.path.join(out, "actor_sweep.txt")
    with open(report, "w") as f:
        f.write("# actor_sweep.py: stage %s, process names %d-%d, %d runs\n"
                % (args.stage, first, last, runs))
        f.write("proc\tprofile\tdstage\texit\tsignature\trun\n")
        for proc, dname, rc, sig, run_dir in faults:
            f.write("%d\t%s\t%s\t%d %s\t%s\t%s\n" % (
                proc, names[proc] if 0 <= proc < len(names) else "?", dname, rc,
                boot_sweep.MEANING.get(rc, "?"), sig, os.path.relpath(run_dir, out)))
        f.write("# results of the profiles that finished: %s\n"
                % ", ".join("%s %d" % kv for kv in sorted(results.items())))
    print(open(report).read(), end="")
    return 0 if not faults else 1


if __name__ == "__main__":
    sys.exit(main())
