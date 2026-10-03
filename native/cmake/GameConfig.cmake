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
#
# SDK headers (docs/NATIVE_PORT_PHASE2_3.md, steps 2.3 and 2.8, decision D2): Aurora's headers are
# the only SDK headers. native/include/sdk holds forwarders for the SDK header names only the game has
# (dolphin/os/OS.h -> <dolphin/os.h> + game-only declarations) and comes before Aurora, which wins
# every name both have, so Aurora's own includes stay consistent. game/include/dolphin must
# never be reached (check/check_sdk_shadow.sh). The decomp's own SDK headers (phase 1's
# COS_SDK_HEADERS=decomp mode) were dropped for TARGET_PC in step 2.8.
if (DEFINED COS_SDK_HEADERS AND NOT COS_SDK_HEADERS STREQUAL "aurora")
    message(FATAL_ERROR "COS_SDK_HEADERS=${COS_SDK_HEADERS}: the decomp SDK header mode was removed "
            "in phase 2 step 2.8; the game always compiles against Aurora's headers. "
            "Drop -DCOS_SDK_HEADERS (or reconfigure with --fresh).")
endif ()
unset(COS_SDK_HEADERS CACHE)
include(${CMAKE_CURRENT_LIST_DIR}/Aurora.cmake)
if (NOT COS_WITH_AURORA)
    message(FATAL_ERROR "COS_WITH_AURORA=OFF is no longer supported: the game compiles against "
            "Aurora's SDK headers (phase 2 step 2.8)")
endif ()
set(COS_SDK_INCLUDE_DIRS
        ${COS_NATIVE_ROOT}/include/sdk
        ${aurora_SOURCE_DIR}/include)
# As Dusklight compiles the game (GameABIConfig.cmake): PSMTX* resolve to aurora_mtx's C_MTX*.
list(APPEND COS_GAME_COMPILE_DEFS MTX_USE_PS=1)

set(COS_GAME_INCLUDE_DIRS
        ${COS_NATIVE_ROOT}/include
        ${COS_SDK_INCLUDE_DIRS}
        ${COS_ROOT}/include
        ${COS_ASSETS_DIR}/include
        ${COS_ASSETS_DIR}
        ${COS_ROOT}/src
        ${COS_NATIVE_ROOT}/include/pc/msl)

set(COS_PC_CONFIG_HEADER ${COS_NATIVE_ROOT}/include/pc/cos_pc_config.h)

set(COS_GAME_COMPILE_OPTIONS
        # Force-included first in every unit: what Metrowerks and MSL provided implicitly.
        "SHELL:-include ${COS_PC_CONFIG_HEADER}"
        # The game's dolphin/types.h reached every unit through global.h; its names Aurora lacks (uint,
        # READU32_BE, FLOAT_MIN/MAX) come from this header instead, right after the PC config header.
        "SHELL:-include ${COS_NATIVE_ROOT}/include/sdk/cos_sdk_extras.h"
        # Match the GameCube (and x86): plain char is signed. Same as Dusklight on ARM.
        -fsigned-char
        # Decision H8 (docs/NATIVE_PORT_PHASE4_6.md): the decompiled code type-puns through pointer
        # casts everywhere (MWCC never applied type-based alias analysis to it), so clang must not
        # either. Costs a little optimisation; Dusklight does not use the flag.
        -fno-strict-aliasing
        # MWCC was invoked with -Cpp_exceptions off and -RTTI off.
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        # The game checks `new` for NULL everywhere (a full JKRSolidHeap returns NULL, and
        # fopAcM_entrySolidHeap retries with a bigger heap when CreateHeap fails). MWCC tested the
        # result before running constructors or writing the new[] array cookie; clang assumes a
        # non-noexcept operator new never returns NULL and stores through it unchecked (SIGSEGV
        # addr=0x0 in cBgW::SetTri's new[] on a full actor heap). -fcheck-new restores the check.
        $<$<COMPILE_LANGUAGE:CXX>:-fcheck-new>
        # The decompiled code sometimes reads a local that only one branch wrote (MWCC left stack
        # garbage there). Clang treats that read as undefined behaviour: an uninitialized value
        # passed to a function argument (noundef) makes the path unreachable, so the branch's
        # NULL check is deleted (daWarphr_c::_draw dereferenced a NULL demo camera). Give every
        # automatic variable defined contents so the branches stay as written. GCC 12+ accepts it.
        -ftrivial-auto-var-init=zero
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

# Phase 4 inventory (docs/NATIVE_PORT_PHASE4_6.md, step 4.0a): the 64-bit and missing-return
# diagnostics, appended after the shared flags so no -Wno-... above can hide them. Turning the
# option on or off changes every game unit's flags, so the next build recompiles all of them and
# its log is a complete warning census: native/tools/phase4_inventory.py --log <build log>.
option(COS_PHASE4_WARNINGS "Add the phase 4 64-bit and return-type warnings to every game unit" OFF)
if (COS_PHASE4_WARNINGS)
    list(APPEND COS_GAME_COMPILE_OPTIONS
            -Wint-to-pointer-cast
            -Wpointer-to-int-cast
            -Wint-to-void-pointer-cast
            -Wreturn-type
            -Wfortify-source)
    message(STATUS "cos_native: phase 4 warnings on (COS_PHASE4_WARNINGS)")
endif ()

add_library(cos_game_headers INTERFACE)
target_compile_definitions(cos_game_headers INTERFACE ${COS_GAME_COMPILE_DEFS})
target_include_directories(cos_game_headers INTERFACE ${COS_GAME_INCLUDE_DIRS})
target_compile_options(cos_game_headers INTERFACE ${COS_GAME_COMPILE_OPTIONS})

if (NOT EXISTS "${COS_ASSETS_DIR}")
    message(STATUS "cos_native: asset headers not generated yet (${COS_ASSETS_DIR}); "
            "units that include assets/ or res/Object/ headers will not compile until they are")
endif ()
