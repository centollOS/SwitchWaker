#!/usr/bin/env python3
"""Fight every enemy, and play every boss, mini-boss and minigame room, for crash detection and to
record the pipelines combat uses (COS_SMOKE=combat-sweep, native/src/pc/harness/sweeps/pc_combat_sweep.cpp).

  native/tools/combat_sweep.py enemies [options]     every enemy spawned in front of the player
  native/tools/combat_sweep.py rooms [options]       every boss / mini-boss / minigame room
  native/tools/combat_sweep.py list-enemies|list-rooms   print the cases and exit
  native/tools/combat_sweep.py pipelines --bundled DB SWEEP_DIR...   the new-pipelines table

Every run is `run.sh combat-sweep ... --uncapped` with COS_CACHE_PER_RUN=1, so each run directory
holds cache/pipeline_cache.db: the pipelines that run used (gen_pipeline_cache.sh --merge-from).

enemies: the enemy actors come from the code, not a hand list: every actor profile whose group is
fopAc_ENEMY_e (game/src/d/actor/*.cpp, the profile's "Group" field), plus the actors that use the
enemy freeze/burn helpers (enemy_ice/enemy_fire) whatever their group (morths, kargarocs...);
except the plain actors and NPCs among them (Zelda in Ganon's tower), so the morths and the pigs
(group fopAc_ENV_e); their dStage names come from d_stage.cpp's OBJNAME table. Each name is spawned with the parameters
and angle x/z of its placements on the disc (every ACTR/ACT0-b/TGOB record of every stage and
room): the most common one and up to --variants-1 more with another low byte (the type field of
most enemies); a name the disc never places is spawned with parameters 0. One run per case (so the
pipelines are attributed to that enemy), --jobs at a time; a fault ends only that case's run.
Defaults: --stage M_NewD2:0:0 (the first dungeon's entrance: the smoke spawns on the first of eight
directions with floor at the player's height; the Outset pier, sea:44:0, drowns him), --frames
1200 per case, items bombs (X), sword, shield, bow
(Y), hookshot (Z), an empty bottle (the smoke puts a fairy in it). --death adds one case at the end (the first mo2 variant, a moblin)
with COS_COMBAT_DEATH=1: the player is left to die (fairy revival, then game over).
--home boots each case in the room of the disc placement its parameters came from instead (at
that room's spawn point, as room_sweep.py picks it, or for a placement on one layer that layer
at the spawn point nearest to it; with ROOM_SETUP's event bits for the stage; --stage for an unplaced name or a room without
a spawn point), so the enemy is fought under its own stage's lights and fog.

rooms: the rooms come from the disc's stage list and the code: (1) every room of the stages whose
name marks a boss or mini-boss stage (B, BOSS or MB suffix, Xboss*, Ganon*, GTower, M2tower,
M2ganon) that holds an enemy actor (or, but for the B suffix, any room of such a stage when none
does); (2) every room
that places an actor whose code runs a minigame (dComIfGp_startMiniGame, MiniGameRupee,
dMinigame_Starter_c, dTimer_create*, MiniGameInit, TYPE_MINIGAME); (3) EXTRA_ROOMS (Mrs. Marie's
school, asked for, found by no rule). Spawn point: as room_sweep.py
picks it (an SCLS entry when there is one), except where a boss (an actor whose code calls
onStageBossEnemy/onStageBossDemo) is placed on one layer only: that layer, at the spawn point
nearest to it (Helmaroc on M2tower's layer 3). Each room is run with COS_COMBAT_MODE=room for
--frames frames (default 6000) of the combat cycle against the nearest enemy, a screenshot every
--shot-every frames (default 1000). ROOM_SETUP below adds story event bits (COS_BOOT_EVENTS)
where a boss needs them to appear.

Options (both): --jobs N (default 2), --timeout S per run (default 600), --only LIST (names or
stage:room), --disc PATH, --exe PATH, --out DIR (default build/combat-sweep/<mode>-<timestamp>),
--items LIST (COS_BOOT_ITEMS).
Report: <out>/combat_sweep.tsv and .md: per case the result (died, timeout, refused,
self-deleted, left-stage, done, FAULT with the boot_sweep.py signature, xfail for ROOM_EXPECTED),
the smoke's counters,
the number of pipelines in its cache and the high-water marks of Aurora's fixed staging buffers
(KiB of vertices/uniforms/indices/storage of 5120/24576/2048/8192: a frame past one aborts). Exit 0 if no run faulted, 1 otherwise.

pipelines: for each run directory under the given sweep directories, the pipelines of its
cache/pipeline_cache.db that the bundled file (--bundled, default
build/pipeline-cache/initial_pipeline_cache.db) lacks, keyed (type, hash); also how many of them
no other run of the sweeps had. Written to <first sweep dir>/new_pipelines.tsv and printed, top
contributors first.
Nothing here is meant for git.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures
import glob
import importlib.util
import os
import re
import sqlite3
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))
COS_RUN = os.path.join(SCRIPT_DIR, "run.sh")
ACTOR_DIR = os.path.join(REPO, "game", "src", "d", "actor")
D_STAGE = os.path.join(REPO, "game", "src", "d", "d_stage.cpp")
DEFAULT_ITEMS = "31,38,3B,27,2F,50"

_spec = importlib.util.spec_from_file_location("boot_sweep", os.path.join(SCRIPT_DIR, "boot_sweep.py"))
boot_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(boot_sweep)
_spec = importlib.util.spec_from_file_location("room_sweep", os.path.join(SCRIPT_DIR, "room_sweep.py"))
room_sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(room_sweep)

# Boss rooms whose boss needs story event bits to be there (COS_BOOT_EVENTS), or other per-room
# setup: stage or (stage, room) -> {env}.
ROOM_SETUP: dict = {
    # Event flag 0x2D01 (set by M2tower's rescue.stb): d_s_play.cpp mounts the player's demo
    # animations LkD01.arc only with it, and these stages' cutscenes ask for LkD01 files
    # (boot_sweep.py's _LKD01: without it the debug boot stops in J3DAnmTexPattern).
    "GTower": {"COS_BOOT_EVENTS": "2D01"},
    "M2ganon": {"COS_BOOT_EVENTS": "2D01"},
    "GanonK": {"COS_BOOT_EVENTS": "2D01"},
}

# Boss and mini-boss stages by name: a B suffix (M_DragB, SirenB, kazeB...) only counts with an enemy in
# the room (figureB, KATA_HB are not boss stages); the others count even without one (the boss
# of M2ganon, GanonK... comes with an event).
# (stage, room) -> (signature regex, reason): runs whose failure is not a port bug.
_GANONC = ("leftover rooms: GanonC's doors link no room (TGDR angle x 4095, rooms 63/63) and no SCLS "
           "exit leads to rooms 1-5, so the game never enters them; their Phantom Ganon throws energy "
           "balls whose hit effect (0x8245, ID_AK_SN_BPGHITDARKSHOT00) is in Pscene080 (GanonJ, the "
           "real maze) but not in GanonC's Pscene084, and d_a_fgmahou.cpp uses the NULL emitter "
           "unchecked (a NULL access on the GameCube too)")
_E3ROOP = ("leftover E3 stage without a camera record (room_sweep.py ROOM_EXPECTED_FAIL): the PLAY "
           "scene never finishes creating")
ROOM_EXPECTED = {
    ("GanonC", 1): (r"addr=0x2a4 .* in C_MTXIdentity", _GANONC),
    ("GanonC", 2): (r"addr=0x2a4 .* in C_MTXIdentity", _GANONC),
    ("GanonC", 3): (r"addr=0x2a4 .* in C_MTXIdentity", _GANONC),
    ("GanonC", 4): (r"addr=0x2a4 .* in C_MTXIdentity", _GANONC),
    ("E3ROOP", 0): (r"TIMEOUT|timeout|state: scene=ROOM_SCENE", _E3ROOP),
}

BOSS_STAGE = re.compile(r"^(?:.*(?:B|BOSS|MB)|Xboss\d|Ganon[A-Z]|GTower|M2tower|M2ganon)$")
BOSS_STAGE_STRONG = re.compile(r"^(?:.*(?:BOSS|MB)|Xboss\d|Ganon[A-Z]|GTower|M2tower|M2ganon)$")
# Rooms asked for by name that no code rule finds: Mrs. Marie's school on Windfall (d_a_npc_ho).
EXTRA_ROOMS = [("Nitiyou", 0, "requested:Ho")]
MINIGAME_CODE = re.compile(r"dComIfGp_startMiniGame|MiniGameRupee|dMinigame_Starter_c|"
                           r"dTimer_create|MiniGameInit|TYPE_MINIGAME")
# Code that refers to minigames only to stay out of them.
MINIGAME_EXCLUDE = {"d_a_player_main.cpp", "d_a_ship.cpp", "d_a_arrow.cpp"}


def actor_profiles():
    """{file: [(profile, procname enum, group)]} from game/src/d/actor/*.cpp."""
    out = {}
    for path in sorted(glob.glob(os.path.join(ACTOR_DIR, "*.cpp"))):
        text = boot_sweep.read(path)
        profs = []
        for m in re.finditer(r"actor_process_profile_definition2? g_profile_(\w+) = \{(.*?)\n\};",
                             text, re.S):
            body = m.group(2)
            pn = re.search(r"/\* Proc Name\s*\*/\s*(fpcNm_\w+)", body)
            gr = re.search(r"/\* Group\s*\*/\s*(\w+)", body)
            if pn:
                profs.append((m.group(1), pn.group(1), gr.group(1) if gr else "?"))
        out[os.path.basename(path)] = profs
    return out


def object_names():
    """{fpcNm enum: [dStage names]} from d_stage.cpp's OBJNAME table."""
    names = collections.defaultdict(list)
    for m in re.finditer(r'OBJNAME\("([^"]+)",\s*(fpcNm_\w+)', boot_sweep.read(D_STAGE)):
        names[m.group(2)].append(m.group(1))
    return names


def placements(manifest):
    """{dStage name: [(params, angle x, angle z, stage, room, layer or None, pos)]} over every stage
    and room file (layer: the record's ACTn/SCOn/TGOn/... tag)."""
    out = collections.defaultdict(list)
    for r in manifest["files"]:
        m = boot_sweep.ROOM_ARC.match(r["path"]) or boot_sweep.STAGE_ARC.match(r["path"])
        if not m:
            continue
        dz = boot_sweep.stage_file(r)
        if not dz:
            continue
        room = int(m.group(2)) if m.re is boot_sweep.ROOM_ARC else -1
        for tag, lst in dz.get("actors", {}).items():
            lm = re.fullmatch(r"(?:ACT|SCO|TGO|TRE|DOO)([0-9a-b])", tag)
            layer = int(lm.group(1), 16) if lm else None
            for a in lst:
                ang = a.get("angle") or [0, 0, 0]
                out[a["name"]].append((a["params"] & 0xFFFFFFFF, ang[0], ang[2], m.group(1), room,
                                       layer, a.get("pos")))
    return out


def enemy_cases(manifest, variants):
    """[(name, params, angle x, angle z, why, [(stage, room) of its placements])]."""
    profs = actor_profiles()
    names = object_names()
    procs = []  # (enum, why)
    seen = set()
    for f, lst in profs.items():
        for prof, enum, group in lst:
            if group == "fopAc_ENEMY_e" and enum != "fpcNm_PLAYER_e" and enum not in seen:
                procs.append((enum, "group"))
                seen.add(enum)
    for f, lst in profs.items():
        text = boot_sweep.read(os.path.join(ACTOR_DIR, f))
        if re.search(r"\benemy_(?:ice|fire)\(", text):
            for prof, enum, group in lst:
                if enum not in seen and group not in ("fopAc_ACTOR_e", "fopAc_NPC_e"):
                    procs.append((enum, "enemy_ice"))
                    seen.add(enum)
    places = placements(manifest)
    cases = []
    for enum, why in procs:
        for name in names.get(enum, []):
            pl = places.get(name, [])
            if not pl:
                cases.append((name, 0, 0, 0, why + ",unplaced", []))
                continue
            count = collections.Counter((p, ax, az) for p, ax, az, *_ in pl)
            chosen, lows = [], set()
            for (p, ax, az), _ in count.most_common():
                if len(chosen) >= variants:
                    break
                if chosen and (p & 0xFF) in lows:
                    continue
                chosen.append((p, ax, az))
                lows.add(p & 0xFF)
            for p, ax, az in chosen:
                homes = [(st, rm, ly, pos) for pp, x, z, st, rm, ly, pos in pl if (pp, x, z) == (p, ax, az)]
                cases.append((name, p, ax, az, why, homes))
        if not names.get(enum):
            print("combat_sweep: %s has no dStage name; skipped" % enum, file=sys.stderr)
    return cases


def room_cases(manifest):
    """[(stage, room, point, layer or None, why)] for the boss, mini-boss and minigame rooms."""
    profs = actor_profiles()
    names = object_names()
    enemy_names, minigame_names, boss_names = set(), {}, set()
    for f, lst in profs.items():
        text = boot_sweep.read(os.path.join(ACTOR_DIR, f))
        ice = re.search(r"\benemy_(?:ice|fire)\(", text)
        mg = f not in MINIGAME_EXCLUDE and MINIGAME_CODE.search(text)
        boss_code = re.search(r"onStageBoss(?:Enemy|Demo)\(", text)
        for prof, enum, group in lst:
            if boss_code:
                boss_names.update(names.get(enum, []))
            if group == "fopAc_ENEMY_e" or ice:
                enemy_names.update(names.get(enum, []))
            if mg:
                for n in names.get(enum, []):
                    minigame_names[n] = f
    stages, rooms, points, exits = room_sweep.spawn_points(manifest)
    by_path = {r["path"]: r for r in manifest["files"]}
    room_actors = {}
    boss_layer = {}  # (stage, room) -> (layer, boss position): a boss placed on one layer only
    for stage in stages:
        for n in rooms.get(stage, []):
            dz = boot_sweep.stage_file(by_path["/res/Stage/%s/Room%d.arc" % (stage, n)]) or {}
            room_actors[(stage, n)] = {a["name"] for lst in dz.get("actors", {}).values() for a in lst}
            for tag, lst in dz.get("actors", {}).items():
                lm = re.fullmatch(r"(?:ACT|SCO|TGO|TRE|DOO)([0-9a-b])", tag)
                for a in lst:
                    if lm and a["name"] in boss_names:
                        boss_layer[(stage, n)] = (int(lm.group(1), 16), a.get("pos"))
    out = []
    for stage in stages:
        boss = bool(BOSS_STAGE.match(stage))
        cand = []
        for n in sorted(rooms.get(stage, [])):
            acts = room_actors.get((stage, n), set())
            why = []
            if boss and acts & enemy_names:
                why.append("boss-stage:" + "+".join(sorted(acts & enemy_names)))
            mg = sorted(a for a in acts if a in minigame_names)
            if mg:
                why.append("minigame:" + "+".join(mg))
            if why:
                cand.append((n, ";".join(why)))
        if BOSS_STAGE_STRONG.match(stage) and not cand:
            cand = [(n, "boss-stage") for n in sorted(rooms.get(stage, []))]
        cand += [(n, why) for st, n, why in EXTRA_ROOMS if st == stage]
        for n, why in cand:
            pts = points[stage].get(n, [])
            if not pts:
                continue
            entries = [p for p in pts if (stage, n, p) in exits]
            point, layer = (entries or pts)[0], None
            if (stage, n) in boss_layer:
                # The boss is placed on one layer only (Helmaroc: M2tower's layer 3): boot that
                # layer at the spawn point nearest to it.
                layer, bpos = boss_layer[(stage, n)]
                near = [(sum((a - b) ** 2 for a, b in zip(room_sweep.SPAWN_POS[(stage, n, p)],
                                                           bpos)), p)
                        for p in pts if room_sweep.SPAWN_POS.get((stage, n, p)) and bpos]
                if near:
                    point = min(near)[1]
                why += ";layer%d" % layer
            out.append((stage, n, point, layer, why))
    return out


def run_one(args, run_dir, stage, env):
    cmd = [COS_RUN, "combat-sweep", "--stage", stage, "--uncapped", "--timeout", str(args.timeout),
           "--disc", args.disc, "--quiet", "--run-dir", run_dir]
    if args.exe:
        cmd += ["--exe", args.exe]
    full = dict(os.environ, COS_CACHE_PER_RUN="1", COS_BOOT_ITEMS=args.items, **env)
    t0 = time.time()
    subprocess.run(cmd, env=full, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        rc = int(boot_sweep.read(os.path.join(run_dir, "exit_code.txt")).strip())
    except ValueError:
        rc = -1
    boot_sweep.collapse_log(os.path.join(run_dir, "run.log"))
    log = boot_sweep.read(os.path.join(run_dir, "run.log"))
    lines = [l for l in boot_sweep.read(os.path.join(run_dir, "combat_sweep.txt")).splitlines()
             if l and not l.startswith("#")]
    result = lines[-1] if lines else ""
    hw = re.findall(r"gfx high-water: verts=(\d+) KiB/\d+ uniforms=(\d+) KiB/\d+ indices=(\d+) KiB/\d+ "
                    r"storage=(\d+) KiB", log)
    r = {"rc": rc, "seconds": int(time.time() - t0), "line": result,
         "highwater": "/".join(hw[-1]) if hw else "-",
         "signature": boot_sweep.signature(run_dir, rc, log), "pipelines": cache_rows(run_dir)}
    f = result.split()
    if rc in (0, 1) and len(f) >= 5 and f[4] != "begin":
        r["result"] = f[4]
        r["counters"] = " ".join(f[5:])
    elif rc == 0:
        r["result"] = "ok"
        r["counters"] = ""
    else:
        r["result"] = "FAULT" if rc in (10, 11, 12, 13) or "begin" in result else "FAIL"
        r["counters"] = ""
    if rc == 0 and "combat-sweep: the player in the room" not in log:
        r["result"] = "NO-PLAY"
    return r


def cache_rows(run_dir):
    db = os.path.join(run_dir, "cache", "pipeline_cache.db")
    if not os.path.isfile(db):
        return -1
    try:
        with sqlite3.connect("file:%s?mode=ro" % db, uri=True) as c:
            return c.execute("SELECT COUNT(*) FROM pipeline_cache").fetchone()[0]
    except sqlite3.Error:
        return -1


def safe(s):
    return re.sub(r"[^A-Za-z0-9_.-]", "_", s)


def sweep(args, mode):
    manifest = boot_sweep.load_manifest(args.disc)
    jobs = []  # (key, label, stage, env)
    if mode == "enemies":
        cases = enemy_cases(manifest, args.variants)
        if args.home:
            room_sweep.spawn_points(manifest)  # fills SPAWN_POS
        listfile = os.path.join(args.out, "cases.txt")
        with open(listfile, "w") as f:
            f.write("# <dStage name> <params hex> <angle x> <angle z>   (why)\n")
            for name, p, ax, az, why, homes in cases:
                f.write("%s %08x %d %d\n" % (name, p, ax, az))
        spawns = None
        if args.home:
            _, _, points, exits = room_sweep.spawn_points(manifest)
        for i, (name, p, ax, az, why, homes) in enumerate(cases):
            if args.only and name not in args.only.split(","):
                continue
            stage = args.stage
            env_home = {}
            if args.home:
                # The first placement whose room has a spawn point (an SCLS entry preferred; for a
                # placement on one layer, that layer at the spawn point nearest to it).
                for st, rm, ly, pos in homes:
                    rm = rm if rm >= 0 else 0
                    pts = points.get(st, {}).get(rm, [])
                    if pts:
                        entries = [q for q in pts if (st, rm, q) in exits]
                        pt = (entries or pts)[0]
                        if ly is not None and pos:
                            near = [(sum((a - b) ** 2 for a, b in zip(room_sweep.SPAWN_POS[(st, rm, q)], pos)), q)
                                    for q in pts if room_sweep.SPAWN_POS.get((st, rm, q))]
                            pt = min(near)[1] if near else pt
                        stage = "%s:%d:%d" % (st, rm, pt) + ("" if ly is None else ":%d" % ly)
                        break
                env_home = ROOM_SETUP.get(stage.split(":")[0], {})
                why += ",home=" + stage
            jobs.append(("%03d_%s_%08x" % (i, safe(name), p), "%s %08x" % (name, p), stage,
                         {"COS_COMBAT_SWEEP_LIST": listfile, "COS_COMBAT_SWEEP": str(i),
                          "COS_COMBAT_FRAMES": str(args.frames),
                          "COS_COMBAT_SHOT_EVERY": str(args.shot_every), **env_home}, why))
        if args.death:
            first = next((i for i, c in enumerate(cases) if c[0] == "mo2"),
                         next((i for i, c in enumerate(cases) if c[0] == "Bk"), 0))
            jobs.append(("death_%s" % safe(cases[first][0]), "death %s" % cases[first][0], args.stage,
                         {"COS_COMBAT_SWEEP_LIST": listfile, "COS_COMBAT_SWEEP": str(first),
                          "COS_COMBAT_FRAMES": str(max(args.frames, 2400)), "COS_COMBAT_DEATH": "1",
                          "COS_COMBAT_SHOT_EVERY": "200"}, "death"))
    else:
        for stage, room, point, layer, why in room_cases(manifest):
            if args.only and stage not in args.only.split(",") and \
                    "%s:%d" % (stage, room) not in args.only.split(","):
                continue
            env = {"COS_COMBAT_MODE": "room", "COS_COMBAT_FRAMES": str(args.frames),
                   "COS_COMBAT_SHOT_EVERY": str(args.shot_every)}
            env.update(ROOM_SETUP.get(stage, {}))
            env.update(ROOM_SETUP.get((stage, room), {}))
            spec = "%s:%d:%d" % (stage, room, point) + ("" if layer is None else ":%d" % layer)
            if (stage, room) in ROOM_EXPECTED:
                why += ";xfail"
            jobs.append((spec.replace(":", "_"), spec, spec, env, why))
    print("combat_sweep %s: %d runs, %d at a time; %s" % (mode, len(jobs), args.jobs,
                                                        os.path.relpath(args.out, REPO)), flush=True)
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futs = {pool.submit(run_one, args, os.path.join(args.out, "runs", key), stage, env): key
                for key, label, stage, env, why in jobs}
        done = 0
        for fut in concurrent.futures.as_completed(futs):
            key = futs[fut]
            r = fut.result()
            m = re.match(r"(\w+?)_(\d+)_\d+", key)
            exp = ROOM_EXPECTED.get((m.group(1), int(m.group(2)))) if m and mode == "rooms" else None
            if exp and r["result"] in ("FAULT", "FAIL") and re.search(exp[0], r["signature"]):
                r["result"] = "xfail"
            results[key] = r
            done += 1
            print("[%d/%d] %s: %s exit %d %s%s" % (done, len(jobs), key, r["result"], r["rc"],
                  r["counters"], "" if r["rc"] in (0,) else "  " + r["signature"]), flush=True)
    counts = collections.Counter(r["result"] for r in results.values())
    with open(os.path.join(args.out, "combat_sweep.tsv"), "w") as f:
        f.write("key\tcase\twhy\tresult\texit\tpipelines\tcounters\tsignature\tseconds\t"
                "gfx_kib_vert/unif/idx/stor\n")
        for key, label, stage, env, why in jobs:
            r = results[key]
            f.write("\t".join([key, label, why, r["result"], str(r["rc"]), str(r["pipelines"]),
                               r["counters"], r["signature"], str(r["seconds"]), r["highwater"]]) + "\n")
    with open(os.path.join(args.out, "combat_sweep.md"), "w") as f:
        f.write("# combat_sweep %s %s\n\n" % (mode, os.path.basename(args.out)))
        f.write("stage %s, %d frames, items %s; results: %s\n\n" % (
            args.stage if mode == "enemies" else "(per room)", args.frames, args.items,
            ", ".join("%s %d" % kv for kv in sorted(counts.items()))))
        f.write("| case | why | result | exit | pipelines | counters | signature |\n"
                "|---|---|---|---|---|---|---|\n")
        for key, label, stage, env, why in jobs:
            r = results[key]
            f.write("| %s | %s | %s | %d | %d | %s | %s |\n" % (
                label, why, r["result"], r["rc"], r["pipelines"], r["counters"],
                r["signature"].replace("|", "\\|") if r["rc"] else ""))
    print("combat_sweep %s: %s" % (mode, ", ".join("%s %d" % kv for kv in sorted(counts.items()))))
    return 1 if counts["FAULT"] or counts["FAIL"] else 0


def pipelines(args):
    bundled = set()
    with sqlite3.connect("file:%s?mode=ro" % args.bundled, uri=True) as c:
        bundled.update(c.execute("SELECT type, hash FROM pipeline_cache"))
    per_run = {}
    for d in args.sweeps:
        for db in sorted(glob.glob(os.path.join(d, "runs", "*", "cache", "pipeline_cache.db"))):
            run = os.path.basename(os.path.dirname(os.path.dirname(db)))
            try:
                with sqlite3.connect(db) as c:
                    keys = set(c.execute("SELECT type, hash FROM pipeline_cache"))
            except sqlite3.Error as e:
                print("combat_sweep: %s: %s" % (db, e), file=sys.stderr)
                continue
            per_run[(os.path.basename(os.path.normpath(d)), run)] = keys - bundled
    owners = collections.Counter(k for s in per_run.values() for k in s)
    union = set().union(*per_run.values()) if per_run else set()
    rows = sorted(((len(s), sum(1 for k in s if owners[k] == 1), sw, run)
                   for (sw, run), s in per_run.items()), reverse=True)
    out = os.path.join(args.sweeps[0], "new_pipelines.tsv")
    with open(out, "w") as f:
        f.write("# vs %s (%d rows); %d new pipelines in all over %d runs\n"
                % (args.bundled, len(bundled), len(union), len(per_run)))
        f.write("new\tonly_here\tsweep\trun\n")
        for n, only, sw, run in rows:
            f.write("%d\t%d\t%s\t%s\n" % (n, only, sw, run))
    # By enemy (the variants of one dStage name together) or room: their union.
    groups = collections.defaultdict(set)
    for (sw, run), keys in per_run.items():
        m = re.fullmatch(r"\d{3}_(.+)_[0-9a-f]{8}", run)
        groups[m.group(1) if m else run] |= keys
    gowners = collections.Counter(k for s in groups.values() for k in s)
    with open(out, "a") as f:
        f.write("# by enemy name / room (union of its runs)\nnew\tonly_here\tname\n")
        for n, only, name in sorted(((len(s), sum(1 for k in s if gowners[k] == 1), g)
                                     for g, s in groups.items()), reverse=True):
            if n:
                f.write("%d\t%d\t%s\n" % (n, only, name))
    print(open(out).read(), end="")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("mode", choices=("enemies", "rooms", "list-enemies", "list-rooms", "pipelines"))
    ap.add_argument("sweeps", nargs="*")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--frames", type=int, default=None)
    ap.add_argument("--shot-every", type=int, default=None)
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--stage", default="M_NewD2:0:0")
    ap.add_argument("--variants", type=int, default=2)
    ap.add_argument("--items", default=DEFAULT_ITEMS)
    ap.add_argument("--death", action="store_true")
    ap.add_argument("--home", action="store_true")
    ap.add_argument("--only", default="")
    ap.add_argument("--disc", default=os.environ.get("COS_DISC", ""))
    ap.add_argument("--exe", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--bundled", default=os.path.join(REPO, "build", "pipeline-cache",
                                                      "initial_pipeline_cache.db"))
    args = ap.parse_args()
    if args.mode == "pipelines":
        if not args.sweeps:
            ap.error("pipelines needs sweep directories")
        return pipelines(args)
    if not args.disc:
        print("combat_sweep: no disc image: pass --disc PATH or set COS_DISC", file=sys.stderr)
        return 14
    args.disc = os.path.abspath(args.disc)
    if args.mode == "list-enemies":
        for c in enemy_cases(boot_sweep.load_manifest(args.disc), args.variants):
            print("%s\t%08x\t%d\t%d\t%s\t%s" % (c[:5] + (" ".join("%s:%d" % h[:2] for h in c[5][:4]),)))
        return 0
    if args.mode == "list-rooms":
        for c in room_cases(boot_sweep.load_manifest(args.disc)):
            print("%s\t%d\t%d\t%s\t%s" % c)
        return 0
    if args.frames is None:
        args.frames = 1200 if args.mode == "enemies" else 6000
    if args.shot_every is None:
        args.shot_every = 400 if args.mode == "enemies" else 1000
    args.out = os.path.abspath(args.out or os.path.join(
        REPO, "build", "combat-sweep", "%s-%s" % (args.mode, time.strftime("%Y%m%d-%H%M%S"))))
    os.makedirs(os.path.join(args.out, "runs"), exist_ok=False)
    return sweep(args, args.mode)


if __name__ == "__main__":
    sys.exit(main())
