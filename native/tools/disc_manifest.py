#!/usr/bin/env python3
"""Disc oracle: a manifest of the GZLE01 disc (docs/NATIVE_PORT_PHASE4_6.md, step 4.0d).

The phase 4 format steps check what the native game reads against an independent reading of the
same disc. This script is that reading, in pure Python (standard library only): the GameCube FST,
Yaz0, RARC archives, and the header fields of BMD/BDL/BMT, the J3D animations (BCK/BCA/BTK/BTP/
BRK/BPK/BVA/BLA/BLK), BTI, BFN, BMG, BMC, BLO, JPC, STB, dzs/dzr, dzb and AAF. It writes
build/native-mac/disc_manifest.json, which is derived from the disc and is never committed; the
manifest records counts, names, sizes and header fields, never file contents.

The disc is read at runtime only. On first use its SHA-1 (and main.dol's) is checked against the
expectations below, the only disc-derived values in the repository; the result is cached by
(path, size, mtime) in build/native-mac/runs/disc_check.txt, the cache native/tools/run.sh
uses too.

Usage
  disc_manifest.py [--disc ISO] [--out FILE]      verify the disc, write the manifest
  disc_manifest.py --verify [--disc ISO]          only the SHA-1 checks (exit 0 ok, 14 mismatch)
  disc_manifest.py --check-ls LS [--out FILE]     compare the listing COS_SMOKE=disc-ls wrote
                                                  (<run dir>/disc_ls.txt) with the manifest's FST:
                                                  file and directory counts, entry numbers, paths
                                                  and sizes (exit 0 equal, 1 different)
  disc_manifest.py --check-font FONT [--out FILE] compare what JUTResFont read in COS_SMOKE=font
                                                  (<run dir>/font.txt) with the manifest's BFN
                                                  headers: block counts, INF1 and every WID1/MAP1/
                                                  GLY1 field (exit 0 equal, 1 different)
  disc_manifest.py --check-arc ARC [--out FILE]  compare what JKRArchive read in COS_SMOKE=
                                                  arc-sweep (<run dir>/arc_sweep.txt) with the
                                                  manifest's RARC archives: every archive (nested
                                                  ones included), its counts, every node and every
                                                  file's path, ID, flags, size, offset and expanded
                                                  size (exit 0 equal, 1 different)
  disc_manifest.py --check-msg MSG [--out FILE]  compare what the game's message code read in
                                                  COS_SMOKE=msg-sweep (<run dir>/msg_sweep.txt)
                                                  with the manifest: every BMG (block counts,
                                                  INF1 counts and message-ID digest, DAT1 size),
                                                  the BMC colour tables and the message fonts'
                                                  BFN headers (exit 0 equal, 1 different)
  disc_manifest.py --check-jpa JPA [--out FILE]  compare what JParticle read in COS_SMOKE=
                                                  jpa-sweep (<run dir>/jpa_sweep.txt) with the
                                                  manifest's JPC files: every file's counts, every
                                                  emitter's user index, block, key, field and
                                                  texture counts and block order, every texture's
                                                  name and header (exit 0 equal, 1 different)
  disc_manifest.py --summary [--out FILE]         print the counts of an existing manifest

Manifest (JSON)
  disc      path, size, sha1, dol_sha1, game_id, revision
  fst       entries (root included), files, dirs (root excluded)
  files     one record per FST file, in FST order:
              path, entry, offset, size, format, and per format:
              yaz0: {"size": decompressed size}   (then the decompressed content is parsed)
              rarc: nodes [{type, name, dirs, files, first}], entries (raw file table count,
                    "." and ".." included), files [{path, id, flags, size, offset, format, ...}]
              size is always the stored size (FST or archive entry); a format's own size field
              is header_size
              j3d / bmg / bmc / bfn / blo: magic, header_size, blocks [{tag, size, ...counts}];
              BMG INF1 adds messages (entries with text), distinct_ids and ids_fnv (FNV-1a 64
              of every entry's big-endian u32 offset and u16 number), BMC CLT1 entries and
              colors_fnv (FNV-1a 64 of the colour table)
              bti: the ResTIMG header;  jpc: emitters [{res_id, blocks, keys, fields, textures,
              tags}], textures [names];  stb: version, blocks [{type, id}];  dzs/dzr: chunks
              [{tag, num}] and actors {tag: [{name, params, pos, angle, set_id}]};  dzb: counts
              and the vertex bounding box;  aaf: sections [{type, offset, size, count}]
  summary   per-format counts and the number of parse errors
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys
from collections import Counter

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
BUILD = os.path.join(REPO, "build", "native-mac")
DEFAULT_DISC = "/path/to/GZLE01.iso"
DEFAULT_OUT = os.path.join(BUILD, "disc_manifest.json")
DISC_CHECK_CACHE = os.path.join(BUILD, "runs", "disc_check.txt")

# The supported disc (decision H9): GZLE01 revision 0 as a plain .iso. These hashes are the only
# disc-derived values in the repository. The main.dol hash is the one scripts/prepare.py and
# native/tools/run.sh have always checked; the image hash was taken from that same disc.
EXPECTED_ISO_SHA1 = "0289e70f470dc758c73be9b8bcd8c865fb82d8ae"
EXPECTED_DOL_SHA1 = "8d28bab68bb5078c38e43f29206f0bd01f7e7a67"
GC_MAGIC = b"\xC2\x33\x9F\x3D"

# 2: "size" is always the stored size (a format's own size field moved to header_size).
# 3: BMG INF1 message counts and ID digest, BMC colour tables (step 4.6).
MANIFEST_VERSION = 3

EXIT_OK = 0
EXIT_DIFFERENT = 1
EXIT_USAGE = 2
EXIT_DISC = 14  # as centollos and run.sh


def u8(b, o):
    return b[o]


def u16(b, o):
    return struct.unpack_from(">H", b, o)[0]


def s16(b, o):
    return struct.unpack_from(">h", b, o)[0]


def u32(b, o):
    return struct.unpack_from(">I", b, o)[0]


def s32(b, o):
    return struct.unpack_from(">i", b, o)[0]


def tag4(b, o):
    return b[o:o + 4].decode("latin-1")


def cstr(b, o, limit=None):
    end = b.find(b"\0", o, len(b) if limit is None else min(len(b), o + limit))
    if end < 0:
        end = len(b) if limit is None else min(len(b), o + limit)
    raw = bytes(b[o:end])
    try:
        return raw.decode("shift_jis")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


class ParseError(Exception):
    pass


def need(cond, what):
    if not cond:
        raise ParseError(what)


# ---- disc check ---------------------------------------------------------------------------------

def file_stamp(path):
    st = os.stat(path)
    return "%d %d" % (st.st_size, int(st.st_mtime))


def dol_sha1(f):
    f.seek(0)
    head = f.read(0x440)
    need(head[0x1C:0x20] == GC_MAGIC, "no GameCube disc magic at 0x1C")
    dol_off = u32(head, 0x420)
    f.seek(dol_off)
    dh = f.read(0x100)
    offs = struct.unpack_from(">18I", dh, 0x00)
    sizes = struct.unpack_from(">18I", dh, 0x90)
    size = max([0x100] + [o + s for o, s in zip(offs, sizes) if s])
    f.seek(dol_off)
    return hashlib.sha1(f.read(size)).hexdigest()


def image_sha1(f):
    h = hashlib.sha1()
    f.seek(0)
    while True:
        chunk = f.read(16 << 20)
        if not chunk:
            break
        h.update(chunk)
    return h.hexdigest()


def verify_disc(path, quiet=False):
    """Checks main.dol's and the image's SHA-1 on first use; returns (ok, iso_sha1, dol_sha1)."""
    if not os.path.isfile(path):
        print("disc_manifest: %s: no such file" % path, file=sys.stderr)
        return False, None, None
    stamp = file_stamp(path)
    line = "ok\t%s\t%s\t%s" % (path, stamp, EXPECTED_ISO_SHA1)
    try:
        with open(DISC_CHECK_CACHE, encoding="utf-8") as c:
            if line in c.read().splitlines():
                return True, EXPECTED_ISO_SHA1, EXPECTED_DOL_SHA1
    except OSError:
        pass
    if not quiet:
        print("disc_manifest: first use of %s: checking the SHA-1 of main.dol and of the image"
              % path, file=sys.stderr)
    with open(path, "rb") as f:
        try:
            dsha = dol_sha1(f)
        except ParseError as e:
            print("disc_manifest: %s: %s" % (path, e), file=sys.stderr)
            return False, None, None
        if dsha != EXPECTED_DOL_SHA1:
            print("disc_manifest: %s: main.dol SHA-1 %s, expected %s (GZLE01 revision 0)"
                  % (path, dsha, EXPECTED_DOL_SHA1), file=sys.stderr)
            return False, None, dsha
        isha = image_sha1(f)
    if isha != EXPECTED_ISO_SHA1:
        print("disc_manifest: %s: image SHA-1 %s, expected %s (the plain GZLE01 revision 0 .iso, "
              "decision H9)" % (path, isha, EXPECTED_ISO_SHA1), file=sys.stderr)
        return False, isha, dsha
    os.makedirs(os.path.dirname(DISC_CHECK_CACHE), exist_ok=True)
    with open(DISC_CHECK_CACHE, "a", encoding="utf-8") as c:
        c.write(line + "\n")
    return True, isha, dsha


# ---- Yaz0 ---------------------------------------------------------------------------------------

def yaz0_decompress(src):
    need(len(src) >= 16 and src[:4] == b"Yaz0", "not Yaz0")
    size = u32(src, 4)
    out = bytearray(size)
    sp = 16
    dp = 0
    n_src = len(src)
    while dp < size:
        need(sp < n_src, "Yaz0: source ends early")
        code = src[sp]
        sp += 1
        if code == 0xFF and dp + 8 <= size and sp + 8 <= n_src:
            out[dp:dp + 8] = src[sp:sp + 8]
            dp += 8
            sp += 8
            continue
        for _ in range(8):
            if dp >= size:
                break
            if code & 0x80:
                out[dp] = src[sp]
                dp += 1
                sp += 1
            else:
                b1 = src[sp]
                b2 = src[sp + 1]
                sp += 2
                dist = (((b1 & 0x0F) << 8) | b2) + 1
                n = b1 >> 4
                if n == 0:
                    n = src[sp] + 0x12
                    sp += 1
                else:
                    n += 2
                s = dp - dist
                need(s >= 0, "Yaz0: back reference before the start")
                need(dp + n <= size, "Yaz0: copy past the end")
                if dist >= n:
                    out[dp:dp + n] = out[s:s + n]
                else:
                    pattern = bytes(out[s:dp])
                    out[dp:dp + n] = (pattern * (n // dist + 1))[:n]
                dp += n
            code = (code << 1) & 0xFF
    return out


# ---- formats ------------------------------------------------------------------------------------

def parse_bti(b, base=0):
    need(len(b) >= base + 0x20, "ResTIMG: short")
    return {
        "tex_format": u8(b, base + 0x00), "alpha": u8(b, base + 0x01),
        "width": u16(b, base + 0x02), "height": u16(b, base + 0x04),
        "wrap_s": u8(b, base + 0x06), "wrap_t": u8(b, base + 0x07),
        "index_texture": u8(b, base + 0x08), "color_format": u8(b, base + 0x09),
        "num_colors": u16(b, base + 0x0A), "palette_offset": u32(b, base + 0x0C),
        "mipmap": u8(b, base + 0x10), "min_filter": u8(b, base + 0x14),
        "mag_filter": u8(b, base + 0x15), "mipmap_count": u8(b, base + 0x18),
        "lod_bias": s16(b, base + 0x1A), "image_offset": u32(b, base + 0x1C),
    }


# Blocks whose u16 at 0x08 is an element count (J3D model data, BFN, BMG).
J3D_COUNT_BLOCKS = {"JNT1", "MAT3", "MAT2", "MDL3", "SHP1", "TEX1", "EVP1", "DRW1"}
# J3D animation blocks: offset of the s16 frame count (PAK1/PAF1 have a 4-byte attribute field).
J3D_ANM_FRAMEMAX = {"ANK1": 0x0A, "ANF1": 0x0A, "PAK1": 0x0C, "PAF1": 0x0C, "TTK1": 0x0A,
                    "TRK1": 0x0A, "TPT1": 0x0A, "VAF1": 0x0A, "CLK1": 0x0A, "CLF1": 0x0A,
                    "VCK1": 0x0A, "VCF1": 0x0A}


def jut_block_info(b, o, tag, size):
    info = {"tag": tag, "size": size}
    if tag in J3D_COUNT_BLOCKS:
        info["count"] = u16(b, o + 0x08)
        if tag == "TEX1":
            info["names"] = ntab(b, o + u32(b, o + 0x10))
        elif tag in ("JNT1", "MAT3", "MAT2", "SHP1") and u32(b, o + 0x14) != 0:
            info["names"] = ntab(b, o + u32(b, o + 0x14))  # SHP1 has none on this disc
    elif tag in J3D_ANM_FRAMEMAX:
        info["attribute"] = u8(b, o + 0x08)
        info["frame_max"] = s16(b, o + J3D_ANM_FRAMEMAX[tag])
    return info


def fnv1a64(data, h=0xCBF29CE484222325):
    """FNV-1a, 64 bits: a digest of values the manifest compares without storing them."""
    for byte in data:
        h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def bmg_ids(b, o, entries, entry_size):
    """INF1 message entries (the game: u32 text offset, u16 message number first): how many have text
    (offset != 0), how many distinct numbers those have, and ids_fnv, the FNV-1a digest of
    (offset as u32, number as u16), both big-endian, of every entry in order."""
    need(entry_size >= 6 and o + 0x10 + entries * entry_size <= len(b), "BMG INF1 entries")
    h = 0xCBF29CE484222325
    ids = set()
    messages = 0
    for i in range(entries):
        e = o + 0x10 + i * entry_size
        h = fnv1a64(b[e:e + 6], h)
        if u32(b, e) != 0:
            messages += 1
            ids.add(u16(b, e + 4))
    return {"messages": messages, "distinct_ids": len(ids), "ids_fnv": "%016x" % h}


def ntab(b, o):
    """ResNTAB: u16 count, pad, then {u16 key, u16 offset} entries; names relative to o."""
    need(o + 4 <= len(b), "ResNTAB outside the file")
    n = u16(b, o)
    names = []
    for i in range(n):
        off = u16(b, o + 4 + i * 4 + 2)
        names.append(cstr(b, o + off, 256))
    return names


def parse_jut_file(b, family):
    """JUTDataFileHeader files: 8-byte magic, size, block count, blocks from 0x20."""
    need(len(b) >= 0x20, "JUT file: short header")
    magic = b[0:8].decode("latin-1")
    size = u32(b, 8)
    if family in ("bmg", "bmc"):
        size *= 0x20  # BMG and BMC count their size in 32-byte units
    nblocks = u32(b, 0xC)
    need(size <= len(b), "JUT file: size 0x%x larger than the data 0x%x" % (size, len(b)))
    rec = {"magic": magic, "size": size, "block_count": nblocks, "blocks": []}
    # The file size is not always 32-byte aligned while the last block's size is: a block may
    # end up to 0x1F bytes past the file (some BTK), recorded as "overrun". Some files declare
    # more blocks than they hold (a few BMT): the walk stops at the end and records how many were
    # missing.
    o = 0x20
    for k in range(nblocks):
        if o + 8 > size:
            rec["blocks_missing"] = nblocks - k
            break
        tag = tag4(b, o)
        bsize = u32(b, o + 4)
        need(bsize >= 8 and o + bsize <= size + 0x1F,
             "JUT file: block %s size 0x%x out of range" % (tag, bsize))
        if o + bsize > size:
            rec["overrun"] = o + bsize - size
        if family == "j3d":
            info = jut_block_info(b, o, tag, bsize)
            if tag == "INF1":
                info["flags"] = u16(b, o + 0x08)
                info["packets"] = u32(b, o + 0x0C)
                info["vertices"] = u32(b, o + 0x10)
        elif family == "bmg":
            info = {"tag": tag, "size": bsize}
            if tag == "INF1":
                info["entries"] = u16(b, o + 0x08)
                info["entry_size"] = u16(b, o + 0x0A)
                info["group"] = u16(b, o + 0x0C)
                info.update(bmg_ids(b, o, info["entries"], info["entry_size"]))
            elif tag == "MID1":
                info["entries"] = u16(b, o + 0x08)
                info["format"] = u8(b, o + 0x0A)
                info["info"] = u8(b, o + 0x0B)
        elif family == "bmc":
            info = {"tag": tag, "size": bsize}
            if tag == "CLT1":
                n = u16(b, o + 0x08)
                info["entries"] = n
                info["colors_fnv"] = "%016x" % fnv1a64(b[o + 0x0C:o + 0x0C + 4 * n])
        elif family == "bfn":
            info = {"tag": tag, "size": bsize}
            if tag == "INF1":
                info.update(font_type=u16(b, o + 8), ascent=u16(b, o + 0xA), descent=u16(b, o + 0xC),
                            width=u16(b, o + 0xE), leading=u16(b, o + 0x10),
                            default_code=u16(b, o + 0x12))
            elif tag == "WID1":
                info.update(start=u16(b, o + 8), end=u16(b, o + 0xA))
            elif tag == "MAP1":
                info.update(method=u16(b, o + 8), start=u16(b, o + 0xA), end=u16(b, o + 0xC),
                            entries=u16(b, o + 0xE))
            elif tag == "GLY1":
                info.update(start=u16(b, o + 8), end=u16(b, o + 0xA), cell_width=u16(b, o + 0xC),
                            cell_height=u16(b, o + 0xE), texture_size=u32(b, o + 0x10),
                            texture_format=u16(b, o + 0x14), rows=u16(b, o + 0x16),
                            columns=u16(b, o + 0x18), texture_width=u16(b, o + 0x1A),
                            texture_height=u16(b, o + 0x1C))
        else:
            info = {"tag": tag, "size": bsize}
        rec["blocks"].append(info)
        o += bsize
    if family == "blo":
        rec["panes"] = dict(Counter(bl["tag"] for bl in rec["blocks"]
                                    if bl["tag"] in ("PAN1", "PIC1", "WIN1", "TBX1")))
    return rec


def parse_jpc(b):
    need(len(b) >= 0x20 and b[0:8] == b"JPAC1-00", "JPC: not JPAC1-00")
    n_emitters = u16(b, 8)
    n_textures = u16(b, 0xA)
    rec = {"version": "JPAC1-00", "emitter_count": n_emitters, "texture_count": n_textures,
           "emitters": [], "textures": []}
    o = 0x20
    for _ in range(n_emitters):
        need(o + 0x20 <= len(b) and b[o:o + 8] == b"JEFFjpa1", "JPC: emitter without JEFFjpa1")
        nblocks = u32(b, o + 0xC)
        em = {"res_id": u16(b, o + 0x18), "blocks": nblocks, "keys": u8(b, o + 0x14),
              "fields": u8(b, o + 0x15), "textures": u8(b, o + 0x16), "tags": []}
        bo = o + 0x20
        for _ in range(nblocks):
            need(bo + 8 <= len(b), "JPC: block past the end")
            size = u32(b, bo + 4)
            need(size >= 8, "JPC: block size %d" % size)
            em["tags"].append(tag4(b, bo))
            bo += size
        rec["emitters"].append(em)
        o = bo
    for _ in range(n_textures):
        need(o + 0x40 <= len(b) and b[o:o + 4] == b"TEX1", "JPC: texture without TEX1")
        size = u32(b, o + 4)
        tex = {"name": cstr(b, o + 0x0C, 0x14)}
        tex.update(parse_bti(b, o + 0x20))
        rec["textures"].append(tex)
        o += size
    return rec


def parse_stb(b):
    need(len(b) >= 0x20 and b[0:4] == b"STB\0", "STB: no signature")
    need(u16(b, 4) == 0xFEFF, "STB: byte order mark 0x%04x" % u16(b, 4))
    nblocks = u32(b, 0xC)
    rec = {"version": u16(b, 6), "size": u32(b, 8), "block_count": nblocks,
           "target": cstr(b, 0x10, 8), "target_version": u16(b, 0x1E), "blocks": []}
    o = 0x20
    for _ in range(nblocks):
        need(o + 8 <= len(b), "STB: block past the end")
        size = u32(b, o)
        btype = tag4(b, o + 4)
        need(size >= 8 and o + size <= len(b), "STB: block %r size 0x%x" % (btype, size))
        blk = {"type": btype, "size": size}
        if btype != "JFVB" and size >= 0xC:  # TBlock_object; JFVB holds function values
            id_size = u16(b, o + 0xA)
            blk["id"] = cstr(b, o + 0xC, id_size) if id_size else ""
        rec["blocks"].append(blk)
        o += size
    return rec


ACTOR_TAGS_20 = {"ACTR", "TGOB", "TRES", "PLYR"} | {"ACT" + c for c in "0123456789ab"} | \
                {"TRE" + c for c in "0123456789ab"}
ACTOR_TAGS_24 = {"SCOB", "TGSC", "DOOR", "TGDR"} | {"SCO" + c for c in "0123456789ab"}


def parse_stage(b):
    need(len(b) >= 4, "dzs/dzr: short")
    n = s32(b, 0)
    need(0 <= n < 256 and 4 + n * 12 <= len(b), "dzs/dzr: chunk count %d" % n)
    rec = {"chunk_count": n, "chunks": [], "actors": {}}
    for i in range(n):
        o = 4 + i * 12
        tag = tag4(b, o)
        num = s32(b, o + 4)
        off = u32(b, o + 8)
        need(num >= 0 and off <= len(b), "dzs/dzr: chunk %s num %d offset 0x%x" % (tag, num, off))
        rec["chunks"].append({"tag": tag, "num": num, "offset": off})
        esize = 0x20 if tag in ACTOR_TAGS_20 else 0x24 if tag in ACTOR_TAGS_24 else 0
        if esize:
            need(off + num * esize <= len(b), "dzs/dzr: %s entries past the end" % tag)
            actors = []
            for k in range(num):
                e = off + k * esize
                actors.append({"name": cstr(b, e, 8), "params": u32(b, e + 8),
                               "pos": [round(v, 3) for v in struct.unpack_from(">3f", b, e + 0xC)],
                               "angle": list(struct.unpack_from(">3h", b, e + 0x18)),
                               "set_id": u16(b, e + 0x1E)})
            rec["actors"][tag] = actors
    return rec


def parse_dzb(b):
    need(len(b) >= 0x34, "dzb: short")
    f = struct.unpack_from(">iIiIiIiIiIiII", b, 0)
    rec = {"vertices": f[0], "triangles": f[2], "blocks": f[4], "tree_nodes": f[6], "groups": f[8],
           "infos": f[10], "flag": f[12],
           "offsets": {"vtx": f[1], "tri": f[3], "blk": f[5], "tree": f[7], "grp": f[9], "ti": f[11]}}
    nv, vo = f[0], f[1]
    need(nv >= 0 and vo + nv * 12 <= len(b), "dzb: vertex table past the end")
    if nv:
        v = struct.unpack_from(">%df" % (nv * 3), b, vo)
        xs, ys, zs = v[0::3], v[1::3], v[2::3]
        rec["bbox"] = {"min": [round(min(xs), 3), round(min(ys), 3), round(min(zs), 3)],
                       "max": [round(max(xs), 3), round(max(ys), 3), round(max(zs), 3)]}
    need(f[2] >= 0 and f[3] + f[2] * 0xA <= len(b), "dzb: triangle table past the end")
    return rec


def parse_aaf(b):
    """JaiInit.aaf as JAInter::InitData::checkInitDataOnMemory reads it."""
    words = len(b) // 4
    w = lambda i: u32(b, i * 4)
    sections = []
    i = 0
    while True:
        need(i < words, "aaf: no terminating 0")
        t = w(i)
        i += 1
        if t == 0:
            break
        if t in (1, 4, 5, 6, 7, 8):
            sections.append({"type": t, "offset": w(i), "size": w(i + 1), "flags": w(i + 2)})
            i += 3
        elif t in (2, 3):
            entries = []
            while w(i) != 0:
                entries.append({"offset": w(i), "size": w(i + 1), "flags": w(i + 2)})
                i += 3
            i += 1
            sections.append({"type": t, "count": len(entries), "entries": entries})
        else:
            start = i
            while w(i) != 0:
                i += 1
            i += 1
            sections.append({"type": t, "words": i - start})
    return {"sections": sections}


JUT_MAGICS = {b"J3D1": "j3d", b"J3D2": "j3d"}
JUT_MAGICS8 = {b"MESGbmg1": "bmg", b"MGCLbmc1": "bmc", b"FONTbfn1": "bfn", b"SCRNblo1": "blo"}


def classify(name, b):
    lname = name.lower()
    head = bytes(b[:8])
    if head[:4] == b"RARC":
        return "rarc"
    if head[:4] in JUT_MAGICS:
        return "j3d"
    if head in JUT_MAGICS8:
        return JUT_MAGICS8[head]
    if head == b"JPAC1-00":
        return "jpc"
    if head[:4] == b"STB\0":
        return "stb"
    ext = lname.rsplit(".", 1)[-1] if "." in lname else ""
    if ext == "bti":
        return "bti"
    if ext in ("dzs", "dzr"):
        return ext
    if ext == "dzb":
        return "dzb"
    if ext == "aaf":
        return "aaf"
    return None


def parse_content(name, b, rec, errors, where):
    """Fills rec with format and header fields of b; parse errors are recorded, not raised."""
    fmt = classify(name, b)
    if fmt is None:
        return
    rec["format"] = fmt
    try:
        if fmt == "rarc":
            fields = parse_rarc(b, errors, where)
        elif fmt in ("j3d", "bmg", "bmc", "bfn", "blo"):
            fields = parse_jut_file(b, fmt)
        elif fmt == "jpc":
            fields = parse_jpc(b)
        elif fmt == "stb":
            fields = parse_stb(b)
        elif fmt == "bti":
            fields = parse_bti(b)
        elif fmt in ("dzs", "dzr"):
            fields = parse_stage(b)
        elif fmt == "dzb":
            fields = parse_dzb(b)
        elif fmt == "aaf":
            fields = parse_aaf(b)
        else:
            fields = {}
        # The size a format's header gives is header_size: "size" stays the stored size of the
        # file (FST or archive entry), which the cross-checks compare.
        if "size" in fields:
            fields["header_size"] = fields.pop("size")
        rec.update(fields)
    except (ParseError, struct.error, IndexError) as e:
        rec["error"] = str(e) or type(e).__name__
        errors.append("%s: %s" % (where, rec["error"]))


def parse_rarc(b, errors, where):
    need(len(b) >= 0x40, "RARC: short")
    file_length = u32(b, 4)
    header_length = u32(b, 8)
    data_offset = u32(b, 0xC)
    need(header_length == 0x20, "RARC: header length 0x%x" % header_length)
    info = 0x20
    num_nodes = u32(b, info + 0x00)
    node_off = info + u32(b, info + 0x04)
    num_entries = u32(b, info + 0x08)
    entry_off = info + u32(b, info + 0x0C)
    str_off = info + u32(b, info + 0x14)
    data_base = header_length + data_offset
    need(node_off + num_nodes * 0x10 <= len(b) and entry_off + num_entries * 0x14 <= len(b),
         "RARC: tables past the end")
    nodes = []
    for i in range(num_nodes):
        o = node_off + i * 0x10
        nodes.append({"type": tag4(b, o), "name": cstr(b, str_off + u32(b, o + 4)),
                      "hash": u16(b, o + 8), "entries": u16(b, o + 0xA), "first": u32(b, o + 0xC)})
    # Paths: walk from the root node; a directory entry (flags & 2) other than "." and ".." points
    # at a node by its data_offset.
    entries = []
    for i in range(num_entries):
        o = entry_off + i * 0x14
        tf = u32(b, o + 4)
        entries.append({"id": u16(b, o), "hash": u16(b, o + 2), "flags": tf >> 24,
                        "name": cstr(b, str_off + (tf & 0xFFFFFF)), "data_offset": u32(b, o + 8),
                        "size": u32(b, o + 0xC)})
    files = []
    dirs = 0

    def walk(node_index, prefix, depth):
        nonlocal dirs
        need(depth < 32 and node_index < num_nodes, "RARC: bad node %d" % node_index)
        node = nodes[node_index]
        for k in range(node["first"], node["first"] + node["entries"]):
            need(k < num_entries, "RARC: entry %d out of range" % k)
            e = entries[k]
            if e["flags"] & 0x02:
                if e["name"] in (".", ".."):
                    continue
                dirs += 1
                walk(e["data_offset"], prefix + e["name"] + "/", depth + 1)
                continue
            path = prefix + e["name"]
            rec = {"path": path, "id": e["id"], "flags": e["flags"], "size": e["size"],
                   "offset": e["data_offset"]}
            start = data_base + e["data_offset"]
            need(start + e["size"] <= len(b), "RARC: %s data past the end" % path)
            data = b[start:start + e["size"]]
            sub_where = "%s:%s" % (where, path)
            if data[:4] == b"Yaz0":
                try:
                    data = yaz0_decompress(data)
                    rec["yaz0"] = {"size": len(data)}
                except ParseError as ex:
                    rec["error"] = str(ex)
                    errors.append("%s: %s" % (sub_where, ex))
                    files.append(rec)
                    continue
            parse_content(e["name"], data, rec, errors, sub_where)
            files.append(rec)

    if num_nodes:
        walk(0, "", 0)
    return {"file_length": file_length, "node_count": num_nodes, "entries": num_entries,
            "nodes": [{"type": n["type"], "name": n["name"], "entries": n["entries"],
                       "first": n["first"]} for n in nodes],
            "dir_count": dirs, "file_count": len(files), "files": files}


# ---- FST and manifest ---------------------------------------------------------------------------

def read_fst(f):
    f.seek(0)
    head = f.read(0x440)
    fst_off = u32(head, 0x424)
    fst_size = u32(head, 0x428)
    f.seek(fst_off)
    fst = f.read(fst_size)
    n = u32(fst, 8)
    strtab = n * 12
    entries = [None] * n
    entries[0] = {"dir": True, "name": "", "parent": 0, "next": n}

    paths = [""] * n
    # Directories: parent in word 1, next (one past the last child) in word 2.
    stack = [(0, n)]  # (dir entry, end)
    for i in range(1, n):
        while stack and i >= stack[-1][1]:
            stack.pop()
        w0, a, c = struct.unpack_from(">III", fst, i * 12)
        is_dir = (w0 >> 24) != 0
        name = cstr(fst, strtab + (w0 & 0xFFFFFF))
        parent = stack[-1][0]
        paths[i] = paths[parent] + "/" + name
        if is_dir:
            entries[i] = {"dir": True, "name": name, "parent": a, "next": c}
            stack.append((i, c))
        else:
            entries[i] = {"dir": False, "name": name, "offset": a, "size": c}
    paths[0] = "/"
    return entries, paths


def build_manifest(disc, iso_sha, dsha, progress=True):
    errors = []
    with open(disc, "rb") as f:
        head = f.read(0x20)
        entries, paths = read_fst(f)
        files = []
        n_files = sum(1 for e in entries[1:] if not e["dir"])
        done = 0
        for i, e in enumerate(entries):
            if i == 0 or e["dir"]:
                continue
            rec = {"path": paths[i], "entry": i, "offset": e["offset"], "size": e["size"]}
            name = e["name"]
            lname = name.lower()
            # Bulk media and code: size only (the THP movies alone are 600 MiB).
            if lname.endswith((".thp", ".afc", ".aw", ".map", ".dds")):
                rec["format"] = lname.rsplit(".", 1)[-1]
                files.append(rec)
                continue
            f.seek(e["offset"])
            data = f.read(e["size"])
            if data[:4] == b"Yaz0":
                if lname.endswith(".rel"):
                    rec["yaz0"] = {"size": u32(data, 4)}
                    rec["format"] = "rel"
                    files.append(rec)
                    continue
                try:
                    data = yaz0_decompress(data)
                    rec["yaz0"] = {"size": len(data)}
                except ParseError as ex:
                    rec["error"] = str(ex)
                    errors.append("%s: %s" % (paths[i], ex))
                    files.append(rec)
                    continue
            parse_content(name, data, rec, errors, paths[i])
            files.append(rec)
            done += 1
            if progress and done % 200 == 0:
                print("disc_manifest: %d/%d files" % (len(files), n_files), file=sys.stderr)

    fmt_disc = Counter(r.get("format", "other") for r in files)
    fmt_arc = Counter()
    arc_files = 0

    def count_arc(r):
        nonlocal arc_files
        for af in r.get("files", []):
            arc_files += 1
            fmt_arc[af.get("format", "other")] += 1
            if af.get("format") == "rarc":
                count_arc(af)

    for r in files:
        if r.get("format") == "rarc":
            count_arc(r)
    manifest = {
        "tool": "native/tools/disc_manifest.py",
        "manifest_version": MANIFEST_VERSION,
        "disc": {"path": disc, "size": os.path.getsize(disc), "sha1": iso_sha, "dol_sha1": dsha,
                 "game_id": head[0:6].decode("latin-1"), "revision": head[7]},
        "fst": {"entries": len(entries), "files": n_files,
                "dirs": sum(1 for e in entries[1:] if e["dir"])},
        "summary": {"disc_formats": dict(sorted(fmt_disc.items())),
                    "archive_files": arc_files, "archive_formats": dict(sorted(fmt_arc.items())),
                    "errors": len(errors), "error_list": errors},
        "files": files,
    }
    return manifest


# ---- disc-ls cross-check ------------------------------------------------------------------------

def check_ls(manifest, ls_path):
    """disc_ls.txt lines: 'F <entry> <size> <path>' and 'D <entry> <path>'."""
    want_files = {r["entry"]: (r["path"], r["size"]) for r in manifest["files"]}
    got_files = {}
    got_dirs = 0
    problems = []
    with open(ls_path, encoding="utf-8", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ", 3)
            if parts[0] == "D" and len(parts) >= 3:
                got_dirs += 1
            elif parts[0] == "F" and len(parts) == 4:
                entry, size, path = int(parts[1]), int(parts[2]), parts[3]
                if entry in got_files:
                    problems.append("line %d: entry %d listed twice" % (ln, entry))
                got_files[entry] = (path, size)
            else:
                problems.append("line %d: malformed: %r" % (ln, line))
    fst = manifest["fst"]
    print("disc_manifest: FST %d files, %d dirs; DVDReadDir %d files, %d dirs"
          % (fst["files"], fst["dirs"], len(got_files), got_dirs))
    if len(got_files) != fst["files"]:
        problems.append("file count %d, the FST has %d" % (len(got_files), fst["files"]))
    if got_dirs != fst["dirs"]:
        problems.append("directory count %d, the FST has %d" % (got_dirs, fst["dirs"]))
    for entry, (path, size) in sorted(want_files.items()):
        got = got_files.get(entry)
        if got is None:
            problems.append("entry %d %s not listed" % (entry, path))
        elif got[0] != path or got[1] != size:
            problems.append("entry %d: listed %s (%d bytes), the FST has %s (%d bytes)"
                            % (entry, got[0], got[1], path, size))
    for entry in sorted(set(got_files) - set(want_files)):
        problems.append("entry %d %s is not a file of the FST" % (entry, got_files[entry][0]))
    for p in problems[:40]:
        print("disc_manifest: DIFF " + p)
    if len(problems) > 40:
        print("disc_manifest: ... %d differences in all" % len(problems))
    if problems:
        return EXIT_DIFFERENT
    print("disc_manifest: disc-ls equals the FST")
    return EXIT_OK


# ---- font (step 4.3) and message (step 4.6) cross-checks ---------------------------------------

def records_by_path(manifest):
    """Every file record by path: disc files by their FST path, files inside archives as
    '<archive path>:<path inside>' (nested archives repeat the ':')."""
    out = {}

    def add(name, rec):
        out[name] = rec
        for f in rec.get("files", []):
            add(name + ":" + f["path"], f)

    for r in manifest["files"]:
        add(r["path"], r)
    return out


def check_jut_lines(manifest, report_path, file_kinds, problems):
    """Lines '<KIND> <path> block_count=N <TAG>=count ...' (KIND a key of file_kinds, naming the
    record's format) and '<TAG> <path> <index among that tag> key=value ...', the keys being the
    manifest's. Returns ({KIND: files}, block headers compared); differences go to problems."""
    by_path = records_by_path(manifest)
    files = Counter()
    blocks_checked = 0
    with open(report_path, encoding="utf-8", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 3:
                problems.append("line %d: malformed: %r" % (ln, line))
                continue
            kind, path = parts[0], parts[1]
            rec = by_path.get(path)
            formats = set(file_kinds.values())
            if rec is None or rec.get("format") not in formats or (
                    kind in file_kinds and rec.get("format") != file_kinds[kind]):
                problems.append("line %d: %s is not a %s file of the manifest"
                                % (ln, path, "/".join(sorted(formats)).upper()))
                continue
            blocks = rec.get("blocks", [])
            if kind in file_kinds:
                files[kind] += 1
                want = {"block_count": rec.get("block_count")}
                for b in blocks:
                    want[b["tag"]] = want.get(b["tag"], 0) + 1
                fields = parts[2:]
                target = want
            else:
                same = [b for b in blocks if b["tag"] == kind]
                try:
                    index = int(parts[2])
                except ValueError:
                    problems.append("line %d: malformed index: %r" % (ln, line))
                    continue
                if index >= len(same):
                    problems.append("line %d: %s %s #%d: the file has %d" % (ln, path, kind, index,
                                                                             len(same)))
                    continue
                target = same[index]
                fields = parts[3:]
                blocks_checked += 1
            for field in fields:
                key, _, value = field.partition("=")
                if key not in target:
                    if kind in file_kinds and value == "0":
                        continue  # a block kind the file does not have
                    problems.append("line %d: %s %s: no field %s in the manifest" % (ln, path, kind,
                                                                                    key))
                elif str(target[key]) != value:
                    problems.append("line %d: %s %s %s=%s, the manifest has %s"
                                    % (ln, path, kind, key, value, target[key]))
    return files, blocks_checked


def report_problems(problems, ok_text):
    for p in problems[:40]:
        print("disc_manifest: DIFF " + p)
    if len(problems) > 40:
        print("disc_manifest: ... %d differences in all" % len(problems))
    if problems:
        return EXIT_DIFFERENT
    print("disc_manifest: " + ok_text)
    return EXIT_OK


def check_font(manifest, font_path):
    """font.txt lines: 'FONT <path> block_count=N INF1=1 WID1=n MAP1=n GLY1=n' and
    '<TAG> <path> <index among that tag> key=value ...', the keys being the manifest's."""
    problems = []
    files, blocks_checked = check_jut_lines(manifest, font_path, {"FONT": "bfn"}, problems)
    print("disc_manifest: font.txt: %d font(s), %d block header(s) compared" % (files["FONT"],
                                                                                 blocks_checked))
    if files["FONT"] == 0:
        problems.append("no FONT line")
    return report_problems(problems, "font.txt equals the manifest")


def check_msg(manifest, msg_path):
    """msg_sweep.txt: what COS_SMOKE=msg-sweep read through the game's code, in the font.txt
    syntax: 'BMG <path> block_count=N INF1=1 DAT1=1' with 'INF1 <path> 0 entries= entry_size=
    group= messages= distinct_ids= ids_fnv=' and 'DAT1 <path> 0 size=', 'BMC <path> ...' with
    'CLT1 <path> 0 entries= colors_fnv=', and 'FONT <path> ...' with its blocks for the message
    fonts. Every BMG of the disc must be there."""
    problems = []
    files, blocks_checked = check_jut_lines(
        manifest, msg_path, {"BMG": "bmg", "BMC": "bmc", "FONT": "bfn"}, problems)
    all_bmg = [p for p, r in records_by_path(manifest).items() if r.get("format") == "bmg"]
    messages = sum(b.get("messages", 0) for p in all_bmg
                   for b in records_by_path(manifest)[p].get("blocks", []) if b["tag"] == "INF1")
    print("disc_manifest: msg_sweep.txt: %d BMG (the disc has %d, %d messages), %d BMC, %d font(s), "
          "%d block header(s) compared" % (files["BMG"], len(all_bmg), messages, files["BMC"],
                                           files["FONT"], blocks_checked))
    if files["BMG"] != len(all_bmg):
        problems.append("%d BMG files reported, the disc has %d" % (files["BMG"], len(all_bmg)))
    if files["FONT"] == 0:
        problems.append("no FONT line")
    return report_problems(problems, "msg_sweep.txt equals the manifest")


def check_jpa(manifest, jpa_path):
    """jpa_sweep.txt lines (fields separated by single spaces): 'JPC <path> emitter_count=N
    texture_count=N', 'EMTR <path> <index> res_id= blocks= keys= fields= textures= tags=A,B,...'
    and 'TEX <path> <index> name= <ResTIMG field>=...', the keys being the manifest's (tags joined
    by commas). Every JPC of the disc, every emitter and every texture must be there."""
    by_path = records_by_path(manifest)
    all_jpc = {p: r for p, r in by_path.items() if r.get("format") == "jpc"}
    problems = []
    seen = {}
    emitters = textures = 0
    with open(jpa_path, encoding="utf-8", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            if len(parts) < 3 or parts[0] not in ("JPC", "EMTR", "TEX"):
                problems.append("line %d: malformed: %r" % (ln, line))
                continue
            kind, path = parts[0], parts[1]
            rec = all_jpc.get(path)
            if rec is None:
                problems.append("line %d: %s is not a JPC file of the manifest" % (ln, path))
                continue
            got = seen.setdefault(path, {"JPC": 0, "EMTR": set(), "TEX": set()})
            if kind == "JPC":
                got["JPC"] += 1
                target, fields = rec, parts[2:]
            else:
                try:
                    index = int(parts[2])
                except ValueError:
                    problems.append("line %d: malformed index: %r" % (ln, line))
                    continue
                items = rec["emitters"] if kind == "EMTR" else rec["textures"]
                if index >= len(items):
                    problems.append("line %d: %s %s #%d: the file has %d" % (ln, path, kind, index,
                                                                             len(items)))
                    continue
                got[kind].add(index)
                target, fields = items[index], parts[3:]
                if kind == "EMTR":
                    emitters += 1
                else:
                    textures += 1
            for field in fields:
                key, _, value = field.partition("=")
                if key not in target:
                    problems.append("line %d: %s %s: no field %s in the manifest" % (ln, path, kind,
                                                                                    key))
                    continue
                want = target[key]
                if isinstance(want, list):
                    want = ",".join(str(x) for x in want)
                if str(want) != value:
                    problems.append("line %d: %s %s %s=%s, the manifest has %s"
                                    % (ln, path, kind, key, value, want))
    for path, rec in sorted(all_jpc.items()):
        got = seen.get(path)
        if got is None or got["JPC"] != 1:
            problems.append("%s: %d JPC lines" % (path, 0 if got is None else got["JPC"]))
            continue
        if len(got["EMTR"]) != len(rec["emitters"]):
            problems.append("%s: %d of %d emitters reported" % (path, len(got["EMTR"]),
                                                                len(rec["emitters"])))
        if len(got["TEX"]) != len(rec["textures"]):
            problems.append("%s: %d of %d textures reported" % (path, len(got["TEX"]),
                                                                len(rec["textures"])))
    print("disc_manifest: jpa_sweep.txt: %d JPC (the disc has %d), %d emitters, %d textures "
          "compared" % (len(seen), len(all_jpc), emitters, textures))
    return report_problems(problems, "jpa_sweep.txt equals the manifest")


# ---- archive cross-check (step 4.4) -------------------------------------------------------------

def check_arc(manifest, arc_path):
    """arc_sweep.txt lines (fields separated by single spaces; names have none):
    'ARC <name> nodes=N entries=N files=N dirs=N', 'NODE <name> <index> type=<hex> name=<s>
    entries=N first=N' and 'FILE <name> <path> id=N flags=N size=N offset=N [expanded=N]', where
    <name> is the disc path of the archive, or '<disc path>:<path inside>' for a nested one. Names
    and paths are percent-encoded (a space, '%' and bytes outside printable ASCII as %XX) and
    decoded as cstr() decodes the archive's strings."""
    from urllib.parse import unquote_to_bytes

    def dec(text):
        raw = unquote_to_bytes(text)
        try:
            return raw.decode("shift_jis")
        except UnicodeDecodeError:
            return raw.decode("latin-1")

    want = {}

    def add(name, rec):
        want[name] = rec
        for f in rec.get("files", []):
            if f.get("format") == "rarc":
                add(name + ":" + f["path"], f)

    for r in manifest["files"]:
        if r.get("format") == "rarc":
            add(r["path"], r)
    got = {}
    problems = []
    with open(arc_path, encoding="ascii", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            kind = parts[0]
            if kind not in ("ARC", "NODE", "FILE") or len(parts) < 3:
                problems.append("line %d: malformed: %r" % (ln, line))
                continue
            name = dec(parts[1])
            if kind == "ARC":
                if name in got:
                    problems.append("line %d: %s reported twice" % (ln, name))
                got[name] = {"fields": dict(p.partition("=")[::2] for p in parts[2:]),
                             "nodes": {}, "files": {}}
                continue
            arc = got.get(name)
            if arc is None:
                problems.append("line %d: %s before its ARC line" % (ln, name))
                continue
            key = dec(parts[2])
            fields = dict(p.partition("=")[::2] for p in parts[3:])
            if "name" in fields:
                fields["name"] = dec(fields["name"])
            if kind == "NODE":
                arc["nodes"][int(key)] = fields
            else:
                if key in arc["files"]:
                    problems.append("line %d: %s %s reported twice" % (ln, name, key))
                arc["files"][key] = fields

    files_checked = 0
    for name in sorted(set(want) - set(got)):
        problems.append("%s: archive of the manifest not swept" % name)
    for name in sorted(set(got) - set(want)):
        problems.append("%s: not a RARC archive of the manifest" % name)
    for name in sorted(set(want) & set(got)):
        rec, arc = want[name], got[name]
        expect = {"nodes": rec["node_count"], "entries": rec["entries"],
                  "files": rec["file_count"], "dirs": rec["dir_count"]}
        for k, v in expect.items():
            if arc["fields"].get(k) != str(v):
                problems.append("%s: %s=%s, the manifest has %s" % (name, k, arc["fields"].get(k),
                                                                    v))
        for i, node in enumerate(rec["nodes"]):
            g = arc["nodes"].get(i)
            if g is None:
                problems.append("%s: node %d not reported" % (name, i))
                continue
            w = {"type": node["type"].encode("latin-1").hex(), "name": node["name"],
                 "entries": str(node["entries"]), "first": str(node["first"])}
            for k, v in w.items():
                if g.get(k) != v:
                    problems.append("%s: node %d %s=%s, the manifest has %s" % (name, i, k,
                                                                              g.get(k), v))
        if len(arc["nodes"]) != len(rec["nodes"]):
            problems.append("%s: %d nodes reported, the manifest has %d"
                            % (name, len(arc["nodes"]), len(rec["nodes"])))
        for fr in rec["files"]:
            g = arc["files"].get(fr["path"])
            if g is None:
                problems.append("%s: %s not reported" % (name, fr["path"]))
                continue
            files_checked += 1
            w = {"id": fr["id"], "flags": fr["flags"], "size": fr["size"], "offset": fr["offset"]}
            if fr["flags"] & 0x04:
                w["expanded"] = fr.get("yaz0", {}).get("size")
            for k, v in w.items():
                if g.get(k) != str(v):
                    problems.append("%s: %s %s=%s, the manifest has %s" % (name, fr["path"], k,
                                                                           g.get(k), v))
            for k in set(g) - set(w):
                problems.append("%s: %s has an unexpected field %s" % (name, fr["path"], k))
        if len(arc["files"]) != len(rec["files"]):
            problems.append("%s: %d files reported, the manifest has %d"
                            % (name, len(arc["files"]), len(rec["files"])))
    print("disc_manifest: arc_sweep.txt: %d archive(s), %d file(s) compared (the manifest has %d "
          "archives)" % (len(got), files_checked, len(want)))
    if not got:
        problems.append("no ARC line")
    for p in problems[:40]:
        print("disc_manifest: DIFF " + p)
    if len(problems) > 40:
        print("disc_manifest: ... %d differences in all" % len(problems))
    if problems:
        return EXIT_DIFFERENT
    print("disc_manifest: arc_sweep.txt equals the manifest")
    return EXIT_OK


def load_manifest(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except OSError as e:
        print("disc_manifest: cannot read %s: %s (run disc_manifest.py first)" % (path, e),
              file=sys.stderr)
        sys.exit(EXIT_USAGE)


def print_summary(m):
    s = m["summary"]
    print("disc_manifest: %s %s rev %d, sha1 %s" % (m["disc"]["path"], m["disc"]["game_id"],
                                                   m["disc"]["revision"], m["disc"]["sha1"]))
    print("disc_manifest: FST %d entries: %d files, %d dirs" % (m["fst"]["entries"],
                                                                m["fst"]["files"], m["fst"]["dirs"]))
    print("disc_manifest: disc formats: " +
          ", ".join("%s %d" % kv for kv in s["disc_formats"].items()))
    print("disc_manifest: %d files inside archives: " % s["archive_files"] +
          ", ".join("%s %d" % kv for kv in s["archive_formats"].items()))
    print("disc_manifest: %d parse error(s)" % s["errors"])
    for e in s["error_list"][:20]:
        print("disc_manifest: ERROR " + e)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--disc", default=os.environ.get("COS_DISC", DEFAULT_DISC))
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--verify", action="store_true", help="only check the SHA-1 of the disc")
    ap.add_argument("--check-ls", metavar="LS", help="compare a disc-ls listing with the manifest")
    ap.add_argument("--check-font", metavar="FONT",
                    help="compare a COS_SMOKE=font report with the manifest")
    ap.add_argument("--check-msg", metavar="MSG",
                    help="compare a COS_SMOKE=msg-sweep report with the manifest")
    ap.add_argument("--check-arc", metavar="ARC",
                    help="compare a COS_SMOKE=arc-sweep report with the manifest")
    ap.add_argument("--check-jpa", metavar="JPA",
                    help="compare a COS_SMOKE=jpa-sweep report with the manifest")
    ap.add_argument("--summary", action="store_true", help="print an existing manifest's counts")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if args.check_ls:
        return check_ls(load_manifest(args.out), args.check_ls)
    if args.check_font:
        return check_font(load_manifest(args.out), args.check_font)
    if args.check_arc:
        return check_arc(load_manifest(args.out), args.check_arc)
    if args.check_msg:
        return check_msg(load_manifest(args.out), args.check_msg)
    if args.check_jpa:
        return check_jpa(load_manifest(args.out), args.check_jpa)
    if args.summary:
        print_summary(load_manifest(args.out))
        return EXIT_OK

    ok, iso_sha, dsha = verify_disc(args.disc, args.quiet)
    if not ok:
        return EXIT_DISC
    if args.verify:
        if not args.quiet:
            print("disc_manifest: %s is GZLE01 revision 0 (SHA-1 %s)" % (args.disc, iso_sha))
        return EXIT_OK

    m = build_manifest(args.disc, iso_sha, dsha, progress=not args.quiet)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    tmp = args.out + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(m, f, indent=1, sort_keys=False)
        f.write("\n")
    os.replace(tmp, args.out)
    print_summary(m)
    print("disc_manifest: wrote %s" % args.out)
    return EXIT_OK if m["summary"]["errors"] == 0 else EXIT_DIFFERENT


if __name__ == "__main__":
    sys.exit(main())
