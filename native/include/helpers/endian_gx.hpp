// BE<T> for the GX enums and vertex-format structs found in J3D model data.
//
// Provenance: copied unchanged from Dusklight include/helpers/endian_gx.hpp (CC0, ref/dusklight at
// 40457c6), phase 4 step 4.0b. GameCube shim: game/include/helpers/endian_gx.hpp.
#pragma once

#include "dolphin/gx/GXStruct.h"
#include "endian.h"


#define IMPL_ENUM(type) \
template <> \
constexpr type BE<type>::swap(type val) noexcept { \
    return static_cast<type>(be32(val)); \
}

IMPL_ENUM(GXCullMode);
IMPL_ENUM(GXAttr);
IMPL_ENUM(GXAttrType);
IMPL_ENUM(GXCompType);
IMPL_ENUM(GXCompCnt);

#undef IMPL_ENUM


template <>
struct BE<GXVtxDescList> {
    BE<GXAttr> attr;
    BE<GXAttrType> type;

    constexpr static GXVtxDescList swap[[nodiscard]](GXVtxDescList val) noexcept;
};

template <>
struct BE<GXVtxAttrFmtList> {
    BE<GXAttr> attr;
    BE<GXCompCnt> cnt;
    BE<GXCompType> type;
    u8 frac;

    constexpr static GXVtxAttrFmtList swap[[nodiscard]](GXVtxAttrFmtList val) noexcept;
};

template <>
constexpr GXColorS10 BE<GXColorS10>::swap(GXColorS10 val) noexcept {
    return {
        be16s(val.r),
        be16s(val.g),
        be16s(val.b),
        be16s(val.a),
    };
}

constexpr GXVtxDescList BE<GXVtxDescList>::swap(GXVtxDescList val) noexcept {
    return {
        BE<GXAttr>::swap(val.attr),
        BE<GXAttrType>::swap(val.type),
    };
}

constexpr GXVtxAttrFmtList BE<GXVtxAttrFmtList>::swap(GXVtxAttrFmtList val) noexcept {
    return {
        BE<GXAttr>::swap(val.attr),
        BE<GXCompCnt>::swap(val.cnt),
        BE<GXCompType>::swap(val.type),
        val.frac
    };
}
