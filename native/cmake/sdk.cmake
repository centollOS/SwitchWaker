# cos_sdk: the game-specific part of the GameCube SDK, on top of Aurora's SDK libraries
# (docs/NATIVE_PORT_PHASE2_3.md, step 2.2, decisions D2 and D7).
#
# - Sources are globbed from native/sdk/src/**/*.{c,cpp} with CONFIGURE_DEPENDS, so the phase 2
#   steps that add SDK code (2.6a-2.6f) only add files and never touch CMake.
# - Compiled against Aurora's headers only: never the game's headers or flags (cos_game_headers).
# - cos_sdk_smoke is a headless test program (no window, no GPU). Its tests are globbed from
#   native/sdk/tests/*.cpp and register themselves by name (see tests/smoke.h).
#
# Needs Aurora, so it is only built with COS_WITH_AURORA=ON.

if (NOT COS_WITH_AURORA)
    return()
endif ()

set(COS_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../sdk")
cmake_path(NORMAL_PATH COS_SDK_ROOT)

file(GLOB_RECURSE COS_SDK_SOURCES CONFIGURE_DEPENDS
        "${COS_SDK_ROOT}/src/*.c"
        "${COS_SDK_ROOT}/src/*.cpp")

add_library(cos_sdk STATIC ${COS_SDK_SOURCES})
target_include_directories(cos_sdk PUBLIC "${COS_SDK_ROOT}/include")
# As the game is compiled in Dusklight (GameABIConfig.cmake): the PSMTX* names resolve to the
# C_MTX* functions that aurora_mtx exports. TARGET_PC and AURORA come from aurora::core.
target_compile_definitions(cos_sdk PUBLIC MTX_USE_PS=1)
target_compile_definitions(cos_sdk PRIVATE "COS_AURORA_COMMIT_STR=\"${COS_AURORA_COMMIT}\"")
target_link_libraries(cos_sdk PUBLIC ${COS_AURORA_LIBS})

file(GLOB COS_SDK_SMOKE_SOURCES CONFIGURE_DEPENDS
        "${COS_SDK_ROOT}/tests/*.cpp")
add_executable(cos_sdk_smoke ${COS_SDK_SMOKE_SOURCES})
target_link_libraries(cos_sdk_smoke PRIVATE cos_sdk)
# The program sits at the top of the build directory: build/native-mac/cos_sdk_smoke.
set_target_properties(cos_sdk_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# cos_sdk_smoke_tsan: the same tests and cos_sdk sources built with ThreadSanitizer (step 2.6a:
# the OS thread, mutex, message and alarm code must run race-free). Aurora's libraries are linked
# uninstrumented, without aurora::dvd: it pulls in nod (Rust), and with it the TSan link on macOS
# fails ("too many personality routines for compact unwind": C, C++, Objective-C and Rust). The
# tests do not use DVD, except the DTK part of the "audio" test (step 2.6f): src/audio/DTK.cpp
# drives Aurora's DVD stream commands, so it is left out here and COS_SDK_SMOKE_NO_DVD skips that
# part. Not part of `all`:
#   ninja cos_sdk_smoke_tsan && build/native-mac/cos_sdk_smoke_tsan
# On macOS 26.6 Xcode's clang 17 TSan runtime crashes at start-up; native/sdk/README.md
# ("ThreadSanitizer run") builds it in build/native-mac-tsan with the Command Line Tools clang.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT CMAKE_CROSSCOMPILING)
    set(COS_SDK_TSAN_SOURCES ${COS_SDK_SOURCES})
    list(FILTER COS_SDK_TSAN_SOURCES EXCLUDE REGEX "/src/audio/DTK\\.cpp$")
    add_executable(cos_sdk_smoke_tsan EXCLUDE_FROM_ALL ${COS_SDK_TSAN_SOURCES} ${COS_SDK_SMOKE_SOURCES})
    target_include_directories(cos_sdk_smoke_tsan PRIVATE "${COS_SDK_ROOT}/include")
    target_compile_definitions(cos_sdk_smoke_tsan PRIVATE MTX_USE_PS=1 COS_SDK_SMOKE_NO_DVD=1
            "COS_AURORA_COMMIT_STR=\"${COS_AURORA_COMMIT}\"")
    target_compile_options(cos_sdk_smoke_tsan PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(cos_sdk_smoke_tsan PRIVATE -fsanitize=thread)
    set(COS_SDK_TSAN_LIBS ${COS_AURORA_LIBS})
    list(REMOVE_ITEM COS_SDK_TSAN_LIBS aurora::dvd)
    target_link_libraries(cos_sdk_smoke_tsan PRIVATE ${COS_SDK_TSAN_LIBS})
    set_target_properties(cos_sdk_smoke_tsan PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif ()
