// Scaffold check (C++20): the decomp's base headers compile under TARGET_PC with the host
// C++ library, and the PC config header's intrinsics behave like the PowerPC instructions.
#include "global.h"
#include "algorithm.h"
#include "new.h"

static_assert(TARGET_PC, "TARGET_PC must be defined");
static_assert(VERSION == VERSION_USA, "GZLE01 build");
static_assert(sizeof(s8) == 1 && sizeof(u8) == 1, "8-bit types");
static_assert(sizeof(s16) == 2 && sizeof(u16) == 2, "16-bit types");
static_assert(sizeof(s32) == 4 && sizeof(u32) == 4, "32-bit types");
static_assert(sizeof(s64) == 8 && sizeof(u64) == 8, "64-bit types");
static_assert(sizeof(f32) == 4 && sizeof(f64) == 8, "float types");
static_assert(static_cast<char>(0x80) < 0, "plain char is signed, as on the GameCube");

int cos_pc_check_cxx(u32 value) {
    u8 buffer[64];
    __dcbz(buffer + 32, 0);
    __sync();
    int bits = __cntlzw(value) + __rlwimi(0, (int)value, 8, 16, 23);
    f32 r = (f32)__fres(2.0f) + (f32)__frsqrte(4.0);
    return bits + (int)r + (int)ARRAY_SIZE(buffer);
}
