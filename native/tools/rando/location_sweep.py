#!/usr/bin/env python3
"""Location sweep: check placed items in the running game, location by location (R2.12).

    python3 -I native/tools/rando/location_sweep.py run [options]       # run the sweep (default)
    python3 -I native/tools/rando/location_sweep.py targets [--check]   # (re)write sweep_targets.json
    python3 -I native/tools/rando/location_sweep.py line [options]      # print a regress_rando line

docs/RANDOMIZER_PLAN.md, "The location sweep" and step R2.12. Each item location has runtime keys
(native/src/pc/features/rando/generated/rando_location_keys.inc, R0.5); the sweep boots or warps
the game (native/tools/run.sh rando, debug stage boot) to a stop where a key's actor exists, with a
placement JSON (COS_RANDO_PLACEMENT), and has an in-game smoke check it:

  T1 (static): the patched record (stage.dzs / room.dzr) holds the placed item, and a live actor
     at the record's position carries it;
  T2 (given):  the item is actually obtained in game (a chest is opened with a driven pad) and the
     get-item event gives the placed item and its item_func runs.

Location kinds and the smoke that checks each (sweep_targets.json "kinds"):
  chest  the loc smoke (smokes/pc_rando_smoke_loc.cpp): T1 and T2, one result per key. Many stops
         per run (COS_RANDO_LOC_HOPS), batched per stage; COS_RANDO_LOC_EXPECT makes a stop that
         lands elsewhere fail. Every check line of the run log is parsed.
  actr   the actr smoke (R2.2): T1 on the boot room; one run per stop, its exit code is the result
         of every ACTR key of that stop.
  scob   the scob smoke (R2.3): T1 on the boat, one run over the sea rooms (COS_SCOB_ROOMS); its
         per-room check lines give the result of the room's SCOB keys.
Kinds still without a sweep (events, code locations, boss items, added chests) are listed as
"no-target" in the report; later R2 steps add their kind to "kinds" (and a parser here if their smoke
needs one).

The report is a TSV (default build/rando-sweep/location_sweep.tsv), one row per key and check:
    loc  location  class  kind  key  where  tier  stop  run  result  detail
tier is T1-record, T1-live (chests), T1 (actr/scob: the smoke's own T1) or T2; result is ok, FAIL,
not-reached (the run ended or crashed before the stop, or the stop did not come up), not-checked (the
run went through the stop but the key was not checked) or no-target. A location passes when all its
rows are ok. Exit 0 when no selected row is FAIL, not-reached or not-checked; 1 otherwise; 2 usage.

sweep_targets.json holds per key its stops: "t1" (where T1 runs) and "t2" (where its actor is in the
stay room, for T2), as <stage>:<room>:<point>[:<layer>]. `targets` generates them from the disc (each
key's room: the RoomN archive, or a stage chest's x_rot & 0x3F; the room's spawn point as
native/tools/room_sweep.py chooses it, an SCLS entrance first; the key's layer chunk) and keeps the
file's hand-written "kinds" and "overrides":
    "overrides": {
      "spawn":  {"<stage>:<room>": <point>},          # a better spawn point for that room
      "keys":   {"<where> | <PCR_LOC_*>": {"t1": stop|null, "t2": stop|null, "events": ["2A80"],
                                           "note": "why"}},
    }
<where> is <stage>/<arc>/<tag>[<index>] (e.g. sea/Room11/TRE3[0]). `targets --check` exits 1 when
the file is not what the disc and the overrides give (no disc bytes are written: only stage names,
room, point and layer numbers).

run options:
  --placement P   a placement JSON, or identity | rotation (R0.6's fixtures) | none (no placement:
                  pc_rando() stays 0 and the vanilla items are expected); default rotation
  --tier T        t1 | t2 | both (default t1); T2 is run for the chest kind only
  --kinds K,...   location kinds (default: every kind with a smoke)
  --locs L,...    only these locations (PCR_LOC_* names, with or without the prefix, or numbers)
  --stages S,...  only keys of these stages
  --max-stops N   loc smoke stops per run (default 30)
  --out FILE      the TSV (default build/rando-sweep/location_sweep.tsv)
  --sweep-dir D   where the runs go (default build/rando-sweep/runs-<time>)
  --disc PATH     COS_DISC (default $COS_DISC or ~/Documents/windwaker/GZLE01.iso)
  --dry           print the runs, do not start them
  --no-retry      after a crashed or timed-out run, do not retry its unreached stops one per run
`line` takes the same selection options and prints regress_rando lines (relative placement path).

Mac only, dev time; standard library only (`targets` reads the disc through room_sweep.py's reader
and the disc manifest of native/tools/disc_manifest.py). Nothing it writes is meant for git except
sweep_targets.json.
"""

from __future__ import annotations

import argparse
import collections
import importlib.util
import json
import os
import re
import struct
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(SCRIPT_DIR)
REPO = os.path.normpath(os.path.join(TOOLS, "..", ".."))
RUN_SH = os.path.join(TOOLS, "run.sh")
KEYS_INC = os.path.join(REPO, "native", "src", "pc", "features", "rando", "generated", "rando_location_keys.inc")
LOCS_H = os.path.join(REPO, "native", "include", "pc", "pc_rando_locs.h")
TARGETS = os.path.join(SCRIPT_DIR, "sweep_targets.json")
FIXTURES = os.path.join(REPO, "native", "check", "rando", "fixtures")
PLACEMENTS = {"identity": "R0_6_identity.json", "rotation": "R0_6_rotation.json"}
DEFAULT_DISC = os.path.expanduser("~/Documents/windwaker/GZLE01.iso")
DEFAULT_OUT = os.path.join(REPO, "build", "rando-sweep", "location_sweep.tsv")

TSV_COLUMNS = ["loc", "location", "class", "kind", "key", "where", "tier", "stop", "run", "result", "detail"]


def die(msg, code=2):
    print("location-sweep: " + msg, file=sys.stderr)
    sys.exit(code)


# ---------------------------------------------------------------------------------------------
# The key table (R0.5's generated .inc and pc_rando_locs.h)

_KEY_RE = re.compile(
    r'^\s*\{KeyKind::(\w+), (PCR_LOC_\w+), 0x([0-9A-Fa-f]+), [^,]+, (nullptr|"[^"]*"), (nullptr|"[^"]*"), '
    r'(nullptr|"[^"]*"), (\d+), DzxField::(\w+),')
_EVENT_RE = re.compile(r'\{[^}]*\}, (?:0x[0-9A-Fa-f]+|\d+), (nullptr|"[^"]*")')
_LOC_RE = re.compile(r"^\s*(PCR_LOC_\w+) = (\d+), /\* (.*) \[([A-Z+]+)\] \*/")


def _str(tok):
    return None if tok == "nullptr" else tok[1:-1]


def load_locations():
    """[{id, enum, name, class}] in PCR_LOC_* order."""
    out = []
    with open(LOCS_H) as f:
        for line in f:
            m = _LOC_RE.match(line)
            if m:
                out.append({"id": int(m.group(2)), "enum": m.group(1), "name": m.group(3), "class": m.group(4)})
    if [l["id"] for l in out] != list(range(len(out))) or not out:
        die("%s: unexpected location list" % LOCS_H)
    return out


def load_keys():
    """[{key, kind, loc (enum), vanilla, stage, arc, tag, index, field, where}] in key order."""
    out = []
    with open(KEYS_INC) as f:
        for line in f:
            m = _KEY_RE.match(line)
            if not m:
                continue
            kind, loc, van, stage, arc, tag, index, field = m.groups()
            k = {"key": len(out), "kind": kind, "loc": loc, "vanilla": int(van, 16), "stage": _str(stage),
                 "arc": _str(arc), "tag": _str(tag), "index": int(index), "field": field}
            m2 = _EVENT_RE.search(line)
            k["event"] = _str(m2.group(1)) if m2 else None
            k["where"] = "%s/%s/%s[%d]" % (k["stage"], k["arc"], k["tag"], k["index"]) if k["tag"] else (
                "%s/%s" % (k["stage"], k["arc"]) if k["stage"] else "code")
            if k["kind"] == "Event" and k["event"]:
                k["where"] += ":" + k["event"]
            out.append(k)
    if not out:
        die("%s: no keys" % KEYS_INC)
    return out


def tag_layer(tag):
    c = tag[3]
    if c.isdigit():
        return int(c)
    if c in "ab":
        return 10 + ord(c) - ord("a")
    return -1


def room_of_arc(arc):
    return int(arc[4:]) if arc and arc.startswith("Room") else None


# ---------------------------------------------------------------------------------------------
# sweep_targets.json

def load_targets():
    if not os.path.isfile(TARGETS):
        die("%s missing (run `targets`)" % TARGETS)
    with open(TARGETS) as f:
        t = json.load(f)
    if t.get("format") != 1:
        die("%s: format %r, expected 1" % (TARGETS, t.get("format")))
    return t


def kind_of_key(kinds, key):
    """The first kind whose "key_kinds" (default ["Dzx"]) holds the key's KeyKind and whose "fields"
    (when given) hold its DzxField."""
    for name, k in kinds.items():
        if key["kind"] not in k.get("key_kinds", ["Dzx"]):
            continue
        if "fields" in k and key["field"] not in k["fields"]:
            continue
        return name
    return None


def fixed_stop(kcfg, stage, which):
    """A kind's hand-written stop for a key of `stage` ("stops" / "t2_stops": {stage or "*": stop})."""
    table = kcfg.get(which)
    if table is None:
        return None
    return table.get(stage or "*", table.get("*"))


def stop_str(stage, room, point, layer):
    return "%s:%d:%d" % (stage, room, point) + (":%d" % layer if layer >= 0 else "")


def parse_stop(s):
    p = s.split(":")
    return p[0], int(p[1]), int(p[2]), int(p[3]) if len(p) > 3 else -1


def _import(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def generate_targets(disc_path, old):
    """The "keys" list for every Dzx key of a kind in old["kinds"], from the disc."""
    room_sweep = _import("room_sweep", os.path.join(TOOLS, "room_sweep.py"))
    boot_sweep = room_sweep.boot_sweep
    manifest = boot_sweep.load_manifest(disc_path)
    _stages, _rooms, points, exits = room_sweep.spawn_points(manifest)
    disc = room_sweep.Disc(disc_path)
    kinds = old["kinds"]
    ov = old.get("overrides", {})
    spawn_ov = ov.get("spawn", {})
    key_ov = ov.get("keys", {})
    dz_cache = {}

    def dzx(stage, arc):
        if (stage, arc) not in dz_cache:
            files = disc.arc_files("/res/Stage/%s/%s.arc" % (stage, arc))
            dz_cache[(stage, arc)] = files["stage.dzs" if arc == "Stage" else "room.dzr"]
        return dz_cache[(stage, arc)]

    def entry(stage, arc, tag, index):
        raw = dzx(stage, arc)
        n = struct.unpack_from(">I", raw, 0)[0]
        size = {"TRE": 0x20, "ACT": 0x20, "SCO": 0x24}[tag[:3]]
        for i in range(n):
            t, count, off = struct.unpack_from(">4sII", raw, 4 + i * 12)
            if t.decode("latin1") == tag:
                if index >= count:
                    die("%s/%s %s[%d]: out of range" % (stage, arc, tag, index), 1)
                return raw[off + index * size:off + (index + 1) * size]
        die("%s/%s: no chunk %s" % (stage, arc, tag), 1)

    def spawn(stage, room):
        o = spawn_ov.get("%s:%d" % (stage, room))
        if o is not None:
            return o
        pts = points.get(stage, {}).get(room, [])
        if not pts:
            return None
        entries = [p for p in pts if (stage, room, p) in exits]
        return (entries or pts)[0]

    keys = load_keys()
    out = []
    for k in keys:
        kind = kind_of_key(kinds, k)
        if kind is None:
            continue
        kcfg = kinds[kind]
        if "stops" in kcfg:
            # Kinds whose smoke checks every key of a boot stage (or of the whole game): hand-written
            # stops per stage (code: one stop for all).
            row = {"key": k["key"], "loc": k["loc"], "kind": kind, "where": k["where"]}
            if k.get("event"):
                row["event"] = k["event"]
            row["t1"] = fixed_stop(kcfg, k["stage"], "stops")
            row["t2"] = fixed_stop(kcfg, k["stage"], "t2_stops") if kcfg.get("t2") else None
            for sel in (k["where"], k["loc"]):
                o = key_ov.get(sel)
                if o:
                    for f in ("t1", "t2", "events", "note", "xfail"):
                        if f in o:
                            row[f] = o[f]
            if not row.get("note") and row["t1"] is None:
                row["note"] = "no stop for stage %s in the kind's stops" % k["stage"]
            out.append(row)
            continue
        stage, arc, tag = k["stage"], k["arc"], k["tag"]
        layer = tag_layer(tag)
        room = room_of_arc(arc)
        if room is None and tag[:3] == "TRE":
            room = struct.unpack_from(">H", entry(stage, arc, tag, k["index"]), 0x18)[0] & 0x3F
        row = {"key": k["key"], "loc": k["loc"], "kind": kind, "where": k["where"], "room": room}
        pt = spawn(stage, room) if room is not None else None
        here = stop_str(stage, room, pt, layer) if pt is not None else None
        row["t2"] = here if kinds[kind].get("t2") else None
        if arc == "Stage":
            # Stage records are live from any room of the stage (on the record's layer): T1 runs from
            # the stage's first room with a spawn point, so one stop checks all of them.
            first = None
            for r in sorted(points.get(stage, {})):
                p = spawn(stage, r)
                if p is not None:
                    first = stop_str(stage, r, p, layer)
                    break
            row["t1"] = first
        else:
            row["t1"] = here
        for sel in (k["where"], k["loc"]):
            o = key_ov.get(sel)
            if o:
                for f in ("t1", "t2", "events", "note", "xfail"):
                    if f in o:
                        row[f] = o[f]
        if not row.get("note"):
            if row["t1"] is None:
                row["note"] = "no spawn point in the room"
            elif row["t2"] is None and kinds[kind].get("t2"):
                row["note"] = "no spawn point in room %s: no T2 stop" % room
        out.append(row)
    return out


def dump_targets(t):
    """sweep_targets.json text: the hand-written parts indented, one line per key."""
    head = {k: v for k, v in t.items() if k != "keys"}
    text = json.dumps(head, indent=1)
    rows = ",\n".join("  " + json.dumps(r) for r in t.get("keys", []))
    return text[:-2] + ',\n "keys": [\n' + rows + "\n ]\n}\n"


def cmd_targets(args):
    old = load_targets()
    if not os.path.isfile(args.disc):
        die("no disc image: pass --disc or set COS_DISC")
    new = dict(old)
    new["keys"] = generate_targets(args.disc, old)
    text = dump_targets(new)
    with open(TARGETS) as f:
        cur = f.read()
    if args.check:
        if cur != text:
            print("location-sweep: %s is stale (run `targets`)" % TARGETS, file=sys.stderr)
            return 1
        print("location-sweep: %s up to date (%d keys)" % (TARGETS, len(new["keys"])))
        return 0
    with open(TARGETS, "w") as f:
        f.write(text)
    print("location-sweep: wrote %s (%d keys)" % (TARGETS, len(new["keys"])))
    return 0


# ---------------------------------------------------------------------------------------------
# Selection and runs

def resolve_loc(name, locs):
    if name.isdigit():
        i = int(name)
        if 0 <= i < len(locs):
            return locs[i]["enum"]
    full = name if name.startswith("PCR_LOC_") else "PCR_LOC_" + name
    for l in locs:
        if l["enum"] == full:
            return full
    die("unknown location %r" % name)


def select(args, targets, locs):
    kinds = targets["kinds"]
    want_kinds = set(args.kinds.split(",")) if args.kinds else set(kinds)
    for k in want_kinds:
        if k not in kinds:
            die("unknown kind %r (sweep_targets.json kinds: %s)" % (k, ", ".join(kinds)))
    want_locs = {resolve_loc(x, locs) for x in args.locs.split(",")} if args.locs else None
    want_stages = set(args.stages.split(",")) if args.stages else None
    rows = []
    for r in targets["keys"]:
        if r["kind"] not in want_kinds:
            continue
        if want_locs is not None and r["loc"] not in want_locs:
            continue
        if want_stages is not None and r["where"].split("/")[0] not in want_stages:
            continue
        rows.append(r)
    return rows, want_kinds, want_locs, want_stages


def placement_path(p):
    if p == "none":
        return None
    if p in PLACEMENTS:
        return os.path.join(FIXTURES, PLACEMENTS[p])
    path = os.path.abspath(p)
    if not os.path.isfile(path):
        die("no placement %s" % p)
    return path


class Run:
    """One run.sh invocation: its stops (loc smoke) or its boot stop, and the keys it covers."""

    def __init__(self, kind, stops, rows, tier):
        self.kind, self.stops, self.rows, self.tier = kind, stops, rows, tier
        self.name = ""
        self.opens = []    # T2: locations to open
        self.expect = []   # locations whose chest keys are all checked by this run
        self.events = []
        self.env = []      # exit/lines modes: (name, value) pairs of this run


def plan_loc_runs(rows, tier, max_stops):
    """Batches of stops for the chest kind, per stage in name order, at most max_stops per run
    (a stage is never split)."""
    by_stage = collections.OrderedDict()
    for r in sorted(rows, key=lambda r: (r["where"].split("/")[0], r["key"])):
        stop = r.get("t1") if tier == "T1" else r.get("t2")
        if stop is None:
            continue
        by_stage.setdefault(stop.split(":")[0], []).append((stop, r))
    runs = []
    cur = None
    for stage, items in by_stage.items():
        stops = []
        for stop, _ in items:
            if stop not in stops:
                stops.append(stop)
        stops.sort(key=lambda s: parse_stop(s)[1:])
        if cur is None or len(cur.stops) + len(stops) > max_stops:
            cur = Run("chest", [], [], tier)
            runs.append(cur)
        cur.stops.extend(stops)
        cur.rows.extend(r for _, r in items)
    for run in runs:
        locs_all = collections.defaultdict(list)
        for r in run.rows:
            locs_all[r["loc"]].append(r)
            for e in r.get("events", []):
                if e not in run.events:
                    run.events.append(e)
        if tier == "T2":
            run.opens = list(locs_all)
        run.expect = list(locs_all)
    return runs


def expect_complete(run, all_rows):
    """Only the locations all of whose chest keys this run's stops reach (T1)."""
    if run.tier != "T1":
        return []
    out = []
    for loc in run.expect:
        mine = [r for r in all_rows if r["loc"] == loc and r["kind"] == "chest"]
        if all(r.get("t1") in run.stops for r in mine):
            out.append(loc)
    return out


def plan_runs(args, targets, rows):
    kinds = targets["kinds"]
    tiers = {"t1": ["T1"], "t2": ["T2"], "both": ["T1", "T2"]}[args.tier]
    all_chest = [r for r in targets["keys"] if r["kind"] == "chest"]
    runs = []
    for tier in tiers:
        for kind in kinds:
            kr = [r for r in rows if r["kind"] == kind]
            if not kr:
                continue
            mode = kinds[kind]["mode"]
            if tier == "T2" and not kinds[kind].get("t2"):
                continue
            if mode == "loc":
                for run in plan_loc_runs(kr, tier, args.max_stops):
                    run.expect = expect_complete(run, all_chest)
                    runs.append(run)
            elif mode in ("exit", "lines"):
                # One run per stop (and per-row env, e.g. the event T2 plays): a stop "a,b,c" boots at
                # a and hops to b and c (the kind's "hops_env").
                by_stop = collections.OrderedDict()
                for r in kr:
                    stop = r.get("t1") if tier == "T1" else r.get("t2")
                    if not stop:
                        continue
                    env = tuple((n, v.format(**r)) for n, v in
                                sorted(kinds[kind].get("t2_env" if tier == "T2" else "t1_env", {}).items()))
                    by_stop.setdefault((stop, env), []).append(r)
                for (stop, env), rs in by_stop.items():
                    run = Run(kind, [stop], rs, tier)
                    run.env = list(env)
                    runs.append(run)
            elif mode == "rooms":
                rs = [r for r in kr if r.get("t1")]
                if rs:
                    runs.append(Run(kind, sorted({r["t1"] for r in rs}, key=lambda s: parse_stop(s)[1]), rs, tier))
            else:
                die("kind %s: unknown mode %r" % (kind, mode))
    for i, run in enumerate(runs):
        run.name = "%02d-%s-%s-%s" % (i, run.tier.lower(), run.kind,
                                      run.stops[0].split(",")[0].replace(":", "_") if run.stops else "none")
    return runs


def run_args(run, kinds, placement, rel_placement=False):
    """run.sh options (after the target) for a run."""
    k = kinds[run.kind]
    env = [("COS_RANDO_SMOKE", k["smoke"])]
    if placement is not None:
        env.append(("COS_RANDO_PLACEMENT", os.path.relpath(placement, REPO) if rel_placement else placement))
    opts = list(k.get("args", []))
    if k["mode"] == "loc":
        timeout = 120 + 6 * len(run.stops) + 90 * len(run.opens)
        opts = ["--stage", run.stops[0], "--timeout", str(timeout), "--stall", "120"] + opts
        if len(run.stops) > 1:
            env.append(("COS_RANDO_LOC_HOPS", ",".join(run.stops[1:])))
        if run.opens:
            env.append(("COS_RANDO_LOC_OPEN", ",".join(l[len("PCR_LOC_"):] for l in run.opens)))
        if run.expect:
            env.append(("COS_RANDO_LOC_EXPECT", ",".join(l[len("PCR_LOC_"):] for l in run.expect)))
        if run.events:
            env.append(("COS_RANDO_LOC_EVENTS", ",".join(run.events)))
    elif k["mode"] in ("exit", "lines"):
        parts = run.stops[0].split(",")
        opts = ["--stage", parts[0]] + (["--timeout", str(k["timeout"])] if "timeout" in k else []) + opts
        if len(parts) > 1:
            env.append((k["hops_env"], ",".join(parts[1:])))
        env.extend(run.env)
    elif k["mode"] == "rooms":
        env.append((k["rooms_env"], ",".join(str(parse_stop(s)[1]) for s in run.stops)))
        opts = opts + ["--timeout", str(120 + 4 * len(run.stops))]
    for n, v in k.get("env", {}).items():
        env.append((n, v))
    for n, v in env:
        opts += ["--env", "%s=%s" % (n, v)]
    return opts


# Check lines of the loc smoke (pc_rando_smoke_loc.cpp).
_P = r"\[cos\] rando-smoke loc: "
RE_STOP = re.compile(_P + r"stop (\d+): stage (\S+), stay room (-?\d+), layer (-?\d+)")
RE_REC = re.compile(_P + r"T1 (PCR_LOC_\w+) (\S+)/(\S+) (\w{4})\[(\d+)\]: record holds 0x([0-9A-F]{2}): (ok|FAIL)$")
RE_LIVE = re.compile(_P + r"T1 (PCR_LOC_\w+) (\S+)/(\S+) (\w{4})\[(\d+)\]: live chest at \(([^)]*)\) holds 0x([0-9A-F]{2}): (ok|FAIL)$")
RE_T2 = re.compile(_P + r"T2 (PCR_LOC_\w+): (chest opened and gave 0x[0-9A-F]{2}|its chest was live at a stop)(?:: (.*))?: (ok|FAIL)$")
RE_REACH = re.compile(_P + r"stop (\d+) \((\S+)\) reached: FAIL$")
RE_DETAIL = re.compile(_P + r"  (record|the chest holds)(.*)$")
RE_END = re.compile(r"\[cos\] rando-smoke (\w+): (PASS|FAIL) \((\d+) failed\)")


def read_log(run_dir):
    p = os.path.join(run_dir, "run.log")
    if not os.path.isfile(p):
        return ""
    with open(p, errors="replace") as f:
        return f.read()


def run_rc(run_dir):
    try:
        with open(os.path.join(run_dir, "exit_code.txt")) as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return -1


def crash_line(run_dir, log):
    bt = os.path.join(run_dir, "backtrace.txt")
    if os.path.isfile(bt):
        with open(bt, errors="replace") as f:
            for line in f:
                line = line.strip()
                if line:
                    return line[:160]
    for line in reversed(log.splitlines()):
        if "PANIC" in line or "SIG" in line:
            return line.strip()[:160]
    return ""


def results_loc(run, log, rc, run_dir):
    """{(key, tier): (result, stop, detail)} from a loc smoke log."""
    res = {}
    stop_idx = -1
    stop_seen = set()
    stop_missed = set()
    landed = {}  # stop index -> (stage, stay room, layer) the stop came up in
    last_fail_detail = ""
    for line in log.splitlines():
        m = RE_REACH.search(line)
        if m:
            stop_missed.add(int(m.group(1)))
            continue
        m = RE_STOP.search(line)
        if m:
            stop_idx = int(m.group(1))
            stop_seen.add(stop_idx)
            landed[stop_idx] = (m.group(2), int(m.group(3)), int(m.group(4)))
            continue
        m = RE_DETAIL.search(line)
        if m:
            last_fail_detail = (m.group(1) + m.group(2)).strip()
            continue
        for rx, tier in ((RE_REC, "T1-record"), (RE_LIVE, "T1-live")):
            m = rx.search(line)
            if m:
                where = "%s/%s/%s[%s]" % (m.group(2), m.group(3), m.group(4), m.group(5))
                ok = m.groups()[-1]
                stop = run.stops[stop_idx] if 0 <= stop_idx < len(run.stops) else "?"
                prev = res.get((where, tier))
                detail = "" if ok == "ok" else last_fail_detail
                if prev is None or prev[0] == "ok" and ok == "FAIL":
                    res[(where, tier)] = (ok, stop, detail)
                last_fail_detail = ""
                break
        m = RE_T2.search(line)
        if m:
            res[(m.group(1), "T2")] = (m.group(4), run.stops[stop_idx] if 0 <= stop_idx < len(run.stops) else "?",
                                      m.group(3) or ("" if m.group(4) == "ok" else m.group(2)))
    out = {}
    crashed = rc not in (0, 1)
    why = crash_line(run_dir, log) if crashed else ""
    for r in run.rows:
        stop = r["t1"] if run.tier == "T1" else r["t2"]
        reached = stop in run.stops and run.stops.index(stop) in stop_seen
        tiers = ["T1-record", "T1-live"] if run.tier == "T1" else ["T2"]
        for tier in tiers:
            key = (r["where"], tier) if tier != "T2" else (r["loc"], "T2")
            if key in res:
                out[(r["key"], tier)] = res[key]
            elif not reached and stop in run.stops and run.stops.index(stop) in stop_missed:
                out[(r["key"], tier)] = ("not-reached", stop, "the stop did not come up")
            elif not reached:
                out[(r["key"], tier)] = ("not-reached", stop, "exit %d%s" % (rc, (": " + why) if why else ""))
            else:
                st, room, _pt, _layer = parse_stop(stop)
                got = landed.get(run.stops.index(stop))
                detail = ""
                if got is not None and (got[0] != st or got[1] != room):
                    detail = "the stop came up in %s room %d (layer %d)" % got
                out[(r["key"], tier)] = ("not-checked", stop, detail)
    return out


def results_exit(run, log, rc, run_dir):
    ok = "ok" if rc == 0 else "FAIL"
    detail = "" if rc == 0 else "exit %d" % rc
    if rc not in (0, 1):
        detail += ": " + crash_line(run_dir, log)
    elif rc == 1:
        fails = [l.split("rando-smoke ", 1)[1] for l in log.splitlines() if l.endswith(": FAIL") and "rando-smoke " in l]
        if fails:
            detail += ": " + fails[0][:160]
    return {(r["key"], run.tier): (ok, r["t1"] if run.tier == "T1" else r["t2"], detail) for r in run.rows}


def results_lines(run, log, rc, run_dir, kind_cfg):
    """Per-location check lines (kind "loc_line": groups tier, loc, result): a location's rows get FAIL
    when any of its lines of the run's tier failed, ok when all held, else not-checked (or not-reached
    after a crash or timeout)."""
    rx = re.compile(kind_cfg["loc_line"])
    per_loc = {}
    first_fail = {}
    for line in log.splitlines():
        m = rx.search(line)
        if not m or m.group("tier") != run.tier:
            continue
        loc, res = m.group("loc"), m.group("res")
        if per_loc.get(loc) != "FAIL":
            per_loc[loc] = res
        if res == "FAIL" and loc not in first_fail:
            first_fail[loc] = line.split("rando-smoke ", 1)[-1][:160]
    out = {}
    crashed = rc not in (0, 1)
    why = crash_line(run_dir, log) if crashed else ""
    for r in run.rows:
        stop = r["t1"] if run.tier == "T1" else r["t2"]
        if r["loc"] in per_loc:
            out[(r["key"], run.tier)] = (per_loc[r["loc"]], stop, first_fail.get(r["loc"], ""))
        elif crashed:
            out[(r["key"], run.tier)] = ("not-reached", stop, "exit %d%s" % (rc, (": " + why) if why else ""))
        else:
            out[(r["key"], run.tier)] = ("not-checked", stop, "no check line for the location (exit %d)" % rc)
    return out


def results_rooms(run, log, rc, run_dir, kind_cfg):
    rx = re.compile(kind_cfg["room_line"])
    per_room = {}
    for line in log.splitlines():
        m = rx.search(line)
        if m:
            room, res = int(m.group(1)), m.group(2)
            if per_room.get(room) != "FAIL":
                per_room[room] = res
    out = {}
    why = crash_line(run_dir, log) if rc not in (0, 1) else ""
    for r in run.rows:
        room = parse_stop(r["t1"])[1]
        if room in per_room:
            out[(r["key"], "T1")] = (per_room[room], r["t1"], "")
        else:
            out[(r["key"], "T1")] = ("not-reached", r["t1"], "exit %d%s" % (rc, (": " + why) if why else ""))
    return out


def execute(run, args, kinds, placement, sweep_dir):
    run_dir = os.path.join(sweep_dir, run.name)
    cmd = [RUN_SH, "rando"] + run_args(run, kinds, placement) + ["--disc", args.disc, "--quiet", "--run-dir", run_dir]
    t0 = time.time()
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=REPO)
    dt = time.time() - t0
    rc = run_rc(run_dir)
    log = read_log(run_dir)
    mode = kinds[run.kind]["mode"]
    if mode == "loc":
        res = results_loc(run, log, rc, run_dir)
    elif mode == "exit":
        res = results_exit(run, log, rc, run_dir)
    elif mode == "lines":
        res = results_lines(run, log, rc, run_dir, kinds[run.kind])
    else:
        res = results_rooms(run, log, rc, run_dir, kinds[run.kind])
    bad = sum(1 for v in res.values() if v[0] != "ok")
    print("location-sweep: %s: %d stop(s), exit %d, %.0f s, %d/%d check(s) ok" % (
        run.name, len(run.stops), rc, dt, len(res) - bad, len(res)), flush=True)
    return rc, res


def cmd_run(args):
    targets = load_targets()
    locs = load_locations()
    kinds = targets["kinds"]
    rows, want_kinds, want_locs, want_stages = select(args, targets, locs)
    placement = placement_path(args.placement)
    runs = plan_runs(args, targets, rows)
    if args.dry:
        for run in runs:
            print("%s: run.sh rando %s" % (run.name, " ".join(run_args(run, kinds, placement))))
        return 0
    if not os.path.isfile(args.disc):
        die("no disc image: pass --disc PATH or set COS_DISC")
    exe = os.path.join(REPO, os.environ.get("COS_BUILD_DIR") or os.path.join("build", "native-mac"), "switchwaker")
    if os.path.isfile(exe):
        print("location-sweep: %s built %s" % (os.path.relpath(exe, REPO), time.strftime("%Y-%m-%d %H:%M:%S",
              time.localtime(os.path.getmtime(exe)))), flush=True)
    sweep_dir = args.sweep_dir or os.path.join(REPO, "build", "rando-sweep", "runs-" + time.strftime("%Y%m%d-%H%M%S"))
    os.makedirs(sweep_dir, exist_ok=True)
    results = {}
    run_of = {}
    queue = list(runs)
    retried = 0
    while queue:
        run = queue.pop(0)
        rc, res = execute(run, args, kinds, placement, sweep_dir)
        for k, v in res.items():
            results[k] = v
            run_of[k] = run.name
        if args.retry and kinds[run.kind]["mode"] == "loc" and len(run.stops) > 1:
            # A hop that came up in another room (two neighbouring sea rooms: the player stands where
            # the old room is still loaded) gets a run of its own that boots at that stop.
            wrong = []
            for (key, tier), v in res.items():
                if v[0] == "not-checked" and v[2].startswith("the stop came up in") and v[1] not in wrong:
                    wrong.append(v[1])
            for stop in wrong:
                rs = [r for r in run.rows if (r["t1"] if run.tier == "T1" else r["t2"]) == stop]
                nr = Run(run.kind, [stop], rs, run.tier)
                nr.opens = [l for l in run.opens if any(r["loc"] == l for r in rs)]
                nr.expect = [l for l in run.expect if any(r["loc"] == l for r in rs) and
                             all(r2.get("t1") == stop for r2 in run.rows if r2["loc"] == l)]
                nr.events = run.events
                nr.name = "%s.boot%d" % (run.name, retried)
                retried += 1
                queue.append(nr)
        if args.retry and kinds[run.kind]["mode"] == "loc" and rc not in (0, 1):
            # A crash or timeout: each stop the run did not reach (but the first, which is blamed)
            # gets its own run.
            unreached = []
            for (key, tier), v in res.items():
                if v[0] == "not-reached" and v[1] not in unreached:
                    unreached.append(v[1])
            for stop in unreached[1:]:
                rs = [r for r in run.rows if (r["t1"] if run.tier == "T1" else r["t2"]) == stop]
                nr = Run(run.kind, [stop], rs, run.tier)
                nr.opens = [l for l in run.opens if any(r["loc"] == l for r in rs)]
                nr.events = run.events
                nr.name = "%s.retry%d" % (run.name, retried)
                retried += 1
                queue.append(nr)
    bad = write_report(args, targets, locs, rows, want_kinds, want_locs, want_stages, results, run_of, placement)
    return 0 if bad == 0 else 1


def write_report(args, targets, locs, rows, want_kinds, want_locs, want_stages, results, run_of, placement):
    loc_by_enum = {l["enum"]: l for l in locs}
    tiers = {"t1": ["T1"], "t2": ["T2"], "both": ["T1", "T2"]}[args.tier]
    kinds = targets["kinds"]
    lines = ["\t".join(TSV_COLUMNS)]
    per_loc = collections.OrderedDict()
    counts = collections.Counter()
    for r in rows:
        k_tiers = []
        for t in tiers:
            if t == "T1":
                k_tiers += ["T1-record", "T1-live"] if kinds[r["kind"]]["mode"] == "loc" else ["T1"]
            elif kinds[r["kind"]].get("t2"):
                k_tiers.append("T2")
        for t in k_tiers:
            v = results.get((r["key"], t))
            stop = r.get("t1") if t != "T2" else r.get("t2")
            if v is None:
                v = ("no-target", "", r.get("note", "")) if stop is None else ("not-reached", stop, "not run")
            xf = r.get("xfail", {}).get(t[:2])
            if xf and v[0] != "ok":
                # A listed, reasoned expected failure (sweep_targets.json overrides.keys "xfail"), also
                # for a key without a stop for that tier (no-target).
                v = ("xfail", v[1], "%s (%s: %s)" % (xf, v[0], v[2]))
            l = loc_by_enum[r["loc"]]
            lines.append("\t".join(str(x) for x in [l["id"], r["loc"], l["class"], r["kind"], r["key"], r["where"], t,
                                                    v[1] or "", run_of.get((r["key"], t), ""), v[0], v[2]]))
            counts[(t, v[0])] += 1
            per_loc.setdefault(r["loc"], []).append(v[0])
    # Locations of the selection that no sweep kind covers.
    covered = {r["loc"] for r in targets["keys"]}
    nt = 0
    if not args.kinds:
        for l in locs:
            if l["enum"] in covered:
                continue
            if want_locs is not None and l["enum"] not in want_locs:
                continue
            if want_stages is not None:
                continue
            lines.append("\t".join(str(x) for x in [l["id"], l["enum"], l["class"], "", "", "", "", "", "", "no-target",
                                                    "no sweep kind for this location class yet"]))
            nt += 1
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    n_ok = sum(1 for v in per_loc.values() if all(x == "ok" for x in v))
    print("location-sweep: placement %s" % (os.path.relpath(placement, REPO) if placement else "none (vanilla)"))
    for t in sorted({t for t, _ in counts}):
        print("location-sweep:   %-9s %s" % (t, ", ".join("%s %d" % (res, counts[(t, res)])
                                                       for res in ("ok", "FAIL", "not-reached", "not-checked", "xfail",
                                                                   "no-target")
                                                       if counts[(t, res)])))
    print("location-sweep: %d/%d locations ok%s; report %s" % (
        n_ok, len(per_loc), (", %d without a sweep kind" % nt) if nt else "", args.out))
    bad = [(r, v) for r in lines[1:] for v in [r.split("\t")] if v[9] not in ("ok", "no-target")]
    for _, v in bad[:40]:
        print("location-sweep:   %s %s %s %s: %s %s" % (v[6], v[1], v[5], v[7], v[9], v[10]))
    if len(bad) > 40:
        print("location-sweep:   ... %d more (see the report)" % (len(bad) - 40))
    # Failures: every row not ok, not a listed xfail, and not a row without a stop (no-target rows
    # of a kind without T2 are not selected at all); a location without any sweep kind counts too.
    return sum(1 for _, v in bad if v[9] != "xfail") + nt


def cmd_line(args):
    targets = load_targets()
    locs = load_locations()
    rows, _, _, _ = select(args, targets, locs)
    placement = placement_path(args.placement)
    for run in plan_runs(args, targets, rows):
        print("rando 0 " + " ".join(run_args(run, targets["kinds"], placement, rel_placement=True)))
    return 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    ap.add_argument("cmd", nargs="?", default="run", choices=["run", "targets", "line"])
    ap.add_argument("--placement", default="rotation")
    ap.add_argument("--tier", default="t1", choices=["t1", "t2", "both"])
    ap.add_argument("--kinds")
    ap.add_argument("--locs")
    ap.add_argument("--stages")
    ap.add_argument("--max-stops", type=int, default=30)
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--sweep-dir")
    ap.add_argument("--disc", default=os.environ.get("COS_DISC") or DEFAULT_DISC)
    ap.add_argument("--dry", action="store_true")
    ap.add_argument("--no-retry", dest="retry", action="store_false")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)
    args.disc = os.path.abspath(os.path.expanduser(args.disc))
    if args.max_stops < 1:
        die("--max-stops must be at least 1")
    if args.cmd == "targets":
        return cmd_targets(args)
    if args.cmd == "line":
        return cmd_line(args)
    return cmd_run(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
