#ifndef J3DMODELLOADER_H
#define J3DMODELLOADER_H

#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/JUtility/JUTDataHeader.h"
#include "helpers/endian.h"

// The block structs below are disc data, stored big-endian (phase 4; the same declarations as
// Dusklight's J3DModelLoader.h). A block's file offsets are declared as void* in the decomp, which
// is 8 bytes on the host: OFFSET_PTR_V0 keeps them 4 bytes there, as a big-endian u32 that
// JSUConvertOffsetToPtr adds to the block's address. On the GameCube it is the decomp's void*.
#if TARGET_PC
#define OFFSET_PTR_V0 BE(u32)
#else
#define OFFSET_PTR_V0 void*
#endif

inline u32 getBdlFlag_MaterialType(u32 i_flags) {
    return i_flags & 0x3000;
}

struct J3DModelInfoBlock : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mFlags;
    /* 0x0C */ BE(u32) mPacketNum;
    /* 0x10 */ BE(u32) mVtxNum;
    /* 0x14 */ OFFSET_PTR_V0 mpHierarchy;
};  // Size: 0x18

struct J3DVertexBlock : public JUTDataBlockHeader {
    /* 0x08 */ OFFSET_PTR_V0 mpVtxAttrFmtList;
    /* 0x0C */ OFFSET_PTR_V0 mpVtxPosArray;
    /* 0x10 */ OFFSET_PTR_V0 mpVtxNrmArray;
    /* 0x14 */ OFFSET_PTR_V0 mpVtxNBTArray;
    /* 0x18 */ OFFSET_PTR_V0 mpVtxColorArray[2];
    /* 0x20 */ OFFSET_PTR_V0 mpVtxTexCoordArray[8];
};  // Size: 0x40

struct J3DEnvelopBlock : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mWEvlpMtxNum;
    /* 0x0C */ OFFSET_PTR_V0 mpWEvlpMixMtxNum;
    /* 0x10 */ OFFSET_PTR_V0 mpWEvlpMixMtxIndex;
    /* 0x14 */ OFFSET_PTR_V0 mpWEvlpMixWeight;
    /* 0x18 */ OFFSET_PTR_V0 mpInvJointMtx;
};  // Size: 0x1C

struct J3DDrawBlock : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mMtxNum;
    /* 0x0C */ OFFSET_PTR_V0 mpDrawMtxFlag;
    /* 0x10 */ OFFSET_PTR_V0 mpDrawMtxIndex;
};  // Size: 0x14

struct J3DJointBlock;

struct J3DMaterialBlock : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mMaterialNum;
    /* 0x0C */ OFFSET_PTR_V0 mpMaterialInitData;
    /* 0x10 */ OFFSET_PTR_V0 mpMaterialID;
    /* 0x14 */ OFFSET_PTR_V0 mpNameTable;
    /* 0x18 */ OFFSET_PTR_V0 mpIndInitData;
    /* 0x1C */ OFFSET_PTR_V0 mpCullMode;
    /* 0x20 */ OFFSET_PTR_V0 mpMatColor;
    /* 0x24 */ OFFSET_PTR_V0 mpColorChanNum;
    /* 0x28 */ OFFSET_PTR_V0 mpColorChanInfo;
    /* 0x2C */ OFFSET_PTR_V0 mpAmbColor;
    /* 0x30 */ OFFSET_PTR_V0 mpLightInfo;
    /* 0x34 */ OFFSET_PTR_V0 mpTexGenNum;
    /* 0x38 */ OFFSET_PTR_V0 mpTexCoordInfo;
    /* 0x3C */ OFFSET_PTR_V0 mpTexCoord2Info;
    /* 0x40 */ OFFSET_PTR_V0 mpTexMtxInfo;
    /* 0x44 */ OFFSET_PTR_V0 field_0x44;
    /* 0x48 */ OFFSET_PTR_V0 mpTexNo;
    /* 0x4C */ OFFSET_PTR_V0 mpTevOrderInfo;
    /* 0x50 */ OFFSET_PTR_V0 mpTevColor;
    /* 0x54 */ OFFSET_PTR_V0 mpTevKColor;
    /* 0x58 */ OFFSET_PTR_V0 mpTevStageNum;
    /* 0x5C */ OFFSET_PTR_V0 mpTevStageInfo;
    /* 0x60 */ OFFSET_PTR_V0 mpTevSwapModeInfo;
    /* 0x64 */ OFFSET_PTR_V0 mpTevSwapModeTableInfo;
    /* 0x68 */ OFFSET_PTR_V0 mpFogInfo;
    /* 0x6C */ OFFSET_PTR_V0 mpAlphaCompInfo;
    /* 0x70 */ OFFSET_PTR_V0 mpBlendInfo;
    /* 0x74 */ OFFSET_PTR_V0 mpZModeInfo;
    /* 0x78 */ OFFSET_PTR_V0 mpZCompLoc;
    /* 0x7C */ OFFSET_PTR_V0 mpDither;
    /* 0x80 */ OFFSET_PTR_V0 mpNBTScaleInfo;
};

struct J3DMaterialBlock_v21 : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mMaterialNum;
    /* 0x0C */ OFFSET_PTR_V0 mpMaterialInitData;
    /* 0x10 */ OFFSET_PTR_V0 mpMaterialID;
    /* 0x14 */ OFFSET_PTR_V0 mpNameTable;
    /* 0x18 */ OFFSET_PTR_V0 mpCullMode;
    /* 0x1C */ OFFSET_PTR_V0 mpMatColor;
    /* 0x20 */ OFFSET_PTR_V0 mpColorChanNum;
    /* 0x24 */ OFFSET_PTR_V0 mpColorChanInfo;
    /* 0x28 */ OFFSET_PTR_V0 mpTexGenNum;
    /* 0x2C */ OFFSET_PTR_V0 mpTexCoordInfo;
    /* 0x30 */ OFFSET_PTR_V0 mpTexCoord2Info;
    /* 0x34 */ OFFSET_PTR_V0 mpTexMtxInfo;
    /* 0x38 */ OFFSET_PTR_V0 field_0x44;
    /* 0x3C */ OFFSET_PTR_V0 mpTexNo;
    /* 0x40 */ OFFSET_PTR_V0 mpTevOrderInfo;
    /* 0x44 */ OFFSET_PTR_V0 mpTevColor;
    /* 0x48 */ OFFSET_PTR_V0 mpTevKColor;
    /* 0x4C */ OFFSET_PTR_V0 mpTevStageNum;
    /* 0x50 */ OFFSET_PTR_V0 mpTevStageInfo;
    /* 0x54 */ OFFSET_PTR_V0 mpTevSwapModeInfo;
    /* 0x58 */ OFFSET_PTR_V0 mpTevSwapModeTableInfo;
    /* 0x5C */ OFFSET_PTR_V0 mpFogInfo;
    /* 0x60 */ OFFSET_PTR_V0 mpAlphaCompInfo;
    /* 0x64 */ OFFSET_PTR_V0 mpBlendInfo;
    /* 0x68 */ OFFSET_PTR_V0 mpZModeInfo;
    /* 0x6C */ OFFSET_PTR_V0 mpZCompLoc;
    /* 0x70 */ OFFSET_PTR_V0 mpDither;
    /* 0x74 */ OFFSET_PTR_V0 mpNBTScaleInfo;
};

struct J3DMaterialDLBlock : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mMaterialNum;
    /* 0x0C */ OFFSET_PTR_V0 mpDisplayListInit;
    /* 0x10 */ OFFSET_PTR_V0 mpPatchingInfo;
    /* 0x14 */ OFFSET_PTR_V0 mpCurrentMtxInfo;
    /* 0x18 */ OFFSET_PTR_V0 field_0x18;
    /* 0x1C */ OFFSET_PTR_V0 field_0x1c;
    /* 0x20 */ OFFSET_PTR_V0 mpNameTable;
    /* more */
};

struct J3DShapeBlock;

struct J3DTextureBlock : public JUTDataBlockHeader {
    /* 0x08 */ BE(u16) mTextureNum;
    /* 0x0C */ OFFSET_PTR_V0 mpTextureRes;
    /* 0x10 */ OFFSET_PTR_V0 mpNameTable;
};

static inline u32 getMdlDataFlag_TevStageNum(u32 flags) { return (flags >> 0x10) & 0x1f; }
static inline u32 getMdlDataFlag_TexGenFlag(u32 flags) { return flags & 0x0c000000; }
static inline u32 getMdlDataFlag_ColorFlag(u32 flags) { return flags & 0xc0000000; }
static inline u32 getMdlDataFlag_PEFlag(u32 flags) { return flags & 0x30000000; }
static inline u32 getMdlDataFlag_MtxLoadType(u32 flags) { return flags & 0x10; }

class J3DModelLoaderDataBase {
public:
    static J3DModelData* load(void const*, u32);
    static J3DModelData* loadBinaryDisplayList(void const*, u32);
    static J3DMaterialTable* loadMaterialTable(void const*);
};

class J3DModelLoader {
public:
    J3DModelLoader() :
        mpModelData(NULL),
        mpMaterialTable(NULL),
        mpShapeBlock(NULL),
        mpMaterialBlock(NULL),
        mpModelHierarchy(NULL),
        field_0x18(0) {}

    virtual J3DModelData* load(const void*, u32);
    virtual J3DMaterialTable* loadMaterialTable(const void*);
    virtual J3DModelData* loadBinaryDisplayList(const void*, u32);
    virtual u32 calcLoadSize(const void*, u32);
    virtual u32 calcLoadMaterialTableSize(const void*);
    virtual u32 calcLoadBinaryDisplayListSize(const void*, u32);
    virtual u16 countMaterialNum(const void*);
    virtual void setupBBoardInfo();
    virtual ~J3DModelLoader() {}
    virtual void readMaterial(const J3DMaterialBlock*, u32) {}
    virtual void readMaterial_v21(const J3DMaterialBlock_v21*, u32) {}
    virtual void readMaterialTable(const J3DMaterialBlock*, u32) {}
    virtual void readMaterialTable_v21(const J3DMaterialBlock_v21*, u32) {}
    virtual u32 calcSizeMaterial(const J3DMaterialBlock*, u32) { return 0; }
    virtual u32 calcSizeMaterialTable(const J3DMaterialBlock*, u32) { return 0; }
    
    void readInformation(const J3DModelInfoBlock*, u32);
    void readVertex(const J3DVertexBlock*);
    void readEnvelop(const J3DEnvelopBlock*);
    void readDraw(const J3DDrawBlock*);
    void readJoint(const J3DJointBlock*);
    void readShape(const J3DShapeBlock*, u32);
    void readTexture(const J3DTextureBlock*);
    void readTextureTable(const J3DTextureBlock*);
    void readPatchedMaterial(const J3DMaterialBlock*, u32);
    void readMaterialDL(const J3DMaterialDLBlock*, u32);
    void modifyMaterial(u32);
    u32 calcSizeInformation(const J3DModelInfoBlock*, u32);
    u32 calcSizeJoint(const J3DJointBlock*);
    u32 calcSizeShape(const J3DShapeBlock*, u32);
    u32 calcSizeTexture(const J3DTextureBlock*);
    u32 calcSizeTextureTable(const J3DTextureBlock*);
    u32 calcSizePatchedMaterial(const J3DMaterialBlock*, u32);
    u32 calcSizeMaterialDL(const J3DMaterialDLBlock*, u32);

protected:
    /* 0x04 */ J3DModelData* mpModelData;
    /* 0x08 */ J3DMaterialTable* mpMaterialTable;
    /* 0x0C */ const J3DShapeBlock* mpShapeBlock;
    /* 0x10 */ const J3DMaterialBlock* mpMaterialBlock;
    /* 0x14 */ J3DModelHierarchy* mpModelHierarchy;
    /* 0x18 */ u8 field_0x18;
    /* 0x19 */ u8 field_0x19;
};

class J3DModelLoader_v21 : public J3DModelLoader {
public:
    ~J3DModelLoader_v21() {}
    void readMaterial_v21(const J3DMaterialBlock_v21*, u32);
    void readMaterialTable_v21(const J3DMaterialBlock_v21*, u32);
};

class J3DModelLoader_v26 : public J3DModelLoader {
public:
    ~J3DModelLoader_v26() {}
    void readMaterial(const J3DMaterialBlock*, u32);
    void readMaterialTable(const J3DMaterialBlock*, u32);
    u32 calcSizeMaterial(const J3DMaterialBlock*, u32);
    u32 calcSizeMaterialTable(const J3DMaterialBlock*, u32);
};

#endif /* J3DMODELLOADER_H */
