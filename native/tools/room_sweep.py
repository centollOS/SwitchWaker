#!/usr/bin/env python3
"""Boot every room of every stage of the disc (docs/NATIVE_PORT_PLAN.md, "Room sweep").

  native/tools/run.sh room-sweep [options]        (or this script directly)

The stage sweep (boot_sweep.py) boots each stage once, at one start; a fault in a room it does not
start in goes unseen (bug B10: the first dungeon's room 2). This sweep boots every room that has a
spawn point (or, with --points all, every spawn point of every room) with
`run.sh run --stage <stage>:<room>:<point> --frames N --uncapped --audio on [--input walk]`,
several at a time, and classifies each run as the stage sweep does (exit code, crash or panic
site, stall).

The spawn points, from the stage data only (nothing is hard-coded per stage): the PLYR records the
game would use (dStage_playerInit): those of Stage.arc's stage.dzs when it has any (the room is the
record's parameters & 0x3F, the point its angle z & 0xFF), otherwise those of each Room<N>.arc's
room.dzr whose room bits name that room. With --points room (the default) each room gets one
point: an SCLS exit of the disc leads to it (a real entry) when there is one, the lowest such;
otherwise the lowest point. A room with no spawn point (the debug boot cannot start there) is
listed in the report as "no-spawn" and not run; a spawn point naming a room the disc has no
Room<N>.arc for (Cave08's stage.dzs lists rooms 0 and 4-16, the disc has rooms 1-3) as "no-arc".

By default the player walks a slow circle (native/check/input/room-walk.txt) so the actors near
the spawn point see him move; --input none leaves the pad neutral.

Options:
  --jobs N        runs at a time (default 6)
  --frames N      game frames per run (COS_FRAMES, default 900, counted from boot, uncapped)
  --points MODE   room (one point per room, default) or all (every spawn point)
  --timeout S     run.sh --timeout per run (default 180)
  --stall S       run.sh --stall per run (default 30)
  --only LIST     comma-separated stages, or stage:room (default: every stage)
  --input PATH    controller script (default native/check/input/room-walk.txt; none: no script)
  --rerun N       run each failing spec up to N more times (default 1); a spec that passes on a
                  rerun is reported "flaky" with its first signature
  --list          print the chosen specs and exit (no runs)
  --disc PATH     the GZLE01 .iso (default COS_DISC)
  --exe PATH      the executable (default build/native-mac/centollos)
  --out DIR       the sweep directory (default build/room-sweep/<timestamp>)

Each run lands in <sweep dir>/runs/<stage>_<room>_<point>/ (the usual run.sh run directory, its log
collapsed as boot_sweep.py does). The report is <sweep dir>/room_sweep.md: the counts, one table
row per distinct failure signature (with the specs that hit it) and one row per run (stage, room,
point, result, signature); <sweep dir>/room_sweep.tsv has the same runs tab-separated.
Results: ok (reached the last frame with the player in the room), FAIL, xfail (an expected failure
below, with its signature), flaky (failed, then passed on a rerun), ok-layer (see below), no-spawn,
no-arc.
A run that reaches its last frame but whose player never got into the room (no "outset-debug: The
player in" line: the PLAY scene never finished creating) is a failure with the signature "NO-PLAY"
plus the processes still creating (pc_outset.cpp lists them); when the spawn point has no floor
under it in its room's collision (room.dzb, read from the disc: the player waits in makeBgWait
forever, as on the console) it is "no-floor", an expected failure.
A cutscene spawn point (the points 200 and up, and others) plays a demo from the room's demo bank,
which the game picks by layer; the debug boot's layer -1 resolves to the stage's default layer,
whose bank may not hold that demo (PANIC d_event_data.cpp:1070, as for ENDumi). Such a spec is run
again with layers 0-11 (COS_BOOT_STAGE's fourth field) and reported "ok-layer" with the first
layer that passes, or "no-layer" (an expected failure: the demo is mounted by the scene the game
comes from, or picked by story flags, which a debug boot skips) when none does.
Expected fails: the stage sweep's EXPECTED_FAIL entries hold for every room of their stage (as
intermittent: a pass is fine), plus ROOM_EXPECTED_FAIL below for single rooms; both only for a
debug boot the game cannot make or for disc data the original game cannot run either, never for a
crash in game code. Exit 0 only if no run is FAIL; 1 otherwise; 2 usage; 14 disc problem.
Nothing here is meant for git.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures
import importlib.util
import os
import re
import struct
import subprocess
import threading
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")
DEFAULT_INPUT = os.path.join(REPO, "native", "check", "input", "room-walk.txt")

_spec = importlib.util.spec_from_file_location("boot_sweep",
                                               os.path.join(SCRIPT_DIR, "boot_sweep.py"))
boot_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boot_sweep)

DISC = None
DISC_LOCK = threading.Lock()

# (stage, room) or (stage, room, point) -> (signature regex, reason): single rooms the debug boot cannot enter the way the
# game does, or whose disc data the original game cannot run either. Documented in
# docs/NATIVE_PORT_PLAN.md ("Room sweep").
_NO_CAMERA = ("leftover stage (the E3 show's demo room): its stage.dzs has no CAMR/RCAM record, "
              "so no camera process is created and the player waits in its phase_2 for one; "
              "the PLAY scene never finishes creating")
_TEXT_BMD = ("leftover test stage with incomplete disc data: Room0.arc's model.bmd is a text "
             "file (\"<HeaderInfo>\"), J3DModelLoaderDataBase::load refuses it, dRes_info_c::"
             "loadResource stops at it and leaves room.dzr unloaded: no PLYR record is read, no "
             "player is created and the room scene waits in phase_4 for one")
_SHIP_WARP = ("point 1 of sea room 26 is the arrival of a boss warp riding the ship "
              "(dProcBossWarp_init with demo parameter 2: initShipRideUseItem); a debug boot's new "
              "file has no ship there, dComIfGp_getShipActor() is NULL")
_TBOX_TYPE = ("leftover test stage with incomplete disc data: room 4's takara3 (params 0xFF20027E) "
              "has function type 0x7E, which daTbox_c::CreateInit does not know (JUT_ASSERT, "
              "d_a_tbox.cpp:808; a retail build ignores it)")
ROOM_EXPECTED_FAIL: dict = {
    ("A_R00", 0): (r"^NO-PLAY", _TEXT_BMD),
    ("E3ROOP", 0): (r"^NO-PLAY", _NO_CAMERA),
    ("sea", 26, 1): (r"SIGSEGV .* in daPy_lk_c::dProcBossWarp_init ", _SHIP_WARP),
    ("I_SubAN", 4): (r"^PANIC d_a_tbox\.cpp:808 .* in daTbox_c::CreateInit ", _TBOX_TYPE),
}
# A cutscene spawn point booted on the wrong layer (see the docstring): rerun with layers 0-11.
LAYER_RETRY = r"^PANIC d_event_data\.cpp:1070 "
LAYERS = range(0, 12)


class Disc:
    """Files of the disc image and the RARC archives in them (for the floor check)."""

    def __init__(self, path):
        self.f = open(path, "rb")
        entries, paths = boot_sweep_dm().read_fst(self.f)
        self.files = {p: e for p, e in zip(paths, entries) if not e["dir"]}

    def read(self, path):
        e = self.files[path]
        self.f.seek(e["offset"])
        return self.f.read(e["size"])

    def arc_files(self, path):
        """{name: bytes} of an archive's files (Yaz0 undone)."""
        dm = boot_sweep_dm()
        b = self.read(path)
        if b[:4] == b"Yaz0":
            b = dm.yaz0_decompress(b)
        info = 0x20
        data_base = 0x20 + dm.u32(b, 0xC)
        num_entries = dm.u32(b, info + 0x08)
        entry_off = info + dm.u32(b, info + 0x0C)
        str_off = info + dm.u32(b, info + 0x14)
        out = {}
        for i in range(num_entries):
            o = entry_off + i * 0x14
            tf = dm.u32(b, o + 4)
            if (tf >> 24) & 2:
                continue  # a directory
            data = b[data_base + dm.u32(b, o + 8):data_base + dm.u32(b, o + 8) + dm.u32(b, o + 0xC)]
            if data[:4] == b"Yaz0":
                data = dm.yaz0_decompress(data)
            out[dm.cstr(b, str_off + (tf & 0xFFFFFF))] = data
        return out


def boot_sweep_dm():
    if not hasattr(boot_sweep_dm, "mod"):
        spec = importlib.util.spec_from_file_location("disc_manifest", boot_sweep.DISC_MANIFEST)
        boot_sweep_dm.mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(boot_sweep_dm.mod)
    return boot_sweep_dm.mod


def floor_below(dzb, x, y, z):
    """The highest triangle of a room.dzb under (x, z) at most 50 units above y, or None."""
    nv, vo, nt, to = struct.unpack_from(">iIiI", dzb, 0)
    v = struct.unpack_from(">%df" % (nv * 3), dzb, vo)
    best = None
    for i in range(nt):
        a, b, c = struct.unpack_from(">HHH", dzb, to + i * 10)
        ax, ay, az = v[a * 3:a * 3 + 3]
        bx, by, bz = v[b * 3:b * 3 + 3]
        cx, cy, cz = v[c * 3:c * 3 + 3]
        d = (bz - cz) * (ax - cx) + (cx - bx) * (az - cz)
        if abs(d) < 1e-9:
            continue
        l1 = ((bz - cz) * (x - cx) + (cx - bx) * (z - cz)) / d
        l2 = ((cz - az) * (x - cx) + (ax - cx) * (z - cz)) / d
        l3 = 1.0 - l1 - l2
        if min(l1, l2, l3) < -1e-6:
            continue
        h = l1 * ay + l2 * by + l3 * cy
        if h <= y + 50 and (best is None or h > best):
            best = h
    return best


def spawn_points(manifest):
    """{stage: {room: [points]}} from the PLYR records, every Room<N>.arc's room numbers per stage,
    and the set of (stage, room, point) that an SCLS exit leads to."""
    by_path = {r["path"]: r for r in manifest["files"]}
    stages = sorted(m.group(1) for p in by_path for m in [boot_sweep.STAGE_ARC.match(p)] if m)
    rooms = collections.defaultdict(list)
    for p in by_path:
        m = boot_sweep.ROOM_ARC.match(p)
        if m:
            rooms[m.group(1)].append(int(m.group(2)))
    exits = set()
    for r in manifest["files"]:
        for x in r.get("files", []):
            if x.get("chunks") is not None:
                for e in boot_sweep.scls_records(x):
                    exits.add((e["stage"], e["room"], e["start"]))
    points = {}
    for stage in stages:
        pts = collections.defaultdict(set)
        dzs = boot_sweep.stage_file(by_path["/res/Stage/%s/Stage.arc" % stage])
        for a in (dzs or {}).get("actors", {}).get("PLYR", []):
            pts[a["params"] & 0x3F].add(a["angle"][2] & 0xFF)
            SPAWN_POS.setdefault((stage, a["params"] & 0x3F, a["angle"][2] & 0xFF), a.get("pos"))
        if not pts:
            for n in sorted(rooms.get(stage, [])):
                dzr = boot_sweep.stage_file(by_path["/res/Stage/%s/Room%d.arc" % (stage, n)])
                for a in (dzr or {}).get("actors", {}).get("PLYR", []):
                    if a["params"] & 0x3F == n:
                        pts[n].add(a["angle"][2] & 0xFF)
                        SPAWN_POS.setdefault((stage, n, a["angle"][2] & 0xFF), a.get("pos"))
        points[stage] = {r: sorted(p) for r, p in pts.items()}
    return stages, rooms, points, exits


SPAWN_POS = {}  # (stage, room, point) -> [x, y, z] of its PLYR record


def choose_specs(manifest, mode):
    """[(stage, room, point)] in order; point None: a room with no spawn point, "no-arc": spawn
    points in a room the disc has no archive for."""
    stages, rooms, points, exits = spawn_points(manifest)
    out = []
    for stage in stages:
        all_rooms = sorted(set(rooms.get(stage, [])) | set(points[stage]))
        for room in all_rooms:
            pts = points[stage].get(room, [])
            if room not in rooms.get(stage, []):
                out.append((stage, room, "no-arc"))
            elif not pts:
                out.append((stage, room, None))
            elif mode == "all":
                out.extend((stage, room, p) for p in pts)
            else:
                entries = [p for p in pts if (stage, room, p) in exits]
                out.append((stage, room, (entries or pts)[0]))
    return out


def run_spec(args, sweep_dir, stage, room, point, attempt, layer=None):
    name = "%s_%d_%d" % (stage, room, point) + ("" if attempt == 0 else ".rerun%d" % attempt)
    spec = "%s:%d:%d" % (stage, room, point)
    if layer is not None:
        name += ".layer%d" % layer
        spec += ":%d" % layer
    run_dir = os.path.join(sweep_dir, "runs", name)
    cmd = [COS_RUN, "run", "--stage", spec, "--frames",
           str(args.frames), "--uncapped", "--audio", "on", "--timeout", str(args.timeout),
           "--stall", str(args.stall), "--disc", args.disc, "--quiet", "--run-dir", run_dir]
    if args.input != "none":
        cmd += ["--input", args.input]
    if args.exe:
        cmd += ["--exe", args.exe]
    start = time.time()
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
    except ValueError:
        rc = -1
    boot_sweep.collapse_log(os.path.join(run_dir, "run.log"))
    log = boot_sweep.read(os.path.join(run_dir, "run.log"))
    r = {"rc": rc, "last_frame": boot_sweep.last_frame(run_dir, log),
         "signature": boot_sweep.signature(run_dir, rc, log),
         "seconds": int(time.time() - start), "run_dir": run_dir}
    if rc == 0 and "outset-debug: The player in " not in log:
        creating = re.findall(r"outset-debug:   creating: process (\d+) (\S+)", log)
        r["rc"] = "no-play"
        r["signature"] = "NO-PLAY (PLAY scene never executing%s)" % (
            "; creating: " + ", ".join("%s %s" % c for c in creating) if creating else "")
    return r


def expected(stage, room, point, sig):
    """The reason when a failure with this signature is expected for this spec, else None."""
    entry = (ROOM_EXPECTED_FAIL.get((stage, room, point)) or ROOM_EXPECTED_FAIL.get((stage, room))
             or boot_sweep.EXPECTED_FAIL.get(stage))
    if entry and re.search(entry[0], sig):
        return entry[1]
    return None


def no_floor(disc, stage, room, point):
    """True when the spawn point has no floor under it in its room's collision."""
    pos = SPAWN_POS.get((stage, room, point))
    if pos is None or disc is None:
        return False
    try:
        files = disc.arc_files("/res/Stage/%s/Room%d.arc" % (stage, room))
    except KeyError:
        return False
    dzbs = [d for n, d in files.items() if n.endswith(".dzb")]
    return bool(dzbs) and all(floor_below(d, *pos) is None for d in dzbs)


def sweep_one(args, sweep_dir, stage, room, point):
    r = run_spec(args, sweep_dir, stage, room, point, 0)
    r["result"] = "ok" if r["rc"] == 0 else "FAIL"
    if r["rc"] != 0 and expected(stage, room, point, r["signature"]):
        r["result"] = "xfail"
    if r["rc"] == "no-play" and r["result"] == "FAIL":
        with DISC_LOCK:
            if no_floor(DISC, stage, room, point):
                r["result"] = "no-floor"
    if r["result"] == "FAIL" and re.search(LAYER_RETRY, r["signature"]):
        for layer in LAYERS:
            again = run_spec(args, sweep_dir, stage, room, point, 0, layer)
            if again["rc"] == 0:
                r["result"] = "ok-layer"
                r["signature"] = "ok with layer %d (layer -1: %s)" % (layer, r["signature"])
                break
        else:
            # No layer's bank holds the demo: the scene the game reaches this point from mounts
            # it (or story flags pick it), which a debug boot skips. Not a crash in game code.
            r["result"] = "no-layer"
        return r
    attempt = 0
    while r["result"] == "FAIL" and attempt < args.rerun:
        attempt += 1
        again = run_spec(args, sweep_dir, stage, room, point, attempt)
        if again["rc"] == 0:
            r["result"] = "flaky"
            break
    return r


def md_escape(s):
    return s.replace("|", "\\|")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--frames", type=int, default=900)
    ap.add_argument("--points", choices=("room", "all"), default="room")
    ap.add_argument("--timeout", type=int, default=180)
    ap.add_argument("--stall", type=int, default=30)
    ap.add_argument("--only", default="")
    ap.add_argument("--input", default=DEFAULT_INPUT)
    ap.add_argument("--rerun", type=int, default=1)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--disc", default=os.environ.get("COS_DISC", ""))
    ap.add_argument("--exe", default="")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    if args.jobs < 1 or args.frames < 1 or args.rerun < 0:
        ap.error("--jobs and --frames must be positive, --rerun not negative")
    if not args.disc:
        print("room-sweep: no disc image: pass --disc PATH or set COS_DISC", file=sys.stderr)
        return 14
    args.disc = os.path.abspath(args.disc)
    if args.input != "none":
        args.input = os.path.abspath(args.input)
        if not os.path.isfile(args.input):
            print("room-sweep: no such input script: %s" % args.input, file=sys.stderr)
            return 2
    if subprocess.run([sys.executable, boot_sweep.DISC_MANIFEST, "--verify", "--quiet", "--disc",
                       args.disc]).returncode != 0:
        return 14

    global DISC
    specs = choose_specs(boot_sweep.load_manifest(args.disc), args.points)
    DISC = Disc(args.disc)
    if args.only:
        want = args.only.split(",")
        known = {s[0] for s in specs}
        bad = [w for w in want if w.split(":")[0] not in known]
        if bad:
            print("room-sweep: no such stage: %s" % ", ".join(bad), file=sys.stderr)
            return 2
        specs = [s for s in specs if s[0] in want or "%s:%d" % (s[0], s[1]) in want]
    if args.list:
        for stage, room, point in specs:
            print("%s\t%d\t%s" % (stage, room, "-" if point is None else point))
        return 0

    sweep_dir = os.path.abspath(args.out or os.path.join(
        REPO, "build", "room-sweep", time.strftime("%Y%m%d-%H%M%S")))
    os.makedirs(os.path.join(sweep_dir, "runs"), exist_ok=False)
    runnable = [s for s in specs if isinstance(s[2], int)]
    print("room-sweep: %d specs (%d rooms without a spawn point or archive), %d at a time, %d "
          "frames each; %s" % (len(runnable), len(specs) - len(runnable), args.jobs, args.frames,
             os.path.relpath(sweep_dir, REPO)), flush=True)

    results = {}
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(sweep_one, args, sweep_dir, *s): s for s in runnable}
        done = 0
        for fut in concurrent.futures.as_completed(futures):
            s = futures[fut]
            r = fut.result()
            results[s] = r
            done += 1
            if r["result"] != "ok":
                print("room-sweep: [%d/%d] %s:%d:%d %s exit %s  %s" % (
                    done, len(runnable), s[0], s[1], s[2], r["result"], r["rc"], r["signature"]),
                    flush=True)
            elif done % 25 == 0:
                print("room-sweep: [%d/%d] %ds" % (done, len(runnable), time.time() - t0),
                      flush=True)

    counts = collections.Counter(r["result"] for r in results.values())
    counts["no-spawn"] = sum(1 for s in specs if s[2] is None)
    counts["no-arc"] = sum(1 for s in specs if s[2] == "no-arc")
    sigs = collections.OrderedDict()
    for s in runnable:
        r = results[s]
        if r["result"] != "ok":
            sigs.setdefault(r["signature"], []).append((s, r["result"]))

    with open(os.path.join(sweep_dir, "room_sweep.tsv"), "w") as f:
        f.write("stage\troom\tpoint\tresult\texit\tlast_frame\tsignature\n")
        for stage, room, point in specs:
            if not isinstance(point, int):
                f.write("%s\t%d\t-\t%s\t-\t-\t-\n" % (stage, room, point or "no-spawn"))
                continue
            r = results[(stage, room, point)]
            f.write("%s\t%d\t%d\t%s\t%s\t%s\t%s\n" % (stage, room, point, r["result"], r["rc"],
                                                       r["last_frame"], r["signature"]))
    with open(os.path.join(sweep_dir, "room_sweep.md"), "w") as f:
        f.write("# Room sweep %s\n\n" % os.path.basename(sweep_dir))
        f.write("%d stages, %d specs run (--points %s), %d frames uncapped, input %s, exe %s; "
                "%d min.\n\n" % (len({s[0] for s in specs}), len(runnable), args.points,
                                 args.frames, os.path.basename(args.input) if args.input != "none"
                                 else "none", args.exe or "build/native-mac/centollos",
                                 (time.time() - t0) / 60))
        f.write("Results: %s\n\n" % ", ".join("%s %d" % kv for kv in sorted(counts.items())))
        f.write("## Signatures\n\n| # | signature | result | specs |\n|---|---|---|---|\n")
        for i, (sig, hits) in enumerate(sigs.items(), 1):
            f.write("| %d | %s | %s | %s |\n" % (
                i, md_escape(sig), ", ".join(sorted({h[1] for h in hits})),
                ", ".join("%s:%d:%d" % h[0] for h in hits)))
        f.write("\n## Runs\n\n| stage | room | point | result | signature |\n|---|---|---|---|---|\n")
        for stage, room, point in specs:
            if not isinstance(point, int):
                f.write("| %s | %d | - | %s | - |\n" % (stage, room, point or "no-spawn"))
                continue
            r = results[(stage, room, point)]
            f.write("| %s | %d | %d | %s | %s |\n" % (stage, room, point, r["result"],
                                                      md_escape(r["signature"])))
    print("room-sweep: %s; %d distinct failure signatures; report %s" % (
        ", ".join("%s %d" % kv for kv in sorted(counts.items())), len(sigs),
        os.path.relpath(os.path.join(sweep_dir, "room_sweep.md"), REPO)), flush=True)
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
