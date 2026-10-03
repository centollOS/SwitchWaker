#!/usr/bin/env bash
# Generate the asset headers the game units include ("assets/...", "res/Object/...") from the
# player's disc, the way the decompilation's own build does (native/README.md, "Asset headers").
#
#   native/tools/gen_assets.sh [--disc PATH] [--decomp DIR] [--out DIR]
#
#   --disc PATH    the GZLE01 revision 0 disc image (default: $COS_DISC)
#   --decomp DIR   where the decompilation is checked out and run (default: build/decomp)
#   --out DIR      where the headers go (default: build/native-mac/assets/GZLE01, the default
#                  COS_ASSETS_DIR of native/cmake/GameConfig.cmake)
#
# Steps: a shallow checkout of the decompilation fork at b09eebc (the commit game
# was imported from), the disc linked into its orig/GZLE01/, `python configure.py`, then only the
# ninja targets that write headers: the `dtk dol split` of main.dol and the RELs (which also
# checks main.dol's SHA-1) and the converters that turn the extracted model data into headers.
# The Metrowerks compilers are not downloaded and nothing is compiled. The decomp's tracked
# assets/GZLE01/res (resource index enums, no game data) and the generated
# build/GZLE01/include/assets are then copied to --out. Needs the network the first time (the
# decomp checkout and its dtk binary) and Python 3.10 or newer. Everything stays under build/,
# which git ignores: the generated headers are derived from the disc and must never be committed.
set -euo pipefail

decomp_url="the decompilation fork"
decomp_pin="b09eebc39852e18397031d9a563a8eae124f21f1"
version=GZLE01

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
disc="${COS_DISC:-}"
decomp="$repo/build/decomp"
out="$repo/build/native-mac/assets/$version"

usage() {
    sed -n '2,/^set -euo/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'
    exit 2
}
while [ $# -gt 0 ]; do
    case "$1" in
        --disc) disc="$2"; shift 2 ;;
        --decomp) decomp="$2"; shift 2 ;;
        --out) out="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "gen_assets: unknown option $1" >&2; usage ;;
    esac
done

if [ -z "$disc" ]; then
    echo "gen_assets: no disc image: pass --disc PATH or set COS_DISC (the GZLE01 revision 0 .iso)" >&2
    exit 14
fi
[ -f "$disc" ] || { echo "gen_assets: $disc: no such file" >&2; exit 14; }
case "$disc" in /*) ;; *) disc="$(pwd)/$disc" ;; esac

python=""
for p in python3 python3.13 python3.12 python3.11 python3.10; do
    if command -v "$p" >/dev/null 2>&1 &&
        "$p" -c 'import sys; sys.exit(sys.version_info < (3, 10))' 2>/dev/null; then
        python="$p"
        break
    fi
done
if [ -z "$python" ]; then
    echo "gen_assets: needs Python 3.10 or newer (the decomp's configure.py); e.g. brew install python" >&2
    exit 2
fi
command -v ninja >/dev/null || { echo "gen_assets: needs ninja (brew install ninja)" >&2; exit 2; }

# --- the decompilation, at the pin --------------------------------------------------------------
if [ ! -e "$decomp/.git" ]; then
    echo "gen_assets: fetching the decompilation fork ${decomp_pin:0:7} into $decomp"
    mkdir -p "$decomp"
    git -C "$decomp" init -q
    git -C "$decomp" remote add origin "$decomp_url"
    git -C "$decomp" fetch -q --depth 1 origin "$decomp_pin"
    git -C "$decomp" -c advice.detachedHead=false checkout -q --detach FETCH_HEAD
fi
head="$(git -C "$decomp" rev-parse HEAD)"
if [ "$head" != "$decomp_pin" ]; then
    echo "gen_assets: $decomp is at ${head:0:7}, not ${decomp_pin:0:7}; left unchanged" >&2
    exit 1
fi

# --- the disc, split, headers -------------------------------------------------------------------
# dtk finds the disc image in orig/GZLE01/ (any of the formats it reads, by extension).
orig="$decomp/orig/$version"
find "$orig" -maxdepth 1 -type l -delete
ln -s "$disc" "$orig/$(basename "$disc")"

cd "$decomp"
"$python" configure.py --version "$version" >/dev/null
# The split writes build/GZLE01/config.json; build.ninja then regenerates with the header rules.
ninja "build/$version/config.json"
headers=()
while IFS= read -r t; do headers+=("$t"); done < <(ninja -t targets all |
    sed -n "s|^\(build/$version/include/assets/[^:]*\):.*|\1|p")
[ "${#headers[@]}" -gt 0 ] || { echo "gen_assets: no header targets in build.ninja" >&2; exit 1; }
ninja "${headers[@]}"

# --- copy into the native build ------------------------------------------------------------------
rm -rf "$out"
mkdir -p "$out/include"
cp -R "assets/$version/res" "$out/"
cp -R "build/$version/include/assets" "$out/include/"
echo "gen_assets: $(find "$out" -type f | wc -l | tr -d ' ') headers in $out"
