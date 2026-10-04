# cos_hd_pack: the offline converter of Dolphin-format HD texture packs into the compact pack the
# game streams (docs/HD_TEXTURES.md). A host tool, separate from the game build:
#
#   cmake -S native/tools/hd_pack -B build/hd-pack-tool -G Ninja -DCMAKE_BUILD_TYPE=Release
#   ninja -C build/hd-pack-tool
#
# scripts/hd/build_hd_pack.sh configures, builds and runs it. BC7/BC1/BC3 encoding and decoding
# come from bc7enc_rdo (Richard Geldreich, MIT or public domain), fetched at a pinned commit.
cmake_minimum_required(VERSION 3.24)
project(cos_hd_pack CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

include(FetchContent)
FetchContent_Declare(bc7enc_rdo
        GIT_REPOSITORY https://github.com/richgel999/bc7enc_rdo.git
        GIT_TAG b9438627eef73a1157e84201b6fa6eb2ffd6d9f0)
FetchContent_GetProperties(bc7enc_rdo)
if (NOT bc7enc_rdo_POPULATED)
    FetchContent_Populate(bc7enc_rdo)
endif ()

add_library(bc7codec STATIC
        ${bc7enc_rdo_SOURCE_DIR}/bc7enc.cpp
        ${bc7enc_rdo_SOURCE_DIR}/bc7decomp.cpp
        ${bc7enc_rdo_SOURCE_DIR}/rgbcx.cpp)
target_include_directories(bc7codec PUBLIC ${bc7enc_rdo_SOURCE_DIR})
target_compile_options(bc7codec PRIVATE -w)

find_package(Threads REQUIRED)
add_executable(cos_hd_pack cos_hd_pack.cpp)
target_link_libraries(cos_hd_pack PRIVATE bc7codec Threads::Threads)
