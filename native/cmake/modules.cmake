# Game modules: one OBJECT library per module, built and fixed one at a time in phase 1.
#
# Each module is behind the option COS_MODULE_<name> (with '-' as '_'), OFF until its sources
# compile (modules in COS_MODULES_READY default to ON); turn one on with
# -DCOS_MODULE_<name>=ON or everything with -DCOS_ALL_MODULES=ON.
# Sources are taken from fixed directories of game/src (sorted, so the actor split is
# stable), minus the units listed in deferred.cmake.
include_guard(GLOBAL)

option(COS_ALL_MODULES "Enable every game module regardless of its own option" OFF)

set(COS_MODULES
        SSystem
        JSystem-core
        JSystem-J3D
        JSystem-2D-particle
        JSystem-studio
        framework
        m_Do
        d-core
        actors-1 actors-2 actors-3 actors-4 actors-5 actors-6)

# Modules whose every unit compiles (or is deferred); their options default to ON.
set(COS_MODULES_READY
        SSystem
        JSystem-core
        JSystem-J3D
        JSystem-2D-particle
        JSystem-studio
        framework
        m_Do
        d-core
        actors-1
        actors-2)

set(COS_ACTOR_CHUNKS 6)

# Collect <dir>/*.cpp|*.c (non-recursive unless RECURSE) relative to game/src.
function(_cos_glob out)
    cmake_parse_arguments(G "" "" "DIRS;RECURSE;FILES" ${ARGN})
    set(_all)
    foreach (_d IN LISTS G_DIRS)
        file(GLOB _f CONFIGURE_DEPENDS "${COS_ROOT}/src/${_d}/*.cpp" "${COS_ROOT}/src/${_d}/*.c")
        list(APPEND _all ${_f})
    endforeach ()
    foreach (_d IN LISTS G_RECURSE)
        file(GLOB_RECURSE _f CONFIGURE_DEPENDS "${COS_ROOT}/src/${_d}/*.cpp" "${COS_ROOT}/src/${_d}/*.c")
        list(APPEND _all ${_f})
    endforeach ()
    foreach (_f IN LISTS G_FILES)
        list(APPEND _all "${COS_ROOT}/src/${_f}")
    endforeach ()
    list(SORT _all)
    set(${out} ${_all} PARENT_SCOPE)
endfunction()

_cos_glob(COS_SRC_SSystem RECURSE SSystem)
_cos_glob(COS_SRC_JSystem-core
        DIRS JSystem/JKernel JSystem/JSupport JSystem/JUtility JSystem/JMath JSystem/JGadget
             JSystem/JFramework JSystem/JRenderer)
_cos_glob(COS_SRC_JSystem-J3D
        DIRS JSystem/J3DGraphBase JSystem/J3DGraphAnimator JSystem/J3DGraphLoader JSystem/J3DU)
_cos_glob(COS_SRC_JSystem-2D-particle DIRS JSystem/J2DGraph JSystem/JParticle)
_cos_glob(COS_SRC_JSystem-studio DIRS JSystem/JStage JSystem/JMessage RECURSE JSystem/JStudio)
_cos_glob(COS_SRC_framework DIRS f_pc f_op f_ap c FILES DynamicLink.cpp)
_cos_glob(COS_SRC_m_Do DIRS m_Do)
_cos_glob(COS_SRC_d-core DIRS d)

# Actors (src/d/actor, ~440 units) split into COS_ACTOR_CHUNKS sorted chunks of equal size.
_cos_glob(_actors DIRS d/actor)
list(LENGTH _actors _n_actors)
math(EXPR _chunk "(${_n_actors} + ${COS_ACTOR_CHUNKS} - 1) / ${COS_ACTOR_CHUNKS}")
foreach (_i RANGE 1 ${COS_ACTOR_CHUNKS})
    math(EXPR _begin "(${_i} - 1) * ${_chunk}")
    set(COS_SRC_actors-${_i})
    if (_begin LESS _n_actors)
        list(SUBLIST _actors ${_begin} ${_chunk} COS_SRC_actors-${_i})
    endif ()
endforeach ()

get_property(_deferred GLOBAL PROPERTY COS_DEFERRED_UNITS)

set(_enabled)
foreach (_m IN LISTS COS_MODULES)
    string(REPLACE "-" "_" _opt "COS_MODULE_${_m}")
    if (_m IN_LIST COS_MODULES_READY)
        option(${_opt} "Build the ${_m} game module" ON)
    else ()
        option(${_opt} "Build the ${_m} game module" OFF)
    endif ()

    set(_srcs ${COS_SRC_${_m}})
    list(LENGTH _srcs _total)
    if (_deferred)
        list(REMOVE_ITEM _srcs ${_deferred})
    endif ()
    list(LENGTH _srcs _kept)
    math(EXPR _dropped "${_total} - ${_kept}")
    set_property(GLOBAL PROPERTY "COS_MODULE_SOURCES:${_m}" "${_srcs}")

    if (${_opt} OR COS_ALL_MODULES)
        add_library(${_m} OBJECT ${_srcs})
        target_link_libraries(${_m} PRIVATE cos_game_headers)
        set_target_properties(${_m} PROPERTIES FOLDER "game")
        list(APPEND _enabled ${_m})
        message(STATUS "cos_native: module ${_m}: ${_kept} units (${_dropped} deferred)")
    else ()
        message(STATUS "cos_native: module ${_m}: disabled (${_total} units, ${_dropped} deferred; -D${_opt}=ON)")
    endif ()
endforeach ()

# Aggregate target for every enabled module.
add_custom_target(cos_modules)
if (_enabled)
    add_dependencies(cos_modules ${_enabled})
endif ()

# `ninja cos_deferred` prints the deferred list with reasons.
set(_lines)
foreach (_u IN LISTS _deferred)
    file(RELATIVE_PATH _rel "${COS_ROOT}" "${_u}")
    get_property(_why GLOBAL PROPERTY "COS_DEFER_REASON:${_rel}")
    list(APPEND _lines "${_rel}: ${_why}")
endforeach ()
list(LENGTH _deferred _n_deferred)
file(WRITE "${CMAKE_BINARY_DIR}/cos_deferred.txt" "")
foreach (_l IN LISTS _lines)
    file(APPEND "${CMAKE_BINARY_DIR}/cos_deferred.txt" "${_l}\n")
endforeach ()
add_custom_target(cos_deferred
        COMMAND ${CMAKE_COMMAND} -E echo "${_n_deferred} deferred units:"
        COMMAND ${CMAKE_COMMAND} -E cat "${CMAKE_BINARY_DIR}/cos_deferred.txt"
        VERBATIM)
