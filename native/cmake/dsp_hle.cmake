# cos_dsp_hle: the DSP behind cos_sdk's DSP library (step 5.A of docs/NATIVE_PORT_PHASE4_6.md,
# decisions H6 and H10). Dolphin's high-level DSP emulation (DSPHLE with its JAudio ucode, GPLv2+;
# this repository is GPLv3), compiled from the RecompCore checkout (ref/recompcore) the way the
# iOS and Switch hosts of the upstream recompilation project compile it (apple/ios/CMakeLists.txt,
# switch/host/CMakeLists.txt there), behind
# a small API of its own (native/dsp_hle/cos_dsp_hle.h) so cos_sdk never sees a Dolphin header.
#
# COS_RECOMPCORE_DIR is the RecompCore source checkout. By default it is ref/recompcore of the
# repository, or of the main checkout when this is a git worktree (build/lanes/<lane>).
# Only its sources are read; nothing is generated inside it.

if (NOT COS_WITH_AURORA)
    return() # only cos_sdk uses it, and fmt comes with Aurora
endif ()

set(COS_DSP_HLE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../dsp_hle")
cmake_path(NORMAL_PATH COS_DSP_HLE_ROOT)

if (NOT COS_RECOMPCORE_DIR)
    set(_cos_recompcore "${COS_NATIVE_ROOT}/../ref/recompcore")
    if (NOT EXISTS "${_cos_recompcore}/Source/Core/Core/HW/DSPHLE/DSPHLE.cpp")
        # A git worktree has no ref/: use the main checkout's (git's common directory's parent).
        execute_process(COMMAND git -C "${COS_NATIVE_ROOT}" rev-parse --path-format=absolute --git-common-dir
                OUTPUT_VARIABLE _cos_git_common OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (_cos_git_common)
            set(_cos_recompcore "${_cos_git_common}/../ref/recompcore")
        endif ()
    endif ()
    cmake_path(NORMAL_PATH _cos_recompcore)
    set(COS_RECOMPCORE_DIR "${_cos_recompcore}" CACHE PATH
            "RecompCore source checkout (Dolphin's DSPHLE for the DSP, decision H6)")
endif ()
if (NOT EXISTS "${COS_RECOMPCORE_DIR}/Source/Core/Core/HW/DSPHLE/UCodes/UCodes.cpp")
    message(FATAL_ERROR "cos_native: COS_RECOMPCORE_DIR (${COS_RECOMPCORE_DIR}) is not a RecompCore "
            "checkout (needs Source/Core/Core/HW/DSPHLE)")
endif ()

set(_cos_dsphle "${COS_RECOMPCORE_DIR}/Source/Core/Core/HW/DSPHLE")
file(GLOB _cos_ucodes "${_cos_dsphle}/UCodes/*.cpp")
add_library(cos_dsp_hle STATIC
        # The ARAM accelerator the ucodes read samples through.
        "${COS_RECOMPCORE_DIR}/Source/Core/Core/DSP/DSPAccelerator.cpp"
        ${_cos_dsphle}/DSPHLE.cpp
        ${_cos_dsphle}/MailHandler.cpp
        ${_cos_ucodes}
        "${COS_DSP_HLE_ROOT}/dsp_hle_backend.cpp"
        "${COS_DSP_HLE_ROOT}/dsp_common_shim.cpp")
target_include_directories(cos_dsp_hle PUBLIC "${COS_DSP_HLE_ROOT}")
target_include_directories(cos_dsp_hle PRIVATE
        "${COS_RECOMPCORE_DIR}/Source/Core"
        "${COS_RECOMPCORE_DIR}/Source"
        "${COS_RECOMPCORE_DIR}/Externals"
        "${COS_DSP_HLE_ROOT}/generated")
target_compile_definitions(cos_dsp_hle PRIVATE
        _ARCH_64=1 _M_ARM_64=1 _DEFAULT_SOURCE __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS)
target_compile_features(cos_dsp_hle PRIVATE cxx_std_23)
# Dolphin's code, not ours: its warnings are not acted on here.
target_compile_options(cos_dsp_hle PRIVATE -fno-strict-aliasing -w)
# Aurora's fmt (one fmt per binary) and the host zlib (Common::HashAdler32).
target_link_libraries(cos_dsp_hle PRIVATE fmt::fmt z)
