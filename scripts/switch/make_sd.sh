#!/usr/bin/env bash
# SwitchWaker for players: build the Switch homebrew from your own disc and lay out an SD card folder.
#
#   scripts/switch/make_sd.sh --disc /path/to/GZLE01.iso [--out DIR] [--no-disc] [--deko3d] [--jobs N] [--engine E]
#
# No game files, code or keys come with this repository or its releases. This script does every step on
# your computer, in containers (the host needs bash, git, Python 3 and Docker or Podman):
#   1. checks the disc: GZLE01 revision 0, uncompressed .iso (native/tools/disc_manifest.py --verify);
#   2. builds build/switch-native/switchwaker.nro (scripts/docker/build.sh switch: asset headers from
#      the disc, RecompCore, Aurora, Mesa, Dawn; the first build takes an hour or more, later ones minutes);
#   3. writes OUT/switch/switchwaker/ (default OUT: build/sd) with switchwaker.nro, the bundled
#      initial_pipeline_cache.db and a copy of the disc as GZLE01.iso (--no-disc leaves the disc out,
#      for an update when it is already on the card). Copy OUT's contents to the root of the SD card.
#      --deko3d also builds the experimental deko3d renderer (docs/DEKO3D_MIGRATION_PLAN.md) and writes
#      OUT/switch/switchwaker_dk/ (switchwaker_dk.nro, its initial_pipeline_cache.db and
#      initial_dksh_cache.bin): "SwitchWaker (deko3d)" in the Homebrew Menu, reading the disc, settings
#      and saves of switch/switchwaker/ (no second copy of the disc).
#
# What it makes contains code generated from the disc and the disc itself: for your own console only.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
disc=${COS_DISC:-}
out=$root/build/sd
copy_disc=1
deko3d=0
build_args=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --disc) disc=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --no-disc) copy_disc=0; shift ;;
        --deko3d) deko3d=1; shift ;;
        --jobs|--engine) build_args+=("$1" "$2"); shift 2 ;;
        -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
        *) echo "make_sd: unknown option $1 (--help)" >&2; exit 2 ;;
    esac
done
fail() { echo; echo "make_sd: $*" >&2; exit 1; }
step() { echo; echo "[$1/3] $2"; }

[[ -n $disc ]] || fail "give your disc with --disc /path/to/GZLE01.iso"
[[ -f $disc ]] || fail "no file at $disc"
disc=$(cd "$(dirname "$disc")" && pwd)/$(basename "$disc")
command -v python3 >/dev/null || fail "Python 3 is needed"
command -v docker >/dev/null || command -v podman >/dev/null ||
    fail "Docker or Podman is needed (Docker Desktop on macOS and Windows/WSL)"

step 1 "checking the disc (GZLE01 revision 0, uncompressed .iso)"
if ! python3 -I "$root/native/tools/disc_manifest.py" --verify --disc "$disc"; then
    fail "this is not the disc the port is built for: The Wind Waker (USA), GZLE01 revision 0, as an
uncompressed .iso (not RVZ, GCZ, CISO or NKit; Dolphin can convert: right-click > Convert File)."
fi

step 2 "building the Switch homebrew (an hour or more the first time, minutes later)"
"$root/scripts/docker/build.sh" switch --disc "$disc" ${build_args[@]+"${build_args[@]}"} || fail "the build failed (see above)"
nro=$root/build/switch-native/switchwaker.nro
db=$root/build/switch-native/initial_pipeline_cache.db
[[ -f $nro ]] || fail "the build made no $nro"
[[ -f $db ]] || db=$root/native/data/initial_pipeline_cache.db

step 3 "the SD card folder"
dst=$out/switch/switchwaker
mkdir -p "$dst"
cp "$nro" "$dst/switchwaker.nro"
cp "$db" "$dst/initial_pipeline_cache.db"
if [[ $copy_disc == 1 ]]; then
    echo "  copying the disc ($(du -h "$disc" | cut -f1))"
    cp "$disc" "$dst/GZLE01.iso"
fi
if [[ $deko3d == 1 ]]; then
    echo "  the deko3d renderer (switch/switchwaker_dk/)"
    "$root/scripts/docker/build.sh" switch --renderer deko3d --disc "$disc" ${build_args[@]+"${build_args[@]}"} ||
        fail "the deko3d build failed (see above)"
    dk=$root/build/switch-native-dk
    mkdir -p "$out/switch/switchwaker_dk"
    for f in switchwaker_dk.nro initial_pipeline_cache.db initial_dksh_cache.bin; do
        [[ -f $dk/$f ]] || fail "the deko3d build made no $dk/$f"
        cp "$dk/$f" "$out/switch/switchwaker_dk/$f"
    done
fi
cat <<EOF

Done: $out

Copy the contents of that folder to the root of the SD card (switch/switchwaker/ ends up at
sdmc:/switch/switchwaker/). Saves and settings already on the card are kept. Then hold R while
starting any installed game to open the Homebrew Menu in title mode and pick SwitchWaker. The first
start prepares the shaders once (about 8 to 10 minutes; do not close the game meanwhile).

These files contain the game: they are for your own console only; do not share them.
EOF
