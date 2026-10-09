#!/usr/bin/env python3
"""Per-scene performance of a Switch log, and the phase-0 go/no-go numbers of the deko3d migration
(docs/DEKO3D_MIGRATION_PLAN.md sections 2.3 and 2.4).

    scripts/switch/perf_scenes.py [--csv FILE] [--all-windows] centollos.log [more logs...]

Reads the lines a run with COS_PERF_EVERY=60 (and COS_HITCH_MS) writes (docs/SWITCH_BUILD.md, "Run"):
`[cos] perf frames a-b:` (fps, game thread, begin), `[cos] perf-switch frames a-b:` (render worker
busy, pipelines compiled, scene), `perf-switch dawn gl per frame:` (passes, draws),
`perf-switch gpu per frame` (GPU ms), `perf-switch cpu per frame:` (render worker CPU, mode, clocks),
`[cos] hitch frame N:` (pipeline compile), `[cos] stage: NAME room N created at frame F`, the
`[cos] precompile` summary and `[cos] heaps:`. Each 60-frame window is classified, with
frame = 1000 / fps:

    GPU-bound     GPU ms >= 0.9 x frame
    worker-bound  worker busy >= 0.9 x frame and GPU < 0.8 x frame, or worker CPU >= 0.8 x frame
    game-bound    game thread >= 0.9 x frame and begin (the slot wait) < 0.1 x frame
    compile       a hitch line in the window with pipeline compile > 0 ms
    paced         none of these (the 30 fps pacing is the limit)

A window can be compile and one of the others; its class is the first that applies in the order
compile, GPU-bound, worker-bound, game-bound, paced. Scenes are `stage:room` from the last
`[cos] stage:` line before the window (else the perf-switch line's scene name). "Play" windows are
those whose perf-switch scene is ROOM_SCENE (the player in a stage), the set the go/no-go reads.
Several logs (or several sessions in one log: the frame count starts again) are read in turn.
"""
import argparse
import csv
import re
import statistics
import sys
from collections import OrderedDict, defaultdict

NUM = r"([0-9]+(?:\.[0-9]+)?)"
RE_PERF = re.compile(r"\[cos\] perf frames (\d+)-(\d+): game thread " + NUM + r" ms avg.*?\(begin " + NUM
                     + r".*?; " + NUM + r" fps")
RE_SWITCH = re.compile(r"\[cos\] perf-switch frames (\d+)-(\d+):.*?render worker " + NUM + r" ms/frame busy")
RE_SWITCH_PIPES = re.compile(r"pipelines (\d+) created, (\d+) compiled in " + NUM + r" ms")
RE_SWITCH_SCENE = re.compile(r"scene (\w+)\s*$")
RE_DAWN = re.compile(r"perf-switch dawn gl per frame: " + NUM + r" passes, " + NUM + r" draws")
RE_GPU = re.compile(r"perf-switch gpu per frame \((\d+) read back\): " + NUM + r" ms \(p95 " + NUM)
RE_CPU = re.compile(r"perf-switch cpu per frame: game thread " + NUM + r" ms, render worker " + NUM
                    + r" ms \(busy " + NUM + r" ms wall\)(.*)$")
RE_CPU_MODE = re.compile(r"; (\w+), config \w+, cpu " + NUM + r" MHz, gpu " + NUM + r" MHz")
RE_HITCH = re.compile(r"\[cos\] hitch frame (\d+): busy " + NUM + r" ms.*?pipeline compile " + NUM
                      + r" ms \((\d+)\)")
RE_STAGE = re.compile(r"\[cos\] stage: (\S+) room (-?\d+) created at frame (\d+)")
# the build split ("tint X ms, GL context Y ms each") is in the GL NRO's logs only
RE_PRECOMPILE_DONE = re.compile(r"\[cos\] precompile done: (\d+)/(\d+) pipelines, " + NUM + r" s(?:.*?tint "
                                + NUM + r" ms, GL context " + NUM + r" ms each)?")
RE_SCREEN_AUTO = re.compile(r"\[cos\] precompile screen auto: .*(no loading screen|loading screen until)")
RE_HEAP = re.compile(r"\[cos\] heaps: (\w+)\s+0x[0-9a-f]+ size (0x[0-9a-f]+) free (0x[0-9a-f]+)")

CLASSES = ["compile", "gpu", "worker", "game", "paced"]


class Window:
    __slots__ = ("log", "session", "first", "last", "fps", "game", "begin", "busy", "pipes", "scene_name",
                 "passes", "draws", "gpu", "gpu_p95", "worker_cpu", "mode", "cpu_mhz", "gpu_mhz", "compile_ms",
                 "compile_n", "stage", "cls")

    def __init__(self, log, session, first, last, game, begin, fps):
        self.log, self.session, self.first, self.last = log, session, first, last
        self.game, self.begin, self.fps = game, begin, fps
        self.busy = self.pipes = self.passes = self.draws = self.gpu = self.gpu_p95 = self.worker_cpu = None
        self.scene_name = self.mode = self.cpu_mhz = self.gpu_mhz = None
        self.compile_ms, self.compile_n = 0.0, 0
        self.stage = None
        self.cls = None

    @property
    def frame_ms(self):
        return 1000.0 / self.fps if self.fps > 0 else float("inf")

    @property
    def play(self):
        return self.scene_name == "ROOM_SCENE"

    def classify(self):
        f = self.frame_ms
        if self.compile_ms > 0:
            return "compile"
        if self.gpu is not None and self.gpu >= 0.9 * f:
            return "gpu"
        if self.busy is not None and ((self.busy >= 0.9 * f and (self.gpu is None or self.gpu < 0.8 * f))
                                      or (self.worker_cpu is not None and self.worker_cpu >= 0.8 * f)):
            return "worker"
        if self.game >= 0.9 * f and self.begin < 0.1 * f:
            return "game"
        return "paced"


def parse(path, session_base):
    windows, hitches, info = [], [], defaultdict(list)
    session = session_base
    stages = []  # (session, frame, "stage:room")
    current = None
    last_first = None
    with open(path, errors="replace") as f:
        for line in f:
            m = RE_PERF.search(line)
            if m:
                first, last = int(m.group(1)), int(m.group(2))
                if last_first is not None and first <= last_first:
                    session += 1
                last_first = first
                current = Window(path, session, first, last, float(m.group(3)), float(m.group(4)),
                                 float(m.group(5)))
                windows.append(current)
                continue
            m = RE_STAGE.search(line)
            if m:
                stages.append((session, int(m.group(3)), "%s:%s" % (m.group(1), m.group(2))))
                continue
            m = RE_HITCH.search(line)
            if m:
                hitches.append((session, int(m.group(1)), float(m.group(2)), float(m.group(3)), int(m.group(4))))
                continue
            m = RE_PRECOMPILE_DONE.search(line)
            if m:
                tint, glctx = (float(m.group(4)), float(m.group(5))) if m.group(4) else (None, None)
                info["precompile"].append((session, int(m.group(1)), int(m.group(2)), float(m.group(3)),
                                           tint, glctx))
                continue
            m = RE_SCREEN_AUTO.search(line)
            if m:
                info["loading_screen"].append((session, m.group(1) != "no loading screen"))
                continue
            m = RE_HEAP.search(line)
            if m:
                info["heap_" + m.group(1)].append((int(m.group(2), 16), int(m.group(3), 16)))
                continue
            if current is None:
                continue
            m = RE_SWITCH.search(line)
            if m and int(m.group(1)) == current.first:
                current.busy = float(m.group(3))
                p = RE_SWITCH_PIPES.search(line)
                if p:
                    current.pipes = int(p.group(2))
                s = RE_SWITCH_SCENE.search(line)
                if s:
                    current.scene_name = s.group(1)
                continue
            m = RE_DAWN.search(line)
            if m:
                current.passes, current.draws = float(m.group(1)), float(m.group(2))
                continue
            m = RE_GPU.search(line)
            if m:
                current.gpu, current.gpu_p95 = float(m.group(2)), float(m.group(3))
                continue
            m = RE_CPU.search(line)
            if m:
                current.worker_cpu = float(m.group(2))
                c = RE_CPU_MODE.search(m.group(4))
                if c:
                    current.mode, current.cpu_mhz, current.gpu_mhz = c.group(1), float(c.group(2)), float(c.group(3))
                continue
    for w in windows:
        for s, frame, name in stages:
            if s == w.session and frame <= w.last:
                w.stage = name
        for s, frame, busy, ms, n in hitches:
            if s == w.session and w.first <= frame <= w.last and ms > 0:
                w.compile_ms += ms
                w.compile_n += n
        w.cls = w.classify()
    return windows, hitches, info, session


def med(v):
    return statistics.median(v) if v else None


def pct(v, p):
    if not v:
        return None
    v = sorted(v)
    return v[min(len(v) - 1, int(len(v) * p))]


def fmt(x, digits=1):
    return "-" if x is None else ("%.*f" % (digits, x))


def scene_key(w, by_clock):
    key = w.stage or w.scene_name or "?"
    return "%s @%.0f" % (key, w.cpu_mhz) if by_clock and w.cpu_mhz else key


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--csv", help="also write every window to this CSV file")
    ap.add_argument("--all-windows", action="store_true", help="per-scene table over every window, not only play")
    args = ap.parse_args()

    windows, hitches, info = [], [], defaultdict(list)
    session = 0
    for path in args.logs:
        w, h, i, session = parse(path, session)
        session += 1
        windows += w
        hitches += h
        for k, v in i.items():
            info[k] += v
    if not windows:
        print("no `[cos] perf frames` lines: run with COS_PERF_EVERY=60 (docs/SWITCH_BUILD.md, \"Run\")")
        return 1
    with_gpu = sum(1 for w in windows if w.gpu is not None)
    modes = sorted({"%s %.0f/%.1f MHz" % (w.mode, w.cpu_mhz, w.gpu_mhz) for w in windows if w.mode})
    print("%d windows in %d log(s), %d session(s); %d with GPU times, %d with worker CPU; %s" % (
        len(windows), len(args.logs), len({w.session for w in windows}), with_gpu,
        sum(1 for w in windows if w.worker_cpu is not None), ", ".join(modes) or "mode unknown"))

    # per scene
    sel = windows if args.all_windows else [w for w in windows if w.play]
    # with more than one CPU clock in the logs, a scene's windows are split per clock
    by_clock = len({w.cpu_mhz for w in windows if w.cpu_mhz}) > 1
    by_scene = OrderedDict()
    for w in sel:
        by_scene.setdefault(scene_key(w, by_clock), []).append(w)
    print()
    print("%-24s %5s %13s %13s %15s %7s %13s %7s  %s" % (
        "scene", "wins", "fps med/p10", "GPU med/p95", "worker cpu/busy", "game", "draws med/p90", "us/draw",
        "classes (compile gpu worker game paced)"))
    for scene, ws in by_scene.items():
        fps = [w.fps for w in ws]
        gpu = [w.gpu for w in ws if w.gpu is not None]
        gpu95 = [w.gpu_p95 for w in ws if w.gpu_p95 is not None]
        cpu = [w.worker_cpu for w in ws if w.worker_cpu is not None]
        busy = [w.busy for w in ws if w.busy is not None]
        draws = [w.draws for w in ws if w.draws]
        per_draw = [1000.0 * w.worker_cpu / w.draws for w in ws if w.worker_cpu is not None and w.draws]
        counts = [sum(1 for w in ws if w.cls == c) for c in CLASSES]
        print("%-24s %5d %6s/%-6s %6s/%-6s %7s/%-7s %7s %6s/%-6s %7s  %s" % (
            scene[:24], len(ws), fmt(med(fps)), fmt(pct(fps, 0.1)), fmt(med(gpu)), fmt(med(gpu95)),
            fmt(med(cpu)), fmt(med(busy)), fmt(med([w.game for w in ws])), fmt(med(draws), 0),
            fmt(pct(draws, 0.9), 0), fmt(med(per_draw)), " ".join("%d" % c for c in counts)))

    # go / no-go (section 2.4)
    play = [w for w in windows if w.play]
    slow = [w for w in play if w.fps < 29.0]
    slow_cls = defaultdict(int)
    for w in slow:
        slow_cls[w.cls] += 1
    print()
    print("go / no-go (docs/DEKO3D_MIGRATION_PLAN.md 2.4), play windows only (%d):" % len(play))
    share = 100.0 * slow_cls["worker"] / len(slow) if slow else 0.0
    print("  windows under 29 fps: %d (%s); worker-bound share %.0f %% (GO on performance if >= 20 %%)" % (
        len(slow), ", ".join("%s %d" % (c, slow_cls[c]) for c in CLASSES if slow_cls[c]) or "none", share))
    worst = None
    for scene, ws in by_scene.items():
        cpu = [w.worker_cpu for w in ws if w.play and w.worker_cpu is not None]
        if cpu and (worst is None or med(cpu) > worst[1]):
            worst = (scene, med(cpu))
    print("  highest scene median of worker CPU: %s (GO on performance if >= 18 ms)" % (
        "%s %.1f ms" % worst if worst else "-"))
    go_perf = (slow and share >= 20.0) or (worst is not None and worst[1] >= 18.0)
    all_gpu = bool(slow) and slow_cls["gpu"] == len(slow)
    print("  -> performance: %s" % ("GO" if go_perf else
                                     "DEFER (every window under 29 fps is GPU-bound)" if all_gpu else
                                     "no GO from these windows"))
    compile_hitches = [h for h in hitches if h[3] > 0]
    if compile_hitches:
        print("  compile hitches: %d (pipeline compile %s ms median, max %.0f; %d pipelines)" % (
            len(compile_hitches), fmt(med([h[3] for h in compile_hitches]), 0), max(h[3] for h in compile_hitches),
            sum(h[4] for h in compile_hitches)))
    else:
        print("  compile hitches: none")
    screens = dict(info["loading_screen"])  # session -> the last decision of that session
    for sess, done, total, secs, tint, glctx in info["precompile"]:
        screen = "" if sess not in screens else (" behind a loading screen" if screens[sess] else " behind the game")
        split = "" if tint is None else "; per build tint %.0f ms, GL context %.0f ms" % (tint, glctx)
        print("  warm-up: %d/%d pipelines in %.1f s%s%s" % (done, total, secs, screen, split))
    if not info["precompile"]:
        print("  warm-up: no `[cos] precompile done` line")
    play_pipes = sum(w.pipes or 0 for w in play)
    print("  pipelines compiled in play windows (the bundle's misses): %d" % play_pipes)
    passes = [w.passes for w in play if w.passes]
    print("  EFB passes per frame in play: median %s, max %s" % (fmt(med(passes)), fmt(max(passes) if passes else None)))
    for name in ("root", "main"):
        heaps = info.get("heap_" + name)
        if heaps:
            size, free = heaps[0]
            print("  heap %s after init: %.1f MiB free of %.1f MiB" % (name, free / 1048576.0, size / 1048576.0))

    if args.csv:
        with open(args.csv, "w", newline="") as f:
            out = csv.writer(f)
            out.writerow(["log", "session", "first", "last", "scene", "perf_scene", "fps", "game_ms", "begin_ms",
                          "worker_busy_ms", "worker_cpu_ms", "gpu_ms", "gpu_p95_ms", "passes", "draws",
                          "pipelines_compiled", "compile_hitch_ms", "class"])
            for w in windows:
                out.writerow([w.log, w.session, w.first, w.last, w.stage or "", w.scene_name or "", w.fps, w.game,
                              w.begin, w.busy, w.worker_cpu, w.gpu, w.gpu_p95, w.passes, w.draws, w.pipes,
                              w.compile_ms, w.cls])
    return 0


if __name__ == "__main__":
    sys.exit(main())
