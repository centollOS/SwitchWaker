#!/usr/bin/env bash
# Full regression of the native build in one command, with the game runs in parallel
# (docs/NATIVE_PORT_PHASE4_6.md, "Speed-up", 2026-10-03).
#
# Usage: native/tools/regress.sh [-j N] [--no-build] [--capped]
#
#   1. builds every check target of build/native-mac (of the worktree this script lives in);
#   2. runs the static checks: cos_sdk_smoke, cos_pc_tests, the link census against
#      native/check/expected_unresolved_phase2.txt, symbol_census --all --dups, the phase 4
#      inventory against native/check/phase4_baseline.txt;
#   3. runs every target listed in native/check/regress_targets.txt through run.sh, N at a
#      time (default 4), uncapped unless --capped, and compares each exit code with the expected
#      one in that file.
#
# A step that reaches a milestone or adds a smoke test appends it to regress_targets.txt in the
# same commit. Prints one line per check and a summary; exits 0 only if everything matched.
# Run directories stay in build/native-mac/runs/ (gitignored); the summary names the failing ones.
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
build="$repo/build/native-mac"
targets_file="$repo/native/check/regress_targets.txt"

jobs=4
do_build=1
capped=0
while [ $# -gt 0 ]; do
    case "$1" in
        -j) jobs="$2"; shift 2 ;;
        --no-build) do_build=0; shift ;;
        --capped) capped=1; shift ;;
        -h|--help) sed -n '2,/^set -u/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 2 ;;
        *) echo "regress: unknown option $1" >&2; exit 2 ;;
    esac
done

out="$build/regress"
rm -rf "$out"
mkdir -p "$out"
fail=0
report() { # name status detail
    printf '%-28s %-4s %s\n' "$1" "$2" "$3"
    [ "$2" = ok ] || fail=$((fail + 1))
}

if [ "$do_build" = 1 ]; then
    if ninja -C "$build" all centollos cos_sdk_smoke cos_pc_tests cos_layout_check cos_sdk_shadow_check \
        cos_link_census > "$out/build.log" 2>&1; then
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
check sdk_smoke "$build/cos_sdk_smoke"
check pc_tests "$build/cos_pc_tests"
check link_census diff -u "$repo/native/check/expected_unresolved_phase2.txt" \
    "$build/link_census_unresolved.txt"
check symbol_dups python3 "$script_dir/symbol_census.py" --all --dups
check phase4_inventory python3 "$script_dir/phase4_inventory.py" --check \
    "$repo/native/check/phase4_baseline.txt"

# The disc manifest is written once here, so parallel runs never race to create it.
disc="${COS_DISC:-}"
legacy_disc=/path/to/GZLE01.iso # the maintainer's, used only if it exists
[ -z "$disc" ] && [ -f "$legacy_disc" ] && disc="$legacy_disc"
if [ -z "$disc" ]; then
    echo "regress: no disc image: set COS_DISC (the GZLE01 revision 0 .iso)" >&2
    exit 14
fi
export COS_DISC="$disc"
python3 "$script_dir/disc_manifest.py" --verify --quiet --disc "$disc" > "$out/disc.log" 2>&1 ||
    report disc_verify FAIL "$out/disc.log"
manifest_version="$(sed -n 's/^MANIFEST_VERSION = \([0-9]*\)$/\1/p' "$script_dir/disc_manifest.py")"
if [ ! -f "$build/disc_manifest.json" ] ||
    ! grep -qF "\"manifest_version\": $manifest_version," "$build/disc_manifest.json"; then
    python3 "$script_dir/disc_manifest.py" --quiet --disc "$disc" --out "$build/disc_manifest.json" \
        >> "$out/disc.log" 2>&1 || report disc_manifest FAIL "$out/disc.log"
fi

# --- game runs ------------------------------------------------------------------------------------
# regress_targets.txt: "<target> <expected exit> [extra run.sh options]"; # starts a comment.
# The special target no-disc runs centollos with COS_DISC=/nonexistent.
run_one() { # line
    local target expect opts rc
    read -r target expect opts <<< "$1"
    local log="$out/run-$target-$(echo "$opts" | tr -c 'a-z0-9' '_').log"
    if [ "$target" = no-disc ]; then
        COS_DISC=/nonexistent "$build/centollos" > "$log" 2>&1
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
export out build script_dir capped

grep -v '^\s*#' "$targets_file" | grep -v '^\s*$' |
    xargs -P "$jobs" -I{} bash -c 'run_one "$@"' _ {} > "$out/runs.txt"
while read -r status rest; do
    name="${rest%% *}"
    if [ "$status" = ok ]; then report "run:$name" ok "${rest#"$name"}"; else report "run:$name" FAIL "${rest#"$name"}"; fi
done < <(sort -k2 "$out/runs.txt")

pkill -f "$build/centollos" 2>/dev/null
echo
if [ "$fail" = 0 ]; then
    echo "regress: all checks passed"
    exit 0
fi
echo "regress: $fail check(s) failed"
exit 1
