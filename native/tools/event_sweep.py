#!/usr/bin/env python3
"""Start and run every event of every stage's event list, going on after faults.

  native/tools/event_sweep.py [options]

For each stage of the disc (the start boot_sweep.py picks from the stage data: one run per stage,
since the event list is the stage's, Stage.arc event_list.dat), runs
`run.sh event-sweep --stage <spec> --uncapped --heap-check N` (COS_SMOKE=event-sweep,
native/src/pc/pc_event_sweep.cpp: each event of the list is ordered with the player as the ordering
actor, A is pressed for its messages, and it runs until it ends, gets stuck or times out). A run ends
early on a fault (crash, panic, stall, heap damage), a stage change asked for by an event, or a lost
scene; the next run boots again and starts after the event it ended on (COS_EVENT_SWEEP=<n>-), until
the smoke writes "done". Every run has COS_CACHE_PER_RUN=1: its pipelines land in
<run dir>/cache/pipeline_cache.db (native/tools/gen_pipeline_cache.sh merges them).

Each run lands in <sweep dir>/<stage>/from-<n>/ (the usual run.sh run directory). Reports:
  <sweep dir>/event_sweep.tsv  one line per event: stage, index, name, result, frames, seconds,
                               missing cast, waiting staff / next stage / other details, exit code,
                               signature, run directory;
  <sweep dir>/event_sweep.md   counts, the distinct fault signatures with their events, the events
                               stuck on an engine staff, the stages that did not boot.
Results: end, end-other (another event followed), not-started, stage-change (the event asked for
another stage: the run ends there), scene-lost, and for an event that made no progress or timed
out (the staff still waiting are listed as <name>:<cut>:<staff type>):
  stuck-missing-cast  every waiting staff is an actor not in the debug-booted stage;
  stuck-actor         every waiting staff is an actor (present, a partner stand-in such as TALKMAN
                      or TREASURE, or the player) that did not play its part: actors play only the
                      events they order themselves, in the state they order them from;
  stuck               an engine staff (camera, message, demo package, timekeeper, ...) waits;
the first two are harness artifacts. FAULT: the run ended on the event (exit code and signature as
boot_sweep.py writes them); FAULT-after: the fault came after the event had ended, before the next
one; boot-fail: the stage did not reach its first event (xfail when boot_sweep.py lists the stage
as an expected fail with that signature).
Exit 0 when no event faulted (FAULT/FAULT-after) and every stage booted or is an xfail; 1 otherwise.

Options:
  --jobs N         stages at a time (default 2)
  --only LIST      comma-separated stage names (default every stage with a spawn point)
  --timeout S      wall seconds per event before it counts as stuck (COS_EVENT_SWEEP_SECONDS,
                   default 45)
  --run-timeout S  run.sh --timeout per run (default 3600)
  --heap-check N   run.sh --heap-check (default 30; 0 = off)
  --max-runs N     runs per stage at most (default 60)
  --disc PATH      the GZLE01 .iso (default COS_DISC)
  --exe PATH       the executable (default build/native-mac/centollos)
  --out DIR        the sweep directory (default build/native-mac/runs/event-sweep-<timestamp>)
  --all-events     every event of every stage's list (default: see below)
  --common-min N   an event name in at least N stages' lists is a common one (default 13)
  --common-in LIST the stages the common events run in (default sea,M_NewD2,Asoko,Omori; a
                   common event none of them has runs in the first stage that has it)
  --events LIST    only these event indices (e.g. 0-5,9), with --only for a single stage
The event lists are read from the disc (the names are checked against the game's). Nearly every list
carries the same ~100 generic events (doors, chests, item gets, warps); the rest are the stage's
own. By default each stage runs its own events and the generic ones run in the --common-in stages
only; <sweep dir>/selection.txt lists what each stage runs.
Nothing here is meant for git.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures
import importlib.util
import os
import re
import subprocess
import sys
import threading
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
BUILD = os.path.join(REPO, "build", "native-mac")
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")

_spec = importlib.util.spec_from_file_location("boot_sweep", os.path.join(SCRIPT_DIR, "boot_sweep.py"))
boot_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boot_sweep)
_spec = importlib.util.spec_from_file_location("disc_manifest", os.path.join(SCRIPT_DIR, "disc_manifest.py"))
dm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(dm)

PRINT_LOCK = threading.Lock()
FAULTS = ("FAULT", "FAULT-after")


def log(msg):
    with PRINT_LOCK:
        print(msg, flush=True)


def event_lists(disc):
    """{stage: [event names in event_list.dat order]} read from each /res/Stage/<stage>/Stage.arc
    (dat/event_list.dat: the header's event offset and count, 0xB0-byte events, name first)."""
    out = {}
    with open(disc, "rb") as f:
        entries, paths = dm.read_fst(f)
        for e, p in zip(entries, paths):
            m = boot_sweep.STAGE_ARC.match(p)
            if not e or e["dir"] or not m:
                continue
            f.seek(e["offset"])
            b = f.read(e["size"])
            if b[:4] == b"Yaz0":
                b = dm.yaz0_decompress(b)
            info = 0x20
            num_entries = dm.u32(b, info + 8)
            entry_off = info + dm.u32(b, info + 0xC)
            str_off = info + dm.u32(b, info + 0x14)
            data_base = 0x20 + dm.u32(b, 0xC)
            names = []
            for i in range(num_entries):
                o = entry_off + i * 0x14
                tf = dm.u32(b, o + 4)
                if (tf >> 24) & 2 or dm.cstr(b, str_off + (tf & 0xFFFFFF)) != "event_list.dat":
                    continue
                d = b[data_base + dm.u32(b, o + 8):][:dm.u32(b, o + 0xC)]
                if d[:4] == b"Yaz0":
                    d = dm.yaz0_decompress(d)
                top, num = dm.u32(d, 0), dm.s32(d, 4)
                names = [dm.cstr(d, top + k * 0xB0, 32) for k in range(num)]
            out[m.group(1)] = names
    return out


def select_events(lists, stages, common_min, common_in, all_events):
    """{stage: [indices]}: every event of a stage, or (default) each event name found in fewer than
    common_min stages' lists in every stage that has it, and the common ones (the generic events
    nearly every list carries) only in the common_in stages that have them (else in the first stage
    that has it)."""
    if all_events:
        return {st: list(range(len(lists.get(st, [])))) for st in stages}
    count = collections.Counter(n for st in stages for n in set(lists.get(st, [])))
    sel = {st: set() for st in stages}
    chosen = {}
    for n in count:
        if count[n] >= common_min:
            having = [st for st in sorted(stages) if n in lists.get(st, [])]
            chosen[n] = set([st for st in common_in if st in having] or having[:1])
    for st in stages:
        for i, n in enumerate(lists.get(st, [])):
            if count[n] < common_min or st in chosen[n]:
                sel[st].add(i)
    return {st: sorted(v) for st, v in sel.items()}


def ranges(indices):
    """[1, 2, 3, 7] -> "1-3,7"."""
    out, i = [], 0
    while i < len(indices):
        j = i
        while j + 1 < len(indices) and indices[j + 1] == indices[j] + 1:
            j += 1
        out.append(str(indices[i]) if i == j else "%d-%d" % (indices[i], indices[j]))
        i = j + 1
    return ",".join(out)


def parse_details(fields):
    d = {}
    for f in fields:
        k, _, v = f.partition("=")
        d[k] = v
    return d


def parse_run(path):
    """(num events or None, done, [(idx, name, begin details)], {idx: result dict}) of one
    event_sweep.txt."""
    num, done, begins, results = None, False, [], {}
    for line in boot_sweep.read(path).splitlines():
        m = re.match(r"^# event-sweep .*: (\d+) events", line)
        if m:
            num = int(m.group(1))
            continue
        if line.startswith("done "):
            done = True
            continue
        f = line.split()
        if len(f) < 3 or not f[0].isdigit():
            continue
        idx, name, what = int(f[0]), f[1], f[2]
        if what == "begin":
            begins.append((idx, name, parse_details(f[3:])))
            continue
        r = {"idx": idx, "name": name, "result": what,
             "frames": f[3] if len(f) > 3 else "-", "seconds": f[4] if len(f) > 4 else "-"}
        r.update(parse_details(f[5:]))
        results[idx] = r
    return num, done, begins, results


# The NORMAL staff names that stand for the event's partner actor (resolved from the actor that
# ordered the event: the one talked to, the chest, the door, ...), which the harness's player order
# does not provide.
PARTNER_STAFF = {"TALKMAN", "GIVEMAN", "WINDMAN", "TREASURE", "SHUTTER_DOOR", "TAGHINT", "EXTRA"}


def classify(r):
    """stuck-missing-cast: every waiting staff is an actor not in the stage; stuck-actor: every
    waiting staff is an actor (present, the partner stand-ins or the player) that did not play its
    part, as actors play only the events they order themselves; stuck: an engine staff (camera,
    message, demo package, timekeeper, ...) waits: the ones to look at."""
    if r["result"] != "stuck":
        return r["result"]
    missing = set(filter(None, r.get("missing", "").split("|")))
    waiting = [w.split(":") for w in r.get("waiting", "").split("|") if w.count(":") >= 2]
    if not waiting:
        return "stuck"
    if missing and all(w[0] in missing for w in waiting):
        return "stuck-missing-cast"
    if all(w[2] == "0" or w[0] in PARTNER_STAFF for w in waiting):
        return "stuck-actor"
    return "stuck"


def sweep_stage(args, out, stage, room, point, todo, names):
    spec = "%s:%d:%d" % (stage, room, point)
    rows = []
    stage_dir = os.path.join(out, stage)
    os.makedirs(stage_dir, exist_ok=True)
    runs, num, boot = 0, len(names), None
    todo = sorted(todo)
    while todo and runs < args.max_runs:
        runs += 1
        start = todo[0]
        run_dir = os.path.join(stage_dir, "from-%d" % start)
        cmd = [COS_RUN, "event-sweep", "--stage", spec, "--uncapped", "--timeout", str(args.run_timeout),
               "--stall", "30", "--disc", args.disc, "--quiet", "--run-dir", run_dir]
        if args.heap_check:
            cmd += ["--heap-check", str(args.heap_check)]
        if args.exe:
            cmd += ["--exe", args.exe]
        env = dict(os.environ, COS_EVENT_SWEEP=ranges(todo), COS_CACHE_PER_RUN="1",
                   COS_EVENT_SWEEP_SECONDS=str(args.timeout))
        t0 = time.time()
        subprocess.run(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
        except ValueError:
            rc = -1
        boot_sweep.collapse_log(os.path.join(run_dir, "run.log"))
        text = boot_sweep.read(os.path.join(run_dir, "run.log"))
        sig = boot_sweep.signature(run_dir, rc, text)
        n, done, begins, results = parse_run(os.path.join(run_dir, "event_sweep.txt"))
        if n is not None and n != num:
            log("%s: the game's list has %d events, the disc's %d" % (stage, n, num))
        rel = os.path.relpath(run_dir, out)
        for idx in sorted(results):
            r = results[idx]
            if idx < len(names) and names[idx] != r["name"]:
                log("%s: event %d is %s in the game, %s on the disc" % (stage, idx, r["name"], names[idx]))
            r.update(stage=stage, rc=0, signature="", run=rel, cls=classify(r))
            begin = next((b for b in begins if b[0] == idx), None)
            if begin and "missing" not in r:
                r["missing"] = begin[2].get("missing", "-")
            rows.append(r)
        log("%s from %d: exit %d after %ds, %d events%s" % (stage, start, rc, time.time() - t0, len(results),
                                                             "" if rc == 0 else ": " + sig))
        if done:
            break
        in_flight = [b for b in begins if b[0] not in results]
        if n is None:
            # The stage never reached its first event.
            boot = {"stage": stage, "spec": spec, "rc": rc, "signature": sig, "run": rel}
            break
        if in_flight:
            idx, name, d = in_flight[-1]
            rows.append({"stage": stage, "idx": idx, "name": name, "result": "FAULT", "cls": "FAULT",
                         "frames": "-", "seconds": "-", "missing": d.get("missing", "-"), "rc": rc,
                         "signature": sig, "run": rel})
            todo = [i for i in todo if i > idx]
        elif results:
            last = max(results)
            if rc != 0:
                rows.append({"stage": stage, "idx": last, "name": results[last]["name"], "result": "FAULT-after",
                             "cls": "FAULT-after", "frames": "-", "seconds": "-", "missing": "-", "rc": rc,
                             "signature": sig, "run": rel})
            todo = [i for i in todo if i > last]
        else:
            # A fault (or stop) before the first event of this run: no way forward.
            boot = {"stage": stage, "spec": spec, "rc": rc, "signature": "no event ran from %d: %s" % (start, sig),
                    "run": rel}
            break
    return {"stage": stage, "spec": spec, "num": num, "rows": rows, "boot": boot, "runs": runs}


def md_escape(s):
    return str(s).replace("|", "\\|")


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("-h", "--help", action="store_true")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--only", default=None)
    ap.add_argument("--timeout", type=float, default=45)
    ap.add_argument("--run-timeout", type=int, default=3600)
    ap.add_argument("--heap-check", type=int, default=30)
    ap.add_argument("--max-runs", type=int, default=60)
    ap.add_argument("--disc", default=os.environ.get("COS_DISC", ""))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--out", default=None)
    ap.add_argument("--all-events", action="store_true")
    ap.add_argument("--common-min", type=int, default=13)
    ap.add_argument("--common-in", default="sea,M_NewD2,Asoko,Omori")
    ap.add_argument("--events", default=None)
    args = ap.parse_args()
    if args.help:
        print(__doc__)
        return 0
    if not args.disc:
        print("event_sweep: no disc (--disc or COS_DISC)", file=sys.stderr)
        return 14

    manifest = boot_sweep.load_manifest(args.disc)
    starts = [s for s in boot_sweep.choose_starts(manifest) if s[1] is not None]
    lists = event_lists(args.disc)
    todo = select_events(lists, [s[0] for s in starts], args.common_min, args.common_in.split(","),
                         args.all_events)
    if args.only:
        only = set(args.only.split(","))
        starts = [s for s in starts if s[0] in only]
    if args.events:
        want = set()
        for part in args.events.split(","):
            a, _, b = part.partition("-")
            want.update(range(int(a), int(b or a) + 1))
        todo = {st: [i for i in range(len(lists.get(st, []))) if i in want] for st in todo}
    out = args.out or os.path.join(BUILD, "runs", "event-sweep-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=True)
    log("event-sweep: %d stages, %d of their %d events selected, %d jobs, into %s" % (
        len(starts), sum(len(v) for v in todo.values()), sum(len(lists.get(s[0], [])) for s in starts), args.jobs,
        out))
    with open(os.path.join(out, "selection.txt"), "w") as f:
        for st, room, point, _ in starts:
            f.write("%s %d:%d %d/%d %s\n" % (st, room, point, len(todo[st]), len(lists.get(st, [])), ranges(todo[st])))

    t0 = time.time()
    stages = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        # The stages with the most events first, so the long ones do not come last.
        order = sorted(starts, key=lambda s: -len(todo[s[0]]))
        futs = [pool.submit(sweep_stage, args, out, st, room, point, todo[st], lists.get(st, []))
                for st, room, point, _ in order if todo[st]]
        for f in concurrent.futures.as_completed(futs):
            stages.append(f.result())
    stages.sort(key=lambda s: s["stage"])

    rows = [r for s in stages for r in s["rows"]]
    rows.sort(key=lambda r: (r["stage"], r["idx"], r["result"] in FAULTS))
    with open(os.path.join(out, "event_sweep.tsv"), "w") as f:
        f.write("stage\tidx\tname\tresult\tframes\tseconds\tmissing\tdetails\texit\tsignature\trun\n")
        for r in rows:
            det = " ".join("%s=%s" % (k, r[k]) for k in ("waiting", "why", "next", "running") if r.get(k))
            f.write("%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (
                r["stage"], r["idx"], r["name"], r["cls"], r["frames"], r["seconds"], r.get("missing", "-"),
                det or "-", r["rc"], r["signature"] or "-", r["run"]))

    counts = collections.Counter(r["cls"] for r in rows)
    boots = [s["boot"] for s in stages if s["boot"]]
    for b in boots:
        exp = boot_sweep.EXPECTED_FAIL.get(b["stage"])
        b["xfail"] = bool(exp and re.search(exp[0], b["signature"]))
    sigs = collections.defaultdict(list)
    for r in rows:
        if r["cls"] in FAULTS:
            sigs[r["signature"]].append(r)
    total_events = sum(s["num"] or 0 for s in stages)
    with open(os.path.join(out, "event_sweep.md"), "w") as f:
        f.write("# Event sweep %s\n\n" % time.strftime("%Y-%m-%d %H:%M"))
        f.write("%d stages (%d did not boot, %d of them expected), %d events in their lists, %d results, "
                "%d runs, %.0f min.\n\n" % (len(stages), len(boots), sum(b["xfail"] for b in boots), total_events,
                                            len(rows), sum(s["runs"] for s in stages), (time.time() - t0) / 60))
        f.write("| result | events |\n|---|---|\n")
        for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
            f.write("| %s | %d |\n" % (k, v))
        f.write("\n## Distinct fault signatures\n\n")
        if not sigs:
            f.write("None.\n")
        for sig, rs in sorted(sigs.items(), key=lambda kv: -len(kv[1])):
            f.write("- `%s` (%d): %s\n" % (sig, len(rs), ", ".join(
                "%s#%d %s%s (%s)" % (r["stage"], r["idx"], r["name"], " after" if r["cls"] == "FAULT-after" else "",
                                     r["run"]) for r in rs)))
        f.write("\n## Stages that did not boot to their first event\n\n")
        if not boots:
            f.write("None.\n")
        for b in boots:
            f.write("- %s (%s): exit %s %s%s (%s)\n" % (b["stage"], b["spec"], b["rc"], b["signature"],
                                                       " [xfail]" if b["xfail"] else "", b["run"]))
        f.write("\n## Stuck on an engine staff (camera, message, demo package, ...)\n\n| stage | idx | event | why | waiting | run |\n"
                "|---|---|---|---|---|---|\n")
        for r in rows:
            if r["cls"] == "stuck":
                f.write("| %s | %d | %s | %s | %s | %s |\n" % (r["stage"], r["idx"], md_escape(r["name"]),
                                                             r.get("why", ""), md_escape(r.get("waiting", "")),
                                                             r["run"]))
        f.write("\n## Per stage\n\n| stage | spec | events | runs | results |\n|---|---|---|---|---|\n")
        for s in stages:
            c = collections.Counter(r["cls"] for r in s["rows"])
            f.write("| %s | %s | %s | %d | %s |\n" % (s["stage"], s["spec"], s["num"] if s["num"] is not None else "-",
                                                     s["runs"], ", ".join("%s %d" % kv for kv in sorted(c.items()))))
    print(open(os.path.join(out, "event_sweep.md")).read().split("## Stuck")[0], end="")
    bad = bool(sigs) or any(not b["xfail"] for b in boots)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
