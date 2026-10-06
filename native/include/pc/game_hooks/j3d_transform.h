#ifndef PC_GAME_HOOKS_J3D_TRANSFORM_H
#define PC_GAME_HOOKS_J3D_TRANSFORM_H

// Included by game/include/JSystem/J3DGraphBase/J3DTransform.h under TARGET_PC in place of the
// paired-single J3DPSMulMtxVec forms (step G3 of docs/GAME_CODE_ORGANIZATION.md moved them here).
// Per-vertex math: inline, as the GameCube forms are.
#include <math.h>

// The four J3DPSMulMtxVec overloads are paired-single asm on the GameCube. On the host they
// are plain C (after Dusklight's J3DTransform.h, CC0, ref/dusklight at 40457c6), with the
// quantization the asm gets from GQR7 kept: the S16Vec forms load each s16 scaled by
// 2^-LD_SCALE and store the result scaled by 2^ST_SCALE, truncated and clamped to s16 (Dolphin
// ScaleAndClamp), with the scales J3DGQRSetup7 last wrote (j3dHostGQR7, set by __MTGQR7). The
// vectors are host-order; callers holding console-order (big-endian) arrays convert around the call.
extern u32 j3dHostGQR7;

inline f32 J3DHostGQR7Scale(u32 scale6) {
    // GQR scale fields are 6-bit signed: 0..31 and -32..-1.
    s32 s = (s32)(scale6 & 0x3F);
    if (s >= 32)
        s -= 64;
    return ldexpf(1.0f, s);
}

inline f32 J3DHostGQR7Dequantize(s16 v) {
    return (f32)v / J3DHostGQR7Scale(j3dHostGQR7 >> 24);
}

inline s16 J3DHostGQR7Quantize(f32 v) {
    f32 q = v * J3DHostGQR7Scale(j3dHostGQR7 >> 8);
    if (!(q >= -32768.0f))
        q = -32768.0f;
    if (q > 32767.0f)
        q = 32767.0f;
    return (s16)q;
}

inline void J3DPSMulMtxVec(MtxP mtx, Vec* vec, Vec* dst) {
    f32 x = vec->x, y = vec->y, z = vec->z;
    dst->x = mtx[0][0] * x + mtx[0][1] * y + mtx[0][2] * z + mtx[0][3];
    dst->y = mtx[1][0] * x + mtx[1][1] * y + mtx[1][2] * z + mtx[1][3];
    dst->z = mtx[2][0] * x + mtx[2][1] * y + mtx[2][2] * z + mtx[2][3];
}

inline void J3DPSMulMtxVec(MtxP mtx, S16Vec* vec, S16Vec* dst) {
    // As psq_l with W=1: the translation column is multiplied by 1.0, not by the load scale.
    f32 x = J3DHostGQR7Dequantize(vec->x);
    f32 y = J3DHostGQR7Dequantize(vec->y);
    f32 z = J3DHostGQR7Dequantize(vec->z);
    s16 rx = J3DHostGQR7Quantize(mtx[0][0] * x + mtx[0][1] * y + mtx[0][2] * z + mtx[0][3]);
    s16 ry = J3DHostGQR7Quantize(mtx[1][0] * x + mtx[1][1] * y + mtx[1][2] * z + mtx[1][3]);
    s16 rz = J3DHostGQR7Quantize(mtx[2][0] * x + mtx[2][1] * y + mtx[2][2] * z + mtx[2][3]);
    dst->x = rx;
    dst->y = ry;
    dst->z = rz;
}

inline void J3DPSMulMtxVec(Mtx3P mtx, Vec* vec, Vec* dst) {
    f32 x = vec->x, y = vec->y, z = vec->z;
    dst->x = mtx[0][0] * x + mtx[0][1] * y + mtx[0][2] * z;
    dst->y = mtx[1][0] * x + mtx[1][1] * y + mtx[1][2] * z;
    dst->z = mtx[2][0] * x + mtx[2][1] * y + mtx[2][2] * z;
}

inline void J3DPSMulMtxVec(Mtx3P mtx, S16Vec* vec, S16Vec* dst) {
    f32 x = J3DHostGQR7Dequantize(vec->x);
    f32 y = J3DHostGQR7Dequantize(vec->y);
    f32 z = J3DHostGQR7Dequantize(vec->z);
    s16 rx = J3DHostGQR7Quantize(mtx[0][0] * x + mtx[0][1] * y + mtx[0][2] * z);
    s16 ry = J3DHostGQR7Quantize(mtx[1][0] * x + mtx[1][1] * y + mtx[1][2] * z);
    s16 rz = J3DHostGQR7Quantize(mtx[2][0] * x + mtx[2][1] * y + mtx[2][2] * z);
    dst->x = rx;
    dst->y = ry;
    dst->z = rz;
}

#endif /* PC_GAME_HOOKS_J3D_TRANSFORM_H */
