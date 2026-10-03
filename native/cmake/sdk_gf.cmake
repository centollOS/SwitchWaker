# cos_sdk_gf: the game's own GF sources (game/src/dolphin/gf) compiled into cos_sdk
# (docs/NATIVE_PORT_PHASE2_3.md, step 2.6d).
#
# GF is the SDK's "fast" GX layer: each function writes raw BP/XF/CP register commands through
# GXCmd1u8/u16/u32. Aurora ships the GF headers but no GF bodies, so the bodies come from the decomp,
# as Dusklight compiles Dusklight's libs/dolphin/src/gf/GF*.cpp (files.cmake:1406). Aurora's GXCmd1u*
# write into the same FIFO as every other GX call (lib/gx/fifo.hpp), outside a display list as well
# as inside one, and its command processor parses LOAD_BP/CP/XF_REG in both cases.
#
# - Compiled against Aurora's SDK headers plus the forwarders in native/include/sdk (the game-only
#   names dolphin/gf/GF.h, dolphin/gf/GFTransform.h, dolphin/os/OS.h), with the game-only GF
#   declarations from native/include/sdk/cos_gf_extras.h. The same include order as the game's
#   (GameConfig.cmake), except that game/include is never on the path, and neither are the
#   game's flags (cos_game_headers).
# - An OBJECT library of its own (this file, not sdk.cmake, so 2.6d does not touch the glob), whose
#   objects go into cos_sdk.
# - One TARGET_PC edit (GFGeometry.cpp): Aurora ignores CP_REG_ARRAYBASE, so GFSetArraySized writes
#   Aurora's 64-bit array-base command and GFSetArray, which has no size, stops with OSPanic.
#   The other four units are unchanged decomp code.
#
# Needs Aurora, so it is only built with COS_WITH_AURORA=ON (after sdk.cmake has made cos_sdk).

if (NOT COS_WITH_AURORA OR NOT TARGET cos_sdk)
    return()
endif ()

set(COS_SDK_GF_SOURCES
        "${COS_ROOT}/src/dolphin/gf/GFGeometry.cpp"
        "${COS_ROOT}/src/dolphin/gf/GFLight.cpp"
        "${COS_ROOT}/src/dolphin/gf/GFPixel.cpp"
        "${COS_ROOT}/src/dolphin/gf/GFTev.cpp"
        "${COS_ROOT}/src/dolphin/gf/GFTransform.cpp")

add_library(cos_sdk_gf OBJECT ${COS_SDK_GF_SOURCES})
# Forwarders first, then Aurora's include (from the aurora::* targets below), as in GameConfig. Only GF, GD, GX, MTX and OS are needed; the full list keeps the defines identical
# to cos_sdk's.
target_include_directories(cos_sdk_gf PRIVATE "${COS_NATIVE_ROOT}/include/sdk")
target_compile_definitions(cos_sdk_gf PRIVATE MTX_USE_PS=1)
# The decomp's vertex-descriptor switches leave most enumerators to the hardware defaults on purpose.
target_compile_options(cos_sdk_gf PRIVATE -Wno-switch)
target_link_libraries(cos_sdk_gf PRIVATE ${COS_AURORA_LIBS})

target_sources(cos_sdk PRIVATE $<TARGET_OBJECTS:cos_sdk_gf>)

# The "gf" smoke test (native/sdk/tests/sdk_gf.cpp) includes dolphin/gf/GF.h through its forwarder,
# as the game will.
target_include_directories(cos_sdk_smoke PRIVATE "${COS_NATIVE_ROOT}/include/sdk")

# The ThreadSanitizer smoke program compiles cos_sdk's sources itself (sdk.cmake); it gets the GF
# objects uninstrumented, as it gets Aurora's libraries.
if (TARGET cos_sdk_smoke_tsan)
    target_sources(cos_sdk_smoke_tsan PRIVATE $<TARGET_OBJECTS:cos_sdk_gf>)
    target_include_directories(cos_sdk_smoke_tsan PRIVATE "${COS_NATIVE_ROOT}/include/sdk")
endif ()
