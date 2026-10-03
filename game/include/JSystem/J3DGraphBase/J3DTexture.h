#ifndef J3DTEXTURE_H
#define J3DTEXTURE_H

#include "JSystem/J3DGraphBase/J3DGD.h"
#include "JSystem/J3DGraphBase/J3DStruct.h"
#include "JSystem/J3DGraphBase/J3DTevs.h"
#include "JSystem/JUtility/JUTAssert.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "dolphin/mtx/mtx.h"
#include "dolphin/types.h"

inline void J3DGDLoadTexMtxImm(Mtx pMtx, u32 i, GXTexMtxType mType) {
    u16 cmd = i * 4;
    u8 len = mType == GX_MTX2x4 ? 8 : 12;
    J3DGDWriteXFCmdHdr(cmd, len);
    J3DGDWrite_f32(pMtx[0][0]);
    J3DGDWrite_f32(pMtx[0][1]);
    J3DGDWrite_f32(pMtx[0][2]);
    J3DGDWrite_f32(pMtx[0][3]);
    J3DGDWrite_f32(pMtx[1][0]);
    J3DGDWrite_f32(pMtx[1][1]);
    J3DGDWrite_f32(pMtx[1][2]);
    J3DGDWrite_f32(pMtx[1][3]);
    if (mType == GX_MTX3x4) {
        J3DGDWrite_f32(pMtx[2][0]);
        J3DGDWrite_f32(pMtx[2][1]);
        J3DGDWrite_f32(pMtx[2][2]);
        J3DGDWrite_f32(pMtx[2][3]);
    }
}

class J3DTexture {
private:
    /* 0x0 */ u16 mNum;
    /* 0x4 */ ResTIMG* mpRes;
#if TARGET_PC
    // Aurora binds a texture only through GXLoadTexObj/GXLoadTlut: it does not resolve the image
    // and TLUT addresses that the material display lists write to the BP registers (loadTexNo).
    // Each entry therefore keeps a texture object (and TLUT object), built from its ResTIMG, which
    // J3DTevBlock::loadTexture loads when the material is drawn. Pattern of Dusklight
    // (ref/dusklight/libs/JSystem/include/JSystem/J3DGraphBase/J3DTexture.h, CC0).
    GXTexObj* mpTexObj;
    GXTlutObj* mpTlutObj;

    void initTexObj(u16 index);
#endif

public:
#if TARGET_PC
    J3DTexture(u16 num, ResTIMG* res);
    virtual ~J3DTexture();
    void loadGX(u16 index, GXTexMapID texMapID) const;
    // For code that writes a ResTIMG entry itself instead of calling setResTIMG (daPy_lk_c swaps
    // Link's clothes texture header in place): rebuild that entry's texture object, which would
    // otherwise still describe the previous image.
    void resTIMGChanged(const ResTIMG* entry) {
        J3D_ASSERT(0, entry >= mpRes && entry < mpRes + mNum, "Error : range over.");
        initTexObj((u16)(entry - mpRes));
    }
#else
    J3DTexture(u16 num, ResTIMG* res) : mNum(num), mpRes(res) {}
    virtual ~J3DTexture() {}
#endif

    u16 getNum() const { return mNum; }
    ResTIMG* getResTIMG(u16 index) const {
        J3D_ASSERT(72, index < mNum, "Error : range over.");
        return &mpRes[index];   
    }
    void setResTIMG(u16 index, const ResTIMG& timg) {
        J3D_ASSERT(81, index < mNum, "Error : range over.");
        mpRes[index] = timg;
#if TARGET_PC
        // The offsets become relative to this table entry. timg can lie below it, so the GameCube
        // stores a negative distance as a 32-bit wrap; on the host the readers of a J3DTexture
        // entry (loadTexNo) add the offset sign-extended. Every ResTIMG the game passes here
        // (toon images, the frame-buffer copy, textures of other models) is in a JKR heap inside
        // MEM1 (decision H5), so the distance fits in 32 bits; anything else would be a new case.
        intptr_t delta = (intptr_t)&timg - (intptr_t)(mpRes + index);
        if (delta != (s32)delta) {
            OSPanic(__FILE__, __LINE__, "J3DTexture::setResTIMG: ResTIMG out of 32-bit reach");
        }
        mpRes[index].imageOffset = (u32)((s32)(u32)mpRes[index].imageOffset + (s32)delta);
        mpRes[index].paletteOffset = (u32)((s32)(u32)mpRes[index].paletteOffset + (s32)delta);
        initTexObj(index);
#else
        mpRes[index].imageOffset = ((mpRes[index].imageOffset + (u32)&timg - (u32)(mpRes + index)));
        mpRes[index].paletteOffset = ((mpRes[index].paletteOffset + (u32)&timg - (u32)(mpRes + index)));
#endif
    }
};

class J3DTexMtx : public J3DTexMtxInfo {
public:
    J3DTexMtx() { J3DTexMtxInfo::operator=(j3dDefaultTexMtxInfo); }
    J3DTexMtx(const J3DTexMtxInfo& info) {
        J3DTexMtxInfo::operator=(info);
    }
    ~J3DTexMtx() {}
    void load(u32 texMtxID) const {
        GDOverflowCheck(53);
        J3DGDLoadTexMtxImm((Mtx&)mMtx, GX_TEXMTX0 + texMtxID * 3, (GXTexMtxType)mProjection);
    };
    void calc();

    J3DTexMtxInfo& getTexMtxInfo() { return *this; }
    J3DTextureSRTInfo& getTextureSRT() { return mSRT;}
    Mtx& getMtx() { return mMtx; }
    void setEffectMtx(Mtx effectMtx) { J3DTexMtxInfo::setEffectMtx(effectMtx); }
    Mtx& getViewMtx() { return mViewMtx; }
    void setViewMtx(Mtx viewMtx) { MTXCopy(viewMtx, mViewMtx); }

private:
    /* 0x64 */ Mtx mMtx;
    /* 0x94 */ Mtx mViewMtx;
};  // Size: 0xC4

struct J3DTexCoord : public J3DTexCoordInfo {
    J3DTexCoord() {
        J3DTexCoordInfo::operator=(j3dDefaultTexCoordInfo[0]);
    }
    J3DTexCoord(const J3DTexCoordInfo& info) {
        J3DTexCoordInfo::operator=(info);
    }

    u8 getTexGenType() const { return mTexGenType; }
    u8 getTexGenSrc() const { return mTexGenSrc; }
    u8 getTexGenMtx() const { return mTexGenMtx; }
    void setTexGenMtx(u8 v) { mTexGenMtx = v; }

    // void operator=(const J3DTexCoord&) {}
    // void operator==(J3DTexCoord&) {}
};  // Size: 0x4

#endif /* J3DTEXTURE_H */
