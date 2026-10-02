# Game compile configuration, the native counterpart of the decomp's configure.py cflags and of
# Dusklight's cmake/GameABIConfig.cmake. Everything the game units share goes on the INTERFACE
# library cos_game_headers; each module links it.
include_guard(GLOBAL)

# GZLE01 is VERSIONS.index("GZLE01") == 2 (VERSION_USA) in the decomp's configure.py.
set(COS_VERSION 2 CACHE STRING "Game version: 0 D44J01, 1 GZLJ01, 2 GZLE01, 3 GZLP01")
set(COS_GAME_ID GZLE01 CACHE STRING "Disc ID whose generated asset headers are used")

# Generated asset headers ("assets/...", "res/Object/...") come from the player's disc and live
# under the build directory, never in git (native/README.md, "Asset headers").
set(COS_ASSETS_DIR "${CMAKE_BINARY_DIR}/assets/${COS_GAME_ID}" CACHE PATH
        "Directory holding the asset headers generated from the player's disc")

set(COS_GAME_COMPILE_DEFS
        TARGET_PC=1
        VERSION=${COS_VERSION}
        NDEBUG=1)

# Same order as configure.py's -i list, minus the MSL/Runtime/MetroTRK directories: the host C
# and C++ libraries replace MSL. native/include/pc/msl holds thin shims for the MSL-only header
# names the game includes (algorithm.h, new.h...).
set(COS_GAME_INCLUDE_DIRS
        ${COS_NATIVE_ROOT}/include
        ${COS_ROOT}/include
        ${COS_ASSETS_DIR}/include
        ${COS_ASSETS_DIR}
        ${COS_ROOT}/src
        ${COS_NATIVE_ROOT}/include/pc/msl)

set(COS_PC_CONFIG_HEADER ${COS_NATIVE_ROOT}/include/pc/cos_pc_config.h)

set(COS_GAME_COMPILE_OPTIONS
        # Force-included first in every unit: what Metrowerks and MSL provided implicitly.
        "SHELL:-include ${COS_PC_CONFIG_HEADER}"
        # Match the GameCube (and x86): plain char is signed. Same as Dusklight on ARM.
        -fsigned-char
        # MWCC was invoked with -Cpp_exceptions off and -RTTI off.
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        # Diagnostics only (no code change). Same set as Dusklight, plus the MWCC-isms clang
        # rejects by default but can accept with identical meaning.
        -Wno-multichar                       # 'ABCD' constants: identical big-endian encoding
        -Wno-unknown-pragmas                 # #pragma optimization_level, scheduling, ...
        -Wno-deprecated-declarations
        -Wno-declaration-after-statement
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-trigraphs>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-pod-varargs>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>           # 'register' storage class (C++17)
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>    # implicit narrowing in braces, as MWCC
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-volatile>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-float-conversion>
        -Wno-parentheses
        -Wno-shift-op-parentheses
        -Wno-logical-op-parentheses
        -Wno-bitwise-op-parentheses
        -Wno-dangling-else
        -Wno-unused-value
        -Wno-tautological-compare
        -Wno-tautological-constant-out-of-range-compare
        -Wno-pointer-sign
        -Wno-ignored-attributes
        -Wno-writable-strings
        # 64-bit diagnostics (int-to-pointer-cast, ...) stay visible on purpose: phase 4 input.
        -ferror-limit=50)

add_library(cos_game_headers INTERFACE)
target_compile_definitions(cos_game_headers INTERFACE ${COS_GAME_COMPILE_DEFS})
target_include_directories(cos_game_headers INTERFACE ${COS_GAME_INCLUDE_DIRS})
target_compile_options(cos_game_headers INTERFACE ${COS_GAME_COMPILE_OPTIONS})

if (NOT EXISTS "${COS_ASSETS_DIR}")
    message(STATUS "cos_native: asset headers not generated yet (${COS_ASSETS_DIR}); "
            "units that include assets/ or res/Object/ headers will not compile until they are")
endif ()
