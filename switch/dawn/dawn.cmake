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

    # Counters and timers of the GL replay of a submission (render passes, draws, bind groups,
    # texture binds, glTexParameteri, uniform uploads, buffer copies; Execute, flush, context
    # release), read by the native port's perf-switch lines through dawn_switch_gl_cmd_stats.
    if(NOT EXISTS "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchStatsGL.h")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-command-stats.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_STATS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_STATS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_STATS_PATCH_ERROR
        )
        if(NOT DAWN_GL_STATS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL command statistics patch:\n"
                "${DAWN_GL_STATS_PATCH_OUTPUT}${DAWN_GL_STATS_PATCH_ERROR}")
        endif()
    endif()

    # On top of the statistics patch: CommandBuffer::Execute's LazyClearSyncScope lambda captured
    # the OpenGLFunctions table by value, copying it (and its std::unordered_set<std::string> of
    # GL extension names: hundreds of newlib mallocs) on every queue submit. Capture by reference
    # (the lambda never outlives Execute).
    set(DAWN_OPENGL_COMMAND_BUFFER "${dawn_SOURCE_DIR}/src/dawn/native/opengl/CommandBufferGL.cpp")
    file(READ "${DAWN_OPENGL_COMMAND_BUFFER}" DAWN_OPENGL_COMMAND_BUFFER_TEXT)
    if(DAWN_OPENGL_COMMAND_BUFFER_TEXT MATCHES "LazyClearSyncScope = \\[gl\\]")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-functions-by-ref.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_BYREF_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_BYREF_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_BYREF_PATCH_ERROR
        )
        if(NOT DAWN_GL_BYREF_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL functions-by-reference patch:\n"
                "${DAWN_GL_BYREF_PATCH_OUTPUT}${DAWN_GL_BYREF_PATCH_ERROR}")
        endif()
    endif()

    # On top of the statistics patch: binding a sampled texture sets only the glTexParameteri
    # values its GL texture object does not have yet. Mesa 20.1 treats every swizzle
    # glTexParameteri as a change (flush, all sampler views of the texture dropped and rebuilt
    # by the next draw), and Dawn set base/max level and the four swizzles on every bind.
    set(DAWN_OPENGL_TEXTURE_HEADER "${dawn_SOURCE_DIR}/src/dawn/native/opengl/TextureGL.h")
    file(READ "${DAWN_OPENGL_TEXTURE_HEADER}" DAWN_OPENGL_TEXTURE_HEADER_TEXT)
    if(NOT DAWN_OPENGL_TEXTURE_HEADER_TEXT MATCHES "AppliedSamplingParams")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-texture-params.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_TEXPARAM_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_TEXPARAM_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_TEXPARAM_PATCH_ERROR
        )
        if(NOT DAWN_GL_TEXPARAM_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL texture parameter patch:\n"
                "${DAWN_GL_TEXPARAM_PATCH_OUTPUT}${DAWN_GL_TEXPARAM_PATCH_ERROR}")
        endif()
    endif()

    # On top of those: where a render pass's replay time goes (pipeline applies, bind groups,
    # immediates, vertex/index state, the glDraw* calls, and the draws right after a pipeline
    # change or a texture bind timed apart) and how many UBO ranges, VAOs and index buffers it
    # binds, for the perf-switch lines.
    set(DAWN_OPENGL_STATS_HEADER "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchStatsGL.h")
    file(READ "${DAWN_OPENGL_STATS_HEADER}" DAWN_OPENGL_STATS_HEADER_TEXT)
    if(NOT DAWN_OPENGL_STATS_HEADER_TEXT MATCHES "kDrawCallTicks")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-replay-timers.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_TIMERS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_TIMERS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_TIMERS_PATCH_ERROR
        )
        if(NOT DAWN_GL_TIMERS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL replay timers patch:\n"
                "${DAWN_GL_TIMERS_PATCH_OUTPUT}${DAWN_GL_TIMERS_PATCH_ERROR}")
        endif()
    endif()

    # On top of the timers patch: pipelines without vertex attributes (all of Aurora's GX
    # pipelines) share one VAO, so a pipeline change no longer switches VAOs (on Mesa 20.1 the
    # next draw then revalidates the vertex arrays) nor rebinds the index buffer; the index
    # buffer is rebound only when it changes, primitive restart set only when it changes.
    set(DAWN_OPENGL_RENDER_PIPELINE_HEADER "${dawn_SOURCE_DIR}/src/dawn/native/opengl/RenderPipelineGL.h")
    file(READ "${DAWN_OPENGL_RENDER_PIPELINE_HEADER}" DAWN_OPENGL_RENDER_PIPELINE_HEADER_TEXT)
    if(NOT DAWN_OPENGL_RENDER_PIPELINE_HEADER_TEXT MATCHES "mSharesVertexArrayObject")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-shared-vao.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_SHARED_VAO_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_SHARED_VAO_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_SHARED_VAO_PATCH_ERROR
        )
        if(NOT DAWN_GL_SHARED_VAO_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL shared VAO patch:\n"
                "${DAWN_GL_SHARED_VAO_PATCH_OUTPUT}${DAWN_GL_SHARED_VAO_PATCH_ERROR}")
        endif()
    endif()

    # COS_SWITCH_GL_NO_ERROR=1 at run time: a KHR_no_error GL context (Mesa skips the error
    # checks of every GL call), an opt-in A/B option; the ordinary context if Mesa refuses it.
    set(DAWN_OPENGL_CONTEXT_EGL_SOURCE "${dawn_SOURCE_DIR}/src/dawn/native/opengl/ContextEGL.cpp")
    file(READ "${DAWN_OPENGL_CONTEXT_EGL_SOURCE}" DAWN_OPENGL_CONTEXT_EGL_TEXT)
    if(NOT DAWN_OPENGL_CONTEXT_EGL_TEXT MATCHES "COS_SWITCH_GL_NO_ERROR")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-no-error-context.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_NO_ERROR_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_NO_ERROR_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_NO_ERROR_PATCH_ERROR
        )
        if(NOT DAWN_GL_NO_ERROR_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL no-error context patch:\n"
                "${DAWN_GL_NO_ERROR_PATCH_OUTPUT}${DAWN_GL_NO_ERROR_PATCH_ERROR}")
        endif()
    endif()

    # Pipelines whose stages translate to the same GLSL share one linked GL program (Mesa 20.1 on
    # Horizon has no program binaries, so each program costs a full compile and link on the one GL
    # context). Its own patch file, independent of the command-stats and texture-parameter ones.
    set(DAWN_OPENGL_PIPELINE_SOURCE "${dawn_SOURCE_DIR}/src/dawn/native/opengl/PipelineGL.cpp")
    file(READ "${DAWN_OPENGL_PIPELINE_SOURCE}" DAWN_OPENGL_PIPELINE_TEXT)
    if(NOT DAWN_OPENGL_PIPELINE_TEXT MATCHES "SharedProgramCache")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-program-share.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_PROGRAM_SHARE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_PROGRAM_SHARE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_PROGRAM_SHARE_PATCH_ERROR
        )
        if(NOT DAWN_GL_PROGRAM_SHARE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL program sharing patch:\n"
                "${DAWN_GL_PROGRAM_SHARE_PATCH_OUTPUT}${DAWN_GL_PROGRAM_SHARE_PATCH_ERROR}")
        endif()
    endif()

    # On top of the program sharing patch: counts of program binary cache hits, misses, rejected
    # and stored binaries (dawn_switch_gl_program_binary_stats) for the native port's log; the
    # Switch build of Mesa (switch/mesa) offers program binaries when its shader cache is on.
    file(READ "${DAWN_OPENGL_PIPELINE_SOURCE}" DAWN_OPENGL_PIPELINE_TEXT)
    if(NOT DAWN_OPENGL_PIPELINE_TEXT MATCHES "ProgramBinaryStats")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-program-binary-stats.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_PROGRAM_BINARY_STATS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_PROGRAM_BINARY_STATS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_PROGRAM_BINARY_STATS_PATCH_ERROR
        )
        if(NOT DAWN_GL_PROGRAM_BINARY_STATS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL program binary statistics patch:\n"
                "${DAWN_GL_PROGRAM_BINARY_STATS_PATCH_OUTPUT}${DAWN_GL_PROGRAM_BINARY_STATS_PATCH_ERROR}")
        endif()
    endif()

    # On top of the replay timers, shared-VAO and functions-by-reference patches: ticks for the
    # parts of CommandBuffer::Execute the replay timers leave out (lazy clears, framebuffer set-up,
    # default state, LoadOp clears, pass end, viewport/scissor/blend commands, buffer and texture
    # copies), with the first render pass of each Execute reported apart (docs/SWITCH_PERF_STUDY.md,
    # section 3.4, timer 2).
    file(READ "${DAWN_OPENGL_STATS_HEADER}" DAWN_OPENGL_STATS_HEADER_TEXT)
    if(NOT DAWN_OPENGL_STATS_HEADER_TEXT MATCHES "kPassFramebufferTicks")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-pass-timers.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_PASS_TIMERS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_PASS_TIMERS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_PASS_TIMERS_PATCH_ERROR
        )
        if(NOT DAWN_GL_PASS_TIMERS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL pass timers patch:\n"
                "${DAWN_GL_PASS_TIMERS_PATCH_OUTPUT}${DAWN_GL_PASS_TIMERS_PATCH_ERROR}")
        endif()
    endif()

    # On top of the pass timers: GPU time per render pass and per run of copies with
    # GL_TIME_ELAPSED_EXT queries, read back without waiting a few frames later
    # (SwitchGpuTimerGL.h; docs/SWITCH_PERF_STUDY.md, section 3.4, timer 3).
    if(NOT EXISTS "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchGpuTimerGL.h")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-gpu-timer.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_GPU_TIMER_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_GPU_TIMER_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_GPU_TIMER_PATCH_ERROR
        )
        if(NOT DAWN_GL_GPU_TIMER_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL GPU timer patch:\n"
                "${DAWN_GL_GPU_TIMER_PATCH_OUTPUT}${DAWN_GL_GPU_TIMER_PATCH_ERROR}")
        endif()
    endif()

    # The GPU timer's pass classes from Aurora's real pass labels ("EFB N", "Offscreen N",
    # "Depth Snapshot Pass"): before this the GX passes were all filed under "other". A separate
    # patch so that Dawn trees patched with the first GPU timer get it too.
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchGpuTimerGL.h"
         DAWN_OPENGL_GPU_TIMER_HEADER_TEXT)
    if(NOT DAWN_OPENGL_GPU_TIMER_HEADER_TEXT MATCHES "Depth Snapshot")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-gpu-timer-labels.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_GPU_TIMER_LABELS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_GPU_TIMER_LABELS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_GPU_TIMER_LABELS_PATCH_ERROR
        )
        if(NOT DAWN_GL_GPU_TIMER_LABELS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL GPU timer labels patch:\n"
                "${DAWN_GL_GPU_TIMER_LABELS_PATCH_OUTPUT}${DAWN_GL_GPU_TIMER_LABELS_PATCH_ERROR}")
        endif()
    endif()

    # On top of the pass and GPU timers: COS_SWITCH_GL_FBO_CACHE=1 keeps render pass framebuffers
    # keyed by their attachments instead of creating and deleting one per pass, and drops repeated
    # viewport/scissor/depth-range calls in a pass (SwitchFboCacheGL.h; docs/SWITCH_PERF_STUDY.md,
    # option c). Off by default: the pass set-up is then unchanged.
    if(NOT EXISTS "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchFboCacheGL.h")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-fbo-cache.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_FBO_CACHE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_FBO_CACHE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_FBO_CACHE_PATCH_ERROR
        )
        if(NOT DAWN_GL_FBO_CACHE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL framebuffer cache patch:\n"
                "${DAWN_GL_FBO_CACHE_PATCH_OUTPUT}${DAWN_GL_FBO_CACHE_PATCH_ERROR}")
        endif()
    endif()

    # On top of the framebuffer cache: GL texture and buffer names are deleted once the GPU has
    # finished the work submitted before their destruction (SwitchDeferredDeleteGL.h). libnx's
    # libdrm_nouveau waits for the GPU when Mesa frees a busy bo, and Dawn frees the swapchain
    # texture every frame: the next frame's first clear waited for the whole previous frame on the
    # GPU (docs/SWITCH_PERF_STUDY.md, section 6). COS_SWITCH_GL_DEFER_DELETE=0 deletes at once.
    if(NOT EXISTS "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchDeferredDeleteGL.h")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-deferred-delete.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_DEFERRED_DELETE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_DEFERRED_DELETE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_DEFERRED_DELETE_PATCH_ERROR
        )
        if(NOT DAWN_GL_DEFERRED_DELETE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL deferred delete patch:\n"
                "${DAWN_GL_DEFERRED_DELETE_PATCH_OUTPUT}${DAWN_GL_DEFERRED_DELETE_PATCH_ERROR}")
        endif()
    endif()

    # On top of the deferred deletes: a render pipeline's Tint translation (WGSL -> GLSL) runs
    # before Dawn takes the GL context, so the render worker, which needs the one context for
    # every frame, only waits for Mesa's compile and link; per-build tint/context times for the
    # "[cos] precompile" lines.
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/PipelineGL.h" DAWN_OPENGL_PIPELINE_HEADER_TEXT)
    if(NOT DAWN_OPENGL_PIPELINE_HEADER_TEXT MATCHES "PretranslateStages")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-pipeline-compile.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_PIPELINE_COMPILE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_PIPELINE_COMPILE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_PIPELINE_COMPILE_PATCH_ERROR
        )
        if(NOT DAWN_GL_PIPELINE_COMPILE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL pipeline compile patch:\n"
                "${DAWN_GL_PIPELINE_COMPILE_PATCH_OUTPUT}${DAWN_GL_PIPELINE_COMPILE_PATCH_ERROR}")
        endif()
    endif()

    # On top of all the GL patches above: Aurora's per-draw dynamic-offset uniform buffer is bound
    # once per 64 KiB window and the record's position in it rides with the immediates (Tint's
    # UniformWindowOptions), instead of a glBindBufferRange (and an nvc0 cache invalidation) per
    # draw. COS_SWITCH_GL_UBO_WINDOW=0 at run time for the old path (SwitchUniformWindowGL.h).
    if(NOT EXISTS "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchUniformWindowGL.h")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-ubo-window.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_UBO_WINDOW_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_UBO_WINDOW_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_UBO_WINDOW_PATCH_ERROR
        )
        if(NOT DAWN_GL_UBO_WINDOW_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL uniform window patch:\n"
                "${DAWN_GL_UBO_WINDOW_PATCH_OUTPUT}${DAWN_GL_UBO_WINDOW_PATCH_ERROR}")
        endif()
    endif()

    # On top of the uniform window: a render pipeline change issues only the fixed-function GL
    # state calls whose value differs from what the pass's pipelines last set
    # (COS_SWITCH_GL_STATE_CACHE=0 at run time for the old path).
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/PersistentPipelineStateGL.h"
         DAWN_OPENGL_PERSISTENT_STATE_TEXT)
    if(NOT DAWN_OPENGL_PERSISTENT_STATE_TEXT MATCHES "StateCacheEnabled")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-state-cache.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_STATE_CACHE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_STATE_CACHE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_STATE_CACHE_PATCH_ERROR
        )
        if(NOT DAWN_GL_STATE_CACHE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL state cache patch:\n"
                "${DAWN_GL_STATE_CACHE_PATCH_OUTPUT}${DAWN_GL_STATE_CACHE_PATCH_ERROR}")
        endif()
    endif()

    # On top of the GPU timer: GPU time per group of draws, a group being the frame's last GX debug
    # marker (the native port's COS_GPU_GROUPS) or the render pass's label
    # (dawn_switch_gl_gpu_groups; docs/SWITCH_PERF_STUDY.md, section 8).
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchGpuTimerGL.h"
         DAWN_OPENGL_GPU_TIMER_TEXT)
    if(NOT DAWN_OPENGL_GPU_TIMER_TEXT MATCHES "GpuGroupTotal")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-gpu-groups.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_GPU_GROUPS_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_GPU_GROUPS_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_GPU_GROUPS_PATCH_ERROR
        )
        if(NOT DAWN_GL_GPU_GROUPS_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL GPU groups patch:\n"
                "${DAWN_GL_GPU_GROUPS_PATCH_OUTPUT}${DAWN_GL_GPU_GROUPS_PATCH_ERROR}")
        endif()
    endif()

    # The present split in two timers (blit to the window, which dequeues the NWindow buffer, and
    # eglSwapBuffers) and COS_SWITCH_SWAP_INTERVAL (docs/SWITCH_PERF_STUDY.md, section 8).
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchStatsGL.h" DAWN_OPENGL_SWITCH_STATS_TEXT)
    if(NOT DAWN_OPENGL_SWITCH_STATS_TEXT MATCHES "kPresentBlitNs")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-present-split.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_PRESENT_SPLIT_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_PRESENT_SPLIT_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_PRESENT_SPLIT_PATCH_ERROR
        )
        if(NOT DAWN_GL_PRESENT_SPLIT_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL present split patch:\n"
                "${DAWN_GL_PRESENT_SPLIT_PATCH_OUTPUT}${DAWN_GL_PRESENT_SPLIT_PATCH_ERROR}")
        endif()
    endif()

    # COS_SWITCH_GL_FLUSH_PASSES=1: a glFlush after every render pass (an A/B of CPU/GPU overlap
    # within a frame; docs/SWITCH_PERF_STUDY.md, section 8).
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/CommandBufferGL.cpp" DAWN_OPENGL_COMMAND_BUFFER_TEXT)
    if(NOT DAWN_OPENGL_COMMAND_BUFFER_TEXT MATCHES "COS_SWITCH_GL_FLUSH_PASSES")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-flush-passes.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_FLUSH_PASSES_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_FLUSH_PASSES_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_FLUSH_PASSES_PATCH_ERROR
        )
        if(NOT DAWN_GL_FLUSH_PASSES_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL flush-passes patch:\n"
                "${DAWN_GL_FLUSH_PASSES_PATCH_OUTPUT}${DAWN_GL_FLUSH_PASSES_PATCH_ERROR}")
        endif()
    endif()

    # On top of the GPU groups: the GPU timer's results scaled from the Tegra X1's PTIMER ticks to
    # real time (x31.25/19.2; COS_SWITCH_GPU_TIMER_SCALE overrides; docs/SWITCH_PERF_STUDY.md,
    # section 8).
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchGpuTimerGL.h" DAWN_OPENGL_GPU_TIMER_TEXT2)
    if(NOT DAWN_OPENGL_GPU_TIMER_TEXT2 MATCHES "mScale")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-gpu-timer-scale.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_GPU_TIMER_SCALE_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_GPU_TIMER_SCALE_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_GPU_TIMER_SCALE_PATCH_ERROR
        )
        if(NOT DAWN_GL_GPU_TIMER_SCALE_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL GPU timer scale patch:\n"
                "${DAWN_GL_GPU_TIMER_SCALE_PATCH_OUTPUT}${DAWN_GL_GPU_TIMER_SCALE_PATCH_ERROR}")
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
    # Detected by Surface.cpp's Switch case: later patches (present split, compressed upload) also
    # put DAWN_PLATFORM_IS(SWITCH) blocks in SwapChainEGL.cpp, so a tree patched in another order
    # looked done here and built a Dawn whose surface had no NWindow ("[Surface] is invalid").
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/Surface.cpp" DAWN_SURFACE_TEXT)
    if(NOT DAWN_SURFACE_TEXT MATCHES "DAWN_PLATFORM_IS\\(SWITCH\\)")
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

    # Bug B7: clip z to WebGPU's [0, w] with glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE) instead
    # of Tint's 2z - w remap, which rounded every vertex's depth to about 2^-24 of w and made
    # Outset's shore foam decals flicker (SwitchClipControlGL.h; COS_SWITCH_GL_CLIP_CONTROL=0 at
    # run time for the old remap).
    if(NOT EXISTS "${dawn_SOURCE_DIR}/src/dawn/native/opengl/SwitchClipControlGL.h")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-clip-control.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_CLIP_CONTROL_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_CLIP_CONTROL_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_CLIP_CONTROL_PATCH_ERROR
        )
        if(NOT DAWN_GL_CLIP_CONTROL_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL clip control patch:\n"
                "${DAWN_GL_CLIP_CONTROL_PATCH_OUTPUT}${DAWN_GL_CLIP_CONTROL_PATCH_ERROR}")
        endif()
    endif()

    # HD textures: GLES has no GL_UNPACK_COMPRESSED_BLOCK_*, so upstream uploads compressed
    # textures one row of blocks per glCompressedTexSubImage2D (257 calls for a 512x512 BC7 with
    # mips, each a Mesa staging bo and GPU copy): turning HD textures on blocked the render worker
    # for a second. One call per mip level instead (padded rows packed on the CPU), plus upload and
    # texture creation counters. COS_SWITCH_GL_LEVEL_UPLOAD=0 at run time for the upstream path.
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/CommandBufferGL.cpp" DAWN_OPENGL_COMMAND_BUFFER_TEXT)
    if(NOT DAWN_OPENGL_COMMAND_BUFFER_TEXT MATCHES "SwitchLevelUploads")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-compressed-upload.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_COMPRESSED_UPLOAD_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_COMPRESSED_UPLOAD_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_COMPRESSED_UPLOAD_PATCH_ERROR
        )
        if(NOT DAWN_GL_COMPRESSED_UPLOAD_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL compressed upload patch:\n"
                "${DAWN_GL_COMPRESSED_UPLOAD_PATCH_OUTPUT}${DAWN_GL_COMPRESSED_UPLOAD_PATCH_ERROR}")
        endif()
    endif()

    # On top of everything above: the calling thread's render pipeline build times (translation,
    # waiting for the GL context, holding it) for Aurora's warm-up, which must not take a build
    # that only waited for the render worker for a shader cache miss.
    file(READ "${dawn_SOURCE_DIR}/src/dawn/native/opengl/RenderPipelineGL.cpp" DAWN_OPENGL_RENDER_PIPELINE_TEXT)
    if(NOT DAWN_OPENGL_RENDER_PIPELINE_TEXT MATCHES "dawn_switch_gl_thread_pipeline_ns")
        execute_process(
            COMMAND "${PATCH_EXECUTABLE}" -p1 -i
                    "${CMAKE_CURRENT_LIST_DIR}/patches/dawn-switch-gl-pipeline-wait.patch"
            WORKING_DIRECTORY "${dawn_SOURCE_DIR}"
            RESULT_VARIABLE DAWN_GL_PIPELINE_WAIT_PATCH_RESULT
            OUTPUT_VARIABLE DAWN_GL_PIPELINE_WAIT_PATCH_OUTPUT
            ERROR_VARIABLE DAWN_GL_PIPELINE_WAIT_PATCH_ERROR
        )
        if(NOT DAWN_GL_PIPELINE_WAIT_PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR
                "Could not apply the Dawn Switch GL pipeline wait patch:\n"
                "${DAWN_GL_PIPELINE_WAIT_PATCH_OUTPUT}${DAWN_GL_PIPELINE_WAIT_PATCH_ERROR}")
        endif()
    endif()
endif()
