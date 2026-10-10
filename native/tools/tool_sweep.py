#!/usr/bin/env python3
"""Use each of the player's items and check what it did, not only that nothing crashed.

  native/tools/tool_sweep.py [--jobs N] [--only LIST] [--disc PATH] [--out DIR] [--list]

The bow's arrows left the bow aimed at the ground for months (centollOS/SwitchWaker#1, a float
angle out of the s16 range in daArrow_c::setKeepMatrix): every item run passed, since they
checked for crashes only. Each case here is one `run.sh run` on the Outset pier (sea:44:0) with
COS_BOOT_ITEMS (the first item on X), or in a test room (K_Teste) placed in front of a target
(COS_TOOL_PLACE), a controller script (native/check/input/) and
COS_TOOL_WATCH=1 (native/src/pc/harness/smokes/pc_tool_watch.cpp), whose tool_watch.txt logs the
player (procedure, item in hand, speed, magic, arrows, bombs) and every actor created after a
baseline (an arrow, a boomerang, a bomb, bait...) with how far it went. The case's checks read
that log: an arrow must fly at least 2000 units, a boomerang must go out and come back, iron
boots must slow the player down, and so on (CASES below).

Report: <out>/tool_sweep.md (default build/tool-sweep/<timestamp>), one line per case, PASS or
FAIL with the failed checks; the run directories next to it (screenshots at a few frames). Exit 0
when every case passes. Each run takes a few seconds; the whole sweep under a minute at 4 jobs.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import re
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")
INPUT = "native/check/input"

# daPy_lk_c procedures (daPyProc_*_e, d_a_player_main.h) the checks look for.
PROC_FAN_SWING = 146
PROC_FAN_GLIDE = 147
PROC_HAMMER = (82, 83, 84)
PROC_BOTTLE_DRINK = 163
PROC_BOTTLE_SWING = 165
PROC_WALK = 6
PROC_HOOKSHOT_FLY = 133


class Log:
    """tool_watch.txt parsed: player samples, actor births and deaths."""

    def __init__(self, path: str):
        self.player: list[dict] = []
        self.born: list[dict] = []
        self.gone: list[dict] = []
        with open(path) as f:
            for line in f:
                kind, _, rest = line.partition(" ")
                if kind == "P":
                    d = dict(kv.split("=", 1) for kv in rest.split()[1:])
                    d["frame"] = int(rest.split()[0])
                    for k in ("proc", "equip", "life", "magic", "arrows", "bombs"):
                        d[k] = int(d[k])
                    d["speedF"] = float(d["speedF"])
                    self.player.append(d)
                elif kind == "+":
                    f_ = rest.split()
                    self.born.append({"frame": int(f_[0]), "name": f_[2]})
                elif kind == "-":
                    f_ = rest.split()
                    d = dict(kv.split("=", 1) for kv in f_[3:])
                    self.gone.append({"frame": int(f_[0]), "name": f_[2], "life": int(d["life"]),
                                      "travel": float(d["travel"]), "pmax": float(d["pmax"]),
                                      "pend": float(d["pend"])})

    def gone_named(self, name: str) -> list[dict]:
        return [g for g in self.gone if g["name"] == name]

    def born_named(self, name: str) -> list[dict]:
        return [b for b in self.born if b["name"] == name]

    def procs(self) -> set[int]:
        return {p["proc"] for p in self.player}

    def first(self, key: str) -> int:
        return self.player[0][key]

    def last(self, key: str) -> int:
        return self.player[-1][key]

    def max_of(self, key: str) -> int:
        return max(p[key] for p in self.player)


# Checks: (description, function of the Log returning a value, a predicate on that value).
def flies(name: str, at_least: float):
    return (f"a {name} flies >= {at_least:g}",
            lambda L: max((g["travel"] for g in L.gone_named(name)), default=0.0),
            lambda v: v >= at_least)


def goes_and_returns(name: str, out: float, back: float):
    return (f"a {name} goes >= {out:g} from the player and ends <= {back:g} from him",
            lambda L: [(round(g["pmax"]), round(g["pend"])) for g in L.gone_named(name)],
            lambda v: any(a >= out and b <= back for a, b in v))


def spawns(name: str, at_least: int = 1):
    return (f">= {at_least} {name} created", lambda L: len(L.born_named(name)), lambda v: v >= at_least)


def spends(key: str, at_least: int):
    return (f"{key} drops by >= {at_least}", lambda L: L.max_of(key) - L.last(key), lambda v: v >= at_least)


def runs_proc(*procs: int):
    return (f"the player runs procedure {'/'.join(map(str, procs))}",
            lambda L: sorted(L.procs() & set(procs)), lambda v: len(v) > 0)


def walk_speed_below(limit: float):
    return (f"walking speed (stick held, frames 715-735) < {limit:g}",
            lambda L: max((p["speedF"] for p in L.player if 715 <= p["frame"] <= 735 and p["proc"] == PROC_WALK),
                          default=0.0),
            lambda v: 0.0 < v < limit)


def glides(frames: int, distance: float):
    def value(L):
        g = [p for p in L.player if p["proc"] == PROC_FAN_GLIDE]
        if not g:
            return (0, 0.0)
        a = [float(c) for c in g[0]["pos"].split(",")]
        b = [float(c) for c in g[-1]["pos"].split(",")]
        return (g[-1]["frame"] - g[0]["frame"], round(((a[0] - b[0]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5))
    return (f"the player glides (procedure {PROC_FAN_GLIDE}) >= {frames} frames and >= {distance:g} across",
            value, lambda v: v[0] >= frames and v[1] >= distance)


def reaches(x: float, z: float, within: float):
    def value(L):
        p = L.player[-1]["pos"].split(",")
        return round(((float(p[0]) - x) ** 2 + (float(p[2]) - z) ** 2) ** 0.5)
    return (f"the player ends within {within:g} (across) of {x:g},{z:g}", value, lambda v: v <= within)


ITEMS = "pipeline-items.txt"  # X twice, X held, A, B, the stick a quarter turn; five rounds
CASES = {
    # name: (COS_BOOT_ITEMS, input script, checks[, {"stage": boot stage (default the Outset pier,
    # sea:44:0), "place": COS_TOOL_PLACE}])
    "bow": ("27,38", ITEMS, [flies("Arrow", 2000), spends("arrows", 2)]),
    "fire-arrow": ("27,35,B2,0A,38", "tool-arrows-r1.txt", [flies("Arrow", 2000), spends("magic", 1)]),
    "ice-arrow": ("27,35,B2,0A,38", "tool-arrows-r2.txt", [flies("Arrow", 2000), spends("magic", 1)]),
    "light-arrow": ("27,36,B2,0A,38", "tool-arrows-r3.txt",
                    [flies("Arrow", 2000), spawns("Arrow_l"), spends("magic", 2)]),
    "boomerang": ("2D,38", ITEMS, [goes_and_returns("Boom", 1500, 200)]),
    "hookshot": ("2F,38", ITEMS, [goes_and_returns("HShot", 1000, 200)]),
    "grappling-hook": ("25,38", ITEMS, [goes_and_returns("Himo2", 500, 200)]),
    "bombs": ("31,38", "pipeline-bombs.txt", [spawns("Bomb", 2), spends("bombs", 2)]),
    "bait": ("82,2C,38", ITEMS, [spawns("Esa", 5)]),
    # A swing costs no magic (daPy_lk_c::procFanSwing), a glide does (procFanGlide_init).
    "deku-leaf": ("34,38,B2,0A", ITEMS, [runs_proc(PROC_FAN_SWING)]),
    "deku-leaf-glide": ("34,38,B2,0A", "tool-leaf-glide.txt", [glides(60, 1500), spends("magic", 3)]),
    "skull-hammer": ("33,38", ITEMS, [runs_proc(*PROC_HAMMER)]),
    "iron-boots": ("29,38", ITEMS, [walk_speed_below(8.0)]),
    "no-boots": ("50,38", ITEMS, [("walking speed (frames 715-735) >= 12 (the reference for iron-boots)",
                                   walk_speed_below(1e9)[1], lambda v: v >= 12.0)]),
    "magic-armor": ("2A,38,B2,0A", ITEMS, [spends("magic", 3)]),
    # A test room (no sea to fall into): the hook sticks in a target and pulls the player there.
    "hookshot-target": ("2F,38", "tool-hookshot-target.txt",
                        [runs_proc(PROC_HOOKSHOT_FLY), reaches(500, -2400, 150)],
                        {"stage": "K_Teste:0:0", "place": "500,0,-1500,180"}),
    "red-potion": ("50,51,38", ITEMS, [runs_proc(PROC_BOTTLE_DRINK)]),
    "empty-bottle": ("50,38", ITEMS, [runs_proc(PROC_BOTTLE_SWING)]),
}
SHOTS = "420,460,520,560,600,900"


def run_case(name: str, out: str, disc: str | None) -> tuple[str, bool, list[str]]:
    items, script, checks = CASES[name][:3]
    opts = CASES[name][3] if len(CASES[name]) > 3 else {}
    run_dir = os.path.join(out, name)
    env = dict(os.environ, COS_TOOL_WATCH="1", COS_BOOT_ITEMS=items)
    if "place" in opts:
        env["COS_TOOL_PLACE"] = opts["place"]
    cmd = [COS_RUN, "run", "--uncapped", "--frames", "1500", "--stage", opts.get("stage", "sea:44:0"),
           "--input", f"{INPUT}/{script}", "--shot", SHOTS, "--run-dir", run_dir, "--quiet"]
    if disc:
        cmd += ["--disc", disc]
    rc = subprocess.run(cmd, cwd=REPO, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode
    if rc != 0:
        return name, False, [f"run exit {rc} ({run_dir}/run.log)"]
    try:
        log = Log(os.path.join(run_dir, "tool_watch.txt"))
    except (OSError, KeyError, ValueError, IndexError) as e:
        return name, False, [f"tool_watch.txt unreadable: {e}"]
    if not log.player:
        return name, False, ["no player samples (did the stage load?)"]
    lines, ok = [], True
    for desc, value, good in checks:
        v = value(log)
        passed = bool(good(v))
        ok = ok and passed
        lines.append(f"{'ok  ' if passed else 'FAIL'} {desc}: {v}")
    return name, ok, lines


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--only", help="comma list of case names")
    ap.add_argument("--disc")
    ap.add_argument("--out")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()
    names = list(CASES)
    if args.list:
        for n in names:
            print(n, CASES[n][0], CASES[n][1])
        return 0
    if args.only:
        names = [n for n in args.only.split(",") if n]
        bad = [n for n in names if n not in CASES]
        if bad:
            print(f"tool_sweep: unknown case(s) {', '.join(bad)}", file=sys.stderr)
            return 2
    out = args.out or os.path.join(REPO, "build", "tool-sweep", time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(out, exist_ok=True)
    results = {}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for name, ok, lines in pool.map(lambda n: run_case(n, out, args.disc), names):
            results[name] = (ok, lines)
            print(f"tool_sweep: {name}: {'PASS' if ok else 'FAIL'}", flush=True)
    report = ["# Tool sweep", ""]
    for name in names:
        ok, lines = results[name]
        report.append(f"- **{name}** ({CASES[name][0]}, {CASES[name][1]}): {'PASS' if ok else 'FAIL'}")
        report += [f"  - {l}" for l in lines]
    failed = [n for n in names if not results[n][0]]
    report += ["", f"{len(names) - len(failed)} of {len(names)} passed" + (f"; failed: {', '.join(failed)}" if failed else "")]
    with open(os.path.join(out, "tool_sweep.md"), "w") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report))
    print(f"tool_sweep: report {os.path.relpath(os.path.join(out, 'tool_sweep.md'), REPO)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
