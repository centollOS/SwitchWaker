/*
 * cos_pc_config.h - force-included first in every unit of the native (TARGET_PC) build.
 *
 * It stands in for what Metrowerks CodeWarrior and its MSL library gave the decompiled game
 * implicitly, so that the game sources need as few TARGET_PC edits as possible:
 *   - the PowerPC intrinsics MWCC provides as built-ins (__cntlzw, __rlwimi, __dcbz, __sync,
 *     __fres, __frsqrte), as portable C/C++ with the same results.
 * MSL-only header names (algorithm.h, new.h, ...) are shims in native/include/pc/msl.
 *
 * Keep it small and C11/C++20 clean: it is compiled into every unit, C and C++.
 */
#ifndef COS_PC_CONFIG_H
#define COS_PC_CONFIG_H

#if !defined(TARGET_PC) || !TARGET_PC
#error "cos_pc_config.h belongs to the native TARGET_PC build only"
#endif
#ifdef __MWERKS__
#error "cos_pc_config.h must not be seen by Metrowerks"
#endif

#include <stdint.h>

#define COS_PC_INTRINSIC static inline

/* ---- Metrowerks PowerPC intrinsics ------------------------------------------------------- */
/* global.h declares these only for the original target (#if !TARGET_PC). */

/* cntlzw: count leading zeros of a 32-bit word; 32 for zero. */
COS_PC_INTRINSIC int __cntlzw(unsigned int value) {
    return value == 0 ? 32 : __builtin_clz(value);
}

/*
 * rlwimi rA,rS,SH,MB,ME: rotate rS left by SH, insert it into rA under the mask MB..ME
 * (big-endian bit numbering, bit 0 is the MSB; MB > ME wraps around). MWCC's form is
 * __rlwimi(rA, rS, SH, MB, ME) and returns the new rA.
 */
COS_PC_INTRINSIC int __rlwimi(int ra, int rs, int sh, int mb, int me) {
    uint32_t s = (uint32_t)rs;
    uint32_t n = (uint32_t)sh & 31u;
    uint32_t rot = n == 0 ? s : ((s << n) | (s >> (32u - n)));
    uint32_t from_mb = 0xFFFFFFFFu >> ((uint32_t)mb & 31u);
    uint32_t to_me = 0xFFFFFFFFu << (31u - ((uint32_t)me & 31u));
    uint32_t mask = ((uint32_t)mb & 31u) <= ((uint32_t)me & 31u) ? (from_mb & to_me)
                                                                 : (from_mb | to_me);
    return (int)((rot & mask) | ((uint32_t)ra & ~mask));
}

/* dcbz: zero the 32-byte cache block that holds addr + offset. */
COS_PC_INTRINSIC void __dcbz(void* addr, int offset) {
    uintptr_t block = ((uintptr_t)addr + (intptr_t)offset) & ~(uintptr_t)31;
    __builtin_memset((void*)block, 0, 32);
}

/* sync: full memory barrier. */
COS_PC_INTRINSIC void __sync(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

/*
 * fres / frsqrte: the hardware gives estimates (fres about 1/4096 relative error, frsqrte
 * about 1/32); the host computes them exactly, as Dusklight does for its TARGET_PC math.
 */
COS_PC_INTRINSIC float __fres(float value) {
    return 1.0f / value;
}

COS_PC_INTRINSIC double __frsqrte(double value) {
    return 1.0 / __builtin_sqrt(value);
}

#undef COS_PC_INTRINSIC

/* ---- MSL's <math.h> extensions ------------------------------------------------------------ */
/*
 * MSL's math.h defines DEG_TO_RAD / RAD_TO_DEG (JStudio, d_kankyo_rain, actors). Same text as
 * MSL_Common/Include/math.h, including the unparenthesized argument, but with MSL's float M_PI
 * spelled out: the host's M_PI is a double and would change the arithmetic.
 */
#define COS_MSL_M_PI 3.14159265358979323846f
#ifndef DEG_TO_RAD
#define DEG_TO_RAD(degrees) (degrees * (COS_MSL_M_PI / 180.0f))
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG(radians) (radians * (180.0f / COS_MSL_M_PI + 0.000005f))
#endif

/* ---- MSL's <cmath> -------------------------------------------------------------------------- */
#ifdef __cplusplus
/*
 * MSL declares the C99 float math functions in namespace std (the game calls std::sqrtf,
 * std::fabsf, std::cosf, ...). libc++ only has the std::sqrt(float) overloads, so bring the
 * C library's float functions into std, which is what MSL's <cmath> did.
 */
#include <cmath>
#include <math.h>
namespace std {
using ::acosf;
using ::asinf;
using ::atan2f;
using ::atanf;
using ::ceilf;
using ::cosf;
using ::expf;
using ::fabsf;
using ::floorf;
using ::fmodf;
using ::logf;
using ::powf;
using ::sinf;
using ::sqrtf;
using ::tanf;
} // namespace std

/* ---- MSL's C library in namespace std ------------------------------------------------------ */
/*
 * MSL's C headers (string.h, ctype.h, stdlib.h, stdio.h) declare their functions in std and
 * re-export them globally, so the game calls both strrchr and std::strrchr, tolower and
 * std::tolower. The host <string.h> etc. do not declare the std:: names; the <c...> headers do.
 */
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ---- MSL's <stdarg.h> ----------------------------------------------------------------------- */
/*
 * MSL wraps va_list in std::__tag_va_List (same definition as MSL_Common/Include/stdarg.h);
 * JSystem passes it by pointer (JUTDirectPrint::printSub, JUTConsole::print_f_va) and reads
 * ->list. Same layout here over the host's va_list.
 */
#include <stdarg.h>
namespace std {
struct __tag_va_List {
    va_list list;
};
} // namespace std
#endif

#endif /* COS_PC_CONFIG_H */
