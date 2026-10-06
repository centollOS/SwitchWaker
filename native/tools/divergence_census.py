#!/usr/bin/env python3
"""Divergence census of game/ against its import (step G1 of docs/GAME_CODE_ORGANIZATION.md).

    python3 -I native/tools/divergence_census.py [--out docs/GAME_DIVERGENCE.md] [--fetch | --offline]

Compares game/ at HEAD with the import commit (ce41ca9: snrubrm/tww b09eebc, unchanged) and writes
a Markdown report: every file that differs, its host-guarded blocks (`#if TARGET_PC` and the other
guard macros in GUARD_RE), the changed lines outside any guard (edits to decompiled code that are not
marked; the identity annotations of game/include/helpers such as BE(T) are counted apart), the
commits behind each file, and the state of each unit in zeldaret/tww main (the
upstream decompilation): does its file differ from our base, and does zeldaret's configure.py mark
it Matching for GZLE01. Files equal to zeldaret's (taken from it in step G4) are listed apart, not as
our divergence.

zeldaret/tww and snrubrm/tww are fetched into a bare cache repository, build/upstream-tww.git
(build/ is ignored by git); the main repository's config is not touched. The cache is fetched when
it does not exist yet or with --fetch; --offline skips the upstream sections when there is none.
Standard library only; runs in a few seconds once the cache exists.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

IMPORT_COMMIT = "ce41ca9"
FORK_URL = "https://github.com/snrubrm/tww.git"
FORK_COMMIT = "b09eebc39852e18397031d9a563a8eae124f21f1"
UPSTREAM_URL = "https://github.com/zeldaret/tww.git"
UPSTREAM_REF = "main"
VERSION = "GZLE01"

# A conditional chain is a host guard when one of its #if/#elif lines names one of these.
GUARD_RE = re.compile(r"\b(TARGET_PC|__MWERKS__|__clang__|TARGET_LITTLE_ENDIAN|COS_\w+|address_sanitizer)\b")
DIRECTIVE_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")
BUG_RE = re.compile(r"\bB(\d{1,3})\b")
HUNK_RE = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")
COMMENT_RE = re.compile(r"(//.*|/\*.*)")


def git(args, cwd=None, git_dir=None, check=True):
    cmd = ["git"]
    if git_dir:
        cmd += ["--git-dir", git_dir]
    cmd += args
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and r.returncode != 0:
        sys.exit(f"git {' '.join(args)} failed:\n{r.stderr.decode(errors='replace')}")
    return r.stdout.decode("utf-8", errors="replace")


def read_blobs(specs, cwd=None, git_dir=None):
    """Contents of many '<rev>:<path>' objects through one git cat-file --batch."""
    cmd = ["git"] + (["--git-dir", git_dir] if git_dir else []) + ["cat-file", "--batch"]
    data = subprocess.run(cmd, cwd=cwd, input="".join(s + "\n" for s in specs).encode(),
                          stdout=subprocess.PIPE, check=True).stdout
    out, pos = {}, 0
    for spec in specs:
        nl = data.index(b"\n", pos)
        header = data[pos:nl].decode()
        pos = nl + 1
        if header.endswith(" missing"):
            out[spec] = None
            continue
        size = int(header.split()[2])
        out[spec] = data[pos:pos + size].decode("utf-8", errors="replace")
        pos += size + 1
    return out


def norm(s):
    return re.sub(r"\s+", "", s)


# Self-marking host annotations (game/include/helpers: identity on the GameCube): BE(T), LE(T),
# BE_HOST(x), RES_*(x), OFFSET_PTR(T) = T*, OFFSET_PTR_RAW = u32.
ANNOT_RE = re.compile(r"\b(BE|LE|BE_HOST|RES_[US](?:16|32))\(")
ANNOT_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]helpers/')


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s)
    return s.split("//", 1)[0]


def deannotate(s):
    """A line with the host annotations replaced by what they are on the GameCube, normalised."""
    s = strip_comments(s)
    s = re.sub(r"\bOFFSET_PTR_RAW\b", "u32", s)
    s = re.sub(r"\bOFFSET_PTR\(([^()]*)\)", r"\1*", s)
    s = ANNOT_RE.sub("(", s)
    return re.sub(r"[\s()]+", "", s)


# GameCube values of the guard macros, for the GameCube view of a file (gc_view).
GC_GUARD_VALUES = {"TARGET_PC": 0, "__MWERKS__": 1, "__clang__": 0, "TARGET_LITTLE_ENDIAN": 0,
                   "address_sanitizer": 0}


def eval_guard(kind, rest):
    """1/0 for a guard condition on the GameCube, None when it names anything else."""
    rest = strip_comments(rest).strip()
    if kind in ("ifdef", "ifndef"):
        name = rest.split()[0] if rest else ""
        v = GC_GUARD_VALUES.get(name, 0 if name.startswith("COS_") else None)
        return None if v is None else (v if kind == "ifdef" else 1 - v)
    e = re.sub(r"\bdefined\s*\(\s*(\w+)\s*\)|\bdefined\s+(\w+)", lambda m: "__def_" + (m.group(1) or m.group(2)), rest)
    def name(m):
        n = m.group(0)
        if n.startswith("__def_"):
            n = n[len("__def_"):]
            if n in GC_GUARD_VALUES:
                return "1" if n == "__MWERKS__" else "0"
            return "0" if n.startswith("COS_") else n
        if n in GC_GUARD_VALUES:
            return str(GC_GUARD_VALUES[n])
        return "0" if n.startswith("COS_") else n
    e = re.sub(r"\b[A-Za-z_]\w*\b", name, e)
    if re.search(r"[A-Za-z_]", e):
        return None
    e = e.replace("&&", " and ").replace("||", " or ").replace("!", " not ").replace(" not =", "!=")
    try:
        return 1 if eval(e, {"__builtins__": {}}) else 0
    except Exception:
        return None


def gc_view(text):
    """The file as the GameCube build sees it through the guard chains (non-guard chains kept as
    they are), or None when a guard condition cannot be decided."""
    out, stack = [], []  # frames: [is_guard, taking, any_taken]
    for line in text.split("\n"):
        m = DIRECTIVE_RE.match(line)
        on = all(fr[1] for fr in stack if fr[0])
        if m is None:
            if on:
                out.append(line)
            continue
        kind, rest = m.group(1), m.group(2)
        if kind in ("if", "ifdef", "ifndef"):
            if GUARD_RE.search(rest):
                v = eval_guard(kind, rest)
                if v is None:
                    return None
                stack.append([True, bool(v), bool(v)])
            else:
                stack.append([False, True, True])
                if on:
                    out.append(line)
        elif kind in ("elif", "else"):
            if not stack:
                return None
            fr = stack[-1]
            if fr[0]:
                v = 1 if kind == "else" else eval_guard("if", rest)
                if v is None:
                    return None
                fr[1] = bool(v) and not fr[2]
                fr[2] = fr[2] or fr[1]
            elif GUARD_RE.search(rest):
                return None
            elif all(f[1] for f in stack[:-1] if f[0]):
                out.append(line)
        else:
            if not stack:
                return None
            fr = stack.pop()
            if not fr[0] and all(f[1] for f in stack if f[0]):
                out.append(line)
    return "\n".join(out)


def same_code(a, b):
    """a and b are the same code once comments, whitespace, helper includes and the host
    annotations (deannotate) are left out."""
    def lines(t):
        t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
        return [x for x in (deannotate(l) for l in t.split("\n") if not ANNOT_INCLUDE_RE.match(l)) if x]
    return lines(a) == lines(b)


def is_comment_only(s):
    t = s.strip()
    return t.startswith("//") or t.startswith("/*") or t.startswith("*") or t.endswith("*/")


# --- game/ against the import -------------------------------------------------------------------

def parse_diff(repo):
    """{path: {'added': {lineno: text}, 'deleted': [text]}} from git diff -U0."""
    files = {}
    cur = None
    new_line = 0
    for line in git(["diff", "-U0", "--no-color", "--no-renames", IMPORT_COMMIT, "HEAD", "--", "game/"],
                    cwd=repo).splitlines():
        if line.startswith("diff --git "):
            cur = None
        elif line.startswith("+++ "):
            path = line[4:]
            if path != "/dev/null":
                cur = files.setdefault(path[2:], {"added": {}, "deleted": []})
        elif line.startswith("--- "):
            path = line[4:]
            if path != "/dev/null":
                cur = files.setdefault(path[2:], {"added": {}, "deleted": []})
        elif line.startswith("@@"):
            m = HUNK_RE.match(line)
            new_line = int(m.group(3))
        elif cur is None:
            continue
        elif line.startswith("+"):
            cur["added"][new_line] = line[1:]
            new_line += 1
        elif line.startswith("-"):
            cur["deleted"].append(line[1:])
    return files


def conditional_chains(text):
    """[(start, end, is_guard, depth)] for every #if..#endif chain, 1-based inclusive lines."""
    chains, stack = [], []
    lines = text.split("\n")
    in_comment = False
    for i, line in enumerate(lines, 1):
        stripped = line
        if in_comment:
            if "*/" not in line:
                continue
            stripped = line.split("*/", 1)[1]
            in_comment = False
        m = DIRECTIVE_RE.match(stripped)
        if m is None:
            if "/*" in stripped and "*/" not in stripped.split("/*", 1)[1] and "//" not in stripped.split("/*", 1)[0]:
                in_comment = True
            continue
        kind, rest = m.group(1), m.group(2)
        if kind in ("if", "ifdef", "ifndef"):
            stack.append([i, bool(GUARD_RE.search(rest)), len(stack)])
        elif kind == "elif" and stack:
            stack[-1][1] = stack[-1][1] or bool(GUARD_RE.search(rest))
        elif kind == "endif" and stack:
            start, guard, depth = stack.pop()
            chains.append((start, i, guard, depth))
    for start, guard, depth in stack:  # unbalanced: runs to the end of the file
        chains.append((start, len(lines), guard, depth))
    return sorted(chains)


def first_comment(lines, start, end):
    for n in range(start, end + 1):
        m = COMMENT_RE.search(lines[n - 1])
        if m:
            c = m.group(1).lstrip("/*! ").rstrip("*/ ").strip()
            if c:
                return c[:110]
    return ""


def analyse_file(path, head_text, diff):
    lines = head_text.split("\n")
    chains = conditional_chains(head_text)
    guarded = set()
    for start, end, guard, _ in chains:
        if guard:
            guarded.update(range(start, end + 1))
    added = diff["added"]
    ours = [c for c in chains if c[2] and c[0] in added]
    outer = [c for c in ours if not any(o[0] < c[0] and c[1] <= o[1] for o in ours)]
    blocks = [{"path": path, "start": s, "len": e - s + 1, "comment": first_comment(lines, s, e)}
              for s, e, _, _ in outer]
    guarded_added_norm = collections.Counter(norm(t) for n, t in added.items() if n in guarded)
    unguarded_added = sorted(n for n, t in added.items() if n not in guarded and t.strip())
    deleted = []
    for t in diff["deleted"]:
        k = norm(t)
        if not k:
            continue
        if guarded_added_norm[k] > 0:  # the original line kept in a guarded branch
            guarded_added_norm[k] -= 1
            continue
        deleted.append(t)
    # Split the unguarded lines: annotations (an added line that is a removed line with BE()/OFFSET_PTR()
    # etc. or a comment added, helper includes, comment-only lines) against code changes.
    del_keys = collections.Counter(deannotate(t) for t in deleted)
    annot_added, unguarded_code = [], []
    for n in unguarded_added:
        t = added[n]
        k = deannotate(t)
        if del_keys[k] > 0 and k:
            del_keys[k] -= 1
            annot_added.append(n)
        elif ANNOT_INCLUDE_RE.match(t) or is_comment_only(t) or not k:
            annot_added.append(n)
        else:
            unguarded_code.append(n)
    # A removed line is an annotation too when an annotated added line took its place (counted pairs),
    # or when it was a comment.
    paired = collections.Counter(deannotate(t) for t in deleted)
    paired.subtract(del_keys)
    unguarded_deleted, annot_deleted = [], 0
    for t in deleted:
        k = deannotate(t)
        if paired[k] > 0:
            paired[k] -= 1
            annot_deleted += 1
        elif is_comment_only(t) or not k:
            annot_deleted += 1
        else:
            unguarded_deleted.append(t)
    unguarded_added = unguarded_code
    return {
        "blocks_all": len(ours),
        "blocks": blocks,
        "guarded_lines": sum(b["len"] for b in blocks),
        "unguarded_added": unguarded_added,
        "unguarded_deleted": unguarded_deleted,
        "annot_added": len(annot_added),
        "annot_deleted": annot_deleted,
        "lines": lines,
    }


def ranges(nums):
    out = []
    for n in nums:
        if out and n == out[-1][1] + 1:
            out[-1][1] = n
        else:
            out.append([n, n])
    return out


def commits_per_file(repo):
    log = git(["log", "--no-merges", "--format=%x00%h%x01%s%x01%b%x02", "--name-only",
               f"{IMPORT_COMMIT}..HEAD", "--", "game/"], cwd=repo)
    per_file = collections.defaultdict(list)
    for rec in log.split("\x00")[1:]:
        head, _, names = rec.partition("\x02")
        sha, subject, body = head.split("\x01", 2)
        bugs = sorted({int(b) for b in BUG_RE.findall(subject + "\n" + body)})
        for name in names.split():
            if name.startswith("game/"):
                per_file[name].append((sha, subject, bugs))
    return per_file


# --- upstream ------------------------------------------------------------------------------------

def ensure_cache(cache, fetch, offline):
    if not os.path.isdir(cache):
        if offline:
            return False
        os.makedirs(os.path.dirname(cache), exist_ok=True)
        git(["init", "-q", "--bare", cache])
        git(["remote", "add", "zeldaret", UPSTREAM_URL], git_dir=cache)
        git(["remote", "add", "snrubrm", FORK_URL], git_dir=cache)
        fetch = True
    if fetch:
        print(f"fetching {UPSTREAM_URL} {UPSTREAM_REF} and {FORK_URL} {FORK_COMMIT[:7]} into {cache}", file=sys.stderr)
        git(["fetch", "-q", "zeldaret", UPSTREAM_REF], git_dir=cache)
        if git(["cat-file", "-t", FORK_COMMIT], git_dir=cache, check=False).strip() != "commit":
            git(["fetch", "-q", "snrubrm", FORK_COMMIT], git_dir=cache)
    return True


def tree_blobs(cache, rev):
    out = {}
    for line in git(["ls-tree", "-r", rev, "--", "src", "include"], git_dir=cache).splitlines():
        meta, path = line.split("\t", 1)
        out[path] = meta.split()[2]
    return out


ENTRY_RE = re.compile(r"\b(Object|ActorRel)\(")
STRING_RE = re.compile(r'\s*"([^"]+)"')


def configure_entries(text):
    """(helper, status expression, path) for each Object(status, "path") / ActorRel(status, "name")."""
    for m in ENTRY_RE.finditer(text):
        i, depth, quote = m.end(), 0, False
        while i < len(text):  # the first argument ends at a top-level comma
            c = text[i]
            if c == '"':
                quote = not quote
            elif not quote and c == "(":
                depth += 1
            elif not quote and c == ")":
                if depth == 0:
                    break
                depth -= 1
            elif not quote and c == "," and depth == 0:
                break
            i += 1
        if i >= len(text) or text[i] != ",":
            continue
        s = STRING_RE.match(text, i + 1)
        if s:
            yield m.group(1), text[m.end():i].strip(), s.group(1)


def status_for(expr):
    expr = expr.strip()
    if " or " in expr:  # e.g. MatchingFor(...) or Equivalent: the best of the alternatives
        rank = ["Matching", "Equivalent", "DebugOnly", "NonMatching"]
        alts = [status_for(e) for e in expr.split(" or ")]
        return min(alts, key=lambda a: rank.index(a) if a in rank else len(rank))
    m = re.match(r"(MatchingFor|EquivalentFor)\((.*)\)$", expr, re.S)
    if m:
        hit = VERSION in re.findall(r"\"(\w+)\"", m.group(2))
        if not hit:
            return "NonMatching"
        return "Matching" if m.group(1) == "MatchingFor" else "Equivalent"
    return {"Matching": "Matching", "NonMatching": "NonMatching", "Equivalent": "Equivalent",
            "DEBUG_ONLY": "DebugOnly"}.get(expr, expr)


def unit_status(configure_py):
    """{'src/<path>': status} for GZLE01 from a configure.py."""
    out = {}
    for helper, expr, path in configure_entries(configure_py):
        if helper == "Object":
            out["src/" + path] = status_for(expr)
        else:
            out[f"src/d/actor/{path}.cpp"] = status_for(expr)
    return out


def numstat(cache, a, b):
    out = {}
    for line in git(["diff", "--numstat", "--no-renames", a, b, "--", "src", "include"], git_dir=cache).splitlines():
        ins, dels, path = line.split("\t", 2)
        out[path] = (int(ins) if ins != "-" else 0) + (int(dels) if dels != "-" else 0)
    return out


def upstream_state(cache, repo):
    up = git(["rev-parse", f"zeldaret/{UPSTREAM_REF}"], git_dir=cache).strip()
    mb = git(["merge-base", FORK_COMMIT, up], git_dir=cache).strip()
    info = {
        "up": up,
        "up_desc": git(["log", "-1", "--format=%h %cs %s", up], git_dir=cache).strip(),
        "mb_desc": git(["log", "-1", "--format=%h %cs %s", mb], git_dir=cache).strip(),
        "ahead": int(git(["rev-list", "--count", f"{mb}..{up}"], git_dir=cache)),
        "fork_ahead": int(git(["rev-list", "--count", f"{mb}..{FORK_COMMIT}"], git_dir=cache)),
    }
    # The import is the fork's commit unchanged: compare the trees (cheap, exact).
    same = all(git(["rev-parse", f"{FORK_COMMIT}:{p}"], git_dir=cache).strip()
               == git(["rev-parse", f"{IMPORT_COMMIT}:game/{p}"], cwd=repo).strip()
               for p in ("src", "include", "LICENSE"))
    info["import_verified"] = same
    base, upb, mbb = tree_blobs(cache, FORK_COMMIT), tree_blobs(cache, up), tree_blobs(cache, mb)
    info["delta"] = numstat(cache, FORK_COMMIT, up)
    cfg = read_blobs([f"{r}:configure.py" for r in (up, FORK_COMMIT, mb)], git_dir=cache)
    info["status"] = unit_status(cfg[f"{up}:configure.py"] or "")
    info["fork_status"] = unit_status(cfg[f"{FORK_COMMIT}:configure.py"] or "")
    info["mb_status"] = unit_status(cfg[f"{mb}:configure.py"] or "")
    files = {}
    for p in sorted(set(base) | set(upb)):
        b, u, m = base.get(p), upb.get(p), mbb.get(p)
        if b == u:
            origin = "same"
        elif b is None:
            origin = "upstream-new"
        elif u is None:
            origin = "upstream-removed"
        elif b == m:
            origin = "upstream-moved"
        elif u == m:
            origin = "fork-only"
        else:
            origin = "both"
        files[p] = origin
    info["files"] = files
    info["blobs"] = upb
    return info


# --- report --------------------------------------------------------------------------------------

def md_escape(s):
    return s.replace("|", "\\|").replace("`", "'")


def commit_cell(commits):
    if not commits:
        return ""
    bugs = sorted({b for _, _, bs in commits for b in bs})
    top = "; ".join(f"`{sha}` {md_escape(subj[:60])}" for sha, subj, _ in commits[:3])
    more = f" (+{len(commits) - 3})" if len(commits) > 3 else ""
    bug_s = (" — bugs " + ", ".join(f"B{b}" for b in bugs)) if bugs else ""
    return f"{len(commits)}: {top}{more}{bug_s}"


def main():
    repo = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(repo, "docs", "GAME_DIVERGENCE.md"))
    ap.add_argument("--cache", default=os.path.join(repo, "build", "upstream-tww.git"))
    ap.add_argument("--fetch", action="store_true", help="update the upstream cache first")
    ap.add_argument("--offline", action="store_true", help="never fetch; skip upstream without a cache")
    args = ap.parse_args()

    # The tree id of game/ at HEAD (not a commit id: a commit that changes game/ can carry its own
    # report, and commits elsewhere leave the report as it is).
    head = git(["rev-parse", "--short", "HEAD:game"], cwd=repo).strip()
    status = {}
    for line in git(["diff", "--name-status", "--no-renames", IMPORT_COMMIT, "HEAD", "--", "game/"], cwd=repo).splitlines():
        st, path = line.split("\t", 1)
        status[path] = st
    stat = {}
    for line in git(["diff", "--numstat", "--no-renames", IMPORT_COMMIT, "HEAD", "--", "game/"], cwd=repo).splitlines():
        ins, dels, path = line.split("\t", 2)
        stat[path] = (int(ins), int(dels))
    diffs = parse_diff(repo)

    up = None
    if ensure_cache(args.cache, args.fetch, args.offline):
        up = upstream_state(args.cache, repo)
    # Files whose HEAD content is zeldaret's (step G4: units taken from zeldaret, headers merged to
    # zeldaret's): reported apart, not as our divergence. Other files keep zeldaret's lines as
    # lines taken from upstream (partially merged headers), not as unguarded divergence.
    head_blobs = {}
    for line in git(["ls-tree", "-r", "HEAD", "--", "game/src", "game/include"], cwd=repo).splitlines():
        meta, path = line.split("\t", 1)
        head_blobs[path] = meta.split()[2]
    converged = set()
    if up:
        converged = {p for p in status if status[p] == "M" and up["blobs"].get(p[len("game/"):]) == head_blobs.get(p)}
    texts = read_blobs([f"HEAD:{p}" for p in sorted(status) if status[p] != "D"], cwd=repo)
    # Files that are zeldaret's code with our guarded hunks on top (step G4b): their GameCube view
    # (gc_view) is zeldaret's file once comments, whitespace and annotations are left out.
    hosted = set()
    if up:
        cand = [p for p in status if status[p] == "M" and p not in converged and p[len("game/"):] in up["blobs"]
                and up["files"].get(p[len("game/"):], "same") != "same"]
        got = read_blobs([f"{up['up']}:{p[len('game/'):]}" for p in cand], git_dir=args.cache)
        for p in cand:
            gv = gc_view(texts[f"HEAD:{p}"])
            uv = gc_view(got[f"{up['up']}:{p[len('game/'):]}"] or "")
            if gv is not None and uv is not None and same_code(gv, uv):
                hosted.add(p)
    commits = commits_per_file(repo)

    up_texts, base_texts = {}, {}
    if up:
        want = [p for p in status if status[p] == "M" and p not in converged and p[len("game/"):] in up["blobs"]]
        got = read_blobs([f"{up['up']}:{p[len('game/'):]}" for p in want], git_dir=args.cache)
        up_texts = {p: got[f"{up['up']}:{p[len('game/'):]}"] for p in want}
        base = read_blobs([f"{IMPORT_COMMIT}:{p}" for p in want], cwd=repo)
        base_texts = {p: base[f"{IMPORT_COMMIT}:{p}"] or "" for p in want}
    rows, new_files, conv_rows = [], [], []
    for p in sorted(status):
        if p in converged:
            conv_rows.append({"path": p, "ins": stat[p][0], "dels": stat[p][1]})
            continue
        if status[p] == "A":
            new_files.append(p)
            continue
        if status[p] == "D":
            rows.append({"path": p, "ins": 0, "dels": stat[p][1], "blocks_all": 0, "blocks": [],
                         "guarded_lines": 0, "unguarded_added": [], "unguarded_deleted": [],
                         "annot_added": 0, "annot_deleted": 0, "lines": []})
            continue
        r = analyse_file(p, texts[f"HEAD:{p}"], diffs.get(p, {"added": {}, "deleted": []}))
        r["up_added"] = r["up_deleted"] = 0
        if up_texts.get(p) is not None:
            up_norm = {norm(x) for x in up_texts[p].split("\n")}
            new_up = up_norm - {norm(x) for x in base_texts[p].split("\n")}  # lines zeldaret added
            keep = [n for n in r["unguarded_added"] if norm(r["lines"][n - 1]) not in new_up]
            r["up_added"] = len(r["unguarded_added"]) - len(keep)
            r["unguarded_added"] = keep
            keep = [x for x in r["unguarded_deleted"] if norm(x) in up_norm]
            r["up_deleted"] = len(r["unguarded_deleted"]) - len(keep)
            r["unguarded_deleted"] = keep
        r.update(path=p, ins=stat[p][0], dels=stat[p][1])
        rows.append(r)
    rows.sort(key=lambda r: (-(r["ins"] + r["dels"]), r["path"]))

    def up_cells(path):
        if up is None or not path.startswith("game/"):
            return "", ""
        rel = path[len("game/"):]
        origin = up["files"].get(rel, "-")
        st = up["status"].get(rel, "")
        delta = up["delta"].get(rel, 0)
        return (f"{origin} ({delta})" if origin != "same" else "same"), st

    tot_ins = sum(r["ins"] for r in rows) + sum(stat[p][0] for p in new_files)
    tot_dels = sum(r["dels"] for r in rows)
    tot_blocks = sum(r["blocks_all"] for r in rows)
    tot_outer = sum(len(r["blocks"]) for r in rows)
    tot_guarded = sum(r["guarded_lines"] for r in rows)
    ung = [r for r in rows if r["unguarded_added"] or r["unguarded_deleted"]]
    tot_ung_add = sum(len(r["unguarded_added"]) for r in rows)
    tot_ung_del = sum(len(r["unguarded_deleted"]) for r in rows)
    tot_ann_add = sum(r["annot_added"] for r in rows)
    tot_ann_del = sum(r["annot_deleted"] for r in rows)
    ann_files = sum(1 for r in rows if r["annot_added"] or r["annot_deleted"])
    tot_up_add = sum(r["up_added"] for r in rows)
    tot_up_del = sum(r["up_deleted"] for r in rows)
    up_files = sum(1 for r in rows if r["up_added"] or r["up_deleted"])

    o = []
    w = o.append
    w("# game/ divergence census")
    w("")
    w("Generated by `native/tools/divergence_census.py` — do not edit by hand. Regenerate with")
    w("`python3 -I native/tools/divergence_census.py` (`--fetch` to update the zeldaret/tww cache in")
    w("`build/upstream-tww.git`). Step G1 of [GAME_CODE_ORGANIZATION.md](GAME_CODE_ORGANIZATION.md).")
    w("")
    w(f"Base: the import `{IMPORT_COMMIT}` (snrubrm/tww `{FORK_COMMIT[:7]}`); compared with HEAD, whose game/ is the tree `{head}`.")
    if up:
        w(f"Upstream: zeldaret/tww {UPSTREAM_REF} `{up['up_desc']}`. The fork branched from zeldaret at "
          f"`{up['mb_desc']}`; since then zeldaret has {up['ahead']} commits and the fork {up['fork_ahead']}.")
        w(f"Import equal to snrubrm `{FORK_COMMIT[:7]}` (src/, include/, LICENSE trees): "
          f"**{'yes, verified' if up['import_verified'] else 'NO — the trees differ'}**.")
    else:
        w("Upstream: not available (no cache, --offline).")
    w("")
    w("## Summary")
    w("")
    w("| | |")
    w("| --- | --- |")
    w(f"| Files touched since the import | {len(rows) + len(new_files)} ({len(rows)} changed, {len(new_files)} added), "
      f"besides {len(conv_rows)} files now equal to zeldaret/tww |")
    w(f"| Lines | +{tot_ins} / -{tot_dels} |")
    w(f"| Guarded blocks added (`#if` chains naming a guard macro) | {tot_blocks} ({tot_outer} outermost) |")
    w(f"| Lines inside the outermost added guarded blocks | {tot_guarded} |")
    w(f"| **Unguarded code divergence** (code changed outside any guard) | **{tot_ung_add} added, {tot_ung_del} removed, in {len(ung)} files** |")
    w(f"| Unguarded annotations and cosmetic changes | {tot_ann_add} added, {tot_ann_del} removed, in {ann_files} files |")
    w(f"| Unguarded lines that are zeldaret/tww's (headers partially merged with it) | {tot_up_add} added, {tot_up_del} removed, in {up_files} files |")
    w("")
    w("Guard macros: `TARGET_PC`, `__MWERKS__`, `__clang__`, `TARGET_LITTLE_ENDIAN`, `COS_*`, "
      "`address_sanitizer` (in any `#if`/`#elif` of the chain). A line is guarded when it lies inside "
      "such a chain (the original code in the `#else` branch included). Unguarded added lines: added "
      "lines outside every guard chain. Unguarded removed lines: removed lines whose text (whitespace "
      "ignored) is not kept in a guarded branch. Blank lines are not counted. Unguarded lines are split "
      "into annotations and cosmetic changes (the same code on the GameCube once the identity macros of "
      "`game/include/helpers` — `BE()`, `LE()`, `BE_HOST()`, `RES_*()`, `OFFSET_PTR()`, `OFFSET_PTR_RAW` — "
      "are expanded and comments and whitespace are ignored; `#include \"helpers/...\"`; comment-only "
      "lines) and code divergence (everything else). Files equal to zeldaret/tww (step G4) are listed "
      "apart and left out of everything else; in the other files an unguarded line that zeldaret added "
      "as well (or a removed line that zeldaret removed as well) is counted as zeldaret's, not as our divergence.")
    w("")
    if new_files:
        w("Files added to game/ (ours, not decompiled code): " + ", ".join(f"`{p}` (+{stat[p][0]})" for p in new_files) + ".")
        w("")

    if conv_rows:
        w(f"## Converged on zeldaret/tww ({len(conv_rows)})")
        w("")
        w("Files whose content is zeldaret/tww's at the upstream commit below (step G4: units it marks "
          "Matching, taken verbatim, and the headers they needed). They differ from the import but are not "
          "our edits; re-taking them from a newer zeldaret is a plain copy.")
        w("")
        w(", ".join(f"`{r['path'][5:]}` (+{r['ins']}/-{r['dels']})" for r in conv_rows) + ".")
        w("")

    w("## Unguarded divergence")
    w("")
    w("Code changes to decompiled code that no guard marks (annotations and cosmetic changes left out), "
      "largest first. Line ranges in HEAD with the first line of each range; removed lines quoted as "
      "they were in the import. A pair of removed and added lines is a replaced line.")
    w("")
    for r in sorted(ung, key=lambda r: (-(len(r["unguarded_added"]) + len(r["unguarded_deleted"])), r["path"])):
        w(f"### `{r['path']}` — {len(r['unguarded_added'])} added, {len(r['unguarded_deleted'])} removed")
        w("")
        rg = ranges(r["unguarded_added"])
        for a, b in rg[:12]:
            span = f"{a}" if a == b else f"{a}-{b}"
            w(f"- L{span}: `{md_escape(r['lines'][a - 1].strip()[:100])}`")
        if len(rg) > 12:
            w(f"- ... {len(rg) - 12} more ranges")
        for t in r["unguarded_deleted"][:8]:
            w(f"- removed: `{md_escape(t.strip()[:100])}`")
        if len(r["unguarded_deleted"]) > 8:
            w(f"- ... {len(r['unguarded_deleted']) - 8} more removed lines")
        cm = commits.get(r["path"], [])
        if cm:
            w(f"- commits: {commit_cell(cm)}")
        w("")

    w("## Files")
    w("")
    w("Sorted by lines changed. Blocks: added guarded chains (outermost). Largest: lines and start line "
      "of the largest one. Unguarded: added/removed code lines outside guards (annotation and cosmetic lines in parentheses). Upstream: how zeldaret's file "
      "differs from our base (`same`; `upstream-moved`: only zeldaret changed it since the fork point; "
      "`fork-only`: only the fork changed it; `both`) with the lines that differ; status: zeldaret's "
      f"configure.py for {VERSION}.")
    w("")
    w("| File | +/- | Blocks | Largest | Unguarded | Upstream | Status | Commits |")
    w("| --- | --- | --- | --- | --- | --- | --- | --- |")
    for r in rows:
        big = max(r["blocks"], key=lambda b: (b["len"], -b["start"]), default=None)
        big_s = f"{big['len']} @{big['start']}" if big else ""
        ung_s = f"{len(r['unguarded_added'])}/{len(r['unguarded_deleted'])}" if (r["unguarded_added"] or r["unguarded_deleted"]) else ""
        if r["annot_added"] or r["annot_deleted"]:
            ung_s += f" (annot. {r['annot_added']}/{r['annot_deleted']})"
        o_s, st = up_cells(r["path"])
        w(f"| `{r['path'][5:]}` | +{r['ins']}/-{r['dels']} | {r['blocks_all']} ({len(r['blocks'])}) | {big_s} | "
          f"{ung_s} | {o_s} | {st} | {commit_cell(commits.get(r['path'], []))} |")
    w("")

    if up:
        touched = {r["path"][len("game/"):] for r in rows}
        conv = {r["path"][len("game/"):] for r in conv_rows}
        units = sorted(up["status"])
        cnt = collections.Counter(up["status"].values())
        w("## Upstream (zeldaret/tww)")
        w("")
        w(f"Units in zeldaret's configure.py for {VERSION}: " +
          ", ".join(f"{k} {v}" for k, v in sorted(cnt.items())) + ".")
        oc = collections.Counter(up["files"][p] for p in up["files"] if p.startswith("src/"))
        w("Source files (src/) by origin of the difference: " + ", ".join(f"{k} {v}" for k, v in sorted(oc.items())) + ".")
        oc = collections.Counter(up["files"][p] for p in up["files"] if p.startswith("include/"))
        w("Headers (include/): " + ", ".join(f"{k} {v}" for k, v in sorted(oc.items())) + ".")
        w("")
        host_units = sorted(p[len("game/"):] for p in hosted)
        if host_units:
            w(f"### Converged with our host hunks ({len(host_units)})")
            w("")
            w("Files that are zeldaret's code under our guarded hunks (step G4b): with the guards resolved "
              "for the GameCube (TARGET_PC 0, __MWERKS__ 1, COS_* 0) and the host annotations "
              "(`BE()`...), comments and whitespace left out, the file is zeldaret's. Re-taking them from a "
              "newer zeldaret is a 3-way merge of our hunks. Status: zeldaret's configure.py.")
            w("")
            w(", ".join(f"`{u}`" + (f" ({up['status'][u]})" if u in up["status"] and up["status"][u] != "Matching" else "")
                        for u in host_units) + ".")
            w("")
        cands = [u for u in units if up["status"][u] == "Matching" and up["files"].get(u, "same") != "same"
                 and u not in conv and "game/" + u not in hosted]
        w(f"### Convergence candidates ({len(cands)})")
        w("")
        w(f"Units zeldaret marks Matching for {VERSION} whose file differs from our base and that are not "
          "converged yet. At fork point: the "
          "unit's status in zeldaret when the fork branched (NonMatching there: zeldaret matched it since). "
          "Fork status: the unit in snrubrm's configure.py. Touched: we changed the file since the import (our hunks would "
          "have to be re-applied).")
        w("")
        newly = sum(1 for u in cands if up["mb_status"].get(u) != "Matching")
        w(f"{newly} of them were not Matching at the fork point. By origin: " + ", ".join(
            f"{k} {v}" for k, v in sorted(collections.Counter(up["files"][u] for u in cands).items())) +
          f"; {sum(1 for u in cands if u in touched)} touched by us.")
        w("")
        w("| Unit | Origin | Lines differing | At fork point | Fork status | Touched (+/-) |")
        w("| --- | --- | --- | --- | --- | --- |")
        for u in sorted(cands, key=lambda u: (u not in touched, -up["delta"].get(u, 0), u)):
            t = "game/" + u
            ts = f"yes (+{stat[t][0]}/-{stat[t][1]})" if u in touched else ""
            w(f"| `{u}` | {up['files'][u]} | {up['delta'].get(u, 0)} | {up['mb_status'].get(u, '')} | "
              f"{up['fork_status'].get(u, '')} | {ts} |")
        w("")
        others = [u for u in units if up["status"][u] != "Matching" and up["files"].get(u, "same") != "same"
                  and u not in conv]
        w(f"### Units that differ upstream but are not Matching there ({len(others)})")
        w("")
        w("| Unit | Status | Origin | Lines differing | Touched |")
        w("| --- | --- | --- | --- | --- |")
        for u in sorted(others, key=lambda u: (-up["delta"].get(u, 0), u)):
            w(f"| `{u}` | {up['status'][u]} | {up['files'][u]} | {up['delta'].get(u, 0)} | {'yes' if u in touched else ''} |")
        w("")
        hdrs = sorted(p for p, oo in up["files"].items() if p.startswith("include/") and oo != "same" and p not in conv)
        w(f"### Headers that differ from our base ({len(hdrs)})")
        w("")
        w("A unit taken from zeldaret may need its headers too; a header change reaches every unit "
          "including it. Touched: we changed the header since the import.")
        w("")
        w("| Header | Origin | Lines differing | Touched |")
        w("| --- | --- | --- | --- |")
        for h in sorted(hdrs, key=lambda h: (-up["delta"].get(h, 0), h)):
            w(f"| `{h}` | {up['files'][h]} | {up['delta'].get(h, 0)} | {'yes' if h in touched else ''} |")
        w("")
        rest = sorted(p for p, oo in up["files"].items() if p.startswith("src/") and oo != "same" and p not in up["status"]
                      and p not in conv)
        if rest:
            w(f"Other src/ files that differ and are not a configure.py unit ({len(rest)}): " +
              ", ".join(f"`{p}` ({up['files'][p]}, {up['delta'].get(p, 0)})" for p in rest) + ".")
            w("")

    blocks = sorted((b for r in rows for b in r["blocks"]), key=lambda b: (-b["len"], b["path"], b["start"]))
    w("## Largest guarded blocks (top 50)")
    w("")
    w("Input for step G3 (hoisting into `native/src/pc/game_hooks/`). Length includes the directives "
      "and the original code in the `#else` branch.")
    w("")
    w("| # | Where | Lines | First comment inside |")
    w("| --- | --- | --- | --- |")
    for i, b in enumerate(blocks[:50], 1):
        w(f"| {i} | `{b['path'][5:]}:{b['start']}` | {b['len']} | {md_escape(b['comment'])} |")
    w("")

    with open(args.out, "w") as f:
        f.write("\n".join(o))
    print(f"wrote {os.path.relpath(args.out, repo)}: {len(rows) + len(new_files)} files, +{tot_ins}/-{tot_dels}, "
          f"{tot_blocks} guarded blocks ({tot_guarded} lines), unguarded code {tot_ung_add} added / {tot_ung_del} removed "
          f"in {len(ung)} files, annotations {tot_ann_add}/{tot_ann_del}, {len(conv_rows)} files converged on zeldaret" +
          (f", {len(hosted)} converged with host hunks, {len([u for u in up['status'] if up['status'][u] == 'Matching' and up['files'].get(u, 'same') != 'same' and 'game/' + u not in converged and 'game/' + u not in hosted])} convergence candidates" if up else ""))


if __name__ == "__main__":
    main()
