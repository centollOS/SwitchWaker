#ifndef TCOLOR_H
#define TCOLOR_H

#include "dolphin/gx/GXStruct.h"
#if TARGET_PC
#include "helpers/endian.h"
#endif

namespace JUtility {
struct TColor : public GXColor {
    TColor(u8 r, u8 g, u8 b, u8 a) { set(r, g, b, a); }
    TColor() { set(0xffffffff); }
    TColor(u32 u32Color) { set(u32Color); }
    TColor(GXColor color) { set(color); }
#if TARGET_PC
    // The u32 form is 0xRRGGBBAA, as GX and the disc data (BLO colours) have it: it is the
    // big-endian view of r, g, b, a, so the host reads and writes it through BE(u32) (step 4.5).
    // From Dusklight 40457c6 (CC0), libs/JSystem/include/JSystem/JUtility/TColor.h.
    TColor(BE(u32) u32Color) { set(u32Color); }
#endif

    // TColor(const TColor& other) { set(other.toUInt32()); }
    TColor& operator=(const TColor& other) {
        GXColor::operator=(other);
        return *this;
    }

    operator u32() const { return toUInt32(); }
#if TARGET_PC
    u32 toUInt32() const { return *(BE(u32)*)&r; }
#else
    u32 toUInt32() const { return *(u32*)&r; }
#endif

    void set(u8 cR, u8 cG, u8 cB, u8 cA) {
        r = cR;
        g = cG;
        b = cB;
        a = cA;
    }

#if TARGET_PC
    void set(u32 u32Color) { *(BE(u32)*)&r = u32Color; }
#else
    void set(u32 u32Color) { *(u32*)&r = u32Color; }
#endif
    void set(GXColor gxColor) {
        GXColor* temp = this;
        *temp = gxColor;
    }
};
}  // namespace JUtility

#endif
