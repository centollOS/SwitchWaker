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
