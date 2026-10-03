#!/usr/bin/env python3
"""Link census: what the non-REL game units still need from outside (docs/NATIVE_PORT_PHASE2_3.md,
step 2.5).

native/cmake/census.cmake links every enabled module's objects, minus the REL units
(native/cmake/rel_units.txt), plus cos_sdk and Aurora when they are built, into the bundle
cos_link_census with `-undefined dynamic_lookup`. Whatever the bundle still looks up dynamically
is unresolved: this script sorts it into SDK, REL, JAudio/JAZel, MSL/runtime, deferred game units
and other.

Subcommands:
  rel-units  write native/cmake/rel_units.txt from the decomp's configure.py (unit names only)
  prepare    before the link: symbol census of the inputs (duplicate strong definitions first),
             then the linker response file. Unless --strict, a duplicated symbol is made local in
             a copy of every object but the first that defines it (ld -r), so one duplicate does
             not hide the whole census; the report says how many there were.
  report     after the link: `nm -um` on the bundle, classified, into link_census.txt and the
             sorted list link_census_unresolved.txt (category<TAB>symbol).
"""

from __future__ import annotations

import argparse
import ast
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

sys.dont_write_bytecode = True  # no __pycache__ in the source tree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import symbol_census  # noqa: E402

# ---- rel-units -------------------------------------------------------------------------------

REL_UNITS_HEADER = """\
# REL units of the game: every unit the decomp builds into a .rel instead of main.dol.
# Paths are relative to game/src. Names only: taken from the decomp's configure.py
# (Rel("f_pc_profile_lst") and every ActorRel), no game data.
# The link census (native/cmake/census.cmake) leaves these units out; phase 3 links them in.
# Regenerate: native/tools/link_census.py rel-units --configure <decomp>/configure.py
"""


def rel_units_from_configure(path: str) -> list[str]:
    """Units of every Rel(...) and ActorRel(...) call in configure.py, parsed, never executed."""
    with open(path, encoding="utf-8") as f:
        tree = ast.parse(f.read(), path)
    units: list[str] = []
    for node in ast.walk(tree):
        if not (isinstance(node, ast.Call) and isinstance(node.func, ast.Name)):
            continue
        if node.func.id == "ActorRel":
            arg = node.args[1]
            if isinstance(arg, ast.Constant) and isinstance(arg.value, str):
                units.append(f"d/actor/{arg.value}.cpp")
        elif node.func.id == "Rel":
            for sub in ast.walk(node):
                if (isinstance(sub, ast.Call) and getattr(sub.func, "id", None) == "Object"
                        and len(sub.args) > 1 and isinstance(sub.args[1], ast.Constant)):
                    units.append(sub.args[1].value)
    return sorted(set(units))


def cmd_rel_units(args) -> int:
    units = rel_units_from_configure(args.configure)
    missing = [u for u in units if args.game_src and not os.path.exists(os.path.join(args.game_src, u))]
    if missing:
        print(f"link_census: {len(missing)} REL units not under {args.game_src}: {missing[:5]}",
              file=sys.stderr)
        return 1
    with open(args.out, "w", encoding="utf-8") as f:
        f.write(REL_UNITS_HEADER)
        f.write(f"# {len(units)} units\n")
        f.writelines(u + "\n" for u in units)
    print(f"link_census: {len(units)} REL units written to {args.out}")
    return 0


def read_list(path: str) -> list[str]:
    with open(path, encoding="utf-8") as f:
        return [l.strip() for l in f if l.strip() and not l.lstrip().startswith("#")]


def write_if_changed(path: str, text: str) -> None:
    try:
        with open(path, encoding="utf-8") as f:
            if f.read() == text:
                return
    except FileNotFoundError:
        pass
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


# ---- prepare ---------------------------------------------------------------------------------

def _localize(obj: str, symbols: list[str], out_dir: str, index: int) -> str:
    """Copy `obj` with `symbols` made local (ld -r turns unexported symbols into statics)."""
    base = re.sub(r"[^A-Za-z0-9_.-]", "_", os.path.basename(obj))
    out = os.path.join(out_dir, f"{index:04d}_{base}")
    lst = out + ".unexported"
    with open(lst, "w", encoding="utf-8") as f:
        f.writelines(s + "\n" for s in symbols)
    res = subprocess.run(["ld", "-r", obj, "-o", out, "-unexported_symbols_list", lst],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if res.returncode != 0:
        raise SystemExit(f"link_census: ld -r failed on {obj}:\n{res.stdout}")
    return out


def cmd_prepare(args) -> int:
    objects = read_list(args.objects)
    missing = [o for o in objects if not os.path.isfile(o)]
    if missing:
        print(f"link_census: {len(missing)} objects missing, first: {missing[0]}", file=sys.stderr)
        return 1
    os.makedirs(args.out_dir, exist_ok=True)

    scanned = symbol_census.scan(objects)
    dups = symbol_census.duplicate_strong(scanned)
    weak = symbol_census.weak_size_mismatches(scanned)
    report = symbol_census.format_report(scanned, dups, weak, args.root)
    write_if_changed(os.path.join(args.out_dir, "symbol_census.txt"), report + "\n")
    # One line per duplicate for the final report: symbol<TAB>number of definitions.
    write_if_changed(os.path.join(args.out_dir, "duplicates.txt"),
                     "".join(f"{n}\t{len(o)}\n" for n, o in sorted(dups.items())))

    if dups:
        pretty = symbol_census.demangle(sorted(dups))
        print(f"link_census: {len(dups)} duplicate strong symbols among the census inputs "
              f"(full list: {os.path.join(args.out_dir, 'symbol_census.txt')}):")
        for n in sorted(dups, key=lambda s: pretty[s])[:40]:
            print(f"  {pretty[n]}  x{len(dups[n])}")
        if len(dups) > 40:
            print(f"  ... and {len(dups) - 40} more")

    inputs = list(objects)
    loc_dir = os.path.join(args.out_dir, "localized")
    if dups and not args.strict:
        os.makedirs(loc_dir, exist_ok=True)
        per_obj: dict[str, list[str]] = defaultdict(list)
        for name, owners in dups.items():
            for o in owners[1:]:  # the first definition (input order) stays global
                per_obj[o].append(name)
        index = {o: i for i, o in enumerate(objects)}
        for o, names in per_obj.items():
            inputs[index[o]] = _localize(o, sorted(names), loc_dir, index[o])
        print(f"link_census: made them local in {len(per_obj)} object copies "
              f"({loc_dir}); configure with -DCOS_LINK_CENSUS_STRICT=ON to let them stop the link")
    elif dups:
        print("link_census: strict mode, the link will stop on them")

    # Always rewritten: the bundle reads the objects through this file, so it is the bundle's only
    # dependency on them. prepare runs only when an object changed, and an unchanged response
    # file (CMake custom commands restat their outputs) would leave a stale bundle and report.
    with open(args.rsp, "w", encoding="utf-8") as f:
        f.write("".join(f'"{p}"\n' for p in inputs))
    return 0


# ---- report ----------------------------------------------------------------------------------

# Identifiers declared or defined at column 0 of a C/C++ file: "type name(" or "extern ... name;".
# Neither pattern may cross a newline, so indented call sites on later lines are never harvested.
_DECL_FUNC = re.compile(r"^(?![#/ \t{}])[^;={}()\n]*?\b([A-Za-z_]\w*)\s*\(", re.M)
_DECL_VAR = re.compile(r"^extern[ \t]+[^;()\n]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*;", re.M)
_DECL_TYPE = re.compile(r"^\s*(?:class|struct|namespace|union|enum)\s+([A-Za-z_]\w*)", re.M)
_SRC_EXT = (".c", ".cpp", ".h", ".hpp", ".inc")
_KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "defined", "extern", "static",
             "inline", "void", "int", "char", "float", "double", "asm", "__attribute__"}


def harvest(paths: list[str], types: bool = False) -> dict[str, str]:
    """Name -> file for the identifiers declared at file scope under `paths` (files or dirs)."""
    names: dict[str, str] = {}
    files: list[str] = []
    for p in paths:
        if os.path.isfile(p):
            files.append(p)
        elif os.path.isdir(p):
            for root, _d, fs in os.walk(p):
                files.extend(os.path.join(root, f) for f in sorted(fs) if f.endswith(_SRC_EXT))
    for path in sorted(files):
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError:
            continue
        regexes = [_DECL_FUNC, _DECL_VAR] + ([_DECL_TYPE] if types else [])
        for rx in regexes:
            for m in rx.finditer(text):
                n = m.group(1)
                if n not in _KEYWORDS:
                    names.setdefault(n, path)
    return names


# SDK sub-buckets, by name prefix (checked first) or by the directory the name was found in.
_SDK_PREFIXES = [
    ("audio", r"(AI|DSP|DTK|AX)"),
    ("OS", r"(OS|__OS|PPC|LC|DC|IC|L2|SystemCallVector|__start)"),
    ("GX", r"(GX|__GX)"), ("GD", r"(GD|__GD)"), ("GF", r"GF"),
    ("VI", r"(VI|__VI)"), ("PAD", r"(PAD|__PAD)"), ("SI", r"(SI|__SI)"),
    ("DVD", r"(DVD|__DVD|__fst)"), ("AR", r"(ARQ?|__AR)"), ("CARD", r"(CARD|__CARD)"),
    ("GBA", r"(GBA|__GBA)"), ("EXI", r"(EXI|__EXI)"), ("DB", r"(DB|__DB)"),
    ("MTX", r"(PSMTX|PSVEC|PSQUAT|C_MTX|C_VEC|C_QUAT|MTX|VEC|QUAT)"), ("THP", r"THP"),
]
_SDK_PREFIX_RX = [(cat, re.compile(rf"^{rx}[A-Z_0-9]")) for cat, rx in _SDK_PREFIXES]
_SDK_DIRS = {"ai": "audio", "dsp": "audio", "os": "OS", "base": "OS", "gx": "GX", "gd": "GD",
             "gf": "GF", "vi": "VI", "pad": "PAD", "si": "SI", "dvd": "DVD", "ar": "AR",
             "card": "CARD", "gba": "GBA", "exi": "EXI", "db": "DB", "mtx": "MTX", "thp": "THP"}
_REL_NAMES = re.compile(r"^(OSLink|OSUnlink|OSSetStringTable|g_profile_)")
_MSL_NAMES = re.compile(r"^(stricmp|strnicmp|__(cvt_|ptmf_|construct_|destroy_|register_global|"
                        r"init_cpp|fini_cpp|save_|restore_|div2|mod2|shl2|shr2|cntlzw|dcbz))")
_PRETTY_PREFIXES = ("vtable for ", "VTT for ", "typeinfo for ", "typeinfo name for ",
                    "guard variable for ", "non-virtual thunk to ", "virtual thunk to ",
                    "construction vtable for ")


def qualified_name(pretty: str) -> list[str]:
    """'void ns::Cls<int>::f(int) const' -> ['ns', 'Cls', 'f']; a C name -> [name]."""
    s = pretty
    for p in _PRETTY_PREFIXES:
        if s.startswith(p):
            s = s[len(p):]
    depth, out = 0, []
    for ch in s:  # drop template arguments, stop at the parameter list
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth = max(0, depth - 1)
        elif depth == 0:
            if ch == "(":
                break
            out.append(ch)
    s = "".join(out).strip()
    s = s.split()[-1] if s.split() else s
    return [p for p in s.split("::") if p]


class Classifier:
    def __init__(self, game_root: str, rel_defined: set[str], deferred_units: list[str]):
        src, inc = os.path.join(game_root, "src"), os.path.join(game_root, "include")
        self.rel_defined = rel_defined
        sdk_paths = [os.path.join(src, "dolphin"), os.path.join(inc, "dolphin")]
        self.sdk = harvest(sdk_paths)
        self.sdk_debug = harvest([os.path.join(src, d) for d in
                                  ("amcstubs", "OdemuExi2", "odenotstub", "TRK_MINNOW_DOLPHIN")]
                                 + [os.path.join(inc, d) for d in ("OdemuExi2", "TRK_MINNOW_DOLPHIN")])
        self.audio = harvest([os.path.join(src, "JSystem", "JAudio"),
                              os.path.join(inc, "JSystem", "JAudio"),
                              os.path.join(src, "JAZelAudio"), os.path.join(inc, "JAZelAudio")],
                             types=True)
        self.msl = harvest([os.path.join(src, "PowerPC_EABI_Support")])
        self.deferred = harvest([os.path.join(game_root, u) for u in deferred_units], types=True)

    def _sdk_sub(self, name: str) -> str:
        for cat, rx in _SDK_PREFIX_RX:
            if rx.match(name):
                return cat
        path = self.sdk.get(name, "")
        parts = path.replace(os.sep, "/").split("/dolphin/")
        if len(parts) > 1:
            return _SDK_DIRS.get(parts[-1].split("/")[0], "other")
        return "other"

    def classify(self, mangled: str, pretty: str) -> str:
        q = qualified_name(pretty)
        free = q[-1] if len(q) == 1 else None  # unqualified: a C name or a free function
        head = q[0] if q else pretty
        if _REL_NAMES.match(head) or mangled in self.rel_defined:
            return "REL"
        # A plain C name the SDK declares is an SDK gap even when audio or deferred code also
        # names it; test it first so it can never hide in the JAudio/JAZel or deferred bucket.
        if free is not None and free in self.sdk:
            return "SDK/" + self._sdk_sub(free)
        if head in self.deferred:
            return "deferred"
        if head in self.audio or head.startswith(("JAS", "JAI", "JAZ")):
            return "JAudio/JAZel"
        if free is not None:
            if free in self.sdk or any(rx.match(free) for _c, rx in _SDK_PREFIX_RX):
                return "SDK/" + self._sdk_sub(free)
            if free in self.sdk_debug:
                return "SDK/debug"
            if free in self.msl or _MSL_NAMES.match(free):
                return "MSL/runtime"
        return "other"


CATEGORY_ORDER = ["SDK", "REL", "JAudio/JAZel", "MSL/runtime", "deferred", "other"]


def bundle_unresolved(bundle: str) -> list[str]:
    out = subprocess.run(["nm", "-um", bundle], stdout=subprocess.PIPE, text=True, check=True).stdout
    names = []
    for line in out.splitlines():
        if "(dynamically looked up)" in line:
            tokens = line.replace("(dynamically looked up)", "").split()
            if tokens:
                names.append(tokens[-1])
    return sorted(set(names))


def cmd_report(args) -> int:
    objects = read_list(args.objects)
    rel_objects = read_list(args.rel_objects) if args.rel_objects else []
    deferred = []
    if args.deferred and os.path.exists(args.deferred):
        deferred = [l.split(":", 1)[0] for l in read_list(args.deferred)]
    dups = read_list(args.duplicates) if args.duplicates and os.path.exists(args.duplicates) else []

    unresolved = bundle_unresolved(args.bundle)
    scanned = symbol_census.scan(objects + rel_objects)
    by_path = {o.path: o for o in scanned}
    rel_defined = {d.name for p in rel_objects for d in by_path[p].defined}
    users: dict[str, list[str]] = defaultdict(list)
    for p in objects:
        for n in by_path[p].undefined:
            users[n].append(p)

    pretty = symbol_census.demangle(unresolved)
    cls = Classifier(args.game_root, rel_defined, deferred)
    cat = {n: cls.classify(n, pretty[n]) for n in unresolved}

    def unit(p: str) -> str:
        m = re.search(r"/game/src/(.*)\.o$", p)
        return m.group(1) if m else os.path.basename(p)

    top = Counter(c.split("/")[0] if c.startswith("SDK/") else c for c in cat.values())
    sub = Counter(c for c in cat.values() if c.startswith("SDK/"))
    lines = [
        "cos_link_census: symbols the non-REL game units still need, after linking them",
        f"  objects: {len(objects)} non-REL units ({len(rel_objects)} REL units left out)",
        f"  SDK headers: {args.sdk_headers}; cos_sdk and Aurora linked: {args.with_aurora}",
        f"  duplicate strong symbols among the inputs: {len(dups)}"
        + (" (made local for this census; see link_census/symbol_census.txt)" if dups and not args.strict
           else ""),
        "",
        f"{'category':<20}{'count':>7}",
    ]
    for c in CATEGORY_ORDER:
        lines.append(f"{c:<20}{top.get(c, 0):>7}")
        if c == "SDK":
            for s, n in sorted(sub.items()):
                lines.append(f"  {s:<18}{n:>7}")
    lines.append(f"{'total':<20}{len(unresolved):>7}")
    summary = list(lines)
    lines.append("")

    def order(c: str) -> tuple:
        return (CATEGORY_ORDER.index(c.split("/")[0] if c.startswith("SDK/") else c), c)

    for c in sorted(set(cat.values()), key=order):
        names = sorted((n for n in unresolved if cat[n] == c), key=lambda n: pretty[n])
        lines.append(f"== {c} ({len(names)}) ==")
        for n in names:
            u = users.get(n, [])
            where = ", ".join(unit(p) for p in u[:3]) + (f", +{len(u) - 3}" if len(u) > 3 else "")
            shown = pretty[n] if pretty[n] == n.lstrip("_") or pretty[n] == n[1:] else f"{pretty[n]}  [{n}]"
            lines.append(f"  {shown}    <- {len(u)}: {where}")
        lines.append("")

    write_if_changed(args.out, "\n".join(lines) + "\n")
    write_if_changed(args.unresolved,
                     "".join(f"{cat[n]}\t{n}\n" for n in sorted(unresolved, key=lambda n: (order(cat[n]), n))))
    print("\n".join(summary))
    print(f"link_census: report in {args.out}, list in {args.unresolved}")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sp = ap.add_subparsers(dest="cmd", required=True)

    r = sp.add_parser("rel-units", help="write the REL unit list from configure.py")
    r.add_argument("--configure", required=True)
    r.add_argument("--out", required=True)
    r.add_argument("--game-src", help="check that every unit exists under this game/src")
    r.set_defaults(func=cmd_rel_units)

    p = sp.add_parser("prepare", help="symbol census of the inputs, then the link response file")
    p.add_argument("--objects", required=True, help="list of the census objects")
    p.add_argument("--out-dir", required=True)
    p.add_argument("--rsp", required=True, help="linker response file to write")
    p.add_argument("--root", help="print paths relative to this directory")
    p.add_argument("--strict", action="store_true", help="keep duplicates: they stop the link")
    p.set_defaults(func=cmd_prepare)

    q = sp.add_parser("report", help="classify what the linked bundle still looks up")
    q.add_argument("--bundle", required=True)
    q.add_argument("--objects", required=True)
    q.add_argument("--rel-objects")
    q.add_argument("--duplicates")
    q.add_argument("--deferred", help="cos_deferred.txt (path: reason)")
    q.add_argument("--game-root", required=True)
    q.add_argument("--sdk-headers", default="?")
    q.add_argument("--with-aurora", default="?")
    q.add_argument("--strict", action="store_true")
    q.add_argument("--out", required=True)
    q.add_argument("--unresolved", required=True)
    q.set_defaults(func=cmd_report)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
