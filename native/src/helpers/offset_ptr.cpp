// OffsetPtr (native/include/helpers/offset_ptr.h).
//
// Provenance: copied from Dusklight src/helpers/offset_ptr.cpp (CC0, ref/dusklight at 40457c6),
// phase 4 step 4.0b of docs/NATIVE_PORT_PHASE4_6.md. Two changes:
//  - The null-offset check panics through OSPanic directly instead of JUT_ASSERT: the game's
//    JUT_ASSERT also calls JUTAssertion::showAssert, which would tie this helper (and
//    cos_pc_tests) to JSystem. The outcome, a panic naming this file and line, is the same.
//  - The range check: bit 31 is the "relocated" flag and operator T*() reads bit 30 as the sign,
//    so a positive offset must stay below 0x4000'0000. Dusklight accepted up to 0x7FFF'FFFF, and
//    an offset in [0x4000'0000, 0x7FFF'FFFF] would have decoded as negative.
// Addition (step 4.9a): setBaseAllowZero(), the same relocation for an offset where 0 is valid
// (a path's first point is at offset 0 of its PPNT/RPPN entries: 225 paths on the disc).
#include "helpers/offset_ptr.h"

#include <dolphin/os.h>

bool OffsetPtr::isRelocated() {
    return value & 0x8000'0000;
}

bool OffsetPtr::setBase(void* base) {
    if (value == 0) {
        OSPanic(__FILE__, __LINE__, "OffsetPtr::setBase: null offset");
    }
    return setBaseAllowZero(base);
}

bool OffsetPtr::setBaseAllowZero(void* base) {
    if (isRelocated()) {
        // Already relocated, don't touch it again!
        return false;
    }

    ptrdiff_t diff = (u8*)this - (u8*)base;
    ptrdiff_t newDiff = value - diff;
    // Check that it's in range given that we use the 31st bit as a flag (and the 30th as the sign).
    if (newDiff < -0x4000'0000 || newDiff > 0x3FFF'FFFF) {
        OSPanic(__FILE__, __LINE__, "Not enough space in StageOffsetPtr!");
    }

    value = newDiff | 0x8000'0000;
    return true;
}
