#!/usr/bin/env python3
"""Summarise a draw census (COS_DRAW_CENSUS, native/patches/aurora/0008).

    native/tools/census.py <run dir>/census-<frame>-draws.csv [--top N] [--width W --height H]

Prints the frame's fragment total in screens (fragments / (W x H), default 1280x720), then the
fragments per draw-list bucket and per bucket|material (or packet class), largest first: the
fragments every GX fragment shader ran for ("shaded": all rasterised fragments for draws that
discard or blend; for opaque draws the Mac's hidden-surface removal may skip hidden ones) and
those that survived the alpha compare ("kept"). With the passes CSV next to it, the render passes
and EFB copies of the frame too.
"""
import argparse
import collections
import csv
import os
import sys


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("draws_csv")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--width", type=int, default=1280)
    ap.add_argument("--height", type=int, default=720)
    args = ap.parse_args()
    screen = float(args.width * args.height)
    rows = list(csv.DictReader(open(args.draws_csv)))
    frags = lambda r: int(r.get("frags") or 0)
    kept = lambda r: int(r.get("frags_kept") or 0)
    total = sum(frags(r) for r in rows)
    print(f"{len(rows)} draws; fragments shaded {total} = {total / screen:.2f} screens of "
          f"{args.width}x{args.height}, kept after the alpha compare {sum(kept(r) for r in rows) / screen:.2f}")
    if total == 0:
        print("(no fragment counts: the run lacked AURORA_DRAW_CENSUS=1, which COS_DRAW_CENSUS sets)")

    def table(title, key):
        groups = collections.defaultdict(lambda: [0, 0, 0, 0, 0, collections.Counter(), collections.Counter()])
        for r in rows:
            g = groups[key(r)]
            g[0] += 1
            g[1] += frags(r)
            g[2] += kept(r)
            g[3] += int(r["vtx"])
            g[4] += int(r["alpha_test"])
            g[5][r["textures"]] += frags(r) or 1
            g[6][f"tev{r['tev']} blend{r['blend']} zcmp{r['zcmp']} zupd{r['zupd']} fog{r['fog']}"] += frags(r) or 1
        print(f"--- by {title}: screens shaded / kept, draws, vertices, alpha-tested draws, main config, textures")
        for name, g in sorted(groups.items(), key=lambda kv: -kv[1][1])[: args.top]:
            cfg = g[6].most_common(1)[0][0] if g[6] else ""
            tex = g[5].most_common(1)[0][0] if g[5] else ""
            print(f"{g[1] / screen:7.2f} /{g[2] / screen:6.2f} {g[0]:5d} dr {g[3]:7d} vtx at {g[4]:4d}  "
                  f"{name[:50]:50s} {cfg}  {tex[:60]}")

    table("bucket", lambda r: r["marker"].split("|")[0] or "(none)")
    table("bucket|material", lambda r: r["marker"] or "(none)")
    passes = args.draws_csv.replace("-draws.csv", "-passes.csv")
    if os.path.exists(passes):
        print("--- passes and copies")
        for r in csv.DictReader(open(passes)):
            print(f"{r['op']:>3} {r['kind']:5s} {r['label']:14s} {r['width']}x{r['height']} {r['load']:5s} "
                  f"{r['draws']:>5} draws  copy {r['resolve']} {r['detail']}")


if __name__ == "__main__":
    sys.exit(main())
