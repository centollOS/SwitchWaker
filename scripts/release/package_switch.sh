#!/usr/bin/env bash
# Package the Switch download of a SwitchWaker release (docs/RUNTIME_ASSETS.md, "Player distribution"):
# the NRO built WITHOUT the disc (scripts/switch/build_native.sh --runtime-assets, at the tag) and its caches, laid out
# as the SD card wants them, checked by scripts/release/guard_nro.py (none of the disc's arrays inside).
#
#   scripts/release/package_switch.sh TAG
#     -> build/release/SwitchWaker-TAG-switch.zip and .sha256:
#        switch/switchwaker/switchwaker.nro, initial_pipeline_cache.db, initial_dksh_cache.bin
#
# Players add their own disc as switch/switchwaker/GZLE01.iso (INSTALL.md). Build first, at the tag:
#   git checkout TAG && scripts/switch/build_native.sh --runtime-assets
# The NRO's version (the Homebrew Menu's, git describe at build time) must be TAG. Nothing is uploaded here.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
tag=${1:?usage: package_switch.sh TAG}
fail() { echo "package_switch: $*" >&2; exit 1; }

dk=build/switch-native
files=("$dk/switchwaker.nro" "$dk/initial_pipeline_cache.db" "$dk/initial_dksh_cache.bin")
for f in "${files[@]}"; do [[ -s $f ]] || fail "$f is missing: build it at $tag with --runtime-assets"; done

# built without the disc's headers, and from this tag
nros=("$dk/switchwaker.nro")
decomp=build/decomp
[[ -d $decomp/build/GZLE01/bin/assets ]] || decomp=$(git rev-parse --path-format=absolute --git-common-dir)/../build/decomp
python3 -I scripts/release/guard_nro.py --decomp "$decomp" "${nros[@]}" || fail "an NRO holds the disc's data"
version=${tag#v}
for n in "${nros[@]}"; do
    LC_ALL=C grep -aq -- "$version" "$n" || fail "$n does not carry version $version (built from another commit?)"
done

name=SwitchWaker-$tag-switch
stage=build/release/$name
rm -rf "$stage" "build/release/$name.zip"
mkdir -p "$stage/switch/switchwaker"
cp "$dk/switchwaker.nro" "$dk/initial_pipeline_cache.db" "$dk/initial_dksh_cache.bin" "$stage/switch/switchwaker/"
(cd "$stage" && zip -qr "../$name.zip" switch)
rm -rf "$stage"
(cd build/release && shasum -a 256 "$name.zip" > "$name.zip.sha256" && cat "$name.zip.sha256")
unzip -l "build/release/$name.zip"
