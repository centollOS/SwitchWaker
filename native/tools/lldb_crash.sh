#!/usr/bin/env bash
# Rerun the native executable centollos under lldb in batch mode and print every thread's backtrace,
# the frame variables and the registers at the crash (docs/NATIVE_PORT_PHASE4_6.md, step 6.0,
# "The crash-to-fix loop"). The COS_* environment is inherited by the inferior, so rerun with the
# variables of the failing run (its env.txt):
#
#   env $(cat build/native-mac/runs/<run>/env.txt) native/tools/lldb_crash.sh [--timeout S] [-- args]
#
# lldb needs developer mode (DevToolsSecurity) to launch a process. With it off, lldb waits for an
# authorisation prompt; this script never waits on it: it checks `DevToolsSecurity -status` first
# and exits 3 without starting lldb (use --force to try anyway, still under the timeout). The
# built-in crash handler's backtrace.txt (native/tools/run.sh) is the primary tool.
#
# Output: build/native-mac/runs/lldb-<timestamp>/lldb.txt, also printed.
# Exit codes: lldb's (13 after a stop, via "quit 13"; 0 if the program exited normally), 3 if lldb
# cannot run non-interactively, 10 if it did not finish within the timeout (default 240 s).
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
build="$repo/build/native-mac"
exe="$build/centollos"
timeout_s=240
force=0
extra=()
while [ $# -gt 0 ]; do
    case "$1" in
        --timeout) timeout_s="$2"; shift 2 ;;
        --exe) exe="$2"; shift 2 ;;
        --force) force=1; shift ;;
        --) shift; extra=("$@"); break ;;
        -h|--help) sed -n '2,/^set -u/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 2 ;;
        *) echo "lldb_crash: unknown option $1" >&2; exit 2 ;;
    esac
done
[ -x "$exe" ] || { echo "lldb_crash: $exe not built" >&2; exit 2; }

if [ "$force" = 0 ] && ! DevToolsSecurity -status 2>/dev/null | grep -q "enabled"; then
    echo "lldb_crash: developer mode is off (DevToolsSecurity -status), so lldb would wait for an" >&2
    echo "  authorisation prompt; not starting it. Use the crash handler's backtrace.txt from" >&2
    echo "  native/tools/run.sh, or enable developer mode (decision H9)." >&2
    exit 3
fi

out_dir="$build/runs/lldb-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out_dir"
out="$out_dir/lldb.txt"

lldb --batch \
    -o run \
    -k "thread backtrace all" \
    -k "frame variable" \
    -k "register read" \
    -k "quit 13" \
    -- "$exe" ${extra[@]+"${extra[@]}"} > "$out" 2>&1 &
pid=$!

kill_tree() {
    local p="$1" c
    for c in $(pgrep -P "$p" 2>/dev/null); do
        kill_tree "$c"
    done
    kill -9 "$p" 2>/dev/null
}
trap 'kill_tree "$pid"; exit 130' INT TERM

start=$(date +%s)
timed_out=0
while kill -0 "$pid" 2>/dev/null; do
    if [ $(( $(date +%s) - start )) -ge "$timeout_s" ]; then
        kill_tree "$pid"
        timed_out=1
        break
    fi
    sleep 0.5
done
wait "$pid" 2>/dev/null
rc=$?
trap - INT TERM
cat "$out"
if [ "$timed_out" = 1 ]; then
    if grep -q "Developer mode" "$out" || ! grep -q "^Process " "$out"; then
        echo "lldb_crash: lldb never launched the process (waiting for developer-mode" >&2
        echo "  authorisation?); killed" >&2
        exit 3
    fi
    echo "lldb_crash: lldb did not finish within ${timeout_s}s; killed (output in ${out#"$repo"/})" >&2
    exit 10
fi
echo "lldb_crash: exit $rc; output in ${out#"$repo"/}" >&2
exit "$rc"
