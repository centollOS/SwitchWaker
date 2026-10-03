#!/usr/bin/env bash
# Copy Switch probe NROs to the console over USB (MTP) and pull back their logs.
#
#   scripts/switch/push.sh [--build] [host|host-aurora|native|dawn|gles|boot|FILE.nro]...   (default: dawn)
#   scripts/switch/push.sh --logs
#   scripts/switch/push.sh --game DISC.iso
#   scripts/switch/push.sh --disc DISC.iso
#   scripts/switch/push.sh --native-env ENV.txt
#   scripts/switch/push.sh --pipeline-cache [FILE.db]
#
# Enable USB file transfer on the console first (Horizon's own, haze or DBI).
# Files go to sdmc:/switch/centollos/, are read back, and must match
# the local SHA-256. --build runs each probe's build script first. --logs
# copies the probes' *.log files into build/switch-logs/. --game copies the
# headless host's inputs from the player's own disc: the disc image as
# GZLE01.iso, the DSP ROMs, and main.dol and the 415 RELs extracted by
# scripts/builder/build.sh --source-only (build/device/game); files already
# on the console with the same size are skipped. --disc copies only the disc
# image (all the native port reads; same name and place as --game's). `native`
# is the native port's NRO (scripts/switch/build_native.sh), centollos.nro;
# --native-env copies a run options file as its native/env.txt. --pipeline-cache copies the
# bundled pipeline cache that native/tools/gen_pipeline_cache.sh made (default
# build/pipeline-cache/initial_pipeline_cache.db; it is derived from the player's disc and never
# committed) as initial_pipeline_cache.db next to the NRO, where Aurora seeds its pipeline cache
# from at every start, and checks the read-back. Needs libmtp
# (macOS: brew install libmtp); runs on the host, not in a container.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
remote_dir=switch/centollos
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

if [[ ${1:-} == --game ]]; then
    disc=${2:?usage: push.sh --game DISC.iso}
    game="$root/build/device/game"
    if [[ ! -f $game/main.dol || ! -d $game/rels ]]; then
        echo "push: run scripts/builder/build.sh DISC.iso --source-only first" >&2
        exit 1
    fi
    staging=$(mktemp -d)
    trap 'rm -rf "$staging"' EXIT
    # Name the disc as the host expects without copying 1.4 GB.
    ln -s "$(cd "$(dirname "$disc")" && pwd)/$(basename "$disc")" "$staging/GZLE01.iso"
    "$tool" push-many "$remote_dir" "$staging/GZLE01.iso" \
        "$root/ref/recompcore/Data/Sys/GC/dsp_rom.bin" "$root/ref/recompcore/Data/Sys/GC/dsp_coef.bin"
    "$tool" push-many "$remote_dir/game" "$game/main.dol"
    "$tool" push-many "$remote_dir/game/rels" "$game"/rels/*
    exit 0
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

if [[ ${1:-} == --pipeline-cache ]]; then
    db=${2:-$root/build/pipeline-cache/initial_pipeline_cache.db}
    if [[ ! -s $db ]]; then
        echo "push: $db is missing; run native/tools/gen_pipeline_cache.sh first" >&2
        exit 1
    fi
    staging=$(mktemp -d)
    trap 'rm -rf "$staging"' EXIT
    # Aurora looks for this exact name in its resources path (the NRO's directory).
    cp "$db" "$staging/initial_pipeline_cache.db"
    "$tool" push "$staging/initial_pipeline_cache.db" "$remote_dir" "$staging/readback.db"
    want=$(shasum -a 256 "$db" | cut -d' ' -f1)
    got=$(shasum -a 256 "$staging/readback.db" | cut -d' ' -f1)
    if [[ $want != "$got" ]]; then
        echo "push: read-back of initial_pipeline_cache.db does not match ($got != $want)" >&2
        exit 1
    fi
    echo "verified initial_pipeline_cache.db $want"
    exit 0
fi

if [[ ${1:-} == --logs ]]; then
    mkdir -p "$root/build/switch-logs"
    for log in boot-probe.log gles-probe.log dawn-probe.log host.log; do
        status=0
        "$tool" pull "$remote_dir" "$log" "$root/build/switch-logs/$log" || status=$?
        # 3: that probe has not written a log yet.
        [[ $status -eq 0 || $status -eq 3 ]] || exit "$status"
    done
    # The native port's logs (switch/native/source/cos_switch.cpp).
    mkdir -p "$root/build/switch-logs/native"
    for log in centollos.log centollos.prev.log; do
        status=0
        "$tool" pull "$remote_dir/native" "$log" "$root/build/switch-logs/native/$log" || status=$?
        [[ $status -eq 0 || $status -eq 3 ]] || exit "$status"
    done
    exit 0
fi

build=0
if [[ ${1:-} == --build ]]; then
    build=1
    shift
fi
[[ $# -gt 0 ]] || set -- dawn

for target in "$@"; do
    case $target in
        host) script=build_host.sh nro=build/switch-host/BlueWakeSwitch.nro ;;
        host-aurora) script='' nro=${SWITCH_HOST_BUILD_DIR:-build/switch-host-aurora}/BlueWakeSwitchAurora.nro ;;
        native) script=build_native.sh nro=build/switch-native/centollos.nro ;;
        dawn) script=build_dawn_probe.sh nro=build/switch-dawn-probe/BlueWakeDawnOffscreenProbe.nro ;;
        gles) script=build_gles_probe.sh nro=build/switch-gles-probe/BlueWakeGlesProbe.nro ;;
        boot) script=build_probe.sh nro=build/switch-probe/BlueWakeSwitchProbe.nro ;;
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
