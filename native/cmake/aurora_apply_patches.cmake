# Applies native/patches/aurora/*.patch (decision H11) to an Aurora source tree, in name order.
# Script mode, used by Aurora.cmake both as the FetchContent PATCH_COMMAND and on the per-build-dir
# copy of a local FETCHCONTENT_SOURCE_DIR_AURORA checkout:
#   cmake -DCOS_AURORA_SRC=<tree> -DCOS_AURORA_PATCH_DIR=<dir> -P aurora_apply_patches.cmake
# Idempotent: a patch that already reverse-applies is skipped, so a rerun of the patch step is
# harmless. GIT_CEILING_DIRECTORIES keeps `git apply` from finding an enclosing repository (the
# copy lives inside the build dir, which may sit inside this repository's work tree), so paths in
# the patches stay relative to the Aurora tree.
cmake_minimum_required(VERSION 3.28)

if (NOT COS_AURORA_SRC OR NOT COS_AURORA_PATCH_DIR)
    message(FATAL_ERROR "aurora_apply_patches: COS_AURORA_SRC and COS_AURORA_PATCH_DIR are required")
endif ()

find_program(COS_GIT git REQUIRED)
get_filename_component(_ceiling "${COS_AURORA_SRC}" DIRECTORY)
set(ENV{GIT_CEILING_DIRECTORIES} "${_ceiling}")

# With COS_AURORA_PATCH_HASH (the FetchContent patch step), a stamp in the tree records the patch
# set applied last. The per-patch reverse check below cannot tell a fully patched tree apart when
# later patches change the same lines as earlier ones (0008, 0010 and 0011 do), so a rerun of the
# patch step (any reconfigure) relies on the stamp instead.
set(_stamp "${COS_AURORA_SRC}/.cos_aurora_patches")
if (COS_AURORA_PATCH_HASH AND EXISTS "${_stamp}")
    file(READ "${_stamp}" _old_hash)
    string(STRIP "${_old_hash}" _old_hash)
    if (_old_hash STREQUAL COS_AURORA_PATCH_HASH)
        message(STATUS "cos_native: Aurora patch set already applied (${_stamp})")
        return()
    endif ()
    message(FATAL_ERROR "cos_native: ${COS_AURORA_SRC} has another Aurora patch set applied; "
            "delete _deps/aurora-src and _deps/aurora-subbuild in the build directory and configure again")
endif ()

file(GLOB _patches LIST_DIRECTORIES false "${COS_AURORA_PATCH_DIR}/*.patch")
list(SORT _patches)
foreach (_patch IN LISTS _patches)
    get_filename_component(_name "${_patch}" NAME)
    execute_process(COMMAND "${COS_GIT}" apply --reverse --check "${_patch}"
            WORKING_DIRECTORY "${COS_AURORA_SRC}"
            RESULT_VARIABLE _applied OUTPUT_QUIET ERROR_QUIET)
    if (_applied EQUAL 0)
        message(STATUS "cos_native: Aurora patch ${_name} already applied")
        continue()
    endif ()
    execute_process(COMMAND "${COS_GIT}" apply "${_patch}"
            WORKING_DIRECTORY "${COS_AURORA_SRC}"
            RESULT_VARIABLE _result ERROR_VARIABLE _err)
    if (NOT _result EQUAL 0)
        message(FATAL_ERROR "cos_native: Aurora patch ${_name} does not apply to ${COS_AURORA_SRC}:\n${_err}")
    endif ()
    message(STATUS "cos_native: applied Aurora patch ${_name}")
endforeach ()
if (COS_AURORA_PATCH_HASH)
    file(WRITE "${_stamp}" "${COS_AURORA_PATCH_HASH}\n")
endif ()
