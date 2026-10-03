#ifndef J3DCLUSTERLOADER_H
#define J3DCLUSTERLOADER_H

#include "JSystem/JUtility/JUTDataHeader.h"
#include "helpers/endian.h"

// As in J3DModelLoader.h.
#ifndef OFFSET_PTR_V0
#if TARGET_PC
#define OFFSET_PTR_V0 BE(u32)
#else
#define OFFSET_PTR_V0 void*
#endif
#endif

class J3DDeformData;

// CLS1 disc data, stored big-endian (phase 4; Dusklight's J3DClusterLoader.h declares the same).
// The block's file offsets are OFFSET_PTR_V0 (as in J3DModelLoader.h): a big-endian u32 on TARGET_PC,
// the decomp's void* on the GameCube.
class J3DClusterBlock : public JUTDataBlockHeader {
private:
    friend class J3DClusterLoader;
    friend class J3DClusterLoader_v15;

    /* 0x08 */ BE(u16) mClusterNum;
    /* 0x0A */ BE(u16) mClusterKeyNum;
    /* 0x0C */ BE(u16) mClusterVertexNum;
    /* 0x0E */ BE(u16) mVtxPosNum;
    /* 0x10 */ BE(u16) mVtxNrmNum;
    /* 0x14 */ OFFSET_PTR_V0 mClusterPointer;
    /* 0x18 */ OFFSET_PTR_V0 mClusterKeyPointer;
    /* 0x1C */ OFFSET_PTR_V0 mClusterVertex;
    /* 0x20 */ OFFSET_PTR_V0 mVtxPos;
    /* 0x24 */ OFFSET_PTR_V0 mVtxNrm;
    /* 0x28 */ OFFSET_PTR_V0 mClusterName;
    /* 0x2C */ OFFSET_PTR_V0 mClusterKeyName;
};

struct J3DClusterLoaderDataBase {
    static void* load(const void*);
};

class J3DClusterLoader {
public:
    virtual void* load(const void*) = 0;
    virtual ~J3DClusterLoader() {}
};

class J3DClusterLoader_v15 : public J3DClusterLoader {
public:
    J3DClusterLoader_v15();
    ~J3DClusterLoader_v15();
    void* load(const void*);
    void readCluster(const J3DClusterBlock*);

private:
    /* 0x04 */ J3DDeformData* mpDeformData;
};

#endif /* J3DCLUSTERLOADER_H */
