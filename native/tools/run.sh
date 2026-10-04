#!/usr/bin/env bash
# Run the native executable centollos to a milestone or through a smoke test, with a timeout, and keep
# what the run left in build/native-mac/runs/<target>-<timestamp>/ (docs/NATIVE_PORT_PHASE4_6.md,
# step 6.0 and "The crash-to-fix loop").
#
#   native/tools/run.sh <target> [options] [-- extra arguments for centollos]
#
# <target> is a milestone (static-init, aurora-up, heaps, ... see COS_MILESTONE) or a smoke test
# (crash-test, ... see COS_SMOKE). static-init is milestone M0 and runs the static-init smoke test.
# `run` boots the game with neither: it ends with --frames (exit 0), the timeout or a fault, e.g.
#   native/tools/run.sh run --frames 600 [--uncapped]      (prints the [cos] pacing line)
# `boot-sweep` boots every stage of the disc in turn (step F4-boot-sweep): it hands its options to
# native/tools/boot_sweep.py (see its --help), which runs this script once per stage.
# `actor-sweep` (step 6.9, needs --stage, e.g. sea:44:206) spawns every actor profile next to Link,
# runs it 30 frames and deletes it; COS_ACTOR_SWEEP=<first>[-<last>] limits it to those process
# names. native/tools/actor_sweep.py runs it again after each fault and lists the faults.
#
# Options:
#   --timeout S      in-process watchdog timeout (COS_TIMEOUT_S), default 180
#   --stall S        frame-counter stall limit (COS_STALL_S), default 30
#   --frames N       exit 0 after N game frames (COS_FRAMES)
#   --trace LIST     trace channels (COS_TRACE), e.g. res,scene
#   --uncapped       COS_UNCAPPED=1
#   --audio on|off   COS_AUDIO (default: on since step 5.A, decision H10: JAudio and the DSP run)
#   --audio-dump F   COS_AUDIO_DUMP (step 5.3): the AI's DMA output as a WAV file; a relative F
#                    is put in the run directory, e.g. --audio-dump audio.wav
#   --perf F         COS_PERF (step 6.7): one CSV row of game-thread times per game frame; a
#                    relative F is put in the run directory, e.g. --perf perf.csv
#   --sound          play the audio on SDL's default device; without it SDL_AUDIO_DRIVER=dummy
#                    (headless; an SDL_AUDIO_DRIVER already in the environment is kept)
#   --disc PATH      COS_DISC, the GZLE01 revision 0 .iso: required (option or environment) for
#                    every target that boots the game; without either, the maintainer's
#                    /path/to/GZLE01.iso is used only if it exists
#   --input PATH     COS_INPUT, the controller script (step 6.3; a relative path is taken from the
#                    current directory, else from the repository); pad-echo defaults to
#                    native/check/input/pad-echo.txt. The real new-game flow (new-game M11,
#                    outset-real M14) runs with native/check/input/new-game.txt; those two
#                    milestones start from a clean memory card in <run dir>/card/
#   --stage SPEC     COS_BOOT_STAGE, debug stage boot (step 6.4): <stage>:<room>[:<point>[:<layer>]],
#                    e.g. sea:44:206 (Outset, where the new game starts); COS_BOOT_EVENTS=2A80[,...]
#                    in the environment sets those story event bits on the new file and
#                    COS_BOOT_ITEMS=20[,...] gives it those items, the first on X (bug B6)
#   --shot LIST      COS_SHOT, game frames whose presented image is saved as shot-<frame>.png in
#                    the run directory, e.g. 30,200 (COS_SHOT_EVERY=n in the environment: every
#                    n-th frame; COS_SHOT_DIR: another directory). A run with shots compiles each
#                    pipeline before its first draw (COS_SYNC_PIPELINES, default on with shots), so
#                    a shot never misses a draw whose pipeline was still compiling
#   --aspect A       COS_ASPECT: 4:3, 16:9 or 16:10 (the widescreen option, docs/MODS.md)
#   --heap-check N   COS_HEAP_CHECK: every N game frames check() every JKR heap (and, on the
#                    Switch, walk newlib's free lists); a damaged heap ends the run as exit 1
#   --build          run `ninja -C build/native-mac centollos` first
#   --exe PATH       the executable (default build/native-mac/centollos)
#   --run-dir DIR    put the run in DIR (created; must not exist yet) instead of
#                    build/native-mac/runs/<target>-<timestamp>
#   --quiet          do not print the tail of the log on failure
# Other COS_* variables already in the environment are passed through. COS_CACHE_PER_RUN=1 gives
# the run its own Aurora caches in <run dir>/cache (COS_CACHE_DIR) instead of the shared
# build/native-mac/user/cache: its pipeline_cache.db then lists only the pipelines this run used
# (native/tools/gen_pipeline_cache.sh).
#
# Exit codes (those of centollos): 0 reached, 1 smoke check failed, 2 usage, 10 timeout, 11 stall,
# 12 panic, 13 signal, 14 disc problem. If the process does not end within the timeout plus a
# grace period it is killed and the run counts as a stall (11); a process killed by a signal the
# crash handler could not catch counts as 13.
#
# The run directory holds: command.txt, env.txt, run.log (stdout and stderr), exit_code.txt and,
# when the harness wrote them, backtrace.txt (crash or panic; atos file:line names are appended)
# and stall.txt (every thread's backtrace). On first use the SHA-1 of the disc image and of its
# main.dol are checked against the supported revision (native/tools/disc_manifest.py --verify,
# which holds the expected hashes); the result is cached in build/native-mac/runs/disc_check.txt.
# disc-ls (step 4.0d), font (step 4.3), arc-sweep (step 4.4), msg-sweep (step 4.6), jpa-sweep
# (step 4.7), stage-sweep (step 4.9a), blo-sweep (step 4.13), dzb-sweep (step 4.10),
# audio-parse (step 5.1), j3d-sweep (step 4.11), anm-sweep (step 4.12) and stb-sweep (step 4.17)
# are then compared with the disc manifest (build/native-mac/disc_manifest.json,
# written by disc_manifest.py if missing or of an older MANIFEST_VERSION): a difference turns exit 0 into 1.
# Nothing the run writes is meant for git (build/ is ignored).
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
build="$repo/build/native-mac"

milestones=" static-init aurora-up heaps gfx-create frame-loop logo-scene logo-res opening title-stage title file-select new-game outset-debug outset-control outset-real "
disc_manifest="$script_dir/disc_manifest.py"
grace_s=30

usage() {
    sed -n '2,/^set -u/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'
    exit 2
}

[ $# -ge 1 ] || usage
target="$1"
shift
case "$target" in -h|--help) usage ;; esac
if [ "$target" = boot-sweep ]; then
    exec python3 "$script_dir/boot_sweep.py" "$@"
fi

timeout_s=180
stall_s=30
frames=""
trace="${COS_TRACE:-}"
uncapped="${COS_UNCAPPED:-}"
audio="${COS_AUDIO:-on}"
disc="${COS_DISC:-}"
legacy_disc=/path/to/GZLE01.iso
[ -z "$disc" ] && [ -f "$legacy_disc" ] && disc="$legacy_disc"
input="${COS_INPUT:-}"
stage="${COS_BOOT_STAGE:-}"
shot="${COS_SHOT:-}"
audio_dump="${COS_AUDIO_DUMP:-}"
perf="${COS_PERF:-}"
aspect="${COS_ASPECT:-}"
heap_check="${COS_HEAP_CHECK:-}"
sound=0
do_build=0
exe="$build/centollos"
quiet=0
run_dir_opt=""
extra=()
while [ $# -gt 0 ]; do
    case "$1" in
        --timeout) timeout_s="$2"; shift 2 ;;
        --stall) stall_s="$2"; shift 2 ;;
        --frames) frames="$2"; shift 2 ;;
        --trace) trace="$2"; shift 2 ;;
        --uncapped) uncapped=1; shift ;;
        --audio) audio="$2"; shift 2 ;;
        --audio-dump) audio_dump="$2"; shift 2 ;;
        --perf) perf="$2"; shift 2 ;;
        --sound) sound=1; shift ;;
        --disc) disc="$2"; shift 2 ;;
        --input) input="$2"; shift 2 ;;
        --stage) stage="$2"; shift 2 ;;
        --shot) shot="$2"; shift 2 ;;
        --aspect) aspect="$2"; shift 2 ;;
        --heap-check) heap_check="$2"; shift 2 ;;
        --build) do_build=1; shift ;;
        --exe) exe="$2"; shift 2 ;;
        --quiet) quiet=1; shift ;;
        --run-dir) run_dir_opt="$2"; shift 2 ;;
        --) shift; extra=("$@"); break ;;
        *) echo "run: unknown option $1" >&2; usage ;;
    esac
done
case "$timeout_s" in ''|*[!0-9.]*) echo "run: --timeout needs seconds" >&2; exit 2 ;; esac
case "$stall_s" in ''|*[!0-9.]*) echo "run: --stall needs seconds" >&2; exit 2 ;; esac

needs_disc=1
case "$target" in static-init|crash-test|panic-test|stall-test|timeout-test) needs_disc=0 ;; esac
if [ "$needs_disc" = 1 ] && [ -z "$disc" ]; then
    echo "run: no disc image: pass --disc PATH or set COS_DISC (the GZLE01 revision 0 .iso)" >&2
    exit 14
fi

if [ "$do_build" = 1 ]; then
    ninja -C "$build" centollos >/dev/null || { echo "run: build failed" >&2; exit 2; }
fi
[ -x "$exe" ] || { echo "run: $exe not built (ninja -C build/native-mac centollos)" >&2; exit 2; }

runs="$build/runs"
mkdir -p "$runs"

# --- disc: SHA-1 of the image and of main.dol on first use (decision H9) -----------------------
# Only when the target boots the game: a smoke test that runs before the disc check needs none.
# A missing file is left to centollos, which exits 14 with its own message.
if [ "$needs_disc" = 1 ] && [ -f "$disc" ]; then
    python3 "$disc_manifest" --verify --quiet --disc "$disc" || exit 14
fi

# --- environment ------------------------------------------------------------------------------
if [ -n "$run_dir_opt" ]; then
    [ -e "$run_dir_opt" ] && { echo "run: --run-dir $run_dir_opt exists" >&2; exit 2; }
    mkdir -p "$run_dir_opt" || exit 2
    run_dir="$(cd "$run_dir_opt" && pwd)"
else
    ts="$(date +%Y%m%d-%H%M%S)"
    run_dir="$runs/$target-$ts"
    n=1
    # mkdir without -p claims the name atomically: runs started in the same second (regress.sh
    # -j, several runs of one target in parallel) each get their own directory, and so their own
    # memory card folder (<run dir>/card/), instead of sharing one found free by a separate test.
    until mkdir "$run_dir" 2> /dev/null; do
        [ "$n" -le 1000 ] || { echo "run: cannot create a run directory in $runs" >&2; exit 2; }
        run_dir="$runs/$target-$ts-$n"
        n=$((n + 1))
    done
fi

unset COS_SMOKE COS_MILESTONE
if [ "$target" = "run" ]; then
    : # the game itself: no milestone, no smoke test
elif [[ "$milestones" == *" $target "* ]]; then
    export COS_MILESTONE="$target"
    [ "$target" = "static-init" ] && export COS_SMOKE=static-init
else
    export COS_SMOKE="$target"
fi
export COS_DISC="$disc"
[ -z "$input" ] && [ "$target" = pad-echo ] && input="$repo/native/check/input/pad-echo.txt"
if [ -n "$input" ]; then
    case "$input" in
        /*) ;;
        *) if [ -f "$input" ]; then input="$(pwd)/$input"; else input="$repo/$input"; fi ;;
    esac
    export COS_INPUT="$input"
fi
export COS_TIMEOUT_S="$timeout_s"
export COS_STALL_S="$stall_s"
export COS_AUDIO="$audio"
export COS_RUN_DIR="$run_dir"
[ "${COS_CACHE_PER_RUN:-}" = 1 ] && export COS_CACHE_DIR="$run_dir/cache"
[ -n "$frames" ] && export COS_FRAMES="$frames"
if [ -n "$stage" ]; then export COS_BOOT_STAGE="$stage"; else unset COS_BOOT_STAGE; fi
[ -n "$trace" ] && export COS_TRACE="$trace"
[ -n "$shot" ] && export COS_SHOT="$shot"
[ -n "$uncapped" ] && export COS_UNCAPPED="$uncapped"
if [ -n "$aspect" ]; then export COS_ASPECT="$aspect"; else unset COS_ASPECT; fi
if [ -n "$heap_check" ]; then export COS_HEAP_CHECK="$heap_check"; else unset COS_HEAP_CHECK; fi
if [ -n "$audio_dump" ]; then
    case "$audio_dump" in /*) ;; *) audio_dump="$run_dir/$audio_dump" ;; esac
    export COS_AUDIO_DUMP="$audio_dump"
else
    unset COS_AUDIO_DUMP
fi
if [ -n "$perf" ]; then
    case "$perf" in /*) ;; *) perf="$run_dir/$perf" ;; esac
    export COS_PERF="$perf"
else
    unset COS_PERF
fi
# Runs are headless (and several lanes run at once): SDL's dummy audio driver unless --sound.
if [ "$sound" = 0 ]; then export SDL_AUDIO_DRIVER="${SDL_AUDIO_DRIVER:-dummy}"; fi

printf '%q ' "$exe" "${extra[@]+"${extra[@]}"}" > "$run_dir/command.txt"
echo >> "$run_dir/command.txt"
env | grep '^COS_' | sort > "$run_dir/env.txt"

# --- run --------------------------------------------------------------------------------------
start=$(date +%s)
"$exe" ${extra[@]+"${extra[@]}"} > "$run_dir/run.log" 2>&1 &
pid=$!
cleanup() {
    if kill -0 "$pid" 2>/dev/null; then
        kill -9 "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
    fi
}
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM

hard_limit=$(python3 -c "import math; print(int(math.ceil($timeout_s)) + $grace_s)")
killed=0
while kill -0 "$pid" 2>/dev/null; do
    if [ $(( $(date +%s) - start )) -ge "$hard_limit" ]; then
        echo "run: no exit $hard_limit s after start: killing $pid" >> "$run_dir/run.log"
        kill -9 "$pid" 2>/dev/null
        killed=1
        break
    fi
    sleep 0.2
done
wait "$pid" 2>/dev/null
rc=$?
trap - EXIT INT TERM
elapsed=$(( $(date +%s) - start ))
if [ "$killed" = 1 ]; then
    rc=11
elif [ "$rc" -gt 128 ]; then
    echo "run: centollos died of signal $((rc - 128)) without the crash handler" >> "$run_dir/run.log"
    rc=13
fi

# --- disc-ls (4.0d), font (4.3), arc-sweep (4.4), msg-sweep (4.6), jpa-sweep (4.7), stage-sweep
# (4.9a), blo-sweep (4.13), dzb-sweep (4.10), audio-parse (5.1), j3d-sweep (4.11), anm-sweep (4.12),
# stb-sweep (4.17): what the game read against the manifest
check_arg=""
case "$target" in
    disc-ls) check_arg="--check-ls"; check_file="disc_ls.txt" ;;
    font) check_arg="--check-font"; check_file="font.txt" ;;
    arc-sweep) check_arg="--check-arc"; check_file="arc_sweep.txt" ;;
    msg-sweep) check_arg="--check-msg"; check_file="msg_sweep.txt" ;;
    jpa-sweep) check_arg="--check-jpa"; check_file="jpa_sweep.txt" ;;
    stage-sweep) check_arg="--check-stage"; check_file="stage_sweep.txt" ;;
    blo-sweep) check_arg="--check-blo"; check_file="blo_sweep.txt" ;;
    dzb-sweep) check_arg="--check-dzb"; check_file="dzb_sweep.txt" ;;
    audio-parse) check_arg="--check-audio"; check_file="audio_parse.txt" ;;
    j3d-sweep) check_arg="--check-j3d"; check_file="j3d_sweep.txt" ;;
    anm-sweep) check_arg="--check-anm"; check_file="anm_sweep.txt" ;;
    stb-sweep) check_arg="--check-stb"; check_file="stb_sweep.txt" ;;
esac
if [ -n "$check_arg" ] && [ "$rc" = 0 ]; then
    manifest="$build/disc_manifest.json"
    manifest_version="$(sed -n 's/^MANIFEST_VERSION = \([0-9]*\)$/\1/p' "$disc_manifest")"
    if [ ! -f "$manifest" ] || ! grep -qF "\"path\": \"$disc\"" "$manifest" ||
        ! grep -qF "\"manifest_version\": $manifest_version," "$manifest"; then
        echo "run: writing $manifest" >> "$run_dir/run.log"
        python3 "$disc_manifest" --quiet --disc "$disc" --out "$manifest" >> "$run_dir/run.log" 2>&1 || rc=1
    fi
    if [ "$rc" = 0 ]; then
        python3 "$disc_manifest" --out "$manifest" "$check_arg" "$run_dir/$check_file" \
            >> "$run_dir/run.log" 2>&1 || rc=1
    fi
    grep '^disc_manifest: \(FST\|disc-ls\|font.txt\|arc_sweep.txt\|msg_sweep.txt\|jpa_sweep.txt\|stage_sweep.txt\|blo_sweep.txt\|dzb_sweep.txt\|audio_parse.txt\|j3d_sweep.txt\|anm_sweep.txt\|stb_sweep.txt\|DIFF\)' "$run_dir/run.log" | head -5
fi
echo "$rc" > "$run_dir/exit_code.txt"

# --- symbolise --------------------------------------------------------------------------------
for f in backtrace.txt stall.txt; do
    [ -f "$run_dir/$f" ] || continue
    load="$(sed -n 's/^\[cos\] image .* load=\(0x[0-9a-f]*\).*/\1/p' "$run_dir/$f" | head -1)"
    [ -n "$load" ] || continue
    {
        echo
        echo "[run] atos -o $exe -l $load (file:line where the debug info allows):"
        grep '^\[cos\] frames' "$run_dir/$f" | while IFS= read -r line; do
            # shellcheck disable=SC2086
            atos -o "$exe" -l "$load" ${line#*:} 2>/dev/null | grep -v '^0x' || true
            echo "--"
        done
    } >> "$run_dir/$f"
done

case "$rc" in
    0) meaning="reached" ;;
    1) meaning="smoke check failed" ;;
    2) meaning="usage error" ;;
    10) meaning="timeout" ;;
    11) meaning="stall" ;;
    12) meaning="panic" ;;
    13) meaning="signal" ;;
    14) meaning="disc problem" ;;
    *) meaning="unexpected exit" ;;
esac
echo "run: $target: exit $rc ($meaning) after ${elapsed}s; run dir ${run_dir#"$repo"/}"
if [ "$rc" != 0 ] && [ "$quiet" = 0 ]; then
    tail -n 25 "$run_dir/run.log" | sed 's/^/  | /'
    [ -f "$run_dir/backtrace.txt" ] && echo "  backtrace: ${run_dir#"$repo"/}/backtrace.txt"
    [ -f "$run_dir/stall.txt" ] && echo "  threads:   ${run_dir#"$repo"/}/stall.txt"
fi
exit "$rc"
