# Native port of the game: the decompiled game (game) built with the host compiler.
#
# Phase 1 (docs/NATIVE_PORT_PLAN.md): every translation unit of the game compiles to an object
# with clang under TARGET_PC, against the host C/C++ library instead of Metrowerks' MSL.
# Nothing is linked yet: the GameCube SDK over Aurora is phase 2, the RELs linked statically
# phase 3, 64-bit and endianness semantics phase 4, JAudio phase 5.
#
# Configure:  cmake -S native -B build/native-mac -G Ninja
# Build:      ninja -C build/native-mac -k 0 <module>     (see native/README.md)

cmake_minimum_required(VERSION 3.25)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Build type options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif ()

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
endif ()

project(cos_native LANGUAGES C CXX)

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
# Aurora, the GameCube SDK over WebGPU (phase 2), behind COS_WITH_AURORA.
include(cmake/Aurora.cmake)

# Scaffold check: proves the toolchain, the include paths and the PC config header work.
add_library(cos_scaffold_check OBJECT
        check/pc_check.cpp
        check/pc_check.c)
target_link_libraries(cos_scaffold_check PRIVATE cos_game_headers)
