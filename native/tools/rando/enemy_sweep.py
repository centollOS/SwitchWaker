#!/usr/bin/env python3
"""enemy_sweep.py - the enemy randomizer's room sweep (docs/RANDOMIZER_PLAN.md step R7.4d), Mac only.

    python3 -I native/tools/rando/enemy_sweep.py [--seeds A,B,C | --vanilla] [--jobs N] [--frames N]
                                                 [--only STAGE[:ROOM],...] [--batch N] [--list]
                                                 [--cli CLI] [--exe EXE] [--disc ISO] [--out DIR]

For each seed (default: three seeds, EnemySweepA,EnemySweepB,EnemySweepC) the placement is generated with
our CLI (build/native-mac/cos_rando_cli randomize_enemies=true, the other options at wwrando's defaults).
Then every room of the 222 groups of wwrando's logic/enemy_locations.txt is booted: one stop per
(stage, room, layer) the groups' enemy paths name (214 stops), on the room's spawn point (a point an SCLS
exit leads to when there is one, else the lowest; native/tools/room_sweep.py's choice from the disc's
PLYR records), or point 0 of the stage for the 22 rooms without a spawn point of their own (the debug
boot then still makes that room the stay room: the Savage Labyrinth's lower floors, TF_01/TF_02's
rooms 1-5). A stop with a "LayerN" path is booted on layer N, else on the default layer (-1).

The stops of one stage are one run (at most --batch per run): `run.sh rando --stage <first stop>` with
COS_RANDO_SMOKE=enemysweep (native/src/pc/harness/smokes/pc_rando_smoke_enemysweep.cpp),
COS_RANDO_PLACEMENT, COS_RANDO_SWEEP_HOPS=<the other stops> (stage changes, as the game's exits) and
COS_CACHE_PER_RUN=1. Per stop the smoke checks: the stop's room reached; every enemy location of the
room on its layer written by the patcher and its actor created (live at its home), except spawns that
wait for a switch; the room's memory block and the main/game/archive heaps logged ("heap" line, kept in
the report); --frames frames in the room (default 300) without a crash; must-defeat groups complete
once their enemies are removed (every live ALLdie sets its switch, or no enemy is left for 70 frames);
no heap allocation failure ("[cos] heap: cannot alloc", pc_heap.cpp) from the stage change on.
A run that crashes or times out leaves its later stops unrun: they are run again as a new batch from
the next stop, and the failing stop alone (booted directly) once, to tell a hop problem from a room
problem; both results are reported.

--vanilla runs the same stops without a placement (no COS_RANDO_PLACEMENT; the game's own enemies are
checked the same way). Every failing stop of a seed is also run vanilla, alone, so the report says
whether the failure is the randomizer's ("rando only") or the game's ("vanilla too").
KNOWN_NON_RANDO lists the failures that are not the randomizer's, with their reason; a failing stop
matching one (and failing vanilla too) is "xfail", not a failure.

Report: <out>/enemy_sweep.md and enemy_sweep.tsv (one row per seed and stop: result, the failed checks
or the crash signature, the room block's free bytes); each run's directory under <out>/runs/.
Exit 0 when no stop of any seed is FAIL, 1 otherwise, 2 usage or setup error, 14 disc problem.
At most 3 runs at a time (--jobs, default 3). Nothing read from the disc is written to the repository.
"""
import argparse
import collections
import concurrent.futures
import importlib.util
import json
import os
import re
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import gen_rando_data as grd  # noqa: E402  (yaml_load of the wwrando data)

TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REPO = os.path.normpath(os.path.join(TOOLS, "..", ".."))
RUN_SH = os.path.join(TOOLS, "run.sh")

_spec = importlib.util.spec_from_file_location("room_sweep", os.path.join(TOOLS, "room_sweep.py"))
room_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(room_sweep)
boot_sweep = room_sweep.boot_sweep

DEFAULT_SEEDS = "EnemySweepA,EnemySweepB,EnemySweepC"
PATH_RE = re.compile(r"^([^/]+)/Room(\d+)\.arc(?:/Layer([0-9a-b]))?/Actor([0-9A-F]{3})$")
CHECK_RE = re.compile(r"^\[cos\] rando-smoke enemysweep: (stop (\d+) \(([^)]*)\)[^\n]*): (ok|FAIL)$", re.M)
HEAP_RE = re.compile(r"^\[cos\] rando-smoke enemysweep: heap stop (\d+) \([^)]*\): room (.*)$", re.M)
BLOCK_RE = re.compile(r"block \d+ size (0x[0-9a-f]+) free (0x[0-9a-f]+)")

# (stage, room) -> (signature regex, reason): failures that are not the randomizer's. Each must also
# fail vanilla (checked by the sweep) to count as xfail.
KNOWN_NON_RANDO = {
    ("kindan", 0): (r"unmapped vtx attr 9|PANIC|CRASH",
                    "Aurora fatal 'unmapped vtx attr 9' on a stage change into kindan room 0, also without "
                    "any placement (not the randomizer; reported separately, not fixed here)"),
}


class Stop:
    def __init__(self, stage, room, layer, groups, must_defeat):
        self.stage, self.room, self.layer = stage, room, layer
        self.groups, self.must_defeat = groups, must_defeat
        self.point = None
        self.own_spawn = True

    @property
    def key(self):
        return "%s:%d:%d" % (self.stage, self.room, -1 if self.layer is None else self.layer)

    def spec(self):
        return "%s:%d:%d:%d" % (self.stage, self.room, self.point, -1 if self.layer is None else self.layer)


def table_stops():
    """The stops of enemy_locations.txt, in file order, and the number of groups."""
    locs = grd.yaml_load("logic/enemy_locations.txt")
    stops = collections.OrderedDict()
    ngroups = 0
    for _key, groups in locs.items():
        for g in groups:
            seen = set()
            for e in g["Enemies"]:
                m = PATH_RE.match(e["Path"])
                if not m:
                    oc.die("unexpected enemy path %s" % e["Path"])
                k = (m.group(1), int(m.group(2)), None if m.group(3) is None else int(m.group(3), 16))
                if k in seen:
                    continue
                seen.add(k)
                if k not in stops:
                    stops[k] = Stop(k[0], k[1], k[2], [], False)
                stops[k].groups.append(ngroups)
                stops[k].must_defeat = stops[k].must_defeat or bool(g["Must defeat enemies"])
            ngroups += 1
    return list(stops.values()), ngroups


def choose_points(stops, disc):
    _stages, _rooms, points, exits = room_sweep.spawn_points(boot_sweep.load_manifest(disc))
    for s in stops:
        pts = points.get(s.stage, {}).get(s.room, [])
        if pts:
            entries = [p for p in pts if (s.stage, s.room, p) in exits]
            s.point = (entries or pts)[0]
        else:
            s.point = 0  # the debug boot makes the asked room the stay room anyway
            s.own_spawn = False


def gen_placement(cli, seed, out_dir):
    path = os.path.join(out_dir, "placement-%s.json" % seed)
    r = subprocess.run([cli, "randomize_enemies=true", "--json", path, "--no-logs", "--quiet", seed],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0 or not os.path.isfile(path):
        oc.die("cos_rando_cli failed for %s:\n%s" % (seed, r.stdout))
    with open(path) as f:
        if "enemies" not in json.load(f):
            oc.die("%s: the placement has no \"enemies\" key" % seed)
    return path


RUN_LOCK = __import__("threading").Lock()
RUN_SEQ = [0]


def run_batch(args, out_dir, label, stops, placement):
    """Runs the smoke over stops; returns {stop index in `stops`: result dict} plus the run summary."""
    with RUN_LOCK:
        RUN_SEQ[0] += 1
        n = RUN_SEQ[0]
    name = "%04d-%s-%s" % (n, label, stops[0].key.replace(":", "_"))
    run_dir = os.path.join(out_dir, "runs", name)
    cmd = [RUN_SH, "rando", "--stage", stops[0].spec(), "--env", "COS_RANDO_SMOKE=enemysweep",
           "--env", "COS_CACHE_PER_RUN=1", "--env", "COS_RANDO_SWEEP_FRAMES=%d" % args.frames,
           "--disc", args.disc, "--quiet", "--run-dir", run_dir,
           "--timeout", str(120 + 60 * len(stops)), "--stall", "60"]
    if placement:
        cmd += ["--env", "COS_RANDO_PLACEMENT=%s" % placement]
    if len(stops) > 1:
        cmd += ["--env", "COS_RANDO_SWEEP_HOPS=%s" % ",".join(s.spec() for s in stops[1:])]
    if args.exe:
        cmd += ["--exe", args.exe]
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
    except ValueError:
        rc = -1
    log = boot_sweep.read(os.path.join(run_dir, "run.log"))
    sig = boot_sweep.signature(run_dir, rc, log)
    m = re.search(r"\[cos\] FATAL[^\n]*|unmapped vtx attr \d+", log)
    if rc not in (0, 1) and m and m.group(0) not in sig:
        sig += " (%s)" % m.group(0).strip()
    per = {}
    for cm in CHECK_RE.finditer(log):
        i = int(cm.group(2))
        if i >= len(stops):
            continue
        r = per.setdefault(i, {"fails": [], "done": False, "block": None})
        if cm.group(4) == "FAIL":
            r["fails"].append(cm.group(1))
        if "heap allocation failure" in cm.group(1):
            r["done"] = True
    for hm in HEAP_RE.finditer(log):
        i = int(hm.group(1))
        if i in per:
            bm = BLOCK_RE.search(hm.group(2))
            per[i]["block"] = (int(bm.group(1), 16), int(bm.group(2), 16)) if bm else None
    # A stop not done: the one where the run ended (crash, timeout) gets the run's signature.
    first_undone = next((i for i in range(len(stops)) if not per.get(i, {}).get("done")), None)
    if first_undone is not None:
        r = per.setdefault(first_undone, {"fails": [], "done": False, "block": None})
        r["fails"].append("run ended: exit %d, %s" % (rc, sig))
    return per, first_undone, run_dir, rc


def sweep_stops(args, out_dir, label, stops, placement, pool):
    """{stop.key: result}; result = {"result", "fails", "block", "run_dir", "alone"}"""
    results = {}
    batches = []
    by_stage = collections.OrderedDict()
    for s in stops:
        by_stage.setdefault(s.stage, []).append(s)
    for stage_stops in by_stage.values():
        for i in range(0, len(stage_stops), args.batch):
            batches.append(stage_stops[i:i + args.batch])

    def one(batch):
        out = {}
        todo = batch
        while todo:
            per, undone, run_dir, rc = run_batch(args, out_dir, label, todo, placement)
            for i, s in enumerate(todo):
                if undone is not None and i > undone:
                    break
                r = per.get(i, {"fails": ["no result"], "done": False, "block": None})
                out[s.key] = {"result": "ok" if not r["fails"] else "FAIL", "fails": r["fails"],
                              "block": r["block"], "run_dir": os.path.relpath(run_dir, out_dir), "alone": None}
            if undone is None:
                break
            crashed = todo[undone]
            if len(todo) > 1:
                # the failing stop alone, booted directly: a hop problem or the room's own
                per1, _u, rd1, _rc = run_batch(args, out_dir, label + "-alone", [crashed], placement)
                r1 = per1.get(0, {"fails": ["no result"]})
                out[crashed.key]["alone"] = "ok" if not r1["fails"] else "; ".join(r1["fails"])
            todo = todo[undone + 1:]
        return out

    for fut in concurrent.futures.as_completed([pool.submit(one, b) for b in batches]):
        results.update(fut.result())
    return results


def classify_vanilla(args, out_dir, label, stops, results, pool):
    """Runs every failing stop vanilla, alone; sets "vanilla" and the xfail result."""
    failing = [s for s in stops if results.get(s.key, {}).get("result") == "FAIL"]

    def one(s):
        per, _u, _rd, _rc = run_batch(args, out_dir, label + "-vanilla", [s], None)
        r = per.get(0, {"fails": ["no result"]})
        return s, ("ok" if not r["fails"] else "; ".join(r["fails"]))

    for fut in concurrent.futures.as_completed([pool.submit(one, s) for s in failing]):
        s, v = fut.result()
        r = results[s.key]
        r["vanilla"] = v
        known = KNOWN_NON_RANDO.get((s.stage, s.room))
        text = " ".join(r["fails"]) + " " + (r.get("alone") or "")
        if known and v != "ok" and re.search(known[0], text):
            r["result"] = "xfail"
            r["reason"] = known[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--seeds", default=DEFAULT_SEEDS, help="comma-separated seed names (default %(default)s)")
    ap.add_argument("--vanilla", action="store_true", help="no placement: the game's own enemies")
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--frames", type=int, default=300, help="frames in each room (default 300)")
    ap.add_argument("--batch", type=int, default=12, help="stops per run at most (default 12)")
    ap.add_argument("--only", default="", help="stages or stage:room, comma-separated")
    ap.add_argument("--list", action="store_true", help="print the stops and exit")
    ap.add_argument("--cli", default=os.path.join(REPO, "build", "native-mac", "cos_rando_cli"))
    ap.add_argument("--exe", default="")
    ap.add_argument("--disc", default=os.environ.get("COS_DISC") or os.path.expanduser("~/Documents/windwaker/GZLE01.iso"))
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    if not 1 <= args.jobs <= 3 or args.frames < 90 or args.batch < 1:
        ap.error("--jobs 1-3, --frames >= 90, --batch >= 1")
    args.disc = os.path.abspath(args.disc)
    if not os.path.isfile(args.disc):
        print("enemy_sweep: no disc image %s" % args.disc, file=sys.stderr)
        return 14

    stops, ngroups = table_stops()
    choose_points(stops, args.disc)
    if args.only:
        want = args.only.split(",")
        stops = [s for s in stops if s.stage in want or "%s:%d" % (s.stage, s.room) in want]
    if args.list:
        for s in stops:
            print("%s\t%s\tgroups %s%s%s" % (s.key, s.spec(), ",".join(map(str, s.groups)),
                                            " must-defeat" if s.must_defeat else "",
                                            "" if s.own_spawn else " (no spawn of its own: point 0)"))
        print("%d groups, %d stops" % (ngroups, len(stops)))
        return 0

    out_dir = os.path.abspath(args.out or os.path.join(REPO, "build", "enemy-sweep", time.strftime("%Y%m%d-%H%M%S")))
    os.makedirs(os.path.join(out_dir, "runs"), exist_ok=True)
    seeds = [None] if args.vanilla else [s for s in args.seeds.split(",") if s]
    placements = {s: (gen_placement(args.cli, s, out_dir) if s else None) for s in seeds}
    print("enemy_sweep: %d groups, %d stops, %s, %d frames per stop, %d runs at a time; %s" % (
        ngroups, len(stops), "vanilla" if args.vanilla else "seeds " + ", ".join(seeds), args.frames, args.jobs,
        os.path.relpath(out_dir, REPO)), flush=True)

    all_results = collections.OrderedDict()
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for seed in seeds:
            label = seed or "vanilla"
            res = sweep_stops(args, out_dir, label, stops, placements[seed], pool)
            if seed:
                classify_vanilla(args, out_dir, label, stops, res, pool)
            all_results[label] = res
            c = collections.Counter(r["result"] for r in res.values())
            print("enemy_sweep: %s: %s (%ds)" % (label, ", ".join("%s %d" % kv for kv in sorted(c.items())),
                                                 time.time() - t0), flush=True)
            for s in stops:
                r = res.get(s.key)
                if r and r["result"] != "ok":
                    print("enemy_sweep:   %s %s: %s%s%s" % (r["result"], s.key, "; ".join(r["fails"]),
                                                          "; alone: " + r["alone"] if r.get("alone") else "",
                                                          "; vanilla: " + r["vanilla"] if r.get("vanilla") else ""),
                          flush=True)

    nfail = 0
    with open(os.path.join(out_dir, "enemy_sweep.tsv"), "w") as f:
        f.write("seed\tstop\tspec\tmust_defeat\tresult\tblock_size\tblock_free\tdetail\n")
        for label, res in all_results.items():
            for s in stops:
                r = res.get(s.key, {"result": "FAIL", "fails": ["not run"], "block": None})
                nfail += r["result"] == "FAIL"
                b = r.get("block") or ("", "")
                detail = "; ".join(r["fails"]) + (" | alone: %s" % r["alone"] if r.get("alone") else "") + \
                    (" | vanilla: %s" % r["vanilla"] if r.get("vanilla") else "") + \
                    (" | %s" % r["reason"] if r.get("reason") else "")
                f.write("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s\n" % (label, s.key, s.spec(), s.must_defeat, r["result"],
                                                             b[0] and hex(b[0]), b[1] and hex(b[1]), detail))
    with open(os.path.join(out_dir, "enemy_sweep.md"), "w") as f:
        f.write("# Enemy rando room sweep\n\n%d groups, %d stops, %d frames per stop.\n\n" % (
            ngroups, len(stops), args.frames))
        for label, res in all_results.items():
            c = collections.Counter(r["result"] for r in res.values())
            blocks = [(r["block"][1], k) for k, r in res.items() if r.get("block")]
            low = min(blocks) if blocks else None
            f.write("## %s\n\n%s.%s\n\n" % (label, ", ".join("%s %d" % kv for kv in sorted(c.items())),
                                            " Smallest room memory block free space: 0x%x (%s)." % low if low else ""))
            f.write("| stop | result | detail |\n|---|---|---|\n")
            for s in stops:
                r = res.get(s.key)
                if r and r["result"] != "ok":
                    f.write("| %s | %s | %s |\n" % (s.key, r["result"], ("; ".join(r["fails"]) + (
                        " / alone: " + r["alone"] if r.get("alone") else "") + (
                        " / vanilla: " + r["vanilla"] if r.get("vanilla") else "") + (
                        " / " + r["reason"] if r.get("reason") else "")).replace("|", "\\|")))
            f.write("\n")
    print("enemy_sweep: %d failing stop(s) over %d seed(s); report %s" % (
        nfail, len(seeds), os.path.relpath(os.path.join(out_dir, "enemy_sweep.md"), REPO)), flush=True)
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
