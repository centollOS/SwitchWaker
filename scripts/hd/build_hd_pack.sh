#!/usr/bin/env bash
# Converts a Dolphin-format HD texture pack for GZLE01 into the compact pack the game streams
# (docs/HD_TEXTURES.md). Runs on the Mac (or any host with CMake, Ninja and bsdtar).
#
#   scripts/hd/build_hd_pack.sh PACK [--max-size N] [--out DIR] [--overlay DIR]... [--fast] [--jobs N]
#
# PACK is the pack's .7z/.zip archive or an already extracted directory. An archive is extracted
# once into build/hd-src/<archive name>/ (about 10 GB for Hypatia's HD v2.0). The default
# texture set is the pack's GZL folder (found anywhere under PACK; the "-[Optional Textures]-"
# folders are not used unless given with --overlay, e.g. --overlay "<extracted>/-[Optional
# Textures]-/[Characters] Link - Tunic - FS - Green", whose files override GZL's of the same name).
# --max-size (default 1024, the Mac and docked; 512 suits the Switch handheld) caps the larger side
# of each texture; mips are generated (see native/tools/hd_pack/cos_hd_pack.cpp). The output
# (default build/hd-pack-<max-size>/) is index.bin + dataNN.bin: point COS_HD_PACK at it on the Mac,
# or copy it to the Switch with scripts/switch/push_hd_pack.sh.
#
# Everything this writes is derived from the pack's art: it stays under build/ (gitignored) or on
# the player's own SD card, never in git.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
[[ $# -ge 1 ]] || { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
pack=$1; shift
max_size=1024 out='' fast=() jobs=() overlays=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --max-size) max_size=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --overlay) overlays+=("$2"); shift 2 ;;
        --fast) fast=(--fast); shift ;;
        --jobs) jobs=(--jobs "$2"); shift 2 ;;
        *) echo "build_hd_pack: unknown option $1" >&2; exit 2 ;;
    esac
done
[[ -n $out ]] || out="$root/build/hd-pack-$max_size"

if [[ -f $pack ]]; then
    name=$(basename "$pack"); name=${name%.*}
    src="$root/build/hd-src/$name"
    if [[ ! -f $src/.extracted ]]; then
        echo "build_hd_pack: extracting $pack into $src"
        rm -rf "$src"; mkdir -p "$src"
        bsdtar -xf "$pack" -C "$src"
        touch "$src/.extracted"
    fi
else
    src=$pack
fi
gzl=$(find "$src" -type d -name GZL -not -path '*Optional*' | head -1)
[[ -n $gzl ]] || { echo "build_hd_pack: no GZL folder under $src" >&2; exit 1; }

tool_dir="$root/build/hd-pack-tool"
if [[ ! -x $tool_dir/cos_hd_pack || $root/native/tools/hd_pack/cos_hd_pack.cpp -nt $tool_dir/cos_hd_pack ]]; then
    cmake -S "$root/native/tools/hd_pack" -B "$tool_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
    ninja -C "$tool_dir" > /dev/null
fi

echo "build_hd_pack: $gzl ${overlays[*]:++ ${#overlays[@]} overlay(s)} -> $out (max size $max_size)"
"$tool_dir/cos_hd_pack" --max-size "$max_size" --out "$out" "${fast[@]}" "${jobs[@]}" "$gzl" "${overlays[@]}"
du -sh "$out"
