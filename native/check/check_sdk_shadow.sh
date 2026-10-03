#!/bin/sh
# SDK shadow check (phase 2, step 2.3; docs/NATIVE_PORT_PHASE2_3.md, decision D2).
#
# Aurora's headers are the only SDK headers (the default and only mode since step 2.8): the decomp's own
# game/include/dolphin must never be reached. This script
#   1. reruns the compile of check/sdk_headers.cpp from compile_commands.json with -M (the -MD
#      dependency list, written to <build>/sdk_headers.deps),
#   2. fails if any dependency resolves under game/include/dolphin,
#   3. fails if a header name under game/include/dolphin is missing from sdk_headers.cpp,
#   4. lists the names still pending (between COS_SDK_PENDING_BEGIN/END; none since step 2.4).
#
# Usage: native/check/check_sdk_shadow.sh [build-dir]     (default: build/native-mac)
# Target: ninja -C <build-dir> cos_sdk_shadow_check
set -eu

here=$(cd "$(dirname "$0")" && pwd)
native=$(cd "$here/.." && pwd)
build=${1:-$native/../build/native-mac}
build=$(cd "$build" && pwd)

exec python3 - "$native" "$build" <<'PY'
import json, os, re, shlex, subprocess, sys

native, build = sys.argv[1], sys.argv[2]
unit = os.path.join(native, "check", "sdk_headers.cpp")
cos_dolphin = os.path.realpath(os.path.join(native, "..", "game", "include", "dolphin"))
deps_path = os.path.join(build, "sdk_headers.deps")

def fail(msg):
    print("check_sdk_shadow: FAIL: " + msg)
    sys.exit(1)

# 1. The unit's compile command, rerun with -M.
try:
    with open(os.path.join(build, "compile_commands.json")) as f:
        db = json.load(f)
except OSError as e:
    fail("no compile_commands.json in %s (%s)" % (build, e))
entry = next((e for e in db if os.path.realpath(e["file"]) == os.path.realpath(unit)), None)
if entry is None:
    fail("check/sdk_headers.cpp is not in %s/compile_commands.json" % build)
args = entry["arguments"] if "arguments" in entry else shlex.split(entry["command"])
sdk_dir = os.path.realpath(os.path.join(native, "include", "sdk"))
if not any(a.startswith("-I") and os.path.realpath(a[2:]) == sdk_dir for a in args):
    fail("%s does not compile against the SDK forwarders (native/include/sdk)" % build)
cmd = []
skip = False
for a in args:
    if skip:
        skip = False
        continue
    if a in ("-o", "-MF", "-MT", "-MQ"):
        skip = True
        continue
    if a in ("-c", "-MD", "-MMD"):
        continue
    cmd.append(a)
cmd += ["-M", "-MF", deps_path]
r = subprocess.run(cmd, cwd=entry["directory"], stdout=subprocess.DEVNULL,
                   stderr=subprocess.PIPE, text=True)
if r.returncode != 0:
    sys.stderr.write(r.stderr)
    fail("preprocessing check/sdk_headers.cpp failed")

# 2. Dependencies (make syntax: "obj: dep dep \", spaces in paths escaped as "\ ").
with open(deps_path) as f:
    text = f.read().replace("\\\n", " ")
text = text.split(":", 1)[1] if ":" in text else text
deps = [d.replace("\\ ", " ") for d in re.findall(r"(?:\\ |[^\s])+", text)]
deps = [os.path.realpath(os.path.join(entry["directory"], d)) for d in deps]
shadowed = sorted(d for d in deps if d == cos_dolphin or d.startswith(cos_dolphin + os.sep))
if shadowed:
    for d in shadowed:
        print("  reached: " + os.path.relpath(d, native))
    fail("%d dependencies resolve under game/include/dolphin" % len(shadowed))

# 3. Every decomp SDK header name is listed in the unit.
names = sorted(os.path.relpath(os.path.join(dp, fn), os.path.dirname(cos_dolphin))
               for dp, _, fns in os.walk(cos_dolphin) for fn in fns if fn.endswith(".h"))
src = open(unit).read()
listed = set(re.findall(r"^\s*#\s*include\s*[<\"](dolphin/[^>\"]+)[>\"]", src, re.M))
missing = [n for n in names if n not in listed]
if missing:
    fail("not listed in check/sdk_headers.cpp: " + " ".join(missing))

# 4. Pending names (not compiled until their forwarders exist).
m = re.search(r"COS_SDK_PENDING_BEGIN(.*?)COS_SDK_PENDING_END", src, re.S)
pending = sorted(set(re.findall(r"#\s*include\s*[<\"](dolphin/[^>\"]+)[>\"]", m.group(1)))) if m else []

sdk_fwd = os.path.realpath(os.path.join(native, "include", "sdk")) + os.sep
forwarded = sum(1 for d in deps if d.startswith(sdk_fwd))
print("check_sdk_shadow: ok: %d names checked, %d pending, %d dependencies, %d from "
      "native/include/sdk, none under game/include/dolphin"
      % (len(names) - len(pending), len(pending), len(deps), forwarded))
if pending:
    print("  pending (no forwarder yet): " + " ".join(pending))
PY
