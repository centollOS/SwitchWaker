#!/usr/bin/env bash
# Copy Switch probe NROs to the console over USB (MTP) and pull back their logs.
#
#   scripts/switch/push.sh [--build] [host|host-aurora|dawn|gles|boot|FILE.nro]...   (default: dawn)
#   scripts/switch/push.sh --logs
#   scripts/switch/push.sh --game DISC.iso
#
# Enable USB file transfer on the console first (Horizon's own, haze or DBI).
# Files go to sdmc:/switch/centollos/, are read back, and must match
# the local SHA-256. --build runs each probe's build script first. --logs
# copies the probes' *.log files into build/switch-logs/. --game copies the
# headless host's inputs from the player's own disc: the disc image as
# GZLE01.iso, the DSP ROMs, and main.dol and the 415 RELs extracted by
# scripts/builder/build.sh --source-only (build/device/game); files already
# on the console with the same size are skipped. Needs libmtp
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

if [[ ${1:-} == --logs ]]; then
    mkdir -p "$root/build/switch-logs"
    for log in boot-probe.log gles-probe.log dawn-probe.log host.log; do
        status=0
        "$tool" pull "$remote_dir" "$log" "$root/build/switch-logs/$log" || status=$?
        # 3: that probe has not written a log yet.
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
