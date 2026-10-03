# Deferred units (phase 1, rule 5).
#
# A game unit is left out of the native build only when it cannot compile without the work of a
# later phase (2 SDK over Aurora, 3 static RELs, 4 64-bit/endianness, 5 JAudio). Each one is
# listed here with its path relative to game and the reason; modules.cmake removes it from
# its module and prints how many units each module defers. Nothing is dropped silently.
#
#   cos_defer(src/path/to/unit.cpp "phase N: why it cannot compile yet")
#
# Never deferred because never part of phase 1 (out of scope, not globbed by modules.cmake):
#   src/dolphin, src/PowerPC_EABI_Support, src/TRK_MINNOW_DOLPHIN, src/OdemuExi2,
#   src/odenotstub, src/amcstubs, src/REL. src/JSystem/JAudio and src/JAZelAudio joined the build in
#   phase 3 (step 3.7, module audio in modules.cmake).
include_guard(GLOBAL)

set_property(GLOBAL PROPERTY COS_DEFERRED_UNITS "")

function(cos_defer path reason)
    if (NOT reason)
        message(FATAL_ERROR "cos_defer(${path}): a reason is required")
    endif ()
    if (NOT EXISTS "${COS_ROOT}/${path}")
        message(FATAL_ERROR "cos_defer(${path}): no such unit under game")
    endif ()
    set_property(GLOBAL APPEND PROPERTY COS_DEFERRED_UNITS "${COS_ROOT}/${path}")
    set_property(GLOBAL PROPERTY "COS_DEFER_REASON:${path}" "${reason}")
endfunction()

# ---- Deferred units (none yet) -------------------------------------------------------------
