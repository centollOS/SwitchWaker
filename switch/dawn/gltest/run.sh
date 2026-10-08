#!/usr/bin/env bash
# Builds the Dawn GL test harness (switch/dawn/gltest/README.md) in a Linux container against the
# patched Dawn source and runs gl_draw_test (plain and with GLTEST_DIAG=1) on Mesa's llvmpipe
# through EGL (surfaceless).
#   switch/dawn/gltest/run.sh <patched dawn source> [build dir] [-- VAR=value...]
# With "-- VAR=value..." only that one variant of gl_draw_test runs (e.g. GLTEST_DUMP=1 prints
# Dawn's GLSL). With GLTEST_WGSL=<dir of .wgsl files> (Aurora's shaders, see README.md) gl_shader_test
# also builds a pipeline from each.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
dawn=$(cd "$1" && pwd)
out=${2:-$here/../../../build/gltest}
one=""
if [[ $# -ge 3 && $3 == -- ]]; then
    shift 3
    one="$*"
fi
mkdir -p "$out"
out=$(cd "$out" && pwd)
image=decomp-dawn-gltest:1
docker image inspect "$image" >/dev/null 2>&1 || docker build -t "$image" "$here"
wgsl_mount=()
if [[ -n ${GLTEST_WGSL:-} ]]; then
    wgsl_mount=(-v "$(cd "$GLTEST_WGSL" && pwd):/wgsl:ro")
fi
docker run --rm -v "$dawn:/dawn" -v "$here:/src:ro" -v "$out:/out" ${wgsl_mount[@]+"${wgsl_mount[@]}"} \
    -e EGL_PLATFORM=surfaceless -e ONE="$one" "$image" bash -c '
set -e
cmake -S /src -B /out/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ -DDAWN_SRC=/dawn > /out/configure.log 2>&1 || { tail -30 /out/configure.log; exit 1; }
cmake --build /out/build --target gl_draw_test gl_shader_test --parallel 9 > /out/build.log 2>&1 || { grep -m20 -B2 -A8 "error" /out/build.log; exit 1; }
if [ -n "$ONE" ]; then
    env $ONE /out/build/gl_draw_test
    exit $?
fi
status=0
for env in "" "GLTEST_DIAG=1"; do
    echo "== ${env:-defaults}"
    env $env /out/build/gl_draw_test || status=1
done
if [ -d /wgsl ]; then
    echo "== gl_shader_test"
    /out/build/gl_shader_test /wgsl 2>&1 | grep -v "^Warning" | tail -12 || status=1
    [ "${PIPESTATUS[0]}" = 0 ] || status=1
fi
exit $status
'
