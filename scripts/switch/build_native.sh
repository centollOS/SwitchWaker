#!/usr/bin/env bash
# Build the native port (native/: the decompiled game on Aurora) as a Switch NRO.
#
#   scripts/switch/build_native.sh [--aurora DIR] [--assets DIR] [--recompcore DIR]
#                                  [--dawn-src DIR] [--mesa DIR | --stock-mesa] [--jobs N]
#                                  [--target TARGET]
#
# Output: build/switch-native/switchwaker.nro, next to it the bundled pipeline cache
# initial_pipeline_cache.db (a copy of native/data/'s), and, for addr2line,
# build/switch-native/switchwaker.elf.
# Everything is compiled in a container (Podman or Docker, see container.sh) from the pinned
# devkitPro image plus clang 19 (Containerfile.native): devkitA64's GCC for Aurora, Dawn, the SDK
# and libnx, clang for the game units (switch/native/clang-launcher.sh).
#
# Inputs, outside git (defaults: this checkout's, else the main checkout's when this is a git
# worktree such as build/lanes/<lane>):
#   --aurora DIR      Aurora at native/'s pin 3227d76 (build/aurora-3227d76:
#                     git -C ref/aurora worktree add --detach build/aurora-3227d76 3227d76)
#   --assets DIR      the asset headers generated from the player's disc, as for the Mac build
#                     (build/native-mac/assets/GZLE01; native/README.md, "Asset headers")
#   --recompcore DIR  RecompCore (ref/recompcore), for Dolphin's DSP HLE (native/tools/fetch_recompcore.sh)
#   --dawn-src DIR    an already fetched Dawn source tree to reuse instead of downloading it
#                     (default: build/switch-dawn-probe/_deps/dawn-src if a Dawn source was
#                     fetched there before; the Horizon patches are applied to it if missing)
#   --mesa DIR        the Mesa prefix to link (default: build/switch-mesa/prefix of this checkout,
#                     else of the main checkout; built first with scripts/switch/build_mesa.sh
#                     when neither exists): devkitPro's switch-mesa recipe plus switch/mesa/patches
#                     (the persistent shader cache, docs/SWITCH_BUILD.md "Shader cache")
#   --stock-mesa      link devkitPro's switch-mesa package from the image instead (no shader cache)
#   --jobs N          parallel compile jobs in the container (default: SWITCH_BUILD_JOBS or 4)
#   --target TARGET   build this CMake target instead of the NRO (cos_nro)
#
# The first build fetches Dawn's dependencies, SDL 3's headers, ImGui, Tracy, fmt, xxhash and
# sqlite into build/switch-native and compiles Dawn: allow an hour or more. Later builds reuse it.
# The NRO contains code built from headers generated from the player's disc: for their own
# console only (docs/SWITCH_BUILD.md).
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source "$root/scripts/switch/container.sh"
image=${COS_SWITCH_NATIVE_IMAGE:-localhost/centollos-switch-native-build:2026-10-03}

aurora="" assets="" recompcore="" dawn_src="" mesa="" stock_mesa=0 target=cos_nro
jobs=${SWITCH_BUILD_JOBS:-4}
while [[ $# -gt 0 ]]; do
    case $1 in
        --aurora) aurora=$2; shift 2 ;;
        --assets) assets=$2; shift 2 ;;
        --recompcore) recompcore=$2; shift 2 ;;
        --dawn-src) dawn_src=$2; shift 2 ;;
        --mesa) mesa=$2; shift 2 ;;
        --stock-mesa) stock_mesa=1; shift ;;
        --jobs) jobs=$2; shift 2 ;;
        --target) target=$2; shift 2 ;;
        -h|--help) sed -n '2,36p' "$0"; exit 0 ;;
        *) echo "build_native: unknown option $1" >&2; exit 2 ;;
    esac
done

# The main checkout, when this is a git worktree (its build/ and ref/ hold the shared inputs).
main_root=$root
if common=$(git -C "$root" rev-parse --path-format=absolute --git-common-dir 2>/dev/null); then
    main_root=$(cd "$common/.." && pwd)
fi
first_existing() {
    local path
    for path in "$@"; do
        if [[ -e $path ]]; then
            (cd "$path" && pwd)
            return 0
        fi
    done
    return 1
}
aurora=${aurora:-$(first_existing "$root/build/aurora-3227d76" "$main_root/build/aurora-3227d76" || true)}
assets=${assets:-$(first_existing "$root/build/native-mac/assets/GZLE01" \
                                  "$main_root/build/native-mac/assets/GZLE01" || true)}
recompcore=${recompcore:-$(first_existing "$root/ref/recompcore" "$main_root/ref/recompcore" || true)}
dawn_src=${dawn_src:-$(first_existing "$root/build/switch-dawn-probe/_deps/dawn-src" \
                                      "$main_root/build/switch-dawn-probe/_deps/dawn-src" || true)}

if [[ $stock_mesa == 0 && -z $mesa ]]; then
    mesa=$(first_existing "$root/build/switch-mesa/prefix" "$main_root/build/switch-mesa/prefix" || true)
    if [[ -z $mesa ]]; then
        "$root/scripts/switch/build_mesa.sh" --jobs "$jobs"
        mesa=$root/build/switch-mesa/prefix
    fi
fi

if [[ ! -f $aurora/cmake/aurora_core.cmake ]]; then
    echo "build_native: no Aurora checkout at the pin (--aurora); see native/README.md, \"Aurora\"" >&2
    exit 1
fi
if [[ ! -d $assets/include/assets ]]; then
    echo "build_native: no generated asset headers (--assets); see native/README.md, \"Asset headers\"" >&2
    exit 1
fi
if [[ ! -f $recompcore/Source/Core/Core/HW/DSPHLE/UCodes/UCodes.cpp ]]; then
    echo "build_native: no RecompCore checkout (--recompcore); run native/tools/fetch_recompcore.sh" >&2
    exit 1
fi
aurora=$(cd "$aurora" && pwd)
assets=$(cd "$assets" && pwd)
recompcore=$(cd "$recompcore" && pwd)

engine=$(container_engine)
if [[ -z $engine ]]; then
    echo "build_native: Podman or Docker is required" >&2
    exit 1
fi
if ! container_image_exists "$engine" "$image"; then
    "$engine" build --tag "$image" --file "$root/scripts/switch/Containerfile.native" "$root/scripts/switch"
fi

mounts=(-v "$aurora:/inputs/aurora:ro" -v "$assets:/inputs/assets:ro" -v "$recompcore:/inputs/recompcore:ro")
mesa_flag=-DCOS_SWITCH_MESA_DIR=
if [[ $stock_mesa == 0 ]]; then
    if [[ ! -f $mesa/lib/libEGL.a ]]; then
        echo "build_native: no Mesa at $mesa (scripts/switch/build_mesa.sh, or --stock-mesa)" >&2
        exit 1
    fi
    mesa=$(cd "$mesa" && pwd)
    mounts+=(-v "$mesa:/inputs/mesa:ro")
    mesa_flag=-DCOS_SWITCH_MESA_DIR=/inputs/mesa
fi
dawn_flag=""
if [[ -n $dawn_src && -f $dawn_src/CMakeLists.txt ]]; then
    mounts+=(-v "$(cd "$dawn_src" && pwd):/inputs/dawn-src")
    dawn_flag=-DFETCHCONTENT_SOURCE_DIR_DAWN=/inputs/dawn-src
fi
echo "build_native: aurora=$aurora assets=$assets recompcore=$recompcore dawn-src=${dawn_src:-fetch} mesa=${mesa:-devkitPro switch-mesa}"

container_run "$engine" "$root" "${mounts[@]}" -e JOBS="$jobs" -e TARGET="$target" \
    -e DAWN_FLAG="$dawn_flag" -e MESA_FLAG="$mesa_flag" "$image" bash -lc '
set -euo pipefail
export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH
cmake -S /work/switch/native -B /work/build/switch-native -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DDKP_USE_DOUBLE_OBJECT_FILE_EXTENSIONS=ON -DCOS_SWITCH_NRO_NAME=switchwaker \
    -DCOS_SWITCH_AURORA_SOURCE=/inputs/aurora -DCOS_ASSETS_DIR=/inputs/assets \
    -DCOS_RECOMPCORE_DIR=/inputs/recompcore $DAWN_FLAG $MESA_FLAG >/dev/null
cmake --build /work/build/switch-native --target "$TARGET" --parallel "$JOBS"
'

[[ $target == cos_nro ]] || exit 0
nro="$root/build/switch-native/switchwaker.nro"
if [[ ! -s $nro ]] || [[ $(od -An -tc -j 16 -N4 "$nro" | tr -d ' \n') != NRO0 ]]; then
    echo "build_native: no valid NRO at $nro" >&2
    exit 1
fi
shasum -a 256 "$nro" 2>/dev/null || sha256sum "$nro"
# The bundled pipeline cache goes next to the NRO (sdmc:/switch/switchwaker/ on the console): without
# it there is no warm-up and no "Preparing shaders" screen (docs/SWITCH_BUILD.md).
cp -f "$root/native/data/initial_pipeline_cache.db" "$root/build/switch-native/initial_pipeline_cache.db"
printf 'Built %s (%s bytes); symbols: %s\n' "$nro" "$(wc -c <"$nro" | tr -d ' ')" \
    "$root/build/switch-native/switchwaker.elf"
