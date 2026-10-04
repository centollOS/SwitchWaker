#!/usr/bin/env bash
# Build the Switch's Mesa (EGL, GLES and nouveau: libEGL.a, libGLESv2.a, libglapi.a) from source,
# with the persistent shader cache (switch/mesa/patches), for the native port's NRO.
#
#   scripts/switch/build_mesa.sh [--stock] [--test] [--jobs N]
#
# The recipe is devkitPro's switch-mesa 20.1.0-5 package (pacman-packages switch/mesa/PKGBUILD,
# pinned below): Mesa 20.1.0-rc3 plus devkitPro's three patches, configured with
# /opt/devkitpro/meson-cross.sh switch ... -Db_ndebug=true. On top of it come switch/mesa/patches:
#   0001  newlib timespec_get clash (toolchain compatibility only)
#   0002  compile counters and timers for the log (include/mesa_switch.h)
#   0003  the disk shader cache on Horizon, one file under $MESA_SHADER_CACHE_DIR
#   0004  nvc0's code generation through that cache
# Output: build/switch-mesa/prefix/{lib,include} (scripts/switch/build_native.sh links it), and
# include/mesa_switch_cache_id.h in the source tree: the driver build's name, a hash of the
# source and every patch, which keys the cache (a cache from another build is discarded).
#
#   --stock   only devkitPro's patches and 0001, into build/switch-mesa-stock/prefix: the
#             package's libraries rebuilt (same symbols), to check the recipe
#   --test    also build Mesa's nouveau driver for Linux with 0002-0004 (no devkitPro patches) in
#             a Debian container and run switch/mesa/test: the cache file (persistence, removal,
#             a torn tail, a damaged entry, another driver build, the size limit, its index file
#             read at once and rebuilt or completed when it disagrees) and nvc0's
#             code cache (a hit gives the same code, header and fixups as a fresh translation)
#   --jobs N  parallel jobs (default: SWITCH_BUILD_JOBS or 4)
# Sources are downloaded once into build/switch-mesa/downloads and checked against their sha256.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source "$root/scripts/switch/container.sh"
image=${COS_SWITCH_MESA_IMAGE:-localhost/centollos-switch-mesa-build:2026-10-04}
host_image=${COS_SWITCH_MESA_HOST_IMAGE:-localhost/centollos-switch-mesa-host:2026-10-04}

mesa_version=20.1.0-rc3
mesa_url=https://archive.mesa3d.org/older-versions/20.x/mesa-$mesa_version.tar.xz
mesa_sha256=c90b75ea34302ebde9b81b87c5642fa864c40fe9c4ad34ce0793170c1413168d
# devkitPro/pacman-packages at the commit that holds switch-mesa 20.1.0-5 (sha256 from its PKGBUILD).
dkp_commit=f103fe88e37180ecd0b7a9173b52b7580a54f71a
dkp_url=https://raw.githubusercontent.com/devkitPro/pacman-packages/$dkp_commit/switch/mesa
dkp_patches=(
    "switch-mesa-20.1.0-5.patch 950f93d3e5b6ae9c5a42c2918623fe9a80d7ee398d92d2e73abec86b62d75916"
    "gl_XML.py.patch a9bc326195b3fe29709e079466a8b2162a2ac9409f694eaad5494490155e2dd4"
    "glX_XML.py.patch 1475defcdf8600690eaddeee28d8f01310635c1273fb541f668e8789714d36ca"
)

stock=0 test=0
jobs=${SWITCH_BUILD_JOBS:-4}
while [[ $# -gt 0 ]]; do
    case $1 in
        --stock) stock=1; shift ;;
        --test) test=1; shift ;;
        --jobs) jobs=$2; shift 2 ;;
        -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
        *) echo "build_mesa: unknown option $1" >&2; exit 2 ;;
    esac
done

sha256() { shasum -a 256 "$1" 2>/dev/null | cut -d' ' -f1 || sha256sum "$1" | cut -d' ' -f1; }

downloads=$root/build/switch-mesa/downloads
mkdir -p "$downloads"
fetch() { # fetch URL FILE SHA256
    local url=$1 file=$downloads/$2 want=$3
    if [[ ! -f $file ]] || [[ $(sha256 "$file") != "$want" ]]; then
        echo "build_mesa: downloading $url"
        curl -fsSL -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    if [[ $(sha256 "$file") != "$want" ]]; then
        echo "build_mesa: $file does not have sha256 $want" >&2
        exit 1
    fi
}
fetch "$mesa_url" "mesa-$mesa_version.tar.xz" "$mesa_sha256"
for entry in "${dkp_patches[@]}"; do
    fetch "$dkp_url/${entry%% *}" "${entry%% *}" "${entry##* }"
done

ours=("$root"/switch/mesa/patches/*.patch)
if [[ $stock == 1 ]]; then
    ours=("$root"/switch/mesa/patches/0001-*.patch)
    work=$root/build/switch-mesa-stock
else
    work=$root/build/switch-mesa
fi

# The driver build's name: the version, devkitPro's patches and ours. A change to any patch
# changes it, and the cache written by an older build is discarded on the console.
id_input=$mesa_sha256
for entry in "${dkp_patches[@]}"; do id_input+=" ${entry##* }"; done
for p in "${ours[@]}"; do id_input+=" $(sha256 "$p")"; done
cache_id="switch-mesa-20.1.0-5+centollos-$(printf '%s' "$id_input" | shasum -a 256 | cut -c1-16)"

engine=$(container_engine)
if [[ -z $engine ]]; then
    echo "build_mesa: Podman or Docker is required" >&2
    exit 1
fi
if ! container_image_exists "$engine" "$image"; then
    "$engine" build --tag "$image" --file "$root/scripts/switch/Containerfile.mesa" "$root/scripts/switch"
fi

# A fresh source tree whenever the patches change (the stamp is the cache id).
src=$work/mesa-$mesa_version
mkdir -p "$work"
if [[ ! -f $work/source.stamp ]] || [[ $(cat "$work/source.stamp") != "$cache_id" ]]; then
    rm -rf "$src" "$work/prefix"
    tar -xf "$downloads/mesa-$mesa_version.tar.xz" -C "$work"
    for entry in "${dkp_patches[@]}"; do
        patch -d "$src" -p1 -s -i "$downloads/${entry%% *}"
    done
    for p in "${ours[@]}"; do
        patch -d "$src" -p1 -s -i "$p"
    done
    printf '/* Generated by scripts/switch/build_mesa.sh: the name of this driver build. */\n#define MESA_SWITCH_CACHE_ID "%s"\n' \
        "$cache_id" > "$src/include/mesa_switch_cache_id.h"
    printf '%s\n' "$cache_id" > "$work/source.stamp"
fi
echo "build_mesa: $src ($cache_id)"

rel=${work#"$root"/}
container_run "$engine" "$root" -e JOBS="$jobs" -e REL="$rel" -e SRCNAME="mesa-$mesa_version" "$image" bash -lc '
set -euo pipefail
cd "/work/$REL/$SRCNAME"
if [[ ! -f build/build.ninja ]]; then
    /opt/devkitpro/meson-cross.sh switch ../crossfile.txt build -Db_ndebug=true >../meson.log 2>&1 ||
        { tail -40 ../meson.log; exit 1; }
fi
ninja -C build -j "$JOBS" >../ninja.log 2>&1 || { grep -B2 -A20 "^FAILED" ../ninja.log | head -80; exit 1; }
rm -rf ../install ../prefix
DESTDIR=/work/$REL/install ninja -C build install >/dev/null
mkdir -p ../prefix
mv ../install/opt/devkitpro/portlibs/switch/lib ../install/opt/devkitpro/portlibs/switch/include ../prefix/
if [[ -f include/mesa_switch.h ]]; then cp include/mesa_switch.h ../prefix/include/; fi
rm -rf ../install
'
ls "$work/prefix/lib/libEGL.a" "$work/prefix/lib/libGLESv2.a" "$work/prefix/lib/libglapi.a" >/dev/null
printf '%s\n' "$cache_id" > "$work/prefix/cache_id.txt"
echo "build_mesa: built $work/prefix (cache id $cache_id)"

[[ $test == 1 && $stock == 0 ]] || exit 0

# ---- host test: upstream Mesa + 0002..0004 (the devkitPro patches only build for Horizon) ----
if ! container_image_exists "$engine" "$host_image"; then
    "$engine" build --tag "$host_image" --file "$root/scripts/switch/Containerfile.mesa-host" \
        "$root/scripts/switch"
fi
host=$root/build/switch-mesa/host
hsrc=$host/mesa-$mesa_version
if [[ ! -f $host/source.stamp ]] || [[ $(cat "$host/source.stamp") != "$cache_id" ]]; then
    rm -rf "$host"
    mkdir -p "$host"
    tar -xf "$downloads/mesa-$mesa_version.tar.xz" -C "$host"
    # devkitPro's two Python 3.9 fixes of the GL API generators (the OSMesa build needs them).
    patch -d "$hsrc" -p1 -s -i "$downloads/gl_XML.py.patch"
    patch -d "$hsrc" -p1 -s -i "$downloads/glX_XML.py.patch"
    for p in "$root"/switch/mesa/patches/000[234]-*.patch; do
        # The top-level meson.build hunk edits devkitPro's Horizon branch, absent upstream.
        awk '/^diff --git/ { skip = ($3 == "a/meson.build") } !skip' "$p" |
            patch -d "$hsrc" -p1 -s
    done
    # Test only: give softpipe a disk cache, to drive the GLSL cache through OSMesa.
    patch -d "$hsrc" -p1 -s -i "$root/switch/mesa/test/softpipe-disk-cache.patch"
    echo '#define MESA_SWITCH_CACHE_ID "host-test"' > "$hsrc/include/mesa_switch_cache_id.h"
    printf '%s\n' "$cache_id" > "$host/source.stamp"
fi
hrel=${host#"$root"/}
container_run "$engine" "$root" -e REL="$hrel" -e SRCNAME="mesa-$mesa_version" "$host_image" bash -lc '
set -euo pipefail
cd "/work/$REL/$SRCNAME"
if [[ ! -f build-host/build.ninja ]]; then
    meson setup build-host -Dbuildtype=debugoptimized -Db_ndebug=false \
        -Dc_args=-DMESA_DISK_CACHE_SINGLE_FILE -Dcpp_args=-DMESA_DISK_CACHE_SINGLE_FILE \
        -Dgallium-drivers=nouveau -Ddri-drivers= -Dvulkan-drivers= -Dplatforms=drm -Dglx=disabled \
        -Degl=false -Dgbm=false -Dllvm=false -Dgallium-xa=false -Dgallium-vdpau=false \
        -Dgallium-va=false -Dgallium-xvmc=false -Dgallium-omx=disabled -Dgallium-nine=false \
        -Dgallium-opencl=disabled >../meson.log 2>&1 || { tail -40 ../meson.log; exit 1; }
fi
B=build-host
libs=(src/gallium/drivers/nouveau/libnouveau.a src/gallium/auxiliary/libgallium.a
      src/gallium/auxiliary/libgalliumvl.a src/compiler/nir/libnir.a src/compiler/libcompiler.a
      src/util/libmesa_util.a src/util/format/libmesa_format.a src/util/libxmlconfig.a)
ninja -C $B "${libs[@]}" >../ninja.log 2>&1 || { grep -B2 -A20 "^FAILED" ../ninja.log | head -60; exit 1; }
I="-Iinclude -Isrc -Isrc/gallium/include -Isrc/gallium/auxiliary -Isrc/gallium/drivers/nouveau
   -Isrc/mesa -Isrc/mapi -I$B/src -I$B/src/compiler/nir -Isrc/compiler/nir -Isrc/compiler
   -I/usr/include/libdrm -I/usr/include/libdrm/nouveau -DMESA_DISK_CACHE_SINGLE_FILE
   -DENABLE_SHADER_CACHE -DHAVE_PTHREAD -D_GNU_SOURCE -DHAVE_ENDIAN_H -DHAVE_LINUX_FUTEX_H
   -DHAVE_TIMESPEC_GET"
gcc -std=gnu99 -g -c /work/switch/mesa/test/test_cache.c $I -o /tmp/test_cache.o
g++ -std=c++14 -c /work/switch/mesa/test/test_fixups.cpp $I -o /tmp/test_fixups.o
g++ /tmp/test_cache.o /tmp/test_fixups.o -Wl,--start-group "${libs[@]/#/$B/}" -Wl,--end-group \
    -ldrm_nouveau -ldrm -lz -lzstd -lexpat -lpthread -lm -ldl -o /tmp/test_cache
/tmp/test_cache

# The GL API on top (OSMesa, softpipe): GLSL cache hits and program binaries, run twice.
if [[ ! -f build-osmesa/build.ninja ]]; then
    meson setup build-osmesa -Dbuildtype=debugoptimized -Db_ndebug=false \
        -Dc_args=-DMESA_DISK_CACHE_SINGLE_FILE -Dcpp_args=-DMESA_DISK_CACHE_SINGLE_FILE \
        -Dgallium-drivers=swrast -Dosmesa=gallium -Ddri-drivers= -Dvulkan-drivers= -Dplatforms= \
        -Dglx=disabled -Degl=false -Dgbm=false -Dllvm=false -Dgles1=false -Dgles2=false \
        -Dshared-glapi=true >../osmesa-meson.log 2>&1 || { tail -40 ../osmesa-meson.log; exit 1; }
fi
ninja -C build-osmesa >../osmesa-ninja.log 2>&1 || { grep -B2 -A20 "^FAILED" ../osmesa-ninja.log | head -60; exit 1; }
gcc -O1 -g /work/switch/mesa/test/test_glsl_cache.c -Iinclude -Lbuild-osmesa/src/gallium/targets/osmesa \
    -lOSMesa -o /tmp/test_glsl_cache
export LD_LIBRARY_PATH=build-osmesa/src/gallium/targets/osmesa:build-osmesa/src/mapi/shared-glapi
export MESA_SHADER_CACHE_DIR=/tmp/glsl-cache-test MESA_GLSL=cache_info
rm -rf /tmp/glsl-cache-test && mkdir -p /tmp/glsl-cache-test
/tmp/test_glsl_cache store 2>/tmp/store.err
/tmp/test_glsl_cache load 2>/tmp/load.err
hits=$(grep -c "loading shader program meta data from cache" /tmp/load.err || true)
deferred=$(grep -c "deferring compile of shader" /tmp/load.err || true)
echo "second run: $hits programs linked from the cache, $deferred compiles deferred"
[[ $hits == 6 && $deferred == 12 ]] || { echo "GLSL cache: expected 6 and 12"; exit 1; }
'
