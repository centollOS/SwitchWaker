#!/usr/bin/env bash
# Package the Switch downloads of a SwitchWaker release (docs/RUNTIME_ASSETS.md, "Player distribution"):
# the NROs built WITHOUT the disc (scripts/switch/build_native.sh --runtime-assets, at the tag) and their caches,
# laid out as the SD card wants them, checked by scripts/release/guard_nro.py (none of the disc's arrays inside).
#
#   scripts/release/package_switch.sh TAG [--with-eur]
#     -> build/release/SwitchWaker-TAG-switch.zip and .sha256, for the USA disc ("SwitchWaker" in hbmenu):
#          switch/switchwaker/switchwaker.nro, initial_pipeline_cache.db, initial_dksh_cache.bin,
#          LICENSE.txt and THIRD_PARTY.md (the cache holds third-party shaders: THIRD_PARTY.md)
#     --with-eur: also build/release/SwitchWaker-TAG-switch-eur.zip and .sha256, for the European disc
#        ("SwitchWaker EUR", docs/PAL_DISC_PLAN.md), the same layout:
#          switch/switchwaker/switchwaker.nro (build/switch-native-eur/switchwaker-eur.nro, renamed),
#          initial_pipeline_cache.db, initial_dksh_cache.bin, LICENSE.txt, THIRD_PARTY.md
#
# The in-game update (native/src/pc/features/pc_update.cpp, issues #27/#28) finds these zips on the GitHub
# release by name (SwitchWaker-<tag>-switch.zip / -switch-eur.zip with their .sha256) and installs every
# file under switch/switchwaker/ in them: keep the names, the .sha256 and the layout.
#
# One region per SD card: both zips install switch/switchwaker/switchwaker.nro, the path the HOME-menu
# forwarder starts, so they conflict on purpose (INSTALL.md: to change region, the other zip's
# switchwaker.nro and the matching disc image). The caches are the same files in both.
#
# Players add their own disc as switch/switchwaker/GZLE01.iso, or GZLP01.iso for the European one
# (INSTALL.md). Build first, at the tag:
#   git checkout TAG && scripts/switch/build_native.sh --runtime-assets
#   (--with-eur: scripts/switch/build_native.sh --eur --runtime-assets as well)
# Each NRO's version (the Homebrew Menu's, git describe at build time) must be TAG. Nothing is uploaded here.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
tag=${1:?usage: package_switch.sh TAG [--with-eur]}
with_eur=0
[[ ${2:-} == --with-eur ]] && with_eur=1
fail() { echo "package_switch: $*" >&2; exit 1; }

caches=(initial_pipeline_cache.db initial_dksh_cache.bin)
usa_dir=build/switch-native
eur_dir=build/switch-native-eur
for f in switchwaker.nro "${caches[@]}"; do
    [[ -s $usa_dir/$f ]] || fail "$usa_dir/$f is missing: build it at $tag with --runtime-assets"
done
if [[ $with_eur == 1 ]]; then
    for f in switchwaker-eur.nro "${caches[@]}"; do
        [[ -s $eur_dir/$f ]] || fail "$eur_dir/$f is missing: build it at $tag with --eur --runtime-assets"
    done
    # the caches are built from the same pipeline database: both downloads must carry the same files
    for f in "${caches[@]}"; do
        cmp -s "$usa_dir/$f" "$eur_dir/$f" ||
            fail "$usa_dir/$f and $eur_dir/$f differ: both downloads must carry the same caches"
    done
fi

# built without the disc's headers, and from this tag
decomp_for() { # GAME_ID: the decomp with that disc's split assets (this checkout's, else the main checkout's)
    local d=build/decomp
    [[ -d $d/build/$1/bin/assets ]] || d=$(git rev-parse --path-format=absolute --git-common-dir)/../build/decomp
    echo "$d"
}
python3 -I scripts/release/guard_nro.py --decomp "$(decomp_for GZLE01)" "$usa_dir/switchwaker.nro" ||
    fail "an NRO holds the disc's data"
nros=("$usa_dir/switchwaker.nro")
if [[ $with_eur == 1 ]]; then
    python3 -I scripts/release/guard_nro.py --version GZLP01 --decomp "$(decomp_for GZLP01)" "$eur_dir/switchwaker-eur.nro" ||
        fail "the European NRO holds the disc's data"
    nros+=("$eur_dir/switchwaker-eur.nro")
fi
version=${tag#v}
for n in "${nros[@]}"; do
    # the whole NUL-terminated string (the NACP's display version, the debug server's): "0.3.1" is not "0.3.1-52"
    python3 -I -c 'import sys; sys.exit(b"\0" + sys.argv[2].encode() + b"\0" not in open(sys.argv[1], "rb").read())' "$n" "$version" ||
        fail "$n does not carry version $version (built from another commit?)"
done

# zip NAME BUILD_DIR NRO: the NRO as switch/switchwaker/switchwaker.nro, the caches and the license files next to it
package() {
    local name=$1 dir=$2 nro=$3 stage
    stage=build/release/$name
    rm -rf "$stage" "build/release/$name.zip" "build/release/$name.zip.sha256"
    mkdir -p "$stage/switch/switchwaker"
    cp "$dir/$nro" "$stage/switch/switchwaker/switchwaker.nro"
    for f in "${caches[@]}"; do cp "$dir/$f" "$stage/switch/switchwaker/"; done
    cp LICENSE "$stage/switch/switchwaker/LICENSE.txt"
    cp THIRD_PARTY.md "$stage/switch/switchwaker/"
    (cd "$stage" && zip -qr "../$name.zip" switch)
    rm -rf "$stage"
    (cd build/release && shasum -a 256 "$name.zip" > "$name.zip.sha256" && cat "$name.zip.sha256")
    unzip -l "build/release/$name.zip"
}
mkdir -p build/release
package "SwitchWaker-$tag-switch" "$usa_dir" switchwaker.nro
[[ $with_eur == 0 ]] || package "SwitchWaker-$tag-switch-eur" "$eur_dir" switchwaker-eur.nro
