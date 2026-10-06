// The host bodies of J3DTransform.cpp's paired-single functions, moved out of game/ (step G3 of
// docs/GAME_CODE_ORGANIZATION.md); the GameCube's asm stays there. Out of line as on the GameCube
// (J3DTransform.cpp calls none of them itself). The inline J3DPSMulMtxVec forms are in
// native/include/pc/game_hooks/j3d_transform.h.
#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "dolphin/mtx/mtx.h"

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
// dst = the inverse transpose of src's 3x3 part (its cofactor matrix over the determinant); as
// with the asm, dst is left unchanged when the determinant is 0.
void J3DPSCalcInverseTranspose(Mtx src, Mtx33 dst) {
    f32 c00 = src[1][1] * src[2][2] - src[1][2] * src[2][1];
    f32 c01 = src[1][2] * src[2][0] - src[1][0] * src[2][2];
    f32 c02 = src[1][0] * src[2][1] - src[1][1] * src[2][0];
    f32 det = src[0][0] * c00 + src[0][1] * c01 + src[0][2] * c02;
    if (det == 0.0f)
        return;
    f32 inv = 1.0f / det;
    f32 c10 = src[0][2] * src[2][1] - src[0][1] * src[2][2];
    f32 c11 = src[0][0] * src[2][2] - src[0][2] * src[2][0];
    f32 c12 = src[0][1] * src[2][0] - src[0][0] * src[2][1];
    f32 c20 = src[0][1] * src[1][2] - src[0][2] * src[1][1];
    f32 c21 = src[0][2] * src[1][0] - src[0][0] * src[1][2];
    f32 c22 = src[0][0] * src[1][1] - src[0][1] * src[1][0];
    dst[0][0] = c00 * inv; dst[0][1] = c01 * inv; dst[0][2] = c02 * inv;
    dst[1][0] = c10 * inv; dst[1][1] = c11 * inv; dst[1][2] = c12 * inv;
    dst[2][0] = c20 * inv; dst[2][1] = c21 * inv; dst[2][2] = c22 * inv;
}

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
// Scales the columns of the 3x3 part by scl.
void J3DScaleNrmMtx(Mtx mtx, const Vec& scl) {
    for (int i = 0; i < 3; i++) {
        mtx[i][0] *= scl.x;
        mtx[i][1] *= scl.y;
        mtx[i][2] *= scl.z;
    }
}

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
void J3DScaleNrmMtx33(Mtx33 mtx, const Vec& scl) {
    for (int i = 0; i < 3; i++) {
        mtx[i][0] *= scl.x;
        mtx[i][1] *= scl.y;
        mtx[i][2] *= scl.z;
    }
}

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
// dst = a (3x4) times b read as a 4x4 matrix (16 floats); dst may alias a.
void J3DMtxProjConcat(Mtx a, Mtx b, Mtx dst) {
    const f32* m = &b[0][0];
    Mtx tmp;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = a[i][0] * m[0 * 4 + j] + a[i][1] * m[1 * 4 + j] + a[i][2] * m[2 * 4 + j] +
                        a[i][3] * m[3 * 4 + j];
        }
    }
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            dst[i][j] = tmp[i][j];
}

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
void J3DPSMtx33Copy(Mtx3P src, Mtx3P dst) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            dst[i][j] = src[i][j];
}

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
void J3DPSMtx33CopyFrom34(MtxP src, Mtx3P dst) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            dst[i][j] = src[i][j];
}

// Host body: the GameCube one is paired-single asm only (after Dusklight's J3DTransform.cpp,
// CC0, ref/dusklight at 40457c6).
// mAB[i] = mA * mB[i] for count matrices.
void J3DPSMtxArrayConcat(Mtx mA, Mtx mB, Mtx mAB, u32 count) {
    Mtx* src = (Mtx*)mB;
    Mtx* dst = (Mtx*)mAB;
    for (u32 i = 0; i < count; i++)
        MTXConcat(mA, src[i], dst[i]);
}
