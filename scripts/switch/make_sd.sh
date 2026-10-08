#!/usr/bin/env bash
# SwitchWaker for players: build the Switch homebrew from your own disc and lay out an SD card folder.
#
#   scripts/switch/make_sd.sh --disc /path/to/GZLE01.iso [--out DIR] [--no-disc] [--gl] [--jobs N] [--engine E]
#
# No game files, code or keys come with this repository or its releases. This script does every step on
# your computer, in containers (the host needs bash, git, Python 3 and Docker or Podman):
#   1. checks the disc: GZLE01 revision 0, uncompressed .iso (native/tools/disc_manifest.py --verify);
#   2. builds build/switch-native/switchwaker.nro (scripts/docker/build.sh switch: asset headers from
#      the disc, RecompCore, Aurora, Dawn, the deko3d renderer and its shader cache; the first build
#      takes an hour or more, later ones minutes);
#   3. writes OUT/switch/switchwaker/ (default OUT: build/sd) with switchwaker.nro, the bundled
#      initial_pipeline_cache.db and initial_dksh_cache.bin, and a copy of the disc as GZLE01.iso
#      (--no-disc leaves the disc out, for an update when it is already on the card). Copy OUT's
#      contents to the root of the SD card.
#      --gl also builds the OpenGL ES renderer (Mesa; the renderer before deko3d) and writes
#      OUT/switch/switchwaker_gl/ (switchwaker_gl.nro and its initial_pipeline_cache.db): "SwitchWaker
#      (GL)" in the Homebrew Menu, reading the disc, settings and saves of switch/switchwaker/ (no second
#      copy of the disc).
#
# What it makes contains code generated from the disc and the disc itself: for your own console only.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
disc=${COS_DISC:-}
out=$root/build/sd
copy_disc=1
gl=0
build_args=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --disc) disc=$2; shift 2 ;;
        --out) out=$2; shift 2 ;;
        --no-disc) copy_disc=0; shift ;;
        --gl) gl=1; shift ;;
        --deko3d) echo "make_sd: --deko3d is the default now (switchwaker.nro); --gl adds the GL NRO" >&2; shift ;;
        --jobs|--engine) build_args+=("$1" "$2"); shift 2 ;;
        -h|--help) sed -n '2,19p' "$0"; exit 0 ;;
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
built=$root/build/switch-native

step 3 "the SD card folder"
dst=$out/switch/switchwaker
mkdir -p "$dst"
for f in switchwaker.nro initial_pipeline_cache.db initial_dksh_cache.bin; do
    [[ -f $built/$f ]] || fail "the build made no $built/$f"
    cp "$built/$f" "$dst/$f"
done
if [[ $copy_disc == 1 ]]; then
    echo "  copying the disc ($(du -h "$disc" | cut -f1))"
    cp "$disc" "$dst/GZLE01.iso"
fi
if [[ $gl == 1 ]]; then
    echo "  the GL renderer (switch/switchwaker_gl/)"
    "$root/scripts/docker/build.sh" switch --renderer gl --disc "$disc" ${build_args[@]+"${build_args[@]}"} ||
        fail "the GL build failed (see above)"
    glb=$root/build/switch-native-gl
    mkdir -p "$out/switch/switchwaker_gl"
    for f in switchwaker_gl.nro initial_pipeline_cache.db; do
        [[ -f $glb/$f ]] || fail "the GL build made no $glb/$f"
        cp "$glb/$f" "$out/switch/switchwaker_gl/$f"
    done
fi
cat <<EOF

Done: $out

Copy the contents of that folder to the root of the SD card (switch/switchwaker/ ends up at
sdmc:/switch/switchwaker/). Saves and settings already on the card are kept. Then hold R while
starting any installed game to open the Homebrew Menu in title mode and pick SwitchWaker. The
shaders come prebuilt (initial_dksh_cache.bin): there is no shader preparation at the first start.

These files contain the game: they are for your own console only; do not share them.
EOF
