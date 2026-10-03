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

# Aurora patches (decision H11): native/patches/aurora/*.patch, applied in name order by
# aurora_apply_patches.cmake. The FetchContent download gets them as its PATCH_COMMAND (the patch
# set's hash is part of the command, so a changed set reruns the idempotent step; a removed patch
# needs a fresh download: delete _deps/aurora-src and _deps/aurora-subbuild). A local
# FETCHCONTENT_SOURCE_DIR_AURORA checkout is never patched in place, since several build dirs
# share it: each build dir patches its own copy in _deps/aurora-patched-src, refreshed whenever the
# checkout's commit, its local changes or the patch set change, and builds from that copy.
set(COS_AURORA_PATCH_DIR "${CMAKE_CURRENT_LIST_DIR}/../patches/aurora")
get_filename_component(COS_AURORA_PATCH_DIR "${COS_AURORA_PATCH_DIR}" ABSOLUTE)
set(_cos_aurora_patch_script "${CMAKE_CURRENT_LIST_DIR}/aurora_apply_patches.cmake")
file(GLOB _cos_aurora_patches LIST_DIRECTORIES false CONFIGURE_DEPENDS "${COS_AURORA_PATCH_DIR}/*.patch")
list(SORT _cos_aurora_patches)
set(_cos_aurora_patch_key "")
foreach (_patch IN LISTS _cos_aurora_patches)
    file(SHA256 "${_patch}" _hash)
    get_filename_component(_name "${_patch}" NAME)
    string(APPEND _cos_aurora_patch_key "${_name}=${_hash};")
endforeach ()
string(SHA256 _cos_aurora_patch_hash "${_cos_aurora_patch_key}")

find_package(Git QUIET)

# A local source directory (FETCHCONTENT_SOURCE_DIR_AURORA) must be at the pin.
if (FETCHCONTENT_SOURCE_DIR_AURORA AND GIT_FOUND AND EXISTS "${FETCHCONTENT_SOURCE_DIR_AURORA}/.git")
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${FETCHCONTENT_SOURCE_DIR_AURORA}" rev-parse HEAD
            OUTPUT_VARIABLE _cos_aurora_head
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    if (_cos_aurora_head AND NOT _cos_aurora_head STREQUAL COS_AURORA_COMMIT)
        message(WARNING "cos_native: Aurora at ${FETCHCONTENT_SOURCE_DIR_AURORA} is ${_cos_aurora_head}, "
                "not the pinned ${COS_AURORA_COMMIT}")
    endif ()
endif ()

# COS_AURORA_PREPATCHED: the parent project (switch/native) already applied the patch set to its own
# copy of the checkout, so that copy is used as it is.
if (FETCHCONTENT_SOURCE_DIR_AURORA AND _cos_aurora_patches AND NOT COS_AURORA_PREPATCHED)
    set(_cos_aurora_local "${FETCHCONTENT_SOURCE_DIR_AURORA}")
    set(_cos_aurora_copy "${CMAKE_BINARY_DIR}/_deps/aurora-patched-src")
    # The copy is redone when the checkout or the patch set changes (commit plus `git status` of
    # the checkout, or the newest file time when it is not a git checkout).
    set(_cos_aurora_state "")
    if (GIT_FOUND AND EXISTS "${_cos_aurora_local}/.git")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${_cos_aurora_local}" status --porcelain
                OUTPUT_VARIABLE _cos_aurora_status ERROR_QUIET)
        set(_cos_aurora_state "${_cos_aurora_head}\n${_cos_aurora_status}")
    else ()
        file(GLOB_RECURSE _files LIST_DIRECTORIES false "${_cos_aurora_local}/*")
        foreach (_file IN LISTS _files)
            file(TIMESTAMP "${_file}" _time "%s" UTC)
            string(APPEND _cos_aurora_state "${_file}@${_time};")
        endforeach ()
    endif ()
    string(SHA256 _cos_aurora_stamp "${_cos_aurora_local}\n${_cos_aurora_state}\n${_cos_aurora_patch_hash}")
    set(_cos_aurora_stamp_file "${CMAKE_BINARY_DIR}/_deps/aurora-patched-src.stamp")
    set(_cos_aurora_old_stamp "")
    if (EXISTS "${_cos_aurora_stamp_file}" AND EXISTS "${_cos_aurora_copy}")
        file(READ "${_cos_aurora_stamp_file}" _cos_aurora_old_stamp)
    endif ()
    if (NOT _cos_aurora_old_stamp STREQUAL _cos_aurora_stamp)
        message(STATUS "cos_native: copying Aurora from ${_cos_aurora_local} to ${_cos_aurora_copy} for patching")
        file(REMOVE_RECURSE "${_cos_aurora_copy}")
        file(MAKE_DIRECTORY "${_cos_aurora_copy}")
        file(GLOB _entries LIST_DIRECTORIES true "${_cos_aurora_local}/*" "${_cos_aurora_local}/.*")
        list(FILTER _entries EXCLUDE REGEX "/\\.git$")
        file(COPY ${_entries} DESTINATION "${_cos_aurora_copy}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -DCOS_AURORA_SRC=${_cos_aurora_copy}
                -DCOS_AURORA_PATCH_DIR=${COS_AURORA_PATCH_DIR} -P "${_cos_aurora_patch_script}"
                RESULT_VARIABLE _result)
        if (NOT _result EQUAL 0)
            message(FATAL_ERROR "cos_native: patching the Aurora copy failed")
        endif ()
        file(WRITE "${_cos_aurora_stamp_file}" "${_cos_aurora_stamp}")
    endif ()
    # A normal variable shadows the cache entry for this configure only; the cache keeps pointing
    # at the shared checkout.
    set(FETCHCONTENT_SOURCE_DIR_AURORA "${_cos_aurora_copy}")
endif ()

include(FetchContent)
FetchContent_Declare(aurora
        GIT_REPOSITORY "${COS_AURORA_REPOSITORY}"
        GIT_TAG "${COS_AURORA_COMMIT}"
        PATCH_COMMAND "${CMAKE_COMMAND}" -DCOS_AURORA_SRC=<SOURCE_DIR>
            -DCOS_AURORA_PATCH_DIR=${COS_AURORA_PATCH_DIR}
            -DCOS_AURORA_PATCH_HASH=${_cos_aurora_patch_hash}
            -P "${_cos_aurora_patch_script}"
        EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(aurora)
unset(_cos_aurora_head)

# As in Dusklight. Off GEKKO, Aurora's <dolphin/mtx.h> maps the PSMTX* names to the C_MTX*
# functions by macro (PSMTXConcat -> MTXConcat -> C_MTXConcat), so the library exports C_MTX*.
target_compile_definitions(aurora_mtx PRIVATE MTX_USE_PS=1)

# The SDK libraries the game links (Dusklight's GAME_LIBS, minus the libraries it adds itself).
set(COS_AURORA_LIBS
        aurora::core aurora::gx aurora::gd aurora::si aurora::vi aurora::pad aurora::mtx
        aurora::os aurora::dvd aurora::thp aurora::card
        CACHE INTERNAL "Aurora SDK libraries")
message(STATUS "cos_native: Aurora ${COS_AURORA_COMMIT} from ${aurora_SOURCE_DIR}")
