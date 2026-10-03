# Link census (docs/NATIVE_PORT_PHASE2_3.md, step 2.5): what the non-REL game units still need.
#
#   ninja cos_link_census   ->  build/native-mac/link_census.txt            (counts and lists)
#                               build/native-mac/link_census_unresolved.txt (category<TAB>symbol)
#
# - Inputs: the objects of every enabled module, minus the REL units of rel_units.txt (filtered
#   out of $<TARGET_OBJECTS:m> with $<FILTER:...,EXCLUDE,regex>), plus cos_sdk and Aurora when
#   COS_WITH_AURORA is on. Nothing is recompiled: the module objects are reused.
# - cos_link_census is a MODULE (a Mach-O bundle) linked with -undefined dynamic_lookup, so it
#   links whatever is still missing; tools/link_census.py then sorts the symbols the bundle looks
#   up dynamically (`nm -um`) into SDK, REL, JAudio/JAZel, MSL/runtime, deferred units and other.
# - Before the link, tools/symbol_census.py lists duplicate strong definitions among the inputs
#   (link_census/symbol_census.txt). With COS_LINK_CENSUS_STRICT they stop the link. Without it
#   (the default while the decomp's SDK headers define hardware registers in every unit) they are
#   made local in copies of the objects, so the census still sees the rest; the report counts them.
#
# macOS only: it relies on ld64 bundles and the Xcode nm/otool. Not part of `all`.
include_guard(GLOBAL)

if (NOT APPLE)
    message(STATUS "cos_native: link census disabled (it needs macOS ld64, nm and otool)")
    return()
endif ()

find_package(Python3 3.8 COMPONENTS Interpreter)
if (NOT Python3_Interpreter_FOUND)
    message(STATUS "cos_native: link census disabled (no Python 3)")
    return()
endif ()

option(COS_LINK_CENSUS_STRICT
        "Link census: let duplicate strong symbols stop the link instead of making them local" OFF)

set(_census_dir "${CMAKE_BINARY_DIR}/link_census")
set(_census_tools "${COS_NATIVE_ROOT}/tools")

# REL unit paths (relative to game/src).
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/rel_units.txt" _rel_lines)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/rel_units.txt")
set(_rel_units)
foreach (_l IN LISTS _rel_lines)
    string(STRIP "${_l}" _l)
    if (_l AND NOT _l MATCHES "^#")
        list(APPEND _rel_units "${_l}")
    endif ()
endforeach ()
list(LENGTH _rel_units _n_rel_units)

# Per enabled module: the non-REL objects (nested EXCLUDE filters, one per chunk of REL names, to
# keep each regex small) and the REL objects (one INCLUDE filter per chunk).
set(_census_modules)
set(_census_keep)
set(_census_rel)
set(_census_n_kept 0)
set(_census_n_rel 0)
foreach (_m IN LISTS COS_MODULES)
    if (NOT TARGET ${_m})
        continue()
    endif ()
    list(APPEND _census_modules ${_m})
    get_property(_srcs GLOBAL PROPERTY "COS_MODULE_SOURCES:${_m}")
    set(_names)
    foreach (_s IN LISTS _srcs)
        file(RELATIVE_PATH _rel "${COS_ROOT}/src" "${_s}")
        if (_rel IN_LIST _rel_units)
            string(REGEX REPLACE "[.](c|cpp)$" "" _stem "${_rel}")
            list(APPEND _names "${_stem}")
        endif ()
    endforeach ()
    list(LENGTH _srcs _n_srcs)
    list(LENGTH _names _n_names)
    math(EXPR _census_n_kept "${_census_n_kept} + ${_n_srcs} - ${_n_names}")
    math(EXPR _census_n_rel "${_census_n_rel} + ${_n_names}")

    set(_keep "$<TARGET_OBJECTS:${_m}>")
    set(_begin 0)
    while (_begin LESS _n_names)
        list(SUBLIST _names ${_begin} 40 _chunk)
        math(EXPR _begin "${_begin} + 40")
        list(JOIN _chunk "|" _alt)
        # '/game/src/<unit>.<ext>.o' at the end of the object path; [.] avoids escaping backslashes.
        set(_rx "/game/src/(${_alt})[.](c|cpp)[.]o$")
        set(_keep "$<FILTER:${_keep},EXCLUDE,${_rx}>")
        list(APPEND _census_rel "$<FILTER:$<TARGET_OBJECTS:${_m}>,INCLUDE,${_rx}>")
    endwhile ()
    list(APPEND _census_keep "${_keep}")
endforeach ()

if (NOT _census_modules)
    message(STATUS "cos_native: link census disabled (no module enabled)")
    return()
endif ()

# Object lists, written at generate time (absolute paths, one per line).
file(GENERATE OUTPUT "${_census_dir}/objects.txt"
        CONTENT "$<JOIN:${_census_keep},\n>\n")
file(GENERATE OUTPUT "${_census_dir}/rel_objects.txt"
        CONTENT "$<JOIN:${_census_rel},\n>\n")

set(_census_all_objects)
foreach (_m IN LISTS _census_modules)
    list(APPEND _census_all_objects "$<TARGET_OBJECTS:${_m}>")
endforeach ()

set(_census_strict_arg)
if (COS_LINK_CENSUS_STRICT)
    set(_census_strict_arg --strict)
endif ()

# Before the link: symbol census of the inputs, then the response file the bundle links.
set(_census_rsp "${_census_dir}/link.rsp")
add_custom_command(
        OUTPUT "${_census_rsp}" "${_census_dir}/symbol_census.txt" "${_census_dir}/duplicates.txt"
        COMMAND "${Python3_EXECUTABLE}" "${_census_tools}/link_census.py" prepare
                --objects "${_census_dir}/objects.txt"
                --out-dir "${_census_dir}"
                --rsp "${_census_rsp}"
                --root "${CMAKE_BINARY_DIR}"
                ${_census_strict_arg}
        DEPENDS ${_census_modules} ${_census_all_objects}
                "${_census_dir}/objects.txt"
                "${_census_tools}/link_census.py" "${_census_tools}/symbol_census.py"
        COMMENT "Link census: symbol census of ${_census_n_kept} non-REL units"
        VERBATIM)
add_custom_target(cos_link_census_inputs DEPENDS "${_census_rsp}")

# The bundle. Its only source is an empty unit: the objects come in through the response file.
file(GENERATE OUTPUT "${_census_dir}/census_stub.c" CONTENT
        "/* Generated by native/cmake/census.cmake: the link census bundle's only source. */\nint cos_link_census_stub;\n")
add_library(cos_link_census MODULE EXCLUDE_FROM_ALL "${_census_dir}/census_stub.c")
add_dependencies(cos_link_census cos_link_census_inputs)
target_link_options(cos_link_census PRIVATE "LINKER:-undefined,dynamic_lookup" "@${_census_rsp}")
set_target_properties(cos_link_census PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${_census_dir}"
        SUFFIX ".bundle"
        # The game is C++: link with the C++ driver, so libc++/libc++abi resolve the C++ runtime.
        LINKER_LANGUAGE CXX
        # The report runs POST_BUILD: relink when the report code changes too, so a fixed
        # classifier is never hidden behind an up-to-date bundle.
        LINK_DEPENDS "${_census_rsp};${_census_tools}/link_census.py;${_census_tools}/symbol_census.py")
if (TARGET cos_sdk)
    # cos_sdk links the Aurora SDK libraries (COS_AURORA_LIBS) publicly.
    target_link_libraries(cos_link_census PRIVATE cos_sdk)
    set(_census_aurora ON)
else ()
    set(_census_aurora OFF)
endif ()

add_custom_command(TARGET cos_link_census POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${_census_tools}/link_census.py" report
                --bundle "$<TARGET_FILE:cos_link_census>"
                --objects "${_census_dir}/objects.txt"
                --rel-objects "${_census_dir}/rel_objects.txt"
                --duplicates "${_census_dir}/duplicates.txt"
                --deferred "${CMAKE_BINARY_DIR}/cos_deferred.txt"
                --game-root "${COS_ROOT}"
                --sdk-headers "${COS_SDK_HEADERS}"
                --with-aurora "${_census_aurora}"
                ${_census_strict_arg}
                --out "${CMAKE_BINARY_DIR}/link_census.txt"
                --unresolved "${CMAKE_BINARY_DIR}/link_census_unresolved.txt"
        VERBATIM)

message(STATUS "cos_native: link census over ${_census_n_kept} non-REL units "
        "(${_census_n_rel} of the ${_n_rel_units} REL units enabled, left out); "
        "strict: ${COS_LINK_CENSUS_STRICT}")
