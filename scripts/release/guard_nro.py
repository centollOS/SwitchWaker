#!/usr/bin/env python3
"""Refuse to publish a game binary that carries the disc's data arrays (docs/RUNTIME_ASSETS.md).

    scripts/release/guard_nro.py BINARY... [--decomp build/decomp]

A binary built with COS_RUNTIME_ASSETS=ON holds none of the 163 arrays the decompilation cuts out of
main.dol and the RELs; one built from gen_assets.sh's disc headers holds all of them. This looks for
each array's bytes (from the local dtk split, build/decomp/build/GZLE01/bin/assets/*.bin, as stored:
big-endian, and with its 32-bit words swapped, as the Vec/cXy arrays are compiled) in each binary.
Arrays shorter than 64 bytes, or of a single repeated byte, are skipped (too common to mean anything).
Exits 1 if any array is found. Needs the local split, so it runs where the release is built, never in CI.
"""
import argparse
import struct
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("binaries", nargs="+", type=Path)
    ap.add_argument("--decomp", type=Path, default=Path("build/decomp"))
    args = ap.parse_args()
    bins = sorted((args.decomp / "build/GZLE01/bin/assets").glob("*.bin"))
    if len(bins) != 163:
        sys.exit(f"guard_nro: expected the 163 extracted arrays in {args.decomp}/build/GZLE01/bin/assets, "
                 f"found {len(bins)} (run native/tools/gen_assets.sh with the disc first)")
    probes = []
    for b in bins:
        data = b.read_bytes()
        if len(data) < 64 or len(set(data)) <= 1:
            continue
        n = len(data) // 4
        swapped = struct.pack(f"<{n}I", *struct.unpack(f">{n}I", data[: 4 * n]))
        probes.append((b.stem, data[:64], swapped[:64]))
    bad = 0
    for path in args.binaries:
        blob = path.read_bytes()
        found = [name for name, raw, sw in probes if raw in blob or sw in blob]
        if found:
            bad += 1
            print(f"guard_nro: {path}: {len(found)} of {len(probes)} disc arrays inside "
                  f"(e.g. {', '.join(found[:5])}): built with the disc's headers, not COS_RUNTIME_ASSETS")
        else:
            print(f"guard_nro: {path}: none of the {len(probes)} disc arrays (>= 64 bytes) inside")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
