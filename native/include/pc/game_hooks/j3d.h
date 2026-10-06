#ifndef PC_GAME_HOOKS_J3D_H
#define PC_GAME_HOOKS_J3D_H

// Host helpers of the J3D code (step G3 of docs/GAME_CODE_ORGANIZATION.md moved them out of game/),
// included under TARGET_PC by J3DModelLoader.cpp, J3DShapeFactory.cpp and J3DCluster.cpp.
#include "dolphin/gx/GXAttr.h"
#include "dolphin/mtx/mtx.h"
#include "helpers/endian.h"
#include "helpers/endian_gx.hpp"

// ---- J3DModelLoader.cpp, J3DShapeFactory.cpp: the loaders' host-order copies of big-endian file
// data (native/src/pc/game_hooks/pc_j3d_hooks.cpp). Each copy is allocated on the current heap like
// the rest of the model data; the file is left as it is, so a resource can be loaded again.

struct J3DEnvelopBlock;
struct J3DShapeInitData;

// VTX1's attribute format list, up to and including its GX_VA_NULL entry.
GXVtxAttrFmtList* J3DPcCopyVtxAttrFmtList(const BE(GXVtxAttrFmtList)* src);
// num values of a big-endian array; NULL when src is NULL.
u16* J3DPcCopyU16Array(const BE(u16)* src, u32 num);
f32* J3DPcCopyF32Array(const BE(f32)* src, u32 num);
// EVP1's inverse joint matrices; NULL when the block has none.
Mtx* J3DPcLoadInvJointMtx(const J3DEnvelopBlock* block);
// SHP1's vertex descriptor table, up to the end of the last list a shape uses.
GXVtxDescList* J3DPcCopyVtxDescList(const BE(GXVtxDescList)* src, const J3DShapeInitData* initData,
                                    const BE(u16)* indexTable, u16 shapeNum);

// ---- J3DModelLoader.cpp: material IDs ----------------------------------------------------------

// The material IDs (mDiffFlag) are built from addresses: (u32)ptr, or (u32)ptr >> 4. On the
// GameCube every heap address is in MEM1 (0x80000000-0x817FFFFF), so (u32)ptr has bit 31 set and
// bit 30 clear, and ptr >> 4 has both clear; J3DMatPacket::isSame and isChanged read bit 31 as
// "changed". Every JKR heap lives in Aurora's MEM1 block (256 MiB, decision H5), so the low 30 bits
// of a host address are unique among materials: this gives them the GameCube's upper two bits.
inline u32 J3DGCAddressBits(const void* p) {
    return 0x80000000 | ((u32)(uintptr_t)p & 0x3FFFFFFF);
}

// ---- J3DCluster.cpp: the CPU skinning's vertex reads and writes (per vertex: inline) ----------

// The model's vertex arrays (VTX1 positions and normals) stay big-endian on the host, as Aurora
// reads them, and so do the CPU skinning's transformed arrays: J3DShape::loadVtxArray declares
// every array it binds big-endian (GX_AURORA_LOAD_ARRAYBASE). The deformers below therefore read
// each source vector big-endian, transform it in host order and store the result big-endian
// (J3DCluster.cpp's deformers).
static inline void J3DSkinLoadBE(const void* src, Vec* v) {
    const BE(f32)* p = (const BE(f32)*)src;
    v->x = p[0];
    v->y = p[1];
    v->z = p[2];
}

static inline void J3DSkinStoreBE(void* dst, const Vec* v) {
    BE(f32)* p = (BE(f32)*)dst;
    p[0] = v->x;
    p[1] = v->y;
    p[2] = v->z;
}

static inline void J3DSkinLoadBE(const void* src, S16Vec* v) {
    const BE(s16)* p = (const BE(s16)*)src;
    v->x = p[0];
    v->y = p[1];
    v->z = p[2];
}

static inline void J3DSkinStoreBE(void* dst, const S16Vec* v) {
    BE(s16)* p = (BE(s16)*)dst;
    p[0] = v->x;
    p[1] = v->y;
    p[2] = v->z;
}

#endif /* PC_GAME_HOOKS_J3D_H */
