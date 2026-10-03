#!/usr/bin/env python3
"""Disc oracle: a manifest of the GZLE01 disc (docs/NATIVE_PORT_PHASE4_6.md, step 4.0d).

The phase 4 format steps check what the native game reads against an independent reading of the
same disc. This script is that reading, in pure Python (standard library only): the GameCube FST,
Yaz0, RARC archives, and the header fields of BMD/BDL/BMT, the J3D animations (BCK/BCA/BTK/BTP/
BRK/BPK/BVA/BLA/BLK), BTI, BFN, BMG, BMC, BLO, JPC, STB, dzs/dzr, dzb, AAF and AFC. It writes
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
  disc_manifest.py --check-stage STG [--out FILE] compare what the game's stage code read in
                                                  COS_SMOKE=stage-sweep (<run dir>/
                                                  stage_sweep.txt) with the manifest's dzs/dzr
                                                  files: every file's chunk count, every chunk's
                                                  tag, entry count and offset, every actor
                                                  record's name, parameters, position, angle and
                                                  set id, and every field of the RTBL, STAG,
                                                  FILI, MULT, SCLS, PATH/PPNT, RPAT/RPPN,
                                                  CAMR/RCAM, AROB/RARO, EVNT, 2DMA and SOND
                                                  records (exit 0 equal, 1 different)
  disc_manifest.py --check-blo BLO [--out FILE]  compare the panes J2DScreen built in COS_SMOKE=
                                                  blo-sweep (<run dir>/blo_sweep.txt) with the
                                                  manifest's BLO files: every file of the disc
                                                  (nested archives included) and its PAN1, PIC1,
                                                  WIN1 and TBX1 counts (exit 0 equal, 1 different)
  disc_manifest.py --check-dzb DZB [--out FILE]  compare what the game's collision code read in
                                                  COS_SMOKE=dzb-sweep (<run dir>/dzb_sweep.txt)
                                                  with the manifest's dzb files: every file's
                                                  table counts and offsets, flag and vertex
                                                  bounding box (exit 0 equal, 1 different)
  disc_manifest.py --check-audio AUD [--out FILE] compare what JAudio read in COS_SMOKE=
                                                  audio-parse (<run dir>/audio_parse.txt) with the
                                                  manifest's JaiInit.aaf records: the sound table,
                                                  every bank (instruments, oscillators, key and
                                                  velocity regions, drum sets), wave system (groups,
                                                  .aw files, waves), sequence (JaiSeqs.arc sizes),
                                                  stream (table and .afc headers), scene and fx
                                                  scene line (exit 0 equal, 1 different)
  disc_manifest.py --check-j3d J3D [--out FILE]  compare what the game's J3D loaders built in
                                                  COS_SMOKE=j3d-sweep (<run dir>/j3d_sweep.txt)
                                                  with the manifest's BMD/BDL/BMT files: every
                                                  file, its INF1 flags and vertex count, its
                                                  joint, material, shape, texture, draw and
                                                  envelope counts and its joint, material and
                                                  texture names (exit 0 equal, 1 different)
  disc_manifest.py --check-anm ANM [--out FILE]  compare what the game's J3D animation code gave
                                                  in COS_SMOKE=anm-sweep (<run dir>/
                                                  anm_sweep.txt) with the manifest's J3D1 files:
                                                  every file, its block tag, attribute, frame
                                                  count, track and name counts, and the values
                                                  its getters return at the first, middle and
                                                  last frame (count, sum, sum of |v|, max |v|;
                                                  exit 0 equal, 1 different)
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
              J3D1 animation blocks add attribute, frame_max, count, names (cnames/knames)
              and eval {family: [n, sum, sum |v|, max |v|, tolerance]} (step 4.12);
              BMG INF1 adds messages (entries with text), distinct_ids and ids_fnv (FNV-1a 64
              of every entry's big-endian u32 offset and u16 number), BMC CLT1 entries and
              colors_fnv (FNV-1a 64 of the colour table)
              bti: the ResTIMG header;  jpc: emitters [{res_id, blocks, keys, fields, textures,
              tags}], textures [names];  stb: version, blocks [{type, id}];  dzs/dzr: chunks
              [{tag, num}], actors {tag: [{name, params, pos, angle, set_id}]} and records
              {chunk index: {tag, entries}} (RTBL and the STAGE_RECORDS tags);  dzb: counts
              and the vertex bounding box;  aaf: sections [{type, offset, size, count}] and
              audio {sound_table, banks, wave_systems, streams, scenes, fx} (step 5.1);  afc:
              the first 16 bytes of the stream header
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
# 4: dzs/dzr records of the room, file and path chunks (step 4.9c).
# 5: dzs/dzr records of the environment chunks (step 4.9d).
# 6: JaiInit.aaf's audio records and the .afc headers (step 5.1).
# 7: J3D animation blocks: track counts, update-material names and values evaluated at the first,
#    middle and last frame (step 4.12).
MANIFEST_VERSION = 7

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
        if tag in J3D_ANM_EVAL:
            info.update(anm_eval(b, o, tag, info["frame_max"]))
    return info


# J3D animations evaluated as J3DAnimation.cpp does (step 4.12), from plain big-endian reads of the
# block (offsets of J3DAnimation.h's J3DAnm*Data, relative to the block). Each "family" of values
# the game's getters return at the frames 0, frame_max / 2 and frame_max is summarised as
# [count, sum, sum of |v|, max |v|, tolerance]: f32 arithmetic is emulated (to_f32 after every
# operation) but the game's compiler may fuse a multiply-add, so a value truncated to an integer
# within 1e-3 of an integer boundary adds its step (1 << shift for rotations) to the tolerance.

def anm_hermite(t, t0, v0, d0, t1, v1, d1):
    """JMAHermiteInterpolation (JMath.cpp), f32 operations in its order."""
    F = to_f32
    length = F(t1 - t0)
    f9 = F(t - t0)
    f1 = F(1.0 / length)
    f2 = F(F(f9 * f9) * f1)
    f10 = F(f2 * f1)
    f11 = F(f9 * f10)
    f12 = F(f11 * f1)
    a = F(v0 * F(1.0 + F(F(2.0 * f12) - F(3.0 * f10))))
    b2 = F(v1 * F(F(-2.0 * f12) + F(3.0 * f10)))
    c = F(d0 * F(f9 + F(f11 - F(2.0 * f2))))
    d = F(d1 * F(f11 - f2))
    return F(F(F(a + b2) + c) + d)


def anm_hermite_s(t, t0, v0, d0, t1, v1, d1):
    """J3DHermiteInterpolationS (J3DAnimation.cpp): the paired-single sequence, s16 keys."""
    F = to_f32
    rng = F(t1 - t0)
    u = F(F(t - t0) / rng)
    dv = F(v1 - v0)
    a = F(F(d1 * rng) + v0)
    u2 = F(u * u)
    bb = F(dv - F(rng * d0))
    a = F(F(a - v1) - bb)
    c = F(u2 * a)
    out = F(F(rng * d0) + c)
    out = F(F(out * u) + v0)
    out = F(F(bb * u2) + out)
    return F(out - c)


def anm_key(b, pos, size, fmt, frame, max_frame, ktype, hermite):
    """J3DGetKeyFrameInterpolation / J3DGetKeyFrameInterpolationS: keys of 3 (time, value,
    tangent) or 4 (time, value, in, out) values from pos."""
    def d(i):
        return struct.unpack_from(fmt, b, pos + i * size)[0]
    stride = 3 if ktype == 0 else 4
    if frame < d(0):
        return float(d(1))
    if d(stride * (max_frame - 1)) <= frame:
        return float(d(stride * (max_frame - 1) + 1))
    base = 0
    num = max_frame
    while num > 1:
        mid = num // 2
        if frame >= d(base + stride * mid):
            base += stride * mid
            num -= mid
        else:
            num = mid
    k = [d(base + i) for i in range(7)]
    if stride == 3:
        return hermite(frame, k[0], k[1], k[2], k[3], k[4], k[5])
    return hermite(frame, k[0], k[1], k[3], k[4], k[5], k[6])


class AnmFamily:
    def __init__(self):
        self.n = 0
        self.sum = 0.0
        self.abs = 0.0
        self.max = 0.0
        self.tol = 0.0

    def add(self, v, step=0.0):
        self.n += 1
        self.sum += v
        self.abs += abs(v)
        self.max = max(self.max, abs(v))
        self.tol += step

    def record(self):
        return [self.n, self.sum, self.abs, self.max, self.tol]


def anm_trunc(v, step=1):
    """C's float-to-integer truncation, with the tolerance step when v is near a boundary."""
    near = abs(v - round(v)) < 1e-3 * max(1.0, abs(v))
    return int(v), (step if near else 0)


def wrap_s16(x):
    return ((x + 0x8000) & 0xFFFF) - 0x8000


J3D_ANM_EVAL = {"ANK1", "TTK1", "TRK1", "PAK1", "TPT1", "VAF1"}


def anm_eval(b, o, tag, frame_max):
    def tab(off):
        return o + u32(b, o + off) if u32(b, o + off) != 0 else None

    def kt(pos):  # J3DAnmKeyTableBase
        return u16(b, pos), u16(b, pos + 2), u16(b, pos + 4)

    frames = [0.0, to_f32(frame_max * 0.5), float(frame_max)]
    fam = {}

    def F(name):
        if name not in fam:
            fam[name] = AnmFamily()
        return fam[name]

    out = {}

    def ids(name, off, count):
        p = tab(off)
        for i in range(count):
            F(name).add(u16(b, p + 2 * i))

    def names(off):
        p = tab(off)
        return u16(b, p) if p is not None else 0

    def key_f32(info, data, frame, default):
        mx, offs, ktype = info
        if mx == 0:
            return default
        if mx == 1:
            return f32(b, data + 4 * offs)
        return anm_key(b, data + 4 * offs, 4, ">f", frame, mx, ktype, anm_hermite)

    def key_rot(info, data, frame, shift):
        """J3DTransformInfo/J3DTextureSRTInfo rotation: (s32)S-interpolation << shift, as s16."""
        mx, offs, ktype = info
        if mx == 0:
            return 0, 0
        if mx == 1:
            return wrap_s16(s16(b, data + 2 * offs) << shift), 0
        v = anm_key(b, data + 2 * offs, 2, ">h", frame, mx, ktype, anm_hermite_s)
        i, step = anm_trunc(v, 1 << shift)
        return wrap_s16(i << shift), step

    if tag == "ANK1":
        shift = u8(b, o + 0x09)
        joints = u16(b, o + 0x0C)
        out["count"] = joints
        table, sc, rot, tr = tab(0x14), tab(0x18), tab(0x1C), tab(0x20)
        for frame in frames:
            for j in range(joints):
                for axis in range(3):
                    e = table + (j * 3 + axis) * 0x12
                    F("scale").add(key_f32(kt(e), sc, frame, 1.0))
                    v, step = key_rot(kt(e + 6), rot, frame, shift)
                    F("rot").add(v, step)
                    F("trans").add(key_f32(kt(e + 12), tr, frame, 0.0))
    elif tag == "TTK1":
        shift = u8(b, o + 0x09)
        mats = u16(b, o + 0x0C) // 3
        out["count"] = mats
        out["names"] = names(0x1C)
        table, sc, rot, tr = tab(0x14), tab(0x28), tab(0x2C), tab(0x30)
        for frame in frames:
            for m in range(mats):
                e = table + m * 3 * 0x12
                F("scale").add(key_f32(kt(e), sc, frame, 1.0))
                F("scale").add(key_f32(kt(e + 0x12), sc, frame, 1.0))
                v, step = key_rot(kt(e + 2 * 0x12 + 6), rot, frame, shift)
                F("rot").add(v, step)
                F("trans").add(key_f32(kt(e + 12), tr, frame, 0.0))
                F("trans").add(key_f32(kt(e + 0x12 + 12), tr, frame, 0.0))
        ids("ids", 0x18, mats)
        p = tab(0x20)
        for m in range(mats):
            F("texmtx").add(u8(b, p + m))
        p = tab(0x24)
        if p is not None:
            for m in range(mats * 3):
                F("center").add(f32(b, p + 4 * m))
    elif tag in ("TRK1", "PAK1"):
        if tag == "TRK1":
            groups = [("creg", 0x0C, 0x20, 0x38, 0x1C, -1024.0, 1023.0, 0x28, 0x30),
                      ("kreg", 0x0E, 0x24, 0x48, 0x1C, 0.0, 255.0, 0x2C, 0x34)]
        else:
            groups = [("color", 0x0E, 0x18, 0x24, 0x18, 0.0, 255.0, 0x1C, 0x20)]
        counts = []
        for name, num_off, table_off, val_off, esize, lo, hi, id_off, name_off in groups:
            num = u16(b, o + num_off)
            counts.append(num)
            table = tab(table_off)
            vals = [tab(val_off + 4 * c) for c in range(4)]
            for frame in frames:
                for m in range(num):
                    for c in range(4):
                        mx, offs, ktype = kt(table + m * esize + c * 6)
                        if mx == 0:
                            F(name).add(0)
                        elif mx == 1:
                            raw = s16(b, vals[c] + 2 * offs)
                            F(name).add(raw if name == "creg" else raw & 0xFF)
                        else:
                            v = anm_key(b, vals[c] + 2 * offs, 2, ">h", frame, mx, ktype,
                                        anm_hermite)
                            if v < lo:
                                F(name).add(int(lo))
                            elif v > hi:
                                F(name).add(int(hi))
                            else:
                                i, step = anm_trunc(v)
                                F(name).add(i, step)
            ids(name[0] + "ids" if tag == "TRK1" else "ids", id_off, num)
            out[name[0] + "names" if tag == "TRK1" else "names"] = names(name_off)
        out["count"] = counts[0] if tag == "PAK1" else "%d,%d" % tuple(counts)
    elif tag in ("TPT1", "VAF1"):
        num = u16(b, o + 0x0C)
        out["count"] = num
        table, vals = tab(0x10), tab(0x14)
        size, fmt, name = (2, ">H", "texno") if tag == "TPT1" else (1, ">B", "vis")
        esize = 8 if tag == "TPT1" else 4
        for frame in frames:
            for m in range(num):
                mx, offs = u16(b, table + m * esize), u16(b, table + m * esize + 2)
                # getTexNo/getVisibility: three separate ifs, the last one that holds wins
                idx = None
                if 0.0 <= frame < mx:
                    idx = int(frame) + offs
                if frame < 0.0:
                    idx = offs
                if frame >= mx:
                    idx = mx - 1 + offs
                F(name).add(struct.unpack_from(fmt, b, vals + size * idx)[0])
        if tag == "TPT1":
            ids("ids", 0x18, num)
            out["names"] = names(0x1C)
    out["eval"] = {k: v.record() for k, v in fam.items()}
    return out


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


# Step 4.9c: the records of the room, file and path chunks, read at the format's offsets. Per tag:
# (entry size, [(field, offset, type, count)]); type f is an f32 (kept as its bit pattern), strN a
# string of at most N bytes, the rest big-endian integers. The names are the ones
# COS_SMOKE=stage-sweep writes.
STAGE_RECORDS = {
    "STAG": (0x20, [("near", 0x00, "f", 1), ("far", 0x04, "f", 1),
                    ("camera_tool", 0x08, "u8", 1), ("prop", 0x09, "u8", 1),
                    ("particle", 0x0A, "u16", 1), ("type_schbit", 0x0C, "u32", 1),
                    ("schbit_far", 0x10, "u32", 1),
                    ("f14", 0x14, "u32", 1), ("f18", 0x18, "u32", 1), ("f1c", 0x1C, "u32", 1)]),
    "FILI": (0x08, [("param", 0x00, "u32", 1), ("sea_level", 0x04, "f", 1)]),
    "MULT": (0x0C, [("trans", 0x00, "f", 2), ("angle", 0x08, "s16", 1), ("room", 0x0A, "u8", 1),
                    ("wave_max", 0x0B, "u8", 1)]),
    "SCLS": (0x0C, [("stage", 0x00, "str8", 1), ("start", 0x08, "u8", 1), ("room", 0x09, "u8", 1),
                    ("wipe", 0x0A, "u8", 1), ("b0b", 0x0B, "u8", 1)]),
    "PATH": (0x0C, [("num", 0x00, "u16", 1), ("next", 0x02, "u16", 1), ("args", 0x04, "u8", 4)]),
    "PPNT": (0x10, [("args", 0x00, "u8", 4), ("pos", 0x04, "f", 3)]),
    "CAMR": (0x14, [("type", 0x00, "str16", 1), ("args", 0x10, "u8", 4)]),
    "AROB": (0x14, [("pos", 0x00, "f", 3), ("angle", 0x0C, "s16", 3), ("f12", 0x12, "s16", 1)]),
    "EVNT": (0x18, [("b00", 0x00, "u8", 1), ("name", 0x01, "str15", 1), ("args", 0x10, "u8", 4),
                    ("b14", 0x14, "s8", 1), ("args2", 0x15, "u8", 3)]),
    "2DMA": (0x38, [("f", 0x00, "f", 13), ("bytes", 0x34, "u8", 4)]),
    "SOND": (0x1C, [("name", 0x00, "str8", 1), ("pos", 0x08, "f", 3), ("bytes", 0x14, "u8", 7)]),
    # Step 4.9d: the environment chunks (d_kankyo reads them every frame).
    "LGHT": (0x1C, [("pos", 0x00, "f", 3), ("radius", 0x0C, "f", 1), ("b10", 0x10, "u8", 8),
                    ("color", 0x18, "u8", 3), ("fluct", 0x1B, "u8", 1)]),
    "LGTV": (0x1C, [("pos", 0x00, "f", 3), ("radius", 0x0C, "f", 1), ("b10", 0x10, "u8", 8),
                    ("b18", 0x18, "u8", 3), ("fluct", 0x1B, "u8", 1)]),
    "Colo": (0x0C, [("palette", 0x00, "u8", 8), ("change_rate", 0x08, "f", 1)]),
    "Pale": (0x2C, [("colors", 0x00, "u8", 33), ("virt", 0x21, "u8", 1), ("fog_z", 0x24, "f", 2)]),
    "Virt": (0x24, [("f00", 0x00, "u32", 4), ("kumo", 0x10, "u8", 4), ("kumo_center", 0x14, "u8", 4),
                    ("sky", 0x18, "u8", 3), ("uso_umi", 0x1B, "u8", 3),
                    ("kasumi_mae", 0x1E, "u8", 3)]),
    "EnvR": (0x08, [("pselect", 0x00, "u8", 8)]),
}
for _alias, _tag in (("RPAT", "PATH"), ("RPPN", "PPNT"), ("RCAM", "CAMR"), ("RARO", "AROB"),
                     ("2Dma", "2DMA")):
    STAGE_RECORDS[_alias] = STAGE_RECORDS[_tag]


def stage_field(b, o, typ, count):
    """One field of a STAGE_RECORDS record: an int, a string, or a list when count > 1."""
    if typ.startswith("str"):
        return cstr(b, o, int(typ[3:]))
    fmt = {"u8": "B", "s8": "b", "u16": "H", "s16": "h", "u32": "I", "f": "I"}[typ]
    vals = list(struct.unpack_from(">%d%s" % (count, fmt), b, o))
    return vals[0] if count == 1 else vals


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
    # Step 4.9c: the records of the first chunk of each room, file and path tag (the chunk
    # dStage_dt_c_decode hands out), keyed by the chunk's index. An offset of 0 is "no data" (the
    # game's relocation leaves it null).
    rec["records"] = {}
    first = {}
    for i, c in enumerate(rec["chunks"]):
        first.setdefault(c["tag"], i)
    for tag, i in sorted(first.items(), key=lambda t: t[1]):
        c = rec["chunks"][i]
        num, off = c["num"], c["offset"]
        if off == 0:
            continue
        if tag == "RTBL":
            # A table of u32 file offsets, each to {u8 num, u8, u8, pad, u32 room list offset}.
            need(off + num * 4 <= len(b), "dzs/dzr: RTBL entries past the end")
            entries = []
            for k in range(num):
                e = u32(b, off + k * 4)
                need(e + 8 <= len(b), "dzs/dzr: RTBL entry %d past the end" % k)
                rooms = u32(b, e + 4)
                need(rooms + b[e] <= len(b), "dzs/dzr: RTBL entry %d rooms past the end" % k)
                entries.append({"num": b[e], "b01": b[e + 1], "b02": b[e + 2],
                                "rooms": list(b[rooms:rooms + b[e]])})
            rec["records"][str(i)] = {"tag": tag, "entries": entries}
            continue
        spec = STAGE_RECORDS.get(tag)
        if spec is None:
            continue
        esize, fields = spec
        need(off + num * esize <= len(b), "dzs/dzr: %s entries past the end" % tag)
        rec["records"][str(i)] = {"tag": tag, "entries": [
            {name: stage_field(b, off + k * esize + fo, typ, count)
             for name, fo, typ, count in fields}
            for k in range(num)]}
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
    return {"sections": sections, "audio": parse_aaf_audio(b, sections)}


def f32(b, o):
    return struct.unpack_from(">f", b, o)[0]


def to_f32(x):
    """x rounded to the nearest f32 (an f32 product or quotient of f32 values, computed in double,
    rounds to the f32 result)."""
    return struct.unpack(">f", struct.pack(">f", x))[0]


def parse_sound_table(b, o, size):
    """Section 1 of JaiInit.aaf as JAInter::SoundTable::init reads it: the formats and version
    bytes, per category (18) the sound count and first entry (u16 pairs from offset 6), and the
    16-byte entries from offset 0x50 (flag, priority, byte 5, offset number, pitch, volume)."""
    cats = []
    for c in range(18):
        count, first = u16(b, o + 6 + 4 * c), u16(b, o + 8 + 4 * c)
        sounds = []
        for i in range(count):
            e = o + 0x50 + 16 * (first + i)
            need(e + 16 <= o + size, "aaf: sound %d/%d past the table" % (c, i))
            sounds.append([u32(b, e), u8(b, e + 4), u8(b, e + 5), u16(b, e + 6), f32(b, e + 8),
                           u32(b, e + 12)])
        cats.append({"first": first, "sounds": sounds})
    return {"formats": [u8(b, o), u8(b, o + 1), u8(b, o + 2)], "version": u8(b, o + 3),
            "categories": cats}


def osc_table(b, o):
    """An oscillator envelope: s16 triplets up to and including the first whose mode is > 0xa
    (BNKParser::getOscTableEndPtr); returns [length in s16, FNV-1a 64 of its bytes]."""
    n = 0
    while True:
        mode = s16(b, o + 2 * n)
        n += 3
        if mode > 0xA or n >= 3 * 4096:
            break
    return [n, fnv1a64(bytes(b[o:o + 2 * n]))]


def parse_bnk(b, base):
    """A bank (IBNK, section 2 of JaiInit.aaf) at base, at the offsets BNKParser's structs give:
    the bank ID (word 2), the 0x80 instrument offsets at 0x24 and the 12 percussion offsets at
    0x3B4, all relative to the bank."""
    def at(off):
        return base + off if off else None

    insts = []
    for slot in range(0x80):
        io = at(u32(b, base + 0x24 + 4 * slot))
        if io is None:
            continue
        oscs = []
        for j in range(2):
            oo = at(u32(b, io + 0x10 + 4 * j))
            if oo is None:
                continue
            t, r = at(u32(b, oo + 8)), at(u32(b, oo + 0xC))
            oscs.append([u8(b, oo), f32(b, oo + 4)] + (osc_table(b, t) if t else [0, 0]) +
                        (osc_table(b, r) if r else [0, 0]) + [f32(b, oo + 0x10), f32(b, oo + 0x14)])
        effects = sum(1 for j in range(4) if u32(b, io + 0x18 + 4 * j))
        keys = []
        for k in range(u32(b, io + 0x28)):
            ko = base + u32(b, io + 0x2C + 4 * k)
            velos = []
            for v in range(u32(b, ko + 4)):
                vo = base + u32(b, ko + 8 + 4 * v)
                velos.append([u8(b, vo), u32(b, vo + 4) & 0xFFFF, f32(b, vo + 8), f32(b, vo + 0xC)])
            keys.append([u8(b, ko), velos])
        insts.append({"slot": slot, "volume": f32(b, io + 8), "pitch": f32(b, io + 0xC),
                      "osc": oscs, "effects": effects, "keys": keys})
    drums = []
    for i in range(12):
        po = at(u32(b, base + 0x3B4 + 4 * i))
        if po is None:
            continue
        per2 = tag4(b, po) == "PER2"
        percs = []
        for key in range(0x80):
            pm = at(u32(b, po + 0x88 + 4 * key))
            if pm is None:
                continue
            velos = []
            for v in range(u32(b, pm + 0x10)):
                vo = base + u32(b, pm + 0x14 + 4 * v)
                velos.append([u8(b, vo), u32(b, vo + 4) & 0xFFFF, f32(b, vo + 8), f32(b, vo + 0xC)])
            pan = struct.unpack_from(">b", b, po + 0x288 + key)[0] if per2 else None
            release = u16(b, po + 0x308 + 2 * key) if per2 else None
            percs.append([key, f32(b, pm), f32(b, pm + 4), pan, release, velos])
        drums.append({"slot": 0xE4 + i, "percs": percs})
    return {"id": u32(b, base + 8), "insts": insts, "drums": drums}


def parse_ws(b, base):
    """A wave system (WSYS, section 3 of JaiInit.aaf) at base, at WSParser's offsets: the archive
    bank (0x10) gives each group's .aw file name and waves, the control group (0x14) each group's
    wave IDs."""
    cg = base + u32(b, base + 0x14)
    ab = base + u32(b, base + 0x10)
    groups = []
    for g in range(u32(b, cg + 8)):
        scene = base + u32(b, cg + 0xC + 4 * g)
        ctrl = base + u32(b, scene + 0xC)
        arc = base + u32(b, ab + 8 + 4 * g)
        waves = []
        for w in range(u32(b, ctrl + 4)):
            wid = u32(b, base + u32(b, ctrl + 8 + 4 * w)) & 0xFFFF
            wo = base + u32(b, arc + 0x74 + 4 * w)
            waves.append([wid, u8(b, wo), u8(b, wo + 1), u8(b, wo + 2), f32(b, wo + 4)] +
                         [s32(b, wo + x) for x in (8, 0xC, 0x10, 0x14, 0x18, 0x1C)] +
                         [s16(b, wo + 0x20), s16(b, wo + 0x22), s32(b, wo + 0x28)])
        groups.append({"file": cstr(b, arc, 0x74), "waves": waves})
    return {"groups": groups}


def stream_header(b, o):
    """The first 16 bytes of an AFC header (StreamLib::StreamHeader): sizes, rate and format."""
    return [s32(b, o), s32(b, o + 4), u16(b, o + 8), u16(b, o + 10), u16(b, o + 12), u16(b, o + 14)]


def parse_aaf_audio(b, sections):
    """The records JAInter::InitData::checkInitDataOnMemory hands to JAudio, decoded from the
    sections parse_aaf found: sound table (1), banks (2), wave systems (3), stream table (5),
    scene table (6) and fx scene table (7)."""
    audio = {}
    for s in sections:
        t = s["type"]
        if t == 1:
            audio["sound_table"] = parse_sound_table(b, s["offset"], s["size"])
        elif t == 2:
            audio["banks"] = [dict(parse_bnk(b, e["offset"]), size=e["size"], wavebank=e["flags"])
                              for e in s["entries"]]
        elif t == 3:
            audio["wave_systems"] = [dict(parse_ws(b, e["offset"]), size=e["size"], mode=e["flags"])
                                     for e in s["entries"]]
        elif t == 5:
            o, n = s["offset"], (s["size"] - 0x10) // 0x30
            audio["streams"] = [[cstr(b, o + 0x30 * i + 0x10, 16)] + stream_header(b, o + 0x30 * i + 0x20)
                                for i in range(n)]
        elif t == 6:
            o = s["offset"]
            audio["scenes"] = [bytes(b[o + u32(b, o + 4 + 4 * i):o + u32(b, o + 4 + 4 * i) + 16]).hex()
                               for i in range(u32(b, o))]
        elif t == 7:
            o = s["offset"]
            scenes = []
            for i in range(u32(b, o)):
                c = o + u32(b, o + 0x14 + 4 * i)
                scenes.append([[u8(b, c + 0x20 * l), u16(b, c + 0x20 * l + 2), s16(b, c + 0x20 * l + 4),
                                u16(b, c + 0x20 * l + 6), s16(b, c + 0x20 * l + 8), s32(b, c + 0x20 * l + 0xC)] +
                               [s16(b, c + 0x20 * l + 0x10 + 2 * t) for t in range(8)] for l in range(4)])
            audio["fx"] = {"header": [u32(b, o + 4 * i) for i in range(5)], "scenes": scenes}
    return audio


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
                if rec["format"] == "afc" and e["size"] >= 0x20:
                    f.seek(e["offset"])
                    rec["afc"] = stream_header(f.read(0x20), 0)
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


def check_blo(manifest, blo_path):
    """blo_sweep.txt: what COS_SMOKE=blo-sweep built through J2DScreen::set, one line per file in
    the font.txt syntax: 'BLO <path> PAN1=n PIC1=n WIN1=n TBX1=n', the panes of each kind in the
    game's pane tree against the file's blocks of that tag. Every BLO of the disc must be there."""
    problems = []
    files, _ = check_jut_lines(manifest, blo_path, {"BLO": "blo"}, problems)
    all_blo = [r for r in records_by_path(manifest).values() if r.get("format") == "blo"]
    panes = sum(sum(r.get("panes", {}).values()) for r in all_blo)
    print("disc_manifest: blo_sweep.txt: %d BLO (the disc has %d, %d panes)"
          % (files["BLO"], len(all_blo), panes))
    if files["BLO"] != len(all_blo):
        problems.append("%d BLO files reported, the disc has %d" % (files["BLO"], len(all_blo)))
    return report_problems(problems, "blo_sweep.txt equals the manifest")


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


def check_dzb(manifest, dzb_path):
    """dzb_sweep.txt lines (fields separated by single spaces): 'DZB <archive>:dzb/<file name>
    vertices=N triangles=N blocks=N tree_nodes=N groups=N infos=N flag=N offsets=V,T,B,N,G,I
    [bbox=minX,minY,minZ,maxX,maxY,maxZ]' (step 4.10), the path being the manifest's; offsets in
    the manifest's order (vtx, tri, blk, tree, grp, ti), the bounding box compared as the
    manifest rounds it (3 decimals). Every dzb of the disc must be there once."""
    by_path = records_by_path(manifest)
    # The game converts the files of an archive's 'DZB ' directory (dzb/); the disc has one more
    # copy under dzs/ in /res/Stage/ITest61/Stage.arc, which it never reads as collision.
    all_dzb = {p: r for p, r in by_path.items()
               if r.get("format") == "dzb" and p.rpartition(":")[2].startswith("dzb/")}
    problems = []
    seen = Counter()
    order = ("vtx", "tri", "blk", "tree", "grp", "ti")
    with open(dzb_path, encoding="utf-8", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            if len(parts) < 3 or parts[0] != "DZB":
                problems.append("line %d: malformed: %r" % (ln, line))
                continue
            path, rec = parts[1], all_dzb.get(parts[1])
            if rec is None:
                problems.append("line %d: %s is not a dzb file of the manifest" % (ln, path))
                continue
            seen[parts[1]] += 1
            got = dict(field.partition("=")[::2] for field in parts[2:])
            want = {k: str(rec[k]) for k in ("vertices", "triangles", "blocks", "tree_nodes",
                                             "groups", "infos", "flag")}
            want["offsets"] = ",".join(str(rec["offsets"][k]) for k in order)
            # A vertex offset of 0 is no vertex table (the manifest's box then reads the header).
            if "bbox" in rec and rec["offsets"]["vtx"] != 0:
                box = rec["bbox"]["min"] + rec["bbox"]["max"]
                want["bbox"] = ",".join("%r" % float(x) for x in box)
                try:
                    # Each value printed with 9 significant digits: back to its f32 exactly.
                    got["bbox"] = ",".join(
                        "%r" % round(struct.unpack(">f", struct.pack(">f", float(x)))[0], 3)
                        for x in got.get("bbox", "").split(","))
                except ValueError:
                    pass
            for key in sorted(set(want) | set(got)):
                if got.get(key) != want.get(key):
                    problems.append("line %d: %s %s=%s, the manifest has %s"
                                    % (ln, path, key, got.get(key), want.get(key)))
    for path in sorted(all_dzb):
        if seen[path] != 1:
            problems.append("%s: %d DZB lines" % (path, seen[path]))
    print("disc_manifest: dzb_sweep.txt: %d dzb files (the disc has %d) compared"
          % (len(seen), len(all_dzb)))
    return report_problems(problems, "dzb_sweep.txt equals the manifest")


# ---- stage chunk tables (step 4.9a) -------------------------------------------------------------

def check_stage_record(rec, path, parts, ln, problems, rec_seen, dec):
    """One 'REC <path> <chunk index> <entry> tag=XXXX <field>=<value>...' line of
    stage_sweep.txt (step 4.9c) against the manifest's record; a list is comma-separated, an f32
    in decimal (compared as the f32 it rounds to), a string percent-encoded. True if compared."""
    try:
        index, entry = int(parts[2]), int(parts[3])
        fields = dict(p.partition("=")[::2] for p in parts[4:])
    except (ValueError, IndexError):
        problems.append("line %d: malformed: %r" % (ln, " ".join(parts)))
        return False
    want_chunk = rec.get("records", {}).get(str(index))
    if want_chunk is None or fields.get("tag") != want_chunk["tag"] or \
            not 0 <= entry < len(want_chunk["entries"]):
        problems.append("line %d: %s chunk %d (%s) entry %d: the manifest has %s"
                        % (ln, path, index, fields.get("tag"), entry,
                           "no records there" if want_chunk is None else
                           "%d %s records" % (len(want_chunk["entries"]), want_chunk["tag"])))
        return False
    got_set = rec_seen.setdefault((path, index), set())
    if entry in got_set:
        problems.append("line %d: %s chunk %d entry %d reported twice" % (ln, path, index, entry))
    got_set.add(entry)
    tag = want_chunk["tag"]
    want = want_chunk["entries"][entry]
    if tag == "RTBL":
        types = {"num": "u8", "b01": "u8", "b02": "u8", "rooms": "list"}
    else:
        types = {name: typ for name, _, typ, _ in STAGE_RECORDS[tag][1]}
    for name, typ in types.items():
        text = fields.get(name)
        if text is None:
            problems.append("line %d: %s %s entry %d: no %s" % (ln, path, tag, entry, name))
            continue
        try:
            if typ.startswith("str"):
                got = dec(text)
            elif typ == "f":
                got = [struct.unpack(">I", struct.pack(">f", float(v)))[0]
                       for v in text.split(",")]
            else:
                got = [int(v) for v in text.split(",")] if text else []
        except (ValueError, OverflowError):
            problems.append("line %d: %s %s entry %d: malformed %s=%s"
                            % (ln, path, tag, entry, name, text))
            continue
        if isinstance(got, list) and not isinstance(want[name], list):
            got = got[0] if len(got) == 1 else got
        if got != want[name]:
            problems.append("line %d: %s chunk %d %s entry %d %s=%s, the manifest has %s"
                            % (ln, path, index, tag, entry, name, text, want[name]))
    for name in set(fields) - set(types) - {"tag"}:
        problems.append("line %d: unexpected field %s" % (ln, name))
    return True


def check_stage(manifest, stage_path):
    """stage_sweep.txt lines (fields separated by single spaces): 'STG <path> chunk_count=N',
    'CHUNK <path> <index> tag=XXXX num=N offset=N' and (step 4.9b) 'ACTOR <path> <chunk index>
    <entry> name=<s> params=N pos=X,Y,Z angle=A,B,C set_id=N' and (step 4.9c) REC lines (see
    check_stage_record), <path> being the manifest's name of
    the file ('<archive>:dzs/stage.dzs' or '<archive>:dzr/room.dzr'); the name is percent-encoded
    (a space, '%' and bytes outside printable ASCII as %XX). Every dzs/dzr of the disc, every one
    of its chunks and every actor record of the manifest must be there; positions are compared
    as the manifest rounds them (3 decimals)."""
    from urllib.parse import unquote_to_bytes

    def dec(text):
        raw = unquote_to_bytes(text)
        try:
            return raw.decode("shift_jis")
        except UnicodeDecodeError:
            return raw.decode("latin-1")

    by_path = records_by_path(manifest)
    all_stage = {p: r for p, r in by_path.items() if r.get("format") in ("dzs", "dzr")}
    problems = []
    seen = {}
    chunks = 0
    actors = 0
    actor_seen = {}  # (path, tag) -> set of entry indices
    records = 0
    rec_seen = {}  # (path, chunk index) -> set of entry indices (step 4.9c)
    with open(stage_path, encoding="utf-8", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            if len(parts) < 3 or parts[0] not in ("STG", "CHUNK", "ACTOR", "REC"):
                problems.append("line %d: malformed: %r" % (ln, line))
                continue
            kind, path = parts[0], parts[1]
            rec = all_stage.get(path)
            if rec is None:
                problems.append("line %d: %s is not a dzs/dzr file of the manifest" % (ln, path))
                continue
            got = seen.setdefault(path, {"STG": 0, "CHUNK": set()})
            if kind == "STG":
                got["STG"] += 1
                fields = dict(p.partition("=")[::2] for p in parts[2:])
                if fields.get("chunk_count") != str(rec["chunk_count"]):
                    problems.append("line %d: %s chunk_count=%s, the manifest has %d"
                                    % (ln, path, fields.get("chunk_count"), rec["chunk_count"]))
                continue
            if kind == "REC":
                if check_stage_record(rec, path, parts, ln, problems, rec_seen, dec):
                    records += 1
                continue
            if kind == "ACTOR":
                try:
                    index, entry = int(parts[2]), int(parts[3])
                    fields = dict(p.partition("=")[::2] for p in parts[4:])
                    got_rec = {"name": dec(fields["name"]), "params": int(fields["params"]),
                               "pos": [round(float(v), 3) for v in fields["pos"].split(",")],
                               "angle": [int(v) for v in fields["angle"].split(",")],
                               "set_id": int(fields["set_id"])}
                except (ValueError, IndexError, KeyError):
                    problems.append("line %d: malformed: %r" % (ln, line))
                    continue
                if not 0 <= index < len(rec["chunks"]):
                    problems.append("line %d: %s chunk %d: the file has %d"
                                    % (ln, path, index, len(rec["chunks"])))
                    continue
                tag = rec["chunks"][index]["tag"]
                want_list = rec["actors"].get(tag)
                if want_list is None or not 0 <= entry < len(want_list):
                    problems.append("line %d: %s chunk %d (%s) entry %d: the manifest has %s"
                                    % (ln, path, index, tag, entry,
                                       "no records" if want_list is None else
                                       "%d records" % len(want_list)))
                    continue
                got_set = actor_seen.setdefault((path, tag), set())
                if entry in got_set:
                    problems.append("line %d: %s %s entry %d reported twice"
                                    % (ln, path, tag, entry))
                got_set.add(entry)
                actors += 1
                want = want_list[entry]
                for key in ("name", "params", "pos", "angle", "set_id"):
                    if got_rec[key] != want[key]:
                        problems.append("line %d: %s %s entry %d %s=%s, the manifest has %s"
                                        % (ln, path, tag, entry, key, got_rec[key], want[key]))
                if set(fields) - {"name", "params", "pos", "angle", "set_id"}:
                    problems.append("line %d: unexpected fields %s" % (
                        ln, sorted(set(fields) - {"name", "params", "pos", "angle", "set_id"})))
                continue
            try:
                index = int(parts[2])
            except ValueError:
                problems.append("line %d: malformed index: %r" % (ln, line))
                continue
            if not 0 <= index < len(rec["chunks"]):
                problems.append("line %d: %s chunk %d: the file has %d" % (ln, path, index,
                                                                         len(rec["chunks"])))
                continue
            if index in got["CHUNK"]:
                problems.append("line %d: %s chunk %d reported twice" % (ln, path, index))
            got["CHUNK"].add(index)
            chunks += 1
            want = rec["chunks"][index]
            fields = dict(p.partition("=")[::2] for p in parts[3:])
            for key in ("tag", "num", "offset"):
                if fields.get(key) != str(want[key]):
                    problems.append("line %d: %s chunk %d %s=%s, the manifest has %s"
                                    % (ln, path, index, key, fields.get(key), want[key]))
            for key in set(fields) - {"tag", "num", "offset"}:
                problems.append("line %d: %s chunk %d: unexpected field %s" % (ln, path, index, key))
    for path, rec in sorted(all_stage.items()):
        got = seen.get(path)
        if got is None or got["STG"] != 1:
            problems.append("%s: %d STG lines" % (path, 0 if got is None else got["STG"]))
            continue
        if len(got["CHUNK"]) != len(rec["chunks"]):
            problems.append("%s: %d of %d chunks reported" % (path, len(got["CHUNK"]),
                                                              len(rec["chunks"])))
        for tag, want_list in sorted(rec["actors"].items()):
            n = len(actor_seen.get((path, tag), ()))
            if n != len(want_list):
                problems.append("%s: %d of %d %s records reported" % (path, n, len(want_list),
                                                                        tag))
        for index, want in sorted(rec.get("records", {}).items(), key=lambda t: int(t[0])):
            n = len(rec_seen.get((path, int(index)), ()))
            if n != len(want["entries"]):
                problems.append("%s: %d of %d %s records reported (chunk %s)"
                                % (path, n, len(want["entries"]), want["tag"], index))
    print("disc_manifest: stage_sweep.txt: %d dzs/dzr files (the disc has %d), %d chunks, "
          "%d actor records and %d room/file/path records compared"
          % (len(seen), len(all_stage), chunks, actors, records))
    return report_problems(problems, "stage_sweep.txt equals the manifest")


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


# ---- audio-parse cross-check (step 5.1) ---------------------------------------------------------
def g9(x):
    """An f32 as the harness prints it: %.9g of its double value."""
    return "%.9g" % x


def audio_lines(manifest):
    """The lines COS_SMOKE=audio-parse writes (native/src/pc/pc_audio.cpp), built from the
    manifest's independent reading of JaiInit.aaf, JaiSeqs.arc and the /Audiores files."""
    by_path = {r["path"]: r for r in manifest["files"]}
    aaf = by_path.get("/Audiores/JaiInit.aaf")
    if aaf is None or "audio" not in aaf:
        return None
    a = aaf["audio"]
    st, banks, wss = a["sound_table"], a["banks"], a["wave_systems"]
    out = ["AAF banks %d wavesystems %d scenes %d" % (len(banks), len(wss), len(a["scenes"]))]
    cats = st["categories"]
    cat_max = max([c + 1 for c in range(16) if cats[c]["sounds"]] or [0])
    out.append("STBL formats %d %d %d version %d categories %d" % tuple(st["formats"] + [st["version"], cat_max]))
    for c, cat in enumerate(cats):
        out.append("CAT %d %d %d" % (c, len(cat["sounds"]), cat["first"]))
        for i, (flag, prio, b5, off, pitch, vol) in enumerate(cat["sounds"]):
            out.append("SND %d %d %08x %d %d %d %s %08x" % (c, i, flag, prio, b5, off, g9(pitch), vol))
    # BankMgr::registBankBNK maps each bank's ID to its index, the later bank winning.
    vir = {}
    for i, bank in enumerate(banks):
        if bank["id"] != 0xFFFF:
            vir[bank["id"]] = i
    for v in sorted(vir):
        if v < 0x100:
            out.append("VIR %d %d" % (v, vir[v]))
    for i, bank in enumerate(banks):
        assigned = bank["wavebank"] if bank["wavebank"] < len(wss) else -1
        out.append("BNK %d size %d wavebank %d assigned %d insts %d drumsets %d" % (
            i, bank["size"], bank["wavebank"], assigned, len(bank["insts"]), len(bank["drums"])))
        for inst in bank["insts"]:
            sl = inst["slot"]
            out.append("INST %d %d %s %s osc=%d effects=%d keys=%d" % (
                i, sl, g9(inst["volume"]), g9(inst["pitch"]), len(inst["osc"]), inst["effects"],
                len(inst["keys"])))
            for j, (tgt, rate, tl, tf, rl, rf, f10, f14) in enumerate(inst["osc"]):
                out.append("OSC %d %d %d %d %s %d %016x %d %016x %s %s" % (
                    i, sl, j, tgt, g9(rate), tl, tf, rl, rf, g9(f10), g9(f14)))
            for k, (high, velos) in enumerate(inst["keys"]):
                out.append("KEY %d %d %d %d %d" % (i, sl, k, high, len(velos)))
                for v, (base_vel, wave, vol, pitch) in enumerate(velos):
                    out.append("VEL %d %d %d %d %d %d %s %s" % (i, sl, k, v, base_vel, wave, g9(vol),
                                                             g9(pitch)))
        for drums in bank["drums"]:
            out.append("DRUMS %d %d" % (i, drums["slot"]))
            for key, vol, pitch, pan, release, velos in drums["percs"]:
                # TDrumSet::getParam(key, 127): the first velocity region reaching 127.
                region = next((r for r in velos if r[0] >= 127), None)
                if region is None:
                    continue
                pan_f = to_f32(pan / to_f32(127.0)) if pan is not None else 0.5
                out.append("PERC %d %d %d %s %s %s %d %d" % (
                    i, drums["slot"], key, g9(to_f32(to_f32(vol) * region[2])),
                    g9(to_f32(to_f32(pitch) * region[3])), g9(pan_f),
                    release if release is not None else 1000, region[1]))
    for i, ws in enumerate(wss):
        groups = ws["groups"]
        table = max([w[0] for g in groups for w in g["waves"]] or [0]) + 1
        kind = "simple" if len(groups) == 1 else "basic"
        out.append("WS %d size %d mode %d %s groups %d table %d" % (i, ws["size"], ws["mode"], kind,
                                                                   len(groups), table))
        for g, group in enumerate(groups):
            aw = by_path.get("/Audiores/Banks/" + group["file"])
            out.append("WGRP %d %d %d %d" % (i, g, aw["entry"] if aw else -1, aw["size"] if aw else 0))
            waves = list(enumerate(group["waves"]))
            if kind == "simple":
                by_id = {}
                for _, w in waves:
                    by_id[w[0]] = w
                waves = [(wid, by_id[wid]) for wid in sorted(by_id)]
            for idx, w in waves:
                out.append("WAVE %d %d %d %d %d %d %d %s %s" % (
                    i, g, idx, w[0], w[1], w[2], w[3], g9(w[4]), " ".join(str(x) for x in w[5:])))
    seqs = by_path.get("/Audiores/Seqs/JaiSeqs.arc")
    seq_files = {}
    for f in (seqs or {}).get("files", []):
        if f.get("flags", 0) & 1 and f["id"] not in seq_files:
            seq_files[f["id"]] = f["size"]
    for n, info in enumerate(cats[16]["sounds"]):
        res = info[3] if st["formats"][1] & 1 else n
        out.append("SEQ %d %d %d" % (n, res, seq_files.get(res, 0)))
    for n, (name, *head) in enumerate(a["streams"][:len(cats[17]["sounds"])]):
        afc = by_path.get("/Audiores/Stream/" + name)
        out.append("STRM %d %s %d %d %s afc %s" % (
            n, name, afc["entry"] if afc else -1, afc["size"] if afc else -1,
            " ".join(str(x) for x in head),
            " ".join(str(x) for x in afc["afc"]) if afc and "afc" in afc else "-"))
    for i, scene in enumerate(a["scenes"]):
        out.append("SCENE %d %s" % (i, scene))
    fx = a["fx"]
    out.append("FX %d %d %d %d %d" % tuple(fx["header"]))
    for sidx, lines in enumerate(fx["scenes"]):
        for l, c in enumerate(lines):
            out.append("FXL %d %d %s" % (sidx, l, " ".join(str(x) for x in c)))
    return out


def check_audio(manifest, audio_path):
    """audio_parse.txt: what COS_SMOKE=audio-parse read through JAudio's init-data code (sound
    table, banks, wave systems, sequences, streams, scene and fx tables), line by line against
    audio_lines; every line must be there, in order."""
    problems = []
    want = audio_lines(manifest)
    if want is None:
        problems.append("the manifest has no JaiInit.aaf audio records")
        return report_problems(problems, "")
    with open(audio_path, encoding="utf-8") as f:
        got = [ln.rstrip("\n") for ln in f if ln.strip() and not ln.startswith("#")]
    for n in range(max(len(want), len(got))):
        w = want[n] if n < len(want) else "(none)"
        g = got[n] if n < len(got) else "(none)"
        if w != g:
            problems.append("line %d: manifest '%s', game '%s'" % (n + 1, w, g))
    kinds = Counter(ln.split(" ", 1)[0] for ln in want)
    print("disc_manifest: audio_parse.txt: %d lines (%d sounds, %d banks, %d instruments, %d "
          "drum sets, %d wave systems, %d wave groups, %d waves, %d sequences, %d streams) "
          "compared" % (len(want), kinds["SND"], kinds["BNK"], kinds["INST"], kinds["DRUMS"],
                        kinds["WS"], kinds["WGRP"], kinds["WAVE"], kinds["SEQ"], kinds["STRM"]))
    return report_problems(problems, "audio_parse.txt equals the manifest")


def check_j3d(manifest, j3d_path):
    """j3d_sweep.txt (step 4.11): what the game's J3D loaders built for every BMD/BDL/BMT, fields
    separated by single spaces: 'J3D <path> magic=<s> loader=bmd|bdl|bmt source=<s> flags=0x<hex>'
    followed by its 'INF1 <path> flags=N vertices=N' (the file's INF1 flags not in the loader's),
    'EVP1 <path> count=N' and 'DRW1 <path> count=N' (when the file has the block), 'JNT1 <path>
    count=N names=a,b,...', 'SHP1 <path> count=N' (models), 'MAT <path> count=N names=...' and
    'TEX1 <path> count=N names=...' lines. Names are percent-encoded (',', '%', a space and bytes
    outside printable ASCII as %XX). Every J3D2 file of the disc must be reported once."""
    from urllib.parse import unquote_to_bytes

    def dec(text):
        raw = unquote_to_bytes(text)
        try:
            return raw.decode("shift_jis")
        except UnicodeDecodeError:
            return raw.decode("latin-1")

    def names(value):
        return [dec(n) for n in value.split(",")] if value else []

    by_path = records_by_path(manifest)
    models = {p: r for p, r in by_path.items()
              if r.get("format") == "j3d" and r.get("magic", "")[:4] == "J3D2"}
    problems = []
    got = {}
    with open(j3d_path, encoding="ascii", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            if len(parts) < 3:
                problems.append("line %d: malformed: %r" % (ln, line))
                continue
            kind, path = parts[0], parts[1]
            fields = dict(p.partition("=")[::2] for p in parts[2:])
            if kind == "J3D":
                if path in got:
                    problems.append("line %d: %s reported twice" % (ln, path))
                got[path] = {"J3D": fields}
                continue
            if path not in got:
                problems.append("line %d: %s before its J3D line" % (ln, path))
                continue
            if kind in got[path]:
                problems.append("line %d: %s %s twice" % (ln, path, kind))
            got[path][kind] = fields

    counts = Counter()
    for path in sorted(set(models) - set(got)):
        problems.append("%s: J3D2 file of the manifest not loaded" % path)
    for path in sorted(set(got) - set(models)):
        problems.append("%s: not a J3D2 file of the manifest" % path)
    for path in sorted(set(models) & set(got)):
        rec, g = models[path], got[path]
        blocks = {}
        for b in rec.get("blocks", []):
            blocks.setdefault(b["tag"], b)
        head = g["J3D"]
        if head.get("magic") != rec["magic"]:
            problems.append("%s: magic %s, the manifest has %s" % (path, head.get("magic"),
                                                                 rec["magic"]))
        loader = head.get("loader")
        counts[loader] += 1
        want = {}
        if loader in ("bmd", "bdl"):
            try:
                plan_flags = int(head.get("flags", ""), 16)
            except ValueError:
                problems.append("%s: malformed loader flags %r" % (path, head.get("flags")))
                plan_flags = 0
            inf = blocks.get("INF1", {})
            want["INF1"] = {"flags": str(inf.get("flags", 0) & ~plan_flags & 0xFFFFFFFF),
                            "vertices": str(inf.get("vertices"))}
            for tag in ("EVP1", "DRW1"):
                if tag in blocks:
                    want[tag] = {"count": str(blocks[tag]["count"])}
            want["JNT1"] = {"count": str(blocks.get("JNT1", {}).get("count")),
                            "names": blocks.get("JNT1", {}).get("names", [])}
            want["SHP1"] = {"count": str(blocks.get("SHP1", {}).get("count"))}
            counts["joints"] += blocks.get("JNT1", {}).get("count", 0)
            counts["shapes"] += blocks.get("SHP1", {}).get("count", 0)
        # The materials as the loader names them: MAT3/MAT2 (the MDL3 count when the BDL's
        # material type reads MDL3 alone; the manifest's MDL3 names are those of MAT3).
        mat = blocks.get("MAT3") or blocks.get("MAT2")
        mdl = blocks.get("MDL3")
        if mat is not None:
            want["MAT"] = {"count": str(mat["count"]), "names": mat.get("names", [])}
        elif mdl is not None:
            want["MAT"] = {"count": str(mdl["count"])}
        else:
            want["MAT"] = {"count": "0", "names": []}
        tex = blocks.get("TEX1")
        want["TEX1"] = ({"count": str(tex["count"]), "names": tex.get("names", [])}
                        if tex is not None else {"count": "0"})
        counts["materials"] += int(want["MAT"]["count"])
        counts["textures"] += int(want["TEX1"]["count"])
        for kind, w in want.items():
            line = g.get(kind)
            if line is None:
                problems.append("%s: no %s line" % (path, kind))
                continue
            for key, value in w.items():
                have = names(line.get("names", "")) if key == "names" else line.get(key)
                if have != value:
                    problems.append("%s: %s %s=%s, the manifest has %s" % (path, kind, key, have,
                                                                           value))
        for kind in set(g) - set(want) - {"J3D"}:
            problems.append("%s: unexpected %s line" % (path, kind))
    print("disc_manifest: j3d_sweep.txt: %d model file(s) (%d BMD, %d BDL, %d BMT; the manifest "
          "has %d): %d joints, %d materials, %d shapes, %d textures compared"
          % (len(got), counts["bmd"], counts["bdl"], counts["bmt"], len(models), counts["joints"],
             counts["materials"], counts["shapes"], counts["textures"]))
    if not got:
        problems.append("no J3D line")
    return report_problems(problems, "j3d_sweep.txt equals the manifest")


def check_anm(manifest, anm_path):
    """anm_sweep.txt (step 4.12): what the game's J3D animation loaders and getters gave for every
    J3D1 file, fields separated by single spaces: 'ANM <path> magic=<s> tag=<s> attribute=N
    frame_max=N count=N [names=N | cnames=N knames=N]' (the BRK count is 'c,k'; paths are
    percent-encoded: a space, '%' and bytes outside printable ASCII as %XX), then per family
    of values 'VAL <path> <family> n=N sum=X abs=X max=X'. Counts, names and n must equal the
    manifest's; sum, abs and max may differ by the manifest's tolerance (integer values near a
    truncation boundary) plus 1e-5 of the magnitude for f32 values. Every J3D1 file of the disc
    must be reported once."""
    from urllib.parse import unquote_to_bytes

    def dec(text):
        raw = unquote_to_bytes(text)
        try:
            return raw.decode("shift_jis")
        except UnicodeDecodeError:
            return raw.decode("latin-1")

    by_path = records_by_path(manifest)
    anims = {p: r for p, r in by_path.items()
             if r.get("format") == "j3d" and r.get("magic", "")[:4] == "J3D1"}
    problems = []
    got = {}
    with open(anm_path, encoding="ascii", errors="replace") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            if parts[0] == "ANM" and len(parts) >= 3:
                path = dec(parts[1])
                if path in got:
                    problems.append("line %d: %s reported twice" % (ln, path))
                got[path] = {"head": dict(p.partition("=")[::2] for p in parts[2:]), "val": {}}
            elif parts[0] == "VAL" and len(parts) == 7:
                path = dec(parts[1])
                if path not in got:
                    problems.append("line %d: %s before its ANM line" % (ln, path))
                    continue
                vals = dict(p.partition("=")[::2] for p in parts[3:])
                try:
                    got[path]["val"][parts[2]] = (int(vals["n"]), float(vals["sum"]),
                                                  float(vals["abs"]), float(vals["max"]))
                except (KeyError, ValueError):
                    problems.append("line %d: malformed: %r" % (ln, line))
            else:
                problems.append("line %d: malformed: %r" % (ln, line))

    counts = Counter()
    values = 0
    for path in sorted(set(anims) - set(got)):
        problems.append("%s: J3D1 file of the manifest not reported" % path)
    for path in sorted(set(got) - set(anims)):
        problems.append("%s: not a J3D1 file of the manifest" % path)
    for path in sorted(set(anims) & set(got)):
        rec, g = anims[path], got[path]
        blocks = rec.get("blocks", [])
        if len(blocks) != 1 or "eval" not in blocks[0]:
            problems.append("%s: the manifest has no evaluated block (%s)"
                            % (path, [b["tag"] for b in blocks]))
            continue
        blk = blocks[0]
        counts[blk["tag"]] += 1
        want = {"magic": rec["magic"], "tag": blk["tag"], "attribute": str(blk["attribute"]),
                "frame_max": str(blk["frame_max"]), "count": str(blk["count"])}
        for key in ("names", "cnames", "knames"):
            if key in blk:
                want[key] = str(blk[key])
        if g["head"] != want:
            problems.append("%s: %s, the manifest has %s" % (path, g["head"], want))
        ev = blk["eval"]
        for fam in sorted(set(ev) | set(g["val"])):
            if fam not in ev or fam not in g["val"]:
                problems.append("%s: family %s only in %s" % (path, fam, "the manifest"
                                                             if fam in ev else "the report"))
                continue
            n, vsum, vabs, vmax, tol = ev[fam]
            gn, gsum, gabs, gmax = g["val"][fam]
            values += gn
            if gn != n:
                problems.append("%s: %s n=%d, the manifest has %d" % (path, fam, gn, n))
                continue
            slack = tol + 1e-5 * vabs + 1e-6
            for what, have, exp, lim in (("sum", gsum, vsum, slack), ("abs", gabs, vabs, slack),
                                         ("max", gmax, vmax, tol + 1e-5 * vmax + 1e-6)):
                if abs(have - exp) > lim:
                    problems.append("%s: %s %s=%.9g, the manifest has %.9g (tolerance %.3g)"
                                    % (path, fam, what, have, exp, lim))
    print("disc_manifest: anm_sweep.txt: %d animation file(s) (%s; the manifest has %d): %d "
          "values compared" % (len(got), ", ".join("%s %d" % kv for kv in sorted(counts.items())),
                               len(anims), values))
    if not got:
        problems.append("no ANM line")
    return report_problems(problems, "anm_sweep.txt equals the manifest")


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
    ap.add_argument("--check-stage", metavar="STG",
                    help="compare a COS_SMOKE=stage-sweep report with the manifest")
    ap.add_argument("--check-blo", metavar="BLO",
                    help="compare a COS_SMOKE=blo-sweep report with the manifest")
    ap.add_argument("--check-dzb", metavar="DZB",
                    help="compare a COS_SMOKE=dzb-sweep report with the manifest")
    ap.add_argument("--check-audio", metavar="AUD",
                    help="compare a COS_SMOKE=audio-parse report with the manifest")
    ap.add_argument("--check-j3d", metavar="J3D",
                    help="compare a COS_SMOKE=j3d-sweep report with the manifest")
    ap.add_argument("--check-anm", metavar="ANM",
                    help="compare a COS_SMOKE=anm-sweep report with the manifest")
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
    if args.check_stage:
        return check_stage(load_manifest(args.out), args.check_stage)
    if args.check_blo:
        return check_blo(load_manifest(args.out), args.check_blo)
    if args.check_dzb:
        return check_dzb(load_manifest(args.out), args.check_dzb)
    if args.check_audio:
        return check_audio(load_manifest(args.out), args.check_audio)
    if args.check_j3d:
        return check_j3d(load_manifest(args.out), args.check_j3d)
    if args.check_anm:
        return check_anm(load_manifest(args.out), args.check_anm)
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
