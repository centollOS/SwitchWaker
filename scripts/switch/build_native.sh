#!/usr/bin/env bash
# Build the native port (native/: the decompiled game on Aurora) as a Switch NRO.
#
#   scripts/switch/build_native.sh [--renderer gl|deko3d] [--dk-debug-lib] [--aurora DIR]
#                                  [--assets DIR] [--recompcore DIR] [--dawn-src DIR]
#                                  [--mesa DIR | --stock-mesa] [--jobs N] [--target TARGET]
#
# Output: build/switch-native/switchwaker.nro, next to it the bundled pipeline cache
# initial_pipeline_cache.db (a copy of native/data/'s), and, for addr2line,
# build/switch-native/switchwaker.elf.
#
# --renderer deko3d (docs/DEKO3D_MIGRATION_PLAN.md, phase 2 on): the deko3d NRO instead,
# build/switch-native-dk/switchwaker_dk.nro (+ .elf), "SwitchWaker (deko3d)" in the Homebrew Menu,
# for sdmc:/switch/switchwaker_dk/; next to it the same initial_pipeline_cache.db and
# initial_dksh_cache.bin, the DKSH of every pipeline of that database, built first by
# native/tools/dksh_cache (its own Debian container; build/dksh/ keeps its report and work files)
# and checked (`dksh_cache check`). The container builds in /work/build/switch-native either way:
# build/switch-native-dk is mounted there, so a copy of a GL build tree (cp -cR on APFS) is a
# valid head start. --dk-debug-lib links libdeko3dd (every deko3d call checked; "SwitchWaker
# (deko3d debug)"). The GL NRO (the default, --renderer gl) is unchanged.
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

aurora="" assets="" recompcore="" dawn_src="" mesa="" stock_mesa=0 target=cos_nro renderer=gl dk_debug=OFF
jobs=${SWITCH_BUILD_JOBS:-4}
while [[ $# -gt 0 ]]; do
    case $1 in
        --renderer) renderer=$2; shift 2 ;;
        --dk-debug-lib) dk_debug=ON; shift ;;
        --aurora) aurora=$2; shift 2 ;;
        --assets) assets=$2; shift 2 ;;
        --recompcore) recompcore=$2; shift 2 ;;
        --dawn-src) dawn_src=$2; shift 2 ;;
        --mesa) mesa=$2; shift 2 ;;
        --stock-mesa) stock_mesa=1; shift ;;
        --jobs) jobs=$2; shift 2 ;;
        --target) target=$2; shift 2 ;;
        -h|--help) sed -n '2,46p' "$0"; exit 0 ;;
        *) echo "build_native: unknown option $1" >&2; exit 2 ;;
    esac
done

case $renderer in
    gl) build_dir=build/switch-native nro_name=switchwaker ;;
    deko3d) build_dir=build/switch-native-dk nro_name=switchwaker_dk ;;
    *) echo "build_native: --renderer is gl or deko3d, not $renderer" >&2; exit 2 ;;
esac

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
if [[ $renderer == deko3d ]]; then
    # the deko3d tree at the container path of the GL one (see the header)
    mkdir -p "$root/$build_dir"
    mounts+=(-v "$root/$build_dir:/work/build/switch-native")
fi
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
# The NRO's display version (NACP, at most 15 bytes), as the forwarder's (build_forwarder.sh): git
# describe without the "v", without the hash when that is too long.
describe=$(git -C "$root" describe --tags --always --dirty 2>/dev/null || echo 0)
version=${describe#v}
if [[ ${#version} -gt 15 ]]; then
    version=${version%-g*}
    version=${version:0:15}
fi

label=$renderer
[[ $renderer == deko3d && $dk_debug == ON ]] && label="deko3d (libdeko3dd)"
echo "build_native: renderer=$label version=$version aurora=$aurora assets=$assets recompcore=$recompcore dawn-src=${dawn_src:-fetch} mesa=${mesa:-devkitPro switch-mesa}"

container_run "$engine" "$root" "${mounts[@]}" -e JOBS="$jobs" -e TARGET="$target" \
    -e DAWN_FLAG="$dawn_flag" -e MESA_FLAG="$mesa_flag" -e VERSION="$version" \
    -e RENDERER="$renderer" -e NRO_NAME="$nro_name" -e DK_DEBUG="$dk_debug" "$image" bash -lc '
set -euo pipefail
export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:$PATH
cmake -S /work/switch/native -B /work/build/switch-native -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DDKP_USE_DOUBLE_OBJECT_FILE_EXTENSIONS=ON -DCOS_SWITCH_RENDERER="$RENDERER" \
    -DCOS_SWITCH_NRO_NAME="$NRO_NAME" -DCOS_DK_DEBUG_LIB="$DK_DEBUG" -DCOS_SWITCH_VERSION="$VERSION" \
    -DCOS_SWITCH_AURORA_SOURCE=/inputs/aurora -DCOS_ASSETS_DIR=/inputs/assets \
    -DCOS_RECOMPCORE_DIR=/inputs/recompcore $DAWN_FLAG $MESA_FLAG >/dev/null
cmake --build /work/build/switch-native --target "$TARGET" --parallel "$JOBS"
'

[[ $target == cos_nro ]] || exit 0
out="$root/$build_dir"
nro="$out/$nro_name.nro"
if [[ ! -s $nro ]] || [[ $(od -An -tc -j 16 -N4 "$nro" | tr -d ' \n') != NRO0 ]]; then
    echo "build_native: no valid NRO at $nro" >&2
    exit 1
fi
# The bundled pipeline cache goes next to the NRO (sdmc:/switch/switchwaker/ on the console): without
# it there is no warm-up and no "Preparing shaders" screen (docs/SWITCH_BUILD.md).
cp -f "$root/native/data/initial_pipeline_cache.db" "$out/initial_pipeline_cache.db"
if [[ $renderer == deko3d ]]; then
    # The DKSH of every pipeline of that database (docs/DEKO3D_MIGRATION_PLAN.md section 6): generated,
    # never committed; the NRO loads it at start ("[dk] shader cache:"). check reads it back as the
    # NRO's loader does and prints the same summary.
    mkdir -p "$root/build/dksh"
    "$root/native/tools/dksh_cache/build.sh" --jobs "$jobs" build native/data/initial_pipeline_cache.db \
        build/dksh/initial_dksh_cache.bin >"$root/build/dksh/build.log" 2>&1 || {
        tail -n 40 "$root/build/dksh/build.log" >&2
        echo "build_native: native/tools/dksh_cache failed (build/dksh/build.log)" >&2
        exit 1
    }
    grep -E "^(GX configs|uam:|modules with|fixed shaders|DKSH:|read back)" "$root/build/dksh/initial_dksh_cache.bin.report.txt" || true
    "$root/native/tools/dksh_cache/build.sh" check build/dksh/initial_dksh_cache.bin
    cp -f "$root/build/dksh/initial_dksh_cache.bin" "$out/initial_dksh_cache.bin"
fi
shasum -a 256 "$nro" 2>/dev/null || sha256sum "$nro"
printf 'Built %s (%s bytes); symbols: %s\n' "$nro" "$(wc -c <"$nro" | tr -d ' ')" "$out/$nro_name.elf"
if [[ $renderer == deko3d ]]; then
    printf 'For sdmc:/switch/switchwaker_dk/: %s, %s, %s (the disc and native/ stay in sdmc:/switch/switchwaker/)\n' \
        "$nro_name.nro" initial_pipeline_cache.db initial_dksh_cache.bin
fi
