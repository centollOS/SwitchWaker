# Dawn GL test harness (switch/dawn/gltest/README.md): the Switch's patched Dawn source built for
# Linux's OpenGL ES backend on Mesa's llvmpipe, and test programs for the Switch GL patches.
cmake_minimum_required(VERSION 3.22)
project(dawn_gltest CXX C)
set(CMAKE_CXX_STANDARD 20)
set(DAWN_SRC "/dawn" CACHE PATH "the patched Dawn source (build/dawn-src)")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC CACHE STRING "" FORCE)
foreach(opt DAWN_ENABLE_DESKTOP_GL DAWN_ENABLE_VULKAN DAWN_ENABLE_NULL DAWN_USE_X11 DAWN_USE_WAYLAND
        DAWN_USE_GLFW DAWN_BUILD_SAMPLES DAWN_BUILD_TESTS DAWN_BUILD_BENCHMARKS DAWN_BUILD_PROTOBUF
        DAWN_ENABLE_INSTALL DAWN_FETCH_DEPENDENCIES DAWN_ENABLE_SPIRV_VALIDATION TINT_BUILD_TESTS
        TINT_BUILD_SPV_READER TINT_BUILD_SPV_WRITER TINT_BUILD_GLSL_VALIDATOR TINT_BUILD_MSL_WRITER
        TINT_BUILD_HLSL_WRITER TINT_BUILD_TINTD TINT_BUILD_IR_BINARY TINT_BUILD_BENCHMARKS TINT_BUILD_FUZZERS)
    set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
foreach(opt DAWN_ENABLE_OPENGLES TINT_BUILD_GLSL_WRITER TINT_BUILD_NULL_WRITER TINT_BUILD_CMD_TOOLS)
    set(${opt} ON CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${DAWN_SRC}" dawn EXCLUDE_FROM_ALL)
foreach(test gl_draw_test gl_shader_test)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE webgpu_dawn dawn::dawncpp_headers)
endforeach()
