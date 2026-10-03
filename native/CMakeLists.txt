# Native port of the game: the decompiled game (game) built with the host compiler.
#
# Phase 1 (docs/NATIVE_PORT_PLAN.md): every translation unit of the game compiles to an object
# with clang under TARGET_PC, against the host C/C++ library instead of Metrowerks' MSL.
# Nothing is linked yet: the GameCube SDK over Aurora is phase 2 (the game compiles against
# Aurora's SDK headers since step 2.8), the RELs linked statically
# phase 3, 64-bit and endianness semantics phase 4, JAudio phase 5.
#
# Configure:  cmake -S native -B build/native-mac -G Ninja
# Build:      ninja -C build/native-mac -k 0 <module>     (see native/README.md)

cmake_minimum_required(VERSION 3.25)

# Benchmark variant (docs/NATIVE_PORT_PHASE4_6.md, step 6.7), in a build directory of its own:
#   cmake -S native -B build/native-mac-perf -G Ninja -DCOS_PERF_BUILD=ON
# Release at -O2 (not -O3), every target compiled for generic ARMv8.0 (-march=armv8-a, the A57's
# level) instead of the host CPU's, and Tracy forced off. The executable logs that it is this
# variant; COS_PERF / COS_PERF_EVERY then give its frame times.
option(COS_PERF_BUILD "Benchmark build: -O2, -march=armv8-a, no Tracy (step 6.7)" OFF)

if (COS_PERF_BUILD)
    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING
                "Build type options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
    endif ()
    if (NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "COS_PERF_BUILD needs CMAKE_BUILD_TYPE=Release (got ${CMAKE_BUILD_TYPE})")
    endif ()
    # Before project(), so these are the cache defaults it would otherwise set to -O3.
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Flags used by the C compiler in Release")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Flags used by the C++ compiler in Release")
    set(CMAKE_OBJC_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Flags used by the ObjC compiler in Release")
    set(CMAKE_OBJCXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "Flags used by the ObjC++ compiler in Release")
    set(TRACY_ENABLE OFF CACHE BOOL "Enable Tracy profiling" FORCE)
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Build type options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif ()

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
endif ()

project(cos_native LANGUAGES C CXX)

if (COS_PERF_BUILD)
    # Before any target (Aurora and its dependencies included), so all of them get it.
    if (CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
        add_compile_options(-march=armv8-a)
    else ()
        message(WARNING "COS_PERF_BUILD: not an ARM64 host (${CMAKE_SYSTEM_PROCESSOR}); no -march=armv8-a")
    endif ()
    add_compile_definitions(COS_PERF_BUILD=1)
    message(STATUS "cos_native: COS_PERF_BUILD (Release -O2, -march=armv8-a, Tracy off)")
endif ()

if (APPLE AND NOT CMAKE_OSX_SYSROOT)
    # If the Xcode SDK is lagging behind the system version, CMake needs this done first
    execute_process(COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "cos_native: only clang is tested (got ${CMAKE_CXX_COMPILER_ID})")
endif ()

set(COS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../game" CACHE INTERNAL "Decompiled game source")
set(COS_NATIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Native port root")

# Compile definitions, include paths, flags and the force-included PC config header.
include(cmake/GameConfig.cmake)
# Units excluded from the build until a later phase; every entry has its reason.
include(cmake/deferred.cmake)
# One OBJECT library per module, each behind a COS_MODULE_<name> option.
include(cmake/modules.cmake)
# Aurora, the GameCube SDK over WebGPU (phase 2); already included by GameConfig.cmake.
include(cmake/Aurora.cmake)
# cos_dsp_hle, Dolphin's high-level DSP behind cos_sdk's DSP library (step 5.A, decision H6).
include(cmake/dsp_hle.cmake)
# cos_sdk, the game-specific SDK over Aurora, and its headless test cos_sdk_smoke.
include(cmake/sdk.cmake)
# The game's GF sources (game/src/dolphin/gf) compiled into cos_sdk.
include(cmake/sdk_gf.cmake)
# Link census (phase 2): `ninja cos_link_census` lists what the non-REL units still need.
include(cmake/census.cmake)
# The game executable (phase 3, step 3.8): `ninja centollos` links every module, cos_sdk and Aurora.
include(cmake/executable.cmake)
# Layout check (phase 4, step 4.0c): `ninja cos_layout_check` asserts the GameCube offsets of the
# disc-mapped structs (check/layout_headers.txt) on the host, minus check/layout_xfail.txt.
include(cmake/layout_check.cmake)

# Scaffold check: proves the toolchain, the include paths and the PC config header work.
add_library(cos_scaffold_check OBJECT
        check/pc_check.cpp
        check/pc_check.c)
target_link_libraries(cos_scaffold_check PRIVATE cos_game_headers)

# SDK header check (phase 2, step 2.3): every SDK header name the decomp has, in one unit, against
# the game's SDK headers (Aurora's plus the native/include/sdk forwarders). cos_sdk_shadow_check
# also proves that none of them resolves under game/include/dolphin.
add_library(cos_sdk_header_check OBJECT check/sdk_headers.cpp)
target_link_libraries(cos_sdk_header_check PRIVATE cos_game_headers)
add_custom_target(cos_sdk_shadow_check
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/check/check_sdk_shadow.sh" "${CMAKE_BINARY_DIR}"
        DEPENDS cos_sdk_header_check
        USES_TERMINAL
        COMMENT "Checking that no SDK header resolves under game/include/dolphin")
