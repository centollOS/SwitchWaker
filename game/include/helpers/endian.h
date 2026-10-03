#ifndef HELPERS_ENDIAN_H
#define HELPERS_ENDIAN_H

/*
 * GameCube shim of native/include/helpers/endian.h (phase 4, step 4.0b of
 * docs/NATIVE_PORT_PHASE4_6.md). On the original target the CPU is big-endian like the disc, so a
 * BE(T) field is just a T and RES_* are no-ops: the struct definitions that use them compile to
 * exactly the decomp's code. The names and meanings follow the #else branch of Dusklight's
 * include/helpers/endian.h (CC0, ref/dusklight at 40457c6). The native build reaches
 * native/include/helpers instead (it comes first on the include path); check/check_sdk_shadow.sh
 * fails if anything there resolves here.
 */
#if TARGET_PC
#error "game/include/helpers is the GameCube shim; TARGET_PC must use native/include/helpers"
#endif

#define BE(T) T
#define LE(T) T
#define BE_HOST(T) (T)

#define RES_U16(x) (x)
#define RES_S16(x) (x)
#define RES_U32(x) (x)
#define RES_S32(x) (x)

#endif /* HELPERS_ENDIAN_H */
