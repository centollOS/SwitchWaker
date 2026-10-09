# The game executable "switchwaker" (docs/NATIVE_PORT_PHASE2_3.md, phase 3, step 3.8).
#
#   ninja -C build/native-mac switchwaker
#
# - One executable from the objects of every enabled module (nothing is recompiled), linked
#   with cos_sdk, the Aurora SDK libraries (COS_AURORA_LIBS, public on cos_sdk) and aurora::main,
#   which owns the process entry point and calls the game's main (renamed aurora_main by
#   <aurora/main.h> in m_Do_main.cpp).
# - Object order on the link line: the main.dol units first, then the REL units of rel_units.txt
#   (step 3.5 links them statically), then the audio module (JAudio/JAZelAudio, step 3.7). ld64
#   runs static initialisers in input order, so the REL units see main.dol's globals already
#   initialised, as they did on GameCube when a REL's _prolog ran after boot.
# - Strict: no -undefined dynamic_lookup. Every symbol must resolve here, so `nm -u switchwaker` lists
#   only system and framework symbols.
# - Needs Aurora (cos_sdk) and every module; skipped otherwise. Not part of `all`.
include_guard(GLOBAL)

# The run harness (docs/NATIVE_PORT_PHASE4_6.md, step 6.0): native/src/pc/**/pc_*.cpp (runtime/,
# features/, harness/...; native/src/pc/README.md), globbed recursively into the static library
# cos_pc (API: native/include/pc/pc_harness.h). Compiled like the game units
# (cos_game_headers), since pc_smoke.cpp reads game state. Game units call into it under TARGET_PC,
# so the link census bundle links it too: its symbols must not show up as unresolved.
# The port helpers' sources (native/src/helpers/*.cpp, step 4.0b: OffsetPtr; headers in
# native/include/helpers) go into the same library, so the game and cos_pc_tests link one copy.
file(GLOB_RECURSE _pc_sources CONFIGURE_DEPENDS "${COS_NATIVE_ROOT}/src/pc/pc_*.cpp")
file(GLOB _helper_sources CONFIGURE_DEPENDS "${COS_NATIVE_ROOT}/src/helpers/*.cpp")
list(APPEND _pc_sources ${_helper_sources})
list(SORT _pc_sources)
add_library(cos_pc STATIC ${_pc_sources})
target_link_libraries(cos_pc PRIVATE cos_game_headers)
# pc_assets.cpp: the loader of the runtime assets (GameConfig.cmake, COS_RUNTIME_ASSETS)
if (COS_RUNTIME_ASSETS)
    target_compile_definitions(cos_pc PRIVATE COS_RUNTIME_ASSETS=1)
endif ()
target_include_directories(cos_pc PRIVATE "${COS_NATIVE_ROOT}/src/pc")
# pc_main.cpp (step 6.1) sets cos_sdk's thread hooks (cos_sdk/hooks.h); switchwaker itself links cos_sdk.
target_include_directories(cos_pc PRIVATE "${COS_NATIVE_ROOT}/sdk/include")
# pc_frame.cpp (step 6.2) reads Aurora's events (<aurora/event.h> includes SDL3's headers). Headers
# only: switchwaker links SDL3 through Aurora, and the link census bundle must not gain a library.
if (DEFINED AURORA_SDL3_TARGET AND TARGET ${AURORA_SDL3_TARGET})
    target_include_directories(cos_pc PRIVATE
            $<TARGET_PROPERTY:${AURORA_SDL3_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
endif ()
# pc_shot.cpp (COS_SHOT) reads the presented frame back through Aurora's internal WebGPU state
# (lib/webgpu/gpu.hpp, lib/gfx/render_worker.hpp) and Dawn's C++ headers. Headers only: the symbols
# are in aurora_core and Dawn, which switchwaker and the link census bundle already link through cos_sdk.
set_property(SOURCE "${COS_NATIVE_ROOT}/src/pc/harness/pc_shot.cpp" "${COS_NATIVE_ROOT}/src/pc/game_hooks/pc_capture.cpp"
        APPEND PROPERTY INCLUDE_DIRECTORIES
        "${aurora_SOURCE_DIR}"
        "$<TARGET_PROPERTY:dawn::webgpu_dawn,INTERFACE_INCLUDE_DIRECTORIES>")
# pc_capture.cpp (bug B36, the picto box's photo) also finds the copy texture in Aurora's GX state
# (lib/gx/gx.hpp: abseil's flat_hash_map, fmt through lib/logging.hpp, xxHash through
# lib/gfx/hash.hpp). Headers only.
set_property(SOURCE "${COS_NATIVE_ROOT}/src/pc/game_hooks/pc_capture.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES
        "$<TARGET_PROPERTY:absl::flat_hash_map,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:fmt::fmt,INTERFACE_INCLUDE_DIRECTORIES>"
        "$<TARGET_PROPERTY:xxHash::xxhash,INTERFACE_INCLUDE_DIRECTORIES>")
# pc_main.cpp names Aurora's render worker for the Switch's per-thread CPU times
# (render_worker::enqueue_work; lib/gfx/render_worker.hpp includes only standard headers).
# pc_gpu_opts.cpp (pc_efb_pixel_size, for the options menu) reads the EFB's pixel size (lib/window.hpp, whose SDL
# headers cos_pc already has, and lib/dolphin/vi/vi_internal.hpp); the symbols are aurora_core's.
set_property(SOURCE "${COS_NATIVE_ROOT}/src/pc/runtime/pc_main.cpp" "${COS_NATIVE_ROOT}/src/pc/features/pc_gpu_opts.cpp"
        APPEND PROPERTY INCLUDE_DIRECTORIES "${aurora_SOURCE_DIR}")
# pc_overlay.cpp (COS_FPS_OVERLAY), pc_precompile.cpp (the shader loading screen and indicator) and
# pc_menu.cpp (the options menu) draw with Aurora's ImGui; pc_shot.cpp composites ImGui's draw data
# into COS_SHOT images with COS_SHOT_IMGUI=1. Headers only: aurora_core links imgui.
if (TARGET imgui)
    set(_pc_imgui_sources "${COS_NATIVE_ROOT}/src/pc/features/pc_overlay.cpp" "${COS_NATIVE_ROOT}/src/pc/features/pc_precompile.cpp"
            "${COS_NATIVE_ROOT}/src/pc/features/pc_menu.cpp" "${COS_NATIVE_ROOT}/src/pc/harness/pc_shot.cpp")
    set_property(SOURCE ${_pc_imgui_sources}
            APPEND PROPERTY INCLUDE_DIRECTORIES "$<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>")
    set_property(SOURCE ${_pc_imgui_sources}
            APPEND PROPERTY COMPILE_DEFINITIONS "$<TARGET_PROPERTY:imgui,INTERFACE_COMPILE_DEFINITIONS>")
endif ()
# pc_precompile.cpp reads the bundled pipeline cache's priority count on the desktop hosts
# (COS_PRECOMPILE=boot there). Headers only: aurora_core links sqlite3.
if (TARGET sqlite3)
    set_property(SOURCE "${COS_NATIVE_ROOT}/src/pc/features/pc_precompile.cpp" APPEND PROPERTY INCLUDE_DIRECTORIES
            "$<TARGET_PROPERTY:sqlite3,INTERFACE_INCLUDE_DIRECTORIES>")
endif ()
if (TARGET cos_link_census)
    target_link_libraries(cos_link_census PRIVATE cos_pc)
endif ()

# cos_pc_tests (step 4.0b): host tests of the port helpers (BE(T), OffsetPtr), compiled like a game
# unit. Headless; prints "ok":  ninja cos_pc_tests && build/native-mac/cos_pc_tests
# cos_sdk supplies the SDK functions the helpers call (OSPanic: its default aborts); c_sxyz.cpp (no
# dependencies) the csXyz constructor that BE<csXyz> converts through.
if (TARGET cos_sdk)
    add_executable(cos_pc_tests "${COS_NATIVE_ROOT}/check/pc_tests.cpp"
            "${COS_ROOT}/src/SSystem/SComponent/c_sxyz.cpp")
    target_link_libraries(cos_pc_tests PRIVATE cos_game_headers cos_pc cos_sdk)
    set_target_properties(cos_pc_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif ()

if (NOT TARGET cos_sdk)
    message(STATUS "cos_native: executable switchwaker disabled (needs COS_WITH_AURORA=ON, i.e. cos_sdk)")
    return()
endif ()

set(_exe_missing)
foreach (_m IN LISTS COS_MODULES)
    if (NOT TARGET ${_m})
        list(APPEND _exe_missing ${_m})
    endif ()
endforeach ()
if (_exe_missing)
    message(STATUS "cos_native: executable switchwaker disabled (modules not enabled: ${_exe_missing})")
    return()
endif ()

# REL unit paths (relative to game/src), as census.cmake reads them.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/rel_units.txt" _exe_rel_lines)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/rel_units.txt")
set(_exe_rel_units)
foreach (_l IN LISTS _exe_rel_lines)
    string(STRIP "${_l}" _l)
    if (_l AND NOT _l MATCHES "^#")
        list(APPEND _exe_rel_units "${_l}")
    endif ()
endforeach ()

# Per module: its main.dol objects and its REL objects, split with $<FILTER> over
# $<TARGET_OBJECTS> (chunks of 40 REL names per regex, to keep each regex small).
set(_exe_dol)
set(_exe_rel)
set(_exe_audio)
set(_exe_n_rel 0)
foreach (_m IN LISTS COS_MODULES)
    if (_m STREQUAL "audio")
        list(APPEND _exe_audio "$<TARGET_OBJECTS:${_m}>")
        continue()
    endif ()
    get_property(_srcs GLOBAL PROPERTY "COS_MODULE_SOURCES:${_m}")
    set(_names)
    foreach (_s IN LISTS _srcs)
        file(RELATIVE_PATH _rel "${COS_ROOT}/src" "${_s}")
        if (_rel IN_LIST _exe_rel_units)
            string(REGEX REPLACE "[.](c|cpp)$" "" _stem "${_rel}")
            list(APPEND _names "${_stem}")
        endif ()
    endforeach ()
    list(LENGTH _names _n_names)
    math(EXPR _exe_n_rel "${_exe_n_rel} + ${_n_names}")

    set(_keep "$<TARGET_OBJECTS:${_m}>")
    set(_begin 0)
    while (_begin LESS _n_names)
        list(SUBLIST _names ${_begin} 40 _chunk)
        math(EXPR _begin "${_begin} + 40")
        list(JOIN _chunk "|" _alt)
        set(_rx "/game/src/(${_alt})[.](c|cpp)[.]o$")
        set(_keep "$<FILTER:${_keep},EXCLUDE,${_rx}>")
        list(APPEND _exe_rel "$<FILTER:$<TARGET_OBJECTS:${_m}>,INCLUDE,${_rx}>")
    endwhile ()
    list(APPEND _exe_dol "${_keep}")
endforeach ()

# The objects go in through a response file, in the order above (main.dol, REL, audio); the
# executable's only source is an empty generated unit.
set(_exe_dir "${CMAKE_BINARY_DIR}/cos_exe")
set(_exe_rsp "${_exe_dir}/objects.rsp")
file(GENERATE OUTPUT "${_exe_rsp}"
        CONTENT "$<JOIN:${_exe_dol},\n>\n$<JOIN:${_exe_rel},\n>\n$<JOIN:${_exe_audio},\n>\n")
file(GENERATE OUTPUT "${_exe_dir}/cos_exe_stub.c" CONTENT
        "/* Generated by native/cmake/executable.cmake: the switchwaker executable's only source; the game's\n   objects come in through objects.rsp. */\nint cos_exe_stub;\n")

set(_exe_objects)
foreach (_m IN LISTS COS_MODULES)
    list(APPEND _exe_objects "$<TARGET_OBJECTS:${_m}>")
endforeach ()

add_executable(switchwaker EXCLUDE_FROM_ALL "${_exe_dir}/cos_exe_stub.c")
add_dependencies(switchwaker ${COS_MODULES})
target_link_options(switchwaker PRIVATE "@${_exe_rsp}")
# The process entry point: aurora::main, whose main calls the game's (aurora_main). A platform
# build may set COS_EXE_ENTRY to its own entry library first (the Switch build, switch/native).
if (NOT COS_EXE_ENTRY)
    set(COS_EXE_ENTRY aurora::main)
endif ()
target_link_libraries(switchwaker PRIVATE cos_pc cos_sdk ${COS_EXE_ENTRY})
set_target_properties(switchwaker PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        # The game is C++: link with the C++ driver, so libc++/libc++abi resolve the C++ runtime.
        LINKER_LANGUAGE CXX
        # Relink when an object or the object list changes (they are not sources of the target).
        LINK_DEPENDS "${_exe_rsp};${_exe_objects}")

message(STATUS "cos_native: executable switchwaker from ${_exe_n_rel} REL units after the main.dol "
        "units, then audio")
