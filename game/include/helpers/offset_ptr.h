#ifndef HELPERS_OFFSET_PTR_H
#define HELPERS_OFFSET_PTR_H

/*
 * GameCube shim of native/include/helpers/offset_ptr.h (phase 4, step 4.0b of
 * docs/NATIVE_PORT_PHASE4_6.md). On the original target the game overwrites a file offset in place
 * with a 32-bit pointer, so OFFSET_PTR(T) is the decomp's T* and OFFSET_PTR_RAW its u32. Same
 * definitions as the #else branch of Dusklight's include/helpers/offset_ptr.h (CC0,
 * ref/dusklight at 40457c6).
 */
#if TARGET_PC
#error "game/include/helpers is the GameCube shim; TARGET_PC must use native/include/helpers"
#endif

// As the native header, this one brings BE(T) too.
#include "endian.h"

#define OFFSET_PTR(T) T*
#define OFFSET_PTR_RAW u32

#endif /* HELPERS_OFFSET_PTR_H */
