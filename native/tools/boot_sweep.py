#!/usr/bin/env python3
"""Boot every stage of the disc (docs/NATIVE_PORT_PLAN.md, step F4-boot-sweep).

  native/tools/run.sh boot-sweep [options]        (or this script directly)

Lists every stage on the disc (/res/Stage/<name>/Stage.arc, from the disc manifest that
native/tools/disc_manifest.py writes), picks a start for each from the stage data, and boots each
one with `run.sh run --stage <stage>:<room>:<point> --frames N --uncapped --audio on`, several
at a time. Each stage's run lands in <sweep dir>/<stage>/ (the usual run.sh run directory);
the report is <sweep dir>/boot_sweep.txt. Exit 0 only if every stage reached its last frame
(exit 0 of centollos); 1 otherwise; 2 usage; 14 disc problem.

The start, from the stage data only (nothing is hard-coded per stage):
  - the spawn points are the PLYR records the game would use (dStage_playerInit): those of
    Stage.arc's stage.dzs when it has any (the room is the record's parameters & 0x3F), otherwise
    those of each Room<N>.arc's room.dzr whose room bits name that room;
  - every SCLS exit record of every dzs/dzr on the disc that leads to the stage marks a spawn point
    as a real entry (room and start point equal);
  - the start is the entry with the lowest room, then the lowest point; with no entry, the spawn
    point with the lowest room and point. The layer is left to the game (-1).
  A stage with no PLYR record at all (no play stage, e.g. the name-entry stage) is reported as
  "skip" and does not fail the sweep.

Options:
  --jobs N        runs at a time (default 4)
  --frames N      game frames per stage (COS_FRAMES, default 600, counted from boot)
  --timeout S     run.sh --timeout per stage (default 150)
  --stall S       run.sh --stall per stage (default 30)
  --only LIST     comma-separated stage names (default: every stage)
  --list          print the chosen starts and exit (no runs)
  --disc PATH     the GZLE01 .iso (default COS_DISC or the maintainer's image)
  --exe PATH      the executable (default build/native-mac/centollos)
  --out DIR       the sweep directory (default build/native-mac/runs/boot-sweep-<timestamp>)

Report columns (tab-separated, one line per stage, after a header):
  stage, spec (COS_BOOT_STAGE), source of the start (exit = an SCLS entry, spawn = PLYR only),
  exit code and meaning, play_frame (frame the PLAY scene started the stage, - if never),
  last_frame (the last game frame the run reported), signature (ok, or the crash/panic/stall
  site: kind, fault address or assert file:line, scene, and the first two functions of the
  backtrace past the harness, with file:line).
Expected fails (EXPECTED_FAIL below): a stage the debug boot cannot enter the way the real game
does (it needs a story event flag or a cutscene first) may be listed with the reason and the
signature its debug boot stops at. Such a stage is reported "xfail" and does not fail the sweep
only while it fails with exactly that signature; any other failure fails the sweep as usual, and
a pass is reported "xpass" (the entry should then be removed). The list is never for a crash in
game code.
Run logs are kept; runs of identical consecutive lines (Aurora's per-draw warnings) are collapsed
to one line plus a count to keep the sweep directory small. Nothing here is meant for git.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
BUILD = os.path.join(REPO, "build", "native-mac")
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")
DISC_MANIFEST = os.path.join(SCRIPT_DIR, "disc_manifest.py")
MANIFEST = os.path.join(BUILD, "disc_manifest.json")
LEGACY_DISC = "/path/to/GZLE01.iso"

MEANING = {0: "reached", 1: "check failed", 2: "usage error", 10: "timeout", 11: "stall",
           12: "panic", 13: "signal", 14: "disc problem"}

# Stages the debug boot cannot enter the way the game does: stage -> (signature regex, reason).
# Documented in docs/NATIVE_PORT_PLAN.md ("boot-sweep expected fails").
_LKD01 = ("needs event flag 0x2D01 (set by M2tower's rescue.stb before the game ever reaches the "
          "stage): d_s_play.cpp phase_0 mounts Link's demo animations LkD01.arc only with it, "
          "the debug boot's new file mounts LkD00.arc, and the stage's Link cutscene asks for "
          "LkD01 file ids (id 355 is a .btk in LkD01, a .btp in LkD00)")
EXPECTED_FAIL = {
    "GTower": (r"JUTNameTab::getIndex .*<- J3DAnmTextureSRTKey::searchUpdateMaterialID", _LKD01),
    "M2ganon": (r"JUTNameTab::getIndex .*<- J3DAnmTextureSRTKey::searchUpdateMaterialID", _LKD01),
}

STAGE_ARC = re.compile(r"^/res/Stage/([^/]+)/Stage\.arc$")
ROOM_ARC = re.compile(r"^/res/Stage/([^/]+)/Room(\d+)\.arc$")


def manifest_version():
    with open(DISC_MANIFEST) as f:
        m = re.search(r"^MANIFEST_VERSION = (\d+)$", f.read(), re.M)
    return int(m.group(1))


def load_manifest(disc):
    """The disc manifest, written first if missing, of another disc or of an older version."""
    want = manifest_version()
    m = None
    if os.path.isfile(MANIFEST):
        with open(MANIFEST) as f:
            m = json.load(f)
        if m.get("manifest_version") != want or m.get("disc", {}).get("path") != disc:
            m = None
    if m is None:
        print("boot-sweep: writing %s" % MANIFEST, flush=True)
        r = subprocess.run([sys.executable, DISC_MANIFEST, "--quiet", "--disc", disc,
                            "--out", MANIFEST])
        if r.returncode != 0:
            sys.exit(14)
        with open(MANIFEST) as f:
            m = json.load(f)
    return m


def stage_file(arc):
    for x in arc.get("files", []):
        if x.get("format") in ("dzs", "dzr") or x["path"].endswith((".dzs", ".dzr")):
            return x
    return None


def scls_records(dz):
    out = []
    for rec in dz.get("records", {}).values():
        if rec.get("tag") == "SCLS":
            out.extend(rec["entries"])
    return out


def choose_starts(manifest):
    """[(stage, room, point, source)] per stage, in name order; room None: no spawn point."""
    by_path = {r["path"]: r for r in manifest["files"]}
    stages = sorted(m.group(1) for p in by_path for m in [STAGE_ARC.match(p)] if m)
    rooms = {}
    for p in by_path:
        m = ROOM_ARC.match(p)
        if m:
            rooms.setdefault(m.group(1), []).append(int(m.group(2)))

    # Every SCLS exit of the disc: (stage, room, start point).
    exits = set()
    for r in manifest["files"]:
        for x in r.get("files", []):
            if x.get("chunks") is not None:
                for e in scls_records(x):
                    exits.add((e["stage"], e["room"], e["start"]))

    out = []
    for stage in stages:
        spawns = set()
        dzs = stage_file(by_path["/res/Stage/%s/Stage.arc" % stage])
        for a in (dzs or {}).get("actors", {}).get("PLYR", []):
            spawns.add((a["params"] & 0x3F, a["angle"][2] & 0xFF))
        if not spawns:
            for n in sorted(rooms.get(stage, [])):
                dzr = stage_file(by_path["/res/Stage/%s/Room%d.arc" % (stage, n)])
                for a in (dzr or {}).get("actors", {}).get("PLYR", []):
                    if a["params"] & 0x3F == n:
                        spawns.add((n, a["angle"][2] & 0xFF))
        if not spawns:
            out.append((stage, None, None, "skip"))
            continue
        entries = sorted(s for s in spawns if (stage, s[0], s[1]) in exits)
        if entries:
            room, point = entries[0]
            out.append((stage, room, point, "exit"))
        else:
            room, point = sorted(spawns)[0]
            out.append((stage, room, point, "spawn"))
    return out


def collapse_log(path):
    """Collapse runs of identical consecutive lines into the line and a repeat count."""
    if not os.path.isfile(path):
        return
    tmp = path + ".tmp"
    with open(path, "rb") as src, open(tmp, "wb") as dst:
        prev, count = None, 0

        def flush():
            if prev is not None:
                dst.write(prev)
                if count > 1:
                    dst.write(b"[boot-sweep] (previous line repeated %d more times)\n" % (count - 1))

        for line in src:
            if line == prev:
                count += 1
                continue
            flush()
            prev, count = line, 1
        flush()
    os.replace(tmp, path)


SKIP_FRAME = re.compile(r"^(pc_|OSPanic|OSReport|cos_switch_|JUTAssertion|JUTException|"
                        r"abort|__|std::)")


def game_frames(text, count=2):
    """The first symbolised centollos frames of the first atos block that are not the harness itself,
    joined with " <- " (callee first)."""
    m = re.search(r"^\[run\] atos .*\n((?:.*\n)*?)--$", text, re.M)
    if not m:
        return None
    frames = []
    for line in m.group(1).splitlines():
        fm = re.match(r"^(.*?) \(in centollos\)(?: \((.*?)\))?", line)
        if not fm:
            continue
        func = fm.group(1)
        name = func.split("(")[0] if not func.startswith("operator") else func
        if SKIP_FRAME.match(name):
            continue
        frames.append(name + (" (%s)" % fm.group(2) if fm.group(2) else ""))
        if len(frames) == count:
            break
    return " <- ".join(frames) or None


def read(path):
    try:
        with open(path, errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def signature(run_dir, rc, log):
    if rc == 0:
        return "ok"
    bt = read(os.path.join(run_dir, "backtrace.txt"))
    st = read(os.path.join(run_dir, "stall.txt"))
    text = bt or st
    parts = []
    m = re.search(r"^\[cos\] CRASH (\S+) \(\d+\) code=\S+ addr=(\S+)", text, re.M)
    if m:
        parts.append("CRASH %s addr=%s" % (m.group(1), m.group(2)))
    m = re.search(r'^\[cos\] PANIC in "([^"]*)" on line (\d+)', text, re.M)
    if m:
        parts.append("PANIC %s:%s" % (os.path.basename(m.group(1)), m.group(2)))
    if not parts and st:
        m = re.search(r"^\[cos\] (STALL|TIMEOUT)[^\n]*", st, re.M)
        parts.append(m.group(0)[6:] if m else MEANING.get(rc, "exit %d" % rc))
    if not parts:
        # A check that failed, or an exit without a report: the last harness line.
        lines = [l for l in log.splitlines() if l.startswith("[cos] ") or l.startswith("run:")]
        parts.append(lines[-1] if lines else MEANING.get(rc, "exit %d" % rc))
    m = re.search(r"^\[cos\] state: scene=(\S+)", text, re.M)
    if m:
        parts.append("scene=%s" % m.group(1))
    frames = game_frames(text)
    if frames:
        parts.append("in " + frames)
    return " ".join(parts)


def last_frame(run_dir, log):
    for name in ("backtrace.txt", "stall.txt"):
        m = re.search(r"^\[cos\] state: .*? frame=(\d+)", read(os.path.join(run_dir, name)), re.M)
        if m:
            return m.group(1)
    m = re.findall(r"^\[cos\] FRAMES (\d+) done", log, re.M)
    if m:
        return m[-1]
    m = re.findall(r"frame[= ](\d+)", "\n".join(l for l in log.splitlines()
                                                  if l.startswith("[cos]")))
    return m[-1] if m else "-"


def run_stage(args, sweep_dir, stage, room, point):
    run_dir = os.path.join(sweep_dir, stage)
    spec = "%s:%d:%d" % (stage, room, point)
    cmd = [COS_RUN, "run", "--stage", spec, "--frames", str(args.frames), "--uncapped",
           "--audio", "on", "--timeout", str(args.timeout), "--stall", str(args.stall),
           "--disc", args.disc, "--quiet", "--run-dir", run_dir]
    if args.exe:
        cmd += ["--exe", args.exe]
    start = time.time()
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        rc = int(read(os.path.join(run_dir, "exit_code.txt")).strip())
    except ValueError:
        rc = -1
    collapse_log(os.path.join(run_dir, "run.log"))
    log = read(os.path.join(run_dir, "run.log"))
    m = re.search(r"PLAY scene starts stage .* at frame (\d+)", log)
    return {"rc": rc, "play_frame": m.group(1) if m else "-", "last_frame": last_frame(run_dir, log),
            "signature": signature(run_dir, rc, log), "seconds": int(time.time() - start)}


def expectation(stage, r):
    """"xfail" when a listed stage fails with its listed signature, "xpass" when it passes,
    "" otherwise (unlisted, or a listed stage failing some other way: a real failure)."""
    if stage not in EXPECTED_FAIL:
        return ""
    if r["rc"] == 0:
        return "xpass"
    return "xfail" if re.search(EXPECTED_FAIL[stage][0], r["signature"]) else ""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--frames", type=int, default=600)
    ap.add_argument("--timeout", type=int, default=150)
    ap.add_argument("--stall", type=int, default=30)
    ap.add_argument("--only", default="")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--disc", default=os.environ.get("COS_DISC") or
                    (LEGACY_DISC if os.path.isfile(LEGACY_DISC) else ""))
    ap.add_argument("--exe", default="")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    if args.jobs < 1 or args.frames < 1:
        ap.error("--jobs and --frames must be positive")
    if not args.disc:
        print("boot-sweep: no disc image: pass --disc PATH or set COS_DISC", file=sys.stderr)
        return 14
    args.disc = os.path.abspath(args.disc)
    if subprocess.run([sys.executable, DISC_MANIFEST, "--verify", "--quiet", "--disc",
                       args.disc]).returncode != 0:
        return 14

    starts = choose_starts(load_manifest(args.disc))
    if args.only:
        want = set(args.only.split(","))
        unknown = want - {s[0] for s in starts}
        if unknown:
            print("boot-sweep: no such stage: %s" % ", ".join(sorted(unknown)), file=sys.stderr)
            return 2
        starts = [s for s in starts if s[0] in want]
    if args.list:
        for stage, room, point, source in starts:
            print("%s\t%s\t%s" % (stage, "-" if room is None else "%s:%d:%d" % (stage, room, point),
                                  source))
        return 0

    sweep_dir = args.out or os.path.join(BUILD, "runs",
                                         "boot-sweep-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(sweep_dir, exist_ok=False)
    sweep_dir = os.path.abspath(sweep_dir)
    print("boot-sweep: %d stages, %d at a time, %d frames each; %s" %
          (len(starts), args.jobs, args.frames, os.path.relpath(sweep_dir, REPO)), flush=True)

    results = {}
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {}
        for stage, room, point, source in starts:
            if room is None:
                results[stage] = {"rc": None, "play_frame": "-", "last_frame": "-",
                                  "signature": "skip: no PLYR spawn record", "seconds": 0}
                continue
            futures[pool.submit(run_stage, args, sweep_dir, stage, room, point)] = stage
        for fut in concurrent.futures.as_completed(futures):
            stage = futures[fut]
            r = fut.result()
            r["expect"] = expectation(stage, r)
            results[stage] = r
            print("boot-sweep: %-8s exit %-3s %3ds  %s%s" % (
                stage, r["rc"], r["seconds"], r["signature"],
                "  [%s]" % r["expect"] if r["expect"] else ""), flush=True)

    report = os.path.join(sweep_dir, "boot_sweep.txt")
    failed = xfail = xpass = 0
    with open(report, "w") as f:
        f.write("stage\tspec\tsource\texit\tmeaning\tplay_frame\tlast_frame\tsignature\n")
        for stage, room, point, source in starts:
            r = results[stage]
            spec = "-" if room is None else "%s:%d:%d" % (stage, room, point)
            if r["rc"] is None:
                meaning = "skip"
            else:
                meaning = MEANING.get(r["rc"], "unexpected exit")
                if r.get("expect") == "xfail":
                    meaning += " (xfail)"
                    xfail += 1
                elif r.get("expect") == "xpass":
                    meaning += " (xpass)"
                    xpass += 1
                else:
                    failed += r["rc"] != 0
            f.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (
                stage, spec, source, "-" if r["rc"] is None else r["rc"], meaning,
                r["play_frame"], r["last_frame"], r["signature"]))
        ran = sum(1 for s in starts if s[1] is not None)
        for stage in sorted(results):
            if results[stage].get("expect") == "xfail":
                f.write("# xfail %s: %s\n" % (stage, EXPECTED_FAIL[stage][1]))
            elif results[stage].get("expect") == "xpass":
                f.write("# xpass %s: listed as an expected fail but passed; remove it\n" % stage)
        f.write("# %d stages, %d run, %d passed, %d failed, %d expected fails, %d skipped; "
                "%d frames each, %ds\n" % (len(starts), ran, ran - failed - xfail, failed, xfail,
                                           len(starts) - ran, args.frames, int(time.time() - t0)))
    print("boot-sweep: %d of %d stages passed, %d failed, %d expected fails, %d skipped in %ds; "
          "report %s" % (ran - failed - xfail, ran, failed, xfail, len(starts) - ran,
                         int(time.time() - t0), os.path.relpath(report, REPO)))
    if xpass:
        print("boot-sweep: %d expected fail(s) passed; remove them from EXPECTED_FAIL" % xpass)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
