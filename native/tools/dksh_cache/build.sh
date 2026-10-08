#!/usr/bin/env bash
# Builds native/tools/dksh_cache in a Debian container (Containerfile here; image
# localhost/centollos-dksh-cache:<hash of it>) into build/dksh_cache, then runs it there with the
# given arguments, if any. Paths given to the tool are relative to the repository root (mounted at
# /work). Examples:
#
#   native/tools/dksh_cache/build.sh build native/data/initial_pipeline_cache.db build/dksh/initial_dksh_cache.bin
#   native/tools/dksh_cache/build.sh dump build/dksh/initial_dksh_cache.bin build/dksh/dump
#
# `build` without --fixed also gets the fixed shaders: fixed_wgsl.py extracts their WGSL from the
# patched Aurora copy (and the Switch build's ImGui) into build/dksh_cache/fixed-wgsl first.
#
# Inputs (each looked up in this checkout's build/, then in the main checkout's when this is a git
# worktree, as scripts/switch/build_native.sh does):
#   build/aurora-3227d76                      Aurora at native/'s pin            (--aurora DIR)
#   build/switch-dawn-probe/_deps/dawn-src    the Switch build's patched Dawn     (--dawn-src DIR)
#   build/switch-native/_deps/{fmt,xxhash}-src  Aurora's fmt and xxHash (optional: else downloaded)
#   build/switch-native/_deps/imgui-src       ImGui's WebGPU shaders (optional: else not built)
# Options before the tool's arguments: --jobs N (compile jobs, default: the engine's CPUs).
# Outputs stay in build/: they derive from the committed pipeline list and are never committed.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
source "$root/scripts/switch/container.sh"

aurora="" dawn_src="" jobs=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --aurora) aurora=$2; shift 2 ;;
        --dawn-src) dawn_src=$2; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        *) break ;;
    esac
done

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
dawn_src=${dawn_src:-$(first_existing "$root/build/switch-dawn-probe/_deps/dawn-src" \
                                      "$main_root/build/switch-dawn-probe/_deps/dawn-src" || true)}
fmt_src=$(first_existing "$root/build/switch-native/_deps/fmt-src" "$main_root/build/switch-native/_deps/fmt-src" || true)
xxhash_src=$(first_existing "$root/build/switch-native/_deps/xxhash-src" \
                            "$main_root/build/switch-native/_deps/xxhash-src" || true)
imgui_src=$(first_existing "$root/build/switch-native/_deps/imgui-src" \
                           "$main_root/build/switch-native/_deps/imgui-src" || true)
if [[ ! -f $aurora/lib/gx/shader.cpp ]]; then
    echo "dksh_cache: no Aurora checkout at the pin (--aurora); see native/README.md, \"Aurora\"" >&2
    exit 1
fi
if [[ ! -f $dawn_src/src/tint/lang/glsl/writer/writer.h ]]; then
    echo "dksh_cache: no Dawn source (--dawn-src); the Switch build fetches it (scripts/switch/build_native.sh)" >&2
    exit 1
fi

engine=$(container_engine)
if [[ -z $engine ]]; then
    echo "dksh_cache: Podman or Docker is required" >&2
    exit 1
fi
containerfile=$root/native/tools/dksh_cache/Containerfile
if command -v sha256sum >/dev/null 2>&1; then
    tag=$(sha256sum "$containerfile" | cut -c1-12)
else
    tag=$(shasum -a 256 "$containerfile" | cut -c1-12)
fi
image=localhost/centollos-dksh-cache:$tag
if ! container_image_exists "$engine" "$image"; then
    "$engine" build --tag "$image" --file "$containerfile" "$root/native/tools/dksh_cache"
fi
jobs=${jobs:-$("$engine" run --rm "$image" nproc)}

# GIT_CEILING_DIRECTORIES: git inside the build tree (Dawn's CMake, Aurora's patch step) must not reach
# this checkout's .git, which in a worktree points outside the mounted tree
mounts=(-e GIT_CEILING_DIRECTORIES=/work/build -v "$aurora:/inputs/aurora:ro" -v "$dawn_src:/inputs/dawn-src:ro")
imgui_arg=-
if [[ -n $imgui_src ]]; then
    mounts+=(-v "$imgui_src:/inputs/imgui:ro")
    imgui_arg=/inputs/imgui
fi
defs=(-DCOS_DKSH_AURORA_SRC=/inputs/aurora -DCOS_DKSH_DAWN_SRC=/inputs/dawn-src)
if [[ -n $fmt_src ]]; then
    mounts+=(-v "$fmt_src:/inputs/fmt:ro")
    defs+=(-DFETCHCONTENT_SOURCE_DIR_FMT=/inputs/fmt)
fi
if [[ -n $xxhash_src ]]; then
    mounts+=(-v "$xxhash_src:/inputs/xxhash:ro")
    defs+=(-DFETCHCONTENT_SOURCE_DIR_XXHASH=/inputs/xxhash)
fi

container_run "$engine" "$root" "${mounts[@]}" "$image" bash -c '
    set -e
    jobs=$1; shift
    imgui=$1; shift
    ndefs=$1; shift
    defs=("${@:1:$ndefs}"); shift "$ndefs"
    if ! cmake -S native/tools/dksh_cache -B build/dksh_cache -G Ninja -DCMAKE_BUILD_TYPE=Release "${defs[@]}" \
            >build/dksh_cache.configure.log 2>&1; then
        tail -n 40 build/dksh_cache.configure.log >&2
        exit 1
    fi
    if ! ninja -C build/dksh_cache -j "$jobs" dksh_cache >build/dksh_cache/ninja.log 2>&1; then
        tail -n 80 build/dksh_cache/ninja.log >&2
        exit 1
    fi
    if [[ $# -gt 0 ]]; then
        if [[ $1 == build && " $* " != *" --jobs "* ]]; then set -- "$@" --jobs "$jobs"; fi
        if [[ $1 == build && " $* " != *" --fixed "* ]]; then
            rm -rf build/dksh_cache/fixed-wgsl
            python3 native/tools/dksh_cache/fixed_wgsl.py build/dksh_cache/aurora "$imgui" build/dksh_cache/fixed-wgsl
            set -- "$@" --fixed build/dksh_cache/fixed-wgsl
        fi
        exec build/dksh_cache/dksh_cache "$@"
    fi' bash "$jobs" "$imgui_arg" "${#defs[@]}" "${defs[@]}" "$@"
