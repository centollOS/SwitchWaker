#!/usr/bin/env bash
# Build SwitchWaker in containers on any host with Docker or Podman (Linux, macOS, Windows through
# WSL 2 with Docker Desktop or Podman):
#
#   scripts/docker/build.sh [linux|switch|all] --disc /path/to/GZLE01.iso [options]
#
#   linux    build/native-linux/switchwaker: the native Linux binary (Vulkan, OpenGL fallback),
#            for the container's architecture (the host's: x86_64 or aarch64; see --platform)
#   switch   build/switch-native/switchwaker.nro: the Switch homebrew (scripts/switch/build_native.sh)
#   all      both (default)
#
# Options:
#   --disc PATH       the GZLE01 revision 0 disc image (default: $COS_DISC). Bind-mounted read-only;
#                     it is never copied into an image, the repository or build/.
#   --jobs N          parallel compile jobs (default: the number of CPUs the engine sees)
#   --platform P      linux/amd64 or linux/arm64 for the Linux image and binary (default: the
#                     host's; the other one runs emulated and is several times slower). The
#                     build directory is then build/native-linux-<arch>.
#   --engine E        docker or podman (default: Podman if installed, else Docker)
#   --test            after building, run the headless checks in the container: cos_sdk_smoke,
#                     cos_pc_tests and the static-init, disc-ls and title smoke runs (software
#                     Vulkan under Xvfb; COS_TEST_TARGETS="..." to choose others)
#   --regress         after building, run native/tools/regress.sh in the container (all of it;
#                     slow with software rendering)
#
# What the steps do (all inside containers; the host needs only bash, git and the engine):
#   1. the asset headers from the disc (native/tools/gen_assets.sh) into build/assets/GZLE01,
#      shared by both targets, unless they are already there;
#   2. RecompCore (Dolphin's DSP HLE, native/tools/fetch_recompcore.sh) into ref/recompcore;
#   3. linux: cmake + ninja in the image of scripts/docker/Containerfile.linux;
#      switch: Aurora at native/'s pin into build/aurora-3227d76 if missing, then
#      scripts/switch/build_native.sh (devkitPro image, Mesa and Dawn built in containers).
#
# Downloads are kept between runs, under the repository's ignored directories:
#   build/native-linux*/_deps        Aurora, Dawn (prebuilt package), nod (prebuilt), SDL 3, ...
#   build/decomp-docker              the decompilation checkout and its dtk binary (Linux build)
#   ref/recompcore                   RecompCore
#   build/aurora-3227d76             Aurora at the pin (the Switch build)
#   build/switch-native/_deps        Dawn's source and dependencies for the Switch
#   build/switch-mesa/downloads      Mesa's source tarball and devkitPro's recipe patches
#   container images                 localhost/centollos-linux-build:<hash of the Containerfile>,
#                                    localhost/centollos-switch-*-build:<date>
# Delete a directory to fetch it again; `docker image prune` drops old images.
#
# Every binary built contains code generated from the disc: for the owner of the disc only.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
source "$root/scripts/switch/container.sh"

what=all
disc=${COS_DISC:-}
jobs=""
platform=""
engine=""
do_test=0
do_regress=0
while [[ $# -gt 0 ]]; do
    case $1 in
        linux|switch|all) what=$1; shift ;;
        --disc) disc=$2; shift 2 ;;
        --jobs) jobs=$2; shift 2 ;;
        --platform) platform=$2; shift 2 ;;
        --engine) engine=$2; shift 2 ;;
        --test) do_test=1; shift ;;
        --regress) do_regress=1; shift ;;
        -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "docker/build: unknown argument $1 (see --help)" >&2; exit 2 ;;
    esac
done

if [[ -z $disc ]]; then
    echo "docker/build: no disc image: pass --disc PATH or set COS_DISC (GZLE01 revision 0 .iso)" >&2
    exit 14
fi
if [[ ! -f $disc ]]; then
    echo "docker/build: $disc: no such file" >&2
    exit 14
fi
disc=$(cd "$(dirname "$disc")" && pwd)/$(basename "$disc")
disc_name=$(basename "$disc")

if [[ -n $engine ]]; then
    export SWITCH_CONTAINER_ENGINE=$engine
fi
engine=$(container_engine)
if [[ -z $engine ]]; then
    echo "docker/build: Docker or Podman is required" >&2
    exit 1
fi
if ! "$engine" info >/dev/null 2>&1; then
    echo "docker/build: '$engine info' failed: is the engine running (Docker Desktop started)?" >&2
    exit 1
fi

# --- the Linux build image ------------------------------------------------------------------------
containerfile=$root/scripts/docker/Containerfile.linux
if command -v sha256sum >/dev/null 2>&1; then
    tag=$(sha256sum "$containerfile" | cut -c1-12)
else
    tag=$(shasum -a 256 "$containerfile" | cut -c1-12)
fi
image=localhost/centollos-linux-build:$tag
platform_args=()
arch_suffix=""
if [[ -n $platform ]]; then
    platform_args=(--platform "$platform")
    image=$image-${platform##*/}
    arch_suffix=-${platform##*/}
fi
if ! container_image_exists "$engine" "$image"; then
    echo "docker/build: building $image"
    "$engine" build ${platform_args[@]+"${platform_args[@]}"} --tag "$image" --file "$containerfile" "$root/scripts/docker"
fi
if [[ -z $jobs ]]; then
    jobs=$("$engine" run --rm ${platform_args[@]+"${platform_args[@]}"} "$image" nproc)
fi

# A git worktree (build/lanes/<lane>) keeps git's data in the main checkout, which the container
# cannot see: the scripts below then run without git, which they only need for fetching.
run_linux() { # extra run options..., -- command
    local opts=()
    while [[ $1 != -- ]]; do opts+=("$1"); shift; done
    shift
    container_run "$engine" "$root" ${platform_args[@]+"${platform_args[@]}"} \
        -v "$disc:/disc/$disc_name:ro" -e COS_DISC="/disc/$disc_name" \
        -e XDG_RUNTIME_DIR=/tmp ${opts[@]+"${opts[@]}"} "$image" bash -c "$*"
}

# --- 1. asset headers, 2. RecompCore -----------------------------------------------------------
assets=$root/build/assets/GZLE01
if [[ ! -d $assets/include/assets ]]; then
    echo "docker/build: generating the asset headers from $disc_name into build/assets/GZLE01"
    run_linux -- "native/tools/gen_assets.sh --disc /disc/$disc_name --decomp /work/build/decomp-docker \
        --out /work/build/assets/GZLE01"
fi
if [[ ! -f $root/ref/recompcore/Source/Core/Core/HW/DSPHLE/UCodes/UCodes.cpp ]]; then
    run_linux -- "native/tools/fetch_recompcore.sh /work/ref/recompcore"
fi

# --- 3a. Linux --------------------------------------------------------------------------------------
if [[ $what == linux || $what == all ]]; then
    bdir=build/native-linux$arch_suffix
    echo "docker/build: Linux build in $bdir ($image, $jobs jobs)"
    # The container fetches Aurora itself: a FETCHCONTENT_SOURCE_DIR_AURORA left in the cache by a
    # configure outside this script would name a path the container does not have.
    run_linux -- "cmake -S native -B $bdir -G Ninja -UFETCHCONTENT_SOURCE_DIR_AURORA \
            -DCOS_ASSETS_DIR=/work/build/assets/GZLE01 -DCOS_RECOMPCORE_DIR=/work/ref/recompcore >/dev/null &&
        ninja -C $bdir -j $jobs switchwaker cos_sdk_smoke cos_pc_tests"
    file "$root/$bdir/switchwaker" 2>/dev/null || ls -l "$root/$bdir/switchwaker"

    if [[ $do_test == 1 ]]; then
        targets=${COS_TEST_TARGETS:-static-init disc-ls title}
        # Xvfb started directly, not through xvfb-run: xvfb-run waits for Xvfb's ready signal
        # (SIGUSR1) before running the command and can miss it when Xvfb starts fast, then waits
        # forever (a --regress run sat 2 h before regress.sh even started). Wait for the socket.
        run_linux -e COS_BUILD_DIR="$bdir" -e COS_ALLOW_CPU_ADAPTER=1 -- "set -e
            Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 &
            for i in \$(seq 1 50); do [ -S /tmp/.X11-unix/X99 ] && break; sleep 0.1; done
            export DISPLAY=:99
            $bdir/cos_sdk_smoke >/dev/null 2>&1 && echo 'ok   cos_sdk_smoke' || { echo 'FAIL cos_sdk_smoke'; exit 1; }
            $bdir/cos_pc_tests >/dev/null && echo 'ok   cos_pc_tests' || { echo 'FAIL cos_pc_tests'; exit 1; }
            for t in $targets; do
                if native/tools/run.sh \$t --quiet >/dev/null 2>&1; then
                    echo \"ok   run \$t\"
                else
                    echo \"FAIL run \$t (see $bdir/runs/)\"; exit 1
                fi
            done"
    fi
    if [[ $do_regress == 1 ]]; then
        run_linux -e COS_BUILD_DIR="$bdir" -e COS_ALLOW_CPU_ADAPTER=1 -- \
            "Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 &
            for i in \$(seq 1 50); do [ -S /tmp/.X11-unix/X99 ] && break; sleep 0.1; done
            export DISPLAY=:99
            native/tools/regress.sh -j 4"
    fi
fi

# --- 3b. Switch -------------------------------------------------------------------------------------
if [[ $what == switch || $what == all ]]; then
    aurora_pin=$(sed -n 's/^set(COS_AURORA_COMMIT "\([0-9a-f]*\)".*/\1/p' "$root/native/cmake/Aurora.cmake")
    aurora=$root/build/aurora-${aurora_pin:0:7}
    if [[ ! -f $aurora/cmake/aurora_core.cmake ]]; then
        echo "docker/build: fetching Aurora ${aurora_pin:0:7} into build/aurora-${aurora_pin:0:7}"
        run_linux -- "set -e; d=/work/build/aurora-${aurora_pin:0:7}; rm -rf \$d; mkdir -p \$d; cd \$d
            git init -q; git remote add origin https://github.com/encounter/aurora.git
            git fetch -q --depth 1 origin $aurora_pin
            git -c advice.detachedHead=false checkout -q --detach FETCH_HEAD"
    fi
    echo "docker/build: Switch build (scripts/switch/build_native.sh, $jobs jobs)"
    "$root/scripts/switch/build_native.sh" --aurora "$aurora" --assets "$assets" \
        --recompcore "$root/ref/recompcore" --jobs "$jobs"
fi
