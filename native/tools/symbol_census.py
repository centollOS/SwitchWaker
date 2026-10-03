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


def format_report(objects: list[ObjectSymbols], dups: dict[str, list[str]],
                  weak: dict[str, list[Definition]], root: str | None = None,
                  limit: int = 0) -> str:
    names = sorted(set(dups) | set(weak))
    pretty = demangle(names)
    weak_data = {n: ds for n, ds in weak.items() if not is_code_section(ds[0].section)}
    weak_code = {n: ds for n, ds in weak.items() if is_code_section(ds[0].section)}
    lines = [
        f"symbol_census: {len(objects)} objects",
        f"duplicate strong definitions: {len(dups)}",
        f"weak definitions with differing sizes: {len(weak_data)} data, {len(weak_code)} code",
        "",
    ]

    def cap(items):
        return items if limit <= 0 else items[:limit]

    if dups:
        lines.append(f"== duplicate strong definitions ({len(dups)}) ==")
        for n in cap(sorted(dups, key=lambda s: pretty[s])):
            lines.append(f"{pretty[n]}  [{n}]  x{len(dups[n])}")
            lines.extend(f"    {_short(p, root)}" for p in dups[n])
        lines.append("")
    for title, table in (("data", weak_data), ("code", weak_code)):
        if not table:
            continue
        lines.append(f"== weak {title} definitions with differing sizes ({len(table)}) ==")
        for n in cap(sorted(table, key=lambda s: pretty[s])):
            ds = table[n]
            sizes = sorted({d.size for d in ds})
            lines.append(f"{pretty[n]}  [{n}]  sizes {', '.join(map(str, sizes))}")
            lines.extend(f"    {d.size:>6}  {_short(d.obj, root)}" for d in ds)
        lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("inputs", nargs="+", help="object files, directories, or @list files")
    ap.add_argument("--out", help="write the report here instead of stdout")
    ap.add_argument("--root", help="print object paths relative to this directory")
    ap.add_argument("--limit", type=int, default=0, help="at most this many symbols per section")
    ap.add_argument("--fail-on-dups", action="store_true",
                    help="exit 1 if there is any duplicate strong definition")
    args = ap.parse_args(argv)

    paths = read_inputs(args.inputs)
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
    report = format_report(objects, dups, weak, args.root, args.limit)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(report + "\n")
        print("\n".join(report.splitlines()[:3]))
        print(f"symbol_census: report in {args.out}")
    else:
        print(report)
    return 1 if (args.fail_on_dups and dups) else 0


if __name__ == "__main__":
    sys.exit(main())
