#!/usr/bin/env bash
# Full regression of the native build in one command, with the game runs in parallel
# (docs/NATIVE_PORT_PHASE4_6.md, "Speed-up", 2026-10-03).
#
# Usage: native/tools/regress.sh [-j N] [--no-build] [--capped] [--rando-only] [--list]
#
#   1. builds every check target of build/native-mac (of the worktree this script lives in), or
#      of $COS_BUILD_DIR (e.g. build/native-linux);
#   2. runs the static checks: cos_sdk_smoke, cos_pc_tests, the link census against
#      native/check/expected_unresolved_phase2.txt and symbol_census --all --dups (both on the
#      Mac only: they read Mach-O objects), the phase 4 inventory against
#      native/check/phase4_baseline.txt;
#   3. runs the randomizer's unit tests: one executable cos_rando_test_<step> per
#      native/check/rando/test_<step>.cpp (docs/RANDOMIZER_PLAN.md step R0.1), each must print ok;
#   4. runs every target listed in native/check/regress_targets.txt and in
#      native/check/regress_rando/*.txt (one file per randomizer step, same format) through run.sh,
#      N at a time (default 4), uncapped unless --capped, and compares each exit code with the
#      expected one in that file.
#
#   --rando-only   only the randomizer's part: build switchwaker and cos_rando_tests, run the
#                  rando unit tests and the regress_rando/*.txt lines (no static checks, no
#                  regress_targets.txt)
#   --list         print the run lines (with the file each comes from) and the unit tests, run
#                  nothing
#
# A step that reaches a milestone or adds a smoke test appends it to regress_targets.txt in the
# same commit (a randomizer step: its own native/check/regress_rando/<step>.txt). Prints one line per check and a summary; exits 0 only if everything matched.
# Run directories stay in build/native-mac/runs/ (gitignored); the summary names the failing ones.
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
# COS_BUILD_DIR: another build directory (the Linux build: COS_BUILD_DIR=build/native-linux);
# run.sh reads the same variable. Default build/native-mac.
build="${COS_BUILD_DIR:-$repo/build/native-mac}"
case "$build" in /*) ;; *) build="$repo/$build" ;; esac
export COS_BUILD_DIR="$build"
targets_file="$repo/native/check/regress_targets.txt"
rando_dir="$repo/native/check/regress_rando"
rando_tests_dir="$repo/native/check/rando"

jobs=4
do_build=1
capped=0
rando_only=0
list_only=0
while [ $# -gt 0 ]; do
    case "$1" in
        -j) jobs="$2"; shift 2 ;;
        --no-build) do_build=0; shift ;;
        --capped) capped=1; shift ;;
        --rando-only) rando_only=1; shift ;;
        --list) list_only=1; shift ;;
        -h|--help) sed -n '2,/^set -u/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 2 ;;
        *) echo "regress: unknown option $1" >&2; exit 2 ;;
    esac
done

# The run lines: regress_targets.txt (unless --rando-only), then each regress_rando/*.txt in name
# order; "# <file>" lines for --list. Comments and blank lines dropped.
run_lines() { # with_sources
    local f
    local files=()
    [ "$rando_only" = 1 ] || files+=("$targets_file")
    for f in "$rando_dir"/*.txt; do
        [ -f "$f" ] && files+=("$f")
    done
    for f in "${files[@]}"; do
        [ "$1" = 1 ] && echo "# ${f#"$repo"/}"
        grep -v '^\s*#' "$f" | grep -v '^\s*$'
    done
}
# The randomizer's unit tests: the step name of every native/check/rando/test_<step>.cpp.
rando_test_steps() {
    local f
    for f in "$rando_tests_dir"/test_*.cpp; do
        [ -f "$f" ] || continue
        f="${f##*/test_}"
        echo "${f%.cpp}"
    done
}

if [ "$list_only" = 1 ]; then
    run_lines 1
    echo "# rando unit tests (native/check/rando/test_*.cpp)"
    for s in $(rando_test_steps); do echo "cos_rando_test_$s"; done
    exit 0
fi

out="$build/regress"
rm -rf "$out"
mkdir -p "$out"
fail=0
report() { # name status detail
    printf '%-28s %-4s %s\n' "$1" "$2" "$3"
    [ "$2" = ok ] || fail=$((fail + 1))
}

# The link census needs macOS ld64, nm and otool (native/cmake/census.cmake): only on the Mac.
census=0
[ "$(uname -s)" = Darwin ] && census=1
census_target=""
[ "$census" = 1 ] && census_target=cos_link_census

if [ "$do_build" = 1 ]; then
    build_targets="all switchwaker cos_sdk_smoke cos_pc_tests cos_rando_tests cos_layout_check cos_sdk_shadow_check $census_target"
    [ "$rando_only" = 1 ] && build_targets="switchwaker cos_rando_tests"
    # shellcheck disable=SC2086
    if ninja -C "$build" $build_targets > "$out/build.log" 2>&1; then
        report build ok ""
    else
        report build FAIL "$out/build.log"
        echo "regress: build failed, nothing else run" >&2
        exit 1
    fi
fi

check() { # name command...
    local name="$1"; shift
    if "$@" > "$out/$name.log" 2>&1; then report "$name" ok ""; else report "$name" FAIL "$out/$name.log"; fi
}
if [ "$rando_only" = 0 ]; then
check sdk_smoke "$build/cos_sdk_smoke"
check pc_tests "$build/cos_pc_tests"
if [ "$census" = 1 ]; then
    check link_census diff -u "$repo/native/check/expected_unresolved_phase2.txt" \
        "$build/link_census_unresolved.txt"
fi
# symbol_census.py reads Mach-O objects with nm -m and otool: Mac only. On Linux the strict link
# of switchwaker (ld.lld or GNU ld, no duplicate strong definitions allowed) covers the same check.
if [ "$census" = 1 ]; then
    check symbol_dups python3 "$script_dir/symbol_census.py" --all --dups
fi
check phase4_inventory python3 "$script_dir/phase4_inventory.py" --check \
    "$repo/native/check/phase4_baseline.txt"
fi

# The randomizer's unit tests: each must exist (built from its test_<step>.cpp), exit 0 and print
# "ok" as its last line. The randomizer's hook census (rando_hook_census.py) runs here too.
rando_test() { # step
    local exe="$build/cos_rando_test_$1"
    [ -x "$exe" ] || { echo "regress: $exe missing (not built?)"; return 1; }
    "$exe" || return 1
}
for s in $(rando_test_steps); do
    if rando_test "$s" > "$out/rando_test_$s.log" 2>&1 && [ "$(tail -n 1 "$out/rando_test_$s.log")" = ok ]; then
        report "rando_test:$s" ok ""
    else
        report "rando_test:$s" FAIL "$out/rando_test_$s.log"
    fi
done
check rando_hook_census python3 -I "$script_dir/rando_hook_census.py"

# The disc manifest is written once here, so parallel runs never race to create it.
# The disc the build runs from (native/cmake/GameConfig.cmake: GZLE01, or GZLP01 for COS_VERSION=3,
# docs/PAL_DISC_PLAN.md).
game_id="$(sed -n 's/^COS_GAME_ID:STRING=//p' "$build/CMakeCache.txt" 2>/dev/null)"
game_id="${game_id:-GZLE01}"
disc="${COS_DISC:-}"
if [ -z "$disc" ]; then
    echo "regress: no disc image: set COS_DISC (the $game_id revision 0 .iso)" >&2
    exit 14
fi
export COS_DISC="$disc"
python3 "$script_dir/disc_manifest.py" --verify --quiet --disc "$disc" --game "$game_id" > "$out/disc.log" 2>&1 ||
    report disc_verify FAIL "$out/disc.log"
manifest_version="$(sed -n 's/^MANIFEST_VERSION = \([0-9]*\)$/\1/p' "$script_dir/disc_manifest.py")"
if [ ! -f "$build/disc_manifest.json" ] ||
    ! grep -qF "\"manifest_version\": $manifest_version," "$build/disc_manifest.json"; then
    python3 "$script_dir/disc_manifest.py" --quiet --disc "$disc" --out "$build/disc_manifest.json" \
        >> "$out/disc.log" 2>&1 || report disc_manifest FAIL "$out/disc.log"
fi

# --- game runs ------------------------------------------------------------------------------------
# regress_targets.txt and regress_rando/*.txt: "<target> <expected exit> [extra run.sh options]";
# # starts a comment.
# The special target no-disc runs switchwaker with COS_DISC=/nonexistent. The special target
# script runs the rest of the line as a command from the repository root instead of run.sh (a
# host-only check such as the randomizer's oracle batch, regress_rando/R1.6.txt).
run_one() { # line
    local target expect opts rc
    read -r target expect opts <<< "$1"
    local tag
    tag="$(echo "$opts" | tr -c 'a-z0-9' '_')"
    # A file name holds 255 bytes: a long line keeps its head plus a checksum of the whole.
    [ "${#tag}" -gt 160 ] && tag="${tag:0:120}_$(echo "$opts" | cksum | cut -d' ' -f1)"
    local log="$out/run-$target-$tag.log"
    if [ "$target" = no-disc ]; then
        COS_DISC=/nonexistent "$build/switchwaker" > "$log" 2>&1
        rc=$?
    elif [ "$target" = script ]; then
        (cd "$repo" && bash -c "$opts") > "$log" 2>&1
        rc=$?
    else
        local pace="--uncapped"
        [ "$capped" = 1 ] && pace=""
        # shellcheck disable=SC2086
        "$script_dir/run.sh" "$target" $pace $opts --quiet > "$log" 2>&1
        rc=$?
    fi
    if [ "$rc" = "$expect" ]; then
        echo "ok $target $opts"
    else
        echo "FAIL $target $opts (exit $rc, expected $expect) $log"
    fi
}
export -f run_one
export out build script_dir capped repo

# NUL-separated with -n 1, not -I {}: BSD xargs -I caps the replaced line at 255 bytes and aborts
# the whole run on a longer line (regress_rando/R2.1.txt has one).
if ! run_lines 0 | tr '\n' '\0' |
    xargs -0 -n 1 -P "$jobs" bash -c 'run_one "$1"' _ > "$out/runs.txt"; then
    report runs FAIL "xargs did not run every line"
fi
while read -r status rest; do
    name="${rest%% *}"
    if [ "$status" = ok ]; then report "run:$name" ok "${rest#"$name"}"; else report "run:$name" FAIL "${rest#"$name"}"; fi
done < <(sort -k2 "$out/runs.txt")

pkill -f "$build/switchwaker" 2>/dev/null
echo
if [ "$fail" = 0 ]; then
    echo "regress: all checks passed"
    exit 0
fi
echo "regress: $fail check(s) failed"
exit 1
