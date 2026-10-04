#!/usr/bin/env python3
"""Match a texture census against a Dolphin-format HD texture pack (docs/HD_TEXTURES.md).

    native/tools/hd_census.py CENSUS.txt [CENSUS2.txt ...] --pack PACK

CENSUS files come from runs with COS_HD_CENSUS=<file> (pc_hd_textures.cpp): the Dolphin name of
every distinct static texture the game resolved, "tex1_<w>x<h>_<texhash>[_<tluthash>]_<fmt>".
PACK is any of: a directory of loose tex1_*.dds/.png files (searched recursively, e.g. a pack's
GZL folder), a converted pack directory (index.bin, from cos_hd_pack), a .7z/.zip archive (listed
with bsdtar), or a text file with one file name per line.

A census name matches a pack name with the same width, height, format and texture hash and the
same TLUT hash, or a pack name whose TLUT hash or texture hash is "$" (Dolphin's wildcards, in
that order). "_m" (mipmaps enabled on the sampler) and "_arb" in pack names are ignored, as in
Aurora's lookup. Prints the match ratio overall and per format, the near misses (same texture
hash, size and format but another TLUT hash: a TLUT hashing problem would show here), and with
--list-missing the unmatched names.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
from collections import Counter, defaultdict

NAME = re.compile(r"^tex1_(\d+)x(\d+)(?:_m)?_([0-9a-f]{16}|\$)(?:_([0-9a-f]{16}|\$))?_(\d+)(?:_arb)?(?:\.(?:dds|png))?$",
                  re.IGNORECASE)
FORMATS = {0: "I4", 1: "I8", 2: "IA4", 3: "IA8", 4: "RGB565", 5: "RGB5A3", 6: "RGBA8", 8: "C4", 9: "C8",
           10: "C14X2", 14: "CMPR"}


def parse(name):
    m = NAME.match(name.strip())
    if not m:
        return None
    w, h, tex, tlut, fmt = m.groups()
    return (int(w), int(h), int(fmt), tex.lower(), tlut.lower() if tlut else None)


def pack_names(pack):
    if os.path.isdir(pack) and os.path.exists(os.path.join(pack, "index.bin")):
        data = open(os.path.join(pack, "index.bin"), "rb").read()
        count, = struct.unpack_from("<I", data, 12)
        names_bytes, = struct.unpack_from("<I", data, 32)
        names = 40 + count * 32
        out = []
        for i in range(count):
            off, length = struct.unpack_from("<IH", data, 40 + i * 32)
            out.append(data[names + off:names + off + length].decode())
        return out
    if os.path.isdir(pack):
        return [f for _, _, files in os.walk(pack) for f in files]
    if pack.lower().endswith((".7z", ".zip", ".tar")):
        listing = subprocess.run(["bsdtar", "-tf", pack], check=True, capture_output=True, text=True).stdout
        return [line.rsplit("/", 1)[-1] for line in listing.splitlines()]
    return [line.strip().rsplit("/", 1)[-1] for line in open(pack)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("census", nargs="+")
    ap.add_argument("--pack", required=True)
    ap.add_argument("--list-missing", action="store_true")
    args = ap.parse_args()

    census = set()
    for path in args.census:
        for line in open(path):
            key = parse(line)
            if key:
                census.add(key)

    pack = set()
    by_tex = defaultdict(set)
    bad = 0
    for name in pack_names(args.pack):
        if not name.lower().startswith("tex1_"):
            continue
        key = parse(name)
        if key is None:
            bad += 1
            continue
        pack.add(key)
        by_tex[key[:4]].add(key[4])

    matched, missing, near = [], [], []
    for key in sorted(census):
        w, h, fmt, tex, tlut = key
        candidates = [key]
        if tlut is not None:
            candidates.append((w, h, fmt, tex, "$"))
        candidates.append((w, h, fmt, "$", tlut))
        if any(c in pack for c in candidates):
            matched.append(key)
        else:
            missing.append(key)
            if tlut is not None and by_tex.get(key[:4]):
                near.append(key)

    per_fmt = Counter(k[2] for k in census)
    per_fmt_hit = Counter(k[2] for k in matched)
    print(f"census: {len(census)} distinct static textures from {len(args.census)} file(s)")
    print(f"pack:   {len(pack)} named textures ({bad} unparsable tex1_ names ignored)")
    ratio = 100.0 * len(matched) / max(len(census), 1)
    print(f"match:  {len(matched)} of {len(census)} = {ratio:.1f}%")
    for fmt in sorted(per_fmt):
        print(f"  {FORMATS.get(fmt, fmt):7s} {per_fmt_hit[fmt]:5d} / {per_fmt[fmt]:5d}")
    print(f"near misses (same texture hash/size/format, other TLUT hash): {len(near)}")
    for key in near[:20]:
        w, h, fmt, tex, tlut = key
        print(f"  tex1_{w}x{h}_{tex}_{tlut}_{fmt}  pack TLUTs: {sorted(by_tex[key[:4]])[:4]}")
    if args.list_missing:
        for w, h, fmt, tex, tlut in missing:
            print(f"missing tex1_{w}x{h}_{tex}{'_' + tlut if tlut else ''}_{fmt}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
