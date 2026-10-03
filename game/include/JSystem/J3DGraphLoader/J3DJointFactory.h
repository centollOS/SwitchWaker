#ifndef J3DJOINTFACTORY_H
#define J3DJOINTFACTORY_H

#include "JSystem/J3DGraphBase/J3DTransform.h"
#include "dolphin/types.h"
#include "helpers/endian.h"

class J3DJoint;
struct ResNTAB;

// JNT1 disc data, stored big-endian (phase 4). J3DTransformInfo is a host object too (J3DJoint
// keeps and animates it), so on TARGET_PC the file's copy is J3DTransformInfoData, the same layout
// with big-endian members, which getTransformInfo converts; the GameCube keeps the decomp's type.
#if TARGET_PC
struct J3DTransformInfoData {
    /* 0x00 */ BE(Vec) mScale;
    /* 0x0C */ BE(S16Vec) mRotation;
    /* 0x14 */ BE(Vec) mTranslate;
};  // Size: 0x20
#define J3D_TRANSFORM_INFO_DATA J3DTransformInfoData
#else
#define J3D_TRANSFORM_INFO_DATA J3DTransformInfo
#endif

struct J3DJointInitData {
    /* 0x00 */ BE(u16) mKind;
#if TARGET_PC
    // The file stores 0, 1 or 0xFF (J3DJointFactory::create maps 0xFF to false). A bool holding
    // 0xFF is undefined for clang, which then drops create's 0xFF test: read the byte as a u8.
    /* 0x02 */ u8 mScaleCompensate;
#else
    /* 0x02 */ bool mScaleCompensate;
#endif
    /* 0x04 */ J3D_TRANSFORM_INFO_DATA mTransformInfo;
    /* 0x24 */ BE(f32) mRadius;
    /* 0x28 */ BE(Vec) mMin;
    /* 0x34 */ BE(Vec) mMax;
};  // Size: 0x40

struct J3DJointBlock {
    /* 0x00 */ BE(u32) mMagic;
    /* 0x04 */ BE(u32) mSize;

    /* 0x08 */ BE(u16) mJointNum;
    /* 0x0A */ u16 _pad;

#if TARGET_PC
    // File offsets from the block (4 bytes, big-endian), not host pointers.
    /* 0x0C */ BE(u32) mpJointInitData;
    /* 0x10 */ BE(u32) mpIndexTable;
    /* 0x14 */ BE(u32) mpNameTable;
#else
    /* 0x0C */ J3DJointInitData* mpJointInitData;
    /* 0x10 */ u16* mpIndexTable;
    /* 0x14 */ ResNTAB* mpNameTable;
#endif
};  // Size: 0x18

struct J3DJointFactory {
    J3DJointFactory(J3DJointBlock const&);
    J3DJoint* create(int);

    /* 0x0 */ J3DJointInitData* mJointInitData;
    /* 0x4 */ BE(u16)* mIndexTable;

    u16 getKind(int no) const { return mJointInitData[mIndexTable[no]].mKind; }
    u8 getScaleCompensate(int no) const { return mJointInitData[mIndexTable[no]].mScaleCompensate; }
#if TARGET_PC
    J3DTransformInfo getTransformInfo(int no) const {
        const J3DTransformInfoData& data = mJointInitData[mIndexTable[no]].mTransformInfo;
        J3DTransformInfo info;
        info.mScale = data.mScale;
        S16Vec rotation = data.mRotation;
        info.mRotation.x = rotation.x;
        info.mRotation.y = rotation.y;
        info.mRotation.z = rotation.z;
        info.mTranslate = data.mTranslate;
        return info;
    }
    f32 getRadius(int no) const { return mJointInitData[mIndexTable[no]].mRadius; }
    Vec getMin(int no) const { return mJointInitData[mIndexTable[no]].mMin; }
    Vec getMax(int no) const { return mJointInitData[mIndexTable[no]].mMax; }
#else
    const J3DTransformInfo& getTransformInfo(int no) const {
        return mJointInitData[mIndexTable[no]].mTransformInfo;
    }
    f32 getRadius(int no) const { return mJointInitData[mIndexTable[no]].mRadius; }
    Vec& getMin(int no) const { return mJointInitData[mIndexTable[no]].mMin; }
    Vec& getMax(int no) const { return mJointInitData[mIndexTable[no]].mMax; }
#endif
};

#endif /* J3DJOINTFACTORY_H */