#!/usr/bin/env bash
# Copy the native port's NRO and its inputs to the console over USB (MTP) and pull back its logs.
#
#   scripts/switch/push.sh [--build] [--no-pipeline-cache] [native|FILE.nro]...   (default: native)
#   scripts/switch/push.sh --logs
#   scripts/switch/push.sh --disc DISC.iso
#   scripts/switch/push.sh --native-env ENV.txt
#   scripts/switch/push.sh --pipeline-cache [FILE.db]
#
# Enable USB file transfer on the console first (Horizon's own, haze or DBI).
# Files go to sdmc:/switch/switchwaker/, are read back, and must match
# the local SHA-256. --build runs scripts/switch/build_native.sh first. --logs
# copies the native port's logs into build/switch-logs/native/. --disc copies
# the player's disc image as GZLE01.iso (all the native port reads; skipped if
# already on the console with the same size). `native` is the native port's NRO
# (scripts/switch/build_native.sh), switchwaker.nro;
# --native-env copies a run options file as its native/env.txt. --pipeline-cache copies the
# bundled pipeline cache (default native/data/initial_pipeline_cache.db, the committed one that
# native/tools/gen_pipeline_cache.sh updates) as initial_pipeline_cache.db next to the NRO, where
# Aurora seeds its pipeline cache from at every start, and checks the read-back. Pushing an NRO
# pushes that file as well (unless --no-pipeline-cache): without it the console has no warm-up
# and no "Preparing shaders" screen, and every pipeline is built when first drawn. Needs libmtp
# (macOS: brew install libmtp); runs on the host, not in a container.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
remote_dir=switch/switchwaker
tool_dir="$root/build/switch-tools"
tool="$tool_dir/switch_mtp"
source="$root/scripts/switch/switch_mtp.c"

if [[ ! -x $tool || $source -nt $tool ]]; then
    mkdir -p "$tool_dir"
    if ! flags=$(pkg-config --cflags --libs libmtp 2>/dev/null); then
        echo "push: libmtp not found (macOS: brew install libmtp)" >&2
        exit 1
    fi
    # shellcheck disable=SC2086  # pkg-config output is a word list
    cc -O2 -Wall -o "$tool" "$source" $flags
fi

if [[ ${1:-} == --disc ]]; then
    disc=${2:?usage: push.sh --disc DISC.iso}
    staging=$(mktemp -d)
    trap 'rm -rf "$staging"' EXIT
    ln -s "$(cd "$(dirname "$disc")" && pwd)/$(basename "$disc")" "$staging/GZLE01.iso"
    "$tool" push-many "$remote_dir" "$staging/GZLE01.iso"
    exit 0
fi

if [[ ${1:-} == --native-env ]]; then
    env_file=${2:?usage: push.sh --native-env ENV.txt}
    staging=$(mktemp -d)
    trap 'rm -rf "$staging"' EXIT
    cp "$env_file" "$staging/env.txt"
    "$tool" push "$staging/env.txt" "$remote_dir/native"
    echo "pushed $env_file as $remote_dir/native/env.txt"
    exit 0
fi

bundled_db="$root/native/data/initial_pipeline_cache.db"
push_pipeline_cache() { # FILE.db
    local db=$1 staging want got
    if [[ ! -s $db ]]; then
        echo "push: $db is missing" >&2
        exit 1
    fi
    staging=$(mktemp -d)
    # Aurora looks for this exact name in its resources path (the NRO's directory).
    cp "$db" "$staging/initial_pipeline_cache.db"
    "$tool" push "$staging/initial_pipeline_cache.db" "$remote_dir" "$staging/readback.db"
    want=$(shasum -a 256 "$db" | cut -d' ' -f1)
    got=$(shasum -a 256 "$staging/readback.db" | cut -d' ' -f1)
    rm -rf "$staging"
    if [[ $want != "$got" ]]; then
        echo "push: read-back of initial_pipeline_cache.db does not match ($got != $want)" >&2
        exit 1
    fi
    echo "verified initial_pipeline_cache.db $want"
}

if [[ ${1:-} == --pipeline-cache ]]; then
    push_pipeline_cache "${2:-$bundled_db}"
    exit 0
fi

if [[ ${1:-} == --logs ]]; then
    # The native port's logs (switch/native/source/cos_switch.cpp); status 3: no log written yet.
    mkdir -p "$root/build/switch-logs/native"
    for log in switchwaker.log switchwaker.prev.log; do
        status=0
        "$tool" pull "$remote_dir/native" "$log" "$root/build/switch-logs/native/$log" || status=$?
        [[ $status -eq 0 || $status -eq 3 ]] || exit "$status"
    done
    exit 0
fi

build=0
with_db=1
while [[ ${1:-} == --build || ${1:-} == --no-pipeline-cache ]]; do
    [[ $1 == --build ]] && build=1
    [[ $1 == --no-pipeline-cache ]] && with_db=0
    shift
done
[[ $# -gt 0 ]] || set -- native

for target in "$@"; do
    case $target in
        native) script=build_native.sh nro=build/switch-native/switchwaker.nro ;;
        *.nro) script='' nro=$target ;;
        *) echo "push: unknown target $target" >&2; exit 2 ;;
    esac
    [[ $nro == /* ]] || nro="$root/$nro"
    if [[ $build -eq 1 && -n $script ]]; then
        bash "$root/scripts/switch/$script"
    fi
    if [[ ! -s $nro ]]; then
        echo "push: $nro is missing; build it or pass --build" >&2
        exit 1
    fi

    copy=$(mktemp)
    trap 'rm -f "$copy"' EXIT
    "$tool" push "$nro" "$remote_dir" "$copy"
    want=$(shasum -a 256 "$nro" | cut -d' ' -f1)
    got=$(shasum -a 256 "$copy" | cut -d' ' -f1)
    rm -f "$copy"
    if [[ $want != "$got" ]]; then
        echo "push: read-back of $(basename "$nro") does not match ($got != $want)" >&2
        exit 1
    fi
    echo "verified $(basename "$nro") $want"
done
[[ $with_db -eq 0 ]] || push_pipeline_cache "$bundled_db"
