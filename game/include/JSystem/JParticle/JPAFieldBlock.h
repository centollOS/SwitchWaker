#ifndef JPAFIELDBLOCK_H
#define JPAFIELDBLOCK_H

#include "JSystem/JGeometry.h"
#include "helpers/endian.h"

struct JPAFieldBlockData {
    /* 0x00 */ BE(u32) mFlags;
    /* 0x04 */ BE(f32) mMag;
    /* 0x08 */ BE(f32) mMagRndm;
    /* 0x0C */ BE(f32) mMaxDist;
    /* 0x10 */ JGeometry::TVec3<BE(f32)> mPos;
    /* 0x1C */ JGeometry::TVec3<BE(f32)> mDir;
    /* 0x28 */ BE(f32) mVal1;
    /* 0x2C */ BE(f32) mVal2;
    /* 0x30 */ BE(f32) mVal3;
    /* 0x34 */ BE(f32) mFadeIn;
    /* 0x38 */ BE(f32) mFadeOut;
    /* 0x3C */ BE(f32) mEnTime;
    /* 0x40 */ BE(f32) mDisTime;
    /* 0x44 */ u8 mCycle;
};

class JPAFieldBlock {
public:
    virtual ~JPAFieldBlock() {}
    virtual u8 getType() = 0;
    virtual u32 getID() = 0;
    virtual u32 getVelType() = 0;
    virtual u32 getCycle() = 0;
    virtual u32 getSttFlag() = 0;
    virtual void getPos(JGeometry::TVec3<float>&) = 0;
    virtual void getDir(JGeometry::TVec3<float>&) = 0;
    virtual f32 getMag() = 0;
    virtual f32 getMagRndm() = 0;
    virtual f32 getMaxDist() = 0;
    virtual f32 getVal1() = 0;
    virtual f32 getVal2() = 0;
    virtual f32 getVal3() = 0;
    virtual f32 getFadeIn() = 0;
    virtual f32 getFadeOut() = 0;
    virtual f32 getEnTime() = 0;
    virtual f32 getDisTime() = 0;
};

class JPAFieldBlockArc : public JPAFieldBlock {
public:
    JPAFieldBlockArc(const u8*);
    virtual ~JPAFieldBlockArc() {}

    virtual u8 getType() { return (mpData->mFlags >> 0) & 0x0F; }
    virtual u32 getVelType() { return (mpData->mFlags >> 8) & 0x03; }
    virtual u32 getSttFlag() { return (mpData->mFlags >> 16) & 0xFFFF; }
    virtual u32 getCycle() { return mpData->mCycle; }
    virtual u32 getID() { return 0; }
#if TARGET_PC
    // The file's vectors are big-endian: converted member by member (TVec3<f32>::set).
    virtual void getPos(JGeometry::TVec3<float>& out) { out.set(mpData->mPos); };
    virtual void getDir(JGeometry::TVec3<float>& out) { out.set(mpData->mDir); };
#else
    virtual void getPos(JGeometry::TVec3<float>& out) { out = mpData->mPos; };
    virtual void getDir(JGeometry::TVec3<float>& out) { out = mpData->mDir; };
#endif
    virtual f32 getMag() { return mpData->mMag; }
    virtual f32 getMagRndm() { return mpData->mMagRndm; }
    virtual f32 getMaxDist() { return mpData->mMaxDist; }
    virtual f32 getVal1() { return mpData->mVal1; }
    virtual f32 getVal2() { return mpData->mVal2; }
    virtual f32 getVal3() { return mpData->mVal3; }
    virtual f32 getFadeIn() { return mpData->mFadeIn; }
    virtual f32 getFadeOut() { return mpData->mFadeOut; }
    virtual f32 getEnTime() { return mpData->mEnTime; }
    virtual f32 getDisTime() { return mpData->mDisTime; }

public:
    /* 0x04 */ const JPAFieldBlockData* mpData;
};

#endif /* JPAFIELDBLOCK_H */
