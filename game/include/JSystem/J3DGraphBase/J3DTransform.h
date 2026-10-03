#ifndef J3DTRANSFORM_H
#define J3DTRANSFORM_H

#include "dolphin/mtx/vec.h"
#include "dolphin/mtx/mtxvec.h"
#if TARGET_PC
#include <math.h>
#endif

struct J3DTextureSRTInfo;

struct J3DTransformInfo {
    /* 0x00 */ Vec mScale;
    /* 0x0C */ SVec mRotation;
    /* 0x14 */ Vec mTranslate;

    inline J3DTransformInfo& operator=(const J3DTransformInfo& b) {
        mScale = b.mScale;
        mRotation = b.mRotation;
        mTranslate = b.mTranslate;
        return *this;
    }
};  // Size: 0x20

extern J3DTransformInfo const j3dDefaultTransformInfo;
extern Vec const j3dDefaultScale;
extern Mtx const j3dDefaultMtx;
extern f32 PSMulUnit01[];

void J3DGQRSetup7(u32 param_0, u32 param_1, u32 param_2, u32 param_3);
f32 J3DCalcZValue(MtxP m, Vec v);
void J3DCalcBBoardMtx(Mtx);
void J3DCalcYBBoardMtx(Mtx);
void J3DPSCalcInverseTranspose(f32 (*param_0)[4], f32 (*param_1)[3]);
void J3DGetTranslateRotateMtx(J3DTransformInfo const&, Mtx);
void J3DGetTranslateRotateMtx(s16, s16, s16, f32, f32, f32, Mtx);
void J3DGetTextureMtx(const J3DTextureSRTInfo&, Vec, Mtx);
void J3DGetTextureMtxOld(const J3DTextureSRTInfo&, Vec, Mtx);
void J3DGetTextureMtxMaya(const J3DTextureSRTInfo&, Mtx);
void J3DGetTextureMtxMayaOld(const J3DTextureSRTInfo&, Mtx);
void J3DScaleNrmMtx(Mtx, const Vec&);
void J3DScaleNrmMtx33(Mtx33, const Vec&);
void J3DMtxProjConcat(Mtx, Mtx, Mtx);
void J3DPSMtx33Copy(Mtx3P src, Mtx3P dst);
void J3DPSMtx33CopyFrom34(MtxP src, Mtx3P dst);
void J3DPSMtxArrayConcat(Mtx, Mtx, Mtx, u32);

#if TARGET_PC
// The four J3DPSMulMtxVec overloads below are paired-single asm on the GameCube. On the host they
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
#else
inline void J3DPSMulMtxVec(register MtxP mtx, register Vec* vec, register Vec* dst) {
#ifdef __MWERKS__
    asm {
        psq_l f0, 0(vec), 0, 0
        psq_l f2, 0(mtx), 0, 0
        psq_l f1, 8(vec), 1, 0
        ps_mul f4, f2, f0
        psq_l f3, 8(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        psq_l f8, 16(mtx), 0, 0
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 24(mtx), 0, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 0
        ps_madd f11, f9, f1, f10
        psq_l f2, 32(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 40(mtx), 0, 0
        ps_mul f4, f2, f0
        psq_st f12, 4(dst), 1, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 8(dst), 1, 0
    }
#endif
}

inline void J3DPSMulMtxVec(register MtxP mtx, register S16Vec* vec, register S16Vec* dst) {
#ifdef __MWERKS__
    asm {
        psq_l f0, 0(vec), 0, 7
        psq_l f2, 0(mtx), 0, 0
        psq_l f1, 4(vec), 1, 7
        ps_mul f4, f2, f0
        psq_l f3, 8(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        psq_l f8, 16(mtx), 0, 0
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 24(mtx), 0, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 7
        ps_madd f11, f9, f1, f10
        psq_l f2, 32(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 40(mtx), 0, 0
        ps_mul f4, f2, f0
        psq_st f12, 2(dst), 1, 7
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 4(dst), 1, 7
    }
#endif
}

// regalloc issues
inline void J3DPSMulMtxVec(register Mtx3P mtx, register Vec* vec, register Vec* dst) {
#ifdef __MWERKS__
    asm {
        lis r6, PSMulUnit01@ha
        psq_l f0, 0(vec), 0, 0
        addi r6, r6, PSMulUnit01@l
        psq_l f2, 0(mtx), 0, 0
        psq_l f13, 0(r6), 0, 0
        psq_l f1, 8(vec), 1, 0
        ps_add f1, f13, f1
        psq_l f3, 8(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_l f8, 12(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 20(mtx), 1, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 0
        ps_madd f11, f9, f1, f10
        psq_l f2, 24(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 32(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_st f12, 4(dst), 1, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 8(dst), 1, 0
    }
#endif
}

// regalloc issues
inline void J3DPSMulMtxVec(register Mtx3P mtx, register S16Vec* vec, register S16Vec* dst) {
#ifdef __MWERKS__
    asm {
        lis r6, PSMulUnit01@ha
        psq_l f0, 0(vec), 0, 7
        addi r6, r6, PSMulUnit01@l
        psq_l f2, 0(mtx), 0, 0
        psq_l f13, 0(r6), 0, 0
        psq_l f1, 4(vec), 1, 7
        ps_add f1, f13, f1
        psq_l f3, 8(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_l f8, 12(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 20(mtx), 1, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 7
        ps_madd f11, f9, f1, f10
        psq_l f2, 24(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 32(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_st f12, 2(dst), 1, 7
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 4(dst), 1, 7
    }
#endif
}
#endif /* TARGET_PC */

#endif /* J3DTRANSFORM_H */
