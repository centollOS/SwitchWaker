#!/usr/bin/env python3
"""Census of the randomizer's hunks in game/ (docs/RANDOMIZER_PLAN.md step R0.1, "Hook log").

    python3 -I native/tools/rando_hook_census.py [--root DIR] [--verbose] [--self-test]

Checks, over every source and header under game/ (.c .cpp .h .hpp .inc):

1. Guards. Every use of the randomizer's API (`pc_rando(`, `pc_rando_<x>(`, `#include "pc/pc_rando...`)
   sits on the host side of a host guard: the `#if TARGET_PC` / `#ifdef TARGET_PC` branch (or the
   `#else` of `#if !TARGET_PC` / `#ifndef TARGET_PC`), so the GameCube view of the file is unchanged
   (docs/GAME_CODE_ORGANIZATION.md). Every marker comment `pc_rando(<step>): ...` sits there too.
2. Markers. Every guarded branch that uses the API (other than an include) holds a marker
   `// pc_rando(R<phase>.<n>): <wwrando ref>; original: <value>`.
3. Hook log. Every (file, step) of a marker has a row naming that file (path from the repository root,
   e.g. game/src/d/d_item.cpp) in the section `## R<phase>.<n>` of docs/RANDO_HOOKS.md, and every file a
   section names has a marker of that step (no stale rows). Rows whose first cell is `-` or `none` (a
   step without hunks in game/) are skipped.
4. D17. The event bits wwrando reuses (0x6901-0x6980 and 0x6A01-0x6A40) stay unused by vanilla and
   port code: no dSv_event_flag_c enum entry (game/include/d/d_save_event_flag.inc) has such a value,
   and no event-bit call (any function whose name contains EventBit: dComIfGs_isEventBit,
   onEventBit, checkEventBit, ...) in game/ or native/ passes one, as a literal or as an enum name.
   Allowed: inside a randomizer hunk in game/ (a guarded branch with a marker) and in the randomizer's
   own files in native/ (pc_rando*, features/rando/). Limits: a bit that reaches a call through a
   variable or a table, or that stage data (event_list.dat, dzx) sets, is not seen.
5. Regions (step R7.6a: both builds, USA VERSION=2 and EUR VERSION=3). Each file is read twice, once
   per build, following its `#if`/`#elif` on VERSION (other conditions count for both builds), and
   the PAL units in game/src/pal/ are read as part of the file whose PAL branch includes them (the
   converged units d_file_select, d_menu_window and d_a_bst; PAL_DISC_PLAN.md step 7). Every step must
   have as many markers in a file's USA view as in its PAL view: a hunk that exists on one side of a
   VERSION_SELECT/#if only is reported (USA-only hunks are duplicated into the PAL side).

Prints each problem and a summary; exits 0 when there is none, 1 otherwise (2 on a usage error).
--self-test runs the checks on small synthetic trees and exits 0 when each is caught as expected.
"""
import argparse
import os
import re
import sys
import tempfile

GAME_EXTS = (".c", ".cpp", ".h", ".hpp", ".inc")
NATIVE_EXTS = (".c", ".cpp", ".h", ".hpp", ".inc", ".mm", ".m")
HOOKS_DOC = "docs/RANDO_HOOKS.md"
EVENT_FLAG_INC = "game/include/d/d_save_event_flag.inc"
RESERVED = [(0x6901, 0x6980), (0x6A01, 0x6A40)]

MARKER_RE = re.compile(r"\bpc_rando\((R\d+\.\d+[a-z]?)\)\s*:")  # R7 split: R7.1a, R7.5a, ...
API_RE = re.compile(r"\bpc_rando\w*\s*\(")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]pc/pc_rando')
DIRECTIVE_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")
SECTION_RE = re.compile(r"^##\s+(R\d+\.\d+[a-z]?)\b")
EVENT_CALL_RE = re.compile(r"\b(\w*EventBit\w*)\s*\(\s*(?:\(\s*\w+\s*\)\s*)?([A-Za-z_][\w:]*|0[xX][0-9A-Fa-f]+|\d+)\s*[,)]")
ENUM_ENTRY_RE = re.compile(r"\b(\w+)\s*=\s*(0[xX][0-9A-Fa-f]+|\d+)\s*,")


def reserved(v):
    return any(lo <= v <= hi for lo, hi in RESERVED)


def strip_comments(lines):
    """Each line without its comments (block comments may span lines); string literals are kept."""
    out, in_block = [], False
    for line in lines:
        res, i, n = [], 0, len(line)
        while i < n:
            if in_block:
                j = line.find("*/", i)
                if j < 0:
                    i = n
                else:
                    in_block, i = False, j + 2
                continue
            c = line[i]
            if c == '"' or c == "'":  # skip a literal
                j = i + 1
                while j < n and line[j] != c:
                    j += 2 if line[j] == "\\" else 1
                res.append(line[i:j + 1])
                i = j + 1
                continue
            if line.startswith("//", i):
                break
            if line.startswith("/*", i):
                in_block, i = True, i + 2
                continue
            res.append(c)
            i += 1
        out.append("".join(res))
    return out


def guard_sense(kind, rest):
    """For an #if/#ifdef/#ifndef line: True if its first branch is the host side of a TARGET_PC
    guard, False if its #else is, None when it is not a TARGET_PC guard."""
    rest = rest.strip()
    if kind == "ifdef":
        return True if rest.split()[:1] == ["TARGET_PC"] else None
    if kind == "ifndef":
        return False if rest.split()[:1] == ["TARGET_PC"] else None
    if not re.search(r"\bTARGET_PC\b", rest):
        return None
    if re.match(r"^!\s*(defined\s*\(?\s*)?TARGET_PC\b", rest):
        return False if not re.search(r"&&|\|\|", rest) else None
    if re.match(r"^(defined\s*\(?\s*)?TARGET_PC\b\)?\s*($|&&)", rest):
        return True
    return None


def analyse(lines):
    """Per line: (on_host_side, region) where region identifies the innermost host-side guard branch
    (None outside one)."""
    code = strip_comments(lines)
    stack = []  # frames: [sense or None, in_first_branch, region_id]
    info = []
    for idx, text in enumerate(code):
        m = DIRECTIVE_RE.match(text)
        if m:
            kind, rest = m.group(1), m.group(2)
            if kind in ("if", "ifdef", "ifndef"):
                stack.append([guard_sense(kind, rest), True, idx])
            elif kind in ("elif", "else") and stack:
                stack[-1][1] = False
                stack[-1][2] = idx
            elif kind == "endif" and stack:
                stack.pop()
            info.append((False, None))
            continue
        host, region = False, None
        for sense, first, rid in stack:
            if sense is None:
                continue
            if (sense and first) or (sense is False and not first):
                host, region = True, rid
            else:
                host, region = False, None
        info.append((host, region))
    return code, info


VERSIONS = {"USA": 2, "PAL": 3}
VERSION_NAMES = {"VERSION_DEMO": 0, "VERSION_JPN": 1, "VERSION_USA": 2, "VERSION_PAL": 3}
PAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"(pal/[^"]+)"')
IDENT_RE = re.compile(r"[A-Za-z_]\w*")


def version_value(kind, rest, version):
    """The value of a directive's condition in a build (True/False), or None when the condition is not
    about VERSION (or cannot be read): such a branch counts for both builds."""
    rest = rest.strip()
    if kind in ("ifdef", "ifndef"):
        return None
    idents = set(IDENT_RE.findall(rest))
    if "VERSION" not in idents:
        return None
    allowed = {"VERSION", "TARGET_PC", "defined"} | set(VERSION_NAMES)
    if not idents <= allowed:
        return None
    expr = re.sub(r"\bdefined\s*\(\s*(\w+)\s*\)|\bdefined\s+(\w+)", lambda m: "1", rest)
    expr = re.sub(r"\bVERSION_\w+\b", lambda m: str(VERSION_NAMES[m.group(0)]), expr)
    expr = re.sub(r"\bVERSION\b", str(version), expr)
    expr = re.sub(r"\bTARGET_PC\b", "1", expr)
    expr = expr.replace("&&", " and ").replace("||", " or ")
    expr = re.sub(r"!(?!=)", " not ", expr)
    try:
        return bool(eval(expr, {"__builtins__": {}}, {}))
    except Exception:
        return None


def version_views(code, version):
    """Per line: whether it is compiled in the build of `version` (VERSION conditions only)."""
    stack = []  # frames: [active_parent, taken (True/False/None), active]
    active = []
    for text in code:
        m = DIRECTIVE_RE.match(text)
        cur = stack[-1][2] if stack else True
        if m:
            kind, rest = m.group(1), m.group(2)
            if kind in ("if", "ifdef", "ifndef"):
                v = version_value(kind, rest, version)
                on = v is not False
                stack.append([cur, v if v is not None else None, cur and on])
            elif kind == "elif" and stack:
                parent, taken, _ = stack[-1]
                v = version_value("elif", rest, version)
                if taken is True:
                    on = False
                else:
                    on = v is not False
                stack[-1][1] = True if (taken is True or v is True) else (None if (taken is None or v is None) else False)
                stack[-1][2] = parent and on
            elif kind == "else" and stack:
                parent, taken, _ = stack[-1]
                stack[-1][2] = parent and taken is not True
            elif kind == "endif" and stack:
                stack.pop()
            active.append(False)
            continue
        active.append(cur)
    return active


def region_markers(rel, lines, code, version, root, seen=()):
    """Counter {step: n} of the markers a file has in one build, with the pal/ units it includes."""
    counts = {}
    act = version_views(code, version)
    for i, raw in enumerate(lines):
        if not act[i]:
            continue
        for m in MARKER_RE.finditer(raw):
            if code[i].find("pc_rando(" + m.group(1) + ")") >= 0:
                continue
            counts[m.group(1)] = counts.get(m.group(1), 0) + 1
    for i, text in enumerate(code):
        m = PAL_INCLUDE_RE.match(text)
        if not m:
            continue
        # a directive line: active when its enclosing branch is (the line itself reads False)
        if not version_views(code[:i] + ["int __census_probe;"], version)[i]:
            continue
        inc = os.path.join(root, "game", "src", m.group(1))
        if not os.path.exists(inc) or inc in seen:
            continue
        il = read_lines(inc)
        for step, n in region_markers(m.group(1), il, strip_comments(il), version, root, seen + (inc,)).items():
            counts[step] = counts.get(step, 0) + n
    return counts


def walk(root, sub, exts):
    base = os.path.join(root, sub)
    for d, dirs, files in os.walk(base):
        dirs.sort()
        for f in sorted(files):
            if f.endswith(exts):
                p = os.path.join(d, f)
                yield os.path.relpath(p, root).replace(os.sep, "/"), p


def read_lines(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read().split("\n")


def parse_hooks_doc(root, problems):
    """{step: {file: line}} from docs/RANDO_HOOKS.md."""
    path = os.path.join(root, HOOKS_DOC)
    sections = {}
    if not os.path.exists(path):
        problems.append(f"{HOOKS_DOC}: missing")
        return sections
    step = None
    for n, line in enumerate(read_lines(path), 1):
        m = SECTION_RE.match(line)
        if m:
            step = m.group(1)
            sections.setdefault(step, {})
            continue
        if line.startswith("## "):
            step = None
            continue
        if step is None or not line.lstrip().startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not cells or re.match(r"^:?-{3,}:?$", cells[0]) or cells[0].lower() in ("file", ""):
            continue
        first = cells[0].strip("`").strip()
        if first in ("-", "none", "—"):
            continue
        first = re.sub(r":\d+(-\d+)?$", "", first)  # an optional :line
        sections[step][first] = n
    return sections


def event_enum(root):
    """{name: value} of dSv_event_flag_c's enum."""
    path = os.path.join(root, EVENT_FLAG_INC)
    vals = {}
    if os.path.exists(path):
        for line in strip_comments(read_lines(path)):
            for m in ENUM_ENTRY_RE.finditer(line):
                vals[m.group(1)] = int(m.group(2), 0)
    return vals


def run(root, verbose=False, out=sys.stdout):
    problems = []
    markers = {}  # (file, step) -> [line]
    n_uses = 0
    enum_vals = event_enum(root)
    for name, v in sorted(enum_vals.items()):
        if reserved(v):
            problems.append(f"{EVENT_FLAG_INC}: D17: dSv_event_flag_c::{name} = 0x{v:04X} is a bit wwrando reuses")

    def event_uses(rel, code, allowed):
        text = "\n".join(code)
        for m in EVENT_CALL_RE.finditer(text):
            arg = m.group(2)
            if re.match(r"^(0[xX][0-9A-Fa-f]+|\d+)$", arg):
                v = int(arg, 0)
            else:
                v = enum_vals.get(arg.split("::")[-1])
                if v is None:
                    continue
            if not reserved(v):
                continue
            line = text.count("\n", 0, m.start()) + 1
            if allowed(line):
                continue
            problems.append(f"{rel}:{line}: D17: {m.group(1)}({arg}) uses event bit 0x{v:04X}, which wwrando reuses")

    for rel, path in walk(root, "game", GAME_EXTS):
        lines = read_lines(path)
        if not any("pc_rando" in ln or "EventBit" in ln for ln in lines):
            continue
        code, info = analyse(lines)
        file_markers = {}  # region -> [(line, step)]
        for i, raw in enumerate(lines):
            for m in MARKER_RE.finditer(raw):
                if code[i].find("pc_rando(" + m.group(1) + ")") >= 0:
                    continue  # in code, not a comment: a call, not a marker
                host, region = info[i]
                if not host:
                    problems.append(f"{rel}:{i + 1}: marker pc_rando({m.group(1)}) outside a TARGET_PC host branch")
                    continue
                file_markers.setdefault(region, []).append((i + 1, m.group(1)))
                markers.setdefault((rel, m.group(1)), []).append(i + 1)
        for i, text in enumerate(code):
            if INCLUDE_RE.match(text) or not API_RE.search(text):
                continue  # includes are directive lines: checked below
            n_uses += 1
            host, region = info[i]
            if not host:
                problems.append(f"{rel}:{i + 1}: unguarded randomizer call: {text.strip()}")
            elif region not in file_markers:
                problems.append(f"{rel}:{i + 1}: randomizer call in a guarded branch without a pc_rando(<step>) marker")
        # includes: the directive lines are skipped by analyse(); check them with their own pass
        for i, text in enumerate(code):
            if INCLUDE_RE.match(text) and not include_guarded(code, i):
                problems.append(f"{rel}:{i + 1}: unguarded include of the randomizer's header")
        regions = file_markers
        event_uses(rel, code, lambda line, info=info, regions=regions: info[line - 1][0] and info[line - 1][1] in regions)
        if not rel.startswith("game/src/pal/"):  # read with the file that includes them
            views = {name: region_markers(rel, lines, code, v, root) for name, v in VERSIONS.items()}
            for step in sorted(set(views["USA"]) | set(views["PAL"])):
                nu, npal = views["USA"].get(step, 0), views["PAL"].get(step, 0)
                if nu != npal:
                    problems.append(f"{rel}: region: pc_rando({step}) has {nu} marker(s) in the USA build and "
                                    f"{npal} in the PAL build (game/src/pal/ included)")

    for rel, path in walk(root, "native", NATIVE_EXTS):
        base = os.path.basename(rel)
        own = base.startswith("pc_rando") or "/features/rando/" in rel or "/check/rando/" in rel
        lines = read_lines(path)
        if not any("EventBit" in ln for ln in lines):
            continue
        code = strip_comments(lines)
        event_uses(rel, code, lambda line, own=own: own)

    doc = parse_hooks_doc(root, problems)
    for (rel, step), lns in sorted(markers.items()):
        if rel not in doc.get(step, {}):
            problems.append(f"{rel}:{lns[0]}: marker pc_rando({step}) not listed in {HOOKS_DOC} section ## {step}")
    for step, files in sorted(doc.items()):
        for rel, n in sorted(files.items()):
            if (rel, step) not in markers:
                problems.append(f"{HOOKS_DOC}:{n}: ## {step} lists {rel}, which has no pc_rando({step}) marker")

    if verbose:
        for (rel, step), lns in sorted(markers.items()):
            print(f"{step} {rel}: {len(lns)} marker(s), lines {','.join(map(str, lns))}", file=out)
    for p in problems:
        print(p, file=out)
    steps = sorted({s for _, s in markers})
    print(f"rando_hook_census: {sum(len(v) for v in markers.values())} marker(s) in "
          f"{len({r for r, _ in markers})} file(s) for {len(steps)} step(s), {n_uses} API use(s), "
          f"{len(doc)} hook-log section(s): {'clean' if not problems else f'{len(problems)} problem(s)'}", file=out)
    return problems


def include_guarded(code, i):
    """Whether directive line i sits on the host side of a TARGET_PC guard."""
    _, info = analyse(code[:i] + ["int __census_probe;"])
    return info[i][0]


def self_test():
    import io
    cases = []
    ok_doc = "# hooks\n\n## R9.1\n\n| File | Function | Guard | wwrando |\n|---|---|---|---|\n| `game/src/a.cpp` | f | `?:` | x |\n"
    good = ("#if TARGET_PC\n#include \"pc/pc_rando.h\"\n#endif\nint f() {\n#if TARGET_PC\n"
            "    // pc_rando(R9.1): x.asm 0x10; original: 3\n    return pc_rando() ? 4 : 3;\n#else\n"
            "    return 3;\n#endif\n}\n")
    cases.append(("clean", {"game/src/a.cpp": good, HOOKS_DOC: ok_doc}, 0))
    cases.append(("unguarded call", {"game/src/a.cpp": "int f() { // pc_rando(R9.1): x\n return pc_rando(); }\n",
                                      HOOKS_DOC: ok_doc}, 3))
    cases.append(("else branch", {"game/src/a.cpp": "#if TARGET_PC\nint x;\n#else\n// pc_rando(R9.1): x\nint y = pc_rando();\n#endif\n",
                                   HOOKS_DOC: ok_doc}, 3))
    cases.append(("unmarked", {"game/src/a.cpp": "#if TARGET_PC\nint y = pc_rando();\n#endif\n", HOOKS_DOC: ok_doc}, 2))
    cases.append(("unlisted", {"game/src/a.cpp": good, HOOKS_DOC: "# hooks\n"}, 1))
    cases.append(("stale row", {"game/src/a.cpp": "int z;\n", HOOKS_DOC: ok_doc}, 1))
    cases.append(("unguarded include", {"game/src/a.cpp": "#include \"pc/pc_rando.h\"\nint z;\n",
                                         HOOKS_DOC: "# hooks\n"}, 1))
    cases.append(("ifndef else", {"game/src/a.cpp": "#ifndef TARGET_PC\nint x;\n#else\n// pc_rando(R9.1): x\nint y = pc_rando();\n#endif\n",
                                   HOOKS_DOC: ok_doc}, 0))
    cases.append(("D17 vanilla literal", {"game/src/b.cpp": "void g() { dComIfGs_onEventBit(0x6904); }\n", HOOKS_DOC: ""}, 1))
    cases.append(("D17 enum", {EVENT_FLAG_INC: "enum {\n  UNK_6A02 = 0x6A02,\n  UNK_0101 = 0x0101,\n};\n",
                               "game/src/b.cpp": "void g() { dComIfGs_isEventBit(dSv_event_flag_c::UNK_6A02); }\n",
                               HOOKS_DOC: ""}, 2))
    cases.append(("D17 outside range", {"game/src/b.cpp": "void g() { dComIfGs_onEventBit(0x6A80); mDoAud_seStart(0x6901); }\n",
                                         HOOKS_DOC: ""}, 0))
    cases.append(("D17 in rando hunk", {"game/src/a.cpp": good.replace("return pc_rando() ? 4 : 3;",
                                                                     "return pc_rando() && dComIfGs_isEventBit(0x6901);"),
                                        HOOKS_DOC: ok_doc}, 0))
    cases.append(("D17 native", {"native/src/pc/features/pc_x.cpp": "void g() { dComIfGs_onEventBit(0x6980); }\n",
                                 "native/src/pc/features/rando/pc_rando_y.cpp": "void g() { dComIfGs_onEventBit(0x6980); }\n",
                                 HOOKS_DOC: ""}, 1))
    usa_only = ("#if TARGET_PC\n#include \"pc/pc_rando.h\"\n#endif\n#if VERSION == VERSION_PAL\n#include \"pal/d/a.inc\"\n"
                "#else\nint f() {\n#if TARGET_PC\n    // pc_rando(R9.1): x.asm 0x10; original: 3\n"
                "    return pc_rando() ? 4 : 3;\n#else\n    return 3;\n#endif\n}\n#endif\n")
    pal_inc = ("int f() {\n#if TARGET_PC\n    // pc_rando(R9.1): x.asm 0x10; original: 3\n"
               "    return pc_rando() ? 4 : 3;\n#else\n    return 3;\n#endif\n}\n")
    cases.append(("region USA only", {"game/src/a.cpp": usa_only, HOOKS_DOC: ok_doc}, 1))
    cases.append(("region pal unit", {"game/src/a.cpp": usa_only, "game/src/pal/d/a.inc": pal_inc,
                                      HOOKS_DOC: ok_doc + "| `game/src/pal/d/a.inc` | f | `?:` | x |\n"}, 0))
    cases.append(("region elif", {"game/src/a.cpp": good.replace("int f() {\n", "int f() {\n#if VERSION == VERSION_JPN\n"
                                                                    "int j;\n#elif VERSION == VERSION_USA\n").replace(
                                                                        "    return 3;\n#endif\n}\n", "    return 3;\n#endif\n#endif\n}\n"),
                                  HOOKS_DOC: ok_doc}, 1))
    failed = 0
    for name, files, expect in cases:
        with tempfile.TemporaryDirectory() as d:
            for rel, text in files.items():
                p = os.path.join(d, rel)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, "w") as fh:
                    fh.write(text)
            os.makedirs(os.path.join(d, "game"), exist_ok=True)
            os.makedirs(os.path.join(d, "native"), exist_ok=True)
            buf = io.StringIO()
            got = run(d, out=buf)
            if len(got) != expect:
                failed += 1
                print(f"self-test {name}: {len(got)} problem(s), expected {expect}:\n{buf.getvalue()}")
            else:
                print(f"self-test {name}: ok")
    print("self-test: " + ("ok" if not failed else f"{failed} case(s) failed"))
    return 0 if not failed else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--root", default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
    ap.add_argument("--verbose", action="store_true", help="list every step's markers per file")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        sys.exit(self_test())
    if not os.path.isdir(os.path.join(a.root, "game")):
        print(f"rando_hook_census: {a.root} has no game/", file=sys.stderr)
        sys.exit(2)
    sys.exit(1 if run(a.root, a.verbose) else 0)


if __name__ == "__main__":
    main()
