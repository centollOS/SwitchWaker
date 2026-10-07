#!/usr/bin/env bash
# Package a SwitchWaker release: the source of a git tag (or commit) as a zip, with INSTALL.md at its top,
# checked by scripts/release/guard.py (nothing from the disc, no build of the game, no keys). There is no
# NRO in it: every build carries code generated from the disc, so players build their own
# (scripts/switch/make_sd.sh).
#   scripts/release/package_release.sh v0.1.3    -> build/release/SwitchWaker-v0.1.3.zip and .sha256
set -euo pipefail
cd "$(dirname "$0")/../.."
ref=${1:?usage: package_release.sh TAG_OR_COMMIT}
name=SwitchWaker-${ref}
out=build/release
mkdir -p "$out"
rm -f "$out/$name.zip"
git archive --format=zip --prefix="$name/" -o "$out/$name.zip" "$ref"
python3 -I scripts/release/guard.py "$out/$name.zip"
(cd "$out" && shasum -a 256 "$name.zip" > "$name.zip.sha256" && cat "$name.zip.sha256")
