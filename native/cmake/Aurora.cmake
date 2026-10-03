# Aurora (encounter/aurora, MIT): the GameCube SDK over WebGPU that phase 2 builds on
# (docs/NATIVE_PORT_PHASE2_3.md, step 2.1, decision D1).
#
# Pulled in with FetchContent at Dusklight's pin. COS_WITH_AURORA is ON by default since step 2.8,
# and required: the game compiles against Aurora's SDK headers (GameConfig.cmake). The settings follow Dusklight's CMakeLists.txt (AURORA_ENABLE_*, MTX_USE_PS), except
# RmlUi, which this port does not use.
#
# Offline or local builds point CMake's own override at a checkout of the pinned commit:
#   git -C ref/aurora worktree add --detach build/aurora-3227d76 3227d76
#   cmake ... -DFETCHCONTENT_SOURCE_DIR_AURORA=$PWD/build/aurora-3227d76
# Aurora's own dependencies (Dawn, nod, SDL3, abseil, fmt, ...) are still fetched once into the
# build directory; Dawn and nod come as prebuilt packages where Aurora publishes them.
#
# Included from GameConfig.cmake first (it needs Aurora's include directory), then again from
# CMakeLists.txt: the guard makes the second include a no-op.
include_guard(GLOBAL)

option(COS_WITH_AURORA "Fetch and build Aurora (the GameCube SDK over WebGPU); required" ON)

if (NOT COS_WITH_AURORA)
    return()
endif ()

if (CMAKE_VERSION VERSION_LESS 3.28)
    message(FATAL_ERROR "COS_WITH_AURORA needs CMake 3.28 or newer (FetchContent EXCLUDE_FROM_ALL)")
endif ()

set(COS_AURORA_REPOSITORY "https://github.com/encounter/aurora.git" CACHE STRING
        "Aurora git repository")
# Dusklight's pin (ref/dusklight/extern/aurora), 2026-09-29.
set(COS_AURORA_COMMIT "3227d76c60e1e782ca576610bce61c9e7744d8be" CACHE STRING
        "Aurora commit to build")

# Aurora options, as in Dusklight (CMakeLists.txt:73-78). Cache defaults, so they can be overridden.
set(AURORA_ENABLE_GX ON CACHE BOOL "Enable GX implementation and WebGPU renderer")
set(AURORA_ENABLE_DVD ON CACHE BOOL "Enable DVD API support")
set(AURORA_ENABLE_CARD ON CACHE BOOL "Enable CARD API support")
set(AURORA_ENABLE_THP ON CACHE BOOL "Enable THP decoder")
set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "Enable RmlUi UI support")
set(AURORA_ENABLE_EXAMPLES OFF CACHE BOOL "Enable examples")
set(AURORA_ENABLE_TESTS OFF CACHE BOOL "Enable tests")

# Prebuilt Dawn and nod packages exist for darwin-arm64: no Dawn source build and no Rust needed.
# On other hosts Aurora's own "auto" resolution applies.
if (APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(AURORA_DAWN_PROVIDER "package" CACHE STRING
            "How to provide Dawn: auto, vendor (build from source), system (find_package/imported), package (download prebuilt)")
    set(AURORA_NOD_PROVIDER "package" CACHE STRING
            "How to provide nod: auto, vendor (build from source), system (find_package/imported), package (download prebuilt)")
endif ()

include(FetchContent)
FetchContent_Declare(aurora
        GIT_REPOSITORY "${COS_AURORA_REPOSITORY}"
        GIT_TAG "${COS_AURORA_COMMIT}"
        EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(aurora)

# A local source directory (FETCHCONTENT_SOURCE_DIR_AURORA) must be at the pin.
find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${aurora_SOURCE_DIR}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${aurora_SOURCE_DIR}" rev-parse HEAD
            OUTPUT_VARIABLE _cos_aurora_head
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    if (_cos_aurora_head AND NOT _cos_aurora_head STREQUAL COS_AURORA_COMMIT)
        message(WARNING "cos_native: Aurora at ${aurora_SOURCE_DIR} is ${_cos_aurora_head}, "
                "not the pinned ${COS_AURORA_COMMIT}")
    endif ()
    unset(_cos_aurora_head)
endif ()

# As in Dusklight. Off GEKKO, Aurora's <dolphin/mtx.h> maps the PSMTX* names to the C_MTX*
# functions by macro (PSMTXConcat -> MTXConcat -> C_MTXConcat), so the library exports C_MTX*.
target_compile_definitions(aurora_mtx PRIVATE MTX_USE_PS=1)

# The SDK libraries the game links (Dusklight's GAME_LIBS, minus the libraries it adds itself).
set(COS_AURORA_LIBS
        aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx
        aurora::os aurora::dvd aurora::thp aurora::card
        CACHE INTERNAL "Aurora SDK libraries")
message(STATUS "cos_native: Aurora ${COS_AURORA_COMMIT} from ${aurora_SOURCE_DIR}")
