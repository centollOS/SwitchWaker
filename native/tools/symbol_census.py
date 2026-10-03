#!/usr/bin/env python3
"""Symbol census over object files, without linking (docs/NATIVE_PORT_PHASE2_3.md, step 2.5).

Reports, across a set of Mach-O object files:
  1. duplicate strong definitions: an external, non-weak symbol defined by more than one object.
     These stop a link ("duplicate symbol"), so they are listed first;
  2. weak definitions with differing sizes: an external weak symbol (C++ inline functions,
     vtables, template instances) whose size differs between the objects that define it. For data
     (vtables, typeinfo, static members) this is a strong sign of an ODR violation: two classes of
     the same name with different layouts, merged silently by the linker. For code it is only a
     hint, since each unit optimises its copy of an inline function on its own.

Mach-O has no symbol sizes, so a symbol's size is the distance to the next symbol of its section
(local symbols included) or to the end of the section.

Inputs are object files, directories (searched for *.o) and @list files (one path per line, as
written by native/cmake/census.cmake). Uses the Xcode tools nm and otool.

    native/tools/symbol_census.py build/native-mac/CMakeFiles/SSystem.dir
    native/tools/symbol_census.py @build/native-mac/link_census/objects.txt --out report.txt
    native/tools/symbol_census.py --dol            # main.dol units of build/native-mac

--dol [BUILD_DIR] takes the main.dol units, i.e. the non-REL objects the link census lists in
BUILD_DIR/link_census/objects.txt (written at configure time by native/cmake/census.cmake;
BUILD_DIR defaults to build/native-mac), and exits 1 on any duplicate strong definition (the
phase 2 exit check, step 2.9).

--all [BUILD_DIR] (step 3.1) takes every object phase 3 links into one executable: the main.dol
units (objects.txt), the REL units (rel_objects.txt) and cos_sdk (sdk_objects.txt, when built).
Objects of REL units are marked "(REL)". It adds a source-level section, since a struct with no
virtual functions and no out-of-line members leaves no symbol for nm to see: named types (class,
struct, union) defined at namespace scope, outside unnamed namespaces, in more than one source
file. The scan covers the units' .c/.cpp files, the .inc files they include, and the headers of
game/include (a header counts only when a unit source defines the same name). Each
#if/#elif/#else branch is read from the same brace state, so alternatives of one definition in
one file count once. The report goes to BUILD_DIR/symbol_census.txt unless --out says otherwise;
`ninja cos_symbol_census` writes it too. It only reports (exit 0) unless --dups or --fail-on-dups.

--dups prints only the duplicate strong definitions and exits 1 if there is any (step 3.2).
"""

from __future__ import annotations

import argparse
import bisect
import concurrent.futures
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field

BATCH = 64  # object files per nm/otool call
# build/native-mac of this repository (native/tools/ -> ../../build/native-mac).
_DEFAULT_BUILD = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                               "..", "..", "build", "native-mac"))
# native/ of this repository: CMake names an object after its source path relative to it
# (CMakeFiles/<target>.dir/[./]game/src/d/x.cpp.o for game/src/d/x.cpp).
_NATIVE_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

# nm -m: "<value> (<segment>,<section>) <attributes> <name>"; undefined has no value.
_NM_LINE = re.compile(r"^(?P<value>[0-9a-fA-F]+)?\s*\((?P<sect>[^)]*)\)\s+(?P<rest>.*)$")
_NM_TRAILER = re.compile(r"\s+\((?:from [^)]*|dynamically looked up)\)$")


@dataclass
class Definition:
    obj: str
    name: str
    section: str  # "__TEXT,__text", "absolute", "common"...
    weak: bool
    size: int | None  # None when unknown (absolute, common)


@dataclass
class ObjectSymbols:
    path: str
    defined: list[Definition] = field(default_factory=list)  # external definitions only
    undefined: set[str] = field(default_factory=set)


def read_inputs(items: list[str]) -> list[str]:
    """Expand object paths, directories (recursive *.o) and @list files, keeping order."""
    out: list[str] = []
    for item in items:
        if item.startswith("@"):
            with open(item[1:], encoding="utf-8") as f:
                out.extend(line.strip() for line in f if line.strip() and not line.startswith("#"))
        elif os.path.isdir(item):
            found = []
            for root, _dirs, files in os.walk(item):
                found.extend(os.path.join(root, n) for n in files if n.endswith(".o"))
            out.extend(sorted(found))
        else:
            out.append(item)
    seen: set[str] = set()
    unique = []
    for p in out:
        if p not in seen:
            seen.add(p)
            unique.append(p)
    return unique


def _split_by_prefix(text: str, paths: list[str]) -> dict[str, list[str]]:
    """Split `nm -A` output ("<path>: <line>") into per-file lines."""
    per: dict[str, list[str]] = {p: [] for p in paths}
    for line in text.splitlines():
        path, sep, rest = line.partition(": ")
        if sep and path in per:
            per[path].append(rest)
    return per


def _split_by_file(text: str, paths: list[str]) -> dict[str, list[str]]:
    """Split the output of a multi-file otool call into per-file lines ("<path>:" headers)."""
    headers = {p + ":": p for p in paths}
    per: dict[str, list[str]] = {p: [] for p in paths}
    current = paths[0] if len(paths) == 1 else None
    for line in text.splitlines():
        if line in headers:
            current = headers[line]
            continue
        if current is not None:
            per[current].append(line)
    return per


def _run(cmd: list[str]) -> str:
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        raise SystemExit(f"symbol_census: {' '.join(cmd[:3])} ... failed:\n{res.stderr}")
    return res.stdout


def _section_ends(lines: list[str]) -> dict[str, int]:
    """'segname,sectname' -> end address, from `otool -l`."""
    ends: dict[str, int] = {}
    sect = seg = None
    addr = None
    for line in lines:
        parts = line.split()
        if len(parts) != 2:
            continue
        key, val = parts
        if key == "sectname":
            sect, seg, addr = val, None, None
        elif key == "segname" and sect is not None:
            seg = val
        elif key == "addr" and sect is not None:
            addr = int(val, 16)
        elif key == "size" and sect is not None and seg is not None and addr is not None:
            ends[f"{seg},{sect}"] = addr + int(val, 16)
            sect = None
    return ends


def _parse_object(path: str, nm_lines: list[str], ends: dict[str, int]) -> ObjectSymbols:
    obj = ObjectSymbols(path)
    # (section, address) of every symbol, local ones included, to bound sizes.
    starts: dict[str, list[int]] = {}
    pending: list[tuple[str, int | None, str, bool]] = []
    for line in nm_lines:
        m = _NM_LINE.match(line)
        if not m:
            continue
        rest = _NM_TRAILER.sub("", m.group("rest").strip())
        tokens = rest.split()
        if not tokens:
            continue
        name = tokens[-1]
        attrs = tokens[:-1]
        sect = m.group("sect")
        if sect == "undefined":
            obj.undefined.add(name)
            continue
        value = int(m.group("value"), 16) if m.group("value") else None
        if "," in sect and value is not None:
            starts.setdefault(sect, []).append(value)
        external = "external" in attrs and "non-external" not in attrs
        if not external:
            continue
        pending.append((sect, value, name, "weak" in attrs))
    for addrs in starts.values():
        addrs.sort()
    for sect, value, name, weak in pending:
        size = None
        if "," in sect and value is not None:
            addrs = starts[sect]
            i = bisect.bisect_right(addrs, value)
            end = addrs[i] if i < len(addrs) else ends.get(sect)
            if end is not None:
                size = end - value
        obj.defined.append(Definition(path, name, sect, weak, size))
    return obj


def _scan_batch(paths: list[str]) -> list[ObjectSymbols]:
    nm_out = _split_by_prefix(_run(["nm", "-A", "-m", "-n"] + paths), paths)
    otool_out = _split_by_file(_run(["otool", "-l"] + paths), paths)
    return [_parse_object(p, nm_out[p], _section_ends(otool_out[p])) for p in paths]


def scan(paths: list[str], jobs: int | None = None) -> list[ObjectSymbols]:
    """nm and otool over every object, in parallel batches; result in input order."""
    batches = [paths[i:i + BATCH] for i in range(0, len(paths), BATCH)]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs or os.cpu_count() or 4) as ex:
        results = list(ex.map(_scan_batch, batches))
    return [o for batch in results for o in batch]


def duplicate_strong(objects: list[ObjectSymbols]) -> dict[str, list[str]]:
    """Symbol -> objects (input order) for external non-weak symbols defined more than once."""
    owners: dict[str, list[str]] = {}
    for o in objects:
        for d in o.defined:
            if not d.weak:
                owners.setdefault(d.name, []).append(o.path)
    return {n: objs for n, objs in owners.items() if len(objs) > 1}


def weak_size_mismatches(objects: list[ObjectSymbols]) -> dict[str, list[Definition]]:
    """Symbol -> definitions for external weak symbols whose sizes differ between objects."""
    defs: dict[str, list[Definition]] = {}
    for o in objects:
        for d in o.defined:
            if d.weak and d.size is not None:
                defs.setdefault(d.name, []).append(d)
    return {n: ds for n, ds in defs.items() if len({d.size for d in ds}) > 1}


def demangle(names: list[str]) -> dict[str, str]:
    """Mangled (with Mach-O's leading '_') -> readable, through one c++filt call."""
    if not names:
        return {}
    res = subprocess.run(["c++filt"], input="\n".join(names) + "\n", stdout=subprocess.PIPE,
                         text=True, check=True)
    out = res.stdout.splitlines()
    table = {}
    for mangled, pretty in zip(names, out):
        if pretty == mangled and mangled.startswith("_"):
            pretty = mangled[1:]  # a C name: drop the Mach-O underscore
        table[mangled] = pretty
    return table


def is_code_section(section: str) -> bool:
    return section.startswith("__TEXT,__text") or section.endswith(",__text")


def _short(path: str, root: str | None) -> str:
    if root:
        rel = os.path.relpath(path, root)
        if not rel.startswith(".."):
            return rel
    return path


# ---- source-level census of type definitions (--all) -----------------------------------------

_TOKEN = re.compile(r"[A-Za-z_]\w*|::|[{}();:<>,=*&\[\]]")
_COMMENT_OR_LITERAL = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)
_PP_INCLUDE_INC = re.compile(r'^\s*#\s*include\s*"([^"]+\.inc)"', re.M)
_TYPE_KEYS = ("class", "struct", "union")


def object_source(obj: str, native_root: str = _NATIVE_ROOT) -> str | None:
    """Source file of a CMake object (CMakeFiles/<t>.dir/[./]<path under native/>.o), if it exists."""
    m = re.search(r"/CMakeFiles/[^/]+\.dir/(?:\./)?(.+)\.o$", obj.replace(os.sep, "/"))
    if not m:
        return None
    src = os.path.join(native_root, m.group(1))
    return src if os.path.isfile(src) else None


def _blank_keep_lines(m: re.Match) -> str:
    text = m.group(0)
    if text.startswith(("\"", "'")):
        return '""'
    return "\n" * text.count("\n") or " "


def _logical_lines(text: str) -> list[str]:
    """Comments and literals removed, backslash continuations joined."""
    text = _COMMENT_OR_LITERAL.sub(_blank_keep_lines, text)
    return text.replace("\\\n", " ").split("\n")


def namespace_scope_types(text: str) -> tuple[set[str], bool]:
    """Qualified names of the class/struct/union definitions at namespace scope (not in an unnamed
    namespace, not templates, not local or nested types), and whether the braces balanced (False
    means the scan lost track somewhere, so it may have missed definitions). A heuristic token
    scan, not a parser."""
    found: set[str] = set()
    balanced = True
    # Brace stack: ("ns", name), ("anon", None), ("extern", None) or ("other", None).
    stack: list[tuple[str, str | None]] = []
    # #if nesting: [stack at the #if, stack at the end of the first branch or None].
    pp: list[list] = []
    template_pending = False
    tokens_window: list[str] = []
    for line in _logical_lines(text):
        stripped = line.strip()
        if stripped.startswith("#"):
            directive = stripped[1:].strip().split(None, 1)
            word = directive[0] if directive else ""
            if word in ("if", "ifdef", "ifndef"):
                pp.append([list(stack), None])
            elif word in ("elif", "else") and pp:
                if pp[-1][1] is None:
                    pp[-1][1] = list(stack)
                stack = list(pp[-1][0])
            elif word == "endif" and pp:
                start, first = pp.pop()
                if first is not None:
                    stack = first
            continue
        for tok in _TOKEN.findall(line):
            tokens_window.append(tok)
            if tok == "template":
                template_pending = True
            elif tok == "{":
                kind: tuple[str, str | None] = ("other", None)
                w = tokens_window
                if len(w) >= 2 and w[-2] == "namespace":
                    kind = ("anon", None)
                elif len(w) >= 3 and w[-3] == "namespace":
                    kind = ("ns", w[-2])
                elif len(w) >= 2 and w[-2] == "extern":
                    kind = ("extern", None)
                else:
                    name = _type_head(w)
                    if name and not template_pending and all(k in ("ns", "extern") for k, _ in stack):
                        found.add("::".join([n for k, n in stack if k == "ns"] + [name]))
                    if name:
                        kind = ("type", name)
                stack.append(kind)
                template_pending = False
                tokens_window = []
            elif tok == "}":
                if stack:
                    stack.pop()
                else:
                    balanced = False
                template_pending = False
                tokens_window = []
            elif tok == ";":
                template_pending = False
                tokens_window = []
    return found, balanced and not stack and not pp


def _type_head(w: list[str]) -> str | None:
    """Name of the type whose body the '{' ending `w` opens ("struct A : public B {"), or None."""
    for i, tok in enumerate(w):
        if tok in _TYPE_KEYS:
            if i > 0 and w[i - 1] in ("enum", "friend"):
                return None
            rest = w[i + 1:-1]
            if not rest or not re.match(r"[A-Za-z_]\w*$", rest[0]):
                return None  # anonymous struct
            name = rest[0]
            j = 1
            while j + 1 < len(rest) and rest[j] == "::":  # struct A::B {
                name += "::" + rest[j + 1]
                j += 2
            tail = rest[j:]
            if tail and tail[0] == "final":
                tail = tail[1:]
            if not tail or tail[0] == ":" and "(" not in tail and "=" not in tail:
                return name
            return None
    return None


def type_definitions(objects: list[str], native_root: str = _NATIVE_ROOT
                     ) -> tuple[dict[str, list[str]], list[str]]:
    """Type name -> source files that define it, for names defined in more than one file, at least
    one of them a unit source (.c/.cpp) or an .inc it includes; and the unit files whose braces the
    scan could not balance."""
    unit_files: list[str] = []
    seen: set[str] = set()
    for obj in objects:
        src = object_source(obj, native_root)
        if not src or src in seen:
            continue
        seen.add(src)
        unit_files.append(src)
        with open(src, encoding="utf-8", errors="replace") as f:
            text = f.read()
        for inc in _PP_INCLUDE_INC.findall(text):
            for base in (os.path.dirname(src), os.path.join(native_root, "..", "game", "src")):
                cand = os.path.normpath(os.path.join(base, inc))
                if os.path.isfile(cand):
                    if cand not in seen:
                        seen.add(cand)
                        unit_files.append(cand)
                    break
    defs: dict[str, list[str]] = {}
    unbalanced: list[str] = []
    for path in unit_files:
        with open(path, encoding="utf-8", errors="replace") as f:
            names, balanced = namespace_scope_types(f.read())
        if not balanced:
            unbalanced.append(path)
        for name in names:
            defs.setdefault(name, []).append(path)
    include_root = os.path.join(native_root, "..", "game", "include")
    for root, _dirs, files in os.walk(include_root):
        for n in sorted(files):
            if not n.endswith((".h", ".hpp", ".inc")):
                continue
            path = os.path.join(root, n)
            with open(path, encoding="utf-8", errors="replace") as f:
                text = f.read()
            for name in namespace_scope_types(text)[0]:
                if name in defs:
                    defs[name].append(path)
    return {n: sorted(fs) for n, fs in defs.items() if len(fs) > 1}, sorted(unbalanced)


def format_report(objects: list[ObjectSymbols], dups: dict[str, list[str]],
                  weak: dict[str, list[Definition]], root: str | None = None,
                  limit: int = 0, rel: set[str] | None = None,
                  types: dict[str, list[str]] | None = None, groups: str = "",
                  unscanned: list[str] | None = None,
                  dups_only: bool = False) -> str:
    names = sorted(set(dups) | (set() if dups_only else set(weak)))
    pretty = demangle(names)
    if dups_only:
        weak = {}
    weak_data = {n: ds for n, ds in weak.items() if not is_code_section(ds[0].section)}
    weak_code = {n: ds for n, ds in weak.items() if is_code_section(ds[0].section)}
    rel = rel or set()

    def obj_label(p: str) -> str:
        return _short(p, root) + ("  (REL)" if p in rel else "")

    lines = [f"symbol_census: {len(objects)} objects{groups}",
             f"duplicate strong definitions: {len(dups)}"]
    if not dups_only:
        lines.append(f"weak definitions with differing sizes: {len(weak_data)} data, "
                     f"{len(weak_code)} code")
    if types is not None and not dups_only:
        lines.append(f"types defined in more than one source file: {len(types)}"
                     f" ({len(unscanned or [])} sources with unbalanced braces)")
    lines.append("")

    def cap(items):
        return items if limit <= 0 else items[:limit]

    if dups:
        lines.append(f"== duplicate strong definitions ({len(dups)}) ==")
        for n in cap(sorted(dups, key=lambda s: pretty[s])):
            lines.append(f"{pretty[n]}  [{n}]  x{len(dups[n])}")
            lines.extend(f"    {obj_label(p)}" for p in dups[n])
        lines.append("")
    for title, table in (("data", weak_data), ("code", weak_code)):
        if not table:
            continue
        lines.append(f"== weak {title} definitions with differing sizes ({len(table)}) ==")
        for n in cap(sorted(table, key=lambda s: pretty[s])):
            ds = table[n]
            sizes = sorted({d.size for d in ds})
            lines.append(f"{pretty[n]}  [{n}]  sizes {', '.join(map(str, sizes))}")
            lines.extend(f"    {d.size:>6}  {obj_label(d.obj)}" for d in ds)
        lines.append("")
    src_root = os.path.dirname(_NATIVE_ROOT)
    if types and not dups_only:
        lines.append(f"== types defined in more than one source file ({len(types)}) ==")
        lines.append("(ODR suspects: the linker merges their inline members and vtables silently)")
        for n in cap(sorted(types)):
            lines.append(f"{n}  x{len(types[n])}")
            lines.extend(f"    {os.path.relpath(p, src_root)}" for p in types[n])
        lines.append("")
    if unscanned and not dups_only:
        lines.append(f"== sources the type scan could not balance ({len(unscanned)}) ==")
        lines.append("(their braces did not balance under the #if branch rule: definitions after "
                     "the first imbalance may be missed)")
        lines.extend(f"    {os.path.relpath(p, src_root)}" for p in unscanned)
        lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("inputs", nargs="*", help="object files, directories, or @list files")
    ap.add_argument("--dol", nargs="?", metavar="BUILD_DIR", const=_DEFAULT_BUILD,
                    help="add the main.dol units of the link census in BUILD_DIR (default "
                         "build/native-mac) and fail on duplicate strong definitions")
    ap.add_argument("--all", nargs="?", metavar="BUILD_DIR", const=_DEFAULT_BUILD,
                    help="every object of the phase 3 link in BUILD_DIR (default build/native-mac): "
                         "main.dol units, REL units and cos_sdk, plus the source-level census of "
                         "type definitions; report in BUILD_DIR/symbol_census.txt")
    ap.add_argument("--dups", action="store_true",
                    help="report only duplicate strong definitions; exit 1 if there is any")
    ap.add_argument("--out", help="write the report here instead of stdout")
    ap.add_argument("--root", help="print object paths relative to this directory")
    ap.add_argument("--limit", type=int, default=0, help="at most this many symbols per section")
    ap.add_argument("--fail-on-dups", action="store_true",
                    help="exit 1 if there is any duplicate strong definition")
    args = ap.parse_args(argv)

    inputs = list(args.inputs)
    if args.dol:
        dol_list = os.path.join(args.dol, "link_census", "objects.txt")
        if not os.path.isfile(dol_list):
            print(f"symbol_census: {dol_list} missing (configure {args.dol} first)", file=sys.stderr)
            return 2
        inputs.append("@" + dol_list)
        args.fail_on_dups = True
        if args.root is None:
            args.root = args.dol
    rel_paths: set[str] = set()
    groups = ""
    if args.all:
        if args.dol:
            ap.error("--all already includes the --dol objects")
        census = os.path.join(args.all, "link_census")
        lists = [("main.dol", "objects.txt", True), ("REL", "rel_objects.txt", True),
                 ("cos_sdk", "sdk_objects.txt", False)]
        counts = []
        for label, name, required in lists:
            path = os.path.join(census, name)
            if not os.path.isfile(path):
                if required:
                    print(f"symbol_census: {path} missing (configure {args.all} first)",
                          file=sys.stderr)
                    return 2
                continue
            group = read_inputs(["@" + path])
            if label == "REL":
                rel_paths.update(group)
            counts.append(f"{len(group)} {label}")
            inputs.append("@" + path)
        groups = " (" + ", ".join(counts) + ")"
        if args.root is None:
            args.root = args.all
        if args.out is None and not args.dups:
            args.out = os.path.join(args.all, "symbol_census.txt")
    if args.dups:
        args.fail_on_dups = True
    if not inputs:
        ap.error("no inputs (give object files, directories, @list files, --dol or --all)")
    paths = read_inputs(inputs)
    if not paths:
        print("symbol_census: no object files", file=sys.stderr)
        return 2
    missing = [p for p in paths if not os.path.isfile(p)]
    if missing:
        print(f"symbol_census: {len(missing)} inputs missing, first: {missing[0]}", file=sys.stderr)
        return 2
    objects = scan(paths)
    dups = duplicate_strong(objects)
    weak = weak_size_mismatches(objects)
    types, unscanned = (type_definitions(paths) if args.all and not args.dups else (None, None))
    report = format_report(objects, dups, weak, args.root, args.limit, rel_paths, types, groups,
                           unscanned, dups_only=args.dups)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(report + "\n")
        print("\n".join(report.split("\n\n", 1)[0].splitlines()))
        print(f"symbol_census: report in {args.out}")
    else:
        print(report)
    return 1 if (args.fail_on_dups and dups) else 0


if __name__ == "__main__":
    sys.exit(main())
