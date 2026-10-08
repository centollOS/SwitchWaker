# Dawn GL test harness

The Switch's Dawn GL patches (`switch/dawn/patches/`) cannot run on the Mac (Dawn there is the
Metal backend). This harness builds the same patched Dawn source for Linux's OpenGL ES backend in
a container and runs it on Mesa's llvmpipe (GLES 3.2 through surfaceless EGL), so a patch that
changes what Dawn sends to GL can be checked for correctness before it goes to the console.
Performance numbers still come from the hardware.

    switch/dawn/gltest/run.sh build/dawn-src build/gltest
    GLTEST_WGSL=build/wgsl-dump switch/dawn/gltest/run.sh build/dawn-src build/gltest
    switch/dawn/gltest/run.sh build/dawn-src build/gltest -- GLTEST_DUMP=1

`build/dawn-src` is the Dawn tree the Switch build patched (`build/switch-dawn-src/<key>`, see
`scripts/switch/build_native.sh --dawn-src`); the first run builds Dawn in `build/gltest/build` (a few minutes), later runs only
what changed.

- `gl_draw_test`: 1024 draws shaped like Aurora's GX draws (vertices from a storage buffer, one
  uniform record per draw at a dynamic offset, 64 bytes of immediates, pipeline switches between
  blend/mask/depth/cull states and two uniform layouts, two render passes, records near the
  uniform buffer's end), compared pixel by pixel with a CPU reference. The uniform window and the
  pipeline state cache are always on (their run-time toggles were removed once they shipped).
  `GLTEST_DIAG=1` groups the mismatching cells by the pipelines drawn into them; `GLTEST_DUMP=1`
  prints the GLSL Dawn generates.
- `gl_shader_test <dir>`: builds a render pipeline with Aurora's GX layout from every `.wgsl`
  file in the directory, so Dawn translates each shader and Mesa compiles and links it. The GX
  shaders are Aurora's generated WGSL; to collect them, make Aurora write each shader it builds
  to a file (a local, uncommitted edit of `lib/gx/shader.cpp`'s `build_shader_source` in
  `build/native-mac/_deps/aurora-patched-src`: write `shaderSource` to
  `$AURORA_DUMP_WGSL/<hash>.wgsl`), rebuild `switchwaker`, and run outset-control with
  `COS_PRECOMPILE=all` and the bundled `initial_pipeline_cache.db` next to the executable (823
  shaders on 2026-10-04); undo the edit afterwards.
