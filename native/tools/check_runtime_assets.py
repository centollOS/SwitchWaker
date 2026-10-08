#!/usr/bin/env python3
"""Check the runtime assets against the decompilation's extraction (docs/RUNTIME_ASSETS.md, step 3).

    native/tools/check_runtime_assets.py --stubs build/assets-stubs/GZLE01 \\
        --run-dir RUN --decomp build/decomp

RUN/assets/ holds what COS_SMOKE=assets wrote: every array as the loader (pc_assets.cpp) filled it,
read from the disc through Aurora's DVD layer, Yaz0 and RARC readers of its own and the DOL/REL
section tables. The reference is independent: the decompilation's dtk split of the same disc
(build/decomp/build/GZLE01/bin/assets/<header>.bin, gen_assets.sh), the input its header converters
read. Vec and cXy arrays are compared after swapping their big-endian floats, as the loader does
and the converters did. Every byte must match. Reads disc-derived files only locally; writes nothing.
"""
import argparse
import json
import struct
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--stubs", required=True, type=Path, help="gen_assets.sh --stubs output")
    ap.add_argument("--run-dir", required=True, type=Path, help="COS_RUN_DIR of the assets smoke")
    ap.add_argument("--decomp", required=True, type=Path)
    args = ap.parse_args()
    index = json.loads((args.stubs / "include/assets/cos_asset_index.json").read_text())
    bins = args.decomp / "build/GZLE01/bin/assets"
    bad = 0
    for row in index:
        got_path = args.run_dir / "assets" / f"{row['id']:03d}_{row['name']}.bin"
        want_path = bins / (Path(row["header"]).stem + ".bin")
        if not got_path.exists() or not want_path.exists():
            print(f"MISSING {row['header']}: {got_path if not got_path.exists() else want_path}")
            bad += 1
            continue
        got, want = got_path.read_bytes(), want_path.read_bytes()
        if row["kind"] == "COS_ASSET_F32":
            n = len(want) // 4
            want = struct.pack(f"<{n}I", *struct.unpack(f">{n}I", want[: 4 * n])) + want[4 * n:]
        if got != want:
            diff = next((i for i, (a, b) in enumerate(zip(got, want)) if a != b), min(len(got), len(want)))
            print(f"DIFFER {row['header']}: {len(got)} vs {len(want)} bytes, first difference at {diff:#x}")
            bad += 1
    print(f"check_runtime_assets: {len(index) - bad} of {len(index)} arrays identical to the dtk extraction")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
