# dksh_cache (host): native/data/initial_pipeline_cache.db -> initial_dksh_cache.bin, the offline DKSH
# cache of the deko3d renderer (docs/DEKO3D_MIGRATION_PLAN.md, phase 1 and section 6). Links:
#   - Aurora's GX shader generator (lib/gx/shader.cpp, shader_info.cpp) from a copy of Aurora at
#     native/'s pin with native/patches/aurora and switch/native/aurora/patches applied, as the NRO;
#   - Tint's WGSL reader and GLSL writer from a copy of the Switch build's Dawn source (all of
#     switch/dawn/patches already applied there by the Switch build) plus
#     switch/dawn/patches/dawn-switch-tint-position-y-up.patch;
#   - uamlib (switch/uam) and the shared deko3d shader code (switch/deko).
# Built and run by build.sh in a Debian container (Containerfile). Inputs come as cache variables:
#   COS_DKSH_AURORA_SRC  Aurora at the pin (build/aurora-3227d76)
#   COS_DKSH_DAWN_SRC    the Switch build's Dawn source (build/switch-dawn-probe/_deps/dawn-src)
#   FETCHCONTENT_SOURCE_DIR_FMT / _XXHASH  local copies of Aurora's fmt and xxHash pins (optional)
cmake_minimum_required(VERSION 3.28)
project(dksh_cache C CXX)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)  # no C++20 modules (and no clang-scan-deps in the image)

get_filename_component(COS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(COS_DKSH_AURORA_SRC "" CACHE PATH "Aurora at native/'s pin")
set(COS_DKSH_DAWN_SRC "" CACHE PATH "the Switch build's (patched) Dawn source tree")
if (NOT EXISTS "${COS_DKSH_AURORA_SRC}/lib/gx/shader.cpp")
    message(FATAL_ERROR "dksh_cache: COS_DKSH_AURORA_SRC (${COS_DKSH_AURORA_SRC}) is not an Aurora checkout")
endif ()
if (NOT EXISTS "${COS_DKSH_DAWN_SRC}/src/tint/lang/glsl/writer/writer.h")
    message(FATAL_ERROR "dksh_cache: COS_DKSH_DAWN_SRC (${COS_DKSH_DAWN_SRC}) is not a Dawn source tree")
endif ()
find_program(COS_PATCH patch REQUIRED)

# ---- Dawn: a copy without the parts Tint does not need, plus the deko3d Tint patch -----------------
# The source must carry dawn-switch-gl-clip-control.patch (Tint's depth_zero_to_one): the Switch
# build applies all of switch/dawn/patches to it the first time it configures.
file(READ "${COS_DKSH_DAWN_SRC}/src/tint/lang/glsl/writer/common/options.h" _tint_options)
if (NOT _tint_options MATCHES "depth_zero_to_one")
    message(FATAL_ERROR "dksh_cache: ${COS_DKSH_DAWN_SRC} lacks switch/dawn/patches "
            "(dawn-switch-gl-clip-control.patch); configure the Switch build once (scripts/switch/build_native.sh)")
endif ()
set(_dawn_copy "${CMAKE_BINARY_DIR}/dawn-src")
set(_y_patch "${COS_REPO_ROOT}/switch/dawn/patches/dawn-switch-tint-position-y-up.patch")
file(SHA256 "${_y_patch}" _y_hash)
file(SHA256 "${COS_DKSH_DAWN_SRC}/src/tint/lang/glsl/writer/common/options.h" _opt_hash)
# bump the "copy" number when the copy rules below change
set(_dawn_stamp "copy 2\n${COS_DKSH_DAWN_SRC}\n${_opt_hash}\n${_y_hash}\n")
set(_dawn_previous "")
if (EXISTS "${_dawn_copy}.stamp")
    file(READ "${_dawn_copy}.stamp" _dawn_previous)
endif ()
if (NOT EXISTS "${_dawn_copy}" OR NOT _dawn_previous STREQUAL _dawn_stamp)
    file(REMOVE_RECURSE "${_dawn_copy}" "${_dawn_copy}.stamp")
    message(STATUS "dksh_cache: copying Dawn's source (Tint and what its CMake needs)")
    # third_party trees Tint and Dawn's CMake (all backends off) do not need
    set(_dawn_skip angle swiftshader directx-shader-compiler directx-headers OpenGL-Registry
            EGL-Registry glslang protobuf libprotobuf-mutator gn emsdk depot_tools catapult
            llvm-libc libc\\+\\+ libc\\+\\+abi vulkan-deps vulkan-headers vulkan-loader vulkan-tools
            vulkan-utility-libraries vulkan-validation-layers googletest google_benchmark
            benchmark_shaders glfw3 dxc renderdoc partition_alloc fuzztest libFuzzer node-addon-api
            node-api-headers gpuweb langsvr libdrm mesa meson khronos jsoncpp spirv-tools
            agility-sdk clang-format closure_compiler)
    list(JOIN _dawn_skip "|" _dawn_skip)
    string(REGEX REPLACE "([][+.*()^$?|\\])" "\\\\\\1" _src_re "${COS_DKSH_DAWN_SRC}")
    file(COPY "${COS_DKSH_DAWN_SRC}/" DESTINATION "${_dawn_copy}"
            PATTERN ".git" EXCLUDE REGEX "^${_src_re}/(docs|test|webgpu-cts)$" EXCLUDE
            REGEX "^${_src_re}/third_party/(${_dawn_skip})$" EXCLUDE)
    execute_process(COMMAND "${COS_PATCH}" -p1 --forward -i "${_y_patch}" WORKING_DIRECTORY "${_dawn_copy}"
            RESULT_VARIABLE _r OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    if (NOT _r EQUAL 0)
        message(FATAL_ERROR "dksh_cache: dawn-switch-tint-position-y-up.patch does not apply:\n${_o}${_e}")
    endif ()
    file(WRITE "${_dawn_copy}.stamp" "${_dawn_stamp}")
endif ()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY OFF CACHE STRING "" FORCE)
set(DAWN_BUILD_PROTOBUF OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_NODE_BINDINGS OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL ON CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_METAL OFF CACHE BOOL "" FORCE)
set(DAWN_USE_X11 OFF CACHE BOOL "" FORCE)
set(DAWN_USE_WAYLAND OFF CACHE BOOL "" FORCE)
set(DAWN_USE_GLFW OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)
set(DAWN_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_READER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_WRITER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_MSL_WRITER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_VALIDATOR OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_READER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_IR_BINARY OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TINTD OFF CACHE BOOL "" FORCE)
add_subdirectory("${_dawn_copy}" "${CMAKE_BINARY_DIR}/dawn" EXCLUDE_FROM_ALL SYSTEM)

# ---- Aurora: the NRO's patched copy, only its GX shader generator (and attr_fmt.cpp) is compiled -----------------------
set(_aurora_copy "${CMAKE_BINARY_DIR}/aurora")
set(_aurora_patch_dirs "${COS_REPO_ROOT}/native/patches/aurora" "${COS_REPO_ROOT}/switch/native/aurora/patches")
set(_aurora_stamp "${COS_DKSH_AURORA_SRC}\n")
foreach (_dir IN LISTS _aurora_patch_dirs)
    file(GLOB _patches CONFIGURE_DEPENDS "${_dir}/*.patch")
    list(SORT _patches)
    foreach (_patch IN LISTS _patches)
        file(SHA256 "${_patch}" _hash)
        string(APPEND _aurora_stamp "${_patch}=${_hash}\n")
    endforeach ()
endforeach ()
set(_aurora_previous "")
if (EXISTS "${_aurora_copy}.stamp")
    file(READ "${_aurora_copy}.stamp" _aurora_previous)
endif ()
if (NOT EXISTS "${_aurora_copy}" OR NOT _aurora_previous STREQUAL _aurora_stamp)
    file(REMOVE_RECURSE "${_aurora_copy}" "${_aurora_copy}.stamp")
    file(COPY "${COS_DKSH_AURORA_SRC}/" DESTINATION "${_aurora_copy}" PATTERN ".git" EXCLUDE)
    foreach (_dir IN LISTS _aurora_patch_dirs)
        execute_process(COMMAND "${CMAKE_COMMAND}" -DCOS_AURORA_SRC=${_aurora_copy} -DCOS_AURORA_PATCH_DIR=${_dir}
                -P "${COS_REPO_ROOT}/native/cmake/aurora_apply_patches.cmake" RESULT_VARIABLE _r)
        if (NOT _r EQUAL 0)
            message(FATAL_ERROR "dksh_cache: patching Aurora with ${_dir} failed")
        endif ()
    endforeach ()
    file(WRITE "${_aurora_copy}.stamp" "${_aurora_stamp}")
endif ()

include(FetchContent)
# Aurora's pins (extern/CMakeLists.txt)
FetchContent_Declare(fmt
        URL https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
        URL_HASH SHA256=ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE EXCLUDE_FROM_ALL)
FetchContent_Declare(xxhash
        URL https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.3.tar.gz
        URL_HASH SHA256=aae608dfe8213dfd05d909a57718ef82f30722c392344583d3f39050c7f29a80
        SOURCE_SUBDIR cmake_unofficial DOWNLOAD_EXTRACT_TIMESTAMP FALSE EXCLUDE_FROM_ALL)
set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fmt xxhash)

add_library(dksh_aurora_gx STATIC
        "${_aurora_copy}/lib/gx/shader.cpp"
        "${_aurora_copy}/lib/gx/shader_info.cpp"
        "${_aurora_copy}/lib/gx/attr_fmt.cpp")
target_include_directories(dksh_aurora_gx PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"  # tracy/Tracy.hpp without Tracy
        "${_aurora_copy}/include" "${_aurora_copy}/lib")
target_compile_definitions(dksh_aurora_gx PUBLIC AURORA TARGET_PC AURORA_ENABLE_GX WEBGPU_DAWN)
target_compile_options(dksh_aurora_gx PRIVATE -w)
target_link_libraries(dksh_aurora_gx PUBLIC fmt::fmt xxHash::xxhash absl::flat_hash_map absl::flat_hash_set
        dawn::dawncpp_headers)

# ---- uam and the tool ---------------------------------------------------------------------------
add_subdirectory("${COS_REPO_ROOT}/switch/uam" "${CMAKE_BINARY_DIR}/uam" EXCLUDE_FROM_ALL)
find_package(ZLIB REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(dksh_cache
        dksh_cache.cpp
        aurora_host.cpp
        "${COS_REPO_ROOT}/switch/deko/shader_translate.cpp"
        "${COS_REPO_ROOT}/switch/deko/dksh_file.cpp")
target_include_directories(dksh_cache PRIVATE "${COS_REPO_ROOT}/switch/deko" "${_dawn_copy}"
        "${_dawn_copy}/include" "${COS_REPO_ROOT}/switch/uam/source")
target_compile_options(dksh_cache PRIVATE -Wall -Wextra -Wno-unused-parameter)
# Aurora's headers (math.hpp) warn under -Wextra; aurora_host.cpp includes them
set_source_files_properties(aurora_host.cpp PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-copy-with-user-provided-copy")
target_link_libraries(dksh_cache PRIVATE dksh_aurora_gx uamlib tint_api tint_lang_glsl_writer
        tint_lang_wgsl_reader tint_lang_wgsl_inspector ZLIB::ZLIB SQLite::SQLite3)
