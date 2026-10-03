# Layout check of the disc-mapped structs (docs/NATIVE_PORT_PHASE4_6.md, step 4.0c).
#
#   ninja -C build/native-mac cos_layout_check
#
# native/tools/layout_check.py reads native/check/layout_headers.txt (the disc-mapped structs) and
# native/check/layout_xfail.txt (the checks known to fail until their format step lands) and
# generates <build>/layout_check/cos_layout_check.cpp: one static_assert per /* 0xNN */ offset
# comment and per `// Size: 0xNN` comment of those structs. The OBJECT library
# cos_layout_check_host compiles it with the game's flags, so it builds exactly when every check
# that is not xfailed holds and every xfailed one still fails (a stale xfail entry is an error
# too). -fno-access-control lets offsetof name private and protected members.
# The comments themselves are checked too (--gc-verify): the same checks, without the xfail list,
# must all hold when the decomp's GameCube headers are compiled by clang for PowerPC EABI, so a
# wrong comment fails the target instead of hiding a real difference or inventing one.
# cos_layout_check runs both. Failures with the host offsets: native/tools/layout_check.py --discover
include_guard(GLOBAL)

find_package(Python3 3.8 COMPONENTS Interpreter)
if (NOT Python3_Interpreter_FOUND)
    message(STATUS "cos_native: layout check disabled (no Python 3)")
    return()
endif ()

set(_layout_dir "${CMAKE_BINARY_DIR}/layout_check")
set(_layout_cpp "${_layout_dir}/cos_layout_check.cpp")
set(_layout_tool "${COS_NATIVE_ROOT}/tools/layout_check.py")
# The depfile lists the tool, both lists and every header they name, so editing any of them
# regenerates the unit; the compile itself tracks the headers the unit includes.
add_custom_command(
        OUTPUT "${_layout_cpp}"
        COMMAND "${Python3_EXECUTABLE}" "${_layout_tool}" --version ${COS_VERSION}
                --out "${_layout_cpp}" --depfile "${_layout_dir}/cos_layout_check.d"
        DEPENDS "${_layout_tool}"
                "${COS_NATIVE_ROOT}/check/layout_headers.txt"
                "${COS_NATIVE_ROOT}/check/layout_xfail.txt"
        DEPFILE "${_layout_dir}/cos_layout_check.d"
        COMMENT "Generating the layout checks of the disc-mapped structs"
        VERBATIM)

add_library(cos_layout_check_host OBJECT EXCLUDE_FROM_ALL "${_layout_cpp}")
target_link_libraries(cos_layout_check_host PRIVATE cos_game_headers)
target_compile_options(cos_layout_check_host PRIVATE -fno-access-control)
# After the game's -ferror-limit=50, so every failing check is reported.
set_source_files_properties("${_layout_cpp}" PROPERTIES COMPILE_OPTIONS "-ferror-limit=0")

# The GameCube configuration (no game flags, no TARGET_PC); reruns when the unit changes, i.e.
# when the tool, either list or a listed header changes.
set(_layout_gc_stamp "${_layout_dir}/gc_verify.stamp")
add_custom_command(
        OUTPUT "${_layout_gc_stamp}"
        COMMAND "${Python3_EXECUTABLE}" "${_layout_tool}" --version ${COS_VERSION} --gc-verify
                --cxx "${CMAKE_CXX_COMPILER}" --build "${CMAKE_BINARY_DIR}" --stamp "${_layout_gc_stamp}"
        DEPENDS "${_layout_cpp}" "${_layout_tool}"
        COMMENT "Checking the layout comments against the GameCube configuration"
        VERBATIM)

add_custom_target(cos_layout_check DEPENDS "${_layout_gc_stamp}")
add_dependencies(cos_layout_check cos_layout_check_host)
