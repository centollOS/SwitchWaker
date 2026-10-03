#!/usr/bin/env python3
"""Layout check of the disc-mapped structs (docs/NATIVE_PORT_PHASE4_6.md, step 4.0c).

The decomp records the GameCube layout of a struct in comments: `/* 0x1C */ u32 imageOffset;` before
each field and `};  // Size: 0x20` after the closing brace. A struct whose bytes come straight from
the disc (or go to it) must keep that layout on the host too, or every field after the first
difference reads the wrong bytes. This script turns those comments into
    static_assert(offsetof(ResTIMG, imageOffset) == 0x1C, "...");
for the structs listed in native/check/layout_headers.txt, and the build target cos_layout_check
compiles the result with the game's flags: the target builds exactly when every check holds.

Input files
  native/check/layout_headers.txt   the disc-mapped structs, grouped under their header:
        JSystem/JUtility/JUTTexture.h          header, relative to game/include
            ResTIMG                            struct (qualified name: JKRArchive::SDIFileEntry)
            JKRArchive::SDIDirEntry size=0x10 type=0x0 name_offset=0x4
                                               size= and <field>= add or confirm checks for
                                               fields without an offset comment
  native/check/layout_xfail.txt     the checks known to fail until their format step lands:
        J3DModelFileData                       the whole struct: at least one of its checks fails
        SArcHeader.signature                   one field (or <struct>.sizeof)
    The list is strict: an entry whose checks all hold again fails the build ("XPASS"), so each
    format step removes its structs from the list as it fixes them.

Checks
  <struct>.<field>   offsetof(<struct>, <field>) == the /* 0xNN */ comment (or the field= override)
  <struct>.sizeof    sizeof(<struct>) == the `// Size: 0xNN` comment after the brace (or size=)
  A bitfield gets a check that always fails: MWCC allocates bitfields from the most significant
  bit, clang on a little-endian host from the least, so a disc-mapped bitfield must become a
  mask over a BE(T) field. Until then it is xfailed.

Comment parsing: each listed header is read with a small preprocessor (TARGET_PC=1, VERSION from
the build, __MWERKS__ and DEBUG undefined, the header's own simple #defines), so the fields seen
are the ones clang compiles. Fields of an anonymous union or struct belong to the enclosing struct
(offsetof reaches them by name). A comment without a parsable data member (base classes, vtables,
methods, members of a named anonymous-type member) is skipped, never guessed; --list shows them.

Usage
  layout_check.py --out <file.cpp> [--depfile <file.d>]      generate the check unit (the build)
  layout_check.py --list [STRUCT...]                         the checks parsed per struct
  layout_check.py --scan HEADER                              every struct of a header with its
                                                             comment count (to choose new entries)
  layout_check.py --discover --build build/native-mac [--write-xfail]
        compile every check as a plain assertion (same flags as cos_layout_check, from
        compile_commands.json) and print the failures with the host offsets; --write-xfail
        rewrites layout_xfail.txt with one entry per failing struct.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field

NATIVE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REPO = os.path.dirname(NATIVE)
INCLUDE_ROOT = os.path.join(NATIVE, "..", "game", "include")
HEADERS_TXT = os.path.join(NATIVE, "check", "layout_headers.txt")
XFAIL_TXT = os.path.join(NATIVE, "check", "layout_xfail.txt")
GENERATED_NAME = "cos_layout_check.cpp"

# The macros a game unit sees (native/cmake/GameConfig.cmake and global.h); anything else is
# undefined, as for the real preprocessor.
BASE_DEFINES = {
    "TARGET_PC": 1,
    "VERSION": 2,
    "VERSION_DEMO": 0,
    "VERSION_JPN": 1,
    "VERSION_USA": 2,
    "VERSION_PAL": 3,
    "NDEBUG": 1,
    "__cplusplus": 202002,
}

OFFSET_COMMENT_RE = re.compile(r"^\s*/\*\s*(0x[0-9A-Fa-f]+)\b[^*]*\*/(.*)$")
SIZE_COMMENT_RE = re.compile(r"(?://|/\*)\s*[Ss]ize\s*[:=]?\s*(0x[0-9A-Fa-f]+)")
IDENT_RE = re.compile(r"[A-Za-z_]\w*")
RECORD_HEAD_RE = re.compile(
    r"^(?P<template>template\s*<.*>\s*)?(?P<typedef>typedef\s+)?(?P<kind>struct|class|union)"
    r"(?:\s+__attribute__\s*\(\(.*?\)\))?(?:\s+(?P<name>[A-Za-z_]\w*))?\s*(?:final\s*)?(?::[^{]*)?$",
    re.S)
NAMESPACE_HEAD_RE = re.compile(r"^namespace(?:\s+(?P<name>[A-Za-z_][\w:]*))?$")
NON_FIELD_PREFIXES = ("static ", "virtual ", "typedef ", "using ", "friend ", "enum ", "template",
                      "inline ", "explicit ", "operator", "~", "public:", "private:", "protected:")


class LayoutError(Exception):
    pass


@dataclass
class Field:
    name: str
    offset: int | None      # None: a bitfield (always-failing check)
    line: int
    bitfield: bool = False


@dataclass
class Record:
    qual: str
    header: str
    line: int
    fields: list[Field] = field(default_factory=list)
    size: int | None = None
    skipped: list[tuple[int, str]] = field(default_factory=list)
    template: bool = False


@dataclass
class Scope:
    kind: str               # "record", "namespace", "other"
    name: str | None = None
    typedef: bool = False
    template: bool = False
    line: int = 0
    fields: list[Field] = field(default_factory=list)
    skipped: list[tuple[int, str]] = field(default_factory=list)


# --------------------------------------------------------------------------------------------
# Preprocessing


def _eval_condition(expr: str, defines: dict[str, int]) -> bool:
    expr = re.sub(r"//.*$", "", expr)
    expr = re.sub(r"/\*.*?\*/", "", expr)
    expr = re.sub(r"defined\s*\(\s*(\w+)\s*\)", lambda m: "1" if m.group(1) in defines else "0", expr)
    expr = re.sub(r"defined\s+(\w+)", lambda m: "1" if m.group(1) in defines else "0", expr)

    def ident(m: re.Match) -> str:
        return str(int(defines.get(m.group(0), 0)))

    expr = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)[uUlL]*\b", lambda m: str(int(m.group(1), 0)), expr)
    expr = re.sub(r"[A-Za-z_]\w*", ident, expr)
    expr = expr.replace("&&", " and ").replace("||", " or ")
    expr = re.sub(r"!(?!=)", " not ", expr)
    try:
        return bool(eval(expr, {"__builtins__": {}}, {}))
    except Exception as e:  # noqa: BLE001
        raise LayoutError(f"cannot evaluate #if {expr!r}: {e}") from None


def preprocess(lines: list[str], defines: dict[str, int]) -> list[tuple[int, str]]:
    """The active lines (1-based numbers) under the given macros. Directives are dropped."""
    defines = dict(defines)
    out: list[tuple[int, str]] = []
    # Stack of (active, any_branch_taken, parent_active).
    stack: list[tuple[bool, bool, bool]] = []
    active = True
    i = 0
    n = len(lines)
    while i < n:
        start = i
        line = lines[i]
        # Join continued directive lines.
        text = line
        while text.rstrip().endswith("\\") and i + 1 < n and text.lstrip().startswith("#"):
            i += 1
            text = text.rstrip()[:-1] + " " + lines[i]
        i += 1
        m = re.match(r"^\s*#\s*(\w+)\s*(.*)$", text)
        if not m:
            if active:
                out.append((start + 1, line.rstrip("\n")))
            continue
        d, rest = m.group(1), m.group(2).strip()
        if d in ("if", "ifdef", "ifndef"):
            if not active:
                stack.append((False, True, False))
                continue
            if d == "if":
                cond = _eval_condition(rest, defines)
            else:
                name = rest.split()[0] if rest.split() else ""
                cond = (name in defines) == (d == "ifdef")
            stack.append((cond, cond, True))
            active = cond
        elif d == "elif":
            if not stack:
                raise LayoutError(f"line {start + 1}: #elif without #if")
            _, taken, parent = stack[-1]
            cond = parent and not taken and _eval_condition(rest, defines)
            stack[-1] = (cond, taken or cond, parent)
            active = cond
        elif d == "else":
            if not stack:
                raise LayoutError(f"line {start + 1}: #else without #if")
            _, taken, parent = stack[-1]
            cond = parent and not taken
            stack[-1] = (cond, True, parent)
            active = cond
        elif d == "endif":
            if not stack:
                raise LayoutError(f"line {start + 1}: #endif without #if")
            stack.pop()
            active = stack[-1][0] if stack else True
        elif d == "define" and active:
            dm = re.match(r"^([A-Za-z_]\w*)(?!\()\s*(.*)$", rest)
            if dm:
                val = dm.group(2).strip()
                try:
                    defines[dm.group(1)] = int(val, 0) if val else 1
                except ValueError:
                    defines[dm.group(1)] = 1
        elif d == "undef" and active:
            defines.pop(rest.split()[0] if rest else "", None)
    if stack:
        raise LayoutError("unterminated #if")
    return out


# --------------------------------------------------------------------------------------------
# Struct and comment parsing


def _strip_code(line: str, in_comment: bool) -> tuple[str, bool]:
    """The line without comments and string/char literals (replaced by spaces)."""
    out = []
    i = 0
    n = len(line)
    while i < n:
        if in_comment:
            j = line.find("*/", i)
            if j < 0:
                return "".join(out), True
            i = j + 2
            in_comment = False
            out.append(" ")
            continue
        c = line[i]
        if line.startswith("/*", i):
            in_comment = True
            i += 2
            continue
        if line.startswith("//", i):
            break
        if c in "\"'":
            j = i + 1
            while j < n and line[j] != c:
                j += 2 if line[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
            continue
        out.append(c)
        i += 1
    return "".join(out), in_comment


def _split_top_level(text: str, sep: str) -> list[str]:
    parts, depth, cur = [], 0, []
    for c in text:
        if c in "<([":
            depth += 1
        elif c in ">)]":
            depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append("".join(cur))
    return parts


def parse_declarator(stmt: str) -> tuple[str | None, bool, str]:
    """(member name, is bitfield, reason when no name) of one member declaration statement."""
    s = " ".join(stmt.split())
    if not s:
        return None, False, "no declaration after the comment"
    if s.startswith(NON_FIELD_PREFIXES) or s.startswith("}"):
        return None, False, "not a data member"
    if "{" in s:
        return None, False, "inline type or body"
    if "(" in s:
        fp = re.search(r"\(\s*(?:\w+\s*::\s*)*\*\s*(?:const\s+)?([A-Za-z_]\w*)\s*\)", s)
        if fp:
            return fp.group(1), False, ""
        return None, False, "function declaration"
    s = re.sub(r"\[[^\]]*\]", "", s)
    s = s.split("=", 1)[0]
    first = _split_top_level(s, ",")[0]
    plain = first.replace("::", "__")
    bitfield = False
    if re.search(r"(?<!:):(?!:)", plain):
        bitfield = True
        first = first[:len(plain.split(":", 1)[0])]
    idents = IDENT_RE.findall(re.sub(r"<[^<>]*>", " ", first))
    if len(idents) < 2:
        return None, False, "no type and name"
    return idents[-1], bitfield, ""


def parse_header(path: str, rel: str, defines: dict[str, int]) -> dict[str, Record]:
    with open(path, encoding="utf-8", errors="replace") as f:
        raw = f.readlines()
    active = preprocess(raw, defines)
    records: dict[str, Record] = {}
    stack: list[Scope] = []
    pending = ""          # text since the last ; { or }
    closed: Scope | None = None   # record scope whose trailing "Name;" is still being read
    closed_line = 0
    in_comment = False

    def qual_name(name: str) -> str:
        parts = [s.name for s in stack if s.kind in ("record", "namespace") and s.name]
        return "::".join(parts + [name])

    def nearest_record() -> Scope | None:
        if stack and stack[-1].kind == "record":
            return stack[-1]
        return None

    def finish_record(scope: Scope, trailer: str, line_no: int) -> None:
        names = IDENT_RE.findall(trailer)
        if scope.typedef and not scope.name and names:
            scope.name = names[-1]
        size = None
        for k in (line_no - 1, line_no):
            if 0 <= k < len(raw):
                m = SIZE_COMMENT_RE.search(raw[k])
                if m and (k == line_no - 1 or raw[k].lstrip().startswith(("//", "/*"))):
                    size = int(m.group(1), 16)
                    break
        if not scope.name:
            parent = nearest_record()
            if parent is None:
                return
            if names and not scope.typedef:
                for fl in scope.fields:
                    parent.skipped.append((fl.line, f"{fl.name}: member of an anonymous-type "
                                                    f"member '{names[0]}'"))
                parent.skipped.extend(scope.skipped)
                return
            # Anonymous union/struct: its members are members of the enclosing record.
            parent.fields.extend(scope.fields)
            parent.skipped.extend(scope.skipped)
            return
        q = qual_name(scope.name)
        rec = Record(q, rel, scope.line, list(scope.fields), size, list(scope.skipped), scope.template)
        if q in records:
            # The same name twice (e.g. both branches of an unknown #if): keep the first.
            return
        records[q] = rec

    for line_no, line in active:
        m = OFFSET_COMMENT_RE.match(line)
        if m and not in_comment:
            rec = nearest_record()
            if rec is not None and not rec.template:
                stmt = m.group(2)
                code_stmt, _ = _strip_code(stmt, False)
                if ";" in code_stmt:
                    name, bitfield, why = parse_declarator(code_stmt.split(";", 1)[0])
                elif not code_stmt.strip():
                    name, bitfield, why = None, False, "comment only"
                else:
                    name, bitfield, why = None, False, "declaration continues on the next line"
                if name:
                    if any(f.name == name for f in rec.fields):
                        rec.skipped.append((line_no, f"{name}: declared twice"))
                    else:
                        rec.fields.append(Field(name, None if bitfield else int(m.group(1), 16),
                                                line_no, bitfield))
                else:
                    rec.skipped.append((line_no, f"{why}: {stmt.strip()[:60]}"))
        code, in_comment = _strip_code(line, in_comment)
        for c in code:
            if c == "{":
                if closed is not None:
                    closed = None
                head = " ".join(pending.split())
                head = re.sub(r"^(?:public|private|protected)\s*:\s*", "", head)
                rm = RECORD_HEAD_RE.match(head)
                nm = NAMESPACE_HEAD_RE.match(head)
                if rm:
                    in_template = any(s.template for s in stack if s.kind == "record")
                    stack.append(Scope("record", rm.group("name"), bool(rm.group("typedef")),
                                       bool(rm.group("template")) or in_template, line_no))
                elif nm:
                    stack.append(Scope("namespace", nm.group("name")))
                elif head.startswith('extern "') or head.startswith("extern \"\""):
                    stack.append(Scope("namespace", None))
                else:
                    stack.append(Scope("other"))
                pending = ""
            elif c == "}":
                if not stack:
                    raise LayoutError(f"{rel}:{line_no}: unbalanced '}}'")
                s = stack.pop()
                if s.kind == "record":
                    closed = s
                    closed_line = line_no
                pending = ""
            elif c == ";":
                if closed is not None:
                    finish_record(closed, pending, closed_line)
                    closed = None
                pending = ""
            else:
                pending += c
        pending += " "
    if stack:
        raise LayoutError(f"{rel}: unbalanced braces at end of file")
    return records


def scan_headers(headers: list[str], defines: dict[str, int]) -> dict[str, Record]:
    out: dict[str, Record] = {}
    for rel in headers:
        path = os.path.join(INCLUDE_ROOT, rel)
        if not os.path.isfile(path):
            raise LayoutError(f"layout_headers.txt: no header {rel} under game/include")
        for q, r in parse_header(path, rel, defines).items():
            out.setdefault(q, r)
    return out


# --------------------------------------------------------------------------------------------
# Input lists


@dataclass
class Listed:
    qual: str
    header: str
    line: int                       # line in layout_headers.txt
    overrides: dict[str, int]       # field -> offset; "sizeof" -> size


def read_headers_txt(path: str) -> tuple[list[str], list[Listed]]:
    headers: list[str] = []
    listed: list[Listed] = []
    cur = None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].rstrip()
            if not line.strip():
                continue
            if not line[0].isspace():
                cur = line.strip()
                if cur in headers:
                    raise LayoutError(f"{path}:{n}: header {cur} listed twice")
                headers.append(cur)
                continue
            if cur is None:
                raise LayoutError(f"{path}:{n}: struct before any header line")
            toks = line.split()
            ov: dict[str, int] = {}
            for t in toks[1:]:
                k, sep, v = t.partition("=")
                if not sep:
                    raise LayoutError(f"{path}:{n}: expected <field>=0xNN or size=0xNN, got {t!r}")
                ov["sizeof" if k == "size" else k] = int(v, 0)
            if any(l.qual == toks[0] for l in listed):
                raise LayoutError(f"{path}:{n}: struct {toks[0]} listed twice")
            listed.append(Listed(toks[0], cur, n, ov))
    return headers, listed


def read_xfail(path: str) -> dict[str, int]:
    out: dict[str, int] = {}
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if line in out:
                raise LayoutError(f"{path}:{n}: {line} listed twice")
            out[line] = n
    return out


# --------------------------------------------------------------------------------------------
# Checks


@dataclass
class Check:
    struct: str
    member: str              # field name or "sizeof"
    expected: int | None     # None: bitfield, always fails
    origin: str              # header:line of the comment, or layout_headers.txt:line

    @property
    def id(self) -> str:
        return f"{self.struct}.{self.member}"

    def holds_expr(self) -> str:
        if self.expected is None:
            return "false"
        if self.member == "sizeof":
            return f"sizeof({self.struct}) == 0x{self.expected:X}"
        return f"__builtin_offsetof({self.struct}, {self.member}) == 0x{self.expected:X}"


def build_checks(listed: list[Listed], records: dict[str, Record]) -> tuple[dict[str, list[Check]], list[str]]:
    checks: dict[str, list[Check]] = {}
    errors: list[str] = []
    for l in listed:
        rec = records.get(l.qual)
        if rec is None:
            errors.append(f"layout_headers.txt:{l.line}: struct {l.qual} not found in {l.header}")
            continue
        if rec.header != l.header:
            errors.append(f"layout_headers.txt:{l.line}: {l.qual} is defined in {rec.header}, "
                          f"not {l.header}")
            continue
        if rec.template:
            errors.append(f"layout_headers.txt:{l.line}: {l.qual} is a template")
            continue
        cs: list[Check] = []
        seen = set()
        for fl in rec.fields:
            exp = fl.offset
            origin = f"{rec.header}:{fl.line}"
            if fl.name in l.overrides:
                if fl.offset is not None and l.overrides[fl.name] != fl.offset:
                    errors.append(f"layout_headers.txt:{l.line}: {l.qual}.{fl.name}={l.overrides[fl.name]:#x} "
                                  f"contradicts the comment {fl.offset:#x} at {origin}")
                exp = l.overrides[fl.name] if not fl.bitfield else None
            cs.append(Check(l.qual, fl.name, exp, origin))
            seen.add(fl.name)
        for k, v in l.overrides.items():
            if k == "sizeof" or k in seen:
                continue
            cs.append(Check(l.qual, k, v, f"layout_headers.txt:{l.line}"))
        size = rec.size
        origin = f"{rec.header} (Size comment of {l.qual})"
        if "sizeof" in l.overrides:
            if size is not None and size != l.overrides["sizeof"]:
                errors.append(f"layout_headers.txt:{l.line}: {l.qual} size={l.overrides['sizeof']:#x} "
                              f"contradicts the Size comment {size:#x}")
            size = l.overrides["sizeof"]
            origin = f"layout_headers.txt:{l.line}"
        if size is not None:
            cs.append(Check(l.qual, "sizeof", size, origin))
        if not cs:
            errors.append(f"layout_headers.txt:{l.line}: {l.qual} has no offset or size comment; "
                          f"give size= and <field>= on its line")
            continue
        checks[l.qual] = cs
    return checks, errors


def _msg(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(headers: list[str], checks: dict[str, list[Check]], xfail: dict[str, int],
             strict: bool = True, gc: bool = False) -> tuple[str, list[str]]:
    """The check unit. strict=False: every check as a plain assertion (--discover, --gc-verify);
    gc=True also leaves out the bitfield checks (bitfields are fine on the GameCube)."""
    errors: list[str] = []
    all_ids = {c.id for cs in checks.values() for c in cs}
    for x, n in xfail.items():
        if x not in checks and x not in all_ids:
            errors.append(f"layout_xfail.txt:{n}: {x} is not a check (stale entry?)")
    out = [
        "// Generated by native/tools/layout_check.py from native/check/layout_headers.txt and",
        "// native/check/layout_xfail.txt (docs/NATIVE_PORT_PHASE4_6.md, step 4.0c). Do not edit.",
        "// Compiled with the game's flags plus -fno-access-control (offsetof of private members);",
        "// __builtin_offsetof, so the unit needs no library header (it also compiles for the",
        "// GameCube configuration, --gc-verify).",
        "",
    ]
    for h in headers:
        out.append(f'#include "{h}"')
    out.append("")
    n_checks = n_x = 0
    for struct, cs in checks.items():
        out.append(f"// {struct}")
        whole = strict and struct in xfail
        if whole:
            conj = " &&\n               ".join(c.holds_expr() for c in cs)
            out.append(f"static_assert(!({conj}),")
            out.append(f"              {_msg(f'XPASS layout {struct}: every check holds now; remove it from native/check/layout_xfail.txt')});")
            n_x += len(cs)
            continue
        for c in cs:
            if gc and c.expected is None:
                continue
            if strict and c.id in xfail:
                out.append(f"static_assert(!({c.holds_expr()}), "
                           f"{_msg(f'XPASS layout {c.id}: holds now; remove it from native/check/layout_xfail.txt')});")
                n_x += 1
                continue
            if c.expected is None:
                what = (f"layout {c.id}: bitfield in a disc-mapped struct (bit order differs on the "
                        f"host; use a mask over a BE field) [{c.origin}]")
            elif c.member == "sizeof":
                what = f"layout {c.id}: size differs from 0x{c.expected:X} [{c.origin}]"
            else:
                what = f"layout {c.id}: offset differs from 0x{c.expected:X} [{c.origin}]"
            out.append(f"static_assert({c.holds_expr()}, {_msg(what)});")
            n_checks += 1
    out.append("")
    out.append(f"// {len(checks)} structs, {n_checks} checks, {n_x} xfailed")
    out.append("")
    return "\n".join(out), errors


# --------------------------------------------------------------------------------------------
# Commands


def load(defines: dict[str, int]):
    headers, listed = read_headers_txt(HEADERS_TXT)
    records = scan_headers(headers, defines)
    checks, errors = build_checks(listed, records)
    return headers, listed, records, checks, errors


def write_if_changed(path: str, text: str) -> None:
    try:
        with open(path, encoding="utf-8") as f:
            if f.read() == text:
                return
    except OSError:
        pass
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def cmd_generate(args, defines) -> int:
    headers, _, _, checks, errors = load(defines)
    xfail = read_xfail(XFAIL_TXT)
    text, gen_errors = generate(headers, checks, xfail)
    errors += gen_errors
    if errors:
        for e in errors:
            print(f"layout_check: error: {e}", file=sys.stderr)
        return 1
    write_if_changed(args.out, text)
    if args.depfile:
        deps = [os.path.abspath(__file__), HEADERS_TXT, XFAIL_TXT]
        deps += [os.path.join(INCLUDE_ROOT, h) for h in headers]
        dep = os.path.abspath(args.out).replace(" ", "\\ ") + ": " + " \\\n  ".join(
            d.replace(" ", "\\ ") for d in deps) + "\n"
        write_if_changed(args.depfile, dep)
    return 0


def cmd_list(args, defines) -> int:
    _, listed, records, checks, errors = load(defines)
    xfail = read_xfail(XFAIL_TXT)
    want = set(args.list) if args.list else None
    for l in listed:
        if want and l.qual not in want:
            continue
        rec = records.get(l.qual)
        cs = checks.get(l.qual, [])
        mark = " [xfail]" if l.qual in xfail else ""
        print(f"{l.qual}  ({l.header}:{rec.line if rec else '?'}){mark}")
        for c in cs:
            exp = "bitfield" if c.expected is None else f"0x{c.expected:X}"
            x = " [xfail]" if c.id in xfail else ""
            print(f"    {c.member:<32} {exp:<8} {c.origin}{x}")
        if rec:
            for ln, why in rec.skipped:
                print(f"    skipped {rec.header}:{ln}: {why}")
    for e in errors:
        print(f"error: {e}", file=sys.stderr)
    return 1 if errors else 0


def cmd_scan(args, defines) -> int:
    path = os.path.join(INCLUDE_ROOT, args.scan)
    for q, r in parse_header(path, args.scan, defines).items():
        size = f"size 0x{r.size:X}" if r.size is not None else "no size"
        t = " template" if r.template else ""
        print(f"{args.scan}:{r.line}: {q}: {len(r.fields)} offset comments, {size}, "
              f"{len(r.skipped)} skipped{t}")
    return 0


def _compile_command(build: str) -> tuple[list[str], str]:
    with open(os.path.join(build, "compile_commands.json"), encoding="utf-8") as f:
        entries = json.load(f)
    for e in entries:
        if os.path.basename(e["file"]) == GENERATED_NAME:
            argv = e.get("arguments") or shlex.split(e["command"])
            return argv, e["directory"]
    raise LayoutError(f"{GENERATED_NAME} is not in {build}/compile_commands.json; configure first")


def _host_argv(build: str) -> tuple[list[str], str]:
    """cos_layout_check's compile command, minus its output and source (syntax only)."""
    argv, cwd = _compile_command(build)
    cmd: list[str] = []
    skip = False
    for a in argv:
        if skip:
            skip = False
            continue
        if a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if a in ("-c", "-MD", "-MMD") or a.endswith(GENERATED_NAME):
            continue
        cmd.append(a)
    return cmd, cwd


def _gc_argv(cxx: str, version: int) -> list[str]:
    """The GameCube configuration: the decomp's own headers (no TARGET_PC, so every #if TARGET_PC
    takes its #else) and MSL, compiled by clang for 32-bit big-endian PowerPC EABI, whose struct
    layout rules (natural alignment, 8-byte u64/f64, bitfields from the most significant bit)
    are those of MWCC. Syntax only: nothing is generated."""
    centollos = os.path.join(NATIVE, "..", "game")
    msl = os.path.join(centollos, "src", "PowerPC_EABI_Support")
    incs = [os.path.join(centollos, "include"), os.path.join(centollos, "src"),
            os.path.join(msl, "MSL", "MSL_C", "MSL_Common", "Include"),
            os.path.join(msl, "MSL", "MSL_C", "PPC_EABI", "Include"),
            os.path.join(msl, "MSL", "MSL_C++", "MSL_Common", "Include"),
            os.path.join(msl, "MSL", "MSL_C", "MSL_Common_Embedded", "Math", "Include"),
            os.path.join(msl, "Runtime", "Inc")]
    # NULL as MSL defines it (dolphin/types.h would pick nullptr for C++11 and up; MWCC is C++98).
    return ([cxx, "--target=powerpc-unknown-eabi", "-std=gnu++20", "-nostdinc", "-nostdinc++",
             "-fsigned-char", "-fno-access-control", "-Wno-everything", f"-DVERSION={version}",
             "-DNULL=0"] + [f"-I{d}" for d in incs])


@dataclass
class Failure:
    check: str
    actual: int | None
    text: str


def _compile_checks(text: str, argv: list[str], cwd: str, scratch: str) -> tuple[dict[str, Failure], list[str], list[str]]:
    """Compile a check unit; (failed checks by id, errors in the unit, errors in headers)."""
    os.makedirs(scratch, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", suffix=".cpp", dir=scratch, delete=False) as tf:
        tf.write(text)
        probe = tf.name
    try:
        cmd = argv + ["-fsyntax-only", "-ferror-limit=0", "-fno-color-diagnostics", probe]
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    finally:
        os.unlink(probe)
    failed: dict[str, Failure] = {}
    unit_errors: list[str] = []
    header_errors: list[str] = []
    lines = p.stderr.splitlines()
    for i, ln in enumerate(lines):
        if "error:" not in ln:
            continue
        m = re.search(r"error: static assertion failed.*?: (?:XPASS )?layout (\S+): (.*)$", ln)
        if m:
            actual = None
            for k in range(i + 1, min(i + 8, len(lines))):
                v = re.search(r"expression evaluates to '(\d+) == (\d+)'", lines[k])
                if v:
                    actual = int(v.group(1))
                    break
                if "error:" in lines[k]:
                    break
            failed[m.group(1)] = Failure(m.group(1), actual, m.group(2))
        elif ln.startswith(probe) or "too many errors" in ln:
            unit_errors.append(ln.replace(probe, GENERATED_NAME))
        else:
            header_errors.append(ln)
    if p.returncode != 0 and not (failed or unit_errors or header_errors):
        unit_errors.append(f"compiler exited {p.returncode}: {p.stderr.strip()[:400]}")
    return failed, unit_errors, header_errors


def _print_failures(checks: dict[str, list[Check]], failed: dict[str, Failure], what: str) -> dict[str, list[tuple[Check, Failure]]]:
    by_struct: dict[str, list[tuple[Check, Failure]]] = {}
    for struct, cs in checks.items():
        for c in cs:
            if c.id in failed:
                by_struct.setdefault(struct, []).append((c, failed[c.id]))
    for struct, fs in by_struct.items():
        print(f"{struct}: {len(fs)} of {len(checks[struct])} checks fail")
        for c, f in fs:
            exp = "bitfield" if c.expected is None else f"0x{c.expected:X}"
            act = f"{what} 0x{f.actual:X}" if f.actual is not None else ""
            print(f"    {c.member:<32} decomp {exp:<8} {act}")
    return by_struct


def cmd_discover(args, defines) -> int:
    headers, _, _, checks, errors = load(defines)
    if errors:
        for e in errors:
            print(f"layout_check: error: {e}", file=sys.stderr)
        return 1
    text, _ = generate(headers, checks, {}, strict=False)
    argv, cwd = _host_argv(args.build)
    failed, unit_errors, header_errors = _compile_checks(
        text, argv, cwd, os.path.join(args.build, "layout_check"))
    if unit_errors or header_errors:
        print("\n".join(unit_errors + header_errors), file=sys.stderr)
        print("layout_check: the check unit does not compile (see above)", file=sys.stderr)
        return 1
    n_total = sum(len(cs) for cs in checks.values())
    by_struct = _print_failures(checks, failed, "host")
    print(f"layout_check: {len(checks)} structs, {n_total} checks, {len(failed)} fail "
          f"in {len(by_struct)} structs")
    if args.write_xfail:
        old_comments = []
        if os.path.exists(XFAIL_TXT):
            with open(XFAIL_TXT, encoding="utf-8") as f:
                for ln in f:
                    if ln.startswith("#") or not ln.strip():
                        old_comments.append(ln.rstrip("\n"))
                    else:
                        break
        while old_comments and not old_comments[-1].strip():
            old_comments.pop()
        out = old_comments or [
            "# Layout checks known to fail on the host (docs/NATIVE_PORT_PHASE4_6.md, step 4.0c).",
            "# <struct> xfails the whole struct, <struct>.<field> or <struct>.sizeof one check.",
            "# Strict: an entry whose checks all hold fails the build, so a format step removes the",
            "# structs it fixes. Regenerate: native/tools/layout_check.py --discover --write-xfail",
        ]
        out.append("")
        for struct, fs in by_struct.items():
            first = ", ".join(c.member for c, _ in fs[:3]) + (", ..." if len(fs) > 3 else "")
            out.append(f"{struct:<36} # {len(fs)}/{len(checks[struct])} fail: {first}")
        with open(XFAIL_TXT, "w", encoding="utf-8") as f:
            f.write("\n".join(out) + "\n")
        print(f"layout_check: wrote {os.path.relpath(XFAIL_TXT, REPO)}")
    return 0


def cmd_gc_verify(args, defines) -> int:
    """Every comment-derived check must hold in the GameCube configuration: a failure there is a
    wrong decomp comment (or a wrong size=/field= entry), not a port problem."""
    headers, _, _, checks, errors = load(defines)
    if errors:
        for e in errors:
            print(f"layout_check: error: {e}", file=sys.stderr)
        return 1
    text, _ = generate(headers, checks, {}, strict=False, gc=True)
    failed, unit_errors, header_errors = _compile_checks(
        text, _gc_argv(args.cxx, args.version), NATIVE, os.path.join(args.build, "layout_check"))
    if unit_errors:
        print("\n".join(unit_errors + header_errors), file=sys.stderr)
        print("layout_check: the GameCube check unit does not compile (see above)", file=sys.stderr)
        return 1
    n_total = sum(1 for cs in checks.values() for c in cs if c.expected is not None)
    if failed:
        _print_failures(checks, failed, "GameCube")
        print(f"layout_check: error: {len(failed)} of {n_total} checks do not hold in the GameCube "
              f"configuration: the decomp comment (or layout_headers.txt) is wrong; fix the "
              f"comment to the GameCube offset", file=sys.stderr)
        return 1
    if args.stamp:
        write_if_changed(args.stamp, f"{len(checks)} structs, {n_total} checks hold on the GameCube\n")
    print(f"layout_check: GameCube configuration: {len(checks)} structs, {n_total} checks hold"
          + (f" ({len(header_errors)} errors in header bodies ignored)" if header_errors else ""))
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", help="write the check unit here")
    ap.add_argument("--depfile", help="with --out: write a Makefile depfile")
    ap.add_argument("--list", nargs="*", metavar="STRUCT", help="print the parsed checks")
    ap.add_argument("--scan", metavar="HEADER", help="list every struct of a header")
    ap.add_argument("--discover", action="store_true", help="compile every check, print failures")
    ap.add_argument("--gc-verify", action="store_true",
                    help="check the comments against the GameCube configuration")
    ap.add_argument("--cxx", default="clang++", help="with --gc-verify: the clang to use")
    ap.add_argument("--stamp", help="with --gc-verify: touch this file on success")
    ap.add_argument("--build", default=os.path.join(REPO, "build", "native-mac"))
    ap.add_argument("--write-xfail", action="store_true")
    ap.add_argument("--version", type=int, default=BASE_DEFINES["VERSION"],
                    help="game VERSION macro (default 2, GZLE01)")
    args = ap.parse_args()
    defines = dict(BASE_DEFINES, VERSION=args.version)
    try:
        if args.out:
            return cmd_generate(args, defines)
        if args.list is not None:
            return cmd_list(args, defines)
        if args.scan:
            return cmd_scan(args, defines)
        if args.discover:
            return cmd_discover(args, defines)
        if args.gc_verify:
            return cmd_gc_verify(args, defines)
    except LayoutError as e:
        print(f"layout_check: error: {e}", file=sys.stderr)
        return 1
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
