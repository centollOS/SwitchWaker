# The native port (native/: the decompiled game on Aurora) as a Switch NRO: phase 7 of
# docs/NATIVE_PORT_PLAN.md. Configure with devkitPro's Switch toolchain inside the build image;
# scripts/switch/build_native.sh does it (docs/SWITCH_BUILD.md, "Native port").
#
# It reuses the translated port's Switch layer (switch/):
#   - Dawn, built from source for its OpenGL ES backend with the Horizon patches
#     (switch/dawn/dawn.cmake), the Dawn the console has run Aurora with;
#   - SDL 3 as the libnx shim of switch/aurora/sdl3_shim, plus the few functions Aurora 3227d76
#     calls beyond the translated port's Aurora (sdl3_shim_extra.c);
#   - sqlite's Horizon VFS settings (switch/aurora/sqlite_horizon.c), the USB log
#     (switch/source/common/usb_log.c), the NRO packaging and the SD card layout.
# What differs from the translated port:
#   - Aurora is native/'s pin (3227d76, COS_SWITCH_AURORA_SOURCE), copied into the build directory
#     with native/patches/aurora/*.patch (decision H11) and then aurora/patches/*.patch applied;
#   - nod (Rust, no libnx target) is replaced by nod/nod_gcn.cpp, a reader of plain GameCube
#     disc images behind nod's C API, so Aurora's DVD library reads the .iso on the SD card;
#   - the game units compile with clang 19 (clang-launcher.sh), as on the Mac; Aurora, Dawn, the
#     SDK and libnx stay on devkitA64's GCC;
#   - source/: the harness's Switch platform layer (logs, env.txt, crash report, threads).
# Two renderers (COS_SWITCH_RENDERER, docs/DEKO3D_MIGRATION_PLAN.md): gl, the default, is Dawn's
# OpenGL ES backend on Mesa (switchwaker.nro); deko3d is switch/deko (switchwaker_dk.nro, its own SD
# folder /switch/switchwaker_dk/ with its bundled caches, the data in /switch/switchwaker/ shared).
cmake_minimum_required(VERSION 3.28)
project(cos_switch LANGUAGES C CXX)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    message(FATAL_ERROR "Configure with the devkitPro Switch toolchain (scripts/switch/build_native.sh)")
endif ()

set(COS_SWITCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(SET COS_REPO_ROOT NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(COS_SWITCH_AURORA_SOURCE "" CACHE PATH "A pristine checkout of Aurora at native/'s pin (3227d76)")
if (NOT EXISTS "${COS_SWITCH_AURORA_SOURCE}/cmake/aurora_core.cmake")
    message(FATAL_ERROR "Set COS_SWITCH_AURORA_SOURCE to a checkout of Aurora at native/'s pin")
endif ()

set(COS_SWITCH_RENDERER "gl" CACHE STRING
        "Switch renderer: gl (Dawn's OpenGL ES on Mesa, switchwaker.nro) or deko3d (switch/deko, switchwaker_dk.nro)")
set_property(CACHE COS_SWITCH_RENDERER PROPERTY STRINGS gl deko3d)
if (NOT COS_SWITCH_RENDERER MATCHES "^(gl|deko3d)$")
    message(FATAL_ERROR "COS_SWITCH_RENDERER is gl or deko3d, not ${COS_SWITCH_RENDERER}")
endif ()
# libdeko3dd: deko3d's checks of every call and its messages (switch/deko/device.cpp debug_message: a
# misuse or failure is logged before the process ends); the release library aborts with a bare
# 2359-xxxx result. Off by default (SwitchWakerHD's CMakeLists.txt WWHD_DEKO3D_DEBUG_LIB).
option(COS_DK_DEBUG_LIB "deko3d build: link the debug library libdeko3dd" OFF)
if (COS_SWITCH_RENDERER STREQUAL "deko3d")
    set(_nro_default switchwaker_dk)
else ()
    set(_nro_default switchwaker)
endif ()
set(COS_SWITCH_NRO_NAME "${_nro_default}" CACHE STRING "File name of the NRO and its ELF (without .nro)")
message(STATUS "Switch renderer: ${COS_SWITCH_RENDERER} (${COS_SWITCH_NRO_NAME}.nro)")

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# Frame records everywhere: the crash report (source/cos_switch.cpp) and Atmosphère's walk them.
string(APPEND CMAKE_C_FLAGS " -fno-omit-frame-pointer")
string(APPEND CMAKE_CXX_FLAGS " -fno-omit-frame-pointer")

include(FetchContent)

# ---- Dawn (OpenGL ES, patched for Horizon): webgpu_dawn, dawn::webgpu_dawn and abseil ----------
include("${COS_REPO_ROOT}/switch/dawn/dawn.cmake")

# ---- SDL 3: its headers at Aurora's pin and the libnx shim ----------------------------------------
FetchContent_Declare(sdl3_headers
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.10.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(sdl3_headers)
file(GLOB _shim_sources CONFIGURE_DEPENDS "${COS_REPO_ROOT}/switch/aurora/sdl3_shim/*.c")
add_library(SDL3-static STATIC ${_shim_sources} "${COS_SWITCH_DIR}/sdl3_shim_extra.c")
add_library(SDL3::SDL3-static ALIAS SDL3-static)
target_include_directories(SDL3-static PUBLIC "${sdl3_headers_SOURCE_DIR}/include")
target_include_directories(SDL3-static PRIVATE "${COS_REPO_ROOT}/switch/aurora/sdl3_shim")
target_link_libraries(SDL3-static PUBLIC nx)

# ---- nod: plain GameCube disc images (nod/) ----------------------------------------------------------
add_library(cos_switch_nod STATIC "${COS_SWITCH_DIR}/nod/nod_gcn.cpp")
target_include_directories(cos_switch_nod PUBLIC "${COS_SWITCH_DIR}/nod")
add_library(nod::nod ALIAS cos_switch_nod)

# ---- devkitPro portlibs, under the names Aurora's extern/ looks for ---------------------------------
set(SWITCH_PORTLIBS "${DEVKITPRO_ROOT}/portlibs/switch")
function(cos_switch_portlib target)
    add_library(${target} INTERFACE IMPORTED GLOBAL)
    target_include_directories(${target} INTERFACE "${SWITCH_PORTLIBS}/include")
    target_link_directories(${target} INTERFACE "${SWITCH_PORTLIBS}/lib")
endfunction()
cos_switch_portlib(ZLIB::ZLIB)
target_link_libraries(ZLIB::ZLIB INTERFACE z)
cos_switch_portlib(PNG::PNG)
target_link_libraries(PNG::PNG INTERFACE png16 z)

# ---- Tracy: Aurora's profiler stays off; headers only (its client does not build for Horizon) ------
FetchContent_Declare(tracy
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.14.1.zip
        URL_HASH SHA256=908f3a2917fa86a247abfcf85dcf04bad1db6986a4d40f94b70512f3e9e98d5b
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(tracy)
add_library(TracyClient INTERFACE)
target_include_directories(TracyClient INTERFACE "${tracy_SOURCE_DIR}/public")
add_library(Tracy::TracyClient ALIAS TracyClient)

# ---- ImGui: Aurora's pin, without the SDL backends (aurora/patches stub them on Horizon) -----------
FetchContent_Declare(imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz
        URL_HASH SHA256=466fdef9b18de15f0bb6e288e3d00ffa3d82200ec458ce5e4f724a161d9528a5
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
# ImGui's default "open in shell" uses fork/exec/waitpid.
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS)
target_link_libraries(imgui PRIVATE dawn::webgpu_dawn)

# ---- Aurora: native/'s pin with the shared and the Switch patches, in the build directory -----------
# The copy gets native/patches/aurora/*.patch first (decision H11, the patches every Mac build has),
# then aurora/patches/*.patch, each set in name order, through native/cmake's
# aurora_apply_patches.cmake. It is redone when the source path or either patch set changes.
set(COS_SWITCH_AURORA_DIR "${CMAKE_BINARY_DIR}/aurora-switch")
set(_aurora_patch_dirs "${COS_REPO_ROOT}/native/patches/aurora" "${COS_SWITCH_DIR}/aurora/patches")
set(_aurora_stamp "${COS_SWITCH_AURORA_SOURCE}\n")
set(_aurora_count 0)
foreach (_dir IN LISTS _aurora_patch_dirs)
    file(GLOB _patches CONFIGURE_DEPENDS "${_dir}/*.patch")
    list(SORT _patches)
    foreach (_patch IN LISTS _patches)
        file(SHA256 "${_patch}" _hash)
        string(APPEND _aurora_stamp "${_patch}=${_hash}\n")
        math(EXPR _aurora_count "${_aurora_count} + 1")
    endforeach ()
endforeach ()
set(_aurora_stamp_file "${COS_SWITCH_AURORA_DIR}.stamp")
set(_aurora_previous "")
if (EXISTS "${_aurora_stamp_file}")
    file(READ "${_aurora_stamp_file}" _aurora_previous)
endif ()
if (NOT EXISTS "${COS_SWITCH_AURORA_DIR}" OR NOT _aurora_previous STREQUAL _aurora_stamp)
    file(REMOVE_RECURSE "${COS_SWITCH_AURORA_DIR}" "${_aurora_stamp_file}")
    file(COPY "${COS_SWITCH_AURORA_SOURCE}/" DESTINATION "${COS_SWITCH_AURORA_DIR}"
            PATTERN ".git" EXCLUDE)
    foreach (_dir IN LISTS _aurora_patch_dirs)
        execute_process(COMMAND "${CMAKE_COMMAND}" -DCOS_AURORA_SRC=${COS_SWITCH_AURORA_DIR}
                -DCOS_AURORA_PATCH_DIR=${_dir} -P "${COS_REPO_ROOT}/native/cmake/aurora_apply_patches.cmake"
                RESULT_VARIABLE _result)
        if (NOT _result EQUAL 0)
            message(FATAL_ERROR "cos_switch: patching Aurora with ${_dir} failed")
        endif ()
    endforeach ()
    file(WRITE "${_aurora_stamp_file}" "${_aurora_stamp}")
    message(STATUS "cos_switch: Aurora copied with ${_aurora_count} patches (shared, then Switch)")
endif ()
# The copy already carries native/patches/aurora: native/cmake/Aurora.cmake builds it as it is.
set(COS_AURORA_PREPATCHED ON)
set(FETCHCONTENT_SOURCE_DIR_AURORA "${COS_SWITCH_AURORA_DIR}" CACHE PATH "" FORCE)

# Aurora's providers find the targets above and use them as they are.
set(AURORA_SDL3_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_SDL3_LINKAGE static CACHE STRING "" FORCE)
set(AURORA_DAWN_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_DAWN_LINKAGE static CACHE STRING "" FORCE)
set(AURORA_NOD_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_NOD_LINKAGE static CACHE STRING "" FORCE)
# Cache entries stay uncompressed (no zstd for Horizon needed).
set(AURORA_CACHE_USE_ZSTD OFF CACHE BOOL "" FORCE)

# ---- The native port itself ---------------------------------------------------------------------------
# The NRO's entry point (source/cos_switch_main.cpp) instead of aurora::main.
set(COS_EXE_ENTRY cos_switch_main)
add_subdirectory("${COS_REPO_ROOT}/native" native)

# sqlite (Aurora's caches): no WAL or memory-mapped files (no mmap), lock-free VFS on Horizon,
# exclusive locking (one process; the pager keeps its cache between transactions) and temp files in
# memory (no create-then-unlink on the SD card). Aurora's Switch patch 0006 drops the journal file.
if (TARGET sqlite3)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_WAL=1 SQLITE_MAX_MMAP_SIZE=0
            SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_THREADSAFE=1 SQLITE_DEFAULT_LOCKING_MODE=1
            SQLITE_TEMP_STORE=3)
    target_sources(sqlite3 PRIVATE "${COS_REPO_ROOT}/switch/aurora/sqlite_horizon.c")
    get_target_property(_sqlite_sources sqlite3 SOURCES)
    list(GET _sqlite_sources 0 _sqlite_amalgamation)
    get_filename_component(_sqlite_dir "${_sqlite_amalgamation}" DIRECTORY)
    target_include_directories(sqlite3 PRIVATE "${_sqlite_dir}")
endif ()

# Mesa's EGL/GLES, which Dawn reaches through eglGetProcAddress (aurora/patches). By default the
# Mesa scripts/switch/build_mesa.sh builds (COS_SWITCH_MESA_DIR, set by build_native.sh: devkitPro's
# switch-mesa 20.1.0-5 recipe plus switch/mesa/patches, the persistent shader cache); empty, the
# devkitPro package's libraries in portlibs. libdrm_nouveau is the package's either way.
set(COS_SWITCH_MESA_DIR "" CACHE PATH
        "Mesa prefix from scripts/switch/build_mesa.sh (lib/, include/); empty: devkitPro's switch-mesa")
if (COS_SWITCH_MESA_DIR)
    foreach (_lib EGL GLESv2 glapi)
        if (NOT EXISTS "${COS_SWITCH_MESA_DIR}/lib/lib${_lib}.a")
            message(FATAL_ERROR "COS_SWITCH_MESA_DIR has no lib/lib${_lib}.a (scripts/switch/build_mesa.sh)")
        endif ()
    endforeach ()
    set(COS_SWITCH_GL_LIBS "${COS_SWITCH_MESA_DIR}/lib/libEGL.a" "${COS_SWITCH_MESA_DIR}/lib/libGLESv2.a"
            "${COS_SWITCH_MESA_DIR}/lib/libglapi.a" drm_nouveau)
    target_include_directories(aurora_core BEFORE PRIVATE "${COS_SWITCH_MESA_DIR}/include")
    message(STATUS "Mesa: ${COS_SWITCH_MESA_DIR}")
else ()
    set(COS_SWITCH_GL_LIBS EGL GLESv2 glapi drm_nouveau)
    message(STATUS "Mesa: devkitPro's switch-mesa (${SWITCH_PORTLIBS}/lib)")
endif ()
target_include_directories(aurora_core PRIVATE "${SWITCH_PORTLIBS}/include")
target_link_directories(aurora_core PUBLIC "${SWITCH_PORTLIBS}/lib")
target_link_libraries(aurora_core PUBLIC ${COS_SWITCH_GL_LIBS} nx)

# Game units: newlib's <ctype.h> macros collide with decomp member names (include/cos_switch_compat.h).
target_compile_options(cos_game_headers INTERFACE
        "SHELL:-include ${COS_SWITCH_DIR}/include/cos_switch_compat.h")
# pc_shot.cpp (COS_SHOT) and pc_capture.cpp (bug B36) read Dawn's C++ headers; the vendored Dawn carries them on its own
# header target, not on webgpu_dawn as the Mac's prebuilt package does.
set_property(SOURCE "${COS_REPO_ROOT}/native/src/pc/harness/pc_shot.cpp"
        "${COS_REPO_ROOT}/native/src/pc/game_hooks/pc_capture.cpp" DIRECTORY "${COS_REPO_ROOT}/native"
        APPEND PROPERTY INCLUDE_DIRECTORIES
        "$<TARGET_PROPERTY:dawn::dawncpp_headers,INTERFACE_INCLUDE_DIRECTORIES>")

# Dolphin's DSP HLE shim allocates its memory through <sys/mman.h>, which Horizon lacks: the
# translated port's page-aligned heap stand-in (switch/host/compat/sys/mman.h).
# Its zlib (Common::HashAdler32) is devkitPro's.
target_include_directories(cos_dsp_hle PRIVATE "${COS_REPO_ROOT}/switch/host/compat"
        "${SWITCH_PORTLIBS}/include")

# The SDK over Aurora was written on the Mac, where plain char is signed (arm64 Darwin), as it is
# for the game (-fsigned-char, GameConfig.cmake); AArch64 ELF's is unsigned. Keep the Mac's.
target_compile_options(cos_sdk PRIVATE -fsigned-char)
target_compile_options(cos_sdk_gf PRIVATE -fsigned-char)

# Every target compiled like a game unit (it links cos_game_headers) compiles with clang.
function(cos_switch_use_clang dir)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach (_t IN LISTS _targets)
        get_target_property(_type ${_t} TYPE)
        if (_type STREQUAL "INTERFACE_LIBRARY" OR _type STREQUAL "UTILITY")
            continue()
        endif ()
        get_target_property(_libs ${_t} LINK_LIBRARIES)
        if (_libs AND "cos_game_headers" IN_LIST _libs)
            set_target_properties(${_t} PROPERTIES
                    C_COMPILER_LAUNCHER "${COS_SWITCH_DIR}/clang-launcher.sh"
                    CXX_COMPILER_LAUNCHER "${COS_SWITCH_DIR}/clang-launcher.sh")
        endif ()
    endforeach ()
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach (_d IN LISTS _subdirs)
        cos_switch_use_clang("${_d}")
    endforeach ()
endfunction()
cos_switch_use_clang("${COS_REPO_ROOT}/native")

# ---- The harness's Switch platform layer ---------------------------------------------------------------
add_library(cos_switch_platform STATIC
        "${COS_SWITCH_DIR}/source/cos_switch.cpp"
        "${COS_SWITCH_DIR}/source/cos_switch_stats.cpp"
        "${COS_SWITCH_DIR}/source/cos_shader_cache.cpp"
        "${COS_SWITCH_DIR}/source/thread_wrap.c"
        "${COS_SWITCH_DIR}/source/nv_wrap.c"
        "${COS_REPO_ROOT}/switch/source/common/usb_log.c")
target_include_directories(cos_switch_platform PUBLIC "${COS_SWITCH_DIR}/source"
        PRIVATE "${COS_REPO_ROOT}/switch/source/common")
target_link_libraries(cos_switch_platform PUBLIC nx)
# cos_shader_cache.cpp logs the counters of the Mesa build_mesa.sh builds (include/mesa_switch.h).
if (COS_SWITCH_RENDERER STREQUAL "gl" AND COS_SWITCH_MESA_DIR AND EXISTS "${COS_SWITCH_MESA_DIR}/include/mesa_switch.h")
    target_compile_definitions(cos_switch_platform PRIVATE COS_SWITCH_MESA_STATS=1)
    target_include_directories(cos_switch_platform PRIVATE "${COS_SWITCH_MESA_DIR}/include")
endif ()
# cos_switch_stats.cpp reads Aurora's Switch counters (aurora/patches/0005), Dawn's GL queue
# counters and the disc reader's (nod/), which the executable links.
target_include_directories(cos_switch_platform PRIVATE "${COS_SWITCH_AURORA_DIR}/include")
target_link_libraries(cos_switch_platform PUBLIC aurora_gx webgpu_dawn cos_switch_nod)
add_library(cos_switch_main STATIC "${COS_SWITCH_DIR}/source/cos_switch_main.cpp")
target_link_libraries(cos_switch_main PUBLIC cos_switch_platform)
# The harness (cos_pc) calls the platform layer (pc_crash.cpp, pc_harness.cpp, pc_main.cpp).
target_include_directories(cos_pc PRIVATE "${COS_SWITCH_DIR}/source")
target_link_libraries(cos_pc PRIVATE cos_switch_platform)

# ---- The deko3d renderer (switch/deko; docs/DEKO3D_MIGRATION_PLAN.md phase 2) ---------------------------
# Aurora with AURORA_GFX_DEKO3D (aurora/patches/0013) records against Dawn's Null device and calls
# switch/deko/dk_aurora.h for the device, the swapchain, the present pass and ImGui. Dawn keeps its GL
# backend (Mesa stays linked, unused) until the GL path goes (plan phase 6).
if (COS_SWITCH_RENDERER STREQUAL "deko3d")
    set(_dk_dir "${COS_REPO_ROOT}/switch/deko")
    set(_dk_nro_dir "/switch/switchwaker_dk")
    # The renderer's own shaders (ImGui, the 3x5 text): GLSL compiled by devkitPro's uam at build time and
    # embedded (bin2s), named <name>_vsh.glsl / <name>_fsh.glsl. The test pattern's shaders are WGSL
    # compiled offline into the DKSH cache (shaders/dk_test_pattern.wgsl, native/tools/dksh_cache).
    file(GLOB _dk_glsl CONFIGURE_DEPENDS "${_dk_dir}/shaders/*.glsl")
    set(_dk_dksh)
    foreach (_glsl IN LISTS _dk_glsl)
        get_filename_component(_name "${_glsl}" NAME_WE)
        if (_name MATCHES "_vsh$")
            set(_stage vert)
        elseif (_name MATCHES "_fsh$")
            set(_stage frag)
        else ()
            message(FATAL_ERROR "${_glsl}: deko3d shaders are named <name>_vsh.glsl or <name>_fsh.glsl")
        endif ()
        nx_add_shader_program(cos_dksh_${_name} "${_glsl}" ${_stage} OUTPUT "${CMAKE_BINARY_DIR}/dksh/${_name}.dksh")
        list(APPEND _dk_dksh cos_dksh_${_name})
    endforeach ()
    dkp_add_embedded_binary_library(cos_switch_deko_shaders ${_dk_dksh})
    add_library(cos_switch_deko STATIC
            "${_dk_dir}/device.cpp"
            "${_dk_dir}/memory.cpp"
            "${_dk_dir}/descriptors.cpp"
            "${_dk_dir}/overlay.cpp"
            "${_dk_dir}/pattern.cpp"
            "${_dk_dir}/shader_cache.cpp"
            "${_dk_dir}/dksh_file.cpp")
    target_include_directories(cos_switch_deko PUBLIC "${_dk_dir}")
    target_compile_definitions(cos_switch_deko PRIVATE "COS_SWITCH_NRO_DIR=\"${_dk_nro_dir}\""
            $<$<BOOL:${COS_DK_DEBUG_LIB}>:COS_DK_DEBUG_LIB=1>)
    if (COS_DK_DEBUG_LIB)
        set(_dk_lib deko3dd)
    else ()
        set(_dk_lib deko3d)
    endif ()
    target_link_libraries(cos_switch_deko PRIVATE cos_switch_deko_shaders imgui ZLIB::ZLIB xxHash::xxhash
            PUBLIC ${_dk_lib} nx)
    target_compile_definitions(aurora_core PRIVATE AURORA_GFX_DEKO3D=1)
    target_include_directories(aurora_core PRIVATE "${_dk_dir}")
    target_link_libraries(aurora_core PUBLIC cos_switch_deko)
    # The NRO's own folder: SDL_GetBasePath (the bundled pipeline cache) and the DKSH cache next to it; the
    # log is COS_SWITCH_ROOT/switchwaker_dk.log (source/cos_switch.h).
    target_compile_definitions(SDL3-static PRIVATE "COS_SWITCH_NRO_DIR=\"${_dk_nro_dir}\"")
    target_compile_definitions(cos_switch_platform PUBLIC COS_SWITCH_DEKO3D=1
            "COS_SWITCH_NRO_NAME=\"${COS_SWITCH_NRO_NAME}\"")
    message(STATUS "deko3d: ${_dk_lib}, NRO folder ${_dk_nro_dir}")
endif ()

# ---- The NRO ---------------------------------------------------------------------------------------------
if (NOT TARGET switchwaker)
    message(FATAL_ERROR "native/ did not define the executable switchwaker (modules or cos_sdk missing)")
endif ()
target_link_libraries(switchwaker PRIVATE cos_switch_platform ${COS_SWITCH_GL_LIBS})
# Every thread gets a stack Dawn's shader compiler fits in and runs on the application cores
# (source/thread_wrap.c).
target_link_options(switchwaker PRIVATE "LINKER:--wrap=pthread_create")
# Mesa's and libdrm_nouveau's push-buffer space, GPU waits and submissions, timed per thread for
# the perf-switch lines (source/nv_wrap.c).
target_link_options(switchwaker PRIVATE "LINKER:--wrap=nouveau_pushbuf_space" "LINKER:--wrap=nvFenceWait"
        "LINKER:--wrap=nvGpuChannelKickoff" "LINKER:--wrap=nvGpuChannelAppendEntry")
# abort() writes the logs out and a backtrace first (source/cos_switch.cpp).
target_link_options(switchwaker PRIVATE "LINKER:--wrap=abort")
set_target_properties(switchwaker PROPERTIES OUTPUT_NAME "${COS_SWITCH_NRO_NAME}" SUFFIX ".elf")
# The version the Homebrew Menu shows: scripts/switch/build_native.sh passes git describe's (the tag).
set(COS_SWITCH_VERSION "0.0.0" CACHE STRING "NRO display version (NACP, at most 15 bytes)")
if (COS_SWITCH_RENDERER STREQUAL "deko3d" AND COS_DK_DEBUG_LIB)
    set(_nacp_name "SwitchWaker (deko3d debug)")
elseif (COS_SWITCH_RENDERER STREQUAL "deko3d")
    set(_nacp_name "SwitchWaker (deko3d)")
else ()
    set(_nacp_name "SwitchWaker")
endif ()
nx_generate_nacp(cos_native.nacp
        NAME "${_nacp_name}" AUTHOR "SwitchWaker contributors" VERSION "${COS_SWITCH_VERSION}")
nx_create_nro(cos_nro
        TARGET switchwaker
        OUTPUT "${CMAKE_BINARY_DIR}/${COS_SWITCH_NRO_NAME}.nro"
        NACP cos_native.nacp
        # Original art drawn by scripts/switch/make_icon.py; the HOME-menu forwarder uses it too
        # (scripts/switch/build_forwarder.sh).
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/icon/icon.jpg")
