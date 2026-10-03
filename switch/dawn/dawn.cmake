# Dawn for the Switch: the pinned source, built for its OpenGL ES backend only,
# with the Horizon/newlib patches in patches/ applied to the fetched copy.
# Included by the Dawn probe (CMakeLists.txt here) and by the game build
# (switch/aurora). Defines webgpu_dawn / dawn::webgpu_dawn and the abseil targets.

include(FetchContent)

set(DEVKITPRO_ROOT "$ENV{DEVKITPRO}" CACHE PATH "devkitPro installation root")
if(NOT DEVKITPRO_ROOT)
    set(DEVKITPRO_ROOT "/opt/devkitpro")
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    # Horizon NROs have no libdl; the Switch-specific Dawn patch uses direct
    # EGL proc loading and disables all DynamicLib dlopen/dlsym paths.
    set(CMAKE_DL_LIBS "")
endif()

# Build only Dawn's native OpenGL ES path. This intentionally uses an offscreen
# Dawn render target; it does not claim that Dawn can present to a libnx window.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC CACHE STRING "" FORCE)
set(DAWN_BUILD_PROTOBUF OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_NODE_BINDINGS OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_OPENGLES ON CACHE BOOL "" FORCE)
set(DAWN_ENABLE_DESKTOP_GL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_VULKAN OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D11 OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_METAL OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_SPIRV_VALIDATION OFF CACHE BOOL "" FORCE)
set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)
set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_READER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_SPV_WRITER OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_VALIDATOR OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_IR_BINARY OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TINTD OFF CACHE BOOL "" FORCE)
# Dawn's OpenGLES compute pipeline uses Tint's Null writer for workgroup metadata.
set(TINT_BUILD_NULL_WRITER ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    dawn
    URL https://github.com/encounter/dawn/archive/266c1cf8de969a364afa4fa49311631fc99a881e.tar.gz
    URL_HASH SHA256=03ca9de39e1b534c9a443ede66ce8fcf61521edfa7d526f9356972241cbd957d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(dawn)

if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    find_program(PATCH_EXECUTABLE patch REQUIRED)
    set(ABSL_SOURCE_DIR "${dawn_SOURCE_DIR}/third_party/abseil-cpp")
    file(READ "${ABSL_SOURCE_DIR}/absl/base/internal/sysinfo.cc" ABSL_SYSINFO_SOURCE)
    if(NOT ABSL_SYSINFO_SOURCE MATCHES "svcGetThreadId")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/abseil-switch.patch"
            WORKING_DIRECTORY "${ABSL_SOURCE_DIR}"
            RESULT_VARIABLE ABSL_PATCH_RESULT
            OUTPUT_VARIABLE ABSL_PATCH_OUTPUT
            ERROR_VARIABLE ABSL_PATCH_ERROR
        )
        if(NOT ABSL_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Abseil Switch compatibility patch:\n"
                "${ABSL_PATCH_OUTPUT}${ABSL_PATCH_ERROR}")
        endif()
    endif()

    set(ABSL_ELF_IMAGE_HEADER
        "${ABSL_SOURCE_DIR}/absl/debugging/internal/elf_mem_image.h")
    file(READ "${ABSL_ELF_IMAGE_HEADER}" ABSL_ELF_IMAGE_SOURCE)
    if(NOT ABSL_ELF_IMAGE_SOURCE MATCHES "!defined\\(__SWITCH__\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/abseil-switch-elf.patch"
            WORKING_DIRECTORY "${ABSL_SOURCE_DIR}"
            RESULT_VARIABLE ABSL_ELF_PATCH_RESULT
            OUTPUT_VARIABLE ABSL_ELF_PATCH_OUTPUT
            ERROR_VARIABLE ABSL_ELF_PATCH_ERROR
        )
        if(NOT ABSL_ELF_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Abseil Switch ELF compatibility patch:\n"
                "${ABSL_ELF_PATCH_OUTPUT}${ABSL_ELF_PATCH_ERROR}")
        endif()
    endif()

    set(ABSL_TIMEZONE_SOURCE
        "${ABSL_SOURCE_DIR}/absl/time/internal/cctz/src/time_zone_libc.cc")
    file(READ "${ABSL_TIMEZONE_SOURCE}" ABSL_TIMEZONE_SOURCE_TEXT)
    if(NOT ABSL_TIMEZONE_SOURCE_TEXT MATCHES "extern char \\*_tzname\\[2\\]")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/abseil-switch-timezone-newlib.patch"
            WORKING_DIRECTORY "${ABSL_SOURCE_DIR}"
            RESULT_VARIABLE ABSL_TIMEZONE_PATCH_RESULT
            OUTPUT_VARIABLE ABSL_TIMEZONE_PATCH_OUTPUT
            ERROR_VARIABLE ABSL_TIMEZONE_PATCH_ERROR
        )
        if(NOT ABSL_TIMEZONE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Abseil Switch timezone compatibility patch:\n"
                "${ABSL_TIMEZONE_PATCH_OUTPUT}${ABSL_TIMEZONE_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_PLATFORM_HEADER "${dawn_SOURCE_DIR}/src/utils/platform.h")
    file(READ "${DAWN_PLATFORM_HEADER}" DAWN_PLATFORM_SOURCE)
    if(NOT DAWN_PLATFORM_SOURCE MATCHES "DAWN_PLATFORM_IS_SWITCH 1")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-platform.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_PLATFORM_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_PLATFORM_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_PLATFORM_PATCH_ERROR
        )
        if(NOT DAWN_PLATFORM_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn Switch platform patch:\n"
                "${DAWN_PLATFORM_PATCH_OUTPUT}${DAWN_PLATFORM_PATCH_ERROR}")
        endif()
    endif()

    set(TINT_TEXT_GENERATOR
        "${dawn_SOURCE_DIR}/src/tint/utils/text_generator/text_generator.cc")
    file(READ "${TINT_TEXT_GENERATOR}" TINT_TEXT_GENERATOR_SOURCE)
    if(TINT_TEXT_GENERATOR_SOURCE MATCHES "!isascii\\(c\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-ascii.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE TINT_ASCII_PATCH_RESULT
            OUTPUT_VARIABLE TINT_ASCII_PATCH_OUTPUT
            ERROR_VARIABLE TINT_ASCII_PATCH_ERROR
        )
        if(NOT TINT_ASCII_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn/Tint Switch ASCII patch:\n"
                "${TINT_ASCII_PATCH_OUTPUT}${TINT_ASCII_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_DYNAMIC_LIB_SOURCE
        "${dawn_SOURCE_DIR}/src/dawn/common/DynamicLib.cpp")
    file(READ "${DAWN_DYNAMIC_LIB_SOURCE}" DAWN_DYNAMIC_LIB_TEXT)
    if(NOT DAWN_DYNAMIC_LIB_TEXT MATCHES
       "DAWN_PLATFORM_IS\\(POSIX\\) && !DAWN_PLATFORM_IS\\(SWITCH\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-dynamiclib.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_DYNAMIC_LIB_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_DYNAMIC_LIB_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_DYNAMIC_LIB_PATCH_ERROR
        )
        if(NOT DAWN_DYNAMIC_LIB_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn Switch dynamic-library patch:\n"
                "${DAWN_DYNAMIC_LIB_PATCH_OUTPUT}${DAWN_DYNAMIC_LIB_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_SYSTEM_UTILS_SOURCE
        "${dawn_SOURCE_DIR}/src/dawn/common/SystemUtils.cpp")
    file(READ "${DAWN_SYSTEM_UTILS_SOURCE}" DAWN_SYSTEM_UTILS_TEXT)
    if(NOT DAWN_SYSTEM_UTILS_TEXT MATCHES "DAWN_PLATFORM_IS\\(SWITCH\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-system-utils.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_SYSTEM_UTILS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_SYSTEM_UTILS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_SYSTEM_UTILS_PATCH_ERROR
        )
        if(NOT DAWN_SYSTEM_UTILS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn Switch SystemUtils patch:\n"
                "${DAWN_SYSTEM_UTILS_PATCH_OUTPUT}${DAWN_SYSTEM_UTILS_PATCH_ERROR}")
        endif()
    endif()

    file(READ "${DAWN_SYSTEM_UTILS_SOURCE}" DAWN_SYSTEM_UTILS_TEXT)
    if(NOT DAWN_SYSTEM_UTILS_TEXT MATCHES "static_cast<void>\\(variableName\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-environment.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_ENV_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_ENV_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_ENV_PATCH_ERROR
        )
        if(NOT DAWN_ENV_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn Switch environment patch:\n"
                "${DAWN_ENV_PATCH_OUTPUT}${DAWN_ENV_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_SLEEP_SOURCE "${dawn_SOURCE_DIR}/src/dawn/utils/SystemUtils.cpp")
    file(READ "${DAWN_SLEEP_SOURCE}" DAWN_SLEEP_SOURCE_TEXT)
    if(NOT DAWN_SLEEP_SOURCE_TEXT MATCHES "svcSleepThread")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-sleep.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_SLEEP_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_SLEEP_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_SLEEP_PATCH_ERROR
        )
        if(NOT DAWN_SLEEP_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn Switch sleep patch:\n"
                "${DAWN_SLEEP_PATCH_OUTPUT}${DAWN_SLEEP_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_EGL_PLATFORM_HEADER
        "${dawn_SOURCE_DIR}/third_party/EGL-Registry/src/api/EGL/eglplatform.h")
    file(READ "${DAWN_EGL_PLATFORM_HEADER}" DAWN_EGL_PLATFORM_TEXT)
    if(NOT DAWN_EGL_PLATFORM_TEXT MATCHES "defined\\(__SWITCH__\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-egl-platform.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_EGL_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_EGL_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_EGL_PATCH_ERROR
        )
        if(NOT DAWN_EGL_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn EGL Switch platform patch:\n"
                "${DAWN_EGL_PATCH_OUTPUT}${DAWN_EGL_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_OPENGL_BACKEND_SOURCE
        "${dawn_SOURCE_DIR}/src/dawn/native/opengl/BackendGL.cpp")
    file(READ "${DAWN_OPENGL_BACKEND_SOURCE}" DAWN_OPENGL_BACKEND_TEXT)
    if(DAWN_OPENGL_BACKEND_TEXT MATCHES
       "EGL_EXT_create_context_robustness is required")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-allow-no-context-robustness.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_ROBUSTNESS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_ROBUSTNESS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_ROBUSTNESS_PATCH_ERROR
        )
        if(NOT DAWN_ROBUSTNESS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch diagnostic robustness patch:\n"
                "${DAWN_ROBUSTNESS_PATCH_OUTPUT}${DAWN_ROBUSTNESS_PATCH_ERROR}")
        endif()
    endif()

    file(READ "${DAWN_OPENGL_BACKEND_SOURCE}" DAWN_OPENGL_BACKEND_TEXT)
    if(DAWN_OPENGL_BACKEND_TEXT MATCHES
           "EGL_KHR_fence_sync or EGL_KHR_reusable_sync must be supported" AND
       NOT DAWN_OPENGL_BACKEND_TEXT MATCHES "Diagnostic-only fallback: QueueGL uses glFinish")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-allow-native-fence-sync.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_NATIVE_FENCE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_NATIVE_FENCE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_NATIVE_FENCE_PATCH_ERROR
        )
        if(NOT DAWN_NATIVE_FENCE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch native-fence capability patch:\n"
                "${DAWN_NATIVE_FENCE_PATCH_OUTPUT}${DAWN_NATIVE_FENCE_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_OPENGL_QUEUE_SOURCE
        "${dawn_SOURCE_DIR}/src/dawn/native/opengl/QueueGL.cpp")
    file(READ "${DAWN_OPENGL_QUEUE_SOURCE}" DAWN_OPENGL_QUEUE_TEXT)
    if(NOT DAWN_OPENGL_QUEUE_TEXT MATCHES "mEGLSyncType == EGL_NONE")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-synchronous-queue.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_QUEUE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_QUEUE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_QUEUE_PATCH_ERROR
        )
        if(NOT DAWN_QUEUE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch synchronous diagnostic queue patch:\n"
                "${DAWN_QUEUE_PATCH_OUTPUT}${DAWN_QUEUE_PATCH_ERROR}")
        endif()
    endif()

    # On top of the synchronous queue: GLES 3.0 sync objects instead of a glFinish per
    # submission (COS_SWITCH_GL_FINISH=1 at run time brings the glFinish back).
    file(READ "${DAWN_OPENGL_QUEUE_SOURCE}" DAWN_OPENGL_QUEUE_TEXT)
    if(NOT DAWN_OPENGL_QUEUE_TEXT MATCHES "mGLFencesInFlight")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-fence-queue.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_FENCE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_FENCE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_FENCE_PATCH_ERROR
        )
        if(NOT DAWN_GL_FENCE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL fence queue patch:\n"
                "${DAWN_GL_FENCE_PATCH_OUTPUT}${DAWN_GL_FENCE_PATCH_ERROR}")
        endif()
    endif()

    set(DAWN_WGPU_HELPERS_SOURCE
        "${dawn_SOURCE_DIR}/src/dawn/native/utils/WGPUHelpers.cpp")
    file(READ "${DAWN_WGPU_HELPERS_SOURCE}" DAWN_WGPU_HELPERS_TEXT)
    if(DAWN_WGPU_HELPERS_TEXT MATCHES "strnlen\\(")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-strnlen.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_STRNLEN_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_STRNLEN_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_STRNLEN_PATCH_ERROR
        )
        if(NOT DAWN_STRNLEN_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the pinned Dawn Switch bounded-string patch:\n"
                "${DAWN_STRNLEN_PATCH_OUTPUT}${DAWN_STRNLEN_PATCH_ERROR}")
        endif()
    endif()

    # A window surface on libnx's NWindow, through the Android native window
    # source and an EGL window surface.
    set(DAWN_SWAPCHAIN_EGL_SOURCE "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwapChainEGL.cpp")
    file(READ "${DAWN_SWAPCHAIN_EGL_SOURCE}" DAWN_SWAPCHAIN_EGL_TEXT)
    if(NOT DAWN_SWAPCHAIN_EGL_TEXT MATCHES "DAWN_PLATFORM_IS\\(SWITCH\\)")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-nwindow-surface.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_NWINDOW_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_NWINDOW_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_NWINDOW_PATCH_ERROR
        )
        if(NOT DAWN_NWINDOW_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch NWindow surface patch:\n"
                "${DAWN_NWINDOW_PATCH_OUTPUT}${DAWN_NWINDOW_PATCH_ERROR}")
        endif()
    endif()
endif()
