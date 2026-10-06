// The host side of the J3D loaders and textures, moved out of game/ (step G3 of
// docs/GAME_CODE_ORGANIZATION.md): the big-endian file data converted to host order
// (J3DModelLoader.cpp, J3DShapeFactory.cpp, J3DMaterialFactory.cpp, J3DClusterLoader.cpp) and the
// texture objects Aurora binds (J3DTevs.cpp: J3DTexture; J3DMatBlock.cpp: J3DTevBlock*::
// loadTexture). Loading code, run when a model is loaded; the per-frame paths stay in game/.
#include "JSystem/JSystem.h" // IWYU pragma: keep
#include "pc/game_hooks/j3d.h"
#include "JSystem/J3DGraphAnimator/J3DCluster.h"
#include "JSystem/J3DGraphAnimator/J3DSkinDeform.h"
#include "JSystem/J3DGraphBase/J3DMatBlock.h"
#include "JSystem/J3DGraphBase/J3DSys.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"
#include "JSystem/J3DGraphLoader/J3DClusterLoader.h"
#include "JSystem/J3DGraphLoader/J3DMaterialFactory.h"
#include "JSystem/J3DGraphLoader/J3DModelLoader.h"
#include "JSystem/J3DGraphLoader/J3DShapeFactory.h"
#include "JSystem/JSupport/JSupport.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include <string.h>

// ---- J3DModelLoader.cpp -----------------------------------------------------------------------

// The model's file data in host order (J3DModelLoader.cpp): copies on the current heap, the file
// left as it is so a resource can be loaded again (no swap in place).

// The file's attribute format list is big-endian ({GXAttr, GXCompCnt, GXCompType, frac}, ended
// by GX_VA_NULL); the model keeps a host-order copy for GX. The vertex arrays themselves stay
// big-endian.
GXVtxAttrFmtList* J3DPcCopyVtxAttrFmtList(const BE(GXVtxAttrFmtList)* src) {
    u32 count = 0;
    while (src[count].attr != GX_VA_NULL) {
        count++;
    }
    count++;
    GXVtxAttrFmtList* list = new GXVtxAttrFmtList[count];
    for (u32 i = 0; i < count; i++) {
        list[i].attr = src[i].attr;
        list[i].cnt = src[i].cnt;
        list[i].type = src[i].type;
        list[i].frac = src[i].frac;
    }
    return list;
}

u16* J3DPcCopyU16Array(const BE(u16)* src, u32 num) {
    if (src == NULL) {
        return NULL;
    }
    u16* copy = new u16[num];
    for (u32 i = 0; i < num; i++) {
        copy[i] = src[i];
    }
    return copy;
}

f32* J3DPcCopyF32Array(const BE(f32)* src, u32 num) {
    if (src == NULL) {
        return NULL;
    }
    f32* copy = new f32[num];
    for (u32 i = 0; i < num; i++) {
        copy[i] = src[i];
    }
    return copy;
}

// The end of the EVP1 table at `offset`: the next table of the block or the block's end (the
// tables are back to back; the block size is 32-byte aligned).
static u32 J3DTableEnd(const JUTDataBlockHeader* i_block, u32 offset, const BE(u32)* offsets,
                       int count) {
    u32 end = i_block->mSize;
    for (int i = 0; i < count; i++) {
        u32 other = offsets[i];
        if (other > offset && other < end) {
            end = other;
        }
    }
    return end;
}

// One inverse matrix per joint; the table ends at the next table or at the block's end.
Mtx* J3DPcLoadInvJointMtx(const J3DEnvelopBlock* i_block) {
    if (i_block->mpInvJointMtx == 0) {
        return NULL;
    }
    const BE(u32) offsets[] = {i_block->mpWEvlpMixMtxNum, i_block->mpWEvlpMixMtxIndex,
                               i_block->mpWEvlpMixWeight};
    u32 start = i_block->mpInvJointMtx;
    u32 num = (J3DTableEnd(i_block, start, offsets, 3) - start) / sizeof(Mtx);
    const BE(f32)* src = JSUConvertOffsetToPtr<BE(f32)>(i_block, i_block->mpInvJointMtx);
    Mtx* mtx = new Mtx[num];
    for (u32 i = 0; i < num; i++) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                mtx[i][r][c] = src[i * 12 + r * 4 + c];
            }
        }
    }
    return mtx;
}

// ---- J3DShapeFactory.cpp ----------------------------------------------------------------------

// The SHP1 vertex descriptor lists are big-endian {GXAttr, GXAttrType} pairs, each list ended by
// GX_VA_NULL; shapes address them by byte offset (mVtxDescListIndex). This returns a host-order
// copy of the whole table, up to the end of the last list a shape uses, allocated on the current
// heap like the rest of the model data. The file data is left as it is, so a resource can be
// loaded again (no swap in place).
GXVtxDescList* J3DPcCopyVtxDescList(const BE(GXVtxDescList)* src, const J3DShapeInitData* initData,
                                    const BE(u16)* indexTable, u16 shapeNum) {
    if (src == NULL) {
        return NULL;
    }
    u32 lastStart = 0;
    for (u16 i = 0; i < shapeNum; i++) {
        u32 start = initData[indexTable[i]].mVtxDescListIndex / sizeof(GXVtxDescList);
        if (start > lastStart) {
            lastStart = start;
        }
    }
    u32 count = lastStart;
    while (src[count].attr != GX_VA_NULL) {
        count++;
    }
    count++;
    GXVtxDescList* dst = new GXVtxDescList[count];
    for (u32 i = 0; i < count; i++) {
        dst[i].attr = src[i].attr;
        dst[i].type = src[i].type;
    }
    return dst;
}

// ---- J3DMaterialFactory.cpp -------------------------------------------------------------------

// The material tables hold J3DStruct.h infos, which are host objects as well (J3DTexMtx, J3DFog,
// J3DNBTScale and J3DIndTexMtx keep and animate them). In the file their multi-byte members are
// big-endian: these return a host-order copy, leaving the file data as it is. The first three are
// shared with J3DMaterialFactory_v21 (all declared in J3DMaterialFactory.h).
J3DTexMtxInfo J3DHostTexMtxInfo(const J3DTexMtxInfo& src) {
    J3DTexMtxInfo info;
    memcpy(&info, &src, sizeof(info));
    be_swap(info.mCenter);
    be_swap(info.mSRT.mScaleX);
    be_swap(info.mSRT.mScaleY);
    be_swap(info.mSRT.mRotation);
    be_swap(info.mSRT.mTranslationX);
    be_swap(info.mSRT.mTranslationY);
    be_swap(info.mEffectMtx);
    return info;
}

J3DFogInfo J3DHostFogInfo(const J3DFogInfo& src) {
    J3DFogInfo info;
    memcpy(&info, &src, sizeof(info));
    be_swap(info.mCenter);
    be_swap(info.mStartZ);
    be_swap(info.mEndZ);
    be_swap(info.mNearZ);
    be_swap(info.mFarZ);
    be_swap(info.mFogAdjTable);
    return info;
}

J3DNBTScaleInfo J3DHostNBTScaleInfo(const J3DNBTScaleInfo& src) {
    J3DNBTScaleInfo info;
    memcpy(&info, &src, sizeof(info));
    be_swap(info.mScale);
    return info;
}

J3DIndTexMtxInfo J3DHostIndTexMtxInfo(const J3DIndTexMtxInfo& src) {
    J3DIndTexMtxInfo info;
    memcpy(&info, &src, sizeof(info));
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            be_swap(info.mOffsetMtx[i][j]);
        }
    }
    return info;
}

// ---- J3DClusterLoader.cpp ---------------------------------------------------------------------

// The CLS1 records as stored: big-endian, 32-bit offsets from the block where the host J3DCluster,
// J3DClusterKey and J3DClusterVertex hold pointers (and so are larger).
struct J3DClusterData {
    /* 0x00 */ BE(f32) mMaxAngle;
    /* 0x04 */ BE(f32) mMinAngle;
    /* 0x08 */ BE(u32) mClusterKey;
    /* 0x0C */ u8 mFlags;
    /* 0x0D */ u8 field_0xd[3];
    /* 0x10 */ BE(u16) mKeyNum;
    /* 0x12 */ BE(u16) mPosNum;
    /* 0x14 */ BE(u16) mNrmNum;
    /* 0x16 */ BE(u16) mClusterVertexNum;
    /* 0x18 */ BE(u32) mPosDstIdx;
    /* 0x1C */ BE(u32) mClusterVertex;
    /* 0x20 */ BE(u32) mDeformer;
};  // Size: 0x24

struct J3DClusterKeyData {
    /* 0x00 */ BE(u16) mPosNum;
    /* 0x02 */ BE(u16) mNrmNum;
    /* 0x04 */ BE(u32) mPosFlag;
    /* 0x08 */ BE(u32) mNrmFlag;
};  // Size: 0x0C

struct J3DClusterVertexData {
    /* 0x00 */ BE(u16) mNum;
    /* 0x04 */ BE(u32) mSrcIdx;
    /* 0x08 */ BE(u32) mDstIdx;
};  // Size: 0x0C

// The loader of the GameCube copies the records and relocates their offsets; on the host the
// records are converted into the host structs instead, with the same pointers. The index and
// position/normal arrays they point at stay in the file (big-endian; the deformer reads them,
// step 4.12).
void J3DClusterLoader_v15::readCluster(const J3DClusterBlock* block) {
    mpDeformData->mClusterNum = block->mClusterNum;
    mpDeformData->mClusterKeyNum = block->mClusterKeyNum;
    mpDeformData->mVtxPosNum = block->mVtxPosNum;
    mpDeformData->mVtxNrmNum = block->mVtxNrmNum;
    mpDeformData->mClusterVertexNum = block->mClusterVertexNum;

    if (block->mClusterName != 0) {
        mpDeformData->mClusterName = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mClusterName));
    } else {
        mpDeformData->mClusterName = NULL;
    }
    if (block->mClusterKeyName != 0) {
        mpDeformData->mClusterKeyName = new JUTNameTab(JSUConvertOffsetToPtr<ResNTAB>(block, block->mClusterKeyName));
    } else {
        mpDeformData->mClusterKeyName = NULL;
    }

    mpDeformData->mVtxPos = JSUConvertOffsetToPtr<f32>(block, block->mVtxPos);
    mpDeformData->mVtxNrm = JSUConvertOffsetToPtr<f32>(block, block->mVtxNrm);

    const J3DClusterData* blockCluster = JSUConvertOffsetToPtr<J3DClusterData>(block, block->mClusterPointer);
    const J3DClusterKeyData* blockClusterKey = JSUConvertOffsetToPtr<J3DClusterKeyData>(block, block->mClusterKeyPointer);
    const J3DClusterVertexData* blockClusterVertex = JSUConvertOffsetToPtr<J3DClusterVertexData>(block, block->mClusterVertex);

    mpDeformData->mClusterKeyPointer = new J3DClusterKey[mpDeformData->getClusterKeyNum()];
    for (int i = 0; i < mpDeformData->getClusterKeyNum(); i++) {
        J3DClusterKey* clusterKey = &mpDeformData->mClusterKeyPointer[i];
        clusterKey->mPosNum = blockClusterKey[i].mPosNum;
        clusterKey->mNrmNum = blockClusterKey[i].mNrmNum;
        clusterKey->mPosFlag = JSUConvertOffsetToPtr<u16>(block, blockClusterKey[i].mPosFlag);
        clusterKey->mNrmFlag = JSUConvertOffsetToPtr<u16>(block, blockClusterKey[i].mNrmFlag);
    }

    mpDeformData->mClusterVertex = new J3DClusterVertex[mpDeformData->mClusterVertexNum];
    for (int i = 0; i < mpDeformData->mClusterVertexNum; i++) {
        J3DClusterVertex* clusterVertex = &mpDeformData->mClusterVertex[i];
        clusterVertex->mNum = blockClusterVertex[i].mNum;
        clusterVertex->mSrcIdx = JSUConvertOffsetToPtr<u16>(block, blockClusterVertex[i].mSrcIdx);
        clusterVertex->mDstIdx = JSUConvertOffsetToPtr<u16>(block, blockClusterVertex[i].mDstIdx);
    }

    mpDeformData->mClusterPointer = new J3DCluster[mpDeformData->getClusterNum()];
    for (int i = 0; i < mpDeformData->getClusterNum(); i++) {
        const J3DClusterData& data = blockCluster[i];
        J3DCluster* cluster = &mpDeformData->mClusterPointer[i];
        cluster->mMaxAngle = data.mMaxAngle;
        cluster->mMinAngle = data.mMinAngle;
        cluster->mFlags = data.mFlags;
        cluster->mKeyNum = data.mKeyNum;
        cluster->mPosNum = data.mPosNum;
        cluster->mNrmNum = data.mNrmNum;
        cluster->mClusterVertexNum = data.mClusterVertexNum;
        // The GameCube points mClusterKey at the file's key record (whose flag offsets it never
        // relocates); on the host it points at that record's converted copy (same counts, flag
        // pointers relocated). No BLS file is on the TWW disc.
        u32 keyIdx = ((u32)data.mClusterKey - (u32)block->mClusterKeyPointer) / sizeof(J3DClusterKeyData);
        cluster->mClusterKey = &mpDeformData->mClusterKeyPointer[keyIdx];
        cluster->mPosDstIdx = JSUConvertOffsetToPtr<u16>(block, data.mPosDstIdx);
        // As the GameCube computes it: the record's index, divided once more by the record size.
        u32 vertexIdx = ((u32)data.mClusterVertex - (u32)block->mClusterVertex) / sizeof(J3DClusterVertexData) /
                        sizeof(J3DClusterVertexData);
        cluster->mClusterVertex = &mpDeformData->mClusterVertex[vertexIdx];
        J3DDeformer* deformer = new J3DDeformer(mpDeformData);
        if (cluster->mNrmNum != 0) {
            deformer->field_0x0c = new f32[cluster->mNrmNum * 3];
        } else {
            deformer->field_0x0c = NULL;
        }
        deformer->mFlags = cluster->mFlags;
        deformer->mWeightList = new f32[cluster->mKeyNum];
        cluster->setDeformer(deformer);
    }
}

// ---- J3DTevs.cpp ------------------------------------------------------------------------------

// The texture objects of J3DTexture (see J3DTexture.h): the material display lists still carry
// loadTexNo's BP writes, but Aurora takes the image and TLUT from these objects. Adapted from
// Dusklight (ref/dusklight/libs/JSystem/src/J3DGraphBase/J3DTexture.cpp, loadGX and
// loadGXTexObj, CC0), with the sign-extended offsets of loadTexNo.
J3DTexture::J3DTexture(u16 num, ResTIMG* res) : mNum(num), mpRes(res) {
    mpTexObj = new GXTexObj[num];
    mpTlutObj = new GXTlutObj[num];
    for (u16 i = 0; i < num; i++) {
        initTexObj(i);
    }
}

J3DTexture::~J3DTexture() {
    delete[] mpTexObj;
    delete[] mpTlutObj;
}

void J3DTexture::initTexObj(u16 index) {
    ResTIMG* timg = getResTIMG(index);
    u8* image = (u8*)timg + (s32)(u32)timg->imageOffset;
    GXBool mipmap = timg->mipmapEnabled != 0 ? GX_TRUE : GX_FALSE;
    if (!timg->indexTexture) {
        GXInitTexObj(&mpTexObj[index], image, timg->width, timg->height, (GXTexFmt)timg->format,
                     (GXTexWrapMode)timg->wrapS, (GXTexWrapMode)timg->wrapT, mipmap);
    } else {
        GXInitTexObjCI(&mpTexObj[index], image, timg->width, timg->height, (GXCITexFmt)timg->format,
                       (GXTexWrapMode)timg->wrapS, (GXTexWrapMode)timg->wrapT, mipmap, GX_TLUT0);
        GXInitTlutObj(&mpTlutObj[index], (u8*)timg + (s32)(u32)timg->paletteOffset,
                      (GXTlutFmt)timg->colorFormat, timg->numColors);
    }
    // The values loadTexNo writes with J3DGDSetTexLookupMode.
    GXInitTexObjLOD(&mpTexObj[index], (GXTexFilter)timg->minFilter, (GXTexFilter)timg->magFilter,
                    timg->minLOD * 0.125f, timg->maxLOD * 0.125f, timg->LODBias * 0.01f,
                    timg->biasClamp, timg->doEdgeLOD, (GXAnisotropy)timg->maxAnisotropy);
}

void J3DTexture::loadGX(u16 index, GXTexMapID texMapID) const {
    ResTIMG* timg = getResTIMG(index);
    if (timg->indexTexture) {
        // loadTexNo gives texture map n the TLUT at TMEM (n << 13) + 0xF0000: one TLUT per map.
        GXLoadTlut(&mpTlutObj[index], (GXTlut)texMapID);
        GXInitTexObjTlut(&mpTexObj[index], (GXTlut)texMapID);
    }
    GXLoadTexObj(&mpTexObj[index], texMapID);
}

// ---- J3DMatBlock.cpp --------------------------------------------------------------------------

// J3DTevBlock::loadTexture (see J3DTexture.h): texture map i shows mTexNo[i], as in loadTexNo.
// Adapted from Dusklight (ref/dusklight/libs/JSystem/src/J3DGraphBase/J3DMatBlock.cpp,
// J3DTevBlock*::loadTexture, CC0).
void J3DLoadTexNoArray(const u16* texNo, u32 num) {
    for (u32 i = 0; i < num; i++) {
        if (texNo[i] != 0xffff) {
            j3dSys.getTexture()->loadGX(texNo[i], GXTexMapID(GX_TEXMAP0 + i));
        }
    }
}

void J3DTevBlockPatched::loadTexture() { J3DLoadTexNoArray(mTexNo, ARRAY_SIZE(mTexNo)); }
void J3DTevBlock1::loadTexture() { J3DLoadTexNoArray(mTexNo, ARRAY_SIZE(mTexNo)); }
void J3DTevBlock2::loadTexture() { J3DLoadTexNoArray(mTexNo, ARRAY_SIZE(mTexNo)); }
void J3DTevBlock4::loadTexture() { J3DLoadTexNoArray(mTexNo, ARRAY_SIZE(mTexNo)); }
void J3DTevBlock16::loadTexture() { J3DLoadTexNoArray(mTexNo, ARRAY_SIZE(mTexNo)); }

static u32 J3DCopyTexNoArray(u16* dst, const u16* texNo, u32 num) {
    for (u32 i = 0; i < num; i++) {
        dst[i] = texNo[i];
    }
    return num;
}

u32 J3DTevBlockPatched::getTexNoArray(u16* dst) const { return J3DCopyTexNoArray(dst, mTexNo, ARRAY_SIZE(mTexNo)); }
u32 J3DTevBlock1::getTexNoArray(u16* dst) const { return J3DCopyTexNoArray(dst, mTexNo, ARRAY_SIZE(mTexNo)); }
u32 J3DTevBlock2::getTexNoArray(u16* dst) const { return J3DCopyTexNoArray(dst, mTexNo, ARRAY_SIZE(mTexNo)); }
u32 J3DTevBlock4::getTexNoArray(u16* dst) const { return J3DCopyTexNoArray(dst, mTexNo, ARRAY_SIZE(mTexNo)); }
u32 J3DTevBlock16::getTexNoArray(u16* dst) const { return J3DCopyTexNoArray(dst, mTexNo, ARRAY_SIZE(mTexNo)); }
