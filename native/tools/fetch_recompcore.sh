#!/usr/bin/env bash
# Fetch the RecompCore sources the native build compiles Dolphin's DSP HLE from (step 5.A,
# decision H6; native/cmake/dsp_hle.cmake reads COS_RECOMPCORE_DIR, default ref/recompcore).
#
#   native/tools/fetch_recompcore.sh [DIR]      (default: ref/recompcore of this repository)
#
# It clones https://github.com/elliotttate/RecompCore at the commit the recompilation project builds from
# (branch bluewake, patches/recompcore/README.md), shallow and without submodules: only the
# DSPHLE, DSP accelerator and Common sources are compiled, nothing is generated inside it. An
# existing checkout at the pin is left alone; one at another commit is reported, not changed.
set -euo pipefail

url="https://github.com/elliotttate/RecompCore.git"
pin="8ab24daee9c641634fda5cac30389ad4b2cfda5e"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
dir="${1:-$repo/ref/recompcore}"

if [ -d "$dir/.git" ] || [ -f "$dir/.git" ]; then
    head="$(git -C "$dir" rev-parse HEAD)"
    if [ "$head" = "$pin" ]; then
        echo "fetch_recompcore: $dir is already at ${pin:0:7}"
        exit 0
    fi
    echo "fetch_recompcore: $dir is at ${head:0:7}, not the pin ${pin:0:7}; left unchanged" >&2
    echo "  (git -C \"$dir\" fetch --depth 1 origin $pin && git -C \"$dir\" checkout --detach $pin)" >&2
    exit 1
fi
if [ -e "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
    echo "fetch_recompcore: $dir exists and is not a git checkout" >&2
    exit 1
fi

mkdir -p "$dir"
git -C "$dir" init -q
git -C "$dir" remote add origin "$url"
git -C "$dir" fetch -q --depth 1 origin "$pin"
git -C "$dir" -c advice.detachedHead=false checkout -q --detach FETCH_HEAD
test -f "$dir/Source/Core/Core/HW/DSPHLE/UCodes/UCodes.cpp"
echo "fetch_recompcore: $dir at ${pin:0:7}"
