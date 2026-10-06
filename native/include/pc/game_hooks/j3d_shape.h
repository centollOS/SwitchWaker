#ifndef PC_GAME_HOOKS_J3D_SHAPE_H
#define PC_GAME_HOOKS_J3D_SHAPE_H

// Included by game/src/JSystem/J3DGraphBase/J3DShape.cpp under TARGET_PC in place of the GameCube's
// J3DLoadArrayBasePtr (step G3 of docs/GAME_CODE_ORGANIZATION.md moved this here). Called by
// J3DShape::loadVtxArray for every shape drawn: inline, as in that unit.

// Aurora has no CP_REG_ARRAYBASE (it logs "not supported" and ignores the write: a 32-bit
// physical address cannot hold a host pointer). Its replacement, GX_AURORA_LOAD_ARRAYBASE, takes
// the 64-bit pointer, the array's size in bytes and its byte order, the same command
// GDSetArraySized puts in the shape's VCD/VAT list (after Dusklight's J3DShape.cpp; the stride
// stays the one that list set, as on the GameCube).
static inline void J3DLoadArrayBasePtr(GXAttr attr, void* data, u32 size) {
    u32 idx = (attr == GX_VA_NBT) ? 1 : (attr - GX_VA_POS);
    const u64 addr = (u64)(uintptr_t)data;
    GXCmd1u8(GX_AURORA);
    GXCmd1u16(GX_AURORA_LOAD_ARRAYBASE + idx);
    GXCmd1u32((u32)(addr >> 32));
    GXCmd1u32((u32)addr);
    GXCmd1u32(size);
    // Every array a J3DVertexBuffer hands out is in the console's byte order (big-endian): the
    // model's VTX1 arrays as on the disc, and the CPU deformers' output arrays, which keep the
    // layout of the arrays they are computed from.
    GXCmd1u8(0);
}

// The size of the vertex array `data` points at: the model's own VTX1 array is sized as in the
// shape's VCD/VAT list (so Aurora sees the same array and keeps its cached upload); any other
// array (a deformer's output, allocated for `num` entries) is `num` entries of `stride` bytes.
static inline u32 J3DVtxArraySize(const J3DVertexData* vtxData, const void* data, const void* modelArray,
                           u32 num, u32 stride) {
    if (data == NULL)
        return 0;
    if (data == modelArray)
        return vtxData->getVtxArraySize(data);
    return num * stride;
}

#endif /* PC_GAME_HOOKS_J3D_SHAPE_H */
